#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/DescriptorSet.hpp>
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

private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image mainViewDepth;
  etna::Buffer constants;
  etna::Buffer instanceMatricesBuffer;
  etna::DescriptorSet instanceMatricesDescriptorSet;

  struct PushConstants
  {
    glm::mat4x4 projView;
  } pushConst;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 lightMatrix;

  etna::GraphicsPipeline staticMeshPipeline{};

  glm::uvec2 resolution;

  std::vector<InstanceGroup> instanceGroups;
  std::vector<glm::mat4x4> instanceMatrices;

  void* persistentMapping = nullptr;
};