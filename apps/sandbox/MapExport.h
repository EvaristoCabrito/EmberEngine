#pragma once

// A map file made playable: Engine2's own exporters run on it, read-only (tools/scripts/engine2/*.ts through Engine2's
// vite-node, measure_art.py), as every campaign map was imported. Used by the Map Editor: Salvar prepares the saved map
// for the campaign and for Modo teste; Testar prepares the draft as Engine2's playtest fights it.
//
//   Campaign   export_scene.ts + measure_art.py -> <scenes>/<stem>.scene.json, export_battle.ts -> <battles>/<stem>.battle.json
//   Test       make_test_map.ts (the missing heroes, level 15) -> <test>/maps, then its scene and battle (TEST_LEVEL=15)
//   Playtest   make_test_map.ts KEEP_LEVELS (the authored levels kept) and export_battle.ts LEVELS_FROM_DRAFT, in `dir`
//
// Runs blocking (a few seconds): call it off the main thread.

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "CampaignUi.h"

namespace ember {

enum class MapExportKind { Campaign, Test, Playtest };

struct MapExportResult {
  bool ok = false;
  std::filesystem::path map, scene, battle;  // what the game opens
  std::string error;
};

// `engine2`: Engine2's checkout (its public folder's parent); `log`: every step's output, appended.
inline MapExportResult exportMap(const std::filesystem::path& mapFile, const std::string& stem, MapExportKind kind, const CampaignPaths& paths,
                                 const std::filesystem::path& engine2, const std::filesystem::path& log,
                                 const std::filesystem::path& playtestDir = "build/editor/playtest") {
  MapExportResult r;
  std::error_code ec;
  // vite-node's cache, one folder per kind (a save and a Testar may run at once)
  const std::filesystem::path cache = std::filesystem::path("build/editor/cache") / std::to_string(static_cast<int>(kind));
  std::filesystem::create_directories(cache, ec);
  std::filesystem::create_directories(log.parent_path(), ec);
  const std::string vn = std::format("node \"{}/node_modules/vite-node/vite-node.mjs\" --root \"{}\"", engine2.generic_string(), cache.generic_string());
  const std::string pub = (engine2 / "public").generic_string();
  auto q = [](const std::filesystem::path& p) { return "\"" + p.generic_string() + "\""; };
  std::vector<std::string> steps;
  if (kind == MapExportKind::Campaign) {
    r.map = mapFile, r.scene = paths.scenes / (stem + ".scene.json"), r.battle = paths.battles / (stem + ".battle.json");
    std::filesystem::create_directories(paths.scenes, ec), std::filesystem::create_directories(paths.battles, ec);
    steps.push_back(std::format("{} tools/scripts/engine2/export_scene.ts -- {} {}", vn, q(r.map), q(r.scene)));
    steps.push_back(std::format("python -I tools/scripts/engine2/measure_art.py {} \"{}\"", q(r.scene), pub));
    steps.push_back(std::format("set NO_REFS=1&& {} tools/scripts/engine2/export_battle.ts -- {} {}", vn, q(r.map), q(r.battle)));
  } else {
    const std::filesystem::path base = kind == MapExportKind::Test ? paths.test : playtestDir;
    const bool test = kind == MapExportKind::Test;
    r.map = test ? base / "maps" / (stem + ".json") : base / (stem + ".json");
    r.scene = test ? base / "scenes" / (stem + ".scene.json") : base / (stem + ".scene.json");
    r.battle = test ? base / "battles" / (stem + ".battle.json") : base / (stem + ".battle.json");
    for (const auto& p : {r.map, r.scene, r.battle}) std::filesystem::create_directories(p.parent_path(), ec);
    steps.push_back(std::format("{}{} tools/scripts/engine2/make_test_map.ts -- {} {}", test ? "" : "set KEEP_LEVELS=1&& ", vn, q(mapFile), q(r.map)));
    steps.push_back(std::format("{} tools/scripts/engine2/export_scene.ts -- {} {}", vn, q(r.map), q(r.scene)));
    steps.push_back(std::format("python -I tools/scripts/engine2/measure_art.py {} \"{}\"", q(r.scene), pub));
    steps.push_back(std::format("set TEST_LEVEL=15&& {}set NO_REFS=1&& {} tools/scripts/engine2/export_battle.ts -- {} {}",
                                test ? "" : "set LEVELS_FROM_DRAFT=1&& ", vn, q(r.map), q(r.battle)));
  }
  for (const std::string& step : steps)
    // std::system runs cmd /c; the line starts unquoted, so its quotes are kept as written.
    if (std::system(std::format("{} >> {} 2>&1", step, q(log)).c_str()) != 0) {
      r.error = "veja " + log.generic_string();
      return r;
    }
  r.ok = true;
  return r;
}

}  // namespace ember
