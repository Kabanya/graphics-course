#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#include "cpp_glsl_compat.h"

struct PerlinParams
{
  shader_uint octaves;
  shader_float amplitude;
  shader_float frequencyMultiplier;
  shader_float scale;
  shader_float time;
};

struct UniformParams
{
  shader_mat4 lightMatrix;
  shader_vec3 lightPos;
  shader_float time;
  shader_vec3 baseColor;
  shader_float windStrength;
  shader_float windSpeed;
  shader_float enableDynamicWind;
};

#endif // UNIFORM_PARAMS_H_INCLUDED