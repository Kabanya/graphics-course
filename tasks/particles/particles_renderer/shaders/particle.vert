#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inPositionSize;

layout(std140, set = 0, binding = 1) uniform Constants
{
  mat4 viewProj;
} constants;

void main()
{
  gl_Position = constants.viewProj * vec4(inPositionSize.xyz, 1.0);
  gl_PointSize = inPositionSize.w;
}