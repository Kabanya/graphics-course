#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(std140, set = 0, binding = 1) uniform Constants
{
  mat4 viewProj;
} constants;

layout(std140, set = 0, binding = 2) uniform UniformParams
{
  mat4 lightMatrix;
  vec3 lightPos;
  float time;
  vec3 baseColor;
  float particleAlpha;
} uniformParams;

layout(location = 0) out vec4 out_fragColor;

void main()
{
  out_fragColor = vec4(1.0, 1.0, 1.0, uniformParams.particleAlpha);
}