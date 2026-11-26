#pragma once

// The Spells menu (user, 2026-10-16: "a Spells Menu in test mode with all spells and skills with a drop down menu so
// I can pick and choose; Magic Missile 01, make it serial, for reference sake"). Four lists:
//   Magias / Habilidades  every Ember spell and weapon skill (src/vfx/spell-catalog.ts SPELL_ICON_ID: the heroes'
//                         and the enemy-only ones), each with a dropdown of its saved effect versions, OFF first, then
//                         "<name> 01", "<name> 02", ... (render/SpellEffects.h looks(); never renumbered), and Testar
//                         (the picked unit, else the party's first, casts it at the nearest foe; heals at the nearest
//                         friend). A spell with no version yet shows only OFF.
//   Itens                 what a hero uses from the bag (Poção), with its versions the same way.
//   Efeitos soltos        every effect image of Ember (public/game/fx and effects: the elemental flipbooks only its
//                         dev controls reached, bolts, the arrow, sparks, portals, references), grouped by element and
//                         numbered the same way ("Arcano 01"), each playable alone with Testar. The numbers follow
//                         content/fx.sprites.json; content/fx/effects-list.md maps every number to its file.
// The versions chosen are saved in content/fx/spell-looks.json and used by every cast in battle, one choice for the
// heroes and one for the enemies ("Para": Heróis / Inimigos). Heroes default to Ember's own effect where one is
// ported (user, 2026-10-16: "set these imported 3D with sprites effects like fireball as default for the player
// characters": SpellEffects::classicLook); enemies to the spell's first version.

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
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
  const char* kind;      // Ember's SpellKind
  const char* name;      // Portuguese name
  const char* enName;    // English name for instant recognition and search
  int tier;              // 1..10, or 0 (Truque / Habilidade Especial)
  const char* category;  // "Fogo", "Gelo", "Raio", "Arcano", "Sagrado", "Trevas", "Veneno", "Invocação", "Marcial"
  bool skill;            // a weapon skill (Habilidades), else a spell (Magias)
};

// Every SpellKind of Ember (spell-catalog.ts) and EmberEngine, with tier, category, and bilingual names.
inline const std::vector<SpellEntry>& allSpells() {
  static const std::vector<SpellEntry> kAll = {
      // --- ARCANO ---
      {"magicMissile", "Míssil Mágico", "Magic Missile", 1, "Arcano", false},
      {"phantasmalForce", "Força Fantasmagórica", "Phantasmal Force", 1, "Arcano", false},
      {"placeSwap", "Troca de Lugar", "Place Swap", 1, "Arcano", false},
      {"arcaneBlade", "Lâmina Arcana", "Arcane Blade", 1, "Arcano", true},
      {"arcaneBeam", "Feixe de Luz", "Arcane Beam", 2, "Arcano", false},
      {"anchoredLight", "Luz Ancorada", "Anchored Light", 2, "Arcano", false},
      {"webOfDreams", "Teia dos Sonhos", "Web of Dreams", 2, "Arcano", false},
      {"mirrorImage", "Imagem Espelhada", "Mirror Image", 2, "Arcano", false},
      {"warp", "Portal", "Warp", 3, "Arcano", false},
      {"arcaneReflection", "Reflexo Arcano", "Arcane Reflection", 3, "Arcano", false},
      {"dispelMagic", "Dissipar Magia", "Dispel Magic", 3, "Arcano", false},
      {"cloudOfDaggers", "Nuvem de Adagas", "Cloud of Daggers", 4, "Arcano", false},
      {"nightmare", "Pesadelo", "Nightmare", 4, "Arcano", false},
      {"detonateSummon", "Detonar Invocação", "Detonate Summon", 5, "Arcano", false},
      {"sacrificeBond", "Elo de Sacrifício", "Sacrifice Bond", 5, "Arcano", false},
      {"globeOfInvulnerability", "Globo de Invulnerabilidade", "Globe of Invulnerability", 5, "Arcano", false},
      {"phantasmalForce2", "Força Fantasmagórica II", "Phantasmal Force II", 6, "Arcano", false},
      {"fantomForce", "Força Fantasma", "Phantom Force", 0, "Arcano", false},
      {"witchStitch", "Costura da Bruxa", "Witch Stitch", 0, "Arcano", false},
      {"burialWeight", "Peso Sepulcral", "Burial Weight", 0, "Arcano", false},
      {"abyssalSwap", "Permuta Abissal", "Abyssal Swap", 0, "Arcano", false},
      {"arcaneBanquet", "Banquete Arcano", "Arcane Banquet", 0, "Arcano", false},
      {"twinResonance", "Ressonância Geminada", "Twin Resonance", 0, "Arcano", false},

      // --- FOGO ---
      {"burningHands", "Mãos Flamejantes", "Burning Hands", 2, "Fogo", false},
      {"ignite", "Incendiar", "Ignite", 2, "Fogo", false},
      {"fireball", "Bola de Fogo", "Fireball", 3, "Fogo", false},
      {"pillarOfFlame", "Coluna de Fogo", "Pillar of Flame", 6, "Fogo", false},
      {"meteor", "Meteoro", "Meteor", 7, "Fogo", false},
      {"eruption", "Erupção", "Eruption", 9, "Fogo", false},
      {"primordialEmber", "Brasa Primordial", "Primordial Ember", 10, "Fogo", false},

      // --- GELO ---
      {"frost", "Gelo / Toque Frio", "Frost", 2, "Gelo", false},
      {"iceStorm", "Tempestade de Gelo", "Ice Storm", 4, "Gelo", false},
      {"iceLance", "Lança de Gelo", "Ice Lance", 6, "Gelo", false},
      {"absoluteZero", "Zero Absoluto", "Absolute Zero", 8, "Gelo", false},
      {"glacialGale", "Vendaval Glacial", "Glacial Gale", 9, "Gelo", false},

      // --- ÁGUA ---
      {"waterJet", "Jato d'Água", "Water Jet", 1, "Água", false},
      {"tidalWave", "Maremoto", "Tidal Wave", 6, "Água", false},

      // --- RAIO / ELETRICIDADE ---
      {"shock", "Choque", "Shock", 0, "Raio", false},
      {"lightning", "Relâmpago", "Lightning", 2, "Raio", false},
      {"lightningTier3", "Relâmpago Maior", "Lightning Bolt III", 5, "Raio", false},
      {"thunderstorm", "Tempestade Elétrica", "Thunderstorm", 7, "Raio", false},
      {"chainLightning", "Cadeia de Raios", "Chain Lightning", 8, "Raio", false},
      {"wrathOfTheSkies", "Fúria dos Céus", "Wrath of the Skies", 10, "Raio", false},

      // --- SAGRADO & CURA ---
      {"bless", "Bênção", "Bless", 1, "Sagrado", false},
      {"cureMinor", "Cura Menor", "Cure Minor", 1, "Sagrado", false},
      {"holyStrike", "Golpe Consagrado", "Holy Strike", 1, "Sagrado", true},
      {"cureWounds", "Cura Média", "Cure Wounds", 2, "Sagrado", false},
      {"luminousHalo", "Halo Luminoso", "Luminous Halo", 2, "Sagrado", false},
      {"holyTouch", "Toque Sagrado", "Holy Touch", 2, "Sagrado", false},
      {"turnUndead", "Expulsar Mortos-Vivos", "Turn Undead", 3, "Sagrado", false},
      {"cureDisease", "Curar Doença", "Cure Disease", 3, "Sagrado", false},
      {"crusaderOath", "Juramento", "Crusader's Oath", 3, "Sagrado", false},
      {"crusaderMantle", "Manto do Cruzado", "Crusader's Mantle", 3, "Sagrado", false},
      {"cureLight", "Mãos Curativas", "Healing Hands", 4, "Sagrado", false},
      {"createFoodAndWater", "Criar Comida e Água", "Create Food and Water", 4, "Sagrado", false},
      {"divineBolt", "Raio Divino", "Divine Bolt", 4, "Sagrado", false},
      {"circleOfHealing", "Círculo de Cura", "Circle of Healing", 4, "Sagrado", false},
      {"auraOfProtection", "Aura de Proteção", "Aura of Protection", 5, "Sagrado", false},
      {"resurrection", "Ressurreição", "Resurrection", 5, "Sagrado", false},
      {"judgment", "Julgamento", "Judgment", 5, "Sagrado", false},
      {"divineWrath", "Ira Divina", "Divine Wrath", 6, "Sagrado", false},

      // --- TREVAS & NECROMANCIA ---
      {"spectralBolt", "Raio Espectral", "Spectral Bolt", 0, "Trevas", false},
      {"familiarLifeDrain", "Dreno de Vida", "Familiar Life Drain", 0, "Trevas", false},
      {"markOfRuin", "Marca da Ruína", "Mark of Ruin", 6, "Trevas", false},
      {"boneLance", "Lança de Ossos", "Bone Lance", 6, "Trevas", false},
      {"lifeDrain", "Dreno de Vida", "Life Drain", 7, "Trevas", false},
      {"shroudOfShadows", "Mortalha de Sombras", "Shroud of Shadows", 7, "Trevas", false},
      {"hemorrhage", "Hemorragia", "Hemorrhage", 8, "Trevas", false},
      {"profaneSilence", "Silêncio Profano", "Profane Silence", 8, "Trevas", false},
      {"curseOfDecay", "Maldição da Decadência", "Curse of Decay", 9, "Trevas", false},
      {"soulHarvest", "Colheita de Almas", "Soul Harvest", 10, "Trevas", false},
      {"devouringVoid", "Vazio Devorador", "Devouring Void", 10, "Trevas", false},

      // --- VENENO & ÁCIDO ---
      {"poisonBreath", "Sopro Venenoso", "Poison Breath", 1, "Veneno", false},
      {"acidArrow", "MILF's Magic Arrow", "MILF's Magic Arrow", 2, "Veneno", false},
      {"acidCloud", "Névoa Ácida", "Acid Cloud", 3, "Veneno", false},
      {"minorVenom", "Veneno Menor", "Minor Venom", 4, "Veneno", false},
      {"causticVenom", "Veneno Cáustico", "Caustic Venom", 5, "Veneno", false},
      {"creepingPlague", "Praga Rastejante", "Creeping Plague", 6, "Veneno", false},
      {"blackPestilence", "Peste Negra", "Black Pestilence", 9, "Veneno", false},

      // --- INVOCAÇÕES ---
      {"summonFamiliar", "Invocar Familiar", "Summon Familiar", 1, "Invocação", false},
      {"summonFamiliar2", "Familiar Maior", "Greater Familiar", 2, "Invocação", false},
      {"summonTelesm", "Invocar Telesm", "Summon Telesm", 3, "Invocação", false},
      {"summonFamiliar4", "Familiar Radiante", "Radiant Familiar", 3, "Invocação", false},
      {"empowerSummons", "Fortalecer Invocações", "Empower Summons", 4, "Invocação", false},
      {"summonFamiliar3", "Invocar Numen", "Summon Numen", 6, "Invocação", false},
      {"summonZombieDog", "Cão Zumbi", "Zombie Dog", 8, "Invocação", false},
      {"summonSwampDeer", "Cervo Azul do Pântano", "Swamp Deer", 8, "Invocação", false},
      {"summonHypostasis", "Invocar Hipóstase", "Summon Hypostasis", 10, "Invocação", false},

      // --- HABILIDADES MARCIAIS / ARMAS ---
      {"tendrilSwipe", "Golpe do Tentáculo", "Tendril Swipe", 0, "Marcial", true},
      {"longShot", "Tiro Longo", "Long Shot", 1, "Marcial", true},
      {"doubleStrike", "Golpe Duplo", "Double Strike", 1, "Marcial", true},
      {"bullRush", "Investida", "Bull Rush", 1, "Marcial", true},
      {"piercingThrust", "Investida Perfurante", "Piercing Thrust", 1, "Marcial", true},
      {"provoke", "Provocar", "Provoke", 1, "Marcial", true},
      {"venomArrow", "MILF's Magic Arrow (Arqueiro)", "MILF's Magic Arrow", 1, "Marcial", true},
      {"spearToss", "Arremesso de Lança", "Spear Toss", 1, "Marcial", true},
      {"cleave", "Cutilada", "Cleave", 2, "Marcial", true},
      {"sweep", "Varredura", "Sweep", 2, "Marcial", true},
      {"shieldBash", "Golpe de Escudo", "Shield Bash", 2, "Marcial", true},
      {"piercing", "Tiro Perfurante", "Piercing Shot", 2, "Marcial", true},
      {"bloodyShot", "Tiro Sangrento", "Bloody Shot", 2, "Marcial", true},
      {"hookPull", "Gancho", "Hook Pull", 2, "Marcial", true},
      {"multiShot", "Tiro Múltiplo", "Multi-Shot", 3, "Marcial", true},
      {"executionerStrike", "Golpe do Carrasco", "Executioner's Strike", 3, "Marcial", true},
      {"trip", "Rasteira", "Trip", 3, "Marcial", true},
      {"secondWind", "Segundo Fôlego", "Second Wind", 3, "Marcial", true},
      {"arrowRain", "Chuva de Flechas", "Arrow Rain", 3, "Marcial", true},
      {"poleVault", "Salto de Vara", "Pole Vault", 3, "Marcial", true},
      {"shoulderSmash", "Ombrada", "Shoulder Smash", 4, "Marcial", true},
      {"steelWhirlwind", "Redemoinho de Aço", "Steel Whirlwind", 4, "Marcial", true},
      {"guardBreak", "Quebra-Guarda", "Guard Break", 4, "Marcial", true},
      {"explosiveArrow", "Flecha Explosiva", "Explosive Arrow", 4, "Marcial", true},
      {"cripplingShot", "Tiro Paralisante", "Crippling Shot", 4, "Marcial", true},
      {"disarm", "Desarmar", "Disarm", 4, "Marcial", true},
      {"swapPost", "Troca de Posto", "Swap Post", 4, "Marcial", true},
      {"intimidatingPresence", "Presença Intimidadora", "Intimidating Presence", 5, "Marcial", true},
      {"warCry", "Grito de Guerra", "War Cry", 5, "Marcial", false},
      {"leapingStrike", "Salto Devastador", "Leaping Strike", 5, "Marcial", true},
      {"arrowStorm", "Tempestade de Flechas", "Arrow Storm", 5, "Marcial", true},
      {"deathShot", "Tiro Mortal", "Death Shot", 5, "Marcial", true},
      {"impaleDrag", "Empalar e Arrastar", "Impale & Drag", 5, "Marcial", true},
      {"sentinelVigil", "Vigília da Sentinela", "Sentinel Vigil", 5, "Marcial", false},
      {"sentinelStrike", "Estocada da Sentinela", "Sentinel Strike", 5, "Marcial", true},
      {"stampede", "Estouro / Debandada", "Stampede", 6, "Marcial", true},
  };
  return kAll;
}

// What a hero uses from the bag in battle (Ember usePotion): kind, name.
inline const std::vector<SpellEntry>& allItems() {
  static const std::vector<SpellEntry> kAll = {{"potion", "Poção de Cura", "Healing Potion", 0, "Item", false}};
  return kAll;
}

// Case-insensitive search match for Portuguese name, English name, kind, category or tier
inline bool matchesSpellSearch(const SpellEntry& s, const std::string& query) {
  if (query.empty()) return true;
  auto lower = [](std::string str) {
    for (char& c : str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return str;
  };
  const std::string q = lower(query);
  if (lower(s.name).find(q) != std::string::npos) return true;
  if (lower(s.enName).find(q) != std::string::npos) return true;
  if (lower(s.kind).find(q) != std::string::npos) return true;
  if (lower(s.category).find(q) != std::string::npos) return true;
  const std::string tStr = std::format("t{}", s.tier);
  const std::string tierStr = std::format("tier {}", s.tier);
  if (q == tStr || q == tierStr) return true;
  return false;
}

// A friend is the target (heals, blessings, potions), not a foe.
inline bool spellOnFriend(const std::string& kind) {
  return kind.starts_with("cure") || kind == "bless" || kind == "secondWind" || kind == "auraOfProtection" || kind == "createFoodAndWater" ||
         kind == "potion" || kind == "circleOfHealing" || kind == "resurrection" || kind == "placeSwap" || kind == "mirrorImage" ||
         kind == "detonateSummon" || kind == "sacrificeBond" || kind == "empowerSummons" || kind == "holyTouch" || kind == "crusaderOath" || kind == "crusaderMantle";
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

  // The version a spell plays for a hero or an enemy: the saved choice, else the default (heroes: Ember's own effect
  // where one is ported, else the first version; enemies: the first version; 0 when it has none).
  int look(const std::string& kind, bool enemy) const {
    const std::string resolved = kind == "magicMissileV2" ? "magicMissile" : kind;
    const int side = enemy ? 1 : 0;
    const auto it = looks_[side].find(resolved);
    const int available = SpellEffects::looks(resolved);
    if (it != looks_[side].end()) return std::min(it->second, available);
    if (!enemy && SpellEffects::classicLook(resolved) > 0) return SpellEffects::classicLook(resolved);
    return available > 0 ? 1 : 0;
  }

  // The window. test(kind, version) casts a spell's version; play(clip) plays one loose effect.
  // `open`: shown opened with every list unfolded (captures).
  void draw(const std::function<void(const std::string&, int)>& test, const std::function<void(const std::string&)>& play,
            const std::function<void(VfxElement, int)>& previewElement, bool open = false) {
    ImGui::SetNextWindowPos({8, open ? 44.0f : 300.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(!open, ImGuiCond_FirstUseEver);
    const ImGuiTreeNodeFlags unfold = open ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    ImGui::SetNextWindowSizeConstraints({460, 200}, {760, 880});
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("Magias", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PushFont(nullptr, 16.0f);
    {
      // Which side the lists below choose for.
      static const char* kSides[2] = {"Heróis", "Inimigos"};
      ImGui::SetNextItemWidth(120.0f);
      if (ImGui::BeginCombo("Para", kSides[side_])) {
        for (int i = 0; i < 2; ++i)
          if (ImGui::Selectable(kSides[i], i == side_)) side_ = i;
        ImGui::EndCombo();
      }
      ImGui::SameLine(0, 14.0f);
      ImGui::Text("Organizar:");
      ImGui::SameLine();
      if (ImGui::RadioButton("Por Tier", viewMode_ == 0)) viewMode_ = 0;
      ImGui::SameLine();
      if (ImGui::RadioButton("Por Tipo / Escola", viewMode_ == 1)) viewMode_ = 1;
    }

    // Campo de busca rápida
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputTextWithHint("##searchSpells", "Buscar (ex: fogo, cura, cleave, t3)...", searchBuffer_, sizeof(searchBuffer_));
    if (searchBuffer_[0] != '\0') {
      ImGui::SameLine();
      if (ImGui::Button("Limpar")) searchBuffer_[0] = '\0';
    }

    const std::string query = searchBuffer_;
    if (!query.empty()) {
      ImGui::SeparatorText("Resultados da Busca");
      int matches = 0;
      for (const SpellEntry& s : allSpells()) {
        if (matchesSpellSearch(s, query)) {
          spellRow(s, test, true, true);
          ++matches;
        }
      }
      for (const SpellEntry& s : allItems()) {
        if (matchesSpellSearch(s, query)) {
          spellRow(s, test, true, true);
          ++matches;
        }
      }
      if (matches == 0) {
        ImGui::TextDisabled("Nenhuma magia ou habilidade encontrada.");
      }
    } else if (viewMode_ == 0) {
      // --- Organizado por Tier (T1 a T10 + Truques / Especiais T0) ---
      static const int kTiers[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0};
      for (int t : kTiers) {
        std::vector<const SpellEntry*> list;
        for (const SpellEntry& s : allSpells()) {
          if (s.tier == t) list.push_back(&s);
        }
        if (list.empty()) continue;

        std::stable_sort(list.begin(), list.end(), [](const SpellEntry* a, const SpellEntry* b) {
          if (std::string(a->category) != std::string(b->category))
            return std::string(a->category) < std::string(b->category);
          return std::string(a->name) < std::string(b->name);
        });

        const std::string header = t == 0 ? std::format("Truques & Especiais (Tier 0) ({})", list.size())
                                          : std::format("Tier {} ({} magias)", t, list.size());
        if (ImGui::CollapsingHeader(header.c_str(), unfold)) {
          std::string lastCat = "";
          for (const SpellEntry* s : list) {
            if (s->category != lastCat) {
              lastCat = s->category;
              ImGui::Spacing();
              ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.45f, 1.0f), "--- %s ---", lastCat.c_str());
            }
            spellRow(*s, test, false, true);
          }
        }
      }
    } else {
      // --- Organizado por Tipo / Escola de Magia ---
      static const struct {
        const char* id;
        const char* label;
      } kCategories[] = {
          {"Fogo", "Fogo"},
          {"Gelo", "Gelo"},
          {"Raio", "Raio / Eletricidade"},
          {"Arcano", "Arcano"},
          {"Sagrado", "Sagrado & Cura"},
          {"Trevas", "Trevas & Necromancia"},
          {"Veneno", "Veneno & Ácido"},
          {"Invocação", "Invocações"},
          {"Marcial", "Habilidades Marciais / Armas"},
      };
      for (const auto& cat : kCategories) {
        std::vector<const SpellEntry*> list;
        for (const SpellEntry& s : allSpells()) {
          if (std::string(s.category) == cat.id) list.push_back(&s);
        }
        if (list.empty()) continue;

        std::stable_sort(list.begin(), list.end(), [](const SpellEntry* a, const SpellEntry* b) {
          int ta = a->tier == 0 ? 99 : a->tier;
          int tb = b->tier == 0 ? 99 : b->tier;
          if (ta != tb) return ta < tb;
          return std::string(a->name) < std::string(b->name);
        });

        const std::string header = std::format("{} ({} magias)", cat.label, list.size());
        if (ImGui::CollapsingHeader(header.c_str(), unfold)) {
          int lastTier = -1;
          for (const SpellEntry* s : list) {
            if (s->tier != lastTier) {
              lastTier = s->tier;
              ImGui::Spacing();
              if (lastTier == 0) ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "--- Truques / Especial ---");
              else ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.45f, 1.0f), "--- Tier %d ---", lastTier);
            }
            spellRow(*s, test, true, false);
          }
        }
      }
    }

    if (ImGui::CollapsingHeader("Itens", unfold))
      for (const SpellEntry& s : allItems()) spellRow(s, test, false, false);
    if (ImGui::CollapsingHeader("Efeitos elementais", unfold)) {
      for (int i = 0; i < 8; ++i) {
        const auto element = static_cast<VfxElement>(i);
        int& version = elementVersions_[static_cast<size_t>(i)];
        ImGui::PushID(i);
        ImGui::SetNextItemWidth(230.0f);
        const std::string selected = std::format("{} V{}", vfxElementLabel(element), version);
        if (ImGui::BeginCombo("##elementVersion", selected.c_str())) {
          for (int choice : {2, 3})
            if (ImGui::Selectable(std::format("{} V{}", vfxElementLabel(element), choice).c_str(), version == choice)) version = choice;
          ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Visualizar")) previewElement(element, version);
        ImGui::PopID();
      }
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
  // Ember's effect images by element (file name prefixes), in this order.
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

  void spellRow(const SpellEntry& s, const std::function<void(const std::string&, int)>& test, bool showTier = true, bool showCat = true) {
    const int available = SpellEffects::looks(s.kind);
    int current = look(s.kind, side_ == 1);
    ImGui::PushID(s.kind);

    std::string prefix;
    if (showTier) {
      if (s.tier > 0) prefix += std::format("[T{}] ", s.tier);
      else prefix += "[T0] ";
    }
    if (showCat) {
      prefix += std::format("[{}] ", s.category);
    }
    const std::string fullName = std::format("{}{} ({})", prefix, s.name, s.enName);

    ImGui::SetNextItemWidth(340.0f);
    const auto label = [&](int i) { return i == 0 ? std::format("{}: OFF", fullName) : std::format("{} {:02}", fullName, i); };
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
  int viewMode_ = 0;                     // 0 = Por Tier, 1 = Por Tipo / Escola
  char searchBuffer_[64] = "";
  std::vector<Group> groups_;
  std::map<std::string, int> loosePick_;
  std::array<int, 8> elementVersions_{2, 2, 2, 2, 2, 2, 2, 2};
};

}  // namespace ember
