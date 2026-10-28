#pragma once

// Ember's 3D water (the Map Editor's "Água 3D" brush): Ember's ThreeWater sheet
// (src/game/gfx/three/ThreeWater.ts sampleWater / sampleWaterPatches / rebuild), ported with the same
// sampling, bounds and triangles. Water comes from three map fields: waterLevels (a level per hex, 0..12),
// waterFootprints (the brush's size and shape per hex) and waterPatches (free brush stamps). Coordinates are
// Ember's battle board in tile units (x right, y down, z up; hex centre x = sqrt3 (col + (row & 1) / 2 + 1/2),
// y = 2.4 + 1.5 row + 1). Checked against Ember's own rebuild by Cooker check-water3d.

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ember {

struct WaterPatch {
  double x = 0, y = 0, level = 0, size = 1;
  bool square = false;
};
struct WaterFootprint {
  std::optional<double> size;
  bool square = false;
};

struct Water3DMap {
  int cols = 0, rows = 0;
  std::vector<std::string> tiles;
  std::vector<std::optional<double>> levels;
  std::vector<std::optional<WaterFootprint>> footprints;
  std::vector<WaterPatch> patches;
  std::optional<bool> squareTiles;
  std::string version = "v2";  // waterVersion: v1..v4
  bool any() const;
  // From a map draft (Ember's MissionDraft fields).
  static Water3DMap fromDraft(const nlohmann::json& draft);
};

struct WaterSample {
  double coverage = 0, level = 0;
};
WaterSample sampleWater(double x, double y, int cols, int rows, const std::vector<std::optional<double>>& levels,
                        const std::vector<std::optional<WaterFootprint>>& footprints, bool extendEdges);
WaterSample sampleWaterPatches(double x, double y, const std::vector<const WaterPatch*>& patches);
// Ember mapFloor.ts: every non-void tile fills one rectangle (or the map says so).
bool hasSquareMapBorder(const std::vector<std::string>& tiles, int cols, int rows, std::optional<bool> squareTiles);

struct Water3DSheet {
  // per vertex, as Ember's float32 attributes: position (x, -y, z) x tile, uv (x / 4, -y / 4), coverage,
  // waterDepth (z above the ground, in tiles)
  std::vector<float> position, uv, coverage, depth;
  std::vector<uint32_t> indices;
  bool empty() const { return indices.empty(); }
};
struct Water3DBoard {
  double minX = 0, maxX = 0, minY = 0, maxY = 0;
};
// ThreeWater.rebuild; `ground(x, y)` gets three's frame (x * tile, -y * tile), as Ember's landscape.heightAt.
Water3DSheet buildWater3D(int cols, int rows, double tile, const std::vector<std::optional<double>>& levels,
                          const std::function<bool(int, int)>& allowed, const std::function<double(double, double)>& ground,
                          const std::vector<std::optional<WaterFootprint>>& footprints, const std::vector<WaterPatch>& patches,
                          const std::optional<Water3DBoard>& board);
// As ThreeBattleRenderer.syncWater: allowed = not void, the board rectangle on straight-border maps.
Water3DSheet buildWater3D(const Water3DMap& map, const std::function<double(double, double)>& ground);

// Cooker check-water3d: every case of export_water3d_refs.ts rebuilt here and compared. 0 = identical.
int checkWater3D(const std::string& refsPath);

}  // namespace ember
