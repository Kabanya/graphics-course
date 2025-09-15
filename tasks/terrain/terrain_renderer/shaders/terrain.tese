#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, ccw) in;

layout(binding = 0) uniform sampler2D perlinNoise;
layout(std140, set = 0, binding = 1) uniform TerrainConstants
{
  mat4 proj;
  vec3 camView;
} constants;

const float zScale = 450.0;

layout(location = 0) in vec3 WorldPos_ES_in[];
layout(location = 1) in vec2 TexCoord_ES_in[];

layout(location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
};

out gl_PerVertex
{
  vec4 gl_Position;
};

vec3 calcNorm(vec2 texCoord){
  float h1 = textureOffset(perlinNoise, texCoord, ivec2(-1, 0)).x;
  float h2 = textureOffset(perlinNoise, texCoord, ivec2(1, 0)).x;
  float h3 = textureOffset(perlinNoise, texCoord, ivec2(0, -1)).x;
  float h4 = textureOffset(perlinNoise, texCoord, ivec2(0, 1)).x;

  vec3 gradient = vec3(h1 - h2, h3 - h4, 2.0);
  return normalize(gradient);
}

void main()
{
  vec2 tessCoords = gl_TessCoord.xy;
  vec3 interpolationMask = vec3(tessCoords, 0).xzy;

  wPos = mix(WorldPos_ES_in[0], WorldPos_ES_in[1], interpolationMask);
  texCoord = mix(TexCoord_ES_in[0], TexCoord_ES_in[1], tessCoords);

  float heightSample = texture(perlinNoise, texCoord).x;
  wPos.y += heightSample * zScale;

  wTangent = normalize(vec3(0, 0, 1));
  wNorm = calcNorm(texCoord);

  vec4 worldPosition = vec4(wPos, 1.0);
  gl_Position = constants.proj * worldPosition;
}