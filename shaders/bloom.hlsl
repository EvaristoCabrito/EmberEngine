// Engine2's bloom (UnrealBloomPass: threshold 0.75, radius 0.3, strength = the map's bloomIntensity) as a mip pyramid:
// what clears the threshold is brought down five levels (13-tap filter; the first level with a Karis average so a
// small, very bright flame does not shimmer), then built back up with a 3x3 tent, each level adding its own weight. The
// result in level 0 is smooth at every size: the old tonemap-side gather picked the light up in three sparse rings and a
// flame came out as three copies of itself.
#include "common.hlsli"
#include "aces.hlsli"

struct Push {
  uint source;         // sampled texture: the scene colour (level 0) or this pyramid (earlier / later level)
  uint sampler;        // linear, clamp
  uint destination;    // storage view of the level written (RGBA16F)
  float sourceLevel;   // the mip read from the pyramid
  uint2 size;          // the destination level's size
  float2 texel;        // the source's texel size (the level read)
  float weight;        // down: unused; up: the destination level's own weight
  float sourceWeight;  // up: the weight of the level read (1 once it already holds the accumulated levels)
};
[[vk::push_constant]] Push push;

float luma(float3 c) {
  return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// The part of the scene colour that blooms: smoothstep over the display luminance, like the old gather.
float3 brightAt(float2 uv) {
  const float3 c = gTextures[NonUniformResourceIndex(push.source)].SampleLevel(gSamplers[push.sampler], uv, 0).rgb;
  return c * smoothstep(0.75, 0.85, luma(acesFilmic(c)));
}

float3 tapPyramid(float2 uv) {
  return gTextures[NonUniformResourceIndex(push.source)].SampleLevel(gSamplers[push.sampler], uv, push.sourceLevel).rgb;
}

float karis(float3 c) {
  return 1.0 / (1.0 + luma(c));
}

// level 0: from the scene colour (bright pass + Karis average); the others: from the level above.
[numthreads(8, 8, 1)]
void CSDown(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= push.size)) return;
  const float2 uv = (float2(pixel) + 0.5) / float2(push.size);
  const float2 t = push.texel;
  float3 result;
  if (push.sourceLevel < 0.0) {
    const float3 a = brightAt(uv + t * float2(-2, 2)), b = brightAt(uv + t * float2(0, 2)), c = brightAt(uv + t * float2(2, 2));
    const float3 d = brightAt(uv + t * float2(-2, 0)), e = brightAt(uv), f = brightAt(uv + t * float2(2, 0));
    const float3 g = brightAt(uv + t * float2(-2, -2)), h = brightAt(uv + t * float2(0, -2)), i = brightAt(uv + t * float2(2, -2));
    const float3 j = brightAt(uv + t * float2(-1, 1)), k = brightAt(uv + t * float2(1, 1));
    const float3 l = brightAt(uv + t * float2(-1, -1)), m = brightAt(uv + t * float2(1, -1));
    // five overlapping groups (the standard 13-tap's weights: 0.125 for each corner group, 0.5 for the centre), each also
    // weighted by 1 / (1 + luma) of its own mean
    const float3 g0 = (a + b + d + e) * 0.25, g1 = (b + c + e + f) * 0.25, g2 = (d + e + g + h) * 0.25, g3 = (e + f + h + i) * 0.25,
                 g4 = (j + k + l + m) * 0.25;
    const float w0 = 0.125 * karis(g0), w1 = 0.125 * karis(g1), w2 = 0.125 * karis(g2), w3 = 0.125 * karis(g3), w4 = 0.5 * karis(g4);
    result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / (w0 + w1 + w2 + w3 + w4);
  } else {
    const float3 a = tapPyramid(uv + t * float2(-2, 2)), b = tapPyramid(uv + t * float2(0, 2)), c = tapPyramid(uv + t * float2(2, 2));
    const float3 d = tapPyramid(uv + t * float2(-2, 0)), e = tapPyramid(uv), f = tapPyramid(uv + t * float2(2, 0));
    const float3 g = tapPyramid(uv + t * float2(-2, -2)), h = tapPyramid(uv + t * float2(0, -2)), i = tapPyramid(uv + t * float2(2, -2));
    const float3 j = tapPyramid(uv + t * float2(-1, 1)), k = tapPyramid(uv + t * float2(1, 1));
    const float3 l = tapPyramid(uv + t * float2(-1, -1)), m = tapPyramid(uv + t * float2(1, -1));
    result = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = float4(result, 1.0);
}

// From the smallest level up: level L = its own weight x what it held + the tent-filtered level above.
[numthreads(8, 8, 1)]
void CSUp(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= push.size)) return;
  const float2 uv = (float2(pixel) + 0.5) / float2(push.size);
  const float2 t = push.texel;
  float3 sum = tapPyramid(uv + t * float2(-1, -1)) + 2.0 * tapPyramid(uv + t * float2(0, -1)) + tapPyramid(uv + t * float2(1, -1));
  sum += 2.0 * tapPyramid(uv + t * float2(-1, 0)) + 4.0 * tapPyramid(uv) + 2.0 * tapPyramid(uv + t * float2(1, 0));
  sum += tapPyramid(uv + t * float2(-1, 1)) + 2.0 * tapPyramid(uv + t * float2(0, 1)) + tapPyramid(uv + t * float2(1, 1));
  const float3 own = gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel].rgb;
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = float4(own * push.weight + sum / 16.0 * push.sourceWeight, 1.0);
}
