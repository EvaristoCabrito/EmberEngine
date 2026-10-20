#include "world/Spells.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <array>
#include <cmath>
#include <format>
#include <fstream>
#include <map>
#include <set>

#include "core/Log.h"
#include "world/Progression.h"

namespace ember::e2 {

namespace {

constexpr double kSqrt3 = 1.7320508075688772;  // Math.sqrt(3)
double jsRound(double v) { return std::floor(v + 0.5); }

// --- Engine2 data.ts numbers --------------------------------------------------------------------------------------

struct SpellData {
  const char* kind;
  const char* name;
  int tier;             // SPELL_TIER (0 = none)
  const char* element;  // spellElement ("" = physical / utility)
  bool weaponAbility;   // isWeaponAbility
};
constexpr SpellData kSpells[] = {
    {"magicMissile", "Míssil Mágico", 1, "arcane", false},
    {"magicMissileV2", "Míssil Mágico", 1, "arcane", false},
    {"longShot", "Tiro Longo", 1, "", true},
    {"piercing", "Tiro Perfurante", 2, "", true},
    {"piercingThrust", "Investida Perfurante", 1, "", true},
    {"doubleStrike", "Golpe Duplo", 1, "", true},
    {"lightning", "Relâmpago", 2, "lightning", false},
    {"frost", "Frost", 2, "ice", false},
    {"shock", "Choque", 0, "lightning", false},
    {"cureMinor", "Cura Menor", 1, "", false},
    {"cureWounds", "Cura Média", 2, "", false},
    {"cureLight", "Mãos Curativas", 4, "", false},  // Engine2 CURES.cureLight "Healing Hands"
    {"cureDisease", "Curar Doença", 3, "", false},
    {"createFoodAndWater", "Criar Comida e Água", 4, "", false},
    {"fantomForce", "Força Fantasma", 0, "arcane", false},
    // listed (a unit's own spell bar) but not ported yet
    {"bloodyShot", "Tiro Sangrento", 2, "", true},
    {"multiShot", "Tiro Múltiplo", 3, "", true},
    {"provoke", "Provocar", 1, "", false},
    {"cleave", "Cleave", 2, "", true},
    {"sweep", "Varredura", 2, "", true},
    {"trip", "Rasteira", 3, "", true},
    {"bullRush", "Investida", 1, "", true},
    {"shieldBash", "Golpe de Escudo", 2, "", false},
    {"executionerStrike", "Golpe do Carrasco", 3, "", true},
    {"fireball", "Bola de Fogo", 3, "fire", false},
    {"iceStorm", "Tempestade de Gelo", 4, "ice", false},
    {"causticVenom", "Veneno Cáustico", 5, "poison", false},
    {"poisonBreath", "Sopro Venenoso", 1, "poison", false},
    {"warp", "Portal", 3, "arcane", false},
    {"bless", "Bênção", 1, "", false},
    {"burningHands", "Mãos Flamejantes", 2, "fire", false},
    {"turnUndead", "Expulsar Mortos-Vivos", 3, "holy", false},
    {"divineBolt", "Raio Divino", 4, "holy", false},
    {"summonFamiliar", "Invocar Familiar", 1, "", false},
    {"phantasmalForce", "Força Fantasmagórica", 1, "arcane", false},
    {"webOfDreams", "Teia dos Sonhos", 2, "arcane", false},
    {"summonFamiliar2", "Familiar Maior", 2, "", false},
    {"summonFamiliar3", "Familiar 3", 4, "", false},
    {"summonFamiliar4", "Familiar Radiante", 3, "", false},
    {"summonZombieDog", "Cão Zumbi", 5, "", false},
    {"lightningTier3", "Relâmpago 3", 5, "lightning", false},
    {"minorVenom", "Veneno Menor", 4, "poison", false},
    {"familiarLifeDrain", "Dreno de Vida", 0, "darkness", false},
    {"divineWrath", "Ira Divina", 6, "holy", false},
    {"shoulderSmash", "Investida de Ombro", 4, "", false},
    {"stampede", "Debandada", 6, "", false},
    {"auraOfProtection", "Aura de Proteção", 5, "", false},
    {"intimidatingPresence", "Presença Intimidante", 5, "", false},
    {"secondWind", "Fôlego Renovado", 3, "", false},
};
// EmberEngine's own spells: the Mago Negro's promotions, two per tier from T6 to T10 (docs/SPELLS_PROMOTIONS.md).
struct EmberSpell {
  SpellData data;
  const char* cls;  // the class that learns it
  int learn;        // at this level
};
constexpr EmberSpell kEmberSpells[] = {
    // Bruxo: enfeeblement and damage over time
    {{"markOfRuin", "Marca da Ruína", 6, "darkness", false}, "warlock", 15},
    {{"creepingPlague", "Praga Rastejante", 6, "poison", false}, "warlock", 15},
    {{"lifeDrain", "Dreno de Vida", 7, "darkness", false}, "warlock", 16},
    {{"shroudOfShadows", "Mortalha de Sombras", 7, "darkness", false}, "warlock", 17},
    {{"hemorrhage", "Hemorragia", 8, "darkness", false}, "warlock", 18},
    {{"profaneSilence", "Silêncio Profano", 8, "darkness", false}, "warlock", 19},
    {{"blackPestilence", "Peste Negra", 9, "poison", false}, "warlock", 21},
    {{"curseOfDecay", "Maldição da Decadência", 9, "darkness", false}, "warlock", 22},
    {{"soulHarvest", "Colheita de Almas", 10, "darkness", false}, "warlock", 23},
    {{"devouringVoid", "Vazio Devorador", 10, "darkness", false}, "warlock", 25},
    // Elementalista: pure damage and area
    {{"pillarOfFlame", "Coluna de Fogo", 6, "fire", false}, "elementalist", 15},
    {{"iceLance", "Lança de Gelo", 6, "ice", false}, "elementalist", 15},
    {{"thunderstorm", "Tempestade Elétrica", 7, "lightning", false}, "elementalist", 16},
    {{"meteor", "Meteoro", 7, "fire", false}, "elementalist", 17},
    {{"absoluteZero", "Zero Absoluto", 8, "ice", false}, "elementalist", 18},
    {{"chainLightning", "Cadeia de Raios", 8, "lightning", false}, "elementalist", 19},
    {{"eruption", "Erupção", 9, "fire", false}, "elementalist", 21},
    {{"glacialGale", "Vendaval Glacial", 9, "ice", false}, "elementalist", 22},
    {{"wrathOfTheSkies", "Fúria dos Céus", 10, "lightning", false}, "elementalist", 23},
    {{"primordialEmber", "Brasa Primordial", 10, "ember", false}, "elementalist", 25},
    // Guerreiro (docs/SKILLS_WARRIOR.md, 2026-10-18): weapon skills (the weapon's own roll + bonus dice), two per tier T4 / T5
    {{"steelWhirlwind", "Redemoinho de Aço", 4, "", true}, "swordsman", 11},
    {{"guardBreak", "Quebra-Guarda", 4, "", true}, "swordsman", 13},
    {{"warCry", "Grito de Guerra", 5, "", false}, "swordsman", 15},
    {{"leapingStrike", "Salto Devastador", 5, "", true}, "swordsman", 17},
    // Arqueiro (docs/SKILLS_ARCHER.md, 2026-10-18): bow skills (the bow's own roll + bonus dice), the second of T1 / T3, two of T4 / T5
    {{"venomArrow", "Flecha Envenenada", 1, "", true}, "archer", 2},
    {{"arrowRain", "Chuva de Flechas", 3, "", true}, "archer", 7},
    {{"explosiveArrow", "Flecha Explosiva", 4, "", true}, "archer", 10},
    {{"cripplingShot", "Tiro Paralisante", 4, "", true}, "archer", 12},
    {{"arrowStorm", "Tempestade de Flechas", 5, "", true}, "archer", 13},
    {{"deathShot", "Tiro Mortal", 5, "", true}, "archer", 15},
    // Lanceiro (docs/SKILLS_LANCER.md, 2026-10-18; Aldric, class `aldric`): lance skills that move the board (the lance's own
    // roll + bonus dice), two of T4 / T5, one of T1-T3; the Sentinela and the Templário keep them
    {{"spearToss", "Arremesso", 1, "", true}, "aldric", 2},
    {{"hookPull", "Gancho", 2, "", true}, "aldric", 4},
    {{"poleVault", "Salto de Vara", 3, "", true}, "aldric", 8},
    {{"disarm", "Desarmar", 4, "", true}, "aldric", 11},
    {{"swapPost", "Troca de Posto", 4, "", true}, "aldric", 13},
    {{"impaleDrag", "Empalar e Arrastar", 5, "", true}, "aldric", 15},
    {{"sentinelVigil", "Vigília da Sentinela", 5, "", false}, "aldric", 17},
    // ... and the Vigília's own thrust (never learned: the stance plays it when an enemy attacks)
    {{"sentinelStrike", "Estocada da Sentinela", 5, "", true}, "", 99},
    // Curandeiro (docs/SKILLS_HEALER.md, 2026-10-20; Salazar, class `salazar`): the second spell of T4 and both of T5, holy; the
    // Clérigo and the Bispo keep them. The field spells (Consagrar Acampamento, Prece do Caminho) live in world/FieldSpells.
    {{"circleOfHealing", "Círculo de Cura", 4, "holy", false}, "healer", 9},
    {{"resurrection", "Ressurreição", 5, "holy", false}, "healer", 12},
    {{"judgment", "Julgamento", 5, "holy", false}, "healer", 14},
    // Conjurador (docs/SKILLS_CONJURER.md, 2026-10-20; Malrec, class `conjurer`): a new spell every two levels from 3 to 15, combat and
    // utility by turns; the Arcanista and the Necromante keep them. The field spells (Marcar Runa, Retornar) live in world/FieldSpells.
    {{"placeSwap", "Troca de Lugar", 1, "", false}, "conjurer", 3},
    {{"boneLance", "Lança de Osso", 2, "arcane", false}, "conjurer", 5},
    {{"mirrorImage", "Imagem Espelhada", 2, "", false}, "conjurer", 7},
    {{"detonateSummon", "Detonar Invocação", 3, "arcane", false}, "conjurer", 9},
    {{"sacrificeBond", "Elo de Sacrifício", 3, "", false}, "conjurer", 11},
    {{"nightmare", "Pesadelo", 4, "arcane", false}, "conjurer", 13},
    {{"empowerSummons", "Fortalecer Invocações", 4, "", false}, "conjurer", 15},
};
// The base class a promotion keeps the skills of (the Guerreiro's go on into the Paladino and the Cavaleiro Pesado, the
// Arqueiro's into the Patrulheiro and the Assassino, the Lanceiro's into the Sentinela and the Templário).
const char* emberClassFamily(const std::string& classId) {
  if (classId == "paladin" || classId == "heavyKnight") return "swordsman";
  // Neera's own class id in most battle files is "neera" (an archer with her sprite; "archer" in a few older exports)
  if (classId == "ranger" || classId == "assassin" || classId == "neera") return "archer";
  if (classId == "sentinel" || classId == "templar" || classId == "lancer") return "aldric";
  // Salazar's own class id is "salazar" (a healer with his sprite); the Clérigo and the Bispo are the healer's promotions
  if (classId == "salazar" || classId == "cleric" || classId == "bishop") return "healer";
  if (classId == "sorcerer" || classId == "necromancer") return "conjurer";
  return classId.c_str();
}
// The Conjurador's spells (docs/SKILLS_CONJURER.md).
bool conjurerKind(const std::string& kind) {
  return kind == "placeSwap" || kind == "boneLance" || kind == "mirrorImage" || kind == "detonateSummon" || kind == "sacrificeBond" ||
         kind == "nightmare" || kind == "empowerSummons";
}
// The living summons of a unit (its familiars, its Cão Zumbi), in the battle's order.
std::vector<size_t> summonsOf(const Battle& b, size_t owner) {
  std::vector<size_t> out;
  const std::string& id = b.units()[owner].id;
  for (size_t i = 0; i < b.units().size(); ++i)
    if (b.units()[i].alive && b.units()[i].summoned && b.units()[i].summonerId == id) out.push_back(i);
  return out;
}
// The Curandeiro's spells (docs/SKILLS_HEALER.md): a group heal over an area, a fallen ally raised, holy judgment over an area.
bool healerKind(const std::string& kind) { return kind == "circleOfHealing" || kind == "resurrection" || kind == "judgment"; }
// The Lanceiro's skills (docs/SKILLS_LANCER.md).
bool lancerKind(const std::string& kind) {
  return kind == "spearToss" || kind == "hookPull" || kind == "poleVault" || kind == "disarm" || kind == "swapPost" || kind == "impaleDrag" ||
         kind == "sentinelVigil" || kind == "sentinelStrike";
}
// The Arqueiro's shots aimed at one foe within the bow's reach (+ the skill's extra hexes), with a clear line of fire.
bool archerShot(const std::string& kind) {
  return kind == "venomArrow" || kind == "explosiveArrow" || kind == "cripplingShot" || kind == "deathShot";
}
// ... and its volleys over an area (no line of fire: the arrows come down from above).
bool archerVolley(const std::string& kind) { return kind == "arrowRain" || kind == "arrowStorm"; }
const EmberSpell* emberSpell(const std::string& kind) {
  for (const EmberSpell& s : kEmberSpells)
    if (kind == s.data.kind) return &s;
  return nullptr;
}
const SpellData* spellData(const std::string& kind) {
  for (const SpellData& s : kSpells)
    if (kind == s.kind) return &s;
  if (const EmberSpell* e = emberSpell(kind)) return &e->data;
  return nullptr;
}
int spellTierOf(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s ? s->tier : 0;
}
// Ours (the user, 2026-10-18: "faça a investida de touro ficar como T3 e dê um boost no dano"): a hero's Investida Touro is a
// tier-3 skill (Engine2: tier 1); an enemy's (the Ox's three charges) keeps Engine2's tier 1. Off in the Cooker's checks.
bool bullRushTier3() { return progressionEnabled(); }
int unitTierOf(const BattleUnit& u, const std::string& kind) {
  if (kind == "bullRush" && u.side == "player" && bullRushTier3()) return 3;
  if (kind == "summonFamiliar3" && progressionEnabled()) return 5;  // Familiar Titã (the user, 2026-10-20)
  if (kind == "summonZombieDog" && progressionEnabled()) return 6;  // the Necromante's Cão Zumbi
  return spellTierOf(kind);
}
std::string elementOf(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s ? s->element : "";
}
bool weaponAbility(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s && s->weaponAbility;
}

constexpr int kLongShotRange = 7;  // LONG_SHOT.range
struct Dice {
  int dice = 0, faces = 0;
};
Dice longShotPower(int level);
Dice longShotPowerE2(int level) {
  if (level >= 14) return {2, 12};
  if (level >= 12) return {2, 10};
  if (level >= 9) return {2, 8};
  if (level >= 7) return {2, 6};
  if (level >= 5) return {1, 12};
  if (level >= 3) return {1, 10};
  if (level >= 2) return {2, 4};
  return {1, 8};
}
constexpr int kMissileRange = 5, kMissileDice = 1, kMissileFaces = 4, kMissileBonus = 0;  // MAGIC_MISSILE
constexpr double kMissileMul = 1.15;
// Based on AD&D 2e's Magic Missile, not a copy: one missile, then more on its track (kSpellTracks), five at the end.
int magicMissileCount(int level);
double missileMul(int level);
double piercingMul(int level) { return level >= 13 ? 2.5 : level >= 10 ? 2.25 : level >= 6 ? 2 : 1.5; }
constexpr double kThrustArmorIgnore = 0.2;  // PIERCING_THRUST.armorIgnore
Dice doubleStrikePower(int level) {
  if (level >= 14) return {2, 8};
  if (level >= 13) return {2, 6};
  if (level >= 11) return {1, 12};
  if (level >= 9) return {1, 10};
  if (level >= 7) return {2, 4};
  if (level >= 5) return {1, 8};
  if (level >= 3) return {1, 6};
  if (level >= 2) return {1, 4};
  return {0, 0};
}
// LIGHTNING / SHOCK
constexpr int kLightningRange = 5, kLightningDice = 2, kLightningFaces = 8, kLightningBonus = 0;
// AD&D 2e Lightning Bolt gains dice with the caster's level: here on its track (kSpellTracks).
int lightningDice(int level);
double lightningMul(int level);
constexpr double kLightningMul = 2.0;
constexpr BattleUnit::Echo kLightningEcho{1, 12, 2, 0};
constexpr int kShockRange = 5, kShockDice = 1, kShockFaces = 6, kShockBonus = 0;
constexpr double kShockMul = 0.67;
constexpr BattleUnit::Echo kShockEcho{1, 4, 0, 0};
// FROST (frost.ts)
struct FrostPower {
  int length, dice, faces;
  double mul;
};
FrostPower frostPower(int level);
// CURES: rollCure = floor(floor(MAG / 2) * mul + dice)
struct CureData {
  const char* kind;
  int dice, faces, bonus;
  double mul;
  int range;
};
constexpr CureData kCures[] = {
    {"cureMinor", 1, 6, 0, 1.0, 2},
    {"cureWounds", 2, 6, 0, 1.6, 3},
    {"cureLight", 1, 8, 0, 1.2, 1},
};
const CureData* cureData(const std::string& kind) {
  for (const CureData& c : kCures)
    if (kind == c.kind) return &c;
  return nullptr;
}
constexpr int kCureDiseaseRange = 2;  // CURE_DISEASE.range
// DISEASE
constexpr double kDiseaseBiteChance = 0.2, kDiseaseZombieChance = 0.3, kDiseasePenalty = 0.1;
// CREATE_FOOD_AND_WATER, createFoodAndWaterPower (hunger.ts INN_FULLNESS 120 from level 10)
constexpr int kFoodRadius = 2;
constexpr double kInnFullness = 120;
struct FoodPower {
  int dice, faces, bonus;
  double fullness;
};
FoodPower foodPower(int level) {
  const double full = level >= 10 ? kInnFullness : 100;
  if (level >= 15) return {2, 4, 2, full};
  if (level >= 13) return {1, 6, 2, full};
  if (level >= 11) return {1, 6, 1, full};
  if (level >= 9) return {1, 4, 1, full};
  if (level >= 7) return {1, 4, 0, full};
  if (level >= 5) return {1, 3, 0, full};
  if (level >= 3) return {1, 2, 0, full};
  return {0, 0, 0, full};
}
double fullnessOf(const BattleUnit& u) { return u.fullness ? std::max(0.0, std::min(120.0, *u.fullness)) : 100; }  // hunger.ts fullness
// POTIONS
struct PotionData {
  const char* id;
  const char* name;
  int dice, faces, bonus;
  char effect;  // 'h' heal, 'd' disease, 'm' mana
  int manaRestore;
};
constexpr PotionData kPotions[] = {
    {"weak", "Poção Fraca", 1, 8, 2, 'h', 0},
    {"mid", "Poção Média", 2, 8, 4, 'h', 0},
    {"potent", "Poção De Cura Potente", 2, 12, 6, 'h', 0},
    {"disease", "Poção De Curar Doenças", 0, 0, 0, 'd', 0},
    {"manaSmall", "Poção De Mana Pequena", 0, 0, 0, 'm', 1},
    {"manaMid", "Poção De Mana Média", 0, 0, 0, 'm', 2},
    {"manaLarge", "Poção De Mana Grande", 0, 0, 0, 'm', 3},
};
const PotionData* potionData(const std::string& id) {
  for (const PotionData& p : kPotions)
    if (id == p.id) return &p;
  return nullptr;
}
// Engine2 poisonTiers (overworld.json tables.poisonTiers): the tick's dice and the resistance penalty, by tier.
struct PoisonTier {
  const char* id;
  const char* name;
  int dice, faces, penalty;
  int rank;
};
constexpr PoisonTier kPoisonTiers[] = {
    {"lesser", "Veneno Menor", 1, 4, 0, 1}, {"poison", "Veneno", 1, 10, 10, 2},     {"greater", "Veneno Maior", 1, 10, 20, 3},
    {"deadly", "Veneno Mortal", 2, 8, 40, 4}, {"lethal", "Veneno Letal", 3, 6, 50, 5},
};
const PoisonTier* poisonTier(const std::string& id) {
  for (const PoisonTier& t : kPoisonTiers)
    if (id == t.id) return &t;
  return nullptr;
}

// --- Each spell's own scaling (the user, 2026-10-20: "refazer todas as magias uma por uma com sentido real baseado no Advanced
// Dungeons & Dragons second edition", based on, never copied; "cada nível é uma pequena melhoria, apenas uma pequena"; "são 15 níveis
// de escalonamento"). A spell is learned on its tier's first level for the class; each of the 14 levels after it brings one small
// improvement (a hex of reach, a ring of area, a die, a missile, a little more weight on MAG ...), never two on one level. Ember
// Ashes I stops at level 30 (kMaxLevel); a spell learned after 16 keeps scaling past it in Ember Ashes II (cap 60): nothing here
// stops at 30. The reach never passes 12 hexes (the party's sight); an area starts as 3 hexes (a size-3 body) and grows a ring at a
// time (the Veneno Cáustico the most, four; the Bola de Fogo three). Off in the Cooker's checks. Docs: docs/SPELLS_SCALING.md. ---
constexpr int kReachCap = 12;
struct SpellTrack {
  const char* kind;
  int learn;          // the level it is learned (its tier's first use for the class)
  const char* steps;  // 14 tokens, one per level after `learn`: a letter and the step ("r1" = +1 hex of reach), "-" = none
};
// TRACKS-BEGIN (written by tools/scripts/spells/tracks.py; the level each token lands on = learn + its place, 1 to 14)
constexpr SpellTrack kSpellTracks[] = {
    {"magicMissile", 1, "r1 c1 m0.05 r1 r1 c1 r1 m0.05 c1 r1 r1 m0.05 c1 r1"},
    {"lightning", 3, "r1 m0.1 d1 r1 r1 m0.1 r1 d1 m0.1 r1 r1 d1 m0.1 r1"},
    {"frost", 5, "l1 m0.05 d1 l1 m0.05 l1 d1 m0.05 l1 m0.05 l1 d1 m0.05 l1"},
    {"poisonBreath", 3, "m0.04 l1 m0.04 d1 m0.04 f2 m0.04 m0.04 l1 m0.04 d1 m0.04 f2 m0.04"},
    {"fireball", 6, "r1 d1 a1 r1 r1 d1 r1 a1 d1 r1 r1 a1 d1 r1"},
    {"causticVenom", 11, "r1 a1 m0.1 d1 r1 a1 r1 m0.1 a1 r1 d1 m0.1 a1 r1"},
    {"lightningTier3", 15, "m0.1 r1 d1 m0.1 r1 m0.1 r1 d1 m0.1 r1 m0.1 d1 r1 m0.1"},
    {"cureMinor", 1, "m0.05 r1 d1 m0.05 m0.05 r1 d1 m0.05 r1 m0.05 m0.05 d1 r1 m0.05"},
    {"bless", 3, "p1 t1 p1 a1 t1 p1 p1 t1 p1 t1 a1 p1 t1 p1"},
    {"cureWounds", 3, "m0.05 d1 m0.05 m0.05 r1 m0.05 d1 m0.05 m0.05 m0.05 r1 m0.05 d1 m0.05"},
    {"burningHands", 5, "m0.05 d1 m0.05 l1 f2 m0.05 d1 m0.05 m0.05 l1 f2 m0.05 d1 m0.05"},
    {"turnUndead", 6, "m0.1 d1 m0.1 a1 d1 m0.1 d1 m0.1 m0.1 d1 a1 m0.1 d1 m0.1"},
    {"cureDisease", 6, "- - - r1 - - - - - - r1 - - -"},
    {"divineBolt", 8, "r1 d1 m0.1 r1 a1 r1 d1 m0.1 r1 a1 r1 d1 m0.1 r1"},
    {"circleOfHealing", 9, "r1 d1 m0.1 r1 a1 r1 d1 m0.1 r1 a1 r1 d1 m0.1 r1"},
    {"resurrection", 12, "m0.1 r1 d1 m0.1 m0.1 f2 r1 m0.1 d1 m0.1 m0.1 r1 d1 m0.1"},
    {"judgment", 14, "r1 m0.15 d1 a1 r1 m0.15 d1 T0.25 r1 m0.15 a1 d1 r1 m0.15"},
    {"cureLight", 11, "m0.05 d1 m0.05 m0.05 r1 m0.05 d1 m0.05 m0.05 m0.05 r1 m0.05 d1 m0.05"},
    {"auraOfProtection", 15, "p2 t1 p2 a1 p2 t1 p2 p2 t1 p2 a1 p2 t1 p2"},
    {"intimidatingPresence", 15, "p2 t1 p2 p2 a1 p2 p2 t1 p2 p2 a1 p2 t1 p2"},
    {"divineWrath", 18, "r1 d1 r1 d1 r1 f2 r1 d1 r1 r1 d1 r1 d1 r1"},
    {"summonFamiliar", 1, "- r1 - r1 - r1 - - r1 - r1 - r1 -"},
    {"phantasmalForce", 2, "r1 f2 d1 r1 m0.1 f2 r1 r1 f2 d1 r1 m0.1 f2 r1"},
    {"webOfDreams", 3, "x5 r1 x5 a1 r1 x5 r1 x5 x5 r1 a1 x5 r1 x5"},
    {"summonFamiliar2", 3, "- r1 - r1 - r1 - - r1 - r1 - r1 -"},
    {"placeSwap", 3, "- r1 - - - r1 - - r1 - - - r1 -"},
    {"boneLance", 5, "m0.1 l1 d1 f2 m0.1 l1 d1 m0.1 l1 m0.1 f2 d1 l1 m0.1"},
    {"summonFamiliar4", 6, "- r1 - r1 - r1 - - r1 - r1 - r1 -"},
    {"mirrorImage", 7, "p3 r1 R1 c1 p3 r1 R1 p3 r1 p3 c1 R1 r1 p3"},
    {"detonateSummon", 9, "m0.12 r1 d1 a1 m0.12 r1 d1 m0.12 r1 m0.12 a1 d1 r1 m0.12"},
    {"summonFamiliar3", 11, "- r1 - r1 - r1 - - r1 - r1 - r1 -"},
    {"sacrificeBond", 11, "p-3 p-3 p-3 p-3 p-3 p-3 p-3 p-3 p-3 p-3 p-3 p-3 p-3 p-3"},
    {"nightmare", 13, "r1 a1 d1 r1 m0.15 r1 p10 t1 r1 a1 r1 d1 m0.15 r1"},
    {"summonZombieDog", 15, "- r1 - r1 - r1 - - r1 - r1 - r1 -"},
    {"empowerSummons", 15, "p1 p1 p1 R1 p1 p1 p1 p1 p1 p1 R1 p1 p1 p1"},
    {"markOfRuin", 15, "r1 p5 m0.05 r1 t1 d1 r1 p5 m0.05 f2 r1 p5 m0.05 r1"},
    {"creepingPlague", 15, "r1 m0.05 d1 r1 m0.05 v1 r1 s1 f2 m0.05 r1 d1 m0.05 r1"},
    {"lifeDrain", 16, "m0.05 r1 m0.05 f2 p25 m0.05 m0.05 d1 m0.05 r1 f2 m0.05 p25 m0.05"},
    {"shroudOfShadows", 17, "r1 p5 a1 r1 R1 r1 p5 k1 r1 a1 r1 R1 p5 r1"},
    {"hemorrhage", 18, "m0.05 r1 m0.05 k1 r1 m0.05 K2 u1 m0.05 r1 d1 m0.05 r1 m0.05"},
    {"profaneSilence", 19, "r1 m0.05 a1 t1 r1 m0.05 d1 f2 r1 m0.05 a1 t1 r1 m0.05"},
    {"blackPestilence", 21, "m0.05 r1 a1 d1 m0.05 f2 m0.05 v1 r1 m0.05 a1 d1 f2 m0.05"},
    {"curseOfDecay", 22, "m0.05 r1 p10 T0.1 m0.05 t1 d1 k1 K2 m0.05 r1 p10 T0.1 m0.05"},
    {"soulHarvest", 23, "r1 a1 m0.1 r1 p2 r1 d1 f2 r1 a1 r1 m0.1 p2 r1"},
    {"devouringVoid", 25, "r1 m0.05 p5 s5 r1 m0.05 d1 f2 r1 m0.05 p5 s5 r1 m0.05"},
    {"pillarOfFlame", 15, "r1 m0.1 a1 r1 d1 R1 r1 m0.1 h1 f2 r1 a1 m0.1 r1"},
    {"iceLance", 15, "r1 m0.05 p25 r1 m0.05 d1 r1 m0.05 f2 r1 m0.05 p25 r1 m0.05"},
    {"thunderstorm", 16, "r1 m0.1 a1 r1 d1 r1 m0.1 k1 r1 b2 r1 a1 m0.1 r1"},
    {"meteor", 17, "r1 m0.1 a1 M0.2 r1 d1 m0.1 g1 f2 r1 a1 M0.2 m0.1 r1"},
    {"absoluteZero", 18, "m0.05 r1 a1 m0.05 r1 m0.05 d1 f2 m0.05 r1 m0.05 a1 r1 m0.05"},
    {"chainLightning", 19, "r1 m0.05 j1 e-0.025 r1 d1 r1 m0.05 l1 r1 j1 e-0.025 m0.05 r1"},
    {"eruption", 21, "m0.05 a1 R1 m0.05 d1 m0.05 h1 f2 m0.05 H2 m0.05 a1 R1 m0.05"},
    {"glacialGale", 22, "m0.05 l1 m0.05 f2 m0.05 d1 m0.05 l1 P1 m0.05 f2 m0.05 l1 m0.05"},
    {"wrathOfTheSkies", 23, "r1 m0.1 b1 c1 r1 m0.1 b1 d1 r1 m0.1 b1 r1 m0.1 b1"},
    {"primordialEmber", 25, "r1 m0.1 a1 d1 h1 R1 r1 m0.1 f2 H2 b1 a1 r1 m0.1"},
    {"longShot", 1, "r1 b1 d1 f2 r1 b1 r1 d1 b1 r1 f2 d1 b1 r1"},
    {"venomArrow", 2, "r1 v1 m0.05 d1 f2 r1 v1 m0.05 r1 d1 f2 v1 m0.05 r1"},
    {"bloodyShot", 5, "r1 m0.15 r1 m0.15 k1 r1 m0.15 K2 r1 m0.15 r1 m0.15 r1 m0.15"},
    {"multiShot", 7, "r1 d1 f2 r1 c1 r1 d1 f2 r1 c1 r1 d1 f2 r1"},
    {"arrowRain", 7, "r1 m0.05 a1 r1 m0.05 r1 d1 f2 r1 m0.05 r1 a1 m0.05 r1"},
    {"explosiveArrow", 10, "r1 m0.15 p5 d1 f2 r1 m0.15 p5 r1 d1 f2 m0.15 p5 r1"},
    {"cripplingShot", 12, "p5 r1 m0.1 p5 r1 d1 p5 m0.1 t1 r1 p5 m0.1 r1 p5"},
    {"arrowStorm", 13, "r1 m0.1 a1 f2 r1 m0.1 r1 d1 m0.1 r1 a1 f2 m0.1 r1"},
    {"deathShot", 15, "r1 m0.15 p2 f2 r1 m0.15 p2 d1 r1 m0.15 f2 p2 r1 m0.15"},
    {"minorVenom", 8, "- r1 - - - a1 - - r1 - - - a1 -"},
};
// TRACKS-END
struct ParsedTrack {
  int learn = 0;
  std::vector<std::pair<char, double>> steps;  // in level order: learn + 1, learn + 2 ...
};
const ParsedTrack* parsedTrack(const std::string& kind) {
  static const std::map<std::string, ParsedTrack> all = [] {
    std::map<std::string, ParsedTrack> out;
    for (const SpellTrack& t : kSpellTracks) {
      ParsedTrack& p = out[t.kind];
      p.learn = t.learn;
      for (const char* c = t.steps; *c;) {
        while (*c == ' ') ++c;
        if (!*c) break;
        const char letter = *c++;
        char* end = const_cast<char*>(c);
        const double step = letter == '-' ? 0 : std::strtod(c, &end);
        c = end;
        p.steps.emplace_back(letter, step);
      }
    }
    return out;
  }();
  const auto it = all.find(kind == "magicMissileV2" ? "magicMissile" : kind);
  return it == all.end() ? nullptr : &it->second;
}
// What a spell has gained by this level: the sum of its steps per letter, from the level after it is learned up to `level`.
struct TrackGains {
  std::array<double, 52> v{};
  static int slot(char c) { return c >= 'a' && c <= 'z' ? c - 'a' : c >= 'A' && c <= 'Z' ? 26 + (c - 'A') : -1; }
  double operator()(char c) const { return slot(c) < 0 ? 0 : v[static_cast<size_t>(slot(c))]; }
  int n(char c) const { return static_cast<int>(std::lround((*this)(c))); }
};
TrackGains spellGains(const std::string& kind, int level) {
  TrackGains g;
  const ParsedTrack* t = progressionEnabled() ? parsedTrack(kind) : nullptr;
  if (!t) return g;
  for (size_t i = 0; i < t->steps.size() && t->learn + 1 + static_cast<int>(i) <= level; ++i)
    if (const int at = TrackGains::slot(t->steps[i].first); at >= 0) g.v[static_cast<size_t>(at)] += t->steps[i].second;
  return g;
}
// The spell's track grows this at all (an area spell's rings: its area starts as 3 hexes).
bool trackGrows(const std::string& kind, char letter) {
  if (const ParsedTrack* t = parsedTrack(kind))
    for (const auto& [c, step] : t->steps)
      if (c == letter) return true;
  return false;
}
// The hexes of reach gained since the spell was learned.
int reachGain(const std::string& kind, int level) { return spellGains(kind, level).n('r'); }
// A spell's reach at this level: its own plus what it gained, never past kReachCap (nor below its own).
int grownReach(const std::string& kind, int base, int level) { return std::max(base, std::min(kReachCap, base + reachGain(kind, level))); }
// A bow skill's reach: the bow's own plus the skill's extra hexes, never past kReachCap (nor below the bow's).
int shotReach(int bow, int extra) { return std::max(bow, std::min(kReachCap, bow + extra)); }
// Areas: stage 0 = 3 hexes (the aimed one and two beside it, a size-3 body), stage N = every hex within N rings. An area spell's
// stage = the rings its track gave by this level; any other kind keeps its own radius.
int areaRadius(const std::string& kind, int base, int level) {
  if (!progressionEnabled() || !trackGrows(kind, 'a')) return base;
  return spellGains(kind, level).n('a');
}
// What an area stage reads as ("3 hexes", "raio 2").
std::string areaLabel(int stage) { return stage <= 0 ? std::string("3 hexes") : std::format("raio {}", stage); }
// The poison tier `steps` above `base` (Veneno Menor, Veneno, Veneno Maior, Veneno Mortal, Veneno Letal).
const char* poisonTierUp(const char* base, int steps) {
  static const char* kOrder[] = {"lesser", "poison", "greater", "deadly", "lethal"};
  int at = 0;
  for (int i = 0; i < 5; ++i)
    if (std::string(base) == kOrder[i]) at = i;
  return kOrder[std::clamp(at + steps, 0, 4)];
}
bool emberAreaGrows(const std::string& kind);
// --- The first spells on their tracks (with the progression off, the Cooker's checks: Engine2's own numbers) ---
int magicMissileCount(int level) {
  if (!progressionEnabled()) return level >= 6 ? 3 : level >= 3 ? 2 : 1;
  return 1 + spellGains("magicMissile", level).n('c');
}
double missileMul(int level) { return kMissileMul + spellGains("magicMissile", level)('m'); }
Dice longShotPower(int level) {
  if (!progressionEnabled()) return longShotPowerE2(level);
  const TrackGains g = spellGains("longShot", level);
  return {1 + g.n('d'), 8 + g.n('f')};
}
int lightningDice(int level) { return kLightningDice + spellGains("lightning", level).n('d'); }
double lightningMul(int level) { return kLightningMul + spellGains("lightning", level)('m'); }
FrostPower frostPower(int level) {
  if (!progressionEnabled()) {
    const int lv = std::max(1, std::min(30, level));
    return {2 + std::max(0, lv - 5) / 4, 1 + std::max(0, lv - 5) / 6, 6, 1.1};
  }
  const TrackGains g = spellGains("frost", level);
  return {2 + g.n('l'), 1 + g.n('d'), 6, 1.1 + g('m')};
}

// --- EmberEngine's own spells: their numbers by the caster's level (docs/SPELLS_PROMOTIONS.md) -------------------------
struct EmberNumbers {
  int range = 0, radius = 0;  // reach; the area around the aimed hex (0 = one hex / a line / a cone)
  double mul = 0;             // the weight on floor(MAG / 2)
  int dice = 0, faces = 0, bonus = 0;
  // the spell's own extras
  int pct = 0, turns = 0, rounds = 0, spread = 0, jumps = 0, length = 0, push = 0, count = 0;
  double tickMul = 0;
  int tickDice = 0, tickFaces = 0;
  int hazardDice = 0, hazardFaces = 8;
  double ringMul = 0;
  int ringDice = 0, ringFaces = 0;
  const char* tier = "";
  int durDice = 0, durFaces = 0;
  double decay = 0;
};
EmberNumbers emberNumbers(const std::string& kind, int level) {
  const int lv = std::max(1, level);
  // the spells: their numbers on the level they are learned, plus what their track gave since (spellGains, one step a level)
  const TrackGains g = spellGains(kind, lv);
  const auto mul = [&](double base) { return base + g('m'); };
  const auto dice = [&](int base) { return base + g.n('d'); };
  const auto faces = [&](int base) { return base + g.n('f'); };
  EmberNumbers n;
  // Bruxo
  if (kind == "markOfRuin") {
    n.range = 6, n.mul = mul(1.15), n.dice = dice(1), n.faces = faces(6);
    n.pct = 20 + g.n('p'), n.turns = 3 + g.n('t');
  } else if (kind == "creepingPlague") {
    n.range = 6, n.mul = mul(1.3), n.dice = dice(1), n.faces = faces(8);
    n.tier = poisonTierUp("greater", g.n('v'));
    n.spread = 1 + g.n('s');
  } else if (kind == "lifeDrain") {
    n.range = 5, n.mul = mul(1.6), n.dice = dice(2), n.faces = faces(6);
    n.pct = 50 + g.n('p');
  } else if (kind == "shroudOfShadows") {
    n.range = 6, n.tickMul = 0.7, n.tickDice = 1 + g.n('k'), n.tickFaces = 6;
    n.radius = 1;  // an area (its stage: areaRadius)
    n.rounds = 2 + g.n('R'), n.pct = 20 + g.n('p');
  } else if (kind == "hemorrhage") {
    n.range = 5, n.mul = mul(1.4), n.dice = dice(2), n.faces = faces(6);
    n.tickDice = 2 + g.n('k'), n.tickFaces = 6 + g.n('K');
    n.durDice = 1 + g.n('u'), n.durFaces = 6;
  } else if (kind == "profaneSilence") {
    n.range = 6, n.radius = 1, n.mul = mul(1.1), n.dice = dice(1), n.faces = faces(8);
    n.turns = 2 + g.n('t');
  } else if (kind == "blackPestilence") {
    n.range = 7, n.radius = 1, n.mul = mul(1.2), n.dice = dice(2), n.faces = faces(6);
    n.tier = poisonTierUp("deadly", g.n('v'));
  } else if (kind == "curseOfDecay") {
    n.range = 6, n.mul = mul(1.5), n.dice = dice(2), n.faces = faces(8);
    n.tickMul = 0.5 + g('T'), n.tickDice = 1 + g.n('k'), n.tickFaces = 8 + g.n('K');
    n.pct = 30 + g.n('p');  // the cap
    n.turns = 3 + g.n('t');
    n.spread = 10;  // the step per turn
  } else if (kind == "soulHarvest") {
    n.range = 6, n.radius = 2, n.mul = mul(1.7), n.dice = dice(3), n.faces = faces(6);
    n.pct = 10 + g.n('p');
  } else if (kind == "devouringVoid") {
    n.range = 7, n.mul = mul(2.2), n.dice = dice(3), n.faces = faces(8);
    n.pct = 15 + g.n('p');
    n.spread = 30 + g.n('s');  // Escuridão resistance shred
  }
  // Guerreiro (docs/SKILLS_WARRIOR.md): dice = the bonus on the weapon's own roll
  else if (kind == "steelWhirlwind") {
    n.radius = 1;
    if (lv >= 30) n.dice = 3, n.faces = 8;
    else if (lv >= 26) n.dice = 2, n.faces = 10;
    else if (lv >= 22) n.dice = 2, n.faces = 8;
    else if (lv >= 18) n.dice = 2, n.faces = 6;
    else if (lv >= 14) n.dice = 1, n.faces = 8;
    else n.dice = 1, n.faces = 6;
  } else if (kind == "guardBreak") {
    n.dice = lv >= 19 ? 2 : 1, n.faces = lv >= 25 ? 8 : lv >= 19 ? 6 : 8;
    n.pct = lv >= 28 ? 35 : lv >= 21 ? 30 : lv >= 17 ? 25 : 20;  // DEF off
    n.turns = lv >= 24 ? 3 : 2;
  } else if (kind == "warCry") {
    n.radius = lv >= 19 ? 3 : 2;
    n.pct = lv >= 27 ? 25 : lv >= 21 ? 20 : 15;  // ATK up
    n.turns = lv >= 24 ? 3 : 2;
    n.spread = lv >= 22 ? 1 : 0;  // the foes' turns of fear
  } else if (kind == "leapingStrike") {
    n.range = lv >= 27 ? 5 : lv >= 21 ? 4 : 3;
    n.mul = lv >= 29 ? 2.0 : lv >= 23 ? 1.75 : 1.5;
    n.dice = 2, n.faces = lv >= 25 ? 8 : 6;
    n.pct = lv >= 25 ? 50 : 0;  // the shockwave on the landing's other neighbours: this share of a weapon roll
  }
  // Arqueiro (docs/SKILLS_ARCHER.md): mul = the weight on the bow's roll, dice = the bonus; length = hexes past the bow's reach
  else if (kind == "venomArrow") {
    n.mul = mul(1), n.dice = dice(1), n.faces = faces(4);
    n.tier = poisonTierUp("lesser", g.n('v'));
  } else if (kind == "arrowRain") {
    n.range = 6, n.radius = 1;  // an area (its stage: areaRadius)
    n.mul = mul(0.75), n.dice = dice(1), n.faces = faces(4);
  } else if (kind == "explosiveArrow") {
    n.mul = mul(1.3), n.dice = dice(2), n.faces = faces(6);
    n.pct = 50 + g.n('p');  // the neighbours' share
  } else if (kind == "cripplingShot") {
    n.mul = mul(1), n.dice = dice(1), n.faces = faces(8);
    n.pct = 50 + g.n('p');  // MOV off
    n.turns = 2 + g.n('t');
  } else if (kind == "arrowStorm") {
    n.range = 7, n.radius = 1;
    n.mul = mul(0.9), n.dice = dice(1), n.faces = faces(6);
  } else if (kind == "deathShot") {
    n.length = 2;
    n.mul = mul(2.0), n.dice = dice(2), n.faces = faces(8);
    n.pct = 25 + g.n('p');  // a foe left under this share of its HP dies
  }
  // Lanceiro (docs/SKILLS_LANCER.md): mul = the weight on the lance's roll, dice = the bonus on it
  else if (kind == "spearToss") {
    n.range = lv >= 14 ? 3 : 2;  // hexes thrown
    n.mul = lv >= 20 ? 1.1 : lv >= 8 ? 1.0 : 0.9;
    if (lv >= 24) n.dice = 2, n.faces = 6;
    else if (lv >= 16) n.dice = 1, n.faces = 6;
    else if (lv >= 8) n.dice = 1, n.faces = 4;
    n.tickDice = lv >= 26 ? 2 : lv >= 14 ? 2 : 1, n.tickFaces = lv >= 26 ? 8 : 6;  // the collision, on both bodies
  } else if (kind == "hookPull") {
    n.range = lv >= 26 ? 5 : lv >= 12 ? 4 : 3;
    n.mul = lv >= 24 ? 1.0 : lv >= 15 ? 0.9 : 0.8;
    n.dice = lv >= 28 ? 2 : 1, n.faces = lv >= 18 ? 6 : 4;
    n.turns = lv >= 24 ? 1 : 0;  // Desequilibrado for this many more of its own turns (always until its next one)
  } else if (kind == "poleVault") {
    n.mul = lv >= 26 ? 1.8 : lv >= 20 ? 1.6 : lv >= 14 ? 1.4 : 1.2;
    n.dice = lv >= 20 ? 2 : 1, n.faces = lv >= 29 ? 8 : 6;
  } else if (kind == "disarm") {
    n.dice = lv >= 27 ? 2 : 1, n.faces = lv >= 17 ? 6 : 4;
    n.pct = lv >= 29 ? 60 : lv >= 22 ? 50 : 40;  // ATK off
    n.turns = lv >= 17 ? 3 : 2;
  } else if (kind == "swapPost") {
    n.range = lv >= 28 ? 5 : lv >= 19 ? 4 : 3;
    n.mul = lv >= 30 ? 1.3 : lv >= 25 ? 1.15 : 1.0;
    n.dice = lv >= 21 ? 2 : 1, n.faces = 6;
  } else if (kind == "impaleDrag") {
    n.length = lv >= 24 ? 4 : 3;              // hexes dragged at most
    n.mul = 1.0, n.decay = lv >= 21 ? 0.3 : 0.25;  // + this per hex dragged
    n.dice = lv >= 28 ? 2 : 1, n.faces = 8;
  } else if (kind == "sentinelVigil") {
    n.radius = lv >= 26 ? 2 : 1;
    n.mul = lv >= 27 ? 1.6 : lv >= 23 ? 1.4 : lv >= 20 ? 1.2 : 1.0;
    n.dice = lv >= 23 ? 1 : 0, n.faces = 6;
  }
  // Curandeiro (docs/SKILLS_HEALER.md): the cures' formula, floor(floor(MAG / 2) x mul) + dice (+1 % per healing skill point)
  else if (kind == "circleOfHealing") {
    n.range = 4, n.radius = 1;
    n.mul = mul(1.2), n.dice = dice(1), n.faces = 8;
  } else if (kind == "resurrection") {
    n.range = 1;  // the fallen ally's hex
    n.mul = mul(1.5), n.dice = dice(2), n.faces = faces(8);
  } else if (kind == "judgment") {
    n.range = 6, n.radius = 1;
    n.mul = mul(1.8), n.dice = dice(2), n.faces = 8;
    n.tickMul = 1.5 + g('T');  // the undead's weight
  }
  // Conjurador (docs/SKILLS_CONJURER.md): the game's damage formula on the arcane ones; the rest move the board or last rounds
  else if (kind == "placeSwap") {
    n.range = 6;
  } else if (kind == "boneLance") {
    n.length = 3 + g.n('l');
    n.mul = mul(1.3), n.dice = dice(1), n.faces = faces(8);
  } else if (kind == "mirrorImage") {
    n.range = 6;
    n.count = 2 + g.n('c');  // the copies
    n.rounds = 3 + g.n('R');
    n.pct = 33 + g.n('p');  // the chance a hostile hit strikes a copy
  } else if (kind == "detonateSummon") {
    n.range = 6, n.radius = 1;
    n.mul = mul(1.8), n.dice = dice(2), n.faces = 8;
  } else if (kind == "sacrificeBond") {
    n.pct = 100 + g.n('p');  // the share the summon takes (the rest is gone): it falls on the track
  } else if (kind == "nightmare") {
    n.range = 6, n.radius = 1;
    n.mul = mul(1.5), n.dice = dice(2), n.faces = 8;
    n.pct = 25 + g.n('p');  // the chance of fear
    n.turns = 1 + g.n('t');  // ... for this many turns
    n.tickMul = 2.0;         // a sleeper's weight
  } else if (kind == "empowerSummons") {
    n.pct = 25 + g.n('p');
    n.rounds = 3 + g.n('R');
  }
  // Elementalista
  else if (kind == "pillarOfFlame") {
    n.range = 6, n.radius = 1, n.mul = mul(2.0), n.dice = dice(2), n.faces = faces(8);
    n.hazardDice = 1 + g.n('h'), n.hazardFaces = 8, n.rounds = 1 + g.n('R');
  } else if (kind == "iceLance") {
    n.range = 7, n.mul = mul(2.4), n.dice = dice(2), n.faces = faces(10);
    n.pct = 50 + g.n('p');  // the hexes past the first
  } else if (kind == "thunderstorm") {
    n.range = 6, n.radius = 1, n.mul = mul(1.8), n.dice = dice(2), n.faces = 12;
    n.tickDice = 1 + g.n('k'), n.tickFaces = 12, n.bonus = 4 + g.n('b');  // the echo: tickDice D12 + bonus
  } else if (kind == "meteor") {
    n.range = 8, n.radius = 2, n.mul = mul(3.0), n.dice = dice(4), n.faces = faces(6);
    n.ringMul = 2.0 + g('M'), n.ringDice = 2 + g.n('g'), n.ringFaces = 6;
  } else if (kind == "absoluteZero") {
    n.range = 6, n.radius = 1, n.mul = mul(2.2), n.dice = dice(3), n.faces = faces(8);
  } else if (kind == "chainLightning") {
    n.range = 7, n.mul = mul(2.8), n.dice = dice(3), n.faces = 10;
    n.jumps = 3 + g.n('j');
    n.decay = 0.15 + g('e');
    n.length = 3 + g.n('l');  // a jump's reach
  } else if (kind == "eruption") {
    n.radius = 2 /* round the caster */, n.mul = mul(2.6), n.dice = dice(4), n.faces = faces(8);
    n.hazardDice = 1 + g.n('h'), n.hazardFaces = 8 + g.n('H'), n.rounds = 2 + g.n('R');
  } else if (kind == "glacialGale") {
    n.length = 5 + g.n('l'), n.mul = mul(2.4), n.dice = dice(4), n.faces = faces(6);
    n.push = 1 + g.n('P');
  } else if (kind == "wrathOfTheSkies") {
    n.range = 8, n.count = 4 + g.n('c'), n.mul = mul(2.6), n.dice = dice(3), n.faces = 12, n.bonus = g.n('b');
  } else if (kind == "primordialEmber") {
    n.range = 7, n.radius = 1, n.mul = mul(3.4), n.dice = dice(4), n.faces = faces(10), n.bonus = g.n('b');
    n.hazardDice = 2 + g.n('h'), n.hazardFaces = 8 + g.n('H'), n.rounds = 2 + g.n('R');
  }
  // The reach that grows with the level, gradually (reachGain): the spells cast at a distance (their range) and the Arqueiro's
  // shots (the hexes past the bow's reach). The Guerreiro's leap and the Lanceiro's throws, pulls and swaps keep theirs.
  static const std::set<std::string> kLengthGrows = {"venomArrow", "explosiveArrow", "cripplingShot", "deathShot"};
  if (kLengthGrows.contains(kind)) n.length += reachGain(kind, lv);  // capped with the bow's reach (shotReach)
  else n.range = grownReach(kind, n.range, lv);
  if (emberAreaGrows(kind)) n.radius = areaRadius(kind, n.radius, lv);  // only a spell with an area grows one
  return n;
}
// Both sides stand in its area (else the caster's foes only).
bool emberBothSides(const std::string& kind) {
  return kind == "shroudOfShadows" || kind == "blackPestilence" || kind == "meteor" || kind == "glacialGale" || kind == "primordialEmber" ||
         kind == "detonateSummon";
}
// Aimed at a hex of the board (an area) rather than at a unit.
bool emberAreaSpell(const std::string& kind) {
  return kind == "shroudOfShadows" || kind == "profaneSilence" || kind == "blackPestilence" || kind == "soulHarvest" || kind == "pillarOfFlame" ||
         kind == "thunderstorm" || kind == "meteor" || kind == "absoluteZero" || kind == "primordialEmber" || kind == "circleOfHealing" ||
         kind == "judgment" || kind == "nightmare";
}
// The classes' spells with an area, whose area grows (areaRadius); single-target spells and the weapon skills never gain one.
bool emberAreaGrows(const std::string& kind) { return emberAreaSpell(kind) || kind == "eruption" || kind == "detonateSummon" || archerVolley(kind); }
// Ressurreição: the fallen ally (the party's, not a summon) lying on the hex, or -1. The dead keep the hex they fell on.
int fallenAllyAt(const Battle& b, size_t caster, BattleHex cell) {
  const std::string& side = b.units()[caster].side;
  for (size_t i = 0; i < b.units().size(); ++i) {
    const BattleUnit& t = b.units()[i];
    if (!t.alive && !t.summoned && t.side == side && t.x == cell.x && t.y == cell.y) return static_cast<int>(i);
  }
  return -1;
}
// Aimed at a visible foe.
bool emberBoltSpell(const std::string& kind) {
  return kind == "markOfRuin" || kind == "creepingPlague" || kind == "lifeDrain" || kind == "hemorrhage" || kind == "curseOfDecay" ||
         kind == "devouringVoid" || kind == "chainLightning" || kind == "wrathOfTheSkies";
}

// Experience (data.ts)
constexpr int kMaxLevel = 30;
int expToLevel(int level) {
  const int c = std::max(1, std::min(kMaxLevel - 1, level));
  if (c >= 28) return 600;
  if (c >= 23) return 500;
  if (c >= 18) return 400;
  if (c >= 13) return 300;
  if (c >= 8) return 225;
  if (c >= 3) return 150;
  return 100;
}
int expForHit(int attackerLevel, int defenderLevel) { return std::max(1, 13 + 3 * (defenderLevel - attackerLevel)); }
constexpr double kKillExpBonus = 1.25;
// Enmity (enmity.ts)
constexpr double kHealCe = 0.5, kHealVe = 3;
constexpr double kKillDropChance = 0.01;  // KILL_DROP_CHANCE
constexpr int kFantomForceRange = 6;      // FANTOM_FORCE
constexpr double kFantomForceMul = 0.82;
// Skills (skills.ts)
constexpr double kSkillCap = 100, kSkillGain = 0.1;

int rollDice(int dice, int faces, int bonus, Mulberry32& rng) {
  int total = bonus;
  for (int i = 0; i < dice; ++i) total += 1 + static_cast<int>(std::floor(rng() * faces));
  return total;
}

// --- Board geometry (pathfinding.ts) -------------------------------------------------------------------------------

struct Cube {
  double q = 0, r = 0, s = 0;
};
Cube oddrToCube(int col, int row) {
  const int q = col - (row - (row & 1)) / 2;
  return {static_cast<double>(q), static_cast<double>(row), static_cast<double>(-q - row)};
}
BattleHex cubeToOddr(int q, int r) { return {q + (r - (r & 1)) / 2, r}; }
Cube cubeRound(double q, double r, double s) {
  double rq = jsRound(q), rr = jsRound(r), rs = jsRound(s);
  const double dq = std::abs(rq - q), dr = std::abs(rr - r), ds = std::abs(rs - s);
  if (dq > dr && dq > ds) rq = -rr - rs;
  else if (dr > ds) rr = -rq - rs;
  else rs = -rq - rr;
  return {rq, rr, rs};
}
int hexDist(BattleHex a, BattleHex b) { return Battle::distance(a, b); }

// piercingLine: from `from` through `through` and on to the board's edge, any angle.
std::vector<BattleHex> piercingLine(const Battle& b, BattleHex from, BattleHex through) {
  const int n = hexDist(from, through);
  if (n <= 0) return {};
  const Cube A = oddrToCube(from.x, from.y), B = oddrToCube(through.x, through.y);
  const double stepQ = (B.q - A.q) / n, stepR = (B.r - A.r) / n, stepS = (B.s - A.s) / n;
  std::vector<BattleHex> out;
  bool havePrev = false;
  BattleHex prev{};
  const int maxSteps = b.cols() + b.rows() + 4;
  for (int i = 1; i <= maxSteps; ++i) {
    const Cube c = cubeRound(A.q + stepQ * i, A.r + stepR * i, A.s + stepS * i);
    const BattleHex p = cubeToOddr(static_cast<int>(c.q), static_cast<int>(c.r));
    if (havePrev && p == prev) continue;
    havePrev = true, prev = p;
    if (!b.inBounds(p.x, p.y)) break;
    out.push_back(p);
  }
  return out;
}

bool occupies(const BattleUnit& u, int x, int y) {
  for (const BattleHex& p : Battle::footprint(u, u.x, u.y))
    if (p.x == x && p.y == y) return true;
  return false;
}
// occupancy(units).get(key): the last living unit whose body covers the hex.
int occAt(const Battle& b, BattleHex c) {
  int who = -1;
  for (size_t i = 0; i < b.units().size(); ++i)
    if (b.units()[i].alive && occupies(b.units()[i], c.x, c.y)) who = static_cast<int>(i);
  return who;
}
// units.find(alive && occupies): the first one.
int firstAt(const Battle& b, BattleHex c) {
  for (size_t i = 0; i < b.units().size(); ++i)
    if (b.units()[i].alive && occupies(b.units()[i], c.x, c.y)) return static_cast<int>(i);
  return -1;
}
// targetable: the party may aim at it (alive, an enemy or neutral, not under the fog; debugFreeCast: any side).
bool targetable(const Battle& b, int u) {
  if (u < 0) return false;
  const BattleUnit& v = b.units()[static_cast<size_t>(u)];
  return v.alive && (b.debugFreeCast() || v.side == "enemy" || v.side == "neutral") && !v.hidden;
}

// Detonar Invocação: the caster's own living summon standing on the hex, or -1.
int ownSummonAt(const Battle& b, size_t caster, BattleHex cell) {
  for (size_t i : summonsOf(b, caster))
    if (occupies(b.units()[i], cell.x, cell.y)) return static_cast<int>(i);
  return -1;
}
// Lança de Osso: the line from the caster through the aimed hex, `length` hexes long (it stops at the board's edge only).
std::vector<BattleHex> boneLine(const Battle& b, BattleHex from, BattleHex through, int length) {
  std::vector<BattleHex> line = piercingLine(b, from, through);
  if (line.size() > static_cast<size_t>(length)) line.resize(static_cast<size_t>(length));
  return line;
}
// piercingRay: stops at a barricade, a shot-blocking hex, or high ground the shooter is not on.
std::vector<BattleHex> piercingRay(const Battle& b, BattleHex from, BattleHex through) {
  const std::vector<BattleHex> raw = piercingLine(b, from, through);
  const bool fromHigh = b.cell(from.x, from.y).shot.height != 0;
  std::vector<BattleHex> out;
  for (const BattleHex& p : raw) {
    const BattleCell& t = b.cell(p.x, p.y);
    if (t.terrain == "barricade" || t.shot.blocksShot) break;
    if (t.shot.height && !fromHigh) break;
    out.push_back(p);
  }
  return out;
}
std::vector<BattleHex> piercingThrustRay(const Battle& b, const BattleUnit& caster, BattleHex through) {
  std::vector<BattleHex> raw = piercingRay(b, {caster.x, caster.y}, through);
  if (raw.size() > static_cast<size_t>(caster.maxRange + 1)) raw.resize(static_cast<size_t>(caster.maxRange + 1));
  return raw;
}
// frostAreaTiles (game/frost.ts): one hex wide from next to the caster straight toward the aimed hex at any angle (a
// hair off-centre, so a line between two hexes always picks the same side), as long as its level gives.
std::vector<BattleHex> frostArea(const Battle& b, BattleHex origin, BattleHex through, int level) {
  const int n = hexDist(origin, through);
  if (!n) return {};
  const Cube A = oddrToCube(origin.x, origin.y), B = oddrToCube(through.x, through.y);
  const double aq = A.q + 1e-6, ar = A.r + 2e-6, as = A.s - 3e-6;
  std::vector<BattleHex> out;
  // The line grows with the level like every other spell's reach (reachGrowth; the user, 2026-10-19: "frost really need to scale
  // the distance like the others").
  // The line is Frost's area: it keeps its own growth (+1 hex every 4 levels) and one more at 10 and at 15.
  const int length = frostPower(level).length;
  for (int i = 1; i <= length; ++i) {
    const double t = static_cast<double>(i) / n;
    const Cube c = cubeRound(aq + (B.q - A.q) * t, ar + (B.r - A.r) * t, as + (B.s - A.s) * t);
    const BattleHex p = cubeToOddr(static_cast<int>(c.q), static_cast<int>(c.r));
    if (p.x < 0 || p.y < 0 || p.x >= b.cols() || p.y >= b.rows()) break;
    out.push_back(p);
  }
  return out;
}
// frostTiles: that line, cut at void or where a bolt from the caster is blocked.
std::vector<BattleHex> frostTiles(const Battle& b, BattleHex origin, BattleHex through, int level) {
  const std::vector<BattleHex> cells = frostArea(b, origin, through, level);
  std::vector<BattleHex> out;
  for (const BattleHex& c : cells) {
    if (b.cell(c.x, c.y).terrain == "void" || !clearShot(b, origin, c, false)) break;
    out.push_back(c);
  }
  return out;
}

// --- Charges ----------------------------------------------------------------------------------------------------------

int tierRemaining(const BattleUnit& u, const std::string& kind) {
  if (kind == "frost" && u.side == "enemy") return u.frostCharges;
  if (kind == "poisonBreath" && u.level < 3) return 0;  // POISON_BREATH.unlockLevel
  if (kind == "burningHands" && u.level < 5) return 0;
  const int tier = unitTierOf(u, kind);
  return tier ? u.spells[tier - 1] : 0;
}
void spendTier(BattleUnit& u, const std::string& kind) {
  if (kind == "frost" && u.side == "enemy") {
    u.frostCharges = std::max(0, u.frostCharges - 1);
    return;
  }
  const int tier = unitTierOf(u, kind);
  if (tier) u.spells[tier - 1] -= 1;
}

// --- Facing (faceSpriteToward / BattleEngine.face) --------------------------------------------------------------------

void faceToward(BattleUnit& u, BattleHex to) {
  if (u.x == to.x && u.y == to.y) return;
  const auto [dx, dy] = heading({u.x, u.y}, to);
  if (dx == 0 && dy == 0) return;
  u.faceDx = dx, u.faceDy = dy;
  if (dx > 1e-6) u.facing = 1;
  else if (dx < -1e-6) u.facing = -1;
}

// --- Training, experience, death -----------------------------------------------------------------------------------

double* skillSlot(BattleUnit& u, const std::string& id) {
  for (auto& [k, v] : u.skills)
    if (k == id) return &v;
  u.skills.emplace_back(id, 0.0);
  return &u.skills.back().second;
}
// The combat log's notes made while a step resolves (skill points gained, levels reached); the wrappers around
// resolveStrike / resolveStep / startOfTurnEffects hand them to the battle's log (Engine2 logSkillGain, the level-up tip).
std::vector<std::string> gNotes;
bool gLogEnglish = false;  // the combat log's language, set by the window (setCombatLogEnglish)
const char* pick(const char* pt, const char* en) { return gLogEnglish ? en : pt; }
// toLocaleString("pt-BR", { maximumFractionDigits: 2 }): a comma, no trailing zeros.
std::string numberPt(double v, int digits = 2) {
  std::string t = std::format("{:.{}f}", v, digits);
  if (t.find('.') != std::string::npos) {
    while (t.back() == '0') t.pop_back();
    if (t.back() == '.') t.pop_back();
  }
  if (const size_t dot = t.find('.'); dot != std::string::npos && !gLogEnglish) t[dot] = ',';
  return t;
}
std::string weaponTypeLabelPt(const std::string& type) {
  static const std::pair<const char*, const char*> kLabels[] = {{"sword", "Espadas"},   {"axe", "Machados"},  {"mace", "Maças"},
                                                                {"hammer", "Martelos"}, {"staff", "Cajados"},  {"spear", "Lanças e Armas de Haste"},
                                                                {"bow", "Arcos"},       {"crossbow", "Bestas"}, {"dagger", "Adagas"}};
  static const std::pair<const char*, const char*> kEnglish[] = {{"sword", "Swords"},   {"axe", "Axes"},  {"mace", "Maces"},
                                                                 {"hammer", "Hammers"}, {"staff", "Staves"},  {"spear", "Spears & Polearms"},
                                                                 {"bow", "Bows"},       {"crossbow", "Crossbows"}, {"dagger", "Daggers"}};
  for (const auto& [id, label] : gLogEnglish ? kEnglish : kLabels)
    if (type == id) return label;
  return type;
}
std::string elementLabelPt(const std::string& element) {
  static const std::pair<const char*, const char*> kLabels[] = {{"fire", "Fogo"},   {"lightning", "Raio"},   {"ice", "Gelo"},       {"arcane", "Arcana"},
                                                                {"darkness", "Trevas"}, {"holy", "Sagrado"}, {"poison", "Veneno"}, {"ember", "Brasa"}};
  static const std::pair<const char*, const char*> kEnglish[] = {{"fire", "Fire"},   {"lightning", "Lightning"},   {"ice", "Ice"},       {"arcane", "Arcane"},
                                                                 {"darkness", "Darkness"}, {"holy", "Holy"}, {"poison", "Poison"}, {"ember", "Ember"}};
  for (const auto& [id, label] : gLogEnglish ? kEnglish : kLabels)
    if (element == id) return label;
  return element;
}
void noteSkillGain(const BattleUnit& u, const std::string& skill, double before, double after) {
  const std::string a = numberPt(after), d = numberPt(after - before);
  gNotes.push_back(gLogEnglish ? std::format("{} improved {}: {} (+{})", u.name, skill, a, d) : std::format("{} melhorou {}: {} (+{})", u.name, skill, a, d));
}
// trainingGainForLevel: none 10 levels above the foe, half 5 above, else the usual.
std::optional<double> trainingGain(int unitLevel, int enemyLevel, bool& skip) {
  const int d = unitLevel - enemyLevel;
  skip = d == 10;
  if (d == 5) return 0.05;
  return std::nullopt;
}
double skillGainChance(double v) { return std::max(0.0, std::min(1.0, (kSkillCap - v) / kSkillCap)); }

// Weapon mastery as weaponModifiers reads it for a hero (75 + skill accuracy, (100 + skill) % damage).
void applyWeaponSkill(BattleUnit& u) {
  if (u.side != "player") return;
  if (u.trainsWeapon) {
    const double v = std::max(0.0, std::min(100.0, u.weaponSkill));
    u.combat.accuracy = 75 + v;
    u.combat.damageMul = (100 + v) / 100;
  }
  if (u.offTrainsWeapon) {
    const double v = std::max(0.0, std::min(100.0, u.offWeaponSkill));
    u.combat.offAccuracy = 75 + v;
    u.combat.offDamageMul = (100 + v) / 100;
  }
}

// trainWeapon: the weapon that struck (the off-hand one on an off-hand counter) may gain a skill point.
void trainWeapon(BattleUnit& u, int enemyLevel, Mulberry32& rng, bool offHand = false) {
  const std::string& type = offHand ? u.offWeaponType : u.weaponType;
  if (type.empty() || u.side != "player" || u.summoned || !(offHand ? u.offTrainsWeapon : u.trainsWeapon)) return;
  bool skip = false;
  const std::optional<double> amount = trainingGain(u.level, enemyLevel, skip);
  if (skip) return;
  const double current = std::max(0.0, std::min(100.0, offHand ? u.offWeaponSkill : u.weaponSkill));  // rollWeaponSkillGain
  if (current >= kSkillCap || rng() >= skillGainChance(current)) return;
  const double gained = std::min(kSkillCap, jsRound((current + amount.value_or(kSkillGain)) * 100) / 100);
  // one skill per weapon type: both hands share it when they hold the same type
  if (type == u.weaponType) u.weaponSkill = gained;
  if (type == u.offWeaponType) u.offWeaponSkill = gained;
  *skillSlot(u, type + "Weapon") = gained;
  noteSkillGain(u, weaponTypeLabelPt(type), current, gained);
  applyWeaponSkill(u);
}

// Esquiva (ours, the user, 2026-10-18): a hero an enemy strikes at may gain a point, by trainWeapon's rule (the foe's level,
// the chance falling as the skill rises). Off in the Cooker's checks (no extra draw from Engine2's random numbers).
void trainDodge(BattleUnit& u, int enemyLevel, Mulberry32& rng) {
  if (u.side != "player" || u.summoned || !u.alive || !progressionEnabled()) return;
  bool skip = false;
  const std::optional<double> amount = trainingGain(u.level, enemyLevel, skip);
  if (skip) return;
  double* slot = skillSlot(u, "dodging");
  const double current = std::max(0.0, std::min(kSkillCap, *slot));
  if (current >= kSkillCap || rng() >= skillGainChance(current)) return;
  *slot = std::min(kSkillCap, jsRound((current + amount.value_or(kSkillGain)) * 100) / 100);
  noteSkillGain(u, pick("Esquiva", "Dodging"), current, *slot);
}

void trainResistance(BattleUnit& u, const std::string& element, std::optional<int> enemyLevel, Mulberry32& rng) {
  if (u.side != "player" || u.summoned) return;
  std::optional<double> amount;
  if (enemyLevel) {
    bool skip = false;
    amount = trainingGain(u.level, *enemyLevel, skip);
    if (skip) return;
  }
  double* slot = skillSlot(u, element + "Resistance");
  const double current = *slot;
  if (rng() >= skillGainChance(current)) return;  // rollSkillGain
  const double gained = std::min(kSkillCap, jsRound((current + amount.value_or(kSkillGain)) * 100) / 100);
  *slot = gained;
  noteSkillGain(u, gLogEnglish ? elementLabelPt(element) + " Resistance" : "Resistência a " + elementLabelPt(element), current, gained);
  bool found = false;
  for (auto& [e, v] : u.combat.resistances)
    if (e == element) v = jsRound((v + gained - current) * 100) / 100, found = true;  // Number(x.toFixed(2))
  if (!found) u.combat.resistances.emplace_back(element, jsRound((gained - current) * 100) / 100);
}

void trainHealing(BattleUnit& u, Mulberry32& rng) {
  if (u.side != "player" || u.summoned) return;
  const double current = u.healingSkill;
  if (rng() >= skillGainChance(current)) return;
  u.healingSkill = std::min(kSkillCap, jsRound((current + kSkillGain) * 100) / 100);
  noteSkillGain(u, pick("Cura", "Healing"), current, u.healingSkill);
}

// healingPower: healingAmount(base, healing skill) = +1% per skill point.
int healingPower(const BattleUnit& u, int base) {
  const double points = std::max(0.0, std::min(kSkillCap, u.healingSkill));
  return static_cast<int>(std::floor(std::max(0, base) * (1 + points / 100) + 1e-9));
}

constexpr double kSupportCe = 20, kSupportVe = 120;  // enmity.ts ENMITY.support

// curePlayerDisease: poison gone, the disease's stats given back.
void curePlayerDisease(BattleUnit& u) {
  u.poisoned = false;
  u.poisonTier.clear();
  u.cond.poisonMag = 0, u.cond.plagueSpread = 0, u.cond.plagueBurst = false;
  if (!u.diseaseBase) {
    u.diseased = false;
    return;
  }
  u.atk = u.diseaseBase->atk, u.mag = u.diseaseBase->mag, u.def = u.diseaseBase->def, u.dex = u.diseaseBase->dex, u.mov = u.diseaseBase->mov;
  u.diseaseBase.reset();
  u.diseased = false;
}

// maybeInflictDisease: a wardog's bite (20%) or a zombie's hit (30%) sickens a surviving target.
void maybeInflictDisease(const BattleUnit& actor, BattleUnit& target, Mulberry32& rng) {
  const std::string& c = actor.classId;
  const double chance = c == "wardog" || c == "wardog2"                                                     ? kDiseaseBiteChance
                        : c == "zombie" || c == "zombie2" || c == "undeadOx" || c == "plagueBearingCattle" ? kDiseaseZombieChance
                                                                                                             : 0;
  if (chance <= 0 || !target.alive || target.diseased) return;
  if (rng() >= chance) return;
  inflictDisease(target);
}

// levelUpUnit: the next level's class stats (plus gear), the level's HP gain, and the tier uses it adds.
void levelUp(BattleUnit& u) {
  const int from = u.level, to = from + 1;
  if (u.growth.size() < static_cast<size_t>(to)) {
    u.level = to;
    return;
  }
  const std::array<int, 16>& before = u.growth[static_cast<size_t>(from - 1)];
  const std::array<int, 16>& after = u.growth[static_cast<size_t>(to - 1)];
  u.level = to;
  u.maxHp = after[0] + u.pointHp + u.gearHp;
  u.atk = after[1] + u.pointAtk + u.gearAtk, u.mag = after[2] + u.pointMag + u.gearMag, u.def = after[3] + u.pointDef + u.gearDef, u.dex = after[4] + u.pointDex + u.gearDex;
  u.hp = std::min(u.maxHp, u.hp + (after[0] - before[0]));
  u.mov = after[15] + u.gearMov;
  for (int t = 0; t < 10; ++t) {
    const int gain = after[5 + t] - before[5 + t];
    if (gain > 0) u.spells[t] = std::min(after[5 + t], u.spells[t] + gain);
  }
  grantClassSpells(u);  // a promotion's spell learned at this level
}

}  // namespace

void raiseToLevel(BattleUnit& u, int level, int xp) {
  while (u.level < std::min(level, kMaxLevel)) levelUp(u);
  u.xp = u.level >= kMaxLevel ? 0 : std::max(0, xp);
}

namespace {

void grantExp(BattleUnit& u, int amount) {
  if (u.level >= kMaxLevel || amount <= 0) return;
  u.xp += amount;
  while (u.xp >= expToLevel(u.level) && u.level < kMaxLevel) {
    u.xp -= expToLevel(u.level);
    levelUp(u);
    gNotes.push_back(gLogEnglish ? std::format("{} reached level {}!", u.name, u.level) : std::format("{} subiu para o nível {}!", u.name, u.level));
  }
  if (u.level >= kMaxLevel) u.xp = 0;
}
void gainExp(BattleUnit& u, int targetLevel, int amount, double mul = 1, bool kill = false) {
  if (amount <= 0 || u.side != "player" || !u.alive) return;
  grantExp(u, static_cast<int>(jsRound(expForHit(u.level, targetLevel) * mul * (kill ? kKillExpBonus : 1))));
}
void gainCounterExp(BattleUnit& u) {
  if (u.side != "player" || !u.alive) return;
  grantExp(u, 1);
}

// markDead: an enemy's kill-drop roll (the loot itself is not kept yet).
void markDead(BattleUnit& u, Mulberry32& rng) {
  u.alive = false;
  if (u.side != "enemy") return;
  if (u.guaranteedDrop) {
    rng();  // weightedLootPick
  } else if (rng() < kKillDropChance) {
    rng();  // weightedWeaponPick
  }
}

// staffElementalDamage / staffLifeSteal
double staffBonus(const CombatProfile& p, const std::string& element) {
  if (!p.staff) return 0;
  for (const auto& [e, v] : p.staffBonuses)
    if (e == element) return v;
  return 0;
}
void staffDrain(BattleUnit& u, int actual) {
  if (!u.combat.staff || !u.alive) return;
  const int heal = std::max(0, std::min(u.maxHp - u.hp, static_cast<int>(std::floor(actual * u.combat.staffLifeStealPct / 100))));
  if (heal > 0) u.hp += heal;
}

// spellDamage: the caster's power weighted by the spell plus its dice, never worse than a plain attack.
int spellDamage(Battle& b, size_t att, size_t foe, double mul, int roll, Mulberry32& rng) {
  const BattleUnit& a = b.units()[att];
  const BattleUnit& f = b.units()[foe];
  const Fighter A = fighterAt(b, att, a.x, a.y);
  const BattleCell &attTile = b.cell(a.x, a.y), &defTile = b.cell(f.x, f.y);
  const double spell = std::floor(powerOf(A) * mul) + roll + attTile.atk - 0 - defTile.cover;
  const double weapon = a.combat.hasWeapon ? rollDice(a.combat.dice, a.combat.faces, a.combat.bonus, rng) + a.combat.enh : 0;  // weaponRoll
  const double plain = powerOf(A) + weapon + attTile.atk - 0 - defTile.def;
  return static_cast<int>(std::max(1.0, std::floor(std::max(spell, plain))));
}

std::vector<size_t> unitsOnLine(const Battle& b, size_t caster, const std::vector<BattleHex>& line) {
  std::vector<size_t> ids;
  for (const BattleHex& t : line) {
    const int who = firstAt(b, t);
    if (who >= 0 && static_cast<size_t>(who) != caster && std::find(ids.begin(), ids.end(), static_cast<size_t>(who)) == ids.end())
      ids.push_back(static_cast<size_t>(who));
  }
  return ids;
}

bool contains(const std::vector<std::string>& list, const std::string& kind) { return std::find(list.begin(), list.end(), kind) != list.end(); }

// --- EmberEngine's own spells: helpers ---------------------------------------------------------------------------------

std::vector<BattleHex> hexesWithin(const Battle& b, BattleHex centre, int radius, bool ring = false) {
  std::vector<BattleHex> out;
  for (int y = 0; y < b.rows(); ++y)
    for (int x = 0; x < b.cols(); ++x) {
      const int d = hexDist(centre, {x, y});
      if (ring ? d == radius : d <= radius) out.push_back({x, y});
    }
  return out;
}
bool inArea(const std::vector<BattleHex>& area, BattleHex h) { return std::find(area.begin(), area.end(), h) != area.end(); }
// An area stage's hexes round a centre (areaRadius): 0 = the centre and the first two hexes beside it on the board (a size-3 body),
// N = every hex within N rings.
std::vector<BattleHex> areaTiles(const Battle& b, BattleHex centre, int stage) {
  if (stage >= 1) return hexesWithin(b, centre, stage);
  std::vector<BattleHex> out{centre};
  for (const BattleHex& n : Battle::neighbors(centre.x, centre.y)) {
    if (out.size() >= 3) break;
    if (b.inBounds(n.x, n.y)) out.push_back(n);
  }
  return out;
}
// Everyone standing on the hexes but the caster, in the battle's order (queueFrost's way); the caster's foes only unless
// bothSides.
std::vector<size_t> unitsOn(const Battle& b, size_t caster, const std::vector<BattleHex>& hexes, bool bothSides) {
  std::vector<size_t> ids;
  const std::string& side = b.units()[caster].side;
  for (size_t i = 0; i < b.units().size(); ++i) {
    const BattleUnit& t = b.units()[i];
    if (!t.alive || i == caster) continue;
    if (!bothSides && t.side == side) continue;
    if (std::any_of(hexes.begin(), hexes.end(), [&](const BattleHex& c) { return occupies(t, c.x, c.y); })) ids.push_back(i);
  }
  return ids;
}
// A hex's centre on the board plane (odd-r: odd rows half a hex to the right).
std::pair<double, double> hexCentre(BattleHex h) { return {h.x + 0.5 * (h.y & 1), h.y * (kSqrt3 / 2)}; }
// Vendaval Glacial's cone: every hex within `length` of the caster whose direction lies within ~37 degrees of the aim
// (one hex wide next to the caster, five or so at its end).
std::vector<BattleHex> coneHexes(const Battle& b, BattleHex origin, BattleHex aim, int length) {
  std::vector<BattleHex> out;
  if (origin == aim) return out;
  const auto [ox, oz] = hexCentre(origin);
  const auto [ax, az] = hexCentre(aim);
  const double dx = ax - ox, dz = az - oz, dl = std::hypot(dx, dz);
  for (const BattleHex& h : hexesWithin(b, origin, length)) {
    if (h == origin) continue;
    const auto [hx, hz] = hexCentre(h);
    const double vx = hx - ox, vz = hz - oz, vl = std::hypot(vx, vz);
    if (vl < 1e-6) continue;
    if ((vx * dx + vz * dz) / (vl * dl) >= 0.8) out.push_back(h);
  }
  return out;
}
// Pushes a one-hex unit `steps` hexes away from `from` onto free, passable ground (the hex farthest from `from`, the
// straightest when tied). False when it did not move.
bool pushAway(Battle& b, size_t unit, BattleHex from, int steps) {
  BattleUnit& u = b.units()[unit];
  if (u.size > 1 || u.footprintOffsets) return false;
  bool moved = false;
  for (int s = 0; s < steps; ++s) {
    const int here = hexDist(from, {u.x, u.y});
    const auto [fx, fz] = hexCentre(from);
    const auto [ux, uz] = hexCentre({u.x, u.y});
    const double awayX = ux - fx, awayZ = uz - fz;
    std::optional<BattleHex> best;
    int bestD = here;
    double bestDot = -1e9;
    for (const BattleHex& n : Battle::neighbors(u.x, u.y)) {
      if (!b.inBounds(n.x, n.y)) continue;
      const int d = hexDist(from, n);
      if (d <= here) continue;
      const BattleCell& c = b.cell(n.x, n.y);
      if (!c.passable || c.blocked || c.terrain == "void" || c.terrain == "water" || c.terrain == "barricade") continue;
      if (b.unitAt(n.x, n.y) >= 0) continue;
      const auto [nx, nz] = hexCentre(n);
      const double dot = (nx - ux) * awayX + (nz - uz) * awayZ;
      if (!best || d > bestD || (d == bestD && dot > bestDot)) best = n, bestD = d, bestDot = dot;
    }
    if (!best) break;
    u.x = best->x, u.y = best->y;
    moved = true;
  }
  return moved;
}
// Poisons a unit with a tier (a weaker tier never replaces a stronger one), remembering the poisoner's MAG, whether it
// spreads (Praga Rastejante) and whether it bursts on death (Peste Negra).
void applyPoison(BattleUnit& t, const std::string& tier, double mag, int spread, bool burst) {
  const PoisonTier* nt = poisonTier(tier);
  if (!nt) return;
  const PoisonTier* ot = t.poisoned ? poisonTier(t.poisonTier) : nullptr;
  if (!ot || nt->rank >= ot->rank) t.poisonTier = tier;
  t.poisoned = true;
  t.cond.poisonMag = std::max(t.cond.poisonMag, mag);
  t.cond.plagueSpread = std::max(t.cond.plagueSpread, spread);
  t.cond.plagueBurst = t.cond.plagueBurst || burst;
}
BattleEffect statusEffect(size_t unit, std::string text) {
  BattleEffect e;
  e.unit = unit;
  e.status = true;
  e.text = std::move(text);
  return e;
}
// Imagem Espelhada (the Conjurador's): a hostile blow on a unit with illusions standing has one chance in three of striking a copy
// instead, which vanishes (the attack is spent: nothing lands). Draws one random number when a copy stands.
bool mirrorAbsorbs(BattleUnit& target, Mulberry32& rng, size_t id, std::vector<BattleEffect>& out) {
  if (target.cond.mirrorCopies <= 0) return false;
  if (rng() * 3.0 >= 1.0) return false;
  target.cond.mirrorCopies -= 1;
  if (target.cond.mirrorCopies <= 0) target.cond.mirrorRounds = 0;
  BattleEffect e = statusEffect(id, pick("Ilusão", "Illusion"));
  e.miss = true;  // the window shows it as a blow that found nothing; the log says why
  out.push_back(e);
  return true;
}
// Elo de Sacrifício (the Conjurador's): the unit's nearest living summon takes the hit's share instead (the rest of it is gone); the
// effects of what the summon suffers go to `out`. False when nothing stands between.
bool bondRedirects(Battle& b, size_t target, int dmg, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& t = b.units()[target];
  if (t.cond.bondPct <= 0 || dmg <= 0) return false;
  int nearest = -1, best = 1 << 30;
  for (size_t i : summonsOf(b, target)) {
    const int d = hexDist({t.x, t.y}, {b.units()[i].x, b.units()[i].y});
    if (d < best) best = d, nearest = static_cast<int>(i);
  }
  if (nearest < 0) return false;
  BattleUnit& s = b.units()[static_cast<size_t>(nearest)];
  const int taken = std::max(0, static_cast<int>(std::floor(dmg * t.cond.bondPct / 100.0)));
  s.hp = std::max(0, s.hp - taken);
  out.push_back(statusEffect(target, pick("Elo de Sacrifício", "Sacrifice Bond")));
  BattleEffect e{static_cast<size_t>(nearest), taken, false, false, false, false, false};
  if (s.hp <= 0) markDead(s, rng), e.died = true;
  out.push_back(e);
  return true;
}
// Marca da Ruína: the marked take +pct% from everything.
int marked(const BattleUnit& t, int dmg) {
  return t.cond.markTurns > 0 ? static_cast<int>(std::floor(dmg * (1 + t.cond.markPct / 100.0))) : dmg;
}
// Maldição da Decadência: its stats at the curse's current percentage off what they were.
void applyDecay(BattleUnit& u) {
  if (!u.decayBase) u.decayBase = BattleUnit::Stats{u.atk, u.mag, u.def, u.dex, u.mov};
  const double keep = 1 - u.cond.decayPct / 100.0;
  auto r = [&](int v) { return static_cast<int>(jsRound(v * keep)); };
  u.atk = r(u.decayBase->atk), u.mag = r(u.decayBase->mag), u.def = r(u.decayBase->def), u.dex = r(u.decayBase->dex);
  u.mov = std::max(1, r(u.decayBase->mov));
}
// Tiro Paralisante (the Arqueiro's): MOV at hobblePct % off what it was, given back when the condition lifts.
void applyHobble(BattleUnit& u) {
  if (!u.hobbleBase) u.hobbleBase = u.mov;
  u.mov = std::max(1, static_cast<int>(jsRound(*u.hobbleBase * (1 - u.cond.hobblePct / 100.0))));
}
void liftHobble(BattleUnit& u) {
  if (u.hobbleBase) u.mov = *u.hobbleBase;
  u.hobbleBase.reset();
  u.cond.hobblePct = u.cond.hobbleTurns = 0;
}
// Desarmar (the Lanceiro's): ATK at disarmPct % off what it was, given back when the condition lifts.
void applyDisarm(BattleUnit& u) {
  if (!u.disarmBase) u.disarmBase = u.atk;
  u.atk = std::max(0, static_cast<int>(jsRound(*u.disarmBase * (1 - u.cond.disarmPct / 100.0))));
}
void liftDisarm(BattleUnit& u) {
  if (u.disarmBase) u.atk = *u.disarmBase;
  u.disarmBase.reset();
  u.cond.disarmPct = u.cond.disarmTurns = 0;
}
void liftDecay(BattleUnit& u) {
  if (u.decayBase) u.atk = u.decayBase->atk, u.mag = u.decayBase->mag, u.def = u.decayBase->def, u.dex = u.decayBase->dex, u.mov = u.decayBase->mov;
  u.decayBase.reset();
  u.cond.decayTurns = u.cond.decayPct = u.cond.decayCap = u.cond.decayStep = u.cond.decayDice = u.cond.decayFaces = 0;
  u.cond.decayMul = u.cond.decayMag = 0;
}
// Peste Negra: a unit dying poisoned by it leaves Veneno Maior on its neighbours of its own side.
void afterDeath(Battle& b, size_t dead, std::vector<BattleEffect>& out) {
  BattleUnit& d = b.units()[dead];
  if (!d.cond.plagueBurst || !d.poisoned) return;
  d.cond.plagueBurst = false;
  const std::vector<BattleHex> body = Battle::footprint(d, d.x, d.y);
  for (size_t i = 0; i < b.units().size(); ++i) {
    BattleUnit& t = b.units()[i];
    if (i == dead || !t.alive || t.side != d.side) continue;
    const std::vector<BattleHex> theirs = Battle::footprint(t, t.x, t.y);
    const bool adjacent = std::any_of(body.begin(), body.end(), [&](const BattleHex& p) {
      return std::any_of(theirs.begin(), theirs.end(), [&](const BattleHex& q) { return hexDist(p, q) == 1; });
    });
    if (!adjacent) continue;
    applyPoison(t, "greater", d.cond.poisonMag, 0, false);
    out.push_back(statusEffect(i, "Veneno Maior"));
  }
}
// What eats at a unit at the start of its turn (and again, at once, under Colheita de Almas: countDown = false): its
// poison (Engine2's rule: the tier's dice, its resistance less the tier's penalty, the poisoner's MAG / 2 penetrating),
// its bleeding, its curse's tick and the curse's next step.
void tickConditions(Battle& b, size_t id, Mulberry32& rng, std::vector<BattleEffect>& out, bool countDown, bool withPoison = true) {
  BattleUnit& u = b.units()[id];
  auto hurt = [&](int dmg, const std::string& element, const char* cause) {
    if (!u.alive) return;
    dmg = marked(u, std::max(0, dmg));
    u.hp = std::max(0, u.hp - dmg);
    if (!element.empty()) trainResistance(u, element, std::nullopt, rng);
    BattleEffect e;
    e.unit = id, e.amount = dmg, e.cause = cause;
    if (u.hp <= 0) markDead(u, rng), e.died = true;
    out.push_back(e);
    if (!u.alive) afterDeath(b, id, out);
  };
  if (u.alive && u.poisoned && withPoison)
    if (const PoisonTier* t = poisonTier(u.poisonTier.empty() ? "lesser" : u.poisonTier)) {
      // poisonTickDamage(roll, effectivePoisonResistance(PR, tier, MAG)): rounded once, after the resistance
      const int roll = rollDice(t->dice, t->faces, 0, rng);
      const double eff = std::max(-50.0, std::min(100.0, u.combat.resistance("poison") - t->penalty - u.cond.poisonMag / 2));
      hurt(std::max(0, static_cast<int>(jsRound(roll * (1 - eff / 100)))), "poison", pick("Veneno consome", "Poison consumes"));
    }
  if (u.alive && u.cond.bleedTurns > 0) {
    hurt(rollDice(u.cond.bleedDice, u.cond.bleedFaces, 0, rng), "", pick("Sangramento fere", "Bleeding hurts"));
    if (countDown && --u.cond.bleedTurns <= 0) u.cond.bleedDice = u.cond.bleedFaces = 0;
  }
  if (u.alive && u.cond.decayTurns > 0) {
    const int roll = rollDice(u.cond.decayDice, u.cond.decayFaces, 0, rng);
    const int base = static_cast<int>(std::floor(std::floor(u.cond.decayMag / 2) * u.cond.decayMul)) + roll;
    hurt(static_cast<int>(std::floor(elementalDamage(base, u.combat.resistance("darkness"), u.cond.decayMag))), "darkness", pick("Decadência consome", "Decay consumes"));
    if (countDown && u.alive) {
      if (--u.cond.decayTurns <= 0) {
        liftDecay(u);
      } else {
        u.cond.decayPct = std::min(u.cond.decayCap, u.cond.decayPct + u.cond.decayStep);
        applyDecay(u);
      }
    }
  }
}
// A zone's hazard or tick on whoever starts a turn inside it.
void zoneEffects(Battle& b, size_t id, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& u = b.units()[id];
  const std::vector<BattleHex> body = Battle::footprint(u, u.x, u.y);
  for (const Battle::Zone& z : std::vector<Battle::Zone>(b.zones())) {  // a copy: a death may change the list
    if (!u.alive) return;
    if (z.kind == "meteor" || (z.hazardDice <= 0 && z.dice <= 0)) continue;
    if (std::none_of(body.begin(), body.end(), [&](const BattleHex& h) { return z.covers(h); })) continue;
    int dmg = 0;
    if (z.hazardDice > 0) dmg = static_cast<int>(std::floor(elementalDamage(rollDice(z.hazardDice, z.hazardFaces, 0, rng), u.combat.resistance(z.element), 0)));
    else dmg = static_cast<int>(std::floor(elementalDamage(std::floor(std::floor(z.mag / 2) * z.mul) + rollDice(z.dice, z.faces, 0, rng), u.combat.resistance(z.element), z.mag)));
    dmg = marked(u, std::max(0, dmg));
    u.hp = std::max(0, u.hp - dmg);
    trainResistance(u, z.element, std::nullopt, rng);
    BattleEffect e;
    e.unit = id, e.amount = dmg, e.cause = gLogEnglish ? elementLabelPt(z.element) + " zone hurts" : "Zona de " + elementLabelPt(z.element) + " fere";
    if (u.hp <= 0) markDead(u, rng), e.died = true;
    out.push_back(e);
    if (!u.alive) afterDeath(b, id, out);
  }
}
// Meteoro lands: the zones this caster left fall now, on everyone inside (the centre hex takes the heavier blow).
void landMeteors(Battle& b, size_t caster, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& att = b.units()[caster];
  std::vector<int> falling;
  for (const Battle::Zone& z : b.zones())
    if (z.kind == "meteor" && z.caster == caster) falling.push_back(z.id);
  for (const int id : falling) {
    const auto it = std::find_if(b.zones().begin(), b.zones().end(), [&](const Battle::Zone& z) { return z.id == id; });
    if (it == b.zones().end()) continue;
    const Battle::Zone z = *it;
    b.removeZone(id);
    b.noteLanded(z);
    const std::vector<BattleHex> area = z.cells.empty() ? hexesWithin(b, z.centre, z.radius) : z.cells;
    bool first = true;
    for (size_t i : unitsOn(b, caster, area, true)) {
      BattleUnit& foe = b.units()[i];
      const bool centre = occupies(foe, z.centre.x, z.centre.y);
      const int roll = centre ? rollDice(z.centreDice, z.centreFaces, 0, rng) : rollDice(z.ringDice, z.ringFaces, 0, rng);
      const double base = std::floor(std::floor(z.mag / 2) * (centre ? z.centreMul : z.ringMul)) + roll - b.cell(foe.x, foe.y).cover;
      int dmg = static_cast<int>(std::floor(elementalDamage(std::max(1.0, base), foe.combat.resistance("fire"), z.mag)));
      dmg = marked(foe, std::max(1, dmg));
      foe.hp = std::max(0, foe.hp - dmg);
      trainResistance(foe, "fire", att.alive ? std::optional<int>(att.level) : std::nullopt, rng);
      if (att.alive) {
        b.noteDamage(caster, i, dmg, true);
        if (foe.side != att.side) gainExp(att, foe.level, dmg, first ? 1 : 0.5, foe.hp <= 0);
      }
      first = false;
      BattleEffect e;
      e.unit = i, e.amount = dmg, e.cause = pick("Meteoro atinge", "Meteor strikes");
      if (foe.hp <= 0) markDead(foe, rng), e.died = true;
      out.push_back(e);
      if (!foe.alive) afterDeath(b, i, out);
    }
  }
}
// The cast of one of ours (castSpell's branch): the steps to play, or nothing with the reason.
// Salto Devastador: where the caster lands to strike the foe: its own hex when the weapon already reaches it, else the
// free, passable hex beside the foe nearest to the caster (the first in the neighbours' order on a tie) within `range`
// hexes of the caster; none when there is no such hex.
std::optional<BattleHex> leapLanding(const Battle& b, size_t caster, size_t foe, int range) {
  const BattleUnit &u = b.units()[caster], &t = b.units()[foe];
  const BattleHex from{u.x, u.y};
  if (canHitFrom(b, caster, from, foe)) return from;
  std::optional<BattleHex> best;
  int bestD = 1 << 30;
  for (const BattleHex& h : Battle::neighbors(t.x, t.y)) {
    if (!b.inBounds(h.x, h.y) || !b.cell(h.x, h.y).passable || b.cell(h.x, h.y).terrain == "void" || occAt(b, h) >= 0) continue;
    const int d = hexDist(from, h);
    if (d > range || d >= bestD) continue;
    if (!canHitFrom(b, caster, h, foe)) continue;
    bestD = d, best = h;
  }
  return best;
}

// --- The Lanceiro's board moves (docs/SKILLS_LANCER.md) ----------------------------------------------------------------------
// A hex a body may be thrown, pulled or dragged onto: on the board, walkable ground, nobody on it.
bool landable(const Battle& b, BattleHex h) {
  if (!b.inBounds(h.x, h.y)) return false;
  const BattleCell& c = b.cell(h.x, h.y);
  if (!c.passable || c.blocked || c.terrain == "void" || c.terrain == "water" || c.terrain == "barricade") return false;
  return b.unitAt(h.x, h.y) < 0;
}
// One body on one hex (a bigger one cannot be thrown about).
bool oneHex(const BattleUnit& u) { return u.size <= 1 && !u.footprintOffsets; }
// What the hook can pull: a body of at most two hexes (a wolf, whose authored shape is two; the user, 2026-10-19: "why I cant pull the wolf
// with the hook? ... the max size he can pull is body type 2").
bool pullable(const BattleUnit& u) { return u.size <= 2 && Battle::footprint(u, u.x, u.y).size() <= 2; }
// Every hex of `u`'s body with its anchor on `at` is on the board, walkable and free (its own hexes count as free).
bool bodyFitsAt(const Battle& b, const BattleUnit& u, size_t self, BattleHex at) {
  for (const BattleHex& p : Battle::footprint(u, at.x, at.y)) {
    if (!b.inBounds(p.x, p.y)) return false;
    const BattleCell& c = b.cell(p.x, p.y);
    if (!c.passable || c.blocked || c.terrain == "void" || c.terrain == "water" || c.terrain == "barricade") return false;
    if (const int who = b.unitAt(p.x, p.y); who >= 0 && who != static_cast<int>(self)) return false;
  }
  return true;
}
// The hex past `through` on the straight line from `from` (for neighbours: the next hex in that direction).
BattleHex hexBeyond(BattleHex from, BattleHex through) {
  const Cube a = oddrToCube(from.x, from.y), t = oddrToCube(through.x, through.y);
  return cubeToOddr(static_cast<int>(std::lround(2 * t.q - a.q)), static_cast<int>(std::lround(2 * t.r - a.r)));
}
// The neighbour foe standing on `aim` itself, when the aim is a click on it (-1: the aim is a hex): Arremesso then throws that very foe,
// Empalar e Arrastar drags that very foe, straight away from the caster as far as it goes (the user, 2026-10-19: "should allow me to aim
// in different targets ... I can't ever fire anything where I want").
int aimedNeighbour(const Battle& b, const BattleUnit& u, BattleHex aim) {
  if (hexDist({u.x, u.y}, aim) != 1) return -1;
  const int foe = occAt(b, aim);
  return targetable(b, foe) && oneHex(b.units()[static_cast<size_t>(foe)]) ? foe : -1;
}
// The hex `steps` past `through` on the line from `from`, stopping at the board's edge.
BattleHex farAlong(const Battle& b, BattleHex from, BattleHex through, int steps) {
  BattleHex prev = from, at = through;
  for (int k = 0; k < steps; ++k) {
    const BattleHex next = hexBeyond(prev, at);
    if (!b.inBounds(next.x, next.y)) break;
    prev = at, at = next;
  }
  return at;
}
// Arremesso: the neighbour to be thrown toward `aim` (the one nearest it), its flight, where it ends and what it hits.
struct TossPlan {
  size_t foe = 0;
  std::optional<BattleHex> landing;  // where it ends (none: it never leaves its hex)
  std::optional<size_t> collide;     // the unit it lands against
  std::vector<BattleHex> tiles;      // the foe's hex, its flight, the body it hits
  int hexes = 0;                     // how far it flies
};
std::optional<TossPlan> tossPlan(const Battle& b, size_t caster, BattleHex aim, const EmberNumbers& n, std::string* why = nullptr) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    return std::optional<TossPlan>();
  };
  if (!b.inBounds(aim.x, aim.y)) return fail("Fora do mapa.");
  if (aim == from) return fail("Escolha para onde lançar o inimigo vizinho.");
  std::optional<TossPlan> best;
  int bestD = 1 << 30;
  const int aimed = aimedNeighbour(b, u, aim);  // a click on the neighbour itself: that one, straight away from the caster
  for (const BattleHex& h : Battle::neighbors(u.x, u.y)) {
    const int foe = occAt(b, h);
    if (!targetable(b, foe) || !oneHex(b.units()[static_cast<size_t>(foe)])) continue;
    if (aimed >= 0 && foe != aimed) continue;
    const BattleHex goal = aimed >= 0 ? farAlong(b, from, h, n.range) : aim;
    const int d = hexDist(h, goal);
    if (d < 1 || d > n.range || d >= bestD) continue;
    const std::vector<BattleHex> line = hexLine(h, goal);
    if (std::any_of(line.begin(), line.end(), [&](const BattleHex& c) { return c == from; })) continue;  // never through the thrower
    TossPlan p;
    p.foe = static_cast<size_t>(foe);
    p.tiles.push_back(h);
    BattleHex at = h;
    for (size_t k = 1; k < line.size(); ++k) {
      const BattleHex& c = line[k];
      if (!b.inBounds(c.x, c.y)) break;
      if (const int who = b.unitAt(c.x, c.y); who >= 0) {
        p.collide = static_cast<size_t>(who);
        p.tiles.push_back(c);
        break;
      }
      if (!landable(b, c)) break;
      at = c, p.tiles.push_back(c), ++p.hexes;
    }
    if (p.hexes == 0 && !p.collide) continue;  // nowhere to go that way
    if (p.hexes > 0) p.landing = at;
    best = p, bestD = d;
  }
  if (!best) return fail(std::format("Escolha um hex a até {} hexes de um inimigo vizinho, sem passar por você.", n.range));
  return best;
}
// Gancho: the foe hooked at `aim` and the free hex on the line it is pulled to (the nearest to the caster).
struct HookPlan {
  size_t foe = 0;
  BattleHex dest;
  std::vector<BattleHex> line;
};
std::optional<HookPlan> hookPlan(const Battle& b, size_t caster, BattleHex aim, const EmberNumbers& n, std::string* why = nullptr) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    return std::optional<HookPlan>();
  };
  if (!b.inBounds(aim.x, aim.y)) return fail("Fora do mapa.");
  const int foe = occAt(b, aim);
  if (!targetable(b, foe)) return fail(foe < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
  if (!pullable(b.units()[static_cast<size_t>(foe)])) return fail("Grande demais para ser puxado.");
  const int d = hexDist(from, aim);
  if (d < 2) return fail("Alvo perto demais: o gancho pega a 2 hexes ou mais.");
  if (d > n.range) return fail(std::format("Alvo fora de alcance (máximo {} hexes).", n.range));
  if (!clearShot(b, from, aim, true)) return fail("O gancho está bloqueado.");
  HookPlan p;
  p.foe = static_cast<size_t>(foe);
  p.line = hexLine(from, aim);
  for (size_t k = 1; k + 1 < p.line.size(); ++k)
    if (bodyFitsAt(b, b.units()[static_cast<size_t>(foe)], static_cast<size_t>(foe), p.line[k])) {
      p.dest = p.line[k];
      return p;
    }
  return fail("Não há espaço para puxá-lo até você.");
}
// Salto de Vara: the hex behind the neighbour at `aim` (on the line through it), where the caster lands.
std::optional<BattleHex> vaultLanding(const Battle& b, size_t caster, BattleHex aim, std::string* why = nullptr) {
  const BattleUnit& u = b.units()[caster];
  auto fail = [&](const char* w) {
    if (why) *why = w;
    return std::optional<BattleHex>();
  };
  if (!b.inBounds(aim.x, aim.y)) return fail("Fora do mapa.");
  const int foe = occAt(b, aim);
  if (!targetable(b, foe)) return fail(foe < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
  if (hexDist({u.x, u.y}, aim) != 1) return fail("O salto passa por cima de um inimigo vizinho.");
  if (!oneHex(u) || !oneHex(b.units()[static_cast<size_t>(foe)])) return fail("Não dá para saltar por cima desse inimigo.");
  const BattleHex landing = hexBeyond({u.x, u.y}, aim);
  if (!landable(b, landing)) return fail("Não há hex livre atrás do inimigo.");
  return landing;
}
// Troca de Posto: the ally at `aim` the caster changes places with (-1 with the reason).
int swapAlly(const Battle& b, size_t caster, BattleHex aim, const EmberNumbers& n, std::string* why = nullptr) {
  const BattleUnit& u = b.units()[caster];
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    return -1;
  };
  if (!b.inBounds(aim.x, aim.y)) return fail("Fora do mapa.");
  const int ally = occAt(b, aim);
  if (ally < 0) return fail("Não há aliado na casa selecionada.");
  const BattleUnit& a = b.units()[static_cast<size_t>(ally)];
  if (ally == static_cast<int>(caster) || !a.alive || a.side != u.side) return fail("Escolha um aliado para trocar de lugar.");
  if (!oneHex(u) || !oneHex(a)) return fail("Não dá para trocar de lugar com esse aliado.");
  if (hexDist({u.x, u.y}, aim) > n.range) return fail(std::format("Aliado fora de alcance (máximo {} hexes).", n.range));
  return ally;
}
// Empalar e Arrastar: the neighbour dragged in a straight line away from the caster until `aim`; the caster follows one hex behind.
struct DragPlan {
  size_t foe = 0;
  BattleHex foeTo, casterTo;
  std::vector<BattleHex> tiles;  // the foe's hex and the hexes it is dragged over
  int hexes = 0;
};
std::optional<DragPlan> dragPlan(const Battle& b, size_t caster, BattleHex aim, const EmberNumbers& n, std::string* why = nullptr) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    return std::optional<DragPlan>();
  };
  if (!b.inBounds(aim.x, aim.y)) return fail("Fora do mapa.");
  if (!oneHex(u)) return fail("Não dá para arrastar ninguém assim.");
  const int aimed = aimedNeighbour(b, u, aim);  // a click on the neighbour itself: dragged as far as the line stays free
  for (const BattleHex& h : Battle::neighbors(u.x, u.y)) {
    const int foe = occAt(b, h);
    if (!targetable(b, foe) || !oneHex(b.units()[static_cast<size_t>(foe)])) continue;
    if (aimed >= 0 && foe != aimed) continue;
    DragPlan p;
    p.foe = static_cast<size_t>(foe);
    p.tiles.push_back(h);
    BattleHex prev = from, at = h;
    for (int k = 0; k < n.length; ++k) {
      const BattleHex next = hexBeyond(prev, at);
      if (!landable(b, next)) break;
      prev = at, at = next;
      p.tiles.push_back(at), ++p.hexes;
      if (aimed < 0 && at == aim) {
        p.foeTo = at, p.casterTo = prev;
        return p;
      }
    }
    if (aimed >= 0 && p.hexes > 0) {
      p.foeTo = at, p.casterTo = prev;
      return p;
    }
    if (aimed >= 0) return fail("Não há espaço livre atrás dele para arrastá-lo.");
  }
  return fail(std::format("Escolha um inimigo vizinho ou um hex em linha reta atrás dele, a até {} hexes.", n.length));
}

std::optional<std::vector<BattleStep>> castEmber(Battle& b, size_t caster, const std::string& kind, const std::vector<BattleHex>& cells, std::string* error) {
  BattleUnit& u = b.units()[caster];
  auto fail = [&](const std::string& why) -> std::optional<std::vector<BattleStep>> {
    if (error) *error = why;
    return std::nullopt;
  };
  const EmberNumbers n = emberNumbers(kind, u.level);
  const BattleHex from{u.x, u.y}, cell = cells[0];
  std::vector<BattleStep> steps;
  BattleStep s;
  s.att = caster, s.kind = kind;
  s.dice = n.dice, s.faces = n.faces, s.bonus = 0, s.spellMul = n.mul;
  auto spend = [&] { spendTier(u, kind); };
  if (emberBoltSpell(kind) && kind != "wrathOfTheSkies") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Não há inimigo visível nessa casa.");
    s.tiles = {cell}, s.ids = {static_cast<size_t>(foe)};
    if (kind == "markOfRuin") s.markPct = n.pct, s.markTurns = n.turns;
    else if (kind == "creepingPlague") s.poisonTier = n.tier, s.plagueSpread = n.spread;
    else if (kind == "lifeDrain") s.drainPct = n.pct;
    else if (kind == "hemorrhage") s.bleedDice = n.tickDice, s.bleedFaces = n.tickFaces, s.bleedTurnsDice = n.durDice, s.bleedTurnsFaces = n.durFaces;
    else if (kind == "devouringVoid") s.execPct = n.pct, s.shredDarkness = n.spread;
    else if (kind == "curseOfDecay")
      s.decayStep = n.spread, s.decayCap = n.pct, s.decayTurns = n.turns, s.decayDice = n.tickDice, s.decayFaces = n.tickFaces, s.decayMul = n.tickMul;
    else if (kind == "chainLightning") {
      // The chain: from each struck unit to the nearest foe within reach not struck yet, each jump weaker.
      s.perTargetMul = {1.0};
      BattleHex last = cell;
      for (int j = 0; j < n.jumps; ++j) {
        int next = -1, bestD = 1 << 30;
        for (size_t i = 0; i < b.units().size(); ++i) {
          const BattleUnit& t = b.units()[i];
          if (!t.alive || t.side == u.side || std::find(s.ids.begin(), s.ids.end(), i) != s.ids.end()) continue;
          const int d = hexDist(last, {t.x, t.y});
          if (d >= 1 && d <= n.length && d < bestD) bestD = d, next = static_cast<int>(i);
        }
        if (next < 0) break;
        s.ids.push_back(static_cast<size_t>(next));
        s.tiles.push_back({b.units()[static_cast<size_t>(next)].x, b.units()[static_cast<size_t>(next)].y});
        s.perTargetMul.push_back(s.perTargetMul.back() * (1 - n.decay));
        last = s.tiles.back();
      }
    }
    spend();
    steps.push_back(s);
  } else if (kind == "wrathOfTheSkies") {
    // One bolt per aimed hex, each with its own roll and Relâmpago Maior's echo (castMagicMissile's way).
    std::vector<std::pair<size_t, BattleHex>> bolts;
    for (const BattleHex& c : cells) {
      const int foe = occAt(b, c);
      if (foe < 0) return fail("Não há inimigo visível nessa casa.");
      bolts.emplace_back(static_cast<size_t>(foe), c);
    }
    spend();
    for (const auto& [foe, c] : bolts) {
      BattleStep m = s;
      m.tiles = {c}, m.ids = {foe};
      m.echo = BattleUnit::Echo{2, 12, 8, 0};
      steps.push_back(m);
    }
  } else if (kind == "circleOfHealing") {
    // Holy light over an area: every ally standing in it (the caster too) heals its own roll of the cure's formula. A Heal step
    // with the whole list in `ids` (def = the caster: nobody to turn to).
    const std::vector<BattleHex> area = areaTiles(b, cell, n.radius);
    s.type = BattleStep::Type::Heal;
    s.def = caster;
    s.tiles = area;
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& t = b.units()[i];
      if (!t.alive || t.side != u.side) continue;
      if (!b.debugFreeCast() && t.hp >= t.maxHp) continue;  // the cures' rule: only the hurt are healed (nothing to say for the rest)
      if (std::any_of(area.begin(), area.end(), [&](const BattleHex& c) { return occupies(t, c.x, c.y); })) s.ids.push_back(i);
    }
    if (s.ids.empty()) return fail("Não há aliado ferido nessa área.");
    spend();
    steps.push_back(s);
  } else if (kind == "placeSwap") {
    // Troca de Lugar (the Conjurador's): places changed with an ally or a summon in reach; nothing else.
    std::string why;
    const int ally = swapAlly(b, caster, cell, n, &why);
    if (ally < 0) return fail(why);
    spend();
    s.type = BattleStep::Type::Effect;
    s.ids = {static_cast<size_t>(ally)}, s.tiles = {cell, from};
    s.dice = 0, s.spellMul = 1;
    steps.push_back(s);
  } else if (kind == "boneLance") {
    // A lance of bone through everyone on the line, both sides, as far as it reaches.
    const std::vector<BattleHex> line = boneLine(b, from, cell, n.length);
    if (line.empty()) return fail("Escolha uma reta a partir do personagem.");
    s.tiles = line;
    s.ids = unitsOn(b, caster, line, true);
    spend();
    steps.push_back(s);
  } else if (kind == "mirrorImage") {
    // Illusions round an ally (the caster itself too) for some rounds.
    const int ally = occAt(b, cell);
    if (ally < 0 || !b.units()[static_cast<size_t>(ally)].alive || b.units()[static_cast<size_t>(ally)].side != u.side) return fail("Escolha um aliado.");
    if (hexDist(from, cell) > n.range) return fail(std::format("Aliado fora de alcance (máximo {} hexes).", n.range));
    spend();
    s.type = BattleStep::Type::Effect;
    s.ids = {static_cast<size_t>(ally)}, s.tiles = {cell};
    s.dice = 0, s.spellMul = 1;
    s.mirrorCopies = n.count, s.mirrorRounds = n.rounds;
    steps.push_back(s);
  } else if (kind == "detonateSummon") {
    // One of the caster's own summons blows up: everyone within the radius of it (both sides) takes the roll; the summon is gone.
    const int summon = ownSummonAt(b, caster, cell);
    if (summon < 0) return fail("Escolha uma invocação sua.");
    if (hexDist(from, cell) > n.range) return fail(std::format("Invocação fora de alcance (máximo {} hexes).", n.range));
    const BattleUnit& sm = b.units()[static_cast<size_t>(summon)];
    const std::vector<BattleHex> area = areaTiles(b, {sm.x, sm.y}, n.radius);
    s.tiles = area;
    for (size_t id : unitsOn(b, caster, area, true))
      if (id != static_cast<size_t>(summon)) s.ids.push_back(id);
    s.sacrifice = static_cast<size_t>(summon);
    spend();
    steps.push_back(s);
  } else if (kind == "sacrificeBond") {
    // On the caster, for the rest of the fight: what hits it goes to its nearest summon.
    if (!(cell == from)) return fail("Esta magia não precisa de alvo.");
    spend();
    s.type = BattleStep::Type::Effect;
    s.ids = summonsOf(b, caster), s.tiles = {from};  // the summons it chains to (the window shows them); the bond itself is the caster's
    s.dice = 0, s.spellMul = 1;
    s.bondPct = n.pct;
    steps.push_back(s);
  } else if (kind == "empowerSummons") {
    // Every living summon of the caster, stronger for some rounds.
    if (!(cell == from)) return fail("Esta magia não precisa de alvo.");
    s.ids = summonsOf(b, caster);
    if (s.ids.empty()) return fail("Nenhuma invocação em campo.");
    spend();
    s.type = BattleStep::Type::Effect;
    s.tiles = {from};
    s.dice = 0, s.spellMul = 1;
    s.empowerPct = n.pct, s.empowerRounds = n.rounds;
    steps.push_back(s);
  } else if (kind == "resurrection") {
    // The fallen ally on the hex beside the caster stands again with its own roll of HP and acts on its next turn.
    const int fallen = fallenAllyAt(b, caster, cell);
    if (fallen < 0) return fail("Não há aliado caído nessa casa.");
    if (occAt(b, cell) >= 0) return fail("Alguém está sobre o aliado caído.");
    spend();
    s.type = BattleStep::Type::Heal;
    s.def = static_cast<size_t>(fallen);
    s.tiles = {cell};
    steps.push_back(s);
  } else if (emberAreaSpell(kind)) {
    const std::vector<BattleHex> area = areaTiles(b, cell, n.radius);
    s.tiles = area;
    if (kind == "meteor") {
      s.delayed = true;  // its zone is laid at the impact; it falls at the caster's next turn
    } else if (kind == "shroudOfShadows") {
      s.dice = 0, s.spellMul = 0;  // the zone alone (laid at the impact)
    } else {
      s.ids = unitsOn(b, caster, area, emberBothSides(kind));
      if (kind == "judgment")  // the undead take the heavier weight
        for (size_t id : s.ids) s.perTargetMul.push_back(b.units()[id].undead ? n.tickMul : 1.0);
      if (kind == "nightmare") {  // a sleeper takes double (the hit's own +25 % on a sleeper folded in), and each struck foe may be afraid
        for (size_t id : s.ids) s.perTargetMul.push_back(b.units()[id].asleep ? n.tickMul / 1.25 : 1.0);  // 1.25 = kWebSleepBonusDamage (SpellsE2.inc)
        s.fearPct = n.pct, s.fearTurns = n.turns;
      }
      if (kind == "profaneSilence") s.silenceTurns = n.turns;
      else if (kind == "blackPestilence") s.poisonTier = n.tier, s.plagueBurst = true;
      else if (kind == "soulHarvest") s.harvest = true, s.harvestHealPct = n.pct;
      else if (kind == "thunderstorm") {
        s.echoDice = n.tickDice, s.echoFaces = n.tickFaces, s.echoBonus = n.bonus;
        for (size_t id : s.ids) {
          const BattleUnit& t = b.units()[id];
          for (size_t i = 0; i < b.units().size(); ++i) {
            const BattleUnit& v = b.units()[i];
            if (!v.alive || v.side == u.side || i == caster) continue;
            if (std::find(s.ids.begin(), s.ids.end(), i) != s.ids.end() || std::find(s.echoIds.begin(), s.echoIds.end(), i) != s.echoIds.end()) continue;
            if (hexDist({t.x, t.y}, {v.x, v.y}) == 1) {
              s.echoIds.push_back(i);
              break;
            }
          }
        }
      } else if (kind == "absoluteZero") {
        for (size_t id : s.ids) s.perTargetMul.push_back(b.inZoneOf(b.units()[id], "ice") ? 1.5 : 1.0);
      }
    }
    spend();
    steps.push_back(s);
  } else if (kind == "eruption") {
    if (!(cell == from)) return fail("Esta magia não precisa de alvo.");
    const std::vector<BattleHex> area = areaTiles(b, from, n.radius);
    s.tiles = area;
    s.ids = unitsOn(b, caster, area, false);
    spend();
    steps.push_back(s);
  } else if (kind == "iceLance") {
    const std::vector<BattleHex> line = piercingRay(b, from, cell);
    if (line.empty()) return fail("Escolha uma reta válida.");
    s.ids = unitsOnLine(b, caster, line);
    s.tiles = line;
    for (size_t k = 0; k < s.ids.size(); ++k) s.perTargetMul.push_back(k == 0 ? 1.0 : n.pct / 100.0);
    spend();
    steps.push_back(s);
  } else if (kind == "glacialGale") {
    const std::vector<BattleHex> cone = coneHexes(b, from, cell, n.length);
    if (cone.empty()) return fail("Escolha uma direção a partir do personagem.");
    s.tiles = cone;
    s.ids = unitsOn(b, caster, cone, true);
    s.push = n.push;
    spend();
    steps.push_back(s);
  } else if (kind == "steelWhirlwind") {
    // One turn of the blade: a weapon blow (+ the bonus dice) on every foe round the caster; nobody strikes back.
    if (!(cell == from)) return fail("Esta habilidade gira em volta de quem a usa.");
    std::vector<BattleHex> ring = hexesWithin(b, from, n.radius);
    std::erase_if(ring, [&](const BattleHex& h) { return h == from; });
    s.tiles = ring;
    s.ids = unitsOn(b, caster, ring, false);
    s.dice = 0, s.spellMul = 1;
    s.weaponBonusDice = n.dice, s.weaponBonusFaces = n.faces, s.weaponBonusBonus = 0;
    spend();
    steps.push_back(s);
  } else if (kind == "guardBreak") {
    // A weapon blow (+ the bonus dice) on a foe in reach that leaves its guard broken: DEF down for some of its turns.
    const int foe = occAt(b, cell);
    if (foe < 0 || !targetable(b, foe)) return fail("Escolha um inimigo visível.");
    if (!canHitFrom(b, caster, from, static_cast<size_t>(foe))) return fail("Esse inimigo não está ao alcance da arma.");
    spend();
    s.type = BattleStep::Type::Combat, s.def = static_cast<size_t>(foe);
    s.dice = 0, s.spellMul = 1;
    s.bonusDice = n.faces, s.bonusDiceCount = n.dice, s.bonusFlat = 0;
    s.sunderPct = n.pct, s.sunderTurns = n.turns;
    steps.push_back(s);
  } else if (kind == "warCry") {
    // Around the caster: every ally (itself too) rallied for some of its turns; from level 22 the foes in the radius are afraid.
    if (!(cell == from)) return fail("Esta habilidade não precisa de alvo.");
    const std::vector<BattleHex> area = hexesWithin(b, from, n.radius);
    s.type = BattleStep::Type::Effect;
    s.tiles = area;
    s.dice = 0, s.spellMul = 1;
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& t = b.units()[i];
      if (!t.alive) continue;
      const bool inside = std::any_of(area.begin(), area.end(), [&](const BattleHex& c) { return occupies(t, c.x, c.y); });
      if (!inside) continue;
      if (t.side == u.side) s.ids.push_back(i);
      else if (n.spread > 0 && t.side != "neutral") s.fearIds.push_back(i);
    }
    s.rallyPct = n.pct, s.rallyTurns = n.turns, s.fearTurns = n.spread;
    spend();
    steps.push_back(s);
  } else if (kind == "leapingStrike") {
    // A leap over everything onto a free hex beside the foe, then the blow (x mul, + the bonus dice; nobody strikes back);
    // from level 25 a shockwave: the landing's other neighbours take half a weapon roll each.
    const int foe = occAt(b, cell);
    if (foe < 0 || !targetable(b, foe)) return fail("Escolha um inimigo visível.");
    const std::optional<BattleHex> landing = leapLanding(b, caster, static_cast<size_t>(foe), n.range);
    if (!landing) return fail("Não há hex livre ao lado do alvo ao alcance do salto.");
    spend();
    if (!(*landing == from)) {
      BattleStep m;
      m.type = BattleStep::Type::Move, m.att = caster, m.kind = kind;
      m.path = {from, *landing};
      steps.push_back(m);
    }
    BattleStep c;
    c.type = BattleStep::Type::Combat, c.att = caster, c.def = static_cast<size_t>(foe), c.kind = kind, c.noCounter = true;
    c.dmgMul = n.mul;
    c.bonusDice = n.faces, c.bonusDiceCount = n.dice, c.bonusFlat = 0;
    steps.push_back(c);
    if (n.pct > 0) {
      std::vector<BattleHex> ring;
      for (const BattleHex& h : Battle::neighbors(landing->x, landing->y))
        if (b.inBounds(h.x, h.y)) ring.push_back(h);
      BattleStep w;
      w.type = BattleStep::Type::Spell, w.att = caster, w.kind = kind;
      w.tiles = ring;
      w.dice = 0, w.spellMul = 1;
      for (size_t id : unitsOn(b, caster, ring, false))
        if (id != static_cast<size_t>(foe)) w.ids.push_back(id), w.perTargetMul.push_back(n.pct / 100.0);
      if (!w.ids.empty()) steps.push_back(w);
    }
  } else if (kind == "spearToss") {
    // The lance under the neighbour: a lance blow (x mul + the dice); on a hit the body flies to the chosen hex, and against
    // whoever stands in the way: both take the collision dice.
    std::string why;
    const std::optional<TossPlan> p = tossPlan(b, caster, cell, n, &why);
    if (!p) return fail(why);
    spend();
    s.ids = {p->foe}, s.tiles = p->tiles;
    s.dice = 0, s.spellMul = 1, s.dmgMul = n.mul;
    s.weaponBonusDice = n.dice, s.weaponBonusFaces = n.faces, s.weaponBonusBonus = 0;
    s.throwTo = p->landing, s.arc = 1.4f + 0.25f * static_cast<float>(p->hexes);
    s.collideWith = p->collide, s.collideDice = n.tickDice, s.collideFaces = n.tickFaces;
    steps.push_back(s);
  } else if (kind == "hookPull") {
    // The hook on a foe 2+ hexes away: a lance blow (x mul + the dice); on a hit it is dragged to the free hex nearest the
    // caster on the line, off its feet (no counter until its next turn).
    std::string why;
    const std::optional<HookPlan> p = hookPlan(b, caster, cell, n, &why);
    if (!p) return fail(why);
    spend();
    s.ids = {p->foe}, s.tiles = p->line;
    s.dice = 0, s.spellMul = 1, s.dmgMul = n.mul;
    s.weaponBonusDice = n.dice, s.weaponBonusFaces = n.faces, s.weaponBonusBonus = 0;
    s.throwTo = p->dest, s.arc = 0.0f;
    s.stagger = true, s.staggerTurns = n.turns;
    steps.push_back(s);
  } else if (kind == "poleVault") {
    // Over the neighbour onto the hex behind it, then the blow from behind (x mul + the dice; the rear bonus is sure; no counter).
    std::string why;
    const int foe = occAt(b, cell);
    const std::optional<BattleHex> landing = vaultLanding(b, caster, cell, &why);
    if (!landing || foe < 0) return fail(why);
    spend();
    BattleStep m;
    m.type = BattleStep::Type::Move, m.att = caster, m.kind = kind;
    m.path = {from, *landing};
    steps.push_back(m);
    BattleStep c;
    c.type = BattleStep::Type::Combat, c.att = caster, c.def = static_cast<size_t>(foe), c.kind = kind, c.noCounter = true, c.rearSure = true;
    c.dmgMul = n.mul;
    c.bonusDice = n.faces, c.bonusDiceCount = n.dice, c.bonusFlat = 0;
    steps.push_back(c);
  } else if (kind == "disarm") {
    // A lance blow (+ the dice) on a foe in reach that knocks its weapon away: ATK down and no counter for some of its turns.
    const int foe = occAt(b, cell);
    if (foe < 0 || !targetable(b, foe)) return fail("Escolha um inimigo visível.");
    if (!canHitFrom(b, caster, from, static_cast<size_t>(foe))) return fail("Esse inimigo não está ao alcance da arma.");
    spend();
    s.type = BattleStep::Type::Combat, s.def = static_cast<size_t>(foe);
    s.dice = 0, s.spellMul = 1;
    s.bonusDice = n.faces, s.bonusDiceCount = n.dice, s.bonusFlat = 0;
    s.disarmPct = n.pct, s.disarmTurns = n.turns;
    steps.push_back(s);
  } else if (kind == "swapPost") {
    // Places changed with an ally in reach; then, from the new post, a lance blow (x mul + the dice) on the weakest foe the lance reaches.
    std::string why;
    const int ally = swapAlly(b, caster, cell, n, &why);
    if (ally < 0) return fail(why);
    spend();
    s.type = BattleStep::Type::Effect;
    s.ids = {static_cast<size_t>(ally)}, s.tiles = {cell, from};
    s.dice = 0, s.spellMul = 1;
    steps.push_back(s);
    // Beside the ally (one hex away) the swap is only a swap: no blow (the user, 2026-10-19: "troca de posto when you do that besides an
    // ally should not trigger an ATT"); from farther away the blow from the new post stays.
    int target = -1;
    for (size_t i = 0; i < b.units().size() && hexDist(from, cell) > 1; ++i) {
      if (!targetable(b, static_cast<int>(i)) || !canHitFrom(b, caster, cell, i)) continue;
      if (target < 0 || b.units()[i].hp < b.units()[static_cast<size_t>(target)].hp) target = static_cast<int>(i);
    }
    if (target >= 0) {
      BattleStep c;
      c.type = BattleStep::Type::Combat, c.att = caster, c.def = static_cast<size_t>(target), c.kind = kind;
      c.dmgMul = n.mul;
      c.bonusDice = n.faces, c.bonusDiceCount = n.dice, c.bonusFlat = 0;
      steps.push_back(c);
    }
  } else if (kind == "impaleDrag") {
    // The neighbour impaled and dragged in a straight line to the chosen hex: a lance blow x (mul + the share per hex) + the
    // dice; the caster runs one hex behind it.
    std::string why;
    const std::optional<DragPlan> p = dragPlan(b, caster, cell, n, &why);
    if (!p) return fail(why);
    spend();
    s.ids = {p->foe}, s.tiles = p->tiles;
    s.dice = 0, s.spellMul = 1, s.dmgMul = n.mul + n.decay * p->hexes;
    s.weaponBonusDice = n.dice, s.weaponBonusFaces = n.faces, s.weaponBonusBonus = 0;
    s.throwTo = p->foeTo, s.arc = 0.0f, s.dragTo = p->casterTo;
    steps.push_back(s);
  } else if (kind == "sentinelVigil") {
    // The stance: until its next turn, whoever attacks the caster or an ally within the radius takes its thrust first.
    if (!(cell == from)) return fail("Esta habilidade não precisa de alvo.");
    spend();
    s.type = BattleStep::Type::Effect;
    s.tiles = hexesWithin(b, from, n.radius);
    s.dice = 0, s.spellMul = 1;
    s.vigilPct = static_cast<int>(std::lround(n.mul * 100)), s.vigilRadius = n.radius;
    steps.push_back(s);
  } else if (archerShot(kind)) {
    // One arrow at a foe within the bow's reach (+ the skill's extra hexes), a clear line of fire; the bow's own roll.
    const int foe = occAt(b, cell);
    if (foe < 0 || !targetable(b, foe)) return fail("Escolha um inimigo visível.");
    const int d = hexDist(from, cell);
    if (d < u.minRange) return fail(std::format("Alvo perto demais (alcance mínimo {} hexes).", u.minRange));
    if (d > shotReach(u.maxRange, n.length)) return fail(std::format("Alvo fora de alcance (máximo {} hexes).", shotReach(u.maxRange, n.length)));
    if (!clearShot(b, from, cell, true)) return fail("O tiro está bloqueado.");
    s.dice = 0, s.spellMul = 1;
    s.tiles = {cell}, s.ids = {static_cast<size_t>(foe)};
    s.dmgMul = n.mul;
    s.weaponBonusDice = n.dice, s.weaponBonusFaces = n.faces, s.weaponBonusBonus = 0;
    if (kind == "venomArrow") s.poisonTier = n.tier;
    else if (kind == "cripplingShot") s.hobblePct = n.pct, s.hobbleTurns = n.turns;
    else if (kind == "deathShot") s.execPct = n.pct;
    else if (kind == "explosiveArrow") {  // the burst: the foes on the neighbouring hexes take a share of their own roll
      s.perTargetMul = {1.0};
      for (const BattleHex& h : Battle::neighbors(cell.x, cell.y)) {
        if (!b.inBounds(h.x, h.y)) continue;
        s.tiles.push_back(h);
        for (size_t id : unitsOn(b, caster, {h}, false))
          if (id != static_cast<size_t>(foe) && std::find(s.ids.begin(), s.ids.end(), id) == s.ids.end()) s.ids.push_back(id), s.perTargetMul.push_back(n.pct / 100.0);
      }
    }
    spend();
    steps.push_back(s);
  } else if (archerVolley(kind)) {
    // Arrows from above over an area round the aimed hex: every foe in it takes a share of the bow's roll (+ the dice).
    if (hexDist(from, cell) > n.range) return fail(std::format("Alvo fora de alcance (máximo {} hexes).", n.range));
    if (b.cell(cell.x, cell.y).terrain == "void") return fail("Não há chão nessa casa.");
    const std::vector<BattleHex> area = areaTiles(b, cell, n.radius);
    s.dice = 0, s.spellMul = 1;
    s.tiles = area;
    s.ids = unitsOn(b, caster, area, false);
    s.dmgMul = n.mul;
    s.weaponBonusDice = n.dice, s.weaponBonusFaces = n.faces, s.weaponBonusBonus = 0;
    spend();
    steps.push_back(s);
  } else {
    return fail("Esta habilidade ainda não existe aqui.");
  }
  return steps;
}
// The zone one of ours leaves where it landed (resolveStep: at the impact, with the caster's numbers then).
void layZone(Battle& b, const BattleStep& step, const Fighter& A) {
  const BattleUnit& att = b.units()[step.att];
  const EmberNumbers n = emberNumbers(step.kind, att.level);
  if (step.tiles.empty()) return;
  Battle::Zone z;
  z.kind = step.kind, z.caster = step.att, z.mag = A.mag;
  z.centre = step.kind == "eruption" ? BattleHex{att.x, att.y} : step.tiles.front();
  z.radius = n.radius, z.rounds = n.rounds;
  z.cells = areaTiles(b, z.centre, n.radius);  // its own area stage (3 hexes, then rings)
  z.element = elementOf(step.kind);
  if (step.kind == "pillarOfFlame" || step.kind == "primordialEmber") {
    z.kind = "burning", z.hazardDice = n.hazardDice, z.hazardFaces = n.hazardFaces;
  } else if (step.kind == "eruption") {
    z.kind = "burning", z.ring = true, z.hazardDice = n.hazardDice, z.hazardFaces = n.hazardFaces;
  } else if (step.kind == "shroudOfShadows") {
    z.statPct = n.pct, z.dice = n.tickDice, z.faces = n.tickFaces, z.mul = n.tickMul;
  } else if (step.kind == "meteor") {
    z.rounds = 99, z.centreMul = n.mul, z.centreDice = n.dice, z.centreFaces = n.faces, z.ringMul = n.ringMul, z.ringDice = n.ringDice, z.ringFaces = n.ringFaces;
  } else {
    return;
  }
  b.addZone(z);
}
bool leavesZone(const std::string& kind) {
  return kind == "pillarOfFlame" || kind == "primordialEmber" || kind == "eruption" || kind == "shroudOfShadows" || kind == "meteor";
}

#include "SpellsE2.inc"

}  // namespace

// --- Public ----------------------------------------------------------------------------------------------------------

std::string emberSpellClass(const std::string& kind) {
  const EmberSpell* e = emberSpell(kind);
  return e ? e->cls : "";
}
int spellLearnLevel(const std::string& kind) {
  const EmberSpell* e = emberSpell(kind);
  return e ? e->learn : 0;
}
std::vector<std::string> emberSpells(const std::string& classId, int level) {
  std::vector<std::string> out;
  const std::string family = emberClassFamily(classId);
  for (const EmberSpell& e : kEmberSpells)
    if ((classId == e.cls || family == e.cls) && level >= e.learn) out.push_back(e.data.kind);
  return out;
}
std::vector<std::string> checkSpellTracks(std::vector<std::string>* report) {
  const bool was = progressionEnabled();
  setProgressionEnabled(true);
  // what a cast reads of the spell at a level (its reach, area, dice, weights, counts, rounds ...)
  const auto numbers = [](const std::string& kind, int lv) {
    std::vector<double> v = {static_cast<double>(grownReach(kind, 0, lv)), static_cast<double>(areaRadius(kind, 0, lv))};
    const auto add = [&](std::initializer_list<double> x) { v.insert(v.end(), x); };
    if (emberSpell(kind)) {
      const EmberNumbers n = emberNumbers(kind, lv);
      add({double(n.range), double(n.radius), n.mul, double(n.dice), double(n.faces), double(n.bonus), double(n.pct), double(n.turns),
           double(n.rounds), double(n.spread), double(n.jumps), double(n.length), double(n.push), double(n.count), n.tickMul,
           double(n.tickDice), double(n.tickFaces), double(n.hazardDice), double(n.hazardFaces), n.ringMul, double(n.ringDice),
           double(n.ringFaces), double(n.durDice), double(n.durFaces), n.decay});
      v.push_back(static_cast<double>(std::hash<std::string>{}(n.tier) % 1000003));  // the poison tier, one number
    } else if (kind == "magicMissile") {
      add({double(magicMissileCount(lv)), missileMul(lv)});
    } else if (kind == "lightning") {
      add({double(lightningDice(lv)), lightningMul(lv)});
    } else if (kind == "frost") {
      const FrostPower p = frostPower(lv);
      add({double(p.length), double(p.dice), double(p.faces), p.mul});
    } else if (kind == "fireball") {
      add({double(fireballDice(lv))});
    } else if (kind == "causticVenom" || kind == "minorVenom" || kind == "divineBolt") {
      const BoltData d = boltPower(kind, lv);
      add({double(d.centerDice), d.centerMul, double(d.splashDice), d.splashMul});
    } else if (kind == "lightningTier3") {
      const TrackGains g = spellGains(kind, lv);
      add({double(kLightningT3Dice + g.n('d')), kLightningT3Mul + g('m')});
    } else if (const CureData* c = cureData(kind)) {
      const TrackGains g = spellGains(kind, lv);
      add({double(c->dice + g.n('d')), c->mul + g('m')});
    } else if (kind == "burningHands" || kind == "poisonBreath") {
      const ConeP p = kind == "burningHands" ? burningHandsPower(lv) : poisonBreathPower(lv);
      add({double(p.radius), double(p.dice), double(p.faces), p.mul});  // the cone's depth (its range is the same number)
    } else if (kind == "turnUndead") {
      const UndeadP p = turnUndeadPower(lv);
      add({double(p.dice), double(p.faces), p.mul});
    } else if (kind == "bless") {
      add({double(blessHitBonusPct(lv)), double(blessDuration(lv))});
    } else if (kind == "auraOfProtection" || kind == "intimidatingPresence") {
      const AuraP p = auraPower(kind, lv);
      add({double(p.radius), p.pct, double(p.duration)});
    } else if (kind == "divineWrath") {
      const DiceFaces p = divineWrathPower(lv);
      add({double(p.dice), double(p.faces)});
    } else if (kind == "phantasmalForce") {
      const DiceFaces p = phantasmalForceDice(lv);
      add({double(p.dice), double(p.faces), 1 + spellGains(kind, lv)('m')});
    } else if (kind == "webOfDreams") {
      add({webOfDreamsSleepChance(lv), double(webOfDreamsSize(lv))});
    } else if (kind == "longShot") {
      const Dice p = longShotPower(lv);
      add({double(p.dice), double(p.faces), double(spellGains(kind, lv).n('b'))});
    } else if (kind == "bloodyShot") {
      const DiceFaces p = bloodyShotBleed(lv);
      add({bloodyShotMul(lv), double(p.dice), double(p.faces)});
    } else if (kind == "multiShot") {
      const DiceFaces p = multiShotPower(lv);
      add({double(multiShotTargets(lv)), double(p.dice), double(p.faces)});
    }
    return v;
  };
  std::vector<std::string> fails;
  constexpr int kLastLevel = 60;  // Ember Ashes II's cap
  for (const SpellTrack& t : kSpellTracks) {
    const ParsedTrack* p = parsedTrack(t.kind);
    std::vector<double> prev = numbers(t.kind, t.learn);
    std::string line = std::format("{} (aprendida no {}):", t.kind, t.learn);
    int steps = 0;
    for (int lv = t.learn + 1; lv <= kLastLevel; ++lv) {
      const std::vector<double> now = numbers(t.kind, lv);
      int changed = 0;
      for (size_t i = 0; i < now.size(); ++i)
        if (std::abs(now[i] - prev[i]) > 1e-9) ++changed;
      const size_t at = static_cast<size_t>(lv - t.learn - 1);
      const bool step = at < p->steps.size() && p->steps[at].first != '-';
      // a ring of area moves two numbers (its stage and the spell's radius); a dice-only spell's reach / area keep one each
      if (step && changed == 0) fails.push_back(std::format("{}: nível {} devia melhorar e não mudou nada", t.kind, lv));
      if (!step && changed > 0) fails.push_back(std::format("{}: nível {} mudou {} números sem passo na trilha", t.kind, lv, changed));
      if (step && changed > 1 && !(p->steps[at].first == 'a' && changed == 2) && !(p->steps[at].first == 'r' && changed == 2))
        fails.push_back(std::format("{}: nível {} mudou {} números de uma vez", t.kind, lv, changed));
      if (step) {
        ++steps;
        line += std::format(" {}={}", lv, p->steps[at].first);
      }
      prev = now;
    }
    if (p->steps.size() != 14) fails.push_back(std::format("{}: trilha com {} passos (devem ser 14)", t.kind, p->steps.size()));
    if (report) {
      report->push_back(line + std::format(" ({} melhorias)", steps));
      BattleUnit u;  // the status sheet's line at the learning level, half way and at the end (MAG 40)
      u.mag = 40;
      for (const int lv : {t.learn, t.learn + 7, t.learn + 14})
        if (u.level = lv; !spellScaleText(u, t.kind, false).empty()) report->push_back(std::format("    nível {}: {}", lv, spellScaleText(u, t.kind, false)));
    }
  }
  setProgressionEnabled(was);
  return fails;
}
void grantClassSpells(BattleUnit& u) {
  for (const std::string& k : emberSpells(u.classId, u.level))
    if (!contains(u.spellList, k)) u.spellList.push_back(k);
  if (progressionEnabled() && u.side == "player") {
    const bool necromancer = u.classId == "necromancer" && u.level >= 15;
    if (!necromancer) std::erase(u.spellList, std::string("summonZombieDog"));
    else if (!contains(u.spellList, std::string("summonZombieDog"))) u.spellList.push_back("summonZombieDog");
  }
}
std::string promotionName(const std::string& classId) {
  if (classId == "warlock") return "Bruxo";
  if (classId == "elementalist") return "Elementalista";
  if (classId == "mage" || classId == "voss" || classId.empty()) return "Mago Negro";
  if (classId == "swordsman") return "Guerreiro";
  if (classId == "paladin") return "Paladino";
  if (classId == "heavyKnight") return "Cavaleiro Pesado";
  if (classId == "archer") return "Arqueira";  // Neera, the only one
  if (classId == "ranger") return "Patrulheira";
  if (classId == "assassin") return "Assassina";
  if (classId == "aldric" || classId == "lancer") return "Lanceiro";
  if (classId == "sentinel") return "Sentinela";
  if (classId == "templar") return "Templário";
  if (classId == "healer" || classId == "salazar") return "Curandeiro";
  if (classId == "cleric") return "Clérigo";
  if (classId == "bishop") return "Bispo";
  if (classId == "conjurer") return "Conjurador";
  if (classId == "sorcerer") return "Arcanista";
  if (classId == "necromancer") return "Necromante";
  return classId;
}
void setPromotion(BattleUnit& u, const std::string& classId) {
  u.classId = classId.empty() ? "mage" : classId;
  u.className = promotionName(u.classId);
  std::erase_if(u.spellList, [](const std::string& k) { return emberSpell(k) != nullptr; });
  grantClassSpells(u);
}
std::string conditionsText(const BattleUnit& u) {
  std::string s;
  auto add = [&](const std::string& part) { s += (s.empty() ? "" : " · ") + part; };
  if (u.cond.markTurns > 0) add(std::format("Marcado +{}% ({})", u.cond.markPct, u.cond.markTurns));
  if (u.poisoned)
    if (const PoisonTier* t = poisonTier(u.poisonTier)) add(std::string(t->name) + (u.cond.plagueSpread > 0 ? " (espalha)" : ""));
  if (u.cond.bleedTurns > 0) add(std::format("Sangrando {}D{} ({})", u.cond.bleedDice, u.cond.bleedFaces, u.cond.bleedTurns));
  if (u.cond.silenceTurns > 0) add(std::format("Silenciado ({})", u.cond.silenceTurns));
  if (u.cond.decayTurns > 0) add(std::format("Decadência -{}% ({})", u.cond.decayPct, u.cond.decayTurns));
  if (u.cond.sunderPct > 0) add(std::format("{} -{}% DEF ({})", pick("Guarda quebrada", "Guard broken"), u.cond.sunderPct, u.cond.sunderTurns + 1));
  if (u.cond.rallyPct > 0) add(std::format("{} +{}% ATK ({})", pick("Grito de Guerra", "War Cry"), u.cond.rallyPct, u.cond.rallyTurns + 1));
  if (u.cond.hobblePct > 0) add(std::format("{} -{}% MOV ({})", pick("Paralisado", "Crippled"), u.cond.hobblePct, u.cond.hobbleTurns + 1));
  if (u.cond.disarmPct > 0) add(std::format("{} -{}% ATK ({})", pick("Desarmado", "Disarmed"), u.cond.disarmPct, u.cond.disarmTurns + 1));
  if (u.cond.staggered) add(std::format("{} ({})", pick("Desequilibrado", "Off balance"), u.cond.staggerTurns + 1));
  if (u.cond.vigilPct > 0) add(std::format("{} ×{:g} · {} {}", pick("Vigília", "Vigil"), u.cond.vigilPct / 100.0, pick("raio", "radius"), u.cond.vigilRadius));
  if (u.cond.mirrorCopies > 0) add(std::format("{} ×{} ({})", pick("Imagem Espelhada", "Mirror Image"), u.cond.mirrorCopies, u.cond.mirrorRounds));
  if (u.cond.bondPct > 0) add(std::format("{} {}%", pick("Elo de Sacrifício", "Sacrifice Bond"), u.cond.bondPct));
  if (u.cond.empowerPct > 0) add(std::format("{} +{}% ({})", pick("Fortalecido", "Empowered"), u.cond.empowerPct, u.cond.empowerRounds));
  if (u.diseased) add("Doente");
  // Engine2's statuses
  if (u.stunned) add(std::string(pick("Atordoado", "Stunned")) + std::format(" ({})", u.stunTurns));
  if (u.asleep) add(std::string(pick("Adormecido", "Asleep")) + std::format(" ({})", u.sleepTurns));
  if (u.bleeding) add(u.bleedRoundsLeft ? std::string(pick("Sangrando 1D8 por ação", "Bleeding 1D8 per action")) + std::format(" ({})", *u.bleedRoundsLeft)
                                         : std::string(pick("Sangrando 1D8 por ação", "Bleeding 1D8 per action")));
  if (u.crippled) add(pick("Aleijado -10%", "Crippled -10%"));
  if (u.fearTurns > 0) add(std::string(pick("Com medo", "Afraid")) + std::format(" ({})", u.fearTurns));
  if (u.blessedRoundsLeft > 0) add(std::string(pick("Abençoado", "Blessed")) + std::format(" +{}% ({})", static_cast<int>(jsRound(u.blessedHitBonusPct * 100)), u.blessedRoundsLeft));
  return s;
}
std::string spellScaleTextRaw(const BattleUnit& c, const std::string& kind, bool en);
std::string spellScaleText(const BattleUnit& c, const std::string& kind, bool en) {
  std::string s = spellScaleTextRaw(c, kind, en);
  // one of a thing reads singular ("1 míssil", "cone de 1 hex")
  for (const auto& [many, one] : {std::pair<const char*, const char*>{" 1 mísseis", " 1 míssil"}, {" 1 missiles", " 1 missile"}, {" 1 hexes", " 1 hex"},
                                  {" 1 rodadas", " 1 rodada"}, {" 1 rounds", " 1 round"}})
    for (size_t at = s.find(many); at != std::string::npos; at = s.find(many, at)) s.replace(at, std::strlen(many), one);
  return s;
}
std::string spellScaleTextRaw(const BattleUnit& c, const std::string& kind, bool en) {
  const int lv = c.level;
  const int half = static_cast<int>(std::floor(c.mag / 2.0));
  const auto pick = [&](const char* pt, const char* e) { return std::string(en ? e : pt); };
  const auto dmg = [&](double mul, int dice, int faces, int bonus = 0) {
    std::string r = std::format("{} + {}D{}", static_cast<int>(std::floor(half * mul)), dice, faces);
    return bonus ? r + std::format("+{}", bonus) : r;
  };
  const auto reach = [&](int base) { return std::format("{} {}", pick("alcance", "range"), grownReach(kind, base, lv)); };
  const auto area = [&](int base) {
    const int st = areaRadius(kind, base, lv);
    return st <= 0 ? std::string("3 hexes") : std::format("{} {}", pick("raio", "radius"), st);
  };
  const auto num = [&](double v) {
    std::string t = std::format("{:.2f}", v);
    while (t.back() == '0') t.pop_back();
    if (t.back() == '.') t.pop_back();
    if (!en) std::replace(t.begin(), t.end(), '.', ',');
    return t;
  };
  const std::string damage = pick("dano", "damage");
  if (kind == "magicMissile")
    return std::format("{} · {} {} · {} {}", reach(kMissileRange), magicMissileCount(lv), pick("mísseis", "missiles"), pick("cada um", "each"), dmg(missileMul(lv), kMissileDice, kMissileFaces));
  if (kind == "lightning") return std::format("{} · {} {}", reach(kLightningRange), damage, dmg(lightningMul(lv), lightningDice(lv), kLightningFaces));
  if (kind == "lightningTier3") {
    const TrackGains g = spellGains(kind, lv);
    return std::format("{} · {} {}", reach(kLightningT3Range), damage, dmg(kLightningT3Mul + g('m'), kLightningT3Dice + g.n('d'), kLightningT3Faces, kLightningT3Bonus));
  }
  if (kind == "frost") {
    const FrostPower p = frostPower(lv);
    return std::format("{} {} hexes · {} {}", pick("linha de", "line of"), p.length, damage, dmg(p.mul, p.dice, p.faces));
  }
  if (kind == "burningHands" || kind == "poisonBreath") {
    const ConeP p = kind == "burningHands" ? burningHandsPower(lv) : poisonBreathPower(lv);
    return std::format("{} {} hexes · {} {}", pick("cone de", "cone of"), p.radius, damage, dmg(p.mul, p.dice, p.faces));
  }
  if (kind == "fireball") return std::format("{} · {} · {} {}", reach(kFireballRange), area(kFireballSize), damage, dmg(kFireballMul, fireballDice(lv), kFireballFaces));
  if (kind == "causticVenom" || kind == "minorVenom" || kind == "divineBolt") {
    const BoltData d = boltPower(kind, lv);
    return std::format("{} · {} · {} {} · {} {}", reach(d.range), area(d.size), pick("centro", "centre"), dmg(d.centerMul, d.centerDice, d.centerFaces),
                       pick("em volta", "around"), dmg(d.splashMul, d.splashDice, d.splashFaces));
  }
  if (const CureData* cure = cureData(kind)) {
    const TrackGains g = spellGains(kind, lv);
    return std::format("{} · {} {}", reach(cure->range), pick("cura", "heals"), dmg(cure->mul + g('m'), cure->dice + g.n('d'), cure->faces));
  }
  if (kind == "cureDisease") return std::format("{} · {}", reach(kCureDiseaseRange), pick("remove doença e veneno", "removes disease and poison"));
  if (kind == "bless")
    return std::format("{} {} · +{}% {} · {} {}", area(kBlessRadius), pick("em volta", "around"), blessHitBonusPct(lv), pick("acerto", "hit"), blessDuration(lv), pick("rodadas", "rounds"));
  if (kind == "turnUndead") {
    const UndeadP p = turnUndeadPower(lv);
    return std::format("{} {} · {} {} {}", area(3), pick("em volta", "around"), damage, dmg(p.mul, p.dice, p.faces), pick("nos mortos-vivos", "to the undead"));
  }
  if (kind == "auraOfProtection" || kind == "intimidatingPresence") {
    const AuraP p = auraPower(kind, lv);
    return std::format("{} {} · {}% · {} {}", area(p.radius), pick("em volta", "around"), static_cast<int>(jsRound(p.pct * 100)), p.duration, pick("rodadas", "rounds"));
  }
  if (kind == "divineWrath") {
    const DiceFaces p = divineWrathPower(lv);
    return std::format("{} {} hexes · {} {}D{} + {}", pick("linha de", "line of"), grownReach(kind, kWrathRange, lv), damage, p.dice, p.faces, half);
  }
  if (kind == "phantasmalForce") {
    const DiceFaces p = phantasmalForceDice(lv);
    return std::format("{} · {} {}", reach(kPhantasmalRange), damage, dmg(1 + spellGains(kind, lv)('m'), p.dice, p.faces));
  }
  if (kind == "webOfDreams")
    return std::format("{} · {} · {} {}%", reach(kWebRange), area(1), pick("sono", "sleep"), static_cast<int>(jsRound(webOfDreamsSleepChance(lv) * 100)));
  if (kind == "longShot") {
    const Dice p = longShotPower(lv);
    const int bonus = spellGains(kind, lv).n('b');
    return std::format("{} · {} + {}D{}{}", reach(kLongShotRange), pick("arma", "weapon"), p.dice, p.faces, bonus ? std::format("+{}", bonus) : "");
  }
  if (kind == "bloodyShot") {
    const DiceFaces p = bloodyShotBleed(lv);
    return std::format("{} · ×{} {} · {} {}D{} {}", reach(kBloodyShotRange), num(bloodyShotMul(lv)), pick("da arma", "weapon"), pick("sangra", "bleeds"), p.dice, p.faces, pick("rodadas", "rounds"));
  }
  if (kind == "multiShot") {
    const DiceFaces p = multiShotPower(lv);
    return std::format("{} · {} {} · {}{}", reach(kMultiShotRange), multiShotTargets(lv), pick("flechas", "arrows"), pick("arma", "weapon"),
                       p.dice > 0 ? std::format(" + {}D{}", p.dice, p.faces) : "");
  }
  return {};
}
std::string spellTooltip(const BattleUnit& c, const std::string& kind) {
  const EmberSpell* e = emberSpell(kind);
  if (!e) return {};
  const EmberNumbers n = emberNumbers(kind, c.level);
  const int half = static_cast<int>(std::floor(c.mag / 2.0));
  auto dmg = [&](double mul, int dice, int faces) { return std::format("{} + {}D{}", static_cast<int>(std::floor(half * mul)), dice, faces); };
  static const std::map<std::string, const char*> kElements = {{"darkness", "Escuridão"}, {"poison", "Veneno"}, {"fire", "Fogo"}, {"ice", "Gelo"},
                                                               {"lightning", "Relâmpago"}, {"ember", "Brasa"}, {"holy", "Sagrado"}};
  const std::string element = e->data.element;
  std::string s = std::format("T{} · {}", e->data.tier, kElements.contains(element) ? std::string(kElements.at(element)) : element.empty() ? std::string("Arma") : element);
  if (n.range > 0 && kind != "leapingStrike") s += std::format(" · alcance {}", n.range);
  // The Guerreiro's (the weapon's own roll, hit chance and mastery, plus the dice)
  if (kind == "steelWhirlwind") return s + std::format("\ngira uma vez: golpe de arma + {}D{} em todo inimigo vizinho · sem contra-ataque", n.dice, n.faces);
  if (kind == "guardBreak") return s + std::format("\ngolpe de arma + {}D{} · Guarda quebrada: -{}% DEF por {} turnos", n.dice, n.faces, n.pct, n.turns);
  if (kind == "warCry") return s + std::format("\nem volta de si, raio {} · aliados +{}% ATK por {} turnos{}", n.radius, n.pct, n.turns, n.spread > 0 ? " · inimigos no raio com medo 1 turno" : "");
  if (kind == "leapingStrike")
    return s + std::format("\nsalta até {} hexes para junto do inimigo · ×{:g} da arma + {}D{} · sem contra-ataque{}", n.range, n.mul, n.dice, n.faces,
                           n.pct > 0 ? std::format(" · onda de choque: os outros inimigos vizinhos do pouso levam {}%", n.pct) : "");
  // The Arqueiro's (the bow's own roll, hit chance and mastery, plus the dice)
  if (archerShot(kind)) s += std::format(" · alcance do arco{}", n.length > 0 ? std::format(" + {}", n.length) : "");
  if (kind == "venomArrow") return s + std::format("\ntiro de arco + {}D{} · {}", n.dice, n.faces, poisonTier(n.tier)->name);
  if (kind == "arrowRain") return s + std::format("\nalcance {} · {} · ×{:g} do arco + {}D{} em cada inimigo · sem linha de tiro", n.range, areaLabel(n.radius), n.mul, n.dice, n.faces);
  if (kind == "explosiveArrow") return s + std::format("\n×{:g} do arco + {}D{} · os inimigos vizinhos do alvo levam {}%", n.mul, n.dice, n.faces, n.pct);
  if (kind == "cripplingShot") return s + std::format("\ntiro de arco + {}D{} · Paralisado: -{}% MOV por {} turnos", n.dice, n.faces, n.pct, n.turns);
  if (kind == "arrowStorm") return s + std::format("\nalcance {} · {} · ×{:g} do arco + {}D{} em cada inimigo · sem linha de tiro", n.range, areaLabel(n.radius), n.mul, n.dice, n.faces);
  if (kind == "deathShot") return s + std::format("\n×{:g} do arco + {}D{} · abaixo de {}% do HP, morre", n.mul, n.dice, n.faces, n.pct);
  // The Lanceiro's (the lance's own roll, hit chance and mastery, plus the dice)
  auto bonus = [&] { return n.dice > 0 ? std::format(" + {}D{}", n.dice, n.faces) : std::string(); };
  if (kind == "spearToss")
    return s + std::format("\nergue o inimigo vizinho com a lança e o lança {} hexes na direção escolhida · ×{:g} da lança{} · se cair sobre outra unidade, as duas levam {}D{} de colisão",
                           n.range, n.mul, bonus(), n.tickDice, n.tickFaces);
  if (kind == "hookPull")
    return s + std::format("\nengancha um inimigo a 2–{} hexes e o puxa para junto de si · ×{:g} da lança{} · Desequilibrado: não contra-ataca até o próximo turno{}",
                           n.range, n.mul, bonus(), n.turns > 0 ? " e mais um" : "");
  if (kind == "poleVault")
    return s + std::format("\nsalta por cima do inimigo vizinho e cai nas costas dele · golpe pela retaguarda garantido · ×{:g} da lança{} · sem contra-ataque", n.mul, bonus());
  if (kind == "disarm") return s + std::format("\ngolpe de lança{} · Desarmado: -{}% ATK e sem contra-ataque por {} turnos", bonus(), n.pct, n.turns);
  if (kind == "swapPost")
    return s + std::format("\ntroca de lugar com um aliado a até {} hexes · depois golpeia um inimigo ao alcance do novo posto · ×{:g} da lança{}", n.range, n.mul, bonus());
  if (kind == "impaleDrag")
    return s + std::format("\nempala o inimigo vizinho e o arrasta até {} hexes · ×{:g} da lança +{:g} por hex arrastado{}", n.length, n.mul, n.decay, bonus());
  if (kind == "sentinelVigil")
    return s + std::format("\npostura até o próximo turno · quem atacar você ou um aliado a até {} hex{} leva a sua estocada antes · ×{:g} da lança{}", n.radius,
                           n.radius > 1 ? "es" : "", n.mul, bonus());
  // The Conjurador's
  if (kind == "placeSwap") return s + "\ntroca de lugar com um aliado ou uma invocação ao alcance";
  if (kind == "boneLance") return s + std::format("\nreta de {} hexes a partir de si · {} em todos na reta (os dois lados)", n.length, dmg(n.mul, n.dice, n.faces));
  if (kind == "mirrorImage") return s + std::format("\n{} cópias ilusórias em um aliado por {} rodadas · cada ataque tem 1 chance em 3 de acertar uma cópia, que some", n.count, n.rounds);
  if (kind == "detonateSummon") return s + std::format("\nexplode uma invocação sua · todos a até {} hex{} dela levam {} · a invocação some", n.radius, n.radius > 1 ? "es" : "", dmg(n.mul, n.dice, n.faces));
  if (kind == "sacrificeBond") return s + std::format("\naté o fim da batalha, {}% do dano que você recebe vai para a sua invocação mais próxima", n.pct);
  if (kind == "nightmare") return s + std::format("\n{} · {} em cada inimigo · quem dorme leva o dobro · {}% de chance de medo por {} turno{}", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.pct, n.turns, n.turns > 1 ? "s" : "");
  if (kind == "empowerSummons") return s + std::format("\ntodas as suas invocações: +{}% em todos os atributos por {} rodadas", n.pct, n.rounds);
  // The Curandeiro's (the cures' formula; the healing skill adds +1 % a point)
  if (kind == "circleOfHealing") return s + std::format("\n{} · cada aliado na área cura {} HP", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces));
  if (kind == "resurrection") return s + std::format("\num aliado caído ao lado volta com {} HP · age no seu próximo turno", dmg(n.mul, n.dice, n.faces));
  if (kind == "judgment") return s + std::format("\n{} · {} em cada inimigo · mortos-vivos ×{:g}", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.tickMul);
  if (kind == "markOfRuin") s += std::format("\n{} · o alvo recebe +{}% de dano por {} turnos", dmg(n.mul, n.dice, n.faces), n.pct, n.turns);
  else if (kind == "creepingPlague") s += std::format("\n{} · {} que passa a {} vizinho{} por turno", dmg(n.mul, n.dice, n.faces), poisonTier(n.tier)->name, n.spread, n.spread > 1 ? "s" : "");
  else if (kind == "lifeDrain") s += std::format("\n{} · cura {}% do dano causado", dmg(n.mul, n.dice, n.faces), n.pct);
  else if (kind == "shroudOfShadows") s += std::format("\nzona {} por {} rodadas · -{}% ATK e MAG · {} por turno a quem está dentro", areaLabel(n.radius), n.rounds, n.pct, dmg(n.tickMul, n.tickDice, n.tickFaces));
  else if (kind == "hemorrhage") s += std::format("\n{} · sangra {}D{} por turno durante {}D{} turnos · curas pela metade", dmg(n.mul, n.dice, n.faces), n.tickDice, n.tickFaces, n.durDice, n.durFaces);
  else if (kind == "profaneSilence") s += std::format("\n{} · {} · silenciados por {} turnos, -1 uso do maior tier", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.turns);
  else if (kind == "blackPestilence") s += std::format("\n{} (os dois lados) · {} · {} · quem morre envenenado contagia os vizinhos", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), poisonTier(n.tier)->name);
  else if (kind == "curseOfDecay") s += std::format("\n{} · -{}% de todos os atributos por turno até -{}% · {} por turno · {} turnos", dmg(n.mul, n.dice, n.faces), n.spread, n.pct, dmg(n.tickMul, n.tickDice, n.tickFaces), n.turns);
  else if (kind == "soulHarvest") s += std::format("\n{} · {} em cada inimigo · venenos, sangrias e maldições batem mais uma vez · cura {}% do HP por inimigo", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.pct);
  else if (kind == "devouringVoid") s += std::format("\n{} · -{} de resistência a Escuridão pelo resto da batalha · abaixo de {}% do HP, morre", dmg(n.mul, n.dice, n.faces), n.spread, n.pct);
  else if (kind == "pillarOfFlame") s += std::format("\n{} · {} · o chão arde {}D{} por {} rodada{}", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.hazardDice, n.hazardFaces, n.rounds, n.rounds > 1 ? "s" : "");
  else if (kind == "iceLance") s += std::format("\nreta até a borda · {} · os seguintes recebem {}%", dmg(n.mul, n.dice, n.faces), n.pct);
  else if (kind == "thunderstorm") s += std::format("\n{} · {} · cada atingido ecoa {}D{}+{} num vizinho", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.tickDice, n.tickFaces, n.bonus);
  else if (kind == "meteor") s += std::format("\n{} · cai no início do seu próximo turno · centro {} · anel {}", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), dmg(n.ringMul, n.ringDice, n.ringFaces));
  else if (kind == "absoluteZero") s += std::format("\n{} · {} · ×1.5 dentro de uma Tempestade de Gelo", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces));
  else if (kind == "chainLightning") s += std::format("\n{} · salta até {} vezes (a {} hexes), -{}% por salto", dmg(n.mul, n.dice, n.faces), n.jumps, n.length, static_cast<int>(n.decay * 100));
  else if (kind == "eruption") s += std::format("\nem volta de si, {} · {} em cada inimigo · o anel arde {} rodadas", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.rounds);
  else if (kind == "glacialGale") s += std::format("\ncone de {} hexes (os dois lados) · {} · empurra {} hex", n.length, dmg(n.mul, n.dice, n.faces), n.push);
  else if (kind == "wrathOfTheSkies") s += std::format("\n{} raios à escolha · {} cada · eco 2D12+8", n.count, dmg(n.mul, n.dice, n.faces));
  else if (kind == "primordialEmber") s += std::format("\n{} (os dois lados) · {} · o chão fica em brasa {}D{} por {} rodadas", areaLabel(n.radius), dmg(n.mul, n.dice, n.faces), n.hazardDice, n.hazardFaces, n.rounds);
  return s;
}

std::string spellName(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s ? s->name : kind;
}

namespace {
// The English names, for the combat log in English (Engine2's own names for its spells, ours for the promotions).
constexpr std::pair<const char*, const char*> kSpellNamesEnglish[] = {
    {"magicMissile", "Magic Missile"}, {"magicMissileV2", "Magic Missile"}, {"longShot", "Long Shot"}, {"piercing", "Piercing Shot"},
    {"piercingThrust", "Piercing Thrust"}, {"doubleStrike", "Double Strike"}, {"lightning", "Lightning"}, {"frost", "Frost"},
    {"shock", "Shock"}, {"cureMinor", "Cure Minor"}, {"cureWounds", "Cure Wounds"}, {"cureLight", "Healing Hands"},
    {"cureDisease", "Cure Disease"}, {"createFoodAndWater", "Create Food and Water"}, {"fantomForce", "Phantom Force"},
    {"bloodyShot", "Bloody Shot"}, {"multiShot", "Multi Shot"}, {"provoke", "Provoke"}, {"cleave", "Cleave"}, {"sweep", "Sweep"},
    {"trip", "Trip"}, {"bullRush", "Bull Rush"}, {"shieldBash", "Shield Bash"}, {"executionerStrike", "Executioner's Strike"},
    {"fireball", "Fireball"}, {"iceStorm", "Ice Storm"}, {"causticVenom", "Caustic Venom"}, {"poisonBreath", "Poison Breath"},
    {"warp", "Warp"}, {"bless", "Bless"}, {"burningHands", "Burning Hands"}, {"turnUndead", "Turn Undead"},
    {"divineBolt", "Divine Bolt"}, {"summonFamiliar", "Summon Familiar"}, {"phantasmalForce", "Phantasmal Force"},
    {"webOfDreams", "Web of Dreams"}, {"summonFamiliar2", "Greater Familiar"}, {"summonFamiliar3", "Familiar 3"},
    {"summonFamiliar4", "Radiant Familiar"}, {"summonZombieDog", "Zombie Dog"}, {"lightningTier3", "Lightning 3"},
    {"minorVenom", "Minor Venom"}, {"familiarLifeDrain", "Life Drain"}, {"divineWrath", "Divine Wrath"},
    {"shoulderSmash", "Shoulder Smash"}, {"stampede", "Stampede"}, {"auraOfProtection", "Aura of Protection"},
    {"intimidatingPresence", "Intimidating Presence"}, {"secondWind", "Second Wind"},
    {"markOfRuin", "Mark of Ruin"}, {"creepingPlague", "Creeping Plague"}, {"lifeDrain", "Life Drain"},
    {"shroudOfShadows", "Shroud of Shadows"}, {"hemorrhage", "Hemorrhage"}, {"profaneSilence", "Profane Silence"},
    {"blackPestilence", "Black Pestilence"}, {"curseOfDecay", "Curse of Decay"}, {"soulHarvest", "Soul Harvest"},
    {"devouringVoid", "Devouring Void"}, {"pillarOfFlame", "Pillar of Flame"}, {"iceLance", "Ice Lance"},
    {"thunderstorm", "Thunderstorm"}, {"meteor", "Meteor"}, {"absoluteZero", "Absolute Zero"}, {"chainLightning", "Chain Lightning"},
    {"eruption", "Eruption"}, {"glacialGale", "Glacial Gale"}, {"wrathOfTheSkies", "Wrath of the Skies"},
    {"primordialEmber", "Primordial Ember"},
    {"steelWhirlwind", "Steel Whirlwind"}, {"guardBreak", "Guard Break"}, {"warCry", "War Cry"}, {"leapingStrike", "Devastating Leap"},
    {"venomArrow", "Venom Arrow"}, {"arrowRain", "Arrow Rain"}, {"explosiveArrow", "Explosive Arrow"}, {"cripplingShot", "Crippling Shot"},
    {"arrowStorm", "Arrow Storm"}, {"deathShot", "Death Shot"},
    {"spearToss", "Spear Toss"}, {"hookPull", "Hook"}, {"poleVault", "Pole Vault"}, {"disarm", "Disarm"}, {"swapPost", "Change of Post"},
    {"impaleDrag", "Impale and Drag"}, {"sentinelVigil", "Sentinel's Vigil"}, {"sentinelStrike", "Sentinel's Thrust"},
    {"circleOfHealing", "Circle of Healing"}, {"resurrection", "Resurrection"}, {"judgment", "Judgment"},
    {"placeSwap", "Place Swap"}, {"boneLance", "Bone Lance"}, {"mirrorImage", "Mirror Image"}, {"detonateSummon", "Detonate Summon"},
    {"sacrificeBond", "Sacrifice Bond"}, {"nightmare", "Nightmare"}, {"empowerSummons", "Empower Summons"},
};
}  // namespace

std::string spellNameIn(const std::string& kind, bool english) {
  if (english)
    for (const auto& [k, en] : kSpellNamesEnglish)
      if (kind == k) return en;
  return spellName(kind);
}

bool spellPorted(const std::string& kind) {
  return kind == "magicMissile" || kind == "longShot" || kind == "piercing" || kind == "piercingThrust" || kind == "doubleStrike" || kind == "lightning" ||
         kind == "frost" || kind == "cureMinor" || kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "createFoodAndWater" ||
         emberSpell(kind) != nullptr || e2Kind(kind);
}

bool spellWeaponAbility(const std::string& kind) { return weaponAbility(kind); }

bool spellInstant(const std::string& kind) {
  return kind == "createFoodAndWater" || kind == "auraOfProtection" || kind == "intimidatingPresence" || kind == "sacrificeBond" ||
         kind == "empowerSummons";
}
// An area around the caster, shown at once and cast on Confirmar. Grito de Guerra, Vigília da Sentinela and Erupção were instant (cast the
// moment the socket was pressed, the area never shown): the user, 2026-10-19: "how the fuck im going to cast something without knowing the
// area of effect".
bool spellSelfArea(const std::string& kind) {
  return kind == "sweep" || kind == "turnUndead" || kind == "steelWhirlwind" || kind == "warCry" || kind == "sentinelVigil" || kind == "eruption";
}
bool spellAimsDirection(const std::string& kind) {
  return kind == "piercing" || kind == "piercingThrust" || kind == "frost" || kind == "iceLance" || kind == "glacialGale" ||
         kind == "burningHands" || kind == "poisonBreath" || kind == "divineWrath" || kind == "stampede" || kind == "boneLance";
}

int potionIndex(const std::string& id) {
  for (int i = 0; i < 7; ++i)
    if (id == kPotionIds[i]) return i;
  return -1;
}

std::string potionName(const std::string& id) {
  const PotionData* p = potionData(id);
  if (!p) return id;
  return p->dice > 0 ? std::format("{} {}D{}{}", p->name, p->dice, p->faces, p->bonus ? std::format("+{}", p->bonus) : "") : p->name;  // potionLabel
}

bool potionTargetValid(const Battle& b, size_t actor, BattleHex cell, std::string* error) {
  const BattleUnit& a = b.units()[actor];
  auto fail = [&](const char* why) {
    if (error) *error = why;
    return false;
  };
  // validPotionTarget: an alive ally standing on the hex, the actor itself or one hex away; else potionTargetError.
  for (const BattleUnit& t : b.units())
    if (t.alive && t.side == "player" && t.x == cell.x && t.y == cell.y) {
      if (&t == &a || hexDist({a.x, a.y}, cell) == 1) return true;
      break;
    }
  const int occupant = firstAt(b, cell);
  if (occupant < 0) return fail("Não há aliado nessa casa. Escolha você ou um aliado adjacente.");
  const BattleUnit& o = b.units()[static_cast<size_t>(occupant)];
  if (o.side != "player") return fail("Poções só podem ser usadas em você ou em um aliado.");
  if (static_cast<size_t>(occupant) != actor && hexDist({a.x, a.y}, cell) != 1)
    return fail("Esse aliado está longe demais; poções alcançam apenas o próprio personagem ou um aliado adjacente.");
  return fail("Esse personagem não pode receber esta poção agora.");
}

std::optional<BattleStep> usePotion(const Battle& b, size_t actor, const std::string& id, BattleHex cell, std::string* error) {
  const BattleUnit& a = b.units()[actor];
  const PotionData* p = potionData(id);
  const int slot = potionIndex(id);
  auto fail = [&](const std::string& why) -> std::optional<BattleStep> {
    if (error) *error = why;
    return std::nullopt;
  };
  if (!p || slot < 0 || a.side != "player" || !a.alive || a.bag[slot] <= 0) return fail("Sem essa poção.");
  if (!potionTargetValid(b, actor, cell, error)) return std::nullopt;
  size_t target = actor;
  for (size_t i = 0; i < b.units().size(); ++i)
    if (b.units()[i].alive && b.units()[i].side == "player" && b.units()[i].x == cell.x && b.units()[i].y == cell.y) {
      target = i;
      break;
    }
  const BattleUnit& t = b.units()[target];
  // applyPotion's refusals (nothing spent, the turn goes on)
  if (p->effect == 'd' && !t.diseased && !t.poisoned) return fail(std::format("{} · {} não está doente.", p->name, t.name));
  if (p->effect == 'm') {
    bool room = false;
    for (int k = 0; k < 10 && !room; ++k) {
      const int cap = t.level >= 1 && static_cast<size_t>(t.level) <= t.growth.size() ? t.growth[static_cast<size_t>(t.level - 1)][static_cast<size_t>(5 + k)] : 0;
      room = cap > 0 && t.spells[k] < cap;
    }
    if (!room) return fail(std::format("{} · magias de {} já estão no máximo.", p->name, t.name));
  }
  if (p->effect == 'h' && t.hp >= t.maxHp) return fail(std::format("{} já está com HP cheio.", t.name));
  BattleStep s;
  s.type = BattleStep::Type::Potion;
  s.att = actor, s.def = target;
  s.kind = "potion";
  s.potion = id;
  return s;
}

void inflictDisease(BattleUnit& u) {
  u.diseased = true;
  u.diseaseBase = BattleUnit::Stats{u.atk, u.mag, u.def, u.dex, u.mov};
  auto pen = [](int n) { return static_cast<int>(jsRound(n * (1 - kDiseasePenalty))); };
  u.atk = pen(u.atk), u.mag = pen(u.mag), u.def = pen(u.def), u.dex = pen(u.dex);
  u.mov = std::max(1, pen(u.mov));
}

void reapplyGear(BattleUnit& u) {
  if (u.level < 1 || static_cast<size_t>(u.level) > u.growth.size()) return;
  const std::array<int, 16>& g = u.growth[static_cast<size_t>(u.level - 1)];
  const double keep = 1 - u.hungerPenaltyPct, sick = u.diseased ? 1 - kDiseasePenalty : 1;
  auto r = [](double v) { return static_cast<int>(jsRound(v)); };
  u.maxHp = r((g[0] + u.pointHp + u.gearHp) * keep);
  const BattleUnit::Stats base{r((g[1] + u.pointAtk + u.gearAtk) * keep), r((g[2] + u.pointMag + u.gearMag) * keep), r((g[3] + u.pointDef + u.gearDef) * keep),
                               r((g[4] + u.pointDex + u.gearDex) * keep), g[15] + u.gearMov};
  u.atk = r(base.atk * sick), u.mag = r(base.mag * sick), u.def = r(base.def * sick), u.dex = r(base.dex * sick);
  u.mov = u.diseased ? std::max(1, r(base.mov * sick)) : base.mov;
  u.diseaseBase = u.diseased ? std::optional(base) : std::nullopt;
  u.hp = std::min(u.maxHp, u.hp);
}

void spawnConditions(BattleUnit& u) {
  if (u.level < 1 || static_cast<size_t>(u.level) > u.growth.size()) return;
  const std::array<int, 16>& g = u.growth[static_cast<size_t>(u.level - 1)];
  const double keep = 1 - u.hungerPenaltyPct, sick = u.diseased ? 1 - kDiseasePenalty : 1;
  auto r = [](double v) { return static_cast<int>(jsRound(v)); };
  // spawnUnit: one rounding of class + points + gear, x hunger x disease (reapplyGear rounds twice)
  u.maxHp = r((g[0] + u.pointHp + u.gearHp) * keep);
  u.atk = r((g[1] + u.pointAtk + u.gearAtk) * keep * sick), u.mag = r((g[2] + u.pointMag + u.gearMag) * keep * sick);
  u.def = r((g[3] + u.pointDef + u.gearDef) * keep * sick), u.dex = r((g[4] + u.pointDex + u.gearDex) * keep * sick);
  u.mov = u.diseased ? std::max(1, r((g[15] + u.gearMov) * sick)) : g[15] + u.gearMov;
  u.diseaseBase = u.diseased ? std::optional(BattleUnit::Stats{r((g[1] + u.pointAtk + u.gearAtk) * keep), r((g[2] + u.pointMag + u.gearMag) * keep),
                                                               r((g[3] + u.pointDef + u.gearDef) * keep), r((g[4] + u.pointDex + u.gearDex) * keep), g[15] + u.gearMov})
                             : std::nullopt;
  u.hp = std::min(u.hp, u.maxHp);
}

void hurtScene(Battle& b) {
  for (size_t i = 0; i < b.units().size(); ++i) {
    BattleUnit& u = b.units()[i];
    if (u.alive) u.hp = std::max(1, u.maxHp / 2);
    if (i % 2 == 0 && u.side == "player") inflictDisease(u);
    if (i % 3 == 0) u.poisoned = true, u.poisonTier = "lesser";
    u.fullness = 0.0;
    for (int t = 0; t < 10; ++t) u.spells[t] /= 2;
  }
}

std::string spellPose(const std::string& kind) {
  if (kind == "cureMinor" || kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "bless" || kind == "circleOfHealing" ||
      kind == "resurrection")
    return "heal";
  if (kind == "longShot" || kind == "bloodyShot" || kind == "multiShot" || kind == "piercing" || kind == "doubleStrike" || kind == "piercingThrust" ||
      kind == "cleave" || kind == "sweep" || kind == "trip" || kind == "bullRush" || kind == "executionerStrike" || kind == "shieldBash" ||
      kind == "shoulderSmash" || kind == "stampede" || kind == "steelWhirlwind" || kind == "guardBreak" || kind == "warCry" || kind == "leapingStrike" ||
      archerShot(kind) || archerVolley(kind) || (lancerKind(kind) && kind != "sentinelVigil"))
    return "attack";
  return "cast";
}

int spellTierNumber(const std::string& kind) {
  if (kind == "bullRush" && bullRushTier3()) return 3;  // the heroes' (unitTierOf)
  if (kind == "summonFamiliar3" && progressionEnabled()) return 5;
  if (kind == "summonZombieDog" && progressionEnabled()) return 6;
  return spellTierOf(kind == "magicMissileV2" ? "magicMissile" : kind);
}

int spellUsesLeft(const Battle& b, size_t unit, const std::string& kind) {
  const BattleUnit& u = b.units()[unit];
  if (kind == "shock" && u.classId != "familiar4") return u.shockCharges;
  if (kind == "fantomForce") return u.fantomForceCharges;
  if (kind == "familiarLifeDrain") return u.lifeDrainCharges.value_or(1);
  if (familiarOwnSpell(u, kind == "magicMissileV2" ? "magicMissile" : kind)) return u.spellCharges;
  return tierRemaining(u, kind == "magicMissileV2" ? "magicMissile" : kind);
}

int spellTargetCount(const Battle& b, size_t unit, const std::string& kind) {
  if (kind == "wrathOfTheSkies") return emberNumbers(kind, b.units()[unit].level).count;
  if (kind == "multiShot") return multiShotTargets(b.units()[unit].level);
  return kind == "magicMissile" || kind == "magicMissileV2" ? magicMissileCount(b.units()[unit].level) : 1;
}

std::vector<BattleHex> spellZone(const Battle& b, size_t caster, const std::string& kind) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  std::vector<BattleHex> zone;
  if (spellInstant(kind)) return zone;
  int minRange = 0, maxRange = -1;
  bool shot = false, arrow = false;
  const auto grown = [&](int base) { return grownReach(kind, base, u.level); };
  if (kind == "longShot") minRange = u.minRange, maxRange = grown(kLongShotRange), shot = true, arrow = true;
  else if (kind == "lightning") maxRange = grown(kLightningRange);
  else if (kind == "shock") maxRange = kShockRange;
  else if (kind == "magicMissile" || kind == "magicMissileV2") maxRange = grown(kMissileRange), shot = true;
  else if (kind == "doubleStrike") minRange = u.minRange, maxRange = u.maxRange, shot = u.maxRange > 1, arrow = true;
  else if (const CureData* cure = cureData(kind)) maxRange = grownReach(kind, cure->range, u.level);
  else if (kind == "cureDisease") maxRange = grown(kCureDiseaseRange);
  else if (emberBoltSpell(kind) || emberAreaSpell(kind)) maxRange = emberNumbers(kind, u.level).range;  // ours: no line of fire needed
  else if (kind == "resurrection") maxRange = 1;  // the fallen ally beside the caster (or under its feet)
  // The Conjurador's: an ally / a summon in reach; the bone lance is a ray
  else if (kind == "placeSwap" || kind == "mirrorImage" || kind == "detonateSummon") maxRange = emberNumbers(kind, u.level).range;
  else if (kind == "boneLance") maxRange = -1;
  else if (kind == "guardBreak") minRange = u.minRange, maxRange = u.maxRange, shot = u.maxRange > 1, arrow = true;  // the weapon's reach
  else if (kind == "steelWhirlwind") maxRange = 1;
  else if (kind == "leapingStrike") maxRange = -1;  // wherever a landing exists
  else if (archerShot(kind)) minRange = u.minRange, maxRange = shotReach(u.maxRange, emberNumbers(kind, u.level).length), shot = true, arrow = true;
  else if (archerVolley(kind)) maxRange = emberNumbers(kind, u.level).range;  // from above: no line of fire
  // The Lanceiro's: the hook flies (a line of fire), the vault and the throw work on a neighbour, the swap reaches an ally
  else if (kind == "hookPull") minRange = 2, maxRange = emberNumbers(kind, u.level).range, shot = true, arrow = true;
  else if (kind == "poleVault") maxRange = 1;
  else if (kind == "disarm") minRange = u.minRange, maxRange = u.maxRange, shot = u.maxRange > 1, arrow = true;  // the weapon's reach
  else if (kind == "swapPost") maxRange = emberNumbers(kind, u.level).range;
  else if (kind == "spearToss" || kind == "impaleDrag") maxRange = -1;  // wherever a throw / a drag can end
  else if (kind == "fireball") maxRange = grown(kFireballRange), shot = true;
  else if (kind == "causticVenom") maxRange = grown(kCausticVenom.range), shot = true;
  else if (kind == "minorVenom") maxRange = grown(kMinorVenom.range), shot = true;
  else if (kind == "divineBolt") maxRange = grown(kDivineBolt.range), shot = true;
  else if (kind == "webOfDreams") maxRange = grown(kWebRange), shot = true;
  else if (kind == "iceStorm") maxRange = grown(iceStormPower(u.level).range), shot = true;
  else if (kind == "bloodyShot") minRange = u.minRange, maxRange = grown(kBloodyShotRange), shot = true, arrow = true;
  else if (kind == "multiShot") minRange = u.minRange, maxRange = grown(kMultiShotRange), shot = true, arrow = true;
  else if (kind == "lightningTier3") maxRange = grown(kLightningT3Range);
  else if (kind == "phantasmalForce") maxRange = grown(kPhantasmalRange), shot = true;
  else if (kind == "trip" || kind == "executionerStrike" || kind == "shieldBash") minRange = u.minRange, maxRange = u.maxRange, shot = u.maxRange > 1, arrow = true;
  else if (kind == "familiarLifeDrain") maxRange = kLifeDrainRange;
  else if (kind == "provoke") maxRange = provokePower(u.level).range;
  else if (kind == "cleave" || kind == "shoulderSmash") maxRange = 1;
  else if (kind == "sweep") maxRange = kSweepRadius;
  for (int y = 0; y < b.rows(); ++y)
    for (int x = 0; x < b.cols(); ++x) {
      const BattleHex cell{x, y};
      const int d = hexDist(from, cell);
      if (maxRange < 0) {  // a ray: where it is valid
        if (d <= 9 && spellAimValid(b, caster, kind, cell)) zone.push_back(cell);
        continue;
      }
      if (d < minRange || d > maxRange) continue;
      if (shot && d > 0 && !clearShot(b, from, cell, arrow)) continue;
      zone.push_back(cell);
    }
  return zone;
}

bool spellAimValid(const Battle& b, size_t caster, const std::string& kind, BattleHex cell, std::string* error) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  auto fail = [&](const char* why) {
    if (error) *error = why;
    return false;
  };
  if (!b.inBounds(cell.x, cell.y)) return fail("Fora do mapa.");
  const int here = occAt(b, cell);
  const int d = hexDist(from, cell);
  if (kind == "frost") return frostTiles(b, from, cell, u.level).empty() ? fail("Escolha uma reta a partir do personagem.") : true;
  if (kind == "longShot") {
    if (here < 0) return fail("Não há unidade na casa selecionada.");
    if (!targetable(b, here)) return fail("Escolha um inimigo visível.");
    if (d < u.minRange) return fail("Alvo perto demais.");
    if (const int reach = grownReach(kind, kLongShotRange, u.level); d > reach)
      return fail(std::format("Alvo fora de alcance (máximo {} hexes).", reach).c_str());
    return clearShot(b, from, cell, true) ? true : fail("O tiro está bloqueado.");
  }
  if (kind == "piercing") return piercingRay(b, from, cell).empty() ? fail("Escolha uma reta válida.") : true;
  if (kind == "piercingThrust") return piercingThrustRay(b, u, cell).empty() ? fail("Escolha uma reta na frente.") : true;
  if (kind == "lightning" || kind == "shock") {
    if (!targetable(b, here)) return fail(here < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
    const int reach = kind == "lightning" ? grownReach(kind, kLightningRange, u.level) : kShockRange;
    return d <= reach ? true : fail(std::format("Alvo fora de alcance (máximo {} hexes).", reach).c_str());
  }
  if (kind == "magicMissile" || kind == "magicMissileV2") {
    // magicMissileTargetError, then targetable
    if (here < 0 || !b.units()[static_cast<size_t>(here)].alive || b.units()[static_cast<size_t>(here)].hidden) return fail("Não há inimigo visível nessa casa.");
    const std::string& side = b.units()[static_cast<size_t>(here)].side;
    if (!b.debugFreeCast() && side != "enemy" && side != "neutral") return fail("Míssil Mágico não pode mirar em aliados.");
    if (const int reach = grownReach(kind, kMissileRange, u.level); d > reach)
      return fail(std::format("Alvo fora de alcance (máximo {} hexes).", reach).c_str());
    if (!clearShot(b, from, cell, false)) return fail("O tiro está bloqueado.");
    return targetable(b, here) ? true : fail("Não há inimigo visível nessa casa.");
  }
  if (kind == "doubleStrike") {
    if (here < 0) return fail("Não há unidade na casa selecionada.");
    const BattleUnit& f = b.units()[static_cast<size_t>(here)];
    if (!f.alive || f.side == u.side) return fail("Essa habilidade não pode mirar em uma aliada.");
    return canHitFrom(b, caster, from, static_cast<size_t>(here)) ? true : fail("Esse inimigo não está ao alcance ou não pode ser atingido daqui.");
  }
  if (const CureData* cure = cureData(kind)) {
    if (d > grownReach(kind, cure->range, u.level)) {
      const std::string why = std::format("Alvo fora de alcance (máximo {} hexes).", grownReach(kind, cure->range, u.level));
      if (error) *error = why;
      return false;
    }
    if (here < 0 || !b.units()[static_cast<size_t>(here)].alive) return fail("Escolha uma aliada na casa selecionada.");
    const BattleUnit& w = b.units()[static_cast<size_t>(here)];
    if (b.debugFreeCast()) return true;  // validHealTarget: anyone alive in range
    if (w.side != "player") return fail("Essa cura só pode ser usada em uma aliada.");
    return w.hp < w.maxHp ? true : fail("Essa aliada está com a vida cheia.");
  }
  if (kind == "cureDisease") {  // validCureDiseaseTarget, spellAimError
    if (const int reach = grownReach(kind, kCureDiseaseRange, u.level); d > reach)
      return fail(std::format("Alvo fora de alcance (máximo {} hexes).", reach).c_str());
    if (here < 0 || !b.units()[static_cast<size_t>(here)].alive) return fail("Escolha uma aliada na casa selecionada.");
    const BattleUnit& w = b.units()[static_cast<size_t>(here)];
    if (b.debugFreeCast()) return true;
    if (w.side != "player") return fail("A cura de doença só pode ser usada em uma aliada.");
    return w.diseased || w.poisoned ? true : fail("Essa aliada não está doente nem envenenada.");
  }
  if (kind == "createFoodAndWater") return cell == from ? true : fail("Esta magia não precisa de alvo.");  // no aim: self-centred
  // EmberEngine's own spells
  if (bool valid = false; aimE2(b, caster, kind, cell, valid, error)) return valid;
  if (emberSpell(kind)) {
    const EmberNumbers n = emberNumbers(kind, u.level);
    if (kind == "eruption") return cell == from ? true : fail("Esta magia não precisa de alvo.");
    // The Guerreiro's skills
    if (kind == "steelWhirlwind" || kind == "warCry") return cell == from ? true : fail("Esta habilidade não precisa de alvo.");
    if (kind == "guardBreak") {
      if (!targetable(b, here)) return fail(here < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
      return canHitFrom(b, caster, from, static_cast<size_t>(here)) ? true : fail("Esse inimigo não está ao alcance da arma.");
    }
    // The Arqueiro's skills
    if (archerShot(kind)) {
      if (!targetable(b, here)) return fail(here < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
      if (d < u.minRange) return fail("Alvo perto demais.");
      if (d > shotReach(u.maxRange, n.length)) {
        if (error) *error = std::format("Alvo fora de alcance (máximo {} hexes).", shotReach(u.maxRange, n.length));
        return false;
      }
      return clearShot(b, from, cell, true) ? true : fail("O tiro está bloqueado.");
    }
    if (archerVolley(kind)) {
      if (d > n.range) {
        if (error) *error = std::format("Alvo fora de alcance (máximo {} hexes).", n.range);
        return false;
      }
      return b.cell(cell.x, cell.y).terrain == "void" ? fail("Não há chão nessa casa.") : true;
    }
    if (kind == "leapingStrike") {
      if (!targetable(b, here)) return fail(here < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
      if (d > n.range + 1) {
        if (error) *error = std::format("Alvo fora de alcance (salto de até {} hexes).", n.range);
        return false;
      }
      return leapLanding(b, caster, static_cast<size_t>(here), n.range) ? true : fail("Não há hex livre ao lado do alvo ao alcance do salto.");
    }
    if (kind == "iceLance") return piercingRay(b, from, cell).empty() ? fail("Escolha uma reta válida.") : true;
    if (kind == "glacialGale") return coneHexes(b, from, cell, n.length).empty() ? fail("Escolha uma direção a partir do personagem.") : true;
    // The Lanceiro's skills
    if (kind == "spearToss") {
      std::string why;
      return tossPlan(b, caster, cell, n, &why) ? true : fail(why.c_str());
    }
    if (kind == "hookPull") {
      std::string why;
      return hookPlan(b, caster, cell, n, &why) ? true : fail(why.c_str());
    }
    if (kind == "poleVault") {
      std::string why;
      return vaultLanding(b, caster, cell, &why) ? true : fail(why.c_str());
    }
    if (kind == "disarm") {
      if (!targetable(b, here)) return fail(here < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
      return canHitFrom(b, caster, from, static_cast<size_t>(here)) ? true : fail("Esse inimigo não está ao alcance da arma.");
    }
    if (kind == "swapPost") {
      std::string why;
      return swapAlly(b, caster, cell, n, &why) >= 0 ? true : fail(why.c_str());
    }
    if (kind == "impaleDrag") {
      std::string why;
      return dragPlan(b, caster, cell, n, &why) ? true : fail(why.c_str());
    }
    if (kind == "sentinelVigil") return cell == from ? true : fail("Esta habilidade não precisa de alvo.");
    // The Conjurador's
    if (kind == "placeSwap") {
      std::string why;
      return swapAlly(b, caster, cell, n, &why) >= 0 ? true : fail(why.c_str());
    }
    if (kind == "boneLance") return boneLine(b, from, cell, n.length).empty() ? fail("Escolha uma reta a partir do personagem.") : true;
    if (kind == "mirrorImage") {
      if (here < 0 || !b.units()[static_cast<size_t>(here)].alive || b.units()[static_cast<size_t>(here)].side != u.side) return fail("Escolha um aliado.");
      if (d > n.range) {
        if (error) *error = std::format("Aliado fora de alcance (máximo {} hexes).", n.range);
        return false;
      }
      return true;
    }
    if (kind == "detonateSummon") {
      if (ownSummonAt(b, caster, cell) < 0) return fail("Escolha uma invocação sua.");
      if (d > n.range) {
        if (error) *error = std::format("Invocação fora de alcance (máximo {} hexes).", n.range);
        return false;
      }
      return true;
    }
    if (kind == "sacrificeBond" || kind == "empowerSummons") return cell == from ? true : fail("Esta magia não precisa de alvo.");
    // The Curandeiro's
    if (kind == "resurrection") {
      if (d > 1) return fail("O aliado caído precisa estar ao lado.");
      if (fallenAllyAt(b, caster, cell) < 0) return fail("Não há aliado caído nessa casa.");
      return occAt(b, cell) < 0 ? true : fail("Alguém está sobre o aliado caído.");
    }
    if (kind == "circleOfHealing") {
      if (d > n.range) {
        if (error) *error = std::format("Alvo fora de alcance (máximo {} hexes).", n.range);
        return false;
      }
      if (b.cell(cell.x, cell.y).terrain == "void") return fail("Não há chão nessa casa.");
      for (const BattleUnit& t : b.units())
        if (t.alive && t.side == u.side && (b.debugFreeCast() || t.hp < t.maxHp) && inArea(areaTiles(b, cell, n.radius), {t.x, t.y})) return true;
      return fail("Não há aliado ferido nessa área.");
    }
    if (emberBoltSpell(kind)) {
      if (!targetable(b, here)) return fail(here < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
      if (d > n.range) {
        if (error) *error = std::format("Alvo fora de alcance (máximo {} hexes).", n.range);
        return false;
      }
      return true;
    }
    if (emberAreaSpell(kind)) {
      if (d > n.range) {
        if (error) *error = std::format("Alvo fora de alcance (máximo {} hexes).", n.range);
        return false;
      }
      return b.cell(cell.x, cell.y).terrain == "void" ? fail("Não há chão nessa casa.") : true;
    }
  }
  return fail("Esta habilidade ainda não existe aqui.");
}

std::vector<BattleHex> spellArea(const Battle& b, size_t caster, const std::string& kind, BattleHex cell) {
  const BattleUnit& u = b.units()[caster];
  if (kind == "frost") return frostTiles(b, {u.x, u.y}, cell, u.level);
  if (kind == "piercing") return piercingRay(b, {u.x, u.y}, cell);
  if (kind == "piercingThrust") return piercingThrustRay(b, u, cell);
  if (emberSpell(kind)) {
    const EmberNumbers n = emberNumbers(kind, u.level);
    if (kind == "iceLance") return piercingRay(b, {u.x, u.y}, cell);
    if (kind == "glacialGale") return coneHexes(b, {u.x, u.y}, cell, n.length);
    if (kind == "eruption") return areaTiles(b, {u.x, u.y}, n.radius);
    if (emberAreaSpell(kind)) return areaTiles(b, cell, n.radius);
    if (kind == "steelWhirlwind") {
      std::vector<BattleHex> ring = hexesWithin(b, {u.x, u.y}, n.radius);
      std::erase_if(ring, [&](const BattleHex& h) { return h.x == u.x && h.y == u.y; });
      return ring;
    }
    if (kind == "warCry") return hexesWithin(b, {u.x, u.y}, n.radius);
    if (archerVolley(kind)) return areaTiles(b, cell, n.radius);
    if (kind == "explosiveArrow") return hexesWithin(b, cell, 1);
    if (kind == "leapingStrike") {  // the foe's hex and where the caster lands
      std::vector<BattleHex> out{cell};
      if (const int foe = occAt(b, cell); foe >= 0)
        if (const std::optional<BattleHex> landing = leapLanding(b, caster, static_cast<size_t>(foe), n.range); landing && !(*landing == cell)) out.push_back(*landing);
      return out;
    }
    // The Lanceiro's: the foe and where it (or the caster) ends up
    if (kind == "spearToss") {
      const std::optional<TossPlan> p = tossPlan(b, caster, cell, n);
      return p ? p->tiles : std::vector<BattleHex>{cell};
    }
    if (kind == "hookPull") {
      const std::optional<HookPlan> p = hookPlan(b, caster, cell, n);
      return p ? std::vector<BattleHex>{cell, p->dest} : std::vector<BattleHex>{cell};
    }
    if (kind == "poleVault") {
      const std::optional<BattleHex> landing = vaultLanding(b, caster, cell);
      return landing ? std::vector<BattleHex>{cell, *landing} : std::vector<BattleHex>{cell};
    }
    if (kind == "swapPost") return {cell, {u.x, u.y}};
    if (kind == "impaleDrag") {
      const std::optional<DragPlan> p = dragPlan(b, caster, cell, n);
      return p ? p->tiles : std::vector<BattleHex>{cell};
    }
    if (kind == "sentinelVigil") return hexesWithin(b, {u.x, u.y}, n.radius);
    // The Conjurador's
    if (kind == "boneLance") return boneLine(b, {u.x, u.y}, cell, n.length);
    if (kind == "placeSwap") return {cell, {u.x, u.y}};
    if (kind == "detonateSummon") {
      const int sm = ownSummonAt(b, caster, cell);
      return sm >= 0 ? areaTiles(b, {b.units()[static_cast<size_t>(sm)].x, b.units()[static_cast<size_t>(sm)].y}, n.radius) : std::vector<BattleHex>{cell};
    }
  }
  return areaE2(b, caster, kind, cell);
}

std::optional<std::vector<BattleStep>> castSpell(Battle& b, size_t caster, const std::string& kind, const std::vector<BattleHex>& cells, std::string* error,
                                                 bool checkUses) {
  BattleUnit& u = b.units()[caster];
  auto fail = [&](const std::string& why) -> std::optional<std::vector<BattleStep>> {
    if (error) *error = why;
    return std::nullopt;
  };
  if (!u.alive) return fail("Unidade indisponível.");
  if (cells.empty()) return fail("A magia precisa de um alvo.");
  if (u.cond.silenceTurns > 0) return fail("Silenciado: não pode conjurar.");
  if (static_cast<int>(cells.size()) != spellTargetCount(b, caster, kind)) return fail("Escolha todos os alvos.");
  if (checkUses && u.side == "player" && !contains(u.spellList, kind)) return fail("A classe ou o nível não permite esta magia.");
  if (checkUses && spellUsesLeft(b, caster, kind) <= 0) return fail("Sem usos restantes.");
  for (const BattleHex& c : cells)
    if (!spellAimValid(b, caster, kind, c, error)) return std::nullopt;
  if (emberSpell(kind)) return castEmber(b, caster, kind, cells, error);
  if (e2Kind(kind)) return castE2(b, caster, kind, cells, error, checkUses);
  const BattleHex cell = cells[0];
  std::vector<BattleStep> steps;
  BattleStep s;
  s.att = caster;
  s.kind = kind;
  if (kind == "longShot") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Não há unidade na casa selecionada.");
    spendTier(u, kind);
    const Dice p = longShotPower(u.level);
    s.tiles = {cell}, s.ids = {static_cast<size_t>(foe)};
    s.weaponBonusDice = p.dice, s.weaponBonusFaces = p.faces, s.weaponBonusBonus = spellGains(kind, u.level).n('b');
    steps.push_back(s);
  } else if (kind == "piercing" || kind == "piercingThrust") {
    const std::vector<BattleHex> line = kind == "piercing" ? piercingRay(b, {u.x, u.y}, cell) : piercingThrustRay(b, u, cell);
    if (line.empty()) return fail("Escolha uma reta válida.");
    s.ids = unitsOnLine(b, caster, line);
    spendTier(u, kind);
    s.tiles = line;
    if (kind == "piercing") s.dmgMul = piercingMul(u.level);
    steps.push_back(s);
  } else if (kind == "lightning") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Spell was not cast.");
    spendTier(u, kind);
    s.tiles = {cell}, s.ids = {static_cast<size_t>(foe)};
    s.dice = lightningDice(u.level), s.faces = kLightningFaces, s.bonus = kLightningBonus, s.spellMul = lightningMul(u.level), s.echo = kLightningEcho;
    steps.push_back(s);
  } else if (kind == "magicMissile" || kind == "magicMissileV2") {
    // One step per missile, each with its own roll (castMagicMissile collects them, then spends one use).
    std::vector<std::pair<size_t, BattleHex>> shots;
    for (const BattleHex& c : cells) {
      const int foe = occAt(b, c);
      if (foe < 0) return fail("Não há inimigo visível nessa casa.");
      shots.emplace_back(static_cast<size_t>(foe), c);
    }
    spendTier(u, "magicMissile");
    for (const auto& [foe, c] : shots) {
      BattleStep m = s;
      m.kind = "magicMissileV2";
      m.tiles = {c}, m.ids = {foe};
      m.dice = kMissileDice, m.faces = kMissileFaces, m.bonus = kMissileBonus, m.spellMul = missileMul(u.level);
      steps.push_back(m);
    }
  } else if (kind == "doubleStrike") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Spell was not cast.");
    spendTier(u, kind);
    const Dice p = doubleStrikePower(u.level);
    s.type = BattleStep::Type::Combat;
    s.def = static_cast<size_t>(foe);
    if (p.dice > 0) s.bonusDice = p.faces, s.bonusDiceCount = p.dice, s.bonusFlat = 0;
    BattleStep first = s;
    first.noCounter = true;
    steps.push_back(first);
    steps.push_back(s);
  } else if (kind == "frost") {
    if (tierRemaining(u, "frost") <= 0) return fail("Spell was not cast.");
    const std::vector<BattleHex> area = frostTiles(b, {u.x, u.y}, cell, u.level);
    if (area.empty()) return fail("Spell was not cast.");
    // queueFrost: everyone standing on the line but the caster, in the battle's own order.
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& t = b.units()[i];
      if (!t.alive || i == caster) continue;
      if (std::any_of(area.begin(), area.end(), [&](const BattleHex& c) { return occupies(t, c.x, c.y); })) s.ids.push_back(i);
    }
    const FrostPower p = frostPower(u.level);
    spendTier(u, "frost");
    s.tiles = area;
    s.dice = p.dice, s.faces = p.faces, s.bonus = 0, s.spellMul = p.mul;
    steps.push_back(s);
  } else if (cureData(kind) || kind == "cureDisease") {
    // castHeal / castCureDisease
    const int target = occAt(b, cell);
    if (target < 0) return fail("Spell was not cast.");
    spendTier(u, kind);
    s.type = kind == "cureDisease" ? BattleStep::Type::CureDisease : BattleStep::Type::Heal;
    s.def = static_cast<size_t>(target);
    steps.push_back(s);
  } else if (kind == "createFoodAndWater") {
    // startCreateFoodAndWater: the caster and every ally within 2 hexes who is hungrier than the spell feeds.
    const FoodPower p = foodPower(u.level);
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& t = b.units()[i];
      if (t.alive && t.side == u.side && hexDist({u.x, u.y}, {t.x, t.y}) <= kFoodRadius && fullnessOf(t) < p.fullness) s.ids.push_back(i);
    }
    if (s.ids.empty()) return fail("Já está bem alimentado.");
    spendTier(u, kind);
    s.type = BattleStep::Type::Food;
    s.tiles = {cell};
    s.dice = p.dice, s.faces = p.faces, s.bonus = p.bonus, s.fullness = p.fullness;
    steps.push_back(s);
  } else {
    // Choque (no player tier), and everything not ported yet.
    return fail("Esta habilidade ainda não existe aqui.");
  }
  return steps;
}

bool strikesBack(const Battle& b, const BattleStep& step) {
  if (step.noCounter) return false;
  const BattleUnit &u = b.units()[step.att], &t = b.units()[step.def];
  if (!u.alive || !t.alive || t.stunned) return false;
  if (t.cond.disarmPct > 0 || t.cond.staggered) return false;  // Desarmado / Desequilibrado (the Lanceiro's): no counter
  const Fighter a = fighterAt(b, step.att, u.x, u.y), d = fighterAt(b, step.def, t.x, t.y);
  return canCounter(b, a, {u.x, u.y}, d);
}

namespace {

// The combat log's lines for the effects a step just produced (Engine2 engine.ts pushLog): `actor` is who acted (-1 for
// what befalls a unit at the start of its turn, which carries its own verb in `cause`).
void logEffects(Battle& b, const std::vector<BattleEffect>& out, size_t from, int actor, const BattleStep* step) {
  const auto nameOf = [&](size_t i) { return b.units()[i].name; };
  const std::string who = actor >= 0 ? nameOf(static_cast<size_t>(actor)) : std::string();
  const bool en = gLogEnglish;
  // What was used (user, 2026-10-17: the log names the spell or skill): the spell / skill, the off-hand blow, or nothing for the
  // weapon's plain attack. A counter is always the defender's plain blow.
  const std::string used = !step                 ? std::string()
                           : !step->kind.empty() ? spellNameIn(step->kind, en)
                           : step->customDice    ? std::string(en ? "Off-hand" : "Mão Secundária")
                                                 : std::string();
  for (size_t k = from; k < out.size(); ++k) {
    const BattleEffect& e = out[k];
    if (e.moved && e.amount == 0 && !e.died && !e.miss && e.text.empty()) continue;  // a charge's / a leap's walk, a hop or a dash: nothing to say
    const std::string target = nameOf(e.unit);
    const std::string crit = e.crit ? (en ? " (critical)" : " (crítico)") : "";
    // "Salazar usou Mãos Flamejantes em Kael" / "Orc contra-atacou Kael" / "Kael atacou Orc"
    const std::string action = e.counter     ? (en ? std::format("{} countered {}", who, target) : std::format("{} contra-atacou {}", who, target))
                               : used.empty() ? (en ? std::format("{} attacked {}", who, target) : std::format("{} atacou {}", who, target))
                                              : (en ? std::format("{} used {} on {}", who, used, target) : std::format("{} usou {} em {}", who, used, target));
    if (!e.cause.empty()) {
      b.pushLog(en ? std::format("{} {}: {} damage", e.cause, target, e.amount) : std::format("{} {}: {} dano", e.cause, target, e.amount));
    } else if (e.miss) {
      b.pushLog(en ? std::format("{}: missed", action) : std::format("{}: errou", action));
    } else if (e.heal) {
      if (step && step->type == BattleStep::Type::Potion)
        b.pushLog(e.text.empty() ? (en ? std::format("{} used {} on {} and recovered {} HP.", who, potionName(step->potion), target, e.amount)
                                       : std::format("{} usou {} em {} e recuperou {} HP.", who, potionName(step->potion), target, e.amount))
                                 : (en ? std::format("{} used {} on {}: {}", who, potionName(step->potion), target, e.text)
                                       : std::format("{} usou {} em {}: {}", who, potionName(step->potion), target, e.text)));
      else if (!e.text.empty())
        b.pushLog(used.empty() ? std::format("{}: {}", target, e.text) : std::format("{}: {} ({})", target, e.text, used));
      else if (used.empty())
        b.pushLog(en ? std::format("{} healed {}: +{} HP", who, target, e.amount) : std::format("{} curou {}: +{} HP", who, target, e.amount));
      else
        b.pushLog(en ? std::format("{} healed {} with {}: +{} HP", who, target, used, e.amount)
                     : std::format("{} curou {} com {}: +{} HP", who, target, used, e.amount));
    } else if (e.status) {
      b.pushLog(used.empty() || actor < 0 ? std::format("{}: {}", target, e.text) : std::format("{}: {} ({})", target, e.text, used));
    } else {
      b.pushLog(en ? std::format("{}: {} damage{}", action, e.amount, crit) : std::format("{}: {} dano{}", action, e.amount, crit));
    }
    if (e.died) b.pushLog(en ? std::format("{} was defeated.", target) : std::format("{} foi derrotado.", target));
  }
}

void flushNotes(Battle& b) {
  for (std::string& n : gNotes) b.pushLog(std::move(n));
  gNotes.clear();
}

// adjustAffinity(a, b, delta): between two heroes of the party only (a summon is nobody's friend).
void affinityDelta(Battle& b, size_t a, size_t t, double delta) {
  const BattleUnit &x = b.units()[a], &y = b.units()[t];
  if (a == t || x.side != "player" || y.side != "player" || x.summoned || y.summoned) return;
  if (delta < 0) b.hurtAllies()[a].insert(t);  // hurt by this action: no side-by-side +0.1 for it when the action ends
  b.adjustAffinity(x.name, y.name, delta);
}
// gainSupportAffinity: +0.2 once per helped unit per action.
void supportAffinity(Battle& b, size_t actor, size_t target) {
  const BattleUnit &x = b.units()[actor], &y = b.units()[target];
  if (x.side != "player" || y.side != "player" || x.summoned || y.summoned) return;
  std::set<size_t>& seen = b.supportRecipients()[actor];
  if (!seen.insert(target).second) return;
  affinityDelta(b, actor, target, 0.2);
}

// applyTileHazard: flames and embers hurt whoever ends a step (or starts its turn) on them.
void tileHazard(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& u = b.units()[unit];
  const BattleCell& c = b.cell(u.x, u.y);
  if (!u.alive || c.hazardDice <= 0) return;
  int dmg = 0;
  for (int i = 0; i < c.hazardDice; ++i) dmg += 1 + static_cast<int>(std::floor(rng() * c.hazardFaces));
  const std::string element = c.terrain == "flame" ? "fire" : c.terrain == "ember" ? "ember" : "";
  if (!element.empty()) dmg = static_cast<int>(std::floor(elementalDamage(dmg, u.combat.resistance(element), 0)));
  dmg = marked(u, dmg);
  u.hp = std::max(0, u.hp - dmg);
  if (!element.empty()) trainResistance(u, element, std::nullopt, rng);
  BattleEffect e{unit, dmg, false, false, false, false, false};
  e.cause = c.terrain == "flame" ? pick("Chamas feriram", "Flames burned") : pick("Brasas feriram", "Embers burned");
  if (u.hp <= 0) markDead(u, rng), e.died = true;
  out.push_back(e);
  if (!u.alive) afterDeath(b, unit, out);
}

// provoke(target, attacker): a neutral struck by the party turns, and its whole pack with it.
void provokeNeutral(Battle& b, size_t target, size_t attacker, std::vector<BattleEffect>& out) {
  const BattleUnit& t = b.units()[target];
  if (t.side != "neutral" || b.units()[attacker].side != "player") return;
  std::vector<size_t> pack;
  for (size_t i = 0; i < b.units().size(); ++i)
    if (b.units()[i].alive && b.units()[i].side == "neutral" && b.units()[i].classId == t.classId) pack.push_back(i);
  for (size_t i : pack) b.units()[i].side = "enemy";
  BattleEffect e = statusEffect(target, pack.size() > 1 ? pick("reage, e todo o bando vem junto", "strikes back, and its whole pack with it")
                                                         : pick("se volta contra vocês", "turns on you"));
  out.push_back(e);
}

// stepCombat's "hit" / "counterHit": one strike of an exchange, with every skill's extras.
void resolveStrikeImpl(Battle& b, const BattleStep& step, bool counter, Mulberry32& rng, std::vector<BattleEffect>& out) {
  const size_t ai = counter ? step.def : step.att, ti = counter ? step.att : step.def;
  BattleUnit& actor = b.units()[ai];
  BattleUnit& target = b.units()[ti];
  const bool hitStage = !counter;
  const bool bash = hitStage && step.kind == "shieldBash";
  // Explicit dice: the attacker's own off-hand strike, or the defender's off-hand counter when the attacker stands within
  // its reach.
  CustomDice custom;
  bool useCustom = false;
  if (hitStage) {
    if (step.customDice) custom = {step.customDiceCount, step.customFaces, step.customBonus}, useCustom = true;
  } else if (actor.combat.offHandWeapon && hexDist({actor.x, actor.y}, {target.x, target.y}) <= actor.combat.offMaxRange) {
    custom = {actor.combat.offDice, actor.combat.offFaces, actor.combat.offBonus}, useCustom = true;
  }
  const bool arrowShot = actor.combat.arrow && !useCustom;
  const bool arcaneBolt = !arrowShot && actor.combat.arcane;
  if (hitStage && step.rearSure) {  // Salto de Vara (the Lanceiro's): the blow comes from behind, whichever way the foe faced
    const auto [dx, dy] = heading({target.x, target.y}, {actor.x, actor.y});
    if (dx != 0 || dy != 0) {
      target.faceDx = -dx, target.faceDy = -dy;
      if (-dx > 1e-6) target.facing = 1;
      else if (-dx < -1e-6) target.facing = -1;
    }
  }
  if (target.side != actor.side && mirrorAbsorbs(target, rng, ti, out)) return;  // Imagem Espelhada: the blow found a copy
  const Fighter A = fighterAt(b, ai, actor.x, actor.y), T = fighterAt(b, ti, target.x, target.y);
  Hit hit = rollDamage(b, A, T, rng, !bash, useCustom ? &custom : nullptr);
  if (target.side == "enemy" && !bash) trainWeapon(actor, target.level, rng, useCustom);  // the off-hand weapon on an off-hand strike
  if (actor.side == "enemy") trainDodge(target, actor.level, rng);
  const bool usesArcane = arcaneBolt && !useCustom && (counter || step.kind.empty());
  const bool usesStaff = actor.combat.staff && !useCustom && (counter || step.kind.empty());
  if (!hit.landed) {
    BattleEffect m{ti, 0, false, true, false, false, counter};
    if (hitStage && step.kind == "bullRush") m.abortRush = true;  // a missed charge shoves nobody aside: the rest of it is dropped
    out.push_back(m);
    return;
  }
  int dmg = hit.dmg;
  int bonusRoll = 0;
  if (hitStage && step.bonusDice > 0) {
    bonusRoll = rollDice(step.bonusDiceCount, step.bonusDice, step.bonusFlat, rng);
    dmg += bonusRoll;
  }
  // Bull Rush's wall-impact dice: folded into this same hit (never a second hit, never a second counter).
  if (hitStage && step.wallDice > 0) dmg += rollDice(step.wallDice, step.wallFaces, 0, rng);
  // Executioner's Strike replaces, rather than stacks with, a normal crit.
  if (hitStage && step.kind == "executionerStrike") {
    const ExecP p = executionerStrikePower(actor.level);
    if (target.maxHp > 0 && static_cast<double>(target.hp) / target.maxHp <= p.threshold)
      dmg = std::max(1, static_cast<int>(std::floor((hit.preCritDmg + bonusRoll) * p.mult)));
  }
  if (hitStage && step.dmgMul != 1) dmg = std::max(1, static_cast<int>(std::floor(dmg * step.dmgMul)));
  if (hitStage && step.stunChance > 0 && rng() < step.stunChance) target.stunned = true, target.stunTurns = 1;
  if (target.asleep) {  // a hit on a sleeper hurts 25% more and wakes it
    dmg = std::max(1, static_cast<int>(std::floor(dmg * (1 + kWebSleepBonusDamage))));
    target.asleep = false, target.sleepTurns = 0;
  }
  dmg = std::max(1, static_cast<int>(std::floor(dmg * b.zoneDamageMul(target))));
  if (usesArcane || usesStaff) {
    const std::string element = usesStaff ? actor.combat.staffElement : "arcane";
    const double staffed = dmg * (1 + (usesStaff ? staffBonus(actor.combat, element) : 0) / 100);
    dmg = static_cast<int>(std::floor(elementalDamage(staffed, target.combat.resistance(element), A.mag)));
    trainResistance(target, element, actor.level, rng);
  }
  dmg = marked(target, dmg);  // Marca da Ruína
  if (hitStage && !step.kind.empty() && dmg > 0) affinityDelta(b, ai, ti, -1);  // a skill that hurts a friend costs affinity
  const bool bonded = target.side != actor.side && bondRedirects(b, ti, dmg, rng, out);  // Elo de Sacrifício: its summon takes it
  if (bonded) dmg = 0;
  const int actual = std::min(target.hp, dmg);
  target.hp = std::max(0, target.hp - dmg);
  if (target.side != actor.side && !useCustom) staffDrain(actor, actual);
  b.noteDamage(ai, ti, dmg, false);
  provokeNeutral(b, ti, ai, out);
  if (target.side != actor.side) {
    if (hitStage) gainExp(actor, target.level, dmg, 1, target.hp <= 0);
    else gainCounterExp(actor);
  }
  BattleEffect e{ti, dmg, hit.crit, false, false, false, counter};
  if (target.hp <= 0) {
    markDead(target, rng);
    e.died = true;
  } else {
    if (hitStage) maybeInflictDisease(actor, target, rng);
    if (hitStage && step.kind == "trip") {
      // Rasteira causes Bleeding (1D8 each action) and takes 10% off every stat for the rest of the fight.
      target.bleeding = true;
      target.bleedRoundsLeft.reset(), target.bleedRoundMarker.reset();
      if (!target.crippled) {
        target.crippled = true;
        const double keep = 1 - kTripStatPenalty;
        target.atk = static_cast<int>(jsRound(target.atk * keep)), target.mag = static_cast<int>(jsRound(target.mag * keep));
        target.def = static_cast<int>(jsRound(target.def * keep)), target.dex = static_cast<int>(jsRound(target.dex * keep));
        target.mov = std::max(1, static_cast<int>(jsRound(target.mov * keep)));
      }
    }
    if (hitStage && step.kind == "shieldBash") target.stunned = true, target.stunTurns = shieldBashPower(actor.level).stunTurns;
    if (hitStage && step.sunderPct > 0) {  // Quebra-Guarda: the guard broken, DEF down for some of its turns
      target.cond.sunderPct = std::max(target.cond.sunderPct, step.sunderPct);
      target.cond.sunderTurns = std::max(target.cond.sunderTurns, step.sunderTurns);
    }
    if (hitStage && step.disarmPct > 0) {  // Desarmar: the weapon knocked away, ATK down and no counter for some of its turns
      target.cond.disarmPct = std::max(target.cond.disarmPct, step.disarmPct);
      target.cond.disarmTurns = std::max(target.cond.disarmTurns, step.disarmTurns);
      applyDisarm(target);
    }
    if (hitStage && step.kind == "bullRush" && step.knockTo) {
      target.x = step.knockTo->x, target.y = step.knockTo->y;
      e.pushed = true;
    }
  }
  if (!bonded) out.push_back(e);
  if (!target.alive) {
    afterDeath(b, ti, out);
  } else {
    if (hitStage && step.sunderPct > 0) out.push_back(statusEffect(ti, std::format("{} -{}%", pick("Guarda quebrada", "Guard broken"), step.sunderPct)));
    if (hitStage && step.disarmPct > 0) out.push_back(statusEffect(ti, std::format("{} -{}% ATK", pick("Desarmado", "Disarmed"), step.disarmPct)));
  }
}

void resolveStepImpl(Battle& b, const BattleStep& step, Mulberry32& rng, std::vector<BattleEffect>& out);

}  // namespace

void setCombatLogEnglish(bool english) { gLogEnglish = english; }
bool combatLogEnglish() { return gLogEnglish; }

std::vector<BattleStep> vigilInterrupts(const Battle& b, size_t attacker, const std::vector<size_t>& targets) {
  std::vector<BattleStep> steps;
  if (attacker >= b.units().size()) return steps;
  const BattleUnit& a = b.units()[attacker];
  if (!a.alive) return steps;
  for (size_t i = 0; i < b.units().size(); ++i) {
    const BattleUnit& s = b.units()[i];
    if (i == attacker || !s.alive || s.cond.vigilPct <= 0 || s.side == a.side || s.side == "neutral") continue;
    bool covered = false;
    for (const size_t t : targets) {
      if (t >= b.units().size()) continue;
      const BattleUnit& v = b.units()[t];
      if (t == i || (v.alive && v.side == s.side && hexDist({s.x, s.y}, {v.x, v.y}) <= s.cond.vigilRadius)) {
        covered = true;
        break;
      }
    }
    if (!covered || !canHitFrom(b, i, {s.x, s.y}, attacker)) continue;
    BattleStep st;
    st.type = BattleStep::Type::Spell, st.att = i, st.kind = "sentinelStrike";
    st.ids = {attacker}, st.tiles = {{a.x, a.y}};
    st.dice = 0, st.spellMul = 1, st.dmgMul = s.cond.vigilPct / 100.0;
    const EmberNumbers n = emberNumbers("sentinelVigil", s.level);
    st.weaponBonusDice = n.dice, st.weaponBonusFaces = n.faces, st.weaponBonusBonus = 0;
    steps.push_back(st);
  }
  return steps;
}

void resolveStrike(Battle& b, const BattleStep& step, bool counter, Mulberry32& rng, std::vector<BattleEffect>& out) {
  gNotes.clear();
  const size_t from = out.size();
  resolveStrikeImpl(b, step, counter, rng, out);
  logEffects(b, out, from, static_cast<int>(counter ? step.def : step.att), &step);
  flushNotes(b);
}

void playSteps(Battle& b, const std::vector<BattleStep>& steps, Mulberry32& rng, std::vector<BattleEffect>& out) {
  std::vector<BattleStep> queue = steps;
  bool finished = false;  // Engine2's first finishCombat -> finishAction: the action is done (acted) and builds +0.1 with its neighbours
  for (size_t i = 0; i < queue.size(); ++i) {
    const size_t before = out.size();
    resolveStep(b, queue[i], rng, out);
    if (!finished && queue[i].type != BattleStep::Type::Move) {
      finished = true;
      // Engine2's auras are a banner, not a finishCombat: they build no affinity with the neighbours.
      if (queue[i].kind != "auraOfProtection" && queue[i].kind != "intimidatingPresence") gainAdjacentAffinity(b, queue[i].att);
    }
    bool abort = false;
    for (size_t k = before; k < out.size(); ++k) abort = abort || out[k].abortRush;
    if (!abort) continue;
    const size_t actor = queue[i].att;
    std::vector<BattleStep> kept(queue.begin(), queue.begin() + static_cast<long>(i) + 1);
    for (size_t k = i + 1; k < queue.size(); ++k)
      if (!(queue[k].att == actor && (queue[k].type == BattleStep::Type::Move || (queue[k].type == BattleStep::Type::Combat && queue[k].kind == "bullRush")))) kept.push_back(queue[k]);
    queue = std::move(kept);
  }
}

std::vector<double> unitStateNumbers(const BattleUnit& u) {
  int tier = 0;
  if (u.poisoned)
    if (const PoisonTier* t = poisonTier(u.poisonTier.empty() ? "lesser" : u.poisonTier)) tier = t->rank;
  std::vector<double> v = {static_cast<double>(u.hp), u.alive ? 1.0 : 0.0, static_cast<double>(u.x), static_cast<double>(u.y), u.stunned ? 1.0 : 0.0,
                           static_cast<double>(u.stunTurns), u.asleep ? 1.0 : 0.0, static_cast<double>(u.sleepTurns), u.bleeding ? 1.0 : 0.0,
                           static_cast<double>(u.bleedRoundsLeft.value_or(-1)), u.crippled ? 1.0 : 0.0, static_cast<double>(u.fearTurns), static_cast<double>(u.atk),
                           static_cast<double>(u.mag), static_cast<double>(u.def), static_cast<double>(u.dex), static_cast<double>(u.mov), u.poisoned ? 1.0 : 0.0,
                           static_cast<double>(tier), u.cond.poisonMag, u.side == "player" ? 0.0 : u.side == "enemy" ? 1.0 : 2.0, static_cast<double>(u.xp),
                           static_cast<double>(u.level), static_cast<double>(u.maxHp), u.shock ? 1.0 : 0.0, static_cast<double>(u.blessedRoundsLeft), u.blessedHitBonusPct,
                           static_cast<double>(u.spellCharges), static_cast<double>(u.lifeDrainCharges.value_or(-1))};
  for (int t = 0; t < 10; ++t) v.push_back(static_cast<double>(u.spells[t]));
  return v;
}

namespace {
std::pair<double, double> cellHash(const std::vector<BattleHex>& cells) {
  double h = 0;
  for (const BattleHex& c : cells) h += (c.x * 1009 + c.y * 7919) % 1000003;
  return {static_cast<double>(cells.size()), h};
}
double sideNumber(const std::string& s) { return s == "player" ? 0.0 : s == "enemy" ? 1.0 : 2.0; }
}  // namespace

nlohmann::json zoneStateNumbers(const Battle& b) {
  nlohmann::json ice = nlohmann::json::array(), web = nlohmann::json::array(), aura = nlohmann::json::array();
  for (const Battle::IceZone& z : b.iceZones()) {
    const auto [n, h] = cellHash(z.cells);
    ice.push_back({z.roundsLeft, n, h, z.damageDice, z.damageFaces, z.damageMul, z.casterMag, z.casterLevel, sideNumber(z.side)});
  }
  for (const Battle::WebZone& z : b.webZones()) {
    const auto [n, h] = cellHash(z.cells);
    web.push_back({z.roundsLeft, n, h, z.sleepChance});
  }
  for (const Battle::AuraZone& z : b.auraZones()) {
    const auto [n, h] = cellHash(z.cells);
    aura.push_back({z.roundsLeft, n, h, z.kind == "protection" ? 0 : 1, sideNumber(z.side), z.pct});
  }
  nlohmann::json warp = nlohmann::json::array();
  for (const Battle::WarpGate& g : b.warpGates()) warp.push_back({g.roundsLeft, g.a.x, g.a.y, g.b.x, g.b.y, g.level});
  return {{"ice", ice}, {"web", web}, {"aura", aura}, {"warp", warp}};
}

nlohmann::json enmityNumbers(const Battle& b) {
  nlohmann::json out = nlohmann::json::array();
  for (const auto& e : b.enmityDump()) out.push_back({e[0], e[1], e[2], e[3]});
  return out;
}

void resolveStep(Battle& b, const BattleStep& step, Mulberry32& rng, std::vector<BattleEffect>& out) {
  if (step.type == BattleStep::Type::Combat) {  // its strikes (and the counter) log themselves
    resolveStepImpl(b, step, rng, out);
    return;
  }
  gNotes.clear();
  const size_t from = out.size();
  resolveStepImpl(b, step, rng, out);
  logEffects(b, out, from, static_cast<int>(step.att), &step);
  flushNotes(b);
}

namespace {

void resolveStepImpl(Battle& b, const BattleStep& step, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& att = b.units()[step.att];
  if (step.type == BattleStep::Type::Combat) {
    BattleUnit& def = b.units()[step.def];
    if (!att.alive || !def.alive) return;
    faceToward(att, {def.x, def.y});
    resolveStrike(b, step, false, rng, out);
    if (strikesBack(b, step)) {
      faceToward(def, {att.x, att.y});
      resolveStrike(b, step, true, rng, out);
    }
    return;
  }
  if (step.type == BattleStep::Type::Move) {  // a charge: the walk, hex by hex (a flame underfoot burns)
    if (!att.alive || step.path.size() < 2) return;
    for (size_t k = 1; k < step.path.size() && att.alive; ++k) {
      att.x = step.path[k].x, att.y = step.path[k].y;
      tileHazard(b, step.att, rng, out);
    }
    BattleEffect m;
    m.unit = step.att, m.moved = true;
    out.push_back(m);
    return;
  }
  if (step.type == BattleStep::Type::Effect) {
    if (const SummonSpec* sp = summonSpec(step.kind)) {  // the familiar steps out: its initiative is rolled as it appears
      const SummonData& data = summonData();
      const auto it = data.classes.find(sp->classId);
      if (it == data.classes.end() || step.tiles.empty() || !att.alive) return;
      const int roll = 1 + static_cast<int>(std::floor(rng() * 20));
      BattleUnit f = makeSummon(b, att, *sp, it->second, step.tiles.front(), roll);
      b.units().push_back(std::move(f));  // `att` and every reference into the battle's units end here
      BattleEffect e;
      e.unit = b.units().size() - 1, e.status = true, e.text = "Invocado";
      out.push_back(e);
      return;
    }
    if (step.kind == "webOfDreams") {  // whoever stands in the patch (its anchor hex) may fall asleep for 1D4 of its turns
      for (BattleUnit& v : b.units()) {
        if (!v.alive || std::none_of(step.tiles.begin(), step.tiles.end(), [&](const BattleHex& c) { return c.x == v.x && c.y == v.y; })) continue;
        if (rng() < step.spellMul) {
          v.asleep = true;
          v.sleepTurns = rollDice(kWebSleepDice, kWebSleepFaces, 0, rng);
        }
      }
      return;
    }
    if (step.kind == "provoke") {  // castProvoke: no damage, a huge volatile enmity on every enemy it reaches
      for (size_t id : step.ids) {
        b.addEnmity(id, step.att, kProvokeCe, kProvokeVe);
        out.push_back(statusEffect(id, "Provocado"));
      }
    }
    if (step.kind == "warCry") {  // Grito de Guerra: the allies rallied (ATK up for some of their turns), the foes afraid
      for (size_t id : step.ids) {
        BattleUnit& t = b.units()[id];
        if (!t.alive) continue;
        t.cond.rallyPct = std::max(t.cond.rallyPct, step.rallyPct), t.cond.rallyTurns = std::max(t.cond.rallyTurns, step.rallyTurns);
        out.push_back(statusEffect(id, std::format("{} +{}%", pick("Grito de Guerra", "War Cry"), step.rallyPct)));
      }
      for (size_t id : step.fearIds) {
        BattleUnit& t = b.units()[id];
        if (!t.alive || step.fearTurns <= 0) continue;
        t.fearTurns = std::max(t.fearTurns, step.fearTurns), t.fearSourceId = att.id;
        out.push_back(statusEffect(id, pick("Com medo", "Afraid")));
      }
      b.noteAware(step.att, kSupportCe, kSupportVe);
    }
    // The Conjurador's (docs/SKILLS_CONJURER.md)
    if (step.kind == "mirrorImage") {
      for (size_t id : step.ids) {
        BattleUnit& t = b.units()[id];
        if (!t.alive) continue;
        t.cond.mirrorCopies = std::max(t.cond.mirrorCopies, step.mirrorCopies), t.cond.mirrorRounds = std::max(t.cond.mirrorRounds, step.mirrorRounds);
        if (id != step.att) supportAffinity(b, step.att, id);
        out.push_back(statusEffect(id, std::format("{} ×{}", pick("Imagem Espelhada", "Mirror Image"), step.mirrorCopies)));
      }
      b.noteAware(step.att, kSupportCe, kSupportVe);
      return;
    }
    if (step.kind == "sacrificeBond") {
      att.cond.bondPct = step.bondPct;
      out.push_back(statusEffect(step.att, pick("Elo de Sacrifício", "Sacrifice Bond")));
      b.noteAware(step.att, kSupportCe, kSupportVe);
      return;
    }
    if (step.kind == "empowerSummons") {
      for (size_t id : step.ids) {
        BattleUnit& t = b.units()[id];
        if (!t.alive) continue;
        if (!t.empowerBase) t.empowerBase = BattleUnit::Stats{t.atk, t.mag, t.def, t.dex, t.mov};
        const double k = 1 + step.empowerPct / 100.0;
        t.atk = static_cast<int>(jsRound(t.empowerBase->atk * k)), t.mag = static_cast<int>(jsRound(t.empowerBase->mag * k));
        t.def = static_cast<int>(jsRound(t.empowerBase->def * k)), t.dex = static_cast<int>(jsRound(t.empowerBase->dex * k));
        t.mov = std::max(1, static_cast<int>(jsRound(t.empowerBase->mov * k)));
        t.cond.empowerPct = step.empowerPct, t.cond.empowerRounds = std::max(t.cond.empowerRounds, step.empowerRounds);
        out.push_back(statusEffect(id, std::format("{} +{}%", pick("Fortalecido", "Empowered"), step.empowerPct)));
      }
      b.noteAware(step.att, kSupportCe, kSupportVe);
      return;
    }
    if (step.kind == "swapPost" || step.kind == "placeSwap") {  // Troca de Posto (the Lanceiro's) / Troca de Lugar (the Conjurador's): the caster and the ally change hexes (both cards hop over)
      if (step.ids.empty() || !att.alive) return;
      BattleUnit& ally = b.units()[step.ids[0]];
      if (!ally.alive) return;
      std::swap(att.x, ally.x), std::swap(att.y, ally.y);
      for (const size_t who : {step.att, step.ids[0]}) {
        BattleEffect m;
        m.unit = who, m.moved = true, m.pushed = true, m.arc = 0.55f;
        out.push_back(m);
      }
      supportAffinity(b, step.att, step.ids[0]);
    }
    if (step.kind == "sentinelVigil") {  // Vigília da Sentinela: the stance, until the caster's next turn
      att.cond.vigilPct = step.vigilPct, att.cond.vigilRadius = step.vigilRadius;
      out.push_back(statusEffect(step.att, pick("Vigília", "Vigil")));
      b.noteAware(step.att, kSupportCe, kSupportVe);
    }
    return;
  }
  if (step.type == BattleStep::Type::Heal && step.kind == "resurrection") {
    // Ressurreição (the Curandeiro's): the fallen ally stands again with its own roll of HP (the cure's formula, +1 % per healing
    // skill point, never above its whole), what death left on it gone, and acts on its next turn (its turn this round is over).
    BattleUnit& target = b.units()[step.def];
    if (!att.alive || target.alive) return;
    const Fighter A = fighterAt(b, step.att, att.x, att.y);
    const EmberNumbers n = emberNumbers(step.kind, att.level);
    const int base = static_cast<int>(std::floor(std::floor(A.mag / 2) * n.mul + rollDice(n.dice, n.faces, 0, rng)));
    const int hp = std::max(1, std::min(target.maxHp, healingPower(att, base)));
    if (target.hobbleBase) target.mov = *target.hobbleBase;
    if (target.disarmBase) target.atk = *target.disarmBase;
    if (target.decayBase) target.atk = target.decayBase->atk, target.mag = target.decayBase->mag, target.def = target.decayBase->def, target.dex = target.decayBase->dex;
    target.hobbleBase.reset(), target.disarmBase.reset(), target.decayBase.reset();
    target.cond = BattleUnit::Conditions{};
    target.poisoned = false, target.poisonTier.clear();
    target.stunned = false, target.stunTurns = 0, target.asleep = false, target.sleepTurns = 0;
    target.bleeding = false, target.bleedRoundsLeft.reset(), target.bleedRoundMarker.reset(), target.bleedMovedThisTurn = false;
    target.fearTurns = 0, target.fearSourceId.clear(), target.shock.reset();
    target.alive = true, target.hp = hp;
    target.moved = true, target.acted = true, target.moveUsed = 0;
    supportAffinity(b, step.att, step.def);
    trainHealing(att, rng);
    b.noteAware(step.att, hp * kHealCe, hp * kHealVe);
    gainExp(att, target.level, hp);
    BattleEffect up = statusEffect(step.def, pick("Ressuscitado", "Revived"));
    up.revived = true;
    out.push_back(up);
    out.push_back({step.def, hp, false, false, true, false, false});
    return;
  }
  if (step.type == BattleStep::Type::Heal) {
    // One unit (the cures: `def`), or every unit of `ids` with its own roll (Círculo de Cura, the Curandeiro's).
    const std::vector<size_t> healed = step.ids.empty() ? std::vector<size_t>{step.def} : step.ids;
    const EmberNumbers n = emberSpell(step.kind) ? emberNumbers(step.kind, att.level) : EmberNumbers{};
    const CureData* c = cureData(step.kind);
    if (!att.alive || (!c && !emberSpell(step.kind))) return;
    for (const size_t id : healed) {
      BattleUnit& target = b.units()[id];
      if (!target.alive) continue;
      const Fighter A = fighterAt(b, step.att, att.x, att.y);
      // rollCure (the cure's own numbers), then healingAmount (+1% per healing skill point)
      const TrackGains cg = spellGains(step.kind, att.level);
      const int base = c ? static_cast<int>(std::floor(std::floor(A.mag / 2) * (c->mul + cg('m')) + rollDice(c->dice + cg.n('d'), c->faces, c->bonus, rng)))
                         : static_cast<int>(std::floor(std::floor(A.mag / 2) * n.mul + rollDice(n.dice, n.faces, 0, rng)));
      int heal = healingPower(att, base);
      if (target.cond.bleedTurns > 0) heal /= 2;  // Hemorragia: heals received halved
      const int gained = std::min(heal, target.maxHp - target.hp);
      target.hp += gained;
      if (gained > 0) {
        supportAffinity(b, step.att, id);
        trainHealing(att, rng);
        b.noteAware(step.att, gained * kHealCe, gained * kHealVe);
      }
      gainExp(att, target.level, gained);
      out.push_back({id, gained, false, false, true, false, false});
    }
    return;
  }
  if (step.type == BattleStep::Type::CureDisease) {  // stepCureDisease
    BattleUnit& target = b.units()[step.def];
    if (!att.alive || !target.alive) return;
    curePlayerDisease(target);
    supportAffinity(b, step.att, step.def);
    trainHealing(att, rng);
    b.noteAware(step.att, kSupportCe, kSupportVe);
    BattleEffect e{step.def, 0, false, false, true, false, false};
    e.text = "curado";
    out.push_back(e);
    return;
  }
  if (step.type == BattleStep::Type::Food) {  // startCreateFoodAndWater's effect
    int gained = 0;
    for (size_t id : step.ids) {
      BattleUnit& t = b.units()[id];
      t.fullness = step.fullness;
      t.hungerPenaltyPct = 0;
      reapplyGear(t);
      gained += step.dice > 0 ? rollDice(step.dice, step.faces, step.bonus, rng) : 0;
      BattleEffect e{id, 0, false, false, true, false, false};
      e.text = "alimentado";
      out.push_back(e);
    }
    b.addLootRations(gained);
    b.noteAware(step.att, kSupportCe, kSupportVe);
    if (gained > 0 && !step.ids.empty()) {
      BattleEffect e{step.att, gained, false, false, true, false, false};
      e.text = std::format("+{} {}", gained, gained == 1 ? "ração" : "rações");
      out.push_back(e);
    }
    return;
  }
  if (step.type == BattleStep::Type::Potion) {  // applyPotion
    BattleUnit& target = b.units()[step.def];
    const PotionData* p = potionData(step.potion);
    const int slot = potionIndex(step.potion);
    if (!p || slot < 0 || !att.alive || !target.alive || att.bag[slot] <= 0) return;
    if (p->effect == 'd') {
      if (!target.diseased && !target.poisoned) return;
      supportAffinity(b, step.att, step.def);
      att.bag[slot] -= 1;
      curePlayerDisease(target);
      trainHealing(att, rng);
      BattleEffect e{step.def, 0, false, false, true, false, false};
      e.text = "curado";
      out.push_back(e);
      return;
    }
    if (p->effect == 'm') {
      int restored = 0;
      for (int t = 0; t < 10; ++t) {
        const int cap = target.level >= 1 && static_cast<size_t>(target.level) <= target.growth.size()
                            ? target.growth[static_cast<size_t>(target.level - 1)][static_cast<size_t>(5 + t)]
                            : 0;  // tierUses(class, tier, level)
        if (cap <= 0) continue;
        const int next = std::min(cap, target.spells[t] + p->manaRestore);
        restored += next - target.spells[t];
        target.spells[t] = next;
      }
      if (restored <= 0) return;
      att.bag[slot] -= 1;
      BattleEffect e{step.def, restored, false, false, true, false, false};
      e.text = std::format("+{} magia", restored);
      out.push_back(e);
      return;
    }
    if (target.hp >= target.maxHp) return;
    int heal = healingPower(att, rollDice(p->dice, p->faces, p->bonus, rng));
    if (target.cond.bleedTurns > 0) heal /= 2;  // Hemorragia
    const int gained = std::min(heal, target.maxHp - target.hp);
    target.hp += gained;
    if (gained > 0) supportAffinity(b, step.att, step.def), trainHealing(att, rng);
    gainExp(att, target.level, gained);
    att.bag[slot] -= 1;
    out.push_back({step.def, gained, false, false, true, false, false});
    return;
  }
  // A spell's hits (stepSpell). The caster turns to its first target, every foe it hits turns to the caster.
  if (!att.alive) return;
  if (step.kind == "bless") {  // applyBless: +pct% to hit for some rounds, for every ally still standing
    for (size_t id : step.ids) {
      BattleUnit& ally = b.units()[id];
      if (!ally.alive || ally.side != att.side) continue;
      const double pct = blessHitBonusPct(att.level) / 100.0;
      ally.blessedHitBonusPct = std::max(ally.blessedHitBonusPct, pct);
      ally.blessedRoundsLeft = blessDuration(att.level);
      supportAffinity(b, step.att, id);
      out.push_back(statusEffect(id, std::format("Bênção +{}%", static_cast<int>(jsRound(pct * 100)))));
    }
    return;
  }
  if (!step.ids.empty()) faceToward(att, {b.units()[step.ids[0]].x, b.units()[step.ids[0]].y});
  else if (!step.tiles.empty()) faceToward(att, step.tiles[0]);
  for (size_t id : step.ids) {
    BattleUnit& r = b.units()[id];
    if (r.side != att.side) faceToward(r, {att.x, att.y});
  }
  const std::string element = elementOf(step.kind);
  // Every elemental spell trains its caster's matching resistance once, against the first enemy it reaches.
  if (!element.empty())
    for (size_t id : step.ids) {
      const BattleUnit& e = b.units()[id];
      if (e.alive && e.side == "enemy") {
        trainResistance(att, element, e.level, rng);
        break;
      }
    }
  const bool ember = emberSpell(step.kind) != nullptr;
  static const char* const kAoeKinds[] = {"turnUndead", "fireball", "cleave", "piercing", "causticVenom", "minorVenom", "piercingThrust", "sweep",
                                          "divineWrath", "shoulderSmash", "stampede", "burningHands", "poisonBreath", "tendrilSwipe"};
  const bool aoe = std::any_of(std::begin(kAoeKinds), std::end(kAoeKinds), [&](const char* k) { return step.kind == k; }) || (ember && step.ids.size() > 1);
  bool firstAoe = true;
  int thrustHits = 0, enemiesStruck = 0, drained = 0;
  std::vector<size_t> struck;  // ours: who took the hit and stands (Colheita's extra ticks)
  bool thrown = false;         // the Lanceiro's: the struck body was moved
  const bool ability = weaponAbility(step.kind);
  if (step.sacrifice && *step.sacrifice < b.units().size()) {  // Detonar Invocação: the summon blows up before its blast lands
    BattleUnit& sm = b.units()[*step.sacrifice];
    if (sm.alive) {
      sm.hp = 0;
      markDead(sm, rng);
      BattleEffect e{*step.sacrifice, 0, false, false, false, true, false};
      e.text = pick("Detonado", "Detonated");
      out.push_back(e);
    }
  }
  for (size_t k = 0; k < step.ids.size(); ++k) {
    const size_t id = step.ids[k];
    BattleUnit& foe = b.units()[id];
    if (!foe.alive || (step.kind == "turnUndead" && !foe.undead)) continue;
    if (b.cell(foe.x, foe.y).terrain == "barricade") continue;  // "bloqueado"
    if (foe.side != att.side && mirrorAbsorbs(foe, rng, id, out)) continue;  // Imagem Espelhada: a copy took it
    int dmg = 0;
    bool crit = false, landed = true, weaponRolled = false;
    if (step.centerId && id == *step.centerId) {
      const int roll = rollDice(step.centerDice, step.centerFaces, step.centerBonus, rng);
      dmg = spellDamage(b, step.att, id, step.centerMul, roll, rng);
    } else if (step.dice > 0) {
      const int roll = rollDice(step.dice, step.faces, step.bonus, rng);
      dmg = spellDamage(b, step.att, id, step.spellMul, roll, rng);
    } else if (step.kind == "piercingThrust") {
      // Armour-piercing: the defender's DEF counts 20% lower for this hit.
      const Fighter A = fighterAt(b, step.att, att.x, att.y);
      Fighter F = fighterAt(b, id, foe.x, foe.y);
      const int softDef = std::max(0, static_cast<int>(std::floor(foe.def * (1 - kThrustArmorIgnore))));
      // affinityUnit on the softened copy: the same bonus, applied to the softened DEF
      F.def = foe.def != 0 ? softDef * (F.def / foe.def) : 0;
      const Hit h = rollDamage(b, A, F, rng, true);
      weaponRolled = true;
      landed = h.landed;
      dmg = thrustHits == 0 ? h.dmg : std::max(1, static_cast<int>(std::floor(h.dmg * 0.5)));
      crit = h.crit;
      if (landed) ++thrustHits;
    } else {
      const Fighter A = fighterAt(b, step.att, att.x, att.y), F = fighterAt(b, id, foe.x, foe.y);
      const Hit h = rollDamage(b, A, F, rng, ability);
      weaponRolled = true;
      landed = h.landed;
      dmg = h.dmg;
      crit = h.crit;
      if (step.weaponBonusDice > 0 && landed) dmg += rollDice(step.weaponBonusDice, step.weaponBonusFaces, step.weaponBonusBonus, rng);
    }
    if (ability) {
      if (!weaponRolled) {
        const double accuracy = std::max(0.0, std::min(100.0, weaponAccuracy(b, fighterAt(b, step.att, att.x, att.y), false) -
                                                                  fighterAt(b, id, foe.x, foe.y).dex));
        landed = accuracy >= 100 || rng() * 100 < accuracy;
      }
      if (foe.side == "enemy") trainWeapon(att, foe.level, rng);
      if (att.side == "enemy") trainDodge(foe, att.level, rng);
    }
    if (!landed) {
      out.push_back({id, 0, false, true, false, false, false});
      continue;
    }
    if (step.kind == "fantomForce") dmg = std::max(1, static_cast<int>(std::floor(dmg * kFantomForceMul)));
    if (step.dmgMul != 1 && step.dmgMul > 0) dmg = std::max(1, static_cast<int>(std::floor(dmg * step.dmgMul)));  // ours go under 1 too (the volleys)
    if (step.kind == "cleave" && occupiedHexCount(foe) >= kCleaveLargeHexes) dmg = std::max(1, static_cast<int>(std::floor(dmg * kCleaveLargeMul)));
    if (foe.asleep) {  // a hit on a sleeper hurts 25% more and wakes it
      dmg = std::max(1, static_cast<int>(std::floor(dmg * (1 + kWebSleepBonusDamage))));
      foe.asleep = false, foe.sleepTurns = 0;
    }
    if (k < step.perTargetMul.size() && step.perTargetMul[k] != 1.0) dmg = std::max(1, static_cast<int>(std::floor(dmg * step.perTargetMul[k])));
    dmg = std::max(1, static_cast<int>(std::floor(dmg * b.zoneDamageMul(foe))));
    if (!element.empty()) {
      const double staffed = dmg * (1 + staffBonus(att.combat, element) / 100);
      dmg = static_cast<int>(std::floor(elementalDamage(staffed, foe.combat.resistance(element), fighterAt(b, step.att, att.x, att.y).mag)));
    }
    dmg = marked(foe, dmg);  // Marca da Ruína
    if (dmg > 0 && !step.kind.empty()) affinityDelta(b, step.att, id, -1);
    const bool bonded = foe.side != att.side && bondRedirects(b, id, dmg, rng, out);  // Elo de Sacrifício: its summon takes it
    if (bonded) dmg = 0;
    const int actual = std::min(foe.hp, dmg);
    foe.hp = std::max(0, foe.hp - dmg);
    if (!element.empty() && foe.side != att.side) staffDrain(att, actual);
    if (step.kind == "turnUndead" && foe.hp > 0) {
      foe.fearTurns = std::max(foe.fearTurns, kTurnUndeadFearTurns);
      foe.fearSourceId = att.id;
    }
    b.noteDamage(step.att, id, dmg, !ability);
    provokeNeutral(b, id, step.att, out);
    if (step.poison) {
      // each landed hit may poison: 100 - the effective Poison Resistance (less the tier's penalty and half the caster's MAG)
      const std::string tierId = step.kind == "causticVenom" ? "poison" : "lesser";
      const PoisonTier* pt = poisonTier(tierId);
      const double casterMag = fighterAt(b, step.att, att.x, att.y).mag;
      const double eff = std::max(-50.0, std::min(100.0, foe.combat.resistance("poison") - pt->penalty - casterMag / 2));
      if (rng() * 100 < std::max(0.0, std::min(100.0, 100 - eff))) {
        const PoisonTier* cur = foe.poisoned ? poisonTier(foe.poisonTier.empty() ? "lesser" : foe.poisonTier) : nullptr;
        const PoisonTier* next = !cur || cur->rank >= pt->rank ? (cur ? cur : pt) : pt;
        const std::string nextId = next->id;
        if (!cur || nextId != foe.poisonTier) foe.cond.poisonMag = casterMag;
        else if (nextId == tierId) foe.cond.poisonMag = std::max(foe.cond.poisonMag, casterMag);
        foe.poisonTier = nextId;
        foe.poisoned = true;
      }
    }
    if (!element.empty()) trainResistance(foe, element, att.level, rng);
    if (step.kind == "familiarLifeDrain" && !att.summonerId.empty())
      for (size_t hi = 0; hi < b.units().size(); ++hi) {
        BattleUnit& healer = b.units()[hi];
        if (healer.id != att.summonerId || !healer.alive) continue;
        const int gained = std::min(static_cast<int>(jsRound(dmg * lifeDrainHealMul(att.level))), healer.maxHp - healer.hp);
        if (gained > 0) {
          healer.hp += gained;
          out.push_back({hi, gained, false, false, true, false, false});
        }
        break;
      }
    if (foe.side != att.side) {
      const bool kill = foe.hp <= 0;
      const bool mageClass = att.classId == "mage" || att.classId == "voss" || att.classId == "conjurer" || att.classId == "warlock" || att.classId == "elementalist" ||
                           att.classId == "sorcerer" || att.classId == "necromancer";
      const bool doubled = kill && !aoe && (mageClass || step.kind == "longShot" || step.kind == "bloodyShot");
      const double mul = doubled ? 2 : aoe && !firstAoe ? 0.5 : 1;
      if (aoe) firstAoe = false;
      gainExp(att, foe.level, dmg, mul, kill);
      ++enemiesStruck;
    }
    if (step.drainPct > 0) drained += actual;
    BattleEffect e{id, dmg, crit, false, false, false, false};
    if (foe.hp <= 0) {
      markDead(foe, rng);
      e.died = true;
    } else {
      if (step.echo) {
        BattleUnit::Echo echo = *step.echo;
        echo.mag = fighterAt(b, step.att, att.x, att.y).mag;
        foe.shock = echo;
      }
      if (step.kind == "bloodyShot" && (!foe.bleeding || foe.bleedRoundsLeft)) {
        const DiceFaces duration = bloodyShotBleed(att.level);
        foe.bleeding = true;
        foe.bleedRoundsLeft = rollDice(duration.dice, duration.faces, 0, rng);
        foe.bleedRoundMarker = b.round();
      }
      if (step.kind == "sweep") {
        const BattleHex before{foe.x, foe.y};
        knockBack(b, step.att, id);
        if (foe.x != before.x || foe.y != before.y) e.pushed = true;
      }
      if (step.kind == "shoulderSmash") {
        const BattleHex before{foe.x, foe.y};
        for (int i = 0; i < kShoulderSmashKnockback; ++i) knockBack(b, step.att, id);
        if (foe.x != before.x || foe.y != before.y) e.pushed = true;
      }
    }
    // Vendaval Glacial: the struck are pushed away from the caster (their card follows: `pushed`).
    if (foe.alive && step.push > 0 && pushAway(b, id, {att.x, att.y}, step.push)) e.pushed = true;
    // The Lanceiro's throws, pulls and drags: the body lands where the cast planned (its card flies there: `arc`).
    if (foe.alive && step.throwTo && (oneHex(foe) || (step.kind == "hookPull" && pullable(foe)))) {
      foe.x = step.throwTo->x, foe.y = step.throwTo->y;
      e.pushed = true, e.arc = step.arc;
      thrown = true;
    }
    if (!bonded) out.push_back(e);
    if (!foe.alive) {
      afterDeath(b, id, out);
      continue;
    }
    // Vazio Devorador: what is left under the threshold dies; the rest loses Escuridão resistance for the fight.
    if (step.shredDarkness > 0) {
      bool found = false;
      for (auto& [el, v] : foe.combat.resistances)
        if (el == "darkness") v -= step.shredDarkness, found = true;
      if (!found) foe.combat.resistances.emplace_back("darkness", -static_cast<double>(step.shredDarkness));
    }
    if (step.execPct > 0 && foe.hp * 100 < step.execPct * foe.maxHp) {
      foe.hp = 0;
      markDead(foe, rng);
      BattleEffect x = statusEffect(id, "Executado");
      x.died = true;
      out.push_back(x);
      afterDeath(b, id, out);
      continue;
    }
    // The conditions ours leave
    const double mag = fighterAt(b, step.att, att.x, att.y).mag;
    if (step.fearPct > 0 && foe.side != att.side && rng() * 100 < step.fearPct) {  // Pesadelo: afraid for some turns
      foe.fearTurns = std::max(foe.fearTurns, step.fearTurns), foe.fearSourceId = att.id;
      out.push_back(statusEffect(id, pick("Com medo", "Afraid")));
    }
    if (step.markTurns > 0) {
      foe.cond.markPct = std::max(foe.cond.markPct, step.markPct), foe.cond.markTurns = std::max(foe.cond.markTurns, step.markTurns);
      out.push_back(statusEffect(id, std::format("Marcado +{}%", step.markPct)));
    }
    if (!step.poisonTier.empty()) {
      applyPoison(foe, step.poisonTier, mag, step.plagueSpread, step.plagueBurst);
      if (const PoisonTier* t = poisonTier(step.poisonTier)) out.push_back(statusEffect(id, t->name));
    }
    if (step.bleedDice > 0) {
      const int turns = rollDice(step.bleedTurnsDice, step.bleedTurnsFaces, 0, rng);
      if (turns >= foe.cond.bleedTurns) foe.cond.bleedDice = step.bleedDice, foe.cond.bleedFaces = step.bleedFaces, foe.cond.bleedTurns = turns;
      out.push_back(statusEffect(id, "Sangrando"));
    }
    if (step.silenceTurns > 0) {
      foe.cond.silenceTurns = std::max(foe.cond.silenceTurns, step.silenceTurns);
      for (int t = 9; t >= 0; --t)
        if (foe.spells[t] > 0) {
          foe.spells[t] -= 1;
          break;
        }
      out.push_back(statusEffect(id, "Silenciado"));
    }
    if (step.hobblePct > 0) {  // Tiro Paralisante: MOV down for some of its turns
      foe.cond.hobblePct = std::max(foe.cond.hobblePct, step.hobblePct), foe.cond.hobbleTurns = std::max(foe.cond.hobbleTurns, step.hobbleTurns);
      applyHobble(foe);
      out.push_back(statusEffect(id, std::format("{} -{}% MOV", pick("Paralisado", "Crippled"), foe.cond.hobblePct)));
    }
    if (step.stagger) {  // Gancho: pulled off its feet, it cannot counter until its next turn (and staggerTurns more of them)
      foe.cond.staggered = true, foe.cond.staggerTurns = std::max(foe.cond.staggerTurns, step.staggerTurns);
      out.push_back(statusEffect(id, pick("Desequilibrado", "Off balance")));
    }
    if (step.collideWith && step.collideDice > 0 && *step.collideWith < b.units().size()) {
      // Arremesso: the thrown body lands against another: both take the collision dice (a body's weight: no resistance), once
      // the flight is over (`delay`).
      for (const size_t who : {id, *step.collideWith}) {
        BattleUnit& v = b.units()[who];
        if (!v.alive) continue;
        const int d = marked(v, std::max(1, rollDice(step.collideDice, step.collideFaces, 0, rng)));
        v.hp = std::max(0, v.hp - d);
        b.noteDamage(step.att, who, d, false);
        if (v.side != att.side) gainExp(att, v.level, d, 0.5, v.hp <= 0);
        BattleEffect c{who, d, false, false, false, false, false};
        c.delay = 0.45f;
        if (v.hp <= 0) markDead(v, rng), c.died = true;
        out.push_back(c);
        if (!v.alive) afterDeath(b, who, out);
      }
    }
    if (step.decayTurns > 0) {
      foe.cond.decayTurns = std::max(foe.cond.decayTurns, step.decayTurns);
      foe.cond.decayCap = std::max(foe.cond.decayCap, step.decayCap), foe.cond.decayStep = step.decayStep;
      foe.cond.decayDice = step.decayDice, foe.cond.decayFaces = step.decayFaces, foe.cond.decayMul = step.decayMul, foe.cond.decayMag = mag;
      foe.cond.decayPct = std::min(foe.cond.decayCap, std::max(foe.cond.decayPct, step.decayStep));
      applyDecay(foe);
      out.push_back(statusEffect(id, std::format("Decadência -{}%", foe.cond.decayPct)));
    }
    struck.push_back(id);
  }
  // Empalar e Arrastar: the caster runs one hex behind the dragged body (its card dashes after it).
  if (step.dragTo && thrown && att.alive) {
    att.x = step.dragTo->x, att.y = step.dragTo->y;
    if (step.throwTo) faceToward(att, *step.throwTo);
    BattleEffect m;
    m.unit = step.att, m.moved = true, m.pushed = true, m.arc = 0.0f;
    out.push_back(m);
  }
  if (!ember) return;
  const Fighter A = fighterAt(b, step.att, att.x, att.y);
  // Tempestade Elétrica: the echoes, at once, on the neighbours of the struck.
  for (size_t id : step.echoIds) {
    BattleUnit& foe = b.units()[id];
    if (!foe.alive) continue;
    const int roll = rollDice(step.echoDice, step.echoFaces, step.echoBonus, rng);
    const int dmg = marked(foe, static_cast<int>(std::floor(elementalDamage(roll, foe.combat.resistance("lightning"), A.mag))));
    foe.hp = std::max(0, foe.hp - dmg);
    trainResistance(foe, "lightning", att.level, rng);
    b.noteDamage(step.att, id, dmg, true);
    if (foe.side != att.side) gainExp(att, foe.level, dmg, 0.5, foe.hp <= 0);
    BattleEffect e{id, dmg, false, false, false, false, false};
    if (foe.hp <= 0) markDead(foe, rng), e.died = true;
    out.push_back(e);
    if (!foe.alive) afterDeath(b, id, out);
  }
  // Dreno de Vida: the caster heals a share of what it took.
  if (step.drainPct > 0 && drained > 0 && att.alive) {
    const int heal = std::min(att.maxHp - att.hp, static_cast<int>(std::floor(drained * step.drainPct / 100.0)));
    if (heal > 0) {
      att.hp += heal;
      out.push_back({step.att, heal, false, false, true, false, false});
    }
  }
  // Colheita de Almas: every condition on the struck bites once more, now; the caster heals per enemy struck.
  if (step.harvest) {
    for (size_t id : struck)
      if (b.units()[id].alive) tickConditions(b, id, rng, out, false);
    if (att.alive && enemiesStruck > 0) {
      const int heal = std::min(att.maxHp - att.hp, static_cast<int>(std::floor(att.maxHp * step.harvestHealPct / 100.0)) * enemiesStruck);
      if (heal > 0) {
        att.hp += heal;
        out.push_back({step.att, heal, false, false, true, false, false});
      }
    }
  }
  // The zones ours leave (burning ground, the Mortalha, a Meteoro waiting to fall).
  if (leavesZone(step.kind)) layZone(b, step, A);
}

}  // namespace

static void startOfTurnEffectsImpl(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);

void startOfTurnEffects(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out) {
  gNotes.clear();
  const size_t from = out.size();
  startOfTurnEffectsImpl(b, unit, rng, out);
  logEffects(b, out, from, -1, nullptr);
  flushNotes(b);
}

static void startOfTurnEffectsImpl(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& u = b.units()[unit];
  if (!u.alive) return;
  if (u.shock) {
    const BattleUnit::Echo echo = *u.shock;
    u.shock.reset();
    const int base = rollDice(echo.dice, echo.faces, echo.bonus, rng);
    const int dmg = static_cast<int>(std::floor(elementalDamage(base, u.combat.resistance("lightning"), echo.mag)));
    u.hp = std::max(0, u.hp - dmg);
    trainResistance(u, "lightning", std::nullopt, rng);
    BattleEffect e{unit, dmg, false, false, false, false, false};
    e.cause = pick("Eco de relâmpago em", "Lightning echo on");
    if (u.hp <= 0) markDead(u, rng), e.died = true;
    out.push_back(e);
    if (!u.alive) afterDeath(b, unit, out);
  }
  // Its poison (Engine2: the tier's dice, its Poison Resistance less the tier's penalty and the poisoner's MAG / 2)
  if (u.alive) tickConditions(b, unit, rng, out, false, true);
  // Tempestade de Gelo: whoever starts its turn inside the field is hurt
  if (u.alive)
    for (const Battle::IceZone zone : std::vector<Battle::IceZone>(b.iceZones())) {  // a copy: a death may change the list
      if (!u.alive) break;
      const std::vector<BattleHex> body = Battle::footprint(u, u.x, u.y);
      if (std::none_of(zone.cells.begin(), zone.cells.end(), [&](const BattleHex& c) { return std::any_of(body.begin(), body.end(), [&](const BattleHex& p) { return p == c; }); })) continue;
      const double base = std::floor(std::floor(zone.casterMag / 2) * zone.damageMul) + rollDice(zone.damageDice, zone.damageFaces, 0, rng);
      int casterIdx = -1;
      for (size_t i = 0; i < b.units().size(); ++i)
        if (b.units()[i].id == zone.casterId) casterIdx = static_cast<int>(i);
      const double staffed = casterIdx >= 0 ? base * (1 + staffBonus(b.units()[static_cast<size_t>(casterIdx)].combat, "ice") / 100) : base;
      const int dmg = static_cast<int>(std::floor(elementalDamage(staffed, u.combat.resistance("ice"), zone.casterMag)));
      const int actual = std::min(u.hp, dmg);
      u.hp = std::max(0, u.hp - dmg);
      if (casterIdx >= 0 && b.units()[static_cast<size_t>(casterIdx)].side != u.side) staffDrain(b.units()[static_cast<size_t>(casterIdx)], actual);
      trainResistance(u, "ice", zone.casterLevel, rng);
      if (casterIdx >= 0 && b.units()[static_cast<size_t>(casterIdx)].side == "player") trainResistance(b.units()[static_cast<size_t>(casterIdx)], "ice", u.level, rng);
      BattleEffect e{unit, dmg, false, false, false, false, false};
      e.cause = pick("Tempestade de Gelo atinge", "Ice Storm strikes");
      if (u.hp <= 0) markDead(u, rng), e.died = true;
      out.push_back(e);
      if (!u.alive) afterDeath(b, unit, out);
    }
  // The staff's regeneration (restoreStaffHp): HP back, capped to what is missing.
  if (u.alive && u.combat.staff && u.combat.staffRegen > 0) {
    const int heal = std::min(u.maxHp - u.hp, static_cast<int>(std::floor(u.combat.staffRegen)));
    if (heal > 0) {
      u.hp += heal;
      out.push_back({unit, heal, false, false, true, false, false});
    }
  }
  // Fôlego Renovado (the paladin's passive): the first time its turn opens badly wounded with a tier-3 use banked
  if (u.alive && u.classId == "paladin" && u.maxHp > 0 && static_cast<double>(u.hp) / u.maxHp <= 0.3 && tierRemaining(u, "secondWind") > 0) {
    spendTier(u, "secondWind");
    const double pct = u.level >= 13 ? 0.75 : u.level >= 10 ? 0.5 : 0.25;
    const int heal = std::min(u.maxHp - u.hp, static_cast<int>(std::floor(pct * u.dex)));
    if (heal > 0) {
      u.hp += heal;
      out.push_back({unit, heal, false, false, true, false, false});
    }
  }
  // applyTileHazard: flames and embers hurt whoever starts its turn on them.
  if (u.alive) tileHazard(b, unit, rng, out);
  // --- EmberEngine's own: the zones it stands in, its conditions, the Meteoro it called, the countdowns ---
  if (u.alive) zoneEffects(b, unit, rng, out);
  if (u.alive) tickConditions(b, unit, rng, out, true, false);
  if (u.alive) landMeteors(b, unit, rng, out);
  if (!u.alive) return;
  // Praga Rastejante: the poison jumps to neighbours of its own side not yet poisoned.
  if (u.poisoned && u.cond.plagueSpread > 0) {
    int left = u.cond.plagueSpread;
    const std::vector<BattleHex> body = Battle::footprint(u, u.x, u.y);
    for (size_t i = 0; i < b.units().size() && left > 0; ++i) {
      BattleUnit& t = b.units()[i];
      if (i == unit || !t.alive || t.side != u.side || t.poisoned) continue;
      const std::vector<BattleHex> theirs = Battle::footprint(t, t.x, t.y);
      const bool adjacent = std::any_of(body.begin(), body.end(), [&](const BattleHex& p) {
        return std::any_of(theirs.begin(), theirs.end(), [&](const BattleHex& q) { return hexDist(p, q) == 1; });
      });
      if (!adjacent) continue;
      applyPoison(t, u.poisonTier, u.cond.poisonMag, 0, false);
      if (const PoisonTier* pt = poisonTier(u.poisonTier)) out.push_back(statusEffect(i, pt->name));
      --left;
    }
  }
  if (u.cond.markTurns > 0 && --u.cond.markTurns <= 0) u.cond.markPct = 0;
  if (u.cond.silenceTurns > 0) --u.cond.silenceTurns;
  // The Guerreiro's: "N turnos" = N of the unit's own turns with the condition on; it lifts as the next one begins.
  if (u.cond.sunderPct > 0) {
    if (u.cond.sunderTurns > 0) --u.cond.sunderTurns;
    else u.cond.sunderPct = 0;
  }
  if (u.cond.rallyPct > 0) {
    if (u.cond.rallyTurns > 0) --u.cond.rallyTurns;
    else u.cond.rallyPct = 0;
  }
  if (u.cond.hobblePct > 0) {
    if (u.cond.hobbleTurns > 0) --u.cond.hobbleTurns;
    else liftHobble(u);
  }
  // The Lanceiro's: Desarmado and Desequilibrado lift the same way; the Vigília ends as its sentinel's next turn begins.
  if (u.cond.disarmPct > 0) {
    if (u.cond.disarmTurns > 0) --u.cond.disarmTurns;
    else liftDisarm(u);
  }
  if (u.cond.staggered) {
    if (u.cond.staggerTurns > 0) --u.cond.staggerTurns;
    else u.cond.staggered = false;
  }
  if (u.cond.vigilPct > 0) u.cond.vigilPct = 0, u.cond.vigilRadius = 0;
}

void stepOnHex(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out) { tileHazard(b, unit, rng, out); }

namespace {
// data.ts weightedLootPick: weapons (not owned) and gear by power, the rarer the dearer.
const LootTables::Entry* weightedLootPick(const LootTables& t, Mulberry32& rng, int maxLevel, const std::set<std::string>& owned) {
  auto build = [&](int level) {
    std::vector<std::pair<const LootTables::Entry*, double>> entries;
    for (const LootTables::Entry& e : t.entries)
      if (!e.starting && e.power <= level && !(e.weapon && owned.count(e.id))) entries.push_back({&e, 1 / std::sqrt(std::max(1.0, e.price))});
    return entries;
  };
  std::vector<std::pair<const LootTables::Entry*, double>> entries = build(maxLevel);
  if (entries.empty()) entries = build(t.maxLevel);
  if (entries.empty()) return nullptr;
  double total = 0;
  for (const auto& [e, w] : entries) total += w;
  double roll = rng() * total;
  for (const auto& [e, w] : entries) {
    roll -= w;
    if (roll <= 0) return e;
  }
  return entries.back().first;
}
}  // namespace

bool useLockpick(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& u = b.units()[unit];
  if (u.side != "player" || !u.alive || u.lockpick <= 0) return false;
  const int li = b.adjacentLock(unit);
  if (li < 0) return false;
  Battle::Lock& lock = b.locks()[static_cast<size_t>(li)];
  const std::shared_ptr<const LootTables> tables = LootTables::shared();
  lock.open = true;
  for (const auto& [index, cell] : lock.after) b.setCell(index, cell);
  u.lockpick -= 1;
  if (lock.kind == "chest" && tables) {
    // CHEST_LOOT's three tiers: the same pool and range, climbing odds and gear headroom
    const std::string prefix = lock.tier == "best" ? "best" : lock.tier == "better" ? "better" : "";
    auto value = [&](const char* name) {
      std::string key = name;
      if (!prefix.empty()) key[0] = static_cast<char>(std::toupper(key[0])), key = prefix + key;
      return tables->chest.value(key, 0.0);
    };
    const int gain = static_cast<int>(value("emberBase") + std::floor(rng() * value("emberDice")));
    b.addLootEmber(gain);
    std::vector<Battle::ChestLoot::Item> found;
    // weightedPotionPick, then givePotion: the opener first, then the turn order (wrapping), then anyone left; a full party discards it
    std::string kind;
    {
      double total = 0;
      for (const LootTables::Potion& p : tables->potions) total += p.weight;
      double roll = rng() * total;
      for (const LootTables::Potion& p : tables->potions) {
        roll -= p.weight;
        if (roll <= 0) {
          kind = p.id;
          break;
        }
      }
      if (kind.empty() && !tables->potions.empty()) kind = tables->potions.back().id;
    }
    const LootTables::Potion* potion = nullptr;
    for (const LootTables::Potion& p : tables->potions)
      if (p.id == kind) potion = &p;
    const int slot = potionIndex(kind);
    if (potion && slot >= 0) {
      std::vector<size_t> order = b.turnOrder();
      if (order.empty())
        for (size_t i = 0; i < b.units().size(); ++i) order.push_back(i);
      size_t start = 0;
      for (size_t i = 0; i < order.size(); ++i)
        if (order[i] == unit) start = i;
      std::vector<size_t> sequence;
      std::set<size_t> seen;
      for (size_t i = 0; i < order.size(); ++i) {
        const size_t id = order[(start + i) % order.size()];
        if (seen.insert(id).second) sequence.push_back(id);
      }
      for (size_t i = 0; i < b.units().size(); ++i)
        if (!seen.count(i)) sequence.push_back(i);
      std::optional<size_t> taker;
      for (size_t t : sequence) {
        BattleUnit& v = b.units()[t];
        if (v.side != "player" || !v.alive || v.bag[slot] >= potion->carryMax) continue;
        v.bag[slot] += 1;
        taker = t;
        break;
      }
      found.push_back({taker ? potion->name + (*taker != unit ? " → " + b.units()[*taker].name : std::string())
                             : potion->name + pick(" (sem espaço — descartada)", " (no room — discarded)"),
                       "potion", potion->id});
    }
    if (rng() < value("gearChance")) {
      int highest = 1;
      for (const BattleUnit& v : b.units())
        if (v.side == "enemy") highest = std::max(highest, v.level);
      const int gearLevel = std::max(1, std::min(tables->maxLevel, static_cast<int>(jsRound(highest * value("gearTierMul")))));
      if (const LootTables::Entry* drop = weightedLootPick(*tables, rng, gearLevel, b.ownedWeapons())) {
        if (drop->weapon) b.ownedWeapons().insert(drop->id), b.lootWeapons().push_back(drop->id);
        else b.lootEquipment().push_back(drop->id);
        found.push_back({drop->name, drop->weapon ? "weapon" : "gear", drop->id});
      }
    }
    if (rng() < 0.4) {
      const int qty = 1 + static_cast<int>(std::floor(rng() * 4));
      b.addLootRations(qty);
      found.push_back({pick("Rações ×", "Rations ×") + std::to_string(qty), "rations", ""});
    }
    std::string names;
    for (size_t i = 0; i < found.size(); ++i) names += (i ? ", " : "") + found[i].name;
    b.pushLog(std::format("{}{} · +{} Gold · {}{}.", u.name, pick(" arrombou o baú", " picked the chest's lock"), gain, pick("achou ", "found "), names));
    b.setChestLoot({u.name, gain, found});
  } else {
    b.pushLog(u.name + pick(" arrombou a porta.", " picked the door's lock."));
  }
  bleedOnAction(b, unit, false, rng, out);  // using an item is an action: a bleeding opener takes the 1D8 too
  gainAdjacentAffinity(b, unit);
  u.acted = true;
  return true;
}

std::optional<BattleStep> offHandStep(const Battle& b, size_t unit, size_t foe) {
  const BattleUnit& u = b.units()[unit];
  if (!u.combat.offHandWeapon && !u.combat.offHandShield) return std::nullopt;
  if (!canOffHandAct(b, unit, {u.x, u.y}, foe)) return std::nullopt;
  BattleStep s;
  s.type = BattleStep::Type::Combat;
  s.att = unit, s.def = foe;
  if (u.combat.offHandShield) {
    s.kind = "shieldBash", s.dmgMul = u.combat.offShieldMul, s.stunChance = 0.7;
  } else {
    s.customDice = true;
    s.customDiceCount = u.combat.offDice, s.customFaces = u.combat.offFaces, s.customBonus = u.combat.offBonus;
  }
  return s;
}

void gainAdjacentAffinity(Battle& b, size_t unit) {
  const BattleUnit& u = b.units()[unit];
  // a friend this action hurt loses affinity and gains none back for standing beside the one who hurt it (taken out here, so
  // it never carries over into the unit's next action)
  std::set<size_t> hurt;
  if (const auto h = b.hurtAllies().find(unit); h != b.hurtAllies().end()) hurt = std::move(h->second), b.hurtAllies().erase(h);
  if (u.acted || !u.alive || u.side != "player" || u.summoned) return;
  const auto it = b.supportRecipients().find(unit);
  std::set<size_t> recipients = it == b.supportRecipients().end() ? std::set<size_t>() : it->second;
  recipients.insert(hurt.begin(), hurt.end());
  const std::vector<BattleHex> mine = Battle::footprint(u, u.x, u.y);
  for (size_t i = 0; i < b.units().size(); ++i) {
    const BattleUnit& v = b.units()[i];
    if (i == unit || !v.alive || v.side != "player" || v.summoned) continue;
    const std::vector<BattleHex> theirs = Battle::footprint(v, v.x, v.y);
    const bool adjacent = std::any_of(mine.begin(), mine.end(), [&](const BattleHex& p) {
      return std::any_of(theirs.begin(), theirs.end(), [&](const BattleHex& q) { return hexDist(p, q) == 1; });
    });
    if (adjacent && !recipients.count(i)) affinityDelta(b, unit, i, 0.1);
  }
  b.supportRecipients().erase(unit);
}

FinishOutcome confirmFinish(Battle& b, Mulberry32& rng) {
  FinishOutcome r;
  if (!b.winAvailable()) return r;
  size_t hero = 0;
  const Battle::Exit* exit = b.activeExit(&hero);
  r.hero = hero;
  if (exit && exit->kind == "escape") {
    if (rng() * 100 < b.fleeChance(hero)) {
      b.setFinished();
      r.ended = true;
    } else {
      BattleUnit& u = b.units()[hero];
      u.moved = true, u.acted = true;
      r.escapeFailed = true;
    }
    return r;
  }
  b.setFinished();
  r.ended = true;
  return r;
}

std::optional<BattleUnit> summonSkeleton(const std::string& id, const std::string& classId) {
  const SummonData& data = summonData();
  const auto it = data.classes.find(classId);
  if (it == data.classes.end()) return std::nullopt;
  const SummonClass& cls = it->second;
  BattleUnit f;
  f.id = id, f.classId = cls.id, f.className = cls.name, f.sprite = cls.sprite, f.side = "player", f.alive = true, f.summoned = true;
  f.combat.resistances = cls.resistances;
  f.initiativeBonus = cls.initiativeBonus;
  f.mov = cls.mov, f.minRange = cls.minRange, f.maxRange = cls.maxRange, f.size = cls.size;
  f.footprintOffsets = cls.offsets, f.footprintW = cls.footprintW, f.footprintH = cls.footprintH;
  f.faceDx = 1.0, f.faceDy = 0.0, f.facing = 1, f.fullness = 100;
  return f;
}

std::optional<BattleHex> warpCrossing(const Battle& b, size_t unitIdx) {
  const BattleUnit& u = b.units()[unitIdx];
  if (u.side != "player" && Battle::footprint(u, u.x, u.y).size() > 1) return std::nullopt;
  for (const Battle::WarpGate& g : b.warpGates()) {
    const bool atA = u.x == g.a.x && u.y == g.a.y, atB = u.x == g.b.x && u.y == g.b.y;
    if (!atA && !atB) continue;
    const BattleHex other = atA ? g.b : g.a;
    auto isGate = [&](BattleHex c) {
      return std::any_of(b.warpGates().begin(), b.warpGates().end(), [&](const Battle::WarpGate& h) { return c == h.a || c == h.b; });
    };
    std::vector<BattleHex> landing;
    for (const BattleHex& c : hexAreaTiles(b, other, 2)) {
      if (isGate(c) || !b.cell(c.x, c.y).passable) continue;
      bool taken = false;
      for (size_t i = 0; i < b.units().size(); ++i)
        if (i != unitIdx && b.units()[i].alive && occupies(b.units()[i], c.x, c.y)) taken = true;
      if (!taken) landing.push_back(c);
    }
    if (landing.empty()) return std::nullopt;
    std::stable_sort(landing.begin(), landing.end(), [&](const BattleHex& p, const BattleHex& q) {
      const int dp = hexDist(p, other), dq = hexDist(q, other);
      return dp != dq ? dp < dq : p.y != q.y ? p.y < q.y : p.x < q.x;
    });
    return landing.front();
  }
  return std::nullopt;
}

bool bleedOnAction(Battle& b, size_t unit, bool isMove, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& u = b.units()[unit];
  if (!u.alive || !u.bleeding || (isMove && u.bleedMovedThisTurn)) return u.alive;
  if (isMove) u.bleedMovedThisTurn = true;
  const int dmg = rollDice(1, 8, 0, rng);
  u.hp = std::max(0, u.hp - dmg);
  BattleEffect e{unit, dmg, false, false, false, false, false};
  e.cause = pick("Sangramento fere", "Bleeding hurts");
  if (u.hp <= 0) markDead(u, rng), e.died = true;
  out.push_back(e);
  if (!u.alive) afterDeath(b, unit, out);
  return u.alive;
}

TurnStart beginUnitTurn(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& u = b.units()[unit];
  TurnStart r;
  u.bleedMovedThisTurn = false;
  startOfTurnEffects(b, unit, rng, out);
  if (!u.alive) {
    r.kind = TurnStart::Kind::Lost;
    return r;
  }
  if (u.stunned) {
    u.stunTurns = std::max(0, u.stunTurns - 1);
    u.stunned = u.stunTurns > 0;
    u.moved = true, u.acted = true;
    r.kind = TurnStart::Kind::Lost;
    r.tip = pick(" está atordoado(a) — perde o turno.", " is stunned and loses the turn.");
    return r;
  }
  // Still standing in a web at the start of its own turn: another sleep roll, stacking 1D4 more sleep.
  if (b.isWebCell(u.x, u.y) && rng() < b.webCellSleepChance(u.x, u.y)) {
    const int extra = rollDice(kWebSleepDice, kWebSleepFaces, 0, rng);
    u.asleep = true;
    u.sleepTurns += extra;
  }
  if (u.asleep) {
    u.sleepTurns = std::max(0, u.sleepTurns - 1);
    u.asleep = u.sleepTurns > 0;
    u.moved = true, u.acted = true;
    r.kind = TurnStart::Kind::Lost;
    r.tip = pick(" está adormecido(a) — perde o turno.", " is asleep and loses the turn.");
    return r;
  }
  if (u.fearTurns > 0) {
    u.fearTurns = std::max(0, u.fearTurns - 1);
    std::vector<size_t> threats;
    int source = -1;
    for (size_t i = 0; i < b.units().size(); ++i)
      if (b.units()[i].id == u.fearSourceId) source = static_cast<int>(i);
    if (source >= 0) threats.push_back(static_cast<size_t>(source));
    else
      for (size_t i = 0; i < b.units().size(); ++i)
        if (b.units()[i].alive && b.units()[i].side != u.side && b.units()[i].side != "neutral") threats.push_back(i);
    b.setTurnRestrained(b.isWebCell(u.x, u.y));
    const std::vector<ReachCell> reach = b.reachable(unit, true), paths = b.reachable(unit, false);
    auto distance = [&](BattleHex c) {
      if (threats.empty()) return 0;
      int best = 1 << 30;
      for (size_t t : threats) best = std::min(best, hexDist(c, {b.units()[t].x, b.units()[t].y}));
      return best;
    };
    BattleHex destination{u.x, u.y};
    for (const ReachCell& c : reach)
      if (distance({c.x, c.y}) > distance(destination)) destination = {c.x, c.y};
    if (!(destination.x == u.x && destination.y == u.y)) {
      std::vector<BattleHex> path = Battle::path(paths, destination.x, destination.y);
      if (path.size() > 1) r.fleePath = std::move(path);
    }
    u.moved = true, u.acted = true;
    r.kind = TurnStart::Kind::Fled;
    r.tip = pick(" foge de medo e não pode atacar.", " flees in fear and cannot attack.");
    return r;
  }
  b.setTurnRestrained(b.isWebCell(u.x, u.y));
  return r;
}

std::optional<AiCast> planEnemyCast(Battle& b, size_t unit) {
  BattleUnit& u = b.units()[unit];
  if (!u.alive || u.side != "enemy") return std::nullopt;
  if (u.cond.silenceTurns > 0) return std::nullopt;  // Silêncio Profano: it swings its weapon instead
  const std::vector<ReachCell> reach = b.reachable(unit, true), walk = b.reachable(unit, false);
  // Enmity: whoever tops its table, else every hero standing.
  std::vector<size_t> heroes;
  if (const int focus = b.enmityTarget(unit); focus >= 0) {
    heroes.push_back(static_cast<size_t>(focus));
  } else {
    for (size_t i = 0; i < b.units().size(); ++i)
      if (b.units()[i].side == "player" && b.units()[i].alive) heroes.push_back(i);
  }
  auto score = [&](size_t h) {
    const BattleUnit& f = b.units()[h];
    return (f.maxHp - f.hp) * 3 + (f.hp <= 8 ? 20 : 0);
  };
  struct Pick {
    size_t foe = 0;
    BattleHex from;
    int score = 0;
  };
  // The best foe from any reachable hex within `range` (and a clear bolt when asked), first best kept.
  auto best = [&](int range, bool bolt) {
    std::optional<Pick> p;
    for (const ReachCell& c : reach)
      for (size_t h : heroes) {
        const BattleUnit& f = b.units()[h];
        if (hexDist({c.x, c.y}, {f.x, f.y}) > range) continue;
        if (bolt && !clearShot(b, {c.x, c.y}, {f.x, f.y}, false)) continue;
        const int sc = score(h);
        if (!p || sc > p->score) p = Pick{h, {c.x, c.y}, sc};
      }
    return p;
  };
  auto walkTo = [&](BattleHex from) {
    std::vector<BattleHex> path;
    if (from.x != u.x || from.y != u.y) path = Battle::path(walk, from.x, from.y);
    return path;
  };
  auto single = [&](const Pick& p, const char* kind, int dice, int faces, int bonus, double mul, std::optional<BattleUnit::Echo> echo) {
    AiCast cast;
    cast.path = walkTo(p.from);
    BattleStep s;
    s.att = unit, s.kind = kind;
    const BattleUnit& f = b.units()[p.foe];
    s.tiles = {{f.x, f.y}}, s.ids = {p.foe};
    s.dice = dice, s.faces = faces, s.bonus = bonus, s.spellMul = mul, s.echo = echo;
    cast.steps.push_back(s);
    return cast;
  };
  // tryAiShock
  auto shock = [&]() -> std::optional<AiCast> {
    if (u.shockCharges <= 0) return std::nullopt;
    const std::optional<Pick> p = best(kShockRange, false);
    if (!p) return std::nullopt;
    u.shockCharges -= 1;
    return single(*p, "shock", kShockDice, kShockFaces, kShockBonus, kShockMul, kShockEcho);
  };
  const std::string& c = u.classId;
  // The Ox spends its three tier-1 charges on Investida Touro (runAiFor's first branch): the nearest body of a foe it can charge.
  if (c == "bigBlueCalf" && !u.acted && tierRemaining(u, "bullRush") > 0) {
    struct Target {
      size_t foe;
      BattleHex cell;
    };
    std::vector<Target> targets;
    for (size_t h : heroes)
      for (const BattleHex& cell : Battle::footprint(b.units()[h], b.units()[h].x, b.units()[h].y)) targets.push_back({h, cell});
    std::stable_sort(targets.begin(), targets.end(), [&](const Target& p, const Target& q) {
      const int dp = hexDist({u.x, u.y}, p.cell), dq = hexDist({u.x, u.y}, q.cell);
      return dp != dq ? dp < dq : b.units()[p.foe].hp < b.units()[q.foe].hp;
    });
    for (const Target& t : targets)
      if (bullRushCharge(b, unit, t.cell)) {
        AiCast cast;
        if (std::optional<std::vector<BattleStep>> steps = castE2(b, unit, "bullRush", {t.cell}, nullptr, false)) {
          cast.steps = std::move(*steps);
          return cast;
        }
        break;
      }
  }
  const bool cultist = c == "cultist" || c == "cultistV2" || c == "emberedWraith" || c == "swampBlueCalf";
  // Ours: a level-15 cultist's one Tempestade de Gelo (Battle.cpp gives it the use). Laid where it covers the most heroes and
  // none of its own side (the field hurts whoever starts a turn in it), from a hex it can reach with a clear shot.
  if ((c == "cultist" || c == "cultistV2") && u.spells[3] > 0 && u.level >= 8) {
    struct StormPick {
      BattleHex at, from;
      int score = 0;
    };
    std::optional<StormPick> p;
    std::vector<BattleHex> centres;
    for (size_t h : heroes)
      for (const BattleHex& t : hexAreaTiles(b, {b.units()[h].x, b.units()[h].y}, 1))
        if (std::none_of(centres.begin(), centres.end(), [&](const BattleHex& q) { return q == t; })) centres.push_back(t);
    for (const BattleHex& at : centres) {
      const std::vector<BattleHex> area = iceStormAreaTiles(b, at, u.level);
      auto inside = [&](const BattleUnit& v) { return std::any_of(area.begin(), area.end(), [&](const BattleHex& q) { return occupies(v, q.x, q.y); }); };
      int hits = 0, sc = 0;
      bool ownSide = false;
      for (size_t i = 0; i < b.units().size(); ++i) {
        const BattleUnit& v = b.units()[i];
        if (!v.alive || !inside(v)) continue;
        if (v.side == "player") ++hits, sc += 10 + (v.maxHp - v.hp);
        else if (v.side == u.side) ownSide = true;
      }
      if (hits == 0 || ownSide || (p && sc <= p->score)) continue;
      for (const ReachCell& cell : reach)
        if (hexDist({cell.x, cell.y}, at) <= grownReach("iceStorm", iceStormPower(u.level).range, u.level) && clearShot(b, {cell.x, cell.y}, at, false)) {
          p = StormPick{at, {cell.x, cell.y}, sc};
          break;
        }
    }
    if (p) {
      AiCast cast;
      cast.path = walkTo(p->from);
      if (std::optional<std::vector<BattleStep>> steps = castE2(b, unit, "iceStorm", {p->at}, nullptr, false)) {
        cast.steps = std::move(*steps);
        return cast;
      }
    }
  }
  if (cultist && (u.spells[0] > 0 || u.spells[1] > 0 || u.shockCharges > 0 || u.fantomForceCharges > 0)) {
    if (u.spells[1] > 0)
      if (const std::optional<Pick> p = best(grownReach("lightning", kLightningRange, u.level), false)) {
        spendTier(u, "lightning");
        return single(*p, "lightning", lightningDice(u.level), kLightningFaces, kLightningBonus, lightningMul(u.level), kLightningEcho);
      }
    if (u.fantomForceCharges > 0)
      if (const std::optional<Pick> p = best(grownReach("phantasmalForce", kFantomForceRange, u.level), true)) {
        u.fantomForceCharges = std::max(0, u.fantomForceCharges - 1);
        return single(*p, "fantomForce", u.fantomDice, u.fantomFaces, 0, 1, std::nullopt);
      }
    if (std::optional<AiCast> s = shock()) return s;
    if (u.spells[0] > 0)
      if (const std::optional<Pick> p = best(grownReach("magicMissile", kMissileRange, u.level), true)) {
        spendTier(u, "magicMissile");
        return single(*p, "magicMissile", kMissileDice, kMissileFaces, kMissileBonus, missileMul(u.level), std::nullopt);
      }
  }
  // Frost: the close-range fallback, from where it stands, along the line through each hero.
  if (c == "cultistV2" && u.frostCharges > 0) {
    std::optional<std::pair<BattleHex, int>> bestFrost;  // target, score
    for (size_t h : heroes) {
      const BattleUnit& f = b.units()[h];
      const std::vector<BattleHex> cells = frostTiles(b, {u.x, u.y}, {f.x, f.y}, u.level);
      auto on = [&](const BattleUnit& v) { return std::any_of(cells.begin(), cells.end(), [&](const BattleHex& p) { return occupies(v, p.x, p.y); }); };
      int hits = 0, allies = 0;
      for (size_t g : heroes) hits += on(b.units()[g]) ? 1 : 0;
      if (!hits) continue;
      for (size_t i = 0; i < b.units().size(); ++i) {
        const BattleUnit& v = b.units()[i];
        if (v.alive && v.side == u.side && i != unit && on(v)) ++allies;
      }
      const int sc = hits * 10 - allies * 12;
      if (!bestFrost || sc > bestFrost->second) bestFrost = std::pair{BattleHex{f.x, f.y}, sc};
    }
    if (bestFrost && bestFrost->second > 0) {
      const std::vector<BattleHex> cells = frostTiles(b, {u.x, u.y}, bestFrost->first, u.level);
      if (!cells.empty()) {
        AiCast cast;
        BattleStep s;
        s.att = unit, s.kind = "frost";
        for (size_t i = 0; i < b.units().size(); ++i) {
          const BattleUnit& t = b.units()[i];
          if (!t.alive || i == unit) continue;
          if (std::any_of(cells.begin(), cells.end(), [&](const BattleHex& p) { return occupies(t, p.x, p.y); })) s.ids.push_back(i);
        }
        const FrostPower fp = frostPower(u.level);
        spendTier(u, "frost");
        s.tiles = cells;
        s.dice = fp.dice, s.faces = fp.faces, s.bonus = 0, s.spellMul = fp.mul;
        cast.steps.push_back(s);
        return cast;
      }
    }
  }
  // tryAiMinorVenom (the Undead Ox, the plague cattle): Veneno Menor on the splash that reaches the most foes
  auto bestSplash = [&](const BoltData& d, const char* kind, bool bolt = true) {
    struct SplashPick {
      BattleHex at, from;
      int score = 0;
    };
    std::optional<SplashPick> p;
    for (const ReachCell& cell : reach)
      for (size_t h : heroes) {
        const BattleUnit& f = b.units()[h];
        if (hexDist({cell.x, cell.y}, {f.x, f.y}) > grownReach(kind, d.range, u.level)) continue;
        if (bolt && !clearShot(b, {cell.x, cell.y}, {f.x, f.y}, false)) continue;
        int hits = 0, sc = 0;
        for (const BattleHex& t : areaTiles(b, {f.x, f.y}, areaRadius(kind, d.size, u.level)))
          for (size_t g : heroes) {
            const BattleUnit& hit = b.units()[g];
            if (hit.x != t.x || hit.y != t.y) continue;
            ++hits;
            sc += (hit.maxHp - hit.hp) + (hit.hp <= 8 ? 15 : 0);
            break;
          }
        if (hits == 0) continue;
        sc += hits * 10;
        if (!p || sc > p->score) p = SplashPick{{f.x, f.y}, {cell.x, cell.y}, sc};
      }
    return p;
  };
  if ((c == "undeadOx" || c == "plagueBearingCattle") && tierRemaining(u, "minorVenom") > 0)
    if (const auto p = bestSplash(kMinorVenom, "minorVenom")) {
      AiCast cast;
      cast.path = walkTo(p->from);
      if (std::optional<std::vector<BattleStep>> steps = castE2(b, unit, "minorVenom", {p->at}, nullptr, false)) {
        cast.steps = std::move(*steps);
        return cast;
      }
    }
  const bool birolho = c == "birolho" || c == "birolho2" || c == "birolho3" || c == "birolhoLegs" || c == "birolhoLegs2";
  // Birolho: Relampago outranks Veneno Caustico outranks Choque outranks Missil Magico. (Engine2 tests its tier-4 uses and spends
  // the venom's own tier 5: kept as it is.)
  if (birolho && (u.spells[0] > 0 || u.spells[1] > 0 || u.spells[3] > 0 || u.shockCharges > 0)) {
    if (u.spells[1] > 0)
      if (const std::optional<Pick> p = best(grownReach("lightning", kLightningRange, u.level), false)) {
        spendTier(u, "lightning");
        return single(*p, "lightning", lightningDice(u.level), kLightningFaces, kLightningBonus, lightningMul(u.level), kLightningEcho);
      }
    if (u.spells[3] > 0)
      if (const auto p = bestSplash(kCausticVenom, "causticVenom")) {
        AiCast cast;
        cast.path = walkTo(p->from);
        if (std::optional<std::vector<BattleStep>> steps = castE2(b, unit, "causticVenom", {p->at}, nullptr, false)) {
          cast.steps = std::move(*steps);
          return cast;
        }
      }
    if (std::optional<AiCast> s = shock()) return s;
    if (u.spells[0] > 0)
      if (const std::optional<Pick> p = best(grownReach("magicMissile", kMissileRange, u.level), true)) {
        spendTier(u, "magicMissile");
        return single(*p, "magicMissile", kMissileDice, kMissileFaces, kMissileBonus, missileMul(u.level), std::nullopt);
      }
  }
  // Rocco The Bird: Burning Beak (Maos Flamejantes) outranks Missil Magico outranks Choque; the cone is never aimed where it would
  // also burn one of his own.
  if (c == "roccoTheBird" && (u.spells[0] > 0 || u.spells[1] > 0 || u.shockCharges > 0)) {
    if (u.spells[1] > 0) {
      const ConeP power = burningHandsPower(u.level);
      struct ConePick {
        std::vector<BattleHex> tiles;
        std::vector<size_t> ids;
        BattleHex from;
        int score = 0;
      };
      std::optional<ConePick> cone;
      for (const ReachCell& cell : reach)
        for (const CubeI& dir : kCubeDirs) {
          const std::vector<BattleHex> tiles = coneSector(b, {cell.x, cell.y}, dir, power.radius);
          std::vector<size_t> ids;
          int sc = 0;
          bool burnsAlly = false;
          for (const BattleHex& t : tiles) {
            const int who = firstAt(b, t);
            if (who < 0 || static_cast<size_t>(who) == unit || std::find(ids.begin(), ids.end(), static_cast<size_t>(who)) != ids.end()) continue;
            const BattleUnit& v = b.units()[static_cast<size_t>(who)];
            if (v.side != "player") burnsAlly = true;
            ids.push_back(static_cast<size_t>(who));
            sc += 10 + (v.maxHp - v.hp) * 3 + (v.hp <= 8 ? 20 : 0);
          }
          if (burnsAlly || ids.empty()) continue;
          if (!cone || sc > cone->score) cone = ConePick{tiles, ids, {cell.x, cell.y}, sc};
        }
      if (cone) {
        AiCast cast;
        cast.path = walkTo(cone->from);
        spendTier(u, "burningHands");
        BattleStep s;
        s.att = unit, s.kind = "burningHands", s.tiles = cone->tiles, s.ids = cone->ids;
        s.dice = power.dice, s.faces = power.faces, s.bonus = 0, s.spellMul = power.mul;
        cast.steps.push_back(s);
        return cast;
      }
    }
    if (u.spells[0] > 0)
      if (const std::optional<Pick> p = best(grownReach("magicMissile", kMissileRange, u.level), true)) {
        spendTier(u, "magicMissile");
        return single(*p, "magicMissile", kMissileDice, kMissileFaces, kMissileBonus, missileMul(u.level), std::nullopt);
      }
    if (std::optional<AiCast> s = shock()) return s;
  }
  // The Carnivorous Plant (runAiFor): with more than one foe in reach of her tendril swipe she swipes them all; else Veneno Caustico (her
  // own tier 3), Sopro Venenoso (tier 1), Veneno Menor (tier 4). The Sapling has only its one Sopro Venenoso.
  if (c == "carnivorousPlant" || c == "sapling") {
    if (c == "carnivorousPlant") {
      // tryAiPlantSwipe: the ring of hexes around her body (not the rows more than 2 above it)
      struct Swipe {
        std::vector<BattleHex> tiles;
        std::vector<size_t> ids;
        BattleHex from;
        int score = 0;
      };
      std::optional<Swipe> swipe;
      for (const ReachCell& cell : reach) {
        const std::vector<BattleHex> body = Battle::footprint(u, cell.x, cell.y);
        Swipe s;
        s.from = {cell.x, cell.y};
        for (const BattleHex& bh : body)
          for (const BattleHex& p : Battle::neighbors(bh.x, bh.y)) {
            if (p.y < cell.y - 2 || !b.inBounds(p.x, p.y) || std::find(body.begin(), body.end(), p) != body.end()) continue;
            if (std::find(s.tiles.begin(), s.tiles.end(), p) == s.tiles.end()) s.tiles.push_back(p);
          }
        for (const BattleHex& t : s.tiles) {
          const int who = firstAt(b, t);
          if (who < 0) continue;
          const BattleUnit& v = b.units()[static_cast<size_t>(who)];
          if (static_cast<size_t>(who) == unit || v.side == u.side || std::find(s.ids.begin(), s.ids.end(), static_cast<size_t>(who)) != s.ids.end()) continue;
          s.ids.push_back(static_cast<size_t>(who));
          s.score += 10 + (v.maxHp - v.hp) * 3 + (v.hp <= 8 ? 20 : 0);
        }
        if (s.ids.size() < 2) continue;
        if (!swipe || s.score > swipe->score) swipe = std::move(s);
      }
      if (swipe) {
        AiCast cast;
        cast.path = walkTo(swipe->from);
        BattleStep s;
        s.att = unit, s.kind = "tendrilSwipe", s.tiles = swipe->tiles, s.ids = swipe->ids;
        cast.steps.push_back(s);
        return cast;
      }
      if (u.spells[2] > 0)
        if (const auto p = bestSplash(kCausticVenom, "causticVenom")) {
          std::array<int, 10> kept{};
          std::copy(std::begin(u.spells), std::end(u.spells), kept.begin());
          if (std::optional<std::vector<BattleStep>> steps = castE2(b, unit, "causticVenom", {p->at}, nullptr, false)) {
            std::copy(kept.begin(), kept.end(), std::begin(u.spells));  // she pays from her own tier 3 (Veneno Menor keeps the tier 4)
            u.spells[2] -= 1;
            AiCast cast;
            cast.path = walkTo(p->from);
            cast.steps = std::move(*steps);
            return cast;
          }
        }
    }
    if (u.spells[0] > 0) {
      // tryAiPlantPoisonBreath: the cone is never aimed where it would also hit one of her own
      const ConeP power = poisonBreathPower(u.level);
      struct ConePick {
        std::vector<BattleHex> tiles;
        std::vector<size_t> ids;
        BattleHex from;
        int score = 0;
      };
      std::optional<ConePick> cone;
      for (const ReachCell& cell : reach)
        for (const CubeI& dir : kCubeDirs) {
          const std::vector<BattleHex> tiles = coneSector(b, {cell.x, cell.y}, dir, power.radius);
          std::vector<size_t> ids;
          int sc = 0;
          bool hitsAlly = false;
          for (const BattleHex& t : tiles) {
            const int who = firstAt(b, t);
            if (who < 0 || static_cast<size_t>(who) == unit || std::find(ids.begin(), ids.end(), static_cast<size_t>(who)) != ids.end()) continue;
            const BattleUnit& v = b.units()[static_cast<size_t>(who)];
            if (v.side == u.side) hitsAlly = true;
            ids.push_back(static_cast<size_t>(who));
            sc += 10 + (v.maxHp - v.hp) * 3 + (v.hp <= 8 ? 20 : 0);
          }
          if (hitsAlly || ids.empty()) continue;
          if (!cone || sc > cone->score) cone = ConePick{tiles, ids, {cell.x, cell.y}, sc};
        }
      if (cone) {
        AiCast cast;
        cast.path = walkTo(cone->from);
        spendTier(u, "poisonBreath");
        BattleStep s;
        s.att = unit, s.kind = "poisonBreath", s.tiles = cone->tiles, s.ids = cone->ids;
        s.dice = power.dice, s.faces = power.faces, s.bonus = 0, s.spellMul = power.mul, s.poison = true;
        cast.steps.push_back(s);
        return cast;
      }
    }
    if (c == "carnivorousPlant" && tierRemaining(u, "minorVenom") > 0)
      if (const auto p = bestSplash(kMinorVenom, "minorVenom")) {
        AiCast cast;
        cast.path = walkTo(p->from);
        if (std::optional<std::vector<BattleStep>> steps = castE2(b, unit, "minorVenom", {p->at}, nullptr, false)) {
          cast.steps = std::move(*steps);
          return cast;
        }
      }
  }
  // Any other enemy mage still gets Choque.
  if (!cultist && !birolho && c != "cultistV2" && u.shockCharges > 0)
    if (std::optional<AiCast> s = shock()) return s;
  // The Brigand Crossbowman: Tiro Multiplo (tier 3) from the spot that reaches the most foes, else Tiro Sangrento (tier 2).
  if (c == "brigandCrossbowman" && (u.spells[1] > 0 || u.spells[2] > 0)) {
    const bool multi = u.spells[2] > 0;
    const int range = grownReach(multi ? "multiShot" : "bloodyShot", multi ? kMultiShotRange : kBloodyShotRange, u.level);
    struct VolleyPick {
      BattleHex from;
      std::vector<size_t> foes;
      int score = 0;
    };
    std::optional<VolleyPick> pick;
    for (const ReachCell& cell : reach) {
      std::vector<std::pair<size_t, int>> ranked;
      for (size_t h : heroes) {
        const BattleUnit& f = b.units()[h];
        const int d = hexDist({cell.x, cell.y}, {f.x, f.y});
        if (d >= u.minRange && d <= range && clearShot(b, {cell.x, cell.y}, {f.x, f.y}, true)) ranked.emplace_back(h, score(h));
      }
      if (ranked.empty()) continue;
      std::stable_sort(ranked.begin(), ranked.end(), [](const auto& p, const auto& q) { return p.second > q.second; });
      int sc = 0;
      if (multi) for (const auto& r : ranked) sc += r.second + 10;
      else sc = ranked.front().second;
      if (!pick || sc > pick->score) {
        pick = VolleyPick{{cell.x, cell.y}, {}, sc};
        for (const auto& r : ranked) pick->foes.push_back(r.first);
      }
    }
    if (pick) {
      AiCast cast;
      cast.path = walkTo(pick->from);
      spendTier(u, multi ? "multiShot" : "bloodyShot");
      if (multi) {
        const DiceFaces power = multiShotPower(u.level);
        for (int i = 0; i < multiShotTargets(u.level); ++i) {
          const size_t foe = pick->foes[static_cast<size_t>(i) % pick->foes.size()];
          BattleStep s;
          s.att = unit, s.kind = "multiShot", s.tiles = {{b.units()[foe].x, b.units()[foe].y}}, s.ids = {foe};
          s.weaponBonusDice = power.dice, s.weaponBonusFaces = power.faces, s.weaponBonusBonus = 0;
          cast.steps.push_back(s);
        }
      } else {
        const size_t foe = pick->foes.front();
        BattleStep s;
        s.att = unit, s.kind = "bloodyShot", s.tiles = {{b.units()[foe].x, b.units()[foe].y}}, s.ids = {foe}, s.dmgMul = bloodyShotMul(u.level);
        cast.steps.push_back(s);
      }
      return cast;
    }
  }
  // Crossbowmen: Tiro Perfurante while banked, else Tiro Longo.
  if ((c == "brigand" || c == "baseCrossbowman") && (u.spells[0] > 0 || u.spells[1] > 0)) {
    const bool piercing = u.spells[1] > 0;
    std::optional<Pick> p;
    for (const ReachCell& cell : reach)
      for (size_t h : heroes) {
        const BattleUnit& f = b.units()[h];
        if (!piercing) {
          const int d = hexDist({cell.x, cell.y}, {f.x, f.y});
          if (d < u.minRange || d > grownReach("longShot", kLongShotRange, u.level)) continue;
          if (!clearShot(b, {cell.x, cell.y}, {f.x, f.y}, true)) continue;
        } else {
          const std::vector<BattleHex> line = piercingRay(b, {cell.x, cell.y}, {f.x, f.y});
          if (std::none_of(line.begin(), line.end(), [&](const BattleHex& q) { return q.x == f.x && q.y == f.y; })) continue;
        }
        const int sc = score(h);
        if (!p || sc > p->score) p = Pick{h, {cell.x, cell.y}, sc};
      }
    if (p) {
      AiCast cast;
      cast.path = walkTo(p->from);
      spendTier(u, piercing ? "piercing" : "longShot");
      BattleStep s;
      s.att = unit;
      const BattleUnit& f = b.units()[p->foe];
      if (!piercing) {
        const Dice power = longShotPower(u.level);
        s.kind = "longShot";
        s.tiles = {{f.x, f.y}}, s.ids = {p->foe};
        s.weaponBonusDice = power.dice, s.weaponBonusFaces = power.faces, s.weaponBonusBonus = spellGains("longShot", u.level).n('b');
      } else {
        // the line from where it will stand; the bodies on it as the board is now (it has not walked yet)
        const std::vector<BattleHex> line = piercingRay(b, p->from, {f.x, f.y});
        s.ids = unitsOnLine(b, unit, line);
        s.kind = "piercing";
        s.tiles = line;
        s.dmgMul = piercingMul(u.level);
      }
      cast.steps.push_back(s);
      return cast;
    }
  }
  return std::nullopt;
}

}  // namespace ember::e2
