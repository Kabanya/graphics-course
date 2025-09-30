#pragma once

#include <etna/Buffer.hpp>
#include <etna/ComputePipeline.hpp>
#include <vector>
#include <memory>
#include "Emitter.hpp"

class ParticleSystem
{
public:
  ParticleSystem() = default;

  void addEmitter(std::unique_ptr<Emitter> emitter);
  void removeEmitter(std::size_t index);
  void render(vk::CommandBuffer cmd_buf, etna::Buffer& ssbo, uint32_t particle_count);

  void update(float delta_time, glm::vec3 wind,
              const etna::ComputePipeline& spawn_pipeline,
              const etna::ComputePipeline& calculate_pipeline,
              const etna::ComputePipeline& integrate_pipeline);

  std::vector<std::unique_ptr<Emitter>> emitters;
  std::vector<std::unique_ptr<Emitter>> pendingDestruction;

  etna::Buffer particleSSBO;
  etna::Buffer particleUBO;
  etna::Buffer particleCountBuffer;
  etna::Buffer emitterSSBO;
  etna::Buffer spawnUBO;

  void* particleSSBOMapping = nullptr;
  void* emitterSSBOMapping = nullptr;
  void* particleCountMapping = nullptr;

  etna::Buffer particleBuffer;

  std::uint32_t const maxParticles = 5'000'000;
  std::uint32_t max_particlesPerEmitter = 10'000;
  std::uint32_t currentParticleCount = 0;

public:
  struct ParticleUBO {
    float deltaT;
    uint32_t particleCount;
    glm::vec3 gravity;
    glm::vec3 wind;
    float drag;
  };
  ParticleUBO particleUbo;

  struct SpawnUBO {
    float deltaTime;
    uint32_t emitterCount;
  };

  struct EmitterGPU {
    glm::vec3 position;
    float timeSinceLastSpawn;
    glm::vec3 initialVelocity;
    float spawnFrequency;
    float particleLifetime;
    float size;
    uint32_t maxParticles;
    uint32_t currentParticles;
  };
};