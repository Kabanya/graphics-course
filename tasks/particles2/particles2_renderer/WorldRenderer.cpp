#include "WorldRenderer.hpp"

#include <algorithm>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <etna/RenderTargetStates.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include <vector>

#include "etna/Etna.hpp"
#include "shaders/UniformParams.h"

WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()},
    gui{std::make_unique<WorldRendererGui>(*this)}
{
  groupCountX = (terrainTextureSizeWidth + computeWorkgroupSize - 1) / computeWorkgroupSize;
  groupCountY = (terrainTextureSizeHeight + computeWorkgroupSize - 1) / computeWorkgroupSize;
}

void WorldRenderer::allocateResources(const glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});

  mainViewDepth = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });

  constants = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(WorldRendererConstants),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "world_renderer_constants",
  });

  uniformParamsBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "uniform_params",
  });
  uniformMapping = uniformParamsBuffer.map();

  perlinValuesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(PerlinParams),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "perlin_values",
  });
  perlinValuesMapping = perlinValuesBuffer.map();

  std::memcpy(perlinValuesMapping, &perlinParams, sizeof(PerlinParams));

  maxInstances = 1;
  instanceMatricesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = maxInstances * sizeof(glm::mat4x4),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "instance_matrices",
  });
  persistentMapping = instanceMatricesBuffer.map();

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

  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  createTerrainMap(cmdBuf);
  ETNA_CHECK_VK_RESULT(cmdBuf.end());

  cmdManager->submitAndWait(cmdBuf);
}

void WorldRenderer::loadScene(const std::filesystem::path& path)
{
  sceneMgr->selectScene(path);

  auto instanceCount = sceneMgr->getInstanceMatrices().size();
  if (instanceCount > maxInstances)
  {
    maxInstances = static_cast<std::uint32_t>(instanceCount);

    instanceMatricesBuffer = {};
    persistentMapping = nullptr;

    auto& ctx = etna::get_context();
    instanceMatricesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo
    {
      .size = maxInstances * sizeof(glm::mat4x4),
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .name = "instance_matrices",
    });
    persistentMapping = instanceMatricesBuffer.map();
  }
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "static_mesh_material",
    {PARTICLES2_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     PARTICLES2_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  etna::create_program("perlin_terrain", {PARTICLES2_RENDERER_SHADERS_ROOT "terrain_perlin.comp.spv"});
  etna::create_program("normal_map_generation", {PARTICLES2_RENDERER_SHADERS_ROOT "terrain_normal.comp.spv"});
  etna::create_program(
    "terrain_render",
    {PARTICLES2_RENDERER_SHADERS_ROOT "quad.vert.spv",
     PARTICLES2_RENDERER_SHADERS_ROOT "terrain.tesc.spv",
     PARTICLES2_RENDERER_SHADERS_ROOT "terrain.tese.spv",
     PARTICLES2_RENDERER_SHADERS_ROOT "terrain.frag.spv"});
  etna::create_program("particle_render", {PARTICLES2_RENDERER_SHADERS_ROOT "particle.frag.spv", PARTICLES2_RENDERER_SHADERS_ROOT "particle.vert.spv"});
  etna::create_program("particle_calculate", {PARTICLES2_RENDERER_SHADERS_ROOT "particle_calculate.comp.spv"});
  etna::create_program("particle_integrate", {PARTICLES2_RENDERER_SHADERS_ROOT "particle_integrate.comp.spv"});
  etna::create_program("particle_spawn", {PARTICLES2_RENDERER_SHADERS_ROOT "particle_spawn.comp.spv"});
  etna::create_program("particle_sort", {PARTICLES2_RENDERER_SHADERS_ROOT "particle_sort.comp.spv"});
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
    }},
  };

  auto& ctx = etna::get_context();
  auto& pipelineManager = ctx.getPipelineManager();

  quadRenderer = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{
    .format = swapchain_format,
    .rect = {{0, 0}, {512, 512}},
  });

  particleSystem = std::make_unique<ParticleSystem>();

  particleSystem->setupPipelines();

  staticMeshPipeline = {};
  staticMeshPipeline = pipelineManager.createGraphicsPipeline(
    "static_mesh_material",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = sceneVertexInputDesc,
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
    });
  perlinPipeline            = pipelineManager.createComputePipeline("perlin_terrain", {});
  normalPipeline            = pipelineManager.createComputePipeline("normal_map_generation", {});

  terrainPipeline           = pipelineManager.createGraphicsPipeline(
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
    });
  particlePipeline = pipelineManager.createGraphicsPipeline(
    "particle_render",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = etna::VertexShaderInputDescription{
        .bindings = {etna::VertexShaderInputDescription::Binding{
          .byteStreamDescription = etna::VertexByteStreamFormatDescription{
            .stride = sizeof(ParticleGPU),
            .attributes = {
              etna::VertexByteStreamFormatDescription::Attribute{
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = 0,
              },
              etna::VertexByteStreamFormatDescription::Attribute{
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = 16,
              },
            },
          },
        }},
      },
      .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePointList},
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eNone,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .blendingConfig = {
        .attachments = {vk::PipelineColorBlendAttachmentState{
          .blendEnable = VK_TRUE,
          .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
          .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
          .colorBlendOp = vk::BlendOp::eAdd,
          .srcAlphaBlendFactor = vk::BlendFactor::eOne,
          .dstAlphaBlendFactor = vk::BlendFactor::eZero,
          .alphaBlendOp = vk::BlendOp::eAdd,
          .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        }},
        .logicOpEnable = false,
        .logicOp = vk::LogicOp::eCopy,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {swapchain_format},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });
}

bool WorldRenderer::isVisibleBoundingBox(
  const glm::vec3& min, const glm::vec3& max, const glm::mat4& mvp) const
{
  const std::array<glm::vec3, 8> vertices =
  {
    glm::vec3(min.x, min.y, min.z),
    glm::vec3(min.x, min.y, max.z),
    glm::vec3(min.x, max.y, min.z),
    glm::vec3(min.x, max.y, max.z),
    glm::vec3(max.x, min.y, min.z),
    glm::vec3(max.x, min.y, max.z),
    glm::vec3(max.x, max.y, min.z),
    glm::vec3(max.x, max.y, max.z),
  };

  return std::ranges::any_of(vertices, [&](const glm::vec3& v)
  {
      glm::vec4 clip = mvp * glm::vec4(v, 1.0f);
      if (clip.w != 0.0f)
      {
        float x = clip.x / clip.w;
        float y = clip.y / clip.w;
        float z = clip.z / clip.w;
        return x >= -1.0f && x <= 1.0f && y >= -1.0f && y <= 1.0f && z >= 0.0f && z <= 1.0f;
      }
    return false;
  });
}

void WorldRenderer::debugInput(const Keyboard& kb)
{
  if (kb[KeyboardKey::k1] == ButtonState::Falling) {
    enableFrustumCulling = !enableFrustumCulling;
    printf("Frustum Culling: %s\n", enableFrustumCulling ? "ON" : "OFF");
  }
  if (kb[KeyboardKey::k2] == ButtonState::Falling) {
    enableTessellation = !enableTessellation;
    printf("Tessellation: %s\n", enableTessellation ? "ON" : "OFF");
  }
  if (kb[KeyboardKey::k3] == ButtonState::Falling) {
    enableSceneRendering = !enableSceneRendering;
    printf("Avocados Rendering: %s\n", enableSceneRendering ? "ON" : "OFF");
  }
  if (kb[KeyboardKey::k4] == ButtonState::Falling) {
    enableTerrainRendering = !enableTerrainRendering;
    printf("Terrain Rendering: %s\n", enableTerrainRendering ? "ON" : "OFF");
  }
  if (kb[KeyboardKey::k5] == ButtonState::Falling) {
    enableParticleRendering = !enableParticleRendering;
    printf("Particle Rendering: %s\n", enableParticleRendering ? "ON" : "OFF");
  }
  if (kb[KeyboardKey::kZ] == ButtonState::Falling) {
    showTabs = !showTabs;
    printf("GUI tabs %s\n", showTabs ? "shown" : "hidden");
  }
  if (kb[KeyboardKey::kQ] == ButtonState::Falling) {
    drawDebugTerrainQuad = !drawDebugTerrainQuad;
    printf("Debug Terrain Quad: %s\n", drawDebugTerrainQuad ? "ON" : "OFF");
  }
}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  std::ranges::sort(emittersToRemove);
  for (auto idx : emittersToRemove)
    particleSystem->removeEmitter(idx);
  emittersToRemove.clear();

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
    camView = packet.mainCam.position;
  }

  std::memcpy(uniformMapping, &uniformParams, sizeof(UniformParams));

  float dt = packet.currentTime - previousTime;
  previousTime = packet.currentTime;

  particleSystem->update(dt, wind);

  currentParticleCount = 0;
  totalParticles = 0;
  if (!particleSystem->emitters.empty()) {
    for (const auto& emitter : particleSystem->emitters)
      totalParticles += emitter.currentParticleCount;
    currentParticleCount = totalParticles;
  }
  totalParticles = currentParticleCount;
  while (totalParticles >= nextMilestone && fpsMilestones.find(nextMilestone) == fpsMilestones.end()) {
    fpsMilestones[nextMilestone] = ImGui::GetIO().Framerate;
    nextMilestone += 5000;
  }

  std::memcpy(perlinValuesMapping, &perlinParams, sizeof(PerlinParams));

  WorldRendererConstants worldConstants{
    .viewProj = worldViewProj,
    .camView = glm::vec4(camView, 1.f),
    .enableTessellation = enableTessellation ? 1 : 0
  };

  void* constantsMapping = constants.map();
  std::memcpy(constantsMapping, &worldConstants, sizeof(WorldRendererConstants));
  constants.unmap();

  auto instanceMeshes = sceneMgr->getInstanceMeshes();
  auto instanceMatricesData = sceneMgr->getInstanceMatrices();

  if (instanceMeshes.empty())
    return;

  const size_t instanceCount = instanceMeshes.size();
  static std::vector<std::pair<std::uint32_t, std::uint32_t>> meshInstancePairs;
  meshInstancePairs.clear();
  meshInstancePairs.reserve(instanceCount);

  for (size_t i = 0; i < instanceCount; ++i)
    meshInstancePairs.emplace_back(instanceMeshes[i], static_cast<std::uint32_t>(i));

  std::ranges::sort(meshInstancePairs);

  static std::vector<std::pair<std::uint32_t, std::uint32_t>> visiblePairs;
  visiblePairs.clear();
  visiblePairs.reserve(instanceCount);

  if (enableFrustumCulling)
  {
    for (const auto& pair : meshInstancePairs)
    {
      auto [meshIdx, instanceIdx] = pair;
      auto instanceMatrix = instanceMatricesData[instanceIdx];

      bool visible = false;

      const auto& meshes = sceneMgr->getMeshes();
      const auto& bboxes = sceneMgr->getRelemsBoundingBoxes();
      const auto& mesh = meshes[meshIdx];

      for (std::uint32_t j = 0; j < mesh.relemCount; ++j)
      {
        std::uint32_t relemIdx = mesh.firstRelem + j;
        const auto& bbox = bboxes[relemIdx];
        glm::vec3 min(bbox.aabb.minX, bbox.aabb.minY, bbox.aabb.minZ);
        glm::vec3 max(bbox.aabb.maxX, bbox.aabb.maxY, bbox.aabb.maxZ);
        if (isVisibleBoundingBox(min, max, worldViewProj * instanceMatrix))
        {
          visible = true;
          break;
        }
      }
      if (visible)
        visiblePairs.push_back(pair);
    }
  }
  else
  {
    visiblePairs = meshInstancePairs;
  }

  meshInstancePairs = std::move(visiblePairs);

  instanceGroups.clear();
  instanceMatrices.clear();
  instanceMatrices.reserve(instanceCount);

  if (meshInstancePairs.empty())
    return;
  std::uint32_t currentMesh = meshInstancePairs[0].first;
  std::uint32_t groupStart = 0;

  for (size_t i = 0; i < meshInstancePairs.size(); ++i)
  {
    const auto [meshIdx, instanceIdx] = meshInstancePairs[i];
    instanceMatrices.push_back(instanceMatricesData[instanceIdx]);

    if (meshIdx != currentMesh || i == meshInstancePairs.size() - 1)
    {
      std::uint32_t groupSize = static_cast<uint32_t>(i) - groupStart;
      if (meshIdx == currentMesh && i == meshInstancePairs.size() - 1)
        groupSize++;

      instanceGroups.push_back(InstanceGroup{
        .meshIdx = currentMesh,
        .firstInstance = groupStart,
        .instanceCount = groupSize,
      });

      if (meshIdx != currentMesh)
      {
        currentMesh = meshIdx;
        groupStart = static_cast<uint32_t>(i);
      }
    }
  }

  if (!instanceMatrices.empty() && (persistentMapping != nullptr))
  {
    const size_t copySize = instanceMatrices.size() * sizeof(glm::mat4x4);
    std::memcpy(persistentMapping, instanceMatrices.data(), copySize);
  }
}

void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer() || instanceGroups.empty())
    return;

  renderedInstances = 0;
  for (const auto& group : instanceGroups) {
    renderedInstances += group.instanceCount;
  }

  cmd_buf.bindVertexBuffers(0, {sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);

  auto shaderInfo = etna::get_shader_program("static_mesh_material");
  if (shaderInfo.isDescriptorSetUsed(0))
  {
    auto descSet = etna::create_descriptor_set(
      shaderInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, instanceMatricesBuffer.genBinding()},
        etna::Binding{1, constants.genBinding()},
      });

    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
      {descSet.getVkSet()}, {});
  }
  else
  {
    return;
  }

  // render group
  const std::span<const Mesh>& meshes = sceneMgr->getMeshes();
  const std::span<const RenderElement>& renderElements = sceneMgr->getRenderElements();

  for (const auto& group : instanceGroups)
  {
    if (group.meshIdx >= meshes.size())
      continue;

    const auto& mesh = meshes[group.meshIdx];
    const auto& firstRelem = mesh.firstRelem;
    const auto& relemCount = mesh.relemCount;

    for (size_t j = 0; j < relemCount; ++j)
    {
      const uint64_t renderElemId = firstRelem + j;
      if (renderElemId >= renderElements.size())
        continue;

      const RenderElement& renderElemIndex = renderElements[renderElemId];

      cmd_buf.drawIndexed(
        renderElemIndex.indexCount,
        group.instanceCount,
        renderElemIndex.indexOffset,
        renderElemIndex.vertexOffset,
        group.firstInstance);
    }
  }

  etna::flush_barriers(cmd_buf);
}

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld)
  {
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    if (enableSceneRendering)
    {
      ETNA_PROFILE_GPU(cmd_buf, renderScene);
      cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
      renderScene(cmd_buf, staticMeshPipeline.getVkPipelineLayout());
    }
    if (enableTerrainRendering)
    {
      ETNA_PROFILE_GPU(cmd_buf, renderTerrain);
      cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, terrainPipeline.getVkPipeline());
      renderTerrain(cmd_buf);
    }
  }

  if (enableParticleRendering) {
    ETNA_PROFILE_GPU(cmd_buf, sortParticles);
    particleSystem->sortAllEmitters(cmd_buf, camView);
    etna::flush_barriers(cmd_buf);
  }

  if (enableParticleRendering) {
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view, .loadOp = vk::AttachmentLoadOp::eLoad}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, particlePipeline.getVkPipeline());
    auto shaderInfo = etna::get_shader_program("particle_render");
    if (shaderInfo.isDescriptorSetUsed(0)) {
      auto descSet = etna::create_descriptor_set(
        shaderInfo.getDescriptorLayoutId(0),
        cmd_buf,
        {
          etna::Binding{0, constants.genBinding()},
          etna::Binding{1, uniformParamsBuffer.genBinding()},
        });
      cmd_buf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, particlePipeline.getVkPipelineLayout(), 0,
        {descSet.getVkSet()}, {});
    }
    particleSystem->render(cmd_buf);
  }

  if (drawDebugTerrainQuad)
    quadRenderer->render(cmd_buf, target_image, target_image_view, perlinTerrainImage, defaultSampler);
}

void WorldRenderer::renderTerrain(vk::CommandBuffer cmd_buf) const
{
  auto info = etna::get_shader_program("terrain_render");
  auto perlinBind = perlinTerrainImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
  auto normalBind = normalMapTerrainImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

  auto descSet = etna::create_descriptor_set(
    info.getDescriptorLayoutId(0),
    cmd_buf,
    {
      etna::Binding{0, perlinBind},
      etna::Binding{1, normalBind},
      etna::Binding{2, constants.genBinding()},
      etna::Binding{3, uniformParamsBuffer.genBinding()},
    });
  auto vkSet = descSet.getVkSet();
  auto layout = terrainPipeline.getVkPipelineLayout();

  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &vkSet, 0, nullptr);

  cmd_buf.draw(4, (terrainTextureSizeWidth * terrainTextureSizeHeight) / (computeWorkgroupSize * computeWorkgroupSize * 16), 0, 0);
}

void WorldRenderer::regenerateTerrain()
{
  auto& ctx = etna::get_context();
  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  createTerrainMap(cmdBuf);
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdManager->submitAndWait(cmdBuf);
}

void WorldRenderer::createTerrainMap(const vk::CommandBuffer cmd_buf)
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

    auto binding = perlinTerrainImage.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral, {});

    auto set = etna::create_descriptor_set(
      perlinInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, binding},
        etna::Binding{1, perlinValuesBuffer.genBinding()},
      });

    const vk::DescriptorSet vkSet = set.getVkSet();

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
    auto binding0 = perlinTerrainImage.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral, {});
    auto binding1 = normalMapTerrainImage.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral, {});

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

float WorldRenderer::getCameraSpeed() const
{
  switch (cameraSpeedLevel)
  {
    case CameraSpeedLevel::Slow:
      return 1.0f;
    case CameraSpeedLevel::Middle:
      return 25.0f;
    case CameraSpeedLevel::Fast:
      return 50.0f;
    default:
      return 40.0f;
  }
}

void WorldRenderer::drawGui() const
{
  gui->drawGui();
}
