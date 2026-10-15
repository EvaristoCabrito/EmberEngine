// FOG LOOK 008 (2026-10-15): the map's own mist, Engine2's Névoas (src/game/mist3d.ts on src/render/volumetricFog.ts):
// the map editor's type (mist2 soft haze, mist3 blotchy, mist4 the vortex round the edges with the centre clear,
// fog1 only over unrevealed ground, fog5 low creeping wisps), intensity (density x intensity^1.5) and speed, as on
// each map. Unlike looks 001-007 it needs no fog of war: without one the mist lies over the whole board (fog1: only
// past its edge) and follows the lowest ground; with one, fog1 follows the never-seen ground. Lit by the real sky's
// irradiance when the atmosphere is on. Model and marching as look 006 (Engine2's mist4 copy).
#include "lighting.hlsli"

// Matches ember::Look008Frame (FogLooks.cpp): look 006's record, then the mist's kind. 288 bytes.
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
  float2 center;           // the board's centre: the vortex and the clear centre
  float swirl;             // vortex turn rate (Engine2 uSwirl)
  float clearRadius;       // no mist inside this radius ...
  float edgeRadius;        // ... full mist beyond this one
  float3 pad2;
  uint kind;               // 0 over the whole board, 1 mist4's vortex, 2 fog1 (unrevealed ground only)
  uint3 pad3;
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

// The ground the mist lies on and, with a fog of war, how unseen the point is (1 = never seen).
float2 coverage(MeshFrame f, CloudFrame c, float2 xz) {
  float k = 1.0, ground = c.groundRange.x;  // no fog of war: the lowest ground everywhere
  if (f.fogMask == kNoTexture) return float2(c.kind == 2 ? 0.0 : 1.0, ground);
  const float2 uv = (xz - f.fogRect.xy) * f.fogRect.zw;
  if (all(uv >= 0.0) && all(uv <= 1.0)) {
    const float4 field = gTextures[NonUniformResourceIndex(f.fogMask)].SampleLevel(gSamplers[f.fogSampler], uv, 0);
    k = smoothstep(0.0, 1.0, field.y);  // never-seen ground, blurred
    ground = field.z;
  }
  return float2(k, ground);
}

float density(MeshFrame f, CloudFrame c, float3 p) {
  float2 k = coverage(f, c, p.xz);
  const float2 rel = p.xz - c.center;
  const float r = length(rel);
  const float2 outside = max(max(c.board.xy - p.xz, p.xz - c.board.zw), 0.0);
  const bool offBoard = length(outside) > 0.0;
  if (c.kind == 1) k.x = smoothstep(c.clearRadius, c.edgeRadius, r);  // mist4: clear centre, full at the edges
  else if (c.kind == 2) k.x = offBoard ? 1.0 : k.x;                   // fog1: unrevealed ground and past the board
  else k.x = 1.0;                                                     // the whole world
  k.x *= 1.0 - smoothstep(c.reach * 0.35, c.reach, length(outside));
  if (k.x <= 0.0) return 0.0;
  float h = saturate((c.thickness - (p.y - k.y)) / max(c.thickness, 1e-3));
  if (h <= 0.0) return 0.0;
  h *= 1.0 + c.pool * (c.groundRange.y - k.y) / max(c.groundRange.y - c.groundRange.x, 1e-3);
  float3 q = p;
  if (c.swirl > 0.0) {  // mist4's slow vortex: the farther out, the slower it turns
    const float a = c.swirl * c.time * c.speed / (1.0 + r * 0.08);
    const float ca = cos(a), sa = sin(a);
    q.xz = c.center + float2(ca * rel.x - sa * rel.y, sa * rel.x + ca * rel.y);
  }
  const float n = fbm(f, c, q * c.scale + c.wind * c.time * c.speed);
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
  float3 ambient = 0.0;
  if (f.lightingLayers & kLayerIndirect)
    ambient = (f.atmosphere != 0 ? skyIrradiance(f, float3(0.0, 1.0, 0.0)) : f.skyColor * f.ambientIntensity) * c.ambientShare;
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
