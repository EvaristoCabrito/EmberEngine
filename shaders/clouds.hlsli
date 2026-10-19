// Nuvens básicas (2026-10-19; the user: "faz uma nuvem básica para ver, com OFF", then "tem que ser boa"): cloud sheets
// high over the board, their cover a drifting fractal noise. The same cover field is read three times, so the sky, the
// reflections on water and the shadows on the board all show the same clouds:
//   cloudCover()   a sheet's cover at a point of it (0 clear .. 1 thick)
//   cloudShadow()  the sun through the low sheet onto a surface (lighting.hlsli: multiplies the sun's term), by day only
//   cloudSheet() / cloudsOverSky() (atmosphere.hlsli, need the sky tables) the lit sheets over the sky along a ray
// MeshFrame.cloudFlags: kCloudLayer (the sheets exist), kCloudShadows (the low one shadows the board). The numbers live in
// the AtmosphereFrame's cloud* fields and are read here by their byte offsets, one by one: passed inside the
// AtmosphereFrame by value, DXC read them as 0 (the moonTexture gotcha of 2026-10-15, CLAUDE.md: fields after its uint64).
// OFF = both flags clear: the image is exactly what it was without this file.
#ifndef EMBER_CLOUDS_HLSLI
#define EMBER_CLOUDS_HLSLI

#include "scene.hlsli"

// Debug view 20 (--debug-view 20): the sun through the clouds on every surface (white = clear, dark = under a cloud), and the
// low sheet's cover in the sky.
static const uint kDebugClouds = 20u;

struct CloudParams {
  float coverage;   // share of the sky covered (0 clear .. 1 overcast)
  float altitude;   // the sheet's world height
  float shadow;     // how dark its shadow gets on the board (0 none .. 1 full)
  float scale;      // world units per noise cell (the size of one cloud)
  float2 wind;      // drift, world units per second
  float thickness;  // optical depth of a full cover, seen straight up
};
// AtmosphereFrame: cloudCoverage at byte 276 (after starLook), cloudWind at 292 (scene.hlsli / render/Atmosphere.h).
static const uint kCloudParamsOffset = 276u;
CloudParams loadCloudParams(MeshFrame f) {
  const float4 a = vk::RawBufferLoad<float4>(f.atmosphere + kCloudParamsOffset);
  const float2 w = vk::RawBufferLoad<float2>(f.atmosphere + kCloudParamsOffset + 16u);
  CloudParams c;
  c.coverage = a.x;
  c.altitude = a.y;
  c.shadow = a.z;
  c.scale = a.w;
  c.wind = w;
  c.thickness = 3.2;
  return c;
}
// The high sheet (cirrus): finer, thinner, faster, sparser, 1.6 x as high.
CloudParams highSheet(CloudParams c) {
  c.scale *= 2.6;
  c.coverage *= 0.6;
  c.wind = c.wind * 1.8 + float2(1.3, -0.7);
  c.thickness = 0.9;
  c.altitude *= 1.6;
  return c;
}

float cloudHash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float cloudNoise(float2 p) {
  const float2 cell = floor(p), fr = frac(p);
  const float2 u = fr * fr * (3.0 - 2.0 * fr);
  return lerp(lerp(cloudHash(cell), cloudHash(cell + float2(1.0, 0.0)), u.x),
              lerp(cloudHash(cell + float2(0.0, 1.0)), cloudHash(cell + float2(1.0, 1.0)), u.x), u.y);
}
// Four octaves of value noise (0..1, most mass near 0.5), the domain bent by a slow second noise so the shapes are not
// the lattice's.
float cloudFbm(float2 p) {
  p += (float2(cloudNoise(p * 0.37 + 11.3), cloudNoise(p * 0.41 + 57.1)) - 0.5) * 1.6;
  float n = cloudNoise(p) * 0.5;
  n += cloudNoise(p * 2.03 + 19.7) * 0.25;
  n += cloudNoise(p * 4.11 + 7.2) * 0.125;
  n += cloudNoise(p * 8.27 + 3.9) * 0.0625;
  return n / 0.9375;
}

// The sheet's cover at a point `xz` of it: 0 clear .. 1 thick.
float cloudCover(CloudParams c, float time, float2 xz) {
  const float2 p = (xz + c.wind * time) / max(c.scale, 1.0);
  const float n = saturate((cloudFbm(p) - 0.2) / 0.6);
  const float threshold = 1.0 - saturate(c.coverage);
  return smoothstep(threshold - 0.12, threshold + 0.22, n);
}

// The cover with its edges eroded by a finer noise (where the cover is low), so puffs break up instead of blurring: what
// the sky, the reflections and the shadows all read (the fine structure is board-sized: a shadow's edge has shape).
float cloudCoverEroded(CloudParams c, float time, float2 xz) {
  const float cover = cloudCover(c, time, xz);
  if (cover <= 0.001) return 0.0;
  const float2 p = (xz + c.wind * time * 1.15) / max(c.scale, 1.0);
  const float detail = cloudNoise(p * 6.3 + 3.1) * 0.6 + cloudNoise(p * 13.7 - 5.2) * 0.4;
  return saturate(cover - (1.0 - cover) * detail * 0.55);
}

// The sun through the low sheet onto `position`, computed from the cover field: 1 lit .. (1 - shadow) under a thick cloud.
// Nothing at night (the key light is then the moon: no flag) and nothing once the sun is low (its shadows would stretch
// without end).
float cloudShadowCompute(MeshFrame f, float3 position) {
  const float3 sun = f.sunDirection;
  const float low = smoothstep(0.06, 0.2, sun.y);
  if (low <= 0.0) return 1.0;
  const CloudParams c = loadCloudParams(f);
  const float h = max(c.altitude - position.y, 1.0);
  const float2 at = position.xz + sun.xz * (h / max(sun.y, 0.06));
  const float cover = cloudCoverEroded(c, f.time, at);
  return 1.0 - c.shadow * low * (1.0 - exp(-cover * c.thickness));
}
// (the sample in its own helper, the coordinate finished first: the DXC gotcha of volumeSampleAt)
float cloudShadowSample(uint map, uint sampler, float2 uv) {
  return gTextures[NonUniformResourceIndex(map)].SampleLevel(gSamplers[NonUniformResourceIndex(sampler)], uv, 0).r;
}
// The sun through the low sheet onto `position`: the shadow map over the board (one texel read; skyreflect.hlsl
// CSCloudShadow fills it once a frame with cloudShadowCompute at the board's height), the cover computed here off the map.
// AtmosphereFrame: cloudShadowRect at byte 304, cloudShadowMap at 320 (read by offset: the moonTexture gotcha).
float cloudShadow(MeshFrame f, float3 position) {
  if ((f.cloudFlags & kCloudShadows) == 0 || f.atmosphere == 0) return 1.0;
  const uint map = vk::RawBufferLoad<uint>(f.atmosphere + 320u);
  if (map == kNoTexture) return cloudShadowCompute(f, position);
  const float4 rect = vk::RawBufferLoad<float4>(f.atmosphere + 304u);
  float2 uv = (position.xz - rect.xy) * rect.zw;
  if (any(uv < 0.0) || any(uv > 1.0)) return cloudShadowCompute(f, position);
  uv = saturate(uv);
  return cloudShadowSample(map, f.volumeSampler, uv);  // linear, clamped (set every frame)
}

#endif
