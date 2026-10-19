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

// The view direction through a pixel (centre at +0.5).
float3 skyDirection(MeshFrame f, AtmosphereFrame a, float2 pixel) {
  const float2 uv = pixel / f.screenSize;
  const float4 h = mul(float4(uv * 2.0 - 1.0, 1.0, 1.0), a.inverseViewProjection);
  return normalize(h.xyz / h.w - f.cameraPosition);
}

// The star field's saved looks (looks/stars/README.md; never edited, a change is a new look): the stars' radiance
// in `dir` before the air's transmittance. `pixelAngle`: one pixel's width (radians).
float starHash(float3 cell) { return frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453); }
float3 starField(uint look, float3 dir, float pixelAngle) {
  if (look == 1) {  // 001 (de72f3c): one star in 0.25% of the cells of a 300-per-unit grid, each a whole cell (squares)
    const float n = starHash(floor(dir * 300.0));
    return n > 0.9975 ? (n - 0.9975) * 400.0 : 0.0;
  }
  if (look == 2) {  // 002: the same stars as points about a pixel wide at a random spot in their cells
    const float3 cell = floor(dir * 300.0);
    const float n = starHash(cell);
    if (n <= 0.9975) return 0.0;
    const float3 jitter = frac(sin(cell.yzx * float3(91.17, 47.53, 13.71) + n * 113.0) * 24634.6345);
    const float d = length(dir - normalize((cell + 0.2 + 0.6 * jitter) / 300.0)) / max(pixelAngle, 1e-5);
    return (n - 0.9975) * 1200.0 * exp(-d * d * 1.5);
  }
  // 003: dense (2% of a 400-per-unit grid, ~15x 002), brightness spread like a real sky (most faint, a few bright)
  // and colour from blue-white to orange; points about a pixel wide.
  const float3 cell = floor(dir * 400.0);
  const float n = starHash(cell);
  if (n <= 0.98) return 0.0;
  const float r = (n - 0.98) / 0.02;
  const float3 jitter = frac(sin(cell.yzx * float3(91.17, 47.53, 13.71) + n * 113.0) * 24634.6345);
  const float d = length(dir - normalize((cell + 0.2 + 0.6 * jitter) / 400.0)) / max(pixelAngle, 1e-5);
  const float t = frac(jitter.x * 7.31 + jitter.z * 3.17);
  const float3 tint = t < 0.5 ? lerp(float3(0.75, 0.85, 1.15), 1.0, t * 2.0) : lerp(1.0, float3(1.15, 0.9, 0.7), t * 2.0 - 1.0);
  return tint * (0.12 + 3.5 * pow(r, 5.0)) * exp(-d * d * 1.5);
}

// --- The real night sky (star look 4 "real"; render/StarSky.h, content/sky/stars.json) -------------------------------------
// The catalogue's stars as world directions (GpuStar: float4 direction + magnitude, float4 colour), each a point about a
// pixel wide whose peak follows its magnitude (2.512 x per step) in its own colour, the brightest with a soft glow; under them
// look 003's dim random field, faint, for the thousands of stars no catalogue of 220 holds; the Milky Way as a band about
// the galactic plane, brightest towards the galactic centre (the bulge), broken by the dust lanes (the Great Rift); the two
// Magellanic clouds. Everything in the sky's raw radiance (before exposure), scaled by starScale (a magnitude-0 star's peak).
static const uint kStarsOffset = 400u, kStarCountOffset = 408u, kStarScaleOffset = 412u, kGalaxyPoleOffset = 416u,
                  kMilkyWayOffset = 428u, kGalaxyCentreOffset = 432u, kMagellanicOffset = 448u;
float3 realStars(uint64_t atmosphere, float3 dir, float pixelAngle) {
  const uint64_t stars = vk::RawBufferLoad<uint64_t>(atmosphere + kStarsOffset);
  const uint count = vk::RawBufferLoad<uint>(atmosphere + kStarCountOffset);
  const float scale = vk::RawBufferLoad<float>(atmosphere + kStarScaleOffset);
  float3 radiance = 0.0;
  if (stars != 0) {
    // A star's image: a Gaussian about a pixel wide (the lens), wider for the bright ones (the eye's glare), its peak at the
    // magnitude's brightness so a faint star stays a faint point and a bright one a small blazing disc.
    const float sigma = max(pixelAngle * 0.9, 0.0005);
    const float cosLimit = cos(sigma * 14.0);
    for (uint i = 0; i < count; ++i) {
      const float4 s = vk::RawBufferLoad<float4>(stars + i * 32u);
      const float c = dot(dir, s.xyz);
      if (c < cosLimit) continue;
      const float4 colour = vk::RawBufferLoad<float4>(stars + i * 32u + 16u);
      const float brightness = scale * pow(2.512, -s.w);
      const float angle = acos(clamp(c, -1.0, 1.0));
      // The eye's glare round the bright ones: a wider, fainter Gaussian growing with the brightness (Sirius a small blaze,
      // a third-magnitude star a bare point).
      const float bright = saturate((2.5 - s.w) / 4.0);
      const float glow = sigma * (1.5 + 4.0 * bright);
      radiance += colour.rgb * brightness * (exp(-angle * angle / (sigma * sigma)) + 0.3 * bright * exp(-angle * angle / (glow * glow)));
    }
  }
  // The dim field under the catalogue (look 003 at a tenth), the faint stars no one names.
  radiance += starField(3u, dir, pixelAngle) * 0.1;
  // The Milky Way: galactic latitude b (from the pole) and longitude l (from the centre, about the pole).
  const float milky = vk::RawBufferLoad<float>(atmosphere + kMilkyWayOffset);
  if (milky > 0.0) {
    const float3 pole = vk::RawBufferLoad<float3>(atmosphere + kGalaxyPoleOffset);
    const float3 centre = vk::RawBufferLoad<float3>(atmosphere + kGalaxyCentreOffset);
    const float sinB = dot(dir, pole);
    const float b = asin(clamp(sinB, -1.0, 1.0));
    const float3 inPlane = normalize(dir - pole * sinB + 1e-5);
    const float3 centreInPlane = normalize(centre - pole * dot(centre, pole) + 1e-5);
    const float l = atan2(dot(cross(centreInPlane, inPlane), pole), dot(centreInPlane, inPlane));  // -pi .. pi
    // The band: a thin bright core in a wider halo, both thicker and brighter towards the centre (the bulge).
    const float toCentre = exp(-l * l / 1.6);
    const float width = radians(6.0) * (1.0 + 1.2 * toCentre);
    float band = exp(-b * b / (width * width)) * (0.5 + 1.5 * toCentre) + 0.35 * exp(-b * b / (width * width * 9.0)) * (0.5 + toCentre);
    // Its texture: patches of star clouds, and the dust lanes (the Great Rift from Cygnus to Centaurus, below the centre).
    const float2 g = float2(l * 8.0, b * 14.0);
    const float clouds = 0.55 + 0.9 * cloudNoise(g * 1.7 + 3.1) * cloudNoise(g * 0.7 + 9.4);
    const float rift = smoothstep(0.35, 0.6, cloudNoise(g * 1.1 + float2(21.0, 4.0))) * smoothstep(radians(20.0), 0.0, abs(b - radians(1.5))) *
                       smoothstep(-2.2, -0.2, l) * smoothstep(1.4, 0.3, l);
    band *= clouds * (1.0 - 0.75 * rift);
    radiance += band * milky * scale * 0.03 * float3(0.92, 0.9, 1.0);
    // The Magellanic clouds: two soft patches of the same light.
    [unroll] for (uint k = 0; k < 2; ++k) {
      const float4 m = vk::RawBufferLoad<float4>(atmosphere + kMagellanicOffset + k * 16u);
      if (m.w <= 0.0) continue;
      const float angle = acos(clamp(dot(dir, m.xyz), -1.0, 1.0));
      const float patch = exp(-angle * angle / (m.w * m.w)) * (0.6 + 0.8 * cloudNoise(dir.xz * 40.0 + dir.y * 17.0 + k * 5.0));
      radiance += patch * milky * scale * (k == 0 ? 0.03 : 0.02) * float3(0.95, 0.93, 1.0);
    }
  }
  return radiance;
}

// The moon as a light in the air: the Mie aureole round it (the forward lobe on the air's optical depth along the view, the
// same law as the sun's) and the glow at the disc's rim. The user, 2026-10-19: "a lua não aparenta emanar a luz".
float3 moonAureole(AtmosphereFrame a, float3 dir, float3 moonDirection, float3 moonLight, float moonTanRadius) {
  const float3 air = transmittanceToTop(a, a.viewHeight, dir.y);
  const float mieDepth = dot(a.mieScattering, 1.0 / 3.0) * a.mieScaleHeight / max(dir.y, 0.08);
  const float c = dot(dir, moonDirection);
  float3 glow = moonLight * air * mieDepth * phaseMie(c, 0.96) * 0.6;
  if (moonTanRadius > 0.0) {
    const float angle = acos(clamp(c, -1.0, 1.0)), r = atan(moonTanRadius);
    if (angle > r) glow += moonLight * air * 0.02 * (r * r) / (angle * angle);
  }
  return glow;
}

// The moon's disc over `radiance`: tonight's phase art (Engine2's icons: the disc fills 80% of the image), upright
// in the sky, its brightness the art x glow, seen through the air (`air`: transmittance towards it). `pixelAngle`:
// one pixel's width (radians). Takes the moon's fields one by one: with the AtmosphereFrame passed by value, DXC
// (Vulkan SDK 1.4.363.0) read moonTexture wrong inside the function (2026-10-15, found with raw buffer probes).
float3 moonDisc(float3 dir, float pixelAngle, float3 radiance, float3 moonDirection, uint moonTexture, float tanRadius,
                float glow, uint linearSampler, float3 air) {
  const float c = dot(dir, moonDirection);
  if (moonTexture == kNoTexture || c <= 0.0) return radiance;
  const float3 right = normalize(cross(moonDirection, float3(0.0, 1.0, 0.0)));
  const float3 up = cross(right, moonDirection);
  const float2 p = float2(dot(dir, right), dot(dir, up)) / (c * tanRadius);  // +-1 on the disc's edge
  if (any(abs(p) > 1.25)) return radiance;
  uint width, height;
  gTextures[NonUniformResourceIndex(moonTexture)].GetDimensions(width, height);
  const float lod = log2(max(pixelAngle * 0.4 * float(width) / tanRadius, 1e-4));
  const float4 art = gTextures[NonUniformResourceIndex(moonTexture)].SampleLevel(gSamplers[linearSampler],
                                                                                 0.5 + float2(p.x, -p.y) * 0.4, max(lod, 0.0));
  // Only the lit part of the moon covers the sky: the art paints the dark side opaque black, which read as a black disc on a
  // bright dusk sky (the user, 2026-10-19: "esse sol negro"); in the real world the dark side vanishes against the sky.
  const float lit = smoothstep(0.0, 0.12, dot(art.rgb, float3(0.2126, 0.7152, 0.0722)));
  return lerp(radiance, art.rgb * glow * air, art.a * lit);
}

// Nuvens básicas (clouds.hlsli; the user: "tem que ser boa"): two cloud sheets seen from below along the ray from
// `origin` towards `dir` (world): a high, thin, fast layer (cirrus: 1.6 x the altitude, finer, 60% of the coverage) behind
// a lower, thicker one (cumulus). Per sheet:
//   shape    the cover field (clouds.hlsli), its edges eroded by a finer noise so puffs break up instead of blurring;
//            the view's path through the sheet lengthens at grazing angles (thicker towards the horizon)
//   light    the key light on the sheet's top (its colour through the air above the board), self-shadowed by the cloud
//            between the point and the light (two taps along the light across the sheet: lit rims, dark cores), scattered
//            with a two-lobe phase (bright "silver" rims towards the light, a little back-scatter), a powder term (thin
//            edges darker than their middles), and the bottom darkening with the thickness above it; the fill light is
//            the sky's irradiance from above (through the sheet) and the lit ground's from below
//   air      distant clouds fade into the horizon's own colour (the sky radiance there), like everything far away
// Returns the cloud radiance (raw: before skyScale and exposure) and how much of the sky it covers; `clear` = the sky's
// radiance in `dir` (the haze colour). The caller composites it over the sky, its stars and its moon, so thin cloud veils
// the moon and a thick one hides the stars.
struct CloudHit {
  float3 color;
  float alpha;
};
CloudHit cloudSheet(MeshFrame f, AtmosphereFrame a, CloudParams c, float3 origin, float3 dir, float3 clear) {
  CloudHit none;
  none.color = 0.0;
  none.alpha = 0.0;
  const float t = (c.altitude - origin.y) / dir.y;
  if (t <= 0.0) return none;
  const float2 hit = origin.xz + dir.xz * t;
  const float cover = cloudCoverEroded(c, f.time, hit);
  if (cover <= 0.001) return none;
  const float thickness = c.thickness;
  // Optical depth: vertical through the sheet, longer along a slanted view.
  const float tau = cover * thickness;
  const float tauView = tau / max(dir.y, 0.25);
  const float horizon = smoothstep(0.0, 0.08, dir.y);
  float alpha = (1.0 - exp(-tauView)) * horizon;
  if (alpha <= 0.002) return none;

  // The key light on the sheet's top and through the cloud towards it.
  const float3 sun = a.sunDirection;
  const float3 keyOnTop = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, sun.y) * saturate(sun.y * 6.0 + 0.3);
  float selfShadow = 1.0;
  if (sun.y > 0.05) {
    const float2 step = sun.xz / max(sun.y, 0.2) * (c.scale * 0.12);
    const float ahead = cloudCover(c, f.time, hit + step) * 0.6 + cloudCover(c, f.time, hit + step * 2.2) * 0.4;
    selfShadow = exp(-ahead * thickness * 0.9);
  }
  const float cosTheta = dot(dir, sun);
  const float phase = (0.72 * phaseHG(cosTheta, 0.62) + 0.28 * phaseHG(cosTheta, -0.18)) * 4.0 * kPi;  // 1 = isotropic
  const float powder = 1.0 - exp(-tau * 2.5);
  const float bottom = exp(-tau * 0.5);  // the light that gets down through the sheet to its underside
  // Fill light: the sky from above (through the sheet) and the lit ground from below (CSAmbient's raw up irradiance).
  const float3 skyUp = float3(vk::RawBufferLoad<float4>(a.ambient + 16).w, vk::RawBufferLoad<float4>(a.ambient + 32).w,
                              vk::RawBufferLoad<float4>(a.ambient + 48).w);
  const float3 sunOnGround = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, sun.y) * saturate(sun.y);
  const float3 groundUp = a.groundAlbedo * (skyUp + sunOnGround);  // irradiance on an underside from a Lambert ground
  const float3 fill = skyUp * lerp(0.55, 0.22, saturate(tau / 3.2)) + groundUp * 0.5;
  const float3 albedo = 0.94;
  // Direct light scattered towards the eye (phase x multiple-scatter lift), then the fill (Lambert).
  float3 direct = keyOnTop * selfShadow * phase * lerp(0.8, 1.35, powder) * bottom * 0.45;
  // The 24 h cycle's moon lights the sheet too (moonlit clouds at night).
  const float3 moonLight = atmoMoonLight(f.atmosphere);
  if (dot(moonLight, 1.0) > 0.0) {
    const float3 moon = atmoMoonDirection(f.atmosphere);
    const float3 moonOnTop = moonLight * transmittanceToTop(a, a.viewHeight, moon.y) * saturate(moon.y * 6.0 + 0.3);
    const float cm = dot(dir, moon);
    const float phaseMoon = (0.72 * phaseHG(cm, 0.62) + 0.28 * phaseHG(cm, -0.18)) * 4.0 * kPi;
    direct += moonOnTop * phaseMoon * lerp(0.8, 1.35, powder) * bottom * 0.45;
  }
  float3 color = albedo * (direct + fill / kPi);
  // Distance haze: towards the horizon the cloud takes the sky's own colour there.
  const float haze = 1.0 - exp(-t / 7000.0);
  color = lerp(color, clear, haze);
  CloudHit h;
  h.color = color;
  h.alpha = alpha;
  return h;
}
// Both sheets over the sky radiance `sky` (which may already hold the sun's disc, the stars and the moon): the high
// sheet first, the low one over it.
float3 cloudsOverSky(MeshFrame f, AtmosphereFrame a, float3 origin, float3 dir, float3 clear, float3 sky) {
  if ((f.cloudFlags & kCloudLayer) == 0 || (f.cloudFlags & kCloudVolumetric) != 0 || dir.y <= 0.002) return sky;
  const CloudParams low = loadCloudParams(f);
  const CloudHit highHit = cloudSheet(f, a, highSheet(low), origin, dir, clear);
  sky = lerp(sky, highHit.color, highHit.alpha);
  const CloudHit lowHit = cloudSheet(f, a, low, origin, dir, clear);
  return lerp(sky, lowHit.color, lowHit.alpha);
}

// Sol, look 001 (2026-10-19; the user: "não tem nem sol", "um sol digno de um triple A"): the sun as a camera sees it.
//   disc     the real radiance of the photosphere's disc (illuminance / its solid angle: it saturates to white), darker
//            towards its limb (the real sun's limb darkening), through the air at that elevation (red at the horizon)
//   corona   a bright rim falling off as 1 / angle^2 out to a few radii (what the lens and the eye add round a source that
//            bright), in the sun's own colour
//   aureole  the air's forward scattering towards the sun (a narrow Mie lobe, g 0.96, on the Mie optical depth along the
//            view: the sky-view table carries only the wide g 0.8 lobe), wide and soft, strongest low in the sky where the
//            air is thickest and the light reddest: the glow that fills the sky round the sun at dusk
// `r` = the disc's angular radius. The 24 h cycle's moon takes the same aureole (its own light) when it is the sky's second
// light; at night the moon's disc (moonDisc) gets a soft halo of its own.
// (the fields after the frame's uint64 passed one by one: the moonTexture gotcha)
float3 sunLook001(AtmosphereFrame a, float3 dir, float pixelAngle, uint sunDisc, float sunTanRadius, float3 moonDirection, float3 moonLight) {
  float3 radiance = 0.0;
  const float3 air = transmittanceToTop(a, a.viewHeight, dir.y);
  const float mieDepth = dot(a.mieScattering, 1.0 / 3.0) * a.mieScaleHeight / max(dir.y, 0.08);  // optical depth along the view
  if (sunDisc != 0 && a.sunDirection.y > -0.03) {
    const float c = dot(dir, a.sunDirection);
    const float angle = acos(clamp(c, -1.0, 1.0));
    const float r = sunTanRadius > 0.0 ? atan(sunTanRadius) : 0.00465;
    const float3 sunLight = a.sunIlluminance * air;
    // The disc: a white core (30 x the light: it saturates and blooms) darkening towards its rim (Eddington's limb darkening,
    // 1 - u (1 - mu), u 0.6, steepened so the rim reads as a warm edge instead of burning out with the core).
    const float u = saturate(angle / r);
    const float limb = 1.0 - 0.6 * (1.0 - sqrt(max(1.0 - u * u, 0.0)));
    const float disc = 1.0 - smoothstep(r - pixelAngle, r + pixelAngle, angle);
    radiance += sunLight * (2.0 + 28.0 * limb * limb * limb * limb) * disc;
    // The corona: 1 / angle^2 beyond the rim, held at the rim's own brightness.
    const float rim = r * 1.15;
    const float corona = angle > rim ? (rim * rim) / (angle * angle) : 1.0;
    radiance += sunLight * 1.2 * corona * (1.0 - disc * 0.85);
    // The aureole: the Mie lobe towards the sun, on the air's optical depth along the view.
    radiance += sunLight * mieDepth * phaseMie(c, 0.96) * 0.45 * (1.0 - disc);
  }
  if (dot(moonLight, 1.0) > 0.0 && moonDirection.y > -0.03) {
    const float cm = dot(dir, moonDirection);
    radiance += moonLight * air * mieDepth * phaseMie(cm, 0.96) * 0.45;
  }
  return radiance;
}

// Nuvens volumétricas (cloudsvol.hlsl CSMarch, once a frame at half resolution): the march at this pixel of the screen,
// rgb = radiance (raw), a = transmittance. (the sample in its own helper: the DXC gotcha)
float4 cloudVolAt(MeshFrame f, float2 uv) {
  const uint march = vk::RawBufferLoad<uint>(f.atmosphere + kVolMarchOffset);
  if (march == kNoTexture) return float4(0.0, 0.0, 0.0, 1.0);
  // A 3 x 3 Gaussian over the half-resolution march (the dithered march's grain smoothed away; a cloud has no edge sharper
  // than this).
  const float2 texel = vk::RawBufferLoad<float2>(f.atmosphere + kVolMarchSizeOffset);
  const float2 t = 1.0 / max(texel, 1.0);
  float4 sum = 0.0;
  [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x) {
      const float w = (x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0);
      sum += gTextures[NonUniformResourceIndex(march)].SampleLevel(gSamplers[f.volumeSampler], uv + float2(x, y) * t, 0) * w;
    }
  return sum / 16.0;
}

// What shows where nothing was drawn: the sky (outdoors), the sun's disc by day, stars at night (or once the sun is
// down) and the moon's disc, the clouds over them.
float3 skyBackground(MeshFrame f, uint2 pixel) {
  const AtmosphereFrame a = loadAtmosphere(f);
  const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
  const float3 clear = skyRadiance(a, dir);
  float3 radiance = clear * a.skyScale;
  if (dir.y > 0.0) {
    const float pixelAngle = length(skyDirection(f, a, float2(pixel) + float2(1.5, 0.5)) - dir);
    const uint sunLook = vk::RawBufferLoad<uint>(f.atmosphere + kSunLookOffset);
    // The sun's disc, seen through the air: the real 0.53 degrees across, or the panel's "Sol: cinema" size (sunTanRadius,
    // the moon's small disc: the real one is a dot the player never finds) with a soft corona around it; or look 001.
    const float c = dot(dir, a.sunDirection);
    if (sunLook == 1u) {
      radiance += sunLook001(a, dir, pixelAngle, a.sunDisc, a.sunTanRadius, atmoMoonDirection(f.atmosphere), atmoMoonLight(f.atmosphere)) * a.skyScale;
    } else if (a.sunDisc != 0 && a.sunDirection.y > -0.02) {
      const float angle = acos(clamp(c, -1.0, 1.0));
      const float r = a.sunTanRadius > 0.0 ? atan(a.sunTanRadius) : 0.00465;
      const float3 sunLight = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, dir.y);
      const float disc = 1.0 - smoothstep(r - pixelAngle, r + pixelAngle, angle);
      radiance += sunLight * 20.0 * disc;
      if (a.sunTanRadius > 0.0) radiance += sunLight * 0.06 * exp(-angle / (r * 2.5));
    }
    // The moon as a light in the air (always: physics, not a look): its aureole and the glow at its rim. At night the key light
    // is the moon (sunDisc 0: a.sunIlluminance is its light, a.sunDirection its disc since 2026-10-19); in the 24 h cycle it is
    // the sky's second light (moonIlluminance).
    {
      const float3 moonLight = a.sunDisc == 0 ? a.sunIlluminance : atmoMoonLight(f.atmosphere);
      const float3 moonDir = a.moonTexture != kNoTexture ? atmoMoonDirection(f.atmosphere) : a.sunDirection;
      if (dot(moonLight, 1.0) > 0.0 && moonDir.y > -0.05)
        radiance += moonAureole(a, dir, moonDir, moonLight, a.moonTexture != kNoTexture ? a.moonTanRadius : 0.0) * a.skyScale;
    }
    // Stars, fixed to the sky: at night, or fading in as the sun goes down (the saved look in starLook; 4 = the real sky).
    const float night = max(a.stars, a.sunDisc != 0 ? 1.0 - smoothstep(-0.12, 0.02, a.sunDirection.y) : 0.0);
    if (night > 0.0) {
      const float3 air = transmittanceToTop(a, a.viewHeight, dir.y);
      if (a.starLook == 4u) {
        // Under the bright "Luar: cinema" moon the sky would drown the catalogue's stars: they rise with the moon's light, at
        // most 6 x (the eye sees more stars than a camera metering a moonlit ground does).
        const float boost = a.sunDisc != 0 ? 1.0 : clamp(dot(a.sunIlluminance, float3(0.2126, 0.7152, 0.0722)) * 0.4, 1.0, 6.0);
        radiance += realStars(f.atmosphere, dir, pixelAngle) * night * boost * air * a.skyScale;
      } else {
        // The star looks were tuned against the dim moonlit sky of light mode 2; under a brighter moon ("Luar: cinema") the
        // sky would drown them, so they rise with the key light's strength, at most 12 x.
        const float starBoost = a.sunDisc != 0 ? 1.0 : clamp(dot(a.sunIlluminance, float3(0.2126, 0.7152, 0.0722)) * 0.6, 1.0, 12.0);
        radiance += starField(a.starLook, dir, pixelAngle) * night * starBoost * air;
      }
    }
    if (a.moonTexture != kNoTexture) {
      radiance = moonDisc(dir, pixelAngle, radiance, a.moonDirection, a.moonTexture, a.moonTanRadius, a.moonGlow, a.linearSampler,
                          transmittanceToTop(a, a.viewHeight, dir.y));
    }
    // The clouds last, over the stars and the moon (a thin veil over the moon, a thick cloud hiding the stars): the volumetric
    // layer marched this frame (cloudsvol.hlsl), or the sheets' light, on the sky's own scale.
    if (f.cloudFlags & kCloudVolumetric) {
      const float4 cloud = cloudVolAt(f, (float2(pixel) + 0.5) / f.screenSize);
      radiance = radiance * cloud.a + cloud.rgb;
    } else {
      radiance = cloudsOverSky(f, a, f.cameraPosition, dir, clear * a.skyScale, radiance);
    }
  }
  return radiance * f.exposure;
}

#endif
