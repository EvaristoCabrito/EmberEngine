// HDR scene colour -> display. ACES filmic as three.js fits it (Stephen Hill's RRT + ODT fit, input scaled by
// exposure / 0.6), so EmberEngine compares like for like with Engine2's prototype (the colour floor); the
// film look is tuned later. Exposure itself lives in the lights (lighting.hlsli); push.exposure is 1.
#include "common.hlsli"

struct Push {
  uint source;
  uint samplerIndex;
  float exposure;
  float bloom;  // Engine2's full-scene bloom strength (the map's bloomIntensity); 0 = none
};
[[vk::push_constant]] Push push;

FullscreenOut VSMain(uint id : SV_VertexID) {
  return fullscreenTriangle(id);
}

float3 rrtAndOdtFit(float3 v) {
  const float3 a = v * (v + 0.0245786) - 0.000090537;
  const float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
  return a / b;
}

float3 acesFilmic(float3 color) {
  // sRGB -> AP1-ish input and back (three.js ACESInputMat / ACESOutputMat, column-major there).
  const float3x3 inputMat = float3x3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834, 0.01566, 0.02840, 0.13383, 0.83777);
  const float3x3 outputMat = float3x3(1.60475, -0.53108, -0.07367, -0.10208, 1.10813, -0.00605, -0.00327, -0.07276, 1.07602);
  color = color / 0.6;
  color = mul(inputMat, color);
  color = rrtAndOdtFit(color);
  color = mul(outputMat, color);
  return saturate(color);
}

float3 linearToSrgb(float3 c) {
  return select(c <= 0.0031308, c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055);
}

// Engine2's bloom (UnrealBloomPass: threshold 0.75, radius 0.3, strength = the map's bloomIntensity), gathered here
// in one pass: what clears the threshold on screen, picked up in three rings (near, mid, wide) and added back.
float3 bright(float2 uv) {
  const float3 c = gTextures[push.source].SampleLevel(gSamplers[push.samplerIndex], uv, 0).rgb * push.exposure;
  const float l = dot(acesFilmic(c), float3(0.2126, 0.7152, 0.0722));
  return c * smoothstep(0.75, 0.85, l);
}
float3 bloomAt(float2 uv) {
  uint w, h;
  gTextures[push.source].GetDimensions(w, h);
  const float2 px = 1.0 / float2(w, h) * (h / 1080.0);
  const float radii[3] = {5.0, 14.0, 36.0}, weights[3] = {0.5, 0.3, 0.2};
  float3 sum = 0.0;
  [unroll] for (uint r = 0; r < 3; ++r) {
    float3 ring = 0.0;
    [unroll] for (uint k = 0; k < 8; ++k) {
      const float a = (k + 0.5 * r) * 0.78539816;
      ring += bright(uv + float2(cos(a), sin(a)) * radii[r] * px);
    }
    sum += ring / 8.0 * weights[r];
  }
  return sum;
}

float4 PSMain(FullscreenOut i) : SV_Target0 {
  const float3 hdr = gTextures[push.source].SampleLevel(gSamplers[push.samplerIndex], i.uv, 0).rgb;
  if (push.exposure < 0.0) return float4(hdr, 1.0);  // debug views: values written as they are
  float3 c = hdr * push.exposure;
  if (push.bloom > 0.0) c += bloomAt(i.uv) * push.bloom;
  return float4(linearToSrgb(acesFilmic(c)), 1.0);
}
