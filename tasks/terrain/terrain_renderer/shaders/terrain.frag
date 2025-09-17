#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_fragColor;
layout(binding = 2) uniform sampler2D normalMap;


layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec2 texCoord;
  float tessLevel;
} surf;

void main()
{
  const vec3 sunDirection = normalize(vec3(0.4, 0.7, 0.6));
  const vec3 sunColor = vec3(1.0, 0.98, 0.92) * 0.7;
  const vec3 skyColor = vec3(0.6, 0.8, 1.0) * 0.25;
  const vec3 warmAmbient = vec3(0.7, 0.8, 0.6) * 0.18;

  vec3 wNorm = texture(normalMap, surf.texCoord).xyz * 2.0 - 1.0;
  wNorm = normalize(wNorm);

  float tessNormalized = clamp(surf.tessLevel * 10.0, 0.0, 1.0);
  vec3 tessColor = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), tessNormalized);

  vec3 surfaceColor = mix(vec3(0.32, 0.72, 0.22), tessColor, 0.7);

  float NdotL = clamp(dot(wNorm, sunDirection), 0.0, 1.0);
  NdotL = pow(NdotL, 1.1);
  vec3 diffuse = NdotL * sunColor;

  float skyFactor = wNorm.y * 0.5 + 0.5;
  vec3 skyLight = skyColor * skyFactor;

  vec3 ambient = warmAmbient + skyLight;

  float rimFactor = 1.0 - max(dot(wNorm, normalize(-surf.wPos)), 0.0);
  rimFactor = pow(rimFactor, 2.5) * 0.12;
  vec3 rimLight = rimFactor * vec3(0.8, 0.9, 1.0);

  vec3 finalColor = (diffuse + ambient + rimLight) * surfaceColor;

  out_fragColor.rgb = finalColor;
  out_fragColor.a = 1.0f;
}
