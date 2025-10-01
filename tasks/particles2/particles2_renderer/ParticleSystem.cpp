#include "ParticleSystem.hpp"

#include <algorithm>
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
}

void ParticleSystem::update(float dt, const glm::vec3 wind_value)
{
  for (auto& emitter : emitters)
  {
    emitter.update(dt, wind_value, particleSpawnPipeline, particleCalculatePipeline, particleIntegratePipeline);
  }
}

void ParticleSystem::render(vk::CommandBuffer cmd_buf, glm::vec3 cam_pos)
{
  std::sort(emitters.begin(), emitters.end(), [cam_pos](const Emitter& a, const Emitter& b) {
      return glm::distance(a.position, cam_pos) > glm::distance(b.position, cam_pos);
  });

  for (auto& emitter : emitters)
  {
    if (emitter.currentParticleCount == 0)
      continue;

    cmd_buf.bindVertexBuffers(0, {emitter.particleSSBO.get()}, {0});
    cmd_buf.drawIndirect(emitter.indirectDrawBuffer.get(), 0, 1, 0);
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