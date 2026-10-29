#pragma once

// Ember's 3D architecture (model3d wall / doorway / door / secretDoor: plain and thick walls, arched doorways and doors,
// castle walls, towers and gates, temple facades, rock walls, the timber and dungeon reference doors), built as Ember
// builds it: Ember's createWallGeometry (src/game/gfx/three/ThreeWalls.ts + ThreeCastle / ThreeTemple / ThreeRocks /
// ThreeReferenceDoors) in its physical form, placed by Ember's decor.ts addArchitecture (square-grid cells, joins to
// the neighbouring architecture, a human-sized height, the piece stood on the ground). Y up, rows along +Z, like the
// board.
//
// Cooker check-architecture compares every piece's solid with Ember's own (export_architecture_refs.ts).

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <array>
#include <vector>

#include "world/ThreeGeometry.h"

namespace ember {

// The DecorationDef fields the builders read (content/data/editor-catalog.json "editor"."architectureDefs").
struct ArchitectureDef {
  std::string id, model3d;
  double heightScale = 1, wallThicknessScale = 1;
  std::optional<double> architectureSpan;
  std::string doorStyle, rockStyle, templeStyle, castleStyle, wallTexture;
  bool thickWall = false, dungeonReference = false, connects = true;  // connects: joins its neighbours (not rocks / trees / props)
  // openAir (2026-10-29, the Monastery's precinct; the user: "vai ser util na cidade e fazenda"): a muralha, not a building's wall.
  // It joins its neighbours, blocks and casts shadow like any wall, but encloses no room: the yard it rings stays under the sky
  // (no roof slab over it, its hexes outside the probes' room), and it gets no shadow skirt.
  bool openAir = false;
  // EmberEngine's own pieces (content/models/architecture.json, ids ember-arch-*): "wall" (a joined run with a base and a string
  // course), "window" (a lancet window with a splayed inner embrasure, light through it), "slit" (an arrow loop), "portal" (an
  // arched doorway two hexes wide); the gothic parapet kit: "gothic-wall" (a panelled run: tracery windows on the deck side, tally
  // marks on the void side), "gothic-niche" (a hooded guardian in its niche / a drain hole), "gothic-pier" (the wall with a pilaster
  // and its pinnacle at the hex's centre); the Inn's kit (2026-10-22, "inn-*": a lime-plastered timber-framed wall with its sill beam,
  // wall plate, studs and mid rail; "inn-window" a casement window, two leaves of leaded glass standing open into the room, the light
  // through the opening; "inn-door" a one-hex plank door that swings into the room (animated like the watchtower's); "inn-doorway"
  // the same frame open; "inn-sconce" the wall with an iron bracket and a candle lantern on the room's side, its own light). Their
  // masonry repeats every textureRepeat units on every face (square, world-aligned).
  std::string emberStyle;
  double textureRepeat = 3.6;
};

// EmberEngine's pieces: the axis of a straight piece (a window, a slit, a torch bracket) in Ember's frame (x along the board,
// y = -z of the board), radians: along its wall neighbours when it has any (the first connection, turned into [0, pi)), else the
// editor's quarter turn; the half turn (rotation 2 / 3) adds pi and so flips the piece's inner side, which is local -y before the
// turn: on the board (sin a, cos a). MapStage turns windows so that side faces the room.
double emberAxisAngle(int rotation, const std::vector<std::pair<double, double>>& connections);
// The stairs ("stairs-up" / "stairs-down"): their torch's flame in the piece's space (Y up, like pieceGeometry), and the down
// stairs' well as the rectangle of ground cut away (x0, z0, x1, z1 relative to the piece's anchor, board axes).
std::optional<std::array<double, 3>> emberStairFlame(const ArchitectureDef& def, int rotation);
// The direction of a span piece's local +x (the stairs' climb) on the board after its quarter turns (piece space: x, 0, z).
std::array<double, 3> emberDirPiece(int rotation);
std::optional<std::array<double, 4>> emberStairHole(const ArchitectureDef& def, int rotation);
// Their extra materials, tagged in the UVs (u = 100 * tag + 50 + the part's own u): 0 the masonry texture, 1 oak, 2 iron, 3 a flame
// (emissive), 4 dark oak, 5 a dark void (the stairs' openings), 6 the steps' stone, 7 the gothic parapet's atlas (the carved
// panels, the guardians, the pilasters' faces: UVs authored by the builder, like the flame's); the Inn's kit (2026-10-22): 8 leaded
// glass (the casements' panes, the lantern's), 9 the Inn's timber (sill beams, wall plates, studs, rails, the frames: UVs authored
// along each beam's grain), 10 the Inn's door planks (UVs authored across and up the leaf). MapStage splits them into parts and
// takes the tag off again (emberMaterialU).
inline constexpr double kEmberUvTag = 100.0;
inline constexpr int kEmberMaterialTags = 12;  // (11: the Inn's stone footing, 2026-10-25)
inline int emberMaterialTag(double u) { return u >= kEmberUvTag ? static_cast<int>(u / kEmberUvTag) : 0; }
inline double emberMaterialU(double u) {
  const int tag = emberMaterialTag(u);
  return tag ? u - kEmberUvTag * tag - 50.0 : u;
}
// The tag's material name in ArchitectureDefs::materials ("" = always flat).
inline const char* emberMaterialName(int tag) {
  static const char* kNames[kEmberMaterialTags] = {"", "oak", "iron", "fire", "darkOak", "", "steps", "gothic", "glass", "innTimber", "innDoor", "innStone"};
  return tag >= 0 && tag < kEmberMaterialTags ? kNames[tag] : "";
}
// The animated door ("door" style): one leaf (side -1 left, +1 right), closed, in the piece's own space like pieceGeometry
// (Y up, turned by the piece's quarter turns), and its hinge there; MapStage turns the leaf about the hinge (world Y, side x
// angle: inwards) as the door opens. The "gate" style bakes its leaves swung open into the piece. The Inn's door ("inn-door") is
// two hexes wide with two leaves, like the watchtower's.
inline bool emberAnimatedDoor(const std::string& style) { return style == "door" || style == "inn-door"; }
// The pieces whose inner side MapStage turns to the room (a window's splay, a torch, a lantern, the parapets' guardians' side).
inline bool emberFacesRoom(const std::string& style) {
  return style == "window" || style == "slit" || style == "wall-torch" || style == "inn-window" || style == "inn-sconce" || style.starts_with("gothic-");
}
three::Geometry emberDoorLeafPiece(const ArchitectureDef& def, int rotation, int side);
std::array<double, 3> emberDoorHingePiece(const ArchitectureDef& def, int rotation, int side);
// The Inn's sconce ("inn-sconce"): its lantern's flame as (distance out from the wall's centre plane on the room's side, height).
std::array<double, 2> innSconceLight(const ArchitectureDef& def);
inline constexpr double kEmberDoorOpenRadians = 1.75;  // ~100 degrees: the leaves lie back against the inner wall

struct ArchitectureDefs {
  std::map<std::string, ArchitectureDef> defs;
  // EmberEngine's kit materials by name ("oak", "iron", "darkOak", "steps", "fire": the tagged parts, emberMaterialTag) -> texture URL
  // (content/models/architecture.json "materials", "editor"."architectureMaterials" in the catalog); absent = the flat colour.
  std::map<std::string, std::string> materials;
  bool load(const std::filesystem::path& catalogFile);
  const ArchitectureDef* find(const std::string& id) const;
};

// ThreeWalls.ts createWallGeometry: Z up (Ember's own axes), x along the board, y = -z of the board.
three::Geometry wallGeometry(const ArchitectureDef& def, double tile, int rotation, const std::vector<std::pair<double, double>>& connections,
                             std::pair<double, double> origin, bool projectHeight, double textureHeightScale);

// A piece's wall texture as a cooked asset (Cooker architecture): "/game/textures/walls/crypt-v2.png" ->
// "architecture/textures/walls/crypt-v2.tex" in build/meshes/architecture.pak. EmberEngine's own textures are
// "/ember/<path under content/>" (content/textures/walls/watchtower-v01.jpg -> "architecture/ember/textures/walls/watchtower-v01.tex").
inline std::string architectureTextureAsset(const std::string& wallTexture) {
  std::string path = wallTexture.starts_with("/game/") ? wallTexture.substr(6) : wallTexture.starts_with("/") ? wallTexture.substr(1) : wallTexture;
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

}  // namespace ember
