#pragma once

// The game's look in its menus and HUD: Engine2's own fonts (title.css: Newsreader for titles, Figtree for text,
// Pirata One for marks and buttons; bundled in content/fonts, OFL) and its art, read at run time from Engine2's public
// folder (title backgrounds, panels, portraits) and shown through Dear ImGui.

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <imgui.h>

namespace ember {

enum class UiFont { Ui, Display, Mark };
// The font, or ImGui's default when its file is missing. Sizes are given per push (ImGui 1.93: PushFont(font, px)).
ImFont* uiFont(UiFont font);
// Called once by DebugUi while it builds the font atlas.
void loadUiFonts();

struct UiImage {
  ImTextureRef texture;
  int width = 0, height = 0;
};
// An image file (PNG / JPG), loaded on first use and kept; null when it cannot be read.
const UiImage* uiImage(const std::filesystem::path& file);
// True once uiImage has the file (or knows it cannot be read): it then returns at once.
bool uiImageLoaded(const std::filesystem::path& file);

// The title screen's art load (Engine2 title/main.ts loadTitleResources): files decoded one after another on a
// background thread and handed to uiImage's cache on the main thread, so the screens after the title open at once
// and the loading bar shows the real count. Main thread: update() once per frame.
class UiPreload {
 public:
  explicit UiPreload(std::vector<std::filesystem::path> files);
  ~UiPreload();
  UiPreload(const UiPreload&) = delete;
  UiPreload& operator=(const UiPreload&) = delete;
  // Adopts what is decoded; returns (files settled, files in all), a file that cannot be read counting as settled.
  std::pair<size_t, size_t> update();

 private:
  struct Decoded {
    std::string key;
    unsigned char* pixels = nullptr;
    int w = 0, h = 0, n = 0;
  };
  std::vector<std::filesystem::path> files_;
  std::mutex mutex_;
  std::vector<Decoded> decoded_;
  size_t done_ = 0;
  std::jthread worker_;  // last: started once everything it touches exists
};

// Engine2's title.css helpers for an ImGui draw list.
// A vertical gradient through colour stops (position 0..1, colour).
struct GradientStop {
  float at;
  ImU32 color;
};
void verticalGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, std::initializer_list<GradientStop> stops);
// An image covering the rectangle (CSS background-size: cover), its focus at (fx, fy) of the image (0.5, 0.42 = "center 42%").
void imageCover(ImDrawList* dl, const UiImage& image, ImVec2 a, ImVec2 b, float fx = 0.5f, float fy = 0.5f, ImU32 tint = IM_COL32_WHITE);
// Text with letter spacing (em of the font size) and an optional soft shadow (CSS text-shadow 0 2px 6px #000).
float spacedTextWidth(ImFont* font, float size, const char* text, float spacingEm);
void spacedText(ImDrawList* dl, ImFont* font, float size, ImVec2 at, ImU32 color, const char* text, float spacingEm, bool shadow = true);

// --- Engine2's ember-ui pieces (src/ember-ui/styles.css) ---------------------------------------------------------------
// .ember-panel: iron plate, gold hairline, L-shaped gold corners and an ember seam on top.
void emberPanel(ImDrawList* dl, ImVec2 a, ImVec2 b);
// .ember-rule: a gold hairline fading at both ends with a small diamond in the middle.
void emberRule(ImDrawList* dl, float x0, float x1, float y);
// .ember-slot: a dark trough (result rows, save slots); lit on hover.
void emberSlot(ImDrawList* dl, ImVec2 a, ImVec2 b, bool hover = false);
// .ember-kicker / .ember-title: Pirata One lettering with the ember glow (kicker: spaced capitals).
void emberKicker(ImDrawList* dl, ImVec2 at, float size, const char* text, bool defeat = false);
void emberTitle(ImDrawList* dl, ImVec2 at, float size, const char* text);
// .ember-btn (.ember-btn-primary / .ember-btn-ghost), full size: Pirata One capitals and the gold diamond caps.
bool emberBigButton(const char* id, const char* label, ImVec2 at, ImVec2 size, bool primary, float fontSize);

// Engine2's briefing art per mission id (GameApp BRIEF_ART), a path under its public folder; empty = none.
inline const char* engine2BriefArt(const std::string& id) {
  static const std::pair<const char*, const char*> kArt[] = {
      {"farmlands", "game/assets/brief-farmlands.jpg"}, {"vau", "game/assets/brief-vau.jpg"}, {"bosque", "game/assets/brief-bosque.jpg"},
      {"aldeia", "game/assets/brief-aldeia.jpg"}, {"muralha", "game/assets/brief-muralha.jpg"}, {"fortaleza", "game/assets/brief-fortaleza.jpg"},
      {"templo", "game/assets/brief-templo.jpg"}, {"cripta", "game/assets/brief-cripta.jpg"}, {"estalagem", "game/assets/brief-estalagem.jpg"},
      {"colina", "game/assets/brief-colina.jpg"}, {"passagem", "game/assets/brief-passagem.jpg"},
      {"watchtower-gate-floor", "game/assets/brief-watchtower.jpg"}, {"vertente", "game/assets/brief-vertente.jpg"},
      {"portao", "game/assets/brief-portao.jpg"}, {"profundezas", "game/assets/profundezas-bg.jpg"}, {"thebridge", "game/assets/brief-thebridge.jpg"},
      {"wisp-forest", "game/assets/brief-wisp-forest.jpg"}, {"wisp-forest-2", "game/assets/brief-wisp-forest.jpg"},
      {"wisp-forest-crossing", "game/assets/brief-wisp-forest.jpg"}, {"wisp-forest-crossing-boss", "game/assets/brief-wisp-forest.jpg"},
      {"cemiterio-esquecidos", "game/assets/Xemitery Briefing.jpg"},
      {"cemiterio-esquecidos-cripta-2", "game/assets/2Cemitério dos Esquecidos — Câmara Profunda✕.jpg"},
      {"misty-cave-dungeon", "game/assets/As Profundezas EnevoadasBrief.jpg"},
      {"cemiterio-esquecidos-cripta", "game/assets/Andar 3Cemitério dos Esquecidos — Cripta✕.jpg"},
      {"cemiterio-esquecidos-mausoleu", "game/assets/Andar 4Cemitério dos Esquecidos — Mausoléu✕.jpg"},
      {"cemiterio-esquecidos-ruinas", "game/assets/Andar 5Cemitério dos Esquecidos — Ruínas Submersas.jpg"},
      {"frozen-tundra-crossing", "game/assets/brief-frozen-tundra.jpg"}};
  for (const auto& [k, v] : kArt)
    if (id == k) return v;
  return "";
}

}  // namespace ember
