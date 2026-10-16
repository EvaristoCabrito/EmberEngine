#pragma once

// The Inn, copied from Engine2 (src/game/InnScreen.tsx): "A Estalagem do Osso Seco" over its painted room, the people
// you can talk to (Brue, O Mudo, Cliente Suspeito, A Hóspede), what each says, Brue's night's rest, every quest each
// one gives (accept, hand in), the Adega (a meal per hero or for everyone, potions and lockpicks into the chosen hero's
// bag, rations for the party), Vargan's forge (weapons for the chosen hero's class: equip, unequip, enhance +1, sell,
// buy; rings; armour and accessories) and the old healer (Cura Média per cast, up to four per person, and the cure for
// disease and poison). Rules: engine/world/Inn.
//
// Engine2 reaches Vargan and the healer by walking the Inn's own map and talking to them; that walkable Inn is not
// ported, so both open from the header here. Mochila / Equipar open the party's inventory (InventoryScreen.h) on the
// hero chosen in the Adega or at the forge. Not ported: the smith's intro film, sound, typing a ration count.

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "InventoryScreen.h"
#include "render/UiArt.h"
#include "world/Inn.h"

namespace ember {

class InnScreen {
 public:
  struct Context {
    e2::Json& save;
    std::function<void()> write;  // persists `save`
    const e2::InnRules& rules;
    const e2::InnData& data;
    const e2::Overworld& world;
    std::filesystem::path engine2Public;
  };

  // True when the player left the Inn.
  bool draw(ImVec2 size, Context& c) {
    now_ = static_cast<float>(ImGui::GetTime());
    stepHealing(c);
    if (inventory_) {
      InventoryScreen::Context ic{c.save, c.write, c.rules, c.data, c.world, c.engine2Public};
      if (inventory_->draw(size, ic)) inventory_.reset();
      return false;
    }
    bool leave = false;
    if (view_ == View::Smith) drawSmith(size, c);
    else if (view_ == View::Healer) drawHealer(size, c);
    else leave = drawMain(size, c);
    return leave;
  }

 private:
  enum class View { Main, Smith, Healer };
  struct Npc {
    const char* id;
    const char* name;
    const char* role;
    const char* portrait;
    const char* talk;
    bool shop;
  };
  static constexpr Npc kNpcs[4] = {
      {"brue", "Brue", "Estalajadeiro", "game/portraits/brue.png",
       "O fogo ainda pega. As camas, não. Gold compra o que sobrou da adega. Salazar pode levar frasco — não precisa, mas o copo não recusa.", true},
      {"mudo", "O Mudo", "Viajante", "game/portraits/mudo.png",
       "Ele não fala. Nunca. Carrega uma pequena tábua de madeira e um pedaço de giz, e usa-a para responder quando não pode simplesmente apontar ou ir "
       "embora. Na tábua, uma frase aparece repetidamente: “Não fique parado por muito tempo.” Ninguém sabe de onde ele veio. Conhece estradas que não "
       "aparecem em mapa algum e parece sempre estar a caminho de algum lugar. Quando perguntam quanto custa uma bebida na velha adega, ele escreve "
       "apenas: “Ainda tem preço.”",
       false},
      {"suspicious", "Cliente Suspeito", "Viajante", "game/portraits/1beca9c9-789c-4b44-8930-c53b88c982ac.jpg",
       "Ele mantém o rosto meio escondido e observa quem entra e quem sai. Parece escolher cada palavra antes de pronunciá-la.", false},
      {"porao", "A Hóspede", "Porão", "game/portraits/porao.png",
       "Não subo. O chão me conhece. Tragam histórias, não luz. Se Brue ainda mede Gold, o mundo não acabou.", false},
  };
  static constexpr const char* kPotionOrder[7] = {"weak", "mid", "potent", "disease", "manaSmall", "manaMid", "manaLarge"};
  static constexpr const char* kPotionIcons[7] = {"game/icons/potion-weak.png", "game/icons/potion-mid.png", "game/icons/potion-potent.png",
                                                  "game/icons/potion-disease.png", "game/icons/potion-manaSmall.png", "game/icons/potion-manaMid.png",
                                                  "game/icons/potion-manaLarge.png"};
  static constexpr const char* kHeroes[6] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};
  static constexpr ImU32 kMuted = IM_COL32(180, 180, 180, 255), kFg = IM_COL32(240, 240, 240, 255), kAccent = IM_COL32(216, 216, 216, 255),
                         kDanger = IM_COL32(181, 74, 50, 255), kAmber = IM_COL32(253, 230, 138, 255);

  std::vector<std::string> roster(const Context& c) const {
    std::vector<std::string> out;
    for (const char* h : kHeroes)
      if (c.world.heroRecruited(h, c.save)) out.push_back(h);
    return out;
  }
  int ember(const Context& c) const { return c.save.value("ember", 0); }
  void apply(Context& c, std::optional<e2::Json> next) {
    if (!next) return;
    c.save = std::move(*next);
    c.write();
  }
  static ImTextureRef* none() { return nullptr; }
  const UiImage* art(const Context& c, std::string path) const {
    if (const size_t q = path.find('?'); q != std::string::npos) path.resize(q);
    if (!path.empty() && path[0] == '/') path.erase(0, 1);
    return uiImage(c.engine2Public / std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())));
  }

  // --- layout helpers ----------------------------------------------------------------------------------------------

  static void text(const std::string& s, float size, ImU32 color, float wrap, UiFont font = UiFont::Ui) {
    ImGui::PushFont(uiFont(font), size);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }
  static void kicker(const std::string& s, float size = 12.0f) {
    emberKicker(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), size, upperUtf8(s).c_str());
    ImGui::Dummy({1, size * 1.4f});
  }
  // A panel (ember-panel, or ember-slot when `slot`) around whatever `body(innerWidth)` lays out, `width` wide.
  template <typename Body>
  static void panel(float width, float pad, Body body, bool slot = false) {
    // Its own splitter, so panels can sit inside panels (ImDrawList's own split does not nest).
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawListSplitter splitter;
    splitter.Split(dl, 2);
    splitter.SetCurrentChannel(dl, 1);
    ImGui::SetCursorScreenPos({start.x + pad, start.y + pad});
    ImGui::BeginGroup();
    body(width - 2 * pad);
    ImGui::EndGroup();
    const float h = ImGui::GetItemRectMax().y - start.y + pad;
    splitter.SetCurrentChannel(dl, 0);
    if (slot) emberSlot(dl, start, {start.x + width, start.y + h});
    else emberPanel(dl, start, {start.x + width, start.y + h});
    splitter.Merge(dl);
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy({width, h});
    ImGui::Dummy({1, 6});
  }
  static bool button(const char* id, const std::string& label, float w, float h, bool primary, bool disabled = false, float font = 11.0f) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::BeginDisabled(disabled);
    const bool clicked = emberBigButton(id, upperUtf8(label).c_str(), at, {w, h}, primary, font);
    ImGui::EndDisabled();
    if (disabled) ImGui::GetWindowDrawList()->AddRectFilled(at, {at.x + w, at.y + h}, IM_COL32(17, 17, 17, 140));
    return clicked && !disabled;
  }
  static bool small(const char* id, const char* label, bool disabled) {
    return emberTextPlate(id, {32, 32}, label, 16.0f, disabled);
  }
  // A tooltip after the hover lingers (ItemTip).
  static void tip(const std::string& s) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) return;
    ImGui::BeginTooltip();
    ImGui::PushFont(uiFont(UiFont::Ui), 13.0f);
    ImGui::PushTextWrapPos(320);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::EndTooltip();
  }
  void heroRow(const Context& c, std::string& hero, const char* id, const std::function<void()>& changed) {
    int k = 0;
    for (const std::string& name : roster(c)) {
      if (k++) ImGui::SameLine();
      ImGui::PushID(id), ImGui::PushID(name.c_str());
      const float w = uiFont(UiFont::Mark)->CalcTextSizeA(11.0f, FLT_MAX, 0, name.c_str()).x * 1.6f + 36;
      if (button("##hero", name, w, 30, hero == name)) {
        hero = name;
        if (changed) changed();
      }
      ImGui::PopID(), ImGui::PopID();
    }
  }
  void header(ImVec2 size, Context& c, const char* back, const char* kick, const char* title, bool& backPressed, bool showSmithHealer) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos({16, 16});
    if (emberTextPlate("##back", {72, 40}, back, 12.0f)) backPressed = true;
    emberKicker(dl, {100, 16}, 12.0f, upperUtf8(kick).c_str());
    emberTitle(dl, {100, 32}, 24.0f, title);
    float x = size.x - 16;
    // Gold
    const std::string gold = std::format("{} Gold", ember(c));
    const float gw = 44 + uiFont(UiFont::Ui)->CalcTextSizeA(14.0f, FLT_MAX, 0, gold.c_str()).x;
    x -= gw;
    ImGui::SetCursorScreenPos({x, 18});
    emberPlate("##gold", {gw, 36}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
      if (const UiImage* icon = uiImage(c.engine2Public / "game/icons/gold-coins-pile.png")) d->AddImage(icon->texture, {a.x + 8, a.y + 6}, {a.x + 32, a.y + 30});
      d->AddText(uiFont(UiFont::Ui), 14.0f, {a.x + 38, (a.y + b.y) / 2 - 8}, kMuted, gold.c_str());
    });
    // Mochila / Equipar, on the hero chosen here (the forge's or the Adega's); the healer has neither.
    if (view_ != View::Healer) {
      const std::string& who = view_ == View::Smith ? smithHero_ : hero_;
      x -= 12 + 96;
      ImGui::SetCursorScreenPos({x, 14});
      if (emberTextPlate("##equipar", {96, 44}, "EQUIPAR", 12.0f)) inventory_.emplace(who, false);
      x -= 12 + 128;
      ImGui::SetCursorScreenPos({x, 14});
      if (emberPlate("##mochila", {128, 44}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) {
            if (const UiImage* icon = art(c, c.data.bagIcon)) d->AddImage(icon->texture, {a.x + 8, a.y + 6}, {a.x + 40, a.y + 38});
            d->AddText(uiFont(UiFont::Mark), 12.0f, {a.x + 48, (a.y + b.y) / 2 - 7}, ink, "MOCHILA");
          }))
        inventory_.emplace(who, true);
    }
    if (showSmithHealer) {
      x -= 12 + 112;
      ImGui::SetCursorScreenPos({x, 16});
      if (emberTextPlate("##healer", {112, 40}, "CURANDEIRO", 12.0f)) view_ = View::Healer, healerNote_.clear();
      x -= 12 + 100;
      ImGui::SetCursorScreenPos({x, 16});
      if (emberTextPlate("##smith", {100, 40}, "FERREIRO", 12.0f)) view_ = View::Smith, smithNote_.clear();
    }
  }

  // --- the Inn's main room -----------------------------------------------------------------------------------------

  bool drawMain(ImVec2 size, Context& c) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, IM_COL32(17, 17, 17, 255));
    if (const UiImage* bg = uiImage(c.engine2Public / "game/assets/brief-estalagem.jpg")) imageCover(dl, *bg, {0, 0}, size);
    verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(17, 17, 17, 26)}, {0.5f, IM_COL32(17, 17, 17, 64)}, {1.0f, IM_COL32(17, 17, 17, 204)}});
    bool leave = false;
    header(size, c, "SAIR", "Pousada à margem da cinza", "A Estalagem do Osso Seco", leave, true);
    if (!talked_) talked_ = true, apply(c, c.rules.talkToNpc(c.save, kNpcs[npc_].id));

    const float colW = std::min(512.0f, size.x - 32);
    const float x0 = (size.x - colW) / 2, top = 72;
    ImGui::SetCursorScreenPos({x0, top});
    ImGui::BeginChild("##innbody", {colW + 16, size.y - top - 72}, 0, ImGuiWindowFlags_NoBackground);
    if (scrollTo_ >= 0) ImGui::SetScrollY(scrollTo_), scrollTo_ = -1;
    // The people: three portraits per row.
    {
      const float cw = (colW - 16) / 3;
      const ImVec2 o = ImGui::GetCursorScreenPos();
      for (int i = 0; i < 4; ++i) {
        const ImVec2 a{o.x + (i % 3) * (cw + 8), o.y + (i / 3) * (cw * 1.5f + 40)};
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##npc", {cw, cw * 1.5f + 28}) && npc_ != i) {
          npc_ = i;
          apply(c, c.rules.talkToNpc(c.save, kNpcs[i].id));
        }
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        ImDrawList* d = ImGui::GetWindowDrawList();
        emberSlot(d, a, {a.x + cw, a.y + cw * 1.5f + 28}, hover || npc_ == i);
        if (const UiImage* img = art(c, kNpcs[i].portrait)) imageCover(d, *img, {a.x + 1, a.y + 1}, {a.x + cw - 1, a.y + cw * 1.5f});
        if (npc_ == i) d->AddRectFilled(a, {a.x + 3, a.y + cw * 1.5f + 28}, IM_COL32(217, 98, 28, 255));
        d->AddText(uiFont(UiFont::Ui), 12.0f, {a.x + 8, a.y + cw * 1.5f + 6}, IM_COL32(226, 194, 148, 255), kNpcs[i].name);
      }
      ImGui::SetCursorScreenPos({o.x, o.y + 2 * (cw * 1.5f + 40)});
    }
    const Npc& npc = kNpcs[npc_];
    // What they say.
    panel(colW, 12, [&](float w) {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      if (const UiImage* img = art(c, npc.portrait)) imageCover(ImGui::GetWindowDrawList(), *img, a, {a.x + 64, a.y + 96});
      ImGui::SetCursorScreenPos({a.x + 76, a.y});
      ImGui::BeginGroup();
      text(std::format("{} · {}", npc.name, npc.role), 14.0f, kFg, w - 76);
      text(npc.talk, 14.0f, IM_COL32(240, 240, 240, 230), w - 76);
      ImGui::EndGroup();
      ImGui::SetCursorScreenPos({a.x, std::max(a.y + 96, ImGui::GetItemRectMax().y)});
      ImGui::Dummy({1, 1});
    });
    if (std::string(npc.id) == "brue") drawRest(colW, c);
    drawQuests(colW, c, npc.id);
    if (npc.shop) drawShop(colW, c);
    ImGui::EndChild();
    ImGui::SetCursorScreenPos({x0, size.y - 60});
    if (button("##leave", "‹  Sair da Adega", colW, 44, false)) leave = true;
    return leave;
  }

  void drawRest(float colW, Context& c) {
    const std::vector<std::string> party = roster(c);
    const int cost = static_cast<int>(party.size()) * c.data.restPrice;
    panel(colW, 12, [&](float w) {
      kicker("Descanso · uma noite");
      text("Dormir na estalagem avança um dia, recupera 75% do HP perdido e restaura todos os usos de feitiço.", 14.0f, IM_COL32(240, 240, 240, 230), w);
      text(std::format("Prévia do custo · {} pessoas × {} Gold = {} Gold", party.size(), c.data.restPrice, cost), 14.0f, kFg, w);
      const int day = c.save.value("gameClock", 0);
      text(std::format("Após dormir: {} Gold · Dia {} → {}", std::max(0, ember(c) - cost), day, day + 1), 12.0f, kMuted, w);
      if (!restNote_.empty() && now_ - restAt_ < 3.5f) text(restNote_, 14.0f, kAccent, w);
      if (button("##rest", std::format("Passar a Noite · {} Gold", cost), w, 40, true, party.empty() || ember(c) < cost)) {
        int healed = 0;
        if (std::optional<e2::Json> next = c.rules.passNight(c.save, party, healed)) {
          apply(c, std::move(next));
          restNote_ = std::format("Dia {}. O grupo recuperou {} HP e todos os feitiços foram restaurados.", c.save.value("gameClock", 0), healed);
        } else {
          restNote_ = "Brue recusou. Falta Gold para todo o grupo.";
        }
        restAt_ = now_;
      }
    });
  }

  void drawQuests(float colW, Context& c, const std::string& npc) {
    for (const e2::Json* q : c.rules.questsFor(npc)) {
      const std::string status = c.rules.questStatus(c.save, *q);
      const std::string id = q->value("id", "");
      if (status == "available" && !c.rules.questOffered(c.save, id)) continue;
      int have = 0, total = 0;
      c.rules.questProgress(c.save, *q, have, total);
      panel(colW, 12, [&](float w) {
        kicker(std::string("Missão · ") + (status == "done" ? "concluída" : status == "available" ? "oferecida" : status == "ready" ? "pronta para entregar" : "em andamento"));
        text(q->value("title", ""), 14.0f, kFg, w);
        text(q->value(status == "available" ? "offer" : status == "active" ? "active" : status == "ready" ? "ready" : "done", ""), 14.0f, IM_COL32(240, 240, 240, 230), w);
        if (status != "done") text("Onde: " + q->value("place", ""), 12.0f, kMuted, w);
        const std::string kind = q->value("kind", "");
        if (status != "available" && status != "done")
          text(kind == "kill" ? (have >= total ? "Alvo abatido" : "Alvo ainda vivo") : kind == "recruit" ? (have >= total ? "Oficial resgatado" : "Oficial ainda cativo")
                                                                                     : std::format("Encontrados {} / {}", have, total),
               12.0f, kMuted, w);
        if (status != "done") {
          std::string reward = std::format("Recompensa: {} Gold", q->value("reward", 0));
          for (const e2::Json& p : q->value("rewardPotions", e2::Json::array()))
            if (const e2::InnPotion* pp = c.data.potion(p.get<std::string>())) reward += " + " + pp->label;
          text(reward, 12.0f, kMuted, w);
        }
        ImGui::PushID(id.c_str());
        if (status == "available" && button("##accept", "Aceitar missão", w, 40, true)) apply(c, c.rules.acceptQuest(c.save, id));
        if (status == "ready" && button("##turnin", std::format("Entregar · {} Gold", q->value("reward", 0)), w, 40, true)) apply(c, c.rules.turnInQuest(c.save, id));
        ImGui::PopID();
      });
    }
  }

  void drawShop(float colW, Context& c) {
    const std::vector<std::string> party = roster(c);
    if (std::find(party.begin(), party.end(), hero_) == party.end() && !party.empty()) hero_ = party[0];
    const e2::Json& bag = c.save.contains("bags") && c.save["bags"].contains(hero_) ? c.save["bags"][hero_] : e2::Json::object();
    auto have = [&](const std::string& k) { return bag.is_object() ? bag.value(k, 0) : 0; };
    int total = lockpicks_ * c.data.lockpickPrice, items = lockpicks_;
    for (const char* k : kPotionOrder)
      if (const e2::InnPotion* p = c.data.potion(k)) total += cart_[k] * p->price, items += cart_[k];
    const int remain = ember(c) - total;
    panel(colW, 12, [&](float w) {
      kicker("Adega · quem leva");
      heroRow(c, hero_, "##shophero", [&] { cart_.clear(), lockpicks_ = 0, note_.clear(); });
      // A meal.
      std::vector<std::string> mealTargets;
      for (const std::string& h : party)
        if (e2::Overworld::fullness(c.save.value("heroHunger", e2::Json::object()).value(h, e2::Json())) < 120 &&
            c.save.value("unitHp", e2::Json::object()).value(h, 1) > 0)
          mealTargets.push_back(h);
      const int mealAll = static_cast<int>(mealTargets.size()) * c.data.mealPrice;
      panel(w, 12,
            [&](float iw) {
              text(std::format("Refeição para {} · {} Gold", hero_, c.data.mealPrice), 14.0f, kFg, iw);
              text("Enche a saciedade até 120% · bônus de 20%", 12.0f, kMuted, iw);
              hungerBar(c, hero_, iw);
              const double full = e2::Overworld::fullness(c.save.value("heroHunger", e2::Json::object()).value(hero_, e2::Json()));
              const bool cannot = ember(c) < c.data.mealPrice || full >= 120 || c.save.value("unitHp", e2::Json::object()).value(hero_, 1) <= 0;
              if (button("##meal", std::format("Comer · {} Gold", c.data.mealPrice), (iw - 8) / 2, 32, true, cannot)) {
                std::optional<e2::Json> next = c.rules.buyMeal(c.save, hero_);
                note_ = next ? hero_ + " comeu. Saciedade: 120%." : "Falta Gold ou o personagem já está satisfeito.";
                noteAt_ = now_;
                apply(c, std::move(next));
              }
              ImGui::SameLine();
              if (button("##mealall", std::format("Todos · {} Gold", mealAll), (iw - 8) / 2, 32, false, mealTargets.empty() || ember(c) < c.data.mealPrice)) {
                int fed = 0;
                for (const std::string& h : party)
                  if (std::optional<e2::Json> next = c.rules.buyMeal(c.save, h)) c.save = std::move(*next), ++fed;
                if (fed) c.write();
                note_ = fed == 0 ? "Ninguém comeu. Falta Gold ou já estão satisfeitos."
                        : fed == static_cast<int>(mealTargets.size()) ? "Todos comeram. Saciedade: 120%."
                                                                    : std::format("{} comeram por {} Gold · Gold não deu pros demais.", fed, fed * c.data.mealPrice);
                noteAt_ = now_;
              }
              text(std::format("Prévia: {} {} podem comer · total {} Gold", mealTargets.size(), mealTargets.size() == 1 ? "pessoa" : "pessoas", mealAll), 12.0f,
                   kMuted, iw);
            },
            true);
      // Potions and the lockpick.
      for (int i = 0; i < 7; ++i) {
        const e2::InnPotion* p = c.data.potion(kPotionOrder[i]);
        if (!p) continue;
        itemRow(c, kPotionIcons[i], p->label, std::format("comprar {} · tem {} / {} · {} Gold", cart_[p->id], have(p->id), p->carryMax, p->price), p->tooltip,
                cart_[p->id], have(p->id) + cart_[p->id] >= p->carryMax, w, p->id.c_str(), [&](int d) {
                  note_.clear();
                  cart_[p->id] = std::clamp(cart_[p->id] + d, 0, std::max(0, p->carryMax - have(p->id)));
                });
      }
      itemRow(c, "game/icons/lockpick.png", "Gazua", std::format("comprar {} · tem {} / {} · {} Gold", lockpicks_, have("lockpick"), c.data.bagMax, c.data.lockpickPrice),
              c.data.lockpickTooltip, lockpicks_, have("lockpick") + lockpicks_ >= c.data.bagMax, w, "lockpick", [&](int d) {
                note_.clear();
                lockpicks_ = std::clamp(lockpicks_ + d, 0, std::max(0, c.data.bagMax - have("lockpick")));
              });
      text(std::format("Conta {} Gold · restam {}", total, remain), 14.0f, remain < 0 ? kDanger : kMuted, w);
      if (!note_.empty() && now_ - noteAt_ < 2.2f) text(note_, 14.0f, kAccent, w);
      if (button("##pay", "Pagar", w - 120, 32, true, items <= 0)) {
        if (remain < 0) note_ = std::format("Faltam {} Gold.", -remain);
        else if (std::optional<e2::Json> next = c.rules.pay(c.save, hero_, cart_, lockpicks_)) {
          apply(c, std::move(next));
          cart_.clear(), lockpicks_ = 0;
          note_ = "Pago. Os frascos foram para o saco.";
        } else {
          note_ = "Brue recusou. Gold ou espaço.";
        }
        noteAt_ = now_;
      }
      ImGui::SameLine();
      if (button("##clear", "Limpar", 112, 32, false)) cart_.clear(), note_.clear();
      // Rations for the party.
      ImGui::Dummy({1, 6});
      kicker("Rações · para toda a party");
      const int rations = c.save.value("rations", 0);
      const int stack = c.data.rationStackMax;
      const bool roomForOne = c.world.bagHasRoom(c.save, (rations + rationsQty_ + 1 + stack - 1) / stack - (rations + stack - 1) / stack);
      itemRow(c, c.data.rationsIcon, "Rações", std::format("comprar {} · tem {} · {} Gold cada", rationsQty_, rations, c.data.rationsPrice),
              "Alimenta o grupo inteiro por um dia cada, no mapa. Empilha até 30 por espaço na mochila.", rationsQty_, !roomForOne, w, "rations",
              [&](int d) { rationsQty_ = std::clamp(rationsQty_ + d, 0, stack * 20); });
      if (!rationsNote_.empty() && now_ - rationsAt_ < 2.2f) text(rationsNote_, 14.0f, kAccent, w);
      if (button("##buyrations", rationsQty_ > 0 ? std::format("Comprar ({} Gold)", rationsQty_ * c.data.rationsPrice) : std::string("Comprar"), w, 40, true,
                 rationsQty_ <= 0 || ember(c) < rationsQty_ * c.data.rationsPrice)) {
        if (std::optional<e2::Json> next = c.rules.buyRations(c.save, rationsQty_)) {
          apply(c, std::move(next));
          rationsQty_ = 0;
          rationsNote_ = "Pago. As rações foram para a mochila.";
        } else {
          rationsNote_ = "Brue recusou. Gold ou espaço na mochila.";
        }
        rationsAt_ = now_;
      }
    });
  }

  void hungerBar(const Context& c, const std::string& hero, float w) {
    const double full = e2::Overworld::fullness(c.save.value("heroHunger", e2::Json::object()).value(hero, e2::Json()));
    const ImVec2 a = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({a.x, a.y + 4}, {a.x + w, a.y + 8}, IM_COL32(74, 74, 74, 255), 2);
    dl->AddRectFilled({a.x, a.y + 4}, {a.x + w * static_cast<float>(std::min(100.0, full) / 120), a.y + 8}, full <= 25 ? kDanger : kAccent, 2);
    if (full > 100) dl->AddRectFilled({a.x + w * 100 / 120, a.y + 4}, {a.x + w * static_cast<float>(full / 120), a.y + 8}, IM_COL32(201, 162, 39, 255));
    ImGui::Dummy({w, 12});
  }

  // .tavern-item-window: icon, name and its line, − count +.
  void itemRow(const Context& c, const std::string& icon, const std::string& name, const std::string& line, const std::string& tooltip, int qty, bool maxed, float w,
               const char* id, const std::function<void(int)>& change) {
    const ImVec2 a = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a, {a.x + w, a.y + 44}, IM_COL32(0, 0, 0, 255));
    dl->AddRect(a, {a.x + w, a.y + 44}, IM_COL32(74, 74, 74, 255));
    if (const UiImage* img = art(c, icon)) dl->AddImage(img->texture, {a.x + 8, a.y + 10}, {a.x + 32, a.y + 34});
    dl->AddText(uiFont(UiFont::Ui), 14.0f, {a.x + 40, a.y + 5}, kFg, name.c_str());
    dl->AddText(uiFont(UiFont::Ui), 11.0f, {a.x + 40, a.y + 24}, kMuted, line.c_str());
    ImGui::SetCursorScreenPos(a);
    ImGui::PushID(id);
    ImGui::InvisibleButton("##tip", {w - 112, 44});
    tip(tooltip);
    ImGui::SetCursorScreenPos({a.x + w - 108, a.y + 6});
    if (small("##minus", "−", qty <= 0)) change(-1);
    const std::string n = std::to_string(qty);
    const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(14.0f, FLT_MAX, 0, n.c_str());
    dl->AddText(uiFont(UiFont::Ui), 14.0f, {a.x + w - 54 - ts.x / 2, a.y + 14}, kFg, n.c_str());
    ImGui::SetCursorScreenPos({a.x + w - 40, a.y + 6});
    if (small("##plus", "+", maxed)) change(1);
    ImGui::PopID();
    ImGui::SetCursorScreenPos({a.x, a.y + 48});
    ImGui::Dummy({w, 1});
  }

  // --- Vargan's forge ----------------------------------------------------------------------------------------------

  void drawSmith(ImVec2 size, Context& c) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, IM_COL32(17, 17, 17, 255));
    if (const UiImage* bg = uiImage(c.engine2Public / "game/ui/smith-background.jpg")) imageCover(dl, *bg, {0, 0}, size, 0.0f, 0.5f);
    bool back = false;
    header(size, c, "‹ VOLTAR", "A forja no porão", "Vargan, o Ferreiro", back, false);
    if (back) {
      view_ = View::Main;
      return;
    }
    const std::vector<std::string> party = roster(c);
    if (std::find(party.begin(), party.end(), smithHero_) == party.end() && !party.empty()) smithHero_ = party[0];
    const std::string cls = c.rules.heroClass(c.save, smithHero_);
    const float colW = std::min(512.0f, size.x - 32);
    const float x0 = size.x - 16 - colW, top = 72;
    ImGui::SetCursorScreenPos({x0, top});
    ImGui::BeginChild("##smithbody", {colW + 16, size.y - top - 16}, 0, ImGuiWindowFlags_NoBackground);
    panel(colW, 12, [&](float w) {
      text("“Aço, sangue, alma — tudo é forjado.” Ele não fala mais que isso. Aponta pra bigorna e espera você escolher.", 14.0f, IM_COL32(240, 240, 240, 230), w);
    });
    panel(colW, 12, [&](float w) {
      kicker("Equipamento exibido");
      heroRow(c, smithHero_, "##smithhero", [&] { smithNote_.clear(); });
      // The pool: weapons this hero's class can wield, weakest first.
      std::vector<const e2::InnWeapon*> pool;
      for (const e2::InnWeapon& wpn : c.data.weapons)
        if (wpn.price > 0 && std::find(wpn.usableBy.begin(), wpn.usableBy.end(), cls) != wpn.usableBy.end()) pool.push_back(&wpn);
      std::stable_sort(pool.begin(), pool.end(), [](const auto* a, const auto* b) { return a->power < b->power; });
      const e2::Json& owned = c.save.value("weapons", e2::Json::object());
      const std::string equippedId = c.save.value("equipped", e2::Json::object()).value(smithHero_, "");
      const e2::InnWeapon* equipped = c.data.weapon(equippedId);
      const int enh = equipped ? owned.value(equippedId, 0) : 0;
      const bool bagFull = !c.world.bagHasRoom(c.save, 1);
      ImGui::Dummy({1, 4});
      kicker("Equipada");
      if (equipped) {
        weaponRow(c, *equipped, enh, w, 80, true, [&](float bx, float by) {
          const std::optional<int> next = enh < c.data.weaponMaxEnh ? std::optional<int>(c.data.enhCost[static_cast<size_t>(enh)]) : std::nullopt;
          ImGui::SetCursorScreenPos({bx, by});
          if (button("##upgrade", next ? std::format("+1 · {} Gold", *next) : std::string("Máx."), 132, 26, true, !next || ember(c) < *next, 10.0f)) {
            std::optional<e2::Json> s = c.rules.upgradeWeapon(c.save, equippedId);
            smithNote_ = s ? equipped->name + " aprimorada." : "Vargan recusou. Falta Gold ou já está no máximo.";
            apply(c, std::move(s));
          }
          ImGui::SetCursorScreenPos({bx, by + 30});
          if (button("##unequip", "Desequipar", 132, 26, false, false, 10.0f)) apply(c, c.rules.equipWeapon(c.save, smithHero_, ""));
          ImGui::SetCursorScreenPos({bx, by + 60});
          if (button("##sellequipped", std::format("Vender · {} Gold", equipped->sell[static_cast<size_t>(std::min<int>(enh, static_cast<int>(equipped->sell.size()) - 1))]), 132, 26,
                     false, false, 10.0f)) {
            int value = 0;
            std::optional<e2::Json> s = c.rules.sellWeapon(c.save, equippedId, value);
            if (s) smithNote_ = std::format("{} vendida por {} Gold.", equipped->name, value);
            apply(c, std::move(s));
          }
        });
      } else {
        text("Nenhuma arma equipada ainda.", 14.0f, kMuted, w);
      }
      std::vector<const e2::InnWeapon*> inBag, onBench;
      for (const e2::InnWeapon* wpn : pool) {
        if (owned.contains(wpn->id) && wpn->id != equippedId) inBag.push_back(wpn);
        if (!owned.contains(wpn->id)) onBench.push_back(wpn);
      }
      if (!inBag.empty()) {
        ImGui::Dummy({1, 4});
        kicker("No saco");
        for (const e2::InnWeapon* wpn : inBag) {
          const int e = owned.value(wpn->id, 0);
          ImGui::PushID(wpn->id.c_str());
          weaponRow(c, *wpn, e, w, 64, false, [&](float bx, float by) {
            ImGui::SetCursorScreenPos({bx, by});
            if (button("##equip", "Equipar", 132, 26, false, false, 10.0f)) apply(c, c.rules.equipWeapon(c.save, smithHero_, wpn->id));
            ImGui::SetCursorScreenPos({bx, by + 30});
            if (button("##sell", std::format("Vender · {} Gold", wpn->sell[static_cast<size_t>(std::min<int>(e, static_cast<int>(wpn->sell.size()) - 1))]), 132, 26, false,
                       false, 10.0f)) {
              int value = 0;
              std::optional<e2::Json> s = c.rules.sellWeapon(c.save, wpn->id, value);
              if (s) smithNote_ = std::format("{} vendida por {} Gold.", wpn->name, value);
              apply(c, std::move(s));
            }
          }, cls);
          ImGui::PopID();
        }
      }
      if (!onBench.empty()) {
        ImGui::Dummy({1, 4});
        kicker("Na bancada");
        for (const e2::InnWeapon* wpn : onBench) {
          ImGui::PushID(wpn->id.c_str());
          weaponRow(c, *wpn, -1, w, 64, false, [&](float bx, float by) {
            ImGui::SetCursorScreenPos({bx, by});
            if (button("##buy", "Comprar", 132, 26, true, bagFull || ember(c) < wpn->price, 10.0f)) {
              if (!c.world.bagHasRoom(c.save, 1)) smithNote_ = "Mochila cheia.";
              else if (std::optional<e2::Json> s = c.rules.buyWeapon(c.save, wpn->id)) {
                apply(c, std::move(s));
                smithNote_ = wpn->name + " comprada. Equipe no saco quando quiser.";
              } else {
                smithNote_ = "Vargan recusou. Falta Gold.";
              }
            }
          }, cls);
          ImGui::PopID();
        }
      }
      // Rings, then the inn's basic armour and accessories (the cheaper half of each slot).
      auto fits = [&](const e2::InnGear& g) {
        return !g.usableBy || std::find(g.usableBy->begin(), g.usableBy->end(), cls) != g.usableBy->end();
      };
      std::vector<const e2::InnGear*> rings, gear;
      std::map<std::string, std::vector<const e2::InnGear*>> bySlot;
      std::vector<std::string> slotOrder;
      for (const e2::InnGear& g : c.data.equipment) {
        if (!fits(g)) continue;
        if (g.slot == "ring1" || g.slot == "ring2") {
          if (g.price > 0) rings.push_back(&g);
          continue;
        }
        if (g.price <= 0 || g.pouch) continue;
        if (!bySlot.contains(g.slot)) slotOrder.push_back(g.slot);
        bySlot[g.slot].push_back(&g);
      }
      auto byPrice = [](const e2::InnGear* a, const e2::InnGear* b) { return a->price < b->price; };
      std::stable_sort(rings.begin(), rings.end(), byPrice);
      for (const std::string& s : slotOrder) {
        auto& items = bySlot[s];
        std::stable_sort(items.begin(), items.end(), byPrice);
        items.resize((items.size() + 1) / 2);
        gear.insert(gear.end(), items.begin(), items.end());
      }
      std::stable_sort(gear.begin(), gear.end(), byPrice);
      ImGui::Dummy({1, 4});
      kicker("Anéis");
      text("Cabem nos dois dedos da paper doll. Compra dois se quiser testar os dois espaços.", 11.0f, kMuted, w);
      gearGrid(c, rings, w, bagFull);
      ImGui::Dummy({1, 4});
      kicker("Armaduras e acessórios");
      text("Estoque básico da estalagem. As peças superiores pertencem ao ferreiro da cidade.", 11.0f, kMuted, w);
      gearGrid(c, gear, w, bagFull);
      if (!smithNote_.empty()) text(smithNote_, 14.0f, kAccent, w);
    });
    ImGui::EndChild();
    if (!smithNote_.empty()) {
      const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(14.0f, FLT_MAX, 0, smithNote_.c_str());
      ImDrawList* fg = ImGui::GetForegroundDrawList();
      const ImVec2 a{(size.x - ts.x) / 2 - 16, 80}, b{(size.x + ts.x) / 2 + 16, 80 + ts.y + 16};
      fg->AddRectFilled(a, b, IM_COL32(17, 17, 17, 240), 2);
      fg->AddRect(a, b, IM_COL32(226, 170, 100, 200), 2);
      fg->AddText(uiFont(UiFont::Ui), 14.0f, {a.x + 16, a.y + 8}, kFg, smithNote_.c_str());
    }
  }

  // A weapon line: icon, name (+enhancement), dice / range / price, "Mão principal", the class bonus; buttons on the right.
  void weaponRow(const Context& c, const e2::InnWeapon& wpn, int enh, float w, float iconSize, bool accent, const std::function<void(float, float)>& buttons,
                 const std::string& cls = "") {
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const float h = std::max(iconSize + 12, 96.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(a, {a.x + w, a.y + h}, accent ? kAccent : IM_COL32(74, 74, 74, 255), 4);
    dl->AddRectFilled({a.x + 6, a.y + 6}, {a.x + 6 + iconSize, a.y + 6 + iconSize}, IM_COL32(0, 0, 0, 255), 2);
    if (const UiImage* img = art(c, wpn.icon)) dl->AddImage(img->texture, {a.x + 6, a.y + 6}, {a.x + 6 + iconSize, a.y + 6 + iconSize});
    const float tx = a.x + 14 + iconSize, tw = w - iconSize - 14 - 148;
    ImFont* ui = uiFont(UiFont::Ui);
    float y = a.y + 6;
    const std::string name = wpn.name + (enh > 0 ? std::format(" +{}", enh) : "");
    dl->AddText(ui, 14.0f, {tx, y}, kFg, name.c_str(), nullptr, tw), y += 18;
    std::string line = wpn.dice + (enh > 0 ? std::format(" + {} aprimoro", enh) : "") + " · " + wpn.range;
    if (enh < 0) line = (wpn.recommendedLevel ? std::format("Nv. {} · ", *wpn.recommendedLevel) : "") + wpn.dice + " · " + wpn.range + std::format(" · {} Gold", wpn.price);
    dl->AddText(ui, 11.0f, {tx, y}, kMuted, line.c_str(), nullptr, tw), y += 15;
    if (enh < 0 && !wpn.magic.empty()) dl->AddText(ui, 11.0f, {tx, y}, kMuted, wpn.magic.c_str(), nullptr, tw), y += ui->CalcTextSizeA(11.0f, FLT_MAX, tw, wpn.magic.c_str()).y + 2;
    spacedText(dl, ui, 10.0f, {tx, y}, kMuted, "MÃO PRINCIPAL", 0.05f, false), y += 14;
    if (!wpn.bonusClass.empty() && !cls.empty())
      dl->AddText(ui, 11.0f, {tx, y}, wpn.bonusClass == cls ? kAccent : kMuted, ("+10% dano · " + wpn.bonusClassName).c_str());
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton("##weapontip", {w - 148, h});
    tip(wpn.tooltip[static_cast<size_t>(std::clamp(enh, 0, static_cast<int>(wpn.tooltip.size()) - 1))]);
    buttons(a.x + w - 140, a.y + 6);
    ImGui::SetCursorScreenPos({a.x, a.y + h + 4});
    ImGui::Dummy({w, 1});
  }

  void gearGrid(Context& c, const std::vector<const e2::InnGear*>& items, float w, bool bagFull) {
    const float cw = (w - 6) / 2;
    const ImVec2 o = ImGui::GetCursorScreenPos();
    int i = 0;
    for (const e2::InnGear* g : items) {
      const ImVec2 a{o.x + (i % 2) * (cw + 6), o.y + (i / 2) * 82.0f};
      ImDrawList* dl = ImGui::GetWindowDrawList();
      dl->AddRect(a, {a.x + cw, a.y + 76}, IM_COL32(74, 74, 74, 255), 4);
      dl->AddRectFilled({a.x + 6, a.y + 6}, {a.x + 70, a.y + 70}, IM_COL32(0, 0, 0, 255), 2);
      if (const UiImage* img = art(c, g->icon)) dl->AddImage(img->texture, {a.x + 6, a.y + 6}, {a.x + 70, a.y + 70});
      ImFont* ui = uiFont(UiFont::Ui);
      dl->PushClipRect({a.x + 76, a.y}, {a.x + cw - 4, a.y + 76}, true);
      dl->AddText(ui, 12.0f, {a.x + 78, a.y + 8}, kFg, g->name.c_str());
      dl->AddText(ui, 10.0f, {a.x + 78, a.y + 26}, kMuted, std::format("{} · {} Gold", g->typeSlot, g->price).c_str());
      dl->PopClipRect();
      ImGui::SetCursorScreenPos(a);
      ImGui::PushID(g->id.c_str());
      ImGui::InvisibleButton("##geartip", {cw, 40});
      tip(g->tooltip);
      ImGui::SetCursorScreenPos({a.x + 78, a.y + 42});
      if (button("##buygear", "Comprar", cw - 86, 26, true, bagFull || ember(c) < g->price, 10.0f)) {
        if (!c.world.bagHasRoom(c.save, 1)) smithNote_ = "Mochila cheia.";
        else if (std::optional<e2::Json> s = c.rules.buyEquipment(c.save, g->id)) {
          apply(c, std::move(s));
          smithNote_ = g->name + " foi colocada na Mochila.";
        } else {
          smithNote_ = "Vargan recusou. Falta Gold.";
        }
      }
      ImGui::PopID();
      ++i;
    }
    ImGui::SetCursorScreenPos({o.x, o.y + ((i + 1) / 2) * 82.0f});
    ImGui::Dummy({w, 1});
  }

  // --- the old healer ----------------------------------------------------------------------------------------------

  int neededCasts(const Context& c, const std::string& hero) const {
    const int hp = c.rules.statusHp(c.save, hero), max = c.rules.statusMaxHp(c.save, hero);
    if (hp <= 0 || hp >= max) return 0;
    const int per = std::max(1, static_cast<int>(std::ceil(max * 0.25)));
    return std::min(4, (max - hp + per - 1) / per);
  }

  // One queued Cura Média every 0.65 s, its glow on the hero treated.
  void stepHealing(Context& c) {
    if (queue_.empty() || now_ - lastCast_ < 0.65f) return;
    const std::string hero = queue_.front();
    queue_.erase(queue_.begin());
    int amount = 0;
    std::optional<e2::Json> next = c.rules.healerCast(c.save, hero, amount);
    if (!next || amount <= 0) {
      healerNote_ = "O Curandeiro não conseguiu completar a cura.";
      queue_.clear(), healing_.clear();
      return;
    }
    apply(c, std::move(next));
    healing_ = hero, healingAmount_ = amount, lastCast_ = now_;
    if (queue_.empty()) casts_.clear();
  }

  void drawHealer(ImVec2 size, Context& c) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, IM_COL32(17, 17, 17, 255));
    if (const UiImage* bg = uiImage(c.engine2Public / "game/assets/brief-estalagem.jpg")) imageCover(dl, *bg, {0, 0}, size);
    verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(17, 17, 17, 64)}, {0.5f, IM_COL32(17, 17, 17, 115)}, {1.0f, IM_COL32(17, 17, 17, 217)}});
    const bool busy = !queue_.empty() || (!healing_.empty() && now_ - lastCast_ < 0.65f);
    if (!busy) healing_.clear();
    bool back = false;
    header(size, c, "VOLTAR", "Curandeiro Ancião", "Cura Média", back, false);
    if (back && !busy) {
      view_ = View::Main;
      return;
    }
    const std::vector<std::string> party = roster(c);
    const float colW = std::min(576.0f, size.x - 32);
    const float x0 = (size.x - colW) / 2, top = 72;
    ImGui::SetCursorScreenPos({x0, top});
    ImGui::BeginChild("##healerbody", {colW + 16, size.y - top - 16}, 0, ImGuiWindowFlags_NoBackground);
    panel(colW, 12, [&](float w) {
      text("“Uma Cura Média restaura 25% do HP máximo. Cinco Gold por conjuração; no máximo quatro por pessoa.”", 14.0f, kFg, w);
      text("Cada conjuração mostra a Cura Média sobre o personagem tratado.", 12.0f, kMuted, w);
    });
    std::vector<std::string> ailing;
    for (const std::string& h : party)
      if (c.rules.diseased(c.save, h) || c.rules.poisoned(c.save, h)) ailing.push_back(h);
    const int ailCost = static_cast<int>(ailing.size()) * c.data.healerAilmentPrice;
    panel(colW, 12, [&](float w) {
      text("Curar doenças e venenos", 14.0f, kFg, w);
      std::string names;
      for (size_t i = 0; i < ailing.size(); ++i) names += (i ? ", " : "") + ailing[i];
      text(ailing.empty() ? std::string("Ninguém do grupo está doente ou envenenado.")
                          : std::format("Tratamento de {} · {} {} × {} Gold.", names, ailing.size(), ailing.size() == 1 ? "pessoa" : "pessoas", c.data.healerAilmentPrice),
           12.0f, kMuted, w);
      text(std::format("Prévia do custo · {} Gold · após o tratamento: {} Gold", ailCost, std::max(0, ember(c) - ailCost)), 14.0f, kFg, w);
      if (button("##cure", std::format("Tratar doenças e venenos · {} Gold", ailCost), w, 36, false, busy || ailing.empty() || ailCost > ember(c))) {
        int count = 0;
        std::optional<e2::Json> next = c.rules.cureAilments(c.save, ailing, count);
        healerNote_ = !next ? "O Curandeiro não conseguiu tratar o grupo." : count == 0 ? "Ninguém precisava de tratamento."
                                                                                       : std::format("{} {}.", count, count == 1 ? "pessoa foi tratada" : "pessoas foram tratadas");
        if (next && count) apply(c, std::move(next));
      }
    });
    int selectedCount = 0, allCount = 0;
    for (const std::string& h : party) {
      const int allowed = neededCasts(c, h);
      const int chosen = std::min(allowed, std::max(0, casts_[h]));
      selectedCount += chosen, allCount += allowed;
      const int max = c.rules.statusMaxHp(c.save, h), hp = c.rules.statusHp(c.save, h);
      const int per = std::max(1, static_cast<int>(std::ceil(max * 0.25)));
      panel(colW, 12, [&](float w) {
        const ImVec2 a = ImGui::GetCursorScreenPos();
        ImDrawList* d = ImGui::GetWindowDrawList();
        const bool glow = healing_ == h;
        if (glow)
          for (int k = 4; k >= 1; --k) d->AddCircleFilled({a.x + 32, a.y + 32}, 32 + k * 4.0f, IM_COL32(255, 211, 105, 30), 32);
        const e2::Json& portrait = c.world.tables()["portraits"].value(h, e2::Json::object());
        if (const UiImage* img = art(c, portrait.value("src", ""))) {
          d->AddImageRounded(img->texture, a, {a.x + 64, a.y + 64}, {0, 0}, {1, 1}, IM_COL32_WHITE, 32);
        }
        if (glow) d->AddCircle({a.x + 32, a.y + 32}, 33, IM_COL32(253, 230, 138, 255), 32, 2);
        ImGui::SetCursorScreenPos({a.x + 76, a.y});
        ImGui::BeginGroup();
        text(h, 14.0f, kFg, w - 76 - 150);
        text(std::format("HP {} / {}{}", hp, max, chosen > 0 ? std::format(" → {} / {}", std::min(max, hp + per * chosen), max) : ""), 14.0f, kFg, w - 76 - 150);
        if (c.rules.diseased(c.save, h) || c.rules.poisoned(c.save, h))
          text(c.rules.diseased(c.save, h) && c.rules.poisoned(c.save, h) ? "Doente · Envenenado" : c.rules.diseased(c.save, h) ? "Doente" : "Envenenado", 12.0f, kDanger,
               w - 226);
        if (hp <= 0) text("Caído · não pode receber cura", 12.0f, kMuted, w - 226);
        if (glow) text(std::format("Cura Média · +{} HP", healingAmount_), 12.0f, kAmber, w - 226);
        ImGui::EndGroup();
        const float bx = a.x + w - 140;
        d->AddText(uiFont(UiFont::Ui), 10.0f, {bx + 20, a.y}, kMuted, "CONJURAÇÕES");
        ImGui::PushID(h.c_str());
        ImGui::SetCursorScreenPos({bx, a.y + 16});
        if (small("##less", "−", busy || chosen <= 0)) casts_[h] = chosen - 1;
        d->AddText(uiFont(UiFont::Ui), 14.0f, {bx + 50, a.y + 24}, kFg, std::format("{}/4", chosen).c_str());
        ImGui::SetCursorScreenPos({bx + 96, a.y + 16});
        if (small("##more", "+", busy || chosen >= allowed || chosen >= 4)) casts_[h] = chosen + 1;
        ImGui::PopID();
        d->AddText(uiFont(UiFont::Ui), 12.0f, {bx + 40, a.y + 52}, kMuted, std::format("{} Gold", chosen * c.data.healerCastPrice).c_str());
        ImGui::SetCursorScreenPos({a.x, std::max(a.y + 70, ImGui::GetItemRectMax().y)});
        ImGui::Dummy({1, 1});
      });
    }
    const int selCost = selectedCount * c.data.healerCastPrice, allCost = allCount * c.data.healerCastPrice;
    panel(colW, 12, [&](float w) {
      text(std::format("Prévia da seleção · {} conjurações · {} Gold", selectedCount, selCost), 14.0f, kFg, w);
      text(std::format("Após a seleção: {} Gold", std::max(0, ember(c) - selCost)), 12.0f, kMuted, w);
      if (!healerNote_.empty()) text(healerNote_, 14.0f, kAmber, w);
      auto treat = [&](bool all) {
        const int cost = all ? allCost : selCost;
        if (cost > ember(c)) {
          healerNote_ = std::format("São necessários {} Gold; você tem {}.", cost, ember(c));
          return;
        }
        healerNote_.clear();
        for (const std::string& h : party) {
          const int n = all ? neededCasts(c, h) : std::min(neededCasts(c, h), std::max(0, casts_[h]));
          for (int k = 0; k < n; ++k) queue_.push_back(h);
        }
        lastCast_ = now_ - 1;
      };
      if (button("##treatsel", std::format("Curar seleção · {} Gold", selCost), (w - 8) / 2, 40, true, busy || selectedCount == 0 || selCost > ember(c))) treat(false);
      ImGui::SameLine();
      if (button("##treatall", std::format("Curar todos · {} Gold", allCost), (w - 8) / 2, 40, false, busy || allCount == 0 || allCost > ember(c))) treat(true);
      text(std::format("Curar todos: {} conjurações necessárias · {} Gold", allCount, allCost), 12.0f, kMuted, w);
    });
    ImGui::EndChild();
  }

  View view_ = View::Main;
  int npc_ = 0;
  bool talked_ = false;
  std::string hero_ = "Kael", smithHero_ = "Kael";
  std::map<std::string, int> cart_;
  int lockpicks_ = 0, rationsQty_ = 0;
  std::string note_, rationsNote_, restNote_, smithNote_, healerNote_;
  float noteAt_ = -10, rationsAt_ = -10, restAt_ = -10, now_ = 0;
  std::map<std::string, int> casts_;
  std::vector<std::string> queue_;
  std::string healing_;
  int healingAmount_ = 0;
  float lastCast_ = -10;
  float scrollTo_ = -1;
  std::optional<InventoryScreen> inventory_;

 public:
  // Captures: which room opens first.
  void preview(const std::string& what) {
    if (what == "smith") view_ = View::Smith;
    if (what == "healer") view_ = View::Healer;
    if (what == "pack") inventory_.emplace("Kael", true);
    if (what == "doll") inventory_.emplace("Kael", false);
    if (what.starts_with("scroll")) scrollTo_ = static_cast<float>(std::atof(what.c_str() + 6));
  }
};

}  // namespace ember
