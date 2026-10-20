#pragma once

// Engine2's Modo teste (the title's "Modo teste" link: game.html?start=test), copied from src/game/GameApp.tsx
// (TestMenuScreen, MapChoiceScreen, CampaignScreen, the test branches of goToMap / openMission / startBattle) and
// src/game/WorldMapScreen.tsx (the classic map and its LocationPanel).
//
//   O que abrir?   Debug: any campaign mission, nothing locked, nothing written to the real save.
//                  Map Editor (MapEditorStage.h) and Dev Controls (the renderer switches, DevControls.h).
//   Debug          asks once per test session how to travel: Classic Tactical (this file's map: click any place, go
//                  straight to its missions) or RPG Map (OverworldStage with the session's own save: walk anywhere,
//                  jump to any pin). A mission opens its briefing and is fought as Engine2's test battle: all six
//                  heroes at level 15 with a new save's gear (tools/scripts/engine2/export_test_missions.sh).
//
// The test session (TestSession) lives in main.cpp from the "Modo teste" click until the player leaves to the title:
// its own save (GameApp freshTestOverworld: a new save, every hero at DEFAULT_TEST_LEVEL, TEST_EMBER Gold), the
// chosen travel style. Nothing of it reaches the save bank.
//
// Not ported: the cutscene films (Inn arrival, wisp forest), sound (the map's mute / volume buttons are left out: the
// game plays no sound yet), the Inn from the classic map (it opens from the RPG map).

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "DevGfx.h"
#include "EmberMenus.h"
#include "GameMusic.h"
#include "Stage.h"
#include "render/Camera.h"
#include "render/DebugUi.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "render/UiArt.h"
#include "world/Campaign.h"
#include "world/Overworld.h"

namespace ember {

// GameApp DEFAULT_TEST_LEVEL ("the one place to change the test-mode level") and TEST_EMBER.
inline constexpr int kDefaultTestLevel = 30;  // was 15 (the user, 2026-10-19: "subir o level dos testes para 30")
inline constexpr long long kTestEmber = 900000;

struct TestSession {
  e2::Json save;        // GameApp testOverworld (freshTestOverworld on entry)
  std::string mapMode;  // "" until Debug asks: "classic" or "rpg" (session-only, never persisted)
};

// GameApp freshTestOverworld: a brand-new save with every hero at DEFAULT_TEST_LEVEL; testEmber (900000) is the
// session's Gold.
inline TestSession freshTestSession(const e2::Campaign& campaign) {
  TestSession s;
  s.save = campaign.emptySave();
  e2::Json levels = e2::Json::object();
  for (const char* h : {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"}) levels[h] = kDefaultTestLevel;
  s.save["levels"] = levels;
  s.save["ember"] = kTestEmber;
  return s;
}

class TestModeStage : public Stage {
 public:
  enum class Screen { Menu, MapChoice, WorldMap, Scenarios, Briefing, DevControls };
  struct Callbacks {
    std::function<void()> title;                    // Voltar from the menu: the title screen (the session ends)
    std::function<void()> rpgMap;                   // the RPG map with the session's save
    std::function<void(const MissionEntry&)> play;  // a mission's test battle
    std::function<void()> editor;                   // Map Editor
    std::function<void()> spells;                   // Teste de Magias (ours, user 2026-10-16): the spells test battle
    std::function<void()> fullscreen;               // Opções: Tela cheia
  };

  // `devGfx`: Dev Controls' switches (DevGfx.h), held by the caller for every map it opens; saved on every change.
  TestModeStage(CampaignPaths paths, e2::Campaign& campaign, const e2::Overworld& world, TestSession& session, DevGfx& devGfx,
                Callbacks callbacks, Screen screen = Screen::Menu, std::string notice = {})
      : paths_(std::move(paths)), campaign_(campaign), world_(world), session_(session), devGfx_(devGfx), callbacks_(std::move(callbacks)),
        screen_(screen), notice_(std::move(notice)) {}

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

  // GameApp's music chain: the classic world map plays its piece, a briefing its mission's track; the test menus none.
  Music music() const override {
    if (briefing_) return music::forMission(*briefing_);
    if (screen_ == Screen::WorldMap) return music::theme("worldMap");
    return {};
  }

  void event(const SDL_Event& e) override {
    if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || e.key.key != SDLK_ESCAPE) return;
    if (options_) options_ = false;
    else if (open_) open_.reset();
    else if (screen_ == Screen::Briefing) screen_ = briefingFrom_;
    else if (screen_ == Screen::Scenarios) screen_ = Screen::WorldMap;
    else if (screen_ == Screen::DevControls) screen_ = Screen::Menu;
    else if (screen_ == Screen::WorldMap) options_ = true;  // OptionsButton escapeKey
  }

  void drawPanel() override {
    const ImGuiIO& io = ImGui::GetIO();
    now_ = static_cast<float>(ImGui::GetTime());
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##testmode", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    switch (screen_) {
      case Screen::Menu: drawMenu(io.DisplaySize); break;
      case Screen::MapChoice: drawMapChoice(io.DisplaySize); break;
      case Screen::WorldMap: drawWorldMap(io.DisplaySize); break;
      case Screen::Scenarios: drawScenarios(io.DisplaySize); break;
      case Screen::DevControls: drawDevControls(io.DisplaySize); break;
      case Screen::Briefing:
        if (briefing_) {
          const int r = drawBriefing(io.DisplaySize, kRem, *briefing_, paths_.engine2Public);
          if (r < 0) screen_ = briefingFrom_;
          else if (r > 0 && callbacks_.play) callbacks_.play(*briefing_);
        }
        break;
    }
    if (!notice_.empty()) drawNotice(io.DisplaySize);
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  // Captures: an open location panel on the classic map.
  void previewLocation(const std::string& id) { open_ = id; }

  // GameApp goToMap in test mode: the travel style chosen once per session, else the chooser.
  void goToMap() {
    if (session_.mapMode.empty()) screen_ = Screen::MapChoice;
    else if (session_.mapMode == "classic") screen_ = Screen::WorldMap;
    else if (callbacks_.rpgMap) callbacks_.rpgMap();
  }

 private:
  static constexpr float kRem = 16.0f;  // CSS px are canvas units (DebugUi::kUiHeight)
  static constexpr float kZoomStops[4] = {70, 90, 110, 130};
  // ember-ui @theme
  static constexpr ImU32 kBg = IM_COL32(17, 17, 17, 255), kSurface = IM_COL32(28, 28, 28, 255), kFg = IM_COL32(240, 240, 240, 255),
                         kMuted = IM_COL32(180, 180, 180, 255), kBorder = IM_COL32(74, 74, 74, 255), kAccent = IM_COL32(216, 216, 216, 255);

  static ImFont* display() { return uiFont(UiFont::Display); }
  static ImFont* sans() { return uiFont(UiFont::Ui); }

  // --- small pieces -------------------------------------------------------------------------------------------------

  // `uppercase tracking-[x]` text.
  static void caps(ImDrawList* dl, ImVec2 at, float size, ImU32 ink, const std::string& text, float spacing) {
    spacedText(dl, sans(), size, at, ink, upperUtf8(text).c_str(), spacing, false);
  }
  // Wrapped text; returns its height.
  static float wrapped(ImDrawList* dl, ImFont* font, float size, ImVec2 at, ImU32 ink, const std::string& text, float width) {
    dl->AddText(font, size, at, ink, text.c_str(), nullptr, width);
    return font->CalcTextSizeA(size, FLT_MAX, width, text.c_str()).y;
  }
  // The plain screens' back button: size-10, border border-border, ChevronLeft.
  bool backButton(const char* id, ImVec2 at, ImU32 border = IM_COL32(74, 74, 74, 255), ImU32 fill = 0) {
    ImGui::SetCursorScreenPos(at);
    const bool clicked = ImGui::InvisibleButton(id, {40, 40});
    const bool hover = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (fill) dl->AddRectFilled(at, {at.x + 40, at.y + 40}, fill);
    dl->AddRect(at, {at.x + 40, at.y + 40}, hover ? kAccent : border);
    iconChevronLeft(dl, {at.x + 20, at.y + 20}, 20, kFg);
    return clicked;
  }
  // A Tailwind card button: border, fill, hover border-accent; true when clicked.
  bool cardButton(const char* id, ImVec2 a, ImVec2 size, ImU32 fill, ImU32 border, bool disabled = false) {
    ImGui::SetCursorScreenPos(a);
    ImGui::BeginDisabled(disabled);
    const bool clicked = ImGui::InvisibleButton(id, size);
    ImGui::EndDisabled();
    const bool hover = !disabled && ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 b{a.x + size.x, a.y + size.y};
    if (fill) dl->AddRectFilled(a, b, fill);
    dl->AddRect(a, b, hover ? kAccent : border);
    return clicked && !disabled;
  }

  void drawNotice(ImVec2 size) {
    const float t = 12.0f;
    const ImVec2 ts = sans()->CalcTextSizeA(t, FLT_MAX, 0.0f, notice_.c_str());
    ImGui::SetCursorScreenPos({(size.x - ts.x - 24.0f) * 0.5f, 96.0f});
    if (emberTextPlate("##notice", {ts.x + 24.0f, ts.y + 12.0f}, notice_.c_str(), t)) notice_.clear();
  }

  // --- TestMenuScreen -----------------------------------------------------------------------------------------------

  void drawMenu(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, kBg);
    // header: px-4 pt-4 pb-4, border-b
    const float headerH = 16 + 40 + 16;
    dl->AddLine({0, headerH}, {size.x, headerH}, kBorder);
    if (backButton("##menuback", {16, 16}) && callbacks_.title) callbacks_.title();
    caps(dl, {16 + 40 + 12, 17}, 14.0f, kMuted, "Modo teste", 0.18f);
    dl->AddText(display(), 30.0f, {16 + 40 + 12, 17 + 20}, kFg, "O que abrir?");
    // the choices: max-w-md, centred, gap-3, p-5 (Engine2's three, then ours: Teste de Magias)
    struct Choice {
      const char* title;
      const char* text;
      std::function<void()> go;
    };
    constexpr int kChoices = 4;
    const Choice choices[kChoices] = {
        {"Debug", "Joga qualquer missão da campanha, sem travar progresso — o de sempre.", [this] { goToMap(); }},
        {"Map Editor", "Pinta terreno, posiciona spawns, testa na hora e exporta pra colar no jogo.", callbacks_.editor},
        {"Dev Controls", "Liga/desliga sombras do renderizador 3D pra comparar em combate.", [this] { screen_ = Screen::DevControls; }},
        {"Teste de Magias", "Campo de teste com todas as magias e habilidades: escolhe a versão de cada efeito e testa na hora.",
         callbacks_.spells},
    };
    const float w = std::min(448.0f, size.x - 40.0f) - 40.0f;
    const float x = (size.x - w) * 0.5f;
    float heights[kChoices], total = 0;
    for (int i = 0; i < kChoices; ++i) {
      heights[i] = 16 + 24 * 1.25f + 4 + sans()->CalcTextSizeA(14.0f, FLT_MAX, w - 40, choices[i].text).y + 16;
      total += heights[i] + (i ? 12 : 0);
    }
    float y = headerH + std::max(20.0f, (size.y - headerH - total) * 0.5f);
    for (int i = 0; i < kChoices; ++i) {
      ImGui::PushID(i);
      const bool clicked = cardButton("##choice", {x, y}, {w, heights[i]}, IM_COL32(17, 17, 17, 102), kBorder, !choices[i].go);
      ImGui::PopID();
      dl->AddText(display(), 24.0f, {x + 20, y + 16}, choices[i].go ? kFg : IM_COL32(240, 240, 240, 102), choices[i].title);
      wrapped(dl, sans(), 14.0f, {x + 20, y + 16 + 24 * 1.25f + 4}, choices[i].go ? kMuted : IM_COL32(180, 180, 180, 102), choices[i].text, w - 40);
      if (clicked && choices[i].go) choices[i].go();
      y += heights[i] + 12;
    }
  }

  // --- MapChoiceScreen ----------------------------------------------------------------------------------------------

  void drawMapChoice(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, IM_COL32(8, 10, 13, 255));
    if (const UiImage* board = uiImage(paths_.engine2Public / "game/ui/travel-board.png")) imageCover(dl, *board, {0, 0}, size, 0.5f, 0.5f);
    verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(0, 0, 0, 77)}, {0.5f, IM_COL32(0, 0, 0, 13)}, {1.0f, IM_COL32(0, 0, 0, 89)}});
    const float headerH = 16 + 40 + 16;
    dl->AddRectFilled({0, 0}, {size.x, headerH}, IM_COL32(0, 0, 0, 26));
    dl->AddLine({0, headerH}, {size.x, headerH}, IM_COL32(255, 255, 255, 26));
    if (backButton("##choiceback", {16, 16}, IM_COL32(255, 255, 255, 64), IM_COL32(0, 0, 0, 115))) screen_ = Screen::Menu;  // onBack: testMenu
    caps(dl, {16 + 40 + 12, 17}, 14.0f, IM_COL32(203, 213, 225, 255), "Mapa", 0.18f);
    dl->AddText(display(), 30.0f, {16 + 40 + 12, 17 + 20}, IM_COL32(255, 255, 255, 255), "Como quer viajar?");
    // two cards at the bottom: max-w-5xl, md:grid-cols-2, gap-3, p-8
    const float pad = size.x >= 640 ? 32.0f : 16.0f;
    const float listW = std::min(1024.0f, size.x - 2 * pad);
    const bool twoCols = size.x >= 768;
    const float cardW = twoCols ? (listW - 12) / 2 : listW;
    const char* titles[2] = {"Classic Tactical", "RPG Map"};
    const char* texts[2] = {"O mapa de sempre: escolha qualquer local desbloqueado e vá direto pra missão.",
                            "O grupo viaja hexágono por hexágono; cada passo custa um dia — suprimentos, encontros e recuperação entram em jogo."};
    float h = 0;
    for (const char* t : texts) h = std::max(h, 16 + 30.0f + 4 + sans()->CalcTextSizeA(14.0f, FLT_MAX, cardW - 40, t).y + 16);
    const float x0 = (size.x - listW) * 0.5f;
    const float bottom = size.y - 20;
    for (int i = 0; i < 2; ++i) {
      const ImVec2 a = twoCols ? ImVec2{x0 + i * (cardW + 12), bottom - h} : ImVec2{x0, bottom - h - (1 - i) * (h + 12)};
      ImGui::PushID(i);
      // rounded-lg border white/25 bg-black/45 backdrop-blur-sm
      const bool clicked = cardButton("##mode", a, {cardW, h}, IM_COL32(0, 0, 0, 115), IM_COL32(255, 255, 255, 64));
      ImGui::PopID();
      dl->AddText(display(), 24.0f, {a.x + 20, a.y + 16}, kFg, titles[i]);
      wrapped(dl, sans(), 14.0f, {a.x + 20, a.y + 16 + 30 + 4}, kMuted, texts[i], cardW - 40);
      if (clicked) {
        session_.mapMode = i == 0 ? "classic" : "rpg";
        goToMap();
      }
    }
  }

  // --- WorldMapScreen (classic) ---------------------------------------------------------------------------------------

  // The places on the map (GameApp mapVisibleLocations: test mode shows every one) and what the classic map knows of
  // them: status and missions with test = true, against the real active slot's completed list (GameApp `save`).
  const e2::Json& realSave() {
    if (!realSaveRead_) realSave_ = campaign_.activeSave(), realSaveRead_ = true;
    return realSave_;
  }
  const e2::CampaignMission* missionData(const std::string& id) const { return campaign_.data().mission(id); }

  void openMission(const std::string& id) {
    // GameApp openMission / beginMission in test mode: nothing gated, finished missions open again; the Inn's briefing says
    // "Entrar" and opens the walkable Inn (its arrival film is not ported here).
    std::string error;
    if (std::optional<MissionEntry> e = testMission(campaign_, paths_, id, error)) {
      briefing_ = std::move(e);
      briefingFrom_ = screen_;
      screen_ = Screen::Briefing;
    } else {
      notice_ = error;
    }
  }

  void drawWorldMap(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // radial-gradient(ellipse at 30% 20%, #1e1e1e 0%, #0b0b0b 70%)
    dl->AddRectFilled({0, 0}, size, IM_COL32(11, 11, 11, 255));
    for (int i = 12; i >= 1; --i) {
      const float k = static_cast<float>(i) / 12.0f;
      const int v = 11 + static_cast<int>(19 * (1 - k));
      dl->AddCircleFilled({size.x * 0.3f, size.y * 0.2f}, std::max(size.x, size.y) * 0.7f * k, IM_COL32(v, v, v, 40), 64);
    }
    const float headerH = drawMapHeader(size);
    drawMapViewport({0, headerH}, size);
    drawZoom(size);
    if (open_) drawLocationPanel(size);
    if (options_) drawOptions(size);
  }

  float drawMapHeader(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float top = 16.0f;
    ImGui::SetCursorScreenPos({16.0f, top + 6.0f});
    if (emberPlate("##back", {40, 40}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconChevronLeft(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 20, ink); }))
      screen_ = Screen::Menu;  // onBack: testMenu
    emberKicker(dl, {68.0f, top + 4.0f}, 14.0f, "MODO TESTE");
    emberTitle(dl, {68.0f, top + 22.0f}, 30.0f, "Mapa");
    ImFont* ui = sans();
    const float h = 36.0f, gap = 12.0f;
    float x = size.x - 16.0f;
    const float y = top + 6.0f;
    auto place = [&](float w) {
      x -= w;
      ImGui::SetCursorScreenPos({x, y});
      x -= gap;
    };
    const std::string gold = std::format("{} Gold", static_cast<long long>(session_.save.value("ember", kTestEmber)));
    const float goldW = 8.0f + 24.0f + 6.0f + ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, gold.c_str()).x + 8.0f;
    place(goldW);
    emberPlate("##gold", {goldW, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
      if (const UiImage* icon = uiImage(paths_.engine2Public / "game/icons/gold-coins-pile.png"))
        imageContain(d, *icon, {a.x + 8, (a.y + b.y) * 0.5f - 12}, {a.x + 32, (a.y + b.y) * 0.5f + 12});  // object-contain
      d->AddText(ui, 14.0f, {a.x + 38, (a.y + b.y) * 0.5f - 8}, kMuted, gold.c_str());
    });
    place(h);
    if (emberPlate("##options", {h, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconGear(d, {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}, 16, ink); }))
      options_ = true;
    place(h);  // WorldMapScreen's sound button
    soundButton("##sound", {h, h}, 16);
    {
      const char* label = "LISTA";
      const float w = spacedTextWidth(ui, 12.0f, label, 0.14f) + 24.0f;
      place(w);
      if (emberPlate("##list", {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) {
            spacedText(d, ui, 12.0f, {a.x + 12, (a.y + b.y) * 0.5f - 7}, ink, label, 0.14f, false);
          }))
        screen_ = Screen::Scenarios;
    }
    return top + 64.0f + 16.0f;
  }

  void drawMapViewport(ImVec2 a, ImVec2 b) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const UiImage* map = uiImage(paths_.engine2Public / "game/assets/world-map.jpg");
    const float vw = b.x - a.x, vh = b.y - a.y;
    const float imgW = map ? vw * kZoomStops[zoom_] / 100.0f : std::min(vw * 0.7f, 448.0f);
    const float imgH = map ? imgW * static_cast<float>(map->height) / static_cast<float>(map->width) : vh * 0.7f;
    const float contentW = imgW + 16.0f, contentH = imgH + 16.0f;
    // Opens centred on the first place with a mission the real save has not finished (centerLocationId).
    if (!centred_) {
      centreX_ = centreY_ = 0.5f;
      for (const e2::MapLocation& loc : world_.locations()) {
        bool pending = false;
        for (const std::string& id : loc.missionIds) pending = pending || !done(id);
        if (pending) {
          centreX_ = static_cast<float>(loc.x / 100.0), centreY_ = static_cast<float>(loc.y / 100.0);
          break;
        }
      }
      centred_ = true, recentre_ = true;
    }
    if (recentre_) scrollX_ = centreX_ * contentW - vw / 2, scrollY_ = centreY_ * contentH - vh / 2, recentre_ = false;
    ImGui::SetCursorScreenPos(a);
    ImGui::SetNextItemAllowOverlap();  // the zoom plates drawn after the viewport take their own clicks
    ImGui::InvisibleButton("##viewport", {vw, vh}, ImGuiButtonFlags_MouseButtonLeft);
    const bool over = ImGui::IsItemHovered() && !worldMapZoomBoxHovered(b);
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActivated()) drag_ = {io.MousePos.x, io.MousePos.y, scrollX_, scrollY_}, dragMoved_ = false;
    if (ImGui::IsItemActive() && drag_) {
      requestGrabCursor();  // Engine2 world-map-pan-grab: the gauntlet from the press on
      const float dx = io.MousePos.x - drag_->x, dy = io.MousePos.y - drag_->y;
      if (!dragMoved_ && (std::abs(dx) > 4 || std::abs(dy) > 4)) dragMoved_ = true;
      if (dragMoved_) scrollX_ = drag_->sx - dx, scrollY_ = drag_->sy - dy;
    }
    if (ImGui::IsItemDeactivated()) drag_.reset();
    if (over && io.MouseWheel != 0.0f) {
      if (io.KeyShift) scrollX_ -= io.MouseWheel * 100.0f;
      else scrollY_ -= io.MouseWheel * 100.0f;
    }
    scrollX_ = std::clamp(scrollX_, 0.0f, std::max(0.0f, contentW - vw));
    scrollY_ = std::clamp(scrollY_, 0.0f, std::max(0.0f, contentH - vh));
    const bool clicked = over && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !dragMoved_;
    centreX_ = (scrollX_ + vw / 2) / contentW, centreY_ = (scrollY_ + vh / 2) / contentH;

    const ImVec2 o{a.x + 8.0f - scrollX_, a.y + 8.0f - scrollY_};
    dl->PushClipRect(a, b, true);
    if (map) dl->AddImageRounded(map->texture, o, {o.x + imgW, o.y + imgH}, {0, 0}, {1, 1}, IM_COL32_WHITE, 8.0f);
    std::string clickedId;
    for (const e2::MapLocation& loc : world_.locations()) {
      const std::string st = world_.locationStatus(loc, realSave(), true);
      const auto missions = world_.visibleMissionsOf(loc, realSave(), true);
      const ImVec2 c{o.x + static_cast<float>(loc.x) / 100.0f * imgW, o.y + static_cast<float>(loc.y) / 100.0f * imgH};
      const float r = 20.0f;
      const bool hover = over && std::hypot(io.MousePos.x - c.x, io.MousePos.y - c.y) <= r;
      drawPin(dl, c, r * (hover ? 1.1f : 1.0f), st, std::any_of(missions.begin(), missions.end(), [](const e2::MapMission* m) { return m->hub; }),
              static_cast<int>(missions.size()), flashId_ == loc.id ? now_ - flashAt_ : -1.0f);
      if (hover && clicked) clickedId = loc.id;
    }
    dl->PopClipRect();
    if (!clickedId.empty())
      for (const e2::MapLocation& loc : world_.locations())
        if (loc.id == clickedId) {
          const std::string st = world_.locationStatus(loc, realSave(), true);
          const auto missions = world_.visibleMissionsOf(loc, realSave(), true);
          if (st == "locked") flashId_ = loc.id, flashAt_ = now_;
          else if (missions.size() > 1) open_ = loc.id;
          else if (!missions.empty()) openMission(missions[0]->id);
        }
  }

  // A location pin: size-10 rounded-full border-2 bg-bg/80 (locked: border-border, half opacity; done: Check; the Inn
  // open: inn-glow), a mission count badge when it holds more than one; locked-flash on a locked click.
  void drawPin(ImDrawList* dl, ImVec2 c, float r, const std::string& st, bool hub, int count, float flashAge) {
    const int alpha = st == "locked" ? 128 : 255;
    ImU32 border = st == "locked" ? kBorder : kAccent;
    if (st != "locked" && st != "done" && hub) {
      const float k = 0.5f - 0.5f * std::cos(now_ / 2.8f * 2 * std::numbers::pi_v<float>);
      border = IM_COL32(static_cast<int>(73 + (201 - 73) * k), static_cast<int>(73 + (160 - 73) * k), static_cast<int>(73 + (112 - 73) * k), 255);
      for (int i = 3; i >= 1; --i) dl->AddCircle(c, r + i * 3.0f, IM_COL32(201, 150, 88, static_cast<int>((22 + 28 * k) * 0.6f)), 32, 3.0f);
    }
    if (flashAge >= 0 && flashAge < 0.5f) {
      const float k = std::sin(flashAge / 0.5f * std::numbers::pi_v<float>);
      border = IM_COL32(214, 60, 60, 255);
      for (int i = 3; i >= 1; --i) dl->AddCircle(c, r + i * 3.0f, IM_COL32(214, 60, 60, static_cast<int>(70 * k)), 32, 3.0f);
    }
    dl->AddCircleFilled(c, r, IM_COL32(17, 17, 17, 204 * alpha / 255), 32);
    dl->AddCircle(c, r - 1, (border & ~IM_COL32_A_MASK) | (static_cast<ImU32>(((border >> IM_COL32_A_SHIFT) & 0xFF) * alpha / 255) << IM_COL32_A_SHIFT), 32, 2.0f);
    const float s = 16.0f * r / 20.0f;
    if (st == "locked") iconLock(dl, c, s, IM_COL32(180, 180, 180, alpha));
    else if (st == "done") iconCheck(dl, c, s, kAccent);
    else iconMapPin(dl, c, s, kAccent);
    if (count > 1) {
      const ImVec2 bc{c.x + r - 2, c.y - r + 2};
      dl->AddCircleFilled(bc, 8, kBg);
      dl->AddCircle(bc, 8, kBorder);
      const std::string n = std::to_string(count);
      const ImVec2 ts = sans()->CalcTextSizeA(10.0f, FLT_MAX, 0, n.c_str());
      dl->AddText(sans(), 10.0f, {bc.x - ts.x / 2, bc.y - ts.y / 2}, IM_COL32(240, 240, 240, 230), n.c_str());
    }
  }

  // Bottom right: ember-plate p-1 with Aproximar, "n/4" (counted from the wide end) and Afastar.
  void drawZoom(ImVec2 size) {
    if (!uiImage(paths_.engine2Public / "game/assets/world-map.jpg")) return;
    if (worldMapZoomBox(size, zoom_)) recentre_ = true;
  }

  bool done(const std::string& id) {
    for (const e2::Json& c : realSave().value("completed", e2::Json::array()))
      if (c.is_string() && c.get<std::string>() == id) return true;
    return false;
  }

  // A modal veil (bg-bg/45) under a panel; true when the veil itself was clicked.
  bool veil(ImVec2 size, const char* id) {
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(17, 17, 17, 115));
    ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    const bool clicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0);
    ImGui::End();
    ImGui::PopStyleColor();
    return clicked;
  }

  // LocationPanel: the place's missions as ember-slot rows (kicker "01 · place · feito", title, objective).
  void drawLocationPanel(ImVec2 size) {
    const e2::MapLocation* loc = nullptr;
    for (const e2::MapLocation& l : world_.locations())
      if (l.id == *open_) loc = &l;
    if (!loc || veil(size, "##locveil")) {
      open_.reset();
      return;
    }
    const auto missions = world_.visibleMissionsOf(*loc, realSave(), true);
    std::vector<std::string> order;
    for (const e2::MapMission* m : world_.missionsOf(*loc)) order.push_back(m->id);
    const float w = std::min(448.0f, size.x - 32.0f), inner = w - 40.0f;
    ImGui::SetNextWindowPos({size.x * 0.5f, size.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSizeConstraints({w, 0}, {w, size.y * 0.8f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    ImGui::Begin("##locpanel", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wa = ImGui::GetWindowPos(), wb{wa.x + ImGui::GetWindowSize().x, wa.y + ImGui::GetWindowSize().y};
    dl->PushClipRectFullScreen();
    emberPanel(dl, wa, wb);
    dl->PopClipRect();
    const ImVec2 head = ImGui::GetCursorScreenPos();
    emberTitle(dl, {head.x, head.y + 2}, 20.0f, loc->name.c_str());
    ImGui::SetCursorScreenPos({head.x + inner - 32, head.y});
    if (emberPlate("##locclose", {32, 32}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, IM_COL32(223, 191, 142, 255)); }))
      open_.reset();
    ImGui::SetCursorScreenPos({head.x, head.y + 32 + 16});
    std::string pick;
    for (size_t i = 0; i < missions.size(); ++i) {
      const e2::MapMission* m = missions[i];
      const std::string st = world_.missionStatus(m->id, realSave(), order, true);
      const e2::CampaignMission* cm = missionData(m->id);
      const std::string place = cm ? cm->place : "", objective = cm ? cm->objective : "";
      const float objH = sans()->CalcTextSizeA(16.0f, FLT_MAX, inner - 32, objective.c_str()).y;
      const float rowH = 12 + 18 + 4 + 32 + objH + 12;
      const ImVec2 a = ImGui::GetCursorScreenPos(), b{a.x + inner, a.y + rowH};
      ImGui::PushID(static_cast<int>(i));
      const bool clicked = ImGui::InvisibleButton("##mission", {inner, rowH});
      const bool hover = ImGui::IsItemHovered();
      ImGui::PopID();
      emberSlot(dl, a, b, hover);
      const int alpha = st == "locked" ? 102 : 255;
      float kx = a.x + 16;
      if (st == "locked") iconLock(dl, {kx + 6, a.y + 21}, 12, IM_COL32(220, 190, 140, alpha)), kx += 18;
      if (st == "done") iconCheck(dl, {kx + 6, a.y + 21}, 12, kAccent), kx += 18;
      const std::string kick = std::format("{:02} · {}{}", i + 1, place, st == "done" ? " · feito" : "");
      emberKicker(dl, {kx, a.y + 12}, 14.0f, upperUtf8(kick).c_str());
      emberTitle(dl, {a.x + 16, a.y + 34}, 24.0f, (cm ? cm->title : m->title).c_str());
      wrapped(dl, sans(), 16.0f, {a.x + 16, a.y + 34 + 32}, IM_COL32(180, 180, 180, alpha), objective, inner - 32);
      if (clicked) {
        if (st == "locked") flashId_ = m->id, flashAt_ = now_;
        else pick = m->id;
      }
      ImGui::Dummy({1, 8});
    }
    // test mode's note
    const ImVec2 n = ImGui::GetCursorScreenPos();
    dl->AddText(sans(), 12.0f, {n.x, n.y + 4}, kMuted, "Modo teste: todos os capítulos estão abertos.");
    ImGui::Dummy({inner, 20});
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!pick.empty()) {
      open_.reset();
      openMission(pick);
    }
  }

  // OptionsButton (the map's gear, EmberMenus.h OptionsDialog): Voltar à tela inicial (onReturnToTitle = onBack: the test
  // menu), Gráficos (Dev Controls' own switches), Áudio, Idiomas, Acessibilidade.
  void drawOptions(ImVec2 size) {
    if (!optionsDialog_.isOpen()) optionsDialog_.open();
    menu::OptionsDialog::Actions actions;
    actions.title = [this] { screen_ = Screen::Menu; };
    actions.fullscreen = callbacks_.fullscreen;
    actions.gfx = &devGfx_;
    optionsDialog_.draw(size, actions);
    if (!optionsDialog_.isOpen()) options_ = false;
  }

  // --- DevControlsScreen -----------------------------------------------------------------------------------------------

  // A switch row (role=switch): title and hint on the left, the h-6 w-11 pill on the right; true when clicked.
  bool switchRow(const char* id, float x, float& y, float w, const char* title, const char* hint, bool on) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float hintH = sans()->CalcTextSizeA(14.0f, FLT_MAX, w - 40 - 44 - 16, hint).y;
    const float h = 16 + 20 * 1.25f + 4 + hintH + 16;
    const bool clicked = cardButton(id, {x, y}, {w, h}, IM_COL32(17, 17, 17, 102), kBorder);
    dl->AddText(display(), 20.0f, {x + 20, y + 16}, kFg, title);
    wrapped(dl, sans(), 14.0f, {x + 20, y + 16 + 25 + 4}, kMuted, hint, w - 40 - 44 - 16);
    const ImVec2 p{x + w - 20 - 44, y + (h - 24) / 2};
    dl->AddRectFilled(p, {p.x + 44, p.y + 24}, on ? kAccent : kBorder, 12.0f);
    dl->AddCircleFilled({on ? p.x + 44 - 12 : p.x + 12, p.y + 12}, 10.0f, kBg, 20);
    y += h + 12;
    return clicked;
  }

  void drawDevControls(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, kBg);
    const float headerH = 16 + 40 + 16;
    dl->AddLine({0, headerH}, {size.x, headerH}, kBorder);
    if (backButton("##devback", {16, 16})) screen_ = Screen::Menu;
    caps(dl, {16 + 40 + 12, 17}, 14.0f, kMuted, "Modo teste", 0.18f);
    dl->AddText(display(), 30.0f, {16 + 40 + 12, 17 + 20}, kFg, "Dev Controls");
    ImGui::SetCursorScreenPos({0, headerH + 1});
    ImGui::BeginChild("##devcontrols", {size.x, size.y - headerH - 1}, 0, ImGuiWindowFlags_NoBackground);
    dl = ImGui::GetWindowDrawList();
    const float w = std::min(768.0f, size.x - 40.0f) - 40.0f;
    const float x = (size.x - w) * 0.5f;
    float y = ImGui::GetCursorScreenPos().y + 20;
    DevGfx& g = devGfx_;
    bool changed = false;

    // GraphicsQualityControl: fieldset, legend, Baixa / Média / Alta, the profile's note, "Personalizada" when edited.
    {
      const char* note = "Baixa: sem sombras ou FX atmosféricos. Média: sombras 2048 suaves. Alta: sombras 4096.";
      const char* note2 = "Ajusta resolução, sombras e iluminação. Mantém os modelos e as regras do jogo.";
      const bool custom = g.custom();
      const float h = 12 + 8 + 40 + 8 + 16 + (custom ? 24 : 0) + 8 + 16 + 12;
      dl->AddRectFilled({x, y}, {x + w, y + h}, IM_COL32(17, 17, 17, 204));
      dl->AddRect({x, y}, {x + w, y + h}, kBorder);
      const ImVec2 lt = sans()->CalcTextSizeA(14.0f, FLT_MAX, 0, "Qualidade gráfica");
      dl->AddRectFilled({x + 10, y - 9}, {x + 18 + lt.x, y + 9}, kBg);
      dl->AddText(sans(), 14.0f, {x + 14, y - 8}, kFg, "Qualidade gráfica");
      const char* ids[3] = {"low", "medium", "high"};
      const char* labels[3] = {"Baixa", "Média", "Alta"};
      const float bw = (w - 24 - 16) / 3;
      for (int i = 0; i < 3; ++i) {
        const bool pressed = !custom && g.quality == ids[i];
        const ImVec2 a{x + 12 + i * (bw + 8), y + 20};
        ImGui::PushID(i);
        if (cardButton("##q", a, {bw, 40}, pressed ? IM_COL32(216, 216, 216, 51) : 0, pressed ? kAccent : kBorder)) g.setQuality(ids[i]), changed = true;
        ImGui::PopID();
        const ImVec2 ts = sans()->CalcTextSizeA(14.0f, FLT_MAX, 0, labels[i]);
        dl->AddText(sans(), 14.0f, {a.x + (bw - ts.x) / 2, a.y + 12}, kFg, labels[i]);
      }
      float ty = y + 20 + 40 + 8;
      dl->AddText(sans(), 12.0f, {x + 12, ty}, kMuted, note);
      ty += 16;
      if (custom) dl->AddText(sans(), 12.0f, {x + 12, ty + 8}, kFg, std::format("Personalizada · {}", g.shadowResolution).c_str()), ty += 24;
      dl->AddText(sans(), 12.0f, {x + 12, ty + 8}, kMuted, note2);
      y += h + 12;
    }
    caps(dl, {x, y}, 14.0f, kMuted, "Sombras", 0.14f);
    y += 28;
    if (switchRow("##realShadows", x, y, w, "Sombras reais", "Sombra projetada pelo sol (unidades e props).", g.realShadows))
      g.realShadows = !g.realShadows, changed = true;
    if (switchRow("##softShadows", x, y, w, "Sombras suaves (PCF)", "Borda da sombra suavizada em vez de serrilhada.", g.softShadows))
      g.softShadows = !g.softShadows, changed = true;
    if (switchRow("##localLights", x, y, w, "Luzes do mapa",
                  "Braseiros, fogueiras, lanternas e casas em chamas iluminam o chão, os personagens e os props perto deles.", g.localLights))
      g.localLights = !g.localLights, changed = true;
    if (switchRow("##fogOfWar", x, y, w, "Fog of war", "Névoa de guerra nos mapas que a usam. Desligado = tudo visível, pra comparar.", g.fogOfWar))
      g.fogOfWar = !g.fogOfWar, changed = true;
    if (switchRow("##fogDebug", x, y, w, "Fog of war — debug", "Cores por hex: verde = visível, âmbar = explorado, vermelho = inexplorado.", g.fogDebug))
      g.fogDebug = !g.fogDebug, changed = true;
    // DEV_SKY_SLIDERS (the sun's two): a fixed sun once moved; "Padrão" gives the map back its own time of day.
    auto slider = [&](const char* id, const char* label, float& value, float lo, float hi) {
      const float h = 16 + 25 + 8 + 24 + 16;
      dl->AddRectFilled({x, y}, {x + w, y + h}, IM_COL32(17, 17, 17, 102));
      dl->AddRect({x, y}, {x + w, y + h}, kBorder);
      dl->AddText(display(), 20.0f, {x + 20, y + 16}, kFg, label);
      const std::string v = g.sunMoved ? std::format("{}°", static_cast<int>(std::round(value))) : "hora do dia";
      const ImVec2 vs = display()->CalcTextSizeA(20.0f, FLT_MAX, 0, v.c_str());
      dl->AddText(display(), 20.0f, {x + w - 20 - vs.x, y + 16}, kMuted, v.c_str());
      ImGui::SetCursorScreenPos({x + 20, y + 16 + 25 + 8});
      ImGui::PushItemWidth(w - 40);
      ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(74, 74, 74, 255));
      ImGui::PushStyleColor(ImGuiCol_SliderGrab, kAccent);
      ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, kFg);
      if (ImGui::SliderFloat(id, &value, lo, hi, "", ImGuiSliderFlags_AlwaysClamp)) g.sunMoved = true, changed = true;
      ImGui::PopStyleColor(3);
      ImGui::PopItemWidth();
      y += h + 12;
    };
    slider("##sunAz", "Sol — direção", g.sunAzimuth, 0.0f, 359.0f);
    slider("##sunEl", "Sol — altura", g.sunElevation, 5.0f, 85.0f);
    if (g.sunMoved) {
      if (cardButton("##sunreset", {x, y}, {160, 36}, 0, kBorder)) g.sunMoved = false, g.sunAzimuth = 53.13f, g.sunElevation = 45.0f, changed = true;
      dl->AddText(sans(), 14.0f, {x + 16, y + 9}, kFg, "Sol padrão");
      y += 36 + 12;
    }
    wrapped(dl, sans(), 12.0f, {x, y + 4}, kMuted, "Salvo neste computador. Vale em qualquer combate aberto depois.", w);
    ImGui::SetCursorScreenPos({x, y + 24});
    ImGui::Dummy({1, 20});
    ImGui::EndChild();
    if (changed && !g.save()) notice_ = "Não foi possível gravar " + DevGfx::path().string() + ".";
  }

  // --- CampaignScreen ("Cenários", the map's Lista) ------------------------------------------------------------------

  void drawScenarios(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, kBg);
    const float headerH = 16 + 40 + 16;
    dl->AddLine({0, headerH}, {size.x, headerH}, kBorder);
    if (backButton("##scenback", {16, 16})) screen_ = Screen::WorldMap;
    caps(dl, {16 + 40 + 12, 17}, 14.0f, kMuted, "Modo teste", 0.18f);
    dl->AddText(display(), 30.0f, {16 + 40 + 12, 17 + 20}, kFg, "Cenários");
    {
      const std::string gold = std::format("{} Gold", static_cast<long long>(session_.save.value("ember", kTestEmber)));
      const ImVec2 ts = sans()->CalcTextSizeA(14.0f, FLT_MAX, 0, gold.c_str());
      const ImVec2 p{size.x - 16 - ts.x - 16 - 24, 16 + 8};
      dl->AddRect(p, {size.x - 16, p.y + 26}, kBorder);
      if (const UiImage* icon = uiImage(paths_.engine2Public / "game/icons/gold-coins-pile.png")) imageContain(dl, *icon, {p.x + 6, p.y + 3}, {p.x + 26, p.y + 23});
      dl->AddText(sans(), 14.0f, {p.x + 30, p.y + 5}, kMuted, gold.c_str());
    }
    ImGui::SetCursorScreenPos({0, headerH + 1});
    ImGui::BeginChild("##scenarios", {size.x, size.y - headerH - 1}, 0, ImGuiWindowFlags_NoBackground);
    ImDrawList* cl = ImGui::GetWindowDrawList();
    const float w = size.x - 32;
    float y = ImGui::GetCursorScreenPos().y + 16;
    std::string pick;
    const auto& missions = campaign_.data().missions;
    for (size_t i = 0; i < missions.size(); ++i) {
      const e2::CampaignMission& m = missions[i];
      const e2::MapMission* mm = world_.mission(m.id);
      const bool hub = mm && mm->hub, finished = done(m.id);
      const float objH = sans()->CalcTextSizeA(16.0f, FLT_MAX, w - 32, m.objective.c_str()).y;
      const float h = 12 + 20 + 32 + objH + 12;
      ImGui::PushID(static_cast<int>(i));
      // rounded-xl border bg-surface px-4 py-3 (the open Inn: inn-glow)
      ImU32 border = kBorder;
      if (hub) {
        const float k = 0.5f - 0.5f * std::cos(now_ / 2.8f * 2 * std::numbers::pi_v<float>);
        border = IM_COL32(static_cast<int>(105 + (201 - 105) * k), static_cast<int>(105 + (160 - 105) * k), static_cast<int>(105 + (112 - 105) * k), 255);
      }
      if (cardButton("##scen", {16, y}, {w, h}, kSurface, border)) pick = m.id;
      ImGui::PopID();
      caps(cl, {32, y + 12}, 14.0f, kMuted, std::format("{:02} · {}{}", i + 1, m.place, hub ? " · aberta" : finished ? " · feito" : ""), 0.16f);
      cl->AddText(display(), 24.0f, {32, y + 12 + 20}, kFg, m.title.c_str());
      wrapped(cl, sans(), 16.0f, {32, y + 12 + 20 + 32}, kMuted, m.objective, w - 32);
      y += h + 8;
    }
    ImGui::SetCursorScreenPos({0, y + 8});
    ImGui::Dummy({1, 8});
    ImGui::EndChild();
    if (!pick.empty()) openMission(pick);
  }

  CampaignPaths paths_;
  e2::Campaign& campaign_;
  const e2::Overworld& world_;
  TestSession& session_;
  DevGfx& devGfx_;
  Callbacks callbacks_;
  Screen screen_ = Screen::Menu, briefingFrom_ = Screen::WorldMap;
  std::string notice_;
  std::optional<MissionEntry> briefing_;
  e2::Json realSave_;
  bool realSaveRead_ = false;
  std::optional<std::string> open_;
  bool options_ = false;
  menu::OptionsDialog optionsDialog_;  // Opções (EmberMenus.h), shown while options_
  std::string flashId_;
  float flashAt_ = -10.0f, now_ = 0.0f;
  int zoom_ = 3;  // opens at the biggest stop
  bool centred_ = false, recentre_ = false, dragMoved_ = false;
  float centreX_ = 0.5f, centreY_ = 0.5f, scrollX_ = 0.0f, scrollY_ = 0.0f;
  struct Drag {
    float x, y, sx, sy;
  };
  std::optional<Drag> drag_;
  int frames_ = 0;
};

}  // namespace ember
