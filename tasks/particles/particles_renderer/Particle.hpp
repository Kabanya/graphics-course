#pragma once

#include <glm/glm.hpp>

struct Particle32T
{
  glm::vec3 position;
  glm::vec3 velocity;
  float remainingLifetime;
  float size;

  glm::vec3 getPosition() const { return position; }
  void setPosition(const glm::vec3& pos) { position = pos; }
  glm::vec3 getVelocity() const { return velocity; }
  void  setVelocity(const glm::vec3& vel) { velocity = vel; }
  float getRemainingLifetime() const { return remainingLifetime; }
  void  setRemainingLifetime(float lt) { remainingLifetime = lt; }
  float getSize() const { return size; }
  void  setSize(float s) { size = s; }
};

struct [[gnu::packed]] Particle24T
{
  float position[3];
  std::uint16_t velocityX;
  std::uint16_t velocityY;
  std::uint16_t velocityZ;
  std::uint16_t remainingLifetime;
  std::uint16_t size;

  std::uint8_t padding[2];

  glm::vec3 getPosition() const { return glm::vec3(position[0], position[1], position[2]); }
  void setPosition(const glm::vec3& pos) { position[0] = pos.x; position[1] = pos.y; position[2] = pos.z; }

  void setVelocity(const glm::vec3& vel)
  {
    velocityX = static_cast<std::uint16_t>((vel.x + 50.0f) * 655.35f);
    velocityY = static_cast<std::uint16_t>((vel.y + 50.0f) * 655.35f);
    velocityZ = static_cast<std::uint16_t>((vel.z + 50.0f) * 655.35f);
  }

  glm::vec3 getVelocity() const
  {
    return glm::vec3(
      (velocityX / 655.35f) - 50.0f,
      (velocityY / 655.35f) - 50.0f,
      (velocityZ / 655.35f) - 50.0f
    );
  }

  float getRemainingLifetime() const { return remainingLifetime / 655.35f; }
  void  setRemainingLifetime(float lt) { remainingLifetime = static_cast<std::uint16_t>(lt * 655.35f); }
  float getSize() const { return size / 655.35f; }
  void  setSize(float s) { size = static_cast<std::uint16_t>(s * 655.35f); }
};

typedef Particle32T Particle;