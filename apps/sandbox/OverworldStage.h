#pragma once

// The campaign's world map, copied from Engine2 (src/game/OverworldMapScreen.tsx, GameApp's overworld handlers and
// src/ember-ui/styles.css; user, 2026-10-15: "importar todos os menus corretamente"). Its rules are the C++ port in
// engine/world/Overworld (checked against Engine2 step for step).
//
// On screen: the header (Campanha · RPG / Mapa, Party, Missões, the day clock, the moon, rations, hunger, Alimentar
// todos, Opções, Gold); the painted world map (game/assets/world-map.jpg) at four zoom stops, dragged with the mouse;
// the fog over every place the party has never stood near; the time-of-day tint; the location pins (locked / open /
// done / the Inn's glow, with a mission count); the leader's idle sprite on the party's hex (click it to see the
// hexes it can reach and how long each takes); the party panel (portraits, hunger and life bars, Explorar); travel
// events; the location's mission list; the quest log; the Party menu (leader, formation, affinities, road training);
// the first-visit explainer. A mission opens its briefing, then its fight (main.cpp).
//
// Not ported yet: field spells (Create Food and Water, Warp), the inventory and status panels, the Inn, intro
// cutscenes, sound.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "Stage.h"
#include "render/Camera.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "render/UiArt.h"
#include "world/Campaign.h"
#include "world/Combat.h"
#include "world/Overworld.h"

namespace ember {

class OverworldStage : public Stage {
 public:
  struct Callbacks {
    std::function<void(const MissionEntry&)> play;  // a mission's fight
    std::function<void()> title;                    // back to the title screen
    std::function<void()> fullscreen;               // Opções: Tela cheia
  };

  OverworldStage(CampaignPaths paths, e2::Campaign& campaign, const e2::Overworld& world, Callbacks callbacks, std::string notice = {})
      : paths_(std::move(paths)), campaign_(campaign), world_(world), callbacks_(std::move(callbacks)), notice_(std::move(notice)) {
    save_ = campaign_.activeSave();
    const e2::Json& pos = save_["overworldPos"];
    prevCol_ = pos.value("col", 0);
    partyX_ = -1;  // placed on the first frame
    // GameApp start: a fight saved mid-way, else a pending mission, opens its briefing first.
    std::string id;
    if (save_.contains("battle") && save_["battle"].is_object()) id = save_["battle"].value("missionId", "");
    if (id.empty() && save_.contains("pendingMission") && save_["pendingMission"].is_string()) id = save_["pendingMission"].get<std::string>();
    if (!id.empty()) openMission(id, true);
  }

  // Captures: a panel opened as a click would ("party", "skills", "quests", "options", "move", "location:<id>", "save").
  void preview(const std::string& what) {
    if (what == "party") party_ = true;
    if (what == "skills") party_ = true, partyTab_ = 1;
    if (what == "quests") questLog_ = true;
    if (what == "options") options_ = true;
    if (what == "move") movementOpen_ = true;
    if (what.starts_with("location:")) open_ = what.substr(9);
    if (what == "save") slots_.emplace(), slots_->open(SlotScreen::Mode::Save, true);
  }
  // Captures end after a second of frames (time for Engine2's art to load).
  bool done() const override { return frames_ > 60; }

  void frame(Renderer& renderer, VkExtent2D) override {
    ++frames_;
    renderer.setScene(Camera{}, {});
    renderer.setMeshes(MeshSceneView{});
    renderer.setTerrain({});
    renderer.setWater({});
    renderer.setLights({});
    renderer.setGroundMarks({}, 0, 0);
    renderer.setFogOfWar(nullptr, 0);
    renderer.setVolumetricFog({});
    renderer.setFogBank({});
    renderer.setBackground(Renderer::kBlackBackground);
    renderer.setBloom(0.0f);
  }

  void event(const SDL_Event& e) override {
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE) {
      // Esc: closes what is open; on the bare map it opens Opções (OptionsButton escapeKey).
      if (briefing_) briefing_.reset();
      else if (slots_) slots_.reset();
      else if (options_) options_ = false;
      else if (open_) open_.reset();
      else if (questLog_) questLog_ = false;
      else if (party_) party_ = false;
      else if (movementOpen_) movementOpen_ = false;
      else options_ = true;
    }
  }

  void drawPanel() override {
    const ImGuiIO& io = ImGui::GetIO();
    now_ = static_cast<float>(ImGui::GetTime());
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##overworld", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    if (briefing_) {
      const int r = drawBriefing(io.DisplaySize, kRem, *briefing_, paths_.engine2Public);
      if (r < 0) briefing_.reset();
      else if (r > 0 && callbacks_.play) callbacks_.play(*briefing_);
    } else if (slots_) {
      bool back = false;
      const int chosen = slots_->draw(io.DisplaySize, kRem, campaign_, paths_.engine2Public, back, &save_);
      if (back) slots_.reset();
      else if (chosen >= 0) {
        if (slots_->mode() == SlotScreen::Mode::Load) {
          save_ = campaign_.activeSave();
          notice_ = slots_->status();
          slots_.reset();
          const e2::Json& pos = save_["overworldPos"];
          prevCol_ = pos.value("col", 0), partyX_ = -1, centred_ = false;
          std::string id;
          if (save_.contains("battle") && save_["battle"].is_object()) id = save_["battle"].value("missionId", "");
          if (id.empty() && save_.contains("pendingMission") && save_["pendingMission"].is_string()) id = save_["pendingMission"].get<std::string>();
          if (!id.empty()) openMission(id, true);
        } else {
          notice_ = slots_->status();
          slots_.reset();
        }
      }
    } else {
      drawMap(io.DisplaySize);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

 private:
  static constexpr float kRem = 16.0f;  // CSS px are canvas units (DebugUi::kUiHeight)
  static constexpr float kZoomStops[4] = {70, 90, 110, 130};
  static constexpr const char* kHeroes[6] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};
  static constexpr ImU32 kAccent = IM_COL32(216, 216, 216, 255), kMuted = IM_COL32(180, 180, 180, 255), kFg = IM_COL32(240, 240, 240, 255),
                         kDanger = IM_COL32(181, 74, 50, 255), kBorder = IM_COL32(74, 74, 74, 255);

  // --- the save -------------------------------------------------------------------------------------------------------

  bool recruited(const std::string& hero) const { return world_.heroRecruited(hero, save_); }
  std::vector<std::string> partyHeroes() const {
    std::vector<std::string> out;
    for (const char* h : kHeroes)
      if (recruited(h)) out.push_back(h);
    return out;
  }
  // partyLeaderOf: the saved leader while in the party, else Kael.
  std::string leader() const {
    const e2::Json& l = save_.contains("partyLeader") ? save_["partyLeader"] : e2::Json();
    if (l.is_string() && recruited(l.get<std::string>())) return l.get<std::string>();
    return "Kael";
  }
  void write() {
    if (!campaign_.saveActive(save_)) notice_ = "Não foi possível gravar o progresso (" + e2::Campaign::savePath().string() + ").";
  }
  double num(const e2::Json& obj, const std::string& key, double fallback) const {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(key);
    return it != obj.end() && it->is_number() ? it->get<double>() : fallback;
  }
  bool completed(const std::string& id) const {
    for (const e2::Json& c : save_.value("completed", e2::Json::array()))
      if (c.is_string() && c.get<std::string>() == id) return true;
    return false;
  }
  // OverworldMapScreen mapHeroMaxHp: class HP at the hero's level plus gear HP, less the hunger penalty.
  int heroMaxHp(const std::string& hero) const {
    double gear = 0;
    if (save_.contains("equipment") && save_["equipment"].contains(hero))
      for (const auto& [slot, id] : save_["equipment"][hero].items())
        if (id.is_string())
          for (const e2::Json& e : world_.tables()["equipment"])
            if (e["id"] == id) gear += e.value("hp", 0.0);
    const double streak = num(save_, "hungerStreak", 0);
    const double penalty = std::min(0.9, std::max(0.0, streak - 3) * 0.1);
    return static_cast<int>(std::floor((world_.maxHp(save_, hero) + gear) * (1 - penalty) + 0.5));
  }

  // GameApp openMission: gated missions must be open; a finished mission only re-opens when it is a dungeon.
  void openMission(const std::string& id, bool fromSave = false) {
    if (!fromSave) {
      if (const e2::MapMission* m = world_.mission(id); m && m->gated && world_.missionAccess(id, save_) != "available") return;
      if (completed(id)) {
        const e2::MapMission* done = world_.mission(id);
        if (!done || !done->keepsSpawns) {
          notice_ = "Você não pode repetir missões já completadas.";
          return;
        }
      }
      if (const e2::MapMission* m = world_.mission(id); m && m->hub) {
        notice_ = "A Estalagem ainda não existe no Ember: é o próximo passo.";
        return;
      }
    }
    std::string error;
    if (std::optional<MissionEntry> e = campaignMission(campaign_, paths_, id, error)) briefing_ = std::move(e);
    else notice_ = error;
  }

  // GameApp onOverworldStep: stepOverworld, written at once; a road battle opens its mission, anything else its notice.
  void step(int col, int row) {
    std::random_device device;
    e2::Mulberry32 rng(device());
    std::optional<e2::OverworldEvent> ev;
    e2::Json next = world_.step(save_, {col, row}, rng, ev);
    if (next == save_) return;
    save_ = std::move(next);
    write();
    if (ev && ev->kind == "battle" && !ev->missionId.empty()) {
      openMission(ev->missionId);
      return;
    }
    event_ = ev;
  }

  // hunger.ts useRation on each hero of the list; the party's hunger streak clears when everyone has eaten.
  int feedAll() {
    int fed = 0;
    for (const std::string& hero : partyHeroes()) {
      const double rations = num(save_, "rations", 0);
      const double hp = num(save_.value("unitHp", e2::Json::object()), hero, 1);
      const double full = e2::Overworld::fullness(save_.value("heroHunger", e2::Json::object()).value(hero, e2::Json()));
      if (rations < 1 || hp <= 0 || full >= 100) continue;
      save_["rations"] = static_cast<long long>(rations - 1);
      save_["heroHunger"][hero] = 100;
      ++fed;
    }
    if (fed && (num(save_, "hungerStreak", 0) > 0 || num(save_, "hungerHours", 0) > 0)) {
      bool all = true;
      for (const std::string& hero : partyHeroes()) {
        const double hp = num(save_.value("unitHp", e2::Json::object()), hero, world_.maxHp(save_, hero));
        if (hp > 0 && !(num(save_.value("heroHunger", e2::Json::object()), hero, 100) > 0)) all = false;
      }
      if (all) save_["hungerStreak"] = 0, save_["hungerHours"] = 0;
    }
    if (fed) write();
    return fed;
  }

  void showHint(std::string text) {
    hint_ = std::move(text);
    hintAt_ = now_;
  }

  // --- the map ----------------------------------------------------------------------------------------------------------

  void drawMap(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // radial-gradient(ellipse at 30% 20%, #1e1e1e 0%, #0b0b0b 70%)
    dl->AddRectFilled({0, 0}, size, IM_COL32(11, 11, 11, 255));
    for (int i = 12; i >= 1; --i) {
      const float k = static_cast<float>(i) / 12.0f;
      const int v = 11 + static_cast<int>(19 * (1 - k));
      dl->AddCircleFilled({size.x * 0.3f, size.y * 0.2f}, std::max(size.x, size.y) * 0.7f * k, IM_COL32(v, v, v, 40), 64);
    }
    const float headerH = drawHeader(size);
    drawViewport({0, headerH}, {size.x, size.y});
    drawPartyPanel(size);
    drawZoom(size);
    if (!hint_.empty() && now_ - hintAt_ < 1.2f) centredPlate(size, 96.0f, hint_, false);
    if (!notice_.empty() && centredPlate(size, 96.0f + (hint_.empty() || now_ - hintAt_ >= 1.2f ? 0.0f : 40.0f), notice_, true)) notice_.clear();
    drawTrainingHint(size);
    if (event_) drawEvent(size);
    if (open_) drawLocationPanel(size);
    if (questLog_) drawQuestLog(size);
    if (party_) drawParty(size);
    if (options_) drawOptions(size);
    if (!save_.value("seenOverworldIntro", false)) drawIntro(size);
  }

  // A small plate centred at `y` (hints, notices); true when clicked.
  bool centredPlate(ImVec2 size, float y, const std::string& text, bool button) {
    const float t = 12.0f;
    const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(t, FLT_MAX, 0.0f, text.c_str());
    ImGui::SetCursorScreenPos({(size.x - ts.x - 24.0f) * 0.5f, y});
    ImGui::PushID(button ? "notice" : "hint");
    const bool clicked = emberTextPlate("##p", {ts.x + 24.0f, ts.y + 12.0f}, text.c_str(), t);
    ImGui::PopID();
    return button && clicked;
  }

  // The header: kicker and title on the left, the plates on the right (wrapping like Engine2's flex-wrap row).
  float drawHeader(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float top = 16.0f;
    emberKicker(dl, {16.0f, top + 4.0f}, 14.0f, "CAMPANHA · RPG");
    emberTitle(dl, {16.0f, top + 22.0f}, 30.0f, "Mapa");
    // The plates, right to left.
    ImFont* ui = uiFont(UiFont::Ui);
    const float h = 36.0f, gap = 12.0f;
    float x = size.x - 16.0f;
    const float y = top + 6.0f;
    auto place = [&](float w) {
      x -= w;
      const ImVec2 at{x, y};
      x -= gap;
      ImGui::SetCursorScreenPos(at);
    };
    // Gold
    const std::string gold = std::format("{} Gold", static_cast<long long>(num(save_, "ember", 0)));
    const float goldW = 8.0f + 24.0f + 6.0f + ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, gold.c_str()).x + 8.0f;
    place(goldW);
    emberPlate("##gold", {goldW, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
      if (const UiImage* icon = uiImage(paths_.engine2Public / "game/icons/gold-coins-pile.png"))
        d->AddImage(icon->texture, {a.x + 8, (a.y + b.y) * 0.5f - 12}, {a.x + 32, (a.y + b.y) * 0.5f + 12});
      d->AddText(ui, 14.0f, {a.x + 38, (a.y + b.y) * 0.5f - 8}, kMuted, gold.c_str());
    });
    // Opções
    place(h);
    if (emberPlate("##options", {h, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconGear(d, {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}, 16, ink); }))
      options_ = true;
    // Alimentar todos
    {
      const char* label = "Alimentar todos";
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, label).x + 24.0f;
      place(w);
      if (emberTextPlate("##feed", {w, h}, label, 14.0f)) {
        const int heroes = static_cast<int>(partyHeroes().size());
        const int fed = feedAll();
        showHint(fed == 0 ? "Ninguém comeu — sem rações ou já saciados." : fed == heroes ? "Todos comeram." : std::format("{} comeram — rações não deram pros demais.", fed));
      }
    }
    // Fome
    if (const double streak = num(save_, "hungerStreak", 0); streak > 0) {
      const std::string text = std::format("Fome {}d", static_cast<long long>(streak));
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text.c_str()).x + 16.0f;
      place(w);
      emberPlate("##hunger", {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { d->AddText(ui, 14.0f, {a.x + 8, (a.y + b.y) * 0.5f - 8}, kDanger, text.c_str()); });
    }
    // Rações
    {
      const std::string text = std::format("Rações {}", static_cast<long long>(num(save_, "rations", 0)));
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text.c_str()).x + 16.0f;
      place(w);
      emberPlate("##rations", {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { d->AddText(ui, 14.0f, {a.x + 8, (a.y + b.y) * 0.5f - 8}, kMuted, text.c_str()); });
    }
    // The moon (moonPhase.ts: an 8-day cycle, every third full moon a blood moon)
    {
      place(h);
      const int day = static_cast<int>(num(save_, "gameClock", 0));
      static const char* kPhases[8] = {"new", "waxingCrescent", "firstQuarter", "waxingGibbous", "full", "waningGibbous", "lastQuarter", "waningCrescent"};
      const int d = std::max(0, day - 1);
      std::string phase = kPhases[d % 8];
      if (phase == "full" && (d / 8) % 3 == 2) phase = "blood";
      const std::string icon = phase == "full" ? "game/ui/moon/moon-full-v2.png" : "game/ui/moon/moon-" + phase + ".png";
      emberPlate("##moon", {h, h}, [&](ImDrawList* dd, ImVec2 a, ImVec2 b, ImU32) {
        if (const UiImage* img = uiImage(paths_.engine2Public / icon)) dd->AddImage(img->texture, {a.x + 4, a.y + 4}, {b.x - 4, b.y - 4});
      });
    }
    // Clock
    {
      const std::string text = std::format("Dia {} · {:02}:00", static_cast<long long>(num(save_, "gameClock", 0)), e2::Overworld::campaignHour(save_));
      const float w = 8 + 16 + 8 + ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text.c_str()).x + 8;
      place(w);
      emberPlate("##clock", {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
        iconClock(d, {a.x + 16, (a.y + b.y) * 0.5f}, 16, kMuted);
        d->AddText(ui, 14.0f, {a.x + 32, (a.y + b.y) * 0.5f - 8}, kFg, text.c_str());
      });
    }
    // Missões
    {
      const float w = 12 + 16 + 8 + ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, "Missões").x + 12;
      place(w);
      if (emberPlate("##quests", {w, 40.0f}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) {
            iconBook(d, {a.x + 20, (a.y + b.y) * 0.5f}, 16, ink);
            d->AddText(ui, 14.0f, {a.x + 36, (a.y + b.y) * 0.5f - 8}, ink, "Missões");
          }))
        questLog_ = true;
    }
    // Party
    {
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, "Party").x + 20;
      place(w);
      if (emberTextPlate("##party", {w, h}, "Party", 14.0f)) party_ = true, partyTab_ = 0, formationSaved_.clear(), formation_.clear();
    }
    return top + 64.0f + 16.0f;
  }

  // The map itself: the painted world in a scrollable viewport (drag with the mouse, wheel scrolls), the fog, the
  // time-of-day tint, the pins, the step hexes and the leader.
  void drawViewport(ImVec2 a, ImVec2 b) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const UiImage* map = uiImage(paths_.engine2Public / "game/assets/world-map.jpg");
    const float vw = b.x - a.x, vh = b.y - a.y;
    const float imgW = vw * kZoomStops[zoom_] / 100.0f;
    const float imgH = map ? imgW * static_cast<float>(map->height) / static_cast<float>(map->width) : vh * 0.7f;
    const float contentW = imgW + 16.0f, contentH = imgH + 16.0f;
    const e2::Json& pos = save_["overworldPos"];
    const int col = pos.value("col", 0), row = pos.value("row", 0);
    double px = 0, py = 0;
    e2::Overworld::hexToWorld(col, row, px, py);
    // Opens centred on the party (mount), keeps the centre when zooming.
    if (!centred_) {
      centreX_ = static_cast<float>(px / 100.0), centreY_ = static_cast<float>(py / 100.0), centred_ = true, recentre_ = true;
    }
    if (recentre_) {
      scrollX_ = centreX_ * contentW - vw / 2, scrollY_ = centreY_ * contentH - vh / 2;
      recentre_ = false;
    }
    scrollX_ = std::clamp(scrollX_, 0.0f, std::max(0.0f, contentW - vw));
    scrollY_ = std::clamp(scrollY_, 0.0f, std::max(0.0f, contentH - vh));
    // Input over the viewport: drag (moved > 4 px) pans, wheel scrolls (shift: sideways).
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton("##viewport", {vw, vh}, ImGuiButtonFlags_MouseButtonLeft);
    const bool overViewport = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActivated()) drag_ = {io.MousePos.x, io.MousePos.y, scrollX_, scrollY_}, dragMoved_ = false;
    if (ImGui::IsItemActive() && drag_) {
      const float dx = io.MousePos.x - drag_->x, dy = io.MousePos.y - drag_->y;
      if (!dragMoved_ && (std::abs(dx) > 4 || std::abs(dy) > 4)) dragMoved_ = true;
      if (dragMoved_) scrollX_ = drag_->sx - dx, scrollY_ = drag_->sy - dy;
    }
    if (ImGui::IsItemDeactivated()) drag_.reset();
    if (overViewport && io.MouseWheel != 0.0f) {
      if (io.KeyShift) scrollX_ -= io.MouseWheel * 100.0f;
      else scrollY_ -= io.MouseWheel * 100.0f;
    }
    scrollX_ = std::clamp(scrollX_, 0.0f, std::max(0.0f, contentW - vw));
    scrollY_ = std::clamp(scrollY_, 0.0f, std::max(0.0f, contentH - vh));
    const bool clicked = overViewport && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !dragMoved_;
    centreX_ = (scrollX_ + vw / 2) / contentW, centreY_ = (scrollY_ + vh / 2) / contentH;

    const ImVec2 o{a.x + 8.0f - scrollX_, a.y + 8.0f - scrollY_};  // the image's corner
    auto at = [&](double xPct, double yPct) { return ImVec2{o.x + static_cast<float>(xPct) / 100.0f * imgW, o.y + static_cast<float>(yPct) / 100.0f * imgH}; };
    dl->PushClipRect(a, b, true);
    if (map) dl->AddImageRounded(map->texture, o, {o.x + imgW, o.y + imgH}, {0, 0}, {1, 1}, IM_COL32_WHITE, 8.0f);
    drawFog(dl, o, imgW, imgH);
    // Sky tint by the hour.
    {
      const std::string t = e2::Overworld::timeOfDay(e2::Overworld::campaignHour(save_));
      const ImU32 tint = t == "darkNight" ? IM_COL32(5, 12, 43, 173) : t == "brightNight" ? IM_COL32(34, 62, 132, 122) : t == "dusk" ? IM_COL32(166, 67, 39, 82)
                         : t == "dawn" ? IM_COL32(230, 137, 112, 64) : t == "noon" ? IM_COL32(255, 197, 108, 31) : 0;
      if (tint) dl->AddRectFilled(o, {o.x + imgW, o.y + imgH}, tint, 8.0f);
    }
    // Reachable: the party's hex and every neighbour it may step to.
    std::vector<e2::OverworldHex> dots;
    for (const e2::OverworldHex& n : e2::Overworld::neighbors(col, row))
      if (world_.isCell(n.col, n.row) && world_.canStep(save_, {col, row}, n)) dots.push_back(n);
    auto walkable = [&](int c, int r) {
      if (c == col && r == row) return true;
      return std::any_of(dots.begin(), dots.end(), [&](const e2::OverworldHex& d) { return d.col == c && d.row == r; });
    };
    const e2::MapLocation* standing = world_.locationAt(col, row);
    std::string clickedLocation;
    std::optional<e2::OverworldHex> clickedDot;
    bool clickedLeader = false;
    const ImVec2 mouse = io.MousePos;
    // Pins.
    std::vector<std::pair<const e2::MapLocation*, std::string>> pins;
    for (const e2::MapLocation& loc : world_.locations()) {
      if (!world_.locationVisible(loc, save_)) continue;
      // isExplored: Stone Bridge before O Vau, the Inn, where the party stands, or a hex it has stood on.
      bool explored = (!completed("vau") && loc.id == "stonebridge") || loc.id == "estalagem" || (standing && standing->id == loc.id);
      for (const e2::Json& h : save_.value("exploredHexes", e2::Json::array()))
        if (h.is_string() && h.get<std::string>() == std::format("{},{}", loc.col, loc.row)) explored = true;
      if (!explored) continue;
      pins.push_back({&loc, world_.locationStatus(loc, save_)});
    }
    for (const auto& [loc, st] : pins) {
      const ImVec2 c = at(loc->x, loc->y);
      const bool reachable = walkable(loc->col, loc->row) || (!completed("vau") && loc->id == "stonebridge");
      const float r = 20.0f;
      const bool hover = std::hypot(mouse.x - c.x, mouse.y - c.y) <= r && overViewport;
      const float s = hover ? 1.1f : 1.0f;
      const int alpha = reachable ? 255 : 178;
      const auto missions = world_.visibleMissionsOf(*loc, save_);
      const bool hub = std::any_of(missions.begin(), missions.end(), [](const e2::MapMission* m) { return m->hub; });
      ImU32 border = kAccent;
      if (st == "locked") border = IM_COL32(74, 74, 74, 128);
      else if (st != "done" && hub) {
        const float k = 0.5f - 0.5f * std::cos(now_ / 2.8f * 2 * std::numbers::pi_v<float>);
        border = IM_COL32(static_cast<int>(73 + (201 - 73) * k), static_cast<int>(73 + (160 - 73) * k), static_cast<int>(73 + (112 - 73) * k), 255);
        for (int i = 3; i >= 1; --i) dl->AddCircle(c, r * s + i * 3.0f, IM_COL32(201, 150, 88, static_cast<int>((22 + 28 * k) * 0.6f)), 32, 3.0f);
      }
      if (flashId_ == loc->id && now_ - flashAt_ < 0.5f) {
        const float k = std::sin((now_ - flashAt_) / 0.5f * std::numbers::pi_v<float>);
        border = IM_COL32(214, 60, 60, 255);
        for (int i = 3; i >= 1; --i) dl->AddCircle(c, r + i * 3.0f, IM_COL32(214, 60, 60, static_cast<int>(70 * k)), 32, 3.0f);
      }
      dl->AddCircleFilled(c, r * s, IM_COL32(17, 17, 17, alpha * 204 / 255), 32);
      dl->AddCircle(c, r * s - 1, (border & ~IM_COL32_A_MASK) | (static_cast<ImU32>(((border >> IM_COL32_A_SHIFT) & 0xFF) * alpha / 255) << IM_COL32_A_SHIFT), 32, 2.0f);
      const ImU32 iconInk = st == "locked" ? IM_COL32(180, 180, 180, alpha / 2) : IM_COL32(216, 216, 216, alpha);
      if (st == "locked") iconLock(dl, c, 16 * s, iconInk);
      else if (st == "done") iconCheck(dl, c, 16 * s, iconInk);
      else iconMapPin(dl, c, 16 * s, iconInk);
      if (missions.size() > 1) {
        const ImVec2 bc{c.x + r * s - 2, c.y - r * s + 2};
        dl->AddCircleFilled(bc, 8, IM_COL32(17, 17, 17, 255));
        dl->AddCircle(bc, 8, kBorder);
        const std::string n = std::to_string(missions.size());
        const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(10.0f, FLT_MAX, 0, n.c_str());
        dl->AddText(uiFont(UiFont::Ui), 10.0f, {bc.x - ts.x / 2, bc.y - ts.y / 2}, IM_COL32(240, 240, 240, 230), n.c_str());
      }
      if (hover && clicked) clickedLocation = loc->id;
    }
    // Step hexes (movement open): the hex shape, the accent wash, the travel time.
    if (movementOpen_)
      for (const e2::OverworldHex& d : dots) {
        const e2::OverworldCell* cell = world_.cell(d.col, d.row);
        const ImVec2 c = at(cell->x, cell->y);
        const float w = std::max(44.0f, std::sqrt(3.0f) * 5.0f / 100.0f * imgW), h = std::max(44.0f, 10.0f / 100.0f * imgH);
        const ImVec2 p[6] = {{c.x, c.y - h / 2}, {c.x + w / 2, c.y - h / 4}, {c.x + w / 2, c.y + h / 4}, {c.x, c.y + h / 2}, {c.x - w / 2, c.y + h / 4}, {c.x - w / 2, c.y - h / 4}};
        const bool hover = overViewport && std::abs(mouse.x - c.x) < w / 2 && std::abs(mouse.y - c.y) < h / 2 - std::abs(mouse.x - c.x) / (w / 2) * h / 4;
        dl->AddConvexPolyFilled(p, 6, hover ? IM_COL32(216, 216, 216, 204) : IM_COL32(216, 216, 216, 122));
        if (hover) dl->AddPolyline(p, 6, kFg, 2.0f, ImDrawFlags_Closed);
        const std::string label = e2::Overworld::travelTimeLabel(cell->hours);
        const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(12.0f, FLT_MAX, 0, label.c_str());
        dl->AddText(uiFont(UiFont::Ui), 12.0f, {c.x - ts.x / 2 + 1, c.y - ts.y / 2 + 1}, IM_COL32(17, 17, 17, 255), label.c_str());
        dl->AddText(uiFont(UiFont::Ui), 12.0f, {c.x - ts.x / 2, c.y - ts.y / 2}, kFg, label.c_str());
        if (hover && clicked) clickedDot = d;
        if (hover) ImGui::SetTooltip("Saciedade: −%s", std::format("{}", e2::Overworld::travelHungerCost(cell->hours)).c_str());
      }
    // The leader: idle frames of their own sprite, sliding hex to hex (0.5 s), turned the way they last walked.
    {
      double tx = px, ty = py;
      if (standing) tx = standing->x, ty = standing->y;
      if (partyX_ < 0) partyX_ = static_cast<float>(tx), partyY_ = static_cast<float>(ty), fromX_ = partyX_, fromY_ = partyY_, moveAt_ = -10;
      if (std::abs(targetX_ - tx) > 1e-6 || std::abs(targetY_ - ty) > 1e-6) {
        fromX_ = partyX_, fromY_ = partyY_, targetX_ = static_cast<float>(tx), targetY_ = static_cast<float>(ty), moveAt_ = now_;
        if (col != prevCol_) facingLeft_ = col < prevCol_;
        prevCol_ = col;
      }
      const float k = std::clamp((now_ - moveAt_) / 0.5f, 0.0f, 1.0f);
      const float e = k < 0.5f ? 2 * k * k : 1 - std::pow(-2 * k + 2, 2.0f) / 2;  // ease-in-out
      partyX_ = fromX_ + (targetX_ - fromX_) * e, partyY_ = fromY_ + (targetY_ - fromY_) * e;
      const ImVec2 foot = at(partyX_, partyY_);
      const Box box = drawLeader(dl, foot, leader());
      if (overViewport && box.contains(mouse) && clicked) clickedLeader = true;
    }
    dl->PopClipRect();

    // Clicks, in Engine2's order of priority (the leader's button sits on top).
    if (clickedLeader) movementOpen_ = !movementOpen_;
    else if (clickedDot) walkTo(clickedDot->col, clickedDot->row);
    else if (!clickedLocation.empty()) {
      for (const auto& [loc, st] : pins)
        if (loc->id == clickedLocation) pinClicked(*loc, st, walkable(loc->col, loc->row), standing);
    }
  }

  void walkTo(int col, int row) {
    if (!movementOpen_) return;
    const e2::Json& pos = save_["overworldPos"];
    if (!world_.canStep(save_, {pos.value("col", 0), pos.value("row", 0)}, {col, row})) return;
    movementOpen_ = false;
    step(col, row);
  }

  void pinClicked(const e2::MapLocation& loc, const std::string& st, bool walkable, const e2::MapLocation* standing) {
    if (!completed("vau") && loc.id == "stonebridge") return openMission("vau");
    if (standing && standing->id == loc.id) return enterLocation(loc, st);
    if (walkable) return walkTo(loc.col, loc.row);
    showHint("Ande até lá primeiro.");
  }

  void enterLocation(const e2::MapLocation& loc, const std::string& st) {
    if (loc.deadlineDay && num(save_, "gameClock", 0) > *loc.deadlineDay) return showHint("Prazo esgotado por aqui.");
    if (st == "locked") {
      flashId_ = loc.id, flashAt_ = now_;
      return;
    }
    const auto missions = world_.visibleMissionsOf(loc, save_);
    if (missions.size() > 1) {
      open_ = loc.id;
      return;
    }
    if (!missions.empty()) {
      const e2::MapMission* m = missions[0];
      std::vector<std::string> order;
      for (const e2::MapMission* x : world_.missionsOf(loc)) order.push_back(x->id);
      if (m->crossing && world_.missionStatus(m->id, save_, order) == "done") open_ = loc.id;
      else openMission(m->id);
    }
  }

  // The fog: dark (rgba 6,6,6,.78) except a soft circle (radius 9 % of the image, solid to 65 %) around every hex the
  // party has stood on, drawn as a grid of shaded cells (Engine2's SVG mask, circles compositing over each other).
  void drawFog(ImDrawList* dl, ImVec2 o, float w, float h) {
    constexpr int N = 96;
    std::vector<std::pair<double, double>> centres;
    for (const e2::Json& k : save_.value("exploredHexes", e2::Json::array())) {
      if (!k.is_string()) continue;
      int c = 0, r = 0;
      if (std::sscanf(k.get<std::string>().c_str(), "%d,%d", &c, &r) != 2) continue;
      double x = 0, y = 0;
      e2::Overworld::hexToWorld(c, r, x, y);
      centres.push_back({x, y});
    }
    std::vector<float> alpha((N + 1) * (N + 1));
    for (int j = 0; j <= N; ++j)
      for (int i = 0; i <= N; ++i) {
        const double x = 100.0 * i / N, y = 100.0 * j / N;
        double keep = 1.0;
        for (const auto& [cx, cy] : centres) {
          const double d = std::hypot(x - cx, y - cy) / 9.0;
          const double a = d <= 0.65 ? 1.0 : d >= 1.0 ? 0.0 : 1.0 - (d - 0.65) / 0.35;
          keep *= 1.0 - a;
        }
        alpha[static_cast<size_t>(j * (N + 1) + i)] = static_cast<float>(0.78 * keep);
      }
    auto col = [&](int i, int j) { return IM_COL32(6, 6, 6, static_cast<int>(255 * alpha[static_cast<size_t>(j * (N + 1) + i)])); };
    for (int j = 0; j < N; ++j)
      for (int i = 0; i < N; ++i) {
        const ImVec2 a{o.x + w * i / N, o.y + h * j / N}, b{o.x + w * (i + 1) / N, o.y + h * (j + 1) / N};
        dl->AddRectFilledMultiColor(a, b, col(i, j), col(i + 1, j), col(i + 1, j + 1), col(i, j + 1));
      }
  }

  // LeaderMarker: the leader's idle frames (BattleEngine.idleFrame pacing), each cropped to its own box, 48 px tall,
  // its feet on the hex; mirrored when last walking left. Returns the button's box.
  struct Box {
    ImVec2 a, b;
    bool contains(ImVec2 p) const { return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y; }
  };
  Box drawLeader(ImDrawList* dl, ImVec2 foot, const std::string& hero) {
    struct Sheet {
      const char* hero;
      const char* dir;
      int idle;
    };
    static const Sheet kSheets[] = {{"Kael", "Kael_Final/kael-final-002", 36}, {"Neera", "neera/neera-v2-001/idle-", 36}, {"Voss", "voss", 12},
                                    {"Salazar", "salazar-final", 36},       {"Aldric", "aldric", 36},                    {"Malrec", "malrec", 36}};
    const Sheet* sheet = &kSheets[0];
    for (const Sheet& s : kSheets)
      if (hero == s.hero) sheet = &s;
    const int cycle = sheet->idle * 2 - 2;
    const double pace = (sheet->idle >= 24 ? sheet->idle / 3.0 : cycle / 2.6) * (hero == "Malrec" ? 0.9 : 1.0);
    const int stepI = static_cast<int>(std::floor(now_ * pace)) % cycle;
    const int source = stepI < sheet->idle ? stepI : cycle - stepI;
    const int count = sheet->idle == 36 ? 12 : sheet->idle;
    const int frameIndex = (sheet->idle == 36 ? source / 3 : source) % count;
    const int file = sheet->idle == 36 ? 1 + frameIndex * 3 : 1 + frameIndex;
    const e2::Json& bounds = world_.tables()["leaderFrameBounds"][hero];
    float left, top, width, height, cw, ch;
    if (hero == "Neera" || hero == "Salazar") {
      float l = 1e9f, t = 1e9f, r = -1e9f, bt = -1e9f;
      for (const e2::Json& b : bounds) {
        l = std::min(l, b[0].get<float>()), t = std::min(t, b[1].get<float>());
        r = std::max(r, b[0].get<float>() + b[2].get<float>()), bt = std::max(bt, b[1].get<float>() + b[3].get<float>());
      }
      left = l, top = t, width = r - l, height = bt - t, cw = bounds[0][4].get<float>(), ch = bounds[0][5].get<float>();
    } else {
      const e2::Json& b = bounds[static_cast<size_t>(frameIndex) % bounds.size()];
      left = b[0].get<float>(), top = b[1].get<float>(), width = b[2].get<float>(), height = b[3].get<float>(), cw = b[4].get<float>(), ch = b[5].get<float>();
    }
    const float scale = 48.0f / height;
    const float boxW = width * scale;
    // flex h-12 w-16 items-end justify-center: a 64 x 48 box, its bottom on the hex (translate -50%, -100%).
    const Box button{{foot.x - std::max(32.0f, boxW / 2), foot.y - 48.0f}, {foot.x + std::max(32.0f, boxW / 2), foot.y}};
    const std::string path = std::string("game/sprites/") + sheet->dir + (hero == "Neera" ? "" : "/") + std::to_string(file) + ".png";
    if (const UiImage* img = uiImage(paths_.engine2Public / path)) {
      const float x0 = foot.x - boxW / 2, y0 = foot.y - 48.0f;
      // The crop box of the frame, in texture coordinates.
      float u0 = left / cw, v0 = top / ch, u1 = (left + width) / cw, v1 = (top + height) / ch;
      if (facingLeft_) std::swap(u0, u1);
      // drop-shadow(0 2px 3px rgba(0,0,0,.6))
      dl->AddImage(img->texture, {x0, y0 + 2}, {x0 + boxW, y0 + 50}, {u0, v0}, {u1, v1}, IM_COL32(0, 0, 0, 120));
      dl->AddImage(img->texture, {x0, y0}, {x0 + boxW, y0 + 48}, {u0, v0}, {u1, v1});
    }
    return button;
  }

  // --- the party panel, bottom left -------------------------------------------------------------------------------------

  void drawPartyPanel(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const std::vector<std::string> heroes = partyHeroes();
    const e2::Json& pos = save_["overworldPos"];
    const e2::MapLocation* standing = world_.locationAt(pos.value("col", 0), pos.value("row", 0));
    std::string line;
    const auto wisp = world_.wispHex();
    if (wisp && world_.wispForestUncleared(save_) && pos.value("col", 0) == wisp->col && pos.value("row", 0) == wisp->row)
      line = "Limpe a Wisp Forest para poder seguir viagem para o leste.";
    else
      line = movementOpen_ ? "Escolha um hexágono · o tempo depende do terreno" : "Clique em " + leader() + " para mover";
    const float pad = 12.0f;
    const float rowW = static_cast<float>(heroes.size()) * 40.0f + static_cast<float>(std::max<size_t>(1, heroes.size()) - 1) * 12.0f;
    const float textW = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, line.c_str()).x;
    const float w = std::min(size.x - 96.0f, std::max(rowW, textW) + 2 * pad);
    const float h = pad + 12.0f * 1.4f + 8.0f + 48.0f + 8.0f + 8.0f + (standing ? 44.0f : 0.0f) + pad;
    const ImVec2 a{16.0f, size.y - 16.0f - h}, b{a.x + w, size.y - 16.0f};
    if (const UiImage* bg = uiImage(paths_.engine2Public / "game/ui/map-status-background.jpg")) imageCover(dl, *bg, a, b);
    dl->AddRectFilled(a, b, IM_COL32(17, 17, 17, 64), 0);
    dl->AddRect(a, b, kBorder, 0);
    // Block input under the panel.
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton("##partypanel", {w, h});
    dl->AddText(ui, 12.0f, {a.x + pad, a.y + pad}, kMuted, line.c_str());
    float x = a.x + pad;
    const float y = a.y + pad + 12.0f * 1.4f + 8.0f;
    for (const std::string& hero : heroes) {
      const e2::Json& portrait = world_.tables()["portraits"][hero];
      std::string src = portrait.value("src", "");
      if (const size_t q = src.find('?'); q != std::string::npos) src.resize(q);
      if (!src.empty() && src[0] == '/') src.erase(0, 1);
      if (const UiImage* img = uiImage(paths_.engine2Public / std::filesystem::path(reinterpret_cast<const char8_t*>(src.c_str())))) imageCover(dl, *img, {x, y}, {x + 40, y + 48});
      ImGui::SetCursorScreenPos({x, y});
      ImGui::PushID(hero.c_str());
      ImGui::InvisibleButton("##portrait", {40, 48});
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hero.c_str());
      ImGui::PopID();
      // HungerBar: 4 px, the first 100 of 120 in the accent (red at 25 or less), the overfed part gold.
      const double full = e2::Overworld::fullness(save_.value("heroHunger", e2::Json::object()).value(hero, e2::Json()));
      const float by = y + 48 + 4;
      dl->AddRectFilled({x, by}, {x + 40, by + 4}, IM_COL32(74, 74, 74, 255), 2.0f);
      dl->AddRectFilled({x, by}, {x + 40 * static_cast<float>(std::min(100.0, full) / 120.0), by + 4}, full <= 25 ? kDanger : kAccent, 2.0f);
      if (full > 100) dl->AddRectFilled({x + 40 * 100.0f / 120.0f, by}, {x + 40 * static_cast<float>(full / 120.0), by + 4}, IM_COL32(201, 162, 39, 255));
      // LifeBar: red, green when poisoned, purple when ill.
      const int maxHp = std::max(1, heroMaxHp(hero));
      const double hp = std::clamp(num(save_.value("unitHp", e2::Json::object()), hero, maxHp), 0.0, static_cast<double>(maxHp));
      const bool poisoned = save_.contains("heroPoisons") && save_["heroPoisons"].contains(hero) && !save_["heroPoisons"][hero].is_null() && save_["heroPoisons"][hero] != false;
      const bool diseased = save_.contains("heroDiseases") && save_["heroDiseases"].value(hero, false);
      const float ly = by + 8;
      dl->AddRectFilled({x, ly}, {x + 40, ly + 4}, IM_COL32(74, 74, 74, 255), 2.0f);
      const float lw = 40 * static_cast<float>(hp / maxHp);
      if (poisoned && diseased) dl->AddRectFilledMultiColor({x, ly}, {x + lw, ly + 4}, IM_COL32(63, 155, 58, 255), IM_COL32(138, 70, 194, 255), IM_COL32(138, 70, 194, 255), IM_COL32(63, 155, 58, 255));
      else dl->AddRectFilled({x, ly}, {x + lw, ly + 4}, poisoned ? IM_COL32(63, 155, 58, 255) : diseased ? IM_COL32(138, 70, 194, 255) : IM_COL32(184, 50, 42, 255), 2.0f);
      x += 52.0f;
    }
    if (standing) {
      const std::string label = "Explorar " + standing->name;
      ImGui::SetCursorScreenPos({a.x + pad, b.y - pad - 44.0f});
      if (ImGui::InvisibleButton("##explore", {ui->CalcTextSizeA(12.0f, FLT_MAX, 0, label.c_str()).x, 44.0f})) enterLocation(*standing, world_.locationStatus(*standing, save_));
      dl->AddText(ui, 12.0f, {a.x + pad, b.y - pad - 28.0f}, kAccent, label.c_str());
    }
  }

  void drawZoom(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float x = size.x - 16.0f - 52.0f, y = size.y - 16.0f - (44 * 2 + 20 + 8);
    // ember-plate p-1
    dl->AddRectFilled({x, y}, {x + 52, y + 44 * 2 + 20 + 8}, IM_COL32(17, 17, 17, 240), 2.0f);
    dl->AddRect({x, y}, {x + 52, y + 44 * 2 + 20 + 8}, IM_COL32(190, 150, 95, 115), 2.0f);
    ImGui::SetCursorScreenPos({x + 4, y + 4});
    if (emberPlate("##zoomin", {44, 44}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconZoom(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 20, ink, true); }, zoom_ >= 3))
      zoom_ = std::min(3, zoom_ + 1), recentre_ = true;
    const std::string label = std::format("{}/4", 4 - zoom_);
    const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(10.0f, FLT_MAX, 0, label.c_str());
    dl->AddText(uiFont(UiFont::Ui), 10.0f, {x + 26 - ts.x / 2, y + 4 + 44 + 4}, kMuted, label.c_str());
    ImGui::SetCursorScreenPos({x + 4, y + 4 + 44 + 20});
    if (emberPlate("##zoomout", {44, 44}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconZoom(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 20, ink, false); }, zoom_ <= 0))
      zoom_ = std::max(0, zoom_ - 1), recentre_ = true;
  }

  // --- dialogs ----------------------------------------------------------------------------------------------------------

  // A modal veil (ember-veil) under a panel; true when the veil itself was clicked.
  bool veil(ImVec2 size, const char* id) {
    ImGui::SetCursorScreenPos({0, 0});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool clicked = ImGui::InvisibleButton(id, size);
    verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(0, 0, 0, 89)}, {1.0f, IM_COL32(0, 0, 0, 184)}});
    return clicked;
  }

  // A panel window centred on the screen, drawn above the map (its own ImGui window, so its buttons win the clicks).
  bool beginPanel(const char* id, ImVec2 size, float width, float maxHeight) {
    ImGui::SetNextWindowPos({size.x / 2, size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({std::min(width, size.x - 32.0f), 0});
    ImGui::SetNextWindowSizeConstraints({0, 0}, {std::min(width, size.x - 32.0f), maxHeight});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    const bool open = ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
    emberPanel(ImGui::GetWindowDrawList(), a, b);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
    return open;
  }
  void endPanel() {
    ImGui::GetWindowDrawList()->ChannelsMerge();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }
  // Text helpers inside panels.
  static void text(const std::string& s, float size, ImU32 color, UiFont font = UiFont::Ui, float wrap = 0) {
    ImGui::PushFont(uiFont(font), size);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    if (wrap > 0) ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
    ImGui::TextUnformatted(s.c_str());
    if (wrap > 0) ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }
  static void kicker(const std::string& s, float size) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    emberKicker(ImGui::GetWindowDrawList(), at, size, upperUtf8(s).c_str());
    ImGui::Dummy({1, size * 1.3f});
  }
  static void title(const std::string& s, float size) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    emberTitle(ImGui::GetWindowDrawList(), at, size, s.c_str());
    ImGui::Dummy({1, size * 1.1f});
  }
  static bool closeButton(const char* id) {
    return emberPlate(id, {36, 36}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, IM_COL32(223, 191, 142, 255)); });
  }
  static void slot(ImVec2 a, ImVec2 b) { emberSlot(ImGui::GetWindowDrawList(), a, b, false); }

  void drawEvent(ImVec2 size) {
    veil(size, "##eventveil");
    beginPanel("##event", size, 384, size.y * 0.8f);
    text(event_->text, 14.0f, kFg, UiFont::Ui, 340);
    ImGui::Dummy({1, 16});
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (emberBigButton("##eventok", "OK", at, {344, 44}, true, 14.0f)) event_.reset();
    endPanel();
  }

  void drawLocationPanel(ImVec2 size) {
    const e2::MapLocation* loc = nullptr;
    for (const e2::MapLocation& l : world_.locations())
      if (l.id == *open_) loc = &l;
    if (!loc) return open_.reset();
    if (veil(size, "##locveil")) return open_.reset();
    beginPanel("##location", size, 448, size.y * 0.8f);
    const float inner = 408;
    const ImVec2 head = ImGui::GetCursorScreenPos();
    title(loc->name, 20.0f);
    ImGui::SetCursorScreenPos({head.x + inner - 32, head.y});
    if (emberPlate("##locclose", {32, 32}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, IM_COL32(223, 191, 142, 255)); }))
      open_.reset();
    ImGui::SetCursorScreenPos({head.x, head.y + 40});
    std::vector<std::string> order;
    for (const e2::MapMission* m : world_.missionsOf(*loc)) order.push_back(m->id);
    int i = 0;
    std::string pick;
    for (const e2::MapMission* m : world_.visibleMissionsOf(*loc, save_)) {
      ++i;
      const std::string st = world_.missionStatus(m->id, save_, order);
      const bool optionalCrossing = st == "done" && m->crossing;
      const e2::CampaignMission* cm = campaign_.data().mission(m->id);
      const std::string place = cm ? cm->place : std::string();
      const std::string objective = cm ? cm->objective : std::string();
      const ImVec2 a = ImGui::GetCursorScreenPos();
      ImFont* ui = uiFont(UiFont::Ui);
      const float objH = ui->CalcTextSizeA(16.0f, FLT_MAX, inner - 32, objective.c_str()).y;
      const float h = 12 + 18 + 30 + objH + (optionalCrossing ? 24 + 52 : 0) + 12;
      slot(a, {a.x + inner, a.y + h});
      ImGui::PushID(m->id.c_str());
      ImGui::SetCursorScreenPos({a.x + 12, a.y + 8});
      const bool clicked = ImGui::InvisibleButton("##mission", {inner - 24, 18 + 30 + objH + 8});
      const bool hover = ImGui::IsItemHovered();
      const int fade = st == "locked" ? 102 : 255;
      ImDrawList* dl = ImGui::GetWindowDrawList();
      if (flashId_ == m->id && now_ - flashAt_ < 0.5f) dl->AddRect(a, {a.x + inner, a.y + h}, IM_COL32(214, 60, 60, 200), 0.0f, 2.0f, ImDrawFlags_None);
      const std::string kick = std::format("{:02} · {}{}", i, place, st == "done" ? " · feito" : "");
      float kx = a.x + 16;
      if (st == "locked") iconLock(dl, {kx + 6, a.y + 20}, 12, IM_COL32(223, 191, 142, fade)), kx += 18;
      if (st == "done") iconCheck(dl, {kx + 6, a.y + 20}, 12, kAccent), kx += 18;
      emberKicker(dl, {kx, a.y + 12}, 14.0f, upperUtf8(kick).c_str());
      emberTitle(dl, {a.x + 16, a.y + 32}, 24.0f, m->title.c_str());
      dl->AddText(ui, 16.0f, {a.x + 16, a.y + 62}, IM_COL32(180, 180, 180, fade), objective.c_str(), nullptr, inner - 32);
      if (hover && st != "locked" && !optionalCrossing) dl->AddRect(a, {a.x + inner, a.y + h}, IM_COL32(226, 170, 100, 120), 0.0f, 1.0f, ImDrawFlags_None);
      if (clicked && !optionalCrossing) {
        if (st == "locked") flashId_ = m->id, flashAt_ = now_;
        else pick = m->id;
      }
      if (optionalCrossing) {
        dl->AddText(ui, 14.0f, {a.x + 16, a.y + 66 + objH}, kAccent, "Travessia concluída · escolha como seguir");
        const float bw = (inner - 40) / 2;
        if (emberBigButton("##pass", "PASSAR SEM ENTRAR", {a.x + 16, a.y + 90 + objH}, {bw, 40}, false, 12.0f)) open_.reset();
        if (emberBigButton("##again", "EXPLORAR DE NOVO", {a.x + 24 + bw, a.y + 90 + objH}, {bw, 40}, true, 12.0f)) pick = m->id;
      }
      ImGui::PopID();
      ImGui::SetCursorScreenPos({a.x, a.y + h + 8});
    }
    ImGui::Dummy({inner, 1});
    endPanel();
    if (!pick.empty()) {
      open_.reset();
      openMission(pick);
    }
  }

  void drawQuestLog(ImVec2 size) {
    if (veil(size, "##questveil")) {
      questLog_ = false;
      return;
    }
    beginPanel("##questlog", size, 672, size.y * 0.82f);
    const float inner = 632;
    const e2::Json& quests = world_.tables()["quests"];
    struct Entry {
      const e2::Json* q;
      std::string status;
      int have, total;
    };
    std::vector<Entry> active, done;
    auto has = [&](const char* key, const std::string& v) {
      for (const e2::Json& x : save_.value(key, e2::Json::array()))
        if (x.is_string() && x.get<std::string>() == v) return true;
      return false;
    };
    for (const e2::Json& q : quests) {
      const std::string id = q.value("id", ""), kind = q.value("kind", "");
      int have = 0, total = 0;
      if (kind == "kill") total = 1, have = has("questKills", q.value("targetName", "")) ? 1 : 0;
      else if (kind == "recruit") total = 1, have = has("flags", "recruited:" + q.value("targetName", "")) ? 1 : 0;
      else
        for (const e2::Json& p : q.value("pickups", e2::Json::array())) ++total, have += has("questItems", id + ":" + p.value("id", "")) ? 1 : 0;
      std::string status = has("questsDone", id) ? "done" : !has("questsActive", id) ? "available" : have >= total ? "ready" : "active";
      if (status == "done") done.push_back({&q, status, have, total});
      else if (status == "active" || status == "ready") active.push_back({&q, status, have, total});
    }
    const ImVec2 head = ImGui::GetCursorScreenPos();
    kicker(std::format("Estalagem · {}/{} concluídas", done.size(), quests.size()), 12.0f);
    title("Registro de missões", 24.0f);
    ImGui::SetCursorScreenPos({head.x + inner - 36, head.y});
    if (closeButton("##questclose")) questLog_ = false;
    ImGui::SetCursorScreenPos({head.x, head.y + 64});
    const std::pair<const char*, std::vector<Entry>*> sections[2] = {{"Missões ativas", &active}, {"Missões concluídas", &done}};
    const char* empties[2] = {"Nenhuma missão ativa. Fale com o pessoal da Estalagem.", "Nenhuma missão concluída ainda."};
    for (int s = 0; s < 2; ++s) {
      kicker(std::format("{} · {}", sections[s].first, sections[s].second->size()), 14.0f);
      if (sections[s].second->empty()) {
        const ImVec2 a = ImGui::GetCursorScreenPos();
        slot(a, {a.x + inner, a.y + 44});
        ImGui::GetWindowDrawList()->AddText(uiFont(UiFont::Ui), 14.0f, {a.x + 12, a.y + 14}, kMuted, empties[s]);
        ImGui::Dummy({inner, 52});
      }
      for (const Entry& e : *sections[s].second) {
        const e2::Json& q = *e.q;
        const ImVec2 a = ImGui::GetCursorScreenPos();
        const float h = e.status == "done" ? 64 : 84;
        slot(a, {a.x + inner, a.y + h});
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* ui = uiFont(UiFont::Ui);
        if (e.status == "done") iconCheck(dl, {a.x + 20, a.y + 22}, 16, kAccent);
        else dl->AddCircleFilled({a.x + 20, a.y + 22}, 4, kAccent);
        emberTitle(dl, {a.x + 36, a.y + 10}, 18.0f, q.value("title", "").c_str());
        const char* state = e.status == "done" ? "Concluída" : e.status == "ready" ? "Entregar" : "Em andamento";
        const float sw = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, state).x;
        dl->AddText(ui, 12.0f, {a.x + inner - 12 - sw, a.y + 14}, kMuted, state);
        const std::string kind = q.value("kind", "");
        const std::string what = (kind == "kill" ? "Abater " + q.value("targetName", "") : std::format("Recolher {} itens", e.total)) + " · " + q.value("place", "");
        dl->AddText(ui, 14.0f, {a.x + 36, a.y + 36}, kMuted, what.c_str(), nullptr, inner - 48);
        if (e.status != "done") {
          std::string line = kind == "kill" ? (e.have >= e.total ? "Alvo abatido — entregar na Estalagem" : "Alvo ainda vivo") : std::format("Encontrados {} / {}", e.have, e.total);
          line += std::format(" · Recompensa {} Gold", q.value("reward", 0));
          for (const e2::Json& p : q.value("rewardPotions", e2::Json::array()))
            for (const e2::Json& pd : world_.tables()["potions"])
              if (pd["id"] == p) line += " + " + pd["name"].get<std::string>();
          dl->AddText(ui, 12.0f, {a.x + 36, a.y + 58}, kMuted, line.c_str());
        }
        ImGui::Dummy({inner, h + 8});
      }
      ImGui::Dummy({1, 8});
    }
    endPanel();
  }

  // The Party menu: Grupo (leader, starting formation, affinities) and Skills (road training).
  void drawParty(ImVec2 size) {
    if (veil(size, "##partyveil")) {
      party_ = false;
      return;
    }
    const std::vector<std::string> heroes = partyHeroes();
    beginPanel("##partymenu", size, 768, size.y * 0.82f);
    const float inner = 728;
    const ImVec2 head = ImGui::GetCursorScreenPos();
    kicker(std::format("Grupo · {} {} · Líder: {}", heroes.size(), heroes.size() == 1 ? "membro" : "membros", leader()), 12.0f);
    title("Party", 24.0f);
    ImGui::SetCursorScreenPos({head.x + inner - 36, head.y});
    if (closeButton("##partyclose")) party_ = false;
    ImGui::SetCursorScreenPos({head.x, head.y + 64});
    {
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (emberBigButton("##tabgroup", "GRUPO", at, {120, 32}, partyTab_ == 0, 12.0f)) partyTab_ = 0;
      if (emberBigButton("##tabskills", "SKILLS", {at.x + 128, at.y}, {120, 32}, partyTab_ == 1, 12.0f)) partyTab_ = 1;
      ImGui::SetCursorScreenPos({at.x, at.y + 44});
    }
    ImGui::BeginChild("##partybody", {inner, std::min(size.y * 0.82f - 200, 640.0f)}, 0, ImGuiWindowFlags_NoBackground);
    if (partyTab_ == 1) drawSkillsTab(heroes, inner - 16);
    else drawGroupTab(heroes, inner - 16);
    ImGui::EndChild();
    endPanel();
  }

  void drawGroupTab(const std::vector<std::string>& heroes, float inner) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    // Líder do grupo
    kicker("Líder do grupo", 14.0f);
    {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      slot(a, {a.x + inner, a.y + 120});
      dl->AddText(ui, 14.0f, {a.x + 12, a.y + 12}, kMuted, "O líder anda pelo mapa e pelas áreas livres.");
      float x = a.x + 12;
      for (const std::string& hero : heroes) {
        ImGui::SetCursorScreenPos({x, a.y + 40});
        ImGui::PushID(hero.c_str());
        const bool chosen = leader() == hero;
        if (ImGui::InvisibleButton("##leader", {64, 72})) {
          save_["partyLeader"] = hero;
          write();
        }
        ImGui::PopID();
        emberSlot(dl, {x, a.y + 40}, {x + 64, a.y + 112}, chosen);
        if (chosen) dl->AddRectFilled({x, a.y + 40}, {x + 3, a.y + 112}, IM_COL32(217, 98, 28, 255));
        portrait(dl, hero, {x + 12, a.y + 44}, {x + 52, a.y + 92});
        const ImVec2 ts = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, hero.c_str());
        dl->AddText(ui, 12.0f, {x + 32 - ts.x / 2, a.y + 94}, chosen ? kAccent : kFg, hero.c_str());
        x += 72;
      }
      ImGui::SetCursorScreenPos({a.x, a.y + 128});
    }
    // Formação inicial: the radius-2 board (rows 3-4-5-4-3), two clicks swap two cells.
    kicker("Formação inicial", 14.0f);
    {
      if (formation_.empty()) {
        std::vector<std::string> saved;
        for (const e2::Json& v : save_.value("partyFormation", e2::Json::array())) saved.push_back(v.is_string() ? v.get<std::string>() : "");
        formation_.assign(19, "");
        for (size_t i = 0; i < saved.size() && i < 19; ++i)
          if (std::find(heroes.begin(), heroes.end(), saved[i]) != heroes.end()) formation_[i] = saved[i];
        for (const std::string& h : heroes)
          if (std::find(formation_.begin(), formation_.end(), h) == formation_.end())
            if (auto free = std::find(formation_.begin(), formation_.end(), ""); free != formation_.end()) *free = h;
        formationPick_ = -1;
      }
      const ImVec2 a = ImGui::GetCursorScreenPos();
      const float boardH = 24 + 5 * 76 + 24;
      slot(a, {a.x + inner, a.y + 72 + 52 + boardH});
      dl->AddText(ui, 14.0f, {a.x + 12, a.y + 12}, kMuted,
                  "A linha de frente começa nas posições mais próximas do inimigo; a retaguarda, nas mais distantes. Clique em duas casas para "
                  "trocá-las de lugar (vale a casa vazia). Mapas com aberturas perigosas ou posições especiais preservam sua formação própria.",
                  nullptr, inner - 24);
      if (emberBigButton("##saveformation", "SALVAR FORMAÇÃO", {a.x + 12, a.y + 72}, {180, 44}, true, 12.0f)) {
        // cleanPartyFormation: heroes once each, empty cells kept up to the board's size, trailing empties dropped.
        e2::Json out = e2::Json::array();
        std::set<std::string> seen;
        for (const std::string& v : formation_) {
          if (v.empty()) out.push_back("");
          else if (seen.insert(v).second) out.push_back(v);
        }
        while (!out.empty() && out.back() == "") out.erase(out.size() - 1);
        save_["partyFormation"] = out;
        const bool ok = campaign_.saveActive(save_) && campaign_.activeSave().value("partyFormation", e2::Json::array()) == out;
        formationSaved_ = ok ? "saved" : "failed";
      }
      if (formationSaved_ == "saved") dl->AddText(ui, 14.0f, {a.x + 204, a.y + 86}, kAccent, "✓ Formação salva — conferida no arquivo do save.");
      if (formationSaved_ == "failed") dl->AddText(ui, 14.0f, {a.x + 204, a.y + 86}, kDanger, "Não foi possível salvar a formação. Tente de novo.");
      const float cx = a.x + inner / 2;
      float y = a.y + 124;
      emberKicker(dl, {cx - 36, y}, 12.0f, "INIMIGO");
      {
        const float tx = cx + 34, ty = y + 2;
        dl->AddTriangleFilled({tx, ty + 9}, {tx + 9, ty + 9}, {tx + 4.5f, ty}, IM_COL32(223, 191, 142, 255));
      }
      y += 20;
      const int rows[5][2] = {{0, 3}, {3, 4}, {7, 5}, {12, 4}, {16, 3}};
      const char* labels[5] = {"FRENTE", "", "MEIO", "", "RETAGUARDA"};
      for (int r = 0; r < 5; ++r) {
        const int start = rows[r][0], n = rows[r][1];
        const float rowW = n * 88.0f;
        float x = cx - rowW / 2;
        if (*labels[r]) emberKicker(dl, {cx - 5 * 44.0f - 100, y + 40}, 12.0f, labels[r]);
        for (int k = 0; k < n; ++k) {
          const int cellIndex = start + k;
          const ImVec2 c{x + 44, y + 48};
          ImGui::SetCursorScreenPos({x + 2, y});
          ImGui::PushID(cellIndex);
          if (ImGui::InvisibleButton("##cell", {84, 96})) {
            if (formationPick_ < 0) formationPick_ = cellIndex;
            else {
              const int from = formationPick_;
              formationPick_ = -1;
              if (from != cellIndex && !(formation_[static_cast<size_t>(from)].empty() && formation_[static_cast<size_t>(cellIndex)].empty())) {
                std::swap(formation_[static_cast<size_t>(from)], formation_[static_cast<size_t>(cellIndex)]);
                formationSaved_.clear();
              }
            }
          }
          ImGui::PopID();
          auto hexPts = [&](float w, float h, ImVec2* p) {
            p[0] = {c.x, c.y - h / 2}, p[1] = {c.x + w / 2, c.y - h / 4}, p[2] = {c.x + w / 2, c.y + h / 4};
            p[3] = {c.x, c.y + h / 2}, p[4] = {c.x - w / 2, c.y + h / 4}, p[5] = {c.x - w / 2, c.y - h / 4};
          };
          ImVec2 outer[6], innerP[6];
          hexPts(84, 96, outer), hexPts(80, 92, innerP);
          const std::string& hero = formation_[static_cast<size_t>(cellIndex)];
          dl->AddConvexPolyFilled(outer, 6, formationPick_ == cellIndex ? IM_COL32(217, 98, 28, 255) : hero.empty() ? IM_COL32(190, 150, 95, 64) : IM_COL32(190, 150, 95, 115));
          dl->AddConvexPolyFilled(innerP, 6, IM_COL32(9, 9, 9, 255));
          if (hero.empty()) {
            const ImVec2 t1 = ui->CalcTextSizeA(11.0f, FLT_MAX, 0, "vazia");
            dl->AddText(ui, 11.0f, {c.x - t1.x / 2, c.y - 12}, kMuted, "vazia");
          } else {
            portrait(dl, hero, {c.x - 18, c.y - 34}, {c.x + 18, c.y + 6});
            const ImVec2 t1 = ui->CalcTextSizeA(11.0f, FLT_MAX, 0, hero.c_str());
            dl->AddText(ui, 11.0f, {c.x - t1.x / 2, c.y + 8}, formationPick_ == cellIndex ? IM_COL32(255, 224, 180, 255) : IM_COL32(226, 194, 148, 255), hero.c_str());
          }
          const std::string n1 = std::to_string(cellIndex + 1);
          const ImVec2 t2 = ui->CalcTextSizeA(11.0f, FLT_MAX, 0, n1.c_str());
          dl->AddText(ui, 11.0f, {c.x - t2.x / 2, c.y + 22}, kMuted, n1.c_str());
          x += 88;
        }
        y += 76;
      }
      ImGui::SetCursorScreenPos({a.x, a.y + 72 + 52 + boardH + 8});
    }
    // Afinidades
    kicker("Afinidades", 14.0f);
    {
      auto score = [&](const std::string& x, const std::string& y) {
        const std::string pair = x < y ? x + "|" + y : y + "|" + x;
        return num(save_.value("affinityScores", e2::Json::object()), pair, 0);
      };
      auto grade = [](double p) { return p >= 100 ? "Alta" : p >= 80 ? "Boa" : p >= 50 ? "Média" : p >= 25 ? "Regular" : "Ruim"; };
      auto bonus = [](double p) { return p >= 90 ? 8 : p >= 50 ? 5 : p >= 25 ? 2 : 0; };
      const float cardW = (inner - 8) / 2;
      int n = 0;
      float rowTop = ImGui::GetCursorScreenPos().y, rowH = 0;
      const float x0 = ImGui::GetCursorScreenPos().x;
      for (const std::string& h : heroes) {
        const float x = x0 + (n % 2) * (cardW + 8);
        if (n % 2 == 0 && n) rowTop += rowH + 8, rowH = 0;
        const int others = static_cast<int>(heroes.size()) - 1;
        const float h1 = 40 + std::max(1, others) * 56.0f;
        slot({x, rowTop}, {x + cardW, rowTop + h1});
        emberTitle(dl, {x + 12, rowTop + 10}, 18.0f, h.c_str());
        float y = rowTop + 40;
        for (const std::string& o : heroes) {
          if (o == h) continue;
          const double p = score(h, o);
          dl->AddText(ui, 14.0f, {x + 12, y}, kFg, o.c_str());
          const std::string right = std::format("{} · {}/100", grade(p), std::format("{}", std::round(p * 10) / 10));
          const float rw = ui->CalcTextSizeA(14.0f, FLT_MAX, 0, right.c_str()).x;
          dl->AddText(ui, 14.0f, {x + cardW - 12 - rw, y}, kMuted, right.c_str());
          dl->AddRectFilled({x + 12, y + 22}, {x + cardW - 12, y + 28}, kBorder, 3.0f);
          dl->AddRectFilled({x + 12, y + 22}, {x + 12 + (cardW - 24) * static_cast<float>(std::clamp(p, 0.0, 100.0) / 100.0), y + 28}, kAccent, 3.0f);
          const std::string help = std::format("Bônus por adjacência: +{}% ⓘ{}", bonus(p), p >= 80 ? " · Skill de dupla desbloqueada" : "");
          dl->AddText(ui, 12.0f, {x + 12, y + 32}, kMuted, help.c_str());
          y += 56;
        }
        if (others < 1) dl->AddText(ui, 14.0f, {x + 12, y}, kMuted, "As afinidades aparecerão quando outro personagem entrar no grupo.", nullptr, cardW - 24);
        rowH = std::max(rowH, h1);
        ++n;
      }
      ImGui::SetCursorScreenPos({x0, rowTop + rowH + 8});
      ImGui::Dummy({inner, 1});
      text("25: +2% · 50: +5% · 80: skill de dupla · 90: +8% · 100 nas três relações: Ultimate de trio", 12.0f, kMuted, UiFont::Ui, inner);
    }
  }

  void drawSkillsTab(const std::vector<std::string>& heroes, float inner) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const e2::Json& t = world_.tables();
    const double stepHours = t.value("travelTrainingHours", 12.0), gain = t.value("skillGain", 0.1), cap = t.value("skillCap", 100.0);
    // Party skill order: weapon skills, then Poison Resistance, then the other resistances and Healing as listed.
    std::vector<std::string> order;
    for (const e2::Json& id : t["skillIds"])
      if (id.get<std::string>().ends_with("Weapon")) order.push_back(id.get<std::string>());
    order.push_back("poisonResistance");
    for (const e2::Json& id : t["skillIds"])
      if (!id.get<std::string>().ends_with("Weapon") && id.get<std::string>() != "poisonResistance") order.push_back(id.get<std::string>());
    const float cardW = (inner - 8) / 2;
    int n = 0;
    float rowTop = ImGui::GetCursorScreenPos().y, rowH = 0;
    const float x0 = ImGui::GetCursorScreenPos().x;
    for (const std::string& hero : heroes) {
      const float x = x0 + (n % 2) * (cardW + 8);
      if (n % 2 == 0 && n) rowTop += rowH + 8, rowH = 0;
      const std::string training = save_.value("travelTraining", e2::Json::object()).value(hero, "");
      std::vector<std::string> shown;
      for (const std::string& id : order) {
        if (id.ends_with("Weapon") && !heroUsesWeapon(hero, id.substr(0, id.size() - 6))) continue;
        shown.push_back(id);
      }
      const float h = 36 + 22 + static_cast<float>(shown.size()) * 34 + 12;
      slot({x, rowTop}, {x + cardW, rowTop + h});
      emberTitle(dl, {x + 12, rowTop + 10}, 18.0f, hero.c_str());
      std::string line = "Treino na estrada: nenhum — toque em “Treinar” numa skill.";
      if (!training.empty()) {
        const double banked = std::fmod(num(save_.value("travelTrainingHours", e2::Json::object()), hero, 0), stepHours);
        const int left = std::max(1, static_cast<int>(std::ceil(stepHours - banked)));
        line = std::format("Treino na estrada: {} · próximo +0,1 em {}h de viagem", t["skillNames"].value(training, training), left);
      }
      dl->AddText(ui, 12.0f, {x + 12, rowTop + 36}, training.empty() ? kMuted : kAccent, line.c_str(), nullptr, cardW - 24);
      float y = rowTop + 58;
      for (const std::string& id : shown) {
        const double points = num(save_.value("heroSkills", e2::Json::object()).value(hero, e2::Json::object()), id, 0);
        const bool on = training == id;
        std::string name = t["skillNames"].value(id, id);
        if (id == "healing") name += std::format(" · +{}% healing", std::round(points * 100) / 100);
        dl->AddText(ui, 14.0f, {x + 12, y}, kFg, name.c_str());
        const std::string value = std::format("{}/{}", std::round(points * 100) / 100, cap);
        const float vw = ui->CalcTextSizeA(14.0f, FLT_MAX, 0, value.c_str()).x;
        dl->AddText(ui, 14.0f, {x + cardW - 12 - vw, y}, kMuted, value.c_str());
        ImGui::PushID((hero + id).c_str());
        if (emberBigButton("##train", on ? "TREINANDO" : "TREINAR", {x + cardW - 12 - vw - 8 - 96, y - 4}, {96, 24}, on, 10.0f)) {
          e2::Json& tr = save_["travelTraining"];
          if (!tr.is_object()) tr = e2::Json::object();
          if (on) tr.erase(hero);
          else tr[hero] = id;
          if (!save_["travelTrainingHours"].is_object()) save_["travelTrainingHours"] = e2::Json::object();
          save_["travelTrainingHours"][hero] = 0;
          markTrainingHint();
          write();
        }
        ImGui::PopID();
        dl->AddRectFilled({x + 12, y + 22}, {x + cardW - 12, y + 28}, kBorder, 3.0f);
        dl->AddRectFilled({x + 12, y + 22}, {x + 12 + (cardW - 24) * static_cast<float>(std::clamp(points / cap, 0.0, 1.0)), y + 28}, kAccent, 3.0f);
        y += 34;
      }
      rowH = std::max(rowH, h);
      ++n;
    }
    ImGui::SetCursorScreenPos({x0, rowTop + rowH + 8});
    ImGui::Dummy({inner, 1});
    text(std::format("Treino na estrada: cada personagem pode treinar uma skill por vez enquanto o grupo viaja — +0,1 a cada {}h de viagem. Trocar de skill "
                     "recomeça a contagem.",
                     static_cast<int>(stepHours)),
         12.0f, kMuted, UiFont::Ui, inner);
    (void)gain;
  }

  // weaponSkills.ts weaponTypesForClass: which weapon skills a hero's class trains (from the exported weapon list).
  bool heroUsesWeapon(const std::string& hero, const std::string& type) const {
    const e2::Json& types = world_.tables().contains("classWeaponTypes") ? world_.tables()["classWeaponTypes"] : e2::Json();
    std::string cls = world_.tables()["baseClass"].value(hero, "");
    if (save_.contains("promotions") && save_["promotions"].contains(hero) && save_["promotions"][hero].is_string()) cls = save_["promotions"][hero].get<std::string>();
    if (!types.is_object() || !types.contains(cls)) return true;
    for (const e2::Json& t : types[cls])
      if (t == type) return true;
    return false;
  }

  void markTrainingHint() {
    e2::Json& flags = save_["flags"];
    if (!flags.is_array()) flags = e2::Json::array();
    for (const e2::Json& f : flags)
      if (f == "hint:travel-training") return;
    flags.push_back("hint:travel-training");
  }

  // One-time tip once A Ponte de Pedra is cleared: Party → Skills.
  void drawTrainingHint(ImVec2 size) {
    // Under every other panel (Engine2: z-30 against their z-40 veils).
    if (!completed("thebridge") || party_ || event_ || open_ || questLog_ || options_ || !save_.value("seenOverworldIntro", false)) return;
    for (const e2::Json& f : save_.value("flags", e2::Json::array()))
      if (f == "hint:travel-training") return;
    ImGui::SetNextWindowPos({size.x / 2, 144}, ImGuiCond_Always, {0.5f, 0.0f});
    ImGui::SetNextWindowSize({std::min(480.0f, size.x - 32), 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 16});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##traininghint", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
    emberPanel(ImGui::GetWindowDrawList(), a, b);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
    title("Dica · Treino na estrada", 18.0f);
    text("Abra o menu Party e explore a aba Skills. Toque em “Treinar” numa skill de cada personagem: ela sobe sozinha enquanto o grupo viaja, +0,1 a cada 12h de "
         "estrada. Uma skill por vez — nada treina até você escolher.",
         14.0f, kMuted, UiFont::Ui, std::min(448.0f, size.x - 64));
    ImGui::Dummy({1, 8});
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (emberBigButton("##hintok", "ENTENDI", at, {120, 32}, false, 11.0f)) markTrainingHint(), write();
    if (emberBigButton("##hintopen", "ABRIR PARTY · SKILLS", {at.x + 128, at.y}, {200, 32}, true, 11.0f)) {
      markTrainingHint();
      write();
      party_ = true, partyTab_ = 1, formation_.clear(), formationSaved_.clear();
    }
    ImGui::GetWindowDrawList()->ChannelsMerge();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }

  // GameApp OverworldIntroScreen: the movement and hunger explainer, once per campaign.
  void drawIntro(ImVec2 size) {
    ImGui::SetCursorScreenPos({0, 0});
    ImGui::InvisibleButton("##introveil", size);
    ImGui::GetWindowDrawList()->AddRectFilled({0, 0}, size, IM_COL32(17, 17, 17, 230));
    beginPanel("##intro", size, 448, size.y * 0.9f);
    kicker("Sistema", 12.0f);
    title("Movimento e fome", 24.0f);
    const float w = 408;
    text("Clique no personagem no mapa para ver os hexágonos que o grupo pode alcançar e escolher para onde ir. Cada passo custa um dia.", 14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 6});
    text("Cada herói tem uma barra de saciedade. Ela desce ao longo da marcha pelo mapa e um pouco a cada ação em combate. Rações (compradas na Estalagem ou "
         "achadas em batalha) e refeições na Estalagem enchem essa barra de volta.",
         14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 6});
    text("Se ela chegar a zero e o grupo continuar sem comer, começa o status de Fome: uma penalidade de −10% em todos os atributos, que piora a cada dia faminto "
         "até um teto de −90%.",
         14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 6});
    text("Fique de olho na barra e mantenha rações na mochila antes de partir.", 14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 10});
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (emberBigButton("##introok", "ENTENDI", at, {w, 44}, true, 13.0f)) {
      save_["seenOverworldIntro"] = true;
      save_["pendingMission"] = nullptr;
      write();
    }
    endPanel();
  }

  // Opções (OptionsDialog): Jogo (Salvar, Carregar, Voltar à tela inicial) and Gráficos (Tela cheia).
  void drawOptions(ImVec2 size) {
    if (veil(size, "##optionsveil")) {
      options_ = false;
      return;
    }
    beginPanel("##options", size, 704, size.y * 0.94f);
    const float inner = 664;
    const ImVec2 head = ImGui::GetCursorScreenPos();
    title("Opções", 24.0f);
    ImGui::SetCursorScreenPos({head.x + inner - 110, head.y});
    if (emberBigButton("##optclose", "FECHAR", {head.x + inner - 110, head.y}, {110, 32}, false, 11.0f)) options_ = false;
    ImGui::SetCursorScreenPos({head.x, head.y + 48});
    const float colW = (inner - 8) / 2;
    const ImVec2 a = ImGui::GetCursorScreenPos();
    slot(a, {a.x + colW, a.y + 170});
    emberTitle(ImGui::GetWindowDrawList(), {a.x + 12, a.y + 8}, 16.0f, "Jogo");
    if (emberBigButton("##optsave", "SALVAR JOGO", {a.x + 12, a.y + 36}, {colW - 24, 36}, false, 11.0f)) {
      options_ = false;
      slots_.emplace();
      slots_->open(SlotScreen::Mode::Save, true);
    }
    if (emberBigButton("##optload", "CARREGAR JOGO", {a.x + 12, a.y + 80}, {colW - 24, 36}, false, 11.0f)) {
      options_ = false;
      slots_.emplace();
      slots_->open(SlotScreen::Mode::Load, true);
    }
    if (emberBigButton("##opttitle", "VOLTAR À TELA INICIAL", {a.x + 12, a.y + 124}, {colW - 24, 36}, false, 11.0f) && callbacks_.title) callbacks_.title();
    const ImVec2 g{a.x + colW + 8, a.y};
    slot(g, {g.x + colW, g.y + 82});
    emberTitle(ImGui::GetWindowDrawList(), {g.x + 12, g.y + 8}, 16.0f, "Gráficos");
    if (emberBigButton("##optfull", "TELA CHEIA", {g.x + 12, g.y + 36}, {colW - 24, 32}, false, 11.0f) && callbacks_.fullscreen) callbacks_.fullscreen();
    ImGui::SetCursorScreenPos({a.x, a.y + 178});
    ImGui::Dummy({inner, 1});
    endPanel();
  }

  void portrait(ImDrawList* dl, const std::string& hero, ImVec2 a, ImVec2 b) {
    const e2::Json& p = world_.tables()["portraits"].value(hero, e2::Json::object());
    std::string src = p.value("src", "");
    if (const size_t q = src.find('?'); q != std::string::npos) src.resize(q);
    if (!src.empty() && src[0] == '/') src.erase(0, 1);
    if (const UiImage* img = uiImage(paths_.engine2Public / std::filesystem::path(reinterpret_cast<const char8_t*>(src.c_str())))) imageCover(dl, *img, a, b);
  }

  CampaignPaths paths_;
  e2::Campaign& campaign_;
  const e2::Overworld& world_;
  Callbacks callbacks_;
  e2::Json save_;
  std::string notice_, hint_;
  float hintAt_ = -10, now_ = 0;
  std::optional<e2::OverworldEvent> event_;
  std::optional<std::string> open_;  // the location whose mission list is open
  std::optional<MissionEntry> briefing_;
  std::optional<SlotScreen> slots_;
  bool movementOpen_ = false, questLog_ = false, party_ = false, options_ = false;
  int partyTab_ = 0;
  std::vector<std::string> formation_;
  int formationPick_ = -1;
  std::string formationSaved_;
  std::string flashId_;
  float flashAt_ = -10;
  int zoom_ = 3;
  float scrollX_ = 0, scrollY_ = 0, centreX_ = 0.5f, centreY_ = 0.5f;
  bool centred_ = false, recentre_ = false;
  struct Drag {
    float x, y, sx, sy;
  };
  std::optional<Drag> drag_;
  bool dragMoved_ = false;
  float partyX_ = -1, partyY_ = 0, fromX_ = 0, fromY_ = 0, targetX_ = -1, targetY_ = -1, moveAt_ = -10;
  int prevCol_ = 0;
  bool facingLeft_ = false;
  int frames_ = 0;
};

}  // namespace ember
