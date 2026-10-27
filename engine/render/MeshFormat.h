#pragma once

#include <cstdint>

#include "assets/AssetId.h"

namespace ember {

// Cooked mesh data, shared by the cooker and the runtime. All little-endian.
//
// One asset per source model: meshes/<name>.mesh. The model is split into clusters (meshlets) of up to
// kMeshClusterMaxVertices vertices / kMeshClusterMaxTriangles triangles, arranged in a cluster LOD DAG
// (meshoptimizer clusterlod): simplified clusters replace groups of more detailed ones, so the GPU picks
// per cluster how much detail the screen needs. A BVH over the groups (one tree per DAG level) lets the GPU
// skip whole subtrees.
//
// Layout after the MeshHeader, each section 16-byte aligned (meshSections() gives the offsets):
//   MeshVertex       vertices[vertexCount]
//   uint32_t         clusterVertices[clusterVertexCount]   per cluster: indices into vertices[]
//   uint8_t          clusterTriangles[triangleBytes]       per cluster: 3 local indices per triangle, padded to 4
//   MeshCluster      clusters[clusterCount]
//   MeshGroup        groups[groupCount]
//   MeshNode         nodes[nodeCount]                       nodes[0..levelCount) are the per-level roots
//   MeshMaterial     materials[materialCount]
//
// LOD rule (screen error of bounds b: b.error / max(distance(b.center, camera) - b.radius, zNear) * proj / 2):
//   descend into a node while its error is over the threshold; in a leaf's group, draw cluster c if
//   c.refinedGroup < 0 or groups[c.refinedGroup].error <= threshold.

inline constexpr uint32_t kMeshMagic = 0x4853454D;  // "MESH"
inline constexpr uint32_t kMeshFormatVersion = 1;
inline constexpr uint32_t kMeshClusterMaxVertices = 64;   // one RDNA 2 wave: one vertex and one triangle
inline constexpr uint32_t kMeshClusterMaxTriangles = 64;  // per thread (AMD's guidance: 64-128)
inline constexpr uint32_t kMeshNodeWidth = 8;             // BVH children per internal node

struct MeshHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t vertexCount;
  uint32_t clusterVertexCount;
  uint32_t triangleBytes;
  uint32_t clusterCount;
  uint32_t groupCount;
  uint32_t nodeCount;
  uint32_t levelCount;
  uint32_t materialCount;
  uint32_t sourceTriangles;  // full detail, for reports
  uint32_t reserved;
  float boundsMin[3];        // mesh space, metres (glTF: +Y up)
  float boundsRadius;        // sphere around the whole mesh, centred at boundsCenter
  float boundsMax[3];
  float pad0;
  float boundsCenter[3];
  float pad1;
};
static_assert(sizeof(MeshHeader) == 96);

struct MeshVertex {
  float position[3];
  uint32_t normal;    // octahedral, 2 x snorm16
  uint32_t tangent;   // octahedral, 2 x snorm16; the lowest bit of y holds the bitangent sign (1 = negative)
  float uv[2];
  uint32_t color;     // RGBA8, RGB sRGB-encoded (glTF COLOR_0 is linear; 8-bit linear would band in darks), white when absent
};
static_assert(sizeof(MeshVertex) == 32);

struct MeshCluster {
  float center[3];    // bounding sphere, mesh space (culling only; not monotonic across LODs)
  float radius;
  int8_t coneAxis[3];  // backface cone (meshopt_Bounds, snorm8); cutoff 127 = never cull
  int8_t coneCutoff;
  uint32_t vertexOffset;    // into clusterVertices
  uint32_t triangleOffset;  // byte offset into clusterTriangles
  uint8_t vertexCount;
  uint8_t triangleCount;
  uint16_t material;
  int32_t refinedGroup;     // group this cluster was simplified from, or -1 for full detail
  uint32_t group;           // group this cluster belongs to
  uint32_t reserved[2];
};
static_assert(sizeof(MeshCluster) == 48);

struct MeshGroup {
  float center[3];   // bounds of the simplified version of this group (clusters whose refinedGroup is this one)
  float radius;
  float error;       // FLT_MAX for groups never simplified further
  uint32_t depth;    // DAG level
  uint32_t firstCluster;  // the group's own clusters: clusters[firstCluster .. firstCluster + clusterCount)
  uint32_t clusterCount;
};
static_assert(sizeof(MeshGroup) == 32);

struct MeshNode {
  float center[3];   // worst case over the subtree's groups
  float radius;
  float error;
  int32_t group;     // leaf: the group; internal: -1
  uint32_t childOffset;
  uint32_t childCount;
};
static_assert(sizeof(MeshNode) == 32);

enum MeshMaterialFlags : uint32_t {
  kMaterialAlphaMask = 1u << 0,    // alpha-tested at alphaCutoff
  kMaterialDoubleSided = 1u << 1,
  kMaterialFlipbook = 1u << 2,     // 4x4 atlas frame comes from InstanceGpu::pad
};

// glTF metallic-roughness. Textures are separate assets (TextureFormat.h); 0 = none (factor only).
struct MeshMaterial {
  float baseColor[4];       // linear
  float emissive[3];        // linear, multiplied by the emissive strength
  float metallic;
  float roughness;
  float normalScale;
  float occlusionStrength;
  float alphaCutoff;
  uint32_t flags;
  uint32_t reserved[3];
  AssetId baseColorTexture;  // BC7 sRGB, alpha = opacity
  AssetId normalTexture;     // BC5: tangent-space X, Y (Z rebuilt)
  AssetId ormTexture;        // BC7 linear: R occlusion, G roughness, B metallic
  AssetId emissiveTexture;   // BC7 sRGB
};
static_assert(sizeof(MeshMaterial) == 96);

struct MeshSections {
  uint64_t vertices, clusterVertices, clusterTriangles, clusters, groups, nodes, materials, end;
};

constexpr uint64_t meshAlign(uint64_t offset) { return (offset + 15) & ~uint64_t{15}; }

constexpr MeshSections meshSections(const MeshHeader& h) {
  MeshSections s{};
  s.vertices = meshAlign(sizeof(MeshHeader));
  s.clusterVertices = meshAlign(s.vertices + uint64_t{h.vertexCount} * sizeof(MeshVertex));
  s.clusterTriangles = meshAlign(s.clusterVertices + uint64_t{h.clusterVertexCount} * sizeof(uint32_t));
  s.clusters = meshAlign(s.clusterTriangles + h.triangleBytes);
  s.groups = meshAlign(s.clusters + uint64_t{h.clusterCount} * sizeof(MeshCluster));
  s.nodes = meshAlign(s.groups + uint64_t{h.groupCount} * sizeof(MeshGroup));
  s.materials = meshAlign(s.nodes + uint64_t{h.nodeCount} * sizeof(MeshNode));
  s.end = s.materials + uint64_t{h.materialCount} * sizeof(MeshMaterial);
  return s;
}

}  // namespace ember
