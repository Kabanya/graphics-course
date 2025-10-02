#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/DescriptorSet.hpp>
#include <glm/glm.hpp>
#include <memory>

#include "TerrainRenderer.hpp"

#include "WorldRendererGui.hpp"
#include "shaders/UniformParams.h"
#include "scene/SceneManager.hpp"
#include "wsi/Keyboard.hpp"
#include "render_utils/QuadRenderer.hpp"

#include "FramePacket.hpp"

struct InstanceGroup
{
  uint32_t meshIdx;
  uint32_t firstInstance;
  uint32_t instanceCount;
};

enum class CameraSpeedLevel
{
  Slow = 0,
  Middle = 1,
  Fast = 2
};

class WorldRenderer
{
  friend WorldRendererGui;
public:
  explicit WorldRenderer();

  void loadScene(std::filesystem::path path);

  void loadShaders();
  void allocateResources(glm::uvec2 swapchain_resolution);
  void setupPipelines(vk::Format swapchain_format);

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void renderWorld(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);
  const PerlinParams& getPerlinParams() const;
  void setPerlinParams(const PerlinParams& params);
  void regenerateTerrain();

  float getCameraSpeed() const;
  void drawGui();

private:
  void renderScene(
    vk::CommandBuffer cmd_buf, vk::PipelineLayout pipeline_layout);

  bool isVisibleBoundingBox(const glm::vec3& min, const glm::vec3& max, const glm::mat4& mvp) const;

private:
  // Scene and managers
  std::unique_ptr<SceneManager>     sceneMgr;
  std::unique_ptr<WorldRendererGui> gui;
  std::unique_ptr<QuadRenderer>     quadRenderer;
  std::unique_ptr<TerrainRenderer>  terrainRenderer;

  // Pipelines
  etna::GraphicsPipeline staticMeshPipeline{};

  // Images and textures
  etna::Image   mainViewDepth;
  etna::Sampler default_sampler;

  // Buffers
  etna::Buffer instanceMatricesBuffer;
  etna::Buffer constants;
  etna::Buffer uniform_params_buffer;

  // Buffer mappings
  void* persistentMapping = nullptr;
  void* uniformMapping = nullptr;

  // Instance data
  std::vector<InstanceGroup> instanceGroups;
  std::vector<glm::mat4>     instanceMatrices;
  std::uint32_t maxInstances      = 0;
  std::uint32_t renderedInstances = 0;

  // Camera parameters
  glm::mat4x4 worldViewProj;
  glm::vec3 camView;
  float nearPlane;
  float farPlane;
  CameraSpeedLevel cameraSpeedLevel = CameraSpeedLevel::Fast;

  struct WorldRendererConstants{
    glm::mat4 viewProj;
    glm::vec4 camView;
    int enableTessellation;
  };

  UniformParams uniformParams{
    .lightMatrix = {},
    .lightPos = {},
    .time = {},
    .baseColor = {0.9f, 0.92f, 1.0f},
  };

  PerlinParams perlinParams{
    .octaves = 10u,
    .amplitude = 0.5f,
    .frequencyMultiplier = 2.0f,
    .scale = 8.0f,
  };

  // Render toggles
  bool enableFrustumCulling    = true;
  bool enableTessellation      = true;
  bool enableTerrainRendering  = true;
  bool enableSceneRendering    = true;
  bool enableParticleRendering = true;

  // UI toggles
  bool showPerformanceInfo     = true;
  bool showTerrainSettings     = true;
  bool drawDebugTerrainQuad    = false;
  bool showTabs                = true;

  glm::uvec2 resolution;
};