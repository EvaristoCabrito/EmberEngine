// Volumetric fog: R7's literal fog, world-space and lit (ARCHITECTURE §7.4b). Froxels: a camera-aligned grid of
// kFroxelX x kFroxelY screen tiles x kFroxelZ depth slices spaced logarithmically between volumeNear and
// volumeFar (Wronski 2014, "Volumetric Fog", Assassin's Creed IV; Hillaire 2015, "Physically Based and Unified
// Volumetric Rendering in Frostbite"). Our own implementation:
//   CSInject     one thread per froxel: the fog's density there (low and in valleys, dense along the edge of
//                never-seen ground, drifting noise), lit by the sun through its shadow maps, the sky, and the
//                local lights of the froxel's light cluster; blended with last frame's result reprojected
//                (the slice position is jittered every frame, so the history smooths banding away).
//   CSIntegrate  one thread per screen tile: front to back, energy-conserving (Hillaire), the light scattered
//                towards the camera and the transmittance up to each slice. Surfaces read it at their own
//                depth (lighting.hlsli applyVolume).
// The fog of war rules the light, never the other way round: never-seen froxels scatter nothing (they still
// absorb: black fog), remembered froxels get no local light and are darkened like remembered ground.
#include "lighting.hlsli"

// Matches ember::VolumeFrame (Renderer.cpp). 224 bytes.
struct VolumeFrame {
  float4x4 inverseViewProjection;
  float4x4 previousViewProjection;
  float3 wind;              // world units per second
  float time;
  float3 albedo;            // scattering / extinction
  float density;            // extinction per world unit at the ground
  float scaleHeight;        // density falls by e over this height above the ground
  float valley;             // extra density per world unit the ground lies below its surroundings
  float boundaryDensity;    // extra extinction along the edge of never-seen ground
  float boundaryHeight;     // ... falling by e over this height
  float noiseScale;         // drift noise frequency (per world unit)
  float anisotropy;         // Henyey-Greenstein g
  float historyWeight;      // share of last frame kept (0 = none)
  float jitter;             // this frame's slice offset, -0.5..0.5
  uint injected;            // storage volume written by CSInject (and read by CSIntegrate)
  uint history;             // sampled volume: last frame's CSInject result
  uint integrated;          // storage volume written by CSIntegrate
  uint historyValid;
  float meanGround;         // ground height off the board / where the mask has none
  uint3 pad;
};

struct Push {
  uint64_t frame;   // MeshFrame
  uint64_t volume;  // VolumeFrame
};
[[vk::push_constant]] Push push;

VolumeFrame loadVolume() { return vk::RawBufferLoad<VolumeFrame>(push.volume); }

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
float fbm(float3 p) { return valueNoise(p) * 0.6 + valueNoise(p * 2.03 + 17.1) * 0.3 + valueNoise(p * 4.01 + 41.7) * 0.1; }

// Extinction per world unit at p.
float fogDensity(MeshFrame f, VolumeFrame v, float3 p) {
  float4 field = float4(0.0, 0.0, v.meanGround, 0.0);  // mask darkness, boundary, ground, valley
  if (f.fogMask != kNoTexture) {
    const float2 uv = (p.xz - f.fogRect.xy) * f.fogRect.zw;
    if (all(uv >= 0.0) && all(uv <= 1.0))
      field = gTextures[NonUniformResourceIndex(f.fogMask)].SampleLevel(gSamplers[f.fogSampler], uv, 0);
  }
  const float h = max(p.y - field.z, 0.0);
  const float ground = v.density * exp(-h / v.scaleHeight) * (1.0 + v.valley * field.w);
  const float edge = v.boundaryDensity * 4.0 * field.y * (1.0 - field.y) * exp(-h / v.boundaryHeight);  // peaks at the edge
  const float n = fbm(p * v.noiseScale + v.wind * v.time);
  return (ground + edge) * lerp(0.25, 1.75, n);
}

// --- Light -----------------------------------------------------------------------------------------------

// The sun's shadow maps at a point in the air (one hardware-filtered tap each: the history softens it).
float airShadow(MeshFrame f, float3 p) {
  if ((f.lightingLayers & kLayerShadows) == 0 || f.shadowMap == kNoTexture) return 1.0;
  const float4 c = mul(float4(p, 1.0), f.shadowMatrix);
  const float3 uvz = float3(c.xy * 0.5 + 0.5, c.z);
  if (any(uvz.xy < 0.0) || any(uvz.xy > 1.0) || uvz.z > 1.0) return 1.0;
  SamplerComparisonState cmp = gComparisonSamplers[f.shadowSampler];
  float lit = gTextures[NonUniformResourceIndex(f.shadowMap)].SampleCmpLevelZero(cmp, uvz.xy, uvz.z);
  if (f.cardShadowMap != kNoTexture) lit = min(lit, gTextures[NonUniformResourceIndex(f.cardShadowMap)].SampleCmpLevelZero(cmp, uvz.xy, uvz.z));
  return lit;
}

// Local lights scattered towards the camera at p (the light cluster p falls in; Ember's falloff).
float3 airLocalLights(MeshFrame f, float3 p, float3 toCamera, float g) {
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
    color += light.color * f.exposure * falloff * phaseHG(dot(toLight / max(d, 1e-4), -toCamera), g);
  }
  return color;
}

// --- Froxels -----------------------------------------------------------------------------------------------

// World position of a froxel coordinate (uv across the screen, w along the slices).
float3 froxelWorld(MeshFrame f, VolumeFrame v, float2 uv, float w) {
  const float depth = froxelDepth(f, w);
  const float4 h = mul(float4(uv * 2.0 - 1.0, f.zNear / depth, 1.0), v.inverseViewProjection);  // reversed infinite Z
  return h.xyz / h.w;
}

[numthreads(8, 8, 1)]
void CSInject(uint3 id : SV_DispatchThreadID) {
  if (any(id >= uint3(kFroxelX, kFroxelY, kFroxelZ))) return;
  const MeshFrame f = loadFrame(push.frame);
  const VolumeFrame v = loadVolume();
  const float2 uv = (float2(id.xy) + 0.5) / float2(kFroxelX, kFroxelY);
  const float3 p = froxelWorld(f, v, uv, (float(id.z) + 0.5 + v.jitter) / float(kFroxelZ));
  const float3 toCamera = normalize(f.cameraPosition - p);

  const float extinction = fogDensity(f, v, p);
  const Sight sight = tacticalSight(f, p);
  float3 light = 0.0;
  if (sight.color > 0.0) {
    if (f.lightingLayers & kLayerSun)
      light += f.sunColor * f.sunIntensity * airShadow(f, p) * phaseHG(dot(f.sunDirection, -toCamera), v.anisotropy);
    if (f.lightingLayers & kLayerIndirect) light += (f.skyColor + f.groundColor) * 0.5 * f.ambientIntensity / kPi;  // uniform sky
    if (sight.local > 0.0) light += airLocalLights(f, p, toCamera, v.anisotropy) * sight.local;
    light *= sight.color;  // remembered air is as dark as remembered ground
  }
  float4 result = float4(light * v.albedo * extinction, extinction);

  // Last frame's result where this point was then (the jitter makes each frame sample a different depth).
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
  if (sight.color <= 0.0) result.rgb = 0.0;  // never seen scatters nothing, whatever the history held
  gStorageVolumes[NonUniformResourceIndex(v.injected)][id] = result;
}

[numthreads(8, 8, 1)]
void CSIntegrate(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= uint2(kFroxelX, kFroxelY))) return;
  const MeshFrame f = loadFrame(push.frame);
  const VolumeFrame v = loadVolume();
  const float2 uv = (float2(id.xy) + 0.5) / float2(kFroxelX, kFroxelY);
  // Ray length per unit of view depth along this tile's ray.
  const float stretch = length(froxelWorld(f, v, uv, 0.5) - f.cameraPosition) / froxelDepth(f, 0.5);
  float3 scattered = 0.0;
  float transmittance = 1.0;
  float previousDepth = 0.0;
  for (uint z = 0; z < kFroxelZ; ++z) {
    const float depth = froxelDepth(f, float(z + 1) / float(kFroxelZ));
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
