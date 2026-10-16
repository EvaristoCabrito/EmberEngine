// Atmosfera volumétrica v1 (docs/ATMOSPHERE.md): the air of the visible world as a lit, three-dimensional medium.
// Froxels (Wronski 2014; Hillaire 2015 "Physically Based and Unified Volumetric Rendering in Frostbite"; the same
// family as Unreal's Volumetric Fog): a camera-aligned grid of screen tiles x logarithmic depth slices between the
// camera's near plane and the profile's reach. Our own implementation, versioned: this file is v1 and is never
// edited once a later version exists (a change is a new file).
//   CSInject     one thread per froxel: extinction there (Koschmieder visibility -> sigma_t, exponential height
//                falloff over the ground, drifting fbm noise, valleys, local fog volumes, the never-seen bank of the
//                fog of war), lit by the sun through its shadow maps (soft: several taps), the sky (the SH ambient's
//                isotropic term) and the local lights of its light cluster, each local light occluded by the relief
//                on its way; blended with last frame's result reprojected (slice jitter + history = temporal
//                integration). The fog of war rules the light, never the other way round.
//   CSIntegrate  one thread per screen tile: front to back, energy-conserving (Hillaire), the light scattered
//                towards the camera and the transmittance up to each slice. Surfaces read it at their own depth
//                (lighting.hlsli applyVolume: depth-exact composition, no halos); effects take the transmittance;
//                the sky / backdrop take the last slice.
#include "atmosphere.hlsli"

// Matches ember::VolumetricsFrame (render/Volumetrics.cpp). 272 bytes, then kMaxVolumes x LocalVolume (32 bytes
// each) at offset 272, read by address (no 64-bit field inside the record: DXC reads those wrong by value).
struct VolumetricsFrame {
  float4x4 inverseViewProjection;
  float4x4 previousViewProjection;
  uint3 grid;               // froxels: tiles across, down, slices
  float jitter;             // this frame's slice offset, -0.5..0.5
  float3 wind;              // world units per second
  float time;
  float3 albedo;            // scattering / extinction (the fog's colour)
  float extinction;         // sigma_t per world unit at ground level (3.912 / visibility)
  float scaleHeight;        // density falls by e over this height above the ground
  float valley;             // extra density per unit the ground lies below its surroundings
  float noiseScale;         // fbm frequency (per world unit)
  float noiseStrength;      // 0 uniform .. 1 fully broken up
  float anisotropy;         // Henyey-Greenstein g of the forward lobe
  float backLobe;           // share of a backward lobe (g = -0.3): the side away from the sun is not flat
  float sunScale;           // multipliers on each light's in-scatter
  float skyScale;
  float localScale;
  float historyWeight;      // share of last frame kept (0 = none)
  float meanGround;         // ground height off the board / where the mask has none
  float bankDensity;        // the dense fog over never-seen ground (fog of war): sigma_t at its surface
  float bankHeight;         // ... falling by e over this height
  uint sunTaps;             // shadow-map taps per froxel (1 hard, 4 soft)
  uint occlusionTaps;       // relief samples between a froxel and each local light (0 none)
  uint historyValid;
  uint injected;            // storage volume written by CSInject (and read by CSIntegrate)
  uint history;             // sampled volume: last frame's CSInject result
  uint integrated;          // storage volume written by CSIntegrate
  uint volumeCount;         // local fog volumes in use
  float nearFade;           // no fog within this distance of the eye (readability), fading in over as much again
  float shadowSoftness;     // the sun taps' disc radius, world units
  float2 pad;
};
struct LocalVolume {
  float3 center;
  float density;    // sigma_t added inside
  float3 halfExtent;
  float feather;    // world units over which it fades out past its box
};
static const uint kFrameBytes = 272;
static const uint kMaxVolumes = 16;

struct Push {
  uint64_t frame;        // MeshFrame
  uint64_t volumetrics;  // VolumetricsFrame
};
[[vk::push_constant]] Push push;

VolumetricsFrame loadVolumetrics() { return vk::RawBufferLoad<VolumetricsFrame>(push.volumetrics); }
LocalVolume loadVolume(uint i) { return vk::RawBufferLoad<LocalVolume>(push.volumetrics + kFrameBytes + i * 32); }

// --- Density -------------------------------------------------------------------------------------------

float hash3(float3 p) {
  p = frac(p * 0.3183099 + 0.1);
  p *= 17.0;
  return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float valueNoise(float3 x) {
  const float3 i = floor(x), f = frac(x), u = f * f * (3.0 - 2.0 * f);
  return lerp(lerp(lerp(hash3(i), hash3(i + float3(1, 0, 0)), u.x), lerp(hash3(i + float3(0, 1, 0)), hash3(i + float3(1, 1, 0)), u.x), u.y),
              lerp(lerp(hash3(i + float3(0, 0, 1)), hash3(i + float3(1, 0, 1)), u.x), lerp(hash3(i + float3(0, 1, 1)), hash3(i + float3(1, 1, 1)), u.x), u.y),
              u.z);
}
// Three octaves, the second and third drifting at their own speeds so the fog changes shape as it moves. Each octave
// is filtered to the froxel: `slab` is the slice's length along the ray in noise units (world length x frequency);
// an octave whose period is shorter than the slab averages out to its mean (0.5) instead of being point-sampled
// (point samples at the jittered depth differ from frame to frame and never settle: the far slabs boiled).
float fbm(float3 p, float3 wind, float time, float slab) {
  const float a0 = saturate(1.0 - 0.5 * slab), a1 = saturate(1.0 - 0.5 * slab * 2.07), a2 = saturate(1.0 - 0.5 * slab * 4.13);
  return lerp(0.5, valueNoise(p + wind * time), a0) * 0.55 + lerp(0.5, valueNoise(p * 2.07 + 17.1 - wind * time * 0.6), a1) * 0.3 +
         lerp(0.5, valueNoise(p * 4.13 + 41.7 + wind * time * 1.7), a2) * 0.15;
}

// The fog of war's fields at p.xz: darkness, boundary of the never-seen, ground height, valley depth.
float4 maskField(MeshFrame f, VolumetricsFrame v, float3 p) {
  float4 field = float4(0.0, 0.0, v.meanGround, 0.0);
  if (f.fogMask != kNoTexture) {
    const float2 uv = (p.xz - f.fogRect.xy) * f.fogRect.zw;
    if (all(uv >= 0.0) && all(uv <= 1.0)) field = gTextures[NonUniformResourceIndex(f.fogMask)].SampleLevel(gSamplers[f.fogSampler], uv, 0);
    else field.y = 1.0;  // off the board: never seen
  }
  return field;
}

// Extinction (sigma_t) per world unit at p: x = the air (lit by the fog of war's rules), y = the never-seen bank
// (lit by sun and sky only: it reads the mask alone, so its light reveals nothing).
float2 extinctionAt(MeshFrame f, VolumetricsFrame v, float3 p, float4 field, float slabLength) {
  const float h = max(p.y - field.z, 0.0);
  const float n = fbm(p * v.noiseScale, v.wind * v.noiseScale, v.time, slabLength * v.noiseScale);
  const float variation = lerp(1.0 - v.noiseStrength, 1.0 + v.noiseStrength, n);
  float sigma = v.extinction * exp(-h / v.scaleHeight) * (1.0 + v.valley * field.w) * variation;
  // Local volumes: caves, clearings, corridors, mist over water (boxes with a feathered edge).
  for (uint i = 0; i < v.volumeCount; ++i) {
    const LocalVolume lv = loadVolume(i);
    const float3 d = max(abs(p - lv.center) - lv.halfExtent, 0.0);
    const float w = 1.0 - smoothstep(0.0, max(lv.feather, 1e-3), length(d));
    sigma += lv.density * w * lerp(0.7, 1.3, n);
  }
  // The fog of war's bank: dense over never-seen ground (reads only the mask: never the ground's shape).
  const float bank = v.bankDensity * field.y * exp(-h / v.bankHeight) * lerp(0.6, 1.4, n);
  // Nothing right at the eye (units stay readable when the camera is close to them).
  const float eye = smoothstep(v.nearFade, v.nearFade * 2.0, length(p - f.cameraPosition));
  return max(float2(sigma, bank) * eye, 0.0);
}

// --- Light -----------------------------------------------------------------------------------------------

// Two-lobe Henyey-Greenstein: forward glow towards the light, a little back-scatter so the far side is not flat.
float phase2(float cosTheta, float g, float back) { return lerp(phaseHG(cosTheta, g), phaseHG(cosTheta, -0.3), back); }

// The sun's shadow maps at a point in the air: one hardware-filtered tap, or a few on a disc across the sun (soft
// shafts).
float airShadowTap(MeshFrame f, float3 p) {
  const float4 c = mul(float4(p, 1.0), f.shadowMatrix);
  const float3 uvz = float3(c.xy * 0.5 + 0.5, c.z);
  if (any(uvz.xy < 0.0) || any(uvz.xy > 1.0) || uvz.z > 1.0) return 1.0;
  SamplerComparisonState cmp = gComparisonSamplers[f.shadowSampler];
  float lit = gTextures[NonUniformResourceIndex(f.shadowMap)].SampleCmpLevelZero(cmp, uvz.xy, uvz.z);
  if (f.cardShadowMap != kNoTexture) lit = min(lit, gTextures[NonUniformResourceIndex(f.cardShadowMap)].SampleCmpLevelZero(cmp, uvz.xy, uvz.z));
  return lit;
}
float airShadow(MeshFrame f, VolumetricsFrame v, float3 p) {
  if ((f.lightingLayers & kLayerShadows) == 0 || f.shadowMap == kNoTexture) return 1.0;
  if (v.sunTaps <= 1) return airShadowTap(f, p);
  const float3 s = f.sunDirection;
  const float3 a = normalize(cross(s, abs(s.y) < 0.9 ? float3(0, 1, 0) : float3(1, 0, 0))), b = cross(s, a);
  const float2 taps[4] = {float2(0.7, 0.3), float2(-0.3, 0.7), float2(-0.7, -0.3), float2(0.3, -0.7)};
  float lit = 0.0;
  [unroll] for (uint k = 0; k < 4; ++k) lit += airShadowTap(f, p + (a * taps[k].x + b * taps[k].y) * v.shadowSoftness);
  return lit * 0.25;
}

// How much of a local light reaches p through the relief: samples along the way, each hidden when it lies under the
// ground the fog of war's mask records there (a lamp behind a bank does not light the fog on this side).
float reliefVisibility(MeshFrame f, VolumetricsFrame v, float3 p, float3 lightPos) {
  if (v.occlusionTaps == 0 || f.fogMask == kNoTexture) return 1.0;
  float hidden = 0.0;
  for (uint k = 1; k <= v.occlusionTaps; ++k) {
    const float3 q = lerp(p, lightPos, float(k) / float(v.occlusionTaps + 1));
    const float2 uv = (q.xz - f.fogRect.xy) * f.fogRect.zw;
    if (any(uv < 0.0) || any(uv > 1.0)) continue;
    const float ground = gTextures[NonUniformResourceIndex(f.fogMask)].SampleLevel(gSamplers[f.fogSampler], uv, 0).z;
    hidden += smoothstep(0.0, 0.6, ground - q.y);
  }
  return 1.0 - saturate(hidden / float(v.occlusionTaps));
}

// Local lights scattered towards the camera at p (the light cluster p falls in; the lights' own falloff).
float3 airLocalLights(MeshFrame f, VolumetricsFrame v, float3 p, float3 toCamera) {
  if ((f.lightingLayers & kLayerLocal) == 0 || f.lightCount == 0) return 0.0;
  const float4 clip = mul(float4(p, 1.0), f.viewProjection);
  if (clip.w <= 0.0) return 0.0;
  const float2 ndc = clip.xy / clip.w;
  const uint2 tile = uint2(clamp((ndc * 0.5 + 0.5) * float2(kClusterX, kClusterY), 0.0, float2(kClusterX, kClusterY) - 1.0));
  const float depth = -mul(float4(p, 1.0), f.view).z;
  const uint cluster = tile.x + tile.y * kClusterX + clusterSlice(f, depth) * kClusterX * kClusterY;
  const uint64_t list = f.clusters + clusterOffset(cluster);
  const uint count = vk::RawBufferLoad<uint>(list);
  float3 color = 0.0;
  for (uint k = 0; k < count; ++k) {
    const LocalLight light = loadLight(f, vk::RawBufferLoad<uint>(list + 4 + k * 4));
    const float3 toLight = light.position - p;
    const float d = length(toLight);
    if (d >= light.range) continue;
    const float ratio = d / light.range;
    const float window = saturate(1.0 - ratio * ratio * ratio * ratio);
    const float falloff = window * window / max(pow(d, light.decay), 0.01);
    const float visible = reliefVisibility(f, v, p, light.position);
    if (visible <= 0.0) continue;
    color += light.color * f.exposure * falloff * visible * phase2(dot(toLight / max(d, 1e-4), -toCamera), v.anisotropy, v.backLobe);
  }
  return color;
}

// The sky's light scattered isotropically: E / pi, with E the mean irradiance (the SH ambient's first term, or the
// hemisphere's average), exposure already in.
float3 skyInScatter(MeshFrame f) {
  if ((f.lightingLayers & kLayerIndirect) == 0) return 0.0;
  if (f.atmosphere != 0) {
    const uint64_t sh = loadAtmosphere(f).ambient;
    return max(vk::RawBufferLoad<float4>(sh).rgb * 0.282095, 0.0) / kPi;
  }
  return (f.skyColor + f.groundColor) * 0.5 * f.ambientIntensity / kPi;
}

// --- Froxels -----------------------------------------------------------------------------------------------

float3 froxelWorld(MeshFrame f, VolumetricsFrame v, float2 uv, float w) {
  const float depth = froxelDepth(f, w);
  const float4 h = mul(float4(uv * 2.0 - 1.0, f.zNear / depth, 1.0), v.inverseViewProjection);  // reversed infinite Z
  return h.xyz / h.w;
}

[numthreads(8, 8, 1)]
void CSInject(uint3 id : SV_DispatchThreadID) {
  const VolumetricsFrame v = loadVolumetrics();
  if (any(id >= v.grid)) return;
  const MeshFrame f = loadFrame(push.frame);
  const float2 uv = (float2(id.xy) + 0.5) / float2(v.grid.xy);
  // Everything is read at the slice's fixed centre: no depth jitter. Jitter + an exponential history (Wronski /
  // Frostbite) keeps a residual boil of (1 - weight) x the slab's spread for ever, and the user sees it ("a fumaça
  // treme sem parar", 2026-10-16); without jitter a still scene gives identical frames, and the slices (64-96, read
  // trilinearly) are fine enough that no banding shows. v.jitter is kept in the record (always 0 now) for a later look.
  const float3 p = froxelWorld(f, v, uv, (float(id.z) + 0.5) / float(v.grid.z));
  const float3 pLight = p;
  const float3 toCamera = normalize(f.cameraPosition - pLight);

  // This slice's length along the ray (the froxel's depth extent), for the noise filter.
  const float slabLength = (froxelDepth(f, float(id.z + 1) / float(v.grid.z)) - froxelDepth(f, float(id.z) / float(v.grid.z))) *
                           length(p - f.cameraPosition) / max(froxelDepth(f, (float(id.z) + 0.5) / float(v.grid.z)), 1e-3);
  const float4 field = maskField(f, v, p);
  const float2 sigma = extinctionAt(f, v, p, field, slabLength);  // air, never-seen bank
  const Sight sight = tacticalSight(f, pLight);
  float3 sunSky = 0.0;
  if (any(sigma > 0.0)) {
    if (f.lightingLayers & kLayerSun)
      sunSky += f.sunColor * f.sunIntensity * v.sunScale * airShadow(f, v, pLight) * phase2(dot(f.sunDirection, -toCamera), v.anisotropy, v.backLobe);
    sunSky += skyInScatter(f) * v.skyScale;
  }
  float3 light = 0.0;
  if (sight.color > 0.0 && sigma.x > 0.0) {
    light = sunSky;
    if (sight.local > 0.0) light += airLocalLights(f, v, pLight, toCamera) * v.localScale * sight.local;
    light *= sight.color;  // remembered air is as dark as remembered ground
  }
  float4 result = float4((light * sigma.x + sunSky * sigma.y) * v.albedo, sigma.x + sigma.y);

  // Last frame's result where this point was then (the jitter makes each frame sample a different depth, so the
  // history integrates the slices over time).
  if (v.historyValid != 0 && v.historyWeight > 0.0) {
    const float4 c = mul(float4(p, 1.0), v.previousViewProjection);
    if (c.w > 0.0) {
      const float3 uvw = float3(c.xy / c.w * 0.5 + 0.5, froxelSlice(f, c.w));
      if (all(uvw >= 0.0) && all(uvw <= 1.0)) {
        const float4 history = gTextures3D[NonUniformResourceIndex(v.history)].SampleLevel(gSamplers[f.volumeSampler], uvw, 0);
        result = lerp(result, history, v.historyWeight);
      }
    }
  }
  // Never-seen air scatters nothing, whatever the history held (the bank's own light stays: it reads the mask alone).
  if (sight.color <= 0.0) result.rgb = min(result.rgb, sunSky * sigma.y * v.albedo);
  gStorageVolumes[NonUniformResourceIndex(v.injected)][id] = result;
}

[numthreads(8, 8, 1)]
void CSIntegrate(uint3 id : SV_DispatchThreadID) {
  const VolumetricsFrame v = loadVolumetrics();
  if (any(id.xy >= v.grid.xy)) return;
  const MeshFrame f = loadFrame(push.frame);
  const float2 uv = (float2(id.xy) + 0.5) / float2(v.grid.xy);
  // Ray length per unit of view depth along this tile's ray.
  const float stretch = length(froxelWorld(f, v, uv, 0.5) - f.cameraPosition) / froxelDepth(f, 0.5);
  float3 scattered = 0.0;
  float transmittance = 1.0;
  float previousDepth = 0.0;
  for (uint z = 0; z < v.grid.z; ++z) {
    const float depth = froxelDepth(f, float(z + 1) / float(v.grid.z));
    const float step = (depth - previousDepth) * stretch;
    previousDepth = depth;
    const float4 s = gStorageVolumes[NonUniformResourceIndex(v.injected)][uint3(id.xy, z)];
    const float extinction = max(s.a, 1e-6);
    const float slab = exp(-extinction * step);
    // Hillaire 2015: the scattered light integrated analytically over the slab.
    scattered += transmittance * (s.rgb - s.rgb * slab) / extinction;
    transmittance *= slab;
    gStorageVolumes[NonUniformResourceIndex(v.integrated)][uint3(id.xy, z)] = float4(scattered, transmittance);
  }
}
