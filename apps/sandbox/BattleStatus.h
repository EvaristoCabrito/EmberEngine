#pragma once

// The character status sheet of a fight, copied from Engine2 (src/game/GameApp.tsx StatusPanel, characterCondition,
// ember-ui/styles.css .status-panel / .status-condition): the portrait with the hunger bar under it, name, class, level,
// XP; the condition badge (and its detail window); the Mochila / Equipar shortcuts; the HP bar; the level-up points
// (three per level, spent on VIT / ATK / MAG / DEF / DEX); the attribute cells; the spells and skills of the unit's class
// with their formulas and uses (content/engine2/status-rows.json, built by export_status_rows.ts with Engine2's own
// functions); the potions carried. The arrows beside the close button browse the units of the fight.

#include <imgui.h>
#include <imgui_internal.h>  // BringWindowToDisplayFront

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "GamePrefs.h"
#include "render/UiArt.h"
#include "world/Battle.h"
#include "world/FieldSpells.h"
#include "world/Spells.h"

namespace ember {

class BattleStatusSheet {
 public:
  // What the sheet asks of the window around it (null = the control is not shown).
  struct Hooks {
    std::function<const UiImage*(const e2::BattleUnit&)> portrait;
    std::function<bool(const e2::BattleUnit&)> portraitWhole;  // the photo in the tall 2:3 box (108 x 162), the party's format
    std::function<const UiImage*(const std::string& kind)> spellIcon;
    std::function<int(int level)> xpToLevel;
    std::function<std::array<int, 5>(const e2::BattleUnit&)> allocation;  // the points spent: hp, atk, mag, def, dex
    std::function<bool(size_t unit, int stat, int delta)> adjust;         // true when the point was moved
    std::function<void(size_t unit)> openPack, openEquip;                  // Mochila / Equipar
    std::function<std::vector<size_t>()> order;                            // the units the arrows browse
    std::function<std::string(const std::string& potionId)> potionTip;     // a potion's tooltip, empty = none
    std::function<std::string()> lockpickTip;
    std::function<std::vector<std::string>(const e2::BattleUnit&, const char* stat)> gearLines;  // the worn pieces behind a stat
  };

  void open(size_t unit) { open_ = true, unit_ = unit, conditionOpen_ = false, skillsOpen_ = false, scrollTarget_ = 0.0f, scrollReset_ = true; }
  void close() { open_ = false, conditionOpen_ = false, skillsOpen_ = false; }
  void openSkills() { skillsOpen_ = true; }  // captures ("skills:<unit>")
  bool isOpen() const { return open_; }
  size_t unit() const { return unit_; }
  void closeConditionOrSheet() {
    if (skillsOpen_) skillsOpen_ = false;
    else if (conditionOpen_) conditionOpen_ = false;
    else open_ = false;
  }

  void draw(const e2::Battle& battle, const std::filesystem::path& publicDir, const Hooks& hooks) {
    if (!open_ || unit_ >= battle.units().size()) return void(open_ = false);
    const bool en = uiEnglish();
    const e2::BattleUnit& u = battle.units()[unit_];
    const ImGuiIO& io = ImGui::GetIO();
    // the veil: a click outside the sheet closes it
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 150));
    ImGui::Begin("##statusveil", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());  // on top, without the focus change that drops a held click
    const bool veilClicked = ImGui::InvisibleButton("##statusveilbtn", io.DisplaySize);
    ImGui::End();
    ImGui::PopStyleColor();
    if (veilClicked) return void(open_ = false);

    const float width = std::min(480.0f, io.DisplaySize.x - 32.0f);  // max-w-[27rem], widened for readable text
    const float maxHeight = io.DisplaySize.y * 0.85f;                // max-h-[85dvh]
    const float height = std::min(maxHeight, contentHeight_ > 0 ? contentHeight_ + 48.0f : maxHeight);
    ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {24, 24});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##statussheet", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                               ImGuiWindowFlags_NoScrollWithMouse);  // its own scrollbar and wheel below
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    wheelScroll();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(3);
    dl->ChannelsSetCurrent(0);
    const ImVec2 wa = ImGui::GetWindowPos(), wb{wa.x + ImGui::GetWindowSize().x, wa.y + ImGui::GetWindowSize().y};
    drawWindowFrame(dl, wa, wb, publicDir);
    dl->ChannelsSetCurrent(1);
    const float inner = width - 48.0f - (scrollable_ ? 14.0f : 0.0f);  // the scrollbar takes its 14 px from the layout
    const ImVec2 top = ImGui::GetCursorScreenPos();
    float y = top.y;
    y = drawHeader(dl, battle, u, top, inner, publicDir, hooks, en);
    y = drawHp(dl, u, top.x, y, inner);
    if (u.side == "player" && hooks.adjust) y = drawPointsBox(dl, u, top.x, y, inner, publicDir, hooks, en);
    y = drawAttributes(dl, u, top.x, y, inner, hooks, en);
    if (u.side == "player") y = drawPotions(dl, u, top.x, y, inner, publicDir, hooks, en);
    ImGui::SetCursorScreenPos({top.x, y});
    ImGui::Dummy({inner, 4.0f});
    contentHeight_ = y - top.y;
    scrollable_ = ImGui::GetScrollMaxY() > 0.0f;
    dl->ChannelsSetCurrent(2);
    if (scrollable_) drawScrollbar(dl, wa, wb);
    dl->ChannelsMerge();
    ImGui::End();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (conditionOpen_) drawConditionWindow(statusCondition(u, en), publicDir);
    if (skillsOpen_ && u.side == "player") drawSkillsWindow(battle, u, publicDir, hooks, en);
  }

 private:
  static constexpr ImU32 kFg = IM_COL32(240, 240, 240, 255), kMuted = IM_COL32(180, 180, 180, 255), kBorder = IM_COL32(74, 74, 74, 255),
                         kAccent = IM_COL32(216, 216, 216, 255), kDanger = IM_COL32(181, 74, 50, 255), kBg = IM_COL32(14, 14, 14, 255),
                         kOk = IM_COL32(150, 200, 130, 255), kWarn = IM_COL32(201, 154, 92, 255), kSky = IM_COL32(125, 211, 252, 255);

  // The sheet's own art (the user's, 2026-10-19: StatusScreenIcons), squared and cropped from Attachments/StatusScreenIcons.
  static constexpr const char *kMochilaIcon = "content/ui/status/mochila-brasa-v01.png", *kEquiparIcon = "content/ui/status/equipar-brasa-v01.png",
                              *kSkillsIcon = "content/ui/status/skills-brasa-v01.png";

  struct Condition {
    std::string title, detail, icon;  // icon: the status art's name
    ImU32 tone;
  };

  // characterCondition: the one condition the sheet names (title, what it does, its picture, its colour).
  static Condition statusCondition(const e2::BattleUnit& u, bool en) {
    auto L = [&](const char* pt, const char* english) { return std::string(en ? english : pt); };
    if (u.stunned) return {L("Atordoado", "Stunned"), L("Atordoamento · perde o próximo turno.", "Stun · loses the next turn."), "stunned", kDanger};
    if (u.asleep) return {L("Dormindo", "Asleep"), L("Sono mágico · perde turnos até acordar ou sofrer dano.", "Magic sleep · loses turns until it wakes or is hurt."), "asleep", kDanger};
    if (u.cond.bleedTurns > 0)
      return {L("Sangrando", "Bleeding"),
              L("Sangramento · sofre dano a cada ação e ao se mover.", "Bleeding · hurt on every action and when moving.") + std::format(" {}D{} · {} {}.", u.cond.bleedDice, u.cond.bleedFaces, L("Restam", "Rounds left:"), u.cond.bleedTurns),
              "bleeding", kDanger};
    if (u.shock)
      return {L("Eletrificado", "Shocked"), L("Choque elétrico · o eco do relâmpago atinge este personagem no início do próximo turno.", "Electric shock · the lightning's echo strikes at the start of the next turn."), "shocked", kDanger};
    if (u.poisoned) {
      const std::string tag = e2::conditionsText(u);
      return {L("Envenenado", "Poisoned"), L("Veneno · sofre dano no início de cada turno. Use Curar Doença ou uma Poção de Curar Doenças para remover. ", "Poison · hurts at the start of every turn. A Cure Disease or a Disease potion removes it. ") + tag, "poisoned", kDanger};
    }
    if (u.diseased) return {L("Doente", "Sick"), L("Doença · todos os atributos ficam 10% menores até receber Curar Doença ou uma Poção de Curar Doenças.", "Disease · every attribute is 10% lower until cured."), "diseased", kDanger};
    if (u.hungerPenaltyPct > 0) {
      const int pct = static_cast<int>(std::round(u.hungerPenaltyPct * 100));
      if (pct >= 90) return {L("Inconsciente", "Unconscious"), std::format("{} (−{}%)", L("Fome extrema · desmaiado, praticamente inútil em combate. Só volta ao normal numa estalagem.", "Extreme hunger · fainted, nearly useless in a fight. Only an inn restores it."), pct), "hungry", kDanger};
      return {L("Com fome", "Hungry"), std::format("{} (−{}%)", L("Fome · vários dias sem comer. Volta ao normal comendo ou numa estalagem.", "Hunger · days without eating. Eating or an inn restores it."), pct), "hungry", kWarn};
    }
    if (const std::string ours = e2::conditionsText(u); !ours.empty()) return {ours, ours, "", IM_COL32(196, 150, 232, 255)};
    return {L("Saudável", "Healthy"), L("Saudável · não há veneno, doença ou fome afetando este personagem.", "Healthy · no poison, disease or hunger affects this character."), "healthy", kOk};
  }

  // An image drawn to fill the box (object-cover) with rounded corners.
  static void coverRounded(ImDrawList* dl, const UiImage& img, ImVec2 a, ImVec2 b, float rounding, ImU32 tint, float fx = 0.5f, float fy = 0.5f) {
    const float bw = b.x - a.x, bh = b.y - a.y, iw = static_cast<float>(img.width), ih = static_cast<float>(img.height);
    const float scale = std::max(bw / iw, bh / ih);
    const float visW = bw / scale / iw, visH = bh / scale / ih;
    const float u0 = (1.0f - visW) * fx, v0 = (1.0f - visH) * fy;
    dl->AddImageRounded(img.texture, a, b, {u0, v0}, {u0 + visW, v0 + visH}, tint, rounding);
  }

  // .ember-window over .status-panel's background: the painted sheet under a 25% wash, a 1 px border, rounded-xl.
  static void drawWindowFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, const std::filesystem::path& publicDir) {
    dl->AddRectFilled({a.x + 4, a.y + 10}, {b.x + 4, b.y + 14}, IM_COL32(0, 0, 0, 90), 14.0f);  // the drop shadow
    dl->AddRectFilled(a, b, IM_COL32(20, 20, 20, 245), 12.0f);
    if (const UiImage* art = uiImage(publicDir / "game/ui/status-background.jpg")) coverRounded(dl, *art, a, b, 12.0f, IM_COL32(255, 255, 255, 230));
    dl->AddRectFilled(a, b, IM_COL32(20, 20, 20, 64), 12.0f);
    dl->AddRect(a, b, kBorder, 12.0f, 1.0f, ImDrawFlags_None);
  }

  static bool iconButton(ImDrawList* dl, const char* id, ImVec2 a, float s, const char* icon) {
    ImGui::SetCursorScreenPos(a);
    const bool hit = ImGui::InvisibleButton(id, {s, s});
    dl->AddRectFilled(a, {a.x + s, a.y + s}, IM_COL32(0, 0, 0, 200), 6.0f);
    dl->AddRect(a, {a.x + s, a.y + s}, ImGui::IsItemHovered() ? kAccent : kBorder, 6.0f);
    uiIcon(dl, icon, {a.x + s / 2, a.y + s / 2}, s * 0.52f, kFg);
    return hit;
  }

  float drawHeader(ImDrawList* dl, const e2::Battle& battle, const e2::BattleUnit& u, ImVec2 top, float inner, const std::filesystem::path& publicDir, const Hooks& hooks, bool en) {
    ImFont* ui = uiFont(UiFont::Ui);
    const bool player = u.side == "player";
    // Portrait (108 x 130, rounded-lg, bordered, black), the hunger bar under it. The party's photos (the user, 2026-10-18) sit in
    // a 2:3 box, 108 x 162: their close-ups whole, nothing cut, every hero's box the same size.
    const UiImage* face = hooks.portrait ? hooks.portrait(u) : nullptr;
    float boxH = 130.0f;
    if (hooks.portraitWhole && hooks.portraitWhole(u)) boxH = 162.0f;
    const ImVec2 pa = top, pb{top.x + 108.0f, top.y + boxH};
    dl->AddRectFilled(pa, pb, IM_COL32(0, 0, 0, 255), 8.0f);
    if (face) coverRounded(dl, *face, pa, pb, 8.0f, IM_COL32_WHITE, 0.5f, 0.3f);
    dl->AddRect(pa, pb, kBorder, 8.0f);
    float bottom = pb.y;
    if (player) {
      const double fullness = u.fullness.value_or(100.0);
      const float by = pb.y + 6.0f;
      dl->AddRectFilled({pa.x, by}, {pb.x, by + 8.0f}, kBorder, 4.0f);
      const float base = static_cast<float>(std::min(100.0, fullness) / 120.0) * 108.0f;
      dl->AddRectFilled({pa.x, by}, {pa.x + base, by + 8.0f}, fullness <= 25 ? kDanger : kAccent, 4.0f);
      if (fullness > 100) dl->AddRectFilled({pa.x + base, by}, {pa.x + base + static_cast<float>((fullness - 100) / 120.0) * 108.0f, by + 8.0f}, IM_COL32(125, 190, 110, 255), 4.0f);
      ImGui::SetCursorScreenPos({pa.x, by - 3});
      ImGui::InvisibleButton("##hunger", {108.0f, 14.0f});
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", std::format("{}: {:.1f}%\n{}", en ? "Satiety" : "Saciedade", fullness, en ? "−50 per day · −2 per action" : "−50 por dia · −2 por ação").c_str());
      bottom = by + 14.0f;
    }
    // Name (display font), class · level, XP bar.
    const float tx = top.x + 120.0f, colRight = top.x + inner;
    const float rightW = 150.0f, rightX = colRight - rightW;
    dl->AddText(uiFont(UiFont::Display), 22.0f, {tx, top.y - 2.0f}, kFg, u.name.c_str(), nullptr, rightX - tx - 8.0f);
    const std::string cls = u.className.empty() ? u.classId : u.className;
    const std::string rank = u.rank.empty() || player ? "" : std::format(" · {} {}", en ? "Rank" : "Classe", u.rank);  // ours (enemy-ranks.json)
    dl->AddText(ui, 15.0f, {tx, top.y + 28.0f}, u.side == "enemy" ? kDanger : kMuted, std::format("{} · {} {}{}", cls, en ? "Lv" : "Nv", u.level, rank).c_str());
    if (player) {
      const float bw = std::min(144.0f, rightX - tx - 10.0f);
      if (u.level >= 30) {
        dl->AddText(ui, 13.0f, {tx, top.y + 52.0f}, kMuted, en ? "Max level" : "Nível máximo");
      } else {
        const int need = hooks.xpToLevel ? hooks.xpToLevel(u.level) : 100;
        dl->AddRectFilled({tx, top.y + 54.0f}, {tx + bw, top.y + 60.0f}, kBorder, 3.0f);
        dl->AddRectFilled({tx, top.y + 54.0f}, {tx + bw * std::clamp(static_cast<float>(u.xp) / static_cast<float>(std::max(need, 1)), 0.0f, 1.0f), top.y + 60.0f}, kAccent, 3.0f);
        dl->AddText(ui, 13.0f, {tx, top.y + 63.0f}, kMuted, std::format("{}/{} XP", u.xp, need).c_str());
      }
    }
    // The right column: ▲ ▼ ✕, the condition badge, Mochila, Equipar.
    {
      const float s = 28.0f;
      float ix = colRight - 3 * s - 12.0f;
      const bool up = iconButton(dl, "##stprev", {ix, top.y}, s, "chevron-up");
      const bool down = iconButton(dl, "##stnext", {ix + s + 6.0f, top.y}, s, "chevron-down");
      if (iconButton(dl, "##stclose", {ix + 2 * (s + 6.0f), top.y}, s, "x")) open_ = false;
      if ((up || down) && hooks.order) {
        const std::vector<size_t> order = hooks.order();
        if (!order.empty()) {
          const auto at = std::find(order.begin(), order.end(), unit_);
          const size_t i = at == order.end() ? 0 : static_cast<size_t>(at - order.begin());
          unit_ = order[(i + (up ? order.size() - 1 : 1)) % order.size()];
        }
      }
      float cy = top.y + s + 10.0f;
      const Condition cond = statusCondition(u, en);
      // The title wraps in the badge ("Grito de Guerra +15% ATK (2)" is two lines): the badge grows to hold it, the buttons below follow.
      const ImVec2 titleSize = ui->CalcTextSizeA(14.0f, FLT_MAX, rightW - 52.0f, cond.title.c_str());
      const float badgeH = std::max(48.0f, 24.0f + titleSize.y + 9.0f);
      const ImVec2 ca{rightX, cy}, cb{rightX + rightW, cy + badgeH};
      ImGui::SetCursorScreenPos(ca);
      if (ImGui::InvisibleButton("##statuscond", {rightW, badgeH})) conditionOpen_ = true;
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", cond.detail.c_str());
      dl->AddRectFilled(ca, cb, IM_COL32(0, 0, 0, 255), 6.0f);
      dl->AddRect(ca, cb, ImGui::IsItemHovered() ? kAccent : cond.tone, 6.0f);
      drawConditionIcon(dl, cond, {ca.x + 8, ca.y + 8}, 32.0f, publicDir);
      dl->AddText(ui, 12.0f, {ca.x + 46, ca.y + 9}, kMuted, upperUtf8(en ? "Condition" : "Condição").c_str());
      dl->AddText(ui, 14.0f, {ca.x + 46, ca.y + 24}, kFg, cond.title.c_str(), nullptr, rightW - 52.0f);
      cy += badgeH + 6.0f;
      auto shortcut = [&](const char* id, const char* iconPath, const std::string& label, const std::function<void(size_t)>& action) {
        if (!action) return;
        const ImVec2 a{rightX, cy}, b{rightX + rightW, cy + 44.0f};
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton(id, {rightW, 44.0f})) action(unit_);
        dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 255), 6.0f);
        dl->AddRect(a, b, ImGui::IsItemHovered() ? kAccent : kBorder, 6.0f);
        if (const UiImage* icon = uiImage(std::filesystem::path(iconPath))) imageContain(dl, *icon, {a.x + 3, a.y + 3}, {a.x + 41, a.y + 41});  // the user's art, 2026-10-19
        dl->AddText(ui, 14.0f, {a.x + 48, a.y + 14}, kFg, label.c_str());
        cy += 44.0f + 6.0f;
      };
      if (player) {
        shortcut("##stpack", kMochilaIcon, en ? "Backpack" : "Mochila", hooks.openPack);
        shortcut("##stequip", kEquiparIcon, en ? "Equip" : "Equipar", hooks.openEquip);
        {  // Skills: the spells, the skills and the trained perícias in a window of their own
          const ImVec2 a{rightX, cy}, b{rightX + rightW, cy + 44.0f};
          ImGui::SetCursorScreenPos(a);
          if (ImGui::InvisibleButton("##stskills", {rightW, 44.0f})) skillsOpen_ = true;
          dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 255), 6.0f);
          dl->AddRect(a, b, ImGui::IsItemHovered() ? kAccent : kBorder, 6.0f);
          if (const UiImage* icon = uiImage(std::filesystem::path(kSkillsIcon))) imageContain(dl, *icon, {a.x + 3, a.y + 3}, {a.x + 41, a.y + 41});
          dl->AddText(ui, 14.0f, {a.x + 48, a.y + 14}, kFg, "Skills");
          cy += 44.0f + 6.0f;
        }
      }
      bottom = std::max(bottom, cy);
    }
    (void)battle;
    return bottom + 10.0f;
  }

  static void drawConditionIcon(ImDrawList* dl, const Condition& c, ImVec2 at, float size, const std::filesystem::path& publicDir) {
    if (c.icon.empty()) return;
    if (c.icon == "hungry") {  // the one drawn as a vector in Engine2
      static std::string svg;
      if (svg.empty()) {
        std::ifstream in(publicDir / "game/icons/status/condition-hungry-001.svg", std::ios::binary);
        svg.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
      }
      if (!svg.empty()) uiIconSvg(dl, "status-hungry", svg, {at.x + size / 2, at.y + size / 2}, size, IM_COL32_WHITE);
      return;
    }
    static const std::pair<const char*, const char*> kFiles[] = {{"stunned", "stun.png"}, {"asleep", "asleep.png"},   {"bleeding", "bleeding.png"},
                                                                 {"shocked", "shocked.png"}, {"poisoned", "poisoned.png"}, {"diseased", "diseased.png"},
                                                                 {"healthy", "healthy.png"}};
    for (const auto& [name, file] : kFiles)
      if (c.icon == name)
        if (const UiImage* img = uiImage(publicDir / "game/icons/status" / file)) imageContain(dl, *img, at, {at.x + size, at.y + size});
  }

  // The condition's own window: the big picture, the title in its colour, what it does.
  void drawConditionWindow(const Condition& c, const std::filesystem::path& publicDir) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 120));
    ImGui::Begin("##condveil", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());  // on top, without the focus change that drops a held click
    if (ImGui::InvisibleButton("##condveilbtn", io.DisplaySize)) conditionOpen_ = false;
    ImGui::End();
    ImGui::PopStyleColor();
    const float w = std::min(320.0f, io.DisplaySize.x - 32.0f);
    ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##condwindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const float inner = w - 40.0f;
    ImFont* ui = uiFont(UiFont::Ui);
    if (iconButton(dl, "##condclose", {top.x + inner - 30.0f, top.y}, 30.0f, "x")) conditionOpen_ = false;
    drawConditionIcon(dl, c, {top.x + inner / 2 - 48.0f, top.y + 4.0f}, 96.0f, publicDir);
    float y = top.y + 112.0f;
    const ImVec2 ts = uiFont(UiFont::Display)->CalcTextSizeA(18.0f, FLT_MAX, 0.0f, c.title.c_str());
    dl->AddText(uiFont(UiFont::Display), 18.0f, {top.x + (inner - ts.x) / 2, y}, c.tone, c.title.c_str());
    y += 28.0f;
    const ImVec2 ds = ui->CalcTextSizeA(14.0f, FLT_MAX, inner, c.detail.c_str());
    dl->AddText(ui, 14.0f, {top.x + 0.0f, y}, kMuted, c.detail.c_str(), nullptr, inner);
    y += ds.y + 6.0f;
    ImGui::SetCursorScreenPos({top.x, y});
    ImGui::Dummy({inner, 4.0f});
    dl->ChannelsSetCurrent(0);
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    drawWindowFrame(dl, a, b, publicDir);
    dl->ChannelsMerge();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }

  float drawHp(ImDrawList* dl, const e2::BattleUnit& u, float x, float y, float inner) {
    ImFont* ui = uiFont(UiFont::Ui);
    const std::string hp = std::format("{}/{}", u.hp, u.maxHp);
    const float hw = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, hp.c_str()).x;
    const float barR = x + inner - hw - 8.0f;
    dl->AddRectFilled({x, y + 2.0f}, {barR, y + 10.0f}, kBorder, 4.0f);
    const float f = u.maxHp > 0 ? std::clamp(static_cast<float>(u.hp) / static_cast<float>(u.maxHp), 0.0f, 1.0f) : 0.0f;
    if (f > 0) dl->AddRectFilled({x, y + 2.0f}, {x + (barR - x) * f, y + 10.0f}, u.side == "enemy" ? kDanger : kAccent, 4.0f);
    dl->AddText(ui, 14.0f, {x + inner - hw, y - 1.0f}, kFg, hp.c_str());
    return y + 28.0f;
  }

  // "Pontos de atributo": the unspent points of the level-ups (3 per level), icon, rule line.
  float drawPointsBox(ImDrawList* dl, const e2::BattleUnit& u, float x, float y, float inner, const std::filesystem::path& publicDir, const Hooks& hooks, bool en) {
    ImFont* ui = uiFont(UiFont::Ui);
    const std::array<int, 5> spent = hooks.allocation ? hooks.allocation(u) : std::array<int, 5>{};
    const int budget = std::max(0, (u.level - 1) * 3), used = spent[0] + spent[1] + spent[2] + spent[3] + spent[4], left = std::max(0, budget - used);
    const ImVec2 a{x, y}, b{x + inner, y + 64.0f};
    dl->AddRectFilled(a, b, kBg, 8.0f);
    dl->AddRect(a, b, IM_COL32(216, 216, 216, 100), 8.0f);
    if (const UiImage* icon = uiImage(publicDir / "game/icons/stat-points-001.png")) imageContain(dl, *icon, {a.x + 12, a.y + 12}, {a.x + 52, a.y + 52});
    dl->AddText(uiFont(UiFont::Ui), 12.0f, {a.x + 64, a.y + 6}, kWarn, upperUtf8(en ? "Attribute points" : "Pontos de atributo").c_str());
    dl->AddText(uiFont(UiFont::Display), 18.0f, {a.x + 64, a.y + 20}, kFg, en ? std::format("{} available", left).c_str() : std::format("{} {}", left, left == 1 ? "disponível" : "disponíveis").c_str());
    dl->AddText(ui, 13.0f, {a.x + 64, a.y + 44}, kMuted, en ? "Earn 3 per level and spend them as you like." : "Ganhe 3 por nível e distribua como quiser.");
    return b.y + 14.0f;
  }

  float drawAttributes(ImDrawList* dl, const e2::BattleUnit& u, float x, float y, float inner, const Hooks& hooks, bool en) {
    ImFont* ui = uiFont(UiFont::Ui);
    sectionTitle(dl, {x, y}, en ? "Attributes" : "Atributos");
    y += 26.0f;
    const std::array<int, 5> spent = hooks.allocation ? hooks.allocation(u) : std::array<int, 5>{};
    const int budget = std::max(0, (u.level - 1) * 3), left = std::max(0, budget - (spent[0] + spent[1] + spent[2] + spent[3] + spent[4]));
    const bool hungry = u.hungerPenaltyPct > 0;
    const Condition cond = statusCondition(u, en);
    struct Cell { const char* label; std::string value; int gear; bool penalised; int stat; };
    const std::string range = u.minRange == u.maxRange ? std::to_string(u.maxRange) : std::format("{}-{}", u.minRange, u.maxRange);
    const Cell cells[8] = {{"VIT", std::to_string(u.maxHp), u.gearHp, hungry, 0},   {"ATK", std::to_string(static_cast<int>(u.atk)), u.gearAtk, hungry, 1},
                           {"MAG", std::to_string(static_cast<int>(u.mag)), u.gearMag, hungry, 2}, {"DEF", std::to_string(static_cast<int>(u.def)), u.gearDef, hungry, 3},
                           {"DEX", std::to_string(static_cast<int>(u.dex)), u.gearDex, hungry, 4}, {"INI", std::to_string(u.initiative), 0, false, -1},
                           {"MOV", u.mov > 0 ? std::to_string(u.mov) : "0", u.gearMov, false, -1}, {en ? "Range" : "Alcance", range, 0, false, -1}};
    const bool controls = u.side == "player" && hooks.adjust;
    const float gap = 6.0f, cw = (inner - 3 * gap) / 4.0f, ch = controls ? 70.0f : 46.0f;
    for (int i = 0; i < 8; ++i) {
      const ImVec2 a{x + (i % 4) * (cw + gap), y + (i / 4) * (ch + gap)};
      ImGui::SetCursorScreenPos(a);
      ImGui::PushID(i);
      ImGui::SetNextItemAllowOverlap();  // the − / + buttons drawn over it must get their clicks
      ImGui::InvisibleButton("##cell", {cw, ch});
      const bool hovered = ImGui::IsItemHovered();
      ImGui::PopID();
      if (hovered && cells[i].penalised) ImGui::SetTooltip("%s", cond.detail.c_str());
      else if (hovered && cells[i].gear != 0) {
        static const char* kStatNames[8] = {"hp", "atk", "mag", "def", "dex", "", "mov", ""};
        std::string tip = en ? "Equipment bonus:" : "Bônus de equipamento:";
        if (hooks.gearLines) {
          for (const std::string& line : hooks.gearLines(u, kStatNames[i])) tip += "\n" + line;
        } else {
          tip += std::format(" {:+d}", cells[i].gear);
        }
        ImGui::SetTooltip("%s", tip.c_str());
      }
      dl->AddRectFilled(a, {a.x + cw, a.y + ch}, kBg, 6.0f);
      dl->AddRect(a, {a.x + cw, a.y + ch}, kBorder, 6.0f);
      const ImU32 ink = cells[i].penalised ? kDanger : cells[i].gear != 0 ? kSky : kFg;
      const ImVec2 ls = ui->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, cells[i].label);
      dl->AddText(ui, 11.0f, {a.x + (cw - ls.x) / 2, a.y + 5.0f}, kMuted, cells[i].label);
      const ImVec2 vs = ui->CalcTextSizeA(17.0f, FLT_MAX, 0.0f, cells[i].value.c_str());
      dl->AddText(ui, 17.0f, {a.x + (cw - vs.x) / 2, a.y + 18.0f}, ink, cells[i].value.c_str());
      if (controls && cells[i].stat >= 0) {  // − +N +
        const int stat = cells[i].stat;
        const float by = a.y + 42.0f, bs = 20.0f, mid = a.x + cw / 2;
        const bool canRemove = spent[static_cast<size_t>(stat)] > 0, canAdd = left > 0;
        ImGui::PushID(100 + i);
        ImGui::BeginDisabled(!canRemove);
        ImGui::SetCursorScreenPos({mid - 30.0f, by});
        const bool minus = ImGui::InvisibleButton("##minus", {bs, bs});
        ImGui::EndDisabled();
        dl->AddRectFilled({mid - 30.0f, by}, {mid - 30.0f + bs, by + bs}, IM_COL32(0, 0, 0, 200), 4.0f);
        dl->AddRect({mid - 30.0f, by}, {mid - 30.0f + bs, by + bs}, kBorder, 4.0f);
        dl->AddText(ui, 17.0f, {mid - 30.0f + 5.0f, by + 0.5f}, canRemove ? kFg : IM_COL32(240, 240, 240, 90), "−");
        ImGui::BeginDisabled(!canAdd);
        ImGui::SetCursorScreenPos({mid + 10.0f, by});
        const bool plus = ImGui::InvisibleButton("##plus", {bs, bs});
        ImGui::EndDisabled();
        dl->AddRectFilled({mid + 10.0f, by}, {mid + 10.0f + bs, by + bs}, IM_COL32(0, 0, 0, 200), 4.0f);
        dl->AddRect({mid + 10.0f, by}, {mid + 10.0f + bs, by + bs}, kBorder, 4.0f);
        dl->AddText(ui, 17.0f, {mid + 10.0f + 4.5f, by + 0.5f}, canAdd ? kFg : IM_COL32(240, 240, 240, 90), "+");
        ImGui::PopID();
        const std::string pts = std::format("+{}", spent[static_cast<size_t>(stat)]);
        const ImVec2 ps = ui->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, pts.c_str());
        dl->AddText(ui, 12.0f, {mid - ps.x / 2, by + 3.0f}, kAccent, pts.c_str());
        if (minus) hooks.adjust(unit_, stat, -1);
        if (plus) hooks.adjust(unit_, stat, +1);
      }
    }
    return y + 2 * (ch + gap) + 6.0f;
  }

  // content/engine2/status-rows.json: the rows per rules class and level.
  static const e2::Json& rowsTable() {
    static const e2::Json table = [] {
      std::ifstream in("content/engine2/status-rows.json", std::ios::binary);
      e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
      return j.is_object() ? j : e2::Json::object();
    }();
    return table;
  }

  static std::string diceText(int dice, int faces, int bonus) {
    if (dice <= 0) return {};
    return bonus ? std::format("{}D{}+{}", dice, faces, bonus) : std::format("{}D{}", dice, faces);
  }

  struct SpellRow {
    std::string kind, text;  // text: the sheet's own line (a skill's formula) or the spell's numbers at this level
    int tier = 0, uses = 0;
    std::string title, about, note;  // its name; how it works; where it is on its scaling track
  };

  // The class's rows of spells and skills for the unit's level (status-rows.json), then the ones this port has that the sheet of the
  // class does not list, by tier.
  static std::vector<SpellRow> spellRows(const e2::BattleUnit& u, bool en) {
    std::string key = e2::FieldSpells::rulesClass(u.classId);
    if (key == "aldric") key = "lancer";
    if (key == "healer" && u.name == "Salazar") key = "healer:Salazar";
    std::vector<SpellRow> rows;
    const e2::Json& table = rowsTable();
    if (table.contains("rows") && table["rows"].contains(key) && table["rows"][key].contains(std::to_string(std::clamp(u.level, 1, 30)))) {
      for (const e2::Json& r : table["rows"][key][std::to_string(std::clamp(u.level, 1, 30))]) {
        SpellRow row;
        row.kind = r.value("kind", "");
        row.tier = r.contains("tier") && r["tier"].is_number() ? r["tier"].get<int>() : 0;
        if (const int ours = e2::spellTierNumber(row.kind); ours > 0) row.tier = ours;  // ours (Spells.cpp unitTierOf): Investida T3, Familiar Titã T5, Cão Zumbi T6
        // the Cão Zumbi is the Necromante's (grantClassSpells): a hero without it has no line for it
        if (row.kind == "summonZombieDog" && u.side == "player" && std::find(u.spellList.begin(), u.spellList.end(), row.kind) == u.spellList.end()) continue;
        row.uses = row.tier >= 1 && row.tier <= 10 ? u.spells[row.tier - 1] : 0;
        if (r.value("uses", false) && row.uses <= 0) continue;
        std::string text = tr(r.value("text", ""));
        // a first spell's line from its own numbers at this level (its track), else the sheet's own line
        const std::string scaled = e2::spellScaleText(u, row.kind, en);
        if (!scaled.empty()) text = scaled;
        if (scaled.empty() && r.contains("mag") && r["mag"].is_object()) {
          const e2::Json& m = r["mag"];
          const double mul = m.value("mul", 1.0);
          const int dice = m.value("dice", 0), faces = m.value("faces", 0), bonus = m.value("bonus", 0);
          const long long total = static_cast<long long>(std::floor(std::floor(u.mag / 2.0) * mul));
          const std::string roll = diceText(dice, faces, bonus);
          // what it does, in words: "· dano 39 + 1D10" (the MAG formula behind the number is left out: unreadable here)
          while (!text.empty() && (text.back() == ' ' || text.ends_with("·"))) text.erase(text.size() - (text.back() == ' ' ? 1 : std::string("·").size()));
          const bool heals = row.kind.starts_with("cure");
          const char* what = heals ? (en ? "heals" : "cura") : (en ? "damage" : "dano");
          text += std::format(" · {} {}{}", what, total, roll.empty() ? "" : " + " + roll);
        }
        if (scaled.empty()) text += r.value("after", std::string());
        if (r.contains("lockLevel") && r["lockLevel"].is_number() && u.level < r["lockLevel"].get<int>()) {
          text += std::format(" · {} {}+", en ? "level" : "nível", r["lockLevel"].get<int>());
          row.uses = 0;
        }
        row.text = std::move(text);
        rows.push_back(std::move(row));
      }
    }
    for (const std::string& kind : u.spellList) {
      if (std::any_of(rows.begin(), rows.end(), [&](const SpellRow& r) { return r.kind == kind; })) continue;
      const int tier = e2::spellTierNumber(kind);
      // ours: their numbers and what they do (spellTooltip, after its tier · element line)
      std::string about = e2::spellTooltip(u, kind);
      rows.push_back({kind, about, tier, tier >= 1 && tier <= 10 ? u.spells[tier - 1] : 0});
    }
    for (SpellRow& r : rows) {
      r.title = tr(e2::spellName(r.kind));
      r.about = e2::spellExplain(r.kind, en);
      r.note = e2::spellTrackNote(r.kind, u.level, en);
      if (r.text.empty() || r.text == r.title) r.text.clear();
    }
    std::stable_sort(rows.begin(), rows.end(), [](const SpellRow& a, const SpellRow& b) { return (a.tier ? a.tier : 99) < (b.tier ? b.tier : 99); });
    return rows;
  }

  static std::string pointsText(double v, bool en) {
    std::string t = std::format("{:.2f}", v);
    while (t.back() == '0') t.pop_back();
    if (t.back() == '.') t.pop_back();
    if (!en) std::replace(t.begin(), t.end(), '.', ',');
    return t;
  }

  static std::string skillName(const std::string& id, bool en) {
    static const std::map<std::string, std::pair<const char*, const char*>> kNames = {
        {"healing", {"Cura", "Healing"}},
        {"dodging", {"Esquiva", "Dodging"}},
        {"swordWeapon", {"Espadas", "Swords"}},
        {"axeWeapon", {"Machados", "Axes"}},
        {"maceWeapon", {"Maças", "Maces"}},
        {"hammerWeapon", {"Martelos", "Hammers"}},
        {"staffWeapon", {"Cajados", "Staves"}},
        {"spearWeapon", {"Lanças e armas de haste", "Spears & Polearms"}},
        {"bowWeapon", {"Arcos", "Bows"}},
        {"crossbowWeapon", {"Bestas", "Crossbows"}},
        {"daggerWeapon", {"Adagas", "Daggers"}},
        {"fireResistance", {"Resistência a Fogo", "Fire Resistance"}},
        {"lightningResistance", {"Resistência a Raio", "Lightning Resistance"}},
        {"iceResistance", {"Resistência a Gelo", "Ice Resistance"}},
        {"arcaneResistance", {"Resistência Arcana", "Arcane Resistance"}},
        {"darknessResistance", {"Resistência a Trevas", "Darkness Resistance"}},
        {"holyResistance", {"Resistência Sagrada", "Holy Resistance"}},
        {"poisonResistance", {"Resistência a Veneno", "Poison Resistance"}},
        {"emberResistance", {"Resistência a Brasa", "Ember Resistance"}}};
    const auto it = kNames.find(id);
    return it == kNames.end() ? id : (en ? it->second.second : it->second.first);
  }

  // The Skills window (the world map's Skills in the same plates): the spells and skills as cards, then the trained perícias with
  // their bars. A veil behind it; a click outside, ✕ or Esc closes it.
  void drawSkillsWindow(const e2::Battle& battle, const e2::BattleUnit& u, const std::filesystem::path& publicDir, const Hooks& hooks, bool en) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 150));
    ImGui::Begin("##skillsveil", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    if (ImGui::InvisibleButton("##skillsveilbtn", io.DisplaySize)) skillsOpen_ = false;
    ImGui::End();
    ImGui::PopStyleColor();
    const float w = std::min(720.0f, io.DisplaySize.x - 32.0f), h = io.DisplaySize.y * 0.86f;
    ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {24, 24});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##skillswindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    ImFont* ui = uiFont(UiFont::Ui);
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const float inner = w - 48.0f;
    const std::string kicker = upperUtf8(std::format("{} · {} {}", u.name, en ? "Lv" : "Nv", u.level));
    if (const UiImage* icon = uiImage(std::filesystem::path(kSkillsIcon))) imageContain(dl, *icon, {top.x, top.y - 2.0f}, {top.x + 52.0f, top.y + 50.0f});
    dl->AddText(ui, 12.0f, {top.x + 62.0f, top.y}, kMuted, kicker.c_str());
    dl->AddText(uiFont(UiFont::Display), 26.0f, {top.x + 62.0f, top.y + 16.0f}, IM_COL32(224, 189, 128, 255), "Skills");
    if (iconButton(dl, "##skillsclose", {top.x + inner - 30.0f, top.y}, 30.0f, "x")) skillsOpen_ = false;
    const float bodyTop = 62.0f, bodyH = h - 48.0f - bodyTop;
    ImGui::SetCursorScreenPos({top.x, top.y + bodyTop});
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(176, 128, 72, 255));
    ImGui::BeginChild("##skillsbody", {inner, bodyH}, 0, ImGuiWindowFlags_NoBackground);
    {
      ImDrawList* bd = ImGui::GetWindowDrawList();
      const float bw = inner - 14.0f;  // the scrollbar's share
      const float x0 = ImGui::GetCursorScreenPos().x;
      float y = ImGui::GetCursorScreenPos().y;
      // --- Magias e habilidades: a card a row (the user, 2026-10-20: the descriptions here, in a larger font, explained well): icon,
      // name, tier and uses, how it works, its numbers at this level, its scaling track ---
      sectionTitle(bd, {x0, y}, en ? "Spells and skills" : "Magias e habilidades");
      y += 28.0f;
      const std::vector<SpellRow> rows = spellRows(u, en);
      constexpr float kTitle = 19.0f, kBody = 16.0f, kNote = 14.0f;
      const float gap = 10.0f, cardW = bw, iconS = 56.0f, textW = cardW - 14.0f - iconS - 14.0f - 16.0f;
      ImFont* title = uiFont(UiFont::Display);
      for (const SpellRow& r : rows) {
        const float aboutH = r.about.empty() ? 0.0f : ui->CalcTextSizeA(kBody, FLT_MAX, textW, r.about.c_str()).y + 6.0f;
        const float textH = r.text.empty() ? 0.0f : ui->CalcTextSizeA(kBody, FLT_MAX, textW, r.text.c_str()).y + 6.0f;
        const float noteH = r.note.empty() ? 0.0f : ui->CalcTextSizeA(kNote, FLT_MAX, textW, r.note.c_str()).y + 4.0f;
        const float cardH = std::max(iconS + 24.0f, 12.0f + kTitle + 8.0f + aboutH + textH + noteH + 12.0f);
        const ImVec2 a{x0, y}, b{x0 + cardW, y + cardH};
        const bool dim = r.tier > 0 && r.uses <= 0;
        bd->AddRectFilled(a, b, kBg, 8.0f);
        bd->AddRect(a, b, kBorder, 8.0f);
        const ImVec2 ia{a.x + 14.0f, a.y + 12.0f}, ib{ia.x + iconS, ia.y + iconS};
        bd->AddRectFilled(ia, ib, IM_COL32(0, 0, 0, 255), 6.0f);
        if (const UiImage* icon = hooks.spellIcon ? hooks.spellIcon(r.kind) : nullptr) coverRounded(bd, *icon, ia, ib, 6.0f, dim ? IM_COL32(255, 255, 255, 110) : IM_COL32_WHITE);
        bd->AddRect(ia, ib, kBorder, 6.0f);
        const float tx = ib.x + 14.0f;
        float ty = a.y + 12.0f;
        bd->AddText(title, kTitle, {tx, ty}, dim ? kMuted : IM_COL32(224, 189, 128, 255), r.title.c_str());
        if (r.tier > 0) {
          const std::string uses = std::format("Tier {} · ×{}", r.tier, r.uses);
          const ImVec2 us = ui->CalcTextSizeA(kNote, FLT_MAX, 0.0f, uses.c_str());
          bd->AddText(ui, kNote, {b.x - 14.0f - us.x, ty + 3.0f}, r.uses > 0 ? kAccent : kDanger, uses.c_str());
        }
        ty += kTitle + 8.0f;
        if (!r.about.empty()) bd->AddText(ui, kBody, {tx, ty}, dim ? kMuted : kFg, r.about.c_str(), nullptr, textW), ty += aboutH;
        if (!r.text.empty()) bd->AddText(ui, kBody, {tx, ty}, dim ? kMuted : kAccent, r.text.c_str(), nullptr, textW), ty += textH;
        if (!r.note.empty()) bd->AddText(ui, kNote, {tx, ty}, kMuted, r.note.c_str(), nullptr, textW);
        y += cardH + gap;
      }
      if (rows.empty()) {
        bd->AddText(ui, 14.0f, {x0, y}, kMuted, en ? "This character has no spells or skills yet." : "Este personagem ainda não tem magias nem habilidades.");
        y += 24.0f;
      }
      // --- Perícias: every skill the sheet owes, a bar each (the weapon's, the resistances, Cura, Esquiva) ---
      y += 10.0f;
      sectionTitle(bd, {x0, y}, en ? "Trained skills" : "Perícias");
      y += 28.0f;
      auto trained = [&](const std::string& id) {
        for (const auto& [k, v] : u.skills)
          if (k == id) return v;
        return 0.0;
      };
      std::vector<std::pair<std::string, double>> skills;
      auto add = [&](const std::string& id, double v) {
        if (std::none_of(skills.begin(), skills.end(), [&](const auto& e) { return e.first == id; })) skills.push_back({id, v});
      };
      if (u.trainsWeapon) {  // the weapon its class trains
        static const std::vector<std::pair<std::string, std::string>> kWeaponSkill = {
            {"sword", "swordWeapon"}, {"blade", "swordWeapon"}, {"katana", "swordWeapon"}, {"scimitar", "swordWeapon"}, {"axe", "axeWeapon"},
            {"mace", "maceWeapon"},   {"hammer", "hammerWeapon"}, {"staff", "staffWeapon"}, {"spear", "spearWeapon"},    {"lance", "spearWeapon"},
            {"pike", "spearWeapon"},  {"polearm", "spearWeapon"}, {"halberd", "spearWeapon"}, {"bow", "bowWeapon"},      {"crossbow", "crossbowWeapon"},
            {"dagger", "daggerWeapon"}};
        for (const auto& [type, id] : kWeaponSkill)
          if (type == u.weaponType) add(id, std::max(u.weaponSkill, trained(id)));
      }
      for (const auto& [id, v] : u.skills)
        if (id.ends_with("Weapon")) add(id, v);
      for (const char* id : {"poisonResistance", "fireResistance", "lightningResistance", "iceResistance", "arcaneResistance", "darknessResistance",
                             "holyResistance", "emberResistance"})
        add(id, trained(id));
      if (!u.summoned) {
        add("healing", std::max(u.healingSkill, trained("healing")));
        add("dodging", trained("dodging"));
      }
      const ImVec2 pa{x0, y};
      const float panelH = 14.0f + static_cast<float>(skills.size()) * 40.0f;
      bd->AddRectFilled(pa, {pa.x + bw, pa.y + panelH}, kBg, 8.0f);
      bd->AddRect(pa, {pa.x + bw, pa.y + panelH}, kBorder, 8.0f);
      float sy = pa.y + 10.0f;
      for (const auto& [id, v] : skills) {
        const std::string value = std::format("{}/100", pointsText(v, en));
        const float vw = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, value.c_str()).x;
        std::string name = skillName(id, en);
        if (id == "healing") name += en ? std::format(" · +{}% healing", pointsText(v, en)) : std::format(" · +{}% de cura", pointsText(v, en));
        if (id == "dodging") name += en ? std::format(" · -{}% to be hit", pointsText(v / 2, en)) : std::format(" · -{}% de acerto contra", pointsText(v / 2, en));
        bd->AddText(ui, 14.0f, {pa.x + 14.0f, sy}, kFg, name.c_str());
        bd->AddText(ui, 14.0f, {pa.x + bw - 14.0f - vw, sy}, kMuted, value.c_str());
        bd->AddRectFilled({pa.x + 14.0f, sy + 22.0f}, {pa.x + bw - 14.0f, sy + 28.0f}, kBorder, 3.0f);
        bd->AddRectFilled({pa.x + 14.0f, sy + 22.0f}, {pa.x + 14.0f + (bw - 28.0f) * static_cast<float>(std::clamp(v / 100.0, 0.0, 1.0)), sy + 28.0f}, kAccent, 3.0f);
        sy += 40.0f;
      }
      y += panelH + 12.0f;
      // --- Afinidades: this character's score with every other member of the party in the fight (as the world map's Party shows them) ---
      std::vector<const e2::BattleUnit*> mates;
      for (const e2::BattleUnit& o : battle.units())
        if (o.side == "player" && !o.summoned && o.name != u.name &&
            std::none_of(mates.begin(), mates.end(), [&](const e2::BattleUnit* m) { return m->name == o.name; }))
          mates.push_back(&o);
      if (!mates.empty()) {
        y += 10.0f;
        sectionTitle(bd, {x0, y}, en ? "Affinities" : "Afinidades");
        y += 28.0f;
        const float affH = 14.0f + static_cast<float>(mates.size()) * 56.0f;
        bd->AddRectFilled({x0, y}, {x0 + bw, y + affH}, kBg, 8.0f);
        bd->AddRect({x0, y}, {x0 + bw, y + affH}, kBorder, 8.0f);
        float ay = y + 10.0f;
        for (const e2::BattleUnit* o : mates) {
          const std::string pair = u.name < o->name ? u.name + "|" + o->name : o->name + "|" + u.name;
          const auto it = battle.affinityScores().find(pair);
          const double score = it == battle.affinityScores().end() ? 0.0 : it->second;
          const char* grade = score < 0 ? (score <= -90 ? (en ? "Hatred" : "Ódio") : score <= -50 ? (en ? "Hostile" : "Hostil") : score <= -25 ? (en ? "Strained" : "Tensa") : (en ? "Cold" : "Fria"))
                                        : (score >= 100 ? (en ? "High" : "Alta") : score >= 80 ? (en ? "Good" : "Boa") : score >= 50 ? (en ? "Fair" : "Média") : score >= 25 ? (en ? "Regular" : "Regular") : (en ? "Poor" : "Ruim"));
          const int bonus = score >= 90 ? 8 : score >= 50 ? 5 : score >= 25 ? 2 : score <= -90 ? -8 : score <= -50 ? -5 : score <= -25 ? -2 : 0;
          bd->AddText(ui, 14.0f, {x0 + 14.0f, ay}, kFg, o->name.c_str());
          const std::string right = std::format("{} · {}/100", grade, pointsText(score, en));
          const float rw = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, right.c_str()).x;
          bd->AddText(ui, 14.0f, {x0 + bw - 14.0f - rw, ay}, kMuted, right.c_str());
          bd->AddRectFilled({x0 + 14.0f, ay + 22.0f}, {x0 + bw - 14.0f, ay + 28.0f}, kBorder, 3.0f);
          bd->AddRectFilled({x0 + 14.0f, ay + 22.0f}, {x0 + 14.0f + (bw - 28.0f) * static_cast<float>(std::clamp(std::abs(score), 0.0, 100.0) / 100.0), ay + 28.0f},
                            score < 0 ? IM_COL32(196, 58, 46, 255) : kAccent, 3.0f);
          const std::string help = bonus < 0 ? std::format("{}: {}%", en ? "Side-by-side penalty" : "Penalidade por adjacência", bonus)
                                             : std::format("{}: +{}%{}", en ? "Side-by-side bonus" : "Bônus por adjacência", bonus,
                                                           score >= 80 ? (en ? " · Duo skill unlocked" : " · Skill de dupla desbloqueada") : "");
          bd->AddText(ui, 12.0f, {x0 + 14.0f, ay + 32.0f}, kMuted, help.c_str());
          ay += 56.0f;
        }
        y += affH + 12.0f;
      }
      ImGui::SetCursorScreenPos({x0, y});
      ImGui::Dummy({bw, 4.0f});
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    dl->ChannelsSetCurrent(0);
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    drawWindowFrame(dl, a, b, publicDir);
    dl->ChannelsMerge();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }

  float drawPotions(ImDrawList* dl, const e2::BattleUnit& u, float x, float y, float inner, const std::filesystem::path& publicDir, const Hooks& hooks, bool en) {
    ImFont* ui = uiFont(UiFont::Ui);
    sectionTitle(dl, {x, y}, en ? "Potions" : "Poções");
    y += 26.0f;
    static constexpr int kCarryMax[7] = {5, 5, 5, 5, 5, 5, 5};  // POTION_CARRY_MAX (a pouch raises it; the bag's cap is Engine2's)
    // two per row: the name on top (one line, cut with … if it must), "count · dice" under it
    constexpr int kCols = 2;
    const float gap = 6.0f, cw = (inner - (kCols - 1) * gap) / kCols, ch = 40.0f, textW = cw - 44.0f;
    auto fit = [&](std::string s, float size) {
      if (ui->CalcTextSizeA(size, FLT_MAX, 0.0f, s.c_str()).x <= textW) return s;
      while (!s.empty() && ui->CalcTextSizeA(size, FLT_MAX, 0.0f, (s + "…").c_str()).x > textW) {
        do s.pop_back(); while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80);  // whole UTF-8 characters
        if (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0xC0) s.pop_back();
      }
      return s + "…";
    };
    int n = 0;
    for (int k = 0; k < 7; ++k) {
      if (u.bag[k] <= 0) continue;
      const ImVec2 a{x + (n % kCols) * (cw + gap), y + (n / kCols) * (ch + gap)};
      ImGui::SetCursorScreenPos(a);
      ImGui::PushID(200 + k);
      ImGui::InvisibleButton("##potion", {cw, ch});
      ImGui::PopID();
      if (ImGui::IsItemHovered() && hooks.potionTip)
        if (const std::string tip = hooks.potionTip(e2::kPotionIds[k]); !tip.empty()) ImGui::SetTooltip("%s", tip.c_str());
      dl->AddRectFilled(a, {a.x + cw, a.y + ch}, kBg, 6.0f);
      dl->AddRect(a, {a.x + cw, a.y + ch}, kBorder, 6.0f);
      const std::string file = std::format("game/icons/potion-{}.png", e2::kPotionIds[k]);
      if (const UiImage* icon = uiImage(publicDir / file)) imageContain(dl, *icon, {a.x + 8, a.y + 8}, {a.x + 32, a.y + 32});
      // potionLabel = "name dice": the dice go to the second line
      std::string name = e2::potionName(e2::kPotionIds[k]), dice;
      if (const size_t sp = name.rfind(' '); sp != std::string::npos && name.find('D', sp) != std::string::npos &&
                                             std::isdigit(static_cast<unsigned char>(name[sp + 1])))
        dice = name.substr(sp + 1), name.resize(sp);
      dl->AddText(ui, 14.0f, {a.x + 40, a.y + 5}, kFg, fit(name, 14.0f).c_str());
      const std::string sub = dice.empty() ? std::format("{}/{}", u.bag[k], kCarryMax[k]) : std::format("{}/{} · {}", u.bag[k], kCarryMax[k], dice);
      dl->AddText(ui, 13.0f, {a.x + 40, a.y + 23}, kMuted, sub.c_str());
      ++n;
    }
    if (u.lockpick > 0) {  // the lockpicks of the bag
      const ImVec2 a{x + (n % kCols) * (cw + gap), y + (n / kCols) * (ch + gap)};
      ImGui::SetCursorScreenPos(a);
      ImGui::InvisibleButton("##lockpick", {cw, ch});
      if (ImGui::IsItemHovered() && hooks.lockpickTip)
        if (const std::string tip = hooks.lockpickTip(); !tip.empty()) ImGui::SetTooltip("%s", tip.c_str());
      dl->AddRectFilled(a, {a.x + cw, a.y + ch}, kBg, 6.0f);
      dl->AddRect(a, {a.x + cw, a.y + ch}, kBorder, 6.0f);
      if (const UiImage* icon = uiImage(publicDir / "game/icons/lockpick.png")) imageContain(dl, *icon, {a.x + 8, a.y + 8}, {a.x + 32, a.y + 32});
      dl->AddText(ui, 14.0f, {a.x + 40, a.y + 5}, kFg, en ? "Lockpick" : "Gazua");
      dl->AddText(ui, 13.0f, {a.x + 40, a.y + 23}, kMuted, std::to_string(u.lockpick).c_str());
      ++n;
    }
    if (n == 0) {
      dl->AddText(ui, 14.0f, {x, y}, kMuted, en ? "No potions carried." : "Sem poções carregadas.");
      return y + 22.0f;
    }
    return y + ((n + kCols - 1) / kCols) * (ch + gap);
  }

  // ember-ui/styles.css ::-webkit-scrollbar: 14 px, a dark track with a faint edge, a pill thumb in aged bronze with a gold ring
  // and a warm glow (brighter under the pointer). Drawn by hand over the window's own scroll (the browser's one is not ImGui's).
  void drawScrollbar(ImDrawList* dl, ImVec2 wa, ImVec2 wb) {
    const ImGuiIO& io = ImGui::GetIO();
    const float maxY = ImGui::GetScrollMaxY(), scrollY = ImGui::GetScrollY(), viewH = wb.y - wa.y;
    const ImVec2 t0{wb.x - 14.0f - 1.0f, wa.y + 1.0f}, t1{wb.x - 1.0f, wb.y - 1.0f};
    const float trackH = t1.y - t0.y;
    const float thumbH = std::clamp(trackH * viewH / (viewH + maxY), 34.0f, trackH);
    const float travel = std::max(1.0f, trackH - thumbH);
    const float thumbY = t0.y + travel * (scrollY / maxY);
    const bool overTrack = io.MousePos.x >= t0.x && io.MousePos.x <= t1.x && io.MousePos.y >= t0.y && io.MousePos.y <= t1.y;
    const bool overThumb = overTrack && io.MousePos.y >= thumbY && io.MousePos.y <= thumbY + thumbH;
    if (overTrack && io.MouseClicked[0] && ImGui::IsWindowHovered()) {
      dragging_ = true;
      grab_ = overThumb ? io.MousePos.y - thumbY : thumbH / 2;
    }
    if (dragging_ && io.MouseDown[0]) ImGui::SetScrollY(std::clamp((io.MousePos.y - grab_ - t0.y) / travel, 0.0f, 1.0f) * maxY);
    else dragging_ = false;
    // the track: linear-gradient(90deg, #090808, #1a1a1a 48%, #090808), a #363636 edge
    const float mid = t0.x + 14.0f * 0.48f;
    dl->AddRectFilledMultiColor(t0, {mid, t1.y}, IM_COL32(9, 8, 8, 255), IM_COL32(26, 26, 26, 255), IM_COL32(26, 26, 26, 255), IM_COL32(9, 8, 8, 255));
    dl->AddRectFilledMultiColor({mid, t0.y}, t1, IM_COL32(26, 26, 26, 255), IM_COL32(9, 8, 8, 255), IM_COL32(9, 8, 8, 255), IM_COL32(26, 26, 26, 255));
    dl->AddRect(t0, t1, IM_COL32(54, 54, 54, 255), 0.0f, 1.0f, ImDrawFlags_None);
    // the thumb: a 3 px #121212 border round a pill whose colour runs d1a05f > 505050 > 1d1d1d > 727272 (brighter on hover)
    const bool hot = overThumb || dragging_;
    const ImVec2 p0{t0.x + 1.0f, thumbY}, p1{t1.x - 1.0f, thumbY + thumbH};
    dl->AddRectFilled({p0.x - 2.0f, p0.y - 2.0f}, {p1.x + 2.0f, p1.y + 2.0f}, hot ? IM_COL32(231, 110, 31, 70) : IM_COL32(190, 85, 24, 52), 10.0f);  // the glow
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 18, 255), 999.0f);
    const ImVec2 q0{p0.x + 3.0f, p0.y + 3.0f}, q1{p1.x - 3.0f, p1.y - 3.0f};
    const float r = (q1.x - q0.x) / 2.0f;
    const ImU32 c0 = hot ? IM_COL32(240, 197, 123, 255) : IM_COL32(209, 160, 95, 255), c1 = hot ? IM_COL32(108, 108, 108, 255) : IM_COL32(80, 80, 80, 255),
                c2 = hot ? IM_COL32(37, 37, 37, 255) : IM_COL32(29, 29, 29, 255), c3 = hot ? IM_COL32(215, 138, 66, 255) : IM_COL32(114, 114, 114, 255);
    dl->AddCircleFilled({(q0.x + q1.x) / 2, q0.y + r}, r, c0);
    dl->AddCircleFilled({(q0.x + q1.x) / 2, q1.y - r}, r, c3);
    const float y0 = q0.y + r, y1 = q1.y - r, span = std::max(0.0f, y1 - y0);
    dl->AddRectFilledMultiColor({q0.x, y0}, {q1.x, y0 + span * 0.36f}, c0, c0, c1, c1);
    dl->AddRectFilledMultiColor({q0.x, y0 + span * 0.36f}, {q1.x, y0 + span * 0.68f}, c1, c1, c2, c2);
    dl->AddRectFilledMultiColor({q0.x, y0 + span * 0.68f}, {q1.x, y1}, c2, c2, c3, c3);
    dl->AddRect(q0, q1, hot ? IM_COL32(255, 231, 172, 158) : IM_COL32(255, 211, 139, 90), 999.0f, 1.0f, ImDrawFlags_None);  // the gold ring
  }

  // A section's title (Atributos, Magias e habilidades, Poções): Newsreader at a size that reads, in the sheet's gold.
  static void sectionTitle(ImDrawList* dl, ImVec2 at, const char* text) {
    dl->AddText(uiFont(UiFont::Display), 17.0f, at, IM_COL32(224, 189, 128, 255), text);
  }

  // The wheel, like a browser's smooth scroll: each notch moves the target kWheelStep, the view glides to it (~0.15 s).
  void wheelScroll() {
    static constexpr float kWheelStep = 56.0f, kGlide = 22.0f;
    const ImGuiIO& io = ImGui::GetIO();
    const float maxY = ImGui::GetScrollMaxY(), scrollY = ImGui::GetScrollY();
    if (scrollReset_) {
      scrollReset_ = false;
      ImGui::SetScrollY(0.0f);
      return;
    }
    if (dragging_) scrollTarget_ = scrollY;  // the thumb drives it
    else if (io.MouseWheel != 0.0f && ImGui::IsWindowHovered()) scrollTarget_ -= io.MouseWheel * kWheelStep;
    scrollTarget_ = std::clamp(scrollTarget_, 0.0f, maxY);
    const float gap = scrollTarget_ - scrollY;
    if (std::abs(gap) < 0.5f) {
      if (gap != 0.0f) ImGui::SetScrollY(scrollTarget_);
      return;
    }
    ImGui::SetScrollY(scrollY + gap * (1.0f - std::exp(-kGlide * io.DeltaTime)));
  }

  bool dragging_ = false, scrollable_ = false, scrollReset_ = false;
  float grab_ = 0, scrollTarget_ = 0;
  bool open_ = false, conditionOpen_ = false, skillsOpen_ = false;
  size_t unit_ = 0;
  float contentHeight_ = 0;
};

}  // namespace ember
