#pragma once

// Ember's 3D architecture (model3d wall / doorway / door / secretDoor: plain and thick walls, arched doorways and doors,
// castle walls, towers and gates, temple facades, rock walls, the timber and dungeon reference doors), built as Engine2
// builds it: Ember's createWallGeometry (src/game/gfx/three/ThreeWalls.ts + ThreeCastle / ThreeTemple / ThreeRocks /
// ThreeReferenceDoors) in its physical form, placed by Engine2's decor.ts addArchitecture (square-grid cells, joins to
// the neighbouring architecture, a human-sized height, the piece stood on the ground). Y up, rows along +Z, like the
// board.
//
// Cooker check-architecture compares every piece's solid with Engine2's own (export_architecture_refs.ts).

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "world/ThreeGeometry.h"

namespace ember::e2 {

// The DecorationDef fields the builders read (content/engine2/editor-catalog.json "editor"."architectureDefs").
struct ArchitectureDef {
  std::string id, model3d;
  double heightScale = 1, wallThicknessScale = 1;
  std::optional<double> architectureSpan;
  std::string doorStyle, rockStyle, templeStyle, castleStyle, wallTexture;
  bool thickWall = false, dungeonReference = false, connects = true;  // connects: joins its neighbours (not rocks / trees / props)
};

struct ArchitectureDefs {
  std::map<std::string, ArchitectureDef> defs;
  bool load(const std::filesystem::path& catalogFile);
  const ArchitectureDef* find(const std::string& id) const;
};

// ThreeWalls.ts createWallGeometry: Z up (Ember's own axes), x along the board, y = -z of the board.
three::Geometry wallGeometry(const ArchitectureDef& def, double tile, int rotation, const std::vector<std::pair<double, double>>& connections,
                             std::pair<double, double> origin, bool projectHeight, double textureHeightScale);

// A piece's wall texture as a cooked asset (Cooker architecture): "/game/textures/walls/crypt-v2.png" ->
// "architecture/textures/walls/crypt-v2.tex" in build/meshes/architecture.pak.
inline std::string architectureTextureAsset(const std::string& wallTexture) {
  std::string path = wallTexture.starts_with("/game/") ? wallTexture.substr(6) : wallTexture;
  if (const size_t dot = path.rfind('.'); dot != std::string::npos) path.resize(dot);
  return "architecture/" + path + ".tex";
}

// One placed piece in the world (decor.ts addArchitecture): its solid in board space (Y up) and where it stands.
struct ArchitecturePiece {
  std::string id;
  three::Geometry geometry;  // already in world space: rotated upright, moved to its cell, stood on the ground
  double x = 0, y = 0, z = 0;
  int anchorX = 0, anchorY = 0;
};

// decor.ts addArchitecture's solid for one piece: createWallGeometry(def at 3.5 x 1.6 its height, tile 1, rot,
// connections, origin, physical, texture height 3.5) turned upright (rotateX -90 degrees), not yet moved to its cell.
three::Geometry pieceGeometry(const ArchitectureDef& def, int rotation, const std::vector<std::pair<double, double>>& connections,
                              std::pair<double, double> origin);

// Cooker check-architecture: every reference case (export_architecture_refs.ts) built here. Returns the differences.
int checkArchitecture(const std::filesystem::path& refs, const ArchitectureDefs& defs);

}  // namespace ember::e2
