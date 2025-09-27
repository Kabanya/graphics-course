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
      auto pos = p.getPosition();
      auto vel = p.getVelocity();
      pos += vel * dt;
      vel += (gravity + wind) * dt - drag * vel * dt;
      p.setPosition(pos);
      p.setVelocity(vel);
      p.setRemainingLifetime(p.getRemainingLifetime() - dt);
      return p.getRemainingLifetime() <= 0.0f;
    }),
particles.end()
  );
}

void Emitter::spawnParticle()
{
  Particle p;
  p.setPosition(position);
  p.setVelocity(initialVelocity);
  p.setRemainingLifetime(particleLifetime);
  p.setSize(size);
  particles.push_back(p);
}

void Emitter::clearParticles()
{
  particles.clear();
}