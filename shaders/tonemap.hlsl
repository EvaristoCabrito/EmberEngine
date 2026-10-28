// HDR scene colour -> display. ACES filmic as three.js fits it (Stephen Hill's RRT + ODT fit, input scaled by
// exposure / 0.6), so EmberEngine compares like for like with Ember's prototype (the colour floor); the
// film look is tuned later. Exposure itself lives in the lights (lighting.hlsli); push.exposure is 1.
#include "common.hlsli"
#include "aces.hlsli"

struct Push {
  uint source;
  uint samplerIndex;
  float exposure;
  float bloom;  // Ember's full-scene bloom strength (the map's bloomIntensity); 0 = none
  uint bloomTexture;  // the bloom pyramid (shaders/bloom.hlsl): level 0 holds the finished bloom
  uint glareTexture;  // the sun's glare and rays (shaders/sunglare.hlsl, half resolution); kNoTexture = none
};
[[vk::push_constant]] Push push;

FullscreenOut VSMain(uint id : SV_VertexID) {
  return fullscreenTriangle(id);
}

float3 linearToSrgb(float3 c) {
  return select(c <= 0.0031308, c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055);
}

// Ember's bloom, built by bloom.hlsl as a mip pyramid; level 0 (half resolution) is sampled bilinearly here.
float3 bloomAt(float2 uv) {
  return gTextures[push.bloomTexture].SampleLevel(gSamplers[push.samplerIndex], uv, 0).rgb;
}

float4 PSMain(FullscreenOut i) : SV_Target0 {
  const float3 hdr = gTextures[push.source].SampleLevel(gSamplers[push.samplerIndex], i.uv, 0).rgb;
  if (push.exposure < 0.0) return float4(hdr, 1.0);  // debug views: values written as they are
  float3 c = hdr * push.exposure;
  if (push.bloom > 0.0) c += bloomAt(i.uv) * push.bloom;
  if (push.glareTexture != 0xFFFFFFFFu) c += gTextures[push.glareTexture].SampleLevel(gSamplers[push.samplerIndex], i.uv, 0).rgb;
  return float4(linearToSrgb(acesFilmic(c)), 1.0);
}
