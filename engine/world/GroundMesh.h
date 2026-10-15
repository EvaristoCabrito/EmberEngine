#pragma once

// The board's ground in Engine2's two player-selectable looks (C:/Engine2/src/render/terrainMesh.ts),
// ported to C++ with the same shapes, constants and blend weights:
//   smooth  one continuous heightfield; every point reads the hex under a gently warped position, so
//           terraces and shores wander naturally around the hex layout
//   hard    a crisp hex cliff at every height step; same-level ground flows seamlessly
// Both read the same cells: gameplay is identical. Also the water sheet and ground heights for
// placing things.

#include <cstdint>
#include <vector>

#include "world/Board.h"

namespace ember::e2 {

enum class EdgeStyle { Smooth, Hard };

// Per vertex: 8 surface weights (grass, forest, pavers, snow, rock, gravel, ash, plains049), then one
// weight per ground layer (BoardData::groundLayerCount).
struct GroundGeometry {
  std::vector<float> positions;  // 3 per vertex
  std::vector<float> normals;    // 3 per vertex
  std::vector<float> weights;    // stride per vertex
  std::vector<uint32_t> indices; // empty = one triangle per 3 vertices
  uint32_t stride = 0;
  size_t vertexCount() const { return positions.size() / 3; }
};

struct WaterGeometry {
  std::vector<float> positions, normals, uvs;
  std::vector<uint32_t> indices;
};

GroundGeometry buildSmoothGround(const Board& board);
GroundGeometry buildHardGround(const Board& board);
// Empty when the map has no water.
WaterGeometry buildWater(const Board& board, EdgeStyle style);
// Height of the ground at a point in the given look, for placing things on it.
double groundHeightAt(const Board& board, EdgeStyle style, double x, double z);
// The smooth look's domain warp.
std::pair<double, double> warp(double x, double z);

}  // namespace ember::e2
