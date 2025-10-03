#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#include "WorldRendererGui.hpp"

WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()},
    gui{std::make_unique<WorldRendererGui>(*this)},
    terrainRenderer{std::make_unique<TerrainRenderer>()},
    grassRenderer{std::make_unique<GrassRenderer>()}
{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  default_sampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});

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
  // constants.map();

  uniform_params_buffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "uniform_params",
  });
  uniformMapping = uniform_params_buffer.map();

  maxInstances = 1;
  instanceMatricesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo
  {
    .size = maxInstances * sizeof(glm::mat4x4),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "instance_matrices",
  });
  persistentMapping = instanceMatricesBuffer.map();

  terrainRenderer->allocateResources(constants, uniform_params_buffer, default_sampler);
  grassRenderer->allocateResources(constants, uniform_params_buffer, default_sampler, terrainRenderer->getPerlinTerrainImage(), terrainRenderer->getWindImage(), terrainRenderer->getTerrainWorldSize());
}

void WorldRenderer::loadScene(std::filesystem::path path)
{
  sceneMgr->selectScene(path);

  auto instanceCount = sceneMgr->getInstanceMatrices().size();
  if (instanceCount > maxInstances)
  {
    maxInstances = static_cast<uint32_t>(instanceCount);

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
    {GRASS_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     GRASS_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  terrainRenderer->loadShaders();
  grassRenderer->loadShaders();
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
    }},
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  quadRenderer = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{
    .format = swapchain_format,
    .rect = {{0, 0}, {512, 512}},
  });

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
  terrainRenderer->setupPipelines(swapchain_format);
  grassRenderer->setupPipelines(swapchain_format);
}

bool WorldRenderer::isVisibleBoundingBox(const glm::vec3& min, const glm::vec3& max, const glm::mat4& mvp) const
{
  std::array<glm::vec3, 8> vertices =
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

  for (const auto& v : vertices)
  {
    glm::vec4 clip = mvp * glm::vec4(v, 1.0f);
    if (clip.w != 0.0f) {
      float x = clip.x / clip.w;
      float y = clip.y / clip.w;
      float z = clip.z / clip.w;
      if (x >= -1.0f && x <= 1.0f && y >= -1.0f && y <= 1.0f && z >= 0.0f && z <= 1.0f)
        return true;
    }
  }
  return false;
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
    enableGrassRendering = !enableGrassRendering;
    printf("Grass Rendering: %s\n", enableGrassRendering ? "ON" : "OFF");
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

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
    camView = packet.mainCam.position;
  }

  std::memcpy(uniformMapping, &uniformParams, sizeof(UniformParams));

  WorldRendererConstants worldConstants{
    .viewProj = worldViewProj,
    .camView = glm::vec4(camView, 1.f),
    .enableTessellation = enableTessellation ? 1 : 0
  };

  void* constantsMapping = constants.map();
  std::memcpy(constantsMapping, &worldConstants, sizeof(WorldRendererConstants));
  constants.unmap();

  terrainRenderer->update(perlinParams);
  terrainRenderer->updateWind(uniformParams.time);
  grassRenderer->update(camView);

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

  std::sort(meshInstancePairs.begin(), meshInstancePairs.end());

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

      for (uint32_t j = 0; j < mesh.relemCount; ++j)
      {
        uint32_t relemIdx = mesh.firstRelem + j;
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
      std::uint32_t groupSize = static_cast<std::uint32_t>(i) - groupStart;
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
        groupStart = static_cast<std::uint32_t>(i);
      }
    }
  }

  if (!instanceMatrices.empty() && (persistentMapping != nullptr))
  {
    const std::size_t copySize = instanceMatrices.size() * sizeof(glm::mat4x4);
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
      const std::uint64_t renderElemId = firstRelem + j;
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
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  // Generate grass blades outside render pass
  if (enableGrassRendering)
    grassRenderer->generateGrass(cmd_buf);

  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    if (enableSceneRendering) {
      cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
      renderScene(cmd_buf, staticMeshPipeline.getVkPipelineLayout());
    }
  }

  if (enableTerrainRendering)
  {
    ETNA_PROFILE_GPU(cmd_buf, renderTerrain);
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view, .loadOp = vk::AttachmentLoadOp::eLoad}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad});

    terrainRenderer->render(cmd_buf);
  }

  // Render grass
  if (enableGrassRendering)
  {
    ETNA_PROFILE_GPU(cmd_buf, renderGrass);
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view, .loadOp = vk::AttachmentLoadOp::eLoad}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad});

    grassRenderer->render(cmd_buf);
  }

  if (drawDebugTerrainQuad)
    quadRenderer->render(cmd_buf, target_image, target_image_view, terrainRenderer->getPerlinTerrainImage(), default_sampler);
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
      return 50.0f;
  }
}

const PerlinParams& WorldRenderer::getPerlinParams() const
{
  return perlinParams;
}

void WorldRenderer::setPerlinParams(const PerlinParams& params)
{
  perlinParams = params;
  terrainRenderer->update(perlinParams);
}

void WorldRenderer::regenerateTerrain()
{
  terrainRenderer->regenerateTerrain();
}

void WorldRenderer::drawGui()
{
  gui->drawGui();
}