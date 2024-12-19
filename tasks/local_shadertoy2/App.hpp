#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/DescriptorSet.hpp>

#include "wsi/OsWindowingManager.hpp"

#include <etna/GraphicsPipeline.hpp>
#include <etna/Sampler.hpp>


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
  etna::ComputePipeline pipelineToy;
  etna::GraphicsPipeline graphicsPipeline;
  etna::Image TextureComputed;
  etna::Image TextureImage;
  etna::Sampler sampler;
  etna::DescriptorSet descriptorSet;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
};
