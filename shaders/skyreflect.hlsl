// The sky's reflection map (2026-10-19; the plan's phase 4: "water, wet ground, metal and polished stone should reflect the
// real sky"): a lat-long image of the sky's radiance, rebuilt every frame from the sky-view table the atmosphere pass
// already made (so it follows the time of day with no jump: the table does), with the cloud sheet over it (clouds.hlsli)
// and the lit ground below the horizon. Level 0 is the sharp sky; each level above is prefiltered for a rougher surface
// (GGX importance sampling, roughness = level / (levels - 1), 32 samples each reading level 0 at the lod their solid angle
// asks for: filtered importance sampling). lighting.hlsli skyReflection() reads it by roughness. 256 x 128 x 6 levels:
// ~44k texels a frame.
#include "cloudsvol.hlsli"

struct Push {
  uint64_t frame;  // MeshFrame (its skyReflect = this map, sampled)
  uint destination;  // storage view of the level written (RGBA16F)
  uint level;        // 0 = the sky itself, 1.. prefiltered
  uint width, height;
};
[[vk::push_constant]] Push push;

// The direction a lat-long texel looks at (the inverse of lighting.hlsli latLongUv, without its pole inset).
float3 latLongDirection(float2 uv) {
  const float phi = (uv.x - 0.5) * 2.0 * kPi, theta = uv.y * kPi;
  const float st = sin(theta);
  return float3(cos(phi) * st, cos(theta), sin(phi) * st);
}

[numthreads(8, 8, 1)]
void CSBase(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const AtmosphereFrame a = loadAtmosphere(f);
  const float3 dir = latLongDirection((float2(pixel) + 0.5) / float2(push.width, push.height));
  float3 radiance;
  if (dir.y >= 0.0) {
    // The sky (no sun disc: the sun's highlight is the direct specular's), with the clouds seen from the board.
    const float3 clear = skyRadiance(a, dir);
    if (f.cloudFlags & kCloudVolumetric) {
      // The volumetric layer, marched coarsely (12 steps: the map is 256 x 128) from the board's middle.
      const CloudVolHit hit = cloudVolMarch(f, a, loadCloudVolume(f), float3(0.0, 0.0, 0.0), dir, clear * a.skyScale, 24u, 0.5);
      radiance = clear * hit.transmittance + hit.color / max(a.skyScale, 1e-4);
    } else {
      radiance = cloudsOverSky(f, a, float3(0.0, 0.0, 0.0), dir, clear, clear);
    }
  } else {
    // Below the horizon: the lit ground (Lambert on the raw sky irradiance plus the key light), as CSAmbient's SH does.
    const float3 skyUp = float3(vk::RawBufferLoad<float4>(a.ambient + 16).w, vk::RawBufferLoad<float4>(a.ambient + 32).w,
                                vk::RawBufferLoad<float4>(a.ambient + 48).w);
    const float3 sunOnGround = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, a.sunDirection.y) * saturate(a.sunDirection.y);
    radiance = a.groundAlbedo / kPi * (skyUp + sunOnGround);
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = float4(radiance, 1.0);
}

// The cloud shadow map over the board (clouds.hlsli cloudShadow): one texel per step of the rect, the sun through the low
// sheet at the board's height (push.level carries the height's bits: the board top, where the shadow lands).
[numthreads(8, 8, 1)]
void CSCloudShadow(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const float4 rect = vk::RawBufferLoad<float4>(f.atmosphere + 304u);
  const float2 xz = rect.xy + (float2(pixel) + 0.5) / float2(push.width, push.height) / rect.zw;
  const float y = asfloat(push.level);
  gStorageImagesR32f[NonUniformResourceIndex(push.destination)][pixel] = cloudShadowCompute(f, float3(xz.x, y, xz.y));
}

// Hammersley point i of n.
float2 hammersley(uint i, uint n) {
  return float2((float(i) + 0.5) / float(n), float(reversebits(i)) * 2.3283064365386963e-10);
}

[numthreads(8, 8, 1)]
void CSFilter(uint2 pixel : SV_DispatchThreadID) {
  if (any(pixel >= uint2(push.width, push.height))) return;
  const MeshFrame f = loadFrame(push.frame);
  const float3 n = latLongDirection((float2(pixel) + 0.5) / float2(push.width, push.height));
  const float roughness = float(push.level) / float(kSkyReflectLevels - 1u);
  const float alpha = max(roughness * roughness, 0.02), a2 = alpha * alpha;
  // A frame around n.
  const float3 up = abs(n.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
  const float3 tx = normalize(cross(up, n)), ty = cross(n, tx);
  // One texel of level 0 covers this solid angle (the equator's; the poles' smaller texels only get a little blurrier).
  const float texelSolidAngle = 4.0 * kPi / (kSkyReflectSize.x * kSkyReflectSize.y);
  static const uint kSamples = 32;
  float3 sum = 0.0;
  float weight = 0.0;
  for (uint i = 0; i < kSamples; ++i) {
    const float2 xi = hammersley(i, kSamples);
    // GGX half vector around n (v = n), its reflection l.
    const float cosTheta = sqrt((1.0 - xi.x) / (1.0 + (a2 - 1.0) * xi.x)), sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    const float phi = 2.0 * kPi * xi.y;
    const float3 h = tx * (sinTheta * cos(phi)) + ty * (sinTheta * sin(phi)) + n * cosTheta;
    const float3 l = 2.0 * dot(n, h) * h - n;
    const float nl = dot(n, l);
    if (nl <= 0.0) continue;
    // pdf of l = D(h) / 4 (v = n); the lod that makes this sample's cone match one texel.
    const float d = a2 / (kPi * pow(cosTheta * cosTheta * (a2 - 1.0) + 1.0, 2.0));
    const float pdf = max(d * 0.25, 1e-5);
    const float sampleSolidAngle = 1.0 / (float(kSamples) * pdf);
    // (never the level being written or one not yet written this frame: the levels below only)
    const float lod = clamp(0.5 * log2(sampleSolidAngle / texelSolidAngle) + 1.0, 0.0, float(push.level - 1u));
    sum += skyReflectSample(f, latLongUv(l), lod) * nl;
    weight += nl;
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel] = float4(sum / max(weight, 1e-5), 1.0);
}
