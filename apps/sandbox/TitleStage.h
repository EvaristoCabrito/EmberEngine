#pragma once

// The title screen and the mission list, copied from Engine2 (index.html + src/title/title.css): its background art
// (public/game/title/title-bg.jpg, centre 42%, under the dark veil), the "EMBER ASHES" mark, the eyebrow, the big
// "Ember", the tagline and the ember menu buttons; "Nova campanha" opens the mission list on Engine2's save-board art
// (public/game/ui/old-ember-save-board.png) with its card style; "Opções" and "Como jogar" open Engine2's dialogs.
// Engine2's art is read at run time from its public folder; the fonts are bundled (content/fonts).

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Stage.h"
#include "render/Camera.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "render/UiArt.h"

namespace ember {

// What the title screen knows how to start: a map with its imported scene and (when exported) its battle.
struct MissionEntry {
  std::string id, title, place;
  std::string draftId, briefing, objective;  // the map's own mission text (Engine2 Mission)
  std::filesystem::path map, scene, battle;
};

class TitleStage : public Stage {
 public:
  struct Paths {
    std::filesystem::path engine2Public = "C:/Engine2/public";  // Engine2's art
    std::filesystem::path maps;                                 // Engine2's maps/ember
    std::filesystem::path scenes;                               // build/engine2-scenes (export_scene.ts)
    std::filesystem::path battles = "content/engine2/battles";  // export_battle.ts
  };
  TitleStage(Paths paths, std::function<void(const MissionEntry&)> play, std::function<void()> quit)
      : paths_(std::move(paths)), play_(std::move(play)), quit_(std::move(quit)) {
    findMissions();
  }

  // Nothing of a map is drawn behind the menus (and nothing a closed map owned is left in the renderer's hands).
  void frame(Renderer& renderer, VkExtent2D) override {
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
      if (dialog_ != Dialog::None) dialog_ = Dialog::None;
      else if (screen_ == Screen::Missions) screen_ = Screen::Title;
      else if (screen_ == Screen::Briefing) screen_ = Screen::Missions;
    }
  }

  void drawPanel() override {
    const ImGuiIO& io = ImGui::GetIO();
    rem_ = 16.0f * std::clamp(io.DisplaySize.y / 900.0f, 0.85f, 2.0f);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##title", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    if (screen_ == Screen::Title) drawTitle(io.DisplaySize);
    else if (screen_ == Screen::Missions) drawMissions(io.DisplaySize);
    else drawBriefing(io.DisplaySize);
    ImGui::End();
    ImGui::PopStyleVar(2);
    if (dialog_ == Dialog::Options) drawOptions(io.DisplaySize);
    if (dialog_ == Dialog::Help) drawHelp(io.DisplaySize);
  }

 private:
  enum class Screen { Title, Missions, Briefing };
  enum class Dialog { None, Options, Help };

  // --- title.css ---------------------------------------------------------------------------------------------------

  void drawTitle(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{0, 0}, b = size;
    dl->AddRectFilled(a, b, IM_COL32(14, 14, 14, 255));  // .title-screen #0e0e0e
    if (const UiImage* hero = uiImage(paths_.engine2Public / "game/title/title-bg.jpg")) imageCover(dl, *hero, a, b, 0.5f, 0.42f);
    // .title-veil
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(8, 8, 8, 69)}, {0.36f, IM_COL32(10, 10, 10, 99)}, {0.72f, IM_COL32(10, 10, 10, 219)}, {1.0f, IM_COL32(14, 14, 14, 255)}});
    // .title-topline .title-mark
    spacedText(dl, uiFont(UiFont::Mark), 0.95f * rem_, {1.25f * rem_, 1.25f * rem_ + 0.4f * rem_}, IM_COL32(231, 199, 150, 191), "EMBER ASHES", 0.18f);

    // .title-content: min(32rem, 100% - 2.5rem) wide, centred, its bottom 6.6rem above the screen's.
    const float width = std::min(32.0f * rem_, size.x - 2.5f * rem_);
    const float x = (size.x - width) * 0.5f;
    const float eyebrow = 0.76f * rem_, h1 = std::clamp(size.x * 0.0825f, 3.3f * rem_, 5.25f * rem_), version = 0.65f * rem_, tagline = rem_;
    const float button = 3.15f * rem_, gap = 0.72f * rem_;
    const char* buttons[] = {"NOVA CAMPANHA", "COMO JOGAR", "OPÇÕES", "SAIR"};
    const int count = static_cast<int>(std::size(buttons));
    const float taglineLines = 2;
    const float total = eyebrow * 1.2f + 0.7f * rem_ + h1 * 0.92f + 0.75f * rem_ + version * 1.2f + rem_ + tagline * 1.65f * taglineLines + 1.7f * rem_ +
                        count * button + (count - 1) * gap;
    float y = size.y - 6.6f * rem_ - total;
    spacedText(dl, uiFont(UiFont::Ui), eyebrow, {x, y}, IM_COL32(194, 167, 127, 255), "TÁTICAS EM CINZAS", 0.28f, false);
    y += eyebrow * 1.2f + 0.7f * rem_;
    // h1: Newsreader, letter-spacing -.055em, shadow 0 3px 18px #000
    for (int k = 4; k >= 1; --k)
      spacedText(dl, uiFont(UiFont::Display), h1, {x, y + 3.0f + k}, IM_COL32(0, 0, 0, 40), "Ember", -0.055f, false);
    spacedText(dl, uiFont(UiFont::Display), h1, {x, y - h1 * 0.08f}, IM_COL32(243, 225, 194, 255), "Ember", -0.055f, false);
    y += h1 * 0.92f + 0.75f * rem_;
    spacedText(dl, uiFont(UiFont::Ui), version, {x + 0.2f * rem_, y}, IM_COL32(221, 199, 164, 184), "EMBER ENGINE · C++ / VULKAN", 0.2f, false);
    y += version * 1.2f + rem_;
    ImGui::PushFont(uiFont(UiFont::Ui), tagline);
    dl->AddText(uiFont(UiFont::Ui), tagline, {x, y + 2}, IM_COL32(0, 0, 0, 160), "Seis sobreviventes. Um tabuleiro de guerra.");
    dl->AddText(uiFont(UiFont::Ui), tagline, {x, y}, IM_COL32(209, 194, 172, 255), "Seis sobreviventes. Um tabuleiro de guerra.");
    dl->AddText(uiFont(UiFont::Ui), tagline, {x, y + tagline * 1.65f + 2}, IM_COL32(0, 0, 0, 160), "Cada casa conta.");
    dl->AddText(uiFont(UiFont::Ui), tagline, {x, y + tagline * 1.65f}, IM_COL32(209, 194, 172, 255), "Cada casa conta.");
    ImGui::PopFont();
    y += tagline * 1.65f * taglineLines + 1.7f * rem_;
    const float menuWidth = std::min(25.0f * rem_, width);
    for (int i = 0; i < count; ++i) {
      if (menuButton(buttons[i], {x, y}, {menuWidth, button}, i == 0)) {
        if (i == 0) screen_ = Screen::Missions;
        if (i == 1) dialog_ = Dialog::Help;
        if (i == 2) dialog_ = Dialog::Options;
        if (i == 3 && quit_) quit_();
      }
      y += button + gap;
    }
  }

  // .menu-button (.primary: the ember gradient; .secondary: dark iron with a gold edge); hover brightens and lifts 1px.
  bool menuButton(const char* label, ImVec2 at, ImVec2 size, bool primary) {
    ImGui::SetCursorScreenPos(at);
    ImGui::PushID(label);
    const bool clicked = ImGui::InvisibleButton("##b", size);
    const bool hover = ImGui::IsItemHovered();
    ImGui::PopID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float lift = hover ? -1.0f : 0.0f;
    const ImVec2 a{at.x, at.y + lift}, b{at.x + size.x, at.y + size.y + lift};
    auto bright = [&](int r, int g, int bl, int al = 255) {
      const float k = hover ? 1.13f : 1.0f;
      return IM_COL32(std::min(255, static_cast<int>(r * k)), std::min(255, static_cast<int>(g * k)), std::min(255, static_cast<int>(bl * k)), al);
    };
    // drop shadow (0 10px 26px rgba(0,0,0,.75) / 0 8px 22px rgba(0,0,0,.7)) and, primary, the ember glow (0 0 18px)
    for (int i = 6; i >= 1; --i) {
      const float s = static_cast<float>(i) * 3.0f;
      dl->AddRectFilled({a.x - s * 0.5f, a.y + s * 0.4f}, {b.x + s * 0.5f, b.y + s}, IM_COL32(0, 0, 0, 22), 2.0f + s);
      if (primary) dl->AddRect({a.x - s, a.y - s}, {b.x + s, b.y + s}, IM_COL32(255, 110, 30, 9), 2.0f + s, 2.0f);
    }
    dl->AddRect({a.x - 2, a.y - 2}, {b.x + 2, b.y + 2}, IM_COL32(0, 0, 0, 255), 2.0f);  // 0 0 0 1px #000
    if (primary) {
      verticalGradient(dl, a, b, {{0.0f, bright(0xd9, 0x62, 0x1c)}, {0.55f, bright(0x9c, 0x2f, 0x10)}, {1.0f, bright(0x56, 0x17, 0x08)}});
      dl->AddLine({a.x + 1, a.y + 1}, {b.x - 1, a.y + 1}, IM_COL32(255, 225, 160, 140));                               // inset 0 1px 0
      dl->AddRectFilledMultiColor({a.x, b.y - 4}, b, IM_COL32(50, 8, 0, 0), IM_COL32(50, 8, 0, 0), IM_COL32(50, 8, 0, 153), IM_COL32(50, 8, 0, 153));
      dl->AddRect(a, b, IM_COL32(255, 200, 130, 140), 2.0f);
    } else {
      verticalGradient(dl, a, b, {{0.0f, bright(0x23, 0x23, 0x23)}, {0.55f, bright(0x0a, 0x0a, 0x0a)}, {1.0f, bright(0x1a, 0x1a, 0x1a)}});
      dl->AddRect({a.x + 1, a.y + 1}, {b.x - 1, b.y - 1}, IM_COL32(255, 220, 160, 18), 2.0f);
      dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 216) : IM_COL32(190, 150, 95, 133), 2.0f);
    }
    const std::string upper = label;
    const float text = 1.05f * rem_;
    ImFont* font = uiFont(UiFont::Mark);
    const float tw = spacedTextWidth(font, text, upper.c_str(), 0.17f);
    const ImVec2 p{std::round((a.x + b.x - tw) * 0.5f), std::round((a.y + b.y - text) * 0.5f)};
    spacedText(dl, font, text, {p.x, p.y + 1}, primary ? IM_COL32(50, 8, 0, 230) : IM_COL32(0, 0, 0, 255), upper.c_str(), 0.17f, false);
    spacedText(dl, font, text, p, primary ? IM_COL32(242, 228, 208, 255) : IM_COL32(226, 194, 148, 255), upper.c_str(), 0.17f, false);
    return clicked;
  }

  // --- the mission list (.save-slots-screen) -----------------------------------------------------------------------

  void drawMissions(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{0, 0}, b = size;
    dl->AddRectFilled(a, b, IM_COL32(8, 10, 13, 255));
    if (const UiImage* board = uiImage(paths_.engine2Public / "game/ui/old-ember-save-board.png")) imageCover(dl, *board, a, b, 0.5f, 0.5f);
    dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 51));  // .save-slots-wash
    // header
    const float headerH = 4.4f * rem_;
    dl->AddRectFilled(a, {b.x, headerH}, IM_COL32(0, 0, 0, 51));
    dl->AddLine({0, headerH}, {b.x, headerH}, IM_COL32(255, 255, 255, 26));
    ImGui::SetCursorScreenPos({rem_, (headerH - 2.25f * rem_) * 0.5f});
    if (plateButton("##back", {6.0f * rem_, 2.25f * rem_}, "‹  Voltar")) screen_ = Screen::Title;
    const float hx = rem_ + 6.0f * rem_ + 0.75f * rem_;
    spacedText(dl, uiFont(UiFont::Ui), 0.65f * rem_, {hx, 0.9f * rem_}, IM_COL32(184, 154, 112, 255), "MISSÕES", 0.18f, false);
    dl->AddText(uiFont(UiFont::Display), 1.9f * rem_, {hx, 0.9f * rem_ + 0.65f * rem_ + 0.25f * rem_}, IM_COL32(241, 220, 192, 255), "Nova campanha");

    // the cards: 3 columns, max 64rem wide, centred
    const float listW = std::min(64.0f * rem_, size.x - 1.5f * rem_);
    const float gapC = 0.75f * rem_, cardH = 8.0f * rem_;
    const int cols = size.x >= 640.0f ? 3 : 2;
    const float cardW = (listW - gapC * (cols - 1)) / cols;
    const float x0 = (size.x - listW) * 0.5f;
    const int rows = static_cast<int>((missions_.size() + cols - 1) / cols);
    const float listH = rows * cardH + std::max(0, rows - 1) * gapC;
    const float top = headerH + std::max(0.75f * rem_, (size.y - headerH - listH) * 0.5f);
    ImGui::SetCursorScreenPos({0, headerH});
    ImGui::BeginChild("##missions", {size.x, size.y - headerH}, 0, ImGuiWindowFlags_NoBackground);
    const float scroll = ImGui::GetScrollY();
    ImDrawList* cl = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < missions_.size(); ++i) {
      const MissionEntry& m = missions_[i];
      const int c = static_cast<int>(i % cols), r = static_cast<int>(i / cols);
      const ImVec2 p{x0 + c * (cardW + gapC), top - headerH + r * (cardH + gapC) + headerH - scroll};
      ImGui::SetCursorScreenPos(p);
      ImGui::PushID(static_cast<int>(i));
      const bool clicked = ImGui::InvisibleButton("##card", {cardW, cardH});
      const bool hover = ImGui::IsItemHovered();
      ImGui::PopID();
      const ImVec2 q{p.x + cardW, p.y + cardH};
      verticalGradient(cl, p, q, {{0.0f, IM_COL32(12, 12, 12, 230)}, {1.0f, IM_COL32(5, 5, 5, 230)}});
      if (hover) for (int k = 3; k >= 1; --k) cl->AddRect({p.x - k * 3.0f, p.y - k * 3.0f}, {q.x + k * 3.0f, q.y + k * 3.0f}, IM_COL32(255, 110, 40, 14), 4.0f, 3.0f);
      cl->AddRect(p, q, hover ? IM_COL32(226, 170, 100, 191) : IM_COL32(190, 150, 95, 71), 2.0f);
      const float pad = rem_;
      float ty = p.y + (cardH - (0.62f * rem_ * 1.3f + 0.15f * rem_ + 1.15f * rem_ * 1.15f + 0.76f * rem_ * 1.3f)) * 0.5f;
      spacedText(cl, uiFont(UiFont::Ui), 0.62f * rem_, {p.x + pad, ty}, IM_COL32(184, 154, 112, 255), upperOf(m.place.empty() ? "Ember" : m.place).c_str(), 0.15f, false);
      ty += 0.62f * rem_ * 1.3f + 0.15f * rem_;
      cl->AddText(uiFont(UiFont::Display), 1.15f * rem_, {p.x + pad, ty}, IM_COL32(241, 220, 192, 255), m.title.c_str());
      ty += 1.15f * rem_ * 1.15f;
      cl->AddText(uiFont(UiFont::Ui), 0.76f * rem_, {p.x + pad, ty}, IM_COL32(196, 183, 164, 255),
                  m.battle.empty() ? "Sem combate exportado (só o mapa)" : "Batalha");
      if (clicked) chosen_ = i, screen_ = Screen::Briefing;
    }
    if (missions_.empty())
      cl->AddText(uiFont(UiFont::Ui), 0.9f * rem_, {x0, top}, IM_COL32(224, 189, 128, 255),
                  ("Nenhuma cena encontrada em " + paths_.scenes.string() + " (export_scene.ts).").c_str());
    ImGui::Dummy({1.0f, top - headerH + listH + rem_});
    ImGui::EndChild();
  }

  // --- GameApp BriefingScreen: the mission's art, its place and title, the briefing and objective, "Entrar em combate".

  void drawBriefing(ImVec2 size) {
    if (chosen_ >= missions_.size()) {
      screen_ = Screen::Missions;
      return;
    }
    const MissionEntry& m = missions_[chosen_];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{0, 0}, b = size;
    dl->AddRectFilled(a, b, IM_COL32(28, 28, 28, 255));  // bg-surface
    const char* art = engine2BriefArt(m.draftId);
    if (*art)
      if (const UiImage* image = uiImage(paths_.engine2Public / std::filesystem::u8path(art))) {
        imageCover(dl, *image, a, b, 0.5f, 0.5f);
        // bg-gradient-to-t from-bg/90 via-bg/45 to-transparent
        verticalGradient(dl, a, b, {{0.0f, IM_COL32(17, 17, 17, 0)}, {0.5f, IM_COL32(17, 17, 17, 115)}, {1.0f, IM_COL32(17, 17, 17, 230)}});
      }
    // header: the back button, the place (kicker) and the title
    const float pad = size.x >= 640.0f ? 1.5f * rem_ : rem_;
    ImGui::SetCursorScreenPos({pad, rem_});
    if (plateButton("##briefback", {2.5f * rem_, 2.5f * rem_}, "‹")) screen_ = Screen::Missions;
    const float hx = pad + 2.5f * rem_ + 0.75f * rem_;
    emberKicker(dl, {hx, rem_ - 0.1f * rem_}, 0.875f * rem_, upperOf(m.place.empty() ? "Ember" : m.place).c_str());
    emberTitle(dl, {hx, rem_ + 0.95f * rem_}, 1.875f * rem_, m.title.c_str());
    // the panel: max-w-xl, p-5
    const float width = std::min(36.0f * rem_, size.x - 2.0f * pad);
    const float inner = width - 2.5f * rem_;
    ImFont* ui = uiFont(UiFont::Ui);
    const float body = 1.125f * rem_;
    const std::string briefing = m.briefing.empty() ? "Sem briefing." : m.briefing;
    const ImVec2 bodySize = ui->CalcTextSizeA(body, FLT_MAX, inner, briefing.c_str());
    const ImVec2 objSize = ui->CalcTextSizeA(rem_, FLT_MAX, inner, m.objective.c_str());
    const float panelH = 1.25f * rem_ + bodySize.y + 1.25f * rem_ + rem_ + 0.75f * rem_ + 0.25f * rem_ + objSize.y + 1.25f * rem_;
    const ImVec2 p{pad, 5.5f * rem_}, q{pad + width, 5.5f * rem_ + panelH};
    emberPanel(dl, p, q);
    float y = p.y + 1.25f * rem_;
    dl->AddText(ui, body, {p.x + 1.25f * rem_, y}, IM_COL32(240, 240, 240, 255), briefing.c_str(), nullptr, inner);
    y += bodySize.y + 1.25f * rem_;
    emberRule(dl, p.x + 1.25f * rem_, q.x - 1.25f * rem_, y);
    y += rem_;
    emberKicker(dl, {p.x + 1.25f * rem_, y}, 0.75f * rem_, "OBJETIVO");
    y += 0.75f * rem_ + 0.25f * rem_;
    dl->AddText(ui, rem_, {p.x + 1.25f * rem_, y}, IM_COL32(216, 216, 216, 255), m.objective.c_str(), nullptr, inner);
    // "Entrar em combate"
    const float bh = 3.5f * rem_;
    if (emberBigButton("##enter", m.battle.empty() ? "ENTRAR" : "ENTRAR EM COMBATE", {pad, size.y - rem_ - bh}, {width, bh}, true, 1.15f * rem_) && play_)
      play_(m);
  }

  // .ember-plate as a button (Engine2's small iron plates: back buttons, the battle bar).
  bool plateButton(const char* id, ImVec2 size, const char* label) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hover = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 b{at.x + size.x, at.y + size.y};
    for (int i = 4; i >= 1; --i) dl->AddRectFilled({at.x - i, at.y + i * 1.5f}, {b.x + i, b.y + i * 1.5f}, IM_COL32(0, 0, 0, 25), 3.0f);
    if (hover) for (int i = 3; i >= 1; --i) dl->AddRect({at.x - i * 3.0f, at.y - i * 3.0f}, {b.x + i * 3.0f, b.y + i * 3.0f}, IM_COL32(255, 110, 40, 18), 4.0f, 3.0f);
    dl->AddRect({at.x - 1, at.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, 255), 2.0f);
    verticalGradient(dl, at, b, {{0.0f, IM_COL32(28, 28, 28, 240)}, {1.0f, IM_COL32(10, 10, 10, 242)}});
    dl->AddRect(at, b, hover ? IM_COL32(226, 170, 100, 216) : IM_COL32(190, 150, 95, 115), 2.0f);
    const float text = 0.8f * rem_;
    const ImVec2 t = uiFont(UiFont::Ui)->CalcTextSizeA(text, FLT_MAX, 0.0f, label);
    dl->AddText(uiFont(UiFont::Ui), text, {std::round((at.x + b.x - t.x) * 0.5f), std::round((at.y + b.y - t.y) * 0.5f)},
                hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(238, 224, 200, 255), label);
    return clicked;
  }

  // --- dialogs (.menu-dialog / .title-options-dialog) ----------------------------------------------------------------

  bool beginDialog(const char* id, ImVec2 screen, float widthRem, const char* title) {
    ImDrawList* bg = ImGui::GetForegroundDrawList();
    (void)bg;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(screen);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(3, 3, 3, 209));  // ::backdrop
    ImGui::Begin((std::string(id) + "_backdrop").c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) dialog_ = Dialog::None;
    ImGui::End();
    ImGui::PopStyleColor();
    const float w = std::min(widthRem * rem_, screen.x - 2.0f * rem_);
    ImGui::SetNextWindowPos({screen.x * 0.5f, screen.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {1.75f * rem_, 1.5f * rem_});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 20, 20, 250));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(190, 150, 95, 133));
    ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushFont(uiFont(UiFont::Display), 2.2f * rem_);
    ImGui::TextColored(ImColor(241, 223, 195), "%s", title);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Separator, IM_COL32(190, 150, 95, 77));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy({1, 0.5f * rem_});
    ImGui::PushFont(uiFont(UiFont::Ui), 0.95f * rem_);
    return true;
  }
  void endDialog() {
    ImGui::Dummy({1, 0.5f * rem_});
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 1.75f * rem_ - 7.0f * rem_);
    if (plateButton("##close", {7.0f * rem_, 2.5f * rem_}, "Fechar")) dialog_ = Dialog::None;
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
  }

  void drawOptions(ImVec2 screen) {
    beginDialog("##options", screen, 40.0f, "Opções");
    ImGui::TextColored(ImColor(196, 156, 105), "Gráficos");
    ImGui::TextColored(ImColor(183, 164, 140), "As escolhas de luz, céu, névoa e câmera ficam no painel do mapa durante a partida.");
    ImGui::Dummy({1, 0.3f * rem_});
    ImGui::TextColored(ImColor(196, 156, 105), "Áudio");
    ImGui::TextColored(ImColor(183, 164, 140), "O Ember ainda não toca som.");
    endDialog();
  }

  void drawHelp(ImVec2 screen) {
    beginDialog("##help", screen, 46.0f, "Como jogar");
    const char* lines[] = {
        "Clique numa unidade sua para ver até onde ela anda; clique numa casa azul para andar.",
        "Inimigos marcados em vermelho podem ser atacados: passe o mouse para ver a previsão, clique para atacar.",
        "Cada unidade age uma vez por rodada, na ordem da iniciativa. \"Fim do turno\" passa a vez.",
        "Botão direito gira a câmera, a roda aproxima, o botão esquerdo segurado arrasta o mapa.",
        "No topo à direita: Normal / Tática (2D), inclinar, girar e voltar à vista padrão.",
    };
    for (const char* l : lines) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 40.0f * rem_);
      ImGui::TextColored(ImColor(234, 220, 201), "·  %s", l);
      ImGui::PopTextWrapPos();
    }
    endDialog();
  }

  // --- missions ------------------------------------------------------------------------------------------------------

  // CSS text-transform: uppercase, for ASCII and the accented Latin letters (UTF-8 C3 A0-BE).
  static std::string upperOf(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) {
      unsigned char& c = reinterpret_cast<unsigned char&>(s[i]);
      if (c == 0xC3 && i + 1 < s.size()) {
        unsigned char& d = reinterpret_cast<unsigned char&>(s[i + 1]);
        if (d >= 0xA0 && d <= 0xBE && d != 0xB7) d = static_cast<unsigned char>(d - 0x20);
        ++i;
      } else if (c < 0x80) {
        c = static_cast<unsigned char>(std::toupper(c));
      }
    }
    return s;
  }

  void findMissions() {
    std::error_code error;
    if (!std::filesystem::is_directory(paths_.scenes, error)) return;
    for (const auto& entry : std::filesystem::directory_iterator(paths_.scenes, error)) {
      const std::string name = entry.path().filename().string();
      const std::string suffix = ".scene.json";
      if (name.size() <= suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
      MissionEntry m;
      m.id = name.substr(0, name.size() - suffix.size());
      m.scene = entry.path();
      m.map = paths_.maps / (m.id + ".json");
      if (!std::filesystem::exists(m.map, error)) continue;
      const std::filesystem::path battle = paths_.battles / (m.id + ".battle.json");
      if (std::filesystem::exists(battle, error)) m.battle = battle;
      m.title = m.id;
      std::ifstream in(m.map, std::ios::binary);
      const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
      if (!j.is_discarded() && j.contains("draft")) {
        const nlohmann::json& d = j["draft"];
        if (d.contains("title") && d["title"].is_string() && !d["title"].get<std::string>().empty()) m.title = d["title"].get<std::string>();
        if (d.contains("place") && d["place"].is_string()) m.place = d["place"].get<std::string>();
        if (d.contains("id") && d["id"].is_string()) m.draftId = d["id"].get<std::string>();
        if (d.contains("briefing") && d["briefing"].is_string()) m.briefing = d["briefing"].get<std::string>();
        if (d.contains("objective") && d["objective"].is_string()) m.objective = d["objective"].get<std::string>();
      }
      missions_.push_back(std::move(m));
    }
    // battles first, then by title
    std::sort(missions_.begin(), missions_.end(), [](const MissionEntry& x, const MissionEntry& y) {
      if (x.battle.empty() != y.battle.empty()) return !x.battle.empty();
      return x.title < y.title;
    });
  }

  Paths paths_;
  std::function<void(const MissionEntry&)> play_;
  std::function<void()> quit_;
  std::vector<MissionEntry> missions_;
  Screen screen_ = Screen::Title;
  size_t chosen_ = 0;
  Dialog dialog_ = Dialog::None;
  float rem_ = 16.0f;
};

}  // namespace ember
