#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include <algorithm>


WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()}
{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  mainViewDepth = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });

  constexpr uint16_t MAX_INSTANCES = 5'000;
  instanceMatricesBuffer = ctx.createBuffer(etna::Buffer::CreateInfo
  {
    .size = MAX_INSTANCES * sizeof(glm::mat4x4),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "instance_matrices",
  });
  persistentMapping = instanceMatricesBuffer.map();

}

WorldRenderer::~WorldRenderer() = default;

void WorldRenderer::loadScene(std::filesystem::path path)
{
  sceneMgr->selectScene(path);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "static_mesh_material",
    {MANY_OBJECTS_BASE_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     MANY_OBJECTS_BASE_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  etna::create_program("static_mesh", {MANY_OBJECTS_BASE_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
    }},
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  staticMeshPipeline = {};
  staticMeshPipeline = pipelineManager.createGraphicsPipeline(
    "static_mesh_material",
    etna::GraphicsPipeline::CreateInfo
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo
        {
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
}

void WorldRenderer::debugInput(const Keyboard&) {}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
  }

  auto instanceMeshes = sceneMgr->getInstanceMeshes();
  auto instanceMatricesData = sceneMgr->getInstanceMatrices();

  if (instanceMeshes.empty())
    return;

  const size_t instanceCount = instanceMeshes.size();
  
  static std::vector<std::pair<uint32_t, uint32_t>> meshInstancePairs;
  meshInstancePairs.clear();
  meshInstancePairs.reserve(instanceCount);
  
  for (size_t i = 0; i < instanceCount; ++i)
  {
    meshInstancePairs.emplace_back(instanceMeshes[i], static_cast<uint32_t>(i));
  }

  std::sort(meshInstancePairs.begin(), meshInstancePairs.end());

  instanceGroups.clear();
  instanceMatrices.clear();
  instanceMatrices.reserve(instanceCount);

  if (meshInstancePairs.empty())
    return;
  uint32_t currentMesh = meshInstancePairs[0].first;
  uint32_t groupStart = 0;

  for (size_t i = 0; i < meshInstancePairs.size(); ++i)
  {
    const auto [meshIdx, instanceIdx] = meshInstancePairs[i];
    instanceMatrices.push_back(instanceMatricesData[instanceIdx]);

    if (meshIdx != currentMesh || i == meshInstancePairs.size() - 1)
    {
      uint32_t groupSize = static_cast<uint32_t>(i) - groupStart;
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

  if (!instanceMatrices.empty() && persistentMapping)
  {
    const size_t copySize = instanceMatrices.size() * sizeof(glm::mat4x4);
    std::memcpy(persistentMapping, instanceMatrices.data(), copySize);
  }
}

void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer() || instanceGroups.empty())
    return;

  cmd_buf.bindVertexBuffers(0, {sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);

  auto shaderInfo = etna::get_shader_program("static_mesh_material");
  if (shaderInfo.isDescriptorSetUsed(0))
  {
    instanceMatricesDescriptorSet = etna::create_descriptor_set(
      shaderInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {etna::Binding{0, instanceMatricesBuffer.genBinding()}});

    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, 
      {instanceMatricesDescriptorSet.getVkSet()}, {});
  }
  else 
  {
    return;
  }

  // Push constants
  struct PushConstantsCompat { glm::mat4x4 mProjView; } pushConstCompat;
  pushConstCompat.mProjView = glob_tm;
  cmd_buf.pushConstants<PushConstantsCompat>(
    pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, {pushConstCompat});

  // render group
  const std::span<const Mesh>& meshes = sceneMgr->getMeshes();
  const std::span<const RenderElement>& renderElements = sceneMgr->getRenderElements();

  for (const auto& group : instanceGroups)
  {
    if (group.meshIdx >= meshes.size())
      continue;

    const auto&[firstRelem, relemCount] = meshes[group.meshIdx];

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
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  // draw final scene to screen
  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }
}