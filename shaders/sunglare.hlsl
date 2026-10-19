// Clarão do sol e raios de luz, look 001 (2026-10-19; the user: "não tem nem sol", "cadê o céu do Final Fantasy XV?"): what
// the lens does when the sun (or the moon) is in or near the frame, computed at half resolution from the finished scene
// colour and added before the tonemap (tonemap.hlsl), so the ACES curve rolls it off like any other light.
//   Raios de luz  crepuscular rays: the bright sky around the light (its aureole, lit cloud rims) smeared radially towards
//                 the light's pixel (Mitchell 2007's screen-space shafts: 40 taps, decaying), so anything dark between
//                 (clouds, towers, the hills) cuts the rays; faded with the distance from the light
//   Clarão        the veiling glare (a wide, soft glow around the light), a faint starburst, two small ghosts and a halo ring
//                 on the lens's axis, all in the light's own colour
// How much of the light is seen (behind a cloud, a tower, the hills) is read off the scene itself: the brightest pixels
// around the light's position; off screen (within a margin), the caller's estimate.
#include "common.hlsli"

struct Push {
  uint64_t frame;       // MeshFrame
  uint source;          // the scene colour (sampled)
  uint sampler;         // linear, clamp
  uint destination;     // storage view (RGBA16F, half resolution)
  uint width, height;
  float lightU, lightV;       // the light's position on screen (0..1; may lie outside)
  float tintR, tintG, tintB;  // the light's colour x brightness on the scene's scale (used when it lies off screen)
  float visibility;     // off screen: how much of it is seen (0 .. 1)
  float shafts;         // strength of the rays (0 = none)
  float flare;          // strength of the glare (0 = none)
  float aspect;         // width / height
};
[[vk::push_constant]] Push push;

float luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

float3 sceneAt(float2 uv) {
  if (any(uv < 0.0) || any(uv > 1.0)) return 0.0;
  return gTextures[NonUniformResourceIndex(push.source)].SampleLevel(gSamplers[NonUniformResourceIndex(push.sampler)], uv, 0).rgb;
}
// What the rays are made of: only what is clearly brighter than the lit scene (the sun's aureole, the glowing cloud rims, the
// moon), so the sky itself and the ground never streak.
float3 brightAt(float2 uv) {
  const float3 c = sceneAt(uv);
  return c * smoothstep(0.9, 2.4, luma(c));
}

[numthreads(8, 8, 1)]
void CSGlare(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const float2 uv = (float2(pixel) + 0.5) / float2(push.width, push.height);
  const float2 light = float2(push.lightU, push.lightV);
  // The light as the scene shows it: the mean of a small disc around its pixel, clipped so the glare never runs away.
  float3 core = float3(push.tintR, push.tintG, push.tintB) * push.visibility;
  const bool onScreen = all(light >= 0.0) && all(light <= 1.0);
  if (onScreen) {
    float3 sum = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
      [unroll] for (int x = -1; x <= 1; ++x) sum += sceneAt(light + float2(x, y) * float2(0.006 / push.aspect, 0.006));
    sum /= 9.0;
    const float l = luma(sum);
    core = l > 1e-4 ? sum * (min(l, 12.0) / l) : 0.0;
    core *= smoothstep(0.6, 2.0, l);  // under the scene's own brightness the light is not seen (behind a cloud, a hill)
  } else {
    // Off screen: the caller's estimate, fading over a margin of a third of the screen beyond its edge.
    const float2 outside = max(max(-light, light - 1.0), 0.0);
    core *= 1.0 - smoothstep(0.0, 0.35, length(outside));
  }
  float3 result = 0.0;
  // Screen-space distances with the aspect taken out (so the glare is round).
  const float2 d = (uv - light) * float2(push.aspect, 1.0);
  const float dist = length(d);
  if (push.shafts > 0.0 && luma(core) > 1e-4) {
    // The rays: 40 taps from this pixel towards the light, decaying; the light's own pixels at the end weigh most.
    static const uint kTaps = 40u;
    const float2 delta = (light - uv) / float(kTaps) * 0.92;
    float2 p = uv;
    float3 sum = 0.0;
    float weight = 1.0;
    for (uint i = 0; i < kTaps; ++i) {
      p += delta;
      sum += brightAt(p) * weight;
      weight *= 0.965;
    }
    sum /= float(kTaps);
    // Faded with the distance from the light, and never brighter than the light itself.
    result += sum * push.shafts * 1.6 * exp(-dist * 1.4) * smoothstep(0.0, 0.3, luma(core));
  }
  if (push.flare > 0.0 && luma(core) > 1e-4) {
    const float3 c = core * push.flare;
    // Veiling glare: a wide soft glow, brightest at the light.
    float3 flare = c * 0.16 * exp(-dist * 7.0) + c * 0.03 * exp(-dist * 2.0);
    // The starburst: six soft rays, fading fast.
    const float angle = atan2(d.y, d.x);
    const float burst = pow(max(abs(cos(angle * 3.0)), 0.0), 24.0) * exp(-dist * 4.5);
    flare += c * 0.12 * burst;
    // Ghosts and a halo ring on the lens axis (mirrored through the screen's centre), faint, each in a slightly different hue.
    const float2 centre = float2(0.5, 0.5);
    const float2 axis = (centre - light) * float2(push.aspect, 1.0);
    const float2 here = (uv - centre) * float2(push.aspect, 1.0);
    const float2 ghost1 = here - axis * 0.45, ghost2 = here + axis * 0.3;
    flare += c * float3(0.9, 0.55, 0.35) * 0.05 * (1.0 - smoothstep(0.03, 0.06, length(ghost1)));
    flare += c * float3(0.5, 0.75, 1.0) * 0.035 * (1.0 - smoothstep(0.015, 0.045, length(ghost2)));
    const float ring = exp(-pow((length(here + axis * 1.1) - 0.32) / 0.025, 2.0));
    flare += c * float3(0.7, 0.85, 1.1) * 0.03 * ring;
    result += flare;
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = float4(result, 1.0);
}
