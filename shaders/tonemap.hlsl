// HDR scene colour -> display. ACES (Narkowicz fit) for now; the film look is tuned later.
#include "common.hlsli"

struct Push {
  uint source;
  uint samplerIndex;
  float exposure;
};
[[vk::push_constant]] Push push;

FullscreenOut VSMain(uint id : SV_VertexID) {
  return fullscreenTriangle(id);
}

float3 acesFitted(float3 x) {
  const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
  return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 linearToSrgb(float3 c) {
  return select(c <= 0.0031308, c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055);
}

float4 PSMain(FullscreenOut i) : SV_Target0 {
  const float3 hdr = gTextures[push.source].SampleLevel(gSamplers[push.samplerIndex], i.uv, 0).rgb;
  return float4(linearToSrgb(acesFitted(hdr * push.exposure)), 1.0);
}
