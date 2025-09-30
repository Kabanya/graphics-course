set(PARTICLES2_RENDERER_SHADERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

add_executable(particles2_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  WorldRendererGui.cpp
  Emitter.cpp
  ParticleSystem.cpp
)

target_link_libraries(particles2_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_compile_definitions(particles2_renderer PRIVATE PARTICLES2_RENDERER_SHADERS_ROOT="${CMAKE_CURRENT_BINARY_DIR}/shaders/")

target_add_shaders(particles2_renderer
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
  shaders/particle_calculate.comp
  shaders/particle_integrate.comp
  shaders/particle_spawn.comp
)
