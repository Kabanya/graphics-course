#include "Emitter.hpp"
#include <algorithm>


void Emitter::update(float dt, uint32_t max_particles, glm::vec3 wind)
{
  timeSinceLastSpawn += dt;
  float spawnInterval = 1.0f / spawnFrequency;
  while (timeSinceLastSpawn >= spawnInterval && particles.size() < max_particles)
  {
    spawnParticle();
    timeSinceLastSpawn -= spawnInterval;
  }

  particles.erase(
  std::remove_if(particles.begin(), particles.end(),
    [dt, this, wind](Particle& p) {
      auto pos = p.position;
      auto vel = p.velocity;
      pos += vel * dt;
      vel += (gravity + wind) * dt - drag * vel * dt;
      p.position = pos;
      p.velocity = vel;
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

void Emitter::clearParticles()
{
  particles.clear();
}