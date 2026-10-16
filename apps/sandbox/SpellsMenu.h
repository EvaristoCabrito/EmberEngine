#pragma once

// The Spells menu (user, 2026-10-16: "a Spells Menu in test mode with all spells and skills with a drop down menu so
// I can pick and choose; Magic Missile 01, make it serial, for reference sake"). Three lists:
//   Magias / Habilidades  every Engine2 spell and weapon skill (src/vfx/spell-catalog.ts SPELL_ICON_ID: the heroes'
//                         and the enemy-only ones), each with a dropdown of its saved effect versions, OFF first, then
//                         "<name> 01", "<name> 02", ... (render/SpellEffects.h looks(); never renumbered), and Testar
//                         (the picked unit, else the party's first, casts it at the nearest foe; heals at the nearest
//                         friend). A spell with no version yet shows only OFF.
//   Efeitos soltos        every effect image of Engine2 (public/game/fx and effects: the elemental flipbooks only its
//                         dev controls reached, bolts, the arrow, sparks, portals, references), grouped by element and
//                         numbered the same way ("Arcano 01"), each playable alone with Testar. The numbers follow
//                         content/fx.sprites.json; content/fx/effects-list.md maps every number to its file.
// The versions chosen are saved in content/fx/spell-looks.json and used by every cast in battle, one choice for the
// heroes and one for the enemies ("Para": Heróis / Inimigos). Heroes default to Engine2's own effect where one is
// ported (user, 2026-10-16: "set these imported 3D with sprites effects like fireball as default for the player
// characters": SpellEffects::engine2Look); enemies to the spell's first version.

#include <imgui.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "render/SpellEffects.h"

namespace ember {

struct SpellEntry {
  const char* kind;  // Engine2's SpellKind
  const char* name;  // Engine2's Portuguese name (data.ts); [ours] where Engine2 has none in Portuguese
  bool skill;        // a weapon skill (Habilidades), else a spell (Magias)
};

// Every SpellKind of Engine2 (spell-catalog.ts), in its order.
inline const std::vector<SpellEntry>& allSpells() {
  static const std::vector<SpellEntry> kAll = {
      {"fireball", "Bola de Fogo", false},
      {"iceStorm", "Tempestade de Gelo", false},
      {"frost", "Frost", false},
      {"bless", "Bênção", false},
      {"cureMinor", "Cura Menor", false},
      {"cureWounds", "Cura Média", false},
      {"cureLight", "Mãos Curativas", false},  // [ours] Engine2: "Healing Hands"
      {"cureDisease", "Curar Doença", false},
      {"longShot", "Tiro Longo", true},
      {"bloodyShot", "Tiro Sangrento", true},
      {"provoke", "Provocar", true},
      {"piercing", "Tiro Perfurante", true},
      {"lightning", "Relâmpago", false},
      {"lightningTier3", "Relâmpago Maior", false},  // [ours]
      {"magicMissile", "Míssil Mágico", false},
      {"magicMissileV2", "Míssil Mágico V2", false},  // [ours] the same spell's second Engine2 version
      {"causticVenom", "Veneno Cáustico", false},
      {"divineBolt", "Raio Divino", false},
      {"minorVenom", "Veneno Menor", false},
      {"doubleStrike", "Golpe Duplo", true},
      {"cleave", "Cleave", true},
      {"piercingThrust", "Investida Perfurante", true},
      {"sweep", "Varredura", true},
      {"trip", "Rasteira", true},
      {"summonFamiliar", "Invocar Familiar", false},
      {"phantasmalForce", "Força Fantasmagórica", false},
      {"fantomForce", "Força Fantasma", false},
      {"summonFamiliar2", "Familiar Maior", false},
      {"summonFamiliar3", "Familiar 3", false},
      {"summonFamiliar4", "Familiar Radiante", false},
      {"summonZombieDog", "Cão Zumbi", false},
      {"lifeDrain", "Dreno de Vida", false},  // [ours]
      {"webOfDreams", "Teia dos Sonhos", false},
      {"warp", "Portal", false},
      {"multiShot", "Tiro Múltiplo", true},
      {"secondWind", "Segundo Fôlego", true},              // [ours]
      {"auraOfProtection", "Aura de Proteção", false},     // [ours]
      {"divineWrath", "Ira Divina", false},                // [ours]
      {"shoulderSmash", "Ombrada", true},                  // [ours]
      {"intimidatingPresence", "Presença Intimidadora", true},  // [ours]
      {"stampede", "Estouro", true},                       // [ours]
      {"shock", "Choque", false},
      {"bullRush", "Investida", true},
      {"executionerStrike", "Golpe do Carrasco", true},
      {"shieldBash", "Golpe de Escudo", true},
      {"poisonBreath", "Sopro Venenoso", false},
      {"tendrilSwipe", "Golpe de Tentáculo", true},  // [ours]
      {"burningHands", "Mãos Flamejantes", false},
      {"turnUndead", "Expulsar Mortos-Vivos", false},
      {"createFoodAndWater", "Criar Comida e Água", false},
  };
  return kAll;
}

// A friend is the target (heals, blessings), not a foe.
inline bool spellOnFriend(const std::string& kind) {
  return kind.starts_with("cure") || kind == "bless" || kind == "secondWind" || kind == "auraOfProtection" || kind == "createFoodAndWater";
}

class SpellsMenu {
 public:
  struct Group {
    std::string name;                // "Arcano"
    std::vector<std::string> clips;  // fx clip names, in content/fx.sprites.json order: number i + 1
  };

  // Reads the saved choices and the effect images' list.
  void load(const std::filesystem::path& choices, const std::filesystem::path& fxManifest) {
    file_ = choices;
    if (std::ifstream in(choices); in) {
      try {
        const nlohmann::json j = nlohmann::json::parse(in);
        if (j.contains("heroes") || j.contains("enemies")) {
          for (int side = 0; side < 2; ++side)
            if (const char* key = side == 0 ? "heroes" : "enemies"; j.contains(key) && j[key].is_object())
              for (const auto& [kind, look] : j[key].items()) looks_[side][kind] = look.get<int>();
        } else {
          // The first file (one list, written before the sides): the enemies' choices; heroes start on the defaults.
          for (const auto& [kind, look] : j.items()) looks_[1][kind] = look.get<int>();
        }
      } catch (const std::exception& e) {
        logWarn("spells menu: {} unreadable ({}); every spell on its first version", choices.string(), e.what());
      }
    }
    groups_.clear();
    std::ifstream in(fxManifest);
    if (!in) return logWarn("spells menu: no {} (loose effects list empty)", fxManifest.string());
    try {
      const nlohmann::json j = nlohmann::json::parse(in);
      for (const auto& set : j.at("sets"))
        for (const auto& c : set.at("clips")) addLoose(c.at("name").get<std::string>());
    } catch (const std::exception& e) {
      logWarn("spells menu: {} unreadable ({})", fxManifest.string(), e.what());
    }
  }

  // The version a spell plays for a hero or an enemy: the saved choice, else the default (heroes: Engine2's own effect
  // where one is ported, else the first version; enemies: the first version; 0 when it has none).
  int look(const std::string& kind, bool enemy) const {
    const int side = enemy ? 1 : 0;
    const auto it = looks_[side].find(kind);
    const int available = SpellEffects::looks(kind);
    if (it != looks_[side].end()) return std::min(it->second, available);
    if (!enemy && SpellEffects::engine2Look(kind) > 0) return SpellEffects::engine2Look(kind);
    return available > 0 ? 1 : 0;
  }

  // The window. test(kind, version) casts a spell's version; play(clip) plays one loose effect.
  // `open`: shown opened with every list unfolded (captures).
  void draw(const std::function<void(const std::string&, int)>& test, const std::function<void(const std::string&)>& play, bool open = false) {
    ImGui::SetNextWindowPos({8, open ? 44.0f : 300.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(!open, ImGuiCond_FirstUseEver);
    const ImGuiTreeNodeFlags unfold = open ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    ImGui::SetNextWindowSizeConstraints({360, 120}, {560, 700});
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::Begin("Magias", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PushFont(nullptr, 16.0f);
    {
      // Which side the lists below choose for.
      static const char* kSides[2] = {"Heróis", "Inimigos"};
      ImGui::SetNextItemWidth(160.0f);
      if (ImGui::BeginCombo("Para", kSides[side_])) {
        for (int i = 0; i < 2; ++i)
          if (ImGui::Selectable(kSides[i], i == side_)) side_ = i;
        ImGui::EndCombo();
      }
    }
    for (int section = 0; section < 2; ++section) {
      if (!ImGui::CollapsingHeader(section == 0 ? "Magias" : "Habilidades", unfold)) continue;
      for (const SpellEntry& s : allSpells())
        if (s.skill == (section == 1)) spellRow(s, test);
    }
    if (ImGui::CollapsingHeader("Efeitos soltos", unfold)) {
      for (const Group& g : groups_) {
        int& pick = loosePick_[g.name];
        ImGui::PushID(g.name.c_str());
        ImGui::SetNextItemWidth(230.0f);
        if (ImGui::BeginCombo("##loose", std::format("{} {:02}", g.name, pick + 1).c_str())) {
          for (int i = 0; i < static_cast<int>(g.clips.size()); ++i) {
            if (ImGui::Selectable(std::format("{} {:02}", g.name, i + 1).c_str(), i == pick)) pick = i;
            ImGui::SetItemTooltip("%s", g.clips[static_cast<size_t>(i)].c_str());
          }
          ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("%s", g.clips[static_cast<size_t>(pick)].c_str());
        ImGui::SameLine();
        if (ImGui::Button("Testar")) play(g.clips[static_cast<size_t>(pick)]);
        ImGui::PopID();
      }
    }
    ImGui::PopFont();
    ImGui::End();
  }

  const std::vector<Group>& groups() const { return groups_; }

 private:
  // Engine2's effect images by element (file name prefixes), in this order.
  void addLoose(const std::string& clip) {
    static const std::pair<const char*, const char*> kGroups[] = {
        {"arcane", "Arcano"}, {"ember", "Brasa"},     {"fireball", "Bola de Fogo"}, {"fire", "Fogo"},     {"frost", "Gelo"},
        {"holy", "Sagrado"},  {"lightning", "Raio"},  {"poison", "Veneno"},        {"caustic", "Veneno Cáustico"},
        {"shadow", "Sombra"}, {"warp-portal", "Portal (arte)"}, {"warp", "Portal"}, {"arrow", "Flecha"},
        {"impact", "Impacto"}, {"blizzard", "Nevasca"}, {"webfloor", "Teia"}};
    std::string name = "Outros";
    for (const auto& [prefix, label] : kGroups)
      if (clip.starts_with(prefix)) {
        name = label;
        break;
      }
    for (Group& g : groups_)
      if (g.name == name) return g.clips.push_back(clip);
    groups_.push_back({name, {clip}});
  }

  void spellRow(const SpellEntry& s, const std::function<void(const std::string&, int)>& test) {
    const int available = SpellEffects::looks(s.kind);
    int current = look(s.kind, side_ == 1);
    ImGui::PushID(s.kind);
    ImGui::SetNextItemWidth(260.0f);
    const auto label = [&](int i) { return i == 0 ? std::format("{}: OFF", s.name) : std::format("{} {:02}", s.name, i); };
    if (ImGui::BeginCombo("##look", label(current).c_str())) {
      for (int i = 0; i <= available; ++i)
        if (ImGui::Selectable(label(i).c_str(), i == current)) {
          current = i;
          looks_[side_][s.kind] = i;
          save();
        }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (available == 0) {
      ImGui::TextDisabled("sem efeito ainda");
    } else {
      ImGui::BeginDisabled(current == 0);
      if (ImGui::Button("Testar")) test(s.kind, current);
      ImGui::EndDisabled();
    }
    ImGui::PopID();
  }

  void save() const {
    // Only what was chosen is written (a spell left on its default follows the default as it changes).
    nlohmann::json j = {{"heroes", nlohmann::json::object()}, {"enemies", nlohmann::json::object()}};
    for (int side = 0; side < 2; ++side)
      for (const auto& [kind, look] : looks_[side]) j[side == 0 ? "heroes" : "enemies"][kind] = look;
    std::error_code ec;
    std::filesystem::create_directories(file_.parent_path(), ec);
    std::ofstream out(file_);
    if (!out) return logWarn("spells menu: cannot write {}", file_.string());
    out << j.dump(1) << "\n";
  }

  std::filesystem::path file_;
  std::map<std::string, int> looks_[2];  // chosen versions: [0] heroes, [1] enemies
  int side_ = 0;                          // the side the menu is choosing for
  std::vector<Group> groups_;
  std::map<std::string, int> loosePick_;
};

}  // namespace ember
