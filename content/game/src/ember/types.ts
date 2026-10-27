export type WeaponType = "sword" | "axe" | "mace" | "hammer" | "staff" | "spear" | "bow" | "crossbow" | "dagger";
export type WeaponSkillValues = Partial<Record<WeaponType, number>>;

export type ResistanceElement = "fire" | "lightning" | "ice" | "arcane" | "darkness" | "holy" | "poison" | "ember";
export type Resistances = Partial<Record<ResistanceElement, number>>;

export type MapTimeOfDay = "day" | "noon" | "dawn" | "dusk" | "brightNight" | "darkNight";
export type PotionId ="mid" | "weak" | "potent" | "disease" | "manaSmall" | "manaMid" | "manaLarge";
/** Poison strength, weakest to strongest — dice and Poison Resistance penalties live in POISON_TIERS (poison.ts). */
export type PoisonTier = "lesser" | "poison" | "greater" | "deadly" | "lethal";

export interface Bag {
  mid: number;
  weak: number;
  potent: number;
  disease: number;
  manaSmall: number;
  manaMid: number;
  manaLarge: number;
  /** Gazuas: abrem baús e portas trancadas no mapa (não são poção, contam à parte). */
  lockpick: number;
}

export interface Spells {
  tier1: number;
  tier2: number;
  tier3: number;
  tier4: number;
  tier5: number;
  tier6: number;
  tier7: number;
  tier8: number;
  tier9: number;
  tier10: number;
}

export const TIER_KEYS = ["tier1", "tier2", "tier3", "tier4", "tier5", "tier6", "tier7", "tier8", "tier9", "tier10"] as const;
export type TierKey = (typeof TIER_KEYS)[number];

export type TerrainId = "plains" | "woods" | "ruins" | "water" | "ember" | "hill" | "flame" | "column" | "nave" | "barricade" | "door" | "void" | "snow";
/** Which faction a unit fights for.
 *
 * "neutral" is the wild-beast side: it holds its ground (never enters the turn order, so it
 * takes no turn and the AI never runs for it), it is not what a "clear the map" victory
 * counts, and it can still be attacked by the player. Striking one wakes every living
 * neutral of that same class on the map — the whole species turns "enemy" at once (see
 * BattleEngine.provoke) and starts acting from the following round. Nothing turns back. */
export type Side = "player" | "enemy" | "neutral";
export type ClassId = import("./encounter-npcs").EncounterNpcId
  | "ancientGolem"
  | "swordsman"
  | "archer"
  | "mage"
  | "healer"
  | "soldier"
  | "brigand"
  | "baseCrossbowman"
  | "brigandCrossbowman"
  | "stagWolf"
  | "bossVoss"
  | "captain"
  | "cultist"
  | "cultistV2"
  | "miliciaV2"
  | "apparition"
  | "minorHorror"
  | "horror"
  | "asherah"
  | "pikeman"
  | "basePikeman"
  | "wardog"
  | "wardog2"
  | "emberedWraith"
  | "zombie" | "zombie2"
  | "undeadOx"
  | "plagueBearingCattle"
  | "troll"
  | "troll2"
  | "roccoTheBird"
  | "morvenianWolf"
  | "mordavianWolf"
  | "mordavianWolfFinal"
  | "jacare"
  | "punisher"
  | "theButcher"
  | "birolho"
  | "birolho2"
  | "birolho3"
  | "birolhoLegs"
  | "birolhoLegs2"
  // Big boss (Type 7 body) with the Birolho spell kit and much higher stats — see
  // CLASSES.carnivorousPlant.
  | "carnivorousPlant"
  // One-hex creature drawn filling its whole hex — see CLASSES.sapling.
  | "sapling"
  | "swampBlueCalf"
  | "bigBlueCalf"
  // Weak, killable flavor civilians (see SpriteId's own note) — random encounters and regular
  // maps, deliberately not the (not yet built) city hubs, where a civilian instance would carry
  // Spawn.dialog instead and never be a combat target at all.
  | "beberrao"
  | "breadLady"
  | "brue"
  | "crazyLady"
  | "mudinho"
  | "oldHealer"
  | "peasant1"
  | "shadyPatron"
  | "soupLady"
  | "villagerF1"
  | "woodsman"
  | "assassin"
  | "rogue"
  | "lancer"
  | "aldric"
  | "sandoval"
  | "kaelFinal"
  | "kaelEarly"
  | "neera"
  | "voss"
  | "salazar"
  | "conjurer"
  | "paladin"
  | "heavyKnight"
  // Promoted classes (promotion at level 15) — provisional stats/sprites, wired for
  // spell-slot progression only. Combat stats, real art and the promotion quest come later.
  | "elementalist"
  | "warlock"
  | "sorcerer"
  | "necromancer"
  | "cleric"
  | "bishop"
  | "ranger"
  | "sentinel"
  | "templar"
  // Conjurer tier 1 (Summon Familiar): not a recruitable class — its combat stats are
  // computed live from its summoner (see castSummonFamiliar), CLASSES.familiar only
  // supplies a sprite/size/range fallback and satisfies the ClassId-keyed tables below.
  | "familiar"
  // Stronger evolution of the same summon, cast once the conjurer has promoted (level 15+,
  // sorcerer/necromancer) — see castSummonFamiliar. Same "stats computed live, this is only
  // a fallback" deal as "familiar" above.
  | "familiar2"
  // Conjurer tier 3 (Summon Familiar Titã, "the Big Guy"): a full-strength summon (100% of
  // the conjurer's current attributes, not a fraction like familiar/familiar2) that can also
  // cast its own Fireball a few times a battle — see familiarSpellCharges/Unit.spellCharges.
  | "familiar3"
  // Conjurer tier 3 (Summon Familiar Radiante): Familiar Maior's kit and stat share on a new
  // body — see SUMMON_FAMILIAR4.
  | "familiar4"
  // Conjurer tier 5 (Invocar Cão Zumbi): a summoned zombie dog — see SUMMON_ZOMBIE_DOG.
  | "zombieDog"
  | "travelingMerchant";
export type SpriteId = "defaultWarrior" | "neera" | "voss" | "salazar" | "aldric" | "malrec" | "defaultLancer" | "soldier" | "brigand" | "captain" | "sorcerer" | "horror" | "minor-horror-001" | "Asherah" | "pikeman" | "wardog" | "wardog2" | "EmberedWraith" | "zombie" | "zombie2" | "undeadOx" | "plague-bearing-cattle" | "troll" | "troll2" | "RoccoTheBird" | "morvenian-wolf" | "mordavian-wolf" | "mordavian-wolf-final" | "punisher" | "theButcher" | "birolho" | "birolho2" | "birolho3" | "BirolhoLegs" | "BirolhoLegs2" | "familiar" | "familiar2" | "familiar3" | "familiar4" | "zombieDog" | "swamp-blue-calf" | "cobalt-blue-deer" | "big-blue-ox-002" | "ancient-golem" | "lancer" | "sandoval" | "kaelFinal" | "kaelEarly" | "conjurer" | "cultist-v2"
  // Milícia V2: idle/hit/death/walk right/walk left, 36 frames each (work/militia_v2_build.py).
  | "militia-v2"
  | "BasePikeman"
  | "BaseCrossbowman"
  | "BrigandCrossbowman"
  | "StagWolf"
  | "BossVoss"
  // Apparition: idle/cast, 36 frames each, cut with the TEK (work/apparition/build.py).
  | "apparition"
  // Jacaré: idle/walk/ATT/Special/hit/death, 36 frames each, cut with the TEK (work/jacare/build.py).
  | "jacare"
  // Carnivorous Plant: idle/atk/cast/hit/death, 36 frames each, cut with the TEK from the
  // user's videos (see work/carnivorous-plant/build.py).
  | "carnivorous-plant-001"
  // Sapling: idle/move/atk/cast/hit/death, 36 frames each, cut with the TEK from the user's
  // videos (see work/sapling/build.py).
  | "sapling-001"
  | import("./encounter-npcs").EncounterNpcId
  // Generic-enemy "alter" sprites, split off so a plain Archer/Mage/Healer enemy (and their
  // own promotions) never renders as literally the same SpriteId as Neera/Voss/Salazar the
  // MCs — see HERO_SPRITE_BY_NAME/CLASSES in engine.ts/data.ts. Each starts as a straight
  // file copy of the hero's own art (archerRecruit/mageRecruit/healerRecruit folders under
  // public/game/sprites/), a placeholder slot ready to be replaced with real dedicated art
  // later without any further code changes.
  | "archerRecruit"
  | "mageRecruit"
  | "healerRecruit"
  // Weak, killable flavor civilians (see ClassId's own note) — 4-frame idle each, the default
  // frame count (not in HERO_IDLE), one character per source sheet.
  | "beberrao"
  | "breadLady"
  | "brue"
  | "crazyLady"
  | "mudinho"
  | "oldHealer"
  | "peasant1"
  | "shadyPatron"
  | "soupLady"
  | "villagerF1"
  | "woodsman" | "travelingMerchant";
export type HealId = "cureMinor" | "cureWounds" | "cureLight";
export type SpellKind =
  | "fireball"
  | "iceStorm"
  | "frost"
  | "bless"
  | HealId
  | "longShot"
  | "bloodyShot"
  /** Warrior Tier 1 (level 3): draws enemies' enmity onto the warrior — see PROVOKE, enmity.ts. */
  | "provoke"
  | "piercing"
  | "lightning"
  | "lightningTier3"
  | "magicMissile"
  | "magicMissileV2"
  | "causticVenom"
  | "divineBolt"
  // Veneno Menor: the mobs' radius-2 venom (MINOR_VENOM in data.ts).
  | "minorVenom"
  | "doubleStrike"
  | "cleave"
  | "cureDisease"
  | "piercingThrust"
  | "sweep"
  | "trip"
  | "summonFamiliar"
  | "phantasmalForce"
  | "fantomForce"
  | "summonFamiliar2"
  | "summonFamiliar3"
  | "summonFamiliar4"
  | "summonZombieDog"
  | "lifeDrain"
  | "webOfDreams"
  | "warp"
  | "multiShot"
  | "secondWind"
  | "auraOfProtection"
  | "divineWrath"
  | "shoulderSmash"
  | "intimidatingPresence"
  | "stampede"
  | "shock"
  | "bullRush"
  | "executionerStrike"
  | "shieldBash"
  | "poisonBreath"
  // Carnivorous Plant's AI-only tendril swipe: a weapon hit on every foe touching the front
  // and flanks of her Type 7 body, played on her ATT sheet.
  | "tendrilSwipe"
  | "burningHands"
  | "turnUndead"
  | "createFoodAndWater";
export type ScreenId = "boot" | "title" | "saveSlots" | "campaign" | "mapChoice" | "vauIntro" | "wispForestIntro" | "innArrivalIntro" | "worldMap" | "overworldMap" | "briefing" | "cutscene" | "epilogue" | "battle" | "victory" | "defeat" | "inn" | "testMenu" | "mapEditor" | "devControls";
export type Phase = "player" | "enemy";
export type InputMode = "idle" | "selected" | "awaitAction" | "awaitAttack" | "awaitOffHand" | "awaitSpell" | "awaitPotion" | "locked";

export interface Point {
  x: number;
  y: number;
}

export interface TerrainDef {
  id: TerrainId;
  name: string;
  moveCost: number;
  def: number;
  atk: number;
  passable: boolean;
  hazardDice?: number;
  hazardFaces?: number;
  height?: number;
  blocksShot?: boolean;
  cover?: number;
}

export interface ClassDef {
  creatureType?: "undead";
  id: ClassId;
  name: string;
  role: string;
  hp: number;
  atk: number;
  mag: number;
  def: number;
  dex: number;
  /** Elemental resistance percentages, independent of general DEX. Missing values are zero. */
  resistances?: Resistances;
  mov: number;
  minRange: number;
  maxRange: number;
  sprite: SpriteId;
  size: number;
  /** Footprint block for big creatures (size >= 4), in hexes. Defaults to 2 wide x 4 tall. */
  footprintW?: number;
  footprintH?: number;
  /**
   * Explicit footprint shape for big creatures (size >= 4), as {dx, dy} offsets from the
   * unit's own tile — dy: 0 is the front row (feet, closest to the player), negative dy is
   * further back. Overrides footprintW/footprintH when set, for shapes that aren't a plain
   * rectangle.
   */
  footprintOffsets?: { dx: number; dy: number }[];
  /** Archetype speed rank (1 fastest .. 10 slowest), converted to an initiative bonus. */
  init?: number;
  /** Marks this class as a named boss: boss objectives recognize it and the battle renders its chief marker. */
  boss?: true;
  /** Marks a class as a summon rather than a member of the cast: conjured into a battle by
   * a spell (the Familiar by the Conjurer's tier 1), gone when it ends, and outside the
   * party's defeat check — losing every summon on the board never loses the mission. It is
   * a property of the class, not of how the unit reached the board, so a summon dropped
   * straight onto a map in the editor behaves the same as one conjured in play. Every
   * summon class that gets added should carry this. */
  summon?: true;
  /** Undead kind. Every class with one is undead, the target of Turn Undead (a future healer
   * spell). */
  undead?: "ghost";
}

/** One reply choice inside a branching DialogLine. Picking it jumps to `next`, or ends the
 * tree if `next` is absent/null — same convention as DialogLine.next below. */
export interface DialogReply {
  /** Directed companion chapter resolved by this response, for one-time affinity changes. */
  companionScene?: string;
  affinity?: { from: string; to: string; delta: -3 | 0 | 3 };
  translations?: Partial<Record<"pt" | "en", string>>;
  text: string;
  next?: string | null;
  /** Opens one of the Inn's menus after the popup closes — see DialogAction. */
  action?: DialogAction;
}

/** A handoff a dialog reply can trigger: an Inn or merchant shop, or a story recruitment. */
export type DialogAction = "tavern" | "smith" | "healer" | "merchant" | "merchantGear" | "recruitAldric" | "acceptSuspectHostageQuest";

/** One screen of the dialog popup: a speaker, an optional portrait, and the line itself.
 * Either it links straight to the next line (`next`, plain "OK" to continue) or it branches
 * (`replies`, one button per reply) — never both; `replies` wins if somehow both are set. */
export interface DialogLine {
  translations?: Partial<Record<"pt" | "en", string>>;
  speakerTranslations?: Partial<Record<"pt" | "en", string>>;
  id: string;
  speaker: string;
  /** A SpriteId to resolve a portrait image from (see resolveDialogPortrait) — omitted shows
   * no image, just the speaker name and text. */
  portrait?: SpriteId;
  text: string;
  replies?: DialogReply[];
  next?: string | null;
}

/** A whole conversation: attached to a Mission (intro/outro) or to a neutral Spawn (an NPC's
 * own conversation, replayed from `startId` every time that unit is clicked). */
export interface DialogTree {
  id: string;
  startId: string;
  lines: DialogLine[];
}

export interface Spawn {
  /** This authored enemy stays at its spawn, e.g. a rooted plant guarding a clearing. */
  holdsPosition?: true;
  name: string;
  classId: ClassId;
  x: number;
  y: number;
  /** Enemy-only: skips the normal 1%-per-kill loot roll and always drops something (from
   * the same random weapon-or-gear pool a chest rolls from) when this unit dies — for named
   * unique bosses the mission wants to reliably reward. */
  guaranteedDrop?: boolean;
  /** Turns this spawn into a talkable NPC: clicking it opens the tree instead of the normal
   * attack/inspect flow, and it can never be attacked (see attackableByPlayer in engine.ts).
   * Meant for neutral-side spawns — a neutral with no dialog stays the existing wild-beast
   * behavior, unchanged. */
  dialog?: DialogTree;
  /** Optional door that must be opened before this NPC can be talked to. */
  dialogRequiresOpenDoor?: { x: number; y: number };
  /** Editor/test escape hatch from HERO_SPRITE_BY_NAME (engine.ts): one of the six named
   * heroes normally always renders with their own pinned sprite no matter what classId they
   * carry, so a promoted hero never visually turns into the stock enemy art their new class
   * shares with real enemies. Setting this on a player-side spawn bypasses that pin for this
   * spawn only, rendering with classId's own class sprite instead — lets the mission editor
   * drop any enemy/creature classId into a hero-named slot and actually see it, for testing.
   * No effect on any other spawn (only the six HERO_SPRITE_BY_NAME names are ever pinned). */
  useClassSprite?: boolean;
}

/** "escape" is Transversal Dungeons: winning has nothing to do with combat — the field never
 * has to be cleared. It becomes available when a living player unit stands on a waypoint
 * (see DecorationDef.exitKind), exactly like rout/boss make it available once the field is clear. */
export type WinCondition = "rout" | "boss" | "escape";

/** A multi-hex terrain prop (mountain, ruin, bridge, ...): rendered as a single image
 * spanning several hexes rather than clipped to one, drawn on top of the regular tile
 * grid so it doesn't need to fill each hex's exact shape. Every hex in its footprint is
 * impassable and blocks line of sight, independent of whatever terrain tile is under it. */
export interface DecorationDef {
  /** Architecture meshes live in the editor's separate 3D Walls palette. */
  model3d?: "wall" | "doorway" | "door" | "secretDoor" | "tree" | "prop";
  /** Paired open/closed architecture variant, used to retain a door's appearance on opening. */
  doorStyle?: "oak" | "reinforced" | "secretStone" | "iron" | "steel" | "castle" | "stoneOak" | "dungeonOak";
  castleStyle?: "battlement" | "ruined" | "gate" | "tower";
  thickWall?: boolean;
  architectureSpan?: number;
  dungeonReference?: boolean;
  templeStyle?: "plain" | "niche" | "relief";
  propModel?: "small-house" | "rocky-outcrop" | "grey-outcrop" | "ember-amanita-muscaria-v01" | "tavern-barrel" | "tavern-chair" | "tavern-candlestick" | "tavern-mug" | "tavern-table" | "tavern-table-small-v01" | "tavern-table-messhall-v01" | "tavern-chair-v04" | "vau-gothic-cottage-v01" | "vau-well-v01" | "wisp-revisited-broken-stump" | "wisp-revisited-rotted-hollow-log" | "wisp-revisited-rotted-hollow-log-v02" | "wisp-revisited-roots-and-fungi" | "watchtower-signal-brazier-v01" | "watchtower-arms-rack-v01" | "watchtower-command-table-v01" | "watchtower-signal-brazier-v02" | "watchtower-arms-rack-v02" | "watchtower-command-table-v02";
  treeModel?: "broadleaf" | "snowy-pine" | "dead-oak" | "dead-snag" | "twisted-stump" | "watchtower-leafy-oak-v01" | "watchtower-leafy-willow-v01" | "watchtower-leafy-birch-v01" | "wisp-revisited-weeping-willow-v02" | "wisp-revisited-giant-dead-oak-v02" | "wisp-revisited-hollow-snag-v02" | "wisp-revisited-uprooted-leaning-v02" | "wisp-revisited-lightning-split-v02" | "wisp-revisited-spiral-twisted-v02" | "wisp-revisited-tall-burnt-pine-v02" | "wisp-revisited-broken-crown-v02" | "wisp-revisited-root-arch-v02" | "wisp-crossing-hollow-sentinel-v02" | "wisp-crossing-lightning-split-v02" | "wisp-crossing-weeping-willow-v02" | "wisp-crossing-ancient-dead-oak-v02";
  rockStyle?: "layered" | "arch" | "broken";
  /** Repeating albedo material for architecture; lighting remains real time. */
  wallTexture?: string;
  /** Extra masonry depth multiplier; defaults to the standard thin wall profile. */
  wallThicknessScale?: number;
  id: string;
  name: string;
  /** Hex offsets from the anchor cell (dx/dy in board coordinates, same convention as
   * Unit.footprintOffsets — {dx:1,dy:0} is always the same-row neighbor). */
  footprint: { dx: number; dy: number }[];
  /** Optional movement collision area when it must differ from the art's sizing/anchor area. */
  blockingFootprint?: { dx: number; dy: number }[];
  /** The terrain this prop means, if it means one.
   *
   * A ridge you can climb sits on "hill". Naming terrain here lets the editor lay
   * that tile with the prop. Solid decorations such as rocks instead block through
   * their own footprints without changing the ground underneath. */
  tile?: TerrainId;
  /** Draw this prop after character sprites so near-side scenery can naturally occlude
   * them. This is visual-only: it does not change movement, line of sight, or terrain. */
  foreground?: boolean;
  /** Explicit sprite-depth layer. “behind” stays above the ground and selection overlay,
   * but below characters; “front” occludes everything behind the prop. */
  unitLayer?: "behind" | "front";
  /** Relative painter's order among decorations in the same visual layer. Higher values draw
   * later, letting a near-side prop deliberately overlap other scenery. */
  decorRenderOrder?: number;
  /** Compatible repeating modules may share exactly one joining hex in the editor. */
  repeatGroup?: string;
  /** Optional vertical presentation multiplier for tall isometric scenery. */
  heightScale?: number;
  /** Mirror alternating placements horizontally to add visual variety to repeated props. */
  mirrorAlternate?: boolean;
  /** Visual multiplier applied to both dimensions without changing the footprint or rules. */
  artScale?: number;
  /** Preserve the photographic image aspect ratio at its footprint width. Opt-in for new assets. */
  artAspect?: number;
  /** Optional reflectance for new decor under calibrated real scene lights; existing art defaults to 1. */
  lightReflectance?: number;
  /** Normalized horizontal artwork offset from its measured base anchor. */
  artOffsetX?: number;
  /** Draws after (in front of) the ground-mist atmosphere sheets (Mist 2/3/4 — see
   * ThreeAtmosphere.ts), which render with depthTest disabled and would otherwise paint over
   * this prop and wash it out wherever the mist band crosses it. Any decoration with a
   * LIGHT_DEFS entry (lighting.ts) gets this automatically — a light source must never read
   * as extinguished/translucent under mist — this flag is for scenery that needs the same
   * treatment without emitting light, e.g. tall props that commonly sit in the map's edge
   * band where Mist 4's border fog lives. */
  aboveGroundMist?: boolean;
  /** Draws above the tactical movement/target overlay so the prop never looks washed out
   * when its hex is highlighted. This affects rendering only, not selection or movement. */
  aboveTacticalOverlays?: boolean;
  /** Casts no shadow — true silhouette caster and hidden shadow-blocking volume both skipped
   * (see ThreeBattleRenderer's decor sync). For thin/tall scenery whose cast shadow reads as
   * an unwanted dark stripe across the board rather than grounding the prop. */
  noShadow?: boolean;
  /** Marks this decoration as a usable waypoint (see BattleEngine.evaluateEnd). "escape" and
   * "dungeon" are the two blue exits; confirming an escape marker rolls the existing 60% flee
   * chance, while a dungeon exit ends the mission directly. "connector" is the red floor link:
   * confirming it sends the party to DecorationPlacement.targetMapId rather than the campaign map. */
  exitKind?: "escape" | "dungeon" | "connector";
}

/** A decoration placed on a mission's map, anchored at (x,y). */
export interface DecorationPlacement {
  id: string;
  x: number;
  y: number;
  /** How far the prop is turned, in sixths of a circle (0-5). A hexagon maps onto itself
   * every 60 degrees, so those are the only turns whose footprint still lands on real
   * hexes. Architecture instead uses quarter-turns (0-3) for rectangular walls and doors.
   * Optional: a map saved before props could turn has no such key, read as 0. */
  rot?: number;
  /** Mirror this placed prop horizontally without rotating its occupied hexes. */
  mirrorX?: boolean;
  /** Explicit wall axis; absent preserves older maps' automatic corner joins. */
  wallOrientation?: "horizontal" | "vertical";
  /**
   * Per-placement rule overrides, set by the two switches in the map editor.
   *
   * Both are additive: absent or false adds nothing and the hex keeps whatever the
   * prop's `DecorationDef.tile` (or the painted terrain) already said, so no map saved
   * before these existed changes behaviour. They cannot take a property away — a
   * barricade stays impassable with `blocksPath` off, because its definition is what
   * makes it solid.
   *
   * They work by choosing the terrain stamped under the prop when the board loads, not
   * by adding a second place where rules live: everything in this engine reads movement,
   * cover and line of sight off `tiles` alone (see footprintCost, clearShot), and the
   * comment on DecorationDef.tile explains why that is worth keeping.
   */
  blocksPath?: boolean;
  yieldsHighGround?: boolean;
  /** Floor-connector placements only (DecorationDef.exitKind === "connector"): the specific
   * mission this hex leads to once the party confirms. That mission is reached only through
   * this connector — it is never listed in a WorldLocation.missionIds, so it never appears as
   * its own card in the campaign map (see WorldLocation.submaps). */
  targetMapId?: string;
  /** Floor-connector placements only: flips the result-screen wording and direction from
   * "Avançar" (deeper into the dungeon) to "Voltar" (back to the floor above) — set on whichever
   * connector was placed on the deeper floor, pointing back at the one above it. Purely a label/
   * intent flag the author sets; nothing computes it from floor numbers automatically. */
  returnConnector?: true;
  /** Physical stair direction, independent of whether this is the return route. */
  connectorDirection?: "up" | "down";
  /** Stair decoration maintained alongside its Watchtower waypoint. */
  waypointStairs?: true;
}

/** A permanent WebGL elemental FX (src/game/gfx) anchored at (x,y) on a mission's map —
 * a lava pit's fire, a frozen pond's ice glint, a rune circle's holy glow, and so on. Placed
 * in the map editor's "FX" mode; spawned once when the battle loads and left running for the
 * whole fight (see BattleEngine.elementalFxPlacements / BattleCanvas). */
export interface ElementalFxPlacement {
  id: string;
  kind: "fire" | "ice" | "water" | "lightning" | "acid" | "holy" | "darkness" | "shore" | "shore2" | "water2" | "water3" | "water4" | "water5";
  x: number;
  y: number;
  /** Footprint as a multiple of one hex's tile size. Omitted = the renderer's default. */
  radiusTiles?: number;
  /** Radians; mainly useful for lightning's tall/narrow bolt shape. */
  rotation?: number;
  /** New render family. Missing on legacy map data means the original regular effect. */
  family?: "regular" | "procedural_pixel";
  element?: "fire" | "frost" | "lightning" | "poison" | "arcane" | "holy" | "shadow" | "ember";
  preset?: string;
  parameters?: Record<string, number | boolean>;
}

/** Authored payment for clearing a battlefield and helping named neutral NPCs. */
export interface VictoryReward {
  ember: number;
  rations: number;
  requiredNpcNames?: string[];
}

export interface Mission {
  /** Preserve authored starting positions for scripted or dangerous openings. */
  lockPartyFormation?: boolean;
  id: string;
  index: number;
  title: string;
  place: string;
  briefing: string;
  objective: string;
  win: WinCondition;
  cols: number;
  rows: number;
  layout: string[];
  playerSpawns: Spawn[];
  enemySpawns: Spawn[];
  /** Wild things that start on no side. Optional: a mission without any is every mission
   * shipped before neutrals existed, and reads as an empty list. */
  neutralSpawns?: Spawn[];
  /** A specific track from public/game/MUSIC (by file name) to play through this mission,
   * instead of the theme its id would otherwise fall into. Absent means the usual theme. */
  music?: string;
  hub?: boolean;
  /** Free-roam map (the walkable Inn): no turns, no win/defeat check, the party leader walks
   * anywhere reachable in one click and talks to NPCs. */
  explore?: boolean;
  /** Explicit indoor square floor/grid and straight border mode; absent preserves legacy layouts. */
  squareTiles?: boolean;
  /** Whether stampTactics dresses this map — the pass that scatters barricades, hills and
   * the high-terrain variants over it after the layout is doubled. On unless a map says
   * otherwise, so nothing already shipped changes; turn it off on a map placed by hand,
   * where the scatter would paint over deliberate work. */
  autoTactics?: boolean;
  /** Whether this mission is played under fog of war: the party sees a radius around
   * itself, terrain it has walked past stays remembered but dim, and enemies it has
   * no sight of are neither drawn nor targetable. Off unless a map asks for it, so
   * every mission shipped before fog existed plays exactly as it always did — the
   * flag is for dungeon levels built around not seeing what is coming. */
  fog?: boolean;
  /** "indoor" softens the sun to a flat ambient wash (no strong directional shadows) — for
   * missions set inside buildings/dungeons where a raking outdoor sun makes no sense. Missions
   * without this set play "outdoor" (today's default look), so nothing shipped changes. */
  environment?: "outdoor" | "indoor";
  /** Time of day, Three renderer only (see ThreeBattleRenderer's TIME_OF_DAY_LIGHT). Undefined
   * uses the softer "day" preset. "noon" retains the former stronger daytime preset. At night
   * the Moon replaces the Sun and sunIntensity drives the Moon instead. */
  timeOfDay?: MapTimeOfDay;
  /** Image ID from the map background catalog; empty or absent means black void. */
  backgroundImage?: string;
  /** DirectionalLight ("sun") intensity override, for the Three renderer only. Undefined uses
   * the renderer's own default. Author-tunable per mission because "how strong should the light
   * and its shadows read" turned out to need a per-map answer, not one global constant. */
  sunIntensity?: number;
  /** HemisphereLight (sky/ground fill) intensity override, same reasoning as sunIntensity —
   * undefined uses the renderer's own default. */
  ambientIntensity?: number;
  /** Ground-mist peak opacity, Three renderer only (see ThreeAtmosphere.ts) — undefined or 0
   * means no mist at all, which is every mission shipped before this existed. Deliberately not
   * exposed above a modest ceiling in the editor (see GameApp.tsx's slider max) — a real,
   * author-controlled value now, not a hardcoded per-mission-id table. */
  mistIntensity?: number;
  /** Which mist implementation this mission uses:
   *  - "mist2" (default): real Three.js world-space planes using a smooth pre-baked noise
   *    texture, spread across the whole battlefield (see ThreeAtmosphere.ts's GroundMist).
   *  - "mist3": the earlier real Three.js world-space InstancedMesh "puff" implementation
   *    (large soft drifting circles) — kept available as its own selectable option, not
   *    deleted, per direct instruction (see ThreeAtmosphere.ts's GroundMistPuffs).
   *  - "vignette": a screen-space haze concentrated at the four corners only, center always
   *    clear (see BattleCanvas.tsx's corner-vignette CSS div, extended to read
   *    mistIntensity/mistColor when this is set).
   *  - "vignette2": a deeper, layered version of the screen-space vignette, with independently
   *    drifting fog banks and a broad, feathered clear area at the center.
   *  - "vignette3": low, directional ground fog crossing the battlefield in pale rolling bands;
   *    deliberately no dark corner vignette or texture reuse.
   *  - "vignette4": a supplied monochrome edge-fog plate, animated as one screen layer.
   *  - "fog1": like "mist2", but masked by the party's own fog-of-war — it only ever shows
   *    over ground they haven't revealed yet (plus the exterior backdrop past the board's
   *    edge); real explored/visible terrain always stays completely clear underneath it (see
   *    ThreeAtmosphere.ts's RevealFog).
   *  - "none": no world-space mist and no screen-space vignette at all, whatever mistIntensity
   *    is set to — an explicit "off" the author can pick, distinct from just never having set
   *    mistIntensity (which still defaults mist2 on at 0.2, see below).
   * Undefined defaults to "mist2", so nothing shipped changes by default. */
  mistType?: "mist2" | "mist3" | "mist4" | "vignette" | "vignette2" | "vignette3" | "vignette4" | "fog1" | "fog5" | "none";
  /** Bloom strength for the Three renderer's post-processing pass (UnrealBloomPass) — real bright-
   * surface glow (sun-lit highlights, additive wisp particles), not a fake overlay. Undefined
   * uses the renderer's own conservative default. Author-tunable per mission like every other
   * atmosphere control tonight, full range including deliberately extreme at max. */
  bloomIntensity?: number;
  /** Speed multiplier for the mist's drift — 1.0 is the renderer's own default pace, same
   * reasoning as wispSpeed: never needs a code change to retune again. */
  mistSpeed?: number;
  /** 0-1 dial for the rising-ember "wisp" particle count, Three renderer only (see
   * ThreeAtmosphere.ts) — 0 or undefined means none, every mission shipped before this existed.
   * A dial, not a raw instance count, so the editor slider stays meaningful regardless of how
   * the renderer scales it internally. */
  wispIntensity?: number;
  /** Speed multiplier for the wisp rise/drift/fade cycle — 1.0 is the renderer's own default
   * pace, lower is slower, higher is faster. Author-controlled so "how fast should this feel"
   * never needs a code change again. */
  wispSpeed?: number;
  /** Fixed wisp color (0xRRGGBB) — no automatic mixing toward the sun/ambient light color, on
   * direct instruction: the author picks the exact color, full stop. Undefined uses the
   * renderer's own default (a warm ember orange). */
  wispColor?: number;
  /** Which art variant to use per tile, row-major, same indexing as layout flattened.
   * Missing/undefined index or omitted array entirely means variant 0 (the default) —
   * existing missions never set this and keep rendering exactly as before. */
  tileVariants?: number[];
  /** Authored terrain levels, row-major. Positive levels grant existing high-ground advantages in both views. Missing cells retain the terrain type's height. */
  terrainElevations?: number[];
  /** Independent 3D water surface levels. Null cells have no authored water. */
  waterLevels?: (number | null)[];
  /** Free-position water strokes in tile-normalized world coordinates (Y down). */
  waterVersion?: "v1" | "v2" | "v3" | "v4";
  waterPatches?: { x: number; y: number; level: number; size: number; shape: "round" | "square" }[];
  /** Water footprint size and shape per cell; absent means the original full round brush. */
  waterFootprints?: ({ size: number; shape: "round" | "square" } | null)[];
  /** Default ground chosen in the map editor. Terrain-changing decorations restore this tile when removed. */
  baseTile?: TerrainId;
  baseVariant?: number;
  /** How far each tile's art is turned, in sixths of a circle (0-5), row-major like
   * tileVariants. A hex maps onto itself every 60 degrees, so its art can be spun without
   * the shape or its neighbours moving — which is what makes a coastline, a road or a wall
   * meet the tile next to it instead of running the wrong way. Optional and absent by
   * default: a mission without it draws every tile the way it was painted. */
  tileRots?: number[];
  /** Multi-hex terrain props (mountains, ruins, bridges, ...) placed on this map.
   * Omitted/empty on every existing mission — purely additive. */
  decorations?: DecorationPlacement[];
  /** Permanent elemental GPU FX (lava fire, icy glints, ...) placed on this map. Omitted on
   * every existing mission — purely additive. */
  elementalFx?: ElementalFxPlacement[];
  /** Coordinates of chests on this map that should roll noticeably better loot when opened
   * — same pool and range as a normal chest (see useLockpick), just tipped toward the
   * better end: more Ember, better gear odds. For a chest worth gating behind a locked
   * door/sub-area rather than just leaving out in the open. Omitted on every existing
   * mission — purely additive. */
  betterChests?: { x: number; y: number }[];
  victoryReward?: VictoryReward;
  /** Shown once, before the player can act, right as the battle screen opens. Omitted on
   * every existing mission — purely additive. */
  introDialog?: DialogTree;
  /** Turns introDialog off without deleting it — absent behaves as on whenever introDialog
   * is set, so only an explicit `false` ever suppresses it. */
  introDialogEnabled?: boolean;
  /** Shown once victory is confirmed (the player clicked "Encerrar missão"), before leaving
   * to the results screen. Never shown on defeat. Omitted on every existing mission. */
  outroDialog?: DialogTree;
  /** Same on/off convention as introDialogEnabled. */
  outroDialogEnabled?: boolean;
}

/** A travel spot on the campaign world map. Most locations cover a single mission; a
 * location can also bundle a short arc of missions (e.g. an approach, an encounter, and
 * its aftermath at the same landmark) picked from one sub-menu instead of getting a
 * marker each — missionIds just lists them in story order. */
export interface WorldLocation {
  id: string;
  name: string;
  /** Location and first mission ignore the campaign's previous-location prerequisite. */
  openAccess?: boolean;
  /** Travel encounters can also roll on this named location's hex. */
  encountersAllowed?: boolean;
  /** A named settlement that Warp may target after the party has visited it. */
  warpCity?: boolean;
  /** Position on the world map image, in percent (0-100) of its width/height. */
  x: number;
  y: number;
  missionIds: string[];
  /** RPG map only: absolute gameClock day after which this location is flagged expired.
   * Opt-in — omitted on every location today, so nothing changes unless one is set. */
  deadlineDay?: number;
  /** Extra maps chained to this location as Transversal Dungeon floors, reached only by a
   * floor-connector decoration in-battle (see DecorationPlacement.targetMapId) — never listed
   * in missionIds, so a submap never gets its own card in the Locais/campaign menu. `floor` is
   * author bookkeeping only (which submap is "deeper" than which, for picking connector
   * targets in the editor); nothing computes it from mission order automatically. Omitted on
   * every location today, so nothing changes unless one is set. */
  submaps?: { missionId: string; floor: number }[];
}

/** Attributes that can receive the three permanent points earned at every level-up. */
export type StatPointAttribute = "hp" | "atk" | "mag" | "def" | "dex";
export type StatPointAllocation = Partial<Record<StatPointAttribute, number>>;

export interface Unit {
  id: string;
  /** Fog of war: how many hexes this unit sees (party units only). Unset = SIGHT_RADIUS. */
  visionRange?: number;
  name: string;
  classId: ClassId;
  className: string;
  role: string;
  side: Side;
  sprite: SpriteId;
  /** Carries Spawn.useClassSprite through so a resumed battle re-derives the same sprite
   * choice on load (see HERO_SPRITE_BY_NAME/resolveHeroSprite in engine.ts) instead of
   * silently reverting to the name-pinned one. Irrelevant for any unit whose name isn't one
   * of the six pinned hero names. */
  useClassSprite?: boolean;
  x: number;
  y: number;
  hp: number;
  escaped?: boolean;
  maxHp: number;
  atk: number;
  mag: number;
  def: number;
  dex: number;
  /** Elemental resistance percentages, independent of general DEX. Missing values are zero. */
  resistances?: Resistances;
  weaponSkills?: WeaponSkillValues;
  healingSkill?: number;
  /** Rolled once at battle start: 1d20 + the class initiative modifier. */
  initiative: number;
  initiativeRoll: number;
  /** Permanent player-chosen bonuses from level-up attribute points. Kept on the live unit
   * so swapping equipment or resuming a battle never erases them. */
  statPointAllocation: StatPointAllocation;
  mov: number;
  minRange: number;
  maxRange: number;
  moved: boolean;
  acted: boolean;
  facing: 1 | -1;
  /** The unit's last real heading on the board (hexCenter delta toward its last attack/counter/cast
   * target, or along its last step). `facing` is re-derived from it for the current camera angle every
   * frame, so a unit keeps pointing where it last fought, whatever the camera does. */
  faceDx?: number;
  faceDy?: number;
  walkPose: "front" | "back" | "side";
  /** Flips at the start of every one of this unit's own turns (see beginUnitTurn). Consulted
   * by sprites with a second idle loop (currently just Malrec, see idles2 in GameArt) to
   * alternate between their two standing animations, and separately by sprites with a second
   * attack cut (currently just Familiar 3, see attacks2 in GameArt) to alternate their swing —
   * everyone else's render code ignores it. */
  idleAlt: boolean;
  alive: boolean;
  drawX: number;
  drawY: number;
  flash: number;
  /** 1 right when a unit levels up, decaying to 0 over a couple seconds — drives the golden
   * glow drawn around the sprite in render() (see levelUpUnit/spawnLevelUp). */
  levelGlow: number;
  /** Same idea as levelGlow but for receiving a beneficial effect — a heal spell landing or
   * a potion being drunk — drawn as a halo (see emitHolyFx / emitPotionZeroFx). Decays
   * independently of levelGlow so the two can overlap. */
  healGlow: number;
  /** Palette the healGlow halo uses: holy gold (minor/medium), disease teal, potion amber,
   * or Potionzero (the original warm-white glow, kept for future skills). */
  healGlowKind: "holyMinor" | "healingHands" | "holyMedium" | "disease" | "potion" | "potionZero" | "food" | "bless";
  fade: number;
  /** Engine time (BattleEngine.time) at which this unit died — drives its death sheet (see
   * GameArt.deaths) and holds off the fade-out until that sheet has played. */
  diedAt?: number;
  /** Rolled at death: plays the alternate death sheet (GameArt.deaths2) instead, for the
   * sprites that have one. */
  deathAlt?: boolean;
  /** Engine time at which this unit last took damage — drives its hit-reaction sheet (see
   * GameArt.hits). */
  hitAt?: number;
  bob: number;
  level: number;
  /** XP toward the next level (0..expToLevel(level)-1). Player-only; always 0 for enemies. */
  xp: number;
  bag: Bag;
  spells: Spells;
  /** Equipped WeaponDef id, or null (player units start with the group's weakest weapon; enemies have none). */
  weaponId: string | null;
  /** Tabletop-style enhancement on the equipped weapon, 0..5. */
  weaponEnh: number;
  size: number;
  footprintW?: number;
  footprintH?: number;
  footprintOffsets?: { dx: number; dy: number }[];
  shock: { dice: number; faces: number; bonus: number; mag?: number } | null;
  /** Enemy-only Choque charges (weaker Relâmpago). Not a player tier — see shockChargesFor.
   * Distinct from `shock` above, which is Relâmpago's echo DoT. */
  shockCharges: number;
  /** A summoned familiar's own spell charges this battle — which spell (if any) is FAMILIAR_SPELL[classId];
   * set at summon time from familiarSpellCharges(conjurer's level), spent by that spell's own
   * castX, never refilled mid-battle. Undefined/0 for every other unit. */
  spellCharges?: number;
  /** Familiar Maior's (tier 2) own Dreno de Vida charges this battle — independent of
   * spellCharges above, since tier 2 is the one familiar tier with two own spells (Magic
   * Missile via spellCharges, Life Drain via this) — set at summon time from
   * familiarLifeDrainCharges(conjurer's level), spent by castLifeDrain, never refilled
   * mid-battle. Undefined/0 for every other unit. */
  lifeDrainCharges?: number;
  /** Enemy-only legacy FantomForce uses, replenished at battle start. */
  fantomForceCharges?: number;
  /** Cultist V2 owns a separate Frost pool; player mages use Tier 2 charges. */
  frostCharges?: number;
  /** Current Bless accuracy bonus, stored as a fraction (0.01 = one percentage point). */
  blessedHitBonusPct?: number;
  /** Remaining rounds for Bless. */
  blessedRoundsLeft?: number;
  /** A summoned familiar's own summoning conjurer, by id — set once at summon time. Consulted
   * by Familiar Maior's Dreno de Vida to know who to heal (see the lifeDrain branch in
   * BattleEngine.stepSpell); undefined for every non-familiar unit. */
  summonerId?: string;
  diseased: boolean;
  diseaseBase: { atk: number; mag: number; def: number; dex: number; mov: number } | null;
  /** Poison: its tier's dice (POISON_TIERS in poison.ts — Lesser 1D4, Poison 1D10, Greater
   * 1D10, Deadly 2D8, Lethal 3D6) at the start of each of this unit's own turns
   * (see startOfTurnEffects) until cured — same cure trigger as diseased (Cure Disease
   * spell or the disease potion), but no stat penalty of its own. */
  poisoned: boolean;
  poisonTier?: PoisonTier;
  /** MAG of whoever applied the current poison — half of it pierces Poison Resistance. */
  poisonMag?: number;
  /** Resistência a Veneno skill (0–100). Heroes carry their trained value; everyone else 0. */
  poisonResist?: number;
  /** Rasteira wound: suffers 1D8 whenever acting; movement only once each own turn. */
  bleeding: boolean;
  /** Timed bleed rounds left for Bloody Shot; undefined means an indefinite Trip bleed. */
  bleedRoundsLeft?: number;
  /** Round when the timed bleed was last applied or decremented; used to expire after full rounds. */
  bleedRoundMarker?: number;
  bleedMovedThisTurn: boolean;
  /** Shield Bash victim: loses their entire next turn, then clears automatically. */
  /** Remaining own turns spent retreating without attacking. Optional for older saves. */
  fearTurns?: number;
  fearSourceId?: string;
  stunned: boolean;
  /** How many of this unit's own upcoming turns `stunned` still eats — Shield Bash sets this
   * to 1, Trip (Lancer tier 3) to 2. Decremented each time it costs a turn; `stunned` only
   * clears once this reaches 0. */
  stunTurns: number;
  /** Trip (Lancer tier 3) victim: a permanent (this battle) −10% to ATK/MAG/DEF/DEX/MOV,
   * applied once and never restored — unlike `diseased`, nothing cures it. */
  crippled: boolean;
  /** 0..0.9 — carried in from the party's overworld hunger streak at spawn (see Roster
   * and hungerPenaltyFor). Unlike `crippled`, this is re-applied every time reapplyGear
   * runs rather than mutated once, since — unlike a battle debuff — it reflects a save
   * value that can be current going into the next mission too, not something this
   * battle inflicts on itself. */
  hungerPenaltyPct: number;
  fullness: number;
  /** Equipped off-hand EquipmentDef id (kind "weapon" or "shield"), or null. */
  offHandId: string | null;
  /** Everything this unit is wearing, by slot. Carried on the unit (not just in the save)
   * so gear can change mid-battle and the stats that depend on it can be recomputed
   * without rebuilding the unit. See gearStatBonus. */
  gear: Partial<Record<EquipSlot, string>>;
  /** Summon Familiar (Conjurer tier 1): a player-side unit that doesn't count toward "any
   * hero still alive" for the defeat check or the playerAlive HUD figure — the party can't
   * survive a wipe on a pet alone. Everything else about it (selecting, moving, acting,
   * being targeted) works exactly like any other player unit. */
  summoned: boolean;
  /** Web of Dreams (Conjurer tier 2) victim: skips its own upcoming turns just like
   * `stunned`, but decrements on a separate counter (`sleepTurns`, set by a 1D4 roll) and
   * clears early — mid-round, not just at its own next turn — the instant it takes a hit,
   * which also applies that hit's `sleepBonusDamage` multiplier. */
  asleep: boolean;
  sleepTurns: number;
  /** Mirrors Spawn.guaranteedDrop — read once in markDead, never touched afterward. */
  guaranteedDrop: boolean;
  /** Mirrors Spawn.dialog — a unit carrying one is a talkable NPC: never an attack target
   * (see attackableByPlayer), and clicking it opens this tree instead of inspect. */
  dialog: DialogTree | null;
  /** Total path cost already spent moving this unit's own turn — reset once in
   * beginUnitTurn. Free repositioning (see effectiveUnitForReach) recomputes reach fresh
   * from wherever the unit currently stands after every move, which without this would
   * hand back a full, fresh `mov` budget each time and let a unit walk the length of the
   * map in hex-by-hex hops within a single turn; subtracting what's already been spent
   * caps the turn's real total distance at `mov`, same as it's always meant to be, while
   * still letting the player freely change their mind about WHERE within that budget to
   * end up (the actual point of free repositioning). */
  moveBudgetUsed: number;
}

export interface UnitPublic {
  id: string;
  name: string;
  classId: ClassId;
  className: string;
  role: string;
  side: Side;
  sprite: SpriteId;
  hp: number;
  escaped?: boolean;
  maxHp: number;
  atk: number;
  mag: number;
  def: number;
  dex: number;
  /** Elemental resistance percentages, independent of general DEX. Missing values are zero. */
  resistances?: Resistances;
  weaponSkills?: WeaponSkillValues;
  healingSkill?: number;
  initiative: number;
  initiativeRoll: number;
  mov: number;
  /** Movement left this turn: MOV minus what has already been walked, clamped to 1 while
   * restrained. Movement is a pool the action does not cancel — spend two hexes, cast, and
   * the other three are still yours — so this is the number that actually matters in play,
   * and the panel counts it down instead of showing the untouched base all turn. */
  movLeft: number;
  minRange: number;
  maxRange: number;
  moved: boolean;
  acted: boolean;
  x: number;
  y: number;
  level: number;
  xp: number;
  bag: Bag;
  spells: Spells;
  frostCharges?: number;
  weaponId: string | null;
  weaponEnh: number;
  size: number;
  diseased: boolean;
  poisoned: boolean;
  poisonTier?: PoisonTier;
  poisonMag?: number;
  bleeding: boolean;
  bleedRoundsLeft?: number;
  blessedHitBonusPct?: number;
  blessedRoundsLeft?: number;
  /** Delayed lightning echo that resolves at the start of this unit's turn. */
  shock: { dice: number; faces: number; bonus: number; mag?: number } | null;
  /** True once the party's hunger streak has passed its 3-day grace period. Optional so
   * older battle saves remain valid. */
  hungry?: boolean;
  /** 0..90 — the live stat penalty this unit is carrying from hunger, in percent. Only
   * meaningful alongside `hungry: true`; see hungerPenaltyFor in overworld.ts. */
  hungerPct?: number;
  fullness?: number;
  /** Remaining own turns spent retreating without attacking. Optional for older saves. */
  fearTurns?: number;
  fearSourceId?: string;
  stunned: boolean;
  crippled: boolean;
  offHandId: string | null;
  summoned: boolean;
  /** A summoned familiar's own spell charges — see the matching field on Unit. */
  spellCharges?: number;
  /** Familiar Maior's own Dreno de Vida charges — see the matching field on Unit. */
  lifeDrainCharges?: number;
  asleep: boolean;
  /** True while this unit's current cell sits inside an active Web of Dreams zone — purely a
   * display flag; the movement penalty it implies is computed live off the zone, not stored. */
  restrained: boolean;
  /** Worn gear by slot — lets the status sheet explain which stats gear is boosting (see
   * gearStatBonus) and which specific piece is behind each one. */
  gear: Partial<Record<EquipSlot, string>>;
}

export interface WeaponDef {
  /** Name-specific passive magic on the earlier mage staff family. */
  magic?: import("./mageStaffMagic").MageStaffMagic;
  /** Quest rewards may be equipped and sold, but never offered or purchased at shops. */
  questOnly?: boolean;
  /** Suggested campaign level for an authored weapon series; does not restrict equipping. */
  recommendedLevel?: number;
  id: string;
  weaponType: WeaponType;
  name: string;
  /** Classes (base and prestige) allowed to equip this weapon. */
  usableBy: ClassId[];
  dice: number;
  faces: number;
  bonus: number;
  price: number;
  /** Attack range, D&D-weapon-style — determined by the weapon itself, not the wielder's class. */
  minRange: number;
  maxRange: number;
  /** True for bow/crossbow-type weapons: grants the elevated-terrain range bonus (see effectiveMaxRange). */
  ranged?: boolean;
  /** Occupies both hands — an offHand item can't be equipped alongside it. */
  twoHanded?: boolean;
  /** The one class (of usableBy's pool, if it's a shared one) this weapon is thematically
   * tuned for — a staff named after a school of magic, say — and deals 10% more damage to
   * whoever wields it while actually being that class. Every other class in the pool can
   * still equip and use it at no penalty, just without the bonus. */
  bonusClass?: ClassId;
}

/**
 * Paper-doll equipment slots. "mainHand" isn't stored here — it's the existing weapon
 * system (SaveData.equipped/weapons). Every other slot is a bare skeleton for now: the
 * type and the UI exist, but EQUIPMENT in data.ts has no items in it yet.
 */
export type EquipSlot =
  | "head"
  | "neck"
  | "shoulders"
  | "back"
  | "chest"
  | "hands"
  | "waist"
  | "legs"
  | "feet"
  | "ring1"
  | "ring2"
  | "offHand";

export interface EquipmentDef {
  id: string;
  /** Weapon equipment only: proficiency for an off-hand strike. */
  weaponType?: WeaponType;
  name: string;
  slot: EquipSlot;
  /** Classes (base and prestige) allowed to equip this item. Empty/omitted = any class. */
  usableBy?: ClassId[];
  hp?: number;
  atk?: number;
  mag?: number;
  def?: number;
  dex?: number;
  /** Elemental resistance percentages, independent of general DEX. Missing values are zero. */
  resistances?: Resistances;
  mov?: number;
  price?: number;
  /** offHand-slot items only: "weapon" grants an off-hand attack command (using this
   * item's own dice/range below); "shield" grants Shield Bash instead (this shield's own
   * dmgMul, 70% chance to stun for the target's next turn). Both show as the command menu's
   * first option, and both are blocked while the main hand holds a WeaponDef.twoHanded
   * weapon. */
  kind?: "weapon" | "shield";
  dice?: number;
  faces?: number;
  bonus?: number;
  minRange?: number;
  maxRange?: number;
  /** Shield Bash's damage multiplier for this specific shield — stronger shields close the
   * gap toward 1 (no penalty at all on the best ones), instead of one flat rate for every
   * shield. */
  dmgMul?: number;
}

export interface Forecast {
  attacker: string;
  defender: string;
  dmgOut: number;
  dmgBack: number;
  /** Chance (0-100) the outgoing hit / the counter lands — see combat.ts's
   * physicalHitChance/magicalHitChance. */
  hitOut: number;
  hitBack: number;
  canCounter: boolean;
  critOut: boolean;
  kill: boolean;
}

export interface TerrainHover {
  id: TerrainId;
  name: string;
  /** Movement points entering this tile costs. Only meaningful when `passable`. */
  moveCost: number;
  def: number;
  atk: number;
  passable: boolean;
  /** True when the tile stops shots and line of sight. */
  blocksShot: boolean;
  hazard?: string;
  note?: string;
  /** A persistent spell zone covering this tile. When present, inspection presents the
   * zone instead of the underlying terrain rules. */
  spellZone?: {
    kind: "webOfDreams" | "iceStorm";
    roundsLeft: number;
    movementCap?: number;
    sleepChance?: number;
    sleepDice?: string;
    damageFormula?: string;
  };
}

export interface HudSnapshot {
  phase: Phase;
  banner: string | null;
  selected: UnitPublic | null;
  hoveredUnit: UnitPublic | null;
  terrain: TerrainHover | null;
  mode: InputMode;
  canAttack: boolean;
  /** Selected unit has an unused offHand item and could still act — "weapon" for an
   * off-hand attack command, "shield" for Shield Bash, null when neither applies. */
  offHandKind: "weapon" | "shield" | null;
  canLockpick: boolean;
  forecast: Forecast | null;
  turn: number;
  objective: string;
  missionTitle: string;
  playerAlive: number;
  enemyAlive: number;
  busy: boolean;
  /** Whether Cancel can safely stop a player movement animation before its queued action resolves. */
  canCancelMovement: boolean;
  result: "victory" | "defeat" | null;
  winAvailable: boolean;
  /** The waypoint currently offering an exit action — see DecorationDef.exitKind and
   * BattleEngine.evaluateEnd. Null when no living player unit is standing on a waypoint. */
  activeExit: DecorationPlacement | null;
  /** Whether the movement taken this turn can still be taken back — see canUndoMove. */
  canUndoMove: boolean;
  /** For a spell that picks more than one target (Magic Missile at level 3+), how many it
   * wants and how many are already chosen. Null when nothing is waiting on a pick. */
  targetPrompt: { name: string; need: number; picked: number } | null;
  zoom: number;
  speedMode: "slow" | "normal" | "fast";
  tip: string | null;
  inspected: UnitPublic | null;
  pendingFoe: UnitPublic | null;
  spellReady: boolean;
  /** A skill is aimed and waiting for Confirmar/Cancelar (see BattleEngine.handleCell). */
  spellArmed: boolean;
  /** Hit chance (0-100) of the aimed weapon skill against the enemy at its aim; null for
   * spells and for aims with no enemy. */
  spellHitChance: number | null;
  spellKind: SpellKind | null;
  /** Battle turn order (both sides mixed), highest opening initiative first. */
  turnQueue: { id: string; name: string; side: Side; acted: boolean; active: boolean; initiative: number }[];
  /** Rolling combat log — attacks, spells, heals, kills, loot — newest last. */
  log: string[];
  /** Set the instant a chest is opened, cleared only when the player dismisses the popup
   * (see acknowledgeChestLoot) — not a transient "just happened" flag like tip, so it
   * survives sitting on screen until the player actually reads it. */
  chestLoot: { unitName: string; ember: number; items: { name: string; icon: string; tip?: string }[] } | null;
  /** Set the instant a dialog-bearing NPC is clicked, cleared only via acknowledgeDialog —
   * same "sits until dismissed" convention as chestLoot above. */
  pendingDialog: DialogTree | null;
}

export interface WalkDirs {
  front: HTMLImageElement;
  back: HTMLImageElement;
  side: HTMLImageElement;
}

export interface GameArt {
  /** Every art variant for a terrain type, e.g. tiles.plains[0]/[1] — index 0 is the
   * default (what existing missions render with when a tile doesn't name a variant). */
  tiles: Record<TerrainId, HTMLImageElement[]>;
  /** Multi-hex decoration art, keyed by DecorationDef.id. */
  decorations: Record<string, HTMLImageElement>;
  sprites: Record<SpriteId, HTMLImageElement[]>;
  attacks: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** A second, distinct attack cut for sprites with an alternate move (Familiar 3, plus
   * Neera's placeholder bow-skill ATT). Unit.idleAlt selects it for Familiar 3. */
  attacks2: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Left-facing counterpart for a second attack cut when its source has authored mirrored frames. */
  attacks2Left: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Short-range (off-hand dagger/katar) attack cut, atk-short-*.png, for the sprites that
   * have one (currently Neera). Used instead of `attacks` whenever the strike is with the
   * off-hand weapon — the unit's own off-hand attack or its off-hand counter. */
  attacksShort: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** A distinct pose for casting a spell, for the few sprites that have one cut — falls back
   * to `attacks` (the melee swing) for every sprite without one, same as it always did. */
  casts: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Walk cycles, for the sprites that have one cut. Played only while a unit is actually
   * moving; a sprite without one keeps falling back to its idle loop run faster, which is
   * what every sprite did before walk cycles existed. */
  walks: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Optional left-facing walk/attack cuts. A sprite with these skips the scale-flip while
   * moving or striking and plays this set when facing === -1 (the right-facing set lives in
   * `walks` / `attacks`). Idle still uses the shared 12-frame sheet and the regular flip. */
  walksLeft: Partial<Record<SpriteId, HTMLImageElement[]>>;
  attacksLeft: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Left-facing counterpart to `casts`, for the sprites that have one cut. Falls back to
   * `casts` (mirrored via the regular flip) for every sprite without one. */
  castsLeft: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** A distinct pose for the defender's own counter-attack stages (counterLunge/Hit/
   * Recover), for the few sprites that have one cut — falls back to `attacks` (the same
   * swing used for a normal attack) for every sprite without one, same as it always did
   * before this existed. */
  counters: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Left-facing counterpart to `counters`. Falls back to `counters` (mirrored via the
   * regular flip) for every sprite without one. */
  countersLeft: Partial<Record<SpriteId, HTMLImageElement[]>>;
  idles: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** A second, distinct standing loop for the few sprites that have one (currently just
   * Malrec) — Unit.idleAlt picks between this and the sprite's regular idle/`sprites` pool,
   * flipping once per that unit's own turn so it visibly alternates stance turn to turn. */
  idles2: Partial<Record<SpriteId, HTMLImageElement[]>>;
  walkDirs: Partial<Record<SpriteId, WalkDirs>>;
  /** Animated walk cycles for moving up the map (away from the camera) and down it (toward
   * the camera) — move-up-*.png / move-down-*.png. Only sprites listed in assets.ts's
   * WALK_UP_DOWN_FRAMES have them; everyone else keeps their left/right walk in every
   * direction, exactly as before. */
  walksUp: Partial<Record<SpriteId, HTMLImageElement[]>>;
  walksDown: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Cosmetic alternate walk cycle (move2-*.png / move2-left-*.png) — played instead of the
   * regular walk on roughly one move in three, purely for variety (Familiar Titã's glowing-rune
   * walk). No gameplay effect. */
  walks2: Partial<Record<SpriteId, HTMLImageElement[]>>;
  walksLeft2: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Death sheet (death-*.png), for the sprites that have one: plays once when the unit dies,
   * holds the last frame for a moment, then the unit fades out as usual. Sprites without one
   * just fade out on death, exactly as before. */
  deaths: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Alternate death sheet (death2-*.png), played instead of `deaths` about one death in
   * three (Unit.deathAlt), for variety. */
  deaths2: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Hit-reaction sheet (hit-*.png), for the sprites that have one: plays once whenever the
   * unit takes damage, unless it is attacking or walking at that moment. On a killing blow it
   * plays first and the death sheet follows. Sprites without one just flash, as before. */
  hits: Partial<Record<SpriteId, HTMLImageElement[]>>;
  /** Second hit-reaction sheet (hit2-*.png), for the sprites that have one: their hits cycle
   * hit, hit, hit2 (see BattleEngine.hitPoolFor). */
  hits2: Partial<Record<SpriteId, HTMLImageElement[]>>;
  impact: HTMLImageElement[];
  /** Ultra-realistic Fireball core; its trail and light remain procedural. */
  fireballCore: HTMLImageElement;
  /** Ultra-realistic Caustic Venom core. */
  causticVenomCore: HTMLImageElement;
  /** Ultra-realistic travelling arrow. */
  arrowCore: HTMLCanvasElement;
  /** Photoreal Relâmpago cores (flicker set). Additive-blend on black. */
  lightningCores: HTMLImageElement[];
  /** Real alpha-cutout spiderweb photo, stamped once per hex a live Web of Dreams zone
   * covers (see renderGround's web-zone loop) — replaces the old procedural WebGL "web"
   * shader quad, which read as an odd glowing patch layered on top of the movement-range
   * highlight instead of a physical web sitting on the ground. */
  webfloor: HTMLImageElement;
  /** Optional full-canvas backdrop, keyed by mission id. */
  backdrops: Record<string, HTMLImageElement>;
}

export interface BattleUnitSnap {
  id: string;
  name: string;
  classId: ClassId;
  side: Side;
  x: number;
  y: number;
  hp: number;
  escaped?: boolean;
  maxHp: number;
  atk: number;
  mag: number;
  def: number;
  dex: number;
  /** Elemental resistance percentages, independent of general DEX. Missing values are zero. */
  resistances?: Resistances;
  weaponSkills?: WeaponSkillValues;
  healingSkill?: number;
  /** Optional for compatibility with saves made before individual initiative existed. */
  initiative?: number;
  initiativeRoll?: number;
  /** Optional for compatibility with battle saves created before attribute points existed. */
  statPointAllocation?: StatPointAllocation;
  mov: number;
  minRange: number;
  maxRange: number;
  moved: boolean;
  acted: boolean;
  facing: 1 | -1;
  faceDx?: number;
  faceDy?: number;
  alive: boolean;
  fade: number;
  level: number;
  xp: number;
  bag: Bag;
  spells: Spells;
  weaponId: string | null;
  weaponEnh: number;
  shock: { dice: number; faces: number; bonus: number; mag?: number } | null;
  /** Enemy-only Choque charges. Distinct from `shock` (Relâmpago echo DoT). */
  shockCharges: number;
  /** Enemy-only legacy FantomForce uses remaining in this battle. Optional for older saves. */
  fantomForceCharges?: number;
  /** Cultist V2 owns a separate Frost pool; player mages use Tier 2 charges. */
  frostCharges?: number;
  blessedHitBonusPct?: number;
  blessedRoundsLeft?: number;
  diseased: boolean;
  diseaseBase: { atk: number; mag: number; def: number; dex: number; mov: number } | null;
  poisoned: boolean;
  poisonTier?: PoisonTier;
  /** Legacy (pre-tier saves): 4 = Lesser, 10 = Poison. Read only when poisonTier is missing. */
  poisonFaces?: 4 | 10;
  poisonMag?: number;
  poisonResist?: number;
  bleeding?: boolean;
  bleedRoundsLeft?: number;
  bleedRoundMarker?: number;
  /** Remaining own turns spent retreating without attacking. Optional for older saves. */
  fearTurns?: number;
  fearSourceId?: string;
  stunned: boolean;
  stunTurns: number;
  crippled: boolean;
  /** Optional for compatibility with battle saves created before hunger existed. */
  hungerPenaltyPct?: number;
  fullness?: number;
  offHandId: string | null;
  gear: Partial<Record<EquipSlot, string>>;
  summoned: boolean;
  asleep: boolean;
  sleepTurns: number;
  guaranteedDrop: boolean;
  dialog: DialogTree | null;
  /** Carries Spawn.useClassSprite through a save/resume round-trip — see its doc comment.
   * Optional for compatibility with battle saves made before this existed. */
  useClassSprite?: boolean;
  moveBudgetUsed: number;
}

export interface BattleSnapshot {
  heroSkills?: Record<string, Partial<Record<`${ResistanceElement}Resistance` | `${WeaponType}Weapon` | "healing", number>>>;
  affinityScores?: Record<string, number>;
  missionId: string;
  /** missionMapKey of the map this fight was played on. A fight saved on an older version
   * of the map (or before this existed) is not resumed — the battle starts on the current map. */
  mapKey?: string;
  /** The mission's intro dialog was already shown in this fight — never replay it on load. */
  introDialogDone?: boolean;
  /** Each enemy's enmity table (enmity.ts EnmitySnapshot): { enemyId: { heroId: [ce, ve] } }. */
  enmity?: Record<string, Record<string, [number, number]>>;
  turn: number;
  phase: Phase;
  units: BattleUnitSnap[];
  tiles: TerrainId[];
  decorations: DecorationPlacement[];
  turnOrder: string[];
  activeUnitId: string | null;
  selectedId: string | null;
  lootEmber: number;
  /** Optional for compatibility with battle saves created before ration loot existed. */
  lootRations?: number;
  /** Inn-quest pickups collected so far this battle, "questId:pickupId". */
  questFound?: string[];
  lootWeapons: string[];
  lootEquipment: string[];
  ownedWeapons: string[];
  /** center/radius optional for compatibility with battle saves made before Dreaming Web's
   * floor patch became one zone-wide WebGL effect instead of a per-hex stamp — a zone
   * resumed from an older save without them just renders no web patch until it's re-cast
   * (see BattleCanvas), same graceful-degradation approach as every other optional field
   * here. */
  webZones: { cells: string[]; roundsLeft: number; center?: Point; radius?: number; sleepChance?: number }[];
  /** Lingering Ice Storm patches. Optional so battles saved before Ice Storm remain loadable. */
  iceStormZones?: { cells: string[]; roundsLeft: number; createdAt: number; center: Point; radius: number; damageDice: number; damageFaces: number; damageMul: number; casterMag: number; casterLevel: number; casterId: string; side: Side }[];
  auraZones: { cells: string[]; roundsLeft: number; kind: "protection" | "intimidation"; side: Side; pct: number }[];
  log: string[];
  winAvailable: boolean;
  chestLoot: { unitName: string; ember: number; items: { name: string; icon: string; tip?: string }[] } | null;
  pendingDialog: DialogTree | null;
  turnRestrained: boolean;
  /** True when beginUnitTurn already ran for the current actor — load must not re-apply
   * start-of-turn echo/poison/stun. False when the next unit hasn't opened their turn yet. */
  turnBegan: boolean;
  /** Which cells the party has seen, one bit each, row-major, base64. Only written on
   * a mission under fog; absent everywhere else, and absent reads as "nothing seen
   * yet", which is also the right answer for a save made before fog existed since no
   * such save can be of a fogged mission. What is currently *visible* is never stored
   * — it falls out of where the party stands, so load recomputes it. */
  explored?: string;
  /** Ids of foes that have already spotted the party, so an alerted enemy stays
   * alerted across a save. Only written under fog; absent reads as none awake. */
  awake?: string[];
}

export interface SaveData {
  version: number;
  completed: string[];
  /** Stable enemy/neutral spawn ids already defeated in crossing dungeons. These persist
   * between incursions so a cleared monster stays gone when the party explores again. */
  crossingDefeatedSpawns: Record<string, string[]>;
  /** Inn quests (see quests.ts). Accepted and unfinished quest ids. Absent reads as none. */
  questsActive?: string[];
  /** Quest ids already handed in and paid. */
  questsDone?: string[];
  /** Quests the party has learned of (offered by their giver) without accepting yet. */
  questsDiscovered?: string[];
  /** Story chapter reached (see progression.ts). Absent reads as chapter 1. */
  chapter?: number;
  /** Named story flags set by quests/triggers (see progression.ts). */
  flags?: string[];
  /** Every scripted conversation that has already played in this save (dialogSeenKey in
   * save.ts). A conversation listed here never opens by itself again — not after saving,
   * loading, leaving or replaying the mission. No size cap: a campaign holds thousands. */
  dialogsSeen?: string[];
  /** NPC ids the party has talked to (Inn NPC ids such as "brue"). */
  npcTalked?: string[];
  /** Fetch-quest pickups already picked up, as "questId:pickupId". */
  questItems?: string[];
  /** Kill-quest target names already killed (recorded even before the quest is accepted). */
  questKills?: string[];
  unitHp: Record<string, number>;
  levels: Record<string, number>;
  xp: Record<string, number>;
  /** Hero name → permanent point allocation. Every level after level 1 grants three points. */
  statPointAllocations: Record<string, StatPointAllocation>;
  bags: Record<string, Bag>;
  /** Hero name → promoted ClassId chosen at PROMOTE_LEVEL. Unset until the player picks. */
  promotions: Record<string, ClassId>;
  /** Owned WeaponDef id → enhancement level (0..5). Presence in the map means it's owned. */
  weapons: Record<string, number>;
  /** Hero name → equipped WeaponDef id. */
  equipped: Record<string, string>;
  /** Hero name → slot → equipped EquipmentDef id. */
  equipment: Record<string, Partial<Record<EquipSlot, string>>>;
  /** Owned but unassigned EquipmentDef id → count — the party's shared gear stash. Loot
   * lands here first (never auto-equipped onto whoever found it); the player assigns it to
   * a hero from the Paperdoll picker, same as the weapon pool already works. */
  looseEquipment: Record<string, number>;
  /** Hero name → tier key → spell uses spent so far in the current scenario (a world-map
   * location's whole run of missions) — carried between missions within one location so
   * charges don't refill until that scenario ends. Cleared back to {} whenever a mission
   * starts a fresh scenario (see startBattle in GameApp.tsx); Stone Bridge always resets,
   * being the tutorial. */
  spellUses: Record<string, Partial<Record<TierKey, number>>>;
  ember: number;
  emberSeeded: boolean;
  muted: boolean;
  updatedAt: number;
  pendingMission: string | null;
  /** Mid-battle board: set by Save during a fight so Load resumes that combat instead of
   * restarting it from the briefing. Cleared on victory, defeat, or a fresh mission start. */
  battle: BattleSnapshot | null;
  /** The Vargan forge intro has played for this party. Absent reads as not yet seen. */
  seenSmithIntro?: boolean;
  /** The movement/hunger explainer, shown once the first time the RPG overworld map
   * screen opens for this party. Absent reads as not yet seen. */
  seenOverworldIntro?: boolean;
  /** Wisp Forest's entrance cinematic has played for this campaign. */
  seenWispForestIntro?: boolean;
  /** The party's first arrival at the Inn cinematic has played for this campaign. */
  seenInnArrivalIntro?: boolean;
  /** The campaign's chosen map/travel style. Test mode never persists this choice. */
  mapMode?: "classic" | "rpg";
  /** RPG map only: current hex position and day count. Unused by the classic map. */
  overworldPos: { col: number; row: number };
  gameClock: number;
  /** Hour on the travel clock (0..23); older saves start at 08:00. */
  gameHour?: number;
  /** Shared relationship score for each named hero pair, from 0 to 100. */
  affinityScores?: Record<string, number>;
  /** Chosen response per directed companion chapter; preserved across leader changes. */
  companionConversations?: Record<string, -3 | 0 | 3>;
  partyFormation?: string[];
  /** Hero who walks the world map and free-roam maps (Party menu); absent means Kael. */
  partyLeader?: string;
  /** Actual travel hours with an empty hunger meter; absent falls back to hungerStreak days. */
  hungerHours?: number;
  /** RPG map only: every hex ("col,row") the party has ever stood on — drives the fog of
   * war (see OverworldMapScreen): a location pin other than the Inn only shows once its
   * hex is in here, and the dark overlay clears in a radius around each one. Grows,
   * never shrinks — nothing un-explores a hex once seen. */
  exploredHexes: string[];
  /** Cumulative travel cost; each adjacent step costs one, including backtracking. */
  overworldMoveBudgetUsed: number;
  /** Fullness remaining per hero (0..120). Missing heroes start full. */
  heroHunger: Record<string, number>;
  /** Overworld illnesses persist between travel and battles until cured with a disease
   * potion or the Curar Doença spell. Missing heroes are healthy for old saves. */
  heroDiseases: Record<string, boolean>;
  /** Poison residue that survives between battles until cured. Older saves default to none. */
  heroPoisons: Record<string, PoisonTier>;
  /** MAG of whoever poisoned each hero, so the road ticks pierce resistance like battle ones. */
  heroPoisonMag?: Record<string, number>;
  /** Per-hero skills (skills.ts) — Resistência a Veneno, … Older saves start every skill at 0. */
  heroSkills?: Record<string, Partial<Record<`${ResistanceElement}Resistance` | `${WeaponType}Weapon` | "healing", number>>>;
  /** Travel training (party menu Skills tab): the one skill each hero practises on the road.
   * Nothing trains until the player picks one. */
  travelTraining?: Partial<Record<string, `${ResistanceElement}Resistance` | `${WeaponType}Weapon` | "healing">>;
  /** Road hours banked toward each hero's next travel-training point (see advanceTravelTraining). */
  travelTrainingHours?: Record<string, number>;
  /** Party-wide ration stock. One ration refills one character's fullness to 100%; inn
   * meals are bought separately. A real backpack item that stacks by RATION_STACK_MAX. */
  rations: number;
  /** Consecutive days the party went unfed on the RPG map (0 = fine). Drives the Hungry
   * status once past the 3-day grace period — see hungerPenaltyFor in overworld.ts.
   * Resets to 0 the moment rations flow again or the party reaches an Inn. */
  hungerStreak: number;
  /** Days left of doubled battle-encounter odds on the road, set by the "large tracks cross
   * the path" event (see BATTLE_ENCOUNTER_CHANCE/stepOverworld in overworld.ts) — something
   * really is out there, whether or not it's crossed your path yet. Decrements by one every
   * travel day, 0 = normal odds. */
  alertStreak: number;
  /** The travel encounter's mission id from the last time one triggered — excluded from the
   * very next pick (see travelEncounterIds/stepOverworld in overworld.ts) so the same fight
   * never repeats twice in a row. null before the first one ever fires. */
  lastRoadEncounterId: string | null;
  /** Travel battle maps seen during this campaign; legacy name retained for existing saves. */
  roadEncountersSeen?: string[];
}

export interface SaveBank {
  version: number;
  lastSlot: number;
  muted: boolean;
  slots: Array<SaveData | null>;
}

export interface GrowthLine {
  name: string;
  from: number;
  to: number;
  hpBattle: number;
  maxFrom: number;
  restHp: number;
  levelHp: number;
  hpCamp: number;
  maxTo: number;
  powerFrom: number;
  powerTo: number;
  powerKind: "AT" | "MAG";
  atkFrom: number;
  atkTo: number;
  magFrom: number;
  magTo: number;
  defFrom: number;
  defTo: number;
  dexFrom: number;
  dexTo: number;
  fallen: boolean;
  /** XP toward the next level at the end of the mission (0..expToLevel(level)-1). */
  xp: number;
  /** XP toward level `from + 1` at mission start. The ResultScreen's XP bar fills from here
   * to full, refills once per level gained, and ends at `xp` (progress toward `to + 1`). */
  xpFrom: number;
  /** Extra spell uses this level-up granted (e.g. "+1 T1 · +1 T2"). Empty when the new
   * level didn't add slots — spent charges are never refilled, only new slots land. */
  skillGain?: string;
}
