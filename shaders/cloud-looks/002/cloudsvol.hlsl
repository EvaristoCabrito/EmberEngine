// Nuvens volumÃƒÆ’Ã‚Â©tricas, look 001 (cloudsvol.hlsli): the noise volumes, made once, and the march, once a frame at half
// resolution (the sky composites it: atmosphere.hlsli skyBackground).
//   CSNoiseBase    128^3: r = Perlin-Worley (the cloud's body), gba = Worley at three frequencies (its fractal lift)
//   CSNoiseDetail  32^3: rgb = Worley at three frequencies (the edges' erosion)
//   CSMarch        per half-resolution pixel of the sky: radiance (raw) and transmittance along the view
// Both noises tile (periodic lattices), so a cloud never shows the volume's edge.
#include "cloudsvol.hlsli"

struct Push {
  uint64_t frame;    // MeshFrame
  uint destination;  // storage view: the noise volume (3D) or the march image (2D)
  uint width, height, depth;
};
[[vk::push_constant]] Push push;

// --- Periodic noise ----------------------------------------------------------------------------------------------------------

float3 hash33(float3 p) {
  p = frac(p * float3(0.1031, 0.1030, 0.0973));
  p += dot(p, p.yxz + 33.33);
  return frac((p.xxy + p.yxx) * p.zyx);
}
float3 hash33p(float3 cell, float period) { return hash33(fmod(cell + period, period)); }

// Worley (cellular) noise, inverted: 1 at a feature point, 0 far from every one; `cells` lattice cells across the tile.
float worley(float3 p, float cells) {
  const float3 q = p * cells;
  const float3 cell = floor(q), fr = q - cell;
  float best = 1.0;
  [unroll] for (int z = -1; z <= 1; ++z)
    [unroll] for (int y = -1; y <= 1; ++y)
      [unroll] for (int x = -1; x <= 1; ++x) {
        const float3 n = float3(x, y, z);
        const float3 feature = n + hash33p(cell + n, cells);  // (`point` is a reserved word in HLSL)
        best = min(best, dot(feature - fr, feature - fr));
      }
  return 1.0 - saturate(sqrt(best));
}

// Periodic gradient (Perlin) noise, 0 .. 1.
float perlin(float3 p, float cells) {
  const float3 q = p * cells;
  const float3 cell = floor(q), fr = q - cell;
  const float3 u = fr * fr * fr * (fr * (fr * 6.0 - 15.0) + 10.0);
  float v[8];
  [unroll] for (int i = 0; i < 8; ++i) {
    const float3 corner = float3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
    const float3 g = normalize(hash33p(cell + corner, cells) * 2.0 - 1.0);
    v[i] = dot(g, fr - corner);
  }
  const float x0 = lerp(v[0], v[1], u.x), x1 = lerp(v[2], v[3], u.x), x2 = lerp(v[4], v[5], u.x), x3 = lerp(v[6], v[7], u.x);
  const float y0 = lerp(x0, x1, u.y), y1 = lerp(x2, x3, u.y);
  return saturate(lerp(y0, y1, u.z) * 0.9 + 0.5);
}
float perlinFbm(float3 p, float cells) {
  return perlin(p, cells) * 0.5 + perlin(p + 0.37, cells * 2.0) * 0.25 + perlin(p + 0.71, cells * 4.0) * 0.125 + perlin(p + 0.13, cells * 8.0) * 0.0625;
}
float worleyFbm(float3 p, float cells) {
  return worley(p, cells) * 0.625 + worley(p + 0.31, cells * 2.0) * 0.25 + worley(p + 0.57, cells * 4.0) * 0.125;
}

[numthreads(4, 4, 4)]
void CSNoiseBase(uint3 id : SV_DispatchThreadID) {
  if (any(id >= uint3(push.width, push.height, push.depth))) return;
  const float3 p = (float3(id) + 0.5) / float3(push.width, push.height, push.depth);
  const float pw = perlinFbm(p, 4.0) / 0.9375;
  const float w0 = worleyFbm(p, 4.0), w1 = worleyFbm(p + 0.19, 8.0), w2 = worleyFbm(p + 0.43, 16.0);
  // Perlin-Worley: the Perlin body with the Worley's billows pressed into it (Schneider's remap).
  const float perlinWorley = remap01(pw, w0 * 0.75, 1.0);
  gStorageVolumes[NonUniformResourceIndex(push.destination)][id] = float4(perlinWorley, w0, w1, w2);
}

[numthreads(4, 4, 4)]
void CSNoiseDetail(uint3 id : SV_DispatchThreadID) {
  if (any(id >= uint3(push.width, push.height, push.depth))) return;
  const float3 p = (float3(id) + 0.5) / float3(push.width, push.height, push.depth);
  gStorageVolumes[NonUniformResourceIndex(push.destination)][id] = float4(worleyFbm(p, 2.0), worleyFbm(p + 0.23, 4.0), worleyFbm(p + 0.61, 8.0), 1.0);
}

// --- The march ----------------------------------------------------------------------------------------------------------------

[numthreads(8, 8, 1)]
void CSMarch(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const AtmosphereFrame a = loadAtmosphere(f);
  // The view through the centre of this half-resolution texel's 2 x 2 block of the screen.
  const float2 screen = (float2(pixel) + 0.5) * (f.screenSize / float2(push.width, push.height));
  const float3 dir = skyDirection(f, a, screen);
  float4 result = float4(0.0, 0.0, 0.0, 1.0);
  if (dir.y > 0.004) {
    const CloudVolume v = loadCloudVolume(f);
    const float3 clear = skyRadiance(a, dir) * a.skyScale;
    // A fixed per-pixel dither of the first step (interleaved gradient noise, Jimenez 2014: the same every frame, nothing boils)
    // breaks the march's contour bands into grain, which the 3 x 3 smoothing of cloudVolAt (atmosphere.hlsli) takes out again.
    const float dither = frac(52.9829189 * frac(0.06711056 * float(pixel.x) + 0.00583715 * float(pixel.y)));
    const CloudVolHit hit = cloudVolMarch(f, a, v, f.cameraPosition, dir, clear, 160u, dither);
    result = float4(hit.color, hit.transmittance);
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = result;
}
