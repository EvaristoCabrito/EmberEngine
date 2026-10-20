// Turns, enmity and the enemies' decisions (Engine2 src/game/engine.ts: sortByInitiative, activeTurnUnit,
// startNewRound, beginUnitTurn, noteDamageEnmity / enmityTarget, runAiFor's plain branch; pathfinding.ts
// terrainDistanceField). Cooker check-battle compares every enemy's decision with Engine2's own runAiFor.

#include <algorithm>
#include <limits>

#include "world/Battle.h"
#include "world/Combat.h"

namespace ember::e2 {

namespace {
constexpr double kEnmityCap = 10000;            // enmity.ts ENMITY.cap
constexpr double kVolatileDecayPerRound = 300;  // ENMITY.volatileDecayPerRound
constexpr double kDamageTakenCe = 2;            // ENMITY.damageTakenCe
constexpr double kWeaponCe = 1, kWeaponVe = 3;  // ENMITY.weapon
constexpr double kSpellCe = 1.5, kSpellVe = 6;  // ENMITY.spell
double capped(double v) { return std::max(0.0, std::min(kEnmityCap, v)); }
}  // namespace

void Battle::sortTurnOrder() {
  turnOrder_.clear();
  for (size_t i = 0; i < units_.size(); ++i)
    if (units_[i].alive && units_[i].side != "neutral") turnOrder_.push_back(i);
  std::stable_sort(turnOrder_.begin(), turnOrder_.end(), [&](size_t a, size_t b) {
    const BattleUnit &p = units_[a], &q = units_[b];
    if (p.initiative != q.initiative) return p.initiative > q.initiative;
    if (p.side != q.side) return p.side == "player";
    return p.id < q.id;
  });
}

int Battle::activeUnit() const {
  for (size_t i : turnOrder_)
    if (units_[i].alive && !units_[i].moved) return static_cast<int>(i);
  return -1;
}

void Battle::startNewRound() {
  for (Enmity& e : enmity_) e.ve = capped(e.ve - kVolatileDecayPerRound);
  for (BattleUnit& u : units_) {
    u.moved = false, u.acted = false;
    // Tiro Sangrento's bleed counts down one round at a time, and Bênção's too
    if (u.bleedRoundsLeft && u.bleedRoundMarker && *u.bleedRoundMarker < round_) {
      u.bleedRoundsLeft = std::max(0, *u.bleedRoundsLeft - 1);
      u.bleedRoundMarker = round_;
      if (*u.bleedRoundsLeft == 0) u.bleeding = false, u.bleedRoundMarker.reset();
    }
    if (u.blessedRoundsLeft > 0) {
      u.blessedRoundsLeft = std::max(0, u.blessedRoundsLeft - 1);
      if (u.blessedRoundsLeft == 0) u.blessedHitBonusPct = 0;
    }
    // The Conjurador's (docs/SKILLS_CONJURER.md): the illusions and the summons' strength last their rounds
    if (u.cond.mirrorRounds > 0 && --u.cond.mirrorRounds == 0) u.cond.mirrorCopies = 0;
    if (u.cond.empowerRounds > 0 && --u.cond.empowerRounds == 0) {
      if (u.empowerBase) u.atk = u.empowerBase->atk, u.mag = u.empowerBase->mag, u.def = u.empowerBase->def, u.dex = u.empowerBase->dex, u.mov = u.empowerBase->mov;
      u.empowerBase.reset();
      u.cond.empowerPct = 0;
    }
  }
  // Engine2's zones and gates last their rounds
  for (WebZone& z : webZones_) --z.roundsLeft;
  std::erase_if(webZones_, [](const WebZone& z) { return z.roundsLeft <= 0; });
  for (IceZone& z : iceZones_) --z.roundsLeft;
  std::erase_if(iceZones_, [](const IceZone& z) { return z.roundsLeft <= 0; });
  for (AuraZone& z : auraZones_) --z.roundsLeft;
  std::erase_if(auraZones_, [](const AuraZone& z) { return z.roundsLeft <= 0; });
  for (WarpGate& g : warpGates_) --g.roundsLeft;
  std::erase_if(warpGates_, [](const WarpGate& g) { return g.roundsLeft <= 0; });
  sortTurnOrder();
  ++round_;
  // Zones last their rounds (a Meteoro waits for its caster's turn instead).
  for (Zone& z : zones_)
    if (z.kind != "meteor") --z.rounds;
  std::erase_if(zones_, [](const Zone& z) { return z.kind != "meteor" && z.rounds <= 0; });
}

int Battle::weakenAt(const BattleUnit& u) const {
  int pct = 0;
  if (!u.alive) return 0;
  for (const Zone& z : zones_) {
    if (z.statPct <= 0) continue;
    for (const BattleHex& h : footprint(u, u.x, u.y))
      if (z.covers(h)) pct = std::max(pct, z.statPct);
  }
  return pct;
}

bool Battle::inZoneOf(const BattleUnit& u, const std::string& element) const {
  if (!u.alive) return false;
  for (const Zone& z : zones_) {
    if (z.element != element || z.kind == "meteor") continue;
    for (const BattleHex& h : footprint(u, u.x, u.y))
      if (z.covers(h)) return true;
  }
  return false;
}

void Battle::beginTurn(size_t unit) {
  BattleUnit& u = units_[unit];
  u.moveUsed = 0;
  if (u.side == "player") u.acted = false;
}

std::string Battle::result() const {
  if (finished_) return "victory";
  if (roaming_) return {};  // free roam has no defeat (Engine2 explore missions: only the waypoints)
  const bool party = std::any_of(units_.begin(), units_.end(), [](const BattleUnit& u) { return u.alive && u.side == "player" && !u.summoned; });
  return party ? std::string() : std::string("defeat");
}

const Battle::Exit* Battle::activeExit(size_t* hero) const {
  for (size_t i = 0; i < units_.size(); ++i) {
    const BattleUnit& u = units_[i];
    if (u.side != "player" || !u.alive) continue;
    for (const Exit& e : exits_)
      for (const BattleHex& c : e.cells)
        if (c.x == u.x && c.y == u.y) {
          if (hero) *hero = i;
          return &e;
        }
  }
  return nullptr;
}

bool Battle::winAvailable() const {
  if (finished_) return false;
  if (activeExit()) return true;
  // Engine2 evaluateEnd: an explore mission ends only at its waypoints. So does every map with fog of war (the user,
  // 2026-10-17: "essas missões são de passar do outro lado", "não existe terminar a missão em mapas com névoa"): every foe
  // down is free movement again, never the end.
  if (explore_ || waypointsOnly_) return false;
  const bool bossAlive = std::any_of(units_.begin(), units_.end(), [](const BattleUnit& u) { return u.side == "enemy" && u.alive && u.boss; });
  const bool anyEnemy = std::any_of(units_.begin(), units_.end(), [](const BattleUnit& u) { return u.side == "enemy" && u.alive; });
  return win_ == "boss" ? !bossAlive : win_ == "escape" ? false : !anyEnemy;
}

double Battle::fleeChance(size_t unit) const {
  const BattleUnit& u = units_[unit];
  return std::min(100.0, 60 + fighterAt(*this, unit, u.x, u.y).dex / kDexDivisor);
}

void Battle::noteWeaponDamage(size_t attacker, size_t target, int dmg) { noteDamage(attacker, target, dmg, false); }

void Battle::noteAware(size_t hero, double ce, double ve) {
  if (units_[hero].side != "player") return;
  for (size_t f = 0; f < units_.size(); ++f) {
    const BattleUnit& foe = units_[f];
    if (foe.side != "enemy" || !foe.alive) continue;
    if (!units_[hero].alive) continue;  // addEnmity: both must still stand
    bool found = false;
    for (Enmity& e : enmity_)
      if (e.enemy == f && e.hero == hero) e.ce = capped(e.ce + ce), e.ve = capped(e.ve + ve), found = true;
    if (!found) enmity_.push_back({f, hero, capped(ce), capped(ve)});
  }
}

std::vector<std::array<double, 4>> Battle::enmityDump() const {
  std::vector<std::array<double, 4>> out;
  for (const Enmity& e : enmity_) out.push_back({static_cast<double>(e.enemy), static_cast<double>(e.hero), e.ce, e.ve});
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a[0] != b[0] ? a[0] < b[0] : a[1] < b[1]; });
  return out;
}

void Battle::addEnmity(size_t foe, size_t hero, double ce, double ve) {
  if (units_[foe].side != "enemy" || units_[hero].side != "player" || !units_[foe].alive || !units_[hero].alive) return;
  for (Enmity& e : enmity_)
    if (e.enemy == foe && e.hero == hero) {
      e.ce = capped(e.ce + ce), e.ve = capped(e.ve + ve);
      return;
    }
  enmity_.push_back({foe, hero, capped(ce), capped(ve)});
}

void Battle::noteDamage(size_t attacker, size_t target, int dmg, bool spell) {
  if (dmg <= 0) return;
  const BattleUnit &a = units_[attacker], &t = units_[target];
  const double kCe = spell ? kSpellCe : kWeaponCe, kVe = spell ? kSpellVe : kWeaponVe;
  if (a.side == "player" && t.side == "enemy") {
    // A hero hurting an enemy draws its attention (addEnmity: both must still stand).
    if (!a.alive || !t.alive) return;
    for (Enmity& e : enmity_)
      if (e.enemy == target && e.hero == attacker) {
        e.ce = capped(e.ce + dmg * kCe), e.ve = capped(e.ve + dmg * kVe);
        return;
      }
    enmity_.push_back({target, attacker, capped(dmg * kCe), capped(dmg * kVe)});
  } else if (a.side == "enemy" && t.side == "player") {
    // An enemy hurting a hero wears that hero's lasting enmity down.
    for (Enmity& e : enmity_)
      if (e.enemy == attacker && e.hero == target) e.ce = capped(e.ce - dmg * kDamageTakenCe);
  }
}

int Battle::enmityTarget(size_t enemy) const {
  int best = -1;
  double bestTotal = 0;
  for (const Enmity& e : enmity_) {
    if (e.enemy != enemy) continue;
    const BattleUnit& h = units_[e.hero];
    if (!h.alive || h.side != "player") continue;
    if (e.ce + e.ve > bestTotal) best = static_cast<int>(e.hero), bestTotal = e.ce + e.ve;
  }
  return best;
}

std::vector<int> Battle::distanceField(BattleHex from) const {
  std::vector<int> dist(cells_.size(), -1);
  if (!inBounds(from.x, from.y)) return dist;
  dist[static_cast<size_t>(from.y * cols_ + from.x)] = 0;
  // A bucket per cost: costs only grow outward, so the buckets are visited in order (a stale entry is skipped).
  std::vector<std::vector<BattleHex>> buckets{{from}};
  for (size_t c = 0; c < buckets.size(); ++c) {
    const std::vector<BattleHex> bucket = std::move(buckets[c]);
    for (const BattleHex& cur : bucket) {
      const int known = dist[static_cast<size_t>(cur.y * cols_ + cur.x)];
      if (known >= 0 && static_cast<size_t>(known) < c) continue;
      for (const BattleHex& n : neighbors(cur.x, cur.y)) {
        if (!inBounds(n.x, n.y)) continue;
        const BattleCell& t = cell(n.x, n.y);
        if (!t.passable) continue;
        const int next = static_cast<int>(c) + t.moveCost;
        int& d = dist[static_cast<size_t>(n.y * cols_ + n.x)];
        if (d >= 0 && d <= next) continue;
        d = next;
        if (buckets.size() <= static_cast<size_t>(next)) buckets.resize(static_cast<size_t>(next) + 1);
        buckets[static_cast<size_t>(next)].push_back(n);
      }
    }
  }
  return dist;
}

AiPlan Battle::planEnemy(size_t unit) const {
  const BattleUnit& u = units_[unit];
  const std::vector<ReachCell> reach = reachable(unit, true), walk = reachable(unit, false);
  // Enmity: whoever tops its table, else every hero standing.
  std::vector<size_t> heroes;
  if (const int focus = enmityTarget(unit); focus >= 0) {
    heroes.push_back(static_cast<size_t>(focus));
  } else {
    for (size_t i = 0; i < units_.size(); ++i)
      if (units_[i].side == "player" && units_[i].alive) heroes.push_back(i);
  }
  // The best strike from any hex it can reach: the most wounded foe, good ground, a foe nearly down.
  AiPlan plan;
  std::optional<double> bestScore;
  BattleHex from{};
  for (const ReachCell& c : reach)
    for (size_t h : heroes) {
      if (!canHitFrom(*this, unit, {c.x, c.y}, h)) continue;
      const BattleUnit& foe = units_[h];
      const double score = (foe.maxHp - foe.hp) * 3 + cell(c.x, c.y).def * 2 + (foe.hp <= 8 ? 20 : 0);
      if (!bestScore || score > *bestScore) bestScore = score, from = {c.x, c.y}, plan.target = static_cast<int>(h);
    }
  if (bestScore) {
    if (from.x != u.x || from.y != u.y) plan.path = path(walk, from.x, from.y);
    return plan;
  }
  if (heroes.empty()) return plan;
  // Close in on the nearest hero by real walking distance (walls and pillars count), not a straight line.
  std::vector<std::vector<int>> fields;
  for (size_t h : heroes) fields.push_back(distanceField({units_[h].x, units_[h].y}));
  auto at = [&](const std::vector<int>& f, int x, int y) {
    const int d = inBounds(x, y) ? f[static_cast<size_t>(y * cols_ + x)] : -1;
    return d < 0 ? std::numeric_limits<double>::infinity() : static_cast<double>(d);
  };
  const std::vector<int>* nearest = &fields.front();
  for (const std::vector<int>& f : fields)
    if (at(f, u.x, u.y) < at(*nearest, u.x, u.y)) nearest = &f;
  // Human-sized hostiles recognise either open Warp mouth as a way to pursue the party through (the walk to the nearer mouth by real
  // distance); their ordinary pursuit resumes after the crossing.
  std::vector<std::vector<int>> gateFields;
  gateFields.reserve(warpGates_.size() * 2);
  if (!warpGates_.empty() && u.side != "player" && footprint(u, u.x, u.y).size() == 1) {
    double bestGate = std::numeric_limits<double>::infinity();
    for (const WarpGate& g : warpGates_)
      for (const BattleHex& opening : {g.a, g.b}) {
        gateFields.push_back(distanceField(opening));
        if (const double d = at(gateFields.back(), u.x, u.y); d < bestGate) bestGate = d, nearest = &gateFields.back();
      }
  }
  std::optional<BattleHex> closest;
  double dist = std::numeric_limits<double>::infinity();
  for (const ReachCell& c : reach)
    if (const double d = at(*nearest, c.x, c.y); d < dist) dist = d, closest = BattleHex{c.x, c.y};
  if (closest && (closest->x != u.x || closest->y != u.y)) plan.path = path(walk, closest->x, closest->y);
  return plan;
}

}  // namespace ember::e2
