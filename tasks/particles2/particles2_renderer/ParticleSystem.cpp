#include "ParticleSystem.hpp"

#include <algorithm>

void ParticleSystem::update(float dt, glm::vec3 wind)
{
    for (auto& emitter : emitters)
    {
        emitter.update(dt, max_particlesPerEmitter, wind);
    }
}

void ParticleSystem::render(vk::CommandBuffer cmd_buf, glm::vec3 cam_pos)
{
    std::sort(emitters.begin(), emitters.end(), [cam_pos](const Emitter& a, const Emitter& b) {
        return glm::distance(a.position, cam_pos) > glm::distance(b.position, cam_pos);
    });

    void* mapping = particleBuffer.map();
    glm::vec4* particleData = static_cast<glm::vec4*>(mapping);

    size_t totalParticles = 0;
    for (auto& emitter : emitters)
    {
        if (emitter.particles.empty())
            continue;

        std::sort(emitter.particles.begin(), emitter.particles.end(), [cam_pos](const Particle& a, const Particle& b) {
            return glm::distance(a.getPosition(), cam_pos) > glm::distance(b.getPosition(), cam_pos);
        });

        for (const auto& particle : emitter.particles)
        {
            if (totalParticles >= MAX_PARTICLES)
                break;
            particleData[totalParticles] = glm::vec4(particle.getPosition(), particle.getSize());
            ++totalParticles;
        }
    }

    particleBuffer.unmap();

    if (totalParticles == 0)
        return;

    cmd_buf.bindVertexBuffers(0, {particleBuffer.get()}, {0});
    cmd_buf.draw(static_cast<uint32_t>(totalParticles), 1, 0, 0);
}

void ParticleSystem::addEmitter(const Emitter& emitter)
{
    emitters.push_back(emitter);
}

void ParticleSystem::removeEmitter(size_t index)
{
    if (index < emitters.size())
    {
        emitters.erase(emitters.begin() + index);
    }
}