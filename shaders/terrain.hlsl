// The board's ground: Engine2's terrain material (C:/Engine2/src/render/terrainMaterial.ts), ported.
// Each vertex carries surface weights (grass, forest, pavers, snow, rock, gravel, ash, plains049) and
// weights for the baked ground layers the map uses (a per-map palette, engine/render/BoardGround.h).
// Flat ground blends the surface textures in world space (4 units per repeat; grass without a visible
// repeat), steep faces take an earth or rock cliff texture projected from the side (2 units), and the
// baked layers (colour, normal, roughness) cover the tops where the map paints them. Weights summing to
// ~0 are void.
#include "lighting.hlsli"

struct Push {
  uint64_t frame;     // MeshFrame
  uint64_t record;    // TerrainGpu
  uint64_t vertices;  // TerrainVertex[] (48 bytes each)
  uint64_t indices;   // uint[]; 0 = one triangle per 3 vertices
};
[[vk::push_constant]] Push push;

static const uint kPaletteSize = 24;
// TerrainGpu (BoardGround.h): uint offsets.
static const uint kProto = 0;              // 9 procedural surfaces: grass forest pavers snow rock gravel ash earth cliff
static const uint kPlains049Color = 9;
static const uint kPlains049Rough = 10;
static const uint kLayerCount = 11;
static const uint kLayerColor = 12;
static const uint kLayerNormal = kLayerColor + kPaletteSize;
static const uint kLayerRough = kLayerNormal + kPaletteSize;
static const uint kLayerHas = kLayerRough + kPaletteSize;

uint recordWord(uint i) { return vk::RawBufferLoad<uint>(push.record + i * 4); }

float4 unpackUnorm8(uint v) { return float4(v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24) / 255.0; }

struct VSOut {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float4 surfacesA : TEXCOORD2;
  float4 surfacesB : TEXCOORD3;
  float4 layers0 : TEXCOORD4;
  float4 layers1 : TEXCOORD5;
  float4 layers2 : TEXCOORD6;
  float4 layers3 : TEXCOORD7;
  float4 layers4 : TEXCOORD8;
  float4 layers5 : TEXCOORD9;
};

VSOut VSMain(uint id : SV_VertexID) {
  const MeshFrame f = loadFrame(push.frame);
  const uint index = push.indices != 0 ? vk::RawBufferLoad<uint>(push.indices + uint64_t(id) * 4) : id;
  const uint64_t v = push.vertices + uint64_t(index) * 48;
  const float3 p = vk::RawBufferLoad<float3>(v);
  VSOut o;
  o.position = mul(float4(p, 1.0), f.viewProjection);
  o.world = p;
  o.normal = decodeNormal(vk::RawBufferLoad<uint>(v + 12));
  const uint2 s = vk::RawBufferLoad<uint2>(v + 16);
  const uint2 la = vk::RawBufferLoad<uint2>(v + 24);
  const uint4 lb = vk::RawBufferLoad<uint4>(v + 32);
  o.surfacesA = unpackUnorm8(s.x);
  o.surfacesB = unpackUnorm8(s.y);
  o.layers0 = unpackUnorm8(la.x);
  o.layers1 = unpackUnorm8(la.y);
  o.layers2 = unpackUnorm8(lb.x);
  o.layers3 = unpackUnorm8(lb.y);
  o.layers4 = unpackUnorm8(lb.z);
  o.layers5 = unpackUnorm8(lb.w);
  return o;
}

// Samples inside branches take explicit derivatives (computed before any branch): implicit ones are
// undefined where neighbouring pixels take different paths.
struct Grad {
  float2 dx, dy;
};

// The ground seen from the sun (depth only): the sun's shadow map. Never-seen ground casts nothing (R7).
struct ShadowOut {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
};
ShadowOut VSShadow(uint id : SV_VertexID) {
  const MeshFrame f = loadFrame(push.frame);
  const uint index = push.indices != 0 ? vk::RawBufferLoad<uint>(push.indices + uint64_t(id) * 4) : id;
  ShadowOut o;
  o.world = vk::RawBufferLoad<float3>(push.vertices + uint64_t(index) * 48);
  o.position = mul(float4(o.world, 1.0), f.shadowMatrix);
  return o;
}
void PSShadow(ShadowOut i) {
  if (neverSeen(loadFrame(push.frame), i.world)) discard;
}

float3 proto(uint layer, float2 uv, Grad g, SamplerState s) {
  return gTextures[NonUniformResourceIndex(recordWord(kProto + layer))].SampleGrad(s, uv, g.dx, g.dy).rgb;
}

// Grass with no visible repeat (Quilez's "texture without tiling"): every 4-unit patch samples the same
// grass at its own random offset, the 3x3 neighbours blended smoothly.
float4 tileHash(float2 p) {
  return frac(sin(float4(1.0 + dot(p, float2(37.0, 17.0)), 2.0 + dot(p, float2(11.0, 47.0)), 3.0 + dot(p, float2(41.0, 29.0)),
                         4.0 + dot(p, float2(23.0, 31.0)))) * 103.0);
}
float3 grassNoTile(float2 uv, Grad gr, SamplerState s) {
  const Texture2D grass = gTextures[NonUniformResourceIndex(recordWord(kProto))];
  const float2 p = floor(uv), fr = frac(uv), dx = gr.dx, dy = gr.dy;
  float3 acc = 0.0;
  float wsum = 0.0;
  [unroll] for (int j = -1; j <= 1; j++)
    [unroll] for (int i = -1; i <= 1; i++) {
      const float2 g = float2(i, j);
      const float4 o = tileHash(p + g);
      const float2 r = g - fr + o.xy;
      const float w = exp(-5.0 * dot(r, r));
      acc += w * grass.SampleGrad(s, uv + o.zw, dx, dy).rgb;
      wsum += w;
    }
  return acc / wsum;
}

float3 sideTex(uint layer, float3 n, float3 wp, Grad zy, Grad xy, SamplerState s) {
  const float wx = n.x * n.x, wz = n.z * n.z;
  return (proto(layer, wp.zy / 2.0, zy, s) * wx + proto(layer, wp.xy / 2.0, xy, s) * wz) / max(wx + wz, 1e-4);
}

float4 PSMain(VSOut i) : SV_Target0 {
  const MeshFrame f = loadFrame(push.frame);
  SamplerState s = gSamplers[f.materialSampler];
  const float3 n = normalize(i.normal);
  const float3 wp = i.world;
  const float2 tuv = wp.xz / 4.0;
  Grad g, gzy, gxy;
  g.dx = ddx(tuv), g.dy = ddy(tuv);
  gzy.dx = ddx(wp.zy / 2.0), gzy.dy = ddy(wp.zy / 2.0);
  gxy.dx = ddx(wp.xy / 2.0), gxy.dy = ddy(wp.xy / 2.0);
  // Never-seen ground is not drawn at all (R7): the black background shows, so no outline over lit ground.
  // (After the derivatives: neighbouring pixels may still need this one's.)
  if (neverSeen(f, wp)) discard;
  const float4 sA = i.surfacesA, sB = i.surfacesB;
  const float total = dot(sA, 1.0) + dot(sB, 1.0);
  // Engine2 samples every surface everywhere; a surface with no weight adds nothing, so it is skipped.
  float3 topC = 0.0;
  if (sA.x > 0.0) topC += grassNoTile(tuv, g, s) * sA.x;
  if (sA.y > 0.0) topC += proto(1, tuv, g, s) * sA.y;
  if (sA.z > 0.0) topC += proto(2, tuv, g, s) * sA.z;
  if (sA.w > 0.0) topC += proto(3, tuv, g, s) * sA.w;
  if (sB.x > 0.0) topC += proto(4, tuv, g, s) * sB.x;
  if (sB.y > 0.0) topC += proto(5, tuv, g, s) * sB.y;
  if (sB.z > 0.0) topC += proto(6, tuv, g, s) * sB.z;
  if (sB.w > 0.0) topC += gTextures[NonUniformResourceIndex(recordWord(kPlains049Color))].SampleGrad(s, tuv, g.dx, g.dy).rgb * sB.w;
  topC /= max(total, 1e-4);

  // Baked ground layers of the map's palette: weighted average of those present.
  const float4 lw[6] = {i.layers0, i.layers1, i.layers2, i.layers3, i.layers4, i.layers5};
  const uint layerCount = recordWord(kLayerCount);
  float gTotal = 0.0, have = 0.0, sum = 0.0;
  float3 setColor = 0.0, setNormal = 0.0;
  float setRough = 0.0;
  for (uint k = 0; k < layerCount; ++k) {
    const float w = lw[k >> 2][k & 3];
    gTotal += w;
    if (recordWord(kLayerHas + k) == 0 || w <= 0.001) continue;
    have += w;
    sum += w;
    setColor += gTextures[NonUniformResourceIndex(recordWord(kLayerColor + k))].SampleGrad(s, tuv, g.dx, g.dy).rgb * w;
    const uint normalTex = recordWord(kLayerNormal + k), roughTex = recordWord(kLayerRough + k);
    float3 tn = float3(0.5, 0.5, 1.0);  // a layer without maps: flat normal, matte (Engine2's fallbacks)
    if (normalTex != kNoTexture) {
      const float2 xy = gTextures[NonUniformResourceIndex(normalTex)].SampleGrad(s, tuv, g.dx, g.dy).rg * 2.0 - 1.0;
      tn = float3(xy, sqrt(saturate(1.0 - dot(xy, xy)))) * 0.5 + 0.5;
    }
    setNormal += tn * w;
    setRough += (roughTex != kNoTexture ? gTextures[NonUniformResourceIndex(roughTex)].SampleGrad(s, tuv, g.dx, g.dy).r : 235.0 / 255.0) * w;
  }
  const float setW = saturate(have / max(gTotal, 1e-4));
  if (setW > 0.0) topC = lerp(topC, setColor / max(sum, 1e-4), setW);

  const float ew = sA.x + sA.y + sB.y + sB.z + sB.w, rw = sA.z + sA.w + sB.x;
  const float flatness = smoothstep(0.45, 0.8, n.y);
  float3 cliffC = 0.0;
  if (flatness < 1.0) {  // cliff faces only (flat tops never show them)
    if (ew > 0.0) cliffC += sideTex(7, n, wp, gzy, gxy, s) * ew;
    if (rw > 0.0) cliffC += sideTex(8, n, wp, gzy, gxy, s) * rw;
    cliffC /= max(ew + rw, 1e-4);
  }
  const float3 ground = lerp(cliffC, topC, flatness);
  const float3 albedo = lerp(float3(0.035, 0.03, 0.03), ground, smoothstep(0.02, 0.2, total));

  float roughness = 0.92;
  if (sB.w > 0.0) roughness = lerp(0.92, gTextures[NonUniformResourceIndex(recordWord(kPlains049Rough))].SampleGrad(s, tuv, g.dx, g.dy).r, saturate(sB.w));
  float3 shading = n;
  if (setW > 0.0) {
    roughness = lerp(roughness, setRough / max(sum, 1e-4), setW);
    // the baked tiles' normal maps, on flat tops (seen from above: +X right, image up = world -Z)
    const float3 tn = (setNormal / max(sum, 1e-4)) * 2.0 - 1.0;
    const float3 wn = normalize(float3(tn.x, tn.z, -tn.y));
    shading = normalize(lerp(n, wn, setW * smoothstep(0.45, 0.8, n.y)));
  }
  if (f.debugView == 1) return float4(n * 0.5 + 0.5, 1.0);
  if (f.debugView == 2) return float4(shading * 0.5 + 0.5, 1.0);
  Surface surface = defaultSurface(wp, n, albedo);
  surface.normal = shading;
  surface.roughness = roughness;
  return float4(shade(f, surface), 1.0);
}
