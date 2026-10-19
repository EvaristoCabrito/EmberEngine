#pragma once

// The board's ground in Engine2's two player-selectable looks (C:/Engine2/src/render/terrainMesh.ts),
// ported to C++ with the same shapes, constants and blend weights:
//   smooth  one continuous heightfield; every point reads the hex under a gently warped position, so
//           terraces and shores wander naturally around the hex layout
//   hard    a crisp hex cliff at every height step; same-level ground flows seamlessly
//   tabletop every hex its own beveled column down to the board's base, a hairline gap between hexes and a
//            seam at every height step: stacked tabletop pieces (R8; Engine2's diorama.html look 3,
//            buildHexTerrain(true): top at 0.93 of the radius, 0.07 bevel, walls at 0.985)
// Both read the same cells: gameplay is identical. Also the water sheet and ground heights for
// placing things.

#include <cstdint>
#include <vector>

#include "world/Board.h"

namespace ember::e2 {

enum class EdgeStyle { Smooth, Hard, Tabletop };

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
// The board's sides: walls from every open edge of an indexed ground (the smooth look's thin cut lip, a void's rim) straight down
// to `base`, so a raised edge never shows a hole under it.
void closeBoardSides(GroundGeometry& g, double base);
GroundGeometry buildTabletopGround(const Board& board);
// Empty when the map has no water.
WaterGeometry buildWater(const Board& board, EdgeStyle style);
// Height of the ground at a point in the given look, for placing things on it.
double groundHeightAt(const Board& board, EdgeStyle style, double x, double z);
// The smooth look's domain warp.
std::pair<double, double> warp(double x, double z);

}  // namespace ember::e2
