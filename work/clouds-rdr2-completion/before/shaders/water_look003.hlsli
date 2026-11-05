// Water look 003 "decente" (2026-10-26; the user: "agua decente número 2 ??? seamless with motion", "eu não quero correnteza eu quero
// uma água que não seja uma bosta completa, faz a 03"). Frozen once seen (looks/water/README.md): a change is look 004.
// Looks 000-002 paint a coloured sheet over the ground; this one is the water BETWEEN the eye and the bed: it reads the scene drawn
// under it (Renderer: the ground and 3D pieces copied before the water, the depth sampled) and lets the light through it.
//   refraction  the ray through the wave normal (index 1.333) lands on the bed and the bed is read there: the riverbed wobbles under
//               the moving surface; a read that lands on something standing out of the water takes the straight one instead
//   thickness   the real path through the water from the depth buffer: the bed fades by Beer's law (red first, then green), the
//               water's own lit body takes its place; at the shore the path goes to 0, so the water ends without an edge
//   caustics    two nets of moving cells focus the sun onto the bed, fading with the depth and the sun's height, shadowed with the
//               surface
//   surface     the sky's reflection and the sun's glint by Fresnel (lighting contract), cloud and board shadows on the glint
//   motion      three long swells at their real speed (omega = sqrt(g k)) and five octaves of flow noise (gradients that turn in
//               place, each octave drifting down the current and riding the slope of the ones before): nothing tiles, nothing
//               slides as a stiff sheet; waves finer than ~3 pixels fade and widen the glint instead of sparkling
//   shore       a thin band of broken foam where the water is a few centimetres deep, drifting with the current
// Both water sheets call emberWater003(): water.hlsl (the board's, look 3) and water3d.hlsl (the Map Editor's, look 4).
#ifndef EMBER_WATER_LOOK003_HLSLI
#define EMBER_WATER_LOOK003_HLSLI

#include "lighting.hlsli"

static const float kCurrent003 = 0.61;       // the current's direction in the world (radians from +x)
static const float kCurrentSpeed003 = 0.20;  // units / s
static const float kGravity003 = 9.8;        // units / s^2 (a unit ~ a metre)
static const float kIor003 = 1.333;
// Per unit of path through the water: absorption + the silt's scattering (a clear river: the bed shows to about a unit deep).
static const float3 kExtinction003 = float3(1.45, 0.80, 0.70);
// The light the water's body sends back where it hides the bed (a green-teal river), lit like a matte surface facing up.
static const float3 kBody003 = float3(0.030, 0.085, 0.080);

uint waterHash003(int2 c) {
  uint h = uint(c.x) * 0x8da6b343u ^ uint(c.y) * 0xd8163841u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return h;
}

// A lattice gradient turned by `spin` (radians), each cell its own way and speed: the pattern lives in place.
float2 waterGrad003(int2 c, float spin) {
  const uint h = waterHash003(c);
  const float turn = ((h & 1u) != 0u ? 1.0 : -1.0) * (0.5 + float((h >> 1) & 255u) / 255.0);
  const float a = float(h) * (2.0 * kPi / 4294967296.0) + spin * turn;
  return float2(cos(a), sin(a));
}

// Flow noise with its analytic derivatives: (value, d/dx, d/dy); value ~ -0.7 .. 0.7.
float3 flowNoise003(float2 p, float spin) {
  const int2 i = int2(floor(p));
  const float2 f = p - floor(p);
  const float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  const float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
  const float2 ga = waterGrad003(i, spin), gb = waterGrad003(i + int2(1, 0), spin);
  const float2 gc = waterGrad003(i + int2(0, 1), spin), gd = waterGrad003(i + int2(1, 1), spin);
  const float va = dot(ga, f), vb = dot(gb, f - float2(1.0, 0.0)), vc = dot(gc, f - float2(0.0, 1.0)), vd = dot(gd, f - float2(1.0, 1.0));
  const float k = va - vb - vc + vd;
  const float v = va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * k;
  const float2 d = ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) + du * (u.yx * k + float2(vb, vc) - va);
  return float3(v, d);
}

// The surface slope (d height / d x, d height / d z) at `p`; `footprint` = the world width of one pixel; `lost` = the slope variance
// of the waves too fine to draw (goes into the roughness).
float2 waterSlope003(float2 p, float t, float footprint, out float lost) {
  const float2 current = float2(cos(kCurrent003), sin(kCurrent003));
  float2 slope = 0.0;
  lost = 0.0;
  // Three long swells: direction (radians off the current), wavelength (units), steepness (k x amplitude), phase seed.
  static const float4 kSwells[3] = {float4(0.35, 3.1, 0.030, 0.0), float4(-0.55, 2.3, 0.026, 2.1), float4(1.25, 1.7, 0.022, 4.4)};
  [unroll] for (uint s = 0; s < 3; ++s) {
    const float4 w = kSwells[s];
    const float a = kCurrent003 + w.x;
    const float2 d = float2(cos(a), sin(a));
    const float k = 2.0 * kPi / w.y;
    const float keep = smoothstep(2.0, 5.0, w.y / max(footprint, 1e-4));
    slope += w.z * cos(k * dot(d, p) - sqrt(kGravity003 * k) * t + w.w) * d * keep;
    lost += 0.5 * w.z * w.z * (1.0 - keep);
  }
  // Five octaves of flow noise: each turned to its own angle (no lattice lines), drifting down the current (the finer ones
  // slower), its gradients turning with time, displaced by the slope of the octaves before it (the ripples ride the bigger ones).
  static const float kAngles[5] = {0.0, 1.1, 2.3, 3.7, 5.1};
  float freq = 0.9, height = 0.030;
  float2 ride = 0.0;
  [unroll] for (uint o = 0; o < 5; ++o) {
    const float c = cos(kAngles[o]), sn = sin(kAngles[o]);
    const float2 rp = float2(c * p.x - sn * p.y, sn * p.x + c * p.y);
    const float2 rc = float2(c * current.x - sn * current.y, sn * current.x + c * current.y);
    const float speed = kCurrentSpeed003 * (1.0 - 0.12 * float(o));
    const float2 q = rp * freq - rc * (speed * freq * t) + ride * 0.35 + float2(17.3 * float(o), -5.9 * float(o));
    const float3 n = flowNoise003(q, t * (0.55 + 0.30 * float(o)));
    const float2 g = n.yz * (height * freq);  // d height / d (rotated p)
    const float keep = smoothstep(2.0, 5.0, (1.0 / freq) / max(footprint, 1e-4));
    slope += float2(c * g.x + sn * g.y, -sn * g.x + c * g.y) * keep;
    lost += 0.5 * (0.5 * height * freq) * (0.5 * height * freq) * (1.0 - keep);
    ride += n.yz;
    freq *= 2.03;
    height *= 0.5;
  }
  return slope;
}

// The distance to the nearest edge between moving cells (0 on an edge).
float causticNet003(float2 p, float t) {
  const int2 c = int2(floor(p));
  const float2 f = p - floor(p);
  float d1 = 8.0, d2 = 8.0;
  [unroll] for (int j = -1; j <= 1; ++j)
    [unroll] for (int i = -1; i <= 1; ++i) {
      const uint h = waterHash003(c + int2(i, j));
      const float2 o = float2(float(h & 0xffffu), float(h >> 16)) / 65535.0;
      const float2 at = 0.5 + 0.38 * sin(t * (0.6 + 0.5 * o) + 2.0 * kPi * o);
      const float d = length(float2(float(i), float(j)) + at - f);
      if (d < d1) {
        d2 = d1;
        d1 = d;
      } else if (d < d2) {
        d2 = d;
      }
    }
  return d2 - d1;
}

// The sun's light gathered on the bed: around 1 on average, bright lines where the two nets cross.
float caustics003(float2 p, float t, float footprint) {
  const float2 r = float2(0.6 * p.x - 0.8 * p.y, 0.8 * p.x + 0.6 * p.y);
  const float a = exp(-causticNet003(p * 1.6, t) * 9.0);
  const float b = exp(-causticNet003(r * 2.3 + 13.7, t * 1.3) * 9.0);
  const float keep = smoothstep(2.0, 5.0, (1.0 / 2.3) / max(footprint, 1e-4));  // too fine to draw: the mean
  return lerp(1.0, 0.45 + a * 1.2 + b * 0.8 + a * b * 1.6, keep);
}

float waterDepthAt003(uint depthTexture, int2 pixel) {
  return gTextures[NonUniformResourceIndex(depthTexture)].Load(int3(pixel, 0)).r;
}
float3 waterBackdropAt003(uint backdrop, uint linearSampler, float2 uv) {
  return gTextures[NonUniformResourceIndex(backdrop)].SampleLevel(gSamplers[NonUniformResourceIndex(linearSampler)], uv, 0).rgb;
}

// The finished, premultiplied colour of the water at `world` (the pixel `position`: SV_Position), over the scene in `backdrop`
// (the scene's colour before the water) and `depthTexture` (its depth, reversed-Z). `coverage` = how much of the hex is water
// (0..1; the brush's edge fade).
float4 emberWater003(MeshFrame f, float3 world, float4 position, uint backdrop, uint depthTexture, uint linearSampler, float coverage) {
  const float3 up = float3(0.0, 1.0, 0.0);
  const float alphaOut = smoothstep(0.05, 0.55, coverage);
  if (f.debugView != 0u) {  // the debug views read the surface as any other
    Surface d = defaultSurface(world, up, kBody003);
    return float4(shade(f, d) * alphaOut, alphaOut);
  }
  const float2 p = world.xz;
  const float footprint = max(length(fwidth(p)), 1e-4);
  float lost;
  const float2 slope = waterSlope003(p, f.time, footprint, lost);
  const float3 n = normalize(float3(-slope.x, 1.0, -slope.y));
  const float3 toEye = f.cameraPosition - world;
  const float dist = max(length(toEye), 1e-4);
  const float3 view = toEye / dist;

  // What lies straight behind this pixel: the vertical depth of the bed under the surface.
  const int2 size = int2(f.screenSize);
  const int2 here = clamp(int2(position.xy), int2(0, 0), size - 1);
  const float hereZ = f.zNear / max(position.z, 1e-7);
  const float straightZ = f.zNear / max(waterDepthAt003(depthTexture, here), 1e-7);
  const float straightPath = min(dist * max(straightZ / hereZ - 1.0, 0.0), 30.0);
  const float bedDepth = min(straightPath * saturate(view.y), 4.0);

  // The refracted ray down to the bed, projected back to the screen.
  const float3 r = refract(-view, n, 1.0 / kIor003);
  const float3 bedRefracted = world + r * (bedDepth / max(-r.y, 0.25));
  const float4 clip = mul(float4(bedRefracted, 1.0), f.viewProjection);
  float2 pixel = (clip.xy / max(clip.w, 1e-5) * 0.5 + 0.5) * f.screenSize;
  float behindZ = straightZ;
  bool refracted = clip.w > 0.0 && all(pixel >= 0.0) && all(pixel < f.screenSize);
  if (refracted) {
    behindZ = f.zNear / max(waterDepthAt003(depthTexture, int2(pixel)), 1e-7);
    refracted = behindZ > hereZ + 0.01;  // landed on something under the water, not on a bank or a piece standing out of it
  }
  if (!refracted) {
    pixel = position.xy;
    behindZ = straightZ;
  }
  // The path through the water to what is read (the surface there taken at this pixel's depth: the offsets are small).
  const float path = min(dist * max(behindZ / hereZ - 1.0, 0.0), 30.0);
  const float below = path * saturate(view.y);
  float3 bed = waterBackdropAt003(backdrop, linearSampler, pixel / f.screenSize);

  const Sight sight = tacticalSight(f, world);
  // The surface: the sky's reflection and the sun's glint (no albedo: the light that never enters the water).
  Surface surface = defaultSurface(world, up, float3(0.0, 0.0, 0.0));
  surface.normal = n;
  surface.roughness = clamp(sqrt(sqrt(0.035 * 0.035 + 2.0 * lost)), 0.05, 0.4);  // GGX alpha^2 ~ 2 x the slope variance lost
  surface.receivesCardShadow = 0.0;
  const float3 reflected = shadeInSight(f, surface, sight) * sight.color;

  // Caustics on the bed: the sun through the waves, shadowed with the surface, gone with the depth and a low sun.
  float sunLit = cloudShadow(f, world) * sunShadow(f, surface);
  sunLit *= saturate(f.sunDirection.y * 5.0) * ((f.lightingLayers & kLayerSun) != 0u ? 1.0 : 0.0);
  const float3 bedPoint = world - view * path;
  const float caustic = caustics003(bedPoint.xz + slope * 0.6, f.time, footprint);
  bed *= 1.0 + (caustic - 1.0) * 0.55 * sunLit * exp(-below * 0.9) * smoothstep(0.0, 0.12, below);

  // The water's own body, lit like a matte surface facing up (the sun through the surface, the sky, torches).
  Surface body = defaultSurface(world, up, kBody003);
  body.specular = 0.0;
  body.receivesCardShadow = 0.0;
  const float3 bodyLight = shadeInSight(f, body, sight) * sight.color;

  // Shore foam: a thin broken band where the water is a few centimetres deep.
  const float2 current = float2(cos(kCurrent003), sin(kCurrent003));
  const float2 fq = p * 3.0 - current * (kCurrentSpeed003 * 3.0 * f.time);
  const float broken = smoothstep(0.08, 0.40, flowNoise003(fq, f.time * 0.8).x * 0.65 + flowNoise003(fq * 2.3 + 7.7, f.time).x * 0.35);
  const float foam = smoothstep(0.0, 0.03, below) * (1.0 - smoothstep(0.05, 0.16, below)) * broken * 0.6;
  Surface foamSurface = defaultSurface(world, up, float3(0.72, 0.76, 0.75));
  foamSurface.normal = n;
  foamSurface.roughness = 0.9;
  foamSurface.specular = 0.3;
  const float3 foamLight = shadeInSight(f, foamSurface, sight) * sight.color;

  // Through the surface: the bed fading into the body by the path; Fresnel takes the rest for the reflection.
  const float transmit = 1.0 - (0.02 + 0.98 * pow(1.0 - saturate(dot(n, view)), 5.0));
  const float3 through = exp(-kExtinction003 * path);
  // The air between the eye and the water (applyVolume = colour x A + B): the bed's copy has it already.
  const float3 airAdd = applyVolume(f, float3(0.0, 0.0, 0.0), world);
  const float3 airMul = applyVolume(f, float3(1.0, 1.0, 1.0), world) - airAdd;
  float3 water = transmit * through * max(bed - airAdd, 0.0) + airMul * (transmit * (1.0 - through) * bodyLight + reflected);
  water += airAdd;
  water = lerp(water, foamLight * airMul + airAdd, foam);
  return float4(water * alphaOut, alphaOut);
}

#endif
