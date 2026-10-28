#pragma once

// A map's scene built in C++ from its draft, the way tools/scripts/data/export_scene.ts + measure_art.py build it
// from Ember's own rules: decoration cards (art facing, size, light, layer), 3D models, architecture and units
// (idle pose, box, pacing), each placed on the board. Ember's tables come resolved per decoration and per unit from
// content/data/editor-catalog.json (export_editor_catalog.ts + measure_art.py --catalog), so any draft (the map
// editor's working copy) gets its scene at once, without running Ember.
//
// Cooker check-scene-builder compares the result with export_scene.ts's scene for every campaign map.

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "world/Board.h"
#include "world/SceneData.h"

namespace ember {

struct FootOffset {
  int dx = 0, dy = 0;
};

struct CatalogDecoration {
  std::string id, name;
  std::vector<FootOffset> footprint, blockingFootprint;  // blocking empty = the footprint
  bool architectureSpan = false, architecture = false, mirrorAlternate = false, standOn = false, lockYaw = false;
  std::string model3d, exitKind;
  std::optional<SceneModel> model;  // model, file, height
  float w = 1, h = 1, nudge = 0;
  std::optional<SceneLight> light;  // seed set per placement
};

struct CatalogFacing {
  std::string file, image;
  bool mirror = false, art = false;
};

struct CatalogUnit {
  std::string classId, key, sprite, set, clip;
  float boxW = 1, boxH = 1, foot = 0;
  int natW = 0, natH = 0, frames = 1;
  std::vector<int> pace;
  bool reversed = false;
  std::optional<SceneLight> light;
  std::optional<std::vector<int>> front;  // the footprint's front row (dx); absent = the anchor hex
};

class EditorCatalog {
 public:
  bool load(const std::filesystem::path& file);
  const CatalogDecoration* decoration(const std::string& id) const;
  const CatalogFacing* facing(const std::string& artId, int step) const;
  const SceneArt* measure(const std::string& image) const;
  // unitForSpawn: a hero by name on the party's side, else the class.
  const CatalogUnit* unit(const std::string& name, const std::string& classId, const std::string& side) const;
  const std::map<std::string, CatalogDecoration>& decorations() const { return decorations_; }
  const std::map<std::string, CatalogUnit>& classes() const { return classes_; }
  // mapBackgrounds.ts resolvedMapBackground: the backdrop file for a draft ("" = none).
  std::string backdrop(const MapDraft& draft) const;

 private:
  std::map<std::string, CatalogDecoration> decorations_;
  std::map<std::string, std::vector<CatalogFacing>> facings_;
  std::map<std::string, SceneArt> measures_;
  std::map<std::string, CatalogUnit> heroes_, classes_;
  std::map<std::string, std::string> backgroundByMap_, backgroundById_;
};

// data.ts placedFootprint: a placement's footprint cells as offsets from its anchor (rot sixths of a turn in cube space
// about the anchor; architecture spans turn in quarter steps).
std::vector<FootOffset> placedFootprint(const CatalogDecoration& def, int x, int y, double rot);
// data.ts decorationPlacementArt: the art a placement shows (floor connectors and stairs by their direction).
std::string placementArt(const MapDecoration& p);

// The scene of `board`'s draft. `skipped` (optional) counts what was left out and why, as export_scene.ts does.
SceneData buildScene(const Board& board, const EditorCatalog& catalog, std::map<std::string, int>* skipped = nullptr);

// Cooker check-scene-builder: the C++ scene of each map against export_scene.ts's (scene file per map, same stem).
// Returns the number of differences.
int checkSceneBuilder(const BoardData& data, const EditorCatalog& catalog, const std::vector<std::filesystem::path>& maps,
                      const std::filesystem::path& scenes, const std::filesystem::path& artRoot);

}  // namespace ember
