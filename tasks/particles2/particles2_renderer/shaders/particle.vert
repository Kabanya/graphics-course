#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 vel;

layout(std140, set = 0, binding = 1) uniform Constants
{
  mat4 viewProj;
} constants;

layout(location = 0) out vec4 vel_out;

void main()
{
  gl_Position = constants.viewProj * vec4(pos.xyz, 1.0);
  gl_PointSize = 5.0;
  vel_out = vel;
}