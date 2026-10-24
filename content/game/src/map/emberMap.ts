// Ember's saved map files, read exactly as Ember writes them (src/game/maps/<id><serial>.json,
// copied into Engine2/maps/ember). Only the fields Engine2 uses so far are typed; the rest is
// carried along untouched so a map can round-trip.

export type TerrainId =
  | 'plains' | 'woods' | 'ruins' | 'water' | 'ember' | 'hill' | 'flame'
  | 'column' | 'nave' | 'barricade' | 'door' | 'void' | 'snow';

export interface EmberSpawn { name: string; classId: string; x: number; y: number; [k: string]: unknown }
export interface EmberDecoration { id: string; x: number; y: number; rot?: number; [k: string]: unknown }

export interface EmberMapDraft {
  id: string;
  title: string;
  cols: number;
  rows: number;
  tiles: TerrainId[];
  tileVariants?: number[];
  terrainElevations?: number[];
  waterLevels?: (number | null)[];
  decorations?: EmberDecoration[];
  playerSpawns?: EmberSpawn[];
  enemySpawns?: EmberSpawn[];
  neutralSpawns?: EmberSpawn[];
  environment?: 'outdoor' | 'indoor';
  timeOfDay?: string;
  squareTiles?: boolean;
  [k: string]: unknown;
}

export interface EmberMapFile { serial: number; savedAt: number; draft: EmberMapDraft }

export interface MapEntry {
  id: string;
  serial: number;
  file: string;
  /** "ember" = Ember's original file (read-only), "e2" = saved by Engine2's editor */
  source: 'ember' | 'e2';
  load: () => Promise<EmberMapFile>;
}

interface ListedMap { id: string; serial: number; file: string; source: 'ember' | 'e2' }
const entry = (m: ListedMap): MapEntry => ({ ...m, load: async () => (await fetch(`/__map/${m.source}/${encodeURIComponent(m.file)}`)).json() });

/** One entry per map id, from the dev server: the highest serial (Ember's or Engine2's) plays;
 * older serials stay on disk as the rollback history. */
export async function listEmberMaps(): Promise<MapEntry[]> {
  const list: ListedMap[] = await (await fetch('/__maps')).json();
  return list.map(entry);
}

/** Save a draft as the next serial in maps/e2. Never overwrites any file. */
export async function saveMap(draft: EmberMapDraft): Promise<MapEntry> {
  const r = await fetch('/__save-map', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify(draft) });
  if (!r.ok) throw new Error(await r.text());
  return entry(await r.json());
}
