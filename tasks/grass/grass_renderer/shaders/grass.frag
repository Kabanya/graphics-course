#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT {
  vec3 wPos;
  vec3 normal;
} fs_in;

void main()
{
  const vec3 wLightPos = vec3(50, 10, 255);
  const vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
  const vec3 surfaceColor = vec3(0.0, 0.5, 0.0);

  const vec3 lightDir = normalize(wLightPos - fs_in.wPos);
  const vec3 diffuse = max(dot(fs_in.normal, lightDir), 0.0f) * lightColor;
  const vec3 ambient = vec3(0.1, 0.1, 0.1);

  out_fragColor.rgb = (diffuse + ambient) * surfaceColor;
  out_fragColor.a = 1.0f;
}