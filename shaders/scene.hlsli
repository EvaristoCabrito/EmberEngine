// The GPU-driven mesh scene: layouts shared with engine/render/MeshFormat.h, MeshGpu.h and the
// renderer's MeshFrame (scalar packing; every record is read through its device address).
#ifndef EMBER_SCENE_HLSLI
#define EMBER_SCENE_HLSLI

#include "common.hlsli"

struct MeshVertex {  // 32 bytes
  float3 position;
  uint normal;   // octahedral, 2 x snorm16
  uint tangent;  // octahedral, 2 x snorm16; lowest bit of y = bitangent sign (1 = negative)
  float2 uv;
  uint color;    // RGBA8, RGB sRGB-encoded
};

struct MeshCluster {  // 48 bytes
  float3 center;
  float radius;
  uint cone;            // axis x, y, z, cutoff: 4 x snorm8
  uint vertexOffset;    // into clusterVertices
  uint triangleOffset;  // bytes into triangles
  uint countsMaterial;  // vertexCount (8) | triangleCount (8) | material (16)
  int refinedGroup;
  uint group;
  uint2 reserved;
};

struct MeshGroup {  // 32 bytes
  float3 center;
  float radius;
  float error;
  uint depth;
  uint firstCluster;
  uint clusterCount;
};

struct MeshNode {  // 32 bytes
  float3 center;
  float radius;
  float error;
  int group;
  uint childOffset;
  uint childCount;
};

struct MeshGpu {  // 80 bytes
  uint64_t vertices;
  uint64_t clusterVertices;
  uint64_t triangles;
  uint64_t clusters;
  uint64_t groups;
  uint64_t nodes;
  uint levelCount;
  uint materialBase;
  uint clusterCount;
  uint ready;
  float4 sphere;
};

struct InstanceGpu {  // 64 bytes
  float4 rows[3];
  uint mesh;
  float scale;
  uint flags;
  uint pad;
};

struct MaterialGpu {  // 80 bytes
  float4 baseColor;
  float3 emissive;
  float metallic;
  float roughness;
  float normalScale;
  float occlusionStrength;
  float alphaCutoff;
  uint flags;
  uint baseColorTexture;
  uint normalTexture;
  uint ormTexture;
  uint emissiveTexture;
  uint3 pad;
};

static const uint kNoTexture = 0xFFFFFFFFu;
static const uint kMaterialAlphaMask = 1u;
static const uint kMaterialDoubleSided = 2u;

// Matches ember::MeshFrame (Renderer.cpp): everything the mesh passes know about this frame.
struct MeshFrame {
  float4x4 viewProjection;
  float4 frustum[5];        // world-space planes (xyz normal pointing inside, w distance); no far plane
  float3 cameraPosition;
  float lodScale;           // projection[1][1] / 2 * screen height / pixel error threshold
  float2 screenSize;
  float zNear;
  uint instanceCount;
  uint meshCount;
  uint maxQueue;
  uint maxClusters;
  uint materialSampler;
  uint workBuffer;          // gBuffers index: counters, indirect arguments, queues, cluster list
  uint64_t meshes;
  uint64_t materials;
  uint64_t instances;
  float3 sunDirection;      // towards the sun
  float sunIntensity;
  float3 sunColor;
  float ambientIntensity;
  float3 skyColor;
  uint pad0;
  float3 groundColor;
  uint pad1;
};

// The work buffer: counters, indirect arguments, then the queues.
static const uint kCounterQueue0 = 0;
static const uint kCounterQueue1 = 1;
static const uint kCounterClusters = 2;
static const uint kCounterQueueOverflow = 3;
static const uint kCounterClusterOverflow = 4;
static const uint kCounterInstances = 5;
static const uint kTraverseArgs = 64;   // byte offsets
static const uint kDrawArgs = 80;
static const uint kQueuesOffset = 128;

// Byte offsets in the work buffer.
uint queueOffset(MeshFrame f, uint queue) { return kQueuesOffset + queue * f.maxQueue * 8; }
uint clusterListOffset(MeshFrame f) { return kQueuesOffset + 2 * f.maxQueue * 8; }
RWByteAddressBuffer work(MeshFrame f) { return gBuffers[NonUniformResourceIndex(f.workBuffer)]; }

MeshFrame loadFrame(uint64_t address) { return vk::RawBufferLoad<MeshFrame>(address); }
InstanceGpu loadInstance(MeshFrame f, uint i) { return vk::RawBufferLoad<InstanceGpu>(f.instances + uint64_t(i) * 64); }
MeshGpu loadMesh(MeshFrame f, uint i) { return vk::RawBufferLoad<MeshGpu>(f.meshes + uint64_t(i) * 80); }
MaterialGpu loadMaterial(MeshFrame f, uint i) { return vk::RawBufferLoad<MaterialGpu>(f.materials + uint64_t(i) * 80); }
MeshCluster loadCluster(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshCluster>(m.clusters + uint64_t(i) * 48); }
MeshGroup loadGroup(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshGroup>(m.groups + uint64_t(i) * 32); }
MeshNode loadNode(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshNode>(m.nodes + uint64_t(i) * 32); }
MeshVertex loadVertex(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshVertex>(m.vertices + uint64_t(i) * 32); }
uint loadClusterVertex(MeshGpu m, uint i) { return vk::RawBufferLoad<uint>(m.clusterVertices + uint64_t(i) * 4); }

// Local indices of one cluster triangle (3 bytes at triangleOffset + 3 * t).
uint3 loadTriangle(MeshGpu m, MeshCluster c, uint t) {
  const uint64_t byteAddress = m.triangles + c.triangleOffset + t * 3;
  const uint64_t aligned = byteAddress & ~uint64_t(3);
  const uint shift = uint(byteAddress - aligned) * 8;
  const uint2 words = vk::RawBufferLoad<uint2>(aligned, 4);
  const uint packed = shift == 0 ? words.x : (words.x >> shift) | (words.y << (32 - shift));
  return uint3(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF);
}

uint clusterVertexCount(MeshCluster c) { return c.countsMaterial & 0xFF; }
uint clusterTriangleCount(MeshCluster c) { return (c.countsMaterial >> 8) & 0xFF; }
uint clusterMaterial(MeshCluster c) { return c.countsMaterial >> 16; }

float3 transformPoint(InstanceGpu inst, float3 p) {
  const float4 h = float4(p, 1.0);
  return float3(dot(inst.rows[0], h), dot(inst.rows[1], h), dot(inst.rows[2], h));
}
float3 transformDirection(InstanceGpu inst, float3 v) {
  return float3(dot(inst.rows[0].xyz, v), dot(inst.rows[1].xyz, v), dot(inst.rows[2].xyz, v));
}

float2 snorm16x2(uint packed) {
  const int2 v = int2(int(packed << 16) >> 16, int(packed) >> 16);
  return max(float2(v) / 32767.0, -1.0);
}
float3 octDecode(float2 e) {
  float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
  const float t = saturate(-n.z);
  n.xy += float2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
  return normalize(n);
}
float3 decodeNormal(uint packed) { return octDecode(snorm16x2(packed)); }
float4 decodeTangent(uint packed) { return float4(octDecode(snorm16x2(packed & 0xFFFEFFFFu)), (packed & 0x10000u) ? -1.0 : 1.0); }

float srgbToLinear(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float4 decodeColor(uint packed) {
  const float4 c = float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
  return float4(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b), c.a);
}

// Screen-space error in pixels of bounds (mesh-space centre, radius, error) under an instance.
float screenError(MeshFrame f, InstanceGpu inst, float3 center, float radius, float error) {
  const float3 c = transformPoint(inst, center);
  const float d = length(c - f.cameraPosition) - radius * inst.scale;
  return error * inst.scale / max(d, f.zNear) * f.lodScale;
}

bool sphereInFrustum(MeshFrame f, float3 center, float radius) {
  [unroll] for (uint i = 0; i < 5; ++i)
    if (dot(f.frustum[i].xyz, center) + f.frustum[i].w < -radius) return false;
  return true;
}

#endif
