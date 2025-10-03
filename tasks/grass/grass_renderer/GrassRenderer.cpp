#include "GrassRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <cmath>

GrassRenderer::GrassRenderer() {}

void GrassRenderer::allocateResources(
  etna::Buffer&      in_constants,
  etna::Buffer&      in_uniform_params_buffer,
  etna::Sampler&     in_default_sampler,
  const etna::Image& in_height_map,
  const etna::Image& in_wind_map,
  float              in_terrain_size)
{
  this->constants             = &in_constants;
  this->uniform_params_buffer = &in_uniform_params_buffer;
  this->default_sampler       = &in_default_sampler;
  this->height_map            = &in_height_map;
  this->wind_map              = &in_wind_map;
  this->terrain_size          = in_terrain_size;

  auto& ctx = etna::get_context();

  // Buffer for grass blades
  bladesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(Blade) * MAX_BLADES,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "grass_blades",
  });

  // Buffer for grass generation params
  grassParamsBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(GrassParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "grass_params",
  });
  grassParamsMapping = grassParamsBuffer.map();
}

void GrassRenderer::loadShaders()
{
  etna::create_program("grass_gen", {GRASS_RENDERER_SHADERS_ROOT "grass_gen.comp.spv"});
  etna::create_program("grass_render",
    {GRASS_RENDERER_SHADERS_ROOT "grass.vert.spv",
    GRASS_RENDERER_SHADERS_ROOT "grass.frag.spv"});
}

void GrassRenderer::setupPipelines(vk::Format swapchain_format) {
  auto& pipelineManager = etna::get_context().getPipelineManager();

  grassGenPipeline = pipelineManager.createComputePipeline("grass_gen", {});

  grassRenderPipeline = pipelineManager.createGraphicsPipeline(
    "grass_render",
    etna::GraphicsPipeline::CreateInfo{
      .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::eTriangleList},
      .rasterizationConfig = vk::PipelineRasterizationStateCreateInfo{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.f,
      },
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {swapchain_format},
        .depthAttachmentFormat = vk::Format::eD32Sfloat,
      },
    });
}

void GrassRenderer::update(const glm::vec3& in_camera_pos)
{
  this->camera_pos = in_camera_pos;
  GrassParams params;
  params.eyePos       = in_camera_pos;
  params.terrainSize  = terrain_size;
  params.grassHeight  = grassHeight;
  params.grassDensity = static_cast<float>(grassDensity);
  params.grassRadius  = grassRadius;
  params.grassWidth   = grassWidth;
  bladeCount = static_cast<std::uint32_t>(grassDensity * 100.0f);
  std::memcpy(grassParamsMapping, &params, sizeof(GrassParams));
}

void GrassRenderer::render(vk::CommandBuffer cmd_buf) {
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, grassRenderPipeline.getVkPipeline());

  auto shaderInfo = etna::get_shader_program("grass_render");
  if (shaderInfo.isDescriptorSetUsed(0)) {
    auto descSet = etna::create_descriptor_set(
      shaderInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, bladesBuffer.genBinding()},
        etna::Binding{2, constants->genBinding()},
        etna::Binding{3, grassParamsBuffer.genBinding()},
        etna::Binding{4, wind_map->genBinding(default_sampler->get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{5, uniform_params_buffer->genBinding()},
      });
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, grassRenderPipeline.getVkPipelineLayout(), 0,
      {descSet.getVkSet()}, {});
  }

  cmd_buf.draw(bladeCount * 6, 1, 0, 0);
}

void GrassRenderer::generateGrass(vk::CommandBuffer cmd_buf) {
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, grassGenPipeline.getVkPipeline());

  auto shaderInfo = etna::get_shader_program("grass_gen");
  if (shaderInfo.isDescriptorSetUsed(0)) {
    auto descSet = etna::create_descriptor_set(
      shaderInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, bladesBuffer.genBinding()},
        etna::Binding{1, height_map->genBinding(default_sampler->get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{2, grassParamsBuffer.genBinding()},
      });
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, grassGenPipeline.getVkPipelineLayout(), 0,
      {descSet.getVkSet()}, {});
  }

  std::uint32_t gridSize = static_cast<uint32_t>(ceil(sqrt(static_cast<double>(bladeCount))));
  groupCountX = (gridSize + 31) / 32;
  groupCountY = (gridSize + 31) / 32;
  cmd_buf.dispatch(groupCountX, groupCountY, 1);
}

void GrassRenderer::setGrassDensity(int density)
{
  grassDensity = density;
}

void GrassRenderer::setGrassHeight(float height)
{
  grassHeight = height;
}

void GrassRenderer::setGrassRadius(float radius)
{
  grassRadius = radius;
}

void GrassRenderer::setGrassWidth(float width)
{
  grassWidth = width;
}
