// Nuvens RDR2 (clouds_rdr2.hlsli the shape, cloudsvol_rdr2.hlsli the light): the compute passes, recorded once a frame by
// render/CloudsRdr2.cpp in this order:
//   CSWeather     the weather map (1024 x 1024 on Ultra): the low layer's coverage and type, the cirrus cover, the growth
//   CSMarch       the march at the preset's resolution (Ultra: every pixel of the screen, every frame; High: a 2 x 2 block
//                 per ray; Low: 4 x 4), each ray's start dithered by blue noise rotated every frame
//   CSResolve     the temporal pass: last frame's resolved clouds reprojected through last frame's view-projection, clamped
//                 to the colour box of the current 3 x 3 neighbourhood (no trail), blended under the new march
//   CSShadow      the shadow map over the board (2048 x 2048 on Ultra): the light's transmittance through the layer
//   CSShadowBlur  its separable blur (the talk's ESM is blurred the same way): what cloudShadow() reads on every surface
// The noise volumes (Perlin-Worley body, Worley detail) are cloudsvol.hlsl's, made once.
#include "cloudsvol_rdr2.hlsli"

struct Push {
  uint64_t frame;    // MeshFrame
  uint destination;  // storage view written by the pass
  uint width, height;
  uint source;       // CSShadowBlur: the sampled map read; CSResolve: unused (the record names the march and the history)
  uint mode;         // CSShadowBlur: 0 horizontal, 1 vertical
};
[[vk::push_constant]] Push push;

// --- The weather map ---------------------------------------------------------------------------------------------------------
[numthreads(8, 8, 1)]
void CSWeather(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const Rdr2Clouds c = loadRdr2(f);
  const float2 xz = c.worldRect.xy + (float2(pixel) + 0.5) / float2(push.width, push.height) / c.worldRect.zw;
  // Fronts and cells: a slow large field (the fronts, 4000 units) and the cells inside them (900 units), both drifting.
  const float2 drift = c.wind * c.time;
  const float front = cloudFbm((xz + drift * 0.6) / 4200.0 + 5.1);
  const float cells = cloudFbm((xz + drift) / 920.0 + 1.7);
  const float n = saturate((cells - 0.18) / 0.62);
  const float threshold = 1.0 - saturate(c.coverage);
  float cover = smoothstep(threshold - 0.14, threshold + 0.24, n);
  // The fronts thin the cover where the weather is light, and matter less as the sky closes in.
  cover *= lerp(smoothstep(0.22, 0.62, front), 1.0, saturate(c.coverage * 1.3));
  const float kind = saturate(c.kind + (cloudNoise((xz + drift * 0.4) / 3400.0 + 23.1) - 0.5) * 0.55);
  // Cirrus: its own slow field, high and sparse.
  const float cirrusField = cloudFbm((xz + drift * 2.0) / 5200.0 + 43.3);
  const float cirrusThreshold = 1.0 - saturate(c.cirrusCoverage);
  const float cirrus = smoothstep(cirrusThreshold - 0.2, cirrusThreshold + 0.2, saturate((cirrusField - 0.2) / 0.6)) * saturate(c.cirrusCoverage);
  // Growth: the towers rise over the densest cells only.
  const float growth = saturate(c.growth) * smoothstep(0.5, 0.92, cover * (0.5 + 0.5 * cloudNoise((xz + drift * 0.8) / 1500.0 + 61.7)));
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = float4(cover, kind, cirrus, growth);
}

// --- The march ---------------------------------------------------------------------------------------------------------------
[numthreads(8, 8, 1)]
void CSMarch(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const AtmosphereFrame a = loadAtmosphere(f);
  const Rdr2Clouds c = loadRdr2(f);
  const uint64_t r = rdr2Record(f);
  const float4 info = vk::RawBufferLoad<float4>(r + kR2MarchInfo);
  const float4 steps = vk::RawBufferLoad<float4>(r + kR2Steps);
  const float2 uv = (float2(pixel) + 0.5) / float2(push.width, push.height);
  const float2 screen = uv * f.screenSize;
  const float3 dir = skyDirection(f, a, screen);
  float4 result = float4(0.0, 0.0, 0.0, 1.0);
  if (dir.y > 0.003) {
    const float3 clear = skyRadiance(a, dir) * a.skyScale;
    const float dither = rdr2BlueNoise(f, pixel, uint(info.z));
    const Rdr2Hit hit = rdr2March(f, a, c, f.cameraPosition, dir, clear, uint(steps.x), uint(steps.y), uint(steps.z), dither, uv);
    result = float4(hit.color, hit.transmittance);
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = result;
}

// --- The temporal pass -------------------------------------------------------------------------------------------------------
float4 rdr2MarchAt(Rdr2Clouds c, int2 at, int2 size) {
  const float2 uv = (float2(clamp(at, int2(0, 0), size - 1)) + 0.5) / float2(size);
  return gTextures[NonUniformResourceIndex(c.march)].SampleLevel(gSamplers[NonUniformResourceIndex(c.samplerClamp)], uv, 0);
}
float4 rdr2HistoryAt(Rdr2Clouds c, float2 uv) {
  return gTextures[NonUniformResourceIndex(c.history)].SampleLevel(gSamplers[NonUniformResourceIndex(c.samplerClamp)], uv, 0);
}

[numthreads(8, 8, 1)]
void CSResolve(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const AtmosphereFrame a = loadAtmosphere(f);
  const Rdr2Clouds c = loadRdr2(f);
  const uint64_t r = rdr2Record(f);
  const int2 size = int2(push.width, push.height);
  const float4 current = rdr2MarchAt(c, int2(pixel), size);
  // The colour box of the 3 x 3 neighbourhood of the new march (the history may not leave it: no trail behind a moving cloud).
  float4 lo = current, hi = current;
  [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x) {
      if (x == 0 && y == 0) continue;
      const float4 n = rdr2MarchAt(c, int2(pixel) + int2(x, y), size);
      lo = min(lo, n);
      hi = max(hi, n);
    }
  float4 result = current;
  const float4 screen = vk::RawBufferLoad<float4>(r + kR2Screen);
  if (screen.z > 0.5) {
    // Where this pixel's sky direction was on screen last frame (the clouds are far: a direction, not a point).
    const float2 uv = (float2(pixel) + 0.5) / float2(size);
    const float3 dir = skyDirection(f, a, uv * f.screenSize);
    const float4x4 previous = vk::RawBufferLoad<float4x4>(r + kR2PreviousViewProj);
    const float4 clip = mul(float4(f.cameraPosition + dir * 50000.0, 1.0), previous);
    if (clip.w > 0.0) {
      const float2 prevUv = clip.xy / clip.w * 0.5 + 0.5;
      if (all(prevUv >= 0.0) && all(prevUv <= 1.0)) {
        float4 history = rdr2HistoryAt(c, prevUv);
        // The box widened a little (the march's own dither moves the neighbourhood): a history inside it is kept whole.
        const float4 pad = (hi - lo) * 0.25 + 0.002;
        history = clamp(history, lo - pad, hi + pad);
        const float4 info = vk::RawBufferLoad<float4>(r + kR2MarchInfo);
        const float blend = info.w < 0.5 ? 0.12 : 0.08;  // Ultra converges faster (every pixel marched every frame)
        result = lerp(history, current, blend);
      }
    }
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = result;
}

// --- The shadow map over the board -------------------------------------------------------------------------------------------
[numthreads(8, 8, 1)]
void CSShadow(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const Rdr2Clouds c = loadRdr2(f);
  const uint64_t r = rdr2Record(f);
  const float4 rect = vk::RawBufferLoad<float4>(r + kR2Shadow);
  const float4 info = vk::RawBufferLoad<float4>(r + kR2ShadowInfo);
  const float2 xz = rect.xy + (float2(pixel) + 0.5) / float2(push.width, push.height) / rect.zw;
  const float3 sun = f.sunDirection;
  float value = 1.0;
  const float low = smoothstep(0.06, 0.2, sun.y);  // nothing once the sun is low (its shadows would stretch without end)
  if (low > 0.0) {
    const float depth = rdr2LightDepthFrom(c, float3(xz.x, info.z, xz.y), sun, uint(info.w), 0.5);
    value = 1.0 - info.y * low * (1.0 - exp(-depth));
  }
  gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel] = value;
}

float rdr2ShadowRead(Rdr2Clouds c, float2 uv) {
  return gTextures[NonUniformResourceIndex(push.source)].SampleLevel(gSamplers[NonUniformResourceIndex(c.samplerClamp)], uv, 0).r;
}
[numthreads(8, 8, 1)]
void CSShadowBlur(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const Rdr2Clouds c = loadRdr2(f);
  const float2 texel = 1.0 / float2(push.width, push.height);
  const float2 step = push.mode == 0u ? float2(texel.x, 0.0) : float2(0.0, texel.y);
  const float2 uv = (float2(pixel) + 0.5) * texel;
  static const float kWeights[5] = {0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162};
  float sum = rdr2ShadowRead(c, uv) * kWeights[0];
  [unroll] for (int i = 1; i < 5; ++i) {
    const float2 o = step * (float(i) * 1.6);
    sum += (rdr2ShadowRead(c, uv + o) + rdr2ShadowRead(c, uv - o)) * kWeights[i];
  }
  gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel] = sum;
}
