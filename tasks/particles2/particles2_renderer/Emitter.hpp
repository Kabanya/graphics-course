#pragma once

#include "Particle.hpp"

#include <vector>
#include <glm/glm.hpp>
#include <etna/Buffer.hpp>
#include <etna/ComputePipeline.hpp>

class Emitter
{
public:
  glm::vec3 position;
  glm::vec3 initialVelocity;
  glm::vec3 gravity;
  float drag;
  float spawnFrequency;
  float particleLifetime;
  float size;
  float timeSinceLastSpawn = 0.0f;

  std::vector<Particle> particles;

  etna::Buffer particleSSBO;
  etna::Buffer particleUBO;
  etna::Buffer particleCountBuffer;
  etna::Buffer spawnUBO;
  etna::Buffer emitterSSBO;

  void* particleSSBOMapping = nullptr;
  void* particleCountMapping = nullptr;
  void* emitterSSBOMapping = nullptr;

  std::uint32_t currentParticleCount = 0;
  std::uint32_t maxParticles = 500'000;

  void update(float dt, glm::vec3 wind,
              const etna::ComputePipeline& spawn_pipeline,
              const etna::ComputePipeline& calculate_pipeline,
              const etna::ComputePipeline& integrate_pipeline);
  void spawnParticle();
  void clearParticles();
  void allocateGPUResources();

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
  SpawnUBO spawnData;

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