#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inPositionSize;

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
  vec3 particleColor;
} uniformParams;

layout(location = 0) out float outLifetime;
layout(location = 1) out vec3 outWorldPos;

void main()
{
  vec3 worldPos = inPositionSize.xyz;
  worldPos.y += sin(uniformParams.time + inPositionSize.x * 2.0) * 0.5;
  worldPos.x += cos(uniformParams.time * 0.7 + inPositionSize.z * 2.0) * 0.3;
  gl_Position = constants.viewProj * vec4(worldPos, 1.0);
  float pulsation = 1.0 + 0.5 * sin(uniformParams.time * 3.0 + inPositionSize.x * 10.0);
  gl_PointSize = inPositionSize.w * pulsation;
  outLifetime = fract(uniformParams.time * 0.5 + inPositionSize.x);
  outWorldPos = worldPos;
}