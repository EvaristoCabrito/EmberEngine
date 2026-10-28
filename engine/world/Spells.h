#pragma once

// Spells and weapon skills on a battle's board: Ember's rules (Ember src/rules/spell-source.ts, the engine's cast /
// aim / resolve methods with presentation removed, and src/game/data.ts for their numbers), ported step for step with
// JavaScript's number rules and Ember's random-number order, so the same seed gives the same result. Cooker
// check-battle casts every ported spell at every hex and compares the board afterwards with Ember's own cast.
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
// Ember's poison tiers (poisonTiers: Veneno Maior / Mortal / Letal), which tick here at the start of the victim's
// turn as Ember's help describes. Numbers: docs/SPELLS_PROMOTIONS.md.

#include <optional>
#include <string>
#include <vector>

#include "world/Battle.h"
#include "world/Combat.h"

namespace ember {

// One queued action (Ember Seq): a spell's hits, a weapon strike (Golpe Duplo, a plain attack) or a heal.
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
  // Ember's SpellAnim / CombatAnim fields for the rest of the skills
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
  std::string poisonTier;            // "greater", "deadly", "lethal" (Ember poisonTiers), applied to every struck unit
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
  // The Lanceiro's skills (docs/SKILLS_LANCER.md): the board moves ride on a landed hit
  std::optional<BattleHex> throwTo;    // Arremesso / Gancho / Empalar e Arrastar: where the struck foe ends up
  float arc = -1.0f;                   // ... its card's flight there: the arc's height in units (0 = dragged along the ground)
  std::optional<size_t> collideWith;   // Arremesso: the unit it lands against; both take collideDice D collideFaces
  int collideDice = 0, collideFaces = 0;
  std::optional<BattleHex> dragTo;     // Empalar e Arrastar: where the caster ends up, behind the foe
  bool rearSure = false;               // Salto de Vara: the blow comes from behind, whichever way the foe faces
  int disarmPct = 0, disarmTurns = 0;  // Desarmar: the struck foe's ATK down, no counter
  int drainUses = 0;                   // ... its talent (Mão Ferida): the struck foe loses this many uses of its skills
  bool stagger = false;                // Gancho: the pulled foe cannot counter ...
  int staggerTurns = 0;                // ... for this many more of its own turns
  int vigilPct = 0, vigilRadius = 0;   // Vigília da Sentinela: the stance on the caster (Type::Effect)
  // The Conjurador's spells (docs/SKILLS_CONJURER.md)
  std::optional<size_t> sacrifice;     // Detonar Invocação: the summon that blows up (dead as the step resolves, before the hits)
  int fearPct = 0;                     // Pesadelo: each struck foe left standing is afraid for fearTurns turns with this chance
  int mirrorCopies = 0, mirrorRounds = 0;  // Imagem Espelhada on every unit of `ids` (Type::Effect)
  int bondPct = 0;                     // Elo de Sacrifício on the caster (Type::Effect)
  int empowerPct = 0, empowerRounds = 0;   // Fortalecer Invocações on every unit of `ids` (Type::Effect)
  int reflectTurns = 0, reflectPct = 0, reflectHealPct = 0;  // Reflexo Arcano on the caster (Type::Effect)
  // The Mago Negro's (docs/SKILLS_MAGE.md)
  int daggers = 0;                     // Nuvem de Adagas on the caster (Type::Effect): the daggers circling it
  int globeRadius = 0, globeRounds = 0, globeTier = 0;  // Globo de Invulnerabilidade round the caster (Type::Effect): its bubble (`tiles`)
  // The Cruzado's (docs/SKILLS_CRUSADER.md)
  int holyDice = 0, holyFaces = 0;     // Golpe Consagrado: a landed blow adds this much holy damage ...
  double holyUndeadMul = 1;            // ... times this on an undead
  int mantleTurns = 0, mantleDice = 0, mantleFaces = 0;  // Manto do Cruzado on every unit of `ids` (Type::Effect)
  // Selene's Lâmina Arcana (docs/SKILLS_CONJURER.md): a landed blow marks the foe (dice D faces arcane if it walks, for turns of its own)
  int arcaneMarkDice = 0, arcaneMarkFaces = 0, arcaneMarkTurns = 0;
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
  bool revived = false;  // Ressurreição (the Curandeiro's): the fallen unit stands again (the window brings its card back)
  float arc = -1.0f;     // ... on a flight (the Lanceiro's throws, pulls, drags, the swap): the arc's height, -1 = at once
  float delay = 0.0f;    // seconds (nominal) the window waits before showing it: what lands after a flight
  std::string cause;     // what hurt it at the start of its turn ("Sangramento fere", ...): the combat log's verb
  int reflectFrom = -1;  // Reflexo Arcano: the warded unit the spell came back from (the window flies it back from there)
};

// --- EmberEngine's own spells: the two promotions of the Mago Negro (user, 2026-10-16/17: "2 magias por tier do 6 ao
// 10"; Bruxo = enfraquecimento e dano no tempo, Elementalista = dano puro e área) ---
// The class a kind belongs to ("warlock" / "elementalist"), empty for Ember's own spells.
std::string emberSpellClass(const std::string& kind);
// The level a class learns it at (0 = not one of ours).
int spellLearnLevel(const std::string& kind);
// The kinds a class knows at a level, in learning order (empty for Ember's classes); a hero's exclusive only for that hero.
std::vector<std::string> emberSpells(const std::string& classId, int level, const std::string& hero = {});
// Every spell's numbers level by level, as the casts read them, from the level it is learned to 60 (Ember Ashes II's cap):
// one small change on each of the 14 levels after learning where its track has a step, none where it has none, none after
// (the user, 2026-10-20). Returns the failures, empty when every spell keeps the rule; `report` gets a line per spell.
std::vector<std::string> checkSpellTracks(std::vector<std::string>* report = nullptr);
// Appends to the unit's spell list what its class knows at its level and does not have yet (level-ups call it too).
void grantClassSpells(BattleUnit& u);
// Promotes (or demotes) a hero: its class id and Portuguese class name, and its spell list rebuilt (Ember's own spells
// kept, ours replaced). "" / "mage" = the Mago Negro again.
void setPromotion(BattleUnit& u, const std::string& classId);
// The Portuguese class name ("Bruxo", "Elementalista", "Mago Negro"; the id itself when unknown).
std::string promotionName(const std::string& classId);
// A unit's conditions in words, for the window ("Marcado +20% · Veneno Maior · Silenciado 2"), empty when none.
std::string conditionsText(const BattleUnit& u);
// The spell's description for the window's tooltip (its numbers at the caster's level).
std::string spellTooltip(const BattleUnit& caster, const std::string& kind);
// A first spell's line for the status sheet (after its name) from its numbers at the caster's level (its track: reach, area, dice ...), in
// Portuguese or English; empty for a kind it does not cover (the sheet keeps its own line then).
std::string spellScaleText(const BattleUnit& caster, const std::string& kind, bool en);
// How a first spell works, in words (who it strikes, what it leaves behind), PT or EN; empty when it has none (ours: spellTooltip).
std::string spellExplain(const std::string& kind, bool en);
// Where the spell stands on its scaling track at this level: "Escala do 1 ao 15 · próximo: nível 4, +1 míssil" (empty: no track).
std::string spellTrackNote(const std::string& kind, int level, bool en);

// The Lanceiro's Vigília da Sentinela (docs/SKILLS_LANCER.md): the thrusts that land on `attacker` before its blow, one per
// sentinel of the other side whose stance covers one of `targets` (itself, or an ally within its radius) and whose lance
// reaches the attacker from where it stands. Empty when nobody stands guard. The window plays them first; a dead attacker
// never strikes.
std::vector<BattleStep> vigilInterrupts(const Battle& b, size_t attacker, const std::vector<size_t>& targets);
// The Mago Negro's Nuvem de Adagas (docs/SKILLS_MAGE.md): the daggers thrown at `attacker` before it acts, by every unit of the other
// side beside it that has daggers left: one when it ended its walk beside that unit (`arrived`), one more when its melee action
// (`meleeTargets`: whom its blow or weapon skill strikes) is aimed at that unit and it is no archer (an arrow is intercepted instead:
// daggerIntercepts). Each step spends a dagger as it resolves; a dead attacker never strikes.
std::vector<BattleStep> daggerInterrupts(const Battle& b, size_t attacker, const std::vector<size_t>& meleeTargets, bool arrived);
// The daggers' own strike: a spell step the cloud throws (never learned).
bool daggerStrikeKind(const std::string& kind);
// Selene's Lâmina Arcana (docs/SKILLS_CONJURER.md): the mark on `walker` going off because it walked (`walked`): one step from whoever
// marked it, its dice in arcane damage, the mark spent as it resolves. Empty when it carries no mark, did not walk, or its marker fell.
std::vector<BattleStep> arcaneMarkInterrupts(const Battle& b, size_t walker, bool walked);
// The mark's burst: a spell step the mark plays (never learned).
bool arcaneMarkBurstKind(const std::string& kind);
// Globo de Invulnerabilidade: `kind` cast by `caster` from where it stands would fail on `target` (inside a globe the caster is not in, the
// spell's tier within the globe's; blows, arrows and weapon skills never).
bool globeBlocks(const Battle& b, size_t caster, const std::string& kind, size_t target);

// The combat log's language for the lines made from now on (the window's Idioma choice).
void setCombatLogEnglish(bool english);
bool combatLogEnglish();

// Spell names as the window shows them (Ember's own Portuguese names).
std::string spellName(const std::string& kind);
// ... in the chosen language (English names for the combat log in English; Portuguese otherwise).
std::string spellNameIn(const std::string& kind, bool english);
// A spell EmberEngine casts (the rest of a unit's list is shown but not castable yet).
bool spellPorted(const std::string& kind);
// A weapon skill (Ember isWeaponAbility): it rolls the weapon's own hit chance on a foe.
bool spellWeaponAbility(const std::string& kind);
// A spell cast at once, with no aim (Criar Comida e Água): cast it at the caster's own hex.
bool spellInstant(const std::string& kind);
// A passive (the Cruzado's Juramento): listed with the unit's spells, never cast, no use spent; it works by itself.
bool spellPassive(const std::string& kind);
// The highest level a class reaches (the Cruzado: 10); every other class the game's own cap (30).
int classLevelCap(const std::string& classId);
// The highest level a hero reaches: its class's, or lower for a hero of its own (Selene: 10).
int heroLevelCap(const std::string& name, const std::string& classId);
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
// Ember usePotion + confirmPotionAt: the step that drinks / hands the potion (applyPotion when it plays), or nothing with
// the reason (applyPotion's own refusals included: full HP, not sick, spells full). It spends the actor's action.
std::optional<BattleStep> usePotion(const Battle& b, size_t actor, const std::string& id, BattleHex cell, std::string* error = nullptr);
// The disease's effect (maybeInflictDisease once its roll lands): 10% off atk / mag / def / dex / mov, kept to give back.
void inflictDisease(BattleUnit& u);
// Ember reapplyGear: the stats again from its class at its level and its gear, less hunger and disease.
void reapplyGear(BattleUnit& u);
// Ember spawnUnit's stats for a hero brought into a fight hungry or sick (its own rounding, not reapplyGear's).
void spawnConditions(BattleUnit& u);
// The support casts' reference scene (export_battle.ts hurtScene): every living unit at half HP, the party's even-indexed
// units diseased, every third unit poisoned, everyone hungry (fullness 0), every tier's uses halved.
void hurtScene(Battle& b);
// The pose a cast plays (Ember startSeq: arrows and blade skills swing, cures heal, the rest cast).
std::string spellPose(const std::string& kind);
// Uses left (Ember remainingSpellUses).
int spellUsesLeft(const Battle& b, size_t unit, const std::string& kind);
// The tier a spell draws its uses from (Ember SPELL_TIER; 0 = none): the status sheet's "Tier N".
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
// Casts it (Ember castSpell): checks the class, the uses and every aim, spends the use and returns the steps to play,
// or nothing (with the reason) when it cannot be cast.
// checkUses = false skips the class and uses gate (the cast methods themselves do not check them).
std::optional<std::vector<BattleStep>> castSpell(Battle& b, size_t caster, const std::string& kind, const std::vector<BattleHex>& cells,
                                                 std::string* error = nullptr, bool checkUses = true);
// Plays one step (Ember drain: resolveSpellSource / resolveCombat with its counter / the heal), drawing from `rng`.
void resolveStep(Battle& b, const BattleStep& step, Mulberry32& rng, std::vector<BattleEffect>& out);
// One strike of a combat step: `counter` = the defender striking back (resolveCombatSource).
void resolveStrike(Battle& b, const BattleStep& step, bool counter, Mulberry32& rng, std::vector<BattleEffect>& out);
// Whether the defender strikes back after the step's strike (resolveCombat's condition).
bool strikesBack(const Battle& b, const BattleStep& step);
// A summoned unit of this class as it stands when a saved fight is resumed (its numbers are then overlaid from the snapshot).
std::optional<BattleUnit> summonSkeleton(const std::string& id, const std::string& classId);
// Ember applyTileHazard for a unit that just stepped onto a hex (a flame under its feet burns it).
void stepOnHex(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);
// Ember tryWarpCrossing: the hex a unit standing on one opening of a warp gate comes out at, beside the other, or nothing.
std::optional<BattleHex> warpCrossing(const Battle& b, size_t unit);
// Plays steps one after another (Ember's queue): a missed Investida Touro drops the rest of its charge.
void playSteps(Battle& b, const std::vector<BattleStep>& steps, Mulberry32& rng, std::vector<BattleEffect>& out);
// Cooker check-battle: a unit's whole state as numbers, the zones and the enmity entries (compared with Ember's own).
std::vector<double> unitStateNumbers(const BattleUnit& u);
nlohmann::json zoneStateNumbers(const Battle& b);
nlohmann::json enmityNumbers(const Battle& b);
// Ember commitOffHandAction ("Mão Secundária"): the blow of what the off hand holds, from where the unit stands. A dagger / katar strikes with its
// own dice (and its own sheet); a shield bashes (75% of the blow by default, a 70% chance to stun for a turn; Golpe de Escudo's stun on a hit). Empty when
// the foe is out of its reach or the off hand holds neither.
std::optional<BattleStep> offHandStep(const Battle& b, size_t unit, size_t foe);
// Ember useLockpick ("Arrombar"): the unit spends a Gazua on the lock beside it. The cells it changes open up; a chest gives Ember, a potion for whoever has room
// (the opener first, then the turn order), maybe a piece of gear and maybe rations, all of it rolled in Ember's order; a bleeding opener pays the 1D8, and
// the action is done (it builds affinity with its neighbours). False when there is nothing to open or no Gazua.
bool useLockpick(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);
// BattleEngine.gainAdjacentAffinity: an action done by a hero builds +0.1 with each hero fighting beside it (not those it already helped
// this action); the support recipients are then forgotten. Called as the action ends, before the unit counts as having acted.
void gainAdjacentAffinity(Battle& b, size_t unit);
// Ember confirmFinish: the player ends the mission. An escape waypoint rolls the hero's DEX chance (a failed try spends its turn and the fight
// goes on); anything else ends it. `escapeChance` is the chance shown for an escape waypoint, `hero` who stands on it.
struct FinishOutcome {
  bool ended = false, escapeFailed = false;
  size_t hero = 0;
};
FinishOutcome confirmFinish(Battle& b, Mulberry32& rng);
// What a unit's turn becomes once it has begun (Ember beginUnitTurn after startOfTurnEffects): it plays on, it loses the turn
// (dead, stunned, asleep) or it flees in fear along `fleePath` and cannot act.
struct TurnStart {
  enum class Kind { Normal, Lost, Fled } kind = Kind::Normal;
  std::vector<BattleHex> fleePath;
  std::string tip;
};
// Ember beginUnitTurn: its start-of-turn effects, then stun, the web's sleep roll, sleep and fear, and whether its cell is webbed
// (turnRestrained). The caller has already called Battle::beginTurn.
TurnStart beginUnitTurn(Battle& b, size_t unit, Mulberry32& rng, std::vector<BattleEffect>& out);
// Ember applyBleedingActionDamage: a bleeding unit takes 1D8 whenever it acts (once per own turn when it walks). False when it bled
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

// A hero brought to the campaign's level and XP before the fight (save.ts levels / xp): Ember's levelUpUnit once per
// level above the battle's own, so its class stats, HP and tier uses grow as they did in play.
void raiseToLevel(BattleUnit& u, int level, int xp);

}  // namespace ember
