#pragma once

class WorldRenderer;

class WorldRendererGui
{
public:
  explicit WorldRendererGui(WorldRenderer& renderer);
  void drawGui();

  void drawPerformanceTab() const;
  void drawRenderTab();
  void drawTerrainTab();
  void drawGrassTab();
  // void drawParticlesTab();
  void drawInfoTab() const;

private:
  WorldRenderer& renderer_;
};