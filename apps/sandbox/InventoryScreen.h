#pragma once

// The party's inventory, copied from Engine2 (src/game/InventoryScreens.tsx): the overlay with its two views.
// Equipar (PaperDollScreen): the hero's paper doll (game/ui/equipment-male.jpg, -female for Neera) with its twelve
// sockets; a socket opens the picker of owned pieces that fit (weapons of the hero's class for the main hand; the
// shared stash's gear for the rest, or a piece another hero wears); Desequipar. Mochila (BackpackScreen): the party's
// shared cells (ration stacks first, loose weapons, loose gear; used / capacity), the hero's potions and lockpick, and
// the item sheet (Equipar on a chosen hero, Usar, Jogar Fora with its confirmation). Heroes switch with the chips or
// the arrows. Rules: engine/world/Inn (InnRules inventory part).
//
// Not ported: the Status button (the status sheet is its own screen, not ported yet).

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <numbers>
#include <random>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "render/UiArt.h"
#include "world/Inn.h"

namespace ember {

class InventoryScreen {
 public:
  struct Context {
    e2::Json& save;
    std::function<void()> write;
    const e2::InnRules& rules;
    const e2::InnData& data;
    const e2::Overworld& world;
    std::filesystem::path engine2Public;
  };

  InventoryScreen(std::string hero, bool backpack) : hero_(std::move(hero)), backpack_(backpack) {}

  // True when the player closed it.
  bool draw(ImVec2 size, Context& c) {
    now_ = static_cast<float>(ImGui::GetTime());
    const std::vector<std::string> heroes = roster(c);
    if (std::find(heroes.begin(), heroes.end(), hero_) == heroes.end() && !heroes.empty()) hero_ = heroes[0];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // ember-veil behind, then the window filling the screen (p-2).
    verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(0, 0, 0, 89)}, {1.0f, IM_COL32(0, 0, 0, 184)}});
    bool close = false;
    if (backpack_) close = drawBackpack(size, c, heroes);
    else close = drawDoll(size, c, heroes);
    if (sheet_) drawSheet(size, c, heroes);
    if (picker_) drawPicker(size, c);
    return close;
  }

 private:
  static constexpr ImU32 kMuted = IM_COL32(180, 180, 180, 255), kFg = IM_COL32(240, 240, 240, 255), kAccent = IM_COL32(216, 216, 216, 255),
                         kDanger = IM_COL32(181, 74, 50, 255), kBorder = IM_COL32(74, 74, 74, 255);
  static constexpr const char* kPotions[7] = {"weak", "mid", "potent", "disease", "manaSmall", "manaMid", "manaLarge"};
  static constexpr const char* kBagIcon = "game/icons/refresh-001/packs/small-pouch.png";

  struct DollSlot {
    const char* id;
    const char* label;
    float left, top, width, height;  // percent of the 1712 x 1152 art
  };
  static constexpr DollSlot kDollSlots[12] = {
      {"head", "Cabeça", 14.953f, 10.764f, 9.638f, 9.722f},          {"neck", "Pescoço", 14.720f, 27.778f, 10.164f, 9.809f},
      {"shoulders", "Ombros", 14.311f, 44.184f, 10.280f, 9.896f},    {"chest", "Peito", 14.194f, 61.111f, 10.631f, 9.809f},
      {"hands", "Mãos", 14.369f, 77.604f, 10.514f, 9.462f},          {"legs", "Pernas", 75.643f, 10.156f, 9.696f, 10.503f},
      {"feet", "Pés", 75.409f, 27.170f, 10.222f, 10.851f},           {"waist", "Cintura", 75.467f, 44.184f, 10.339f, 10.330f},
      {"mainHand", "Mão Principal", 75.526f, 61.024f, 9.813f, 10.069f}, {"offHand", "Mão Secundária", 76.051f, 78.038f, 9.229f, 9.115f},
      {"ring1", "Anel 1", 37.033f, 78.993f, 8.762f, 10.677f},        {"ring2", "Anel 2", 54.731f, 79.601f, 8.645f, 10.069f},
  };

  struct Entry {
    std::string name, icon, tip;
    int count = 0;
    bool weapon = false;
    std::vector<std::pair<std::string, std::function<void(Context&)>>> equipChoices;  // run with this frame's Context
    std::function<void(Context&)> use, discard;
  };

  std::vector<std::string> roster(const Context& c) const {
    std::vector<std::string> out;
    for (const char* h : {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"})
      if (c.world.heroRecruited(h, c.save)) out.push_back(h);
    return out;
  }
  const UiImage* art(const Context& c, std::string path) const {
    if (const size_t q = path.find('?'); q != std::string::npos) path.resize(q);
    if (!path.empty() && path[0] == '/') path.erase(0, 1);
    return uiImage(c.engine2Public / std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())));
  }
  void apply(Context& c, std::optional<e2::Json> next) {
    if (!next) return;
    c.save = std::move(*next);
    c.write();
  }
  void glow(const std::string& slot) { glowSlot_ = slot, glowAt_ = now_; }
  static void tip(const std::string& s) {
    if (!ImGui::IsItemHovered()) return;
    ImGui::BeginTooltip();
    ImGui::PushFont(uiFont(UiFont::Ui), 11.0f);
    ImGui::PushTextWrapPos(256);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::EndTooltip();
  }
  static bool plate(const char* id, const std::string& label, float w, float h, bool accent = false) {
    return emberPlate(id, {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) {
      const ImVec2 t = uiFont(UiFont::Ui)->CalcTextSizeA(12.0f, FLT_MAX, 0, label.c_str());
      d->AddText(uiFont(UiFont::Ui), 12.0f, {std::round((a.x + b.x - t.x) / 2), std::round((a.y + b.y - t.y) / 2)}, accent ? IM_COL32(255, 224, 180, 255) : ink,
                 label.c_str());
      if (accent) d->AddRect(a, b, IM_COL32(226, 170, 100, 217), 2.0f, 1.5f);
    });
  }
  void hungerBar(const Context& c, ImDrawList* dl, ImVec2 a, float w) {
    const double full = e2::Overworld::fullness(c.save.value("heroHunger", e2::Json::object()).value(hero_, e2::Json()));
    dl->AddRectFilled({a.x, a.y}, {a.x + w, a.y + 4}, kBorder, 2);
    dl->AddRectFilled({a.x, a.y}, {a.x + w * static_cast<float>(std::min(100.0, full) / 120), a.y + 4}, full <= 25 ? kDanger : kAccent, 2);
    if (full > 100) dl->AddRectFilled({a.x + w * 100 / 120, a.y}, {a.x + w * static_cast<float>(full / 120), a.y + 4}, IM_COL32(201, 162, 39, 255));
  }

  // The hero switcher: the arrows (right side of the header) and the chips row below it; returns the chips' height.
  float heroChips(const std::vector<std::string>& heroes, ImVec2 at) {
    if (heroes.size() < 2) return 0;
    float x = at.x;
    for (const std::string& h : heroes) {
      const float w = uiFont(UiFont::Ui)->CalcTextSizeA(12.0f, FLT_MAX, 0, h.c_str()).x + 24;
      ImGui::SetCursorScreenPos({x, at.y});
      ImGui::PushID(h.c_str());
      if (plate("##chip", h, w, 28, h == hero_)) hero_ = h;
      ImGui::PopID();
      x += w + 6;
    }
    return 36;
  }
  void heroArrows(const std::vector<std::string>& heroes, float& x, float y) {
    if (heroes.size() < 2) return;
    const auto it = std::find(heroes.begin(), heroes.end(), hero_);
    const size_t i = it == heroes.end() ? 0 : static_cast<size_t>(it - heroes.begin());
    x -= 36;
    ImGui::SetCursorScreenPos({x, y});
    if (emberPlate("##next", {36, 36}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconChevronRight(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, ink); }))
      hero_ = heroes[(i + 1) % heroes.size()];
    x -= 40;
    ImGui::SetCursorScreenPos({x, y});
    if (emberPlate("##prev", {36, 36}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconChevronLeft(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, ink); }))
      hero_ = heroes[(i + heroes.size() - 1) % heroes.size()];
    x -= 8;
  }

  // --- Equipar: the paper doll ---------------------------------------------------------------------------------------

  bool drawDoll(ImVec2 size, Context& c, const std::vector<std::string>& heroes) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{8, 8}, b{size.x - 8, size.y - 8};
    if (const UiImage* bg = uiImage(c.engine2Public / "game/ui/equipment-background.jpg")) imageCover(dl, *bg, {0, 0}, size);
    // ember-window
    dl->AddRect(a, b, kBorder);
    bool close = false;
    ImGui::SetCursorScreenPos({a.x + 8, a.y + 8});
    if (emberPlate("##back", {84, 36}, [](ImDrawList* d, ImVec2 p, ImVec2 q, ImU32 ink) {
          iconChevronLeft(d, {p.x + 18, (p.y + q.y) / 2}, 16, ink);
          d->AddText(uiFont(UiFont::Ui), 12.0f, {p.x + 30, (p.y + q.y) / 2 - 7}, ink, "Voltar");
        }))
      close = true;
    const std::string cls = c.rules.heroClass(c.save, hero_);
    emberTitle(dl, {a.x + 104, a.y + 6}, 20.0f, hero_.c_str());
    const auto cn = c.data.classNames.find(cls);
    dl->AddText(uiFont(UiFont::Ui), 12.0f, {a.x + 104, a.y + 30}, kMuted, cn == c.data.classNames.end() ? cls.c_str() : cn->second.c_str());
    float x = b.x - 8;
    // Mochila
    x -= 120;
    ImGui::SetCursorScreenPos({x, a.y + 8});
    if (emberPlate("##tobackpack", {120, 48}, [&](ImDrawList* d, ImVec2 p, ImVec2 q, ImU32 ink) {
          if (const UiImage* icon = art(c, kBagIcon)) d->AddImage(icon->texture, {p.x + 10, p.y + 8}, {p.x + 42, p.y + 40});
          d->AddText(uiFont(UiFont::Ui), 12.0f, {p.x + 50, (p.y + q.y) / 2 - 7}, ink, "Mochila");
        }))
      backpack_ = true;
    x -= 8;
    heroArrows(heroes, x, a.y + 14);
    const float chips = heroChips(heroes, {a.x + 8, a.y + 64});
    // FittedArt: the doll art letterboxed in what is left.
    const float top = a.y + 64 + chips + 4, bottom = b.y - 8, left = a.x + 8, right = b.x - 8;
    const float ratio = 1712.0f / 1152.0f;
    float w = right - left, h = w / ratio;
    if (h > bottom - top) h = bottom - top, w = h * ratio;
    const ImVec2 o{(left + right - w) / 2, (top + bottom - h) / 2};
    const char* doll = hero_ == "Neera" ? "game/ui/equipment-female.jpg" : "game/ui/equipment-male.jpg";
    if (const UiImage* img = uiImage(c.engine2Public / doll)) dl->AddImage(img->texture, o, {o.x + w, o.y + h});
    const e2::Json& worn = c.save.value("equipment", e2::Json::object()).value(hero_, e2::Json::object());
    const std::string weaponId = c.save.value("equipped", e2::Json::object()).value(hero_, "");
    const e2::InnWeapon* weapon = c.data.weapon(weaponId);
    const int enh = c.save.value("weapons", e2::Json::object()).value(weaponId, 0);
    for (const DollSlot& s : kDollSlots) {
      const ImVec2 p{o.x + w * s.left / 100, o.y + h * s.top / 100}, q{p.x + w * s.width / 100, p.y + h * s.height / 100};
      const bool isWeapon = std::string(s.id) == "mainHand";
      const std::string itemId = isWeapon ? weaponId : worn.value(s.id, "");
      const e2::InnGear* item = isWeapon ? nullptr : c.data.gear(itemId);
      const std::string icon = isWeapon ? (weapon ? weapon->icon : "") : item ? item->icon : "";
      ImGui::SetCursorScreenPos(p);
      ImGui::PushID(s.id);
      if (ImGui::InvisibleButton("##socket", {q.x - p.x, q.y - p.y})) picker_ = s.id;
      const bool hover = ImGui::IsItemHovered();
      tip(isWeapon ? (weapon ? weapon->tooltip[static_cast<size_t>(std::clamp(enh, 0, static_cast<int>(weapon->tooltip.size()) - 1))] : std::format("{} · vazia", s.label))
                   : item ? item->tooltip : std::format("{} · vazio", s.label));
      ImGui::PopID();
      if (hover) dl->AddRectFilled(p, q, IM_COL32(17, 17, 17, 89), 2), dl->AddRect(p, q, IM_COL32(216, 216, 216, 204), 2);
      if (glowSlot_ == s.id && now_ - glowAt_ < 1.8f) {
        const float k = 0.5f - 0.5f * std::cos((now_ - glowAt_) / 2.8f * 2 * std::numbers::pi_v<float>);
        for (int i = 3; i >= 1; --i) dl->AddRect({p.x - i * 3.0f, p.y - i * 3.0f}, {q.x + i * 3.0f, q.y + i * 3.0f}, IM_COL32(201, 150, 88, static_cast<int>(30 + 40 * k)), 4.0f, 3.0f);
        dl->AddRect(p, q, IM_COL32(201, 160, 112, 255), 2);
      }
      if (!icon.empty())
        if (const UiImage* img = art(c, icon)) {
          // max 96 % of the socket, aspect kept, a small drop shadow.
          const float sw = (q.x - p.x) * 0.96f, sh = (q.y - p.y) * 0.96f;
          const float k = std::min(sw / img->width, sh / img->height);
          const float iw = img->width * k, ih = img->height * k;
          const ImVec2 ia{(p.x + q.x - iw) / 2, (p.y + q.y - ih) / 2};
          dl->AddImage(img->texture, {ia.x, ia.y + 1}, {ia.x + iw, ia.y + ih + 1}, {0, 0}, {1, 1}, IM_COL32(0, 0, 0, 160));
          dl->AddImage(img->texture, ia, {ia.x + iw, ia.y + ih});
        }
    }
    return close;
  }

  // The picker of one socket: Desequipar, then every owned piece that fits.
  void drawPicker(ImVec2 size, Context& c) {
    const std::string slot = *picker_;
    ImGui::SetCursorScreenPos({0, 0});
    if (ImGui::InvisibleButton("##pickerveil", size)) {
      picker_.reset();
      return;
    }
    ImGui::GetWindowDrawList()->AddRectFilled({0, 0}, size, IM_COL32(0, 0, 0, 140));
    ImGui::SetNextWindowPos({size.x / 2, size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({std::min(384.0f, size.x - 24), 0});
    ImGui::SetNextWindowSizeConstraints({0, 0}, {384, size.y * 0.8f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 16});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##picker", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImDrawListSplitter split;
    split.Split(dl, 2);
    split.SetCurrentChannel(dl, 1);
    const float inner = std::min(384.0f, size.x - 24) - 32;
    std::string label = "Mão Principal";
    for (const auto& [id, l] : c.data.equipmentSlots)
      if (id == slot) label = l;
    const ImVec2 head = ImGui::GetCursorScreenPos();
    emberTitle(dl, head, 18.0f, label.c_str());
    ImGui::SetCursorScreenPos({head.x + inner - 32, head.y});
    if (emberPlate("##pickerclose", {32, 32}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, IM_COL32(223, 191, 142, 255)); }))
      picker_.reset();
    ImGui::SetCursorScreenPos({head.x, head.y + 44});
    const std::string cls = c.rules.heroClass(c.save, hero_);
    ImFont* ui = uiFont(UiFont::Ui);
    auto row = [&](const char* id, const std::string& icon, const std::string& name, const std::vector<std::pair<std::string, ImU32>>& lines, const std::string& right,
                   bool disabled, bool black, const std::string& tooltip) {
      const ImVec2 p = ImGui::GetCursorScreenPos();
      const float h = 20.0f + static_cast<float>(lines.size()) * 14 + 8;
      const float rh = std::max(h, 56.0f);
      ImGui::PushID(id);
      ImGui::BeginDisabled(disabled);
      const bool clicked = ImGui::InvisibleButton("##row", {inner, rh});
      ImGui::EndDisabled();
      tip(tooltip);
      ImGui::PopID();
      const int a = disabled ? 128 : 255;
      dl->AddRectFilled(p, {p.x + inner, p.y + rh}, black ? IM_COL32(0, 0, 0, a) : IM_COL32(17, 17, 17, a), 4);
      dl->AddRect(p, {p.x + inner, p.y + rh}, kBorder, 4);
      if (!icon.empty())
        if (const UiImage* img = art(c, icon)) dl->AddImage(img->texture, {p.x + 8, p.y + (rh - 40) / 2}, {p.x + 48, p.y + (rh + 40) / 2}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, a));
      float y = p.y + 6;
      dl->AddText(ui, 14.0f, {p.x + 56, y}, (kFg & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT), name.c_str()), y += 19;
      for (const auto& [t, col] : lines) dl->AddText(ui, 11.0f, {p.x + 56, y}, col, t.c_str()), y += 14;
      if (!right.empty()) {
        const ImVec2 ts = ui->CalcTextSizeA(11.0f, FLT_MAX, 0, right.c_str());
        dl->AddText(ui, 11.0f, {p.x + inner - 8 - ts.x, p.y + (rh - ts.y) / 2}, kMuted, right.c_str());
      }
      ImGui::SetCursorScreenPos({p.x, p.y + rh + 6});
      return clicked && !disabled;
    };
    auto plain = [&](const char* id, const char* text) {
      const ImVec2 p = ImGui::GetCursorScreenPos();
      ImGui::PushID(id);
      const bool clicked = ImGui::InvisibleButton("##plain", {inner, 30});
      ImGui::PopID();
      dl->AddRectFilled(p, {p.x + inner, p.y + 30}, IM_COL32(17, 17, 17, 255), 4);
      dl->AddRect(p, {p.x + inner, p.y + 30}, kBorder, 4);
      dl->AddText(ui, 14.0f, {p.x + 8, p.y + 7}, kFg, text);
      ImGui::SetCursorScreenPos({p.x, p.y + 36});
      return clicked;
    };
    const e2::Json& worn = c.save.value("equipment", e2::Json::object()).value(hero_, e2::Json::object());
    const e2::Json& loose = c.save.value("looseEquipment", e2::Json::object());
    if (slot == "mainHand") {
      const std::string weaponId = c.save.value("equipped", e2::Json::object()).value(hero_, "");
      if (c.data.weapon(weaponId) && plain("##unequip", "Desequipar")) {
        apply(c, c.rules.equipWeapon(c.save, hero_, ""));
        picker_.reset();
      }
      std::vector<const e2::InnWeapon*> owned;
      for (const e2::InnWeapon& w : c.data.weapons)
        if (std::find(w.usableBy.begin(), w.usableBy.end(), cls) != w.usableBy.end() && c.save.value("weapons", e2::Json::object()).contains(w.id)) owned.push_back(&w);
      std::stable_sort(owned.begin(), owned.end(), [](const auto* a, const auto* b) { return a->power < b->power; });
      if (owned.empty()) ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner), ImGui::TextColored(ImColor(kMuted), "Nenhuma arma no saco ainda. Compre uma com o Ferreiro."), ImGui::PopTextWrapPos();
      for (const e2::InnWeapon* w : owned) {
        const int e = c.save["weapons"].value(w->id, 0);
        std::vector<std::pair<std::string, ImU32>> lines{{w->dice + " · " + w->range, kMuted}};
        if (!w->bonusClass.empty()) lines.push_back({"+10% dano · " + w->bonusClassName, w->bonusClass == cls ? kAccent : kMuted});
        if (row(w->id.c_str(), w->icon, w->name + (e > 0 ? std::format(" +{}", e) : ""), lines, w->id == weaponId ? "Equipada" : "", w->id == weaponId, true,
                w->tooltip[static_cast<size_t>(std::clamp(e, 0, static_cast<int>(w->tooltip.size()) - 1))])) {
          apply(c, c.rules.equipWeapon(c.save, hero_, w->id));
          picker_.reset();
        }
      }
    } else if (slot == "offHand" && c.rules.offHandBlocked(c.save, hero_)) {
      ImGui::TextColored(ImColor(kMuted), "Arma principal de duas mãos — sem mão livre para a secundária.");
    } else {
      auto wearerOf = [&](const std::string& id) -> std::string {
        const e2::Json allWorn = c.save.value("equipment", e2::Json::object());  // a named copy: items() of a temporary dangles
        for (const auto& [owner, slots] : allWorn.items())
          for (const auto& [s, v] : slots.items())
            if (v == id) return owner;
        return "";
      };
      const std::string here = worn.value(slot, "");
      const std::string otherRing = slot == "ring1" ? worn.value("ring2", "") : slot == "ring2" ? worn.value("ring1", "") : "";
      if (!here.empty() && plain("##unequipitem", "Desequipar")) {
        apply(c, c.rules.equipItem(c.save, hero_, slot, ""));
        picker_.reset();
      }
      int shown = 0;
      for (const e2::InnGear& it : c.data.equipment) {
        if (!e2::InnRules::fitsSlot(it, slot)) continue;
        if (loose.value(it.id, 0) <= 0 && wearerOf(it.id).empty()) continue;
        if (it.usableBy && std::find(it.usableBy->begin(), it.usableBy->end(), cls) == it.usableBy->end()) continue;
        ++shown;
        const bool equipped = here == it.id;
        const int owned = loose.value(it.id, 0);
        const std::string wearer = wearerOf(it.id);
        const bool onOtherFinger = otherRing == it.id && owned <= 0;
        std::string detail = it.kind == "shield" ? std::format("Investida de Escudo · {}% dano · 70% atordoa", static_cast<int>(std::floor(it.dmgMul * 100 + 0.5)))
                             : it.kind == "weapon" ? std::format("{}D{} · Mão secundária", it.dice, it.faces)
                                                   : it.summary;
        std::vector<std::pair<std::string, ImU32>> lines{{upperUtf8(it.typeSlot), kMuted}, {detail, kMuted}};
        const std::string right = equipped ? "Equipado" : onOtherFinger ? "já no outro anel" : !wearer.empty() ? "em " + wearer : "";
        if (row(it.id.c_str(), it.icon, it.name + (owned > 1 ? std::format(" ×{}", owned) : ""), lines, right, equipped || onOtherFinger, false, it.tooltip)) {
          apply(c, c.rules.equipItem(c.save, hero_, slot, it.id));
          picker_.reset();
          break;
        }
      }
      if (!shown) ImGui::TextColored(ImColor(kMuted), "Nenhum item na Mochila para esse espaço ainda.");
    }
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    split.SetCurrentChannel(dl, 0);
    emberPanel(dl, a, b);
    split.Merge(dl);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }

  // --- Mochila --------------------------------------------------------------------------------------------------------

  bool drawBackpack(ImVec2 size, Context& c, const std::vector<std::string>& heroes) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a{8, 8}, b{size.x - 8, size.y - 8};
    if (const UiImage* bg = uiImage(c.engine2Public / "game/ui/backpack-background.jpg")) imageCover(dl, *bg, a, b);
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(6, 6, 6, 18)}, {1.0f, IM_COL32(6, 6, 6, 31)}});
    dl->AddRect(a, b, kBorder);
    bool close = false;
    ImGui::SetCursorScreenPos({a.x + 8, a.y + 12});
    if (emberPlate("##back", {84, 36}, [](ImDrawList* d, ImVec2 p, ImVec2 q, ImU32 ink) {
          iconChevronLeft(d, {p.x + 18, (p.y + q.y) / 2}, 16, ink);
          d->AddText(uiFont(UiFont::Ui), 12.0f, {p.x + 30, (p.y + q.y) / 2 - 7}, ink, "Voltar");
        }))
      close = true;
    if (const UiImage* icon = art(c, kBagIcon)) dl->AddImage(icon->texture, {a.x + 104, a.y + 6}, {a.x + 140, a.y + 42});
    emberTitle(dl, {a.x + 146, a.y + 10}, 24.0f, "Mochila");
    dl->AddText(uiFont(UiFont::Ui), 12.0f, {a.x + 104, a.y + 46}, kMuted, (hero_ + " · clique num item para usar, equipar ou jogar fora").c_str());
    hungerBar(c, dl, {a.x + 104, a.y + 64}, 260);
    float x = b.x - 8;
    // Equipar
    x -= 84;
    ImGui::SetCursorScreenPos({x, a.y + 12});
    if (plate("##todoll", "Equipar", 84, 36)) backpack_ = false;
    // used / capacity
    const int used = c.rules.bagUsed(c.save), cap = c.rules.bagCapacity(c.save);
    const std::string count = std::format("{} / {}", used, cap);
    const float cw = uiFont(UiFont::Ui)->CalcTextSizeA(14.0f, FLT_MAX, 0, count.c_str()).x + 20;
    x -= 8 + cw;
    ImGui::SetCursorScreenPos({x, a.y + 12});
    emberPlate("##count", {cw, 36}, [&](ImDrawList* d, ImVec2 p, ImVec2 q, ImU32) {
      d->AddText(uiFont(UiFont::Ui), 14.0f, {p.x + 10, (p.y + q.y) / 2 - 8}, used >= cap ? kDanger : kFg, count.c_str());
    });
    x -= 8;
    heroArrows(heroes, x, a.y + 12);
    if (heroes.size() > 1) {
      x -= 128;
      ImGui::SetCursorScreenPos({x, a.y + 12});
      if (plate("##feedall", "Alimentar todos", 128, 36)) {
        int fed = 0;
        for (const std::string& h : heroes)
          if (std::optional<e2::Json> next = c.rules.useRation(c.save, h)) c.save = std::move(*next), ++fed;
        if (fed) c.write();
        note_ = fed == 0 ? "Ninguém comeu — sem rações ou já saciados." : fed == static_cast<int>(heroes.size()) ? "Todos comeram." : std::format("{} comeram — rações não deram pros demais.", fed);
        noteAt_ = now_;
      }
      x -= 8;
    }
    if (!note_.empty() && now_ - noteAt_ < 2.2f) {
      const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(12.0f, FLT_MAX, 0, note_.c_str());
      dl->AddText(uiFont(UiFont::Ui), 12.0f, {x - ts.x, a.y + 22}, kAccent, note_.c_str());
    }
    const float chips = heroChips(heroes, {a.x + 8, a.y + 80});
    // The inner box: the party's cells, then the hero's potions and lockpick.
    const ImVec2 ia{a.x + 8, a.y + 80 + chips}, ib{b.x - 8, b.y - 8};
    dl->AddRect(ia, ib, kBorder, 8);
    ImGui::SetCursorScreenPos({ia.x + 1, ia.y + 1});
    ImGui::BeginChild("##backpackbody", {ib.x - ia.x - 2, ib.y - ia.y - 2}, 0, ImGuiWindowFlags_NoBackground);
    ImDrawList* cl = ImGui::GetWindowDrawList();
    const float pad = 16, inner = ib.x - ia.x - 2 - 2 * pad - 12;
    ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x + pad, ImGui::GetCursorScreenPos().y + pad});
    const ImVec2 o = ImGui::GetCursorScreenPos();
    emberKicker(cl, o, 12.0f, "ITENS DA PARTY");
    const std::vector<Entry> entries = backpackEntries(c, heroes);
    const int cols = inner >= 1024 ? 8 : inner >= 640 ? 6 : 4;
    const float cell = (inner - 8.0f * (cols - 1)) / cols;
    const float gy = o.y + 24;
    for (int i = 0; i < cap; ++i) {
      const ImVec2 p{o.x + (i % cols) * (cell + 8), gy + (i / cols) * (cell + 8)}, q{p.x + cell, p.y + cell};
      const Entry* e = i < static_cast<int>(entries.size()) ? &entries[static_cast<size_t>(i)] : nullptr;
      cl->AddRectFilled(p, q, e && e->weapon ? IM_COL32(0, 0, 0, 255) : IM_COL32(17, 17, 17, 178), 6);
      cl->AddRect(p, q, IM_COL32(74, 74, 74, 204), 6);
      if (!e) continue;
      ImGui::SetCursorScreenPos(p);
      ImGui::PushID(i);
      const bool clicked = ImGui::InvisibleButton("##cell", {cell, cell});
      const bool hover = ImGui::IsItemHovered();
      tip(e->tip);
      ImGui::PopID();
      if (hover) cl->AddRect(p, q, kAccent, 6);
      if (const UiImage* img = art(c, e->icon)) {
        const float k = std::min(cell * 0.96f / img->width, cell * 0.96f / img->height);
        const float iw = img->width * k, ih = img->height * k;
        const ImVec2 ia2{(p.x + q.x - iw) / 2, (p.y + q.y - ih) / 2};
        cl->AddImage(img->texture, {ia2.x, ia2.y + 2}, {ia2.x + iw, ia2.y + ih + 2}, {0, 0}, {1, 1}, IM_COL32(0, 0, 0, 200));
        cl->AddImage(img->texture, ia2, {ia2.x + iw, ia2.y + ih});
      }
      if (e->count > 1) {
        const std::string n = std::to_string(e->count);
        const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(10.0f, FLT_MAX, 0, n.c_str());
        cl->AddRectFilled({q.x - ts.x - 8, q.y - ts.y - 4}, {q.x - 2, q.y - 2}, IM_COL32(0, 0, 0, 204), 3);
        cl->AddText(uiFont(UiFont::Ui), 10.0f, {q.x - ts.x - 5, q.y - ts.y - 3}, kFg, n.c_str());
      }
      if (clicked && (!e->equipChoices.empty() || e->use || e->discard)) sheet_ = *e, sheetChoosing_ = false, sheetConfirm_ = false;
    }
    const int rows = (cap + cols - 1) / cols;
    float y = gy + rows * (cell + 8) + 12;
    emberKicker(cl, {o.x, y}, 12.0f, upperUtf8("Poções e gazua de " + hero_).c_str());
    y += 24;
    const e2::Json& bag = c.save.value("bags", e2::Json::object()).value(hero_, e2::Json::object());
    const int pcols = inner >= 640 ? 4 : 2;
    const float pw = (inner - 8.0f * (pcols - 1)) / pcols;
    for (int i = 0; i < 8; ++i) {
      const bool lock = i == 7;
      const std::string kind = lock ? "lockpick" : kPotions[i];
      const e2::InnPotion* p = lock ? nullptr : c.data.potion(kind);
      const int n = bag.value(kind, 0);
      const int max = lock ? c.data.bagMax : p ? p->carryMax : 5;
      const ImVec2 pa{o.x + (i % pcols) * (pw + 8), y + (i / pcols) * 52.0f}, pb{pa.x + pw, pa.y + 44};
      cl->AddRectFilled(pa, pb, IM_COL32(17, 17, 17, 191), 6);
      cl->AddRect(pa, pb, IM_COL32(74, 74, 74, 204), 6);
      const std::string icon = lock ? "game/icons/lockpick.png" : "game/icons/potion-" + kind + ".png";
      if (const UiImage* img = art(c, icon)) cl->AddImage(img->texture, {pa.x + 8, pa.y + 6}, {pa.x + 40, pa.y + 38});
      const std::string name = lock ? "Gazua" : p ? p->label : kind;
      cl->PushClipRect(pa, {pb.x - 6, pb.y}, true);
      cl->AddText(uiFont(UiFont::Ui), lock ? 14.0f : 12.0f, {pa.x + 48, pa.y + 6}, kFg, name.c_str());
      cl->AddText(uiFont(UiFont::Ui), 10.0f, {pa.x + 48, pa.y + 25}, kMuted, std::format("{} / {}", n, max).c_str());
      cl->PopClipRect();
      ImGui::SetCursorScreenPos(pa);
      ImGui::PushID(100 + i);
      const bool clicked = ImGui::InvisibleButton("##potion", {pw, 44});
      tip(lock ? c.data.lockpickTooltip : p ? p->tooltip : "");
      ImGui::PopID();
      if (clicked && n > 0) {
        Entry e;
        e.name = name, e.icon = icon, e.tip = lock ? c.data.lockpickTooltip : p ? p->tooltip : "";
        const std::string hero = hero_;
        if (!lock && kind != "disease")
          e.use = [this, hero, kind](Context& c) {
            static std::mt19937 gen{std::random_device{}()};
            std::uniform_real_distribution<double> uni(0.0, 1.0);
            apply(c, c.rules.usePotion(c.save, hero, kind, [&] { return uni(gen); }));
          };
        e.discard = [this, hero, kind](Context& c) { apply(c, c.rules.discardBagItem(c.save, hero, kind)); };
        sheet_ = std::move(e), sheetChoosing_ = false, sheetConfirm_ = false;
      }
    }
    ImGui::SetCursorScreenPos({o.x, y + 4 * 52.0f + pad});
    ImGui::Dummy({inner, 1});
    ImGui::EndChild();
    return close;
  }

  // BackpackScreen's entries: ration stacks, then loose weapons, then every loose copy of gear.
  std::vector<Entry> backpackEntries(Context& c, const std::vector<std::string>& heroes) {
    std::vector<Entry> out;
    const int rations = c.save.value("rations", 0);
    const int stack = c.data.rationStackMax;
    const double full = e2::Overworld::fullness(c.save.value("heroHunger", e2::Json::object()).value(hero_, e2::Json()));
    for (int i = 0; i < (rations + stack - 1) / stack; ++i) {
      Entry e;
      e.name = "Rações", e.icon = c.data.rationsIcon, e.count = std::min(stack, rations - i * stack);
      e.tip = std::format("{} rações · usar 1 para encher a saciedade de {} até 100%. Atual: {}%.", e.count, hero_, full);
      const std::string hero = hero_;
      if (full < 100) e.use = [this, hero](Context& c) { apply(c, c.rules.useRation(c.save, hero)); };
      e.discard = [this](Context& c) { apply(c, c.rules.discardRation(c.save)); };
      out.push_back(std::move(e));
    }
    const e2::Json& equipped = c.save.value("equipped", e2::Json::object());
    const e2::Json weapons = c.save.value("weapons", e2::Json::object());  // a named copy: items() of a temporary dangles
    for (const auto& [id, enhJ] : weapons.items()) {
      const e2::InnWeapon* w = c.data.weapon(id);
      bool worn = false;
      for (const auto& [h, v] : equipped.items()) worn = worn || v == id;
      if (!w || worn) continue;
      const int enh = enhJ.is_number() ? enhJ.get<int>() : 0;
      Entry e;
      e.name = w->name + (enh > 0 ? std::format(" +{}", enh) : ""), e.icon = w->icon, e.weapon = true;
      e.tip = w->tooltip[static_cast<size_t>(std::clamp(enh, 0, static_cast<int>(w->tooltip.size()) - 1))];
      const std::string wid = id;
      for (const std::string& h : heroes)
        if (std::find(w->usableBy.begin(), w->usableBy.end(), c.rules.heroClass(c.save, h)) != w->usableBy.end())
          e.equipChoices.push_back({h, [this, h, wid](Context& c) {
                                      apply(c, c.rules.equipWeapon(c.save, h, wid));
                                      hero_ = h, glow("mainHand");
                                    }});
      e.discard = [this, wid](Context& c) { apply(c, c.rules.discardWeapon(c.save, wid)); };
      out.push_back(std::move(e));
    }
    const e2::Json loose = c.save.value("looseEquipment", e2::Json::object());
    for (const auto& [id, qtyJ] : loose.items()) {
      const e2::InnGear* item = c.data.gear(id);
      const int count = qtyJ.is_number() ? qtyJ.get<int>() : 0;
      if (!item || count <= 0) continue;
      Entry e;
      e.name = item->name, e.icon = item->icon, e.tip = item->tooltip;
      const std::string iid = id;
      for (const std::string& h : heroes) {
        const std::string cls = c.rules.heroClass(c.save, h);
        // targetSlotFor: a ring takes the free finger (ring1 first); no off-hand under a two-handed weapon.
        std::string slot = item->slot;
        const e2::Json& worn = c.save.value("equipment", e2::Json::object()).value(h, e2::Json::object());
        if (item->slot == "ring1" || item->slot == "ring2") slot = worn.contains("ring1") && worn["ring1"].is_string() ? "ring2" : "ring1";
        if (item->slot == "offHand" && c.rules.offHandBlocked(c.save, h)) continue;
        if (!e2::InnRules::fitsSlot(*item, slot)) continue;
        if (item->usableBy && std::find(item->usableBy->begin(), item->usableBy->end(), cls) == item->usableBy->end()) continue;
        e.equipChoices.push_back({h, [this, h, slot, iid](Context& c) {
                                    apply(c, c.rules.equipItem(c.save, h, slot, iid));
                                    hero_ = h, glow(slot);
                                  }});
      }
      e.discard = [this, iid](Context& c) { apply(c, c.rules.discardEquipment(c.save, iid)); };
      for (int k = 0; k < count; ++k) out.push_back(e);
    }
    return out;
  }

  // ItemActionSheet: Equipar (on which hero, around the item), Usar, Jogar Fora (asks first).
  void drawSheet(ImVec2 size, Context& c, const std::vector<std::string>&) {
    ImGui::SetCursorScreenPos({0, 0});
    if (ImGui::InvisibleButton("##sheetveil", size)) {
      sheet_.reset();
      return;
    }
    ImGui::GetWindowDrawList()->AddRectFilled({0, 0}, size, IM_COL32(0, 0, 0, 140));
    const float w = std::min(320.0f, size.x - 32);
    ImGui::SetNextWindowPos({size.x / 2, size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 16});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##sheet", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImDrawListSplitter split;
    split.Split(dl, 2);
    split.SetCurrentChannel(dl, 1);
    const float inner = w - 32;
    ImFont* ui = uiFont(UiFont::Ui);
    const ImVec2 head = ImGui::GetCursorScreenPos();
    if (const UiImage* img = art(c, sheet_->icon)) dl->AddImage(img->texture, head, {head.x + 44, head.y + 44});
    dl->PushClipRect(head, {head.x + inner - 34, head.y + 46}, true);
    dl->AddText(ui, 14.0f, {head.x + 54, head.y}, kFg, sheet_->name.c_str());
    std::string firstLines = sheet_->tip;
    dl->AddText(ui, 11.0f, {head.x + 54, head.y + 18}, kMuted, firstLines.c_str(), nullptr, inner - 90);
    dl->PopClipRect();
    ImGui::SetCursorScreenPos({head.x + inner - 28, head.y});
    bool closeSheet = false;
    if (emberPlate("##sheetclose", {28, 28}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 14, IM_COL32(223, 191, 142, 255)); }))
      closeSheet = true;
    ImGui::SetCursorScreenPos({head.x, head.y + 56});
    if (sheetChoosing_) {
      // The heroes in a circle around the item ("Equipar em").
      const ImVec2 centre{head.x + inner / 2, head.y + 56 + 128};
      if (const UiImage* img = art(c, sheet_->icon)) dl->AddImage(img->texture, {centre.x - 24, centre.y - 44}, {centre.x + 24, centre.y + 4});
      const ImVec2 ts = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, "Equipar em");
      dl->AddText(ui, 12.0f, {centre.x - ts.x / 2, centre.y + 8}, kMuted, "Equipar em");
      ImGui::SetCursorScreenPos({centre.x - 36, centre.y + 26});
      if (emberTextPlate("##choiceback", {72, 26}, "Voltar", 11.0f)) sheetChoosing_ = false;
      const size_t n = sheet_->equipChoices.size();
      for (size_t i = 0; i < n; ++i) {
        const float ang = -std::numbers::pi_v<float> / 2 + static_cast<float>(i) * 2 * std::numbers::pi_v<float> / static_cast<float>(n);
        const ImVec2 p{centre.x + std::cos(ang) * 94 - 40, centre.y + std::sin(ang) * 94 - 22};
        ImGui::SetCursorScreenPos(p);
        ImGui::PushID(static_cast<int>(i));
        const bool clicked = ImGui::InvisibleButton("##choice", {80, 44});
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        dl->AddRectFilled(p, {p.x + 80, p.y + 44}, hover ? IM_COL32(56, 56, 56, 255) : IM_COL32(17, 17, 17, 255), 22);
        dl->AddRect(p, {p.x + 80, p.y + 44}, kAccent, 22);
        const ImVec2 nt = ui->CalcTextSizeA(14.0f, FLT_MAX, 0, sheet_->equipChoices[i].first.c_str());
        dl->AddText(ui, 14.0f, {p.x + 40 - nt.x / 2, p.y + 22 - nt.y / 2}, kFg, sheet_->equipChoices[i].first.c_str());
        if (clicked) {
          sheet_->equipChoices[i].second(c);
          closeSheet = true;
        }
      }
      ImGui::SetCursorScreenPos({head.x, head.y + 56 + 256});
      ImGui::Dummy({inner, 1});
    } else if (sheetConfirm_) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
      ImGui::PushFont(ui, 12.0f);
      ImGui::TextColored(ImColor(kDanger), "Jogar fora %s? Não pode ser desfeito.", sheet_->name.c_str());
      ImGui::PopFont();
      ImGui::PopTextWrapPos();
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (emberBigButton("##cancel", "CANCELAR", at, {(inner - 8) / 2, 40}, false, 11.0f)) sheetConfirm_ = false;
      ImGui::SetCursorScreenPos({at.x + (inner + 8) / 2, at.y});
      const bool confirm = ImGui::InvisibleButton("##confirm", {(inner - 8) / 2, 40});
      dl->AddRectFilled({at.x + (inner + 8) / 2, at.y}, {at.x + inner, at.y + 40}, IM_COL32(181, 74, 50, 38), 6);
      dl->AddRect({at.x + (inner + 8) / 2, at.y}, {at.x + inner, at.y + 40}, IM_COL32(181, 74, 50, 153), 6);
      const ImVec2 ts = ui->CalcTextSizeA(14.0f, FLT_MAX, 0, "Confirmar");
      dl->AddText(ui, 14.0f, {at.x + (inner + 8) / 2 + ((inner - 8) / 2 - ts.x) / 2, at.y + 12}, kDanger, "Confirmar");
      if (confirm) {
        if (sheet_->discard) sheet_->discard(c);
        closeSheet = true;
      }
    } else {
      const ImVec2 at = ImGui::GetCursorScreenPos();
      const bool canEquip = !sheet_->equipChoices.empty();
      ImGui::BeginDisabled(!canEquip);
      if (emberBigButton("##equip", "EQUIPAR", at, {inner, 40}, false, 11.0f)) sheetChoosing_ = true;
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!sheet_->use);
      if (emberBigButton("##use", "USAR", {at.x, at.y + 46}, {inner, 40}, false, 11.0f) && sheet_->use) {
        sheet_->use(c);
        closeSheet = true;
      }
      ImGui::EndDisabled();
      ImGui::SetCursorScreenPos({at.x, at.y + 92});
      ImGui::BeginDisabled(!sheet_->discard);
      const bool discard = ImGui::InvisibleButton("##discard", {inner, 40});
      ImGui::EndDisabled();
      emberSlot(dl, {at.x, at.y + 92}, {at.x + inner, at.y + 132});
      const ImVec2 ts = ui->CalcTextSizeA(14.0f, FLT_MAX, 0, "Jogar Fora");
      dl->AddText(ui, 14.0f, {at.x + (inner - ts.x) / 2, at.y + 104}, sheet_->discard ? kDanger : IM_COL32(240, 240, 240, 102), "Jogar Fora");
      if (discard) sheetConfirm_ = true;
      // The disabled buttons, dimmed.
      if (!canEquip) dl->AddRectFilled(at, {at.x + inner, at.y + 40}, IM_COL32(17, 17, 17, 150));
      if (!sheet_->use) dl->AddRectFilled({at.x, at.y + 46}, {at.x + inner, at.y + 86}, IM_COL32(17, 17, 17, 150));
    }
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    split.SetCurrentChannel(dl, 0);
    emberPanel(dl, a, b);
    split.Merge(dl);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    if (closeSheet) sheet_.reset();
  }

  std::string hero_;
  bool backpack_ = false;
  float now_ = 0;
  std::optional<std::string> picker_;
  std::optional<Entry> sheet_;
  bool sheetChoosing_ = false, sheetConfirm_ = false;
  std::string glowSlot_;
  float glowAt_ = -10;
  std::string note_;
  float noteAt_ = -10;
};

}  // namespace ember
