#pragma once

// The people on a map who talk, copied from Engine2 (engine.ts handleCell's dialog branch, GameApp's DialogOverlay on
// hud.pendingDialog and its onDialogAction): a click on an NPC with a conversation (in free roam the leader walks up to it
// first) opens it over the map; a reply's affinity goes into the save; a reply's action runs once the conversation closes:
// Brue's "tavern" opens the Inn's tavern (the Adega), Vargan's "smith" the forge, the old healer's "healer" his service
// (InnScreen.h, over the map: leaving it goes back to the Inn's room where the leader stood), the suspicious patron's
// "acceptSuspectHostageQuest" and Aldric's "recruitAldric" write the campaign (never in Modo teste or a playtest). Each line's
// portrait follows DialogOverlay's rule (content/engine2/portraits.json, export_portraits.ts). Conversations replay in full
// every time (Engine2 keeps no "already talked to" state for them).

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
#include "world/Campaign.h"
#include "world/Inn.h"
#include "world/Overworld.h"

namespace ember {

// DialogOverlay.tsx speakerKey: accents off, lower case, trimmed.
inline std::string speakerKey(const std::string& name) {
  std::string out;
  for (size_t i = 0; i < name.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(name[i]);
    if (c == 0xC3 && i + 1 < name.size()) {  // the accented Latin letters (UTF-8 C3 80-BF) to their plain letter
      static const char kPlain[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
      const unsigned char d = static_cast<unsigned char>(name[++i]);
      if (d >= 0x80 && d <= 0xBF) out += static_cast<char>(std::tolower(static_cast<unsigned char>(kPlain[d - 0x80])));
      continue;
    }
    out += static_cast<char>(std::tolower(c));
  }
  const size_t a = out.find_first_not_of(" \t"), b = out.find_last_not_of(" \t");
  return a == std::string::npos ? std::string() : out.substr(a, b - a + 1);
}

// export_dialogs.ts portraitOf over portraits.json: each line's {src, framed} by "<treeId>:<lineId>" (DialogScreen's input).
inline e2::Json npcPortraits(const e2::Json& tree, const std::vector<std::pair<std::string, std::string>>& characters) {
  static const e2::Json table = [] {
    std::ifstream in("content/engine2/portraits.json", std::ios::binary);
    e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
    return j.is_object() ? j : e2::Json::object();
  }();
  const e2::Json speakers = table.value("speakers", e2::Json::object()), sprites = table.value("sprites", e2::Json::object());
  auto portraitFor = [&](const std::string& sprite) -> e2::Json {
    if (sprites.contains(sprite)) return sprites[sprite];
    return {{"src", "/game/sprites/" + sprite + "/1.png"}, {"framed", false}};  // assets.ts portraitFor's fallback
  };
  e2::Json out = e2::Json::object();
  const e2::Json lines = tree.value("lines", e2::Json::array());
  for (const e2::Json& line : lines) {
    const std::string speaker = speakerKey(line.value("speaker", ""));
    const std::string key = tree.value("id", "") + ":" + line.value("id", "");
    if (speaker == "vargan") {
      out[key] = table.value("vargan", e2::Json());
      continue;
    }
    std::string sprite = speakers.value(speaker, "");
    if (sprite.empty()) sprite = line.value("portrait", "");
    if (sprite.empty())
      for (const auto& [name, s] : characters)
        if (speakerKey(name) == speaker && !s.empty()) {
          sprite = s;
          break;
        }
    if (sprite.empty())
      for (const auto& [name, s] : characters) {
        const std::string k = speakerKey(name);
        if (!s.empty() && k.substr(0, k.find(',')) == speaker.substr(0, speaker.find(','))) {
          sprite = s;
          break;
        }
      }
    if (sprite.empty())
      for (const e2::Json& l : lines)
        if (!l.value("portrait", "").empty() && speakerKey(l.value("speaker", "")) == speaker) {
          sprite = l.value("portrait", "");
          break;
        }
    out[key] = sprite.empty() ? e2::Json() : portraitFor(sprite);
  }
  return out;
}

struct NpcTalk {
  std::filesystem::path engine2Public;
  e2::Campaign* record = nullptr;    // the campaign to write into; null in Modo teste and in a playtest
  e2::Json* testSave = nullptr;      // Modo teste's own save (GameApp testOverworld), when there is one
  const e2::Overworld* world = nullptr;
  e2::InnData innData;
  bool hasInn = false;
  std::optional<DialogScreen> dialog;
  std::string action;                // the chosen reply's action, run once the conversation closes
  std::optional<InnScreen> inn;
  e2::Json save;                     // the campaign's active save while the Inn's menus are open (scratch when there is none)
  std::function<void()> previous;    // the overlay that was there before (BattleTalk)
  bool previewing = false;           // captures: `action` runs on the first frame (preview)

  // Captures (--map-ui npc:tavern / npc:smith / npc:healer): that reply's action as if the conversation had just closed.
  void preview(const std::string& a) { action = a, previewing = true; }

  e2::Json& saveRef() { return testSave ? *testSave : save; }
  void write() {
    if (testSave || !record) return;  // Modo teste's save is written in place; a playtest writes nothing
    record->saveActive(save);
  }

  void runAction(MapStage& map) {
    const std::string a = std::exchange(action, {});
    if (a == "tavern" || a == "smith" || a == "healer") {
      if (!hasInn) return map.battleLog(tr("Sem dados da Estalagem (content/engine2/inn.json).", "No Inn data (content/engine2/inn.json)."));
      if (record && !testSave) save = record->activeSave();
      inn.emplace(a == "smith" ? InnScreen::Entry::Smith : a == "healer" ? InnScreen::Entry::Healer : InnScreen::Entry::Tavern);
      return;
    }
    if (!record || testSave) return;  // GameApp: these two write nothing in Modo teste or a playtest
    e2::Json s = record->activeSave();
    auto addTo = [&](const char* key, const std::string& v) {
      e2::Json& list = s[key];
      if (!list.is_array()) list = e2::Json::array();
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

  void draw(MapStage& map) {
    if (previous) previous();
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    if (std::exchange(previewing, false)) runAction(map);
    if (!dialog && !inn)
      if (std::optional<MapStage::Talk> t = map.takeTalk()) {
        dialog.emplace(t->tree, npcPortraits(t->tree, map.characters()), engine2Public);
        action.clear();
      }
    if (dialog) {
      const bool closed = dialog->draw(size, [this, &map](const e2::Json& reply) {
        if (reply.contains("action") && reply["action"].is_string()) action = reply["action"].get<std::string>();
        if (!reply.contains("affinity") || !world) return;
        e2::Json s = record && !testSave ? record->activeSave() : e2::Json::object();
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
      const e2::InnRules rules(innData, *world);
      InnScreen::Context context{saveRef(), [this] { write(); }, rules, innData, *world, engine2Public};
      if (inn->draw(size, context)) inn.reset();
      ImGui::End();
      ImGui::PopStyleVar(2);
    }
    const bool covering = dialog.has_value() || inn.has_value();
    map.inputHeld = covering;
    map.hudHidden = inn.has_value();
    // Turns and music held while this covers the map, let go when it stops (an overlay before this one sets them afresh
    // every frame; a battle script's own hold is never touched).
    if (covering) map.holdTurns = true, heldTurns = true;
    else if (std::exchange(heldTurns, false) && !previous) map.holdTurns = false;
    const bool film = inn && inn->filmPlaying();  // Vargan's film carries its own sound
    if (film) map.musicHeld = true, heldMusic = true;
    else if (std::exchange(heldMusic, false) && !previous) map.musicHeld = false;
  }
  bool heldTurns = false, heldMusic = false;
};

// Hooks the NPC conversations (and, through them, the Inn's menus) onto a map, after any overlay already there.
// `scratch`: the save the Inn's menus work on when there is neither a campaign to write nor a test save (a new save's).
inline std::shared_ptr<NpcTalk> attachNpcTalk(MapStage& map, const std::filesystem::path& engine2Public, const e2::Overworld* world, e2::Campaign* record,
                                              e2::Json* testSave, e2::Json scratch = e2::Json::object()) {
  auto talk = std::make_shared<NpcTalk>();
  talk->save = std::move(scratch);
  talk->engine2Public = engine2Public;
  talk->record = record;
  talk->testSave = testSave;
  talk->world = world;
  talk->hasInn = talk->innData.load("content/engine2/inn.json");
  talk->previous = std::move(map.overlay);
  MapStage* stage = &map;
  map.overlay = [talk, stage] { talk->draw(*stage); };
  return talk;
}

}  // namespace ember
