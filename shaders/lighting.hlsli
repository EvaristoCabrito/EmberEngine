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

static const float kPi = 3.14159265;

// MeshFrame.lightingLayers: each layer can be switched off alone, for measuring it (Renderer::setLightingLayers).
static const uint kLayerSun = 1u;
static const uint kLayerShadows = 2u;
static const uint kLayerIndirect = 4u;

struct Surface {
  float3 position;         // world
  float3 geometricNormal;  // the triangle's (or the card's) own normal: shadow offsets
  float3 normal;           // shading normal (normal map applied), unit length, facing the viewer
  float3 albedo;           // linear
  float roughness;         // perceptual (alpha = roughness^2)
  float metallic;
  float occlusion;         // baked or screen-space ambient occlusion
  float3 emissive;         // linear, before exposure
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
  float lit = 0.0;
  const float radius = f.shadowFilterTexels / f.shadowSize;
  [unroll] for (uint i = 0; i < 16; ++i) lit += map.SampleCmpLevelZero(cmp, uvz.xy + kPoisson16[i] * radius, uvz.z);
  return lit / 16.0;
}

// --- Indirect light (the interface stays when probes and the sky arrive) --------------------------------

// Irradiance E arriving at the surface from everywhere but the direct lights.
float3 indirectDiffuse(MeshFrame f, Surface s) {
  return lerp(f.groundColor, f.skyColor, s.normal.y * 0.5 + 0.5) * f.ambientIntensity;
}

// Prefiltered radiance along the reflection direction (the hemisphere: uniform radiance E / pi).
float3 indirectSpecular(MeshFrame f, Surface s, float3 reflected) {
  return lerp(f.groundColor, f.skyColor, reflected.y * 0.5 + 0.5) * f.ambientIntensity / kPi;
}

// --- Tactical visibility (R7) -------------------------------------------------------------------------

// 1 = in sight. The fog-of-war mask plugs in here (unexplored = 0: nothing lit, nothing shown; remembered =
// darkened, no dynamic light); every light, bounce, fog and effect goes through it.
float visibility(MeshFrame f, float3 position) { return 1.0; }

// --- The surface's colour --------------------------------------------------------------------------------

float3 shade(MeshFrame f, Surface s) {
  const float3 view = normalize(f.cameraPosition - s.position);
  const float a = max(s.roughness * s.roughness, 0.002);
  const float3 diffuseColor = s.albedo * (1.0 - s.metallic);
  const float3 f0 = lerp(0.04, s.albedo, s.metallic);
  const float nv = max(dot(s.normal, view), 1e-4);

  float3 color = 0.0;
  if (f.lightingLayers & kLayerSun) {
    const float3 l = f.sunDirection, h = normalize(l + view);
    const float nl = saturate(dot(s.normal, l)), nh = saturate(dot(s.normal, h)), vh = saturate(dot(view, h));
    if (nl > 0.0) {
      const float3 F = fresnelSchlick(f0, vh);
      color += (diffuseColor * (1.0 - F) / kPi + F * ggx(nh, a) * smithVisibility(nv, nl, a)) * nl * f.sunColor * f.sunIntensity *
               sunShadow(f, s);
    }
  }
  if (f.lightingLayers & kLayerIndirect) {
    // Lambert returns albedo * E / pi; the specular lobe sees the environment's radiance through the
    // split-sum BRDF.
    color += (diffuseColor * indirectDiffuse(f, s) / kPi +
              envBrdfApprox(f0, s.roughness, nv) * indirectSpecular(f, s, reflect(-view, s.normal))) * s.occlusion;
  }
  color += s.emissive * f.exposure;
  return color * visibility(f, s.position);
}

#endif
