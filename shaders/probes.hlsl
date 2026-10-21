// The room's diffuse light probes, relit every frame (Luz das janelas; engine/world/ProbeBake.h): after Final Fantasy XV's ambient
// system (SIGGRAPH 2016 Talks, "Rendering Techniques of Final Fantasy XV"): the sky's light through Precomputed Radiance Transfer,
// so the hour's real sky lights the room through its openings and its bounces; plus the key light's (the sun's, the moon's) bounce
// off the room, from a table over key-light directions, at the key's real direction (the three nearest, weighted by the renderer).
// Out: per probe the irradiance as 9 spherical harmonics (lighting.hlsli probeIrradiance reads them), and the light on an upward
// face in lux for the camera's meter (read back by the renderer).
#include "lighting.hlsli"

struct Push {
  uint64_t frame;     // MeshFrame
  uint64_t bake;      // header (4 x float4: origin, spacing, dims + count, keyCount) then per probe: valid, transfer 81, key keyCount x 4
  uint64_t output;    // header (copied) then per probe 9 x float4 (rgb irradiance with the exposure in; w of the first = valid)
  uint64_t readback;  // per probe: the irradiance on an upward face, lux (the exposure divided out)
  uint3 key;          // the three key-light directions nearest the key light ...
  uint count;
  float3 weight;      // ... and their weights (summing to 1)
  uint pad;
};
[[vk::push_constant]] Push push;

static const float kBandIrradiance[9] = {kPi, 2.0943951, 2.0943951, 2.0943951, 0.785398, 0.785398, 0.785398, 0.785398, 0.785398};

[numthreads(64, 1, 1)]
void CSRelight(uint id : SV_DispatchThreadID) {
  if (id < 4u) vk::RawBufferStore<float4>(push.output + id * 16u, vk::RawBufferLoad<float4>(push.bake + id * 16u));
  if (id >= push.count) return;
  const MeshFrame f = loadFrame(push.frame);
  const uint keyCount = vk::RawBufferLoad<uint>(push.bake + 3u * 16u);
  const uint64_t probe = push.bake + 64u + uint64_t(id) * uint64_t((1u + 81u + keyCount * 4u) * 4u);
  const uint64_t dst = push.output + 64u + uint64_t(id) * 144u;
  const float valid = vk::RawBufferLoad<float>(probe);
  if (valid < 0.5 || f.atmosphere == 0) {
    [unroll] for (uint c = 0; c < 9; ++c) vk::RawBufferStore<float4>(dst + c * 16u, float4(0.0, 0.0, 0.0, 0.0));
    vk::RawBufferStore<float>(push.readback + uint64_t(id) * 4u, 0.0);
    return;
  }
  // The sky's radiance: CSAmbient's unscaled irradiance (after the ambient the passes read), one band factor back, the exposure in.
  const uint64_t sh = loadAtmosphere(f).ambient + 144u;
  float3 sky[9];
  [unroll] for (uint c = 0; c < 9; ++c) sky[c] = vk::RawBufferLoad<float4>(sh + c * 16u).rgb / kBandIrradiance[c] * f.exposure;
  // The radiance arriving at the probe: the transfer of the sky ...
  float3 radiance[9];
  [unroll] for (uint i = 0; i < 9; ++i) {
    float3 sum = 0.0;
    [unroll] for (uint j = 0; j < 9; ++j) sum += vk::RawBufferLoad<float>(probe + (1u + i * 9u + j) * 4u) * sky[j];
    radiance[i] = sum;
  }
  // ... and the key light's bounce (unit irradiance in the table; the key's own, through the clouds, exposure in).
  const float3 origin = vk::RawBufferLoad<float4>(push.bake).xyz, spacing = vk::RawBufferLoad<float4>(push.bake + 16u).xyz;
  const uint4 dims = vk::RawBufferLoad<uint4>(push.bake + 32u);
  const float3 at = origin + float3(id % dims.x, id / (dims.x * dims.z), (id / dims.x) % dims.z) * spacing;
  const float3 keyIrradiance = f.sunColor * f.sunIntensity * cloudShadow(f, at);
  [unroll] for (uint t = 0; t < 3; ++t) {
    const uint64_t row = probe + (1u + 81u + push.key[t] * 4u) * 4u;
    [unroll] for (uint i = 0; i < 4; ++i) radiance[i] += push.weight[t] * vk::RawBufferLoad<float>(row + i * 4u) * keyIrradiance;
  }
  // Irradiance, the meter's upward reading
  const float upBasis[9] = {0.282095, 0.488603, 0.0, 0.0, 0.0, 0.0, -0.315392, 0.0, -0.546274};  // shBasis(0, 1, 0)
  float3 up = 0.0;
  [unroll] for (uint c = 0; c < 9; ++c) {
    const float3 e = radiance[c] * kBandIrradiance[c];
    vk::RawBufferStore<float4>(dst + c * 16u, float4(e, c == 0 ? 1.0 : 0.0));
    up += e * upBasis[c];
  }
  vk::RawBufferStore<float>(push.readback + uint64_t(id) * 4u, max(dot(up, float3(0.2126, 0.7152, 0.0722)), 0.0) / max(f.exposure, 1e-12));
}
