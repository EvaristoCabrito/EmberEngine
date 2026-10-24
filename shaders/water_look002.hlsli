// Water look 002 "correnteza" (2026-10-23; the user: "we really need better seamless moving water in this game", "faz a
// água look 002"): seamless moving water, no tile anywhere. Frozen once seen (looks/water/README.md): a change is look 003.
//   seamless  nothing repeats: ten waves on incommensurate wavelengths (2.4 .. 0.09 units) and directions, their fronts bent by
//             one slow drifting warp field (no straight line, no period), plus two octaves of gradient noise on an integer
//             hash lattice (never a tile, never a seam); the flat normal is the world's up, so the sheet's own triangles
//             never show as bands
//   moving    every wave runs at its real speed (deep-water dispersion: omega = sqrt(g k), the small ripples slower than the
//             swells), the noise drifts down one current at two speeds, the warp drifts with it
//   far away  a wave narrower than ~3 pixels fades out and its slope goes into the roughness (the highlight widens instead
//             of sparkling or flattening)
//   body      the river blue (#2f86a8) of the other looks in the shallows, the dark teal deep down (Beer's law where the sheet
//             knows its depth); Fresnel decides how much of the ground shows through; shore foam drifting with the current
//   light     the lighting contract: sun / moon highlight (GGX), the sky's reflection by Fresnel, cloud shadows
// Both water sheets (water.hlsl, the board's; water3d.hlsl, the Map Editor's) call emberWater002().
#ifndef EMBER_WATER_LOOK002_HLSLI
#define EMBER_WATER_LOOK002_HLSLI

#include "lighting.hlsli"

// Direction (radians off the current), wavelength (units), steepness (k x amplitude), phase seed.
static const float4 kWaves002[10] = {float4(0.00, 2.40, 0.050, 0.0), float4(0.93, 1.71, 0.048, 1.7), float4(-0.71, 1.23, 0.046, 4.1),
                                     float4(1.62, 0.87, 0.044, 2.9), float4(-1.34, 0.61, 0.042, 5.3), float4(0.37, 0.43, 0.040, 0.8),
                                     float4(-2.21, 0.30, 0.038, 3.6), float4(2.57, 0.21, 0.036, 6.0), float4(-0.42, 0.15, 0.034, 1.2),
                                     float4(1.13, 0.09, 0.032, 4.7)};
static const float kCurrent002 = 0.61;       // the current's direction in the world (radians from +x)
static const float kCurrentSpeed002 = 0.16;  // units / s
static const float kGravity002 = 9.8;        // units / s^2 (a unit ~ a metre)

// Integer hash -> a unit gradient (no sin hash: exact at any distance from the origin).
float2 waterGrad002(int2 c) {
  uint h = uint(c.x) * 0x8da6b343u ^ uint(c.y) * 0xd8163841u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  const float a = float(h) * (2.0 * kPi / 4294967296.0);
  return float2(cos(a), sin(a));
}

// Gradient noise with its analytic derivatives: (value, d/dx, d/dy); value ~ -0.7 .. 0.7.
float3 waterNoise002(float2 p) {
  const int2 i = int2(floor(p));
  const float2 f = p - floor(p);
  const float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  const float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
  const float2 ga = waterGrad002(i), gb = waterGrad002(i + int2(1, 0)), gc = waterGrad002(i + int2(0, 1)), gd = waterGrad002(i + int2(1, 1));
  const float va = dot(ga, f), vb = dot(gb, f - float2(1.0, 0.0)), vc = dot(gc, f - float2(0.0, 1.0)), vd = dot(gd, f - float2(1.0, 1.0));
  const float k = va - vb - vc + vd;
  const float v = va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * k;
  const float2 d = ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) + du * (u.yx * k + float2(vb, vc) - va);
  return float3(v, d);
}

// The surface slope (d height / d x, d height / d z) at `p`; `footprint` = the world width of one pixel; `lost` = the slope
// variance of the waves too fine to draw (goes into the roughness).
float2 waterSlope002(float2 p, float t, float footprint, out float lost) {
  const float2 current = float2(cos(kCurrent002), sin(kCurrent002));
  // One slow warp, drifting with the current, bends every wave front (the fronts never run straight or repeat).
  const float warpFreq = 0.11, warpRad = 3.0;
  const float3 warp = waterNoise002(p * warpFreq - current * (kCurrentSpeed002 * warpFreq * t) + float2(17.3, -5.9));
  const float2 warpGrad = warp.yz * (warpFreq * warpRad);
  float2 slope = 0.0;
  lost = 0.0;
  [unroll] for (uint i = 0; i < 10; ++i) {
    const float4 w = kWaves002[i];
    const float a = kCurrent002 + w.x;
    const float2 d = float2(cos(a), sin(a));
    const float k = 2.0 * kPi / w.y;
    const float omega = sqrt(kGravity002 * k);
    const float keep = smoothstep(2.0, 5.0, w.y / max(footprint, 1e-4));  // narrower than ~3 pixels: gone
    const float phase = k * dot(d, p) - omega * t + warp.x * warpRad + w.w;
    const float amp = w.z / k;
    slope += amp * cos(phase) * (k * d + warpGrad) * keep;
    lost += 0.5 * w.z * w.z * (1.0 - keep);
  }
  // Two octaves of noise drifting down the current at two speeds (the small chop riding on the waves).
  const float2 q1 = p * 1.3 - current * (kCurrentSpeed002 * 1.3 * t);
  const float2 q2 = p * 3.1 - current * (kCurrentSpeed002 * 1.7 * 3.1 * t) + float2(41.7, 13.1);
  const float keep1 = smoothstep(2.0, 5.0, (1.0 / 1.3) / max(footprint, 1e-4));
  const float keep2 = smoothstep(2.0, 5.0, (1.0 / 3.1) / max(footprint, 1e-4));
  slope += waterNoise002(q1).yz * (0.030 * 1.3 * keep1) + waterNoise002(q2).yz * (0.012 * 3.1 * keep2);
  lost += 0.5 * (0.040 * 0.040 * (1.0 - keep1) + 0.037 * 0.037 * (1.0 - keep2));
  return slope;
}

// The lit, premultiplied colour of the sheet at `world`. `depth` = water above the ground there (< 0: unknown), `coverage` =
// how much of the hex is water (0..1; the brush's edge fade).
float4 emberWater002(MeshFrame f, float3 world, float depth, float coverage) {
  const float3 up = float3(0.0, 1.0, 0.0);
  const float2 p = world.xz;
  const float footprint = max(length(fwidth(p)), 1e-4);
  float lost;
  const float2 slope = waterSlope002(p, f.time, footprint, lost);
  const float3 n = normalize(up + float3(-slope.x, 0.0, -slope.y));

  // The body: river blue in the shallows, dark teal deep down.
  const float3 shallow = srgbToLinear3(float3(0x2f, 0x86, 0xa8) / 255.0);
  const float3 deep = float3(0.02, 0.07, 0.11);
  const float d = depth >= 0.0 ? depth : 0.3;
  const float3 body = lerp(shallow, deep, 1.0 - exp(-d * 0.6));

  Surface s = defaultSurface(world, up, body);
  s.normal = n;
  // GGX alpha^2 ~ 2 x slope variance: the waves too fine to draw widen the highlight.
  const float alpha = sqrt(0.06 * 0.06 + 2.0 * lost);
  s.roughness = clamp(sqrt(alpha), 0.06, 0.45);
  s.metallic = 0.0;
  float3 color = shade(f, s);

  // How much the ground below shows: Fresnel (Schlick on the wave normal), more of it the shallower the water.
  const float3 view = normalize(f.cameraPosition - world);
  const float fresnel = 0.02 + 0.98 * pow(1.0 - saturate(dot(n, view)), 5.0);
  const float seeThrough = (depth >= 0.0 ? 0.55 * exp(-d * 1.8) : 0.22) * (1.0 - fresnel);
  float alphaOut = 1.0 - seeThrough;

  // Foam along the shore, drifting with the current, broken by the noise.
  if (depth >= 0.0) {
    const float2 current = float2(cos(kCurrent002), sin(kCurrent002));
    const float shore = 1.0 - smoothstep(0.0, 0.14, depth);
    const float2 fq = p * 2.7 - current * (kCurrentSpeed002 * 2.7 * f.time);
    const float broken = smoothstep(0.05, 0.35, waterNoise002(fq).x * 0.65 + waterNoise002(fq * 2.3 + 7.7).x * 0.35);
    const float foam = shore * broken * 0.85;
    Surface fs = defaultSurface(world, up, float3(0.72, 0.76, 0.75));
    fs.normal = n;
    fs.roughness = 0.9;
    fs.specular = 0.3;
    color = lerp(color, shade(f, fs), foam);
    alphaOut = lerp(alphaOut, 0.98, foam);
  }
  alphaOut *= smoothstep(0.05, 0.55, coverage);
  return float4(color * alphaOut, alphaOut);
}

#endif
