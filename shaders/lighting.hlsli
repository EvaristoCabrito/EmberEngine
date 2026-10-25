// The lighting contract (docs/ARCHITECTURE.md §7.1b). Every lit surface (terrain, 3D meshes, 2D cards, later
// water and effects) fills one Surface and calls shade(). Inside: direct light (the sun with its shadow; the
// local-light list comes next), indirect light through indirectDiffuse() / indirectSpecular() (today a
// sky/ground hemisphere; later probes and the sky cubemap, behind the same functions), and the tactical
// visibility mask (R7), which no light may bypass.
//
// Units: linear radiometric RGB. Exposure is already multiplied into every light (MeshFrame), as Luminous does,
// so scene colour stays in fp16 range; emissive surfaces multiply it in themselves.
#ifndef EMBER_LIGHTING_HLSLI
#define EMBER_LIGHTING_HLSLI

#include "scene.hlsli"
#include "clouds.hlsli"

static const float kPi = 3.14159265;

// MeshFrame.lightingLayers: each layer can be switched off alone, for measuring it (Renderer::setLightingLayers).
static const uint kLayerSun = 1u;
static const uint kLayerShadows = 2u;
static const uint kLayerIndirect = 4u;
static const uint kLayerLocal = 8u;

struct Surface {
  float3 position;         // world
  float3 geometricNormal;  // the triangle's (or the card's) own normal: shadow offsets
  float3 normal;           // shading normal (normal map applied), unit length, facing the viewer
  float3 albedo;           // linear
  float roughness;         // perceptual (alpha = roughness^2)
  float metallic;
  float occlusion;         // baked or screen-space ambient occlusion
  float3 emissive;         // linear, before exposure
  float wrap;              // diffuse wrap-around (0 = Lambert; 2D cards: light from the side dims gently)
  float specular;          // 1 = full specular lobe, 0 = diffuse only (2D cards)
  float receivesShadow;    // 1 = the sun's shadow maps darken it, 0 = not
  float receivesCardShadow; // 1 = cards' shadows too, 0 = only the ground's and 3D pieces' (cards themselves)
  float twoSidedLocal;     // 1 = local lights reach it from both sides (a character card stands for a body)
  float3 sightAt;          // where the fog of war is read (the surface itself; a card: its foot, so it is seen whole)
};

Surface defaultSurface(float3 position, float3 normal, float3 albedo) {
  Surface s;
  s.position = position;
  s.geometricNormal = normal;
  s.normal = normal;
  s.albedo = albedo;
  s.roughness = 1.0;
  s.metallic = 0.0;
  s.occlusion = 1.0;
  s.emissive = 0.0;
  s.wrap = 0.0;
  s.specular = 1.0;
  s.receivesShadow = 1.0;
  s.receivesCardShadow = 1.0;
  s.twoSidedLocal = 0.0;
  s.sightAt = position;
  return s;
}

float3 fresnelSchlick(float3 f0, float vh) { return f0 + (1.0 - f0) * pow(1.0 - vh, 5.0); }
float ggx(float nh, float a) {
  const float a2 = a * a, d = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (kPi * d * d);
}
float smithVisibility(float nv, float nl, float a) {  // height-correlated, includes 1 / (4 nv nl)
  const float a2 = a * a;
  const float v = nl * sqrt(nv * nv * (1.0 - a2) + a2), l = nv * sqrt(nl * nl * (1.0 - a2) + a2);
  return 0.5 / max(v + l, 1e-5);
}

// Karis's analytic fit of the split-sum environment BRDF (Lazarov's approximation): scale and bias on F0.
float3 envBrdfApprox(float3 f0, float roughness, float nv) {
  const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022), c1 = float4(1.0, 0.0425, 1.04, -0.04);
  const float4 r = roughness * c0 + c1;
  const float a004 = min(r.x * r.x, exp2(-9.28 * nv)) * r.x + r.y;
  const float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
  return f0 * ab.x + ab.y;
}

// Henyey-Greenstein phase function (participating media: fog, clouds).
float phaseHG(float cosTheta, float g) {
  const float g2 = g * g;
  return (1.0 - g2) / (4.0 * kPi * pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5));
}

// --- Direct light ------------------------------------------------------------------------------------

// The sun's shadow map: one orthographic map fitted to the board, 16 hardware-filtered taps on a Poisson
// disc (soft edges), the receiver pushed along its geometric normal by about a texel (no acne on slopes).
static const float2 kPoisson16[16] = {
  float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.09418410, -0.92938870), float2(0.34495938, 0.29387760),
  float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464), float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
  float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420), float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
  float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)};

float sunShadow(MeshFrame f, Surface s) {
  if ((f.lightingLayers & kLayerShadows) == 0 || f.shadowMap == kNoTexture) return 1.0;
  const float3 p = s.position + s.geometricNormal * f.shadowTexelWorld * 1.5;
  const float4 c = mul(float4(p, 1.0), f.shadowMatrix);
  const float3 uvz = float3(c.xy * float2(0.5, 0.5) + 0.5, c.z);
  if (any(uvz.xy < 0.0) || any(uvz.xy > 1.0) || uvz.z > 1.0) return 1.0;
  Texture2D map = gTextures[NonUniformResourceIndex(f.shadowMap)];
  SamplerComparisonState cmp = gComparisonSamplers[f.shadowSampler];
  const bool cards = s.receivesCardShadow > 0.0 && f.cardShadowMap != kNoTexture;
  Texture2D cardMap = gTextures[NonUniformResourceIndex(cards ? f.cardShadowMap : f.shadowMap)];
  float lit = 0.0;
  const float radius = f.shadowFilterTexels / f.shadowSize;
  [unroll] for (uint i = 0; i < 16; ++i) {
    const float2 uv = uvz.xy + kPoisson16[i] * radius;
    float t = map.SampleCmpLevelZero(cmp, uv, uvz.z);
    if (cards) t = min(t, cardMap.SampleCmpLevelZero(cmp, uv, uvz.z));
    lit += t;
  }
  return lit / 16.0;
}

// The spell light's shadow (Renderer setLocalShadow): the cards seen from that light, a soft 16-tap compare. 1 = lit. Its
// fields come one by one and it runs once per surface, before the light loop (DXC gotcha, CLAUDE.md: read through the MeshFrame
// passed by value, inside the loop, it lost the GPU even when the branch was never taken).
float localShadow(float4x4 lightMatrix, uint mapIndex, uint samplerIndex, float size, float3 position, float3 normal, float3 lightPosition) {
  const float3 p = position + normal * 0.04 + normalize(lightPosition - position) * 0.04;
  const float4 c = mul(float4(p, 1.0), lightMatrix);
  if (c.w <= 1e-4) return 1.0;
  const float3 uvz = float3(c.xy / c.w * 0.5 + 0.5, c.z / c.w);
  if (any(uvz.xy < 0.0) || any(uvz.xy > 1.0) || uvz.z > 1.0 || uvz.z < 0.0) return 1.0;
  Texture2D map = gTextures[NonUniformResourceIndex(mapIndex)];
  SamplerComparisonState cmp = gComparisonSamplers[samplerIndex];
  const float radius = 2.5 / size;
  float lit = 0.0;
  [unroll] for (uint i = 0; i < 16; ++i) lit += map.SampleCmpLevelZero(cmp, uvz.xy + kPoisson16[i] * radius, uvz.z - 0.0005);
  return lit / 16.0;
}

// Local lights through the cluster this point falls in. Engine2's falloff (three.js punctual lights):
// 1 / max(d^decay, 0.01), windowed to zero at the range by (1 - (d / range)^4)^2.
static const float kLightSourceRadius = 0.5;  // world units (~35 cm: a flame with its glow and bracket)
float3 localLights(MeshFrame f, Surface s, float3 view, float3 diffuseColor, float3 f0, float a, float nv) {
  if ((f.lightingLayers & kLayerLocal) == 0 || f.lightCount == 0) return 0.0;
  // (Each light already carries its source's sight: Renderer dims or drops lights out of sight.)
  const float4 clip = mul(float4(s.position, 1.0), f.viewProjection);
  if (clip.w <= 0.0) return 0.0;
  const float2 ndc = clip.xy / clip.w;
  const uint2 tile = uint2(clamp((ndc * 0.5 + 0.5) * float2(kClusterX, kClusterY), 0.0, float2(kClusterX, kClusterY) - 1.0));
  const float depth = -mul(float4(s.position, 1.0), f.view).z;
  const uint cluster = tile.x + tile.y * kClusterX + clusterSlice(f, depth) * kClusterX * kClusterY;
  const uint64_t list = f.clusters + clusterOffset(cluster);
  const uint count = vk::RawBufferLoad<uint>(list);
  const float3 spellLight = f.localShadowPosition;
  const bool spellShadow = f.localShadowMap != kNoTexture && s.receivesCardShadow > 0.0 && (f.lightingLayers & kLayerShadows) != 0;
  const float spellLit = spellShadow ? localShadow(f.localShadowMatrix, f.localShadowMap, f.shadowSampler, f.localShadowSize, s.position,
                                                   s.geometricNormal, spellLight) : 1.0;
  float3 color = 0.0;
  for (uint k = 0; k < count; ++k) {
    const LocalLight light = loadLight(f, vk::RawBufferLoad<uint>(list + 4 + k * 4));
    const float3 toLight = light.position - s.position;
    const float d = length(toLight);
    if (d >= light.range) continue;
    const float3 l = toLight / max(d, 1e-4);
    const float ratio = d / light.range;
    const float window = saturate(1.0 - ratio * ratio * ratio * ratio);
    // A physical light (decay 2) is a small sphere, not a point (Karis 2013, Frostbite 2014): 1 / (d^2 + r^2), so the wall beside a
    // flame takes no white hot spot (the user, 2026-10-25: "muito bloom, pouco radius"); from a unit away it is the inverse square.
    float falloff = window * window / (light.decay == 2.0 ? d * d + kLightSourceRadius * kLightSourceRadius : max(pow(d, light.decay), 0.01)) *
                    lightFacing(light, s.position);
    if (spellShadow && distance(light.position, spellLight) < 1e-3) falloff *= spellLit;
    float3 n = s.normal;
    if (s.twoSidedLocal > 0.0 && dot(n, l) < 0.0) n = -n;
    const float nlRaw = dot(n, l);
    const float nl = saturate(nlRaw), nlDiffuse = saturate((nlRaw + s.wrap) / (1.0 + s.wrap));
    if (nlDiffuse <= 0.0) continue;
    const float3 h = normalize(l + view);
    const float3 F = fresnelSchlick(f0, saturate(dot(view, h))) * s.specular;
    color += (diffuseColor * (1.0 - F) / kPi * nlDiffuse + F * ggx(saturate(dot(n, h)), a) * smithVisibility(nv, nl, a) * nl) *
             light.color * f.exposure * falloff;
  }
  return color;
}

// --- Indirect light (the interface stays when probes and the sky arrive) --------------------------------

// Irradiance E arriving at the surface from everywhere but the direct lights.
// The sky's irradiance on a surface facing n: 9 spherical harmonics (atmosphere.hlsl CSAmbient), already scaled to
// the map's ambient strength.
float3 skyIrradiance(MeshFrame f, float3 n) {
  const uint64_t sh = loadAtmosphere(f).ambient;
  const float b[9] = {0.282095, 0.488603 * n.y, 0.488603 * n.z, 0.488603 * n.x, 1.092548 * n.x * n.y, 1.092548 * n.y * n.z,
                      0.315392 * (3.0 * n.z * n.z - 1.0), 1.092548 * n.x * n.z, 0.546274 * (n.x * n.x - n.y * n.y)};
  float3 e = 0.0;
  [unroll] for (uint c = 0; c < 9; ++c) e += vk::RawBufferLoad<float4>(sh + c * 16).rgb * b[c];
  return max(e, 0.0);
}

// The room's diffuse light probes (probes.hlsl; Luz das janelas, after Final Fantasy XV's): the irradiance at `p` facing `n`, from
// the 8 probes around it (trilinear), each weighted by how much it faces the surface, only those standing in the room; `found` false
// where no probe reaches (outside the room: no light from outside but through the openings).
float3 probeIrradiance(MeshFrame f, float3 p, float3 n, out bool found) {
  const float3 origin = vk::RawBufferLoad<float4>(probesAddress(f)).xyz, spacing = vk::RawBufferLoad<float4>(probesAddress(f) + 16u).xyz;
  const uint4 dims = vk::RawBufferLoad<uint4>(probesAddress(f) + 32u);
  const float3 g = (p + n * 0.35 - origin) / spacing;
  const int3 base = int3(floor(g));
  const float3 t = g - float3(base);
  const float b[9] = {0.282095, 0.488603 * n.y, 0.488603 * n.z, 0.488603 * n.x, 1.092548 * n.x * n.y, 1.092548 * n.y * n.z,
                      0.315392 * (3.0 * n.z * n.z - 1.0), 1.092548 * n.x * n.z, 0.546274 * (n.x * n.x - n.y * n.y)};
  float3 sum = 0.0;
  float total = 0.0;
  [loop] for (uint c = 0; c < 8u; ++c) {  // (a loop, not unrolled: every lit shader carries this)
    const int3 o = int3(c & 1u, (c >> 1) & 1u, (c >> 2) & 1u);
    const int3 q = base + o;
    if (any(q < 0) || q.x >= int(dims.x) || q.y >= int(dims.y) || q.z >= int(dims.z)) continue;
    const uint64_t at = probesAddress(f) + 64u + uint64_t(uint(q.x) + dims.x * (uint(q.z) + dims.z * uint(q.y))) * 144u;
    const float4 first = vk::RawBufferLoad<float4>(at);
    if (first.w < 0.5) continue;
    const float3 tri = lerp(1.0 - t, t, float3(o));
    const float3 toProbe = normalize(origin + float3(q) * spacing - p);
    const float w = tri.x * tri.y * tri.z * smoothstep(-0.2, 0.3, dot(toProbe, n));
    if (w <= 1e-5) continue;
    float3 e = first.rgb * b[0];
    [loop] for (uint k = 1; k < 9u; ++k) e += vk::RawBufferLoad<float4>(at + k * 16u).rgb * b[k];
    sum += e * w;
    total += w;
  }
  found = total > 1e-4;
  return found ? max(sum / total, 0.0) : float3(0.0, 0.0, 0.0);
}

// The room seen from above (the probes' mask: Renderer::ProbeVolume::mask): 1 inside the room (a wall's inner half included), 0 outside;
// 1 where the probes carry no mask. Outside the room nothing from outside lights a surface (the user, 2026-10-21: "não renderizar nada
// do lado de fora"; 2026-10-22: the sun on the windows' outer faces and the foot of the walls bloomed into white blobs).
float probeRoom(MeshFrame f, float3 p) {
  const float4 origin = vk::RawBufferLoad<float4>(probesAddress(f));
  if (origin.w <= 0.0) return 1.0;
  // The roof's height (spacing.w, 0 = none): a surface at it or above (the walls' tops, anything on the roof) is outside the room:
  // the sun and the sky reach the room only through its windows and doors (the user, 2026-10-23: "eu já falei que eu não quero sol
  // nas paredes externas e nem no topo do mapa", "ele só entra pelas janelas e portas").
  const float roof = vk::RawBufferLoad<float4>(probesAddress(f) + 16u).w;
  if (roof > 0.0 && p.y >= roof - 0.05) return 0.0;
  const uint4 mask = vk::RawBufferLoad<uint4>(probesAddress(f) + 48u);  // keyCount, the mask's x and z texels, its offset
  const int2 t = int2(floor((p.xz - origin.xz) / origin.w));
  if (any(t < 0) || t.x >= int(mask.y) || t.y >= int(mask.z)) return 0.0;
  const uint i = uint(t.x) + mask.y * uint(t.y);
  const uint word = vk::RawBufferLoad<uint>(probesAddress(f) + mask.w + (i / 4u) * 4u);
  return float((word >> ((i % 4u) * 8u)) & 0xFFu) / 255.0;
}

float3 indirectDiffuse(MeshFrame f, Surface s) {
  if (probesAddress(f) != 0) {
    bool found;
    return probeIrradiance(f, s.position, s.normal, found);
  }
  if (f.atmosphere != 0) return skyIrradiance(f, s.normal);
  return lerp(f.groundColor, f.skyColor, s.normal.y * 0.5 + 0.5) * f.ambientIntensity;
}

// The sky's reflection map (skyreflect.hlsl): a lat-long image of the sky's radiance (clouds in), level 0 sharp and each
// level above prefiltered for a rougher surface (roughness = level / (levels - 1)). Half a texel in from the poles so the
// repeat sampler never reads across them (u wraps at the azimuth seam, as it should).
static const uint kSkyReflectLevels = 6u;
static const float2 kSkyReflectSize = float2(256.0, 128.0);
float2 latLongUv(float3 dir) {
  const float u = atan2(dir.z, dir.x) / (2.0 * kPi) + 0.5;
  const float v = acos(clamp(dir.y, -1.0, 1.0)) / kPi;
  return float2(u, lerp(0.5 / kSkyReflectSize.y, 1.0 - 0.5 / kSkyReflectSize.y, v));
}
// (the sample in its own helper, the coordinate finished first: the DXC gotcha of volumeSampleAt)
float3 skyReflectSample(MeshFrame f, float2 uv, float lod) {
  return gTextures[NonUniformResourceIndex(f.skyReflect)].SampleLevel(gSamplers[f.materialSampler], uv, lod).rgb;
}
// Prefiltered sky radiance along `dir` for a surface of `roughness`, on the same scale as the SH ambient (CSAmbient
// stores its scale in the ambient record's first w).
float3 skyReflection(MeshFrame f, float3 dir, float roughness) {
  const float scale = vk::RawBufferLoad<float4>(loadAtmosphere(f).ambient).w;
  const float2 uv = latLongUv(dir);
  const float lod = saturate(roughness) * float(kSkyReflectLevels - 1u);
  return skyReflectSample(f, uv, lod) * scale;
}

// Prefiltered radiance along the reflection direction: the sky's reflection map when the frame has one (sharp water,
// polished stone, metal see the real sky, clouds and all), else the sky's irradiance there / pi (a rough reflection;
// the hemisphere's uniform radiance E / pi without the atmosphere).
float3 indirectSpecular(MeshFrame f, Surface s, float3 reflected) {
  if (probesAddress(f) != 0) {  // inside the room: what the probes see that way (no sky but through the openings)
    bool found;
    return probeIrradiance(f, s.position, reflected, found) / kPi;
  }
  if (f.atmosphere != 0) {
    if (f.skyReflect != kNoTexture) return skyReflection(f, reflected, s.roughness);
    return skyIrradiance(f, reflected) / kPi;
  }
  return lerp(f.groundColor, f.skyColor, reflected.y * 0.5 + 0.5) * f.ambientIntensity / kPi;
}

// --- Tactical visibility (R7) -------------------------------------------------------------------------

// What the fog of war lets through at a point (world/FogOfWar.h):
//   color  multiplies the finished colour: 1 in sight, 0.4 remembered (60% dark), 0 never seen
//   local  multiplies local light: 1 in sight, fading to 0 across the feather into remembered ground (memory
//          shows the last-known state, never a torch or spell lighting it now)
// The mask decides what the player may see; atmospheric fog (later) only decides how the visible air looks.
struct Sight {
  float color;
  float local;
};
Sight tacticalSight(MeshFrame f, float3 position) {
  const float a = fogAlpha(f, position);
  Sight s;
  s.color = a >= 254.5 / 255.0 ? 0.0 : 1.0 - a;  // never seen: exactly black
  s.local = saturate(1.0 - a / 0.6);              // 0.6 = FogOfWar.h kExploredAlpha
  return s;
}

// --- The surface's colour --------------------------------------------------------------------------------

// Lit colour before the fog of war's darkening (`sight` limits local light); shade() applies all of it. The local lights' share
// comes back apart in `local` (the cards cap the two shares each on their own: sprites.hlsl).
float3 shadeInSightSplit(MeshFrame f, Surface s, Sight sight, out float3 local) {
  const float3 view = normalize(f.cameraPosition - s.position);
  const float a = max(s.roughness * s.roughness, 0.002);
  const float3 diffuseColor = s.albedo * (1.0 - s.metallic);
  const float3 f0 = lerp(0.04, s.albedo, s.metallic);
  const float nv = max(dot(s.normal, view), 1e-4);

  float3 color = 0.0;
  const float room = probesAddress(f) != 0 ? probeRoom(f, s.position) : 1.0;  // a room lit through its windows: nothing from outside out of it
  if ((f.lightingLayers & kLayerSun) && room > 0.0) {
    const float3 l = f.sunDirection, h = normalize(l + view);
    const float nlRaw = dot(s.normal, l);
    const float nl = saturate(nlRaw), nh = saturate(dot(s.normal, h)), vh = saturate(dot(view, h));
    const float nlDiffuse = saturate((nlRaw + s.wrap) / (1.0 + s.wrap));
    if (nlDiffuse > 0.0) {
      const float3 F = fresnelSchlick(f0, vh) * s.specular;
      // The shadow maps, then the clouds' (clouds.hlsli): every surface, the cards too.
      const float shadow = (s.receivesShadow > 0.0 ? lerp(1.0, sunShadow(f, s), s.receivesShadow) : 1.0) * cloudShadow(f, s.position);
      color += (diffuseColor * (1.0 - F) / kPi * nlDiffuse + F * ggx(nh, a) * smithVisibility(nv, nl, a) * nl) * f.sunColor *
               f.sunIntensity * shadow * room;
    }
  }
  local = sight.local > 0.0 ? localLights(f, s, view, diffuseColor, f0, a, nv) * sight.local : float3(0.0, 0.0, 0.0);
  if ((f.lightingLayers & kLayerIndirect) && room > 0.0) {
    // Lambert returns albedo * E / pi; the specular lobe sees the environment's radiance through the
    // split-sum BRDF.
    color += (diffuseColor * indirectDiffuse(f, s) / kPi +
              envBrdfApprox(f0, s.roughness, nv) * s.specular * indirectSpecular(f, s, reflect(-view, s.normal))) * s.occlusion * room;
  }
  color += s.emissive * f.exposure;
  return color;
}

float3 shadeInSight(MeshFrame f, Surface s, Sight sight) {
  float3 local;
  const float3 color = shadeInSightSplit(f, s, sight, local);
  return color + local;
}

// Whether the air between the camera and the surfaces is applied: the atmosphere's kmPerUnit above 0 (read in a helper, the DXC
// gotcha with values read from a frame passed by value).
bool aerialOn(MeshFrame f) {
  const AtmosphereFrame a = loadAtmosphere(f);
  return a.kmPerUnit > 0.0;
}

// The volumetric fog between the camera and a surface (volume.hlsl): what reaches the eye.
float3 applyVolume(MeshFrame f, float3 color, float3 position) {
  const float4 c = mul(float4(position, 1.0), f.viewProjection);
  if (c.w <= 0.0) return color;
  if (f.atmosphere != 0 && aerialOn(f)) {
    // Aerial perspective (the air between the camera and the surface, at the artistic scale), the farther layer. A scale of 0
    // means no air at all (a room lit through its windows, Tática): at 0 the first slice was still laid at half weight over
    // every surface, the day sky's blue as a flat veil over the whole room (found 2026-10-21, captures 192 / 193).
    const AtmosphereFrame a = loadAtmosphere(f);
    const float w = length(position - f.cameraPosition) * a.kmPerUnit / a.aerialMaxKm;
    const float3 uvw = float3(c.xy / c.w * 0.5 + 0.5, w - 0.5 / 32.0);
    float4 air = gTextures3D[NonUniformResourceIndex(a.aerial)].SampleLevel(gSamplers[a.linearSampler], saturate(uvw), 0);
    air = lerp(float4(0.0, 0.0, 0.0, 1.0), air, saturate(w * 32.0 + 0.5));
    color = color * air.a + air.rgb * f.exposure;
  }
  if (f.volume == kNoTexture) return color;
  const float w = froxelSlice(f, c.w);
  // Each slice holds the fog up to its far side; in front of the first, it thins to none at the eye.
  const float slices = float(max(f.volumeSlices, 1u));
  const float3 uvw = float3(c.xy / c.w * 0.5 + 0.5, w - 0.5 / slices);
  float4 fog = gTextures3D[NonUniformResourceIndex(f.volume)].SampleLevel(gSamplers[f.volumeSampler], saturate(uvw), 0);
  fog = lerp(float4(0.0, 0.0, 0.0, 1.0), fog, saturate(w * slices + 0.5));
  return color * fog.a + fog.rgb;
}

// Atmosfera volumétrica v1 (volumeFlags bit 1): the fog's transmittance alone between the camera and a point, for
// light that adds itself over the scene (effects): it crosses the fog but brings no in-scatter of its own. 1 when
// no volume owns the frame.
// The volume read at a coordinate. DXC gotcha (2026-10-17, the same as volumeFarSample's): with the coordinate built
// or wrapped inline in the SampleLevel call, the read came back 0 and every spell effect (effects.hlsl, vfx.hlsl)
// vanished while the atmosphere was on; the sample lives in its own helper, the coordinate finished first.
float4 volumeSampleAt(MeshFrame f, float3 uvw) {
  return gTextures3D[NonUniformResourceIndex(f.volume)].SampleLevel(gSamplers[f.volumeSampler], uvw, 0);
}
float volumeTransmittance(MeshFrame f, float3 position) {
  if (f.volume == kNoTexture || (f.volumeFlags & 1u) == 0) return 1.0;
  const float4 c = mul(float4(position, 1.0), f.viewProjection);
  if (c.w <= 0.0) return 1.0;
  const float w = froxelSlice(f, c.w);
  const float slices = float(max(f.volumeSlices, 1u));
  float3 uvw = float3(c.xy / c.w * 0.5 + 0.5, w - 0.5 / slices);
  uvw = saturate(uvw);
  const float4 fog = volumeSampleAt(f, uvw);
  return lerp(1.0, fog.a, saturate(w * slices + 0.5));
}
// (volumeFlags bit 2) The fog all the way out, over what shows behind everything (the sky, a painted backdrop).
// The volume's far slice at a screen uv (the fog all the way out).
float4 volumeFarSample(MeshFrame f, float2 uv) {
  const float w = 1.0 - 0.5 / float(max(f.volumeSlices, 1u));
  return gTextures3D[NonUniformResourceIndex(f.volume)].SampleLevel(gSamplers[f.volumeSampler], float3(uv, w), 0);
}
// Debug views 15..19 (--debug-view N): the integrated volume at a screen uv, slices 5 / 90 / 93 / 97 / 99.5 %:
// (transmittance, scattered luminance x4, 1 where a component is NaN or inf, tested by bits: fast-math folds isnan).
static const uint kDebugVolumeFirst = 15u, kDebugVolumeLast = 19u;
float3 debugVolume(MeshFrame f, float2 uv) {
  if (f.volume == kNoTexture) return float3(0.0, 0.0, 1.0);
  const float ws[5] = {0.05, 0.9, 0.93, 0.97, 0.995};
  const float w = ws[clamp(f.debugView, kDebugVolumeFirst, kDebugVolumeLast) - kDebugVolumeFirst];
  const float4 s = gTextures3D[NonUniformResourceIndex(f.volume)].SampleLevel(gSamplers[f.volumeSampler], float3(uv, w), 0);
  const uint4 bits = asuint(s) & 0x7f800000u;
  return float3(s.a, dot(s.rgb, 0.333) * 4.0, any(bits == 0x7f800000u) ? 1.0 : 0.0);
}
// (volumeFlags bit 2) The fog all the way out, over what shows behind everything (the sky, a painted backdrop).
// DXC gotcha (2026-10-16): with the slice coordinate built inline in the float3 of the SampleLevel call, this read 0
// on the normal path (while the same coordinate read the fog in a debug branch): keep the sample in volumeFarSample.
float3 volumeOverBackground(MeshFrame f, float3 color, uint2 pixel) {
  if (f.debugView >= kDebugVolumeFirst && f.debugView <= kDebugVolumeLast) return debugVolume(f, (float2(pixel) + 0.5) / f.screenSize);
  if (f.volume == kNoTexture || (f.volumeFlags & 2u) == 0) return color;
  const float4 fog = volumeFarSample(f, (float2(pixel) + 0.5) / f.screenSize);
  return color * fog.a + fog.rgb;
}

// Debug view 3 (fog of war): mask darkness in red, local-light sight in green, blue = a surface was drawn.
static const uint kDebugFog = 3u;
// Dev Controls "Fog of war — debug" (Engine2 fogDebug): the fog's states as flat colours instead of the feathered
// mask: green = in sight, amber = explored, red = never seen (FogOfWar.h alphas 0 / 0.6 / 1).
static const uint kDebugFogStates = 4u;
float3 fogStateColor(float alpha) {
  return alpha < 0.3 ? float3(0.2, 0.8, 0.25) : alpha < 0.8 ? float3(0.95, 0.65, 0.15) : float3(0.85, 0.15, 0.1);
}

float3 shade(MeshFrame f, Surface s) {
  if (f.debugView >= kDebugVolumeFirst && f.debugView <= kDebugVolumeLast) {
    const float4 c = mul(float4(s.position, 1.0), f.viewProjection);
    return debugVolume(f, c.xy / c.w * 0.5 + 0.5);
  }
  const Sight sight = tacticalSight(f, s.sightAt);
  if (f.debugView == kDebugClouds) return cloudShadow(f, s.position).xxx;
  if (f.debugView == 21u) return probesAddress(f) != 0 ? float3(probeRoom(f, s.position), 0.0, 0.0) : float3(0.0, 0.0, 1.0);  // the room's gate: red = inside, black = outside, blue = no probes
  if (f.debugView == kDebugFog) return float3(1.0 - sight.color, sight.local, 1.0);
  if (f.debugView == kDebugFogStates) return fogStateColor(fogAlpha(f, s.sightAt));
  return applyVolume(f, shadeInSight(f, s, sight) * sight.color, s.position);
}

#endif
