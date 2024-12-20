#include "App.hpp"

#include <stb_image.h>
#include <etna/BlockingTransferHelper.hpp>
#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    etna::initialize(etna::InitParams{
      .applicationName = "Inflight frames",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    resolution = {w, h};
  }

  commandManager = etna::get_context().createPerFrameCmdMgr();
  
  context = &etna::get_context();

  etna::create_program("texture_processing", {INFLIGHT_FRAMES_SHADERS_ROOT "shadertoy2.comp.spv"});
  pipelineToy = etna::get_context().getPipelineManager().createComputePipeline("texture_processing", {});
  
  TextureComputed = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "image_toy",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage =
      vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eStorage |
          vk::ImageUsageFlagBits::eTransferSrc
  });

  etna::create_program(
  "graphics",
  {
    INFLIGHT_FRAMES_SHADERS_ROOT "shadertoy2.vert.spv",
    INFLIGHT_FRAMES_SHADERS_ROOT "shadertoy2.frag.spv"
  });

  graphicsPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "graphics",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {
        .colorAttachmentFormats = { vkWindow->getCurrentFormat() }
       }
    }
  );

  sampler = etna::Sampler(etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat,
    .name = "textureSampler"
  });

  int width;
  int height;
  int channels;

  auto rawImage = stbi_load(
    GRAPHICS_COURSE_RESOURCES_ROOT "/textures/test_tex_1.png",
    &width,
    &height,
    &channels,
    STBI_rgb_alpha
  );

  TextureImage = etna::get_context().createImage(etna::Image::CreateInfo {
    .extent = vk::Extent3D{static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1},
    .name = "Texture",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage =
      vk::ImageUsageFlagBits::eStorage |
      vk::ImageUsageFlagBits::eSampled |
      vk::ImageUsageFlagBits::eTransferDst,
  });

  auto transfer = etna::BlockingTransferHelper(etna::BlockingTransferHelper::CreateInfo{
    .stagingSize = static_cast<unsigned long long>(width * height)
  });

  auto oneShotManager = etna::get_context().createOneShotCmdMgr();
  auto srcData = std::span(
      reinterpret_cast<const std::byte*>(rawImage),
      width * height * STBI_rgb_alpha
  );

  transfer.uploadImage(
    *oneShotManager,
    TextureImage,
    0,
    0,
    srcData
  );

  stbi_image_free(rawImage);
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame()
{
  auto currentCmdBuf = commandManager->acquireNext();

  etna::begin_frame();

  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, renderFrameInflight);

      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

    {
      etna::set_state(
        currentCmdBuf,
        TextureComputed.get(),
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderWrite,
        vk::ImageLayout::eGeneral, //eTransferSrcOptimal
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      auto set = etna::create_descriptor_set(
        etna::get_shader_program("texture_processing").getDescriptorLayoutId(0),
        currentCmdBuf,
        {
          etna::Binding{0, TextureComputed.genBinding({}, vk::ImageLayout::eGeneral)},
        });

      vk::DescriptorSet vkSet = set.getVkSet();

      currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, pipelineToy.getVkPipeline());
      currentCmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineToy.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

      uint32_t groupCountX = (resolution.x + 31) / 32;
      uint32_t groupCountY = (resolution.y + 31) / 32;

      currentCmdBuf.dispatch(groupCountX, groupCountY, 1);
    }
      {
        etna::set_state(
          currentCmdBuf,
          TextureComputed.get(),
          vk::PipelineStageFlagBits2::eFragmentShader,
          vk::AccessFlagBits2::eShaderSampledRead,
          vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor);

        etna::flush_barriers(currentCmdBuf);

        etna::RenderTargetState target{
          currentCmdBuf,
          {
            {0, 0},
            {resolution.x, resolution.y}
          },
      {
        etna::RenderTargetState::AttachmentParams {
          .image = backbuffer,
          .view = backbufferView
        }},
          {}
        };

        auto set = etna::create_descriptor_set(
          etna::get_shader_program("graphics").getDescriptorLayoutId(0),
          currentCmdBuf,
          {
            etna::Binding{0, TextureComputed.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{1, TextureImage.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
          });

        vk::DescriptorSet vkSet = set.getVkSet();

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

        currentCmdBuf.draw(3, 1, 0, 0);
      }

      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
