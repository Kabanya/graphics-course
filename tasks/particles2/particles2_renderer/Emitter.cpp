#include "Emitter.hpp"

#include <etna/GlobalContext.hpp>
#include "etna/Etna.hpp"
#include "etna/OneShotCmdMgr.hpp"
#include "etna/Profiling.hpp"
#include "shaders/UniformParams.h"

// [[deprecated("Use shader for spawning particles instead")]]
// void Emitter::spawnParticle()
// {
//   printf("Use shader for spawning particles instead");
//   // ParticleGPU p;
//   // p.pos = glm::vec4(position, size);
//   // p.vel = glm::vec4(initialVelocity, particleLifetime);
//   // particles.push_back(p);
// }

void Emitter::clearParticles()
{
  std::uint32_t zero = 0;
  memcpy(particleCountMapping, &zero, sizeof(std::uint32_t));
  currentParticleCount = 0;
  VkDrawIndirectCommand resetDraw = {0, 1, 0, 0};
  memcpy(indirectDrawBuffer.map(), &resetDraw, sizeof(VkDrawIndirectCommand));
  indirectDrawBuffer.unmap();
}

void Emitter::allocateGPUResources()
{
  auto& ctx = etna::get_context();

  particleSSBO = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = maxParticlesPerEmitter * sizeof(ParticleGPU),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "emitter_particle_ssbo",
  });
  particleSSBOMapping = particleSSBO.map();

  emitterSSBO = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(EmitterGPU),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "emitter_ssbo",
  });
  emitterSSBOMapping = emitterSSBO.map();

  particleUBO = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(ParticleUBO),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "emitter_particle_ubo",
  });

  particleCountBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(uint32_t) * 2,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "emitter_particle_count",
  });
  particleCountMapping = particleCountBuffer.map();

  indirectDrawBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(VkDrawIndirectCommand),
    .bufferUsage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "indirect_draw_buffer",
  });
  VkDrawIndirectCommand initialDraw = {0, 1, 0, 0};
  memcpy(indirectDrawBuffer.map(), &initialDraw, sizeof(VkDrawIndirectCommand));
  indirectDrawBuffer.unmap();

  spawnUBO = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(SpawnUBO),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "emitter_spawn_ubo",
  });
}

void Emitter::update(float dt, glm::vec3 wind,
                     const etna::ComputePipeline& spawn_pipeline,
                     const etna::ComputePipeline& calculate_pipeline,
                     const etna::ComputePipeline& integrate_pipeline)
{
  // timeSinceLastSpawn += dt;

  auto& ctx = etna::get_context();
  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));

  // Spawn particles on GPU
  if (spawnFrequency > 0.0f)
  {
    EmitterGPU gpuEmitter;
    gpuEmitter.position               = position;
    gpuEmitter.timeSinceLastSpawn     = timeSinceLastSpawn;
    gpuEmitter.initialVelocity        = initialVelocity;
    gpuEmitter.spawnFrequency         = spawnFrequency;
    gpuEmitter.particleLifetime       = particleLifetime;
    gpuEmitter.size                   = size;
    gpuEmitter.maxParticlesPerEmitter = maxParticlesPerEmitter;
    gpuEmitter.currentParticles       = 0;

    // Write emitter datta
    memcpy(emitterSSBOMapping, &gpuEmitter, sizeof(EmitterGPU));

    std::uint32_t countData[2] = {currentParticleCount, maxParticlesPerEmitter};
    memcpy(particleCountMapping, countData, sizeof(countData));

    spawnData.deltaTime = dt;
    spawnData.emitterCount = 1; // Single emitter
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
        etna::Binding{4, indirectDrawBuffer.genBinding()},
      });

    cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, spawn_pipeline.getVkPipeline());
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, spawn_pipeline.getVkPipelineLayout(), 0, {descSetSpawn.getVkSet()}, {});
    cmdBuf.dispatch(1, 1, 1); // Single emitter
    etna::flush_barriers(cmdBuf);
  }

  // Calculate and integrate particles
  if (currentParticleCount > 0)
  {
    particleUbo.deltaT = dt;
    particleUbo.particleCount = currentParticleCount;
    particleUbo.gravity = gravity;
    particleUbo.wind = wind;
    particleUbo.drag = drag;

    memcpy(particleUBO.map(), &particleUbo, sizeof(ParticleUBO));
    particleUBO.unmap();

    ETNA_PROFILE_GPU(cmdBuf, particleUbo);

    // Calculate pass
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
    cmdBuf.dispatch((currentParticleCount + 31) / 32, 1, 1);
    etna::flush_barriers(cmdBuf);

    // Integrate pass
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
    cmdBuf.dispatch((currentParticleCount + 31) / 32, 1, 1);
  }

  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdManager->submitAndWait(cmdBuf);

  // Readback only the total particle slots allocated by spawn shader
  // We use this as the upper bound for processing/sorting
  memcpy(&currentParticleCount, particleCountMapping, sizeof(std::uint32_t));
  if (currentParticleCount > maxParticlesPerEmitter)
    currentParticleCount = maxParticlesPerEmitter;

  // Readback emitter state for time tracking
  if (spawnFrequency > 0.0f)
  {
    EmitterGPU* readBackEmitter = static_cast<EmitterGPU*>(emitterSSBOMapping);
    timeSinceLastSpawn = readBackEmitter->timeSinceLastSpawn;
  }
  // particles.clear();
}