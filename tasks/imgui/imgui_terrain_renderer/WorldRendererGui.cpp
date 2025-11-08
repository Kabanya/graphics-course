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

  ImGui::Separator();
  ImGui::Text("Camera Speed");
  int currentSpeed = static_cast<int>(renderer_.cameraSpeedLevel);
  const char* speedItems[] = { "Slow", "Middle", "Fast" };
  if (ImGui::Combo("##CameraSpeed", &currentSpeed, speedItems, IM_ARRAYSIZE(speedItems)))
    renderer_.cameraSpeedLevel = static_cast<CameraSpeedLevel>(currentSpeed);
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

  ImGui::InputInt("Terrain Texture Width", reinterpret_cast<int*>(&renderer_.terrainTextureSizeWidth));
  ImGui::InputInt("Terrain Texture Height", reinterpret_cast<int*>(&renderer_.terrainTextureSizeHeight));
  ImGui::InputInt("Compute Workgroup Size", reinterpret_cast<int*>(&renderer_.computeWorkgroupSize));
  ImGui::InputInt("Patch Subdivision", reinterpret_cast<int*>(&renderer_.patchSubdivision));
  renderer_.groupCountX = (renderer_.terrainTextureSizeWidth + renderer_.computeWorkgroupSize - 1) / renderer_.computeWorkgroupSize;
  renderer_.groupCountY = (renderer_.terrainTextureSizeHeight + renderer_.computeWorkgroupSize - 1) / renderer_.computeWorkgroupSize;
  ImGui::Text("Group Count X: %u", renderer_.groupCountX);
  ImGui::Text("Group Count Y: %u", renderer_.groupCountY);
  ImGui::Separator();

  ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Perlin Noise Parameters");
  ImGui::SliderInt  ("Octaves", reinterpret_cast<int*>(&renderer_.perlinParams.octaves), 1, 20);
  ImGui::SliderFloat("Amplitude", &renderer_.perlinParams.amplitude, 0.0f, 1.0f);
  ImGui::SliderFloat("Frequency Multiplier", &renderer_.perlinParams.frequencyMultiplier, 1.0f, 4.0f);
  ImGui::SliderFloat("Scale", &renderer_.perlinParams.scale, 1.0f, 20.0f);

  if (ImGui::Button("Regenerate Terrain"))
    renderer_.regenerateTerrain();
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
  ImGui::BulletText("Z: Toggle GUI tabs");
  ImGui::BulletText("Q: Toggle Debug Terrain Quad");
  ImGui::BulletText("WASD: Move camera");
  ImGui::BulletText("F/R: Move camera up/down");
  ImGui::BulletText("Mouse: Rotate camera (hold right-click)");
  ImGui::BulletText("Scroll: Zoom in/out");
  ImGui::BulletText("Shift: Boost camera speed");
  ImGui::BulletText("Escape: Close application");
}