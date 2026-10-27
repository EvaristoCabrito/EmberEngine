// Rasterises the clusters the traversal picked into the visibility buffer (R32_UINT) and depth.
//   ASMain  one thread per cluster-list entry: frustum and backface-cone culling, survivors go to the
//           mesh shader in one workgroup each.
//   MSMain  one workgroup per cluster: up to 64 vertices and 64 triangles, one of each per thread (the
//           shape RDNA 2 exports best).
//   PSMain  writes (cluster-list slot << 6 | triangle); alpha-tested materials clip first.
#include "scene.hlsli"

struct Push {
  uint64_t frame;
  uint pass;  // 0 early: cluster-list entries [0, early); 1 late: [early, total) + the early pass's retest list
};
[[vk::push_constant]] Push push;

static const uint kTaskGroup = 64;

struct Payload {
  uint slots[kTaskGroup];  // cluster-list slots
};
groupshared Payload payload;
groupshared uint survivors;

[numthreads(kTaskGroup, 1, 1)]
void ASMain(uint thread : SV_GroupIndex, uint3 group : SV_GroupID) {
  const MeshFrame f = loadFrame(push.frame);
  if (thread == 0) survivors = 0;
  GroupMemoryBarrierWithGroupSync();

  const uint linearGroup = group.x + group.y * 65535u;
  const uint early = work(f).Load(kCounterEarlyClusters * 4);
  const uint total = min(work(f).Load(kCounterClusters * 4), f.maxClusters);
  const uint index = linearGroup * kTaskGroup + thread;
  uint slot = ~0u;
  if (push.pass == 0) {
    if (index < early) slot = index;
  } else {
    const uint late = total - early, retest = min(work(f).Load(kCounterRetest * 4), f.maxClusters);
    if (index < late) slot = early + index;
    else if (index < late + retest) slot = work(f).Load(retestListOffset(f) + (index - late) * 4);
  }
  if (slot != ~0u) {
    const uint2 item = work(f).Load2(clusterListOffset(f) + slot * 8);
    const InstanceGpu inst = loadInstance(f, item.x);
    const MeshGpu mesh = loadMesh(f, inst.mesh);
    const MeshCluster cluster = loadCluster(mesh, item.y);
    const float3 center = transformPoint(inst, cluster.center);
    const float radius = cluster.radius * inst.scale;
    bool visible = sphereInFrustum(f, center, radius);
    // Backface cone (meshoptimizer's snorm8 cone, tested from the bounding sphere's centre).
    const int4 cone = int4(int(cluster.cone << 24) >> 24, int(cluster.cone << 16) >> 24, int(cluster.cone << 8) >> 24,
                           int(cluster.cone) >> 24);
    if (visible && cone.w < 127) {
      const float3 axis = normalize(transformDirection(inst, float3(cone.xyz) / 127.0));
      const float3 toCluster = center - f.cameraPosition;
      visible = dot(toCluster, axis) < float(cone.w) / 127.0 * length(toCluster) + radius;
    }
    if (visible && push.pass == 0 && f.earlyOcclusion != 0 &&
        !sphereVisibleHiZ(f, center, radius, f.previousView, f.previousProjectionX, f.previousProjectionY)) {
      // Hidden last frame: not drawn now, re-tested in the late pass against this frame's pyramid.
      uint at;
      work(f).InterlockedAdd(kCounterRetest * 4, 1, at);
      if (at < f.maxClusters) work(f).Store(retestListOffset(f) + at * 4, slot);
      visible = false;
    }
    // Late pass: clusters hidden behind what the early pass drew.
    if (visible && push.pass == 1) visible = sphereVisibleHiZ(f, center, radius);
    if (visible) {
      uint index;
      InterlockedAdd(survivors, 1, index);
      payload.slots[index] = slot;
    }
  }
  GroupMemoryBarrierWithGroupSync();
  DispatchMesh(f.costSplit == 1 ? 0 : survivors, 1, 1, payload);
}

struct VertexOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float3 worldPosition : TEXCOORD3;
};

struct PrimitiveOut {
  nointerpolation uint visibility : TEXCOORD1;
  nointerpolation uint material : TEXCOORD2;  // global material index, ~0 when no alpha test is needed
  bool cull : SV_CullPrimitive;
};

// Front faces wind this way in framebuffer space (counter-clockwise in glTF, after Vulkan's Y flip).
static const float kFrontFaceSign = -1.0;

groupshared float4 clipPositions[64];

[outputtopology("triangle")]
[numthreads(64, 1, 1)]
void MSMain(uint thread : SV_GroupIndex, uint3 group : SV_GroupID, in payload Payload taskPayload,
            out vertices VertexOut verts[64], out indices uint3 tris[64], out primitives PrimitiveOut prims[64]) {
  const MeshFrame f = loadFrame(push.frame);
  const uint slot = taskPayload.slots[group.x];
  const uint2 item = work(f).Load2(clusterListOffset(f) + slot * 8);
  const InstanceGpu inst = loadInstance(f, item.x);
  const MeshGpu mesh = loadMesh(f, inst.mesh);
  const MeshCluster cluster = loadCluster(mesh, item.y);
  const uint vertexCount = clusterVertexCount(cluster), triangleCount = clusterTriangleCount(cluster);
  SetMeshOutputCounts(vertexCount, triangleCount);
  if (thread == 0) work(f).InterlockedAdd(kCounterTriangles * 4, triangleCount);

  if (thread < vertexCount) {
    const MeshVertex v = loadVertex(mesh, loadClusterVertex(mesh, cluster.vertexOffset + thread));
    const float3 worldPosition = transformPoint(inst, v.position);
    const float4 clipPos = mul(float4(worldPosition, 1.0), f.viewProjection);
    verts[thread].position = clipPos;
    verts[thread].uv = v.uv;
    verts[thread].worldPosition = worldPosition;
    clipPositions[thread] = clipPos;
  }
  GroupMemoryBarrierWithGroupSync();
  if (thread < triangleCount) {
    const uint3 t = loadTriangle(mesh, cluster, thread);
    tris[thread] = t;
    prims[thread].visibility = (slot << 6) | thread;
    const uint material = mesh.materialBase + clusterMaterial(cluster);
    const uint flags = loadMaterial(f, material).flags;
    prims[thread].material = (flags & kMaterialAlphaMask) ? material : ~0u;
    // Back faces of single-sided materials (only when all three corners are in front of the camera).
    const float4 a = clipPositions[t.x], b = clipPositions[t.y], c = clipPositions[t.z];
    bool cull = false;
    if (!(flags & kMaterialDoubleSided) && a.w > 0.0 && b.w > 0.0 && c.w > 0.0) {
      const float2 pa = a.xy / a.w, pb = b.xy / b.w, pc = c.xy / c.w;
      const float area = (pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x);
      cull = area * kFrontFaceSign <= 0.0;
    }
    prims[thread].cull = cull || f.costSplit == 2;
  }
}

// The pixel shader's view of the per-primitive outputs. DXC does not yet decorate pixel-shader inputs as
// PerPrimitiveEXT itself (microsoft/DirectXShaderCompiler#6862): decoration 5271 = PerPrimitiveEXT,
// capability 5283 = MeshShadingEXT.
struct PrimitiveIn {
  [[vk::ext_decorate(5271)]] nointerpolation uint visibility : TEXCOORD1;
  [[vk::ext_decorate(5271)]] nointerpolation uint material : TEXCOORD2;
};

[[vk::ext_extension("SPV_EXT_mesh_shader")]] [[vk::ext_capability(5283)]]
uint PSMain(VertexOut v, PrimitiveIn p) : SV_Target0 {
  const MeshFrame f = loadFrame(push.frame);
  const uint slot = p.visibility >> 6;
  const uint2 item = work(f).Load2(clusterListOffset(f) + slot * 8);
  const InstanceGpu inst = loadInstance(f, item.x);
  if (p.material != ~0u) {
    const MaterialGpu m = loadMaterial(f, p.material);
    float alpha = m.baseColor.a;
    if (m.baseColorTexture != kNoTexture)
      alpha *= gTextures[NonUniformResourceIndex(m.baseColorTexture)].Sample(gSamplers[f.materialSampler],
                                                                              flipbookUv(v.uv, m.flags, inst.pad)).a;
    clip(alpha - m.alphaCutoff);
  }
  if (inst.flags & kInstanceCameraFade) {
    const float opacity = smoothstep(0.0, 5.0, distance(v.worldPosition, f.cameraPosition));
    const uint2 pixel = uint2(v.position.xy);
    const float noise = frac(52.9829189 * frac(dot(float2(pixel), float2(0.06711056, 0.00583715))));
    clip(opacity - noise);
  }
  return p.visibility;
}

// The sun's shadow map: depth only; alpha-tested materials still cut their holes.
[[vk::ext_extension("SPV_EXT_mesh_shader")]] [[vk::ext_capability(5283)]]
void PSShadow(VertexOut v, PrimitiveIn p) {
  if (p.material != ~0u) {
    const MeshFrame f = loadFrame(push.frame);
    const uint slot = p.visibility >> 6;
    const uint2 item = work(f).Load2(clusterListOffset(f) + slot * 8);
    const InstanceGpu inst = loadInstance(f, item.x);
    const MaterialGpu m = loadMaterial(f, p.material);
    float alpha = m.baseColor.a;
    if (m.baseColorTexture != kNoTexture)
      alpha *= gTextures[NonUniformResourceIndex(m.baseColorTexture)].Sample(gSamplers[f.materialSampler],
                                                                              flipbookUv(v.uv, m.flags, inst.pad)).a;
    clip(alpha - m.alphaCutoff);
  }
}
