#include "render/MeshBuild.h"

#include <meshoptimizer.h>
#define CLUSTERLOD_IMPLEMENTATION
#include <clusterlod.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "core/Log.h"

namespace ember {

namespace {
int16_t snorm16(float v) { return static_cast<int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f)); }
}  // namespace

uint32_t meshOctEncode(const float* n, bool negativeSign) {
  const float l1 = std::abs(n[0]) + std::abs(n[1]) + std::abs(n[2]);
  float x = l1 > 0.0f ? n[0] / l1 : 0.0f, y = l1 > 0.0f ? n[1] / l1 : 0.0f;
  if (n[2] < 0.0f) {
    const float ox = (1.0f - std::abs(y)) * (x >= 0.0f ? 1.0f : -1.0f);
    const float oy = (1.0f - std::abs(x)) * (y >= 0.0f ? 1.0f : -1.0f);
    x = ox;
    y = oy;
  }
  uint16_t ux = static_cast<uint16_t>(snorm16(x)), uy = static_cast<uint16_t>(snorm16(y));
  uy = static_cast<uint16_t>((uy & ~1u) | (negativeSign ? 1u : 0u));
  return ux | (uint32_t{uy} << 16);
}

uint8_t meshSrgbByte(float linear) {
  const float c = std::clamp(linear, 0.0f, 1.0f);
  const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
  return static_cast<uint8_t>(std::lround(s * 255.0f));
}

bool MeshBuild::addPart(const MeshBuildPart& part, uint16_t materialIndex, bool doubleSided, bool textured, bool positionError) {
  const uint32_t vertexBase = static_cast<uint32_t>(vertices_.size());
  const size_t vertexCount = part.positions.size() / 3;
  for (size_t v = 0; v < vertexCount; ++v) {
    MeshVertex out{};
    std::memcpy(out.position, &part.positions[v * 3], 12);
    out.normal = meshOctEncode(&part.normals[v * 3]);
    out.tangent = meshOctEncode(&part.tangents[v * 4], part.tangents[v * 4 + 3] < 0.0f);
    out.uv[0] = part.uvs[v * 2];
    out.uv[1] = part.uvs[v * 2 + 1];
    const float* c = &part.colors[v * 4];
    out.color = meshSrgbByte(c[0]) | (uint32_t{meshSrgbByte(c[1])} << 8) | (uint32_t{meshSrgbByte(c[2])} << 16) |
                (uint32_t(std::lround(std::clamp(c[3], 0.0f, 1.0f) * 255.0f)) << 24);
    vertices_.push_back(out);
  }

  // Attributes the simplifier keeps faithful: normals always; UVs when a texture uses them; colours when the model is
  // vertex-coloured (the trees).
  const size_t stride = 9;
  std::vector<float> attributes(vertexCount * stride, 0.0f);
  for (size_t v = 0; v < vertexCount; ++v) {
    std::memcpy(&attributes[v * stride], &part.normals[v * 3], 12);
    std::memcpy(&attributes[v * stride + 3], &part.uvs[v * 2], 8);
    std::memcpy(&attributes[v * stride + 5], &part.colors[v * 4], 16);
  }
  const float uvWeight = textured ? 0.5f : 0.0f, colorWeight = part.hasColors ? 0.5f : 0.0f;
  // A remeshed proxy's normals come from the voxel surface and are noisy: its error is positional only.
  const float normalWeight = positionError ? 0.0f : 0.5f;
  const float weights[9] = {normalWeight, normalWeight, normalWeight, uvWeight, uvWeight, colorWeight, colorWeight, colorWeight, 0.0f};

  clodConfig config = clodDefaultConfig(kMeshClusterMaxTriangles);
  config.max_vertices = kMeshClusterMaxVertices;
  config.optimize_bounds = true;
  // A group's error never exceeds its own triangles' size: once they are under a pixel the simpler version is used,
  // even where seams or disconnected pieces make the measured error large.
  config.simplify_error_edge_limit = 1.0f;
  clodMesh mesh{};
  mesh.indices = part.indices.data();
  mesh.index_count = part.indices.size();
  mesh.vertex_count = vertexCount;
  mesh.vertex_positions = part.positions.data();
  mesh.vertex_positions_stride = 12;
  mesh.vertex_attributes = attributes.data();
  mesh.vertex_attributes_stride = stride * sizeof(float);
  mesh.attribute_weights = weights;
  mesh.attribute_count = stride;
  // clusterlod simplifies permissively (it may collapse across attribute discontinuities); the seams that must
  // survive are flagged here: UV seams when a texture uses them, colour seams on vertex-coloured models.
  mesh.attribute_protect_mask = (textured ? (1u << 3) | (1u << 4) : 0u) | (part.hasColors ? (1u << 5) | (1u << 6) | (1u << 7) : 0u);

  bool ok = true;
  clodBuild(config, mesh, [&](clodGroup group, const clodCluster* clusters, size_t count) -> int {
    const int groupIndex = static_cast<int>(groups_.size());
    MeshGroup g{};
    for (int i = 0; i < 3; ++i) g.center[i] = group.simplified.center[i];
    g.radius = group.simplified.radius;
    g.error = group.simplified.error;
    g.depth = static_cast<uint32_t>(group.depth);
    g.firstCluster = static_cast<uint32_t>(clusters_.size());
    g.clusterCount = static_cast<uint32_t>(count);
    groups_.push_back(g);
    groupBounds_.push_back({{group.simplified.center[0], group.simplified.center[1], group.simplified.center[2]}, group.simplified.radius,
                            group.simplified.error, group.depth});

    unsigned int local[256];
    unsigned char triangles[kMeshClusterMaxTriangles * 3];
    for (size_t c = 0; c < count; ++c) {
      const clodCluster& cluster = clusters[c];
      const size_t triangleCount = cluster.index_count / 3;
      if (cluster.vertex_count > kMeshClusterMaxVertices || triangleCount > kMeshClusterMaxTriangles) {
        logError("  cluster over the limits ({} vertices, {} triangles)", cluster.vertex_count, triangleCount);
        ok = false;
        continue;
      }
      const size_t unique = clodLocalIndices(local, triangles, cluster.indices, cluster.index_count);
      MeshCluster out{};
      for (int i = 0; i < 3; ++i) out.center[i] = cluster.bounds.center[i];
      out.radius = cluster.bounds.radius;
      const meshopt_Bounds cone = meshopt_computeClusterBounds(cluster.indices, cluster.index_count, part.positions.data(), vertexCount, 12);
      for (int i = 0; i < 3; ++i) out.coneAxis[i] = cone.cone_axis_s8[i];
      out.coneCutoff = doubleSided ? int8_t{127} : cone.cone_cutoff_s8;
      out.vertexOffset = static_cast<uint32_t>(clusterVertices_.size());
      out.triangleOffset = static_cast<uint32_t>(clusterTriangles_.size());
      out.vertexCount = static_cast<uint8_t>(unique);
      out.triangleCount = static_cast<uint8_t>(triangleCount);
      out.material = materialIndex;
      out.refinedGroup = cluster.refined;
      out.group = static_cast<uint32_t>(groupIndex);
      for (size_t i = 0; i < unique; ++i) clusterVertices_.push_back(vertexBase + local[i]);
      clusterTriangles_.insert(clusterTriangles_.end(), triangles, triangles + triangleCount * 3);
      while (clusterTriangles_.size() % 4) clusterTriangles_.push_back(0);
      clusters_.push_back(out);
      dagTriangles_ += triangleCount;
    }
    return groupIndex;  // becomes `refined` of the clusters simplified from this group
  });
  return ok;
}

std::vector<uint8_t> MeshBuild::finish(const std::vector<MeshMaterial>& materials, uint64_t sourceTriangles) {
  if (clusters_.empty()) return {};
  // BVH over the groups, one tree per DAG level.
  std::vector<clodGroup> groups;
  levels_ = 0;
  for (const GroupBounds& g : groupBounds_) {
    clodGroup c{};
    c.depth = g.depth;
    for (int i = 0; i < 3; ++i) c.simplified.center[i] = g.center[i];
    c.simplified.radius = g.radius;
    c.simplified.error = g.error;
    groups.push_back(c);
    levels_ = std::max(levels_, static_cast<uint32_t>(g.depth) + 1);
  }
  std::vector<clodNode> nodes(clodBuildHierarchyBound(groups.size(), kMeshNodeWidth, levels_));
  nodes.resize(clodBuildHierarchy(nodes.data(), groups.data(), groups.size(), kMeshNodeWidth, levels_));

  MeshHeader header{};
  header.magic = kMeshMagic;
  header.version = kMeshFormatVersion;
  header.vertexCount = static_cast<uint32_t>(vertices_.size());
  header.clusterVertexCount = static_cast<uint32_t>(clusterVertices_.size());
  header.triangleBytes = static_cast<uint32_t>(clusterTriangles_.size());
  header.clusterCount = static_cast<uint32_t>(clusters_.size());
  header.groupCount = static_cast<uint32_t>(groups_.size());
  header.nodeCount = static_cast<uint32_t>(nodes.size());
  header.levelCount = levels_;
  header.materialCount = static_cast<uint32_t>(materials.size());
  header.sourceTriangles = static_cast<uint32_t>(sourceTriangles);
  float lo[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, hi[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  for (const MeshVertex& v : vertices_)
    for (int i = 0; i < 3; ++i) lo[i] = std::min(lo[i], v.position[i]), hi[i] = std::max(hi[i], v.position[i]);
  float radius = 0.0f;
  for (int i = 0; i < 3; ++i) {
    header.boundsMin[i] = lo[i];
    header.boundsMax[i] = hi[i];
    header.boundsCenter[i] = (lo[i] + hi[i]) * 0.5f;
  }
  for (const MeshVertex& v : vertices_) {
    float d2 = 0.0f;
    for (int i = 0; i < 3; ++i) d2 += (v.position[i] - header.boundsCenter[i]) * (v.position[i] - header.boundsCenter[i]);
    radius = std::max(radius, d2);
  }
  header.boundsRadius = std::sqrt(radius);

  const MeshSections s = meshSections(header);
  std::vector<uint8_t> out(s.end, 0);
  std::memcpy(out.data(), &header, sizeof(header));
  std::memcpy(out.data() + s.vertices, vertices_.data(), vertices_.size() * sizeof(MeshVertex));
  std::memcpy(out.data() + s.clusterVertices, clusterVertices_.data(), clusterVertices_.size() * sizeof(uint32_t));
  std::memcpy(out.data() + s.clusterTriangles, clusterTriangles_.data(), clusterTriangles_.size());
  std::memcpy(out.data() + s.clusters, clusters_.data(), clusters_.size() * sizeof(MeshCluster));
  std::memcpy(out.data() + s.groups, groups_.data(), groups_.size() * sizeof(MeshGroup));
  for (size_t i = 0; i < nodes.size(); ++i) {
    const clodNode& c = nodes[i];
    const MeshNode node{{c.bounds.center[0], c.bounds.center[1], c.bounds.center[2]}, c.bounds.radius, c.bounds.error, c.group, c.child_offset, c.child_count};
    std::memcpy(out.data() + s.nodes + i * sizeof(MeshNode), &node, sizeof(node));
  }
  std::memcpy(out.data() + s.materials, materials.data(), materials.size() * sizeof(MeshMaterial));
  return out;
}

}  // namespace ember
