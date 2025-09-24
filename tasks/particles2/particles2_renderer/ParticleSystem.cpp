#include "ParticleSystem.hpp"

#include <algorithm>
#include "shaders/UniformParams.h"
#include <cstdint>
#include <etna/GlobalContext.hpp>

void ParticleSystem::update(const float dt, const glm::vec3 wind_value)
{
  for (const auto& emitter : emitters)
  {
    emitter->update(dt, max_particlesPerEmitter, wind_value);
  }
  destructionDelay++;
  if (destructionDelay > 1) {
    (void)etna::get_context().getDevice().waitIdle();
    pendingDestruction.clear();
    destructionDelay = 0;
  }
}

void ParticleSystem::render(const vk::CommandBuffer cmd_buf, glm::vec3 cam_pos)
{
    std::ranges::sort(
      emitters, [cam_pos](const std::unique_ptr<Emitter>& a, const std::unique_ptr<Emitter>& b)
    {
      return glm::distance(a->position, cam_pos) > glm::distance(b->position, cam_pos);
    });

  for (auto& emitter : emitters)
  {
    if (emitter->particles.empty())
      continue;

    std::ranges::sort(emitter->particles, [cam_pos](const Particle& a, const Particle& b)
    {
      return glm::distance(a.position, cam_pos) > glm::distance(b.position, cam_pos);
    });

    void* mapping = emitter->particleBuffer.map();
    ParticleGPU* particleData = static_cast<ParticleGPU*>(mapping);
    const size_t num = std::min(emitter->particles.size(), static_cast<size_t>(emitter->maxParticles));
    for (size_t i = 0; i < num; ++i)
    {
      particleData[i].position = emitter->particles[i].position;
      particleData[i].size = emitter->particles[i].size;
      particleData[i].velocity = emitter->particles[i].velocity;
      particleData[i].lifetime = emitter->particles[i].remainingLifetime;
    }
    emitter->particleBuffer.unmap();
    emitter->aliveParticles = num;

    cmd_buf.bindVertexBuffers(0, {emitter->particleBuffer.get()}, {0});
    cmd_buf.draw(static_cast<std::uint32_t>(num), 1, 0, 0);
  }
}

void ParticleSystem::addEmitter(Emitter emitter)
{
  emitters.push_back(std::make_unique<Emitter>(std::move(emitter)));
}

void ParticleSystem::removeEmitter(const size_t index)
{
  if (index < emitters.size())
  {
    pendingDestruction.push_back(std::move(emitters[index]));
    emitters.erase(emitters.begin() + static_cast<std::int64_t>(index));
  }
}