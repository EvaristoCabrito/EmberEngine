// The atmosphere (ARCHITECTURE §7.5): Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere Rendering
// Technique" (EGSR, doi 10.1111/cgf.14050), our own implementation. Distances in kilometres, Earth's air by
// default (Rayleigh, Mie, ozone). Lookup tables, all rebuilt every frame (atmosphere.hlsl):
//   transmittance   256 x 64   light surviving from a height to the top of the air, by zenith angle
//   multiScattering  32 x 32   second and higher orders of scattering (isotropic, a geometric series)
//   skyView         192 x 108  the sky seen from the board, by azimuth (relative to the sun) and zenith angle
//   aerial           32^3      in-scattered light and transmittance between the camera and a surface
#ifndef EMBER_ATMOSPHERE_HLSLI
#define EMBER_ATMOSPHERE_HLSLI

#include "lighting.hlsli"

static const uint2 kTransmittanceSize = uint2(256, 64);
static const uint2 kMultiScatteringSize = uint2(32, 32);
static const uint2 kSkyViewSize = uint2(192, 108);
static const uint kAerialSize = 32;

// --- Geometry ------------------------------------------------------------------------------------------

// Distance along a ray from `o` (planet-centred) to a sphere of radius r: the nearest hit in front, or -1.
float raySphere(float3 o, float3 d, float r) {
  const float b = dot(o, d), c = dot(o, o) - r * r, h = b * b - c;
  if (h < 0.0) return -1.0;
  const float s = sqrt(h), t0 = -b - s, t1 = -b + s;
  return t0 >= 0.0 ? t0 : (t1 >= 0.0 ? t1 : -1.0);
}

// --- The air at a height ---------------------------------------------------------------------------------

struct Medium {
  float3 scattering;   // total
  float3 extinction;
  float3 rayleigh;     // scattering by each
  float3 mie;
};

Medium mediumAt(AtmosphereFrame a, float height) {  // height above the ground, km
  const float r = exp(-height / a.rayleighScaleHeight), m = exp(-height / a.mieScaleHeight);
  const float o = max(0.0, 1.0 - abs(height - a.ozoneCenter) / (a.ozoneWidth * 0.5));
  Medium s;
  s.rayleigh = a.rayleighScattering * r;
  s.mie = a.mieScattering * m;
  s.scattering = s.rayleigh + s.mie;
  s.extinction = s.rayleigh + a.mieExtinction * m + a.ozoneAbsorption * o;
  return s;
}

float phaseRayleigh(float c) { return 3.0 / (16.0 * kPi) * (1.0 + c * c); }
float phaseMie(float c, float g) {  // Cornette-Shanks
  const float g2 = g * g;
  return 3.0 / (8.0 * kPi) * (1.0 - g2) * (1.0 + c * c) / ((2.0 + g2) * pow(max(1.0 + g2 - 2.0 * g * c, 1e-4), 1.5));
}

// --- Table parameterisations (Bruneton 2017 for transmittance; Hillaire 2020 for the others) ---------------

float2 transmittanceUv(AtmosphereFrame a, float r, float mu) {
  const float H = sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);
  const float rho = sqrt(max(r * r - a.bottomRadius * a.bottomRadius, 0.0));
  const float disc = r * r * (mu * mu - 1.0) + a.topRadius * a.topRadius;
  const float d = max(0.0, -r * mu + sqrt(max(disc, 0.0)));
  const float dMin = a.topRadius - r, dMax = rho + H;
  return float2((d - dMin) / (dMax - dMin), rho / H);
}
void transmittanceFromUv(AtmosphereFrame a, float2 uv, out float r, out float mu) {
  const float H = sqrt(a.topRadius * a.topRadius - a.bottomRadius * a.bottomRadius);
  const float rho = H * uv.y;
  r = sqrt(rho * rho + a.bottomRadius * a.bottomRadius);
  const float dMin = a.topRadius - r, dMax = rho + H, d = dMin + uv.x * (dMax - dMin);
  mu = d == 0.0 ? 1.0 : clamp((H * H - rho * rho - d * d) / (2.0 * r * d), -1.0, 1.0);
}

float3 transmittanceToTop(AtmosphereFrame a, float r, float mu) {
  return gTextures[NonUniformResourceIndex(a.transmittance)].SampleLevel(gSamplers[a.linearSampler], transmittanceUv(a, r, mu), 0).rgb;
}

float3 multiScatteringAt(AtmosphereFrame a, float r, float muSun) {
  const float2 uv = saturate(float2(muSun * 0.5 + 0.5, (r - a.bottomRadius) / (a.topRadius - a.bottomRadius)));
  const float2 texel = (uv * (float2(kMultiScatteringSize) - 1.0) + 0.5) / float2(kMultiScatteringSize);
  return gTextures[NonUniformResourceIndex(a.multiScattering)].SampleLevel(gSamplers[a.linearSampler], texel, 0).rgb;
}

// Sky view: u = azimuth from the sun (squared, finer near it), v = zenith angle (finer near the horizon).
void skyViewFromUv(AtmosphereFrame a, float2 uv, out float viewZenith, out float azimuthCos) {
  const float vHorizon = sqrt(max(a.viewHeight * a.viewHeight - a.bottomRadius * a.bottomRadius, 0.0));
  const float beta = acos(vHorizon / a.viewHeight), zenithHorizon = kPi - beta;
  if (uv.y < 0.5) {
    float c = 1.0 - 2.0 * uv.y;
    c = 1.0 - c * c;
    viewZenith = zenithHorizon * c;
  } else {
    float c = uv.y * 2.0 - 1.0;
    c *= c;
    viewZenith = zenithHorizon + beta * c;
  }
  const float u = uv.x * uv.x;
  azimuthCos = -(u * 2.0 - 1.0);
}
float2 skyViewUv(AtmosphereFrame a, float viewZenith, float azimuthCos) {
  const float vHorizon = sqrt(max(a.viewHeight * a.viewHeight - a.bottomRadius * a.bottomRadius, 0.0));
  const float beta = acos(vHorizon / a.viewHeight), zenithHorizon = kPi - beta;
  float v;
  if (viewZenith < zenithHorizon) {
    float c = viewZenith / zenithHorizon;
    c = 1.0 - c;
    c = sqrt(max(c, 0.0));
    v = 0.5 * (1.0 - c);
  } else {
    float c = (viewZenith - zenithHorizon) / beta;
    c = sqrt(max(c, 0.0));
    v = 0.5 + 0.5 * c;
  }
  const float u = sqrt(saturate(-azimuthCos * 0.5 + 0.5));
  // half-texel inset so the bilinear filter never reads across the table's edge
  return (float2(u, v) * (float2(kSkyViewSize) - 1.0) + 0.5) / float2(kSkyViewSize);
}

// The sky's radiance in a world direction (world +y up), before the key light's strength is applied in
// sunIlluminance (already in): what CSSkyView stored.
float3 skyRadiance(AtmosphereFrame a, float3 dir) {
  const float viewZenith = acos(clamp(dir.y, -1.0, 1.0));
  const float2 dh = normalize(dir.xz + float2(1e-6, 0.0)), sh = normalize(a.sunDirection.xz + float2(1e-6, 0.0));
  const float azimuthCos = dot(dh, sh);
  return gTextures[NonUniformResourceIndex(a.skyView)].SampleLevel(gSamplers[a.linearSampler], skyViewUv(a, viewZenith, azimuthCos), 0).rgb;
}

// What shows where nothing was drawn: the sky (outdoors), a small sun disc, and stars once the sun is down.
float3 skyBackground(MeshFrame f, uint2 pixel) {
  const AtmosphereFrame a = loadAtmosphere(f);
  const float2 uv = (float2(pixel) + 0.5) / f.screenSize;
  const float4 h = mul(float4(uv * 2.0 - 1.0, 1.0, 1.0), a.inverseViewProjection);
  const float3 dir = normalize(h.xyz / h.w - f.cameraPosition);
  float3 radiance = skyRadiance(a, dir) * a.skyScale;
  if (dir.y > 0.0) {
    // The sun's disc (0.53 degrees across), seen through the air.
    const float c = dot(dir, a.sunDirection);
    if (c > 0.99998918 && a.sunDirection.y > -0.02)
      radiance += a.sunIlluminance * transmittanceToTop(a, a.viewHeight, dir.y) * 20.0;
    // Stars: a sparse field fixed to the sky, fading in as the sun goes down.
    const float night = 1.0 - smoothstep(-0.12, 0.02, a.sunDirection.y);
    if (night > 0.0) {
      const float3 cell = floor(dir * 300.0);
      const float n = frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
      if (n > 0.9975) radiance += (n - 0.9975) * 400.0 * night * transmittanceToTop(a, a.viewHeight, dir.y);
    }
  }
  return radiance * f.exposure;
}

#endif
