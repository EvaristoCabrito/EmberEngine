#pragma once

// Campaign and Modo Teste share one map, scene and battle preparation pipeline, all in C++ (world/BattleExport.h): the
// campaign's party added to the map (make_test_map), the scene built by SceneBuilder (export_scene + measure_art), the
// battle file written (export_battle). Nothing here runs node. Takes well under a second; still called off the main
// thread by the window (the loading screen).

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "CampaignUi.h"
#include "core/Log.h"
#include "world/BattleExport.h"
#include "world/Board.h"
#include "world/SceneBuilder.h"

namespace ember {

struct MapExportResult {
  bool ok = false;
  std::filesystem::path map, scene, battle;  // what the game opens
  std::string error;
};

// Where a preparation writes its three files and the party's level in its battle (1 in play; the editor's test battles
// were made at 15).
struct MapExportTarget {
  std::filesystem::path map, scene, battle;
  int playerLevel = 1;
};

// The pipeline itself. `enginePublic`: the game's asset folder (the board's relief grids are read from it); `log` receives
// a line per step, appended.
inline MapExportResult exportMapTo(const std::filesystem::path& mapFile, const std::string& stem, const MapExportTarget& target,
                                   const std::filesystem::path& enginePublic, const std::filesystem::path& log) {
  MapExportResult r;
  std::error_code ec;
  std::filesystem::create_directories(log.parent_path(), ec);
  std::ofstream out(log, std::ios::binary | std::ios::app);
  r.map = target.map, r.scene = target.scene, r.battle = target.battle;
  for (const auto& p : {r.map, r.scene, r.battle}) std::filesystem::create_directories(p.parent_path(), ec);
  const auto fail = [&](const std::string& why) {
    out << "ERRO: " << why << "\n";
    r.error = why + " (veja " + log.generic_string() + ")";
    return r;
  };
  // 1. the map with the campaign's party on it
  std::string note;
  if (!ember::prepareCampaignMap(mapFile, r.map, &note)) return fail("o mapa " + mapFile.generic_string() + " não pôde ser preparado");
  out << note << "\n";
  // 2. its scene
  const std::optional<ember::BoardData> data = ember::BoardData::load("content/data/board.json");
  ember::EditorCatalog catalog;
  if (!data || !catalog.load("content/data/editor-catalog.json")) return fail("content/data/board.json ou editor-catalog.json não puderam ser lidos");
  std::optional<ember::MapDraft> draft = ember::MapDraft::load(r.map);
  if (!draft) return fail("o mapa preparado " + r.map.generic_string() + " não tem tabuleiro");
  const ember::MapDraft draftCopy = *draft;
  ember::Board board(*data, std::move(*draft));
  if (!board.loadRelief(enginePublic)) out << "aviso: relevo de " + stem + " não lido de " + enginePublic.generic_string() + "\n";
  std::map<std::string, int> skipped;
  const ember::SceneData scene = ember::buildScene(board, catalog, &skipped);
  {
    std::ofstream file(r.scene, std::ios::binary);
    if (!file) return fail("não foi possível escrever " + r.scene.generic_string());
    file << ember::sceneJson(scene, draftCopy, skipped).dump(1);
    out << stem << ": " << scene.cards.size() << " cards, " << scene.models.size() << " models, " << scene.architecture << " architecture, " << scene.units.size() << " units; skipped " << nlohmann::json(skipped).dump() << "\n";
  }
  // 3. its battle
  {
    std::ifstream in(r.map, std::ios::binary);
    nlohmann::json mapJson = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    if (!in || mapJson.is_discarded()) return fail("não foi possível reler " + r.map.generic_string());
    std::string error;
    const std::optional<nlohmann::json> battle = ember::exportBattle(mapJson, &error, target.playerLevel);
    if (!battle) return fail("as regras da batalha não puderam ser feitas: " + error);
    std::ofstream file(r.battle, std::ios::binary);
    if (!file) return fail("não foi possível escrever " + r.battle.generic_string());
    file << battle->dump();
    out << stem << ": " << (*battle)["cols"].get<int>() << "x" << (*battle)["rows"].get<int>() << ", " << (*battle)["units"].size() << " units\n";
  }
  r.ok = true;
  return r;
}

// A campaign mission's files in the campaign folder (CampaignPaths::campaign). `engineRoot` is no longer needed (kept so every
// caller stays as it is).
inline MapExportResult exportMap(const std::filesystem::path& mapFile, const std::string& stem, const CampaignPaths& paths,
                                 [[maybe_unused]] const std::filesystem::path& engineRoot, const std::filesystem::path& log) {
  MapExportTarget t;
  t.map = paths.campaign / "maps" / (stem + ".json");
  t.scene = paths.campaign / "scenes" / (stem + ".scene.json");
  t.battle = paths.campaign / "battles" / (stem + ".battle.json");
  return exportMapTo(mapFile, stem, t, paths.artRoot, log);
}

// The three kinds the editor's Testar and the campaign used to name: Campaign (above), Test (build/test-maps, the party at
// 15) and Playtest (a flat folder, the party at 15).
enum class MapExportKind { Campaign, Test, Playtest };
inline MapExportResult exportMap(const std::filesystem::path& mapFile, const std::string& stem, MapExportKind kind, const CampaignPaths& paths,
                                 const std::filesystem::path& engineRoot, const std::filesystem::path& log,
                                 const std::filesystem::path& playtestDir = "build/editor/playtest") {
  if (kind == MapExportKind::Campaign) return exportMap(mapFile, stem, paths, engineRoot, log);
  MapExportTarget t;
  const bool test = kind == MapExportKind::Test;
  const std::filesystem::path base = test ? std::filesystem::path("build/test-maps") : playtestDir;
  t.map = test ? base / "maps" / (stem + ".json") : base / (stem + ".json");
  t.scene = test ? base / "scenes" / (stem + ".scene.json") : base / (stem + ".scene.json");
  t.battle = test ? base / "battles" / (stem + ".battle.json") : base / (stem + ".battle.json");
  t.playerLevel = 15;
  return exportMapTo(mapFile, stem, t, paths.artRoot, log);
}

}  // namespace ember
