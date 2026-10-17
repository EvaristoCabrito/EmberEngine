// FOG LOOK 008 (2026-10-16): look 003 that thins away near the camera. The user saw a white / yellow veil over the whole
// screen that "ping-pongs" when the camera came close (wisp-forest-crossing): the eye had dropped inside the fog's slab
// (past the board everything counts as never-seen, full fog), so 003's march began at the eye and the fog against
// the lens covered everything, coming and going as the eye crossed in and out. Here the density along each ray
// ramps in from nothing at nearFadeStart to full at nearFadeEnd world units from the eye: nothing stands between
// the camera and what it looks at close by; far away the bank is 003's own. Everything else is 003, unchanged.
// Frozen once seen: never edit a saved look; new looks go in new files. Only the user deletes looks.
#include "lighting.hlsli"

// Matches ember::Look008Frame (FogLooks.cpp). 192 bytes.
struct CloudFrame {
  float4x4 inverseViewProjection;
  float2 boardCenter;     // the bank thins into the dark past fadeRadius from here
  float time;
  float baseY;            // the slab: bottom
  float topY;             // ... and top (the fog thins to nothing below it)
  float extinction;       // per world unit at full density
  float fadeRadius;
  float rememberedCover;  // density over remembered ground, relative to never-seen
  float3 wind;            // drift, world units per second
  float noiseScale;       // variation frequency (per world unit)
  float scaleHeight;      // density falls by e over this height above the ground
  float nearFadeStart;    // the fog along a ray starts at nothing this far from the eye ...
  float nearFadeEnd;      // ... and is whole from here on
  float pad0;
  float3 albedo;
  float ambient;          // sky light scale
  uint noise;             // sampled 3D noise (R8, tiling)
  uint depth;             // sampled scene depth (reversed Z)
  uint march;             // storage: half-resolution result (scattered, transmittance)
  uint marchSampled;
  uint color;             // storage: scene colour (rgba16f)
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

float noiseAt(MeshFrame f, CloudFrame c, float3 p) {
  return gTextures3D[NonUniformResourceIndex(c.noise)].SampleLevel(gSamplers[f.materialSampler], p, 0).r;
}

// How far from the board the fog thins into the dark (1 near, 0 at fadeRadius).
float farFade(CloudFrame c, float2 xz) { return 1.0 - smoothstep(c.fadeRadius * 0.4, c.fadeRadius, length(xz - c.boardCenter)); }

// How much fog lies over a point, and the ground it rests on: never-seen ground blurred (fading softly across
// the boundary), remembered ground at rememberedCover, none in sight. Off the board: never seen.
float2 coverage(MeshFrame f, CloudFrame c, float2 xz) {
  const float2 uv = (xz - f.fogRect.xy) * f.fogRect.zw;
  float k = 1.0, ground = c.baseY + 1.0;
  if (all(uv >= 0.0) && all(uv <= 1.0)) {
    const float4 field = gTextures[NonUniformResourceIndex(f.fogMask)].SampleLevel(gSamplers[f.fogSampler], uv, 0);
    const float remembered = smoothstep(0.05, 0.6, min(field.r, 0.6)) * c.rememberedCover;
    k = max(smoothstep(0.0, 1.0, field.y), remembered);
    ground = field.z;
  }
  return float2(k * farFade(c, xz), ground);
}

// Fog density (0..1) at p: coverage, thinning exponentially above the ground and to nothing before the slab's
// top, varied softly by layered noise (stretched flat, two octaves drifting at different speeds so the fog
// changes as it moves). No thresholds: nothing in it has an edge.
float density(MeshFrame f, CloudFrame c, float3 p) {
  if (p.y <= c.baseY || p.y >= c.topY) return 0.0;
  const float2 k = coverage(f, c, p.xz);
  if (k.x <= 0.001) return 0.0;
  const float above = max(p.y - k.y, 0.0);
  const float height = exp(-above / c.scaleHeight) * (1.0 - smoothstep(c.topY - 1.5, c.topY, p.y));
  const float3 q = p * c.noiseScale * float3(1.0, 2.5, 1.0);  // flatter than wide: fog lies in layers
  const float slow = noiseAt(f, c, q - c.wind * c.noiseScale * c.time);
  const float fast = noiseAt(f, c, q * 2.3 - c.wind * c.noiseScale * c.time * 1.6 + float3(0.31, 0.17, 0.53));
  const float n = slow * 0.6 + fast * 0.4;
  // Banks and thin gaps: a smooth power curve (no threshold), so the black shows through where it thins.
  const float variation = pow(saturate((n - 0.2) / 0.65), 1.7) * 1.9;
  return k.x * height * variation;
}

float hash(uint2 p, uint frame) {
  uint h = p.x * 1973u + p.y * 9277u + frame * 26699u;
  h = (h << 13u) ^ h;
  return float((h * (h * h * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 2147483647.0;
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
  const float4 far = mul(float4(uv * 2.0 - 1.0, f.zNear / 1.0, 1.0), c.inverseViewProjection);
  const float3 dir = normalize(far.xyz / far.w - f.cameraPosition);
  float surface = 1e6;
  if (d > 0.0) {
    const float4 s = mul(float4(uv * 2.0 - 1.0, d, 1.0), c.inverseViewProjection);
    surface = length(s.xyz / s.w - f.cameraPosition);
  }
  // The slab between baseY and topY along the ray, up to the surface.
  const float3 o = f.cameraPosition;
  float t0 = 0.0, t1 = surface;
  if (abs(dir.y) > 1e-5) {
    const float ta = (c.baseY - o.y) / dir.y, tb = (c.topY - o.y) / dir.y;
    t0 = max(t0, min(ta, tb));
    t1 = min(t1, max(ta, tb));
  } else if (o.y < c.baseY || o.y > c.topY) {
    t1 = -1.0;
  }
  t1 = min(t1, t0 + c.fadeRadius * 2.0);
  if (t1 <= t0) {
    march[id.xy] = float4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  static const uint kSteps = 40;
  const float step = (t1 - t0) / kSteps;
  float t = t0 + step * hash(id.xy, c.frameIndex);
  const float3 sunLight = (f.lightingLayers & kLayerSun) ? f.sunColor * f.sunIntensity : 0.0;
  const float3 sky = (f.lightingLayers & kLayerIndirect) ? (f.skyColor * 0.75 + f.groundColor * 0.25) * f.ambientIntensity * c.ambient : 0.0;
  const float cosSun = dot(f.sunDirection, dir);
  // Two lobes: a forward glow towards the sun, and back-scattering so the side away from it is not flat.
  const float phase = lerp(phaseHG(cosSun, -0.15), phaseHG(cosSun, 0.6), 0.35);
  float3 scattered = 0.0;
  float transmittance = 1.0;
  for (uint i = 0; i < kSteps && transmittance > 0.01; ++i, t += step) {
    const float3 p = o + dir * t;
    const float rho = density(f, c, p) * smoothstep(c.nearFadeStart, c.nearFadeEnd, t);
    if (rho <= 0.002) continue;
    const float extinction = rho * c.extinction;
    // Through the fog towards the sun: three samples, growing.
    float towardsSun = 0.0, s = 0.5;
    [unroll] for (uint k = 0; k < 3; ++k, s *= 2.0) towardsSun += density(f, c, p + f.sunDirection * s) * s;
    const float sunT = exp(-towardsSun * c.extinction);
    // The sky reaches the upper fog more than the lower; far from the board the fog darkens as it thins, so it
    // ends in the night rather than in a bright rim.
    const float height = saturate((p.y - c.baseY) / (c.topY - c.baseY));
    const float far = farFade(c, p.xz);
    const float3 light = (sunLight * sunT * phase + sky * (0.5 + 0.5 * height)) * c.albedo * far;
    const float slab = exp(-extinction * step);
    scattered += transmittance * (light * extinction - light * extinction * slab) / extinction;
    transmittance *= slab;
  }
  march[id.xy] = float4(scattered, transmittance);
}

[numthreads(8, 8, 1)]
void CSComposite(uint3 id : SV_DispatchThreadID) {
  const CloudFrame c = loadCloud();
  if (any(id.xy >= c.fullSize)) return;
  const MeshFrame f = loadFrame(push.frame);
  Texture2D depthTex = gTextures[NonUniformResourceIndex(c.depth)];
  RWTexture2D<float4> march = gStorageImagesRgba16f[NonUniformResourceIndex(c.march)];
  const float z = linearDepth(f, depthTex.Load(int3(id.xy, 0)).r);
  // The four half-resolution samples around this pixel, weighted bilinearly and by depth likeness.
  const float2 h = (float2(id.xy) + 0.5) / 2.0 - 0.5;
  const int2 base = int2(floor(h));
  const float2 frac2 = h - float2(base);
  float4 sum = 0.0;
  float weight = 0.0;
  [unroll] for (int j = 0; j < 2; ++j)
    [unroll] for (int i = 0; i < 2; ++i) {
      const int2 q = clamp(base + int2(i, j), int2(0, 0), int2(c.halfSize) - 1);
      const float zq = linearDepth(f, depthTex.Load(int3(min(uint2(q) * 2, c.fullSize - 1), 0)).r);
      const float bilinear = (i ? frac2.x : 1.0 - frac2.x) * (j ? frac2.y : 1.0 - frac2.y);
      const float w = bilinear * exp(-abs(zq - z) / (0.03 * min(z, 1e4) + 0.05)) + 1e-5 * bilinear;
      sum += march[q] * w;
      weight += w;
    }
  const float4 fog = weight > 0.0 ? sum / weight : float4(0.0, 0.0, 0.0, 1.0);
  RWTexture2D<float4> color = gStorageImagesRgba16f[NonUniformResourceIndex(c.color)];
  const float4 scene = color[id.xy];
  color[id.xy] = float4(scene.rgb * fog.a + fog.rgb, scene.a);
}
