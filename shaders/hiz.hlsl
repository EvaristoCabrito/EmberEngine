// Depth pyramid for occlusion culling. Each level holds the farthest depth (reversed-Z: the minimum)
// of the texels it covers: one sample through a min-reduction sampler reads a 2x2 footprint.
//   level 0: from the depth buffer; level n: from level n-1.
#include "common.hlsli"

struct Push {
  uint source;   // sampled texture: depth buffer (level 0) or the pyramid (earlier level)
  uint sampler;  // min-reduction, clamp
  uint destination;  // storage view of this level (R32F)
  float sourceLevel;
  uint2 size;    // this level's size
};
[[vk::push_constant]] Push push;

[numthreads(8, 8, 1)]
void CSDownsample(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= push.size)) return;
  const float2 uv = (float2(pixel) + 0.5) / float2(push.size);
  const float d = gTextures[NonUniformResourceIndex(push.source)].SampleLevel(gSamplers[push.sampler], uv, push.sourceLevel).r;
  gStorageImagesR32f[NonUniformResourceIndex(push.destination)][pixel] = d;
}
