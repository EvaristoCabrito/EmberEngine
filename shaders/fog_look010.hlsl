// FOG LOOK 010 (2026-10-21): the gorge's wind fog (A Ponte de Pedra 023; the user: "a strong volumetric fog with strong gusts of wind
// flowing outside of the bridge walls"). A map's own fog, drawn without a fog of war (FogLook::fogOfWar false, MapDraft::emberWindFog):
// the gorge under and beside the bridge filled from its floor (the painting laid flat under the board, FogScene::floorY) up past the
// parapets, the deck itself clear. The wind blows along the gorge (across the bridge, under it), the noise stretched along it into
// streamers; gusts speed the whole flow up and down over seconds and travelling gust fronts push parts of it further. Lit by the key
// light (the moon at night) with a two-lobe phase and the sky; marched at half resolution and composited like look 009 (whose file
// stays as it was). Frozen once seen: a change is a new look.
#include "lighting.hlsli"

// Matches ember::Look010Frame (FogLooks.cpp). 192 bytes.
struct WindFrame {
  float4x4 inverseViewProjection;
  float4 deck;  // x0, z0, x1, z1
  float time;
  float deckY;
  float floorY;
  float topY;
  float3 wind;
  float gust;
  float extinction;
  float noiseScale;
  float wallMargin;
  float nearFadeEnd;
  float3 albedo;
  float ambient;
  uint noise;
  uint depth;
  uint march;
  uint marchSampled;
  uint color;
  uint frameIndex;
  uint2 halfSize;
  uint2 fullSize;
  uint2 pad;
};

struct Push {
  uint64_t frame;  // MeshFrame
  uint64_t cloud;  // WindFrame
};
[[vk::push_constant]] Push push;

WindFrame loadWind() { return vk::RawBufferLoad<WindFrame>(push.cloud); }

float noiseAt(MeshFrame f, WindFrame c, float3 p) {
  return gTextures3D[NonUniformResourceIndex(c.noise)].SampleLevel(gSamplers[f.materialSampler], p, 0).r;
}

// How far the flow has travelled by now: the mean wind with gusts on top (the speed swings between about half and twice the mean over
// a few seconds, never backwards), the integral of that speed so the field never shears.
float gustTravel(WindFrame c) {
  const float t = c.time;
  return t + c.gust * (0.55 * sin(0.61 * t) / 0.61 * 0.6 + 0.3 * sin(1.73 * t + 1.3) / 1.73 * 0.6 + 0.2 * sin(3.1 * t + 0.4) / 3.1 * 0.6);
}

// Fog density (0..) at p.
float density(MeshFrame f, WindFrame c, float3 p, float travel) {
  if (p.y <= c.floorY || p.y >= c.topY) return 0.0;
  // Outside the walls: how far past the deck's long sides (or past its ends); under the deck everything counts.
  const float outZ = max(c.deck.y - p.z, p.z - c.deck.w), outX = max(c.deck.x - p.x, p.x - c.deck.z);
  const float beyond = max(outZ, outX);
  const float region = p.y < c.deckY - 1.5 ? 1.0 : smoothstep(c.wallMargin * 0.25, c.wallMargin + 2.5, beyond);
  if (region <= 0.001) return 0.0;
  // Two layers with clear air between, so the gorge's floor shows through: a pool lying on the floor, and the wind's sheet at the
  // deck's height streaming past the walls; a thin haze between them; nothing above the slab's top.
  const float h = saturate((p.y - c.floorY) / (c.topY - c.floorY));
  const float pool = 0.9 * exp(-(p.y - c.floorY) / 7.0);
  const float sheetY = (p.y - (c.deckY - 0.5)) / 4.0;
  const float sheet = 1.25 * exp(-sheetY * sheetY);
  const float profile = (pool + sheet + 0.1) * (1.0 - smoothstep(0.85, 1.0, h));
  // Thins away far from the bridge, into the painting's own air.
  const float2 centre = float2(c.deck.x + c.deck.z, c.deck.y + c.deck.w) * 0.5;
  const float far = 1.0 - smoothstep(110.0, 220.0, length(p.xz - centre));
  // The noise in the wind's frame: stretched along it (streamers), carried by the gusting flow, the travelling gust fronts (a slow,
  // broad field moving with the mean wind) pushing parts of it further downwind.
  const float speed = length(c.wind);
  const float2 along = c.wind.xz / max(speed, 1e-3), across = float2(-along.y, along.x);
  const float s = c.noiseScale;
  const float3 frame = float3(dot(p.xz, across), p.y * 1.4, dot(p.xz, along) * 0.42);
  const float front = noiseAt(f, c, float3(frame.x * 0.25, frame.y * 0.25, frame.z * 0.6 - speed * c.time * 0.6) * s * 0.5);
  const float carried = speed * travel + c.gust * 18.0 * front;
  const float3 q = frame * s - float3(0.0, 0.0, carried * 0.42 * s);
  const float slow = noiseAt(f, c, q);
  const float fast = noiseAt(f, c, q * 2.6 - float3(0.0, 0.0, carried * 0.42 * s * 0.8) + float3(0.31, 0.17, 0.53));
  const float n = slow * 0.62 + fast * 0.38;
  // Streamers with clear lanes between them (a softened threshold: no hard edge).
  const float banks = smoothstep(0.36, 0.72, n) * 2.4;
  return region * profile * far * banks;
}

float hash(uint2 p, uint frame) {
  uint h = p.x * 1973u + p.y * 9277u + frame * 26699u;
  h = (h << 13u) ^ h;
  return float((h * (h * h * 15731u + 789221u) + 1376312589u) & 0x7fffffffu) / 2147483647.0;
}

float linearDepth(MeshFrame f, float d) { return d > 0.0 ? f.zNear / d : 1e6; }

[numthreads(8, 8, 1)]
void CSMarch(uint3 id : SV_DispatchThreadID) {
  const WindFrame c = loadWind();
  if (any(id.xy >= c.halfSize)) return;
  const MeshFrame f = loadFrame(push.frame);
  RWTexture2D<float4> march = gStorageImagesRgba16f[NonUniformResourceIndex(c.march)];
  const uint2 full = min(id.xy * 2, c.fullSize - 1);
  const float2 uv = (float2(full) + 0.5) / float2(c.fullSize);
  const float d = gTextures[NonUniformResourceIndex(c.depth)].Load(int3(full, 0)).r;
  const float4 farPoint = mul(float4(uv * 2.0 - 1.0, f.zNear / 1.0, 1.0), c.inverseViewProjection);
  const float3 dir = normalize(farPoint.xyz / farPoint.w - f.cameraPosition);
  // The sky stays clear (the user, 2026-10-21: "eu ainda preciso do céu no mapa da ponte, é um mapa externo"): no fog on the background
  // above the horizon; the eye may stand inside the wind sheet, whose near-level rays would cover the sky in grey.
  if (d <= 0.0 && dir.y >= 0.0) {
    march[id.xy] = float4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  float surface = 1e6;
  if (d > 0.0) {
    const float4 s = mul(float4(uv * 2.0 - 1.0, d, 1.0), c.inverseViewProjection);
    surface = length(s.xyz / s.w - f.cameraPosition);
  }
  // The slab between the gorge's floor and topY along the ray, up to the surface (the background: the floor itself).
  const float3 o = f.cameraPosition;
  float t0 = 0.0, t1 = surface;
  if (abs(dir.y) > 1e-5) {
    const float ta = (c.floorY - o.y) / dir.y, tb = (c.topY - o.y) / dir.y;
    t0 = max(t0, min(ta, tb));
    t1 = min(t1, max(ta, tb));
  } else if (o.y < c.floorY || o.y > c.topY) {
    t1 = -1.0;
  }
  t1 = min(t1, t0 + 260.0);
  if (t1 <= t0) {
    march[id.xy] = float4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  static const uint kSteps = 48;
  const float step = (t1 - t0) / kSteps;
  float t = t0 + step * hash(id.xy, c.frameIndex);
  const float travel = gustTravel(c);
  const float3 keyLight = (f.lightingLayers & kLayerSun) ? f.sunColor * f.sunIntensity : 0.0;
  const float3 sky = (f.lightingLayers & kLayerIndirect) ? (f.skyColor * 0.75 + f.groundColor * 0.25) * f.ambientIntensity * c.ambient : 0.0;
  const float cosSun = dot(f.sunDirection, dir);
  const float phase = lerp(phaseHG(cosSun, -0.15), phaseHG(cosSun, 0.6), 0.4);
  float3 scattered = 0.0;
  float transmittance = 1.0;
  for (uint i = 0; i < kSteps && transmittance > 0.01; ++i, t += step) {
    const float3 p = o + dir * t;
    const float rho = density(f, c, p, travel) * smoothstep(0.0, c.nearFadeEnd, t);
    if (rho <= 0.002) continue;
    const float extinction = rho * c.extinction;
    // Towards the key light through the fog: two samples.
    const float towards = density(f, c, p + f.sunDirection * 2.5, travel) * 2.5 + density(f, c, p + f.sunDirection * 8.0, travel) * 5.5;
    const float keyT = exp(-towards * c.extinction);
    // The sky reaches the upper fog more than the deep fog in the gorge.
    const float height = saturate((p.y - c.floorY) / (c.topY - c.floorY));
    const float3 light = (keyLight * keyT * phase + sky * (0.35 + 0.65 * height)) * c.albedo;
    const float slab = exp(-extinction * step);
    scattered += transmittance * (light - light * slab);
    transmittance *= slab;
  }
  march[id.xy] = float4(scattered, transmittance);
}

[numthreads(8, 8, 1)]
void CSComposite(uint3 id : SV_DispatchThreadID) {
  const WindFrame c = loadWind();
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
