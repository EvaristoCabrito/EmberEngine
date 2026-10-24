// Preserved Ember engine.ts spawn/cast/target methods; presentation calls removed.
import { TURN_UNDEAD, turnUndeadPower, isUndeadClass } from "../ember/data.ts";
import { FROST, frostPower, frostAreaTiles, frostCharges } from "./frost.ts";
import { dexAccuracy } from "./dexterity.ts";
import { equippedWeaponType, trainedWeaponSkills, weaponTypesForClass, weaponModifiers, isWeaponAbility } from "./weaponSkills.ts";
import { elementalDamage, spellElement, sumResistances } from "../ember/resistances.ts";
import { AFFINITY_HEROES, affinityBonus, affinityScore, changeAffinity, type AffinityHero } from "./affinity.ts";
import { CAUSTIC_VENOM, DIVINE_BOLT, MINOR_VENOM, CLASSES, CLEAVE, cleaveDoublesVs, cleavePower, CURE_DISEASE, CURES, DECORATIONS, DISEASE, doubleStrikePower, EMPTY_BAG, expToLevel, expForHit, FIREBALL, ICE_STORM, iceStormPower, iceStormAreaTiles, FANTOM_FORCE, formatSpellUseGains, LIGHTNING, LIGHTNING_T3, LONG_SHOT, longShotPower, BLOODY_SHOT, bloodyShotMul, bloodyShotBleed, provokePower, MAGIC_MISSILE, magicMissileCount, MAX_LEVEL, PIERCING, piercingMul, PIERCING_THRUST, SHOCK, SUMMON_FAMILIAR, PHANTASMAL_FORCE, phantasmalForceDice, SUMMON_FAMILIAR2, SUMMON_FAMILIAR2_UNLOCK_LEVEL, SUMMON_FAMILIAR3, SUMMON_FAMILIAR4, SUMMON_ZOMBIE_DOG, FAMILIAR_SPELL, familiarSpellCharges, familiarMagicMissileCharges, LIFE_DRAIN, lifeDrainDice, familiarLifeDrainCharges, lifeDrainHealMul, SWEEP, TRIP, WEAPONS, WEB_OF_DREAMS, diceFormula, fireballOrigin, fireballPower, fireballTiles, hexAreaTiles, isSummonClass, lightningDice, placedFootprint, rollDice, shockChargesFor, spellTier, spellUseGains, starterWeaponFor, STARTING_BAG, statsFor, tierKey, tierUses, gearStatBonus, weaponRoll, MULTI_SHOT, multiShotPower, multiShotTargets, SECOND_WIND, secondWindPct, auraPower, AURA_OF_PROTECTION, INTIMIDATING_PRESENCE, DIVINE_WRATH, divineWrathPower, SHOULDER_SMASH, shoulderSmashPower, SIGHT_RADIUS, STAMPEDE, stampedePower, cultistSpellUses, brigandSpellUses, birolhoSpellUses, webOfDreamsSize, webOfDreamsSleepChance, bullRushPower, executionerStrikePower, shieldBashPower, POISON_BREATH, poisonBreathPower, BURNING_HANDS, burningHandsPower, CREATE_FOOD_AND_WATER, createFoodAndWaterPower, BLESS, rulesClass } from "../ember/data.ts";
import type { SpellTier } from "../ember/data.ts";
import { fantomForceChargesFor, fantomForceDice } from "../ember/data.ts";
import { powerOf } from "./combat.ts";
import { effectivePoisonResistance, POISON_TIERS, poisonChance, poisonDice, poisonTickDamage, poisonTierOf, strongerPoison } from "./poison.ts";
import { rollWeaponSkillGain, rollSkillGain, skillResistances, SKILL_GAIN, healingAmount, skillValue, type HeroSkills, type SkillId } from "./skills.ts";
import { ENMITY, enmityTotal } from "./enmity.ts";
import { canHitFrom, clearShot, computeReachable, cleaveHexes, CUBE_DIRS, footprint, footprintFrontRow, hexNeighbors, hexDist, hexLine, inBounds, key, manhattan, occupies, piercingLine, canTraverseWater, reconstructPath, terrainDistanceField, tileAt, axisWalk, axisDir, coneSector, type Cube } from "./pathfinding.ts";
import { sightReaches } from "./fog.ts";
import { hexDef } from "./hexprops.ts";
import { fullness } from "../ember/hunger.ts";
import type { Bag, BattleUnitSnap, ClassId, HealId, Mission, Point, SpellKind, SpriteId, TerrainId, TierKey, Unit, EquipSlot, StatPointAllocation, StatPointAttribute, Spawn, PoisonTier } from "../ember/types.ts";
import { RuleContext } from "./context.ts";
import type { SpellImpact } from "./context.ts";
import type { CombatImpact } from "./context.ts";
const NOTORIOUS_LEVEL_BONUS: Record<string, number> = {
  "O Birolho": 8,
};
const BULL_RUSH_RANGE = 4;
function remainingTier(classId: ClassId, tier: SpellTier, key: TierKey, level: number, side: Unit["side"], roster: Roster | undefined, name: string): number {
  if (side !== "player") return 0;
  const cap = tierUses(classId, tier, level);
  const spent = roster?.spellSpent?.[name]?.[key] ?? 0;
  return Math.max(0, cap - spent);
}
const MAGE_RANGE_BONUS_CLASSES: ReadonlySet<ClassId> = new Set(["mage", "voss", "elementalist", "warlock"]);
const HERO_SPRITE_BY_NAME: Partial<Record<string, SpriteId>> = {
  Kael: "kaelFinal",
  Neera: "neera",
  Voss: "voss",
  Salazar: "salazar",
  Aldric: "aldric",
  // The old "malrec" sheet was permanently deleted (broken bleed-through art from a bad
  // sprite-sheet slice). This is a fresh slot under his own name, not a fallback to a
  // shared/generic id — seeded from a clean copy of the finished conjurer art (the only
  // good art on hand for him) so he has a real, never-shared, personally-named sprite of
  // his own to replace with dedicated art later.
  Malrec: "malrec",
};
export function heroSpriteFor(name: string, classSprite: SpriteId, useClassSprite?: boolean): SpriteId {
  if (useClassSprite) return classSprite;
  return HERO_SPRITE_BY_NAME[name] ?? classSprite;
}
export function spawnUnit(spawn: Mission["playerSpawns"][number], side: Unit["side"], i: number, roster?: Roster, enemyLevel = 1): Unit {
  const requestedClassId = (side === "player" ? roster?.promotions?.[spawn.name] : undefined) ?? spawn.classId;
  // Kael's early/final entries are visual variants, never gameplay jobs. Every unit named
  // Kael always runs the Warrior/Swordsman kit — including old cloned maps that still name
  // the early visual variant. (His sprite is pinned the same way every other hero's is now,
  // via HERO_SPRITE_BY_NAME below — this classId pin is Kael's own separate, pre-existing
  // exception where even the gameplay job never changes.)
  const isKael = spawn.name === "Kael";
  const classId = isKael ? "swordsman" : requestedClassId;
  const cls = CLASSES[classId];
  const level =
    side === "enemy"
      ? (roster?.enemyLevels?.[i] ?? enemyLevel + (NOTORIOUS_LEVEL_BONUS[spawn.name] ?? 0))
      : side === "neutral"
        ? (roster?.neutralLevels?.[i] ?? enemyLevel)
        : (roster?.levels[spawn.name] ?? 1);
  const st = statsFor(classId, level);
  // PROJECT RULE — do not remove, weaken, or special-case around this for any class,
  // existing or new. Player heroes get to choose where their level-up points go
  // (statPointAllocations); enemies never do, so raw per-level growth alone leaves every
  // enemy class falling behind an optimized build over time. Every enemy unit, of every
  // class, gets a flat +10% to hp/atk/mag/def/dex for every 5 full levels it has,
  // cumulative and stacking (level 12 is +20%, level 27 is +50%), on top of whatever
  // normal growth statsFor already gave it. This lives here — the one choke point every
  // enemy/neutral spawn passes through (see the BattleEngine constructor) — precisely so
  // adding a new enemy class can never forget it or need its own copy of this logic.
  // Never apply this multiplier to the player side.
  if (side === "enemy") {
    const boost = 1 + Math.floor(level / 5) * 0.1;
    st.hp = Math.round(st.hp * boost);
    st.atk = Math.round(st.atk * boost);
    st.mag = Math.round(st.mag * boost);
    st.def = Math.round(st.def * boost);
    st.dex = Math.round(st.dex * boost);
  }
  const statPointAllocation = side === "player" ? { ...(roster?.statPointAllocations?.[spawn.name] ?? {}) } : {};
  const point = (attribute: StatPointAttribute) => statPointAllocation[attribute] ?? 0;
  // The starvation streak determines the severity, but a ration restores an individual
  // hero immediately. A full hero must not keep the group's hunger condition or penalty.
  const heroIsStarving = fullness(roster?.heroHunger?.[spawn.name]) <= 0;
  const hungerPenaltyPct = side === "player" && heroIsStarving ? Math.min(0.9, Math.max(0, roster?.hungerPenaltyPct ?? 0)) : 0;
  const hungerKeep = 1 - hungerPenaltyPct;
  const diseased = side === "player" && roster?.heroDiseases?.[spawn.name] === true;
  const poisoned = side === "player" && !!roster?.heroPoisons?.[spawn.name];
  const diseaseKeep = diseased ? 1 - DISEASE.statPenalty : 1;
  const weapon = side === "player" ? (roster?.weapons?.[spawn.name] ?? { id: starterWeaponFor(classId), enh: 0 }) : null;
  // Range is a weapon property (D&D-weapon-style), not a class stat — falls back to the
  // class baseline only when there's no equipped weapon to read it from (e.g. enemies).
  const weaponDef = weapon?.id ? WEAPONS[weapon.id] : null;
  const minRange = weaponDef?.minRange ?? st.minRange;
  const maxRange = (weaponDef?.maxRange ?? st.maxRange) + (MAGE_RANGE_BONUS_CLASSES.has(classId) ? 1 : 0);
  // A two-handed main-hand weapon leaves no free hand for an off-hand item, regardless of
  // what's saved in equipment — enforced here too, not just at the equip screen.
  const offHandId = side === "player" && !weaponDef?.twoHanded ? (roster?.offHand?.[spawn.name] ?? null) : null;
  const gear: Partial<Record<EquipSlot, string>> = side === "player" ? { ...(roster?.equipment?.[spawn.name] ?? {}) } : {};
  // Worn gear contributes to every core combat stat, not just DEF — kept in sync with
  // reapplyGear below, which redoes this same math after a slot changes mid-battle.
  const gearBonus = gearStatBonus(Object.values(gear));
  const hpCap = roster?.hp[spawn.name];
  const maxHp = Math.round((st.hp + point("hp") + gearBonus.hp) * hungerKeep);
  const hp = hpCap != null && hpCap > 0 ? Math.min(maxHp, hpCap) : maxHp;
  return {
    id: `${side}-${spawn.name}-${i}`,
    name: spawn.name,
    classId: cls.id,
    className: cls.name,
    role: cls.role,
    side,
    sprite: heroSpriteFor(spawn.name, cls.sprite, spawn.useClassSprite),
    useClassSprite: spawn.useClassSprite,
    x: spawn.x,
    y: spawn.y,
    hp,
    maxHp,
    atk: Math.round((st.atk + point("atk") + gearBonus.atk) * hungerKeep * diseaseKeep),
    mag: Math.round((st.mag + point("mag") + gearBonus.mag) * hungerKeep * diseaseKeep),
    def: Math.round((st.def + point("def") + gearBonus.def) * hungerKeep * diseaseKeep),
    dex: Math.round((st.dex + point("dex") + gearBonus.dex) * hungerKeep * diseaseKeep),
    resistances: sumResistances(st.resistances, gearBonus.resistances, side === "player" ? skillResistances(roster?.heroSkills, spawn.name) : undefined),
    weaponSkills: side === "player" ? trainedWeaponSkills(roster?.heroSkills, spawn.name, cls.id) : undefined, healingSkill: skillValue(roster?.heroSkills, spawn.name, "healing"),
    initiative: initiativeBonus(cls.id),
    initiativeRoll: 0,
    statPointAllocation,
    mov: spawn.holdsPosition ? 0 : diseased ? Math.max(1, Math.round((st.mov + gearBonus.mov) * diseaseKeep)) : st.mov + gearBonus.mov,
    gear,
    minRange,
    maxRange,
    moved: false,
    acted: false,
    facing: side === "player" ? 1 : -1,
    faceDx: side === "player" ? 1 : -1,
    faceDy: 0,
    walkPose: "front",
    idleAlt: false,
    alive: true,
    drawX: spawn.x,
    drawY: spawn.y,
    flash: 0,
    levelGlow: 0,
    healGlow: 0,
    healGlowKind: "potionZero",
    fade: 1,
    bob: 0,
    level,
    xp: side === "player" ? (roster?.xp?.[spawn.name] ?? 0) : 0,
    bag: side === "player" ? { ...(roster?.bags?.[spawn.name] ?? (cls.id === "healer" ? EMPTY_BAG : STARTING_BAG)) } : { ...EMPTY_BAG },
    spells: {
      // Cultist and Birolho are enemy-only, so this never competes with a player roster's own
      // tier1/tier2/tier4 uses — see cultistSpellUses/brigandSpellUses/birolhoSpellUses and
      // runAiFor's cultist/brigand/birolho branches.
      // Carnivorous Plant: tier1 = 2 Poison Breath, tier3 = 2 Veneno Cáustico (kept in its own
      // slot because Caustic and Minor Venom normally share tier4), tier4 = 3 Veneno Menor.
      tier1:
        cls.id === "carnivorousPlant"
          ? 2
          // Sapling: 1 Poison Breath.
          : cls.id === "sapling"
          ? 1
          : cls.id === "swampBlueCalf"
          ? tierUses(cls.id, 1, level)
          : cls.id === "bigBlueCalf"
          ? 3
          : cls.id === "roccoTheBird"
          ? 2
          : cls.id === "emberedWraith"
            ? 0
          : cls.id === "cultist" || cls.id === "cultistV2"
          ? cultistSpellUses(level).magicMissile
          // Base Crossbowman: 2 Long Shot; 1 Piercing Shot below (the brigand AI branch spends them).
          : cls.id === "baseCrossbowman"
          ? 2
          : cls.id === "brigand"
            ? brigandSpellUses(level).longShot
            : cls.id === "birolho" || cls.id === "birolho2" || cls.id === "birolho3" || cls.id === "birolhoLegs" || cls.id === "birolhoLegs2"
              ? birolhoSpellUses(level).magicMissile
              : remainingTier(cls.id, 1, "tier1", level, side, roster, spawn.name),
      tier2:
        // Rocco The Bird: tier2 holds his 3 Burning Beak (Burning Hands) casts.
        cls.id === "roccoTheBird"
          ? 3
          : cls.id === "emberedWraith"
            ? 1
          // StagWolf: 2 Relâmpago (its 1 Choque comes from shockChargesFor).
          : cls.id === "stagWolf"
          ? 2
          : cls.id === "cultist" || cls.id === "cultistV2"
          ? cultistSpellUses(level).lightning
          : cls.id === "baseCrossbowman"
          ? 1
          // Brigand Crossbowman: 2 Bloody Shot here, 1 Multi Shot in tier3.
          : cls.id === "brigandCrossbowman"
          ? 2
          : cls.id === "brigand"
            ? brigandSpellUses(level).piercing
            : cls.id === "birolho" || cls.id === "birolho2" || cls.id === "birolho3" || cls.id === "birolhoLegs" || cls.id === "birolhoLegs2"
              ? birolhoSpellUses(level).lightning
              : remainingTier(cls.id, 2, "tier2", level, side, roster, spawn.name),
      tier3: cls.id === "carnivorousPlant" ? 2 : cls.id === "brigandCrossbowman" ? 1 : remainingTier(cls.id, 3, "tier3", level, side, roster, spawn.name),
      tier4:
        cls.id === "carnivorousPlant"
          ? 3
          : cls.id === "birolho" || cls.id === "birolho2" || cls.id === "birolho3" || cls.id === "birolhoLegs" || cls.id === "birolhoLegs2"
          ? birolhoSpellUses(level).causticVenom
          // Undead Ox: 2 Veneno Cáustico per battle (it joins runAiFor's birolho branch).
          : cls.id === "undeadOx" || cls.id === "plagueBearingCattle"
            ? 2
          : remainingTier(cls.id, 4, "tier4", level, side, roster, spawn.name),
      tier5: remainingTier(cls.id, 5, "tier5", level, side, roster, spawn.name),
      tier6: remainingTier(cls.id, 6, "tier6", level, side, roster, spawn.name),
      tier7: remainingTier(cls.id, 7, "tier7", level, side, roster, spawn.name),
      tier8: remainingTier(cls.id, 8, "tier8", level, side, roster, spawn.name),
      tier9: remainingTier(cls.id, 9, "tier9", level, side, roster, spawn.name),
      tier10: remainingTier(cls.id, 10, "tier10", level, side, roster, spawn.name),
    },
    weaponId: weapon?.id ?? null,
    weaponEnh: weapon?.enh ?? 0,
    size: cls.size,
    footprintW: cls.footprintW,
    footprintH: cls.footprintH,
    footprintOffsets: cls.footprintOffsets,
    shock: null,
    shockCharges: side === "enemy" ? shockChargesFor(cls.id) : 0,
    frostCharges: side === "enemy" && cls.id === "cultistV2" ? frostCharges(level) : 0,
    fantomForceCharges: side === "enemy" ? fantomForceChargesFor(cls.id) : 0,
    diseased,
    diseaseBase: diseased
      ? {
          atk: Math.round((st.atk + point("atk") + gearBonus.atk) * hungerKeep),
          mag: Math.round((st.mag + point("mag") + gearBonus.mag) * hungerKeep),
          def: Math.round((st.def + point("def") + gearBonus.def) * hungerKeep),
          dex: Math.round((st.dex + point("dex") + gearBonus.dex) * hungerKeep),
          mov: st.mov + gearBonus.mov,
        }
      : null,
    poisoned,
    poisonTier: poisoned ? poisonTierOf(roster?.heroPoisons?.[spawn.name]) ?? "lesser" : undefined,
    poisonMag: poisoned ? roster?.heroPoisonMag?.[spawn.name] ?? 0 : undefined,
    poisonResist: side === "player" ? skillResistances(roster?.heroSkills, spawn.name).poison ?? 0 : 0,
    bleeding: false,
    bleedRoundsLeft: undefined,
    bleedRoundMarker: undefined,
    bleedMovedThisTurn: false,
    stunned: false,
    stunTurns: 0,
    crippled: false,
    hungerPenaltyPct,
    fullness: fullness(roster?.heroHunger?.[spawn.name]),
    offHandId,
    // Summon-ness is a property of the class, not of how the unit got here: one placed
    // straight onto a map in the editor is outside the party's defeat check just like one
    // the Conjurer calls up mid-battle.
    summoned: isSummonClass(cls.id),
    asleep: false,
    sleepTurns: 0,
    guaranteedDrop: side === "enemy" && !!spawn.guaranteedDrop,
    dialog: spawn.dialog ?? null,
    moveBudgetUsed: 0,
  };
}
export function unitFromSnap(snap: BattleUnitSnap): Unit {
  // Old battle snapshots may contain the visual-only Kael class ids. Normalize the combat
  // class on load as well, so resuming a save never strips his Warrior skills.
  const classId = snap.name === "Kael" ? "swordsman" : snap.classId;
  const cls = CLASSES[classId];
  return {
    id: snap.id,
    name: snap.name,
    classId,
    className: cls?.name ?? classId,
    role: cls?.role ?? "",
    side: snap.side,
    sprite: heroSpriteFor(snap.name, cls?.sprite ?? "soldier", snap.useClassSprite),
    useClassSprite: snap.useClassSprite,
    x: snap.x,
    y: snap.y,
    hp: snap.hp,
    escaped: snap.escaped,
    maxHp: snap.maxHp,
    atk: snap.atk,
    mag: snap.mag,
    def: snap.def,
    dex: snap.dex,
    weaponSkills: snap.weaponSkills,
    resistances: snap.resistances ?? sumResistances(cls?.resistances, gearStatBonus(Object.values(snap.gear ?? {})).resistances, { poison: snap.poisonResist ?? 0 }),
    initiative: snap.initiative ?? initiativeBonus(classId),
    initiativeRoll: snap.initiativeRoll ?? 0,
    statPointAllocation: { ...(snap.statPointAllocation ?? {}) },
    mov: snap.mov,
    minRange: snap.minRange,
    maxRange: snap.maxRange,
    moved: snap.moved,
    acted: snap.acted,
    facing: snap.facing,
    faceDx: snap.faceDx ?? snap.facing,
    faceDy: snap.faceDy ?? 0,
    walkPose: "front",
    idleAlt: false,
    alive: snap.alive,
    drawX: snap.x,
    drawY: snap.y,
    flash: 0,
    levelGlow: 0,
    healGlow: 0,
    healGlowKind: "potionZero",
    fade: snap.fade,
    bob: 0,
    level: snap.level,
    xp: snap.xp,
    bag: { ...snap.bag },
    spells: { ...snap.spells },
    weaponId: snap.weaponId,
    weaponEnh: snap.weaponEnh,
    size: cls?.size ?? 1,
    footprintW: cls?.footprintW,
    footprintH: cls?.footprintH,
    footprintOffsets: cls?.footprintOffsets,
    shock: snap.shock ? { ...snap.shock } : null,
    shockCharges: snap.shockCharges ?? 0,
    frostCharges: snap.frostCharges ?? (snap.side === "enemy" && classId === "cultistV2" ? frostCharges(snap.level) : 0),
    fantomForceCharges: snap.fantomForceCharges ?? (snap.side === "enemy" ? fantomForceChargesFor(classId) : 0),
    blessedHitBonusPct: snap.blessedHitBonusPct ?? 0,
    blessedRoundsLeft: snap.blessedRoundsLeft ?? 0,
    diseased: snap.diseased,
    diseaseBase: snap.diseaseBase ? { ...snap.diseaseBase } : null,
    poisoned: snap.poisoned,
    poisonTier: snap.poisonTier ?? poisonTierOf(snap.poisonFaces),
    poisonMag: snap.poisonMag ?? 0,
    poisonResist: snap.poisonResist ?? 0,
    bleeding: snap.bleeding ?? false,
    bleedRoundsLeft: snap.bleedRoundsLeft,
    bleedRoundMarker: snap.bleedRoundMarker,
    bleedMovedThisTurn: false,
    fearTurns: snap.fearTurns ?? 0,
    fearSourceId: snap.fearSourceId,
    stunned: snap.stunned,
    stunTurns: snap.stunTurns,
    crippled: snap.crippled,
    hungerPenaltyPct: snap.hungerPenaltyPct ?? 0,
    fullness: fullness(snap.fullness),
    offHandId: snap.offHandId,
    gear: { ...snap.gear },
    summoned: snap.summoned,
    asleep: snap.asleep,
    sleepTurns: snap.sleepTurns,
    guaranteedDrop: snap.guaranteedDrop,
    dialog: snap.dialog,
    moveBudgetUsed: snap.moveBudgetUsed,
  };
}
export function takesTurns(u: Unit): boolean {
  return u.alive && u.side !== "neutral";
}
export function initiativeBonus(classId: ClassId): number {
  if (classId === "assassin") return 12;
  if (classId === "rogue") return 11;
  if (classId === "archer" || classId === "neera" || classId === "ranger") return 10;
  return 11 - (CLASSES[classId].init ?? 10);
}
function attackableByPlayer(u: Unit): boolean {
  if (u.dialog) return false;
  return u.alive && (u.side === "enemy" || u.side === "neutral");
}
export interface Roster {
  affinityScores?: Record<string, number>;
  /** Party leader (Party menu); their affinity gains catch up faster — see adjustHeroAffinity. */
  partyLeader?: string;
  hp: Record<string, number>;
  levels: Record<string, number>;
  xp?: Record<string, number>;
  /** Hero name → permanent player-selected level-up bonuses. */
  statPointAllocations?: Record<string, StatPointAllocation>;
  bags?: Record<string, Bag>;
  /** Hero name → promoted ClassId chosen at PROMOTE_LEVEL, overriding the mission spawn's base class. */
  promotions?: Record<string, ClassId>;
  /** Hero name → equipped weapon + enhancement, resolved from save.equipped/save.weapons. Falls
   * back to that class's free starter weapon when a hero has no entry yet. */
  weapons?: Record<string, { id: string; enh: number }>;
  /** Hero name → equipped offHand EquipmentDef id (shield or off-hand weapon), resolved
   * from save.equipment[hero].offHand. */
  offHand?: Record<string, string>;
  /** Hero name → every slot they have filled, straight from save.equipment. The off-hand
   * above is the one slot combat already read; this brings the rest in so worn gear can
   * contribute stats (see gearStatBonus). */
  equipment?: Record<string, Partial<Record<EquipSlot, string>>>;
  /** Index into mission.enemySpawns → level override, for Map Editor balance-testing.
   * Falls back to the mission's uniform enemyLevelFor(index) when an index has no entry.
   * Keyed by index rather than name because enemy spawns routinely share a name (several
   * "Piqueiro" on the same map) — a name-keyed map collapsed every same-named spawn's
   * override onto one shared entry, which is why a level typed into the editor for one of
   * several duplicates silently didn't take. */
  enemyLevels?: Record<number, number>;
  /** Same idea as enemyLevels, but indexing mission.neutralSpawns instead. */
  neutralLevels?: Record<number, number>;
  /** Defeated spawn ids in a previously visited crossing dungeon. The original spawn
   * indexes are retained while filtering so ids stay stable across later incursions. */
  crossingDefeatedSpawns?: string[];
  /** Inn-quest pickups lying on this map right now (see quests.ts activePickupsFor). */
  questPickups?: { key: string; name: string; x: number; y: number }[];
  /** Every weapon id already in the player's save — chest and kill-drop loot rolls exclude
   * these so a drop never announces a weapon the player already has. */
  ownedWeaponIds?: string[];
  /** Hero name → tier key → spell uses already spent so far this scenario (a world-map
   * location's whole run of missions) — carried in from the previous mission(s) so spell
   * charges don't refill until the scenario ends, per direct instruction. Undefined/omitted
   * means "reset to full," used for a scenario's first mission and for Stone Bridge, which
   * always resets (it's the tutorial). See GameApp.tsx's startBattle for who computes this. */
  spellSpent?: Record<string, Partial<Record<TierKey, number>>>;
  /** 0..0.9 — the party's current overworld hunger penalty (see hungerPenaltyFor in
   * overworld.ts), applied uniformly to every player spawn. Party-wide, not per-hero,
   * since hungerStreak itself is party-wide. At the 0.9 cap, a hero whose OWN fullness has
   * also bottomed out is benched from the fight entirely instead of just docked (see
   * heroUnconscious) — heroHunger is per-hero, so one fed today still fights normally even
   * while the rest of the party is still starving. */
  hungerPenaltyPct?: number;
  heroHunger?: Record<string, number>;
  /** Persistent illnesses contracted while travelling. */
  heroDiseases?: Record<string, boolean>;
  /** Poison that remained after the last battle. */
  heroPoisons?: Record<string, PoisonTier>;
  /** MAG of whoever applied that leftover poison. */
  heroPoisonMag?: Record<string, number>;
  /** Trained hero skills (skills.ts) — Resistência a Veneno. */
  heroSkills?: HeroSkills;
}
export type Seq =
  | { type: "move"; id: string; path: Point[] }
  | {
      type: "combat";
      att: string;
      def: string;
      /** A single extra die (bonusDice = faces, bonusFlat = flat add) rolled on top of the
       * attacker's own strike. bonusDiceCount is how many of that die to roll — defaults to
       * 1 (Trip's single d8), Double Strike's higher tiers roll 2. */
      bonusDice?: number;
      bonusDiceCount?: number;
      bonusFlat?: number;
      noCounter?: boolean;
      spellKind?: SpellKind;
      /** Off-hand weapon attack: roll these dice instead of the attacker's main-hand
       * weapon. Only applied on the attacker's own strike, never on a counter. */
      customDice?: { dice: number; faces: number; bonus: number };
      /** Shield Bash: multiplies the attacker's own strike damage (e.g. 0.75). */
      dmgMul?: number;
      /** Shield Bash: chance (0-1) the strike, if it lands, stuns the defender for their
       * next turn. */
      stunChance?: number;
      /** Bull Rush: extra dice folded into the same hit/defence resolution as bonusDice,
       * rolled only when the precomputed knockback (see knockTo) was blocked short of its
       * full distance — never a second hit, never a second counter. */
      wallImpact?: { dice: number; faces: number } | null;
      /** Bull Rush: the target's knockback destination, precomputed at cast time (see
       * castBullRush) since the board can't change mid-action — stepCombat just applies it
       * once the hit lands and the target survives. */
      knockTo?: Point | null;
    }
  | { type: "spell"; att: string; tiles: Point[]; ids: string[]; dice?: number; faces?: number; bonus?: number; moreDice?: number; moreFaces?: number; label?: string; echo?: { dice: number; faces: number; bonus: number }; dmgMul?: number; weaponBonusDice?: number; weaponBonusFaces?: number; weaponBonusBonus?: number; spellKind?: SpellKind; projectileTo?: Point; centerId?: string; centerDice?: number; centerFaces?: number; centerBonus?: number; poison?: boolean; spellMul?: number; centerMul?: number }
  | { type: "heal"; att: string; def: string; kind: HealId }
  | { type: "cureDisease"; att: string; def: string }
  | { type: "banner"; text: string; dur: number }
  | { type: "delay"; dur: number }
  | { type: "checkEnd" };
export abstract class SpellRules extends RuleContext {
protected castFrost(u: Unit, cell: Point): void { if (this.tierRemaining(u, "frost") <= 0 || !this.frostTiles(u, cell).length)
    return; this.queueFrost(u, u, cell); this.spellKind = null; this.mode = "locked"; this.tip = null; }

protected castTurnUndead(unit: Unit): void {
    if (unit.acted || rulesClass(unit.classId) !== "healer" || this.tierRemaining(unit, "turnUndead") <= 0)
        return;
    const p = turnUndeadPower(unit.level);
    const tiles = this.turnUndeadTiles(unit, unit.level);
    const ids = this.units.filter(target => target.alive && !target.dialog && isUndeadClass(target.classId)
        && tiles.some(cell => occupies(target, cell.x, cell.y))).map(target => target.id);
    this.spendTier(unit, "turnUndead");
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: unit.id, tiles, ids, dice: p.dice, faces: p.faces, bonus: 0,
        spellMul: p.mul, label: TURN_UNDEAD.name, spellKind: "turnUndead" });
}

protected castBullRush(unit: Unit, cell: Point): void {
    if (!unit.alive || unit.acted || this.tierRemaining(unit, "bullRush") <= 0)
        return;
    const p = bullRushPower(unit.level);
    const charge = this.bullRushCharge(unit, cell);
    if (!charge) {
        this.tip = `Toque num inimigo a até ${BULL_RUSH_RANGE} hexes, sem aliado ou obstáculo no caminho.`;
        { }
        return;
    }
    this.spendTier(unit, "bullRush");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    let from = { x: unit.x, y: unit.y };
    for (const leg of charge.legs) {
        if (leg.path.length > 0) {
            // The real "move" step carries the charge's motion (a fast dash, see MoveAnim.charge).
            const dash: Seq = { type: "move", id: unit.id, path: [from, ...leg.path] };
            this.chargeMoves.add(dash);
            this.queue.push(dash);
            from = leg.path[leg.path.length - 1]!;
        }
        this.queue.push({
            type: "combat",
            att: unit.id,
            def: leg.foe.id,
            noCounter: true,
            bonusDice: p.faces,
            bonusDiceCount: p.dice,
            bonusFlat: 0,
            wallImpact: leg.push.blocked ? { dice: p.wallDice, faces: p.wallFaces } : null,
            knockTo: leg.push.path.length > 0 ? leg.push.path[leg.push.path.length - 1]! : null,
            spellKind: "bullRush",
        });
    }
}

/** Provoke: no damage — every enemy in the area gets Provoke's huge volatile enmity on the
 * warrior (enmity.ts), so it turns on him until someone out-generates it. */
protected castProvoke(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const area = new Set(this.provokeArea(cell, unit.level).map((p) => key(p.x, p.y)));
    const foes = this.units.filter((f) => f.alive && f.side === "enemy" && this.targetable(f) && footprint(f).some((c) => area.has(key(c.x, c.y))));
    this.spendTier(unit, "provoke");
    for (const foe of foes) {
        this.addEnmity(foe, unit, ENMITY.provoke.ce, ENMITY.provoke.ve);
        this.provokeFx.push({ unitId: foe.id, t: 0 });
        { }
    }
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.missileTargets = [];
    this.tip = null;
    { }
    { }
    this.finishAction(unit);
}

protected castExecutionerStrike(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendTier(unit, "executionerStrike");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = executionerStrikePower(unit.level);
    this.queue.push({ type: "combat", att: unit.id, def: foe.id, bonusDice: power.faces, bonusDiceCount: power.dice, bonusFlat: 0, spellKind: "executionerStrike" });
}

protected castShieldBash(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendTier(unit, "shieldBash");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = shieldBashPower(unit.level);
    this.queue.push({ type: "combat", att: unit.id, def: foe.id, bonusDice: power.faces, bonusDiceCount: power.dice, bonusFlat: 0, spellKind: "shieldBash" });
}

protected castHeal(unit: Unit, cell: Point, kind: HealId): void {
    if (!this.validHealTarget(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const target = occ.get(key(cell.x, cell.y));
    if (!target)
        return;
    this.spendTier(unit, kind);
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "heal", att: unit.id, def: target.id, kind });
}

protected castCureDisease(unit: Unit, cell: Point): void {
    if (!this.validCureDiseaseTarget(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const target = occ.get(key(cell.x, cell.y));
    if (!target)
        return;
    this.spendTier(unit, "cureDisease");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "cureDisease", att: unit.id, def: target.id });
}

protected castLongShot(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) {
        this.tip = "Não há unidade na casa selecionada.";
        { }
        return;
    }
    this.spendTier(unit, "longShot");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = longShotPower(unit.level);
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles: [cell],
        ids: [foe.id],
        label: LONG_SHOT.name,
        weaponBonusDice: power.dice,
        weaponBonusFaces: power.faces,
        weaponBonusBonus: 0,
        spellKind: "longShot",
    });
}

protected castBloodyShot(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const foe = this.occ().get(key(cell.x, cell.y));
    if (!foe) {
        this.tip = "Não há unidade na casa selecionada.";
        { }
        return;
    }
    this.spendTier(unit, "bloodyShot");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles: [cell],
        ids: [foe.id],
        label: BLOODY_SHOT.name,
        dmgMul: bloodyShotMul(unit.level),
        spellKind: "bloodyShot",
    });
}

protected castPiercing(unit: Unit, cell: Point): void {
    const line = this.piercingRay(unit, cell);
    if (!line) {
        this.tip = "Escolha uma reta da colmeia.";
        { }
        return;
    }
    const ids: string[] = [];
    for (const t of line) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== unit.id && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(unit, "piercing");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: unit.id, tiles: line, ids, label: PIERCING.name, dmgMul: piercingMul(unit.level), spellKind: "piercing" });
}

protected castPiercingThrust(unit: Unit, cell: Point): void {
    const line = this.piercingThrustRay(unit, cell);
    if (!line) {
        this.tip = "Escolha uma reta na frente.";
        { }
        return;
    }
    const ids: string[] = [];
    for (const t of line) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== unit.id && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(unit, "piercingThrust");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    if (unit.name !== "Kael") { }
    this.queue.push({ type: "spell", att: unit.id, tiles: line, ids, label: PIERCING_THRUST.name, spellKind: "piercingThrust" });
}

protected castShock(unit: Unit, cell: Point): void {
    if (unit.acted || this.familiarSpellRemaining(unit, "shock") <= 0)
        return;
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const foe = this.occ().get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendFamiliarOrTier(unit, "shock");
    this.spellKind = null;
    this.spellAim = null;
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: unit.id, tiles: [cell], ids: [foe.id], dice: SHOCK.dice, faces: SHOCK.faces, bonus: SHOCK.bonus, label: SHOCK.name, echo: { dice: SHOCK.echoDice, faces: SHOCK.echoFaces, bonus: SHOCK.echoBonus }, spellMul: SHOCK.mul, spellKind: "shock" });
}

protected castLightning(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendTier(unit, "lightning");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles: [cell],
        ids: [foe.id],
        dice: lightningDice(),
        faces: LIGHTNING.faces,
        bonus: LIGHTNING.bonus,
        label: LIGHTNING.name,
        echo: { dice: LIGHTNING.echoDice, faces: LIGHTNING.echoFaces, bonus: LIGHTNING.echoBonus },
        spellMul: LIGHTNING.mul,
        spellKind: "lightning",
    });
}

protected castLightningTier3(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendTier(unit, "lightningTier3");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles: [cell],
        ids: [foe.id],
        dice: LIGHTNING_T3.dice,
        faces: LIGHTNING_T3.faces,
        bonus: LIGHTNING_T3.bonus,
        label: LIGHTNING_T3.name,
        echo: { dice: LIGHTNING_T3.echoDice, faces: LIGHTNING_T3.echoFaces, bonus: LIGHTNING_T3.echoBonus },
        spellMul: LIGHTNING_T3.mul,
        spellKind: "lightningTier3",
    });
}

/** Each missile is aimed separately, so the cast collects one target per tap and only
 * fires once they are all chosen. They may be stacked on one enemy or spread around. */
protected castMagicMissile(unit: Unit, cell: Point): void {
    const targetError = this.magicMissileTargetError(unit, cell);
    if (targetError || !this.spellAimValid(unit, cell)) {
        this.tip = targetError ?? this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) {
        this.tip = "Não há inimigo visível nessa casa.";
        { }
        return;
    }
    const want = magicMissileCount(unit.level);
    this.missileTargets.push({ id: foe.id, cell: { x: cell.x, y: cell.y } });
    if (this.missileTargets.length < want) {
        const left = want - this.missileTargets.length;
        this.tip = `${MAGIC_MISSILE.name} · escolha mais ${left} alvo${left > 1 ? "s" : ""} (pode repetir).`;
        { }
        return;
    }
    const shots = this.missileTargets;
    this.missileTargets = [];
    this.spendFamiliarOrTier(unit, "magicMissile");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    // One queued cast per missile: each rolls its own 3d4 before Arcane Resistance
    // separately, which is what makes splitting them different from one big hit.
    for (const shot of shots) {
        this.queue.push({
            type: "spell",
            att: unit.id,
            tiles: [shot.cell],
            ids: [shot.id],
            dice: MAGIC_MISSILE.dice,
            faces: MAGIC_MISSILE.faces,
            bonus: MAGIC_MISSILE.bonus,
            label: MAGIC_MISSILE.name,
            spellMul: MAGIC_MISSILE.mul,
            spellKind: "magicMissileV2",
        });
    }
}

/** Archer tier 3: the same click-N-targets flow as Magic Missile (this.missileTargets),
 * but each shot is a plain weapon hit plus a bonus die (weaponBonusDice) instead of a
 * MAG-scaled spellDamage roll — Multi Shot is a volley of arrows, not a spell. */
protected castMultiShot(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    const want = multiShotTargets(unit.level);
    this.missileTargets.push({ id: foe.id, cell: { x: cell.x, y: cell.y } });
    if (this.missileTargets.length < want) {
        const left = want - this.missileTargets.length;
        this.tip = `${MULTI_SHOT.name} · escolha mais ${left} alvo${left > 1 ? "s" : ""} (pode repetir).`;
        { }
        return;
    }
    const shots = this.missileTargets;
    this.missileTargets = [];
    this.spendTier(unit, "multiShot");
    this.spellKind = null;
    this.tip = null;
    this.mode = "locked";
    const power = multiShotPower(unit.level);
    for (const shot of shots) {
        this.queue.push({
            type: "spell",
            att: unit.id,
            tiles: [shot.cell],
            ids: [shot.id],
            label: MULTI_SHOT.name,
            weaponBonusDice: power.dice,
            weaponBonusFaces: power.faces,
            weaponBonusBonus: 0,
            spellKind: "multiShot",
        });
    }
}

protected castDoubleStrike(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendTier(unit, "doubleStrike");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    // Rolled fresh for each hit (see doubleStrikePower) — it does not stack across the two
    // strikes, each just gets its own independent roll of the current tier.
    const power = doubleStrikePower(unit.level);
    const bonus = power.dice > 0 ? { bonusDice: power.faces, bonusDiceCount: power.dice, bonusFlat: 0 } : {};
    this.queue.push({ type: "combat", att: unit.id, def: foe.id, noCounter: true, spellKind: "doubleStrike", ...bonus });
    this.queue.push({ type: "combat", att: unit.id, def: foe.id, spellKind: "doubleStrike", ...bonus });
}

protected castTrip(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendTier(unit, "trip");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "combat",
        att: unit.id,
        def: foe.id,
        noCounter: true,
        bonusDice: TRIP.bonusFaces,
        bonusFlat: TRIP.bonusBonus,
        spellKind: "trip",
    });
}

/** Familiar Maior's Dreno de Vida — routed through the same "spell" queue/stepSpell
 * machinery as every other MAG-based cast (spellMul: 1, since its only power scaling is
 * the level-scaled die from lifeDrainDice, not a flat multiplier like Fireball/Lightning's
 * own). The heal-on-hit itself lands inside stepSpell's own lifeDrain branch, once the
 * damage is known. */
protected castLifeDrain(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    unit.lifeDrainCharges = Math.max(0, (unit.lifeDrainCharges ?? 1) - 1);
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const dice = lifeDrainDice(unit.level);
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles: [cell],
        ids: [foe.id],
        dice: dice.dice,
        faces: dice.faces,
        bonus: 0,
        label: LIFE_DRAIN.name,
        spellMul: 1,
        spellKind: "lifeDrain",
    });
}

/** Conjurer tier 1's second spell — a long-range single hit, same MAG/spellMul:1/level-
 * scaled-die shape as castLifeDrain above, just ranged (magicMissile's own FX/hit-timing;
 * no dedicated art of its own yet) instead of melee. */
protected castPhantasmalForce(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe)
        return;
    this.spendTier(unit, "phantasmalForce");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const dice = phantasmalForceDice(unit.level);
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles: [cell],
        ids: [foe.id],
        dice: dice.dice,
        faces: dice.faces,
        bonus: 0,
        label: PHANTASMAL_FORCE.name,
        spellMul: 1,
        spellKind: "phantasmalForce",
    });
}

/** Summon Familiar (Conjurer tier 1): spawns a new player-side unit directly into
 * `this.units` — no queued animation step, it just appears. It has no slot in this round's
 * `turnOrder` (that's rebuilt from `this.units` fresh every round in startNewRound), so it
 * waits for the round after this one to act, same as any other reinforcement would. */
/** Summon Familiar / Summon Familiar 2 / Summon Familiar 3 (Conjurer tiers 1-3): `tier`
 * selects which of the three — same spawn logic, just a stronger creature/class/tier and
 * its own spell/slot per tier, never an automatic upgrade of the one before it. See
 * SUMMON_FAMILIAR2/SUMMON_FAMILIAR3's notes. */
protected castSummonFamiliar(unit: Unit, cell: Point, tier: 1 | 2 | 3 | 4 | 5): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    // `tier` picks the creature (4 = Familiar Radiante, cast from spell tier 3), not the spell tier.
    const cls = tier === 5 ? CLASSES.zombieDog! : tier === 4 ? CLASSES.familiar4! : tier === 3 ? CLASSES.familiar3! : tier === 2 ? CLASSES.familiar2! : CLASSES.familiar!;
    // Re-checked here, not just in startSummonFamiliarX above — the authoritative gate, so
    // the one-per-tier cap and tier 2's own level gate hold even if the awaitSpell state was
    // ever entered some other way.
    if (tier === 2 && unit.level < SUMMON_FAMILIAR2_UNLOCK_LEVEL) {
        this.spellKind = null;
        this.tip = `${SUMMON_FAMILIAR2.name} disponível a partir do nível ${SUMMON_FAMILIAR2_UNLOCK_LEVEL}.`;
        { }
        return;
    }
    if (this.hasFamiliarOut(unit, cls.id)) {
        this.spellKind = null;
        this.tip = `${unit.name} já tem ${cls.name} invocado.`;
        { }
        return;
    }
    const scale = tier === 5 ? SUMMON_ZOMBIE_DOG.statScale : tier === 4 ? SUMMON_FAMILIAR4.statScale : tier === 3 ? SUMMON_FAMILIAR3.statScale : tier === 2 ? SUMMON_FAMILIAR2.statScale : SUMMON_FAMILIAR.statScale;
    const spellKind: SpellKind = tier === 5 ? "summonZombieDog" : tier === 4 ? "summonFamiliar4" : tier === 3 ? "summonFamiliar3" : tier === 2 ? "summonFamiliar2" : "summonFamiliar";
    const spellName = tier === 5 ? SUMMON_ZOMBIE_DOG.name : tier === 4 ? SUMMON_FAMILIAR4.name : tier === 3 ? SUMMON_FAMILIAR3.name : tier === 2 ? SUMMON_FAMILIAR2.name : SUMMON_FAMILIAR.name;
    const namePrefix = tier === 5 ? "Cão Zumbi de" : tier === 4 ? "Familiar Radiante de" : tier === 3 ? "Familiar Titã de" : tier === 2 ? "Familiar Maior de" : "Familiar de";
    const maxHp = Math.max(1, Math.round(unit.maxHp * scale));
    const familiarInitiativeRoll = 1 + Math.floor(this.rng() * 20);
    const familiar: Unit = {
        id: `player-familiar-${this.units.length}`,
        name: `${namePrefix} ${unit.name}`,
        classId: cls.id,
        className: cls.name,
        role: cls.role,
        side: "player",
        sprite: cls.sprite,
        x: cell.x,
        y: cell.y,
        hp: maxHp,
        maxHp,
        atk: Math.round(unit.atk * scale),
        mag: Math.round(unit.mag * scale),
        def: Math.round(unit.def * scale),
        dex: Math.round(unit.dex * scale),
        resistances: { ...cls.resistances },
        initiativeRoll: familiarInitiativeRoll,
        initiative: familiarInitiativeRoll + initiativeBonus(cls.id),
        statPointAllocation: {},
        mov: cls.mov,
        minRange: cls.minRange,
        maxRange: cls.maxRange,
        moved: false,
        acted: false,
        facing: 1,
        faceDx: 1,
        faceDy: 0,
        walkPose: "front",
        idleAlt: false,
        alive: true,
        drawX: cell.x,
        drawY: cell.y,
        flash: 0,
        levelGlow: 0,
        healGlow: 0,
        healGlowKind: "potionZero",
        // Starts invisible and eases in while the conjuring circle plays (see emitPortalFx and
        // the fade-in tick branch), so it visibly steps out of the portal instead of appearing
        // fully solid the instant it's added to this.units.
        fade: 0,
        bob: 0,
        level: unit.level,
        xp: 0,
        bag: { ...EMPTY_BAG },
        spells: { tier1: 0, tier2: 0, tier3: 0, tier4: 0, tier5: 0, tier6: 0, tier7: 0, tier8: 0, tier9: 0, tier10: 0 },
        weaponId: null,
        weaponEnh: 0,
        size: cls.size,
        footprintW: cls.footprintW,
        footprintH: cls.footprintH,
        footprintOffsets: cls.footprintOffsets,
        shock: null,
        shockCharges: 0,
        spellCharges: tier === 4 ? 3 : tier === 5 ? SUMMON_ZOMBIE_DOG.causticVenomCharges : tier === 3 ? familiarSpellCharges(unit.level) : familiarMagicMissileCharges(unit.level),
        lifeDrainCharges: tier === 2 || tier === 4 ? familiarLifeDrainCharges(unit.level) : undefined,
        summonerId: unit.id,
        diseased: false,
        diseaseBase: null,
        poisoned: false,
        bleeding: false,
        bleedMovedThisTurn: false,
        stunned: false,
        stunTurns: 0,
        crippled: false,
        hungerPenaltyPct: 0,
        fullness: 100,
        offHandId: null,
        gear: {},
        summoned: true,
        asleep: false,
        sleepTurns: 0,
        guaranteedDrop: false,
        dialog: null,
        moveBudgetUsed: 0,
    };
    this.spendTier(unit, spellKind);
    this.noteAwareEnmity(unit, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = `${unit.name} invocou ${familiar.name}.`;
    // Unlike the original instant summon, route the completed summon through the same
    // queued spell action that Birolho uses. The familiar remains exactly the same;
    // this only gives its caster the authored casting sequence before the turn ends.
    const step: Seq = {
        type: "spell",
        att: unit.id,
        tiles: [cell],
        ids: [],
        label: spellName,
        spellKind,
    };
    // The familiar, its portal and its sound appear when this step actually starts — after a
    // long-sheet caster's full wind-up (see startSeq), not the instant the spell is clicked.
    this.onSeqStart.set(step, () => {
        this.units.push(familiar);
        { }
        { }
    });
    this.queue.push(step);
}

protected castWebOfDreams(unit: Unit, click: Point): void {
    if (!this.spellAimValid(unit, click)) {
        this.tip = this.spellAimError(unit, click);
        { }
        return;
    }
    const radius = webOfDreamsSize(unit.level);
    const sleepChance = webOfDreamsSleepChance(unit.level);
    const cells = hexAreaTiles(click, radius, this.cols, this.rows);
    const cellKeys = new Set(cells.map((p) => key(p.x, p.y)));
    this.webZones.push({ cells: cellKeys, roundsLeft: WEB_OF_DREAMS.durationRounds, createdAt: this.time, center: { x: click.x, y: click.y }, radius, sleepChance });
    let asleepCount = 0;
    for (const u of this.units) {
        if (!u.alive || !cellKeys.has(key(u.x, u.y)))
            continue;
        if (this.rng() < sleepChance) {
            u.asleep = true;
            u.sleepTurns = rollDice(WEB_OF_DREAMS.sleepDice, WEB_OF_DREAMS.sleepFaces, 0, this.rng);
            asleepCount++;
        }
    }
    this.spendTier(unit, "webOfDreams");
    this.noteAwareEnmity(unit, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.missileTargets = [];
    { }
    { }
    this.tip = `${unit.name} conjurou ${WEB_OF_DREAMS.name}${asleepCount > 0 ? ` — ${asleepCount} adormeceu(ram)` : ""}.`;
    { }
    // Web of Dreams is a zone spell with no damage targets, but it still needs the
    // Conjurer's cast motion before the action is completed.
    this.queue.push({ type: "spell", att: unit.id, tiles: [click], ids: [], label: WEB_OF_DREAMS.name, spellKind: "webOfDreams" });
}

protected castCleave(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const tiles = cleaveHexes(unit, cell, CLEAVE.hexes, this.cols, this.rows);
    if (tiles.length === 0) {
        this.tip = "Toque num hex vizinho.";
        { }
        return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== unit.id && who.side !== unit.side && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(unit, "cleave");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = cleavePower(unit.level);
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        label: CLEAVE.name,
        weaponBonusDice: power.dice,
        weaponBonusFaces: power.faces,
        weaponBonusBonus: 0,
        spellKind: "cleave",
    });
}

/** Paladin tier 6: a holy line down the aimed direction — the one AoE that filters allies
 * OUT of `ids` rather than in, so it can never clip one. Bonus is a flat half-MAG term
 * (weaponBonusBonus) plus a level-gated die, on top of a plain weapon hit. */
protected castDivineWrath(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const tiles = this.wrathRay(unit, cell, DIVINE_WRATH.range);
    if (!tiles || tiles.length === 0) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== unit.id && who.side !== unit.side && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(unit, "divineWrath");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = divineWrathPower(unit.level);
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        label: DIVINE_WRATH.name,
        weaponBonusDice: power.dice,
        weaponBonusFaces: power.faces,
        weaponBonusBonus: Math.floor(unit.mag / 2),
        spellKind: "divineWrath",
    });
}

/** Heavy Knight tier 4: an arc of `hexes` neighbors (same cleaveHexes traversal as Cleave),
 * enemies only, each knocked back a fixed 2 hexes on top of the hit — see the
 * a.spellKind === "shoulderSmash" knockback loop in stepSpell. Refuses to arm at all while
 * a shield is equipped (see startShoulderSmash), so no equipment check needed here. */
protected castShoulderSmash(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const power = shoulderSmashPower(unit.level);
    const tiles = cleaveHexes(unit, cell, power.hexes, this.cols, this.rows);
    if (tiles.length === 0) {
        this.tip = "Toque num hex vizinho.";
        { }
        return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== unit.id && who.side !== unit.side && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(unit, "shoulderSmash");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        label: SHOULDER_SMASH.name,
        weaponBonusDice: power.dice,
        weaponBonusFaces: power.faces,
        weaponBonusBonus: 0,
        spellKind: "shoulderSmash",
    });
}

/** Heavy Knight tier 6: the same aimed line as Divine Wrath, but `ids` keeps EVERY unit in
 * the line except the caster themselves — allies included — which is the one thing that
 * tells it apart from Divine Wrath's ally-proof line. */
protected castStampede(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const tiles = this.wrathRay(unit, cell, STAMPEDE.range);
    if (!tiles || tiles.length === 0) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== unit.id && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(unit, "stampede");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = stampedePower(unit.level);
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        label: STAMPEDE.name,
        weaponBonusDice: power.dice,
        weaponBonusFaces: power.faces,
        weaponBonusBonus: 0,
        spellKind: "stampede",
    });
}

/** Priest tier 2: derives a facing axis from the aimed cell (wrathRay's first step is
 * always one exact hex-neighbor away, so it maps 1:1 onto one of the 6 CUBE_DIRS), then
 * hits every unit in the resulting wedge — no side filter, friendly fire is intentional. */
protected castBurningHands(unit: Unit, cell: Point, kind: "burningHands" | "poisonBreath" = "burningHands"): void {
    const power = (kind === "poisonBreath" ? poisonBreathPower : burningHandsPower)(unit.level);
    const ray = this.wrathRay(unit, cell, power.range);
    const dir = ray && ray[0] ? axisDir(unit, ray[0]) : null;
    if (!dir) {
        this.tip = this.spellAimError(unit, cell);
        { }
        return;
    }
    const tiles = coneSector(unit, dir, power.radius, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== unit.id && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(unit, kind);
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        dice: power.dice,
        faces: power.faces,
        bonus: 0,
        label: kind === "poisonBreath" ? POISON_BREATH.name : BURNING_HANDS.name,
        spellMul: power.mul,
        spellKind: kind,
        poison: kind === "poisonBreath",
    });
}

protected castIceStorm(unit: Unit, click: Point): void {
    if (!this.spellAimValid(unit, click)) {
        this.tip = this.spellAimError(unit, click);
        { }
        return;
    }
    const power = iceStormPower(unit.level);
    const cells = iceStormAreaTiles(click, unit.level, this.cols, this.rows);
    const cellKeys = new Set(cells.map((p) => key(p.x, p.y)));
    this.iceStormZones.push({
        cells: cellKeys,
        roundsLeft: power.durationRounds,
        createdAt: this.time,
        center: { ...click },
        radius: power.size,
        damageDice: power.dice,
        damageFaces: power.faces,
        damageMul: power.mul,
        casterMag: unit.mag,
        casterLevel: unit.level,
        casterId: unit.id,
        side: unit.side,
    });
    this.spendTier(unit, "iceStorm");
    this.spellKind = null;
    this.missileTargets = [];
    this.mode = "locked";
    this.tip = `${unit.name} conjura ${ICE_STORM.name}: área de ${power.areaHexes} hexes, ${power.durationRounds} rodadas.`;
    { }
    this.queue.push({ type: "spell", att: unit.id, tiles: [click], ids: [], label: ICE_STORM.name, spellKind: "iceStorm" });
}

protected castFireball(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    const tiles = fireballTiles(origin, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
        const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (u && !ids.includes(u.id))
            ids.push(u.id);
    }
    this.spendFamiliarOrTier(unit, "fireball");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = fireballPower();
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        dice: power.dice,
        faces: power.faces,
        bonus: power.bonus,
        label: FIREBALL.name,
        spellMul: FIREBALL.mul,
        spellKind: "fireball",
        projectileTo: origin,
    });
}

protected castCausticVenom(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    // Its own radius rather than Fireball's: fireballTiles hardcodes FIREBALL.size, which
    // is why venom could not be widened without widening Fireball with it.
    const tiles = hexAreaTiles(origin, CAUSTIC_VENOM.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
        const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (u && !ids.includes(u.id))
            ids.push(u.id);
    }
    const center = this.units.find((x) => x.alive && occupies(x, origin.x, origin.y));
    this.spendFamiliarOrTier(unit, "causticVenom");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        dice: CAUSTIC_VENOM.splashDice,
        faces: CAUSTIC_VENOM.splashFaces,
        bonus: CAUSTIC_VENOM.splashBonus,
        centerId: center?.id,
        centerDice: CAUSTIC_VENOM.centerDice,
        centerFaces: CAUSTIC_VENOM.centerFaces,
        centerBonus: CAUSTIC_VENOM.centerBonus,
        poison: true,
        label: CAUSTIC_VENOM.name,
        spellMul: CAUSTIC_VENOM.splashMul,
        centerMul: CAUSTIC_VENOM.centerMul,
        spellKind: "causticVenom",
        projectileTo: origin,
    });
}

protected castDivineBolt(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    const tiles = hexAreaTiles(origin, DIVINE_BOLT.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
        const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (u && !ids.includes(u.id))
            ids.push(u.id);
    }
    const center = this.units.find((x) => x.alive && occupies(x, origin.x, origin.y));
    this.spendTier(unit, "divineBolt");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        dice: DIVINE_BOLT.splashDice,
        faces: DIVINE_BOLT.splashFaces,
        bonus: DIVINE_BOLT.splashBonus,
        centerId: center?.id,
        centerDice: DIVINE_BOLT.centerDice,
        centerFaces: DIVINE_BOLT.centerFaces,
        centerBonus: DIVINE_BOLT.centerBonus,
        label: DIVINE_BOLT.name,
        spellMul: DIVINE_BOLT.splashMul,
        centerMul: DIVINE_BOLT.centerMul,
        spellKind: "divineBolt",
        projectileTo: origin,
    });
}

/** Veneno Menor (MINOR_VENOM): Caustic Venom's dice and poison on a radius-2 splash. */
protected castMinorVenom(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    this.spendFamiliarOrTier(unit, "minorVenom");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queueMinorVenom(unit, origin);
}

/** Enemy Choque: same ignore-cover targeting as Relâmpago, ~1/3 the stats. Returns true
 * if a cast was queued so the caller can skip the rest of the AI. */
protected tryAiShock(next: Unit, reach: ReturnType<typeof computeReachable>, walkReach: ReturnType<typeof computeReachable>, players: Unit[]): boolean {
    if (next.shockCharges <= 0)
        return false;
    let best: {
        foe: Unit;
        from: Point;
        score: number;
    } | null = null;
    for (const cell of reach.values()) {
        for (const foe of players) {
            if (manhattan(cell, foe) > SHOCK.range)
                continue;
            const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
            if (!best || score > best.score)
                best = { foe, from: { x: cell.x, y: cell.y }, score };
        }
    }
    if (!best)
        return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
        this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    next.shockCharges -= 1;
    this.queue.push({
        type: "spell",
        att: next.id,
        tiles: [{ x: best.foe.x, y: best.foe.y }],
        ids: [best.foe.id],
        dice: SHOCK.dice,
        faces: SHOCK.faces,
        bonus: SHOCK.bonus,
        label: SHOCK.name,
        echo: { dice: SHOCK.echoDice, faces: SHOCK.echoFaces, bonus: SHOCK.echoBonus },
        spellMul: SHOCK.mul,
        spellKind: "shock",
    });
    this.queue.push({ type: "delay", dur: 0.12 });
    return true;
}

/** Enemy Veneno Menor (Undead Ox): the Birolho branch's venom targeting on the smaller
 * splash. Returns true if a cast was queued so the caller can skip the rest of the AI. */
protected tryAiMinorVenom(next: Unit, reach: ReturnType<typeof computeReachable>, walkReach: ReturnType<typeof computeReachable>, players: Unit[]): boolean {
    if (this.tierRemaining(next, "minorVenom") <= 0)
        return false;
    let best: {
        at: Point;
        from: Point;
        score: number;
    } | null = null;
    for (const cell of reach.values()) {
        for (const foe of players) {
            if (manhattan(cell, foe) > MINOR_VENOM.range)
                continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay))
                continue;
            let hits = 0;
            let score = 0;
            for (const t of hexAreaTiles({ x: foe.x, y: foe.y }, MINOR_VENOM.size, this.cols, this.rows)) {
                const hit = players.find((p) => p.x === t.x && p.y === t.y);
                if (!hit)
                    continue;
                hits += 1;
                score += (hit.maxHp - hit.hp) + (hit.hp <= 8 ? 15 : 0);
            }
            if (hits === 0)
                continue;
            score += hits * 10;
            if (!best || score > best.score)
                best = { at: { x: foe.x, y: foe.y }, from: { x: cell.x, y: cell.y }, score };
        }
    }
    if (!best)
        return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
        this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    this.spendTier(next, "minorVenom");
    this.queueMinorVenom(next, best.at);
    this.queue.push({ type: "delay", dur: 0.12 });
    return true;
}

/** Carnivorous Plant: with more than one foe in reach of her swipe (from any cell she can
 * reach this turn), she swipes them all with a weapon hit on her ATT sheet. */
protected tryAiPlantSwipe(next: Unit, reach: ReturnType<typeof computeReachable>, walkReach: ReturnType<typeof computeReachable>): boolean {
    let best: {
        tiles: Point[];
        ids: string[];
        from: Point;
        score: number;
    } | null = null;
    for (const cell of reach.values()) {
        const tiles = this.plantSwipeTiles(next, cell);
        const ids: string[] = [];
        let score = 0;
        for (const t of tiles) {
            const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
            if (!who || who.id === next.id || who.side === next.side || ids.includes(who.id))
                continue;
            ids.push(who.id);
            score += 10 + (who.maxHp - who.hp) * 3 + (who.hp <= 8 ? 20 : 0);
        }
        if (ids.length < 2)
            continue;
        if (!best || score > best.score)
            best = { tiles, ids, from: { x: cell.x, y: cell.y }, score };
    }
    if (!best)
        return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
        this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    this.queue.push({ type: "spell", att: next.id, tiles: best.tiles, ids: best.ids, label: "Chicote de Gavinhas", spellKind: "tendrilSwipe" });
    this.queue.push({ type: "delay", dur: 0.12 });
    return true;
}

/** Carnivorous Plant's 2 Veneno Cáustico: the Birolho branch's venom targeting, spending
 * her own tier3 slot (her tier4 holds Veneno Menor). */
protected tryAiPlantCausticVenom(next: Unit, reach: ReturnType<typeof computeReachable>, walkReach: ReturnType<typeof computeReachable>, players: Unit[]): boolean {
    if (next.spells.tier3 <= 0)
        return false;
    let best: {
        at: Point;
        from: Point;
        score: number;
    } | null = null;
    for (const cell of reach.values()) {
        for (const foe of players) {
            if (manhattan(cell, foe) > CAUSTIC_VENOM.range)
                continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay))
                continue;
            let hits = 0;
            let score = 0;
            for (const t of hexAreaTiles({ x: foe.x, y: foe.y }, CAUSTIC_VENOM.size, this.cols, this.rows)) {
                const hit = players.find((p) => p.x === t.x && p.y === t.y);
                if (!hit)
                    continue;
                hits += 1;
                score += (hit.maxHp - hit.hp) + (hit.hp <= 8 ? 15 : 0);
            }
            if (hits === 0)
                continue;
            score += hits * 10;
            if (!best || score > best.score)
                best = { at: { x: foe.x, y: foe.y }, from: { x: cell.x, y: cell.y }, score };
        }
    }
    if (!best)
        return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
        this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    next.spells.tier3 -= 1;
    const tiles = hexAreaTiles(best.at, CAUSTIC_VENOM.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
        const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (u && !ids.includes(u.id))
            ids.push(u.id);
    }
    const center = this.units.find((x) => x.alive && occupies(x, best.at.x, best.at.y));
    this.queue.push({
        type: "spell",
        att: next.id,
        tiles,
        ids,
        dice: CAUSTIC_VENOM.splashDice,
        faces: CAUSTIC_VENOM.splashFaces,
        bonus: CAUSTIC_VENOM.splashBonus,
        centerId: center?.id,
        centerDice: CAUSTIC_VENOM.centerDice,
        centerFaces: CAUSTIC_VENOM.centerFaces,
        centerBonus: CAUSTIC_VENOM.centerBonus,
        poison: true,
        label: CAUSTIC_VENOM.name,
        spellMul: CAUSTIC_VENOM.splashMul,
        centerMul: CAUSTIC_VENOM.centerMul,
        spellKind: "causticVenom",
    });
    this.queue.push({ type: "delay", dur: 0.12 });
    return true;
}

/** Carnivorous Plant's 2 Poison Breath: Rocco's Burning Beak cone targeting (never aimed
 * where it would also hit one of her allies), with Poison Breath's power and poison. */
protected tryAiPlantPoisonBreath(next: Unit, reach: ReturnType<typeof computeReachable>, walkReach: ReturnType<typeof computeReachable>): boolean {
    if (next.spells.tier1 <= 0)
        return false;
    const power = poisonBreathPower(next.level);
    let best: {
        tiles: Point[];
        ids: string[];
        from: Point;
        score: number;
    } | null = null;
    for (const cell of reach.values()) {
        for (const dir of CUBE_DIRS) {
            const tiles = coneSector(cell, dir, power.radius, this.cols, this.rows);
            const ids: string[] = [];
            let score = 0;
            let hitsAlly = false;
            for (const t of tiles) {
                const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
                if (!who || who.id === next.id || ids.includes(who.id))
                    continue;
                if (who.side === next.side)
                    hitsAlly = true;
                ids.push(who.id);
                score += 10 + (who.maxHp - who.hp) * 3 + (who.hp <= 8 ? 20 : 0);
            }
            if (hitsAlly || !ids.length)
                continue;
            if (!best || score > best.score)
                best = { tiles, ids, from: { x: cell.x, y: cell.y }, score };
        }
    }
    if (!best)
        return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
        this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    this.spendTier(next, "poisonBreath");
    this.queue.push({
        type: "spell",
        att: next.id,
        tiles: best.tiles,
        ids: best.ids,
        dice: power.dice,
        faces: power.faces,
        bonus: 0,
        label: POISON_BREATH.name,
        spellMul: power.mul,
        spellKind: "poisonBreath",
        poison: true,
    });
    this.queue.push({ type: "delay", dur: 0.12 });
    return true;
}

/** Carnivorous Plant's tendril swipe area with her anchor at `at`: every hex touching her
 * Type 7 body (front and both flanks), except the row behind her head. */
protected plantSwipeTiles(next: Unit, at: Point): Point[] {
    const body = footprint({ ...next, x: at.x, y: at.y });
    const inBody = (p: Point) => body.some((b) => b.x === p.x && b.y === p.y);
    const out: Point[] = [];
    for (const b of body) {
        for (const p of hexNeighbors(b.x, b.y)) {
            if (p.y < at.y - 2 || !inBounds(p.x, p.y, this.cols, this.rows) || inBody(p))
                continue;
            if (!out.some((o) => o.x === p.x && o.y === p.y))
                out.push(p);
        }
    }
    return out;
}

/** Whether a wound-up step's remaining sheet (see heldFrame) has finished playing. */
/** Whether the current stage of this attack is struck with the off-hand dagger/katar: the
 * attacker's own off-hand attack (customDice) or a defender's off-hand counter. */
protected offHandStrike(a: CombatImpact): boolean {
    return a.stage.startsWith("counter") ? !!a.counterCustomDice : !!a.customDice;
}

/** True only for bow/crossbow users. Reach weapons strike physically instead of firing arrows. */
protected isArrowAttack(unit: Unit): boolean {
    // Reach weapons are always physical, even if an imported loadout is incorrectly flagged ranged.
    if (unit.classId === "pikeman" || unit.classId === "basePikeman" || unit.classId === "lancer" || unit.classId === "aldric" || unit.classId === "sandoval" || unit.classId === "sentinel" || unit.classId === "templar")
        return false;
    // Besteiros are enemy crossbow users; an imported/legacy weapon id must never turn their
    // ranged attack into a silent or melee action.
    if (unit.classId === "brigand" || unit.classId === "baseCrossbowman" || unit.classId === "brigandCrossbowman")
        return true;
    if (unit.weaponId)
        return !!WEAPONS[unit.weaponId]?.ranged;
    // Default campaign loadouts: Neera starts as a bow user before gear is assigned.
    return unit.classId === "archer" || unit.classId === "ranger" || unit.classId === "assassin";
}

/** Only spellcasting classes use the distinct basic-attack arcane bolt. */
protected isArcaneCaster(unit: Unit): boolean {
    return unit.classId === "mage" || unit.classId === "voss" || unit.classId === "elementalist" || unit.classId === "warlock" || unit.classId === "cultist" || unit.classId === "cultistV2" || unit.classId === "birolho" || unit.classId === "birolho2" || unit.classId === "birolho3" || unit.classId === "birolhoLegs" || unit.classId === "birolhoLegs2";
}

/** Each completed action builds affinity with heroes fighting beside its actor. */
protected gainAdjacentAffinity(u: Unit): void {
    if (u.acted || !u.alive || u.side !== "player" || u.summoned)
        return;
    const recipients = this.supportAffinityRecipients.get(u.id);
    for (const ally of this.adjacentAllies(u)) {
        if (!recipients?.has(ally.id))
            this.adjustAffinity(u, ally, 0.1);
    }
    this.supportAffinityRecipients.delete(u.id);
}

protected nudgeOffHazard(unit: Unit): void {
    const here = this.hexAt(unit.x, unit.y);
    if (here.passable || canTraverseWater(unit, here, this.decorOverlay, unit.x, unit.y, this.cols))
        return;
    const occ = this.occ();
    const seen = new Set<string>([key(unit.x, unit.y)]);
    const q: Point[] = [{ x: unit.x, y: unit.y }];
    while (q.length) {
        const cur = q.shift()!;
        for (const n of hexNeighbors(cur.x, cur.y)) {
            if (n.x < 0 || n.y < 0 || n.x >= this.cols || n.y >= this.rows)
                continue;
            const k = key(n.x, n.y);
            if (seen.has(k))
                continue;
            seen.add(k);
            const terr = this.hexAt(n.x, n.y);
            const who = occ.get(k);
            if ((terr.passable || canTraverseWater(unit, terr, this.decorOverlay, n.x, n.y, this.cols)) && (!who || who.id === unit.id)) {
                unit.x = n.x;
                unit.y = n.y;
                unit.drawX = n.x;
                unit.drawY = n.y;
                return;
            }
            q.push(n);
        }
    }
}

/** A hero must never start a map standing on a waypoint, or the "use waypoint" prompt
 * fires immediately. Moves it to the nearest passable, unoccupied, non-waypoint hex
 * (occ() includes body-type zones, so no zone is ever entered). */
protected nudgeOffWaypoint(unit: Unit): void {
    const waypointCells = new Set<string>();
    for (const d of this.decorations) {
        if (!DECORATIONS[d.id]?.exitKind)
            continue;
        for (const f of placedFootprint(d))
            waypointCells.add(key(d.x + f.dx, d.y + f.dy));
    }
    if (!waypointCells.has(key(unit.x, unit.y)))
        return;
    const occ = this.occ();
    const seen = new Set<string>([key(unit.x, unit.y)]);
    const q: Point[] = [{ x: unit.x, y: unit.y }];
    while (q.length) {
        const cur = q.shift()!;
        for (const n of hexNeighbors(cur.x, cur.y)) {
            if (n.x < 0 || n.y < 0 || n.x >= this.cols || n.y >= this.rows)
                continue;
            const k = key(n.x, n.y);
            if (seen.has(k))
                continue;
            seen.add(k);
            const who = occ.get(k);
            if (this.hexAt(n.x, n.y).passable && !waypointCells.has(k) && (!who || who.id === unit.id)) {
                unit.x = n.x;
                unit.y = n.y;
                unit.drawX = n.x;
                unit.drawY = n.y;
                return;
            }
            q.push(n);
        }
    }
}

protected uniqueNeutralNpcSpawns(spawns: readonly Spawn[]): {
    spawn: Spawn;
    index: number;
}[] {
    const names = new Set<string>();
    const appearances = new Set<string>();
    return spawns.flatMap((spawn, index) => {
        if (!spawn.dialog)
            return [{ spawn, index }];
        const name = spawn.name.normalize("NFD").replace(/[\u0300-\u036f]/g, "").trim().toLocaleLowerCase();
        const appearance = CLASSES[spawn.classId]?.sprite ?? spawn.classId;
        if (names.has(name) || appearances.has(appearance))
            return [];
        names.add(name);
        appearances.add(appearance);
        return [{ spawn, index }];
    });
}

/** One battle-opening roll per unit: 1d20 + its archetype modifier. The class data's
 * historical scale is a delay (1 fast .. 10 slow), so 11-delay is the additive bonus. */
protected rollOpeningInitiative(units: Unit[]): void {
    for (const unit of units) {
        unit.initiativeRoll = 1 + Math.floor(this.rng() * 20);
        unit.initiative = unit.initiativeRoll + initiativeBonus(unit.classId);
    }
}

/** Highest opening initiative first. On an exact draw, the player wins. */
protected sortByInitiative(units: Unit[]): string[] {
    return [...units]
        .sort((a, b) => {
        if (a.initiative !== b.initiative)
            return b.initiative - a.initiative;
        if (a.side !== b.side)
            return a.side === "player" ? -1 : 1;
        return a.id.localeCompare(b.id);
    })
        .map((u) => u.id);
}

/** The strongest level among this battle's own enemy spawns — a per-spawn value (see
 * Mission.enemySpawns[].level, falling back to enemyLevelFor(mission.index) at spawn
 * time), already on the same 1..MAX_LEVEL scale gearPowerLevel runs on. Loot rolls cap
 * to this directly instead of stretching the coarse mission-index curve, so a mission
 * whose enemies are actually weak can't hand out gear built for a much harder one. */
protected highestEnemyLevel(): number {
    let max = 1;
    for (const u of this.units) {
        if (u.side === "enemy" && u.level > max)
            max = u.level;
    }
    return max;
}

protected confirmSweep(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || this.mode !== "awaitSpell" || this.spellKind !== "sweep")
        return;
    const tiles = this.sweepTiles(u);
    const ids: string[] = [];
    for (const t of tiles) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (who && who.id !== u.id && who.side !== u.side && !ids.includes(who.id))
            ids.push(who.id);
    }
    this.spendTier(u, "sweep");
    this.spellKind = null;
    this.missileTargets = [];
    this.spellArmed = false;
    this.spellAim = null;
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: u.id, tiles, ids, label: SWEEP.name, spellKind: "sweep" });
    if (u.name !== "Kael") { }
}

/** Aura of Protection (Paladin tier 5) / Intimidating Presence (Heavy Knight tier 5): both
 * instant and self-centered, same as Sweep — no aim, no confirmSpell branch needed. */
startAuraOfProtection(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "auraOfProtection") <= 0)
        return;
    const p = auraPower(u.level);
    const cells = new Set(hexAreaTiles({ x: u.x, y: u.y }, p.radius, this.cols, this.rows).map((c) => key(c.x, c.y)));
    this.auraZones.push({ cells, roundsLeft: p.duration, kind: "protection", side: u.side, pct: p.pct });
    this.spendTier(u, "auraOfProtection");
    this.noteAwareEnmity(u, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.tip = `${AURA_OF_PROTECTION.name}: aliados a até ${p.radius} hexes tomam ${Math.round(p.pct * 100)}% menos dano por ${p.duration} rodadas.`;
    this.mode = "locked";
    this.queue.push({ type: "banner", text: AURA_OF_PROTECTION.name, dur: 1.1 });
    { }
}

startIntimidatingPresence(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "intimidatingPresence") <= 0)
        return;
    const p = auraPower(u.level);
    const cells = new Set(hexAreaTiles({ x: u.x, y: u.y }, p.radius, this.cols, this.rows).map((c) => key(c.x, c.y)));
    this.auraZones.push({ cells, roundsLeft: p.duration, kind: "intimidation", side: u.side, pct: p.pct });
    this.spendTier(u, "intimidatingPresence");
    this.noteAwareEnmity(u, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.tip = `${INTIMIDATING_PRESENCE.name}: inimigos a até ${p.radius} hexes tomam ${Math.round(p.pct * 100)}% mais dano por ${p.duration} rodadas.`;
    this.mode = "locked";
    { }
    this.queue.push({ type: "banner", text: INTIMIDATING_PRESENCE.name, dur: 1.1 });
    { }
}

/** Healer tier 1 Bless: the caster is the center and every living ally within three hexes
 * receives the level-scaled accuracy bonus as the authored 3D wave reaches them. */
startBless(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "bless") <= 0)
        return;
    if (rulesClass(u.classId) !== "healer")
        return;
    if (u.level < BLESS.unlockLevel) {
        this.tip = `Bless disponível a partir do nível ${BLESS.unlockLevel}.`;
        { }
        return;
    }
    const allies = this.units.filter((target) => target.alive && target.side === u.side && hexDist(u, target) <= BLESS.radius);
    this.spendTier(u, "bless");
    this.noteAwareEnmity(u, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.tip = `Bless: +${BLESS.hitBonusPct(u.level)}% de chance de acerto por ${BLESS.durationRounds(u.level)} rodadas para aliados a até ${BLESS.radius} hexes.`;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: u.id, tiles: allies.map((target) => ({ x: target.x, y: target.y })), ids: allies.map((target) => target.id), label: BLESS.name, spellKind: "bless" });
    { }
}

/** Healer tier 3: resolves instantly like Aura of Protection/Intimidating Presence — never
 * arms awaitSpell, so it never reaches spellAimValid/confirmSpell. Tops off the hunger of
 * the caster and every ally within CREATE_FOOD_AND_WATER.radius hexes, adding plain Rations
 * per ally fed to the same pool a battle-picked-up ration would (see lootRations,
 * reconciled back into save.rations at battle end — see GameApp.tsx). */
startCreateFoodAndWater(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "createFoodAndWater") <= 0)
        return;
    const power = createFoodAndWaterPower(u.level);
    const targets = this.units.filter((t) => t.alive && t.side === u.side && hexDist(u, t) <= CREATE_FOOD_AND_WATER.radius && fullness(t.fullness) < power.fullness);
    if (targets.length === 0) {
        this.tip = "Já está bem alimentado.";
        { }
        return;
    }
    let gained = 0;
    for (const t of targets) {
        t.fullness = power.fullness;
        t.hungerPenaltyPct = 0;
        this.reapplyGear(t);
        gained += power.dice > 0 ? rollDice(power.dice, power.faces, power.bonus, this.rng) : 0;
        { }
    }
    this.lootRations += gained;
    this.spendTier(u, "createFoodAndWater");
    this.noteAwareEnmity(u, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.mode = "locked";
    this.tip = `${CREATE_FOOD_AND_WATER.name}: fome restaurada${power.fullness > 100 ? ` (${power.fullness}%)` : ""}${gained > 0 ? `, +${gained} rações` : ""}.`;
    this.queue.push({ type: "banner", text: CREATE_FOOD_AND_WATER.name, dur: 1.1 });
    { }
}

protected spellAimValid(caster: Unit, cell: Point): boolean {
    if (!this.spellKind)
        return false;
    if (this.spellKind === "warp") {
        if (!inBounds(cell.x, cell.y, this.cols, this.rows) || !this.hexAt(cell.x, cell.y).passable)
            return false;
        return !this.units.some(unit => unit.alive && occupies(unit, cell.x, cell.y));
    }
    if (this.spellKind === "fireball") {
        if (manhattan(caster, cell) > FIREBALL.range)
            return false;
        return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "frost")
        return this.frostTiles(caster, cell).length > 0;
    if (this.spellKind === "iceStorm") {
        if (manhattan(caster, cell) > iceStormPower(caster.level).range)
            return false;
        return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "causticVenom") {
        if (manhattan(caster, cell) > CAUSTIC_VENOM.range)
            return false;
        return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "divineBolt") {
        if (manhattan(caster, cell) > DIVINE_BOLT.range)
            return false;
        return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "minorVenom") {
        if (manhattan(caster, cell) > MINOR_VENOM.range)
            return false;
        return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "longShot") {
        const d = manhattan(caster, cell);
        const here = this.occ().get(key(cell.x, cell.y));
        if (!this.targetable(here) || d < caster.minRange || d > this.longMax(caster))
            return false;
        return clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay);
    }
    if (this.spellKind === "bloodyShot") {
        const d = manhattan(caster, cell);
        const here = this.occ().get(key(cell.x, cell.y));
        if (!this.targetable(here) || d < caster.minRange || d > BLOODY_SHOT.range)
            return false;
        return clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay);
    }
    if (this.spellKind === "piercing")
        return this.piercingRay(caster, cell) !== null;
    if (this.spellKind === "piercingThrust")
        return this.piercingThrustRay(caster, cell) !== null;
    if (this.spellKind === "lightning") {
        const here = this.occ().get(key(cell.x, cell.y));
        if (!this.targetable(here) || manhattan(caster, cell) > LIGHTNING.range)
            return false;
        return true;
    }
    if (this.spellKind === "lightningTier3") {
        const here = this.occ().get(key(cell.x, cell.y));
        if (!this.targetable(here) || manhattan(caster, cell) > LIGHTNING_T3.range)
            return false;
        return true;
    }
    if (this.spellKind === "shock") {
        const here = this.occ().get(key(cell.x, cell.y));
        if (!this.targetable(here) || manhattan(caster, cell) > SHOCK.range)
            return false;
        return true;
    }
    if (this.spellKind === "sweep") {
        return manhattan(caster, cell) <= SWEEP.radius;
    }
    if (this.spellKind === "magicMissile" || this.spellKind === "magicMissileV2") {
        return this.magicMissileTargetError(caster, cell) === null && this.targetable(this.occ().get(key(cell.x, cell.y)));
    }
    if (this.spellKind === "phantasmalForce") {
        const here = this.occ().get(key(cell.x, cell.y));
        if (!this.targetable(here) || manhattan(caster, cell) > PHANTASMAL_FORCE.range)
            return false;
        return clearShot(caster, cell, this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "summonFamiliar" || this.spellKind === "summonFamiliar2" || this.spellKind === "summonFamiliar3" || this.spellKind === "summonFamiliar4" || this.spellKind === "summonZombieDog") {
        const range = this.spellKind === "summonZombieDog" ? SUMMON_ZOMBIE_DOG.range : this.spellKind === "summonFamiliar4" ? SUMMON_FAMILIAR4.range : this.spellKind === "summonFamiliar3" ? SUMMON_FAMILIAR3.range : this.spellKind === "summonFamiliar2" ? SUMMON_FAMILIAR2.range : SUMMON_FAMILIAR.range;
        if (manhattan(caster, cell) > range)
            return false;
        // Familiar 3 is a real multi-hex creature (FOOTPRINT_TYPE_6) — every cell of the shape
        // it would actually occupy has to be checked, not just the anchor tile, or it can be
        // summoned half-overlapping a wall/unit/off-map edge (same class of bug computeReachable
        // was fixed for — see footprintCost's comment in pathfinding.ts).
        const bodyClass = this.spellKind === "summonFamiliar3" ? CLASSES.familiar3! : this.spellKind === "summonZombieDog" ? CLASSES.zombieDog! : null;
        const cells = bodyClass ? footprint({ x: cell.x, y: cell.y, size: bodyClass.size, footprintOffsets: bodyClass.footprintOffsets }) : [cell];
        const occ = this.occ();
        for (const p of cells) {
            if (!inBounds(p.x, p.y, this.cols, this.rows))
                return false;
            if (!this.hexAt(p.x, p.y).passable)
                return false;
            if (occ.get(key(p.x, p.y)))
                return false;
        }
        return true;
    }
    if (this.spellKind === "webOfDreams") {
        if (manhattan(caster, cell) > WEB_OF_DREAMS.range)
            return false;
        return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "doubleStrike" || this.spellKind === "trip" || this.spellKind === "lifeDrain" || this.spellKind === "executionerStrike" || this.spellKind === "shieldBash") {
        const here = this.occ().get(key(cell.x, cell.y));
        return !!here && here.alive && here.side !== caster.side && canHitFrom(caster, caster, here, this.tiles, this.cols, this.decorOverlay);
    }
    if (this.spellKind === "provoke") {
        const here = this.occ().get(key(cell.x, cell.y));
        return !!here && here.alive && here.side === "enemy" && this.targetable(here) && hexDist(caster, cell) <= provokePower(caster.level).range;
    }
    if (this.spellKind === "cleave" || this.spellKind === "shoulderSmash") {
        return hexNeighbors(caster.x, caster.y).some((p) => p.x === cell.x && p.y === cell.y);
    }
    if (this.spellKind === "bullRush") {
        return this.bullRushCharge(caster, cell) !== null;
    }
    if (this.spellKind === "burningHands" || this.spellKind === "poisonBreath") {
        return this.wrathRay(caster, cell, (this.spellKind === "poisonBreath" ? poisonBreathPower : burningHandsPower)(caster.level).range) !== null;
    }
    if (this.spellKind === "multiShot") {
        const d = manhattan(caster, cell);
        const here = this.occ().get(key(cell.x, cell.y));
        if (!this.targetable(here) || d < caster.minRange || d > MULTI_SHOT.range)
            return false;
        return clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay);
    }
    if (this.spellKind === "divineWrath")
        return this.wrathRay(caster, cell, DIVINE_WRATH.range) !== null;
    if (this.spellKind === "stampede")
        return this.wrathRay(caster, cell, STAMPEDE.range) !== null;
    if (this.spellKind === "cureDisease")
        return this.validCureDiseaseTarget(caster, cell);
    return this.validHealTarget(caster, cell);
}

protected queueFrost(u: Unit, origin: Point, through: Point): void { const cells = this.frostTiles(origin, through, u.level); if (!cells.length)
    return; const ids = this.units.filter(target => target.alive && target.id !== u.id && cells.some(c => occupies(target, c.x, c.y))).map(target => target.id); const p = frostPower(u.level); this.spendTier(u, "frost"); this.queue.push({ type: "spell", att: u.id, tiles: cells, ids, dice: p.dice, faces: p.faces, bonus: 0, spellMul: p.mul, label: FROST.name, spellKind: "frost" }); }

protected frostTiles(origin: Point, through: Point, level?: number): Point[] {
    const caster = origin as Unit;
    const cells = frostAreaTiles(origin, through, level ?? caster.level, this.cols, this.rows);
    const out: Point[] = [];
    for (const cell of cells) {
        if (tileAt(this.tiles, this.cols, cell.x, cell.y) === "void" || !clearShot(origin, cell, this.tiles, this.cols, "bolt", this.decorOverlay))
            break;
        out.push(cell);
    }
    return out;
}

protected turnUndeadTiles(cell: Point, level: number): Point[] {
    return hexAreaTiles(cell, turnUndeadPower(level).radius, this.cols, this.rows)
        .filter(p => tileAt(this.tiles, this.cols, p.x, p.y) !== "void");
}

protected tierRemaining(u: Unit, kind: SpellKind): number {
    if (kind === "frost" && u.side === "enemy")
        return u.frostCharges ?? 0;
    if (kind === "poisonBreath" && u.level < POISON_BREATH.unlockLevel)
        return 0;
    if (kind === "burningHands" && u.level < 5)
        return 0;
    const tier = spellTier(kind);
    return tier ? u.spells[tierKey(tier)] : 0;
}

protected spendTier(u: Unit, kind: SpellKind): void {
    if (kind === "frost" && u.side === "enemy") {
        u.frostCharges = Math.max(0, (u.frostCharges ?? 0) - 1);
        return;
    }
    const tier = spellTier(kind);
    if (!tier)
        return;
    u.spells[tierKey(tier)] -= 1;
}

protected longMax(u: Unit): number {
    return LONG_SHOT.range;
}

protected isHeal(kind: SpellKind | null): kind is HealId {
    return kind === "cureMinor" || kind === "cureWounds" || kind === "cureLight";
}

/** True while (x,y) sits inside any still-active Web of Dreams patch. */
protected isWebCell(x: number, y: number): boolean {
    return this.webZones.some((z) => z.cells.has(key(x, y)));
}

/** The sleep chance of whichever Dreaming Web zone covers (x,y) — set once at cast time
 * from the caster's level (see castWebOfDreams/webOfDreamsSleepChance) and carried on the
 * zone itself, so a lingering roll always uses the level that created the zone rather than
 * whatever level some other unit is at now. Falls back to the base chance for a zone
 * restored from an older save that predates this field. */
protected webCellSleepChance(x: number, y: number): number {
    const zone = this.webZones.find((z) => z.cells.has(key(x, y)));
    return zone?.sleepChance ?? WEB_OF_DREAMS.sleepChance;
}

/** Combined multiplier from every active Aura of Protection / Intimidating Presence zone
 * covering `defender`'s current cell — applied to the final damage of a hit right before it
 * comes off their HP, same insertion point as the sleepBonusDamage multiplier. Protection
 * only discounts a zone's own side; Intimidating Presence only surcharges the other side, so
 * a unit standing in both a friendly and a hostile zone at once takes both at the same time. */
protected zoneDamageMul(defender: Unit): number {
    let mul = 1;
    for (const z of this.auraZones) {
        if (!z.cells.has(key(defender.x, defender.y)))
            continue;
        if (z.kind === "protection" && z.side === defender.side)
            mul *= 1 - z.pct;
        if (z.kind === "intimidation" && z.side !== defender.side)
            mul *= 1 + z.pct;
    }
    return mul;
}

/**
 * Whether the player may swing at, shoot or cast on this unit — `attackableByPlayer`
 * plus sight. Every enemy-targeting branch of spellAimValid and the attack picker
 * funnel through here, which is the whole of "you cannot aim at what you cannot
 * see": a foe standing in the dark is not a legal target, so no spell arms on it
 * and no attack offers itself.
 *
 * Fog does not go the other way. `occ` stays sight-blind, so an unseen body still
 * blocks a step and still stops an arrow — walking through one because the party
 * had not spotted it yet would be a worse lie than not being able to shoot it.
 */
protected targetable(u: Unit | undefined): u is Unit {
    if (!u || !u.alive || u.dialog)
        return false;
    // debugFreeCast drops attackableByPlayer's ally/enemy filter (so a single-target spell
    // can be aimed at your own party too) but the dialog-NPC exclusion above still always
    // applies — a talk-only fixture unit still isn't a sane thing to fireball.
    if (!this.debugFreeCast && !attackableByPlayer(u))
        return false;
    return !this.unitHidden(u);
}

protected validHealTarget(caster: Unit, cell: Point): boolean {
    if (!this.isHeal(this.spellKind))
        return false;
    const range = CURES[this.spellKind].range;
    if (manhattan(caster, cell) > range)
        return false;
    const occ = this.occ();
    const who = occ.get(key(cell.x, cell.y));
    if (!who || !who.alive)
        return false;
    if (this.debugFreeCast)
        return true;
    return who.side === "player" && who.hp < who.maxHp;
}

protected validCureDiseaseTarget(caster: Unit, cell: Point): boolean {
    if (manhattan(caster, cell) > CURE_DISEASE.range)
        return false;
    const occ = this.occ();
    const who = occ.get(key(cell.x, cell.y));
    if (!who || !who.alive)
        return false;
    if (this.debugFreeCast)
        return true;
    return who.side === "player" && (who.diseased || who.poisoned);
}

/** Give Magic Missile's target picker and cast guard the same actionable reason. */
protected magicMissileTargetError(caster: Unit, cell: Point): string | null {
    const here = this.occ().get(key(cell.x, cell.y));
    if (!here || !here.alive || here.dialog || this.unitHidden(here)) {
        return "Não há inimigo visível nessa casa.";
    }
    if (!this.debugFreeCast && here.side !== "enemy" && here.side !== "neutral") {
        return "Míssil Mágico não pode mirar em aliados.";
    }
    if (manhattan(caster, cell) > MAGIC_MISSILE.range) {
        return `Alvo fora de alcance (máximo ${MAGIC_MISSILE.range} hexes).`;
    }
    if (!clearShot(caster, cell, this.tiles, this.cols, "bolt", this.decorOverlay)) {
        return this.shotBlockedTip(caster, cell, "bolt");
    }
    return null;
}

/** Divine Wrath / Stampede: the same directional-line traversal as Piercing (aimed by
 * clicking through a cell to set the direction), just capped to their own range instead of
 * running the length of the board. */
protected wrathRay(caster: Unit, through: Point, range: number): Point[] | null {
    const raw = this.piercingRay(caster, through);
    if (!raw)
        return null;
    const capped = raw.slice(0, range);
    return capped.length ? capped : null;
}

protected piercingRay(from: Point, through: Point): Point[] | null {
    const raw = piercingLine(from, through, this.cols, this.rows);
    if (!raw)
        return null;
    const fromHigh = !!this.hexAt(from.x, from.y).height;
    const out: Point[] = [];
    for (const p of raw) {
        const t = this.hexAt(p.x, p.y);
        if (t.id === "barricade" || t.blocksShot)
            break;
        if (t.height && !fromHigh)
            break;
        out.push(p);
    }
    return out.length ? out : null;
}

/** Piercing Thrust (Lancer tier 1): the same straight-line traversal as Piercing, capped
 * to the caster's own weapon reach + 1 hex — a short lunge, not an arrow flying the length
 * of the board. */
protected piercingThrustRay(caster: Unit, through: Point): Point[] | null {
    const raw = this.piercingRay(caster, through);
    if (!raw)
        return null;
    const capped = raw.slice(0, caster.maxRange + 1);
    return capped.length ? capped : null;
}

/** Bull Rush: aim at an enemy up to BULL_RUSH_RANGE away and charge the straight line to
 * it. Any enemy standing in the way is hit and shoved aside (to whichever flank has room,
 * off the rest of the line) and the charge carries on; the aimed enemy is hit and pushed
 * forward. If an in-the-way enemy has no room on either side, the charge stops there and
 * it becomes the one pushed forward. An ally or blocking terrain in the way = no charge.
 * Everything is simulated on a copy of the board, so no push or landing hex ever overlaps
 * a unit or a body-type target zone. */
protected bullRushCharge(caster: Unit, cell: Point): {
    dir: Cube;
    foe: Unit;
    /** Charge segments: run `path`, then hit `foe` and push it along `push`. */
    legs: {
        path: Point[];
        foe: Unit;
        push: {
            path: Point[];
            blocked: boolean;
        };
    }[];
} | null {
    if (hexDist(caster, cell) > BULL_RUSH_RANGE)
        return null;
    const occ = new Map(this.occ());
    const target = occ.get(key(cell.x, cell.y));
    if (!target || !target.alive || target.side === caster.side)
        return null;
    const line = hexLine(caster, cell).slice(1);
    const legs: {
        path: Point[];
        foe: Unit;
        push: {
            path: Point[];
            blocked: boolean;
        };
    }[] = [];
    let run: Point[] = [];
    let at: Point = { x: caster.x, y: caster.y };
    const place = (u: Unit, to: Point) => {
        for (const c of footprint(u))
            if (occ.get(key(c.x, c.y)) === u)
                occ.delete(key(c.x, c.y));
        for (const c of footprint({ ...u, x: to.x, y: to.y }))
            occ.set(key(c.x, c.y), u);
    };
    for (let i = 0; i < line.length; i++) {
        const pt = line[i]!;
        if (this.chargeTerrainBlocked(pt.x, pt.y))
            return null;
        const body = footprint({ ...caster, ...pt });
        if (caster.classId === "bigBlueCalf" && body.some((c) => this.chargeTerrainBlocked(c.x, c.y)))
            return null;
        const who = caster.classId === "bigBlueCalf"
            ? body.map((c) => occ.get(key(c.x, c.y))).find((u) => u != null && u.id !== caster.id)
            : occ.get(key(pt.x, pt.y));
        if (!who || who.id === caster.id) {
            if (caster.classId === "bigBlueCalf" && footprint({ ...caster, ...pt }).some((c) => this.chargeTerrainBlocked(c.x, c.y) || (occ.get(key(c.x, c.y)) != null && occ.get(key(c.x, c.y))!.id !== caster.id)))
                return null;
            run.push(pt);
            at = pt;
            continue;
        }
        if (!who.alive || who.side === caster.side)
            return null;
        const dir = axisDir(at, pt);
        if (!dir)
            return null;
        // The charger's landing hex for this leg (and only that one), so pushes never land on it.
        for (const [k, u] of occ)
            if (u === caster)
                occ.delete(k);
        for (const c of footprint({ ...caster, ...at }))
            occ.set(key(c.x, c.y), caster);
        if (who.id !== target.id) {
            // In the way: shove it aside, off the rest of the line, then keep charging.
            const di = CUBE_DIRS.findIndex((d) => d.q === dir.q && d.r === dir.r && d.s === dir.s);
            const ahead = new Set(line.slice(i).map((c) => key(c.x, c.y)));
            let side: {
                path: Point[];
                blocked: boolean;
            } | null = null;
            for (const s of [CUBE_DIRS[(di + 1) % 6]!, CUBE_DIRS[(di + 5) % 6]!]) {
                const push = this.bullRushPush(who, s, occ);
                const land = push.path[push.path.length - 1];
                if (!land)
                    continue;
                const clear = footprint({ ...who, x: land.x, y: land.y }).every((c) => !ahead.has(key(c.x, c.y)));
                if (clear) {
                    side = push;
                    break;
                }
            }
            if (side) {
                legs.push({ path: run, foe: who, push: side });
                place(who, side.path[side.path.length - 1]!);
                run = [];
                i--; // re-check this hex, now empty
                continue;
            }
        }
        // The aimed enemy, or one that can't be moved aside: hit it and push it forward.
        legs.push({ path: run, foe: who, push: this.bullRushPush(who, dir, occ) });
        return { dir, foe: who, legs };
    }
    return null;
}

/** A Bull Rush push, precomputed before anything moves: who, where it ends up, and
 * whether it slammed into something short of its full distance (impact damage). */
protected bullRushPush(foe: Unit, dir: Cube, occ: Map<string, Unit>): {
    path: Point[];
    blocked: boolean;
} {
    const dist = this.bullRushPushDistance(foe);
    const knock = axisWalk({ x: foe.x, y: foe.y }, dir, this.cols, this.rows, dist, (pt) => !this.bullRushPushFits(foe, pt, occ));
    return { path: knock.path, blocked: knock.path.length < dist };
}

/** Bull Rush push distance: a body-type creature (any multi-hex footprint) moves 1 hex, a
 * normal one-hex creature 2. */
protected bullRushPushDistance(foe: Unit): number {
    return foe.footprintOffsets && foe.footprintOffsets.length > 1 ? 1 : 2;
}

/** Whether `foe` can be pushed so its anchor lands on `to`: its front row must be in
 * bounds on passable ground, and no cell of its whole body may overlap any other unit or
 * that unit's target zone (`occ` already has the charger at its landing hex). */
protected bullRushPushFits(foe: Unit, to: Point, occ: Map<string, Unit>): boolean {
    const placed = { ...foe, x: to.x, y: to.y };
    for (const c of footprintFrontRow(placed))
        if (this.chargeTerrainBlocked(c.x, c.y))
            return false;
    for (const c of footprint(placed)) {
        if (!inBounds(c.x, c.y, this.cols, this.rows))
            continue;
        const who = occ.get(key(c.x, c.y));
        if (who && who.id !== foe.id)
            return false;
    }
    return true;
}

/** Terrain stops a charge. */
protected chargeTerrainBlocked(x: number, y: number): boolean {
    return !inBounds(x, y, this.cols, this.rows) || !hexDef(this.tiles, this.cols, x, y, this.decorOverlay).passable;
}

/** One of each familiar tier at a time per caster — a conjurer re-casting a tier it
 * already has out just replaces nothing and clutters the field, so every summonFamiliarX
 * entry point (start and cast, both checked for the same reason spellAimValid AND
 * castX both validate range) blocks while a living familiar of that exact class still
 * carries this caster's id as its summonerId. Tiers stack freely with each other — this is
 * a per-tier cap, not "one familiar total". */
protected hasFamiliarOut(caster: Unit, classId: ClassId): boolean {
    return this.units.some((u) => u.alive && u.summonerId === caster.id && u.classId === classId);
}

/** `kind`'s remaining casts for `u` this battle: a familiar casting its OWN spell (see
 * FAMILIAR_SPELL) draws from its own spellCharges (set at summon time, never a slot-table
 * tier — see familiarSpellCharges); every other caster (including a familiar's other
 * actions, which is a no-op since they have none) uses the normal tier-slot pool. */
protected familiarSpellRemaining(u: Unit, kind: SpellKind): number {
    return FAMILIAR_SPELL[u.classId] === kind ? (u.spellCharges ?? 0) : this.tierRemaining(u, kind);
}

/** Spends one cast of `kind` for `u`: its own spellCharges if `kind` is that familiar's own
 * spell (see FAMILIAR_SPELL), otherwise the normal tier-slot pool — has to agree with
 * familiarSpellRemaining above on which pool a given (unit, kind) pair actually draws from. */
protected spendFamiliarOrTier(u: Unit, kind: SpellKind): void {
    if (FAMILIAR_SPELL[u.classId] === kind)
        u.spellCharges = Math.max(0, (u.spellCharges ?? 1) - 1);
    else
        this.spendTier(u, kind);
}

protected sweepTiles(u: Unit): Point[] {
    return hexAreaTiles({ x: u.x, y: u.y }, SWEEP.radius, this.cols, this.rows).filter((t) => t.x !== u.x || t.y !== u.y);
}

protected spellAimError(caster: Unit, cell: Point): string {
    const kind = this.spellKind;
    if (!kind)
        return "Nenhuma habilidade está mirando agora.";
    if (kind === "magicMissile" || kind === "magicMissileV2") {
        return this.magicMissileTargetError(caster, cell) ?? "Esse alvo não pode ser atingido por esta habilidade.";
    }
    if (kind === "cureMinor" || kind === "cureWounds" || kind === "cureLight") {
        const range = CURES[kind].range;
        const who = this.occ().get(key(cell.x, cell.y));
        if (manhattan(caster, cell) > range)
            return `Alvo fora de alcance (máximo ${range} hexes).`;
        if (!who)
            return "Escolha uma aliada na casa selecionada.";
        if (!this.debugFreeCast && who.side !== "player")
            return "Essa cura só pode ser usada em uma aliada.";
        if (!this.debugFreeCast && who.hp >= who.maxHp)
            return "Essa aliada está com a vida cheia.";
        return "Essa casa não atende aos requisitos desta cura.";
    }
    if (kind === "cureDisease") {
        const who = this.occ().get(key(cell.x, cell.y));
        if (manhattan(caster, cell) > CURE_DISEASE.range)
            return `Alvo fora de alcance (máximo ${CURE_DISEASE.range} hexes).`;
        if (!who)
            return "Escolha uma aliada na casa selecionada.";
        if (!this.debugFreeCast && who.side !== "player")
            return "A cura de doença só pode ser usada em uma aliada.";
        if (!this.debugFreeCast && !who.diseased && !who.poisoned)
            return "Essa aliada não está doente nem envenenada.";
        return "Essa casa não atende aos requisitos desta habilidade.";
    }
    const target = this.occ().get(key(cell.x, cell.y));
    const targetRequired = ["longShot", "bloodyShot", "lightning", "lightningTier3", "shock", "phantasmalForce", "multiShot", "doubleStrike", "trip", "lifeDrain", "executionerStrike", "shieldBash", "provoke"].includes(kind);
    if (targetRequired) {
        if (!target)
            return "Não há unidade na casa selecionada.";
        if (this.unitHidden(target))
            return "Escolha um inimigo visível.";
        if (target.dialog)
            return "Personagens de conversa não podem ser alvos de ataque.";
        if (target.side === caster.side)
            return "Essa habilidade não pode mirar em uma aliada.";
    }
    const distance = manhattan(caster, cell);
    if (kind === "longShot" || kind === "bloodyShot" || kind === "multiShot") {
        const max = kind === "longShot" ? this.longMax(caster) : kind === "bloodyShot" ? BLOODY_SHOT.range : MULTI_SHOT.range;
        if (distance < caster.minRange)
            return `Alvo perto demais (alcance mínimo ${caster.minRange} hexes).`;
        if (distance > max)
            return `Alvo fora de alcance (máximo ${max} hexes).`;
        if (!clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay))
            return this.shotBlockedTip(caster, cell, "arrow");
    }
    if (kind === "lightning" || kind === "lightningTier3" || kind === "shock" || kind === "phantasmalForce") {
        const range = kind === "lightning" ? LIGHTNING.range : kind === "lightningTier3" ? LIGHTNING_T3.range : kind === "shock" ? SHOCK.range : PHANTASMAL_FORCE.range;
        if (distance > range)
            return `Alvo fora de alcance (máximo ${range} hexes).`;
        if (kind === "phantasmalForce" && !clearShot(caster, cell, this.tiles, this.cols, "bolt", this.decorOverlay))
            return this.shotBlockedTip(caster, cell, "bolt");
    }
    if (kind === "fireball" || kind === "causticVenom" || kind === "divineBolt" || kind === "minorVenom" || kind === "webOfDreams") {
        const range = kind === "fireball" ? FIREBALL.range : kind === "causticVenom" ? CAUSTIC_VENOM.range : kind === "divineBolt" ? DIVINE_BOLT.range : kind === "minorVenom" ? MINOR_VENOM.range : WEB_OF_DREAMS.range;
        if (distance > range)
            return `Casa fora de alcance (máximo ${range} hexes).`;
        if (!clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay))
            return this.shotBlockedTip(caster, fireballOrigin(cell, this.cols, this.rows), "bolt");
    }
    if (kind === "iceStorm") {
        const range = iceStormPower(caster.level).range;
        if (distance > range)
            return `Casa fora de alcance (máximo ${range} hexes).`;
        if (!clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay))
            return this.shotBlockedTip(caster, fireballOrigin(cell, this.cols, this.rows), "bolt");
    }
    if (kind === "provoke") {
        const range = provokePower(caster.level).range;
        if (hexDist(caster, cell) > range)
            return `Alvo fora de alcance (máximo ${range} hexes).`;
    }
    if (kind === "cleave" || kind === "shoulderSmash")
        return "Escolha um hex vizinho ao personagem.";
    if (kind === "sweep")
        return `Escolha uma casa dentro do raio ${SWEEP.radius}.`;
    if (kind === "piercing" || kind === "piercingThrust" || kind === "burningHands" || kind === "poisonBreath" || kind === "divineWrath" || kind === "stampede") {
        return "Escolha uma linha reta válida dentro do alcance da habilidade.";
    }
    if (kind === "bullRush")
        return "Não há um caminho livre com espaço para concluir a investida nesse alvo.";
    if (kind === "summonFamiliar" || kind === "summonFamiliar2" || kind === "summonFamiliar3" || kind === "summonFamiliar4" || kind === "summonZombieDog") {
        const range = kind === "summonZombieDog" ? SUMMON_ZOMBIE_DOG.range : kind === "summonFamiliar4" ? SUMMON_FAMILIAR4.range : kind === "summonFamiliar3" ? SUMMON_FAMILIAR3.range : kind === "summonFamiliar2" ? SUMMON_FAMILIAR2.range : SUMMON_FAMILIAR.range;
        if (distance > range)
            return `Ponto de invocação fora de alcance (máximo ${range} hexes).`;
        if (target)
            return "O ponto de invocação está ocupado.";
        if (!inBounds(cell.x, cell.y, this.cols, this.rows))
            return "O ponto de invocação fica fora do mapa.";
        if (!this.hexAt(cell.x, cell.y).passable)
            return "O terreno bloqueia a invocação.";
        return "Não há espaço livre para essa criatura nesse ponto.";
    }
    if (["doubleStrike", "trip", "lifeDrain", "executionerStrike", "shieldBash"].includes(kind)) {
        return "Esse inimigo não está ao alcance ou não pode ser atingido daqui.";
    }
    return "Esta casa não atende aos requisitos de alvo da habilidade.";
}

/** Sweep / Shoulder Smash: shoves `foe` one hex further away from `att`, silently doing
 * nothing if that hex is off the board, impassable, or already occupied — a blocked shove
 * just fails, it never displaces someone else instead. Always one hex (SWEEP.knockback),
 * even when the target is two hexes out in Sweep's radius-2 area. */
protected knockBack(att: Unit, foe: Unit): void {
    const neighbors = hexNeighbors(foe.x, foe.y);
    let dest: Point | null = null;
    let best = manhattan(att, foe);
    for (const n of neighbors) {
        if (!inBounds(n.x, n.y, this.cols, this.rows))
            continue;
        const d = manhattan(att, n);
        if (d > best) {
            best = d;
            dest = n;
        }
    }
    if (!dest)
        return;
    if (!this.hexAt(dest.x, dest.y).passable)
        return;
    if (this.units.some((u) => u.alive && occupies(u, dest.x, dest.y)))
        return;
    foe.x = dest.x;
    foe.y = dest.y;
    foe.drawX = dest.x;
    foe.drawY = dest.y;
    { }
}

/** Board cells Provoke reaches around its aim — only the aim itself at radius 0. */
protected provokeArea(cell: Point, level: number): Point[] {
    const { radius } = provokePower(level);
    const out: Point[] = [];
    for (let y = cell.y - radius; y <= cell.y + radius; y++) {
        for (let x = cell.x - radius - 1; x <= cell.x + radius + 1; x++) {
            if (inBounds(x, y, this.cols, this.rows) && hexDist(cell, { x, y }) <= radius)
                out.push({ x, y });
        }
    }
    return out;
}

protected queueMinorVenom(unit: Unit, origin: Point): void {
    const tiles = hexAreaTiles(origin, MINOR_VENOM.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
        const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (u && !ids.includes(u.id))
            ids.push(u.id);
    }
    const center = this.units.find((x) => x.alive && occupies(x, origin.x, origin.y));
    this.queue.push({
        type: "spell",
        att: unit.id,
        tiles,
        ids,
        dice: MINOR_VENOM.splashDice,
        faces: MINOR_VENOM.splashFaces,
        bonus: MINOR_VENOM.splashBonus,
        centerId: center?.id,
        centerDice: MINOR_VENOM.centerDice,
        centerFaces: MINOR_VENOM.centerFaces,
        centerBonus: MINOR_VENOM.centerBonus,
        poison: true,
        label: MINOR_VENOM.name,
        spellMul: MINOR_VENOM.splashMul,
        centerMul: MINOR_VENOM.centerMul,
        spellKind: "minorVenom",
        projectileTo: origin,
    });
}

/** Damage for one spell hit on one target.
 *
 * A spell is a boosted version of the hit the caster could have made instead: same power,
 * same protection, with the caster's own stat weighted by the spell's multiplier and the
 * spell's dice standing in for the weapon. Every multiplier is above 1 and the result is
 * floored at a plain attack, so a cast can never come out worse than simply swinging —
 * which it could before, because spells ignored the caster's stat entirely and a mage's
 * MAG only ever improved their basic attack.
 *
 * The multiplier weights the power term alone. Applied to the whole total it would scale
 * terrain protection with it. Elemental resistance is applied after spell damage. */
protected spellDamage(att: Unit, foe: Unit, mul: number, roll: number): number {
    att = this.affinityUnit(att);
    foe = this.affinityUnit(foe);
    const attTile = this.hexAt(att.x, att.y);
    const defTile = this.hexAt(foe.x, foe.y);
    const prot = 0;
    const spell = Math.floor(powerOf(att) * mul) + roll + attTile.atk - prot - (defTile.cover ?? 0);
    const plain = powerOf(att) + weaponRoll(att.weaponId, att.weaponEnh, this.rng) + attTile.atk - prot - defTile.def;
    return Math.max(1, Math.floor(Math.max(spell, plain)));
}

/** A wardog's bite (20%) or a zombie's hit (30%) can inflict disease on a surviving target. */
protected maybeInflictDisease(actor: Unit, target: Unit): void {
    const chance = actor.classId === "wardog" || actor.classId === "wardog2" ? DISEASE.biteChance : (actor.classId === "zombie" || actor.classId === "zombie2" || actor.classId === "undeadOx" || actor.classId === "plagueBearingCattle") ? DISEASE.zombieChance : 0;
    if (chance <= 0 || !target.alive || target.diseased)
        return;
    if (this.rng() >= chance)
        return;
    target.diseased = true;
    target.diseaseBase = { atk: target.atk, mag: target.mag, def: target.def, dex: target.dex, mov: target.mov };
    const pen = (n: number) => Math.round(n * (1 - DISEASE.statPenalty));
    target.atk = pen(target.atk);
    target.mag = pen(target.mag);
    target.def = pen(target.def);
    target.dex = pen(target.dex);
    target.mov = Math.max(1, pen(target.mov));
    this.tip = `${target.name} não se sente muito bem.`;
}

protected gainExp(attacker: Unit, targetLevel: number, amount: number, multiplier = 1, isKill = false): void {
    if (amount <= 0 || attacker.side !== "player" || !attacker.alive)
        return;
    const killMul = isKill ? SpellRules.KILL_EXP_BONUS_MUL : 1;
    const gained = Math.round(expForHit(attacker.level, targetLevel) * multiplier * killMul);
    this.grantExp(attacker, gained);
}

/** A successful counter always earns exactly 1 XP — flat, no level-gap scaling, no kill
 * bonus, no stacking with anything. The counter still deals its full real damage; this
 * only caps what it's worth in experience, so a unit can't out-level by baiting hits and
 * countering instead of attacking. */
protected gainCounterExp(attacker: Unit): void {
    if (attacker.side !== "player" || !attacker.alive)
        return;
    this.grantExp(attacker, 1);
}

/** Routes earned XP to whoever should actually keep it: a summoned familiar (summonerId
 * set) redirects FAMILIAR_XP_SHARE of its own gain to its summoning conjurer instead of
 * keeping any itself; every other unit keeps 100% of its own gain, unchanged from before. */
protected grantExp(attacker: Unit, amount: number): void {
    if (attacker.summonerId) {
        const conjurer = this.units.find((u) => u.id === attacker.summonerId);
        if (!conjurer || conjurer.side !== "player" || !conjurer.alive || conjurer.level >= MAX_LEVEL)
            return;
        this.addExp(conjurer, Math.floor(amount * SpellRules.FAMILIAR_XP_SHARE));
        return;
    }
    if (attacker.level >= MAX_LEVEL)
        return;
    this.addExp(attacker, amount);
}

protected addExp(attacker: Unit, gained: number): void {
    if (gained <= 0)
        return;
    attacker.xp += gained;
    while (attacker.xp >= expToLevel(attacker.level) && attacker.level < MAX_LEVEL) {
        attacker.xp -= expToLevel(attacker.level);
        this.levelUpUnit(attacker);
    }
    if (attacker.level >= MAX_LEVEL)
        attacker.xp = 0;
}

/** Bumps a unit by one level: stat growth, the level's HP gain added to current HP (not a
 * full heal), and any newly-unlocked tier uses granted right away. Spent charges stay
 * spent — only the extra slots this level adds land in the remaining pool. */
protected levelUpUnit(u: Unit): void {
    const from = u.level;
    const to = from + 1;
    const before = statsFor(u.classId, from);
    const after = statsFor(u.classId, to);
    u.level = to;
    u.maxHp = after.hp + (u.statPointAllocation.hp ?? 0);
    u.atk = after.atk + (u.statPointAllocation.atk ?? 0);
    u.mag = after.mag + (u.statPointAllocation.mag ?? 0);
    u.def = after.def + (u.statPointAllocation.def ?? 0);
    u.dex = after.dex + (u.statPointAllocation.dex ?? 0);
    u.hp = Math.min(u.maxHp, u.hp + (after.hp - before.hp));
    this.reapplyGear(u);
    const nextSpells = { ...u.spells };
    const gains = spellUseGains(u.classId, from, to);
    for (const g of gains) {
        const have = Number.isFinite(nextSpells[g.key]) ? nextSpells[g.key] : 0;
        const cap = tierUses(u.classId, g.tier, to);
        nextSpells[g.key] = Math.min(cap, have + g.gain);
    }
    u.spells = nextSpells;
    const extra = formatSpellUseGains(gains);
    this.tip = extra ? `${u.name} subiu para o nível ${to} · ${extra}` : `${u.name} subiu para o nível ${to}!`;
    { }
    if (extra) {
        { }
    }
    { }
    { }
}

protected trainWeapon(unit: Unit, type: import("../ember/types.ts").WeaponType | undefined, enemyLevel: number): void {
    if (!type || unit.side !== "player" || unit.summoned || !weaponTypesForClass(unit.classId).includes(type))
        return;
    const amount = this.trainingGainForLevel(unit.level, enemyLevel);
    if (amount === null)
        return;
    const id = `${type}Weapon` as const;
    const current = this.heroSkills[unit.name]?.[id] ?? 0;
    const gained = rollWeaponSkillGain(current, this.rng, amount ?? SKILL_GAIN);
    if (gained === null)
        return;
    this.heroSkills[unit.name] = { ...this.heroSkills[unit.name], [id]: gained };
    unit.weaponSkills = { ...unit.weaponSkills, [type]: gained };
    this.logSkillGain(unit, id, current, gained);
}

/** A familiar's elemental magic practises its summoner's matching resistance. */
protected trainElementUse(caster: Unit, element: import("../ember/types.ts").ResistanceElement, enemyLevel: number): void {
    const learner = caster.summoned
        ? this.units.find(unit => unit.id === caster.summonerId && unit.alive && unit.side === caster.side)
        : caster;
    if (learner)
        this.trainResistance(learner, element, enemyLevel);
}

protected trainHealing(actor: Unit): void {
    if (actor.side !== "player" || actor.summoned)
        return;
    const current = skillValue(this.heroSkills, actor.name, "healing");
    const next = rollSkillGain(current, this.rng);
    if (next === null)
        return;
    this.heroSkills[actor.name] = { ...this.heroSkills[actor.name], healing: next };
    actor.healingSkill = next;
    this.logSkillGain(actor, "healing", current, next);
}

protected trainResistance(unit: Unit, element: import("../ember/types.ts").ResistanceElement, enemyLevel?: number): void {
    if (unit.side !== "player" || unit.summoned)
        return;
    const amount = enemyLevel == null ? undefined : this.trainingGainForLevel(unit.level, enemyLevel);
    if (amount === null)
        return;
    const id = `${element}Resistance` as const;
    const current = this.heroSkills[unit.name]?.[id] ?? 0;
    const gained = rollSkillGain(current, this.rng, amount ?? SKILL_GAIN);
    if (gained === null)
        return;
    this.heroSkills[unit.name] = { ...this.heroSkills[unit.name], [id]: gained };
    unit.resistances = { ...unit.resistances, [element]: Number(((unit.resistances?.[element] ?? 0) + gained - current).toFixed(2)) };
    if (element === "poison")
        unit.poisonResist = gained;
    this.logSkillGain(unit, id, current, gained);
}

protected healingPower(actor: Unit, base: number): number {
    return healingAmount(base, skillValue(this.heroSkills, actor.name, "healing"));
}

protected trainingGainForLevel(unitLevel: number, enemyLevel: number): number | null | undefined {
    const difference = unitLevel - enemyLevel;
    if (difference === 10)
        return null;
    if (difference === 5)
        return 0.05;
    return undefined;
}

/** Every skill point a hero earns in battle is announced in the combat log. */
protected logSkillGain(unit: Unit, id: SkillId, before: number, after: number): void {
    const number = (n: number) => n.toLocaleString("pt-BR", { maximumFractionDigits: 2 });
    { }
}

protected adjacentAllies(u: Unit): Unit[] {
    return this.units.filter(ally => ally.id !== u.id && ally.alive && ally.side === "player" && !ally.summoned
        && footprint(u).some(a => footprint(ally).some(b => hexDist(a, b) === 1)));
}

protected affinityUnit(u: Unit): Unit {
    if (u.side !== "player" || u.summoned || !u.alive)
        return u;
    const bonus = Math.max(0, ...this.adjacentAllies(u).map(ally => affinityBonus(affinityScore(this.affinityScores, u.name, ally.name))));
    return bonus ? { ...u, atk: u.atk * (1 + bonus), mag: u.mag * (1 + bonus), def: u.def * (1 + bonus), dex: u.dex * (1 + bonus) } : u;
}

protected adjustAffinity(a: Unit, b: Unit, delta: number): void {
    if (a.id === b.id || a.side !== "player" || b.side !== "player" || a.summoned || b.summoned)
        return;
    this.adjustHeroAffinity(a.name, b.name, delta, false);
}

protected adjustHeroAffinity(a: string, b: string, delta: number, dialogue = true): void {
    if (a === b || delta === 0 || !AFFINITY_HEROES.includes(a as AffinityHero) || !AFFINITY_HEROES.includes(b as AffinityHero))
        return;
    if (dialogue && delta > 0 && (a === this.partyLeader || b === this.partyLeader)) {
        delta = Math.round(delta * (2 - affinityScore(this.affinityScores, a, b) / 100) * 100) / 100;
    }
    this.affinityScores = changeAffinity(this.affinityScores, a as AffinityHero, b as AffinityHero, delta);
    { }
}

/** Recomputes whatever worn gear contributes, after a slot changed mid-battle. Every core
 * stat gearStatBonus returns is applied here, kept in sync with the matching lines in
 * spawnUnit — folded into its own step rather than the equip methods so both entry points
 * stay in sync. */
protected reapplyGear(u: Unit): void {
    const base = statsFor(u.classId, u.level);
    const bonus = gearStatBonus(Object.values(u.gear));
    if (u.side === "player" && !u.summoned)
        u.weaponSkills = trainedWeaponSkills(this.heroSkills, u.name, u.classId);
    // Re-applied fresh every time rather than mutated once (unlike crippled) — see
    // Unit.hungerPenaltyPct's own doc comment.
    const hungerKeep = 1 - u.hungerPenaltyPct;
    const diseaseKeep = u.diseased ? 1 - DISEASE.statPenalty : 1;
    u.maxHp = Math.round((base.hp + (u.statPointAllocation.hp ?? 0) + bonus.hp) * hungerKeep);
    const diseaseBase = {
        atk: Math.round((base.atk + (u.statPointAllocation.atk ?? 0) + bonus.atk) * hungerKeep),
        mag: Math.round((base.mag + (u.statPointAllocation.mag ?? 0) + bonus.mag) * hungerKeep),
        def: Math.round((base.def + (u.statPointAllocation.def ?? 0) + bonus.def) * hungerKeep),
        dex: Math.round((base.dex + (u.statPointAllocation.dex ?? 0) + bonus.dex) * hungerKeep),
        mov: base.mov + bonus.mov,
    };
    u.atk = Math.round(diseaseBase.atk * diseaseKeep);
    u.mag = Math.round(diseaseBase.mag * diseaseKeep);
    u.def = Math.round(diseaseBase.def * diseaseKeep);
    u.dex = Math.round(diseaseBase.dex * diseaseKeep);
    u.resistances = sumResistances(base.resistances, bonus.resistances, u.side === "player" && !u.summoned ? skillResistances(this.heroSkills, u.name) : undefined);
    u.mov = u.diseased ? Math.max(1, Math.round(diseaseBase.mov * diseaseKeep)) : diseaseBase.mov;
    u.diseaseBase = u.diseased ? diseaseBase : null;
    u.hp = Math.min(u.maxHp, u.hp);
}

/** Bleeding hurts on every action; walking only opens the wound once per own turn. */
protected applyBleedingActionDamage(u: Unit | undefined, isMove: boolean): boolean {
    if (!u || !u.alive || !u.bleeding || (isMove && u.bleedMovedThisTurn))
        return !!u?.alive;
    if (isMove)
        u.bleedMovedThisTurn = true;
    const dmg = rollDice(1, 8, 0, this.rng);
    u.hp = Math.max(0, u.hp - dmg);
    u.flash = 1;
    u.hitAt = this.time;
    this.spawnHit(u, dmg, false);
    this.tip = `Sangramento · 1D8 dano`;
    { }
    { }
    if (u.hp <= 0) {
        this.markDead(u);
        return false;
    }
    return true;
}

/** Lightning echo + standing-hazard damage, applied once when this unit's own turn begins. */
protected startOfTurnEffects(u: Unit): void {
    if (!u.alive)
        return;
    if (u.shock) {
        const echo = u.shock;
        u.shock = null;
        const baseDamage = rollDice(echo.dice, echo.faces, echo.bonus, this.rng);
        const dmg = Math.floor(elementalDamage(baseDamage, u.resistances?.lightning ?? 0, echo.mag ?? 0));
        u.hp = Math.max(0, u.hp - dmg);
        u.flash = 1;
        u.hitAt = this.time;
        this.spawnHit(u, dmg, false);
        this.tip = `Relâmpago · ${diceFormula(echo.dice, echo.faces, echo.bonus)}`;
        { }
        this.trainResistance(u, "lightning");
        { }
        if (u.hp <= 0) {
            this.markDead(u);
        }
    }
    if (u.alive && u.poisoned) {
        const tier = POISON_TIERS[u.poisonTier ?? "lesser"];
        const dmg = poisonTickDamage(rollDice(tier.dice, tier.faces, 0, this.rng), effectivePoisonResistance(u.resistances?.poison ?? 0, u.poisonTier ?? "lesser", u.poisonMag ?? 0));
        u.hp = Math.max(0, u.hp - dmg);
        u.flash = 1;
        u.hitAt = this.time;
        this.spawnHit(u, dmg, false);
        this.tip = `${tier.name} · ${poisonDice(u.poisonTier ?? "lesser")} dano`;
        { }
        { }
        // Suffering the poison is use too — every tick is a Poison Resistance check.
        if (u.side === "player" && !u.summoned)
            this.trainResistance(u, "poison");
        if (u.hp <= 0) {
            this.markDead(u);
        }
    }
    if (u.alive) {
        for (const zone of this.iceStormZones) {
            const isStandingInZone = [...zone.cells].some((cell) => {
                const comma = cell.indexOf(",");
                return occupies(u, Number(cell.slice(0, comma)), Number(cell.slice(comma + 1)));
            });
            if (!u.alive || !isStandingInZone)
                continue;
            const base = Math.floor(Math.floor(zone.casterMag / 2) * zone.damageMul) + rollDice(zone.damageDice, zone.damageFaces, 0, this.rng);
            const dmg = Math.floor(elementalDamage(base, u.resistances?.ice ?? 0, zone.casterMag));
            u.hp = Math.max(0, u.hp - dmg);
            u.flash = 1;
            u.hitAt = this.time;
            if (dmg > 0)
                this.spawnHit(u, dmg, false);
            this.tip = `${ICE_STORM.name} · ${diceFormula(zone.damageDice, zone.damageFaces, 0)}`;
            { }
            { }
            this.trainResistance(u, "ice", zone.casterLevel);
            const caster = this.units.find((candidate) => candidate.id === zone.casterId);
            if (caster && caster.side === "player")
                this.trainElementUse(caster, "ice", u.level);
            if (u.hp <= 0)
                this.markDead(u);
            else { }
        }
    }
    // Second Wind (Paladin tier 3): passive, never a hotbar cast — the first time this
    // paladin's own turn opens at or below the "badly wounded" line with a tier-3 use still
    // banked, it heals itself and spends the use. classId-gated explicitly, since tierUses
    // hands out tier-3 slots to every class, not just paladin.
    if (u.alive && u.classId === "paladin" && u.hp / u.maxHp <= SECOND_WIND.badlyWoundedPct && this.tierRemaining(u, "secondWind") > 0) {
        this.spendTier(u, "secondWind");
        const heal = Math.min(u.maxHp - u.hp, Math.floor(secondWindPct(u.level) * u.dex));
        if (heal > 0) {
            u.hp += heal;
            u.flash = 1;
            { }
            this.tip = `${SECOND_WIND.name} · +${heal} HP`;
            { }
            { }
        }
    }
    if (u.alive)
        this.applyTileHazard(u, { x: u.x, y: u.y });
    this.evaluateEnd();
}

protected applyTileHazard(unit: Unit, cell: Point): void {
    const terr = this.hexAt(cell.x, cell.y);
    if (!terr.hazardDice || !unit.alive)
        return;
    const faces = terr.hazardFaces ?? 8;
    let dmg = 0;
    for (let i = 0; i < terr.hazardDice; i++)
        dmg += 1 + Math.floor(this.rng() * faces);
    const element = terr.id === "flame" ? "fire" : terr.id === "ember" ? "ember" : undefined;
    if (element)
        dmg = Math.floor(elementalDamage(dmg, unit.resistances?.[element] ?? 0));
    unit.hp = Math.max(0, unit.hp - dmg);
    unit.flash = 1;
    unit.hitAt = this.time;
    this.spawnHit(unit, dmg, false);
    { }
    if (element)
        this.trainResistance(unit, element);
    { }
    if (unit.hp <= 0) {
        this.markDead(unit);
        this.onNextIdle = null;
    }
}

protected runAiFor(next: Unit): void {
    // Under fog, a foe that has not seen the party yet holds its ground. Without this
    // the whole point of fog is lost from the other side: the party creeps through a
    // dark corridor while every enemy on the level walks straight at them, having been
    // told where they are by a turn loop rather than by seeing them.
    if (!this.wakeIfSeesParty(next)) {
        next.moved = true;
        next.acted = true;
        return;
    }
    this.smashBarricades(next);
    const reach = computeReachable(this.effectiveUnitForReach(next), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay, this.elevations);
    // Every move this function queues has to be reconstructed off this unpruned pass, not
    // `reach` above — same reasoning as commitMove's walkReach: `reach` deletes any cell along
    // the way that isn't itself a legal place to stop (an ally standing there, or — the one
    // that actually bites here — a big multi-hex footprint that can't fit stopped on that cell
    // even though it can walk through it), leaving a dangling parent reference that silently
    // truncates reconstructPath to a single point short of the real destination. A size-1
    // walker rarely has any such cell on its route so this went unnoticed; a size-4 footprint
    // (Golem, Birolho, Horror, Asherah, Troll) has one on almost every route, which is why
    // only they ever looked "stuck" — the AI had already picked a real, reachable destination,
    // it just never got a real path to it.
    const walkReach = computeReachable(this.effectiveUnitForReach(next), this.tiles, this.cols, this.rows, this.units, false, this.decorOverlay, this.elevations);
    // Enmity (enmity.ts): once heroes have drawn this enemy's attention, it goes after whoever
    // tops its enmity table — attacking them if it can, otherwise walking toward them (FFXI
    // style), ignoring easier targets. With no enmity yet it keeps its usual targeting.
    const focus = this.enmityTarget(next);
    const players = focus ? [focus] : this.units.filter((u) => u.side === "player" && u.alive);
    // The Ox spends its own three per-battle tier-1 charges on warrior Bull Rush.
    if (next.classId === "bigBlueCalf" && !next.acted && this.tierRemaining(next, "bullRush") > 0) {
        const targets = players.flatMap((foe) => footprint(foe).map((cell) => ({ foe, cell })))
            .sort((a, b) => hexDist(next, a.cell) - hexDist(next, b.cell) || a.foe.hp - b.foe.hp);
        const target = targets.find(({ cell }) => this.bullRushCharge(next, cell) !== null);
        if (target) {
            this.castBullRush(next, target.cell);
            return;
        }
    }
    // Cultists retain their spell priority; Swamp Blue Calf enters only for its separate
    // Phantom System charge pool below.
    if ((next.classId === "cultist" || next.classId === "cultistV2" || next.classId === "stagWolf" || next.classId === "emberedWraith" || next.classId === "swampBlueCalf") && (next.spells.tier1 > 0 || next.spells.tier2 > 0 || next.shockCharges > 0 || (next.fantomForceCharges ?? 0) > 0)) {
        if (next.spells.tier2 > 0) {
            let bestBolt: {
                foe: Unit;
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const foe of players) {
                    if (manhattan(cell, foe) > LIGHTNING.range)
                        continue;
                    const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
                    if (!bestBolt || score > bestBolt.score)
                        bestBolt = { foe, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestBolt) {
                if (bestBolt.from.x !== next.x || bestBolt.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestBolt.from) });
                }
                this.spendTier(next, "lightning");
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: [{ x: bestBolt.foe.x, y: bestBolt.foe.y }],
                    ids: [bestBolt.foe.id],
                    dice: lightningDice(),
                    faces: LIGHTNING.faces,
                    bonus: LIGHTNING.bonus,
                    label: LIGHTNING.name,
                    echo: { dice: LIGHTNING.echoDice, faces: LIGHTNING.echoFaces, bonus: LIGHTNING.echoBonus },
                    spellMul: LIGHTNING.mul,
                    spellKind: "lightning",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
        if ((next.fantomForceCharges ?? 0) > 0) {
            let bestForce: {
                foe: Unit;
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const foe of players) {
                    if (manhattan(cell, foe) > FANTOM_FORCE.range)
                        continue;
                    if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay))
                        continue;
                    const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
                    if (!bestForce || score > bestForce.score)
                        bestForce = { foe, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestForce) {
                if (bestForce.from.x !== next.x || bestForce.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestForce.from) });
                }
                next.fantomForceCharges = Math.max(0, next.fantomForceCharges! - 1);
                const dice = fantomForceDice(next.level);
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: [{ x: bestForce.foe.x, y: bestForce.foe.y }],
                    ids: [bestForce.foe.id],
                    dice: dice.dice,
                    faces: dice.faces,
                    bonus: 0,
                    label: FANTOM_FORCE.name,
                    spellMul: 1,
                    spellKind: "fantomForce",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
        if (this.tryAiShock(next, reach, walkReach, players))
            return;
        if (next.spells.tier1 > 0) {
            let bestSpell: {
                foe: Unit;
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const foe of players) {
                    if (manhattan(cell, foe) > MAGIC_MISSILE.range)
                        continue;
                    if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay))
                        continue;
                    const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
                    if (!bestSpell || score > bestSpell.score)
                        bestSpell = { foe, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestSpell) {
                if (bestSpell.from.x !== next.x || bestSpell.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestSpell.from) });
                }
                this.spendTier(next, "magicMissile");
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: [{ x: bestSpell.foe.x, y: bestSpell.foe.y }],
                    ids: [bestSpell.foe.id],
                    dice: MAGIC_MISSILE.dice,
                    faces: MAGIC_MISSILE.faces,
                    bonus: MAGIC_MISSILE.bonus,
                    label: MAGIC_MISSILE.name,
                    spellMul: MAGIC_MISSILE.mul,
                    spellKind: "magicMissile",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
    }
    // Frost is the close-range fallback after Cultist ranged spell attempts.
    if (next.classId === "cultistV2" && (next.frostCharges ?? 0) > 0) {
        let best: {
            from: Point;
            target: Point;
            score: number;
        } | null = null;
        for (const from of [next])
            for (const foe of players) {
                const cells = this.frostTiles(from, foe, next.level);
                const hits = players.filter(p => cells.some(c => occupies(p, c.x, c.y))).length;
                if (!hits)
                    continue;
                const allies = this.units.filter(u => u.alive && u.side === next.side && u.id !== next.id && cells.some(c => occupies(u, c.x, c.y))).length;
                const score = hits * 10 - allies * 12 - hexDist(next, from);
                if (!best || score > best.score)
                    best = { from: { x: from.x, y: from.y }, target: { x: foe.x, y: foe.y }, score };
            }
        if (best && best.score > 0) {
            if (best.from.x !== next.x || best.from.y !== next.y)
                this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
            this.queueFrost(next, best.from, best.target);
            this.queue.push({ type: "delay", dur: .12 });
            return;
        }
    }
    // Undead Ox — its tier4 Veneno Menor (2 per battle), then plain melee.
    if ((next.classId === "undeadOx" || next.classId === "plagueBearingCattle") && this.tryAiMinorVenom(next, reach, walkReach, players))
        return;
    // Carnivorous Plant — more than one character in reach of her tendril swipe: swipe instead
    // of casting. Otherwise Veneno Cáustico, then Poison Breath, then Veneno Menor, then plain melee.
    if (next.classId === "carnivorousPlant") {
        if (this.tryAiPlantSwipe(next, reach, walkReach))
            return;
        if (this.tryAiPlantCausticVenom(next, reach, walkReach, players))
            return;
        if (this.tryAiPlantPoisonBreath(next, reach, walkReach))
            return;
        if (this.tryAiMinorVenom(next, reach, walkReach, players))
            return;
    }
    // Sapling — its 1 Poison Breath (same cone targeting as the Carnivorous Plant's), then melee.
    if (next.classId === "sapling" && this.tryAiPlantPoisonBreath(next, reach, walkReach))
        return;
    // Birolho (and Birolho2) — Relâmpago outranks Caustic Venom outranks Choque outranks Magic Missile.
    if ((next.classId === "birolho" || next.classId === "birolho2" || next.classId === "birolho3" || next.classId === "birolhoLegs" || next.classId === "birolhoLegs2") && (next.spells.tier1 > 0 || next.spells.tier2 > 0 || next.spells.tier4 > 0 || next.shockCharges > 0)) {
        if (next.spells.tier2 > 0) {
            let bestBolt: {
                foe: Unit;
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const foe of players) {
                    if (manhattan(cell, foe) > LIGHTNING.range)
                        continue;
                    const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
                    if (!bestBolt || score > bestBolt.score)
                        bestBolt = { foe, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestBolt) {
                if (bestBolt.from.x !== next.x || bestBolt.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestBolt.from) });
                }
                this.spendTier(next, "lightning");
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: [{ x: bestBolt.foe.x, y: bestBolt.foe.y }],
                    ids: [bestBolt.foe.id],
                    dice: lightningDice(),
                    faces: LIGHTNING.faces,
                    bonus: LIGHTNING.bonus,
                    label: LIGHTNING.name,
                    echo: { dice: LIGHTNING.echoDice, faces: LIGHTNING.echoFaces, bonus: LIGHTNING.echoBonus },
                    spellMul: LIGHTNING.mul,
                    spellKind: "lightning",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
        if (next.spells.tier4 > 0) {
            let bestVenom: {
                at: Point;
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const foe of players) {
                    if (manhattan(cell, foe) > CAUSTIC_VENOM.range)
                        continue;
                    if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay))
                        continue;
                    const splash = hexAreaTiles({ x: foe.x, y: foe.y }, CAUSTIC_VENOM.size, this.cols, this.rows);
                    let hits = 0;
                    let score = 0;
                    for (const t of splash) {
                        const hit = players.find((p) => p.x === t.x && p.y === t.y);
                        if (!hit)
                            continue;
                        hits += 1;
                        score += (hit.maxHp - hit.hp) + (hit.hp <= 8 ? 15 : 0);
                    }
                    if (hits === 0)
                        continue;
                    score += hits * 10;
                    if (!bestVenom || score > bestVenom.score)
                        bestVenom = { at: { x: foe.x, y: foe.y }, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestVenom) {
                if (bestVenom.from.x !== next.x || bestVenom.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestVenom.from) });
                }
                this.spendTier(next, "causticVenom");
                const tiles = hexAreaTiles(bestVenom.at, CAUSTIC_VENOM.size, this.cols, this.rows);
                const ids: string[] = [];
                for (const t of tiles) {
                    const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
                    if (u && !ids.includes(u.id))
                        ids.push(u.id);
                }
                const center = this.units.find((x) => x.alive && occupies(x, bestVenom.at.x, bestVenom.at.y));
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles,
                    ids,
                    dice: CAUSTIC_VENOM.splashDice,
                    faces: CAUSTIC_VENOM.splashFaces,
                    bonus: CAUSTIC_VENOM.splashBonus,
                    centerId: center?.id,
                    centerDice: CAUSTIC_VENOM.centerDice,
                    centerFaces: CAUSTIC_VENOM.centerFaces,
                    centerBonus: CAUSTIC_VENOM.centerBonus,
                    poison: true,
                    label: CAUSTIC_VENOM.name,
                    spellMul: CAUSTIC_VENOM.splashMul,
                    centerMul: CAUSTIC_VENOM.centerMul,
                    spellKind: "causticVenom",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
        if (this.tryAiShock(next, reach, walkReach, players))
            return;
        if (next.spells.tier1 > 0) {
            let bestBolt: {
                foe: Unit;
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const foe of players) {
                    if (manhattan(cell, foe) > MAGIC_MISSILE.range)
                        continue;
                    if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay))
                        continue;
                    const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
                    if (!bestBolt || score > bestBolt.score)
                        bestBolt = { foe, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestBolt) {
                if (bestBolt.from.x !== next.x || bestBolt.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestBolt.from) });
                }
                this.spendTier(next, "magicMissile");
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: [{ x: bestBolt.foe.x, y: bestBolt.foe.y }],
                    ids: [bestBolt.foe.id],
                    dice: MAGIC_MISSILE.dice,
                    faces: MAGIC_MISSILE.faces,
                    bonus: MAGIC_MISSILE.bonus,
                    label: MAGIC_MISSILE.name,
                    spellMul: MAGIC_MISSILE.mul,
                    spellKind: "magicMissile",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
    }
    // Rocco The Bird — per battle: 1 Choque, 2 Magic Missile, 3 Burning Beak (Burning Hands,
    // shown as "Burning Beak" when he casts it). Burning Beak outranks Magic Missile outranks
    // Choque; the cone is never aimed where it would also burn one of his own allies.
    if (next.classId === "roccoTheBird" && (next.spells.tier1 > 0 || next.spells.tier2 > 0 || next.shockCharges > 0)) {
        if (next.spells.tier2 > 0) {
            const power = burningHandsPower(next.level);
            let bestCone: {
                tiles: Point[];
                ids: string[];
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const dir of CUBE_DIRS) {
                    const tiles = coneSector(cell, dir, power.radius, this.cols, this.rows);
                    const ids: string[] = [];
                    let score = 0;
                    let burnsAlly = false;
                    for (const t of tiles) {
                        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
                        if (!who || who.id === next.id || ids.includes(who.id))
                            continue;
                        if (who.side !== "player")
                            burnsAlly = true;
                        ids.push(who.id);
                        score += 10 + (who.maxHp - who.hp) * 3 + (who.hp <= 8 ? 20 : 0);
                    }
                    if (burnsAlly || !ids.length)
                        continue;
                    if (!bestCone || score > bestCone.score)
                        bestCone = { tiles, ids, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestCone) {
                if (bestCone.from.x !== next.x || bestCone.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestCone.from) });
                }
                this.spendTier(next, "burningHands");
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: bestCone.tiles,
                    ids: bestCone.ids,
                    dice: power.dice,
                    faces: power.faces,
                    bonus: 0,
                    label: "Burning Beak",
                    spellMul: power.mul,
                    spellKind: "burningHands",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
        if (next.spells.tier1 > 0) {
            let bestSpell: {
                foe: Unit;
                from: Point;
                score: number;
            } | null = null;
            for (const cell of reach.values()) {
                for (const foe of players) {
                    if (manhattan(cell, foe) > MAGIC_MISSILE.range)
                        continue;
                    if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay))
                        continue;
                    const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
                    if (!bestSpell || score > bestSpell.score)
                        bestSpell = { foe, from: { x: cell.x, y: cell.y }, score };
                }
            }
            if (bestSpell) {
                if (bestSpell.from.x !== next.x || bestSpell.from.y !== next.y) {
                    this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestSpell.from) });
                }
                this.spendTier(next, "magicMissile");
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: [{ x: bestSpell.foe.x, y: bestSpell.foe.y }],
                    ids: [bestSpell.foe.id],
                    dice: MAGIC_MISSILE.dice,
                    faces: MAGIC_MISSILE.faces,
                    bonus: MAGIC_MISSILE.bonus,
                    label: MAGIC_MISSILE.name,
                    spellMul: MAGIC_MISSILE.mul,
                    spellKind: "magicMissile",
                });
                this.queue.push({ type: "delay", dur: 0.12 });
                return;
            }
        }
        if (this.tryAiShock(next, reach, walkReach, players))
            return;
    }
    // Any other enemy mage (a player-class mage spawned as a foe, promoted casters, etc.)
    // still gets Choque even if they don't share the cultist/birolho AI branches.
    if (next.side === "enemy" &&
        next.shockCharges > 0 &&
        next.classId !== "cultist" &&
        next.classId !== "cultistV2" &&
        next.classId !== "birolho" &&
        next.classId !== "birolho2" &&
        next.classId !== "birolho3" &&
        next.classId !== "birolhoLegs" &&
        next.classId !== "birolhoLegs2" &&
        this.tryAiShock(next, reach, walkReach, players)) {
        return;
    }
    // Brigand Crossbowman ("Besteiro Salteador"): 1 Multi Shot, then 2 Bloody Shot per battle
    // (tier3 / tier2 at spawn). Multi Shot spends its volley from the spot that reaches the most
    // foes (repeating targets when fewer stand in range); Bloody Shot picks one target the same
    // way the brigand's Long Shot does.
    if (next.classId === "brigandCrossbowman" && (next.spells.tier2 > 0 || next.spells.tier3 > 0)) {
      const multi = next.spells.tier3 > 0;
      const range = multi ? MULTI_SHOT.range : BLOODY_SHOT.range;
      let pick: { from: Point; foes: Unit[]; score: number } | null = null;
      for (const cell of reach.values()) {
        const ranked = players
          .filter((foe) => {
            const d = manhattan(cell, foe);
            return d >= next.minRange && d <= range && clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "arrow", this.decorOverlay);
          })
          .map((foe) => ({ foe, score: (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0) }))
          .sort((a, b) => b.score - a.score);
        if (!ranked.length) continue;
        const score = multi ? ranked.reduce((sum, r) => sum + r.score + 10, 0) : ranked[0].score;
        if (!pick || score > pick.score) pick = { from: { x: cell.x, y: cell.y }, foes: ranked.map((r) => r.foe), score };
      }
      if (pick) {
        if (pick.from.x !== next.x || pick.from.y !== next.y) this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, pick.from) });
        this.spendTier(next, multi ? "multiShot" : "bloodyShot");
        if (multi) {
          const power = multiShotPower(next.level);
          for (let i = 0; i < multiShotTargets(next.level); i++) {
            const foe = pick.foes[i % pick.foes.length];
            this.queue.push({ type: "spell", att: next.id, tiles: [{ x: foe.x, y: foe.y }], ids: [foe.id], label: MULTI_SHOT.name, weaponBonusDice: power.dice, weaponBonusFaces: power.faces, weaponBonusBonus: 0, spellKind: "multiShot" });
          }
        } else {
          const foe = pick.foes[0];
          this.queue.push({ type: "spell", att: next.id, tiles: [{ x: foe.x, y: foe.y }], ids: [foe.id], label: BLOODY_SHOT.name, dmgMul: bloodyShotMul(next.level), spellKind: "bloodyShot" });
        }
        this.queue.push({ type: "delay", dur: 0.12 });
        return;
      }
    }

    // Brigand ("Besteiro") is the one enemy archer — see brigandSpellUses. Piercing outranks
    // Long Shot whenever both are still banked, same priority shape as the cultist branch
    // above. Long Shot picks one target the same way; Piercing aims THROUGH a target the same
    // way castPiercing does, so it can also clip whoever else stands on that line (allies
    // included) — no side filter, matching the player-facing spell.
    if ((next.classId === "brigand" || next.classId === "baseCrossbowman") && (next.spells.tier1 > 0 || next.spells.tier2 > 0)) {
        const spellKind: "piercing" | "longShot" = next.spells.tier2 > 0 ? "piercing" : "longShot";
        const longMax = LONG_SHOT.range;
        let bestSpell: {
            foe: Unit;
            from: Point;
            score: number;
        } | null = null;
        for (const cell of reach.values()) {
            for (const foe of players) {
                if (spellKind === "longShot") {
                    const d = manhattan(cell, foe);
                    if (d < next.minRange || d > longMax)
                        continue;
                    if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "arrow", this.decorOverlay))
                        continue;
                }
                else {
                    const line = this.piercingRay({ x: cell.x, y: cell.y }, { x: foe.x, y: foe.y });
                    if (!line || !line.some((p) => p.x === foe.x && p.y === foe.y))
                        continue;
                }
                const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
                if (!bestSpell || score > bestSpell.score)
                    bestSpell = { foe, from: { x: cell.x, y: cell.y }, score };
            }
        }
        if (bestSpell) {
            if (bestSpell.from.x !== next.x || bestSpell.from.y !== next.y) {
                this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, bestSpell.from) });
            }
            this.spendTier(next, spellKind);
            if (spellKind === "longShot") {
                const power = longShotPower(next.level);
                this.queue.push({
                    type: "spell",
                    att: next.id,
                    tiles: [{ x: bestSpell.foe.x, y: bestSpell.foe.y }],
                    ids: [bestSpell.foe.id],
                    label: LONG_SHOT.name,
                    weaponBonusDice: power.dice,
                    weaponBonusFaces: power.faces,
                    weaponBonusBonus: 0,
                    spellKind: "longShot",
                });
            }
            else {
                const line = this.piercingRay(bestSpell.from, { x: bestSpell.foe.x, y: bestSpell.foe.y })!;
                const ids: string[] = [];
                for (const t of line) {
                    const who = this.units.find((u) => u.alive && occupies(u, t.x, t.y));
                    if (who && who.id !== next.id && !ids.includes(who.id))
                        ids.push(who.id);
                }
                this.queue.push({ type: "spell", att: next.id, tiles: line, ids, label: PIERCING.name, dmgMul: piercingMul(next.level), spellKind: "piercing" });
            }
            this.queue.push({ type: "delay", dur: 0.12 });
            return;
        }
    }
    let best: {
        foe: Unit;
        from: Point;
        score: number;
    } | null = null;
    for (const cell of reach.values()) {
        for (const foe of players) {
            if (!canHitFrom(next, cell, foe, this.tiles, this.cols, this.decorOverlay))
                continue;
            const terr = this.hexAt(cell.x, cell.y);
            const score = (foe.maxHp - foe.hp) * 3 + terr.def * 2 + (foe.hp <= 8 ? 20 : 0);
            if (!best || score > best.score)
                best = { foe, from: { x: cell.x, y: cell.y }, score };
        }
    }
    if (best) {
        if (best.from.x !== next.x || best.from.y !== next.y) {
            this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
        }
        this.queue.push({ type: "combat", att: next.id, def: best.foe.id });
        this.queue.push({ type: "delay", dur: 0.12 });
        return;
    }
    if (players.length === 0) {
        next.moved = true;
        return;
    }
    // Real path distance (walls/pillars-aware), not raw hex distance — a straight-line
    // "closest" pick can freeze an enemy in place forever once it's on the far side of an
    // obstacle, because every actual step first reads as moving away (see
    // terrainDistanceField). Computed once per player and reused for both picking who to
    // chase and which reachable cell actually closes the gap.
    const fields = this.playerDistanceFields(players);
    let nearest = fields[0]!;
    for (const f of fields) {
        const dCur = f.field.get(key(next.x, next.y)) ?? Infinity;
        const dBest = nearest.field.get(key(next.x, next.y)) ?? Infinity;
        if (dCur < dBest)
            nearest = f;
    }
    let closest: Point | null = null;
    let dist = Infinity;
    for (const cell of reach.values()) {
        const d = nearest.field.get(key(cell.x, cell.y)) ?? Infinity;
        if (d < dist) {
            dist = d;
            closest = { x: cell.x, y: cell.y };
        }
    }
    if (closest && (closest.x !== next.x || closest.y !== next.y)) {
        this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, closest) });
    }
    next.moved = true;
    this.queue.push({ type: "delay", dur: 0.08 });
}

/** Whoever tops this enemy's enmity table, or null while it has none (usual targeting). */
protected enmityTarget(foe: Unit): Unit | null {
    const row = this.enmity.get(foe.id);
    if (!row)
        return null;
    let best: Unit | null = null;
    let bestTotal = 0;
    for (const [heroId, entry] of row) {
        const hero = this.units.find((u) => u.id === heroId);
        if (!hero || !hero.alive || hero.side !== "player")
            continue;
        const total = enmityTotal(entry);
        if (total > bestTotal) {
            best = hero;
            bestTotal = total;
        }
    }
    return best;
}

protected gainSupportAffinity(actor: Unit, target: Unit): void {
    if (actor.side !== "player" || target.side !== "player" || actor.summoned || target.summoned)
        return;
    const recipients = this.supportAffinityRecipients.get(actor.id) ?? new Set<string>();
    if (recipients.has(target.id))
        return;
    recipients.add(target.id);
    this.supportAffinityRecipients.set(actor.id, recipients);
    this.adjustAffinity(actor, target, 0.2);
}

protected curePlayerDisease(u: Unit): void {
    u.poisoned = false;
    u.poisonTier = undefined;
    u.poisonMag = undefined;
    if (!u.diseaseBase) {
        u.diseased = false;
        return;
    }
    u.atk = u.diseaseBase.atk;
    u.mag = u.diseaseBase.mag;
    u.def = u.diseaseBase.def;
    u.dex = u.diseaseBase.dex;
    u.mov = u.diseaseBase.mov;
    u.diseaseBase = null;
    u.diseased = false;
}

protected effectiveUnitForReach(u: Unit): Unit {
    // A rooted encounter enemy keeps its authored position even after a status
    // effect or a loaded snapshot recalculates its movement stat.
    if (u.side === "enemy" && this.mission.enemySpawns.some((s, i) => s.holdsPosition && u.id === `enemy-${s.name}-${i}`))
        return { ...u, mov: 0 };
    // Free roam: any reachable hex is one click away, however far.
    if (this.mission.explore)
        return { ...u, mov: this.cols * this.rows };
    const remaining = Math.max(0, u.mov - u.moveBudgetUsed);
    const cap = this.turnRestrained ? Math.min(1, remaining) : remaining;
    return cap === u.mov ? u : { ...u, mov: cap };
}

protected smashBarricades(unit: Unit): void {
    if ((unit.classId !== "troll" && unit.classId !== "troll2") || !unit.alive)
        return;
    const fill: TerrainId = this.tiles.includes("nave") ? "nave" : "plains";
    const seen = new Set<string>();
    let n = 0;
    for (const p of footprint(unit)) {
        for (const c of [p, ...hexNeighbors(p.x, p.y)]) {
            if (!inBounds(c.x, c.y, this.cols, this.rows))
                continue;
            const k = key(c.x, c.y);
            if (seen.has(k))
                continue;
            seen.add(k);
            const i = c.y * this.cols + c.x;
            if (this.tiles[i] !== "barricade")
                continue;
            this.tiles[i] = fill;
            // Path costs just changed, so the cached distance fields no longer describe
            // this board (see playerDistanceFields).
            this.terrainVersion++;
            // The prop goes with the terrain — leaving it would draw a barricade over ground
            // that is now walkable.
            for (let d = this.decorations.length - 1; d >= 0; d--) {
                const dec = this.decorations[d];
                if (dec.id === "barricade" && dec.x === c.x && dec.y === c.y) {
                    this.decorations.splice(d, 1);
                    this.refreshDecorOverlay();
                }
            }
            n += 1;
            { }
        }
    }
    if (n) {
        this.tip = "O troll parte a barricada.";
        this.trauma = Math.min(1, this.trauma + 0.35);
        { }
    }
}

/**
 * Whether this foe is allowed to act, waking it if it can see the party.
 *
 * Always true without fog. Under fog a foe starts asleep and wakes the moment any
 * living party member is inside its own sight — its own, not the party's `vis`,
 * since the two see different things and reading the player's fog here would let a
 * foe act on knowledge it does not have.
 *
 * Waking sticks, and is remembered in the save: a foe that loses sight again keeps
 * hunting, because one that forgot the instant the party stepped behind a pillar
 * could be shaken off by walking one hex sideways.
 *
 * Known gap: a shot from beyond its sight radius does not wake it, so a long enough
 * bow can pick off a sleeping foe. Waking on damage needs a hook in the damage path
 * and is worth doing on its own.
 */
protected wakeIfSeesParty(foe: Unit): boolean {
    if (!this.fogged)
        return true;
    if (this.awake.has(foe.id))
        return true;
    for (const p of this.units) {
        if (p.side !== "player" || !p.alive)
            continue;
        if (hexDist(foe, p) > SIGHT_RADIUS)
            continue;
        if (!sightReaches(foe, p, this.tiles, this.cols, this.decorOverlay))
            continue;
        this.awake.add(foe.id);
        return true;
    }
    return false;
}

/**
 * One whole-board distance field per player, cached while the board and the party
 * stand still. See `fieldCache` for why this matters.
 *
 * Keyed on terrain version plus every player's position, so the first enemy of a
 * phase pays for the fields and the rest read them. A player moving (their own
 * phase, or a Trip/Stampede shove during the enemy's) or terrain changing retires
 * the whole set rather than trying to patch it.
 */
protected playerDistanceFields(players: Unit[]): {
    p: Unit;
    field: Map<string, number>;
}[] {
    let stamp = `${this.terrainVersion}`;
    for (const p of players)
        stamp += `|${p.id}:${p.x},${p.y}`;
    if (stamp !== this.fieldStamp) {
        this.fieldCache.clear();
        this.fieldStamp = stamp;
    }
    return players.map((p) => {
        let field = this.fieldCache.get(p.id);
        if (!field) {
            field = terrainDistanceField(p, this.tiles, this.cols, this.rows, this.decorOverlay);
            this.fieldCache.set(p.id, field);
        }
        return { p, field };
    });
}
protected resolveSpellSource(a: SpellImpact, att: Unit): void {
    a.hit = true;
    const usedElement = spellElement(a.spellKind);
    if (usedElement && (a.spellKind === "magicMissile" || a.spellKind === "magicMissileV2")) {
        const enemy = a.ids.map(id => this.units.find(u => u.id === id && u.alive && u.side === "enemy")).find(Boolean);
        if (enemy)
            this.trainElementUse(att, usedElement, enemy.level);
    }
    // Every attack cue (weapon skills included) already played when its animation began, in startSeq.
    // AoE/line spells: the first enemy actually hit grants full XP, every enemy after
    // that in the same cast grants half — hitting a whole group shouldn't out-earn
    // picking them off one at a time, but the first one still counts fully.
    let firstAoeEnemyHit = true;
    // Piercing Thrust: front-to-back falloff along the line — the first body it hits eats
    // the full hit, everyone skewered behind them takes half.
    let thrustHitIndex = 0;
    for (const id of a.ids) {
        const foe = this.units.find((u) => u.id === id && u.alive);
        if (!foe || (a.spellKind === "turnUndead" && !isUndeadClass(foe.classId)))
            continue;
        const defTile = this.hexAt(foe.x, foe.y);
        if (defTile.id === "barricade") {
            { }
            continue;
        }
        let dmg: number;
        let crit = false;
        let landed = true;
        let weaponRolled = false;
        if (a.centerId && foe.id === a.centerId) {
            dmg = this.spellDamage(att, foe, a.centerMul, rollDice(a.centerDice, a.centerFaces, a.centerBonus, this.rng));
        }
        else if (a.extraDice > 0) {
            let roll = rollDice(a.extraDice, a.extraFaces, a.extraBonus, this.rng);
            if (a.moreDice > 0)
                roll += rollDice(a.moreDice, a.moreFaces, 0, this.rng);
            dmg = this.spellDamage(att, foe, a.spellMul, roll);
        }
        else if (a.spellKind === "piercingThrust") {
            // Armor-piercing: the defender's DEF is treated as 20% lower for this hit only.
            const softened = { ...foe, def: Math.max(0, Math.floor(foe.def * (1 - PIERCING_THRUST.armorIgnore))) };
            const hit = this.rollDamageAt(this.affinityUnit(att), this.affinityUnit(softened), tileAt(this.tiles, this.cols, att.x, att.y), tileAt(this.tiles, this.cols, foe.x, foe.y), this.rng);
            weaponRolled = true;
            landed = hit.landed;
            dmg = thrustHitIndex === 0 ? hit.dmg : Math.max(1, Math.floor(hit.dmg * 0.5));
            crit = hit.crit;
            // A miss doesn't count as a body the thrust passed through — only a landed hit
            // advances the front-to-back falloff.
            if (landed)
                thrustHitIndex++;
        }
        else {
            const hit = this.rollDamageAt(this.affinityUnit(att), this.affinityUnit(foe), tileAt(this.tiles, this.cols, att.x, att.y), tileAt(this.tiles, this.cols, foe.x, foe.y), this.rng, isWeaponAbility(a.spellKind));
            weaponRolled = true;
            landed = hit.landed;
            dmg = hit.dmg;
            crit = hit.crit;
            if (a.weaponBonusDice > 0 && landed)
                dmg += rollDice(a.weaponBonusDice, a.weaponBonusFaces, a.weaponBonusBonus, this.rng);
        }
        if (isWeaponAbility(a.spellKind)) {
            const type = equippedWeaponType(att);
            if (!weaponRolled) {
                const mastery = weaponModifiers(att, type);
                const accuracy = dexAccuracy(mastery.accuracy, this.affinityUnit(foe).dex);
                landed = accuracy >= 100 || this.rng() * 100 < accuracy;
            }
            if (foe.side === "enemy")
                this.trainWeapon(att, type, foe.level);
        }
        if (!landed) {
            this.spawnMiss(foe);
            { }
            { }
            continue;
        }
        if (a.spellKind === "fantomForce")
            dmg = Math.max(1, Math.floor(dmg * FANTOM_FORCE.damageMul));
        if (a.dmgMul > 1)
            dmg = Math.max(1, Math.floor(dmg * a.dmgMul));
        if (a.spellKind === "cleave" && cleaveDoublesVs(foe))
            dmg = Math.max(1, Math.floor(dmg * CLEAVE.largeMul));
        if (foe.asleep) {
            dmg = Math.max(1, Math.floor(dmg * (1 + WEB_OF_DREAMS.sleepBonusDamage)));
            foe.asleep = false;
            foe.sleepTurns = 0;
        }
        dmg = Math.max(1, Math.floor(dmg * this.zoneDamageMul(foe)));
        const element = spellElement(a.spellKind);
        if (element)
            dmg = Math.floor(elementalDamage(dmg, foe.resistances?.[element] ?? 0, this.affinityUnit(att).mag));
        if (dmg > 0 && a.spellKind)
            this.adjustAffinity(att, foe, -1);
        foe.hp = Math.max(0, foe.hp - dmg);
        if (a.spellKind === "turnUndead" && foe.hp > 0) {
            foe.fearTurns = Math.max(foe.fearTurns ?? 0, TURN_UNDEAD.fearTurns);
            foe.fearSourceId = att.id;
            { }
        }
        this.noteDamageEnmity(att, foe, dmg, isWeaponAbility(a.spellKind) ? "weapon" : "spell");
        foe.flash = 1;
        foe.hitAt = this.time;
        this.provoke(foe, att);
        const poisonTier: PoisonTier = a.spellKind === "causticVenom" ? "poison" : "lesser";
        if (a.poison && this.rng() * 100 < poisonChance(effectivePoisonResistance(foe.resistances?.poison ?? 0, poisonTier, this.affinityUnit(att).mag))) {
            const currentTier = foe.poisoned ? foe.poisonTier : undefined;
            const nextTier = strongerPoison(currentTier, poisonTier);
            if (!currentTier || nextTier !== currentTier)
                foe.poisonMag = this.affinityUnit(att).mag;
            else if (nextTier === poisonTier)
                foe.poisonMag = Math.max(foe.poisonMag ?? 0, this.affinityUnit(att).mag);
            foe.poisonTier = nextTier;
            foe.poisoned = true;
        }
        if (element)
            this.trainResistance(foe, element, att.level);
        // Dreno de Vida: heals the familiar's own summoning conjurer for a share of the
        // damage it just dealt (see lifeDrainHealMul) — off the real rolled damage, not a
        // separate estimate, same reasoning as every other on-hit effect in this loop.
        if (a.spellKind === "lifeDrain") {
            const healer = this.units.find((u) => u.id === att.summonerId && u.alive);
            if (healer) {
                const gained = Math.min(Math.round(dmg * lifeDrainHealMul(att.level)), healer.maxHp - healer.hp);
                if (gained > 0) {
                    healer.hp += gained;
                    healer.healGlow = 1;
                    healer.healGlowKind = "holyMinor";
                    { }
                }
            }
        }
        // AoE/line abilities (fireball, cleave, piercing...) run this once per unit actually
        // hit, so every landed hit grants its own XP — piercing can also clip an ally in the
        // line, which must never grant XP.
        if (foe.side !== att.side) {
            // Black Mage / Conjurer finishing an enemy off with one of their own single-target
            // spells (Magic Missile, Lightning) doubles the XP from that kill, same as Long
            // Shot (moved onto this same "spell" step so its weapon+dice bonus can scale by
            // level) — never for AoE/line spells, where only the first enemy hit grants full
            // XP and the rest grant half.
            const isAoeSpell = a.spellKind === "turnUndead" ||
                a.spellKind === "fireball" ||
                a.spellKind === "cleave" ||
                a.spellKind === "piercing" ||
                a.spellKind === "causticVenom" ||
                a.spellKind === "minorVenom" ||
                a.spellKind === "piercingThrust" ||
                a.spellKind === "sweep" ||
                a.spellKind === "divineWrath" ||
                a.spellKind === "shoulderSmash" ||
                a.spellKind === "stampede" ||
                a.spellKind === "burningHands" || a.spellKind === "poisonBreath" || a.spellKind === "tendrilSwipe";
            const xpMul = foe.hp <= 0 && !isAoeSpell && (att.classId === "mage" || att.classId === "voss" || att.classId === "conjurer" || a.spellKind === "longShot" || a.spellKind === "bloodyShot")
                ? 2
                : isAoeSpell && !firstAoeEnemyHit
                    ? 0.5
                    : 1;
            if (isAoeSpell)
                firstAoeEnemyHit = false;
            // The universal finishing-blow bonus stacks multiplicatively on top of xpMul —
            // it doesn't replace the mage/conjurer/Long Shot kill bonus above or the AoE
            // per-target share, it applies in addition to whichever of those already fired.
            this.gainExp(att, foe.level, dmg, xpMul, foe.hp <= 0);
        }
        const meleeSkill = a.spellKind === "doubleStrike" || a.spellKind === "cleave" || a.spellKind === "piercingThrust" || a.spellKind === "sweep" || a.spellKind === "trip" || a.spellKind === "shoulderSmash" || a.spellKind === "stampede" || a.spellKind === "tendrilSwipe";
        this.spawnHit(foe, dmg, crit, meleeSkill);
        const large = a.spellKind === "cleave" && cleaveDoublesVs(foe);
        { }
        if (foe.hp <= 0) {
            this.markDead(foe);
        }
        else {
            { }
            if (a.echo)
                foe.shock = { ...a.echo, mag: this.affinityUnit(att).mag };
            if (a.spellKind === "bloodyShot" && (!foe.bleeding || foe.bleedRoundsLeft != null)) {
                const duration = bloodyShotBleed(att.level);
                foe.bleeding = true;
                foe.bleedRoundsLeft = rollDice(duration.dice, duration.faces, 0, this.rng);
                foe.bleedRoundMarker = this.turn;
                { }
            }
            if (a.spellKind === "sweep")
                this.knockBack(att, foe);
            if (a.spellKind === "shoulderSmash") {
                for (let i = 0; i < SHOULDER_SMASH.knockback; i++)
                    this.knockBack(att, foe);
            }
        }
    }
    if (a.spellKind === "minorVenom") { }
    if (a.spellKind === "causticVenom" && att.classId === "carnivorousPlant") { }
    if ((a.spellKind === "piercingThrust" || a.spellKind === "stampede") && a.tiles.length > 0) {
        const end = a.tiles[a.tiles.length - 1]!;
        { }
    }
}

protected resolveCombatSource(a: CombatImpact, actor: Unit, target: Unit): void {const arrowShot=this.isArrowAttack(actor)&&!this.offHandStrike(a);const arcaneBolt=!arrowShot&&this.isArcaneCaster(actor);{
    if (a.stage === "hit" && a.spellKind === "shieldBash") { }
    const attTile = tileAt(this.tiles, this.cols, actor.x, actor.y);
    const defTile = tileAt(this.tiles, this.cols, target.x, target.y);
    // customDice/dmgMul/stunChance are the attacker's own strike (off-hand weapon or
    // Shield Bash) — never applied to the defender's counter, which always uses their
    // real equipped weapon at full strength.
    const dice = a.stage === "hit" ? a.customDice : a.counterCustomDice;
    const hit = dice
        ? this.rollDamageCustomAt(this.affinityUnit(actor), this.affinityUnit(target), attTile, defTile, dice.dice, dice.faces, dice.bonus, this.rng, !(a.stage === "hit" && a.spellKind === "shieldBash"))
        : this.rollDamageAt(this.affinityUnit(actor), this.affinityUnit(target), attTile, defTile, this.rng, !(a.stage === "hit" && a.spellKind === "shieldBash"));
    if (target.side === "enemy" && !(a.stage === "hit" && a.spellKind === "shieldBash"))
        this.trainWeapon(actor, equippedWeaponType(actor, !!dice), target.level);
    const usesArcane = arcaneBolt && !this.offHandStrike(a) && (a.stage === "counterHit" || !a.spellKind);
    if (!hit.landed) {
        this.spawnMiss(target);
        { }
        { }
        // A missed Bull Rush hit shoves nobody aside, so the charge ends here — the rest of
        // it would run straight through the enemy that is still standing in the way.
        if (a.stage === "hit" && a.spellKind === "bullRush") {
            this.queue = this.queue.filter((q) => !(q.type === "move" && q.id === actor.id) && !("att" in q && q.att === actor.id && q.type === "combat" && q.spellKind === "bullRush"));
        }
    }
    else {
        let bonusRoll = 0;
        if (a.stage === "hit" && a.bonusDice > 0) {
            bonusRoll = rollDice(a.bonusDiceCount, a.bonusDice, a.bonusFlat, this.rng);
            hit.dmg += bonusRoll;
        }
        // Bull Rush's wall-impact dice: folded into this same hit/defence resolution
        // (never a second hit, never a second counter) — only rolled when the
        // precomputed knockback (see knockTo/castBullRush) was blocked short.
        if (a.stage === "hit" && a.wallImpact) {
            hit.dmg += rollDice(a.wallImpact.dice, a.wallImpact.faces, 0, this.rng);
        }
        // Executioner's Strike: replaces, rather than stacks with, a normal crit —
        // rebuilt from preCritDmg so any weapon crit rollDamage already applied is
        // discarded in favor of the execution multiplier, never both at once.
        let executed = false;
        if (a.stage === "hit" && a.spellKind === "executionerStrike") {
            const power = executionerStrikePower(actor.level);
            if (target.maxHp > 0 && target.hp / target.maxHp <= power.threshold) {
                hit.dmg = Math.max(1, Math.floor((hit.preCritDmg + bonusRoll) * power.mult));
                executed = true;
            }
        }
        if (a.stage === "hit" && a.dmgMul !== 1) {
            hit.dmg = Math.max(1, Math.floor(hit.dmg * a.dmgMul));
        }
        if (a.stage === "hit" && a.stunChance > 0 && this.rng() < a.stunChance) {
            target.stunned = true;
            target.stunTurns = 1;
            { }
        }
        if (target.asleep) {
            hit.dmg = Math.max(1, Math.floor(hit.dmg * (1 + WEB_OF_DREAMS.sleepBonusDamage)));
            target.asleep = false;
            target.sleepTurns = 0;
        }
        hit.dmg = Math.max(1, Math.floor(hit.dmg * this.zoneDamageMul(target)));
        if (usesArcane) {
            hit.dmg = Math.floor(elementalDamage(hit.dmg, target.resistances?.arcane ?? 0, this.affinityUnit(actor).mag));
            this.trainResistance(target, "arcane", actor.level);
        }
        if (a.stage === "hit" && a.spellKind && hit.dmg > 0)
            this.adjustAffinity(actor, target, -1);
        target.hp = Math.max(0, target.hp - hit.dmg);
        this.noteDamageEnmity(actor, target, hit.dmg, "weapon");
        target.flash = 1;
        target.hitAt = this.time;
        this.provoke(target, actor);
        if (target.side !== actor.side) {
            if (a.stage === "hit") {
                this.gainExp(actor, target.level, hit.dmg, 1, target.hp <= 0);
            }
            else {
                // A counter deals its full real damage but only ever earns a flat 1 XP — see
                // gainCounterExp — so a unit can't out-level by baiting hits and countering
                // instead of attacking.
                this.gainCounterExp(actor);
            }
        }
        this.spawnHit(target, hit.dmg, hit.crit, !this.isArrowAttack(actor) && !this.isArcaneCaster(actor));
        { }
        // The blade swoosh is the visual for the strike landing, not for the target
        // surviving it — fire it here, unconditionally, same as spawnHit/pushLog above,
        // rather than nested under the "target lived" branch below (where it used to be
        // silently skipped on any kill).
        if (a.stage === "hit" && a.spellKind === "trip") {
            const oc = this.hexCenter(actor.x, actor.y);
            const tc = this.hexCenter(target.x, target.y);
            { }
        }
        if (a.stage === "hit" && a.spellKind === "doubleStrike") {
            const oc = this.hexCenter(actor.x, actor.y);
            const tc = this.hexCenter(target.x, target.y);
            const base = Math.atan2(tc.cy - oc.cy, tc.cx - oc.cx);
            this.doubleStrikeAlt = !this.doubleStrikeAlt;
            { }
        }
        if (a.stage === "hit" && a.spellKind === "bullRush") {
            const oc = this.hexCenter(actor.x, actor.y);
            const tc = this.hexCenter(target.x, target.y);
            { }
        }
        if (a.stage === "hit" && a.spellKind === "shieldBash") { }
        if (a.stage === "hit" && a.spellKind === "executionerStrike") {
            const attackerCenter = this.hexCenter(actor.x, actor.y);
            const targetCenter = this.hexCenter(target.x, target.y);
            const fromLeft = attackerCenter.cx < targetCenter.cx ||
                (attackerCenter.cx === targetCenter.cx && actor.x < target.x);
            { }
        }
        if (target.hp <= 0) {
            this.markDead(target);
        }
        else {
            { }
            if (a.stage === "hit")
                this.maybeInflictDisease(actor, target);
            if (a.stage === "hit" && a.spellKind === "trip") {
                // Rasteira causes Bleeding, not stun (per direct instruction).
                target.bleeding = true;
                target.bleedRoundsLeft = undefined;
                target.bleedRoundMarker = undefined;
                if (!target.crippled) {
                    target.crippled = true;
                    const keep = 1 - TRIP.statPenalty;
                    target.atk = Math.round(target.atk * keep);
                    target.mag = Math.round(target.mag * keep);
                    target.def = Math.round(target.def * keep);
                    target.dex = Math.round(target.dex * keep);
                    target.mov = Math.max(1, Math.round(target.mov * keep));
                }
                if (actor.name !== "Kael") { }
            }
            if (a.stage === "hit" && a.spellKind === "shieldBash") {
                target.stunned = true;
                target.stunTurns = shieldBashPower(actor.level).stunTurns;
                { }
            }
            if (a.stage === "hit" && a.spellKind === "bullRush" && a.knockTo) {
                target.x = a.knockTo.x;
                target.y = a.knockTo.y;
                target.drawX = a.knockTo.x;
                target.drawY = a.knockTo.y;
                { }
            }
        }
    }
}}

protected static readonly KILL_EXP_BONUS_MUL=1.25;
protected static readonly FAMILIAR_XP_SHARE=0.1;

}
