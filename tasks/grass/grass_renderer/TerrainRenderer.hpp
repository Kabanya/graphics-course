#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/RenderTargetStates.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "shaders/UniformParams.h"

class TerrainRenderer
{
public:
  explicit TerrainRenderer();

  void allocateResources(
    etna::Buffer& in_constants,
    etna::Buffer& in_uniform_params_buffer, 
    etna::Sampler& in_default_sampler);
  void loadShaders();
  void setupPipelines(vk::Format swapchain_format);
  void update(const PerlinParams& params);
  void render(vk::CommandBuffer cmd_buf);
  void regenerateTerrain();
  void createTerrainMap(vk::CommandBuffer cmd_buf);

  const etna::Image& getPerlinTerrainImage () const { return perlinTerrainImage;       }

  std::uint32_t getTerrainTextureSizeWidth () const { return terrainTextureSizeWidth;  }
  std::uint32_t getTerrainTextureSizeHeight() const { return terrainTextureSizeHeight; }
  std::uint32_t getComputeWorkgroupSize    () const { return computeWorkgroupSize;     }
  std::uint32_t getPatchSubdivision        () const { return patchSubdivision;         }
  std::uint32_t getGroupCountX             () const { return groupCountX;              }
  std::uint32_t getGroupCountY             () const { return groupCountY;              }

  void setTerrainTextureSizeWidth (std::uint32_t w);
  void setTerrainTextureSizeHeight(std::uint32_t h);
  void setComputeWorkgroupSize    (std::uint32_t s);
  void setPatchSubdivision        (std::uint32_t s);

private:
  // Terrain parameters
  std::uint32_t terrainTextureSizeWidth  = 4096;
  std::uint32_t terrainTextureSizeHeight = 4096;
  std::uint32_t computeWorkgroupSize     = 32;
  std::uint32_t patchSubdivision         = 8;
  std::uint32_t groupCountX;
  std::uint32_t groupCountY;

  // Images and textures
  etna::Image perlinTerrainImage;
  etna::Image normalMapTerrainImage;

  // Buffers
  etna::Buffer perlinValuesBuffer;
  void* perlinValuesMapping = nullptr;

  // Pipelines
  etna::ComputePipeline  perlinPipeline {};
  etna::ComputePipeline  normalPipeline {};
  etna::GraphicsPipeline terrainPipeline{};

  PerlinParams perlinParams{
    .octaves = 10u,
    .amplitude = 0.5f,
    .frequencyMultiplier = 2.0f,
    .scale = 8.0f,
  };

  // References to shared buffers
  etna::Buffer*  constants             = nullptr;
  etna::Buffer*  uniform_params_buffer = nullptr;
  etna::Sampler* default_sampler       = nullptr;
};