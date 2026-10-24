import { MAP_BACKGROUNDS, mapBackgroundUrl } from '../mapBackgrounds';
// Ember's map editor (GameApp.tsx MapEditorScreen and its helpers), brought over as-is.
// Only the imports are adapted: Ember's game data comes from Engine2's snapshot (src/ember) and
// GPT's campaign port (src/campaign); Engine2's 3D map stands in for MapPreviewCanvas.
// Left out on purpose (user decision): the old tile art and the elemental 2D map FX.
import { hasSquareMapBorder } from "../../ember/mapFloor";
import { smoothPlainsCanvas, grassThumbnail } from "../../render/textures";
import { removeWallsUnderWatchtowerEntrances } from "../../campaign/watchtowerDungeon";
import { uiText } from "../../ember/gamePreferences";
import { type CSSProperties, type PointerEvent as ReactPointerEvent, type ReactNode, useCallback, useEffect, useMemo, useRef, useState } from "react";
import { ChevronLeft, Dices, Grip, Shuffle, X } from "lucide-react";
import { Button } from "@/components/ui/button";
import { THREE_D_DOOR_VARIANTS, decorationPlacementArt, SOLID_CART_DECOR_IDS, BARRICADE_LIKE_DECOR, BIG_HOUSE_DECOR_IDS, SOLID_HOUSE_DECOR_IDS, SOLID_ROCK_DECOR_IDS, CLASSES, DEADWOODS_DECOR_IDS, FOREST_DECOR_IDS, DECORATIONS, HOUSE_DECOR_IDS, MAX_GRID, MAX_LEVEL, MIN_GRID, TERRAIN, barricadeDecor, decorationCells, placedFootprint, decorationImage, decorationImageWebp, enemyLevelFor, dressMap, isSummonClass, MUSIC_TRACKS, SUMMON_CLASSES, parseLayout, terrainNote } from "../../ember/data";
import { ENCOUNTER_NPC_IDS, encounterNpcSpawn, type EncounterNpcId } from "../../ember/encounter-npcs";
import { DEFAULT_AMBIENT_INTENSITY, DEFAULT_BLOOM_INTENSITY, DEFAULT_SUN_INTENSITY, TIME_OF_DAY_LIGHT } from "../../ember/battleLight";
import { LIGHT_DEFS } from "../../ember/lighting";
import { CLAUDE_VARIANT, TILE_VARIANT_COUNT, tileVariantName, tileVariantSrc, GROUND_VARIANT_LAYERS } from "../../ember/tileVariants";
import { TERRAIN_INFO } from "../../map/board";
import { ENGINE2_V3_GROUND_VARIANTS, ENGINE2_V3_CITY_VARIANT } from "../../ember/tileVariants";
import { DialogEditor } from "./DialogEditor";
import { MapPreviewCanvas, type PreviewDecorationSelection, type PreviewUnitSelection } from "./MapPreview3D";
import {
  ALL_LOCATIONS, ALL_MISSIONS, DEFAULT_LOCATION_SUBMAPS, LOCATION_SLOTS, RANDOM_ENCOUNTER_REGIONS, clearSessionMapOverride, draftToMission,
  latestSerialFor, registerSessionMapOverride, loadActiveDrafts, loadActiveVersions, loadLocaisLocal, LOCAIS_LOCAL_KEY, loadVersionStore,
  locationFill, locationForMission, locationsForOrder, mapFileName, missionById, latestSavedDraft, saveActiveDrafts, saveActiveVersions,
  saveLocaisLocal, saveVersionStore, savedScenarios, savedVersionsFor, serialLabel, slotsFor,
  type MapDraft, type MapFile, type MapVersion, type DraftSpawn,
} from "../../campaign/mapstore";
import type { ClassId, DecorationPlacement, DialogTree, ElementalFxPlacement, GameArt, MapTimeOfDay, Mission, Spawn, SpriteId, TerrainId, WinCondition, WorldLocation } from "../../ember/types";
import { footprint, hexNeighbors } from "../../campaign/pathfinding";

/** A tile's picker image. Every terrain's base tile (001) is its painted diorama surface, drawn by
 * the same painter as the 3D ground, so the thumbnail always matches what the map shows. */
function SmoothPlainsIcon({ className }: { className: string }) {
  const [src, setSrc] = useState<string>();
  useEffect(() => {
    let active = true;
    void smoothPlainsCanvas().then(canvas => { if (active) setSrc(canvas.toDataURL()); }).catch(console.warn);
    return () => { active = false; };
  }, []);
  return src ? <img src={src} alt="Planicie 007" className={className} /> : <span className={className} />;
}

function thumbFor(terrain: TerrainId, variant: number): string {
  if (terrain === "void") return 'data:image/svg+xml,%3Csvg xmlns="http://www.w3.org/2000/svg" width="32" height="32"%3E%3Crect width="32" height="32" fill="black"/%3E%3C/svg%3E';
  if (terrain === "plains" && variant === 48) return "/game/ground-claude/049-plains/color.png";
  // Plains 001 shows the green grass the map actually draws, not a photo
  return terrain === "plains" && variant === 0 ? grassThumbnail() : tileVariantSrc(terrain, variant);
}

/** Baked ground variants appear beside the original surface and keep their own saved indices. */
function groundVersions(terrain: TerrainId): { label: string; variant: number; thumb: string }[] {
  const list = [{ variant: 0, thumb: thumbFor(terrain, 0) }];
  if (terrain === "plains") list.push({ variant: 48, thumb: thumbFor("plains", 48) });
  const claude = CLAUDE_VARIANT[terrain];
  if (claude !== undefined) list.push({ variant: claude, thumb: thumbFor(terrain, claude) });
  for (const entry of ENGINE2_V3_GROUND_VARIANTS.filter((item) => item.terrain === terrain && item.key !== "city")) {
    list.push({ variant: entry.variant, thumb: thumbFor(terrain, entry.variant) });
  }
  for (const entry of GROUND_VARIANT_LAYERS.filter(item => item.terrain === terrain && item.key !== 'photo-city' && item.key !== 'city-stone-paving' && item.key !== 'city-indoor-wood' && item.key !== 'variant-flame-005-fire' && item.key !== 'laje-021-stone-slabs')) {
    list.push({ variant: entry.variant, thumb: thumbFor(terrain, entry.variant) });
  }
  // Plains 02 is the vivid flat grass (saved index 55)
  if (terrain === "plains") { const k = list.findIndex(v => v.variant === 55); if (k > 1) list.splice(1, 0, ...list.splice(k, 1)); }
  // plains: numbered in picker order (001, 002, …); saved indices are unchanged
  if (terrain === "plains") return list.map((v, i) => ({ ...v, label: String(i + 1).padStart(3, "0") }));
  return list.map(v => ({ ...v, label: GROUND_VARIANT_LAYERS.find(item => item.terrain === terrain && item.variant === v.variant)?.label ?? String(v.variant + 1).padStart(3, "0") }));
}

/** The selected base as the picker shows it: terrain name plus its picker number/label. */
function baseVersionLabel(terrain: TerrainId, variant: number): string {
  if (terrain === "plains" && variant === ENGINE2_V3_CITY_VARIANT) return "City · Stone paving";
  if (terrain === "plains" && variant === 61) return "City · Indoor wood";
  const version = groundVersions(terrain).find((v) => v.variant === variant);
  return `${TERRAIN[terrain].name} ${version?.label ?? String(variant + 1).padStart(3, "0")}`;
}

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


/** A map JSON write updates Vite's module list and can reload the app. This one-shot
 * snapshot restores the editor instead of sending the author to the title screen. */
const EDITOR_RESUME_KEY = "ember:editor-resume";
export function readEditorResume(): MapDraft | null {
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
export function clearEditorResume(): void {
  try { window.sessionStorage.removeItem(EDITOR_RESUME_KEY); } catch { /* storage is optional */ }
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
    backgroundImage: m.backgroundImage,
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
  hill: "#8a7a4f",
  flame: "#b5501f",
  column: "#4a4a52",
  nave: "#26262c",
  barricade: "#5a4630",
  door: "#4a3524",
  void: "#000000",
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
    "Planície 02",
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
    setNote(`Base inteira substituída por ${baseVersionLabel(brush, selectedVariant)}.`);
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
    // A repository file is on disk and the campaign plays files from disk (mapstore
    // missionById ignores browser copies for any map that has a file), so no browser copy is
    // stored: a big map's copy overflowed browser storage and failed activation for nothing.
    const copyNote = "";
    setActiveVersions(next);
    saveActiveVersions(next);
    setDraft(f.draft);
    registerSessionMapOverride(f.draft);
    window.dispatchEvent(new CustomEvent("ember:missions-saved"));
    setNote(`${f.file ?? mapFileName(draft.id, f.serial)} agora é a cópia exata ativa na campanha.${copyNote}`);
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
            <span className="text-muted w-28 shrink-0">Imagem de fundo</span>
            <select className="bg-bg border border-border rounded-md px-2 py-1 flex-1" value={draft.backgroundImage ?? ""} onChange={e => setDraft(d => ({ ...d, backgroundImage: e.target.value }))}>
              <option value="">Vazio preto</option>
              {MAP_BACKGROUNDS.map(item => <option key={item.id} value={item.id}>{item.label}</option>)}
            </select>
          </label>
          {draft.backgroundImage && MAP_BACKGROUNDS.find(item => item.id === draft.backgroundImage) && <img className="w-full h-24 object-cover" alt="Fundo selecionado" src={mapBackgroundUrl(MAP_BACKGROUNDS.find(item => item.id === draft.backgroundImage)!)} />}
          <span className="text-xs text-muted">Pasta: public/game/map-backgrounds — JPG, PNG ou WebP</span>
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
                          setVariant((v) => ((v >= 21 && v <= 38 && v !== 22) || v === ENGINE2_V3_CITY_VARIANT || v === 54 || v === 61 ? v : ENGINE2_V3_CITY_VARIANT));
                        } else {
                          setCityMode(false);
                          setVariant((v) => terrain === 'barricade' ? tileVariant : (terrain === "plains" && ((v >= 21 && v <= 38) || v === ENGINE2_V3_CITY_VARIANT || v === 54) ? 0 : Math.min(v, (TILE_VARIANT_COUNT[terrain] ?? 1) - 1)));
                        }
                      }}
                      className={`text-xs px-1.5 py-1 rounded-md border flex items-center gap-1.5 ${selected ? "border-accent" : "border-border"}`}
                    >
                      <img src={thumbFor(terrain, key === "city" ? ENGINE2_V3_CITY_VARIANT : tileVariant)} alt="" className="size-7 rounded-sm shrink-0 object-cover" />
                      {label}
                    </button>
                  );
                })}
            </div>
            {/* Ember's "Versões" strip, for Engine2's numbered Planície tiles: 01 is the base,
                02 is the 049 PBR tile (saved as variant index 48, so existing maps keep it). */}
            {!cityMode && brush !== "void" && (
              <div className="flex items-start gap-1.5 text-xs">
                <span className="mt-1 text-muted uppercase tracking-wide">Versões</span>
                <div className="flex flex-wrap gap-1.5 rounded-md border border-border bg-bg/40 p-1.5">
                  {groundVersions(brush).map(({ label, variant: v, thumb }) => (
                    <button
                      key={v}
                      type="button"
                      title={`${TERRAIN[brush].name} ${label}${CLAUDE_VARIANT[brush] === v ? " · Claude" : ENGINE2_V3_GROUND_VARIANTS.some((entry) => entry.terrain === brush && entry.variant === v) ? " · Engine2 V3" : ""}`}
                      aria-pressed={variant === v}
                      onClick={() => setVariant(v)}
                      className={`flex items-center gap-1 rounded-md border overflow-hidden pr-1.5 ${variant === v ? "border-accent" : "border-border"}`}
                    >
                      {brush === "plains" && v === 56 ? <SmoothPlainsIcon className="size-8 object-cover" /> : thumb ? <img src={thumb} alt="" className="size-8 object-cover" /> : <span aria-hidden="true" className="size-8" style={{ background: brush === "plains" ? "linear-gradient(135deg,#6d9b2e,#4f7d22)" : TERRAIN_SWATCH[brush] }} />}
                      <span>{label}</span>
                    </button>
                  ))}
                </div>
              </div>
            )}
            {cityMode && (
              <div className="flex flex-wrap gap-1.5">{[{ variant: ENGINE2_V3_CITY_VARIANT, label: 'Stone paving' }, ...GROUND_VARIANT_LAYERS.filter(entry => entry.key === 'photo-city' || entry.key === 'city-indoor-wood')].map(entry => <button
                key={entry.variant}
                type="button"
                title={`City · ${entry.label}`}
                aria-pressed={variant === entry.variant}
                onClick={() => setVariant(entry.variant)}
                className={`flex w-fit items-center gap-1 rounded-md border overflow-hidden pr-1.5 text-xs ${variant === entry.variant ? "border-accent" : "border-border"}`}
              >
                <img src={thumbFor('plains', entry.variant)} alt="" className="size-8 object-cover" />
                <span>{entry.label}</span>
              </button>)}</div>
            )}
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
                  <img src={thumbFor("snow", 0)} alt="" className="size-7 rounded-sm shrink-0 object-cover" />
                  Neve e gelo
                </button>
              </div>
            </section>
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
              title={`Preenche todos os hexes com ${baseVersionLabel(brush, Math.min(variant, (TILE_VARIANT_COUNT[brush] ?? 1) - 1))}`}
              onClick={replaceBaseTile}
            >
              {brush === "plains" && variant === 56 ? <SmoothPlainsIcon className="size-5 rounded-sm shrink-0 object-cover" /> : <img src={thumbFor(brush, Math.min(variant, (TILE_VARIANT_COUNT[brush] ?? 1) - 1))} alt="" className="size-5 rounded-sm shrink-0 object-cover" style={{ background: TERRAIN_SWATCH[brush] }} />}
              Substituir base · {baseVersionLabel(brush, Math.min(variant, (TILE_VARIANT_COUNT[brush] ?? 1) - 1))}
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

