#pragma once

// Spells and weapon skills on a battle's board: Ember's rules (Engine2 src/rules/spell-source.ts, the engine's cast /
// aim / resolve methods with presentation removed, and src/game/data.ts for their numbers), ported step for step with
// JavaScript's number rules and Engine2's random-number order, so the same seed gives the same result. Cooker
// check-battle casts every ported spell at every hex and compares the board afterwards with Engine2's own cast.
//
// Ported: Míssil Mágico, Tiro Longo, Tiro Perfurante, Investida Perfurante, Golpe Duplo, Relâmpago (+ its echo), Frost,
// Choque (enemy charges), Cura Menor, Cura Média, Mãos Curativas, Curar Doença, Criar Comida e Água, the battle's potions,
// the disease of a bite; weapon-skill / resistance / healing training, experience and level-ups, a death's
// loot roll, start-of-turn echo and hazards. Not yet: the other spells (area, summons, status zones, poison, bleeding).

#include <optional>
#include <string>
#include <vector>

#include "world/Battle.h"
#include "world/Combat.h"

namespace ember::e2 {

// One queued action (Engine2 Seq): a spell's hits, a weapon strike (Golpe Duplo, a plain attack) or a heal.
struct BattleStep {
  enum class Type { Spell, Combat, Heal, CureDisease, Food, Potion };
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
  // Food: the fullness it brings (its rations roll is dice / faces / bonus); Potion: the potion's id
  double fullness = 0;
  std::string potion;
};

// What a step did, for the window: a number over a unit, a miss, a heal, a death.
struct BattleEffect {
  size_t unit = 0;
  int amount = 0;
  bool crit = false, miss = false, heal = false, died = false;
  bool counter = false;  // the defender striking back
  std::string text;      // shown instead of the number ("curado", "+2 rações", ...)
};

// Spell names as the window shows them (Engine2's own Portuguese names).
std::string spellName(const std::string& kind);
// A spell EmberEngine casts (the rest of a unit's list is shown but not castable yet).
bool spellPorted(const std::string& kind);
// A spell cast at once, with no aim (Criar Comida e Água): cast it at the caster's own hex.
bool spellInstant(const std::string& kind);

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
// The support casts' reference scene (export_battle.ts hurtScene): every living unit at half HP, the party's even-indexed
// units diseased, every third unit poisoned, everyone hungry (fullness 0), every tier's uses halved.
void hurtScene(Battle& b);
// The pose a cast plays (Engine2 startSeq: arrows and blade skills swing, cures heal, the rest cast).
std::string spellPose(const std::string& kind);
// Uses left (Engine2 remainingSpellUses).
int spellUsesLeft(const Battle& b, size_t unit, const std::string& kind);
// How many hexes the cast takes (Míssil Mágico: one per missile, which may repeat).
int spellTargetCount(const Battle& b, size_t unit, const std::string& kind);
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
