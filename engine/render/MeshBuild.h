#pragma once

// The cooked mesh format's geometry builder (MeshFormat.h): parts of triangles split into clusters arranged in a
// cluster LOD DAG (meshoptimizer clusterlod), a BVH over the groups, the header and sections laid out. Shared by the
// Cooker (glTF models, tools/MeshCooker.cpp) and meshes the game builds itself (Ember's architecture, MapStage), so
// both go through the same GPU path: culling, LOD, shadows, the lighting contract.

#include <cstdint>
#include <vector>

#include "render/MeshFormat.h"

namespace ember {

// One material's triangles, one entry per unique vertex.
struct MeshBuildPart {
  std::vector<float> positions, normals, uvs, tangents, colors;  // 3, 3, 2, 4, 4 floats per vertex
  std::vector<uint32_t> indices;
  bool hasColors = false;
};

class MeshBuild {
 public:
  // Adds one part drawn with materials[materialIndex]. `textured`: a texture samples its UVs (they are kept faithful
  // when simplified); `positionError`: the normals are noisy (a remeshed proxy), simplify by position only.
  bool addPart(const MeshBuildPart& part, uint16_t materialIndex, bool doubleSided, bool textured, bool positionError);
  // The cooked bytes (header + sections), or empty when nothing was added.
  std::vector<uint8_t> finish(const std::vector<MeshMaterial>& materials, uint64_t sourceTriangles);

  size_t clusterCount() const { return clusters_.size(); }
  size_t groupCount() const { return groups_.size(); }
  uint32_t levelCount() const { return levels_; }
  uint64_t dagTriangles() const { return dagTriangles_; }

 private:
  std::vector<MeshVertex> vertices_;
  std::vector<uint32_t> clusterVertices_;
  std::vector<uint8_t> clusterTriangles_;
  std::vector<MeshCluster> clusters_;
  std::vector<MeshGroup> groups_;
  struct GroupBounds {
    float center[3], radius, error;
    int depth;
  };
  std::vector<GroupBounds> groupBounds_;
  uint64_t dagTriangles_ = 0;
  uint32_t levels_ = 0;
};

// Octahedral unit-vector encoding, 2 x snorm16 (the lowest bit of y: a tangent's bitangent sign).
uint32_t meshOctEncode(const float* n, bool negativeSign = false);
// A linear colour channel as its sRGB byte.
uint8_t meshSrgbByte(float linear);

}  // namespace ember
