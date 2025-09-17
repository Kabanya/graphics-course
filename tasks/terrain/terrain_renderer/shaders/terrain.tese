#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, cw) in;

layout(binding = 0) uniform sampler2D perlinNoise;
layout(std140, set = 0, binding = 1) uniform TerrainConsts
{
  mat4 proj;
  mat4 viewProj;
  vec4 camView;
  vec2 textureSize;
  int enableTessellation;
} constants;

const float zScale = 400.0;

layout(location = 0) in vec3 WorldPos_ES_in[];
layout(location = 1) in vec2 TexCoord_ES_in[];
layout(location = 2) in float GridSize[];

layout(location = 0) out VS_OUT
{
  vec3 wPos;
  vec2 texCoord;
  float tessLevel;
};

out gl_PerVertex
{
  vec4 gl_Position;
};

void main()
{
  wPos.y = WorldPos_ES_in[0].y;
  wPos.xz = mix(WorldPos_ES_in[0].xz, WorldPos_ES_in[1].xz, gl_TessCoord.xy);
  texCoord = mix(TexCoord_ES_in[0], TexCoord_ES_in[1], gl_TessCoord.xy);
 
  wPos.y += texture(perlinNoise, texCoord).x * zScale;

  gl_Position = constants.viewProj * vec4(wPos, 1.0);

  tessLevel = GridSize[0];
}