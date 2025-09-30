#include "ParticleSystem.hpp"

#include <algorithm>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <etna/RenderTargetStates.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include <vector>

#include "etna/Etna.hpp"
#include "etna/OneShotCmdMgr.hpp"


void ParticleSystem::render(vk::CommandBuffer cmd_buf, etna::Buffer& buffer, uint32_t particle_count)
{
  if (particle_count > 0) {
    cmd_buf.bindVertexBuffers(0, {buffer.get()}, {0});
    cmd_buf.draw(particle_count, 1, 0, 0);
  }
}

void ParticleSystem::addEmitter(std::unique_ptr<Emitter> emitter)
{
  emitters.push_back(std::move(emitter));
}

void ParticleSystem::removeEmitter(const size_t index)
{
  if (index < emitters.size())
  {
    pendingDestruction.push_back(std::move(emitters[index]));
    emitters.erase(emitters.begin() + static_cast<std::int64_t>(index));
  }
}

void ParticleSystem::update(float delta_time, glm::vec3 wind,
                            const etna::ComputePipeline& spawn_pipeline,
                            const etna::ComputePipeline& calculate_pipeline,
                            const etna::ComputePipeline& integrate_pipeline)
{
  auto& ctx = etna::get_context();
  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));

  uint32_t particleCount = currentParticleCount;

  // Spawn particles
  if (!emitters.empty())
  {
    std::vector<EmitterGPU> gpuEmitters;
    for (auto& emitter : emitters)
    {
      EmitterGPU ge;
      ge.position = emitter->position;
      ge.timeSinceLastSpawn = emitter->timeSinceLastSpawn;
      ge.initialVelocity = emitter->initialVelocity;
      ge.spawnFrequency = emitter->spawnFrequency;
      ge.particleLifetime = emitter->particleLifetime;
      ge.size = emitter->size;
      ge.maxParticles = emitter->maxParticles;
      ge.currentParticles = 0;
      gpuEmitters.push_back(ge);
    }

    memcpy(emitterSSBOMapping, gpuEmitters.data(), gpuEmitters.size() * sizeof(EmitterGPU));

    uint32_t countData[2] = {particleCount, maxParticles};
    memcpy(particleCountMapping, countData, sizeof(countData));

    SpawnUBO spawnData;
    spawnData.deltaTime = delta_time;
    spawnData.emitterCount = static_cast<uint32_t>(gpuEmitters.size());
    memcpy(spawnUBO.map(), &spawnData, sizeof(SpawnUBO));
    spawnUBO.unmap();

    auto spawnInfo = etna::get_shader_program("particle_spawn");
    auto descSetSpawn = etna::create_descriptor_set(
      spawnInfo.getDescriptorLayoutId(0),
      cmdBuf,
      {
        etna::Binding{0, particleSSBO.genBinding()},
        etna::Binding{1, emitterSSBO.genBinding()},
        etna::Binding{2, particleCountBuffer.genBinding()},
        etna::Binding{3, spawnUBO.genBinding()},
      });
    cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, spawn_pipeline.getVkPipeline());
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, spawn_pipeline.getVkPipelineLayout(), 0, {descSetSpawn.getVkSet()}, {});
    cmdBuf.dispatch((spawnData.emitterCount + 31) / 32, 1, 1);

    vk::BufferMemoryBarrier2 spawnBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
      .buffer = particleSSBO.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE
    };
    cmdBuf.pipelineBarrier2(vk::DependencyInfo{
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &spawnBarrier
    });
  }

  // Calculate and integrate particles
  ParticleUBO particleUbo;
  particleUbo.deltaT = delta_time;
  particleUbo.particleCount = particleCount;
  particleUbo.gravity = glm::vec3(0.0f, 0.0f, 0.0f);
  particleUbo.wind = wind;
  particleUbo.drag = 0.0f;

  if (particleCount > 0 || !emitters.empty())
  {
    memcpy(particleUBO.map(), &particleUbo, sizeof(ParticleUBO));
    particleUBO.unmap();

    ETNA_PROFILE_GPU(cmdBuf, particleGPUUpdate);

    auto calculateInfo = etna::get_shader_program("particle_calculate");
    auto descSetCalculate = etna::create_descriptor_set(
      calculateInfo.getDescriptorLayoutId(0),
      cmdBuf,
      {
        etna::Binding{0, particleSSBO.genBinding()},
        etna::Binding{1, particleUBO.genBinding()},
      });
    cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, calculate_pipeline.getVkPipeline());
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, calculate_pipeline.getVkPipelineLayout(), 0, {descSetCalculate.getVkSet()}, {});
    cmdBuf.dispatch((std::max(particleCount, 1u) + 31) / 32, 1, 1);

    vk::BufferMemoryBarrier2 calcBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
      .buffer = particleSSBO.get(),
      .offset = 0,
      .size = VK_WHOLE_SIZE
    };
    cmdBuf.pipelineBarrier2(vk::DependencyInfo{
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &calcBarrier
    });

    auto integrateInfo = etna::get_shader_program("particle_integrate");
    auto descSetIntegrate = etna::create_descriptor_set(
      integrateInfo.getDescriptorLayoutId(0),
      cmdBuf,
      {
        etna::Binding{0, particleSSBO.genBinding()},
        etna::Binding{1, particleUBO.genBinding()},
      });
    cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, integrate_pipeline.getVkPipeline());
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, integrate_pipeline.getVkPipelineLayout(), 0, {descSetIntegrate.getVkSet()}, {});
    cmdBuf.dispatch((std::max(particleCount, 1u) + 31) / 32, 1, 1);
  }

  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdManager->submitAndWait(cmdBuf);

  // Readback particle count and emitter states
  if (!emitters.empty())
  {
    memcpy(&particleCount, particleCountMapping, sizeof(uint32_t));
    if (particleCount > maxParticles)
      particleCount = maxParticles;

    EmitterGPU* readBackEmitters = static_cast<EmitterGPU*>(emitterSSBOMapping);
    for (size_t i = 0; i < emitters.size(); ++i)
    {
      emitters[i]->timeSinceLastSpawn = readBackEmitters[i].timeSinceLastSpawn;
    }
  }

  currentParticleCount = particleCount;
}