#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Blade{
  vec3 pos;
  float height;
};

layout(binding = 0, std430) readonly buffer Blades
{
  Blade blades[];
} blades;

layout(std140, set = 0, binding = 2) uniform Constants
{
  mat4 viewProj;
  vec4 camView;
  int enableTessellation;
} constants;

layout(location = 0) out VS_OUT
{
  vec3 wPos;
} vs_out;

void main()
{
  uint bladeIndex = gl_VertexIndex / 2;
  uint vertexType = gl_VertexIndex % 2;

  vec3 bladePos = blades.blades[bladeIndex].pos;
  float bladeHeight = blades.blades[bladeIndex].height;

  vec3 vertexPos = bladePos;
  if (vertexType == 1)
  {
    vertexPos.y += bladeHeight;
  }

  vs_out.wPos = vertexPos;
  gl_Position = constants.viewProj * vec4(vertexPos, 1.0);
}