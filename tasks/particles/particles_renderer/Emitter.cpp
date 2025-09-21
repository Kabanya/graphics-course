#include "Emitter.hpp"

void Emitter::update(float dt)
{
    timeSinceLastSpawn += dt;
    float spawnInterval = 1.0f / spawnFrequency;
    while (timeSinceLastSpawn >= spawnInterval)
    {
        spawnParticle();
        timeSinceLastSpawn -= spawnInterval;
    }

    for (auto it = particles.begin(); it != particles.end(); )
    {
        it->position += it->velocity * dt;
        it->remainingLifetime -= dt;
        if (it->remainingLifetime <= 0.0f)
        {
            it = particles.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Emitter::spawnParticle()
{
    Particle p;
    p.position = position;
    p.velocity = initialVelocity;
    p.remainingLifetime = particleLifetime;
    particles.push_back(p);
}