#pragma once

#include <cstdint>
#include <etna/Buffer.hpp>
#include <etna/ComputePipeline.hpp>
#include <vector>
#include "Emitter.hpp"

class ParticleSystem
{
  public:
  ParticleSystem() = default;

  void allocateResources();
  void setupPipelines();
  // void render(vk::CommandBuffer cmd_buf, etna::Buffer& ssbo, uint32_t particle_count);
  void render(vk::CommandBuffer cmd_buf, glm::vec3 cam_pos);

  void update(float dt, const glm::vec3 wind_value);

  void addEmitter(Emitter&& emitter);
  void reallocateEmitterBuffer(std::size_t index);
  void updateMaxParticles(std::uint32_t new_max);
  void removeEmitter(std::size_t index);
  void clearAllEmitters();


public:
    std::vector<Emitter> emitters;
    const std::vector<Emitter>& getEmitters() const {return emitters;}

  etna::ComputePipeline particleCalculatePipeline{};
  etna::ComputePipeline particleIntegratePipeline{};
  etna::ComputePipeline particleSpawnPipeline{};

  // etna::Buffer particleSSBO;
  // etna::Buffer particleUBO;
  // etna::Buffer particleCountBuffer;
  // etna::Buffer emitterSSBO;
  // etna::Buffer spawnUBO;

  void* particleSSBOMapping = nullptr;
  void* emitterSSBOMapping = nullptr;
  void* particleCountMapping = nullptr;

  std::uint32_t const maxParticles = 5'000'000;
  std::uint32_t max_particlesPerEmitter = Emitter().maxParticles;
  // std::uint32_t currentParticleCount = 0;

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