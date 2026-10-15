// FOG LOOK 001 (2026-10-14): the first fog bank: thick, puffy billows over never-seen ground, self-shadowed
// (the user: "too puffy and cartoony", hard hex-shaped walls). Frozen: never edit a saved look; new looks go in
// new files (shaders/fog_lookNNN.hlsl). Only the user deletes looks.
//
// The fog of war's fog bank (R7, the user's reference, 2026-10-14): a thick, lit, drifting volume of fog lying
// over never-seen ground, thinner over the boundary and remembered ground, none in sight. Nothing under it
// shows (never-seen surfaces are not drawn at all; their lights are dropped; the bank's shape reads only the
// mask, never the ground beneath). It reaches past the board and thins into the dark, so it draws no outline.
//   CSMarch      half resolution: one ray per pixel through the bank's slab, up to the surface behind; density
//                from a tiling 3D noise volume (billows that drift with the wind and slowly change shape),
//                lit by the sun through the bank itself (a short march towards it: tops bright, undersides
//                dark) and the sky; Hillaire's energy-conserving integration.
//   CSComposite  full resolution: the half-resolution result upsampled by depth (no halos at silhouettes), over
//                the finished scene colour.
#include "lighting.hlsli"

// Matches ember::FogLook001Frame (FogLooks.cpp). 176 bytes.
struct CloudFrame {
  float4x4 inverseViewProjection;
  float2 boardCenter;     // the bank thins into the dark past fadeRadius from here
  float time;
  float baseY;            // the slab: bottom
  float topY;             // ... and top (the bank's tops billow below it)
  float extinction;       // per world unit at full density
  float fadeRadius;
  float rememberedCover;  // density over remembered ground, relative to never-seen
  float3 wind;            // drift, world units per second
  float noiseScale;       // billow frequency (per world unit)
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

// How much of the bank lies over a point: 1 never seen (and off the board), rememberedCover over remembered
// ground, 0 in sight; thinning far from the board.
float cover(MeshFrame f, CloudFrame c, float2 xz) {
  const float a = fogAlpha(f, float3(xz.x, 0.0, xz.y));
  float k = a >= 254.5 / 255.0 ? 1.0 : smoothstep(0.0, 0.6, a) * c.rememberedCover;
  k *= 1.0 - smoothstep(c.fadeRadius * 0.55, c.fadeRadius, length(xz - c.boardCenter));
  return k;
}

// Fog density (0..1) at p. Billows: two octaves of drifting noise, the slower one sliding against the faster so
// the shapes change as they drift; the bank's top rises and falls with the large octave.
float density(MeshFrame f, CloudFrame c, float3 p) {
  const float h = (p.y - c.baseY) / (c.topY - c.baseY);
  if (h <= 0.0 || h >= 1.0) return 0.0;
  const float k = cover(f, c, p.xz);
  if (k <= 0.0) return 0.0;
  const float3 q = p * c.noiseScale;
  const float large = noiseAt(f, c, q * float3(1.0, 1.6, 1.0) - c.wind * c.noiseScale * c.time);
  const float detail = noiseAt(f, c, q * 3.1 - c.wind * c.noiseScale * c.time * 1.7 + float3(0.31, 0.17, 0.53));
  // The top of the bank: low where the cover thins, billowing with the large octave.
  const float top = k * (0.45 + 0.55 * large);
  const float profile = smoothstep(0.0, 0.12, h) * (1.0 - smoothstep(top - 0.35, top, h));
  const float shape = saturate((large * 0.7 + detail * 0.3) * 1.6 - 0.35 + k * 0.6);
  return profile * shape * k;
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
    const float rho = density(f, c, p);
    if (rho <= 0.002) continue;
    const float extinction = rho * c.extinction;
    // Through the bank towards the sun: four samples, growing.
    float towardsSun = 0.0, s = 0.25;
    [unroll] for (uint k = 0; k < 4; ++k, s *= 1.8) towardsSun += density(f, c, p + f.sunDirection * s) * s;
    const float sunT = exp(-towardsSun * c.extinction * 0.9);
    // The sky reaches the tops more than the bottom.
    const float height = saturate((p.y - c.baseY) / (c.topY - c.baseY));
    const float3 light = (sunLight * sunT * phase + sky * (0.35 + 0.65 * height)) * c.albedo;
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
