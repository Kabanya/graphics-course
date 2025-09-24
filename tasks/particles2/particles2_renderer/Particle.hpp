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

struct alignas(16) Particle16T
{
  std::int16_t posX;
  std::int16_t posY;
  std::int16_t posZ;
  std::uint8_t size;
  std::uint8_t packedVelocity;
  std::uint16_t remainingLifetime;

  void setPosition(const glm::vec3& pos)
  {
    posX = static_cast<std::int16_t>(pos.x * 256.0f);
    posY = static_cast<std::int16_t>(pos.y * 256.0f);
    posZ = static_cast<std::int16_t>(pos.z * 256.0f);
  }

  glm::vec3 getPosition() const
  {
    return glm::vec3(
      posX / 256.0f,
      posY / 256.0f,
      posZ / 256.0f
    );
  }

  void setVelocity(const glm::vec3& vel)
  {
    std::uint8_t vx = static_cast<std::uint8_t>(glm::clamp((vel.x + 10.0f) / 20.0f * 7.0f, 0.0f, 7.0f)) & 0x7;
    std::uint8_t vy = static_cast<std::uint8_t>(glm::clamp((vel.y + 10.0f) / 20.0f * 7.0f, 0.0f, 7.0f)) & 0x7;
    std::uint8_t vz = static_cast<std::uint8_t>(glm::clamp((vel.z + 10.0f) / 20.0f * 3.0f, 0.0f, 3.0f)) & 0x3;
    packedVelocity = (vx << 5) | (vy << 2) | vz;
  }

  glm::vec3 getVelocity() const
  {
    float vx = ((packedVelocity >> 5) & 0x7) / 7.0f * 20.0f - 10.0f;
    float vy = ((packedVelocity >> 2) & 0x7) / 7.0f * 20.0f - 10.0f;
    float vz = (packedVelocity & 0x3) / 3.0f * 20.0f - 10.0f;
    return glm::vec3(vx, vy, vz);
  }

  float getRemainingLifetime() const { return remainingLifetime / 25.5f; }
  void  setRemainingLifetime(float lt) { remainingLifetime = static_cast<std::uint16_t>(glm::clamp(lt, 0.0f, 10.0f) * 25.5f); }
  float getSize() const { return size / 25.5f; }
  void  setSize(float s) { size = static_cast<std::uint8_t>(glm::clamp(s, 0.0f, 10.0f) * 25.5f); }
};

typedef Particle32T Particle;