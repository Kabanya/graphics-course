#version 450

layout(vertices = 4) out;

layout(std140, set = 0, binding = 1) uniform TerrainConsts
{
  mat4 proj;
  vec3 camView;
  int enableTessellation;
} constants;

layout(binding = 0) uniform sampler2D perlinNoise;

const vec3 startPos = vec3(-1050, -700, -2100);
const uint squareSize = 8;

layout(location = 0) in uint InstanceIndex[];

layout(location = 0) out vec3 WorldPos_ES_in[];
layout(location = 1) out vec2 TexCoord_ES_in[];

const float maxDist = 1024.0;
const float minDist = 1.0;

const float maxTess = 50.0;
const float minTess = 1.0;

vec3 calcPos(in uint cornerNum)
{
  const uint gridSize = 4096 / squareSize;
  uvec2 sqrCoord = uvec2(InstanceIndex[0] / gridSize, InstanceIndex[0] % gridSize);
  uvec2 offset = uvec2(cornerNum / 2u, cornerNum % 2u);
  return vec3((sqrCoord + offset) * squareSize, 0).xzy + startPos;
}

float GetTessLevel(in float dist)
{
  dist = clamp((dist - minDist) / (maxDist - minDist), 0.0, 1.0);
  return mix(maxTess, minTess, dist);
}

void main()
{
  const vec3 corner00 = calcPos(0);
  const vec3 corner01 = calcPos(1);
  const vec3 corner10 = calcPos(2);
  const vec3 corner11 = calcPos(3);

  const float dist00 = distance(corner00, constants.camView);
  const float dist01 = distance(corner01, constants.camView);
  const float dist10 = distance(corner10, constants.camView);
  const float dist11 = distance(corner11, constants.camView);

  const float upDist = min(dist01, dist11);
  const float downDist = min(dist00, dist10);
  const float rightDist = min(dist10, dist11);
  const float leftDist = min(dist00, dist01);

  const float centerDist = min(upDist, downDist);

  if (constants.enableTessellation == 0)
  {
    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[1] = 1.0;
    gl_TessLevelOuter[2] = 1.0;
    gl_TessLevelOuter[3] = 1.0;
    gl_TessLevelInner[0] = 1.0;
  }
  else
  {
    gl_TessLevelOuter[0] = GetTessLevel(leftDist);
    gl_TessLevelOuter[1] = GetTessLevel(downDist);
    gl_TessLevelOuter[2] = GetTessLevel(rightDist);
    gl_TessLevelOuter[3] = GetTessLevel(upDist);
    gl_TessLevelInner[0] = GetTessLevel(centerDist);
  }

  WorldPos_ES_in[gl_InvocationID] = gl_InvocationID == 0 ? corner00 : corner11;
  TexCoord_ES_in[gl_InvocationID] = WorldPos_ES_in[gl_InvocationID].xz / 4096.0 + 0.5;
}