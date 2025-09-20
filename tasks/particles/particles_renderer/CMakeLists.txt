
add_executable(particles_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
)

target_link_libraries(particles_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(particles_renderer
  shaders/static_mesh.frag
  shaders/static_mesh.vert
  shaders/terrain_normal.comp
  shaders/terrain_perlin.comp
  shaders/terrain.tesc
  shaders/terrain.tese
  shaders/terrain.frag
  shaders/quad.vert
)
