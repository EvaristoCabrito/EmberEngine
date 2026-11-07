#include "world/Combat.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ember {

namespace {

constexpr double kSqrt3 = 1.7320508075688772;  // Math.sqrt(3)

// JavaScript's Math.round: halves go up (towards +infinity).
double jsRound(double v) { return std::floor(v + 0.5); }

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

}  // namespace

// pathfinding.ts hexLine: the hexes a straight line from a to b crosses, a first.
std::vector<BattleHex> hexLine(BattleHex a, BattleHex b) {
  const int n = Battle::distance(a, b);
  if (n == 0) return {a};
  const Cube A = oddrToCube(a.x, a.y), B = oddrToCube(b.x, b.y);
  std::vector<BattleHex> out;
  for (int i = 0; i <= n; ++i) {
    const double t = static_cast<double>(i) / n;
    const Cube c = cubeRound(A.q + (B.q - A.q) * t, A.r + (B.r - A.r) * t, A.s + (B.s - A.s) * t);
    const BattleHex p = cubeToOddr(static_cast<int>(c.q), static_cast<int>(c.r));
    if (!out.empty() && out.back() == p) continue;
    out.push_back(p);
  }
  return out;
}

namespace {

// The cell as hexDef reads it: with the decorations folded in, or the bare tile (an empty overlay).
int heightOf(const BattleCell& c, bool decorations) { return decorations ? c.shot.height : c.raw.height; }
bool blocksShotOf(const BattleCell& c, bool decorations) { return decorations ? c.shot.blocksShot : c.raw.blocksShot; }

// hexDef's read: the row-major index into the board's cells (so a column off one side lands on the next row, as
// there), plains past either end.
const BattleCell& cellByIndex(const Battle& b, BattleHex p) {
  static const BattleCell kPlains = [] {
    BattleCell c;
    c.terrain = "plains";
    return c;
  }();
  const int i = p.y * b.cols() + p.x;
  return i >= 0 && i < b.cols() * b.rows() ? b.cell(i % b.cols(), i / b.cols()) : kPlains;
}

}  // namespace

// pathfinding.ts shotBlocker == null.
bool clearShot(const Battle& b, BattleHex from, BattleHex to, bool arrow, bool decorations) {
  const bool fromHigh = heightOf(cellByIndex(b, from), decorations) != 0;
  const double fromLevel = cellByIndex(b, from).level;
  const std::vector<BattleHex> line = hexLine(from, to);
  for (size_t i = 1; i < line.size(); ++i) {
    const BattleHex p = line[i];
    const bool end = i == line.size() - 1;
    const BattleCell& t = cellByIndex(b, p);
    // An arrow from higher ground (the board's real elevation) flies over whatever stands on a lower cell: tall terrain, a
    // decoration, a barricade (the user, 2026-10-22: "o arqueiro em terreno mais alto atira por cima do que está no caminho").
    if (arrow && !end && t.level < fromLevel) continue;
    if (t.terrain == "barricade") {
      if (end) return false;
      const bool shooterBehind = Battle::distance(from, p) <= 1, targetBehind = Battle::distance(to, p) <= 1;
      if (targetBehind || !shooterBehind) return false;
      continue;
    }
    if (blocksShotOf(t, decorations) && !end) return false;
    if (!end && arrow && heightOf(t, decorations) && !fromHigh) return false;
  }
  return true;
}

namespace {

// pathfinding.ts canHitFrom for a unit with these ranges and magic, standing at `from`, against a foe standing at `at`.
bool hitFrom(const Battle& b, const BattleUnit& u, int minRange, int maxRange, double mag, BattleHex from, const BattleUnit& foe, BattleHex at,
             bool decorations, bool fullFootprint) {
  const int max = maxRange + (u.combat.ranged && b.cell(from.x, from.y).raw.height ? 1 : 0);  // effectiveMaxRange
  std::vector<BattleHex> cells;
  if (fullFootprint || !u.footprintOffsets) {
    cells = Battle::footprint(u, from.x, from.y);
  } else {
    for (const BattleHex& o : *u.footprintOffsets)
      if (o.y == 0) cells.push_back({from.x + o.x, from.y});
  }
  const std::vector<BattleHex> body = Battle::footprint(foe, at.x, at.y);
  bool ok = false;
  for (const BattleHex& p : cells) {
    int best = 999;
    for (const BattleHex& f : body) best = std::min(best, Battle::distance(p, f));
    if (best >= minRange && best <= max) {
      ok = true;
      break;
    }
  }
  if (!ok) return false;
  if (maxRange <= 1) return true;  // shotKind: no shot
  return clearShot(b, from, at, mag == 0, decorations);  // a ranged weapon's arrow, else a bolt
}

const BattleCell::Terrain& tileAt(const Battle& b, int x, int y) { return b.cell(x, y).raw; }

}  // namespace

double powerOf(const Fighter& f) { return std::floor((f.magicBlows() ? f.mag : f.atk) / 2); }

namespace {

double protOf(const Fighter& a, const Fighter& d) { return std::floor((a.magicBlows() ? 0 : d.def) / 2); }

// combat.ts terrainBonus: the defender's ground (and cover from shots), the attacker's high ground (+10% of its stat).
std::pair<double, double> terrainBonus(const Battle& b, const Fighter& a, const BattleCell::Terrain& att, const BattleCell::Terrain& def) {
  double defence = def.def;
  if (b.units()[a.unit].maxRange > 1 && def.cover) defence += def.cover;
  const double attack = att.height ? jsRound((a.magicBlows() ? a.mag : a.atk) * 0.1) : att.atk;
  return {attack, defence};
}

// An archer's blow from higher ground (Battle.h kHighGroundHit / kHighGroundAtkPct): a bow or crossbow (not the off-hand) on a
// cell whose real elevation is above the defender's.
bool archerHighGround(const Battle& b, const Fighter& a, const Fighter& d, bool offHand) {
  if (offHand || !b.units()[a.unit].combat.arrow || a.magicBlows()) return false;
  if (!b.inBounds(a.x, a.y) || !b.inBounds(d.x, d.y)) return false;
  return b.cell(a.x, a.y).level > b.cell(d.x, d.y).level;
}

// combat.ts isRearAttack: an adjacent attacker on one of the two hexes behind the defender's heading.
bool isRearAttack(const Fighter& a, const Fighter& d) {
  if (Battle::distance({a.x, a.y}, {d.x, d.y}) != 1) return false;
  const double dx = d.faceDx ? *d.faceDx : static_cast<double>(d.facing);
  const double dy = d.faceDy ? *d.faceDy : 0.0;
  if (std::hypot(dx, dy) < 1e-6) return false;
  const double rear = std::atan2(-dy, -dx);
  const double originX = d.x + (d.y & 1) * 0.5;
  struct Sector {
    BattleHex cell;
    double distance;
  };
  std::vector<Sector> sectors;
  for (const BattleHex& c : Battle::neighbors(d.x, d.y)) {
    const double angle = std::atan2((c.y - d.y) * kSqrt3 / 2, c.x + (c.y & 1) * 0.5 - originX);
    sectors.push_back({c, std::abs(std::atan2(std::sin(angle - rear), std::cos(angle - rear)))});
  }
  std::stable_sort(sectors.begin(), sectors.end(), [](const Sector& p, const Sector& q) {
    const double r = jsRound((p.distance - q.distance) * 1e9);
    if (r != 0) return r < 0;
    if (p.cell.y != q.cell.y) return p.cell.y < q.cell.y;
    return p.cell.x < q.cell.x;
  });
  for (size_t i = 0; i < 2 && i < sectors.size(); ++i)
    if (sectors[i].cell.x == a.x && sectors[i].cell.y == a.y) return true;
  return false;
}

}  // namespace

double elementalDamage(double base, double resistance, double mag) {
  const double effective = std::max(-50.0, std::min(100.0, resistance - 0 - mag / 2));
  return std::max(0.0, base) * (1 - effective / 100);
}

namespace {

struct Preview {
  int dmg = 0;
  double hitChance = 0;
};

// weaponModifiers' blessedHitBonusPct * 100: an enemy always has it, a hero only when its weapon's mastery applies.
double blessBonus(const Battle& b, const Fighter& f, bool offHand) {
  const BattleUnit& u = b.units()[f.unit];
  if (u.blessedHitBonusPct <= 0) return 0;
  if (u.side == "enemy") return u.blessedHitBonusPct * 100;
  return (offHand ? u.offTrainsWeapon : u.trainsWeapon) ? u.blessedHitBonusPct * 100 : 0;
}

}  // namespace

double weaponAccuracy(const Battle& b, const Fighter& f, bool offHand) {
  const BattleUnit& u = b.units()[f.unit];
  // weaponModifiers: an enemy's is 90 + its DEX as it is now (the battle file's number was its DEX when exported, and DEX
  // changes in a fight: level growth, disease, decay); a hero's is 75 + the weapon's skill (100 for a weapon its class does
  // not train). The user's own rule (2026-10-07): the attacker's DEX is no bonus for a hero.
  if (u.side == "enemy" && u.rank.empty()) return 90.0 + u.dex + blessBonus(b, f, offHand);  // a ranked enemy: its rank's mastery
  if (u.side == "enemy") return (offHand ? u.combat.offAccuracy : u.combat.accuracy) + blessBonus(b, f, offHand);
  return (offHand ? u.combat.offAccuracy : u.combat.accuracy) + blessBonus(b, f, offHand);
}

namespace {

// combat.ts previewDamage (the weapon's average roll).
Preview previewDamage(const Battle& b, const Fighter& a, const Fighter& d, const BattleCell::Terrain& att, const BattleCell::Terrain& def,
                      bool offHand, bool useSkill = true) {
  const CombatProfile& p = b.units()[a.unit].combat;
  const auto [bonusAtk, bonusDef] = terrainBonus(b, a, att, def);
  const bool rear = useSkill && isRearAttack(a, d);
  // weaponModifiers, or { accuracy 100, damage 1 } for a blow that does not use the weapon's mastery (a shield's bash)
  const double accuracy = useSkill ? weaponAccuracy(b, a, offHand) : 100, damageMul = useSkill ? (offHand ? p.offDamageMul : p.damageMul) : 1;
  double weapon = 0;
  if (offHand) {
    if (p.offHandWeapon) weapon = p.offDice * (p.offFaces + 1.0) / 2 * damageMul + p.offBonus;
  } else if (p.hasWeapon) {
    weapon = p.dice * (p.faces + 1.0) / 2 * damageMul + p.bonus + p.enh;
  }
  const bool high = archerHighGround(b, a, d, offHand);
  const double chance = useSkill ? hitChance(a, d, accuracy + (high ? kHighGroundHit : 0), rear) : 100.0;
  const double highAtk = high ? jsRound(a.atk * kHighGroundAtkPct / 100) : 0;
  const double raw = powerOf(a) + weapon + bonusAtk + highAtk - protOf(a, d) - bonusDef;
  double dmg = std::max(1.0, std::floor(std::max(1.0, raw) * (offHand ? 1 : (p.classBonus ? 1.1 : 1)) * (rear ? 1.1 : 1)));
  if (!offHand && p.staff && useSkill) {
    const double staffed = dmg * (1 + p.staffBonusPct / 100);
    dmg = std::floor(elementalDamage(staffed, b.units()[d.unit].combat.resistance(p.staffElement), a.mag));
  }
  return {static_cast<int>(dmg), chance};
}

}  // namespace

double hitChance(const Fighter& attacker, const Fighter& defender, double accuracy, bool rear) {
  const double chance = std::max(0.0, std::min(100.0, accuracy + (rear ? 10 : 0) + (attacker.dex - defender.dex) / kDexDivisor - defender.dodge / 2));
  return std::ceil(chance * 100 - 1e-6) / 100;  // rounded up to two decimals: the number shown is the number rolled (the user)
}

double Mulberry32::operator()() {
  a_ += 0x6d2b79f5u;
  uint32_t t = (a_ ^ (a_ >> 15)) * (1u | a_);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

std::pair<double, double> heading(BattleHex from, BattleHex to) {
  auto cx = [](BattleHex h) { return kSqrt3 * (h.x + 0.5 * (h.y & 1) + 0.5); };
  return {cx(to) - cx(from), 1.5 * (to.y - from.y)};
}

Fighter fighterAt(const Battle& b, size_t unit, int x, int y) {
  const BattleUnit& u = b.units()[unit];
  Fighter f{unit, x, y, static_cast<double>(u.atk), static_cast<double>(u.mag), static_cast<double>(u.def), static_cast<double>(u.dex),
            u.faceDx, u.faceDy, u.facing};
  f.atkBlows = u.atkBlows;
  // Esquiva (ours): the skill "dodging", 0..100 (a hero's trained, an enemy's from its rank).
  if (!u.summoned)
    for (const auto& [id, v] : u.skills)
      if (id == "dodging") f.dodge = std::max(0.0, std::min(100.0, v));
  // Mortalha de Sombras (EmberEngine's own): standing in it takes statPct % off ATK and MAG, for as long as it stands there.
  if (const int weak = b.weakenAt(u); weak > 0) {
    f.atk = jsRound(f.atk * (1 - weak / 100.0));
    f.mag = f.mag > 0 ? std::max(1.0, jsRound(f.mag * (1 - weak / 100.0))) : 0;
  }
  // The Guerreiro's conditions (ours): Quebra-Guarda takes sunderPct % off DEF, Grito de Guerra adds rallyPct % to ATK.
  if (u.cond.sunderPct > 0) f.def = jsRound(f.def * (1 - u.cond.sunderPct / 100.0));
  if (u.cond.rallyPct > 0) f.atk = jsRound(f.atk * (1 + u.cond.rallyPct / 100.0));
  if (oathActive(u)) f.atk = jsRound(f.atk * (1 + kOathAtkPct / 100.0));  // the Cruzado's Juramento
  if (u.side != "player" || u.summoned || !u.alive) return f;
  // The best affinity bonus among the party members standing next to it (BattleEngine.adjacentAllies), plus the worst penalty
  // (a neighbour it is at odds with, score -25 and below).
  double bonus = 0, penalty = 0;
  const std::vector<BattleHex> mine = Battle::footprint(u, x, y);
  for (const BattleUnit& v : b.units()) {
    if (v.id == u.id || !v.alive || v.side != "player" || v.summoned) continue;
    const std::vector<BattleHex> theirs = Battle::footprint(v, v.x, v.y);
    const bool adjacent = std::any_of(mine.begin(), mine.end(), [&](const BattleHex& p) {
      return std::any_of(theirs.begin(), theirs.end(), [&](const BattleHex& q) { return Battle::distance(p, q) == 1; });
    });
    if (!adjacent) continue;
    const double pair = b.affinityBonus(u.name, v.name);
    bonus = std::max(bonus, pair), penalty = std::min(penalty, pair);
  }
  bonus += penalty;
  if (bonus != 0) f.atk *= 1 + bonus, f.mag *= 1 + bonus, f.def *= 1 + bonus, f.dex *= 1 + bonus;
  return f;
}

bool canHitFrom(const Battle& b, size_t unit, BattleHex from, size_t foe, bool withDecorations) {
  const BattleUnit& u = b.units()[unit];
  const BattleUnit& f = b.units()[foe];
  return hitFrom(b, u, u.minRange, u.maxRange, u.atkBlows ? 0 : u.mag, from, f, {f.x, f.y}, withDecorations, false);
}

bool canOffHandHit(const Battle& b, size_t unit, BattleHex from, size_t foe, bool withDecorations) {
  const BattleUnit& u = b.units()[unit];
  const BattleUnit& f = b.units()[foe];
  if (!u.combat.offHandWeapon || u.combat.offHandShield) return false;
  return hitFrom(b, u, u.combat.offMinRange, u.combat.offMaxRange, u.atkBlows ? 0 : u.mag, from, f, {f.x, f.y}, withDecorations, false);
}

bool canOffHandAct(const Battle& b, size_t unit, BattleHex from, size_t foe, bool withDecorations) {
  const BattleUnit& u = b.units()[unit];
  if (u.combat.offHandShield) return canHitFrom(b, unit, from, foe, withDecorations);
  return canOffHandHit(b, unit, from, foe, withDecorations);
}

bool canCounter(const Battle& b, const Fighter& attacker, BattleHex from, const Fighter& defender) {
  const BattleUnit& d = b.units()[defender.unit];
  if (!d.alive) return false;
  const BattleUnit& a = b.units()[attacker.unit];
  const BattleHex at{defender.x, defender.y};
  // Counters are judged on the bare tiles (Ember passes no decoration overlay here) and from the whole body.
  const double magic = defender.magicBlows() ? defender.mag : 0;
  if (hitFrom(b, d, d.minRange, d.maxRange, magic, at, a, from, false, true)) return true;
  if (!d.combat.offHandWeapon) return false;
  return hitFrom(b, d, d.combat.offMinRange, d.combat.offMaxRange, magic, at, a, from, false, true);
}

Forecast forecast(const Battle& b, size_t attacker, BattleHex from, size_t defender, bool offHand, bool useSkill) {
  const Fighter att = fighterAt(b, attacker, from.x, from.y);
  const BattleUnit& du = b.units()[defender];
  const Fighter def = fighterAt(b, defender, du.x, du.y);
  const BattleCell::Terrain &attTile = tileAt(b, from.x, from.y), &defTile = tileAt(b, du.x, du.y);
  Forecast f;
  const Preview out = previewDamage(b, att, def, attTile, defTile, offHand, useSkill);
  f.dmgOut = out.dmg, f.hitOut = out.hitChance;
  f.canCounter = canCounter(b, att, from, def);
  const bool daggerCounter = du.combat.offHandWeapon && Battle::distance({du.x, du.y}, from) <= du.combat.offMaxRange;
  if (f.canCounter) {
    // The counter is struck at an attacker facing it (never from behind).
    Fighter facing = att;
    facing.faceDx = def.x + (def.y & 1) * 0.5 - att.x - (att.y & 1) * 0.5;
    facing.faceDy = (def.y - att.y) * kSqrt3 / 2;
    const Preview back = previewDamage(b, def, facing, defTile, attTile, daggerCounter);
    f.dmgBack = back.dmg, f.hitBack = back.hitChance;
  }
  f.kill = out.dmg >= du.hp;
  return f;
}

Hit rollDamage(const Battle& b, const Fighter& a, const Fighter& d, Mulberry32& rng, bool useWeaponSkill, const CustomDice* custom) {
  const CombatProfile& p = b.units()[a.unit].combat;
  const BattleCell::Terrain &att = tileAt(b, a.x, a.y), &def = tileAt(b, d.x, d.y);
  const auto [bonusAtk, bonusDef] = terrainBonus(b, a, att, def);
  const bool rear = useWeaponSkill && isRearAttack(a, d);
  // weaponModifiers, or { accuracy 100, damage 1 } when the hit does not use the weapon's mastery
  const double accuracy = useWeaponSkill ? weaponAccuracy(b, a, custom != nullptr) : 100;
  const double damageMul = useWeaponSkill ? (custom ? p.offDamageMul : p.damageMul) : 1;
  auto roll = [&](int dice, int faces) {  // rollDice(dice, faces, 0, rng)
    double rolled = 0;
    for (int i = 0; i < dice; ++i) rolled += 1 + std::floor(rng() * faces);
    return rolled;
  };
  double weapon = 0;
  if (custom) weapon = roll(custom->dice, custom->faces) * damageMul + custom->bonus;  // rollDamageCustom
  else if (p.hasWeapon) weapon = roll(p.dice, p.faces) * damageMul + p.bonus + p.enh;
  Hit h;
  const bool high = archerHighGround(b, a, d, custom != nullptr);
  h.hitChance = useWeaponSkill ? hitChance(a, d, accuracy + (high ? kHighGroundHit : 0), rear) : 100;
  const double highAtk = high ? jsRound(a.atk * kHighGroundAtkPct / 100) : 0;
  const double raw = powerOf(a) + weapon + bonusAtk + highAtk - protOf(a, d) - bonusDef;
  const double classMul = !custom && p.classBonus ? 1.1 : 1;
  h.preCritDmg = static_cast<int>(std::max(1.0, std::floor(std::max(1.0, raw) * classMul * (rear ? 1.1 : 1))));
  double dmg = h.preCritDmg;
  h.crit = rng() < 0.08;
  if (h.crit) dmg = std::max(1.0, std::floor(dmg * 1.5));
  h.landed = h.hitChance >= 100 || rng() * 100 < h.hitChance;
  h.rolled = static_cast<int>(dmg);
  h.dmg = h.rolled;
  return h;
}

Hit rollHit(const Battle& b, const Fighter& a, const Fighter& d, Mulberry32& rng) {
  const CombatProfile& p = b.units()[a.unit].combat;
  Hit h = rollDamage(b, a, d, rng, true, nullptr);
  double dmg = h.rolled;
  // The engine's hit stage: an arcane caster's bolt and a staff's element pass through the target's resistance.
  if (h.landed && ((!p.arrow && p.arcane) || p.staff)) {
    const std::string element = p.staff ? p.staffElement : "arcane";
    const double staffed = dmg * (1 + (p.staff ? p.staffBonusPct : 0) / 100);
    dmg = std::floor(elementalDamage(staffed, b.units()[d.unit].combat.resistance(element), a.mag));
  }
  h.dmg = static_cast<int>(dmg);
  return h;
}

}  // namespace ember
