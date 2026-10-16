#pragma once

// Engine2's music per screen (src/game/audio.ts getTrack / menuElement and GameApp's music chain), as Stage::Music:
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
  return {"LANDR-Absolute Mental Descruction umn3 10 (3)-Balanced-High.mp3", 0.4f};  // "early"
}

// GameApp's chain for a mission id when its battle file carries no `music` (older exports).
inline Stage::Music forMissionId(const std::string& id) {
  if (id == "templo") return theme("temple");
  if (id == "aldeia") return theme("aldeia");
  if (id == "estalagem") return theme("inn");
  if (id == "muralha" || id == "fortaleza") return theme("siege");
  if (id == "colina" || id == "passagem") return theme("hill");
  if (id == "portao") return theme("portao");
  return theme("early");
}

// A mission's track (its briefing keeps it, as GameApp's inMission): the battle file's `music`, read once per file.
inline Stage::Music forMission(const MissionEntry& m) {
  static std::map<std::filesystem::path, Stage::Music> known;
  if (!m.battle.empty()) {
    if (const auto it = known.find(m.battle); it != known.end()) return it->second;
    std::ifstream in(m.battle, std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    Stage::Music out = forMissionId(m.draftId);
    if (j.is_object() && j.contains("music") && j["music"].is_object() && !j["music"].value("file", std::string()).empty())
      out = {j["music"].value("file", std::string()), j["music"].value("base", 0.4f)};
    return known[m.battle] = out;
  }
  return forMissionId(m.draftId);
}

}  // namespace ember::music
