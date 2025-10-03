
add_executable(grass_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  WorldRendererGui.cpp
  TerrainRenderer.cpp
  GrassRenderer.cpp
)

target_link_libraries(grass_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(grass_renderer
  shaders/static_mesh.frag
  shaders/static_mesh.vert
  shaders/terrain_normal.comp
  shaders/terrain_perlin.comp
  shaders/terrain.tesc
  shaders/terrain.tese
  shaders/terrain.frag
  shaders/quad.vert
  shaders/grass_gen.comp
  shaders/grass.vert
  shaders/grass.frag
)
