
add_executable(many_objects_base_baker
  main.cpp
)

target_link_libraries(many_objects_base_baker
  PRIVATE tinygltf)
