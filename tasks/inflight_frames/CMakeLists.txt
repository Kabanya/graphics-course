
add_executable(inflight_frames
  main.cpp
  App.cpp
)

target_link_libraries(inflight_frames
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(inflight_frames
        shaders/shadertoy2.frag
        shaders/shadertoy2.vert
        shaders/shadertoy2.comp
)
