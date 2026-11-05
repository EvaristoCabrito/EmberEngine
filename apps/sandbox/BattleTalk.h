#pragma once

// A campaign battle's conversations and films, copied from Ember (GameApp BattleScreen introDialogOpen, the outro
// effect on hud.result, the "epilogue" screen): the mission's intro conversation as the battle opens, its outro the
// moment the fight is won, then on Templo and Portão the epilogue film, all over the battle (which waits: holdTurns).
// Each conversation is recorded in the save's dialogsSeen the moment it opens (save.ts dialogSeenKey) and never opens
// by itself again in that campaign; a reply's affinity change goes into the save (BattleEngine.adjustHeroAffinity +
// affinity.ts changeAffinity). Modo teste records nothing, so its conversations replay every time.

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

#include "CutsceneScreen.h"
#include "DialogScreen.h"
#include "MapStage.h"
#include "world/Campaign.h"
#include "world/Overworld.h"

namespace ember {

inline ember::Json loadDialogs(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return ember::Json();
  ember::Json j = ember::Json::parse(in, nullptr, false);
  return j.is_discarded() ? ember::Json() : j;
}

// affinity.ts AFFINITY_HEROES.
inline bool affinityHero(const std::string& name) {
  static const char* kHeroes[] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};
  return std::any_of(std::begin(kHeroes), std::end(kHeroes), [&](const char* h) { return name == h; });
}

// BattleEngine.adjustHeroAffinity(from, to, delta, dialogue = true) on a save: the party leader's positive changes
// catch up faster, then changeAffinity (sorted pair key, one decimal, -100..100). JS Math.round = floor(x + 0.5).
// Returns the combat log's line for it ("Afinidade Kael + Voss: +2,5"), empty when nothing changed.
inline std::string applyReplyAffinity(ember::Json& save, const ember::Overworld& world, const ember::Json& affinity) {
  if (!affinity.is_object()) return {};
  const std::string a = affinity.value("from", ""), b = affinity.value("to", "");
  double delta = affinity.value("delta", 0.0);
  if (a == b || delta == 0 || !std::isfinite(delta) || !affinityHero(a) || !affinityHero(b)) return {};
  const std::string pair = a < b ? a + "|" + b : b + "|" + a;
  ember::Json& scores = save["affinityScores"];
  if (!scores.is_object()) scores = ember::Json::object();
  const double score = scores.contains(pair) && scores[pair].is_number() ? scores[pair].get<double>() : 0.0;
  // partyLeaderOf: the saved leader while recruited, else Kael.
  std::string leader = "Kael";
  if (const auto l = save.find("partyLeader"); l != save.end() && l->is_string() && affinityHero(l->get<std::string>()) &&
                                               world.heroRecruited(l->get<std::string>(), save))
    leader = l->get<std::string>();
  if (delta > 0 && (a == leader || b == leader)) delta = std::floor(delta * (2 - std::max(0.0, score) / 100) * 100 + 0.5) / 100;
  scores[pair] = std::clamp(std::floor((score + delta) * 10 + 0.5) / 10, -100.0, 100.0);  // negative affinity exists
  // delta.toLocaleString("pt-BR"): up to 3 decimals, a comma
  std::string d = std::format("{:.3f}", delta);
  while (d.back() == '0') d.pop_back();
  if (d.back() == '.') d.pop_back();
  if (const size_t dot = d.find('.'); dot != std::string::npos) d[dot] = ',';
  if (uiEnglish()) {
    std::replace(d.begin(), d.end(), ',', '.');
    return std::format("Affinity {} + {}: {}{}", a, b, delta > 0 ? "+" : "", d);
  }
  return std::format("Afinidade {} + {}: {}{}", a, b, delta > 0 ? "+" : "", d);
}

struct BattleTalk {
  std::string mission;
  ember::Json entry;  // dialogs.json's entry for the mission
  std::filesystem::path artRoot;
  ember::Campaign* record = nullptr;  // the campaign to write into; null in Modo teste
  const ember::Overworld* world = nullptr;
  bool introDone = false, outroDone = false, epilogueDone = false;
  std::optional<DialogScreen> dialog;
  std::optional<CutsceneScreen> film;

  // A conversation opens unless this campaign already had it; it is recorded the moment it opens (markDialogSeen).
  void open(MapStage& map, const char* kind, const char* enabledKey) {
    const ember::Json tree = entry.value(kind, ember::Json());
    if (!tree.is_object() || !entry.value(enabledKey, true)) return;
    const std::string key = std::string(kind) + ":" + mission + ":" + tree.value("id", "");
    if (record) {
      ember::Json save = record->activeSave();
      ember::Json& seen = save["dialogsSeen"];
      if (!seen.is_array()) seen = ember::Json::array();
      if (std::find(seen.begin(), seen.end(), key) != seen.end()) return;
      seen.push_back(key);
      record->saveActive(std::move(save));
    }
    // each line's portrait = the one portrait of its speaker (Portraits.h), never the copy baked into dialogs.json
    dialog.emplace(tree, npcPortraits(tree, map.characters()), artRoot);
  }

  void draw(MapStage& map) {
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    if (!introDone) {
      introDone = true;
      open(map, "intro", "introEnabled");
    }
    const bool won = map.battleResult() == "victory";
    if (won && !outroDone) {
      outroDone = true;
      open(map, "outro", "outroEnabled");
    }
    if (dialog) {
      const bool closed = dialog->draw(size, [this, &map](const ember::Json& reply) {
        if (!reply.contains("affinity")) return;
        ember::Json save = record ? record->activeSave() : ember::Json::object();  // Modo teste records nothing, but the log tells
        const std::string line = applyReplyAffinity(save, *world, reply["affinity"]);
        if (!line.empty()) map.battleLog(line);
        if (record) record->saveActive(std::move(save));
      });
      if (closed) dialog.reset();
    } else if (won && !epilogueDone) {
      epilogueDone = true;
      if (mission == "templo" || mission == "portao")
        film.emplace(artRoot, mission == "portao" ? "game/portao-end.mp4" : "game/temple-aftermath.mp4");
    }
    if (film) {
      ImGui::SetNextWindowPos({0, 0});
      ImGui::SetNextWindowSize(size);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
      ImGui::Begin("##battlefilm", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
      if (film->draw(size)) film.reset();
      ImGui::End();
      ImGui::PopStyleVar();
    }
    map.holdTurns = dialog.has_value() || film.has_value();
    map.musicHeld = film.has_value();  // GameApp: the epilogue film is a music-free screen
  }
};

// Hooks the conversations onto a campaign battle (Stage::overlay, drawn after the battle's own panels).
inline std::shared_ptr<BattleTalk> attachBattleTalk(MapStage& map, const std::string& mission, const ember::Json& dialogs, const std::filesystem::path& artRoot,
                             const ember::Overworld& world, ember::Campaign* record) {
  const auto found = dialogs.find(mission);
  if (found == dialogs.end() && mission != "templo" && mission != "portao") return nullptr;
  auto talk = std::make_shared<BattleTalk>();
  talk->mission = mission;
  talk->entry = found != dialogs.end() ? *found : ember::Json::object();
  talk->artRoot = artRoot;
  talk->record = record;
  talk->world = &world;
  MapStage* stage = &map;
  map.overlay = [talk, stage] { talk->draw(*stage); };
  return talk;
}

}  // namespace ember
