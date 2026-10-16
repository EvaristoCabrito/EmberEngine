#include "world/Spells.h"

#include <algorithm>
#include <cmath>
#include <format>

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
const SpellData* spellData(const std::string& kind) {
  for (const SpellData& s : kSpells)
    if (kind == s.kind) return &s;
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

}  // namespace

// --- Public ----------------------------------------------------------------------------------------------------------

std::string spellName(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s ? s->name : kind;
}

bool spellPorted(const std::string& kind) {
  return kind == "magicMissile" || kind == "longShot" || kind == "piercing" || kind == "piercingThrust" || kind == "doubleStrike" || kind == "lightning" ||
         kind == "frost" || kind == "cureMinor" || kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "createFoodAndWater";
}

bool spellInstant(const std::string& kind) { return kind == "createFoodAndWater"; }

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
  return kind == "magicMissile" || kind == "magicMissileV2" ? magicMissileCount(b.units()[unit].level) : 1;
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
  return fail("Esta habilidade ainda não existe aqui.");
}

std::vector<BattleHex> spellArea(const Battle& b, size_t caster, const std::string& kind, BattleHex cell) {
  const BattleUnit& u = b.units()[caster];
  if (kind == "frost") return frostTiles(b, {u.x, u.y}, cell, u.level);
  if (kind == "piercing") return piercingRay(b, {u.x, u.y}, cell);
  if (kind == "piercingThrust") return piercingThrustRay(b, u, cell);
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
  if (static_cast<int>(cells.size()) != spellTargetCount(b, caster, kind)) return fail("Escolha todos os alvos.");
  if (checkUses && u.side == "player" && !contains(u.spellList, kind)) return fail("A classe ou o nível não permite esta magia.");
  if (checkUses && spellUsesLeft(b, caster, kind) <= 0) return fail("Sem usos restantes.");
  for (const BattleHex& c : cells)
    if (!spellAimValid(b, caster, kind, c, error)) return std::nullopt;
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

void resolveStrike(Battle& b, const BattleStep& step, bool counter, Mulberry32& rng, std::vector<BattleEffect>& out) {
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
}

void resolveStep(Battle& b, const BattleStep& step, Mulberry32& rng, std::vector<BattleEffect>& out) {
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
    const int heal = healingPower(att, base);
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
    const int heal = healingPower(att, rollDice(p->dice, p->faces, p->bonus, rng));
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
  const bool aoe = step.kind == "piercing" || step.kind == "piercingThrust";
  bool firstAoe = true;
  int thrustHits = 0;
  const bool ability = weaponAbility(step.kind);
  for (size_t id : step.ids) {
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
    dmg = std::max(1, dmg);  // zoneDamageMul (no zones yet)
    if (!element.empty()) {
      const double staffed = dmg * (1 + staffBonus(att.combat, element) / 100);
      dmg = static_cast<int>(std::floor(elementalDamage(staffed, foe.combat.resistance(element), fighterAt(b, step.att, att.x, att.y).mag)));
    }
    const int actual = std::min(foe.hp, dmg);
    foe.hp = std::max(0, foe.hp - dmg);
    if (!element.empty() && foe.side != att.side) staffDrain(att, actual);
    b.noteDamage(step.att, id, dmg, !ability);
    if (!element.empty()) trainResistance(foe, element, att.level, rng);
    if (foe.side != att.side) {
      const bool kill = foe.hp <= 0;
      const bool doubled = kill && !aoe && (att.classId == "mage" || att.classId == "voss" || att.classId == "conjurer" || step.kind == "longShot");
      const double mul = doubled ? 2 : aoe && !firstAoe ? 0.5 : 1;
      if (aoe) firstAoe = false;
      gainExp(att, foe.level, dmg, mul, kill);
    }
    BattleEffect e{id, dmg, crit, false, false, false, false};
    if (foe.hp <= 0) {
      markDead(foe, rng);
      e.died = true;
    } else if (step.echo) {
      BattleUnit::Echo echo = *step.echo;
      echo.mag = fighterAt(b, step.att, att.x, att.y).mag;
      foe.shock = echo;
    }
    out.push_back(e);
  }
}

void startOfTurnEffects(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out) {
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
    if (u.hp <= 0) markDead(u, rng), e.died = true;
    out.push_back(e);
  }
  // applyTileHazard: flames and embers hurt whoever starts its turn on them.
  const BattleCell& c = b.cell(u.x, u.y);
  if (u.alive && c.hazardDice > 0) {
    int dmg = 0;
    for (int i = 0; i < c.hazardDice; ++i) dmg += 1 + static_cast<int>(std::floor(rng() * c.hazardFaces));
    const std::string element = c.terrain == "flame" ? "fire" : c.terrain == "ember" ? "ember" : "";
    if (!element.empty()) dmg = static_cast<int>(std::floor(elementalDamage(dmg, u.combat.resistance(element), 0)));
    u.hp = std::max(0, u.hp - dmg);
    if (!element.empty()) trainResistance(u, element, std::nullopt, rng);
    BattleEffect e{unit, dmg, false, false, false, false, false};
    if (u.hp <= 0) markDead(u, rng), e.died = true;
    out.push_back(e);
  }
}

std::optional<AiCast> planEnemyCast(Battle& b, size_t unit) {
  BattleUnit& u = b.units()[unit];
  if (!u.alive || u.side != "enemy") return std::nullopt;
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
