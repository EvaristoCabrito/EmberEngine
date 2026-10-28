#pragma once

// The people on a map who talk, copied from Ember (engine.ts handleCell's dialog branch, GameApp's DialogOverlay on
// hud.pendingDialog and its onDialogAction): a click on an NPC with a conversation (in free roam the leader walks up to it
// first) opens it over the map; a reply's affinity goes into the save; a reply's action runs once the conversation closes:
// Brue's "tavern" opens the Inn's tavern (the Adega), Vargan's "smith" the forge, the old healer's "healer" his service
// (InnScreen.h, over the map: leaving it goes back to the Inn's room where the leader stood), the suspicious patron's
// "acceptSuspectHostageQuest" and Aldric's "recruitAldric" write the campaign (never in Modo teste or a playtest). Each line's
// portrait follows DialogOverlay's rule (content/data/portraits.json, export_portraits.ts). Conversations replay in full
// every time (Ember keeps no "already talked to" state for them).

#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "BattleTalk.h"
#include "DialogScreen.h"
#include "InnScreen.h"
#include "MapStage.h"
#include "Portraits.h"
#include "world/Campaign.h"
#include "world/Inn.h"
#include "world/Overworld.h"

namespace ember {

struct NpcTalk {
  std::filesystem::path artRoot;
  ember::Campaign* record = nullptr;    // the campaign to write into; null in Modo teste and in a playtest
  ember::Json* testSave = nullptr;      // Modo teste's own save (GameApp testOverworld), when there is one
  const ember::Overworld* world = nullptr;
  ember::InnData innData;
  bool hasInn = false;
  std::optional<DialogScreen> dialog;
  std::string action;                // the chosen reply's action, run once the conversation closes
  std::optional<InnScreen> inn;
  ember::Json save;                     // the campaign's active save while the Inn's menus are open (scratch when there is none)
  std::function<void()> previous;    // the overlay that was there before (BattleTalk)
  bool previewing = false;           // captures: `action` runs on the first frame (preview)
  // The Inn's pair scenes (content/inn-scenes.json; the user, 2026-10-23/24, ordered with the Inn's secret word): the party leader
  // talking to a companion seated in the Inn, both in the group, plays their film the first time; the two gain its affinity after
  // it, then the companion's conversation opens as usual.
  std::optional<CutsceneScreen> film;
  std::optional<MapStage::Talk> afterFilm;  // the conversation the scene stands before
  ember::Json scene;                           // the scene playing (its inn-scenes.json entry)

  // Captures (--map-ui npc:tavern / npc:smith / npc:healer): that reply's action as if the conversation had just closed.
  void preview(const std::string& a) { action = a, previewing = true; }

  ember::Json& saveRef() { return testSave ? *testSave : save; }
  void write() {
    if (testSave || !record) return;  // Modo teste's save is written in place; a playtest writes nothing
    record->saveActive(save);
  }

  void runAction(MapStage& map) {
    const std::string a = std::exchange(action, {});
    if (a == "tavern" || a == "smith" || a == "healer") {
      if (!hasInn) return map.battleLog(tr("Sem dados da Estalagem (content/data/inn.json).", "No Inn data (content/data/inn.json)."));
      if (record && !testSave) save = record->activeSave();
      inn.emplace(a == "smith" ? InnScreen::Entry::Smith : a == "healer" ? InnScreen::Entry::Healer : InnScreen::Entry::Tavern);
      return;
    }
    if (!record || testSave) return;  // GameApp: these two write nothing in Modo teste or a playtest
    ember::Json s = record->activeSave();
    auto addTo = [&](const char* key, const std::string& v) {
      ember::Json& list = s[key];
      if (!list.is_array()) list = ember::Json::array();
      if (std::find(list.begin(), list.end(), v) == list.end()) list.push_back(v);
    };
    if (a == "acceptSuspectHostageQuest") {
      addTo("flags", "suspeito-watchtower-intel");
      addTo("questsDiscovered", "suspeito-watchtower-captive");
      addTo("questsActive", "suspeito-watchtower-captive");
    } else if (a == "recruitAldric") {
      addTo("flags", "recruited:Aldric");
    } else {
      return;
    }
    record->saveActive(std::move(s));
  }

  // The save a story moment writes into: Modo teste's own, else the campaign's active one, else the scratch one (a playtest).
  ember::Json& storySave() {
    if (testSave) return *testSave;
    if (record) save = record->activeSave();
    return save;
  }
  static const ember::Json& innScenes() {
    static const ember::Json table = [] {
      std::ifstream in("content/inn-scenes.json", std::ios::binary);
      ember::Json j = in ? ember::Json::parse(in, nullptr, false) : ember::Json();
      return j.is_object() && j.contains("scenes") && j["scenes"].is_array() ? j["scenes"] : ember::Json::array();
    }();
    return table;
  }
  // The scene for this talk: on an Inn floor, the leader one of the pair, the one clicked the other, both in the group, not seen yet.
  ember::Json innSceneFor(const MapStage& map, const MapStage::Talk& t) {
    if (map.draftId().rfind("estalagem", 0) != 0) return {};
    const ember::Json& s = storySave();
    const auto l = s.find("partyLeader");
    const std::string leader = l != s.end() && l->is_string() ? l->get<std::string>() : "Kael";
    const std::string talked = t.name.substr(0, t.name.find(','));
    for (const ember::Json& sc : innScenes()) {
      const ember::Json pair = sc.value("pair", ember::Json::array());
      if (pair.size() != 2 || !pair[0].is_string() || !pair[1].is_string()) continue;
      const std::string a = pair[0].get<std::string>(), b = pair[1].get<std::string>();
      if (!((a == leader && b == talked) || (b == leader && a == talked))) continue;
      if (s.value(sc.value("seen", std::string("seen:") + a + "|" + b), false)) continue;
      if (world && (!world->heroRecruited(a, s) || !world->heroRecruited(b, s))) continue;  // both must be in the group
      return sc;
    }
    return {};
  }
  void finishInnScene(MapStage& map) {
    const ember::Json pair = scene.value("pair", ember::Json::array());
    const std::string a = pair[0].get<std::string>(), b = pair[1].get<std::string>();
    const double delta = scene.value("affinity", 0.0);
    ember::Json& s = storySave();
    s[scene.value("seen", std::string("seen:") + a + "|" + b)] = true;
    ember::Json& scores = s["affinityScores"];  // the pair's score (sorted key, one decimal, -100..100), exactly the scene's amount
    if (!scores.is_object()) scores = ember::Json::object();
    const std::string key = a < b ? a + "|" + b : b + "|" + a;
    const double score = scores.contains(key) && scores[key].is_number() ? scores[key].get<double>() : 0.0;
    scores[key] = std::clamp(std::floor((score + delta) * 10 + 0.5) / 10, -100.0, 100.0);
    write();
    if (delta != 0)
      map.battleLog(std::format("{} {} + {}: {}{}", tr("Afinidade", "Affinity"), a, b, delta > 0 ? "+" : "", static_cast<int>(std::lround(delta))));
  }

  void draw(MapStage& map) {
    if (previous) previous();
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    if (std::exchange(previewing, false)) runAction(map);
    if (!dialog && !inn && !film)
      if (std::optional<MapStage::Talk> t = map.takeTalk()) {
        if (ember::Json sc = innSceneFor(map, *t); sc.is_object() && sc.contains("film")) {
          film.emplace(artRoot, std::filesystem::absolute(sc["film"].get<std::string>()).string());
          scene = std::move(sc);
          afterFilm = std::move(t);
        } else {
          dialog.emplace(t->tree, npcPortraits(t->tree, map.characters()), artRoot);
          action.clear();
        }
      }
    if (film) {
      ImGui::SetNextWindowPos({0, 0});
      ImGui::SetNextWindowSize(size);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::Begin("##npcfilm", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
      const bool over = film->draw(size);
      ImGui::End();
      ImGui::PopStyleVar(2);
      if (over) {
        film.reset();
        finishInnScene(map);
        if (std::optional<MapStage::Talk> t = std::exchange(afterFilm, std::nullopt)) {
          dialog.emplace(t->tree, npcPortraits(t->tree, map.characters()), artRoot);
          action.clear();
        }
      }
    }
    if (dialog) {
      const bool closed = dialog->draw(size, [this, &map](const ember::Json& reply) {
        if (reply.contains("action") && reply["action"].is_string()) action = reply["action"].get<std::string>();
        if (!reply.contains("affinity") || !world) return;
        ember::Json s = record && !testSave ? record->activeSave() : ember::Json::object();
        const std::string line = applyReplyAffinity(s, *world, reply["affinity"]);
        if (!line.empty()) map.battleLog(line);
        if (record && !testSave) record->saveActive(std::move(s));
      });
      if (closed) {
        dialog.reset();
        runAction(map);
      }
    }
    if (inn && world) {
      ImGui::SetNextWindowPos({0, 0});
      ImGui::SetNextWindowSize(size);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::Begin("##innmenus", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
      const ember::InnRules rules(innData, *world);
      InnScreen::Context context{saveRef(), [this] { write(); }, rules, innData, *world, artRoot};
      if (inn->draw(size, context)) inn.reset();
      ImGui::End();
      ImGui::PopStyleVar(2);
    }
    const bool covering = dialog.has_value() || inn.has_value() || film.has_value();
    map.inputHeld = covering;
    map.hudHidden = inn.has_value() || film.has_value();
    // Turns and music held while this covers the map, let go when it stops (an overlay before this one sets them afresh
    // every frame; a battle script's own hold is never touched).
    if (covering) map.holdTurns = true, heldTurns = true;
    else if (std::exchange(heldTurns, false) && !previous) map.holdTurns = false;
    const bool filmOn = (inn && inn->filmPlaying()) || film.has_value();  // Vargan's film and the Kael-Neera scene carry their own sound
    if (filmOn) map.musicHeld = true, heldMusic = true;
    else if (std::exchange(heldMusic, false) && !previous) map.musicHeld = false;
  }
  bool heldTurns = false, heldMusic = false;
};

// Hooks the NPC conversations (and, through them, the Inn's menus) onto a map, after any overlay already there.
// `scratch`: the save the Inn's menus work on when there is neither a campaign to write nor a test save (a new save's).
inline std::shared_ptr<NpcTalk> attachNpcTalk(MapStage& map, const std::filesystem::path& artRoot, const ember::Overworld* world, ember::Campaign* record,
                                              ember::Json* testSave, ember::Json scratch = ember::Json::object()) {
  auto talk = std::make_shared<NpcTalk>();
  talk->save = std::move(scratch);
  talk->artRoot = artRoot;
  talk->record = record;
  talk->testSave = testSave;
  talk->world = world;
  talk->hasInn = talk->innData.load("content/data/inn.json");
  talk->previous = std::move(map.overlay);
  MapStage* stage = &map;
  map.overlay = [talk, stage] { talk->draw(*stage); };
  return talk;
}

}  // namespace ember
