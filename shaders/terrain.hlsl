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
  float seamStep;     // tabletop look: a dark seam on the walls at every this many units of height (0 = none)
  uint markCols;      // the board's columns; 0 = no ground marks
  uint64_t marks;     // per hex kMarkLayers x uint2: colour (sRGB + alpha, r in the low byte), half2 rOut / rIn
  uint markRows;
  uint gridCols;      // the battle grid's board (0 = no grid look), its look number and clock (grid_lookNNN.hlsli)
  uint gridRows;
  uint gridLook;
  uint64_t grid;      // per hex Renderer::GridCell: flags, extra
  float gridSeconds;
  uint wallFrom;      // smooth look: vertices from here on are the board's added side walls (BoardGround closeBoardSides); 0 = none
  float4 clip;        // x0, z0, x1, z1: the ground is drawn only inside (a deck that ends at its walls; everything elsewhere)
  float4 hole0;       // x0, z0, x1, z1: the ground is not drawn inside (a stairwell going down, Renderer::setGroundHoles); zero = none
  float4 hole1;
};
[[vk::push_constant]] Push push;

bool inGroundHole(float2 p) {
  return (push.hole0.z > push.hole0.x && all(p > push.hole0.xy) && all(p < push.hole0.zw)) ||
         (push.hole1.z > push.hole1.x && all(p > push.hole1.xy) && all(p < push.hole1.zw));
}

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
static const uint kLayerTile = kLayerHas + kPaletteSize;  // float: world units per repeat, 0 = 4

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
  nointerpolation float wall : TEXCOORD10;  // 1 on the board's added side walls
};

// The board stands on the ground image, never floats (the user, 2026-10-23: "o tabuleiro precisa sempre encostar no ground em
// todos os mapas, não existe exceção"): when the map's floor image lies f.boardDrop under the board's base (Renderer::setBoardDrop),
// every side-wall bottom at that base (a vertical face: the smooth look's lip ring, its void skirts and closeBoardSides walls; the hard
// look's rim walls) goes down by the same amount, so the sides reach the floor. A top face (normal up) never moves; 0 = as before.
float3 boardDropAt(MeshFrame f, float3 p, float3 n) {
  if (f.boardDrop > 0.0 && abs(n.y) < 0.5 && p.y <= f.boardBase + 0.02) p.y -= f.boardDrop;
  return p;
}

VSOut VSMain(uint id : SV_VertexID) {
  const MeshFrame f = loadFrame(push.frame);
  const uint index = push.indices != 0 ? vk::RawBufferLoad<uint>(push.indices + uint64_t(id) * 4) : id;
  const uint64_t v = push.vertices + uint64_t(index) * 48;
  VSOut o;
  o.normal = decodeNormal(vk::RawBufferLoad<uint>(v + 12));
  const float3 p = boardDropAt(f, vk::RawBufferLoad<float3>(v), o.normal);
  o.position = mul(float4(p, 1.0), f.viewProjection);
  o.world = p;
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
  // 0 the ground; 1 a side wall that takes no light or shadow of its own; 2 a side wall lit like the ground (wallFrom's top bit: the Inn).
  const uint wallFrom = push.wallFrom & 0x7FFFFFFFu;
  o.wall = wallFrom != 0 && index >= wallFrom ? ((push.wallFrom & 0x80000000u) != 0 ? 2.0 : 1.0) : 0.0;
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
  const uint64_t v = push.vertices + uint64_t(index) * 48;
  o.world = boardDropAt(f, vk::RawBufferLoad<float3>(v), decodeNormal(vk::RawBufferLoad<uint>(v + 12)));
  o.position = mul(float4(o.world, 1.0), f.shadowMatrix);
  return o;
}
void PSShadow(ShadowOut i) {
  if (neverSeen(loadFrame(push.frame), i.world)) discard;
  if (any(i.world.xz < push.clip.xy) || any(i.world.xz > push.clip.zw)) discard;
  if (inGroundHole(i.world.xz)) discard;
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

// Ground marks (Renderer::setGroundMarks): the battle grid painted on the ground after lighting, in display colours.
static const uint kMarkLayers = 8;
static const float kSqrt3 = 1.7320508;

// three.js ACES fit (tonemap.hlsl) and its inverse, to blend a mark in display space and hand back scene colour.
float3 acesFit(float3 color) {
  const float3x3 inputMat = float3x3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834, 0.01566, 0.02840, 0.13383, 0.83777);
  const float3x3 outputMat = float3x3(1.60475, -0.53108, -0.07367, -0.10208, 1.10813, -0.00605, -0.00327, -0.07276, 1.07602);
  const float3 v = mul(inputMat, color / 0.6);
  return saturate(mul(outputMat, (v * (v + 0.0245786) - 0.000090537) / (v * (0.983729 * v + 0.4329510) + 0.238081)));
}
float3 acesFitInverse(float3 display) {
  const float3x3 outputInv = float3x3(0.643038, 0.311187, 0.045775, 0.059269, 0.931436, 0.009295, 0.005962, 0.063929, 0.930118);
  const float3x3 inputInv = float3x3(1.764741, -0.675778, -0.088963, -0.147028, 1.160252, -0.013224, -0.036337, -0.162436, 1.198773);
  const float3 y = clamp(mul(outputInv, display), 0.0, 0.985);
  // y = (v^2 + 0.0245786 v - 0.000090537) / (0.983729 v^2 + 0.432951 v + 0.238081), solved for v >= 0
  const float3 a = 1.0 - 0.983729 * y, b = 0.0245786 - 0.432951 * y, c = -(0.000090537 + 0.238081 * y);
  const float3 v = (-b + sqrt(max(b * b - 4.0 * a * c, 0.0))) / (2.0 * a);
  return max(mul(inputInv, v) * 0.6, 0.0);
}
float3 toSrgb(float3 c) { return select(c <= 0.0031308, c * 12.92, 1.055 * pow(max(c, 0.0), 1.0 / 2.4) - 0.055); }
float3 fromSrgb(float3 c) { return select(c <= 0.04045, c / 12.92, pow((c + 0.055) / 1.055, 2.4)); }

// The hex under a world point (Hex.h hexAt) on a cols x rows board: its column / row and the point's offset from
// its centre (hex radius 1, pointy-top, odd rows shifted +x). False off the board.
bool hexLocate(float2 xz, uint cols, uint rows, out int2 cr, out float2 p) {
  cr = int2(0, 0);
  p = float2(0.0, 0.0);
  const float ox = kSqrt3 * (float(cols) - 0.5) * 0.5, oz = 0.75 * (float(rows) - 1.0);
  const float rf = (xz.y + oz) / 1.5, qf = (xz.x + ox) / kSqrt3 - rf * 0.5, sf = -qf - rf;
  float q = floor(qf + 0.5), r = floor(rf + 0.5);
  const float s = floor(sf + 0.5);
  const float dq = abs(q - qf), dr = abs(r - rf), ds = abs(s - sf);
  if (dq > dr && dq > ds) q = -r - s;
  else if (dr > ds) r = -q - s;
  const int ri = int(r);
  const int ci = int(q + (r - float(ri & 1)) * 0.5);
  if (ci < 0 || ri < 0 || ci >= int(cols) || ri >= int(rows)) return false;
  cr = int2(ci, ri);
  const float2 centre = float2(kSqrt3 * (float(ci) + 0.5 * float(ri & 1)) - ox, 1.5 * float(ri) - oz);
  p = xz - centre;
  return true;
}
// A point's hex distance from its hex's centre: 1 on the corners' hexagon.
float hexRadial(float2 p) {
  const float e = max(abs(p.x), max(abs(0.5 * p.x + 0.8660254 * p.y), abs(0.5 * p.x - 0.8660254 * p.y)));
  return e / 0.8660254;
}
// ... and the hex's cell index, or -1 off the board (the ground marks' lookup).
float hexDistance(float2 xz, out uint cell) {
  cell = 0;
  int2 cr;
  float2 p;
  if (!hexLocate(xz, push.markCols, push.markRows, cr, p)) return -1.0;
  cell = uint(cr.y) * push.markCols + uint(cr.x);
  return hexRadial(p);
}

#include "grid_look001.hlsli"

float3 applyGroundMarks(float3 hdr, float3 wp, float flatness) {
  const bool marks = push.markCols != 0 && push.marks != 0;
  const bool grid = push.gridCols != 0 && push.grid != 0 && push.gridLook != 0;
  if ((!marks && !grid) || flatness <= 0.0) return hdr;
  float3 display = toSrgb(acesFit(hdr));
  bool any = false;
  uint cell;
  const float d = marks ? hexDistance(wp.xz, cell) : -1.0;
  if (d >= 0.0) {
    const float aa = max(fwidth(d), 1e-4);
    for (uint k = 0; k < kMarkLayers; ++k) {
      const uint2 m = vk::RawBufferLoad<uint2>(push.marks + (uint64_t(cell) * kMarkLayers + k) * 8);
      if (m.x == 0) break;
      const float2 radii = f16tof32(uint2(m.y & 0xFFFF, m.y >> 16));
      const float4 c = unpackUnorm8(m.x);
      float cover = 1.0 - smoothstep(radii.x - aa, radii.x + aa, d);
      if (radii.y > 0.0) cover *= smoothstep(radii.y - aa, radii.y + aa, d);
      const float alpha = c.a * cover * flatness;
      if (alpha <= 0.0) continue;
      display = lerp(display, c.rgb, alpha);
      any = true;
    }
  }
  if (grid) {
    // The grid stays under Engine2's bloom threshold (tonemap.hlsl bright: 0.75 of display luminance), so its bright
    // lines never bloom over the cards standing in front of them (the user saw "sparks" on other characters' faces).
    const float3 lin0 = fromSrgb(display);
    const float before = dot(lin0, float3(0.2126, 0.7152, 0.0722));
    if (gridLook001(display, wp.xz, flatness, push.gridSeconds)) {
      const float3 lin = fromSrgb(display);
      const float cap = max(0.72, before);
      const float l = dot(lin, float3(0.2126, 0.7152, 0.0722));
      if (l > cap) display = toSrgb(lin * (cap / l));
      any = true;
    }
  }
  return any ? acesFitInverse(fromSrgb(display)) : hdr;
}

float3 sideTex(uint layer, float3 n, float3 wp, Grad zy, Grad xy, SamplerState s) {
  const float wx = n.x * n.x, wz = n.z * n.z;
  return (proto(layer, wp.zy / 2.0, zy, s) * wx + proto(layer, wp.xy / 2.0, xy, s) * wz) / max(wx + wz, 1e-4);
}

float4 PSMain(VSOut i) : SV_Target0 {
  const MeshFrame f = loadFrame(push.frame);
  SamplerState s = gSamplers[f.materialSampler];
  // The board's added side walls are painted like the edge's lip above them (the user, 2026-10-19: "deixar o preenchimento
  // inteiro da mesma cor que o resto das bordas"): the ground's own top material laid on the wall from the side, lit as the
  // lip is (its normal tilted up), instead of the cliff texture facing away from the light.
  const bool wall = i.wall > 0.5;
  const float3 geometric = normalize(i.normal);
  const float3 n = wall ? normalize(geometric + float3(0.0, 2.0, 0.0)) : geometric;
  const float3 wp = i.world;
  // Keep the lip material continuous around diagonal map edges: choosing world X or Z per face made adjacent wall strips
  // jump between projections as the camera moved, which showed up as vertical streaks on the board's side.
  const float2 wallTangent = normalize(float2(-geometric.z, geometric.x));
  const float2 tuv = wall ? float2(dot(wp.xz, wallTangent), wp.y) / 4.0 : wp.xz / 4.0;
  Grad g, gzy, gxy;
  g.dx = ddx(tuv), g.dy = ddy(tuv);
  gzy.dx = ddx(wp.zy / 2.0), gzy.dy = ddy(wp.zy / 2.0);
  gxy.dx = ddx(wp.xy / 2.0), gxy.dy = ddy(wp.xy / 2.0);
  // Never-seen ground is not drawn at all (R7): the black background shows, so no outline over lit ground.
  // (After the derivatives: neighbouring pixels may still need this one's.)
  if (neverSeen(f, wp)) discard;
  if (any(wp.xz < push.clip.xy) || any(wp.xz > push.clip.zw)) discard;
  if (inGroundHole(wp.xz)) discard;
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
    // the layer's own scale (EmberEngine's tiles: the size of the same art around the board), else every 4 units
    const float layerTile = asfloat(recordWord(kLayerTile + k));
    const float scale = layerTile > 0.0 ? 4.0 / layerTile : 1.0;
    const float2 luv = tuv * scale, ldx = g.dx * scale, ldy = g.dy * scale;
    setColor += gTextures[NonUniformResourceIndex(recordWord(kLayerColor + k))].SampleGrad(s, luv, ldx, ldy).rgb * w;
    const uint normalTex = recordWord(kLayerNormal + k), roughTex = recordWord(kLayerRough + k);
    float3 tn = float3(0.5, 0.5, 1.0);  // a layer without maps: flat normal, matte (Engine2's fallbacks)
    if (normalTex != kNoTexture) {
      const float2 xy = gTextures[NonUniformResourceIndex(normalTex)].SampleGrad(s, luv, ldx, ldy).rg * 2.0 - 1.0;
      tn = float3(xy, sqrt(saturate(1.0 - dot(xy, xy)))) * 0.5 + 0.5;
    }
    setNormal += tn * w;
    setRough += (roughTex != kNoTexture ? gTextures[NonUniformResourceIndex(roughTex)].SampleGrad(s, luv, ldx, ldy).r : 235.0 / 255.0) * w;
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
  float3 ground = lerp(cliffC, topC, flatness);
  if (push.seamStep > 0.0) {
    // Tabletop pieces: a dark seam where each stacked level meets the next (diorama.html GROOVE).
    const float gy = frac(wp.y / push.seamStep);
    ground *= 1.0 - (1.0 - flatness) * 0.5 * (1.0 - smoothstep(0.0, 0.07, gy));
  }
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
  // The board's side walls take no light or shadow of their own (the user, 2026-10-23: "esse sombreado nas bordas do tabuleiro tem que
  // acabar, essa área não deve ser afetada por luz e sombra"): one even light over the whole wall, the light a flat top takes in the
  // open, with no shadow, no highlight and no facing: the added walls whole, and every steep side face of the board (its cut edge, the
  // sides of its raised ground), eased in from a slope's 0.6 to 0.35 of up so a hill never shows a line. The Inn's floors as before
  // (wallFrom's top bit).
  const bool sidesLit = (push.wallFrom & 0x80000000u) != 0;
  const float side = sidesLit ? 0.0 : wall ? 1.0 : 1.0 - smoothstep(0.35, 0.6, geometric.y);
  if (side > 0.0) {
    const float3 up = float3(0.0, 1.0, 0.0);
    surface.normal = normalize(lerp(surface.normal, up, side));
    surface.geometricNormal = normalize(lerp(geometric, up, side));
    surface.receivesShadow = 1.0 - side;
    surface.receivesCardShadow = 1.0 - side;
    surface.specular = 1.0 - side;
  }
  return float4(applyGroundMarks(shade(f, surface), wp, smoothstep(0.5, 0.7, geometric.y)), 1.0);
}
