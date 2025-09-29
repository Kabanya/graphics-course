#include "ParticleSystem.hpp"

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

void ParticleSystem::render(vk::CommandBuffer cmd_buf, etna::Buffer& buffer, uint32_t particle_count)
{
  if (particle_count > 0) {
    cmd_buf.bindVertexBuffers(0, {buffer.get()}, {0});
    cmd_buf.draw(particle_count, 1, 0, 0);
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