#pragma once

#include "Particle.hpp"

#include <vector>
#include <glm/glm.hpp>

class Emitter
{
public:
    glm::vec3 position;
    float spawnFrequency;
    float particleLifetime;
    glm::vec3 initialVelocity;
    glm::vec3 gravity;
    float size;

    std::vector<Particle> particles;
    float timeSinceLastSpawn = 0.0f;

    void update(float dt);
    void spawnParticle();
};