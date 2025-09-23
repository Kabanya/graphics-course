#pragma once

#include "Particle.hpp"

#include <vector>
#include <glm/glm.hpp>

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

    void update(float dt, std::uint32_t max_particles, glm::vec3 wind);
    void spawnParticle();
    void clearParticles();
};