#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace ember {

// GPU-side records of the mesh scene, read by shaders/scene.hlsli (same layout, scalar packing).

// One loaded mesh: device addresses of its cooked sections (MeshFormat.h) in the geometry buffer.
struct MeshGpu {
  uint64_t vertices;         // MeshVertex[]
  uint64_t clusterVertices;  // uint32_t[]
  uint64_t triangles;        // uint8_t[] (3 per triangle, 4-byte aligned per cluster)
  uint64_t clusters;         // MeshCluster[]
  uint64_t groups;           // MeshGroup[]
  uint64_t nodes;            // MeshNode[]
  uint32_t levelCount;       // nodes[0 .. levelCount) are the per-level BVH roots
  uint32_t materialBase;     // first entry in the scene's MaterialGpu[]
  uint32_t clusterCount;
  uint32_t ready;            // 0 until the geometry upload finished (instances of it are skipped)
  glm::vec4 sphere;          // mesh-space centre, radius
};
static_assert(sizeof(MeshGpu) == 80);

// One placed object.
struct InstanceGpu {
  glm::vec4 rows[3];  // world transform, rows of a 3x4 matrix: world = rows * (p, 1)
  uint32_t mesh;
  float scale;        // largest axis scale (bounds and LOD error)
  uint32_t flags;     // kInstance* below
  uint32_t pad;        // flipbook frame for meshes with kMaterialFlipbook
};
static_assert(sizeof(InstanceGpu) == 64);
// InstanceGpu::flags (shaders/cull.hlsl): drawn only into the sun's shadow map (a roof that darkens a room without hiding it from
// the camera: MapStage's window light), never dropped by the fog of war's never-seen test (it stands on no hex), and wall faces
// faded by the visibility shader inside the camera's 5 m radius.
inline constexpr uint32_t kInstanceShadowOnly = 1u, kInstanceAlwaysSeen = 2u, kInstanceCameraFade = 4u;

inline constexpr uint32_t kNoTextureIndex = 0xFFFFFFFFu;

struct MaterialGpu {
  glm::vec4 baseColor;  // linear
  glm::vec3 emissive;
  float metallic;
  float roughness;
  float normalScale;
  float occlusionStrength;
  float alphaCutoff;
  uint32_t flags;          // MeshMaterialFlags
  uint32_t baseColorTexture;  // bindless indices; kNoTextureIndex until the texture is resident
  uint32_t normalTexture;
  uint32_t ormTexture;
  uint32_t emissiveTexture;
  uint32_t pad[3];
};
static_assert(sizeof(MaterialGpu) == 80);

}  // namespace ember
