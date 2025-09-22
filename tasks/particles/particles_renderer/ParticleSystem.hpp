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

    void update(float dt);
    void render(vk::CommandBuffer cmd_buf, glm::vec3 cam_pos);
    void addEmitter(const Emitter& emitter);

    const std::vector<Emitter>& getEmitters() const { return emitters; }
    const etna::Buffer& getParticleBuffer()   const { return particleBuffer; }

    std::vector<Emitter> emitters;
    etna::Buffer particleBuffer;

    static constexpr std::size_t N_PARTICLES_PER_EMITTER = 2500;
    static constexpr std::size_t MAX_PARTICLES = 100'000;
};