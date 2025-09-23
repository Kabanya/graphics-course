#pragma once

class WorldRenderer;

class WorldRendererGui
{
public:
    explicit WorldRendererGui(WorldRenderer& renderer);
    void drawGui();

    void drawPerformanceTab();
    void drawRenderTab();
    void drawTerrainTab();
    void drawParticlesTab();
    void drawInfoTab();

private:
    WorldRenderer& renderer_;
};