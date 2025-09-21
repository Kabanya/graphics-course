#pragma once

#include "Emitter.hpp"

#include <vector>
#include <etna/Buffer.hpp>
#include <vulkan/vulkan.hpp>

class ParticleSystem
{
public:
    std::vector<Emitter> emitters;
    etna::Buffer particleBuffer;

    void update(float dt);
    void render(vk::CommandBuffer cmd_buf, void* particle_mapping);
    void addEmitter(const Emitter& emitter);
};