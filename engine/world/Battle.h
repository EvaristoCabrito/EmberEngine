#pragma once

// A battle on an Ember map: the board as the movement rules see it, the units and their movement reach. The rules are
// Ember's (Engine2 src/game/pathfinding.ts: footprint, footprintCost, computeReachable, reconstructPath), ported
// step for step so a unit reaches exactly the hexes, at the costs and along the paths, it does there; Cooker
// check-battle compares every unit's reach with Engine2's own (tools/scripts/engine2/export_battle.ts).
//
// Hexes are (column, row) on Ember's odd-r board: odd rows sit half a hex to the right.

#include <array>
#include <filesystem>
#include <optional>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>

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
  int cover = 0;         // cover with the decorations folded in (hexDef: a spell's target cover)
  int hazardDice = 0, hazardFaces = 8;  // standing here hurts at the start of each own turn (flames, embers)
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
  std::vector<std::pair<std::string, double>> staffBonuses;  // the staff's bonus % per element (spells too)
  double staffLifeStealPct = 0;
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

// Potions in the bag's order (Engine2 GameApp ALL_POTIONS; data.ts POTIONS).
inline constexpr const char* kPotionIds[7] = {"weak", "mid", "potent", "disease", "manaSmall", "manaMid", "manaLarge"};

struct BattleUnit {
  std::string id, side, name, classId, sprite, className;
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

  // --- Spells, training, experience (Engine2 spell-source / BattleEngine) ---
  int spells[10] = {};  // uses left per tier (Engine2 Unit.spells tier1..tier10)
  std::vector<std::string> spellList;  // what it may cast (classSpells for its class, level and name)
  int shockCharges = 0, frostCharges = 0, fantomForceCharges = 0;
  int fantomDice = 0, fantomFaces = 0;
  int xp = 0;
  bool guaranteedDrop = false;
  bool hidden = false;  // under the fog of war for the party: cannot be aimed at (unitHidden)
  std::string weaponType;     // its weapon's type ("bow", "sword", ...), empty = none
  bool trainsWeapon = false;  // a hero whose weapon type its class trains (weaponSkills.ts)
  double weaponSkill = 0, healingSkill = 0;
  // its off-hand weapon's type, whether its class trains it, and its skill (an off-hand counter trains it)
  std::string offWeaponType;
  bool offTrainsWeapon = false;
  double offWeaponSkill = 0;
  std::vector<std::pair<std::string, double>> skills;  // trained skills by id ("fireResistance", ...)
  // A hero's growth: per level 1..30 the class's hp atk mag def dex and its uses per tier (levelUpUnit), and what its
  // gear adds over the class's stats.
  std::vector<std::array<int, 16>> growth;  // hp atk mag def dex, tier1..tier10 uses, mov
  int gearHp = 0, gearAtk = 0, gearMag = 0, gearDef = 0, gearDex = 0, gearMov = 0;
  struct Echo {
    int dice = 0, faces = 0, bonus = 0;
    double mag = 0;
  };
  std::optional<Echo> shock;  // a lightning echo that strikes when its own turn begins

  // --- Items and conditions a hero brings into the fight (Engine2 Unit.bag / diseased / poisoned / fullness) ---
  int bag[7] = {};  // potions carried, in kPotionIds order
  bool diseased = false;
  struct Stats {
    int atk = 0, mag = 0, def = 0, dex = 0, mov = 0;
  };
  std::optional<Stats> diseaseBase;  // its stats before the disease's penalty (curePlayerDisease restores them)
  bool poisoned = false;
  std::string poisonTier;            // save.heroPoisons' value ("lesser", ...), kept for the save
  std::optional<double> fullness;    // hunger.ts fullness 0..120; unset reads as 100
  double hungerPenaltyPct = 0;       // a starving hero's stat loss (0..0.9), gone once fed
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
  bool spell = false;           // Engine2 cast something instead
  struct Cast {
    std::string kind;
    std::vector<int> ids;
    std::vector<BattleHex> tiles;
  };
  std::vector<Cast> casts;      // what it cast (its spell steps)
};
// One reference cast (export_battle.ts casts): the caster placed at `at`, the spell aimed at `cell`; when valid, the
// seed it was cast with, how many random numbers it drew and every unit afterwards (hp, alive, echo, maxHp, level, xp)
// plus the caster's charges.
struct CastReference {
  int caster = 0;
  std::string kind;
  BattleHex cell, at;
  bool valid = false;
  uint32_t seed = 0;
  int calls = 0;
  struct After {
    int hp = 0, maxHp = 0, level = 0, xp = 0;
    bool alive = false;
    std::optional<BattleUnit::Echo> shock;
    // the support casts' scene (`hurt`): conditions and hunger afterwards
    bool hasStatus = false;
    int atk = 0, mag = 0, def = 0, dex = 0, mov = 0;
    bool diseased = false, poisoned = false;
    double fullness = 100;
    int spells[10] = {};
    int bag[7] = {};
  };
  std::vector<After> after;
  int spells[10] = {};
  int shockCharges = 0, frostCharges = 0;
  // Cast in the hurt scene (export_battle.ts hurtScene: everyone at half HP, some diseased / poisoned, all hungry).
  bool hurt = false;
  int lootRations = 0;
  std::string potion;  // a potion used instead of a spell (its id; `kind` is then "potion")
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
  // Engine2 BattleEngine debugFreeCast (Modo teste's battles, export_battle.ts TEST_LEVEL): spells may be aimed at any
  // visible unit, allies included, and cures at anyone, hurt or not.
  bool debugFreeCast() const { return debugFreeCast_; }
  // An explore mission (Engine2 mission.explore) roams free: no turns, any reachable hex in one walk, until the party
  // meets an enemy (Ember's rule, the user's: then the fight starts with every unit's own movement).
  bool roaming() const { return roaming_; }
  void endRoaming() { roaming_ = false; }
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
  const std::vector<CastReference>& castReferences() const { return castRefs_; }
  // Engine2's enmity hooks for spells and support (spell damage, heals noticed by every enemy).
  void noteDamage(size_t attacker, size_t target, int dmg, bool spell);
  void noteAware(size_t hero, double ce, double ve);
  // Rations conjured in this fight (Engine2 lootRations: Criar Comida e Água), added to the save's at its end.
  int lootRations() const { return lootRations_; }
  void addLootRations(int n) { lootRations_ += n; }

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

  // --- A fight in progress, saved and resumed (Engine2 captureSnapshot / applySnapshot; world/BattleSave.cpp) ---
  // The units' live state, the round ("turn"), its initiative order and the enmity tables, in BattleSnapshot's names.
  nlohmann::json snapshot() const;
  // Overlays a snapshot on this battle (just loaded); false, and nothing changed, when its units are not this battle's.
  bool applySnapshot(const nlohmann::json& snap);

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
  bool debugFreeCast_ = false;
  bool roaming_ = false;
  int cols_ = 0, rows_ = 0;
  std::vector<BattleCell> cells_;
  std::vector<BattleUnit> units_;
  std::vector<std::tuple<std::string, std::string, double>> affinity_;
  std::vector<ForecastReference> forecastRefs_;
  std::vector<RollReference> rollRefs_;
  int lootRations_ = 0;
  std::vector<AiReference> aiRefs_;
  std::vector<CastReference> castRefs_;
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
