#version 460

// Adjusted from here: https://github.com/lettier/3d-game-shaders-for-beginners/blob/master/demonstration/shaders/fragment/median-filter.frag

#define MAX_SIZE        3
#define MAX_KERNEL_SIZE ((MAX_SIZE * 2 + 1) * (MAX_SIZE * 2 + 1))
#define MAX_BINS_SIZE   8

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) out vec4 outFragColor;
  
layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const int32_t Size = 2;
layout(constant_id = 1) const int32_t BinsSize = 6;

void main() {
  const vec2 texSize  = textureSize(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], 0).xy;
  const vec2 texelSize = 1.0 / textureSize(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], 0);

  int32_t size = Size;
  if (size <= 0) { outFragColor = texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV); return; }
  if (size > MAX_SIZE) { size = MAX_SIZE; }
  
  const int32_t kernelSize = int32_t(pow(size * 2 + 1, 2));
  const int32_t binsSize = clamp(BinsSize, 1, MAX_BINS_SIZE);

  int32_t i        = 0;
  int32_t j        = 0;
  int32_t count    = 0;
  int32_t binIndex = 0;

  vec4  colors[MAX_KERNEL_SIZE];
  float bins[MAX_BINS_SIZE];
  int32_t binIndexes[colors.length()];

  float total = 0;
  const float limit = floor(float(kernelSize) / 2) + 1;

  float value       = 0;
  const vec3 valueRatios = vec3(0.3, 0.59, 0.11);

  for (i = -size; i <= size; ++i) {
    for (j = -size; j <= size; ++j) {
      const vec2 offsetUV = inUV + vec2(i, j) * texelSize;
      colors[count] = texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], offsetUV);
      ++count;
    }
  }

  for (i = 0; i < binsSize; ++i) {
    bins[i] = 0;
  }

  for (i = 0; i < kernelSize; ++i) {
    value           = dot(colors[i].rgb, valueRatios);
    binIndex        = int(floor(value * binsSize));
    binIndex        = clamp(binIndex, 0, binsSize - 1);
    bins[binIndex] += 1;
    binIndexes[i]   = binIndex;
  }

  binIndex = 0;

  for (i = 0; i < binsSize; ++i) {
    total += bins[i];
    if (total >= limit) {
      binIndex = i;
      break;
    }
  }

  outFragColor = colors[0];

  for (i = 0; i < kernelSize; ++i) {
    if (binIndexes[i] == binIndex) {
      outFragColor = colors[i];
      break;
    }
  }
}