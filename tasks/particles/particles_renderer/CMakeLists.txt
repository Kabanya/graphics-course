set(IMGUI_TERRAIN_RENDERER_SHADERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

add_executable(particles_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  WorldRendererGui.cpp
  Emitter.cpp
  ParticleSystem.cpp
)

target_link_libraries(particles_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_compile_definitions(particles_renderer PRIVATE IMGUI_TERRAIN_RENDERER_SHADERS_ROOT="${CMAKE_CURRENT_BINARY_DIR}/shaders/")

target_add_shaders(particles_renderer
  shaders/static_mesh.frag
  shaders/static_mesh.vert
  shaders/terrain_normal.comp
  shaders/terrain_perlin.comp
  shaders/terrain.tesc
  shaders/terrain.tese
  shaders/terrain.frag
  shaders/quad.vert
  shaders/particle.frag
  shaders/particle.vert
)
