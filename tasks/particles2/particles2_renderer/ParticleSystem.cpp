#include "ParticleSystem.hpp"
#include "etna/Etna.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <etna/RenderTargetStates.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include <vector>

void ParticleSystem::allocateResources(){/*empty*/}

void ParticleSystem::setupPipelines()
{
  auto& ctx = etna::get_context();
  auto& pipelineManager = ctx.getPipelineManager();
  particleCalculatePipeline = pipelineManager.createComputePipeline("particle_calculate", {});
  particleIntegratePipeline = pipelineManager.createComputePipeline("particle_integrate", {});
  particleSpawnPipeline     = pipelineManager.createComputePipeline("particle_spawn",     {});
  particleSortPipeline      = pipelineManager.createComputePipeline("particle_sort",      {});
}

void ParticleSystem::update(float dt, const glm::vec3 wind_value)
{
  for (auto& emitter : emitters)
  {
    emitter.update(dt, wind_value, particleSpawnPipeline, particleCalculatePipeline, particleIntegratePipeline);
  }
}

void ParticleSystem::sortAllEmitters(vk::CommandBuffer cmd_buf, glm::vec3 cam_pos)
{
  for (auto& emitter : emitters)
  {
    if (emitter.currentParticleCount == 0)
      continue;

    sortEmitterParticles(cmd_buf, emitter, cam_pos);
  }
}

void ParticleSystem::render(vk::CommandBuffer cmd_buf)
{
  for (auto& emitter : emitters)
  {
    if (emitter.currentParticleCount == 0)
      continue;

    cmd_buf.bindVertexBuffers(0, {emitter.particleSSBO.get()}, {0});
    cmd_buf.drawIndirect(emitter.indirectDrawBuffer.get(), 0, 1, 0);
  }
}

void ParticleSystem::sortEmitterParticles(
  vk::CommandBuffer cmd_buf,
  Emitter& emitter,
  glm::vec3 cam_pos)
{
  if (emitter.currentParticleCount <= 1)
    return;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, particleSortPipeline.getVkPipeline());

  auto shaderInfo = etna::get_shader_program("particle_sort");

  std::uint32_t paddedCount = 1;
  while (paddedCount < emitter.currentParticleCount)
    paddedCount <<= 1;

  struct PushConstants {
    glm::vec3     cameraPosition;
    std::uint32_t particleCount;
    std::uint32_t stage;
    std::uint32_t substage;
  } pushConstants;

  pushConstants.cameraPosition = cam_pos;
  pushConstants.particleCount = emitter.currentParticleCount;

  for (std::uint32_t stage = 2; stage <= paddedCount; stage <<= 1) {
    for (std::uint32_t substage = stage >> 1; substage > 0; substage >>= 1)
    {
      pushConstants.stage = substage;
      pushConstants.substage = stage;

      auto descSet = etna::create_descriptor_set(
        shaderInfo.getDescriptorLayoutId(0),
        cmd_buf,
        {
          etna::Binding{0, emitter.particleSSBO.genBinding()},
        });

      auto vkSet = descSet.getVkSet();

      cmd_buf.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        particleSortPipeline.getVkPipelineLayout(),
        0,
        1,
        &vkSet,
        0,
        nullptr);

      cmd_buf.pushConstants(
        particleSortPipeline.getVkPipelineLayout(),
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(PushConstants),
        &pushConstants);

      std::uint32_t workgroups = (paddedCount + 255) / 256;
      cmd_buf.dispatch(workgroups, 1, 1);
      // etna::flush_barriers(cmd_buf);
      vk::MemoryBarrier2 barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
      };
      vk::DependencyInfo depInfo{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier,
      };
      cmd_buf.pipelineBarrier2(&depInfo);
    }
  }
}

void ParticleSystem::addEmitter(Emitter&& emitter)
{
  emitters.push_back(std::move(emitter));
  emitters.back().allocateGPUResources();
}

void ParticleSystem::removeEmitter(size_t index)
{
  if (index < emitters.size())
  {
    emitters.erase(emitters.begin() + index);
  }
}

void ParticleSystem::clearAllEmitters()
{
  emitters.clear();
}
