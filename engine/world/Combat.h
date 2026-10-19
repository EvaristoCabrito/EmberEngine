#pragma once

// Combat on a battle's board: Ember's rules for a plain weapon attack (Engine2 src/game/combat.ts: makeForecast,
// previewDamage, rollDamage, canCounter, isRearAttack, terrainBonus; pathfinding.ts canHitFrom, clearShot,
// shotBlocker, hexLine; the engine's affinity bonus and its arcane / staff element step on a hit), ported step for
// step with JavaScript's number rules (doubles, Math.round, Math.floor) and Engine2's mulberry32 random numbers, so the
// same seed gives the same rolls. Cooker check-battle compares forecasts and seeded rolls with Engine2's own.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
  double dodge = 0;  // a hero's Esquiva skill, 0..100 (ours): each point takes half a point off the blows aimed at it
  bool atkBlows = false;  // BattleUnit::atkBlows: its weapon strikes with ATK even with MAG
  bool magicBlows() const { return mag > 0 && !atkBlows; }
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

// Explicit dice in place of the main-hand weapon (an off-hand weapon: rollDamageCustom).
struct CustomDice {
  int dice = 1, faces = 4, bonus = 0;
};

// combat.ts rollDamage (rollDamageCustom with `custom`): no element step. useWeaponSkill = false drops the weapon's
// mastery and the rear bonus and always lands (spell-like hits).
Hit rollDamage(const Battle& b, const Fighter& attacker, const Fighter& defender, Mulberry32& rng, bool useWeaponSkill = true,
               const CustomDice* custom = nullptr);
// pathfinding.ts hexLine / shotBlocker == null (`arrow` = an arrow's flight, else a bolt; decorations folded in or not).
std::vector<BattleHex> hexLine(BattleHex a, BattleHex b);
bool clearShot(const Battle& b, BattleHex from, BattleHex to, bool arrow, bool decorations = true);
// combat.ts powerOf; resistances.ts elementalDamage (resistance in %, softened by the caster's MAG / 2).
double powerOf(const Fighter& f);
double elementalDamage(double base, double resistance, double mag);
// Every weapon blow's chance to land, in % (ours, the user, 2026-10-18): clamp(0..100, accuracy + 10 from behind +
// (attacker's DEX - defender's DEX) / kDexDivisor - defender's Esquiva / 2), DEX and Esquiva as fighterAt reads them;
// rounded up to two decimals.
double hitChance(const Fighter& attacker, const Fighter& defender, double accuracy, bool rear);

// A weapon blow's accuracy before the target's DEX and the rear bonus (weaponModifiers): 75 + weapon skill for a hero, 90 + its
// live DEX for an enemy, plus a blessing.
double weaponAccuracy(const Battle& b, const Fighter& attacker, bool offHand);
// The unit placed at (x, y), with the affinity bonus its party neighbours there give it (BattleEngine.affinityUnit).
Fighter fighterAt(const Battle& b, size_t unit, int x, int y);
// Whether the unit, standing at `from`, reaches the foe with its weapon and has a clear shot (canHitFrom).
bool canHitFrom(const Battle& b, size_t unit, BattleHex from, size_t foe, bool withDecorations = true);
// The same with the off-hand weapon's reach (Engine2 offHandReach: a dagger or katar beside a bow reaches by its own range,
// the bow's minimum would leave an adjacent foe out of reach).
bool canOffHandHit(const Battle& b, size_t unit, BattleHex from, size_t foe, bool withDecorations = true);
// Whether the off-hand action ("Mão Secundária") reaches the foe from `from`: a dagger / katar by its own reach, a shield's bash by the
// unit's own (commitOffHandAction).
bool canOffHandAct(const Battle& b, size_t unit, BattleHex from, size_t foe, bool withDecorations = true);
// The forecast the battle shows for an attack from `from` (makeForecast).
// `useSkill` false: a blow without the weapon's mastery (a shield's bash: 100% to hit, no rear bonus, no staff element).
Forecast forecast(const Battle& b, size_t attacker, BattleHex from, size_t defender, bool offHand = false, bool useSkill = true);
// A real plain attack's hit (rollDamage, then the arcane bolt / staff element step), drawing from `rng` in Engine2's
// order. The attacker stands where it is.
Hit rollHit(const Battle& b, const Fighter& attacker, const Fighter& defender, Mulberry32& rng);
// The defender can strike back at an attacker standing at `from` (canCounter).
bool canCounter(const Battle& b, const Fighter& attacker, BattleHex from, const Fighter& defender);
// Engine2 hexCenter's heading from one hex to another (screen units: its faceDx / faceDy).
std::pair<double, double> heading(BattleHex from, BattleHex to);

}  // namespace ember::e2
