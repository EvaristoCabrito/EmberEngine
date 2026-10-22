#pragma once

// The game's own map preparation, in C++: what tools/scripts/engine2/{make_test_map,export_scene,export_battle}.ts +
// measure_art.py did through node, now done by the game itself when a mission opens (apps/sandbox/MapExport.h).
//
//   prepareCampaignMap  make_test_map.ts: the campaign's six heroes added to a battle map as entry positions (free-roam
//                       maps unchanged), legacy class ids repaired; the whole map file written again.
//   exportBattle        export_battle.ts: the battle file of a prepared map, every field the game reads (cells as the
//                       movement rules see them, the units with their stats, spells, charges, gear and sound cues, the
//                       exits, the locks and what opening each one changes, the mission's music). The reference answers
//                       Cooker check-battle compares against (forecasts, rolls, casts ...) are the TypeScript exporter's
//                       alone and are not written here.
//   sceneJson           export_scene.ts + measure_art.py: a scene built by SceneBuilder, written as the scene file
//                       SceneData::load reads.
//
// The rule tables (classes, terrain, decorations, weapons, sound cues ...) come from content/engine2/battle-rules.json,
// written once by tools/scripts/engine2/export_battle_rules.ts from the rule source; nothing here runs node.
//
// Cooker check-battle-export compares exportBattle's file with the TypeScript exporter's for the proof maps.

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "world/SceneData.h"

namespace ember::e2 {

struct MapDraft;

// make_test_map.ts on `mapFile`; the prepared map file at `outFile`. `note` gets the one-line summary the script printed.
bool prepareCampaignMap(const std::filesystem::path& mapFile, const std::filesystem::path& outFile, std::string* note);

// export_battle.ts on a map file's JSON ({serial, savedAt, draft} or a bare draft). Empty with `error` set when a spawn
// names a class the rules do not know, or the rules file cannot be read. `playerLevel`: the fresh party's level (1 in play;
// the test battles in build/engine2-test were made at 15).
std::optional<nlohmann::json> exportBattle(const nlohmann::json& mapFile, std::string* error, int playerLevel = 1);

// The scene file of a built scene (export_scene.ts's shape, with measure_art.py's art already in it).
nlohmann::json sceneJson(const SceneData& scene, const MapDraft& draft, const std::map<std::string, int>& skipped);

// Cooker check-battle-export: exportBattle on <maps>/<stem>.json against each reference battle file (its stem without
// ".battle"), the fields the game reads. Returns the number of files that differ.
int checkBattleExport(const std::filesystem::path& maps, const std::vector<std::filesystem::path>& references, int playerLevel = 1);

}  // namespace ember::e2
