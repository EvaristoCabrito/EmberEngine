#include "world/Spells.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <map>

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
};
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
Dice longShotPower(int level) {
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
int magicMissileCount(int level) { return level >= 6 ? 3 : level >= 3 ? 2 : 1; }
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
FrostPower frostPower(int level) {
  const int lv = std::max(1, std::min(30, level));
  return {2 + std::max(0, lv - 5) / 4, 1 + std::max(0, lv - 5) / 6, 6, 1.1};
}
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
  const int lv = std::max(1, std::min(30, level));
  EmberNumbers n;
  // Bruxo
  if (kind == "markOfRuin") {
    n.range = 6, n.mul = 1.15, n.dice = 1, n.faces = 6;
    n.pct = lv >= 27 ? 35 : lv >= 23 ? 30 : lv >= 19 ? 25 : 20;
    n.turns = lv >= 22 ? 4 : 3;
  } else if (kind == "creepingPlague") {
    n.range = 6, n.mul = 1.3, n.dice = 1, n.faces = 8;
    n.tier = lv >= 25 ? "deadly" : "greater";
    n.spread = lv >= 21 ? 2 : 1;
  } else if (kind == "lifeDrain") {
    n.range = 5, n.mul = 1.6, n.dice = 2, n.faces = lv >= 26 ? 10 : lv >= 20 ? 8 : 6;
    n.pct = lv >= 28 ? 100 : lv >= 22 ? 75 : 50;
  } else if (kind == "shroudOfShadows") {
    n.range = 6, n.tickMul = 0.7, n.tickDice = 1, n.tickFaces = 6;
    if (lv >= 28) n.radius = 3, n.rounds = 4, n.pct = 35;
    else if (lv >= 24) n.radius = 2, n.rounds = 4, n.pct = 30;
    else if (lv >= 20) n.radius = 2, n.rounds = 3, n.pct = 25;
    else n.radius = 1, n.rounds = 2, n.pct = 20;
  } else if (kind == "hemorrhage") {
    n.range = 5, n.mul = 1.4, n.dice = 2, n.faces = 6;
    n.tickDice = lv >= 26 ? 3 : 2, n.tickFaces = lv >= 30 ? 8 : lv >= 26 ? 6 : lv >= 22 ? 8 : 6;
    n.durDice = lv >= 24 ? 2 : 1, n.durFaces = lv >= 24 ? 4 : 6;
  } else if (kind == "profaneSilence") {
    n.range = 6, n.radius = lv >= 26 ? 2 : 1, n.mul = 1.1, n.dice = 1, n.faces = 8;
    n.turns = lv >= 24 ? 3 : 2;
  } else if (kind == "blackPestilence") {
    n.range = 7, n.radius = lv >= 25 ? 3 : 2, n.mul = 1.2, n.dice = 2, n.faces = 6;
    n.tier = lv >= 29 ? "lethal" : "deadly";
  } else if (kind == "curseOfDecay") {
    n.range = 6, n.mul = 1.5, n.dice = 2, n.faces = 8;
    n.tickMul = 0.5, n.tickDice = 1, n.tickFaces = 8;
    n.pct = lv >= 29 ? 50 : lv >= 25 ? 40 : 30;  // the cap
    n.turns = lv >= 27 ? 4 : 3;
    n.spread = 10;  // the step per turn
  } else if (kind == "soulHarvest") {
    n.range = 6, n.radius = 2, n.mul = lv >= 27 ? 1.9 : 1.7;
    n.dice = lv >= 29 ? 4 : 3, n.faces = lv >= 29 ? 6 : lv >= 26 ? 8 : 6;
    n.pct = 10;
  } else if (kind == "devouringVoid") {
    n.range = 7, n.mul = 2.2, n.dice = lv >= 28 ? 4 : 3, n.faces = 8;
    n.pct = lv >= 30 ? 25 : lv >= 27 ? 20 : 15;
    n.spread = 30;  // Escuridão resistance shred
  }
  // Elementalista
  else if (kind == "pillarOfFlame") {
    n.range = 6, n.radius = lv >= 23 ? 2 : 1, n.mul = 2.0, n.dice = lv >= 19 ? 3 : 2, n.faces = 8;
    n.hazardDice = 1, n.hazardFaces = 8, n.rounds = lv >= 27 ? 2 : 1;
  } else if (kind == "iceLance") {
    n.range = 7, n.mul = lv >= 28 ? 2.6 : 2.4, n.dice = lv >= 25 ? 3 : 2, n.faces = 10;
    n.pct = lv >= 21 ? 100 : 50;  // the hexes past the first
  } else if (kind == "thunderstorm") {
    n.range = 6, n.radius = lv >= 27 ? 3 : 2, n.mul = 1.8, n.dice = lv >= 21 ? 3 : 2, n.faces = 12;
    n.tickDice = lv >= 24 ? 2 : 1, n.tickFaces = 12, n.bonus = 4;  // the echo: tickDice D12 + 4
  } else if (kind == "meteor") {
    n.range = 8, n.radius = lv >= 26 ? 3 : 2, n.mul = lv >= 29 ? 3.3 : 3.0, n.dice = lv >= 22 ? 5 : 4, n.faces = 6;
    n.ringMul = 2.0, n.ringDice = 2, n.ringFaces = 6;
  } else if (kind == "absoluteZero") {
    n.range = 6, n.radius = lv >= 30 ? 3 : 2, n.mul = lv >= 27 ? 2.5 : 2.2, n.dice = lv >= 23 ? 4 : 3, n.faces = 8;
  } else if (kind == "chainLightning") {
    n.range = 7, n.mul = 2.8, n.dice = lv >= 24 ? 4 : 3, n.faces = 10;
    n.jumps = lv >= 26 ? 5 : lv >= 22 ? 4 : 3;
    n.decay = lv >= 28 ? 0.10 : 0.15;
    n.length = 3;  // a jump's reach
  } else if (kind == "eruption") {
    n.radius = lv >= 25 ? 4 : 3, n.mul = lv >= 30 ? 2.9 : 2.6, n.dice = lv >= 27 ? 5 : 4, n.faces = 8;
    n.hazardDice = 1, n.hazardFaces = 8, n.rounds = 2;
  } else if (kind == "glacialGale") {
    n.length = lv >= 24 ? 6 : 5, n.mul = 2.4, n.dice = lv >= 26 ? 5 : 4, n.faces = 6;
    n.push = lv >= 29 ? 2 : 1;
  } else if (kind == "wrathOfTheSkies") {
    n.range = 8, n.count = lv >= 26 ? 5 : 4, n.mul = lv >= 30 ? 2.9 : 2.6, n.dice = lv >= 28 ? 4 : 3, n.faces = 12;
  } else if (kind == "primordialEmber") {
    n.range = 7, n.radius = lv >= 30 ? 4 : 3, n.mul = lv >= 29 ? 3.7 : 3.4, n.dice = lv >= 27 ? 5 : 4, n.faces = 10;
    n.hazardDice = 2, n.hazardFaces = 8, n.rounds = 2;
  }
  return n;
}
// Both sides stand in its area (else the caster's foes only).
bool emberBothSides(const std::string& kind) {
  return kind == "shroudOfShadows" || kind == "blackPestilence" || kind == "meteor" || kind == "glacialGale" || kind == "primordialEmber";
}
// Aimed at a hex of the board (an area) rather than at a unit.
bool emberAreaSpell(const std::string& kind) {
  return kind == "shroudOfShadows" || kind == "profaneSilence" || kind == "blackPestilence" || kind == "soulHarvest" || kind == "pillarOfFlame" ||
         kind == "thunderstorm" || kind == "meteor" || kind == "absoluteZero" || kind == "primordialEmber";
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
  for (int i = 1; i <= frostPower(level).length; ++i) {
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
  const int tier = spellTierOf(kind);
  return tier ? u.spells[tier - 1] : 0;
}
void spendTier(BattleUnit& u, const std::string& kind) {
  if (kind == "frost" && u.side == "enemy") {
    u.frostCharges = std::max(0, u.frostCharges - 1);
    return;
  }
  const int tier = spellTierOf(kind);
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
  u.maxHp = after[0] + u.gearHp;
  u.atk = after[1] + u.gearAtk, u.mag = after[2] + u.gearMag, u.def = after[3] + u.gearDef, u.dex = after[4] + u.gearDex;
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
void tickConditions(Battle& b, size_t id, Mulberry32& rng, std::vector<BattleEffect>& out, bool countDown) {
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
  if (u.alive && u.poisoned)
    if (const PoisonTier* t = poisonTier(u.poisonTier)) {
      const int roll = rollDice(t->dice, t->faces, 0, rng);
      hurt(static_cast<int>(std::floor(elementalDamage(roll, u.combat.resistance("poison") - t->penalty, u.cond.poisonMag))), "poison", pick("Veneno consome", "Poison consumes"));
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
    const std::vector<BattleHex> area = hexesWithin(b, z.centre, z.radius);
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
  } else if (emberAreaSpell(kind)) {
    const std::vector<BattleHex> area = hexesWithin(b, cell, n.radius);
    s.tiles = area;
    if (kind == "meteor") {
      s.delayed = true;  // its zone is laid at the impact; it falls at the caster's next turn
    } else if (kind == "shroudOfShadows") {
      s.dice = 0, s.spellMul = 0;  // the zone alone (laid at the impact)
    } else {
      s.ids = unitsOn(b, caster, area, emberBothSides(kind));
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
    const std::vector<BattleHex> area = hexesWithin(b, from, n.radius);
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
  for (const EmberSpell& e : kEmberSpells)
    if (classId == e.cls && level >= e.learn) out.push_back(e.data.kind);
  return out;
}
void grantClassSpells(BattleUnit& u) {
  for (const std::string& k : emberSpells(u.classId, u.level))
    if (!contains(u.spellList, k)) u.spellList.push_back(k);
}
std::string promotionName(const std::string& classId) {
  if (classId == "warlock") return "Bruxo";
  if (classId == "elementalist") return "Elementalista";
  if (classId == "mage" || classId == "voss" || classId.empty()) return "Mago Negro";
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
  if (u.diseased) add("Doente");
  return s;
}
std::string spellTooltip(const BattleUnit& c, const std::string& kind) {
  const EmberSpell* e = emberSpell(kind);
  if (!e) return {};
  const EmberNumbers n = emberNumbers(kind, c.level);
  const int half = static_cast<int>(std::floor(c.mag / 2.0));
  auto dmg = [&](double mul, int dice, int faces) { return std::format("{} + {}D{}", static_cast<int>(std::floor(half * mul)), dice, faces); };
  static const std::map<std::string, const char*> kElements = {{"darkness", "Escuridão"}, {"poison", "Veneno"}, {"fire", "Fogo"}, {"ice", "Gelo"},
                                                               {"lightning", "Relâmpago"}, {"ember", "Brasa"}};
  std::string s = std::format("T{} · {}", e->data.tier, kElements.contains(e->data.element) ? kElements.at(e->data.element) : e->data.element);
  if (n.range > 0) s += std::format(" · alcance {}", n.range);
  if (kind == "markOfRuin") s += std::format("\n{} · o alvo recebe +{}% de dano por {} turnos", dmg(n.mul, n.dice, n.faces), n.pct, n.turns);
  else if (kind == "creepingPlague") s += std::format("\n{} · {} que passa a {} vizinho{} por turno", dmg(n.mul, n.dice, n.faces), poisonTier(n.tier)->name, n.spread, n.spread > 1 ? "s" : "");
  else if (kind == "lifeDrain") s += std::format("\n{} · cura {}% do dano causado", dmg(n.mul, n.dice, n.faces), n.pct);
  else if (kind == "shroudOfShadows") s += std::format("\nzona raio {} por {} rodadas · -{}% ATK e MAG · {} por turno a quem está dentro", n.radius, n.rounds, n.pct, dmg(n.tickMul, n.tickDice, n.tickFaces));
  else if (kind == "hemorrhage") s += std::format("\n{} · sangra {}D{} por turno durante {}D{} turnos · curas pela metade", dmg(n.mul, n.dice, n.faces), n.tickDice, n.tickFaces, n.durDice, n.durFaces);
  else if (kind == "profaneSilence") s += std::format("\nraio {} · {} · silenciados por {} turnos, -1 uso do maior tier", n.radius, dmg(n.mul, n.dice, n.faces), n.turns);
  else if (kind == "blackPestilence") s += std::format("\nraio {} (os dois lados) · {} · {} · quem morre envenenado contagia os vizinhos", n.radius, dmg(n.mul, n.dice, n.faces), poisonTier(n.tier)->name);
  else if (kind == "curseOfDecay") s += std::format("\n{} · -{}% de todos os atributos por turno até -{}% · {} por turno · {} turnos", dmg(n.mul, n.dice, n.faces), n.spread, n.pct, dmg(n.tickMul, n.tickDice, n.tickFaces), n.turns);
  else if (kind == "soulHarvest") s += std::format("\nraio {} · {} em cada inimigo · venenos, sangrias e maldições batem mais uma vez · cura {}% do HP por inimigo", n.radius, dmg(n.mul, n.dice, n.faces), n.pct);
  else if (kind == "devouringVoid") s += std::format("\n{} · -{} de resistência a Escuridão pelo resto da batalha · abaixo de {}% do HP, morre", dmg(n.mul, n.dice, n.faces), n.spread, n.pct);
  else if (kind == "pillarOfFlame") s += std::format("\nraio {} · {} · o chão arde {}D{} por {} rodada{}", n.radius, dmg(n.mul, n.dice, n.faces), n.hazardDice, n.hazardFaces, n.rounds, n.rounds > 1 ? "s" : "");
  else if (kind == "iceLance") s += std::format("\nreta até a borda · {} · os seguintes recebem {}%", dmg(n.mul, n.dice, n.faces), n.pct);
  else if (kind == "thunderstorm") s += std::format("\nraio {} · {} · cada atingido ecoa {}D{}+{} num vizinho", n.radius, dmg(n.mul, n.dice, n.faces), n.tickDice, n.tickFaces, n.bonus);
  else if (kind == "meteor") s += std::format("\nraio {} · cai no início do seu próximo turno · centro {} · anel {}", n.radius, dmg(n.mul, n.dice, n.faces), dmg(n.ringMul, n.ringDice, n.ringFaces));
  else if (kind == "absoluteZero") s += std::format("\nraio {} · {} · ×1.5 dentro de uma Tempestade de Gelo", n.radius, dmg(n.mul, n.dice, n.faces));
  else if (kind == "chainLightning") s += std::format("\n{} · salta até {} vezes (a {} hexes), -{}% por salto", dmg(n.mul, n.dice, n.faces), n.jumps, n.length, static_cast<int>(n.decay * 100));
  else if (kind == "eruption") s += std::format("\nem volta de si, raio {} · {} em cada inimigo · o anel arde {} rodadas", n.radius, dmg(n.mul, n.dice, n.faces), n.rounds);
  else if (kind == "glacialGale") s += std::format("\ncone de {} hexes (os dois lados) · {} · empurra {} hex", n.length, dmg(n.mul, n.dice, n.faces), n.push);
  else if (kind == "wrathOfTheSkies") s += std::format("\n{} raios à escolha · {} cada · eco 2D12+8", n.count, dmg(n.mul, n.dice, n.faces));
  else if (kind == "primordialEmber") s += std::format("\nraio {} (os dois lados) · {} · o chão fica em brasa {}D{} por {} rodadas", n.radius, dmg(n.mul, n.dice, n.faces), n.hazardDice, n.hazardFaces, n.rounds);
  return s;
}

std::string spellName(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s ? s->name : kind;
}

bool spellPorted(const std::string& kind) {
  return kind == "magicMissile" || kind == "longShot" || kind == "piercing" || kind == "piercingThrust" || kind == "doubleStrike" || kind == "lightning" ||
         kind == "frost" || kind == "cureMinor" || kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "createFoodAndWater" ||
         emberSpell(kind) != nullptr;
}

bool spellWeaponAbility(const std::string& kind) { return weaponAbility(kind); }

bool spellInstant(const std::string& kind) { return kind == "createFoodAndWater" || kind == "eruption"; }

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
  u.maxHp = r((g[0] + u.gearHp) * keep);
  const BattleUnit::Stats base{r((g[1] + u.gearAtk) * keep), r((g[2] + u.gearMag) * keep), r((g[3] + u.gearDef) * keep), r((g[4] + u.gearDex) * keep),
                               g[15] + u.gearMov};
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
  u.maxHp = r((g[0] + u.gearHp) * keep);
  u.atk = r((g[1] + u.gearAtk) * keep * sick), u.mag = r((g[2] + u.gearMag) * keep * sick);
  u.def = r((g[3] + u.gearDef) * keep * sick), u.dex = r((g[4] + u.gearDex) * keep * sick);
  u.mov = u.diseased ? std::max(1, r((g[15] + u.gearMov) * sick)) : g[15] + u.gearMov;
  u.diseaseBase = u.diseased ? std::optional(BattleUnit::Stats{r((g[1] + u.gearAtk) * keep), r((g[2] + u.gearMag) * keep), r((g[3] + u.gearDef) * keep),
                                                               r((g[4] + u.gearDex) * keep), g[15] + u.gearMov})
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
  if (kind == "cureMinor" || kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "bless") return "heal";
  if (kind == "longShot" || kind == "bloodyShot" || kind == "multiShot" || kind == "piercing" || kind == "doubleStrike" || kind == "piercingThrust" ||
      kind == "cleave" || kind == "sweep" || kind == "trip")
    return "attack";
  return "cast";
}

int spellUsesLeft(const Battle& b, size_t unit, const std::string& kind) {
  const BattleUnit& u = b.units()[unit];
  if (kind == "shock") return u.shockCharges;
  if (kind == "fantomForce") return u.fantomForceCharges;
  return tierRemaining(u, kind == "magicMissileV2" ? "magicMissile" : kind);
}

int spellTargetCount(const Battle& b, size_t unit, const std::string& kind) {
  if (kind == "wrathOfTheSkies") return emberNumbers(kind, b.units()[unit].level).count;
  return kind == "magicMissile" || kind == "magicMissileV2" ? magicMissileCount(b.units()[unit].level) : 1;
}

std::vector<BattleHex> spellZone(const Battle& b, size_t caster, const std::string& kind) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  std::vector<BattleHex> zone;
  if (spellInstant(kind)) return zone;
  int minRange = 0, maxRange = -1;
  bool shot = false, arrow = false;
  if (kind == "longShot") minRange = u.minRange, maxRange = kLongShotRange, shot = true, arrow = true;
  else if (kind == "lightning") maxRange = kLightningRange;
  else if (kind == "shock") maxRange = kShockRange;
  else if (kind == "magicMissile" || kind == "magicMissileV2") maxRange = kMissileRange, shot = true;
  else if (kind == "doubleStrike") minRange = u.minRange, maxRange = u.maxRange, shot = u.maxRange > 1, arrow = true;
  else if (const CureData* cure = cureData(kind)) maxRange = cure->range;
  else if (kind == "cureDisease") maxRange = kCureDiseaseRange;
  else if (emberBoltSpell(kind) || emberAreaSpell(kind)) maxRange = emberNumbers(kind, u.level).range;  // ours: no line of fire needed
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
    if (d > kLongShotRange) return fail("Alvo fora de alcance (máximo 7 hexes).");
    return clearShot(b, from, cell, true) ? true : fail("O tiro está bloqueado.");
  }
  if (kind == "piercing") return piercingRay(b, from, cell).empty() ? fail("Escolha uma reta válida.") : true;
  if (kind == "piercingThrust") return piercingThrustRay(b, u, cell).empty() ? fail("Escolha uma reta na frente.") : true;
  if (kind == "lightning" || kind == "shock") {
    if (!targetable(b, here)) return fail(here < 0 ? "Não há unidade na casa selecionada." : "Escolha um inimigo visível.");
    return d <= (kind == "lightning" ? kLightningRange : kShockRange) ? true : fail("Alvo fora de alcance (máximo 5 hexes).");
  }
  if (kind == "magicMissile" || kind == "magicMissileV2") {
    // magicMissileTargetError, then targetable
    if (here < 0 || !b.units()[static_cast<size_t>(here)].alive || b.units()[static_cast<size_t>(here)].hidden) return fail("Não há inimigo visível nessa casa.");
    const std::string& side = b.units()[static_cast<size_t>(here)].side;
    if (!b.debugFreeCast() && side != "enemy" && side != "neutral") return fail("Míssil Mágico não pode mirar em aliados.");
    if (d > kMissileRange) return fail("Alvo fora de alcance (máximo 5 hexes).");
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
    if (d > cure->range) {
      const std::string why = std::format("Alvo fora de alcance (máximo {} hexes).", cure->range);
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
    if (d > kCureDiseaseRange) return fail("Alvo fora de alcance (máximo 2 hexes).");
    if (here < 0 || !b.units()[static_cast<size_t>(here)].alive) return fail("Escolha uma aliada na casa selecionada.");
    const BattleUnit& w = b.units()[static_cast<size_t>(here)];
    if (b.debugFreeCast()) return true;
    if (w.side != "player") return fail("A cura de doença só pode ser usada em uma aliada.");
    return w.diseased || w.poisoned ? true : fail("Essa aliada não está doente nem envenenada.");
  }
  if (kind == "createFoodAndWater") return cell == from ? true : fail("Esta magia não precisa de alvo.");  // no aim: self-centred
  // EmberEngine's own spells
  if (emberSpell(kind)) {
    const EmberNumbers n = emberNumbers(kind, u.level);
    if (kind == "eruption") return cell == from ? true : fail("Esta magia não precisa de alvo.");
    if (kind == "iceLance") return piercingRay(b, from, cell).empty() ? fail("Escolha uma reta válida.") : true;
    if (kind == "glacialGale") return coneHexes(b, from, cell, n.length).empty() ? fail("Escolha uma direção a partir do personagem.") : true;
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
    if (kind == "eruption") return hexesWithin(b, {u.x, u.y}, n.radius);
    if (emberAreaSpell(kind)) return hexesWithin(b, cell, n.radius);
  }
  return {cell};
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
    s.weaponBonusDice = p.dice, s.weaponBonusFaces = p.faces, s.weaponBonusBonus = 0;
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
    s.dice = kLightningDice, s.faces = kLightningFaces, s.bonus = kLightningBonus, s.spellMul = kLightningMul, s.echo = kLightningEcho;
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
      m.dice = kMissileDice, m.faces = kMissileFaces, m.bonus = kMissileBonus, m.spellMul = kMissileMul;
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
  if (!u.alive || !t.alive) return false;
  const Fighter a = fighterAt(b, step.att, u.x, u.y), d = fighterAt(b, step.def, t.x, t.y);
  return canCounter(b, a, {u.x, u.y}, d);
}

namespace {

// The combat log's lines for the effects a step just produced (Engine2 engine.ts pushLog): `actor` is who acted (-1 for
// what befalls a unit at the start of its turn, which carries its own verb in `cause`).
void logEffects(Battle& b, const std::vector<BattleEffect>& out, size_t from, int actor, const BattleStep* step) {
  const auto nameOf = [&](size_t i) { return b.units()[i].name; };
  const std::string who = actor >= 0 ? nameOf(static_cast<size_t>(actor)) : std::string();
  for (size_t k = from; k < out.size(); ++k) {
    const BattleEffect& e = out[k];
    const std::string target = nameOf(e.unit);
    const bool en = gLogEnglish;
    const std::string crit = e.crit ? (en ? " (critical)" : " (crítico)") : "";
    if (!e.cause.empty()) {
      b.pushLog(en ? std::format("{} {}: {} damage", e.cause, target, e.amount) : std::format("{} {}: {} dano", e.cause, target, e.amount));
    } else if (e.miss) {
      b.pushLog(en ? std::format("{} attacked {}: Missed", who, target) : std::format("{} atacou {}: Missed", who, target));
    } else if (e.heal) {
      if (step && step->type == BattleStep::Type::Potion)
        b.pushLog(e.text.empty() ? (en ? std::format("{} used {} on {} and recovered {} HP.", who, potionName(step->potion), target, e.amount)
                                       : std::format("{} usou {} em {} e recuperou {} HP.", who, potionName(step->potion), target, e.amount))
                                 : (en ? std::format("{} used {} on {}: {}", who, potionName(step->potion), target, e.text)
                                       : std::format("{} usou {} em {}: {}", who, potionName(step->potion), target, e.text)));
      else
        b.pushLog(e.text.empty() ? (en ? std::format("{} healed {}: +{} HP", who, target, e.amount) : std::format("{} curou {}: +{} HP", who, target, e.amount))
                                 : std::format("{}: {}", target, e.text));
    } else if (e.status) {
      b.pushLog(std::format("{}: {}", target, e.text));
    } else if (step && step->type == BattleStep::Type::Spell) {
      b.pushLog(en ? std::format("{} hit {} with magic: {} damage{}", who, target, e.amount, crit)
                   : std::format("{} atingiu {} com magia: {} dano{}", who, target, e.amount, crit));
    } else {
      b.pushLog(en ? std::format("{} attacked {}: {} damage{}", who, target, e.amount, crit) : std::format("{} atacou {}: {} dano{}", who, target, e.amount, crit));
    }
    if (e.died) b.pushLog(en ? std::format("{} was defeated.", target) : std::format("{} foi derrotado.", target));
  }
}

void flushNotes(Battle& b) {
  for (std::string& n : gNotes) b.pushLog(std::move(n));
  gNotes.clear();
}

void resolveStrikeImpl(Battle& b, const BattleStep& step, bool counter, Mulberry32& rng, std::vector<BattleEffect>& out) {
  const size_t ai = counter ? step.def : step.att, ti = counter ? step.att : step.def;
  BattleUnit& actor = b.units()[ai];
  BattleUnit& target = b.units()[ti];
  // The counter's off-hand weapon: its own dice when the attacker stands within its reach.
  CustomDice offDice;
  const bool off = counter && actor.combat.offHandWeapon && hexDist({actor.x, actor.y}, {target.x, target.y}) <= actor.combat.offMaxRange;
  if (off) offDice = {actor.combat.offDice, actor.combat.offFaces, actor.combat.offBonus};
  const bool hitStage = !counter;
  const bool arrowShot = actor.combat.arrow && !off;
  const bool arcaneBolt = !arrowShot && actor.combat.arcane;
  const Fighter A = fighterAt(b, ai, actor.x, actor.y), T = fighterAt(b, ti, target.x, target.y);
  Hit hit = rollDamage(b, A, T, rng, true, off ? &offDice : nullptr);
  if (target.side == "enemy") trainWeapon(actor, target.level, rng, off);  // the off-hand weapon on an off-hand counter
  const bool usesArcane = arcaneBolt && !off && (counter || step.kind.empty());
  const bool usesStaff = actor.combat.staff && !off && (counter || step.kind.empty());
  if (!hit.landed) {
    out.push_back({ti, 0, false, true, false, false, counter});
    return;
  }
  int dmg = hit.dmg;
  if (hitStage && step.bonusDice > 0) dmg += rollDice(step.bonusDiceCount, step.bonusDice, step.bonusFlat, rng);
  if (hitStage && step.dmgMul != 1) dmg = static_cast<int>(std::max(1.0, std::floor(dmg * step.dmgMul)));
  dmg = std::max(1, dmg);  // zoneDamageMul (no zones yet)
  if (usesArcane || usesStaff) {
    const std::string element = usesStaff ? actor.combat.staffElement : "arcane";
    const double staffed = dmg * (1 + (usesStaff ? staffBonus(actor.combat, element) : 0) / 100);
    dmg = static_cast<int>(std::floor(elementalDamage(staffed, target.combat.resistance(element), A.mag)));
    trainResistance(target, element, actor.level, rng);
  }
  dmg = marked(target, dmg);  // Marca da Ruína
  const int actual = std::min(target.hp, dmg);
  target.hp = std::max(0, target.hp - dmg);
  if (target.side != actor.side && !off) staffDrain(actor, actual);
  b.noteDamage(ai, ti, dmg, false);
  if (target.side != actor.side) {
    if (hitStage) gainExp(actor, target.level, dmg, 1, target.hp <= 0);
    else gainCounterExp(actor);
  }
  BattleEffect e{ti, dmg, hit.crit, false, false, false, counter};
  if (target.hp <= 0) {
    markDead(target, rng);
    e.died = true;
  } else if (hitStage) {
    maybeInflictDisease(actor, target, rng);
  }
  out.push_back(e);
  if (!target.alive) afterDeath(b, ti, out);
}

void resolveStepImpl(Battle& b, const BattleStep& step, Mulberry32& rng, std::vector<BattleEffect>& out);

}  // namespace

void setCombatLogEnglish(bool english) { gLogEnglish = english; }

void resolveStrike(Battle& b, const BattleStep& step, bool counter, Mulberry32& rng, std::vector<BattleEffect>& out) {
  gNotes.clear();
  const size_t from = out.size();
  resolveStrikeImpl(b, step, counter, rng, out);
  logEffects(b, out, from, static_cast<int>(counter ? step.def : step.att), &step);
  flushNotes(b);
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
  if (step.type == BattleStep::Type::Heal) {
    BattleUnit& target = b.units()[step.def];
    if (!att.alive || !target.alive) return;
    const Fighter A = fighterAt(b, step.att, att.x, att.y);
    // rollCure (the cure's own numbers), then healingAmount (+1% per healing skill point)
    const CureData& c = *cureData(step.kind);
    const int base = static_cast<int>(std::floor(std::floor(A.mag / 2) * c.mul + rollDice(c.dice, c.faces, c.bonus, rng)));
    int heal = healingPower(att, base);
    if (target.cond.bleedTurns > 0) heal /= 2;  // Hemorragia: heals received halved
    const int gained = std::min(heal, target.maxHp - target.hp);
    target.hp += gained;
    if (gained > 0) {
      trainHealing(att, rng);
      b.noteAware(step.att, gained * kHealCe, gained * kHealVe);
    }
    gainExp(att, target.level, gained);
    out.push_back({step.def, gained, false, false, true, false, false});
    return;
  }
  if (step.type == BattleStep::Type::CureDisease) {  // stepCureDisease
    BattleUnit& target = b.units()[step.def];
    if (!att.alive || !target.alive) return;
    curePlayerDisease(target);
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
    if (gained > 0) trainHealing(att, rng);
    gainExp(att, target.level, gained);
    att.bag[slot] -= 1;
    out.push_back({step.def, gained, false, false, true, false, false});
    return;
  }
  // A spell's hits (stepSpell). The caster turns to its first target, every foe it hits turns to the caster.
  if (!att.alive) return;
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
  const bool aoe = step.kind == "piercing" || step.kind == "piercingThrust" || (ember && step.ids.size() > 1);
  bool firstAoe = true;
  int thrustHits = 0, enemiesStruck = 0, drained = 0;
  std::vector<size_t> struck;  // ours: who took the hit and stands (Colheita's extra ticks)
  const bool ability = weaponAbility(step.kind);
  for (size_t k = 0; k < step.ids.size(); ++k) {
    const size_t id = step.ids[k];
    BattleUnit& foe = b.units()[id];
    if (!foe.alive) continue;
    if (b.cell(foe.x, foe.y).terrain == "barricade") continue;  // "bloqueado"
    int dmg = 0;
    bool crit = false, landed = true, weaponRolled = false;
    if (step.dice > 0) {
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
        const double accuracy = std::max(0.0, std::min(100.0, att.combat.accuracy - fighterAt(b, id, foe.x, foe.y).dex));
        landed = accuracy >= 100 || rng() * 100 < accuracy;
      }
      if (foe.side == "enemy") trainWeapon(att, foe.level, rng);
    }
    if (!landed) {
      out.push_back({id, 0, false, true, false, false, false});
      continue;
    }
    if (step.kind == "fantomForce") dmg = std::max(1, static_cast<int>(std::floor(dmg * kFantomForceMul)));
    if (step.dmgMul > 1) dmg = std::max(1, static_cast<int>(std::floor(dmg * step.dmgMul)));
    if (k < step.perTargetMul.size() && step.perTargetMul[k] != 1.0) dmg = std::max(1, static_cast<int>(std::floor(dmg * step.perTargetMul[k])));
    dmg = std::max(1, dmg);  // zoneDamageMul (no zones yet)
    if (!element.empty()) {
      const double staffed = dmg * (1 + staffBonus(att.combat, element) / 100);
      dmg = static_cast<int>(std::floor(elementalDamage(staffed, foe.combat.resistance(element), fighterAt(b, step.att, att.x, att.y).mag)));
    }
    dmg = marked(foe, dmg);  // Marca da Ruína
    const int actual = std::min(foe.hp, dmg);
    foe.hp = std::max(0, foe.hp - dmg);
    if (!element.empty() && foe.side != att.side) staffDrain(att, actual);
    b.noteDamage(step.att, id, dmg, !ability);
    if (!element.empty()) trainResistance(foe, element, att.level, rng);
    if (foe.side != att.side) {
      const bool kill = foe.hp <= 0;
      const bool mageClass = att.classId == "mage" || att.classId == "voss" || att.classId == "conjurer" || att.classId == "warlock" || att.classId == "elementalist";
      const bool doubled = kill && !aoe && (mageClass || step.kind == "longShot");
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
    } else if (step.echo) {
      BattleUnit::Echo echo = *step.echo;
      echo.mag = fighterAt(b, step.att, att.x, att.y).mag;
      foe.shock = echo;
    }
    // Vendaval Glacial: the struck are pushed away from the caster (their card follows: `pushed`).
    if (foe.alive && step.push > 0 && pushAway(b, id, {att.x, att.y}, step.push)) e.pushed = true;
    out.push_back(e);
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
  // applyTileHazard: flames and embers hurt whoever starts its turn on them.
  const BattleCell& c = b.cell(u.x, u.y);
  if (u.alive && c.hazardDice > 0) {
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
  // --- EmberEngine's own: the zones it stands in, its conditions, the Meteoro it called, the countdowns ---
  if (u.alive) zoneEffects(b, unit, rng, out);
  if (u.alive) tickConditions(b, unit, rng, out, true);
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
  const bool cultist = c == "cultist" || c == "cultistV2" || c == "emberedWraith" || c == "swampBlueCalf";
  if (cultist && (u.spells[0] > 0 || u.spells[1] > 0 || u.shockCharges > 0 || u.fantomForceCharges > 0)) {
    if (u.spells[1] > 0)
      if (const std::optional<Pick> p = best(kLightningRange, false)) {
        spendTier(u, "lightning");
        return single(*p, "lightning", kLightningDice, kLightningFaces, kLightningBonus, kLightningMul, kLightningEcho);
      }
    if (u.fantomForceCharges > 0)
      if (const std::optional<Pick> p = best(kFantomForceRange, true)) {
        u.fantomForceCharges = std::max(0, u.fantomForceCharges - 1);
        return single(*p, "fantomForce", u.fantomDice, u.fantomFaces, 0, 1, std::nullopt);
      }
    if (std::optional<AiCast> s = shock()) return s;
    if (u.spells[0] > 0)
      if (const std::optional<Pick> p = best(kMissileRange, true)) {
        spendTier(u, "magicMissile");
        return single(*p, "magicMissile", kMissileDice, kMissileFaces, kMissileBonus, kMissileMul, std::nullopt);
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
  const bool birolho = c == "birolho" || c == "birolho2" || c == "birolho3" || c == "birolhoLegs" || c == "birolhoLegs2";
  // Any other enemy mage still gets Choque.
  if (!cultist && !birolho && c != "cultistV2" && u.shockCharges > 0)
    if (std::optional<AiCast> s = shock()) return s;
  // Crossbowmen: Tiro Perfurante while banked, else Tiro Longo.
  if ((c == "brigand" || c == "baseCrossbowman") && (u.spells[0] > 0 || u.spells[1] > 0)) {
    const bool piercing = u.spells[1] > 0;
    std::optional<Pick> p;
    for (const ReachCell& cell : reach)
      for (size_t h : heroes) {
        const BattleUnit& f = b.units()[h];
        if (!piercing) {
          const int d = hexDist({cell.x, cell.y}, {f.x, f.y});
          if (d < u.minRange || d > kLongShotRange) continue;
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
        s.weaponBonusDice = power.dice, s.weaponBonusFaces = power.faces, s.weaponBonusBonus = 0;
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
