#include "Emitter.hpp"

#include <etna/GlobalContext.hpp>
#include "etna/Etna.hpp"
#include "etna/OneShotCmdMgr.hpp"
#include "etna/Profiling.hpp"
// #include "shaders/UniformParams.h"

// void Emitter::update(float dt, const std::uint32_t max_particles, glm::vec3 wind)
// {
//   timeSinceLastSpawn += dt;
//   float spawnInterval = 1.0f / spawnFrequency;
//   while (timeSinceLastSpawn >= spawnInterval && particles.size() < max_particles)
//   {
//     spawnParticle();
//     timeSinceLastSpawn -= spawnInterval;
//   }

//   std::erase_if(
//     particles,
//     [dt, this, wind](Particle& p)
//     {
//       auto pos = p.position;
//       auto vel = p.velocity;
//       pos += vel * dt;
//       vel += (gravity + wind) * dt - drag * vel * dt;
//       p.position = pos;
//       p.velocity = vel;
//       p.remainingLifetime -= dt;
//       return p.remainingLifetime <= 0.0f;
//     });
// }

void Emitter::spawnParticle()
{
  Particle p;
  p.position = position;
  p.velocity = initialVelocity;
  p.remainingLifetime = particleLifetime;
  p.size = size;
  particles.push_back(p);
}

void Emitter::clearParticles()
{
  particles.clear();
}

void Emitter::allocateGPUResources()
{
  auto& ctx = etna::get_context();

  particleSSBO = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = maxParticles * sizeof(Particle),
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
  // CPU update for spawn timing
  timeSinceLastSpawn += dt;

  auto& ctx = etna::get_context();
  auto cmdManager = ctx.createOneShotCmdMgr();
  auto cmdBuf = cmdManager->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));

  // Spawn particles on GPU
  if (spawnFrequency > 0.0f)
  {
    struct EmitterGPU {
      glm::vec3 position;
      float timeSinceLastSpawn;
      glm::vec3 initialVelocity;
      float spawnFrequency;
      float particleLifetime;
      float size;
      uint32_t maxParticles;
      uint32_t currentParticles;
    };

    EmitterGPU gpuEmitter;
    gpuEmitter.position = position;
    gpuEmitter.timeSinceLastSpawn = timeSinceLastSpawn;
    gpuEmitter.initialVelocity = initialVelocity;
    gpuEmitter.spawnFrequency = spawnFrequency;
    gpuEmitter.particleLifetime = particleLifetime;
    gpuEmitter.size = size;
    gpuEmitter.maxParticles = maxParticles;
    gpuEmitter.currentParticles = 0;

    // Write emitter data (assuming single emitter buffer)
    memcpy(emitterSSBOMapping, &gpuEmitter, sizeof(EmitterGPU));

    uint32_t countData[2] = {currentParticleCount, maxParticles};
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
      });

    cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, spawn_pipeline.getVkPipeline());
    cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, spawn_pipeline.getVkPipelineLayout(), 0, {descSetSpawn.getVkSet()}, {});
    cmdBuf.dispatch(1, 1, 1); // Single emitter

    // vk::BufferMemoryBarrier2 spawnBarrier{
    //   .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
    //   .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
    //   .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
    //   .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
    //   .buffer = particleSSBO.get(),
    //   .offset = 0,
    //   .size = VK_WHOLE_SIZE
    // };
    // cmdBuf.pipelineBarrier2(vk::DependencyInfo{
    //   .bufferMemoryBarrierCount = 1,
    //   .pBufferMemoryBarriers = &spawnBarrier
    // });
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

    // vk::BufferMemoryBarrier2 calcBarrier{
    //   .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
    //   .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
    //   .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
    //   .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
    //   .buffer = particleSSBO.get(),
    //   .offset = 0,
    //   .size = VK_WHOLE_SIZE
    // };
    // cmdBuf.pipelineBarrier2(vk::DependencyInfo{
    //   .bufferMemoryBarrierCount = 1,
    //   .pBufferMemoryBarriers = &calcBarrier
    // });
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

  // Readback particle count and emitter state
  memcpy(&currentParticleCount, particleCountMapping, sizeof(uint32_t));
  if (currentParticleCount > maxParticles)
    currentParticleCount = maxParticles;

  // Update timeSinceLastSpawn from GPU
  if (spawnFrequency > 0.0f)
  {
    EmitterGPU* readBackEmitter = static_cast<EmitterGPU*>(emitterSSBOMapping);
    timeSinceLastSpawn = readBackEmitter->timeSinceLastSpawn;
  }

  // Clear CPU particles vector as we're using GPU buffers now
  particles.clear();
}