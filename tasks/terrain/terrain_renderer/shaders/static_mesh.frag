#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

void main()
{
  const vec3 wLightPos = vec3(100, 150, 600);
  const vec3 lightColor = vec3(1.2, 1.1, 0.9); // Warm sunlight
  const vec3 surfaceColor = vec3(0.4, 0.5, 0.3); // green-brown terrain

  // Directional lighting
  vec3 lightDir = normalize(wLightPos - surf.wPos);
  float NdotL = dot(surf.wNorm, lightDir);

  float lightFactor = smoothstep(-0.6, 1.0, NdotL);
  vec3 diffuse = lightFactor * lightColor;

  float skyFactor = (surf.wNorm.y + 1.0) * 0.5;
  vec3 skyColor = vec3(0.4, 0.6, 0.8);
  vec3 groundColor = vec3(0.1, 0.05, 0.03);
  vec3 ambient = mix(groundColor, skyColor, skyFactor) * 0.4;

  vec3 finalColor = (diffuse + ambient) * surfaceColor;

  out_fragColor = vec4(finalColor, 1.0);
}
