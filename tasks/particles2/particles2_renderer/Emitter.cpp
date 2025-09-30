#include "Emitter.hpp"

#include <etna/GlobalContext.hpp>
#include "shaders/UniformParams.h"

void Emitter::update(float dt, const std::uint32_t max_particles, glm::vec3 wind)
{
  timeSinceLastSpawn += dt;
  float spawnInterval = 1.0f / spawnFrequency;
  while (timeSinceLastSpawn >= spawnInterval && particles.size() < max_particles)
  {
    spawnParticle();
    timeSinceLastSpawn -= spawnInterval;
  }

  std::erase_if(
    particles,
    [dt, this, wind](Particle& p)
    {
      auto pos = p.position;
      auto vel = p.velocity;
      pos += vel * dt;
      vel += (gravity + wind) * dt - drag * vel * dt;
      p.position = pos;
      p.velocity = vel;
      p.remainingLifetime -= dt;
      return p.remainingLifetime <= 0.0f;
    });
}

void Emitter::spawnParticle()
{
  Particle p;
  p.position = position;
  p.velocity = initialVelocity;
  p.remainingLifetime = particleLifetime;
  p.size = size;
  particles.push_back(p);
}

void Emitter::clearParticles()
{
  particles.clear();
}

void Emitter::createBuffer()
{
  etna::Buffer::CreateInfo bufferInfo{
    .size = maxParticles * sizeof(ParticleGPU),
    .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "emitter_particle_buffer",
  };
  particleBuffer = etna::get_context().createBuffer(bufferInfo);
}