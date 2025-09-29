#pragma once

#include "Emitter.hpp"

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

class ParticleSystem
{
public:
  ParticleSystem()  = default;
  ~ParticleSystem() = default;

  void update(float dt, glm::vec3 wind);
  void render(vk::CommandBuffer cmd_buf, etna::Buffer& buffer, uint32_t particle_count);
  void addEmitter(Emitter emitter);
  void removeEmitter(size_t index);

  etna::Buffer particleBuffer;

  glm::vec3 wind = {0.0f, 0.0f, 0.0f};

  std::vector<std::unique_ptr<Emitter>> emitters;

  std::uint32_t max_particlesPerEmitter = 2500;

  std::vector<std::unique_ptr<Emitter>> pendingDestruction;
  std::int32_t destructionDelay = 0;
};