#pragma once

#include "TerrainRenderer.hpp"
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

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
  float grassWidth;
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
    const etna::Image& in_wind_map,
    float              in_terrain_size);
  void loadShaders();
  void setupPipelines(vk::Format swapchain_format);
  void update(const glm::vec3& in_camera_pos);
  void render(vk::CommandBuffer cmd_buf);

  void generateGrass  (vk::CommandBuffer cmd_buf);
  void setGrassDensity(int density);
  void setGrassHeight (float height);
  void setGrassRadius (float radius);
  void setGrassWidth  (float width);

  float    getGrassHeight () const { return grassHeight;  }
  int      getGrassDensity() const { return grassDensity; }
  float    getGrassRadius () const { return grassRadius;  }
  float    getGrassWidth  () const { return grassWidth;   }
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
  const etna::Image* wind_map = nullptr;

  // Parameters
  int   grassDensity = 400;
  float grassHeight  = 5.0f;
  float grassRadius  = 100.0f;
  float grassWidth   = 0.1f;
  float terrain_size = TerrainRenderer::TERRAIN_GRID_SIZE;

  glm::vec3 camera_pos;
  std::uint32_t bladeCount = 100;

  // Compute workgroup sizes
  std::uint32_t groupCountX = 4;
  std::uint32_t groupCountY = 4;
  std::uint32_t MAX_BLADES = grassDensity * 10'000;
};