#pragma once

// The Map Editor's "Gerar terreno", ported from Engine2 (src/ember/data.ts dressMap and the editor's own steps in
// src/editor/ember/MapEditorScreen.tsx): the draft as a mission with no decorations (mapstore draftToMission, with the
// watchtower floors' closing walls), the tactical scatter (barricade walls), columns turned into rocks, chests on open
// ground, the random scenery scatter, then a barricade prop on every bare barricade tile. Generate replaces: the
// draft's own decorations go, its units and tile art stay.
//
// Everything but the scenery scatter is seeded as Engine2 seeds it (the mission's index, its id); the scatter draws
// from `random` (Engine2: Math.random). Cooker check-dress compares the result with Engine2's own run on the same
// drafts with Math.random replaced by the same generator (tools/scripts/engine2/export_dress_refs.ts).

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "world/Combat.h"  // Mulberry32 (Engine2 mulberry32 / data.ts mulberry32Local)
#include "world/SceneBuilder.h"

namespace ember::e2 {

// The tables the generator reads (content/engine2/editor-catalog.json "editor").
struct DressTables {
  std::map<std::string, bool> passable;         // TERRAIN[t].passable
  std::map<std::string, int> variantCount;      // TILE_VARIANT_COUNT
  std::map<std::string, std::string> tileChar;  // TILE_CHAR
  struct Deco {
    std::string id, model3d, exitKind;
    bool chest = false, manual = false, solidRock = false, rockStyle = false;
  };
  std::vector<Deco> decorations;  // DECORATIONS in Object.keys order
  bool load(const std::filesystem::path& catalogFile);
  const Deco* deco(const std::string& id) const;
};

struct DressResult {
  std::vector<std::string> tiles;
  std::vector<int> tileVariants;
  nlohmann::ordered_json decorations;  // the placements, as the draft stores them
  std::map<std::string, int> counts;   // non-plains tiles by terrain (the editor's note)
};

// MapEditorScreen "Gerar terreno" on `draft` (Engine2's MapDraft JSON). `exclude`: decorations kept out of the scatter
// (the editor's dice).
DressResult dressDraft(const nlohmann::ordered_json& draft, const DressTables& tables, const EditorCatalog& catalog,
                       const std::set<std::string>& exclude, const std::function<double()>& random);

// Cooker check-dress: each reference case (export_dress_refs.ts) run again here. Returns the number of differences.
int checkDress(const std::filesystem::path& refs, const DressTables& tables, const EditorCatalog& catalog);

}  // namespace ember::e2
