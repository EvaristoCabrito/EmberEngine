// Nuvens volumétricas, look 001 (2026-10-19; the user: "um céu inteiro com 24 horas de transição com sol, lua, tudo que
// existe na atmosfera ... triple AAA", "cadê o céu do Final Fantasy XV?"): the lit march through the cloud layer whose
// density field lives in clouds.hlsli (cloudVolDensity: Perlin-Worley base shaped by the weather, Worley detail eroding the
// edges, the Horizon Zero Dawn / FFXV recipe, Schneider 2015). Ray marched from the camera, lit by the sun (and the 24 h
// cycle's moon) through the cloud (a short march towards the light: dark cores, lit rims, the powder darkening of thin
// edges) with the multiple-scattering octaves of Wrenninge (bright silver linings), the sky's own irradiance as the fill
// (dark bases, bright tops), hazed into the horizon with the distance.
//   cloudVolMarch()  the lit layer along a ray: radiance (raw: before skyScale and exposure) and transmittance
// Needs the sky tables (atmosphere.hlsli); the density alone does not (clouds.hlsli: the shadows on the board read it).
#ifndef EMBER_CLOUDSVOL_HLSLI
#define EMBER_CLOUDSVOL_HLSLI

#include "atmosphere.hlsli"

// Light reaching `p` through the cloud from direction `toLight`: a short march (five taps across the layer, the far ones
// cheap), exponential in the optical depth.
// The taps lie at growing distances (short first: the edge towards the light decides the lighting; each weighs the gap it
// stands for) and start at a per-pixel offset: taps at fixed distances shaded the cloud in bands (a tap crossing the top
// edge jumped the shadow: horizontal stripes under a high sun, 2026-10-19).
float cloudVolLightDepth(CloudVolume v, float time, float3 p, float3 toLight, float dither) {
  const float layer = max(v.top - v.base, 1.0);
  static const float kAt[6] = {0.02, 0.05, 0.1, 0.18, 0.3, 0.5};
  static const float kGap[6] = {0.02, 0.03, 0.05, 0.1, 0.16, 0.3};
  float depth = 0.0;
  [unroll] for (int i = 0; i < 6; ++i) {
    const float t = layer * (kAt[i] + kGap[i] * (dither - 0.5));
    depth += cloudVolDensity(v, time, p + toLight * t, true) * layer * kGap[i];
  }
  depth += cloudVolDensity(v, time, p + toLight * layer * 1.3, true) * layer * 0.5;  // the rest of the layer above
  return depth * kCloudVolExtinction;
}

// Wrenninge's multiple-scattering approximation: the single-scatter lobe repeated with attenuation, eccentricity and
// contribution halved each octave (bright edges where the light leaks through). The phase is the raw HG (it integrates to 1
// over the sphere: the slab integration below supplies the extinction), the octaves lift a dense cloud towards a white
// Lambert surface (a sunlit top about E / pi).
float cloudVolScatter(float lightDepth, float cosTheta) {
  float sum = 0.0;
  float a = 1.0, b = 1.0, c = 1.0;
  [unroll] for (int o = 0; o < 3; ++o) {
    const float phase = lerp(phaseHG(cosTheta, 0.7 * c), phaseHG(cosTheta, -0.25 * c), 0.35);
    sum += b * phase * exp(-lightDepth * a);
    a *= 0.5;
    b *= 0.5;
    c *= 0.5;
  }
  return sum * 2.2;
}

struct CloudVolHit {
  float3 color;        // radiance (raw: the sky's scale, before exposure)
  float transmittance; // 1 = nothing in the way
  float depth;         // distance to the cloud's mean (0 = no cloud)
};

// The lit layer along the ray from `origin` towards `dir`, `steps` samples through the layer (fewer for the reflection map).
// `clear` = the sky's radiance in `dir` on the sky's scale (what the far clouds haze into).
// `dither` (0..1): a fixed per-pixel offset of the first step (interleaved gradient noise of the pixel: the same every frame,
// so nothing boils), which turns the march's slabs (horizontal bands across a cloud, the user's "degraus") into fine grain the
// half-resolution upsample smooths; 0.5 for the reflection map.
CloudVolHit cloudVolMarch(MeshFrame f, AtmosphereFrame a, CloudVolume v, float3 origin, float3 dir, float3 clear, uint steps, float dither) {
  CloudVolHit none;
  none.color = 0.0;
  none.transmittance = 1.0;
  none.depth = 0.0;
  if (dir.y <= 0.004 || origin.y >= v.top) return none;
  // The slab: from the base (or the camera) to the top, capped where the weather has cleared (cloudWeather: the field thins
  // with the distance from the world's origin, so the horizon band keeps its clearings for the low sun, the moon and the
  // stars; a per-row thinning tried before sheared the clouds into "curtains").
  const float tBase = max((v.base - origin.y) / dir.y, 0.0);
  const float tTop = (v.top - origin.y) / dir.y;
  static const float kFar = 9000.0;
  const float t0 = tBase, t1 = min(tTop, kFar);
  if (t1 <= t0) return none;
  const float3 sun = a.sunDirection;
  const float3 moon = atmoMoonDirection(f.atmosphere);
  const float3 moonLight = atmoMoonLight(f.atmosphere);
  const bool moonOn = dot(moonLight, 1.0) > 0.0 && moon.y > -0.1;
  // The lights on top of the layer (through the air above the board), fading as they set.
  const float3 sunOnTop = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, sun.y) * saturate(sun.y * 8.0 + 0.2);
  const float3 moonOnTop = moonOn ? moonLight * transmittanceToTop(a, a.viewHeight, moon.y) * saturate(moon.y * 8.0 + 0.2) : 0.0;
  // Fill light: the sky from above (the raw up irradiance CSAmbient stored), the lit ground from below; top bright, base dark.
  const float3 skyUp = float3(vk::RawBufferLoad<float4>(a.ambient + 16).w, vk::RawBufferLoad<float4>(a.ambient + 32).w,
                              vk::RawBufferLoad<float4>(a.ambient + 48).w);
  const float3 sunOnGround = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, sun.y) * saturate(sun.y);
  const float3 groundUp = a.groundAlbedo * (skyUp + sunOnGround);
  const float cosSun = dot(dir, sun), cosMoon = dot(dir, moon);
  const float layer = max(v.top - v.base, 1.0);
  float3 light = 0.0;
  float transmittance = 1.0;
  float depthSum = 0.0, depthWeight = 0.0;
  // The step grows with the distance (a far cloud needs no finer sampling than a near one on screen), never shorter than a
  // thirtieth of the layer nor longer than half of it; fixed (no jitter: the fog's "a fumaça treme sem parar"), so the
  // picture never boils. `steps` caps the count (the slab is left early once the cloud is opaque).
  // Horizon's stepping: coarse steps through clear air (a cheap density test), fine steps once inside a cloud, back to coarse
  // after a run of empty samples.
  const float dtMin = layer / 64.0, dtMax = layer * 0.5;
  float t = t0 + clamp(t0 * 0.01, dtMin, dtMax) * dither;
  uint empty = 0;
  for (uint i = 0; i < steps && t < t1; ++i) {
    // Deep inside an opaque cloud (little light left to add) the step doubles.
    // 0.004 rad per step (measured 2026-10-19: at 0.01 the shells showed as stripes 20 px apart whatever the distance; a
    // constant 2-unit step had none).
    const float fine = clamp(t * 0.004, dtMin, dtMax) * (transmittance < 0.3 ? 2.0 : 1.0);
    const float dt = min(empty >= 3 ? fine * 3.0 : fine, t1 - t);
    const float3 p = origin + dir * (t + dt * 0.5);
    if (empty >= 3 && cloudVolDensity(v, f.time, p, true) <= 0.0) {
      t += dt;
      continue;
    }
    t += dt;
    const float density = cloudVolDensityLod(v, f.time, p, false, saturate((t - 800.0) / 5000.0));
    if (density <= 0.001) {
      ++empty;
      continue;
    }
    empty = 0;
    {
      const float ext = density * kCloudVolExtinction;
      const float h = saturate((p.y - v.base) / layer);
      // Direct: the sun (and the moon) through the cloud above this point, scattered towards the eye.
      const float sunDepth = cloudVolLightDepth(v, f.time, p, sun, dither);
      float3 direct = sunOnTop * cloudVolScatter(sunDepth, cosSun);
      if (moonOn) direct += moonOnTop * cloudVolScatter(cloudVolLightDepth(v, f.time, p, moon, dither), cosMoon);
      // The powder term (Schneider): the edges facing the light, where little cloud lies between them and the sun, scatter less
      // than their middles: the cauliflower's creases darken, the bulges stay lit.
      const float powder = lerp(1.0, 1.0 - exp(-sunDepth * 2.0), 0.6);
      // Fill: the sky from above through the cloud over it (brighter at the top), the ground from below (at the base).
      // (a cumulus base is mid-grey, not black: the cloud's own multiple scattering carries the sky's light down through it)
      const float3 fill = skyUp * lerp(0.4, 0.85, h) * lerp(0.6, 1.0, exp(-sunDepth * 0.1)) + groundUp * lerp(0.3, 0.05, h);
      const float3 radiance = (direct * powder + fill / kPi) * 0.9;  // albedo 0.9
      // The slab, energy-conserving (Hillaire's analytic integration).
      const float slab = exp(-ext * dt);
      light += transmittance * (radiance - radiance * slab);
      depthSum += t * transmittance * (1.0 - slab);
      depthWeight += transmittance * (1.0 - slab);
      transmittance *= slab;
      // Opaque from here on: the 1 % left at the stop let the sun's disc (radiance thousands of times the sky's) burn through
      // a thick cloud as if drawn in front of it (the user, 2026-10-23: "o sol está na frente das nuvens").
      if (transmittance < 0.01) {
        transmittance = 0.0;
        break;
      }
    }
  }
  CloudVolHit hit;
  hit.transmittance = transmittance;
  hit.depth = depthWeight > 0.0 ? depthSum / depthWeight : 0.0;
  // Distance haze: far clouds take the sky's own colour there (the air between), like everything far away.
  const float haze = depthWeight > 0.0 ? 1.0 - exp(-hit.depth / 9000.0) : 0.0;
  const float covered = 1.0 - transmittance;
  hit.color = lerp(light, clear * covered, haze);
  // Towards the horizon the layer thins into the haze (the slab's far cap).
  const float horizon = smoothstep(0.004, 0.03, dir.y);
  hit.color *= horizon;
  hit.transmittance = lerp(1.0, hit.transmittance, horizon);
  return hit;
}

#endif
