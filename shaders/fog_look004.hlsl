// FOG LOOK 004 (2026-10-14): Ember's volumetric mist model, new candidate, not yet seen by the user. Frozen
// once seen: never edit a saved look; new looks go in new files (shaders/fog_lookNNN.hlsl). Only the user
// deletes looks.
//
// The fog of war's mist (R7, the user, 2026-10-14): believable volumetric ground mist, densest over never-seen
// (black) ground, thinner over remembered ground, none in sight; soft everywhere, no edges, and the black may
// show through. Its model is Ember's volumetric mist (src/render/volumetricFog.ts, the "fog1"
// preset in src/game/mist3d.ts); the code is ours:
//   - a layer `thickness` deep over the local ground, densest at the ground, thicker in the low places (it
//     pools), thinning with height (heightPow);
//   - broken into wisps by drifting value-noise fBm (one baked white-noise lattice, smoothly filtered),
//     smoothstep(contrast) with a floor of even haze, stretched by `scale` per axis;
//   - lit mostly by the sky (grey: the light is desaturated), a little by the sun or moon with a faint forward
//     glow, and by local lights as soft, clamped halos; no self-shadowing;
//   - fading out past the board, so it draws no outline.
// The fog of war rules it: coverage comes only from the mask (never-seen ground blurred so it fades softly
// across the boundary; remembered ground at rememberedCover), the ground it follows under never-seen hexes is
// the board's mean (it reveals no shape), and only lights in sight make halos.
//   CSMarch      half resolution: one ray per pixel through the mist's slab, up to the surface behind.
//   CSComposite  full resolution: upsampled by depth likeness (no halos at silhouettes), over the scene colour.
#include "lighting.hlsli"

// Matches ember::FogLook004Frame (FogLooks.cpp). 240 bytes.
struct CloudFrame {
  float4x4 inverseViewProjection;
  float4 board;            // x0, z0, x1, z1: the mist fades out past it
  float time;
  float speed;             // drift speed multiplier
  float density;           // extinction per world unit at the densest point
  float thickness;         // layer depth over the local ground
  float base;              // the slab: bottom (lowest ground)
  float top;               // ... and top (highest ground + thickness)
  float pool;              // extra density in low ground (0 = none)
  float heightPow;         // 1 = thins linearly with height, 2+ hugs the ground
  float3 scale;            // noise frequency per axis (world units^-1)
  float floorShare;        // share of density present where the noise is empty (even haze)
  float3 wind;             // noise drift per second
  float reach;             // how far past the board it still reaches
  float2 contrast;         // noise -> wisps: smoothstep(x, y)
  float2 groundRange;      // lowest, highest ground (pooling)
  float3 albedo;
  float rememberedCover;   // density over remembered ground, relative to never-seen
  float ambientShare;      // of the sky light
  float sunShare;          // of the sun or moon
  float lightShare;        // of each local light (halos)
  float lightReach;        // of each local light's range
  uint noise;              // sampled 3D white-noise lattice (R8, tiling)
  uint depth;              // sampled scene depth (reversed Z)
  uint march;              // storage: half-resolution result (glow, transmittance)
  uint marchSampled;
  uint color;              // storage: scene colour (rgba16f)
  uint frameIndex;
  uint2 halfSize;
  uint2 fullSize;
  uint2 pad;
};

struct Push {
  uint64_t frame;  // MeshFrame
  uint64_t cloud;  // CloudFrame
};
[[vk::push_constant]] Push push;

CloudFrame loadCloud() { return vk::RawBufferLoad<CloudFrame>(push.cloud); }

static const float kNoiseSize = 64.0;

// Smooth value noise from the baked lattice: one filtered fetch, its position bent by smoothstep so it
// interpolates as smoothly as per-corner hashing.
float noise(MeshFrame f, CloudFrame c, float3 x) {
  const float3 p = x + 1.0, i = floor(p);
  float3 t = p - i;
  t = t * t * (3.0 - 2.0 * t);
  return gTextures3D[NonUniformResourceIndex(c.noise)].SampleLevel(gSamplers[f.materialSampler], (i + t - 0.5) / kNoiseSize, 0).r;
}
float fbm(MeshFrame f, CloudFrame c, float3 p) {
  return noise(f, c, p) * 0.55 + noise(f, c, p * 2.03 + 7.1) * 0.3 + noise(f, c, p * 4.1 + 3.7) * 0.15;
}

// How much mist the fog of war allows over a point, and the ground it lies on.
float2 coverage(MeshFrame f, CloudFrame c, float2 xz) {
  const float2 uv = (xz - f.fogRect.xy) * f.fogRect.zw;
  float k = 1.0, ground = c.groundRange.x;  // off the board: never seen
  if (all(uv >= 0.0) && all(uv <= 1.0)) {
    const float4 field = gTextures[NonUniformResourceIndex(f.fogMask)].SampleLevel(gSamplers[f.fogSampler], uv, 0);
    const float remembered = smoothstep(0.05, 0.6, min(field.r, 0.6)) * c.rememberedCover;
    k = max(smoothstep(0.0, 1.0, field.y), remembered);  // field.y: never-seen ground, blurred
    ground = field.z;                                      // never seen: the board's mean
  }
  return float2(k, ground);
}

float density(MeshFrame f, CloudFrame c, float3 p) {
  float2 k = coverage(f, c, p.xz);
  const float2 outside = max(max(c.board.xy - p.xz, p.xz - c.board.zw), 0.0);
  k.x *= 1.0 - smoothstep(c.reach * 0.35, c.reach, length(outside));
  if (k.x <= 0.0) return 0.0;
  float h = saturate((c.thickness - (p.y - k.y)) / max(c.thickness, 1e-3));
  if (h <= 0.0) return 0.0;
  h *= 1.0 + c.pool * (c.groundRange.y - k.y) / max(c.groundRange.y - c.groundRange.x, 1e-3);
  const float n = fbm(f, c, p * c.scale + c.wind * c.time * c.speed);
  const float wisps = smoothstep(c.contrast.x, c.contrast.y, n);
  return c.density * pow(h, c.heightPow) * k.x * (c.floorShare + (1.0 - c.floorShare) * wisps);
}

float linearDepth(MeshFrame f, float d) { return d > 0.0 ? f.zNear / d : 1e6; }

[numthreads(8, 8, 1)]
void CSMarch(uint3 id : SV_DispatchThreadID) {
  const CloudFrame c = loadCloud();
  if (any(id.xy >= c.halfSize)) return;
  const MeshFrame f = loadFrame(push.frame);
  RWTexture2D<float4> march = gStorageImagesRgba16f[NonUniformResourceIndex(c.march)];
  const uint2 full = min(id.xy * 2, c.fullSize - 1);
  const float2 uv = (float2(full) + 0.5) / float2(c.fullSize);
  const float d = gTextures[NonUniformResourceIndex(c.depth)].Load(int3(full, 0)).r;
  const float4 nearPoint = mul(float4(uv * 2.0 - 1.0, 1.0, 1.0), c.inverseViewProjection);
  const float3 rd = normalize(nearPoint.xyz / nearPoint.w - f.cameraPosition);
  float tMax = 1e4;
  if (d > 0.0) {
    const float4 s = mul(float4(uv * 2.0 - 1.0, d, 1.0), c.inverseViewProjection);
    tMax = length(s.xyz / s.w - f.cameraPosition);
  }
  // Only where the ray is inside the mist's layer.
  const float3 o = f.cameraPosition;
  const float lo = min(c.base, c.groundRange.x) - 0.5, hi = c.top;
  float t0 = 0.0, t1 = tMax;
  if (abs(rd.y) > 1e-4) {
    const float ta = (lo - o.y) / rd.y, tb = (hi - o.y) / rd.y;
    t0 = max(t0, min(ta, tb));
    t1 = min(t1, max(ta, tb));
  } else if (o.y < lo || o.y > hi) {
    t1 = -1.0;
  }
  if (t1 <= t0) {
    march[id.xy] = float4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  static const uint kSteps = 28;
  const float dt = (t1 - t0) / kSteps;
  // Interleaved gradient noise (Jimenez 2014) hides the step banding.
  const float jitter = frac(52.9829189 * frac(dot(float2(id.xy) + float(c.frameIndex % 64u) * 5.588238, float2(0.06711056, 0.00583715))));
  // The mist reads grey: the sky and the sun or moon set its brightness and barely tint it; a little
  // forward glow towards the sun.
  const float sunPhase = 0.6 + 0.25 * pow(max(dot(rd, f.sunDirection), 0.0), 6.0);
  float3 ambient = (f.lightingLayers & kLayerIndirect) ? f.skyColor * f.ambientIntensity * c.ambientShare : 0.0;
  if (f.lightingLayers & kLayerSun) ambient += f.sunColor * f.sunIntensity * c.sunShare * sunPhase;
  ambient = lerp(dot(ambient, float3(0.299, 0.587, 0.114)), ambient, 0.1);
  float transmittance = 1.0;
  float3 glow = 0.0;
  for (uint i = 0; i < kSteps; ++i) {
    const float3 p = o + rd * (t0 + (float(i) + jitter) * dt);
    const float rho = density(f, c, p);
    if (rho < 1e-4) continue;
    float3 light = ambient;
    if ((f.lightingLayers & kLayerLocal) && f.lightCount > 0) {
      // Halos round the local lights (only lights in sight are in the list; remembered air gets none).
      const float sightLocal = tacticalSight(f, p).local;
      if (sightLocal > 0.0)
        for (uint k = 0; k < f.lightCount; ++k) {
          const LocalLight l = loadLight(f, k);
          const float3 v = l.position - p;
          const float r = l.range * c.lightReach, q = dot(v, v) / (r * r);
          if (q < 1.0) light += l.color * f.exposure * c.lightShare * sightLocal * (1.0 - q) / (1.0 + 6.0 * q);
        }
    }
    light = min(light, 2.0);  // a spell's light warms the mist, never floods it
    const float a = 1.0 - exp(-rho * dt);
    glow += transmittance * a * c.albedo * light;
    transmittance *= 1.0 - a;
    if (transmittance < 0.01) break;
  }
  march[id.xy] = float4(min(glow, 64.0), transmittance);
}

[numthreads(8, 8, 1)]
void CSComposite(uint3 id : SV_DispatchThreadID) {
  const CloudFrame c = loadCloud();
  if (any(id.xy >= c.fullSize)) return;
  const MeshFrame f = loadFrame(push.frame);
  Texture2D depthTex = gTextures[NonUniformResourceIndex(c.depth)];
  RWTexture2D<float4> march = gStorageImagesRgba16f[NonUniformResourceIndex(c.march)];
  const float z = linearDepth(f, depthTex.Load(int3(id.xy, 0)).r);
  // The four half-resolution samples around this pixel, weighted bilinearly and down where their depth differs.
  const float2 h = (float2(id.xy) + 0.5) / 2.0 - 0.5;
  const int2 base = int2(floor(h));
  const float2 w2 = h - float2(base);
  float4 sum = 0.0;
  float weight = 0.0;
  [unroll] for (int j = 0; j < 2; ++j)
    [unroll] for (int i = 0; i < 2; ++i) {
      const int2 q = clamp(base + int2(i, j), int2(0, 0), int2(c.halfSize) - 1);
      const float zq = linearDepth(f, depthTex.Load(int3(min(uint2(q) * 2, c.fullSize - 1), 0)).r);
      const float bilinear = (i ? w2.x : 1.0 - w2.x) * (j ? w2.y : 1.0 - w2.y);
      const float k = bilinear / (0.02 + abs(zq - z) / max(z, 1e-3) * 50.0) + 1e-5;
      sum += march[q] * k;
      weight += k;
    }
  const float4 fog = sum / weight;
  RWTexture2D<float4> color = gStorageImagesRgba16f[NonUniformResourceIndex(c.color)];
  const float4 scene = color[id.xy];
  color[id.xy] = float4(scene.rgb * fog.a + fog.rgb, scene.a);
}
