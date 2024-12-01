#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/DescriptorSet.hpp>
#include <etna/Sampler.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include "wsi/OsWindowingManager.hpp"


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  etna::GlobalContext* context;
  // etna::ComputePipeline pipelineToy;
  etna::GraphicsPipeline graphicsPipeline;
  etna::GraphicsPipeline pipelineManager{};
  
  etna::Image ImageToy2;
  etna::DescriptorSet descriptorSet;


  etna::DescriptorSet vkSet;
  etna::DescriptorSet set;
  
  etna::Sampler defaultSampler;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
};
