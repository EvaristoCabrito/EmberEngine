#pragma once

// A battle on an Ember map: the board as the movement rules see it, the units and their movement reach. The rules are
// Ember's (Engine2 src/game/pathfinding.ts: footprint, footprintCost, computeReachable, reconstructPath), ported
// step for step so a unit reaches exactly the hexes, at the costs and along the paths, it does there; Cooker
// check-battle compares every unit's reach with Engine2's own (tools/scripts/engine2/export_battle.ts).
//
// Hexes are (column, row) on Ember's odd-r board: odd rows sit half a hex to the right.

#include <filesystem>
#include <optional>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace ember::e2 {

struct BattleHex {
  int x = 0, y = 0;  // column, row
  bool operator==(const BattleHex&) const = default;
};

// One cell as movement reads it (Engine2 hexprops.hexDef: the terrain with the decorations' blocking folded in).
struct BattleCell {
  std::string terrain;  // the tile's terrain id ("plains", "water", "barricade", ...)
  bool passable = true;
  int moveCost = 1;
  int def = 0, atk = 0;
  bool blocked = false;  // a decoration stands here and stops walking (HEX_BLOCKED)
  // Combat: the tile's own terrain (combat.ts terrainBonus, effectiveMaxRange; counters' line of fire) and the cell
  // with the decorations folded in (a shot's line of fire).
  struct Terrain {
    int def = 0, atk = 0, height = 0, cover = 0;
    bool blocksShot = false;
  } raw;
  struct Shot {
    int height = 0;
    bool blocksShot = false;
  } shot;
};

// What a unit fights with, as Engine2 resolves it (weapon tables, weapon skills, staff magic, off-hand).
struct CombatProfile {
  bool hasWeapon = false;  // a weapon definition: its dice are rolled
  int dice = 0, faces = 0, bonus = 0, enh = 0;
  bool ranged = false;      // a ranged weapon: +1 reach from high ground
  bool classBonus = false;  // the weapon is tuned for this class: x1.1
  double accuracy = 100, damageMul = 1;        // main-hand mastery
  double offAccuracy = 100, offDamageMul = 1;  // off-hand mastery
  bool offHandWeapon = false;
  int offMinRange = 1, offMaxRange = 1, offDice = 1, offFaces = 4, offBonus = 0;
  bool staff = false;  // staff magic: its element and bonus
  std::string staffElement = "arcane";
  double staffBonusPct = 0;
  bool arcane = false;  // an arcane caster: its plain attack is an arcane bolt
  bool arrow = false;   // its plain attack is an arrow
  std::vector<std::pair<std::string, double>> resistances;
  double resistance(const std::string& element) const {
    for (const auto& [e, v] : resistances)
      if (e == element) return v;
    return 0;
  }
};

// One reachable hex: the movement it costs and the hex it is reached from (none for the start).
struct ReachCell {
  int x = 0, y = 0, cost = 0;
  int parentX = -1, parentY = -1;
};

struct BattleUnit {
  std::string id, side, name, classId, sprite;
  int x = 0, y = 0;  // the anchor hex: for a big body, its front row (closest to the player)
  bool alive = true;
  int mov = 0, moveUsed = 0;  // movement this turn and what is spent of it
  bool acted = false;         // it has acted this turn (its movement may still be used)
  bool moved = false;         // its turn in this round is over (Engine2 Unit.moved)
  int initiativeBonus = 0, initiative = 0;  // its class's bonus; the battle-opening roll + bonus
  int hp = 0, maxHp = 0, atk = 0, def = 0, mag = 0, dex = 0, minRange = 1, maxRange = 1;
  int size = 1;
  int level = 1;
  bool summoned = false;
  CombatProfile combat;
  std::optional<double> faceDx, faceDy;  // its last heading on the board (Engine2 screen units), if any
  int facing = 1;
  std::optional<std::vector<BattleHex>> footprintOffsets;  // an authored body shape (dx, dy), else by size
  int footprintW = 2, footprintH = 4;                       // the size >= 4 rectangle
  std::vector<ReachCell> reference;                         // Engine2's reach at the start (the check's reference)
};

// An enemy's decision (Engine2 runAiFor's plain branch): the walk (start first; empty = stays) and the foe it strikes.
struct AiPlan {
  std::vector<BattleHex> path;
  int target = -1;
};
struct AiReference {
  int unit = 0;
  std::optional<BattleHex> at;  // placed here first (the near-party scenario)
  AiPlan plan;
  bool spell = false;           // Engine2 cast something instead (not ported yet: skipped by the check)
};

// Engine2's reference answers for the combat check (export_battle.ts).
struct ForecastReference {
  int attacker = 0, defender = 0, x = 0, y = 0, dmgOut = 0, dmgBack = 0;
  double hitOut = 0, hitBack = 0;
  bool canCounter = false, kill = false, canHit = false;
};
struct RollReference {
  int attacker = 0, defender = 0, x = 0, y = 0;
  uint32_t seed = 0;
  int dmg = 0;
  bool crit = false, landed = false;
  double hitChance = 0;
  int preCritDmg = 0, finalDmg = 0;
};

class Battle {
 public:
  static std::optional<Battle> load(const std::filesystem::path& battleJson);

  int cols() const { return cols_; }
  int rows() const { return rows_; }
  const std::string& map() const { return map_; }
  const BattleCell& cell(int x, int y) const { return cells_[static_cast<size_t>(y * cols_ + x)]; }
  bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < cols_ && y < rows_; }
  std::vector<BattleUnit>& units() { return units_; }
  const std::vector<BattleUnit>& units() const { return units_; }

  // The hexes a unit's body covers with its anchor at (x, y) (pathfinding.ts footprint).
  static std::vector<BattleHex> footprint(const BattleUnit& u, int x, int y);
  // The living unit whose body covers (x, y), or -1.
  int unitAt(int x, int y) const;
  // Every hex the unit can end its move on with the movement it has left (computeReachable with stop points
  // pruned), the start included at cost 0. pruneStops = false keeps hexes it may only pass through.
  std::vector<ReachCell> reachable(size_t unit, bool pruneStops = true) const;
  // The walk from the unit's hex to (x, y) through a reach map, start first; empty when (x, y) is not in it.
  static std::vector<BattleHex> path(const std::vector<ReachCell>& reach, int x, int y);
  // Moves the unit to (x, y) along a reachable path, spending its cost. False when (x, y) is not reachable.
  bool move(size_t unit, int x, int y);
  // A new turn: every unit's movement is whole again and it may act.
  void newTurn();
  // The party's affinity bonus between two names (0 = none).
  double affinityBonus(const std::string& a, const std::string& b) const;
  const std::vector<ForecastReference>& forecastReferences() const { return forecastRefs_; }
  const std::vector<RollReference>& rollReferences() const { return rollRefs_; }
  const std::vector<AiReference>& aiReferences() const { return aiRefs_; }

  // --- Turns (Engine2 BattleEngine: one battle-opening initiative roll, a round in initiative order) ---
  // Every unit that takes turns (alive, not neutral) rolls 1d20 + its bonus; the order is highest first, the party
  // winning a draw, then the unit's id.
  template <typename Rng>
  void rollInitiative(Rng& rng) {
    for (BattleUnit& u : units_)
      if (u.alive && u.side != "neutral") u.initiative = 1 + static_cast<int>(rng() * 20) + u.initiativeBonus;
    sortTurnOrder();
  }
  // The first unit in this round's order still to act, or -1 when everyone has gone.
  int activeUnit() const;
  // A new round: volatile enmity fades, every unit may act again, the order is sorted again.
  void startNewRound();
  // The unit's own turn begins: its movement pool is whole.
  void beginTurn(size_t unit);
  int round() const { return round_; }
  const std::vector<size_t>& turnOrder() const { return turnOrder_; }
  // "victory" / "defeat" once one side has nobody standing, else empty.
  std::string result() const;

  // --- Enmity (Engine2 enmity.ts): who each enemy goes after ---
  void noteWeaponDamage(size_t attacker, size_t target, int dmg);
  int enmityTarget(size_t enemy) const;  // the hero topping its table, or -1
  // Engine2 terrainDistanceField: the walking cost from a hex to every hex, terrain only.
  std::vector<int> distanceField(BattleHex from) const;  // per cell, -1 = unreachable
  // The enemy's decision for its turn (runAiFor's plain branch: strike the best foe it can reach, else close in).
  AiPlan planEnemy(size_t unit) const;

  // Ember's hex neighbours in its own order (east, then counter-clockwise from the top); not clipped to the board.
  static std::vector<BattleHex> neighbors(int x, int y);
  static int distance(BattleHex a, BattleHex b);

 private:
  std::optional<int> stepCost(int x, int y, const BattleUnit& self, const std::vector<int>& occupancy, bool stop) const;
  std::vector<int> occupancy() const;  // per cell: the unit standing there, or -1

  std::string map_;
  int cols_ = 0, rows_ = 0;
  std::vector<BattleCell> cells_;
  std::vector<BattleUnit> units_;
  std::vector<std::tuple<std::string, std::string, double>> affinity_;
  std::vector<ForecastReference> forecastRefs_;
  std::vector<RollReference> rollRefs_;
  std::vector<AiReference> aiRefs_;
  void sortTurnOrder();
  std::vector<size_t> turnOrder_;
  int round_ = 1;
  struct Enmity {
    size_t enemy, hero;
    double ce = 0, ve = 0;
  };
  std::vector<Enmity> enmity_;
};

}  // namespace ember::e2
