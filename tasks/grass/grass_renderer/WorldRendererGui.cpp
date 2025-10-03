#include "WorldRendererGui.hpp"

#include "WorldRenderer.hpp"

#include <imgui.h>
#include <etna/GlobalContext.hpp>
#include "shaders/UniformParams.h"

WorldRendererGui::WorldRendererGui(WorldRenderer& renderer)
  : renderer_(renderer)
{
}

void WorldRendererGui::drawGui()
{
  if (renderer_.showTabs)
  {
    if (ImGui::Begin("Renderer Settings", &renderer_.showTabs))
    {
      if (ImGui::BeginTabBar("SettingsTabs"))
      {
        if (ImGui::BeginTabItem("Performance"))
        {
          drawPerformanceTab();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Render"))
        {
          drawRenderTab();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Terrain"))
        {
          drawTerrainTab();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Grass"))
        {
          drawGrassTab();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Info"))
        {
          drawInfoTab();
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
    }
    ImGui::End();
  }
}

void WorldRendererGui::drawPerformanceTab() const
{
  ImGui::Text(
    "Application average %.3f ms/frame (%.1f FPS)",
    1000.0f / ImGui::GetIO().Framerate,
    ImGui::GetIO().Framerate);
  ImGui::Text("Rendered Instances: %u", renderer_.renderedInstances);
  ImGui::Text("Grass Blades: %u", renderer_.grassRenderer->getBladeCount());
}

void WorldRendererGui::drawRenderTab()
{
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Culling and Tessellation");
  ImGui::Checkbox   ("Enable Frustum Culling", &renderer_.enableFrustumCulling);
  ImGui::Checkbox   ("Enable Tessellation", &renderer_.enableTessellation);

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Rendering Options");
  ImGui::Checkbox   ("Enable Avocados Rendering", &renderer_.enableSceneRendering);
  ImGui::Checkbox   ("Enable Terrain Rendering", &renderer_.enableTerrainRendering);
  ImGui::Checkbox   ("Enable Grass Rendering", &renderer_.enableGrassRendering);
}

void WorldRendererGui::drawTerrainTab()
{
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Terrain Parameters");
  ImGui::Checkbox("Draw Debug Terrain Quad", &renderer_.drawDebugTerrainQuad);
  float color[3]{renderer_.uniformParams.baseColor.r, renderer_.uniformParams.baseColor.g, renderer_.uniformParams.baseColor.b};
  ImGui::ColorEdit3(
    "Terrain base color", color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
  renderer_.uniformParams.baseColor = {color[0], color[1], color[2]};

  float pos[3]{renderer_.uniformParams.lightPos.x, renderer_.uniformParams.lightPos.y, renderer_.uniformParams.lightPos.z};
  ImGui::SliderFloat3("Light source position", pos, -10.f, 10.f);
  renderer_.uniformParams.lightPos = {pos[0], pos[1], pos[2]};

  int width = static_cast<int>(renderer_.terrainRenderer->getTerrainTextureSizeWidth());
  if (ImGui::InputInt("Terrain Texture Width", &width))
    renderer_.terrainRenderer->setTerrainTextureSizeWidth(static_cast<std::uint32_t>(width));

  int height = static_cast<int>(renderer_.terrainRenderer->getTerrainTextureSizeHeight());
  if (ImGui::InputInt("Terrain Texture Height", &height))
    renderer_.terrainRenderer->setTerrainTextureSizeHeight(static_cast<std::uint32_t>(height));

  int workgroup = static_cast<int>(renderer_.terrainRenderer->getComputeWorkgroupSize());
  if (ImGui::InputInt("Compute Workgroup Size", &workgroup))
    renderer_.terrainRenderer->setComputeWorkgroupSize(static_cast<std::uint32_t>(workgroup));

  int patch = static_cast<int>(renderer_.terrainRenderer->getPatchSubdivision());
  if (ImGui::InputInt("Patch Subdivision", &patch))
    renderer_.terrainRenderer->setPatchSubdivision(static_cast<std::uint32_t>(patch));

  ImGui::Text("Group Count X: %u", renderer_.terrainRenderer->getGroupCountX());
  ImGui::Text("Group Count Y: %u", renderer_.terrainRenderer->getGroupCountY());
  ImGui::Separator();

  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Perlin Noise Parameters");
  PerlinParams params = renderer_.getPerlinParams();
  bool changed = false;
  changed |= ImGui::SliderInt  ("Octaves", reinterpret_cast<int*>(&params.octaves), 1, 20);
  changed |= ImGui::SliderFloat("Amplitude", &params.amplitude, 0.0f, 1.0f);
  changed |= ImGui::SliderFloat("Frequency Multiplier", &params.frequencyMultiplier, 1.0f, 4.0f);
  changed |= ImGui::SliderFloat("Scale", &params.scale, 1.0f, 20.0f);
  if (changed)
    renderer_.setPerlinParams(params);

  if (ImGui::Button("Regenerate Terrain"))
    renderer_.regenerateTerrain();
}

void WorldRendererGui::drawGrassTab()
{
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Grass Parameters");
  ImGui::Checkbox("Synchronize Density with Radius", &syncDensityRadius);
  float grassHeight = renderer_.grassRenderer->getGrassHeight();
  if (ImGui::SliderFloat("Grass Height", &grassHeight, 0.1f, 20.0f))
    renderer_.grassRenderer->setGrassHeight(grassHeight);
  float grassWidth = renderer_.grassRenderer->getGrassWidth();
  if (ImGui::SliderFloat("Grass Width", &grassWidth, 0.01f, 1.0f))
    renderer_.grassRenderer->setGrassWidth(grassWidth);
  int grassDensity = renderer_.grassRenderer->getGrassDensity();
  if (ImGui::InputInt("Grass Density", &grassDensity, 1, 250))
  {
    if (syncDensityRadius && oldGrassDensity > 0)
    {
      float ratio = sqrt(static_cast<float>(grassDensity) / oldGrassDensity);
      float newRadius = renderer_.grassRenderer->getGrassRadius() * ratio;
      renderer_.grassRenderer->setGrassRadius(newRadius);
      oldGrassRadius = newRadius;
    }
    renderer_.grassRenderer->setGrassDensity(grassDensity);
    oldGrassDensity = grassDensity;
  }
  float grassRadius = renderer_.grassRenderer->getGrassRadius();
  if (ImGui::SliderFloat("Grass Radius", &grassRadius, 10.0f, 1000.0f))
  {
    if (syncDensityRadius && oldGrassRadius > 0.0f)
    {
      float ratio = grassRadius / oldGrassRadius;
      int newDensity = static_cast<int>(renderer_.grassRenderer->getGrassDensity() * ratio * ratio);
      renderer_.grassRenderer->setGrassDensity(newDensity);
      oldGrassDensity = newDensity;
    }
    renderer_.grassRenderer->setGrassRadius(grassRadius);
    oldGrassRadius = grassRadius;
  }

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Wind Parameters");
  ImGui::SliderFloat("Wind Strength", &renderer_.uniformParams.windStrength, 0.0f, 5.0f);
  ImGui::SliderFloat("Wind Speed", &renderer_.uniformParams.windSpeed, 0.0f, 10.0f);

  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Wind Perlin Parameters");
  PerlinParams windParams = renderer_.getWindParams();
  bool windChanged = false;
  windChanged |= ImGui::SliderInt  ("Wind Octaves", reinterpret_cast<int*>(&windParams.octaves), 1, 20);
  windChanged |= ImGui::SliderFloat("Wind Amplitude", &windParams.amplitude, 0.0f, 1.0f);
  windChanged |= ImGui::SliderFloat("Wind Frequency Multiplier", &windParams.frequencyMultiplier, 1.0f, 4.0f);
  windChanged |= ImGui::SliderFloat("Wind Scale", &windParams.scale, 1.0f, 20.0f);
  if (windChanged)
    renderer_.setWindParams(windParams);
}

void WorldRendererGui::drawInfoTab() const
{
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press 'B' to recompile and reload shaders");

  ImGui::Separator();
  ImGui::NewLine();
  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Controls:");
  ImGui::BulletText("1: Toggle Frustum Culling");
  ImGui::BulletText("2: Toggle Tessellation");
  ImGui::BulletText("3: Toggle Avocados Rendering");
  ImGui::BulletText("4: Toggle Terrain Rendering");
  ImGui::BulletText("5: Toggle Grass Rendering");
  ImGui::BulletText("Z: Toggle GUI tabs");
  ImGui::BulletText("Q: Toggle Debug Terrain Quad");
  ImGui::BulletText("WASD: Move camera");
  ImGui::BulletText("F/R: Move camera up/down");
  ImGui::BulletText("Mouse: Rotate camera (hold right-click)");
  ImGui::BulletText("Scroll: Zoom in/out");
  ImGui::BulletText("Shift: Boost camera speed");
  ImGui::BulletText("Escape: Close application");
}