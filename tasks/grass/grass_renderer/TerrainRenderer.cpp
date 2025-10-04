#include "TerrainRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/OneShotCmdMgr.hpp>

TerrainRenderer::TerrainRenderer()
{
  groupCountX = (terrainTextureSizeWidth + computeWorkgroupSize - 1) / computeWorkgroupSize;
  groupCountY = (terrainTextureSizeHeight + computeWorkgroupSize - 1) / computeWorkgroupSize;
}

void TerrainRenderer::allocateResources(
  etna::Buffer&  in_constants,
  etna::Buffer&  in_uniform_params_buffer,
  etna::Sampler& in_default_sampler)
{
  this->constants             = &in_constants;
  this->uniform_params_buffer = &in_uniform_params_buffer;
  this->default_sampler       = &in_default_sampler;

  auto& ctx = etna::get_context();

  perlinValuesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(PerlinParams),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "perlin_values",
  });
  perlinValuesMapping = perlinValuesBuffer.map();

  windValuesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(PerlinParams),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "wind_values",
  });
  windValuesMapping = windValuesBuffer.map();

  std::memcpy(perlinValuesMapping, &perlinParams, sizeof(PerlinParams));
  std::memcpy(windValuesMapping, &windParams, sizeof(PerlinParams));

  perlinTerrainImage = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{terrainTextureSizeWidth, terrainTextureSizeHeight, 1},
    .name = "perlin_noise",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  normalMapTerrainImage = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{terrainTextureSizeWidth, terrainTextureSizeHeight, 1},
    .name = "normal_map_terrain",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  windImage = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{terrainTextureSizeWidth, terrainTextureSizeHeight, 1},
    .name = "wind_map",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage});

  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  createTerrainMap(cmdBuf);
  createWindMap(cmdBuf);
  ETNA_CHECK_VK_RESULT(cmdBuf.end());

  cmdManager->submitAndWait(cmdBuf);
}

void TerrainRenderer::loadShaders()
{
  etna::create_program("perlin_terrain", {GRASS_RENDERER_SHADERS_ROOT "terrain_perlin.comp.spv"});
  etna::create_program("normal_map_generation", {GRASS_RENDERER_SHADERS_ROOT "terrain_normal.comp.spv"});
  etna::create_program("wind_perlin", {GRASS_RENDERER_SHADERS_ROOT "wind_perlin.comp.spv"});
  etna::create_program(
    "terrain_render",
    {GRASS_RENDERER_SHADERS_ROOT "terrain.vert.spv",
     GRASS_RENDERER_SHADERS_ROOT "terrain.tesc.spv",
     GRASS_RENDERER_SHADERS_ROOT "terrain.tese.spv",
     GRASS_RENDERER_SHADERS_ROOT "terrain.frag.spv"});
}

void TerrainRenderer::setupPipelines(vk::Format swapchain_format)
{
  auto& pipelineManager = etna::get_context().getPipelineManager();

  perlinPipeline  = pipelineManager.createComputePipeline("perlin_terrain", {});
  normalPipeline  = pipelineManager.createComputePipeline("normal_map_generation", {});
  windPipeline    = pipelineManager.createComputePipeline("wind_perlin", {});
  terrainPipeline = pipelineManager.createGraphicsPipeline(
    "terrain_render",
    etna::GraphicsPipeline::CreateInfo{
      .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePatchList},
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eBack,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {swapchain_format},
        .depthAttachmentFormat = vk::Format::eD32Sfloat,
      },
    }
  );
}

void TerrainRenderer::update(const PerlinParams& params)
{
  perlinParams = params;
  std::memcpy(perlinValuesMapping, &perlinParams, sizeof(PerlinParams));
}

void TerrainRenderer::updateWind(float time)
{
  windParams.time = time;
  std::memcpy(windValuesMapping, &windParams, sizeof(PerlinParams));

  auto& ctx = etna::get_context();
  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  createWindMap(cmdBuf);
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdManager->submitAndWait(cmdBuf);
}

void TerrainRenderer::render(vk::CommandBuffer cmd_buf)
{
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, terrainPipeline.getVkPipeline());

  auto info = etna::get_shader_program("terrain_render");
  auto perlinBind = perlinTerrainImage.genBinding(default_sampler->get(), vk::ImageLayout::eShaderReadOnlyOptimal);
  auto normalBind = normalMapTerrainImage.genBinding(default_sampler->get(), vk::ImageLayout::eShaderReadOnlyOptimal);

  auto descSet = etna::create_descriptor_set(
    info.getDescriptorLayoutId(0),
    cmd_buf,
    {
      etna::Binding{0, perlinBind},
      etna::Binding{1, normalBind},
      etna::Binding{2, constants->genBinding()},
      etna::Binding{3, uniform_params_buffer->genBinding()},
    });
  auto vkSet = descSet.getVkSet();
  auto layout = terrainPipeline.getVkPipelineLayout();

  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &vkSet, 0, nullptr);

  cmd_buf.draw(4, (terrainTextureSizeWidth * terrainTextureSizeHeight) / (computeWorkgroupSize * computeWorkgroupSize * 16), 0, 0);
}

void TerrainRenderer::regenerateTerrain()
{
  auto& ctx = etna::get_context();
  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  createTerrainMap(cmdBuf);
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdManager->submitAndWait(cmdBuf);
}

void TerrainRenderer::createTerrainMap(vk::CommandBuffer cmd_buf)
{
  etna::set_state(
    cmd_buf,
    perlinTerrainImage.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderWrite,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);

  etna::flush_barriers(cmd_buf);

  {
    auto perlinInfo = etna::get_shader_program("perlin_terrain");

    auto binding = perlinTerrainImage.genBinding(default_sampler->get(), vk::ImageLayout::eGeneral, {});

    auto set = etna::create_descriptor_set(
      perlinInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding},
        etna::Binding{1, perlinValuesBuffer.genBinding()},
      });

    vk::DescriptorSet vkSet = set.getVkSet();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, perlinPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      perlinPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);

    cmd_buf.dispatch(terrainTextureSizeWidth / computeWorkgroupSize, terrainTextureSizeHeight / computeWorkgroupSize, 1);
  }

  etna::set_state(
    cmd_buf,
    perlinTerrainImage.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderRead,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmd_buf,
    normalMapTerrainImage.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderWrite,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);

  {
    auto normalInfo = etna::get_shader_program("normal_map_generation");
    auto binding0 = perlinTerrainImage.genBinding(default_sampler->get(), vk::ImageLayout::eGeneral, {});
    auto binding1 = normalMapTerrainImage.genBinding(default_sampler->get(), vk::ImageLayout::eGeneral, {});

    auto set = etna::create_descriptor_set(
      normalInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding0},
        etna::Binding{1, binding1},
      });

    vk::DescriptorSet vkSet = set.getVkSet();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, normalPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      normalPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);

    cmd_buf.dispatch(groupCountX, groupCountY, 1);
  }

  etna::set_state(
    cmd_buf,
    perlinTerrainImage.get(),
    vk::PipelineStageFlagBits2::eTessellationEvaluationShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);

  etna::set_state(
    cmd_buf,
    normalMapTerrainImage.get(),
    vk::PipelineStageFlagBits2::eTessellationEvaluationShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);
}

void TerrainRenderer::createWindMap(vk::CommandBuffer cmd_buf)
{
  etna::set_state(
    cmd_buf,
    windImage.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderWrite,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);

  {
    auto windInfo = etna::get_shader_program("wind_perlin");

    auto binding = windImage.genBinding(default_sampler->get(), vk::ImageLayout::eGeneral, {});

    auto set = etna::create_descriptor_set(
      windInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding},
        etna::Binding{1, windValuesBuffer.genBinding()},
      });

    vk::DescriptorSet vkSet = set.getVkSet();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, windPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      windPipeline.getVkPipelineLayout(),
      0,
      1,
      &vkSet,
      0,
      nullptr);

    cmd_buf.dispatch(terrainTextureSizeWidth / computeWorkgroupSize, terrainTextureSizeHeight / computeWorkgroupSize, 1);
  }

  etna::set_state(
    cmd_buf,
    windImage.get(),
    vk::PipelineStageFlagBits2::eVertexShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::ImageLayout::eReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);
}

void TerrainRenderer::setWindParams(const PerlinParams& params)
{
  windParams = params;
  std::memcpy(windValuesMapping, &windParams, sizeof(PerlinParams));
  auto& ctx = etna::get_context();
  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  createWindMap(cmdBuf);
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdManager->submitAndWait(cmdBuf);
}

void TerrainRenderer::setTerrainTextureSizeWidth(std::uint32_t w)
{
  terrainTextureSizeWidth = w;
  groupCountX = (terrainTextureSizeWidth + computeWorkgroupSize - 1) / computeWorkgroupSize;
  regenerateTerrain();
}

void TerrainRenderer::setTerrainTextureSizeHeight(std::uint32_t h)
{
  terrainTextureSizeHeight = h;
  groupCountY = (terrainTextureSizeHeight + computeWorkgroupSize - 1) / computeWorkgroupSize;
  regenerateTerrain();
}

void TerrainRenderer::setComputeWorkgroupSize(std::uint32_t s)
{
  computeWorkgroupSize = s;
  groupCountX = (terrainTextureSizeWidth  + computeWorkgroupSize - 1) / computeWorkgroupSize;
  groupCountY = (terrainTextureSizeHeight + computeWorkgroupSize - 1) / computeWorkgroupSize;
  regenerateTerrain();
}

void TerrainRenderer::setPatchSubdivision(std::uint32_t s)
{
  patchSubdivision = s;
}
