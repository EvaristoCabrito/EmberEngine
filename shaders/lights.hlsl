// Clustered light culling (FFXV: tiles x logarithmic depth slices). The view frustum is split into
// kClusterX x kClusterY screen tiles and kClusterZ depth slices spaced logarithmically between clusterNear
// and clusterFar; each cluster lists the local lights whose sphere of influence touches it. Every lit
// surface finds its cluster from its position (lighting.hlsli localLights) and walks only that list.
//   CSClusters  one thread per cluster: test every light's sphere against the cluster's view-space box.
#include "scene.hlsli"

struct Push {
  uint64_t frame;
};
[[vk::push_constant]] Push push;

[numthreads(64, 1, 1)]
void CSClusters(uint id : SV_DispatchThreadID) {
  const MeshFrame f = loadFrame(push.frame);
  if (id >= kClusterCount) return;
  const uint cx = id % kClusterX, cy = (id / kClusterX) % kClusterY, cz = id / (kClusterX * kClusterY);
  // The cluster's view-space box: its depth range, and the tile's corners at both depths (forward = -z).
  const float zNear = clusterSliceDepth(f, cz), zFar = clusterSliceDepth(f, cz + 1);
  const float2 ndc0 = float2(cx, cy) / float2(kClusterX, kClusterY) * 2.0 - 1.0;
  const float2 ndc1 = float2(cx + 1, cy + 1) / float2(kClusterX, kClusterY) * 2.0 - 1.0;
  // Perspective: view x = ndc.x * depth / projX, view y = -ndc.y * depth / projY (Vulkan NDC y down).
  float3 lo = float3(1e30, 1e30, -zFar), hi = float3(-1e30, -1e30, -zNear);
  [unroll] for (uint k = 0; k < 4; ++k) {
    const float2 ndc = float2(k & 1 ? ndc1.x : ndc0.x, k & 2 ? ndc1.y : ndc0.y);
    [unroll] for (uint d = 0; d < 2; ++d) {
      const float depth = d ? zFar : zNear;
      const float2 v = float2(ndc.x * depth / f.projectionX, -ndc.y * depth / f.projectionY);
      lo.xy = min(lo.xy, v);
      hi.xy = max(hi.xy, v);
    }
  }
  RWByteAddressBuffer clusters = gBuffers[NonUniformResourceIndex(f.clusterBuffer)];
  uint count = 0;
  for (uint i = 0; i < f.lightCount && count < kClusterMaxLights; ++i) {
    const LocalLight light = loadLight(f, i);
    const float3 c = mul(float4(light.position, 1.0), f.view).xyz;
    const float3 nearest = clamp(c, lo, hi);
    const float3 d = c - nearest;
    if (dot(d, d) <= light.range * light.range) {
      clusters.Store(clusterOffset(id) + 4 + count * 4, i);
      ++count;
    }
  }
  clusters.Store(clusterOffset(id), count);
}
