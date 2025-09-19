
add_executable(imgui_terrain_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
)

target_link_libraries(imgui_terrain_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(imgui_terrain_renderer
  shaders/static_mesh.frag
  shaders/static_mesh.vert
  shaders/terrain_normal.comp
  shaders/terrain_perlin.comp
  shaders/terrain.tesc
  shaders/terrain.tese
  shaders/terrain.frag
  shaders/quad.vert
)
