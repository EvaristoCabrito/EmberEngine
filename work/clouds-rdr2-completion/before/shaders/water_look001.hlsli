// Water look 001 "ember" (2026-10-19; the user: "preciso de ganhos na água desesperadamente", "aceito novos tiles de
// água"): EmberEngine's own water material, no texture tile at all. Frozen once seen (looks/water/README.md): a change
// is look 002.
//   waves    six directional waves of falling wavelength (5.2 .. 0.37 units) with analytic slopes, the finest ones fading
//            where they would be narrower than a few pixels (no sparkle at a distance), and a drifting noise ripple
//   body     the water's own colour: Ember's river blue (#2f86a8) in the shallows, deepening to a dark teal with the depth
//            below (Beer's law) where the sheet knows its depth; without a depth the body is mid-deep
//   light    the lighting contract: sun highlight (GGX, roughness 0.10), the sky's reflection map by Fresnel
//            (lighting.hlsli indirectSpecular: clouds, dusk, all of it), the sky's light on the body, the cloud shadows
//   see-through  Fresnel decides how much of the ground below shows: steep views see into the water, grazing ones see sky
//   foam     where the sheet knows its depth: a broken fringe along the shore
// Both water sheets (water.hlsl, the board's; water3d.hlsl, the Map Editor's) call emberWater().
#ifndef EMBER_WATER_LOOK001_HLSLI
#define EMBER_WATER_LOOK001_HLSLI

#include "lighting.hlsli"

// Direction angle, wavelength (units), amplitude (units), phase speed (units / s).
static const float4 kEmberWaves[6] = {float4(0.35, 5.2, 0.030, 1.10), float4(2.10, 3.1, 0.020, 1.35), float4(1.20, 1.9, 0.012, 1.70),
                                      float4(3.90, 1.1, 0.0065, 2.10), float4(0.80, 0.62, 0.0032, 2.6), float4(2.70, 0.37, 0.0016, 3.1)};

// The surface slope at a point of the sheet (d height / d x, d height / d z), `footprint` = the world width of one pixel.
float2 emberWaveSlope(float2 p, float t, float footprint) {
  float2 slope = 0.0;
  [unroll] for (uint i = 0; i < 6; ++i) {
    const float4 w = kEmberWaves[i];
    const float2 d = float2(cos(w.x), sin(w.x));
    const float k = 2.0 * kPi / w.y;
    // a wave narrower than ~3 pixels is gone (it would only alias)
    const float keep = smoothstep(2.0, 5.0, w.y / max(footprint, 1e-4));
    const float phase = k * (dot(d, p) - w.w * t);
    // sharpened crests: h = A (sin + 1 - (sin + 1)^2 / 4)' ... kept as a plain sine with a crest term
    const float s = sin(phase), c = cos(phase);
    slope += d * (w.z * k * c * (1.0 + 0.35 * s) * keep);
  }
  // a slow ripple noise on top (clouds.hlsli's value noise), finite differences
  const float2 q = p * 1.7 + float2(0.21, -0.13) * t;
  const float keepN = smoothstep(1.5, 4.0, 0.6 / max(footprint, 1e-4));
  const float e = 0.08;
  const float nx = cloudNoise(q + float2(e, 0.0)) - cloudNoise(q - float2(e, 0.0));
  const float nz = cloudNoise(q + float2(0.0, e)) - cloudNoise(q - float2(0.0, e));
  slope += float2(nx, nz) * (0.012 / (2.0 * e) * keepN);
  return slope;
}

// The lit, premultiplied colour of the sheet at `world` (its flat normal `up`). `depth` = water above the ground there
// (< 0: unknown), `coverage` = how much of the hex is water (0..1; the brush's edge fade).
float4 emberWater(MeshFrame f, float3 world, float3 up, float depth, float coverage) {
  const float2 p = world.xz;
  const float footprint = max(length(fwidth(p)), 1e-4);
  const float2 slope = emberWaveSlope(p, f.time, footprint);
  const float3 n = normalize(up + float3(-slope.x, 0.0, -slope.y));

  // The body: river blue in the shallows, dark teal deep down.
  const float3 shallow = srgbToLinear3(float3(0x2f, 0x86, 0xa8) / 255.0);
  const float3 deep = float3(0.02, 0.07, 0.11);
  // (a sheet that knows no depth keeps the river blue: Ember's water read bright and the user wants that blue; clear
  // water: a unit of depth takes less than half the way to the deep colour)
  const float d = depth >= 0.0 ? depth : 0.3;
  const float3 body = lerp(shallow, deep, 1.0 - exp(-d * 0.6));

  Surface s = defaultSurface(world, up, body);
  s.normal = n;
  s.roughness = lerp(0.10, 0.24, smoothstep(0.05, 0.4, footprint));  // a touch rougher far away (the waves it lost)
  s.metallic = 0.0;
  float3 color = shade(f, s);

  // How much the ground below shows: Fresnel (Schlick on the wave normal), more of it the shallower the water.
  const float3 view = normalize(f.cameraPosition - world);
  const float fresnel = 0.02 + 0.98 * pow(1.0 - saturate(dot(n, view)), 5.0);
  const float seeThrough = (depth >= 0.0 ? 0.55 * exp(-d * 1.8) : 0.22) * (1.0 - fresnel);
  float alpha = 1.0 - seeThrough;

  // Foam along the shore, broken by the ripple noise.
  if (depth >= 0.0) {
    const float shore = 1.0 - smoothstep(0.0, 0.14, depth);
    const float broken = smoothstep(0.42, 0.7, cloudNoise(p * 3.1 + float2(0.3, 0.2) * f.time) * 0.6 + cloudNoise(p * 7.3 - 0.4 * f.time) * 0.4);
    const float foam = shore * broken * 0.85;
    Surface fs = defaultSurface(world, up, float3(0.72, 0.76, 0.75));
    fs.normal = n;
    fs.roughness = 0.9;
    fs.specular = 0.3;
    color = lerp(color, shade(f, fs), foam);
    alpha = lerp(alpha, 0.98, foam);
  }
  alpha *= smoothstep(0.05, 0.55, coverage);
  return float4(color * alpha, alpha);
}

#endif
