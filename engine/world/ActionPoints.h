#pragma once

// Action Points (PA) on a battle's board, EmberEngine's own (the user, 2026-11-06): every unit fights from one reserve of AP.
// Its maximum is 12 + floor(DEX / 10) + the armor's bonus (unarmored +5 ... heavy plate +0) + what gear, talents, conditions and
// modifiers add; attacks cost by the weapon's category (dagger 5, sword / axe / blunt 6, polearm / bow 7), a potion 5, Examinar
// its own, each hex walked its terrain's AP (1 on flat ground), and a counter the defender's own AP out of the same reserve (a
// Staff Counter 6, a Spell Counter 7): no AP, no counter. The AP left at the end of a turn stays for counters until the unit's
// next own turn begins, when it is refreshed (once). Every number comes from content/data/action-points.json (apRules);
// docs/ACTION_POINTS.md says which are the design's and which are provisional.
//
// One authoritative path: apMax / apCost / apStepCost resolve every modifier (flat first, then the percentages, one rounding,
// never under zero); apCanAfford / apTrySpend are the only gate and the only spend (Battle::move and MapStage's walks for
// movement, apCommitSteps for an action's steps, resolveStrike for a counter); apRefresh is the one refresh.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "world/Battle.h"

namespace ember {

struct BattleStep;

const char* apSourceName(ApModifierSource s);
ApModifierSource apSourceFromName(const std::string& name);

// The rules as content/data/action-points.json gives them (the file read once; the design's defaults when it is missing).
struct ApRules {
  int baseAp = 12, dexPerAp = 10;
  int maxAp = 34;  // the ceiling of any unit's maximum: the AP scale runs from 1 to 34 (the user, 2026-11-06)
  std::string rounding = "floor";  // for the percentages on the maximum and on actions: floor | round | ceil
  struct Armor {
    std::string slot = "chest", defaultCategory = "unarmored";
    std::vector<std::pair<std::string, int>> categories;        // in file order
    std::map<std::string, std::string> items, classes;           // item id -> category; class id -> category
    std::map<std::string, std::pair<std::string, std::string>> names;  // category -> (pt, en)
  } armor;
  struct Weapons {
    std::map<std::string, std::string> categories, classWeapons;  // weaponType -> category; class id -> category
    std::map<std::string, int> attackCost;
    std::string defaultCategory = "blunt";
  } weapons;
  int potion = 5, lockpick = 5;
  struct Spells {
    int defaultCost = 8;
    bool weaponSkillsUseWeaponCost = false;
    std::map<int, int> byTier;
    std::map<std::string, int> byKind;
  } spells;
  struct Counters {
    std::map<std::string, int> byType;  // "staff", "spell", and any weapon category listed
  } counters;
  struct Movement {
    int minimum = 1;
    std::string rounding = "round";
    bool cappedByMov = false, freeWhileRoaming = true;
    struct Terrain {
      std::optional<int> apCost;  // none = the tile's own moveCost
      int modifier = 0;
      std::string category;
    };
    std::map<std::string, Terrain> terrain;
    std::map<std::string, int> zones;  // "web", "iceStorm", "aura", "zone"
  } movement;
  struct Refresh {
    std::string policy = "replace";  // replace | accumulate
    double accumulateCap = 2.0;
  } refresh;
  struct Bonus {
    int flat = 0;
    double pct = 0;
    int movementFlat = 0;
    double movementPct = 0;
  };
  std::map<std::string, Bonus> status, talents, equipment;
  struct Examine {
    int cost = 3;
    bool spendsAction = false;
    struct Category {
      std::string id, pt, en;
      std::optional<double> maxRatio;
    };
    std::vector<Category> categories;
  } examine;
  struct Ai {
    double counterReserveWeight = 1;  // the AI's score adds the AP a strike leaves it, times this
  } ai;
  bool loaded = false;  // the file was read

  int armorBonus(const std::string& category) const;
  std::string weaponCategory(const std::string& weaponType) const;
  int attackCost(const std::string& category) const;
  const Movement::Terrain* terrainOf(const std::string& id) const {
    const auto it = movement.terrain.find(id);
    return it == movement.terrain.end() ? nullptr : &it->second;
  }
};
// The rules, read once from content/data/action-points.json (relative to the working directory, like every data file).
const ApRules& apRules();
// Reads them again (a test that changes the file, the editor).
void reloadApRules();

// --- The resolver ---------------------------------------------------------------------------------------------------------
// The unit's maximum AP now (base + DEX + armor + gear + talents + conditions + its modifiers of scope Max).
int apMax(const Battle& b, const BattleUnit& u);
// The armor's bonus, by category (the unit's own, or the default when it has none).
int apArmorBonus(const BattleUnit& u);
// The unit's weapon category for the costs: a hero's main-hand weaponType folded by the rules, an enemy's class row, else the default.
std::string apWeaponCategory(const BattleUnit& u);
// The off-hand weapon's category (a dagger), empty without one.
std::string apOffHandCategory(const BattleUnit& u);

// One action to price.
struct ApAction {
  enum class Kind { Attack, OffHand, Spell, Potion, Lockpick, Examine, Counter };
  Kind kind = Kind::Attack;
  std::string spell;        // Kind::Spell: the kind (a weapon skill prices as the weapon when the rules say so)
  std::string counterType;  // Kind::Counter: "staff", "spell" or a weapon category
  static ApAction attack() { return {Kind::Attack, {}, {}}; }
  static ApAction offHand() { return {Kind::OffHand, {}, {}}; }
  static ApAction cast(std::string kind) { return {Kind::Spell, std::move(kind), {}}; }
  static ApAction potion() { return {Kind::Potion, {}, {}}; }
  static ApAction lockpick() { return {Kind::Lockpick, {}, {}}; }
  static ApAction examine() { return {Kind::Examine, {}, {}}; }
  static ApAction counter(std::string type) { return {Kind::Counter, {}, std::move(type)}; }
};
// The action's effective cost for the unit (its base, then the unit's modifiers that cover it; never under zero).
int apCost(const Battle& b, const BattleUnit& u, const ApAction& action);
// The AP one hex costs the unit: the terrain's own (or the tile's moveCost as the walk already read it: `effectiveMoveCost`),
// the zones laid on the hex, the unit's conditions and movement modifiers; never under the rules' minimum.
int apStepCost(const Battle& b, const BattleUnit& u, int x, int y, int effectiveMoveCost);
// The cheapest hex the unit could ever step on (the rules' minimum through its modifiers): whether it can still walk at all.
int apCheapestStep(const Battle& b, const BattleUnit& u);

// The counter the defender would answer `attacker` (standing at `from`) with: "spell" (an arcane bolt from afar), "staff" (its staff
// on a foe beside it), its off-hand dagger's category when that reaches, else its weapon's category.
std::string apCounterType(const Battle& b, const BattleUnit& defender, const BattleUnit& attacker, BattleHex from);
// ... and what that counter costs it, or what its own counter of the plainest kind costs (the AI's reserve) without an attacker.
int apCounterCost(const Battle& b, const BattleUnit& defender, const BattleUnit& attacker, BattleHex from);
int apOwnCounterCost(const Battle& b, const BattleUnit& u);
// NO AP, NO COUNTER: whether the defender has the AP of the counter it would make.
bool apCanCounter(const Battle& b, const BattleUnit& defender, const BattleUnit& attacker, BattleHex from);

// The one gate and the one spend. apTrySpend takes nothing when the cost is more than the unit has (never negative).
bool apCanAfford(const BattleUnit& u, int cost);
bool apTrySpend(BattleUnit& u, int cost);
// The cost of an action given as its steps (the first step names it: a plain blow, the off hand, a skill or spell, a potion); the
// walk inside a skill (Investida Touro) is part of it.
int apStepsCost(const Battle& b, size_t owner, const std::vector<BattleStep>& steps);
// Validates and spends an action's cost once, at its commit (MapStage::play, the checks). False, and nothing spent, when it cannot be paid.
bool apCommitSteps(Battle& b, size_t owner, const std::vector<BattleStep>& steps);

// The refresh at the start of the unit's own turn (Battle::beginTurn): its expiring modifiers count down, then the policy gives it
// its AP (replace: the maximum; accumulate: added, capped). Once per round: a turn begun twice (a fight resumed) refreshes once.
void apRefresh(Battle& b, size_t unit);
// Its maximum changed (armor swapped in the fight, a modifier): the reserve moves by the same amount, never under zero and never
// over the new maximum, so swapping back and forth gives nothing (the project's rule for HP: MapStage adjustStat).
void apOnMaxChanged(BattleUnit& u, int oldMax, int newMax);

// Examinar: how the target measures against the examiner, from the real forecasts both ways (the turns the examiner needs to down
// it over the turns it needs to down the examiner), named by the rules' categories.
struct ApExamine {
  std::string categoryId, labelPt, labelEn;
  double ratio = 0;       // < 1: the examiner is ahead
  int dmgOut = 0, dmgIn = 0;
  double hitOut = 0, hitIn = 0;
  std::string weaponCategory;  // the target's
  int attackCost = 0, counterCost = 0, apMaxTarget = 0;
};
ApExamine apExamine(const Battle& b, size_t examiner, size_t target);

// Save: the unit's AP state in and out of a snapshot (BattleSave.cpp). A snapshot without it (saved before AP) gets the full
// maximum once, here, and never again.
void apWriteSnapshot(const BattleUnit& u, nlohmann::json& s);
void apReadSnapshot(const Battle& b, BattleUnit& u, const nlohmann::json& s, int round);

// The armor category of a worn chest piece (by its id, the rules' table), or the default.
std::string apArmorCategoryOfItem(const std::string& itemId);
// The category's name for the sheet (pt / en).
std::string apArmorName(const std::string& category, bool en);

}  // namespace ember
