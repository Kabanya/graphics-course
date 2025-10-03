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

layout(std140, set = 0, binding = 3) uniform GrassParams {
  vec3 eyePos;
  float terrainSize;
  float grassHeight;
  float grassDensity;
  float grassRadius;
  float grassWidth;
} params;

layout(binding = 4) uniform sampler2D windMap;

layout(std140, set = 0, binding = 5) uniform AppData
{
  mat4 lightMatrix;
  vec3 lightPos;
  float time;
  vec3 baseColor;
  float windStrength;
} appData;

layout(location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 normal;
} vs_out;

void main()
{
  uint bladeIndex = gl_VertexIndex / 6;
  uint vertexInBlade = gl_VertexIndex % 6;

  vec3  bladePos    = blades.blades[bladeIndex].pos;
  float bladeHeight = blades.blades[bladeIndex].height;

  if (bladeHeight == 0.0)
  {
    gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
    return;
  }

  vec3 dir = normalize(bladePos - constants.camView.xyz);
  vec3 right = normalize(cross(dir, vec3(0, 1, 0)));
  vec3 up = normalize(cross(right, dir));

  float halfWidth = params.grassWidth * 0.5;

  vec3 offsets[6] = vec3[6](
    -right * halfWidth, // bottom left
    right * halfWidth,  // bottom right
    -right * halfWidth + up * bladeHeight, // top left
    -right * halfWidth + up * bladeHeight, // top left again
    right * halfWidth,  // bottom right
    right * halfWidth + up * bladeHeight   // top right
  );

  vec3 vertexPos = bladePos + offsets[vertexInBlade];

  // Sample wind
  vec2 windTexCoord = (bladePos.xz / params.terrainSize) * 0.5 + 0.5; // assuming terrain from -terrainSize/2 to terrainSize/2
  vec2 windDir = texture(windMap, windTexCoord).xy;
  float windFactor = sin(appData.time * 5.0 + bladePos.x * 0.01 + bladePos.z * 0.01) * 0.5 + 0.5; // animate over time
  float heightFactor = offsets[vertexInBlade].y / bladeHeight; // 0 at bottom, 1 at top
  vec3 windOffset = vec3(windDir.x, 0.0, windDir.y) * appData.windStrength * windFactor * heightFactor;

  vertexPos += windOffset;

  vs_out.wPos = vertexPos;
  vs_out.normal = normalize(cross(up, right));
  gl_Position = constants.viewProj * vec4(vertexPos, 1.0);
}