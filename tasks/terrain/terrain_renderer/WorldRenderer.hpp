#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/DescriptorSet.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/glm.hpp>

#include "scene/SceneManager.hpp"
#include "wsi/Keyboard.hpp"

#include "FramePacket.hpp"


class WorldRenderer
{
public:
  WorldRenderer();
  ~WorldRenderer();

  void loadScene(std::filesystem::path path);

  void loadShaders();
  void allocateResources(glm::uvec2 swapchain_resolution);
  void setupPipelines(vk::Format swapchain_format);

  bool isVisibleBoundingBox(const glm::vec3& min, const glm::vec3& max, const glm::mat4& mvp) const;

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawGui();
  void renderWorld(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

private:
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);

  struct InstanceGroup
  {
    std::uint32_t meshIdx;
    std::uint32_t firstInstance;
    std::uint32_t instanceCount;
  };

  void createTerrainMap(vk::CommandBuffer cmd_buf);
  void renderTerrain(vk::CommandBuffer cmd_buf);

private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image mainViewDepth;
  etna::Image perlin_terrain_image;
  etna::Image normal_terrain_image;
  etna::Sampler defaultSampler;

  etna::Buffer constants;
  etna::Buffer instanceMatricesBuffer;
  etna::DescriptorSet instanceMatricesDescriptorSet;

  glm::vec3 camView;
  glm::mat4x4 worldViewProj;
  glm::mat4x4 lightMatrix;

  etna::GraphicsPipeline staticMeshPipeline{};
  etna::GraphicsPipeline terrainPipeline{};
  etna::ComputePipeline perlinPipeline{};
  etna::ComputePipeline normalPipeline{};

  etna::Sampler perlinSampler;

  glm::uvec2 resolution;

  std::vector<InstanceGroup> instanceGroups;
  std::vector<glm::mat4x4> instanceMatrices;

  struct TerrainConsts
  {
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::vec4 camView;
    glm::vec2 textureSize;
    std::int32_t enableTessellation = 1;
    float minTessLevel = 1.0f;    // Добавить
    float maxTessLevel = 32.0f;   // Добавить
    float minDistance = 10.0f;    // Добавить
    float maxDistance = 200.0f;   // Добавить
  };

  void* persistentMapping = nullptr;
  bool enableFrustumCulling = true;
  bool enableTessellation = true;
  std::uint32_t maxInstances = 0;

  static constexpr std::uint32_t TERRAIN_TEXTURE_SIZE_WIDTH  = 4096;
  static constexpr std::uint32_t TERRAIN_TEXTURE_SIZE_HEIGHT = 4096;
  static constexpr std::uint32_t COMPUTE_WORKGROUP_SIZE = 32;
  static constexpr std::uint32_t PATCH_SUBDIVISION = 8;
  static constexpr std::uint32_t GROUP_COUNT_X =
   (TERRAIN_TEXTURE_SIZE_WIDTH + COMPUTE_WORKGROUP_SIZE - 1) / COMPUTE_WORKGROUP_SIZE;
  static constexpr std::uint32_t GROUP_COUNT_Y =
   (TERRAIN_TEXTURE_SIZE_HEIGHT + COMPUTE_WORKGROUP_SIZE - 1) / COMPUTE_WORKGROUP_SIZE;
};
