#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
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

  etna::create_program("local_shadertoy2", {LOCAL_SHADERTOY2_SHADERS_ROOT "toy.comp.spv"});
  pipelineToy = etna::get_context().getPipelineManager().createComputePipeline("local_shadertoy2", {});
  
  ImageToy = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "image_toy",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
  });
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
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::set_state(
        currentCmdBuf,
        ImageToy.get(),
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderWrite,
        vk::ImageLayout::eGeneral, //eTransferSrcOptimal
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      auto set = etna::create_descriptor_set(
        etna::get_shader_program("local_shadertoy1").getDescriptorLayoutId(0),
        currentCmdBuf,
        {
          etna::Binding{0, ImageToy.genBinding({}, vk::ImageLayout::eGeneral)},
        });

      vk::DescriptorSet vkSet = set.getVkSet();

      currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, pipelineToy.getVkPipeline());
      currentCmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineToy.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

      uint32_t groupCountX = (resolution.x + 31) / 32;
      uint32_t groupCountY = (resolution.y + 31) / 32;

      currentCmdBuf.dispatch(groupCountX, groupCountY, 1);

        etna::set_state(
        currentCmdBuf,
        ImageToy.get(),
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      vk::ImageBlit blitRegion{
          .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
          .srcOffsets = std::array<vk::Offset3D, 2>{
              vk::Offset3D{0, 0, 0},
              vk::Offset3D{static_cast<int32_t>(resolution.x), static_cast<int32_t>(resolution.y), 1}},
          .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
          .dstOffsets = std::array<vk::Offset3D, 2>{
              vk::Offset3D{0, 0, 0},
              vk::Offset3D{static_cast<int32_t>(resolution.x), static_cast<int32_t>(resolution.y), 1}}
      };

      currentCmdBuf.blitImage(
          ImageToy.get(), vk::ImageLayout::eTransferSrcOptimal,
          backbuffer, vk::ImageLayout::eTransferDstOptimal,
          blitRegion, vk::Filter::eNearest
      );

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
