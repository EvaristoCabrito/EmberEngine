// Ember's 3D water (the Map Editor's Água 3D brush): Engine2's ThreeWater material
// (C:/Engine2/src/game/gfx/three/ThreeWater.ts), in the board's 3D view (ThreeBattleRenderer's relief branch:
// waterFlat 0). The sheet comes from engine/world/Water3D (Engine2's rebuild, checked exact); here, per Engine2:
//   vertex   two crossing sine swells lift the surface (0.014 / 0.009 tile) and set its normal (with a finer third
//            slope term), in Engine2's tile frame p = (x, -y)
//   colour   MeshStandardMaterial(colour, map) of the map's water version (v1..v4) with the colour flowing,
//            a two-layer (three in v4) crossing ripple normal map, a procedural cloud / crest / cross-wave tint,
//            shore foam from the depth above the ground, edges faded by the brush coverage
//   light    diffuse + specular x (0.035, 0.055, 0.065) (the tactical highlight suppression), a sky tint by
//            Fresnel, then Engine2's soft knee (0.7) so water stays in the muted palette
// Departures, both forced: Engine2's water textures (public/game/textures/water*/water-*.png) are not in its
// repository, so the ground pack's own water layer (ground-claude/water) stands in; and the sky tint is Engine2's
// colour scaled by this frame's real sky light (the lighting contract) instead of a fixed number, so water darkens
// at night like everything else. Heights: Engine2's battle board rises 1.5 tile per level, this board 1, so
// the sheet's heights are divided by 1.5 (BoardGround); the swell too.
// Two looks (Escolhas "Agua 3D"; the user, 2026-10-16: "azul como a agua do rio do mapa"):
//   0 rio    the board's own water material (water.hlsl: Engine2's diorama water, #2f86a8), so painted water and the
//            map's river look the same; edges still fade with the brush coverage
//   1 leito  Engine2's ThreeWater material above, with the river-bed stand-in texture (dark slate)
#include "lighting.hlsli"

struct Push {
  uint64_t frame;     // MeshFrame
  uint64_t vertices;  // Water3DVertex[] (32 bytes: position, coverage, depth, Engine2 point)
  uint64_t indices;   // uint[]
  uint colorMap, normalMap, roughMap;  // bindless (kNoTexture: none)
  float colorR, colorG, colorB;        // material colour (linear)
  float opacity, normalScale, roughness, modern, v4, repeat;
  uint look;  // 0 rio, 1 leito
};
[[vk::push_constant]] Push push;

// water.hlsl's ripple height field, in texels of Engine2's 128 px tile
float riverRipple(float2 p) {
  return sin(p.x * 0.19 + sin(p.y * 0.11) * 2.0) * 0.5 + sin(p.y * 0.23 + p.x * 0.07) * 0.5 + sin((p.x + p.y) * 0.31) * 0.25;
}

struct VSOut {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float2 boardPoint : TEXCOORD2;  // Engine2's tile frame (x, -y): vWaterPoint
  float coverage : TEXCOORD3;
  float depth : TEXCOORD4;
};

VSOut VSMain(uint id : SV_VertexID) {
  const MeshFrame f = loadFrame(push.frame);
  const uint index = vk::RawBufferLoad<uint>(push.indices + uint64_t(id) * 4);
  const uint64_t v = push.vertices + uint64_t(index) * 32;
  const float3 base = vk::RawBufferLoad<float3>(v);
  const float coverage = vk::RawBufferLoad<float>(v + 12);
  const float depth = vk::RawBufferLoad<float>(v + 16);
  const float2 p = vk::RawBufferLoad<float2>(v + 20);
  const float t = f.time;
  const float phaseA = dot(p, float2(2.2, 1.5)) + t * 1.4;
  const float phaseB = dot(p, float2(-1.2, 3.1)) - t * 0.9;
  float2 slope = 0.014 * cos(phaseA) * float2(2.2, 1.5) + 0.009 * cos(phaseB) * float2(-1.2, 3.1);
  slope += 0.08 * cos(dot(p, float2(8.0, 5.0)) + t * 1.8) * float2(0.8, 0.5);
  // three's object frame (x, y = -board y, z up) -> this world (x, y up, z = board y)
  const float3 n3 = normalize(float3(-slope, 1.0));
  VSOut o;
  o.world = base + float3(0.0, (0.014 * sin(phaseA) + 0.009 * sin(phaseB)) / 1.5, 0.0);
  o.normal = float3(n3.x, n3.z, -n3.y);
  o.boardPoint = p;
  o.coverage = coverage;
  o.depth = depth;
  o.position = mul(float4(o.world, 1.0), f.viewProjection);
  return o;
}

float waterHash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float waterNoise(float2 p) {
  const float2 cell = floor(p), fr = frac(p);
  const float2 u = fr * fr * (3.0 - 2.0 * fr);
  return lerp(lerp(waterHash(cell), waterHash(cell + float2(1.0, 0.0)), u.x),
              lerp(waterHash(cell + float2(0.0, 1.0)), waterHash(cell + float2(1.0, 1.0)), u.x), u.y);
}
float waterCloud(float2 p) { return waterNoise(p) * 0.57 + waterNoise(p * 2.03 + 13.4) * 0.29 + waterNoise(p * 4.11 + 7.2) * 0.14; }

// three's TextureLoader flips images (v from the bottom row); cooked textures start at the top.
float4 sampleMap(uint map, SamplerState s, float2 uv, float4 none) {
  if (map == kNoTexture) return none;
  return gTextures[NonUniformResourceIndex(map)].Sample(s, float2(uv.x, 1.0 - uv.y));
}
// The ground pack's normal maps are BC5 (X, Y): Z rebuilt.
float3 rippleAt(SamplerState s, float2 uv) {
  if (push.normalMap == kNoTexture) return float3(0.0, 0.0, 1.0);
  const float2 xy = gTextures[NonUniformResourceIndex(push.normalMap)].Sample(s, float2(uv.x, 1.0 - uv.y)).xy * 2.0 - 1.0;
  return float3(xy, sqrt(saturate(1.0 - dot(xy, xy))));
}

float4 PSMain(VSOut i) : SV_Target0 {
  const MeshFrame f = loadFrame(push.frame);
  SamplerState s = gSamplers[f.materialSampler];
  const float2 vp = i.boardPoint;
  const float t = f.time;
  const float2 uv = vp * 0.25 * push.repeat;  // three: uv (x / 4, -y / 4) x texture.repeat
  // (texture reads before any discard: the neighbours' derivatives)
  const float2 flow = float2(t * 0.009, t * 0.004) * push.modern * (1.0 + 1.2 * push.v4);
  const float3 rippleA = rippleAt(s, uv + flow);
  const float3 rippleB = rippleAt(s, uv * 1.73 - flow * 0.65);
  const float3 rippleC = rippleAt(s, float2(uv.y, -uv.x) * 2.61 + flow * 1.9);
  const float2 colorFlow = float2(t * 0.009, t * 0.004) * push.modern;
  const float4 texel = sampleMap(push.colorMap, s, uv + colorFlow, 1.0);
  const float roughTexel = sampleMap(push.roughMap, s, uv, 1.0).r;
  if (i.coverage < 0.05) discard;
  if (neverSeen(f, i.world)) discard;  // never-seen water is not drawn (R7)

  if (push.look == 0) {  // rio: water.hlsl's material on this sheet
    const float2 ruv = i.world.xz / 4.0 + float2(0.02, 0.013) * t;
    const float2 rp = frac(ruv) * 128.0;
    const float3 tn = normalize(float3(-(riverRipple(rp + float2(1, 0)) - riverRipple(rp - float2(1, 0))),
                                       -(riverRipple(rp + float2(0, 1)) - riverRipple(rp - float2(0, 1))), 2.0));
    const float3 up = float3(0.0, 1.0, 0.0);
    Surface river = defaultSurface(i.world, up, srgbToLinear3(float3(0x2f, 0x86, 0xa8) / 255.0));
    river.normal = normalize(up + float3(tn.x, 0.0, tn.y) * 0.6 / max(tn.z, 1e-3));
    river.roughness = 0.5;
    river.metallic = 0.05;
    const float opacity = 0.74 * smoothstep(0.05, 0.55, i.coverage);
    return float4(shade(f, river) * opacity, opacity);
  }

  // normal_fragment_maps: tangent along board x, bitangent along three's +y (= -z here)
  const float3 n = normalize(i.normal);
  const float3 mapN = normalize(float3((rippleA.xy + rippleB.xy * 0.38 * push.modern + rippleC.xy * 0.45 * push.v4) * push.normalScale, rippleA.z));
  const float3 shading = normalize(float3(1.0, 0.0, 0.0) * mapN.x + float3(0.0, 0.0, -1.0) * mapN.y + n * mapN.z);

  // map_fragment + color_fragment
  // (the stand-in texture is opaque; its cooked alpha is not used)
  float4 diffuse = float4(float3(push.colorR, push.colorG, push.colorB) * texel.rgb, push.opacity);
  const float2 drift = float2(t * 0.055, -t * 0.035);
  const float cloud = waterCloud(vp * 2.2 + drift);
  const float detail = waterNoise(vp * 8.0 - drift * 1.7);
  const float2 warp = float2(cloud, waterCloud(vp * 2.2 + drift + 29.0));
  const float phase = dot(vp, float2(12.0, 5.0)) + warp.x * 7.0 + t * 0.7;
  const float wave = 0.5 + 0.5 * sin(phase);
  const float aa = max(fwidth(wave), 0.04);
  const float crest = smoothstep(0.76 - aa, 0.96 + aa, wave);
  const float broken = smoothstep(0.35, 0.7, waterCloud(vp * 3.7 - drift + 51.0));
  const float crossWave = sin(dot(vp, float2(-6.0, 15.0)) + warp.y * 5.0 - t * 0.5);
  const float modern = push.modern, v4 = push.v4;
  diffuse.rgb *= lerp(lerp(0.74 + 0.4 * cloud + 0.08 * detail, 0.92 + 0.13 * cloud + 0.035 * detail, modern), 1.0, v4);
  diffuse.rgb = lerp(diffuse.rgb, lerp(float3(0.20, 0.48, 0.49), float3(0.09, 0.17, 0.20), modern), crest * broken * lerp(0.24, 0.08, modern) * (1.0 - v4));
  diffuse.rgb += lerp(float3(0.018, 0.027, 0.025), float3(0.003, 0.005, 0.007), modern) * max(0.0, crossWave) * broken * (1.0 - v4);
  const float shore = 1.0 - smoothstep(0.015, 0.16, i.depth);
  const float foam = shore * lerp(0.035 + 0.09 * crest * broken, 0.015 + 0.045 * crest * broken, modern);
  diffuse.rgb = lerp(diffuse.rgb, float3(0.53, 0.67, 0.62), foam);
  diffuse.a *= smoothstep(0.05, 0.55, i.coverage);
  diffuse.a *= lerp(1.0, lerp(0.55, 1.0, smoothstep(0.45, 1.0, i.coverage)), v4);

  Surface surf = defaultSurface(i.world, n, diffuse.rgb);
  surf.normal = shading;
  surf.metallic = 0.0;
  surf.roughness = max(push.roughness * roughTexel, 0.68);  // roughnessmap_fragment, then the tactical floor
  if (f.debugView == kDebugFog || f.debugView == kDebugFogStates) return float4(shade(f, surf), 1.0);

  // opaque_fragment: diffuse + suppressed specular, sky tint by Fresnel, soft knee
  const Sight sight = tacticalSight(f, surf.sightAt);
  Surface diffuseOnly = surf;
  diffuseOnly.specular = 0.0;
  Surface specularOnly = surf;
  specularOnly.albedo = 0.0;
  const float3 totalDiffuse = shadeInSight(f, diffuseOnly, sight);
  const float3 totalSpecular = shadeInSight(f, specularOnly, sight);
  float3 outgoing = totalDiffuse + totalSpecular * float3(0.035, 0.055, 0.065);
  const float3 toEye = normalize(f.cameraPosition - i.world);
  const float facing = saturate(dot(shading, toEye));
  const float fresnel = lerp(0.02 + 0.30 * pow(1.0 - facing, 4.0), 0.03 + 0.40 * pow(1.0 - facing, 3.0), v4);
  const float viewUp = mul(float4(shading, 0.0), f.view).y;
  const float3 tint = lerp(float3(0.035, 0.065, 0.08), lerp(float3(0.22, 0.29, 0.33), float3(0.30, 0.38, 0.43), v4), smoothstep(-0.3, 0.8, viewUp));
  // Engine2's tint is a fixed radiance for its up-facing sky (0.22, 0.29, 0.33: luminance 0.276); here it is
  // scaled by the sky radiance this frame actually has above the water (E / pi).
  float sky = 0.0;
  if (f.lightingLayers & kLayerIndirect) sky = dot(indirectDiffuse(f, surf), float3(0.2126, 0.7152, 0.0722)) / kPi / 0.276;
  outgoing = lerp(outgoing, tint * sky, fresnel * modern);
  const float peak = max(max(outgoing.r, outgoing.g), outgoing.b);
  outgoing *= 0.7 / (0.7 + peak);
  const float3 color = applyVolume(f, outgoing * sight.color, i.world);
  return float4(color * diffuse.a, diffuse.a);
}
