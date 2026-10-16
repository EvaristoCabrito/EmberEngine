#pragma once

// Pieces of Engine2's campaign screens shared by the title and the world map: the save-slot screen (src/title/main.ts
// renderSaveSlots + GameApp's in-game saveSlots), the iron plate button (.ember-plate), a few lucide icons drawn by
// hand, and a campaign mission's map / scene / battle files (Engine2's newest, or the Map Editor's newer save).

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

#include "render/UiArt.h"
#include "world/Campaign.h"

namespace ember {

// What a screen knows how to start: a map with its imported scene and (when exported) its battle.
struct MissionEntry {
  std::string id, title, place;
  std::string draftId, briefing, objective;  // the map's own mission text (Engine2 Mission)
  std::filesystem::path map, scene, battle;
  std::string campaignId;  // a campaign mission (its result goes into the active save slot); empty in test mode
};

struct CampaignPaths {
  std::filesystem::path engine2Public = "C:/Engine2/public";
  std::filesystem::path maps;                                 // Engine2's maps/ember
  std::filesystem::path scenes;                               // build/engine2-scenes (export_scene.ts)
  std::filesystem::path battles = "content/engine2/battles";  // export_battle.ts
  std::filesystem::path test = "build/engine2-test";          // Modo teste's missions (export_test_missions.sh)
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

// Where they go: Engine2's own file format and names (<id><serial>.json), never in Engine2's folder.
inline std::filesystem::path savedMapsDir() { return "content/maps/ember"; }

// The highest serial of `<id>NNN.json` in a folder (Engine2 map-save-plugin nextSerial); 0 when there is none.
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

// A mission's map as the game plays it: the newest saved file of its id (Engine2 mapstore latestSavedDraft), the Map
// Editor's own save when its serial is above the one campaign.json names (Engine2's newest), else Engine2's. `own` says
// which; the draft's own texts replace campaign.json's when it is ours (an edit may have changed them).
struct MissionMap {
  std::filesystem::path file;
  std::string stem;
  bool own = false;
};
inline MissionMap missionMap(const CampaignPaths& paths, const e2::CampaignMission& m) {
  const std::string engine2Stem = std::filesystem::path(m.mapFile).stem().string();
  int engine2Serial = 0;
  if (engine2Stem.size() >= 3) {
    const std::string digits = engine2Stem.substr(engine2Stem.size() - 3);
    if (std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) engine2Serial = std::stoi(digits);
  }
  const int own = highestMapSerial(savedMapsDir(), m.id);
  if (own > engine2Serial) {
    const std::string stem = std::format("{}{:03}", m.id, own);
    return {savedMapsDir() / (stem + ".json"), stem, true};
  }
  return {paths.maps / m.mapFile, engine2Stem, false};
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
inline std::optional<MissionEntry> campaignMission(const e2::Campaign& campaign, const CampaignPaths& paths, const std::string& id, std::string& error) {
  const e2::CampaignMission* m = campaign.data().mission(id);
  if (!m || m->mapFile.empty()) {
    error = "A missão " + id + " não tem mapa salvo no Engine2.";
    return std::nullopt;
  }
  const MissionMap map = missionMap(paths, *m);
  MissionEntry e;
  e.id = map.stem, e.title = m->title, e.place = m->place, e.draftId = m->id, e.briefing = m->briefing, e.objective = m->objective;
  if (map.own) missionTextsFromDraft(e, map.file);
  e.map = map.file;
  e.scene = paths.scenes / (map.stem + ".scene.json");
  std::error_code ec;
  if (!std::filesystem::exists(e.map, ec) || !std::filesystem::exists(e.scene, ec)) {
    error = map.own ? "O mapa " + map.stem + " foi salvo no editor mas ainda não foi preparado para o jogo (salve de novo no editor)."
                    : "O mapa " + map.stem + " (" + m->title + ") ainda não foi importado para o Ember (export_scene.ts).";
    return std::nullopt;
  }
  if (const std::filesystem::path battle = paths.battles / (map.stem + ".battle.json"); std::filesystem::exists(battle, ec)) e.battle = battle;
  e.campaignId = m->id;
  return e;
}

// The same mission as Engine2's Modo teste fights it (all six heroes, level 15, a new save's gear: tools/scripts/engine2/
// export_test_missions.sh for Engine2's maps, the Map Editor's Salvar for its own): its scene and battle from
// build/engine2-test. No campaignId: nothing is written back.
inline std::optional<MissionEntry> testMission(const e2::Campaign& campaign, const CampaignPaths& paths, const std::string& id, std::string& error) {
  const e2::CampaignMission* m = campaign.data().mission(id);
  if (!m || m->mapFile.empty()) {
    error = "A missão " + id + " não tem mapa salvo no Engine2.";
    return std::nullopt;
  }
  const MissionMap map = missionMap(paths, *m);
  MissionEntry e;
  e.id = map.stem, e.title = m->title, e.place = m->place, e.draftId = m->id, e.briefing = m->briefing, e.objective = m->objective;
  if (map.own) missionTextsFromDraft(e, map.file);
  e.map = map.file;
  e.scene = paths.test / "scenes" / (map.stem + ".scene.json");
  e.battle = paths.test / "battles" / (map.stem + ".battle.json");
  std::error_code ec;
  if (!std::filesystem::exists(e.map, ec) || !std::filesystem::exists(e.scene, ec) || !std::filesystem::exists(e.battle, ec)) {
    error = map.own ? "O mapa " + map.stem + " foi salvo no editor mas ainda não foi preparado para o modo teste (salve de novo no editor)."
                    : "O mapa " + map.stem + " (" + m->title + ") ainda não foi importado para o modo teste (export_test_missions.sh).";
    return std::nullopt;
  }
  return e;
}

// Engine2's ember-plate as a button with its own face painted by `paint(drawList, min, max, ink)`.
template <typename Paint>
bool emberPlate(const char* id, ImVec2 size, Paint paint, bool disabled = false) {
  ImGui::BeginDisabled(disabled);
  const bool clicked = ImGui::InvisibleButton(id, size);
  ImGui::EndDisabled();
  const bool hover = !disabled && ImGui::IsItemHovered();
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  ImDrawList* dl = ImGui::GetWindowDrawList();
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

// lucide icons in their 24 px box, drawn at `size` centred on `c`.
struct Lucide {
  ImVec2 origin;
  float scale;
  Lucide(ImVec2 c, float size) : origin{c.x - size * 0.5f, c.y - size * 0.5f}, scale(size / 24.0f) {}
  ImVec2 at(float x, float y) const { return {origin.x + x * scale, origin.y + y * scale}; }
  float stroke() const { return std::max(1.0f, 2.0f * scale); }
};
inline void iconX(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  dl->AddLine(b.at(18, 6), b.at(6, 18), ink, b.stroke());
  dl->AddLine(b.at(6, 6), b.at(18, 18), ink, b.stroke());
}
inline void iconCheck(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  const ImVec2 p[3] = {b.at(20, 6), b.at(9, 17), b.at(4, 12)};
  dl->AddPolyline(p, 3, ink, b.stroke(), ImDrawFlags_None);
}
inline void iconLock(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  dl->AddRect(b.at(3, 11), b.at(21, 22), ink, 2 * b.scale, b.stroke(), ImDrawFlags_None);
  dl->PathArcTo(b.at(12, 7), 5 * b.scale, std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>, 12);
  dl->PathStroke(ink, b.stroke(), ImDrawFlags_None);
  dl->AddLine(b.at(7, 7), b.at(7, 11), ink, b.stroke());
  dl->AddLine(b.at(17, 7), b.at(17, 11), ink, b.stroke());
}
inline void iconMapPin(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  // "M20 10c0 4.993-5.539 10.193-7.399 11.799a1 1 0 0 1-1.202 0C9.539 20.193 4 14.993 4 10a8 8 0 0 1 16 0"
  dl->PathArcTo(b.at(12, 10), 8 * b.scale, std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>, 16);
  dl->PathBezierCubicCurveTo(b.at(20, 15), b.at(14.5f, 20.2f), b.at(12, 22));
  dl->PathBezierCubicCurveTo(b.at(9.5f, 20.2f), b.at(4, 15), b.at(4, 10));
  dl->PathStroke(ink, b.stroke(), ImDrawFlags_None);
  dl->AddCircle(b.at(12, 10), 3 * b.scale, ink, 12, b.stroke());
}
inline void iconBook(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  // BookOpen: two pages meeting at the spine
  const ImVec2 left[5] = {b.at(12, 7), b.at(8, 4), b.at(2, 4), b.at(2, 18), b.at(9, 18)};
  const ImVec2 right[5] = {b.at(12, 7), b.at(16, 4), b.at(22, 4), b.at(22, 18), b.at(15, 18)};
  dl->AddPolyline(left, 5, ink, b.stroke(), ImDrawFlags_None);
  dl->AddPolyline(right, 5, ink, b.stroke(), ImDrawFlags_None);
  dl->AddLine(b.at(9, 18), b.at(12, 21), ink, b.stroke());
  dl->AddLine(b.at(15, 18), b.at(12, 21), ink, b.stroke());
  dl->AddLine(b.at(12, 7), b.at(12, 21), ink, b.stroke());
}
inline void iconClock(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  dl->AddCircle(b.at(12, 12), 10 * b.scale, ink, 24, b.stroke());
  const ImVec2 hands[3] = {b.at(12, 6), b.at(12, 12), b.at(16, 14)};
  dl->AddPolyline(hands, 3, ink, b.stroke(), ImDrawFlags_None);
}
inline void iconGear(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  dl->AddCircle(b.at(12, 12), 3 * b.scale, ink, 12, b.stroke());
  for (int i = 0; i < 8; ++i) {
    const float a = static_cast<float>(i) * std::numbers::pi_v<float> / 4;
    dl->AddLine(b.at(12 + 6.5f * std::cos(a), 12 + 6.5f * std::sin(a)), b.at(12 + 9.5f * std::cos(a), 12 + 9.5f * std::sin(a)), ink, 2.2f * b.stroke());
  }
  dl->AddCircle(b.at(12, 12), 7 * b.scale, ink, 24, b.stroke());
}
inline void iconZoom(ImDrawList* dl, ImVec2 c, float s, ImU32 ink, bool in) {
  const Lucide b(c, s);
  dl->AddCircle(b.at(11, 11), 8 * b.scale, ink, 20, b.stroke());
  dl->AddLine(b.at(21, 21), b.at(16.65f, 16.65f), ink, b.stroke());
  dl->AddLine(b.at(8, 11), b.at(14, 11), ink, b.stroke());
  if (in) dl->AddLine(b.at(11, 8), b.at(11, 14), ink, b.stroke());
}
inline void iconChevronLeft(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) {
  const Lucide b(c, s);
  const ImVec2 p[3] = {b.at(15, 18), b.at(9, 12), b.at(15, 6)};
  dl->AddPolyline(p, 3, ink, b.stroke(), ImDrawFlags_None);
}

// The six save slots (Engine2 title.css .save-slot*): "Nova campanha" (writes a fresh campaign, an occupied slot asks for
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
  int draw(ImVec2 size, float rem, e2::Campaign& campaign, const std::filesystem::path& engine2Public, bool& back, const e2::Json* current = nullptr) {
    back = false;
    int chosen = -1;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{0, 0}, b = size;
    dl->AddRectFilled(a, b, IM_COL32(8, 10, 13, 255));
    if (const UiImage* board = uiImage(engine2Public / "game/ui/old-ember-save-board.png")) imageCover(dl, *board, a, b, 0.5f, 0.5f);
    dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 51));
    const float title = std::clamp(size.x * 0.04f, 1.45f * rem, 1.9f * rem);
    const float headerH = 0.75f * rem + 0.65f * rem * 1.2f + 0.25f * rem + title + 0.3f * rem + 0.78f * rem * 1.35f + 0.75f * rem;
    dl->AddRectFilled(a, {b.x, headerH}, IM_COL32(0, 0, 0, 51));
    dl->AddLine({0, headerH}, {b.x, headerH}, IM_COL32(255, 255, 255, 26));
    const float padX = 1.75f * rem;
    ImGui::SetCursorScreenPos({padX, (headerH - 2.25f * rem) * 0.5f});
    if (emberTextPlate("##slotsback", {5.5f * rem, 2.25f * rem}, "‹  Voltar", 0.8f * rem)) back = true;
    const float hx = padX + 5.5f * rem + 0.75f * rem;
    float hy = 0.75f * rem;
    spacedText(dl, uiFont(UiFont::Ui), 0.65f * rem, {hx, hy}, IM_COL32(184, 154, 112, 255), "ARQUIVOS", 0.18f, false);
    hy += 0.65f * rem * 1.2f + 0.25f * rem;
    const char* heading = mode_ == Mode::Load ? (inGame_ ? "Carregar jogo" : "Carregar campanha") : mode_ == Mode::Save ? "Salvar jogo" : "Nova campanha";
    dl->AddText(uiFont(UiFont::Display), title, {hx, hy + 2.0f}, IM_COL32(0, 0, 0, 255), heading);
    dl->AddText(uiFont(UiFont::Display), title, {hx, hy}, IM_COL32(241, 220, 192, 255), heading);
    hy += title + 0.3f * rem;
    dl->AddText(uiFont(UiFont::Ui), 0.78f * rem, {hx, hy}, IM_COL32(196, 183, 164, 255),
                mode_ == Mode::Load ? "O último usado vem marcado. Escolha uma campanha para continuar."
                                    : "Escolha um espaço. Um espaço ocupado será substituído.");
    const e2::SaveBank& bank = campaign.bank();
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
      const std::optional<e2::Json>& slot = bank.slots[i];
      const bool empty = !e2::Campaign::occupied(slot);
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
      const e2::SlotProgress progress = campaign.progress(slot);
      const std::string name = empty ? "Vazio" : progress.title;
      const std::string detail = empty ? "Nenhuma campanha salva" : progress.detail;
      const std::string date = empty ? std::string() : e2::Campaign::formatSaveDate(slot->value("updatedAt", 0.0));
      const float inner = cardW - 2.0f * rem;
      ImFont* ui = uiFont(UiFont::Ui);
      const ImVec2 detailSize = ui->CalcTextSizeA(0.76f * rem, FLT_MAX, inner, detail.c_str());
      const float detailH = std::min(detailSize.y, 2.0f * 0.76f * rem * 1.3f);
      float h = 0.62f * rem * 1.3f + 0.18f * rem + 0.15f * rem + 1.15f * rem * 1.15f + 0.18f * rem + detailH;
      if (!date.empty()) h += 0.18f * rem + 0.12f * rem + 0.65f * rem * 1.3f;
      if (confirming) h += 0.18f * rem + 0.15f * rem + 0.68f * rem * 1.3f;
      float ty = p.y + std::max(0.75f * rem, (cardH - h) * 0.5f);
      const float tx = p.x + rem;
      spacedText(dl, ui, 0.62f * rem, {tx, ty}, ink(184, 154, 112), std::format("ESPAÇO {}", i + 1).c_str(), 0.15f, false);
      if (last) {
        const float w = spacedTextWidth(ui, 0.55f * rem, "ÚLTIMO USADO", 0.1f);
        spacedText(dl, ui, 0.55f * rem, {q.x - rem - w, ty + 0.05f * rem}, ink(223, 191, 142), "ÚLTIMO USADO", 0.1f, false);
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
        dl->AddText(ui, 0.68f * rem, {tx, ty}, IM_COL32(223, 191, 142, 255), "Toque de novo para substituir.");
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
  bool choose(int index, e2::Campaign& campaign, const e2::Json* current = nullptr) {
    const int number = index + 1;
    overwrite_ = -1;
    if (mode_ == Mode::New) {
      e2::Json save = campaign.emptySave();
      save["pendingMission"] = "vau";
      save["mapMode"] = "rpg";
      if (!campaign.writeSlot(index, std::move(save))) {
        status_ = "Não foi possível gravar o espaço (" + e2::Campaign::savePath().string() + ").";
        return false;
      }
      status_ = std::format("Campanha criada e salva no espaço {}.", number);
      campaign.freshCampaign = true;
      return true;
    }
    if (mode_ == Mode::Save) {
      if (!current || !campaign.writeSlot(index, *current)) {
        status_ = "Não foi possível gravar o espaço.";
        return false;
      }
      status_ = std::format("Campanha salva no espaço {}.", number);
      return true;
    }
    if (!e2::Campaign::occupied(campaign.bank().slots[static_cast<size_t>(index)])) return false;
    if (!campaign.selectSlot(index)) {
      status_ = "Não foi possível selecionar o espaço.";
      return false;
    }
    const e2::SlotProgress p = campaign.progress(campaign.activeSave());
    status_ = std::format("Espaço {} carregado: {} · {}.", number, p.title, p.detail);
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
inline int drawBriefing(ImVec2 size, float rem, const MissionEntry& m, const std::filesystem::path& engine2Public) {
  int result = 0;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 a{0, 0}, b = size;
  dl->AddRectFilled(a, b, IM_COL32(28, 28, 28, 255));
  const char* art = engine2BriefArt(m.draftId);
  if (*art)
    if (const UiImage* image = uiImage(engine2Public / std::filesystem::path(reinterpret_cast<const char8_t*>(art)))) {
      imageCover(dl, *image, a, b, 0.5f, 0.5f);
      verticalGradient(dl, a, b, {{0.0f, IM_COL32(17, 17, 17, 0)}, {0.5f, IM_COL32(17, 17, 17, 115)}, {1.0f, IM_COL32(17, 17, 17, 230)}});
    }
  const float pad = size.x >= 640.0f ? 1.5f * rem : rem;
  ImGui::SetCursorScreenPos({pad, rem});
  if (emberTextPlate("##briefback", {2.5f * rem, 2.5f * rem}, "‹", rem)) result = -1;
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
  if (emberBigButton("##enter", m.battle.empty() ? "ENTRAR" : "ENTRAR EM COMBATE", {pad, size.y - rem - bh}, {width, bh}, true, 1.15f * rem)) result = 1;
  return result;
}

}  // namespace ember
