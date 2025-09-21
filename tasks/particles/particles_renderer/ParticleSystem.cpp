#include "ParticleSystem.hpp"

#include <vector>

void ParticleSystem::update(float dt)
{
    for (auto& emitter : emitters)
    {
        emitter.update(dt);
    }
}

void ParticleSystem::render(vk::CommandBuffer cmd_buf, void* particle_mapping)
{
    std::vector<glm::vec3> positions;
    for (const auto& emitter : emitters)
    {
        for (const auto& particle : emitter.particles)
        {
            positions.push_back(particle.position);
        }
    }

    if (positions.empty())
        return;

    std::memcpy(particle_mapping, positions.data(), positions.size() * sizeof(glm::vec3));

    cmd_buf.bindVertexBuffers(0, {particleBuffer.get()}, {0});
    cmd_buf.draw(static_cast<uint32_t>(positions.size()), 1, 0, 0);
}

void ParticleSystem::addEmitter(const Emitter& emitter)
{
    emitters.push_back(emitter);
}