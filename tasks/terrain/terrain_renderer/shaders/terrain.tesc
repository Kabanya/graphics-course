#version 450
layout(vertices = 2) out;

layout(std140, set = 0, binding = 1) uniform TerrainConsts
{
  mat4 proj;
  mat4 viewProj;
  vec4 camView;
  vec2 textureSize;
  int enableTessellation;
  float minTessLevel;
  float maxTessLevel;
  float minDistance;
  float maxDistance;
} constants;

const vec3 startPos = vec3(-1050, -700, -2100);
const uint squareSize = 8;

layout(location = 0) in uint InstanceIndex[];

layout(location = 0) out vec3 WorldPos_ES_in[];
layout(location = 1) out vec2 TexCoord_ES_in[];
layout(location = 2) out float GridSize[];

vec3 calcPos(in uint cornerNum)
{
  const uint gridSize = uint(constants.textureSize.x) / squareSize;
  uvec2 sqrCoord = uvec2(InstanceIndex[0] % gridSize, InstanceIndex[0] / gridSize);
  uvec2 offset = uvec2(cornerNum / 2u, 1u - (cornerNum % 2u));
  return vec3((sqrCoord + offset) * squareSize, 0).xzy + startPos;
}

vec2 calcTexcoord(in uint cornerNum)
{
  const uint gridSize = uint(constants.textureSize.x) / squareSize;
  uvec2 sqrCoord = uvec2(InstanceIndex[0] % gridSize, InstanceIndex[0] / gridSize);
  uvec2 offset = uvec2(cornerNum / 2u, 1u - (cornerNum % 2u));
  return vec2((sqrCoord + offset) * squareSize) / constants.textureSize.x;
}

float GetTessLevel(in float dist)
{
  if (constants.enableTessellation == 0) {
    return 1.0;
  }

  // Нормализуем расстояние от 0 до 1
  float normalizedDist = clamp((dist - constants.minDistance) / (constants.maxDistance - constants.minDistance), 0.0, 1.0);
  
  // Инвертируем: близко = высокая тесселяция, далеко = низкая
  return mix(constants.maxTessLevel, constants.minTessLevel, normalizedDist);
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

  const float centerDist = min(min(ol0, ol2), min(ol1, ol3));

  gl_TessLevelOuter[0] = GetTessLevel(ol0);
  gl_TessLevelOuter[1] = GetTessLevel(ol1);
  gl_TessLevelOuter[2] = GetTessLevel(ol2);
  gl_TessLevelOuter[3] = GetTessLevel(ol3);
  
  float centerTess = GetTessLevel(centerDist);
  gl_TessLevelInner[0] = centerTess;
  gl_TessLevelInner[1] = centerTess;

  // Передаем только 2 угла как в рабочем коде
  WorldPos_ES_in[gl_InvocationID] = gl_InvocationID == 0 ? corner00 : corner11;
  TexCoord_ES_in[gl_InvocationID] = gl_InvocationID == 0 ? calcTexcoord(0) : calcTexcoord(3);
  GridSize[gl_InvocationID] = float(squareSize);
}