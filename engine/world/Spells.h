#pragma once

// Spells and weapon skills on a battle's board: Ember's rules (Engine2 src/rules/spell-source.ts, the engine's cast /
// aim / resolve methods with presentation removed, and src/game/data.ts for their numbers), ported step for step with
// JavaScript's number rules and Engine2's random-number order, so the same seed gives the same result. Cooker
// check-battle casts every ported spell at every hex and compares the board afterwards with Engine2's own cast.
//
// Ported: Míssil Mágico, Tiro Longo, Tiro Perfurante, Investida Perfurante, Golpe Duplo, Relâmpago (+ its echo), Frost,
// Choque (enemy charges), Cura Menor, Cura Média, Mãos Curativas, Curar Doença, Criar Comida e Água, the battle's potions,
// the disease of a bite; weapon-skill / resistance / healing training, experience and level-ups, a death's
// loot roll, start-of-turn echo and hazards. Not yet: the other spells (area, summons, status zones, bleeding).
//
// EmberEngine's own (2026-10-17, the user's "pode criar todas elas"): the Mago Negro's two promotions, 2 spells per
// tier from T6 to T10 (kEmberSpells in Spells.cpp): the Bruxo (enfeeblement and damage over time: marks, poisons that
// spread, bleeding, silence, a decaying curse, a life drain, an execution) and the Elementalista (pure damage and area:
// fire, ice, lightning and Brasa; lines, cones, chains, zones that burn, a meteor that falls a turn later). They use the
// game's own formula (floor(floor(MAG / 2) × weight) + dice, the element's resistance, MAG / 2 of penetration) and
// Engine2's poison tiers (poisonTiers: Veneno Maior / Mortal / Letal), which tick here at the start of the victim's
// turn as Engine2's help describes. Numbers: docs/SPELLS_PROMOTIONS.md.

#include <optional>
#include <string>
#include <vector>

#include "world/Battle.h"
#include "world/Combat.h"

namespace ember::e2 {

// One queued action (Engine2 Seq): a spell's hits, a weapon strike (Golpe Duplo, a plain attack) or a heal.
struct BattleStep {
  enum class Type { Spell, Combat, Heal, CureDisease, Food, Potion, Move, Effect };
  Type type = Type::Spell;
  size_t att = 0;
  std::string kind;  // the spell (empty = a plain attack)
  // Spell
  std::vector<BattleHex> tiles;
  std::vector<size_t> ids;  // the units it hits, in order
  int dice = 0, faces = 8, bonus = 0;
  double spellMul = 1;
  int weaponBonusDice = 0, weaponBonusFaces = 8, weaponBonusBonus = 0;
  double dmgMul = 1;
  std::optional<BattleUnit::Echo> echo;  // lightning: what strikes again when the target's turn begins
  // Combat / heal
  size_t def = 0;
  bool noCounter = false;
  int bonusDice = 0, bonusDiceCount = 1, bonusFlat = 0;  // an extra die on a landed strike (bonusDice = faces)
  // Engine2's SpellAnim / CombatAnim fields for the rest of the skills
  std::optional<size_t> centerId;  // Bola de Fogo-style: the aimed unit rolls the centre dice, everyone else the splash
  double centerMul = 1;
  int centerDice = 0, centerFaces = 0, centerBonus = 0;
  bool poison = false;             // each landed hit may poison (Veneno Cáustico, Sopro Venenoso)
  int wallDice = 0, wallFaces = 0;  // Investida Touro: the impact dice when the shove is blocked short
  std::optional<BattleHex> knockTo;  // ... and where the struck is shoved
  double stunChance = 0;           // an off-hand shield bash
  bool customDice = false;         // an off-hand weapon strike: its own dice
  int customDiceCount = 1, customFaces = 4, customBonus = 0;
  std::vector<BattleHex> path;     // Type::Move: the walk (start first)
  // Food: the fullness it brings (its rations roll is dice / faces / bonus); Potion: the potion's id
  double fullness = 0;
  std::string potion;

  // --- EmberEngine's own spells (the Bruxo's and the Elementalista's T6-T10; see kEmberSpells in Spells.cpp) ---
  std::vector<double> perTargetMul;  // a weight per entry of `ids` (empty = 1 each): Lança de Gelo, Cadeia de Raios, Zero Absoluto
  std::vector<size_t> echoIds;       // Tempestade Elétrica: struck again at once by echoDice (no MAG weight, lightning resistance)
  int echoDice = 0, echoFaces = 0, echoBonus = 0;
  int drainPct = 0;                  // Dreno de Vida: the caster heals this % of the damage dealt
  bool harvest = false;              // Colheita de Almas: every condition on the struck ticks once more, now
  int harvestHealPct = 0;            // ... and the caster heals this % of its max HP per enemy struck
  int execPct = 0;                   // Vazio Devorador: a target left under this % of its HP dies
  int shredDarkness = 0;             // ... and loses this much Escuridão resistance for the rest of the fight
  int push = 0;                      // Vendaval Glacial: every struck unit is pushed this many hexes away from the caster
  bool delayed = false;              // Meteoro: nothing lands now (its zone waits for the caster's next turn)
  // Conditions the hits leave (Battle.h BattleUnit::Conditions)
  int markPct = 0, markTurns = 0;
  std::string poisonTier;            // "greater", "deadly", "lethal" (Engine2 poisonTiers), applied to every struck unit
  int plagueSpread = 0;
  bool plagueBurst = false;
  int bleedDice = 0, bleedFaces = 0, bleedTurnsDice = 0, bleedTurnsFaces = 0;
  int silenceTurns = 0;
  int decayStep = 0, decayCap = 0, decayTurns = 0, decayDice = 0, decayFaces = 0;
  double decayMul = 0;
  // The Guerreiro's skills (docs/SKILLS_WARRIOR.md)
  int sunderPct = 0, sunderTurns = 0;  // Quebra-Guarda: the struck foe's DEF down
  int rallyPct = 0, rallyTurns = 0;    // Grito de Guerra: every unit of `ids` gets ATK up ...
  std::vector<size_t> fearIds;         // ... and these foes are afraid for fearTurns turns
  int fearTurns = 0;
  int hobblePct = 0, hobbleTurns = 0;  // Tiro Paralisante (the Arqueiro's): the struck foe's MOV down
};

// What a step did, for the window: a number over a unit, a miss, a heal, a death.
struct BattleEffect {
  size_t unit = 0;
  int amount = 0;
  bool crit = false, miss = false, heal = false, died = false;
  bool counter = false;  // the defender striking back
  std::string text;      // shown instead of the number ("curado", "+2 rações", ...)
  bool status = false;   // a condition's word ("Marcado", "Envenenado", ...): no hit pose, its own colour
  bool abortRush = false;  // a missed Investida Touro: the rest of the charge is dropped
  bool moved = false;      // Type::Move / knockback: the unit is on another hex now
  bool pushed = false;   // the unit was moved to another hex (Vendaval Glacial): the window re-places its card
  std::string cause;     // what hurt it at the start of its turn ("Sangramento fere", ...): the combat log's verb
};

// --- EmberEngine's own spells: the two promotions of the Mago Negro (user, 2026-10-16/17: "2 magias por tier do 6 ao
// 10"; Bruxo = enfraquecimento e dano no tempo, Elementalista = dano puro e área) ---
// The class a kind belongs to ("warlock" / "elementalist"), empty for Engine2's own spells.
std::string emberSpellClass(const std::string& kind);
// The level a class learns it at (0 = not one of ours).
int spellLearnLevel(const std::string& kind);
// The kinds a class knows at a level, in learning order (empty for Engine2's classes).
std::vector<std::string> emberSpells(const std::string& classId, int level);
// Appends to the unit's spell list what its class knows at its level and does not have yet (level-ups call it too).
void grantClassSpells(BattleUnit& u);
// Promotes (or demotes) a hero: its class id and Portuguese class name, and its spell list rebuilt (Engine2's own spells
// kept, ours replaced). "" / "mage" = the Mago Negro again.
void setPromotion(BattleUnit& u, const std::string& classId);
// The Portuguese class name ("Bruxo", "Elementalista", "Mago Negro"; the id itself when unknown).
std::string promotionName(const std::string& classId);
// A unit's conditions in words, for the window ("Marcado +20% · Veneno Maior · Silenciado 2"), empty when none.
std::string conditionsText(const BattleUnit& u);
// The spell's description for the window's tooltip (its numbers at the caster's level).
std::string spellTooltip(const BattleUnit& caster, const std::string& kind);

// The combat log's language for the lines made from now on (the window's Idioma choice).
void setCombatLogEnglish(bool english);
bool combatLogEnglish();

// Spell names as the window shows them (Engine2's own Portuguese names).
std::string spellName(const std::string& kind);
// ... in the chosen language (English names for the combat log in English; Portuguese otherwise).
std::string spellNameIn(const std::string& kind, bool english);
// A spell EmberEngine casts (the rest of a unit's list is shown but not castable yet).
bool spellPorted(const std::string& kind);
// A weapon skill (Engine2 isWeaponAbility): it rolls the weapon's own hit chance on a foe.
bool spellWeaponAbility(const std::string& kind);
// A spell cast at once, with no aim (Criar Comida e Água): cast it at the caster's own hex.
bool spellInstant(const std::string& kind);
// A spell centred on its caster whose area is shown first, then confirmed (Varredura, Expulsar Mortos-Vivos).
bool spellSelfArea(const std::string& kind);
// A spell aimed by direction from the caster (a line or a cone through any hex): every hex is a valid aim, so the window
// shows the line / cone under the cursor and the foes it can reach, not a zone.
bool spellAimsDirection(const std::string& kind);

// Potions (data.ts POTIONS; their ids in the bag's order: kPotionIds, Battle.h).
int potionIndex(const std::string& id);  // -1 when unknown
std::string potionName(const std::string& id);  // potionLabel: name and dice
// A potion may go on the hex (validPotionTarget: the actor itself or an adjacent ally); `error` = potionTargetError.
bool potionTargetValid(const Battle& b, size_t actor, BattleHex cell, std::string* error = nullptr);
// Engine2 usePotion + confirmPotionAt: the step that drinks / hands the potion (applyPotion when it plays), or nothing with
// the reason (applyPotion's own refusals included: full HP, not sick, spells full). It spends the actor's action.
std::optional<BattleStep> usePotion(const Battle& b, size_t actor, const std::string& id, BattleHex cell, std::string* error = nullptr);
// The disease's effect (maybeInflictDisease once its roll lands): 10% off atk / mag / def / dex / mov, kept to give back.
void inflictDisease(BattleUnit& u);
// Engine2 reapplyGear: the stats again from its class at its level and its gear, less hunger and disease.
void reapplyGear(BattleUnit& u);
// Engine2 spawnUnit's stats for a hero brought into a fight hungry or sick (its own rounding, not reapplyGear's).
void spawnConditions(BattleUnit& u);
// The support casts' reference scene (export_battle.ts hurtScene): every living unit at half HP, the party's even-indexed
// units diseased, every third unit poisoned, everyone hungry (fullness 0), every tier's uses halved.
void hurtScene(Battle& b);
// The pose a cast plays (Engine2 startSeq: arrows and blade skills swing, cures heal, the rest cast).
std::string spellPose(const std::string& kind);
// Uses left (Engine2 remainingSpellUses).
int spellUsesLeft(const Battle& b, size_t unit, const std::string& kind);
// The tier a spell draws its uses from (Engine2 SPELL_TIER; 0 = none): the status sheet's "Tier N".
int spellTierNumber(const std::string& kind);
// How many hexes the cast takes (Míssil Mágico: one per missile, which may repeat).
int spellTargetCount(const Battle& b, size_t unit, const std::string& kind);
// The spell's zone: every hex within its reach from the caster (range, the shot's line) before asking for a target in
// it, so the window can show where it may be aimed even when nothing stands there; rays (Frost, Tiro / Investida
// Perfurante) give the hexes where the ray is valid; empty for an instant spell.
std::vector<BattleHex> spellZone(const Battle& b, size_t caster, const std::string& kind);
// Whether the unit may aim this spell at the hex (spellAimValid); `error` gets the window's reason when not.
bool spellAimValid(const Battle& b, size_t caster, const std::string& kind, BattleHex cell, std::string* error = nullptr);
// The hexes the spell would cover aimed at `cell` (lines and Frost), for the aim marks; the cell itself otherwise.
std::vector<BattleHex> spellArea(const Battle& b, size_t caster, const std::string& kind, BattleHex cell);
// Casts it (Engine2 castSpell): checks the class, the uses and every aim, spends the use and returns the steps to play,
// or nothing (with the reason) when it cannot be cast.
// checkUses = false skips the class and uses gate (the cast methods themselves do not check them).
std::optional<std::vector<BattleStep>> castSpell(Battle& b, size_t caster, const std::string& kind, const std::vector<BattleHex>& cells,
                                                 std::string* error = nullptr, bool checkUses = true);
// Plays one step (Engine2 drain: resolveSpellSource / resolveCombat with its counter / the heal), drawing from `rng`.
void resolveStep(Battle& b, const BattleStep& step, Mulberry32& rng, std::vector<BattleEffect>& out);
// One strike of a combat step: `counter` = the defender striking back (resolveCombatSource).
void resolveStrike(Battle& b, const BattleStep& step, bool counter, Mulberry32& rng, std::vector<BattleEffect>& out);
// Whether the defender strikes back after the step's strike (resolveCombat's condition).
bool strikesBack(const Battle& b, const BattleStep& step);
// A summoned unit of this class as it stands when a saved fight is resumed (its numbers are then overlaid from the snapshot).
std::optional<BattleUnit> summonSkeleton(const std::string& id, const std::string& classId);
// Engine2 applyTileHazard for a unit that just stepped onto a hex (a flame under its feet burns it).
void stepOnHex(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);
// Engine2 tryWarpCrossing: the hex a unit standing on one opening of a warp gate comes out at, beside the other, or nothing.
std::optional<BattleHex> warpCrossing(const Battle& b, size_t unit);
// Plays steps one after another (Engine2's queue): a missed Investida Touro drops the rest of its charge.
void playSteps(Battle& b, const std::vector<BattleStep>& steps, Mulberry32& rng, std::vector<BattleEffect>& out);
// Cooker check-battle: a unit's whole state as numbers, the zones and the enmity entries (compared with Engine2's own).
std::vector<double> unitStateNumbers(const BattleUnit& u);
nlohmann::json zoneStateNumbers(const Battle& b);
nlohmann::json enmityNumbers(const Battle& b);
// Engine2 commitOffHandAction ("Mão Secundária"): the blow of what the off hand holds, from where the unit stands. A dagger / katar strikes with its
// own dice (and its own sheet); a shield bashes (75% of the blow by default, a 70% chance to stun for a turn; Golpe de Escudo's stun on a hit). Empty when
// the foe is out of its reach or the off hand holds neither.
std::optional<BattleStep> offHandStep(const Battle& b, size_t unit, size_t foe);
// Engine2 useLockpick ("Arrombar"): the unit spends a Gazua on the lock beside it. The cells it changes open up; a chest gives Ember, a potion for whoever has room
// (the opener first, then the turn order), maybe a piece of gear and maybe rations, all of it rolled in Engine2's order; a bleeding opener pays the 1D8, and
// the action is done (it builds affinity with its neighbours). False when there is nothing to open or no Gazua.
bool useLockpick(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);
// BattleEngine.gainAdjacentAffinity: an action done by a hero builds +0.1 with each hero fighting beside it (not those it already helped
// this action); the support recipients are then forgotten. Called as the action ends, before the unit counts as having acted.
void gainAdjacentAffinity(Battle& b, size_t unit);
// Engine2 confirmFinish: the player ends the mission. An escape waypoint rolls the hero's DEX chance (a failed try spends its turn and the fight
// goes on); anything else ends it. `escapeChance` is the chance shown for an escape waypoint, `hero` who stands on it.
struct FinishOutcome {
  bool ended = false, escapeFailed = false;
  size_t hero = 0;
};
FinishOutcome confirmFinish(Battle& b, Mulberry32& rng);
// What a unit's turn becomes once it has begun (Engine2 beginUnitTurn after startOfTurnEffects): it plays on, it loses the turn
// (dead, stunned, asleep) or it flees in fear along `fleePath` and cannot act.
struct TurnStart {
  enum class Kind { Normal, Lost, Fled } kind = Kind::Normal;
  std::vector<BattleHex> fleePath;
  std::string tip;
};
// Engine2 beginUnitTurn: its start-of-turn effects, then stun, the web's sleep roll, sleep and fear, and whether its cell is webbed
// (turnRestrained). The caller has already called Battle::beginTurn.
TurnStart beginUnitTurn(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);
// Engine2 applyBleedingActionDamage: a bleeding unit takes 1D8 whenever it acts (once per own turn when it walks). False when it bled
// out. `isMove`: it is walking.
bool bleedOnAction(Battle& b, size_t unit, bool isMove, Mulberry32& rng, std::vector<BattleEffect>& out);
// The start of a unit's own turn (startOfTurnEffects): the lightning echo, the ground's hazard.
void startOfTurnEffects(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);

// The enemy AI's spell branches (runAiFor: cultists, crossbowmen): the steps of the spell it casts this turn (its walk
// first, as a path), or nothing when it casts nothing.
struct AiCast {
  std::vector<BattleHex> path;
  std::vector<BattleStep> steps;
};
std::optional<AiCast> planEnemyCast(Battle& b, size_t unit);

// A hero brought to the campaign's level and XP before the fight (save.ts levels / xp): Engine2's levelUpUnit once per
// level above the battle's own, so its class stats, HP and tier uses grow as they did in play.
void raiseToLevel(BattleUnit& u, int level, int xp);

}  // namespace ember::e2
