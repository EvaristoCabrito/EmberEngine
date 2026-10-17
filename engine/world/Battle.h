#pragma once

// A battle on an Ember map: the board as the movement rules see it, the units and their movement reach. The rules are
// Ember's (Engine2 src/game/pathfinding.ts: footprint, footprintCost, computeReachable, reconstructPath), ported
// step for step so a unit reaches exactly the hexes, at the costs and along the paths, it does there; Cooker
// check-battle compares every unit's reach with Engine2's own (tools/scripts/engine2/export_battle.ts).
//
// Hexes are (column, row) on Ember's odd-r board: odd rows sit half a hex to the right.

#include <array>
#include <map>
#include <set>
#include <filesystem>
#include <optional>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
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
  bool offHandShield = false;  // a shield in the off hand (Golpe de Escudo needs one; Investida de Ombro none)
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
  bool undead = false;  // creatureType "undead" (Expulsar Mortos-Vivos)
  bool boss = false;    // a boss class: a "boss" mission is won when none stands
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
  // The level-up points spent on each attribute (Engine2 statPointAllocation: three per level, +1 to the stat each).
  int pointHp = 0, pointAtk = 0, pointMag = 0, pointDef = 0, pointDex = 0;
  struct Echo {
    int dice = 0, faces = 0, bonus = 0;
    double mag = 0;
  };
  std::optional<Echo> shock;  // a lightning echo that strikes when its own turn begins

  // --- EmberEngine's own conditions (the Bruxo's spells, world/Spells.cpp "Ember spells"): each counts down in the
  // victim's own turns (applied at the start of its turn, then one less) ---
  struct Conditions {
    int markPct = 0, markTurns = 0;      // Marca da Ruína: takes +pct% damage from everything
    int plagueSpread = 0;                // Praga Rastejante: its poison jumps to this many adjacent allies each turn
    bool plagueBurst = false;            // Peste Negra: dying poisoned spreads Veneno Maior to its neighbours
    double poisonMag = 0;                // the MAG of whoever poisoned it (the tick's resistance penetration)
    int bleedDice = 0, bleedFaces = 0, bleedTurns = 0;  // Hemorragia: bleeds this at each turn; heals received halved
    int silenceTurns = 0;                // Silêncio Profano: cannot cast
    int decayTurns = 0, decayPct = 0, decayCap = 0, decayStep = 0;  // Maldição da Decadência: -step% of every stat per turn, to the cap
    int decayDice = 0, decayFaces = 0;
    double decayMul = 0, decayMag = 0;   // ... and its Escuridão tick: floor(floor(mag / 2) * mul) + dice
    bool any() const { return markTurns > 0 || plagueSpread > 0 || plagueBurst || bleedTurns > 0 || silenceTurns > 0 || decayTurns > 0; }
  } cond;

  // --- Items and conditions a hero brings into the fight (Engine2 Unit.bag / diseased / poisoned / fullness) ---
  int bag[7] = {};  // potions carried, in kPotionIds order
  int lockpick = 0;  // Engine2 bag.lockpick (the sheet lists them: "Gazua")
  bool diseased = false;
  struct Stats {
    int atk = 0, mag = 0, def = 0, dex = 0, mov = 0;
  };
  std::optional<Stats> diseaseBase;  // its stats before the disease's penalty (curePlayerDisease restores them)
  std::optional<Stats> decayBase;    // its stats before Maldição da Decadência (given back when the curse lifts)
  bool poisoned = false;
  std::string poisonTier;            // save.heroPoisons' value ("lesser", ...), kept for the save
  std::optional<double> fullness;    // hunger.ts fullness 0..120; unset reads as 100
  double hungerPenaltyPct = 0;       // a starving hero's stat loss (0..0.9), gone once fed

  // --- Engine2's status conditions (Engine2 Unit.stunned / asleep / bleeding / crippled / fearTurns / blessed...) ---
  bool stunned = false;              // loses its next turns (Golpe de Escudo, an off-hand bash)
  int stunTurns = 0;
  bool asleep = false;               // Teia dos Sonhos: loses its turns, wakes on a hit (which hurts 25% more)
  int sleepTurns = 0;
  bool bleeding = false;             // 1D8 whenever it acts (Tiro Sangrento for some rounds, Rasteira for good)
  std::optional<int> bleedRoundsLeft;   // rounds left of Tiro Sangrento's bleed (none = until cured, Rasteira)
  std::optional<int> bleedRoundMarker;  // the round it was last counted down in
  bool bleedMovedThisTurn = false;   // walking opens the wound once per own turn
  bool crippled = false;             // Rasteira: 10% off every stat for the rest of the fight
  int fearTurns = 0;                 // Expulsar Mortos-Vivos: flees instead of fighting
  std::string fearSourceId;
  double blessedHitBonusPct = 0;     // Bênção: +hit chance (a fraction) for some rounds
  int blessedRoundsLeft = 0;
  std::string summonerId;            // a familiar's conjurer
  int spellCharges = 0;              // a familiar's own spell (Míssil Mágico / Bola de Fogo / Veneno Menor)
  std::optional<int> lifeDrainCharges;  // a Familiar Maior / Radiante's Dreno de Vida
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
  // A skill cast (export_battle.ts skillCasts): every unit's state afterwards as numbers (Spells.h unitStateNumbers), the
  // zones and the enmity entries.
  bool skill = false;
  std::vector<std::vector<double>> ext;
  nlohmann::json zones, enmity;
  int unitCount = 0;
  int patchTier = 0, patchValue = 0;  // the caster was given this tier's uses first (a start-method needs some)
  nlohmann::json affinity;            // the pairs' scores afterwards
};

// A unit's turn beginning under a condition (export_battle.ts turnStarts): the board right after Engine2's beginUnitTurn.
struct TurnStartReference {
  int unit = 0;
  std::string scenario;
  uint32_t seed = 0;
  int calls = 0;
  std::vector<std::vector<double>> ext;
  nlohmann::json zones, enmity;
  bool moved = false, acted = false, restrained = false;
  std::vector<BattleHex> flee;
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
  // The party's affinity bonus between two names (0 = none): from the pair's score (Engine2 affinity.ts affinityBonus: 25 -> 2%,
  // 50 -> 5%, 90 -> 8%), or the file's own list for a battle file that carries no scores.
  double affinityBonus(const std::string& a, const std::string& b) const;
  // The pairs' scores 0..100 ("A|B" with the names in order), which a fight moves (friendly fire -1, healing +0.2, fighting side by
  // side +0.1) and the campaign's save keeps.
  const std::map<std::string, double>& affinityScores() const { return affinityScores_; }
  void setAffinityScores(std::map<std::string, double> scores) { affinityScores_ = std::move(scores), scoresKnown_ = true; }
  // BattleEngine.adjustHeroAffinity: both must be of the six heroes; the score is kept to a tenth within 0..100; the change is logged.
  void adjustAffinity(const std::string& a, const std::string& b, double delta);
  std::map<size_t, std::set<size_t>>& supportRecipients() { return supportRecipients_; }
  // Drops Engine2's reference answers (a copy of the battle for one cast does not need them).
  void clearReferences() { forecastRefs_.clear(), rollRefs_.clear(), aiRefs_.clear(), castRefs_.clear(), turnRefs_.clear(); }
  const std::vector<ForecastReference>& forecastReferences() const { return forecastRefs_; }
  const std::vector<RollReference>& rollReferences() const { return rollRefs_; }
  const std::vector<AiReference>& aiReferences() const { return aiRefs_; }
  const std::vector<CastReference>& castReferences() const { return castRefs_; }
  const std::vector<TurnStartReference>& turnStartReferences() const { return turnRefs_; }
  // Engine2's enmity hooks for spells and support (spell damage, heals noticed by every enemy).
  void noteDamage(size_t attacker, size_t target, int dmg, bool spell);
  void noteAware(size_t hero, double ce, double ve);
  // Every enmity entry as [enemy, hero, ce, ve], sorted (the cast check compares them with Engine2's table).
  std::vector<std::array<double, 4>> enmityDump() const;
  // Engine2 addEnmity: this much enmity of `foe` for `hero` (an enemy and a hero, both standing).
  void addEnmity(size_t foe, size_t hero, double ce, double ve);
  // Rations conjured in this fight (Engine2 lootRations: Criar Comida e Água), added to the save's at its end.
  int lootRations() const { return lootRations_; }
  void addLootRations(int n) { lootRations_ += n; }
  // The combat log (Engine2 engine.ts log / pushLog): attacks, spells, heals, kills, conditions, skill gains and level-ups,
  // newest last, capped at 200 lines. Saved in the snapshot.
  const std::vector<std::string>& log() const { return log_; }
  void pushLog(std::string line) {
    log_.push_back(std::move(line));
    if (log_.size() > 200) log_.erase(log_.begin());
  }

  // --- Engine2's zones (engine.ts webZones / iceStormZones / auraZones / warpGates), each one round shorter at every new round ---
  struct WebZone {  // Teia dos Sonhos: anyone starting its turn inside may fall asleep; its movement is clamped to 1 hex
    std::vector<BattleHex> cells;
    int roundsLeft = 0;
    BattleHex center;
    int radius = 0;
    double sleepChance = 0.25;
  };
  struct IceZone {  // Tempestade de Gelo: hurts whoever starts its turn inside
    std::vector<BattleHex> cells;
    int roundsLeft = 0;
    BattleHex center;
    int radius = 0;
    int damageDice = 1, damageFaces = 6;
    double damageMul = 0.65;
    double casterMag = 0;
    int casterLevel = 1;
    std::string casterId;
    std::string side;
  };
  struct AuraZone {  // Aura de Proteção / Presença Intimidante
    std::vector<BattleHex> cells;
    int roundsLeft = 0;
    std::string kind;  // "protection" | "intimidation"
    std::string side;
    double pct = 0;
  };
  struct WarpGate {  // Portal: a pair of gates a unit may cross
    BattleHex a, b;
    int roundsLeft = 0;
    int level = 1;
  };
  std::vector<WebZone>& webZones() { return webZones_; }
  const std::vector<WebZone>& webZones() const { return webZones_; }
  std::vector<IceZone>& iceZones() { return iceZones_; }
  const std::vector<IceZone>& iceZones() const { return iceZones_; }
  std::vector<AuraZone>& auraZones() { return auraZones_; }
  const std::vector<AuraZone>& auraZones() const { return auraZones_; }
  std::vector<WarpGate>& warpGates() { return warpGates_; }
  const std::vector<WarpGate>& warpGates() const { return warpGates_; }
  // engine.ts isWebCell / webCellSleepChance / zoneDamageMul
  bool isWebCell(int x, int y) const;
  double webCellSleepChance(int x, int y) const;
  double zoneDamageMul(const BattleUnit& defender) const;
  // The unit begins its turn restrained (its cell is webbed): its movement is clamped to one hex for the turn.
  bool turnRestrained() const { return turnRestrained_; }
  void setTurnRestrained(bool v) { turnRestrained_ = v; }

  // --- Zones (EmberEngine's own: the Elementalista's burning ground and Meteoro, the Bruxo's Mortalha de Sombras) ---
  // A zone lies on the hexes within `radius` of its centre (`ring`: only at exactly that distance) for `rounds` rounds
  // (one less at every new round; gone at 0). What it does to whoever starts a turn inside: a hazard (dice of its
  // element, like Engine2's flames), a weighted tick (floor(floor(mag / 2) * mul) + dice of its element) and a stat
  // penalty while inside (statPct % off ATK and MAG). "meteor": nothing until its caster's next turn begins, when it
  // lands (world/Spells.cpp startOfTurnEffects).
  struct Zone {
    int id = 0;
    std::string kind;
    BattleHex centre;
    int radius = 1, rounds = 1;
    bool ring = false;
    size_t caster = 0;
    std::string element;
    int hazardDice = 0, hazardFaces = 8;
    int dice = 0, faces = 0;
    double mul = 0, mag = 0;
    int statPct = 0;
    // Meteoro: what lands (the centre hex's and the ring's weight and dice)
    double centreMul = 0, ringMul = 0;
    int centreDice = 0, centreFaces = 0, ringDice = 0, ringFaces = 0;
    bool covers(BattleHex h) const {
      const int d = Battle::distance(centre, h);
      return ring ? d == radius : d <= radius;
    }
  };
  const std::vector<Zone>& zones() const { return zones_; }
  int addZone(Zone z) {
    z.id = ++zoneSerial_;
    zones_.push_back(std::move(z));
    return zoneSerial_;
  }
  void removeZone(int id) { std::erase_if(zones_, [&](const Zone& z) { return z.id == id; }); }
  std::vector<Zone>& zonesMutable() { return zones_; }
  // The stat penalty on a unit standing in a Mortalha (the strongest zone it stands in), in %.
  int weakenAt(const BattleUnit& u) const;
  // Whether the unit stands in a zone of this element (Zero Absoluto: ×1.5 inside a Tempestade de Gelo).
  bool inZoneOf(const BattleUnit& u, const std::string& element) const;
  // Meteoros that landed since the last call (their zones, for the window to draw the fall).
  std::vector<Zone> takeLanded() { return std::exchange(landed_, {}); }
  void noteLanded(const Zone& z) { landed_.push_back(z); }

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
  // Engine2 evaluateEnd: "defeat" once no hero stands (a summon does not count), "victory" once the mission was ended
  // (confirmFinish); empty while it goes on. Winning does not end it by itself: winAvailable() offers the end.
  std::string result() const;
  // The mission's win rule ("rout": no enemy stands; "boss": no boss stands; "escape": only a waypoint) and its waypoints.
  const std::string& win() const { return win_; }
  struct Exit {
    std::string id, kind;  // "escape" (a DEX chance to get away), "dungeon" and "connector" (leave at once)
    std::vector<BattleHex> cells;
    std::string targetMapId;
  };
  const std::vector<Exit>& exits() const { return exits_; }
  // The end is on offer: a hero stands on a waypoint, or the win rule is met.
  bool winAvailable() const;
  // The waypoint a living hero is standing on, if any, and that hero.
  const Exit* activeExit(size_t* hero = nullptr) const;
  // Engine2 fleeChance: min(100, 60 + DEX / 3) with the affinity bonus.
  double fleeChance(size_t unit) const;
  void setFinished() { finished_ = true; }
  bool finished() const { return finished_; }

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
  std::vector<std::string> log_;
  std::vector<AiReference> aiRefs_;
  std::vector<CastReference> castRefs_;
  std::vector<TurnStartReference> turnRefs_;
  void sortTurnOrder();
  std::vector<size_t> turnOrder_;
  int round_ = 1;
  struct Enmity {
    size_t enemy, hero;
    double ce = 0, ve = 0;
  };
  std::vector<Enmity> enmity_;
  std::vector<Zone> zones_, landed_;
  int zoneSerial_ = 0;
  std::map<std::string, double> affinityScores_;
  bool scoresKnown_ = false;
  std::map<size_t, std::set<size_t>> supportRecipients_;  // who each unit has already helped this action (gainSupportAffinity)
  std::string win_ = "rout";
  std::vector<Exit> exits_;
  bool finished_ = false;
  std::vector<WebZone> webZones_;
  std::vector<IceZone> iceZones_;
  std::vector<AuraZone> auraZones_;
  std::vector<WarpGate> warpGates_;
  bool turnRestrained_ = false;
};

}  // namespace ember::e2
