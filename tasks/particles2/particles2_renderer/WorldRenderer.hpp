#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/DescriptorSet.hpp>
#include <glm/glm.hpp>

#include "shaders/UniformParams.h"
#include "scene/SceneManager.hpp"
#include "wsi/Keyboard.hpp"
#include "render_utils/QuadRenderer.hpp"

#include "FramePacket.hpp"

#include "ParticleSystem.hpp"
#include "WorldRendererGui.hpp"

struct InstanceGroup
{
  std::uint32_t meshIdx;
  std::uint32_t firstInstance;
  std::uint32_t instanceCount;
};

enum class CameraSpeedLevel
{
  Slow = 0,
  Middle = 1,
  Fast = 2
};

class WorldRenderer
{
  friend class WorldRendererGui;
public:
  explicit WorldRenderer();

  void loadScene(const std::filesystem::path& path);

  void loadShaders();
  void allocateResources(glm::uvec2 swapchain_resolution);
  void setupPipelines(vk::Format swapchain_format);

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawGui() const;
  void renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

  float getCameraSpeed() const;

private:
  void renderScene(vk::CommandBuffer cmd_buf, vk::PipelineLayout pipeline_layout);
  void createTerrainMap(vk::CommandBuffer cmd_buf);
  void renderTerrain(vk::CommandBuffer cmd_buf) const;
  void reallocateTerrainResources();
  void regenerateTerrain();

  bool isVisibleBoundingBox(const glm::vec3& min, const glm::vec3& max, const glm::mat4& mvp) const;

private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image mainViewDepth;
  etna::Image perlinTerrainImage;
  etna::Image normalMapTerrainImage;

  etna::Buffer instanceMatricesBuffer;
  etna::Buffer constants;
  etna::Buffer uniformParamsBuffer;
  etna::Buffer perlinValuesBuffer;

  void* persistentMapping = nullptr;
  void* uniformMapping = nullptr;
  void* perlinValuesMapping = nullptr;
  void* particleSSBOMapping = nullptr;
  void* emitterSSBOMapping = nullptr;
  void* particleCountMapping = nullptr;

  std::uint32_t maxInstances = 0;
  std::uint32_t max_particles = 5'000'000;

  std::vector<InstanceGroup> instanceGroups;
  std::vector<glm::mat4> instanceMatrices;

  glm::mat4x4 worldViewProj;
  glm::vec3 camView;
  float nearPlane;
  float farPlane;

  etna::GraphicsPipeline staticMeshPipeline{};
  etna::ComputePipeline  perlinPipeline{};
  etna::ComputePipeline  normalPipeline{};
  etna::GraphicsPipeline terrainPipeline{};
  etna::GraphicsPipeline particlePipeline{};

  std::unique_ptr<QuadRenderer> quadRenderer;

  std::unique_ptr<ParticleSystem> particleSystem;

  std::unique_ptr<WorldRendererGui> gui;

  glm::vec3 wind = {0.0f, 0.0f, 0.0f};

  struct WorldRendererConstants{
    glm::mat4 viewProj;
    glm::vec4 camView;
    int enableTessellation;
  };

  UniformParams uniformParams{
    .lightMatrix   = {},
    .lightPos      = {},
    .time          = {},
    .baseColor     = {0.9f, 0.92f, 1.0f},
    .particleAlpha = 0.5f,
    .particleColor = {1.0f, 1.0f, 1.0f},
  };

  PerlinParams perlinParams{
    .octaves             = 10u,
    .amplitude           = 0.5f,
    .frequencyMultiplier = 2.0f,
    .scale               = 8.0f,
  };

  etna::Sampler defaultSampler;

  glm::uvec2 resolution;

  bool enableFrustumCulling    = true;
  bool enableTessellation      = true;
  bool enableTerrainRendering  = true;
  bool enableSceneRendering    = true;
  bool enableParticleRendering = true;

  bool showPerformanceInfo     = true;
  bool showTerrainSettings     = true;
  bool drawDebugTerrainQuad    = false;
  bool showTabs                = true;
  bool showFpsMilestones       = true;
  bool clearAllEmitters        = false;

  CameraSpeedLevel cameraSpeedLevel = CameraSpeedLevel::Middle;

  float previousTime = 0.0f;

  std::uint32_t terrainTextureSizeWidth  = 4096;
  std::uint32_t terrainTextureSizeHeight = 4096;
  std::uint32_t computeWorkgroupSize = 32;
  std::uint32_t patchSubdivision = 8;
  std::uint32_t groupCountX;
  std::uint32_t groupCountY;

  std::uint32_t renderedInstances = 0;

  std::size_t totalParticles  = 0;
  std::uint32_t nextMilestone = 20'000;
  std::map<std::uint32_t, float> fpsMilestones;
  std::vector<size_t> emittersToRemove;
  std::uint32_t currentParticleCount = 0;
};