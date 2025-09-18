#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_fragColor;
layout(binding = 1) uniform sampler2D normalMapTerrainImage;


layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec2 texCoord;
} surf;

void main()
{
  const vec3 wLightPos = vec3(50, 10, 255);
  const vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
  const vec3 wNorm = normalize(texture(normalMapTerrainImage, surf.texCoord).xyz * 2.0 - 1.0);
  const vec3 surfaceColor = vec3(0.4, 0.8, 0.2);

  const vec3 lightDir = normalize(wLightPos - surf.wPos);
  const vec3 viewDir = normalize(-surf.wPos);
  const vec3 halfDir = normalize(lightDir + viewDir);

  const vec3 diffuse = max(dot(wNorm, lightDir), 0.0f) * lightColor;
  const vec3 specular = pow(max(dot(wNorm, halfDir), 0.0f), 128.0f) * lightColor;
  const vec3 ambient = vec3(0.1, 0.1, 0.1);

  out_fragColor.rgb = (diffuse + specular + ambient) * surfaceColor;
  out_fragColor.a = 1.0f;
}