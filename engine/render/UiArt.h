#pragma once

// The game's look in its menus and HUD: Ember's own fonts (title.css: Newsreader for titles, Figtree for text,
// Pirata One for marks and buttons; bundled in content/fonts, OFL) and its art, read at run time from Ember's public
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
// Called by DebugUi between ImGui::Render and the draw: every UI image drawn smaller than its file is swapped for a copy
// resampled to its size on screen (the browser's smooth downscale), so icons and portraits stay sharp, never jagged.
void refineUiImages(ImDrawData* drawData);
// The loading screen before a map (the user, 2026-10-29: "loading screens should load and unload that"): the UI images a fight
// never draws (the world maps' paintings, the briefings' art, and their resampled copies) leave the GPU, their pixels kept in
// RAM; portraits, icons and frames stay. Whatever a screen draws again goes back on the GPU by itself (refineUiImages).
void releaseUiImages();
// One axis of a resample of premultiplied float RGBA: a tent as wide as a source pixel when enlarging and as wide as a
// destination pixel when shrinking (every source pixel averaged). dstW is the new length along that axis.
std::vector<float> uiResampleAxis(const std::vector<float>& src, int srcW, int srcH, int dstW, bool horizontal);

// A lucide icon (content/ui/lucide/<name>.svg, Ember's lucide 0.510.0) rasterised at its exact pixel size on screen
// and tinted with `ink`; `size` = its 24-unit box in UI units (Ember: size-4 = 16), centred on `centre`.
// strokeWidth: the SVG's stroke in its 24 box (lucide 2; Ember's options headings 1.5).
void uiIcon(ImDrawList* dl, const char* name, ImVec2 centre, float size, ImU32 ink, float strokeWidth = 2.0f);
// The same for an SVG given as text (white = ink); `key` names it in the raster cache.
void uiIconSvg(ImDrawList* dl, const std::string& key, const std::string& svg, ImVec2 centre, float size, ImU32 ink);

// The title screen's art load (Ember title/main.ts loadTitleResources): files decoded one after another on a
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

// Ember's title.css helpers for an ImGui draw list.
// A vertical gradient through colour stops (position 0..1, colour).
struct GradientStop {
  float at;
  ImU32 color;
};
void verticalGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, std::initializer_list<GradientStop> stops);
// An image covering the rectangle (CSS background-size: cover), its focus at (fx, fy) of the image (0.5, 0.42 = "center 42%").
void imageCover(ImDrawList* dl, const UiImage& image, ImVec2 a, ImVec2 b, float fx = 0.5f, float fy = 0.5f, ImU32 tint = IM_COL32_WHITE);
// The whole image inside the rectangle, its shape kept, centred (CSS object-fit: contain).
void imageContain(ImDrawList* dl, const UiImage& image, ImVec2 a, ImVec2 b, ImU32 tint = IM_COL32_WHITE);
// Text with letter spacing (em of the font size) and an optional soft shadow (CSS text-shadow 0 2px 6px #000).
float spacedTextWidth(ImFont* font, float size, const char* text, float spacingEm);
void spacedText(ImDrawList* dl, ImFont* font, float size, ImVec2 at, ImU32 color, const char* text, float spacingEm, bool shadow = true);

// --- Ember's ember-ui pieces (src/ember-ui/styles.css) ---------------------------------------------------------------
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

// Ember's briefing art per mission id (GameApp BRIEF_ART), a path under its public folder; empty = none.
inline const char* classicBriefArt(const std::string& id) {
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

// The briefing art drawn for a mission id, from content/briefings (the project's one folder of briefings; the backgrounds are in
// content/backdrops): EmberEngine's own first, else the file the mission's table names (Ember's name, its copy here). Nothing is read from
// Ember's folders (the user, 2026-10-20: "JAMAIS em dois engines"); empty = none.
// Random encounters (ids "random-..."): their own art (the user's, 2026-10-28: Attachments/Briefings), the same art repeated on the
// encounters that look alike, the two merchants' old backgrounds, else the one shared random-encounter picture.
inline std::filesystem::path briefArtFile(const std::string& id, const std::filesystem::path& /*artRoot, unused: kept for the callers*/) {
  static const std::pair<const char*, const char*> kOwn[] = {
      {"ashen-forest-crossing", "content/briefings/ashen-forest-brief-v02.jpg"},
      {"random-encounter-11", "content/backdrops/merchant-road-background-001.jpg"},
      {"random-encounter-14", "content/backdrops/merchant-snow-market-background-001.jpg"},
      {"random-amanita-glade", "content/briefings/brief-random-amanita-glade-v01.jpg"},
      {"random-backward-hunt", "content/briefings/brief-random-backward-hunt-v01.jpg"},
      {"random-bell-beneath-ice", "content/briefings/brief-random-bell-beneath-ice-v01.jpg"},
      {"random-blightwood-verge", "content/briefings/brief-random-blightwood-verge-v01.jpg"},
      {"random-broken-antler-grove", "content/briefings/brief-random-broken-antler-grove-v01.jpg"},
      {"random-cairnkeepers-rise", "content/briefings/brief-random-cairnkeepers-rise-v01.jpg"},
      {"random-caravan-green-road", "content/briefings/brief-random-caravan-green-road-v01.jpg"},
      {"random-coldwind-saddle", "content/briefings/brief-random-coldwind-saddle-v01.jpg"},
      {"random-crystal-ossuary", "content/briefings/brief-random-crystal-ossuary-v01.jpg"},
      {"random-encounter-12", "content/briefings/brief-random-encounter-12-v01.jpg"},
      {"random-encounter-13", "content/briefings/brief-random-encounter-13-v01.jpg"},
      {"random-encounter-3", "content/briefings/brief-random-encounter-3-v01.jpg"},
      {"random-frostbound-sanctuary", "content/briefings/brief-random-frostbound-sanctuary-v01.jpg"},
      {"random-greycairn-plateau", "content/briefings/brief-random-greycairn-plateau-v01.jpg"},
      {"random-hollow-pine-basin", "content/briefings/brief-random-hollow-pine-basin-v01.jpg"},
      {"random-hollow-root-den", "content/briefings/brief-random-hollow-root-den-v01.jpg"},
      {"random-icelands-blue-antlers", "content/briefings/brief-random-icelands-blue-antlers-v01.jpg"},
      {"random-icelands-last-beacon", "content/briefings/brief-random-icelands-last-beacon-v01.jpg"},
      {"random-mora-hollow-stream", "content/briefings/brief-random-mora-hollow-stream-v01.jpg"},
      {"random-mora-rootwatch", "content/briefings/brief-random-mora-rootwatch-v01.jpg"},
      {"random-plains-broken-caravan", "content/briefings/brief-random-plains-broken-caravan-v01.jpg"},
      {"random-plains-hill-patrol", "content/briefings/brief-random-plains-hill-patrol-v01.jpg"},
      {"random-plains-old-stones", "content/briefings/brief-random-plains-old-stones-v01.jpg"},
      {"random-plains-silent-harvest", "content/briefings/brief-random-plains-silent-harvest-v01.jpg"},
      {"random-plains-sunken-ford", "content/briefings/brief-random-plains-sunken-ford-v01.jpg"},
      {"random-plains-windgrass", "content/briefings/brief-random-plains-windgrass-v01.jpg"},
      {"random-rimewater-shrine", "content/briefings/brief-random-rimewater-shrine-v01.jpg"},
      {"random-river-rope-ambush", "content/briefings/brief-random-river-rope-ambush-v01.jpg"},
      {"random-road-broken-milepost", "content/briefings/brief-random-road-broken-milepost-v01.jpg"},
      {"random-road-three-routes-market", "content/briefings/brief-random-road-three-routes-market-v01.jpg"},
      {"random-small-toll-collector", "content/briefings/brief-random-small-toll-collector-v01.jpg"},
      {"random-splintered-trunk-pass", "content/briefings/brief-random-splintered-trunk-pass-v01.jpg"},
      {"random-stoneshelf-pass", "content/briefings/brief-random-stoneshelf-pass-v01.jpg"},
      {"random-walking-campfire", "content/briefings/brief-random-walking-campfire-v01.jpg"},
      {"random-whispering-reeds", "content/briefings/brief-random-whispering-reeds-v01.jpg"},
      {"random-encounter-7", "content/briefings/brief-random-encounter-7-v02.jpg"},
      {"random-encounter-1", "content/briefings/brief-random-amanita-glade-v01.jpg"},
      {"random-encounter-10", "content/briefings/brief-random-amanita-glade-v01.jpg"},
      {"random-encounter-2", "content/briefings/brief-random-hollow-root-den-v01.jpg"},
      {"random-encounter-4", "content/briefings/brief-random-river-rope-ambush-v01.jpg"},
      {"random-encounter-5", "content/briefings/brief-random-road-three-routes-market-v01.jpg"},
      {"random-encounter-6", "content/briefings/brief-random-plains-sunken-ford-v01.jpg"},
      {"random-encounter-8", "content/briefings/brief-random-encounter-7-v01.jpg"},
      {"random-encounter-9", "content/briefings/brief-random-encounter-3-v01.jpg"},
      {"random-forest011-cedar-glade", "content/briefings/brief-random-encounter-13-v01.jpg"},
      {"random-forest011-hemlock-trail", "content/briefings/brief-random-encounter-3-v01.jpg"},
      {"random-forest011-hollow-grove", "content/briefings/brief-random-amanita-glade-v01.jpg"},
      {"random-forest011-needle-crossing", "content/briefings/brief-random-hollow-pine-basin-v01.jpg"},
      {"random-icelands-i1", "content/briefings/brief-random-coldwind-saddle-v01.jpg"},
      {"random-icelands-i2", "content/briefings/brief-random-coldwind-saddle-v01.jpg"},
      {"random-icelands-i3", "content/briefings/brief-random-bell-beneath-ice-v01.jpg"},
      {"random-icelands-i4", "content/briefings/brief-random-icelands-blue-antlers-v01.jpg"},
      {"random-icelands-i6", "content/briefings/brief-random-small-toll-collector-v01.jpg"},
      {"random-icelands-i7", "content/briefings/brief-random-plains-old-stones-grey-v01.jpg"},
      {"random-icelands-i8", "content/briefings/brief-random-bell-beneath-ice-v01.jpg"},
      {"random-icelands-i9", "content/briefings/brief-random-coldwind-saddle-v01.jpg"},
      {"random-icelands-i10", "content/briefings/brief-random-backward-hunt-v01.jpg"},
      {"random-icelands-i12", "content/briefings/brief-random-icelands-last-beacon-v01.jpg"},
      {"random-icelands-i13", "content/briefings/brief-random-small-toll-collector-v01.jpg"},
      {"random-icelands-i14", "content/briefings/brief-random-icelands-blue-antlers-v01.jpg"},
      {"random-photographic-grassland", "content/briefings/brief-random-plains-hill-patrol-v01.jpg"},
      {"random-photographic-meadow", "content/briefings/brief-random-plains-sunken-ford-v01.jpg"},
      {"random-photographic-prairie", "content/briefings/brief-random-plains-windgrass-v01.jpg"}};
  for (const auto& [k, v] : kOwn)
    if (id == k) return std::filesystem::path(v);
  if (id.rfind("random-", 0) == 0) return std::filesystem::path("content/briefings/random-encounter-briefing.jpg");
  const char* art = classicBriefArt(id);
  if (!*art) return {};
  const std::filesystem::path name = std::filesystem::u8path(art).filename();
  std::error_code ec;
  for (const char* folder : {"content/briefings", "content/backdrops"})  // a briefing, or a background used as one (profundezas-bg)
    if (const std::filesystem::path here = std::filesystem::path(folder) / name; std::filesystem::exists(here, ec)) return here;
  return {};
}

}  // namespace ember
