#pragma once

#include "Emitter.hpp"

#include <vector>
#include <etna/Buffer.hpp>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

class ParticleSystem
{
public:
    ParticleSystem()  = default;
    ~ParticleSystem() = default;

    void update(float dt, glm::vec3 wind);
    void render(vk::CommandBuffer cmd_buf, glm::vec3 cam_pos);
    void addEmitter(const Emitter& emitter);
    void removeEmitter(size_t index);

    const std::vector<Emitter>& getEmitters() const { return emitters; }
    const etna::Buffer& getParticleBuffer()   const { return particleBuffer; }

    glm::vec3 wind = {0.0f, 0.0f, 0.0f};

    std::vector<Emitter> emitters;
    etna::Buffer particleBuffer;

    uint32_t max_particlesPerEmitter = 2500;

    static constexpr std::size_t MAX_PARTICLES = 500'000;
};