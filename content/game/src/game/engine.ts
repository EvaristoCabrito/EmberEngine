import { TURN_UNDEAD, turnUndeadPower, turnUndeadFormula, isUndeadClass, WARP, warpPartyRadius, warpPortalRounds } from "./data";
import { drawTurnUndeadV4, TURN_UNDEAD_V4_DURATION } from "./gfx/TurnUndeadV4";
import { FROST, frostPower, frostAreaTiles, frostCharges } from "./frost";
import { drawProvokeVFX, PROVOKE_FX_DURATION } from "./gfx/ProvokeVFX";
import { dexAccuracy, dexEscapeChance } from "./dexterity";
import { equippedWeaponType, trainedWeaponSkills, weaponTypesForClass, weaponModifiers, isWeaponAbility } from "./weaponSkills";
import { elementalDamage, spellElement, sumResistances, SPELL_CLASSIFICATION } from "./resistances";
import { staffElementalDamage, staffLifeSteal, type MageStaffMagic } from "../ember/mageStaffMagic";
import { AFFINITY_HEROES, affinityBonus, affinityScore, changeAffinity, cleanAffinityScores, type AffinityHero } from "./affinity";
import { tacticalGridStyleQuiet as tacticalGridStyle, GRID_MOVE, GRID_ROUTE, GRID_ALLY, GRID_ENEMY, GRID_ENEMY_TARGET, GRID_ENEMY_GLOW, GRID_OFFHAND_TARGET } from "./tacticalGrid";
import { isHexGroundVariant, requestSpriteArt } from "./assets";
import { drawGroundTexture, drawHexGround } from "./hexGround";
import { vauBackdropBounds } from "./vauBackdrop";
import { BIG_HOUSE_DECOR_IDS, CAUSTIC_VENOM, DIVINE_BOLT, MINOR_VENOM, DECOR_ART_SCALE, HOUSE_ART_SCALE, CHEST_DECOR_IDS, CHEST_LOOT, CLASSES, CLEAVE, cleaveDoublesVs, cleaveFormula, cleavePower, CURE_DISEASE, CURES, DECORATIONS, DISEASE, DOUBLE_STRIKE, doubleStrikeFormula, doubleStrikePower, EMPTY_BAG, EQUIPMENT, EXP_TO_LEVEL, expToLevel, expForHit, FIREBALL, ICE_STORM, iceStormPower, iceStormAreaTiles, iceStormFormula, FANTOM_FORCE, FOOTPRINT_TYPE_7, FOOTPRINT_TYPE_8, formatSpellUseGains, HIGH_GROUND_LIFT, HOUSE_DECOR_IDS, KILL_DROP_CHANCE, LIGHTNING, LIGHTNING_T3, LONG_SHOT, longShotFormula, longShotPower, BLOODY_SHOT, bloodyShotMul, bloodyShotBleed, PROVOKE, provokePower, provokeFormula, MAGIC_MISSILE, magicMissileCount, MAX_LEVEL, PIERCING, piercingMul, PIERCING_THRUST, POTION_CARRY_MAX, POTIONS, RATIONS_ICON, SHOCK, SUMMON_FAMILIAR, PHANTASMAL_FORCE, PHANTASMAL_FORCE_UNLOCK_LEVEL, phantasmalForceDice, phantasmalForceFormula, SUMMON_FAMILIAR2, SUMMON_FAMILIAR2_UNLOCK_LEVEL, SUMMON_FAMILIAR3, SUMMON_FAMILIAR4, SUMMON_ZOMBIE_DOG, FAMILIAR_SPELL, familiarSpellCharges, familiarMagicMissileCharges, LIFE_DRAIN, lifeDrainDice, lifeDrainFormula, familiarLifeDrainCharges, lifeDrainHealMul, SWEEP, TRIP, WEAPON_MAX_ENH, WEAPONS, WEB_OF_DREAMS, healFormula, barricadeDecor, decorationCells, decorationFacing, decorationImage, decorationImageRetryWebp, diceFormula, effectiveMaxRange, enemyLevelFor, equipmentIcon, fireballFormula, fireballOrigin, fireballPower, fireballRangeTiles, fireballTiles, hexAreaTiles, isProjectile, isSummonClass, isBossClass, lightningDice, lightningFormula, lightningTier3Formula, parseLayout, placedFootprint, potionLabel, rollCure, rollDice, rollPotion, shockChargesFor, spellFormula, spellTier, spellUseGains, starterWeaponFor, STARTING_BAG, statsFor, terrainNote, TERRAIN, tierKey, tierUses, gearStatBonus, offHandBlocked, equipmentFitsSlot, equipmentSlotName, equipmentTooltip, weaponTooltip, potionTooltip, weaponIcon, weaponRoll, weightedLootPick, weightedPotionPick, MULTI_SHOT, multiShotFormula, multiShotPower, multiShotTargets, SECOND_WIND, secondWindPct, auraPower, AURA_OF_PROTECTION, INTIMIDATING_PRESENCE, DIVINE_WRATH, divineWrathFormula, divineWrathPower, SHOULDER_SMASH, shoulderSmashFormula, shoulderSmashPower, SIGHT_RADIUS, STAMPEDE, stampedeFormula, stampedePower, cultistSpellUses, brigandSpellUses, birolhoSpellUses, webOfDreamsSize, webOfDreamsSleepChance, BULL_RUSH, BULL_RUSH_UNLOCK_LEVEL, bullRushFormula, bullRushPower, EXECUTIONER_STRIKE, executionerStrikeFormula, executionerStrikePower, SHIELD_BASH, shieldBashPower, POISON_BREATH, poisonBreathFormula, poisonBreathPower, BURNING_HANDS, burningHandsFormula, burningHandsPower, CREATE_FOOD_AND_WATER, createFoodAndWaterPower, BLESS, rulesClass } from "./data";
import type { SpellTier } from "./data";
import { weightedWeaponPick, shieldBashFormula, fantomForceChargesFor, fantomForceDice } from "./data";
import { clearRockColumnTiles, placedBlockingFootprint, THREE_D_DOOR_VARIANTS } from "./data";
import { mapFloorRects, floorRectParts, hasSquareMapBorder } from "./mapFloor";
import { closeWatchtowerWalls } from "./watchtowerDungeon";
import { decorationPlacementArt } from "./data";
import { canCounter, makeForecast, mulberry32, powerOf, protOf, rollDamage, rollDamageCustom } from "./combat";
import { effectivePoisonResistance, POISON_TIERS, poisonChance, poisonDice, poisonTickDamage, poisonTierOf, strongerPoison } from "./poison";
import { cleanHeroSkills, rollWeaponSkillGain, rollSkillGain, skillResistances, SKILL_GAIN, SKILLS, healingAmount, skillValue, type HeroSkills, type SkillId } from "./skills";
import { addToEntry, ENMITY, enmityFromSnapshot, enmityToSnapshot, enmityTotal, type EnmityEntry, type EnmityTable } from "./enmity";
import {
  attackableEnemies,
  canHitFrom,
  clearShot,
  shotBlocker,
  computeReachable,
  computeThreat,
  cleaveHexes,
  CUBE_DIRS,
  cubeRound,
  footprint,
  footprintFrontRow,
  hexNeighbors,
  hexDist,
  hexLine,
  inBounds,
  inWeaponRange,
  key,
  manhattan,
  occupancy,
  occupies,
  piercingLine,
  shotKind,
  allAxisRays,
  canTraverseWater,
  reconstructPath,
  terrainDistanceField,
  tileAt,
  unitSize,
  axisWalk,
  axisDir,
  coneWedge,
  coneSector,
  type ReachCell,
  type Cube,
} from "./pathfinding";
import { packExplored, relight, sightReaches, unpackExplored } from "./fog";
import { getDevGfx } from "./gfx/three/devGfx";
import { decorationAnchor } from "./gfx/decorationAnchor";
import { buildDecorOverlay, hexDef, type DecorOverlay } from "./hexprops";
import { ACTION_HUNGER_COST, drainHunger, fullness } from "./hunger";
import { HUNGER_PENALTY_MAX } from "./overworld";
import { hasMonsterSfx, sfxPlay } from "./audio";
import { NOTORIOUS_LEVEL_BONUS } from "./quests";
// Shadows the DOM global of the same name: the WebGL2DRenderer used for the battle canvas
// (see BattleCanvas.tsx) implements this instead of a real Path2D, and every `new Path2D()`
// below (the blade-sweep crescent) needs to build one it understands.
import { Path2D } from "./gfx/WebGL2DRenderer";
import type { ElementKind } from "./gfx/params";
import type {
  Bag,
  BattleSnapshot,
  BattleUnitSnap,
  TerrainDef,
  ClassId,
  DecorationPlacement,
  DialogTree,
  ElementalFxPlacement,
  Forecast,
  GameArt,
  HealId,
  HudSnapshot,
  InputMode,
  Mission,
  Phase,
  Point,
  PotionId,
  SpellKind,
  SpriteId,
  TerrainId,
  TierKey,
  Unit,
  UnitPublic,
  EquipSlot,
  StatPointAllocation,
  StatPointAttribute,
  Spawn,
  PoisonTier,
} from "./types";

interface Layout {
  ox: number;
  oy: number;
  tile: number;
  cols: number;
  rows: number;
}

interface Particle {
  live: boolean;
  x: number;
  y: number;
  vx: number;
  vy: number;
  life: number;
  max: number;
  size: number;
  color: string;
  text?: string;
  kind: "spark" | "text" | "impact";
  frame: number;
}

const PARTICLE_CAP = 32;
export const ZOOM_RADII = [22, 34, 50, 72];

/** Which WebGL elemental FX shader (see gfx/shaders.ts) a landed spell hit lights up on its
 * target tile(s), and how long that shader patch lingers (seconds) before it self-expires —
 * see EffectsRenderer.spawnEffect's `duration` option. Only spells with a clear elemental
 * theme are listed; anything absent here (melee skills, arrows, heals, ...) queues no FX. */
const SPELL_ELEMENT_FX: Partial<Record<SpellKind, { kind: ElementKind; duration: number }>> = {
  causticVenom: { kind: "acid", duration: 1.3 },
  minorVenom: { kind: "acid", duration: 1.3 },
  divineBolt: { kind: "holy", duration: 1.15 },
  // Lightning/Lightning Tier 3/Choque deliberately have NO entry here — per direct report,
  // the newer WebGL shader burst this table drives read as an odd "3D" pop layered on top
  // of the older, plain 2D bolt/spark cue (see emitLightningFx, still called separately for
  // all three in stepSpell) — that older cue is the only lightning FX any of them get now.
  divineWrath: { kind: "holy", duration: 0.9 },
};

/** Seconds one step of a walk animation takes. Shared by the position and the
 * high-ground lift so a unit's feet and its elevation move on the same clock. */
const MOVE_STEP_DUR = 0.12;

/** Level-up flourish: a small pixel-space burst anchored to a unit's hex (recomputed every
 * frame from its live position, so it still tracks correctly if the unit somehow moves mid-
 * burst) rather than routed through the hex-grid-snapped Particle system above — that one
 * always renders at its host hex's exact center, which is right for a hit-spark but too
 * coarse for sparks that are meant to actually scatter. */
interface LevelUpSpark {
  live: boolean;
  unitId: string;
  kind: "ring" | "star" | "label";
  /** Offset from the unit's hex center, in pixels at the burst's own tile scale — rescaled by
   * the live cell size at draw time so it still reads right after a zoom change. */
  dx: number;
  dy: number;
  vx: number;
  vy: number;
  life: number;
  max: number;
  size: number;
  hue: number;
  rot: number;
  vrot: number;
  refCell: number;
  text?: string;
}

const LEVEL_UP_FX_CAP = 72;

function blankLevelUpSpark(): LevelUpSpark {
  return {
    live: false,
    unitId: "",
    kind: "star",
    dx: 0,
    dy: 0,
    vx: 0,
    vy: 0,
    life: 0,
    max: 1,
    size: 1,
    hue: 46,
    rot: 0,
    vrot: 0,
    refCell: 1,
  };
}

/** How long Magic Missile's bolt takes to reach its target — stepSpell's own hit/damage tick
 * for a magicMissile cast fires at exactly this time (see MISSILE_HIT_AT below) instead of
 * the normal 0.18, so slowing this down keeps the impact flash/number landing right as the
 * bolt visually arrives instead of drifting out of sync with it. */
const MISSILE_TRAVEL = 0.18;
/** Phantasmal Force is an apparition, not a bolt: give its attacking silhouette enough
 * screen time to read before its claws land. */
const PHANTASMAL_FORCE_TRAVEL = 0.38;
/** Phantom System retains its own legacy bolt timing. */
const FANTOM_FORCE_TRAVEL = 0.38;
/** stepSpell's hit tick, per spellKind — every other spell keeps the original 0.18; only
 * Magic Missile's is tied to its own (now longer) travel time. */
const MISSILE_HIT_AT = MISSILE_TRAVEL;
/** Flight time of the actual spell bolts (Magic Missile, Fireball, Caustic Venom) — slower and
 * more cinematic than arrows, so the effect can be seen travelling. Their hit/damage tick
 * (stepSpell's hitAt) is this same value, so the impact still lands as the bolt arrives. */
const SPELL_TRAVEL = 0.6;
/** Standard projectile timing: Neera's arrows now land at the same speed as the rest of combat. */
const ARROW_TRAVEL = MISSILE_TRAVEL;
/** How much longer the bolt's glowing trail lingers on screen, fading, after the bolt
 * itself has already landed. Kept short enough that MISSILE_TRAVEL + this stays under 0.55
 * — stepSpell's own finishCombat threshold for every spell — so the trail's afterglow never
 * outlives the active step it belongs to. */
const MISSILE_AFTERGLOW = 0.2;
/** Dreaming Web's shot is purely cosmetic — the spell's real effect (the zone, the sleep
 * rolls) already happens synchronously in castWebOfDreams before this ever starts playing, so
 * unlike every other MissileFx kind its travel time isn't tied to any hit-timing threshold and
 * can just be as long as it needs to be to actually read as the dense, tangled WebGL beam it
 * is (see BattleEngine.webShotBeam / shaders.ts WEB_SHOT) instead of a blink-and-miss streak. */
export const WEB_SHOT_TRAVEL = 0.85;
export interface FireballVfxRequest {
  id: string;
  casterId: string;
  target: Point;
  /** Exact board cells included in Fireball's damage resolution. */
  tiles: Point[];
}
export type FireballVfxEvent = { id: string; phase: "launch" | "impact" | "complete" };
export type CausticVenomVfxRequest = FireballVfxRequest;
export type CausticVenomVfxEvent = FireballVfxEvent;
export interface PhantasmalForceVfxRequest { id: string; target: Point; targetUnitId: string }
export type PhantasmalForceVfxEvent = { id: string; phase: "impact" | "complete" };
export interface BlessVfxRequest { id: string; center: Point; allies: { id: string; x: number; y: number; distanceHexes: number }[] }
export type BlessVfxTimelineEvent = "bless_charge" | "bless_release" | "bless_wave" | "bless_unit_receive" | "bless_absorb" | "bless_complete";
export type BlessVfxEvent = { id: string; phase: "apply"; unitId: string } | { id: string; phase: "complete" } | { id: string; phase: "timeline"; event: BlessVfxTimelineEvent; unitId?: string };
export interface MagicMissileV2VfxRequest { id: string; casterId: string; targetUnitId: string }
export type MagicMissileV2VfxEvent = { id: string; phase: "impact"; index: number } | { id: string; phase: "complete" };
export interface BurningHandsV2VfxRequest { id: string; casterId: string; tiles: Point[]; poison?: boolean }
export type BurningHandsV2VfxEvent = { id: string; phase: "release" | "complete" };
export interface VarreduraVfxRequest { id: string; casterId: string; tiles: Point[]; targetIds: string[] }
export interface CleaveVfxRequest { id: string; casterId: string; tiles: Point[]; targetIds: string[] }
export type MagicMissileV2TimelineEvent = "magic_missile_charge" | "magic_missile_launch_1" | "magic_missile_launch_2" | "magic_missile_launch_3" | "magic_missile_impact_1" | "magic_missile_impact_2" | "magic_missile_impact_3" | "magic_missile_complete";

/** A traveling spell bolt (currently just Magic Missile) — hex-to-hex in pixel space, timed to
 * land right as stepSpell's own hit/damage tick fires (a.t >= MISSILE_HIT_AT), so the streak
 * and the impact flash/number line up without the two systems knowing about each other. */
interface MissileFx {
  live: boolean;
  fromX: number;
  fromY: number;
  toX: number;
  toY: number;
  t: number;
  travel: number;
  max: number;
  hue: number;
  neeraArrow: boolean;
  kind: "magicMissile" | "phantasmalForce" | "fantomForce" | "fireball" | "causticVenom" | "minorVenom" | "longShot" | "arcaneBolt" | "webOfDreams";
  seed: number;
}


const MISSILE_FX_CAP = 12;
/** A brief, code-drawn patch of fire on one affected Fireball hex. */
interface FireballBurstFx {
  live: boolean;
  x: number;
  y: number;
  t: number;
  max: number;
  seed: number;
  kind: "fireball" | "causticVenom";
}
const FIREBALL_BURST_CAP = 19;
function blankFireballBurstFx(): FireballBurstFx {
  return { live: false, x: 0, y: 0, t: 0, max: 0.58, seed: 0, kind: "fireball" };
}

function blankMissileFx(): MissileFx {
  return { live: false, fromX: 0, fromY: 0, toX: 0, toY: 0, t: 0, max: MISSILE_TRAVEL + MISSILE_AFTERGLOW, travel: MISSILE_TRAVEL, hue: 268, neeraArrow: false, kind: "magicMissile", seed: 0 };
}

/** How long the Lightning strike's flash lasts, start to fully faded — short and sudden on
 * purpose, a real strike rather than a travelling bolt. Choque keeps this; Relâmpago uses a
 * longer, heavier sky-fall (see LightningFx.power). */
const LIGHTNING_STRIKE_DUR = 0.42;
const LIGHTNING_RAIO_DUR = 0.72;
const LIGHTNING_T3_DUR = 0.92;
/** How many hexes of "sky" the bolt is drawn falling from, above the struck hex. */
const LIGHTNING_FALL_HEIGHT = 3.2;
const LIGHTNING_RAIO_FALL_HEIGHT = 6.4;

/** One fork off the main bolt — its own short jagged line, peeling away partway down. */
interface LightningBranch {
  /** 0-1, how far down the main bolt this fork leaves it. */
  at: number;
  /** -1 or 1: which side it forks toward. */
  side: number;
  /** Lateral jitter per segment, as a fraction of a tile — fixed at emit time so the bolt
   * holds a steady shape across its short life instead of jittering frame to frame. */
  segs: number[];
}

/** A bolt struck down from directly above a hex — thick, jagged, forked, gone in well under
 * half a second. The zigzag and branch shapes are generated once at emit time (see
 * emitLightningFx) and just fade over `t`, rather than being redrawn randomly every frame,
 * so the bolt reads as one firm, deliberate strike instead of a flickering scribble. */
interface LightningFx {
  live: boolean;
  x: number;
  y: number;
  t: number;
  max: number;
  hue: number;
  segs: number[];
  branches: LightningBranch[];
  /** "shock" is Choque. "raio" is Relâmpago. "t3" is Lighting Tier 3 — viewport-tall strike. */
  power: "shock" | "raio" | "t3" | "divine" | "divineSplash";
}

const LIGHTNING_FX_CAP = 16;

function blankLightningFx(): LightningFx {
  return { live: false, x: 0, y: 0, t: 0, max: LIGHTNING_STRIKE_DUR, hue: 205, segs: [], branches: [], power: "shock" };
}

/** A blue conjuring circle that opens on the ground, spins, and closes again — Summon
 * Familiar's cast tell. Purely cosmetic and self-timed: the familiar itself is added to
 * `this.units` immediately (so its stats/turn-order slot exist right away), just with
 * `fade: 0` until this plays out, so it visibly steps out of the portal rather than the two
 * looking unrelated. */
interface PortalFx {
  live: boolean;
  x: number;
  y: number;
  t: number;
  max: number;
  seed: number;
  /** Body shape of what steps out (FOOTPRINT_TYPE_*), so the portal centres on the whole
   * body rather than just its anchor hex. Null for a one-hex summon. */
  body: { dx: number; dy: number }[] | null;
  /** Familiar Titã's portal: red, and no light column rising out of it. */
  red: boolean;
  /** Warp uses an upright, expanding blue gate instead of the summoning circle. */
  warp?: boolean;
  radius?: number;
  /** Paired Warp gate that loops until its round timer expires. */
  persistent?: boolean;
}
interface WarpGatePair { a: Point; b: Point; roundsLeft: number; level: number; casterId: string }
const PORTAL_FX_CAP = 16;
function blankPortalFx(): PortalFx {
  return { live: false, x: 0, y: 0, t: 0, max: 0.85, seed: 0, body: null, red: false, warp: false, radius: 1 };
}
let warpPortalFallbackImage: HTMLImageElement | null = null;
let warpPortalFlipbooks: [HTMLImageElement, HTMLImageElement, HTMLImageElement] | null = null;
function getWarpPortalFlipbooks(): [HTMLImageElement, HTMLImageElement, HTMLImageElement] | null {
  if (typeof Image === "undefined") return null;
  if (!warpPortalFlipbooks) {
    warpPortalFlipbooks = ["main", "secondary", "particles"].map((layer) => {
      const image = new Image();
      image.src = `/game/fx/warp-v3-${layer}-flipbook-4x4.png`;
      return image;
    }) as [HTMLImageElement, HTMLImageElement, HTMLImageElement];
  }
  const [main, secondary, particles] = warpPortalFlipbooks;
  return [main, secondary, particles].every((image) => image.complete && image.naturalWidth > 0) ? warpPortalFlipbooks : null;
}
function getWarpPortalFallbackImage(): HTMLImageElement | null {
  if (typeof Image === "undefined") return null;
  if (!warpPortalFallbackImage) {
    warpPortalFallbackImage = new Image();
    warpPortalFallbackImage.src = "/game/effects/warp-portal-v01.png";
  }
  return warpPortalFallbackImage.complete && warpPortalFallbackImage.naturalWidth > 0 ? warpPortalFallbackImage : null;
}

/** Divine light / potion burst sitting on a character. Independent of healGlow so the old
 * Potionzero halo can still be fired on its own for a future skill. */
/** food = Create Food and Water: Cura Média's exact light, in blue. */
type HolyKind = "minor" | "hands" | "medium" | "disease" | "potion" | "food";
interface HolyFx {
  live: boolean;
  unitId: string;
  x: number;
  y: number;
  t: number;
  max: number;
  kind: HolyKind;
  seed: number;
  rays: number[];
}
const HOLY_FX_CAP = 10;
function blankHolyFx(): HolyFx {
  return { live: false, unitId: "", x: 0, y: 0, t: 0, max: 0.8, kind: "minor", seed: 0, rays: [] };
}
function holyDuration(kind: HolyKind): number {
  if (kind === "hands") return 0.94;
  if (kind === "medium" || kind === "food") return 1.18;
  if (kind === "disease") return 1.02;
  if (kind === "potion") return 0.88;
  return 0.7;
}

/** The shared physical-skill visual for every warrior/lancer/knight tier — a wipe of steel
 * light (a blade arc, a low cut, a flattened ring, a fast dash, or a bare shock ring), never
 * fire or a magic glow, so a physical skill never reads as a spell going off. One pooled
 * system covers all of them; `kind` picks the shape drawn (see drawBladeFx). */
/** execution = Golpe do Carrasco's axe chop; tripSweep = Rasteira's low sweep + blood. */
/** rushTrail/rushImpact = Bull Rush: the golden charge streak left behind, and its forward
 * burst on the target — its own look, never Sweep's ring. */
type BladeKind = "arc" | "cross" | "lowCut" | "ring" | "dash" | "shockRing" | "execution" | "tripSweep" | "rushTrail" | "rushImpact";
interface BladeFx {
  live: boolean;
  kind: BladeKind;
  /** Origin hex — the attacker for arc/ring/dash/shockRing, the target for cross/lowCut. */
  x: number;
  y: number;
  /** Dash-only: the far hex the steel streak travels to. */
  toX: number;
  toY: number;
  /** Arc-only: unwrapped sweep angle range, a1 always >= a0. Cross reuses a0 alone as the
   * single slash's angle. */
  a0: number;
  a1: number;
  t: number;
  max: number;
  seed: number;
  /** Shoulder Smash's arc reads heavier/warmer than Cleave's — same shape, different tint. */
  warm: boolean;
  /** Execution chop mirrors toward the attacker's side of its target. */
  mirrorX: boolean;
}
const BLADE_FX_CAP = 12;
function blankBladeFx(): BladeFx {
  return { live: false, kind: "arc", x: 0, y: 0, toX: 0, toY: 0, a0: 0, a1: 0, t: 0, max: 0.32, seed: 0, warm: false, mirrorX: false };
}

function blankParticle(): Particle {
  return {
    live: false,
    x: 0,
    y: 0,
    vx: 0,
    vy: 0,
    life: 0,
    max: 1,
    size: 1,
    color: "#fff",
    kind: "spark",
    frame: 0,
  };
}

type Seq =
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
  /** The caster's healing pose only, for an effect that already resolved instantly (Create
   * Food and Water); queued only for sprites with a healing sheet (GameArt.castsHeal). */
  | { type: "castPose"; id: string }
  | { type: "checkEnd" };

interface MoveAnim {
  type: "move";
  id: string;
  path: Point[];
  i: number;
  t: number;
  /** Bull Rush's charge: a fast dash with its speed-streak FX (see drawChargeFx). */
  charge?: boolean;
}

/** Global playback rule for long sprite sheets, keyed only on frame count (never on a sprite
 * name), so every 36-frame FINAL sprite — and any added later — plays at a real frame rate
 * instead of being squeezed into the timing built for 4-12 frame sheets. Sheets shorter than
 * LONG_SHEET_FRAMES are untouched. */
const LONG_SHEET_FRAMES = 24;
/** Bull Rush's targeting radius, in hexes. */
const BULL_RUSH_RANGE = 4;
/** Seconds the movement/range grid takes to fade in once every action and effect is done. */
const OVERLAY_FADE_IN = 0.4;
/** Default: one full pass of any long sheet (idle, walk, attack, cast) lasts this many
 * seconds, whatever its frame count — a 36-frame sheet plays at 12 fps. */
const LONG_ANIM_SECONDS = 3;
/** Kael's 36-frame swing, start to finish — shorter than the 3s every other long sheet gets. */
const KAEL_FINAL_ATTACK_SECONDS = 2;
/** Apparition's 60-frame ATT (5 s of her video at real speed), longer than the usual 3 s on purpose. */
const APPARITION_ATTACK_SECONDS = 5;
/** Seconds into BladeSlash1Dagger.mp3 Kael's swing sound starts from (see stepCombat). */
const KAEL_BLADE_SOUND_START = 1.12;
/** A death sheet (GameArt.deaths) plays over this long, then the body lies still for
 * DEATH_HOLD_SECONDS before fading out like any other fallen unit. */
const DEATH_ANIM_SECONDS = 3;
const DEATH_HOLD_SECONDS = 1;
/** A hit-reaction sheet (GameArt.hits) plays over this long. On a killing blow it plays
 * first and the death sheet starts right after it. */
const HIT_ANIM_SECONDS = 3;
/** Walk cycles run faster than the rest: one full pass of a long walk sheet takes this long. */
const LONG_WALK_SECONDS = 1.5;
// Give the heavy Ox time to settle into each pose; all of its clocks share this pace.
const BIG_BLUE_OX_PACE = 0.75;
/** Malrec's sheets (public/game/sprites/malrec): standing body height (staff included, frame 1)
 * and feet height above the canvas bottom, in source pixels, per file prefix. Drawn so every
 * sheet's body is MALREC_BODY_CELLS tall — his idle size as it always was (296 px of a 320 px
 * canvas in the standard 1.704-cell box). Mirrored in units/visual.ts for the 3D cards. */
const MALREC_SHEET_PX: Record<string, { body: number; feet: number }> = {
  "": { body: 296, feet: 12 },
  "idle2-": { body: 296, feet: 12 },
  "atk-": { body: 244, feet: 38 },
  "cast-": { body: 312, feet: 4 },
  "move-": { body: 668, feet: 7 },
  "move-left-": { body: 630, feet: 11 },
};
const MALREC_BODY_CELLS = 1.42 * 1.2 * 296 / 320;
/** Salazar V2's walk: his 36-frame loop plays at 1.9 s and he crosses a hex in 0.34 s (default
 * 0.22 s), a compromise — one loop of the authored stride covers only ~1 hex, so at playable speed
 * his feet still slide a little. Both scale with the speed mode like everyone else's walk. */
const SALAZAR_WALK_LOOP_SECONDS = 1.9;
const SALAZAR_STEP_PACE = 0.22 / 0.34;
// Sums of the supplied Minor Horror atlas JSON frame durations.
const MINOR_HORROR_SECONDS = { idle: 3.240, attack: 2.844, cast: 3.168, walk: 3.456, death: 3.924 };
/** Bow shots on a long sheet: normal ATT shots wait for the full sheet. Bow skills normally
 * release mid-sheet, except Neera's Special sheet, which must finish before her arrow leaves. */
const LONG_ARROW_RELEASE_SECONDS = LONG_ANIM_SECONDS;
const LONG_ARROW_SKILL_RELEASE_SECONDS = 2;

interface CombatAnim {
  type: "combat";
  att: string;
  def: string;
  stage: "lunge" | "hit" | "recover" | "counterLunge" | "counterHit" | "counterRecover" | "fade";
  t: number;
  swapped: boolean;
  bonusDice: number;
  bonusDiceCount: number;
  bonusFlat: number;
  noCounter: boolean;
  spellKind: SpellKind | null;
  customDice: { dice: number; faces: number; bonus: number } | null;
  counterCustomDice: { dice: number; faces: number; bonus: number } | null;
  /** Long-sheet bow counter: engine time the defender started drawing. The counter arrow
   * waits for the whole ATT sheet (LONG_ARROW_RELEASE_SECONDS) — see stepCombat. */
  counterWindAt?: number;
  dmgMul: number;
  stunChance: number;
  wallImpact: { dice: number; faces: number } | null;
  knockTo: Point | null;
  /** The attacker already played its full sheet in a wind-up: finish the rest of it, then hold the last frame. */
  held?: boolean;
  /** Seconds of the sheet the wind-up already played, and engine time the step started. */
  heldFrom?: number;
  heldAt?: number;
}

interface SpellAnim {
  type: "spell";
  att: string;
  /** The caster already played its full sheet in a wind-up: finish the rest of it, then hold the last frame. */
  held?: boolean;
  /** Seconds of the sheet the wind-up already played, and engine time the step started. */
  heldFrom?: number;
  heldAt?: number;
  tiles: Point[];
  ids: string[];
  t: number;
  hit: boolean;
  fireballVfxId?: string;
  fireballVfxLaunched?: boolean;
  fireballImpact?: boolean;
  fireballComplete?: boolean;
  causticVenomVfxId?: string;
  causticVenomImpact?: boolean;
  causticVenomComplete?: boolean;
  phantasmalVfxId?: string;
  phantasmalImpact?: boolean;
  phantasmalComplete?: boolean;
  blessVfxId?: string;
  blessComplete?: boolean;
  blessAppliedIds?: string[];
  magicMissileV2VfxId?: string;
  magicMissileV2Impact?: boolean;
  magicMissileV2Complete?: boolean;
  burningHandsVfxId?: string;
  burningHandsReleased?: boolean;
  burningHandsComplete?: boolean;
  varreduraVfxQueued?: boolean;
  cleaveVfxQueued?: boolean;
  extraDice: number;
  extraFaces: number;
  extraBonus: number;
  moreDice: number;
  moreFaces: number;
  echo: { dice: number; faces: number; bonus: number } | null;
  dmgMul: number;
  weaponBonusDice: number;
  weaponBonusFaces: number;
  weaponBonusBonus: number;
  spellKind: SpellKind | null;
  /** Exact hex a travelling spell projectile must reach before its AoE resolves. */
  projectileTo: Point | null;
  /** Caustic Venom: the one unit in `ids` that takes the bigger centerDice/Faces/Bonus roll
   * instead of the regular extraDice/Faces/Bonus splash roll — null for every other spell. */
  centerId: string | null;
  centerDice: number;
  centerFaces: number;
  centerBonus: number;
  /** Whether landing a hit also poisons the target (see startOfTurnEffects) — both sides,
   * Caustic Venom's splash spares no one. */
  poison: boolean;
  /** How hard the caster's own power lands for this spell. Above 1 for every spell, which
   * is what keeps a cast ahead of the plain hit the same unit could have made instead. */
  spellMul: number;
  /** The same, for Caustic Venom's centre hex, which is stronger than its splash. */
  centerMul: number;
}

interface HealAnim {
  type: "heal";
  att: string;
  /** The healer already played its full sheet in a wind-up: finish the rest of it, then hold the last frame. */
  held?: boolean;
  /** Seconds of the sheet the wind-up already played, and engine time the step started. */
  heldFrom?: number;
  heldAt?: number;
  def: string;
  kind: HealId;
  t: number;
  applied: boolean;
}

interface CureDiseaseAnim {
  type: "cureDisease";
  att: string;
  def: string;
  t: number;
  applied: boolean;
}

type Active =
  | MoveAnim
  | CombatAnim
  | SpellAnim
  | HealAnim
  | CureDiseaseAnim
  | { type: "banner"; text: string; t: number; dur: number }
  | { type: "delay"; t: number; dur: number }
  /** Long-sheet wind-up (see startSeq): the caster/archer plays its whole cast or attack
   * sheet before the spell, skill or arrow step it precedes is allowed to start. */
  | { type: "windup"; id: string; t: number; dur: number; pose: "cast" | "attack" | "specialAttack"; heal?: boolean };

/** Whether an action is a spell that deals no damage — heals, Bless, Cure Disease, Create Food
 * and Water — which plays the caster's healing sheet (GameArt.castsHeal) when it has one. */
function isSupportCast(a: { type: string; spellKind?: SpellKind | null }): boolean {
  return a.type === "heal" || a.type === "cureDisease" || (a.type === "spell" && !!a.spellKind && SPELL_CLASSIFICATION[a.spellKind] === "utility");
}

function pub(u: Unit, restrained: boolean, movLeft: number): UnitPublic {
  return {
    id: u.id,
    name: u.name,
    classId: u.classId,
    className: u.className,
    role: u.role,
    side: u.side,
    sprite: u.sprite,
    hp: u.hp,
    escaped: u.escaped,
    maxHp: u.maxHp,
    atk: u.atk,
    mag: u.mag,
    def: u.def,
    dex: u.dex,
    resistances: { ...u.resistances },
    weaponSkills: { ...u.weaponSkills }, healingSkill: u.healingSkill ?? 0,
    initiative: u.initiative,
    initiativeRoll: u.initiativeRoll,
    mov: u.mov,
    movLeft,
    minRange: u.minRange,
    maxRange: u.maxRange,
    moved: u.moved,
    acted: u.acted,
    x: u.x,
    y: u.y,
    level: u.level,
    xp: u.xp,
    bag: { ...u.bag },
    spells: { ...u.spells },
    frostCharges: u.frostCharges,
    weaponId: u.weaponId,
    weaponEnh: u.weaponEnh,
    size: u.size,
    diseased: u.diseased,
    poisoned: u.poisoned,
    poisonTier: u.poisonTier,
    poisonMag: u.poisonMag,
    bleeding: u.bleeding,
    bleedRoundsLeft: u.bleedRoundsLeft,
    blessedHitBonusPct: u.blessedHitBonusPct,
    blessedRoundsLeft: u.blessedRoundsLeft,
    shock: u.shock ? { ...u.shock } : null,
    fearTurns: u.fearTurns,
    fearSourceId: u.fearSourceId,
    stunned: u.stunned,
    crippled: u.crippled,
    hungry: u.hungerPenaltyPct > 0,
    hungerPct: Math.round(u.hungerPenaltyPct * 100),
    fullness: u.fullness,
    offHandId: u.offHandId,
    summoned: u.summoned,
    spellCharges: u.spellCharges,
    lifeDrainCharges: u.lifeDrainCharges,
    asleep: u.asleep,
    restrained,
    gear: { ...u.gear },
  };
}

/** Everything BattleEngine.computeUnitVisual derives about a unit's current animated pose —
 * shared between renderUnitsAndOverlays' own Canvas2D draw loop and ThreeBattleRenderer's unit
 * meshes (see the public unitVisual() wrapper) so the two never compute this differently. */
export interface UnitVisual {
  img: HTMLImageElement | undefined;
  w: number;
  h: number;
  /** How far below the unit's anchor position its feet sit (Y-down). */
  footY: number;
  bob: number;
  sway: number;
  breath: number;
  lift: number;
  /** The exact ctx.scale(scaleX, scaleY) factors renderUnitsAndOverlays applies — scaleX's
   * sign carries facing/mirroring, its magnitude (and scaleY) the breath squash/stretch. */
  scaleX: number;
  scaleY: number;
  /** Extra downward shift of the image's own box (cultistV2's cast pose only) — see
   * computeUnitVisual's comment on why its cast cut needs this. */
  footOffset: number;
}

interface Roster {
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

/** True once a hero is starving badly enough to be benched outright rather than merely
 * fighting at reduced stats — the party's hunger streak has hit its worst tier AND this
 * specific hero's own fullness is still at zero (feeding just them, even while the rest of
 * the party stays hungry, keeps them off this list). */
function heroUnconscious(name: string, roster?: Roster): boolean {
  if (!roster) return false;
  return (roster.hungerPenaltyPct ?? 0) >= HUNGER_PENALTY_MAX && fullness(roster.heroHunger?.[name]) <= 0;
}

/** Remaining uses for one spell tier at spawn — the class/level cap minus whatever the
 * roster says this hero already spent so far this scenario (see Roster.spellSpent), never
 * below 0. Always 0 for enemies, matching the previous unconditional side-check inline. */
function remainingTier(classId: ClassId, tier: SpellTier, key: TierKey, level: number, side: Unit["side"], roster: Roster | undefined, name: string): number {
  if (side !== "player") return 0;
  const cap = tierUses(classId, tier, level);
  const spent = roster?.spellSpent?.[name]?.[key] ?? 0;
  return Math.max(0, cap - spent);
}

// The mage line's own staves are pooled with the conjurer line's (ARCANE_ALL — any arcane
// caster can wield any arcane staff), so this bonus can't live on the weapon without also
// handing it to conjurer/sorcerer/necromancer. It's a trait of the mage class itself:
// applied after weapon-or-class range is resolved below, on top of either source.
// Voss is not in it: his attack reaches 2, his staff's own range.
const MAGE_RANGE_BONUS_CLASSES: ReadonlySet<ClassId> = new Set(["mage", "elementalist", "warlock"]);

// A named hero's own permanent look, independent of whatever classId they currently carry.
// Without this, a promoted hero (see PROMOTIONS — Aldric to Sentinel/Templar, Neera to
// Ranger/Assassin, ...) would render with the promoted class's own `sprite`, which is also
// the generic sprite every enemy of that same class uses — the hero would visually turn
// into a stock enemy unit the moment they promoted. classId itself still changes normally
// (stats, spells); only the sprite stays pinned to the hero's identity.
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

/** The name-pin above, with the mission editor's per-spawn escape hatch (Spawn.useClassSprite
 * — see its doc comment in types.ts) applied first: set, it renders with classId's own class
 * sprite instead, so any enemy/creature classId dropped into a hero-named slot actually shows
 * up as itself rather than snapping back to that hero's pinned look. */
/** Resolves the persistent visual identity used by battle units and out-of-battle hero views. */
export function heroSpriteFor(name: string, classSprite: SpriteId, useClassSprite?: boolean): SpriteId {
  if (useClassSprite) return classSprite;
  return HERO_SPRITE_BY_NAME[name] ?? classSprite;
}

function spawnUnit(spawn: Mission["playerSpawns"][number], side: Unit["side"], i: number, roster?: Roster, enemyLevel = 1): Unit {
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
  let weapon = side === "player" ? (roster?.weapons?.[spawn.name] ?? { id: starterWeaponFor(classId), enh: 0 }) : null;
  // Preserve owned items in old saves; only incompatible live wielding uses the class starter.
  if (weapon?.id && !WEAPONS[weapon.id]?.usableBy.includes(classId)) weapon = { id: starterWeaponFor(classId), enh: 0 };
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
  const gearBonus = gearStatBonus(Object.values(gear), weapon?.id, classId);
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

function unitFromSnap(snap: BattleUnitSnap): Unit {
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

/** Whether a unit gets its own turn. Neutrals hold their ground: they are placed, they can
 * be attacked, and they do nothing until something wakes them (see BattleEngine.provoke). */
function takesTurns(u: Unit): boolean {
  return u.alive && u.side !== "neutral";
}

/** Initiative modifier by archetype. Legacy class values are speed ranks, inverted into
 * bonuses; dedicated agile jobs deliberately sit above every other archetype. */
function initiativeBonus(classId: ClassId): number {
  if (classId === "assassin") return 12;
  if (classId === "rogue") return 11;
  if (classId === "archer" || classId === "neera" || classId === "ranger") return 10;
  return 11 - (CLASSES[classId].init ?? 10);
}

/** Whether the player may swing at this unit. Enemies always; wild neutrals too — that is
 * how a beast gets provoked in the first place. A neutral carrying a dialog tree is an NPC,
 * not a beast — never an attack target, clicking it opens the conversation instead (see
 * BattleEngine.handleCell). */
function attackableByPlayer(u: Unit): boolean {
  if (u.dialog) return false;
  return u.alive && (u.side === "enemy" || u.side === "neutral");
}

function easeOut(t: number): number {
  return 1 - (1 - t) * (1 - t);
}

export class BattleEngine {
  /** React preserves a running battle across development updates; refresh its methods too. */
  static refreshLiveEngine(engine: BattleEngine): void {
    Object.setPrototypeOf(engine, BattleEngine.prototype);
    engine.provokeFx ??= [];
    engine.supportAffinityRecipients ??= new Map();
    // fields added since the battle began: the new methods use them (a missing one threw
    // mid-commitMove and left the battle locked after every move)
    const added = engine as unknown as { warpedThisMove?: Set<string>; warpFloorHalo?: boolean };
    added.warpedThisMove ??= new Set();
    added.warpFloorHalo ??= true;
  }
  /** Stable sprite overlap order, independent of animation sway and camera depth. */
  readonly unitActionOrder = new Map<string, number>();
  private unitActionSerial = 0;
  noteUnitDrawAction(id: string): void {
    this.unitActionOrder.set(id, ++this.unitActionSerial);
  }
  readonly mission: Mission;
  readonly tiles: TerrainId[];
  /** Art variant index per tile, same indexing as tiles. Undefined/missing = variant 0. */
  readonly tileVariants: number[];
  /** How far each tile's art is turned, in sixths of a circle. */
  readonly tileRots: number[];
  readonly decorations: DecorationPlacement[];
  /** Permanent elemental GPU FX placed on this map in the editor — spawned once at battle
   * start and left running for the whole fight. See BattleCanvas/gfx.EffectsRenderer. */
  readonly elementalFxPlacements: ElementalFxPlacement[];
  /** `row * cols + col` keys of tiles whose photo art is suppressed in favor of a full-cover
   * WebGL water FX (water/water2 only — see the constructor and renderGround). */
  private readonly waterFxTileKeys: Set<number>;
  readonly cols: number;
  readonly rows: number;
  units: Unit[] = [];
  art: GameArt;
  phase: Phase = "player";
  mode: InputMode = "locked";
  turn = 1;
  selectedId: string | null = null;
  inspectedId: string | null = null;
  pendingFoeId: string | null = null;
  /** The staged attack (pendingFoeId) is the off-hand strike — see stageAttack. */
  private pendingAttackOffHand = false;
  threat: Point[] = [];
  cursor: Point = { x: 0, y: 0 };
  reach: Map<string, ReachCell> = new Map();
  attackFrom: Map<string, Point> = new Map();
  /** Active Web of Dreams patches (Conjurer tier 2) — cast, not terrain, so they live here
   * rather than on the map. Ticks down by one every startNewRound and is dropped at 0.
   * center/radius (the cast cell and hexAreaTiles' own radius) are the zone's hex-cluster
   * shape, kept alongside `cells` so BattleCanvas can size ONE WebGL "web" effect over the
   * whole zone (see webZoneRadiusTiles) instead of stamping a separate copy per hex — that
   * per-hex stamping used to be the only option and is what used to clash into a snowflake
   * cluster on anything bigger than a single hex. */
  webZones: { cells: Set<string>; roundsLeft: number; createdAt?: number; center?: Point; radius?: number; sleepChance?: number }[] = [];
  /** Active Ice Storm damage fields, fixed to the cells covered when cast. */
  iceStormZones: { cells: Set<string>; roundsLeft: number; createdAt: number; center: Point; radius: number; damageDice: number; damageFaces: number; damageMul: number; casterMag: number; casterLevel: number; casterId: string; side: Unit["side"] }[] = [];
  /** One-shot WebGL elemental FX spawn requests queued by a landed spell hit (see
   * SPELL_ELEMENT_FX/queueElementalFx) — BattleCanvas's render loop drains this every frame
   * and calls EffectsRenderer.spawnEffect for each, since `fx` itself only exists over there.
   * Each request self-expires after its own `duration`, so nothing here needs manual removal. */
  elementalFxRequests: { kind: ElementKind; x: number; y: number; duration: number }[] = [];
  /** Active Aura of Protection / Intimidating Presence zones (Paladin/Heavy Knight tier 5) —
   * same fixed-cells-at-cast-time, ticks-down-every-round shape as webZones. "protection"
   * cuts damage taken by units on the caster's own side standing in the zone; "intimidation"
   * raises damage taken by units on the OTHER side — see zoneDamageMul. */
  auraZones: { cells: Set<string>; roundsLeft: number; kind: "protection" | "intimidation"; side: Unit["side"]; pct: number }[] = [];
  /** Whether the unit whose turn is currently active was standing in a web zone at the
   * START of that turn — decided once in beginUnitTurn and left alone for the rest of it
   * (see effectiveUnitForReach). */
  private turnRestrained = false;
  /** Where the active unit stood when its turn began, and whether anything irreversible has
   * happened since — see undoMove. Cleared with the turn. */
  private turnStart: Point | null = null;
  private moveSpoiled = false;
  /**
   * Cached `occupancy` map plus the layout it describes, packed one int per unit.
   *
   * `occupancy` walks every living unit and expands its footprint into a fresh Map
   * keyed by string, and it was rebuilt at each of twenty-odd call sites — eight of
   * them inside one `spellAimValid`, which `render` runs every frame while a spell
   * is aimed. At a hundred-odd multi-hex units that dominates the frame.
   *
   * The guard compares the packed layout rather than counting a version: positions
   * and aliveness are mutated in place all over this file, so a counter would need
   * a bump at every one of those sites and one missed bump hands out a stale map —
   * a unit that reads as passable when it is not. Comparing is O(units) of integer
   * work against O(units x footprint) of Map building, so it still pays, and it
   * cannot go stale. Footprint shape is fixed at spawn, so it needs no stamp; a
   * summon changes the array length, which the compare catches.
   */
  /**
   * The two per-placement decoration switches, folded to one byte per cell.
   *
   * Rebuilt whenever the decoration list changes rather than consulted per query: a
   * rule asking about a hex must not walk every prop on the board to find out, and
   * `terrainDistanceField` asks about every cell six times over. Read through
   * `hexAt`, never directly.
   */
  private decorOverlay: DecorOverlay = new Uint8Array(0);
  private occCache: Map<string, Unit> | null = null;
  private occStamp: number[] = [];
  /**
   * Whole-board distance fields, one per player, shared by every enemy that runs its
   * AI against the same board (see playerDistanceFields).
   *
   * `terrainDistanceField` is a Dijkstra over every cell, and runAiFor built one per
   * player for each enemy in turn. Players cannot move during the enemy phase, so
   * all of those were the same field computed again and again: at 160x160 with a
   * hundred enemies and six players that is six hundred whole-board searches per
   * round, about 45 seconds of them. Six suffice.
   *
   * `terrainVersion` is bumped by the two things that reshape the board mid-battle —
   * a smashed barricade and an opened chest — since either changes path costs.
   */
  private fieldCache = new Map<string, Map<string, number>>();
  private fieldStamp = "";
  private terrainVersion = 0;
  /**
   * Fog of war, one byte per cell, row-major like `tiles`.
   *
   *   0 unseen   — never in sight; drawn as nothing at all
   *   1 explored — walked past and remembered: terrain draws dim, but whatever
   *                moves through it does not, because memory is not sight
   *   2 visible  — in sight of a living party member this instant
   *
   * Empty when `mission.fog` is off, and every read goes through `visible`/`explored`
   * which answer true for everything in that case, so the twenty missions that
   * shipped before fog behave exactly as they did.
   *
   * Recomputed when the party moves rather than per frame — sight only changes when
   * someone walks, dies or the board does (see refreshVisibility).
   */
  private vis: Uint8Array = new Uint8Array(0);
  private visStamp = "";
  /** Bumped every time the fog-of-war visibility grid actually changes — the renderer's fog
   * mask rebuilds only when this moves, never per frame. */
  visVersion = 0;
  /** Foes that have already spotted the party, so waking sticks. Ids rather than a
   * flag on Unit, which keeps it out of the per-unit save validation. */
  private awake = new Set<string>();
  /** All living combatants, mixed by their single battle-opening initiative roll. */
  private turnOrder: string[] = [];
  /** id of the unit whose turn we've already dispatched — lets the tick loop react only on change. */
  private activeUnitId: string | null = null;
  orig: Point | null = null;
  /** Movement already spent at the last cancel-safe point (turn start or completed action). */
  private origMoveBudgetUsed: number | null = null;
  hover: Point | null = null;
  private lastClickAt = 0;
  private lastClickCell: Point | null = null;
  result: "victory" | "defeat" | null = null;
  /** True when the mission's normal objective is complete or a waypoint is occupied. The
   * player confirms before ending; it flips back if a new enemy appears or the party leaves. */
  winAvailable = false;
  /** The waypoint a player unit is standing on, set alongside winAvailable by evaluateEnd.
   * The HUD and result screen use it to select the correct action and destination. */
  activeExit: DecorationPlacement | null = null;
  /** Recheck after each player movement settles, regardless of the mission's win condition. */
  private waypointCheckPending = false;
  banner: string | null = null;
  /** Ember found in chests opened mid-battle; folded into the save's Ember total on victory. */
  lootEmber = 0;
  /** Rations found in chests opened mid-battle (see useLockpick's 40% roll); folded into
   * the save's rations stock on victory, same as lootEmber. */
  lootRations = 0;
  /** Inn-quest pickups still lying on the map, and the keys collected so far this battle
   * (folded into save.questItems on victory, see GameApp's persistVictory). */
  questPickups: { key: string; name: string; x: number; y: number }[] = [];
  questFound: string[] = [];
  /** Weapon ids found in chests or off an enemy kill mid-battle; folded into the save's
   * weapon stash on victory. */
  /** Targets picked so far for a multi-missile Magic Missile, one per missile. Cleared
   * whenever aiming ends, so an abandoned cast never leaks into the next one. */
  private missileTargets: { id: string; cell: Point }[] = [];
  lootWeapons: string[] = [];
  /** EquipmentDef ids found in chests mid-battle; folded into the save's shared gear stash
   * on victory (save.looseEquipment) — never auto-equipped onto whoever opened the chest,
   * the player assigns it to a hero afterward from the Paperdoll picker. */
  lootEquipment: string[] = [];
  /** Every weapon id the player already owns, plus anything granted mid-battle the moment
   * it's granted — checked before every loot roll so a chest or kill drop never announces
   * a weapon the player already has (it used to: the roll didn't know about ownership at
   * all, so a "found" weapon could silently vanish once persistVictory deduped it against
   * the save, with nothing to show for the mid-battle "you found X" message). */
  private ownedWeapons: Set<string>;
  /** Rolling combat log — attacks, spells, heals, kills, and loot, newest last. Capped so
   * a long battle doesn't grow it without bound; read via getHud() for the in-battle log
   * view. */
  log: string[] = [];
  /** missionMapKey of the authored map this battle was built from — set by whoever starts the
   * battle, stamped into every snapshot (see BattleSnapshot.mapKey). */
  mapKey = "";
  /** The mission's intro dialog has already been shown for this fight. Saved in every
   * snapshot, so saving and loading never replays it (see BattleSnapshot.introDialogDone). */
  introDialogDone = false;
  /** See HudSnapshot.chestLoot — set the instant a chest opens, cleared only by
   * acknowledgeChestLoot() (the player's "Ok" on the popup), not by anything time-based. */
  private chestLoot: { unitName: string; ember: number; items: { name: string; icon: string; tip?: string }[] } | null = null;
  /** See HudSnapshot.pendingDialog — set the instant a dialog-bearing NPC is clicked,
   * cleared only by acknowledgeDialog() (the player closing the popup), same convention as
   * chestLoot above. */
  private pendingDialog: DialogTree | null = null;
  tip: string | null;
  private lastTipSeen: string | null = null;
  private tipSetAt = 0;
  time = 0;
  trauma = 0;
  hitstop = 0;
  zoom = 1;
  /** Camera elevation above the board plane, in degrees. */
  cameraTilt = 0;
  /** Horizontal camera orbit around the board center, in degrees. */
  cameraTiltSide = 0;
  /** Opt-in spatial terrain, upright sprites and scenery for the tactics camera. */
  tacticsCamera = false;
  /** How long a unit takes to glide across one hex — "normal" is the default, readable
   * pace; "fast" is the old, snappier speed for players who prefer it. Toggled from the
   * pause menu, applies to the very next step (mid-step changes aren't jarring since a
   * step is at most a quarter second). */
  speedMode: "slow" | "normal" | "fast" = "normal";
  camX = 0;
  camY = 0;
  /** The Three scene owns architecture even while sprite props move to the FX overlay. */
  architectureRenderedInThree = false;
  /** Off (0) in battle. The editor preview opts into a small edge rim so camera focus near
   * the board boundary does not expose half a viewport of empty void. */
  private previewPanMarginRadii = 0;
  private viewW = 1;
  private viewH = 1;
  private camReady = false;
  /** This frame's screen-shake offset, rolled once in renderGround and reused (not
   * re-rolled) by renderUnitsAndOverlays, so the two layers shake together instead of
   * jittering apart when they're drawn onto separate canvases (see BattleCanvas's FX
   * overlay) — two independent `Math.random()` calls would desync them. */
  private frameShakeDx = 0;
  private frameShakeDy = 0;
  private queue: Seq[] = [];
  private active: Active | null = null;
  /** Queue steps whose long-sheet wind-up already played (see startSeq), with how many
   * seconds of the sheet it covered — the step picks the pose up from there. */
  private woundUp = new WeakMap<Seq, number>();
  /** Monster hit/death cues already played, per unit (see tick and audio.ts MONSTER_SFX). */
  private readonly monsterSoundCues = new WeakMap<Unit, { hitAt?: number; death?: boolean }>();
  /** Hits taken so far, per unit, for sprites with a second hit sheet (see hitPoolFor). */
  private readonly hitSheetCounts = new WeakMap<Unit, { hitAt: number; count: number }>();
  /** Plague Bearing Cattle death cues already played (see tick). */
  private readonly cattleDeathCued = new WeakSet<Unit>();
  /** Carnivorous Plant hit/death cues already played, per unit (see tick). */
  private plantSoundCues = new WeakMap<Unit, { hitAt?: number; death?: boolean }>();
  /** Work a queued step defers until it really starts (after any wind-up) — e.g. a summon's
   * familiar appearing. Run once, then dropped. */
  private onSeqStart = new WeakMap<Seq, () => void>();
  /** Units already charged Bleeding for the action currently playing (see startSeq). */
  private bleedChargedIds = new Set<string>();
  /** Queued move steps that are Bull Rush charges (see castBullRush / startSeq). */
  private chargeMoves = new WeakSet<Seq>();
  private particles: Particle[] = Array.from({ length: PARTICLE_CAP }, blankParticle);
  private particleLive = 0;
  private levelUpFx: LevelUpSpark[] = Array.from({ length: LEVEL_UP_FX_CAP }, blankLevelUpSpark);
  private levelUpFxLive = 0;
  private missileFx: MissileFx[] = Array.from({ length: MISSILE_FX_CAP }, blankMissileFx);
  private missileFxLive = 0;
  private fireballBurstFx: FireballBurstFx[] = Array.from({ length: FIREBALL_BURST_CAP }, blankFireballBurstFx);
  private fireballBurstFxLive = 0;
  private lightningFx: LightningFx[] = Array.from({ length: LIGHTNING_FX_CAP }, blankLightningFx);
  private lightningFxLive = 0;
  private holyFx: HolyFx[] = Array.from({ length: HOLY_FX_CAP }, blankHolyFx);
  private holyFxLive = 0;
  private turnUndeadFx: { tiles: Point[]; t: number }[] = [];
  private provokeFx: { unitId: string; t: number }[] = [];
  private bladeFx: BladeFx[] = Array.from({ length: BLADE_FX_CAP }, blankBladeFx);
  private bladeFxLive = 0;
  private portalFx: PortalFx[] = Array.from({ length: PORTAL_FX_CAP }, blankPortalFx);
  private portalFxLive = 0;
  /** Every open Warp pair: a mage can keep several open at once, each on its own round timer. */
  private warpGates: WarpGatePair[] = [];
  /** Units a gate carried across during the move now playing (commitMove's onNextIdle). */
  private readonly warpedThisMove = new Set<string>();
  /** Warp's flat floor glow and floor reflection under the gate. The 3D battle draws the gate on
   * an upright sheet, where they read as a horizontal halo across it, so it turns them off. */
  warpFloorHalo = true;
  /** 0-1: how visible the movement/range grid is right now (see OVERLAY_FADE_IN). Read by
   * both renderers when they draw boardOverlayLayers. */
  overlayFade = 1;
  /** Flips each time Double Strike lands, so its two hits swoosh opposite diagonals and read
   * as one crossing pair of slashes rather than the same cut drawn twice. */
  private doubleStrikeAlt = false;
  private onNextIdle: (() => void) | null = null;
  private rng: () => number;
  private listeners = new Set<() => void>();
  private reducedMotion = false;
  private layout: Layout = { ox: 0, oy: 0, tile: 48, cols: 8, rows: 7 };
  private spellArmed = false;
  private spellAim: Point | null = null;
  private spellKind: SpellKind | null = null;
  /** Set while mode === "awaitPotion": which potion the selected unit is about to use on
   * whichever valid target (self or an adjacent ally — see confirmPotionAt) is tapped next. */
  private potionAim: PotionId | null = null;
  /** After a mid-battle load, the next beginUnitTurn must not re-run start-of-turn effects
   * (echo, poison, stun skip) — those already happened on the turn we saved in the middle of. */
  private skipStartOfTurn = false;
  /** Test-mode-only: drops every ally/enemy/condition restriction on who a spell can target
   * (targetable's side check, healing/curing an enemy, aiming at a full-HP or undiseased
   * unit) so a debug session can freely fire any spell at any unit just to look at its FX,
   * without the normal "that's not a valid target" gameplay rules getting in the way. Wired
   * from GameApp's testMode — never true for a real save. */
  private debugFreeCast = false;
  readonly frostVfxRequests: { origin:Point; cells:Point[]; seed:number }[] = [];
  readonly fireballVfxRequests: FireballVfxRequest[] = [];
  readonly fireballVfxEvents: FireballVfxEvent[] = [];
  fireballVfxAvailable = false;
  private fireballVfxSequence = 0;
  readonly causticVenomVfxRequests: CausticVenomVfxRequest[] = [];
  readonly causticVenomVfxEvents: CausticVenomVfxEvent[] = [];
  causticVenomVfxAvailable = false;
  private causticVenomVfxSequence = 0;
  readonly phantasmalForceVfxRequests: PhantasmalForceVfxRequest[] = [];
  readonly phantasmalForceVfxEvents: PhantasmalForceVfxEvent[] = [];
  phantasmalForceVfxAvailable = false;
  private phantasmalForceVfxSequence = 0;
  readonly blessVfxRequests: BlessVfxRequest[] = [];
  readonly blessVfxEvents: BlessVfxEvent[] = [];
  readonly blessTimelineEvents: { id: string; event: BlessVfxTimelineEvent; unitId?: string }[] = [];
  blessVfxAvailable = false;
  private blessVfxSequence = 0;
  readonly magicMissileV2VfxRequests: MagicMissileV2VfxRequest[] = [];
  readonly magicMissileV2VfxEvents: MagicMissileV2VfxEvent[] = [];
  readonly magicMissileV2TimelineEvents: { id: string; event: MagicMissileV2TimelineEvent; index?: number }[] = [];
  magicMissileV2VfxAvailable = false;
  private magicMissileV2VfxSequence = 0;
  readonly burningHandsV2VfxRequests: BurningHandsV2VfxRequest[] = [];
  readonly burningHandsV2VfxEvents: BurningHandsV2VfxEvent[] = [];
  burningHandsV2VfxAvailable = false;
  private burningHandsV2VfxSequence = 0;
  readonly varreduraVfxRequests: VarreduraVfxRequest[] = [];
  private varreduraVfxSequence = 0;
  readonly cleaveVfxRequests: CleaveVfxRequest[] = [];
  private cleaveVfxSequence = 0;

  affinityScores: Record<string, number> = {};
  heroSkills: HeroSkills = {};
  partyLeader = "Kael";
  private supportAffinityRecipients = new Map<string, Set<string>>();

  private adjacentAllies(u: Unit): Unit[] {
    return this.units.filter(ally => ally.id !== u.id && ally.alive && ally.side === "player" && !ally.summoned
      && footprint(u).some(a => footprint(ally).some(b => hexDist(a, b) === 1)));
  }

  private affinityUnit(u: Unit): Unit {
    if (u.side !== "player" || u.summoned || !u.alive) return u;
    const bonus = Math.max(0, ...this.adjacentAllies(u).map(ally => affinityBonus(affinityScore(this.affinityScores, u.name, ally.name))));
    return bonus ? { ...u, atk: u.atk * (1 + bonus), mag: u.mag * (1 + bonus), def: u.def * (1 + bonus), dex: u.dex * (1 + bonus) } : u;
  }

  /** Enmity (enmity.ts): each enemy's score per player-side unit. Drives enemy targeting. */
  private enmity: EnmityTable = new Map();

  private addEnmity(foe: Unit, hero: Unit, ce: number, ve: number): void {
    if (foe.side !== "enemy" || hero.side !== "player" || !foe.alive || !hero.alive) return;
    const row = this.enmity.get(foe.id) ?? new Map<string, EnmityEntry>();
    row.set(hero.id, addToEntry(row.get(hero.id), ce, ve));
    this.enmity.set(foe.id, row);
  }

  /** Damage in either direction: a hero hurting an enemy raises their enmity with it (spells
   * more than weapons); an enemy hurting a hero wears that hero's cumulative enmity down. */
  private noteDamageEnmity(attacker: Unit, target: Unit, dmg: number, kind: "weapon" | "spell"): void {
    if (dmg <= 0) return;
    if (attacker.side === "player" && target.side === "enemy") {
      const k = ENMITY[kind];
      this.addEnmity(target, attacker, dmg * k.ce, dmg * k.ve);
    } else if (attacker.side === "enemy" && target.side === "player") {
      const entry = this.enmity.get(attacker.id)?.get(target.id);
      if (entry) this.enmity.get(attacker.id)!.set(target.id, addToEntry(entry, -dmg * ENMITY.damageTakenCe, 0));
    }
  }

  /** Heals and support skills are noticed by every enemy already aware of the party. */
  private noteAwareEnmity(hero: Unit, ce: number, ve: number): void {
    if (hero.side !== "player") return;
    for (const foe of this.units) {
      if (foe.side !== "enemy" || !foe.alive) continue;
      if (this.fogged && !this.awake.has(foe.id)) continue;
      this.addEnmity(foe, hero, ce, ve);
    }
  }

  /** Whoever tops this enemy's enmity table, or null while it has none (usual targeting). */
  private enmityTarget(foe: Unit): Unit | null {
    const row = this.enmity.get(foe.id);
    if (!row) return null;
    let best: Unit | null = null;
    let bestTotal = 0;
    for (const [heroId, entry] of row) {
      const hero = this.units.find((u) => u.id === heroId);
      if (!hero || !hero.alive || hero.side !== "player") continue;
      const total = enmityTotal(entry);
      if (total > bestTotal) {
        best = hero;
        bestTotal = total;
      }
    }
    return best;
  }

  private adjustAffinity(a: Unit, b: Unit, delta: number): void {
    if (a.id === b.id || a.side !== "player" || b.side !== "player" || a.summoned || b.summoned) return;
    this.adjustHeroAffinity(a.name, b.name, delta, false);
  }

  private gainSupportAffinity(actor: Unit, target: Unit): void {
    if (actor.side !== "player" || target.side !== "player" || actor.summoned || target.summoned) return;
    const recipients = this.supportAffinityRecipients.get(actor.id) ?? new Set<string>();
    if (recipients.has(target.id)) return;
    recipients.add(target.id);
    this.supportAffinityRecipients.set(actor.id, recipients);
    this.adjustAffinity(actor, target, 0.2);
  }

  /** Each completed action builds affinity with heroes fighting beside its actor. */
  private gainAdjacentAffinity(u: Unit): void {
    if (u.acted || !u.alive || u.side !== "player" || u.summoned) return;
    const recipients = this.supportAffinityRecipients.get(u.id);
    for (const ally of this.adjacentAllies(u)) {
      if (!recipients?.has(ally.id)) this.adjustAffinity(u, ally, 0.1);
    }
    this.supportAffinityRecipients.delete(u.id);
  }

  private adjustHeroAffinity(a: string, b: string, delta: number, dialogue = true): void {
    if (a === b || delta === 0 || !AFFINITY_HEROES.includes(a as AffinityHero) || !AFFINITY_HEROES.includes(b as AffinityHero)) return;
    if (dialogue && delta > 0 && (a === this.partyLeader || b === this.partyLeader)) {
      delta = Math.round(delta * (2 - affinityScore(this.affinityScores, a, b) / 100) * 100) / 100;
    }
    this.affinityScores = changeAffinity(this.affinityScores, a as AffinityHero, b as AffinityHero, delta);
    this.pushLog(`Afinidade ${a} + ${b}: ${delta > 0 ? "+" : ""}${delta.toLocaleString("pt-BR")}`);
  }

  applyDialogAffinity(reply: { affinity?: { from: string; to: string; delta: number } }): void {
    if (reply.affinity) this.adjustHeroAffinity(reply.affinity.from, reply.affinity.to, reply.affinity.delta);
  }

  constructor(mission: Mission, art: GameArt, roster: Roster, seed = 1, debugFreeCast = false) {
    this.affinityScores = cleanAffinityScores(roster.affinityScores);
    this.heroSkills = cleanHeroSkills(roster.heroSkills, Object.keys(roster.heroSkills ?? {}));
    this.partyLeader = roster.partyLeader ?? "Kael";
    this.debugFreeCast = debugFreeCast;
    this.mission = mission;
    this.art = art;
    this.ownedWeapons = new Set(roster.ownedWeaponIds ?? []);
    this.cols = mission.cols;
    this.rows = mission.rows;
    this.tiles = parseLayout(mission.layout);
    this.tileVariants = mission.tileVariants ?? [];
    this.tileRots = mission.tileRots ?? [];
    this.decorations = (mission.decorations ?? []).map((d) => ({ ...d }));
    this.decorations = closeWatchtowerWalls(mission.id, this.tiles, this.cols, this.rows, this.decorations);
    this.tiles = clearRockColumnTiles(this.tiles, this.cols, this.rows, this.decorations, mission.baseTile);
    this.elementalFxPlacements = (mission.elementalFx ?? []).map((p) => ({ ...p }));
    // A tile under a full-coverage water FX placement (water/water2) skips its own photo
    // tile art entirely — see renderGround. That art is one of 22 independently-centered
    // variants (assets.ts TILE_VARIANT_COUNT.water), so two neighboring water hexes almost
    // always draw two different, unaligned photos and the grid seam is baked into the art
    // itself; a shader overlay tinting/refracting that art can never hide the mismatch. The
    // WebGL FX is the entire visual for those hexes instead of a glaze on top of one.
    // Shore/Shore2 are deliberately excluded: they only cover HALF their hex (the shader
    // draws its own flat sand color on the dry half, fading fully transparent past the tide
    // line), so they still need the real land art showing through underneath.
    const WATER_FAMILY = new Set(["water", "water2"]);
    this.waterFxTileKeys = new Set(
      this.elementalFxPlacements.filter((p) => WATER_FAMILY.has(p.kind)).map((p) => p.y * this.cols + p.x),
    );
    // Art is loaded once at boot — a decoration added later (or after HMR) is in
    // DECORATIONS and in the editor <img>, but missing from art.decorations, so combat
    // used to skip it. Fill any hole so Testar paints the same props the editor lists.
    for (const p of this.decorations) {
      if (DECORATIONS[p.id]?.model3d) continue;
      const artId = decorationPlacementArt(p);
      if (this.art.decorations[artId]?.naturalWidth) continue;
      const img = new Image();
      img.src = decorationImage(artId);
      decorationImageRetryWebp(img, artId);
      this.art.decorations[artId] = img;
    }
    // A decoration that names a tile (barricade, locked chest, rocks) stamps that terrain so
    // the picture and the rules cannot disagree. A prop with no tile — tree, fallen log —
    // sits on whatever hex was already painted. The old fallback to "column" is what made
    // trunks show up as marble pillars in playtest.
    for (const p of this.decorations) {
      const def = DECORATIONS[p.id];
      if (!def?.tile) continue;
      for (const { dx, dy } of placedFootprint(p)) {
        const x = p.x + dx;
        const y = p.y + dy;
        if (x >= 0 && x < this.cols && y >= 0 && y < this.rows) this.tiles[y * this.cols + x] = def.tile;
      }
    }
    // Debug-only, off by default: most 2D "water" TERRAIN tiles on the actual maps have no
    // water/water2 FX object placed on top at all, so only the ~25 hexes a designer happened
    // to hand-place one on get the animated WebGL surface — every other water tile just shows
    // its flat, static photo art. Runs after the decoration tile-stamping above (not before —
    // an earlier version of this ran before that stamping and so could read a hex's pre-stamp
    // tile, e.g. a decoration whose def.tile turns its hex into/out of "water" after this would
    // have already decided). This block only ever PUSHES a brand-new placement for a water tile
    // that has none of its own AND has no decoration on it (a "water" FX is a fully opaque
    // full-hex quad composited over the already-rendered 2D scene — see gfx/shaders.ts WATER
    // branch and EffectsRenderer's u_scene upload — so adding one under an existing decoration
    // would bury it, leaving only a faint trace via the shader's own scene-reflection term); it
    // never reads back or mutates an existing FX placement (of any kind), so with the flag off,
    // on any hex a designer already gave an FX to, or on any decorated hex, behavior is
    // unchanged. Toggle via devtools: localStorage.setItem("emberash:landShoreFx", "1") then
    // reload, "0" (or removed) to undo.
    if (this.landShoreFxDebugEnabled()) {
      const occupied = new Set(this.elementalFxPlacements.map((p) => p.y * this.cols + p.x));
      for (const p of this.decorations) {
        for (const { dx, dy } of placedFootprint(p)) occupied.add((p.y + dy) * this.cols + (p.x + dx));
      }
      for (let wy = 0; wy < this.rows; wy++) {
        for (let wx = 0; wx < this.cols; wx++) {
          const key = wy * this.cols + wx;
          if (this.tiles[key] !== "water" || occupied.has(key)) continue;
          this.elementalFxPlacements.push({ id: `fx-synth-water-${key}`, kind: "water", x: wx, y: wy });
        }
      }
      this.waterFxTileKeys = new Set(
        this.elementalFxPlacements.filter((p) => WATER_FAMILY.has(p.kind)).map((p) => p.y * this.cols + p.x),
      );
    }
    this.decorations.push(...barricadeDecor(this.tiles, this.cols, this.rows, this.decorations));
    this.refreshDecorOverlay();
    this.rng = mulberry32(seed + mission.index * 97);
    const defeatedCrossingSpawns = new Set(roster.crossingDefeatedSpawns ?? []);
    this.questPickups = (roster.questPickups ?? []).map((p) => ({ ...p }));
    this.units = [
      ...mission.playerSpawns.filter((s) => !heroUnconscious(s.name, roster)).map((s, i) => spawnUnit(s, "player", i, roster)),
      ...mission.enemySpawns.flatMap((s, i) => {
        const id = `enemy-${s.name}-${i}`;
        return defeatedCrossingSpawns.has(id) ? [] : [spawnUnit(s, "enemy", i, roster, enemyLevelFor(mission.index))];
      }),
      ...(this.uniqueNeutralNpcSpawns(mission.neutralSpawns ?? [])).flatMap(({ spawn: s, index: i }) => {
        const id = `neutral-${s.name}-${i}`;
        return defeatedCrossingSpawns.has(id) ? [] : [spawnUnit(s, "neutral", i, roster, enemyLevelFor(mission.index))];
      }),
    ];
    for (const u of this.units) {
      this.nudgeOffHazard(u);
      if (u.side === "player") this.nudgeOffWaypoint(u);
      u.bob = this.rng() * 16;
    }
    this.rollOpeningInitiative(this.units.filter(takesTurns));
    this.turnOrder = this.sortByInitiative(this.units.filter(takesTurns));
    const first = this.units.find((u) => u.side === "player");
    if (first) this.cursor = { x: first.x, y: first.y };
    this.tip =
      mission.index === 0
        ? "Toque numa aliada para mover. Toque num inimigo para ver HP e alcance."
        : mission.win === "boss"
          ? "Objetivo: o capitão. Toque nele para ver a área de perigo."
          : "Toque num inimigo para ver HP, alcance e onde ele pode atacar.";
    if (typeof window !== "undefined" && window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      this.reducedMotion = true;
    }
    // The heroUnconscious filter above can (rarely) bench every last player spawn — the
    // whole party starved out at once — leaving no one to take a turn. Catch that here
    // rather than softlocking on a battle nobody can ever act in.
    this.evaluateEnd();
  }

  /** One battle-opening roll per unit: 1d20 + its archetype modifier. The class data's
   * historical scale is a delay (1 fast .. 10 slow), so 11-delay is the additive bonus. */
  private rollOpeningInitiative(units: Unit[]): void {
    for (const unit of units) {
      unit.initiativeRoll = 1 + Math.floor(this.rng() * 20);
      unit.initiative = unit.initiativeRoll + initiativeBonus(unit.classId);
    }
  }

  /** Highest opening initiative first. On an exact draw, the player wins. */
  private sortByInitiative(units: Unit[]): string[] {
    return [...units]
      .sort((a, b) => {
        if (a.initiative !== b.initiative) return b.initiative - a.initiative;
        if (a.side !== b.side) return a.side === "player" ? -1 : 1;
        return a.id.localeCompare(b.id);
      })
      .map((u) => u.id);
  }

  subscribe(fn: () => void): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private emit(): void {
    for (const fn of this.listeners) fn();
  }

  /** How many targets an aimed spell still wants, for a spell that picks more than one.
   *
   * Magic Missile fires 2 missiles at level 3 and 3 at level 6, each aimed separately, and
   * the only thing that ever said so was the tip line at the bottom of the screen — small,
   * grey, and easy to walk straight past while wondering why the spell hasn't gone off. The
   * HUD puts this where it has to be read. Null for a single-target cast, which needs no
   * counting. */
  private targetPrompt(): { name: string; need: number; picked: number } | null {
    if (this.spellKind !== "magicMissile") return null;
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u) return null;
    const need = magicMissileCount(u.level);
    return need > 1 ? { name: MAGIC_MISSILE.name, need, picked: this.missileTargets.length } : null;
  }

  getHud(): HudSnapshot {
    const selected = this.units.find((u) => u.id === this.selectedId) ?? null;
    const hoverCell = this.hover ?? this.cursor;
    const hoverUnit = hoverCell
      ? this.units.find((u) => u.alive && occupies(u, hoverCell.x, hoverCell.y))
      : undefined;
    // Void is erased ground — no terrain card for empty space. Nor for a hex under fog of war
    // the party has never seen: its terrain is exactly what the player must not know yet.
    const terr =
      hoverCell && tileAt(this.tiles, this.cols, hoverCell.x, hoverCell.y) !== "void" && this.explored(hoverCell.x, hoverCell.y)
        ? this.hexAt(hoverCell.x, hoverCell.y)
        : null;
    const hoveredWeb = hoverCell
      ? this.webZones
          .filter((zone) => zone.cells.has(key(hoverCell.x, hoverCell.y)))
          .reduce<(typeof this.webZones)[number] | null>((best, zone) => !best || zone.roundsLeft > best.roundsLeft ? zone : best, null)
      : null;
    const hoveredIceStorm = hoverCell
      ? this.iceStormZones
          .filter((zone) => zone.cells.has(key(hoverCell.x, hoverCell.y)))
          .reduce<(typeof this.iceStormZones)[number] | null>((best, zone) => !best || zone.roundsLeft > best.roundsLeft ? zone : best, null)
      : null;
    const inspected = this.units.find((u) => u.id === this.inspectedId) ?? null;
    const pendingFoe = this.units.find((u) => u.id === this.pendingFoeId) ?? null;
    // A foe out of sight gets no damage forecast either — the HUD must not leak what
    // the board is hiding.
    const foeForForecast = pendingFoe ?? (this.targetable(inspected ?? undefined) ? inspected : null);
    let forecast: Forecast | null = null;
    if (selected && foeForForecast && selected.side === "player") {
      const offHandItem = EQUIPMENT[selected.offHandId ?? ""];
      const offHandAttack = pendingFoe
        ? this.pendingAttackOffHand
        : this.mode === "awaitOffHand" || (this.mode !== "awaitSpell" && offHandItem?.kind === "weapon" && this.isArrowAttack(selected) && hexDist(selected, foeForForecast) <= (offHandItem.maxRange ?? 1));
      // Off-hand actions strike from the current cell; main-hand movement forecasts can
      // change adjacency bonuses and must not be reused for a dagger or shield strike.
      const from = offHandAttack ? undefined : this.attackFrom.get(foeForForecast.id);
      const fx = from?.x ?? selected.x;
      const fy = from?.y ?? selected.y;
      const fake = { ...selected, x: fx, y: fy };
      forecast = makeForecast(
        this.affinityUnit(fake),
        this.affinityUnit(foeForForecast),
        tileAt(this.tiles, this.cols, fx, fy),
        tileAt(this.tiles, this.cols, foeForForecast.x, foeForForecast.y),
        this.tiles,
        this.cols,
        offHandAttack && offHandItem?.kind === "weapon",
        !(offHandAttack && offHandItem?.kind === "shield") && (this.mode !== "awaitSpell" || isWeaponAbility(this.spellKind)),
      );
    }
    const canAttack =
      !!selected &&
      !selected.acted &&
      (this.attackFrom.size > 0 ||
        this.units.some((u) => u.alive && u.side !== selected.side && canHitFrom(selected, selected, u, this.tiles, this.cols, this.decorOverlay)));
    const canLockpick = !!selected && !selected.acted && selected.bag.lockpick > 0 && !!this.adjacentLock(selected);
    const offHandKind: "weapon" | "shield" | null =
      selected && !selected.acted && selected.offHandId ? (EQUIPMENT[selected.offHandId]?.kind ?? null) : null;
    return {
      phase: this.phase,
      banner: this.banner,
      selected: selected ? pub(this.affinityUnit(selected), this.isWebCell(selected.x, selected.y), this.movLeft(selected)) : null,
      hoveredUnit: hoverUnit ? pub(hoverUnit, this.isWebCell(hoverUnit.x, hoverUnit.y), this.movLeft(hoverUnit)) : null,
      terrain: terr
        ? {
            id: terr.id,
            name: terr.name,
            moveCost: terr.moveCost,
            def: terr.def,
            atk: terr.atk,
            passable: terr.passable,
            blocksShot: !!terr.blocksShot,
            hazard: terr.hazardDice ? `${terr.hazardDice}d${terr.hazardFaces ?? 8}` : undefined,
            note: hoveredWeb || hoveredIceStorm ? undefined : terrainNote(terr.id),
            spellZone: hoveredWeb
              ? {
                  kind: "webOfDreams" as const,
                  roundsLeft: hoveredWeb.roundsLeft,
                  movementCap: 1,
                  sleepChance: hoveredWeb.sleepChance ?? WEB_OF_DREAMS.sleepChance,
                  sleepDice: diceFormula(WEB_OF_DREAMS.sleepDice, WEB_OF_DREAMS.sleepFaces, 0),
                }
              : hoveredIceStorm
                ? {
                    kind: "iceStorm" as const,
                    roundsLeft: hoveredIceStorm.roundsLeft,
                    damageFormula: iceStormFormula(hoveredIceStorm.casterLevel, hoveredIceStorm.casterMag),
                  }
                : undefined,
          }
        : null,
      mode: this.mode,
      canAttack,
      offHandKind,
      canLockpick,
      forecast,
      turn: this.turn,
      objective: this.mission.objective,
      missionTitle: this.mission.title,
      playerAlive: this.units.filter((u) => u.side === "player" && u.alive && !u.summoned).length,
      // The HUD may only count enemies the party can currently see while fog is active.
      enemyAlive: this.units.filter((u) => u.side === "enemy" && u.alive && !this.unitHidden(u)).length,
      busy: this.mode === "locked" || !!this.active || this.queue.length > 0,
      canCancelMovement:
        (this.active?.type === "move" &&
          !!this.selectedId &&
          this.active.id === this.selectedId &&
          this.units.some((u) => u.id === this.selectedId && u.side === "player" && u.alive)) ||
        this.canCancelCommittedMovement(),
      result: this.result,
      winAvailable: this.winAvailable,
      activeExit: this.activeExit,
      canUndoMove: this.canUndoMove(),
      targetPrompt: this.targetPrompt(),
      zoom: this.zoom,
      speedMode: this.speedMode,
      tip: this.tip,
      inspected: inspected
        ? pub(inspected, this.isWebCell(inspected.x, inspected.y), this.movLeft(inspected))
        : pendingFoe
          ? pub(pendingFoe, this.isWebCell(pendingFoe.x, pendingFoe.y), this.movLeft(pendingFoe))
          : null,
      pendingFoe: pendingFoe ? pub(pendingFoe, this.isWebCell(pendingFoe.x, pendingFoe.y), this.movLeft(pendingFoe)) : null,
      spellReady:
        this.mode === "awaitSpell" &&
        !!selected &&
        (this.spellKind === "sweep" || this.spellKind === "turnUndead" || (!!this.hover && this.spellAimValid(selected, this.hover))),
      spellArmed: this.mode === "awaitSpell" && !!selected && this.spellArmed && !!this.spellAim,
      spellHitChance: (() => {
        // Weapon skills roll the same weapon hit chance as a basic attack (see the forecast above).
        if (this.mode !== "awaitSpell" || !selected || !this.spellArmed || !this.spellAim || !isWeaponAbility(this.spellKind)) return null;
        const foe = this.occ().get(key(this.spellAim.x, this.spellAim.y));
        if (!foe || foe.side === selected.side || !this.targetable(foe)) return null;
        return makeForecast(
          this.affinityUnit(selected),
          this.affinityUnit(foe),
          tileAt(this.tiles, this.cols, selected.x, selected.y),
          tileAt(this.tiles, this.cols, foe.x, foe.y),
          this.tiles,
          this.cols,
          false,
          true,
        ).hitOut;
      })(),
      spellKind: this.mode === "awaitSpell" ? this.spellKind : null,
      turnQueue: (() => {
        const active = this.activeTurnUnit();
        return this.turnOrder
          .map((id) => this.units.find((u) => u.id === id))
          // Fog of war: an enemy the party can't currently see stays off the list too, so the
          // turn order never gives away who is out there (unitHidden is false without fog).
          .filter((u): u is Unit => !!u && u.alive && !this.unitHidden(u))
          .map((u) => ({ id: u.id, name: u.name, side: u.side, acted: u.moved, active: u.id === active?.id, initiative: u.initiative }));
      })(),
      log: this.log,
      chestLoot: this.chestLoot,
      pendingDialog: this.pendingDialog,
    };
  }

  /** The player's "Ok" on the chest loot popup — see HudSnapshot.chestLoot. */
  acknowledgeChestLoot(): void {
    this.chestLoot = null;
  }

  /** Opens an NPC's conversation — see handleCell's dialog branch. Always starts fresh from
   * the tree's own startId; NPC dialog replays in full every time, no "already talked to"
   * state kept. */
  private openDialog(tree: DialogTree): void {
    this.pendingDialog = tree;
  }

  /** The player closing the dialog popup — see HudSnapshot.pendingDialog. */
  acknowledgeDialog(): void {
    this.pendingDialog = null;
  }

  battlePlayerHunger(): Record<string, number> {
    return Object.fromEntries(this.units.filter((u) => u.side === "player" && !u.summoned).map((u) => [u.name, u.fullness]));
  }

  battlePlayerHp(): Record<string, number> {
    const out: Record<string, number> = {};
    for (const u of this.units) {
      if (u.side !== "player" || u.summoned) continue;
      out[u.name] = u.alive ? u.hp : 0;
    }
    return out;
  }

  remainingPlayerHp(): Record<string, number> {
    const out: Record<string, number> = {};
    for (const u of this.units) {
      if (u.side !== "player" || u.summoned) continue;
      if (!u.alive) out[u.name] = Math.max(1, Math.ceil(u.maxHp * 0.5));
      else out[u.name] = Math.min(u.maxHp, u.hp + Math.ceil((u.maxHp - u.hp) * 0.5));
    }
    return out;
  }

  remainingBags(): Record<string, Bag> {
    const out: Record<string, Bag> = {};
    for (const u of this.units) {
      if (u.side !== "player" || u.summoned) continue;
      out[u.name] = { ...u.bag };
    }
    return out;
  }

  /** Hands a found potion to `starter` (the one who opened the chest), or — if their bag for
   * that kind is already full — to the next living party member who will act, walking the
   * current initiative order and wrapping into the next round. Returns the unit who took it,
   * or null if the whole party is capped out so the drop is discarded. */
  private givePotion(starter: Unit, kind: PotionId): Unit | null {
    const cap = POTION_CARRY_MAX[kind];
    const orderIds = this.turnOrder.length > 0 ? this.turnOrder : this.units.map((u) => u.id);
    const startIdx = Math.max(0, orderIds.indexOf(starter.id));
    const sequenced: Unit[] = [];
    const seen = new Set<string>();
    for (let i = 0; i < orderIds.length; i++) {
      const id = orderIds[(startIdx + i) % orderIds.length]!;
      const u = this.units.find((x) => x.id === id);
      if (!u || seen.has(u.id)) continue;
      seen.add(u.id);
      sequenced.push(u);
    }
    for (const u of this.units) {
      if (seen.has(u.id)) continue;
      sequenced.push(u);
    }
    for (const target of sequenced) {
      if (target.side !== "player" || !target.alive) continue;
      const have = target.bag[kind] ?? 0;
      if (have < cap) {
        target.bag[kind] = have + 1;
        return target;
      }
    }
    return null;
  }

  /** Hero name → tier key → spell uses spent so far this scenario, for persisting into
   * save.spellUses (see Roster.spellSpent) — recomputed as the current class/level cap
   * minus whatever's left, so a level-up mid-battle naturally reflects the bigger cap
   * instead of needing its own bookkeeping. */
  spentTiers(): Record<string, Partial<Record<TierKey, number>>> {
    const out: Record<string, Partial<Record<TierKey, number>>> = {};
    for (const u of this.units) {
      if (u.side !== "player") continue;
      const perTier: Partial<Record<TierKey, number>> = {};
      for (let t = 1; t <= 10; t++) {
        const key = tierKey(t as SpellTier);
        const cap = tierUses(u.classId, t as SpellTier, u.level);
        const left = Number.isFinite(u.spells[key]) ? u.spells[key] : 0;
        perTier[key] = Math.max(0, cap - left);
      }
      out[u.name] = perTier;
    }
    return out;
  }

  /** Freeze the live board so a save can resume this fight instead of restarting it. */
  captureSnapshot(): BattleSnapshot {
    const units = this.units.map((u): BattleUnitSnap => {
      const snap: BattleUnitSnap = {
        id: u.id,
        name: u.name,
        classId: u.classId,
        side: u.side,
        x: Math.round(u.x),
        y: Math.round(u.y),
        hp: u.hp,
    escaped: u.escaped,
        maxHp: u.maxHp,
        atk: u.atk,
        mag: u.mag,
        def: u.def,
        dex: u.dex,
    resistances: { ...u.resistances },
    weaponSkills: { ...u.weaponSkills }, healingSkill: u.healingSkill ?? 0,
        initiative: u.initiative,
        initiativeRoll: u.initiativeRoll,
        statPointAllocation: { ...u.statPointAllocation },
        mov: u.mov,
        minRange: u.minRange,
        maxRange: u.maxRange,
        moved: u.moved,
        acted: u.acted,
        facing: u.facing,
        faceDx: u.faceDx,
        faceDy: u.faceDy,
        alive: u.alive,
        fade: u.fade,
        level: u.level,
        xp: u.xp,
        bag: { ...u.bag },
        spells: { ...u.spells },
        weaponId: u.weaponId,
        weaponEnh: u.weaponEnh,
        shock: u.shock ? { ...u.shock } : null,
        shockCharges: u.shockCharges ?? 0,
        frostCharges: u.frostCharges,
        fantomForceCharges: u.fantomForceCharges,
        blessedHitBonusPct: u.blessedHitBonusPct,
        blessedRoundsLeft: u.blessedRoundsLeft,
        diseased: u.diseased,
        diseaseBase: u.diseaseBase ? { ...u.diseaseBase } : null,
        poisoned: u.poisoned,
    poisonTier: u.poisonTier,
        poisonMag: u.poisonMag,
        poisonResist: u.poisonResist,
        bleeding: u.bleeding,
        bleedRoundsLeft: u.bleedRoundsLeft,
        bleedRoundMarker: u.bleedRoundMarker,
        fearTurns: u.fearTurns,
    fearSourceId: u.fearSourceId,
    stunned: u.stunned,
        stunTurns: u.stunTurns,
        crippled: u.crippled,
        hungerPenaltyPct: u.hungerPenaltyPct,
        fullness: u.fullness,
        offHandId: u.offHandId,
        gear: { ...u.gear },
        summoned: u.summoned,
        asleep: u.asleep,
        sleepTurns: u.sleepTurns,
        guaranteedDrop: u.guaranteedDrop,
        dialog: u.dialog,
        moveBudgetUsed: u.moveBudgetUsed,
        useClassSprite: u.useClassSprite,
      };
      return snap;
    });
    // An in-flight action would otherwise replay (or vanish) on load. Spend the acting
    // unit's action so they don't act twice; enemies also finish the turn.
    if (this.active || this.queue.length > 0) {
      const active = this.activeTurnUnit();
      const snap = active ? units.find((u) => u.id === active.id) : undefined;
      if (snap) {
        snap.acted = true;
        if (this.phase === "enemy" || snap.mov - snap.moveBudgetUsed <= 0) {
          snap.moved = true;
        }
      }
    }
    return {
      affinityScores: { ...this.affinityScores },
      heroSkills: structuredClone(this.heroSkills),
      missionId: this.mission.id,
      mapKey: this.mapKey || undefined,
      introDialogDone: this.introDialogDone,
      enmity: enmityToSnapshot(this.enmity),
      turn: this.turn,
      phase: this.phase,
      units,
      tiles: [...this.tiles],
      decorations: this.decorations.map((d) => ({ ...d })),
      turnOrder: [...this.turnOrder],
      activeUnitId: this.activeTurnUnit()?.id ?? this.activeUnitId,
      selectedId: this.selectedId,
      lootEmber: this.lootEmber,
      lootRations: this.lootRations,
      questFound: [...this.questFound],
      lootWeapons: [...this.lootWeapons],
      lootEquipment: [...this.lootEquipment],
      ownedWeapons: [...this.ownedWeapons],
      webZones: this.webZones.map((z) => ({ cells: [...z.cells], roundsLeft: z.roundsLeft, center: z.center, radius: z.radius, sleepChance: z.sleepChance })),
      iceStormZones: this.iceStormZones.map((z) => ({ ...z, cells: [...z.cells], center: { ...z.center } })),
      auraZones: this.auraZones.map((z) => ({
        cells: [...z.cells],
        roundsLeft: z.roundsLeft,
        kind: z.kind,
        side: z.side,
        pct: z.pct,
      })),
      warpGates: this.warpGates.map((g) => ({ ...g, a: { ...g.a }, b: { ...g.b } })),
      log: [...this.log],
      winAvailable: this.winAvailable,
      chestLoot: this.chestLoot
        ? { unitName: this.chestLoot.unitName, ember: this.chestLoot.ember, items: this.chestLoot.items.map((i) => ({ ...i })) }
        : null,
      pendingDialog: this.pendingDialog,
      turnRestrained: this.turnRestrained,
      turnBegan: !!this.activeTurnUnit() && this.activeUnitId === this.activeTurnUnit()?.id,
      explored: this.snapshotExplored(),
      awake: this.fogged && this.awake.size > 0 ? [...this.awake] : undefined,
    };
  }

  /** Overlay a saved fight onto this engine (which has already constructed the mission). */
  applySnapshot(snap: BattleSnapshot): void {
    this.affinityScores = cleanAffinityScores(snap.affinityScores ?? this.affinityScores);
    this.heroSkills = cleanHeroSkills(snap.heroSkills ?? this.heroSkills, Object.keys(snap.heroSkills ?? this.heroSkills));
    if (snap.missionId !== this.mission.id) return;
    if (snap.tiles.length === this.tiles.length) {
      for (let i = 0; i < snap.tiles.length; i++) this.tiles[i] = snap.tiles[i]!;
      this.terrainVersion++;
    }
    this.decorations.splice(0, this.decorations.length, ...snap.decorations.map((d) => ({ ...d })));
    // Old in-progress saves can carry the same column scaffolding as old maps.
    const cleanedTiles = clearRockColumnTiles(this.tiles, this.cols, this.rows, this.decorations, this.mission.baseTile);
    for (let i = 0; i < cleanedTiles.length; i++) this.tiles[i] = cleanedTiles[i]!;
    this.refreshDecorOverlay();
    // older saves carry a single warpGate
    this.warpGates = (snap.warpGates ?? (snap.warpGate ? [snap.warpGate] : [])).map((g) => ({ ...g, a: { ...g.a }, b: { ...g.b } }));
    for (const gate of this.warpGates) {
      this.emitWarpFx(gate.a.x, gate.a.y, warpPartyRadius(gate.level), gate.level);
      this.emitWarpFx(gate.b.x, gate.b.y, warpPartyRadius(gate.level), gate.level);
    }
    const needsOpeningInitiative = snap.units.some((unit) => unit.initiative == null || unit.initiativeRoll == null);
    this.units = snap.units.map(saved => {
      const unit = unitFromSnap(saved);
      if (unit.side === "player" && !unit.summoned) {
        // Earlier snapshots can contain unit proficiency without the top-level skill ledger.
        for (const type of weaponTypesForClass(unit.classId)) {
          const id = `${type}Weapon` as const;
          if (this.heroSkills[unit.name]?.[id] == null && saved.weaponSkills?.[type] != null) {
            this.heroSkills[unit.name] = { ...this.heroSkills[unit.name], [id]: saved.weaponSkills[type] };
          }
        }
        this.heroSkills = cleanHeroSkills(this.heroSkills, Object.keys(this.heroSkills));
        unit.weaponSkills = trainedWeaponSkills(this.heroSkills, unit.name, unit.classId); unit.healingSkill = skillValue(this.heroSkills, unit.name, "healing");
      }
      if (!saved.resistances && unit.side === "player" && !unit.summoned) {
        unit.resistances = sumResistances(CLASSES[unit.classId]?.resistances, gearStatBonus(Object.values(unit.gear), unit.weaponId, unit.classId).resistances, skillResistances(this.heroSkills, unit.name));
      }
      const staff = unit.weaponId ? WEAPONS[unit.weaponId] : undefined;
      if (staff?.magic && unit.side === "player" && !unit.summoned) {
        if (!staff.usableBy.includes(unit.classId)) {
          unit.weaponId = starterWeaponFor(unit.classId); unit.weaponEnh = 0;
          const starter = unit.weaponId ? WEAPONS[unit.weaponId] : undefined;
          unit.minRange = starter?.minRange ?? 1; unit.maxRange = starter?.maxRange ?? 1;
        }
        this.reapplyGear(unit);
      }
      return unit;
    });
    this.invalidateOcc();
    this.turn = snap.turn;
    this.phase = snap.phase;
    if (needsOpeningInitiative) {
      const actingUnits = this.units.filter(takesTurns);
      this.rollOpeningInitiative(actingUnits);
      this.turnOrder = this.sortByInitiative(actingUnits);
    } else {
      this.turnOrder = [...snap.turnOrder];
    }
    this.lootEmber = snap.lootEmber;
    this.lootRations = snap.lootRations ?? 0;
    this.questFound = [...(snap.questFound ?? [])];
    this.questPickups = this.questPickups.filter((p) => !this.questFound.includes(p.key));
    this.lootWeapons = [...snap.lootWeapons];
    this.lootEquipment = [...snap.lootEquipment];
    this.ownedWeapons = new Set(snap.ownedWeapons);
    this.webZones = snap.webZones.map((z) => ({ cells: new Set(z.cells), roundsLeft: z.roundsLeft, center: z.center, radius: z.radius, sleepChance: z.sleepChance }));
    this.iceStormZones = (snap.iceStormZones ?? []).map((z) => ({ ...z, cells: new Set(z.cells), center: { ...z.center } }));
    this.auraZones = snap.auraZones.map((z) => ({
      cells: new Set(z.cells),
      roundsLeft: z.roundsLeft,
      kind: z.kind,
      side: z.side,
      pct: z.pct,
    }));
    this.log = [...snap.log];
    this.winAvailable = snap.winAvailable;
    this.waypointCheckPending = true;
    this.chestLoot = snap.chestLoot
      ? { unitName: snap.chestLoot.unitName, ember: snap.chestLoot.ember, items: snap.chestLoot.items.map((i) => ({ ...i })) }
      : null;
    this.pendingDialog = snap.pendingDialog;
    // A saved fight is always past its opening: the intro opens before the player can act,
    // so a snapshot from before this flag existed counts as done too.
    this.introDialogDone = snap.introDialogDone ?? true;
    this.enmity = enmityFromSnapshot(snap.enmity);
    this.turnRestrained = snap.turnRestrained;
    this.result = null;
    this.queue.length = 0;
    this.active = null;
    this.mode = "locked";
    this.selectedId = null;
    this.pendingFoeId = null;
    this.inspectedId = null;
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.potionAim = null;
    this.missileTargets = [];
    this.hover = null;
    this.reach.clear();
    this.attackFrom.clear();
    this.threat = [];
    this.orig = null;
    this.origMoveBudgetUsed = null;
    this.turnStart = null;
    this.moveSpoiled = true;
    this.skipStartOfTurn = snap.turnBegan;
    // After units and tiles are in place, so the length check has the right board and
    // the first refreshVisibility relights around wherever the party actually landed.
    this.restoreExplored(snap.explored);
    this.awake = new Set(snap.awake ?? []);
    this.activeUnitId = null;
    const first = this.units.find((u) => u.side === "player" && u.alive);
    if (first) {
      this.cursor = { x: first.x, y: first.y };
      this.centerOn(first.x, first.y);
    }
    this.tip = "Combate retomado.";
  }

  tick(dt: number): void {
    const cap = Math.min(0.05, dt);
    this.time += cap;
    // Same speedMode scaling stepActive applies to combat/spell/heal's own a.t (see there for
    // the full explanation) — applied here too, to every spell/attack VISUAL effect timer
    // (missile bolts, fireball bursts, lightning, holy rays, blade sweeps, summon portals) so
    // they stay in lockstep with the now-slower hit timing instead of the bolt still zipping
    // across the screen at the old speed while the damage/hit-tick that's supposed to land
    // right as it arrives now fires later. Ambient stuff below (particles, level-up sparks,
    // bob/breathing, fade) deliberately stays on the unscaled `cap` — slowing those down too
    // would make idle units look like they're moving through syrup for no reason.
    const actionCap = cap * (this.speedMode === "fast" ? 1 : this.speedMode === "slow" ? 0.4 : 0.65);
    for (const fx of this.provokeFx) fx.t += actionCap;
    this.provokeFx = this.provokeFx.filter(fx => fx.t < PROVOKE_FX_DURATION);
    if (this.tip !== this.lastTipSeen) {
      this.lastTipSeen = this.tip;
      this.tipSetAt = this.time;
    } else if (this.tip !== null && this.time - this.tipSetAt >= 5) {
      this.tip = null;
      this.lastTipSeen = null;
    }
    if (this.onNextIdle && !this.active && this.queue.length === 0) {
      const fn = this.onNextIdle;
      this.onNextIdle = null;
      fn();
    }
    // Waypoints are position-driven, so recheck once a player movement sequence is fully
    // settled. This covers rout/boss maps as well as the transversal dungeon's escape maps.
    if (this.waypointCheckPending && !this.active && this.queue.length === 0) {
      this.waypointCheckPending = false;
      this.evaluateEnd();
    }
    if (this.trauma > 0) this.trauma = Math.max(0, this.trauma - cap * 2.2);
    for (const u of this.units) {
      if (u.flash > 0) u.flash = Math.max(0, u.flash - cap * 4);
      if (u.levelGlow > 0) u.levelGlow = Math.max(0, u.levelGlow - cap * 0.42);
      if (u.healGlow > 0) u.healGlow = Math.max(0, u.healGlow - cap * 0.7);
      if (!u.alive && u.fade > 0 && !this.deathSheetPlaying(u)) u.fade = Math.max(0, u.fade - cap * 2.4);
      // A freshly summoned unit starts at fade 0 (see castSummonFamiliar) and eases back in
      // while its portal plays, rather than popping fully opaque the instant it's added.
      else if (u.alive && u.fade < 1) u.fade = Math.min(1, u.fade + cap * 2.4);
      // Carnivorous Plant's and Sapling's hit and death sounds start with their sheets: the hit
      // sheet at hitAt, the death sheet HIT_ANIM_SECONDS after diedAt (it plays after the hit sheet).
      if (u.sprite === "carnivorous-plant-001" || u.sprite === "sapling-001") {
        const sapling = u.sprite === "sapling-001";
        let cue = this.plantSoundCues.get(u);
        if (!cue) this.plantSoundCues.set(u, (cue = { hitAt: u.hitAt }));
        if (u.hitAt != null && u.hitAt !== cue.hitAt) {
          cue.hitAt = u.hitAt;
          if (sapling) sfxPlay.saplingHit();
          else sfxPlay.carnivorousPlantHit();
        }
        if (!u.alive && u.diedAt != null && !cue.death && this.time - u.diedAt >= HIT_ANIM_SECONDS) {
          cue.death = true;
          if (sapling) sfxPlay.saplingDeath();
          else sfxPlay.carnivorousPlantDeath();
        }
      }
      // Plague Bearing Cattle has no hit sheet: its death sheet starts at diedAt, and so does its sound.
      if (u.sprite === "plague-bearing-cattle" && !u.alive && u.diedAt != null && !this.cattleDeathCued.has(u)) {
        this.cattleDeathCued.add(u);
        sfxPlay.plagueCattleDeath();
      }
      // Monsters with their own hit/death cues (audio.ts MONSTER_SFX): the hit cue starts with the
      // hit sheet at hitAt, the death cue with the death sheet — after the hit sheet when the
      // monster has one, straight away for the Big Blue Ox (it skips its hit sheet on death).
      if (hasMonsterSfx(u.sprite, "hit") || hasMonsterSfx(u.sprite, "death")) {
        let cue = this.monsterSoundCues.get(u);
        if (!cue) this.monsterSoundCues.set(u, (cue = { hitAt: u.hitAt }));
        if (u.hitAt != null && u.hitAt !== cue.hitAt) {
          cue.hitAt = u.hitAt;
          if (u.alive || u.classId !== "bigBlueCalf") sfxPlay.monster(u.sprite, "hit");
        }
        const hitLead = u.classId !== "bigBlueCalf" && this.art.hits[u.sprite] ? HIT_ANIM_SECONDS : 0;
        if (!u.alive && u.diedAt != null && !cue.death && this.time - u.diedAt >= hitLead) {
          cue.death = true;
          const alt = u.deathAlt && !!this.art.deaths2[u.sprite];
          if (!(alt && sfxPlay.monster(u.sprite, "death2"))) sfxPlay.monster(u.sprite, "death");
        }
      }
      if (u.alive) {
        const haste =
          u.classId === "wardog" ? 1.4 : u.size >= 4 ? 0.58 : u.classId === "mage" || u.classId === "cultist" || u.classId === "cultistV2" ? 0.8 : 1;
        u.bob += cap * haste;
      }
    }
    if (this.particleLive) {
      let live = 0;
      for (const p of this.particles) {
        if (!p.live) continue;
        p.life += cap;
        if (p.life >= p.max) {
          p.live = false;
          continue;
        }
        p.x += p.vx * cap;
        p.y += p.vy * cap;
        if (p.kind === "impact") p.frame += cap * 12;
        live += 1;
      }
      this.particleLive = live;
    }
    if (this.levelUpFxLive) {
      let live = 0;
      for (const s of this.levelUpFx) {
        if (!s.live) continue;
        s.life += cap;
        if (s.life >= s.max) {
          s.live = false;
          continue;
        }
        s.dx += s.vx * cap;
        s.dy += s.vy * cap;
        if (s.kind !== "label") s.vy += s.refCell * 0.9 * cap;
        s.rot += s.vrot * cap;
        live += 1;
      }
      this.levelUpFxLive = live;
    }
    if (this.missileFxLive) {
      let live = 0;
      for (const m of this.missileFx) {
        if (!m.live) continue;
        m.t += actionCap;
        // Track the bolt itself while it's actually in flight (not the afterglow lingering
        // once it's landed) — same one-time ensureVisible nudge startSeq already does for the
        // cast's start/end points, just repeated every frame along the flight path so the
        // camera pans smoothly with a fast-travelling shot instead of only snapping to catch
        // up once it's already arrived.
        if (m.t < m.travel) {
          const k = m.t / m.travel;
          this.ensureVisible(m.fromX + (m.toX - m.fromX) * k, m.fromY + (m.toY - m.fromY) * k);
        }
        if (m.t >= m.max) {
          m.live = false;
          continue;
        }
        live += 1;
      }
      this.missileFxLive = live;
    }
    if (this.fireballBurstFxLive) {
      let live = 0;
      for (const burst of this.fireballBurstFx) {
        if (!burst.live) continue;
        burst.t += actionCap;
        if (burst.t >= burst.max) { burst.live = false; continue; }
        live += 1;
      }
      this.fireballBurstFxLive = live;
    }
    if (this.lightningFxLive) {
      let live = 0;
      for (const l of this.lightningFx) {
        if (!l.live) continue;
        l.t += actionCap;
        if (l.t >= l.max) {
          l.live = false;
          continue;
        }
        live += 1;
      }
      this.lightningFxLive = live;
    }
    this.turnUndeadFx = this.turnUndeadFx.filter((fx) => (fx.t += actionCap) < TURN_UNDEAD_V4_DURATION);
    if (this.holyFxLive) {
      let live = 0;
      for (const h of this.holyFx) {
        if (!h.live) continue;
        h.t += actionCap;
        if (h.t >= h.max) {
          h.live = false;
          continue;
        }
        live += 1;
      }
      this.holyFxLive = live;
    }
    if (this.bladeFxLive) {
      let live = 0;
      for (const b of this.bladeFx) {
        if (!b.live) continue;
        b.t += actionCap;
        if (b.t >= b.max) {
          b.live = false;
          continue;
        }
        live += 1;
      }
      this.bladeFxLive = live;
    }
    if (this.portalFxLive) {
      let live = 0;
      for (const p of this.portalFx) {
        if (!p.live) continue;
        p.t += actionCap;
        if (p.t >= p.max) {
          // an open Warp gate waits fully open, its loop running on: wrapping its clock back to 0
          // closed and reopened it every 2.4 s (a flicker)
          if (p.persistent && this.warpGates.some((g) => (g.a.x === p.x && g.a.y === p.y) || (g.b.x === p.x && g.b.y === p.y))) {
            live += 1;
            continue;
          }
          p.live = false;
          continue;
        }
        live += 1;
      }
      this.portalFxLive = live;
    }
    if (this.hitstop > 0) {
      this.hitstop -= cap;
      this.emit();
      return;
    }
    if (!this.active && this.queue.length) this.startSeq(this.queue.shift()!);
    if (this.active) this.stepActive(cap);
    // Movement/range grid: hidden while anything is still playing (a walk, attack or spell,
    // or a projectile/impact FX still on screen), then fades in once the board is quiet.
    const boardBusy =
      !!this.active ||
      this.queue.length > 0 ||
      this.missileFxLive > 0 ||
      this.fireballBurstFxLive > 0 ||
      this.lightningFxLive > 0 ||
      this.holyFxLive > 0 ||
      this.turnUndeadFx.length > 0 ||
      this.provokeFx.length > 0 ||
      this.bladeFxLive > 0 ||
      this.portalFx.some((fx) => fx.live && !fx.persistent);
    this.overlayFade = boardBusy ? 0 : Math.min(1, this.overlayFade + cap / OVERLAY_FADE_IN);
    // An action is over once nothing is queued or playing — the next one may bleed again.
    if (!this.active && this.queue.length === 0) this.bleedChargedIds.clear();
    if (!this.result && !this.active && this.queue.length === 0) {
      const active = this.activeTurnUnit();
      const activeId = active?.id ?? null;
      if (activeId !== this.activeUnitId) {
        this.activeUnitId = activeId;
        if (active) this.beginUnitTurn(active);
        else this.startNewRound();
      }
    }
    this.emit();
  }

  /** Point a directional sprite (conjurer / lancer) at a column so walk and attack
   * play the matching left/right cut instead of a mirrored idle. Other sprites keep
   * the historical "facing = 1 shows the sheet as drawn" convention — EXCEPT familiar3,
   * familiar4, morvenian-wolf and mordavian-wolf, whose facing drives render()'s ordinary CSS
   * mirror (dirAction is false for them, same as every other non-directional sprite) rather than
   * an asset pick. Outside faceSpriteToward, u.facing only
   * ever changes from actually walking (see the stepMove branch that sets it), so a unit
   * that attacked without moving, or moved one way and then got attacked from the other
   * side, kept whatever stale facing its last step left behind instead of turning to face
   * the fight — the "not facing the enemy" report, distinct from (and left uncaught by) the
   * earlier mirrored-attack-frame fix above familiar3Scale. */
  private faceSpriteToward(id: string, x: number, y: number): void {
    const u = this.units.find((n) => n.id === id);
    if (!u) return;
    // Every attacker, shooter and counter-attacker turns toward its target. Compare screen
    // position, not column: same reason as stepMove's facing — on this row-staggered grid a
    // same-column target is still left or right on screen, and comparing columns left those
    // units facing wherever their last walk pointed them.
    const from = this.hexCenter(u.x, u.y);
    const to = this.hexCenter(x, y);
    if (to.cx === from.cx && to.cy === from.cy) return;
    u.faceDx = to.cx - from.cx;
    u.faceDy = to.cy - from.cy;
    this.applyHeading(u);
  }

  /** Hard rule for every unit: on-screen facing always follows its last real heading (faceDx/faceDy),
   * resolved for the current camera angle — it never falls back to a default, and a camera rotation
   * can't leave a unit facing away from whoever it last fought. */
  private applyHeading(u: Unit): void {
    if (u.faceDx == null || u.faceDy == null) return;
    const azimuth = this.tacticsCamera ? (this.cameraTiltSide * Math.PI) / 180 : 0;
    const screenDx = u.faceDx * Math.cos(azimuth) - u.faceDy * Math.sin(azimuth);
    if (screenDx > 1e-6) u.facing = 1;
    else if (screenDx < -1e-6) u.facing = -1;
  }

  private startSeq(step: Seq): void {
    // Facing is action state, never a render-time calculation. Preserve it after the
    // action ends, and do not turn again when a wound-up action resumes.
    if (!this.woundUp.has(step) && (step.type === "combat" || step.type === "spell")) {
      const actor = this.units.find(u => u.id === step.att);
      const targets = step.type === "combat" ? [step.def] : step.ids;
      const target = this.units.find(u => u.id === targets[0]) ?? (step.type === "spell" ? step.tiles[0] : undefined);
      if (actor && target) this.faceSpriteToward(actor.id, target.x, target.y);
      for (const id of targets) {
        const recipient = this.units.find(u => u.id === id);
        if (step.type === "spell" && actor && recipient && actor.side !== recipient.side && (step.spellKind !== "turnUndead" || isUndeadClass(recipient.classId))) this.faceSpriteToward(recipient.id, actor.x, actor.y);
      }
    }
    // Fire authored casting/healing cues when the action begins. Long cast sheets can
    // hold the queued step for a wind-up, so only play on the first visit to this step.
    if (!this.woundUp.has(step)) {
      if (step.type === "spell") {
        const caster = this.units.find((u) => u.id === step.att);
        const arrowSpell = step.spellKind === "longShot" || step.spellKind === "bloodyShot" || step.spellKind === "multiShot" || step.spellKind === "piercing";
        const meleeSkill = step.spellKind === "cleave" || step.spellKind === "sweep" || step.spellKind === "shoulderSmash" || step.spellKind === "stampede" || step.spellKind === "piercingThrust" || step.spellKind === "trip" || step.spellKind === "doubleStrike";
        // Neera's bow skills wind up for her full Special sheet (LONG_ANIM_SECONDS) before the
        // arrow leaves — her shot sound is timed to that release (see sfxPlay.arrowAttack).
        if (arrowSpell) sfxPlay.arrowAttack(caster?.sprite === "neera", this.reducedMotion ? 0 : LONG_ANIM_SECONDS);
        else if (step.spellKind === "tendrilSwipe") sfxPlay.carnivorousPlantAttack();
        else if (meleeSkill && caster) this.playMeleeCue(caster, false, step.spellKind);
        // Rule: every cast sound plays with the casting animation, so Bless's heal cue is here at the
        // start of the sheet too, not after the wind-up.
        else if (step.spellKind === "bless") sfxPlay.healBy(caster?.sprite);
        else if (!meleeSkill) {
          if (isSupportCast(step) && hasMonsterSfx(caster?.sprite, "heal")) sfxPlay.healBy(caster?.sprite);
          else if (caster?.sprite === "minor-horror-001") sfxPlay.minorHorrorCast();
          else if (caster?.sprite === "carnivorous-plant-001") sfxPlay.carnivorousPlantCast();
          else if (caster?.sprite === "sapling-001") sfxPlay.saplingCast();
          else if (caster?.sprite === "plague-bearing-cattle") sfxPlay.plagueCattleCast();
          else if (sfxPlay.monster(caster?.sprite, "cast")) {
            // A monster's own cast cue (audio.ts MONSTER_SFX).
          }
          else if (caster?.sprite === "cultist-v2") sfxPlay.cultistV2Spellcast();
          else sfxPlay.spell();
        }
      } else if (step.type === "combat") {
        const attacker = this.units.find((u) => u.id === step.att);
        if (step.spellKind === "shieldBash") {
          // ShieldBash.mp3 is cued at the strike, after the attack wind-up.
        } else if (attacker && (attacker.classId === "brigand" || attacker.classId === "baseCrossbowman" || attacker.classId === "brigandCrossbowman" || (!step.customDice && this.isArrowAttack(attacker)))) {
          sfxPlay.arrowAttack(attacker.sprite === "neera", this.reducedMotion ? 0 : LONG_ARROW_RELEASE_SECONDS);
        } else if (attacker && !step.customDice && this.isArcaneCaster(attacker)) {
          if (attacker.sprite === "cultist-v2") sfxPlay.cultistV2Attack();
          else if (!sfxPlay.monster(attacker.sprite, "attack")) sfxPlay.magicAttack();
        } else if (attacker && !this.isArcaneCaster(attacker) && (attacker.sprite !== "kaelFinal" || !!step.customDice)) {
          // Kael's long main-hand swing is cued at its strike instead (see stepCombat's lunge end).
          if (attacker.sprite === "minor-horror-001") sfxPlay.minorHorrorAttack();
          else if (attacker.sprite === "carnivorous-plant-001") sfxPlay.carnivorousPlantAttack();
          else if (attacker.sprite === "sapling-001") sfxPlay.saplingAttack();
          else if (attacker.sprite === "plague-bearing-cattle") sfxPlay.plagueCattleAttack();
          else if (this.monsterAttackCue(attacker)) {
            // A monster's own attack cue (audio.ts MONSTER_SFX).
          }
          else this.playMeleeCue(attacker, !!step.customDice, step.spellKind);
        }
      } else if (step.type === "heal" || step.type === "cureDisease") {
        sfxPlay.healBy(this.units.find((u) => u.id === step.att)?.sprite);
      }
    }
    // Long sheets only (LONG_SHEET_FRAMES+): a spell, skill, heal or ranged shot waits for the
    // caster's/archer's whole cast or attack sheet to finish before it goes off — the arrow
    // leaves the bow only after the draw, not halfway through it. The step is put back at the
    // front of the queue and runs unchanged once the wind-up ends. Short sheets and melee
    // swings skip this entirely.
    // Cure Disease only winds up for a caster with a healing sheet (Salazar V2); every other
    // sprite keeps playing it with no pose, as before.
    const healSheetCure = step.type === "cureDisease" && !!this.art.castsHeal[this.units.find((u) => u.id === step.att)?.sprite as SpriteId];
    if ((step.type === "spell" || step.type === "heal" || step.type === "combat" || healSheetCure) && !this.woundUp.has(step) && !this.reducedMotion) {
      const actor = this.units.find((u) => u.id === step.att);
      const target =
        step.type === "combat" || step.type === "heal" || step.type === "cureDisease"
          ? this.units.find((u) => u.id === step.def)
          : step.ids[0]
            ? this.units.find((u) => u.id === step.ids[0])
            : null;
      // Warrior/Lancer physical skills land together with their swing — no wind-up (same
      // melee-skill list stepSpell's hit cue uses).
      const meleeSkill =
        step.type === "spell" &&
        (step.spellKind === "doubleStrike" || step.spellKind === "cleave" || step.spellKind === "piercingThrust" || step.spellKind === "sweep" || step.spellKind === "trip" || step.spellKind === "shoulderSmash" || step.spellKind === "stampede" || step.spellKind === "tendrilSwipe");
      const ranged = !meleeSkill && (step.type !== "combat" || (!!actor && !step.customDice && (this.isArrowAttack(actor) || this.isArcaneCaster(actor))));
      const arrowSkill = step.type === "spell" && (step.spellKind === "longShot" || step.spellKind === "bloodyShot" || step.spellKind === "multiShot" || step.spellKind === "piercing");
      const neeraArrowSkill = !!actor && actor.sprite === "neera" && arrowSkill;
      const pose = neeraArrowSkill ? "specialAttack" : step.type === "combat" ? "attack" : "cast";
      const frames = actor
        ? neeraArrowSkill
          ? (this.art.attacks2[actor.sprite] ?? this.art.attacks[actor.sprite])
          : pose === "cast"
          ? this.castFrames(actor.sprite, isSupportCast(step))
          : actor.classId !== "bigBlueCalf" && actor.sprite !== "neera" && actor.idleAlt
            ? (this.art.attacks2[actor.sprite] ?? this.art.attacks[actor.sprite])
            : this.art.attacks[actor.sprite]
        : undefined;
      const targetAlive = step.type !== "combat" || !!target?.alive;
      if (actor && ranged && targetAlive && (frames?.length ?? 0) >= LONG_SHEET_FRAMES) {
        const arrowAttack = step.type === "combat" && this.isArrowAttack(actor);
        const release = neeraArrowSkill
          ? LONG_ANIM_SECONDS
          : arrowSkill
            ? LONG_ARROW_SKILL_RELEASE_SECONDS
            : arrowAttack
              ? LONG_ARROW_RELEASE_SECONDS
              : actor.classId === "minorHorror"
                ? MINOR_HORROR_SECONDS.cast
                : LONG_ANIM_SECONDS;
        this.woundUp.set(step, release);
        this.queue.unshift(step);
        const look = target ?? (step.type === "spell" ? step.tiles[0] : undefined);
        if (look) this.faceSpriteToward(actor.id, look.x, look.y);
        this.ensureVisible(actor.x, actor.y);
        this.active = { type: "windup", id: actor.id, t: 0, dur: release, pose, heal: pose === "cast" && isSupportCast(step) };
        return;
      }
    }
    const held = this.woundUp.has(step);
    const heldFrom = this.woundUp.get(step);
    const heldAt = this.time;
    const deferred = this.onSeqStart.get(step);
    if (deferred) {
      this.onSeqStart.delete(step);
      deferred();
    }
    const actionId = step.type === "move" ? step.id : step.type === "combat" || step.type === "spell" || step.type === "heal" || step.type === "cureDisease" ? step.att : null;
    const isMove = step.type === "move" && step.path.length > 1;
    // One bleed per action: a multi-step action (Double Strike's two swings, Bull Rush's
    // charge + hits) is charged once, not per step. Moving has its own once-per-turn charge.
    const charged = !isMove && !!actionId && this.bleedChargedIds.has(actionId);
    if (actionId && step.type !== "move") this.bleedChargedIds.add(actionId);
    if (actionId && (isMove || (step.type !== "move" && !charged)) && !this.applyBleedingActionDamage(this.units.find((u) => u.id === actionId), isMove)) {
      // Bled out mid-action: drop whatever else that unit still had queued.
      this.queue = this.queue.filter((q) => !("att" in q && q.att === actionId) && !(q.type === "move" && q.id === actionId));
      this.onNextIdle = null;
      if (this.selectedId === actionId) this.selectedId = null;
      if (this.phase === "player") this.mode = "idle";
      this.evaluateEnd();
      return;
    }
    if (step.type === "move") {
      this.active = { type: "move", id: step.id, path: step.path, i: 0, t: 0, charge: this.chargeMoves.has(step) };
      // Cultist V2 has its own dedicated left/right walk cues (see assets.ts's move-left-*
      // cut) — play whichever matches this move's own first step instead of the generic
      // footstep beep every other sprite uses.
      const mover = this.units.find((u) => u.id === step.id);
      if (mover?.sprite === "minor-horror-001") {
        sfxPlay.minorHorrorWalk();
      } else if (mover?.sprite === "sapling-001") {
        sfxPlay.saplingWalk();
      } else if (mover?.sprite === "plague-bearing-cattle") {
        sfxPlay.plagueCattleWalk();
      } else if (sfxPlay.monster(mover?.sprite, "walk")) {
        // A monster's own walk cue (audio.ts MONSTER_SFX), faded out when the move ends.
      } else if (mover?.sprite === "cultist-v2" && step.path.length >= 2) {
        if (step.path[1]!.x < step.path[0]!.x) sfxPlay.cultistV2WalkLeft();
        else sfxPlay.cultistV2WalkRight();
      } else {
        sfxPlay.move();
      }
    } else if (step.type === "combat") {
      const target = this.units.find((u) => u.id === step.def);
      if (!target || !target.alive) return;
      const attacker = this.units.find((u) => u.id === step.att);
      this.faceSpriteToward(step.att, target.x, target.y);
      // Keep the defender's heading through the incoming hit so rear attacks retain their bonus.
      // Bring both ends of the attack into view regardless of who's acting — this used to be
      // enemy-only (side !== "player"), which meant the camera dutifully followed every enemy
      // swing but never panned to show the PLAYER's own target when it was off past the turn's
      // starting view. That read as "the camera doesn't follow spells" even though it was
      // working fine — just only for the other side.
      if (attacker) {
        this.ensureVisible(attacker.x, attacker.y);
        this.ensureVisible(target.x, target.y);
      }
      this.active = {
        type: "combat",
        att: step.att,
        def: step.def,
        stage: "lunge",
        // After a wind-up the draw is already done: start at the end of the lunge so the
        // arrow/bolt leaves on the very next tick.
        t: held ? 0.2 : 0,
        held,
        heldFrom,
        heldAt,
        swapped: false,
        bonusDice: step.bonusDice ?? 0,
        bonusDiceCount: step.bonusDiceCount ?? 1,
        bonusFlat: step.bonusFlat ?? 0,
        noCounter: step.noCounter ?? false,
        spellKind: step.spellKind ?? null,
        customDice: step.customDice ?? null,
        counterCustomDice: null,
        dmgMul: step.dmgMul ?? 1,
        stunChance: step.stunChance ?? 0,
        wallImpact: step.wallImpact ?? null,
        knockTo: step.knockTo ?? null,
      };
    } else if (step.type === "spell") {
      this.active = {
        type: "spell",
        att: step.att,
        held,
        heldFrom,
        heldAt,
        tiles: step.tiles,
        ids: step.ids,
        t: 0,
        hit: false,
        extraDice: step.dice ?? 0,
        extraFaces: step.faces ?? 8,
        extraBonus: step.bonus ?? 0,
        moreDice: step.moreDice ?? 0,
        moreFaces: step.moreFaces ?? 6,
        echo: step.echo ?? null,
        dmgMul: step.dmgMul ?? 1,
        weaponBonusDice: step.weaponBonusDice ?? 0,
        weaponBonusFaces: step.weaponBonusFaces ?? 8,
        weaponBonusBonus: step.weaponBonusBonus ?? 0,
        spellKind: step.spellKind ?? null,
        projectileTo: step.projectileTo ?? null,
        centerId: step.centerId ?? null,
        centerDice: step.centerDice ?? 0,
        centerFaces: step.centerFaces ?? 8,
        centerBonus: step.centerBonus ?? 0,
        poison: step.poison ?? false,
        spellMul: step.spellMul ?? 1,
        centerMul: step.centerMul ?? step.spellMul ?? 1,
      };
      {
        const look = step.ids[0]
          ? this.units.find((u) => u.id === step.ids[0])
          : null;
        const at = look ?? step.tiles[0];
        if (at) this.faceSpriteToward(step.att, at.x, at.y);
        const caster = this.units.find((u) => u.id === step.att);
        // Same fix as the combat branch above: this was enemy-only (side !== "player"), so a
        // player's own spell never panned the camera toward its target — only ever the caster,
        // wherever the camera already happened to be sitting from the start of their turn.
        // ensureAreaVisible covers the whole blast/cone/line, not just its centroid, so a wide
        // AOE's far edge isn't left off-screen just because its middle fit.
        if (caster) {
          this.ensureVisible(caster.x, caster.y);
          this.ensureAreaVisible(step.tiles);
        }
      }
      this.banner = step.label ?? "";
      // (Bless's cue already played with the casting animation, in startSeq.)
      if (step.spellKind === "bless") { /* no second cue at the release */ }
      else if (step.spellKind !== "longShot" && step.spellKind !== "multiShot" && step.spellKind !== "piercing") sfxPlay.crit();
      if(step.spellKind === "frost"){const caster=this.units.find(u=>u.id===step.att);if(caster)this.frostVfxRequests.push({origin:{x:caster.x,y:caster.y},cells:step.tiles.map(c=>({...c})),seed:Math.floor(this.time*1000)});}
      if (step.spellKind === "magicMissile") {
        const caster = this.units.find((u) => u.id === step.att);
        if (caster) for (const t of step.tiles) this.emitMissileFx(caster.x, caster.y, t.x, t.y, "magicMissile");
      }
      if (step.spellKind === "fantomForce") {
        const caster = this.units.find((u) => u.id === step.att);
        const target = step.tiles[0];
        if (caster && target) this.emitMissileFx(caster.x, caster.y, target.x, target.y, "fantomForce");
      }
      if (step.spellKind === "phantasmalForce" && (!this.phantasmalForceVfxAvailable || this.reducedMotion)) {
        const caster = this.units.find((u) => u.id === step.att);
        const target = step.tiles[0];
        if (caster && target) this.emitMissileFx(caster.x, caster.y, target.x, target.y, "phantasmalForce");
      }
      if (step.spellKind === "causticVenom") {
        const caster = this.units.find((u) => u.id === step.att);
        const target = step.projectileTo ?? null;
        if (caster && target) this.emitMissileFx(caster.x, caster.y, target.x, target.y, caster.classId === "carnivorousPlant" ? "minorVenom" : step.spellKind);
      }
      // Veneno Menor always flies the original 2D venom bolt (the 3D V2 smoke is Caustic only).
      if (step.spellKind === "minorVenom") {
        const caster = this.units.find((u) => u.id === step.att);
        const target = step.projectileTo ?? null;
        if (caster && target) this.emitMissileFx(caster.x, caster.y, target.x, target.y, "minorVenom");
      }
      if (step.spellKind === "longShot" || step.spellKind === "bloodyShot") {
        const caster = this.units.find((u) => u.id === step.att);
        const target = step.tiles[0];
        if (caster && target) this.emitMissileFx(caster.x, caster.y, target.x, target.y, "longShot");
      }
      if (step.spellKind === "multiShot") {
        const caster = this.units.find((u) => u.id === step.att);
        if (caster) for (const target of step.tiles) this.emitMissileFx(caster.x, caster.y, target.x, target.y, "longShot");
      }
      if (step.spellKind === "piercing") {
        const caster = this.units.find((u) => u.id === step.att);
        const target = step.tiles[step.tiles.length - 1];
        if (caster && target) this.emitMissileFx(caster.x, caster.y, target.x, target.y, "longShot");
      }
      if (step.spellKind === "divineBolt") {
        const center = step.projectileTo ?? step.tiles[0];
        if (center) this.emitLightningFx(center.x, center.y, "divine");
        for (const t of step.tiles) {
          if (!center || t.x !== center.x || t.y !== center.y) this.emitLightningFx(t.x, t.y, "divineSplash");
        }
      }
      if (step.spellKind === "lightning" || step.spellKind === "lightningTier3" || step.spellKind === "shock") {
        const power = step.spellKind === "lightningTier3" ? "t3" : step.spellKind === "lightning" ? "raio" : "shock";
        for (const t of step.tiles) this.emitLightningFx(t.x, t.y, power);
      }
    } else if (step.type === "heal") {
      this.active = { type: "heal", att: step.att, def: step.def, kind: step.kind, t: 0, applied: false, held, heldFrom, heldAt };
      this.banner = CURES[step.kind].name;
      sfxPlay.ui();
      const healed = this.units.find((u) => u.id === step.def);
      if (healed) this.faceSpriteToward(step.att, healed.x, healed.y);
      const healer = this.units.find((u) => u.id === step.att);
      // Same enemy-only fix as combat/spell above.
      if (healer) this.ensureVisible(healer.x, healer.y);
      if (healed) this.ensureVisible(healed.x, healed.y);
    } else if (step.type === "cureDisease") {
      this.active = { type: "cureDisease", att: step.att, def: step.def, t: 0, applied: false };
      this.banner = CURE_DISEASE.name;
      sfxPlay.ui();
    } else if (step.type === "castPose") {
      if (!this.reducedMotion) this.active = { type: "windup", id: step.id, t: 0, dur: LONG_ANIM_SECONDS, pose: "cast", heal: true };
    } else if (step.type === "banner") {
      this.banner = step.text;
      this.active = { type: "banner", text: step.text, t: 0, dur: step.dur };
      sfxPlay.turn();
    } else if (step.type === "delay") {
      this.active = { type: "delay", t: 0, dur: step.dur };
    } else if (step.type === "checkEnd") {
      this.evaluateEnd();
    }
  }

  private stepActive(dt: number): void {
    const a = this.active;
    if (!a) return;
    if (a.type === "windup") {
      a.t += dt;
      if (a.t >= a.dur) this.active = null;
      return;
    }
    if (a.type === "delay" || a.type === "banner") {
      a.t += dt;
      if (a.type === "banner" && a.t >= a.dur) this.banner = null;
      if (a.t >= a.dur) {
        this.active = null;
        if (a.type === "banner" && a.text === "Fase do jogador") this.mode = "idle";
      }
      return;
    }
    if (a.type === "move") {
      const unit = this.units.find((u) => u.id === a.id);
      if (!unit || a.path.length < 2) {
        this.active = null;
        return;
      }
      const from = a.path[a.i]!;
      const to = a.path[a.i + 1];
      if (!to) {
        unit.x = from.x;
        unit.y = from.y;
        unit.drawX = from.x;
        unit.drawY = from.y;
        // A Bull Rush dash leaves its golden streak behind, fading out (see rushTrail).
        const origin = a.path[0];
        if (a.charge && origin) this.emitBladeFx("rushTrail", origin.x, origin.y, { toX: from.x, toY: from.y });
        if (unit.sprite === "plague-bearing-cattle") sfxPlay.plagueCattleWalkStop();
        sfxPlay.monsterWalkStop(unit.sprite);
        this.active = null;
        return;
      }
      // Comparing logical column (to.x vs from.x) used to leave a "straight-up/straight-down"
      // hex neighbor (hexNeighbors' [0,-1]/[0,1] entries, same column) with no left/right info
      // at all — a unit walking due north/south kept whatever facing it had and visibly walked
      // sideways with its back leading. That was the wrong axis to check: this is a row-staggered
      // grid (hexCenter's cx depends on row & 1), so even a same-column step shifts the unit a
      // little left or right ON SCREEN — never zero, for any of the six neighbor directions.
      // Comparing actual screen position instead turns correctly on every single step, including
      // a move that's only one hex and never touches a different column at all.
      const fromScreen = this.hexCenter(from.x, from.y);
      const toScreen = this.hexCenter(to.x, to.y);
      if (toScreen.cx !== fromScreen.cx || toScreen.cy !== fromScreen.cy) {
        unit.faceDx = toScreen.cx - fromScreen.cx;
        unit.faceDy = toScreen.cy - fromScreen.cy;
        this.applyHeading(unit);
      }
      unit.walkPose = to.y < from.y ? "back" : to.y > from.y ? "front" : "side";
      a.t += dt;
      const dur = this.moveStepDur(a);
      const k = Math.min(1, a.t / dur);
      unit.drawX = from.x + (to.x - from.x) * k;
      unit.drawY = from.y + (to.y - from.y) * k;
      // The camera follows the walk of whoever's turn it is: a hero, or the enemy taking its own
      // turn (activeUnitId stays on it while its queued moves play) — the same follow for both.
      if ((unit.side === "player" && this.activeTurnUnit()?.id === unit.id) || (unit.side !== "player" && this.activeUnitId === unit.id)) {
        const cx = fromScreen.cx + (toScreen.cx - fromScreen.cx) * k;
        const cy = fromScreen.cy + (toScreen.cy - fromScreen.cy) * k;
        this.centerOnPoint(cx, cy);
      }
      if (a.t >= dur) {
        a.i += 1;
        // Carry the leftover into the next hex so the glide never hitches between steps.
        a.t = Math.min(a.t - dur, dur);
        unit.x = to.x;
        unit.y = to.y;
        unit.drawX = to.x;
        unit.drawY = to.y;
        if (unit.side === "player") this.waypointCheckPending = true;
        this.ensureVisible(unit.x, unit.y);
        // Walking can change the world — a troll shoulders a barricade down, a hazard tile
        // bites. Either one makes the move unrewindable: undoMove can put a unit back, it
        // cannot un-break a wall or un-take damage. Note it and the undo bows out.
        const hpBefore = unit.hp;
        const propsBefore = this.decorations.length;
        this.smashBarricades(unit);
        this.applyTileHazard(unit, to);
        if (unit.alive && this.tryWarpCrossing(unit, a)) return;
        if (unit.hp !== hpBefore || this.decorations.length !== propsBefore) this.moveSpoiled = true;
        if (!unit.alive) {
          if (unit.sprite === "plague-bearing-cattle") sfxPlay.plagueCattleWalkStop();
          sfxPlay.monsterWalkStop(unit.sprite);
          this.active = null;
          this.selectedId = null;
          this.pendingFoeId = null;
          this.evaluateEnd();
          if (!this.result && this.phase === "player") this.mode = "idle";
        }
      }
      return;
    }
    // Movement already scales its own duration directly off speedMode (see `dur` above) —
    // combat/spell/heal never did, they always ran at one hardcoded pace no matter what the
    // player picked, which is why "Lenta" visibly slowed walking but did nothing for the part
    // that actually needs slowing down: the hit itself, a bolt's flight, an AOE's burst. This
    // scales the dt these four steppers see instead of touching every fixed-time threshold
    // inside them individually (stepCombat/stepSpell/stepHeal/stepCureDisease are full of
    // those, e.g. stepSpell's finishCombat/afterglow timing) — a smaller dt makes `a.t` climb
    // toward those same unchanged thresholds more slowly, uniformly stretching the whole
    // animation without touching the careful relative timing between its stages.
    // "Rápida" keeps today's actual speed (unchanged, for players who already picked it and
    // are happy with it); "Normal" is deliberately slowed down some on its own, since this was
    // the direct, repeated report — the default pace read as too fast to actually see what
    // just happened; "Lenta" is slowed down a lot, enough to really watch a cast land.
    const speedScale = this.speedMode === "fast" ? 1 : this.speedMode === "slow" ? 0.4 : 0.65;
    const actionDt = dt * speedScale * this.longSheetActionPace(a, speedScale);
    if (a.type === "combat") this.stepCombat(a, actionDt);
    if (a.type === "spell") this.stepSpell(a, actionDt);
    if (a.type === "heal") this.stepHeal(a, actionDt);
    if (a.type === "cureDisease") this.stepCureDisease(a, actionDt);
  }

  private stepCombat(a: CombatAnim, dt: number): void {
    const att = this.units.find((u) => u.id === a.att);
    const def = this.units.find((u) => u.id === a.def);
    if (!att || !def) {
      this.active = null;
      return;
    }
    a.t += dt;
    const lunge = 0.2;
    if (a.stage === "counterLunge" && a.counterWindAt != null && this.time - a.counterWindAt < LONG_ARROW_RELEASE_SECONDS) {
      a.t = 0;
      return;
    }
    if (a.stage === "counterLunge" && a.counterWindAt != null && a.t < lunge) a.t = lunge;
    if (a.stage === "lunge" || a.stage === "counterLunge") {
      const actor = a.stage === "lunge" ? att : def;
      const target = a.stage === "lunge" ? def : att;
      const k = Math.min(1, a.t / lunge);
      const arrowShot = this.isArrowAttack(actor) && !this.offHandStrike(a);
      const arcaneBolt = !arrowShot && this.isArcaneCaster(actor);
      const ranged = arrowShot || arcaneBolt;
      actor.drawX = actor.x + (target.x - actor.x) * (ranged ? 0 : 0.28) * k;
      actor.drawY = actor.y + (target.y - actor.y) * (ranged ? 0 : 0.28) * k;
      if (a.t >= lunge) {
        if (arrowShot) {
          this.emitMissileFx(actor.x, actor.y, target.x, target.y, "longShot");
        } else if (arcaneBolt) {
          this.emitMissileFx(actor.x, actor.y, target.x, target.y, "arcaneBolt");
        } else if (actor.sprite === "kaelFinal" && !this.offHandStrike(a) && !(a.stage === "lunge" && a.spellKind === "shieldBash")) {
          // Cued here, ~0.74 s into his 2 s sheet. BladeSlash1Dagger.mp3 has ~1.1 s of silence
          // before its swoosh, loudest at ~1.45 s — starting 1.12 s in lands that peak on
          // frame 23, the big horizontal slash (~1.07 s). Same for his counter.
          this.playMeleeCue(actor, false, a.stage === "lunge" ? a.spellKind : undefined, KAEL_BLADE_SOUND_START);
        }
        a.t = 0;
        a.stage = a.stage === "lunge" ? "hit" : "counterHit";
      }
      return;
    }
    if (a.stage === "hit" || a.stage === "counterHit") {
      const actor = a.stage === "hit" ? att : def;
      const target = a.stage === "hit" ? def : att;
      const arrowShot = this.isArrowAttack(actor) && !this.offHandStrike(a);
      const arcaneBolt = !arrowShot && this.isArcaneCaster(actor);
      const impactAt = arrowShot ? ARROW_TRAVEL : arcaneBolt ? MISSILE_TRAVEL : 0.02;
      if (a.t >= impactAt && a.t - dt < impactAt) {
        if (a.stage === "hit" && a.spellKind === "shieldBash") sfxPlay.shieldBash();
        const attTile = tileAt(this.tiles, this.cols, actor.x, actor.y);
        const defTile = tileAt(this.tiles, this.cols, target.x, target.y);
        // customDice/dmgMul/stunChance are the attacker's own strike (off-hand weapon or
        // Shield Bash) — never applied to the defender's counter, which always uses their
        // real equipped weapon at full strength.
        const dice = a.stage === "hit" ? a.customDice : a.counterCustomDice;
        const hit = dice
          ? rollDamageCustom(this.affinityUnit(actor), this.affinityUnit(target), attTile, defTile, dice.dice, dice.faces, dice.bonus, this.rng, !(a.stage === "hit" && a.spellKind === "shieldBash"))
          : rollDamage(this.affinityUnit(actor), this.affinityUnit(target), attTile, defTile, this.rng, !(a.stage === "hit" && a.spellKind === "shieldBash"));
        if (target.side === "enemy" && !(a.stage === "hit" && a.spellKind === "shieldBash")) this.trainWeapon(actor, equippedWeaponType(actor, !!dice), target.level);
        const usesArcane = arcaneBolt && !this.offHandStrike(a) && (a.stage === "counterHit" || !a.spellKind);
        const usesStaffElement = !!this.staffMagic(actor) && !dice && (a.stage === "counterHit" || !a.spellKind);
        if (!hit.landed) {
          this.spawnMiss(target);
          this.pushLog(`${actor.name} atacou ${target.name}: Missed`);
          sfxPlay.miss();
          // A missed Bull Rush hit shoves nobody aside, so the charge ends here — the rest of
          // it would run straight through the enemy that is still standing in the way.
          if (a.stage === "hit" && a.spellKind === "bullRush") {
            this.queue = this.queue.filter((q) => !(q.type === "move" && q.id === actor.id) && !("att" in q && q.att === actor.id && q.type === "combat" && q.spellKind === "bullRush"));
          }
        } else {
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
            sfxPlay.stun();
          }
          if (target.asleep) {
            hit.dmg = Math.max(1, Math.floor(hit.dmg * (1 + WEB_OF_DREAMS.sleepBonusDamage)));
            target.asleep = false;
            target.sleepTurns = 0;
          }
          hit.dmg = Math.max(1, Math.floor(hit.dmg * this.zoneDamageMul(target)));
          if (usesArcane || usesStaffElement) {
            const magic = !dice ? this.staffMagic(actor) : undefined;
            const element = magic?.attackElement ?? "arcane";
            hit.dmg = Math.floor(elementalDamage(staffElementalDamage(hit.dmg, magic, element), target.resistances?.[element] ?? 0, this.affinityUnit(actor).mag));
            this.trainResistance(target, element, actor.level);
          }
          if (a.stage === "hit" && a.spellKind && hit.dmg > 0) this.adjustAffinity(actor, target, -1);
          const actualStaffDamage = Math.min(target.hp, hit.dmg);
          target.hp = Math.max(0, target.hp - hit.dmg);
          if (target.side !== actor.side && !dice) this.staffDrain(actor, actualStaffDamage);
          this.noteDamageEnmity(actor, target, hit.dmg, "weapon");
          target.flash = 1;
          target.hitAt = this.time;
          this.provoke(target, actor);
          if (target.side !== actor.side) {
            if (a.stage === "hit") {
              this.gainExp(actor, target.level, hit.dmg, 1, target.hp <= 0);
            } else {
              // A counter deals its full real damage but only ever earns a flat 1 XP — see
              // gainCounterExp — so a unit can't out-level by baiting hits and countering
              // instead of attacking.
              this.gainCounterExp(actor);
            }
          }
          this.spawnHit(target, hit.dmg, hit.crit, !this.isArrowAttack(actor) && !this.isArcaneCaster(actor));
          this.pushLog(`${actor.name} atacou ${target.name}: ${hit.dmg} dano${hit.crit ? " (crítico)" : ""}`);
          // The blade swoosh is the visual for the strike landing, not for the target
          // surviving it — fire it here, unconditionally, same as spawnHit/pushLog above,
          // rather than nested under the "target lived" branch below (where it used to be
          // silently skipped on any kill).
          if (a.stage === "hit" && a.spellKind === "trip") {
            const oc = this.hexCenter(actor.x, actor.y);
            const tc = this.hexCenter(target.x, target.y);
            this.emitBladeFx("tripSweep", target.x, target.y, { a0: Math.atan2(tc.cy - oc.cy, tc.cx - oc.cx) });
          }
          if (a.stage === "hit" && a.spellKind === "doubleStrike") {
            const oc = this.hexCenter(actor.x, actor.y);
            const tc = this.hexCenter(target.x, target.y);
            const base = Math.atan2(tc.cy - oc.cy, tc.cx - oc.cx);
            this.doubleStrikeAlt = !this.doubleStrikeAlt;
            this.emitBladeFx("cross", target.x, target.y, { a0: base + (this.doubleStrikeAlt ? 0.7 : -0.7) });
          }
          if (a.stage === "hit" && a.spellKind === "bullRush") {
            const oc = this.hexCenter(actor.x, actor.y);
            const tc = this.hexCenter(target.x, target.y);
            this.emitBladeFx("rushImpact", target.x, target.y, { a0: Math.atan2(tc.cy - oc.cy, tc.cx - oc.cx) });
          }
          if (a.stage === "hit" && a.spellKind === "shieldBash") this.emitBladeFx("shockRing", target.x, target.y);
          if (a.stage === "hit" && a.spellKind === "executionerStrike") {
            const attackerCenter = this.hexCenter(actor.x, actor.y);
            const targetCenter = this.hexCenter(target.x, target.y);
            const fromLeft = attackerCenter.cx < targetCenter.cx ||
              (attackerCenter.cx === targetCenter.cx && actor.x < target.x);
            this.emitBladeFx("execution", target.x, target.y, { warm: executed, mirrorX: fromLeft });
          }
          if (target.hp <= 0) {
            this.markDead(target);
          } else {
            sfxPlay.hit();
            if (a.stage === "hit") this.maybeInflictDisease(actor, target);
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
              if (actor.name !== "Kael") sfxPlay.trip(this.isBladeAttack(actor));
            }
            if (a.stage === "hit" && a.spellKind === "shieldBash") {
              target.stunned = true;
              target.stunTurns = shieldBashPower(actor.level).stunTurns;
              sfxPlay.stun();
            }
            if (a.stage === "hit" && a.spellKind === "bullRush" && a.knockTo) {
              target.x = a.knockTo.x;
              target.y = a.knockTo.y;
              target.drawX = a.knockTo.x;
              target.drawY = a.knockTo.y;
              this.emitParticle({
                x: target.drawX,
                y: target.drawY + 0.3,
                vx: 0,
                vy: 0,
                life: 0,
                max: 0.35,
                size: 1,
                color: "#c9b28a",
                kind: "impact",
                frame: 0,
              });
            }
          }
        }
        if (!this.reducedMotion) this.trauma = Math.min(1, this.trauma + (hit.landed ? 0.28 : 0.08));
        this.hitstop = hit.landed ? 0.06 : 0;
      }
      if (a.t >= (arrowShot ? ARROW_TRAVEL + 0.18 : this.isArcaneCaster(actor) ? MISSILE_TRAVEL + 0.18 : 0.18)) {
        a.t = 0;
        a.stage = a.stage === "hit" ? "recover" : "counterRecover";
      }
      return;
    }
    if (a.stage === "recover" || a.stage === "counterRecover") {
      const actor = a.stage === "recover" ? att : def;
      const k = Math.min(1, a.t / 0.16);
      actor.drawX = actor.drawX + (actor.x - actor.drawX) * k;
      actor.drawY = actor.drawY + (actor.y - actor.drawY) * k;
      // A wound-up bow shot also waits for the archer's follow-through to finish.
      if (a.t >= 0.16 && (a.stage !== "recover" || this.heldDone(a))) {
        actor.drawX = actor.x;
        actor.drawY = actor.y;
        a.t = 0;
        if (a.stage === "recover") {
          if (!a.noCounter && !def.stunned && def.alive && canCounter(att, def, { x: att.x, y: att.y }, this.tiles, this.cols)) {
            this.faceSpriteToward(def.id, att.x, att.y);
            const offHand = def.offHandId ? EQUIPMENT[def.offHandId] : null;
            // The off-hand dagger/katar counters only an attacker within its own reach;
            // anyone further away gets the main weapon (the bow).
            const inDaggerReach = offHand?.kind === "weapon" && hexDist(def, att) <= (offHand.maxRange ?? 1);
            a.counterCustomDice = inDaggerReach ? { dice: offHand.dice ?? 1, faces: offHand.faces ?? 4, bonus: offHand.bonus ?? 0 } : null;
            // A long-sheet bow counter plays the whole ATT sheet before the arrow leaves,
            // same as that unit's own ATT shot.
            const counterSheet = this.art.counters[def.sprite] ?? this.art.attacks[def.sprite];
            a.counterWindAt =
              !a.counterCustomDice && this.isArrowAttack(def) && (counterSheet?.length ?? 0) >= LONG_SHEET_FRAMES && !this.reducedMotion ? this.time : undefined;
            // startSeq only cues the attacker's own strike, so the counter plays its cue here,
            // as the counter animation begins.
            if (!a.counterCustomDice && this.isArrowAttack(def)) sfxPlay.arrowAttack(def.sprite === "neera", a.counterWindAt != null ? LONG_ARROW_RELEASE_SECONDS : 0);
            else if (this.isArcaneCaster(def)) {
              if (def.sprite === "cultist-v2") sfxPlay.cultistV2Attack();
              else if (!sfxPlay.monster(def.sprite, "attack")) sfxPlay.magicAttack();
            } else if (def.sprite === "minor-horror-001") sfxPlay.minorHorrorAttack();
            else if (def.sprite === "carnivorous-plant-001") sfxPlay.carnivorousPlantAttack();
            else if (def.sprite === "sapling-001") sfxPlay.saplingAttack();
            else if (def.sprite === "plague-bearing-cattle") sfxPlay.plagueCattleAttack();
            else if (this.monsterAttackCue(def)) {
              // A monster's own attack cue (audio.ts MONSTER_SFX).
            }
            else if (def.sprite !== "kaelFinal" || a.counterCustomDice) this.playMeleeCue(def, !!a.counterCustomDice);
            a.stage = "counterLunge";
          }
          else if (!def.alive) a.stage = "fade";
          else this.finishCombat(att);
        } else if (!att.alive) a.stage = "fade";
        else this.finishCombat(att);
      }
      return;
    }
    if (a.stage === "fade") {
      for (const u of this.units) {
        if (!u.alive && u.fade > 0 && !this.deathSheetPlaying(u)) u.fade = Math.max(0, u.fade - dt * 2.4);
      }
      if (a.t >= 0.4) this.finishCombat(att);
    }
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
  private spellDamage(att: Unit, foe: Unit, mul: number, roll: number): number {
    att = this.affinityUnit(att);
    foe = this.affinityUnit(foe);
    const attTile = this.hexAt(att.x, att.y);
    const defTile = this.hexAt(foe.x, foe.y);
    const prot = 0;
    const spell = Math.floor(powerOf(att) * mul) + roll + attTile.atk - prot - (defTile.cover ?? 0);
    const plain = powerOf(att) + weaponRoll(att.weaponId, att.weaponEnh, this.rng) + attTile.atk - prot - defTile.def;
    return Math.max(1, Math.floor(Math.max(spell, plain)));
  }

  private stepSpell(a: SpellAnim, dt: number): void {
    const att = this.units.find((u) => u.id === a.att);
    if (!att) {
      this.active = null;
      return;
    }
    a.t += dt;
    if (a.spellKind === "bless") {
      if (this.blessVfxAvailable && !this.reducedMotion) {
        if (!a.blessVfxId) {
          const id = `bless-${++this.blessVfxSequence}`;
          a.blessVfxId = id;
          const allies = a.ids.map((unitId) => this.units.find((unit) => unit.id === unitId && unit.alive)).filter((unit): unit is Unit => !!unit);
          this.blessVfxRequests.push({ id, center: { x: att.x, y: att.y }, allies: allies.map((unit) => ({ id: unit.id, x: unit.x, y: unit.y, distanceHexes: hexDist(att, unit) })) });
        }
        for (let index = this.blessVfxEvents.length - 1; index >= 0; index--) {
          const event = this.blessVfxEvents[index]!;
          if (event.id !== a.blessVfxId) continue;
          this.blessVfxEvents.splice(index, 1);
          if (event.phase === "complete") a.blessComplete = true;
          else if (event.phase === "timeline") continue;
          else {
            const applied = a.blessAppliedIds ?? (a.blessAppliedIds = []);
            if (applied.includes(event.unitId)) continue;
            const ally = this.units.find((unit) => unit.id === event.unitId && unit.alive && unit.side === att.side);
            if (ally) { this.applyBless(ally, att.level); this.gainSupportAffinity(att, ally); }
            applied.push(event.unitId);
          }
        }
      } else if (a.t >= 0.3) {
        for (const id of a.ids) {
          const applied = a.blessAppliedIds ?? (a.blessAppliedIds = []);
          if (applied.includes(id)) continue;
          const ally = this.units.find((unit) => unit.id === id && unit.alive && unit.side === att.side);
          if (ally) { this.applyBless(ally, att.level); this.gainSupportAffinity(att, ally); }
          applied.push(id);
        }
        if (a.t >= 1.5) a.blessComplete = true;
      }
      if (a.blessComplete && this.heldDone(a)) this.finishCombat(att);
      return;
    }
    const syncFireballVfx = a.spellKind === "fireball" && this.fireballVfxAvailable && !this.reducedMotion && !!a.projectileTo;
    const syncCausticVenomVfx = a.spellKind === "causticVenom" && att.classId !== "carnivorousPlant" && this.causticVenomVfxAvailable && !this.reducedMotion && !!a.projectileTo;
    const syncPhantasmalVfx = a.spellKind === "phantasmalForce" && this.phantasmalForceVfxAvailable && !this.reducedMotion;
    const syncBurningHandsVfx = (a.spellKind === "burningHands" || a.spellKind === "poisonBreath") && this.burningHandsV2VfxAvailable && !this.reducedMotion;
    if (a.spellKind === "cleave" && !a.cleaveVfxQueued && a.t >= 0.18 && !this.reducedMotion) {
      a.cleaveVfxQueued = true;
      this.cleaveVfxRequests.push({ id: `cleave-sweep-${++this.cleaveVfxSequence}`, casterId: att.id, tiles: a.tiles.map((tile) => ({ ...tile })), targetIds: [...a.ids] });
    }
    if (a.spellKind === "sweep" && !a.varreduraVfxQueued && a.t >= 0.18 && !this.reducedMotion) {
      a.varreduraVfxQueued = true;
      this.varreduraVfxRequests.push({ id: `varredura-${++this.varreduraVfxSequence}`, casterId: att.id, tiles: a.tiles.map((tile) => ({ ...tile })), targetIds: [...a.ids] });
    }
    if (syncBurningHandsVfx) {
      if (!a.burningHandsVfxId) {
        const id = `burning-hands-v2-${++this.burningHandsV2VfxSequence}`;
        a.burningHandsVfxId = id;
        this.burningHandsV2VfxRequests.push({ id, casterId: att.id, tiles: a.tiles.map((tile) => ({ ...tile })), poison: a.spellKind === "poisonBreath" });
      }
      for (let index = this.burningHandsV2VfxEvents.length - 1; index >= 0; index--) {
        const event = this.burningHandsV2VfxEvents[index]!;
        if (event.id !== a.burningHandsVfxId) continue;
        this.burningHandsV2VfxEvents.splice(index, 1);
        if (event.phase === "release") a.burningHandsReleased = true;
        else a.burningHandsComplete = true;
      }
      // Preserve authoritative damage timing if the renderer is interrupted mid-cast.
      if (!a.burningHandsReleased && a.t >= 0.7) a.burningHandsReleased = true;
      if (!a.burningHandsComplete && a.t >= 2.5) a.burningHandsComplete = true;
    }
    if (syncFireballVfx) {
      if (!a.fireballVfxId) {
        const id = `fireball-${++this.fireballVfxSequence}`;
        a.fireballVfxId = id;
        this.fireballVfxRequests.push({ id, casterId: att.id, target: { ...a.projectileTo! }, tiles: a.tiles.map((tile) => ({ ...tile })) });
      }
      for (let index = this.fireballVfxEvents.length - 1; index >= 0; index--) {
        const event = this.fireballVfxEvents[index]!;
        if (event.id !== a.fireballVfxId) continue;
        this.fireballVfxEvents.splice(index, 1);
        if (event.phase === "launch") {
          a.fireballVfxLaunched = true;
          // The fallback remains until the 3D renderer confirms it has taken over the shot.
          for (const missile of this.missileFx) if (missile.kind === "fireball") missile.live = false;
        } else if (event.phase === "impact") a.fireballImpact = true;
        else if (event.phase === "complete") a.fireballComplete = true;
      }
    }
    if (syncCausticVenomVfx) {
      if (!a.causticVenomVfxId) {
        const id = `caustic-venom-${++this.causticVenomVfxSequence}`;
        a.causticVenomVfxId = id;
        this.causticVenomVfxRequests.push({ id, casterId: att.id, target: { ...a.projectileTo! }, tiles: a.tiles.map((tile) => ({ ...tile })) });
      }
      for (let index = this.causticVenomVfxEvents.length - 1; index >= 0; index--) {
        const event = this.causticVenomVfxEvents[index]!;
        if (event.id !== a.causticVenomVfxId) continue;
        this.causticVenomVfxEvents.splice(index, 1);
        if (event.phase === "impact") a.causticVenomImpact = true;
        else if (event.phase === "complete") a.causticVenomComplete = true;
      }
      // If the 3D renderer is interrupted mid-cast, combat still resolves on schedule.
      if (!a.causticVenomImpact && a.t >= SPELL_TRAVEL + 0.38) a.causticVenomImpact = true;
      if (!a.causticVenomComplete && a.t >= SPELL_TRAVEL + 2.3) a.causticVenomComplete = true;
    }
    if (syncPhantasmalVfx) {
      if (!a.phantasmalVfxId) {
        const id = `phantasmal-${++this.phantasmalForceVfxSequence}`;
        a.phantasmalVfxId = id;
        const target = a.tiles[0];
        if (target) this.phantasmalForceVfxRequests.push({ id, target: { ...target }, targetUnitId: a.ids[0] ?? "" });
      }
      for (let index = this.phantasmalForceVfxEvents.length - 1; index >= 0; index--) {
        const event = this.phantasmalForceVfxEvents[index]!;
        if (event.id !== a.phantasmalVfxId) continue;
        this.phantasmalForceVfxEvents.splice(index, 1);
        if (event.phase === "impact") a.phantasmalImpact = true;
        else a.phantasmalComplete = true;
      }
    }
    const syncMagicMissileV2Vfx = a.spellKind === "magicMissileV2" && this.magicMissileV2VfxAvailable && !this.reducedMotion;
    if (syncMagicMissileV2Vfx) {
      if (!a.magicMissileV2VfxId) {
        const targetUnitId = a.ids[0];
        const target = targetUnitId ? this.units.find((unit) => unit.id === targetUnitId) : undefined;
        if (target) {
          const id = `magic-missile-v2-${++this.magicMissileV2VfxSequence}`;
          a.magicMissileV2VfxId = id;
          this.magicMissileV2VfxRequests.push({ id, casterId: att.id, targetUnitId: target.id });
        } else a.magicMissileV2VfxId = "";
      }
      for (let index = this.magicMissileV2VfxEvents.length - 1; index >= 0; index--) {
        const event = this.magicMissileV2VfxEvents[index]!;
        if (event.id !== a.magicMissileV2VfxId) continue;
        this.magicMissileV2VfxEvents.splice(index, 1);
        // Each queued target shot has its own damage roll and complete hero-missile impact.
        if (event.phase === "impact") {
          if (event.index === 0) a.magicMissileV2Impact = true;
        } else a.magicMissileV2Complete = true;
      }
      // Keep combat authoritative if the renderer is torn down or misses an event. These
      // fallbacks sit after the deliberate V2 charge, flight and residual-light sequence.
      if (!a.magicMissileV2Impact && a.t >= 2.5) a.magicMissileV2Impact = true;
      if (!a.magicMissileV2Complete && a.t >= 3.2) a.magicMissileV2Complete = true;
    }
    const arrowSpell = a.spellKind === "longShot" || a.spellKind === "bloodyShot" || a.spellKind === "multiShot" || a.spellKind === "piercing";
    const hitAt = a.spellKind === "frost" ? .75 : arrowSpell ? ARROW_TRAVEL : a.spellKind === "fantomForce" ? FANTOM_FORCE_TRAVEL : a.spellKind === "phantasmalForce" ? PHANTASMAL_FORCE_TRAVEL : a.spellKind === "magicMissile" || a.spellKind === "magicMissileV2" || a.spellKind === "fireball" || a.spellKind === "causticVenom" || a.spellKind === "minorVenom" ? SPELL_TRAVEL : 0.18;
    if (syncMagicMissileV2Vfx && a.magicMissileV2VfxId === "") {
      if (a.t >= hitAt) a.magicMissileV2Impact = true;
    }
    if (!a.hit && (syncFireballVfx ? a.fireballImpact === true : syncCausticVenomVfx ? a.causticVenomImpact === true : syncPhantasmalVfx ? a.phantasmalImpact === true : syncMagicMissileV2Vfx ? a.magicMissileV2Impact === true : syncBurningHandsVfx ? a.burningHandsReleased === true : a.t >= hitAt)) {
      a.hit = true;
      if (a.spellKind === "turnUndead") this.turnUndeadFx.push({ tiles: a.tiles.map((p) => ({ ...p })), t: 0.001 });
      const usedElement = spellElement(a.spellKind);
      // Every elemental spell trains its caster's matching resistance ("improves by using"),
      // not just Magic Missile: once per cast, against the first enemy it reaches.
      if (usedElement) {
        const enemy = a.ids.map(id => this.units.find(u => u.id === id && u.alive && u.side === "enemy")).find(Boolean);
        if (enemy) this.trainElementUse(att, usedElement, enemy.level);
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
        if (!foe || (a.spellKind === "turnUndead" && !isUndeadClass(foe.classId))) continue;
        const defTile = this.hexAt(foe.x, foe.y);
        if (defTile.id === "barricade") {
          this.emitParticle({
            x: foe.drawX,
            y: foe.drawY - 0.35,
            vx: 0,
            vy: -0.18,
            life: 0,
            max: 1.6,
            size: 1,
            color: "#e0b48a",
            text: "bloqueado",
            kind: "text",
            frame: 0,
          });
          continue;
        }
        let dmg: number;
        let crit = false;
        let landed = true;
        let weaponRolled = false;
        if (a.centerId && foe.id === a.centerId) {
          dmg = this.spellDamage(att, foe, a.centerMul, rollDice(a.centerDice, a.centerFaces, a.centerBonus, this.rng));
        } else if (a.extraDice > 0) {
          let roll = rollDice(a.extraDice, a.extraFaces, a.extraBonus, this.rng);
          if (a.moreDice > 0) roll += rollDice(a.moreDice, a.moreFaces, 0, this.rng);
          dmg = this.spellDamage(att, foe, a.spellMul, roll);
        } else if (a.spellKind === "piercingThrust") {
          // Armor-piercing: the defender's DEF is treated as 20% lower for this hit only.
          const softened = { ...foe, def: Math.max(0, Math.floor(foe.def * (1 - PIERCING_THRUST.armorIgnore))) };
          const hit = rollDamage(
            this.affinityUnit(att),
            this.affinityUnit(softened),
            tileAt(this.tiles, this.cols, att.x, att.y),
            tileAt(this.tiles, this.cols, foe.x, foe.y),
            this.rng,
          );
          weaponRolled = true;
          landed = hit.landed;
          dmg = thrustHitIndex === 0 ? hit.dmg : Math.max(1, Math.floor(hit.dmg * 0.5));
          crit = hit.crit;
          // A miss doesn't count as a body the thrust passed through — only a landed hit
          // advances the front-to-back falloff.
          if (landed) thrustHitIndex++;
        } else {
          const hit = rollDamage(
            this.affinityUnit(att),
            this.affinityUnit(foe),
            tileAt(this.tiles, this.cols, att.x, att.y),
            tileAt(this.tiles, this.cols, foe.x, foe.y),
            this.rng,
            isWeaponAbility(a.spellKind),
          );
          weaponRolled = true;
          landed = hit.landed;
          dmg = hit.dmg;
          crit = hit.crit;
          if (a.weaponBonusDice > 0 && landed) dmg += rollDice(a.weaponBonusDice, a.weaponBonusFaces, a.weaponBonusBonus, this.rng);
        }
        if (isWeaponAbility(a.spellKind)) {
          const type = equippedWeaponType(att);
          if (!weaponRolled) {
            const mastery = weaponModifiers(att, type);
            const accuracy = dexAccuracy(mastery.accuracy, this.affinityUnit(foe).dex);
            landed = accuracy >= 100 || this.rng() * 100 < accuracy;

          }
          if (foe.side === "enemy") this.trainWeapon(att, type, foe.level);
        }
        if (!landed) {
          this.spawnMiss(foe);
          this.pushLog(`${att.name} atacou ${foe.name}: Missed`);
          sfxPlay.miss();
          continue;
        }
        if (a.spellKind === "fantomForce") dmg = Math.max(1, Math.floor(dmg * FANTOM_FORCE.damageMul));
        if (a.dmgMul > 1) dmg = Math.max(1, Math.floor(dmg * a.dmgMul));
        if (a.spellKind === "cleave" && cleaveDoublesVs(foe)) dmg = Math.max(1, Math.floor(dmg * CLEAVE.largeMul));
        if (foe.asleep) {
          dmg = Math.max(1, Math.floor(dmg * (1 + WEB_OF_DREAMS.sleepBonusDamage)));
          foe.asleep = false;
          foe.sleepTurns = 0;
        }
        dmg = Math.max(1, Math.floor(dmg * this.zoneDamageMul(foe)));
        const element = spellElement(a.spellKind);
        if (element) dmg = Math.floor(elementalDamage(staffElementalDamage(dmg, this.staffMagic(att), element), foe.resistances?.[element] ?? 0, this.affinityUnit(att).mag));
        if (dmg > 0 && a.spellKind) this.adjustAffinity(att, foe, -1);
        const actualStaffDamage = Math.min(foe.hp, dmg);
        foe.hp = Math.max(0, foe.hp - dmg);
        if (element && foe.side !== att.side) this.staffDrain(att, actualStaffDamage);
        if (a.spellKind === "turnUndead" && foe.hp > 0) {
          foe.fearTurns = Math.max(foe.fearTurns ?? 0, TURN_UNDEAD.fearTurns);
          foe.fearSourceId = att.id;
          this.pushLog(`${foe.name} fears the divine light and retreats for ${TURN_UNDEAD.fearTurns} turns.`);
        }
        this.noteDamageEnmity(att, foe, dmg, isWeaponAbility(a.spellKind) ? "weapon" : "spell");
        foe.flash = 1;
        foe.hitAt = this.time;
        this.provoke(foe, att);
        const poisonTier: PoisonTier = a.spellKind === "causticVenom" ? "poison" : "lesser";
        if (a.poison && this.rng() * 100 < poisonChance(effectivePoisonResistance(foe.resistances?.poison ?? 0, poisonTier, this.affinityUnit(att).mag))) {
          const currentTier = foe.poisoned ? foe.poisonTier : undefined;
          const nextTier = strongerPoison(currentTier, poisonTier);
          if (!currentTier || nextTier !== currentTier) foe.poisonMag = this.affinityUnit(att).mag;
          else if (nextTier === poisonTier) foe.poisonMag = Math.max(foe.poisonMag ?? 0, this.affinityUnit(att).mag);
          foe.poisonTier = nextTier;
          foe.poisoned = true;
        }
        if (element) this.trainResistance(foe, element, att.level);
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
              this.emitParticle({
                x: healer.drawX,
                y: healer.drawY - 0.35,
                vx: 0,
                vy: -0.18,
                life: 0,
                max: 2,
                size: 1,
                color: "#d8ead2",
                text: `+${gained}`,
                kind: "text",
                frame: 0,
              });
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
          const isAoeSpell =
            a.spellKind === "turnUndead" ||
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
          const xpMul =
            foe.hp <= 0 && !isAoeSpell && (att.classId === "mage" || att.classId === "voss" || att.classId === "conjurer" || a.spellKind === "longShot" || a.spellKind === "bloodyShot")
              ? 2
              : isAoeSpell && !firstAoeEnemyHit
                ? 0.5
                : 1;
          if (isAoeSpell) firstAoeEnemyHit = false;
          // The universal finishing-blow bonus stacks multiplicatively on top of xpMul —
          // it doesn't replace the mage/conjurer/Long Shot kill bonus above or the AoE
          // per-target share, it applies in addition to whichever of those already fired.
          this.gainExp(att, foe.level, dmg, xpMul, foe.hp <= 0);
        }
        const meleeSkill = a.spellKind === "doubleStrike" || a.spellKind === "cleave" || a.spellKind === "piercingThrust" || a.spellKind === "sweep" || a.spellKind === "trip" || a.spellKind === "shoulderSmash" || a.spellKind === "stampede" || a.spellKind === "tendrilSwipe";
        this.spawnHit(foe, dmg, crit, meleeSkill);
        const large = a.spellKind === "cleave" && cleaveDoublesVs(foe);
        this.pushLog(
          `${att.name} atingiu ${foe.name} com magia: ${dmg} dano${crit ? " (crítico)" : ""}${large ? " · Cleave x2 criatura grande" : ""}`,
        );
        if (foe.hp <= 0) {
          this.markDead(foe);
        } else {
          sfxPlay.hit();
          if (a.echo) foe.shock = { ...a.echo, mag: this.affinityUnit(att).mag };
          if (a.spellKind === "bloodyShot" && (!foe.bleeding || foe.bleedRoundsLeft != null)) {
            const duration = bloodyShotBleed(att.level);
            foe.bleeding = true;
            foe.bleedRoundsLeft = rollDice(duration.dice, duration.faces, 0, this.rng);
            foe.bleedRoundMarker = this.turn;
            this.pushLog(`${foe.name} sangra por ${foe.bleedRoundsLeft} rodadas (1D8 por ação).`);
          }
          if (a.spellKind === "sweep") this.knockBack(att, foe);
          if (a.spellKind === "shoulderSmash") {
            for (let i = 0; i < SHOULDER_SMASH.knockback; i++) this.knockBack(att, foe);
          }
        }
      }
      // Veneno Menor keeps the original venom impact: the green burst on every splash hex.
      if (a.spellKind === "poisonBreath" && !syncBurningHandsVfx) this.emitFireballBurstFx(a.tiles, "causticVenom");
      if (a.spellKind === "minorVenom") this.emitFireballBurstFx(a.tiles, "causticVenom");
      if (a.spellKind === "causticVenom" && att.classId === "carnivorousPlant") this.emitFireballBurstFx(a.tiles, "causticVenom");
      const elementFx = a.spellKind ? SPELL_ELEMENT_FX[a.spellKind] : undefined;
      if (elementFx && !(a.spellKind === "burningHands" && syncBurningHandsVfx) && !(a.spellKind === "causticVenom" && syncCausticVenomVfx)) this.queueElementalFx(elementFx.kind, a.tiles, elementFx.duration);
      if (((a.spellKind === "cleave" && !a.cleaveVfxQueued) || a.spellKind === "shoulderSmash") && a.tiles.length > 0) {
        const { a0, a1 } = this.arcSweepAngles({ x: att.x, y: att.y }, a.tiles);
        this.emitBladeFx("arc", att.x, att.y, { a0, a1, warm: a.spellKind === "shoulderSmash" });
      }
      if (a.spellKind === "sweep" && !a.varreduraVfxQueued) this.emitBladeFx("ring", att.x, att.y);
      if ((a.spellKind === "piercingThrust" || a.spellKind === "stampede") && a.tiles.length > 0) {
        const end = a.tiles[a.tiles.length - 1]!;
        this.emitBladeFx("dash", att.x, att.y, { toX: end.x, toY: end.y });
      }
      if (!this.reducedMotion) this.trauma = Math.min(1, this.trauma + (a.spellKind === "lightningTier3" ? 0.95 : a.spellKind === "lightning" ? 0.72 : 0.45));

    }
    // The Conjurer has a 36-frame casting sheet. Let it finish its visual motion without
    // changing the hit timing above; every other spell keeps the existing duration.
    // The slower spell bolts (SPELL_TRAVEL) need the step to outlast their flight, impact and
    // trail afterglow.
    const boltSpell = a.spellKind === "magicMissile" || a.spellKind === "magicMissileV2" || a.spellKind === "fireball" || a.spellKind === "causticVenom" || a.spellKind === "minorVenom" || a.spellKind === "fantomForce";
    const spellEnd = Math.max(a.spellKind === "frost" ? 1.1 : 0,att.sprite === "conjurer" ? 0.72 : 0.55, boltSpell ? SPELL_TRAVEL + MISSILE_AFTERGLOW + 0.15 : 0, syncPhantasmalVfx ? 1.5 : 0, syncCausticVenomVfx ? SPELL_TRAVEL + 2.3 : 0);
    if (syncMagicMissileV2Vfx && a.magicMissileV2VfxId === "" && a.t >= spellEnd) a.magicMissileV2Complete = true;
    if (syncFireballVfx) {
      if (a.fireballComplete && this.heldDone(a)) this.finishCombat(att);
    } else if (syncCausticVenomVfx) {
      if (a.causticVenomComplete && this.heldDone(a)) this.finishCombat(att);
    } else if (syncPhantasmalVfx) {
      if (a.phantasmalComplete && this.heldDone(a)) this.finishCombat(att);
    } else if (syncMagicMissileV2Vfx) {
      if (a.magicMissileV2Complete && this.heldDone(a)) this.finishCombat(att);
    } else if (syncBurningHandsVfx) {
      if (a.burningHandsComplete && this.heldDone(a)) this.finishCombat(att);
    } else if (a.t >= spellEnd && this.heldDone(a)) this.finishCombat(att);
  }

  private stepHeal(a: HealAnim, dt: number): void {
    const att = this.units.find((u) => u.id === a.att);
    const target = this.units.find((u) => u.id === a.def);
    if (!att || !target) {
      this.active = null;
      return;
    }
    a.t += dt;
    if (!a.applied && a.t >= 0.2) {
      a.applied = true;
      const heal = this.healingPower(att, rollCure(a.kind, this.affinityUnit(att).mag, this.rng));
      const gained = Math.min(heal, target.maxHp - target.hp);
      target.hp += gained;
      if (gained > 0) { this.gainSupportAffinity(att, target); this.trainHealing(att); }
      if (gained > 0) this.noteAwareEnmity(att, gained * ENMITY.heal.ce, gained * ENMITY.heal.ve);
      this.gainExp(att, target.level, gained);
      this.emitParticle({
        x: target.drawX,
        y: target.drawY - 0.35,
        vx: 0,
        vy: -0.18,
        life: 0,
        max: 2,
        size: 1,
        color: "#d8ead2",
        text: `+${gained}`,
        kind: "text",
        frame: 0,
      });
      this.tip = `${CURES[a.kind].name} · +${gained} HP`;
      this.pushLog(`${att.name} curou ${target.name}: +${gained} HP`);
      this.emitHolyFx(target.x, target.y, a.kind === "cureMinor" ? "minor" : a.kind === "cureLight" ? "hands" : "medium", target.id);
    }
    if (a.t >= 0.5) this.finishCombat(att);
  }

  private stepCureDisease(a: CureDiseaseAnim, dt: number): void {
    const att = this.units.find((u) => u.id === a.att);
    const target = this.units.find((u) => u.id === a.def);
    if (!att || !target) {
      this.active = null;
      return;
    }
    a.t += dt;
    if (!a.applied && a.t >= 0.2) {
      a.applied = true;
      this.curePlayerDisease(target);
      this.gainSupportAffinity(att, target);
      this.trainHealing(att);
      this.noteAwareEnmity(att, ENMITY.support.ce, ENMITY.support.ve);
      this.emitParticle({
        x: target.drawX,
        y: target.drawY - 0.35,
        vx: 0,
        vy: -0.18,
        life: 0,
        max: 2,
        size: 1,
        color: "#d8ead2",
        text: "curado",
        kind: "text",
        frame: 0,
      });
      this.tip = `${CURE_DISEASE.name} · ${target.name} está curado.`;
      this.emitHolyFx(target.x, target.y, "disease", target.id);
    }
    if (a.t >= 0.5) this.finishCombat(att);
  }

  /** A wardog's bite (20%) or a zombie's hit (30%) can inflict disease on a surviving target. */
  private maybeInflictDisease(actor: Unit, target: Unit): void {
    const chance = actor.classId === "wardog" || actor.classId === "wardog2" ? DISEASE.biteChance : (actor.classId === "zombie" || actor.classId === "zombie2" || actor.classId === "undeadOx" || actor.classId === "plagueBearingCattle") ? DISEASE.zombieChance : 0;
    if (chance <= 0 || !target.alive || target.diseased) return;
    if (this.rng() >= chance) return;
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

  /**
   * Grants XP for an action with a measurable, real effect — damage on a hit, HP restored by
   * a heal or potion — and applies any level-ups on the spot, mid-battle. Multi-target
   * abilities (fireball, cleave, piercing...) call this once per unit actually hit, so every
   * landed hit counts on its own. Side-eligibility (don't gain XP for friendly fire) is the
   * caller's job, since the same helper also grants XP for healing your own side.
   */
  private pushLog(line: string): void {
    this.log.push(line);
    if (this.log.length > 200) this.log.shift();
  }

  /** Single choke point for a unit's death: sfx, the log line, and — for an enemy — the
   * kill-drop roll, so every death path (melee, counter, spell, lightning echo, tile
   * hazard) behaves identically instead of four separate copies of the same logic. */
  private markDead(u: Unit): void {
    u.alive = false;
    u.diedAt = this.time;
    // About one death in three plays the alternate death sheet, for sprites that have one.
    u.deathAlt = !!this.art.deaths2[u.sprite] && Math.random() < 1 / 3;
    sfxPlay.death();
    this.pushLog(`${u.name} foi derrotado.`);
    if (u.side === "enemy" && u.guaranteedDrop) {
      // Named unique bosses (Spawn.guaranteedDrop) skip the roll entirely and always drop
      // something — from the same weapon-or-gear pool a chest rolls from, not the plain
      // weapon-only kill-drop pool below.
      const drop = weightedLootPick(this.rng, this.highestEnemyLevel(), this.ownedWeapons);
      if (drop.kind === "weapon") {
        if (this.ownedWeapons.has(drop.id)) {
          this.lootEmber += 15;
        } else {
          this.ownedWeapons.add(drop.id);
          this.lootWeapons.push(drop.id);
          this.pushLog(`Loot: ${WEAPONS[drop.id]?.name ?? drop.id}`);
        }
      } else {
        this.lootEquipment.push(drop.id);
        this.pushLog(`Loot: ${EQUIPMENT[drop.id]?.name ?? drop.id}`);
      }
    } else if (u.side === "enemy" && this.rng() < KILL_DROP_CHANCE) {
      // 1% per kill, capped to what this mission's own enemies are geared for (see
      // highestEnemyLevel), and never a weapon already owned — an early mission never hands
      // out the campaign's best gear.
      const id = weightedWeaponPick(this.rng, Object.keys(WEAPONS), this.highestEnemyLevel());
      if (this.ownedWeapons.has(id)) {
        this.lootEmber += 15;
      } else {
        this.ownedWeapons.add(id);
        this.lootWeapons.push(id);
        this.pushLog(`Loot: ${WEAPONS[id]?.name ?? id}`);
      }
    }
  }

  /** Finishing off a target pays 25% more XP than just wounding it — stacks multiplicatively
   * with whatever skill-specific multiplier (mage/conjurer/Long Shot's own kill bonus, the
   * AoE first-target-only full share, etc.) the caller already worked out, rather than
   * replacing it. */
  private static readonly KILL_EXP_BONUS_MUL = 1.25;

  /** A summoned familiar never persists past this battle to keep XP of its own, so its
   * attacks/spells/counters instead pay its summoning conjurer this fraction of what a real
   * unit would have earned — floored in grantExp below, never rounded up, so a small gain (a
   * familiar's own flat 1 XP counter, say) becomes 0 rather than bouncing back up to 1. */
  private static readonly FAMILIAR_XP_SHARE = 0.1;

  private gainExp(attacker: Unit, targetLevel: number, amount: number, multiplier = 1, isKill = false): void {
    if (amount <= 0 || attacker.side !== "player" || !attacker.alive) return;
    const killMul = isKill ? BattleEngine.KILL_EXP_BONUS_MUL : 1;
    const gained = Math.round(expForHit(attacker.level, targetLevel) * multiplier * killMul);
    this.grantExp(attacker, gained);
  }

  /** A successful counter always earns exactly 1 XP — flat, no level-gap scaling, no kill
   * bonus, no stacking with anything. The counter still deals its full real damage; this
   * only caps what it's worth in experience, so a unit can't out-level by baiting hits and
   * countering instead of attacking. */
  private gainCounterExp(attacker: Unit): void {
    if (attacker.side !== "player" || !attacker.alive) return;
    this.grantExp(attacker, 1);
  }

  /** Routes earned XP to whoever should actually keep it: a summoned familiar (summonerId
   * set) redirects FAMILIAR_XP_SHARE of its own gain to its summoning conjurer instead of
   * keeping any itself; every other unit keeps 100% of its own gain, unchanged from before. */
  private grantExp(attacker: Unit, amount: number): void {
    if (attacker.summonerId) {
      const conjurer = this.units.find((u) => u.id === attacker.summonerId);
      if (!conjurer || conjurer.side !== "player" || !conjurer.alive || conjurer.level >= MAX_LEVEL) return;
      this.addExp(conjurer, Math.floor(amount * BattleEngine.FAMILIAR_XP_SHARE));
      return;
    }
    if (attacker.level >= MAX_LEVEL) return;
    this.addExp(attacker, amount);
  }

  private addExp(attacker: Unit, gained: number): void {
    if (gained <= 0) return;
    attacker.xp += gained;
    while (attacker.xp >= expToLevel(attacker.level) && attacker.level < MAX_LEVEL) {
      attacker.xp -= expToLevel(attacker.level);
      this.levelUpUnit(attacker);
    }
    if (attacker.level >= MAX_LEVEL) attacker.xp = 0;
  }

  /** Bumps a unit by one level: stat growth, the level's HP gain added to current HP (not a
   * full heal), and any newly-unlocked tier uses granted right away. Spent charges stay
   * spent — only the extra slots this level adds land in the remaining pool. */
  private levelUpUnit(u: Unit): void {
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
    this.pushLog(this.tip);
    if (extra) {
      this.emitParticle({
        x: u.drawX,
        y: u.drawY - 0.55,
        vx: 0,
        vy: -0.18,
        life: 0,
        max: 1.8,
        size: 1,
        color: "#e8d48a",
        text: extra,
        kind: "text",
        frame: 0,
      });
    }
    this.emitLevelUpFx(u, to);
    sfxPlay.levelUp();
  }

  private curePlayerDisease(u: Unit): void {
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

  /**
   * Marks a unit as having acted this turn.
   *
   * Movement is a pool of MOV per turn, not a single move that acting cancels: spend two
   * hexes, cast, and the other three are still there to run with. So acting no longer ends
   * the turn just because the unit had already walked — it stays selected with whatever
   * budget is left. The turn ends here only when there is nothing left to do with it (the
   * pool is empty), or for a unit that is dead or on the enemy side, where leaving anything
   * "selected" would expose it to player input.
   *
   * What acting does close off is the refund: undoMove refuses once acted is set, because an
   * action was taken from a position a rewind would erase.
   */
  private finishAction(u: Unit): void {
    this.gainAdjacentAffinity(u);
    this.noteUnitDrawAction(u.id);
    if (u.side === "player" && !u.summoned) u.fullness = drainHunger(u.fullness, ACTION_HUNGER_COST);
    u.acted = true;
    this.pendingFoeId = null;
    this.inspectedId = null;
    this.threat = [];
    this.attackFrom.clear();
    const spent = !u.alive || u.side !== "player" || u.mov - u.moveBudgetUsed <= 0;
    if (spent) {
      u.moved = true;
      this.selectedId = null;
      this.reach.clear();
      this.orig = null;
      this.turnStart = null;
      this.mode = this.phase === "player" ? "idle" : "locked";
      return;
    }
    this.selectedId = u.id;
    this.orig = { x: u.x, y: u.y };
    this.origMoveBudgetUsed = u.moveBudgetUsed;
    this.reach = computeReachable(this.effectiveUnitForReach(u), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
    this.mode = "selected";
  }

  private finishCombat(att: Unit): void {
    att.drawX = att.x;
    att.drawY = att.y;
    this.active = null;
    this.spellKind = null;
    this.missileTargets = [];
    // A spell/heal/cureDisease sets this.banner directly (the cast name, e.g. "Bola de
    // Fogo") when it starts, outside the dedicated "banner" active-step type — which is
    // the only other thing that ever set it, and the only thing that ever cleared it (see
    // stepActive). Every skill routes through this single completion point regardless of
    // which one it was, so clearing it here is the one place that actually covers all of
    // them instead of the banner sitting on screen until something unrelated overwrites it.
    this.banner = null;
    this.evaluateEnd();
    if (this.result) {
      this.gainAdjacentAffinity(att);
      att.acted = true;
      this.selectedId = null;
      this.pendingFoeId = null;
      this.inspectedId = null;
      this.threat = [];
      this.reach.clear();
      this.attackFrom.clear();
      this.orig = null;
      this.mode = "idle";
      return;
    }
    this.finishAction(att);
  }

  private smashBarricades(unit: Unit): void {
    if ((unit.classId !== "troll" && unit.classId !== "troll2") || !unit.alive) return;
    const fill: TerrainId = this.tiles.includes("nave") ? "nave" : "plains";
    const seen = new Set<string>();
    let n = 0;
    for (const p of footprint(unit)) {
      for (const c of [p, ...hexNeighbors(p.x, p.y)]) {
        if (!inBounds(c.x, c.y, this.cols, this.rows)) continue;
        const k = key(c.x, c.y);
        if (seen.has(k)) continue;
        seen.add(k);
        const i = c.y * this.cols + c.x;
        if (this.tiles[i] !== "barricade") continue;
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
        this.emitParticle({
          x: c.x,
          y: c.y,
          vx: 0,
          vy: -0.2,
          life: 0,
          max: 0.45,
          size: 1,
          color: "#c4a07a",
          kind: "impact",
          frame: 0,
        });
      }
    }
    if (n) {
      this.tip = "O troll parte a barricada.";
      this.trauma = Math.min(1, this.trauma + 0.35);
      sfxPlay.hit();
    }
  }

  private nudgeOffHazard(unit: Unit): void {
    const here = this.hexAt(unit.x, unit.y);
    if (here.passable || canTraverseWater(unit, here, this.decorOverlay, unit.x, unit.y, this.cols)) return;
    const occ = this.occ();
    const seen = new Set<string>([key(unit.x, unit.y)]);
    const q: Point[] = [{ x: unit.x, y: unit.y }];
    while (q.length) {
      const cur = q.shift()!;
      for (const n of hexNeighbors(cur.x, cur.y)) {
        if (n.x < 0 || n.y < 0 || n.x >= this.cols || n.y >= this.rows) continue;
        const k = key(n.x, n.y);
        if (seen.has(k)) continue;
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
  private nudgeOffWaypoint(unit: Unit): void {
    const waypointCells = new Set<string>();
    for (const d of this.decorations) {
      if (!DECORATIONS[d.id]?.exitKind) continue;
      for (const f of placedFootprint(d)) waypointCells.add(key(d.x + f.dx, d.y + f.dy));
    }
    if (!waypointCells.has(key(unit.x, unit.y))) return;
    const occ = this.occ();
    const seen = new Set<string>([key(unit.x, unit.y)]);
    const q: Point[] = [{ x: unit.x, y: unit.y }];
    while (q.length) {
      const cur = q.shift()!;
      for (const n of hexNeighbors(cur.x, cur.y)) {
        if (n.x < 0 || n.y < 0 || n.x >= this.cols || n.y >= this.rows) continue;
        const k = key(n.x, n.y);
        if (seen.has(k)) continue;
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

  /** Bleeding hurts on every action; walking only opens the wound once per own turn. */
  private applyBleedingActionDamage(u: Unit | undefined, isMove: boolean): boolean {
    if (!u || !u.alive || !u.bleeding || (isMove && u.bleedMovedThisTurn)) return !!u?.alive;
    if (isMove) u.bleedMovedThisTurn = true;
    const dmg = rollDice(1, 8, 0, this.rng);
    u.hp = Math.max(0, u.hp - dmg);
    u.flash = 1;
    u.hitAt = this.time;
    this.spawnHit(u, dmg, false);
    this.tip = `Sangramento · 1D8 dano`;
    this.pushLog(`Sangramento fere ${u.name}: ${dmg} dano`);
    sfxPlay.hit();
    if (u.hp <= 0) {
      this.markDead(u);
      return false;
    }
    return true;
  }

  /** Using an item (potion, lockpick) is an action: a bleeding user takes the 1D8 too. */
  private bleedOnItemUse(u: Unit): void {
    if (!this.applyBleedingActionDamage(u, false)) this.evaluateEnd();
  }

  /** Lightning echo + standing-hazard damage, applied once when this unit's own turn begins. */
  private startOfTurnEffects(u: Unit): void {
    if (!u.alive) return;
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
      this.pushLog(`Eco de relâmpago em ${u.name}: ${dmg} dano`);
      this.trainResistance(u, "lightning");
      sfxPlay.hit();
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
      this.pushLog(`Veneno consome ${u.name}: ${dmg} dano`);
      sfxPlay.hit();
      // Suffering the poison is use too — every tick is a Poison Resistance check.
      if (u.side === "player" && !u.summoned) this.trainResistance(u, "poison");
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
        if (!u.alive || !isStandingInZone) continue;
        const base = Math.floor(Math.floor(zone.casterMag / 2) * zone.damageMul) + rollDice(zone.damageDice, zone.damageFaces, 0, this.rng);
        const zoneCaster = this.units.find(candidate => candidate.id === zone.casterId);
        const dmg = Math.floor(elementalDamage(staffElementalDamage(base, zoneCaster ? this.staffMagic(zoneCaster) : undefined, "ice"), u.resistances?.ice ?? 0, zone.casterMag));
        const actualStaffDamage = Math.min(u.hp, dmg);
        u.hp = Math.max(0, u.hp - dmg);
        if (zoneCaster && zoneCaster.side !== u.side) this.staffDrain(zoneCaster, actualStaffDamage);
        u.flash = 1;
        u.hitAt = this.time;
        if (dmg > 0) this.spawnHit(u, dmg, false);
        this.tip = `${ICE_STORM.name} · ${diceFormula(zone.damageDice, zone.damageFaces, 0)}`;
        this.pushLog(`${ICE_STORM.name} atinge ${u.name}: ${dmg} dano`);
        this.queueElementalFx("ice", [{ x: u.x, y: u.y }], 0.8);
        this.trainResistance(u, "ice", zone.casterLevel);
        const caster = this.units.find((candidate) => candidate.id === zone.casterId);
        if (caster && caster.side === "player") this.trainElementUse(caster, "ice", u.level);
        if (u.hp <= 0) this.markDead(u);
        else sfxPlay.hit();
      }
    }
    // Second Wind (Paladin tier 3): passive, never a hotbar cast — the first time this
    // paladin's own turn opens at or below the "badly wounded" line with a tier-3 use still
    // banked, it heals itself and spends the use. classId-gated explicitly, since tierUses
    // hands out tier-3 slots to every class, not just paladin.
    if (u.alive) this.restoreStaffHp(u, Math.min(this.staffMagic(u)?.regeneration ?? 0, u.maxHp - u.hp));
    if (u.alive && u.classId === "paladin" && u.hp / u.maxHp <= SECOND_WIND.badlyWoundedPct && this.tierRemaining(u, "secondWind") > 0) {
      this.spendTier(u, "secondWind");
      const heal = Math.min(u.maxHp - u.hp, Math.floor(secondWindPct(u.level) * u.dex));
      if (heal > 0) {
        u.hp += heal;
        u.flash = 1;
        this.emitParticle({
          x: u.drawX,
          y: u.drawY - 0.35,
          vx: 0,
          vy: -0.18,
          life: 0,
          max: 2,
          size: 1,
          color: "#d8ead2",
          text: `+${heal}`,
          kind: "text",
          frame: 0,
        });
        this.tip = `${SECOND_WIND.name} · +${heal} HP`;
        this.pushLog(`${u.name} usa ${SECOND_WIND.name}: +${heal} HP`);
        sfxPlay.heal();
      }
    }
    if (u.alive) this.applyTileHazard(u, { x: u.x, y: u.y });
    this.evaluateEnd();
  }

  private trainingGainForLevel(unitLevel: number, enemyLevel: number): number | null | undefined {
    const difference = unitLevel - enemyLevel;
    if (difference === 10) return null;
    if (difference === 5) return 0.05;
    return undefined;
  }

  private trainWeapon(unit: Unit, type: import("./types").WeaponType | undefined, enemyLevel: number): void {
    if (!type || unit.side !== "player" || unit.summoned || !weaponTypesForClass(unit.classId).includes(type)) return;
    const amount = this.trainingGainForLevel(unit.level, enemyLevel);
    if (amount === null) return;
    const id = `${type}Weapon` as const;
    const current = this.heroSkills[unit.name]?.[id] ?? 0;
    const gained = rollWeaponSkillGain(current, this.rng, amount ?? SKILL_GAIN);
    if (gained === null) return;
    this.heroSkills[unit.name] = { ...this.heroSkills[unit.name], [id]: gained };
    unit.weaponSkills = { ...unit.weaponSkills, [type]: gained };
    this.logSkillGain(unit, id, current, gained);
  }

  /** Every skill point a hero earns in battle is announced in the combat log. */
  private logSkillGain(unit: Unit, id: SkillId, before: number, after: number): void {
    const number = (n: number) => n.toLocaleString("pt-BR", { maximumFractionDigits: 2 });
    this.pushLog(`${unit.name} melhorou ${SKILLS[id].name}: ${number(after)} (+${number(after - before)})`);
  }

  /** A familiar's elemental magic practises its summoner's matching resistance. */
  private trainElementUse(caster: Unit, element: import("./types").ResistanceElement, enemyLevel: number): void {
    const learner = caster.summoned
      ? this.units.find(unit => unit.id === caster.summonerId && unit.alive && unit.side === caster.side)
      : caster;
    if (learner) this.trainResistance(learner, element, enemyLevel);
  }

  private healingPower(actor: Unit, base: number): number {
    return healingAmount(base, skillValue(this.heroSkills, actor.name, "healing"));
  }

  private staffMagic(unit: Unit): MageStaffMagic | undefined {
    const weapon = unit.weaponId ? WEAPONS[unit.weaponId] : undefined;
    return weapon?.usableBy.includes(unit.classId) ? weapon.magic : undefined;
  }

  private restoreStaffHp(unit: Unit, amount: number): void {
    if (!unit.alive || amount <= 0) return;
    const heal = Math.min(unit.maxHp - unit.hp, Math.floor(amount));
    if (heal <= 0) return;
    unit.hp += heal;
    unit.healGlow = 1; unit.healGlowKind = "holyMinor";
    this.emitParticle({ x: unit.drawX, y: unit.drawY - 0.35, vx: 0, vy: -0.18, life: 0, max: 1.4, size: 1, color: "#c5dfa6", text: `+${heal}`, kind: "text", frame: 0 });
    this.pushLog(`${unit.name}: ${WEAPONS[unit.weaponId!]!.name} recuperou ${heal} HP.`);
  }

  private staffDrain(unit: Unit, actualDamage: number): void {
    this.restoreStaffHp(unit, staffLifeSteal(actualDamage, unit.maxHp - unit.hp, this.staffMagic(unit)));
  }

  private trainHealing(actor: Unit): void {
    if (actor.side !== "player" || actor.summoned) return;
    const current = skillValue(this.heroSkills, actor.name, "healing");
    const next = rollSkillGain(current, this.rng);
    if (next === null) return;
    this.heroSkills[actor.name] = { ...this.heroSkills[actor.name], healing: next };
    actor.healingSkill = next;
    this.logSkillGain(actor, "healing", current, next);
  }

  private trainResistance(unit: Unit, element: import("./types").ResistanceElement, enemyLevel?: number): void {
    if (unit.side !== "player" || unit.summoned) return;
    const amount = enemyLevel == null ? undefined : this.trainingGainForLevel(unit.level, enemyLevel);
    if (amount === null) return;
    const id = `${element}Resistance` as const;
    const current = this.heroSkills[unit.name]?.[id] ?? 0;
    const gained = rollSkillGain(current, this.rng, amount ?? SKILL_GAIN);
    if (gained === null) return;
    this.heroSkills[unit.name] = { ...this.heroSkills[unit.name], [id]: gained };
    unit.resistances = { ...unit.resistances, [element]: Number(((unit.resistances?.[element] ?? 0) + gained - current).toFixed(2)) };
    if (element === "poison") unit.poisonResist = gained;
    this.logSkillGain(unit, id, current, gained);
  }

  private applyTileHazard(unit: Unit, cell: Point): void {
    const terr = this.hexAt(cell.x, cell.y);
    if (!terr.hazardDice || !unit.alive) return;
    const faces = terr.hazardFaces ?? 8;
    let dmg = 0;
    for (let i = 0; i < terr.hazardDice; i++) dmg += 1 + Math.floor(this.rng() * faces);
    const element = terr.id === "flame" ? "fire" : terr.id === "ember" ? "ember" : undefined;
    if (element) dmg = Math.floor(elementalDamage(dmg, unit.resistances?.[element] ?? 0));
    unit.hp = Math.max(0, unit.hp - dmg);
    unit.flash = 1;
    unit.hitAt = this.time;
    this.spawnHit(unit, dmg, false);
    this.pushLog(`${terr.name} feriu ${unit.name}: ${dmg} dano`);
    if (element) this.trainResistance(unit, element);
    sfxPlay.hit();
    if (unit.hp <= 0) {
      this.markDead(unit);
      this.onNextIdle = null;
    }
  }

  private emitParticle(init: Omit<Particle, "live">): void {
    if (this.reducedMotion && init.kind === "spark") return;
    let slot: Particle | undefined;
    for (const p of this.particles) {
      if (!p.live) {
        slot = p;
        break;
      }
    }
    if (!slot) {
      slot = this.particles.find((p) => p.kind !== "text") ?? this.particles[0]!;
      let oldest = 0;
      for (const p of this.particles) {
        if (p.kind === "text") continue;
        if (p.life / p.max > oldest) {
          oldest = p.life / p.max;
          slot = p;
        }
      }
    } else this.particleLive += 1;
    slot.live = true;
    slot.x = init.x;
    slot.y = init.y;
    slot.vx = init.vx;
    slot.vy = init.vy;
    slot.life = init.life;
    slot.max = init.max;
    slot.size = init.size;
    slot.color = init.color;
    slot.text = init.text;
    slot.kind = init.kind;
    slot.frame = init.frame;
  }

  /** Golden burst played once when a unit levels up: an expanding ring, a scatter of small
   * stars, and a big glowing "Nível X!" label over the head, all anchored to the unit's hex
   * and drifting in real pixel space (see LevelUpSpark) rather than the grid-snapped Particle
   * system above. Also arms the unit's own sustained levelGlow (see tick/render) so the
   * character itself, not just the burst around it, reads as glowing for a couple seconds. */
  private emitLevelUpFx(u: Unit, level: number): void {
    u.levelGlow = 1;
    if (this.reducedMotion) return;
    const cell = this.layout.tile;
    const claim = (): LevelUpSpark | undefined => {
      let slot = this.levelUpFx.find((s) => !s.live);
      if (slot) {
        this.levelUpFxLive += 1;
        return slot;
      }
      slot = this.levelUpFx[0];
      let oldest = 0;
      for (const s of this.levelUpFx) {
        if (s.life / s.max > oldest) {
          oldest = s.life / s.max;
          slot = s;
        }
      }
      return slot;
    };
    const spawn = (init: Omit<LevelUpSpark, "live">) => {
      const slot = claim();
      if (!slot) return;
      Object.assign(slot, init, { live: true });
    };
    for (const [max, size] of [
      [0.7, 0],
      [0.95, 0],
    ] as const) {
      spawn({
        unitId: u.id,
        kind: "ring",
        dx: 0,
        dy: -cell * 0.55,
        vx: 0,
        vy: 0,
        life: 0,
        max,
        size,
        hue: 46,
        rot: 0,
        vrot: 0,
        refCell: cell,
      });
    }
    const n = 18;
    for (let i = 0; i < n; i++) {
      const angle = (Math.PI * 2 * i) / n + (this.rng() - 0.5) * 0.4;
      const speed = cell * (0.9 + this.rng() * 1.1);
      spawn({
        unitId: u.id,
        kind: "star",
        dx: 0,
        dy: -cell * 0.55,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed * 0.7 - cell * 0.6,
        life: 0,
        max: 0.85 + this.rng() * 0.5,
        size: cell * (0.05 + this.rng() * 0.05),
        hue: 42 + this.rng() * 20,
        rot: this.rng() * Math.PI,
        vrot: (this.rng() - 0.5) * 6,
        refCell: cell,
      });
    }
    spawn({
      unitId: u.id,
      kind: "label",
      text: `Nível ${level}`,
      dx: 0,
      dy: 0,
      vx: 0,
      vy: -this.layout.tile * 0.05,
      life: 0,
      max: 2.2,
      size: this.layout.tile * Math.sqrt(3) * 0.5,
      hue: 46,
      rot: 0,
      vrot: 0,
      refCell: cell,
    });
  }

  /** Potionzero — the original warm-white halo + rising motes. Kept as its own FX so a
   * future skill can fire it without sharing the new holy/potion bursts. Not used by
   * current heals or potions. */
  emitPotionZeroFx(u: Unit): void {
    u.healGlow = 1;
    u.healGlowKind = "potionZero";
    if (this.reducedMotion) return;
    const n = 6;
    for (let i = 0; i < n; i++) {
      const ang = -Math.PI / 2 + (this.rng() - 0.5) * 1.6;
      const speed = 0.5 + this.rng() * 0.6;
      this.emitParticle({
        x: u.drawX + (this.rng() - 0.5) * 0.5,
        y: u.drawY - 0.1,
        vx: Math.cos(ang) * speed * 0.3,
        vy: Math.sin(ang) * speed - 0.3,
        life: 0,
        max: 0.7 + this.rng() * 0.3,
        size: 2 + this.rng() * 2,
        color: "#fff6df",
        kind: "spark",
        frame: 0,
      });
    }
  }

  /** Divine light on a hex (and optional unit). minor = Cura Menor, medium = Cura Média /
   * hands = Healing Hands, disease = Curar Doença (teal), potion = the drink FX. */
  emitHolyFx(x: number, y: number, kind: HolyKind, unitId = ""): void {
    const u = unitId ? this.units.find((n) => n.id === unitId) : this.units.find((n) => n.alive && n.x === x && n.y === y);
    if (u) {
      u.healGlow = kind === "minor" ? 0.72 : kind === "hands" ? 0.86 : kind === "potion" ? 0.88 : 1;
      u.healGlowKind = kind === "minor" ? "holyMinor" : kind === "hands" ? "healingHands" : kind === "medium" ? "holyMedium" : kind === "food" ? "food" : kind === "disease" ? "disease" : "potion";
    }
    if (this.reducedMotion) return;
    let slot = this.holyFx.find((h) => !h.live);
    if (!slot) {
      slot = this.holyFx[0]!;
      let oldest = 0;
      for (const h of this.holyFx) {
        if (h.t / h.max > oldest) {
          oldest = h.t / h.max;
          slot = h;
        }
      }
    } else this.holyFxLive += 1;
    const rayCount = kind === "medium" || kind === "food" ? 10 : kind === "hands" || kind === "disease" ? 8 : kind === "potion" ? 5 : 6;
    slot.live = true;
    slot.unitId = u?.id ?? unitId;
    slot.x = x;
    slot.y = y;
    slot.t = 0;
    slot.max = holyDuration(kind);
    slot.kind = kind;
    slot.seed = this.rng() * Math.PI * 2;
    slot.rays = Array.from({ length: rayCount }, (_, i) => (Math.PI * 2 * i) / rayCount + (this.rng() - 0.5) * 0.18);
  }

  /** Queues one WebGL elemental FX spawn per target tile, drained by BattleCanvas's render
   * loop (see elementalFxRequests). Respects reducedMotion the same way every other spell-hit
   * FX emitter here does. */
  private queueElementalFx(kind: ElementKind, tiles: Point[], duration: number): void {
    if (this.reducedMotion) return;
    for (const t of tiles) this.elementalFxRequests.push({ kind, x: t.x, y: t.y, duration });
  }

  /** One burning patch per Fireball area cell, all procedural so it conforms to every map. */
  private emitFireballBurstFx(tiles: Point[], kind: "fireball" | "causticVenom"): void {
    if (this.reducedMotion) return;
    for (const cell of tiles) {
      let burst = this.fireballBurstFx.find((x) => !x.live);
      if (!burst) burst = this.fireballBurstFx[0]!;
      else this.fireballBurstFxLive += 1;
      burst.live = true;
      burst.x = cell.x;
      burst.y = cell.y;
      burst.t = 0;
      burst.max = kind === "causticVenom" ? 0.92 : 0.58;
      burst.seed = this.rng() * Math.PI * 2;
      // This assignment is essential: pooled slots default to fireball, which previously
      // made every Caustic Venom impact enter the flame-rendering branch.
      burst.kind = kind;
    }
  }
  /** Select the supplied cue for the actual weapon or Kael's Blade Skill. */
  private playMeleeCue(unit: Unit, offHand = false, skill?: string | null, bladeStartAt = 0): void {
    if (unit.name === "Kael" && skill && ["cleave", "sweep", "shoulderSmash", "stampede", "piercingThrust", "trip", "doubleStrike", "bullRush", "executionerStrike"].includes(skill)) {
      sfxPlay.kaelBladeSkill();
      return;
    }
    const weaponId = offHand ? unit.offHandId : unit.weaponId ?? starterWeaponFor(unit.classId);
    if (weaponId && EQUIPMENT[weaponId]?.weaponType === "dagger") sfxPlay.daggerAttack();
    else sfxPlay.meleeAttack(this.isBladeAttack(unit, offHand), bladeStartAt);
  }

  /** Resolve the striking hand; shared class pools also contain blunt weapons. */
  private isBladeAttack(unit: Unit, offHand = false): boolean {
    if (offHand) return !!unit.offHandId && EQUIPMENT[unit.offHandId]?.kind === "weapon";
    const weaponId = unit.weaponId ?? starterWeaponFor(unit.classId);
    if (!weaponId) return false;
    // By weapon type first: swords named without an "espada" prefix (montante-da-ruina,
    // zweihander-profana, cimitarra-do-dragao) played Kael's swing with the blunt cue.
    const type = EQUIPMENT[weaponId]?.weaponType;
    if (type) return type === "sword" || type === "axe" || type === "dagger";
    return /^(espada|machado|lamina|adaga|punhal|katar)/.test(weaponId);
  }

  /** True only for bow/crossbow users. Reach weapons strike physically instead of firing arrows. */
  private isArrowAttack(unit: Unit): boolean {
    // Reach weapons are always physical, even if an imported loadout is incorrectly flagged ranged.
    if (unit.classId === "pikeman" || unit.classId === "basePikeman" || unit.classId === "lancer" || unit.classId === "aldric" || unit.classId === "sandoval" || unit.classId === "sentinel" || unit.classId === "templar") return false;
    // Besteiros are enemy crossbow users; an imported/legacy weapon id must never turn their
    // ranged attack into a silent or melee action.
    if (unit.classId === "brigand" || unit.classId === "baseCrossbowman" || unit.classId === "brigandCrossbowman") return true;
    if (unit.weaponId) return !!WEAPONS[unit.weaponId]?.ranged;
    // Default campaign loadouts: Neera starts as a bow user before gear is assigned.
    return unit.classId === "archer" || unit.classId === "ranger" || unit.classId === "assassin";
  }

  /** Only spellcasting classes use the distinct basic-attack arcane bolt. */
  private isArcaneCaster(unit: Unit): boolean {
    return unit.classId === "mage" || unit.classId === "voss" || unit.classId === "elementalist" || unit.classId === "warlock" || unit.classId === "cultist" || unit.classId === "cultistV2" || unit.classId === "birolho" || unit.classId === "birolho2" || unit.classId === "birolho3" || unit.classId === "birolhoLegs" || unit.classId === "birolhoLegs2";
  }

  /** One glowing bolt per target, hex-to-hex — see MissileFx. */
  private emitMissileFx(fromX: number, fromY: number, toX: number, toY: number, kind: "magicMissile" | "phantasmalForce" | "fantomForce" | "fireball" | "causticVenom" | "minorVenom" | "longShot" | "arcaneBolt" | "webOfDreams"): void {
    // The arrow-release cue lands the instant the arrow leaves the bow (the draw cue played when the pull began).
    if (kind === "longShot") sfxPlay.arrowRelease(this.units.find((u) => u.x === Math.round(fromX) && u.y === Math.round(fromY) && u.alive)?.sprite === "neera");
    if (this.reducedMotion) return;
    let slot = this.missileFx.find((m) => !m.live);
    if (!slot) {
      slot = this.missileFx[0]!;
      let oldest = 0;
      for (const m of this.missileFx) {
        if (m.t / m.max > oldest) {
          oldest = m.t / m.max;
          slot = m;
        }
      }
    } else this.missileFxLive += 1;
    slot.live = true;
    slot.fromX = fromX;
    slot.fromY = fromY;
    slot.toX = toX;
    slot.toY = toY;
    slot.t = 0;
    slot.travel = kind === "longShot" ? ARROW_TRAVEL : kind === "webOfDreams" ? WEB_SHOT_TRAVEL : kind === "fantomForce" ? FANTOM_FORCE_TRAVEL : kind === "phantasmalForce" ? PHANTASMAL_FORCE_TRAVEL : kind === "magicMissile" || kind === "fireball" || kind === "causticVenom" || kind === "minorVenom" ? SPELL_TRAVEL : MISSILE_TRAVEL;
    // The legacy Magic Missile ends on impact; its trail must not linger past the hit.
    slot.max = slot.travel + (kind === "magicMissile" ? 0 : MISSILE_AFTERGLOW);
    // Phantom System uses the original purple 2D bolt; only Phantasmal Force is a blue apparition.
    slot.hue = kind === "fireball" ? 22 : kind === "causticVenom" || kind === "minorVenom" ? 104 : kind === "longShot" ? 205 : kind === "arcaneBolt" ? 2 : kind === "webOfDreams" ? 276 : kind === "phantasmalForce" ? 202 : 268;
    slot.neeraArrow = kind === "longShot" && this.units.some((u) => u.alive && u.sprite === "neera" && u.x === Math.round(fromX) && u.y === Math.round(fromY));
    slot.kind = kind;
    slot.seed = this.rng() * Math.PI * 2;
  }

  /** A bolt struck down onto one hex — see LightningFx. The jagged shape (main bolt plus
   * forks) is rolled once here so it stays put for the strike's whole short life.
   * `power: "shock"` is Choque; `"raio"` is Relâmpago; `"t3"` is Lighting Tier 3. */
  private emitLightningFx(x: number, y: number, power: "shock" | "raio" | "t3" | "divine" | "divineSplash" = "shock"): void {
    if (this.reducedMotion) return;
    const emitOne = (spread: number, segs: number, branchMin: number, branchExtra: number, hue: number) => {
      let slot = this.lightningFx.find((l) => !l.live);
      if (!slot) {
        slot = this.lightningFx[0]!;
        let oldest = 0;
        for (const l of this.lightningFx) {
          if (l.t / l.max > oldest) {
            oldest = l.t / l.max;
            slot = l;
          }
        }
      } else this.lightningFxLive += 1;
      const rollSegs = (n: number, s: number) => Array.from({ length: n }, () => (this.rng() - 0.5) * s);
      slot.live = true;
      slot.x = x;
      slot.y = y;
      slot.t = 0;
      slot.max = power === "t3" ? LIGHTNING_T3_DUR : power === "raio" || power === "divine" ? LIGHTNING_RAIO_DUR : LIGHTNING_STRIKE_DUR;
      slot.hue = hue;
      slot.segs = rollSegs(segs, spread);
      slot.power = power;
      const branchCount = branchMin + Math.floor(this.rng() * (branchExtra + 1));
      const branchSegs = power === "t3" ? 7 : power === "raio" || power === "divine" ? 6 : 4;
      const branchSpread = power === "t3" ? 0.62 : power === "raio" || power === "divine" ? 0.55 : 0.4;
      slot.branches = Array.from({ length: branchCount }, () => ({
        at: 0.18 + this.rng() * 0.58,
        side: this.rng() < 0.5 ? -1 : 1,
        segs: rollSegs(branchSegs, branchSpread),
      }));
    };
    if (power === "divine") {
      emitOne(0.28, 11, 3, 2, 42 + this.rng() * 12);
      emitOne(0.18, 8, 2, 1, 48 + this.rng() * 8);
    } else if (power === "divineSplash") {
      emitOne(0.14, 6, 1, 1, 42 + this.rng() * 12);
    } else if (power === "t3") {
      emitOne(0.2, 9, 2, 1, 206 + this.rng() * 10);
      emitOne(0.12, 7, 1, 1, 198 + this.rng() * 8);
    } else if (power === "raio") {
      emitOne(0.42, 14, 4, 2, 210 + this.rng() * 18);
      emitOne(0.28, 11, 2, 2, 198 + this.rng() * 14);
    } else {
      emitOne(0.34, 9, 2, 1, 200 + this.rng() * 20);
    }
  }

  /** Summon Familiar's conjuring circle — see PortalFx/drawPortalFx. */
  private emitPortalFx(x: number, y: number, body: { dx: number; dy: number }[] | null = null, red = false): void {
    if (this.reducedMotion) return;
    let slot = this.portalFx.find((p) => !p.live);
    if (!slot) {
      slot = this.portalFx[0]!;
      let oldest = 0;
      for (const p of this.portalFx) {
        if (p.t / p.max > oldest) {
          oldest = p.t / p.max;
          slot = p;
        }
      }
    } else this.portalFxLive += 1;
    slot.live = true;
    slot.x = x;
    slot.y = y;
    slot.t = 0;
    // A multi-hex body gets a bigger circle (sized in drawPortalFx) that stays open longer.
    slot.max = body && body.length > 1 ? 1.3 : 0.85;
    slot.seed = this.rng() * Math.PI * 2;
    slot.body = body && body.length > 1 ? body : null;
    slot.red = red;
    slot.warp = false;
    slot.persistent = false;
  }

  private emitWarpFx(x: number, y: number, radius: number, level: number): void {
    if (this.reducedMotion) return;
    let slot = this.portalFx.find((p) => !p.live);
    if (!slot) slot = this.portalFx[0]!;
    else this.portalFxLive += 1;
    slot.live = true;
    slot.x = x;
    slot.y = y;
    slot.t = 0;
    slot.max = 2.4;
    slot.seed = this.rng() * Math.PI * 2;
    slot.body = null;
    slot.red = false;
    slot.warp = true;
    slot.radius = radius;
    slot.persistent = true;
  }

  /** One steel-swoosh effect — see BladeFx/BladeKind. Shared by every warrior/lancer/knight
   * physical skill; `opts` fills in only whatever that shape needs (arc's a0/a1, dash's
   * toX/toY, Shoulder Smash's warm tint). */
  private emitBladeFx(
    kind: BladeKind,
    x: number,
    y: number,
    opts: { a0?: number; a1?: number; toX?: number; toY?: number; warm?: boolean; mirrorX?: boolean; dur?: number } = {},
  ): void {
    if (this.reducedMotion) return;
    let slot = this.bladeFx.find((b) => !b.live);
    if (!slot) {
      slot = this.bladeFx[0]!;
      let oldest = 0;
      for (const b of this.bladeFx) {
        if (b.t / b.max > oldest) {
          oldest = b.t / b.max;
          slot = b;
        }
      }
    } else this.bladeFxLive += 1;
    slot.live = true;
    slot.kind = kind;
    slot.x = x;
    slot.y = y;
    slot.toX = opts.toX ?? x;
    slot.toY = opts.toY ?? y;
    slot.a0 = opts.a0 ?? 0;
    slot.a1 = opts.a1 ?? opts.a0 ?? 0;
    slot.warm = opts.warm ?? false;
    slot.mirrorX = opts.mirrorX ?? false;
    slot.t = 0;
    slot.max = opts.dur ?? (kind === "rushTrail" ? 0.5 : kind === "rushImpact" ? 0.55 : kind === "execution" ? 0.7 : kind === "tripSweep" ? 0.65 : kind === "ring" || kind === "shockRing" ? 0.46 : kind === "dash" ? 0.36 : 0.4);
    slot.seed = this.rng() * Math.PI * 2;
  }

  /** The unwrapped angle range (a0..a1, a1 >= a0) from `origin` through each hex in
   * `tiles` in order — used to point Cleave/Shoulder Smash's blade arc at exactly the fan of
   * hexes cleaveHexes picked, whichever of the 6 ring directions that turned out to be. */
  private arcSweepAngles(origin: Point, tiles: Point[]): { a0: number; a1: number } {
    const o = this.hexCenter(origin.x, origin.y);
    const angleTo = (t: Point) => {
      const c = this.hexCenter(t.x, t.y);
      return Math.atan2(c.cy - o.cy, c.cx - o.cx);
    };
    const unwrap = (base: number, ang: number) => {
      let d = ang - base;
      while (d > Math.PI) d -= Math.PI * 2;
      while (d < -Math.PI) d += Math.PI * 2;
      return base + d;
    };
    const a0 = angleTo(tiles[0]!);
    let last = a0;
    for (let i = 1; i < tiles.length; i++) last = unwrap(last, angleTo(tiles[i]!));
    return last >= a0 ? { a0, a1: last } : { a0: last, a1: a0 };
  }

  /** A whiffed attack: just the floating "Missed" text, no impact flash or hit particles. */
  private spawnMiss(target: Unit): void {
    this.emitParticle({
      x: target.drawX,
      y: target.drawY - 0.35,
      vx: 0,
      vy: -0.18,
      life: 0,
      max: 2,
      size: 1,
      color: "#c9c4bb",
      text: "Missed",
      kind: "text",
      frame: 0,
    });
  }

  private spawnHit(target: Unit, dmg: number, crit: boolean, physicalImpact = false): void {
    const cx = target.drawX;
    const cy = target.drawY;
    this.emitParticle({
      x: cx,
      y: cy - 0.35,
      vx: 0,
      vy: -0.18,
      life: 0,
      max: 2,
      size: 1,
      color: crit ? "#f0ebe3" : "#f2d2c6",
      text: crit ? `CRÍTICO  −${dmg}` : `−${dmg}`,
      kind: "text",
      frame: 0,
    });
    this.emitParticle({
      x: cx,
      y: cy - 0.15,
      vx: 0,
      vy: 0,
      life: 0,
      max: 0.32,
      size: 1,
      color: "#fff",
      kind: "impact",
      frame: 0,
    });
    if (this.reducedMotion) return;
    const n = 3;
    for (let i = 0; i < n; i++) {
      const ang = (Math.PI * 2 * i) / n + this.rng();
      this.emitParticle({
        x: cx,
        y: cy,
        vx: Math.cos(ang) * (1.4 + this.rng()),
        vy: Math.sin(ang) * (1.4 + this.rng()) - 0.4,
        life: 0,
        max: 0.28 + this.rng() * 0.12,
        size: 2 + this.rng() * 2,
        color: i % 2 ? "#b54a32" : "#f0ebe3",
        kind: "spark",
        frame: 0,
      });
    }
  }

  /** A player strike on a wild neutral wakes the whole species: every living neutral of the
   * same class turns "enemy" at once. They are not in this round's turn order, so they rouse
   * and start acting from the next round. Nothing turns a woken beast back. */
  private provoke(target: Unit, attacker: Unit): void {
    if (target.side !== "neutral" || attacker.side !== "player") return;
    const pack = this.units.filter((u) => u.alive && u.side === "neutral" && u.classId === target.classId);
    for (const u of pack) u.side = "enemy";
    this.pushLog(
      pack.length > 1
        ? `${target.name} reage — e todo o bando de ${CLASSES[target.classId].name.toLowerCase()} vem junto (${pack.length}).`
        : `${target.name} se volta contra vocês.`,
    );
    this.evaluateEnd();
  }

  /** Inn-quest pickups: a living player unit standing on one picks it up. Checked from
   * evaluateEnd, which already runs after every walk finishes. */
  private collectQuestPickups(): void {
    if (this.questPickups.length === 0) return;
    for (const pickup of [...this.questPickups]) {
      const finder = this.units.find((u) => u.side === "player" && u.alive && u.x === pickup.x && u.y === pickup.y);
      if (!finder) continue;
      this.questPickups = this.questPickups.filter((p) => p.key !== pickup.key);
      this.questFound.push(pickup.key);
      this.tip = `${finder.name} encontrou: ${pickup.name}.`;
      this.pushLog(this.tip);
      sfxPlay.chest();
    }
  }

  private evaluateEnd(): void {
    if (this.result) return;
    this.collectQuestPickups();
    const exitHit = this.exitDecorationHere();
    // Free-roam maps have no normal win/lose condition, but authored waypoints stay usable.
    if (this.mission.explore) {
      this.winAvailable = !!exitHit;
      this.activeExit = exitHit;
      return;
    }
    const p = this.units.some((u) => u.side === "player" && u.alive && !u.summoned);
    const bossAlive = this.units.some((u) => u.side === "enemy" && u.alive && isBossClass(u.classId));
    const anyEnemy = this.units.some((u) => u.side === "enemy" && u.alive);
    // A placed waypoint is usable on every mission. Escape markers request a 60% escape
    // attempt when confirmed; dungeon exits and floor connectors end the mission directly.
    // Without this, the markers on ordinary rout maps were silently ignored.
    const won = !!exitHit || (this.mission.win === "boss" ? !bossAlive : this.mission.win === "escape" ? false : !anyEnemy);
    // Victory doesn't end the battle by itself anymore — it just makes ending it an option
    // (see winAvailable/confirmFinish) so the player can keep taking normal turns to loot
    // remaining chests, with a click-whenever-ready control staying available the whole
    // time rather than a one-shot prompt they could dismiss and then have no way back to.
    // If a trap/trigger spawns a fresh enemy after the field first looked clear, this goes
    // back to false on its own until they're dealt with too. Defeat has no such choice:
    // with no player units left there's nothing left to do.
    this.winAvailable = won;
    this.activeExit = exitHit;
    if (!p) this.result = "defeat";
  }

  /** The waypoint a living player unit is currently standing on, if any. */
  private exitDecorationHere(): DecorationPlacement | null {
    for (const u of this.units) {
      if (u.side !== "player" || !u.alive) continue;
      const hit = this.decorations.find((d) => DECORATIONS[d.id]?.exitKind && placedFootprint(d).some((f) => d.x + f.dx === u.x && d.y + f.dy === u.y));
      if (hit) return hit;
    }
    return null;
  }

  private exitUnitHere(exit: DecorationPlacement): Unit | null {
    const cells = placedFootprint(exit);
    return this.units.find((unit) =>
      unit.side === "player" && unit.alive && cells.some((cell) => exit.x + cell.dx === unit.x && exit.y + cell.dy === unit.y),
    ) ?? null;
  }

  /** Confirms the currently offered mission exit. Escape waypoints keep their explicit 60%
   * chance; on failure the active hero loses their turn and the encounter continues. */
  canConfirmFinish(): boolean {
    if (!this.winAvailable || this.result) return false;
    if (this.activeExit && DECORATIONS[this.activeExit.id]?.exitKind === "escape") {
      return !!this.exitUnitHere(this.activeExit);
    }
    return true;
  }

  confirmFinish(): void {
    if (!this.canConfirmFinish()) return;
    if (this.activeExit && DECORATIONS[this.activeExit.id]?.exitKind === "escape") {
      const u = this.exitUnitHere(this.activeExit);
      if (!u) return;
      if (this.rng() * 100 < this.fleeChance(u)) {
        this.tip = `${u.name} encontrou uma saída! O grupo foge do combate.`;
        this.pushLog(this.tip);
        this.result = "victory";
      } else {
        u.moved = true;
        u.acted = true;
        u.x = Math.round(u.drawX);
        u.y = Math.round(u.drawY);
        u.drawX = u.x;
        u.drawY = u.y;
        this.deselect(true);
        this.tip = `${u.name} não conseguiu fugir — o combate continua.`;
        this.pushLog(this.tip);
      }
      sfxPlay.ui();
      this.emit();
      return;
    }
    this.result = "victory";
  }

  /** First not-yet-acted unit in this round's initiative order, or null if everyone has gone. */
  activeTurnUnit(): Unit | null {
    for (const id of this.turnOrder) {
      const u = this.units.find((x) => x.id === id);
      if (u && u.alive && !u.moved) return u;
    }
    return null;
  }

  /** Whoever the board should visually credit as "acting right now" — for activeTurnHighlight
   * only, never for turn-order logic (which stays on activeTurnUnit/`.moved` exactly as it
   * is). Enemy AI (runAiFor) sets `.moved = true` the instant it DECIDES to move, not once the
   * queued walk actually finishes — turn-advancement needs that (tick() only looks for the
   * next unit once `this.queue` fully drains, so the flag has to already be true by then), but
   * it means an enemy's own `activeTurnUnit()` stops returning it before its walk animation
   * even starts, so the hex vanished mid-move ("enemies have no hex when they move", a direct
   * complaint). Prefer whoever `this.active` (the queue item currently mid-playback) actually
   * belongs to — `.id` on a move, `.att` on everything else with an actor — falling back to
   * activeTurnUnit() the rest of the time (nothing queued, or a queue item with no actor, like
   * a banner/delay). */
  private visuallyActingUnit(): Unit | null {
    const a = this.active as { id?: string; att?: string } | null;
    const actorId = a?.id ?? a?.att;
    if (actorId) {
      const u = this.units.find((x) => x.id === actorId);
      if (u && u.alive) return u;
    }
    return this.activeTurnUnit();
  }

  private select(unit: Unit): void {
    if (unit.side !== "player" || !unit.alive || unit.moved || this.phase !== "player") return;
    const active = this.activeTurnUnit();
    if (active && active.id !== unit.id) {
      this.tip = `Ainda não é a vez de ${unit.name} — espere ${active.name} agir.`;
      return;
    }
    if (this.selectedId === unit.id && this.mode === "awaitAction") return;
    this.selectedId = unit.id;
    this.pendingFoeId = null;
    this.inspectedId = null;
    this.orig = { x: unit.x, y: unit.y };
    this.origMoveBudgetUsed = unit.moveBudgetUsed;
    this.reach = computeReachable(this.effectiveUnitForReach(unit), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
    this.attackFrom = unit.acted ? new Map() : this.visibleAttackTargets(unit);
    this.threat = [];
    this.mode = "selected";
    this.tip = null;
    this.ensureVisible(unit.x, unit.y);
    sfxPlay.select();
  }

  /** Read-only snapshot of any living unit, keyed by id — lets UI browse the roster (a
   * "next character" control on the status sheet, say) without touching inspectedId or
   * selectedId, so it can't disturb an attack/spell forecast already in progress the way
   * calling the private inspect() from outside would. */
  publicUnit(unitId: string): UnitPublic | null {
    const u = this.units.find((candidate) => candidate.id === unitId);
    if (!u || !u.alive) return null;
    return pub(this.affinityUnit(u), this.isWebCell(u.x, u.y), this.movLeft(u));
  }

  /** Drops the current inspection without touching the selection, so the same unit can be
   * clicked open again after its status sheet is closed. */
  dismissInspect(): void {
    this.inspectedId = null;
    this.threat = [];
    this.tip = null;
  }

  private inspect(unit: Unit): void {
    this.inspectedId = unit.id;
    this.threat = computeThreat(unit, this.tiles, this.cols, this.rows, this.units, this.decorOverlay);
    const max = effectiveMaxRange(unit, tileAt(this.tiles, this.cols, unit.x, unit.y));
    const tile = this.hexAt(unit.x, unit.y);
    this.tip = `${unit.name} · HP ${unit.hp}/${unit.maxHp} · Alc ${unit.minRange === max ? max : `${unit.minRange}–${max}`}${
      tile.height ? " · alto +10% atq" : ""
    }${tile.id === "barricade" ? " · barricada bloqueia projéteis" : ""}${
      unit.classId === "troll" || unit.classId === "troll2" ? " · parte barricadas" : ""
    }${
      unit.shock ? ` · Relâmpago ${diceFormula(unit.shock.dice, unit.shock.faces, unit.shock.bonus)} no turno` : ""
    }${unit.diseased ? " · Doente (−10% em todos os stats)" : ""}${unit.poisoned ? ` · ${POISON_TIERS[unit.poisonTier ?? "lesser"].name} (${poisonDice(unit.poisonTier ?? "lesser")} dano por turno)` : ""}`;
    this.ensureVisible(unit.x, unit.y);
    sfxPlay.ui();
  }

  /** Whether the movement taken this turn can still be taken back.
   *
   * Only for the player's own active unit, only while it is standing somewhere other than
   * where its turn began, and only while nothing has been spent that a rewind could not
   * honestly return: acting fixes the position the action was taken from, and a move that
   * broke a barricade or crossed a hazard has already changed the board (see moveSpoiled). */
  canUndoMove(): boolean {
    const u = this.activeTurnUnit();
    return (
      !!u &&
      u.side === "player" &&
      u.alive &&
      !u.acted &&
      !u.moved &&
      !this.moveSpoiled &&
      !!this.turnStart &&
      !this.active &&
      this.queue.length === 0 &&
      (u.x !== this.turnStart.x || u.y !== this.turnStart.y) &&
      (this.mode === "selected" ||
        this.mode === "awaitAction" ||
        this.mode === "awaitAttack" ||
        this.mode === "awaitOffHand" ||
        this.mode === "awaitSpell" ||
        this.mode === "awaitPotion")
    );
  }

  /** A post-action move that used the last movement points can still be cancelled before
   * the player explicitly ends the turn. `orig` is the safe position captured when the
   * action finished, so this only rewinds the movement after that action. */
  private canCancelCommittedMovement(): boolean {
    const u = this.units.find((candidate) => candidate.id === this.selectedId);
    return !!u &&
      u.side === "player" &&
      u.alive &&
      u.acted &&
      !u.moved &&
      !this.moveSpoiled &&
      !!this.orig &&
      (u.x !== this.orig.x || u.y !== this.orig.y) &&
      !this.active &&
      this.queue.length === 0 &&
      this.mode === "selected";
  }
  /** Puts the active unit back where its turn began and refunds every hex it walked — the
   * whole budget, not the last hop, so a wrong click costs nothing. Undoing is not itself a
   * move: the unit is left selected with its full reach, exactly as the turn opened. */
  undoMove(): void {
    if (!this.canUndoMove()) return;
    const u = this.activeTurnUnit()!;
    const back = this.turnStart!;
    u.x = back.x;
    u.y = back.y;
    u.drawX = back.x;
    u.drawY = back.y;
    u.moveBudgetUsed = 0;
    this.orig = { x: back.x, y: back.y };
    this.origMoveBudgetUsed = 0;
    this.pendingFoeId = null;
    this.inspectedId = null;
    this.threat = [];
    this.spellArmed = false;
    this.spellAim = null;
    this.spellKind = null;
    this.potionAim = null;
    this.missileTargets = [];
    this.selectedId = u.id;
    this.mode = "selected";
    this.reach = computeReachable(this.effectiveUnitForReach(u), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
    this.attackFrom = this.visibleAttackTargets(u);
    this.ensureVisible(u.x, u.y);
    this.centerOn(u.x, u.y);
    this.tip = `${u.name} voltou ao ponto de partida — ${u.mov} de movimento de volta.`;
    sfxPlay.ui();
    this.emit();
  }

  deselect(commit = false): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    const orig = this.orig;
    const canRestore = !commit && u && orig && (this.mode === "awaitAction" || this.mode === "selected");
    if (canRestore && u && orig) {
      // A cancel returns to the last safe point as one complete snapshot: position AND
      // movement. Before an action that is turn start (full movement); after an action it
      // is the action's position and the movement already spent to reach it.
      u.x = orig.x;
      u.y = orig.y;
      u.drawX = u.x;
      u.drawY = u.y;
      u.moveBudgetUsed = this.origMoveBudgetUsed ?? (u.acted ? u.moveBudgetUsed : 0);
      // Leaving a waypoint by cancelling the move invalidates the offered exit immediately.
      // Otherwise the stale exit prompt can still be confirmed from elsewhere on the map.
      this.evaluateEnd();
    }
    this.selectedId = null;
    this.pendingFoeId = null;
    this.inspectedId = null;
    this.threat = [];
    this.reach.clear();
    this.attackFrom.clear();
    this.orig = null;
    this.origMoveBudgetUsed = null;
    this.mode = "idle";
  }

  /** Backs a cancelled skill/attack/off-hand choice out to the same "selected" state the
   * unit was already in — reach and attackable targets recomputed fresh — instead of the
   * old half-cleared "awaitAction" mode, which never recomputed reach and left the
   * movement highlight gone until the unit was fully deselected and reselected. */
  private returnToSelected(u: Unit): void {
    this.selectedId = u.id;
    this.pendingFoeId = null;
    this.mode = "selected";
    this.reach = computeReachable(this.effectiveUnitForReach(u), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
    this.attackFrom = u.acted ? new Map() : this.visibleAttackTargets(u);
  }

  cancel(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    // Movement animations and their queued follow-up are still cancellable: the action has
    // not resolved until that walk settles. Rewind to the last safe point before deselecting.
    // This also lets a player change their mind after reaching a waypoint but before trying
    // its escape chance.
    if (this.active?.type === "move" && u?.side === "player" && u.alive && this.active.id === u.id) {
      this.active = null;
      this.queue.length = 0;
      this.onNextIdle = null;
      this.waypointCheckPending = false;
      this.mode = "selected";
      this.deselect();
      sfxPlay.ui();
      this.emit();
      return;
    }
    // Before an action resolves, Cancel has the same full-turn movement rewind as
    // Undo Movement: return to turn start, restore the whole movement budget, and keep
    // the unit selected so another move or action can be chosen. With no movement to
    // rewind, the individual action-mode handlers below simply return to selection.
    if (this.canUndoMove()) {
      this.undoMove();
      return;
    }
    if (this.mode === "awaitSpell") {
      this.spellArmed = false;
      this.spellAim = null;
      this.spellKind = null;
      this.missileTargets = [];
      this.tip = null;
      if (u) this.returnToSelected(u);
      else this.deselect();
      sfxPlay.ui();
      return;
    }
    if (this.mode === "awaitPotion") {
      this.potionAim = null;
      this.tip = null;
      if (u) this.returnToSelected(u);
      else this.deselect();
      sfxPlay.ui();
      return;
    }
    if ((this.mode === "awaitAttack" || this.mode === "awaitOffHand") && u) {
      this.tip = null;
      this.returnToSelected(u);
      sfxPlay.ui();
      return;
    }
    this.deselect();
    sfxPlay.ui();
  }

  wait(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || this.phase !== "player") return;
    this.noteUnitDrawAction(u.id);
    u.moved = true;
    u.x = Math.round(u.drawX);
    u.y = Math.round(u.drawY);
    u.drawX = u.x;
    u.drawY = u.y;
    this.deselect(true);
    sfxPlay.ui();
  }

  startAttack(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted) return;
    this.mode = "awaitAttack";
    this.tip = "Toque no alvo.";
    sfxPlay.ui();
  }

  startOffHand(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || !u.offHandId) return;
    this.mode = "awaitOffHand";
    this.tip = "Toque no alvo.";
    sfxPlay.ui();
  }

  /** Shared by off-hand highlighting and target validation so both use identical reach. */
  private offHandReach(unit: Unit): Unit {
    const item = unit.offHandId ? EQUIPMENT[unit.offHandId] : null;
    return item?.kind === "weapon"
      ? { ...unit, minRange: item.minRange ?? 1, maxRange: item.maxRange ?? 1 }
      : unit;
  }

  /** `kind`'s remaining casts for `u` this battle: a familiar casting its OWN spell (see
   * FAMILIAR_SPELL) draws from its own spellCharges (set at summon time, never a slot-table
   * tier — see familiarSpellCharges); every other caster (including a familiar's other
   * actions, which is a no-op since they have none) uses the normal tier-slot pool. */
  private familiarSpellRemaining(u: Unit, kind: SpellKind): number {
    return FAMILIAR_SPELL[u.classId] === kind ? (u.spellCharges ?? 0) : this.tierRemaining(u, kind);
  }

  /** Spends one cast of `kind` for `u`: its own spellCharges if `kind` is that familiar's own
   * spell (see FAMILIAR_SPELL), otherwise the normal tier-slot pool — has to agree with
   * familiarSpellRemaining above on which pool a given (unit, kind) pair actually draws from. */
  private spendFamiliarOrTier(u: Unit, kind: SpellKind): void {
    if (FAMILIAR_SPELL[u.classId] === kind) u.spellCharges = Math.max(0, (u.spellCharges ?? 1) - 1);
    else this.spendTier(u, kind);
  }

  startFireball(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.familiarSpellRemaining(u, "fireball") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "fireball";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${FIREBALL.name}: alcance ${FIREBALL.range}, ${fireballFormula(u.mag)} em área. Toque para mirar, toque de novo para lançar.`;
    sfxPlay.ui();
  }

  private frostTiles(origin:Point,through:Point,level?:number):Point[]{
    const caster=origin as Unit;const cells=frostAreaTiles(origin,through,level??caster.level,this.cols,this.rows);const out:Point[]=[];
    for(const cell of cells){if(tileAt(this.tiles,this.cols,cell.x,cell.y)==="void"||!clearShot(origin,cell,this.tiles,this.cols,"bolt",this.decorOverlay))break;out.push(cell);}return out;
  }
  startFrost():void {const u=this.units.find(x=>x.id===this.selectedId);if(!u||u.acted||(u.side==="player"&&u.level<FROST.unlockLevel)||this.tierRemaining(u,"frost")<=0)return;this.mode="awaitSpell";this.spellKind="frost";this.spellArmed=false;this.spellAim=null;this.hover=null;this.tip=`Frost: linha de ${frostPower(u.level).length} hexes à frente. Dano de Ice; atinge aliados também.`;}
  private queueFrost(u:Unit,origin:Point,through:Point):void {const cells=this.frostTiles(origin,through,u.level);if(!cells.length)return;const ids=this.units.filter(target=>target.alive&&target.id!==u.id&&cells.some(c=>occupies(target,c.x,c.y))).map(target=>target.id);const p=frostPower(u.level);this.spendTier(u,"frost");this.queue.push({type:"spell",att:u.id,tiles:cells,ids,dice:p.dice,faces:p.faces,bonus:0,spellMul:p.mul,label:FROST.name,spellKind:"frost"});}
  private castFrost(u:Unit,cell:Point):void {if(this.tierRemaining(u,"frost")<=0||!this.frostTiles(u,cell).length)return;this.queueFrost(u,u,cell);this.spellKind=null;this.mode="locked";this.tip=null;}
  startTurnUndead(): void {
    const u = this.units.find(x => x.id === this.selectedId);
    if (!u || u.acted || rulesClass(u.classId) !== "healer" || this.tierRemaining(u, "turnUndead") <= 0) return;
    const p = turnUndeadPower(u.level);
    this.mode = "awaitSpell";
    this.spellKind = "turnUndead";
    this.spellArmed = true;
    this.spellAim = { x: u.x, y: u.y };
    this.hover = { x: u.x, y: u.y };
    this.tip = `${TURN_UNDEAD.name}: radius ${p.radius} around the priest (${p.areaHexes} hexes), ${turnUndeadFormula(u.level, u.mag)} Holy damage and ${p.fearTurns} turns of fear against undead. Review the highlighted area, then confirm or cancel.`;
  }

  private turnUndeadTiles(cell: Point, level: number): Point[] {
    return hexAreaTiles(cell, turnUndeadPower(level).radius, this.cols, this.rows)
      .filter(p => tileAt(this.tiles, this.cols, p.x, p.y) !== "void");
  }

  private castTurnUndead(unit: Unit): void {
    if (unit.acted || rulesClass(unit.classId) !== "healer" || this.tierRemaining(unit, "turnUndead") <= 0) return;
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

  startIceStorm(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || u.level < ICE_STORM.unlockLevel || this.tierRemaining(u, "iceStorm") <= 0) return;
    const power = iceStormPower(u.level);
    this.mode = "awaitSpell";
    this.spellKind = "iceStorm";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${ICE_STORM.name}: alcance ${power.range}, área de ${power.areaHexes} hexes, ${power.durationRounds} rodadas, ${iceStormFormula(u.level, u.mag)} de dano de gelo por turno na área. Afeta aliados e inimigos que permanecerem no campo.`;
    sfxPlay.ui();
  }

  startCausticVenom(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.familiarSpellRemaining(u, "causticVenom") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "causticVenom";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${CAUSTIC_VENOM.name}: alcance ${CAUSTIC_VENOM.range}, alvo ${diceFormula(CAUSTIC_VENOM.centerDice, CAUSTIC_VENOM.centerFaces, CAUSTIC_VENOM.centerBonus)}, respingo ${diceFormula(CAUSTIC_VENOM.splashDice, CAUSTIC_VENOM.splashFaces, CAUSTIC_VENOM.splashBonus)} em área — envenena todos atingidos, até aliados. Toque para mirar, toque de novo para lançar.`;
    sfxPlay.ui();
  }

  startDivineBolt(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.name !== "Salazar" || rulesClass(u.classId) !== "healer" || u.acted || this.tierRemaining(u, "divineBolt") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "divineBolt";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${DIVINE_BOLT.name}: alcance ${DIVINE_BOLT.range}, centro ${spellFormula(u.mag, DIVINE_BOLT.centerMul, DIVINE_BOLT.centerDice, DIVINE_BOLT.centerFaces, DIVINE_BOLT.centerBonus)}, hexes adjacentes ${spellFormula(u.mag, DIVINE_BOLT.splashMul, DIVINE_BOLT.splashDice, DIVINE_BOLT.splashFaces, DIVINE_BOLT.splashBonus)}. Holy, raio ${DIVINE_BOLT.size}; atinge todos na área. Toque para mirar, toque de novo para lançar.`;
    sfxPlay.ui();
  }

  startMinorVenom(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.familiarSpellRemaining(u, "minorVenom") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "minorVenom";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${MINOR_VENOM.name}: alcance ${MINOR_VENOM.range}, alvo ${diceFormula(MINOR_VENOM.centerDice, MINOR_VENOM.centerFaces, MINOR_VENOM.centerBonus)}, respingo ${diceFormula(MINOR_VENOM.splashDice, MINOR_VENOM.splashFaces, MINOR_VENOM.splashBonus)} em área de raio ${MINOR_VENOM.size} — envenena todos atingidos, até aliados. Toque para mirar, toque de novo para lançar.`;
    sfxPlay.ui();
  }

  startLongShot(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "longShot") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "longShot";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${LONG_SHOT.name}: alcance ${u.minRange}–${this.longMax(u)}, ${longShotFormula(u.level)} − DF. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startBloodyShot(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || u.level < BLOODY_SHOT.unlockLevel || this.tierRemaining(u, "bloodyShot") <= 0) return;
    const bleed = bloodyShotBleed(u.level);
    this.mode = "awaitSpell";
    this.spellKind = "bloodyShot";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${BLOODY_SHOT.name}: ${Math.round(bloodyShotMul(u.level) * 100)}% do dano de arma, alcance ${u.minRange}–${BLOODY_SHOT.range}; sangramento 1D8 por ação durante ${diceFormula(bleed.dice, bleed.faces, 0)} rodadas. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startPiercing(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "piercing") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "piercing";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${PIERCING.name}: reta da colmeia. ${piercingMul(u.level)}× do AT − DF em cada um na linha, aliado ou inimigo.`;
    sfxPlay.ui();
  }

  startShock(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.familiarSpellRemaining(u, "shock") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "shock";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${SHOCK.name}: alcance ${SHOCK.range}, ${spellFormula(u.mag, SHOCK.mul, SHOCK.dice, SHOCK.faces, SHOCK.bonus)}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startLightning(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "lightning") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "lightning";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `Relâmpago: alcance ${LIGHTNING.range}, ${lightningFormula(u.mag)}. Atravessa cobertura e barricadas. No turno seguinte ${diceFormula(LIGHTNING.echoDice, LIGHTNING.echoFaces, LIGHTNING.echoBonus)}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startLightningTier3(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "lightningTier3") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "lightningTier3";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${LIGHTNING_T3.name}: alcance ${LIGHTNING_T3.range}, ${lightningTier3Formula(u.mag)}. Atravessa cobertura e barricadas. Eco ${diceFormula(LIGHTNING_T3.echoDice, LIGHTNING_T3.echoFaces, LIGHTNING_T3.echoBonus)}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startMagicMissile(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.familiarSpellRemaining(u, "magicMissile") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "magicMissile";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    const shots = magicMissileCount(u.level);
    this.tip = `${MAGIC_MISSILE.name}: alcance ${MAGIC_MISSILE.range}, ${spellFormula(u.mag, MAGIC_MISSILE.mul, MAGIC_MISSILE.dice, MAGIC_MISSILE.faces, MAGIC_MISSILE.bonus)} por míssil. ${shots} míssil${shots > 1 ? "eis, um alvo cada (pode repetir)" : ""}. Acerto garantido. Toque no inimigo.`;
    sfxPlay.ui();
  }

  /** Familiar Maior's own second spell — its own dedicated lifeDrainCharges pool, never the
   * generic familiarSpellRemaining/spendFamiliarOrTier machinery (that's reserved for the ONE
   * own-spell every other familiar tier has). */
  startLifeDrain(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || (u.lifeDrainCharges ?? 0) <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "lifeDrain";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${LIFE_DRAIN.name}: alcance ${LIFE_DRAIN.range}, ${lifeDrainFormula(u.level, u.mag)}, cura o invocador em ${Math.round(lifeDrainHealMul(u.level) * 100)}% do dano causado. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startDoubleStrike(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "doubleStrike") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "doubleStrike";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${DOUBLE_STRIKE.name}: ataca duas vezes, ${doubleStrikeFormula(u.level)}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startCleave(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "cleave") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "cleave";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${CLEAVE.name}: ${CLEAVE.hexes} hexes adjacentes, ${cleaveFormula(u.level)}. x${CLEAVE.largeMul} em criaturas de ${CLEAVE.largeHexes}+ hexes. Toque num hex vizinho.`;
    sfxPlay.ui();
  }

  /** Warrior tier 1's alternative to Corte Duplo (shares its charge pool), available at level 3. */
  startBullRush(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "bullRush") <= 0) return;
    if (u.classId !== "bigBlueCalf" && u.level < BULL_RUSH_UNLOCK_LEVEL) {
      this.tip = `${BULL_RUSH.name} disponível a partir do nível ${BULL_RUSH_UNLOCK_LEVEL}.`;
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "bullRush";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    const p = bullRushPower(u.level);
    this.tip = `${BULL_RUSH.name}: investida em linha reta até ${BULL_RUSH_RANGE} hexes, para no primeiro inimigo, ${bullRushFormula(u.level)}, sem contra-ataque. Empurra 2 hexes (criaturas grandes: 1). Se o empurrão bater em algo, causa +${diceFormula(p.wallDice, p.wallFaces, 0)} de impacto.`;
    sfxPlay.ui();
  }

  /** Every hex from `u` to `cell` walked strictly along one of the 6 true hex axes — the
   * shared geometry behind Bull Rush's approach (spellAimValid/castBullRush) and its
   * knockback (castBullRush's wall-impact precomputation). `blockedAt` reports true for
   * anything that stops the charge: impassable terrain/walls/columns/barricades/locked
   * doors/decorations (all folded into hexDef's `passable`, see hexprops.ts) or a live unit. */
  private axisBlocked(x: number, y: number, occ: Map<string, Unit>): boolean {
    if (!inBounds(x, y, this.cols, this.rows)) return true;
    if (!hexDef(this.tiles, this.cols, x, y, this.decorOverlay).passable) return true;
    return !!occ.get(key(x, y));
  }

  /** Terrain stops a charge. */
  private chargeTerrainBlocked(x: number, y: number): boolean {
    return !inBounds(x, y, this.cols, this.rows) || !hexDef(this.tiles, this.cols, x, y, this.decorOverlay).passable;
  }

  /** A Bull Rush push, precomputed before anything moves: who, where it ends up, and
   * whether it slammed into something short of its full distance (impact damage). */
  private bullRushPush(foe: Unit, dir: Cube, occ: Map<string, Unit>): { path: Point[]; blocked: boolean } {
    const dist = this.bullRushPushDistance(foe);
    const knock = axisWalk({ x: foe.x, y: foe.y }, dir, this.cols, this.rows, dist, (pt) => !this.bullRushPushFits(foe, pt, occ));
    return { path: knock.path, blocked: knock.path.length < dist };
  }

  /** Bull Rush: aim at an enemy up to BULL_RUSH_RANGE away and charge the straight line to
   * it. Any enemy standing in the way is hit and shoved aside (to whichever flank has room,
   * off the rest of the line) and the charge carries on; the aimed enemy is hit and pushed
   * forward. If an in-the-way enemy has no room on either side, the charge stops there and
   * it becomes the one pushed forward. An ally or blocking terrain in the way = no charge.
   * Everything is simulated on a copy of the board, so no push or landing hex ever overlaps
   * a unit or a body-type target zone. */
  private bullRushCharge(
    caster: Unit,
    cell: Point,
  ): {
    dir: Cube;
    foe: Unit;
    /** Charge segments: run `path`, then hit `foe` and push it along `push`. */
    legs: { path: Point[]; foe: Unit; push: { path: Point[]; blocked: boolean } }[];
  } | null {
    if (hexDist(caster, cell) > BULL_RUSH_RANGE) return null;
    const occ = new Map(this.occ());
    const target = occ.get(key(cell.x, cell.y));
    if (!target || !target.alive || target.side === caster.side) return null;
    const line = hexLine(caster, cell).slice(1);
    const legs: { path: Point[]; foe: Unit; push: { path: Point[]; blocked: boolean } }[] = [];
    let run: Point[] = [];
    let at: Point = { x: caster.x, y: caster.y };
    const place = (u: Unit, to: Point) => {
      for (const c of footprint(u)) if (occ.get(key(c.x, c.y)) === u) occ.delete(key(c.x, c.y));
      for (const c of footprint({ ...u, x: to.x, y: to.y })) occ.set(key(c.x, c.y), u);
    };
    for (let i = 0; i < line.length; i++) {
      const pt = line[i]!;
      if (this.chargeTerrainBlocked(pt.x, pt.y)) return null;
      const body = footprint({ ...caster, ...pt });
      if (caster.classId === "bigBlueCalf" && body.some((c) => this.chargeTerrainBlocked(c.x, c.y))) return null;
      const who = caster.classId === "bigBlueCalf"
        ? body.map((c) => occ.get(key(c.x, c.y))).find((u) => u != null && u.id !== caster.id)
        : occ.get(key(pt.x, pt.y));
      if (!who || who.id === caster.id) {
        if (caster.classId === "bigBlueCalf" && footprint({ ...caster, ...pt }).some((c) =>
          this.chargeTerrainBlocked(c.x, c.y) || (occ.get(key(c.x, c.y)) != null && occ.get(key(c.x, c.y))!.id !== caster.id))) return null;
        run.push(pt);
        at = pt;
        continue;
      }
      if (!who.alive || who.side === caster.side) return null;
      const dir = axisDir(at, pt);
      if (!dir) return null;
      // The charger's landing hex for this leg (and only that one), so pushes never land on it.
      for (const [k, u] of occ) if (u === caster) occ.delete(k);
      for (const c of footprint({ ...caster, ...at })) occ.set(key(c.x, c.y), caster);
      if (who.id !== target.id) {
        // In the way: shove it aside, off the rest of the line, then keep charging.
        const di = CUBE_DIRS.findIndex((d) => d.q === dir.q && d.r === dir.r && d.s === dir.s);
        const ahead = new Set(line.slice(i).map((c) => key(c.x, c.y)));
        let side: { path: Point[]; blocked: boolean } | null = null;
        for (const s of [CUBE_DIRS[(di + 1) % 6]!, CUBE_DIRS[(di + 5) % 6]!]) {
          const push = this.bullRushPush(who, s, occ);
          const land = push.path[push.path.length - 1];
          if (!land) continue;
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

  /** Bull Rush push distance: a body-type creature (any multi-hex footprint) moves 1 hex, a
   * normal one-hex creature 2. */
  private bullRushPushDistance(foe: Unit): number {
    return foe.footprintOffsets && foe.footprintOffsets.length > 1 ? 1 : 2;
  }

  /** Whether `foe` can be pushed so its anchor lands on `to`: its front row must be in
   * bounds on passable ground, and no cell of its whole body may overlap any other unit or
   * that unit's target zone (`occ` already has the charger at its landing hex). */
  private bullRushPushFits(foe: Unit, to: Point, occ: Map<string, Unit>): boolean {
    const placed = { ...foe, x: to.x, y: to.y };
    for (const c of footprintFrontRow(placed)) if (this.chargeTerrainBlocked(c.x, c.y)) return false;
    for (const c of footprint(placed)) {
      if (!inBounds(c.x, c.y, this.cols, this.rows)) continue;
      const who = occ.get(key(c.x, c.y));
      if (who && who.id !== foe.id) return false;
    }
    return true;
  }

  private castBullRush(unit: Unit, cell: Point): void {
    if (!unit.alive || unit.acted || this.tierRemaining(unit, "bullRush") <= 0) return;
    const p = bullRushPower(unit.level);
    const charge = this.bullRushCharge(unit, cell);
    if (!charge) {
      this.tip = `Toque num inimigo a até ${BULL_RUSH_RANGE} hexes, sem aliado ou obstáculo no caminho.`;
      sfxPlay.ui();
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

  /** Warrior tier 3: adjacent, replaces (never stacks with) a normal crit — see
   * stepCombat's executionerStrike branch. */
  startProvoke(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || u.level < PROVOKE.unlockLevel || this.tierRemaining(u, "provoke") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "provoke";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${PROVOKE.name}: ${provokeFormula(u.level)}. Os inimigos atingidos voltam-se contra ${u.name}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  /** Board cells Provoke reaches around its aim — only the aim itself at radius 0. */
  private provokeArea(cell: Point, level: number): Point[] {
    const { radius } = provokePower(level);
    const out: Point[] = [];
    for (let y = cell.y - radius; y <= cell.y + radius; y++) {
      for (let x = cell.x - radius - 1; x <= cell.x + radius + 1; x++) {
        if (inBounds(x, y, this.cols, this.rows) && hexDist(cell, { x, y }) <= radius) out.push({ x, y });
      }
    }
    return out;
  }

  /** Provoke: no damage — every enemy in the area gets Provoke's huge volatile enmity on the
   * warrior (enmity.ts), so it turns on him until someone out-generates it. */
  private castProvoke(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const area = new Set(this.provokeArea(cell, unit.level).map((p) => key(p.x, p.y)));
    const foes = this.units.filter((f) => f.alive && f.side === "enemy" && this.targetable(f) && footprint(f).some((c) => area.has(key(c.x, c.y))));
    this.spendTier(unit, "provoke");
    for (const foe of foes) {
      this.addEnmity(foe, unit, ENMITY.provoke.ce, ENMITY.provoke.ve);
      this.provokeFx.push({ unitId: foe.id, t: 0 });
      this.emitParticle({ x: foe.drawX, y: foe.drawY - 0.35, vx: 0, vy: -0.18, life: 0, max: 2, size: 1, color: "#e0603a", text: "Provocado", kind: "text", frame: 0 });
    }
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.missileTargets = [];
    this.tip = null;
    this.pushLog(`${unit.name} usa ${PROVOKE.name}: ${foes.map((f) => f.name).join(", ")} ${foes.length > 1 ? "voltam-se" : "volta-se"} contra ${unit.name}.`);
    sfxPlay.ui();
    this.finishAction(unit);
  }

  startExecutionerStrike(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "executionerStrike") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "executionerStrike";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${EXECUTIONER_STRIKE.name}: ${executionerStrikeFormula(u.level)}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  private castExecutionerStrike(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
    this.spendTier(unit, "executionerStrike");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = executionerStrikePower(unit.level);
    this.queue.push({ type: "combat", att: unit.id, def: foe.id, bonusDice: power.faces, bonusDiceCount: power.dice, bonusFlat: 0, spellKind: "executionerStrike" });
  }

  /** Warrior tier 2's shield-only option (shares Cleave's charge pool) — refuses to arm
   * without a shield in the off hand, the mirror of startShoulderSmash's own "no shield"
   * gate (which requires bare/two hands instead). */
  startShieldBash(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "shieldBash") <= 0) return;
    if (!u.offHandId || EQUIPMENT[u.offHandId]?.kind !== "shield") {
      this.tip = "Requer um escudo equipado.";
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "shieldBash";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${SHIELD_BASH.name}: ${shieldBashFormula(u.level)}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  private castShieldBash(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
    this.spendTier(unit, "shieldBash");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    const power = shieldBashPower(unit.level);
    this.queue.push({ type: "combat", att: unit.id, def: foe.id, bonusDice: power.faces, bonusDiceCount: power.dice, bonusFlat: 0, spellKind: "shieldBash" });
  }

  startPiercingThrust(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "piercingThrust") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "piercingThrust";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${PIERCING_THRUST.name}: reta curta, ignora ${Math.round(PIERCING_THRUST.armorIgnore * 100)}% da defesa. 1º alvo dano cheio, os demais metade.`;
    sfxPlay.ui();
  }

  /** Sweep (Lancer tier 2): self-centered AoE — preview the radius, then confirm. */
  startSweep(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "sweep") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "sweep";
    this.spellArmed = true;
    this.spellAim = { x: u.x, y: u.y };
    this.hover = { x: u.x, y: u.y };
    this.tip = `${SWEEP.name}: inimigos a até ${SWEEP.radius} hexes, dano de arma, empurra ${SWEEP.knockback} hex. A área está marcada — Lançar para confirmar.`;
    sfxPlay.ui();
  }

  private sweepTiles(u: Unit): Point[] {
    return hexAreaTiles({ x: u.x, y: u.y }, SWEEP.radius, this.cols, this.rows).filter((t) => t.x !== u.x || t.y !== u.y);
  }

  private confirmSweep(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || this.mode !== "awaitSpell" || this.spellKind !== "sweep") return;
    const tiles = this.sweepTiles(u);
    const ids: string[] = [];
    for (const t of tiles) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== u.id && who.side !== u.side && !ids.includes(who.id)) ids.push(who.id);
    }
    this.spendTier(u, "sweep");
    this.spellKind = null;
    this.missileTargets = [];
    this.spellArmed = false;
    this.spellAim = null;
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: u.id, tiles, ids, label: SWEEP.name, spellKind: "sweep" });
    if (u.name !== "Kael") sfxPlay.sweep(this.isBladeAttack(u));
  }

  startTrip(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "trip") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "trip";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${TRIP.name}: dano da arma + ${diceFormula(1, TRIP.bonusFaces, TRIP.bonusBonus)}, causa Sangramento (1D8 a cada ação) e reduz stats em ${Math.round(TRIP.statPenalty * 100)}% até o fim do combate. Toque no inimigo.`;
    sfxPlay.ui();
  }

  /** One of each familiar tier at a time per caster — a conjurer re-casting a tier it
   * already has out just replaces nothing and clutters the field, so every summonFamiliarX
   * entry point (start and cast, both checked for the same reason spellAimValid AND
   * castX both validate range) blocks while a living familiar of that exact class still
   * carries this caster's id as its summonerId. Tiers stack freely with each other — this is
   * a per-tier cap, not "one familiar total". */
  private hasFamiliarOut(caster: Unit, classId: ClassId): boolean {
    return this.units.some((u) => u.alive && u.summonerId === caster.id && u.classId === classId);
  }

  private isWarpDungeon(): boolean {
    const missionText = `${this.mission.id} ${this.mission.title} ${this.mission.place}`.toLowerCase();
    if (/dungeon|crossing|travessia|cripta|crypt|cave|caverna|ruins|ruínas|colina|passagem|profundezas|watchtower.*(undercroft|prison)|masmorra/.test(missionText)) return true;
    return (this.mission.decorations ?? []).some((placement) => {
      const kind = DECORATIONS[placement.id]?.exitKind;
      return kind === "dungeon" || kind === "connector";
    });
  }

  /** Crossing happens on the movement step that enters either opening. */
  private tryWarpCrossing(unit: Unit, action: MoveAnim): boolean {
    if (unit.side !== "player" && footprint(unit).length > 1) return false;
    const gate = this.warpGates.find((g) => (unit.x === g.a.x && unit.y === g.a.y) || (unit.x === g.b.x && unit.y === g.b.y));
    if (!gate) return false;
    const atA = unit.x === gate.a.x && unit.y === gate.a.y;
    const other = atA ? gate.b : gate.a;
    // Land beside the far gate, never in either opening: landing in it counted as entering it,
    // which sent the unit straight back — an endless blink between the two gates.
    const isGate = (cell: Point) => this.warpGates.some((g) => (cell.x === g.a.x && cell.y === g.a.y) || (cell.x === g.b.x && cell.y === g.b.y));
    const landing = hexAreaTiles(other, 2, this.cols, this.rows)
      .filter((cell) => !isGate(cell) && this.hexAt(cell.x, cell.y).passable && !this.units.some((otherUnit) => otherUnit.id !== unit.id && otherUnit.alive && occupies(otherUnit, cell.x, cell.y)))
      .sort((a, b) => hexDist(a, other) - hexDist(b, other) || a.y - b.y || a.x - b.x)[0];
    if (!landing) return false;
    // the move's end must leave it here, not put it on the hex that was clicked (see commitMove)
    this.warpedThisMove.add(unit.id);
    unit.x = landing.x;
    unit.y = landing.y;
    unit.drawX = landing.x;
    unit.drawY = landing.y;
    action.path = [{ x: landing.x, y: landing.y }];
    action.i = 0;
    action.t = 0;
    this.invalidateOcc();
    this.ensureVisible(landing.x, landing.y);
    this.tip = unit.side === "player" ? `${unit.name} atravessou o portal Warp.` : `${unit.name} atravessou o portal Warp.`;
    return true;
  }

  startWarp(): void {
    const unit = this.units.find((candidate) => candidate.id === this.selectedId);
    if (!unit || unit.acted || rulesClass(unit.classId) !== "mage") return;
    if (unit.level < WARP.unlockLevel) {
      this.tip = `${WARP.name} disponível a partir do nível ${WARP.unlockLevel}.`;
      sfxPlay.ui();
      return;
    }
    if (this.tierRemaining(unit, "warp") <= 0) return;
    this.spellKind = "warp";
    if (this.isWarpDungeon()) {
      this.castWarp(unit, { x: unit.x, y: unit.y }, true);
      return;
    }
    this.mode = "awaitSpell";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `Warp: escolha o outro ponto do portal. As aberturas ficam por ${warpPortalRounds(unit.level)} rodadas; atravesse uma delas para cruzar.`;
    sfxPlay.ui();
  }

  private castWarp(unit: Unit, target: Point, toEntrance = false): void {
    if (unit.level < WARP.unlockLevel || this.tierRemaining(unit, "warp") <= 0) {
      this.spellKind = null;
      return;
    }
    const radius = warpPartyRadius(unit.level);
    // a cell already holding a gate (or a unit) can't take a new opening
    const occupiedByOther = (cell: Point) => this.units.some((occupant) => occupant.alive && occupies(occupant, cell.x, cell.y))
      || this.warpGates.some((g) => (g.a.x === cell.x && g.a.y === cell.y) || (g.b.x === cell.x && g.b.y === cell.y));
    const freeCellsNear = (center: Point, maxRadius: number) => hexAreaTiles(center, maxRadius, this.cols, this.rows)
      .filter((cell) => this.hexAt(cell.x, cell.y).passable && !occupiedByOther(cell))
      .sort((a, b) => hexDist(a, center) - hexDist(b, center) || a.y - b.y || a.x - b.x);
    const sourceCandidates = hexNeighbors(unit.x, unit.y)
      .filter((cell) => this.hexAt(cell.x, cell.y).passable && !occupiedByOther(cell));
    const nearestEnemy = this.units.filter((foe) => foe.alive && foe.side !== "player")
      .sort((a, b) => hexDist(unit, a) - hexDist(unit, b))[0];
    sourceCandidates.sort((a, b) => (nearestEnemy ? hexDist(a, nearestEnemy) - hexDist(b, nearestEnemy) : 0) || a.y - b.y || a.x - b.x);
    const source = sourceCandidates[0];
    const entrance = this.mission.playerSpawns[0] ?? { x: unit.x, y: unit.y };
    const farCandidates = toEntrance ? freeCellsNear(entrance, 3) : freeCellsNear(target, 1);
    const destination = farCandidates[0];
    if (!source || !destination || (source.x === destination.x && source.y === destination.y)) {
      this.spellKind = null;
      this.tip = "Não há espaço livre para abrir os dois portais.";
      sfxPlay.ui();
      return;
    }
    const opened: WarpGatePair = { a: source, b: destination, roundsLeft: warpPortalRounds(unit.level), level: unit.level, casterId: unit.id };
    this.warpGates.push(opened);
    this.emitWarpFx(source.x, source.y, radius, unit.level);
    this.emitWarpFx(destination.x, destination.y, radius, unit.level);
    this.spendTier(unit, "warp");
    this.noteAwareEnmity(unit, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.missileTargets = [];
    this.mode = "locked";
    this.tip = `Warp: dois portais abertos por ${opened.roundsLeft} rodadas. Aliados e inimigos de tamanho humano podem atravessar ao alcançá-los.`;
    this.queue.push({ type: "spell", att: unit.id, tiles: [source, destination], ids: [], label: WARP.name, spellKind: "warp" });
  }

  startSummonFamiliar(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "summonFamiliar") <= 0) return;
    if (this.hasFamiliarOut(u, "familiar")) {
      this.tip = `${u.name} já tem ${SUMMON_FAMILIAR.name} invocado.`;
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "summonFamiliar";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${SUMMON_FAMILIAR.name}: convoca um aliado com metade dos seus atributos atuais, até ${SUMMON_FAMILIAR.range} hexes. Pode lançar Míssil Mágico por conta própria. Toque num espaço livre.`;
    sfxPlay.ui();
  }

  /** Conjurer tier 1's second spell — shares Invocar Familiar's own tier-1 pool of uses
   * (tierRemaining/spendTier), but gated further by the caster's own level, since tierUses
   * alone can't express "unlocked partway through a tier both spells already share". */
  startPhantasmalForce(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "phantasmalForce") <= 0) return;
    if (u.level < PHANTASMAL_FORCE_UNLOCK_LEVEL) {
      this.tip = `${PHANTASMAL_FORCE.name} disponível a partir do nível ${PHANTASMAL_FORCE_UNLOCK_LEVEL}.`;
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "phantasmalForce";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${PHANTASMAL_FORCE.name}: alcance ${PHANTASMAL_FORCE.range}, ${phantasmalForceFormula(u.level, u.mag)}. Toque no inimigo.`;
    sfxPlay.ui();
  }

  startSummonFamiliar2(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "summonFamiliar2") <= 0) return;
    if (u.level < SUMMON_FAMILIAR2_UNLOCK_LEVEL) {
      this.tip = `${SUMMON_FAMILIAR2.name} disponível a partir do nível ${SUMMON_FAMILIAR2_UNLOCK_LEVEL}.`;
      sfxPlay.ui();
      return;
    }
    if (this.hasFamiliarOut(u, "familiar2")) {
      this.tip = `${u.name} já tem ${SUMMON_FAMILIAR2.name} invocado.`;
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "summonFamiliar2";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${SUMMON_FAMILIAR2.name}: convoca um aliado maior, com ${Math.round(SUMMON_FAMILIAR2.statScale * 100)}% dos seus atributos atuais, até ${SUMMON_FAMILIAR2.range} hexes. Pode lançar Míssil Mágico ou Dreno de Vida por conta própria. Toque num espaço livre.`;
    sfxPlay.ui();
  }

  startSummonFamiliar3(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "summonFamiliar3") <= 0) return;
    if (this.hasFamiliarOut(u, "familiar3")) {
      this.tip = `${u.name} já tem ${SUMMON_FAMILIAR3.name} invocado.`;
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "summonFamiliar3";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${SUMMON_FAMILIAR3.name}: convoca um aliado com ${Math.round(SUMMON_FAMILIAR3.statScale * 100)}% dos seus atributos atuais, até ${SUMMON_FAMILIAR3.range} hexes. Pode lançar Bola de Fogo por conta própria. Toque num espaço livre.`;
    sfxPlay.ui();
  }

  startSummonFamiliar4(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "summonFamiliar4") <= 0) return;
    if (this.hasFamiliarOut(u, "familiar4")) {
      this.tip = `${u.name} já tem ${SUMMON_FAMILIAR4.name} invocado.`;
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "summonFamiliar4";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${SUMMON_FAMILIAR4.name}: convoca um aliado radiante, com ${Math.round(SUMMON_FAMILIAR4.statScale * 100)}% dos seus atributos atuais, até ${SUMMON_FAMILIAR4.range} hexes. Pode lançar Míssil Mágico ou Dreno de Vida por conta própria. Toque num espaço livre.`;
    sfxPlay.ui();
  }

  startSummonZombieDog(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "summonZombieDog") <= 0) return;
    if (this.hasFamiliarOut(u, "zombieDog")) {
      this.tip = `${u.name} já tem ${CLASSES.zombieDog!.name} invocado.`;
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "summonZombieDog";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${SUMMON_ZOMBIE_DOG.name}: convoca um Cão Zumbi com ${Math.round(SUMMON_ZOMBIE_DOG.statScale * 100)}% dos seus atributos atuais, até ${SUMMON_ZOMBIE_DOG.range} hexes. Pode lançar Veneno Cáustico ${SUMMON_ZOMBIE_DOG.causticVenomCharges}× por conta própria. Toque num espaço livre.`;
    sfxPlay.ui();
  }

  startWebOfDreams(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "webOfDreams") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "webOfDreams";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${WEB_OF_DREAMS.name}: cria uma teia grudenta por ${WEB_OF_DREAMS.durationRounds} rodadas — quem estiver dentro fica com movimento reduzido a 1 hex, e testa ${Math.round(webOfDreamsSleepChance(u.level) * 100)}% de chance de adormecer por ${diceFormula(WEB_OF_DREAMS.sleepDice, WEB_OF_DREAMS.sleepFaces, 0)} turnos a cada turno que permanecer lá dentro (cumulativo). Alcance ${WEB_OF_DREAMS.range}, raio ${webOfDreamsSize(u.level)}. Toque para mirar.`;
    sfxPlay.ui();
  }

  startMultiShot(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "multiShot") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "multiShot";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    const want = multiShotTargets(u.level);
    this.tip = `${MULTI_SHOT.name}: ${multiShotFormula(u.level)}, alcance ${MULTI_SHOT.range}. Escolha ${want} alvos (pode repetir).`;
    sfxPlay.ui();
  }

  /** Aura of Protection (Paladin tier 5) / Intimidating Presence (Heavy Knight tier 5): both
   * instant and self-centered, same as Sweep — no aim, no confirmSpell branch needed. */
  startAuraOfProtection(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "auraOfProtection") <= 0) return;
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
    sfxPlay.ui();
  }

  startIntimidatingPresence(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "intimidatingPresence") <= 0) return;
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
    this.emitBladeFx("shockRing", u.x, u.y);
    this.queue.push({ type: "banner", text: INTIMIDATING_PRESENCE.name, dur: 1.1 });
    sfxPlay.ui();
  }

  /** Healer tier 1 Bless: the caster is the center and every living ally within three hexes
   * receives the level-scaled accuracy bonus as the authored 3D wave reaches them. */
  startBless(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "bless") <= 0) return;
    if (rulesClass(u.classId) !== "healer") return;
    if (u.level < BLESS.unlockLevel) {
      this.tip = `Bless disponível a partir do nível ${BLESS.unlockLevel}.`;
      sfxPlay.ui();
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
    sfxPlay.ui();
  }

  /** Healer tier 3: resolves instantly like Aura of Protection/Intimidating Presence — never
   * arms awaitSpell, so it never reaches spellAimValid/confirmSpell. Tops off the hunger of
   * the caster and every ally within CREATE_FOOD_AND_WATER.radius hexes, adding plain Rations
   * per ally fed to the same pool a battle-picked-up ration would (see lootRations,
   * reconciled back into save.rations at battle end — see GameApp.tsx). */
  startCreateFoodAndWater(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "createFoodAndWater") <= 0) return;
    const power = createFoodAndWaterPower(u.level);
    const targets = this.units.filter(
      (t) => t.alive && t.side === u.side && hexDist(u, t) <= CREATE_FOOD_AND_WATER.radius && fullness(t.fullness) < power.fullness,
    );
    if (targets.length === 0) {
      this.tip = "Já está bem alimentado.";
      sfxPlay.ui();
      return;
    }
    let gained = 0;
    for (const t of targets) {
      t.fullness = power.fullness;
      t.hungerPenaltyPct = 0;
      this.reapplyGear(t);
      gained += power.dice > 0 ? rollDice(power.dice, power.faces, power.bonus, this.rng) : 0;
      this.emitHolyFx(t.x, t.y, "food", t.id);
    }
    this.lootRations += gained;
    this.spendTier(u, "createFoodAndWater");
    this.noteAwareEnmity(u, ENMITY.support.ce, ENMITY.support.ve);
    this.spellKind = null;
    this.spellArmed = false;
    this.spellAim = null;
    this.mode = "locked";
    this.tip = `${CREATE_FOOD_AND_WATER.name}: fome restaurada${power.fullness > 100 ? ` (${power.fullness}%)` : ""}${gained > 0 ? `, +${gained} rações` : ""}.`;
    // Salazar V2 plays his healing sheet for it (the effect has already resolved above).
    if ((this.art.castsHeal[u.sprite]?.length ?? 0) >= LONG_SHEET_FRAMES) this.queue.push({ type: "castPose", id: u.id });
    this.queue.push({ type: "banner", text: CREATE_FOOD_AND_WATER.name, dur: 1.1 });
    sfxPlay.healBy(u.sprite);
  }

  startDivineWrath(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "divineWrath") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "divineWrath";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${DIVINE_WRATH.name}: linha reta, ${divineWrathFormula(u.level, u.mag)}, nunca atinge aliados. Alcance ${DIVINE_WRATH.range}. Toque para mirar.`;
    sfxPlay.ui();
  }

  /** Shoulder Smash (Heavy Knight tier 4): refuses to arm while a shield is equipped in the
   * off hand — it's the bare-handed/two-handed version of a knightly charge. */
  startShoulderSmash(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "shoulderSmash") <= 0) return;
    if (u.offHandId && EQUIPMENT[u.offHandId]?.kind === "shield") {
      this.tip = "Requer as duas mãos livres — sem escudo equipado.";
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitSpell";
    this.spellKind = "shoulderSmash";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    const p = shoulderSmashPower(u.level);
    this.tip = `${SHOULDER_SMASH.name}: ${p.hexes} hexes adjacentes, ${shoulderSmashFormula(u.level)}, empurra ${SHOULDER_SMASH.knockback} hexes. Toque num hex vizinho.`;
    sfxPlay.ui();
  }

  startStampede(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "stampede") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "stampede";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${STAMPEDE.name}: linha reta, ${stampedeFormula(u.level)}, atinge todos na linha (aliados inclusos). Alcance ${STAMPEDE.range}. Toque para mirar.`;
    sfxPlay.ui();
  }

  confirmSpell(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || this.mode !== "awaitSpell" || !this.spellKind) return;
    if (this.spellKind === "turnUndead") { this.castTurnUndead(u); return; }
    if (this.spellKind === "sweep") {
      this.confirmSweep();
      return;
    }
    const cell = this.hover;
    if (!cell) return;
    if (this.spellKind === "warp") {
      if (!this.spellAimValid(u, cell)) {
        this.tip = "Escolha uma casa livre.";
        sfxPlay.ui();
        return;
      }
      this.castWarp(u, cell);
      return;
    }
    if (this.spellKind === "fireball") {
      this.confirmFireball();
      return;
    }
    if (this.spellKind === "frost") { this.castFrost(u, cell); return; }
    if (this.spellKind === "iceStorm") {
      this.castIceStorm(u, cell);
      return;
    }
    if (this.spellKind === "causticVenom") {
      this.confirmCausticVenom();
      return;
    }
    if (this.spellKind === "divineBolt") {
      this.confirmDivineBolt();
      return;
    }
    if (this.spellKind === "minorVenom") {
      this.confirmMinorVenom();
      return;
    }
    if (this.spellKind === "bloodyShot") {
      this.castBloodyShot(u, cell);
      return;
    }
    if (this.spellKind === "longShot") {
      this.castLongShot(u, cell);
      return;
    }
    if (this.spellKind === "piercing") {
      this.castPiercing(u, cell);
      return;
    }
    if (this.spellKind === "piercingThrust") {
      this.castPiercingThrust(u, cell);
      return;
    }
    if (this.spellKind === "trip") {
      this.castTrip(u, cell);
      return;
    }
    if (this.spellKind === "summonFamiliar") {
      this.castSummonFamiliar(u, cell, 1);
      return;
    }
    if (this.spellKind === "summonFamiliar2") {
      this.castSummonFamiliar(u, cell, 2);
      return;
    }
    if (this.spellKind === "summonFamiliar3") {
      this.castSummonFamiliar(u, cell, 3);
      return;
    }
    if (this.spellKind === "summonFamiliar4") {
      this.castSummonFamiliar(u, cell, 4);
      return;
    }
    if (this.spellKind === "summonZombieDog") {
      this.castSummonFamiliar(u, cell, 5);
      return;
    }
    if (this.spellKind === "webOfDreams") {
      this.castWebOfDreams(u, cell);
      return;
    }
    if (this.spellKind === "lightning") {
      this.castLightning(u, cell);
      return;
    }
    if (this.spellKind === "lightningTier3") {
      this.castLightningTier3(u, cell);
      return;
    }
    if (this.spellKind === "magicMissile" || this.spellKind === "magicMissileV2") {
      this.castMagicMissile(u, cell);
      return;
    }
    if (this.spellKind === "lifeDrain") {
      this.castLifeDrain(u, cell);
      return;
    }
    if (this.spellKind === "phantasmalForce") {
      this.castPhantasmalForce(u, cell);
      return;
    }
    if (this.spellKind === "doubleStrike") {
      this.castDoubleStrike(u, cell);
      return;
    }
    if (this.spellKind === "cleave") {
      this.castCleave(u, cell);
      return;
    }
    if (this.spellKind === "cureDisease") {
      this.castCureDisease(u, cell);
      return;
    }
    if (this.spellKind === "multiShot") {
      this.castMultiShot(u, cell);
      return;
    }
    if (this.spellKind === "divineWrath") {
      this.castDivineWrath(u, cell);
      return;
    }
    if (this.spellKind === "shoulderSmash") {
      this.castShoulderSmash(u, cell);
      return;
    }
    if (this.spellKind === "stampede") {
      this.castStampede(u, cell);
      return;
    }
    if (this.spellKind === "bullRush") {
      this.castBullRush(u, cell);
      return;
    }
    if (this.spellKind === "provoke") {
      this.castProvoke(u, cell);
      return;
    }
    if (this.spellKind === "executionerStrike") {
      this.castExecutionerStrike(u, cell);
      return;
    }
    if (this.spellKind === "shieldBash") {
      this.castShieldBash(u, cell);
      return;
    }
    if (this.spellKind === "burningHands" || this.spellKind === "poisonBreath") {
      this.castBurningHands(u, cell, this.spellKind);
      return;
    }
    // instant, resolved directly by their own startX() — never reaches here
    if (this.spellKind === "auraOfProtection" || this.spellKind === "intimidatingPresence" || this.spellKind === "createFoodAndWater") return;
    // secondWind is a passive triggered from startOfTurnEffects, never armed via a startX()
    if (this.spellKind === "secondWind") return;
    // Choque is enemy-AI only — never armed from the player hotbar
    if (this.spellKind === "shock") {
      this.castShock(u, cell);
      return;
    }
    if (this.isHeal(this.spellKind)) this.castHeal(u, cell, this.spellKind);
  }

  startCure(kind: HealId): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, kind) <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = kind;
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${CURES[kind].name}: ${healFormula(u.mag, kind)} HP, alcance ${CURES[kind].range}. Toque num aliado ferido.`;
    sfxPlay.ui();
  }

  /** Priest tier 2: a short frontal fire cone, aimed by clicking through a direction like
   * Divine Wrath/Stampede. Friendly fire on purpose — see BURNING_HANDS's own comment. */
  startBurningHands(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "burningHands") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "burningHands";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${BURNING_HANDS.name}: cone curto à frente, ${burningHandsFormula(u.level, u.mag)} contra resistência elemental. Atinge aliados também — mire com cuidado. Toque para mirar.`;
    sfxPlay.ui();
  }

  startPoisonBreath(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "poisonBreath") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "poisonBreath";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${POISON_BREATH.name}: cone curto à frente, ${poisonBreathFormula(u.level, u.mag)} contra resistência elemental; Veneno Menor (1D4 por turno). Atinge aliados também — mire com cuidado. Toque para mirar.`;
    sfxPlay.ui();
  }

  startCureDisease(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.acted || this.tierRemaining(u, "cureDisease") <= 0) return;
    this.mode = "awaitSpell";
    this.spellKind = "cureDisease";
    this.spellArmed = false;
    this.spellAim = null;
    this.hover = null;
    this.tip = `${CURE_DISEASE.name}: cura doença e veneno, alcance ${CURE_DISEASE.range}. Toque num aliado doente.`;
    sfxPlay.ui();
  }

  confirmHeal(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    const cell = this.hover;
    if (!u || this.mode !== "awaitSpell" || !cell || !this.isHeal(this.spellKind)) return;
    this.castHeal(u, cell, this.spellKind);
  }

  private isHeal(kind: SpellKind | null): kind is HealId {
    return kind === "cureMinor" || kind === "cureWounds" || kind === "cureLight";
  }

  private tierRemaining(u: Unit, kind: SpellKind): number {
    if(kind === "frost" && u.side === "enemy")return u.frostCharges ?? 0;
    if (kind === "poisonBreath" && u.level < POISON_BREATH.unlockLevel) return 0;
    if (kind === "burningHands" && u.level < 5) return 0;
    const tier = spellTier(kind);
    return tier ? u.spells[tierKey(tier)] : 0;
  }

  private spendTier(u: Unit, kind: SpellKind): void {
    if(kind === "frost" && u.side === "enemy"){u.frostCharges=Math.max(0,(u.frostCharges??0)-1);return;}
    const tier = spellTier(kind);
    if (!tier) return;
    u.spells[tierKey(tier)] -= 1;
  }

  private longMax(u: Unit): number {
    return LONG_SHOT.range;
  }

  /** True while (x,y) sits inside any still-active Web of Dreams patch. */
  private isWebCell(x: number, y: number): boolean {
    return this.webZones.some((z) => z.cells.has(key(x, y)));
  }

  /** The sleep chance of whichever Dreaming Web zone covers (x,y) — set once at cast time
   * from the caster's level (see castWebOfDreams/webOfDreamsSleepChance) and carried on the
   * zone itself, so a lingering roll always uses the level that created the zone rather than
   * whatever level some other unit is at now. Falls back to the base chance for a zone
   * restored from an older save that predates this field. */
  private webCellSleepChance(x: number, y: number): number {
    const zone = this.webZones.find((z) => z.cells.has(key(x, y)));
    return zone?.sleepChance ?? WEB_OF_DREAMS.sleepChance;
  }

  /** Combined multiplier from every active Aura of Protection / Intimidating Presence zone
   * covering `defender`'s current cell — applied to the final damage of a hit right before it
   * comes off their HP, same insertion point as the sleepBonusDamage multiplier. Protection
   * only discounts a zone's own side; Intimidating Presence only surcharges the other side, so
   * a unit standing in both a friendly and a hostile zone at once takes both at the same time. */
  private zoneDamageMul(defender: Unit): number {
    let mul = 1;
    for (const z of this.auraZones) {
      if (!z.cells.has(key(defender.x, defender.y))) continue;
      if (z.kind === "protection" && z.side === defender.side) mul *= 1 - z.pct;
      if (z.kind === "intimidation" && z.side !== defender.side) mul *= 1 + z.pct;
    }
    return mul;
  }

  /** Every reach computation for a player unit's own turn — including every re-derive free
   * repositioning does after each move — funnels through here.
   *
   * Reach is measured from wherever the unit is actually standing right now, capped by
   * mov - moveBudgetUsed: movement is spent as you walk, cumulatively, exactly like the
   * panel counts it down. A prior version anchored reach at this.turnStart with the full mov
   * instead, meaning moveBudgetUsed measured distance-from-turnStart rather than distance
   * walked — walk 3 hexes out and 3 back and it read 0 again, full budget restored, every
   * cell within mov of the start tile re-selectable indefinitely. That's not a movement cap,
   * it's a teleport with a leash. The real fix for "an exploratory move can strand you" was
   * already sitting right here: canUndoMove/undoMove, a full manual rewind to turnStart for
   * exactly a wrong click — never trade the cap itself away for that.
   *
   * Enemy AI turns never set this.turnStart and don't reposition, so they were never affected
   * by the turnStart-anchoring either way — they've always read straight off their own live
   * x/y, same as here.
   *
   * Web of Dreams' "restrained / difficult terrain" clause — a unit whose current cell was
   * webbed at the START of its turn (this.turnRestrained, decided once in beginUnitTurn, not
   * re-checked live) clamps mov to 1 — applies on top, for both sides. */
  /** Movement this unit has left this turn, off the same cumulative moveBudgetUsed
   * commitMove accumulates — how far it's actually walked. Reach (effectiveUnitForReach)
   * shrinks with it too now, so this and what's selectable always agree. It's also what
   * decides when an already-acted unit's turn auto-ends (see commitMove), and what the panel
   * counts down as the unit walks.
   *
   * The restrained clamp applies to whoever's turn it actually is and nobody else:
   * turnRestrained is decided once, in beginUnitTurn, for the active unit, and says nothing
   * about an enemy the player happens to be inspecting. */
  private movLeft(u: Unit): number {
    const remaining = Math.max(0, u.mov - u.moveBudgetUsed);
    return this.turnRestrained && this.activeTurnUnit()?.id === u.id ? Math.min(remaining, 1) : remaining;
  }

  private effectiveUnitForReach(u: Unit): Unit {
    // A rooted encounter enemy keeps its authored position even after a status
    // effect or a loaded snapshot recalculates its movement stat.
    if (u.side === "enemy" && this.mission.enemySpawns.some((s, i) => s.holdsPosition && u.id === `enemy-${s.name}-${i}`)) return { ...u, mov: 0 };
    // Free roam: any reachable hex is one click away, however far.
    if (this.mission.explore) return { ...u, mov: this.cols * this.rows };
    const remaining = Math.max(0, u.mov - u.moveBudgetUsed);
    const cap = this.turnRestrained ? Math.min(1, remaining) : remaining;
    return cap === u.mov ? u : { ...u, mov: cap };
  }

  /** Whether this mission hides anything at all. */
  get fogged(): boolean {
    return this.mission.fog === true && getDevGfx().fogOfWar;
  }

  /** In sight of the party right now. Always true on a mission without fog. */
  visible(x: number, y: number): boolean {
    if (!this.fogged) return true;
    return this.vis[y * this.cols + x] === 2;
  }

  /** Seen at least once — visible now, or remembered. True everywhere without fog. */
  explored(x: number, y: number): boolean {
    if (!this.fogged) return true;
    return (this.vis[y * this.cols + x] ?? 0) > 0;
  }

  /** Whether a unit is hidden from the player: any cell of its footprint in sight
   * reveals the whole of it, so a big body never half-appears. Public because
   * ThreeBattleRenderer needs this same fog-of-war gate for its own unit meshes. */
  unitHidden(u: Unit): boolean {
    if (!this.fogged || u.side === "player") return false;
    return !footprint(u).some((p) => this.visible(p.x, p.y));
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
  private targetable(u: Unit | undefined): u is Unit {
    if (!u || !u.alive || u.dialog) return false;
    // debugFreeCast drops attackableByPlayer's ally/enemy filter (so a single-target spell
    // can be aimed at your own party too) but the dialog-NPC exclusion above still always
    // applies — a talk-only fixture unit still isn't a sane thing to fireball.
    if (!this.debugFreeCast && !attackableByPlayer(u)) return false;
    return !this.unitHidden(u);
  }

  /** Give Magic Missile's target picker and cast guard the same actionable reason. */
  private magicMissileTargetError(caster: Unit, cell: Point): string | null {
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

  private spellAimError(caster: Unit, cell: Point): string {
    const kind = this.spellKind;
    if (!kind) return "Nenhuma habilidade está mirando agora.";
    if (kind === "magicMissile" || kind === "magicMissileV2") {
      return this.magicMissileTargetError(caster, cell) ?? "Esse alvo não pode ser atingido por esta habilidade.";
    }
    if (kind === "cureMinor" || kind === "cureWounds" || kind === "cureLight") {
      const range = CURES[kind].range;
      const who = this.occ().get(key(cell.x, cell.y));
      if (manhattan(caster, cell) > range) return `Alvo fora de alcance (máximo ${range} hexes).`;
      if (!who) return "Escolha uma aliada na casa selecionada.";
      if (!this.debugFreeCast && who.side !== "player") return "Essa cura só pode ser usada em uma aliada.";
      if (!this.debugFreeCast && who.hp >= who.maxHp) return "Essa aliada está com a vida cheia.";
      return "Essa casa não atende aos requisitos desta cura.";
    }
    if (kind === "cureDisease") {
      const who = this.occ().get(key(cell.x, cell.y));
      if (manhattan(caster, cell) > CURE_DISEASE.range) return `Alvo fora de alcance (máximo ${CURE_DISEASE.range} hexes).`;
      if (!who) return "Escolha uma aliada na casa selecionada.";
      if (!this.debugFreeCast && who.side !== "player") return "A cura de doença só pode ser usada em uma aliada.";
      if (!this.debugFreeCast && !who.diseased && !who.poisoned) return "Essa aliada não está doente nem envenenada.";
      return "Essa casa não atende aos requisitos desta habilidade.";
    }

    const target = this.occ().get(key(cell.x, cell.y));
    const targetRequired = ["longShot", "bloodyShot", "lightning", "lightningTier3", "shock", "phantasmalForce", "multiShot", "doubleStrike", "trip", "lifeDrain", "executionerStrike", "shieldBash", "provoke"].includes(kind);
    if (targetRequired) {
      if (!target) return "Não há unidade na casa selecionada.";
      if (this.unitHidden(target)) return "Escolha um inimigo visível.";
      if (target.dialog) return "Personagens de conversa não podem ser alvos de ataque.";
      if (target.side === caster.side) return "Essa habilidade não pode mirar em uma aliada.";
    }

    const distance = manhattan(caster, cell);
    if (kind === "longShot" || kind === "bloodyShot" || kind === "multiShot") {
      const max = kind === "longShot" ? this.longMax(caster) : kind === "bloodyShot" ? BLOODY_SHOT.range : MULTI_SHOT.range;
      if (distance < caster.minRange) return `Alvo perto demais (alcance mínimo ${caster.minRange} hexes).`;
      if (distance > max) return `Alvo fora de alcance (máximo ${max} hexes).`;
      if (!clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay)) return this.shotBlockedTip(caster, cell, "arrow");
    }
    if (kind === "lightning" || kind === "lightningTier3" || kind === "shock" || kind === "phantasmalForce") {
      const range = kind === "lightning" ? LIGHTNING.range : kind === "lightningTier3" ? LIGHTNING_T3.range : kind === "shock" ? SHOCK.range : PHANTASMAL_FORCE.range;
      if (distance > range) return `Alvo fora de alcance (máximo ${range} hexes).`;
      if (kind === "phantasmalForce" && !clearShot(caster, cell, this.tiles, this.cols, "bolt", this.decorOverlay)) return this.shotBlockedTip(caster, cell, "bolt");
    }
    if (kind === "fireball" || kind === "causticVenom" || kind === "divineBolt" || kind === "minorVenom" || kind === "webOfDreams") {
      const range = kind === "fireball" ? FIREBALL.range : kind === "causticVenom" ? CAUSTIC_VENOM.range : kind === "divineBolt" ? DIVINE_BOLT.range : kind === "minorVenom" ? MINOR_VENOM.range : WEB_OF_DREAMS.range;
      if (distance > range) return `Casa fora de alcance (máximo ${range} hexes).`;
      if (!clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay)) return this.shotBlockedTip(caster, fireballOrigin(cell, this.cols, this.rows), "bolt");
    }
    if (kind === "iceStorm") {
      const range = iceStormPower(caster.level).range;
      if (distance > range) return `Casa fora de alcance (máximo ${range} hexes).`;
      if (!clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay)) return this.shotBlockedTip(caster, fireballOrigin(cell, this.cols, this.rows), "bolt");
    }
    if (kind === "provoke") {
      const range = provokePower(caster.level).range;
      if (hexDist(caster, cell) > range) return `Alvo fora de alcance (máximo ${range} hexes).`;
    }
    if (kind === "cleave" || kind === "shoulderSmash") return "Escolha um hex vizinho ao personagem.";
    if (kind === "sweep") return `Escolha uma casa dentro do raio ${SWEEP.radius}.`;
    if (kind === "piercing" || kind === "piercingThrust" || kind === "burningHands" || kind === "poisonBreath" || kind === "divineWrath" || kind === "stampede") {
      return "Escolha uma linha reta válida dentro do alcance da habilidade.";
    }
    if (kind === "bullRush") return "Não há um caminho livre com espaço para concluir a investida nesse alvo.";
    if (kind === "summonFamiliar" || kind === "summonFamiliar2" || kind === "summonFamiliar3" || kind === "summonFamiliar4" || kind === "summonZombieDog") {
      const range = kind === "summonZombieDog" ? SUMMON_ZOMBIE_DOG.range : kind === "summonFamiliar4" ? SUMMON_FAMILIAR4.range : kind === "summonFamiliar3" ? SUMMON_FAMILIAR3.range : kind === "summonFamiliar2" ? SUMMON_FAMILIAR2.range : SUMMON_FAMILIAR.range;
      if (distance > range) return `Ponto de invocação fora de alcance (máximo ${range} hexes).`;
      if (target) return "O ponto de invocação está ocupado.";
      if (!inBounds(cell.x, cell.y, this.cols, this.rows)) return "O ponto de invocação fica fora do mapa.";
      if (!this.hexAt(cell.x, cell.y).passable) return "O terreno bloqueia a invocação.";
      return "Não há espaço livre para essa criatura nesse ponto.";
    }
    if (["doubleStrike", "trip", "lifeDrain", "executionerStrike", "shieldBash"].includes(kind)) {
      return "Esse inimigo não está ao alcance ou não pode ser atingido daqui.";
    }
    return "Esta casa não atende aos requisitos de alvo da habilidade.";
  }

  /**
   * `attackableEnemies` filtered down to foes the party can actually see.
   *
   * The underlying pass is sight-blind on purpose — it is shared with the enemy AI,
   * which has no business consulting the player's fog. Dropping hidden foes here
   * keeps the attack offers, and the highlights drawn from them, honest without
   * teaching the pathfinder about fog.
   */
  private visibleAttackTargets(unit: Unit): Map<string, Point> {
    const reach = this.reach;
    const all = attackableEnemies(unit, reach, this.units, this.tiles, this.cols, this.decorOverlay);
    if (!this.fogged) return all;
    for (const id of [...all.keys()]) {
      const foe = this.units.find((u) => u.id === id);
      if (!foe || this.unitHidden(foe)) all.delete(id);
    }
    return all;
  }

  /**
   * Recompute sight if the party has moved since the last pass.
   *
   * Cells already marked explored stay explored — fog lifts and never falls back to
   * unseen. The stamp is the party's own layout, so this is a cheap no-op on the
   * frames and turns where nobody walked.
   *
   * Cost is O(party x radius^2), independent of how big the board is: a 160x160
   * dungeon costs exactly what a 20x16 skirmish does.
   */
  private refreshVisibility(): void {
    if (!this.fogged) return;
    const cells = this.cols * this.rows;
    if (this.vis.length !== cells) {
      this.vis = new Uint8Array(cells);
      this.visStamp = "";
    }
    let stamp = `${this.terrainVersion}`;
    for (const u of this.units) {
      if (u.side !== "player" || !u.alive) continue;
      stamp += `|${u.id}:${u.x},${u.y}:${u.visionRange ?? SIGHT_RADIUS}`;
    }
    if (stamp === this.visStamp) return;
    this.visStamp = stamp;
    this.visVersion++;

    // Every cell a living party member stands on is an eye, so a four-hex body sees
    // around its whole bulk rather than from one nominal corner of it.
    const eyes: Point[] = [];
    const radii: number[] = [];
    for (const u of this.units) {
      if (u.side !== "player" || !u.alive) continue;
      for (const p of footprint(u)) {
        eyes.push(p);
        radii.push(u.visionRange ?? SIGHT_RADIUS);
      }
    }
    // Pure vision range, no line of sight: trees and walls blocking the party's view left
    // hexes right beside them dark or black and impossible to reveal. Every hex within a
    // unit's range is fully revealed. (Attacks and enemy AI keep their own sight checks.)
    relight(this.vis, eyes, radii, this.tiles, this.cols, this.rows, this.decorOverlay, false);
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
  private wakeIfSeesParty(foe: Unit): boolean {
    if (!this.fogged) return true;
    if (this.awake.has(foe.id)) return true;
    for (const p of this.units) {
      if (p.side !== "player" || !p.alive) continue;
      if (hexDist(foe, p) > SIGHT_RADIUS) continue;
      if (!sightReaches(foe, p, this.tiles, this.cols, this.decorOverlay)) continue;
      this.awake.add(foe.id);
      return true;
    }
    return false;
  }

  /** Explored cells for the save, or absent on a mission without fog. */
  private snapshotExplored(): string | undefined {
    if (!this.fogged || this.vis.length === 0) return undefined;
    return packExplored(this.vis);
  }

  /** Restore explored cells, falling back to nothing seen when the save carries none
   * or carries a bitset that does not fit this board — see unpackExplored. */
  private restoreExplored(encoded: string | undefined): void {
    const cells = this.cols * this.rows;
    this.visStamp = "";
    this.vis = (this.fogged && encoded ? unpackExplored(encoded, cells) : null) ?? new Uint8Array(cells);
  }

  /**
   * The consolidated properties of one hex: painted terrain with the decoration layer
   * folded in. Every rule in this class goes through here instead of reading `TERRAIN`
   * off `tiles` directly, which is what lets a placement's switches change movement,
   * sight and the high-ground bonus without the board itself being rewritten.
   */
  /** "Tiro bloqueado: <what> no caminho." — names the prop (chests stay generic) or terrain that
   * stops this shot, so the player is never left guessing. */
  private shotBlockedTip(from: Point, to: Point, kind: "arrow" | "bolt"): string {
    const p = shotBlocker(from, to, this.tiles, this.cols, kind, this.decorOverlay);
    if (!p) return "Linha de tiro bloqueada.";
    const deco = this.decorations.find((d) => DECORATIONS[d.id] && placedBlockingFootprint(d).some((f) => d.x + f.dx === p.x && d.y + f.dy === p.y));
    const what = deco
      ? (CHEST_DECOR_IDS.has(deco.id) ? "um baú" : DECORATIONS[deco.id]!.name)
      : this.hexAt(p.x, p.y).height && !this.hexAt(p.x, p.y).blocksShot
        ? `terreno alto (${this.hexAt(p.x, p.y).name})`
        : this.hexAt(p.x, p.y).name;
    return `Tiro bloqueado: ${what} no caminho.`;
  }

  private hexAt(x: number, y: number): TerrainDef {
    return hexDef(this.tiles, this.cols, x, y, this.decorOverlay);
  }

  /** Real elevation for the Three renderer's terrain mesh — the same consolidated
   * high-ground flag (painted `hill` terrain or a decoration's `yieldsHighGround` switch,
   * see hexprops.ts) every combat/LOS rule already reads through hexAt, exposed read-only
   * so terrain geometry can be generated FROM this gameplay data instead of a second,
   * possibly-drifting copy of it. */
  hexElevated(x: number, y: number): boolean {
    return !!this.hexAt(x, y).height;
  }

  /** Refold the decoration switches. Call after anything adds or removes a prop. */
  refreshDecorOverlay(): void {
    this.decorOverlay = buildDecorOverlay(this.decorations, this.cols, this.rows, placedBlockingFootprint, this.mission.terrainElevations);
  }

  /** Who stands where, rebuilt only when the layout actually moved. See `occCache`. */
  private occ(): Map<string, Unit> {
    const n = this.units.length;
    let same = this.occCache !== null && this.occStamp.length === n;
    for (let i = 0; i < n; i++) {
      const u = this.units[i]!;
      // x and y are bounded by MAX_GRID, so this packs without overlap.
      const packed = (u.x * 1024 + u.y) * 2 + (u.alive ? 1 : 0);
      if (this.occStamp[i] !== packed) {
        same = false;
        this.occStamp[i] = packed;
      }
    }
    if (same) return this.occCache!;
    this.occStamp.length = n;
    const units = this.units;
    this.occCache = occupancy(units);
    return this.occCache;
  }

  /**
   * Drop the cache when `this.units` is replaced wholesale rather than mutated.
   * The packed compare only sees positions, so a restore that happens to land every
   * unit on the cell it already held would otherwise keep a map pointing at the
   * previous Unit objects — same coordinates, wrong identities.
   */
  private invalidateOcc(): void {
    this.occCache = null;
    this.occStamp.length = 0;
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
  private playerDistanceFields(players: Unit[]): { p: Unit; field: Map<string, number> }[] {
    let stamp = `${this.terrainVersion}`;
    for (const p of players) stamp += `|${p.id}:${p.x},${p.y}`;
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

  private spellAimValid(caster: Unit, cell: Point): boolean {
    if (!this.spellKind) return false;
    if (this.spellKind === "warp") {
      if (!inBounds(cell.x, cell.y, this.cols, this.rows) || !this.hexAt(cell.x, cell.y).passable) return false;
      return !this.units.some((unit) => unit.alive && occupies(unit, cell.x, cell.y));
    }
    if (this.spellKind === "fireball") {
      if (manhattan(caster, cell) > FIREBALL.range) return false;
      return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if(this.spellKind === "frost")return this.frostTiles(caster,cell).length>0;
    if (this.spellKind === "iceStorm") {
      if (manhattan(caster, cell) > iceStormPower(caster.level).range) return false;
      return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "causticVenom") {
      if (manhattan(caster, cell) > CAUSTIC_VENOM.range) return false;
      return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "divineBolt") {
      if (manhattan(caster, cell) > DIVINE_BOLT.range) return false;
      return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "minorVenom") {
      if (manhattan(caster, cell) > MINOR_VENOM.range) return false;
      return clearShot(caster, fireballOrigin(cell, this.cols, this.rows), this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "longShot") {
      const d = manhattan(caster, cell);
      const here = this.occ().get(key(cell.x, cell.y));
      if (!this.targetable(here) || d < caster.minRange || d > this.longMax(caster)) return false;
      return clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay);
    }
    if (this.spellKind === "bloodyShot") {
      const d = manhattan(caster, cell);
      const here = this.occ().get(key(cell.x, cell.y));
      if (!this.targetable(here) || d < caster.minRange || d > BLOODY_SHOT.range) return false;
      return clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay);
    }
    if (this.spellKind === "piercing") return this.piercingRay(caster, cell) !== null;
    if (this.spellKind === "piercingThrust") return this.piercingThrustRay(caster, cell) !== null;
    if (this.spellKind === "lightning") {
      const here = this.occ().get(key(cell.x, cell.y));
      if (!this.targetable(here) || manhattan(caster, cell) > LIGHTNING.range) return false;
      return true;
    }
    if (this.spellKind === "lightningTier3") {
      const here = this.occ().get(key(cell.x, cell.y));
      if (!this.targetable(here) || manhattan(caster, cell) > LIGHTNING_T3.range) return false;
      return true;
    }
    if (this.spellKind === "shock") {
      const here = this.occ().get(key(cell.x, cell.y));
      if (!this.targetable(here) || manhattan(caster, cell) > SHOCK.range) return false;
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
      if (!this.targetable(here) || manhattan(caster, cell) > PHANTASMAL_FORCE.range) return false;
      return clearShot(caster, cell, this.tiles, this.cols, "bolt", this.decorOverlay);
    }
    if (this.spellKind === "summonFamiliar" || this.spellKind === "summonFamiliar2" || this.spellKind === "summonFamiliar3" || this.spellKind === "summonFamiliar4" || this.spellKind === "summonZombieDog") {
      const range = this.spellKind === "summonZombieDog" ? SUMMON_ZOMBIE_DOG.range : this.spellKind === "summonFamiliar4" ? SUMMON_FAMILIAR4.range : this.spellKind === "summonFamiliar3" ? SUMMON_FAMILIAR3.range : this.spellKind === "summonFamiliar2" ? SUMMON_FAMILIAR2.range : SUMMON_FAMILIAR.range;
      if (manhattan(caster, cell) > range) return false;
      // Familiar 3 is a real multi-hex creature (FOOTPRINT_TYPE_6) — every cell of the shape
      // it would actually occupy has to be checked, not just the anchor tile, or it can be
      // summoned half-overlapping a wall/unit/off-map edge (same class of bug computeReachable
      // was fixed for — see footprintCost's comment in pathfinding.ts).
      const bodyClass = this.spellKind === "summonFamiliar3" ? CLASSES.familiar3! : this.spellKind === "summonZombieDog" ? CLASSES.zombieDog! : null;
      const cells = bodyClass ? footprint({ x: cell.x, y: cell.y, size: bodyClass.size, footprintOffsets: bodyClass.footprintOffsets }) : [cell];
      const occ = this.occ();
      for (const p of cells) {
        if (!inBounds(p.x, p.y, this.cols, this.rows)) return false;
        if (!this.hexAt(p.x, p.y).passable) return false;
        if (occ.get(key(p.x, p.y))) return false;
      }
      return true;
    }
    if (this.spellKind === "webOfDreams") {
      if (manhattan(caster, cell) > WEB_OF_DREAMS.range) return false;
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
      if (!this.targetable(here) || d < caster.minRange || d > MULTI_SHOT.range) return false;
      return clearShot(caster, cell, this.tiles, this.cols, "arrow", this.decorOverlay);
    }
    if (this.spellKind === "divineWrath") return this.wrathRay(caster, cell, DIVINE_WRATH.range) !== null;
    if (this.spellKind === "stampede") return this.wrathRay(caster, cell, STAMPEDE.range) !== null;
    if (this.spellKind === "cureDisease") return this.validCureDiseaseTarget(caster, cell);
    return this.validHealTarget(caster, cell);
  }

  /** Divine Wrath / Stampede: the same directional-line traversal as Piercing (aimed by
   * clicking through a cell to set the direction), just capped to their own range instead of
   * running the length of the board. */
  private wrathRay(caster: Unit, through: Point, range: number): Point[] | null {
    const raw = this.piercingRay(caster, through);
    if (!raw) return null;
    const capped = raw.slice(0, range);
    return capped.length ? capped : null;
  }

  private piercingRay(from: Point, through: Point): Point[] | null {
    const raw = piercingLine(from, through, this.cols, this.rows);
    if (!raw) return null;
    const fromHigh = !!this.hexAt(from.x, from.y).height;
    const out: Point[] = [];
    for (const p of raw) {
      const t = this.hexAt(p.x, p.y);
      if (t.id === "barricade" || t.blocksShot) break;
      if (t.height && !fromHigh) break;
      out.push(p);
    }
    return out.length ? out : null;
  }

  /** Piercing Thrust (Lancer tier 1): the same straight-line traversal as Piercing, capped
   * to the caster's own weapon reach + 1 hex — a short lunge, not an arrow flying the length
   * of the board. */
  private piercingThrustRay(caster: Unit, through: Point): Point[] | null {
    const raw = this.piercingRay(caster, through);
    if (!raw) return null;
    const capped = raw.slice(0, caster.maxRange + 1);
    return capped.length ? capped : null;
  }

  /** Sweep / Shoulder Smash: shoves `foe` one hex further away from `att`, silently doing
   * nothing if that hex is off the board, impassable, or already occupied — a blocked shove
   * just fails, it never displaces someone else instead. Always one hex (SWEEP.knockback),
   * even when the target is two hexes out in Sweep's radius-2 area. */
  private knockBack(att: Unit, foe: Unit): void {
    const neighbors = hexNeighbors(foe.x, foe.y);
    let dest: Point | null = null;
    let best = manhattan(att, foe);
    for (const n of neighbors) {
      if (!inBounds(n.x, n.y, this.cols, this.rows)) continue;
      const d = manhattan(att, n);
      if (d > best) {
        best = d;
        dest = n;
      }
    }
    if (!dest) return;
    if (!this.hexAt(dest.x, dest.y).passable) return;
    if (this.units.some((u) => u.alive && occupies(u, dest.x, dest.y))) return;
    foe.x = dest.x;
    foe.y = dest.y;
    foe.drawX = dest.x;
    foe.drawY = dest.y;
    this.emitParticle({
      x: foe.drawX,
      y: foe.drawY + 0.3,
      vx: 0,
      vy: 0,
      life: 0,
      max: 0.35,
      size: 1,
      color: "#c9b28a",
      kind: "impact",
      frame: 0,
    });
  }

  private validHealTarget(caster: Unit, cell: Point): boolean {
    if (!this.isHeal(this.spellKind)) return false;
    const range = CURES[this.spellKind].range;
    if (manhattan(caster, cell) > range) return false;
    const occ = this.occ();
    const who = occ.get(key(cell.x, cell.y));
    if (!who || !who.alive) return false;
    if (this.debugFreeCast) return true;
    return who.side === "player" && who.hp < who.maxHp;
  }

  private validCureDiseaseTarget(caster: Unit, cell: Point): boolean {
    if (manhattan(caster, cell) > CURE_DISEASE.range) return false;
    const occ = this.occ();
    const who = occ.get(key(cell.x, cell.y));
    if (!who || !who.alive) return false;
    if (this.debugFreeCast) return true;
    return who.side === "player" && (who.diseased || who.poisoned);
  }

  private healRangeTiles(from: Point, range: number): Point[] {
    const out: Point[] = [];
    for (let y = 0; y < this.rows; y++) {
      for (let x = 0; x < this.cols; x++) {
        if (manhattan(from, { x, y }) <= range) out.push({ x, y });
      }
    }
    return out;
  }

  private castHeal(unit: Unit, cell: Point, kind: HealId): void {
    if (!this.validHealTarget(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const target = occ.get(key(cell.x, cell.y));
    if (!target) return;
    this.spendTier(unit, kind);
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "heal", att: unit.id, def: target.id, kind });
  }

  private castCureDisease(unit: Unit, cell: Point): void {
    if (!this.validCureDiseaseTarget(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const target = occ.get(key(cell.x, cell.y));
    if (!target) return;
    this.spendTier(unit, "cureDisease");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "cureDisease", att: unit.id, def: target.id });
  }

  confirmFireball(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    const cell = this.hover;
    if (!u || this.mode !== "awaitSpell" || !cell) return;
    if (manhattan(u, cell) > FIREBALL.range) {
      this.tip = "Fora de alcance.";
      sfxPlay.ui();
      return;
    }
    this.castFireball(u, cell);
  }

  confirmCausticVenom(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    const cell = this.hover;
    if (!u || this.mode !== "awaitSpell" || !cell) return;
    if (manhattan(u, cell) > CAUSTIC_VENOM.range) {
      this.tip = "Fora de alcance.";
      sfxPlay.ui();
      return;
    }
    this.castCausticVenom(u, cell);
  }

  confirmDivineBolt(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    const cell = this.hover;
    if (!u || u.name !== "Salazar" || this.mode !== "awaitSpell" || this.spellKind !== "divineBolt" || !cell) return;
    if (!this.spellAimValid(u, cell)) {
      this.tip = this.spellAimError(u, cell);
      sfxPlay.ui();
      return;
    }
    this.castDivineBolt(u, cell);
  }

  confirmMinorVenom(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    const cell = this.hover;
    if (!u || this.mode !== "awaitSpell" || !cell) return;
    if (manhattan(u, cell) > MINOR_VENOM.range) {
      this.tip = "Fora de alcance.";
      sfxPlay.ui();
      return;
    }
    this.castMinorVenom(u, cell);
  }

  private castLongShot(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) {
      this.tip = "Não há unidade na casa selecionada.";
      sfxPlay.ui();
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

  private castBloodyShot(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const foe = this.occ().get(key(cell.x, cell.y));
    if (!foe) {
      this.tip = "Não há unidade na casa selecionada.";
      sfxPlay.ui();
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

  private castPiercing(unit: Unit, cell: Point): void {
    const line = this.piercingRay(unit, cell);
    if (!line) {
      this.tip = "Escolha uma reta da colmeia.";
      sfxPlay.ui();
      return;
    }
    const ids: string[] = [];
    for (const t of line) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== unit.id && !ids.includes(who.id)) ids.push(who.id);
    }
    this.spendTier(unit, "piercing");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: unit.id, tiles: line, ids, label: PIERCING.name, dmgMul: piercingMul(unit.level), spellKind: "piercing" });
  }

  private castPiercingThrust(unit: Unit, cell: Point): void {
    const line = this.piercingThrustRay(unit, cell);
    if (!line) {
      this.tip = "Escolha uma reta na frente.";
      sfxPlay.ui();
      return;
    }
    const ids: string[] = [];
    for (const t of line) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== unit.id && !ids.includes(who.id)) ids.push(who.id);
    }
    this.spendTier(unit, "piercingThrust");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    if (unit.name !== "Kael") sfxPlay.thrust(this.isBladeAttack(unit));
    this.queue.push({ type: "spell", att: unit.id, tiles: line, ids, label: PIERCING_THRUST.name, spellKind: "piercingThrust" });
  }

  private castShock(unit: Unit, cell: Point): void {
    if (unit.acted || this.familiarSpellRemaining(unit, "shock") <= 0) return;
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const foe = this.occ().get(key(cell.x, cell.y));
    if (!foe) return;
    this.spendFamiliarOrTier(unit, "shock");
    this.spellKind = null;
    this.spellAim = null;
    this.tip = null;
    this.mode = "locked";
    this.queue.push({ type: "spell", att: unit.id, tiles: [cell], ids: [foe.id], dice: SHOCK.dice, faces: SHOCK.faces, bonus: SHOCK.bonus, label: SHOCK.name, echo: { dice: SHOCK.echoDice, faces: SHOCK.echoFaces, bonus: SHOCK.echoBonus }, spellMul: SHOCK.mul, spellKind: "shock" });
  }

  private castLightning(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
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

  private castLightningTier3(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
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
  private castMagicMissile(unit: Unit, cell: Point): void {
    const targetError = this.magicMissileTargetError(unit, cell);
    if (targetError || !this.spellAimValid(unit, cell)) {
      this.tip = targetError ?? this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) {
      this.tip = "Não há inimigo visível nessa casa.";
      sfxPlay.ui();
      return;
    }

    const want = magicMissileCount(unit.level);
    this.missileTargets.push({ id: foe.id, cell: { x: cell.x, y: cell.y } });
    if (this.missileTargets.length < want) {
      const left = want - this.missileTargets.length;
      this.tip = `${MAGIC_MISSILE.name} · escolha mais ${left} alvo${left > 1 ? "s" : ""} (pode repetir).`;
      sfxPlay.ui();
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
  private castMultiShot(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;

    const want = multiShotTargets(unit.level);
    this.missileTargets.push({ id: foe.id, cell: { x: cell.x, y: cell.y } });
    if (this.missileTargets.length < want) {
      const left = want - this.missileTargets.length;
      this.tip = `${MULTI_SHOT.name} · escolha mais ${left} alvo${left > 1 ? "s" : ""} (pode repetir).`;
      sfxPlay.ui();
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

  private castDoubleStrike(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
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

  private castTrip(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
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
  private castLifeDrain(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
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
  private castPhantasmalForce(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const occ = this.occ();
    const foe = occ.get(key(cell.x, cell.y));
    if (!foe) return;
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
  private castSummonFamiliar(unit: Unit, cell: Point, tier: 1 | 2 | 3 | 4 | 5): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
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
      sfxPlay.ui();
      return;
    }
    if (this.hasFamiliarOut(unit, cls.id)) {
      this.spellKind = null;
      this.tip = `${unit.name} já tem ${cls.name} invocado.`;
      sfxPlay.ui();
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
      this.emitPortalFx(cell.x, cell.y, cls.footprintOffsets ?? null, tier === 3);
      sfxPlay.summonFamiliar();
    });
    this.queue.push(step);
  }

  private castWebOfDreams(unit: Unit, click: Point): void {
    if (!this.spellAimValid(unit, click)) {
      this.tip = this.spellAimError(unit, click);
      sfxPlay.ui();
      return;
    }
    const radius = webOfDreamsSize(unit.level);
    const sleepChance = webOfDreamsSleepChance(unit.level);
    const cells = hexAreaTiles(click, radius, this.cols, this.rows);
    const cellKeys = new Set(cells.map((p) => key(p.x, p.y)));
    this.webZones.push({ cells: cellKeys, roundsLeft: WEB_OF_DREAMS.durationRounds, createdAt: this.time, center: { x: click.x, y: click.y }, radius, sleepChance });
    let asleepCount = 0;
    for (const u of this.units) {
      if (!u.alive || !cellKeys.has(key(u.x, u.y))) continue;
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
    this.emitMissileFx(unit.x, unit.y, click.x, click.y, "webOfDreams");
    this.emitParticle({
      x: click.x,
      y: click.y - 0.2,
      vx: 0,
      vy: -0.3,
      life: 0,
      max: 0.5,
      size: 1.4,
      color: "#8c6cd8",
      kind: "impact",
      frame: 0,
    });
    this.tip = `${unit.name} conjurou ${WEB_OF_DREAMS.name}${asleepCount > 0 ? ` — ${asleepCount} adormeceu(ram)` : ""}.`;
    this.pushLog(`${unit.name} conjura ${WEB_OF_DREAMS.name}.`);
    // Web of Dreams is a zone spell with no damage targets, but it still needs the
    // Conjurer's cast motion before the action is completed.
    this.queue.push({ type: "spell", att: unit.id, tiles: [click], ids: [], label: WEB_OF_DREAMS.name, spellKind: "webOfDreams" });
  }

  private castCleave(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const tiles = cleaveHexes(unit, cell, CLEAVE.hexes, this.cols, this.rows);
    if (tiles.length === 0) {
      this.tip = "Toque num hex vizinho.";
      sfxPlay.ui();
      return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== unit.id && who.side !== unit.side && !ids.includes(who.id)) ids.push(who.id);
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
  private castDivineWrath(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const tiles = this.wrathRay(unit, cell, DIVINE_WRATH.range);
    if (!tiles || tiles.length === 0) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== unit.id && who.side !== unit.side && !ids.includes(who.id)) ids.push(who.id);
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
  private castShoulderSmash(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const power = shoulderSmashPower(unit.level);
    const tiles = cleaveHexes(unit, cell, power.hexes, this.cols, this.rows);
    if (tiles.length === 0) {
      this.tip = "Toque num hex vizinho.";
      sfxPlay.ui();
      return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== unit.id && who.side !== unit.side && !ids.includes(who.id)) ids.push(who.id);
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
  private castStampede(unit: Unit, cell: Point): void {
    if (!this.spellAimValid(unit, cell)) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const tiles = this.wrathRay(unit, cell, STAMPEDE.range);
    if (!tiles || tiles.length === 0) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const ids: string[] = [];
    for (const t of tiles) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== unit.id && !ids.includes(who.id)) ids.push(who.id);
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
  private castBurningHands(unit: Unit, cell: Point, kind: "burningHands" | "poisonBreath" = "burningHands"): void {
    const power = (kind === "poisonBreath" ? poisonBreathPower : burningHandsPower)(unit.level);
    const ray = this.wrathRay(unit, cell, power.range);
    const dir = ray && ray[0] ? axisDir(unit, ray[0]) : null;
    if (!dir) {
      this.tip = this.spellAimError(unit, cell);
      sfxPlay.ui();
      return;
    }
    const tiles = coneSector(unit, dir, power.radius, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
      const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (who && who.id !== unit.id && !ids.includes(who.id)) ids.push(who.id);
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

  /** Arms a potion: the next tap on self or an adjacent ally (see confirmPotionAt) applies
   * it and spends the actor's action — same as attacking or casting, never a free extra. */
  usePotion(kind: PotionId): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.side !== "player" || !u.alive || u.acted) return;
    if (this.mode !== "awaitAction" && this.mode !== "selected" && this.mode !== "awaitAttack" && this.mode !== "awaitSpell")
      return;
    if (this.phase !== "player" || this.result) return;
    if (u.bag[kind] <= 0) return;
    this.mode = "awaitPotion";
    this.potionAim = kind;
    this.tip = `${potionLabel(kind)}: toque em você ou num aliado adjacente. Gasta a ação.`;
    sfxPlay.ui();
  }

  /** Whether `cell` is a legal potion target for `actor`: an alive ally on their own hex or
   * one hex away — the "1 de radius" a potion reaches, per direct instruction. */
  private validPotionTarget(actor: Unit, cell: Point): Unit | null {
    const target = this.units.find((x) => x.alive && x.side === "player" && x.x === cell.x && x.y === cell.y);
    if (!target) return null;
    if (target.id === actor.id) return target;
    return hexNeighbors(actor.x, actor.y).some((n) => n.x === cell.x && n.y === cell.y) ? target : null;
  }

  private potionTargetError(actor: Unit, cell: Point): string {
    const occupant = this.units.find((x) => x.alive && occupies(x, cell.x, cell.y));
    if (!occupant) return "Não há aliado nessa casa. Escolha você ou um aliado adjacente.";
    if (occupant.side !== "player") return "Poções só podem ser usadas em você ou em um aliado.";
    if (occupant.id !== actor.id && !hexNeighbors(actor.x, actor.y).some((n) => n.x === cell.x && n.y === cell.y)) {
      return "Esse aliado está longe demais; poções alcançam apenas o próprio personagem ou um aliado adjacente.";
    }
    return "Esse personagem não pode receber esta poção agora.";
  }

  /** The tap that resolves an armed potion (see usePotion/handleCell's awaitPotion branch). */
  private confirmPotionAt(actor: Unit, cell: Point): void {
    const kind = this.potionAim;
    if (!kind) return;
    const target = this.validPotionTarget(actor, cell);
    if (!target) {
      this.tip = this.potionTargetError(actor, cell);
      sfxPlay.ui();
      return;
    }
    this.mode = "awaitAction";
    this.potionAim = null;
    this.applyPotion(actor, target, kind);
  }

  /** The potion's actual effect on `target`, spent from `actor`'s bag and ending their turn
   * — actor and target are the same unit for a self-drink, or actor hands it to an adjacent
   * ally (see confirmPotionAt/validPotionTarget). */
  private applyPotion(actor: Unit, target: Unit, kind: PotionId): void {
    const def = POTIONS[kind];
    if (def.effect === "disease") {
      if (!target.diseased && !target.poisoned) {
        this.tip = `${def.name} · ${target.name} não está doente.`;
        sfxPlay.ui();
        return;
      }
      this.gainSupportAffinity(actor, target);
      actor.bag[kind] -= 1;
      this.curePlayerDisease(target);
      this.trainHealing(actor);
      actor.x = Math.round(actor.drawX);
      actor.y = Math.round(actor.drawY);
      this.tip = `${def.name} · ${target.name} curado(a) da doença.`;
      this.pushLog(`${actor.name} usou ${potionLabel(kind)} em ${target.name} e curou a doença.`);
      this.emitHolyFx(target.x, target.y, "potion", target.id);
      sfxPlay.ui();
      this.bleedOnItemUse(actor);
      this.finishAction(actor);
      return;
    }
    if (def.effect === "mana") {
      const restore = def.manaRestore ?? 0;
      let restored = 0;
      for (let t = 1; t <= 10; t++) {
        const tk = tierKey(t as SpellTier);
        const cap = tierUses(target.classId, t as SpellTier, target.level);
        if (cap <= 0) continue;
        const next = Math.min(cap, target.spells[tk] + restore);
        restored += next - target.spells[tk];
        target.spells[tk] = next;
      }
      if (restored <= 0) {
        this.tip = `${def.name} · magias de ${target.name} já estão no máximo.`;
        sfxPlay.ui();
        return;
      }
      this.gainSupportAffinity(actor, target);
      actor.bag[kind] -= 1;
      actor.x = Math.round(actor.drawX);
      actor.y = Math.round(actor.drawY);
      this.emitParticle({
        x: target.drawX,
        y: target.drawY - 0.35,
        vx: 0,
        vy: -0.18,
        life: 0,
        max: 2,
        size: 1,
        color: "#a08cd8",
        text: `+${restored}`,
        kind: "text",
        frame: 0,
      });
      this.tip = `${def.name} · +${restored} usos de magia (${target.name})`;
      this.pushLog(`${actor.name} usou ${potionLabel(kind)} em ${target.name} e restaurou ${restored} usos de magia.`);
      this.emitHolyFx(target.x, target.y, "potion", target.id);
      sfxPlay.ui();
      this.bleedOnItemUse(actor);
      this.finishAction(actor);
      return;
    }
    if (target.hp >= target.maxHp) {
      this.tip = `${target.name} já está com HP cheio.`;
      sfxPlay.ui();
      return;
    }
    const heal = this.healingPower(actor, rollPotion(kind, this.rng));
    const gained = Math.min(heal, target.maxHp - target.hp);
    target.hp += gained;
    if (gained > 0) { this.gainSupportAffinity(actor, target); this.trainHealing(actor); }
    this.gainExp(actor, target.level, gained);
    actor.bag[kind] -= 1;
    actor.x = Math.round(actor.drawX);
    actor.y = Math.round(actor.drawY);
    this.emitParticle({
      x: target.drawX,
      y: target.drawY - 0.35,
      vx: 0,
      vy: -0.18,
      life: 0,
      max: 2,
      size: 1,
      color: "#d8ead2",
      text: `+${gained}`,
      kind: "text",
      frame: 0,
    });
    this.tip = `${potionLabel(kind)} · +${gained} HP (${target.name})`;
    this.pushLog(`${actor.name} usou ${potionLabel(kind)} em ${target.name} e recuperou ${gained} HP.`);
    this.emitHolyFx(target.x, target.y, "potion", target.id);
    sfxPlay.ui();
    this.bleedOnItemUse(actor);
    this.finishAction(actor);
  }

  /** First adjacent locked chest/door around a unit's own tile, or null if none. A chest is
   * always a decoration (see CHEST_DECOR_IDS) — there is no "chest" terrain anymore — while a
   * door is still real terrain (see the "door" TerrainId). */
  private adjacentLock(u: Unit): Point | null {
    for (const p of hexNeighbors(u.x, u.y)) {
      if (!inBounds(p.x, p.y, this.cols, this.rows)) continue;
      if (tileAt(this.tiles, this.cols, p.x, p.y) === "door") return p;
      if (this.decorations.some(d => (DECORATIONS[d.id]?.model3d === "door" || DECORATIONS[d.id]?.model3d === "secretDoor") && d.x === p.x && d.y === p.y)) return p;
      if (this.decorations.some((d) => CHEST_DECOR_IDS.has(d.id) && d.x === p.x && d.y === p.y)) return p;
    }
    return null;
  }

  /** The strongest level among this battle's own enemy spawns — a per-spawn value (see
   * Mission.enemySpawns[].level, falling back to enemyLevelFor(mission.index) at spawn
   * time), already on the same 1..MAX_LEVEL scale gearPowerLevel runs on. Loot rolls cap
   * to this directly instead of stretching the coarse mission-index curve, so a mission
   * whose enemies are actually weak can't hand out gear built for a much harder one. */
  private highestEnemyLevel(): number {
    let max = 1;
    for (const u of this.units) {
      if (u.side === "enemy" && u.level > max) max = u.level;
    }
    return max;
  }

  /** Removes one found-but-unclaimed weapon/item from this battle's loot list, because it
   * has just been equipped and written into the save directly. Without this the victory
   * fold would credit the same drop a second time. */
  claimLoot(kind: "weapon" | "equipment", id: string): void {
    const list = kind === "weapon" ? this.lootWeapons : this.lootEquipment;
    const i = list.indexOf(id);
    if (i >= 0) list.splice(i, 1);
  }

  /** Applies or refunds one already-authorized permanent level-up point. Authorization (the
   * three-points-per-level budget) belongs to the save/UI; the engine owns the live stat
   * update so the status sheet, damage forecast and any immediately-following action agree. */
  adjustStatPoint(unitId: string, stat: StatPointAttribute, delta: 1 | -1): boolean {
    const u = this.units.find((candidate) => candidate.id === unitId);
    if (!u || u.side !== "player" || !u.alive || this.result) return false;
    const current = u.statPointAllocation[stat] ?? 0;
    if (delta < 0 && current <= 0) return false;
    const previousMaxHp = u.maxHp;
    const next = current + delta;
    if (next > 0) u.statPointAllocation[stat] = next;
    else delete u.statPointAllocation[stat];
    this.reapplyGear(u);
    // A point invested in vitality should be useful immediately; refunding it never leaves
    // current HP above the newly reduced maximum.
    if (stat === "hp" && delta > 0) u.hp = Math.min(u.maxHp, u.hp + (u.maxHp - previousMaxHp));
    this.tip = `${u.name}: ${stat.toUpperCase()} ${delta > 0 ? "+1" : "−1"}.`;
    this.pushLog(this.tip);
    sfxPlay.ui();
    return true;
  }

  /** Feeding is an immediate individual recovery: remove hunger's derived penalty and
   * recompute the live stats so the status panel switches back to Saudável at once. */
  feedUnit(unitId: string): boolean {
    const u = this.units.find((candidate) => candidate.id === unitId);
    if (!u || u.side !== "player" || !u.alive) return false;
    u.fullness = 100;
    u.hungerPenaltyPct = 0;
    this.reapplyGear(u);
    return true;
  }

  /** Recomputes whatever worn gear contributes, after a slot changed mid-battle. Every core
   * stat gearStatBonus returns is applied here, kept in sync with the matching lines in
   * spawnUnit — folded into its own step rather than the equip methods so both entry points
   * stay in sync. */
  private reapplyGear(u: Unit): void {
    const base = statsFor(u.classId, u.level);
    const bonus = gearStatBonus(Object.values(u.gear), u.weaponId, u.classId);
    if (u.side === "player" && !u.summoned) u.weaponSkills = trainedWeaponSkills(this.heroSkills, u.name, u.classId);
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

  /** Swaps a unit's main-hand weapon mid-battle.
   *
   * Changing gear is free and unlimited: it costs neither the turn's action nor its
   * movement, happens in any order around them, and can repeat until the turn is passed.
   * So this deliberately does not check `acted` and never calls finishAction — unlike
   * opening a chest, which does spend the action.
   *
   * Range is a weapon property, so it moves with the weapon; damage is rolled from
   * `weaponId` at attack time and follows on its own. */
  equipWeaponOn(unitId: string, weaponId: string, enh: number): boolean {
    const u = this.units.find((x) => x.id === unitId);
    if (!u || u.side !== "player" || !u.alive) return false;
    if (this.phase !== "player" || this.result) return false;
    if (!weaponId) {
      u.weaponId = null;
      u.weaponEnh = 0;
      u.minRange = CLASSES[u.classId].minRange;
      u.maxRange = CLASSES[u.classId].maxRange;
      this.reapplyGear(u);
      this.tip = `${u.name} guardou a arma.`;
      this.pushLog(this.tip);
      sfxPlay.ui();
      return true;
    }
    const def = WEAPONS[weaponId];
    if (!def || !def.usableBy.includes(u.classId)) return false;

    for (const other of this.units) {
      if (other !== u && other.side === "player" && other.weaponId === weaponId) {
        other.weaponId = null;
        other.weaponEnh = 0;
        other.minRange = 1;
        other.maxRange = 1;
        this.reapplyGear(other);
      }
    }

    u.weaponId = weaponId;
    u.weaponEnh = Math.max(0, Math.min(WEAPON_MAX_ENH, Math.floor(enh)));
    u.minRange = def.minRange;
    u.maxRange = def.maxRange;
    // Two hands on the weapon leaves none for an off-hand item — the same rule spawnUnit
    // applies at the start of a battle, enforced again when the weapon changes mid-fight.
    if (def.twoHanded && u.offHandId) {
      u.gear.offHand = undefined;
      u.offHandId = null;
    }
    this.reapplyGear(u);
    this.tip = `${u.name} equipou ${def.name}${u.weaponEnh > 0 ? ` +${u.weaponEnh}` : ""}.`;
    this.pushLog(this.tip);
    sfxPlay.ui();
    return true;
  }

  /** Swaps one worn equipment slot mid-battle — free, like the main hand above. Passing
   * null empties the slot. */
  equipItemOn(unitId: string, slot: EquipSlot, itemId: string | null): boolean {
    const u = this.units.find((x) => x.id === unitId);
    if (!u || u.side !== "player" || !u.alive) return false;
    if (this.phase !== "player" || this.result) return false;
    const item = itemId ? EQUIPMENT[itemId] : null;
    if (itemId && (!item || !equipmentFitsSlot(item, slot))) return false;
    if (slot === "offHand" && itemId && offHandBlocked(u.weaponId)) return false;

    if (itemId) {
      for (const other of this.units) {
        if (other === u || other.side !== "player") continue;
        for (const [otherSlot, equippedId] of Object.entries(other.gear) as [EquipSlot, string][]) {
          if (equippedId !== itemId) continue;
          delete other.gear[otherSlot];
          if (otherSlot === "offHand") other.offHandId = null;
          this.reapplyGear(other);
          break;
        }
      }
    }

    if (itemId) u.gear[slot] = itemId;
    else delete u.gear[slot];
    if (slot === "offHand") u.offHandId = itemId;
    this.reapplyGear(u);

    this.tip = item ? `${u.name} equipou ${item.name}.` : `${u.name} tirou o item de ${equipmentSlotName(slot)}.`;
    this.pushLog(this.tip);
    sfxPlay.ui();
    return true;
  }

  /** "Arrombar": spends a Gazua to open an adjacent locked chest/door. */
  useLockpick(): void {
    const u = this.units.find((x) => x.id === this.selectedId);
    if (!u || u.side !== "player" || !u.alive) return;
    if (this.mode !== "awaitAction" && this.mode !== "selected" && this.mode !== "awaitAttack" && this.mode !== "awaitSpell")
      return;
    if (this.phase !== "player" || this.result) return;
    if (u.bag.lockpick <= 0) return;
    const target = this.adjacentLock(u);
    if (!target) return;
    const i = target.y * this.cols + target.x;
    const chestDecorId = this.decorations.find((dec) => CHEST_DECOR_IDS.has(dec.id) && dec.x === target.x && dec.y === target.y)?.id;
    const wasChest = !!chestDecorId;
    const architectureDoor = this.decorations.find(dec => (DECORATIONS[dec.id]?.model3d === "door" || DECORATIONS[dec.id]?.model3d === "secretDoor") && dec.x === target.x && dec.y === target.y);
    // A chest never touches the floor underneath it. Doors are terrain, so opening one
    // restores the map's normal walkable floor.
    if (!wasChest && !architectureDoor) this.tiles[i] = this.mission.baseTile ?? "nave";
    if (architectureDoor) {
      const style = DECORATIONS[architectureDoor.id]?.doorStyle ?? "oak";
      architectureDoor.id = THREE_D_DOOR_VARIANTS[style].open;
      architectureDoor.blocksPath = undefined;
      this.refreshDecorOverlay();
    }
    // A chest never touches `tiles` (see DECORATIONS.locked-chest's own comment) — the real
    // floor is already sitting there, so opening it leaves it alone.
    this.terrainVersion++;
    // decorations is readonly (the renderer holds the same array), so drop the chest's
    // decoration in place rather than rebinding the field.
    for (let d = this.decorations.length - 1; d >= 0; d--) {
      const dec = this.decorations[d];
      if (CHEST_DECOR_IDS.has(dec.id) && dec.x === target.x && dec.y === target.y) {
        this.decorations.splice(d, 1);
        this.refreshDecorOverlay();
      }
    }
    u.bag.lockpick -= 1;
    u.x = Math.round(u.drawX);
    u.y = Math.round(u.drawY);
    this.emitParticle({
      x: target.x,
      y: target.y,
      vx: 0,
      vy: -0.2,
      life: 0,
      max: 0.45,
      size: 1,
      color: "#d8b862",
      kind: "impact",
      frame: 0,
    });
    const found: { name: string; icon: string; tip?: string }[] = [];
    if (wasChest) {
      // Every chest gives Ember, a guaranteed potion (weighted so the weak tier is the
      // common case, rarer as potency climbs), and — a separate, independent roll — a
      // chance at a piece of gear, weighted so the strongest is the rarest and capped to
      // what this mission's own enemies are geared for (see highestEnemyLevel). Tier is
      // small/medium/large by decoration id, or bumped to "better" for a small chest listed in
      // Mission.betterChests (gated behind a locked area, say) — same pool and range
      // throughout, just climbing odds and gear-tier headroom.
      const betterSpot = this.mission.betterChests?.some((c) => c.x === target.x && c.y === target.y) ?? false;
      const tier: "base" | "better" | "best" =
        chestDecorId === "chest-large" ? "best" : chestDecorId === "chest-medium" || betterSpot ? "better" : "base";
      const emberBase = tier === "best" ? CHEST_LOOT.bestEmberBase : tier === "better" ? CHEST_LOOT.betterEmberBase : CHEST_LOOT.emberBase;
      const emberDice = tier === "best" ? CHEST_LOOT.bestEmberDice : tier === "better" ? CHEST_LOOT.betterEmberDice : CHEST_LOOT.emberDice;
      const gearChance = tier === "best" ? CHEST_LOOT.bestGearChance : tier === "better" ? CHEST_LOOT.betterGearChance : CHEST_LOOT.gearChance;
      const gearTierMul = tier === "best" ? CHEST_LOOT.bestGearTierMul : tier === "better" ? CHEST_LOOT.betterGearTierMul : CHEST_LOOT.gearTierMul;
      const gain = emberBase + Math.floor(this.rng() * emberDice);
      this.lootEmber += gain;
      const potionKind = weightedPotionPick(this.rng);
      const who = this.givePotion(u, potionKind);
      if (who) {
        const passed = who.id !== u.id ? ` → ${who.name}` : "";
        found.push({
          name: `${POTIONS[potionKind].name}${passed}`,
          icon: `/game/icons/potion-${potionKind}.png?v=ds2`,
          tip: potionTooltip(potionKind),
        });
      } else {
        found.push({
          name: `${POTIONS[potionKind].name} (sem espaço — descartada)`,
          icon: `/game/icons/potion-${potionKind}.png?v=ds2`,
          tip: potionTooltip(potionKind),
        });
      }
      if (this.rng() < gearChance) {
        const gearLevel = Math.max(1, Math.min(MAX_LEVEL, Math.round(this.highestEnemyLevel() * gearTierMul)));
        const drop = weightedLootPick(this.rng, gearLevel, this.ownedWeapons);
        if (drop.kind === "weapon") {
          this.ownedWeapons.add(drop.id);
          this.lootWeapons.push(drop.id);
          found.push({ name: WEAPONS[drop.id]!.name, icon: weaponIcon(drop.id), tip: weaponTooltip(WEAPONS[drop.id]!) });
        } else {
          this.lootEquipment.push(drop.id);
          found.push({ name: EQUIPMENT[drop.id]!.name, icon: equipmentIcon(drop.id), tip: equipmentTooltip(EQUIPMENT[drop.id]!) });
        }
      }
      // A second, independent roll — same rng, same "extra on top of the guaranteed
      // potion" shape as the gear roll just above.
      if (this.rng() < 0.4) {
        const qty = 1 + Math.floor(this.rng() * 4);
        this.lootRations += qty;
        found.push({ name: `Rações ×${qty}`, icon: RATIONS_ICON, tip: "Alimenta o grupo por dias no mapa." });
      }
      const foundNames = found.map((f) => f.name).join(", ");
      this.tip = `${u.name} arrombou o baú · +${gain} Gold · achou ${foundNames}.`;
      this.pushLog(this.tip);
      this.chestLoot = { unitName: u.name, ember: gain, items: found };
    } else {
      this.tip = `${u.name} arrombou a porta.`;
      this.pushLog(this.tip);
    }
    this.bleedOnItemUse(u);
    this.finishAction(u);
    if (wasChest) {
      sfxPlay.chest();
      if (found.length > 0) setTimeout(() => sfxPlay.loot(), 130);
    } else {
      sfxPlay.ui();
    }
  }

  /** "Fim do turno": passes whoever's turn it currently is (same as Esperar). */
  endTurn(): void {
    const active = this.activeTurnUnit();
    if (!active || active.side !== "player" || this.result) return;
    active.moved = true;
    active.x = Math.round(active.drawX);
    active.y = Math.round(active.drawY);
    active.drawX = active.x;
    active.drawY = active.y;
    this.deselect(true);
    sfxPlay.ui();
  }

  /** A random encounter can only be escaped by the hero whose turn it is, once that hero
   * reaches any outer hex of the battlefield. This engine owns the DEX-adjusted roll; the campaign
   * screen handles a successful transition back to the world map. A miss spends this hero's
   * turn, so enemies continue their normal turns and are the only source of ensuing damage. */
  canAttemptFlee(): boolean {
    const u = this.activeTurnUnit();
    return !!u &&
      u.side === "player" &&
      u.alive &&
      !u.moved &&
      !u.summoned &&
      !this.result &&
      !this.active &&
      this.queue.length === 0 &&
      this.phase === "player" &&
      this.mode === "selected" &&
      footprint(u).some((cell) => cell.x <= 0 || cell.y <= 0 || cell.x >= this.cols - 1 || cell.y >= this.rows - 1);
  }

  /** Rolls a DEX-adjusted escape for the active edge-bound hero. Failed attempts deliberately do
   * not inflict scripted damage: they end the hero's turn, letting the encounter's enemies
   * carry on attacking normally before the party can try again. */
  fleeChance(unit: Unit | undefined = this.activeTurnUnit() ?? undefined): number {
    if (!unit) return 0;
    return dexEscapeChance(this.affinityUnit(unit).dex);
  }

  attemptFlee(): boolean {
    if (!this.canAttemptFlee()) return false;
    const u = this.activeTurnUnit()!;
    if (this.rng() * 100 < this.fleeChance(u)) {
      this.tip = `${u.name} encontrou uma saída! O grupo foge do combate.`;
      this.pushLog(this.tip);
      sfxPlay.ui();
      return true;
    }
    u.moved = true;
    u.x = Math.round(u.drawX);
    u.y = Math.round(u.drawY);
    u.drawX = u.x;
    u.drawY = u.y;
    this.deselect(true);
    this.tip = `${u.name} não conseguiu fugir — o combate continua.`;
    this.pushLog(this.tip);
    sfxPlay.ui();
    this.emit();
    return false;
  }

  /** Dispatches control for whoever is next in this round's initiative order. */
  private beginUnitTurn(u: Unit): void {
    // takesTurns keeps neutrals out of the turn order, so whoever reaches here is on one of
    // the two sides that actually take turns.
    this.phase = u.side === "player" ? "player" : "enemy";
    const resumed = this.skipStartOfTurn;
    this.skipStartOfTurn = false;
    if (!resumed) {
      // Flips once per this unit's own turn — see idleAlt's doc comment on Unit. A resumed
      // turn (flee attempt failed, etc.) isn't a new turn, so it doesn't flip again.
      u.idleAlt = !u.idleAlt;
      u.moveBudgetUsed = 0;
      u.bleedMovedThisTurn = false;
      this.startOfTurnEffects(u);
      if (!u.alive) {
        this.activeUnitId = null; // force re-detection next tick, skipping the unit that just died
        return;
      }
      if (u.stunned) {
        u.stunTurns = Math.max(0, u.stunTurns - 1);
        u.stunned = u.stunTurns > 0;
        u.moved = true;
        u.acted = true;
        this.tip = `${u.name} está atordoado(a) — perde o turno.`;
        this.activeUnitId = null; // force re-detection next tick, moving on to whoever's next
        return;
      }
      // Still standing in an active web patch at the start of your own turn means another
      // sleepChance roll every turn you stay put, not just the one at cast — and a success
      // stacks another 1D4 onto whatever sleepTurns you're already carrying (even mid-nap)
      // rather than replacing it, so lingering in the web keeps digging the hole deeper.
      if (this.isWebCell(u.x, u.y) && this.rng() < this.webCellSleepChance(u.x, u.y)) {
        const wasAsleep = u.asleep;
        const extra = rollDice(WEB_OF_DREAMS.sleepDice, WEB_OF_DREAMS.sleepFaces, 0, this.rng);
        u.asleep = true;
        u.sleepTurns += extra;
        this.pushLog(wasAsleep ? `${u.name} afunda mais fundo na teia (+${extra} turnos).` : `${u.name} adormece na teia.`);
      }
      if (u.asleep) {
        u.sleepTurns = Math.max(0, u.sleepTurns - 1);
        u.asleep = u.sleepTurns > 0;
        u.moved = true;
        u.acted = true;
        this.tip = `${u.name} está adormecido(a) — perde o turno.`;
        this.activeUnitId = null; // force re-detection next tick, moving on to whoever's next
        return;
      }
      if ((u.fearTurns ?? 0) > 0) {
        u.fearTurns = Math.max(0, (u.fearTurns ?? 0) - 1);
        const source = this.units.find(x => x.id === u.fearSourceId);
        const threats = source ? [source] : this.units.filter(x => x.alive && x.side !== u.side && x.side !== "neutral");
        this.turnRestrained = this.isWebCell(u.x, u.y);
        const reach = computeReachable(this.effectiveUnitForReach(u), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
        const paths = computeReachable(this.effectiveUnitForReach(u), this.tiles, this.cols, this.rows, this.units, false, this.decorOverlay);
        const distance = (cell: Point) => threats.length ? Math.min(...threats.map(t => hexDist(cell, t))) : 0;
        let destination: Point = u;
        for (const cell of reach.values()) if (distance(cell) > distance(destination)) destination = cell;
        if (destination !== u) {
          const path = reconstructPath(paths, destination);
          if (path.length > 1) this.queue.push({ type: "move", id: u.id, path });
        }
        u.moved = true;
        u.acted = true;
        this.mode = "locked";
        this.tip = `${u.name} flees in fear and cannot attack.`;
        if (this.queue.length === 0) this.activeUnitId = null;
        return;
      }
      // Decided once, off the unit's position right now (the start of its turn) — every
      // reach computation for the rest of this turn (repositioning included) uses this same
      // verdict instead of re-checking, see effectiveUnitForReach.
      this.turnRestrained = this.isWebCell(u.x, u.y);
    } else if (!u.alive) {
      this.activeUnitId = null;
      return;
    }
    if (u.side === "player") {
      if (!resumed) u.acted = false;
      this.selectedId = u.id;
      this.pendingFoeId = null;
      this.inspectedId = null;
      this.orig = { x: u.x, y: u.y };
      this.origMoveBudgetUsed = u.moveBudgetUsed;
      this.turnStart = { x: u.x, y: u.y };
      this.moveSpoiled = resumed;
      this.reach = computeReachable(this.effectiveUnitForReach(u), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
      this.attackFrom = u.acted ? new Map() : this.visibleAttackTargets(u);
      this.threat = [];
      this.mode = "selected";
      this.tip = null;
      this.centerOn(u.x, u.y);
    } else {
      this.mode = "locked";
      // Same camera as a hero's turn (per direct request — enemies acting at the screen edge
      // were barely visible and read as moving too fast): centre on the acting enemy before
      // its queued actions start, and follow its walk (see stepActive's "move" branch).
      this.centerOn(u.x, u.y);
      this.runAiFor(u);
    }
  }

  /** Everyone has had their turn this round — reset and re-roll the initiative order. */
  private startNewRound(): void {
    // Volatile enmity fades round by round (enmity.ts); cumulative enmity stays.
    for (const row of this.enmity.values()) {
      for (const [heroId, entry] of row) row.set(heroId, addToEntry(entry, 0, -ENMITY.volatileDecayPerRound));
    }
    this.mode = "locked";
    this.selectedId = null;
    this.pendingFoeId = null;
    this.inspectedId = null;
    this.reach.clear();
    this.attackFrom.clear();
    this.threat = [];
    for (const u of this.units) {
      u.moved = false;
      u.acted = false;
      if (u.bleedRoundsLeft != null && u.bleedRoundMarker != null && u.bleedRoundMarker < this.turn) {
        u.bleedRoundsLeft = Math.max(0, u.bleedRoundsLeft - 1);
        u.bleedRoundMarker = this.turn;
        if (u.bleedRoundsLeft === 0) {
          u.bleeding = false;
          u.bleedRoundMarker = undefined;
        }
      }
      if ((u.blessedRoundsLeft ?? 0) > 0) {
        u.blessedRoundsLeft = Math.max(0, u.blessedRoundsLeft! - 1);
        if (u.blessedRoundsLeft === 0) u.blessedHitBonusPct = 0;
      }
    }
    for (const z of this.webZones) z.roundsLeft -= 1;
    this.webZones = this.webZones.filter((z) => z.roundsLeft > 0);
    for (const z of this.iceStormZones) z.roundsLeft -= 1;
    this.iceStormZones = this.iceStormZones.filter((z) => z.roundsLeft > 0);
    for (const z of this.auraZones) z.roundsLeft -= 1;
    this.auraZones = this.auraZones.filter((z) => z.roundsLeft > 0);
    for (const gate of this.warpGates) gate.roundsLeft -= 1;
    const closed = this.warpGates.filter((gate) => gate.roundsLeft <= 0);
    if (closed.length) {
      this.warpGates = this.warpGates.filter((gate) => gate.roundsLeft > 0);
      const stillOpen = (fx: PortalFx) => this.warpGates.some((g) => (g.a.x === fx.x && g.a.y === fx.y) || (g.b.x === fx.x && g.b.y === fx.y));
      for (const fx of this.portalFx) {
        if (fx.persistent && !stillOpen(fx)) fx.live = false;
      }
      this.portalFxLive = this.portalFx.filter((fx) => fx.live).length;
      this.tip = closed.length > 1 ? "Portais de Warp se fecharam." : "Um par de portais de Warp se fechou.";
    }
    // Neutrals are left out, so they never get a turn and the AI never runs for them. One
    // provoked mid-round isn't in this round's order either: it wakes up and acts from the
    // next round, which reads as the beast rousing rather than instantly retaliating.
    this.turnOrder = this.sortByInitiative(this.units.filter(takesTurns));
    this.turn += 1;
    this.activeUnitId = null;
  }

  private applyBless(target: Unit, casterLevel: number): void {
    const pct = BLESS.hitBonusPct(casterLevel) / 100;
    target.blessedHitBonusPct = Math.max(target.blessedHitBonusPct ?? 0, pct);
    const duration = BLESS.durationRounds(casterLevel);
    target.blessedRoundsLeft = duration;
    target.healGlow = 1;
    target.healGlowKind = "bless";
    this.pushLog(`${target.name} recebeu Bless: +${Math.round(pct * 100)}% de acerto por ${duration} rodadas.`);
  }

  /** Enemy Choque: same ignore-cover targeting as Relâmpago, ~1/3 the stats. Returns true
   * if a cast was queued so the caller can skip the rest of the AI. */
  private tryAiShock(
    next: Unit,
    reach: ReturnType<typeof computeReachable>,
    walkReach: ReturnType<typeof computeReachable>,
    players: Unit[],
  ): boolean {
    if (next.shockCharges <= 0) return false;
    let best: { foe: Unit; from: Point; score: number } | null = null;
    for (const cell of reach.values()) {
      for (const foe of players) {
        if (manhattan(cell, foe) > SHOCK.range) continue;
        const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
        if (!best || score > best.score) best = { foe, from: { x: cell.x, y: cell.y }, score };
      }
    }
    if (!best) return false;
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

  private runAiFor(next: Unit): void {
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
    const reach = computeReachable(this.effectiveUnitForReach(next), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
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
    const walkReach = computeReachable(this.effectiveUnitForReach(next), this.tiles, this.cols, this.rows, this.units, false, this.decorOverlay);
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
        let bestBolt: { foe: Unit; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const foe of players) {
            if (manhattan(cell, foe) > LIGHTNING.range) continue;
            const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
            if (!bestBolt || score > bestBolt.score) bestBolt = { foe, from: { x: cell.x, y: cell.y }, score };
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
        let bestForce: { foe: Unit; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const foe of players) {
            if (manhattan(cell, foe) > FANTOM_FORCE.range) continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay)) continue;
            const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
            if (!bestForce || score > bestForce.score) bestForce = { foe, from: { x: cell.x, y: cell.y }, score };
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
      if (this.tryAiShock(next, reach, walkReach, players)) return;
      if (next.spells.tier1 > 0) {
        let bestSpell: { foe: Unit; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const foe of players) {
            if (manhattan(cell, foe) > MAGIC_MISSILE.range) continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay)) continue;
            const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
            if (!bestSpell || score > bestSpell.score) bestSpell = { foe, from: { x: cell.x, y: cell.y }, score };
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
    if(next.classId==="cultistV2"&&(next.frostCharges??0)>0){
      let best:{from:Point;target:Point;score:number}|null=null;
      for(const from of [next])for(const foe of players){const cells=this.frostTiles(from,foe,next.level);const hits=players.filter(p=>cells.some(c=>occupies(p,c.x,c.y))).length;if(!hits)continue;
        const allies=this.units.filter(u=>u.alive&&u.side===next.side&&u.id!==next.id&&cells.some(c=>occupies(u,c.x,c.y))).length;
        const score=hits*10-allies*12-hexDist(next,from);if(!best||score>best.score)best={from:{x:from.x,y:from.y},target:{x:foe.x,y:foe.y},score};
      }
      if(best&&best.score>0){if(best.from.x!==next.x||best.from.y!==next.y)this.queue.push({type:"move",id:next.id,path:reconstructPath(walkReach,best.from)});this.queueFrost(next,best.from,best.target);this.queue.push({type:"delay",dur:.12});return;}
    }
    // Undead Ox — its tier4 Veneno Menor (2 per battle), then plain melee.
    if ((next.classId === "undeadOx" || next.classId === "plagueBearingCattle") && this.tryAiMinorVenom(next, reach, walkReach, players)) return;

    // Carnivorous Plant — more than one character in reach of her tendril swipe: swipe instead
    // of casting. Otherwise Veneno Cáustico, then Poison Breath, then Veneno Menor, then plain melee.
    if (next.classId === "carnivorousPlant") {
      if (this.tryAiPlantSwipe(next, reach, walkReach)) return;
      if (this.tryAiPlantCausticVenom(next, reach, walkReach, players)) return;
      if (this.tryAiPlantPoisonBreath(next, reach, walkReach)) return;
      if (this.tryAiMinorVenom(next, reach, walkReach, players)) return;
    }

    // Sapling — its 1 Poison Breath (same cone targeting as the Carnivorous Plant's), then melee.
    if (next.classId === "sapling" && this.tryAiPlantPoisonBreath(next, reach, walkReach)) return;

    // Birolho (and Birolho2) — Relâmpago outranks Caustic Venom outranks Choque outranks Magic Missile.
    if ((next.classId === "birolho" || next.classId === "birolho2" || next.classId === "birolho3" || next.classId === "birolhoLegs" || next.classId === "birolhoLegs2") && (next.spells.tier1 > 0 || next.spells.tier2 > 0 || next.spells.tier4 > 0 || next.shockCharges > 0)) {
      if (next.spells.tier2 > 0) {
        let bestBolt: { foe: Unit; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const foe of players) {
            if (manhattan(cell, foe) > LIGHTNING.range) continue;
            const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
            if (!bestBolt || score > bestBolt.score) bestBolt = { foe, from: { x: cell.x, y: cell.y }, score };
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
        let bestVenom: { at: Point; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const foe of players) {
            if (manhattan(cell, foe) > CAUSTIC_VENOM.range) continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay)) continue;
            const splash = hexAreaTiles({ x: foe.x, y: foe.y }, CAUSTIC_VENOM.size, this.cols, this.rows);
            let hits = 0;
            let score = 0;
            for (const t of splash) {
              const hit = players.find((p) => p.x === t.x && p.y === t.y);
              if (!hit) continue;
              hits += 1;
              score += (hit.maxHp - hit.hp) + (hit.hp <= 8 ? 15 : 0);
            }
            if (hits === 0) continue;
            score += hits * 10;
            if (!bestVenom || score > bestVenom.score) bestVenom = { at: { x: foe.x, y: foe.y }, from: { x: cell.x, y: cell.y }, score };
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
            if (u && !ids.includes(u.id)) ids.push(u.id);
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
      if (this.tryAiShock(next, reach, walkReach, players)) return;
      if (next.spells.tier1 > 0) {
        let bestBolt: { foe: Unit; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const foe of players) {
            if (manhattan(cell, foe) > MAGIC_MISSILE.range) continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay)) continue;
            const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
            if (!bestBolt || score > bestBolt.score) bestBolt = { foe, from: { x: cell.x, y: cell.y }, score };
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
        let bestCone: { tiles: Point[]; ids: string[]; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const dir of CUBE_DIRS) {
            const tiles = coneSector(cell, dir, power.radius, this.cols, this.rows);
            const ids: string[] = [];
            let score = 0;
            let burnsAlly = false;
            for (const t of tiles) {
              const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
              if (!who || who.id === next.id || ids.includes(who.id)) continue;
              if (who.side !== "player") burnsAlly = true;
              ids.push(who.id);
              score += 10 + (who.maxHp - who.hp) * 3 + (who.hp <= 8 ? 20 : 0);
            }
            if (burnsAlly || !ids.length) continue;
            if (!bestCone || score > bestCone.score) bestCone = { tiles, ids, from: { x: cell.x, y: cell.y }, score };
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
        let bestSpell: { foe: Unit; from: Point; score: number } | null = null;
        for (const cell of reach.values()) {
          for (const foe of players) {
            if (manhattan(cell, foe) > MAGIC_MISSILE.range) continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay)) continue;
            const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
            if (!bestSpell || score > bestSpell.score) bestSpell = { foe, from: { x: cell.x, y: cell.y }, score };
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
      if (this.tryAiShock(next, reach, walkReach, players)) return;
    }

    // Any other enemy mage (a player-class mage spawned as a foe, promoted casters, etc.)
    // still gets Choque even if they don't share the cultist/birolho AI branches.
    if (
      next.side === "enemy" &&
      next.shockCharges > 0 &&
      next.classId !== "cultist" &&
      next.classId !== "cultistV2" &&
      next.classId !== "birolho" &&
      next.classId !== "birolho2" &&
      next.classId !== "birolho3" &&
      next.classId !== "birolhoLegs" &&
      next.classId !== "birolhoLegs2" &&
      this.tryAiShock(next, reach, walkReach, players)
    ) {
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
      let bestSpell: { foe: Unit; from: Point; score: number } | null = null;
      for (const cell of reach.values()) {
        for (const foe of players) {
          if (spellKind === "longShot") {
            const d = manhattan(cell, foe);
            if (d < next.minRange || d > longMax) continue;
            if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "arrow", this.decorOverlay)) continue;
          } else {
            const line = this.piercingRay({ x: cell.x, y: cell.y }, { x: foe.x, y: foe.y });
            if (!line || !line.some((p) => p.x === foe.x && p.y === foe.y)) continue;
          }
          const score = (foe.maxHp - foe.hp) * 3 + (foe.hp <= 8 ? 20 : 0);
          if (!bestSpell || score > bestSpell.score) bestSpell = { foe, from: { x: cell.x, y: cell.y }, score };
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
        } else {
          const line = this.piercingRay(bestSpell.from, { x: bestSpell.foe.x, y: bestSpell.foe.y })!;
          const ids: string[] = [];
          for (const t of line) {
            const who = this.units.find((u) => u.alive && occupies(u, t.x, t.y));
            if (who && who.id !== next.id && !ids.includes(who.id)) ids.push(who.id);
          }
          this.queue.push({ type: "spell", att: next.id, tiles: line, ids, label: PIERCING.name, dmgMul: piercingMul(next.level), spellKind: "piercing" });
        }
        this.queue.push({ type: "delay", dur: 0.12 });
        return;
      }
    }

    let best: { foe: Unit; from: Point; score: number } | null = null;
    for (const cell of reach.values()) {
      for (const foe of players) {
        if (!canHitFrom(next, cell, foe, this.tiles, this.cols, this.decorOverlay)) continue;
        const terr = this.hexAt(cell.x, cell.y);
        const score = (foe.maxHp - foe.hp) * 3 + terr.def * 2 + (foe.hp <= 8 ? 20 : 0);
        if (!best || score > best.score) best = { foe, from: { x: cell.x, y: cell.y }, score };
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
      if (dCur < dBest) nearest = f;
    }
    // Human-sized hostiles recognize either active Warp opening as a route to pursue the
    // party through. Their ordinary pursuit resumes after crossing, one turn at a time.
    if (this.warpGates.length && next.side !== "player" && footprint(next).length === 1) {
      let best = Infinity;
      for (const gate of this.warpGates) for (const opening of [gate.a, gate.b]) {
        const field = terrainDistanceField(opening, this.tiles, this.cols, this.rows, this.decorOverlay);
        const d = field.get(key(next.x, next.y)) ?? Infinity;
        if (d < best) { best = d; nearest = { p: next, field }; }
      }
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

  pointerMove(cssX: number, cssY: number): void {
    // An aimed skill waits for Confirmar: confirmSpell casts at `hover`, so moving the mouse
    // toward the button must not drag the aim along. Clicking another cell re-aims.
    if (this.mode === "awaitSpell" && this.spellArmed && this.spellAim) return;
    const cell = this.cellAt(cssX, cssY);
    this.hover = cell;
  }

  /** Inspect a unit under the mouse without changing the selected unit or action mode. */
  inspectAt(cssX: number, cssY: number): string | null {
    if (this.result) return null;
    const cell = this.cellAt(cssX, cssY);
    if (!cell) return null;
    const unit = this.occ().get(key(cell.x, cell.y));
    if (!unit?.alive) return null;
    this.inspect(unit);
    return unit.id;
  }

  pointerDown(cssX: number, cssY: number, via: "click" | "tap" = "click"): void {
    if (this.result || this.mode === "locked") return;
    const cell = this.cellAt(cssX, cssY);
    if (!cell) {
      if (this.mode === "selected" || this.mode === "awaitAction") this.deselect();
      return;
    }
    const now = typeof performance !== "undefined" ? performance.now() : Date.now();
    const selected = this.units.find((u) => u.id === this.selectedId);
    const same =
      this.lastClickCell && this.lastClickCell.x === cell.x && this.lastClickCell.y === cell.y && now - this.lastClickAt < 340;
    this.lastClickAt = now;
    this.lastClickCell = cell;
    if (same && (this.mode === "awaitAction" || this.mode === "selected") && selected && occupies(selected, cell.x, cell.y)) {
      this.wait();
      // turnOrder interleaves both sides by initiative, so whoever's up next here is just as
      // often an enemy as it is an ally. select() falls through to inspect() for anyone who
      // isn't a controllable player unit — right for an explicit click on a foe, wrong here:
      // ending your own turn should never pop somebody else's status sheet open as a side
      // effect. Only ever auto-select the next unit when it's actually a player unit whose
      // turn it now is; an enemy up next is left alone for the engine's own turn dispatcher.
      const next = this.activeTurnUnit();
      if (next && next.side === "player") this.select(next);
      return;
    }
    this.cursor = cell;
    this.ensureVisible(cell.x, cell.y);
    this.handleCell(cell, via);
  }

  keyDown(code: string): void {
    if (this.result || this.mode === "locked") {
      if (code === "KeyE") this.endTurn();
      return;
    }
    if (code === "Enter" || code === "Space") this.handleCell(this.cursor, "click");
    if (code === "Escape") this.cancel();
    if (code === "KeyE") this.endTurn();
    if (code === "KeyZ") this.wait();
  }

  private handleCell(cell: Point, via: "click" | "tap" = "click"): void {
    const occ = this.occ();
    const here = occ.get(key(cell.x, cell.y));
    const selected = this.units.find((u) => u.id === this.selectedId);
    // A staged attack (see stageAttack) is dropped by clicking anywhere but an enemy; clicking
    // another enemy re-targets it below.
    if (this.pendingFoeId && !this.targetable(here)) this.pendingFoeId = null;

    if (this.mode === "awaitPotion" && selected) {
      this.hover = cell;
      this.confirmPotionAt(selected, cell);
      return;
    }

    if (this.mode === "awaitSpell" && selected) {
      if (this.spellKind === "turnUndead") {
        this.tip = "The area is centered on the priest. Confirm to cast or cancel to keep the turn.";
        return;
      }
      if (this.spellKind === "sweep") {
        if (manhattan(selected, cell) <= SWEEP.radius) this.confirmSweep();
        else {
          this.tip = "A área já está marcada — Lançar para confirmar.";
          sfxPlay.ui();
        }
        return;
      }
      this.hover = cell;
      if (!this.spellAimValid(selected, cell)) {
        this.tip = this.spellAimError(selected, cell);
        this.spellArmed = false;
        sfxPlay.ui();
        return;
      }
      // Multi-target skills collect one target per pick; only the pick that casts asks to confirm.
      const picks = this.spellKind === "magicMissile" ? magicMissileCount(selected.level) : this.spellKind === "multiShot" ? multiShotTargets(selected.level) : 1;
      if (this.missileTargets.length + 1 < picks) {
        this.confirmSpell();
        return;
      }
      // Every click (mouse or tap) first aims the skill; the HUD then asks Confirmar/Cancelar
      // (see spellArmed). Clicking the same aimed cell again still casts, as a tap always did.
      if (!this.spellArmed || !this.spellAim || this.spellAim.x !== cell.x || this.spellAim.y !== cell.y) {
        this.spellArmed = true;
        this.spellAim = cell;
        this.tip = null;
        sfxPlay.ui();
        return;
      }
      this.confirmSpell();
      return;
    }

    if (here && here.side === "player" && here.alive && this.phase === "player") {
      // A left click keeps control with the selected unit. Use right-click to inspect another.
      if (selected && here.id !== selected.id && !this.mission.explore) return;
      if (selected && this.mode === "awaitAction") {
        if (here.id === selected.id) return;
        this.deselect();
      }
      this.select(here);
      return;
    }
    if (here?.dialog && here.alive) {
      const dialogSpawn = this.mission.neutralSpawns?.find((spawn) => spawn.name === here.name && spawn.x === here.x && spawn.y === here.y);
      const lockedDoor = dialogSpawn?.dialogRequiresOpenDoor;
      if (lockedDoor && tileAt(this.tiles, this.cols, lockedDoor.x, lockedDoor.y) === "door") {
        this.tip = "A cela está trancada. Use uma gazua para abrir a porta.";
        this.pushLog(this.tip);
        sfxPlay.ui();
        return;
      }
      // Free roam: walk up to the NPC first, then talk. Already beside them (or nowhere
      // free to stand) just talks from where the leader is.
      if (this.mission.explore && selected && this.mode === "selected" && !hexNeighbors(here.x, here.y).some((n) => n.x === selected.x && n.y === selected.y)) {
        let best: Point | null = null;
        let bestCost = Infinity;
        for (const n of hexNeighbors(here.x, here.y)) {
          const cost = this.reach.get(key(n.x, n.y))?.cost;
          if (cost !== undefined && cost < bestCost && !this.units.some((u) => u.alive && u.x === n.x && u.y === n.y)) {
            best = n;
            bestCost = cost;
          }
        }
        if (best) {
          const tree = here.dialog;
          this.commitMove(selected, best, () => this.openDialog(tree));
          return;
        }
      }
      this.openDialog(here.dialog);
      return;
    }
    if (this.targetable(here)) {
      if (selected && !selected.acted && this.mode === "awaitOffHand") {
        // Same reach as commitOffHandAction: a dagger/katar reaches by its own range, not the
        // main-hand bow's (an archer's bow can't hit adjacent, so using it here made every
        // adjacent target read "Fora de alcance").
        const offHandReach = this.offHandReach(selected);
        if (canHitFrom(offHandReach, selected, here, this.tiles, this.cols, this.decorOverlay)) {
          this.stageAttack(here, true);
          return;
        }
        this.tip = "Fora de alcance.";
        sfxPlay.ui();
        return;
      }
      // A left click on an enemy performs the active attack or action only.
      // A primary click on a valid enemy is the basic attack action. The Atacar button
      // still arms targeting explicitly, while right-click is reserved for inspection.
      if (selected && !selected.acted && (this.mode === "awaitAttack" || this.mode === "awaitAction" || this.mode === "selected")) {
        // A bow user with an off-hand dagger/katar strikes with it by default whenever the foe is
        // within its reach from where they stand — it always outhits the arrow.
        const offHandItem = selected.offHandId ? EQUIPMENT[selected.offHandId] : null;
        if (offHandItem?.kind === "weapon" && this.isArrowAttack(selected) && canHitFrom(this.offHandReach(selected), selected, here, this.tiles, this.cols, this.decorOverlay)) {
          this.stageAttack(here, true);
          return;
        }
        // attackFrom stores the best reachable hex for each visible foe. Using only the
        // unit's current hex let ranged units fire but made melee attacks appear dead
        // unless the player first moved adjacent by hand.
        if (this.attackFrom.get(here.id) || canHitFrom(selected, selected, here, this.tiles, this.cols, this.decorOverlay)) {
          this.stageAttack(here, false);
          return;
        }
        if (shotKind(selected) && inWeaponRange(selected.x, selected.y, here.x, here.y, selected.minRange, effectiveMaxRange(selected, tileAt(this.tiles, this.cols, selected.x, selected.y)))) {
          this.tip = this.shotBlockedTip(selected, here, shotKind(selected)!);
          sfxPlay.ui();
          return;
        }
      }
      return;
    }
    if (selected && this.mode === "selected") {
      if (this.reach.has(key(cell.x, cell.y)) && !here) {
        this.commitMove(selected, cell);
        return;
      }
    }
    if (selected && this.mode === "awaitAction" && !here) {
      this.deselect();
    }
    if (!here && this.inspectedId && !selected) {
      this.inspectedId = null;
      this.threat = [];
      this.tip = null;
    }
  }

  private commitMove(unit: Unit, to: Point, after?: () => void): void {
    // this.reach is anchored at the unit's live position (see effectiveUnitForReach) — right
    // for validating `to` and reading its cost, but it's still the default pruneStopPoints
    // pass, which deletes any cell along the way that isn't itself a legal place to stop
    // (e.g. one an ally occupies), leaving a dangling parent reference that would silently
    // truncate reconstructPath before it reaches `to`. The walk only needs SOME valid route
    // through, so it gets its own unpruned pass off the same anchor.
    const walkReach = computeReachable(this.effectiveUnitForReach(unit), this.tiles, this.cols, this.rows, this.units, false, this.decorOverlay);
    const path = reconstructPath(walkReach, to);
    // Never substitute a straight walk when the destination has become blocked.
    if (path.length < 2 || path[0].x !== unit.x || path[0].y !== unit.y) return;
    // Cost of THIS hop, from wherever the unit currently stands — this.reach is anchored
    // there too, so this is already a per-hop delta, not a cumulative total.
    const stepCost = this.reach.get(key(to.x, to.y))?.cost ?? 0;
    this.mode = "locked";
    this.queue.push({ type: "move", id: unit.id, path });
    this.queue.push({ type: "delay", dur: 0.02 });
    this.warpedThisMove.delete(unit.id);
    this.onNextIdle = () => {
      // A Warp gate on the way carried the unit across mid-walk: it stays where it landed.
      // Snapping it to the clicked hex put it straight back on the near side (back and forth).
      if (!this.warpedThisMove.delete(unit.id)) {
        unit.x = Math.round(to.x);
        unit.y = Math.round(to.y);
      }
      unit.drawX = unit.x;
      unit.drawY = unit.y;
      // Accumulates: movement is spent as the unit walks, hop by hop, never refunded by a
      // later move — only undoMove (a full rewind to turnStart) reverts spent movement.
      unit.moveBudgetUsed += stepCost;
      // Having acted doesn't make this move the last one — it comes out of the same pool as
      // any other. The turn ends when the pool runs dry (with the action already spent),
      // never merely because the unit acted first.
      if (unit.acted && unit.mov - unit.moveBudgetUsed <= 0) {
        // Keep the actor selected with no reach so the player can undo this post-action
        // movement before explicitly passing the turn. auto-passing here removed any
        // chance to cancel the move that spent the final movement point.
        this.selectedId = unit.id;
        this.reach.clear();
        this.attackFrom.clear();
        this.mode = "selected";
        return;
      }
      // Not acted yet — movement isn't a one-shot: the unit stays "selected" with a fresh
      // reach from its new spot, so the player can keep repositioning freely until they
      // either use a skill (see the u.acted branch above, unchanged) or end the turn.
      this.selectedId = unit.id;
      this.mode = "selected";
      this.reach = computeReachable(this.effectiveUnitForReach(unit), this.tiles, this.cols, this.rows, this.units, true, this.decorOverlay);
      this.attackFrom = this.visibleAttackTargets(unit);
      after?.();
    };
  }

  /** Clicking an enemy no longer attacks outright: it stages the attack on that foe, and the
   * HUD shows its forecast (hit chance, damage, counter) with Confirmar/Cancelar —
   * see confirmPendingAttack / cancelPendingAttack. `offHand`: the click resolved to the
   * off-hand strike (dagger/katar/shield) rather than the main attack. */
  private stageAttack(foe: Unit, offHand: boolean): void {
    this.pendingFoeId = foe.id;
    this.pendingAttackOffHand = offHand;
    this.tip = null;
    sfxPlay.ui();
  }

  /** Confirmar on the staged attack: runs exactly what the click used to run. */
  confirmPendingAttack(): void {
    const selected = this.units.find((u) => u.id === this.selectedId);
    const foe = this.units.find((u) => u.id === this.pendingFoeId);
    this.pendingFoeId = null;
    if (!selected || !foe || !foe.alive || selected.acted || this.phase !== "player") return;
    if (this.pendingAttackOffHand) {
      this.commitOffHandAction(selected, foe, { x: selected.x, y: selected.y });
      return;
    }
    const from = this.attackFrom.get(foe.id);
    this.commitAttack(selected, foe, from ?? { x: selected.x, y: selected.y });
  }

  /** Cancelar on an aimed skill's confirm panel: the skill is put away and the unit is back to
   * choosing — unlike cancel(), its movement this turn is kept, not rewound. */
  cancelSkillConfirm(): void {
    if (this.mode !== "awaitSpell") return;
    const u = this.units.find((x) => x.id === this.selectedId);
    this.spellArmed = false;
    this.spellAim = null;
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    if (u) this.returnToSelected(u);
    else this.deselect();
    sfxPlay.ui();
  }

  /** Cancelar on the staged attack: nothing happens, the unit keeps its turn. */
  cancelPendingAttack(): void {
    if (!this.pendingFoeId) return;
    this.pendingFoeId = null;
    sfxPlay.ui();
  }

  private commitAttack(unit: Unit, foe: Unit, from: Point): void {
    const at = { x: Math.round(from.x), y: Math.round(from.y) };
    if (!canHitFrom(unit, at, foe, this.tiles, this.cols, this.decorOverlay)) {
      this.mode = "awaitAction";
      this.tip = "Fora de alcance.";
      return;
    }
    this.mode = "locked";
    if (at.x !== unit.x || at.y !== unit.y) {
      // Unpruned pass, same reason as commitMove: this.reach drops pass-through cells (an ally's
      // hex), which broke the path back to the unit — no move got queued and the attack landed
      // from where the unit already stood, out of its real range.
      const walkReach = computeReachable(this.effectiveUnitForReach(unit), this.tiles, this.cols, this.rows, this.units, false, this.decorOverlay);
      const path = reconstructPath(walkReach, at);
      if (path.length > 1) this.queue.push({ type: "move", id: unit.id, path });
    }
    this.queue.push({ type: "combat", att: unit.id, def: foe.id });
  }

  /** Off-hand attack (a light weapon in the offHand slot) or Shield Bash (a shield
   * there) — whichever EQUIPMENT[unit.offHandId].kind resolves to. Reuses the same
   * "already in range from here" check as a normal Atacar; no move-then-act chaining. */
  private commitOffHandAction(unit: Unit, foe: Unit, from: Point): void {
    const item = unit.offHandId ? EQUIPMENT[unit.offHandId] : null;
    // A dagger/katar reaches only as far as the off-hand weapon itself, not the main bow.
    const reach = this.offHandReach(unit);
    if (!item || !canHitFrom(reach, from, foe, this.tiles, this.cols, this.decorOverlay)) {
      this.mode = "awaitAction";
      this.tip = "Fora de alcance.";
      return;
    }
    this.mode = "locked";
    if (item.kind === "shield") {
      this.queue.push({ type: "combat", att: unit.id, def: foe.id, dmgMul: item.dmgMul ?? 0.75, stunChance: 0.7, spellKind: "shieldBash" });
    } else {
      this.queue.push({
        type: "combat",
        att: unit.id,
        def: foe.id,
        customDice: { dice: item.dice ?? 1, faces: item.faces ?? 4, bonus: item.bonus ?? 0 },
      });
    }
  }

  private castIceStorm(unit: Unit, click: Point): void {
    if (!this.spellAimValid(unit, click)) {
      this.tip = this.spellAimError(unit, click);
      sfxPlay.ui();
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
    this.pushLog(`${unit.name} conjura ${ICE_STORM.name}.`);
    this.queue.push({ type: "spell", att: unit.id, tiles: [click], ids: [], label: ICE_STORM.name, spellKind: "iceStorm" });
  }

  private castFireball(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    const tiles = fireballTiles(origin, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
      const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (u && !ids.includes(u.id)) ids.push(u.id);
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

  private castCausticVenom(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    // Its own radius rather than Fireball's: fireballTiles hardcodes FIREBALL.size, which
    // is why venom could not be widened without widening Fireball with it.
    const tiles = hexAreaTiles(origin, CAUSTIC_VENOM.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
      const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (u && !ids.includes(u.id)) ids.push(u.id);
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

  private castDivineBolt(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    const tiles = hexAreaTiles(origin, DIVINE_BOLT.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
      const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (u && !ids.includes(u.id)) ids.push(u.id);
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
  private castMinorVenom(unit: Unit, click: Point): void {
    const origin = fireballOrigin(click, this.cols, this.rows);
    this.spendFamiliarOrTier(unit, "minorVenom");
    this.spellKind = null;
    this.missileTargets = [];
    this.tip = null;
    this.mode = "locked";
    this.queueMinorVenom(unit, origin);
  }

  private queueMinorVenom(unit: Unit, origin: Point): void {
    const tiles = hexAreaTiles(origin, MINOR_VENOM.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
      const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (u && !ids.includes(u.id)) ids.push(u.id);
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

  /** Enemy Veneno Menor (Undead Ox): the Birolho branch's venom targeting on the smaller
   * splash. Returns true if a cast was queued so the caller can skip the rest of the AI. */
  private tryAiMinorVenom(
    next: Unit,
    reach: ReturnType<typeof computeReachable>,
    walkReach: ReturnType<typeof computeReachable>,
    players: Unit[],
  ): boolean {
    if (this.tierRemaining(next, "minorVenom") <= 0) return false;
    let best: { at: Point; from: Point; score: number } | null = null;
    for (const cell of reach.values()) {
      for (const foe of players) {
        if (manhattan(cell, foe) > MINOR_VENOM.range) continue;
        if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay)) continue;
        let hits = 0;
        let score = 0;
        for (const t of hexAreaTiles({ x: foe.x, y: foe.y }, MINOR_VENOM.size, this.cols, this.rows)) {
          const hit = players.find((p) => p.x === t.x && p.y === t.y);
          if (!hit) continue;
          hits += 1;
          score += (hit.maxHp - hit.hp) + (hit.hp <= 8 ? 15 : 0);
        }
        if (hits === 0) continue;
        score += hits * 10;
        if (!best || score > best.score) best = { at: { x: foe.x, y: foe.y }, from: { x: cell.x, y: cell.y }, score };
      }
    }
    if (!best) return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
      this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    this.spendTier(next, "minorVenom");
    this.queueMinorVenom(next, best.at);
    this.queue.push({ type: "delay", dur: 0.12 });
    return true;
  }

  /** Carnivorous Plant's tendril swipe area with her anchor at `at`: every hex touching her
   * Type 7 body (front and both flanks), except the row behind her head. */
  private plantSwipeTiles(next: Unit, at: Point): Point[] {
    const body = footprint({ ...next, x: at.x, y: at.y });
    const inBody = (p: Point) => body.some((b) => b.x === p.x && b.y === p.y);
    const out: Point[] = [];
    for (const b of body) {
      for (const p of hexNeighbors(b.x, b.y)) {
        if (p.y < at.y - 2 || !inBounds(p.x, p.y, this.cols, this.rows) || inBody(p)) continue;
        if (!out.some((o) => o.x === p.x && o.y === p.y)) out.push(p);
      }
    }
    return out;
  }

  /** Carnivorous Plant: with more than one foe in reach of her swipe (from any cell she can
   * reach this turn), she swipes them all with a weapon hit on her ATT sheet. */
  private tryAiPlantSwipe(
    next: Unit,
    reach: ReturnType<typeof computeReachable>,
    walkReach: ReturnType<typeof computeReachable>,
  ): boolean {
    let best: { tiles: Point[]; ids: string[]; from: Point; score: number } | null = null;
    for (const cell of reach.values()) {
      const tiles = this.plantSwipeTiles(next, cell);
      const ids: string[] = [];
      let score = 0;
      for (const t of tiles) {
        const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
        if (!who || who.id === next.id || who.side === next.side || ids.includes(who.id)) continue;
        ids.push(who.id);
        score += 10 + (who.maxHp - who.hp) * 3 + (who.hp <= 8 ? 20 : 0);
      }
      if (ids.length < 2) continue;
      if (!best || score > best.score) best = { tiles, ids, from: { x: cell.x, y: cell.y }, score };
    }
    if (!best) return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
      this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    this.queue.push({ type: "spell", att: next.id, tiles: best.tiles, ids: best.ids, label: "Chicote de Gavinhas", spellKind: "tendrilSwipe" });
    this.queue.push({ type: "delay", dur: 0.12 });
    return true;
  }

  /** Carnivorous Plant's 2 Veneno Cáustico: the Birolho branch's venom targeting, spending
   * her own tier3 slot (her tier4 holds Veneno Menor). */
  private tryAiPlantCausticVenom(
    next: Unit,
    reach: ReturnType<typeof computeReachable>,
    walkReach: ReturnType<typeof computeReachable>,
    players: Unit[],
  ): boolean {
    if (next.spells.tier3 <= 0) return false;
    let best: { at: Point; from: Point; score: number } | null = null;
    for (const cell of reach.values()) {
      for (const foe of players) {
        if (manhattan(cell, foe) > CAUSTIC_VENOM.range) continue;
        if (!clearShot(cell, { x: foe.x, y: foe.y }, this.tiles, this.cols, "bolt", this.decorOverlay)) continue;
        let hits = 0;
        let score = 0;
        for (const t of hexAreaTiles({ x: foe.x, y: foe.y }, CAUSTIC_VENOM.size, this.cols, this.rows)) {
          const hit = players.find((p) => p.x === t.x && p.y === t.y);
          if (!hit) continue;
          hits += 1;
          score += (hit.maxHp - hit.hp) + (hit.hp <= 8 ? 15 : 0);
        }
        if (hits === 0) continue;
        score += hits * 10;
        if (!best || score > best.score) best = { at: { x: foe.x, y: foe.y }, from: { x: cell.x, y: cell.y }, score };
      }
    }
    if (!best) return false;
    if (best.from.x !== next.x || best.from.y !== next.y) {
      this.queue.push({ type: "move", id: next.id, path: reconstructPath(walkReach, best.from) });
    }
    next.spells.tier3 -= 1;
    const tiles = hexAreaTiles(best.at, CAUSTIC_VENOM.size, this.cols, this.rows);
    const ids: string[] = [];
    for (const t of tiles) {
      const u = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
      if (u && !ids.includes(u.id)) ids.push(u.id);
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
  private tryAiPlantPoisonBreath(
    next: Unit,
    reach: ReturnType<typeof computeReachable>,
    walkReach: ReturnType<typeof computeReachable>,
  ): boolean {
    if (next.spells.tier1 <= 0) return false;
    const power = poisonBreathPower(next.level);
    let best: { tiles: Point[]; ids: string[]; from: Point; score: number } | null = null;
    for (const cell of reach.values()) {
      for (const dir of CUBE_DIRS) {
        const tiles = coneSector(cell, dir, power.radius, this.cols, this.rows);
        const ids: string[] = [];
        let score = 0;
        let hitsAlly = false;
        for (const t of tiles) {
          const who = this.units.find((x) => x.alive && occupies(x, t.x, t.y));
          if (!who || who.id === next.id || ids.includes(who.id)) continue;
          if (who.side === next.side) hitsAlly = true;
          ids.push(who.id);
          score += 10 + (who.maxHp - who.hp) * 3 + (who.hp <= 8 ? 20 : 0);
        }
        if (hitsAlly || !ids.length) continue;
        if (!best || score > best.score) best = { tiles, ids, from: { x: cell.x, y: cell.y }, score };
      }
    }
    if (!best) return false;
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

  /** CSS-pixel screen position (matching the coordinate space `render()` just drew into) of a
   * hex's center, plus the current tile size — what the WebGL FX overlay needs to keep a spawned
   * effect glued to its hex while the camera pans/zooms. Also carries a second, camera-INDEPENDENT
   * position (worldX/worldY) for the same hex — the exact same hexCenter formula, just without
   * this frame's pan offset (this.layout.ox/oy) folded in. The water/river shaders sample their
   * noise field from that instead of screen position: sampling from the live screen position
   * meant every camera pan (which happens constantly — dragging, zoom, the camera following a
   * moving unit) shifted the whole noise field by the pan delta, on top of its real u_time-driven
   * animation, so the water visibly slid/warped in lockstep with the camera instead of just
   * flowing. worldX/worldY still scale with the current tile size (so zooming rescales the
   * pattern, which reads as expected), only the pan-induced translation is removed. */
  effectAnchor(col: number, row: number): { x: number; y: number; tile: number; worldX: number; worldY: number } {
    const { cx, cy } = this.hexCenter(col, row);
    const { tile } = this.layout;
    const sqrt3 = Math.sqrt(3);
    const worldX = tile * sqrt3 * (col + (this.mission.squareTiles ? 0 : 0.5 * (row & 1)) + 0.5);
    const worldY = this.boardPad(tile) + tile * (1.5 * row + 1);
    return { x: cx, y: cy, tile, worldX, worldY };
  }

  /** Footprint (as a multiple of one hex's own tile size, the same unit SpawnOptions.radiusTiles
   * already uses everywhere else) for ONE "web" WebGL effect drawn over an entire Dreaming Web
   * zone — see BattleCanvas's zone sync. Neighboring hex centers on this grid sit sqrt(3) tiles
   * apart (a regular hex grid — verified: dx=tile*sqrt3/2, dy=tile*1.5 gives the same
   * hypot(dx,dy)=tile*sqrt3 to every one of the 6 neighbors, not just the horizontal pair), so a
   * cube-distance-R hex disk's farthest cell sits R*sqrt(3) tiles out along its own spoke; + 1.0
   * reaches that cell's own outer edge, matching DEFAULT_RADIUS_TILES.web's existing convention
   * that 1.0 fills exactly one hex. */
  webZoneRadiusTiles(radius: number): number {
    return radius * Math.sqrt(3) + 1.0;
  }

  /** Live geometry for Dreaming Web's travelling WebGL shot — null whenever no such shot is
   * currently in flight (including once it lands: the beam only exists while actually
   * travelling, per the same `m.t < m.travel` window MissileFx tracks; the floor patch that
   * appears at the target hex is its own separate "web" effect, not this one fading out).
   * BattleCanvas polls this every frame and feeds it straight into
   * EffectsRenderer.updateOverride — see EffectOverride for why a fixed-hex getAnchor(col,row)
   * effect can't represent a continuously moving, continuously growing beam on its own. */
  webShotBeam(): { x: number; y: number; worldX: number; worldY: number; tile: number; angle: number; length: number } | null {
    const m = this.missileFx.find((x) => x.live && x.kind === "webOfDreams" && x.t < x.travel);
    if (!m) return null;
    const from = this.effectAnchor(m.fromX, m.fromY);
    const to = this.effectAnchor(m.toX, m.toY);
    const k = Math.min(1, m.t / m.travel);
    const headX = from.x + (to.x - from.x) * k;
    const headY = from.y + (to.y - from.y) * k;
    const headWorldX = from.worldX + (to.worldX - from.worldX) * k;
    const headWorldY = from.worldY + (to.worldY - from.worldY) * k;
    const dx = headX - from.x;
    const dy = headY - from.y;
    return {
      x: (from.x + headX) / 2,
      y: (from.y + headY) / 2,
      worldX: (from.worldX + headWorldX) / 2,
      worldY: (from.worldY + headWorldY) / 2,
      tile: from.tile,
      angle: Math.atan2(dy, dx),
      length: Math.hypot(dx, dy),
    };
  }

  panBy(dx: number, dy: number): void {
    this.camX += dx;
    this.camY += dy;
    this.clampCam();
  }

  /** Preserve an editor preview camera while its draft mission is rebuilt. */
  cameraPosition(): { x: number; y: number } {
    return { x: this.camX, y: this.camY };
  }

  restoreCamera(position: { x: number; y: number }): void {
    this.camX = position.x;
    this.camY = position.y;
    this.clampCam();
  }

  /** Editor-only overlay: show the footprint of the decoration brush in the live preview. */
  drawDecorationHighlight(ctx: any, decorationId: string, selected?: { x: number; y: number; rot?: number }): void {
    const tile = this.layout.tile;
    ctx.save();
    ctx.lineWidth = Math.max(2, tile * 0.075);
    ctx.strokeStyle = "rgba(255, 207, 82, 0.98)";
    ctx.fillStyle = "rgba(255, 190, 46, 0.14)";
    ctx.shadowColor = "rgba(255, 174, 35, 0.95)";
    ctx.shadowBlur = Math.max(7, tile * 0.32);
    for (const placement of this.decorations) {
      if (placement.id !== decorationId) continue;
      if (selected && (placement.x !== selected.x || placement.y !== selected.y || (placement.rot ?? 0) !== (selected.rot ?? 0))) continue;
      for (const cell of placedFootprint(placement)) {
        const { cx, cy } = this.hexCenter(placement.x + cell.dx, placement.y + cell.dy);
        this.hexPath(ctx, cx, cy, tile * 0.91);
        ctx.fill();
        ctx.stroke();
      }
    }
    ctx.restore();
  }
  setZoom(level: number): void {
    const next = Math.max(0, Math.min(ZOOM_RADII.length - 1, Math.round(level)));
    if (next === this.zoom) return;
    const old = ZOOM_RADII[this.zoom]!;
    const neu = ZOOM_RADII[next]!;
    const k = neu / old;
    this.camX = (this.camX + this.viewW / 2) * k - this.viewW / 2;
    this.camY = (this.camY + this.viewH / 2) * k - this.viewH / 2;
    this.zoom = next;
    this.clampCam();
    this.emit();
  }

  setSpeed(mode: "slow" | "normal" | "fast"): void {
    this.speedMode = mode;
    this.emit();
  }

  cycleZoom(dir: number): void {
    this.setZoom(this.zoom + (dir < 0 ? -1 : 1));
  }

  private boardPad(tile: number): number {
    return tile * 2.4;
  }

  private boardSize(tile: number): { w: number; h: number } {
    const sqrt3 = Math.sqrt(3);
    return {
      w: tile * sqrt3 * (this.cols + 0.5),
      h: tile * (1.5 * (this.rows - 1) + 2) + this.boardPad(tile),
    };
  }

  /** Normal gameplay stays on the playable board, rather than panning across a large
   * decorative backdrop. The editor may opt into a fixed preview rim. */
  private cameraMargin(tile: number): { x: number; y: number } {
    // A centered unit may be near the board edge; allow a quarter viewport of camera travel
    // beyond every edge so the party can still stay in the exact screen center there. The
    // editor preview opts into a small fixed rim instead: a half-viewport overscroll exposes
    // a huge black strip when a map's starting party is close to its edge.
    const previewMargin = this.previewPanMarginRadii > 0 ? this.previewPanMarginRadii * tile : null;
    return {
      // Normal gameplay stays within the map. A wide backdrop overscroll is disorienting
      // and makes the board look smaller than it is. Only the editor's explicit preview rim
      // may add camera room outside the playable grid.
      x: previewMargin ?? 0,
      y: previewMargin ?? 0,
    };
  }

  private uniqueNeutralNpcSpawns(spawns: readonly Spawn[]): { spawn: Spawn; index: number }[] {
    const names = new Set<string>();
    const appearances = new Set<string>();
    return spawns.flatMap((spawn, index) => {
      if (!spawn.dialog) return [{ spawn, index }];
      const name = spawn.name.normalize("NFD").replace(/[\u0300-\u036f]/g, "").trim().toLocaleLowerCase();
      const appearance = CLASSES[spawn.classId]?.sprite ?? spawn.classId;
      if (names.has(name) || appearances.has(appearance)) return [];
      names.add(name);
      appearances.add(appearance);
      return [{ spawn, index }];
    });
  }

  /** Editor-only edge room for panning. Real battle never calls this, so battle camera bounds
   * stay unchanged. */
  setPreviewPanMargin(radii: number): void {
    this.previewPanMarginRadii = Math.max(0, radii);
    this.clampCam();
  }

  private clampCam(): void {
    const tile = ZOOM_RADII[this.zoom]!;
    const { w, h } = this.boardSize(tile);
    const roomX = 0;
    const roomY = 0;
    // Under fog of war, the camera stays within the playable board instead of exposing the
    // decorative dark rim around it. The first tile row starts after boardPad; keep that
    // offset out of view too. If a board is smaller than the viewport, center it as a whole.
    if (this.fogged) {
      const maxX = w - this.viewW;
      const boardTop = this.boardPad(tile);
      const maxY = h - this.viewH;
      const loX = maxX < 0 ? maxX / 2 : 0, hiX = maxX < 0 ? maxX / 2 : maxX;
      const loY = maxY < boardTop ? (boardTop + maxY) / 2 : boardTop, hiY = maxY < boardTop ? (boardTop + maxY) / 2 : maxY;
      this.camX = Math.min(hiX + roomX, Math.max(loX - roomX, this.camX));
      this.camY = Math.min(hiY + roomY, Math.max(loY - roomY, this.camY));
      return;
    }
    // HARD RULE (direct request): on every map the camera pans exactly 4 hexes past each edge
    // of the grid — left, right, top and bottom, no more, no less. The only exceptions are
    // fog-of-war maps (branch above) and map 4, A Ponte de Pedra ("thebridge"), which keep the
    // bounds below. The editor preview's own pan rim (previewPanMarginRadii) is a protected
    // map-editor control and also keeps the bounds below. Do not add other exceptions.
    if (this.mission.id !== "thebridge" && this.previewPanMarginRadii <= 0) {
      const edgeHexes = 4;
      const edgeX = edgeHexes * Math.sqrt(3) * tile;
      const edgeY = edgeHexes * 1.5 * tile;
      const loX = -edgeX;
      const hiX = w + edgeX - this.viewW;
      const loY = this.boardPad(tile) - edgeY;
      const hiY = h + edgeY - this.viewH;
      // Grid plus its 4-hex rim smaller than the window: no pan room on that axis, centered.
      this.camX = hiX < loX ? (loX + hiX) / 2 : Math.min(hiX, Math.max(loX, this.camX));
      this.camY = hiY < loY ? (loY + hiY) / 2 : Math.min(hiY, Math.max(loY, this.camY));
      // The camera also stops where the background picture ends (direct request), even inside
      // the 4-hex rim. Only O Vau's picture is pinned to the board; every other map's picture
      // follows the camera and always fills the screen (as it does in the tilted Tactics view).
      const pinnedBackdrop = this.mission.id === "vau" && !this.tacticsCamera ? this.art.backdrops?.vau : undefined;
      if (pinnedBackdrop) {
        const b = vauBackdropBounds(tile, this.cols, this.viewW, this.viewH, pinnedBackdrop.width / Math.max(1, pinnedBackdrop.height));
        // Never so tight that part of the grid itself becomes unreachable: at close zoom O Vau's
        // picture ends above the board's bottom row.
        this.camX = Math.min(Math.max(b.left + b.width, w) - this.viewW, Math.max(Math.min(b.left, 0), this.camX));
        this.camY = Math.min(Math.max(b.top + b.height, h) - this.viewH, Math.max(Math.min(b.top, this.boardPad(tile)), this.camY));
      }
      return;
    }
    const margin = this.cameraMargin(tile);
    // For a board smaller than its window, the natural resting camera is its centered
    // position. Larger boards retain their existing origin, merely gaining this small rim.
    const naturalMaxX = w - this.viewW;
    const naturalMaxY = h - this.viewH;
    const minX = naturalMaxX < 0 ? naturalMaxX / 2 - margin.x : -margin.x;
    const minY = naturalMaxY < 0 ? naturalMaxY / 2 - margin.y : -margin.y;
    const maxX = naturalMaxX < 0 ? naturalMaxX / 2 + margin.x : naturalMaxX + margin.x;
    const maxY = naturalMaxY < 0 ? naturalMaxY / 2 + margin.y : naturalMaxY + margin.y;
    this.camX = Math.min(maxX + roomX, Math.max(minX - roomX, this.camX));
    this.camY = Math.min(maxY + roomY, Math.max(minY - roomY, this.camY));
  }

  ensureVisible(col: number, row: number): void {
    const { cx, cy } = this.hexCenter(col, row);
    const tile = ZOOM_RADII[this.zoom]!;
    const m = 64;
    const top = this.boardPad(tile);
    if (cx < m) this.camX += cx - m;
    if (cy < top) this.camY += cy - top;
    if (cx > this.viewW - m) this.camX += cx - (this.viewW - m);
    if (cy > this.viewH - m) this.camY += cy - (this.viewH - m);
    this.clampCam();
  }

  /** Same job as ensureVisible, but for a whole spread of tiles at once — a Fireball/Caustic
   * Venom blast, an enemy's cone or line spell, anything hitting more than one hex. A single
   * ensureVisible(centroid) call still left a wide spread's outer edge off past the viewport
   * (the centroid can sit comfortably in view while the blast's far corner doesn't); this pulls
   * both the near and far corner of the affected area's bounding box in, one after the other —
   * each call sees the camera position the previous one just left, so the two corners converge
   * toward "as much of the whole spread fits as the viewport allows" rather than fighting each
   * other. A spread wider than the viewport itself still can't fully fit — no amount of panning
   * fixes that, only zooming out would — but every real spell's radius is well within one
   * screen, so this covers the actual reported case (a wide blast landing partly off-frame). */
  private ensureAreaVisible(tiles: readonly Point[]): void {
    if (tiles.length === 0) return;
    let minX = tiles[0]!.x, maxX = tiles[0]!.x, minY = tiles[0]!.y, maxY = tiles[0]!.y;
    for (const t of tiles) {
      if (t.x < minX) minX = t.x;
      if (t.x > maxX) maxX = t.x;
      if (t.y < minY) minY = t.y;
      if (t.y > maxY) maxY = t.y;
    }
    this.ensureVisible(minX, minY);
    this.ensureVisible(maxX, maxY);
  }

  private focusPlayers(): void {
    // Use roster/spawn order, not initiative: the opening camera must start at the party's
    // configured start point every time, even when a different hero wins initiative.
    const firstStartingPlayer = this.units.find((unit) => unit.side === "player" && unit.alive);
    const firstUnit = firstStartingPlayer ?? this.units[0];
    if (firstUnit) this.centerOn(firstUnit.x, firstUnit.y);
  }

  /** Opens an editor preview at the party's first configured starting position. */
  centerOnStartingParty(): void {
    this.focusPlayers();
  }

  /** Centers the camera on the board's own geometric middle, independent of any unit's
   * position — unlike focusPlayers/centerOn, which the map editor's preview panel should NOT
   * use: a spawn tucked near one edge (or no units at all yet, on a still-empty draft) would
   * otherwise leave the preview opening on a corner instead of showing the whole drafted map. */
  centerOnBoard(): void {
    this.centerOn((this.cols - 1) / 2, (this.rows - 1) / 2);
  }

  /** Same purpose as centerOnBoard (an editor-preview-only initial framing, independent of any
   * unit's position), but opens on the board's LEFT edge instead of its geometric middle — per
   * direct request: every map's own content starts at its left edge, and the preview centering
   * on the whole (often mostly-empty) bounding box read as opening on empty void instead.
   * centerOn(0, ...) clamps against the left margin same as any other camera move, so this
   * still leaves the small backdrop-peek margin clampCam already grants every camera. */
  centerOnBoardLeft(): void {
    this.centerOn(0, (this.rows - 1) / 2);
  }

  private centerOn(col: number, row: number): void {
    const { cx, cy } = this.hexCenter(col, row);
    this.centerOnPoint(cx, cy);
  }

  private centerOnPoint(cx: number, cy: number): void {
    // cx/cy are screen coordinates in the current layout, so apply their offset from the
    // viewport center to the existing camera origin.
    this.camX += cx - this.viewW / 2;
    this.camY += cy - this.viewH / 2;
    this.clampCam();
  }

  /** Resolve a canvas coordinate to a board cell without changing game state. */
  cellAt(cssX: number, cssY: number): Point | null {
    const { ox, oy, tile } = this.layout;
    const sqrt3 = Math.sqrt(3);
    if (this.mission.squareTiles) {
      const col = Math.floor((cssX - ox) / (tile * sqrt3));
      const row = Math.floor((cssY - oy - this.boardPad(tile) - tile * 0.25) / (tile * 1.5));
      return col >= 0 && row >= 0 && col < this.cols && row < this.rows ? { x: col, y: row } : null;
    }
    const x = cssX - ox - tile * sqrt3 * 0.5;
    const y = cssY - oy - this.boardPad(tile) - tile;
    const q = ((sqrt3 / 3) * x - (1 / 3) * y) / tile;
    const r = ((2 / 3) * y) / tile;
    const c = cubeRound(q, r, -q - r);
    const col = c.q + (c.r - (c.r & 1)) / 2;
    const row = c.r;
    if (col < 0 || row < 0 || col >= this.cols || row >= this.rows) return null;
    return { x: col, y: row };
  }

  /** Finds the unit (if any) whose drawn sprite rectangle contains a canvas coordinate, not
   * just whichever single hex it's anchored to — a sprite commonly extends well beyond its own
   * hex on screen, so an exact-hex hit test alone makes some units hard to click. Used by the
   * map editor's preview to make right-click pickup work anywhere on a unit's visible art,
   * matching cellAt's own (cssX, cssY) convention. Ignores live idle wobble (sway/bob/lift):
   * the editor preview never ticks, so those sit at their rest value anyway. */
  unitSpriteAt(cssX: number, cssY: number): Unit | null {
    const tile = ZOOM_RADII[this.zoom]!;
    const cell = tile * Math.sqrt(3);
    for (const u of this.units) {
      if (u.fade <= 0 || this.unitHidden(u)) continue;
      const { cx: px, cy: py } = this.unitPixel(u);
      const { w, h, footY, footOffset } = this.computeUnitVisual(u, cell, tile);
      if (cssX < px - w / 2 || cssX > px + w / 2) continue;
      if (cssY < py + footY - h + footOffset || cssY > py + footY + footOffset) continue;
      return u;
    }
    return null;
  }

  private hexCenter(col: number, row: number): { cx: number; cy: number } {
    const { ox, oy, tile } = this.layout;
    const sqrt3 = Math.sqrt(3);
    return {
      cx: ox + tile * sqrt3 * (col + (this.mission.squareTiles ? 0 : 0.5 * (row & 1)) + 0.5),
      cy: oy + this.boardPad(tile) + tile * (1.5 * row + 1),
    };
  }

  /** True when the land-shore-FX debug flag is set in this browser. Local to this class —
   * nothing else in the codebase reads or writes this key, so flipping it can only ever affect
   * the synthesized-placements block in the constructor above. */
  private landShoreFxDebugEnabled(): boolean {
    if (typeof window === "undefined") return false;
    try {
      return window.localStorage.getItem("emberash:landShoreFx") === "1";
    } catch {
      return false;
    }
  }

  private hexPath(ctx: any, cx: number, cy: number, size: number): void {
    ctx.beginPath();
    for (let i = 0; i < 6; i++) {
      const a = (Math.PI / 180) * (60 * i - 30);
      const x = cx + size * Math.cos(a);
      const y = cy + size * Math.sin(a);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.closePath();
  }

  /** Whether a facing's own drawing exists, kicking off its load the first time it is
   * asked for. A file that 404s settles as "no" and the prop keeps the base drawing —
   * every prop starts with only its east art, so this is the normal answer, not a fault. */
  private decorArtReady(file: string): boolean {
    const known = this.art.decorations[file];
    if (known) return known.naturalWidth > 0;
    const img = new Image();
    img.src = decorationImage(file);
    this.art.decorations[file] = img;
    return false;
  }

  /** Multi-hex terrain props draw as one image over their whole footprint's bounding box,
   * not hex-clipped like regular tiles — they don't need to fill the exact hex shape. */
  private drawDecorations(
    ctx: any,
    tile: number,
    cssW: number,
    cssH: number,
    layer: "ground" | "behind" | "front" = "ground",
    depthRange?: { after: number; through: number },
  ): void {
    const SQRT3 = Math.sqrt(3);
    // Higher-priority near-side scenery paints last, while equal priorities preserve placement
    // order. This makes transparent decoration overlaps deterministic.
    const orderedDecorations = this.decorations
      .map((p, index) => ({ p, index, order: DECORATIONS[p.id]?.decorRenderOrder ?? 0 }))
      .sort((a, b) => a.order - b.order || a.index - b.index);
    for (const { p } of orderedDecorations) {
      const def = DECORATIONS[p.id];
      if (def?.model3d) continue;
      const artId = decorationPlacementArt(p);
      let img = this.art.decorations[artId];
      if ((!img || !img.naturalWidth) && def) {
        img = this.art.decorations[artId] ?? new Image();
        if (!img.src) {
          img.src = decorationImage(artId);
          decorationImageRetryWebp(img, artId);
        }
        this.art.decorations[artId] = img;
      }
      const decorLayer = def?.unitLayer ?? (def?.foreground ? "front" : "ground");
      if (!def || !img || decorLayer !== layer) continue;
      if (depthRange && layer === "ground") {
        const frontDepth = Math.max(...placedFootprint(p).map(({ dx, dy }) => this.effectAnchor(p.x + dx, p.y + dy).worldY));
        if (frontDepth <= depthRange.after || frontDepth > depthRange.through) continue;
      }
      // Props are part of the ground, so they follow the terrain rule: remembered once
      // walked past, hidden while never seen. One explored cell shows the whole prop —
      // a five-hex parapet half-drawn at a fog edge would read as broken art.
      if (this.fogged && !placedFootprint(p).some((f) => this.explored(p.x + f.dx, p.y + f.dy))) continue;
      let minDx = 0;
      let maxDx = 0;
      let minDy = 0;
      let maxDy = 0;
      let sumCx = 0;
      let sumCy = 0;
      // The shape sets how big the image is drawn; the turn is applied to the canvas
      // below, so the box is measured unturned and carried around with it. The centre,
      // though, has to be where the prop actually sits once turned.
      for (const { dx, dy } of def.footprint) {
        minDx = Math.min(minDx, dx);
        maxDx = Math.max(maxDx, dx);
        minDy = Math.min(minDy, dy);
        maxDy = Math.max(maxDy, dy);
      }
      for (const { dx, dy } of placedFootprint(p)) {
        const c = this.hexCenter(p.x + dx, p.y + dy);
        sumCx += c.cx;
        sumCy += c.cy;
      }
      const n = def.footprint.length;
      const cx = sumCx / n;
      const cy = sumCy / n;
      const one = def.footprint.length === 1;
      const item = CHEST_DECOR_IDS.has(p.id);
      const tree = p.id === "dead-tree";
      const log = p.id === "fallen-log";
      const wall = p.id === "barricade";
      const waypoint = !!def.exitKind;
      const stoneStairs = p.id === "stone-stairs-up-001" || p.id === "stone-stairs-down-001";
      // Single-building houses share the "house" art scale (per user request), and their
      // movement-blocking footprint covers the full ground base beneath that art.
      const house = HOUSE_DECOR_IDS.has(p.id);
      const bigHouse = BIG_HOUSE_DECOR_IDS.has(p.id);
      const anyHouse = house || bigHouse;
      const w0 = tree
        ? tile * 1.28
        : log
          ? tile * SQRT3 * 2.05
          : wall
            ? tile * 1.42
            : anyHouse
              ? tile * 1.45 * 3
              : waypoint
                ? tile * SQRT3 * (one ? 1 : 2)
                : item
                  ? tile * 0.92
                  : one
                    ? tile * 1.55
                    : tile * SQRT3 * (maxDx - minDx + 1.7);
      const baseH = tree
        ? tile * 2.55
        : log
          ? tile * 0.82
          : wall
            ? tile * 1.18
            : anyHouse
              ? tile * 1.58 * 3
              : waypoint
                ? tile * 2
                : item
                  ? tile * 0.72
                  : one
                    ? tile * 1.65
                    : tile * (1.5 * (maxDy - minDy) + 2.3);
      const h0 = def.artAspect ? w0 / def.artAspect : baseH * (def.heightScale ?? 1);
      // Taller near-side props rise upward from their ground anchor instead of stretching
      // equally in both directions. That preserves the shallow isometric perspective.
      // Chests use the hex's ground anchor directly; a per-item nudge displaced them off-center.
      const dy0 = waypoint ? 0 : (tree ? -tile * 0.55 : wall ? -tile * 0.12 : anyHouse ? -tile * 0.28 * 3 : 0) - (h0 - baseH) * 0.42;
      // Global art scale (see DECOR_ART_SCALE), grown from the bottom edge — same as
      // ThreeBattleRenderer's decorSize, so both renderers draw props the same size.
      const artScale = waypoint ? 1 : (anyHouse ? HOUSE_ART_SCALE : DECOR_ART_SCALE) * (def.artScale ?? 1);
      const w = stoneStairs ? tile * SQRT3 : w0 * artScale;
      const h = stoneStairs ? tile * 3.5 : h0 * artScale;
      const dy = stoneStairs ? 0 : dy0 - (h0 * (artScale - 1)) / 2;
      // Cull on the box actually drawn, which is why this sits after the sizing above and
      // not up by the centre. Every branch below centres the image on `(cx, cy + dy)`, so
      // one bounding circle bounds the turned cases as well as the straight one.
      //
      // The previous test allowed the centre a flat `tile * 4` of slack, but a prop is only
      // as cullable as it is wide: a row of five spans `SQRT3 * (4 + 1.7) / 2 ≈ 4.94` tiles
      // either side of its centre, and a row of four `≈ 4.07`. Both exceed 4, so a bridge
      // parapet straddling a screen edge was dropped whole while part of it still belonged
      // on screen. Deriving the reach from `w`/`h` keeps that honest for any footprint.
      const reach = Math.hypot(w, h) / 2;
      if (cx + reach < 0 || cx - reach > cssW || cy + dy + reach < 0 || cy + dy - reach > cssH) continue;
      // Facing art if the prop has it, the way isometric games do it: a drawing per facing,
      // mirrored to cover the opposite one. Only when a facing has no drawing do we fall
      // back to turning the bitmap, which tilts rather than faces and is a placeholder.
      const alternateMirror = !!def.mirrorAlternate && p.x >= this.cols / 2;
      // Paired posts on opposite halves of the map face as mirror images across its vertical
      // centerline. Select the baked side-4 sprite instead of flipping at render time; props
      // without the dedicated file keep the normal facing fallback.
      const facingRot = ((p.rot ?? 0) + (alternateMirror ? 3 : 0)) % 6;
      const facing = decorationFacing(artId, facingRot, (file) => this.decorArtReady(file));
      const facingMirror = facing.mirror;
      const art = facing.own ? (this.art.decorations[facing.file] ?? img) : img;
      const anchor = decorationAnchor(art);
      let anchorDx = (anchor ? (0.5 - (anchor.u0 + anchor.u1) / 2) : 0) * w + (def.artOffsetX ?? 0) * w;
      let anchorDy = anchor ? (1 - anchor.v) * h : 0;

      if (facing.step === 0) {
        if (p.mirrorX) {
          ctx.save();
          ctx.translate(cx, cy + dy);
          ctx.scale(-1, 1);
          ctx.drawImageLit(art, -w / 2 - anchorDx, -h / 2 + anchorDy, w, h);
          ctx.restore();
        } else {
          ctx.drawImageLit(art, cx - w / 2 + anchorDx, cy - h / 2 + dy + anchorDy, w, h);
        }
      } else if (facing.own) {
        ctx.save();
        ctx.translate(cx, cy + dy);
        if (facingMirror) { ctx.scale(-1, 1); anchorDx = -anchorDx; }
        ctx.translate(anchorDx, anchorDy);
        ctx.drawImageLit(art, -w / 2, -h / 2, w, h);
        ctx.restore();
      } else {
        ctx.save();
        ctx.translate(cx, cy + dy);
        ctx.rotate((facing.step * Math.PI) / 3);
        ctx.translate(anchorDx, anchorDy);
        ctx.drawImageLit(art, -w / 2, -h / 2, w, h);
        ctx.restore();
      }
    }
  }

  private footprintCentroid(
    x: number,
    y: number,
    size: number,
    footprintW?: number,
    footprintOffsets?: { dx: number; dy: number }[],
  ): { cx: number; cy: number } {
    // Units with an extended footprint anchor on their front tile(s) only — averaging in the
    // cells behind them would drag the sprite's feet upward, off the tile the player actually
    // sees them standing on.
    const cells =
      size >= 4 || footprintOffsets ? footprintFrontRow({ x, y, footprintOffsets }, footprintW ?? 2) : footprint({ x, y, size });
    let cx = 0;
    let cy = 0;
    for (const p of cells) {
      const c = this.hexCenter(p.x, p.y);
      cx += c.cx;
      cy += c.cy;
    }
    const n = Math.max(1, cells.length);
    return { cx: cx / n, cy: cy / n };
  }

  /** World-space (camera-independent) equivalent of footprintCentroid — the same front-row
   * average, in the same worldX/worldY terms effectAnchor already exposes for a single hex.
   * Needed because effectAnchor only ever answers for one plain hex, which is wrong for a
   * multi-hex boss (Troll, Horror, Asherah, ...): its sprite anchors on its footprint's front
   * row, not the hex `col`/`row` happen to name — see footprintCentroid's own comment. */
  private footprintCentroidWorld(
    x: number,
    y: number,
    size: number,
    footprintW?: number,
    footprintOffsets?: { dx: number; dy: number }[],
  ): { worldX: number; worldY: number } {
    const cells =
      size >= 4 || footprintOffsets ? footprintFrontRow({ x, y, footprintOffsets }, footprintW ?? 2) : footprint({ x, y, size });
    const { tile } = this.layout;
    const sqrt3 = Math.sqrt(3);
    let wx = 0;
    let wy = 0;
    for (const p of cells) {
      wx += tile * sqrt3 * (p.x + (this.mission.squareTiles ? 0 : 0.5 * (p.y & 1)) + 0.5);
      wy += this.boardPad(tile) + tile * (1.5 * p.y + 1);
    }
    const n = Math.max(1, cells.length);
    return { worldX: wx / n, worldY: wy / n };
  }

  /**
   * How far this unit's sprite rides above its hex, in pixels, for high ground.
   *
   * Mirrors unitPixel's interpolation instead of reading the current cell outright:
   * during a step `u.x`/`u.y` still hold the cell being left, so a unit walking onto a
   * hill would snap upward as the step ended. Easing it over the same step makes the
   * climb read as a climb.
   *
   * Reads the consolidated properties, so a prop whose `yieldsHighGround` switch is on
   * lifts a sprite exactly as a painted hill does — one answer for the bonus and for
   * the picture. Uses the anchor cell, which is the cell the combat bonus reads too.
   */
  private unitLift(u: Unit, cell: number): number {
    const full = cell * HIGH_GROUND_LIFT;
    const liftAt = (x: number, y: number) => (this.hexAt(x, y).height ? full : 0);
    if (this.active && this.active.type === "move" && this.active.id === u.id) {
      const a = this.active;
      const from = a.path[a.i];
      const to = a.path[a.i + 1];
      if (from && to) {
        const k = Math.min(1, a.t / this.moveStepDur(a));
        const A = liftAt(from.x, from.y);
        const B = liftAt(to.x, to.y);
        return A + (B - A) * k;
      }
    }
    return liftAt(u.x, u.y);
  }

  /** One hex step's duration — the single clock both the move stepper and every drawn
   * position (unitPixel/unitAnchor/unitLift) use, so the sprite glides at a constant speed
   * instead of dashing ahead and waiting for the step to finish. */
  private moveStepDur(a?: MoveAnim): number {
    const walk = this.speedMode === "fast" ? 0.12 : this.speedMode === "slow" ? 0.36 : 0.22;
    // A Bull Rush charge is a burst, about 3x walking pace.
    const mover = a && this.units.find((u) => u.id === a.id);
    const ox = mover?.classId === "bigBlueCalf";
    if (mover?.sprite === "salazar" && !a?.charge) return walk / SALAZAR_STEP_PACE;
    return (a?.charge ? walk * 0.5 : walk) / (ox ? BIG_BLUE_OX_PACE : 1);
  }

  private unitPixel(u: Unit): { cx: number; cy: number } {
    if (this.active && this.active.type === "move" && this.active.id === u.id) {
      const a = this.active;
      const from = a.path[a.i];
      const to = a.path[a.i + 1];
      if (from && to) {
        const k = Math.min(1, a.t / this.moveStepDur(a));
        const A = this.footprintCentroid(from.x, from.y, u.size, u.footprintW, u.footprintOffsets);
        const B = this.footprintCentroid(to.x, to.y, u.size, u.footprintW, u.footprintOffsets);
        return { cx: A.cx + (B.cx - A.cx) * k, cy: A.cy + (B.cy - A.cy) * k };
      }
    }
    return this.footprintCentroid(u.x, u.y, u.size, u.footprintW, u.footprintOffsets);
  }

  /** Public, world-space (camera-independent) equivalent of the private unitPixel — the anchor
   * position ThreeBattleRenderer needs for ANY unit, boss/multi-hex ones included, instead of
   * the plain single-hex position effectAnchor(u.drawX, u.drawY) gives (wrong for a footprint
   * that anchors on its front row — see footprintCentroidWorld). Mirrors unitPixel's own
   * mid-move interpolation so a boss's sprite tracks the same eased position while walking that
   * its combat hit box does. */
  unitAnchor(u: Unit): { worldX: number; worldY: number } {
    if (this.active && this.active.type === "move" && this.active.id === u.id) {
      const a = this.active;
      const from = a.path[a.i];
      const to = a.path[a.i + 1];
      if (from && to) {
        const k = Math.min(1, a.t / this.moveStepDur(a));
        const A = this.footprintCentroidWorld(from.x, from.y, u.size, u.footprintW, u.footprintOffsets);
        const B = this.footprintCentroidWorld(to.x, to.y, u.size, u.footprintW, u.footprintOffsets);
        return { worldX: A.worldX + (B.worldX - A.worldX) * k, worldY: A.worldY + (B.worldY - A.worldY) * k };
      }
    }
    return this.footprintCentroidWorld(u.x, u.y, u.size, u.footprintW, u.footprintOffsets);
  }

  /** True while an action's visual sequence is still playing. */
  isAnimating(): boolean {
    return this.active !== null || this.queue.length > 0;
  }

  /** Walk-cycle frame for a unit mid-move, driven by how far along its path it actually is.
   *
   * Not by the global bob clock, which is what a walk cut got before and why none of them
   * played: a hex step lasts 0.22s (0.12 in fast mode) and bob runs at 0.58x for anything
   * size 4 or over, so a golem advanced barely half a frame per hex — measured, three of its
   * eight frames across three hexes, starting on whichever one bob's random spawn value
   * landed on. Tied to the path instead, every walk starts at frame 0 and runs a full loop
   * every two hexes, at the same pace for a golem as for a familiar. */
  private walkFrame(u: Unit, n: number): number {
    const a = this.active;
    if (n <= 1 || !a || a.type !== "move" || a.id !== u.id) return 0;
    const ox = u.classId === "bigBlueCalf";
    if (u.sprite === "minor-horror-001") {
      const dur = this.moveStepDur(a);
      return Math.floor((a.i * dur + Math.min(a.t, dur)) * n / MINOR_HORROR_SECONDS.walk) % n;
    }
    const salazar = u.sprite === "salazar";
    const dur = ox || salazar ? this.moveStepDur(a) : this.speedMode === "fast" ? 0.12 : this.speedMode === "slow" ? 0.36 : 0.22;
    const steps = a.i + Math.min(1, a.t / dur);
    // A sheet's full loop used to always take exactly 2 hexes no matter its frame count, so a
    // 36-frame sheet (Aldric, Malrec, Cultist V2, Kael Final, Conjurer, The Butcher) flipped
    // through 3-6x more frames per hex than a 6-12 frame sheet and read as frantic next to
    // them. Capping the frames-per-hex rate at what a 12-frame sheet already gets leaves every
    // sheet at n<=12 untouched and only slows the oversized ones down to match its pace.
    const framesPerHex =
      n >= LONG_SHEET_FRAMES ? (n / (salazar ? SALAZAR_WALK_LOOP_SECONDS * (dur * SALAZAR_STEP_PACE / 0.22) : LONG_WALK_SECONDS)) * (ox ? BIG_BLUE_OX_PACE : 1) * dur : Math.min(n / 2, 6) * (u.sprite === "conjurer" || u.sprite === "malrec" ? 0.9 : 1);
    return Math.floor(steps * framesPerHex) % n;
  }

  /** Whether this move plays the cosmetic alternate walk (see GameArt.walks2). Decided once
   * per queued move and remembered for it, so the cycle never flips mid-walk. Visual only —
   * Math.random, never the seeded battle rng, so it can't change any gameplay roll. */
  private readonly walkAlt = new WeakMap<object, boolean>();
  private walkAltFor(move: object): boolean {
    let alt = this.walkAlt.get(move);
    if (alt === undefined) {
      alt = Math.random() < 1 / 3;
      this.walkAlt.set(move, alt);
    }
    return alt;
  }

  /** True while a unit that just died is still playing its death sheet (GameArt.deaths) or
   * lying still on its last frame — its fade-out waits until this is over. */
  private deathSheetPlaying(u: Unit): boolean {
    if (u.alive || u.diedAt == null || !this.art.deaths[u.sprite]) return false;
    const hitLead = u.classId !== "bigBlueCalf" && this.art.hits[u.sprite] ? HIT_ANIM_SECONDS : 0;
    const deathSeconds = u.classId === "minorHorror" ? MINOR_HORROR_SECONDS.death : DEATH_ANIM_SECONDS / (u.classId === "bigBlueCalf" ? BIG_BLUE_OX_PACE : 1);
    return this.time - u.diedAt < hitLead + deathSeconds + DEATH_HOLD_SECONDS;
  }

  private idleFrame(u: Unit, n: number): number {
    if (n <= 1) return 0;
    // Neera V2 Idle atlas: 36 frames, 98 ms per frame.
    if (u.sprite === "neera") return Math.floor(u.bob / 0.098) % n;
    const moving = this.active?.type === "move" && this.active.id === u.id;
    // Milícia V2 Idle sheet: 36 frames at its authored 119 ms per frame (its sheet's JSON), played
    // forward — the sheet loops seamlessly. The generic 3 s pass below ran it ~1.4x too fast.
    if (u.sprite === "militia-v2" && !moving) return Math.floor(u.bob / 0.119) % n;
    if (u.sprite === "minor-horror-001") return Math.floor(u.bob * n / MINOR_HORROR_SECONDS.idle) % n;
    if (u.sprite === "big-blue-ox-002") return Math.floor(u.bob * (moving ? 8 * BIG_BLUE_OX_PACE : n / 5.5)) % n;
    if (u.classId === "familiar" || u.classId === "familiar2") {
      // Familiar 2's idle went from 12 to 36 frames over the same footage span; scaling by
      // n / 12 keeps its loop the same length it always was.
      const rate = (moving ? 8.0 : 5.5) * (u.classId === "familiar2" ? n / 12 : 1);
      return Math.floor(u.bob * rate) % n;
    }
    if (u.classId === "wardog" || u.classId === "swampBlueCalf" || u.classId === "bigBlueCalf") {
      const rate = moving ? 4.2 : 2.6;
      return Math.floor(u.bob * rate) % n;
    }
    const base =
      u.classId === "horror" || u.classId === "asherah" || u.classId === "troll" || u.classId === "ancientGolem"
        ? 2.0
        : u.sprite === "defaultWarrior" || u.sprite === "kaelEarly" || u.classId === "mage" || u.classId === "cultist" || u.classId === "cultistV2" || u.classId === "healer"
          ? 1.7
          : isBossClass(u.classId)
            ? 1.75
            : 1.85;
    // Conjurer sheets (and Malrec's own, the same 36-frame data) are intentionally 10%
    // slower without slowing turn or spell logic.
    const animationRate = u.sprite === "travelingMerchant" ? 0.45 : u.sprite === "conjurer" || u.sprite === "malrec" ? 0.9 : 1;
    const rate = base * (moving ? 2.2 : 1) * animationRate;
    if (moving || this.reducedMotion) return Math.floor(u.bob * rate) % n;
    const cycle = Math.max(2, n * 2 - 2);
    const pace = (n >= LONG_SHEET_FRAMES ? n / LONG_ANIM_SECONDS : cycle / 2.6) * animationRate;
    const x = Math.floor(u.bob * pace) % cycle;
    return x < n ? x : cycle - x;
  }

  /** Long sheets (see LONG_SHEET_FRAMES): stretches an attack/cast's whole clock so its
   * sheet lasts LONG_ANIM_SECONDS, the same uniform dt scaling speedMode already uses, so
   * the hit, damage and FX stay in sync with the pose — they just happen later. Never makes
   * anything faster than the chosen speedMode; returns 1 for every short sheet. */
  private longSheetActionPace(a: Active, speedScale: number): number {
    let frames: unknown[] | undefined;
    let span: number;
    let seconds = LONG_ANIM_SECONDS;
    if (a.type === "combat") {
      if (a.stage === "fade") return 1;
      const counter = a.stage.startsWith("counter");
      // Already played in full during its wind-up (see startSeq) — no stretching on top.
      if (a.held && !counter) return 1;
      if (counter && a.counterWindAt != null) return 1;
      const sprite = this.units.find((u) => u.id === (counter ? a.def : a.att))?.sprite;
      if (!sprite) return 1;
      if (sprite === "big-blue-ox-002") seconds /= BIG_BLUE_OX_PACE;
      if (sprite === "kaelFinal") seconds = KAEL_FINAL_ATTACK_SECONDS;
      if (sprite === "apparition") seconds = APPARITION_ATTACK_SECONDS;
      if (sprite === "minor-horror-001") seconds = MINOR_HORROR_SECONDS.attack;
      frames = (this.offHandStrike(a) ? this.art.attacksShort[sprite] : undefined) ?? (counter ? this.art.counters[sprite] : undefined) ?? this.art.attacks[sprite];
      // stepCombat's lunge + hit + recover clocks, which attackPose spreads the sheet across.
      span = 0.2 + 0.18 + 0.16;
    } else if (a.type === "spell" || a.type === "heal") {
      if (a.held) return 1;
      const sprite = this.units.find((u) => u.id === a.att)?.sprite;
      if (!sprite) return 1;
      if (sprite === "big-blue-ox-002") seconds /= BIG_BLUE_OX_PACE;
      if (sprite === "minor-horror-001") seconds = MINOR_HORROR_SECONDS.cast;
      frames = this.castFrames(sprite, isSupportCast(a));
      // attackPose's castDuration.
      span = sprite === "conjurer" || sprite === "malrec" ? 0.65 : 0.4;
    } else return 1;
    const n = frames?.length ?? 0;
    if (n < LONG_SHEET_FRAMES) return 1;
    return Math.min(1, span / speedScale / seconds);
  }

  /** After a wind-up: carry on from where it stopped (a bow's follow-through after the
   * arrow left), then hold the last frame. */
  private heldFrame(a: { heldFrom?: number; heldAt?: number }, n: number): number {
    const played = (a.heldFrom ?? LONG_ANIM_SECONDS) + (this.time - (a.heldAt ?? this.time));
    return Math.min(n - 1, Math.floor((played / LONG_ANIM_SECONDS) * n));
  }

  /** Whether a wound-up step's remaining sheet (see heldFrame) has finished playing. */
  /** Whether the current stage of this attack is struck with the off-hand dagger/katar: the
   * attacker's own off-hand attack (customDice) or a defender's off-hand counter. */
  private offHandStrike(a: CombatAnim): boolean {
    return a.stage.startsWith("counter") ? !!a.counterCustomDice : !!a.customDice;
  }

  private heldDone(a: { held?: boolean; heldFrom?: number; heldAt?: number }): boolean {
    if (!a.held) return true;
    return (a.heldFrom ?? LONG_ANIM_SECONDS) + (this.time - (a.heldAt ?? this.time)) >= LONG_ANIM_SECONDS;
  }

  private attackPose(u: Unit): number | null {
    const a = this.active;
    if (!a) return null;
    // Long-sheet wind-up (see startSeq): the whole sheet, start to finish, over its duration.
    if (a.type === "windup") {
      if (a.id !== u.id) return null;
      const frames =
        a.pose === "cast"
          ? this.castFrames(u.sprite, !!a.heal)
          : a.pose === "specialAttack"
            ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite])
          : u.classId !== "bigBlueCalf" && u.sprite !== "neera" && u.idleAlt
            ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite])
            : this.art.attacks[u.sprite];
      const n = frames?.length ?? 0;
      if (n < 1) return null;
      return Math.min(n - 1, Math.floor((a.t / (u.sprite === "minor-horror-001" ? MINOR_HORROR_SECONDS.cast : LONG_ANIM_SECONDS)) * n));
    }
    // Visual-only pacing: the Conjurer holds each authored pose 10% longer. Every stage
    // duration below (cast lead-in and the lunge/hit/recover splits) has to scale by the
    // same factor as animationT, or the frame index caps out at 90% of a stage's range
    // and jumps to the next stage's start the instant the real timer crosses over — a
    // visible skip, and a jump straight to a differently-cropped frame if the sheet's
    // per-frame crop isn't perfectly uniform (the "size change" this was causing).
    const pace = u.sprite === "conjurer" || u.sprite === "malrec" ? 0.9 : 1;
    const animationT = a.t * pace;
    // A dedicated cast pose (currently just Birolho's cast-*.png), for a spell or heal only —
    // falls back to the melee attacks cut for every sprite without one, same as before this
    // existed. Checked first so a caster with both never mixes an index meant for one pool's
    // frame count into the other.
    if ((a.type === "spell" || a.type === "heal") && a.att === u.id) {
      const neeraArrowSkill = u.sprite === "neera" && a.type === "spell" && (a.spellKind === "longShot" || a.spellKind === "bloodyShot" || a.spellKind === "multiShot" || a.spellKind === "piercing");
      // The Carnivorous Plant's tendril swipe is her ATT, not a cast: it plays her attack sheet.
      const castFrames = neeraArrowSkill ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite]) : a.type === "spell" && a.spellKind === "tendrilSwipe" ? this.art.attacks[u.sprite] : this.castFrames(u.sprite, isSupportCast(a));
      if (!castFrames || castFrames.length < 3) return null;
      const n = castFrames.length;
      // Familiar Titã goes back to his idle loop once his cast sheet has played, instead of
      // holding its last frame while the spell flies.
      if (a.held && u.sprite === "familiar3" && this.heldDone(a)) return null;
      if (a.held) return this.heldFrame(a, n);
      if (n === 4) {
        if (animationT < 0.12) return 0;
        if (animationT < 0.22) return 1;
        if (animationT < 0.4) return 2;
        return 3;
      }
      // The Conjurer's authored 36-frame casting sequence needs a readable lead-in;
      // other casters retain the established timing. Malrec's own cast-*.png is a copy
      // of that same sequence, so it needs the same lead-in.
      const castDuration = u.sprite === "conjurer" || u.sprite === "malrec" ? 0.65 : 0.4;
      return Math.min(n - 1, Math.floor(Math.min(0.99, animationT / castDuration) * n));
    }
    if (a.type === "combat") {
      // Bull Rush has its own charge animation and impact FX (rushTrail/rushImpact) —
      // it never plays the ATT swing sheet, unlike every other combat step.
      if (a.spellKind === "bullRush" && u.classId !== "bigBlueCalf") return null;
      const counter = a.stage.startsWith("counter");
      const actor = counter ? a.def : a.att;
      if (u.id !== actor) return null;
      // Familiar 3's second, distinct attack cut (currently the only sprite with one) —
      // Unit.idleAlt (the same once-per-turn flip Malrec's idles2 uses) alternates it in for
      // its own attack stages, same idea as idles2 but for the swing instead of the stand.
      const attackPool = u.classId === "bigBlueCalf"
        ? (a.spellKind === "bullRush" && !counter ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite]) : this.art.attacks[u.sprite])
        : u.sprite !== "neera" && u.idleAlt ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite]) : this.art.attacks[u.sprite];
      // A dedicated counter pose (currently just theButcher's counter-*.png) for the
      // defender's stages only — falls back to the same attacks cut every sprite without
      // one already used for countering, same as before this existed.
      const short = this.offHandStrike(a) ? this.art.attacksShort[u.sprite] : undefined;
      const frames = short ?? (counter ? this.art.counters[u.sprite] : undefined) ?? attackPool;
      if (!frames || frames.length < 4) return null;
      const n = frames.length;
      if (a.held && !counter) return this.heldFrame(a, n);
      if (counter && a.counterWindAt != null) return Math.min(n - 1, Math.floor(((this.time - a.counterWindAt) / LONG_ANIM_SECONDS) * n));
      const long = n >= 12;
      // stepCombat's real per-stage clocks (see lunge/impactAt/recover there): 0.2s lunge,
      // 0.18s hit, 0.16s recover, same for every sprite. animationT runs at `pace` of real
      // time, so the divisor has to run at that same pace — otherwise a stage ends in real
      // time before animationT/divisor ever reaches 1, and the index jumps straight to the
      // next stage's start instead of finishing this one's frames.
      const lungeDur = 0.2 * pace;
      const hitDur = 0.18 * pace;
      const recoverDur = 0.16 * pace;
      if (long) {
        // Long authored sheets use the whole motion: half for the wind-up, then a quarter
        // for impact and a quarter for recovery. This keeps legacy 12-frame cuts identical
        // while allowing the Conjurer's 36-frame cast/attack to play in full.
        const lungeN = Math.max(2, Math.round(n * 0.5));
        const hitN = Math.max(2, Math.round(n * 0.25));
        const hitStart = lungeN;
        const recoverStart = Math.min(n - 1, hitStart + hitN);
        if (a.stage === "lunge" || a.stage === "counterLunge") return Math.min(lungeN - 1, Math.floor((animationT / lungeDur) * lungeN));
        if (a.stage === "hit" || a.stage === "counterHit") return Math.min(recoverStart - 1, hitStart + Math.floor((animationT / hitDur) * hitN));
        if (a.stage === "recover" || a.stage === "counterRecover") return Math.min(n - 1, recoverStart + Math.floor((animationT / recoverDur) * (n - recoverStart)));
        return n - 1;
      }
      // Short sets: the classic cut is one frame per stage (0-1 lunge, 2 hit, 3 recover).
      // Anything between 5 and 11 frames — the familiar's 8 — walks the same three stages
      // across every frame it has instead of stopping at index 3 and wasting the rest.
      const lungeEnd = Math.max(1, Math.round((n - 1) * 0.35));
      const hitEnd = Math.max(lungeEnd + 1, Math.round((n - 1) * 0.6));
      const span = (from: number, to: number, prog: number) =>
        Math.min(to, from + Math.floor(Math.max(0, Math.min(0.999, prog)) * (to - from + 1)));
      if (a.stage === "lunge" || a.stage === "counterLunge") return span(0, lungeEnd, animationT / lungeDur);
      if (a.stage === "hit" || a.stage === "counterHit") return span(lungeEnd + 1, hitEnd, animationT / hitDur);
      if (a.stage === "recover" || a.stage === "counterRecover") return span(hitEnd + 1, n - 1, animationT / recoverDur);
      return n - 1;
    }
    return null;
  }

  private liveMotion(u: Unit, cell: number): { bob: number; sway: number; breath: number } {
    if (!u.alive || this.reducedMotion) return { bob: 0, sway: 0, breath: 0 };
    const t = u.bob;
    if (u.classId === "familiar" || u.classId === "familiar2") {
      return {
        bob: Math.sin(t * 1.6) * 2.4,
        sway: Math.sin(t * 0.9) * 0.7,
        breath: 0.02 + Math.sin(t * 1.6) * 0.02,
      };
    }
    if (u.classId === "wardog" || u.classId === "swampBlueCalf" || u.classId === "bigBlueCalf") {
      return {
        bob: Math.sin(t * 2.2) * 1.15,
        sway: 0,
        breath: 0.014 + Math.sin(t * 2.2) * 0.018,
      };
    }
    const heavy = u.size >= 4 ? 1.4 : u.size === 2 ? 1.12 : 1;
    if (u.sprite === "defaultWarrior" || u.sprite === "kaelEarly" || u.sprite === "aldric" || u.sprite === "defaultLancer" || u.sprite === "lancer" || u.sprite === "sandoval" || u.sprite === "conjurer" || u.sprite === "malrec" || u.sprite === "salazar" || u.size >= 4) {
      return { bob: 0, sway: 0, breath: 0 };
    }
    const bob = Math.sin(t * 1.55) * (1.15 * heavy);
    const sway = Math.sin(t * 0.85 + 0.3) * (cell * 0.008 * heavy);
    const breath = 0.012 + Math.sin(t * 1.55) * 0.014;
    return { bob, sway, breath };
  }

  /** Every property of a unit's current animated pose — pose selection (idle/walk/atk/cast/
   * counter), the size/scale corrections tied to whichever pose that turns out to be, and the
   * live idle-motion (bob/sway/breath) and high-ground lift on top — computed once here so
   * renderUnitsAndOverlays and ThreeBattleRenderer (via the public unitVisual() wrapper below)
   * can never drift apart into two separate copies of this logic. Ported verbatim from what
   * used to be inlined in renderUnitsAndOverlays's own per-unit loop; see that method's history
   * for the reasoning behind each individual correction. */
  private computeUnitVisual(u: Unit, cell: number, tile: number): UnitVisual {
    this.applyHeading(u);
    // Sprites load per battle (see ensureSpriteArt in assets.ts). Anything that still reaches
    // the board without its art (a map-editor preview, a mid-battle addition) requests it here
    // and draws as soon as it lands.
    if (!this.art.sprites[u.sprite]) void requestSpriteArt(this.art, u.sprite);
    // Undead Ox occupies four hexes but uses the size-2 visual box so it doesn't render
    // at the giant size-4 scale used by creatures like Rocco.
    const s = u.classId === "undeadOx" ? 2 : unitSize(u);
    const boss = isBossClass(u.classId);
    const { bob, sway, breath } = this.liveMotion(u, cell);
    const lift = this.unitLift(u, cell);
    const atk = this.attackPose(u);
    const moving = this.active?.type === "move" && this.active.id === u.id;
    // Reading a pose must never change the last direction established by combat/movement.
    // idleAlt flips once per this unit's own turn (see beginUnitTurn) — a sprite with a
    // second idle loop (currently just Malrec's idles2) alternates into it; everyone else
    // has no idles2 entry, so this is a no-op fallback to their regular idle/stand pool.
    const idlePool = u.idleAlt ? (this.art.idles2[u.sprite] ?? this.art.idles[u.sprite]) : this.art.idles[u.sprite];
    const idle = !atk && !moving ? idlePool : undefined;
    // While moving, a sprite that has a walk cut plays it; one that doesn't falls back to
    // its idle loop, which idleFrame already runs faster for a moving unit.
    const faceRight = u.facing === 1;
    // Lancer's authored move/move-left cuts read backwards against their own facing
    // (moving right visibly played the left-facing footage and vice versa) — swap which
    // pool answers which facing, walk only, per direct report. Cultist V2's own walk
    // "backwards" complaint has a different cause: see dirActionWalk below.
    const useWalkLeft = u.sprite === "lancer" ? faceRight : !faceRight;
    // Cosmetic alternate walk (GameArt.walks2), picked once per move, about one move in three.
    const altWalk = moving && !!this.art.walks2[u.sprite] && this.walkAltFor(this.active!);
    const sideWalkPool = altWalk
      ? (useWalkLeft ? (this.art.walksLeft2[u.sprite] ?? this.art.walks2[u.sprite]) : this.art.walks2[u.sprite])
      : useWalkLeft ? (this.art.walksLeft[u.sprite] ?? this.art.walks[u.sprite]) : this.art.walks[u.sprite];
    // Moving up the map (walkPose "back") or down it ("front") plays that direction's own
    // cycle for sprites that have one (GameArt.walksUp/walksDown); everyone else keeps the
    // side walk in every direction.
    const walkPool =
      (u.walkPose === "back" ? this.art.walksUp[u.sprite] : u.walkPose === "front" ? this.art.walksDown[u.sprite] : undefined) ?? sideWalkPool;
    // Same idleAlt alternation attackPose applies to pick its index (see that function's
    // attackPool) — mirrored here so the frame actually drawn comes from the same array.
    const oxRush = u.classId === "bigBlueCalf" && this.active?.type === "combat" && this.active.spellKind === "bullRush" && this.active.att === u.id && !this.active.stage.startsWith("counter");
    const neeraArrowSkill = u.sprite === "neera" && (
      (this.active?.type === "spell" && this.active.att === u.id && (this.active.spellKind === "longShot" || this.active.spellKind === "bloodyShot" || this.active.spellKind === "multiShot" || this.active.spellKind === "piercing")) ||
      (this.active?.type === "windup" && this.active.id === u.id && this.active.pose === "specialAttack")
    );
    const atkBase = neeraArrowSkill
      ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite])
      : u.classId === "bigBlueCalf"
        ? (oxRush ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite]) : this.art.attacks[u.sprite])
        : u.sprite !== "neera" && u.idleAlt ? (this.art.attacks2[u.sprite] ?? this.art.attacks[u.sprite]) : this.art.attacks[u.sprite];
    const offHandSwing =
      this.active?.type === "combat" &&
      (this.active.stage.startsWith("counter") ? this.active.def : this.active.att) === u.id &&
      this.offHandStrike(this.active);
    const atkShort = offHandSwing ? this.art.attacksShort[u.sprite] : undefined;
    const atkPool = atkShort ?? (faceRight ? atkBase : (neeraArrowSkill ? (this.art.attacks2Left[u.sprite] ?? atkBase) : (this.art.attacksLeft[u.sprite] ?? atkBase)));
    const walk = atk == null && moving ? walkPool : undefined;
    // attackPose computes its index against whichever pool it picked (casts for a spell/heal
    // cast, counters for the defender's own counter stages, attacks otherwise), so this has
    // to mirror that same choice or the index lands in the wrong array.
    const casting =
      this.active &&
      (((this.active.type === "spell" || this.active.type === "heal") && this.active.att === u.id && !(this.active.type === "spell" && this.active.spellKind === "tendrilSwipe")) ||
        (this.active.type === "windup" && (this.active.pose === "cast" || this.active.pose === "specialAttack") && this.active.id === u.id));
    const healing =
      !!this.active &&
      (((this.active.type === "spell" || this.active.type === "heal") && this.active.att === u.id && isSupportCast(this.active)) ||
        (this.active.type === "windup" && this.active.id === u.id && !!this.active.heal));
    const castPool = healing && this.art.castsHeal[u.sprite]
      ? this.art.castsHeal[u.sprite]
      : neeraArrowSkill
      ? faceRight
        ? this.art.attacks2[u.sprite]
        : (this.art.attacks2Left[u.sprite] ?? this.art.attacks2[u.sprite])
      : faceRight
        ? this.art.casts[u.sprite]
        : (this.art.castsLeft[u.sprite] ?? this.art.casts[u.sprite]);
    const countering = this.active?.type === "combat" && this.active.stage.startsWith("counter") && this.active.def === u.id;
    const counterPool = faceRight ? this.art.counters[u.sprite] : (this.art.countersLeft[u.sprite] ?? this.art.counters[u.sprite]);
    // Hit reaction (GameArt.hits): plays for HIT_ANIM_SECONDS after taking damage, unless the
    // unit is attacking or walking. On a killing blow it plays first, then the death sheet.
    const hitPool = this.hitPoolFor(u);
    const oxPosePace = u.classId === "bigBlueCalf" ? BIG_BLUE_OX_PACE : 1;
    // The Ox hit cut ends at source frame 75 instead of 83; retain its playback pace.
    const hitSeconds = HIT_ANIM_SECONDS / oxPosePace * (u.classId === "bigBlueCalf" ? 75 / 83 : 1);
    const deathSeconds = u.classId === "minorHorror" ? MINOR_HORROR_SECONDS.death : DEATH_ANIM_SECONDS / oxPosePace;
    const sinceHit = hitPool && u.hitAt != null ? this.time - u.hitAt : Infinity;
    const directOxDeath = u.classId === "bigBlueCalf" && !u.alive && this.art.deaths[u.sprite] != null;
    const hitPlaying = !directOxDeath && sinceHit < hitSeconds && (!u.alive || (atk == null && !moving));
    const deathPool = !hitPlaying && !u.alive && u.diedAt != null ? ((u.deathAlt ? this.art.deaths2[u.sprite] : undefined) ?? this.art.deaths[u.sprite]) : undefined;
    const deathT = u.diedAt != null ? this.time - u.diedAt - (hitPool && !directOxDeath ? HIT_ANIM_SECONDS : 0) : 0;
    const frames = hitPlaying ? hitPool : deathPool ?? (atk != null ? (casting ? (castPool ?? atkPool) : countering ? (counterPool ?? atkPool) : atkPool) : walk ?? idle ?? this.art.sprites[u.sprite]);
    const n = frames?.length ?? 0;
    const fi = hitPlaying
      ? Math.min(n - 1, Math.floor((sinceHit / hitSeconds) * n))
      : deathPool
        ? Math.min(n - 1, Math.max(0, Math.floor((deathT / deathSeconds) * n)))
        : atk != null ? atk : walk ? this.walkFrame(u, n) : this.idleFrame(u, n || 4);
    const walkDirs = moving ? this.art.walkDirs[u.sprite] : undefined;
    const img = (walkDirs ? walkDirs[u.walkPose] : undefined) ?? frames?.[fi] ?? frames?.[0];
    // The draw-size correction keys off the footprint SHAPE (reference equality against
    // FOOTPRINT_TYPE_8 or FOOTPRINT_TYPE_7), not a hardcoded classId — every big creature
    // (Troll, Asherah, Horror, and any future one on either shape) gets the same default
    // correction automatically, rather than needing its own one-off case added here.
    // Depends on that creature's own sprite frames being cropped to roughly the same
    // canvas-fill ratio as the others — this correction assumes that, it doesn't measure it.
    const isBigCreatureFootprint = u.footprintOffsets === FOOTPRINT_TYPE_8 || u.footprintOffsets === FOOTPRINT_TYPE_7;
    const isLancer = u.classId === "lancer" || u.sprite === "lancer" || u.sprite === "defaultLancer";
    const isSandoval = u.classId === "sandoval" || u.sprite === "sandoval";
    const isFamiliar = u.classId === "familiar" || u.sprite === "familiar";
    const isKaelFinal = u.sprite === "kaelFinal";
    const isCultistV2 = u.classId === "cultistV2" || u.sprite === "cultist-v2";
    const isNeera = u.sprite === "neera";
    const isSoldier = u.sprite === "soldier";
    const spriteScale = isLancer ? 1.4 : isSandoval ? 1.2 : isFamiliar ? 0.5 : isKaelFinal ? 0.9 : isCultistV2 ? 0.98 : isNeera ? 0.9 : isSoldier ? 0.9 : 1;
    const familiar2WidthMul = u.sprite === "familiar2" ? 2.544 : 1;
    const familiar2WalkScale = u.sprite === "familiar2" && walk ? 0.97 : 1;
    const isCultistV2Casting = isCultistV2 && casting;
    const cultistV2CastHeightMul = isCultistV2Casting ? 1.24 : 1;
    const cultistV2CastWidthMul = isCultistV2Casting ? 1.06 : 1;
    const isCultistV2Attacking = isCultistV2 && atk != null && !isCultistV2Casting;
    const cultistV2AtkScale = isCultistV2Attacking ? 1.13 : 1;
    const cultistV2WalkScale = isCultistV2 && walk ? 1.02 : 1;
    // Kael Final's atk-*.png sheet is exported on a bigger, more-padded canvas than his
    // stand-*.png idle sheet (432x640 vs 354x528, character filling ~90% of frame height on
    // average during the swing vs ~98% standing) — since drawImage stretches the WHOLE sheet
    // into the same fixed on-screen box regardless of the sheet's own resolution, that extra
    // padding alone reads as him visibly shrinking the instant ATT starts and popping back on
    // return to idle. Measured via alpha-bbox fill ratio (idle vs a spread of atk frames,
    // matched on occupied-area so this doesn't stretch him, just restores his idle size) —
    // see scratchpad measure_bbox.py from the neera/kael-final shrink report.
    const isKaelFinalAttacking = isKaelFinal && atk != null;
    // 1.07 (an average over the swing) still left him visibly smaller as ATT starts: his upright
    // atk-1 fills 86.4% of its 432x640 canvas vs idle's 98.1% of 354x528 — 98.1 / 86.4 = 1.135.
    const kaelFinalAtkScale = isKaelFinalAttacking ? 1.135 : 1;
    // Same root cause as Kael Final above: Neera's atk-*.png (360x572) and cast-*.png
    // (360x520) sheets both carry noticeably more padding around her than her idle stand
    // sheet (318x556) does — atk fills ~66% of canvas width on average vs idle's ~84%, cast
    // is padded even further. Two separate corrections because the two sheets are padded by
    // different amounts (measured area-fill ratio, same method as Kael Final's above).
    const isNeeraCasting = isNeera && casting;
    const isNeeraAttacking = isNeera && atk != null && !isNeeraCasting;
    const neeraAtkScale = isNeeraAttacking ? 1.14 : 1;
    const neeraCastScale = isNeeraCasting ? 1.18 : 1;
    const familiar3Scale = u.classId === "familiar3" ? 1.4 : 1;
    // Titan V2 is a wide 16:9 creature frame, so give Familiar 3 its natural
    // horizontal footprint rather than squeezing the silhouette into the old square box.
    const familiar3WidthScale = u.classId === "familiar3" ? 1.9 : 1;
    // BirolhoLegs/BirolhoLegs2 ship on wide padded canvases (906x647 / 1280x705, creature
    // ~75% of canvas height) instead of birolho3's tight crop — these bring the creature to
    // birolho3's on-screen height and keep the canvas's own aspect instead of squeezing it.
    const birolhoLegsHeightScale = u.sprite === "BirolhoLegs" ? 1.32 : u.sprite === "BirolhoLegs2" ? 1.29 : 1;
    const birolhoLegsWidthScale = u.sprite === "BirolhoLegs" ? 2.17 : u.sprite === "BirolhoLegs2" ? 2.75 : 1;
    // troll2 (472x360 canvas, figure ~94% of its height): the troll's on-screen height, with
    // the canvas's own aspect kept instead of squeezed into the tall creature box.
    const troll2HeightScale = u.sprite === "troll2" ? 1.03 : 1;
    const troll2WidthScale = u.sprite === "troll2" ? 1.59 : 1;
    // WarDog 2 (560x340 canvas, figure ~94% of its height): the War Dog's on-screen figure
    // height (~84% of its square box), with the wide canvas's own aspect kept.
    const wardog2HeightScale = u.sprite === "wardog2" ? 0.896 : 1;
    const wardog2WidthScale = u.sprite === "wardog2" ? 1.372 : 1;
    // Embered Wraith (292x360 canvas, figure ~95% of its height, same fill as the plain human
    // sheets): human height, width follows the canvas aspect (0.811 / 0.782).
    const wraithWidthScale = u.sprite === "minor-horror-001" ? 1.163 : u.sprite === "EmberedWraith" ? 1.037 : 1;
    // Zombie Dog (437x321 canvas, figure ~80% of its height): the dogs' on-screen figure height
    // (~84% of the size-2 box, same as WarDog 2), with the canvas's own aspect kept.
    const zombieDogHeightScale = u.sprite === "zombieDog" ? 1.051 : 1;
    const zombieDogWidthScale = u.sprite === "zombieDog" ? 1.33 : 1;
    // Its hit-*.png and new death-*.png are cut on a wider 528x321 canvas (same height, same
    // figure scale) so the tail swing fits — widen the box by the same ratio (528/437) for
    // those two sheets only, so the dog stays exactly its usual size. death2-*.png (the old
    // cut) keeps the regular 437x321 canvas.
    const zombieDogWideSheet = u.sprite === "zombieDog" && (hitPlaying || (!!deathPool && deathPool === this.art.deaths[u.sprite]));
    const zombieDogWideSheetScale = zombieDogWideSheet ? 528 / 437 : 1;
    // Rocco The Bird (639x360 canvas, figure ~79% of its height): troll2's on-screen figure
    // height (same Type 7 body), with the wide canvas's own aspect kept.
    const roccoHeightScale = u.sprite === "RoccoTheBird" ? 1.22 : 1;
    const roccoWidthScale = u.sprite === "RoccoTheBird" ? 2.55 : 1;
    // Carnivorous Plant (640x360 canvas, figure ~93.6% of its height): Rocco's on-screen figure
    // height (same Type 7 body), with the wide canvas's own aspect kept.
    const plantHeightScale = u.sprite === "carnivorous-plant-001" ? 1.03 : 1;
    const plantWidthScale = u.sprite === "carnivorous-plant-001" ? 2.15 : 1;
    // Sapling (611x360 canvas, standing figure 363 px wide): one hex, drawn filling its whole hex
    // (figure width = one hex width), with the canvas's own aspect kept.
    const saplingHeightScale = u.sprite === "sapling-001" ? 0.582 : 1;
    const saplingWidthScale = u.sprite === "sapling-001" ? 1.264 : 1;
    // familiar4 (1302x620 canvas, figure ~90% of its height): Familiar Maior's on-screen
    // height, with the wide canvas's own aspect kept.
    const familiar4HeightScale = u.sprite === "familiar4" ? 0.97 : 1;
    const familiar4WidthScale = u.sprite === "familiar4" ? 2.61 : 1;
    // Mordavian Wolf Final: same 3:2 sheet and figure fill as the old wolf, but drawn at the
    // sheet's own aspect instead of squeezed into the size-2 box (1.5 / 1.0756).
    const wolfFinalWidthScale = u.sprite === "mordavian-wolf-final" ? 1.395 : 1;
    // Zombie (480x360 canvas, figure ~95% of its height, same fill as the plain human sheets):
    // height stays the human box, width follows the canvas aspect (1.333 / 0.782) so the wide
    // lunge frames aren't squeezed. Idle, walk and ATT share this one canvas, so nothing shrinks.
    const zombieWidthScale = u.sprite === "zombie" ? 1.705 : u.sprite === "zombie2" ? 1.085 : 1;
    // Undead Ox is 15% larger than its previous size; keep Plague Bearing Cattle unchanged.
    // Plague Bearing Cattle: same 640x404 canvas, ground line and standing fill as the Undead Ox.
    const undeadOxScale = u.classId === "undeadOx" ? 0.75 * 1.1 * 1.15 : 1;
    const undeadOxHeightScale = (u.sprite === "big-blue-ox-002" ? 0.85 : u.sprite === "undeadOx" || u.sprite === "plague-bearing-cattle" ? 1.283 : 1) * undeadOxScale;
    const undeadOxWidthScale = (u.sprite === "big-blue-ox-002" ? 1.49 : u.sprite === "undeadOx" || u.sprite === "plague-bearing-cattle" ? 1.889 : 1) * undeadOxScale;
    let h =
      cell *
      (s >= 4 ? 3.35 : s === 2 ? 1.72 : boss ? 1.44 : 1.42) *
      1.2 *
      (isBigCreatureFootprint ? 0.75 : 1) *
      spriteScale *
      cultistV2CastHeightMul *
      cultistV2AtkScale *
      cultistV2WalkScale *
      familiar3Scale *
      familiar2WalkScale *
      birolhoLegsHeightScale *
      troll2HeightScale *
      wardog2HeightScale *
      zombieDogHeightScale *
      undeadOxHeightScale *
      roccoHeightScale *
      plantHeightScale *
      saplingHeightScale *
      familiar4HeightScale *
      kaelFinalAtkScale *
      neeraAtkScale *
      neeraCastScale;
    let w =
      cell *
      (s >= 4 ? 2.85 : s === 2 ? 1.85 : boss ? 1.12 : 1.11) *
      1.2 *
      (isBigCreatureFootprint ? 0.75 : 1) *
      spriteScale *
      familiar2WidthMul *
      familiar2WalkScale *
      cultistV2CastWidthMul *
      cultistV2AtkScale *
      cultistV2WalkScale *
      familiar3Scale *
      familiar3WidthScale *
      birolhoLegsWidthScale *
      troll2WidthScale *
      wardog2WidthScale *
      wraithWidthScale *
      zombieDogWidthScale *
      zombieDogWideSheetScale *
      roccoWidthScale *
      plantWidthScale *
      saplingWidthScale *
      familiar4WidthScale *
      wolfFinalWidthScale *
      zombieWidthScale *
      undeadOxWidthScale *
      kaelFinalAtkScale *
      neeraAtkScale *
      neeraCastScale;
    // Neera V2 exports use different canvas padding. Keep a fixed source-pixel scale
    // per sheet, measured from its first upright hood-to-boot pose (not the bow).
    // Weapons and leaning poses can change bounds without resizing her body per frame.
    const neeraV2Sheet = img?.src.includes("/neera-v2-001/")
      ? img.src.match(/\/(idle|atk2|atk-short|atk|move)-(?:left-)?\d+\.png/)?.[1] : undefined;
    if (neeraV2Sheet && img) {
      const standingBodyPixels: Record<string, number> = { idle: 800, atk: 672, atk2: 755, "atk-short": 675, move: 471 };
      // 1.53 cells: middle of Kael, Aldric, Salazar and Voss's visible human heights.
      const worldPerPixel = cell * 1.53 / standingBodyPixels[neeraV2Sheet]!;
      h = img.naturalHeight * worldPerPixel;
      w = img.naturalWidth * worldPerPixel;
    }
    // Milícia V2: every sheet (idle/hit/death/walks) is cut by work/militia_v2_build.py onto one
    // shared 534x762 canvas at one scale, standing body 628 px tall, feet at y 698. Same human
    // height as Neera V2 above (1.53 cells), so he matches the other humans by default.
    const isMiliciaV2 = u.sprite === "militia-v2";
    const miliciaV2PerPixel = cell * 1.53 / 628;
    if (isMiliciaV2 && img) {
      h = img.naturalHeight * miliciaV2PerPixel;
      w = img.naturalWidth * miliciaV2PerPixel;
    }
    // Apparition: idle/cast/walk/ATT share one TEK crop box (369x637, work/apparition/build.py),
    // standing body 597 px, the same 1.53-cell human height as Neera V2 / Milícia V2. Her standing
    // feet sit 34 px above the box bottom (room for the ATT stepping toward the camera).
    const apparitionPerPixel = cell * 1.53 / 597;
    if (u.sprite === "apparition" && img) {
      h = img.naturalHeight * apparitionPerPixel;
      w = img.naturalWidth * apparitionPerPixel;
    }
    // Jacaré: every sheet shares one TEK crop box (814x282, work/jacare/build.py), centred on its
    // feet, feet 13 px above the bottom. Its idle body, tail included, is 581 px long — drawn
    // 3.4 cells long over its 3-hex (Type 3 Long) footprint, bigger than the Mordavian Wolf.
    const jacarePerPixel = cell * 3.4 / 581;
    if (u.sprite === "jacare" && img) {
      h = img.naturalHeight * jacarePerPixel;
      w = img.naturalWidth * jacarePerPixel;
    }
    // Malrec: each sheet was exported at its own scale (body incl. staff, frame 1, measured: idle
    // 296 px of a 256x320 canvas, ATT 244 px with feet 38 px up, cast 312 px, walks on bigger
    // canvases). One fixed scale per sheet keeps his body the idle's size in every pose, and
    // the feet on the idle's ground line (feet 12 px above the idle canvas bottom).
    const malrecSheet = u.sprite === "malrec" ? img?.src.match(/\/malrec\/(idle2-|atk-|cast-|move-left-|move-)?\d+\.png/)?.[1] ?? (img?.src.includes("/malrec/") ? "" : undefined) : undefined;
    const malrecPose = malrecSheet !== undefined ? MALREC_SHEET_PX[malrecSheet] : undefined;
    const malrecPerPixel = malrecPose ? cell * MALREC_BODY_CELLS / malrecPose.body : 0;
    if (malrecPose && img) {
      h = img.naturalHeight * malrecPerPixel;
      w = img.naturalWidth * malrecPerPixel;
    }
    // Salazar V2: every sheet is rescaled onto one shared 416x613 canvas (salazar-final/), idle
    // body 480 px head to feet, feet 8 px above the bottom. Same 1.53-cell human height.
    const isSalazarV2 = u.sprite === "salazar";
    const salazarV2PerPixel = cell * 1.53 / 480;
    if (isSalazarV2 && img) {
      h = img.naturalHeight * salazarV2PerPixel;
      w = img.naturalWidth * salazarV2PerPixel;
    }
    // The cast cut's own content also sits higher inside its canvas than idle/attack's does
    // (feet reach only ~87% of the way down vs idle's ~99%) — without this, boosting h above
    // would float the feet even further off the ground than they already subtly are. Shifts
    // the whole draw down by that measured gap so the feet land back on the anchor point.
    // Kael Final's atk sheet and Neera's atk/cast sheets each measured a smaller, consistent
    // version of the same gap (feet sitting a bit higher in their own canvas than idle's does)
    // — same fix, smaller correction.
    // Milícia V2's feet sit 64 px above his canvas bottom (room for the death fall); Neera V2's
    // sit 3 px above hers with no offset, so shift him down by the 61 px difference.
    const footOffset = neeraV2Sheet ? 0 : isMiliciaV2 ? 61 * miliciaV2PerPixel : isSalazarV2 ? 8 * salazarV2PerPixel : malrecPose ? (malrecPose.feet - 12) * malrecPerPixel : u.sprite === "apparition" ? 34 * apparitionPerPixel : u.sprite === "jacare" ? 13 * jacarePerPixel : isCultistV2Casting
      ? h * 0.127
      : isKaelFinalAttacking
        ? h * 0.025
        : isNeeraAttacking
          ? h * 0.042
          : isNeeraCasting
            ? h * 0.045
            : u.sprite === "familiar2"
              // The 36-frame cuts share one camera anchor so a tentacle stepping toward the
              // camera keeps room below; resting feet sit ~2.8% above the canvas bottom.
              ? h * 0.028
              : 0;
    // Big creatures plant their feet at the bottom corner of their front hex (tile * 0.9,
    // matching the hex outline radius used elsewhere) instead of the smaller offset tuned
    // for normal-size sprites, so the feet don't float above the tile they stand on.
    const footY = s >= 4 ? tile * 0.9 : cell * 0.42;
    // Dedicated left/right walk+attack cuts already face the enemy, so flipping
    // them would put the spear/staff on the wrong side. Idle still flips.
    const dirActionWalk = (u.sprite === "aldric" || u.sprite === "defaultLancer" || u.sprite === "lancer" || u.sprite === "sandoval" || u.sprite === "theButcher" || u.sprite === "familiar2" || u.sprite === "familiar3" || u.sprite === "cultist-v2" || u.sprite === "militia-v2" || u.sprite === "salazar" || u.sprite === "cobalt-blue-deer" || u.sprite === "neera") && moving;
    // Suppress mirroring only when the frame actually came from an authored left cut.
    // Having a left ATT cut must not suppress the mirror of casts, counters or off-hand art.
    const dirActionAttack = atk != null && !!frames && (
      frames === this.art.attacksLeft[u.sprite] ||
      frames === this.art.attacks2Left[u.sprite] ||
      frames === this.art.castsLeft[u.sprite] ||
      frames === this.art.countersLeft[u.sprite]
    );
    const dirAction = dirActionWalk || dirActionAttack;
    // The familiar's art is drawn facing left by default — the opposite of every other
    // sprite's "facing 1 shows the sheet as drawn" convention — so its mirror has to run
    // backwards from u.facing or it walks left while visually facing right and vice versa.
    // defaultWarrior's kael-v2 stand cut was shot facing left but its atk-*.png cut was shot
    // facing right — see the identical comment this replaced in renderUnitsAndOverlays for
    // the full reasoning on both of these.
    const defaultWarriorIdleOrWalkReversed = u.sprite === "defaultWarrior" && atk == null;
    // Neera's move-*.png cut (WALK_FRAMES.neera in assets.ts) was shot facing left, same
    // situation as the familiar/defaultWarrior cases above, but only for the walk pose —
    // her idle/attack art faces right as normal. Without this she read backwards in BOTH
    // directions: facing right drew the raw left-facing footage unflipped (walked right
    // while visibly facing left), and facing left then mirrored that already-left-facing
    // footage into facing right (walked left while visibly facing right) — reported as
    // "two reverse walk" rather than the one intended mirror-for-left-only.
    // (Her walk now has authored Walk Right/Walk Left cuts and is never mirrored — see
    // dirActionWalk. Her idle must NOT be reversed: it is drawn facing the same way as her
    // ATT/Special art, so reversing it made her flip sides every time she went idle -> attack.)
    const neeraWalkReversed = u.sprite === "neera" && atk == null && moving;
    const deerFacingReversed = u.sprite === "cobalt-blue-deer";
    // Kael Final's idle (1..36.png) is drawn turned three-quarters to the LEFT, while his atk and
    // move cuts face right — so his idle must mirror the other way to face his enemy.
    const kaelFinalIdleReversed = u.sprite === "kaelFinal" && atk == null && walk == null;
    const facing = u.classId === "familiar" || defaultWarriorIdleOrWalkReversed || neeraWalkReversed || deerFacingReversed || kaelFinalIdleReversed ? -u.facing : u.facing;
    const flip = dirAction ? 1 : facing;
    // A fixed set of sprites skip the breath squash/stretch entirely (ctx.scale(flip, 1)) —
    // see the identical branch this replaced in renderUnitsAndOverlays.
    const noBreathScale =
      !!neeraV2Sheet ||
      u.sprite === "defaultWarrior" ||
      u.sprite === "kaelEarly" ||
      u.sprite === "aldric" ||
      u.sprite === "defaultLancer" ||
      u.sprite === "lancer" ||
      u.sprite === "sandoval" ||
      u.sprite === "conjurer" ||
      u.sprite === "malrec" ||
      u.sprite === "salazar";
    const scaleX = noBreathScale ? flip : flip * (1 - breath * 0.22);
    const scaleY = noBreathScale ? 1 : 1 + breath;
    return { img, w, h, footY, bob, sway, breath, lift, scaleX, scaleY, footOffset };
  }

  /** The cast-pose frames for this sprite: its healing sheet for a spell that deals no damage
   * (when it has one), else its cast sheet, else its attack swing. */
  private castFrames(sprite: SpriteId, heal: boolean): HTMLImageElement[] | undefined {
    return (heal ? this.art.castsHeal[sprite] : undefined) ?? this.art.casts[sprite] ?? this.art.attacks[sprite];
  }

  /** A monster's own attack cue (audio.ts MONSTER_SFX). When this swing plays its second attack
   * sheet (GameArt.attacks2 on an idleAlt turn — the same choice computeUnitVisual makes) and
   * the sprite has an "attack2" cue, that one plays; otherwise its regular "attack" cue. */
  private monsterAttackCue(u: Unit): boolean {
    const second = u.classId !== "bigBlueCalf" && u.sprite !== "neera" && u.idleAlt && !!this.art.attacks2[u.sprite];
    return (second && sfxPlay.monster(u.sprite, "attack2")) || sfxPlay.monster(u.sprite, "attack");
  }

  /** The hit sheet for the hit being played. A sprite with a second hit sheet (GameArt.hits2,
   * e.g. the Apparition) cycles them per hit taken: hit, hit, hit2, hit, hit, hit2, ... Each new
   * hitAt counts once, however many times a frame reads it. */
  private hitPoolFor(u: Unit): HTMLImageElement[] | undefined {
    const second = this.art.hits2[u.sprite];
    if (!second || u.hitAt == null) return this.art.hits[u.sprite];
    let seen = this.hitSheetCounts.get(u);
    if (!seen) this.hitSheetCounts.set(u, (seen = { hitAt: u.hitAt, count: 1 }));
    else if (seen.hitAt !== u.hitAt) { seen.hitAt = u.hitAt; seen.count += 1; }
    return seen.count % 3 === 0 ? second : this.art.hits[u.sprite];
  }

  /** Public wrapper around computeUnitVisual — ThreeBattleRenderer calls this every frame to
   * animate its own unit meshes (walk/attack/cast/counter poses, live idle motion) instead of
   * only ever showing a static idle frame, using the exact same pose/size logic
   * renderUnitsAndOverlays draws with on the Canvas2D-shim canvas. `tile` is the same
   * `ZOOM_RADII[this.zoom]` value render()/renderUnitsAndOverlays already key off. */
  unitVisual(u: Unit, tile: number): UnitVisual {
    return this.computeUnitVisual(u, tile * Math.sqrt(3), tile);
  }

  /** Persistent visual-code status FX: it tracks a unit, loops with engine time,
   * and needs no image, texture, or background. */
  private drawStatusFx(ctx: any, u: Unit, w: number, h: number): void {
    if (!u.poisoned && !u.diseased) return;

    const layer = (poison: boolean) => {
      const core = poison ? "105,238,116" : "176,92,246";
      const dark = poison ? "24,112,63" : "78,34,126";
      const speed = poison ? 0.72 : 0.48;
      const seed = (u.x * 1.73 + u.y * 2.41 + u.id.length * 0.37) % (Math.PI * 2);
      ctx.save();
      ctx.globalCompositeOperation = "lighter";
      const pulse = 0.58 + Math.sin(this.time * (poison ? 3.8 : 2.5) + seed) * 0.16;
      const haze = ctx.createRadialGradient(0, -h * 0.18, 0, 0, -h * 0.18, w * 0.48);
      haze.addColorStop(0, `rgba(${core},${0.12 * pulse})`);
      haze.addColorStop(0.55, `rgba(${dark},${0.055 * pulse})`);
      haze.addColorStop(1, `rgba(${dark},0)`);
      ctx.fillStyle = haze;
      ctx.beginPath();
      ctx.ellipse(0, -h * 0.18, w * 0.48, h * 0.18, 0, 0, Math.PI * 2);
      ctx.fill();

      // Motes loop from feet to head, so the status looks alive rather than like a cast.
      for (let i = 0; i < 8; i += 1) {
        const rise = (this.time * speed + i * 0.137 + seed * 0.11) % 1;
        const wave = this.time * (1.8 + (i % 3) * 0.21) + i * 2.37 + seed;
        const x = Math.sin(wave) * w * (0.13 + (i % 4) * 0.042);
        const y = -h * (0.1 + rise * 0.72);
        const r = Math.max(1.2, w * (i % 3 === 0 ? 0.035 : 0.022));
        const alpha = (0.18 + (1 - rise) * 0.38) * (poison ? 1 : 0.82);
        ctx.shadowColor = `rgba(${core},${alpha})`;
        ctx.shadowBlur = r * 3.2;
        ctx.fillStyle = `rgba(${core},${alpha})`;
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.fill();
      }

      // Thin rising vapour: green is sharper/acidic, purple is slower/sickly.
      ctx.lineCap = "round";
      for (let side = -1; side <= 1; side += 2) {
        ctx.strokeStyle = `rgba(${core},${poison ? 0.3 : 0.22})`;
        ctx.shadowColor = `rgba(${core},0.42)`;
        ctx.shadowBlur = w * 0.08;
        ctx.lineWidth = Math.max(1, w * 0.017);
        ctx.beginPath();
        for (let step = 0; step <= 5; step += 1) {
          const p = step / 5;
          const y = -h * (0.08 + p * 0.64);
          const x = side * w * (0.1 + Math.sin(this.time * (poison ? 2.2 : 1.45) + p * 7 + seed) * 0.1);
          if (step === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }
      ctx.restore();
    };

    if (u.poisoned) layer(true);
    if (u.diseased) layer(false);
  }

  /** Draws a complete frame: ground then units/overlays, on one canvas — everything below
   * still works exactly as before. A caller that needs units/HP-bars on a visually separate
   * layer from the ground (see BattleCanvas's WebGL elemental-FX overlay, which needs to
   * insert itself between the two) calls renderGround and renderUnitsAndOverlays directly
   * instead of this. */
  render(ctx: any, cssW: number, cssH: number, dpr: number): void {
    this.renderGround(ctx, cssW, cssH, dpr);
    this.renderUnitsAndOverlays(ctx, cssW, cssH);
  }

  /** Tiles, decorations, terrain-rule overlays (walk/attack/spell range highlights, the
   * active-turn glow, the hover cursor) — everything at or below "ground level". Opens this
   * frame's screen-shake transform but does not close it here (see renderUnitsAndOverlays). */
  /** Advances camera/visibility bookkeeping for this frame WITHOUT drawing anything — the
   * non-drawing prefix renderGround always ran, factored out so an alternate renderer (see
   * gfx/three/ThreeBattleRenderer.ts) can keep `layout`/visibility/camera state in sync without
   * going through the Canvas2D-shim draw path. renderGround calls this too, so its own
   * behavior is byte-for-byte unchanged. Returns the current zoom level's tile size, since
   * every caller needs it right after anyway. */
  updateCameraLayout(cssW: number, cssH: number): number {
    const tile = ZOOM_RADII[this.zoom]!;
    // Battle screens can mount during a route transition while their container is still
    // zero-sized. Do not lock camReady until a real viewport exists; the first visible frame
    // must focus the party with the final viewport dimensions.
    if (cssW < 64 || cssH < 64) return tile;
    // Cheap no-op unless the party moved since the last frame — see refreshVisibility.
    // Sitting here means anything drawn, and anything the HUD reads off this engine,
    // is deciding against current sight rather than last turn's.
    this.refreshVisibility();
    this.viewW = cssW;
    this.viewH = cssH;
    if (!this.camReady) {
      this.layout = { ox: 0, oy: 0, tile, cols: this.cols, rows: this.rows };
      // beginUnitTurn can run on the first animation frame before a real viewport has ever
      // been measured. Discard that provisional camera delta before focusing the party.
      this.camX = 0;
      this.camY = 0;
      this.camReady = true;
      this.focusPlayers();
    }
    this.clampCam();
    // Camera coordinates are allowed slightly negative/over the far edge so panning can
    // reveal the backdrop around every combat map, even when the board is smaller than view.
    const ox = -this.camX;
    const oy = -this.camY;
    this.layout = { ox, oy, tile, cols: this.cols, rows: this.rows };
    // Rolled once per frame here (not inside renderGround) so it still applies under
    // ThreeBattleRenderer, which calls this but never calls renderGround — renderGround and
    // renderUnitsAndOverlays both just read frameShakeDx/Dy now instead of one of them owning
    // the randomization the other silently depended on.
    const shake = this.reducedMotion ? 0 : this.trauma * this.trauma;
    if (shake) {
      this.frameShakeDx = (Math.random() - 0.5) * 10 * shake;
      this.frameShakeDy = (Math.random() - 0.5) * 10 * shake;
    } else {
      this.frameShakeDx = 0;
      this.frameShakeDy = 0;
    }
    return tile;
  }

  renderGround(ctx: any, cssW: number, cssH: number, dpr: number): void {
    const tile = this.updateCameraLayout(cssW, cssH);
    const { w: boardW, h: boardH } = this.boardSize(tile);
    const { ox, oy } = this.layout;

    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssW, cssH);
    const backdrop = this.art.backdrops[this.mission.id];
    if (backdrop) {
      const ir = backdrop.width / Math.max(1, backdrop.height);
      const cr = cssW / Math.max(1, cssH);
      let dw: number;
      let dh: number;
      if (ir > cr) {
        dh = cssH;
        dw = cssH * ir;
      } else {
        dw = cssW;
        dh = cssW / ir;
      }
      ctx.drawImage(backdrop, (cssW - dw) / 2, (cssH - dh) / 2, dw, dh);
      ctx.fillStyle = "rgba(0, 0, 0, 0.42)";
      ctx.fillRect(0, 0, cssW, cssH);
    } else {
      ctx.fillStyle = "#000000";
      ctx.fillRect(0, 0, cssW, cssH);
    }

    // Randomized once per frame in updateCameraLayout now, not here — see its own comment.
    const shake = this.frameShakeDx !== 0 || this.frameShakeDy !== 0;
    if (shake) {
      ctx.save();
      ctx.translate(this.frameShakeDx, this.frameShakeDy);
    }

    const floorRects = mapFloorRects(this.tiles, this.cols, this.rows, this.decorations);
    const squareBorder = hasSquareMapBorder(this.tiles, this.cols, this.rows, this.mission.squareTiles);
    const floorOrigin = this.hexCenter(0, 0);
    for (let y = 0; y < this.rows; y++) {
      for (let x = 0; x < this.cols; x++) {
        const cx = floorOrigin.cx + (x + (squareBorder ? 0 : (y & 1) * 0.5)) * Math.sqrt(3) * tile;
        const cy = floorOrigin.cy + y * 1.5 * tile;
        if (cx < -tile * 2 || cy < -tile * 2 || cx > cssW + tile * 2 || cy > cssH + tile * 2) continue;
        // Never seen: draw nothing at all. Cheaper than the clipped path below, which is
        // why fog makes a big fogged board lighter to draw rather than heavier.
        if (!this.explored(x, y)) continue;
        const drawId = tileAt(this.tiles, this.cols, x, y);
        // Void erases the tile: draw nothing, so the backdrop shows through.
        if (drawId === "void") continue;
        const isWaterFx = this.waterFxTileKeys.has(y * this.cols + x);
        ctx.save();
        const floorRect = floorRects.get(y * this.cols + x);
        if (!floorRect) { ctx.restore(); continue; }
        ctx.beginPath();
        if (!squareBorder) this.hexPath(ctx, cx, cy, tile);
        else for (const part of floorRectParts(floorRect)) ctx.rect(floorOrigin.cx - Math.sqrt(3) * tile / 2 + part.minX * tile,
          floorOrigin.cy - 0.75 * tile + part.minY * tile,
          (part.maxX - part.minX) * tile, (part.maxY - part.minY) * tile);
        ctx.clip();
        // Remembered but not in sight: the ground the party walked past, dimmed so it
        // reads as recall rather than as somewhere they can currently see into.
        if (!this.visible(x, y)) ctx.globalAlpha = 0.38;
        if (isWaterFx) {
          // Flat lakebed fill instead of the photo tile art — the WebGL water FX (see
          // BattleCanvas/gfx.EffectsRenderer) is drawn fully opaque over this hex and owns
          // the entire look, so nothing needs to show through here at all.
          ctx.fillStyle = "#0c2230";
          ctx.fill();
        } else {
          const variants = this.art.tiles[drawId];
          const variant = this.tileVariants[y * this.cols + x] ?? 0;
          const img = variants[variant] ?? variants[0];
          // A turned hex spins about its own centre, inside the clip. Sixty degrees maps a
          // hexagon onto itself, so only the picture moves — the shape stays put and the
          // neighbours still line up.
          const rot = this.tileRots[y * this.cols + x] ?? 0;
          const continuousGround = isHexGroundVariant(drawId, variant);
          if (rot && !continuousGround) {
            ctx.translate(cx, cy);
            ctx.rotate((rot * Math.PI) / 3);
            ctx.translate(-cx, -cy);
          }
          if (img && continuousGround) {
            // Shared board coordinates keep neighboring hexes on the same surface.
            // Material rotation is intentionally fixed; rotating a single cell would split it.
            drawHexGround(ctx, img, cx, cy, cx - this.layout.ox, cy - this.layout.oy, tile);
          } else if (img) drawGroundTexture(ctx, img, cx - tile, cy - tile, tile * 2, tile * 2);
          else {
            ctx.fillStyle = "#1e1b18";
            ctx.fill();
          }
        }
        ctx.restore();
      }
    }

    // Dreaming Web's persistent floor patch: a real alpha-cutout spiderweb photo (GameArt.
    // webfloor) stamped on every hex a live zone covers — replaces the old procedural WebGL
    // "web" shader quad, whose own glow doubled up with the movement-range highlight's glow
    // right after casting it (see overlay()'s glow: false for web cells below) and read as an
    // odd bright pop rather than something actually sitting on the ground. Gated per-hex, not
    // per-zone: a real stamped image has nothing to gain from withholding the whole zone until
    // every one of its cells is explored the way the old single full-footprint quad did — each
    // hex reveals its own web the moment that hex itself is explored. Still withheld until
    // WEB_SHOT_TRAVEL elapses since the zone's own createdAt, so it shows up exactly when the
    // travelling shot (see BattleCanvas's webShot sync) actually lands rather than popping in
    // the instant the spell is cast.
    for (const zone of this.webZones) {
      if (zone.createdAt != null && this.time < zone.createdAt + WEB_SHOT_TRAVEL) continue;
      for (const k of zone.cells) {
        const comma = k.indexOf(",");
        const wx = Number(k.slice(0, comma));
        const wy = Number(k.slice(comma + 1));
        if (!Number.isFinite(wx) || !Number.isFinite(wy) || !this.explored(wx, wy)) continue;
        const { cx, cy } = this.hexCenter(wx, wy);
        if (cx < -tile * 2 || cy < -tile * 2 || cx > cssW + tile * 2 || cy > cssH + tile * 2) continue;
        ctx.save();
        this.hexPath(ctx, cx, cy, tile * 1.0);
        ctx.clip();
        if (!this.visible(wx, wy)) ctx.globalAlpha = 0.38;
        ctx.drawImage(this.art.webfloor, cx - tile, cy - tile, tile * 2, tile * 2);
        ctx.restore();
      }
    }
    if (shake) ctx.restore();
    // Ground/behind decorations are drawn in renderUnitsAndOverlays instead of here, so they
    // land on the units canvas — stacked above the WebGL elemental FX canvas sitting in
    // between this canvas and that one (see BattleCanvas) — rather than being hidden under it.

    // Walkable/attack/spell-range highlights and the active-turn glow: split into their own
    // method (see renderBoardOverlays) so ThreeBattleRenderer — which replaces this function
    // entirely rather than calling it — can still draw them onto its own overlay canvas. Own
    // shake save/translate/restore pair in there rather than sharing this function's (already
    // closed above), so it renders identically whichever caller reaches it.
    this.renderBoardOverlays(ctx, cssW, cssH);
  }

  /** Canvas2D drawing for the movement/attack/spell-range highlight + active-turn ring — used
   * by the legacy `?renderer=legacy` path only (via renderGround's call site below). Split out
   * of renderGround as its own method because it used to be that function's inlined tail end,
   * and the actual cell/color decisions now live in boardOverlayLayers/activeTurnHighlight so
   * ThreeBattleRenderer can render the same highlight as real world-space geometry instead
   * (see ThreeBattleRenderer.syncOverlay) — this method is just the Canvas2D fill+glow+stroke
   * treatment on top of that shared data. */
  renderBoardOverlays(ctx: any, cssW: number, cssH: number): void {
    const { tile } = this.layout;
    const shake = this.frameShakeDx !== 0 || this.frameShakeDy !== 0;
    if (shake) {
      ctx.save();
      ctx.translate(this.frameShakeDx, this.frameShakeDy);
    }
    const drawLayer = (cells: Point[], fill: string, glow: boolean) => {
      const style = tacticalGridStyle(fill);
      ctx.save();
      ctx.globalAlpha = fill === GRID_MOVE ? this.overlayFade : 1;
      ctx.shadowColor = fill === GRID_ENEMY_TARGET ? GRID_ENEMY_GLOW : style.edge;
      ctx.shadowBlur = 0;
      ctx.fillStyle = style.fill;
      ctx.strokeStyle = style.edge;
      ctx.lineWidth = Math.max(1, tile * 0.025);
      for (const c of cells) {
        const { cx, cy } = this.hexCenter(c.x, c.y);
        this.hexPath(ctx, cx, cy, tile);
        ctx.fill();
        if (fill === GRID_MOVE || fill === GRID_ENEMY_TARGET || fill === GRID_OFFHAND_TARGET) {
          this.hexPath(ctx, cx, cy, tile * 0.94);
          ctx.stroke();
        }
      }
      ctx.restore();
    };
    // Which cells are highlighted, and in what color, is decided once in boardOverlayLayers —
    // shared with ThreeBattleRenderer, which turns each layer into a real world-space hex mesh
    // ordered between terrain and decorations, instead of a Canvas2D fill — so the cell/color
    // logic (the mode/spell switch that used to live inline here) can never drift between the
    // two renderers. This method only knows how to paint a layer once it has one.
    ctx.save();
    for (const layer of this.boardOverlayLayers()) drawLayer(layer.cells, layer.fill, layer.glow);
    ctx.restore();

    // Route breadcrumbs and destination are crisp ground markings, without bloom.
    const route = this.movementPreview();
    ctx.save();
    ctx.shadowBlur = 0;
    ctx.globalAlpha = 1;
    route.forEach((c, i) => {
      const { cx, cy } = this.hexCenter(c.x, c.y);
      this.hexPath(ctx, cx, cy, tile * (i === route.length - 1 ? 0.94 : 0.12));
      ctx.fillStyle = i === route.length - 1 ? "rgba(220,226,235,0.1)" : GRID_ROUTE;
      ctx.strokeStyle = "rgba(8,12,16,0.95)";
      ctx.lineWidth = Math.max(4, tile * 0.10);
      ctx.fill();
      ctx.stroke();
      ctx.strokeStyle = GRID_ROUTE;
      ctx.shadowColor = GRID_ROUTE;
      ctx.shadowBlur = tile * 0.08;
      ctx.lineWidth = Math.max(2, tile * 0.045);
      ctx.stroke();
      ctx.shadowBlur = 0;
    });
    ctx.restore();
    const marker = this.activeTurnHighlight();
    if (marker) {
      const { cx, cy } = this.hexCenter(marker.x, marker.y);
      ctx.save();
      ctx.shadowBlur = 0;
      ctx.strokeStyle = "rgba(12,20,25,0.85)";
      ctx.lineWidth = Math.max(4, tile * 0.11);
      this.hexPath(ctx, cx, cy, tile * 0.94);
      ctx.stroke();
      ctx.strokeStyle = marker.player ? "rgba(220,226,235,0.32)" : marker.fill;
      ctx.shadowColor = marker.fill;
      ctx.shadowBlur = marker.player ? tile * 0.035 : 0;
      ctx.lineWidth = Math.max(2, tile * 0.055);
      ctx.stroke();
      ctx.restore();
    }

    if (shake) ctx.restore();
  }

  /** Pure data: which cells are highlighted right now (walkable range, attack range, an aimed
   * spell/AoE, an aura zone, the idle threat preview, ...) and what color each group gets —
   * every `overlay(cells, fill, glow)` call that used to live inline in renderBoardOverlays,
   * unchanged in behavior, just collected instead of drawn immediately. Shared by
   * renderBoardOverlays (Canvas2D fill + glow/blur/stroke) and ThreeBattleRenderer (a flat
   * translucent hex mesh per cell, positioned between terrain and decorations in world space)
   * so the two can never disagree about which cells light up or in what color. */
  boardOverlayLayers(): { cells: Point[]; fill: string; glow: boolean }[] {
    const layers: { cells: Point[]; fill: string; glow: boolean }[] = [];
    // `glow` defaults on for every existing caller. Dreaming Web's own persistent floor patch
    // (a separate WebGL layer, see BattleCanvas's webFloorIds sync) already lights a webbed hex
    // with its own breathing glow — stacking this overlay's soft halo on top of that, on every
    // hex of a zone that can easily be a dozen-plus hexes and sits lit for
    // several whole rounds (unlike a one-shot spell flash that's gone before anyone can really
    // look at it), is what read as the movement highlight suddenly "blowing out" right after
    // casting it. Passing false keeps the flat fill — still marks the hex as walkable — but
    // drops the glow that was doubling up on the web's own.
    const push = (cells: Iterable<Point>, fill: string, glow = true) => {
      const arr = Array.isArray(cells) ? cells : [...cells];
      if (arr.length) layers.push({ cells: arr, fill, glow });
    };

    for (const zone of this.auraZones) {
      const cells = [...zone.cells].map((k) => {
        const [x, y] = k.split(",").map(Number);
        return { x: x!, y: y! };
      });
      push(cells, zone.kind === "protection" ? "rgba(150,210,255,0.3)" : "rgba(220,90,70,0.3)");
    }

    for (const zone of this.iceStormZones) {
      const cells = [...zone.cells].map((cell) => {
        const [x, y] = cell.split(",").map(Number);
        return { x: x!, y: y! };
      });
      push(cells, "rgba(135,205,255,0.28)", false);
    }

    if (this.mode === "idle" && this.threat.length) push(this.threat, "rgba(220,120,90,0.5)");

    if (this.mode === "awaitPotion") {
      const selected = this.units.find((u) => u.id === this.selectedId);
      if (selected) {
        const range = [{ x: selected.x, y: selected.y }, ...hexNeighbors(selected.x, selected.y)].filter((c) =>
          this.validPotionTarget(selected, c),
        );
        push(range, "rgba(150,210,170,0.45)");
        const cell = this.hover;
        if (cell && this.validPotionTarget(selected, cell)) push([cell], "rgba(170,230,180,0.55)");
      }
    }

    if (this.mode === "awaitSpell") {
      const selected = this.units.find((u) => u.id === this.selectedId);
      if (selected && this.spellKind === "fireball") {
        push(fireballRangeTiles(selected, this.cols, this.rows), "rgba(235,140,70,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && manhattan(selected, cell) <= FIREBALL.range) {
          push(fireballTiles(fireballOrigin(cell, this.cols, this.rows), this.cols, this.rows), "rgba(235,140,70,0.55)");
        }
      } else if (selected && this.spellKind === "causticVenom") {
        push(this.healRangeTiles(selected, CAUSTIC_VENOM.range), "rgba(200,210,90,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && manhattan(selected, cell) <= CAUSTIC_VENOM.range) {
          push(hexAreaTiles(fireballOrigin(cell, this.cols, this.rows), CAUSTIC_VENOM.size, this.cols, this.rows), "rgba(200,210,90,0.55)");
        }
      } else if (selected && this.spellKind === "divineBolt") {
        push(this.healRangeTiles(selected, DIVINE_BOLT.range), "rgba(255,205,110,0.48)");
        const cell = this.hover ?? this.spellAim;
        if (cell && manhattan(selected, cell) <= DIVINE_BOLT.range) {
          push(hexAreaTiles(fireballOrigin(cell, this.cols, this.rows), DIVINE_BOLT.size, this.cols, this.rows), "rgba(255,225,150,0.62)");
        }
      } else if (selected && this.spellKind === "minorVenom") {
        push(this.healRangeTiles(selected, MINOR_VENOM.range), "rgba(200,210,90,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && manhattan(selected, cell) <= MINOR_VENOM.range) {
          push(hexAreaTiles(fireballOrigin(cell, this.cols, this.rows), MINOR_VENOM.size, this.cols, this.rows), "rgba(200,210,90,0.55)");
        }
      } else if (selected && this.spellKind === "sweep") {
        push(this.sweepTiles(selected), "rgba(220,150,70,0.5)");
      } else if (selected && this.spellKind === "longShot") {
        const reach: Point[] = [];
        const max = this.longMax(selected);
        for (let y = 0; y < this.rows; y++) {
          for (let x = 0; x < this.cols; x++) {
            const d = manhattan(selected, { x, y });
            if (d >= selected.minRange && d <= max) reach.push({ x, y });
          }
        }
        push(reach, "rgba(210,190,90,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(230,200,100,0.55)");
      } else if (selected && this.spellKind === "piercing") {
        push(allAxisRays(selected, this.cols, this.rows), "rgba(220,160,70,0.45)");
        const cell = this.hover ?? this.spellAim;
        const line = cell ? this.piercingRay(selected, cell) : null;
        if (line) push(line, "rgba(235,170,80,0.55)");
      } else if (selected && this.spellKind === "piercingThrust") {
        push(this.healRangeTiles(selected, selected.maxRange + 1), "rgba(220,160,80,0.45)");
        const cell = this.hover ?? this.spellAim;
        const line = cell ? this.piercingThrustRay(selected, cell) : null;
        if (line) push(line, "rgba(235,175,90,0.55)");
      } else if (selected && (this.spellKind === "doubleStrike" || this.spellKind === "trip" || this.spellKind === "lifeDrain")) {
        push(this.healRangeTiles(selected, selected.maxRange), "rgba(220,120,80,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(235,120,80,0.55)");
      } else if (selected && this.spellKind === "cleave") {
        push(hexNeighbors(selected.x, selected.y), "rgba(220,120,80,0.45)");
        const cell = this.hover ?? this.spellAim;
        const arc = cell ? cleaveHexes(selected, cell, CLEAVE.hexes, this.cols, this.rows) : [];
        if (arc.length) push(arc, "rgba(235,120,80,0.55)");
      } else if (selected && this.spellKind === "summonFamiliar") {
        push(this.healRangeTiles(selected, SUMMON_FAMILIAR.range), "rgba(180,150,235,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(200,170,245,0.55)");
      } else if (selected && this.spellKind === "summonFamiliar2") {
        push(this.healRangeTiles(selected, SUMMON_FAMILIAR2.range), "rgba(180,150,235,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(200,170,245,0.55)");
      } else if (selected && this.spellKind === "summonFamiliar4") {
        push(this.healRangeTiles(selected, SUMMON_FAMILIAR4.range), "rgba(180,150,235,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(200,170,245,0.55)");
      } else if (selected && this.spellKind === "summonZombieDog") {
        push(this.healRangeTiles(selected, SUMMON_ZOMBIE_DOG.range), "rgba(180,150,235,0.45)");
        const cell = this.hover ?? this.spellAim;
        // Preview the dog's whole Type 2 body, not just the anchor tile.
        if (cell && this.spellAimValid(selected, cell)) push(footprint({ x: cell.x, y: cell.y, size: CLASSES.zombieDog!.size, footprintOffsets: CLASSES.zombieDog!.footprintOffsets }), "rgba(200,170,245,0.55)");
      } else if (selected && this.spellKind === "summonFamiliar3") {
        push(this.healRangeTiles(selected, SUMMON_FAMILIAR3.range), "rgba(180,150,235,0.45)");
        const cell = this.hover ?? this.spellAim;
        // Preview the full 6-hex silhouette he'd actually land on, not just the anchor tile.
        if (cell && this.spellAimValid(selected, cell)) push(footprint({ x: cell.x, y: cell.y, size: CLASSES.familiar3!.size, footprintOffsets: CLASSES.familiar3!.footprintOffsets }), "rgba(200,170,245,0.55)");
      } else if (selected && this.spellKind === "webOfDreams") {
        push(this.healRangeTiles(selected, WEB_OF_DREAMS.range), "rgba(170,140,230,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && manhattan(selected, cell) <= WEB_OF_DREAMS.range) {
          push(hexAreaTiles(cell, webOfDreamsSize(selected.level), this.cols, this.rows), "rgba(185,155,240,0.55)");
        }
      } else if (selected && this.spellKind === "lightning") {
        push(this.healRangeTiles(selected, LIGHTNING.range), "rgba(140,200,245,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(160,215,255,0.55)");
      } else if (selected && this.spellKind === "lightningTier3") {
        push(this.healRangeTiles(selected, LIGHTNING_T3.range), "rgba(120,210,255,0.5)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(180,235,255,0.65)");
      } else if (selected && this.spellKind === "magicMissile") {
        push(this.healRangeTiles(selected, MAGIC_MISSILE.range), "rgba(180,150,235,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(200,170,245,0.55)");
      } else if (selected && this.spellKind === "phantasmalForce") {
        push(this.healRangeTiles(selected, PHANTASMAL_FORCE.range), "rgba(180,150,235,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(200,170,245,0.55)");
      } else if (selected && this.isHeal(this.spellKind)) {
        push(this.healRangeTiles(selected, CURES[this.spellKind].range), "rgba(150,210,170,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.validHealTarget(selected, cell)) push([cell], "rgba(170,230,180,0.55)");
      } else if (selected && this.spellKind === "cureDisease") {
        push(this.healRangeTiles(selected, CURE_DISEASE.range), "rgba(150,210,170,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.validCureDiseaseTarget(selected, cell)) push([cell], "rgba(170,230,180,0.55)");
      } else if (selected && this.spellKind === "multiShot") {
        push(this.healRangeTiles(selected, MULTI_SHOT.range), "rgba(210,190,90,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(230,200,100,0.55)");
      } else if (selected && this.spellKind === "divineWrath") {
        push(this.healRangeTiles(selected, DIVINE_WRATH.range), "rgba(255,225,140,0.4)");
        const cell = this.hover ?? this.spellAim;
        const line = cell ? this.wrathRay(selected, cell, DIVINE_WRATH.range) : null;
        if (line) push(line, "rgba(255,225,140,0.6)");
      } else if (selected && this.spellKind === "shoulderSmash") {
        push(hexNeighbors(selected.x, selected.y), "rgba(220,120,80,0.45)");
        const cell = this.hover ?? this.spellAim;
        const arc = cell ? cleaveHexes(selected, cell, shoulderSmashPower(selected.level).hexes, this.cols, this.rows) : [];
        if (arc.length) push(arc, "rgba(235,120,80,0.55)");
      } else if (selected && this.spellKind === "stampede") {
        push(this.healRangeTiles(selected, STAMPEDE.range), "rgba(200,90,60,0.4)");
        const cell = this.hover ?? this.spellAim;
        const line = cell ? this.wrathRay(selected, cell, STAMPEDE.range) : null;
        if (line) push(line, "rgba(200,90,60,0.6)");
      } else if (selected && this.spellKind === "bullRush") {
        // The 4-hex targeting area, then only enemy hexes a charge really reaches (the first
        // unit on a clear line), over each enemy's whole body — nothing lit is a dud click.
        push(this.healRangeTiles(selected, BULL_RUSH_RANGE), "rgba(220,120,80,0.22)");
        const legal: Point[] = [];
        for (const u of this.units) {
          if (!u.alive || u.side === selected.side) continue;
          for (const c of footprint(u)) {
            if (!inBounds(c.x, c.y, this.cols, this.rows)) continue;
            if (this.bullRushCharge(selected, c)?.foe.id === u.id) legal.push(c);
          }
        }
        push(legal, "rgba(220,120,80,0.45)");
        const cell = this.hover ?? this.spellAim;
        const charge = cell ? this.bullRushCharge(selected, cell) : null;
        if (cell && charge) {
          for (const leg of charge.legs) {
            push(leg.path, "rgba(235,120,80,0.6)");
            push(footprint(leg.foe), "rgba(255,90,60,0.7)");
            // Where each hit enemy ends up; redder when its push is cut short (impact damage).
            const land = leg.push.path[leg.push.path.length - 1] ?? { x: leg.foe.x, y: leg.foe.y };
            if (leg.push.path.length > 0 || leg.push.blocked) {
              push(footprint({ ...leg.foe, x: land.x, y: land.y }), leg.push.blocked ? "rgba(255,60,40,0.8)" : "rgba(255,180,120,0.6)");
            }
          }
        }
      } else if (selected && this.spellKind === "provoke") {
        push(this.healRangeTiles(selected, provokePower(selected.level).range), "rgba(224,96,58,0.3)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push(this.provokeArea(cell, selected.level), "rgba(235,96,58,0.55)");
      } else if (selected && (this.spellKind === "executionerStrike" || this.spellKind === "shieldBash")) {
        push(this.healRangeTiles(selected, selected.maxRange), "rgba(220,120,80,0.45)");
        const cell = this.hover ?? this.spellAim;
        if (cell && this.spellAimValid(selected, cell)) push([cell], "rgba(235,120,80,0.55)");
      } else if (selected && (this.spellKind === "burningHands" || this.spellKind === "poisonBreath")) {
        const power = (this.spellKind === "poisonBreath" ? poisonBreathPower : burningHandsPower)(selected.level);
        push(allAxisRays(selected, this.cols, this.rows).filter((p) => hexDist(selected, p) <= power.range), this.spellKind === "poisonBreath" ? "rgba(100,200,60,0.35)" : "rgba(235,140,70,0.35)");
        const cell = this.hover ?? this.spellAim;
        const ray = cell ? this.wrathRay(selected, cell, power.range) : null;
        const dir = ray && ray[0] ? axisDir(selected, ray[0]) : null;
        if (dir) push(coneSector(selected, dir, power.radius, this.cols, this.rows), this.spellKind === "poisonBreath" ? "rgba(100,200,60,0.55)" : "rgba(235,140,70,0.55)");
      }
      if(selected&&this.spellKind==="frost"){const cell=this.hover??this.spellAim;if(cell)push(this.frostTiles(selected,cell),"rgba(160,220,255,0.5)");}
      if (selected && this.spellKind === "turnUndead") {
        push(this.turnUndeadTiles(selected, selected.level), "rgba(255,225,145,0.45)");
      }
      if (selected && this.spellKind === "iceStorm") {
        const power = iceStormPower(selected.level);
        push(this.healRangeTiles(selected, power.range), "rgba(125,195,245,0.38)");
        const cell = this.hover ?? this.spellAim;
        if (cell && manhattan(selected, cell) <= power.range) {
          push(iceStormAreaTiles(cell, selected.level, this.cols, this.rows), "rgba(150,220,255,0.5)");
        }
      }
    }

    if (this.mode === "selected" || this.mode === "awaitAttack" || this.mode === "awaitAction" || this.mode === "awaitOffHand") {
      // Free roam reaches the whole floor; tinting all of it would just wash the map blue.
      if (this.mode === "selected" && !this.mission.explore) {
        // computeReachable always keeps the unit's starting cell so it can build paths out
        // of that cell, even if the unit was placed on an impassable prop. That start cell
        // is not a valid movement destination, so don't paint the movement grid beneath a
        // house (or any other solid prop) just because the selected unit is standing there.
        const reachable = [...this.reach.values()].filter((cell) => this.hexAt(cell.x, cell.y).passable);
        const inWeb = reachable.filter((c) => this.isWebCell(c.x, c.y));
        const clear = inWeb.length ? reachable.filter((c) => !this.isWebCell(c.x, c.y)) : reachable;
        push(clear, GRID_MOVE);
        if (inWeb.length) push(inWeb, GRID_MOVE, false);
      }
      const selected = this.units.find((u) => u.id === this.selectedId);
      const offHandReach = selected && this.mode === "awaitOffHand" ? this.offHandReach(selected) : null;
      const atkTiles: Point[] = [];
      const offHandTiles: Point[] = [];
      for (const foe of this.units) {
        if (!foe.alive || foe.side === "player") continue;
        // Hub maps: NPCs are people to talk to, never marked as targets.
        if ((this.mission.hub || this.mission.explore) && foe.side === "neutral") continue;
        if (this.mode === "selected" && this.attackFrom.has(foe.id)) atkTiles.push(...footprint(foe));
        if ((this.mode === "awaitAttack" || this.mode === "awaitAction") && selected && canHitFrom(selected, selected, foe, this.tiles, this.cols, this.decorOverlay)) {
          atkTiles.push(...footprint(foe));
        }
        if (offHandReach && selected && this.targetable(foe) && canHitFrom(offHandReach, selected, foe, this.tiles, this.cols, this.decorOverlay)) {
          offHandTiles.push(...footprint(foe));
        }
      }
      push(atkTiles, GRID_ENEMY_TARGET);
      push(offHandTiles, GRID_OFFHAND_TARGET);
      if (this.pendingFoeId) {
        const foe = this.units.find((u) => u.id === this.pendingFoeId);
        if (foe) push(footprint(foe), GRID_ENEMY_TARGET);
      }
    }

    return layers;
  }

  /** The active-turn unit's pulsing gold/red ring, as one more cell+color — kept separate from
   * boardOverlayLayers because the Canvas2D path draws it with its own bespoke size/glow (see
   * renderBoardOverlays' own active-turn block), not the generic drawLayer treatment.
   * ThreeBattleRenderer uses this instead, to get the same cell and color without duplicating
   * BattleEngine's turn-order logic. */
  private previewReach: Map<string, ReachCell> | null = null;
  private previewKey = "";
  private previewCells: Point[] = [];

  /** Uses the same unpruned walk graph as commitMove, including passage through allies. */
  movementPreview(): Point[] {
    const selected = this.units.find((u) => u.id === this.selectedId);
    const to = this.hover ?? this.cursor;
    if (this.mode !== "selected" || this.active || !selected || this.mission.explore ||
        !this.reach.has(key(to.x, to.y)) || (to.x === selected.x && to.y === selected.y)) return [];
    const previewKey = `${selected.id}:${selected.x},${selected.y}:${to.x},${to.y}:${selected.moveBudgetUsed}`;
    if (this.previewReach !== this.reach || this.previewKey !== previewKey) {
      const walkReach = computeReachable(this.effectiveUnitForReach(selected), this.tiles, this.cols, this.rows, this.units, false, this.decorOverlay);
      this.previewCells = reconstructPath(walkReach, to).slice(1);
      this.previewReach = this.reach;
      this.previewKey = previewKey;
    }
    return this.previewCells;
  }

  activeTurnHighlight(): { x: number; y: number; fill: string; player: boolean } | null {
    const active = this.visuallyActingUnit();
    if (!active) return null;
    let { x, y } = active;
    // Logical x/y commit only when a walk step finishes, but the sprite is already moving.
    // Advance the marker at the visible midpoint so it never trails one hex behind.
    const moving = this.active;
    if (moving?.type === "move" && moving.id === active.id) {
      // The gold player marker is a turn cue, not a second moving sprite. Hide it during a
      // hero's walk; enemy movement keeps its red marker so AI turns remain easy to follow.
      // Keep the acting unit identifiable throughout movement.
      if (active.side === "player") return null;
      const from = moving.path[moving.i];
      const to = moving.path[moving.i + 1];
      if (from && to) {
        const dur = this.speedMode === "fast" ? 0.12 : this.speedMode === "slow" ? 0.36 : 0.22;
        const progress = easeOut(Math.min(1, moving.t / dur));
        ({ x, y } = progress < 0.5 ? from : to);
      }
    }
    return { x, y, fill: active.side === "enemy" ? GRID_ENEMY : GRID_ALLY, player: active.side === "player" };
  }

  /** Units, HP bars, particles, projectiles, banners, and the foreground decoration layer —
   * drawn on top of renderGround's output. Re-applies this frame's screen-shake offset (see
   * frameShakeDx/Dy) independently rather than sharing one still-open ctx.save() with
   * renderGround, since the two may be drawing onto two different canvases. */
  /** Inn-quest pickups lying on the ground: a small pulsing gold glint (not a hex outline),
   * hidden under fog of war until the hex has been explored. */
  private drawQuestPickups(ctx: any, tile: number): void {
    if (this.questPickups.length === 0) return;
    const pulse = this.reducedMotion ? 1 : 0.75 + Math.sin(this.time * 3.4) * 0.25;
    for (const pickup of this.questPickups) {
      if (!this.explored(pickup.x, pickup.y)) continue;
      const { cx, cy } = this.hexCenter(pickup.x, pickup.y);
      const r = tile * 0.2;
      ctx.save();
      ctx.shadowColor = `rgba(255,208,96,${(0.85 * pulse).toFixed(3)})`;
      ctx.shadowBlur = tile * 0.45 * pulse;
      ctx.fillStyle = "rgba(255,226,140,0.95)";
      ctx.beginPath();
      ctx.moveTo(cx, cy - r);
      ctx.lineTo(cx + r * 0.7, cy);
      ctx.lineTo(cx, cy + r);
      ctx.lineTo(cx - r * 0.7, cy);
      ctx.closePath();
      ctx.fill();
      ctx.restore();
    }
  }

  /** Floating combat text ("Missed", damage, heals, level-up labels) for the tactics camera,
   * drawn upright on a plain screen canvas. Same text, colors and timing as the
   * renderUnitsAndOverlays pass; toScreen maps a top-down board point lifted `up` pixels
   * above the ground onto the real camera view, and scale is the camera's sprite scale. */
  renderFloatingTextHud(ctx: CanvasRenderingContext2D, toScreen: (cx: number, cy: number, up: number) => { x: number; y: number }, scale: number): void {
    const tile = ZOOM_RADII[this.zoom]!;
    if (this.particleLive) {
      const dmgCell = tile * Math.sqrt(3);
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      for (const p of this.particles) {
        if (!p.live || p.kind !== "text" || !p.text) continue;
        // Anchor on the hex the text spawned over (p.x/p.y drift with vx/vy): the drifted row
        // can round to a neighbor, which a turned camera shows off to the side of the unit.
        const { cx, cy } = this.hexCenter(Math.round(p.x - p.vx * p.life), Math.round(p.y - p.vy * p.life));
        const fade = 0.4;
        ctx.globalAlpha = p.life < p.max - fade ? 1 : Math.max(0, 1 - (p.life - (p.max - fade)) / fade);
        const fontPx = Math.max(16, Math.round(dmgCell * 0.42 * scale));
        ctx.font = `800 ${fontPx}px Figtree, sans-serif`;
        ctx.lineJoin = "round";
        ctx.lineWidth = Math.max(4, fontPx * 0.22);
        ctx.strokeStyle = "rgba(12,11,10,0.92)";
        ctx.fillStyle = p.color;
        const at = toScreen(cx, cy, dmgCell * 0.85);
        const y = at.y - p.life * 16 * scale;
        ctx.strokeText(p.text, at.x, y);
        ctx.fillText(p.text, at.x, y);
      }
      ctx.globalAlpha = 1;
    }
    if (this.levelUpFxLive) {
      for (const s of this.levelUpFx) {
        if (!s.live || s.kind !== "label") continue;
        const unit = this.units.find((u) => u.id === s.unitId);
        if (!unit) continue;
        const k = s.life / s.max;
        const cellNow = tile * Math.sqrt(3);
        const us = unitSize(unit);
        const boss = unit.classId === "captain";
        const isBig = unit.footprintOffsets === FOOTPRINT_TYPE_8 || unit.footprintOffsets === FOOTPRINT_TYPE_7;
        const hh = cellNow * (us >= 4 ? 3.35 : us === 2 ? 1.72 : boss ? 1.44 : 1.42) * 1.2 * (isBig ? 0.75 : 1);
        const footY = us >= 4 ? tile * 0.9 : cellNow * 0.42;
        const { cx: upx, cy: upy } = this.unitPixel(unit);
        const labelFade = k < 0.12 ? k / 0.12 : k > 0.75 ? Math.max(0, 1 - (k - 0.75) / 0.25) : 1;
        const pop = k < 0.12 ? 1.35 - 0.35 * (k / 0.12) : 1;
        const at = toScreen(upx, upy + footY, hh - s.dy);
        const size = s.size * scale;
        ctx.save();
        ctx.globalAlpha = labelFade;
        ctx.translate(at.x + s.dx * scale, at.y);
        ctx.scale(pop, pop);
        ctx.textAlign = "center";
        ctx.textBaseline = "bottom";
        ctx.font = `900 ${Math.round(size)}px Figtree, sans-serif`;
        ctx.shadowColor = `hsla(${s.hue}, 100%, 65%, 0.95)`;
        ctx.shadowBlur = size * 0.9;
        ctx.lineJoin = "round";
        ctx.lineWidth = Math.max(4, size * 0.16);
        ctx.strokeStyle = "rgba(24,16,4,0.9)";
        ctx.strokeText(s.text ?? "", 0, 0);
        ctx.fillStyle = `hsl(${s.hue}, 100%, 74%)`;
        ctx.fillText(s.text ?? "", 0, 0);
        ctx.shadowBlur = size * 1.6;
        ctx.fillText(s.text ?? "", 0, 0);
        ctx.restore();
      }
    }
  }

  /** getLightAt, when given, answers "how much extra light falls on this screen point right
   * now?" from actually-active spell casts (fire/acid/holy/darkness/webShot) — see
   * EffectsRenderer.lightBoostAt, which BattleCanvas wires this to. Positive brightens a unit
   * standing near a fire/holy/acid glow or a travelling web shot; negative (darkness) dims one.
   * Omitted (the render() convenience path above, which has no EffectsRenderer of its own)
   * simply skips the check — units draw exactly as if nothing were casting light nearby.
   *
   * skipGroundDecor, when true, skips the "ground"/"behind" drawDecorations calls below (the
   * "front" one near the end still runs) — set by BattleCanvas when gfx/three/
   * ThreeBattleRenderer is drawing the ground canvas instead of WebGL2DRenderer, since that
   * renderer already draws those same two layers itself (see its own ensureDecorBuilt); without
   * this every ground/behind prop would be drawn twice, once by each renderer. */
  renderUnitsAndOverlays(
    ctx: any,
    cssW: number,
    cssH: number,
    getLightAt?: (px: number, py: number) => number,
    skipGroundDecor?: boolean,
    // ThreeBattleRenderer draws unit sprites itself once it has them (see its own
    // ensureUnitsBuilt/syncUnits) — this skips just the character-image draw calls below so
    // they don't double-draw, while everything else in this loop (shadow, HP bar, level/heal
    // glow, status FX) keeps rendering on this canvas exactly as before, per
    // THREEJS_MILESTONE1_HANDOFF.md's scoping of what stays here vs what moves.
    skipUnitSprites?: boolean,
    // MILESTONE 2 — ThreeBattleRenderer now casts a real shadow from an invisible per-unit box
    // (see its own shadowCasterMaterial/updateSun); this skips just this fake ellipse so the two
    // don't visibly double up under ?renderer=three. Independent of skipUnitSprites: the fake
    // shadow is keyed to the sprite's own screen position/pose (px, sway, lift, breath, foot),
    // not to whether the sprite image itself still draws here.
    skipUnitShadow?: boolean,
    // The mouse-selection hex outline below assumed unit sprites were drawn later on this same
    // canvas, so painting it first put it "under" them — true for the legacy 2D renderer, but
    // ThreeBattleRenderer's characters live one canvas down, stacked BELOW this one (see
    // BattleCanvas), so that outline ended up drawn in front of every character instead. Skip
    // it here and ThreeBattleRenderer draws the same outline itself as scene geometry, at the
    // same z it uses for boardOverlayLayers/activeTurnHighlight — genuinely behind decorations
    // and units rather than merely earlier in one canvas' own draw order.
    skipCursorHex?: boolean,
    // Fog 2's requested stacking is decorations → fog → units. When Three owns decorations,
    // foreground props must skip this top canvas too or they would leap above both fog and units.
    skipFrontDecor?: boolean,
    // ThreeBattleRenderer draws summoning portals as a ground layer beneath units (see its
    // syncPortalFx), so this overlay — which sits above Three's units — skips them.
    skipPortalFx?: boolean,
    // Under the spatial camera, health bars are drawn in a separate screen-facing pass so they
    // stay upright and follow the camera-facing character billboards.
    skipUnitHealthHud?: boolean,
    // Same for floating combat text ("Missed", damage, heals, level-up) under the tactics
    // camera: this canvas is warped onto the tilted ground there, so renderFloatingTextHud
    // draws it upright in screen space instead.
    skipFloatingText?: boolean,
  ): void {
    const tile = ZOOM_RADII[this.zoom]!;
    const sqrt3 = Math.sqrt(3);
    const shake = this.reducedMotion ? 0 : this.trauma * this.trauma;
    if (shake) {
      ctx.save();
      ctx.translate(this.frameShakeDx, this.frameShakeDy);
    }

    if (!skipGroundDecor) {
      // Ordinary ground props are interleaved with characters by row depth below. Explicit
      // rear props stay under every character; foreground props remain the final pass.
      // A rear parapet must remain visible over the ground and tactical highlights, while
      // character sprites still pass in front of it.
      this.drawDecorations(ctx, tile, cssW, cssH, "behind");
    }
    if (!skipPortalFx) this.drawPortalFx(ctx, tile);
    this.drawQuestPickups(ctx, tile);

    // The mouse-selection hex outline is drawn here, on this (topmost) canvas rather than
    // in renderGround, so it always reads above the WebGL water FX layer stacked in between
    // the ground and units canvases (see BattleCanvas) instead of being hidden under it —
    // but before any unit sprite, so the outline (and its blocked/height label) reads as a
    // ground marking under the units instead of a decal painted over their artwork. Under
    // ThreeBattleRenderer the characters live one canvas further down instead (see
    // skipCursorHex's own comment), so only the label stays here; the outline itself is
    // skipped and drawn as real scene geometry there instead.
    {
      const cur = this.hover ?? this.cursor;
      const { cx, cy } = this.hexCenter(cur.x, cur.y);
      const hid = tileAt(this.tiles, this.cols, cur.x, cur.y);
      const ht = TERRAIN[hid];
      const blocked = !ht.passable;
      // Void is erased ground (not drawn at all): no outline or "VAZIO" label over nothing.
      // Same for a never-seen hex under fog of war — its terrain must not leak through a label.
      const erased = hid === "void" || !this.explored(cur.x, cur.y);
      if (!skipCursorHex && !erased) {
        if (blocked) {
          ctx.save();
          ctx.shadowColor = "rgba(219,58,44,0.95)";
          ctx.shadowBlur = 0;
          ctx.strokeStyle = "rgba(231,133,115,0.95)";
          ctx.lineWidth = 3;
          this.hexPath(ctx, cx, cy, tile * 0.9);
          ctx.stroke();
          ctx.restore();
        } else {
          ctx.strokeStyle = "rgba(240,235,227,0.9)";
          ctx.lineWidth = 2;
          this.hexPath(ctx, cx, cy, tile * 0.9);
          ctx.stroke();
        }
      }
      if (!erased && (blocked || ht.height)) {
        const label = blocked ? ht.name.toUpperCase() : "ALTO +2";
        const fontPx = Math.max(11, Math.round(tile * 0.32));
        ctx.font = `700 ${fontPx}px Figtree, sans-serif`;
        ctx.textAlign = "center";
        ctx.textBaseline = "top";
        ctx.lineJoin = "round";
        ctx.lineWidth = Math.max(3, fontPx * 0.22);
        ctx.strokeStyle = "rgba(12,11,10,0.88)";
        ctx.fillStyle = blocked ? "#ff7a68" : "#efe4c4";
        ctx.strokeText(label, cx, cy + tile * 0.38);
        ctx.fillText(label, cx, cy + tile * 0.38);
      }
    }

    const cell = tile * sqrt3;
    // A single "sun" direction shared (by hand, kept in sync — see the comment on
    // WebGL2DRenderer's lightDirX/Y) with the sprite relighting in WebGL2DRenderer.ts: that
    // renderer's default light points toward (-0.6, -0.8) screen-space, so shadows here use the
    // exact opposite vector, offset and stretched along that axis instead of sitting as a
    // perfectly round puddle centered under every unit regardless of where the light actually is.
    const shadowDirX = 0.6;
    const shadowDirY = 0.8;
    const shadowOffset = cell * 0.16;
    const sorted = [...this.units].sort((a, b) => this.unitAnchor(a).worldY - this.unitAnchor(b).worldY || a.drawX - b.drawX);
    let lastGroundDecorDepth = -Infinity;
    for (const u of sorted) {
      if (u.fade <= 0) continue;
      // Out of sight, off the board. Unlike terrain there is no remembered version of a
      // body: a unit the party cannot see is simply not drawn, because a ghost left at
      // the last place it was seen would be read as where it is now.
      if (this.unitHidden(u)) continue;
      const unitDepth = this.unitAnchor(u).worldY;
      if (!skipGroundDecor) {
        this.drawDecorations(ctx, tile, cssW, cssH, "ground", { after: lastGroundDecorDepth, through: unitDepth });
        lastGroundDecorDepth = unitDepth;
      }
      const s = unitSize(u);
      const boss = isBossClass(u.classId);
      const { cx: px, cy: py } = this.unitPixel(u);
      const foot = s >= 4 ? 2.15 : s === 2 ? 1.5 : boss ? 1.12 : 1;
      // Pose (idle/walk/atk/cast/counter), size corrections, live idle motion (bob/sway/
      // breath) and high-ground lift — see computeUnitVisual's own comment; shared with
      // ThreeBattleRenderer's own unit meshes via the public unitVisual() wrapper.
      const { bob, sway, breath, lift, img, w, h, footY, scaleX, scaleY, footOffset } = this.computeUnitVisual(u, cell, tile);
      ctx.save();
      // A unit only dims after every queued animation has completed. Marking it as moved
      // happens when the action starts, so dimming immediately would make a Multi-Shot
      // archer translucent before its last arrow has landed.
      ctx.globalAlpha = u.fade * (u.moved && u.side === "player" && this.phase === "player" && !this.active ? 0.9 : 1);
      if (!skipUnitShadow) {
        // A soft cast shadow instead of a flat dark puddle: a radial gradient (center dark,
        // fading fully transparent at the edge) offset toward shadowDir so it reads as light
        // falling across the board rather than an ambient-occlusion blob glued to every unit's
        // feet. A unit standing on high ground (lift > 0, see unitLift — including mid-step
        // while walking on/off a raised hex) throws a slightly longer shadow, same as a real
        // object held further from the ground it's cast onto.
        const stretch = 1 + Math.min(0.6, lift / cell) * 0.5;
        const shadowCx = px + sway + shadowDirX * shadowOffset * stretch;
        const shadowCy = py + cell * 0.22 + shadowDirY * shadowOffset * stretch;
        const shadowRx = cell * 0.24 * foot * (1 + breath * 0.4) * stretch;
        const shadowRy = cell * 0.1 * Math.min(2.2, foot) * (1 - breath * 0.3);
        const shadowGrad = ctx.createRadialGradient(shadowCx, shadowCy, 0, shadowCx, shadowCy, Math.max(shadowRx, shadowRy));
        shadowGrad.addColorStop(0, "rgba(6,7,10,0.5)");
        shadowGrad.addColorStop(0.72, "rgba(6,7,10,0.3)");
        shadowGrad.addColorStop(1, "rgba(6,7,10,0)");
        ctx.fillStyle = shadowGrad;
        ctx.beginPath();
        ctx.ellipse(shadowCx, shadowCy, shadowRx, shadowRy, 0, 0, Math.PI * 2);
        ctx.fill();
      }
      ctx.translate(px + sway, py + footY + bob - lift);
      ctx.scale(scaleX, scaleY);
      if (u.levelGlow > 0) {
        const pulse = 0.75 + Math.sin(this.time * 7) * 0.25;
        const bg = ctx.createRadialGradient(0, -h * 0.5, 0, 0, -h * 0.5, w * 1.15);
        bg.addColorStop(0, `rgba(255,214,120,${0.5 * u.levelGlow * pulse})`);
        bg.addColorStop(1, "rgba(255,214,120,0)");
        ctx.fillStyle = bg;
        ctx.beginPath();
        ctx.arc(0, -h * 0.5, w * 1.15, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowColor = `rgba(255,208,110,${0.95 * u.levelGlow})`;
        ctx.shadowBlur = w * 0.4 * u.levelGlow * pulse;
      }
      // Heal / potion halo on the sprite itself. Palette comes from healGlowKind so Cura
      // Menor, Cura Média, Curar Doença, the new potion burst and Potionzero all read apart.
      if (u.healGlow > 0) {
        const pulse = 0.8 + Math.sin(this.time * 5) * 0.2;
        const halo = this.healHaloRgb(u.healGlowKind);
        const reach = u.healGlowKind === "holyMedium" || u.healGlowKind === "food" ? 1.35 : u.healGlowKind === "healingHands" ? 1.13 : u.healGlowKind === "holyMinor" ? 0.92 : 1.08;
        const bg = ctx.createRadialGradient(0, -h * 0.5, 0, 0, -h * 0.5, w * reach);
        bg.addColorStop(0, `rgba(${halo.core},${0.5 * u.healGlow * pulse})`);
        bg.addColorStop(0.45, `rgba(${halo.mid},${0.22 * u.healGlow * pulse})`);
        bg.addColorStop(1, `rgba(${halo.mid},0)`);
        ctx.fillStyle = bg;
        ctx.beginPath();
        ctx.arc(0, -h * 0.5, w * reach, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowColor = `rgba(${halo.core},${0.9 * u.healGlow})`;
        ctx.shadowBlur = w * (u.healGlowKind === "holyMedium" || u.healGlowKind === "food" ? 0.48 : 0.32) * u.healGlow * pulse;
      }
      // Real point-light influence from whatever's actually casting light nearby right now
      // (a fire/holy/acid glow, a travelling web shot, darkness's own dimming) — see
      // EffectsRenderer.lightBoostAt. Skipped entirely when a hit-flash is already driving
      // the filter (a rare, deliberately much brighter flash that shouldn't be diluted by
      // ambient spell light), and when nothing nearby is casting anything (the common case).
      const lightBoost = getLightAt ? getLightAt(px, py) : 0;
      if (u.flash > 0) ctx.filter = `brightness(${1.8 + u.flash})`;
      else if (Math.abs(lightBoost) > 0.03) ctx.filter = `brightness(${Math.max(0.35, 1 + lightBoost * 0.5)})`;
      if (skipUnitSprites) {
        // ThreeBattleRenderer already drew this unit's sprite on its own canvas, at the same
        // world position — see the param doc above.
      } else if (img) ctx.drawImageLit(img, -w / 2, -h + footOffset, w, h);
      else {
        ctx.fillStyle = u.side === "player" ? "#8a97a1" : u.side === "neutral" ? "#5f8a58" : "#a35a4a";
        ctx.fillRect(-w / 2, -h, w, h);
      }
      // A second glow pass on top of the sprite (shadowBlur alone, no offset, mimics an outer
      // rim glow following the art's own alpha edges) so the effect reads as coming off the
      // character rather than just floating behind it.
      if (!skipUnitSprites && u.levelGlow > 0 && img) {
        const pulse = 0.75 + Math.sin(this.time * 7) * 0.25;
        ctx.shadowBlur = w * 0.55 * u.levelGlow * pulse;
        ctx.drawImage(img, -w / 2, -h + footOffset, w, h);
      }
      if (!skipUnitSprites && u.healGlow > 0 && img) {
        const pulse = 0.8 + Math.sin(this.time * 5) * 0.2;
        const halo = this.healHaloRgb(u.healGlowKind);
        ctx.shadowColor = `rgba(${halo.core},${0.88 * u.healGlow})`;
        ctx.shadowBlur = w * (u.healGlowKind === "holyMedium" || u.healGlowKind === "food" ? 0.58 : 0.42) * u.healGlow * pulse;
        ctx.drawImage(img, -w / 2, -h + footOffset, w, h);
      }
      // Status FX is one fixed size (a normal one-hex unit's box), never the sprite's own size.
      this.drawStatusFx(ctx, u, cell * 1.11 * 1.2, cell * 1.42 * 1.2);
      ctx.filter = "none";
      ctx.shadowBlur = 0;
      ctx.restore();

      if (u.alive) {
        const bw = cell * (s >= 4 ? 1.35 : s === 2 ? 0.9 : boss ? 0.68 : 0.62);
        const bh = Math.max(4, cell * 0.07);
        const bx = px - bw / 2;
        const by = py - h + cell * 0.42 + bob - lift - Math.max(8, cell * 0.12);
        if (!skipUnitHealthHud) {
          ctx.fillStyle = "rgba(12,11,10,0.82)";
          ctx.fillRect(bx - 1, by - 1, bw + 2, bh + 2);
          ctx.fillStyle = "#2c2824";
          ctx.fillRect(bx, by, bw, bh);
          // Green for wild neutrals, so a beast that isn't hunting you doesn't read as an
          // enemy — it turns red on its own the moment it is provoked and joins that side.
          ctx.fillStyle = u.side === "player" ? "#c8c4bc" : u.side === "neutral" ? "#5f9e52" : "#b54a32";
          ctx.fillRect(bx, by, bw * Math.max(0, u.hp / u.maxHp), bh);
          if (cell >= 32) {
            ctx.font = `600 ${Math.round(cell * 0.22)}px Figtree, sans-serif`;
            ctx.textAlign = "center";
            ctx.textBaseline = "bottom";
            ctx.lineJoin = "round";
            ctx.lineWidth = 3;
            ctx.strokeStyle = "rgba(12,11,10,0.9)";
            ctx.fillStyle = "#f0ebe3";
            ctx.strokeText(`${u.hp}`, px, by - 1);
            ctx.fillText(`${u.hp}`, px, by - 1);
          }
        }
        if ((u.fearTurns ?? 0) > 0) {
          ctx.font = `bold ${Math.max(11, cell * .18)}px sans-serif`;
          ctx.textAlign = "center";
          ctx.fillStyle = "#ffe6a0";
          ctx.fillText(`Fear ${u.fearTurns}`, px, by - bh - cell * .16);
        }
        if (u.stunned) {
          const gx = px;
          const gy = by - bh - cell * 0.16;
          const r = cell * 0.13;
          const glow = ctx.createRadialGradient(gx, gy, 0, gx, gy, r);
          glow.addColorStop(0, "rgba(255,90,70,0.95)");
          glow.addColorStop(0.6, "rgba(255,60,50,0.55)");
          glow.addColorStop(1, "rgba(255,60,50,0)");
          ctx.fillStyle = glow;
          ctx.beginPath();
          ctx.arc(gx, gy, r, 0, Math.PI * 2);
          ctx.fill();
        }
      }

    }

    this.drawParticleFx(ctx, tile, skipFloatingText);

    this.drawLevelUpFx(ctx, tile, skipFloatingText);

    this.drawFireballBurstFx(ctx, tile);
    this.drawMissileFx(ctx, tile);

    this.drawLightningFx(ctx, tile);

    this.drawChargeFx(ctx, tile);
    this.drawHolyFx(ctx, tile);
    this.drawTurnUndeadFx(ctx, tile);
    this.drawBladeFx(ctx, tile);
    this.drawProvokeFx(ctx, tile);

    // Finish the ordinary ground props after the last character row, so nearer characters
    // remain in front while props closer to the camera hide characters behind them.
    if (!skipGroundDecor) this.drawDecorations(ctx, tile, cssW, cssH, "ground", { after: lastGroundDecorDepth, through: Infinity });
    if (!skipFrontDecor) this.drawDecorations(ctx, tile, cssW, cssH, "front");

    if (shake) ctx.restore();
  }

  healHaloRgb(kind: Unit["healGlowKind"]): { core: string; mid: string } {
    if (kind === "bless") return { core: "255,250,216", mid: "255,189,67" };
    if (kind === "disease") return { core: "200,255,230", mid: "70,210,160" };
    if (kind === "potion") return { core: "255,230,170", mid: "255,150,60" };
    if (kind === "holyMedium") return { core: "255,250,220", mid: "255,210,90" };
    if (kind === "healingHands") return { core: "255,249,225", mid: "255,215,120" };
    if (kind === "food") return { core: "225,242,255", mid: "110,175,255" };
    if (kind === "holyMinor") return { core: "255,248,230", mid: "255,220,150" };
    return { core: "255,250,235", mid: "255,248,224" }; // potionZero
  }

  /** Bull Rush's charge: a Flash-style golden speed streak from where the charge started to
   * the charger, with speed lines and flickering crackles along it — drawn only while the
   * charge move itself is playing. */
  private drawChargeFx(ctx: any, tile: number): void {
    const a = this.active;
    if (!a || a.type !== "move" || !a.charge || this.reducedMotion) return;
    const u = this.units.find((n) => n.id === a.id);
    const origin = a.path[0];
    if (!u || !origin) return;
    const head = this.unitPixel(u);
    this.drawRushStreak(ctx, tile, this.hexCenter(origin.x, origin.y), head, 1);
  }

  /** The golden speed streak from `o` to `head` (screen space, hex centres), at `alpha`. */
  private drawRushStreak(ctx: any, tile: number, o: { cx: number; cy: number }, head: { cx: number; cy: number }, alpha: number): void {
    const hy = head.cy - tile * 0.45;
    const oy = o.cy - tile * 0.45;
    const dx = head.cx - o.cx;
    const dy = hy - oy;
    const len = Math.hypot(dx, dy);
    if (len < 1 || alpha <= 0.01) return;
    const nx = -dy / len;
    const ny = dx / len;
    const flick = Math.floor(this.time * 30);
    const rnd = (n: number) => {
      const v = Math.sin(n * 12.9898 + flick * 78.233) * 43758.5453;
      return v - Math.floor(v);
    };
    ctx.save();
    ctx.globalCompositeOperation = "lighter";
    ctx.globalAlpha = alpha;
    ctx.lineCap = "round";
    // The streak: a tapered band, brightest at the charger, fading toward the start.
    const band = ctx.createLinearGradient(o.cx, oy, head.cx, hy);
    band.addColorStop(0, "rgba(255,200,80,0)");
    band.addColorStop(0.6, "rgba(255,190,70,0.28)");
    band.addColorStop(1, "rgba(255,240,190,0.75)");
    const wHead = tile * 0.42;
    ctx.fillStyle = band;
    ctx.beginPath();
    ctx.moveTo(o.cx, oy);
    ctx.lineTo(head.cx + nx * wHead, hy + ny * wHead);
    ctx.lineTo(head.cx - nx * wHead, hy - ny * wHead);
    ctx.closePath();
    ctx.fill();
    // Speed lines behind the charger.
    for (let i = 0; i < 9; i++) {
      const off = (rnd(i + 1) - 0.5) * tile * 0.9;
      const back = tile * (0.6 + rnd(i + 20) * 1.6);
      const sx = head.cx + nx * off - (dx / len) * tile * 0.2;
      const sy = hy + ny * off - (dy / len) * tile * 0.2;
      ctx.strokeStyle = `rgba(255,236,180,${0.35 + rnd(i + 40) * 0.45})`;
      ctx.lineWidth = Math.max(1, tile * 0.025);
      ctx.beginPath();
      ctx.moveTo(sx, sy);
      ctx.lineTo(sx - (dx / len) * back, sy - (dy / len) * back);
      ctx.stroke();
    }
    // Crackles: short jagged sparks flickering along the streak.
    ctx.shadowColor = "rgba(255,210,90,0.95)";
    ctx.shadowBlur = tile * 0.25;
    for (let c = 0; c < 4; c++) {
      const f = 0.35 + rnd(c + 60) * 0.6;
      let x = o.cx + dx * f;
      let y = oy + dy * f;
      ctx.strokeStyle = `rgba(255,250,220,${0.6 + rnd(c + 70) * 0.4})`;
      ctx.lineWidth = Math.max(1, tile * 0.02);
      ctx.beginPath();
      ctx.moveTo(x, y);
      for (let k = 0; k < 4; k++) {
        x += (rnd(c * 9 + k + 80) - 0.5) * tile * 0.5;
        y += (rnd(c * 9 + k + 90) - 0.5) * tile * 0.5;
        ctx.lineTo(x, y);
      }
      ctx.stroke();
    }
    ctx.restore();
  }

  private drawHolyFx(ctx: any, tile: number): void {
    if (!this.holyFxLive) return;
    for (const fx of this.holyFx) {
      if (!fx.live) continue;
      const u = fx.unitId ? this.units.find((n) => n.id === fx.unitId) : null;
      const pos = u ? this.hexCenter(u.drawX, u.drawY) : this.hexCenter(fx.x, fx.y);
      const cx = pos.cx;
      const cy = pos.cy;
      const k = fx.t / fx.max;
      const appear = Math.min(1, fx.t / 0.07);
      const hold = fx.kind === "medium" || fx.kind === "food" ? 0.36 : fx.kind === "hands" ? 0.31 : fx.kind === "disease" ? 0.32 : 0.26;
      const fade = (k < hold ? 1 : Math.max(0, 1 - (k - hold) / (1 - hold))) * appear;
      if (fade <= 0) continue;
      ctx.save();
      ctx.globalCompositeOperation = "lighter";
      if (fx.kind === "potion") this.drawPotionBurst(ctx, cx, cy, tile, fx, fade, k);
      else this.drawDivineLight(ctx, cx, cy, tile, fx, fade, k);
      ctx.restore();
    }
    ctx.globalAlpha = 1;
  }

  /** Summon Familiar's conjuring circle — a blue magic ring that opens on the ground, holds,
   * then closes, drawn on the ground layer (before the sorted unit-sprite pass) so the
   * familiar visibly steps out of it as its own fade-in ramps up (see castSummonFamiliar /
   * the `else if (u.alive && u.fade < 1)` tick branch) instead of just popping in next to an
   * unrelated puff of particles. */
  /** Whether any summoning portal is open — see ThreeBattleRenderer.syncPortalFx. */
  portalFxActive(): boolean {
    return this.portalFxLive > 0;
  }

  portalFxHasWarp(): boolean {
    return this.portalFx.some((fx) => fx.live && fx.warp);
  }

  /** Paints the open portals (screen-space, same as the 2D overlay) onto `ctx`. Used by
   * ThreeBattleRenderer to show them as a ground layer beneath units. */
  drawPortalFxLayer(ctx: any): void {
    this.drawPortalFx(ctx, ZOOM_RADII[this.zoom]!);
  }

  /** Screen-space box around every open portal (circle, glow, rising motes, light column),
   * so ThreeBattleRenderer only repaints/uploads that area instead of the whole viewport. */
  portalFxBounds(): { x0: number; y0: number; x1: number; y1: number } | null {
    if (!this.portalFxLive) return null;
    const tile = ZOOM_RADII[this.zoom]!;
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const p of this.portalFx) {
      if (!p.live) continue;
      let cx: number, cy: number, maxR = p.warp ? tile * (0.9 + (p.radius ?? 1) * 0.52) : tile * 0.95;
      if (p.body) {
        const cells = footprint({ x: p.x, y: p.y, size: 4, footprintOffsets: p.body }).map((c) => this.hexCenter(c.x, c.y));
        cx = cells.reduce((s, c) => s + c.cx, 0) / cells.length;
        cy = cells.reduce((s, c) => s + c.cy, 0) / cells.length;
        maxR = Math.max(maxR, Math.max(...cells.map((c) => Math.abs(c.cx - cx))) + tile * 0.95);
      } else ({ cx, cy } = this.hexCenter(p.x, p.y));
      const big = maxR / (tile * 0.95);
      const rx = maxR * 1.3 + tile * 0.4;
      const up = p.warp ? maxR * 2.35 : Math.max(maxR * 0.8, tile * (1.6 + big * 0.9), tile * (0.9 + big * 0.4)) + tile * 0.4;
      x0 = Math.min(x0, cx - rx); x1 = Math.max(x1, cx + rx);
      y0 = Math.min(y0, cy - up); y1 = Math.max(y1, cy + maxR * 0.8 + tile * 0.4);
    }
    return x1 > x0 ? { x0, y0, x1, y1 } : null;
  }

  private drawPortalFx(ctx: any, tile: number): void {
    if (!this.portalFxLive) return;
    const ease = (x: number) => 1 - (1 - Math.min(1, Math.max(0, x))) ** 3;
    for (const p of this.portalFx) {
      if (!p.live) continue;
      // Centre on the whole body that steps out, and size the circle to its on-screen width,
      // so a multi-hex familiar (e.g. Familiar Titã's 3x2 Type 6) gets a portal it fits in.
      let cx: number;
      let cy: number;
      let maxR = p.warp ? tile * (0.9 + (p.radius ?? 1) * 0.52) : tile * 0.95;
      if (p.body) {
        const cells = footprint({ x: p.x, y: p.y, size: 4, footprintOffsets: p.body }).map((c) => this.hexCenter(c.x, c.y));
        cx = cells.reduce((s, c) => s + c.cx, 0) / cells.length;
        cy = cells.reduce((s, c) => s + c.cy, 0) / cells.length;
        const spread = Math.max(...cells.map((c) => Math.abs(c.cx - cx)));
        maxR = Math.max(maxR, spread + tile * 0.95);
      } else {
        ({ cx, cy } = this.hexCenter(p.x, p.y));
      }
      const k = p.t / p.max;
      const openEnd = 0.35;
      const closeStart = 0.65;
      const radiusK = p.persistent ? (k < openEnd ? ease(k / openEnd) : 1) : (k < openEnd ? ease(k / openEnd) : k < closeStart ? 1 : Math.max(0, 1 - ease((k - closeStart) / (1 - closeStart))));
      if (radiusK <= 0.01) continue;
      const big = maxR / (tile * 0.95);
      const r = maxR * radiusK;
      const flat = 0.55;
      const spin = p.t * 3.2 + p.seed;
      if (p.warp) {
        const gateY = cy - r * 0.88;
        const gateW = r * 0.58;
        const gateH = r * 1.42;
        ctx.save();
        ctx.globalCompositeOperation = "lighter";
        if (this.warpFloorHalo) {
        const floorGlow = ctx.createRadialGradient(cx, cy, 0, cx, cy, r * 1.35);
        floorGlow.addColorStop(0, `rgba(195,225,255,${0.42 * radiusK})`);
        floorGlow.addColorStop(0.42, `rgba(75,150,255,${0.26 * radiusK})`);
        floorGlow.addColorStop(1, "rgba(20,65,170,0)");
        ctx.fillStyle = floorGlow;
        ctx.beginPath();
        ctx.ellipse(cx, cy, r * 1.35, r * 0.34, 0, 0, Math.PI * 2);
        ctx.fill();
        }

        const flipbooks = getWarpPortalFlipbooks();
        if (flipbooks) {
          const [mainSheet, groundSheet, moteSheet] = flipbooks;
          const frame = (Math.floor(p.t * 12) + Math.floor(p.seed * 3)) % 16;
          const drawFrame = (sheet: HTMLImageElement, index: number, x: number, y: number, w: number, h: number) => {
            const sw = sheet.naturalWidth / 4, sh = sheet.naturalHeight / 4;
            const sx = (index % 4) * sw, sy = Math.floor(index / 4) * sh;
            ctx.drawImage(sheet, sx, sy, sw, sh, x, y, w, h);
          };

          // Poison V2's layered flipbook technique: a looping 4x4 main atlas, a separate
          // animated floor reflection, and independently phased motes from their own atlas.
          ctx.save();
          ctx.globalCompositeOperation = "lighter";
          ctx.imageSmoothingEnabled = true;
          ctx.globalAlpha = 0.72 * radiusK;
          if (this.warpFloorHalo) drawFrame(groundSheet, (frame + 3) % 16, cx - r * 1.25, cy - r * 0.38, r * 2.5, r * 0.76);

          const imageW = gateW * 2.75;
          const imageH = gateH * 2.55;
          ctx.save();
          ctx.translate(cx, gateY);
          ctx.rotate(Math.sin(spin * 0.46) * 0.018);
          ctx.globalAlpha = 0.9 * radiusK;
          drawFrame(mainSheet, frame, -imageW / 2, -imageH / 2, imageW, imageH);
          ctx.restore();

          const moteCount = Math.round(12 + (p.radius ?? 1) * 5);
          for (let mote = 0; mote < moteCount; mote++) {
            const rise = (p.t * 0.55 + mote * 0.13 + (p.seed % 1)) % 1;
            const angle = p.seed + mote * 2.399 + spin * 0.15;
            const mx = cx + Math.cos(angle) * r * (0.62 + rise * 0.22);
            const my = gateY + gateH * (0.82 - rise * 1.68);
            const size = tile * (0.12 + ((mote * 7) % 5) * 0.018);
            ctx.globalAlpha = (1 - rise) * 0.78 * radiusK;
            drawFrame(moteSheet, (frame + mote * 5) % 16, mx - size / 2, my - size / 2, size, size);
          }
          ctx.restore();
          ctx.restore();
          continue;
        }

        const portalImage = getWarpPortalFallbackImage();
        if (portalImage) {
          const imageW = gateW * 2.75;
          const imageH = gateH * 2.55;
          ctx.save();
          ctx.globalAlpha = 0.9 * radiusK;
          ctx.drawImage(portalImage, cx - imageW / 2, gateY - imageH / 2, imageW, imageH);
          ctx.restore();
          ctx.restore();
          continue;
        }

        // The oval gate opens like a vertical wound in the air: layered blue-white currents
        // wrap a deep interior, distinct from the familiar's flat summoning circle.
        ctx.save();
        ctx.beginPath();
        ctx.ellipse(cx, gateY, gateW, gateH, 0, 0, Math.PI * 2);
        ctx.clip();
        // A narrow radial field gives the aperture a dark sapphire edge and a bright
        // dimensional core; using gateH here flattened the whole opening to white.
        const interior = ctx.createRadialGradient(cx - gateW * 0.12, gateY - gateH * 0.04, gateW * 0.015, cx, gateY, gateW * 1.2);
        interior.addColorStop(0, `rgba(248,253,255,${0.96 * radiusK})`);
        interior.addColorStop(0.2, `rgba(171,224,255,${0.96 * radiusK})`);
        interior.addColorStop(0.48, `rgba(58,149,255,${0.94 * radiusK})`);
        interior.addColorStop(0.76, `rgba(20,70,177,${0.92 * radiusK})`);
        interior.addColorStop(1, `rgba(4,15,52,${0.94 * radiusK})`);
        ctx.fillStyle = interior;
        ctx.fillRect(cx - gateW, gateY - gateH, gateW * 2, gateH * 2);
        // Moving event horizon: three broken ellipses orbit in opposite directions,
        // producing the layered, screen-space motion of a 2D WebGL portal shader.
        const pulse = 0.82 + Math.sin(spin * 0.7) * 0.08;
        ctx.globalAlpha = radiusK * pulse;
        ctx.lineCap = "round";
        for (let ring = 0; ring < 4; ring++) {
          const orbit = spin * (ring % 2 ? -0.12 : 0.16) + ring * 1.42;
          ctx.strokeStyle = ring === 1 ? "rgba(238,249,255,0.68)" : "rgba(123,203,255,0.48)";
          ctx.lineWidth = Math.max(1, tile * (ring === 1 ? 0.03 : 0.018));
          ctx.beginPath();
          ctx.ellipse(cx + Math.sin(orbit) * gateW * 0.08, gateY + Math.cos(orbit) * gateH * 0.035,
            gateW * (0.34 + (ring % 2) * 0.1), gateH * (0.18 + ring * 0.055), orbit * 0.12,
            orbit, orbit + Math.PI * (1.18 + (ring % 2) * 0.24));
          ctx.stroke();
        }
        // Fine currents bend through the aperture and drift sideways over time.
        for (let strand = 0; strand < 13; strand++) {
          const phase = spin * (strand % 2 ? -0.52 : 0.44) + strand * 0.71;
          const sway = Math.sin(phase) * gateW * (0.2 + (strand % 3) * 0.045);
          const mid = Math.sin(phase * 0.63) * gateW * 0.3;
          ctx.strokeStyle = strand % 4 === 0 ? "rgba(255,255,255,0.88)" : strand % 3 === 0 ? "rgba(163,222,255,0.72)" : "rgba(56,143,255,0.66)";
          ctx.lineWidth = Math.max(1, tile * (strand % 4 === 0 ? 0.026 : 0.014));
          ctx.beginPath();
          ctx.moveTo(cx + sway, gateY - gateH * 0.96);
          ctx.bezierCurveTo(cx - mid, gateY - gateH * 0.44, cx + mid + sway * 0.7, gateY + gateH * 0.36, cx - sway * 0.32, gateY + gateH * 0.96);
          ctx.stroke();
        }
        // A narrow horizontal flare makes the center read as a passage through space.
        const core = ctx.createRadialGradient(cx + Math.sin(spin) * gateW * 0.08, gateY, 0, cx, gateY, gateW * 0.78);
        core.addColorStop(0, `rgba(255,255,255,${0.52 * radiusK})`);
        core.addColorStop(0.28, `rgba(172,225,255,${0.32 * radiusK})`);
        core.addColorStop(1, "rgba(65,150,255,0)");
        ctx.fillStyle = core;
        ctx.fillRect(cx - gateW, gateY - tile * 0.18, gateW * 2, tile * 0.36);
        ctx.restore();

        ctx.lineCap = "round";
        ctx.shadowColor = "rgba(100,180,255,0.95)";
        ctx.shadowBlur = tile * 0.34;
        for (let arc = 0; arc < 3; arc++) {
          const inset = arc * tile * 0.075;
          ctx.strokeStyle = arc === 1 ? `rgba(232,247,255,${0.92 * radiusK})` : `rgba(70,155,255,${0.82 * radiusK})`;
          ctx.lineWidth = Math.max(1.5, tile * (arc === 1 ? 0.036 : 0.025));
          ctx.beginPath();
          ctx.ellipse(cx, gateY, Math.max(1, gateW + tile * (0.12 - arc * 0.02)), gateH + tile * (0.1 - arc * 0.02), spin * (arc - 1) * 0.012, 0, Math.PI * 2);
          ctx.stroke();
          ctx.beginPath();
          ctx.ellipse(cx, gateY, gateW * 0.76 - inset * 0.25, gateH * 0.82 - inset, spin * (1 - arc) * 0.008, spin + arc * 1.9, spin + arc * 1.9 + Math.PI * 1.5);
          ctx.stroke();
        }
        ctx.shadowBlur = 0;
        for (let mote = 0; mote < Math.round(10 + (p.radius ?? 1) * 4); mote++) {
          const angle = p.seed + mote * 2.399;
          const rise = (p.t * 0.4 + mote * 0.13) % 1;
          const mx = cx + Math.cos(angle + spin * 0.15) * r * (0.52 + rise * 0.16);
          const my = gateY + gateH * (0.8 - rise * 1.65);
          ctx.fillStyle = `rgba(176,220,255,${(1 - rise) * 0.78 * radiusK})`;
          ctx.beginPath();
          ctx.arc(mx, my, Math.max(1, tile * 0.026), 0, Math.PI * 2);
          ctx.fill();
        }
        ctx.restore();
        continue;
      }
      // Blue by default; Familiar Titã's portal is red (see PortalFx.red).
      const pal = (blue: string, red: string) => (p.red ? red : blue);

      ctx.save();
      ctx.globalCompositeOperation = "lighter";

      // Ground glow under the circle.
      const glow = ctx.createRadialGradient(cx, cy, 0, cx, cy, r * 1.3);
      glow.addColorStop(0, `rgba(${pal("150,195,255", "255,150,130")},${0.55 * radiusK})`);
      glow.addColorStop(0.6, `rgba(${pal("95,145,255", "235,70,55")},${0.32 * radiusK})`);
      glow.addColorStop(1, pal("rgba(60,110,255,0)", "rgba(180,20,20,0)"));
      ctx.fillStyle = glow;
      ctx.beginPath();
      ctx.ellipse(cx, cy, r * 1.3, r * 1.3 * flat, 0, 0, Math.PI * 2);
      ctx.fill();

      // Swirling vortex: curved arms spiralling into the centre, spinning fast.
      ctx.lineCap = "round";
      ctx.shadowColor = pal("rgba(140,190,255,0.9)", "rgba(255,90,70,0.9)");
      ctx.shadowBlur = tile * 0.2;
      const arms = big > 1.5 ? 5 : 4;
      for (let i = 0; i < arms; i++) {
        const base = spin * 2.1 + (i / arms) * Math.PI * 2;
        ctx.strokeStyle = `rgba(${pal("185,220,255", "255,170,150")},${0.5 * radiusK})`;
        ctx.lineWidth = Math.max(1, tile * 0.03);
        ctx.beginPath();
        for (let s = 0; s <= 14; s++) {
          const f = s / 14;
          const a = base + f * 2.4;
          const rr = r * 0.85 * (1 - f);
          const x = cx + Math.cos(a) * rr;
          const y = cy + Math.sin(a) * rr * flat;
          if (s === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }

      // Outer rim: one solid thin ring plus rune ticks marching around it.
      ctx.strokeStyle = `rgba(${pal("200,230,255", "255,190,175")},${0.75 * radiusK})`;
      ctx.lineWidth = Math.max(1, tile * 0.02);
      ctx.shadowBlur = tile * 0.2;
      ctx.beginPath();
      ctx.ellipse(cx, cy, r * 1.08, r * 1.08 * flat, 0, 0, Math.PI * 2);
      ctx.stroke();
      const ticks = Math.round(18 * Math.max(1, big * 0.8));
      ctx.lineWidth = Math.max(1, tile * 0.025);
      for (let i = 0; i < ticks; i++) {
        const a = -spin * 0.6 + (i / ticks) * Math.PI * 2;
        const x0 = cx + Math.cos(a) * r * 0.96;
        const y0 = cy + Math.sin(a) * r * 0.96 * flat;
        const x1 = cx + Math.cos(a) * r * 1.08;
        const y1 = cy + Math.sin(a) * r * 1.08 * flat;
        ctx.beginPath();
        ctx.moveTo(x0, y0);
        ctx.lineTo(x1, y1);
        ctx.stroke();
      }

      // Outer ring: a broken circle of arcs rotating one way — the "magic circle" border.
      const segs = 10;
      ctx.strokeStyle = `rgba(${pal("175,218,255", "255,150,130")},${0.85 * radiusK})`;
      ctx.lineWidth = Math.max(1.5, tile * 0.035);
      ctx.shadowBlur = tile * 0.25;
      for (let i = 0; i < segs; i++) {
        const a0 = spin + (i / segs) * Math.PI * 2;
        const a1 = a0 + ((Math.PI * 2) / segs) * 0.55;
        ctx.beginPath();
        ctx.ellipse(cx, cy, r, r * flat, 0, a0, a1);
        ctx.stroke();
      }

      // Inner ring: tighter, thinner, spinning the opposite way.
      ctx.strokeStyle = `rgba(${pal("222,240,255", "255,215,205")},${0.7 * radiusK})`;
      ctx.lineWidth = Math.max(1, tile * 0.018);
      ctx.shadowBlur = tile * 0.15;
      const innerSegs = 6;
      for (let i = 0; i < innerSegs; i++) {
        const a0 = -spin * 1.4 + (i / innerSegs) * Math.PI * 2;
        const a1 = a0 + ((Math.PI * 2) / innerSegs) * 0.6;
        ctx.beginPath();
        ctx.ellipse(cx, cy, r * 0.6, r * 0.6 * flat, 0, a0, a1);
        ctx.stroke();
      }

      // Column of light rising out of the circle while it's open (not on the red portal).
      ctx.shadowBlur = 0;
      if (!p.red) {
        const colH = tile * (1.6 + big * 0.9) * radiusK;
        const colW = r * 0.75;
        const col = ctx.createLinearGradient(cx, cy, cx, cy - colH);
        col.addColorStop(0, `rgba(170,210,255,${0.32 * radiusK})`);
        col.addColorStop(0.5, `rgba(120,170,255,${0.14 * radiusK})`);
        col.addColorStop(1, "rgba(90,140,255,0)");
        ctx.fillStyle = col;
        ctx.beginPath();
        ctx.moveTo(cx - colW, cy);
        ctx.quadraticCurveTo(cx - colW * 0.55, cy - colH * 0.5, cx - colW * 0.3, cy - colH);
        ctx.lineTo(cx + colW * 0.3, cy - colH);
        ctx.quadraticCurveTo(cx + colW * 0.55, cy - colH * 0.5, cx + colW, cy);
        // Base follows the circle's own near edge, never a hard straight cut across it.
        ctx.ellipse(cx, cy, colW, colW * flat, 0, 0, Math.PI);
        ctx.closePath();
        ctx.fill();
      }

      // Motes drifting up out of the circle.
      const motes = Math.round(8 * Math.max(1, big));
      for (let i = 0; i < motes; i++) {
        const ang = p.seed + i * 2.4;
        const rise = (p.t * 0.6 + i * 0.17) % 1;
        const mx = cx + Math.cos(ang) * r * 0.6;
        const my = cy + Math.sin(ang) * r * 0.6 * flat - rise * tile * (0.9 + big * 0.4);
        ctx.fillStyle = `rgba(${pal("195,222,255", "255,180,160")},${(1 - rise) * 0.7 * radiusK})`;
        ctx.beginPath();
        ctx.arc(mx, my, tile * 0.028, 0, Math.PI * 2);
        ctx.fill();
      }

      ctx.restore();
    }
  }

  /** The shared steel-swoosh visual — see BladeFx/BladeKind. Every shape here is plain
   * white-steel light (glow pass + bright core pass), the same treatment a real blade catches
   * the light with, and never fire or a magic-circle glow. */
  /** Hit sparks and floating numbers. Split out so the 3D overlay can draw each on its own hex. */
  private drawParticleFx(ctx: any, tile: number, skipFloatingText = false): void {
    if (this.particleLive) {
      const dmgCell = tile * Math.sqrt(3);
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      for (const p of this.particles) {
        if (!p.live || p.kind === "text") continue;
        const { cx, cy } = this.hexCenter(Math.round(p.x), Math.round(p.y));
        const px = cx;
        const py = cy - tile * 0.2;
        ctx.globalAlpha = 1 - p.life / p.max;
        if (p.kind === "impact") {
          const img = this.art.impact[Math.min(3, Math.floor(p.frame))];
          if (img) ctx.drawImage(img, px - tile * 0.45, py - tile * 0.45, tile * 0.9, tile * 0.9);
        } else {
          ctx.fillStyle = p.color;
          ctx.fillRect(px, py, p.size, p.size);
        }
      }
      for (const p of this.particles) {
        if (skipFloatingText || !p.live || p.kind !== "text" || !p.text) continue;
        const { cx, cy } = this.hexCenter(Math.round(p.x), Math.round(p.y));
        const fade = 0.4;
        const a = p.life < p.max - fade ? 1 : Math.max(0, 1 - (p.life - (p.max - fade)) / fade);
        ctx.globalAlpha = a;
        const fontPx = Math.max(16, Math.round(dmgCell * 0.42));
        ctx.font = `800 ${fontPx}px Figtree, sans-serif`;
        ctx.lineJoin = "round";
        ctx.lineWidth = Math.max(4, fontPx * 0.22);
        ctx.strokeStyle = "rgba(12,11,10,0.92)";
        ctx.fillStyle = p.color;
        ctx.strokeText(p.text, cx, cy - dmgCell * 0.85 - p.life * 16);
        ctx.fillText(p.text, cx, cy - dmgCell * 0.85 - p.life * 16);
      }
      ctx.globalAlpha = 1;
    }
  }

  /** Level-up rings, stars and labels. Split out so the 3D overlay can draw each on its own hex. */
  private drawLevelUpFx(ctx: any, tile: number, skipFloatingText = false): void {
    if (this.levelUpFxLive) {
      for (const s of this.levelUpFx) {
        if (!s.live) continue;
        const unit = this.units.find((u) => u.id === s.unitId);
        if (!unit) continue;
        const { cx, cy } = this.hexCenter(Math.round(unit.x), Math.round(unit.y));
        const x = cx + s.dx;
        const y = cy + s.dy;
        const k = s.life / s.max;
        if (s.kind === "ring") {
          const r = s.refCell * (0.15 + k * 1.25);
          ctx.globalAlpha = Math.max(0, 1 - k) * 0.85;
          ctx.strokeStyle = `hsl(${s.hue}, 95%, 68%)`;
          ctx.lineWidth = Math.max(1.5, s.refCell * 0.05 * (1 - k));
          ctx.beginPath();
          ctx.arc(x, y, r, 0, Math.PI * 2);
          ctx.stroke();
          if (k < 0.3) {
            const flash = ctx.createRadialGradient(x, y, 0, x, y, s.refCell * 0.5);
            flash.addColorStop(0, `rgba(255,250,220,${0.6 * (1 - k / 0.3)})`);
            flash.addColorStop(1, "rgba(255,250,220,0)");
            ctx.fillStyle = flash;
            ctx.globalAlpha = 1;
            ctx.beginPath();
            ctx.arc(x, y, s.refCell * 0.5, 0, Math.PI * 2);
            ctx.fill();
          }
          continue;
        }
        if (s.kind === "label") {
          if (skipFloatingText) continue;
          const tileNow = this.layout.tile;
          const cellNow = tileNow * Math.sqrt(3);
          const us = unitSize(unit);
          const boss = unit.classId === "captain";
          const isBig = unit.footprintOffsets === FOOTPRINT_TYPE_8 || unit.footprintOffsets === FOOTPRINT_TYPE_7;
          const hh = cellNow * (us >= 4 ? 3.35 : us === 2 ? 1.72 : boss ? 1.44 : 1.42) * 1.2 * (isBig ? 0.75 : 1);
          const footY = us >= 4 ? tileNow * 0.9 : cellNow * 0.42;
          const { cx: upx, cy: upy } = this.unitPixel(unit);
          const labelFade = k < 0.12 ? k / 0.12 : k > 0.75 ? Math.max(0, 1 - (k - 0.75) / 0.25) : 1;
          const pop = k < 0.12 ? 1.35 - 0.35 * (k / 0.12) : 1;
          ctx.save();
          ctx.globalAlpha = labelFade;
          ctx.translate(upx + s.dx, upy + footY - hh + s.dy);
          ctx.scale(pop, pop);
          ctx.textAlign = "center";
          ctx.textBaseline = "bottom";
          ctx.font = `900 ${Math.round(s.size)}px Figtree, sans-serif`;
          ctx.shadowColor = `hsla(${s.hue}, 100%, 65%, 0.95)`;
          ctx.shadowBlur = s.size * 0.9;
          ctx.lineJoin = "round";
          ctx.lineWidth = Math.max(4, s.size * 0.16);
          ctx.strokeStyle = "rgba(24,16,4,0.9)";
          ctx.strokeText(s.text ?? "", 0, 0);
          ctx.fillStyle = `hsl(${s.hue}, 100%, 74%)`;
          ctx.fillText(s.text ?? "", 0, 0);
          ctx.shadowBlur = s.size * 1.6;
          ctx.fillText(s.text ?? "", 0, 0);
          ctx.restore();
          continue;
        }
        const fade = k < 0.15 ? k / 0.15 : k > 0.7 ? Math.max(0, 1 - (k - 0.7) / 0.3) : 1;
        ctx.globalAlpha = fade;
        const size = s.size * (1 - k * 0.35);
        ctx.save();
        ctx.translate(x, y);
        ctx.rotate(s.rot);
        const glow = ctx.createRadialGradient(0, 0, 0, 0, 0, size * 2.2);
        glow.addColorStop(0, `hsla(${s.hue}, 100%, 82%, 0.9)`);
        glow.addColorStop(1, `hsla(${s.hue}, 100%, 60%, 0)`);
        ctx.fillStyle = glow;
        ctx.beginPath();
        ctx.arc(0, 0, size * 2.2, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = `hsl(${s.hue}, 95%, 78%)`;
        ctx.beginPath();
        for (let i = 0; i < 8; i++) {
          const ang = (Math.PI / 4) * i;
          const r = i % 2 === 0 ? size : size * 0.35;
          const px = Math.cos(ang) * r;
          const py = Math.sin(ang) * r;
          if (i === 0) ctx.moveTo(px, py);
          else ctx.lineTo(px, py);
        }
        ctx.closePath();
        ctx.fill();
        ctx.restore();
      }
      ctx.globalAlpha = 1;
    }
  }

  /** Fireball / Caustic Venom bursts. Split out so the 3D overlay can draw each on its own hex. */
  private drawFireballBurstFx(ctx: any, tile: number): void {
    if (this.fireballBurstFxLive) {
      for (const burst of this.fireballBurstFx) {
        if (!burst.live) continue;
        const { cx, cy } = this.hexCenter(burst.x, burst.y);
        const k = burst.t / burst.max;
        const fade = Math.max(0, 1 - k);
        const venom = burst.kind === "causticVenom";
        const radius = tile * (0.34 + k * 0.72);
        ctx.save();
        ctx.globalCompositeOperation = "lighter";
        const glow = ctx.createRadialGradient(cx, cy - tile * 0.1, 0, cx, cy - tile * 0.1, radius);
        glow.addColorStop(0, venom ? `rgba(232,255,175,${0.84 * fade})` : `rgba(255,248,194,${0.9 * fade})`);
        glow.addColorStop(0.22, venom ? `rgba(159,242,45,${0.76 * fade})` : `rgba(255,174,35,${0.78 * fade})`);
        glow.addColorStop(0.62, venom ? `rgba(25,150,54,${0.45 * fade})` : `rgba(236,62,12,${0.42 * fade})`);
        glow.addColorStop(1, venom ? "rgba(4,72,30,0)" : "rgba(128,18,0,0)");
        ctx.fillStyle = glow;
        ctx.beginPath();
        ctx.arc(cx, cy - tile * 0.1, radius, 0, Math.PI * 2);
        ctx.fill();
        if (venom) {
          // Caustic Venom dissipates as heavy green smoke, rather than borrowing Fireball's
          // flame tongues. The curling paths remain confined to the struck hexes.
          ctx.lineCap = "round";
          for (let i = 0; i < 8; i += 1) {
            const pair = i % 4;
            const angle = burst.seed + pair * (Math.PI / 2) + (i >= 4 ? Math.PI : 0);
            const drift = tile * (0.14 + pair * 0.035 + k * 0.2);
            const startX = cx + Math.cos(angle) * drift * 0.45;
            const startY = cy + Math.sin(angle) * drift * 0.22;
            const endX = cx + Math.cos(angle) * drift;
            const endY = cy - tile * (0.16 + k * (0.36 + (pair % 2) * 0.08));
            ctx.strokeStyle = pair % 3 === 0 ? `rgba(190,255,126,${0.54 * fade})` : `rgba(47,188,82,${0.48 * fade})`;
            ctx.lineWidth = tile * (0.07 + (pair % 2) * 0.026) * (0.85 + k * 0.4);
            ctx.shadowColor = "rgba(71,235,94,0.72)";
            ctx.shadowBlur = tile * 0.16;
            ctx.beginPath();
            ctx.moveTo(startX, startY);
            ctx.quadraticCurveTo(cx + Math.cos(angle) * drift * 0.72, cy - tile * (0.1 + k * 0.24), endX, endY);
            ctx.stroke();
          }
        } else {
          // Fireball keeps its existing rising tongues and embers.
          for (let i = 0; i < 7; i += 1) {
            const angle = burst.seed + i * 2.41 + k * 5.2;
            const spread = tile * (0.18 + (i % 3) * 0.1) * (0.75 + k * 0.35);
            const px = cx + Math.cos(angle) * spread;
            const py = cy - tile * (0.08 + k * 0.28) + Math.sin(angle) * spread * 0.45;
            const r = tile * (0.055 + (i % 2) * 0.025) * fade;
            ctx.shadowColor = "rgba(255,106,12,0.95)";
            ctx.shadowBlur = tile * 0.22;
            ctx.fillStyle = i % 3 === 0 ? `rgba(255,239,150,${fade})` : `rgba(255,93,8,${0.85 * fade})`;
            ctx.beginPath();
            ctx.arc(px, py, Math.max(1, r), 0, Math.PI * 2);
            ctx.fill();
          }
        }
        ctx.restore();
      }
    }
  }

  /** Lightning strikes and holy rays. Split out so the 3D overlay can draw each on its own hex. */
  private drawLightningFx(ctx: any, tile: number): void {
    if (this.lightningFxLive) {
      for (const l of this.lightningFx) {
        if (!l.live) continue;
        const t3 = l.power === "t3";
        const raio = l.power === "raio";
        const divine = l.power === "divine";
        const divineSplash = l.power === "divineSplash";
        const { cx, cy } = this.hexCenter(l.x, l.y);
        const topY = t3 ? -tile * 0.2 : cy - tile * (raio || divine ? LIGHTNING_RAIO_FALL_HEIGHT : divineSplash ? 1.1 : LIGHTNING_FALL_HEIGHT);
        const k = l.t / l.max;
        const reveal = Math.min(1, l.t / (t3 ? 0.07 : raio || divine ? 0.1 : divineSplash ? 0.045 : 0.06));
        const hold = t3 ? 0.32 : raio || divine ? 0.28 : 0.35;
        const fade = k < hold ? 1 : Math.max(0, 1 - (k - hold) / (1 - hold));
        if (fade <= 0) continue;

        ctx.save();
        ctx.globalCompositeOperation = "lighter";
        const pulse = t3 || divine || divineSplash ? 0.82 + 0.18 * Math.abs(Math.sin(l.t * 52 + l.hue)) : 1;
        // Real strikes restrike down the same channel two or three times in a fraction of a
        // second — a hard strobe rather than a smooth fade.
        const strobe = l.t < 0.05 ? 1 : l.t < 0.08 ? 0.22 : l.t < 0.14 ? 1 : l.t < 0.17 ? 0.35 : l.t < 0.21 ? 0.95 : 0.8;
        const glow = fade * pulse * strobe;

        // Seeded from the shape rolled at emit time, so the channel holds one fixed shape for
        // the whole strike instead of re-rolling every frame.
        const makeRng = (s: number) => () => {
          s = (s + 0x6d2b79f5) >>> 0;
          let r = Math.imul(s ^ (s >>> 15), 1 | s);
          r = (r + Math.imul(r ^ (r >>> 7), 61 | r)) ^ r;
          return ((r ^ (r >>> 14)) >>> 0) / 4294967296;
        };
        const baseSeed = (Math.floor(l.hue * 1000) ^ Math.floor((l.segs[0] ?? 0) * 1e6) ^ (l.branches.length * 7919)) >>> 0;
        const rnd = makeRng(baseSeed);
        const minSeg = tile * 0.08;
        // Midpoint displacement: each halving adds a smaller kink, which is what gives
        // lightning its fractal, crackling edge instead of a few straight zigzags.
        const zig = (ax: number, ay: number, bx: number, by: number, rough: number, r: () => number) => {
          const out = [{ x: ax, y: ay }];
          const rec = (x0: number, y0: number, x1: number, y1: number, d: number) => {
            const dx = x1 - x0;
            const dy = y1 - y0;
            const len = Math.hypot(dx, dy);
            if (len < minSeg) {
              out.push({ x: x1, y: y1 });
              return;
            }
            const off = (r() - 0.5) * d;
            const mx = (x0 + x1) / 2 - (dy / len) * off;
            const my = (y0 + y1) / 2 + (dx / len) * off;
            rec(x0, y0, mx, my, d * 0.55);
            rec(mx, my, x1, y1, d * 0.55);
          };
          rec(ax, ay, bx, by, Math.hypot(bx - ax, by - ay) * rough);
          return out;
        };

        const main = zig(cx + (rnd() - 0.5) * tile * (t3 ? 0.5 : 1.1), topY, cx, cy, t3 ? 0.14 : 0.2, rnd);
        const mainShown = Math.max(2, Math.ceil(main.length * reveal));

        // Forks: every one is generated (keeps the shape stable) but only drawn once the
        // descending leader has passed its split point.
        const forks: { pts: { x: number; y: number }[]; shown: number; w: number; a: number }[] = [];
        for (const b of l.branches) {
          const idx = Math.min(main.length - 2, Math.floor(b.at * main.length));
          const start = main[idx]!;
          const ang = Math.PI / 2 + b.side * (0.35 + rnd() * 0.55);
          const len = Math.max(tile * 0.6, (cy - start.y) * (0.3 + rnd() * 0.35));
          const pts = zig(start.x, start.y, start.x + Math.cos(ang) * len, start.y + Math.sin(ang) * len, 0.3, rnd);
          const forkReveal = Math.max(0, Math.min(1, (reveal - b.at) / (1 - b.at + 0.001)));
          const shown = idx < mainShown ? Math.ceil(pts.length * forkReveal) : 0;
          forks.push({ pts, shown, w: 0.55, a: 0.8 });
          if (rnd() < 0.65) {
            const sIdx = Math.floor(pts.length * (0.3 + rnd() * 0.4));
            const s = pts[sIdx]!;
            const sAng = ang + b.side * (0.3 + rnd() * 0.5);
            const sLen = len * (0.3 + rnd() * 0.25);
            const sub = zig(s.x, s.y, s.x + Math.cos(sAng) * sLen, s.y + Math.sin(sAng) * sLen, 0.32, rnd);
            forks.push({ pts: sub, shown: shown > sIdx ? Math.ceil(sub.length * Math.min(1, (shown - sIdx) / Math.max(1, pts.length - sIdx))) : 0, w: 0.32, a: 0.55 });
          }
        }

        // Three passes per channel: a tight coloured glow, a pale inner sheath and a thin
        // white-hot core — the core stays hairline-thin, which is what reads as electricity.
        const mainW = t3 ? 2.2 : divine ? 1.8 : raio ? 1.5 : divineSplash ? 0.85 : 1;
        const drawChannel = (pts: { x: number; y: number }[], shown: number, w: number, a: number) => {
          if (shown < 2) return;
          ctx.beginPath();
          ctx.moveTo(pts[0]!.x, pts[0]!.y);
          for (let i = 1; i < shown; i++) ctx.lineTo(pts[i]!.x, pts[i]!.y);
          ctx.lineJoin = "round";
          ctx.lineCap = "round";
          ctx.shadowColor = `hsla(${l.hue}, 100%, 66%, ${Math.min(1, a * glow)})`;
          ctx.shadowBlur = tile * 0.45 * w;
          ctx.strokeStyle = `hsla(${l.hue}, 100%, 64%, ${0.42 * a * glow})`;
          ctx.lineWidth = tile * 0.085 * w;
          ctx.stroke();
          ctx.shadowBlur = 0;
          ctx.strokeStyle = `hsla(${l.hue}, 100%, 86%, ${0.8 * a * glow})`;
          ctx.lineWidth = tile * 0.032 * w;
          ctx.stroke();
          ctx.strokeStyle = `rgba(255,255,255,${Math.min(1, a * glow)})`;
          ctx.lineWidth = Math.max(1, tile * 0.014 * w);
          ctx.stroke();
        };
        for (const f of forks) drawChannel(f.pts, f.shown, mainW * f.w, f.a);
        drawChannel(main, mainShown, mainW, 1);

        // Ground discharge: short arcs crawling out from the impact, re-rolled fast so they
        // crackle while the bolt is connected.
        if (mainShown >= main.length && k < 0.5) {
          const crackle = makeRng((baseSeed ^ (Math.floor(l.t * 40) * 2654435761)) >>> 0);
          const arcs = t3 ? 6 : raio || divine ? 5 : 3;
          const reachT = tile * (t3 ? 1.3 : divine ? 1.15 : raio ? 0.9 : divineSplash ? 0.42 : 0.6);
          for (let i = 0; i < arcs; i++) {
            const ang = crackle() * Math.PI * 2;
            const len = reachT * (0.45 + crackle() * 0.55);
            const arc = zig(cx, cy, cx + Math.cos(ang) * len, cy + Math.sin(ang) * len * 0.5, 0.35, crackle);
            drawChannel(arc, arc.length, mainW * 0.3, 0.7 * (1 - k / 0.5));
          }
        }

        const flashDuration = t3 ? 0.62 : divine ? 0.58 : raio ? 0.55 : 0.5;
        if (k < flashDuration) {
          const flashFade = Math.max(0, 1 - k / flashDuration);
          const flashR = tile * (t3 ? 2.1 : divine ? 1.7 : raio ? 1.55 : divineSplash ? 0.48 : 0.9);
          const flash = ctx.createRadialGradient(cx, cy, 0, cx, cy, flashR);
          flash.addColorStop(0, `hsla(${l.hue}, 100%, 94%, ${(t3 ? 0.98 : divine ? 0.96 : raio ? 0.92 : divineSplash ? 0.68 : 0.7) * flashFade * pulse})`);
          flash.addColorStop(0.32, `hsla(${l.hue}, 100%, 78%, ${(t3 ? 0.55 : divine ? 0.52 : raio ? 0.45 : divineSplash ? 0.22 : 0.3) * flashFade})`);
          flash.addColorStop(1, `hsla(${l.hue}, 100%, 70%, 0)`);
          ctx.fillStyle = flash;
          ctx.beginPath();
          ctx.arc(cx, cy, flashR, 0, Math.PI * 2);
          ctx.fill();
        }
        ctx.restore();
      }
      ctx.globalAlpha = 1;
    }
  }

  /** Turn Undead's hexes. Split out so the 3D overlay can draw each on its own hex. */
  private drawTurnUndeadFx(ctx: any, tile: number): void {
    for (const fx of this.turnUndeadFx) {
      const cells = fx.tiles.map((p) => {
        const { cx, cy } = this.hexCenter(p.x, p.y);
        return { x: cx, y: cy, corners: Array.from({ length: 6 }, (_, i): [number, number] => {
          const angle = (60 * i - 30) * Math.PI / 180;
          return [cx + tile * Math.cos(angle), cy + tile * Math.sin(angle)];
        }) };
      });
      drawTurnUndeadV4(ctx, cells, fx.t);
    }
  }

  /** Provoke's mark on its target. Split out so the 3D overlay can draw each on its own hex. */
  private drawProvokeFx(ctx: any, tile: number): void {
    for (const fx of this.provokeFx) {
      const target = this.units.find(u => u.id === fx.unitId && u.alive);
      if (!target || !this.targetable(target)) continue;
      const { cx, cy } = this.hexCenter(target.x, target.y);
      drawProvokeVFX(ctx, cx, cy - tile * 0.15, tile * 1.1, fx.t);
    }
  }

  /** Ember's traveling bolts (missileFx), split out so the 3D overlay can draw each on its own sheet. */
  private drawMissileFx(ctx: any, tile: number): void {
    if (this.missileFxLive) {
      for (const m of this.missileFx) {
        if (!m.live) continue;
        if (m.kind === "magicMissile" && m.t >= m.travel) continue;
        // The integrated Three.js fireball is the only projectile visual when its renderer is
        // active; never layer the legacy Canvas sprite over the original 3D tavern fireball.
        if (m.kind === "fireball" && this.fireballVfxAvailable && !this.reducedMotion) continue;
        if (m.kind === "causticVenom" && this.causticVenomVfxAvailable && !this.reducedMotion) continue;
        const from = this.hexCenter(m.fromX, m.fromY);
        const to = this.hexCenter(m.toX, m.toY);
        const dxT = to.cx - from.cx;
        const dyT = to.cy - from.cy;
        const dist = Math.hypot(dxT, dyT) || 1;
        const nx = -dyT / dist;
        const ny = dxT / dist;
        const along = (k: number) => {
          const wave = Math.sin(k * Math.PI * 2.4 + m.seed) * tile * 0.16 * (1 - k * 0.6);
          return { x: from.cx + dxT * k + nx * wave, y: from.cy - tile * 0.3 + dyT * k + ny * wave };
        };
        // kHead: the bolt's own position, 0-1, frozen at 1 once it lands. afterglow: 0 while
        // still flying, ramping to 1 as the lingering trail fades out after arrival.
        const kHead = Math.min(1, m.t / m.travel);
        const afterglow = Math.max(0, (m.t - m.travel) / MISSILE_AFTERGLOW);
        const physicalArrow = m.kind === "longShot";
        if (physicalArrow) {
          // All of Neera's arrow attacks share a restrained blood-red wake and a brief
          // impact sparkle; the arrow itself keeps the approved projectile art.
          const head = { x: from.cx + dxT * kHead, y: from.cy - tile * 0.3 + dyT * kHead };
          const flightAngle = Math.atan2(dyT, dxT);
          ctx.save();
          ctx.globalCompositeOperation = "lighter";
          ctx.globalAlpha = (1 - afterglow) * (m.neeraArrow ? 0.42 : 0.24);
          ctx.strokeStyle = m.neeraArrow ? "rgba(220,38,54,0.82)" : "rgba(215,222,226,0.74)";
          ctx.lineWidth = Math.max(1, tile * (m.neeraArrow ? 0.018 : 0.012));
          for (let ring = 1; ring <= 2; ring += 1) {
            const bk = Math.max(0, kHead - ring * 0.1);
            const back = { x: from.cx + dxT * bk, y: from.cy - tile * 0.3 + dyT * bk };
            if (m.neeraArrow) {
              ctx.beginPath();
              ctx.ellipse(back.x, back.y, tile * (0.10 + ring * 0.035), tile * (0.027 + ring * 0.01), flightAngle, 0, Math.PI * 2);
              ctx.stroke();
            } else {
              ctx.beginPath();
              ctx.ellipse(back.x, back.y, tile * (0.09 + ring * 0.035), tile * (0.024 + ring * 0.01), flightAngle, 0, Math.PI * 2);
              ctx.stroke();
            }
          }
          if (m.neeraArrow && afterglow < 1) {
            // A few ember-red motes follow the arrow during flight and flare outward on impact.
            for (let spark = 0; spark < 5; spark += 1) {
              const trail = spark * 0.055;
              const k = Math.max(0, kHead - trail);
              const x = from.cx + dxT * k;
              const y = from.cy - tile * 0.3 + dyT * k;
              const phase = m.seed + spark * 2.4 + this.time * 5;
              const spread = tile * (0.025 + spark * 0.012);
              ctx.fillStyle = `rgba(255,${58 + spark * 14},${48 + spark * 8},${(1 - afterglow) * (0.85 - spark * 0.11)})`;
              ctx.beginPath();
              ctx.arc(x + Math.cos(phase) * spread, y + Math.sin(phase) * spread, tile * (0.018 + (spark % 2) * 0.008), 0, Math.PI * 2);
              ctx.fill();
            }
            if (kHead >= 1) {
              ctx.globalAlpha = (1 - afterglow) * 0.8;
              ctx.strokeStyle = "rgba(255,75,62,0.9)";
              ctx.lineWidth = Math.max(1, tile * 0.016);
              for (let ray = 0; ray < 7; ray += 1) {
                const a = m.seed + ray * (Math.PI * 2 / 7);
                ctx.beginPath();
                ctx.moveTo(head.x + Math.cos(a) * tile * 0.035, head.y + Math.sin(a) * tile * 0.035);
                ctx.lineTo(head.x + Math.cos(a) * tile * (0.12 + afterglow * 0.12), head.y + Math.sin(a) * tile * (0.12 + afterglow * 0.12));
                ctx.stroke();
              }
            }
          }
          ctx.restore();
          ctx.save();
          ctx.translate(head.x, head.y);
          // The supplied source points northeast (-45°); rotate from that intrinsic direction to the flight angle.
          ctx.rotate(flightAngle + Math.PI / 4);
          ctx.globalCompositeOperation = "source-over";
          ctx.globalAlpha = 1 - afterglow;
          // 25% larger (was 1.08) so the arrow reads over the hex grid.
          ctx.drawImage(this.art.arrowCore, -tile * 0.675, -tile * 0.675, tile * 1.35, tile * 1.35);
          ctx.restore();
          continue;
        }

        // Dreaming Web's shot is now the WebGL "webShot" beam (see BattleEngine.webShotBeam /
        // BattleCanvas) — this MissileFx entry still exists purely as the timing clock that
        // drives it (fromX/Y, toX/Y, t, travel), so it's kept alive and aged like any other
        // missile, it just draws nothing of its own here.
        if (m.kind === "webOfDreams") continue;

        if (m.kind === "phantasmalForce") {
          // A translucent attacker races along the cast path rather than behaving like a
          // coloured projectile: skull, streaming lower body and two reaching claws make the
          // hit read as a brief hostile apparition on the victim's hex.
          const head = along(kHead);
          const fade = 1 - afterglow;
          const pulse = 0.9 + 0.1 * Math.sin(this.time * 15 + m.seed);
          const reachAngle = Math.atan2(dyT, dxT);
          ctx.save();
          ctx.translate(head.x, head.y);
          ctx.globalCompositeOperation = "lighter";
          ctx.globalAlpha = fade;
          ctx.shadowColor = "rgba(64,196,255,0.95)";
          ctx.shadowBlur = tile * 0.38;

          // The tapering, ragged body remains upright so the figure reads at a glance even
          // when it is flying sideways across the battlefield.
          const body = ctx.createLinearGradient(0, -tile * 0.35, 0, tile * 0.58);
          body.addColorStop(0, "rgba(188,246,255,0.80)");
          body.addColorStop(0.34, "rgba(43,165,255,0.55)");
          body.addColorStop(1, "rgba(19,82,222,0)");
          ctx.fillStyle = body;
          ctx.beginPath();
          ctx.moveTo(-tile * 0.20 * pulse, -tile * 0.08);
          ctx.quadraticCurveTo(-tile * 0.34, tile * 0.22, -tile * 0.17, tile * 0.57);
          ctx.quadraticCurveTo(0, tile * 0.38, tile * 0.08, tile * 0.62);
          ctx.quadraticCurveTo(tile * 0.24, tile * 0.24, tile * 0.20 * pulse, -tile * 0.08);
          ctx.closePath();
          ctx.fill();

          // Pale face and hollow eyes give the effect a figure-like presence without needing
          // a separate sprite sheet.
          ctx.shadowBlur = tile * 0.18;
          ctx.fillStyle = "rgba(180,242,255,0.92)";
          ctx.beginPath();
          ctx.ellipse(0, -tile * 0.24, tile * 0.16, tile * 0.19, 0, 0, Math.PI * 2);
          ctx.fill();
          ctx.fillStyle = "rgba(9,45,118,0.92)";
          for (const eye of [-1, 1]) {
            ctx.beginPath();
            ctx.ellipse(eye * tile * 0.058, -tile * 0.25, tile * 0.034, tile * 0.045, 0, 0, Math.PI * 2);
            ctx.fill();
          }

          // Two long spectral arms aim into the direction of travel; their three-fingered
          // tips close on impact.
          ctx.rotate(reachAngle);
          ctx.strokeStyle = "rgba(127,225,255,0.86)";
          ctx.lineCap = "round";
          ctx.lineWidth = tile * 0.07;
          for (const side of [-1, 1]) {
            ctx.beginPath();
            ctx.moveTo(0, side * tile * 0.04);
            ctx.quadraticCurveTo(tile * 0.20, side * tile * 0.20, tile * 0.39, side * tile * 0.13);
            ctx.stroke();
            for (let claw = -1; claw <= 1; claw += 1) {
              ctx.beginPath();
              ctx.moveTo(tile * 0.35, side * tile * 0.13);
              ctx.lineTo(tile * 0.49, side * tile * (0.13 + claw * 0.07));
              ctx.stroke();
            }
          }
          if (kHead >= 1) {
            ctx.strokeStyle = `rgba(212,251,255,${0.9 * fade})`;
            ctx.lineWidth = tile * 0.035;
            for (const side of [-1, 1]) {
              ctx.beginPath();
              ctx.arc(tile * 0.48, side * tile * 0.10, tile * (0.16 + afterglow * 0.28), side < 0 ? -1.9 : 1.9, side < 0 ? -0.35 : 0.35);
              ctx.stroke();
            }
          }
          ctx.restore();
          continue;
        }

        const minorArcaneBolt = m.kind === "arcaneBolt";

        if (minorArcaneBolt) {
          // Mage basic attack: two thin arcane pressure waves, then a runic impact at the target.
          const head = along(kHead);
          const angle = Math.atan2(dyT, dxT);
          ctx.save();
          ctx.globalCompositeOperation = "lighter";
          ctx.globalAlpha = 1 - afterglow;
          ctx.strokeStyle = "rgba(202,92,255,0.72)";
          ctx.lineWidth = Math.max(1, tile * 0.017);
          for (let ring = 1; ring <= 2; ring += 1) {
            const back = along(Math.max(0, kHead - ring * 0.1));
            ctx.beginPath();
            ctx.ellipse(back.x, back.y, tile * (0.09 + ring * 0.035), tile * (0.025 + ring * 0.012), angle + Math.PI / 4, 0, Math.PI * 2);
            ctx.stroke();
          }
          ctx.fillStyle = "rgba(244,150,255,0.92)";
          ctx.beginPath(); ctx.arc(head.x, head.y, tile * 0.035, 0, Math.PI * 2); ctx.fill();
          if (kHead >= 1) {
            ctx.strokeStyle = "rgba(255,110,220,0.88)";
            ctx.lineWidth = Math.max(1, tile * 0.014);
            for (let ray = 0; ray < 8; ray += 1) {
              const a = m.seed + ray * Math.PI / 4;
              const inner = tile * 0.05;
              const outer = tile * (0.12 + 0.09 * afterglow);
              ctx.beginPath();
              ctx.moveTo(head.x + Math.cos(a) * inner, head.y + Math.sin(a) * inner * 0.55);
              ctx.lineTo(head.x + Math.cos(a) * outer, head.y + Math.sin(a) * outer * 0.55);
              ctx.stroke();
            }
          }
          ctx.restore();
          continue;
        }

        // The light trace it leaves behind: a single stroke along the whole path already
        // flown, distinct from the comet below (which only ever hugs the head) — this is
        // what stays visible on the ground after the bolt has passed through.
        if (kHead > 0.02) {
          const steps = 16;
          ctx.beginPath();
          for (let i = 0; i <= steps; i++) {
            const p = along((i / steps) * kHead);
            if (i === 0) ctx.moveTo(p.x, p.y);
            else ctx.lineTo(p.x, p.y);
          }
          const traceFade = (1 - afterglow) * (physicalArrow ? 0.13 : minorArcaneBolt ? 0.44 : 0.55);
          ctx.lineCap = "round";
          ctx.lineJoin = "round";
          ctx.lineWidth = tile * (physicalArrow ? 0.018 : minorArcaneBolt ? 0.028 : 0.05);
          ctx.strokeStyle = physicalArrow ? `rgba(218,224,226,${traceFade})` : `hsla(${m.hue}, 90%, 74%, ${traceFade})`;
          ctx.shadowColor = physicalArrow ? `rgba(218,224,226,${traceFade})` : `hsla(${m.hue}, 95%, 70%, ${traceFade})`;
          ctx.shadowBlur = tile * (physicalArrow ? 0.1 : 0.4);
          ctx.stroke();
          ctx.shadowBlur = 0;
        }

        if (afterglow < 1) {
          // A bigger, punchier comet trail right behind the head.
          const cometCount = minorArcaneBolt ? 11 : 7;
          for (let i = cometCount; i >= 0; i--) {
            const tk = Math.max(0, kHead - i * 0.05);
            const p = along(tk);
            const fade = (1 - i / 8) * (1 - afterglow);
            const r = tile * (physicalArrow ? (0.045 - i * 0.004) : (minorArcaneBolt ? 0.64 : 1) * (0.16 - i * 0.016));
            if (r <= 0) continue;
            const g = ctx.createRadialGradient(p.x, p.y, 0, p.x, p.y, r * 3);
            g.addColorStop(0, physicalArrow ? `rgba(228,232,234,${fade * 0.18})` : `hsla(${m.hue}, 95%, 86%, ${fade})`);
            g.addColorStop(0.35, physicalArrow ? `rgba(150,158,162,${fade * 0.08})` : `hsla(${m.hue}, 92%, 68%, ${fade * 0.75})`);
            g.addColorStop(1, physicalArrow ? `rgba(120,130,136,0)` : `hsla(${m.hue}, 90%, 55%, 0)`);
            ctx.fillStyle = g;
            ctx.beginPath();
            ctx.arc(p.x, p.y, r * 3, 0, Math.PI * 2);
            ctx.fill();
          }
          const head = along(kHead);
          // A big soft aura around the head, well beyond the core, for real glow.
          const auraFade = 1 - afterglow;
          const aura = ctx.createRadialGradient(head.x, head.y, 0, head.x, head.y, tile * (physicalArrow ? 0.16 : minorArcaneBolt ? 0.34 : 0.55));
          aura.addColorStop(0, physicalArrow ? `rgba(230,234,236,${0.1 * auraFade})` : `hsla(${m.hue}, 100%, 85%, ${(minorArcaneBolt ? 0.34 : 0.55) * auraFade})`);
          aura.addColorStop(1, physicalArrow ? `rgba(180,188,192,0)` : `hsla(${m.hue}, 100%, 60%, 0)`);
          ctx.fillStyle = aura;
          ctx.beginPath();
          ctx.arc(head.x, head.y, tile * (physicalArrow ? 0.16 : minorArcaneBolt ? 0.34 : 0.55), 0, Math.PI * 2);
          ctx.fill();
          if (minorArcaneBolt) {
            // Basic mage attack: a compact scarlet lance, deliberately unlike Magic Missile.
            ctx.save();
            ctx.globalCompositeOperation = "lighter";
            ctx.globalAlpha = auraFade;
            const core = ctx.createRadialGradient(head.x, head.y, 0, head.x, head.y, tile * 0.18);
            core.addColorStop(0, "rgba(255,242,200,0.98)");
            core.addColorStop(0.22, "rgba(255,104,58,0.9)");
            core.addColorStop(0.62, "rgba(182,20,27,0.35)");
            core.addColorStop(1, "rgba(110,0,8,0)");
            ctx.fillStyle = core;
            ctx.beginPath(); ctx.arc(head.x, head.y, tile * 0.18, 0, Math.PI * 2); ctx.fill();
            ctx.strokeStyle = "rgba(255,96,55,0.72)";
            ctx.lineWidth = Math.max(1, tile * 0.018);
            for (let spark = 0; spark < 12; spark += 1) {
              const a = m.seed + spark * 2.399 + this.time * (2.2 + spark * 0.09);
              const radius = tile * (0.12 + ((spark * 7) % 6) * 0.018);
              const x = head.x + Math.cos(a) * radius;
              const y = head.y + Math.sin(a) * radius * 0.55;
              ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x - Math.cos(a) * tile * 0.065, y - Math.sin(a) * tile * 0.04); ctx.stroke();
            }
            ctx.restore();
          }
          const projectileCore = m.kind === "fireball" ? this.art.fireballCore : m.kind === "causticVenom" || m.kind === "minorVenom" ? this.art.causticVenomCore : null;
          if (m.kind === "longShot" && this.art.arrowCore) {
            // One shared approved arrow asset for normal shots, Multi Shot, Long Shot and Piercing Shot.
            const angle = Math.atan2(dyT, dxT);
            ctx.save();
            ctx.translate(head.x, head.y);
            ctx.rotate(angle);
            ctx.globalCompositeOperation = "source-over";
            ctx.globalAlpha = auraFade;
            ctx.drawImage(this.art.arrowCore, -tile * 0.56, -tile * 0.22, tile * 1.12, tile * 0.44);
            ctx.restore();
          }
          if (projectileCore) {
            // v2 art: a real alpha-cutout comet (dense ball toward the source's own
            // bottom-right corner, wispy tail trailing to the top-left), drawn with normal
            // alpha compositing now that it has actual transparency instead of the old v1's
            // flattened black background (which only ever worked via additive blending).
            const img = projectileCore;
            const flightAngle = Math.atan2(dyT, dxT);
            // The art's ball-and-tail sit on its own fixed diagonal (45°, bottom-right) —
            // rotating by the difference between that and the shot's actual flight angle
            // points the ball at the target regardless of cast direction, the same
            // orient-to-travel-direction treatment as Dreaming Web's shot (see
            // BattleEngine.webShotBeam).
            const pulse = 1 + 0.05 * Math.sin(this.time * 13 + m.seed);
            const w = tile * 1.9 * pulse;
            const h = (w * img.naturalHeight) / img.naturalWidth;
            ctx.save();
            ctx.translate(head.x, head.y);
            ctx.rotate(flightAngle - Math.PI / 4);
            ctx.globalCompositeOperation = "source-over";
            ctx.globalAlpha = auraFade;
            ctx.drawImage(img, -w / 2, -h / 2, w, h);
            ctx.restore();
          }
          ctx.fillStyle = `rgba(255,255,255,${(physicalArrow ? 0 : 0.95) * auraFade})`;
          ctx.beginPath();
          ctx.arc(head.x, head.y, tile * (minorArcaneBolt ? 0.05 : 0.085), 0, Math.PI * 2);
          ctx.fill();
        }
      }
      ctx.globalAlpha = 1;
    }
  }

  private drawBladeFx(ctx: any, tile: number): void {
    if (!this.bladeFxLive) return;
    for (const b of this.bladeFx) {
      if (!b.live) continue;
      const k = b.t / b.max;
      const { cx, cy } = this.hexCenter(b.x, b.y);
      ctx.save();
      ctx.lineCap = "round";
      ctx.lineJoin = "round";

      if (b.kind === "arc") {
        // A filled crescent (outer arc forward, inner arc back) rather than a thin translucent
        // stroke — reads as an actual blade sweep at a glance instead of a faint smear, and a
        // dark source-over outline first keeps it legible over bright ground art that would
        // otherwise wash out a purely additive white streak.
        const swingEnd = 0.42;
        const swing = Math.min(1, k / swingEnd);
        const fadeStart = 0.48;
        const fade = k < fadeStart ? 1 : Math.max(0, 1 - (k - fadeStart) / (1 - fadeStart));
        if (fade > 0.01) {
          // Adjacent hex centers sit tile*sqrt3 (~1.73*tile) apart, not ~1*tile — the band has
          // to actually stretch out past the attacker's own hex and across the 3 target hexes'
          // centers, or the whole sweep reads as a small smudge sitting on the attacker instead
          // of a blade cutting through the fanned-out hexes.
          const rOuter = tile * 2.05;
          const rInner = tile * 0.95;
          const end = b.a0 + (b.a1 - b.a0) * swing;
          const path = new Path2D();
          path.arc(cx, cy, rOuter, b.a0, end, false);
          path.arc(cx, cy, rInner, end, b.a0, true);
          path.closePath();

          ctx.globalCompositeOperation = "source-over";
          ctx.globalAlpha = fade;
          ctx.strokeStyle = "rgba(10,14,22,0.85)";
          ctx.lineWidth = tile * 0.055;
          ctx.stroke(path);

          const grad = ctx.createRadialGradient(cx, cy, rInner, cx, cy, rOuter);
          if (b.warm) {
            grad.addColorStop(0, "rgba(255,150,60,0.12)");
            grad.addColorStop(0.42, "rgba(255,255,255,0.97)");
            grad.addColorStop(0.75, "rgba(255,195,120,0.92)");
            grad.addColorStop(1, "rgba(255,140,50,0.1)");
          } else {
            grad.addColorStop(0, "rgba(170,205,255,0.12)");
            grad.addColorStop(0.42, "rgba(255,255,255,0.98)");
            grad.addColorStop(0.75, "rgba(205,228,255,0.92)");
            grad.addColorStop(1, "rgba(150,195,255,0.1)");
          }
          ctx.fillStyle = grad;
          ctx.fill(path);

          ctx.globalCompositeOperation = "lighter";
          ctx.shadowColor = b.warm ? "rgba(255,150,55,0.9)" : "rgba(190,220,255,0.9)";
          ctx.shadowBlur = tile * 0.4;
          ctx.fillStyle = b.warm ? `rgba(255,180,100,${0.3 * fade})` : `rgba(205,228,255,${0.3 * fade})`;
          ctx.fill(path);
          ctx.shadowBlur = 0;

          if (swing < 1) {
            const midR = (rOuter + rInner) / 2;
            const tipX = cx + Math.cos(end) * midR;
            const tipY = cy + Math.sin(end) * midR;
            const flash = ctx.createRadialGradient(tipX, tipY, 0, tipX, tipY, tile * 0.42);
            flash.addColorStop(0, `rgba(255,255,255,${0.95 * fade})`);
            flash.addColorStop(1, "rgba(255,255,255,0)");
            ctx.fillStyle = flash;
            ctx.beginPath();
            ctx.arc(tipX, tipY, tile * 0.42, 0, Math.PI * 2);
            ctx.fill();
          }
        }
      } else if (b.kind === "rushTrail") {
        // Bull Rush: the dash's streak, left behind and fading once the charger has stopped.
        this.drawRushStreak(ctx, tile, this.hexCenter(b.x, b.y), this.hexCenter(b.toX, b.toY), Math.max(0, 1 - k));
      } else if (b.kind === "rushImpact") {
        // Bull Rush's hit: a golden forward burst — flash, shock arcs opening in the charge
        // direction, sparks sprayed ahead. Deliberately nothing like Sweep's ring.
        const fade = Math.max(0, 1 - k);
        const chestY = cy - tile * 0.45;
        ctx.globalCompositeOperation = "lighter";
        const flash = ctx.createRadialGradient(cx, chestY, 0, cx, chestY, tile * (0.7 + k * 0.8));
        flash.addColorStop(0, `rgba(255,248,215,${0.85 * fade})`);
        flash.addColorStop(0.4, `rgba(255,200,80,${0.45 * fade})`);
        flash.addColorStop(1, "rgba(255,160,40,0)");
        ctx.fillStyle = flash;
        ctx.beginPath();
        ctx.arc(cx, chestY, tile * (0.7 + k * 0.8), 0, Math.PI * 2);
        ctx.fill();
        ctx.lineCap = "round";
        ctx.shadowColor = "rgba(255,210,90,0.95)";
        ctx.shadowBlur = tile * 0.3;
        for (let j = 0; j < 3; j++) {
          const r = tile * (0.35 + k * 1.5) - j * tile * 0.22;
          if (r <= 0) continue;
          ctx.strokeStyle = `rgba(255,${230 - j * 30},${170 - j * 50},${(0.9 - j * 0.22) * fade})`;
          ctx.lineWidth = Math.max(1.5, tile * (0.08 - j * 0.02));
          ctx.beginPath();
          ctx.arc(cx - Math.cos(b.a0) * tile * 0.3, chestY - Math.sin(b.a0) * tile * 0.3, r, b.a0 - 0.85, b.a0 + 0.85);
          ctx.stroke();
        }
        ctx.shadowBlur = 0;
        for (let i = 0; i < 10; i++) {
          const ang = b.a0 + (Math.sin(b.seed * 9.7 + i * 5.3) * 0.5) * 1.3;
          const r0 = tile * (0.25 + k * 0.6);
          const r1 = r0 + tile * (0.3 + (i % 3) * 0.15) * (1 - k * 0.5);
          ctx.strokeStyle = `rgba(255,236,180,${0.85 * fade})`;
          ctx.lineWidth = Math.max(1, tile * 0.025);
          ctx.beginPath();
          ctx.moveTo(cx + Math.cos(ang) * r0, chestY + Math.sin(ang) * r0 * 0.7);
          ctx.lineTo(cx + Math.cos(ang) * r1, chestY + Math.sin(ang) * r1 * 0.7);
          ctx.stroke();
        }
      } else if (b.kind === "execution") {
        // Golpe do Carrasco: a heavy axe chop — a wide curved blade crashing down through the
        // target, then an impact ring and cracks on the ground. `warm` = the execution
        // triggered: bigger, with a blood-red edge and flash.
        const exec = b.warm;
        const sz = exec ? 1.35 : 1;
        const fall = Math.min(1, k / 0.28);
        const fade = k < 0.4 ? 1 : Math.max(0, 1 - (k - 0.4) / 0.6);
        if (fade > 0.01) {
          const topY = cy - tile * 2.2 * sz;
          const botY = cy + tile * 0.15;
          const headY = topY + (botY - topY) * fall * fall;
          const w = tile * 0.8 * sz;
          const blade = new Path2D();
          const side = b.mirrorX ? -1 : 1;
          blade.moveTo(cx - w * 0.25 * side, topY);
          blade.quadraticCurveTo(cx + w * 1.7 * side, (topY + headY) / 2, cx, headY);
          blade.quadraticCurveTo(cx + w * 0.15 * side, (topY + headY) / 2, cx - w * 0.25 * side, topY);
          blade.closePath();
          ctx.globalCompositeOperation = "source-over";
          ctx.globalAlpha = fade;
          ctx.strokeStyle = "rgba(10,12,18,0.85)";
          ctx.lineWidth = tile * 0.07;
          ctx.stroke(blade);
          ctx.fillStyle = exec ? "rgba(255,226,218,0.97)" : "rgba(244,248,255,0.97)";
          ctx.fill(blade);
          ctx.globalCompositeOperation = "lighter";
          ctx.shadowColor = exec ? "rgba(255,40,28,0.95)" : "rgba(210,228,255,0.9)";
          ctx.shadowBlur = tile * 0.45 * sz;
          ctx.fillStyle = exec ? `rgba(255,70,50,${0.55 * fade})` : `rgba(200,225,255,${0.45 * fade})`;
          ctx.fill(blade);
          ctx.shadowBlur = 0;
          if (fall >= 1) {
            const ik = Math.min(1, (k - 0.28) / 0.5);
            const r = tile * (0.4 + ik * 1.2) * sz;
            ctx.strokeStyle = exec ? `rgba(255,60,40,${0.8 * (1 - ik)})` : `rgba(230,238,255,${0.7 * (1 - ik)})`;
            ctx.lineWidth = Math.max(1.5, tile * 0.06 * (1 - ik));
            ctx.beginPath();
            ctx.ellipse(cx, botY, r, r * 0.35, 0, 0, Math.PI * 2);
            ctx.stroke();
            ctx.strokeStyle = exec ? `rgba(255,120,90,${0.7 * (1 - ik)})` : `rgba(220,230,245,${0.6 * (1 - ik)})`;
            ctx.lineWidth = Math.max(1, tile * 0.025);
            for (let i = 0; i < 6; i++) {
              const a = b.seed + i * 1.05;
              const len = tile * (0.5 + (i % 3) * 0.18) * sz * Math.min(1, ik * 2.5);
              ctx.beginPath();
              ctx.moveTo(cx, botY);
              ctx.lineTo(cx + Math.cos(a) * len * 0.5, botY + Math.sin(a) * len * 0.18);
              ctx.lineTo(cx + Math.cos(a + 0.25) * len, botY + Math.sin(a + 0.25) * len * 0.35);
              ctx.stroke();
            }
            if (exec) {
              const chestY = cy - tile * 0.6;
              const flash = ctx.createRadialGradient(cx, chestY, 0, cx, chestY, tile * 1.4);
              flash.addColorStop(0, `rgba(255,120,90,${0.6 * (1 - ik)})`);
              flash.addColorStop(1, "rgba(160,0,0,0)");
              ctx.fillStyle = flash;
              ctx.beginPath();
              ctx.arc(cx, chestY, tile * 1.4, 0, Math.PI * 2);
              ctx.fill();
            }
          }
        }
      } else if (b.kind === "tripSweep") {
        // Rasteira: a low curved blade sweep across the target's feet, in the direction of the
        // attack, then a spray of blood droplets — the wound that leaves it Bleeding.
        const sweep = Math.min(1, k / 0.35);
        const fade = k < 0.45 ? 1 : Math.max(0, 1 - (k - 0.45) / 0.55);
        if (fade > 0.01) {
          const dirSign = Math.cos(b.a0) >= 0 ? 1 : -1;
          const fy = cy + tile * 0.32;
          const rx = tile * 0.95;
          const ry = tile * 0.3;
          const aStart = dirSign > 0 ? Math.PI : 0;
          const aEnd = aStart - dirSign * Math.PI * Math.max(0.02, sweep);
          const ccw = dirSign > 0;
          const cut = new Path2D();
          cut.ellipse(cx, fy, rx, ry, 0, aStart, aEnd, ccw);
          cut.ellipse(cx, fy, rx * 0.7, ry * 0.5, 0, aEnd, aStart, !ccw);
          cut.closePath();
          ctx.globalCompositeOperation = "source-over";
          ctx.globalAlpha = fade;
          ctx.strokeStyle = "rgba(10,12,18,0.85)";
          ctx.lineWidth = tile * 0.06;
          ctx.stroke(cut);
          ctx.fillStyle = "rgba(244,248,255,0.96)";
          ctx.fill(cut);
          ctx.globalCompositeOperation = "lighter";
          ctx.shadowColor = "rgba(210,228,255,0.9)";
          ctx.shadowBlur = tile * 0.3;
          ctx.fillStyle = `rgba(200,225,255,${0.45 * fade})`;
          ctx.fill(cut);
          ctx.shadowBlur = 0;
          // Blood: droplets thrown up from the shins, arcing and falling.
          const bk = (k - 0.25) / 0.75;
          if (bk > 0) {
            ctx.globalCompositeOperation = "source-over";
            const shinY = cy + tile * 0.05;
            for (let i = 0; i < 14; i++) {
              const hash = (n: number) => {
                const v = Math.sin(n * 12.9898 + b.seed * 78.233) * 43758.5453;
                return v - Math.floor(v);
              };
              const r1 = hash(i + 1);
              const r2 = hash(i + 31);
              const vx = (dirSign * (0.2 + r1 * 1.1) + (r2 - 0.5) * 1.2) * tile;
              const vy = (0.5 + r2 * 1.2) * tile;
              const x = cx + vx * bk;
              const y = shinY - vy * bk + tile * 1.9 * bk * bk;
              const size = tile * (0.05 + r1 * 0.045) * (1 - bk * 0.35);
              ctx.globalAlpha = fade * Math.min(1, (1 - bk) * 2);
              ctx.fillStyle = "rgba(168,12,18,0.97)";
              ctx.beginPath();
              ctx.ellipse(x, y, size, size * 1.35, Math.atan2(vy, vx), 0, Math.PI * 2);
              ctx.fill();
              ctx.fillStyle = "rgba(255,95,90,0.85)";
              ctx.beginPath();
              ctx.arc(x - size * 0.3, y - size * 0.3, size * 0.35, 0, Math.PI * 2);
              ctx.fill();
            }
          }
        }
      } else if (b.kind === "cross" || b.kind === "lowCut") {
        const low = b.kind === "lowCut";
        const fade = Math.max(0, 1 - k * (low ? 1.25 : 1.1));
        if (fade > 0.01) {
          const len = tile * (low ? 0.78 : 1.05);
          const ang = low ? 0.07 : b.a0;
          const oy = low ? tile * 0.3 : -tile * 0.15;
          const dx = Math.cos(ang) * len * 0.5;
          const dy = Math.sin(ang) * len * 0.5;
          ctx.translate(cx, cy + oy);
          ctx.globalCompositeOperation = "source-over";
          ctx.globalAlpha = fade;
          ctx.strokeStyle = "rgba(10,14,22,0.85)";
          ctx.lineWidth = tile * (low ? 0.13 : 0.16);
          ctx.beginPath();
          ctx.moveTo(-dx, -dy);
          ctx.lineTo(dx, dy);
          ctx.stroke();
          ctx.strokeStyle = "rgba(255,255,255,0.98)";
          ctx.lineWidth = tile * (low ? 0.05 : 0.065);
          ctx.beginPath();
          ctx.moveTo(-dx, -dy);
          ctx.lineTo(dx, dy);
          ctx.stroke();
          ctx.globalCompositeOperation = "lighter";
          ctx.strokeStyle = `rgba(205,228,255,${0.55 * fade})`;
          ctx.lineWidth = tile * (low ? 0.22 : 0.26);
          ctx.shadowColor = "rgba(205,228,255,0.9)";
          ctx.shadowBlur = tile * 0.3;
          ctx.beginPath();
          ctx.moveTo(-dx, -dy);
          ctx.lineTo(dx, dy);
          ctx.stroke();
        }
      } else if (b.kind === "ring" || b.kind === "shockRing") {
        const tight = b.kind === "shockRing";
        const fade = Math.max(0, 1 - k);
        if (fade > 0.01) {
          // Same real hex spacing as the arc above (neighbor centers ~1.73*tile out) — Sweep's
          // ring needs to visibly wash out past the first ring of hexes, not stay pinned close
          // to the caster's own tile.
          const r = tile * (tight ? 0.35 + k * 1.05 : 0.55 + k * 2.05);
          ctx.translate(cx, cy);
          ctx.scale(1, tight ? 0.62 : 0.5);
          ctx.globalCompositeOperation = "source-over";
          ctx.globalAlpha = fade;
          ctx.strokeStyle = "rgba(10,14,22,0.75)";
          ctx.lineWidth = tile * (tight ? 0.14 : 0.2) * (1 - k * 0.4);
          ctx.beginPath();
          ctx.arc(0, 0, r, 0, Math.PI * 2);
          ctx.stroke();
          ctx.strokeStyle = "rgba(255,255,255,0.95)";
          ctx.lineWidth = tile * (tight ? 0.06 : 0.09) * (1 - k * 0.4);
          ctx.beginPath();
          ctx.arc(0, 0, r, 0, Math.PI * 2);
          ctx.stroke();
          ctx.globalCompositeOperation = "lighter";
          ctx.strokeStyle = `rgba(205,228,255,${0.55 * fade})`;
          ctx.lineWidth = tile * (tight ? 0.17 : 0.24);
          ctx.shadowColor = "rgba(205,228,255,0.9)";
          ctx.shadowBlur = tile * 0.3;
          ctx.beginPath();
          ctx.arc(0, 0, r, 0, Math.PI * 2);
          ctx.stroke();
          if (tight) {
            ctx.globalCompositeOperation = "source-over";
            ctx.strokeStyle = `rgba(255,255,255,${0.5 * fade})`;
            ctx.lineWidth = tile * 0.03;
            ctx.beginPath();
            ctx.arc(0, 0, r * 0.68, 0, Math.PI * 2);
            ctx.stroke();
          }
        }
      } else if (b.kind === "dash") {
        const travel = b.max * 0.55;
        const kHead = Math.min(1, b.t / travel);
        const fade = b.t < travel ? 1 : Math.max(0, 1 - (b.t - travel) / (b.max - travel));
        if (fade > 0.01) {
          const to = this.hexCenter(b.toX, b.toY);
          const headX = cx + (to.cx - cx) * kHead;
          const headY = cy + (to.cy - cy) * kHead;
          const dx = to.cx - cx;
          const dy = to.cy - cy;
          const len = Math.hypot(dx, dy) || 1;
          const nx = -dy / len;
          const ny = dx / len;

          ctx.globalCompositeOperation = "source-over";
          ctx.globalAlpha = fade;
          ctx.strokeStyle = "rgba(10,14,22,0.85)";
          ctx.lineWidth = tile * 0.16;
          ctx.beginPath();
          ctx.moveTo(cx, cy);
          ctx.lineTo(headX, headY);
          ctx.stroke();
          ctx.strokeStyle = "rgba(255,255,255,0.98)";
          ctx.lineWidth = tile * 0.07;
          ctx.beginPath();
          ctx.moveTo(cx, cy);
          ctx.lineTo(headX, headY);
          ctx.stroke();

          ctx.globalCompositeOperation = "lighter";
          ctx.strokeStyle = `rgba(205,228,255,${0.55 * fade})`;
          ctx.lineWidth = tile * 0.3;
          ctx.shadowColor = "rgba(205,228,255,0.9)";
          ctx.shadowBlur = tile * 0.35;
          ctx.beginPath();
          ctx.moveTo(cx, cy);
          ctx.lineTo(headX, headY);
          ctx.stroke();
          ctx.shadowBlur = 0;

          ctx.globalCompositeOperation = "source-over";
          for (let i = 0; i < 3; i++) {
            const t2 = Math.max(0, kHead - i * 0.16);
            const px = cx + dx * t2;
            const py = cy + dy * t2;
            const w = tile * (0.22 - i * 0.05);
            ctx.strokeStyle = `rgba(255,255,255,${(0.5 - i * 0.14) * fade})`;
            ctx.lineWidth = tile * 0.025;
            ctx.beginPath();
            ctx.moveTo(px - nx * w, py - ny * w);
            ctx.lineTo(px + nx * w, py + ny * w);
            ctx.stroke();
          }

          if (kHead < 1) {
            const flash = ctx.createRadialGradient(headX, headY, 0, headX, headY, tile * 0.32);
            flash.addColorStop(0, `rgba(255,255,255,${0.95 * fade})`);
            flash.addColorStop(1, "rgba(255,255,255,0)");
            ctx.globalCompositeOperation = "lighter";
            ctx.fillStyle = flash;
            ctx.beginPath();
            ctx.arc(headX, headY, tile * 0.32, 0, Math.PI * 2);
            ctx.fill();
          }
        }
      }
      ctx.restore();
    }
    ctx.globalAlpha = 1;
  }

  private drawDivineLight(
    ctx: any,
    cx: number,
    cy: number,
    tile: number,
    fx: HolyFx,
    fade: number,
    k: number,
  ): void {
    const medium = fx.kind === "medium" || fx.kind === "food";
    const hands = fx.kind === "hands";
    const disease = fx.kind === "disease";
    const h = disease ? 158 : fx.kind === "food" ? 208 : 46;
    const s = disease ? 80 : fx.kind === "food" ? 90 : 95;
    const height = tile * (medium ? 2.85 : hands ? 2.2 : disease ? 2.25 : 1.55);
    const width = tile * (medium ? 0.58 : hands ? 0.45 : disease ? 0.48 : 0.32);
    const topY = cy - height;
    const chestY = cy - tile * 0.55;
    const pulse = 0.88 + 0.12 * Math.abs(Math.sin(this.time * 7 + fx.seed));

    const shaft = ctx.createLinearGradient(cx, topY, cx, cy + tile * 0.1);
    shaft.addColorStop(0, `hsla(${h}, ${s}%, 96%, 0)`);
    shaft.addColorStop(0.18, `hsla(${h}, ${s}%, 94%, ${(medium ? 0.42 : hands ? 0.32 : 0.22) * fade * pulse})`);
    shaft.addColorStop(0.55, `hsla(${h}, ${s}%, 82%, ${(medium ? 0.55 : hands ? 0.435 : 0.32) * fade})`);
    shaft.addColorStop(0.88, `hsla(${h}, ${s}%, 78%, ${(medium ? 0.28 : hands ? 0.22 : 0.16) * fade})`);
    shaft.addColorStop(1, `hsla(${h}, ${s}%, 70%, 0)`);
    ctx.fillStyle = shaft;
    ctx.beginPath();
    ctx.moveTo(cx - width * 0.22, topY);
    ctx.lineTo(cx + width * 0.22, topY);
    ctx.lineTo(cx + width, cy + tile * 0.08);
    ctx.lineTo(cx - width, cy + tile * 0.08);
    ctx.closePath();
    ctx.fill();

    const coreW = width * (medium ? 0.28 : hands ? 0.25 : 0.22);
    const core = ctx.createLinearGradient(cx, topY, cx, cy);
    core.addColorStop(0, `hsla(${h}, 40%, 100%, ${0.55 * fade})`);
    core.addColorStop(0.7, `hsla(${h}, 80%, 96%, ${(medium ? 0.85 : hands ? 0.675 : 0.5) * fade})`);
    core.addColorStop(1, `hsla(${h}, 80%, 90%, 0)`);
    ctx.fillStyle = core;
    ctx.fillRect(cx - coreW, topY, coreW * 2, cy - topY);

    const bloomR = tile * (medium ? 1.45 : hands ? 1.13 : disease ? 1.2 : 0.82);
    const bloom = ctx.createRadialGradient(cx, chestY, 0, cx, chestY, bloomR);
    bloom.addColorStop(0, `hsla(${h}, 90%, 96%, ${(medium ? 0.72 : hands ? 0.56 : 0.4) * fade * pulse})`);
    bloom.addColorStop(0.35, `hsla(${h}, ${s}%, 72%, ${(medium ? 0.38 : hands ? 0.29 : 0.2) * fade})`);
    bloom.addColorStop(1, `hsla(${h}, ${s}%, 60%, 0)`);
    ctx.fillStyle = bloom;
    ctx.beginPath();
    ctx.arc(cx, chestY, bloomR, 0, Math.PI * 2);
    ctx.fill();

    const groundR = tile * (medium ? 0.85 : hands ? 0.7 : 0.55) * (0.7 + k * 0.35);
    const ground = ctx.createRadialGradient(cx, cy, 0, cx, cy, groundR);
    ground.addColorStop(0, `hsla(${h}, ${s}%, 90%, ${(medium ? 0.55 : hands ? 0.425 : 0.3) * fade})`);
    ground.addColorStop(1, `hsla(${h}, ${s}%, 70%, 0)`);
    ctx.fillStyle = ground;
    ctx.beginPath();
    ctx.ellipse(cx, cy + tile * 0.06, groundR, groundR * 0.38, 0, 0, Math.PI * 2);
    ctx.fill();

    if (disease) {
      const ringR = tile * (0.22 + k * 0.95);
      ctx.strokeStyle = `hsla(158, 90%, 72%, ${0.55 * fade})`;
      ctx.lineWidth = Math.max(1.5, tile * 0.045);
      ctx.beginPath();
      ctx.ellipse(cx, cy + tile * 0.04, ringR, ringR * 0.4, 0, 0, Math.PI * 2);
      ctx.stroke();
    }

    if (hands) {
      // Two cupped arcs gather light toward the receiver like a pair of healing hands.
      ctx.strokeStyle = `hsla(42, 95%, 82%, ${0.7 * fade * pulse})`;
      ctx.lineWidth = Math.max(1.5, tile * 0.035);
      for (const side of [-1, 1]) {
        ctx.beginPath();
        ctx.moveTo(cx + side * tile * 0.48, chestY - tile * 0.25);
        ctx.bezierCurveTo(cx + side * tile * 0.72, chestY + tile * 0.2, cx + side * tile * 0.3, chestY + tile * 0.5, cx + side * tile * 0.08, chestY + tile * 0.22);
        ctx.stroke();
      }
    }
    const motes = medium ? 16 : hands ? 11 : disease ? 12 : 7;
    for (let i = 0; i < motes; i++) {
      const rise = ((fx.seed * 13 + i * 0.37 + k * (medium ? 1.6 : hands ? 1.35 : 1.1)) % 1);
      const sway = Math.sin(fx.seed + i * 1.7 + this.time * 3) * tile * 0.18;
      const mx = cx + sway + ((i % 5) - 2) * tile * 0.08;
      const my = cy - rise * height * 0.95;
      const r = tile * (medium ? 0.045 : hands ? 0.0375 : 0.03) * (1 - rise * 0.4) * fade;
      ctx.fillStyle = `hsla(${h}, 80%, 96%, ${(0.55 + (i % 3) * 0.15) * fade})`;
      ctx.beginPath();
      ctx.arc(mx, my, Math.max(0.8, r), 0, Math.PI * 2);
      ctx.fill();
    }
  }

  private drawPotionBurst(
    ctx: any,
    cx: number,
    cy: number,
    tile: number,
    fx: HolyFx,
    fade: number,
    k: number,
  ): void {
    const chestY = cy - tile * 0.5;
    const aura = ctx.createRadialGradient(cx, chestY, 0, cx, chestY, tile * 0.95);
    aura.addColorStop(0, `hsla(32, 100%, 88%, ${0.55 * fade})`);
    aura.addColorStop(0.4, `hsla(22, 95%, 58%, ${0.32 * fade})`);
    aura.addColorStop(1, "hsla(18, 90%, 40%, 0)");
    ctx.fillStyle = aura;
    ctx.beginPath();
    ctx.arc(cx, chestY, tile * 0.95, 0, Math.PI * 2);
    ctx.fill();

    for (let i = 0; i < 12; i++) {
      const ang = fx.seed + i * 0.52 + fx.t * (2.8 + (i % 3) * 0.4);
      const rad = tile * (0.18 + (i % 4) * 0.06) * (0.85 + Math.sin(fx.t * 6 + i) * 0.12);
      const px = cx + Math.cos(ang) * rad;
      const py = chestY + Math.sin(ang) * rad * 0.72 - tile * 0.08 * Math.sin(fx.t * 5 + i);
      const r = tile * (0.04 + (i % 3) * 0.012) * fade;
      ctx.fillStyle = i % 3 === 0 ? `hsla(48, 100%, 88%, ${0.9 * fade})` : `hsla(22, 95%, 62%, ${0.75 * fade})`;
      ctx.beginPath();
      ctx.arc(px, py, Math.max(1, r), 0, Math.PI * 2);
      ctx.fill();
    }

    const splash = tile * (0.28 + k * 0.42);
    ctx.strokeStyle = `hsla(28, 95%, 70%, ${0.55 * fade})`;
    ctx.lineWidth = Math.max(1.4, tile * 0.04);
    ctx.beginPath();
    ctx.ellipse(cx, cy + tile * 0.05, splash, splash * 0.38, 0, 0, Math.PI * 2);
    ctx.stroke();
    const puddle = ctx.createRadialGradient(cx, cy + tile * 0.05, 0, cx, cy + tile * 0.05, splash);
    puddle.addColorStop(0, `hsla(36, 100%, 80%, ${0.4 * fade})`);
    puddle.addColorStop(1, "hsla(24, 90%, 50%, 0)");
    ctx.fillStyle = puddle;
    ctx.beginPath();
    ctx.ellipse(cx, cy + tile * 0.05, splash, splash * 0.38, 0, 0, Math.PI * 2);
    ctx.fill();

    for (let i = 0; i < 8; i++) {
      const rise = ((fx.seed + i * 0.21 + k * 1.2) % 1);
      ctx.fillStyle = `hsla(48, 100%, 92%, ${(0.7 - rise * 0.4) * fade})`;
      ctx.beginPath();
      ctx.arc(cx + Math.sin(fx.seed + i * 2) * tile * 0.22, cy - rise * tile * 1.05, tile * 0.028 * fade, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

// Keep the open campaign's battle state while installing current combat rules after HMR.
if (import.meta.hot) {
  const live = (window as Window & { __emberEngine?: BattleEngine }).__emberEngine;
  if (live) BattleEngine.refreshLiveEngine(live);
  import.meta.hot.accept();
}
