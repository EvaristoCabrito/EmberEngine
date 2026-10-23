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

// --- Nuvens volumétricas, look 001 (cloudsvol.hlsli marches and lights it; this is the density field) ----------------------
// A layer of air between two heights whose density is a 3D noise field: a Perlin-Worley base shaped by the sheets' own
// weather (so the basic and the volumetric clouds share their cover and wind), Worley detail eroding the edges (Schneider
// 2015, the Horizon Zero Dawn / FFXV recipe). MeshFrame.cloudFlags kCloudVolumetric says the layer is this one (the sheets
// are then not drawn); the numbers after the AtmosphereFrame's moonIlluminance are read by byte offset (the gotcha).
static const uint kCloudVolumetric = 4u;
// MeshFrame.cloudFlags kCloudSkyScaled: the clouds' own light on the sky's scale (AtmosphereFrame.skyScale) like the clear sky
// around them (the watchtower, 2026-10-22: its sky shown at the outdoor exposure, the clouds burned white without it).
static const uint kCloudSkyScaled = 8u;
static const uint kVolNoiseBaseOffset = 352u;    // uint: sampled 3D texture, the base noise (128^3: r Perlin-Worley, gba Worley)
static const uint kVolNoiseDetailOffset = 356u;  // uint: the detail noise (32^3: rgb Worley at three frequencies)
static const uint kVolMarchOffset = 360u;        // uint: the half-resolution march result (sampled: rgb radiance, a transmittance)
static const uint kVolSamplerOffset = 364u;      // uint: a linear, repeating sampler for the noise
static const uint kVolBaseOffset = 368u;         // float: the layer's bottom (world y)
static const uint kVolTopOffset = 372u;          // float: its top
static const uint kSunLookOffset = 376u;         // uint: the sun's look (0 as before, 1 = look 001: atmosphere.hlsli)
static const uint kVolMarchSizeOffset = 384u;    // float2: the march image's size in texels
static const float kCloudVolExtinction = 0.35;   // per world unit at density 1 (a unit is ~25 m of air: a cumulus is opaque within
                                                 // a few units, its bases dark; the edges glow through the density ramp)

struct CloudVolume {
  float base, top;
  float coverage;  // 0 .. 1
  float scale;     // world units per weather cell (one cloud)
  float2 wind;     // world units per second
  uint noiseBase, noiseDetail, sampler;
};
CloudVolume loadCloudVolume(MeshFrame f) {
  CloudVolume v;
  const float4 a = vk::RawBufferLoad<float4>(f.atmosphere + kCloudParamsOffset);
  v.coverage = a.x;
  v.scale = a.w;
  v.wind = vk::RawBufferLoad<float2>(f.atmosphere + kCloudParamsOffset + 16u);
  v.noiseBase = vk::RawBufferLoad<uint>(f.atmosphere + kVolNoiseBaseOffset);
  v.noiseDetail = vk::RawBufferLoad<uint>(f.atmosphere + kVolNoiseDetailOffset);
  v.sampler = vk::RawBufferLoad<uint>(f.atmosphere + kVolSamplerOffset);
  v.base = vk::RawBufferLoad<float>(f.atmosphere + kVolBaseOffset);
  v.top = vk::RawBufferLoad<float>(f.atmosphere + kVolTopOffset);
  return v;
}

float remap01(float x, float a, float b) { return saturate((x - a) / max(b - a, 1e-5)); }

// (the samples in their own helpers, the coordinate finished first: the DXC gotcha of volumeSampleAt)
float4 cloudNoiseBaseAt(CloudVolume v, float3 uvw) {
  return gTextures3D[NonUniformResourceIndex(v.noiseBase)].SampleLevel(gSamplers[NonUniformResourceIndex(v.sampler)], uvw, 0);
}
float4 cloudNoiseDetailAt(CloudVolume v, float3 uvw) {
  return gTextures3D[NonUniformResourceIndex(v.noiseDetail)].SampleLevel(gSamplers[NonUniformResourceIndex(v.sampler)], uvw, 0);
}

// The weather over a point of the sky: the sheets' own cover field as the column's coverage, and a cloud type (0 low and
// flat, stratus-like .. 1 tall cumulus) from a slow noise.
float2 cloudWeather(CloudVolume v, float time, float2 xz) {
  CloudParams c;
  c.coverage = v.coverage;
  c.scale = v.scale;
  c.wind = v.wind;
  c.altitude = v.base;
  c.shadow = 0.0;
  c.thickness = 1.0;
  // The weather clears with the distance from the world's origin (the boards stand near it): the far layer, which the
  // game's low camera sees across the horizon band, opens up instead of closing into a ceiling.
  const float cover = cloudCover(c, time, xz) * (1.0 - smoothstep(3500.0, 8500.0, length(xz)));
  const float kind = cloudNoise((xz + v.wind * time * 0.7) / (v.scale * 2.7) + 31.7);
  return float2(cover, kind);
}

// Where in its column the cloud lives (stratus low and flat, cumulus tall): a multiplier on the density by the relative
// height h (0 at the base .. 1 at the top).
float cloudHeightShape(float h, float kind) {
  const float lowTop = lerp(0.35, 0.95, kind);
  const float rise = smoothstep(0.0, 0.08 + 0.1 * kind, h);
  const float fall = 1.0 - smoothstep(lowTop - 0.25, lowTop, h);
  return rise * fall;
}

// The density at a world point, 0 .. 1. `cheap` = no detail erosion (the light march, the shadows). `lod` (0 near .. 1 far):
// the fine erosion fades out by 0.4 and the coarse one by 1 (far clouds sampled at long steps otherwise alias into stripes
// and grain: the "curtains" near the horizon).
float cloudVolDensityLod(CloudVolume v, float time, float3 p, bool cheap, float lod) {
  const float h = saturate((p.y - v.base) / max(v.top - v.base, 1.0));
  if (h <= 0.0 || h >= 1.0) return 0.0;
  const float2 weather = cloudWeather(v, time, p.xz);
  const float cover = weather.x;
  if (cover <= 0.005) return 0.0;
  const float shape = cloudHeightShape(h, weather.y);
  if (shape <= 0.0) return 0.0;
  // Base shape: Perlin-Worley (r) with lower-frequency Worley (gba) as a fractal lift, drifting with the wind and leaning a
  // little with height (wind shear).
  const float cell = v.scale * 0.9;  // one base-noise tile = about one cloud
  const float3 drift = float3(v.wind.x * time + h * 5.0, 0.0, v.wind.y * time);
  const float3 uvw = (p + drift) / cell;
  const float4 base = cloudNoiseBaseAt(v, uvw);
  const float lowFbm = base.g * 0.625 + base.b * 0.25 + base.a * 0.125;
  float density = remap01(base.r, -(1.0 - lowFbm), 1.0) * shape;
  // The weather carves the cloud: the column's cover decides how much of the noise survives (Schneider's remap).
  density = remap01(density, 1.0 - cover, 1.0) * cover;
  if (density <= 0.001) return 0.0;
  // The cheap density (the light march, the shadows) is eroded by the detail's average, so the light sees a cloud of the
  // same size as the one drawn (un-eroded it was fatter and every surface read as deep inside: grey clouds, 2026-10-19).
  if (cheap) return remap01(density, 0.28, 1.0);
  if (lod < 1.0) {
    // Detail: Worley fbm erodes the edges (wispy at the base, billowy towards the top), then a finer octave carves the
    // cauliflower's small bulges into the edge that survived (Horizon's two-level erosion). The tiles are large (68 and 45
    // units: a 17-unit tile repeated up a 260-unit cloud showed as horizontal stripes, 2026-10-19).
    const float4 d = cloudNoiseDetailAt(v, (p + drift * 1.3) / (cell * 0.18));
    const float detailFbm = d.r * 0.625 + d.g * 0.25 + d.b * 0.125;
    const float erosion = lerp(detailFbm, 1.0 - detailFbm, saturate(h * 6.0));
    density = remap01(density, erosion * 0.5 * (1.0 - lod), 1.0);
    const float fineWeight = 1.0 - saturate(lod / 0.4);
    if (fineWeight > 0.0 && density > 0.001 && density < 0.6) {
      const float4 d2 = cloudNoiseDetailAt(v, (p + drift * 1.7) / (cell * 0.12));
      const float fine = d2.r * 0.625 + d2.g * 0.25 + d2.b * 0.125;
      density = remap01(density, fine * 0.25 * (1.0 - density) * fineWeight, 1.0);
    }
  }
  return saturate(density);
}
float cloudVolDensity(CloudVolume v, float time, float3 p, bool cheap) { return cloudVolDensityLod(v, time, p, cheap, 0.0); }

// The sun through the volumetric layer onto `position`: the density along the light's path through the layer, 8 taps.
float cloudVolShadow(MeshFrame f, float3 position, float low) {
  const float3 sun = f.sunDirection;
  const CloudVolume v = loadCloudVolume(f);
  const float shadow = vk::RawBufferLoad<float4>(f.atmosphere + kCloudParamsOffset).z;
  const float t0 = (v.base - position.y) / max(sun.y, 0.06), t1 = (v.top - position.y) / max(sun.y, 0.06);
  if (t1 <= t0) return 1.0;
  const float dt = (t1 - t0) / 8.0;
  float depth = 0.0;
  [unroll] for (int i = 0; i < 8; ++i) depth += cloudVolDensity(v, f.time, position + sun * (t0 + dt * (float(i) + 0.5)), true) * dt;
  return 1.0 - shadow * low * (1.0 - exp(-depth * kCloudVolExtinction));
}

// The sun through the low sheet onto `position`, computed from the cover field: 1 lit .. (1 - shadow) under a thick cloud.
// Nothing at night (the key light is then the moon: no flag) and nothing once the sun is low (its shadows would stretch
// without end).
float cloudShadowCompute(MeshFrame f, float3 position) {
  const float3 sun = f.sunDirection;
  const float low = smoothstep(0.06, 0.2, sun.y);
  if (low <= 0.0) return 1.0;
  if (f.cloudFlags & kCloudVolumetric) return cloudVolShadow(f, position, low);
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
