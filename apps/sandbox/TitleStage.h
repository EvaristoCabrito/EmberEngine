#pragma once

// The title screen, copied from Engine2 (index.html + src/title/title.css + main.ts): its background art
// (public/game/title/title-bg.jpg, centre 42%, under the dark veil), the "EMBER ASHES" mark, the eyebrow, the big
// "Ember", the tagline and the ember menu buttons. "Nova campanha" and "Continuar" open the six save slots on Engine2's
// save-board art (public/game/ui/old-ember-save-board.png, CampaignUi.h SlotScreen); a chosen slot starts its campaign
// on the world map (OverworldStage). "Modo teste" (bottom left) is Engine2's test entry (TestModeStage.h).
// "Como jogar" and "Opções" open Engine2's own windows (EmberMenus.h HelpModal / OptionsDialog). Every text in Portuguese
// or English (index.html data-pt / data-en), as Opções sets the interface.
// Engine2's art is read at run time from its public folder; the fonts are bundled (content/fonts).

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "CampaignUi.h"
#include "EmberMenus.h"
#include "GameMusic.h"
#include "LoadingScreens.h"
#include "Stage.h"
#include "render/Camera.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "render/UiArt.h"
#include "world/Campaign.h"

namespace ember {

class TitleStage : public Stage {
 public:
  struct Paths {
    std::filesystem::path engine2Public = "content/game/public";
    std::filesystem::path maps;                                 // Engine2's maps/ember
    std::filesystem::path scenes;                               // build/engine2-scenes (export_scene.ts)
    std::filesystem::path battles = "content/engine2/battles";  // export_battle.ts
  };
  // play: a test-mode map; campaign: the chosen slot's campaign (the world map); quit: Sair. `preload`: the title's art
  // already decoding (main.cpp starts it with the window, artFiles), else the title starts it.
  TitleStage(Paths paths, e2::Campaign* campaign, std::function<void(const MissionEntry&)> play, std::function<void()> startCampaign,
             std::function<void()> quit, std::unique_ptr<UiPreload> preload = nullptr)
      : paths_(std::move(paths)), campaign_(campaign), play_(std::move(play)), startCampaign_(std::move(startCampaign)), quit_(std::move(quit)) {
    findMissions();
    preload_ = preload ? std::move(preload) : std::make_unique<UiPreload>(titleArt());
  }
  // The window's start (main.cpp BootStage): the bar that was up while the game's data loaded carries on here, its `units`
  // counted before the art's.
  void carryLoader(const loading::TitleLoader& loader, size_t units) { loader_ = loader, bootUnits_ = units; }
  // The title's background (title.css .title-screen + .title-veil); its art only once decoded (never read on this thread).
  static void drawBackdrop(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    dl->AddRectFilled(a, b, IM_COL32(14, 14, 14, 255));  // .title-screen #0e0e0e
    if (const std::filesystem::path art = backdropFile(); uiImageLoaded(art))
      if (const UiImage* hero = uiImage(art)) imageCover(dl, *hero, a, b, 0.5f, 0.42f);
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(8, 8, 8, 69)}, {0.36f, IM_COL32(10, 10, 10, 99)}, {0.72f, IM_COL32(10, 10, 10, 219)}, {1.0f, IM_COL32(14, 14, 14, 255)}});
  }
  // #title-loader's place: the menu column's left edge and its width.
  static std::pair<float, float> loaderColumn(ImVec2 size, float rem) {
    const float width = std::min(32.0f * rem, size.x - 2.5f * rem);
    return {(size.x - width) * 0.5f, std::min(25.0f * rem, size.x - 2.5f * rem)};
  }
  // "Modo teste" opens Engine2's test menu through this (without it: the old list of every exported map).
  void onTestMode(std::function<void()> open) { testMode_ = std::move(open); }
  // Opções: Tela cheia and Dev Controls' switches (Gráficos), as the game holds them.
  void onOptions(std::function<void()> fullscreen, DevGfx* gfx) { fullscreen_ = std::move(fullscreen), gfx_ = gfx; }
  // Captures: Como jogar (tab, scroll) or Opções opened.
  void previewHelp(int tab, float scroll) { help_.preview(tab, scroll); }
  void previewOptions() { options_.open(); }
  // Captures end after a second of frames (time for Engine2's art to load).
  bool done() const override { return frames_ > 60; }
  // Captures: the slot screen as "Nova campanha" (false) or "Continuar" (true) opens it.
  void previewSlots(bool load) { openSlots(load); }
  bool picked() const { return picked_; }
  // The title page's own music (index.html menu-music), on through its save slots; a test briefing plays its mission's.
  Music music() const override {
    if (screen_ == Screen::Title || screen_ == Screen::Slots) return music::theme("title");
    if (screen_ == Screen::Briefing && chosen_ < missions_.size()) return music::forMission(missions_[chosen_]);
    return {};
  }
  // Captures: a slot chosen as a click would (writes the bank given with --save-bank).
  void previewPick(int index) {
    if (campaign_ && slots_.choose(index, *campaign_)) picked_ = true;
  }
  // Back from a campaign fight: the slot screen opens on the campaign, its status line saying where it stands.
  void showCampaign(std::string text) {
    slots_.open(SlotScreen::Mode::Load);
    slots_.setStatus(std::move(text));
    screen_ = Screen::Slots;
  }

  // Nothing of a map is drawn behind the menus (and nothing a closed map owned is left in the renderer's hands).
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
      if (help_.isOpen() || options_.isOpen()) return;  // they close themselves
      if (screen_ == Screen::Missions || screen_ == Screen::Slots) screen_ = Screen::Title;
      else if (screen_ == Screen::Briefing) screen_ = Screen::Missions;
    }
  }

  void drawPanel() override {
    const ImGuiIO& io = ImGui::GetIO();
    // The title's art load (title/main.ts loadTitleResources): the bar shows how much of it has settled.
    if (preload_) {
      const auto [done, total] = preload_->update();
      const size_t all = bootUnits_ + total;
      loader_.set(all ? static_cast<float>(bootUnits_ + done) / static_cast<float>(all) : 1.0f, done >= total, ImGui::GetTime());
    }
    rem_ = 16.0f;  // canvas units: the UI is zoomed to the window (DebugUi::kUiHeight), 24 px at 1080
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##title", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    if (screen_ == Screen::Title) drawTitle(io.DisplaySize);
    else if (screen_ == Screen::Slots) drawSlots(io.DisplaySize);
    else if (screen_ == Screen::Missions) drawMissions(io.DisplaySize);
    else if (chosen_ < missions_.size()) {
      const int r = drawBriefing(io.DisplaySize, rem_, missions_[chosen_], paths_.engine2Public);
      if (r < 0) screen_ = Screen::Missions;
      else if (r > 0 && play_) play_(missions_[chosen_]);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    menu::OptionsDialog::Actions actions;
    actions.fullscreen = fullscreen_;
    actions.gfx = gfx_;
    options_.draw(io.DisplaySize, actions);
    help_.draw(io.DisplaySize);
  }

 private:
  enum class Screen { Title, Slots, Missions, Briefing };

  // --- title.css ---------------------------------------------------------------------------------------------------

  void drawTitle(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{0, 0}, b = size;
    drawBackdrop(dl, a, b);
    // .title-topline .title-mark
    spacedText(dl, uiFont(UiFont::Mark), 0.95f * kTitleText * rem_, {1.25f * rem_, 1.25f * rem_ + 0.4f * rem_}, IM_COL32(231, 199, 150, 191), "EMBER ASHES", 0.18f);
    ImGui::SetCursorScreenPos({size.x - rem_ - 2.75f * rem_, 1.25f * rem_});
    soundButton("##titlesound", {2.75f * rem_, 2.75f * rem_}, 1.25f * rem_, true);

    // .title-content: min(32rem, 100% - 2.5rem) wide, centred, its bottom 6.6rem above the screen's.
    const float width = std::min(32.0f * rem_, size.x - 2.5f * rem_);
    const float x = (size.x - width) * 0.5f;
    // every text 1.4x Engine2's but the heading (user 2026-10-16: "all fonts too small"; boxes unchanged)
    const float h1 = std::clamp(size.x * 0.0825f, 3.3f * rem_, 5.25f * rem_), version = 1.1f * rem_;
    const float button = 3.15f * rem_, gap = 0.72f * rem_;
    // Engine2's menu: Nova campanha, Continuar (only with a save), Como jogar, Opções; Sair closes the game (a browser
    // tab had no need of it).
    enum Action { New, Continue, Help, Options, Quit };
    std::vector<std::pair<std::string, Action>> menu = {{upperUtf8(tr("Nova campanha", "New campaign")), New}};
    if (campaign_ && campaign_->hasAnySave()) menu.push_back({upperUtf8(tr("Continuar", "Continue")), Continue});
    menu.push_back({upperUtf8(tr("Como jogar", "How to play")), Help}), menu.push_back({upperUtf8(tr("Opções", "Options")), Options});
    menu.push_back({upperUtf8(tr("Sair", "Quit")), Quit});
    const int count = static_cast<int>(menu.size());
    const float total = h1 * 0.92f + 0.75f * rem_ + version * 1.2f + 1.7f * rem_ + count * button + (count - 1) * gap;
    float y = size.y - 6.6f * rem_ - total;
    // h1: only "Ember Ashes" in Pirata One (user 2026-10-16: no eyebrow, no Newsreader "Ember"), shadow 0 3px 18px #000
    for (int k = 4; k >= 1; --k)
      spacedText(dl, uiFont(UiFont::Mark), h1, {x, y + 3.0f + k}, IM_COL32(0, 0, 0, 40), "Ember Ashes", 0.0f, false);
    spacedText(dl, uiFont(UiFont::Mark), h1, {x, y - h1 * 0.08f}, IM_COL32(243, 225, 194, 255), "Ember Ashes", 0.0f, false);
    y += h1 * 0.92f + 0.75f * rem_;
    // the engine line, made readable over the art (user 2026-10-16): larger, tighter tracking, opaque, with the tagline's shadow
    {
      const char* engineLine = "EMBER ENGINE · C++ / VULKAN · VERSION 0.36.5";
      spacedText(dl, uiFont(UiFont::Ui), version, {x + 0.2f * rem_, y + 2}, IM_COL32(0, 0, 0, 170), engineLine, 0.12f, false);
      spacedText(dl, uiFont(UiFont::Ui), version, {x + 0.2f * rem_, y}, IM_COL32(232, 213, 182, 245), engineLine, 0.12f, false);
    }
    // no tagline under it (user 2026-10-16: only the title and the engine line)
    y += version * 1.2f + 1.7f * rem_;
    const float menuWidth = std::min(25.0f * rem_, width);
    for (int i = 0; i < count; ++i) {
      // index.html: "Nova campanha" stays disabled until the title's art has loaded (main.ts enables it at the end)
      const bool enabled = menu[static_cast<size_t>(i)].second != New || loader_.ready();
      if (menuButton(menu[static_cast<size_t>(i)].first.c_str(), {x, y}, {menuWidth, button}, i == 0, enabled)) {
        switch (menu[static_cast<size_t>(i)].second) {
          case New: openSlots(false); break;
          case Continue: openSlots(true); break;
          case Help: help_.open(); break;
          case Options: options_.open(); break;
          case Quit:
            if (quit_) quit_();
            break;
        }
      }
      y += button + gap;
    }
    // #title-loader under the menu column (same left edge; min(25rem, 100% - 2.5rem) wide)
    loader_.draw(dl, size, x, std::min(25.0f * rem_, size.x - 2.5f * rem_), rem_, ImGui::GetTime(), ImGui::GetIO().DeltaTime);
    // .test-entry: "Modo teste", bottom left, faint uppercase; every exported map.
    {
      const std::string labelText = upperUtf8(tr("Modo teste", "Test mode"));
      const char* label = labelText.c_str();
      const float text = 0.62f * rem_;  // Engine2's size (user 2026-10-16: keep this one as it was)
      const float tw = spacedTextWidth(uiFont(UiFont::Ui), text, label, 0.14f);
      const ImVec2 at{0.55f * rem_, size.y - 0.55f * rem_ - text - 0.6f * rem_};
      ImGui::SetCursorScreenPos(at);
      const bool clicked = ImGui::InvisibleButton("##testmode", {tw + 0.9f * rem_, text + 0.6f * rem_});
      const bool hover = ImGui::IsItemHovered();
      spacedText(dl, uiFont(UiFont::Ui), text, {at.x + 0.45f * rem_, at.y + 0.3f * rem_},
                 hover ? IM_COL32(239, 212, 165, 255) : IM_COL32(220, 203, 177, 117), label, 0.14f, false);
      if (clicked) {
        if (testMode_) testMode_();  // Engine2's test menu (TestModeStage.h)
        else screen_ = Screen::Missions;
      }
    }
  }

  // --- the save slots (CampaignUi.h SlotScreen) ------------------------------------------------------------------

  void openSlots(bool load) {
    slots_.open(load ? SlotScreen::Mode::Load : SlotScreen::Mode::New);
    screen_ = Screen::Slots;
  }

  void drawSlots(ImVec2 size) {
    if (!campaign_) {
      ImGui::GetWindowDrawList()->AddText(uiFont(UiFont::Ui), rem_, {rem_, rem_}, IM_COL32(224, 189, 128, 255),
                                         "Sem dados de campanha (content/engine2/campaign.json).");
      return;
    }
    bool back = false;
    const int chosen = slots_.draw(size, rem_, *campaign_, paths_.engine2Public, back);
    if (back) screen_ = Screen::Title;
    else if (chosen >= 0 && startCampaign_) startCampaign_();
  }

  // .menu-button (.primary: the ember gradient; .secondary: dark iron with a gold edge); hover brightens and lifts 1px.
  bool menuButton(const char* label, ImVec2 at, ImVec2 size, bool primary, bool enabled = true) {
    ImGui::SetCursorScreenPos(at);
    ImGui::PushID(label);
    const bool clicked = ImGui::InvisibleButton("##b", size) && enabled;
    const bool hover = ImGui::IsItemHovered() && enabled;
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
    const float text = 1.05f * kTitleText * rem_;
    ImFont* font = uiFont(UiFont::Mark);
    const float tw = spacedTextWidth(font, text, upper.c_str(), 0.17f);
    const ImVec2 p{std::round((a.x + b.x - tw) * 0.5f), std::round((a.y + b.y - text) * 0.5f)};
    spacedText(dl, font, text, {p.x, p.y + 1}, primary ? IM_COL32(50, 8, 0, 230) : IM_COL32(0, 0, 0, 255), upper.c_str(), 0.17f, false);
    spacedText(dl, font, text, p, primary ? IM_COL32(242, 228, 208, 255) : IM_COL32(226, 194, 148, 255), upper.c_str(), 0.17f, false);
    // .menu-button:disabled { opacity: .48 }: the screen's own dark showing through
    if (!enabled) dl->AddRectFilled({a.x - 2, a.y - 2}, {b.x + 2, b.y + 2}, IM_COL32(14, 14, 14, 133), 2.0f);
    return clicked;
  }

  // What the title loads before the game opens (Engine2 loadTitleResources: the title art, the loading screen's art, the
  // game's art): every menu picture of game/ui and game/title, the world map and the mission briefings, so the screens
  // after the title open at once.
  std::vector<std::filesystem::path> titleArt() const { return artFiles(paths_.engine2Public); }

 public:
  static std::filesystem::path backdropFile() { return "content/backdrops/title-bg.jpg"; }
  // The title's background first (the screen shows it as soon as it is in), then the rest.
  static std::vector<std::filesystem::path> artFiles(const std::filesystem::path& engine2Public) {
    std::vector<std::filesystem::path> files;
    auto add = [&](const std::filesystem::path& dir, const char* prefix) {
      std::error_code ec;
      for (const auto& e : std::filesystem::directory_iterator(engine2Public / dir, ec)) {
        if (!e.is_regular_file()) continue;
        // wide names: Engine2's art has names no narrow conversion survives ("✕", "—"), which are skipped
        std::wstring ext = e.path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        if (ext != L".png" && ext != L".jpg" && ext != L".jpeg") continue;
        const std::wstring name = e.path().filename().wstring();
        if (std::any_of(name.begin(), name.end(), [](wchar_t c) { return c > 0x7e; })) continue;
        if (prefix && name.rfind(std::wstring(prefix, prefix + std::strlen(prefix)), 0) != 0) continue;
        if (!uiImageLoaded(e.path())) files.push_back(e.path());
      }
    };
    add("game/title", nullptr);
    add("game/ui", nullptr);
    {  // the briefings live in content/briefings (nothing of them is read from Engine2's folders)
      std::error_code ec;
      for (const auto& e : std::filesystem::directory_iterator("content/briefings", ec)) {
        const std::string name = e.path().filename().string();
        if (e.is_regular_file() && name.rfind("brief-", 0) == 0 && !uiImageLoaded(e.path())) files.push_back(e.path());
      }
    }
    if (const auto map = engine2Public / "game/assets/world-map.jpg"; !uiImageLoaded(map)) files.push_back(map);
    std::sort(files.begin(), files.end());
    if (!uiImageLoaded(backdropFile())) files.insert(files.begin(), backdropFile());
    return files;
  }

 private:

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
  e2::Campaign* campaign_ = nullptr;
  SlotScreen slots_;
  bool picked_ = false;
  std::function<void(const MissionEntry&)> play_;
  std::function<void()> startCampaign_;
  std::function<void()> quit_;
  std::function<void()> testMode_;
  std::vector<MissionEntry> missions_;
  Screen screen_ = Screen::Title;
  size_t chosen_ = 0;
  menu::HelpModal help_;
  menu::OptionsDialog options_;
  std::function<void()> fullscreen_;
  DevGfx* gfx_ = nullptr;
  float rem_ = 16.0f;
  static constexpr float kTitleText = 1.4f;  // the title page's text over Engine2's sizes
  int frames_ = 0;
  std::unique_ptr<UiPreload> preload_;
  loading::TitleLoader loader_;
  size_t bootUnits_ = 0;  // carryLoader: what was loaded before the title, counted in its bar
};

}  // namespace ember
