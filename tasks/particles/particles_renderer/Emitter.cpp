#include "Emitter.hpp"
#include "ParticleSystem.hpp"

void Emitter::update(float dt)
{
    timeSinceLastSpawn += dt;
    float spawnInterval = 1.0f / spawnFrequency;
    while (timeSinceLastSpawn >= spawnInterval && particles.size() < ParticleSystem::N_PARTICLES_PER_EMITTER)
    {
        spawnParticle();
        timeSinceLastSpawn -= spawnInterval;
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [dt, this](Particle& p) {
                p.position += p.velocity * dt;
                p.velocity += gravity * dt;
                p.remainingLifetime -= dt;
                return p.remainingLifetime <= 0.0f;
            }),
        particles.end()
    );
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