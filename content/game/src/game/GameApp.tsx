import { hasSquareMapBorder } from "./mapFloor";
import { FARMLANDS_SERVICES, isFarmlandsSmithWeapon, isFarmlandsConnector, farmlandsDoorLabel } from "./farmlands";
import { FROST, frostPower } from "./frost";
import { campaignPartySetup } from "./campaignParty";
import { dexEscapeChance } from "./dexterity";
import { trainedWeaponSkills, weaponTypesForClass, weaponModifiers } from "./weaponSkills";
import { WEAPON_TYPE_LABELS, weaponSkillAccuracy, weaponSkillDamageMultiplier } from "./weaponTypes";
import { healingAmount, skillValue, rollSkillGain, skillGainChance, skillResistances, SKILL_CAP, SKILL_GAIN, TRAVEL_TRAINING_HINT_FLAG } from "./skills";
import { sumResistances, RESISTANCE_ELEMENTS, RESISTANCE_LABELS } from "./resistances";
import { isFloorConnector, floorConnectorDirection } from "./data";
import { WISP_BOSS_ID, WISP_CROSSING_ID, wispCrossingCompleted, routeWispCrossing, completedAfterWispVictory } from "./wispCrossing";
import { removeWallsUnderWatchtowerEntrances } from "./watchtowerDungeon";
import { applyPartyFormation, cleanPartyFormation, cleanPartyLeader, partyLeaderOf } from "./partyFormation";
import { OptionsButton } from "./OptionsMenu";
import { CUTSCENE_SUBTITLES, syncSubtitles } from "./cutsceneSubtitles";
import { uiText, useGamePreferences, type Translations } from "./gamePreferences";
import { GraphicsQualityControl } from "./GraphicsQualityControl";
import { cloneElement, Fragment, isValidElement, type CSSProperties, type PointerEvent as ReactPointerEvent, type ReactElement, type ReactNode, useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState, useSyncExternalStore } from "react";
import { ChevronDown, ChevronLeft, ChevronRight, ChevronUp, Dices, Grip, ListOrdered, Lock, Pencil, RotateCcw, Shuffle, SlidersHorizontal, Swords, Volume2, VolumeX, X } from "lucide-react";
import { Button } from "@/components/ui/button";
import { artProgress, ensureDecorationArt, ensureTerrainArt, ensureSpriteArt, loadGameArt, portraitFor, releaseSpriteArt, subscribeArtProgress, TILE_VARIANT_COUNT, tileVariantName, tileVariantSrc } from "./assets";
import { getAudioVolumes, getCutsceneAudioSettings, installAudioUnlock, pauseMusic, playFile, playMenuMusic, playTheme, resumeAudio, resumeMusic, setCutsceneEnabled, setCutsceneVolume, setMusicVolume, setMuted, setSfxVolume, sfxPlay, stopMusic, unlockAudio } from "./audio";
import { Battle3D as BattleCanvas } from "./Battle3D";
import { MapEditorScreen as MapEditorScreen3D } from "../editor/ember/MapEditorScreen";
import { ENGINE2_V3_CITY_VARIANT } from "../ember/tileVariants";
import { ELEMENT_LABELS, PLACEABLE_ELEMENT_KINDS, type PlaceableElementKind } from "./gfx/params";
import { ELEMENT_FX_REGISTRY, pixelDefaults, pixelPresetsFor, type PixelElement, type PixelElementSettings } from "./gfx/three/ProceduralElementEmitter";
import { THREE_D_DOOR_VARIANTS } from "./data";
import { decorationPlacementArt } from "./data";
import { SOLID_CART_DECOR_IDS } from "./data";
import { FANTOM_FORCE } from "./data";
import { victoryRewardFor } from "./victory-reward";
import { ENCOUNTER_NPC_IDS, encounterNpcSpawn, type EncounterNpcId } from "./encounter-npcs";
import { DEFAULT_AMBIENT_INTENSITY, DEFAULT_BLOOM_INTENSITY, DEFAULT_SUN_INTENSITY, TIME_OF_DAY_LIGHT } from "./gfx/three/ThreeBattleRenderer";
import { getDevGfx, setDevGfx, subscribeDevGfx, type DevGfxSettings } from "./gfx/three/devGfx";
import { DevGfxPreview } from "./gfx/three/DevGfxPreview";
import { VfxDebugPanel } from "./gfx/three/VfxDebugPanel";
import { Hd2dTestScreen } from "./gfx/three/Hd2dTestScene";
import { HEALER_AILMENT_PRICE, HEALER_CAST_PRICE, NIGHT_REST_PRICE, InnScreen } from "./InnScreen";
import { PartyInventoryOverlay, ItemTip } from "./InventoryScreens";
import { DialogOverlay } from "./DialogOverlay";
import { CompanionConversations } from "./CompanionConversations";
import { resolveCompanionReply } from "./companionDialogues";
import { AFFINITY_HEROES } from "./affinity";
import { LIGHT_DEFS } from "./lighting";
import { DialogEditor } from "./DialogEditor";
import { BLESS, BARRICADE_LIKE_DECOR, BIG_HOUSE_DECOR_IDS, SOLID_HOUSE_DECOR_IDS, SOLID_ROCK_DECOR_IDS, CAUSTIC_VENOM, DIVINE_BOLT, MINOR_VENOM, CHEST_LOOT, CLASSES, DEADWOODS_DECOR_IDS, FOREST_DECOR_IDS, CLEAVE, cleaveFormula, CURE_DISEASE, CURES, DECORATIONS, DOUBLE_STRIKE, doubleStrikeFormula, EQUIPMENT, EXP_TO_LEVEL, expToLevel, FAMILIAR_SPELL, FIREBALL, formatSpellUseGains, LIFE_DRAIN, lifeDrainFormula, lifeDrainHealMul, HOUSE_DECOR_IDS, KILL_DROP_CHANCE, LIGHTNING, LIGHTNING_T3, LONG_SHOT, longShotFormula, BLOODY_SHOT, bloodyShotMul, bloodyShotBleed, bloodyShotFormula, PROVOKE, provokeFormula, MAGIC_MISSILE, PIERCING, piercingMul, PIERCING_THRUST, MAX_GRID, MAX_LEVEL, MIN_GRID, POTIONS, POTION_LOOT_WEIGHT, PROMOTE_LEVEL, PROMOTED_BASE, PROMOTIONS, rulesClass, SHOCK, STAT_POINTS_PER_LEVEL, SUMMON_FAMILIAR, PHANTASMAL_FORCE, PHANTASMAL_FORCE_UNLOCK_LEVEL, phantasmalForceFormula, SUMMON_FAMILIAR2, SUMMON_FAMILIAR2_UNLOCK_LEVEL, SUMMON_FAMILIAR3, SUMMON_FAMILIAR4, SUMMON_ZOMBIE_DOG, SWEEP, TRIP, TERRAIN, WEAPONS, WEAPON_MAX_ENH, WEB_OF_DREAMS, WARP, BAG_MAX, LOCKPICK_PRICE, POTION_CARRY_MAX, POTION_PRICE, RATION_STACK_MAX, RATIONS_PRICE, barricadeDecor, decorationCells, placedFootprint, placedBlockingFootprint, decorationImage, decorationImageWebp, diceFormula, emberForKill, enemyLevelFor, equippedPouchId, fireballFormula, healFormula, heroRecruited, lightningFormula, lightningTier3Formula, dressMap, isSummonClass, MUSIC_TRACKS, SUMMON_CLASSES, parseLayout, potionLabel, potionTooltip, lockpickTooltip, partyBagHasRoom, pouchIcon, rangeLabel, rollPotion, sheetLine, spellFormula, spellIcon, spellTier, spellUseGains, startingBags, statsFor, terrainNote, tierKey, tierUses, weaponEnhCost, weaponSellValue, equipmentFitsSlot, gearStatBonus, MULTI_SHOT, multiShotFormula, SECOND_WIND, secondWindPct, auraPower, AURA_OF_PROTECTION, INTIMIDATING_PRESENCE, DIVINE_WRATH, divineWrathPower, SHOULDER_SMASH, shoulderSmashFormula, STAMPEDE, stampedeFormula, BULL_RUSH, BULL_RUSH_UNLOCK_LEVEL, EXECUTIONER_STRIKE, SHIELD_BASH, POISON_BREATH, poisonBreathPower, poisonBreathFormula, BURNING_HANDS, CREATE_FOOD_AND_WATER, createFoodAndWaterFormula, createFoodAndWaterPower, rollDice, type SpellTier } from "./data";
import { QUESTS, activePickupsFor, questById, questProgress, questStatus, questsFor } from "./quests";
import { advanceProgression, evaluate, isGatedMission, missionAccess, type MissionAccess, type ProgressExtras } from "./progression";
import { BattleEngine, heroSpriteFor } from "./engine";
import { MapPreviewCanvas, type PreviewDecorationSelection, type PreviewUnitSelection } from "./MapPreviewCanvas";
import { WorldMapScreen } from "./WorldMapScreen";
import { campaignHour, campaignTimeOfDay, usesTravelClock } from "./campaignTime";
import { moonPhaseOf } from "./moonPhase";
import { OverworldMapScreen } from "./OverworldMapScreen";
import { LoadingCurtain, useLoadingCurtain } from "./MapLoadingOverlay";
import { rememberedStepMs, rememberStepMs, StepClock, stepPercent } from "./loadStages";
import { HungerBar } from "./HungerBar";
import { buyInnMeal, fullness, useRation } from "./hunger";
import { POISON_TIERS, poisonDice, poisonTierOf } from "./poison";
import { hungerPenaltyFor, partyIsFed, stepOverworld, teleportOverworld, worldToHex, type OverworldEvent } from "./overworld";
import { GoldAmount } from "./GoldAmount";
import { DISPLAY_VERSION } from "./version";
import { returnToTitle } from "./titleNavigation";
import { TURN_UNDEAD, TURN_UNDEAD_PROGRESSION, turnUndeadPower, ICE_STORM, iceStormPower } from "./data";
import {
  ALL_LOCATIONS,
  ALL_MISSIONS,
  DEFAULT_LOCATION_SUBMAPS,
  LOCATION_SLOTS,
  MAP_ACTIVE_KEY,
  MAP_ACTIVE_DRAFTS_KEY,
  MAP_VERSIONS_KEY,
  RANDOM_ENCOUNTER_REGIONS,
  isRandomEncounter,
  isCrossingDungeon,
  keepsDefeatedSpawns,
  clearSessionMapOverride,
  draftToMission,
  latestSerialFor,
  registerSessionMapOverride,
  loadActiveDrafts,
  loadActiveVersions,
  loadLocaisLocal,
  LOCAIS_LOCAL_KEY,
  loadVersionStore,
  locationFill,
  locationForMission,
  locationsForOrder,
  mapFileName,
  missionById,
  missionMapKey,
  missionsForLocation,
  latestSavedDraft,
  saveActiveDrafts,
  saveActiveVersions,
  saveLocaisLocal,
  saveVersionStore,
  savedScenarios,
  savedVersionsFor,
  serialLabel,
  slotsFor,
  type MapDraft,
  type MapFile,
  type MapVersion,
  type DraftSpawn,
} from "./mapstore";

/** The real latest saved draft for a scenario, asked from the dev server directly rather than
 * trusted from latestSavedDraft's static snapshot — see the "Carregar mapa..."/"Abrir mapa
 * salvo" pickers' own comments for why that snapshot goes stale the instant any save happens
 * after this page loaded. Falls back to the stale snapshot only when there's no dev server to
 * ask (a built release). Every "reopen this saved map in the editor" entry point should use
 * this, not latestSavedDraft directly, or it silently reintroduces the same staleness. */
async function fetchLatestDraft(id: string): Promise<MapDraft | undefined> {
  try {
    const response = await fetch(`/__map-list?id=${encodeURIComponent(id)}`);
    const body = (await response.json()) as { ok?: boolean; files?: MapFile[] };
    if (!response.ok || !body.ok || !Array.isArray(body.files) || body.files.length === 0) throw new Error("lista indisponível");
    const latestFile = body.files.reduce((best: MapFile, f) => (f.serial > best.serial ? f : best));
    return latestFile.draft;
  } catch {
    return latestSavedDraft(id);
  }
}
import {
  activeSave,
  emptySave,
  formatStamp,
  hasAnySave,
  isSlotEmpty,
  loadBank,
  setMutedBank,
  SLOT_COUNT,
  slotProgress,
  writeSlot,
  selectSlot,
  dialogSeenKey,
} from "./save";
import type { Bag, BattleSnapshot, ClassId, DecorationPlacement, DialogAction, DialogTree, ElementalFxPlacement, EquipSlot, GameArt, GrowthLine, HudSnapshot, MapTimeOfDay, Mission, PoisonTier, PotionId, SaveBank, SaveData, ScreenId, SpellKind, Spawn, SpriteId, StatPointAllocation, StatPointAttribute, TerrainId, UnitPublic, WinCondition, WorldLocation } from "./types";
import { footprint, hexDist, hexNeighbors, key as hexKey } from "./pathfinding";
import { buildDecorOverlay, HEX_BLOCKED } from "./hexprops";

/** A map JSON write updates Vite's module list and can reload the app. This one-shot
 * snapshot restores the editor instead of sending the author to the title screen. */
const EDITOR_RESUME_KEY = "ember:editor-resume";
function readEditorResume(): MapDraft | null {
  try {
    const raw = window.sessionStorage.getItem(EDITOR_RESUME_KEY);
    if (!raw) return null;
    const draft = JSON.parse(raw) as MapDraft;
    return typeof draft?.id === "string" && Array.isArray(draft.tiles) ? draft : null;
  } catch {
    return null;
  }
}
function armEditorResume(draft: MapDraft): void {
  try { window.sessionStorage.setItem(EDITOR_RESUME_KEY, JSON.stringify(draft)); } catch { /* saving still proceeds */ }
}
function clearEditorResume(): void {
  try { window.sessionStorage.removeItem(EDITOR_RESUME_KEY); } catch { /* storage is optional */ }
}
/** Weapons belong to the party. Equipping one moves it from the previous wielder; only
 * potions and lockpicks stay in the individual bags. */
function equipSharedWeapon(save: SaveData, hero: string, weaponId: string): SaveData | null {
  if (save.weapons[weaponId] == null) return null;
  const classId = save.promotions[hero] ?? MAP_STATUS_CLASS[hero];
  if (!classId || !WEAPONS[weaponId]?.usableBy.includes(classId)) return null;
  const equipped = { ...save.equipped };
  for (const [owner, id] of Object.entries(equipped)) {
    if (owner !== hero && id === weaponId) delete equipped[owner];
  }
  equipped[hero] = weaponId;
  return { ...save, equipped };
}

/** Equipment is a physical party pool: select a reserve piece or transfer one from another
 * hero. Consumable bags are intentionally not part of this function. */
function equipSharedItem(save: SaveData, hero: string, slot: EquipSlot, itemId: string): SaveData | null {
  const item = EQUIPMENT[itemId];
  if (!item || !equipmentFitsSlot(item, slot)) return null;
  const reserve = save.looseEquipment[itemId] ?? 0;
  const current = save.equipment[hero]?.[slot];
  if (current === itemId) return save;
  // Prefer a spare in the stash so two copies of the same ring can fill both fingers.
  // Only pull the piece off another slot when there is no reserve left.
  const source =
    reserve > 0
      ? undefined
      : Object.entries(save.equipment)
          .flatMap(([owner, slots]) => (Object.entries(slots) as [EquipSlot, string][]).map(([usedSlot, id]) => ({ owner, usedSlot, id })))
          .find((entry) => entry.id === itemId && !(entry.owner === hero && entry.usedSlot === slot));
  if (!source && reserve <= 0) return null;

  const equipment = Object.fromEntries(Object.entries(save.equipment).map(([owner, slots]) => [owner, { ...slots }])) as SaveData["equipment"];
  const looseEquipment = { ...save.looseEquipment };
  if (source) delete equipment[source.owner]![source.usedSlot];
  else if (reserve === 1) delete looseEquipment[itemId];
  else looseEquipment[itemId] = reserve - 1;
  if (current) looseEquipment[current] = (looseEquipment[current] ?? 0) + 1;
  equipment[hero] = { ...(equipment[hero] ?? {}), [slot]: itemId };
  return { ...save, equipment, looseEquipment };
}

function unequipSharedItem(save: SaveData, hero: string, slot: EquipSlot): SaveData {
  const current = save.equipment[hero]?.[slot];
  if (!current) return save;
  const equipment = Object.fromEntries(Object.entries(save.equipment).map(([owner, slots]) => [owner, { ...slots }])) as SaveData["equipment"];
  const looseEquipment = { ...save.looseEquipment };
  delete equipment[hero]![slot];
  looseEquipment[current] = (looseEquipment[current] ?? 0) + 1;
  return { ...save, equipment, looseEquipment };
}

function unequipSharedWeapon(save: SaveData, hero: string): SaveData {
  if (!save.equipped[hero]) return save;
  const equipped = { ...save.equipped };
  delete equipped[hero];
  return { ...save, equipped };
}

/** Discards one owned-but-unequipped weapon for good. Only ever offered on a weapon the
 * "Itens da party" grid already shows — which itself only lists weapons nobody currently
 * has equipped — so this never needs to touch save.equipped. */
function discardSharedWeapon(save: SaveData, weaponId: string): SaveData {
  if (save.weapons[weaponId] == null) return save;
  const weapons = { ...save.weapons };
  delete weapons[weaponId];
  return { ...save, weapons };
}

/** Discards one spare copy of a piece of equipment from the party's shared stash. Only the
 * loose pool, never a copy someone currently has on — see the "Jogar Fora" gate in
 * InventoryScreens.tsx, which only enables when looseEquipment[itemId] > 0. */
function discardSharedEquipment(save: SaveData, itemId: string): SaveData {
  const reserve = save.looseEquipment[itemId] ?? 0;
  if (reserve <= 0) return save;
  const looseEquipment = { ...save.looseEquipment };
  if (reserve <= 1) delete looseEquipment[itemId];
  else looseEquipment[itemId] = reserve - 1;
  return { ...save, looseEquipment };
}

/** Discards one ration from the party's shared stock. */
function discardRation(save: SaveData): SaveData {
  if (save.rations <= 0) return save;
  return { ...save, rations: save.rations - 1 };
}

/** Discards one potion (or one gazua) from a specific hero's personal bag. */
function discardBagItem(save: SaveData, hero: string, kind: keyof Bag): SaveData {
  const bag = save.bags[hero];
  if (!bag || (bag[kind] ?? 0) <= 0) return save;
  return { ...save, bags: { ...save.bags, [hero]: { ...bag, [kind]: bag[kind] - 1 } } };
}

/** A hero's current max HP, gear and hunger included — the same formula the map's own
 * status sheet uses (see mapStatusUnit), needed here too so an out-of-battle heal potion
 * caps at the same ceiling the status sheet already shows. */
function heroMaxHp(save: SaveData, hero: string): number {
  const classId = save.promotions[hero] ?? MAP_STATUS_CLASS[hero] ?? "swordsman";
  const stats = statsFor(classId, save.levels[hero] ?? 1);
  const gearBonus = gearStatBonus(Object.values(save.equipment[hero] ?? {}), save.equipped[hero], classId);
  const hungerKeep = 1 - hungerPenaltyFor(save.hungerStreak);
  return Math.round((stats.hp + gearBonus.hp) * hungerKeep);
}

/** "Usar" a potion outside of battle — there is no live Unit to apply it to, so this
 * mirrors BattleEngine.applyPotion's heal/mana branches directly against SaveData. Disease
 * potions clear the persistent illness that can be contracted during overworld travel. */
function useHeroPotion(save: SaveData, hero: string, kind: PotionId): SaveData {
  const bag = save.bags[hero];
  if (!bag || (bag[kind] ?? 0) <= 0) return save;
  const def = POTIONS[kind];
  if (def.effect === "mana") {
    const classId = save.promotions[hero] ?? MAP_STATUS_CLASS[hero] ?? "swordsman";
    const level = save.levels[hero] ?? 1;
    const spent = { ...(save.spellUses[hero] ?? {}) };
    let restoredAny = false;
    for (let t = 1; t <= 10; t++) {
      const tk = tierKey(t as SpellTier);
      if (tierUses(classId, t as SpellTier, level) <= 0) continue;
      const cur = spent[tk] ?? 0;
      if (cur <= 0) continue;
      const next = Math.max(0, cur - (def.manaRestore ?? 0));
      if (next !== cur) restoredAny = true;
      spent[tk] = next;
    }
    if (!restoredAny) return save;
    return { ...save, bags: { ...save.bags, [hero]: { ...bag, [kind]: bag[kind] - 1 } }, spellUses: { ...save.spellUses, [hero]: spent } };
  }
  if (def.effect === "disease") {
    if (!save.heroDiseases[hero] && !save.heroPoisons[hero]) return save;
    const heroDiseases = { ...save.heroDiseases };
    delete heroDiseases[hero];
    const heroPoisons = { ...save.heroPoisons };
    delete heroPoisons[hero];
    const nextHealing = rollSkillGain(skillValue(save.heroSkills, hero, "healing"), Math.random);
    const heroSkills = nextHealing === null ? save.heroSkills : { ...save.heroSkills, [hero]: { ...save.heroSkills?.[hero], healing: nextHealing } };
    return { ...save, heroSkills, bags: { ...save.bags, [hero]: { ...bag, [kind]: bag[kind] - 1 } }, heroDiseases, heroPoisons };
  }
  const maxHp = heroMaxHp(save, hero);
  const current = save.unitHp[hero] ?? maxHp;
  if (current >= maxHp) return save;
  const gained = Math.min(healingAmount(rollPotion(kind, Math.random), skillValue(save.heroSkills, hero, "healing")), maxHp - current);
  if (gained <= 0) return save;
  const nextHealing = rollSkillGain(skillValue(save.heroSkills, hero, "healing"), Math.random);
  const heroSkills = nextHealing === null ? save.heroSkills : { ...save.heroSkills, [hero]: { ...save.heroSkills?.[hero], healing: nextHealing } };
  return { ...save, heroSkills, unitHp: { ...save.unitHp, [hero]: current + gained }, bags: { ...save.bags, [hero]: { ...bag, [kind]: bag[kind] - 1 } } };
}
function hudBlank(): HudSnapshot {
  return {
    phase: "player",
    banner: null,
    selected: null,
    hoveredUnit: null,
    terrain: null,
    mode: "idle",
    canAttack: false,
    offHandKind: null,
    canLockpick: false,
    forecast: null,
    turn: 1,
    objective: "",
    missionTitle: "",
    playerAlive: 0,
    enemyAlive: 0,
    busy: false,
    canCancelMovement: false,
    result: null,
    winAvailable: false,
    activeExit: null,
    canUndoMove: false,
    targetPrompt: null,
    zoom: 1,
    speedMode: "normal",
    tip: null,
    inspected: null,
    pendingFoe: null,
    spellReady: false,
    spellArmed: false,
    spellHitChance: null,
    spellKind: null,
    turnQueue: [],
    log: [],
    chestLoot: null,
    pendingDialog: null,
  };
}

/** The inn only acts as a rest stop after its own chapter has been completed. */
function innUnlocked(completed: string[]): boolean {
  return completed.includes("estalagem");
}

/** How a mission currently shows on the map (see progression.ts): a gate can hide it, show it
 * locked, or leave it open. Absent means no gating at all. */
type MissionAccessFn = (missionId: string) => MissionAccess;

/** What the save alone cannot tell progression conditions: owned gear and who is in the party. */
function progressionExtras(save: SaveData): ProgressExtras {
  const items = [
    ...Object.keys(save.weapons ?? {}),
    ...Object.keys(save.looseEquipment ?? {}),
    ...Object.values(save.equipment ?? {}).flatMap((slots) => Object.values(slots).filter((id): id is string => typeof id === "string")),
  ];
  const party = ["Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"].filter((name) => heroRecruited(name, save.completed, save.flags));
  return { items, party };
}

/** The player advances through a location chapter-by-chapter. A new location becomes
 * available only when every chapter in the prior populated location is complete. Missions
 * governed by a progression gate (progression.json) sit outside that chain: they open through
 * their own quest/chapter conditions and never block the next location. */
function previousPopulatedLocation(location: WorldLocation, locations: WorldLocation[]): WorldLocation | null {
  const at = locations.findIndex((candidate) => candidate.id === location.id);
  for (let i = at - 1; i >= 0; i -= 1) {
    const previous = locations[i]!;
    if (missionsForLocation(previous).some((mission) => !isGatedMission(mission.id))) return previous;
  }
  return null;
}

function lockedMission(
  id: string,
  completed: string[],
  test: boolean,
  locations: WorldLocation[],
  fallbackOrder: string[],
  access?: MissionAccessFn,
): boolean {
  if (test) return false;
  if (completed.includes(id)) return true;
  if (access && isGatedMission(id)) return access(id) !== "available";

  const location = locations.find((candidate) => candidate.missionIds.includes(id));
  if (location?.openAccess) return false;
  if (!location) {
    const at = fallbackOrder.indexOf(id);
    return at < 0 || (at > 0 && !fallbackOrder.slice(0, at).every((previousId) => completed.includes(previousId)));
  }

  const ids = missionsForLocation(location).map((mission) => mission.id).filter((missionId) => !isGatedMission(missionId));
  const at = ids.indexOf(id);
  if (at < 0) return true;
  if (at > 0) return !ids.slice(0, at).every((previousId) => completed.includes(previousId));

  const previousLocation = previousPopulatedLocation(location, locations);
  return previousLocation !== null && !missionsForLocation(previousLocation).filter((mission) => !isGatedMission(mission.id)).every((mission) => completed.includes(mission.id));
}

function missionStatus(
  id: string,
  completed: string[],
  test: boolean,
  locations: WorldLocation[],
  fallbackOrder: string[],
  access?: MissionAccessFn,
): "locked" | "available" | "done" {
  if (completed.includes(id)) return "done";
  return lockedMission(id, completed, test, locations, fallbackOrder, access) ? "locked" : "available";
}

function locationStatus(
  location: WorldLocation,
  completed: string[],
  test: boolean,
  locations: WorldLocation[],
  access?: MissionAccessFn,
): "locked" | "available" | "done" {
  // Hidden missions are not on the map, so they never count toward what the location holds.
  const missions = missionsForLocation(location).filter((mission) => !access || test || access(mission.id) !== "hidden");
  const pending = missions.filter((mission) => !completed.includes(mission.id));
  // Debug mode opens every location, including ones with no missions assigned yet (the new
  // areas); the campaign keeps its chapter gating.
  if (pending.length === 0) return missions.length > 0 ? "done" : test ? "available" : "locked";
  const order = missionsForLocation(location).map((mission) => mission.id);
  return pending.some((mission) => missionStatus(mission.id, completed, test, locations, order, access) !== "locked") ? "available" : "locked";
}

const BRIEF_ART: Record<string, string> = {
  farmlands: "/game/assets/brief-farmlands.jpg",
  vau: "/game/assets/brief-vau.jpg",
  bosque: "/game/assets/brief-bosque.jpg?v=2",
  aldeia: "/game/assets/brief-aldeia.jpg",
  muralha: "/game/assets/brief-muralha.jpg",
  fortaleza: "/game/assets/brief-fortaleza.jpg",
  templo: "/game/assets/brief-templo.jpg",
  cripta: "/game/assets/brief-cripta.jpg",
  estalagem: "/game/assets/brief-estalagem.jpg",
  colina: "/game/assets/brief-colina.jpg",
  passagem: "/game/assets/brief-passagem.jpg?v=2",
  "watchtower-gate-floor": "/game/assets/brief-watchtower.jpg",
  vertente: "/game/assets/brief-vertente.jpg?v=2",
  portao: "/game/assets/brief-portao.jpg",
  profundezas: "/game/assets/profundezas-bg.jpg?v=2",
  thebridge: "/game/assets/brief-thebridge.jpg?v=2",
  "wisp-forest": "/game/assets/brief-wisp-forest.jpg",
  "wisp-forest-2": "/game/assets/brief-wisp-forest.jpg",
  "wisp-forest-crossing": "/game/assets/brief-wisp-forest.jpg",
  "wisp-forest-crossing-boss": "/game/assets/brief-wisp-forest.jpg",
  // Original file names kept as supplied, so the paths are percent-encoded.
  "cemiterio-esquecidos": encodeURI("/game/assets/Xemitery Briefing.jpg"),
  "cemiterio-esquecidos-cripta-2": encodeURI("/game/assets/2Cemitério dos Esquecidos — Câmara Profunda✕.jpg"),
  "misty-cave-dungeon": encodeURI("/game/assets/As Profundezas EnevoadasBrief.jpg"),
  "cemiterio-esquecidos-cripta": encodeURI("/game/assets/Andar 3Cemitério dos Esquecidos — Cripta✕.jpg"),
  "cemiterio-esquecidos-mausoleu": encodeURI("/game/assets/Andar 4Cemitério dos Esquecidos — Mausoléu✕.jpg"),
  "cemiterio-esquecidos-ruinas": encodeURI("/game/assets/Andar 5Cemitério dos Esquecidos — Ruínas Submersas.jpg"),
  "frozen-tundra-crossing": "/game/assets/brief-frozen-tundra.jpg",
};

function briefArt(id: string): string | null {
  return BRIEF_ART[id] ?? null;
}


// Carried-bag icon follows the waist pouch equipped on that hero (small / large / satchel).
const BAG_ICON = pouchIcon(null);

type SlotAction = { kind: "spell"; spell: SpellKind } | { kind: "potion"; potion: PotionId };
const HOTBAR_SLOTS = 12;
/** Modo teste: Ember "infinito" pra testar compras/upgrades sem travar em custo. */
const TEST_EMBER = 900000;
const ALL_POTIONS: PotionId[] = ["weak", "mid", "potent", "disease", "manaSmall", "manaMid", "manaLarge"];
const HOTBAR_KEY = "ember-hotbar-v1";

// Prestige-only spells a promoted class adds on top of whatever its base class already
// granted (see classSpells below) — hybrid, nothing lost at PROMOTE_LEVEL. Second Wind isn't
// here: it's a passive the engine triggers itself from startOfTurnEffects (see SECOND_WIND in
// data.ts), never a hotbar cast — it still spends a tier-3 use through the same accounting,
// just automatically instead of by the player picking a slot.
const PRESTIGE_SPELLS: Partial<Record<ClassId, SpellKind[]>> = {
  paladin: ["cureLight", "auraOfProtection", "divineWrath"],
  heavyKnight: ["shoulderSmash", "intimidatingPresence", "stampede"],
  elementalist: ["lightningTier3"],
};

function classSpells(classId: ClassId, level = Number.POSITIVE_INFINITY, heroName?: string): SpellKind[] {
  // Promoted classes keep everything the base class already granted (hybrid, nothing
  // lost at PROMOTE_LEVEL), plus their own prestige-only spells from PRESTIGE_SPELLS.
  const base = ((): SpellKind[] => {
    switch (rulesClass(classId)) {
      case "swordsman":
        return ["doubleStrike", "provoke", "bullRush", "cleave", "shieldBash", "executionerStrike"];
      case "cultistV2": return ["frost", "magicMissile", "lightning"];
      case "mage":
        return ["magicMissile", "poisonBreath", "frost", "lightning", "fireball", "iceStorm", "causticVenom", "warp"];
      case "conjurer":
        // Summon Swarm (tiers 3-4) joins this list as it's built — see SPELL_TIER for the
        // intended tier assignment. Phantasmal Force is tier 1's own second spell (see
        // PHANTASMAL_FORCE_UNLOCK_LEVEL — it shares tier 1's pool with summonFamiliar but
        // isn't selectable/castable until level 2); summonFamiliar2 (Familiar Maior) is the
        // same deal at tier 2 (shares that tier's pool of uses with webOfDreams).
        // summonZombieDog: tier 5 here for testing — meant to become a Necromancer tier 6 spell.
        return ["summonFamiliar", "phantasmalForce", "webOfDreams", "summonFamiliar2", "summonFamiliar4", "summonFamiliar3", "summonZombieDog"];
      case "familiar":
        // Familiar's own hotbar, once summoned — Magic Missile is its only action beyond a
        // plain attack (see FAMILIAR_SPELL/familiarMagicMissileCharges).
        return ["magicMissile"];
      case "familiar2":
        // Familiar Maior's own hotbar — Magic Missile plus its own second spell, Toque
        // Vampírico (see LIFE_DRAIN/familiarLifeDrainCharges), each with an independent
        // charge pool.
        return ["magicMissile", "lifeDrain"];
      case "familiar4":
        // Familiar Radiante has three Shock casts plus Life Drain.
        return ["shock", "lifeDrain"];
      case "familiar3":
        // The Big Guy's own hotbar, once summoned — its only action beyond a plain attack.
        return ["fireball"];
      case "zombieDog":
        // Cão Zumbi's own hotbar: Veneno Menor, twice per battle (FAMILIAR_SPELL charges).
        return ["minorVenom"];
      case "archer":
        return ["longShot", "piercing", "bloodyShot", "multiShot"];
      case "healer":
        return ["cureMinor", "bless", "cureWounds", "burningHands", "cureDisease", "turnUndead", "createFoodAndWater"];
      case "lancer":
      case "aldric":
        return ["piercingThrust", "sweep", "trip"];
      default:
        return [];
    }
  })();
  const salazarOnly: SpellKind[] = heroName === "Salazar" && rulesClass(classId) === "healer" ? ["divineBolt"] : [];
  const spells = [...base, ...salazarOnly, ...(PRESTIGE_SPELLS[classId] ?? [])].filter((spell) =>
    (spell !== "frost" || classId === "cultistV2" || level >= FROST.unlockLevel) &&
    (spell !== "bullRush" || level >= BULL_RUSH_UNLOCK_LEVEL) &&
    (spell !== "bless" || level >= BLESS.unlockLevel) &&
    (spell !== "poisonBreath" || level >= POISON_BREATH.unlockLevel) &&
    (spell !== "burningHands" || level >= 5) &&
    (spell !== "bloodyShot" || level >= BLOODY_SHOT.unlockLevel) &&
    (spell !== "provoke" || level >= PROVOKE.unlockLevel) &&
    (spell !== "phantasmalForce" || level >= PHANTASMAL_FORCE_UNLOCK_LEVEL) &&
    (spell !== "summonFamiliar2" || level >= SUMMON_FAMILIAR2_UNLOCK_LEVEL) &&
    (spell !== "iceStorm" || level >= ICE_STORM.unlockLevel) &&
    (spell !== "warp" || level >= 7) &&
    (["familiar", "familiar2", "familiar3", "familiar4", "zombieDog", "cultistV2"].includes(classId) || !spellTier(spell) || tierUses(classId, spellTier(spell)!, level) > 0),
  );
  return spells;
}

function defaultHotbarSpells(classId: ClassId, level = Number.POSITIVE_INFINITY, heroName?: string): SpellKind[] {
  // Create Food and Water is a field utility. Keep it available in the spell picker, but
  // leave it off the default battle hotbar so players can choose it there if they want.
  return classSpells(classId, level, heroName).filter((spell) => spell !== "createFoodAndWater");
}

function defaultSlots(classId: ClassId, level = Number.POSITIVE_INFINITY, heroName?: string): (SlotAction | null)[] {
  const combined: SlotAction[] = [
    ...defaultHotbarSpells(classId, level, heroName).map((spell): SlotAction => ({ kind: "spell", spell })),
    ...ALL_POTIONS.map((potion): SlotAction => ({ kind: "potion", potion })),
  ];
  const slots: (SlotAction | null)[] = combined.slice(0, HOTBAR_SLOTS);
  while (slots.length < HOTBAR_SLOTS) slots.push(null);
  return slots;
}

function loadHotbars(): Record<string, (SlotAction | null)[]> {
  try {
    const raw = window.localStorage.getItem(HOTBAR_KEY);
    return raw ? JSON.parse(raw) : {};
  } catch {
    return {};
  }
}

function saveHotbars(bars: Record<string, (SlotAction | null)[]>) {
  try {
    window.localStorage.setItem(HOTBAR_KEY, JSON.stringify(bars));
  } catch {
    // localStorage unavailable — hotbar just won't persist across reloads
  }
}

function slotIcon(action: SlotAction): string {
  if (action.kind === "potion") return `/game/icons/potion-${action.potion}.png?v=ds2`;
  switch (action.spell) {
    case "doubleStrike":
      return spellIcon("cleave-crossed-blades");
    case "cleave":
      return spellIcon("cleave");
    case "fireball":
      return spellIcon("fireball");
    case "frost": return spellIcon("frost");
    case "iceStorm":
      return spellIcon("ice-storm");
    case "causticVenom":
    case "minorVenom":
      return spellIcon("caustic-venom");
    case "divineBolt":
      return spellIcon("divine-bolt");
    case "lightning":
      return spellIcon("lightning");
    case "lightningTier3":
      return spellIcon("lightning");
    case "shock":
      return spellIcon("lightning");
    case "magicMissile":
    case "magicMissileV2":
      return spellIcon("magic-missile");
    case "longShot":
      return spellIcon("long-shot");
    case "bloodyShot":
      return spellIcon("bloody-shot");
    case "piercing":
      return spellIcon("piercing");
    case "cureMinor":
      return spellIcon("cure-minor");
    case "cureWounds":
      return spellIcon("cure-wounds");
    case "cureDisease":
      return spellIcon("cure-disease");
    case "piercingThrust":
      return spellIcon("piercing-thrust");
    case "sweep":
      return spellIcon("sweep");
    case "trip":
      return spellIcon("trip");
    case "summonFamiliar":
      return spellIcon("summon-familiar");
    // No dedicated art yet — reuses Magic Missile's own icon, closest in theme to a single
    // ranged magic bolt.
    case "phantasmalForce":
    case "fantomForce":
      return spellIcon("phantasmal-force");
    // No dedicated art yet for the tier-2/3 summons — each reuses the same familiar icon.
    case "summonFamiliar2":
      return spellIcon("summon-familiar2");
    case "summonFamiliar3":
      return spellIcon("summon-familiar3");
    case "summonFamiliar4":
      return spellIcon("summon-familiar4");
    case "summonZombieDog":
      return spellIcon("summon-zombie-dog");
    case "webOfDreams":
      return spellIcon("web-of-dreams");
    case "warp": return spellIcon("warp");
    case "lifeDrain":
      return spellIcon("life-drain");
    // secondWind is never actually shown (see PRESTIGE_SPELLS) but the switch stays exhaustive.
    case "multiShot":
      return spellIcon("multi-shot");
    case "secondWind":
    case "cureLight":
      return spellIcon("cure-light");
    case "bless":
      return spellIcon("bless");
    case "auraOfProtection":
      return spellIcon("aura-of-protection");
    case "intimidatingPresence":
      return spellIcon("intimidating-presence");
    case "divineWrath":
      return spellIcon("divine-wrath");
    case "shoulderSmash":
      return spellIcon("shoulder-smash");
    case "stampede":
      return spellIcon("stampede");
    case "bullRush":
      return spellIcon("bull-rush");
    case "executionerStrike":
      return spellIcon("executioner-strike");
    case "provoke":
      return spellIcon("provoke");
    case "shieldBash":
      return spellIcon("shield-bash");
    case "poisonBreath":
      return spellIcon("poison-breath");
    // Carnivorous Plant's AI-only swipe; never on a hotbar, listed to keep the switch exhaustive.
    case "tendrilSwipe":
      return spellIcon("sweep");
    case "burningHands":
      return spellIcon("burning-hands");
    case "turnUndead":
      return spellIcon("turn-undead");
    case "createFoodAndWater":
      return spellIcon("create-food-and-water");
  }
}

function slotLabel(action: SlotAction): string {
  if (action.kind === "potion") return potionLabel(action.potion);
  switch (action.spell) {
    case "doubleStrike":
      return DOUBLE_STRIKE.name;
    case "cleave":
      return CLEAVE.name;
    case "fireball":
      return FIREBALL.name;
    case "frost": return FROST.name;
    case "iceStorm":
      return ICE_STORM.name;
    case "causticVenom":
      return CAUSTIC_VENOM.name;
    case "divineBolt":
      return "Divine Bolt";
    case "minorVenom":
      return MINOR_VENOM.name;
    case "lightning":
      return LIGHTNING.name;
    case "lightningTier3":
      return LIGHTNING_T3.name;
    case "shock":
      return SHOCK.name;
    case "magicMissile":
    case "magicMissileV2":
      return MAGIC_MISSILE.name;
    case "longShot":
      return LONG_SHOT.name;
    case "bloodyShot":
      return BLOODY_SHOT.name;
    case "piercing":
      return PIERCING.name;
    case "cureMinor":
      return CURES.cureMinor.name;
    case "bless":
      return BLESS.name;
    case "cureWounds":
      return CURES.cureWounds.name;
    case "cureDisease":
      return CURE_DISEASE.name;
    case "piercingThrust":
      return PIERCING_THRUST.name;
    case "sweep":
      return SWEEP.name;
    case "trip":
      return TRIP.name;
    case "summonFamiliar":
      return SUMMON_FAMILIAR.name;
    case "phantasmalForce":
      return PHANTASMAL_FORCE.name;
    case "fantomForce":
      return FANTOM_FORCE.name;
    case "summonFamiliar2":
      return SUMMON_FAMILIAR2.name;
    case "summonFamiliar3":
      return SUMMON_FAMILIAR3.name;
    case "summonFamiliar4":
      return SUMMON_FAMILIAR4.name;
    case "summonZombieDog":
      return SUMMON_ZOMBIE_DOG.name;
    case "webOfDreams":
      return WEB_OF_DREAMS.name;
    case "warp": return "Warp";
    case "lifeDrain":
      return LIFE_DRAIN.name;
    case "multiShot":
      return MULTI_SHOT.name;
    case "secondWind":
      return SECOND_WIND.name;
    case "cureLight":
      return CURES.cureLight.name;
    case "auraOfProtection":
      return AURA_OF_PROTECTION.name;
    case "intimidatingPresence":
      return INTIMIDATING_PRESENCE.name;
    case "divineWrath":
      return DIVINE_WRATH.name;
    case "shoulderSmash":
      return SHOULDER_SMASH.name;
    case "stampede":
      return STAMPEDE.name;
    case "bullRush":
      return BULL_RUSH.name;
    case "executionerStrike":
      return EXECUTIONER_STRIKE.name;
    case "provoke":
      return PROVOKE.name;
    case "shieldBash":
      return SHIELD_BASH.name;
    case "poisonBreath":
      return POISON_BREATH.name;
    case "tendrilSwipe":
      return "Chicote de Gavinhas";
    case "burningHands":
      return BURNING_HANDS.name;
    case "turnUndead":
      return TURN_UNDEAD.name;
    case "createFoodAndWater":
      return CREATE_FOOD_AND_WATER.name;
  }
}

function slotTooltip(action: SlotAction): string {
  if (action.kind === "potion") return potionTooltip(action.potion);
  return slotLabel(action);
}

function slotCount(action: SlotAction, unit: UnitPublic): number {
  if (action.kind === "potion") return unit.bag[action.potion];
  // A familiar's own spell (Fireball for Familiar Titã, Magic Missile for Familiar/Familiar
  // Maior) draws from its own per-summon spellCharges, never the normal tier slot table
  // (which stays all-zero for a summoned familiar) — see familiarSpellRemaining in engine.ts,
  // which this has to agree with or the badge/disabled-state lies about what a click will
  // actually do.
  if (FAMILIAR_SPELL[unit.classId] === action.spell) return unit.spellCharges ?? 0;
  // Familiar Maior's own second spell — its own dedicated lifeDrainCharges pool, never the
  // FAMILIAR_SPELL/spellCharges pair above (that's reserved for the one own-spell every other
  // familiar tier has) — see the lifeDrain guard in BattleEngine.startLifeDrain.
  if (action.spell === "lifeDrain" && (unit.classId === "familiar2" || unit.classId === "familiar4")) return unit.lifeDrainCharges ?? 0;
  // Both share their tier's pool of uses with another tier-1/2 spell, but each unlocks later
  // than that shared pool itself does (PHANTASMAL_FORCE_UNLOCK_LEVEL/SUMMON_FAMILIAR2_UNLOCK_
  // LEVEL) — showing the raw tier count here would read as castable before it actually is,
  // so the badge reads 0 until the caster's own level catches up, matching the guard in
  // BattleEngine.startPhantasmalForce/startSummonFamiliar2.
  if (action.spell === "phantasmalForce" && unit.level < PHANTASMAL_FORCE_UNLOCK_LEVEL) return 0;
  if (action.spell === "bless" && unit.level < BLESS.unlockLevel) return 0;
  if(action.spell==="frost"){if(unit.side==="enemy")return unit.frostCharges??0;if(unit.level<FROST.unlockLevel)return 0;}
  if (action.spell === "poisonBreath" && unit.level < POISON_BREATH.unlockLevel) return 0;
  if (action.spell === "summonFamiliar2" && unit.level < SUMMON_FAMILIAR2_UNLOCK_LEVEL) return 0;
  if (action.spell === "warp" && unit.level < 7) return 0;
  const tier = spellTier(action.spell);
  return tier ? unit.spells[tierKey(tier)] : 0;
}

const MAP_STATUS_CLASS: Record<string, ClassId> = { Kael: "swordsman", Neera: "archer", Voss: "mage", Salazar: "healer", Aldric: "aldric", Malrec: "conjurer" };

/** Adapts persistent campaign data to the exact UnitPublic contract consumed by the shared
 * battle status sheet. The sheet itself stays singular; only its data source changes. */
function mapStatusUnit(save: SaveData, hero: string): UnitPublic {
  const classId = save.promotions[hero] ?? MAP_STATUS_CLASS[hero] ?? "swordsman";
  const cls = CLASSES[classId];
  const level = save.levels[hero] ?? 1;
  const stats = statsFor(classId, level);
  const gear = save.equipment[hero] ?? {};
  const gearBonus = gearStatBonus(Object.values(gear), save.equipped[hero], classId);
  const emptySpells = { tier1: 0, tier2: 0, tier3: 0, tier4: 0, tier5: 0, tier6: 0, tier7: 0, tier8: 0, tier9: 0, tier10: 0 };
  // Same source the battle roster reads (see startBattle's hungerPenaltyPct) — this used to
  // be hardcoded to "never hungry" here, so the RPG map's own status sheet could never show
  // the condition even after many unfed days, only a live battle could.
  // A starvation streak sets how severe hunger would be, but this character is healthy
  // immediately after being fed even if another party member still needs food.
  const hungerPenaltyPct = fullness(save.heroHunger[hero]) <= 0 ? hungerPenaltyFor(save.hungerStreak) : 0;
  // The condition badge used to be the only sign of this — VIT/ATK/MAG/DEF/DEX themselves
  // still read at full value here, unlike the live battle roster (see spawnUnit's
  // hungerKeep), so the sheet warned about a penalty its own numbers never showed.
  const hungerKeep = 1 - hungerPenaltyPct;
  const diseaseKeep = save.heroDiseases[hero] ? 0.9 : 1;
  const maxHp = Math.round((stats.hp + gearBonus.hp) * hungerKeep);
  return {
    id: `map:${hero}`, name: hero, classId, className: cls.name, role: cls.role, side: "player", sprite: heroSpriteFor(hero, cls.sprite),
    hp: Math.min(maxHp, save.unitHp[hero] ?? maxHp), maxHp,
    atk: Math.round((stats.atk + gearBonus.atk) * hungerKeep * diseaseKeep),
    mag: Math.round((stats.mag + gearBonus.mag) * hungerKeep * diseaseKeep),
    def: Math.round((stats.def + gearBonus.def) * hungerKeep * diseaseKeep),
    dex: Math.round((stats.dex + gearBonus.dex) * hungerKeep * diseaseKeep),
    resistances: sumResistances(stats.resistances, gearBonus.resistances, skillResistances(save.heroSkills, hero)),
    weaponSkills: trainedWeaponSkills(save.heroSkills, hero, classId),
    healingSkill: skillValue(save.heroSkills, hero, "healing"),
    initiative: cls.init ?? 0, initiativeRoll: cls.init ?? 0, mov: Math.max(1, Math.round((stats.mov + gearBonus.mov) * diseaseKeep)), movLeft: Math.max(1, Math.round((stats.mov + gearBonus.mov) * diseaseKeep)), minRange: cls.minRange, maxRange: cls.maxRange,
    moved: false, acted: false, x: save.overworldPos.col, y: save.overworldPos.row, level, xp: save.xp[hero] ?? 0,
    bag: save.bags[hero] ?? { mid: 0, weak: 0, potent: 0, disease: 0, manaSmall: 0, manaMid: 0, manaLarge: 0, lockpick: 0 },
    spells: emptySpells, weaponId: save.equipped[hero] ?? null, weaponEnh: 0, size: cls.size, diseased: save.heroDiseases[hero] === true, poisoned: !!save.heroPoisons[hero], poisonTier: poisonTierOf(save.heroPoisons[hero]), bleeding: false, shock: null,
    hungry: hungerPenaltyPct > 0, hungerPct: Math.round(hungerPenaltyPct * 100), fullness: save.heroHunger[hero], stunned: false, crippled: false, offHandId: null, summoned: false, asleep: false, restrained: false,
    gear,
  };
}

/** Fold the live battle condition back into the campaign without dropping an illness on a
 * recruited hero who was not deployed in this particular mission. */
function mergeBattleDiseases(existing: Record<string, boolean>, engine: BattleEngine): Record<string, boolean> {
  const heroDiseases = { ...existing };
  for (const unit of engine.units) {
    if (unit.side !== "player" || unit.summoned) continue;
    if (unit.diseased) heroDiseases[unit.name] = true;
    else delete heroDiseases[unit.name];
  }
  return heroDiseases;
}

function mergeBattlePoisons(existing: Record<string, PoisonTier>, engine: BattleEngine): Record<string, PoisonTier> {
  const heroPoisons = { ...existing };
  for (const unit of engine.units) {
    if (unit.side !== "player" || unit.summoned) continue;
    if (unit.poisoned) heroPoisons[unit.name] = unit.poisonTier ?? "lesser";
    else delete heroPoisons[unit.name];
  }
  return heroPoisons;
}

function mergeBattlePoisonMagic(existing: Record<string, number> | undefined, engine: BattleEngine): Record<string, number> {
  const magic = { ...existing };
  for (const unit of engine.units) {
    if (unit.side !== "player" || unit.summoned) continue;
    if (unit.poisoned) magic[unit.name] = unit.poisonMag ?? 0;
    else delete magic[unit.name];
  }
  return magic;
}

/** Every familiar a conjurer can summon — preloaded as soon as a conjurer is in the party and
 * kept loaded (see partyHasConjurer in GameApp), so a summon never waits on art. */
const FAMILIAR_SPRITES: SpriteId[] = ["familiar", "familiar2", "familiar3", "familiar4", "zombieDog"];

/** Sprites a battle's own units use — loaded before its board opens (see startBattle). */
function battleSpriteIds(battle: BattleEngine): SpriteId[] {
  return battle.units.map((u) => u.sprite);
}

/** How long each battle-loading step took last time (ms), so the bar can weigh them. */
const BATTLE_LOAD_STEPS_KEY = "ember.battleLoadStepMs";
const BATTLE_LOAD_STEPS_FALLBACK = [1000, 4000, 4000];

/** The caption under the battle bar: which real step is running, with its own count. */
function battleLoadCaption({ step, done, total }: { step: number; done: number; total: number }): string {
  if (step === 0) return total ? `Carregando personagens · ${done}/${total}` : "Carregando personagens";
  if (step === 1) return total ? `Montando o campo · ${done}/${total}` : "Montando o campo";
  return total ? `Compilando efeitos · ${done}/${total}` : "Compilando efeitos";
}

/** Handoff before paint: the gameplay document never draws its legacy title. */
function TitlePageHandoff() {
  useLayoutEffect(() => { stopMusic(); returnToTitle(); }, []);
  return null;
}

export function GameApp() {
  useGamePreferences();
  const startMode = typeof window === "undefined" ? null : new URLSearchParams(window.location.search).get("start");
  const startOnMap = startMode === "map";
  const startNewCampaign = startMode === "new";
  const [resumeEditorDraft] = useState<MapDraft | null>(() => (typeof window === "undefined" ? null : readEditorResume()));
  const [screen, setScreen] = useState<ScreenId>(() => (startNewCampaign ? "boot" : startOnMap ? "overworldMap" : startMode === "test" ? "testMenu" : startMode === "continue" ? "battle" : resumeEditorDraft ? "mapEditor" : "title"));
  // Up from the instant a battle is requested until BattleCanvas reports "ember:battle-ready"
  // (art loaded, first frame drawn, every spell shader compiled and linked), so all of that
  // one-time work happens behind it instead of as stalls mid-fight. The timer is only a
  // safety net so the curtain can never get stuck.
  const [battleLoading, setBattleLoading] = useState(false);
  // The battle bar's real steps (see loadStages.ts): 0 sprites + decorations (files), 1 building
  // the 3D battlefield (its decoration images, from Battle3D), 2 compiling shaders (programs the
  // GPU has finished). Each step's share of the bar is the time it took last time.
  const [battleLoad, setBattleLoad] = useState({ step: 0, done: 0, total: 0 });
  const battleClock = useRef<StepClock | null>(null);
  const battleStepMs = useRef(rememberedStepMs(BATTLE_LOAD_STEPS_KEY, BATTLE_LOAD_STEPS_FALLBACK));
  const battleLoadShown = useRef("");
  const setBattleStep = useCallback((step: number, done: number, total: number) => {
    const clock = battleClock.current;
    if (!clock || step < clock.current) return;
    clock.enter(step);
    // The warm-up reports every frame: only a real change re-renders.
    const key = `${step}|${done}|${total}`;
    if (key === battleLoadShown.current) return;
    battleLoadShown.current = key;
    setBattleLoad({ step, done, total });
  }, []);
  const battleAssetProgress = useRef({
    sprites: { loaded: 0, total: 0 },
    decorations: { loaded: 0, total: 0 },
    terrain: { loaded: 0, total: 0 },
  });
  const reportBattleAssetProgress = useCallback((group: "sprites" | "decorations" | "terrain", loaded: number, total: number) => {
    battleAssetProgress.current[group] = { loaded, total };
    const groups = Object.values(battleAssetProgress.current);
    const loadedAssets = groups.reduce((sum, item) => sum + item.loaded, 0);
    const assetCount = groups.reduce((sum, item) => sum + item.total, 0);
    setBattleStep(0, loadedAssets, assetCount);
  }, [setBattleStep]);
  useEffect(() => {
    if (!battleLoading) return;
    const warm = (event: Event) => {
      const status = (event as CustomEvent<{ step: string; done: number; total: number }>).detail;
      setBattleStep(status.step === "shaders" ? 2 : 1, status.done, status.total);
    };
    const done = () => {
      const clock = battleClock.current;
      if (clock) rememberStepMs(BATTLE_LOAD_STEPS_KEY, clock.finish());
      battleClock.current = null;
      setBattleLoad({ step: BATTLE_LOAD_STEPS_FALLBACK.length, done: 1, total: 1 });
      setBattleLoading(false);
    };
    window.addEventListener("ember:battle-step", warm);
    window.addEventListener("ember:battle-ready", done);
    const safety = window.setTimeout(() => setBattleLoading(false), 20000);
    return () => {
      window.removeEventListener("ember:battle-step", warm);
      window.removeEventListener("ember:battle-ready", done);
      window.clearTimeout(safety);
    };
  }, [battleLoading, setBattleStep]);
  // The currently active map style. Normal campaigns persist their choice in SaveData;
  // test mode deliberately remains session-only.
  const [mapMode, setMapMode] = useState<"classic" | "rpg" | null>(() => (startOnMap ? "rpg" : null));
  const [overworldEvent, setOverworldEvent] = useState<OverworldEvent | null>(null);
  const [mapStatusHero, setMapStatusHero] = useState<string | null>(null);
  const [mapInventoryRequestHero, setMapInventoryRequestHero] = useState<string | null>(null);
  const [mapInventoryRequestView, setMapInventoryRequestView] = useState<"backpack" | "equipment">("backpack");
  const [bank, setBank] = useState<SaveBank>(() => (typeof window === "undefined" ? { version: 7, lastSlot: 0, muted: false, slots: [null, null, null, null, null] } : loadBank()));
  const save = activeSave(bank);
  const [art, setArt] = useState<GameArt | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [missionId, setMissionId] = useState<string | null>(null);
  const [wispForestNextMissionId, setWispForestNextMissionId] = useState<string | null>(null);
  const [engine, setEngine] = useState<BattleEngine | null>(null);
  /** Which Inn menu an NPC in the walkable Inn opened, while it's showing — leaving it goes
   * back to the walkable Inn (same engine, same spot) instead of out to the map. */
  const [innEntry, setInnEntry] = useState<DialogAction | null>(null);
  const [hud, setHud] = useState<HudSnapshot>(hudBlank);
  const [paused, setPaused] = useState(false);
  // Esc opens and closes the options menu in battle (right-click still cancels an action).
  useEffect(() => {
    if (screen !== "battle") return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== "Escape" || (e.target as HTMLElement | null)?.closest("input, textarea, select")) return;
      e.preventDefault();
      if (paused) { setSlotMode(null); setOverwrite(null); setPaused(false); resumeMusic(); }
      else { pauseMusic(); setPaused(true); }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [screen, paused]);
  const [companionConversationsOpen, setCompanionConversationsOpen] = useState(false);
  // The mission's outro dialog (see hud.result effect below) — opens once, right when
  // victory is confirmed, and never reopens after being closed even though hud.result
  // stays "victory" for the rest of the battle.
  const [outroDialogOpen, setOutroDialogOpen] = useState(false);
  const outroDialogShownRef = useRef(false);
  const [help, setHelp] = useState(false);
  const [muted, setMutedUi] = useState(() => (typeof window === "undefined" ? false : loadBank().muted));
  const muteReady = useRef(false);
  const [lastGrowth, setLastGrowth] = useState<GrowthLine[] | null>(null);
  const [lastLoot, setLastLoot] = useState<string[]>([]);
  const [pendingPromotions, setPendingPromotions] = useState<{ name: string; options: [ClassId, ClassId] }[]>([]);
  // Set right after a victory that leaves more chapters at the same location — the world
  // map opens with that location's chapter list already popped open instead of the bare
  // map, so a multi-mission location plays as one continuous series of combats.
  const [openLocationOnMap, setOpenLocationOnMap] = useState<string | null>(null);
  // A Locais save made in an earlier session lives in localStorage (see saveLocaisLocal in
  // mapstore.ts) — read it here too, not just in the editor's own order/slots/locationOrder
  // state below, so the actual world map reflects it on a fresh load/reopen, not only while
  // the editor itself is open and its "ember:locations-saved" event has fired this session.
  const [campaignLocations, setCampaignLocations] = useState<WorldLocation[]>(() => {
    const local = loadLocaisLocal();
    return local ? locationsForOrder(local.order, local.locationOrder, local.submaps, local.knownMissionIds) : ALL_LOCATIONS;
  });
  const [campaignMissionRevision, setCampaignMissionRevision] = useState(0);
  useEffect(() => {
    const refreshLocations = () => {
      const local = loadLocaisLocal();
      setCampaignLocations(local ? locationsForOrder(local.order, local.locationOrder, local.submaps, local.knownMissionIds) : ALL_LOCATIONS);
    };
    const onStorage = (event: StorageEvent) => {
      if (event.key === LOCAIS_LOCAL_KEY || event.key === null) refreshLocations();
    };
    refreshLocations();
    window.addEventListener("ember:locations-saved", refreshLocations);
    window.addEventListener("storage", onStorage);
    return () => {
      window.removeEventListener("ember:locations-saved", refreshLocations);
      window.removeEventListener("storage", onStorage);
    };
  }, [ALL_LOCATIONS]);
  useEffect(() => {
    const refreshMissions = () => setCampaignMissionRevision((revision) => revision + 1);
    window.addEventListener("ember:missions-saved", refreshMissions);
    return () => window.removeEventListener("ember:missions-saved", refreshMissions);
  }, []);
  const campaignMissions = useMemo(() => {
    const used = new Set<string>();
    const ordered = campaignLocations.flatMap((loc) =>
      loc.missionIds.flatMap((id) => {
        const mission = missionById(id);
        if (!mission || used.has(mission.id)) return [];
        used.add(mission.id);
        return [mission];
      }),
    );
    return ordered;
  }, [campaignLocations, campaignMissionRevision]);
  const [testMode, setTestMode] = useState(false);
  const [testEmber, setTestEmber] = useState(TEST_EMBER);
  // Test mode's own overworld/Inn state (position, exploration, hunger, rations, gear) —
  // kept entirely separate from the real save so wandering the RPG map, eating, or
  // shopping in test mode can never write through to it. Reset to null on every fresh
  // "Modo Teste" entry (see onTest below), so a test session always starts back at the
  // western ford instead of resuming wherever a previous test session or the real
  // playthrough left off.
  const [testOverworld, setTestOverworld] = useState<SaveData | null>(null);
  const awardedRef = useRef<string | null>(null);
  // Each hero's level/XP when this battle began: the result screen's XP bar fills from here.
  // save.xp can't be used — spending a stat point or saving to a slot mid-battle writes the
  // live XP into it, which left the bar nothing to fill.
  const battleStartProgressRef = useRef<Record<string, { level: number; xp: number }>>({});
  // Bumped by every startBattle; a battle whose sprites finish loading after a newer one was
  // requested is dropped (see startBattle).
  const battleLoadRef = useRef(0);
  const combatStartRef = useRef<SaveData | null>(null);
  const resumeBattleRef = useRef<BattleSnapshot | null>(null);
  const [slotMode, setSlotMode] = useState<"new" | "continue" | "save" | "load" | null>(null);
  const [slotReturnScreen, setSlotReturnScreen] = useState<ScreenId>("title");
  const [overwrite, setOverwrite] = useState<number | null>(null);
  // The save/load slots opened from a battle sit on top of it instead of replacing it:
  // unmounting BattleScreen threw away the whole 3D battlefield, and coming back rebuilt it
  // from scratch — the multi-second freeze after every save.
  const battleUnderSlots = screen === "saveSlots" && slotReturnScreen === "battle";
  // Real save steps (snapshot, write, done) while a slot is being written; null otherwise.
  const [saveProgress, setSaveProgress] = useState<number | null>(null);
  const savingSlotRef = useRef(false);
  const loadingCurtain = useLoadingCurtain(battleUnderSlots ? "battle" : screen);

  useEffect(() => {
    let alive = true;
    loadGameArt()
      .then((a) => {
        if (alive) setArt(a);
      })
      .catch((err: Error) => {
        if (alive) setLoadError(err.message);
      });
    return () => {
      alive = false;
    };
  }, []);

  useEffect(() => {
    setMuted(muted);
    if (!muteReady.current) {
      muteReady.current = true;
      return;
    }
    setBank((b) => setMutedBank(b, muted));
  }, [muted]);

  useEffect(() => {
    const onVis = () => {
      if (document.visibilityState === "visible") resumeAudio();
    };
    document.addEventListener("visibilitychange", onVis);
    const disarm = installAudioUnlock();
    return () => {
      document.removeEventListener("visibilitychange", onVis);
      disarm();
    };
  }, []);

  const [customMission, setCustomMission] = useState<Mission | null>(null);
  /** The map open in the Map Editor, kept out here so a playtest — which unmounts that
   * screen — does not discard it. */
  const editorDraft = useRef<MapDraft | null>(resumeEditorDraft);
  const mission = customMission && customMission.id === missionId ? customMission : missionId ? missionById(missionId) : undefined;
  const hasProgress = hasAnySave(bank);

  useEffect(() => {
    if (resumeEditorDraft) clearEditorResume();
  }, [resumeEditorDraft]);

  const applySlot = (next: SaveBank) => {
    setBank(next);
    const rec = activeSave(next);
    combatStartRef.current = rec;
  };

  const persistCurrent = (data: SaveData, slot = bank.lastSlot) => {
    // Every save write settles progression first (chapter triggers, finished-quest effects).
    const next = writeSlot(bank, slot, { ...advanceProgression(data, progressionExtras(data)), muted });
    applySlot(next);
    return next;
  };

  const withLiveBattle = (data: SaveData): SaveData => {
    if (!engine || !missionId) return data;
    return {
      ...data,
      pendingMission: missionId,
      battle: engine.captureSnapshot(),
      spellUses: { ...data.spellUses, ...engine.spentTiers() },
      bags: { ...data.bags, ...engine.remainingBags() },
      affinityScores: { ...engine.affinityScores },
      heroSkills: structuredClone(engine.heroSkills),
      unitHp: { ...data.unitHp, ...engine.battlePlayerHp() },
      heroHunger: { ...data.heroHunger, ...engine.battlePlayerHunger() },
    };
  };

  const enterFromSave = (rec: SaveData) => {
    setTestMode(false);
    setLastGrowth(null);
    setLastLoot([]);
    if (rec.battle && rec.pendingMission && missionById(rec.pendingMission)) {
      // Resume belongs to gameplay immediately, including the wait for base art.
      // Leaving the previous screen here exposed the title menu between loading overlays.
      resumeBattleRef.current = rec.battle;
      setMissionId(rec.pendingMission);
      setScreen("battle");
      return;
    }
    if (rec.pendingMission && missionById(rec.pendingMission)) {
      setMissionId(rec.pendingMission);
      setScreen("briefing");
      return;
    }
    setMissionId(null);
    goToMap();
  };

  /** Test mode's whole point is checking every hero's balance regardless of story progress,
   * but a mission's own playerSpawns only ever lists whichever heroes the story had actually
   * recruited by that point — no mission written before Aldric/Malrec join the party
   * includes them, so they silently never appeared in a test battle at all. Adds whichever
   * of the six are missing, each placed via the same nearest-free-cell BFS real spawns get
   * nudged onto for a hazard (see BattleEngine.nudgeOffHazard) — never a hardcoded offset
   * that could land on a wall, water, or another unit on a layout this never saw. */
  const TEST_PARTY_CLASS: Record<string, ClassId> = { Kael: "kaelFinal", Neera: "neera", Voss: "voss", Salazar: "salazar", Aldric: "aldric", Malrec: "conjurer" };
  function addAdditionalPartyHeroes(mission: Mission, roster: Record<string, ClassId>): Mission {
    const present = new Set(mission.playerSpawns.map((s) => s.name));
    const missing = Object.keys(roster).filter((name) => !present.has(name));
    if (missing.length === 0) return mission;
    const terrain = parseLayout(mission.layout);
    const occupied = new Set([...mission.playerSpawns, ...mission.enemySpawns, ...(mission.neutralSpawns ?? [])].map((s) => hexKey(s.x, s.y)));
    const anchor = mission.playerSpawns[0] ?? { x: 0, y: 0 };
    const added: Spawn[] = [];
    for (const name of missing) {
      const seen = new Set([hexKey(anchor.x, anchor.y)]);
      const q: { x: number; y: number }[] = [anchor];
      let placed: { x: number; y: number } | null = null;
      while (q.length && !placed) {
        const cur = q.shift()!;
        for (const n of hexNeighbors(cur.x, cur.y)) {
          if (n.x < 0 || n.y < 0 || n.x >= mission.cols || n.y >= mission.rows) continue;
          const k = hexKey(n.x, n.y);
          if (seen.has(k)) continue;
          seen.add(k);
          const terr = TERRAIN[terrain[n.y * mission.cols + n.x]];
          if (terr?.passable && !occupied.has(k)) {
            placed = n;
            break;
          }
          q.push(n);
        }
      }
      if (!placed) continue; // no free cell anywhere reachable — skip rather than overlap
      occupied.add(hexKey(placed.x, placed.y));
      added.push({ name, classId: roster[name]!, x: placed.x, y: placed.y });
    }
    return added.length > 0 ? { ...mission, playerSpawns: [...mission.playerSpawns, ...added] } : mission;
  }

  /** Taking a floor connector: the party arrives beside the connector on the new floor that
   * leads back to the floor it just left — where it came in — instead of on the map's authored
   * start hexes, which only make sense when entering from the world map. Cells are taken in
   * walking order (BFS over passable ground) out from that connector, skipping waypoint hexes,
   * blocking decorations, and every enemy/neutral body-type zone. If the floor has no matching
   * connector or not enough room around it, the authored spawns are kept. */
  function arriveAtConnector(mission: Mission, fromMissionId: string): Mission {
    const decorations = mission.decorations ?? [];
    const back = decorations.find((d) => isFloorConnector(d) && d.targetMapId === fromMissionId);
    if (!back || mission.playerSpawns.length === 0) return mission;
    const terrain = parseLayout(mission.layout);
    const overlay = buildDecorOverlay(decorations, mission.cols, mission.rows, placedBlockingFootprint, mission.terrainElevations);
    const waypointCells = new Set(decorations.filter((d) => DECORATIONS[d.id]?.exitKind).flatMap((d) => placedFootprint(d).map((f) => hexKey(d.x + f.dx, d.y + f.dy))));
    const bodyCells = new Set([...mission.enemySpawns, ...(mission.neutralSpawns ?? [])].flatMap((s) => {
      const cls = CLASSES[s.classId];
      return footprint({ x: s.x, y: s.y, size: cls?.size, footprintW: cls?.footprintW, footprintH: cls?.footprintH, footprintOffsets: cls?.footprintOffsets }).map((p) => hexKey(p.x, p.y));
    }));
    const walkable = (x: number, y: number) => {
      if (x < 0 || y < 0 || x >= mission.cols || y >= mission.rows) return false;
      const i = y * mission.cols + x;
      return !!TERRAIN[terrain[i]]?.passable && !((overlay[i] ?? 0) & HEX_BLOCKED);
    };
    const cells: { x: number; y: number }[] = [];
    const seen = new Set([hexKey(back.x, back.y)]);
    const q = [{ x: back.x, y: back.y }];
    while (q.length && cells.length < mission.playerSpawns.length) {
      const cur = q.shift()!;
      for (const n of hexNeighbors(cur.x, cur.y)) {
        const k = hexKey(n.x, n.y);
        if (seen.has(k) || !walkable(n.x, n.y)) continue;
        seen.add(k);
        q.push(n);
        if (!waypointCells.has(k) && !bodyCells.has(k) && cells.length < mission.playerSpawns.length) cells.push(n);
      }
    }
    if (cells.length < mission.playerSpawns.length) return mission;
    return { ...mission, playerSpawns: mission.playerSpawns.map((s, i) => ({ ...s, x: cells[i]!.x, y: cells[i]!.y })) };
  }
  function addMissingTestHeroes(mission: Mission): Mission {
    return addAdditionalPartyHeroes(mission, TEST_PARTY_CLASS);
  }

  // Familiars are preloaded as soon as a conjurer (Malrec) is in the party, and stay loaded.
  const partyHasConjurer = Object.entries(TEST_PARTY_CLASS).some(([name, classId]) => rulesClass(classId) === "conjurer" && (testMode || heroRecruited(name, save.completed, save.flags)));
  useEffect(() => {
    if (art && partyHasConjurer) void ensureSpriteArt(art, FAMILIAR_SPRITES);
  }, [art, partyHasConjurer]);
  // Once a new battle is on screen, drop every sprite the previous one loaded that this one
  // doesn't use (familiars stay while a conjurer is in the party), so memory follows the
  // current fight (see releaseSpriteArt).
  useEffect(() => {
    if (art && engine) releaseSpriteArt(art, [...battleSpriteIds(engine), ...(partyHasConjurer ? FAMILIAR_SPRITES : [])]);
  }, [art, engine, partyHasConjurer]);

  /** The base to start a fresh test-mode overworld/Inn session from: current roster/stats
   * (so party composition still matches whatever test mode has going) with every
   * overworld/Inn field reset to a brand-new save's defaults — the western ford, full
   * rations, no exploration. See testOverworld above. */
  const freshTestOverworld = useCallback((): SaveData => {
    const fresh = emptySave(muted);
    return {
      ...fresh,
      overworldPos: fresh.overworldPos,
      gameClock: fresh.gameClock,
      overworldMoveBudgetUsed: fresh.overworldMoveBudgetUsed,
      heroHunger: fresh.heroHunger,
      heroDiseases: fresh.heroDiseases,
      heroPoisons: fresh.heroPoisons,
      heroPoisonMag: fresh.heroPoisonMag,
      heroSkills: fresh.heroSkills,
      rations: fresh.rations,
      hungerStreak: fresh.hungerStreak,
      exploredHexes: fresh.exploredHexes,
      // Gear starts as a brand-new save's: each hero's starter weapon in hand, nothing else
      // owned — never the real slot's collected weapons/equipment.
      weapons: fresh.weapons,
      equipped: fresh.equipped,
      equipment: fresh.equipment,
      looseEquipment: fresh.looseEquipment,
      // Test mode always starts the party at DEFAULT_TEST_LEVEL, never whatever the real
      // save slot's own progression happens to be (a fresh/new real game reads level 1 here
      // otherwise, since this spreads ...save above) — the whole point of testing is having
      // enough level to actually reach higher-tier spells/gear without grinding first.
      levels: { Kael: DEFAULT_TEST_LEVEL, Neera: DEFAULT_TEST_LEVEL, Voss: DEFAULT_TEST_LEVEL, Salazar: DEFAULT_TEST_LEVEL, Aldric: DEFAULT_TEST_LEVEL, Malrec: DEFAULT_TEST_LEVEL },
    };
  }, [save, muted]);
  const startBattle = useCallback(
    (
      id: string,
      carried?: Record<string, number>,
      override?: Mission,
      playerLevels?: Record<string, number>,
      enemyLevels?: Record<number, number>,
      neutralLevels?: Record<number, number>,
      resume?: BattleSnapshot,
      preserveKnockouts = false,
      arrivedFrom?: string,
    ) => {
      if (!art) return;
      const save = testMode ? (testOverworld ?? freshTestOverworld()) : activeSave(bank);
      const carriedHp = carried ?? save.unitHp;
      // A real mission start (no override) always clears any leftover playtest identity —
      // otherwise a stale customMission from an earlier Map Editor session can collide
      // with a real campaign mission of the same id (missionToDraft now targets the real
      // id for versioning) and reroute a normal victory back into the editor.
      if (!override) {
        setCustomMission(null);
      }
      if (!testMode && !override && id === WISP_BOSS_ID && wispCrossingCompleted(save.completed)) return;
      const sourceMission = override ?? missionById(id);
      const resolved = sourceMission ? routeWispCrossing(sourceMission, testMode ? [] : save.completed) : undefined;
      if (!resolved) return;
      // A saved fight carries its own board (tiles, decorations, units), so it resumes even when
      // the mission was edited since (the fingerprint hashed the whole mission: any text, stat or
      // decor edit threw the fight away and overwrote the save with a fresh start, so Load acted
      // like New game). Only a board of a different size can't take the saved tiles.
      const mapKey = missionMapKey(sourceMission!);
      if (resume && resume.tiles.length !== sourceMission!.cols * sourceMission!.rows) resume = undefined;
      const load = ++battleLoadRef.current;
      setBattleLoading(true);
      battleAssetProgress.current = {
        sprites: { loaded: 0, total: 0 },
        decorations: { loaded: 0, total: 0 },
        terrain: { loaded: 0, total: 0 },
      };
      battleClock.current = new StepClock(BATTLE_LOAD_STEPS_FALLBACK.length);
      battleStepMs.current = rememberedStepMs(BATTLE_LOAD_STEPS_KEY, BATTLE_LOAD_STEPS_FALLBACK);
      battleLoadShown.current = "";
      setBattleLoad({ step: 0, done: 0, total: 0 });
      const tutorialMap = resolved.index <= (missionById("thebridge")?.index ?? 3) && !resolved.id.startsWith("random-encounter-");
      // The travel clock drives lighting for random maps and "-crossing" maps only for now — other campaign maps keep their authored time of day.
      let timed = !testMode && !tutorialMap && (resolved.id.startsWith("random-") || resolved.id.endsWith("-crossing")) && resolved.environment !== "indoor" && usesTravelClock(save)
        ? (() => {
            const timeOfDay = campaignTimeOfDay(campaignHour(save));
            const light = TIME_OF_DAY_LIGHT[timeOfDay];
            return { ...resolved, environment: resolved.environment ?? "outdoor", timeOfDay, sunIntensity: light.key, ambientIntensity: light.ambient };
          })()
        : resolved;
      // tonight's moon: night battles are lit by the phase of the campaign's current day
      timed = { ...timed, moonPhase: moonPhaseOf(save.gameClock) };
      // Companions sit in the walkable Inn as NPCs, but only once they've actually joined.
      const freedAldric =
        !testMode && timed.id === "watchtower-prison" && heroRecruited("Aldric", save.completed, save.flags) && timed.neutralSpawns
          ? { ...timed, neutralSpawns: timed.neutralSpawns.filter((spawn) => spawn.name !== "Aldric") }
          : timed;
      const seated =
        freedAldric.explore && !testMode && freedAldric.neutralSpawns
          ? { ...freedAldric, neutralSpawns: freedAldric.neutralSpawns.filter((s) => !(s.name in TEST_PARTY_CLASS) || heroRecruited(s.name, save.completed, save.flags)) }
          : freedAldric;
      // A free-roam map is walked by the party leader alone. A leader other than Kael takes the
      // authored walker's place (Kael waits with the rest of the party) and isn't also seated.
      const leaderName = partyLeaderOf(testMode ? (testOverworld?.partyLeader ?? save.partyLeader) : save.partyLeader, (hero) => testMode || heroRecruited(hero, save.completed, save.flags));
      const led =
        seated.explore && leaderName !== "Kael" && seated.playerSpawns[0]
          ? {
              ...seated,
              playerSpawns: [{ ...seated.playerSpawns[0], name: leaderName, classId: TEST_PARTY_CLASS[leaderName] }, ...seated.playerSpawns.slice(1)],
              neutralSpawns: seated.neutralSpawns?.filter((s) => s.name !== leaderName),
            }
          : seated;
      let m = testMode && !resolved.explore
        ? addMissingTestHeroes(led)
        : !testMode && led.id.startsWith("watchtower-") && heroRecruited("Aldric", save.completed, save.flags)
          ? addAdditionalPartyHeroes(led, { Aldric: "aldric" })
          : led;
      if (!testMode && !override) {
        const campaign = campaignPartySetup(m, save);
        m = addAdditionalPartyHeroes(campaign.mission, campaign.roster);
      }
      // Advancing through a dungeon connector carries wounds forward. Heroes who fell on
      // the previous floor stay out of the next one instead of respawning at full HP
      // because zero was treated like a missing HP value.
      if (preserveKnockouts) {
        m = { ...m, playerSpawns: m.playerSpawns.filter((spawn) => carriedHp[spawn.name] !== 0) };
      }
      if (arrivedFrom && !resume) m = arriveAtConnector(m, arrivedFrom);
      if (!resume) {
        const heroes = m.playerSpawns.filter(spawn => spawn.name in TEST_PARTY_CLASS);
        const protectedStart = tutorialMap || heroes.some(a => heroes.some(b => hexDist(a, b) > 4))
          || heroes.some(hero => (CLASSES[hero.classId].size ?? 1) > 1)
          || heroes.some(hero => m.enemySpawns.some(enemy => hexDist(hero, enemy) <= Math.max(3, CLASSES[enemy.classId].maxRange)));
        m = applyPartyFormation(m, save.partyFormation, protectedStart);
      }
      // !!! DO NOT change this back to `m.index + 1` (mission-position level) !!!
      // Test mode exists so the party can be tested at full strength on ANY mission without
      // grinding first — that means DEFAULT_TEST_LEVEL (see its own definition below, also
      // commented), not "whatever level roughly matches this mission's spot in the campaign".
      // This exact line has been reverted back to m.index + 1 by mistake multiple times
      // across sessions — if you're about to "fix" or "simplify" this, don't; ask first.
      const levels: Record<string, number> = testMode
        ? override
          ? Object.fromEntries(m.playerSpawns.map((s) => [s.name, playerLevels?.[s.name] ?? DEFAULT_TEST_LEVEL]))
          : Object.fromEntries(m.playerSpawns.map((s) => [s.name, DEFAULT_TEST_LEVEL]))
        : save.levels;
      const bags = save.bags;
      // Partial progress toward the next level (not enough to level up yet) has to carry
      // into the battle same as levels/bags do — otherwise every mission start quietly
      // zeroes out whatever XP was left over from the previous one, most visibly when
      // replaying an already-completed mission with nothing left to kill.
      const xp = testMode ? undefined : save.xp;
      // Test mode always starts at full HP — half-HP carry-over only makes sense for real runs.
      const hp = { ...carriedHp };
      if (!testMode) {
        const snapshot: SaveData = {
          ...save,
          unitHp: hp,
          levels,
          bags,
          pendingMission: id,
          battle: resume ?? null,
          muted,
        };
        combatStartRef.current = snapshot;
        // A load already wrote this slot — persisting again would smash the just-selected
        // save with whatever slot was active on the previous render.
        if (!resume) persistCurrent(snapshot);
      }
      const promotions = save.promotions;
      const weapons = Object.fromEntries(Object.entries(save.equipped).map(([hero, id]) => [hero, { id, enh: save.weapons[id] ?? 0 }]));
      // Test mode fights with the same starting off-hand kit as a new game (Neera's dagger).
      const offHand = Object.fromEntries(
            Object.entries(save.equipment)
              .map(([hero, e]) => [hero, e.offHand] as const)
              .filter((entry): entry is [string, string] => !!entry[1]),
          );
      // Every worn slot, not just the off-hand: gear contributes stats now (gearStatBonus),
      // so the battle needs the whole map rather than the one slot combat already read.
      const equipment = save.equipment;
      const statPointAllocations = save.statPointAllocations;
      const ownedWeaponIds = Object.keys(save.weapons);
      // Loading or re-entering a mission must carry saved expenditure. Replenishment happens at
      // explicit rest/reset events — and at the start of each of the four Stone Bridge tutorial
      // battles (O Vau, Bosque, Aldeia, A Ponte), which always open with every skill use back.
      // (A resumed battle still restores its own mid-fight state from the snapshot.)
      const tutorialBattle = locationForMission(m.id)?.id === "stonebridge";
      const spellSpent = testMode ? undefined : tutorialBattle ? {} : save.spellUses;
      const hungerPenaltyPct = hungerPenaltyFor(save.hungerStreak);
      const heroHunger = save.heroHunger;
      const heroDiseases = save.heroDiseases;
      const heroPoisons = save.heroPoisons;
      const crossingDefeatedSpawns = !testMode && keepsDefeatedSpawns(m) ? save.crossingDefeatedSpawns[m.id] ?? [] : [];
      const questPickups = testMode ? undefined : activePickupsFor(save, m.id);
      const battle = new BattleEngine(m, art, { hp, levels, bags, xp, promotions, weapons, offHand, equipment, statPointAllocations, enemyLevels, neutralLevels, ownedWeaponIds, affinityScores: save.affinityScores, partyLeader: leaderName, spellSpent, hungerPenaltyPct, heroHunger, heroDiseases, heroPoisons, heroPoisonMag: save.heroPoisonMag, heroSkills: save.heroSkills, crossingDefeatedSpawns, questPickups }, Date.now() % 100000, testMode);
      battle.mapKey = mapKey;
      // An intro conversation already played in this campaign never opens again (SaveData.dialogsSeen).
      if (!testMode && !override && m.introDialog && (save.dialogsSeen ?? []).includes(dialogSeenKey("intro", m.id, m.introDialog.id))) battle.introDialogDone = true;
      if (resume && resume.missionId === m.id) battle.applySnapshot(resume);
      if (typeof window !== "undefined" && window.innerWidth < 720) battle.zoom = 0;
      // Sprites load per battle (see ensureSpriteArt): the board opens once this battle's own
      // units (and the familiars, with a conjurer in the party) are loaded. If another
      // startBattle comes in meanwhile, the newer one wins.
      const report = (group: "sprites" | "decorations" | "terrain", loaded: number, total: number) => {
        if (load === battleLoadRef.current) reportBattleAssetProgress(group, loaded, total);
      };
      void Promise.all([
        ensureSpriteArt(art, [...battleSpriteIds(battle), ...(partyHasConjurer ? FAMILIAR_SPRITES : [])], (loaded, total) => report("sprites", loaded, total)),
        ensureDecorationArt(art, battle.decorations.map(p => p.id), (loaded, total) => report("decorations", loaded, total)),
        // Engine2: the old 2D tile art is gone; the 3D battlefield draws its own ground.
        Promise.resolve(),
      ]).then(() => {
        if (load !== battleLoadRef.current) return;
        setBattleStep(1, 0, 1);
        awardedRef.current = null;
        battleStartProgressRef.current = Object.fromEntries(battle.units.filter((u) => u.side === "player").map((u) => [u.name, { level: u.level, xp: u.xp }]));
        setEngine(battle);
        setMissionId(id);
        setHud(battle.getHud());
        setPaused(false);
        outroDialogShownRef.current = false;
        setOutroDialogOpen(false);
        setSlotMode(null);
        setScreen("battle");
      });
    },
    [art, save, testMode, testOverworld, freshTestOverworld, muted, bank, campaignLocations, partyHasConjurer, reportBattleAssetProgress, setBattleStep],
  );

  useEffect(() => {
    const snap = resumeBattleRef.current;
    if (!art || !snap || !missionId) return;
    startBattle(missionId, save.unitHp, undefined, undefined, undefined, undefined, snap);
    resumeBattleRef.current = null;
  }, [art, missionId, startBattle, save.unitHp]);

  const onHud = useCallback((next: HudSnapshot) => {
    setHud(next);
  }, []);

  /** Records a scripted conversation in the save the moment it opens (SaveData.dialogsSeen),
   * so it never opens by itself again in this campaign. Test mode and editor playtests
   * don't record, so authors can replay their dialogs. */
  const markDialogSeen = (key: string) => {
    if (testMode || customMission) return;
    const rec = readMapSave();
    const seen = rec.dialogsSeen ?? [];
    if (seen.includes(key)) return;
    persistCurrent({ ...rec, dialogsSeen: [...seen, key] });
  };
  const dialogAlreadySeen = (key: string) => !testMode && !customMission && (readMapSave().dialogsSeen ?? []).includes(key);

  useEffect(() => {
    if (screen !== "battle" || !hud.result) return;
    // The outro dialog fires once, exactly when victory is confirmed — never on defeat,
    // and never more than once even though hud.result stays "victory" afterward — and never
    // again in this campaign once it has played (SaveData.dialogsSeen).
    if (hud.result === "victory" && mission?.outroDialog && mission.outroDialogEnabled !== false && !outroDialogShownRef.current) {
      const key = dialogSeenKey("outro", mission.id, mission.outroDialog.id);
      if (!dialogAlreadySeen(key)) {
        markDialogSeen(key);
        setOutroDialogOpen(true);
        return;
      }
      outroDialogShownRef.current = true;
    }
    const t = window.setTimeout(() => {
      if (hud.result === "victory" && (missionId === "templo" || missionId === "portao")) {
        setScreen("epilogue");
        return;
      }
      if (hud.result) setScreen(hud.result);
    }, 1100);
    return () => window.clearTimeout(t);
  }, [hud.result, screen, missionId, mission, outroDialogOpen]);

  const closeOutroDialog = useCallback(() => {
    outroDialogShownRef.current = true;
    setOutroDialogOpen(false);
  }, []);

  const persistVictory = useCallback(() => {
    if (!engine || !mission) return;
    const battleHp = engine.battlePlayerHp();
    const floorConnector = isFloorConnector(engine.activeExit);
    const bags = engine.remainingBags();
    const growth: GrowthLine[] = [];
    const newPromotions: { name: string; options: [ClassId, ClassId] }[] = [];
    const levels = { ...save.levels };
    const xp = { ...(save.xp ?? {}) };
    const hp: Record<string, number> = {};
    // Summoned allies disappear with the battle and are recreated from their summoner's
    // current stats next time, so they are not campaign progression rows.
    for (const u of engine.units.filter((x) => x.side === "player" && !x.summoned)) {
      // Levels (and any level-ups from XP earned mid-battle) already happened live in the
      // engine — `from` is just whatever was on file before this mission started.
      const from = battleStartProgressRef.current[u.name]?.level ?? levels[u.name] ?? u.level;
      const to = u.level;
      const stFrom = statsFor(u.classId, from);
      const stTo = statsFor(u.classId, to);
      const mag = CLASSES[u.classId].mag > 0;
      const battle = battleHp[u.name] ?? u.hp;
      const healed = floorConnector
        ? (u.alive ? battle : 0)
        : u.alive
          ? Math.min(stTo.hp, battle + Math.ceil((stTo.hp - battle) * 0.5))
          : Math.max(1, Math.ceil(stTo.hp * 0.5));
      const restHp = floorConnector ? 0 : u.alive ? healed - battle : healed;
      hp[u.name] = healed;
      growth.push({
        name: u.name,
        from,
        to,
        hpBattle: battle,
        maxFrom: stFrom.hp,
        restHp,
        levelHp: stTo.hp - stFrom.hp,
        hpCamp: healed,
        maxTo: stTo.hp,
        powerFrom: mag ? stFrom.mag : stFrom.atk,
        powerTo: mag ? stTo.mag : stTo.atk,
        powerKind: mag ? "MAG" : "AT",
        atkFrom: stFrom.atk,
        atkTo: stTo.atk,
        magFrom: stFrom.mag,
        magTo: stTo.mag,
        defFrom: stFrom.def,
        defTo: stTo.def,
        dexFrom: stFrom.dex,
        dexTo: stTo.dex,
        fallen: !u.alive,
        xp: u.xp,
        xpFrom: battleStartProgressRef.current[u.name]?.xp ?? save.xp?.[u.name] ?? 0,
        skillGain: from === to ? undefined : formatSpellUseGains(spellUseGains(u.classId, from, to)),
      });
      if (!testMode && u.alive) {
        levels[u.name] = to;
        xp[u.name] = u.xp;
      }
      const options = PROMOTIONS[u.classId];
      if (!testMode && u.alive && options && !save.promotions[u.name] && from < PROMOTE_LEVEL && to >= PROMOTE_LEVEL) {
        newPromotions.push({ name: u.name, options });
      }
    }
    if (awardedRef.current === mission.id) return;
    awardedRef.current = mission.id;
    setLastGrowth(growth);
    if (newPromotions.length > 0) setPendingPromotions(newPromotions);
    const completed = completedAfterWispVictory(mission.id, save.completed);
    if (!testMode) {
      const crossingDefeatedSpawns = keepsDefeatedSpawns(mission)
        ? {
            ...save.crossingDefeatedSpawns,
            [mission.id]: [...new Set([
              ...(save.crossingDefeatedSpawns[mission.id] ?? []),
              ...engine.units.filter((unit) => (unit.side === "enemy" || unit.side === "neutral") && !unit.alive && !unit.summoned && !unit.escaped).map((unit) => unit.id),
            ])],
          }
        : save.crossingDefeatedSpawns;
      const loot = engine.units
        .filter((x) => x.side === "enemy" && !x.alive && !x.escaped)
        .reduce((n, u) => n + emberForKill(u.classId), 0);
      const weapons = { ...save.weapons };
      const looseEquipment = { ...save.looseEquipment };
      const heroDiseases = mergeBattleDiseases(save.heroDiseases, engine);
      const heroPoisons = mergeBattlePoisons(save.heroPoisons, engine);
      const found: string[] = [];
      const reward = victoryRewardFor(mission, engine.units);
      if (reward.ember > 0 || reward.rations > 0) found.push(`Recompensa por ajudar: ${reward.ember} Gold e ${reward.rations} rações`);
      // Weapon drops are already resolved and logged live, in-battle, by the engine
      // (kill drops in markDead, chest loot in useLockpick — both ownership- and
      // mission-level-aware). This just folds engine.lootWeapons into the save; it used to
      // ALSO roll its own separate 15%-per-dead-enemy chance here, completely independent
      // of and in addition to the engine's roll, silently doubling the real drop odds.
      for (const id of engine.lootWeapons) {
        if (weapons[id] != null) continue;
        const probe = { ...save, weapons: { ...weapons, [id]: 0 }, looseEquipment };
        if (!partyBagHasRoom(probe, 1, testMode)) {
          found.push(`${WEAPONS[id]!.name} (mochila cheia)`);
          continue;
        }
        weapons[id] = 0;
        found.push(WEAPONS[id]!.name);
      }
      // Equipment found in chests goes to the party's shared, unassigned stash — never
      // auto-equipped onto whoever happened to open the chest — so the player assigns it to
      // whichever hero they want from the Paperdoll picker afterward.
      for (const id of engine.lootEquipment) {
        const probe = { ...save, weapons, looseEquipment: { ...looseEquipment, [id]: (looseEquipment[id] ?? 0) + 1 } };
        if (!partyBagHasRoom(probe, 1, testMode)) {
          found.push(`${EQUIPMENT[id]!.name} (mochila cheia)`);
          continue;
        }
        looseEquipment[id] = (looseEquipment[id] ?? 0) + 1;
        found.push(EQUIPMENT[id]!.name);
      }
      setLastLoot(found);
      // Inn quests: pickups collected this battle, and any quest target that died. Kills are
      // recorded whether or not the quest was accepted yet, since a crossing dungeon keeps
      // its dead monsters dead (see crossingDefeatedSpawns above).
      const questItems = [...new Set([...(save.questItems ?? []), ...engine.questFound])];
      const deadEnemyNames = new Set(engine.units.filter((x) => x.side === "enemy" && !x.alive && !x.summoned && !x.escaped).map((x) => x.name));
      const questKills = [...new Set([...(save.questKills ?? []), ...QUESTS.filter((q) => q.kind === "kill" && q.missionId === mission.id && q.targetName && deadEnemyNames.has(q.targetName)).map((q) => q.targetName!)])];
      persistCurrent({
        ...save,
        completed,
        crossingDefeatedSpawns,
        questItems,
        questKills,
        unitHp: hp,
        bags,
        affinityScores: { ...engine.affinityScores },
        heroHunger: { ...save.heroHunger, ...engine.battlePlayerHunger() },
        heroDiseases,
        heroPoisons,
        heroPoisonMag: mergeBattlePoisonMagic(save.heroPoisonMag, engine),
        heroSkills: structuredClone(engine.heroSkills),
        levels,
        xp,
        weapons,
        looseEquipment,
        // engine.spentTiers() already reflects the full scenario-cumulative total (it was
        // seeded from save.spellUses at battle start unless this mission reset the
        // scenario) — a straight overwrite, not a merge.
        spellUses: engine.spentTiers(),
        ember: (save.ember ?? 0) + loot + engine.lootEmber + reward.ember,
        rations: save.rations + engine.lootRations + reward.rations,
        emberSeeded: true,
        muted,
        pendingMission: null,
        battle: null,
      });
    }
  }, [engine, mission, save, testMode, muted, bank]);

  useEffect(() => {
    if (screen === "victory") persistVictory();
  }, [screen, persistVictory]);

  const choosePromotion = (name: string, classId: ClassId) => {
    sfxPlay.ui();
    persistCurrent({ ...save, promotions: { ...save.promotions, [name]: classId } });
    setPendingPromotions((list) => list.filter((p) => p.name !== name));
  };

  const bootAudio = () => {
    unlockAudio();
    sfxPlay.ui();
  };

  const [missionNotice, setMissionNotice] = useState<string | null>(null);
  const openMission = (id: string) => {
    bootAudio();
    const flowSave = readMapSave();
    if (!testMode && isGatedMission(id)) {
      const rec = readMapSave();
      if (missionAccess(id, rec, progressionExtras(rec)) !== "available") return;
    }
    // Only dungeons can be entered again once done; any other finished mission is closed for good.
    if (!testMode && save.completed.includes(id)) {
      const done = missionById(id);
      if (!done || !keepsDefeatedSpawns(done)) {
        setMissionNotice("Você não pode repetir missões já completadas.");
        return;
      }
    }
    setMissionNotice(null);
    const wispForest = campaignLocations.find((location) => location.id === "wisp-forest");
    const enteringWispForest = Boolean(
      wispForest &&
        !flowSave.seenWispForestIntro &&
        wispForest.missionIds.includes(id) &&
        !wispForest.missionIds.some((mission) => flowSave.completed.includes(mission)),
    );
    if (enteringWispForest) {
      setWispForestNextMissionId(id);
      setScreen("wispForestIntro");
      return;
    }
    // The vau-intro cutscene is the campaign's opening: it plays once, from New Game only
    // (see the save-slot "new" pick). Opening O Vau itself never replays it.
    setWispForestNextMissionId(null);
    setCustomMission(null);
    setMissionId(id);
    setScreen("briefing");
  };

  const finishWispForestIntro = () => {
    const nextMissionId = wispForestNextMissionId;
    setWispForestNextMissionId(null);
    if (testMode) {
      writeMapSave({ ...readMapSave(), seenWispForestIntro: true, pendingMission: null, battle: null });
    } else {
      persistCurrent({ ...save, seenWispForestIntro: true, pendingMission: null, battle: null });
    }
    if (!nextMissionId) {
      setScreen("overworldMap");
      return;
    }
    setCustomMission(null);
    setMissionId(nextMissionId);
    setScreen("briefing");
  };

  const markInnVisited = (record: SaveData): SaveData => {
    const inn = campaignLocations.find((location) => location.id === "estalagem");
    if (!inn) return record;
    const hex = worldToHex(inn.x, inn.y);
    const visitedHex = hexKey(hex.x, hex.y);
    return {
      ...record,
      exploredHexes: (record.exploredHexes ?? []).includes(visitedHex)
        ? record.exploredHexes
        : [...(record.exploredHexes ?? []), visitedHex],
    };
  };

  const finishInnArrivalIntro = () => {
    const current = readMapSave();
    const completed = current.completed.includes("estalagem") ? current.completed : [...current.completed, "estalagem"];
    const visited = markInnVisited({ ...current, completed });
    if (testMode) {
      writeMapSave({ ...visited, seenInnArrivalIntro: true, pendingMission: null, battle: null });
    } else {
      persistCurrent({ ...markInnVisited({ ...save, completed }), seenInnArrivalIntro: true, pendingMission: null, battle: null });
    }
    // After the arrival scene the party walks into the Inn map, as on every later visit;
    // the tavern/smith menus open from talking to Brue and Vargan there.
    if (missionById("estalagem")?.explore) {
      startBattle("estalagem");
      return;
    }
    setScreen("inn");
  };

  const beginMission = () => {
    if (!missionId) return;
    bootAudio();
    // Cinematics are bound to a mission id, never its campaign position. Moving or adding
    // chapters therefore cannot detach this scene from Aldeia Queimada.
    if (missionId === "templo" || missionId === "aldeia" || missionId === "thebridge") {
      setScreen("cutscene");
      return;
    }
    // Only the actual inn opens the InnScreen. A user-authored map may retain an old hub flag.
    // It must still launch its own battle when selected from the campaign.
    if (missionId === "estalagem") {
      // Debug should be able to view the first-visit tavern arrival and smith cutscenes too.
      if (testMode && !readMapSave().seenInnArrivalIntro) {
        setScreen("innArrivalIntro");
        return;
      }
      if (missionById(missionId)?.explore) {
        startBattle(missionId);
        return;
      }
      if (!readMapSave().seenInnArrivalIntro) {
        setScreen("innArrivalIntro");
        return;
      }
      // The walkable Inn; Brue's tavern and Vargan's smith open from talking to them. Its
      // visit is recorded on the way out (see onQuit): startBattle saves its own copy of the
      // record right here, which would drop a completion written just before it.
      const current = readMapSave();
      const completed = current.completed.includes(missionId) ? current.completed : [...current.completed, missionId];
      const visited = markInnVisited({ ...current, completed, pendingMission: null, battle: null });
      if (testMode) writeMapSave(visited);
      else persistCurrent(visited);
      setScreen("inn");
      return;
    }
    startBattle(missionId);
  };

  useEffect(() => {
    if (muted) {
      stopMusic();
      return;
    }
    if (screen === "boot" || screen === "cutscene" || screen === "epilogue" || screen === "vauIntro" || screen === "wispForestIntro" || screen === "innArrivalIntro") {
      stopMusic();
      return;
    }
    // Victory/defeat and the mission briefing keep whatever track that mission plays
    // instead of falling through to the menu theme below — a win screen or a briefing
    // is still "in" that mission, not back at the title.
    const inMission = screen === "battle" || screen === "victory" || screen === "defeat" || screen === "briefing";
    // A mission that names its own track wins over every rule below: the chain of ids after
    // this is the default for missions that never picked one.
    // Only a track that is actually there wins: a name left behind by a renamed or removed
    // file falls through to the theme chain instead of leaving the mission silent.
    const named = inMission ? mission?.music : undefined;
    const chosen = named && MUSIC_TRACKS.includes(named) ? named : undefined;
    if (chosen) {
      playFile(chosen);
      return;
    }
    if (inMission && missionId === "templo") {
      playTheme("temple");
      return;
    }
    if (inMission && missionId === "aldeia") {
      playTheme("aldeia");
      return;
    }
    if (inMission && (missionId === "vau" || missionId === "bosque" || missionId === "cripta" || missionId === "vertente")) {
      playTheme("early");
      return;
    }
    if (screen === "inn") {
      // Vargan's first-visit intro video carries its own sound; the smith starts the inn
      // theme itself once the video ends (see InnScreen's finishSmithIntro).
      if (innEntry === "smith" && !save.seenSmithIntro) {
        stopMusic();
        return;
      }
      playTheme("inn");
      return;
    }
    // The walkable Inn keeps the same theme as its menus, so walking in and out of Brue's
    // and Vargan's screens never switches tracks.
    if (inMission && missionId === "estalagem") {
      playTheme("inn");
      return;
    }
    if (screen === "worldMap" || screen === "overworldMap") {
      playTheme("worldMap");
      return;
    }
    if (inMission && (missionId === "muralha" || missionId === "fortaleza")) {
      playTheme("siege");
      return;
    }
    if (inMission && (missionId === "colina" || missionId === "passagem")) {
      playTheme("hill");
      return;
    }
    if (inMission && missionId === "portao") {
      playTheme("portao");
      return;
    }
    if (inMission) {
      playTheme("early");
      return;
    }
    // Intro music belongs only to the title. Save slots, map choice and other
    // transition screens must not restart it after the player starts the game.
    // The title song carries on through the save-slot screen opened from the title.
    if (screen === "title" || (screen === "saveSlots" && slotReturnScreen === "title")) playMenuMusic();
    else stopMusic();
  }, [screen, muted, missionId, innEntry, save.seenSmithIntro, slotReturnScreen]);

  // Leaving the Inn returns to the map already selected for this run.
  const returnToCurrentMap = useCallback(() => {
    const mode = mapMode ?? save.mapMode ?? "classic";
    setMapMode(mode);
    setScreen(mode === "classic" ? "worldMap" : "overworldMap");
  }, [mapMode, save.mapMode]);

  // Campaign maps reuse the mode stored in that save. Debug asks for a map style once
  // when the test run begins, then returns to that map for the rest of the run.
  const goToMap = useCallback(() => {
    // Test mode asks once at entry, then preserves the chosen map until leaving the run.
    if (testMode) {
      if (!mapMode) {
        setScreen("mapChoice");
        return;
      }
      setScreen(mapMode === "classic" ? "worldMap" : "overworldMap");
      return;
    }
    // Older campaign records predate the RPG map preference. Returning from a mission
    // should still land on a playable campaign map instead of restarting at the chooser.
    const mode = save.mapMode ?? mapMode ?? "classic";
    setMapMode(mode);
    if (!save.mapMode) persistCurrent({ ...save, mapMode: mode });
    setScreen(mode === "classic" ? "worldMap" : "overworldMap");
  }, [save, mapMode, testMode, persistCurrent]);

  // /game.html?start=test enters the original Test Mode menu, matching the title button.
  const testStartHandled = useRef(false);
  useEffect(() => {
    if (startMode !== "test" || testStartHandled.current) return;
    testStartHandled.current = true;
    setTestMode(true);
    setTestEmber(TEST_EMBER);
    setTestOverworld(null);
    setLastGrowth(null);
    setLastLoot([]);
    setMissionId(null);
    setMapMode(null);
    setScreen("testMenu");
  }, [startMode]);

  const continueStartHandled = useRef(false);
  useEffect(() => {
    if (startMode !== "continue" || continueStartHandled.current) return;
    continueStartHandled.current = true;
    enterFromSave(save);
  }, [startMode]);

  const leaveBoot = useCallback(() => {
    // Debug boot can still open the travel-mode chooser. A new campaign's vignette
    // finishes at save-slot selection; the chosen slot then starts O Vau's intro.
    if (testMode) {
      playTheme("worldMap");
      setScreen("mapChoice");
      return;
    }
    if (startNewCampaign) {
      setMissionId("vau");
      setScreen("vauIntro");
      return;
    }
    setSlotReturnScreen("title");
    setSlotMode("new");
    setOverwrite(null);
    setScreen("saveSlots");
  }, [testMode, startNewCampaign]);

  const goToTitle = useCallback(() => {
    stopMusic();
    // Wipe every trace of test mode the instant you leave it — testMode itself used to
    // linger true here (only onNew/onContinue on the title screen ever cleared it, as a
    // defensive afterthought), and testOverworld/testEmber stayed at whatever test mode
    // left them, not reset until the next "Modo Teste" entry. None of it ever touches the
    // real save, but it shouldn't outlive the session that made it either.
    setTestMode(false);
    setTestOverworld(null);
    setTestEmber(TEST_EMBER);
    returnToTitle();
  }, []);

  /** The save every map/Inn handler below reads: the real bank normally, or test mode's
   * own ephemeral overworld snapshot — never the real bank — while testing. */
  const readMapSave = useCallback(
    (): SaveData => (testMode ? (testOverworld ?? freshTestOverworld()) : activeSave(bank)),
    [testMode, testOverworld, freshTestOverworld, bank],
  );
  /** Writes a map/Inn action's result back — to test mode's own state, never the real
   * bank, while testing, so nothing done there ever becomes a real savegame. */
  const writeMapSave = useCallback(
    (next: SaveData) => {
      if (testMode) setTestOverworld(next);
      else persistCurrent(next);
    },
    [testMode],
  );
  // The map/Inn's own view of the save — test-safe (see readMapSave). Battle keeps reading
  // `save`/`liveSave` directly; it already isolates test mode through its own dedicated
  // overrides (testEmber, playtest roster building, ...), untouched by this.
  const overworldSave = readMapSave();
  // Mission gating (progression.ts): what each mission currently shows as on the world map.
  const missionAccessFor = (id: string): MissionAccess => missionAccess(id, overworldSave, progressionExtras(overworldSave), testMode);
  const visibleMissionsOf = (loc: WorldLocation) => missionsForLocation(loc).filter((mission) => testMode || missionAccessFor(mission.id) !== "hidden");
  const onOverworldStep = useCallback(
    (col: number, row: number) => {
      const rec = readMapSave();
      const { save: next, event } = stepOverworld(rec, col, row, campaignLocations, testMode);
      if (next !== rec) writeMapSave(next);
      // A rolled road encounter launches straight into its battle — never shown as a
      // dismissible text popup like every other overworld event.
      if (event?.kind === "battle" && event.missionId) {
        openMission(event.missionId);
        return;
      }
      if (event) setOverworldEvent(event);
    },
    [campaignLocations, testMode, openMission, readMapSave, writeMapSave],
  );
  const consumeRation = (hero: string) => {
    if (screen === "battle" && engine) {
      const rec = activeSave(bank);
      const unit = engine.units.find((u) => u.name === hero && u.side === "player" && !u.summoned && u.alive);
      if (!unit || engine.getHud().busy || fullness(unit.fullness) >= 100 || rec.rations + engine.lootRations < 1) return;
      if (!engine.feedUnit(unit.id)) return;
      if (rec.rations > 0) persistCurrent(withLiveBattle({ ...rec, rations: rec.rations - 1 }));
      else {
        engine.lootRations -= 1;
        persistCurrent(withLiveBattle(rec));
      }
      onHud(engine.getHud());
      return;
    }
    const rec = readMapSave();
    let next = useRation(rec, hero);
    if (next === rec) return;
    // A ration can clear the whole party's streak the moment it does, same as a completed
    // overworld step would next time it ran — otherwise "Fome Xd" and its stat penalty sit
    // stale on-screen until the party's next move recomputes them.
    if ((next.hungerStreak > 0 || (next.hungerHours ?? 0) > 0) && partyIsFed(next, testMode)) next = { ...next, hungerStreak: 0, hungerHours: 0 };
    writeMapSave(next);
  };
  /** Mochila's "Alimentar todos" — one ration per hero in the given roster, off the shared
   * party stock. Inn/overworld only (mirrors consumeRation's plain, non-battle branch;
   * battle rations come out of engine.lootRations too and need that per-unit bookkeeping,
   * not worth threading through a bulk action here). */
  const consumeRationAll = (heroes: string[]) => {
    const rec = readMapSave();
    let next = rec;
    let fed = 0;
    for (const hero of heroes) {
      const after = useRation(next, hero);
      if (after !== next) fed++;
      next = after;
    }
    if (fed === 0) return 0;
    if ((next.hungerStreak > 0 || (next.hungerHours ?? 0) > 0) && partyIsFed(next, testMode)) next = { ...next, hungerStreak: 0, hungerHours: 0 };
    writeMapSave(next);
    return fed;
  };
  /** World-map-only cast of Create Food and Water (Healer tier 4) — the battle-side version
   * is engine.startCreateFoodAndWater(); this one lands directly on SaveData since there's
   * no live BattleEngine to hold the effect outside a fight. Spends from the same
   * save.spellUses tier-4 pool (see SPELL_TIER) a battle
   * cast would (see remainingTier/tierRemaining in engine.ts, which reads the exact same
   * field at battle start) — recovers at half-rate per overworld day, see stepOverworld's
   * new spellUses math in overworld.ts. */
  const castCreateFoodAndWater = (hero: string): boolean => {
    const rec = readMapSave();
    const classId = rec.promotions[hero] ?? MAP_STATUS_CLASS[hero];
    if (!classId || rulesClass(classId) !== "healer") return false;
    const level = rec.levels[hero] ?? 1;
    const spent = rec.spellUses[hero]?.tier4 ?? 0;
    if (tierUses(classId, 4, level) - spent <= 0) return false;
    const power = createFoodAndWaterPower(level);
    if (fullness(rec.heroHunger[hero]) >= power.fullness) return false;
    const gained = power.dice > 0 ? rollDice(power.dice, power.faces, power.bonus, Math.random) : 0;
    writeMapSave({
      ...rec,
      heroHunger: { ...rec.heroHunger, [hero]: power.fullness },
      rations: rec.rations + gained,
      spellUses: { ...rec.spellUses, [hero]: { ...rec.spellUses[hero], tier4: spent + 1 } },
    });
    return true;
  };
  /** World-map Warp shares the mage's battle tier-3 pool and only targets explored cities. */
  const castWarpOverworld = (hero: string, cityId: string): boolean => {
    const rec = readMapSave();
    const classId = rec.promotions[hero] ?? MAP_STATUS_CLASS[hero];
    const level = rec.levels[hero] ?? 1;
    if (!classId || rulesClass(classId) !== "mage" || level < WARP.unlockLevel) return false;
    const destination = campaignLocations.find((location) => location.id === cityId);
    if (!destination) return false;
    const hex = worldToHex(destination.x, destination.y);
    const cityHex = `${hex.x},${hex.y}`;
    const visited = (rec.exploredHexes ?? []).includes(cityHex) || destination.missionIds.some((id) => rec.completed.includes(id));
    if (!visited || (hex.x === rec.overworldPos.col && hex.y === rec.overworldPos.row)) return false;
    const spent = rec.spellUses[hero]?.tier3 ?? 0;
    if (tierUses(classId, WARP.tier, level) - spent <= 0) return false;
    writeMapSave({
      ...rec,
      overworldPos: { col: hex.x, row: hex.y },
      exploredHexes: (rec.exploredHexes ?? []).includes(cityHex) ? rec.exploredHexes : [...(rec.exploredHexes ?? []), cityHex],
      spellUses: { ...rec.spellUses, [hero]: { ...rec.spellUses[hero], tier3: spent + 1 } },
      pendingMission: null,
    });
    sfxPlay.summonFamiliar();
    return true;
  };
  // Modo teste only: a non-adjacent pin jumps straight there, free of charge — see
  // OverworldMapScreen's onTeleport doc. Adjacent pins/wild hexes still go through
  // onOverworldStep above even in test mode, so the day clock and rations stay testable.
  const onOverworldTeleport = useCallback(
    (col: number, row: number) => {
      writeMapSave(teleportOverworld(readMapSave(), col, row));
    },
    [readMapSave, writeMapSave],
  );

  // Shared outside-of-battle equip handlers — same shape the Inn's Mochila/Paperdoll have
  // always used, now also handed to the RPG overworld map's own Mochila (see
  // OverworldMapScreen), which used to render that picker without any onEquipWeapon/
  // onEquipItem at all: every tap there was a silent no-op, so a hero's owned weapon could
  // sit in the backpack forever looking "stuck."
  const equipHeroWeapon = useCallback(
    (hero: string, weaponId: string) => {
      const rec = readMapSave();
      if (!weaponId) {
        const next = unequipSharedWeapon(rec, hero);
        if (!partyBagHasRoom(next, 0, testMode)) return;
        writeMapSave({ ...next, pendingMission: null });
        return;
      }
      const next = equipSharedWeapon(rec, hero, weaponId);
      if (next) writeMapSave({ ...next, pendingMission: null });
    },
    [testMode, readMapSave, writeMapSave],
  );
  const equipHeroItem = useCallback(
    (hero: string, slot: EquipSlot, itemId: string | null) => {
      const rec = readMapSave();
      if (!itemId) {
        const next = unequipSharedItem(rec, hero, slot);
        if (!partyBagHasRoom(next, 0, testMode)) return;
        writeMapSave({ ...next, pendingMission: null });
        return;
      }
      const next = equipSharedItem(rec, hero, slot, itemId);
      if (next) writeMapSave({ ...next, pendingMission: null });
    },
    [testMode, readMapSave, writeMapSave],
  );

  // Mochila's "Jogar Fora" / "Usar" actions (see ItemActionSheet in InventoryScreens.tsx) —
  // outside of battle these just rewrite the save directly, same shape as the equip
  // callbacks above.
  const discardHeroWeapon = useCallback(
    (weaponId: string) => writeMapSave({ ...discardSharedWeapon(readMapSave(), weaponId), pendingMission: null }),
    [readMapSave, writeMapSave],
  );
  const discardHeroEquipment = useCallback(
    (itemId: string) => writeMapSave({ ...discardSharedEquipment(readMapSave(), itemId), pendingMission: null }),
    [readMapSave, writeMapSave],
  );
  const discardHeroRation = useCallback(() => writeMapSave({ ...discardRation(readMapSave()), pendingMission: null }), [readMapSave, writeMapSave]);
  const discardHeroBagItem = useCallback(
    (hero: string, kind: PotionId | "lockpick") => writeMapSave({ ...discardBagItem(readMapSave(), hero, kind), pendingMission: null }),
    [readMapSave, writeMapSave],
  );
  const useHeroPotionOutside = useCallback(
    (hero: string, kind: PotionId) => writeMapSave({ ...useHeroPotion(readMapSave(), hero, kind), pendingMission: null }),
    [readMapSave, writeMapSave],
  );

  // A location with no visible missions stays off both map views until its gate reveals one.
  // The overworld simulation still receives campaignLocations so hidden sites retain their
  // authored hex encounter biome before the marker is discovered.
  const mapVisibleLocations = campaignLocations.filter((location) =>
    testMode || location.missionIds.some((id) => missionAccessFor(id) !== "hidden"),
  );

  return (
    <main className="relative h-dvh min-h-0 bg-bg text-fg overflow-hidden">
      <LoadingCurtain
        visible={screen !== "title" && screen !== "boot" && (loadingCurtain || battleLoading || (screen === "battle" && !engine))}
        progress={battleLoading || screen === "battle" ? Math.floor(stepPercent(battleStepMs.current, battleLoad.step, battleLoad.done, battleLoad.total)) : null}
        status={battleLoading || screen === "battle" ? battleLoadCaption(battleLoad) : undefined}
      />
      {screen === "boot" && (
        <CutsceneScreen src="/game/title-open.mp4" onSkip={leaveBoot} />
      )}
      {screen === "title" && <TitlePageHandoff />}

      {screen === "testMenu" && (
        <TestMenuScreen
          ready={!!art}
          onBack={goToTitle}
          onDebug={goToMap}
          onMapEditor={() => setScreen("mapEditor")}
          onDevControls={() => setScreen("devControls")}
        />
      )}

      {screen === "devControls" && <DevControlsScreen onBack={() => setScreen("testMenu")} />}

      {screen === "mapChoice" && (
        <MapChoiceScreen
          onBack={() => setScreen(testMode ? "testMenu" : "title")}
          onPick={(mode) => {
            setMapMode(mode);
            if (!testMode) persistCurrent({ ...save, mapMode: mode });
            // Open the map at the saved starting hex. The first trip is a player action.
            setScreen(mode === "classic" ? "worldMap" : "overworldMap");
          }}
        />
      )}

      {screen === "vauIntro" && (
        <CutsceneScreen
          src="/game/vau-intro.mp4"
          onSkip={() => {
            setMissionId("vau");
            setScreen("briefing");
          }}
        />
      )}

      {screen === "wispForestIntro" && (
        <CutsceneScreen src="/game/wisp-entrance.mp4" subtitles={{ pt: "/game/subtitles/wisp-entrance.pt.vtt", en: "/game/subtitles/wisp-entrance.en.vtt" }} onSkip={finishWispForestIntro} />
      )}

      {screen === "innArrivalIntro" && (
        <CutsceneScreen src="/game/inn-arrival.mp4" subtitles={{ pt: "/game/subtitles/inn-arrival.pt.vtt", en: "/game/subtitles/inn-arrival.en.vtt" }} onSkip={finishInnArrivalIntro} />
      )}

      {screen === "mapEditor" && art && (
        <MapEditorScreen3D
          art={art}
          // The editor unmounts while a playtest runs, so the map being worked on is held
          // out here and handed back on return — otherwise testing a map threw it away.
          initialDraft={editorDraft.current}
          onDraftChange={(d) => {
            editorDraft.current = d;
          }}
          onBack={() => { clearEditorResume(); setScreen(testMode ? "testMenu" : "title"); }}
          onPlaytest={(m, playerLevels, enemyLevels, neutralLevels) => {
            setCustomMission(m);
            startBattle(m.id, {}, m, playerLevels, enemyLevels, neutralLevels);
          }}
        />
      )}

      {screen === "campaign" && (
        <CampaignScreen
          missions={campaignMissions.filter((mission) => testMode || missionAccessFor(mission.id) !== "hidden")}
          locations={mapVisibleLocations}
          missionAccessFor={missionAccessFor}
          completed={save.completed}
          test={testMode}
          ember={testMode ? testEmber : (save.ember ?? 0)}
          onBack={() => setScreen("worldMap")}
          onPick={openMission}
        />
      )}

      {screen === "worldMap" && (
        <WorldMapScreen
          locations={mapVisibleLocations}
          status={(loc) => locationStatus(loc, save.completed, testMode, campaignLocations, missionAccessFor)}
          missionStatus={(id) => missionStatus(id, save.completed, testMode, campaignLocations, campaignMissions.map((mission) => mission.id), missionAccessFor)}
          missionsOf={visibleMissionsOf}
          ember={testMode ? testEmber : (save.ember ?? 0)}
          test={testMode}
          muted={muted}
          onMute={() => {
            unlockAudio();
            setMutedUi((v) => !v);
          }}
          autoOpenLocationId={openLocationOnMap}
          centerLocationId={mapVisibleLocations.find((location) => location.missionIds.some((id) => !save.completed.includes(id)))?.id ?? null}
          onBack={() => setScreen(testMode ? "testMenu" : "title")}
          onPick={openMission}
          onOpenList={() => setScreen("campaign")}
        />
      )}

      {screen === "overworldMap" && (
        <OverworldMapScreen
          onSaveFormation={order => {
            const formation = cleanPartyFormation(order);
            // Test mode keeps it for the test session only, like every other test-mode map state.
            if (testMode) {
              writeMapSave({ ...readMapSave(), partyFormation: formation });
              return { ok: true, test: true };
            }
            persistCurrent({ ...readMapSave(), partyFormation: formation });
            // Proof, not a promise: read the slot back from storage and compare.
            const stored = activeSave(loadBank()).partyFormation ?? [];
            return { ok: JSON.stringify(stored) === JSON.stringify(formation), test: false };
          }}
          onSaveLeader={hero => {
            writeMapSave({ ...readMapSave(), partyLeader: cleanPartyLeader(hero) });
          }}
          onSetTravelTraining={(hero, skill) => {
            // One road-training skill per hero; picking or switching restarts its 12 h count.
            const current = readMapSave();
            const travelTraining = { ...current.travelTraining };
            if (skill) travelTraining[hero] = skill;
            else delete travelTraining[hero];
            const flags = current.flags?.includes(TRAVEL_TRAINING_HINT_FLAG) ? current.flags : [...(current.flags ?? []), TRAVEL_TRAINING_HINT_FLAG];
            writeMapSave({ ...current, travelTraining, travelTrainingHours: { ...current.travelTrainingHours, [hero]: 0 }, flags });
          }}
          onSeenTravelTrainingHint={() => {
            const current = readMapSave();
            if (!current.flags?.includes(TRAVEL_TRAINING_HINT_FLAG)) writeMapSave({ ...current, flags: [...(current.flags ?? []), TRAVEL_TRAINING_HINT_FLAG] });
          }}
          locations={mapVisibleLocations}
          status={(loc) => locationStatus(loc, save.completed, testMode, campaignLocations, missionAccessFor)}
          missionStatus={(id) => missionStatus(id, save.completed, testMode, campaignLocations, campaignMissions.map((mission) => mission.id), missionAccessFor)}
          missionsOf={visibleMissionsOf}
          ember={testMode ? testEmber : (save.ember ?? 0)}
          test={testMode}
          muted={muted}
          onMute={() => {
            unlockAudio();
            setMutedUi((v) => !v);
          }}
          onSave={() => {
            setOverwrite(null);
            setSlotReturnScreen("overworldMap");
            setSlotMode("save");
            setScreen("saveSlots");
          }}
          onLoad={() => {
            setOverwrite(null);
            setSlotReturnScreen("overworldMap");
            setSlotMode("load");
            setScreen("saveSlots");
          }}
          overworldPos={overworldSave.overworldPos}
          gameClock={overworldSave.gameClock}
          rations={overworldSave.rations}
          hungerStreak={overworldSave.hungerStreak}
          heroHunger={overworldSave.heroHunger}
          save={overworldSave}
          onUseRation={consumeRation}
          onUseRationAll={consumeRationAll}
          onCastCreateFoodAndWater={castCreateFoodAndWater}
          onCastWarp={castWarpOverworld}
          onEquipWeapon={equipHeroWeapon}
          onEquipItem={equipHeroItem}
          onUsePotion={useHeroPotionOutside}
          onDiscardWeapon={discardHeroWeapon}
          onDiscardEquipment={discardHeroEquipment}
          onDiscardRation={discardHeroRation}
          onDiscardBagItem={discardHeroBagItem}
          inventoryRequestHero={mapInventoryRequestHero}
          inventoryRequestView={mapInventoryRequestView}
          onInventoryRequestHandled={() => setMapInventoryRequestHero(null)}
          onOpenStatus={setMapStatusHero}
          event={overworldEvent}
          onDismissEvent={() => setOverworldEvent(null)}
          onStep={onOverworldStep}
          onTeleport={onOverworldTeleport}
          onBack={() => setScreen(testMode ? "testMenu" : "title")}
          onPick={openMission}
        />
      )}
      {missionNotice && (screen === "campaign" || screen === "worldMap" || screen === "overworldMap") && (
        <button
          type="button"
          onClick={() => setMissionNotice(null)}
          className="fixed z-50 top-24 left-1/2 -translate-x-1/2 ember-plate px-3 py-1.5 text-xs"
        >
          {missionNotice}
        </button>
      )}
      {screen === "overworldMap" && !overworldSave.seenOverworldIntro && (
        <OverworldIntroScreen
          onClose={() => {
            writeMapSave({ ...overworldSave, seenOverworldIntro: true, pendingMission: null });
          }}
        />
      )}
      {(screen === "overworldMap" || screen === "inn") && mapStatusHero && (
        <StatusPanel
          unit={mapStatusUnit(overworldSave, mapStatusHero)}
          statPointAllocation={overworldSave.statPointAllocations[mapStatusHero] ?? {}}
          unspentStatPoints={Math.max(0, ((overworldSave.levels[mapStatusHero] ?? 1) - 1) * STAT_POINTS_PER_LEVEL - Object.values(overworldSave.statPointAllocations[mapStatusHero] ?? {}).reduce((total, value) => total + (value ?? 0), 0))}
          bagIcon={pouchIcon(equippedPouchId(overworldSave.equipment, mapStatusHero))}
          onClose={() => setMapStatusHero(null)}
          onOpenInventory={screen === "overworldMap" ? () => {
              setMapStatusHero(null);
              setMapInventoryRequestView("backpack");
              setMapInventoryRequestHero(mapStatusHero);
            } : undefined}
          onOpenEquipment={screen === "overworldMap" ? () => {
              setMapStatusHero(null);
              setMapInventoryRequestView("equipment");
              setMapInventoryRequestHero(mapStatusHero);
            } : undefined}
        />
      )}

      {screen === "briefing" && mission && (
        <BriefingScreen
          mission={mission}
          onBack={goToMap}
          onStart={beginMission}
          muted={muted}
          onMute={() => setMutedUi((v) => !v)}
        />
      )}

      {screen === "inn" && (
        <InnScreen
          onUseRation={consumeRation}
          onUseRationAll={consumeRationAll}
          onOpenStatus={setMapStatusHero}
          onBuyMeal={(hero) => {
            const rec = readMapSave();
            const source = testMode ? { ...rec, ember: testEmber } : rec;
            const next = buyInnMeal(source, hero);
            if (next === source) return false;
            if (testMode) setTestEmber(next.ember);
            writeMapSave({ ...next, ember: testMode ? rec.ember : next.ember });
            return true;
          }}
          onBuyMealAll={(heroes) => {
            const rec = readMapSave();
            const source = testMode ? { ...rec, ember: testEmber } : rec;
            let next = source;
            let fed = 0;
            for (const hero of heroes) {
              const after = buyInnMeal(next, hero);
              if (after !== next) fed++;
              next = after;
            }
            if (fed === 0) return 0;
            if (testMode) setTestEmber(next.ember);
            writeMapSave({ ...next, ember: testMode ? rec.ember : next.ember });
            return fed;
          }}
          onPassNight={(heroes: string[]) => {
            const rec = readMapSave();
            const cost = heroes.length * NIGHT_REST_PRICE;
            const balance = testMode ? testEmber : rec.ember ?? 0;
            if (heroes.length === 0 || balance < cost) return false;
            const unitHp = { ...rec.unitHp };
            let healed = 0;
            for (const hero of heroes) {
              const target = mapStatusUnit(rec, hero);
              const missing = Math.max(0, target.maxHp - target.hp);
              if (missing <= 0) continue;
              const amount = Math.min(missing, Math.ceil(missing * 0.75));
              unitHp[hero] = target.hp + amount;
              healed += amount;
            }
            if (testMode) setTestEmber(balance - cost);
            const next = {
              ...rec,
              ember: testMode ? rec.ember : balance - cost,
              gameClock: rec.gameClock + 1,
              gameHour: usesTravelClock(rec) ? 8 : rec.gameHour,
              unitHp,
              spellUses: {},
            };
            writeMapSave(next);
            return { day: next.gameClock, healed };
          }}
          bags={overworldSave.bags}
          ember={testMode ? testEmber : (save.ember ?? 0)}
          muted={muted}
          weapons={overworldSave.weapons}
          equipped={overworldSave.equipped}
          heroClass={Object.fromEntries(
            [...DEFAULT_HEROES, ...TEST_EXTRA_HEROES.filter((h) => testMode || heroRecruited(h.name, save.completed, save.flags))].map((h) => [h.name, overworldSave.promotions[h.name] ?? h.classId]),
          )}
          healerTargets={[
            ...DEFAULT_HEROES,
            ...TEST_EXTRA_HEROES.filter((h) => testMode || heroRecruited(h.name, save.completed, save.flags)),
          ].filter((h) => testMode || heroRecruited(h.name, save.completed, save.flags)).map((h) => {
            const unit = mapStatusUnit(overworldSave, h.name);
            return { name: h.name, hp: unit.hp, maxHp: unit.maxHp, sprite: unit.sprite, diseased: unit.diseased, poisoned: unit.poisoned };
          })}
          onHealerCast={(hero: string) => {
            const rec = readMapSave();
            const balance = testMode ? testEmber : rec.ember ?? 0;
            if (balance < HEALER_CAST_PRICE) return false;
            const target = mapStatusUnit(rec, hero);
            const missing = target.maxHp - target.hp;
            if (missing <= 0 || target.hp <= 0) return false;
            const amount = Math.min(Math.max(1, Math.ceil(target.maxHp * 0.25)), missing);
            if (amount <= 0) return false;
            if (testMode) setTestEmber(balance - HEALER_CAST_PRICE);
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember : balance - HEALER_CAST_PRICE,
              unitHp: { ...rec.unitHp, [hero]: target.hp + amount },
            });
            sfxPlay.heal();
            return amount;
          }}
          onHealerCureAilments={(heroes: string[]) => {
            const rec = readMapSave();
            const affected = heroes.filter((hero) => rec.heroDiseases[hero] || rec.heroPoisons[hero]);
            if (affected.length === 0) return 0;
            const cost = affected.length * HEALER_AILMENT_PRICE;
            const balance = testMode ? testEmber : rec.ember ?? 0;
            if (balance < cost) return false;
            const heroDiseases = { ...rec.heroDiseases };
            const heroPoisons = { ...rec.heroPoisons };
            for (const hero of affected) {
              delete heroDiseases[hero];
              delete heroPoisons[hero];
            }
            if (testMode) setTestEmber(balance - cost);
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember : balance - cost,
              heroDiseases,
              heroPoisons,
            });
            sfxPlay.heal();
            return affected.length;
          }}
          save={testMode ? { ...overworldSave, ember: testEmber } : overworldSave}
          test={testMode}
          onMute={() => {
            unlockAudio();
            setMutedUi((v) => !v);
          }}
          farmlandsService={innEntry ? FARMLANDS_SERVICES[missionId ?? ""] : undefined}
          startInSmith={innEntry === "smith"}
          startInHealer={innEntry === "healer"}
          startInMerchant={innEntry === "merchant"}
          startInMerchantGear={innEntry === "merchantGear"}
          merchantBackdrop={missionId === "random-encounter-14" ? "/game/assets/merchant-snow-market-background-001.jpg" : "/game/assets/merchant-road-background-001.jpg"}
          onLeave={
            innEntry
              ? () => {
                  // Rebuilt from the save rather than resuming the old engine: purchases made
                  // in Brue's/Vargan's menu must reach the party, and the old engine's stale
                  // bags/hunger would otherwise be written back over them on the next equip.
                  // The leader is put back exactly where they stood to talk.
                  const leader = engine?.units.find((u) => u.side === "player" && u.alive && !u.summoned);
                  const base = engine?.mission ?? missionById("estalagem");
                  setInnEntry(null);
                  if (!base || !missionId) {
                    goToMap();
                    return;
                  }
                  const spawns = base.playerSpawns.map((s, i) => (i === 0 && leader ? { ...s, x: leader.x, y: leader.y } : s));
                  startBattle(missionId, undefined, { ...base, playerSpawns: spawns }, undefined, undefined, undefined, undefined, !!FARMLANDS_SERVICES[missionId]);
                }
              : returnToCurrentMap
          }
          onBuyWeapon={(hero: string, weaponId: string) => {
            const rec = readMapSave();
            const w = WEAPONS[weaponId];
            if (!w || rec.weapons[weaponId] != null) return false;
            if (FARMLANDS_SERVICES[missionId ?? ""]?.kind === "smith" && !isFarmlandsSmithWeapon(weaponId)) return false;
            if (!partyBagHasRoom(rec, 1, testMode)) return false;
            const held = testMode ? testEmber : (rec.ember ?? 0);
            if (held < w.price) return false;
            if (testMode) setTestEmber(held - w.price);
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember ?? 0 : held - w.price,
              emberSeeded: true,
              weapons: { ...rec.weapons, [weaponId]: 0 },
              pendingMission: null,
            });
            return true;
          }}
          onBuyEquipment={(itemId: string) => {
            const rec = readMapSave();
            const item = EQUIPMENT[itemId];
            const price = item?.price ?? 0;
            if (!item || (price <= 0 && !testMode)) return false;
            if (!partyBagHasRoom(rec, 1, testMode)) return false;
            const held = testMode ? testEmber : (rec.ember ?? 0);
            if (held < price) return false;
            if (testMode) setTestEmber(held - price);
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember ?? 0 : held - price,
              emberSeeded: true,
              looseEquipment: { ...rec.looseEquipment, [itemId]: (rec.looseEquipment[itemId] ?? 0) + 1 },
              pendingMission: null,
            });
            return true;
          }}
          onTalkToNpc={(npcId: string) => {
            const rec = readMapSave();
            if (missionId && FARMLANDS_SERVICES[missionId]) {
              if (!(rec.npcTalked ?? []).includes(missionId)) writeMapSave({ ...rec, npcTalked: [...(rec.npcTalked ?? []), missionId] });
              return;
            }
            let next = rec;
            if (!(rec.npcTalked ?? []).includes(npcId)) next = { ...next, npcTalked: [...(rec.npcTalked ?? []), npcId] };
            const extras = progressionExtras(next);
            const offered = questsFor(npcId as "brue" | "mudo" | "suspicious" | "porao").filter((quest) => questStatus(next, quest) === "available" && evaluate(quest.availability, next, extras) && !(next.questsDiscovered ?? []).includes(quest.id));
            if (offered.length > 0) next = { ...next, questsDiscovered: [...(next.questsDiscovered ?? []), ...offered.map((quest) => quest.id)] };
            if (next !== rec) writeMapSave(next);
          }}
          questOffered={(questId: string) => {
            const quest = questById(questId);
            return !quest || evaluate(quest.availability, overworldSave, progressionExtras(overworldSave));
          }}
          onAcceptQuest={(questId: string) => {
            const rec = readMapSave();
            const quest = questById(questId);
            if (!quest || questStatus(rec, quest) !== "available") return false;
            writeMapSave({ ...rec, questsActive: [...(rec.questsActive ?? []), questId] });
            return true;
          }}
          onTurnInQuest={(questId: string) => {
            const rec = readMapSave();
            const quest = questById(questId);
            if (!quest || questStatus(rec, quest) !== "ready") return false;
            if (testMode) setTestEmber(testEmber + quest.reward);
            // Reward potions go to whichever hero has room (mana potions try the casters
            // first); a potion nobody has room for is dropped, same as a full-party chest find.
            const bags = Object.fromEntries(Object.entries(rec.bags).map(([hero, bag]) => [hero, { ...bag }]));
            for (const kind of quest.rewardPotions) {
              const heroes = Object.keys(bags);
              const order = kind === "manaMid" || kind === "manaSmall" || kind === "manaLarge" ? [...heroes].sort((a, b) => Number(b === "Voss" || b === "Salazar") - Number(a === "Voss" || a === "Salazar")) : heroes;
              const taker = order.find((hero) => (bags[hero]![kind] ?? 0) < POTION_CARRY_MAX[kind]);
              if (taker) bags[taker]![kind] = (bags[taker]![kind] ?? 0) + 1;
            }
            writeMapSave({
              ...rec,
              bags,
              ember: testMode ? rec.ember ?? 0 : (rec.ember ?? 0) + quest.reward,
              emberSeeded: true,
              questsActive: (rec.questsActive ?? []).filter((id) => id !== questId),
              questsDone: [...(rec.questsDone ?? []), questId],
            });
            return true;
          }}
          onEquipWeapon={equipHeroWeapon}
          onEquipItem={equipHeroItem}
          onUsePotion={useHeroPotionOutside}
          onDiscardWeapon={discardHeroWeapon}
          onDiscardEquipment={discardHeroEquipment}
          onDiscardRation={discardHeroRation}
          onDiscardBagItem={discardHeroBagItem}
          onUpgradeWeapon={(weaponId: string) => {
            const rec = readMapSave();
            const enh = rec.weapons[weaponId] ?? 0;
            if (enh >= WEAPON_MAX_ENH) return false;
            const cost = weaponEnhCost(enh + 1);
            const held = testMode ? testEmber : (rec.ember ?? 0);
            if (held < cost) return false;
            if (testMode) setTestEmber(held - cost);
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember ?? 0 : held - cost,
              emberSeeded: true,
              weapons: { ...rec.weapons, [weaponId]: enh + 1 },
              pendingMission: null,
            });
            return true;
          }}
          onSellWeapon={(weaponId: string) => {
            const rec = readMapSave();
            const enh = rec.weapons[weaponId];
            if (enh == null) return false;
            const value = weaponSellValue(weaponId, enh);
            const held = testMode ? testEmber : (rec.ember ?? 0);
            if (testMode) setTestEmber(held + value);
            const weapons = { ...rec.weapons };
            delete weapons[weaponId];
            const equipped = { ...rec.equipped };
            for (const hero of Object.keys(equipped)) {
              if (equipped[hero] === weaponId) delete equipped[hero];
            }
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember ?? 0 : held + value,
              emberSeeded: true,
              weapons,
              equipped,
              pendingMission: null,
            });
            return value;
          }}
          onSeenSmithIntro={() => {
            const rec = readMapSave();
            writeMapSave({ ...rec, seenSmithIntro: true, pendingMission: null });
          }}
          onPay={(hero: string, cart: Record<PotionId, number>, lockpicks: number) => {
            const rec = readMapSave();
            let cost = 0;
            const bag = { ...(rec.bags[hero] ?? startingBags()[hero]) };
            for (const kind of Object.keys(cart) as PotionId[]) {
              const qty = cart[kind] ?? 0;
              if (qty <= 0) continue;
              if ((bag[kind] ?? 0) + qty > POTION_CARRY_MAX[kind]) return false;
              cost += POTION_PRICE[kind] * qty;
              bag[kind] = (bag[kind] ?? 0) + qty;
            }
            if (lockpicks > 0) {
              if ((bag.lockpick ?? 0) + lockpicks > BAG_MAX) return false;
              cost += LOCKPICK_PRICE * lockpicks;
              bag.lockpick = (bag.lockpick ?? 0) + lockpicks;
            }
            const held = testMode ? testEmber : (rec.ember ?? 0);
            if (cost <= 0 || held < cost) return false;
            if (testMode) setTestEmber(held - cost);
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember ?? 0 : held - cost,
              emberSeeded: true,
              bags: { ...rec.bags, [hero]: bag },
              pendingMission: null,
            });
            return true;
          }}
          onBuyRations={(qty: number) => {
            if (qty <= 0) return false;
            const rec = readMapSave();
            if (!partyBagHasRoom(rec, Math.ceil((rec.rations + qty) / RATION_STACK_MAX) - Math.ceil(rec.rations / RATION_STACK_MAX), testMode)) return false;
            const cost = RATIONS_PRICE * qty;
            const held = testMode ? testEmber : (rec.ember ?? 0);
            if (held < cost) return false;
            if (testMode) setTestEmber(held - cost);
            writeMapSave({
              ...rec,
              ember: testMode ? rec.ember ?? 0 : held - cost,
              emberSeeded: true,
              rations: rec.rations + qty,
              pendingMission: null,
            });
            return true;
          }}
        />
      )}

      {screen === "cutscene" && (
        <CutsceneScreen
          src={
            missionId === "aldeia"
              ? "/game/aldeia-intro.mp4"
              : missionId === "thebridge"
                ? "/game/thebridge-intro.mp4"
                : "/game/asherah-rite.mp4"
          }
          subtitles={missionId === "aldeia" ? { pt: "/game/subtitles/aldeia-intro.pt.vtt", en: "/game/subtitles/aldeia-intro.en.vtt" } : undefined}
          onSkip={() => startBattle(missionId === "aldeia" ? "aldeia" : missionId === "thebridge" ? "thebridge" : "templo")}
        />
      )}

      {screen === "epilogue" && (
        <CutsceneScreen
          src={missionId === "portao" ? "/game/portao-end.mp4" : "/game/temple-aftermath.mp4"}
          onSkip={() => setScreen("victory")}
        />
      )}

      {(screen === "battle" || battleUnderSlots) && engine && (
        // display:contents adds no box; visibility hides the battle (and its pause menu)
        // behind the slot screen while keeping the 3D canvas at full size.
        <div style={{ display: "contents", visibility: battleUnderSlots ? "hidden" : undefined }}>
        <BattleScreen
          onUseRation={consumeRation}
          engine={engine}
          hud={hud}
          paused={paused}
          muted={muted}
          save={save}
          playtest={!!customMission}
          firstBattleHintSeen={overworldSave.flags?.includes("hint:first-battle") ?? false}
          onFirstBattleHintShown={() => {
            if (customMission) return;
            const rec = readMapSave();
            if (rec.flags?.includes("hint:first-battle")) return;
            const next = { ...rec, flags: [...(rec.flags ?? []), "hint:first-battle"] };
            writeMapSave(testMode ? next : withLiveBattle(next));
          }}
          fleeable={!customMission && !!missionId && isRandomEncounter(missionId)}
          onAffinityChange={() => {
            if (!testMode && !customMission) persistCurrent({ ...readMapSave(), affinityScores: { ...engine.affinityScores } });
          }}
          onIntroDialogShown={() => {
            if (engine.mission.introDialog) markDialogSeen(dialogSeenKey("intro", engine.mission.id, engine.mission.introDialog.id));
          }}
          onExploreConnector={(exit) => {
            if (engine.mission.id === "farmlands" && DECORATIONS[exit.id]?.exitKind === "dungeon") {
              writeMapSave({ ...withLiveBattle(readMapSave()), battle: null, pendingMission: null });
              goToMap(); return;
            }
            if (!exit.targetMapId || !isFarmlandsConnector(engine.mission.id, exit.targetMapId)) return;
            startBattle(exit.targetMapId, save.unitHp, undefined, undefined, undefined, undefined, undefined, true, engine.mission.id);
          }}
          onDialogAction={(action) => {
            if (action === "acceptSuspectHostageQuest") {
              if (testMode || customMission) return;
              const rec = readMapSave();
              const quest = questById("suspeito-watchtower-captive");
              if (!quest) return;
              const intelFlag = "suspeito-watchtower-intel";
              const flags = rec.flags ?? [];
              const discovered = rec.questsDiscovered ?? [];
              const active = rec.questsActive ?? [];
              writeMapSave({
                ...rec,
                flags: flags.includes(intelFlag) ? flags : [...flags, intelFlag],
                questsDiscovered: discovered.includes(quest.id) ? discovered : [...discovered, quest.id],
                questsActive: active.includes(quest.id) ? active : [...active, quest.id],
              });
              return;
            }
            if (action === "recruitAldric") {
              if (testMode || customMission) return;
              const rec = activeSave(bank);
              const flag = "recruited:Aldric";
              if (!(rec.flags ?? []).includes(flag)) {
                persistCurrent(withLiveBattle({ ...rec, flags: [...(rec.flags ?? []), flag] }));
              }
              return;
            }
            setInnEntry(action);
            setScreen("inn");
          }}
          outroDialogOpen={outroDialogOpen}
          onCloseOutroDialog={closeOutroDialog}
          // Gear swapped during a fight is permanent, so it lands in the save the moment it
          // happens rather than waiting for a victory that may never come. `alsoOwn` marks a
          // piece that came out of a chest this battle: the engine has already removed it
          // from the loot list, so recording ownership here is what keeps it.
          onEquipWeapon={(hero, weaponId, alsoOwn) => {
            const rec = activeSave(bank);
            if (!weaponId) {
              const next = unequipSharedWeapon(rec, hero);
              if (!partyBagHasRoom(next, 0, testMode)) return;
              persistCurrent(withLiveBattle(next));
              return;
            }
            const owned = alsoOwn && rec.weapons[weaponId] == null ? { ...rec, weapons: { ...rec.weapons, [weaponId]: 0 } } : rec;
            const next = equipSharedWeapon(owned, hero, weaponId);
            if (next) persistCurrent(withLiveBattle(next));
          }}
          onEquipItem={(hero, slot, itemId, alsoOwn) => {
            const rec = activeSave(bank);
            if (!itemId) {
              const next = unequipSharedItem(rec, hero, slot);
              if (!partyBagHasRoom(next, 0, testMode)) return;
              persistCurrent(withLiveBattle(next));
              return;
            }
            const owned = alsoOwn ? { ...rec, looseEquipment: { ...rec.looseEquipment, [itemId]: (rec.looseEquipment[itemId] ?? 0) + 1 } } : rec;
            const next = equipSharedItem(owned, hero, slot, itemId);
            if (next) persistCurrent(withLiveBattle(next));
          }}
          onAdjustStatPoint={(hero, unitId, stat, delta) => {
            const liveUnit = engine.units.find((candidate) => candidate.id === unitId && candidate.name === hero);
            if (!liveUnit) return false;
            const current = save.statPointAllocations[hero] ?? {};
            const spent = Object.values(current).reduce((total, value) => total + (value ?? 0), 0);
            const budget = Math.max(0, (liveUnit.level - 1) * STAT_POINTS_PER_LEVEL);
            if ((delta > 0 && spent >= budget) || (delta < 0 && (current[stat] ?? 0) <= 0)) return false;
            if (!engine.adjustStatPoint(unitId, stat, delta)) return false;
            const allocation: StatPointAllocation = { ...current };
            const next = (allocation[stat] ?? 0) + delta;
            if (next > 0) allocation[stat] = next;
            else delete allocation[stat];
            persistCurrent(withLiveBattle({
              ...save,
              // The engine may have leveled the hero during this battle. Saving that live
              // level together with the allocation prevents a reload from trimming its new
              // three-point budget back to the pre-battle level.
              levels: { ...save.levels, [hero]: liveUnit.level },
              xp: { ...save.xp, [hero]: liveUnit.xp },
              statPointAllocations: { ...save.statPointAllocations, [hero]: allocation },
            }));
            onHud(engine.getHud());
            return true;
          }}
          onHud={onHud}
          onPause={() => { pauseMusic(); setPaused(true); }}
          onTitle={goToTitle}
          onResume={() => {
            setSlotMode(null);
            setOverwrite(null);
            setPaused(false);
            resumeMusic();
          }}
          onMute={() => {
            unlockAudio();
            setMutedUi((v) => !v);
          }}
          onSave={() => {
            setOverwrite(null);
            setSlotReturnScreen("battle");
            setSlotMode("save");
            setScreen("saveSlots");
          }}
          onLoad={() => {
            setOverwrite(null);
            setSlotReturnScreen("battle");
            setSlotMode("load");
            setScreen("saveSlots");
          }}
          onQuit={() => {
            setPaused(false);
            setSlotMode(null);
            // A random road encounter is not a campaign chapter: fleeing it must clear
            // the resumable battle snapshot before returning to the overworld. Otherwise
            // the campaign save keeps reopening the encounter instead of letting travel
            // continue (test mode did not persist that snapshot, which hid this bug).
            if (!customMission && missionId && isRandomEncounter(missionId)) {
              const rec = activeSave(bank);
              if (!testMode) {
                persistCurrent({
                  ...rec,
                  bags: { ...rec.bags, ...engine.remainingBags() },
                  affinityScores: { ...engine.affinityScores },
                  unitHp: { ...rec.unitHp, ...engine.battlePlayerHp() },
                  heroHunger: { ...rec.heroHunger, ...engine.battlePlayerHunger() },
                  heroDiseases: mergeBattleDiseases(rec.heroDiseases, engine),
                  heroPoisons: mergeBattlePoisons(rec.heroPoisons, engine),
                  heroPoisonMag: mergeBattlePoisonMagic(rec.heroPoisonMag, engine),
                  heroSkills: structuredClone(engine.heroSkills),
                  pendingMission: null,
                  battle: null,
                });
              }
              setMissionId(null);
              setEngine(null);
              goToMap();
              return;
            }
            // Leaving the walkable Inn: record the visit and drop the resumable snapshot, so
            // the save doesn't keep reopening the Inn on "Continuar".
            if (!customMission && engine.mission.explore) {
              const rec = activeSave(bank);
              if (!testMode) {
                const id = engine.mission.id;
                persistCurrent({ ...rec, completed: rec.completed.includes(id) ? rec.completed : [...rec.completed, id], pendingMission: null, battle: null });
              }
              setMissionId(null);
              setEngine(null);
              if (engine.mission.id === "estalagem") returnToCurrentMap();
              else goToMap();
              return;
            }
            setEngine(null);
            // A playtest belongs to the editor: end it and you are back where you were,
            // with the map still loaded. Quitting a real mission still exits to the map.
            if (customMission) {
              setCustomMission(null);
              setScreen("mapEditor");
              return;
            }
            goToMap();
          }}
        />
        </div>
      )}

      {screen === "victory" && mission && (
        <ResultScreen
          win
          title={mission.title}
          body={
            mission.id === WISP_CROSSING_ID && hud.activeExit?.targetMapId === WISP_BOSS_ID
              ? "A trilha leva ao covil da Planta Carnívora. Derrotem-na para concluir a travessia."
              : DECORATIONS[hud.activeExit?.id ?? ""]?.exitKind === "dungeon"
              ? "Vocês encontraram a saída da masmorra."
              : hud.activeExit?.id === "escape-exit"
                ? "Vocês escaparam a tempo."
                : isFloorConnector(hud.activeExit)
                  ? mission.id === "estalagem"
                    ? "Subir para o Segundo andar"
                    : mission.id === "estalagem-andar-2"
                      ? "Descer para o Primeiro andar"
                      : floorConnectorDirection(hud.activeExit)
                        ? floorConnectorDirection(hud.activeExit) === "up" ? "Subindo para o andar superior." : "Descendo para o andar inferior."
                        : hud.activeExit.returnConnector
                    ? "Vocês voltam ao andar anterior."
                    : "Vocês seguem mais fundo na masmorra."
                  : "O campo ficou em silêncio."
          }
          // Floor connector only: jumps straight into the linked floor (never listed in any
          // Locais location, so onMap's normal campaign path can't reach it — see
          // WorldLocation.submaps) instead of returning to the campaign map.
          advanceLabel={hud.activeExit?.targetMapId === WISP_BOSS_ID ? "Enfrentar a Planta" : isFloorConnector(hud.activeExit) ? (floorConnectorDirection(hud.activeExit) ? floorConnectorDirection(hud.activeExit) === "up" ? "Subir" : "Descer" : hud.activeExit.returnConnector ? "Voltar" : "Avançar") : undefined}
          onAdvance={
            isFloorConnector(hud.activeExit) && hud.activeExit.targetMapId
              ? () => startBattle(hud.activeExit!.targetMapId!, save.unitHp, undefined, undefined, undefined, undefined, undefined, true, mission.id)
              : undefined
          }
          resting={!isFloorConnector(hud.activeExit)}
          turn={hud.turn}
          growth={mission.id === "estalagem" || mission.id.startsWith("estalagem-andar-") ? null : lastGrowth}
          loot={lastLoot}
          art={briefArt(mission.id)}
          innOpen={!customMission && innUnlocked(save.completed) && mission.index <= 11}
          onInn={() => {
            if (missionById("estalagem")?.explore) {
              startBattle("estalagem");
              return;
            }
            setMissionId("estalagem");
            setScreen("inn");
          }}
          onMap={() => {
            if (customMission) {
              setCustomMission(null);
              setScreen("mapEditor");
              return;
            }
            if (mission.id === "estalagem") {
              returnToCurrentMap();
              return;
            }
            // Recomputed rather than read off save.completed directly — testMode never
            // persists, so save.completed wouldn't yet include this mission there.
            const completed = completedAfterWispVictory(mission.id, save.completed);
            const loc = campaignLocations.find((location) => location.missionIds.includes(mission.id) || location.submaps?.some(s => s.missionId === mission.id));
            const scenarioDone = !loc || loc.missionIds.every((id) => completed.includes(id));
            // Mid-scenario: reopen the world map straight onto this mission's location so
            // its list pops open immediately — a multi-mission location plays as one
            // continuous series of combats instead of dropping back to the bare map after
            // every fight. Once the whole scenario is done, leave no location open — the
            // map's own centerLocationId already re-centers on wherever's next.
            setOpenLocationOnMap(scenarioDone ? null : (loc?.id ?? null));
            goToMap();
          }}
          mapLabel={customMission ? "Voltar ao editor" : "Mapa"}

          // The raw "next mission by global index" shortcut this used to offer could skip
          // straight past an entire other location (missions aren't numbered in location
          // order) — hasNext is always false below now, so this never fires; onMap is the
          // one continue path, and it's location-aware.
          onNext={() => {}}
          hasNext={false}
        />
      )}

      {screen === "victory" && mission && mission.id !== "estalagem" && !mission.id.startsWith("estalagem-andar-") && pendingPromotions.length > 0 && (
        <PromotionScreen pending={pendingPromotions} onPick={choosePromotion} />
      )}

      {screen === "defeat" && mission && (
        <ResultScreen
          win={false}
          title={mission.title}
          body="A linha quebrou."
          turn={hud.turn}
          growth={null}
          art={briefArt(mission.id)}

          onNext={() => startBattle(mission.id, save.unitHp, customMission ?? undefined)}
          onMap={
            customMission
              ? () => {
                  setCustomMission(null);
                  setScreen("mapEditor");
                }
              : undefined
          }
          mapLabel={customMission ? "Voltar ao editor" : undefined}
          hasNext
          retry
        />
      )}

      {screen === "saveSlots" && slotMode && (
        <SlotScreen
          mode={slotMode}
          bank={bank}
          overwrite={overwrite}
          onOverwrite={setOverwrite}
          onClose={() => {
            setSlotMode(null);
            setOverwrite(null);
            setScreen(slotReturnScreen);
          }}
          onPick={(index) => {
            bootAudio();
            if (slotMode === "new") {
              // A new campaign starts with cutscene dialogue at full volume, independent
              // of the previous campaign's audio mix.
              setCutsceneVolume(1);
              const next = writeSlot(bank, index, { ...emptySave(muted), pendingMission: "vau", mapMode: "rpg" });
              applySlot(next);
              setSlotMode(null);
              setOverwrite(null);
              setLastGrowth(null);
              setLastLoot([]);
              setMissionId(null);
              setEngine(null);
              setScreen("vauIntro");
              return;
            }
            if (slotMode === "continue" || slotMode === "load") {
              const rec = bank.slots[index];
              if (!rec) return;
              const next = selectSlot(bank, index);
              applySlot(next);
              setSlotMode(null);
              setOverwrite(null);
              setPaused(false);
              setEngine(null);
              enterFromSave(rec);
              return;
            }
            if (savingSlotRef.current) return;
            savingSlotRef.current = true;
            // Each step gets a painted frame before it runs, so the bar shows where the save
            // is instead of the screen freezing until the whole write is done.
            const painted = () => new Promise<void>((resolve) => requestAnimationFrame(() => requestAnimationFrame(() => resolve())));
            void (async () => {
              setSaveProgress(0);
              await painted();
              const snapshot = (() => {
                // Map saves must use the current map record. combatStartRef holds the
                // snapshot from before the last battle/map transition, so using it here
                // silently reset travel progress when a player saved from the RPG map.
                if (slotReturnScreen === "overworldMap" || slotReturnScreen === "worldMap" || slotReturnScreen === "campaign") {
                  return { ...readMapSave(), muted };
                }
                if (engine && missionId) {
                  const levels = { ...save.levels };
                  const xp = { ...save.xp };
                  for (const u of engine.units) {
                    if (u.side !== "player" || u.summoned) continue;
                    levels[u.name] = u.level;
                    xp[u.name] = u.xp;
                  }
                  return {
                    ...save,
                    pendingMission: missionId,
                    battle: engine.captureSnapshot(),
                    bags: { ...save.bags, ...engine.remainingBags() },
                    affinityScores: { ...engine.affinityScores },
                    heroSkills: structuredClone(engine.heroSkills),
                    unitHp: { ...save.unitHp, ...engine.battlePlayerHp() },
                    heroHunger: { ...save.heroHunger, ...engine.battlePlayerHunger() },
                    spellUses: engine.spentTiers(),
                    levels,
                    xp,
                    muted,
                  };
                }
                return combatStartRef.current ?? { ...save, pendingMission: missionId, muted, battle: save.battle ?? null };
              })();
              setSaveProgress(50);
              await painted();
              const next = writeSlot(bank, index, snapshot);
              setSaveProgress(100);
              await painted();
              applySlot(next);
              setSlotMode(null);
              setOverwrite(null);
              setPaused(false);
              setScreen(slotReturnScreen);
            })().finally(() => {
              savingSlotRef.current = false;
              setSaveProgress(null);
            });
          }}
          saveProgress={saveProgress}
        />
      )}
      {!testMode && (screen === "worldMap" || screen === "overworldMap" || screen === "campaign") && <>
        <button type="button" className="absolute bottom-4 right-4 z-30 ember-btn ember-btn-sm" onClick={() => setCompanionConversationsOpen(true)}>Conversations</button>
        {companionConversationsOpen && <CompanionConversations
          save={save}
          leader={partyLeaderOf(overworldSave.partyLeader, h => testMode || heroRecruited(h, overworldSave.completed, overworldSave.flags))}
          heroes={AFFINITY_HEROES.filter(h => testMode || heroRecruited(h, overworldSave.completed, overworldSave.flags))}
          onClose={() => setCompanionConversationsOpen(false)}
          onLeader={hero => writeMapSave({ ...readMapSave(), partyLeader: cleanPartyLeader(hero) })}
          onReply={(reply, leader) => {
            const rec = readMapSave();
            const resolved = resolveCompanionReply(rec.affinityScores, rec.companionConversations, reply, leader);
            persistCurrent({ ...rec, affinityScores: resolved.scores, companionConversations: resolved.memory });
          }}
        />}
      </>}
    </main>
  );
}

export function CutsceneScreen({
  src,
  subtitles,
  onSkip,
}: {
  src: string;
  subtitles?: Translations;
  onSkip: () => void;
}) {
  const prefs = useGamePreferences();
  const ref = useRef<HTMLVideoElement>(null);
  // Cutscene audio is an independent persisted setting; the game's master mute does not touch it.
  const [cutsceneSoundEnabled, setCutsceneSoundEnabled] = useState(() => getCutsceneAudioSettings().enabled);
  const subtitleTracks = CUTSCENE_SUBTITLES[src] ?? subtitles;
  const selectedSubtitle = prefs.subtitles ? subtitleTracks?.[prefs.subtitleLanguage] : undefined;
  useEffect(() => {
    syncSubtitles(ref.current?.textTracks, prefs.subtitles, prefs.subtitleLanguage);
  }, [prefs.subtitles, prefs.subtitleLanguage, selectedSubtitle]);
  const [portrait, setPortrait] = useState(
    () => typeof window !== "undefined" && window.matchMedia("(max-width: 720px) and (orientation: portrait)").matches,
  );
  useEffect(() => {
    const mq = window.matchMedia("(max-width: 720px) and (orientation: portrait)");
    const sync = () => setPortrait(mq.matches);
    sync();
    mq.addEventListener("change", sync);
    const ori = screen.orientation as ScreenOrientation & { lock?: (mode: string) => Promise<void>; unlock?: () => void };
    void ori.lock?.("landscape").catch(() => {});
    return () => {
      mq.removeEventListener("change", sync);
      try {
        ori.unlock?.();
      } catch {
        /* ignore */
      }
    };
  }, []);
  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const settings = getCutsceneAudioSettings();
    el.volume = settings.volume;
    el.muted = !settings.enabled;
    setCutsceneSoundEnabled(settings.enabled);
    const syncSound = () => setCutsceneSoundEnabled(getCutsceneAudioSettings().enabled);
    el.addEventListener("volumechange", syncSound);
    let stuckTimer = 0;
    const clearStuckTimer = () => {
      if (stuckTimer) {
        window.clearTimeout(stuckTimer);
        stuckTimer = 0;
      }
    };
    // The only auto-skip left: a genuinely broken/blocked video that never actually starts
    // playing. Anything that does start plays all the way to its own end (onEnded) or until
    // "Pular" is clicked — never cut off by a blind clock partway through.
    const armStuckTimer = () => {
      clearStuckTimer();
      stuckTimer = window.setTimeout(() => {
        if (el.paused) onSkip();
      }, 8000);
    };
    const kick = () => {
      armStuckTimer();
      void el.play().catch(() => {
        el.muted = true;
        void el.play().catch(() => {});
      });
    };
    kick();
    el.addEventListener("canplay", kick);
    el.addEventListener("playing", clearStuckTimer);
    return () => {
      el.removeEventListener("canplay", kick);
      el.removeEventListener("playing", clearStuckTimer);
      el.removeEventListener("volumechange", syncSound);
      clearStuckTimer();
    };
  }, [src, onSkip]);
  return (
    <section className="relative h-dvh w-dvw bg-black overflow-hidden">
      <div className="cutscene-stage">
        <video ref={ref} src={src} playsInline autoPlay preload="auto" data-ember-cutscene onEnded={onSkip} onError={onSkip}>
          {selectedSubtitle && <track key={selectedSubtitle} kind="subtitles" src={selectedSubtitle} srcLang={prefs.subtitleLanguage === "pt" ? "pt-BR" : "en"} label={prefs.subtitleLanguage === "pt" ? "Português (Brasil)" : "English"} default onLoad={() => syncSubtitles(ref.current?.textTracks, prefs.subtitles, prefs.subtitleLanguage)} />}
        </video>
      </div>
      {portrait && (
        <p className="pointer-events-none absolute inset-x-0 top-[max(0.75rem,env(safe-area-inset-top))] text-center text-[11px] tracking-[0.16em] uppercase text-muted">
          {uiText("Deite o telefone")}
        </p>
      )}
      <div className="absolute right-3 top-[max(0.75rem,env(safe-area-inset-top))] z-20 flex gap-1 p-2">
        <button
          type="button"
          className="grid size-9 place-items-center rounded bg-black/40 text-white/90"
          aria-label={uiText(cutsceneSoundEnabled ? "Silenciar cutscenes" : "Ativar som das cutscenes")}
          aria-pressed={cutsceneSoundEnabled}
          onClick={() => {
            const video = ref.current;
            if (!video) return;
            // Browsers can force-mute an autoplay attempt. In that case this click resumes
            // the enabled setting; otherwise it toggles the independent cutscene mute.
            const enable = cutsceneSoundEnabled && video.muted ? true : !cutsceneSoundEnabled;
            setCutsceneEnabled(enable);
            setCutsceneSoundEnabled(getCutsceneAudioSettings().enabled);
            if (enable) void video.play().catch(() => {});
          }}
        >
          {cutsceneSoundEnabled ? <Volume2 className="size-4" /> : <VolumeX className="size-4" />}
        </button>
      </div>
      <div className="absolute inset-x-0 bottom-0 z-10 p-5 pb-[max(1.25rem,env(safe-area-inset-bottom))] flex justify-end">
        <Button size="md" variant="ghost" onClick={onSkip}>
          {uiText("Pular")}
        </Button>
      </div>
    </section>
  );
}

function TitleScreen({
  ready,
  error,
  hasProgress,
  muted,
  help,
  onMute,
  onHelp,
  onNew,
  onContinue,
  onTest,
}: {
  ready: boolean;
  error: string | null;
  hasProgress: boolean;
  muted: boolean;
  help: boolean;
  onMute: () => void;
  onHelp: () => void;
  onNew: () => void;
  onContinue: () => void;
  onTest: () => void;
}) {
  useGamePreferences();
  const progress = useArtLoadProgress(ready);
  return (
    <section className="relative min-h-dvh flex flex-col overflow-hidden">
      <div className="title-hero absolute inset-0" aria-hidden />
      <div className="title-veil absolute inset-0" />
      <header className="relative z-10 flex items-center justify-between px-4 pt-[max(1.25rem,env(safe-area-inset-top))]">
        <span className="text-[0.95rem] tracking-[0.18em] text-[#dfbf8e]" style={{ fontFamily: '"Pirata One", var(--font-display)' }}>Ember Ashes</span>
        <button
          type="button"
          onClick={onMute}
          className="size-11 grid place-items-center ember-icon-btn"
          aria-label={uiText(muted ? "Ativar som" : "Silenciar")}
        >
          {muted ? <VolumeX className="size-5" /> : <Volume2 className="size-5" />}
        </button>
      </header>
      {/* Deliberately tiny and tucked in a corner away from the main menu column — a dev/QA
          entry point, not something a player should ever tap by accident reaching for
          "Nova campanha" or "Continuar". */}
      <button
        type="button"
        onClick={onTest}
        className="absolute z-10 bottom-2 left-2 px-1.5 py-0.5 text-[10px] uppercase tracking-wide text-muted/60 hover:text-muted disabled:opacity-40"
      >
        Modo teste
      </button>
      {/* Menu column lifted to leave room for the loading bar underneath it; the tiny "Modo teste"
          button stays where it was, bottom-left. */}
      <div className="relative z-10 flex flex-1 flex-col justify-end px-5 pb-[max(6.5rem,calc(env(safe-area-inset-bottom)+5.5rem))] max-w-xl mx-auto w-full">
        <p className="text-sm tracking-[0.12em] text-[#dfbf8e] mb-3">{uiText("Táticas em cinzas")}</p>
        <h1 className="font-display text-[#dfbf8e] text-2xl sm:text-3xl font-medium tracking-tight leading-none mb-4">Ember</h1>
        <p className="text-[11px] tracking-[0.18em] text-[#dfbf8e] -mt-3 mb-4">Version {DISPLAY_VERSION}</p>
        <p className="text-[#dfbf8e] text-base leading-relaxed mb-8 max-w-md">
          {uiText("Seis sobreviventes. Um tabuleiro de guerra. Cada casa conta.")}
        </p>
        <div className="flex flex-col gap-3">
          <Button size="xl" className="ember-btn ember-btn-primary" disabled={!ready} onClick={onNew}>
            {uiText(ready ? "Nova campanha" : "Carregando…")}
          </Button>
          {hasProgress && (
            <Button size="lg" variant="ghost" className="ember-btn ember-btn-ghost" disabled={!ready} onClick={onContinue}>
              {uiText("Continuar")}
            </Button>
          )}
          <Button size="lg" variant="quiet" className="ember-btn ember-btn-ghost" onClick={onHelp}>
            {uiText("Como jogar")}
          </Button>
        </div>
        <div className="mt-3"><OptionsButton muted={muted} onMute={onMute} /></div>
        {error && <p className="mt-4 text-sm text-danger">{error}</p>}
      </div>
      <TitleLoader progress={progress} ready={ready} />
      {help && <HelpModal onClose={onHelp} />}
    </section>
  );
}

// LOCKED (2026-10-06): the title loading bar — real % only, smooth fill, no stripes, never an
// indeterminate/fake sweep. Do not change; see "Loading bars" in CLAUDE.md's Locked behavior.
/** Real art-loading progress for the title screen, kept monotonic (the raw ratio can dip when a
 * later batch of images is requested) and never shown as 100% before loading has truly finished. */
function useArtLoadProgress(ready: boolean): number {
  const raw = useSyncExternalStore(subscribeArtProgress, artProgress, () => 0);
  const peak = useRef(0);
  peak.current = Math.max(peak.current, ready ? 1 : Math.min(raw, 0.97));
  return peak.current;
}

const LOADER_EMBERS = [
  { x: 8, d: 0, t: 3.4 },
  { x: 19, d: 1.1, t: 4.1 },
  { x: 31, d: 2.2, t: 3.7 },
  { x: 44, d: 0.5, t: 4.4 },
  { x: 57, d: 1.7, t: 3.5 },
  { x: 68, d: 2.9, t: 4.0 },
  { x: 79, d: 0.9, t: 3.8 },
  { x: 90, d: 2.4, t: 4.3 },
];

/** Dark-fantasy loading bar under the title menu: a gothic-framed trough filled with molten
 * ember, a flickering spark at the leading edge and embers drifting off it. Fades away a moment
 * after loading finishes. */
function TitleLoader({ progress, ready }: { progress: number; ready: boolean }) {
  const [leaving, setLeaving] = useState(false);
  const [gone, setGone] = useState(false);
  useEffect(() => {
    if (!ready) return;
    const fade = window.setTimeout(() => setLeaving(true), 900);
    const remove = window.setTimeout(() => setGone(true), 1900);
    return () => {
      window.clearTimeout(fade);
      window.clearTimeout(remove);
    };
  }, [ready]);
  if (gone) return null;
  const pct = Math.round(progress * 100);
  return (
    <div className={`title-loader${ready ? " is-ready" : ""}${leaving ? " is-leaving" : ""}`} role="progressbar" aria-label="Carregando" aria-valuemin={0} aria-valuemax={100} aria-valuenow={pct}>
      <div className="title-loader-label">
        <span>{ready ? "" : "Despertando as cinzas"}</span>
        <span className="title-loader-pct">{pct}%</span>
      </div>
      <div className="title-loader-frame">
        <div className="title-loader-track">
          <div className="title-loader-fill" style={{ width: `${pct}%` }}>
            <span className="title-loader-spark" />
          </div>
        </div>
        <span className="title-loader-cap title-loader-cap-l" aria-hidden />
        <span className="title-loader-cap title-loader-cap-r" aria-hidden />
        <div className="title-loader-embers" aria-hidden>
          {LOADER_EMBERS.map((e, i) => (
            <i key={i} style={{ left: `${Math.min(e.x, Math.max(2, pct))}%`, animationDelay: `${e.d}s`, animationDuration: `${e.t}s` }} />
          ))}
        </div>
      </div>
    </div>
  );
}

/** One entry per casting-speed tier: which classes share it, one classId to read the table
 * from (every class in the group has identical numbers), and how many tiers it goes up to.
 * Class names, not hero names — keeps it about the role, not who's playing it. */
const SKILL_SPEED_GROUPS: { label: string; classes: string; classId: ClassId; maxTier: number }[] = [
  {
    label: "Conjuração Rápida",
    classes: ["mage", "conjurer", "healer", "elementalist", "sorcerer", "bishop"].map((c) => CLASSES[c as ClassId].name).join(", "),
    classId: "mage",
    maxTier: 10,
  },
  {
    label: "Conjuração Média",
    classes: ["archer", "warlock", "necromancer", "cleric", "paladin", "assassin", "templar"].map((c) => CLASSES[c as ClassId].name).join(", "),
    classId: "archer",
    maxTier: 8,
  },
  {
    label: "Conjuração Lenta",
    classes: ["swordsman", "lancer", "aldric", "heavyKnight", "ranger", "sentinel"].map((c) => CLASSES[c as ClassId].name).join(", "),
    classId: "swordsman",
    maxTier: 6,
  },
];

/** One row per potion in the loot-weighted pick (weightedPotionPick) — % chance is its share
 * of the total weight across every potion, so this always matches what's actually rolled. */
const POTION_LOOT_ROWS: { name: string; weight: number }[] = (Object.entries(POTION_LOOT_WEIGHT) as [keyof typeof POTION_LOOT_WEIGHT, number][]).map(
  ([id, weight]) => ({ name: POTIONS[id].name, weight }),
);
const POTION_LOOT_TOTAL = POTION_LOOT_ROWS.reduce((n, r) => n + r.weight, 0);

/** One entry per skill/spell tier slot (SPELL_TIER's own keys) — tier, damage formula (a
 * plain string for flat skills, or a function of the caster's MAG for the ones that scale),
 * and a one-line effect note. Kept next to SPELL_TIER by hand since a skill's shape
 * (splash, line, status effect) isn't data SPELL_TIER itself carries.
 *
 * The scaling ones take MAG rather than level: a spell weights the caster's own power now
 * instead of growing on a table of its own. */
/** Which base class a skill belongs to — inverted from classSpells (promoted classes keep
 * their base class's list, so this only ever needs the six base owners). */
const SKILL_CLASS: Partial<Record<SpellKind, ClassId>> = {
  doubleStrike: "swordsman",
  provoke: "swordsman",
  cleave: "swordsman",
  magicMissile: "mage",
  lightning: "mage",
  lightningTier3: "elementalist",
  fireball: "mage",
  warp: "mage",
  iceStorm: "mage",
  frost: "mage",
  causticVenom: "mage",
  divineBolt: "healer",
  turnUndead: "healer",
  poisonBreath: "mage",
  summonFamiliar: "conjurer",
  summonFamiliar2: "conjurer",
  summonFamiliar3: "conjurer",
  summonFamiliar4: "conjurer",
  summonZombieDog: "conjurer",
  webOfDreams: "conjurer",
  longShot: "archer",
  bloodyShot: "archer",
  piercing: "archer",
  cureMinor: "healer",
  bless: "healer",
  cureWounds: "healer",
  cureDisease: "healer",
  piercingThrust: "lancer",
  sweep: "lancer",
  trip: "lancer",
  multiShot: "archer",
  secondWind: "paladin",
  cureLight: "paladin",
  auraOfProtection: "paladin",
  divineWrath: "paladin",
  shoulderSmash: "heavyKnight",
  intimidatingPresence: "heavyKnight",
  stampede: "heavyKnight",
};

const SKILL_DAMAGE_ROWS: { name: string; cls: ClassId; tier: SpellTier; formula: string | ((x: number) => string); param?: "level"; progression?: { levels: string; range: number; size: number; duration: number; damage: string }[]; note: string }[] = [
  { name: BLESS.name, cls: "healer" as const, tier: spellTier("bless")!, formula: "—", note: `Healer nível ${BLESS.unlockLevel}. Raio ${BLESS.radius}; +1% de acerto por nível até +10% no nível 13. Duração: 3 turnos no nível 3; 4 no 5; 5 no 7; 6 no 9; 7 no 12; 8 no 15.` },
  { name: MAGIC_MISSILE.name, cls: SKILL_CLASS.magicMissile!, tier: spellTier("magicMissile")!, formula: (mag: number) => spellFormula(mag, MAGIC_MISSILE.mul, MAGIC_MISSILE.dice, MAGIC_MISSILE.faces, MAGIC_MISSILE.bonus), note: "Nunca erra. 1 míssil, 2 no nível 3, 3 no nível 6 — um alvo cada." },
  {
    name: LONG_SHOT.name,
    cls: SKILL_CLASS.longShot!,
    tier: spellTier("longShot")!,
    formula: (level: number) => longShotFormula(level),
    param: "level" as const,
    note: `Alcance 7. Dado sobe em níveis 2,3,5,7,9,12,14.`,
  },
  {
    name: BLOODY_SHOT.name,
    cls: SKILL_CLASS.bloodyShot!,
    tier: spellTier("bloodyShot")!,
    formula: (level: number) => bloodyShotFormula(level),
    param: "level" as const,
    note: "Aprendido no nível 5. Sangramento causa 1D8 por ação; a duração aumenta nos níveis 8, 12 e 15.",
  },
  { name: CURES.cureMinor.name, cls: SKILL_CLASS.cureMinor!, tier: spellTier("cureMinor")!, formula: (mag: number) => `${healFormula(mag, "cureMinor")} (cura)`, note: "—" },
  {
    name: DOUBLE_STRIKE.name,
    cls: SKILL_CLASS.doubleStrike!,
    tier: spellTier("doubleStrike")!,
    formula: (level: number) => doubleStrikeFormula(level),
    param: "level" as const,
    note: "Ataca duas vezes; cada acerto rola seu próprio bônus (não acumula).",
  },
  {
    name: PROVOKE.name,
    cls: SKILL_CLASS.provoke!,
    tier: spellTier("provoke")!,
    formula: (level: number) => provokeFormula(level),
    param: "level" as const,
    note: "Aprendido no nível 3. Sem dano: os inimigos atingidos passam a mirar o guerreiro até alguém gerar mais inimizade. Vira área de raio 1 no nível 6, 2 no 9, 3 no 12 e 4 no 15; o alcance sobe de 4 até 8.",
  },
  { name: PIERCING_THRUST.name, cls: SKILL_CLASS.piercingThrust!, tier: spellTier("piercingThrust")!, formula: `dano de arma, −${Math.round(PIERCING_THRUST.armorIgnore * 100)}% armadura`, note: "Acerta em linha; o segundo alvo recebe metade." },
  { name: SUMMON_FAMILIAR.name, cls: SKILL_CLASS.summonFamiliar!, tier: spellTier("summonFamiliar")!, formula: "—", note: `Invoca aliado com ${Math.round(SUMMON_FAMILIAR.statScale * 100)}% dos atributos atuais — pode lançar Míssil Mágico por conta própria.` },
  { name: SUMMON_FAMILIAR2.name, cls: SKILL_CLASS.summonFamiliar2!, tier: spellTier("summonFamiliar2")!, formula: "—", note: `Invoca aliado maior, com ${Math.round(SUMMON_FAMILIAR2.statScale * 100)}% dos atributos atuais — pode lançar Míssil Mágico ou Dreno de Vida por conta própria.` },
  { name: SUMMON_FAMILIAR4.name, cls: SKILL_CLASS.summonFamiliar4!, tier: spellTier("summonFamiliar4")!, formula: "—", note: `Invoca aliado radiante, com ${Math.round(SUMMON_FAMILIAR4.statScale * 100)}% dos atributos atuais — pode lançar Míssil Mágico ou Dreno de Vida por conta própria.` },
  { name: SUMMON_FAMILIAR3.name, cls: SKILL_CLASS.summonFamiliar3!, tier: spellTier("summonFamiliar3")!, formula: "—", note: `Invoca aliado com ${Math.round(SUMMON_FAMILIAR3.statScale * 100)}% dos atributos atuais — pode lançar Bola de Fogo por conta própria.` },
  {
    name: LIGHTNING.name,
    cls: SKILL_CLASS.lightning!,
    tier: spellTier("lightning")!,
    formula: (mag: number) => lightningFormula(mag),
    note: `Atravessa cobertura e barricadas. Eco em outro alvo adjacente: ${diceFormula(LIGHTNING.echoDice, LIGHTNING.echoFaces, LIGHTNING.echoBonus)}.`,
  },
  {
    name: LIGHTNING_T3.name,
    cls: SKILL_CLASS.lightningTier3!,
    tier: spellTier("lightningTier3")!,
    formula: (mag: number) => lightningTier3Formula(mag),
    note: `Elementalista T5. Atravessa cobertura e barricadas. Eco ${diceFormula(LIGHTNING_T3.echoDice, LIGHTNING_T3.echoFaces, LIGHTNING_T3.echoBonus)}.`,
  },
  {
    name: PIERCING.name,
    cls: SKILL_CLASS.piercing!,
    tier: spellTier("piercing")!,
    formula: (level: number) => `${piercingMul(level)}× dano de arma`,
    param: "level" as const,
    note: "Multiplicador sobe nos níveis 6, 10 e 13.",
  },
  { name: CURES.cureWounds.name, cls: SKILL_CLASS.cureWounds!, tier: spellTier("cureWounds")!, formula: (mag: number) => `${healFormula(mag, "cureWounds")} (cura)`, note: "—" },
  {
    name: CLEAVE.name,
    cls: SKILL_CLASS.cleave!,
    tier: spellTier("cleave")!,
    formula: (level: number) => cleaveFormula(level),
    param: "level" as const,
    note: `Atinge até ${CLEAVE.hexes} hexes. x${CLEAVE.largeMul} em criaturas grandes (${CLEAVE.largeHexes}+ hexes). Dado sobe nos níveis 9, 11 e 14.`,
  },
  { name: SWEEP.name, cls: SKILL_CLASS.sweep!, tier: spellTier("sweep")!, formula: "dano de arma", note: `Inimigos a até ${SWEEP.radius} hexes; empurra ${SWEEP.knockback} hex. Prévia da área antes de confirmar.` },
  {
    name: WEB_OF_DREAMS.name,
    cls: SKILL_CLASS.webOfDreams!,
    tier: spellTier("webOfDreams")!,
    formula: "—",
    note: `Alcance ${WEB_OF_DREAMS.range}. Raio ${WEB_OF_DREAMS.size} (2 no nível 7, 3 no nível 12). ${Math.round(WEB_OF_DREAMS.sleepChance * 100)}% de dormir por ${diceFormula(WEB_OF_DREAMS.sleepDice, WEB_OF_DREAMS.sleepFaces, 0)} turnos (+${Math.round(WEB_OF_DREAMS.sleepBonusDamage * 100)}% dano ao acordar); prende o movimento a 1 hex na área por ${WEB_OF_DREAMS.durationRounds} turnos.`,
  },
  { name: WARP.name, cls: SKILL_CLASS.warp!, tier: spellTier("warp")!, formula: "—", note: "Aprendido no nível 7. Em combate, abre um portal para outro ponto do mapa; aliados próximos acompanham. Em masmorras, leva o grupo próximo de volta à entrada. O alcance do grupo e o tempo do portal aumentam gradualmente com o nível." },
  { name: TRIP.name, cls: SKILL_CLASS.trip!, tier: spellTier("trip")!, formula: `arma +${diceFormula(1, TRIP.bonusFaces, TRIP.bonusBonus)}`, note: `Causa Sangramento (1D8 a cada ação); −${Math.round(TRIP.statPenalty * 100)}% de status pro resto da batalha.` },
  {
    name: FIREBALL.name,
    cls: SKILL_CLASS.fireball!,
    tier: spellTier("fireball")!,
    formula: (mag: number) => fireballFormula(mag),
    note: `Área de raio ${FIREBALL.size}.`,
  },
  { name: POISON_BREATH.name, cls: SKILL_CLASS.poisonBreath!, tier: spellTier("poisonBreath")!, formula: (mag: number) => poisonBreathFormula(POISON_BREATH.unlockLevel, mag), note: "Aprendido no nível 3; progressão começa no nível 3. Cone de raio 1–5. Progressão de dano de Mãos Flamejantes atrasada em 2 níveis. Veneno Menor: 1D4 por turno; atinge aliados também." },
  { name: FROST.name, cls: "mage" as const, tier: 2 as const, formula: (mag:number)=>spellFormula(mag,FROST.mul,1,6,0), note:"Aprendido no nível 5. Linha de 2 hexes à frente; +1 hex a cada 4 níveis. Dano de Ice e fogo amigo. Cultist V2: 1 carga inicial, +1 nos níveis 5, 12 e 20." },
  {
    name: TURN_UNDEAD.name, cls: "healer" as const, tier: 3 as const,
    formula: "See the full progression beside it.",
    progression: TURN_UNDEAD_PROGRESSION.map((p, i) => ({
      levels: i === TURN_UNDEAD_PROGRESSION.length - 1 ? "22–30 (capped at 22)" : `${p.level}–${TURN_UNDEAD_PROGRESSION[i + 1]!.level - 1}`,
      range: p.range, size: turnUndeadPower(p.level).areaHexes, duration: TURN_UNDEAD.fearTurns,
      damage: `⌊⌊MAG / 2⌋ × ${p.mul}⌋ + ${p.dice}D${p.faces}`,
    })),
    note: "Preview the area around the priest, then confirm or cancel; no target selection. Holy damage and 2 own turns of fear: undead retreat without attacking. Radius grows at every breakpoint from 4 to 9 hexes (61–271 affected cells). Other creature types are completely unaffected.",
  },
  { name: CURE_DISEASE.name, cls: SKILL_CLASS.cureDisease!, tier: spellTier("cureDisease")!, formula: "—", note: "Clériga T3. Cura doença e veneno. Luz teal." },
  {
    name: ICE_STORM.name,
    cls: SKILL_CLASS.iceStorm!,
    tier: spellTier("iceStorm")!,
    formula: "Veja a progressão completa ao lado.",
    progression: [8, 12, 16, 20, 24, 28].map((level) => {
      const power = iceStormPower(level);
      return { levels: `${level}–${Math.min(level + 3, 30)}`, range: power.range, size: power.areaHexes, duration: power.durationRounds, damage: `⌊⌊MAG / 2⌋ × ${power.mul}⌋ + ${power.dice}D${power.faces}` };
    }),
    note: "A cada turno da unidade dentro da área, causa dano de Ice, reduzido pela resistência a Ice. Atinge aliados também. A área fica no campo pelo número de rodadas indicado.",
  },
  {
    name: CAUSTIC_VENOM.name,
    cls: SKILL_CLASS.causticVenom!,
    tier: spellTier("causticVenom")!,
    formula: (mag: number) =>
      `centro ${spellFormula(mag, CAUSTIC_VENOM.centerMul, CAUSTIC_VENOM.centerDice, CAUSTIC_VENOM.centerFaces, CAUSTIC_VENOM.centerBonus)} · respingo ${spellFormula(mag, CAUSTIC_VENOM.splashMul, CAUSTIC_VENOM.splashDice, CAUSTIC_VENOM.splashFaces, CAUSTIC_VENOM.splashBonus)}`,
    note: `Alcance ${CAUSTIC_VENOM.range}. ${POISON_TIERS.poison.name}: ${poisonDice("poison")} no início de cada turno do alvo, até curado (−10 pontos de Poison Resistance). Área de raio ${CAUSTIC_VENOM.size}, pega os dois lados.`,
  },
  {
    name: DIVINE_BOLT.name,
    cls: SKILL_CLASS.divineBolt!,
    tier: spellTier("divineBolt")!,
    formula: (mag: number) => `centro ${spellFormula(mag, DIVINE_BOLT.centerMul, DIVINE_BOLT.centerDice, DIVINE_BOLT.centerFaces, DIVINE_BOLT.centerBonus)} · adjacente ${spellFormula(mag, DIVINE_BOLT.splashMul, DIVINE_BOLT.splashDice, DIVINE_BOLT.splashFaces, DIVINE_BOLT.splashBonus)}`,
    note: `Somente Salazar. Alcance ${DIVINE_BOLT.range}; raio ${DIVINE_BOLT.size}; dano Holy e atinge até seis hexes adjacentes.`,
  },
  {
    name: MULTI_SHOT.name,
    cls: SKILL_CLASS.multiShot!,
    tier: spellTier("multiShot")!,
    formula: (level: number) => multiShotFormula(level),
    param: "level" as const,
    note: `2 alvos (3 no nível 11), alcance 6. Dado sobe no nível 8 e 13.`,
  },
  {
    name: SECOND_WIND.name,
    cls: SKILL_CLASS.secondWind!,
    tier: spellTier("secondWind")!,
    formula: (level: number) => `${Math.round(secondWindPct(level) * 100)}% de DEX`,
    param: "level" as const,
    note: `Passiva: cura sozinho ao cair a ${Math.round(SECOND_WIND.badlyWoundedPct * 100)}% de HP ou menos. Não é um golpe do atalho.`,
  },
  { name: CURES.cureLight.name, cls: SKILL_CLASS.cureLight!, tier: spellTier("cureLight")!, formula: (mag: number) => `${healFormula(mag, "cureLight")} (cura)`, note: "Entre Cura Menor e Cura Média; usos próprios do Paladino." },
  {
    name: AURA_OF_PROTECTION.name,
    cls: SKILL_CLASS.auraOfProtection!,
    tier: spellTier("auraOfProtection")!,
    formula: (level: number) => {
      const p = auraPower(level);
      return `raio ${p.radius}, −${Math.round(p.pct * 100)}% dano, ${p.duration} rodadas`;
    },
    param: "level" as const,
    note: "Instantânea, centrada em si mesmo — sem mira. Escala nos níveis 20, 22, 24, 26, 28 e 30.",
  },
  {
    name: DIVINE_WRATH.name,
    cls: SKILL_CLASS.divineWrath!,
    tier: spellTier("divineWrath")!,
    formula: (level: number) => {
      const p = divineWrathPower(level);
      return `arma + MAG/2 + ${diceFormula(p.dice, p.faces, 0)}`;
    },
    param: "level" as const,
    note: `Linha reta mirada, alcance ${DIVINE_WRATH.range} — nunca atinge aliados. Dado sobe nos níveis 19, 22, 26 e 30.`,
  },
  {
    name: SHOULDER_SMASH.name,
    cls: SKILL_CLASS.shoulderSmash!,
    tier: spellTier("shoulderSmash")!,
    formula: (level: number) => shoulderSmashFormula(level),
    param: "level" as const,
    note: `Requer sem escudo. Arco de hexes cresce até 4; empurra ${SHOULDER_SMASH.knockback} hexes.`,
  },
  {
    name: INTIMIDATING_PRESENCE.name,
    cls: SKILL_CLASS.intimidatingPresence!,
    tier: spellTier("intimidatingPresence")!,
    formula: (level: number) => {
      const p = auraPower(level);
      return `raio ${p.radius}, +${Math.round(p.pct * 100)}% dano, ${p.duration} rodadas`;
    },
    param: "level" as const,
    note: "Instantânea, centrada em si mesmo — o oposto da Aura de Proteção, mesma escala.",
  },
  {
    name: STAMPEDE.name,
    cls: SKILL_CLASS.stampede!,
    tier: spellTier("stampede")!,
    formula: (level: number) => stampedeFormula(level),
    param: "level" as const,
    note: `Linha reta mirada, alcance ${STAMPEDE.range} — atinge aliados também. Dado sobe nos níveis 21, 24, 27 e 30.`,
  },
].sort((a, b) => a.tier - b.tier);

const SKILL_DAMAGE_NOTES_EN: Record<string, string> = {
  [BLESS.name]: `Healer, level ${BLESS.unlockLevel}. Radius ${BLESS.radius}; +1% hit chance per level, up to +10% at level 13. Duration: 3 turns at level 3; 4 at 5; 5 at 7; 6 at 9; 7 at 12; 8 at 15.`,
  [MAGIC_MISSILE.name]: "Never misses. 1 missile, 2 at level 3, 3 at level 6 — each targets one unit.",
  [LONG_SHOT.name]: "Range 7. Damage die increases at levels 2, 3, 5, 7, 9, 12, and 14.",
  [PROVOKE.name]: "Warrior Tier 1, learned at level 3. No damage: every enemy it reaches turns on the warrior until someone generates more enmity. Becomes an area of radius 1 at level 6, 2 at 9, 3 at 12 and 4 at 15; reach grows from 4 to 8.",
  [BLOODY_SHOT.name]: "Archer Tier 2, learned at level 5. Deals weapon damage with a level-based multiplier and causes 1D8 bleeding damage per action for 1D6 rounds (1D8 at level 8, 2D4 at level 12, 2D6 at level 15).",
  [CURES.cureWounds.name]: "—",
  [CURES.cureMinor.name]: "—",
  [DOUBLE_STRIKE.name]: "Attacks twice; each hit rolls its own bonus (bonuses do not stack).",
  [PIERCING_THRUST.name]: "Hits in a line; the second target takes half damage.",
  [SUMMON_FAMILIAR.name]: `Summons an ally with ${Math.round(SUMMON_FAMILIAR.statScale * 100)}% of your current stats. It can cast Magic Missile on its own.`,
  [SUMMON_FAMILIAR2.name]: `Summons a greater ally with ${Math.round(SUMMON_FAMILIAR2.statScale * 100)}% of your current stats. It can cast Magic Missile or Life Drain on its own.`,
  [SUMMON_FAMILIAR4.name]: `Summons a radiant ally with ${Math.round(SUMMON_FAMILIAR4.statScale * 100)}% of your current stats. It can cast Magic Missile or Life Drain on its own.`,
  [SUMMON_FAMILIAR3.name]: `Summons an ally with ${Math.round(SUMMON_FAMILIAR3.statScale * 100)}% of your current stats. It can cast Fireball on its own.`,
  [LIGHTNING.name]: `Pierces cover and barricades. Echoes to another adjacent target for ${diceFormula(LIGHTNING.echoDice, LIGHTNING.echoFaces, LIGHTNING.echoBonus)}.`,
  [LIGHTNING_T3.name]: `Elementalist T5. Pierces cover and barricades. Echo: ${diceFormula(LIGHTNING_T3.echoDice, LIGHTNING_T3.echoFaces, LIGHTNING_T3.echoBonus)}.`,
  [PIERCING.name]: "Multiplier increases at levels 6, 10, and 13.",
  [CURES.cureWounds.name]: "—",
  [CLEAVE.name]: `Hits up to ${CLEAVE.hexes} hexes. x${CLEAVE.largeMul} against large creatures (${CLEAVE.largeHexes}+ hexes). Damage die increases at levels 9, 11, and 14.`,
  [SWEEP.name]: `Enemies within ${SWEEP.radius} hexes; knocks them back ${SWEEP.knockback} hex. Shows the area before confirming.`,
  [WEB_OF_DREAMS.name]: `Range ${WEB_OF_DREAMS.range}. Radius ${WEB_OF_DREAMS.size} (2 at level 7, 3 at level 12). ${Math.round(WEB_OF_DREAMS.sleepChance * 100)}% chance to sleep for ${diceFormula(WEB_OF_DREAMS.sleepDice, WEB_OF_DREAMS.sleepFaces, 0)} turns (+${Math.round(WEB_OF_DREAMS.sleepBonusDamage * 100)}% damage when awakened); movement is limited to 1 hex in the area for ${WEB_OF_DREAMS.durationRounds} turns.`,
  [TRIP.name]: `Causes Bleeding (1D8 at the start of each action); −${Math.round(TRIP.statPenalty * 100)}% to stats for the rest of the battle.`,
  [FIREBALL.name]: `Area radius ${FIREBALL.size}.`,
  [POISON_BREATH.name]: "Learned at level 3; progression begins at level 3. Cone radius 1–5. Burning Hands damage progression is delayed by 2 levels. Minor Poison deals 1D4 per turn; it can hit allies too.",
  [FROST.name]: "Learned at level 5. A line of 2 hexes ahead, +1 hex every 4 levels. Ice damage; can hit allies. Cultist V2: 1 starting charge, +1 at levels 5, 12, and 20.",
  [CURE_DISEASE.name]: "Cleric T3. Cures disease and poison. Teal light.",
  [ICE_STORM.name]: "At the start of each turn, units in the area take Ice damage reduced by Ice Resistance. It can hit allies. The area remains on the battlefield for the listed number of rounds.",
  [CAUSTIC_VENOM.name]: `Range ${CAUSTIC_VENOM.range}. Medium Poison: 1D10 at the start of each turn until cured. Radius ${CAUSTIC_VENOM.size}; affects both sides.`,
  [DIVINE_BOLT.name]: `Salazar only. Range ${DIVINE_BOLT.range}; radius ${DIVINE_BOLT.size}; Holy damage, affecting up to six adjacent hexes.`,
  [MULTI_SHOT.name]: "2 targets (3 at level 11), range 6. Damage die increases at levels 8 and 13.",
  [SECOND_WIND.name]: `Passive: automatically heals when HP falls to ${Math.round(SECOND_WIND.badlyWoundedPct * 100)}% or lower. It is not an action-bar attack.`,
  [CURES.cureLight.name]: "Between Minor and Medium Heal; exclusive uses for the Paladin.",
  [AURA_OF_PROTECTION.name]: "Instant, centered on self — no aiming. Scales at levels 20, 22, 24, 26, 28, and 30.",
  [DIVINE_WRATH.name]: `Aimed straight line, range ${DIVINE_WRATH.range} — never hits allies. Damage die increases at levels 19, 22, 26, and 30.`,
  [SHOULDER_SMASH.name]: `Requires no shield. Hex arc grows to 4; knocks targets back ${SHOULDER_SMASH.knockback} hexes.`,
  [INTIMIDATING_PRESENCE.name]: "Instant, centered on self — no aiming. The opposite of Aura of Protection, with the same scaling.",
  [STAMPEDE.name]: `Aimed straight line, range ${STAMPEDE.range} — hits allies too. Damage die increases at levels 21, 24, 27, and 30.`,
};

function helpFormulaText(value: string): string {
  const english = value
    .replaceAll("dano de arma", "weapon damage")
    .replaceAll("armadura", "armor")
    .replaceAll("arma", "weapon")
    .replaceAll("cura", "healing")
    .replaceAll("centro", "center")
    .replaceAll("respingo", "splash")
    .replaceAll("raio", "radius")
    .replaceAll("% de DEX", "% DEX")
    .replaceAll("% dano", "% damage")
    .replaceAll("dano", "damage")
    .replaceAll("alcance", "range")
    .replaceAll("rodadas", "rounds")
    .replaceAll("Veja a progressão completa ao lado.", "See the full progression beside it.");
  return uiText(value, { en: english });
}

const RESISTANCE_LABELS_PT: Record<(typeof RESISTANCE_ELEMENTS)[number], string> = {
  fire: "Fogo",
  lightning: "Relâmpago",
  ice: "Gelo",
  arcane: "Arcano",
  darkness: "Escuridão",
  holy: "Sagrado",
  poison: "Veneno",
  ember: "Brasa",
};

export function HelpModal({ onClose }: { onClose: () => void }) {
  const { uiLanguage } = useGamePreferences();
  const numberLocale = uiLanguage === "en" ? "en-US" : "pt-BR";
  const [tab, setTab] = useState<"basicos" | "tabelas" | "loot" | "dano" | "formulas">("basicos");
  return (
    <div className="absolute inset-0 z-20 ember-veil flex items-end sm:items-center justify-center p-4">
      {/* The framed panel stays put and only the inner area scrolls, so the panel's gold
          corners don't scroll away with the content. */}
      <div className="relative w-full max-w-lg max-h-[85dvh] flex flex-col ember-panel p-6">
        <div className="flex items-start justify-between gap-4 mb-4 shrink-0">
          <h2 className="font-display text-2xl leading-none ember-title">{uiText("Como jogar")}</h2>
          <button type="button" onClick={onClose} className="size-8 grid place-items-center ember-icon-btn" aria-label={uiText("Fechar")}>
            <X className="size-4" />
          </button>
        </div>
        <div className="grid grid-cols-3 sm:grid-cols-5 gap-1 mb-4 shrink-0">
          <Button size="sm" variant={tab === "basicos" ? undefined : "quiet"} className={`ember-btn ember-btn-sm ${tab === "basicos" ? "ember-btn-primary" : "ember-btn-ghost"}`} onClick={() => setTab("basicos")}>
            {uiText("Básicos")}
          </Button>
          <Button size="sm" variant={tab === "tabelas" ? undefined : "quiet"} className={`ember-btn ember-btn-sm ${tab === "tabelas" ? "ember-btn-primary" : "ember-btn-ghost"}`} onClick={() => setTab("tabelas")}>
            {uiText("Usos")}
          </Button>
          <Button size="sm" variant={tab === "dano" ? undefined : "quiet"} className={`ember-btn ember-btn-sm ${tab === "dano" ? "ember-btn-primary" : "ember-btn-ghost"}`} onClick={() => setTab("dano")}>
            {uiText("Dano")}
          </Button>
          <Button size="sm" variant={tab === "formulas" ? undefined : "quiet"} className={`ember-btn ember-btn-sm ${tab === "formulas" ? "ember-btn-primary" : "ember-btn-ghost"}`} onClick={() => setTab("formulas")}>
            {uiText("Fórmulas", { en: "Formulas" })}
          </Button>
          <Button size="sm" variant={tab === "loot" ? undefined : "quiet"} className={`ember-btn ember-btn-sm ${tab === "loot" ? "ember-btn-primary" : "ember-btn-ghost"}`} onClick={() => setTab("loot")}>
            {uiText("Loot")}
          </Button>
        </div>
        <div className="min-h-0 flex-1 overflow-y-auto ember-scrollbar pr-1">
        {tab === "basicos" ? (
          <ul className="space-y-3 text-sm text-muted leading-relaxed">
            <li>{uiText("Toque numa aliada para ver movimento (azul) e ataque (vermelho).", { en: "Select an ally to see movement (blue) and attack (red) ranges." })}</li>
            <li>{uiText("Toque num inimigo para ver HP, alcance e a área vermelha de perigo.", { en: "Select an enemy to see HP, range, and its red danger zone." })}</li>
            <li>{uiText("Todo mundo tem AT, MAG, DF, DEX, Mov e Alc. Nada fica de fora da ficha.", { en: "Every unit has ATK, MAG, DEF, DEX, Move, and Range. All stats are shown on its card." })}</li>
            <li>{uiText("Terreno alto: +10% do ATK ou MAG (arredondado); armas de alcance ganham +1 de alcance. Flechas disparadas do alto passam por outro hex alto.", { en: "High ground: +10% ATK or MAG (rounded); ranged weapons gain +1 range. Arrows fired from high ground pass over another high hex." })}</li>
            <li>{uiText("Barricada (estacas, 3 hexes): ninguém passa. De trás você atira. Projéteis não acertam quem está atrás.", { en: "Barricade (three hexes of stakes): units cannot pass through it. You can shoot from behind it, and projectiles do not hit units behind it." })}</li>
            <li>{uiText("Após um ataque, o alvo pode contra-atacar se estiver vivo, não estiver atordoado e conseguir alcançar quem atacou. A prévia mostra chance e dano do contra-ataque.", { en: "After an attack, the target can counter if alive, not stunned, and able to reach the attacker. The preview shows counter hit chance and damage." })}</li>
            <li>{uiText("Veneno causa dano no início do turno da vítima até ser curado. Sangramento causa 1D8 por ação; mover causa isso no máximo uma vez por turno. Doença reduz os atributos em 10%.", { en: "Poison deals damage at the start of the victim’s turn until cured. Bleeding deals 1D8 per action; movement triggers it at most once per turn. Disease reduces stats by 10%." })}</li>
            <li>{uiText("Atacar uma fera neutra acorda todas as feras vivas da mesma espécie; elas começam a agir na rodada seguinte.", { en: "Attacking a neutral beast provokes every living beast of the same species; they start acting next round." })}</li>
            <li>{uiText("Depois de mover, dois cliques no personagem = Esperar e passa ao próximo.", { en: "After moving, click the unit twice to Wait and pass to the next unit." })}</li>
          </ul>
        ) : tab === "tabelas" ? (
          <div className="space-y-5">
            <p className="text-sm text-muted leading-relaxed">
              {uiText("Cada classe tem uma velocidade de conjuração — ela decide quantos usos de cada tier (1 a 5) a classe tem em cada nível. As tabelas abaixo mostram os números exatos, nível a nível.", { en: "Each class has a casting speed, which determines how many uses of each tier (1 to 5) it gets at each level. The tables below show the exact numbers for every level." })}
            </p>
            {SKILL_SPEED_GROUPS.map((g) => (
              <div key={g.label}>
                <p className="text-sm font-medium">
                  {uiText(g.label)} <span className="text-muted font-normal">· {g.classes.split(", ").map((name) => uiText(name)).join(", ")}</span>
                </p>
                <div className="overflow-x-auto">
                  <table className="w-full text-xs tabular-nums border-collapse">
                    <thead>
                      <tr className="text-muted">
                        <th className="text-left font-normal pr-2 py-1">{uiText("Nv")}</th>
                        {Array.from({ length: g.maxTier }, (_, i) => i + 1).map((t) => (
                          <th key={t} className="text-right font-normal px-1.5 py-1">
                            T{t}
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {Array.from({ length: MAX_LEVEL }, (_, i) => i + 1).map((level) => (
                        <tr key={level} className="border-t border-border/60">
                          <td className="text-left py-0.5 pr-2 text-muted">{level}</td>
                          {Array.from({ length: g.maxTier }, (_, i) => i + 1).map((t) => (
                            <td key={t} className="text-right px-1.5 py-0.5">
                              {tierUses(g.classId, t as SpellTier, level)}
                            </td>
                          ))}
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>
            ))}
          </div>
        ) : tab === "formulas" ? (
          <div className="space-y-6 text-sm">
            <section className="space-y-2">
              <h3 className="font-display text-lg ember-title">{uiText("Proficiência com armas", { en: "Weapon Proficiency" })}</h3>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("Cada personagem só treina os tipos de arma permitidos para sua classe. A proficiência vai de 0 a 100: cada ponto acrescenta 1 ponto percentual à precisão e 1% aos dados da arma.", { en: "Each character trains only weapon types allowed by their class. Proficiency ranges from 0 to 100: every point adds 1 percentage point to accuracy and 1% to the weapon dice." })}
              </p>
              <p className="rounded-md border border-border bg-bg px-2 py-2 text-xs tabular-nums leading-relaxed">
                <span className="text-accent">{uiText("Precisão final", { en: "Final accuracy" })}</span>: {uiText("limite entre 0% e 100% de (75% + proficiência da arma + bônus de acerto − DEX do defensor)", { en: "Clamp (75% + weapon proficiency + hit bonus − defender DEX) to 0%–100%." })}<br />
                <span className="text-accent">{uiText("Multiplicador dos dados", { en: "Weapon-dice multiplier" })}</span>: {uiText("1 + (proficiência da arma ÷ 100)", { en: "1 + (weapon proficiency ÷ 100)" })}
              </p>
              <div className="overflow-x-auto">
                <table className="w-full text-xs tabular-nums border-collapse">
                  <thead><tr className="text-muted border-b border-border/60">
                    <th className="text-left font-normal pr-2 py-1">{uiText("Proficiência", { en: "Proficiency" })}</th>
                    <th className="text-right font-normal px-1.5 py-1">{uiText("Precisão base", { en: "Base accuracy" })}</th>
                    <th className="text-right font-normal pl-1.5 py-1">{uiText("Multiplicador dos dados", { en: "Weapon-dice multiplier" })}</th>
                  </tr></thead>
                  <tbody>{[0, 1, 20, 50, 100].map((value) => (
                    <tr key={value} className="border-t border-border/60">
                      <td className="py-1 pr-2">{value}</td>
                      <td className="text-right px-1.5 py-1">{weaponSkillAccuracy(value)}%</td>
                      <td className="text-right pl-1.5 py-1">×{weaponSkillDamageMultiplier(value).toFixed(2)}</td>
                    </tr>
                  ))}</tbody>
                </table>
              </div>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("Exemplo de precisão: proficiência 20, bônus de acerto 0 e defensor com DEX 15 → 75 + 20 − 15 = 80% de chance. Em 100 pontos de proficiência, a precisão base chega a 175%; o limite final ainda é aplicado depois da DEX do defensor.", { en: "Accuracy example: proficiency 20, no hit bonus, defender DEX 15 → 75 + 20 − 15 = 80% chance. At 100 proficiency, base accuracy reaches 175%; the final cap is applied after subtracting defender DEX." })}
              </p>
            </section>

            <section className="space-y-2">
              <h3 className="font-display text-lg ember-title">{uiText("Destreza (DEX)", { en: "Dexterity (DEX)" })}</h3>
              <p className="rounded-md border border-border bg-bg px-2 py-2 text-xs tabular-nums leading-relaxed">
                <span className="text-accent">{uiText("Precisão contra você", { en: "Accuracy against you" })}</span>: {uiText("chance do ataque − sua DEX, limitado entre 0% e 100%.", { en: "attack chance − your DEX, clamped to 0%–100%." })}<br />
                <span className="text-accent">{uiText("Fuga", { en: "Escape" })}</span>: {uiText("mínimo entre 100% e (60% + DEX ÷ 3)", { en: "the lower of 100% and (60% + DEX ÷ 3)" })}
              </p>
              <div className="overflow-x-auto">
                <table className="w-full text-xs tabular-nums border-collapse">
                  <thead><tr className="text-muted border-b border-border/60">
                    <th className="text-left font-normal pr-2 py-1">DEX</th>
                    <th className="text-right font-normal pl-1.5 py-1">{uiText("Chance de fuga", { en: "Escape chance" })}</th>
                  </tr></thead>
                  <tbody>{[0, 1, 3, 15, 30, 60, 120].map((dex) => (
                    <tr key={dex} className="border-t border-border/60">
                      <td className="py-1 pr-2">{dex}</td>
                      <td className="text-right pl-1.5 py-1">{Number(dexEscapeChance(dex).toFixed(2)).toLocaleString(numberLocale)}%</td>
                    </tr>
                  ))}</tbody>
                </table>
              </div>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("Fugir exige que o personagem ativo alcance um hex da borda do campo. A chance é calculada pela DEX de quem tenta fugir; falhar consome o turno.", { en: "To flee, the active character must reach a battlefield edge hex. The chance uses the fleeing character's DEX; a failed attempt uses their turn." })}
              </p>
            </section>

            <section className="space-y-2">
              <h3 className="font-display text-lg ember-title">{uiText("Resistências elementais", { en: "Elemental Resistances" })}</h3>
              <p className="rounded-md border border-border bg-bg px-2 py-2 text-xs tabular-nums leading-relaxed">
                <span className="text-accent">{uiText("Resistência total", { en: "Total resistance" })}</span>: {uiText("bônus da classe + perícia de resistência + bônus de equipamento", { en: "class bonus + resistance skill + equipment bonus" })}<br />
                <span className="text-accent">{uiText("Resistência efetiva", { en: "Effective resistance" })}</span>: {uiText("limite entre −50% e 100% de (resistência − penalidade do efeito − MAG do atacante ÷ 2)", { en: "Clamp (resistance − effect penalty − attacker MAG ÷ 2) to −50%–100%." })}<br />
                <span className="text-accent">{uiText("Dano depois da resistência", { en: "Damage after resistance" })}</span>: {uiText("arredondar para baixo [dano base × (1 − resistência efetiva ÷ 100)]", { en: "floor [base damage × (1 − effective resistance ÷ 100)]" })}
              </p>
              <div className="overflow-x-auto">
                <table className="w-full text-xs border-collapse">
                  <thead><tr className="text-muted border-b border-border/60">
                    <th className="text-left font-normal pr-2 py-1">{uiText("Perícia", { en: "Skill" })}</th>
                    <th className="text-left font-normal pl-1.5 py-1">{uiText("Dano que reduz", { en: "Damage reduced" })}</th>
                  </tr></thead>
                  <tbody>{RESISTANCE_ELEMENTS.map((element) => (
                    <tr key={element} className="border-t border-border/60">
                      <td className="py-1 pr-2">{uiText(`${RESISTANCE_LABELS_PT[element]} Resistência`, { en: `${RESISTANCE_LABELS[element]} Resistance` })}</td>
                      <td className="pl-1.5 py-1">{uiText(RESISTANCE_LABELS_PT[element], { en: RESISTANCE_LABELS[element] })}</td>
                    </tr>
                  ))}</tbody>
                </table>
              </div>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("Cada ponto de resistência reduz 1 ponto percentual do dano daquele elemento. MAG 40 penetra 20 pontos. Exemplo: resistência 70 − 20 = 50% efetiva; um golpe de 30 causa 15. Resistências são independentes: Resistência a Fogo não reduz Relâmpago, Gelo, Arcano, Escuridão, Sagrado, Veneno ou Brasa.", { en: "Each resistance point reduces damage from its element by 1 percentage point. MAG 40 penetrates 20 points. Example: resistance 70 − 20 = 50% effective; a 30-damage hit deals 15. Resistances are independent: Fire Resistance does not reduce Lightning, Ice, Arcane, Darkness, Holy, Poison, or Ember." })}
              </p>
            </section>

            <section className="space-y-2">
              <h3 className="font-display text-lg ember-title">{uiText("Veneno", { en: "Poison" })}</h3>
              <p className="rounded-md border border-border bg-bg px-2 py-2 text-xs tabular-nums leading-relaxed">
                <span className="text-accent">{uiText("Resistência efetiva a veneno", { en: "Effective Poison Resistance" })}</span>: {uiText("limite entre −50% e 100% de (Resistência a Veneno − penalidade do veneno − MAG do atacante ÷ 2)", { en: "Clamp (Poison Resistance − poison penalty − attacker MAG ÷ 2) to −50%–100%." })}<br />
                <span className="text-accent">{uiText("Chance de aplicação após acertar", { en: "Application chance after a hit" })}</span>: {uiText("limite entre 0% e 100% de (100% − resistência efetiva)", { en: "Clamp (100% − effective resistance) to 0%–100%." })}<br />
                <span className="text-accent">{uiText("Dano por turno", { en: "Damage per turn" })}</span>: {uiText("arredondar ao inteiro mais próximo [dano rolado × (1 − resistência efetiva ÷ 100)]", { en: "round to the nearest integer [rolled damage × (1 − effective resistance ÷ 100)]" })}
              </p>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("Exemplo: Resistência a Veneno 70, penalidade do veneno 20 e MAG atacante 40 → 70 − 20 − 20 = 30% efetiva. Uma rolagem de dano 30 causa 30 × 70% = 21. Com Resistência 71, a resistência efetiva é 31%; o dano antes do arredondamento é 20,7.", { en: "Example: Poison Resistance 70, poison penalty 20, attacker MAG 40 → 70 − 20 − 20 = 30% effective. A rolled tick of 30 deals 30 × 70% = 21. At Resistance 71, effective resistance is 31%; damage before rounding is 20.7." })}
              </p>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("A penalidade e os dados do tick dependem do veneno aplicado. Veneno causa dano no início do turno da vítima e permanece até ser curado. Dano acima do limite de resistência pode chegar a 150% do valor rolado; a chance de aplicação continua limitada a 100%.", { en: "The resistance penalty and tick dice depend on the applied poison. Poison ticks at the start of the victim's turn and remains until cured. Damage can reach 150% of the roll at the resistance floor; application chance remains capped at 100%." })}
              </p>
            </section>

            <section className="space-y-2">
              <h3 className="font-display text-lg ember-title">{uiText("Aprendizado das perícias", { en: "Skill Progression" })}</h3>
              <p className="rounded-md border border-border bg-bg px-2 py-2 text-xs tabular-nums leading-relaxed">
                <span className="text-accent">{uiText("Chance de ganhar perícia", { en: "Skill-gain chance" })}</span>: {uiText("(100 − valor atual da perícia) ÷ 100; máximo 100, mínimo 0.", { en: "(100 − current skill value) ÷ 100; minimum 0, maximum 100." })}<br />
                <span className="text-accent">{uiText("Limite", { en: "Cap" })}</span>: 100. {uiText("Resistências ganham normalmente", { en: "Resistance skills normally gain" })} +{SKILL_GAIN.toLocaleString(numberLocale)} {uiText("por sucesso de treino.", { en: "per successful training attempt." })}
              </p>
              <div className="overflow-x-auto">
                <table className="w-full text-xs tabular-nums border-collapse">
                  <thead><tr className="text-muted border-b border-border/60">
                    <th className="text-left font-normal pr-2 py-1">{uiText("Valor atual", { en: "Current value" })}</th>
                    <th className="text-right font-normal pl-1.5 py-1">{uiText("Chance de ganho", { en: "Gain chance" })}</th>
                  </tr></thead>
                  <tbody>{[0, 50, 90, 99, SKILL_CAP].map((value) => (
                    <tr key={value} className="border-t border-border/60">
                      <td className="py-1 pr-2">{value}</td>
                      <td className="text-right pl-1.5 py-1">{skillGainChance(value) * 100}%</td>
                    </tr>
                  ))}</tbody>
                </table>
              </div>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("Perícia de arma pode treinar ao atacar inimigos com aquele tipo de arma, inclusive quando erra. Resistência treina ao usar ou sofrer dano do elemento correspondente; em área, cada inimigo atingido permite uma tentativa. Magias dos familiares treinam a resistência correspondente do conjurador.", { en: "Weapon proficiency can train by attacking enemies with that weapon type, including misses. A resistance trains when you use or take damage from its element; each enemy hit by an area attack gives one gain attempt. Familiar spells train the summoner's matching resistance." })}
              </p>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("Contra inimigo 5 níveis abaixo: o ganho é +0,05. Com diferença de 10 níveis: não há ganho. No treino de estrada, escolha uma perícia por personagem: resistência recebe +0,1 a cada 12 horas; arma recebe +1 a cada 120 horas. A contagem restante é mantida ao trocar de perícia.", { en: "Against an enemy 5 levels below: gain is +0.05. At a 10-level difference: no gain. For road training, choose one skill per character: resistance gains +0.1 every 12 hours; weapon proficiency gains +1 every 120 hours. Remaining time carries over when switching skills." })}
              </p>
            </section>
          </div>
        ) : tab === "dano" ? (
          <div className="space-y-5">
            <div className="space-y-2">
              <p className="text-sm leading-relaxed">
                <span className="text-accent">{uiText("Ataque normal", { en: "Normal attack" })}</span> = {uiText("metade do seu ATK (ou MAG, se for conjurador) + dados da arma + terreno − metade da DEF do alvo (resistência elemental, contra magia). Metades não contam: arredonda pra baixo. Mínimo 1 de dano.", { en: "half your ATK (or MAG for a caster) + weapon dice + terrain − half the target's DEF (elemental resistance against magic). Fractions round down. Minimum 1 damage." })}
              </p>
              <p className="text-sm leading-relaxed">
                <span className="text-accent">{uiText("Magia", { en: "Spell" })}</span> = {uiText("a mesma conta, com a sua metade de MAG multiplicada pelo peso da magia e os dados dela no lugar da arma. Todo peso é maior que 1, e o resultado nunca fica abaixo de um ataque normal — conjurar sempre vale mais que bater.", { en: "Uses the same calculation, with half your MAG multiplied by the spell's multiplier and its dice in place of the weapon dice. Every multiplier is greater than 1, and the result is never lower than a normal attack." })}
              </p>
              <p className="text-xs text-muted leading-relaxed">
                {uiText("As fórmulas mostram o dano pelo MAG do conjurador. Ice Storm ocupa 3 hexes nos níveis 8–11 e 7 hexes a partir do nível 12, sem novos aumentos de área. Alcance, duração e dano continuam a progredir; os valores aparecem em cada faixa de nível.", { en: "The formulas show damage by the caster's MAG. Ice Storm covers 3 hexes at levels 8–11 and 7 hexes from level 12 onward, with no further area increases. Range, duration, and damage continue to scale; values are shown for each level range." })}
              </p>
            </div>
            <div className="overflow-x-auto">
              <table className="w-full text-xs border-collapse">
                <thead>
                  <tr className="text-muted">
                    <th className="text-left font-normal pr-2 py-1">{uiText("Habilidade")}</th>
                    <th className="text-left font-normal px-1.5 py-1">{uiText("Classe")}</th>
                    <th className="text-center font-normal px-1.5 py-1">Tier</th>
                    <th className="text-left font-normal px-1.5 py-1">{uiText("Fórmula")}</th>
                    <th className="text-left font-normal pl-1.5 py-1">{uiText("Efeito")}</th>
                  </tr>
                </thead>
                <tbody>
                  {SKILL_DAMAGE_ROWS.map((row) => (
                    <tr key={row.name} className="border-t border-border/60 align-top">
                      <td className="text-left py-1 pr-2 font-medium whitespace-nowrap">{uiText(row.name)}</td>
                      <td className="text-left px-1.5 py-1 text-muted whitespace-nowrap">{uiText(CLASSES[row.cls].name)}</td>
                      <td className="text-center px-1.5 py-1 text-muted">T{row.tier}</td>
                      <td className="text-left px-1.5 py-1 tabular-nums">
                        {row.progression ? (
                          <div className="space-y-1">
                            {row.progression.map((step) => (
                              <div key={step.levels}>
                                {uiText("Níveis", { en: "Levels" })} {step.levels}: {uiText("alcance", { en: "range" })} {step.range}, {uiText("área de", { en: "area of" })} {step.size} {uiText("hexes", { en: "hexes" })}, {uiText("duração", { en: "duration" })} {step.duration} {uiText("rodadas", { en: "rounds" })}, {uiText("dano", { en: "damage" })} {step.damage}
                              </div>
                            ))}
                          </div>
                        ) : typeof row.formula === "string" ? (
                          helpFormulaText(row.formula)
                        ) : row.param === "level" ? (
                          <span className="space-x-1.5">
                            <span>{uiText("Nv1")}: {helpFormulaText(row.formula(1))}</span>
                            <span className="text-muted">· {uiText("Nv7")}: {helpFormulaText(row.formula(7))}</span>
                            <span className="text-muted">· {uiText("Nv14")}: {helpFormulaText(row.formula(14))}</span>
                          </span>
                        ) : (
                          <span className="space-x-1.5">
                            <span>{uiText("MAG 10")}: {helpFormulaText(row.formula(10))}</span>
                            <span className="text-muted">· {uiText("MAG 20")}: {helpFormulaText(row.formula(20))}</span>
                            <span className="text-muted">· {uiText("MAG 40")}: {helpFormulaText(row.formula(40))}</span>
                          </span>
                        )}
                      </td>
                      <td className="text-left pl-1.5 py-1 text-muted">{uiText(row.note, { en: SKILL_DAMAGE_NOTES_EN[row.name] })}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        ) : (
          <div className="space-y-5">
            <p className="text-sm text-muted leading-relaxed">
              {uiText("Chances de drop, do jeito que estão programadas agora.")}
            </p>
            <div>
              <p className="text-sm font-medium">{uiText("Poções em baú (por baú)")}</p>
              <p className="text-xs text-muted leading-relaxed mb-2">
                {uiText("Todo baú dá Gold + uma poção garantida (sorteada abaixo) + uma chance separada de item. Se quem abriu já estiver no máximo daquela poção (5), ela passa para o próximo personagem que vai agir; se todos estiverem cheios, é descartada.")}
              </p>
              <div className="overflow-x-auto">
                <table className="w-full text-xs tabular-nums border-collapse">
                  <thead>
                    <tr className="text-muted">
                      <th className="text-left font-normal pr-2 py-1">{uiText("Poção")}</th>
                      <th className="text-right font-normal pl-1.5 py-1">{uiText("Chance")}</th>
                    </tr>
                  </thead>
                  <tbody>
                    {POTION_LOOT_ROWS.map((r) => (
                      <tr key={r.name} className="border-t border-border/60">
                        <td className="text-left py-0.5 pr-2">{uiText(r.name)}</td>
                        <td className="text-right pl-1.5 py-0.5">{((r.weight / POTION_LOOT_TOTAL) * 100).toFixed(0)}%</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
            <div className="text-sm text-muted leading-relaxed space-y-1">
              <p className="text-fg font-medium text-sm">{uiText("Gold e item de baú")}</p>
              <p>
                {uiText("Gold:")} {CHEST_LOOT.emberBase}–{CHEST_LOOT.emberBase + CHEST_LOOT.emberDice - 1} {uiText("por baú.")}
              </p>
              <p>{uiText("Chance extra de arma ou equipamento:")} {Math.round(CHEST_LOOT.gearChance * 100)}%.</p>
            </div>
            <div className="text-sm text-muted leading-relaxed space-y-1">
              <p className="text-fg font-medium text-sm">{uiText("Drop ao matar inimigo")}</p>
              <p>{uiText("Inimigo comum:")} {(KILL_DROP_CHANCE * 100).toFixed(0)}% {uiText("de chance de largar uma arma.")}</p>
              <p>{uiText("Chefes nomeados (drop garantido): sempre largam arma ou equipamento ao morrer.")}</p>
            </div>
            <p className="text-xs text-muted leading-relaxed">
              {uiText("Toda arma/equipamento largado é sorteado por preço — quanto mais caro, mais raro — e limitado ao nível de itens da missão atual, então cada trecho da campanha só solta o que faz sentido pra ele.")}
            </p>
          </div>
        )}
        </div>
        <Button className="mt-5 w-full shrink-0 ember-btn ember-btn-primary" onClick={onClose}>
          {uiText("Entendi")}
        </Button>
      </div>
    </div>
  );
}

function TestMenuScreen({
  ready,
  onBack,
  onDebug,
  onMapEditor,
  onDevControls,
}: {
  ready: boolean;
  onBack: () => void;
  onDebug: () => void;
  onMapEditor: () => void;
  onDevControls: () => void;
}) {
  return (
    <section className="h-dvh min-h-0 flex flex-col bg-bg">
      <header className="flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-4 border-b border-border">
        <button type="button" onClick={onBack} className="size-10 grid place-items-center rounded-md border border-border" aria-label="Voltar">
          <ChevronLeft className="size-5" />
        </button>
        <div className="flex-1 min-w-0">
          <p className="text-sm uppercase tracking-[0.18em] text-muted">Modo teste</p>
          <h1 className="font-display text-3xl leading-none">O que abrir?</h1>
        </div>
      </header>
      <div className="flex-1 min-h-0 flex flex-col justify-center gap-3 p-5 max-w-md mx-auto w-full">
        <button
          type="button"
          disabled={!ready}
          onClick={onDebug}
          className="text-left rounded-xl border border-border bg-bg/40 px-5 py-4 hover:border-accent disabled:opacity-40"
        >
          <p className="font-display text-2xl leading-tight">Debug</p>
          <p className="text-sm text-muted mt-1">Joga qualquer missão da campanha, sem travar progresso — o de sempre.</p>
        </button>
        <button
          type="button"
          disabled={!ready}
          onClick={onMapEditor}
          className="text-left rounded-xl border border-border bg-bg/40 px-5 py-4 hover:border-accent disabled:opacity-40"
        >
          <p className="font-display text-2xl leading-tight">Map Editor</p>
          <p className="text-sm text-muted mt-1">Pinta terreno, posiciona spawns, testa na hora e exporta pra colar no jogo.</p>
        </button>
        <button
          type="button"
          onClick={onDevControls}
          className="text-left rounded-xl border border-border bg-bg/40 px-5 py-4 hover:border-accent"
        >
          <p className="font-display text-2xl leading-tight">Dev Controls</p>
          <p className="text-sm text-muted mt-1">Liga/desliga sombras do renderizador 3D pra comparar em combate.</p>
        </button>
      </div>
    </section>
  );
}

const DEV_GFX_ROWS: { key: "realShadows" | "softShadows" | "contactShadows" | "localLights" | "fogOfWar" | "fogDebug" | "ambientOcclusion"; label: string; hint: string }[] = [
  { key: "realShadows", label: "Sombras reais", hint: "Sombra projetada pelo sol (unidades e props)." },
  { key: "softShadows", label: "Sombras suaves (PCF)", hint: "Borda da sombra suavizada em vez de serrilhada." },
  { key: "contactShadows", label: "Contact shadows", hint: "Mancha escura curta nos pés de cada unidade." },
  { key: "localLights", label: "Luzes do mapa", hint: "Braseiros, fogueiras, lanternas e casas em chamas iluminam o chão, os personagens e os props perto deles." },
  { key: "fogOfWar", label: "Fog of war", hint: "Névoa de guerra nos mapas que a usam. Desligado = tudo visível, pra comparar." },
  { key: "fogDebug", label: "Fog of war — debug", hint: "Cores por hex: verde = visível, âmbar = explorado, vermelho = inexplorado." },
  { key: "ambientOcclusion", label: "Oclusão ambiente", hint: "Escurece de leve a luz ambiente do chão junto a props, muros e desníveis." },
];

const DEV_SKY_SLIDERS: { key: "sunAzimuth" | "sunElevation" | "moonAzimuth" | "moonElevation"; label: string; min: number; max: number }[] = [
  { key: "sunAzimuth", label: "Sol — direção", min: 0, max: 359 },
  { key: "sunElevation", label: "Sol — altura", min: 5, max: 85 },
  { key: "moonAzimuth", label: "Lua — direção", min: 0, max: 359 },
  { key: "moonElevation", label: "Lua — altura", min: 5, max: 85 },
];

/** Dev-only toggles for the battle renderer's shadow features (see gfx/three/devGfx.ts) —
 * saved per-browser and applied live, so flip here then open a fight via Debug to compare. */
function DevControlsScreen({ onBack }: { onBack: () => void }) {
  const gfx = useSyncExternalStore(subscribeDevGfx, getDevGfx);
  const [hd2dTest, setHd2dTest] = useState(false);
  if (hd2dTest) return <Hd2dTestScreen onBack={() => setHd2dTest(false)} />;
  return (
    <section className="h-dvh min-h-0 flex flex-col bg-bg">
      <header className="flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-4 border-b border-border">
        <button type="button" onClick={onBack} className="size-10 grid place-items-center rounded-md border border-border" aria-label="Voltar">
          <ChevronLeft className="size-5" />
        </button>
        <div className="flex-1 min-w-0">
          <p className="text-sm uppercase tracking-[0.18em] text-muted">Modo teste</p>
          <h1 className="font-display text-3xl leading-none">Dev Controls</h1>
        </div>
      </header>
      <div className="flex-1 min-h-0 overflow-y-auto flex flex-col gap-3 p-5 max-w-3xl mx-auto w-full">
        <GraphicsQualityControl />
        <VfxDebugPanel />
        <DevGfxPreview />
        <button type="button" onClick={() => setHd2dTest(true)} className="rounded-xl border border-accent bg-bg/40 px-5 py-4 text-left font-display text-xl hover:bg-accent/10">
          Cena 3D — teste HD-2D
        </button>
        <p className="text-sm uppercase tracking-[0.14em] text-muted">Sombras</p>
        {DEV_GFX_ROWS.map((row) => (
          <button
            key={row.key}
            type="button"
            role="switch"
            aria-checked={gfx[row.key]}
            onClick={() => setDevGfx({ [row.key]: !gfx[row.key] })}
            className="flex items-center gap-4 text-left rounded-xl border border-border bg-bg/40 px-5 py-4 hover:border-accent"
          >
            <span className="flex-1 min-w-0">
              <span className="block font-display text-xl leading-tight">{row.label}</span>
              <span className="block text-sm text-muted mt-1">{row.hint}</span>
            </span>
            <span className={`relative h-6 w-11 shrink-0 rounded-full transition-colors ${gfx[row.key] ? "bg-accent" : "bg-border"}`}>
              <span className={`absolute top-0.5 size-5 rounded-full bg-bg transition-all ${gfx[row.key] ? "left-[1.375rem]" : "left-0.5"}`} />
            </span>
          </button>
        ))}
        {DEV_SKY_SLIDERS.map((s) => (
          <label key={s.key} className="flex flex-col gap-2 rounded-xl border border-border bg-bg/40 px-5 py-4">
            <span className="flex justify-between font-display text-xl leading-tight">
              <span>{s.label}</span>
              <span className="text-muted">{Math.round(gfx[s.key])}°</span>
            </span>
            <input type="range" min={s.min} max={s.max} step={1} value={gfx[s.key]} onChange={(e) => setDevGfx({ [s.key]: Number(e.target.value) })} />
          </label>
        ))}
        <p className="text-xs text-muted leading-relaxed pt-1">Salvo neste navegador. Vale em qualquer combate com o renderizador 3D (padrão).</p>
      </div>
    </section>
  );
}

/** Lets the player pick between the classic map (click any unlocked pin, jump straight to
 * its missions) and the RPG map (a hidden hex grid — the party moves one hex at a time, and
 * every step costs a day). Neither replaces the other: this is asked once per session,
 * right before either map first opens, precisely so the classic path — the one the current
 * demo relies on — never gets silently swapped out from under it. */
function MapChoiceScreen({ onBack, onPick }: { onBack: () => void; onPick: (mode: "classic" | "rpg") => void }) {
  return (
    <section className="relative h-dvh min-h-0 flex flex-col overflow-hidden bg-[#080a0d] text-fg">
      <img src="/game/ui/travel-board.png" alt="" className="absolute inset-0 size-full object-cover object-center" />
      <div className="absolute inset-0 bg-gradient-to-b from-black/30 via-black/5 to-black/35" aria-hidden="true" />
      <header className="relative z-10 flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-4 border-b border-white/10 bg-black/10">
        <button type="button" onClick={onBack} className="size-10 grid place-items-center rounded-md border border-white/25 bg-black/45" aria-label="Voltar">
          <ChevronLeft className="size-5" />
        </button>
        <div className="flex-1 min-w-0">
          <p className="text-sm uppercase tracking-[0.18em] text-slate-300">Mapa</p>
          <h1 className="font-display text-3xl leading-none text-white">Como quer viajar?</h1>
        </div>
      </header>
      <div className="relative z-10 flex-1 min-h-0 flex items-end justify-center p-4 sm:p-8 pb-[max(1.25rem,env(safe-area-inset-bottom))]">
        <div className="grid w-full max-w-5xl grid-cols-1 md:grid-cols-2 gap-3">
        <button
          type="button"
          onClick={() => onPick("classic")}
          className="text-left rounded-lg border border-white/25 bg-black/45 backdrop-blur-sm px-4 py-3 hover:border-accent sm:px-5 sm:py-4"
        >
          <p className="font-display text-2xl leading-tight">Classic Tactical</p>
          <p className="text-sm text-muted mt-1">O mapa de sempre: escolha qualquer local desbloqueado e vá direto pra missão.</p>
        </button>
        <button
          type="button"
          onClick={() => onPick("rpg")}
          className="text-left rounded-lg border border-white/25 bg-black/45 backdrop-blur-sm px-4 py-3 hover:border-accent sm:px-5 sm:py-4"
        >
          <p className="font-display text-2xl leading-tight">RPG Map</p>
          <p className="text-sm text-muted mt-1">O grupo viaja hexágono por hexágono; cada passo custa um dia — suprimentos, encontros e recuperação entram em jogo.</p>
        </button>
        </div>
      </div>
    </section>
  );
}

/** Index of a cell in the flat tile array, or -1 when it is off the board.
 *
 * A prop may hang off the edge, so its footprint routinely names cells that do not exist.
 * `y * cols + x` cannot express that: x = -1 lands on the previous row's last cell, so
 * stamping a footprint blind would silently repaint a hex on the far side of the map. */
function cellIndex(x: number, y: number, cols: number, rows: number): number {
  if (x < 0 || y < 0 || x >= cols || y >= rows) return -1;
  return y * cols + x;
}

const DECO_SHUFFLE_EXCLUDE_KEY = "ember-deco-shuffle-exclude";
const EDITOR_COLS_DEFAULT = 20;
const EDITOR_ROWS_DEFAULT = 20;

/** Default level for a newly added spawn, and for every hero's level in test mode: enough
 * spell slots unlocked to actually test with, without being maxed out.
 *
 * !!! THIS IS THE ONE PLACE TO CHANGE THE TEST-MODE LEVEL — never hardcode a level number
 * anywhere else, and never let test mode fall back to a mission-position-based level
 * (m.index + 1) instead of this constant. Test mode's entire point is full-strength testing
 * on any mission with no grinding; reverting to a per-mission level defeats that and has
 * happened by accident multiple times already. If a level-related bug shows up in test mode,
 * fix it here or ask first — don't route around this constant. */
const DEFAULT_TEST_LEVEL = 15;

/** One canonical scenario prefix everywhere: the editor's ID becomes the exact file prefix.
 * `Vau 01` therefore saves as `vau-01001.json` only if the author actually made the ID
 * `vau-01`; the trailing three digits are always the generated save serial. */
/** The id input's live typing: lowercases and collapses invalid characters as the author types,
 * but never trims a trailing "-" (typing "vau-" mid-word would otherwise have it eaten before
 * the next letter lands) and never falls back to a default for an empty value (clearing the
 * field to type a new name must actually leave it blank, not snap back to "scenario"). Both of
 * those only get applied by normalizeScenarioId below, at the point an id is actually saved. */
function stripScenarioId(value: string): string {
  return value
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .slice(0, 64);
}

function normalizeScenarioId(value: string): string {
  return stripScenarioId(value).replace(/^-+|-+$/g, "") || "scenario";
}

/** Finds the ground that should reappear when a terrain-changing decoration is removed.
 * Old maps created before “Substituir base” retain their own first tile as a safe fallback. */
function baseForDraft(d: MapDraft): { tile: TerrainId; variant: number } {
  const tile = d.baseTile ?? d.tiles[0] ?? "plains";
  const maxVariant = Math.max(1, TILE_VARIANT_COUNT[tile] ?? 1) - 1;
  const candidate = d.baseVariant ?? d.tileVariants[0] ?? 0;
  return { tile, variant: Math.max(0, Math.min(maxVariant, candidate)) };
}

function blankDraft(): MapDraft {
  return {
    id: `custom-${Date.now().toString(36)}`,
    index: 0,
    title: "Mapa sem nome",
    place: "",
    briefing: "",
    objective: "Derrote todos os inimigos",
    win: "rout",
    hub: false,
    autoTactics: true,
    fog: false,
    environment: "outdoor",
    timeOfDay: "day",
    sunIntensity: DEFAULT_SUN_INTENSITY,
    ambientIntensity: DEFAULT_AMBIENT_INTENSITY,
    mistIntensity: 0.2,
    mistSpeed: 1,
    mistType: "none",
    bloomIntensity: DEFAULT_BLOOM_INTENSITY,
    wispIntensity: 0.02,
    wispSpeed: 1,
    wispColor: 0xffa552,
    locationId: "",
    cols: EDITOR_COLS_DEFAULT,
    rows: EDITOR_ROWS_DEFAULT,
    tiles: Array.from({ length: EDITOR_COLS_DEFAULT * EDITOR_ROWS_DEFAULT }, () => "plains" as TerrainId),
    tileVariants: Array.from({ length: EDITOR_COLS_DEFAULT * EDITOR_ROWS_DEFAULT }, () => 0),
    baseTile: "plains",
    baseVariant: 0,
    tileRots: Array.from({ length: EDITOR_COLS_DEFAULT * EDITOR_ROWS_DEFAULT }, () => 0),
    music: "",
    decorations: [],
    elementalFx: [],
    playerSpawns: [],
    enemySpawns: [],
    neutralSpawns: [],
  };
}


/** Loads an existing campaign mission into the editor, targeting that same mission's id —
 * so saved versions stack up under it and "Ativar" can make one of them live for that
 * real campaign slot. The immutable static Mission data itself is never touched; this
 * only ever writes to the versioned localStorage store. */
/** The three spawn lists a draft carries, and the Side each one spawns into. */
type SpawnKey = "playerSpawns" | "enemySpawns" | "neutralSpawns";
const SPAWN_SIDE: Record<SpawnKey, "player" | "enemy" | "neutral"> = {
  playerSpawns: "player",
  enemySpawns: "enemy",
  neutralSpawns: "neutral",
};

/** Resolve a preview selection to the editor draft placement. Watchtower preview
 * normalization can swap a connector's up/down stair art or rotation without
 * changing its saved hex, so connector selections fall back to their origin hex. */
function findPreviewDecoration(
  placements: DecorationPlacement[],
  selected: PreviewDecorationSelection,
): DecorationPlacement | undefined {
  const exact = placements.find((p) =>
    p.id === selected.id && p.x === selected.x && p.y === selected.y && (p.rot ?? 0) === (selected.rot ?? 0),
  );
  if (exact) return exact;
  if (DECORATIONS[selected.id]?.exitKind !== "connector") {
    // A preview built before the position repair was removed can still contain
    // the relocated exit. Only resolve by ID when there is no ambiguity.
    const matches = placements.filter(p => p.id === selected.id);
    return DECORATIONS[selected.id]?.exitKind && matches.length === 1 ? matches[0] : undefined;
  }
  return placements.find((p) =>
    p.x === selected.x && p.y === selected.y && DECORATIONS[p.id]?.exitKind === "connector",
  );
}

const WATCHTOWER_ENTRANCE_ID = "watchtower-stone-open-door-2hex";
function previewDecorationCells(placements: DecorationPlacement[], candidate: DecorationPlacement): Set<string> {
  const candidateCells = new Set(placedFootprint(candidate).map(f => `${candidate.x + f.dx},${candidate.y + f.dy}`));
  const isWaypoint = !!DECORATIONS[candidate.id]?.exitKind;
  return decorationCells(placements.filter((p) => {
    const overlaps = placedFootprint(p).some(f => candidateCells.has(`${p.x + f.dx},${p.y + f.dy}`));
    if (!overlaps) return true;
    if (candidate.id === WATCHTOWER_ENTRANCE_ID && (DECORATIONS[p.id]?.model3d === "wall" || !!DECORATIONS[p.id]?.exitKind)) return false;
    if (isWaypoint && p.id === WATCHTOWER_ENTRANCE_ID) return false;
    return true;
  }));
}

function missionToDraft(m: Mission): MapDraft {
  const n = m.cols * m.rows;
  const variants = m.tileVariants ?? [];
  return {
    id: m.id,
    index: m.index,
    title: m.title,
    place: m.place,
    briefing: m.briefing,
    objective: m.objective,
    win: m.win,
    hub: !!m.hub,
    explore: m.explore === true,
    squareTiles: m.squareTiles,
    autoTactics: m.autoTactics !== false,
    fog: m.fog === true,
    environment: m.environment === "indoor" ? "indoor" : "outdoor",
    timeOfDay: m.timeOfDay ?? "day",
    sunIntensity: m.sunIntensity ?? DEFAULT_SUN_INTENSITY,
    ambientIntensity: m.ambientIntensity ?? DEFAULT_AMBIENT_INTENSITY,
    mistIntensity: m.mistIntensity ?? 0.2,
    mistSpeed: m.mistSpeed ?? 1,
    mistType: m.mistType ?? "none",
    bloomIntensity: m.bloomIntensity ?? DEFAULT_BLOOM_INTENSITY,
    wispIntensity: m.wispIntensity ?? 0.02,
    wispSpeed: m.wispSpeed ?? 1,
    wispColor: m.wispColor ?? 0xffa552,
    locationId: locationForMission(m.id)?.id ?? "",
    cols: m.cols,
    rows: m.rows,
    tiles: parseLayout(m.layout),
    tileVariants: Array.from({ length: n }, (_, i) => variants[i] ?? 0),
    terrainElevations: m.terrainElevations?.slice(),
    waterLevels: m.waterLevels?.slice(),
    waterVersion: m.waterVersion,
    waterPatches: m.waterPatches?.map(p => ({ ...p })),
    waterFootprints: m.waterFootprints?.map(p => p ? { ...p } : null),
    baseTile: m.baseTile,
    baseVariant: m.baseVariant,
    tileRots: Array.from({ length: n }, (_, i) => m.tileRots?.[i] ?? 0),
    music: m.music ?? "",
    decorations: m.decorations ?? [],
    elementalFx: m.elementalFx ?? [],
    lockPartyFormation: m.lockPartyFormation,
    playerSpawns: m.playerSpawns.map((s) => ({ ...s, level: DEFAULT_TEST_LEVEL })),
    enemySpawns: m.enemySpawns.map((s) => ({ ...s, level: enemyLevelFor(m.index) })),
    neutralSpawns: (m.neutralSpawns ?? []).map((s) => ({ ...s, level: enemyLevelFor(m.index) })),
    introDialog: m.introDialog,
    introDialogEnabled: m.introDialogEnabled,
    outroDialog: m.outroDialog,
    outroDialogEnabled: m.outroDialogEnabled,
    victoryReward: m.victoryReward,
  };
}

const DEFAULT_HEROES: { name: string; classId: ClassId }[] = [
  { name: "Kael", classId: "kaelFinal" },
  { name: "Neera", classId: "neera" },
  { name: "Voss", classId: "voss" },
  { name: "Salazar", classId: "salazar" },
];

/** Heroes who join later are added to the test party before recruitment; normal party menus
 * include them once their story flag or authored joining mission makes them available. */
const TEST_EXTRA_HEROES: { name: string; classId: ClassId }[] = [
  { name: "Aldric", classId: "aldric" },
  { name: "Malrec", classId: "conjurer" },
];

/** The editor's spawn lists, in display order. A spawn's class decides whether it is listed
 * as a summon, so a summon is grouped as one wherever it was placed from. */
/** Every spawn list, in the order a cell is searched for whoever stands on it. */
const SPAWN_KEYS: SpawnKey[] = ["playerSpawns", "enemySpawns", "neutralSpawns"];

/** Sorts display names the way a Portuguese reader scans a list: case and accents ignored,
 * so "Água" lands with the A's and not after Z. */
const byName = (a: string, b: string) => a.localeCompare(b, "pt-BR", { sensitivity: "base" });

/** The grid letter's colour, by side: blue ally, green neutral, red enemy. */
const SIDE_INK: Record<"player" | "enemy" | "neutral", string> = {
  player: "text-sky-300",
  neutral: "text-emerald-400",
  enemy: "text-red-400",
};

/** P hero, E enemy, S summon (either side), N wild neutral — the colour says the side, the
 * letter says what it is. */
function spawnGlyph(sp: DraftSpawn, side: "player" | "enemy" | "neutral"): string {
  if (isSummonClass(sp.classId)) return "S";
  return side === "player" ? "P" : side === "neutral" ? "N" : "E";
}

const SPAWN_GROUPS: { side: SpawnKey; summon: boolean; label: string }[] = [
  { side: "playerSpawns", summon: false, label: "Heróis" },
  { side: "playerSpawns", summon: true, label: "Invocações aliadas" },
  { side: "enemySpawns", summon: false, label: "Inimigos" },
  { side: "neutralSpawns", summon: false, label: "Feras neutras" },
  { side: "neutralSpawns", summon: true, label: "Invocações neutras" },
];

/** Everyone the Map Editor can drop on a board. Two more than DEFAULT_HEROES, which is the
 * starting four the campaign and the inn are built around — the Lancer and the Conjurer are
 * party members too, and a map being authored should be able to place them. Kept separate
 * so widening the editor's reach does not quietly recruit them into a campaign. */
const EDITOR_HEROES: { name: string; classId: ClassId }[] = [
  ...DEFAULT_HEROES,
  { name: "Aldric", classId: "aldric" as ClassId },
  { name: "Malrec", classId: "conjurer" as ClassId },
].sort((a, b) => byName(a.name, b.name));

/** Writes the draft to src/game/maps/<id><serial>.json through the dev server's
 * /__map-save route (scripts/map-save-plugin.mjs). Only reachable while `npm run dev`
 * is running; a built/deployed app has no project-file route, so the save must fail
 * clearly instead of claiming a browser-local copy is a game map. */
async function saveMapToRepo(draft: MapDraft): Promise<{ ok: true; serial: number; file: string } | { ok: false; error: string }> {
  try {
    const dex = await fetch("/__map-save", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ...draft, id: normalizeScenarioId(draft.id), decorations: removeWallsUnderWatchtowerEntrances(draft.decorations) }),
    });
    let body: { ok?: boolean; serial?: number; file?: string; error?: string };
    try {
      body = (await dex.json()) as typeof body;
    } catch {
      return { ok: false, error: `a rota /__map-save respondeu HTTP ${dex.status}, sem confirmação válida` };
    }
    if (!dex.ok || !body.ok) return { ok: false, error: body.error ?? `a rota /__map-save respondeu HTTP ${dex.status}` };
    if (!Number.isInteger(body.serial) || (body.serial ?? 0) < 1 || typeof body.file !== "string" || !body.file) {
      return { ok: false, error: "a rota /__map-save não confirmou o arquivo e a versão gravados" };
    }
    return { ok: true, serial: body.serial!, file: body.file };
  } catch (err) {
    const detail = err instanceof Error ? err.message : String(err);
    return { ok: false, error: `não foi possível acessar /__map-save (${detail})` };
  }
}

/** Deletes one saved map file through the dev server's /__map-delete route. Same
 * constraint as saving: only reachable while `npm run dev` is running. */
async function deleteMapFile(file: string): Promise<{ ok: true } | { ok: false; error: string }> {
  try {
    const dex = await fetch("/__map-delete", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ file }),
    });
    const body = (await dex.json()) as { ok?: boolean; error?: string; stillOnDisk?: boolean };
    if (!dex.ok || !body.ok) return { ok: false, error: body.error ?? `HTTP ${dex.status}` };
    if (body.stillOnDisk) return { ok: false, error: "o arquivo continua no disco" };
    return { ok: true };
  } catch (err) {
    return { ok: false, error: err instanceof Error ? err.message : String(err) };
  }
}

/** Decoration ids "Gerar terreno" leaves out of its random scatter — per direct
 * instruction, a prop can be too distinctive to want scattered at random without pulling
 * it out of DECORATIONS entirely and losing manual placement too. Browser-local, same as
 * the version store: this is an editor preference, not campaign data. */
function loadDecoShuffleExclude(): string[] {
  try {
    const raw = window.localStorage.getItem(DECO_SHUFFLE_EXCLUDE_KEY);
    const parsed = raw ? (JSON.parse(raw) as unknown) : [];
    return Array.isArray(parsed) ? parsed.filter((x): x is string => typeof x === "string") : [];
  } catch {
    return [];
  }
}

function saveDecoShuffleExclude(ids: string[]) {
  try {
    window.localStorage.setItem(DECO_SHUFFLE_EXCLUDE_KEY, JSON.stringify(ids));
  } catch {
    // ignore
  }
}

const TERRAIN_SWATCH: Record<TerrainId, string> = {
  plains: "#9c8f6f",
  woods: "#3f5c3a",
  ruins: "#6b6560",
  water: "#2c5f7a",
  ember: "#7a2c2c",
  hill: "#6c6c6c",
  flame: "#b5501f",
  column: "#4a4a52",
  nave: "#26262c",
  barricade: "#454545",
  door: "#373737",
  void: "#050505",
  snow: "#d8dee2",
};

const BUILDER_TERRAIN: TerrainId[] = [
  "plains",
  "woods",
  "water",
  "ruins",
  "ember",
  "hill",
  "flame",
  "nave",
  "column",
  // "barricade" is deliberately not here: it is a decoration now, placed with the Decoração
  // brush, which lays its terrain with it. Painting the bare tile still works — a map that
  // already had one keeps it, and the prop is derived on load — but authoring goes one way.
  // "chest" no longer exists as a TerrainId at all — a chest is purely a decoration
  // (locked-chest/chest-medium/chest-large) that never touches the tile underneath it.
  // highwood/deadtree/highruin used to be listed here too — retired entirely per direct
  // instruction (they were mechanically identical to "hill", just three redundant visual
  // reskins of it — see clearScrappedGroundTiles in data.ts, which converted every existing
  // occurrence to hill + a matching decoration). They no longer exist as a TerrainId at all.
  "door",
  "void",
  "snow",
];

/** Variants removed from the editor's "Versões" picker, per direct request. Hidden rather
 * than deleted: variant indices are positional, so dropping one would shift every later
 * variant and repaint saved maps. plains 15 = "Trilha de Terra". */
const HIDDEN_VARIANTS: Partial<Record<TerrainId, number[]>> = {
  plains: [15, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38],
  // Keep these saved-map indices intact while removing them from the water picker.
  water: [3, 7],
};

const VARIANT_LABEL: Partial<Record<TerrainId, string[]>> = {
  plains: [
    "Planície sombria", "Planície florida", "Planície original", "Antiga", "Terra", "Pedra", "Cinza", "Pedras",
    "Clareira", "Rochas", "Lajedo", "Pedregulho", "Prado", "Flores silvestres", "Relva", "Trilha de Terra",
    "Terra com pedregulhos", "Lama com pegadas", "Grama viçosa", "Grama com trevos", "Grama com arbustos",
    "Rua de cascalho", "Caminho de terra", "Calçamento de pedras", "Rua em ruínas",
    "Calçamento destruído", "Trilha de pedras", "Pedregulho antigo", "Trilha lamacenta",
    "Piso de madeira",
    "Piso de madeira rústica",
    "Piso de taverna escuro",
    "Piso hexagonal de madeira",
    "Piso de madeira remendada",
    "Piso de madeira em mosaico",
    "Piso de tábuas usadas",
    "Piso de taverna clássica",
    "Piso de taverna tranquila",
    "City · Solo contínuo 001",
    "Planície · Solo contínuo 001",
    "Madeira discreta · Solo contínuo 001",
    "Grama alta · Solo contínuo 001",
    "Planície escura · Solo contínuo 001",
    "Planície · Fotográfica 009", "Prado · Fotográfico 009", "Pradaria · Fotográfica 009", "Farmlands · Terra cultivada", "Farmlands · Trilha de cascalho",
  ],
  woods: ["Solo de bosque", "Bosque sombrio", "Bosque", "Sebes", "Pinhal", "Bosque 04", "Terra", "Bosque 12", "Bosque 13", "Bosque · Solo contínuo 001", "Bosque · Solo escuro fotográfico 011", "Bosque · Agulhas fotográficas 011"],
  ruins: ["Ruínas sombrias", "Ruínas originais", "Pedra 02", "Pedra 03", "Pedra 04", "Pátio mosaico", "Lajes partidas", "Ruínas · Solo contínuo 001"],
  water: ["Água costeira", "Antiga", "Praia", "Pântano", "Costa baixo", "Costa esq.", "Costa dir.", "Mar fundo", "Mar fundo 2", "Costa 01", "Costa 02", "Ponta baixo 01", "Ponta baixo 02", "Água rasa", "Água rasa 2", "Água costa", "Água costa 2", "Pântano escuro", "Praia", "Rio", "Mar", "Mar profundo", "Água · Solo contínuo 001", "Gruta · Lago fotográfico 006"],
  ember: ["Brasa", "Brasa 2", "Antiga", "Cinzas", "Brasa viva", "Brasa · Solo contínuo 001"],
  hill: ["Platô rochoso", "Trilha elevada", "Ruínas elevadas", "Platô musgoso", "Colina · Solo contínuo 001"],
  flame: ["Chama", "Antiga", "Fogo", "Chama · Solo contínuo 001"],
  nave: ["Laje", "Laje Negra", "Laje · Solo contínuo 001", "Templo antigo · Calcário contínuo 001", "Templo antigo · Basalto contínuo 001", "Masmorra · Lajes contínuas 001", "Masmorra · Tijolos contínuos 001", "Caverna · Solo contínuo 001", "Caverna com cristais · Solo contínuo 001", "Caverna · Cristais marcantes 002", "Caverna · Ardósia contínua 003", "Caverna · Cascalho contínuo 003", "Gruta · Basalto contínuo 004", "Gruta · Pedra rachada 004", "Gruta · Musgo contínuo 004", "Gruta · Terra fotográfica 006", "Gruta · Calcário fotográfico 006", "Gruta · Musgo fotográfico 006", "Farmlands · Tábuas de carvalho"],
  column: ["Coluna", "Antiga", "Coluna · Solo contínuo 001"],
  snow: [
    "Neve Rasa 4", "Neve Rasa 5", "Neve Funda 2",
    "Mato Seco", "Folhas Mortas", "Pinhal Ressequido", "Bosque Gelado",
    "Pinhal Frio", "Folhas Congeladas", "Brejo Congelado", "Urze Gelada",
    "Planície Ressequida", "Planície Congelada", "Encosta Morta", "Arbustos Frios",
    "Neve · Solo contínuo 001",
    "Tundra sem neve · Solo contínuo 001",
    "Tundra com neve · Solo contínuo 001",
    "Neve · Vento fotográfico 010", "Neve · Crosta fotográfica 010", "Neve · Pó fotográfico 010",
  ],
};

/** Hover text for a terrain type: its combat stats plus terrainNote()'s callout, so the
 * editor documents what each tile actually does instead of just naming it. */
function terrainHint(t: TerrainId, variant?: number): string {
  const d = TERRAIN[t];
  // Which art file this cell actually paints with. Two variants of one terrain are
  // identical in every rule below, so the name is the only thing that tells them apart.
  const label = variant == null ? null : (VARIANT_LABEL[t]?.[variant] ?? tileVariantName(t, variant));
  const head = d.name;
  const parts = [head, d.passable ? `Mov ${d.moveCost}` : "Intransponível", `Def +${d.def}`, `Atk +${d.atk}`];
  if (d.blocksShot) parts.push("bloqueia tiro/visão");
  if (d.hazardDice) parts.push(`dano ${d.hazardDice}D${d.hazardFaces} ao entrar e a cada turno`);
  const note = terrainNote(t);
  return note ? `${parts.join(" · ")} — ${note}` : parts.join(" · ");
}

function ResizableEditorPanel({
  children,
  className,
  style,
  title,
  minHeight,
  contentClassName = "h-full w-full",
}: {
  children: ReactNode;
  className: string;
  style?: CSSProperties;
  title: string;
  minHeight: number;
  contentClassName?: string;
}) {
  const panelRef = useRef<HTMLDivElement>(null);
  const resizeStart = useRef<{ x: number; y: number; width: number; height: number } | null>(null);
  const [size, setSize] = useState<{ width: number; height: number } | null>(null);

  const startResize = (event: ReactPointerEvent<HTMLButtonElement>) => {
    const panel = panelRef.current;
    if (!panel) return;
    event.preventDefault();
    const rect = panel.getBoundingClientRect();
    resizeStart.current = { x: event.clientX, y: event.clientY, width: rect.width, height: rect.height };
    event.currentTarget.setPointerCapture(event.pointerId);
  };

  const resize = (event: ReactPointerEvent<HTMLButtonElement>) => {
    const start = resizeStart.current;
    if (!start) return;
    setSize({
      width: Math.max(280, start.width + event.clientX - start.x),
      height: Math.max(minHeight, start.height + event.clientY - start.y),
    });
  };

  const stopResize = (event: ReactPointerEvent<HTMLButtonElement>) => {
    resizeStart.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
  };

  return (
    <div ref={panelRef} className={`relative shrink-0 ${className}`} style={{ ...style, ...(size ?? {}) }} title={title}>
      <div className={contentClassName}>{children}</div>
      <button
        type="button"
        className="absolute bottom-0 right-0 z-10 grid size-8 touch-none place-items-center rounded-tl-md border-l border-t border-border bg-bg/90 text-muted cursor-se-resize"
        aria-label={title}
        onPointerDown={startResize}
        onPointerMove={resize}
        onPointerUp={stopResize}
        onPointerCancel={stopResize}
      >
        <Grip className="size-4 rotate-45" />
      </button>
    </div>
  );
}

export function MapEditorScreen({
  art,
  onBack,
  onPlaytest,
  initialDraft,
  onDraftChange,
}: {
  art: GameArt;
  onBack: () => void;
  onPlaytest: (m: Mission, playerLevels: Record<string, number>, enemyLevels: Record<number, number>, neutralLevels: Record<number, number>) => void;
  /** The map to reopen with — what was being edited before a playtest took the screen away. */
  initialDraft?: MapDraft | null;
  onDraftChange?: (draft: MapDraft) => void;
}) {
  const [showPreview, setShowPreview] = useState(true);
  const [showTechnicalMap, setShowTechnicalMap] = useState(false);
  // Keep the visual preview current with placement, deletion, and orientation edits.
  const [previewMission, setPreviewMission] = useState<Mission | null>(null);
  const immediateFxPreviewDraftRef = useRef<MapDraft | null>(null);
  /** Which DialogTree the DialogEditor modal is currently open for, if any — the mission's
   * own intro/outro, or one neutral spawn's own conversation. */
  const [dialogEditorTarget, setDialogEditorTarget] = useState<{ kind: "intro" } | { kind: "outro" } | { kind: "spawn"; index: number } | null>(null);
  const [shuffleExclude, setShuffleExclude] = useState<Set<string>>(() => new Set(loadDecoShuffleExclude()));
  const toggleShuffleExclude = (id: string) => {
    setShuffleExclude((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      saveDecoShuffleExclude([...next]);
      return next;
    });
  };
  const [versionStore, setVersionStore] = useState<Record<string, MapVersion[]>>(() => loadVersionStore());
  const [activeVersions, setActiveVersions] = useState<Record<string, number>>(() => loadActiveVersions());
  const [draft, setDraft] = useState<MapDraft>(() => initialDraft ?? blankDraft());
  useEffect(() => {
    setDraft((d) => {
      const misplaced = d.enemySpawns.filter((s) => isSummonClass(s.classId) || CLASSES[s.classId].role.startsWith("Civil"));
      if (misplaced.length === 0) return d;
      return {
        ...d,
        enemySpawns: d.enemySpawns.filter((s) => !isSummonClass(s.classId) && !CLASSES[s.classId].role.startsWith("Civil")),
        neutralSpawns: [...(d.neutralSpawns ?? []), ...misplaced],
      };
    });
  }, []);
  useEffect(() => {
    setDraft(d => {
      const decorations = removeWallsUnderWatchtowerEntrances(d.decorations);
      return decorations.length === d.decorations.length ? d : { ...d, decorations };
    });
  }, [removeWallsUnderWatchtowerEntrances]);
  // Undo/redo for the map editor, up to 10 steps each way. A burst of rapid changes (typing
  // in a text field, dragging a paint stroke across several hexes) is coalesced into a
  // single step by waiting for a short pause before committing one to history, so undo
  // moves through whole edits instead of one keystroke or one hex at a time.
  const [draftPast, setDraftPast] = useState<MapDraft[]>([]);
  const [draftFuture, setDraftFuture] = useState<MapDraft[]>([]);
  const lastDraftRef = useRef(draft);
  const pendingBeforeRef = useRef<MapDraft | null>(null);
  const coalesceTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const applyingHistoryRef = useRef(false);
  useEffect(() => {
    const previous = lastDraftRef.current;
    lastDraftRef.current = draft;
    if (applyingHistoryRef.current) {
      applyingHistoryRef.current = false;
      return;
    }
    if (previous === draft) return;
    if (pendingBeforeRef.current === null) pendingBeforeRef.current = previous;
    if (coalesceTimerRef.current) clearTimeout(coalesceTimerRef.current);
    coalesceTimerRef.current = setTimeout(() => {
      const before = pendingBeforeRef.current;
      pendingBeforeRef.current = null;
      coalesceTimerRef.current = null;
      if (before === null) return;
      setDraftPast((p) => [...p, before].slice(-10));
      setDraftFuture([]);
    }, 600);
  }, [draft]);
  useEffect(
    () => () => {
      if (coalesceTimerRef.current) clearTimeout(coalesceTimerRef.current);
    },
    [],
  );
  const undoDraft = useCallback(() => {
    if (draftPast.length === 0) return;
    if (coalesceTimerRef.current) {
      clearTimeout(coalesceTimerRef.current);
      coalesceTimerRef.current = null;
      pendingBeforeRef.current = null;
    }
    const prevState = draftPast[draftPast.length - 1]!;
    setDraftPast((p) => p.slice(0, -1));
    setDraftFuture((f) => [draft, ...f].slice(0, 10));
    applyingHistoryRef.current = true;
    setDraft(prevState);
  }, [draft, draftPast]);
  const redoDraft = useCallback(() => {
    if (draftFuture.length === 0) return;
    if (coalesceTimerRef.current) {
      clearTimeout(coalesceTimerRef.current);
      coalesceTimerRef.current = null;
      pendingBeforeRef.current = null;
    }
    const nextState = draftFuture[0]!;
    setDraftFuture((f) => f.slice(1));
    setDraftPast((p) => [...p, draft].slice(-10));
    applyingHistoryRef.current = true;
    setDraft(nextState);
  }, [draft, draftFuture]);
  const [brush, setBrush] = useState<TerrainId>("plains");
  const [terrain3D, setTerrain3D] = useState(false);
  const [elevationTool, setElevationTool] = useState<"raise" | "lower" | "level">("raise");
  const [elevationLevel, setElevationLevel] = useState(0);
  const [elevationRadius, setElevationRadius] = useState(0);
  const [waterErase, setWaterErase] = useState(false);
  const [waterLevel, setWaterLevel] = useState(0.5);
  const [waterRadius, setWaterRadius] = useState(0);
  const [waterSize, setWaterSize] = useState(1);
  const [waterShape, setWaterShape] = useState<"round" | "square">("round");
  const [variant, setVariant] = useState(0);
  const [cityMode, setCityMode] = useState(false);
  // While armed, clicking a hex in Terreno mode turns it instead of painting it.
  const [turning, setTurning] = useState(false);
  const [turningDeco, setTurningDeco] = useState(false);
  const [decoBrush, setDecoBrush] = useState<string>(Object.keys(DECORATIONS)[0]!);
  const [wallOrientation, setWallOrientation] = useState<"horizontal" | "vertical">("horizontal");
  // A placed prop is selected by clicking any hex of its footprint; Delete removes this exact placement.
  const [selectedPlacedDecoration, setSelectedPlacedDecoration] = useState<{ id: string; x: number; y: number; rot?: number } | null>(null);
  const [decoSection, setDecoSection] = useState("Todas");
  const [fxBrush, setFxBrush] = useState<PlaceableElementKind>("fire");
  const [fxFamily, setFxFamily] = useState<"regular" | "procedural_pixel">("regular");
  const [pixelFxBrush, setPixelFxBrush] = useState<PixelElement>("fire");
  const [pixelFxPresetId, setPixelFxPresetId] = useState("procedural_pixel_fire");
  const [pixelFxSettings, setPixelFxSettings] = useState<PixelElementSettings>(() => pixelDefaults("fire"));
  const [mode, setMode] = useState<"paint" | "elevation" | "water" | "player" | "enemy" | "npc" | "summon" | "decoration" | "architecture" | "elementalFx">("paint");
  // Which summon class the "Invocação" brush drops. Summons live in playerSpawns alongside
  // the heroes — the class itself says which of the two a spawn is (isSummonClass), so
  // there is no third list to keep in sync and no saved map to migrate.
  const [summonBrush, setSummonBrush] = useState<ClassId>(SUMMON_CLASSES[0] ?? "familiar");
  const [npcBrush, setNpcBrush] = useState<EncounterNpcId | "breadLady">("breadLady");
  // Summons can be allied or neutral, never enemies.
  const [summonSide, setSummonSide] = useState<"player" | "neutral">("player");
  const [gridStyle, setGridStyle] = useState<"hex" | "square">("hex");
  const [exportText, setExportText] = useState<string | null>(null);
  const [copyOk, setCopyOk] = useState(false);
  const [savingMap, setSavingMap] = useState(false);
  const savingMapRef = useRef(false);
  // Every message carries a serial so repeating an action visibly re-fires: saving twice in
  // a row used to leave the same sentence sitting there, indistinguishable from nothing
  // having happened.
  const [note, setNoteRaw] = useState<{ text: string; n: number } | null>(null);
  /** The loud one: a full-width panel that stays until dismissed, for the answer to "did it
   * actually save". The small note above it is for running commentary. */
  const [bigNote, setBigNote] = useState<{ ok: boolean; title: string; lines: string[]; dump?: string } | null>(null);
  const noteSerial = useRef(0);
  const setNote = useCallback((text: string) => {
    noteSerial.current += 1;
    setNoteRaw({ text, n: noteSerial.current });
  }, []);
  useEffect(() => {
    onDraftChange?.(draft);
  }, [draft, onDraftChange]);

  useEffect(() => {
    if (!showPreview) return;
    if (immediateFxPreviewDraftRef.current === draft) {
      immediateFxPreviewDraftRef.current = null;
      return;
    }
    setPreviewMission(draftToMission(draft));
  }, [draft, showPreview]);

  const [showLocations, setShowLocations] = useState(false);
  const [showRandomEncounters, setShowRandomEncounters] = useState(false);
  const [encounterRegions, setEncounterRegions] = useState(() => RANDOM_ENCOUNTER_REGIONS);
  // Locais' own guaranteed-local copy (see saveLocaisLocal's doc comment in mapstore.ts) —
  // read once here so a returning session picks up wherever it last actually saved instead
  // of the shipped/static defaults, same "localStorage wins over static data" precedence
  // missionById already gives an activated map draft.
  const locaisLocal = loadLocaisLocal();
  // Play order per location, keyed by location id. Seeded from what ALL_LOCATIONS resolved
  // to, so a location with no stored order still lists its missions in the order they play.
  const [order, setOrder] = useState<Record<string, string[]>>(
    () => Object.fromEntries((locaisLocal
      ? locationsForOrder(locaisLocal.order, locaisLocal.locationOrder, locaisLocal.submaps, locaisLocal.knownMissionIds)
      : ALL_LOCATIONS).map((l) => [l.id, [...l.missionIds]])),
  );
  // This is the chapter order between world-map markers. It is independent from the
  // missions listed inside each location and does not move the markers visually.
  const [locationOrder, setLocationOrder] = useState<string[]>(
    () => locaisLocal?.locationOrder ?? ALL_LOCATIONS.map((location) => location.id),
  );
  // Transversal Dungeon submaps per location (see WorldLocation.submaps) — purely authoring
  // bookkeeping, so unlike order/slots it has no repo-file/dev-server write of its own; the
  // guaranteed local save below is the only copy.
  const [submaps, setSubmaps] = useState<Record<string, { missionId: string; floor: number }[]>>(() => ({ ...DEFAULT_LOCATION_SUBMAPS, ...locaisLocal?.submaps }));

  // Serialize repo writes so rapid arrow presses cannot let an older request win.
  const orderWrites = useRef<Promise<void>>(Promise.resolve());
  const saveOrder = (next: Record<string, string[]>, nextLocations = locationOrder): Promise<void> => {
    setOrder(next);
    setLocationOrder(nextLocations);
    const localOk = saveLocaisLocal({ order: next, slots, locationOrder: nextLocations, submaps });
    if (!localOk) {
      setNote("NÃO SALVOU: o navegador recusou gravar a ordem.");
      return Promise.resolve();
    }
    setNote("Ordem salva neste navegador e aplicada à campanha.");
    const write = async () => {
      try {
        for (const [route, payload] of [["/__map-order", next], ["/__location-order", nextLocations]] as const) {
          const dex = await fetch(route, {
            method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(payload),
          });
          const body = await dex.json() as { ok?: boolean; error?: string };
          if (!dex.ok || !body.ok) throw new Error(body.error ?? `HTTP ${dex.status}`);
        }
        setNote("Ordem das missões e dos Locais salva no navegador e no repositório.");
      } catch {
        setNote("Ordem salva neste navegador e aplicada à campanha (sem cópia no repositório).");
      }
    };
    orderWrites.current = orderWrites.current.then(write, write);
    return orderWrites.current;
  };

  /** Sends a mission to another location. It leaves every other list and joins the end of
   * the destination's, which is then reordered with the arrows. */
  const transferMission = (missionId: string, toLocationId: string) => {
    const next: Record<string, string[]> = {};
    for (const [locId, ids] of Object.entries(order)) {
      const kept = ids.filter((id) => id !== missionId);
      next[locId] = kept;
    }
    next[toLocationId] = [...(next[toLocationId] ?? []), missionId];
    void saveOrder(next);
  };

  const saveEncounterRegions = async (next: typeof encounterRegions) => {
    setEncounterRegions(next);
    try {
      const dex = await fetch("/__random-encounters", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ regions: next }),
      });
      const body = (await dex.json()) as { ok?: boolean; error?: string };
      if (!dex.ok || !body.ok) throw new Error(body.error ?? `HTTP ${dex.status}`);
      setNote("Regiões de R-Encounter atualizadas.");
    } catch (err) {
      setNote(`Sem servidor de dev — regiões não gravadas (${err instanceof Error ? err.message : String(err)}).`);
    }
  };

  const moveInOrder = (locationId: string, missionId: string, dir: -1 | 1) => {
    const list = [...(order[locationId] ?? [])];
    const i = list.indexOf(missionId);
    const j = i + dir;
    if (i < 0 || j < 0 || j >= list.length) return;
    [list[i], list[j]] = [list[j]!, list[i]!];
    void saveOrder({ ...order, [locationId]: list });
  };

  const moveLocationInOrder = (locationId: string, dir: -1 | 1) => {
    const next = [...locationOrder];
    const i = next.indexOf(locationId);
    const j = i + dir;
    if (i < 0 || j < 0 || j >= next.length) return;
    [next[i], next[j]] = [next[j]!, next[i]!];
    void saveOrder(order, next);
  };

  const versions = versionStore[draft.id] ?? [];
  const [armedDelete, setArmedDelete] = useState("");
  const removeFromLocation = (locationId: string, missionId: string) => {
    const key = `location:${locationId}:${missionId}`;
    if (armedDelete !== key) {
      setArmedDelete(key);
      setNote(`Remover? Clique de novo para tirar este mapa de Locais. O arquivo do mapa não será apagado.`);
      return;
    }
    setArmedDelete("");
    setOrder((current) => ({ ...current, [locationId]: (current[locationId] ?? []).filter((id) => id !== missionId) }));
    setNote(`Mapa removido deste Local. Clique Salvar em Locais para gravar a campanha.`);
  };
  const [repoFiles, setRepoFiles] = useState<MapFile[]>(() => savedVersionsFor(draft.id));
  const refreshRepoFiles = useCallback(async (id: string) => {
    try {
      const response = await fetch(`/__map-list?id=${encodeURIComponent(id)}`);
      const body = (await response.json()) as { ok?: boolean; files?: MapFile[] };
      if (!response.ok || !body.ok || !Array.isArray(body.files)) throw new Error("lista indisponível");
      setRepoFiles(body.files);
    } catch {
      // Built releases have no dev-only endpoint. Their static list is still useful.
      setRepoFiles(savedVersionsFor(id));
    }
  }, []);
  useEffect(() => { void refreshRepoFiles(draft.id); }, [draft.id, refreshRepoFiles]);
  const repoLatest = repoFiles.reduce((latest, file) => Math.max(latest, file.serial), 0);
  // "Arquivo mais novo"/"versão mais nova" used to be decided within each list on its own
  // serial numbering — repository files (thebridge020.json...) and browser-local versions
  // (v001, v002...) count on two completely independent counters, so the higher-numbered
  // file could easily be older in real time than a local version saved after it. Compared
  // by actual savedAt instead, across both lists, so only whichever one is truly the most
  // recent save gets tagged, wherever it happens to live.
  const latestRepoFile = repoFiles.reduce((best: MapFile | null, f) => (!best || f.savedAt > best.savedAt ? f : best), null);
  const latestVersion = versions.reduce((best: MapVersion | null, v) => (!best || v.savedAt > best.savedAt ? v : best), null);
  const trueLatestIsVersion = !!latestVersion && (!latestRepoFile || latestVersion.savedAt > latestRepoFile.savedAt);
  const [savedLocationMaps, setSavedLocationMaps] = useState<{ id: string; title: string; index: number; hub?: boolean }[]>(() =>
    savedScenarios().map((scenario) => ({ id: scenario.id, title: latestSavedDraft(scenario.id)?.title ?? scenario.id, index: latestSavedDraft(scenario.id)?.index ?? 0, hub: latestSavedDraft(scenario.id)?.hub })),
  );
  const refreshSavedLocationMaps = useCallback(async () => {
    try {
      const response = await fetch("/__map-list");
      const body = (await response.json()) as { ok?: boolean; scenarios?: { id: string; title: string; index: number; hub?: boolean }[] };
      if (!response.ok || !body.ok || !Array.isArray(body.scenarios)) return;
      setSavedLocationMaps(body.scenarios);
    } catch {
      // The static list stays usable outside the local dev server.
    }
  }, []);
  useEffect(() => { void refreshSavedLocationMaps(); }, [refreshSavedLocationMaps]);
  // Locais is the campaign list. A saved map becomes playable only after it is added
  // to a Local; saved-but-unassigned maps remain available below solely for assignment.
  const campaignIds = useMemo(() => {
    const ids = new Set<string>();
    for (const locationId of locationOrder) {
      const fallback = ALL_LOCATIONS.find((location) => location.id === locationId)?.missionIds ?? [];
      for (const id of order[locationId] ?? fallback) ids.add(id);
    }
    return ids;
  }, [order, locationOrder]);
  /** Reserve maps only. Campaign maps stay in their own campaign pickers; use the refreshed
   * on-disk list so newly added reserve files appear without relying on mapstore's eager glob. */
  const pickable = (() => {
    const byId = new Map<string, { id: string; title: string; files: number; local: number }>();
    for (const s of savedScenarios()) {
      if (campaignIds.has(s.id)) continue;
      byId.set(s.id, { id: s.id, title: latestSavedDraft(s.id)?.title ?? s.id, files: s.files, local: (versionStore[s.id] ?? []).length });
    }
    for (const map of savedLocationMaps) {
      if (campaignIds.has(map.id)) continue;
      const existing = byId.get(map.id);
      if (existing) existing.title = map.title || existing.title;
      else byId.set(map.id, { id: map.id, title: map.title || map.id, files: 1, local: (versionStore[map.id] ?? []).length });
    }
    for (const [id, list] of Object.entries(versionStore)) {
      if (campaignIds.has(id)) continue;
      const existing = byId.get(id);
      if (existing) existing.local = list.length;
      else if (list.length > 0) byId.set(id, { id, title: list[list.length - 1].draft.title || id, files: 0, local: list.length });
    }
    return [...byId.values()].sort((a, b) => byName(a.title, b.title));
  })();
  /** Every "reserva" map: not in the campaign, full stop, no in-between — a saved-but-
   * unassigned file (savedLocationMaps) or a shipped-but-unassigned mission (R1/R2 —
   * "vertente"/"portao" — and anything else in ALL_MISSIONS never given to a Local). This is
   * both the encounter-region assignment pool AND the Locais "Adicionar mapa…" (reserva)
   * picker's source — a map only ever needs one "not yet campanha" list. */
  const randomEncounterReferences = useMemo(() => {
    const known = new Map<string, { id: string; title: string; index: number }>();
    for (const map of savedLocationMaps) if (!campaignIds.has(map.id)) known.set(map.id, map);
    for (const m of ALL_MISSIONS) if (!m.hub && !campaignIds.has(m.id) && !known.has(m.id)) known.set(m.id, { id: m.id, title: m.title, index: m.index });
    return [...known.values()].sort((a, b) => a.index - b.index || byName(a.title, b.title));
  }, [campaignIds, savedLocationMaps]);
  // ONLY missions actually assigned to a Local — the two dropdowns split on purpose (this
  // one is campaign-only; "Abrir mapa salvo" below is every saved file, reserves and scratch
  // maps included), so this must never pull in anything else again: not R1/R2 ("vertente"/
  // "portao", prepared-but-unassigned reserve maps), not an arbitrary saved-but-unassigned
  // map. Assigning something to a Local is what makes it campanha in the first place.
  const campaignMapReferences = useMemo(() => {
    const known = new Map<string, { id: string; title: string; index: number }>();
    for (const id of campaignIds) {
      const mission = missionById(id);
      if (mission && !mission.hub) known.set(id, { id, title: mission.title, index: mission.index });
    }
    return [...known.values()].sort((a, b) => a.index - b.index || byName(a.title, b.title));
  }, [campaignIds]);
  const campaignLoadOptions = campaignMapReferences;
  /** Hub maps (the Inn, future towns) — their own "CityHubs" picker, kept apart from the
   * campaign list so they're easy to find. */
  const cityHubOptions = useMemo(
    () => {
      const hubs = new Map(ALL_MISSIONS.filter((m) => m.hub).map((m) => [m.id, { id: m.id, title: m.title, index: m.index }]));
      for (const map of savedLocationMaps) if (map.hub) hubs.set(map.id, map);
      return [...hubs.values()].sort((a, b) => a.index - b.index || byName(a.title, b.title));
    },
    [savedLocationMaps],
  );
  /** Loads a campaign (or CityHubs) map into the editor — the latest file on disk first. */
  const loadCampaignMap = async (id: string) => {
    // Same staleness as the "Abrir mapa salvo" picker below (see its own comment) —
    // latestSavedDraft reads mapstore.ts's eager import.meta.glob snapshot, frozen
    // at page load and never refreshed by map-save-plugin.mjs's saves on purpose.
    // Ask the dev server for the real latest file first; fall back to the stale
    // snapshot only when there's none to ask (a built release).
    try {
      const response = await fetch(`/__map-list?id=${encodeURIComponent(id)}`);
      const body = (await response.json()) as { ok?: boolean; files?: MapFile[] };
      if (!response.ok || !body.ok || !Array.isArray(body.files) || body.files.length === 0) throw new Error("lista indisponível");
      const latestFile = body.files.reduce((best: MapFile, f) => (f.serial > best.serial ? f : best));
      const m = draftToMission(latestFile.draft);
      setDraft(missionToDraft(m));
      setNote(`Carregado "${m.title}" (${m.id}) no editor — ${m.cols}x${m.rows}.`);
      return;
    } catch {
      // No dev server (built release) — fall back to the static snapshot.
    }
    const saved = latestSavedDraft(id);
    // Saved drafts carry editor-only metadata such as the chosen replacement base.
    // Prefer that exact source when reopening a map, before its playable Mission view.
    const m = saved ? draftToMission(saved) : missionById(id);
    if (!m) return;
    setDraft(missionToDraft(m));
    setNote(`Carregado "${m.title}" (${m.id}) no editor — ${m.cols}x${m.rows}.`);
  };
  // Every saved map, campaign or reserve — a floor connector's target is picked from this
  // full pool rather than either list alone, since a Transversal Dungeon submap is typically
  // a reserve map (not assigned to any Local's missionIds — see WorldLocation.submaps) but
  // nothing stops an author pointing a connector at a regular campaign mission instead.
  const connectorTargetReferences = useMemo(
    () => [...campaignMapReferences, ...randomEncounterReferences].sort((a, b) => byName(a.title, b.title)),
    [campaignMapReferences, randomEncounterReferences],
  );
  const [slots, setSlots] = useState<Record<string, number>>(() => locaisLocal?.slots ?? LOCATION_SLOTS);

  /** Declares how many missions a location is meant to hold, so the editor can show what
   * is still to author. Writes src/game/map-slots.json through the dev server — config,
   * not a version, so it replaces the previous count instead of adding a serial. */
  const doSaveSlots = async (next: Record<string, number>) => {
    setSlots(next);
    const localOk = saveLocaisLocal({ order, slots: next, locationOrder, submaps });
    try {
      const dex = await fetch("/__map-slots", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(next),
      });
      const body = (await dex.json()) as { ok?: boolean; error?: string };
      if (!dex.ok || !body.ok) {
        setNote(localOk ? "Vagas salvas neste navegador." : `Não deu pra gravar as vagas: ${body.error ?? `HTTP ${dex.status}`}`);
        return;
      }
      setNote("Vagas do local atualizadas em src/game/map-slots.json.");
    } catch {
      setNote(localOk ? "Vagas salvas neste navegador (sem servidor de dev pro repositório)." : "NÃO SALVOU: nem localmente, nem no repositório.");
    }
  };
  /** Adds/replaces this location's floor entry for a mission (see WorldLocation.submaps) —
   * authoring bookkeeping for which reserve map is which Transversal Dungeon floor, so a
   * floor-connector's "Leva para" dropdown reads sensibly; it never touches missionIds, so it
   * cannot make a submap appear as its own card in the campaign menu. */
  const setLocationSubmap = (locationId: string, missionId: string, floor: number) => {
    setSubmaps((prev) => {
      const cleaned = Object.fromEntries(Object.entries(prev).map(([id, list]) => [id, list.filter(s => s.missionId !== missionId)]));
      const oldFloor = prev[locationId]?.find(s => s.missionId === missionId)?.floor;
      const list = (cleaned[locationId] ?? []).map(s => oldFloor != null && s.floor === floor ? { ...s, floor: oldFloor } : s);
      const next = { ...cleaned, [locationId]: [...list, { missionId, floor }].sort((a, b) => b.floor - a.floor) };
      const nextOrder = Object.fromEntries(Object.entries(order).map(([id, list]) => [id, list.filter(mapId => mapId !== missionId)]));
      setOrder(nextOrder);
      saveLocaisLocal({ order: nextOrder, slots, locationOrder, submaps: next });
      return next;
    });
  };
  const removeLocationSubmap = (locationId: string, missionId: string) => {
    setSubmaps((prev) => {
      const next = { ...prev, [locationId]: (prev[locationId] ?? []).filter((s) => s.missionId !== missionId) };
      saveLocaisLocal({ order, slots, locationOrder, submaps: next });
      return next;
    });
  };
  /** Re-reads the guaranteed-local Locais copy (see saveLocaisLocal in mapstore.ts) and
   * replaces order/slots/locationOrder with exactly that — called right as the Locais screen
   * opens (see its own onClick below), not just once at mount like these three useState
   * initializers are. order/locationOrder/slots otherwise stay frozen at whatever they were
   * the moment this component first mounted, for as long as the browser tab stays open —
   * which can be hours into a session — so a save made from a DIFFERENT tab in the same
   * browser (localStorage is shared browser-wide, same origin, across every tab) would
   * never reach this one's own state. saveScenarios's "Salvar" then writes the *entire*
   * current order/slots/locationOrder, every location included, so a location this tab
   * never actually learned about — because some other tab saved it after this one
   * mounted — would go out with whatever this tab's own stale stand-in for it was,
   * silently reverting or erasing a real change. Confirmed by reproducing it. This closes
   * that window from "however long the tab's been open" down to "however long the Locais
   * screen's been open". No-op (leaves state as-is) if nothing has ever been saved locally
   * yet. */
  const refreshLocaisState = () => {
    const fresh = loadLocaisLocal();
    if (!fresh) { setSubmaps(prev => ({ ...DEFAULT_LOCATION_SUBMAPS, ...prev })); return; }
    setOrder(Object.fromEntries(locationsForOrder(fresh.order, fresh.locationOrder, fresh.submaps, fresh.knownMissionIds).map((l) => [l.id, [...l.missionIds]])));
    setSlots(fresh.slots);
    setLocationOrder(fresh.locationOrder);
    setSubmaps({ ...DEFAULT_LOCATION_SUBMAPS, ...fresh.submaps });
  };
  useEffect(() => {
    const onStorage = (event: StorageEvent) => {
      if (event.key !== LOCAIS_LOCAL_KEY && event.key !== null) return;
      const fresh = loadLocaisLocal();
      if (!fresh) return;
      setOrder(Object.fromEntries(locationsForOrder(fresh.order, fresh.locationOrder, fresh.submaps, fresh.knownMissionIds).map((l) => [l.id, [...l.missionIds]])));
      setSlots(fresh.slots);
      setLocationOrder(fresh.locationOrder);
      setSubmaps({ ...DEFAULT_LOCATION_SUBMAPS, ...fresh.submaps });
    };
    window.addEventListener("storage", onStorage);
    return () => window.removeEventListener("storage", onStorage);
  }, []);
  /** Writes the Locais configuration — which missions each location holds, in what order,
   * and how many it is meant to hold. The local save (see saveLocaisLocal in mapstore.ts) is
   * the one this promises: it always works, needs no dev server, and is what every other
   * Locais/editor screen in this same browser reads from (see refreshLocaisState). The repo
   * write (src/game/map-order.json etc., through the dev server) happens too when one is
   * running — real files a build ships with — but it's a bonus on top, never the difference
   * between "saved" and "NÃO SALVOU" the way it used to be. */
  const saveScenarios = async () => {
    setBigNote(null);
    await orderWrites.current;
    const localOk = saveLocaisLocal({ order, slots, locationOrder, submaps });
    if (!localOk) {
      setBigNote({
        ok: false,
        title: "NÃO SALVOU",
        lines: [
          "O navegador recusou gravar (modo privado, armazenamento bloqueado ou cheio).",
          "O texto abaixo é a sua configuração. Copie e guarde: cola numa conversa e eu gravo por você.",
        ],
        dump: JSON.stringify({ order, locationOrder, slots, submaps }, null, 2),
      });
      return;
    }
    const post = async (route: string, payload: unknown) => {
      const dex = await fetch(route, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(payload),
      });
      const body = (await dex.json()) as { ok?: boolean; error?: string; file?: string; onDisk?: unknown };
      if (!dex.ok || !body.ok) throw new Error(body.error ?? `HTTP ${dex.status}`);
      return body;
    };
    try {
      const o = await post("/__map-order", order);
      const sl = await post("/__map-slots", slots);
      const lo = await post("/__location-order", locationOrder);
      const floors = await post("/__location-submaps", submaps);
      const locais = Object.keys((o.onDisk as Record<string, unknown>) ?? {}).length;
      const vagas = Object.keys((sl.onDisk as Record<string, unknown>) ?? {}).length;
      setBigNote({
        ok: true,
        title: "ESTÁ SALVO",
        lines: [
          "Salvo neste navegador — vale já, sem precisar de servidor de dev.",
          `Também gravado no repositório: ${o.file} — ${locais} ${locais === 1 ? "local" : "locais"} com ordem definida`,
          `${sl.file} — ${vagas} ${vagas === 1 ? "local" : "locais"} com vagas definidas`,
          `${lo.file} — sequência de locais da campanha confirmada`,
          `${floors.file} — submaps e andares confirmados`,
        ],
      });
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setBigNote({
        ok: true,
        title: "ESTÁ SALVO",
        lines: [
          "Salvo neste navegador — vale já, sem precisar de servidor de dev.",
          `Não foi gravado no repositório (${msg}) — só afeta o arquivo que um build usaria; sua campanha já está valendo com a cópia local.`,
        ],
      });
    }
  };

  const activeSerial = activeVersions[draft.id];

  const decoLookup = useMemo(() => {
    const m = new Map<string, string>();
    for (const p of draft.decorations) {
      const def = DECORATIONS[p.id];
      if (!def) continue;
      for (const f of placedFootprint(p)) m.set(`${p.x + f.dx},${p.y + f.dy}`, def.name);
    }
    return m;
  }, [draft.decorations]);

  const setTile = (i: number, t: TerrainId) => {
    // Painting under a prop is allowed, but it is worth saying out loud: the prop is only
    // the picture, so repainting here is what decides whether that house can be climbed.
    const x = i % draft.cols;
    const y = Math.floor(i / draft.cols);
    const under = draft.decorations.find((p) => {
      const def = DECORATIONS[p.id];
      return def?.tile && placedFootprint(p).some((f) => p.x + f.dx === x && p.y + f.dy === y);
    });
    if (under) {
      const def = DECORATIONS[under.id]!;
      if (t !== def.tile) {
        setNote(
          `${def.name} agora está sobre ${TERRAIN[t].name.toLowerCase()} — ${TERRAIN[t].passable ? "dá pra andar por cima" : "não se atravessa"}. O terreno manda, não o desenho.`,
        );
      }
    }
    setDraft((d) => {
      const tiles = d.tiles.slice();
      const tileVariants = d.tileVariants.slice();
      tiles[i] = t;
      tileVariants[i] = Math.min(variant, TILE_VARIANT_COUNT[t] - 1);
      return { ...d, tiles, tileVariants };
    });
  };

  /** Paints the selected terrain across the board while leaving units and decorations in
   * place, so a map can start from one coherent ground layer before detail work begins. */
  const replaceBaseTile = () => {
    const selectedVariant = Math.min(variant, (TILE_VARIANT_COUNT[brush] ?? 1) - 1);
    setDraft((d) => ({
      ...d,
      baseTile: brush,
      baseVariant: selectedVariant,
      tiles: Array.from({ length: d.cols * d.rows }, () => brush),
      tileVariants: Array.from({ length: d.cols * d.rows }, () => selectedVariant),
      tileRots: Array.from({ length: d.cols * d.rows }, () => 0),
    }));
    setNote(`Base inteira substituída por ${TERRAIN[brush].name.toLowerCase()} · ${VARIANT_LABEL[brush]?.[selectedVariant] ?? `arte ${selectedVariant + 1}`}.`);
  };

  /** Turns one hex's art a sixth of a circle. The tile, its variant and everything standing
   * on it are left alone — only which way the picture points, which is what makes a coast, a
   * road or a wall meet its neighbour instead of running the wrong way. Six presses come
   * back to where it started. */
  /** Where the red dot sits for a given turn: the hex's bottom side, carried around with the
   * art. Sixty degrees per step, measured from straight down, as a fraction of the cell's
   * half-height so both grids can place it the same way. */
  const bottomDot = (rot: number, radius: number) => {
    const a = Math.PI / 2 + ((rot % 6) * Math.PI) / 3;
    return { dx: Math.cos(a) * radius, dy: Math.sin(a) * radius };
  };

  const turnTile = (i: number) => {
    setDraft((d) => {
      const tileRots = (d.tileRots ?? Array.from({ length: d.tiles.length }, () => 0)).slice();
      tileRots[i] = ((tileRots[i] ?? 0) + 1) % 6;
      return { ...d, tileRots };
    });
  };

  const toggleSpawn = (x: number, y: number) => {
    setDraft((d) => {
      // Summons share the spawn list of the side they belong to — the class itself says a
      // spawn is a summon (isSummonClass), so there is no third list to keep in sync and no
      // saved map to migrate. Either brush on a side lifts whatever unit is on the cell, so
      // clicking a familiar with the Herói brush removes the familiar rather than no-opping.
      const key: SpawnKey =
        mode === "enemy"
          ? "enemySpawns"
          : mode === "npc"
            ? "neutralSpawns"
            : mode === "summon"
              ? summonSide === "neutral" ? "neutralSpawns" : "playerSpawns"
              : "playerSpawns";
      const list = d[key] ?? [];
      const existing = list.findIndex((s) => s.x === x && s.y === y);
      if (existing >= 0) {
        return { ...d, [key]: list.filter((_, i) => i !== existing) };
      }
      const summons = list.filter((s) => isSummonClass(s.classId)).length;
      const plain = list.length - summons;
      const spawn: DraftSpawn =
        mode === "summon"
          ? {
              name: `${CLASSES[summonBrush].name} ${summons + 1}`,
              classId: summonBrush,
              x,
              y,
              level: key === "playerSpawns" ? DEFAULT_TEST_LEVEL : enemyLevelFor(0),
            }
          : mode === "player"
            ? { name: `Herói ${plain + 1}`, classId: "swordsman", x, y, level: DEFAULT_TEST_LEVEL }
            : mode === "npc"
              ? npcBrush === "breadLady"
                ? { name: `Civil ${plain + 1}`, classId: "breadLady", x, y, level: enemyLevelFor(0) }
                : { ...encounterNpcSpawn(npcBrush, x, y), level: enemyLevelFor(0) }
              : { name: `Inimigo ${plain + 1}`, classId: "miliciaV2", x, y, level: enemyLevelFor(0) };
      return { ...d, [key]: [...list, spawn] };
    });
  };

  /** Turns the prop under (x, y) one sixth of a circle, footprint and all.
   *
   * Refuses the turn when the new footprint would leave the board or land on another
   * prop — the same rule placing one obeys — so a turn can never silently overlap. The
   * terrain the prop stamps moves with it: lifted off the hexes it leaves, laid on the
   * ones it takes, or a turned house would leave climbable ground behind it. */
  const turnDecoration = (x: number, y: number) => {
    setDraft((d) => {
      const hit = d.decorations.find((p) => placedFootprint(p).some((f) => p.x + f.dx === x && p.y + f.dy === y));
      if (!hit) {
        setNote("Nao ha decoracao nessa casa pra girar.");
        return d;
      }
      const def = DECORATIONS[hit.id];
      if (!def) return d;
      const nextRot = (((hit.rot ?? 0) + 1) % (def.model3d ? 4 : 6));
      const turned: DecorationPlacement = { ...hit, rot: nextRot, ...(def.model3d ? { wallOrientation: nextRot % 2 ? "vertical" : "horizontal" } : {}) };
      const before = placedFootprint(hit);
      const after = placedFootprint(turned);
      // A Waypoint is a flat ground marking, not a physical object — turning it can't "bump
      // into" another prop the way turning a real object could (see toggleDecoration's own
      // identical exemption).
      if (!def.exitKind) {
        const others = previewDecorationCells(d.decorations.filter((p) => p !== hit), turned);
        for (const f of after) {
          if (others.has(`${hit.x + f.dx},${hit.y + f.dy}`)) {
            setNote(`${def.name} nao cabe girada aqui — bateria em outra decoracao.`);
            return d;
          }
        }
      }
      const tiles = [...d.tiles];
      const tileVariants = [...d.tileVariants];
      const tileRots = [...(d.tileRots ?? [])];
      if (def.tile) {
        const base = baseForDraft(d);
        for (const f of before) {
          const i = cellIndex(hit.x + f.dx, hit.y + f.dy, d.cols, d.rows);
          if (i >= 0 && tiles[i] === def.tile) { tiles[i] = base.tile; tileVariants[i] = base.variant; tileRots[i] = 0; }
        }
        for (const f of after) {
          const i = cellIndex(hit.x + f.dx, hit.y + f.dy, d.cols, d.rows);
          if (i >= 0) tiles[i] = def.tile;
        }
      }
      setNote(`${def.name} em ${hit.x},${hit.y}: girada para ${(turned.rot ?? 0) * (def.model3d ? 90 : 60)}°${(turned.rot ?? 0) === 0 ? " (de volta ao original)" : ""}.`);
      if (def.model3d) setSelectedPlacedDecoration({ id: turned.id, x: turned.x, y: turned.y, rot: turned.rot });
      return { ...d, tiles, tileVariants, tileRots, decorations: removeWallsUnderWatchtowerEntrances(d.decorations.map((p) => (p === hit ? turned : p))) };
    });
  };

  /** The placement the two rule switches act on — the one clicked in the map. */
  const selectedPlacement = selectedPlacedDecoration
    ? findPreviewDecoration(draft.decorations, selectedPlacedDecoration)
    : undefined;
  const selectedPlacementIsSolidHouse = !!selectedPlacement && (
    HOUSE_DECOR_IDS.has(selectedPlacement.id) || BIG_HOUSE_DECOR_IDS.has(selectedPlacement.id) || SOLID_HOUSE_DECOR_IDS.has(selectedPlacement.id)
  );
  const selectedPlacementIsSolidCart = !!selectedPlacement && SOLID_CART_DECOR_IDS.has(selectedPlacement.id);
  const selectedArchitecture = selectedPlacement ? DECORATIONS[selectedPlacement.id]?.model3d : undefined;
  const selectedPlacementIsSolidArchitecture = selectedArchitecture === "wall" || selectedArchitecture === "door" || selectedArchitecture === "secretDoor";
  const activeWallOrientation = selectedArchitecture
    ? selectedPlacement?.wallOrientation ?? ((selectedPlacement?.rot ?? 0) % 2 ? "vertical" : "horizontal")
    : wallOrientation;
  const changeWallOrientation = (orientation: "horizontal" | "vertical") => {
    setWallOrientation(orientation);
    if (!selectedPlacement || !selectedArchitecture) return;
    const rot = orientation === "vertical" ? 1 : 0;
    setDraft(d => ({ ...d, decorations: removeWallsUnderWatchtowerEntrances(d.decorations.map(p =>
      p.id === selectedPlacement.id && p.x === selectedPlacement.x && p.y === selectedPlacement.y
        ? { ...p, rot, wallOrientation: orientation } : p)) }));
    setSelectedPlacedDecoration({ id: selectedPlacement.id, x: selectedPlacement.x, y: selectedPlacement.y, rot });
  };

  /**
   * Flip one of a placement's rule switches. Off is stored as absent rather than
   * `false`, which keeps a saved map's JSON to what an author actually turned on and
   * matches how `rot` and `autoTactics` are already written.
   */
  const toggleDecorationRule = useCallback(
    (flag: "blocksPath" | "yieldsHighGround") => {
      const selected = selectedPlacedDecoration;
      if (!selected) {
        setNote("Clique em qualquer hex de uma decoração no mapa antes de mudar as regras dela.");
        return;
      }
      setDraft((d) => {
        const hit = findPreviewDecoration(d.decorations, selected);
        if (!hit) {
          setNote("Essa decoração já não está no mapa.");
          return d;
        }
        const turningOn = !hit[flag];
        const next: DecorationPlacement = { ...hit, [flag]: turningOn ? true : undefined };
        const name = DECORATIONS[hit.id]?.name ?? hit.id;
        const label = flag === "blocksPath" ? "Bloquear caminho" : "Alto terreno";
        setNote(`${name}: ${label} ${turningOn ? "ligado" : "desligado"}.`);
        return { ...d, decorations: d.decorations.map((p) => (p === hit ? next : p)) };
      });
    },
    [selectedPlacedDecoration, setNote],
  );

  /** Floor-connector placements only (DecorationDef.exitKind === "connector"): which mission
   * this specific hex leads to. Mirrors toggleDecorationRule's own find/replace pattern. */
  const setConnectorTarget = useCallback(
    (targetMapId: string) => {
      const selected = selectedPlacedDecoration;
      if (!selected) return;
      setDraft((d) => {
        const hit = findPreviewDecoration(d.decorations, selected);
        if (!hit) return d;
        const next: DecorationPlacement = { ...hit, targetMapId: targetMapId || undefined };
        return { ...d, decorations: d.decorations.map((p) => (p === hit ? next : p)) };
      });
    },
    [selectedPlacedDecoration],
  );

  /** Floor-connector placements only: flips the result-screen wording/direction between
   * "Avançar" (deeper) and "Voltar" (back up) — see DecorationPlacement.returnConnector. */
  const toggleReturnConnector = useCallback(() => {
    const selected = selectedPlacedDecoration;
    if (!selected) return;
    setDraft((d) => {
      const hit = findPreviewDecoration(d.decorations, selected);
      if (!hit) return d;
      const next: DecorationPlacement = { ...hit, returnConnector: hit.returnConnector ? undefined : true };
      return { ...d, decorations: d.decorations.map((p) => (p === hit ? next : p)) };
    });
  }, [selectedPlacedDecoration]);

  const removeSelectedDecoration = useCallback(() => {
    const selected = selectedPlacedDecoration;
    if (!selected) {
      setNote("Clique em qualquer hex da decoração e então pressione Delete.");
      return;
    }
    setDraft((d) => {
      const hit = findPreviewDecoration(d.decorations, selected);
      if (!hit) {
        setNote("Essa decoração já não está no mapa.");
        return d;
      }
      const hitDef = DECORATIONS[hit.id];
      const tiles = [...d.tiles];
      const tileVariants = [...d.tileVariants];
      const tileRots = [...(d.tileRots ?? [])];
      const base = baseForDraft(d);
      if (hitDef?.tile) {
        for (const f of placedFootprint(hit)) {
          const i = cellIndex(hit.x + f.dx, hit.y + f.dy, d.cols, d.rows);
          if (i >= 0 && tiles[i] === hitDef.tile) { tiles[i] = base.tile; tileVariants[i] = base.variant; tileRots[i] = 0; }
        }
      }
      setNote(`${hitDef?.name ?? hit.id} removida.`);
      return { ...d, tiles, tileVariants, tileRots, decorations: d.decorations.filter((p) => p !== hit) };
    });
    setSelectedPlacedDecoration(null);
  }, [selectedPlacedDecoration, setNote]);

  useEffect(() => {
    const onEditorDelete = (event: KeyboardEvent) => {
      if (event.key !== "Delete") return;
      // The map preview's own Delete listener (MapPreviewCanvas) handles deleting a held unit
      // and calls preventDefault() when it does — this listener must then stay out of it, or
      // its own "nothing selected" note overwrites the unit-deleted note right after.
      if (event.defaultPrevented) return;
      const target = event.target as HTMLElement | null;
      if (target?.closest("input, textarea, select, [contenteditable=true]")) return;
      event.preventDefault();
      removeSelectedDecoration();
    };
    window.addEventListener("keydown", onEditorDelete);
    return () => window.removeEventListener("keydown", onEditorDelete);
  }, [removeSelectedDecoration]);
  const toggleDecoration = (x: number, y: number) => {
    const clicked = draft.decorations.find((p) => placedFootprint(p).some((f) => p.x + f.dx === x && p.y + f.dy === y));
    if (clicked && DECORATIONS[clicked.id]?.model3d && DECORATIONS[decoBrush]?.model3d && clicked.id !== decoBrush) {
      const replacement: DecorationPlacement = {
        ...clicked, id: decoBrush, rot: wallOrientation === "vertical" ? 1 : 0, wallOrientation,
        blocksPath: undefined, yieldsHighGround: undefined,
      };
      setDraft(d => ({ ...d, decorations: removeWallsUnderWatchtowerEntrances(d.decorations.map(p =>
        p.id === clicked.id && p.x === clicked.x && p.y === clicked.y ? replacement : p)) }));
      setSelectedPlacedDecoration(null);
      setNote(`${DECORATIONS[decoBrush]?.name} colocada em ${clicked.x},${clicked.y}.`);
      return;
    }
    // A Waypoint (Escape/Dungeon Exit, floor connector) is a flat ground marking, not a
    // physical object — it can share a hex with anything already there, including another
    // Waypoint, instead of being blocked by it or redirecting the click to it. Placing two
    // exits near each other is common (e.g. both ends of a small room), and since exits are
    // 2 hexes wide, clicking near one used to land on its own second hex and silently select
    // it instead of placing the new one. With a Waypoint brush active, a click still selects
    // (for delete/edit) when it lands exactly on an existing placement's OWN anchor hex —
    // only a click that only reaches an existing Waypoint through its second/offset hex falls
    // through to placing a new one instead, which is the actual "clicking near it" case above.
    const brushIsWaypoint = !!DECORATIONS[decoBrush]?.exitKind;
    const entranceOverWaypoint = decoBrush === WATCHTOWER_ENTRANCE_ID && !!DECORATIONS[clicked?.id ?? ""]?.exitKind;
    if (clicked && !entranceOverWaypoint && (!brushIsWaypoint || (!!DECORATIONS[clicked.id]?.exitKind && clicked.x === x && clicked.y === y))) {
      const clickedDef = DECORATIONS[clicked.id];
      setSelectedPlacedDecoration({ id: clicked.id, x: clicked.x, y: clicked.y, rot: clicked.rot });
      setNote(`${clickedDef?.name ?? clicked.id} selecionada. Pressione Delete para remover.`);
      return;
    }
    setDraft((d) => {
      const def = DECORATIONS[decoBrush];
      if (!def) return d;
      const blocksByDefault = BARRICADE_LIKE_DECOR.has(decoBrush) || HOUSE_DECOR_IDS.has(decoBrush) || BIG_HOUSE_DECOR_IDS.has(decoBrush) || SOLID_HOUSE_DECOR_IDS.has(decoBrush) || SOLID_CART_DECOR_IDS.has(decoBrush) || SOLID_ROCK_DECOR_IDS.has(decoBrush);
      const placed: DecorationPlacement = def.model3d
        ? { id: decoBrush, x, y, rot: wallOrientation === "vertical" ? 1 : 0, wallOrientation }
        : blocksByDefault ? { id: decoBrush, x, y, blocksPath: true } : { id: decoBrush, x, y };
      const covered = previewDecorationCells(d.decorations, placed);
      // A new prop always stays where it was clicked. Parapets do not choose a new
      // position by themselves; only their ordinary horizontal footprint is occupied.
      for (const f of placedFootprint(placed)) {
        if (!brushIsWaypoint && covered.has(`${x + f.dx},${y + f.dy}`)) return d;
      }
      const tiles = [...d.tiles];
      if (def.tile) {
        for (const f of def.footprint) {
          const i = cellIndex(x + f.dx, y + f.dy, d.cols, d.rows);
          if (i >= 0) tiles[i] = def.tile;
        }
      }
      // Barricade-family City props block like a real barricade without repainting the
      // hex to barricade's dirt/rubble ground art — defaulted on here instead of the
      // author having to remember to check "Bloquear caminho" every time. Houses too.
      // No auto-selection of any sort, per direct instruction: placing stays on the current
      // brush so the author can keep placing more of the same thing; they select something
      // else (to inspect/delete/edit rules) only by clicking it themselves.
      return { ...d, tiles, decorations: removeWallsUnderWatchtowerEntrances([...d.decorations, placed]) };
    });
  };
  const toggleElementalFx = (x: number, y: number) => {
    const list = draft.elementalFx ?? [];
    // A tile can hold several independent FX. Toggle only the selected family and element so
    // placing the 2D elemental layer does not remove a pixel flipbook on the same hex (or vice versa).
    const hit = list.find((p) =>
      p.x === x && p.y === y && (
        fxFamily === "procedural_pixel"
          ? p.family === "procedural_pixel" && p.element === pixelFxBrush
          : p.family !== "procedural_pixel" && p.kind === fxBrush
      )
    );
    let next: MapDraft;
    if (hit) {
      setNote(`${hit.family === "procedural_pixel" ? hit.element : ELEMENT_LABELS[hit.kind]} FX removido de ${x},${y}.`);
      next = { ...draft, elementalFx: list.filter((p) => p !== hit) };
    } else {
      const pixelKinds: Record<PixelElement, PlaceableElementKind> = { fire:"fire",frost:"ice",lightning:"lightning",poison:"acid",arcane:"darkness",holy:"holy",shadow:"darkness",ember:"fire" };
      const kind = fxFamily === "procedural_pixel" ? pixelKinds[pixelFxBrush] : fxBrush;
      const placed: ElementalFxPlacement = fxFamily === "procedural_pixel"
        ? { id: `fx-${Date.now().toString(36)}-${Math.floor(Math.random() * 1e4)}`, kind, x, y, family: "procedural_pixel", element: pixelFxBrush, preset: pixelFxPresetId, parameters: { ...pixelFxSettings } }
        : { id: `fx-${Date.now().toString(36)}-${Math.floor(Math.random() * 1e4)}`, kind, x, y, family: "regular" };
      setNote(`${fxFamily === "procedural_pixel" ? pixelFxBrush : ELEMENT_LABELS[fxBrush]} FX colocado em ${x},${y}.`);
      next = { ...draft, elementalFx: [...list, placed] };
    }
    setDraft(next);
    // Unlike terrain painting, a placed emitter must be visible at once so the author can
    // tell that the chosen FX was put on this exact hex. Keep the normal draft debounce for
    // other edits, but don't hide elemental placement behind it.
    if (showPreview) {
      immediateFxPreviewDraftRef.current = next;
      setPreviewMission(draftToMission(next));
    }
  };

  const onCellClick = (x: number, y: number, point?: { x: number; y: number }) => {
    const i = y * draft.cols + x;
    if (mode === "water") {
      setDraft(d => {
        const waterLevels = Array.from({ length: d.cols * d.rows }, (_, index) => d.waterLevels?.[index] ?? null);
        let waterPatches = (d.waterPatches ?? []).map(p => ({ ...p }));
        const waterFootprints = Array.from({ length: d.cols * d.rows }, (_, index) => d.waterFootprints?.[index] ?? null);
        let frontier = [{ x, y }];
        const visited = new Set<number>();
        for (let ring = 0; ring <= waterRadius; ring++) {
          const next: { x: number; y: number }[] = [];
          for (const cell of frontier) {
            if (cell.x < 0 || cell.y < 0 || cell.x >= d.cols || cell.y >= d.rows) continue;
            const index = cell.y * d.cols + cell.x;
            if (visited.has(index)) continue;
            visited.add(index);
            if (d.tiles[index] !== "void" || waterErase) {
              if (point) {
                const px = point.x + Math.sqrt(3) * (cell.x - x + 0.5 * ((cell.y & 1) - (y & 1)));
                const py = point.y + 1.5 * (cell.y - y);
                if (waterErase) {
                  waterPatches = waterPatches.filter(p => {
                    const distance = waterShape === "square" ? Math.max(Math.abs(p.x-px), Math.abs(p.y-py)) : Math.hypot(p.x-px,p.y-py);
                    return distance > (waterSize + p.size) * 1.25;
                  });
                  waterLevels[index] = null; waterFootprints[index] = null;
                } else {
                  waterPatches = waterPatches.filter(p => !(Math.hypot(p.x-px,p.y-py) < 0.001 && p.size === waterSize && p.shape === waterShape && p.level === waterLevel));
                  waterPatches.push({ x: px, y: py, level: waterLevel, size: waterSize, shape: waterShape });
                }
              } else {
                const px = Math.sqrt(3) * (cell.x + 0.5 * (cell.y & 1) + 0.5);
                const py = 2.4 + 1.5 * cell.y + 1;
                if (waterErase) {
                  waterPatches = waterPatches.filter(p => Math.hypot(p.x-px,p.y-py) > (waterSize+p.size)*1.25);
                  waterLevels[index] = null; waterFootprints[index] = null;
                } else {
                  waterPatches = waterPatches.filter(p => !(Math.hypot(p.x-px,p.y-py) < 0.001 && p.size === waterSize && p.shape === waterShape && p.level === waterLevel));
                  waterPatches.push({ x: px, y: py, level: waterLevel, size: waterSize, shape: waterShape });
                }
              }
            }
            next.push(...hexNeighbors(cell.x, cell.y));
          }
          frontier = next;
        }
        return { ...d, waterLevels, waterFootprints, waterPatches };
      });
    } else if (mode === "elevation") {
      setDraft(d => {
        const terrainElevations = Array.from({ length: d.cols * d.rows }, (_, index) =>
          d.terrainElevations?.[index] ?? TERRAIN[d.tiles[index] ?? "plains"].height ?? 0);
        let frontier = [{ x, y }];
        const visited = new Set<number>();
        for (let ring = 0; ring <= elevationRadius; ring++) {
          const next: { x: number; y: number }[] = [];
          for (const cell of frontier) {
            if (cell.x < 0 || cell.y < 0 || cell.x >= d.cols || cell.y >= d.rows) continue;
            const index = cell.y * d.cols + cell.x;
            if (visited.has(index)) continue;
            visited.add(index);
            if (d.tiles[index] !== "void") terrainElevations[index] = elevationTool === "level" ? elevationLevel
              : Math.max(0, Math.min(12, terrainElevations[index]! + (elevationTool === "raise" ? 1 : -1)));
            next.push(...hexNeighbors(cell.x, cell.y));
          }
          frontier = next;
        }
        return { ...d, terrainElevations };
      });
    } else if (mode === "paint") {
      if (turning) {
        turnTile(i);
        const now = (((draft.tileRots?.[i] ?? 0) + 1) % 6) * 60;
        setNote(`${TERRAIN[draft.tiles[i]!].name} em ${x},${y}: girado para ${now}°${now === 0 ? " (de volta ao original)" : ""}.`);
      } else setTile(i, brush);
    }
    else if (mode === "decoration" || mode === "architecture") {
      if (turningDeco) {
        turnDecoration(x, y);
        return;
      }
      const def = DECORATIONS[decoBrush];
      const existing = draft.decorations.some((p) => placedFootprint(p).some((f) => p.x + f.dx === x && p.y + f.dy === y));
      if (!existing && def?.tile) {
        setNote(`${def.name} sobre ${TERRAIN[def.tile].name.toLowerCase()} — ${TERRAIN[def.tile].passable ? "dá pra subir em cima" : "não se atravessa"}.`);
      }
      toggleDecoration(x, y);
    }
    else if (mode === "elementalFx") toggleElementalFx(x, y);
    else toggleSpawn(x, y);
  };

  /** The technical grid is reserved for the fast rotation gesture; normal painting lives in the preview. */
  const onTechnicalClick = (x: number, y: number) => {
    if (mode === "paint") {
      const i = y * draft.cols + x;
      turnTile(i);
      const now = (((draft.tileRots?.[i] ?? 0) + 1) % 6) * 60;
      setNote(`${TERRAIN[draft.tiles[i]!].name} em ${x},${y}: girado para ${now}°${now === 0 ? " (de volta ao original)" : ""}.`);
    } else if (mode === "decoration" || mode === "architecture") {
      turnDecoration(x, y);
    } else onCellClick(x, y);
  };

  const resize = (cols: number, rows: number) => {
    cols = Math.max(MIN_GRID, Math.min(MAX_GRID, cols));
    rows = Math.max(MIN_GRID, Math.min(MAX_GRID, rows));
    setDraft((d) => {
      const base = baseForDraft(d);
      const tiles: TerrainId[] = [];
      const tileVariants: number[] = [];
      const tileRots: number[] = [];
      const terrainElevations: number[] = [];
      const waterLevels: (number | null)[] = [];
      const waterFootprints: NonNullable<MapDraft["waterFootprints"]> = [];
      for (let r = 0; r < rows; r++) {
        for (let c = 0; c < cols; c++) {
          const inOld = r < d.rows && c < d.cols;
          waterFootprints.push(inOld ? (d.waterFootprints?.[r * d.cols + c] ?? null) : null);
          waterLevels.push(inOld ? (d.waterLevels?.[r * d.cols + c] ?? null) : null);
          tiles.push(inOld ? (d.tiles[r * d.cols + c] ?? base.tile) : base.tile);
          tileVariants.push(inOld ? (d.tileVariants[r * d.cols + c] ?? base.variant) : base.variant);
          tileRots.push(inOld ? (d.tileRots?.[r * d.cols + c] ?? 0) : 0);
          terrainElevations.push(inOld ? (d.terrainElevations?.[r * d.cols + c] ?? TERRAIN[d.tiles[r * d.cols + c] ?? base.tile].height ?? 0) : 0);
        }
      }
      const inBounds = (s: Spawn) => s.x < cols && s.y < rows;
      const decorations = d.decorations.filter((p) => {
        const def = DECORATIONS[p.id];
        if (!def) return false;
        return def.footprint.every((f) => p.x + f.dx >= 0 && p.y + f.dy >= 0 && p.x + f.dx < cols && p.y + f.dy < rows);
      });
      return {
        ...d,
        cols,
        rows,
        tiles,
        tileVariants,
        tileRots,
        terrainElevations: d.terrainElevations ? terrainElevations : undefined,
        waterLevels: d.waterLevels ? waterLevels : undefined,
        waterPatches: d.waterPatches?.filter(p => p.x >= 0 && p.x < (cols + 0.5) * Math.sqrt(3) && p.y >= 2.4 && p.y < 2.4 + rows * 1.5 + 0.5),
        waterFootprints: d.waterFootprints ? waterFootprints : undefined,
        decorations,
        elementalFx: (d.elementalFx ?? []).filter((p) => p.x >= 0 && p.y >= 0 && p.x < cols && p.y < rows),
        playerSpawns: d.playerSpawns.filter(inBounds),
        enemySpawns: d.enemySpawns.filter(inBounds),
        neutralSpawns: (d.neutralSpawns ?? []).filter(inBounds),
      };
    });
  };

  const updateSpawn = (side: SpawnKey, i: number, patch: Partial<DraftSpawn>) => {
    setDraft((d) => {
      const list = (d[side] ?? []).slice();
      list[i] = { ...list[i]!, ...patch };
      return { ...d, [side]: list };
    });
  };

  const removeSpawn = (side: SpawnKey, i: number) => {
    setDraft((d) => ({ ...d, [side]: (d[side] ?? []).filter((_, idx) => idx !== i) }));
  };

  const selectPreviewUnit = (unit: PreviewUnitSelection) => {
    setSelectedPlacedDecoration(null);
    setNote(`${unit.name}: pressione Delete para remover, ou arraste para outro hex pra mover.`);
  };

  // Stable identity: MapPreviewCanvas's own "Delete" keydown listener re-registers whenever
  // this prop's reference changes (see its onHeldUnitDelete effect deps), and it needs to keep
  // firing before GameApp's onEditorDelete listener below (which bails out once this one has
  // already handled the key via event.preventDefault) — a fresh function every render made that
  // ordering unreliable and let onEditorDelete's own note clobber this one's right after a
  // successful delete.
  const deleteHeldPreviewUnit = useCallback((selected: PreviewUnitSelection) => {
    setDraft((d) => {
      const unit = (d[selected.side] ?? [])[selected.index];
      if (!unit) return d;
      setNote(`${unit.name} removido do mapa.`);
      return { ...d, [selected.side]: (d[selected.side] ?? []).filter((_, index) => index !== selected.index) };
    });
  }, [setDraft, setNote]);

  const placePreviewUnit = (selected: PreviewUnitSelection, x: number, y: number) => {
    const occupied = SPAWN_KEYS.some((side) => (draft[side] ?? []).some((spawn, index) =>
      !(side === selected.side && index === selected.index) && spawn.x === x && spawn.y === y,
    ));
    if (occupied) {
      setNote("Esse hex já tem uma unidade. Escolha um hex vazio.");
      return;
    }
    const current = (draft[selected.side] ?? [])[selected.index];
    if (!current) {
      setNote("Essa unidade não existe mais. Arraste outra na prévia.");
      return;
    }
    updateSpawn(selected.side, selected.index, { x, y });
    setNote(`${current.name} movido para ${x},${y}.`);
  };

  const selectPreviewDecoration = (decoration: PreviewDecorationSelection) => {
    setSelectedPlacedDecoration(decoration);
    const def = DECORATIONS[decoration.id];
    setNote(`${def?.name ?? decoration.id} selecionada. Arraste até um hex vazio da prévia para movê-la.`);
  };

  /** Right-click-drag drop for an existing decoration, mirroring placePreviewUnit: refuses the
   * same way a fresh placement or a turn would (see toggleDecoration/turnDecoration) — off the
   * board or overlapping another prop — and re-stamps the terrain it carries under it the same
   * way a turn does, since a moved house has to leave its climbable ground behind, not drag it. */
  const placePreviewDecoration = (selected: PreviewDecorationSelection, x: number, y: number) => {
    setDraft((d) => {
      const hit = findPreviewDecoration(d.decorations, selected);
      if (!hit) {
        setNote("Essa decoração já não está no mapa.");
        return d;
      }
      const def = DECORATIONS[hit.id];
      if (!def) return d;
      if (hit.x === x && hit.y === y) return d;
      const moved = { ...hit, x, y };
      const before = placedFootprint(hit);
      const after = placedFootprint(moved);
      if (!after.every((f) => x + f.dx >= 0 && y + f.dy >= 0 && x + f.dx < d.cols && y + f.dy < d.rows)) {
        setNote(`${def.name} não cabe aí — sairia do mapa.`);
        return d;
      }
      const others = previewDecorationCells(d.decorations.filter((p) => p !== hit), moved);
      for (const f of after) {
        if (others.has(`${x + f.dx},${y + f.dy}`)) {
          setNote(`${def.name} não cabe aí — bateria em outra decoração.`);
          return d;
        }
      }
      const tiles = [...d.tiles];
      const tileVariants = [...d.tileVariants];
      const tileRots = [...(d.tileRots ?? [])];
      if (def.tile) {
        const base = baseForDraft(d);
        for (const f of before) {
          const i = cellIndex(hit.x + f.dx, hit.y + f.dy, d.cols, d.rows);
          if (i >= 0 && tiles[i] === def.tile) { tiles[i] = base.tile; tileVariants[i] = base.variant; tileRots[i] = 0; }
        }
        for (const f of after) {
          const i = cellIndex(x + f.dx, y + f.dy, d.cols, d.rows);
          if (i >= 0) tiles[i] = def.tile;
        }
      }
      setSelectedPlacedDecoration(moved);
      setNote(`${def.name} movida para ${x},${y}.`);
      return { ...d, tiles, tileVariants, tileRots, decorations: removeWallsUnderWatchtowerEntrances(d.decorations.map((p) => (p === hit ? moved : p))) };
    });
  };
  /** Drops one hero or the whole party on the bottom row. Worked out from the current draft
   * rather than inside the state updater: React runs that when it pleases, so counting
   * there reported on placements that had not happened yet. */
  const addHeroes = (who: { name: string; classId: ClassId }[]) => {
    const occupied = new Set(SPAWN_KEYS.flatMap((k) => draft[k] ?? []).map((s) => `${s.x},${s.y}`));
    const already = new Set(draft.playerSpawns.map((s) => s.name));
    const added: DraftSpawn[] = [];
    const skipped: string[] = [];
    let x = 0;
    const y = draft.rows - 1;
    for (const h of who) {
      if (already.has(h.name)) {
        skipped.push(h.name);
        continue;
      }
      while (x < draft.cols && occupied.has(`${x},${y}`)) x++;
      if (x >= draft.cols) break;
      added.push({ name: h.name, classId: h.classId, x, y, level: DEFAULT_TEST_LEVEL });
      occupied.add(`${x},${y}`);
      x++;
    }
    if (added.length > 0) setDraft((d) => ({ ...d, playerSpawns: [...d.playerSpawns, ...added] }));
    const put = added.map((a) => a.name).join(", ");
    if (added.length > 0) {
      setNote(skipped.length > 0 ? `${put} na linha de baixo (${skipped.join(", ")} já estava lá).` : `${put} na linha de baixo.`);
    } else {
      setNote(`${who.map((h) => h.name).join(", ")} já ${who.length > 1 ? "estavam" : "estava"} no mapa.`);
    }
  };

  /** A map is saved only when /__map-save confirms its project file was written and read
   * back. Browser storage is not a substitute for the game's map file. */
  const doSave = async (draftToSave: MapDraft = draft) => {
    if (savingMapRef.current) return false;
    savingMapRef.current = true;
    setSavingMap(true);
    try {
      const canonicalId = normalizeScenarioId(draftToSave.id);
      const canonicalTitle = canonicalId === "thebridge" ? "A Ponte de Pedra" : draftToSave.title;
      const savedDraft = canonicalId === draftToSave.id && canonicalTitle === draftToSave.title
        ? draftToSave
        : { ...draftToSave, id: canonicalId, title: canonicalTitle };
      if (savedDraft !== draftToSave) setDraft(savedDraft);
      armEditorResume(savedDraft);
      const repo = await saveMapToRepo(savedDraft);
      if (!repo.ok) {
        setNote(`NÃO SALVO: ${repo.error}. O rascunho continua aberto; nenhum arquivo do jogo foi confirmado.`);
        return false;
      }

      // Publish the confirmed file into the running campaign immediately. On the next
      // launch, mapstore reads that same highest-serial project file from disk.
      registerSessionMapOverride(savedDraft);
      window.dispatchEvent(new CustomEvent("ember:missions-saved"));
      const nextActive = { ...activeVersions, [savedDraft.id]: repo.serial };
      setActiveVersions(nextActive);
      try {
        await refreshRepoFiles(savedDraft.id);
        await refreshSavedLocationMaps();
      } catch {
        // The project file was already confirmed by the save route; list refresh is separate.
      }
      setNote(`Salvo: ${repo.file} (v${serialLabel(repo.serial)}).`);
      return true;
    } finally {
      savingMapRef.current = false;
      setSavingMap(false);
    }
  };

  const doExport = () => {
    setExportText(JSON.stringify(draftToMission(draft), null, 2));
    setCopyOk(false);
    setNote("Exportado para copiar — isso não salva o mapa no jogo.");
  };

  /** Copies one browser-local version into src/game/maps/ without overwriting it.
   * The dev route assigns the next ID### serial on disk. */
  const doSendVersionToRepo = async (version: MapVersion) => {
    armEditorResume(version.draft);
    const repo = await saveMapToRepo(version.draft);
    if (repo.ok) {
      await refreshRepoFiles(version.draft.id);
      await refreshSavedLocationMaps();
    }
    setNote(
      repo.ok
        ? `v${serialLabel(version.serial)} enviada ao repositório como ${repo.file}.`
        : `NÃO ENVIOU v${serialLabel(version.serial)} ao repositório: ${repo.error}`,
    );
  };
  const doActivate = (serial: number) => {
    const selected = (versionStore[draft.id] ?? []).find((version) => version.serial === serial);
    if (!selected) {
      setNote(`NÃO ATIVOU v${serialLabel(serial)}: a cópia local dessa versão não foi encontrada.`);
      return;
    }
    const next = { ...activeVersions, [draft.id]: serial };
    if (!saveActiveDrafts({ ...loadActiveDrafts(), [draft.id]: selected.draft })) {
      setNote(`NÃO ATIVOU v${serialLabel(serial)}: o navegador recusou salvar a cópia da campanha.`);
      return;
    }
    setActiveVersions(next);
    saveActiveVersions(next);
    registerSessionMapOverride(selected.draft);
    window.dispatchEvent(new CustomEvent("ember:missions-saved"));
    setNote(`v${serialLabel(serial)} agora é a cópia exata valendo pra "${draft.id}" na campanha.`);
  };

  const doDeactivate = () => {
    const next = { ...activeVersions };
    delete next[draft.id];
    const activeDrafts = { ...loadActiveDrafts() };
    delete activeDrafts[draft.id];
    setActiveVersions(next);
    saveActiveVersions(next);
    saveActiveDrafts(activeDrafts);
    clearSessionMapOverride(draft.id);
    window.dispatchEvent(new CustomEvent("ember:missions-saved"));
    setNote(`"${draft.id}" voltou a usar o cenário original.`);
  };

  /** Select an existing repository file directly for campaign play. It does not write a
   * replacement file: activation is a local campaign pointer to this exact draft. */
  const doActivateFile = (f: { serial: number; draft: MapDraft; file?: string }) => {
    const next = { ...activeVersions, [draft.id]: f.serial };
    // The file is on disk and the campaign plays files from disk (mapstore missionById ignores
    // browser copies for any map with a file), so no browser copy is stored: maps of any size
    // activate, instead of failing when a big map overflows browser storage.
    setActiveVersions(next);
    saveActiveVersions(next);
    setDraft(f.draft);
    registerSessionMapOverride(f.draft);
    window.dispatchEvent(new CustomEvent("ember:missions-saved"));
    setNote(`${f.file ?? mapFileName(draft.id, f.serial)} agora é a cópia exata ativa na campanha.`);
  };

  /** Deletes a saved file. Two clicks: the first arms the button, so a misclick on a
   * row does not throw away a version that has no undo. */
  const doDeleteFile = async (name: string) => {
    if (armedDelete !== name) {
      setArmedDelete(name);
      setNote(`Clique de novo no X pra apagar ${name} — isso não tem volta.`);
      return;
    }
    setArmedDelete("");
    const dex = await deleteMapFile(name);
    if (dex.ok) await refreshRepoFiles(draft.id);
    setNote(dex.ok ? `${name} apagado.` : `NÃO APAGOU ${name}: ${dex.error}`);
  };

  /** Browser-local versions need the same two-click confirmation as repository files. */
  const doDeleteVersion = (serial: number) => {
    const key = `local:${draft.id}:${serial}`;
    if (armedDelete !== key) {
      setArmedDelete(key);
      setNote(`Erase? Clique de novo para apagar a versão local v${serialLabel(serial)} — isso não tem volta.`);
      return;
    }
    setArmedDelete("");
    const list = (versionStore[draft.id] ?? []).filter((v) => v.serial !== serial);
    const next = { ...versionStore, [draft.id]: list };
    if (list.length === 0) delete next[draft.id];
    setVersionStore(next);
    saveVersionStore(next);
    if (activeVersions[draft.id] === serial) doDeactivate();
    setNote(`v${serialLabel(serial)} excluída.`);
  };

  // Every list the editor offers is sorted by what it shows, not by the order things were
  // declared in — a class table grouped by role is fine to read in code and useless to
  // search in a dropdown. pt-BR collation so accents and case sort where a reader expects.
  const classOptions = (Object.keys(CLASSES) as ClassId[]).sort((a, b) => byName(CLASSES[a].name, CLASSES[b].name));
  const summonOptions = [...SUMMON_CLASSES].sort((a, b) => byName(CLASSES[a].name, CLASSES[b].name));
  const enemyClassOptions = classOptions.filter((c) => !isSummonClass(c) && !CLASSES[c].role.startsWith("Civil"));
  // A named-individual classId (aldric, kaelFinal, conjurer, sandoval, ...) deliberately
  // keeps the same display name/role as the generic job it's a re-skin of (Aldric's own
  // class is still named "Lanceiro", same as the plain Lancer enemy; Sandoval's is
  // "Lanceiro · Lanceiro rival · Chefe") — so any picker that just prints CLASSES[c].name is
  // unfindable/ambiguous for that classId specifically. This map lets such a picker show
  // that individual's own name for their own classId only, leaving every generic classId's
  // label untouched. Covers every recruitable hero (EDITOR_HEROES) plus named non-recruit
  // individuals who have their own classId/sprite but aren't a playable party option.
  const NAMED_NON_HERO_CLASS_IDS: { name: string; classId: ClassId }[] = [{ name: "Sandoval", classId: "sandoval" }];
  const heroNameByClassId: Partial<Record<ClassId, string>> = Object.fromEntries(
    [...EDITOR_HEROES, ...NAMED_NON_HERO_CLASS_IDS].map((h) => [h.classId, h.name]),
  );
  // One entry per distinct sprite (several classes share art — a promoted class, an
  // alternate skin), labeled by whichever class name reaches it first. Named heroes go
  // first so each of them claims their own sprite's slot under their own name — the
  // dialog editor needs to be able to name Kael/Neera/Voss/Salazar/Aldric/Malrec as
  // speakers regardless of whether their final art has landed yet.
  const portraitOptions = (() => {
    const seen = new Set<SpriteId>();
    // Named heroes claim their sprite's slot under their own name (Kael, not "Guerreiro")
    // even while they still share on-disk art with a generic class — this used to read
    // CLASSES[c].name for every entry, which stamped every MC's option with their class's
    // name instead, so none of them were findable by their actual name in the picker.
    // Kept as their own group ahead of every other class (sorted only among themselves),
    // not folded into the alphabetical class list, so they're the first thing the picker
    // offers — every other unit is still in the list right after, nothing removed.
    const heroes: { id: SpriteId; label: string }[] = [];
    for (const h of EDITOR_HEROES) {
      const sprite = CLASSES[h.classId].sprite;
      if (seen.has(sprite)) continue;
      seen.add(sprite);
      heroes.push({ id: sprite, label: h.name });
    }
    const rest: { id: SpriteId; label: string }[] = [];
    for (const c of classOptions) {
      const sprite = CLASSES[c].sprite;
      if (seen.has(sprite)) continue;
      seen.add(sprite);
      rest.push({ id: sprite, label: CLASSES[c].name });
    }
    return [...heroes.sort((a, b) => byName(a.label, b.label)), ...rest.sort((a, b) => byName(a.label, b.label))];
  })();
  const decorOptions = Object.values(DECORATIONS).filter(dec => !dec.model3d).sort((a, b) => byName(a.name, b.name));
  const [architectureDecorations, setArchitectureDecorations] = useState(false);
  const [thickWalls, setThickWalls] = useState(false);
  const architectureOptions = Object.values(DECORATIONS).filter(dec => !!dec.model3d && !!(dec.rockStyle || dec.treeModel || dec.propModel) === architectureDecorations
    && (dec.id !== WATCHTOWER_ENTRANCE_ID || draft.id.startsWith("watchtower-"))
    && (architectureDecorations || !!dec.thickWall === thickWalls))
    .sort((a, b) => Number(a.model3d === "wall") - Number(b.model3d === "wall"));
  const decorationSectionFor = (id: string) => {
    if (DECORATIONS[id]?.exitKind) return "Waypoints";
    if (id === "merchant-covered-cart-001" || id === "city-market-stall" || id === "city-market-stall-2" || id === "city-market-wagon-new") return "Shops";
    // Everything that emits light (see LIGHT_DEFS), burning houses included, in one place.
    if (LIGHT_DEFS[id]) return "Lights";
    if (HOUSE_DECOR_IDS.has(id) || BIG_HOUSE_DECOR_IDS.has(id)) return "Houses";
    if (DEADWOODS_DECOR_IDS.has(id)) return "Madeira Morta";
    if (FOREST_DECOR_IDS.has(id)) return "Forest";
    if (
      id === "barricade" ||
      id === "wooden-barricade-1" ||
      id === "city-spike-barricade-low" ||
      id === "city-palisade-frame" ||
      id === "city-wattle-fence" ||
      id === "city-wooden-barricade" ||
      id === "city-palisade-banner" ||
      id === "city-spike-barricade-large" ||
      id === "city-spike-barricade" ||
      id === "city-banner-barricade" ||
      id === "city-stone-banner-wall"
    )
      return "Barricada";
    if (id.startsWith("wilds-")) return "Wilds";
    if (id.startsWith("cave-")) return "Cave";
    if (id.startsWith("torture-")) return "Torture";
    if (id.startsWith("city-")) return "City";
    if (id.includes("bridge") || id.includes("ember-channels")) return "Pontes";
    if (id.includes("mountain") || id.includes("ridge") || id.includes("rock") || id.includes("boulder") || id.includes("spike") || id.includes("cliff")) return "Pedras e relevo";
    if (id.includes("tree") || id.includes("forest") || id.includes("wood") || id.includes("log") || id.includes("mossy")) return "Natureza";
    if (id.includes("ruined") || id.includes("tower") || id.includes("mansion") || id.includes("wall") || id.includes("gate") || id.includes("shrine") || id.includes("house") || id.includes("hut") || id.includes("hamlet")) return "Ruínas e construções";
    return "Objetos";
  };
  // "Todas" stays pinned first (it's the "show everything" reset, not a real category);
  // every actual category below it is kept in alphabetical order.
  const decorationSections = ["Todas", "Barricada", "Cave", "City", "Forest", "Houses", "Lights", "Madeira Morta", "Natureza", "Objetos", "Pedras e relevo", "Pontes", "Ruínas e construções", "Shops", "Torture", "Waypoints", "Wilds"];
  const visibleDecorOptions = mode === "architecture" ? architectureOptions : decoSection === "Todas" ? decorOptions : decorOptions.filter((dec) => decorationSectionFor(dec.id) === decoSection);

  // Clicking a placed prop is also a lookup action: open its palette section and arm the
  // exact matching brush, so the highlighted menu entry always tells the author its name.
  useEffect(() => {
    const id = selectedPlacedDecoration?.id;
    if (!id || !DECORATIONS[id]) return;
    if (!DECORATIONS[id]?.model3d || mode !== "architecture") setDecoBrush(id);
    if (DECORATIONS[id]?.model3d) setMode("architecture");
    else if (mode === "architecture") setMode("decoration");
    setDecoSection(decorationSectionFor(id));
  }, [selectedPlacedDecoration]);

  /** Whatever unit stands on a cell, across all three spawn lists. */
  const spawnAt = (x: number, y: number) => {
    for (const key of SPAWN_KEYS) {
      const sp = (draft[key] ?? []).find((s) => s.x === x && s.y === y);
      if (sp) return { sp, side: SPAWN_SIDE[key], key };
    }
    return null;
  };

  /** Cell tooltip: the name, the class, and what the letter on the cell means. */
  const spawnHint = (sp: DraftSpawn, side: "player" | "enemy" | "neutral") => {
    const what = isSummonClass(sp.classId) ? "invocação" : side === "neutral" ? "fera neutra" : side === "enemy" ? "inimigo" : "herói";
    const where = side === "player" ? "aliada" : side === "enemy" ? "inimiga" : "neutra";
    return `${sp.name} · ${CLASSES[sp.classId].name} · ${isSummonClass(sp.classId) ? `${what} ${where}` : what}`;
  };

  return (
    <section className="map-editor h-dvh min-h-0 min-w-0 w-full flex flex-col bg-bg">
      <header className="flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-4 border-b border-border">
        <button type="button" onClick={onBack} className="size-10 grid place-items-center rounded-md border border-border" aria-label="Voltar">
          <ChevronLeft className="size-5" />
        </button>
        <div className="flex-1 min-w-0">
          <p className="text-sm uppercase tracking-[0.18em] text-muted">Modo teste</p>
          <h1 className="font-display text-2xl leading-none">Map Editor</h1>
        </div>
      </header>

      <div className="flex-1 min-h-0 min-w-0 overflow-x-hidden overflow-y-auto p-4 flex flex-col gap-4">
        <div className="flex flex-wrap items-center gap-2 text-sm">
          <Button
            variant="ghost"
            size="sm"
            onClick={() => {
              const d = blankDraft();
              setDraft(d);
              setNote(`Mapa novo em branco — cenário "${d.id}", ${d.cols}x${d.rows}.`);
            }}
          >
            Novo
          </Button>
          <Button
            variant="ghost"
            size="sm"
            onClick={() => {
              refreshLocaisState();
              setShowLocations(true);
            }}
          >
            Locais
          </Button>
          <Button variant="ghost" size="sm" onClick={() => setShowRandomEncounters(true)}>
            R-Encounter
          </Button>
          <Button
            variant="ghost"
            size="sm"
            title="Espalha barricadas, barrancos e terreno alto pelo mapa do tamanho atual — depois é só limpar o que não serve"
            onClick={() => {
              // Everything the campaign lays over a map on load — scatter, rocks, chests
              // and decoration — aimed at the map in hand: fill the board with something to
              // react to, then clear what does not belong.
              // Generate replaces, it does not pile on: the decoration pass appends to what
              // it is handed, so without clearing first a second press stacked scenery on
              // top of the last lot.
              const filled = dressMap(draftToMission({ ...draft, autoTactics: true, decorations: [] }), shuffleExclude);
              const tiles = parseLayout(filled.layout);
              // The scatter paints barricades as terrain; they are a decoration now, so the
              // props come back with them — same derivation the engine does on load.
              const scattered = filled.decorations ?? [];
              const decorations = [...scattered, ...barricadeDecor(tiles, draft.cols, draft.rows, scattered)];
              setDraft((d) => ({
                ...d,
                tiles,
                tileVariants: tiles.map((t, i) => Math.min(d.tileVariants[i] ?? 0, (TILE_VARIANT_COUNT[t] ?? 1) - 1)),
                tileRots: tiles.map((_t, i) => d.tileRots?.[i] ?? 0),
                decorations,
              }));
              const counts = new Map<TerrainId, number>();
              for (const t of tiles) if (t !== "plains") counts.set(t, (counts.get(t) ?? 0) + 1);
              const summary = [...counts.entries()].map(([t, n]) => `${n} ${TERRAIN[t].name.toLowerCase()}`).join(", ");
              setNote(
                `Gerado em ${draft.cols}x${draft.rows}: ${summary || "nada"}${decorations.length > 0 ? `, ${decorations.length} decoração(ões)` : ""} — apague o que não servir.`,
              );
            }}
          >
            Gerar terreno
          </Button>
          <select
            className="bg-bg border border-border rounded-md px-2 py-1.5"
            value=""
            onChange={(e) => void loadCampaignMap(e.target.value)}
          >
            <option value="">Carregar mapa da campanha…</option>
            {campaignLoadOptions.map((map) => (
              <option key={map.id} value={map.id}>
                {map.title}
              </option>
            ))}
          </select>          {pickable.length > 0 && (
            <select
              id="mapPick"
              className="flex-1 min-w-0 bg-bg border border-border rounded-md px-2 py-1.5"
              value=""
              title="Abre o save mais recente desse cenário — a lista de arquivos abaixo deixa escolher outro serial"
              onChange={async (e) => {
                const id = e.target.value;
                if (!id) return;
                // latestSavedDraft/latestSerialFor read mapstore.ts's eager import.meta.glob
                // snapshot — taken once when this page/module loaded, and map-save-plugin.mjs
                // deliberately suppresses the HMR that would normally refresh it on a map file
                // write (see its handleHotUpdate: reloading the whole game on every save would
                // throw the author out of the editor). That leaves this glob permanently stale
                // the instant ANY save happens after page load — including a save from a
                // different tab, or an earlier session — so it can silently open an older
                // file than what's actually on disk (reads as "loads the first version I ever
                // saved" instead of the latest). /__map-list?id= hits the disk directly, same
                // as refreshRepoFiles already does for the "files in repository" panel, so
                // it's asked first here too; the stale glob is now only the fallback for a
                // built release with no dev server to ask.
                try {
                  const response = await fetch(`/__map-list?id=${encodeURIComponent(id)}`);
                  const body = (await response.json()) as { ok?: boolean; files?: MapFile[] };
                  if (!response.ok || !body.ok || !Array.isArray(body.files) || body.files.length === 0) throw new Error("lista indisponível");
                  const latestFile = body.files.reduce((best: MapFile, f) => (f.serial > best.serial ? f : best));
                  setDraft(latestFile.draft);
                  setNote(`Aberto ${latestFile.file ?? mapFileName(id, latestFile.serial)} — o save mais novo de "${id}".`);
                  return;
                } catch {
                  // No dev server (built release) — fall back to the static snapshot.
                }
                const fromDisk = latestSavedDraft(id);
                if (fromDisk) {
                  setDraft(fromDisk);
                  setNote(`Aberto ${mapFileName(id, latestSerialFor(id))} — o save mais novo de "${id}".`);
                  return;
                }
                const list = versionStore[id];
                const latest = list?.[list.length - 1];
                if (latest) {
                  setDraft(latest.draft);
                  setNote(`Aberta v${serialLabel(latest.serial)} de "${id}" — só neste navegador, sem arquivo.`);
                }
              }}
            >
              <option value="">Abrir mapa salvo…</option>
              {pickable.map((row) => (
                <option key={row.id} value={row.id}>
                  {row.title} · {row.id} ({row.files > 0 ? `${row.files} arquivo${row.files === 1 ? "" : "s"}` : `${row.local} só no navegador`}
                  {activeVersions[row.id] ? `, v${serialLabel(activeVersions[row.id])} ativa` : ""})
                </option>
              ))}
            </select>
          )}
          {cityHubOptions.length > 0 && (
            <select
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value=""
              title="Mapas de hub (a Estalagem, cidades) — sem combate"
              onFocus={() => void refreshSavedLocationMaps()}
              onChange={(e) => {
                if (e.target.value) void loadCampaignMap(e.target.value);
              }}
            >
              <option value="">CityHubs…</option>
              {cityHubOptions.map((map) => (
                <option key={map.id} value={map.id}>
                  {map.title}
                </option>
              ))}
            </select>
          )}
        </div>


        <div className="grid grid-cols-2 gap-2 text-sm">
          <label className="flex flex-col gap-1" title="O cenário da campanha que essa edição mira. Bate com o id de uma missão real (ex.: o-vau) pra poder ativar essa versão nela, ou qualquer id livre pra um mapa avulso.">
            <span className="text-muted text-xs uppercase tracking-wide">Cenário alvo (Id)</span>
            <input
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value={draft.id}
              onChange={(e) => setDraft((d) => ({ ...d, id: stripScenarioId(e.target.value) }))}
            />
          </label>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Título</span>
            <input
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value={draft.id === "thebridge" ? "A Ponte de Pedra" : draft.title}
              readOnly={draft.id === "thebridge"}
              title={draft.id === "thebridge" ? "Nome canônico deste capítulo" : undefined}
              onChange={(e) => setDraft((d) => ({ ...d, title: e.target.value }))}
            />
          </label>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Local</span>
            <input
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value={draft.place}
              onChange={(e) => setDraft((d) => ({ ...d, place: e.target.value }))}
            />
          </label>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Trilha</span>
            <select
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value={draft.music ?? ""}
              onChange={(e) => {
                const v = e.target.value;
                setDraft((d) => ({ ...d, music: v }));
                setNote(v ? `Trilha desta missão: ${v}.` : "Trilha desta missão: a do tema padrão.");
              }}
            >
              <option value="">Tema padrão (pelo id da missão)</option>
              {[...MUSIC_TRACKS].sort(byName).map((f) => (
                <option key={f} value={f}>
                  {f}
                </option>
              ))}
            </select>
            <span className="text-muted text-[11px]">
              Os arquivos de public/game/MUSIC, pelo nome. Toca durante o briefing, a batalha e as telas de fim.
            </span>
          </label>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Objetivo</span>
            <input
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value={draft.objective}
              onChange={(e) => setDraft((d) => ({ ...d, objective: e.target.value }))}
            />
          </label>
          <label className="flex flex-col gap-1 col-span-2">
            <span className="text-muted text-xs uppercase tracking-wide">Briefing</span>
            <textarea
              className="bg-bg border border-border rounded-md px-2 py-1.5 min-h-16"
              value={draft.briefing}
              onChange={(e) => setDraft((d) => ({ ...d, briefing: e.target.value }))}
            />
          </label>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Recompensa · Gold</span>
            <input type="number" min={0} step={1} className="bg-bg border border-border rounded-md px-2 py-1.5" value={draft.victoryReward?.ember ?? 0}
              onChange={(e) => setDraft((d) => ({ ...d, victoryReward: { ...d.victoryReward, ember: Math.max(0, Math.floor(Number(e.target.value) || 0)), rations: d.victoryReward?.rations ?? 0 } }))} />
          </label>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Recompensa · rações</span>
            <input type="number" min={0} step={1} className="bg-bg border border-border rounded-md px-2 py-1.5" value={draft.victoryReward?.rations ?? 0}
              onChange={(e) => setDraft((d) => ({ ...d, victoryReward: { ...d.victoryReward, ember: d.victoryReward?.ember ?? 0, rations: Math.max(0, Math.floor(Number(e.target.value) || 0)) } }))} />
          </label>
          <div className="flex flex-col gap-1 col-span-2">
            <span className="text-muted text-xs uppercase tracking-wide">NPCs necessários para a recompensa</span>
            {[...new Set([...(draft.neutralSpawns ?? []).filter(s => s.dialog).map(s => s.name), ...(draft.victoryReward?.requiredNpcNames ?? [])])].map(name => (
              <label key={name} className="flex items-center gap-2 text-sm">
                <input type="checkbox" checked={draft.victoryReward?.requiredNpcNames?.includes(name) ?? false}
                  onChange={(e) => { const checked = e.target.checked; setDraft((d) => ({ ...d, victoryReward: { ember: d.victoryReward?.ember ?? 0, rations: d.victoryReward?.rations ?? 0, requiredNpcNames: checked ? [...new Set([...(d.victoryReward?.requiredNpcNames ?? []), name])] : (d.victoryReward?.requiredNpcNames ?? []).filter(value => value !== name) } })); }} />
                {name}
              </label>
            ))}
            <span className="text-muted text-[11px]">Pagamento ao concluir a vitória, com todos os inimigos derrotados e os NPCs indicados vivos.</span>
          </div>
          <div className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Diálogo de abertura</span>
            <div className="flex items-center gap-2">
              <label className="flex items-center gap-1.5 text-xs shrink-0">
                <input
                  type="checkbox"
                  checked={draft.introDialogEnabled !== false}
                  onChange={(e) => setDraft((d) => ({ ...d, introDialogEnabled: e.target.checked }))}
                />
                Ativado
              </label>
              <Button size="sm" variant="quiet" onClick={() => setDialogEditorTarget({ kind: "intro" })}>
                {draft.introDialog ? "Editar diálogo" : "Criar diálogo"}
              </Button>
            </div>
          </div>
          <div className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Diálogo de encerramento</span>
            <div className="flex items-center gap-2">
              <label className="flex items-center gap-1.5 text-xs shrink-0">
                <input
                  type="checkbox"
                  checked={draft.outroDialogEnabled !== false}
                  onChange={(e) => setDraft((d) => ({ ...d, outroDialogEnabled: e.target.checked }))}
                />
                Ativado
              </label>
              <Button size="sm" variant="quiet" onClick={() => setDialogEditorTarget({ kind: "outro" })}>
                {draft.outroDialog ? "Editar diálogo" : "Criar diálogo"}
              </Button>
            </div>
          </div>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Vitória</span>
            <select
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value={draft.win}
              onChange={(e) => setDraft((d) => ({ ...d, win: e.target.value as WinCondition }))}
            >
              <option value="rout">Derrote todos (rout)</option>
              <option value="boss">Derrube o chefe (boss)</option>
              <option value="escape">Transversal — alcance a saída (escape)</option>
            </select>
          </label>
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Local no mapa-múndi</span>
            <select
              className="bg-bg border border-border rounded-md px-2 py-1.5"
              value={draft.locationId}
              onChange={(e) => setDraft((d) => ({ ...d, locationId: e.target.value }))}
            >
              <option value="">Nenhum — não aparece no mapa</option>
              {ALL_LOCATIONS.map((l) => {
                const planned = slotsFor(l.id);
                return (
                  <option key={l.id} value={l.id}>
                    {l.name}
                    {planned > 0 ? ` (${l.missionIds.length}/${planned})` : l.missionIds.length === 0 ? " (vazio)" : ` (${l.missionIds.length})`}
                  </option>
                );
              })}
            </select>
          </label>
          {draft.locationId !== "" &&
            (() => {
              const loc = ALL_LOCATIONS.find((l) => l.id === draft.locationId);
              const fill = locationFill(draft.locationId);
              const declared = slots[draft.locationId] ?? 0;
              return (
                <label className="flex flex-col gap-1">
                  <span className="text-muted text-xs uppercase tracking-wide">Telas em {loc?.name ?? draft.locationId}</span>
                  <div className="flex items-center gap-2">
                    <input
                      type="number"
                      min={0}
                      max={99}
                      className="w-16 bg-bg border border-border rounded-md px-2 py-1.5"
                      value={declared}
                      onChange={(e) => {
                        const n = Math.max(0, Math.min(99, Number(e.target.value) || 0));
                        const next = { ...slots };
                        if (n > 0) next[draft.locationId] = n;
                        else delete next[draft.locationId];
                        void doSaveSlots(next);
                      }}
                    />
                    <span className="text-xs text-muted">
                      {fill.declared === 0
                        ? `${fill.filled} feita(s) — sem plano definido`
                        : `${fill.filled} de ${fill.declared} feita(s), faltam ${fill.empty}`}
                    </span>
                  </div>
                  {loc && loc.missionIds.length > 0 && (
                    <span className="text-xs text-muted truncate">
                      Já lá: {loc.missionIds.join(", ")}
                    </span>
                  )}
                </label>
              );
            })()}
          <label className="flex items-center gap-2 text-xs">
            <input type="checkbox" checked={!!draft.lockPartyFormation} onChange={e => setDraft(d => ({ ...d, lockPartyFormation: e.target.checked }))} />
            Preservar posições iniciais (ignorar formação do grupo)
          </label>
          <label className="flex items-center gap-2 mt-5">
            <input type="checkbox" checked={draft.hub} onChange={(e) => setDraft((d) => ({ ...d, hub: e.target.checked }))} />
            <span className="text-muted">É um hub (sem combate)</span>
          </label>
          <label className="flex items-center gap-2 rounded border border-accent px-3 py-2" title="Ligado: tiles alinhados e bordas retas para construir interiores. Desligado: piso e contorno hexagonais.">
            <input type="checkbox" aria-label="Tiles e bordas quadradas" checked={draft.squareTiles ?? hasSquareMapBorder(draft.tiles, draft.cols, draft.rows)} onChange={e => setDraft(d => ({ ...d, squareTiles: e.target.checked }))} />
            <span className="font-medium">Tiles e bordas quadradas</span>
          </label>
          <label className="flex items-center gap-2" title="Sem turnos nem vitória: o líder anda livre com um clique e conversa com os NPCs (a Estalagem)">
            <input type="checkbox" checked={!!draft.explore} onChange={(e) => setDraft((d) => ({ ...d, explore: e.target.checked }))} />
            <span className="text-muted">Exploração livre (sem turnos)</span>
          </label>
          <label className="flex items-center gap-2" title="Barricadas, barrancos e variantes de terreno alto espalhados por cima do mapa depois que ele carrega">
            <input
              type="checkbox"
              checked={draft.autoTactics}
              onChange={(e) => {
                setDraft((d) => ({ ...d, autoTactics: e.target.checked }));
                setNote(
                  e.target.checked
                    ? "Terreno automático ligado — barricadas e barrancos entram por cima do que você pintou."
                    : "Terreno automático desligado — o mapa carrega exatamente como está pintado.",
                );
              }}
            />
            <span className="text-muted">Terreno automático</span>
          </label>
          <label className="flex items-center gap-2" title="O grupo só vê um raio em volta de si; o que já passou fica lembrado mas escuro, e inimigos sem linha de visão não aparecem nem podem ser alvo">
            <input
              type="checkbox"
              checked={!!draft.fog}
              onChange={(e) => {
                setDraft((d) => ({ ...d, fog: e.target.checked }));
                setNote(
                  e.target.checked
                    ? "Névoa ligada — inimigos fora da linha de visão não aparecem nem podem ser alvo."
                    : "Névoa desligada — o mapa inteiro fica visível, como nas missões antigas.",
                );
              }}
            />
            <span className="text-muted">Névoa de guerra</span>
          </label>
        </div>

        <div className="flex flex-col gap-2 mt-3 p-3 rounded-md border border-border bg-bg/40">
          <span className="text-xs uppercase tracking-wide text-muted">Iluminação</span>
          <label className="flex items-center gap-2 text-sm">
            <span className="text-muted w-28 shrink-0">Ambiente</span>
            <select
              className="bg-bg border border-border rounded-md px-2 py-1 flex-1"
              value={draft.environment ?? "outdoor"}
              onChange={(e) => {
                const environment = e.target.value === "indoor" ? "indoor" : "outdoor";
                setDraft((d) => ({ ...d, environment }));
              }}
            >
              <option value="outdoor">Externo (sol forte, sombras marcadas)</option>
              <option value="indoor">Interno (luz suave, sem sol direto)</option>
            </select>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Escolher ajusta a luz do sol/lua e a luz ambiente para essa hora — dá pra refinar nos controles abaixo">
            <span className="text-muted w-28 shrink-0">Hora do dia</span>
            <select
              className="bg-bg border border-border rounded-md px-2 py-1 flex-1"
              value={draft.timeOfDay ?? "day"}
              onChange={(e) => {
                const timeOfDay = e.target.value as MapTimeOfDay;
                const preset = TIME_OF_DAY_LIGHT[timeOfDay];
                setDraft((d) => ({ ...d, timeOfDay, sunIntensity: preset.key, ambientIntensity: preset.ambient }));
              }}
            >
              {(Object.keys(TIME_OF_DAY_LIGHT) as MapTimeOfDay[]).map((t) => (
                <option key={t} value={t}>
                  {TIME_OF_DAY_LIGHT[t].label}
                </option>
              ))}
            </select>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Força da luz do sol (ou da lua, à noite) e de suas sombras — vai até um extremo de propósito">
            <span className="text-muted w-28 shrink-0">{TIME_OF_DAY_LIGHT[draft.timeOfDay ?? "day"].moon ? "Intensidade da lua" : "Intensidade do sol"}</span>
            <input
              type="range"
              min={0}
              max={6}
              step={0.1}
              className="flex-1"
              value={draft.sunIntensity ?? DEFAULT_SUN_INTENSITY}
              onChange={(e) => setDraft((d) => ({ ...d, sunIntensity: Number(e.target.value) }))}
            />
            <span className="text-muted text-xs w-10 text-right">{(draft.sunIntensity ?? DEFAULT_SUN_INTENSITY).toFixed(2)}</span>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Luz de preenchimento geral — vai até um extremo de propósito">
            <span className="text-muted w-28 shrink-0">Luz ambiente</span>
            <input
              type="range"
              min={0}
              max={4}
              step={0.1}
              className="flex-1"
              value={draft.ambientIntensity ?? DEFAULT_AMBIENT_INTENSITY}
              onChange={(e) => setDraft((d) => ({ ...d, ambientIntensity: Number(e.target.value) }))}
            />
            <span className="text-muted text-xs w-10 text-right">{(draft.ambientIntensity ?? DEFAULT_AMBIENT_INTENSITY).toFixed(2)}</span>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Brilho real de pós-processamento em superfícies claras/iluminadas — vai até um extremo de propósito">
            <span className="text-muted w-28 shrink-0">Brilho (bloom)</span>
            <input
              type="range"
              min={0}
              max={3}
              step={0.05}
              className="flex-1"
              value={draft.bloomIntensity ?? DEFAULT_BLOOM_INTENSITY}
              onChange={(e) => setDraft((d) => ({ ...d, bloomIntensity: Number(e.target.value) }))}
            />
            <span className="text-muted text-xs w-10 text-right">{(draft.bloomIntensity ?? DEFAULT_BLOOM_INTENSITY).toFixed(2)}</span>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Qual implementação de névoa esta missão usa">
            <span className="text-muted w-28 shrink-0">Tipo de névoa</span>
            <select
              className="bg-bg border border-border rounded-md px-2 py-1 flex-1"
              value={draft.mistType ?? "none"}
              onChange={(e) =>
                setDraft((d) => ({
                  ...d,
                  mistType: e.target.value as "mist2" | "mist3" | "mist4" | "vignette" | "vignette2" | "vignette3" | "vignette4" | "fog1" | "fog5" | "none",
                }))
              }
            >
              <option value="none">Sem névoa</option>
              <option value="mist2">Névoa 2 (textura suave, mundo inteiro)</option>
              <option value="mist3">Névoa 3 (ruído original, mundo inteiro)</option>
              <option value="mist4">Névoa 4 (vórtice nas bordas do mapa, centro sempre limpo)</option>
              <option value="fog1">Névoa 01 (só sobre área não revelada e o fundo, limpa no mapa revelado)</option>
              <option value="fog5">Névoa 5 (rasteira, fiapos finos e véus)</option>
              <option value="vignette">Vinheta (tela inteira, bordas suaves)</option>
              <option value="vignette2">Vinheta 2 (bancos de névoa profundos, centro limpo)</option>
              <option value="vignette3">Vinheta 3 (névoa rasteira em faixas, sem bordas escuras)</option>
              <option value="vignette4">Vinheta 4 (névoa monocromática nas bordas)</option>
            </select>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Névoa, só na batalha real (não aparece nesta prévia) — 1.0 é bem pesada de propósito">
            <span className="text-muted w-28 shrink-0">Névoa</span>
            <input
              type="range"
              min={0}
              max={1}
              step={0.02}
              className="flex-1"
              value={draft.mistIntensity ?? 0}
              onChange={(e) => setDraft((d) => ({ ...d, mistIntensity: Number(e.target.value) }))}
            />
            <span className="text-muted text-xs w-10 text-right">{(draft.mistIntensity ?? 0).toFixed(2)}</span>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Velocidade da deriva da névoa — 1.0 é o ritmo padrão">
            <span className="text-muted w-28 shrink-0">Vel. da névoa</span>
            <input
              type="range"
              min={0.1}
              max={4}
              step={0.05}
              className="flex-1"
              value={draft.mistSpeed ?? 1}
              onChange={(e) => setDraft((d) => ({ ...d, mistSpeed: Number(e.target.value) }))}
            />
            <span className="text-muted text-xs w-10 text-right">{(draft.mistSpeed ?? 1).toFixed(2)}</span>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Partículas de brasa/wisp subindo, só na batalha real — 1.0 é uma tempestade delas de propósito">
            <span className="text-muted w-28 shrink-0">Wisps</span>
            <input
              type="range"
              min={0}
              max={1}
              step={0.02}
              className="flex-1"
              value={draft.wispIntensity ?? 0}
              onChange={(e) => setDraft((d) => ({ ...d, wispIntensity: Number(e.target.value) }))}
            />
            <span className="text-muted text-xs w-10 text-right">{(draft.wispIntensity ?? 0).toFixed(2)}</span>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Velocidade da subida/deriva/desaparecimento dos wisps — 1.0 é o ritmo padrão">
            <span className="text-muted w-28 shrink-0">Velocidade</span>
            <input
              type="range"
              min={0.1}
              max={4}
              step={0.05}
              className="flex-1"
              value={draft.wispSpeed ?? 1}
              onChange={(e) => setDraft((d) => ({ ...d, wispSpeed: Number(e.target.value) }))}
            />
            <span className="text-muted text-xs w-10 text-right">{(draft.wispSpeed ?? 1).toFixed(2)}</span>
          </label>
          <label className="flex items-center gap-2 text-sm" title="Cor exata dos wisps — sem mistura automática com a luz da cena">
            <span className="text-muted w-28 shrink-0">Cor dos wisps</span>
            <input
              type="color"
              className="h-8 w-14 bg-bg border border-border rounded-md p-0.5"
              value={`#${(draft.wispColor ?? 0xffa552).toString(16).padStart(6, "0")}`}
              onChange={(e) => setDraft((d) => ({ ...d, wispColor: Number.parseInt(e.target.value.slice(1), 16) }))}
            />
          </label>
          <p className="text-xs text-muted">
            A vista 3D mostra estes ajustes na prévia; Testar também usa a iluminação salva no mapa.
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-2 text-sm">
          <label className="flex items-center gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Col</span>
            <input
              type="number"
              min={MIN_GRID}
              max={MAX_GRID}
              className="w-16 bg-bg border border-border rounded-md px-2 py-1"
              value={draft.cols}
              onChange={(e) => resize(Number(e.target.value) || draft.cols, draft.rows)}
            />
          </label>
          <label className="flex items-center gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">Lin</span>
            <input
              type="number"
              min={MIN_GRID}
              max={MAX_GRID}
              className="w-16 bg-bg border border-border rounded-md px-2 py-1"
              value={draft.rows}
              onChange={(e) => resize(draft.cols, Number(e.target.value) || draft.rows)}
            />
          </label>
          <div className="flex-1" />
          <div className="flex rounded-md border border-border overflow-hidden text-xs">
            {(["hex", "square"] as const).map((g) => (
              <button
                key={g}
                type="button"
                onClick={() => setGridStyle(g)}
                title={g === "hex" ? "Grade em hexágono, igual ao jogo" : "Grade quadrada (mais rápida de editar)"}
                className={`px-2.5 py-1.5 ${gridStyle === g ? "bg-accent text-bg" : "bg-bg text-muted"}`}
              >
                {g === "hex" ? "Hexágono" : "Quadrado"}
              </button>
            ))}
          </div>
          <div className="flex rounded-md border border-border overflow-hidden text-xs">
            {(["paint", "elevation", "water", "decoration", "architecture", "props3d", "player", "enemy", "npc", "summon"] as const).map((m) => (
              <button
                key={m}
                type="button"
                onClick={() => {
                  setMode(m === "props3d" ? "architecture" : m);
                  if (m === "architecture" || m === "props3d") {
                    setArchitectureDecorations(m === "props3d");
                    setDecoBrush(m === "props3d" ? "prop-3d-tavern-table" : thickWalls ? "castle-3d-thick" : "wall-3d-stone");
                  }
                  if (m === "elevation" || m === "water") setTerrain3D(true);
                  if (m === "decoration" && DECORATIONS[decoBrush]?.model3d) setDecoBrush(decorOptions[0]!.id);
                }}
                className={`px-2.5 py-1.5 ${(m === "props3d" ? mode === "architecture" && architectureDecorations : m === "architecture" ? mode === "architecture" && !architectureDecorations : mode === m) ? "bg-accent text-bg" : "bg-bg text-muted"}`}
              >
                {m === "paint" ? "Terreno" : m === "elevation" ? "Elevação" : m === "water" ? "Água 3D" : m === "decoration" ? "Decoração" : m === "architecture" ? "3D Walls" : m === "props3d" ? "3D Decorations" : m === "player" ? "Herói" : m === "enemy" ? "Inimigo" : m === "npc" ? "NPC" : "Invocação"}
              </button>
            ))}
          </div>
        </div>

        <div className="flex flex-wrap items-center gap-2">
          <Button variant="ghost" size="sm" onClick={() => addHeroes(EDITOR_HEROES)}>
            Party completa
          </Button>
          {EDITOR_HEROES.map((h) => (
            <Button
              key={h.name}
              variant="ghost"
              size="sm"
              title={CLASSES[h.classId].name}
              onClick={() => addHeroes([h])}
            >
              {h.name}
            </Button>
          ))}
          <p className="text-xs text-muted ml-auto">Nível de cada um é editável na lista abaixo.</p>
        </div>

        {mode === "water" && <div className="flex flex-wrap items-center gap-2 text-xs">
          <Button size="sm" variant={!waterErase ? "primary" : "ghost"} onClick={() => setWaterErase(false)}>Pintar água</Button>
          <Button size="sm" variant={waterErase ? "primary" : "ghost"} onClick={() => setWaterErase(true)}>Remover água</Button>
          <label>Versão <select aria-label="Versão da água" value={draft.waterVersion ?? "v2"} onChange={e => setDraft(d => ({ ...d, waterVersion: e.target.value as "v1" | "v2" | "v3" | "v4" }))} className="bg-bg border border-border rounded px-1 py-1"><option value="v1">Água V1 — clássica</option><option value="v2">Água V2 — lago realista</option><option value="v3">3D Water V3</option><option value="v4">3D Water V4</option></select></label>
          <label>Formato <select aria-label="Formato do pincel de água" value={waterShape} onChange={e => setWaterShape(e.target.value as "round" | "square")} className="bg-bg border border-border rounded px-1 py-1"><option value="round">Redondo</option><option value="square">Quadrado</option></select></label>
          <label>Tamanho <select aria-label="Tamanho do pincel de água" value={waterSize} onChange={e => setWaterSize(Number(e.target.value))} className="bg-bg border border-border rounded px-1 py-1">{[0.25, 0.5, 0.75, 1].map(size => <option key={size} value={size}>{size * 100}%</option>)}</select></label>
          <label>Nível <input aria-label="Nível da água" type="number" min={0} max={12} step={0.25} value={waterLevel} onChange={e => setWaterLevel(Math.max(0, Math.min(12, Number(e.target.value) || 0)))} className="w-16 bg-bg border border-border rounded px-1 py-1" /></label>
          <label>Área <select aria-label="Área do pincel de água" value={waterRadius} onChange={e => setWaterRadius(Number(e.target.value))} className="bg-bg border border-border rounded px-1 py-1">{[0, 1, 2, 3].map(r => <option key={r} value={r}>{r === 0 ? "Uma célula" : r === 1 ? "1 anel" : r + " anéis"}</option>)}</select></label>
          <span className="text-muted">Superfície contínua com ondas. Use o mesmo nível para um lago; o terreno acima da água forma as margens. Ctrl+Z desfaz.</span>
        </div>}
        {mode === "elevation" && <div className="flex flex-wrap items-center gap-2 text-xs">
          {(["raise", "lower", "level"] as const).map(tool => <Button key={tool} size="sm" variant={elevationTool === tool ? "primary" : "ghost"} onClick={() => setElevationTool(tool)}>{tool === "raise" ? "Elevar +1" : tool === "lower" ? "Baixar −1" : "Nivelar"}</Button>)}
          <label>Nível <input aria-label="Nível de elevação" type="number" min={0} max={12} value={elevationLevel} onChange={e => setElevationLevel(Math.max(0, Math.min(12, Number(e.target.value) || 0)))} className="w-14 bg-bg border border-border rounded px-1 py-1" /></label>
          <label>Área <select aria-label="Área do pincel de elevação" value={elevationRadius} onChange={e => setElevationRadius(Number(e.target.value))} className="bg-bg border border-border rounded px-1 py-1">{[0, 1, 2, 3].map(r => <option key={r} value={r}>{r === 0 ? "Uma célula" : r === 1 ? "1 anel" : `${r} anéis`}</option>)}</select></label>
          <span className="text-muted">Clique para esculpir. Níveis 0–12; Nivelar em 0 remove a elevação. Ctrl+Z desfaz.</span>
        </div>}
        {mode === "paint" && (
          <div className="flex flex-col gap-2">
            <div className="flex flex-wrap items-center gap-2">
              <p className="text-xs text-muted flex-1 min-w-[12rem]">Escolha um terreno ou grupo de tiles para pintar o mapa.</p>
              <Button
                size="sm"
                variant={turning ? "primary" : "ghost"}
                onClick={() => {
                  setTurning((v) => !v);
                  setNote(turning ? "Pincel de volta: clicar pinta o terreno." : "Girar armado: cada clique num hex vira o desenho 60°, sem trocar o terreno. O ponto vermelho mostra onde é o lado de baixo.");
                }}
                title="Gira o desenho do hex 60° por clique, para casar costa, estrada e muro com o vizinho. O terreno e a variante não mudam; seis cliques voltam ao original."
              >
                {turning ? "Girando — clique num hex" : "Girar hex"}
              </Button>
            </div>
            <div className="flex flex-wrap gap-1.5">
              {[
                ...BUILDER_TERRAIN.filter((t) => t !== "snow").map((terrain) => ({
                  key: terrain,
                  terrain,
                  label: TERRAIN[terrain].name,
                  tileVariant: 0,
                })),
                { key: "city" as const, terrain: "plains" as const, label: "City", tileVariant: 21 },
              ]
                .sort((a, b) => byName(a.label, b.label))
                .map(({ key, terrain, label, tileVariant }) => {
                  const selected = key === "city" ? cityMode && brush === "plains" : !cityMode && brush === terrain;
                  return (
                    <button
                      key={key}
                      type="button"
                      title={key === "city" ? `City · ${terrainHint("plains", 21)}` : terrainHint(terrain, brush === terrain ? variant : 0)}
                      onClick={() => {
                        setBrush(terrain);
                        if (key === "city") {
                          setCityMode(true);
                          setVariant((v) => ((v >= 21 && v <= 38 && v !== 22) || v === ENGINE2_V3_CITY_VARIANT ? v : 21));
                        } else {
                          setCityMode(false);
                          setVariant((v) => (terrain === "plains" && ((v >= 21 && v <= 38) || v === ENGINE2_V3_CITY_VARIANT) ? 0 : Math.min(v, (TILE_VARIANT_COUNT[terrain] ?? 1) - 1)));
                        }
                      }}
                      className={`text-xs px-1.5 py-1 rounded-md border flex items-center gap-1.5 ${selected ? "border-accent" : "border-border"}`}
                    >
                      <img src={tileVariantSrc(terrain, tileVariant)} alt="" className="size-7 rounded-sm object-cover bg-bg" />
                      {label}
                    </button>
                  );
                })}
            </div>
            {brush === "hill" && (
              <p className="text-xs text-muted">Colina cria relevo. Use Elevação para esculpir vários níveis e ver o resultado na prévia 3D.</p>
            )}
            <section className="flex flex-col gap-2 rounded-md border border-border bg-bg/30 p-2" aria-label="Icelands">
              <div className="flex flex-wrap items-center gap-2">
                <h3 className="text-xs font-semibold uppercase tracking-wide text-muted">Icelands</h3>
                <button
                  type="button"
                  title={terrainHint("snow", brush === "snow" ? variant : 0)}
                  onClick={() => {
                    setBrush("snow");
                    setCityMode(false);
                    setVariant((v) => Math.min(v, TILE_VARIANT_COUNT.snow - 1));
                  }}
                  className={`text-xs px-1.5 py-1 rounded-md border flex items-center gap-1.5 ${brush === "snow" ? "border-accent" : "border-border"}`}
                >
                  <img src={tileVariantSrc("snow", 0)} alt="" className="size-7 rounded-sm object-cover bg-bg" />
                  Neve e gelo
                </button>
              </div>
              {brush === "snow" && (
                <div className="flex items-start gap-1.5 text-xs">
                  <span className="mt-1 text-muted uppercase tracking-wide">Tiles</span>
                  <div className="h-28 min-h-[104px] min-w-0 flex-1 ember-scrollbar overflow-x-auto overflow-y-hidden rounded-md border border-border bg-bg/40 p-1.5">
                    <div className="grid grid-flow-col grid-rows-2 auto-cols-max gap-1.5">
                      {Array.from({ length: TILE_VARIANT_COUNT.snow }, (_, i) => i)
                        .sort((a, b) => byName(VARIANT_LABEL.snow?.[a] ?? String(a + 1).padStart(3, "0"), VARIANT_LABEL.snow?.[b] ?? String(b + 1).padStart(3, "0")))
                        .map((i) => (
                          <button
                            key={i}
                            type="button"
                            title={VARIANT_LABEL.snow?.[i] ?? `Arte ${String(i + 1).padStart(3, "0")}`}
                            onClick={() => setVariant(i)}
                            className={`flex items-center gap-1 rounded-md border overflow-hidden pr-1.5 ${variant === i ? "border-accent" : "border-border"}`}
                          >
                            <img src={tileVariantSrc("snow", i)} alt="" className="size-8 object-cover" />
                            <span>{VARIANT_LABEL.snow?.[i] ?? String(i + 1).padStart(3, "0")}</span>
                          </button>
                        ))}
                    </div>
                  </div>
                </div>
              )}
            </section>
            {brush !== "snow" && (TILE_VARIANT_COUNT[brush] ?? 1) >= 1 && (
              <div className="flex items-start gap-1.5 text-xs">
                <span className="mt-1 text-muted uppercase tracking-wide">Versões</span>
                <div className="h-28 min-h-[104px] min-w-0 flex-1 ember-scrollbar overflow-x-auto overflow-y-hidden rounded-md border border-border bg-bg/40 p-1.5">
                  <div className="grid grid-flow-col grid-rows-2 auto-cols-max gap-1.5">
                    {/* Sorted by label for display only — each button still targets its own
                        original variant index i (art file, saved-map value), so re-sorting
                        this list can never relabel or repaint an existing tile. */}
                    {Array.from({ length: TILE_VARIANT_COUNT[brush] ?? 1 }, (_, i) => i)
                      .filter((i) => cityMode && brush === "plains" ? (i >= 21 && i <= 38 && i !== 22) || i === ENGINE2_V3_CITY_VARIANT : !HIDDEN_VARIANTS[brush]?.includes(i))
                      .sort((a, b) => byName(VARIANT_LABEL[brush]?.[a] ?? String(a + 1).padStart(3, "0"), VARIANT_LABEL[brush]?.[b] ?? String(b + 1).padStart(3, "0")))
                      .map((i) => (
                  <button
                    key={i}
                    type="button"
                    title={VARIANT_LABEL[brush]?.[i] ?? `Arte ${String(i + 1).padStart(3, "0")}`}
                    onClick={() => setVariant(i)}
                    className={`flex items-center gap-1 rounded-md border overflow-hidden pr-1.5 ${variant === i ? "border-accent" : "border-border"}`}
                  >
                    <img src={tileVariantSrc(brush, i)} alt="" className="size-8 object-cover" />
                    <span>{VARIANT_LABEL[brush]?.[i] ?? String(i + 1).padStart(3, "0")}</span>
                  </button>
                    ))}
                  </div>
                </div>
              </div>
            )}
          </div>
        )}
        {(mode === "decoration" || mode === "architecture") && (
          <div className="flex flex-col gap-2">
            {mode === "architecture" && !architectureDecorations && (
              <div className="flex gap-2">
                {[false, true].map(thick => (
                  <Button key={String(thick)} size="sm" variant={thickWalls === thick ? "primary" : "ghost"}
                    onClick={() => {
                      setThickWalls(thick);
                      setDecoBrush(thick ? "castle-3d-thick" : "wall-3d-stone");
                    }}>
                    {thick ? "Thick Walls" : "Regular Walls"}
                  </Button>
                ))}
              </div>
            )}
            <div className="flex flex-wrap items-center gap-2">
              <p className="text-xs text-muted flex-1 min-w-[12rem]">
                {mode === "architecture"
                  ? "Coloque paredes em linhas e colunas para formar salas retangulares contínuas. Paredes e portas fechadas bloqueiam movimento e visão; passagens abertas permitem atravessar. Selecione uma peça e use Delete para remover."
                  : "Clique na casa âncora pra colocar; clique em qualquer casa que a decoração cubra pra remover. Toda casa coberta fica intransponível e bloqueia visão/tiro, não importa o terreno por baixo."}
              </p>
              <Button
                size="sm"
                variant={turningDeco ? "primary" : "ghost"}
                onClick={() => {
                  setTurningDeco((v) => !v);
                  setNote(
                    turningDeco
                      ? mode === "architecture" ? "Pincel 3D ativo: clique para colocar ou selecionar uma peça." : "Pincel de volta: clicar coloca e remove decoração."
                      : mode === "architecture" ? "Girar peça 3D armado: cada clique vira a peça 90°. Quatro cliques voltam ao original." : "Girar objeto armado: cada clique numa decoração vira ela 60°, com toda a área junto. Seis cliques voltam ao original.",
                  );
                }}
                title={mode === "architecture" ? "Gira a peça 3D 90° por clique." : "Gira a decoração 60° por clique. Uma que ocupa vários hexes gira a área inteira de uma vez — um hexágono cai sobre si mesmo a cada 60°, então essas são as únicas voltas que ainda caem em casas reais."}
              >
                {turningDeco ? mode === "architecture" ? "Girando — clique numa peça 3D" : "Girando — clique numa decoração" : "Girar objeto"}
              </Button>
            </div>
            {mode === "decoration" && <p className="text-xs text-muted">
              O dado em cada uma liga/desliga se ela pode sair no sorteio de "Gerar terreno" — aceso participa, apagado só
              entra no mapa se você colocar à mão.
            </p>}

            <div className={mode === "architecture" ? "border border-border rounded-md p-1.5 bg-bg/40" : "ember-scrollbar overflow-x-auto overflow-y-hidden border border-border rounded-md p-1.5 bg-bg/40 h-28 min-h-[104px] min-w-[280px]"}>
              <div className={mode === "architecture" ? "grid grid-cols-2 xl:grid-cols-3 gap-1.5" : "grid grid-rows-2 grid-flow-col auto-cols-max gap-1.5"}>
                {visibleDecorOptions.map((dec) => {
                  const excluded = shuffleExclude.has(dec.id);
                  return (
                    <div
                      key={dec.id}
                      className={`flex items-center gap-1 text-xs pl-2 pr-1 py-1 rounded-md border transition-shadow ${decoBrush === dec.id ? "border-amber-300 bg-amber-300/15 ring-2 ring-amber-300/70 shadow-[0_0_13px_rgba(251,191,36,0.55)]" : "border-border"}`}
                    >
                      <button
                        type="button"
                        title={`${dec.name} · ${dec.footprint.length} hexes`}
                        aria-pressed={decoBrush === dec.id}
                        onClick={() => { setDecoBrush(dec.id); setSelectedPlacedDecoration(null); setTurningDeco(false); }}
                        className="flex items-center gap-1.5"
                      >
                        {dec.model3d && !dec.wallTexture ? <span className="size-6 grid place-items-center rounded-sm border border-border text-[10px] font-semibold">3D</span> : <img
                          src={dec.wallTexture ?? decorationImage(dec.id)}
                          alt=""
                          className="size-6 rounded-sm object-cover bg-bg"
                          onError={(e) => {
                            e.currentTarget.onerror = null;
                            e.currentTarget.src = decorationImageWebp(dec.id);
                          }}
                        />}
                        {dec.name}
                      </button>
                      {!dec.model3d && <button
                        type="button"
                        onClick={() => toggleShuffleExclude(dec.id)}
                        className={`px-1 ${excluded ? "text-muted" : "text-accent"}`}
                        aria-label={excluded ? `${dec.name}: fora do sorteio` : `${dec.name}: no sorteio`}
                        title={
                          excluded
                            ? 'Fora do sorteio de "Gerar terreno" — clique pra incluir'
                            : 'No sorteio de "Gerar terreno" — clique pra excluir'
                        }
                      >
                        <Dices className="size-3.5" />
                      </button>}
                    </div>
                  );
                })}
              </div>
            </div>

            <div className="flex flex-col gap-1.5 border border-border rounded-md p-2 bg-bg/40">
              <div className="flex items-center gap-2 text-xs">
                <span className="uppercase tracking-wide text-muted">{mode === "architecture" ? "Regras da peça 3D" : "Regras da decoração"}</span>
                <span className="text-muted">
                  {selectedPlacement
                    ? `${DECORATIONS[selectedPlacement.id]?.name ?? selectedPlacement.id} em ${selectedPlacement.x},${selectedPlacement.y}`
                    : mode === "architecture" ? "clique numa peça 3D no mapa" : "clique numa decoração no mapa"}
                </span>
              </div>
              {mode === "architecture" ? (
                <div className="flex gap-2" role="group" aria-label="Orientação da peça 3D">
                  {(["horizontal", "vertical"] as const).map(orientation => (
                    <button key={orientation} type="button" aria-pressed={activeWallOrientation === orientation}
                      onClick={() => changeWallOrientation(orientation)}
                      className={`flex-1 rounded border px-3 py-2 text-sm ${activeWallOrientation === orientation ? "border-accent bg-accent/15 text-accent" : "border-border text-muted"}`}>
                      {orientation === "horizontal" ? "Horizontal" : "Vertical"}
                    </button>
                  ))}
                </div>
              ) : <><label
                className={`flex items-center gap-2 text-sm ${selectedPlacement ? "" : "opacity-50"}`}
                title="Ligado, o hexágono deixa de ser navegável. Nesta engine sólido é sólido: também passa a barrar flecha e névoa."
              >
                <input
                  type="checkbox"
                  disabled={!selectedPlacement || selectedPlacementIsSolidHouse || selectedPlacementIsSolidCart || selectedPlacementIsSolidArchitecture}
                  checked={!!selectedPlacement?.blocksPath || selectedPlacementIsSolidHouse || selectedPlacementIsSolidCart || selectedPlacementIsSolidArchitecture}
                  onChange={() => toggleDecorationRule("blocksPath")}
                />
                <span className="text-muted">Bloquear caminho</span>
              </label>
              <label
                className={`flex items-center gap-2 text-sm ${selectedPlacement ? "" : "opacity-50"}`}
                title="Ligado, quem estiver no hexágono recebe os bônus de terreno alto: +2 de dano, +1 de alcance para arco. Com Bloquear caminho também ligado vira rochedo — ninguém sobe, e flecha de quem está embaixo não passa por cima."
              >
                <input
                  type="checkbox"
                  disabled={!selectedPlacement}
                  checked={!!selectedPlacement?.yieldsHighGround}
                  onChange={() => toggleDecorationRule("yieldsHighGround")}
                />
                <span className="text-muted">Alto terreno</span>
              </label></>}
              {mode === "architecture" && (selectedArchitecture === "door" || selectedArchitecture === "doorway" || selectedArchitecture === "secretDoor") && selectedPlacement && selectedPlacement.id !== WATCHTOWER_ENTRANCE_ID && (
                <Button size="sm" onClick={() => {
                  const style = DECORATIONS[selectedPlacement.id]?.doorStyle ?? "oak";
                  const pair = THREE_D_DOOR_VARIANTS[style];
                  const id = selectedArchitecture === "door" || selectedArchitecture === "secretDoor" ? pair.open : pair.closed;
                  setDraft(d => ({ ...d, decorations: d.decorations.map(p =>
                    p.id === selectedPlacement.id && p.x === selectedPlacement.x && p.y === selectedPlacement.y
                      ? { ...p, id, blocksPath: undefined } : p) }));
                  setSelectedPlacedDecoration({ ...selectedPlacement, id });
                }}>{selectedArchitecture === "door" || selectedArchitecture === "secretDoor" ? "Abrir porta" : "Fechar passagem"}</Button>
              )}
              <p className="text-xs text-muted">
                {mode === "architecture" ? "Escolha a orientação para colocar novas peças ou mudar a peça selecionada. Paredes, portas fechadas e passagens secretas bloqueiam o caminho; vãos abertos permitem atravessar."
                  : 'Os dois só acrescentam: desligados, o hexágono mantém a regra do terreno que está embaixo. Uma barricada segue intransponível com "Bloquear caminho" desligado, porque é a definição dela que a torna sólida.'}
              </p>
            </div>

            {selectedPlacement && DECORATIONS[selectedPlacement.id]?.exitKind === "connector" && (
              <div className="flex flex-col gap-1.5 border border-border rounded-md p-2 bg-bg/40">
                <span className="text-xs uppercase tracking-wide text-muted">Passagem de andar</span>
                <label className="flex flex-col gap-1 text-sm">
                  <span className="text-muted text-xs">Leva para</span>
                  <select
                    className="bg-bg border border-border rounded px-1.5 py-1 text-xs"
                    value={selectedPlacement.targetMapId ?? ""}
                    onChange={(e) => setConnectorTarget(e.target.value)}
                  >
                    <option value="">Escolha um mapa…</option>
                    {connectorTargetReferences.map((map) => (
                      <option key={map.id} value={map.id}>
                        {map.title} · {map.id}
                      </option>
                    ))}
                  </select>
                </label>
                <label
                  className="flex items-center gap-2 text-sm"
                  title='Desligado: o resultado da batalha mostra "Avançar" (mais fundo na masmorra). Ligado: mostra "Voltar" (andar anterior). Puramente o texto/sentido mostrado ao jogador — quem decide qual mapa liga a qual é o campo "Leva para" acima.'
                >
                  <input type="checkbox" checked={!!selectedPlacement.returnConnector} onChange={toggleReturnConnector} />
                  <span className="text-muted">Volta para o andar anterior (em vez de avançar)</span>
                </label>
              </div>
            )}
          </div>
        )}
        {(mode === "player" || mode === "enemy") && (
          <p className="text-xs text-muted">
            Clique numa casa vazia pra adicionar {mode === "player" ? "um herói" : "um inimigo"}; clique numa casa ocupada
            (do mesmo lado) pra remover. Edite nome/classe na lista abaixo.
          </p>
        )}

        {mode === "npc" && (
          <label className="flex flex-col gap-1 text-sm">
            Personagem
            <select className="rounded border border-border bg-bg p-2" value={npcBrush} onChange={(event) => setNpcBrush(event.target.value as EncounterNpcId | "breadLady")}>
              {(["breadLady", ...ENCOUNTER_NPC_IDS] as const).map((id) => <option key={id} value={id}>{uiText(CLASSES[id].name)}</option>)}
            </select>
            <span className="text-xs text-muted">Os novos personagens têm animação de quatro quadros e diálogo próprio.</span>
          </label>
        )}
        {mode === "summon" && (
          <div className="flex flex-col gap-2 border border-border rounded-md p-2 bg-bg/40">
            <div className="flex flex-wrap items-center gap-2">
              <span className="text-xs uppercase tracking-wide text-muted">Lado</span>
              <div className="flex rounded-md overflow-hidden border border-border text-xs">
                {(["player", "neutral"] as const).map((sd) => (
                  <button
                    key={sd}
                    type="button"
                    onClick={() => setSummonSide(sd)}
                    className={`px-2.5 py-1.5 ${
                      summonSide === sd
                        ? sd === "neutral"
                          ? "bg-emerald-500 text-bg"
                          : "bg-accent text-bg"
                        : "bg-bg text-muted"
                    }`}
                  >
                    {sd === "player" ? "Aliada" : "Neutra"}
                  </button>
                ))}
              </div>
              <span className="text-xs uppercase tracking-wide text-muted ml-2">Tipo</span>
              <select
                className="bg-bg border border-border rounded-md px-1.5 py-1 text-xs"
                value={summonBrush}
                onChange={(e) => setSummonBrush(e.target.value as ClassId)}
              >
                {/* Neutral is also how an NPC gets placed (see the "Diálogo" button on its
                    spawn row below) — a talkable character can be any class, not just the
                    ones flagged as summons, so the picker widens for that side only. */}
                {(summonSide === "neutral" ? classOptions : summonOptions).map((c) => (
                  <option key={c} value={c}>
                    {heroNameByClassId[c] ?? `${CLASSES[c].name} · ${CLASSES[c].role}`}
                  </option>
                ))}
              </select>
            </div>
            <p className="text-xs text-muted">
              Clique numa casa vazia pra pôr {CLASSES[summonBrush].name.toLowerCase()}{" "}
              {summonSide === "neutral" ? "como fera neutra" : "do lado aliado"};
              clique numa casa ocupada desse lado pra remover.
            </p>
            <p className="text-xs text-muted">
              {summonSide === "player"
                ? "Aliadas não contam na derrota — perder todas não perde a missão."
                : "Neutras ficam paradas: não entram na ordem de turno e não contam pra limpar o mapa. Atacar uma acorda o bando inteiro da mesma classe, que vira inimigo e passa a agir na rodada seguinte — a menos que ela tenha um Diálogo (veja a lista de unidades abaixo): aí não pode ser atacada, e clicar nela conversa em vez de brigar."}
            </p>
          </div>
        )}

        <div className="flex flex-wrap items-center justify-between gap-2">
          <p className="text-xs uppercase tracking-wide text-muted">Prévia com os gráficos do jogo</p>
          <div className="flex items-center gap-1.5">
            <Button
              size="sm"
              variant="quiet"
              title="Preenche todos os hexes com o terreno e a versão selecionados acima"
              onClick={replaceBaseTile}
            >
              <img src={tileVariantSrc(brush, variant)} alt="" className="size-5 rounded-sm object-cover" />
              Substituir base
            </Button>
            <Button
              size="sm"
              variant={mode === "elementalFx" ? "primary" : "quiet"}
              title="Ativar modo de colocação de FX elementais: escolha um emissor e clique num hex da prévia"
              onClick={() => setMode((m) => (m === "elementalFx" ? "paint" : "elementalFx"))}
            >
              FX do mapa
            </Button>
            <Button
              size="sm"
              variant="quiet"
              disabled={draftPast.length === 0}
              title={draftPast.length > 0 ? `Desfazer (${draftPast.length} disponível)` : "Nada para desfazer"}
              onClick={undoDraft}
            >
              ↶ Desfazer
            </Button>
            <Button
              size="sm"
              variant="quiet"
              disabled={draftFuture.length === 0}
              title={draftFuture.length > 0 ? `Refazer (${draftFuture.length} disponível)` : "Nada para refazer"}
              onClick={redoDraft}
            >
              ↷ Refazer
            </Button>
            {mode !== "architecture" && <label className="flex items-center gap-1 rounded-md border border-border bg-bg px-2 py-1 text-xs" title="Categoria atualmente exibida na paleta de decorações">
              <span className="text-muted">Decorações</span>
              <select className="max-w-36 bg-transparent text-fg outline-none" value={decoSection} onChange={(e) => setDecoSection(e.target.value)}>
                {decorationSections.map((section) => (
                  <option key={section} value={section}>{section}</option>
                ))}
              </select>
            </label>}
            <Button
              size="sm"
              variant={showPreview ? "quiet" : "ghost"}
              aria-pressed={showPreview}
              onClick={() => {
                const next = !showPreview;
                setShowPreview(next);
                if (next) setPreviewMission(draftToMission(draft));
              }}
            >
              {showPreview ? "Ocultar prévia" : "Mostrar prévia"}
            </Button>
            <Button
              size="sm"
              variant={showTechnicalMap ? "quiet" : "ghost"}
              aria-pressed={showTechnicalMap}
              title="Mostrar ou ocultar a grade técnica de edição do mapa"
              onClick={() => setShowTechnicalMap((visible) => !visible)}
            >
              {showTechnicalMap ? "Ocultar mapa técnico" : "Mostrar mapa técnico"}
            </Button>
          </div>
        </div>
        {mode === "elementalFx" && (
          <div className="flex flex-col gap-2 border border-border rounded-md p-2 bg-bg/40">
            <p className="text-xs text-muted flex-1 min-w-[12rem]">
              Efeito permanente do mapa (WebGL) — fogo de lava, brilho de gelo, runa sagrada... Clique numa casa na
              prévia abaixo pra colocar o elemento escolhido; clique de novo pra remover só esse FX. Efeitos regulares
              2D e pixel podem coexistir na mesma casa. Toca sozinho
              assim que a batalha carrega, e continua a batalha inteira.
            </p>
            <label className="flex flex-col gap-1 text-xs text-muted sm:max-w-xs"><span>Família de FX</span><select aria-label="Família de FX" value={fxFamily} onChange={(event) => setFxFamily(event.target.value as "regular" | "procedural_pixel")} className="min-h-9 rounded border border-border bg-bg px-2 text-sm text-fg"><option value="regular">Regular</option><option value="procedural_pixel">Procedural Pixel</option></select></label>
            <div className="flex flex-wrap gap-1.5">
              {fxFamily === "regular" ? PLACEABLE_ELEMENT_KINDS.map((k) => (
                <button
                  key={k}
                  type="button"
                  onClick={() => setFxBrush(k)}
                  className={`text-xs px-2 py-1 rounded-md border ${fxBrush === k ? "border-accent bg-accent/15" : "border-border"}`}
                >
                  {ELEMENT_LABELS[k]}
                </button>
              )) : ELEMENT_FX_REGISTRY.filter((entry) => entry.family === "procedural_pixel").map((entry) => (
                <button key={entry.element} type="button" onClick={() => {
                  const element = entry.element as PixelElement;
                  setPixelFxBrush(element);
                  setPixelFxSettings(pixelDefaults(element, 2));
                  setPixelFxPresetId(pixelPresetsFor(element)[0]?.id ?? entry.id);
                }} className={`text-xs px-2 py-1 rounded-md border ${pixelFxBrush === entry.element ? "border-accent bg-accent/15" : "border-border"}`}>{entry.label.replace("Procedural Pixel ", "")}</button>
              ))}
            </div>
            {fxFamily === "procedural_pixel" && <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
              <label className="flex flex-col gap-1 rounded border border-border px-2 py-1.5 text-xs sm:col-span-2"><span>Preset</span><select aria-label="Preset de FX" value={pixelFxPresetId} onChange={(event) => setPixelFxPresetId(event.target.value)} className="min-h-9 rounded border border-border bg-bg px-2 text-sm text-fg">{pixelPresetsFor(pixelFxBrush).map((preset) => <option key={preset.id} value={preset.id}>{preset.label}</option>)}</select></label>
              <div className="flex flex-wrap gap-2 rounded border border-border p-2 sm:col-span-2">
                <button type="button" className={`rounded border px-2 py-1 text-xs ${pixelFxSettings.visualsEnabled ? "border-accent bg-accent/15" : "border-border"}`} onClick={() => setPixelFxSettings((settings) => ({ ...settings, visualsEnabled: true }))}>Mostrar emissor</button>
                <button type="button" className={`rounded border px-2 py-1 text-xs ${!pixelFxSettings.visualsEnabled && pixelFxSettings.lightEnabled ? "border-accent bg-accent/15" : "border-border"}`} onClick={() => setPixelFxSettings((settings) => ({ ...settings, visualsEnabled: false, lightEnabled: true }))}>Teste: só luz real</button>
              </div>
              {([
                ["scale","Escala",0.4,2.5,0.05],["intensity","Intensidade",0.2,2.5,0.05],["particleCount","Partículas",12,160,1],["density","Densidade",0.25,2.5,0.05],["spawnRate","Taxa de emissão",0.25,2.5,0.05],["lifetime","Duração de partícula",0.4,5,0.1],["duration","Duração total",0,60,0.5],["velocity","Velocidade",0.1,2,0.05],["verticalForce","Força vertical",0.01,0.5,0.01],["spread","Abertura",0.1,1.2,0.05],["drag","Arrasto",0,1.5,0.05],["turbulence","Turbulência",0,1.5,0.05],["rotation","Rotação",0,3,0.05],["emissive","Emissão HDR",0,5,0.1],["opacity","Opacidade",0.1,1,0.05],["lightIntensity","Luz real",0,3,0.05],["lightRadius","Raio da luz",0.3,5,0.1],["lightDecay","Decaimento da luz",0.5,3,0.1],["flickerAmount","Oscilação da luz",0,1,0.05],["flickerSpeed","Velocidade da oscilação",0.2,30,0.2],["animationSpeed","Velocidade da animação",0.2,3,0.05],["seed","Semente",1,999999,1]
              ] as [keyof PixelElementSettings,string,number,number,number][]).map(([key,label,min,max,step]) => <label key={key} className="flex flex-col gap-1 rounded border border-border px-2 py-1.5 text-xs"><span className="flex justify-between gap-2"><span>{label}</span><output>{pixelFxSettings[key]}</output></span><input aria-label={label} type="range" min={min} max={max} step={step} value={pixelFxSettings[key] as number} onChange={(event) => setPixelFxSettings((settings) => ({ ...settings, [key]: Number(event.target.value) }))} /></label>)}
              <label className="flex min-h-9 items-center justify-between rounded border border-border px-2 text-xs"><span>Luz real habilitada</span><input type="checkbox" checked={pixelFxSettings.lightEnabled} onChange={(event) => setPixelFxSettings((settings) => ({ ...settings, lightEnabled: event.target.checked }))} /></label>
              <label className="flex min-h-9 items-center justify-between rounded border border-border px-2 text-xs"><span>Loop</span><input type="checkbox" checked={pixelFxSettings.loop} onChange={(event) => setPixelFxSettings((settings) => ({ ...settings, loop: event.target.checked }))} /></label>
            </div>}
            {(draft.elementalFx?.length ?? 0) > 0 && (
              <p className="text-xs text-muted">{draft.elementalFx?.length} colocado(s) — lista pra remover fica lá embaixo, com decorações e unidades.</p>
            )}
          </div>
        )}
        {showPreview && (
          <ResizableEditorPanel
            className="map-preview-window overflow-hidden border border-border rounded-md bg-black h-[40vh] min-h-[220px] min-w-[280px]"
            title="Arraste esta alça para redimensionar a prévia"
            minHeight={220}
          >
            {previewMission ? (
              <MapPreviewCanvas
                mission={previewMission}
                art={art}
                onCellClick={onCellClick}
                tacticsView={terrain3D}
                onTacticsViewChange={setTerrain3D}
                selectedDecorationId={mode === "decoration" || mode === "architecture" ? decoBrush : undefined}
                selectedPlacedDecoration={selectedPlacedDecoration}
                onUnitSelect={selectPreviewUnit}
                onHeldUnitDelete={deleteHeldPreviewUnit}
                onUnitPlace={placePreviewUnit}
                onDecorationSelect={selectPreviewDecoration}
                onDecorationPlace={placePreviewDecoration}
                primaryObjectDrag={!turningDeco}
              />
            ) : (
              <div className="h-full w-full grid place-items-center text-xs text-muted">Carregando prévia…</div>
            )}
          </ResizableEditorPanel>
        )}

        {showTechnicalMap && <ResizableEditorPanel
          className="overflow-hidden border border-border rounded-md p-2 bg-black h-[60vh] min-h-[320px] min-w-[280px]"
          contentClassName="ember-scrollbar h-full w-full overflow-auto"
          title="Mapa técnico de edição — arraste esta alça para redimensionar"
          minHeight={320}
        >
          <div className="grid min-h-full min-w-full w-max place-items-center">
            <p className="sticky left-0 top-0 z-10 w-full bg-black/90 px-2 py-1 text-xs uppercase tracking-wide text-muted">Mapa técnico de edição</p>
            {gridStyle === "square" ? (
              <div
                className="grid gap-px w-max"
                style={{ gridTemplateColumns: `repeat(${draft.cols}, 56px)` }}
              >
              {draft.tiles.map((t, i) => {
                const x = i % draft.cols;
                const y = Math.floor(i / draft.cols);
                const occ = spawnAt(x, y);
                const deco = decoLookup.get(`${x},${y}`);
                return (
                  <button
                    key={i}
                    type="button"
                    title={occ ? spawnHint(occ.sp, occ.side) : (deco ?? terrainHint(t, draft.tileVariants[i] ?? 0))}
                    onClick={() => onTechnicalClick(x, y)}
                    className={`relative size-[56px] grid place-items-center text-sm font-bold ${deco ? "outline outline-2 outline-offset-[-2px] outline-amber-400/80" : ""}`}
                    style={{ background: TERRAIN_SWATCH[t] }}
                  >
                    {/* The type's own colour fills the cell; the serial names which art
                        variant is painted there; the red dot marks the hex's bottom side, so
                        a turned tile can be read without clicking it. */}
                    <span className="absolute inset-0 grid place-items-center text-[11px] font-semibold text-white [text-shadow:0_1px_2px_rgba(0,0,0,0.95)]">
                      {String((draft.tileVariants[i] ?? 0) + 1).padStart(3, "0")}
                    </span>
                    {(() => {
                      const { dx, dy } = bottomDot(draft.tileRots?.[i] ?? 0, 21);
                      return (
                        <span
                          className="absolute size-[7px] rounded-full bg-red-500 ring-1 ring-black/60"
                          style={{ left: 28 + dx - 3.5, top: 28 + dy - 3.5 }}
                          title={`lado de baixo · girado ${((draft.tileRots?.[i] ?? 0) * 60)}°`}
                        />
                      );
                    })()}
                    {occ ? (
                      <span className={SIDE_INK[occ.side]}>{spawnGlyph(occ.sp, occ.side)}</span>
                    ) : deco ? (
                      <span className="text-amber-300">D</span>
                    ) : null}
                  </button>
                );
              })}
              </div>
            ) : (
            (() => {
              // The editor grid is where the map actually gets read: the old 13 left the
              // per-hex serial nowhere to sit, and the serial now sits in the middle with a
              // facing dot around it, so it wants the room.
              const HR = 32;
              const SQRT3 = Math.sqrt(3);
              const hexW = SQRT3 * HR;
              const hexH = 2 * HR;
              const boardW = HR * SQRT3 * (draft.cols + 0.5);
              const boardH = HR * (1.5 * (draft.rows - 1) + 2);
              return (
                <div className="relative" style={{ width: boardW, height: boardH }}>
                  {draft.tiles.map((t, i) => {
                    const x = i % draft.cols;
                    const y = Math.floor(i / draft.cols);
                    const occ = spawnAt(x, y);
                    const deco = decoLookup.get(`${x},${y}`);
                    const cx = HR * SQRT3 * (x + 0.5 * (y & 1) + 0.5);
                    const cy = HR * (1.5 * y + 1);
                    return (
                      <button
                        key={i}
                        type="button"
                        title={occ ? spawnHint(occ.sp, occ.side) : (deco ?? terrainHint(t, draft.tileVariants[i] ?? 0))}
                        onClick={() => onTechnicalClick(x, y)}
                        className={`absolute grid place-items-center text-sm font-bold border ${deco ? "border-amber-400" : "border-black/20"}`}
                        style={{
                          left: cx - hexW / 2,
                          top: cy - HR,
                          width: hexW,
                          height: hexH,
                          background: TERRAIN_SWATCH[t],
                          clipPath: "polygon(50% 0%, 100% 25%, 100% 75%, 50% 100%, 0% 75%, 0% 25%)",
                        }}
                      >
                        {/* The type's own colour fills the hex; the serial names which art
                            variant is painted there; the red dot marks the hex's bottom
                            side, so a turned tile reads without clicking it. */}
                        <span className="absolute inset-0 grid place-items-center text-[11px] font-semibold text-white [text-shadow:0_1px_2px_rgba(0,0,0,0.95)]">
                          {String((draft.tileVariants[i] ?? 0) + 1).padStart(3, "0")}
                        </span>
                        {(() => {
                          const { dx, dy } = bottomDot(draft.tileRots?.[i] ?? 0, HR * 0.66);
                          return (
                            <span
                              className="absolute size-[7px] rounded-full bg-red-500 ring-1 ring-black/60"
                              style={{ left: hexW / 2 + dx - 3.5, top: HR + dy - 3.5 }}
                              title={`lado de baixo · girado ${((draft.tileRots?.[i] ?? 0) * 60)}°`}
                            />
                          );
                        })()}
                        {occ ? (
                          <span className={SIDE_INK[occ.side]}>{spawnGlyph(occ.sp, occ.side)}</span>
                        ) : deco ? (
                          <span className="text-amber-300">D</span>
                        ) : null}
                      </button>
                    );
                  })}
                </div>
              );
            })()
            )}
          </div>
        </ResizableEditorPanel>}

        {draft.decorations.length > 0 && (
          <div className="flex flex-col gap-1.5">
            <p className="text-xs uppercase tracking-wide text-muted">Decorações ({draft.decorations.length})</p>
            {draft.decorations.map((p, i) => (
              <div key={i} className="flex items-center gap-1.5 text-xs bg-bg border border-border rounded-md px-2 py-1">
                {DECORATIONS[p.id]?.model3d && !DECORATIONS[p.id]?.wallTexture ? <span className="size-6 grid place-items-center rounded-sm border border-border text-[10px] font-semibold">3D</span> : <img
                  src={DECORATIONS[p.id]?.wallTexture ?? decorationImage(decorationPlacementArt(p))}
                  alt=""
                  className="size-6 rounded-sm object-cover"
                  onError={(e) => {
                    e.currentTarget.onerror = null;
                    e.currentTarget.src = decorationImageWebp(decorationPlacementArt(p));
                  }}
                />}
                <span className="flex-1 min-w-0 truncate">{DECORATIONS[p.id]?.name ?? p.id}</span>
                <span className="text-muted tabular-nums">{p.x},{p.y}</span>
                <button
                  type="button"
                  onClick={() => setDraft((d) => ({ ...d, decorations: d.decorations.filter((_, idx) => idx !== i) }))}
                  className="text-danger px-1"
                  aria-label="Remover"
                >
                  <X className="size-3.5" />
                </button>
              </div>
            ))}
          </div>
        )}

        {(draft.elementalFx?.length ?? 0) > 0 && (
          <div className="flex flex-col gap-1.5">
            <p className="text-xs uppercase tracking-wide text-muted">Efeitos elementais ({draft.elementalFx?.length})</p>
            {(draft.elementalFx ?? []).map((p) => (
              <div key={p.id} className="flex items-center gap-1.5 text-xs bg-bg border border-border rounded-md px-2 py-1">
                <span className="flex-1 min-w-0 truncate">{ELEMENT_LABELS[p.kind]}</span>
                <span className="text-muted tabular-nums">{p.x},{p.y}</span>
                <button
                  type="button"
                  onClick={() => setDraft((d) => ({ ...d, elementalFx: (d.elementalFx ?? []).filter((q) => q.id !== p.id) }))}
                  className="text-danger px-1"
                  aria-label="Remover"
                >
                  <X className="size-3.5" />
                </button>
              </div>
            ))}
          </div>
        )}

        {/* Four groups over two lists: a spawn's class decides whether it is listed as a
            unit of the cast or as a summon, so changing the class in the dropdown moves the
            row between groups on its own. Indices stay the real ones into draft[side] —
            updateSpawn/removeSpawn address the underlying list, not the filtered view. */}
        {SPAWN_GROUPS.map((group) => {
          const side = group.side;
          const rows = (draft[side] ?? []).map((sp, i) => ({ sp, i })).filter(({ sp }) => isSummonClass(sp.classId) === group.summon);
          if (rows.length === 0) return null;
          return (
          <div key={`${side}-${group.summon}`} className="flex flex-col gap-1.5">
            <p className="text-xs uppercase tracking-wide text-muted">
              {group.label} ({rows.length})
            </p>
            {rows.map(({ sp: s, i }) => (
              <div key={i} className="flex items-center gap-1.5 text-xs">
                <span className="text-muted tabular-nums w-10">{s.x},{s.y}</span>
                <input
                  className="flex-1 min-w-0 bg-bg border border-border rounded-md px-1.5 py-1"
                  value={s.name}
                  onChange={(e) => updateSpawn(side, i, { name: e.target.value })}
                />
                <select
                  className="bg-bg border border-border rounded-md px-1.5 py-1"
                  value={s.classId}
                  // useClassSprite: true so the pick actually renders as that class/sprite
                  // right away — a named hero (Aldric, Kael, ...) would otherwise keep
                  // rendering as their own permanently pinned hero art (see
                  // HERO_SPRITE_BY_NAME/resolveHeroSprite in engine.ts) no matter what class
                  // is picked here, which is what the old separate "Sprite: Herói/Classe"
                  // toggle button used to require an extra manual step to override. Per
                  // direct instruction: whatever's picked in this selector IS the character,
                  // no second step, and it's stored per-spawn (so it only affects this one
                  // mission's draft, not the hero's real pinned art anywhere else).
                  onChange={(e) => updateSpawn(side, i, { classId: e.target.value as ClassId, useClassSprite: true })}
                >
                  {(side === "enemySpawns" ? enemyClassOptions : classOptions).map((c) => (
                    <option key={c} value={c}>
                      {heroNameByClassId[c] ?? `${CLASSES[c].name} · ${CLASSES[c].role}`}
                    </option>
                  ))}
                </select>
                <label className="flex items-center gap-1 shrink-0" title="Nível (só afeta o Testar)">
                  <span className="text-muted">Nv</span>
                  <input
                    type="number"
                    min={1}
                    max={MAX_LEVEL}
                    className="w-12 bg-bg border border-border rounded-md px-1 py-1"
                    value={s.level}
                    onChange={(e) =>
                      updateSpawn(side, i, { level: Math.max(1, Math.min(MAX_LEVEL, Number(e.target.value) || DEFAULT_TEST_LEVEL)) })
                    }
                  />
                </label>
                {side === "enemySpawns" && (
                  <button
                    type="button"
                    onClick={() => {
                      // Never hand a random enemy a named hero's own classId (Aldric,
                      // kaelFinal, Malrec's conjurer, ...) — that classId's unit ID belongs
                      // exclusively to that hero, not to a shuffled mook.
                      const pool = enemyClassOptions.filter((c) => !heroNameByClassId[c]);
                      const pick = pool[Math.floor(Math.random() * pool.length)] ?? s.classId;
                      updateSpawn(side, i, { classId: pick, useClassSprite: true });
                    }}
                    className="text-muted hover:text-fg px-1.5"
                    aria-label="Sortear classe"
                    title="Sortear uma classe inimiga aleatória"
                  >
                    <Shuffle className="size-3.5" />
                  </button>
                )}
                {side === "neutralSpawns" && (
                  <button
                    type="button"
                    onClick={() => setDialogEditorTarget({ kind: "spawn", index: i })}
                    className="text-xs text-muted hover:text-fg px-1.5 border border-border rounded-md shrink-0"
                    title="Editar o diálogo desta unidade"
                  >
                    {s.dialog ? "Diálogo" : "+ Diálogo"}
                  </button>
                )}
                <button type="button" onClick={() => removeSpawn(side, i)} className="text-danger px-1.5" aria-label="Remover">
                  <X className="size-3.5" />
                </button>
              </div>
            ))}
          </div>
          );
        })}


        <div className="flex flex-col gap-1.5">
          <p className="text-xs uppercase tracking-wide text-muted">
            Arquivos de "{draft.id}" no repositório ({repoFiles.length})
          </p>
          {repoFiles.length === 0 ? (
            <p className="text-xs text-muted">Nenhum — Salvar grava src/game/maps/{mapFileName(draft.id, 1)}.</p>
          ) : (
            <div className="flex flex-col gap-1">
              {repoFiles
                .slice()
                .reverse()
                .map((f) => (
                  <div key={f.serial} className="flex items-center gap-1.5 text-xs bg-bg border border-border rounded-md px-2 py-1.5">
                    <span className={`font-bold tabular-nums ${f.serial === repoLatest ? "text-accent" : ""}`}>{serialLabel(f.serial)}</span>
                    <span className="text-muted flex-1 min-w-0 truncate">
                      {f.file ?? mapFileName(draft.id, f.serial)}
                      {f.serial === activeSerial
                        ? " · ativa na campanha"
                        : f.serial === repoLatest && !trueLatestIsVersion
                          ? " · arquivo mais novo"
                          : ""}
                    </span>
                    <Button size="sm" variant="quiet" onClick={() => setDraft(f.draft)}>
                      Carregar
                    </Button>
                    <Button size="sm" variant="quiet" disabled={f.serial === activeSerial} onClick={() => doActivateFile(f)}>
                      Ativar
                    </Button>
                    <button
                      type="button"
                      onClick={() => void doDeleteFile(f.file ?? mapFileName(draft.id, f.serial))}
                      className={`px-1 ${armedDelete === (f.file ?? mapFileName(draft.id, f.serial)) ? "text-danger font-bold" : "text-danger"}`}
                      aria-label={`Apagar ${f.file ?? mapFileName(draft.id, f.serial)}`}
                    >
                      {armedDelete === (f.file ?? mapFileName(draft.id, f.serial)) ? "Erase?" : "✕"}
                    </button>
                  </div>
                ))}
            </div>
          )}
        </div>

        <div className="flex flex-col gap-1.5">
          <p className="text-xs uppercase tracking-wide text-muted">
            Versões salvas de "{draft.id}" ({versions.length})
          </p>
          {versions.length === 0 ? (
            <p className="text-xs text-muted">Nenhuma ainda — Salvar cria a v{serialLabel(1)}.</p>
          ) : (
            <div className="flex flex-col gap-1">
              {versions
                .slice()
                .reverse()
                .map((v) => (
                  <div key={v.serial} className="flex items-center gap-1.5 text-xs bg-bg border border-border rounded-md px-2 py-1.5">
                    <span className={`font-bold tabular-nums ${activeSerial === v.serial ? "text-accent" : ""}`}>v{serialLabel(v.serial)}</span>
                    <span className="text-muted flex-1 min-w-0 truncate">
                      {new Date(v.savedAt).toLocaleString()}
                      {activeSerial === v.serial
                        ? " · ativa na campanha"
                        : trueLatestIsVersion && v.serial === latestVersion?.serial
                          ? " · versão mais nova"
                          : ""}
                    </span>
                    <Button size="sm" variant="quiet" onClick={() => setDraft(v.draft)}>
                      Carregar
                    </Button>
                    <Button size="sm" variant="quiet" onClick={() => void doSendVersionToRepo(v)} title="Grava esta versão local no repositório com o próximo serial ID###">
                      Enviar ao repositório
                    </Button>
                    <Button size="sm" variant="quiet" disabled={activeSerial === v.serial} onClick={() => doActivate(v.serial)}>
                      Ativar
                    </Button>
                    <button
                      type="button"
                      onClick={() => doDeleteVersion(v.serial)}
                      className={`text-danger px-1 ${armedDelete === `local:${draft.id}:${v.serial}` ? "font-bold" : ""}`}
                      aria-label={`Excluir versão v${serialLabel(v.serial)}`}
                    >
                      {armedDelete === `local:${draft.id}:${v.serial}` ? "Erase?" : <X className="size-3.5" />}
                    </button>
                  </div>
                ))}
            </div>
          )}
          {activeSerial != null && (
            <Button size="sm" variant="ghost" onClick={doDeactivate} title="Volta esse cenário a usar os dados originais imutáveis em vez de uma versão editada">
              Usar cenário original (desativar v{activeSerial})
            </Button>
          )}
        </div>

        {exportText && (
          <label className="flex flex-col gap-1">
            <span className="text-muted text-xs uppercase tracking-wide">
              Exportado para copiar — não salvo no jogo
            </span>
            <textarea readOnly className="bg-bg border border-border rounded-md px-2 py-1.5 text-xs font-mono h-40" value={exportText} />
          </label>
        )}
      </div>

      <div className="p-4 pb-[max(1rem,env(safe-area-inset-bottom))] flex flex-col gap-2 border-t border-border">
        {/* Pinned to the action bar rather than sitting up in the form: Salvar lives down
            here, and a confirmation printed a screen and a half above it reads as silence.
            Keyed on the serial so the same message re-renders when an action repeats. */}
        {bigNote && (
          <div
            className={`rounded-lg border-2 px-3 py-2 max-h-[30vh] overflow-y-auto ${bigNote.ok ? "border-emerald-400 bg-emerald-500/20" : "border-red-500 bg-red-500/20"}`}
          >
            <div className="flex items-start justify-between gap-3">
              <p className={`font-display text-base font-bold tracking-tight leading-tight ${bigNote.ok ? "text-emerald-300" : "text-red-300"}`}>
                {bigNote.title}
              </p>
              <button type="button" onClick={() => setBigNote(null)} className="text-lg leading-none px-1 opacity-70" aria-label="Fechar">
                ×
              </button>
            </div>
            <ul className="mt-1 space-y-0.5">
              {bigNote.lines.map((l, i) => (
                <li key={i} className="text-xs leading-snug">
                  {l}
                </li>
              ))}
            </ul>
            {bigNote.dump && (
              <textarea
                readOnly
                value={bigNote.dump}
                onFocus={(e) => e.currentTarget.select()}
                className="mt-2 w-full h-20 bg-bg border border-border rounded-md p-2 text-xs font-mono"
              />
            )}
          </div>
        )}
        {note && (
          <p
            key={note.n}
            role={note.text.startsWith("NÃO SALVO:") ? "alert" : "status"}
            className={`text-sm leading-snug font-medium rounded-md px-2 py-1.5 ${
              note.text.startsWith("NÃO SALVO:")
                ? "text-red-300 bg-red-500/20 border border-red-500"
                : note.text.startsWith("Salvo:")
                  ? "text-emerald-300 bg-emerald-500/20 border border-emerald-400"
                  : "text-accent bg-accent/15 border border-accent/60"
            }`}
          >
            {note.text}
          </p>
        )}
        <div className="flex gap-2">
          <Button
            variant="quiet"
            className="flex-1 h-[22px] px-2.5 text-xs min-w-0"
            onClick={() => {
              const playerLevels = Object.fromEntries(draft.playerSpawns.map((s) => [s.name, s.level]));
              // Keyed by spawn index, not name — enemy/neutral spawns routinely share a
              // name (several "Piqueiro" on the same map), and a name-keyed map collapsed
              // every same-named spawn's level onto one shared entry, silently dropping
              // whatever the editor set for the others. See Roster.enemyLevels in engine.ts.
              const enemyLevels = Object.fromEntries(draft.enemySpawns.map((s, i) => [i, s.level]));
              const neutralLevels = Object.fromEntries((draft.neutralSpawns ?? []).map((s, i) => [i, s.level]));
              setNote("Testando — Encerrar teste nas Opções traz o mapa de volta como está.");
              onPlaytest(draftToMission(draft), playerLevels, enemyLevels, neutralLevels);
            }}
          >
            Testar
          </Button>

          <Button variant="quiet" className="flex-1 h-[22px] px-2.5 text-xs min-w-0" disabled={savingMap} onClick={() => void doSave()}>
            {savingMap ? "Salvando…" : "Salvar mapa"}
          </Button>
          <Button variant="quiet" className="flex-1 h-[22px] px-2.5 text-xs min-w-0" onClick={doExport}>
            Exportar
          </Button>
          {exportText && (
            <Button
              variant="quiet"
              className="flex-1"
              onClick={async () => {
                try {
                  await navigator.clipboard.writeText(exportText);
                  setCopyOk(true);
                } catch {
                  setCopyOk(false);
                }
              }}
            >
              {copyOk ? "Copiado!" : "Copiar"}
            </Button>
          )}
        </div>
      </div>

      {showLocations && (
        <div
          className="absolute inset-0 z-50 ember-veil flex items-center justify-center p-4"
          onClick={(e) => {
            if (e.target === e.currentTarget) setShowLocations(false);
          }}
        >
          <div className="w-full max-w-lg max-h-[85dvh] overflow-y-auto ember-window rounded-xl p-5">
            <div className="flex items-center justify-between gap-3 mb-3">
              <div>
                <p className="font-display text-xl leading-tight">Locais</p>
                <p className="text-xs text-muted">Setas do título mudam a progressão entre Locais; setas das missões mudam a sequência interna.</p>
              </div>
              <div className="flex items-center gap-2">
                <Button size="sm" variant="quiet" onClick={() => void saveScenarios()}>
                  Salvar
                </Button>
                <button type="button" onClick={() => setShowLocations(false)} className="size-8 grid place-items-center rounded-md border border-border" aria-label="Fechar">
                  <X className="size-4" />
                </button>
              </div>
            </div>

            <div className="flex flex-col gap-3">
              {locationOrder.map((locationId, locationIndex) => {
                const loc = ALL_LOCATIONS.find((location) => location.id === locationId);
                if (!loc) return null;
                const ids = order[loc.id] ?? loc.missionIds;
                const planned = slotsFor(loc.id);
                return (
                  <div key={loc.id} className="border border-border rounded-md p-2.5">
                    <div className="flex items-center gap-2 mb-1.5">
                      <p className="flex-1 text-xs uppercase tracking-wide text-muted">
                        {locationIndex + 1}. {loc.name}
                        {planned > 0 ? ` · ${ids.length}/${planned}` : ids.length > 0 ? ` · ${ids.length}` : " · vazio"}
                      </p>
                      <button type="button" disabled={locationIndex === 0} onClick={() => moveLocationInOrder(loc.id, -1)} className="px-1.5 rounded border border-border disabled:opacity-30" aria-label={`Subir ${loc.name} na campanha`} title="Subir Local na campanha">
                        ↑
                      </button>
                      <button type="button" disabled={locationIndex === locationOrder.length - 1} onClick={() => moveLocationInOrder(loc.id, 1)} className="px-1.5 rounded border border-border disabled:opacity-30" aria-label={`Descer ${loc.name} na campanha`} title="Descer Local na campanha">
                        ↓
                      </button>
                    </div>
                    <div className="mb-2 flex flex-wrap items-center gap-2">
                      <Button
                        size="sm"
                        variant="quiet"
                        onClick={() => {
                          const fresh = blankDraft();
                          const mapId = normalizeScenarioId(`${loc.id}-${Date.now().toString(36)}`);
                          const nextIndex =
                            Math.max(
                              -1,
                              ...ids.map((id) =>
                                id === draft.id
                                  ? draft.index
                                  : missionById(id)?.index ?? campaignMapReferences.find((map) => map.id === id)?.index ?? -1,
                              ),
                            ) + 1;
                          setDraft({
                            ...fresh,
                            id: mapId,
                            index: nextIndex,
                            title: `Novo mapa — ${loc.name}`,
                            place: loc.name,
                            locationId: loc.id,
                          });
                          // saveOrder (not setOrder) so the assignment reaches
                          // src/game/map-order.json immediately — it used to only exist in
                          // this component's state, undone by a reload unless the author
                          // separately remembered "Salvar cenários" before leaving, which is
                          // what was reading as "it never becomes a campaign map".
                          void saveOrder({ ...order, [loc.id]: [...(order[loc.id] ?? []), mapId] });
                          setShowLocations(false);
                          setNote(`Mapa novo criado para ${loc.name} e já na campanha. Salvar mapa grava o conteúdo dele.`);
                        }}
                      >
                        Novo mapa aqui
                      </Button>
                      {/* Two separate lists, not one merged pool — per direct instruction.
                          "Adicionar mapa" is every map that HAS a save file but isn't in any
                          Local's order yet (same "outside the campaign" set
                          randomEncounterReferences already uses); "Adicionar mapa da
                          campanha" is the opposite — a map already assigned to some OTHER
                          Local, for moving it here instead. A map's only ever in one list at
                          a time: joining a Local via either one is what makes it a campaign
                          map, and it only leaves that set once removed from every Local. */}
                      <select
                        className="min-w-0 flex-1 bg-bg border border-border rounded px-1.5 py-1 text-xs"
                        value=""
                        title="Coloca neste Local um mapa seu já salvo que ainda não está na campanha"
                        onChange={(e) => {
                          const mapId = e.target.value;
                          e.target.value = "";
                          if (mapId) transferMission(mapId, loc.id);
                        }}
                      >
                        <option value="">Adicionar mapa…</option>
                        {randomEncounterReferences.map((map) => (
                          <option key={map.id} value={map.id}>
                            {map.title} · {map.id}
                          </option>
                        ))}
                      </select>
                      <select
                        className="min-w-0 flex-1 bg-bg border border-border rounded px-1.5 py-1 text-xs"
                        value=""
                        title="Move pra este Local um mapa que já está em outro Local da campanha"
                        onChange={(e) => {
                          const mapId = e.target.value;
                          e.target.value = "";
                          if (mapId) transferMission(mapId, loc.id);
                        }}
                      >
                        <option value="">Adicionar mapa da campanha…</option>
                        {campaignMapReferences.filter((map) => campaignIds.has(map.id) && !ids.includes(map.id)).map((map) => (
                          <option key={map.id} value={map.id}>
                            {map.title} · {map.id}
                          </option>
                        ))}
                      </select>
                    </div>
                    <p className="text-xs text-muted mb-2">Andar 0: entrada. Valores negativos: subsolo. Valores positivos: acima da entrada.</p>
                    <div className="flex flex-col gap-1">
                      {[...ids.map((missionId, i) => ({ missionId, floor: 0, campaignIndex: i })), ...(submaps[loc.id] ?? []).map(s => ({ ...s, campaignIndex: -1 }))].sort((a,b) => b.floor - a.floor).map(s => {
                        const m = missionById(s.missionId);
                        return <div key={s.missionId} className="flex items-center gap-2 text-xs bg-bg border border-border rounded-md px-2 py-1.5">
                          <label className="flex items-center gap-1">Andar <input type="number" step="1" className="w-16 bg-bg border border-border rounded px-1 py-1" aria-label={`Andar de ${m?.title ?? s.missionId}`} value={s.floor} disabled={s.campaignIndex >= 0}
                            onChange={e => { const floor = e.target.valueAsNumber; if(Number.isInteger(floor)) setLocationSubmap(loc.id,s.missionId,floor); }} /></label>
                          <span className="flex-1 min-w-0 truncate">{m?.title ?? s.missionId}</span>
                          <button type="button" className="px-2 py-1 border border-border rounded" onClick={() => { void loadCampaignMap(s.missionId); setShowLocations(false); }}>Editar</button>
                          {s.campaignIndex >= 0 ? <>
                            <button type="button" disabled={s.campaignIndex === 0} onClick={() => moveInOrder(loc.id,s.missionId,-1)} aria-label="Subir na ordem da campanha">↑</button>
                            <button type="button" disabled={s.campaignIndex === ids.length-1} onClick={() => moveInOrder(loc.id,s.missionId,1)} aria-label="Descer na ordem da campanha">↓</button>
                          </> : <>
                            <button type="button" aria-label={`Subir andar de ${m?.title ?? s.missionId}`} onClick={() => setLocationSubmap(loc.id,s.missionId,s.floor === -1 ? 1 : s.floor+1)}>↑</button>
                            <button type="button" aria-label={`Descer andar de ${m?.title ?? s.missionId}`} onClick={() => setLocationSubmap(loc.id,s.missionId,s.floor === 1 ? -1 : s.floor-1)}>↓</button>
                          </>}
                          <button type="button" className="px-1 border border-border rounded text-danger" aria-label={`Remover ${m?.title ?? s.missionId}`} onClick={() => s.campaignIndex >= 0 ? removeFromLocation(loc.id,s.missionId) : removeLocationSubmap(loc.id,s.missionId)}>Remover</button>
                        </div>;
                      })}
                    </div>

                    <div className="mt-2 pt-2 border-t border-border">
                      <p className="text-[10px] uppercase tracking-wide text-muted mb-1">
                        Submaps (andares extras, fora do menu de campanha)
                      </p>
                      <div className="flex flex-wrap items-center gap-2">
                        <select
                          className="min-w-0 flex-1 bg-bg border border-border rounded px-1.5 py-1 text-xs"
                          value=""
                          title="Mapa seu já salvo, ainda fora da campanha, pra virar submap deste Local"
                          onChange={(e) => {
                            const missionId = e.target.value;
                            e.target.value = "";
                            if (!missionId) return;
                            const nextFloor = Math.max(0, ...(submaps[loc.id] ?? []).map((s) => s.floor)) + 1;
                            setLocationSubmap(loc.id, missionId, nextFloor);
                          }}
                        >
                          <option value="">Adicionar submap…</option>
                          {connectorTargetReferences
                            .filter((map) => !ids.includes(map.id) && !(submaps[loc.id] ?? []).some((s) => s.missionId === map.id))
                            .map((map) => (
                              <option key={map.id} value={map.id}>
                                {map.title} · {map.id}
                              </option>
                            ))}
                        </select>
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
        </div>
      )}

      {showRandomEncounters && (
        <div
          className="absolute inset-0 z-50 ember-veil flex items-center justify-center p-4"
          onClick={(e) => {
            if (e.target === e.currentTarget) setShowRandomEncounters(false);
          }}
        >
          <div className="w-full max-w-lg max-h-[85dvh] overflow-y-auto ember-window rounded-xl p-5">
            <div className="flex items-center justify-between gap-3 mb-3">
              <div>
                <p className="font-display text-xl leading-tight">R-Encounter</p>
                <p className="text-xs text-muted">Encontros separados da campanha. Regiões são grupos livres para receber novos biomas depois.</p>
              </div>
              <div className="flex items-center gap-2">
                <Button
                  size="sm"
                  variant="quiet"
                  onClick={() => {
                    const name = window.prompt("Nome da nova região:");
                    if (!name?.trim()) return;
                    const id = normalizeScenarioId(name);
                    if (encounterRegions.some((region) => region.id === id)) {
                      setNote("Já existe uma região com esse nome.");
                      return;
                    }
                    void saveEncounterRegions([...encounterRegions, { id, name: name.trim(), encounterIds: [] }]);
                  }}
                >
                  Nova região
                </Button>
                <button type="button" onClick={() => setShowRandomEncounters(false)} className="size-8 grid place-items-center rounded-md border border-border" aria-label="Fechar">
                  <X className="size-4" />
                </button>
              </div>
            </div>

            <div className="flex flex-col gap-3">
              {encounterRegions.map((region) => {
                const assigned = new Set(encounterRegions.flatMap((entry) => entry.encounterIds));
                const maps = region.encounterIds.map((id) => randomEncounterReferences.find((map) => map.id === id) ?? { id, title: id, index: 0 });
                const updateRegion = (encounterIds: string[]) =>
                  void saveEncounterRegions(encounterRegions.map((entry) => (entry.id === region.id ? { ...entry, encounterIds } : entry)));
                return (
                  <div key={region.id} className="border border-border rounded-md p-2.5">
                    <div className="flex items-center gap-2 mb-2">
                      <p className="flex-1 text-xs uppercase tracking-wide text-muted">{uiText(region.name)} · {maps.length} encontro{maps.length === 1 ? "" : "s"}</p>
                      <button
                        type="button"
                        onClick={() => {
                          if (!window.confirm(`Remover a região ${region.name}? Os mapas não serão apagados.`)) return;
                          void saveEncounterRegions(encounterRegions.filter((entry) => entry.id !== region.id));
                        }}
                        className="px-1 rounded border border-border text-danger"
                        aria-label={`Remover região ${region.name}`}
                      >
                        ✕
                      </button>
                    </div>
                    <div className="mb-2 flex items-center gap-2">
                      <Button
                        size="sm"
                        variant="quiet"
                        onClick={() => {
                          const fresh = blankDraft();
                          const id = normalizeScenarioId(`random-${region.id}-${Date.now().toString(36)}`);
                          updateRegion([...region.encounterIds, id]);
                          // Include the generated scenario id so two encounters in the same
                          // region never begin with the same editor-visible name.
                          setDraft({ ...fresh, id, index: 0, title: `Encontro em ${region.name} · ${id.replace(/^random-[^-]+-/, "")}`, place: region.name, locationId: "" });
                          setShowRandomEncounters(false);
                          setNote(`Novo encontro criado em ${region.name}. Salve o mapa para gravar o conteúdo.`);
                        }}
                      >
                        Novo encontro
                      </Button>
                      <select
                        className="min-w-0 flex-1 bg-bg border border-border rounded px-1.5 py-1 text-xs"
                        value=""
                        onChange={(e) => {
                          const id = e.target.value;
                          e.target.value = "";
                          if (id) updateRegion([...region.encounterIds, id]);
                        }}
                      >
                        <option value="">Adicionar mapa salvo…</option>
                        {randomEncounterReferences.filter((map) => !assigned.has(map.id)).map((map) => <option key={map.id} value={map.id}>{map.title} · {map.id}</option>)}
                      </select>
                    </div>
                    {maps.length === 0 ? <p className="text-xs text-muted">Nenhum encontro nesta região ainda.</p> : (
                      <div className="flex flex-col gap-1">
                        {maps.map((map, index) => (
                          <div key={map.id} className="flex items-center gap-1.5 text-xs bg-bg border border-border rounded-md px-2 py-1.5">
                            <span className="tabular-nums text-muted w-5 shrink-0">{index + 1}.</span>
                            <button type="button" className="flex-1 min-w-0 truncate text-left" onClick={async () => { const saved = await fetchLatestDraft(map.id); if (saved) { setDraft(saved); setShowRandomEncounters(false); } }} title="Abrir encontro no editor">{map.title}</button>
                            <button type="button" disabled={index === 0} onClick={() => { const next = [...region.encounterIds]; [next[index - 1], next[index]] = [next[index]!, next[index - 1]!]; updateRegion(next); }} className="px-1.5 rounded border border-border disabled:opacity-30" aria-label="Subir">↑</button>
                            <button type="button" disabled={index === maps.length - 1} onClick={() => { const next = [...region.encounterIds]; [next[index], next[index + 1]] = [next[index + 1]!, next[index]!]; updateRegion(next); }} className="px-1.5 rounded border border-border disabled:opacity-30" aria-label="Descer">↓</button>
                            <button type="button" onClick={() => updateRegion(region.encounterIds.filter((id) => id !== map.id))} className="px-1 rounded border border-border text-danger" aria-label={`Remover ${map.title}`}>✕</button>
                          </div>
                        ))}
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          </div>
        </div>
      )}

      {dialogEditorTarget && (
        <DialogEditor
          title={
            dialogEditorTarget.kind === "intro"
              ? "Diálogo de abertura"
              : dialogEditorTarget.kind === "outro"
                ? "Diálogo de encerramento"
                : `Diálogo — ${draft.neutralSpawns?.[dialogEditorTarget.index]?.name ?? "unidade"}`
          }
          tree={
            dialogEditorTarget.kind === "intro"
              ? draft.introDialog
              : dialogEditorTarget.kind === "outro"
                ? draft.outroDialog
                : draft.neutralSpawns?.[dialogEditorTarget.index]?.dialog
          }
          onChange={async (tree) => {
            const next = dialogEditorTarget.kind === "intro" ? { ...draft, introDialog: tree }
              : dialogEditorTarget.kind === "outro" ? { ...draft, outroDialog: tree }
              : { ...draft, neutralSpawns: (draft.neutralSpawns ?? []).map((spawn, index) => index === dialogEditorTarget.index ? { ...spawn, dialog: tree } : spawn) };
            setDraft(next);
            return await doSave(next);
          }}
          onClose={() => setDialogEditorTarget(null)}
          portraitOptions={portraitOptions}
        />
      )}
    </section>
  );
}

function CampaignScreen({
  missions = ALL_MISSIONS,
  locations = ALL_LOCATIONS,
  missionAccessFor,
  completed,
  test,
  ember,
  onBack,
  onPick,
}: {
  missions?: Mission[];
  locations?: WorldLocation[];
  missionAccessFor?: MissionAccessFn;
  completed: string[];
  test: boolean;
  ember: number;
  onBack: () => void;
  onPick: (id: string) => void;
}) {
  return (
    <section className="h-dvh min-h-0 flex flex-col bg-bg">
      <header className="flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-4 border-b border-border">
        <button type="button" onClick={onBack} className="size-10 grid place-items-center rounded-md border border-border" aria-label="Voltar">
          <ChevronLeft className="size-5" />
        </button>
        <div className="flex-1 min-w-0">
          <p className="text-sm uppercase tracking-[0.18em] text-muted">{test ? "Modo teste" : "Campanha"}</p>
          <h1 className="font-display text-3xl leading-none">Cenários</h1>
        </div>
        <p className="text-sm text-muted border border-border rounded-md px-2 py-1"><GoldAmount amount={ember} /></p>
      </header>
      <ol className="flex-1 min-h-0 overflow-auto p-4 pb-[max(1rem,env(safe-area-inset-bottom))] flex flex-col gap-2">
        {missions.map((m, campaignNumber) => {
          const lock = lockedMission(m.id, completed, test, locations, missions.map((mission) => mission.id), missionAccessFor);
          const done = completed.includes(m.id);
          const openInn = !!m.hub && !lock;
          return (
            <li key={m.id}>
              <button
                type="button"
                disabled={lock}
                onClick={() => onPick(m.id)}
                className={`w-full text-left rounded-xl border bg-surface px-4 py-3 disabled:opacity-40 ${
                  openInn ? "inn-open" : "border-border"
                }`}
              >
                <p className="flex items-center gap-2 text-sm uppercase tracking-[0.16em] text-muted">
                  {String(campaignNumber + 1).padStart(2, "0")} · {m.place}
                  {m.hub && !lock ? " · aberta" : done ? " · feito" : ""}
                  {lock && <Lock className="size-4 shrink-0" aria-label="Cenário bloqueado" />}
                </p>
                <p className="font-display text-2xl">{m.title}</p>
                <p className="text-base text-muted">{m.objective}</p>
              </button>
            </li>
          );
        })}
      </ol>
    </section>
  );
}

function BriefingScreen({
  mission,
  onBack,
  onStart,
  muted,
  onMute,
}: {
  mission: (typeof ALL_MISSIONS)[number];
  onBack: () => void;
  onStart: () => void;
  muted: boolean;
  onMute: () => void;
}) {
  // One shared backdrop for the currently shipped random encounters. Keep this routing
  // isolated here so future encounter-specific art can replace it by id without touching
  // authored campaign briefings.
  const art = mission.id === "random-encounter-14"
    ? "/game/assets/merchant-snow-market-background-001.jpg"
    : mission.id === "random-encounter-11"
      ? "/game/assets/merchant-road-background-001.jpg"
    : isRandomEncounter(mission.id) ? "/game/ui/random-encounter-briefing.jpg" : briefArt(mission.id);
  return (
    <section className="relative h-dvh min-h-0 flex flex-col overflow-hidden bg-surface">
      {art && (
        <>
          <img src={art} alt="" className="absolute inset-0 h-full w-full object-cover" />
          <div className="absolute inset-0 bg-gradient-to-t from-bg/90 via-bg/45 to-transparent" />
        </>
      )}
      <header className="relative z-10 flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-4 sm:px-6">
        <button type="button" onClick={onBack} className="size-10 grid place-items-center ember-icon-btn" aria-label="Voltar">
          <ChevronLeft className="size-5" />
        </button>
        <div className="flex-1">
          <p className="text-sm ember-kicker">{mission.place}</p>
          <h1 className="font-display text-3xl leading-none ember-title">{mission.title}</h1>
        </div>
        <button
          type="button"
          onClick={onMute}
          className="size-10 grid place-items-center ember-icon-btn"
          aria-label={muted ? "Ativar som" : "Silenciar"}
        >
          {muted ? <VolumeX className="size-5" /> : <Volume2 className="size-5" />}
        </button>
      </header>
      <div className="relative z-10 flex-1 min-h-0 overflow-y-auto px-4 pb-4 sm:px-6">
        <div className="relative max-w-xl ember-panel p-5">
          <p className="text-lg leading-relaxed text-fg">{mission.briefing}</p>
          <div className="mt-5 ember-rule pt-4">
            <p className="text-xs ember-kicker">Objetivo</p>
            <p className="mt-1 text-base font-medium text-accent">{mission.objective}</p>
          </div>
        </div>
      </div>
      <div className="relative z-10 px-4 pb-[max(1rem,env(safe-area-inset-bottom))] sm:px-6">
        <Button size="xl" className="w-full max-w-xl ember-btn ember-btn-primary" onClick={onStart}>
          <Swords className="size-5" /> {mission.id === "estalagem" ? "Entrar" : "Entrar em combate"}
        </Button>
      </div>
    </section>
  );
}

/** Battles whose intro dialog has already opened. Saving (or loading/cancelling from the slot
 * screen) leaves and re-mounts BattleScreen with the same engine — without this, every return
 * re-ran the lazy init below and reopened the intro. A new battle is a new engine, so it still
 * gets its intro once. */
const introDialogShown = new WeakSet<BattleEngine>();

function BattleScreen({
  onUseRation,
  engine,
  hud,
  paused,
  muted,
  save,
  onHud,
  onPause,
  onResume,
  onMute,
  onSave,
  onLoad,
  onQuit,
  onTitle,
  onEquipWeapon,
  onEquipItem,
  onAdjustStatPoint,
  outroDialogOpen,
  onCloseOutroDialog,
  playtest = false,
  fleeable = false,
  onDialogAction,
  onExploreConnector,
  onAffinityChange,
  onIntroDialogShown,
  firstBattleHintSeen = false,
  onFirstBattleHintShown,
}: {
  engine: BattleEngine;
  onUseRation: (hero: string) => void;
  hud: HudSnapshot;
  paused: boolean;
  muted: boolean;
  save: SaveData;
  onHud: (h: HudSnapshot) => void;
  onPause: () => void;
  onResume: () => void;
  onMute: () => void;
  onSave: () => void;
  onLoad: () => void;
  onQuit: () => void;
  onTitle: () => void;
  /** Persist a mid-battle gear change. `alsoOwn` is true when the item came out of a chest
   * this battle and therefore is not in the save's owned lists yet. */
  onEquipWeapon?: (hero: string, weaponId: string, alsoOwn: boolean) => void;
  /** Owned by the parent (GameApp) because it also gates the leave-battle transition once
   * victory is confirmed — see the hud.result effect there. */
  outroDialogOpen: boolean;
  onCloseOutroDialog: () => void;
  onEquipItem?: (hero: string, slot: EquipSlot, itemId: string | null, alsoOwn: boolean) => void;
  onAdjustStatPoint?: (hero: string, unitId: string, stat: StatPointAttribute, delta: 1 | -1) => boolean;
  /** True while running a map from the editor, which exits back to it rather than quitting. */
  playtest?: boolean;
  /** Random encounters offer an edge-only, DEX-adjusted flee action; authored campaign missions remain resumable. */
  fleeable?: boolean;
  /** An NPC reply that opens one of the Inn's menus (Brue's tavern, Vargan's smith). */
  onDialogAction?: (action: DialogAction) => void;
  onExploreConnector?: (exit: DecorationPlacement) => void;
  onAffinityChange?: () => void;
  /** The intro conversation just opened: record it in the save so it never opens again. */
  onIntroDialogShown?: () => void;
  firstBattleHintSeen?: boolean;
  onFirstBattleHintShown?: () => void;
}) {
  const [showStatus, setShowStatus] = useState(false);
  const [showCombatGuide, setShowCombatGuide] = useState(false);
  const [showLog, setShowLog] = useState(false);
  // "Magias" menu (spells outside the hotbar), drawn like the status window.
  const [spellMenuOpen, setSpellMenuOpen] = useState(false);
  const [audioSettingsOpen, setAudioSettingsOpen] = useState(false);
  const [audioLevels, setAudioLevels] = useState(() => getAudioVolumes());
  /** Editor playtests deliberately do not write their character progression into the campaign
   * save, but their level-up controls still need a real, responsive temporary allocation. */
  const [playtestStatPointAllocations, setPlaytestStatPointAllocations] = useState<Record<string, StatPointAllocation>>({});
  const logRef = useRef<HTMLDivElement | null>(null);
  const [invView, setInvView] = useState<"doll" | "pack" | null>(null);
  // Rest the pointer on a tile (or, on touch, hold a press) to read what its terrain does
  // — the same numbers the map editor shows on hover, which the player had no way to see
  // during a fight.
  const [heldTile, setHeldTile] = useState(false);
  // Keep turn order tucked away until the player opens it.
  const [showTurnOrder, setShowTurnOrder] = useState(false);
  const [cameraTilt, setCameraTilt] = useState(engine.cameraTilt);
  const [cameraTiltSide, setCameraTiltSide] = useState(engine.cameraTiltSide);
  const [tacticsCamera, setTacticsCamera] = useState(false);
  const cameraAnimation = useRef<number | null>(null);
  const moveCamera = (tilt: number, side: number) => {
    if (cameraAnimation.current !== null) cancelAnimationFrame(cameraAnimation.current);
    const startTilt = engine.cameraTilt;
    const startSide = engine.cameraTiltSide;
    const start = performance.now();
    setCameraTilt(tilt);
    setCameraTiltSide(side);
    const step = (now: number) => {
      const progress = Math.min(1, (now - start) / 240);
      const eased = progress * progress * (3 - 2 * progress);
      engine.cameraTilt = startTilt + (tilt - startTilt) * eased;
      engine.cameraTiltSide = startSide + (side - startSide) * eased;
      if (progress < 1) cameraAnimation.current = requestAnimationFrame(step);
      else {
        // Normalize only after the transition so crossing 360 degrees takes the short route.
        engine.cameraTiltSide = ((side + 180) % 360 + 360) % 360 - 180;
        setCameraTiltSide(engine.cameraTiltSide);
        cameraAnimation.current = null;
      }
    };
    cameraAnimation.current = requestAnimationFrame(step);
  };
  useEffect(() => {
    setCameraTilt(engine.cameraTilt);
    setCameraTiltSide(engine.cameraTiltSide);
    setTacticsCamera(false);
    return () => {
      if (cameraAnimation.current !== null) cancelAnimationFrame(cameraAnimation.current);
      cameraAnimation.current = null;
    };
  }, [engine]);
  const [hotbars, setHotbars] = useState<Record<string, (SlotAction | null)[]>>({});
  const [editingSlots, setEditingSlots] = useState(false);
  const [pickerSlot, setPickerSlot] = useState<number | null>(null);
  // Dismissing the finish/waypoint prompt hides only the prompt. The footer keeps a reopen
  // action available, so dismissing it never strands the player without a way to proceed.
  const [winPopupDismissed, setWinPopupDismissed] = useState(false);
  // The "Primeira batalha" orientation hint below — stays up until tapped, since it was
  // pointer-events-none and had no way to dismiss it at all.
  const [firstBattleHintDismissed, setFirstBattleHintDismissed] = useState(firstBattleHintSeen);
  const firstBattleHintRecorded = useRef(false);
  useEffect(() => {
    if (engine.mission.id !== "vau" || playtest || firstBattleHintDismissed || firstBattleHintRecorded.current) return;
    firstBattleHintRecorded.current = true;
    onFirstBattleHintShown?.();
  }, [engine, playtest, firstBattleHintDismissed, onFirstBattleHintShown]);
  // The mission's intro dialog — lazy-init so it only ever opens once, right as this screen
  // first mounts (a fresh mount happens per battle: see BattleEngine construction in
  // startBattle), never on a re-render.
  // engine.introDialogDone: already played in this fight (restored from a save) or earlier in
  // this campaign (SaveData.dialogsSeen, applied in startBattle) — it never opens again.
  const [introDialogOpen, setIntroDialogOpen] = useState(() => !introDialogShown.has(engine) && !engine.introDialogDone && !!engine.mission.introDialog && engine.mission.introDialogEnabled !== false);
  useEffect(() => {
    if (!introDialogOpen) return;
    introDialogShown.add(engine);
    if (!engine.introDialogDone) {
      engine.introDialogDone = true;
      onIntroDialogShown?.();
    }
  }, [introDialogOpen, engine, onIntroDialogShown]);
  const wasWinAvailable = useRef(false);
  const previousExitKey = useRef<string | null>(null);
  useEffect(() => {
    if (hud.winAvailable && !wasWinAvailable.current) setWinPopupDismissed(false);
    wasWinAvailable.current = hud.winAvailable;
  }, [hud.winAvailable]);
  useEffect(() => {
    const exitKey = hud.activeExit ? `${hud.activeExit.id}:${hud.activeExit.x}:${hud.activeExit.y}` : null;
    if (exitKey && exitKey !== previousExitKey.current) setWinPopupDismissed(false);
    previousExitKey.current = exitKey;
  }, [hud.activeExit]);
  useEffect(() => {
    setHotbars(loadHotbars());
  }, []);
  // Opening the log starts at the newest line; after that, new lines only pull it down while
  // the reader is still at the bottom (logPinnedRef, set by the log's onScroll).
  const logPinnedRef = useRef(true);
  useEffect(() => {
    if (showLog) logPinnedRef.current = true;
  }, [showLog]);
  useEffect(() => {
    if (showLog && logRef.current && logPinnedRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [showLog, hud.log.length]);
  // "Abrir log": the whole combat log full screen, opened at the newest line; a click anywhere closes it.
  const [logFullscreen, setLogFullscreen] = useState(false);
  const fullLogRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (logFullscreen && fullLogRef.current) fullLogRef.current.scrollTop = fullLogRef.current.scrollHeight;
  }, [logFullscreen]);
  // Loot found (a chest opened, a kill dropped gear) jumps the footer straight to the log so
  // it isn't missed; it reverts to the stat sheet on its own once a different unit takes its
  // turn, same as if the player had never touched the toggle.
  const prevLogLenRef = useRef(hud.log.length);
  useEffect(() => {
    if (hud.log.length > prevLogLenRef.current) {
      const added = hud.log.slice(prevLogLenRef.current);
      if (added.some((line) => line.startsWith("Loot:") || line.includes("achou"))) setShowLog(true);
    }
    prevLogLenRef.current = hud.log.length;
  }, [hud.log.length]);
  const activeUnitId = hud.turnQueue.find((t) => t.active)?.id ?? null;
  const prevActiveIdRef = useRef(activeUnitId);
  useEffect(() => {
    if (activeUnitId !== prevActiveIdRef.current) {
      prevActiveIdRef.current = activeUnitId;
      setShowLog(false);
    }
  }, [activeUnitId]);
  // The status sheet can browse to a unit other than whichever one hud.selected/pendingFoe/
  // inspected currently points at — engine.publicUnit is a pure read, so this never touches
  // selectedId/pendingFoeId/inspectedId and can't disturb an attack/spell forecast already
  // in progress. Resets whenever the whole status/inventory flow closes back to the plain
  // battle screen, so it reopens next time on the natural target rather than wherever
  // browsing last left off.
  const [browseId, setBrowseId] = useState<string | null>(null);
  useEffect(() => {
    if (!showStatus && !invView) setBrowseId(null);
  }, [showStatus, invView]);
  const inspectBoardUnit = useCallback((unitId: string) => {
    setBrowseId(unitId);
    setShowStatus(true);
  }, []);

  const unit: UnitPublic | null = hud.selected ?? hud.pendingFoe ?? hud.inspected;
  const statusUnit: UnitPublic | null = (browseId ? engine.publicUnit(browseId) : null) ?? unit;
  const statusAllocation = statusUnit
    ? (playtest ? playtestStatPointAllocations[statusUnit.name] ?? {} : save.statPointAllocations[statusUnit.name] ?? {})
    : {};
  const unspentStatusPoints = statusUnit
    ? Math.max(0, (statusUnit.level - 1) * STAT_POINTS_PER_LEVEL - Object.values(statusAllocation).reduce((total, value) => total + (value ?? 0), 0))
    : 0;
  const adjustStatusPoint = statusUnit?.side === "player"
    ? (stat: StatPointAttribute, delta: 1 | -1) => {
        const unit = statusUnit;
        if (!playtest) return onAdjustStatPoint?.(unit.name, unit.id, stat, delta) ?? false;
        const current = playtestStatPointAllocations[unit.name] ?? {};
        const spent = Object.values(current).reduce((total, value) => total + (value ?? 0), 0);
        const budget = Math.max(0, (unit.level - 1) * STAT_POINTS_PER_LEVEL);
        if ((delta > 0 && spent >= budget) || (delta < 0 && (current[stat] ?? 0) <= 0)) return false;
        if (!engine.adjustStatPoint(unit.id, stat, delta)) return false;
        const allocation: StatPointAllocation = { ...current };
        const next = (allocation[stat] ?? 0) + delta;
        if (next > 0) allocation[stat] = next;
        else delete allocation[stat];
        setPlaytestStatPointAllocations((all) => ({ ...all, [unit.name]: allocation }));
        onHud(engine.getHud());
        return true;
      }
    : undefined;
  // The Mochila/Equipamento screens read straight off `save`, which is only the roster
  // snapshot from before this mission started — a chest opened mid-battle mutates the live
  // BattleEngine unit (and engine.lootEmber/lootWeapons/lootEquipment), not `save`, so those
  // finds never showed up until the mission ended. Patch a live view in for the duration of
  // the battle instead of touching the screens themselves, which are also used from the Inn
  // (no `engine` there, where `save` genuinely is the whole truth).
  // Test battles show a brand-new save's gear (starter weapon in hand, nothing else), never
  // the real slot's collected weapons/equipment.
  const gearBase = playtest ? emptySave() : save;
  const liveWeapons = { ...gearBase.weapons };
  for (const id of engine.lootWeapons) if (!(id in liveWeapons)) liveWeapons[id] = 0;
  const liveLooseEquipment = { ...gearBase.looseEquipment };
  for (const id of engine.lootEquipment) liveLooseEquipment[id] = (liveLooseEquipment[id] ?? 0) + 1;
  const liveSave: SaveData = {
    ...save,
    heroHunger: { ...save.heroHunger, ...engine.battlePlayerHunger() },
    ember: save.ember + engine.lootEmber,
    rations: save.rations + engine.lootRations,
    bags: { ...save.bags, ...Object.fromEntries(engine.units.filter((u) => u.side === "player").map((u) => [u.name, u.bag])) },
    weapons: liveWeapons,
    equipped: gearBase.equipped,
    equipment: gearBase.equipment,
    looseEquipment: liveLooseEquipment,
  };
  const foe = hud.pendingFoe ?? (hud.inspected && hud.inspected.side === "enemy" && hud.selected ? hud.inspected : null);
  const showAct = hud.mode === "awaitAction" || hud.mode === "awaitAttack" || hud.mode === "selected" || hud.mode === "awaitSpell";
  const actor = hud.selected?.side === "player" ? hud.selected : null;
  const slots = actor
    ? (() => {
        const saved = hotbars[actor.name];
        const expectedSpells = classSpells(actor.classId, actor.level, actor.name);
        if (!saved) return defaultSlots(actor.classId, actor.level, actor.name);
        // Repair hotbars persisted while a hero alias incorrectly resolved to no spells, or
        // to a stale class's spells (hotbars are keyed by hero NAME, not classId — a bar
        // saved for a name before that hero's class was fixed elsewhere, e.g. Malrec once
        // showing Lancer spells, stays wrong forever otherwise; there's no other trigger
        // that would ever re-derive it). Respect real customization: only auto-heal a bar
        // that has no spell actions at all, or contains one that doesn't actually belong to
        // this class's current kit — a deliberately empty/potion-only slot is left alone.
        const savedSpellKinds = saved.filter((slot): slot is Extract<SlotAction, { kind: "spell" }> => slot?.kind === "spell").map((slot) => slot.spell);
        // hasStaleSpell alone has to be enough to trigger a repair — gating the whole check on
        // expectedSpells.length > 0 (as this used to) meant a class with NO real spells at all
        // (Familiar/Familiar Maior — plain melee summons, expectedSpells === []) could never
        // self-heal a bar that picked up a stale spell under a colliding name (hotbars are
        // keyed by unit NAME — see the comment above — and every conjurer's familiar of a given
        // tier shares the same deterministic name, "Familiar de X", across every battle), so a
        // spell like Magic Missile stuck on it stayed forever, always reading 0 uses since
        // these classes never populate a real tier table for it to draw from.
        const hasStaleSpell = savedSpellKinds.some((k) => !expectedSpells.includes(k));
        if (hasStaleSpell || (expectedSpells.length > 0 && savedSpellKinds.length === 0)) {
          const repaired: (SlotAction | null)[] = [
            ...defaultHotbarSpells(actor.classId, actor.level, actor.name).map((spell): SlotAction => ({ kind: "spell", spell })),
            ...saved.filter((slot) => slot?.kind === "potion"),
          ].slice(0, HOTBAR_SLOTS);
          while (repaired.length < HOTBAR_SLOTS) repaired.push(null);
          return repaired;
        }
        return saved;
      })()
    : [];

  function setSlot(index: number, action: SlotAction | null) {
    if (!actor) return;
    const next = { ...hotbars, [actor.name]: slots.map((s, i) => (i === index ? action : s)) };
    setHotbars(next);
    saveHotbars(next);
  }

  function runSlot(action: SlotAction) {
    if (action.kind === "potion") {
      engine.usePotion(action.potion);
      return;
    }
    switch (action.spell) {
      case "doubleStrike":
        engine.startDoubleStrike();
        break;
      case "cleave":
        engine.startCleave();
        break;
      case "fireball":
        engine.startFireball();
        break;
      case "frost": engine.startFrost(); break;
      case "iceStorm":
        engine.startIceStorm();
        break;
      case "causticVenom":
        engine.startCausticVenom();
        break;
      case "divineBolt":
        engine.startDivineBolt();
        break;
      case "minorVenom":
        engine.startMinorVenom();
        break;
      case "lightning":
        engine.startLightning();
        break;
      case "lightningTier3":
        engine.startLightningTier3();
        break;
      case "shock":
        engine.startShock();
        break;
      case "magicMissile":
        engine.startMagicMissile();
        break;
      case "lifeDrain":
        engine.startLifeDrain();
        break;
      case "longShot":
        engine.startLongShot();
        break;
      case "bloodyShot":
        engine.startBloodyShot();
        break;
      case "piercing":
        engine.startPiercing();
        break;
      case "cureMinor":
        engine.startCure("cureMinor");
        break;
      case "bless":
        engine.startBless();
        break;
      case "cureWounds":
        engine.startCure("cureWounds");
        break;
      case "cureDisease":
        engine.startCureDisease();
        break;
      case "piercingThrust":
        engine.startPiercingThrust();
        break;
      case "sweep":
        engine.startSweep();
        break;
      case "trip":
        engine.startTrip();
        break;
      case "summonFamiliar":
        engine.startSummonFamiliar();
        break;
      case "phantasmalForce":
        engine.startPhantasmalForce();
        break;
      case "summonFamiliar2":
        engine.startSummonFamiliar2();
        break;
      case "summonFamiliar3":
        engine.startSummonFamiliar3();
        break;
      case "summonFamiliar4":
        engine.startSummonFamiliar4();
        break;
      case "summonZombieDog":
        engine.startSummonZombieDog();
        break;
      case "webOfDreams":
        engine.startWebOfDreams();
        break;
      case "warp":
        engine.startWarp();
        break;
      case "multiShot":
        engine.startMultiShot();
        break;
      case "cureLight":
        engine.startCure("cureLight");
        break;
      case "auraOfProtection":
        engine.startAuraOfProtection();
        break;
      case "divineWrath":
        engine.startDivineWrath();
        break;
      case "shoulderSmash":
        engine.startShoulderSmash();
        break;
      case "intimidatingPresence":
        engine.startIntimidatingPresence();
        break;
      case "stampede":
        engine.startStampede();
        break;
      case "secondWind":
        // Passive — never reaches the hotbar (see PRESTIGE_SPELLS); nothing to run.
        break;
      case "bullRush":
        engine.startBullRush();
        break;
      case "executionerStrike":
        engine.startExecutionerStrike();
        break;
      case "provoke":
        engine.startProvoke();
        break;
      case "shieldBash":
        engine.startShieldBash();
        break;
      case "poisonBreath":
        engine.startPoisonBreath();
        break;
      case "burningHands":
        engine.startBurningHands();
        break;
      case "turnUndead":
        engine.startTurnUndead();
        break;
      case "createFoodAndWater":
        engine.startCreateFoodAndWater();
        break;
    }
  }

  function slotDisabled(action: SlotAction): boolean {
    if (!actor || hud.busy) return true;
    const count = slotCount(action, actor);
    if (count <= 0) return true;
    if (!showAct || actor.acted) return true;
    return false;
  }

  /** Why a greyed-out slot can't be used right now, shown in its tooltip so it never looks like a bug. */
  function slotDisabledReason(action: SlotAction): string | null {
    if (!actor) return null;
    if (hud.busy) return "aguarde a ação atual terminar";
    if (slotCount(action, actor) <= 0) return "sem usos restantes";
    if (actor.acted) return `${actor.name} já agiu neste turno`;
    if (!showAct) return "termine ou cancele a ação atual primeiro";
    return null;
  }

  function slotActive(action: SlotAction): boolean {
    if (action.kind === "potion") return hud.mode === "awaitPotion";
    return hud.mode === "awaitSpell" && hud.spellKind === action.spell;
  }

  function activateSlot(i: number) {
    if (!actor || i < 0 || i >= slots.length) return;
    const action = slots[i];
    if (editingSlots) {
      setPickerSlot(i);
      return;
    }
    if (!action || slotDisabled(action)) return;
    runSlot(action);
  }
  const activateSlotRef = useRef(activateSlot);
  activateSlotRef.current = activateSlot;
  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      const m = /^F([1-9]|1[0-2])$/.exec(e.key);
      if (!m) return;
      e.preventDefault();
      activateSlotRef.current(Number(m[1]) - 1);
    }
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  return (
    <section className="relative h-dvh min-h-0 flex flex-col bg-bg">
      <div className="relative flex-1 min-h-0">
        <BattleCanvas
          engine={engine}
          onHud={onHud}
          onInspectUnit={inspectBoardUnit}
          paused={paused || introDialogOpen || outroDialogOpen || !!hud.pendingDialog}
          onTileReadout={setHeldTile}
        />
        <button
          type="button"
          onClick={() => setShowCombatGuide(true)}
          aria-label={uiText("Regras de combate", { en: "Combat rules" })}
          className="absolute right-2 top-[max(0.5rem,env(safe-area-inset-top))] z-10 pointer-events-auto flex items-center gap-1.5 rounded-md border border-white/15 bg-[#111b22]/95 px-2 py-1 text-[10px] leading-none text-slate-300 transition-colors hover:bg-[#25313b] focus-visible:outline focus-visible:outline-2 focus-visible:outline-[#cdd3dc]"
        >
          <Swords className="size-3" /><span>{uiText("Regras", { en: "Rules" })}</span>
        </button>
        {hud.turnQueue.length > 0 && (
          <div className="pointer-events-none absolute left-2 right-2 top-[max(0.5rem,env(safe-area-inset-top))] flex flex-col items-start gap-1">
            <button type="button" aria-expanded={showTurnOrder} aria-label={showTurnOrder ? "Ocultar ordem de turnos" : "Mostrar ordem de turnos"}
              onClick={() => setShowTurnOrder((open) => !open)}
              className="pointer-events-auto flex items-center gap-1.5 rounded-md border border-white/15 bg-[#111b22]/95 px-2 py-1 text-[10px] leading-none text-slate-300 transition-colors hover:bg-[#25313b] focus-visible:outline focus-visible:outline-2 focus-visible:outline-[#cdd3dc]">
              <ListOrdered className="size-3" /><span>Ordem</span>
            </button>
            {showTurnOrder && (
              <div className="flex w-fit max-w-[calc(100vw-1rem)] flex-wrap gap-0.5 rounded-md border border-white/10 bg-[#111b22]/85 p-1 text-[11px] leading-none">
                {hud.turnQueue.map((q, i) => (
                  <span key={q.id} title={`${q.name}${q.active ? " · agora" : q.acted ? " · já agiu" : ""}`} aria-label={`${q.name}${q.active ? ", agora" : q.acted ? ", já agiu" : ""}`} className={`flex shrink-0 items-center gap-1 rounded border px-1 py-1 ${q.active ? "border-[#dce2eb]/60 bg-[#dce2eb]/10 text-[#dce2eb]" : q.acted ? "border-transparent text-slate-500" : "border-white/10 text-slate-300"}`}>
                    <span className="shrink-0 text-[9px] tabular-nums text-slate-500">{i + 1}</span>
                    <span className="max-w-32 truncate">{uiText(q.name)}</span>
                    {q.active && <span className="size-1 shrink-0 rounded-full bg-current" aria-hidden="true" />}
                    {q.side === "enemy" && !q.active && <span className="size-1.5 rounded-full bg-[#e78573]" aria-label="Inimigo" />}
                  </span>
                ))}
              </div>
            )}
          </div>
        )}
        {heldTile && hud.terrain && (
          <div className="pointer-events-none absolute inset-x-0 bottom-2 flex justify-center px-3">
            <div className="ember-plate px-3 py-2 max-w-sm">
              {hud.terrain.spellZone ? (
                <>
                  <p className="font-display text-base leading-tight">{uiText(hud.terrain.spellZone.kind === "iceStorm" ? ICE_STORM.name : WEB_OF_DREAMS.name)}</p>
                  <p className="text-xs text-muted tabular-nums mt-0.5">
                    {hud.terrain.spellZone.roundsLeft} {hud.terrain.spellZone.roundsLeft === 1 ? "rodada restante" : "rodadas restantes"}
                  </p>
                  {hud.terrain.spellZone.kind === "iceStorm" ? (
                    <p className="text-xs text-accent mt-1">{uiText(`Dano de gelo por turno: ${hud.terrain.spellZone.damageFormula}. Quem sair da área deixa de receber dano.`)}</p>
                  ) : (
                    <p className="text-xs text-accent mt-1">
                      movimento limitado a {hud.terrain.spellZone.movementCap} hex · {Math.round((hud.terrain.spellZone.sleepChance ?? 0) * 100)}% de chance de adormecer por {hud.terrain.spellZone.sleepDice} a cada turno dentro da teia; duração cumulativa.
                    </p>
                  )}
                </>
              ) : (
                <>
                  <p className="font-display text-base leading-tight">{uiText(hud.terrain.name)}</p>
                  <p className="text-xs text-muted tabular-nums mt-0.5">
                    {hud.terrain.passable ? `Mov ${hud.terrain.moveCost}` : "Intransponível"} · Def +{hud.terrain.def} · Atk +{hud.terrain.atk}
                    {hud.terrain.blocksShot ? " · bloqueia tiro/visão" : ""}
                    {hud.terrain.hazard ? ` · dano ${hud.terrain.hazard} ao entrar e a cada turno` : ""}
                  </p>
                  {hud.terrain.note && <p className="text-xs text-accent mt-1">{hud.terrain.note}</p>}
                </>
              )}
            </div>
          </div>
        )}
        <div className="pointer-events-none absolute inset-x-2 top-[max(0.5rem,env(safe-area-inset-top))] z-20 flex items-start justify-end gap-1">
          {showTurnOrder && !engine.mission.explore && (
            <p className="ember-plate mr-1 px-1.5 py-0.5 text-[10px] tabular-nums text-muted pointer-events-none">
              T{hud.turn} · {hud.playerAlive}/{hud.enemyAlive}
            </p>
          )}
          {hud.terrain && (hud.terrain.note || hud.terrain.id === "barricade" || hud.terrain.id === "hill") && (
            <p className="ember-plate px-1.5 py-0.5 text-[10px] text-accent pointer-events-none max-w-[14rem] truncate">
              {hud.terrain.name}
            </p>
          )}
          <div className="flex items-center gap-1 pointer-events-auto shrink-0">
            <button
              type="button"
              onClick={() => {
                const enabled = !tacticsCamera;
                engine.tacticsCamera = enabled;
                setTacticsCamera(enabled);
                // Tática is the straight-overhead 2D view (Battle3D): no tilt or turn either way
                moveCamera(0, 0);
              }}
              className="h-7 px-2 ember-plate text-[10px] tracking-[0.14em] uppercase"
              aria-pressed={tacticsCamera}
              title="Alternar entre vista normal e vista tática diagonal"
            >
              {tacticsCamera ? "Tática" : "Normal"}
            </button>
            <button
              type="button"
              onClick={() => moveCamera(cameraTilt - 5, cameraTiltSide)}
              className="size-7 grid place-items-center ember-plate disabled:opacity-40"
              aria-label={`Diminuir inclinação da câmera (${cameraTilt}°)`}
              title="Diminuir inclinação da câmera"
            >
              <ChevronDown className="size-3.5" />
            </button>
            <button
              type="button"
              onClick={() => moveCamera(cameraTilt + 5, cameraTiltSide)}
              className="size-7 grid place-items-center ember-plate disabled:opacity-40"
              aria-label={`Aumentar inclinação da câmera (${cameraTilt}°)`}
              title="Aumentar inclinação da câmera"
            >
              <ChevronUp className="size-3.5" />
            </button>
            <button
              type="button"
              onClick={() => moveCamera(cameraTilt, cameraTiltSide - 15)}
              className="size-7 grid place-items-center ember-plate disabled:opacity-40"
              aria-label={`Girar câmera para a esquerda (${cameraTiltSide}°)`}
              title="Girar câmera para a esquerda"
            >
              <ChevronLeft className="size-3.5" />
            </button>
            <button
              type="button"
              onClick={() => moveCamera(cameraTilt, cameraTiltSide + 15)}
              className="size-7 grid place-items-center ember-plate disabled:opacity-40"
              aria-label={`Girar câmera para a direita (${cameraTiltSide}°)`}
              title="Girar câmera para a direita"
            >
              <ChevronRight className="size-3.5" />
            </button>
            <button
              type="button"
              onClick={() => {
                engine.tacticsCamera = false;
                setTacticsCamera(false);
                moveCamera(0, 0);
              }}
              className="size-7 grid place-items-center ember-plate"
              aria-label="Voltar ao ângulo normal"
              title="Voltar ao ângulo normal"
            >
              <RotateCcw className="size-3.5" />
            </button>
            <button
              type="button"
              onClick={onMute}
              className="size-7 grid place-items-center ember-plate"
              aria-label="Som"
            >
              {muted ? <VolumeX className="size-3.5" /> : <Volume2 className="size-3.5" />}
            </button>
            <button
              type="button"
              onClick={onPause}
              className="h-7 px-2 ember-plate text-[10px] tracking-[0.14em] uppercase"
            >
              Opções
            </button>
          </div>
        </div>
        {hud.banner && (
          <div className="pointer-events-none absolute inset-x-0 bottom-12 flex justify-center">
            <div className="ember-plate px-4 py-1.5 font-display text-lg tracking-wide ember-title">
              {hud.banner}
            </div>
          </div>
        )}
        {hud.targetPrompt && !hud.result && (
          <div className="pointer-events-none absolute inset-x-0 top-14 flex justify-center px-3">
            <div className="ember-plate is-accent px-4 py-2.5 text-center">
              <p className="font-display text-lg tracking-wide leading-tight ember-title">
                Escolha {hud.targetPrompt.need} alvo{hud.targetPrompt.need > 1 ? "s" : ""}
              </p>
              <p className="text-sm text-muted mt-0.5">
                {hud.targetPrompt.picked} de {hud.targetPrompt.need} escolhido{hud.targetPrompt.picked === 1 ? "" : "s"}
                {" · "}
                {hud.targetPrompt.need - hud.targetPrompt.picked === 1
                  ? "falta 1"
                  : `faltam ${hud.targetPrompt.need - hud.targetPrompt.picked}`}
                {" · pode repetir o mesmo alvo"}
              </p>
            </div>
          </div>
        )}
        {hud.tip && !(hud.winAvailable && !winPopupDismissed) && (
          <div className="pointer-events-none absolute inset-x-2 bottom-2">
            <p className="ember-plate px-2 py-1 text-xs text-muted text-center">{hud.tip}</p>
          </div>
        )}
        {hud.winAvailable && !hud.result && !winPopupDismissed && (
          <div className="pointer-events-none absolute inset-x-2 bottom-2 flex justify-center">
            <div className="pointer-events-auto ember-plate is-accent px-3 py-2 flex items-center gap-3 flex-wrap justify-center">
              <p className="text-sm">
                {engine.mission.id === "farmlands" && DECORATIONS[hud.activeExit?.id ?? ""]?.exitKind === "dungeon"
                  ? "Voltar ao mapa do mundo?"
                  : engine.mission.explore && isFarmlandsConnector(engine.mission.id, hud.activeExit?.targetMapId)
                  ? `${farmlandsDoorLabel(hud.activeExit!.targetMapId!)}?`
                  : hud.activeExit?.targetMapId === WISP_BOSS_ID
                  ? "A passagem leva à Planta Carnívora. Desejam avançar para o encontro?"
                  : hud.activeExit?.id === "escape-exit"
                  ? `Encontraram uma rota de fuga. Desejam tentar escapar? (${engine.fleeChance().toLocaleString("pt-BR", { maximumFractionDigits: 2 })}% de chance)`
                  : DECORATIONS[hud.activeExit?.id ?? ""]?.exitKind === "dungeon"
                    ? "Encontraram a saída da masmorra. Desejam sair?"
                    : isFloorConnector(hud.activeExit)
                      ? floorConnectorDirection(hud.activeExit)
                        ? floorConnectorDirection(hud.activeExit) === "up" ? "Encontraram uma passagem para subir. Deseja subir?" : "Encontraram uma passagem para descer. Deseja descer?"
                        : hud.activeExit.returnConnector
                        ? "Encontraram a passagem de volta. Deseja voltar?"
                        : "Encontraram uma passagem para o próximo andar. Deseja avançar?"
                      : "Todos os inimigos caíram. Encerrar a missão?"}
              </p>
              <div className="flex items-center gap-2">
                <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={!engine.canConfirmFinish()} onClick={() => {
                  if (engine.mission.explore && hud.activeExit && (isFarmlandsConnector(engine.mission.id, hud.activeExit.targetMapId) || (engine.mission.id === "farmlands" && DECORATIONS[hud.activeExit.id]?.exitKind === "dungeon")) && onExploreConnector) onExploreConnector(hud.activeExit);
                  else engine.confirmFinish();
                }}>
                  {engine.mission.explore && isFarmlandsConnector(engine.mission.id, hud.activeExit?.targetMapId) ? (hud.activeExit?.targetMapId === "farmlands" ? "Sair" : "Entrar") : hud.activeExit?.id === "escape-exit" ? "Tentar escapar" : isFloorConnector(hud.activeExit) ? "Sim" : hud.activeExit ? "Sair" : "Encerrar missão"}
                </Button>
                <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => setWinPopupDismissed(true)}>
                  {isFloorConnector(hud.activeExit) ? "Não" : hud.activeExit ? "Ficar" : "Continuar explorando"}
                </Button>
              </div>
            </div>
          </div>
        )}
        {/* Clicking an enemy stages the attack (engine.stageAttack): show the hit chance, then
            the player confirms or cancels. Damage is a dice roll, so no damage is predicted. */}
        {hud.pendingFoe && hud.forecast && !hud.result && (
          <div className="pointer-events-none absolute inset-x-2 bottom-2 z-30 flex justify-center">
            <div data-attack-confirm className="pointer-events-auto ember-plate is-accent px-4 py-3 flex flex-col items-center gap-1.5 text-center">
              <p className="text-xs ember-kicker">Atacar {hud.pendingFoe.name}</p>
              <p className="font-display text-2xl leading-none ember-title tabular-nums">{hud.forecast.hitOut}% de acerto</p>
              <div className="mt-1 flex items-center gap-2">
                <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" onClick={() => engine.confirmPendingAttack()}>
                  Confirmar
                </Button>
                <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => engine.cancelPendingAttack()}>
                  Cancelar
                </Button>
              </div>
            </div>
          </div>
        )}
        {/* An aimed skill waits here for Confirmar/Cancelar (engine.handleCell arms every click).
            Weapon skills on an enemy show their hit chance; no damage is predicted. */}
        {hud.spellArmed && hud.spellKind && !hud.result && !hud.pendingFoe && (
          <div className="pointer-events-none absolute inset-x-2 bottom-2 z-30 flex justify-center">
            <div data-skill-confirm className="pointer-events-auto ember-plate is-accent px-4 py-3 flex flex-col items-center gap-1.5 text-center">
              <p className="text-xs ember-kicker">{uiText(slotLabel({ kind: "spell", spell: hud.spellKind } as SlotAction))}</p>
              {hud.spellHitChance != null && (
                <p className="font-display text-2xl leading-none ember-title tabular-nums">{hud.spellHitChance}% de acerto</p>
              )}
              <div className="mt-1 flex items-center gap-2">
                <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={!hud.spellReady || hud.busy} onClick={() => engine.confirmSpell()}>
                  Confirmar
                </Button>
                <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => engine.cancelSkillConfirm()}>
                  Cancelar
                </Button>
              </div>
            </div>
          </div>
        )}
        {/* The whole combat log, full screen ("Abrir log"); a click anywhere closes it. */}
        {logFullscreen && (
          <div
            data-log-fullscreen
            role="button"
            tabIndex={0}
            aria-label={uiText("Fechar log", { en: "Close log" })}
            onClick={() => setLogFullscreen(false)}
            className="fixed inset-0 z-[60] bg-bg/95 cursor-pointer flex justify-center p-4 sm:p-8"
          >
            <div ref={fullLogRef} className="w-full max-w-3xl h-full overflow-y-auto overscroll-contain ember-scrollbar ember-panel p-5">
              <p className="text-xs ember-kicker mb-3">
                {uiText("Log de combate", { en: "Combat log" })} · {uiText("clique para fechar", { en: "click to close" })}
              </p>
              {hud.log.length === 0 ? (
                <p className="text-sm text-muted">{uiText("Nada aconteceu ainda.", { en: "Nothing has happened yet." })}</p>
              ) : (
                hud.log.map((line, i) => (
                  <p key={i} className="text-sm text-fg/90 leading-relaxed">
                    {line}
                  </p>
                ))
              )}
            </div>
          </div>
        )}
        {hud.chestLoot && (
          <div className="absolute inset-0 z-50 ember-veil flex items-center justify-center p-4">
            <div className="relative w-full max-w-sm ember-panel p-5">
              <p className="text-xs ember-kicker">Baú aberto</p>
              <h2 className="font-display text-2xl leading-none mt-1 mb-3 ember-title">{hud.chestLoot.unitName} encontrou</h2>
              <ul className="flex flex-col gap-1.5 mb-4">
                <li className="text-sm flex items-center gap-1.5">
                  <GoldAmount amount={hud.chestLoot.ember} prefix className="text-accent font-bold" />
                </li>
                {hud.chestLoot.items.map((item, i) => (
                  <li key={i}>
                    <ItemTip text={item.tip ?? item.name} className="text-sm flex items-center gap-2">
                      <img src={item.icon} alt="" className="size-8 rounded-sm object-cover bg-bg shrink-0" />
                      <span>{uiText(item.name)}</span>
                    </ItemTip>
                  </li>
                ))}
                {hud.chestLoot.items.length === 0 && <li className="text-sm text-muted">Nada além do Gold.</li>}
              </ul>
              <Button className="w-full ember-btn ember-btn-primary" onClick={() => engine.acknowledgeChestLoot()}>
                Ok
              </Button>
            </div>
          </div>
        )}
        {introDialogOpen && engine.mission.introDialog && (
          <DialogOverlay characters={engine.units} onReply={reply => { engine.applyDialogAffinity(reply); onAffinityChange?.(); }} tree={engine.mission.introDialog} onClose={() => setIntroDialogOpen(false)} />
        )}
        {hud.pendingDialog && <DialogOverlay characters={engine.units} onReply={reply => { engine.applyDialogAffinity(reply); onAffinityChange?.(); }} tree={hud.pendingDialog} onClose={() => engine.acknowledgeDialog()} onAction={onDialogAction} />}
        {outroDialogOpen && engine.mission.outroDialog && <DialogOverlay characters={engine.units} onReply={reply => { engine.applyDialogAffinity(reply); onAffinityChange?.(); }} tree={engine.mission.outroDialog} onClose={onCloseOutroDialog} />}
      </div>

      {engine.mission.id === "vau" && !playtest && !firstBattleHintDismissed && (
        <aside className="absolute z-20 inset-x-3 bottom-28 sm:bottom-32 flex justify-center" aria-label="Orientação inicial">
          <button
            type="button"
            onClick={() => setFirstBattleHintDismissed(true)}
            className="max-w-md ember-plate is-accent px-3 py-2 text-center text-xs leading-relaxed"
          >
            <span className="font-medium text-accent">Primeira batalha:</span> clique no retrato para abrir status e equipamento. Clique na barra de HP para abrir o log de combate.
            <span className="block mt-1 text-[10px] uppercase tracking-wide text-muted">Toque para fechar</span>
          </button>
        </aside>
      )}

      <footer className="relative shrink-0 ember-hud-bar px-2 pt-1.5 pb-[max(0.5rem,env(safe-area-inset-bottom))]">
        <div className="min-h-16 sm:min-h-[4.5rem] flex items-center gap-2">
          {unit ? (
            <>
              <button
                type="button"
                onClick={() => setShowStatus(true)}
                className="shrink-0 rounded-md ring-offset-2 ring-offset-surface focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent active:opacity-80"
                title="Abrir status e equipamento" aria-label="Abrir status e equipamento"
              >
                <img
                  src={portraitFor(unit.sprite).src}
                  alt=""
                  style={{ objectPosition: portraitFor(unit.sprite).position }}
                  className={portraitFor(unit.sprite).framed ? "h-16 w-12 sm:h-20 sm:w-14 object-cover rounded-md" : "h-14 w-14 object-contain"}
                />
                {unit.side === "player" && <HungerBar name={unit.name} value={unit.fullness} />}
              </button>
              {/* A div, not a <button>: a scroll box inside a button can't be wheel/drag scrolled
                  reliably, and grabbing its scrollbar counted as a click that closed the log. */}
              <div
                role="button"
                tabIndex={0}
                onClick={(e) => {
                  const log = logRef.current;
                  if (log && e.target === log && e.nativeEvent.offsetX >= log.clientWidth) return; // scrollbar grab
                  setShowLog((v) => !v);
                }}
                onKeyDown={(e) => {
                  if (e.key === "Enter" || e.key === " ") {
                    e.preventDefault();
                    setShowLog((v) => !v);
                  }
                }}
                className="min-w-0 flex-1 cursor-pointer text-left rounded-md focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
                title={showLog ? "Fechar log e ver status" : "Abrir log de combate"} aria-label={showLog ? "Fechar log e ver status" : "Abrir log de combate"}
              >
                {showLog ? (
                  <div
                    ref={logRef}
                    onScroll={(e) => {
                      // Follow new lines only while the reader is at the bottom; scrolled up, stay put.
                      const el = e.currentTarget;
                      logPinnedRef.current = el.scrollHeight - el.scrollTop - el.clientHeight < 8;
                    }}
                    className="h-16 sm:h-20 overflow-y-auto overscroll-contain pr-1 ember-scrollbar"
                  >
                    {hud.log.length === 0 ? (
                      <p className="text-[12.5px] text-muted">Nada aconteceu ainda.</p>
                    ) : (
                      hud.log.map((line, i) => (
                        <p key={i} className="text-[12.5px] text-muted leading-snug">
                          {line}
                        </p>
                      ))
                    )}
                  </div>
                ) : (
                  <>
                    <div className="flex items-baseline justify-between gap-2">
                      <p className="text-sm font-medium truncate">
                        {unit.name} · Nv {unit.level}
                        {unit.side === "player" && (
                          <span className="text-xs text-muted font-normal ml-1 align-middle tabular-nums">
                            {unit.level >= MAX_LEVEL ? "· NÍVEL MÁX." : `· ${unit.xp}/${expToLevel(unit.level)} XP`}
                          </span>
                        )}
                      </p>
                      <p className={`text-xs ${unit.side === "enemy" ? "text-danger" : "text-muted"}`}>
                        {unit.className}
                        {unit.diseased && <span className="text-danger"> · Doente</span>}
                        {unit.poisoned && <span className="text-danger"> · {POISON_TIERS[unit.poisonTier ?? "lesser"].name} ({poisonDice(unit.poisonTier ?? "lesser")})</span>}
                      </p>
                    </div>
                    <div className="mt-0.5 flex items-center gap-2">
                      <div className="h-1.5 flex-1 rounded-full bg-border overflow-hidden">
                        <div
                          className={`hp-fill h-full ${unit.side === "enemy" ? "bg-danger" : "bg-accent"}`}
                          style={{ width: `${Math.max(0, (unit.hp / unit.maxHp) * 100)}%` }}
                        />
                      </div>
                      <p className="text-xs tabular-nums text-fg shrink-0">
                        {unit.hp}/{unit.maxHp}
                      </p>
                    </div>
                    <p className="text-[11px] tabular-nums text-muted leading-snug">
                      {hud.forecast && foe
                        ? `${hud.forecast.hitOut}% em ${foe.name}${hud.forecast.canCounter ? ` · contra ${hud.forecast.hitBack}%` : " · sem contra"}`
                        : sheetLine(unit)}
                    </p>
                  </>
                )}
              </div>
              {showLog && (
                <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost shrink-0 self-start" onClick={() => setLogFullscreen(true)}>
                  {uiText("Abrir log", { en: "Open log" })}
                </Button>
              )}
            </>
          ) : (
            <p className="text-xs text-muted">{hud.phase === "enemy" ? "O inimigo age…" : "Toque num aliado ou num inimigo com o botão direito para ver o status."}</p>
          )}
        </div>
        {engine.mission.explore ? (
          <div className="flex gap-1 min-h-10 items-center mt-1">
            <p className="text-xs text-muted">Clique no chão para andar · clique em alguém para conversar.</p>
            <Button size="sm" className="ml-auto ember-btn ember-btn-sm ember-btn-primary" onClick={onQuit}>
              Sair
            </Button>
          </div>
        ) : (
        <div className="flex flex-wrap gap-1 min-h-10 items-center mt-1">
          {hud.offHandKind && (
            <Button
              size="sm"
              variant="quiet"
              className="ember-btn ember-btn-sm ember-btn-ghost"
              disabled={!showAct || hud.busy || hud.mode === "awaitSpell"}
              onClick={() => engine.startOffHand()}
              title={
                hud.offHandKind === "shield"
                  ? `${Math.round((EQUIPMENT[unit?.offHandId ?? ""]?.dmgMul ?? 0.75) * 100)}% do dano normal · 70% de chance de atordoar por 1 turno`
                  : "Ataca com a arma da mão secundária"
              }
            >
              {hud.offHandKind === "shield" ? "Investida de Escudo" : "Mão Secundária"}
            </Button>
          )}
          <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={!showAct || !hud.canAttack || hud.busy || hud.mode === "awaitSpell"} onClick={() => engine.startAttack()}>
            Atacar
          </Button>
          {hud.mode === "awaitSpell" && (
            <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={!hud.spellReady || hud.busy} onClick={() => engine.confirmSpell()}>
              Lançar
            </Button>
          )}
          {hud.canLockpick && (
            <ItemTip text={lockpickTooltip()}>
              <button
                type="button"
                disabled={!showAct || hud.busy}
                onClick={() => engine.useLockpick()}
                className="relative h-9 px-2 ember-socket flex items-center gap-1 disabled:opacity-40"
              >
                <img src="/game/icons/lockpick.png" alt="" className="size-5 rounded-sm object-contain" />
                <span className="text-sm tabular-nums">×{actor?.bag.lockpick ?? 0}</span>
              </button>
            </ItemTip>
          )}
          {fleeable && engine.canAttemptFlee() && (
            <Button
              size="sm"
              variant="ghost"
              className="ember-btn ember-btn-sm ember-btn-ghost"
              disabled={hud.busy}
              title={`Apenas na borda do mapa. ${engine.fleeChance().toLocaleString("pt-BR", { maximumFractionDigits: 2 })}% de chance; DEX do personagem aumenta a chance de fuga em DEX/3. Se falhar, o turno acaba.`}
              onClick={() => {
                if (engine.attemptFlee()) onQuit();
                else onHud(engine.getHud());
              }}
            >
              Fugir combate · {engine.fleeChance().toLocaleString("pt-BR", { maximumFractionDigits: 2 })}%
            </Button>
          )}
          <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" disabled={!showAct || hud.busy} onClick={() => engine.wait()}>
            Esperar
          </Button>
          <Button
            size="sm"
            variant="ghost"
            className="ember-btn ember-btn-sm ember-btn-ghost"
            disabled={((!showAct && hud.mode !== "awaitPotion" && hud.mode !== "awaitOffHand") || hud.busy) && !hud.canCancelMovement}
            onClick={() => engine.cancel()}
          >
            Cancelar
          </Button>
          {actor && (
            <div className="flex items-center gap-1">
              {slots.map((action, i) => {
                const empty = !action;
                const disabled = action ? slotDisabled(action) : !editingSlots;
                const blockedWhy = action && !editingSlots && disabled ? slotDisabledReason(action) : null;
                const fullFrameIcon = action?.kind === "spell" && action.spell === "summonFamiliar3";
                return (
                  <ItemTip key={i} text={`F${i + 1} · ${action ? slotTooltip(action) : "Slot vazio"}${blockedWhy ? ` — indisponível: ${blockedWhy}` : ""}`} className="relative">
                    <button
                      type="button"
                      disabled={!editingSlots && disabled}
                      onClick={() => activateSlot(i)}
                      className={`relative size-9 grid place-items-center overflow-visible ember-socket ${action && slotActive(action) ? "is-active" : ""} ${editingSlots ? "outline outline-1 outline-dashed outline-muted" : ""} disabled:opacity-40`}
                    >
                      <span className="absolute z-10 -top-1 -left-1 bg-surface border border-border rounded px-0.5 text-[8px] tabular-nums leading-tight text-muted">
                        F{i + 1}
                      </span>
                      {empty ? (
                        <span className="text-muted text-xs">+</span>
                      ) : (
                        <>
                          <img src={slotIcon(action)} alt="" className={fullFrameIcon ? "absolute inset-0 h-full w-full object-cover" : "size-6 rounded-sm object-cover"} />
                          <span className="absolute z-10 -bottom-1 -right-1 bg-surface border border-border rounded px-0.5 text-[9px] tabular-nums leading-tight">
                            {slotCount(action, actor)}
                          </span>
                        </>
                      )}
                    </button>
                  </ItemTip>
                );
              })}
              <button
                type="button"
                onClick={() => setEditingSlots((v) => !v)}
                title="Configurar slots"
                className={`size-9 grid place-items-center ember-socket${editingSlots ? " is-active" : ""}`}
              >
                <Pencil className="size-4" />
              </button>
              {(() => {
                const assigned = new Set(
                  slots.flatMap((slot) => slot?.kind === "spell" ? [slot.spell] : []),
                );
                const extraSpells = classSpells(actor.classId, actor.level, actor.name)
                  .filter((spell) => !assigned.has(spell))
                  .map((spell) => ({ kind: "spell" as const, spell }));
                if (extraSpells.length === 0) return null;
                const menuDisabled = !showAct || hud.busy || actor.acted || hud.mode === "awaitSpell";
                return (
                  <div className="relative">
                    <Button
                      size="sm"
                      variant="quiet"
                      className="ember-btn ember-btn-sm ember-btn-ghost"
                      aria-label={uiText("Magias fora da barra", { en: "Spells outside the hotbar" })}
                      title={uiText("Usar uma magia fora da barra", { en: "Cast a spell outside the hotbar" })}
                      aria-haspopup="menu"
                      aria-expanded={spellMenuOpen && !menuDisabled}
                      disabled={menuDisabled}
                      onClick={() => setSpellMenuOpen((v) => !v)}
                    >
                      {uiText("Magias", { en: "Spells" })}
                    </Button>
                    {spellMenuOpen && !menuDisabled && (
                      <>
                        <div className="fixed inset-0 z-40" onClick={() => setSpellMenuOpen(false)} />
                        {/* Same window, header and tile rows as the status sheet's spell list. */}
                        <div role="menu" className="status-panel ember-window absolute bottom-full left-0 z-50 mb-2 w-80 rounded-xl p-4">
                          <p className="text-xs ember-kicker mb-2">{uiText("Magias fora da barra", { en: "Spells outside the hotbar" })}</p>
                          <div className="grid grid-cols-1 gap-1.5">
                            {extraSpells.map((action) => (
                              <button
                                key={action.spell}
                                type="button"
                                role="menuitem"
                                disabled={slotDisabled(action)}
                                onClick={() => {
                                  setSpellMenuOpen(false);
                                  runSlot(action);
                                }}
                                className="flex w-full items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5 text-left hover:border-accent focus-visible:outline-none focus-visible:border-accent"
                              >
                                <img src={slotIcon(action)} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                                <span className="text-xs truncate min-w-0">{slotLabel(action)}</span>
                                <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier(action.spell)} · ×{slotCount(action, actor)}</span>
                              </button>
                            ))}
                          </div>
                        </div>
                      </>
                    )}
                  </div>
                );
              })()}
            </div>
          )}
          {hud.winAvailable && !hud.result && (
            <Button size="sm" className="ml-auto ember-btn ember-btn-sm ember-btn-primary" onClick={() => hud.activeExit ? setWinPopupDismissed(false) : engine.confirmFinish()}>
              {hud.activeExit ? "Usar waypoint" : "Encerrar missão"}
            </Button>
          )}
          <Button size="sm" variant="ghost" className={`ember-btn ember-btn-sm ember-btn-ghost${hud.winAvailable && !hud.result ? "" : " ml-auto"}`} disabled={hud.phase !== "player" || !!hud.result} onClick={() => engine.endTurn()}>
            Fim do turno
          </Button>
        </div>
        )}
      </footer>

      {paused && (
        <div
          className="absolute inset-0 z-30 bg-bg/80 flex items-center justify-center p-4"
          onClick={(e) => {
            if (e.target === e.currentTarget) onResume();
          }}
        >
          <div className="status-panel w-full max-w-sm max-h-[85dvh] overflow-y-auto ember-window rounded-xl p-6">
            <div className="flex items-start justify-between gap-3 mb-4">
              <h2 className="font-display text-2xl ember-title">Opções</h2>
              <button
                type="button"
                onClick={onResume}
                aria-label="Fechar opções"
                className="size-8 shrink-0 grid place-items-center ember-icon-btn"
              >
                <X className="size-4" />
              </button>
            </div>
            {fleeable && (
              <p className="text-xs text-muted border border-border rounded-md bg-bg/50 px-3 py-2 mb-4">
                Emboscada — não dá pra desistir daqui. A única saída é levar alguém até a
                borda do mapa e tentar fugir (60% + DEX/3 de chance) pelo botão "Fugir combate" na
                barra de ações.
              </p>
            )}
            <p className="text-xs ember-kicker mb-2">Zoom</p>
            <div className="grid grid-cols-4 gap-1 mb-4">
              {(["Distante", "Longe", "Médio", "Perto"] as const).map((label, i) => (
                <Button key={label} size="sm" variant={hud.zoom === i ? undefined : "quiet"} className={`ember-btn ember-btn-sm ${hud.zoom === i ? "ember-btn-primary" : "ember-btn-ghost"}`} onClick={() => engine.setZoom(i)}>
                  {label}
                </Button>
              ))}
            </div>
            <p className="text-xs ember-kicker mb-2">Velocidade</p>
            <div className="grid grid-cols-3 gap-1 mb-4">
              {(["slow", "normal", "fast"] as const).map((mode) => (
                <Button
                  key={mode}
                  size="sm"
                  variant={hud.speedMode === mode ? undefined : "quiet"}
                  className={`ember-btn ember-btn-sm ${hud.speedMode === mode ? "ember-btn-primary" : "ember-btn-ghost"}`}
                  onClick={() => engine.setSpeed(mode)}
                >
                  {mode === "slow" ? "Lenta" : mode === "normal" ? "Normal" : "Rápida"}
                </Button>
              ))}
            </div>
            <div className="mb-4 ember-rule pt-3">
              <button
                type="button"
                className="w-full flex items-center justify-between ember-slot px-3 py-2 text-left"
                onClick={() => setAudioSettingsOpen((open) => !open)}
                aria-expanded={audioSettingsOpen}
              >
                <span className="flex items-center gap-2 text-sm font-medium"><SlidersHorizontal className="size-4 text-accent" /> Áudio</span>
                {audioSettingsOpen ? <span className="text-xs text-muted">Fechar</span> : <span className="text-xs text-muted">Volumes</span>}
              </button>
              {audioSettingsOpen && (
                <div className="mt-2 rounded-md border border-border bg-bg/40 p-3 flex flex-col gap-3">
                  <label className="flex flex-col gap-1.5">
                    <span className="flex items-center justify-between text-xs uppercase tracking-[0.14em] text-muted">
                      Música <span className="tabular-nums text-fg">{Math.round(audioLevels.music * 100)}%</span>
                    </span>
                    <input
                      type="range"
                      min="0"
                      max="1"
                      step="0.01"
                      value={audioLevels.music}
                      onChange={(event) => {
                        const music = Number(event.target.value);
                        setMusicVolume(music);
                        setAudioLevels((levels) => ({ ...levels, music }));
                      }}
                      aria-label="Volume da música"
                    />
                  </label>
                  <label className="flex flex-col gap-1.5">
                    <span className="flex items-center justify-between text-xs uppercase tracking-[0.14em] text-muted">
                      Efeitos <span className="tabular-nums text-fg">{Math.round(audioLevels.sfx * 100)}%</span>
                    </span>
                    <input
                      type="range"
                      min="0"
                      max="1"
                      step="0.01"
                      value={audioLevels.sfx}
                      onChange={(event) => {
                        const sfx = Number(event.target.value);
                        setSfxVolume(sfx);
                        setAudioLevels((levels) => ({ ...levels, sfx }));
                      }}
                      aria-label="Volume dos efeitos"
                    />
                  </label>
                  <label className="flex flex-col gap-1.5">
                    <span className="flex items-center justify-between text-xs uppercase tracking-[0.14em] text-muted">
                      Cutscenes <span className="tabular-nums text-fg">{Math.round(audioLevels.cutscene * 100)}%</span>
                    </span>
                    <input
                      type="range"
                      min="0"
                      max="1"
                      step="0.01"
                      value={audioLevels.cutscene}
                      onChange={(event) => {
                        const cutscene = Number(event.target.value);
                        setCutsceneVolume(cutscene);
                        setAudioLevels((levels) => ({ ...levels, cutscene }));
                      }}
                      aria-label="Volume das cutscenes"
                    />
                  </label>
                  <div className="flex items-center justify-between gap-2">
                    <p className="text-xs text-muted">Deixe Música em 0% para ouvir somente os efeitos.</p>
                    <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => { unlockAudio(); sfxPlay.magicAttack(); }}>
                      Testar
                    </Button>
                  </div>
                </div>
              )}
            </div>
            <div className="flex flex-col gap-2">
              <OptionsButton muted={muted} onMute={onMute} />
              <Button className="ember-btn ember-btn-primary" onClick={onResume}>Continuar</Button>
              <Button variant="quiet" className="ember-btn ember-btn-ghost" onClick={onSave}>
                Save
              </Button>
              <Button variant="quiet" className="ember-btn ember-btn-ghost" onClick={onLoad}>
                Load
              </Button>
              {!fleeable && (
                <Button variant="ghost" className="ember-btn ember-btn-ghost" onClick={onQuit}>
                  {playtest ? "Encerrar teste" : "Desistir"}
                </Button>
              )}
              <Button variant="ghost" className="ember-btn ember-btn-ghost" onClick={onTitle}>
                Tela inicial
              </Button>
            </div>
          </div>
        </div>
      )}

      {showStatus && statusUnit && (
        <StatusPanel
          unit={statusUnit}
          accuracyTarget={hud.forecast && hud.forecast.attacker === statusUnit.id ? engine.publicUnit(hud.forecast.defender) ?? undefined : hud.inspected?.side === "enemy" && hud.inspected.id !== statusUnit.id ? engine.publicUnit(hud.inspected.id) ?? undefined : undefined}
          statPointAllocation={statusAllocation}
          unspentStatPoints={unspentStatusPoints}
          onAdjustStatPoint={adjustStatusPoint}
          bagIcon={pouchIcon(equippedPouchId(liveSave.equipment, statusUnit.name))}
          onClose={() => {
            setShowStatus(false);
            if (hud.inspected) engine.dismissInspect();
          }}
          onCycle={
            hud.turnQueue.length > 1
              ? (dir: 1 | -1) => {
                  const ids = hud.turnQueue.map((t) => t.id);
                  const i = ids.indexOf(statusUnit.id);
                  const nextId = ids[((i < 0 ? 0 : i) + dir + ids.length) % ids.length];
                  if (nextId) setBrowseId(nextId);
                }
              : undefined
          }
          onOpenInventory={
            statusUnit.side === "player"
              ? () => {
                  setShowStatus(false);
                  setInvView("pack");
                }
              : undefined
          }
          onOpenEquipment={
            statusUnit.side === "player"
              ? () => {
                  setShowStatus(false);
                  setInvView("doll");
                }
              : undefined
          }
        />
      )}

      {invView && statusUnit && statusUnit.side === "player" && (
        <PartyInventoryOverlay
          heroName={statusUnit.name}
          classId={statusUnit.classId}
          save={liveSave}
          test={playtest}
          onUseRation={onUseRation}
          onOpenStatus={(hero) => {
            const selected = engine.units.find((unit) => unit.name === hero && unit.side === "player");
            if (selected) setBrowseId(selected.id);
            setInvView(null);
            setShowStatus(true);
          }}
          initialView={invView === "pack" ? "backpack" : "equipment"}
          // Opened from the status panel, so closing goes back to it instead of dropping
          // straight to the battlefield — losing that context on the way out was the "no
          // way back" complaint.
          onClose={() => {
            setInvView(null);
            setShowStatus(true);
          }}
          // Gear changes mid-battle cost nothing — not the action, not the movement, and
          // they can repeat until the turn is passed. Each one writes through to the save
          // as well as the live unit, so a swap made in a fight is permanent whether the
          // battle is won, lost or retried.
          onEquipWeapon={(hero, weaponId) => {
            if (!weaponId) {
              if (!engine.equipWeaponOn(statusUnit.id, "", 0)) return;
              onEquipWeapon?.(hero, "", false);
              return;
            }
            const owned = save.weapons[weaponId] != null;
            const found = engine.lootWeapons.includes(weaponId);
            if (!owned && !found) return;
            if (!engine.equipWeaponOn(statusUnit.id, weaponId, save.weapons[weaponId] ?? 0)) return;
            if (!owned) engine.claimLoot("weapon", weaponId);
            onEquipWeapon?.(hero, weaponId, !owned);
          }}
          onEquipItem={(hero, slot, itemId) => {
            if (!itemId) {
              if (!engine.equipItemOn(statusUnit.id, slot, null)) return;
              onEquipItem?.(hero, slot, null, false);
              return;
            }
            const owned = (save.looseEquipment[itemId] ?? 0) > 0 || Object.values(save.equipment).some((slots) => Object.values(slots).includes(itemId));
            const found = engine.lootEquipment.includes(itemId);
            if (!owned && !found) return;
            if (!engine.equipItemOn(statusUnit.id, slot, itemId)) return;
            if (!owned) engine.claimLoot("equipment", itemId);
            onEquipItem?.(hero, slot, itemId, !owned);
          }}
          // Same aim-then-tap-a-target flow the action bar's own potion button already
          // uses — Usar here just arms it and drops back to the battlefield instead of
          // duplicating applyPotion's targeting logic. Only works for whichever unit is
          // actually mid-turn (engine.usePotion reads this.selectedId itself), same
          // restriction the action bar has always had; viewing another hero's Mochila
          // still shows the button, it just quietly does nothing if tapped.
          // Jogar Fora is intentionally left off mid-battle: permanently deleting party
          // gear/supplies is not something to expose during a fight already in progress.
          onUsePotion={(hero, kind) => {
            if (statusUnit.id !== engine.selectedId) return;
            engine.usePotion(kind);
            onHud(engine.getHud());
            setInvView(null);
            setShowStatus(false);
          }}
        />
      )}

      {pickerSlot != null && actor && (
        <SlotPicker
          classId={actor.classId}
          level={actor.level}
          heroName={actor.name}
          onPick={(action) => {
            setSlot(pickerSlot, action);
            setPickerSlot(null);
          }}
          onClose={() => setPickerSlot(null)}
        />
      )}
      {showCombatGuide && <HelpModal onClose={() => setShowCombatGuide(false)} />}
    </section>
  );
}

function SlotPicker({
  classId,
  level,
  heroName,
  onPick,
  onClose,
}: {
  classId: ClassId;
  level: number;
  heroName: string;
  onPick: (action: SlotAction | null) => void;
  onClose: () => void;
}) {
  const options: SlotAction[] = [
    ...classSpells(classId, level, heroName).map((spell): SlotAction => ({ kind: "spell", spell })),
    ...ALL_POTIONS.map((potion): SlotAction => ({ kind: "potion", potion })),
  ];
  return (
    <div
      className="absolute inset-0 z-50 ember-veil flex items-end sm:items-center justify-center p-4"
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div className="relative w-full max-w-sm ember-panel p-4">
        <div className="flex items-center justify-between mb-3">
          <p className="font-display text-lg ember-title">Escolher pra esse slot</p>
          <button type="button" onClick={onClose} className="size-8 grid place-items-center ember-icon-btn" aria-label="Fechar">
            <X className="size-4" />
          </button>
        </div>
        <div className="grid grid-cols-1 gap-1.5 max-h-[60dvh] overflow-y-auto">
          {options.map((action) => (
            <ItemTip
              key={action.kind === "potion" ? `p-${action.potion}` : `s-${action.spell}`}
              text={action.kind === "potion" ? potionTooltip(action.potion) : slotLabel(action)}
              className="block"
            >
              <button
                type="button"
                onClick={() => onPick(action)}
                className="w-full flex items-center gap-2 ember-slot px-2 py-2 text-left"
              >
                <img src={slotIcon(action)} alt="" className="size-6 rounded-sm object-cover shrink-0" />
                <span className="text-sm">{slotLabel(action)}</span>
              </button>
            </ItemTip>
          ))}
          <button
            type="button"
            onClick={() => onPick(null)}
            className="flex items-center gap-2 ember-slot px-2 py-2 text-left text-muted"
          >
            <span className="size-6 grid place-items-center shrink-0">—</span>
            <span className="text-sm">Deixar vazio</span>
          </button>
        </div>
      </div>
    </div>
  );
}

function flattenSkillRows(children: ReactNode): ReactElement<{ children?: ReactNode }>[] {
  if (Array.isArray(children)) return children.flatMap(flattenSkillRows);
  if (!isValidElement<{ children?: ReactNode }>(children)) return [];
  if (children.type === Fragment) return flattenSkillRows(children.props.children);
  return [children as ReactElement<{ children?: ReactNode }>];
}

function skillRowTier(node: ReactNode): number {
  const text = (child: ReactNode): string => {
    if (typeof child === "string" || typeof child === "number") return String(child);
    if (Array.isArray(child)) return child.map(text).join("");
    if (isValidElement<{ children?: ReactNode }>(child)) return text(child.props.children);
    return "";
  };
  const match = text(node).match(/Tier\s+(\d+)/i);
  return match ? Number(match[1]) : Number.POSITIVE_INFINITY;
}

function sortSkillRowsByTier(children: ReactNode): ReactElement[] {
  return flattenSkillRows(children)
    .map((element, index) => ({ element, index, tier: skillRowTier(element.props.children) }))
    .sort((a, b) => a.tier - b.tier || a.index - b.index)
    .map(({ element, index }) => cloneElement(element, { key: element.key ?? `skill-tier-row-${index}` }));
}

type CharacterCondition = {
  title: string;
  detail: string;
  icon: "healthy" | "poisoned" | "diseased" | "stunned" | "asleep" | "shocked" | "bleeding" | "hungry";
  tone: "ok" | "danger" | "warn";
};

function characterCondition(unit: UnitPublic): CharacterCondition {
  if (unit.stunned) {
    return {
      title: "Atordoado",
      detail: "Atordoamento · perde o próximo turno.",
      icon: "stunned",
      tone: "danger",
    };
  }
  if (unit.asleep) {
    return {
      title: "Dormindo",
      detail: "Sono mágico · perde turnos até acordar ou sofrer dano.",
      icon: "asleep",
      tone: "danger",
    };
  }
  if (unit.bleeding) {
    return {
      title: "Sangrando",
      detail: `Sangramento · sofre 1D8 de dano a cada ação: atacar, lançar magia, usar item, e ao se mover (uma vez por turno).${unit.bleedRoundsLeft != null ? ` Restam ${unit.bleedRoundsLeft} rodadas.` : ""}`,
      icon: "bleeding",
      tone: "danger",
    };
  }
  if (unit.shock) {
    return {
      title: "Eletrificado",
      detail: "Choque elétrico · o eco do relâmpago atinge este personagem no início do próximo turno.",
      icon: "shocked",
      tone: "danger",
    };
  }
  if (unit.poisoned) {
    return {
      title: `${POISON_TIERS[unit.poisonTier ?? "lesser"].name} · ${poisonDice(unit.poisonTier ?? "lesser")}`,
      detail: `${POISON_TIERS[unit.poisonTier ?? "lesser"].name} · sofre ${poisonDice(unit.poisonTier ?? "lesser")} de dano no início de cada turno. Use Curar Doença ou uma Poção de Curar Doenças para removê-lo.`,
      icon: "poisoned",
      tone: "danger",
    };
  }
  if (unit.diseased) {
    return {
      title: "Doente",
      detail: "Doença · todos os atributos ficam 10% menores até receber Curar Doença ou uma Poção de Curar Doenças.",
      icon: "diseased",
      tone: "danger",
    };
  }
  if (unit.hungry) {
    const pct = unit.hungerPct ?? 0;
    if (pct >= 90) {
      return {
        title: "Inconsciente",
        detail: `Fome extrema · desmaiado, praticamente inútil em combate (−${pct}% nos atributos). Só volta ao normal numa estalagem.`,
        icon: "hungry",
        tone: "danger",
      };
    }
    return {
      title: "Com fome",
      detail: `Fome · vários dias sem comer (−${pct}% nos atributos). Volta ao normal comendo ou numa estalagem.`,
      icon: "hungry",
      tone: "warn",
    };
  }
  if ((unit.blessedRoundsLeft ?? 0) > 0 && (unit.blessedHitBonusPct ?? 0) > 0) {
    return { title: "Abençoado", detail: `Bless · +${Math.round((unit.blessedHitBonusPct ?? 0) * 100)}% de chance de acerto por mais ${unit.blessedRoundsLeft} rodadas.`, icon: "healthy", tone: "ok" };
  }
  return {
    title: "Saudável",
    detail: "Saudável · não há veneno, doença ou fome afetando este personagem.",
    icon: "healthy",
    tone: "ok",
  };
}

/** The character sheet shows both the pre-calculated total for this unit's current MAG
 * and the underlying formula (dice included, since those stay random) — total alone hides
 * how it scales, formula alone makes the player do the math. This mirrors spellDamage's
 * two-stage MAG rounding. */
function damageFormula(mag: number, mul: number, dice: number, faces: number, bonus: number): string {
  const total = Math.floor(Math.floor(mag / 2) * mul);
  const magExpr = `⌊⌊MAG ÷ 2⌋ × ${mul}⌋`;
  const roll = diceFormula(dice, faces, bonus);
  return roll ? `${total} (${magExpr}) + ${roll}` : `${total} (${magExpr})`;
}

/** Which equipped pieces (if any) are boosting one core stat, and by how much — the status
 * sheet turns the stat blue and names them in a hover tooltip instead of just showing the
 * post-gear number with no explanation of where it came from. */
function gearContributors(gear: Partial<Record<EquipSlot, string>>, stat: "hp" | "atk" | "mag" | "def" | "dex" | "mov"): { total: number; lines: string[] } {
  let total = 0;
  const lines: string[] = [];
  for (const id of Object.values(gear)) {
    if (!id) continue;
    const item = EQUIPMENT[id];
    const amount = item?.[stat] ?? 0;
    if (!amount) continue;
    total += amount;
    lines.push(`${item!.name} ${amount > 0 ? "+" : ""}${amount}`);
  }
  return { total, lines };
}

function StatusPanel({ unit, accuracyTarget, statPointAllocation, unspentStatPoints, onAdjustStatPoint, bagIcon, onClose, onOpenInventory, onOpenEquipment, onCycle }: { unit: UnitPublic; accuracyTarget?: UnitPublic; statPointAllocation: StatPointAllocation; unspentStatPoints: number; onAdjustStatPoint?: (stat: StatPointAttribute, delta: 1 | -1) => boolean; bagIcon?: string; onClose: () => void; onOpenInventory?: () => void; onOpenEquipment?: () => void; /** Switches which unit the sheet shows — any living unit still in the fight, either side. */ onCycle?: (dir: 1 | -1) => void }) {
  const [showConditionDetail, setShowConditionDetail] = useState(false);
  const gearStat = (stat: "hp" | "atk" | "mag" | "def" | "dex" | "mov") => gearContributors(unit.gear, stat);
  // Fome docks VIT/ATK/MAG/DEF/DEX uniformly (see hungerKeep in mapStatusUnit/spawnUnit) —
  // flagged per stat here so the number itself reads as reduced, not just the condition badge.
  const stats: Array<{ label: string; value: string | number; stat?: StatPointAttribute; gear?: { total: number; lines: string[] }; penalized?: boolean }> = [
    { label: "VIT", value: unit.maxHp, stat: "hp", gear: gearStat("hp"), penalized: unit.hungry },
    { label: "ATK", value: unit.atk, stat: "atk", gear: gearStat("atk"), penalized: unit.hungry },
    { label: "MAG", value: unit.mag, stat: "mag", gear: gearStat("mag"), penalized: unit.hungry },
    { label: "DEF", value: unit.def, stat: "def", gear: gearStat("def"), penalized: unit.hungry },
    { label: "DEX", value: unit.dex, stat: "dex", gear: gearStat("dex"), penalized: unit.hungry },

    { label: "INI", value: unit.initiative },
    { label: "MOV", value: unit.movLeft < unit.mov ? `${unit.movLeft}/${unit.mov}` : unit.mov, gear: gearStat("mov") },
    { label: "Alcance", value: rangeLabel(unit.minRange, unit.maxRange) },
  ];
  // rulesClass also maps the heroes' own classes (voss, salazar, neera, kaelFinal) to their
  // job — PROMOTED_BASE alone left Voss, Salazar, Neera and Kael with no spell list.
  const base = rulesClass(unit.classId);
  const mage = base === "mage";
  const conjurer = base === "conjurer";
  const healer = base === "healer";
  const archer = base === "archer";
  const swordsman = base === "swordsman";
  const lancer = base === "lancer" || base === "aldric";
  const familiar1 = unit.classId === "familiar";
  const familiar2 = unit.classId === "familiar2";
  const familiar3 = unit.classId === "familiar3";
  const familiar4 = unit.classId === "familiar4";
  const paladin = unit.classId === "paladin";
  const heavyKnight = unit.classId === "heavyKnight";
  const spellStatusRow = (spell: SpellKind, label: string) => (
    <div key={spell} className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
      <img src={slotIcon({ kind: "spell", spell })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
      <p className="text-xs leading-snug min-w-0">{uiText(label)}</p>
      {/* Tier and uses sit outside the truncated label so a long label never hides them. */}
      <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier(spell)} · ×{unit.spells[tierKey(spellTier(spell)!)] ?? 0}</span>
    </div>
  );
  const condition = characterCondition(unit);

  return (
    <div
      className="absolute inset-0 z-40 ember-veil flex items-center justify-center p-4"
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div className="status-panel w-full overflow-y-auto ember-window rounded-xl p-6">
        <div className="flex items-start justify-between gap-3 mb-4">
          <div className="flex items-center gap-3 min-w-0">
            <div className="flex flex-col items-center gap-1 shrink-0">
              <div className="status-panel-portrait grid place-items-center overflow-hidden rounded-lg border border-border bg-black">
                {/* Every portrait is treated as a 512×768 (2:3) source, cropped centered to
                    that ratio before it ever fills the box. For a file already 512×768 this
                    crop is a no-op. Malrec's face portrait reads correctly at its natural
                    scale, so it fills the panel directly rather than being cropped again. */}
                {unit.sprite === "conjurer" || unit.sprite === "malrec" ? (
                  <img src={portraitFor(unit.sprite).src} alt="" style={{ objectPosition: portraitFor(unit.sprite).position }} className="h-full w-full object-cover" />
                ) : (
                  <span className="block w-full shrink-0" style={{ aspectRatio: "2 / 3" }}>
                    <img
                      src={portraitFor(unit.sprite).src}
                      alt=""
                      style={{ objectPosition: portraitFor(unit.sprite).position }}
                      className="h-full w-full object-cover"
                    />
                  </span>
                )}
              </div>
              {unit.side === "player" && <HungerBar name={unit.name} value={unit.fullness} />}
            </div>
            <div className="min-w-0">
              <p className="font-display text-xl leading-tight truncate ember-title">{uiText(unit.name)}</p>
              <p className={`text-xs ${unit.side === "enemy" ? "text-danger" : "text-muted"}`}>
                <span className="text-[13px]">{unit.className}</span> · Nv {unit.level}
              </p>
              {unit.side === "player" && (
                <div className="mt-1.5 max-w-[9rem]">
                  {unit.level >= MAX_LEVEL ? (
                    <p className="text-[11px] text-muted tabular-nums">Nível máximo</p>
                  ) : (
                    <>
                      <div className="h-1.5 rounded-full bg-border overflow-hidden">
                        <div className="h-full bg-accent" style={{ width: `${(unit.xp / expToLevel(unit.level)) * 100}%` }} />
                      </div>
                      <p className="text-[11px] text-muted tabular-nums mt-0.5">
                        {unit.xp}/{expToLevel(unit.level)} XP
                      </p>
                    </>
                  )}
                </div>
              )}
            </div>
          </div>
          <div className="flex flex-col items-end gap-2 shrink-0">
            <div className="flex items-center gap-1.5">
              {onCycle && (
                <>
                  <button type="button" onClick={() => onCycle(-1)} className="size-7 grid place-items-center ember-icon-btn" aria-label="Personagem anterior">
                    <ChevronUp className="size-3.5" />
                  </button>
                  <button type="button" onClick={() => onCycle(1)} className="size-7 grid place-items-center ember-icon-btn" aria-label="Próximo personagem">
                    <ChevronDown className="size-3.5" />
                  </button>
                </>
              )}
              <button type="button" onClick={onClose} className="size-7 grid place-items-center ember-icon-btn" aria-label="Fechar">
                <X className="size-3.5" />
              </button>
            </div>
            <div className="flex flex-col gap-1.5 w-full">
              <ItemTip text={condition.detail} className="block">
                <button
                  type="button"
                  onClick={() => setShowConditionDetail(true)}
                  aria-label={`Condição: ${condition.title}. Toque para ver detalhes.`}
                  className={`status-condition bg-black status-condition-${condition.tone} w-full text-left`}
                >
                  <span className={`status-condition-icon status-condition-icon-${condition.icon}`} aria-hidden="true" />
                  <span className="min-w-0">
                    <span className="block text-[10px] uppercase tracking-[0.16em] text-muted">Condição</span>
                    <span className="block text-xs font-medium truncate">{condition.title}</span>
                  </span>
                </button>
              </ItemTip>
              {onOpenInventory && (
                <button type="button" onClick={onOpenInventory} className="h-9 px-2 rounded-md border border-border bg-black text-xs flex items-center gap-1.5">
                  <img src={bagIcon ?? BAG_ICON} alt="" className="size-6 shrink-0 rounded-sm bg-black object-contain" />
                  Mochila
                </button>
              )}
              {onOpenEquipment && (
                <button type="button" onClick={onOpenEquipment} className="h-9 px-2 rounded-md border border-border bg-black text-xs flex items-center gap-1.5">
                  <img src="/game/icons/equipment-dark-001.png" alt="" className="size-6 shrink-0 bg-black object-contain" />
                  Equipar
                </button>
              )}
            </div>
          </div>
        </div>

        <div className="mt-2 mb-4">
          <div className="flex items-center gap-2">
            <div className="h-2 flex-1 rounded-full bg-border overflow-hidden">
              <div
                className={`h-full ${unit.side === "enemy" ? "bg-danger" : "bg-accent"}`}
                style={{ width: `${Math.max(0, (unit.hp / unit.maxHp) * 100)}%` }}
              />
            </div>
            <p className="text-xs tabular-nums text-fg shrink-0">
              {unit.hp}/{unit.maxHp}
            </p>
          </div>
        </div>

        {unit.side === "player" && onAdjustStatPoint && (
          <div className="mb-4 flex items-center gap-3 rounded-lg border border-accent/40 bg-bg px-3 py-2.5">
            <img src="/game/icons/stat-points-001.png" alt="" className="size-10 shrink-0 object-contain" />
            <div className="min-w-0">
              <p className="text-xs ember-kicker">Pontos de atributo</p>
              <p className="font-display text-lg leading-tight tabular-nums">{unspentStatPoints} disponível{unspentStatPoints === 1 ? "" : "is"}</p>
              <p className="text-[11px] text-muted">Ganhe {STAT_POINTS_PER_LEVEL} por nível e distribua como quiser.</p>
            </div>
          </div>
        )}

        <p className="text-xs ember-kicker mb-2">Atributos</p>
        <div className="grid grid-cols-4 gap-1.5 mb-4">
          {stats.map(({ label, value, stat, gear, penalized }) => (
            <div key={label} className="bg-bg border border-border rounded-md px-1 py-1 text-center">
              <p className="text-[9px] uppercase tracking-wide text-muted">{label}</p>
              {penalized ? (
                <ItemTip text={condition.detail} className="block">
                  <p className="text-xs font-medium tabular-nums text-danger">{value}</p>
                </ItemTip>
              ) : gear && gear.total !== 0 ? (
                <ItemTip text={`Bônus de equipamento:\n${gear.lines.join("\n")}`} className="block">
                  <p className="text-xs font-medium tabular-nums text-sky-300">{value}</p>
                </ItemTip>
              ) : (
                <p className="text-xs font-medium tabular-nums">{value}</p>
              )}
              {stat && onAdjustStatPoint ? (
                <div className="mt-0.5 flex items-center justify-center gap-0.5">
                  <button
                    type="button"
                    aria-label={`Remover um ponto de ${label}`}
                    disabled={(statPointAllocation[stat] ?? 0) <= 0}
                    onClick={() => onAdjustStatPoint(stat, -1)}
                    className="size-5 grid place-items-center ember-icon-btn text-xs leading-none disabled:opacity-35"
                  >
                    −
                  </button>
                  <span className="min-w-5 text-[10px] tabular-nums text-accent">+{statPointAllocation[stat] ?? 0}</span>
                  <button
                    type="button"
                    aria-label={`Adicionar um ponto em ${label}`}
                    disabled={unspentStatPoints <= 0}
                    onClick={() => onAdjustStatPoint(stat, 1)}
                    className="size-5 grid place-items-center ember-icon-btn text-xs leading-none disabled:opacity-35"
                  >
                    +
                  </button>
                </div>
              ) : null}
            </div>
          ))}
        </div>

        {unit.side === "player" && (swordsman || mage || conjurer || archer || healer || lancer || familiar1 || familiar2 || familiar3 || familiar4 || paladin || heavyKnight) && (
          <>
            <p className="text-xs ember-kicker mb-2">Magias e habilidades</p>
            <div className="grid grid-cols-1 gap-1.5">
              {sortSkillRowsByTier(
                <>
                  {swordsman && (
                    <>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={slotIcon({ kind: "spell", spell: "doubleStrike" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(DOUBLE_STRIKE.name)} {doubleStrikeFormula(unit.level)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("doubleStrike")} · ×{unit.spells[tierKey(spellTier("doubleStrike")!)]}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={slotIcon({ kind: "spell", spell: "cleave" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(CLEAVE.name)} {CLEAVE.hexes} hex, {cleaveFormula(unit.level)} · x{CLEAVE.largeMul} vs 3+ hex
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("cleave")} · ×{unit.spells[tierKey(spellTier("cleave")!)]}</span>
                      </div>
                      {spellStatusRow("bullRush", BULL_RUSH.name)}
                      {spellStatusRow("shieldBash", SHIELD_BASH.name)}
                      {spellStatusRow("executionerStrike", EXECUTIONER_STRIKE.name)}
                      {unit.level >= PROVOKE.unlockLevel && spellStatusRow("provoke", `${PROVOKE.name} ${provokeFormula(unit.level)}`)}
                    </>
                  )}
                  {unit.side === "player" && !unit.summoned && (
                    <p className="text-xs text-muted">Healing: {(unit.healingSkill ?? 0).toLocaleString("pt-BR", { maximumFractionDigits: 2 })}/100 · +{(unit.healingSkill ?? 0).toLocaleString("pt-BR", { maximumFractionDigits: 2 })}% healing</p>
                  )}
                  {mage && (
                    <>
                      {unit.level >= FROST.unlockLevel && spellStatusRow("frost", `${FROST.name} · ${frostPower(unit.level).dice}D${FROST.faces} · linha de ${frostPower(unit.level).length} hexes`)}
                      {unit.level >= POISON_BREATH.unlockLevel && spellStatusRow("poisonBreath", `${POISON_BREATH.name} · ${poisonBreathFormula(unit.level, unit.mag)} · raio ${poisonBreathPower(unit.level).radius} · Veneno Menor 1D4/turno`)}
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={slotIcon({ kind: "spell", spell: "magicMissile" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(MAGIC_MISSILE.name)} {damageFormula(unit.mag, MAGIC_MISSILE.mul, MAGIC_MISSILE.dice, MAGIC_MISSILE.faces, MAGIC_MISSILE.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("magicMissile")} · ×{unit.spells[tierKey(spellTier("magicMissile")!)]}</span>
                      </div>
                      {unit.spells[tierKey(spellTier("lightning")!)] > 0 && (
                        <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                          <img src={spellIcon("lightning")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                          <p className="text-xs leading-snug min-w-0">
                            Raio {damageFormula(unit.mag, LIGHTNING.mul, LIGHTNING.dice, LIGHTNING.faces, LIGHTNING.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("lightning")} · ×{unit.spells[tierKey(spellTier("lightning")!)]}</span>
                        </div>
                      )}
                      {unit.spells[tierKey(spellTier("fireball")!)] > 0 && (
                        <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                          <img src={spellIcon("fireball")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                          <p className="text-xs leading-snug min-w-0">
                            Fogo {damageFormula(unit.mag, FIREBALL.mul, FIREBALL.dice, FIREBALL.faces, FIREBALL.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("fireball")} · ×{unit.spells[tierKey(spellTier("fireball")!)]}</span>
                        </div>
                      )}
                      {unit.spells[tierKey(spellTier("causticVenom")!)] > 0 && (
                        <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                          <img src={spellIcon("caustic-venom")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                          <p className="text-xs leading-snug min-w-0">
                            {uiText(unit.classId === "undeadOx" || unit.classId === "plagueBearingCattle" ? MINOR_VENOM.name : CAUSTIC_VENOM.name)} {damageFormula(unit.mag, CAUSTIC_VENOM.centerMul, CAUSTIC_VENOM.centerDice, CAUSTIC_VENOM.centerFaces, CAUSTIC_VENOM.centerBonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("causticVenom")} · ×{unit.spells[tierKey(spellTier("causticVenom")!)]}</span>
                        </div>
                      )}
                      {unit.classId === "elementalist" && unit.spells[tierKey(spellTier("lightningTier3")!)] > 0 && (
                        <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                          <img src={spellIcon("lightning")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                          <p className="text-xs leading-snug min-w-0">
                            {LIGHTNING_T3.name} {damageFormula(unit.mag, LIGHTNING_T3.mul, LIGHTNING_T3.dice, LIGHTNING_T3.faces, LIGHTNING_T3.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("lightningTier3")} · ×{unit.spells[tierKey(spellTier("lightningTier3")!)]}</span>
                        </div>
                      )}
                    </>
                  )}
                  {conjurer && (
                    <>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={slotIcon({ kind: "spell", spell: "summonFamiliar" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(SUMMON_FAMILIAR.name)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("summonFamiliar")} · ×{unit.spells[tierKey(spellTier("summonFamiliar")!)]}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={slotIcon({ kind: "spell", spell: "phantasmalForce" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(PHANTASMAL_FORCE.name)} {phantasmalForceFormula(unit.level, unit.mag)}
                          {unit.level < PHANTASMAL_FORCE_UNLOCK_LEVEL && <span className="text-muted"> · nível {PHANTASMAL_FORCE_UNLOCK_LEVEL}+</span>}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("phantasmalForce")} · ×{unit.level >= PHANTASMAL_FORCE_UNLOCK_LEVEL ? unit.spells[tierKey(spellTier("phantasmalForce")!)] : 0}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("web-of-dreams")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(WEB_OF_DREAMS.name)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("webOfDreams")} · ×{unit.spells[tierKey(spellTier("webOfDreams")!)]}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={slotIcon({ kind: "spell", spell: "summonFamiliar2" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {SUMMON_FAMILIAR2.name}
                          {unit.level < SUMMON_FAMILIAR2_UNLOCK_LEVEL && <span className="text-muted"> · nível {SUMMON_FAMILIAR2_UNLOCK_LEVEL}+</span>}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("summonFamiliar2")} · ×{unit.level >= SUMMON_FAMILIAR2_UNLOCK_LEVEL ? unit.spells[tierKey(spellTier("summonFamiliar2")!)] : 0}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("summon-familiar4")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {SUMMON_FAMILIAR4.name}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("summonFamiliar4")} · ×{unit.spells[tierKey(spellTier("summonFamiliar4")!)]}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("summon-familiar3")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {SUMMON_FAMILIAR3.name}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("summonFamiliar3")} · ×{unit.spells[tierKey(spellTier("summonFamiliar3")!)]}</span>
                      </div>
                      {spellStatusRow("summonZombieDog", SUMMON_ZOMBIE_DOG.name)}
                    </>
                  )}
                  {(familiar1 || familiar2) && (
                    <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                      <img src={spellIcon("magic-missile")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                      <p className="text-xs leading-snug min-w-0">
                        {uiText(MAGIC_MISSILE.name)} {damageFormula(unit.mag, MAGIC_MISSILE.mul, MAGIC_MISSILE.dice, MAGIC_MISSILE.faces, MAGIC_MISSILE.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("magicMissile")} · ×{unit.spellCharges ?? 0}</span>
                    </div>
                  )}
                  {familiar4 && (
                    <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                      <img src={slotIcon({ kind: "spell", spell: "shock" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                      <p className="text-xs leading-snug min-w-0">
                        {uiText(SHOCK.name)} {damageFormula(unit.mag, SHOCK.mul, SHOCK.dice, SHOCK.faces, SHOCK.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("shock")} · ×{unit.spellCharges ?? 0}</span>
                    </div>
                  )}
                  {(familiar2 || familiar4) && (
                    <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                      <img src={slotIcon({ kind: "spell", spell: "lifeDrain" })} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                      <p className="text-xs leading-snug min-w-0">
                        {uiText(LIFE_DRAIN.name)} {lifeDrainFormula(unit.level, unit.mag)} · cura {Math.round(lifeDrainHealMul(unit.level) * 100)}% do dano
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("lifeDrain")} · ×{unit.lifeDrainCharges ?? 0}</span>
                    </div>
                  )}
                  {familiar3 && (
                    <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                      <img src={spellIcon("fireball")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                      <p className="text-xs leading-snug min-w-0">
                        Fogo {damageFormula(unit.mag, FIREBALL.mul, FIREBALL.dice, FIREBALL.faces, FIREBALL.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("fireball")} · ×{unit.spellCharges ?? 0}</span>
                    </div>
                  )}
                  {archer && (
                    <>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("long-shot")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(LONG_SHOT.name)} {longShotFormula(unit.level)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("longShot")} · ×{unit.spells[tierKey(spellTier("longShot")!)]}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("piercing")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(PIERCING.name)} {piercingMul(unit.level)}× dano de arma
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("piercing")} · ×{unit.spells[tierKey(spellTier("piercing")!)]}</span>
                      </div>
                    </>
                  )}
                  {healer && (
                    <>
                      {spellStatusRow("bless", BLESS.name)}
                      {spellStatusRow("burningHands", BURNING_HANDS.name)}
                      {spellStatusRow("turnUndead", TURN_UNDEAD.name)}
                      {spellStatusRow("createFoodAndWater", CREATE_FOOD_AND_WATER.name)}
                      {unit.name === "Salazar" && spellStatusRow("divineBolt", DIVINE_BOLT.name)}
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("cure-minor")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(CURES.cureMinor.name)} {damageFormula(unit.mag, CURES.cureMinor.mul, CURES.cureMinor.dice, CURES.cureMinor.faces, CURES.cureMinor.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("cureMinor")} · ×{unit.spells[tierKey(spellTier("cureMinor")!)]}</span>
                      </div>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("cure-wounds")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(CURES.cureWounds.name)} {damageFormula(unit.mag, CURES.cureWounds.mul, CURES.cureWounds.dice, CURES.cureWounds.faces, CURES.cureWounds.bonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("cureWounds")} · ×{unit.spells[tierKey(spellTier("cureWounds")!)]}</span>
                      </div>
                      {unit.spells[tierKey(spellTier("cureDisease")!)] > 0 && (
                        <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                          <img src={spellIcon("cure-disease")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                          <p className="text-xs leading-snug min-w-0">
                            {uiText(CURE_DISEASE.name)} · remove doença e veneno
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("cureDisease")} · ×{unit.spells[tierKey(spellTier("cureDisease")!)]}</span>
                        </div>
                      )}
                    </>
                  )}
                  {lancer && (
                    <>
                      <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                        <img src={spellIcon("piercing-thrust")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                        <p className="text-xs leading-snug min-w-0">
                          {uiText(PIERCING_THRUST.name)} dano de arma, −{Math.round(PIERCING_THRUST.armorIgnore * 100)}% armadura
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("piercingThrust")} · ×{unit.spells[tierKey(spellTier("piercingThrust")!)]}</span>
                      </div>
                      {unit.spells[tierKey(spellTier("sweep")!)] > 0 && (
                        <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                          <img src={spellIcon("sweep")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                          <p className="text-xs leading-snug min-w-0">
                            {uiText(SWEEP.name)} dano de arma
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("sweep")} · ×{unit.spells[tierKey(spellTier("sweep")!)]}</span>
                        </div>
                      )}
                      {unit.spells[tierKey(spellTier("trip")!)] > 0 && (
                        <div className="flex items-center gap-1.5 bg-bg border border-border rounded-md px-2 py-1.5">
                          <img src={spellIcon("trip")} alt="" className="size-5 rounded-sm object-cover shrink-0" />
                          <p className="text-xs leading-snug min-w-0">
                            {uiText(TRIP.name)} arma +{diceFormula(1, TRIP.bonusFaces, TRIP.bonusBonus)}
                        </p>
                        <span className="ml-auto shrink-0 text-xs tabular-nums text-muted">Tier {spellTier("trip")} · ×{unit.spells[tierKey(spellTier("trip")!)]}</span>
                        </div>
                      )}
                    </>
                  )}
                  {paladin && (
                    <>
                      {spellStatusRow("cureLight", CURES.cureLight.name)}
                      {spellStatusRow("auraOfProtection", AURA_OF_PROTECTION.name)}
                      {spellStatusRow("divineWrath", DIVINE_WRATH.name)}
                    </>
                  )}
                  {heavyKnight && (
                    <>
                      {spellStatusRow("shoulderSmash", SHOULDER_SMASH.name)}
                      {spellStatusRow("intimidatingPresence", INTIMIDATING_PRESENCE.name)}
                      {spellStatusRow("stampede", STAMPEDE.name)}
                    </>
                  )}
                </>,
              )}
            </div>
          </>
        )}

        {unit.side === "player" && (
          <>
            <p className="text-xs ember-kicker mb-2 mt-5">Poções</p>
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-1.5">
              {ALL_POTIONS.filter((kind) => (unit.bag[kind] ?? 0) > 0).map((kind) => (
                <ItemTip key={kind} text={potionTooltip(kind)} className="block">
                  <div className="flex items-center gap-1.5 rounded-md border border-border bg-bg px-2 py-1.5">
                    <img src={`/game/icons/potion-${kind}.png?v=ds2`} alt="" className="size-6 shrink-0 object-contain" />
                    <p className="min-w-0 flex-1 truncate text-xs">
                      {potionLabel(kind)}
                      <span className="block text-[10px] tabular-nums text-muted">{unit.bag[kind]}/{POTION_CARRY_MAX[kind]}</span>
                    </p>
                  </div>
                </ItemTip>
              ))}
              {unit.bag.lockpick > 0 && (
                <ItemTip text={lockpickTooltip()} className="block">
                  <div className="flex items-center gap-1.5 rounded-md border border-border bg-bg px-2 py-1.5">
                    <img src="/game/icons/lockpick.png" alt="" className="size-6 shrink-0 object-contain" />
                    <p className="min-w-0 flex-1 truncate text-xs">
                      Gazua
                      <span className="block text-[10px] tabular-nums text-muted">{unit.bag.lockpick}</span>
                    </p>
                  </div>
                </ItemTip>
              )}
              {ALL_POTIONS.every((kind) => (unit.bag[kind] ?? 0) <= 0) && unit.bag.lockpick <= 0 && (
                <p className="col-span-full text-xs text-muted">Sem poções carregadas.</p>
              )}
            </div>
          </>
        )}
      </div>
      {showConditionDetail && (
        <div
          className="fixed inset-0 z-50 grid place-items-center ember-veil p-4"
          onClick={(e) => {
            if (e.target === e.currentTarget) setShowConditionDetail(false);
          }}
        >
          <div className="w-full max-w-xs ember-window rounded-xl p-5 text-center">
            <div className="flex items-start justify-end">
              <button type="button" onClick={() => setShowConditionDetail(false)} className="size-8 grid place-items-center ember-icon-btn" aria-label="Fechar">
                <X className="size-4" />
              </button>
            </div>
            <div className="flex justify-center -mt-4 mb-3">
              <span className={`status-condition-icon-lg status-condition-icon-${condition.icon}`} aria-hidden="true" />
            </div>
            <p className={`text-sm font-display mb-2 ${condition.tone === "danger" ? "text-danger" : condition.tone === "warn" ? "text-[#c99a5c]" : "text-ok"}`}>
              {condition.title}
            </p>
            <p className="text-xs text-muted leading-relaxed">{condition.detail}</p>
          </div>
        </div>
      )}
    </div>
  );
}

/** XP bar on the post-mission screen: starts at the hero's pre-battle progress and plays the
 * real gain — fill to full, flash, refill from empty for each level gained, then ease to the
 * post-battle value. Driven by the Web Animations API, one segment at a time, so it never
 * animates backwards or skips the fill. */
function GrowthXpBar({ fromLevel, toLevel, fromXp, toXp }: { fromLevel: number; toLevel: number; fromXp: number; toXp: number }) {
  const fill = useRef<HTMLSpanElement>(null);
  const flash = useRef<HTMLSpanElement>(null);
  // The real path: the old XP fills to full, every level gained refills from empty, and the
  // last segment ends at the new XP. Each refill restarts at 0 instantly — never slides back.
  const segments = useMemo(() => {
    const share = (xp: number, level: number) => Math.max(0, Math.min(1, xp / expToLevel(level)));
    if (toLevel <= fromLevel) return [{ start: share(fromXp, fromLevel), end: share(toXp, toLevel) }];
    return [
      { start: share(fromXp, fromLevel), end: 1 },
      ...Array.from({ length: toLevel - fromLevel - 1 }, () => ({ start: 0, end: 1 })),
      { start: 0, end: share(toXp, toLevel) },
    ];
  }, [fromLevel, toLevel, fromXp, toXp]);
  const pct = (share: number) => `${share * 100}%`;
  useEffect(() => {
    const el = fill.current;
    if (!el) return;
    const last = segments[segments.length - 1]!;
    if (typeof el.animate !== "function" || window.matchMedia?.("(prefers-reduced-motion: reduce)").matches) {
      el.style.width = pct(last.end);
      return;
    }
    let cancelled = false;
    let running: Animation | null = null;
    const wait = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));
    void (async () => {
      el.style.width = pct(segments[0]!.start);
      await wait(350);
      for (let i = 0; i < segments.length && !cancelled; i++) {
        const { start, end } = segments[i]!;
        el.style.width = pct(start);
        running = el.animate([{ width: pct(start) }, { width: pct(end) }], {
          duration: Math.max(380, 1200 * (end - start)),
          easing: i === segments.length - 1 ? "cubic-bezier(0.2, 0.7, 0.2, 1)" : "cubic-bezier(0.45, 0, 0.55, 1)",
          fill: "forwards",
        });
        await running.finished.catch(() => undefined);
        if (cancelled) return;
        el.style.width = pct(end);
        running.cancel();
        if (i < segments.length - 1) {
          flash.current?.animate([{ opacity: 0 }, { opacity: 1, offset: 0.25 }, { opacity: 0 }], { duration: 520, easing: "ease-out" });
          await wait(300);
        }
      }
    })();
    return () => {
      cancelled = true;
      running?.cancel();
    };
  }, [segments]);
  return (
    <span className="relative h-2.5 w-32 shrink-0 overflow-hidden ember-socket" style={{ borderRadius: 9999 }}>
      <span
        ref={fill}
        className="absolute inset-y-0 left-0 overflow-hidden"
        style={{
          width: pct(segments[0]!.start),
          borderRadius: 9999,
          background: "linear-gradient(90deg, #444444 0%, #c8641e 55%, #e1a541 85%, #fff0a2 100%)",
          boxShadow: "0 0 6px rgba(255, 140, 50, 0.55)",
        }}
      >
        <span aria-hidden="true" className="absolute inset-x-0 top-0 h-1/2 bg-white/15" />
        <span aria-hidden="true" className="absolute inset-y-0 right-0 w-1.5 bg-[#fff0a2] blur-[2px]" />
      </span>
      <span
        ref={flash}
        aria-hidden="true"
        className="pointer-events-none absolute inset-0 opacity-0"
        style={{ borderRadius: 9999, background: "rgba(255, 220, 150, 0.45)", boxShadow: "inset 0 0 8px #fff0a2, 0 0 10px #ffb347" }}
      />
    </span>
  );
}

function ResultScreen({
  win,
  title,
  body,
  turn,
  growth,
  art,
  onNext,
  onInn,
  onMap,
  mapLabel,
  hasNext,
  innOpen,
  retry,
  loot,
  advanceLabel,
  onAdvance,
  resting = true,
}: {
  win: boolean;
  title: string;
  body: string;
  turn: number;
  growth: GrowthLine[] | null;
  art: string | null;
  onNext: () => void;
  onInn?: () => void;
  onMap?: () => void;
  mapLabel?: string;
  hasNext: boolean;
  innOpen?: boolean;
  retry?: boolean;
  loot?: string[];
  /** Transversal Dungeon floor connector only: "Avançar"/"Voltar" straight into the linked
   * floor, shown above the usual Mapa/Estalagem buttons instead of the disabled hasNext path
   * (see its own comment at the victory ResultScreen call site — this is a different, specific
   * destination, not the "next mission by index" hasNext was turned off for). */
  advanceLabel?: string;
  onAdvance?: () => void;
  /** Floor connectors continue the same expedition and do not grant camp recovery. */
  resting?: boolean;
}) {
  return (
    <section className="relative h-dvh min-h-0 flex flex-col overflow-hidden bg-bg">
      {art && (
        <>
          <img src={art} alt="" className="absolute inset-0 h-full w-full object-cover" />
          <div className="absolute inset-0 bg-gradient-to-t from-bg/90 via-bg/40 to-bg/20" />
        </>
      )}
      <div className="relative z-10 flex-1 min-h-0 overflow-y-auto px-4 pt-[max(2rem,env(safe-area-inset-top))] pb-4">
        <div style={{ zoom: 0.75 }}>
        <p className={`text-sm ember-kicker${win ? "" : " is-defeat"}`}>
          <span style={{ zoom: 4 / 3 }}>{win ? "Vitória" : "Derrota"} · T{turn}</span>
        </p>
        <h1 className="font-display text-4xl sm:text-5xl mt-2 mb-2 ember-title"><span style={{ zoom: 4 / 3 }}>{title}</span></h1>
        <p className="text-lg text-muted mb-6">{body}</p>
        {loot && loot.length > 0 && <p className="text-sm text-accent mb-4">Achado no campo: {loot.join(", ")}</p>}
        {growth && growth.length > 0 && (
          <ul className="mb-6 space-y-2 max-w-[34rem] w-full">
            {growth.map((g) => (
              <li key={g.name} className="ember-slot px-3 py-2.5">
                <p className="font-medium text-lg">
                  {g.name}
                  {g.to !== g.from ? ` · Nv ${g.from} → ${g.to}` : ` · Nv ${g.from}`}
                  {g.fallen ? " · caiu" : ""}
                </p>
                {g.to < MAX_LEVEL ? (
                  <p className="flex items-center gap-2 mt-1 text-sm">
                    <GrowthXpBar fromLevel={g.from} toLevel={g.to} fromXp={g.xpFrom} toXp={g.xp} />
                    <span className="text-muted tabular-nums">
                      {g.xp}/{expToLevel(g.to)} XP{g.to !== g.from ? " · subiu" : ""}
                    </span>
                  </p>
                ) : (
                  g.to !== g.from && <p className="mt-1 text-sm text-accent">Nível máximo · subiu</p>
                )}
                <p className="text-sm text-muted tabular-nums mt-1">Combate: {g.hpBattle}/{g.maxFrom}</p>
                {!resting ? (
                  <p className="text-sm text-muted tabular-nums">{g.fallen ? "Fora do próximo andar" : `Próximo andar: ${g.hpCamp} HP · sem descanso`}</p>
                ) : g.fallen ? (
                  <p className="text-sm text-muted tabular-nums">Descanso: revive com {g.hpCamp} HP (metade de {g.maxTo})</p>
                ) : (
                  <p className="text-sm tabular-nums text-fg/90">
                    Descanso: {g.restHp > 0 ? `+${g.restHp} HP` : "sem feridas"}
                    <span className="text-muted"> · metade do que faltava</span>
                  </p>
                )}
                {g.to !== g.from && (
                  <p className="text-sm tabular-nums text-accent">
                    Nível: +{g.levelHp} HP máximo ({g.maxFrom} → {g.maxTo})
                    {g.atkTo !== g.atkFrom ? ` · AT ${g.atkFrom} → ${g.atkTo}` : ""}
                    {g.magTo !== g.magFrom ? ` · MAG ${g.magFrom} → ${g.magTo}` : ""}
                    {g.defTo !== g.defFrom ? ` · DF ${g.defFrom} → ${g.defTo}` : ""}
                    {g.dexTo !== g.dexFrom ? ` · DEX ${g.dexFrom} → ${g.dexTo}` : ""}
                  </p>
                )}
                {g.skillGain ? (
                  <p className="text-sm tabular-nums text-accent">Magias: {g.skillGain} (usos novos deste nível)</p>
                ) : g.to !== g.from ? (
                  <p className="text-sm text-muted">Magias: este nível não adicionou usos — cargas gastas não voltam</p>
                ) : null}
                {resting && <p className="text-base tabular-nums mt-1">Acampamento: {g.hpCamp}/{g.maxTo}</p>}
              </li>
            ))}
          </ul>
        )}
        </div>
      </div>
      <div className="relative z-10 p-4 pb-[max(1rem,env(safe-area-inset-bottom))] flex flex-col gap-2 max-w-xl w-full" style={{ zoom: 0.75 }}>
        {onAdvance && (
          <Button size="xl" className="w-full ember-btn ember-btn-primary" onClick={onAdvance}>
            {advanceLabel ?? "Avançar"}
          </Button>
        )}
        {hasNext && (
          <Button size="xl" className="w-full ember-btn ember-btn-primary" onClick={onNext}>
            {retry ? (
              <>
                <RotateCcw className="size-5" /> Tentar de novo
              </>
            ) : (
              "Próxima missão"
            )}
          </Button>
        )}
        {win && innOpen && onInn && (
          <Button variant="quiet" className="w-full ember-btn ember-btn-ghost inn-open" onClick={onInn}>
            Estalagem do Osso Seco
          </Button>
        )}
        {(win || mapLabel) && onMap && (
          <Button variant="ghost" className="w-full ember-btn ember-btn-ghost" onClick={onMap}>
            {mapLabel ?? "Cenários"}
          </Button>
        )}

      </div>
    </section>
  );
}

function PromotionScreen({
  pending,
  onPick,
}: {
  pending: { name: string; options: [ClassId, ClassId] }[];
  onPick: (name: string, classId: ClassId) => void;
}) {
  const current = pending[0];
  if (!current) return null;
  return (
    <div className="absolute inset-0 z-50 bg-bg/90 flex items-end sm:items-center justify-center p-4">
      <div className="w-full max-w-md ember-window rounded-xl p-5 max-h-[90dvh] overflow-y-auto">
        <p className="text-xs uppercase tracking-[0.18em] text-muted">Nível {PROMOTE_LEVEL}</p>
        <h2 className="font-display text-2xl leading-none mt-1 mb-2">{uiText(current.name)} pode se promover</h2>
        <p className="text-sm text-muted mb-4">
          Escolha um caminho. {current.name} não perde as magias que já tem — as novas se somam a partir de agora.
        </p>
        <div className="flex flex-col gap-2">
          {current.options.map((classId) => {
            const cls = CLASSES[classId];
            return (
              <button
                key={classId}
                type="button"
                onClick={() => onPick(current.name, classId)}
                className="w-full text-left rounded-xl border border-border bg-bg/40 px-4 py-3 hover:border-accent"
              >
                <p className="font-display text-xl leading-tight">{uiText(cls.name)}</p>
                <p className="text-sm text-muted">{sheetLine(statsFor(classId, PROMOTE_LEVEL))}</p>
              </button>
            );
          })}
        </div>
      </div>
    </div>
  );
}

/** Onboarding popup shown once, the first time the RPG overworld map screen itself opens —
 * gated purely on the seenOverworldIntro flag, independent of whichever mission just ended.
 * Covers both how to move on the map and what the hunger bar means, since the very next
 * click the player makes here is the one that moves the party for the first time. */
function OverworldIntroScreen({ onClose }: { onClose: () => void }) {
  return (
    <div className="absolute inset-0 z-50 bg-bg/90 flex items-end sm:items-center justify-center p-4">
      <div className="w-full max-w-md ember-window rounded-xl p-5 max-h-[90dvh] overflow-y-auto">
        <p className="text-xs uppercase tracking-[0.18em] text-muted">Sistema</p>
        <h2 className="font-display text-2xl leading-none mt-1 mb-2">Movimento e fome</h2>
        <p className="text-sm text-muted mb-3">
          Clique no personagem no mapa para ver os hexágonos que o grupo pode alcançar e escolher para onde ir. Cada
          passo custa um dia.
        </p>
        <p className="text-sm text-muted mb-3">
          Cada herói tem uma barra de saciedade. Ela desce ao longo da marcha pelo mapa e um pouco a cada ação em
          combate. Rações (compradas na Estalagem ou achadas em batalha) e refeições na Estalagem enchem essa barra
          de volta.
        </p>
        <p className="text-sm text-muted mb-3">
          Se ela chegar a zero e o grupo continuar sem comer, começa o status de <strong className="text-fg">Fome</strong>:
          uma penalidade de <strong className="text-fg">−10% em todos os atributos</strong>, que piora a cada dia
          faminto até um teto de −90%.
        </p>
        <p className="text-sm text-muted mb-4">Fique de olho na barra e mantenha rações na mochila antes de partir.</p>
        <Button onClick={onClose}>Entendi</Button>
      </div>
    </div>
  );
}

function SlotScreen({
  mode,
  bank,
  overwrite,
  onOverwrite,
  onClose,
  onPick,
  saveProgress = null,
}: {
  mode: "new" | "continue" | "save" | "load";
  bank: SaveBank;
  overwrite: number | null;
  onOverwrite: (i: number | null) => void;
  onClose: () => void;
  onPick: (index: number) => void;
  /** Real save steps done (0-100) while a slot is being written; null when idle. */
  saveProgress?: number | null;
}) {
  const title = mode === "new" ? "Nova campanha" : mode === "save" ? "Salvar jogo" : "Carregar jogo";
  const hint =
    mode === "new"
      ? "Escolha o slot. Um slot ocupado será substituído."
      : mode === "continue" || mode === "load"
        ? "O último usado vem marcado. Escolha um jogo para continuar."
        : "Escolha onde gravar este combate. Um slot ocupado será substituído.";

  return (
    <section className="absolute inset-0 z-40 flex min-h-0 flex-col overflow-hidden bg-[#080a0d] text-fg">
      <img src="/game/ui/travel-board.png" alt="" className="absolute inset-0 size-full object-cover object-center" />
      <div className="absolute inset-0 bg-black/20" aria-hidden="true" />
      <header className="relative z-10 flex shrink-0 items-center gap-3 border-b border-white/10 bg-black/20 px-4 py-3 pt-[max(0.75rem,env(safe-area-inset-top))] sm:px-7">
        <button type="button" onClick={onClose} className="h-9 px-3 shrink-0 flex items-center gap-1.5 ember-plate text-xs" aria-label="Voltar">
          <ChevronLeft className="size-4" /> Voltar
        </button>
        <div className="min-w-0 flex-1">
          <p className="text-xs ember-kicker">Arquivos</p>
          <h1 className="mt-1 font-display text-2xl leading-none ember-title sm:text-3xl">{title}</h1>
          <p className="mt-1 text-xs text-slate-300 sm:text-sm">{hint}</p>
        </div>
      </header>
      <main className="relative z-10 grid flex-1 min-h-0 place-items-center px-3 py-3 sm:px-6 sm:py-5">
        <ol className="grid w-full max-w-5xl grid-cols-2 gap-2 md:grid-cols-3 sm:gap-3">
          {Array.from({ length: SLOT_COUNT }, (_, i) => {
            const slot = bank.slots[i] ?? null;
            const empty = isSlotEmpty(slot);
            const last = i === bank.lastSlot && hasAnySave(bank) && !empty;
            const info = slotProgress(slot);
            const disabled = (mode === "continue" || mode === "load") && empty;
            const confirm = overwrite === i;
            return (
              <li key={i} className="min-h-0">
                <button
                  type="button"
                  aria-label={`${empty ? "Slot vazio" : info.title}, slot ${i + 1}${last ? ", último usado" : ""}`}
                  disabled={disabled}
                  onClick={() => {
                    if ((mode === "new" || mode === "save") && !empty && !confirm) {
                      onOverwrite(i);
                      return;
                    }
                    onPick(i);
                  }}
                  className={`flex min-h-[116px] w-full flex-col justify-center overflow-hidden ember-slot px-3 py-2 text-left disabled:opacity-40 sm:min-h-[128px] sm:px-4 sm:py-3 ${last ? "is-last" : ""}`}
                >
                  <div className="flex items-baseline justify-between gap-2">
                    <p className="text-[10px] ember-kicker sm:text-xs">Slot {i + 1}</p>
                    {last && <p className="truncate text-[9px] uppercase tracking-[0.1em] text-accent sm:text-[10px] sm:tracking-[0.14em]">Último usado</p>}
                  </div>
                  <p className="mt-1 truncate font-display text-lg leading-tight ember-title sm:text-xl">{info.title}</p>
                  <p className="line-clamp-2 text-xs leading-snug text-slate-300 sm:text-sm">{info.detail}</p>
                  {slot && !empty && (
                    <p className="mt-1 text-[10px] tabular-nums text-slate-400 sm:text-xs">{formatStamp(slot.updatedAt)}</p>
                  )}
                  {confirm && <p className="mt-1 text-[10px] text-accent sm:text-xs">Toque de novo para substituir.</p>}
                </button>
              </li>
            );
          })}
        </ol>
      </main>
      {saveProgress !== null && (
        <div className="absolute inset-0 z-20 grid place-items-center bg-black/60" aria-live="polite">
          <div className="w-[min(22rem,80vw)] ember-plate px-5 py-4 text-center">
            <p className="font-display text-lg ember-title">Salvando… {saveProgress}%</p>
            <div
              className="mt-3 h-2.5 overflow-hidden rounded-sm bg-black/60"
              role="progressbar" aria-label="Salvando o jogo" aria-valuemin={0} aria-valuemax={100} aria-valuenow={saveProgress}
            >
              <div
                className="h-full bg-gradient-to-r from-[#444444] via-[#e1a541] to-[#fff0a2] transition-[width] duration-100 ease-linear"
                style={{ width: `${saveProgress}%` }}
              />
            </div>
          </div>
        </div>
      )}
    </section>
  );
}
