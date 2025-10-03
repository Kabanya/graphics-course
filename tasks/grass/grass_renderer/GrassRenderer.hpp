#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

// #include "shaders/UniformParams.h"

struct Blade {
  glm::vec3 pos;
  float height;
};

struct GrassParams {
  glm::vec3 eyePos;
  float terrainSize;
  float grassHeight;
  float grassDensity;
  float grassRadius;
};

class GrassRenderer
{
public:
  explicit GrassRenderer();

  void allocateResources(
    etna::Buffer&      in_constants,
    etna::Buffer&      in_uniform_params_buffer,
    etna::Sampler&     in_default_sampler,
    const etna::Image& in_height_map,
    float              in_terrain_size);
  void loadShaders();
  void setupPipelines(vk::Format swapchain_format);
  void update(const glm::vec3& in_camera_pos);
  void render(vk::CommandBuffer cmd_buf);

  void generateGrass  (vk::CommandBuffer cmd_buf);
  void setGrassDensity(int density);
  void setGrassHeight (float height);
  void setGrassRadius (float radius);

  float    getGrassHeight () const { return grassHeight;  }
  int      getGrassDensity() const { return grassDensity; }
  float    getGrassRadius () const { return grassRadius;  }
  uint32_t getBladeCount  () const { return bladeCount;   }

private:

  // Buffers
  etna::Buffer bladesBuffer;
  etna::Buffer grassParamsBuffer;
  void* grassParamsMapping = nullptr;

  // Pipelines
  etna::ComputePipeline grassGenPipeline;
  etna::GraphicsPipeline grassRenderPipeline;

  etna::Buffer*  constants = nullptr;
  etna::Buffer*  uniform_params_buffer = nullptr;
  etna::Sampler* default_sampler = nullptr;
  const etna::Image* height_map = nullptr;

  // Parameters
  int grassDensity   = 100; // plotnost'
  float grassHeight  = 5.0f;
  float grassRadius  = 100.0f;
  float terrain_size = 1024.0f;

  glm::vec3 camera_pos;
  std::uint32_t bladeCount = 100;

  // Compute workgroup sizes
  std::uint32_t groupCountX = 4;
  std::uint32_t groupCountY = 4;
  std::uint32_t MAX_BLADES = 10'000'000; // TODO: make dynamic
};