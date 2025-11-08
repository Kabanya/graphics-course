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
  vec3 particleColor;
} uniformParams;

layout(location = 0) in float inLifetime;
layout(location = 1) in vec3 inWorldPos;

layout(location = 0) out vec4 out_fragColor;

void main()
{
  vec2 coord = gl_PointCoord - 0.5;
  float dist = length(coord);
  if (dist > 0.5) discard;
  float ringPattern = sin(dist * 20.0 - uniformParams.time * 5.0) * 0.5 + 0.5;
  float glow = 1.0 - smoothstep(0.0, 0.5, dist);
  float coreGlow = 1.0 - smoothstep(0.0, 0.2, dist);
  float angle = atan(coord.y, coord.x);
  float spiral = sin(angle * 5.0 + uniformParams.time * 2.0 - dist * 10.0) * 0.5 + 0.5;
  vec3 centerColor = uniformParams.particleColor * 2.0;
  vec3 edgeColor = uniformParams.particleColor * vec3(0.5, 0.8, 1.5);
  vec3 color = mix(edgeColor, centerColor, coreGlow);
  color *= (0.7 + ringPattern * 0.3);
  color += spiral * 0.3 * glow;
  float twinkle = sin(uniformParams.time * 10.0 + inWorldPos.x * 100.0) * 0.2 + 0.8;
  float alpha = uniformParams.particleAlpha * glow * glow * twinkle;
  out_fragColor = vec4(color, alpha);
}