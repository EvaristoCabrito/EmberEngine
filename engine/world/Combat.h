#pragma once

// Combat on a battle's board: Ember's rules for a plain weapon attack (Engine2 src/game/combat.ts: makeForecast,
// previewDamage, rollDamage, canCounter, isRearAttack, terrainBonus; pathfinding.ts canHitFrom, clearShot,
// shotBlocker, hexLine; the engine's affinity bonus and its arcane / staff element step on a hit), ported step for
// step with JavaScript's number rules (doubles, Math.round, Math.floor) and Engine2's mulberry32 random numbers, so the
// same seed gives the same rolls. Cooker check-battle compares forecasts and seeded rolls with Engine2's own.

#include <cstdint>
#include <optional>
#include <string>

#include "world/Battle.h"

namespace ember::e2 {

// Engine2's mulberry32: one 32-bit state, numbers in [0, 1).
class Mulberry32 {
 public:
  explicit Mulberry32(uint32_t seed) : a_(seed) {}
  double operator()();

 private:
  uint32_t a_;
};

// One side of an exchange: a unit where it stands (or would stand) with its stats as the fight reads them.
struct Fighter {
  size_t unit = 0;
  int x = 0, y = 0;
  double atk = 0, mag = 0, def = 0, dex = 0;  // after the party's affinity bonus
  std::optional<double> faceDx, faceDy;
  int facing = 1;
};

struct Forecast {
  int dmgOut = 0, dmgBack = 0;
  double hitOut = 0, hitBack = 0;
  bool canCounter = false, kill = false;
};

struct Hit {
  int dmg = 0;  // after the arcane / staff element step when it applies
  bool crit = false, landed = false;
  double hitChance = 0;
  int preCritDmg = 0;
  int rolled = 0;  // rollDamage's own damage, before that step
};

// The unit placed at (x, y), with the affinity bonus its party neighbours there give it (BattleEngine.affinityUnit).
Fighter fighterAt(const Battle& b, size_t unit, int x, int y);
// Whether the unit, standing at `from`, reaches the foe with its weapon and has a clear shot (canHitFrom).
bool canHitFrom(const Battle& b, size_t unit, BattleHex from, size_t foe, bool withDecorations = true);
// The forecast the battle shows for an attack from `from` (makeForecast).
Forecast forecast(const Battle& b, size_t attacker, BattleHex from, size_t defender);
// A real plain attack's hit (rollDamage, then the arcane bolt / staff element step), drawing from `rng` in Engine2's
// order. The attacker stands where it is.
Hit rollHit(const Battle& b, const Fighter& attacker, const Fighter& defender, Mulberry32& rng);
// The defender can strike back at an attacker standing at `from` (canCounter).
bool canCounter(const Battle& b, const Fighter& attacker, BattleHex from, const Fighter& defender);
// Engine2 hexCenter's heading from one hex to another (screen units: its faceDx / faceDy).
std::pair<double, double> heading(BattleHex from, BattleHex to);

}  // namespace ember::e2
