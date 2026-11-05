#pragma once

// Pieces of Ember's campaign screens shared by the title and the world map: the save-slot screen (src/title/main.ts
// renderSaveSlots + GameApp's in-game saveSlots), the iron plate button (.ember-plate), the lucide icons (Ember's own
// SVGs, rasterised at their size on screen: render/UiArt.h uiIcon), and a campaign mission's map / scene / battle files (Ember's newest, or the Map Editor's newer save).

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <numbers>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "GamePrefs.h"
#include "render/UiArt.h"
#include "world/Campaign.h"

namespace ember {

// Modo teste's reserve (the user, 2026-10-29: "remove Malrec and Selene from the default Party, its getting too heavy to load"; then
// 2026-10-30: all eight in the party, these two on the reserve): the fresh test save's active party is everyone else
// (TestModeStage freshTestSession, "partyActive"); the Party screen moves heroes between the two.
inline constexpr const char* kTestReserve[] = {"Malrec", "Selene"};

// What a screen knows how to start: a map with its imported scene and (when exported) its battle.
struct MissionEntry {
  std::string id, title, place;
  std::string draftId, briefing, objective;  // the map's own mission text (Ember Mission)
  std::filesystem::path map, scene, battle;
  std::string campaignId;  // a campaign mission (its result goes into the active save slot); empty in test mode
  // A campaign mission whose game files are not made yet (or older than its map): made from `source` as the mission opens
  // (MapExport.h Campaign), into `map` / `scene` / `battle`.
  bool prepare = false;
  std::filesystem::path source;
};

struct CampaignPaths {
  std::filesystem::path artRoot = "content/assets";
  std::filesystem::path maps = "content/maps/ember";
  std::filesystem::path scenes = "build/scenes";
  std::filesystem::path battles = "content/data/battles";
  std::filesystem::path campaign = "build/campaign";
};

// CSS text-transform: uppercase, for ASCII and the accented Latin letters (UTF-8 C3 A0-BE).
inline std::string upperUtf8(std::string s) {
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

// --- the maps the Map Editor saves (MapEditorStage.h Salvar) ----------------------------------------------------------

// Where they go: Ember's own file format and names (<id><serial>.json), never in Ember's folder.
inline std::filesystem::path savedMapsDir() { return "content/maps/ember"; }

// The highest serial of `<id>NNN.json` in a folder (Ember map-save-plugin nextSerial); 0 when there is none.
inline int highestMapSerial(const std::filesystem::path& dir, const std::string& id) {
  int best = 0;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return 0;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    const std::string name = e.path().filename().string();
    if (name.size() != id.size() + 8 || !name.starts_with(id) || !name.ends_with(".json")) continue;
    const std::string digits = name.substr(id.size(), 3);
    if (std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) best = std::max(best, std::stoi(digits));
  }
  return best;
}

// A mission's map as the game plays it: the newest saved file of its id (Ember mapstore latestSavedDraft), the Map
// Editor's own save when its serial is above the one campaign.json names (Ember's newest), else Ember's. `own` says
// which; the draft's own texts replace campaign.json's when it is ours (an edit may have changed them).
struct MissionMap {
  std::filesystem::path file;
  std::string stem;
  bool own = false;
};
// The Map Editor's "Ativar" (Ember activeVersions): the serial of an id the campaign plays instead of the newest, in
// content/maps/active.json ({id: serial}); none when absent.
inline std::filesystem::path activeMapsFile() { return "content/maps/active.json"; }
inline std::optional<int> activeMapSerial(const std::string& id) {
  std::ifstream in(activeMapsFile(), std::ios::binary);
  const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
  if (!j.is_object() || !j.contains(id) || !j[id].is_number_integer()) return std::nullopt;
  return j[id].get<int>();
}
inline MissionMap missionMap(const CampaignPaths& paths, const ember::CampaignMission& m) {
  if (const std::optional<int> active = activeMapSerial(m.id)) {
    const std::string stem = std::format("{}{:03}", m.id, *active);
    std::error_code ec;
    if (std::filesystem::exists(savedMapsDir() / (stem + ".json"), ec)) return {savedMapsDir() / (stem + ".json"), stem, true};
    if (std::filesystem::exists(paths.maps / (stem + ".json"), ec)) return {paths.maps / (stem + ".json"), stem, false};
  }
  const std::string stem = std::filesystem::path(m.mapFile).stem().string();
  int serial = 0;
  if (stem.size() >= 3) {
    const std::string digits = stem.substr(stem.size() - 3);
    if (std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) serial = std::stoi(digits);
  }
  const int own = highestMapSerial(savedMapsDir(), m.id);
  if (own > serial || (m.own && own > 0)) {
    const std::string savedStem = std::format("{}{:03}", m.id, own);
    return {savedMapsDir() / (savedStem + ".json"), savedStem, true};
  }
  return {paths.maps / (stem + ".json"), stem, false};
}
// The texts of a mission entry from its map file's draft (title, place, briefing, objective).
inline void missionTextsFromDraft(MissionEntry& e, const std::filesystem::path& mapFile) {
  std::ifstream in(mapFile, std::ios::binary);
  const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
  if (!j.is_object()) return;
  const nlohmann::json& d = j.contains("draft") ? j["draft"] : j;
  auto text = [&](const char* k, std::string& out) {
    if (d.contains(k) && d[k].is_string()) out = d[k].get<std::string>();
  };
  text("title", e.title), text("place", e.place), text("briefing", e.briefing), text("objective", e.objective);
}

// A campaign mission's files: its newest saved map (missionMap), the scene and battle exported from it.
// Empty with `error` saying why when the mission cannot be played here yet.
inline std::optional<MissionEntry> campaignMission(const ember::Campaign& campaign, const CampaignPaths& paths, const std::string& id, std::string& error) {
  const ember::CampaignMission* m = campaign.data().mission(id);
  if (!m || (m->mapFile.empty() && !m->own)) {
    error = "A missão " + id + " não tem mapa local no projeto.";
    return std::nullopt;
  }
  const MissionMap map = missionMap(paths, *m);
  MissionEntry e;
  e.id = map.stem, e.title = m->title, e.place = m->place, e.draftId = m->id, e.briefing = m->briefing, e.objective = m->objective;
  if (map.own) missionTextsFromDraft(e, map.file);
  std::error_code ec;
  if (!std::filesystem::exists(map.file, ec)) {
    error = "O mapa " + map.stem + " (" + m->title + ") não existe (" + map.file.generic_string() + ").";
    return std::nullopt;
  }
  // Every mission in the Locais plays: its game files (the campaign's party on it, its scene, its rules) are made from its map the
  // first time it opens and again whenever the map is newer than them (MapExport.h Campaign); nothing to do by hand.
  e.source = map.file;
  e.map = paths.campaign / "maps" / (map.stem + ".json");
  e.scene = paths.campaign / "scenes" / (map.stem + ".scene.json");
  e.battle = paths.campaign / "battles" / (map.stem + ".battle.json");
  const auto stamp = [&](const std::filesystem::path& p) { return std::filesystem::exists(p, ec) ? std::filesystem::last_write_time(p, ec) : std::filesystem::file_time_type::min(); };
  const auto made = std::min({stamp(e.map), stamp(e.scene), stamp(e.battle)});
  // ... or older than the data they are made with: the rules (a hero added to the party, 2026-10-25: Isolde) and the catalog
  const auto source = std::max({stamp(map.file), stamp("content/ember-classes.json"), stamp("content/data/battle-rules.json"),
                                stamp("content/data/editor-catalog.json")});
  e.prepare = made == std::filesystem::file_time_type::min() || made < source;
  e.campaignId = m->id;
  return e;
}

// An Interlude mission (world/Interlude.h; docs/INTERLUDE.md): its id is a map id in content/maps/ember (the newest serial plays),
// with no entry in campaign.json; its game files are made the same way as any campaign mission's (MapExport.h). `campaignId`
// is the mission id itself (its result goes into the active slot, where the historical party lives while the chapter is on).
inline std::optional<MissionEntry> interludeMission(const CampaignPaths& paths, const std::string& id, std::string& error, bool test = false) {
  const int serial = highestMapSerial(savedMapsDir(), id);
  if (serial <= 0) {
    error = "O mapa do Interlúdio " + id + " não existe em " + savedMapsDir().generic_string() + ".";
    return std::nullopt;
  }
  const std::string stem = std::format("{}{:03}", id, serial);
  MissionEntry e;
  e.id = stem, e.draftId = id, e.title = id;
  const std::filesystem::path file = savedMapsDir() / (stem + ".json");
  missionTextsFromDraft(e, file);
  e.source = file;
  e.map = paths.campaign / "maps" / (stem + ".json");
  e.scene = paths.campaign / "scenes" / (stem + ".scene.json");
  e.battle = paths.campaign / "battles" / (stem + ".battle.json");
  std::error_code ec;
  const auto stamp = [&](const std::filesystem::path& p) { return std::filesystem::exists(p, ec) ? std::filesystem::last_write_time(p, ec) : std::filesystem::file_time_type::min(); };
  const auto made = std::min({stamp(e.map), stamp(e.scene), stamp(e.battle)});
  const auto source = std::max({stamp(file), stamp("content/ember-classes.json"), stamp("content/data/battle-rules.json"), stamp("content/data/editor-catalog.json")});
  e.prepare = made == std::filesystem::file_time_type::min() || made < source;
  e.campaignId = test ? std::string() : id;
  return e;
}

// Test mode opens the campaign's exact prepared map, scene and battle. Its only gameplay differences come from the
// fresh test save (full party at level 30) and the unlocked map picker; it never uses a second export or combat pipeline.
inline std::optional<MissionEntry> campaignMission(const ember::Campaign& campaign, const CampaignPaths& paths, const std::string& id, std::string& error);
inline std::optional<MissionEntry> testMission(const ember::Campaign& campaign, const CampaignPaths& paths, const std::string& id, std::string& error) {
  std::optional<MissionEntry> entry = campaignMission(campaign, paths, id, error);
  if (entry) entry->campaignId.clear();
  return entry;
}

// Ember's ember-plate face (shadow, hover glow, bevel, border) over [a, b]: paint only, no ImGui item.
inline void emberPlateFace(ImDrawList* dl, ImVec2 a, ImVec2 b, bool hover) {
  for (int i = 6; i >= 1; --i) {
    const float spread = 1.5f * static_cast<float>(i);
    dl->AddRectFilled({a.x - spread, a.y + 6.0f - spread}, {b.x + spread, b.y + 6.0f + spread}, IM_COL32(0, 0, 0, 14), 2.0f + spread);
  }
  if (hover)
    for (int i = 5; i >= 1; --i) {
      const float spread = 1.4f * static_cast<float>(i);
      dl->AddRectFilled({a.x - spread, a.y - spread}, {b.x + spread, b.y + spread}, IM_COL32(255, 110, 40, 12), 2.0f + spread);
    }
  dl->AddRect({a.x - 1.0f, a.y - 1.0f}, {b.x + 1.0f, b.y + 1.0f}, IM_COL32(0, 0, 0, 255), 3.0f);
  const ImU32 top = IM_COL32(28, 28, 28, 240), bottom = IM_COL32(10, 10, 10, 242);
  dl->AddRectFilledMultiColor(a, b, top, top, bottom, bottom);
  for (int i = 0; i < 4; ++i) {
    const float d = 1.0f + 2.0f * static_cast<float>(i);
    dl->AddRect({a.x + d, a.y + d}, {b.x - d, b.y - d}, IM_COL32(0, 0, 0, 90 - 20 * i), 1.0f, 2.0f);
  }
  dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 217) : IM_COL32(190, 150, 95, 115), 2.0f);
}
// Ember's ember-plate as a button with its own face painted by `paint(drawList, min, max, ink)`.
template <typename Paint>
bool emberPlate(const char* id, ImVec2 size, Paint paint, bool disabled = false) {
  ImGui::BeginDisabled(disabled);
  const bool clicked = ImGui::InvisibleButton(id, size);
  ImGui::EndDisabled();
  const bool hover = !disabled && ImGui::IsItemHovered();
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  emberPlateFace(dl, a, b, hover);
  const int alpha = disabled ? 102 : 255;
  paint(dl, a, b, hover ? IM_COL32(255, 224, 180, alpha) : IM_COL32(240, 240, 240, alpha));
  return clicked && !disabled;
}
// A plate with a centred label.
inline bool emberTextPlate(const char* id, ImVec2 size, const char* label, float textSize, bool disabled = false) {
  return emberPlate(
      id, size,
      [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
        const ImVec2 t = uiFont(UiFont::Ui)->CalcTextSizeA(textSize, FLT_MAX, 0.0f, label);
        dl->AddText(uiFont(UiFont::Ui), textSize, {std::round((a.x + b.x - t.x) * 0.5f), std::round((a.y + b.y - t.y) * 0.5f)}, ink, label);
      },
      disabled);
}

// lucide icons: `size` = the icon's 24-unit box (Ember's size-4 = 16), centred on `c`.
inline void iconX(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "x", c, s, ink); }
inline void iconCheck(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "check", c, s, ink); }
inline void iconLock(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "lock", c, s, ink); }
inline void iconMapPin(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "map-pin", c, s, ink); }
inline void iconBook(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "book-open", c, s, ink); }
inline void iconClock(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "clock", c, s, ink); }
inline void iconGear(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "settings", c, s, ink); }
inline void iconZoom(ImDrawList* dl, ImVec2 c, float s, ImU32 ink, bool in) { uiIcon(dl, in ? "zoom-in" : "zoom-out", c, s, ink); }
// WorldMapScreen / OverworldMapScreen's zoom control: ember-plate p-1, bottom-4 right-4, Aproximar (44), zoom level (10 px
// text, py-0.5), Afastar (44), gap-1. `zoom` is the stop index (0 = widest); optional labels keep a sliced scale's level numbers.
inline ImVec2 worldMapZoomBoxMin(ImVec2 size) { return {size.x - 16.0f - 52.0f, size.y - 16.0f - 123.0f}; }
inline bool worldMapZoomBoxHovered(ImVec2 size) {
  const ImVec2 m = ImGui::GetIO().MousePos, a = worldMapZoomBoxMin(size);
  return m.x >= a.x && m.x <= a.x + 52.0f && m.y >= a.y && m.y <= a.y + 123.0f;
}
inline bool worldMapZoomBox(ImVec2 size, int& zoom, bool inert = false, int stops = 4, int firstLevel = 0, int levelCount = 0) {
  const ImVec2 a = worldMapZoomBoxMin(size);
  bool changed = false;
  emberPlateFace(ImGui::GetWindowDrawList(), a, {a.x + 52.0f, a.y + 123.0f}, false);  // paint only: an item here would take the buttons' hover
  ImGui::SetCursorScreenPos({a.x + 4.0f, a.y + 4.0f});
  if (emberPlate("##zoomin", {44, 44}, [](ImDrawList* d, ImVec2 p, ImVec2 q, ImU32 ink) { iconZoom(d, {(p.x + q.x) / 2, (p.y + q.y) / 2}, 20, ink, true); }, inert || zoom >= stops - 1))
    zoom = std::min(stops - 1, zoom + 1), changed = true;
  const std::string label = levelCount > 0 ? std::format("{}/{}", firstLevel + zoom, levelCount) : std::format("{}/{}", stops - zoom, stops);
  ImFont* font = uiFont(UiFont::Ui);
  const ImVec2 ts = font->CalcTextSizeA(10.0f, FLT_MAX, 0, label.c_str());
  ImGui::GetWindowDrawList()->AddText(font, 10.0f, {a.x + 26.0f - ts.x / 2, a.y + 4.0f + 44.0f + 4.0f + 9.5f - ts.y / 2}, IM_COL32(180, 180, 180, 255), label.c_str());
  ImGui::SetCursorScreenPos({a.x + 4.0f, a.y + 4.0f + 44.0f + 4.0f + 19.0f + 4.0f});
  if (emberPlate("##zoomout", {44, 44}, [](ImDrawList* d, ImVec2 p, ImVec2 q, ImU32 ink) { iconZoom(d, {(p.x + q.x) / 2, (p.y + q.y) / 2}, 20, ink, false); }, inert || zoom <= 0))
    zoom = std::max(0, zoom - 1), changed = true;
  return changed;
}
inline void iconChevronLeft(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "chevron-left", c, s, ink); }
inline void iconChevronRight(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "chevron-right", c, s, ink); }
// lucide Volume2 / VolumeX (Ember's sound buttons).
inline void iconSound(ImDrawList* dl, ImVec2 c, float s, ImU32 ink, bool on) { uiIcon(dl, on ? "volume-2" : "volume-x", c, s, ink); }

// Ember's sound button (onMute: Volume2 / VolumeX on a plate): flips audio.ts muted, the same switch as Opções'
// Silenciar, saved with the player's choices. `size` = the plate, `icon` = the icon (Ember size-10 + size-4: 40, 16);
// gold: .ember-icon-btn's gold icon (title, briefing) instead of the plate's light one.
// It shows what is heard: off while muted or with Opções' Música at 0; a touch on "off" brings the music back (unmuted, and
// Música back to full when it was at 0: the user, 2026-10-20, the icon "not working" with Música at 0), on "on" mutes.
inline bool soundButton(const char* id, ImVec2 size, float icon, bool gold = false) {
  GamePrefs& prefs = gamePrefs();
  const bool heard = !prefs.muted && prefs.music > 0.01f;
  if (!emberPlate(id, size, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) {
        if (gold && ink == IM_COL32(240, 240, 240, 255)) ink = IM_COL32(223, 191, 142, 255);  // #dfbf8e
        iconSound(d, {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}, icon, ink, heard);
      }))
    return false;
  if (heard) {
    prefs.muted = true;
  } else {
    prefs.muted = false;
    if (prefs.music <= 0.01f) prefs.music = 1.0f;
  }
  prefs.save();
  return true;
}

// The six save slots (Ember title.css .save-slot*): "Nova campanha" (writes a fresh campaign, an occupied slot asks for
// a second touch), "Carregar campanha" (selects a slot) or "Salvar campanha" (writes the running campaign into a slot,
// same overwrite touch). draw() returns the slot chosen once the action is done, or -1.
class SlotScreen {
 public:
  enum class Mode { New, Load, Save };
  // inGame: opened from the game's Options (GameApp's own slot screen: "Carregar jogo" / "Salvar jogo").
  void open(Mode mode, bool inGame = false) {
    mode_ = mode, inGame_ = inGame, overwrite_ = -1, status_.clear();
  }
  void setStatus(std::string text) { status_ = std::move(text); }
  std::string& status() { return status_; }
  Mode mode() const { return mode_; }

  // `back` true when Voltar was pressed. `current` is the running campaign for Save.
  int draw(ImVec2 size, float rem, ember::Campaign& campaign, const std::filesystem::path& artRoot, bool& back, const ember::Json* current = nullptr) {
    back = false;
    int chosen = -1;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{0, 0}, b = size;
    dl->AddRectFilled(a, b, IM_COL32(8, 10, 13, 255));
    if (const UiImage* board = uiImage(artRoot / "game/ui/old-ember-save-board.png")) imageCover(dl, *board, a, b, 0.5f, 0.5f);
    dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 51));
    const float title = std::clamp(size.x * 0.04f, 1.45f * rem, 1.9f * rem);
    const float headerH = 0.75f * rem + 0.65f * rem * 1.2f + 0.25f * rem + title + 0.3f * rem + 0.78f * rem * 1.35f + 0.75f * rem;
    dl->AddRectFilled(a, {b.x, headerH}, IM_COL32(0, 0, 0, 51));
    dl->AddLine({0, headerH}, {b.x, headerH}, IM_COL32(255, 255, 255, 26));
    const float padX = 1.75f * rem;
    ImGui::SetCursorScreenPos({padX, (headerH - 2.25f * rem) * 0.5f});
    if (emberTextPlate("##slotsback", {5.5f * rem, 2.25f * rem}, ("‹  " + tr("Voltar", "Back")).c_str(), 0.8f * rem)) back = true;
    const float hx = padX + 5.5f * rem + 0.75f * rem;
    float hy = 0.75f * rem;
    spacedText(dl, uiFont(UiFont::Ui), 0.65f * rem, {hx, hy}, IM_COL32(184, 154, 112, 255), upperUtf8(tr("Arquivos", "Archives")).c_str(), 0.18f, false);
    hy += 0.65f * rem * 1.2f + 0.25f * rem;
    const std::string heading = mode_ == Mode::Load ? (inGame_ ? tr("Carregar jogo") : tr("Carregar campanha", "Load campaign"))
                                : mode_ == Mode::Save ? tr("Salvar jogo") : tr("Nova campanha", "New campaign");
    dl->AddText(uiFont(UiFont::Display), title, {hx, hy + 2.0f}, IM_COL32(0, 0, 0, 255), heading.c_str());
    dl->AddText(uiFont(UiFont::Display), title, {hx, hy}, IM_COL32(241, 220, 192, 255), heading.c_str());
    hy += title + 0.3f * rem;
    dl->AddText(uiFont(UiFont::Ui), 0.78f * rem, {hx, hy}, IM_COL32(196, 183, 164, 255),
                (mode_ == Mode::Load ? tr("O último usado vem marcado. Escolha uma campanha para continuar.", "The last used slot is marked. Choose a campaign to continue.")
                                     : tr("Escolha um espaço. Um espaço ocupado será substituído.", "Choose a slot. An occupied slot will be overwritten.")).c_str());
    const ember::SaveBank& bank = campaign.bank();
    const int cols = size.x >= 640.0f ? 3 : 2;
    const float gapC = (cols == 3 ? 0.75f : 0.5f) * rem;
    const float listW = std::min(64.0f * rem, size.x - 1.5f * rem);
    const float cardW = (listW - gapC * static_cast<float>(cols - 1)) / static_cast<float>(cols), cardH = 8.0f * rem;
    const int rows = (static_cast<int>(bank.slots.size()) + cols - 1) / cols;
    const float listH = static_cast<float>(rows) * cardH + static_cast<float>(rows - 1) * gapC;
    const float x0 = (size.x - listW) * 0.5f;
    const float top = headerH + std::max(0.75f * rem, (size.y - headerH - listH) * 0.5f);
    const bool any = campaign.hasAnySave();
    for (size_t i = 0; i < bank.slots.size(); ++i) {
      const std::optional<ember::Json>& slot = bank.slots[i];
      const bool empty = !ember::Campaign::occupied(slot);
      const bool last = static_cast<int>(i) == bank.lastSlot && any && !empty;
      const bool confirming = overwrite_ == static_cast<int>(i);
      const bool disabled = mode_ == Mode::Load && empty;
      const int c = static_cast<int>(i) % cols, r = static_cast<int>(i) / cols;
      const ImVec2 p{x0 + static_cast<float>(c) * (cardW + gapC), top + static_cast<float>(r) * (cardH + gapC)};
      const ImVec2 q{p.x + cardW, p.y + cardH};
      ImGui::SetCursorScreenPos(p);
      ImGui::PushID(static_cast<int>(i));
      ImGui::BeginDisabled(disabled);
      const bool clicked = ImGui::InvisibleButton("##slot", {cardW, cardH});
      ImGui::EndDisabled();
      const bool hover = !disabled && ImGui::IsItemHovered();
      ImGui::PopID();
      verticalGradient(dl, p, q, {{0.0f, IM_COL32(12, 12, 12, 230)}, {1.0f, IM_COL32(5, 5, 5, 230)}});
      for (int k = 1; k <= 4; ++k) dl->AddRect({p.x + k, p.y + k}, {q.x - k, q.y - k}, IM_COL32(0, 0, 0, 50), 2.0f);
      if (hover || last)
        for (int k = 3; k >= 1; --k)
          dl->AddRect({p.x - k * 3.0f, p.y - k * 3.0f}, {q.x + k * 3.0f, q.y + k * 3.0f}, IM_COL32(255, 110, 40, hover ? 16 : 13), 4.0f, 3.0f);
      dl->AddRect(p, q, hover ? IM_COL32(226, 170, 100, 191) : last ? IM_COL32(226, 170, 100, 179) : IM_COL32(190, 150, 95, 71), 2.0f);
      if (last) dl->AddRectFilled(p, {p.x + 3.0f, q.y}, IM_COL32(217, 98, 28, 255));
      const int fade = disabled ? 128 : 255;
      auto ink = [&](int rr, int gg, int bb) { return IM_COL32(rr, gg, bb, fade); };
      const ember::SlotProgress progress = campaign.progress(slot);
      const std::string name = empty ? tr("Vazio", "Empty") : progress.title;
      const std::string detail = empty ? tr("Nenhuma campanha salva", "No saved campaign") : progress.detail;
      const std::string date = empty ? std::string() : ember::Campaign::formatSaveDate(slot->value("updatedAt", 0.0));
      const float inner = cardW - 2.0f * rem;
      ImFont* ui = uiFont(UiFont::Ui);
      const ImVec2 detailSize = ui->CalcTextSizeA(0.76f * rem, FLT_MAX, inner, detail.c_str());
      const float detailH = std::min(detailSize.y, 2.0f * 0.76f * rem * 1.3f);
      float h = 0.62f * rem * 1.3f + 0.18f * rem + 0.15f * rem + 1.15f * rem * 1.15f + 0.18f * rem + detailH;
      if (!date.empty()) h += 0.18f * rem + 0.12f * rem + 0.65f * rem * 1.3f;
      if (confirming) h += 0.18f * rem + 0.15f * rem + 0.68f * rem * 1.3f;
      float ty = p.y + std::max(0.75f * rem, (cardH - h) * 0.5f);
      const float tx = p.x + rem;
      spacedText(dl, ui, 0.62f * rem, {tx, ty}, ink(184, 154, 112), std::format("{} {}", upperUtf8(tr("Espaço", "Slot")), i + 1).c_str(), 0.15f, false);
      if (last) {
        const std::string lastUsed = upperUtf8(tr("Último usado", "Last used"));
        const float w = spacedTextWidth(ui, 0.55f * rem, lastUsed.c_str(), 0.1f);
        spacedText(dl, ui, 0.55f * rem, {q.x - rem - w, ty + 0.05f * rem}, ink(223, 191, 142), lastUsed.c_str(), 0.1f, false);
      }
      ty += 0.62f * rem * 1.3f + 0.18f * rem + 0.15f * rem;
      dl->PushClipRect(p, {q.x - rem, q.y}, true);
      dl->AddText(uiFont(UiFont::Display), 1.15f * rem, {tx, ty}, ink(241, 220, 192), name.c_str());
      ty += 1.15f * rem * 1.15f + 0.18f * rem;
      dl->PushClipRect({p.x, ty}, {q.x, ty + detailH}, true);
      dl->AddText(ui, 0.76f * rem, {tx, ty}, ink(196, 183, 164), detail.c_str(), nullptr, inner);
      dl->PopClipRect();
      ty += detailH;
      if (!date.empty()) {
        ty += 0.18f * rem + 0.12f * rem;
        dl->AddText(ui, 0.65f * rem, {tx, ty}, ink(169, 155, 136), date.c_str());
        ty += 0.65f * rem * 1.3f;
      }
      if (confirming) {
        ty += 0.18f * rem + 0.15f * rem;
        dl->AddText(ui, 0.68f * rem, {tx, ty}, IM_COL32(223, 191, 142, 255), tr("Toque de novo para substituir.", "Select again to overwrite.").c_str());
      }
      dl->PopClipRect();
      if (clicked) {
        if (mode_ != Mode::Load && !empty && !confirming) overwrite_ = static_cast<int>(i);
        else if (choose(static_cast<int>(i), campaign, current)) chosen = static_cast<int>(i);
      }
    }
    if (!status_.empty()) {
      const float t = 0.75f * rem;
      const float wrap = size.x - 2.0f * rem;
      const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(t, FLT_MAX, wrap, status_.c_str());
      dl->AddText(uiFont(UiFont::Ui), t, {(size.x - ts.x) * 0.5f, size.y - 0.5f * rem - ts.y}, IM_COL32(224, 189, 128, 255), status_.c_str(), nullptr, wrap);
    }
    return chosen;
  }

  // main.ts chooseSaveSlot (New / Load) and GameApp's save-mode pick (Save). True when the bank was written.
  bool choose(int index, ember::Campaign& campaign, const ember::Json* current = nullptr) {
    const int number = index + 1;
    overwrite_ = -1;
    if (mode_ == Mode::New) {
      ember::Json save = campaign.emptySave();
      save["pendingMission"] = "vau";
      save["mapMode"] = "rpg";
      if (!campaign.writeSlot(index, std::move(save))) {
        status_ = tr("Não foi possível gravar o espaço", "The slot could not be saved") + " (" + ember::Campaign::savePath().string() + ").";
        return false;
      }
      status_ = uiEnglish() ? std::format("Campaign created and saved in slot {}.", number) : std::format("Campanha criada e salva no espaço {}.", number);
      campaign.freshCampaign = true;
      return true;
    }
    if (mode_ == Mode::Save) {
      if (!current || !campaign.writeSlot(index, *current)) {
        status_ = tr("Não foi possível gravar o espaço.", "The slot could not be saved.");
        return false;
      }
      status_ = uiEnglish() ? std::format("Campaign saved in slot {}.", number) : std::format("Campanha salva no espaço {}.", number);
      return true;
    }
    if (!ember::Campaign::occupied(campaign.bank().slots[static_cast<size_t>(index)])) return false;
    if (!campaign.selectSlot(index)) {
      status_ = tr("Não foi possível selecionar o espaço.", "The slot could not be selected.");
      return false;
    }
    const ember::SlotProgress p = campaign.progress(campaign.activeSave());
    status_ = uiEnglish() ? std::format("Slot {} loaded: {} · {}.", number, p.title, p.detail) : std::format("Espaço {} carregado: {} · {}.", number, p.title, p.detail);
    return true;
  }

 private:
  Mode mode_ = Mode::New;
  bool inGame_ = false;
  int overwrite_ = -1;
  std::string status_;
};

// GameApp BriefingScreen: the mission's art, its place and title, the briefing and objective, "Entrar em combate".
// Returns 1 for Entrar, -1 for back, 0 otherwise.
inline int drawBriefing(ImVec2 size, float rem, const MissionEntry& m, const std::filesystem::path& artRoot) {
  int result = 0;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 a{0, 0}, b = size;
  dl->AddRectFilled(a, b, IM_COL32(28, 28, 28, 255));
  if (const std::filesystem::path art = briefArtFile(m.draftId, artRoot); !art.empty())
    if (const UiImage* image = uiImage(art)) {
      imageCover(dl, *image, a, b, 0.5f, 0.5f);
      verticalGradient(dl, a, b, {{0.0f, IM_COL32(17, 17, 17, 0)}, {0.5f, IM_COL32(17, 17, 17, 115)}, {1.0f, IM_COL32(17, 17, 17, 230)}});
    }
  const float pad = size.x >= 640.0f ? 1.5f * rem : rem;
  ImGui::SetCursorScreenPos({pad, rem});
  if (emberTextPlate("##briefback", {2.5f * rem, 2.5f * rem}, "‹", rem)) result = -1;
  ImGui::SetCursorScreenPos({size.x - pad - 2.5f * rem, rem});
  soundButton("##briefsound", {2.5f * rem, 2.5f * rem}, 1.25f * rem, true);
  const float hx = pad + 2.5f * rem + 0.75f * rem;
  const std::string place = upperUtf8(m.place.empty() ? "Ember" : m.place);
  emberKicker(dl, {hx, rem - 0.1f * rem}, 0.875f * rem, place.c_str());
  emberTitle(dl, {hx, rem + 0.95f * rem}, 1.875f * rem, m.title.c_str());
  const float width = std::min(36.0f * rem, size.x - 2.0f * pad);
  const float inner = width - 2.5f * rem;
  ImFont* ui = uiFont(UiFont::Ui);
  const float body = 1.125f * rem;
  const std::string briefing = m.briefing.empty() ? "Sem briefing." : m.briefing;
  const ImVec2 bodySize = ui->CalcTextSizeA(body, FLT_MAX, inner, briefing.c_str());
  const ImVec2 objSize = ui->CalcTextSizeA(rem, FLT_MAX, inner, m.objective.c_str());
  const float panelH = 1.25f * rem + bodySize.y + 1.25f * rem + rem + 0.75f * rem + 0.25f * rem + objSize.y + 1.25f * rem;
  const ImVec2 p{pad, 5.5f * rem}, q{pad + width, 5.5f * rem + panelH};
  emberPanel(dl, p, q);
  float y = p.y + 1.25f * rem;
  dl->AddText(ui, body, {p.x + 1.25f * rem, y}, IM_COL32(240, 240, 240, 255), briefing.c_str(), nullptr, inner);
  y += bodySize.y + 1.25f * rem;
  emberRule(dl, p.x + 1.25f * rem, q.x - 1.25f * rem, y);
  y += rem;
  emberKicker(dl, {p.x + 1.25f * rem, y}, 0.75f * rem, "OBJETIVO");
  y += 0.75f * rem + 0.25f * rem;
  dl->AddText(ui, rem, {p.x + 1.25f * rem, y}, IM_COL32(216, 216, 216, 255), m.objective.c_str(), nullptr, inner);
  const float bh = 3.5f * rem;
  if (emberBigButton("##enter", m.battle.empty() || m.draftId == "estalagem" ? "ENTRAR" : "ENTRAR EM COMBATE", {pad, size.y - rem - bh}, {width, bh}, true, 1.15f * rem)) result = 1;
  return result;
}

}  // namespace ember
