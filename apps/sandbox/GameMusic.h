#pragma once

// Ember's music per screen (src/game/audio.ts getTrack / menuElement and GameApp's music chain), as Stage::Music:
// the title page's own <audio id="menu-music"> (index.html: the Ashera intro at base 1), the world map's piece, the
// Inn's, and a mission's track for its briefing and battle (the battle file's `music`, export_battle.ts: the
// mission's own file when it names one, else the theme for its id). Screens without music (films, test menus, the map
// editor) return none.

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "CampaignUi.h"
#include "Stage.h"

namespace ember::music {

// audio.ts getTrack: each theme's file under game/MUSIC and its base volume.
inline Stage::Music theme(const std::string& name) {
  if (name == "intro") return {"LANDR-AsheraIntrol-Balanced-High.mp3", 0.7f};
  if (name == "title") return {"LANDR-AsheraIntrol-Balanced-High.mp3", 1.0f};  // index.html menu-music (base 1)
  if (name == "temple") return {"temple.mp3", 0.42f};
  if (name == "aldeia") return {"EMBERBattle01-Balanced-High.mp3", 0.4f};
  if (name == "siege") return {"Absolute Mental Descruction 00-Balanced-High.mp3", 0.4f};
  if (name == "inn") return {"Connard The Barbarian 10.mp3", 0.4f};
  if (name == "hill") return {"Long Space _Opera DJ  UMN+  022-Balanced-High.mp3", 0.4f};
  if (name == "portao") return {"TempleEntrance-Balanced-High.mp3", 0.4f};
  if (name == "worldMap") return {"Tragic Arqui TruePersonaWorldMapEmber-Balanced-High.mp3", 0.4f};
  if (name == "battle") return {"music.mp3", 0.4f};
  if (name == "cemetery") return {"Ember Ashes Ambient OST 0-Balanced-High.mp3", 0.4f};  // EmberEngine's own (content/music)
  if (name == "ashenForest") return {"New Claude Melisma Free 04 - AshenForest-Balanced-High.mp3", 0.4f};  // EmberEngine's own (content/music)
  if (name == "watchtower") return {"OldTowwerDrone  trax 4 Persona 884-Balanced-High.mp3", 0.4f};  // EmberEngine's own (content/music)
  return {"LANDR-Absolute Mental Descruction umn3 10 (3)-Balanced-High.mp3", 0.4f};  // "early"
}

// Every cemetery map (the user, 2026-10-18): its id starts with "cemiterio" (cemiterio-esquecidos, -cripta, -mausoleu, ...).
inline bool isCemetery(const std::string& id) { return id.starts_with("cemiterio"); }
// Every Ashen Forest map (the user, 2026-10-22: "wire it for the Ashen forest Maps"): Ashen Forest Crossing and the Deep Ashen
// region (deep-ashen-forest, -ravine, -root-sanctuary, ...).
inline bool isAshenForest(const std::string& id) { return id.starts_with("ashen-forest") || id.starts_with("deep-ashen"); }
inline bool isWatchtower(const std::string& id) { return id.starts_with("watchtower-"); }

// GameApp's chain for a mission id when its battle file carries no `music` (older exports).
inline Stage::Music forMissionId(const std::string& id) {
  if (isCemetery(id)) return theme("cemetery");
  if (isAshenForest(id)) return theme("ashenForest");
  if (isWatchtower(id)) return theme("watchtower");
  if (id == "templo") return theme("temple");
  if (id == "aldeia") return theme("aldeia");
  if (id == "estalagem") return theme("inn");
  if (id == "muralha" || id == "fortaleza") return theme("siege");
  if (id == "colina" || id == "passagem") return theme("hill");
  if (id == "portao") return theme("portao");
  return theme("early");
}

// A battle file's track: its `music` (export_battle.ts); a `theme` there means the map named none, so a cemetery map
// gets the cemetery's (battle files exported before that theme existed carry "early"). A track the map names is kept.
inline Stage::Music fromBattle(const nlohmann::json& j, Stage::Music fallback) {
  if (!j.is_object()) return fallback;
  const auto music = j.find("music");
  if (music == j.end() || !music->is_object() || music->value("file", std::string()).empty()) return fallback;
  if (music->contains("theme") && isCemetery(j.value("map", std::string()))) return theme("cemetery");
  if (music->contains("theme") && isAshenForest(j.value("map", std::string()))) return theme("ashenForest");
  return {music->value("file", std::string()), music->value("base", 0.4f)};
}

// A mission's track (its briefing keeps it, as GameApp's inMission): the battle file's `music`, read once per file.
inline Stage::Music forMission(const MissionEntry& m) {
  static std::map<std::filesystem::path, Stage::Music> known;
  if (!m.battle.empty()) {
    if (const auto it = known.find(m.battle); it != known.end()) return it->second;
    std::ifstream in(m.battle, std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    return known[m.battle] = fromBattle(j, forMissionId(m.draftId));
  }
  return forMissionId(m.draftId);
}

}  // namespace ember::music
