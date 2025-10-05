#version 450

layout(vertices = 2) out;

layout(std140, set = 0, binding = 2) uniform Constants
{
  mat4 viewProj;
  vec4 camView;
  int enableTessellation;
} constants;

const vec3 TerrainPos = vec3(-500, -300, -500);
const uint squareSize = 32;

layout(location = 0) in uint InstanceIndex[];

layout(location = 0) out vec3 WorldPos_ES_in[];
layout(location = 1) out vec2 TexCoord_ES_in[];
layout(location = 2) out float GridSize[];

const float maxDist = 500.0;
const float minDist = 50.0;

const float maxTess = 128.0;
const float minTess = 1.0;

vec3 calcPos(in uint cornerNum)
{
  const uint gridSize = 1024 / squareSize;
  uvec2 sqrCoord = uvec2(InstanceIndex[0] % gridSize, (1023u - InstanceIndex[0]) / gridSize);
  uvec2 offset = uvec2(cornerNum / 2u, 1u - (cornerNum % 2u));
  return vec3((sqrCoord + offset) * squareSize, 0).xzy + TerrainPos;
}

vec2 calcTexcoord(in uint cornerNum)
{
  const uint gridSize = 1024 / squareSize;
  uvec2 sqrCoord = uvec2(InstanceIndex[0] % gridSize, InstanceIndex[0] / gridSize);
  uvec2 offset = uvec2(cornerNum / 2u, cornerNum % 2u);
  return vec2((sqrCoord + offset) * squareSize) / 1024;
}

float GetTessLevel(in float dist)
{
  if (constants.enableTessellation == 0) return 1.0;
  dist = clamp((dist - minDist) / (maxDist - minDist), 0.0, 1.0);
  return mix(maxTess, minTess, dist);
}

void main()
{
  const vec3 corner00 = calcPos(0);
  const vec3 corner01 = calcPos(1);
  const vec3 corner10 = calcPos(2);
  const vec3 corner11 = calcPos(3);

  const float dist00 = distance(corner00, constants.camView.xyz);
  const float dist01 = distance(corner01, constants.camView.xyz);
  const float dist10 = distance(corner10, constants.camView.xyz);
  const float dist11 = distance(corner11, constants.camView.xyz);

  const float ol0 = min(dist00, dist01);
  const float ol1 = min(dist00, dist10);
  const float ol2 = min(dist10, dist11);
  const float ol3 = min(dist01, dist11);

  const float centerDist = min(ol0, ol3);

  gl_TessLevelOuter[0] = GetTessLevel(ol0);
  gl_TessLevelOuter[1] = GetTessLevel(ol1);
  gl_TessLevelOuter[2] = GetTessLevel(ol2);
  gl_TessLevelOuter[3] = GetTessLevel(ol3);
  float centerTessLevel = GetTessLevel(centerDist);
  gl_TessLevelInner[0] = centerTessLevel;
  gl_TessLevelInner[1] = centerTessLevel;

  WorldPos_ES_in[gl_InvocationID] = gl_InvocationID == 0 ? corner00 : corner11;
  TexCoord_ES_in[gl_InvocationID] = gl_InvocationID == 0 ? calcTexcoord(0) : calcTexcoord(3);
  GridSize[gl_InvocationID] = squareSize / (centerTessLevel * 1024.0);
}