// The baked ground tile sets, side by side with the painted prototype ground:
//   proto  — the procedural surfaces in textures.ts (the default; nothing changes unless picked)
//   claude — public/game/ground-claude/<terrain>/{color,normal,rough}.png
//   gpt    — GPT's Task 9 set, public/game/ground/<family>/ (same names ground-compare uses)
//   engine2-v4 — photoreal 2D terrain textures, public/game/ground-engine2-v4/<terrain>/
// One layer per basic Ember terrain (GROUND_LAYERS in board.ts), plus City and two water
// variants, held in three texture arrays (colour, normal, roughness). A layer a
// set doesn't have yet falls back to the prototype ground. Each set is loaded on first use only.
// The pick is remembered per browser and shared by every terrain material (editor and game).

import * as THREE from 'three';
import { GROUND_LAYER_COUNT, GROUND_LAYERS } from '../map/board';
import { ENGINE2_V3_GROUND_VARIANTS, GROUND_VARIANT_LAYERS, groundVariantMaps } from '../ember/tileVariants';
import { maxAnisotropy } from './gpu';
import { smoothPlainsCanvas } from './textures';

export type GroundSet = 'proto' | 'claude' | 'gpt' | 'engine2-v3' | 'engine2-v4';
export const GROUND_SET_LABEL: Record<GroundSet, string> = { proto: 'Protótipo', claude: 'Claude', gpt: 'GPT', 'engine2-v3': 'Engine2 V3', 'engine2-v4': 'Engine2 V4 Photo' };

/** GPT's folder names per terrain, tried in order (from ground-compare). */
const GPT_FOLDERS: Record<string, string[]> = {
  plains: ['grass-plains', 'grass_plains', 'plains'],
  woods: ['forest-floor', 'forest_floor', 'woods'],
  ruins: ['ruin-pavers', 'ruin_pavers', 'ruins'],
  water: ['river-bed', 'river_bed', 'water'],
  ember: ['ash-embers', 'ash_embers', 'ember'],
  hill: ['hill-grass', 'hill_grass', 'hill'],
  flame: ['ash-embers', 'ash_embers', 'flame'],
  column: ['column-rock', 'column_rock', 'column'],
  nave: ['nave-hall-slabs', 'nave_hall_slabs', 'nave'],
  barricade: ['barricade-dirt', 'barricade_dirt', 'barricade'],
  door: ['dungeon-flagstone', 'dungeon_flagstone', 'door'],
  snow: ['fresh-snow', 'fresh_snow', 'snow'],
};

const folders = (set: Exclude<GroundSet, 'proto'>, terrain: string): string[] => {
  if (set === 'engine2-v3' || set === 'engine2-v4') {
    const v3 = ENGINE2_V3_GROUND_VARIANTS.find((entry) => entry.key === terrain);
    const version = set === 'engine2-v4' ? 'v4' : 'v3';
    if (v3) return [`/game/ground-engine2-${version}/${v3.folder}`];
    // base terrain textures of the set (their picker entries were deleted, the textures stay the terrain's look)
    if (terrain === 'plains' || terrain === 'woods' || terrain === 'water' || terrain === 'ember') return [`/game/ground-engine2-${version}/${terrain}`];
    // The generated V3/V4 packs do not include snow or barricade. Keep those authored tiles
    // visible in full 3D with the existing photoreal Claude PBR materials instead of silently
    // dropping them back to the prototype shader.
    return terrain === 'snow' || terrain === 'barricade' ? [`/game/ground-claude/${terrain}`] : [];
  }
  return set === 'claude'
    ? [...(terrain === 'plains' ? ['/game/ground-claude/plains-v2'] : []), `/game/ground-claude/${terrain}`]
    : (GPT_FOLDERS[terrain] ?? [terrain]).map(f => `/game/ground/${f}`);
};

function emptyArray(): THREE.DataArrayTexture {
  const t = new THREE.DataArrayTexture(new Uint8Array(4 * GROUND_LAYER_COUNT), 1, 1, GROUND_LAYER_COUNT);
  t.needsUpdate = true;
  return t;
}

/** Shared by every terrain material: swapping a value here updates them all. */
export const groundSetUniforms = {
  uGroundSet: { value: 0 },
  tSetColor: { value: emptyArray() as THREE.Texture },
  tSetNormal: { value: emptyArray() as THREE.Texture },
  tSetRough: { value: emptyArray() as THREE.Texture },
  uSetHas: { value: new Array<number>(GROUND_LAYER_COUNT).fill(0) },
};

const KEY = 'engine2:ground-set';
// Claude's tiles are painted per hex (their own versions in the editor), so its arrays are
// always the ones bound; the shader only uses them on hexes painted with them.
let current: GroundSet = 'claude';
const listeners = new Set<(set: GroundSet) => void>();
let anisotropy = 1;

export function getGroundSet(): GroundSet { return current; }
export function onGroundSet(fn: (set: GroundSet) => void): () => void { listeners.add(fn); return () => listeners.delete(fn); }

type Loaded = { color: THREE.DataArrayTexture; normal: THREE.DataArrayTexture; rough: THREE.DataArrayTexture; has: number[] };
const loaded = new Map<GroundSet, Promise<Loaded>>();

async function bitmap(url: string): Promise<ImageBitmap | null> {
  try {
    const res = await fetch(url);
    if (!res.ok || !(res.headers.get('content-type') ?? '').startsWith('image/')) return null;
    // raw pixels: no colour conversion or premultiply (normal and roughness are data)
    return await createImageBitmap(await res.blob(), { colorSpaceConversion: 'none', premultiplyAlpha: 'none' });
  } catch { return null; }
}

async function loadSet(set: Exclude<GroundSet, 'proto'>): Promise<Loaded> {
  const maps = ['color', 'normal', 'rough'] as const;
  // per terrain: the first folder that has a colour map; its normal/rough from the same folder
  const terrains = [...GROUND_LAYERS, 'city', 'shallow-water', 'river-rocks', ...GROUND_VARIANT_LAYERS.map((entry) => entry.key)];
  const layers = await Promise.all(terrains.map(async (terrain) => {
    const addedVariant = GROUND_VARIANT_LAYERS.find((entry) => entry.key === terrain);
    if (addedVariant?.key === 'variant-plains-smooth') {
      return { color: await createImageBitmap(await smoothPlainsCanvas()), normal: null, rough: null };
    }
    if (addedVariant?.key === 'variant-plains-real-grass' || addedVariant?.key === 'variant-plains-real-grass-009' || addedVariant?.key === 'variant-plains-meadow-3d' || addedVariant?.key === 'variant-plains-tall-3d' || addedVariant?.key === 'variant-flame-005-fire' || addedVariant?.key === 'city-stone-paving' || addedVariant?.key === 'city-indoor-wood' || addedVariant?.key === 'laje-021-stone-slabs') {
      const color = await bitmap(groundVariantMaps(addedVariant).color);
      if (!color) throw new Error(`Missing grass texture: ${addedVariant.key}`);
      return { color, normal: null, rough: null };
    }
    if (addedVariant) {
      const paths = groundVariantMaps(addedVariant);
      const [color, normal, rough] = await Promise.all([bitmap(paths.color), bitmap(paths.normal), bitmap(paths.rough)]);
      if (!color || !normal || !rough) throw new Error(`Missing 3D tile material: ${addedVariant.key}`);
      return { color, normal, rough };
    }
    for (const folder of folders(set, terrain)) {
      const color = await bitmap(`${folder}/color.png`);
      if (!color) continue;
      const [normal, rough] = await Promise.all([bitmap(`${folder}/normal.png`), bitmap(`${folder}/rough.png`)]);
      return { color, normal, rough };
    }
    return null;
  }));
  // One size per set. Keep the 15-layer V3 array at 1024 so its three PBR arrays stay practical
  // on GPU memory; source maps remain at their baked resolution on disk.
  const largest = Math.max(1, ...layers.flatMap(l => (l ? [l.color.width, l.color.height] : [])));
  const size = Math.min(1024, largest);
  const canvas = new OffscreenCanvas(size, size);
  const ctx = canvas.getContext('2d', { willReadFrequently: true })!;
  const build = (key: typeof maps[number], fallback: [number, number, number]) => {
    const data = new Uint8Array(size * size * 4 * GROUND_LAYER_COUNT);
    layers.forEach((layer, k) => {
      const img = layer?.[key];
      if (img) {
        ctx.clearRect(0, 0, size, size);
        ctx.drawImage(img, 0, 0, size, size);
        data.set(ctx.getImageData(0, 0, size, size).data, k * size * size * 4);
      } else {
        // a set with a colour map but no normal/rough: flat normal, matte surface
        for (let p = k * size * size * 4, e = p + size * size * 4; p < e; p += 4) { data[p] = fallback[0]; data[p + 1] = fallback[1]; data[p + 2] = fallback[2]; data[p + 3] = 255; }
      }
    });
    const t = new THREE.DataArrayTexture(data, size, size, GROUND_LAYER_COUNT);
    t.wrapS = t.wrapT = THREE.RepeatWrapping;
    t.magFilter = THREE.LinearFilter;
    t.minFilter = THREE.LinearMipmapLinearFilter;
    t.generateMipmaps = true;
    t.anisotropy = anisotropy;
    if (key === 'color') t.colorSpace = THREE.SRGBColorSpace;
    t.needsUpdate = true;
    return t;
  };
  const result: Loaded = {
    color: build('color', [128, 128, 128]),
    normal: build('normal', [128, 128, 255]),
    rough: build('rough', [235, 235, 235]),
    has: layers.map(l => (l ? 1 : 0)),
  };
  for (const l of layers) { l?.color.close(); l?.normal?.close(); l?.rough?.close(); }
  return result;
}

function apply(): void {
  const set = current;
  groundSetUniforms.uGroundSet.value = 0;
  const source = set === 'proto' ? 'claude' : set;
  let job = loaded.get(source);
  if (!job) { job = loadSet(source); loaded.set(source, job); }
  void job.then((l) => {
    if (current !== set) return;
    groundSetUniforms.tSetColor.value = l.color;
    groundSetUniforms.tSetNormal.value = l.normal;
    groundSetUniforms.tSetRough.value = l.rough;
    groundSetUniforms.uSetHas.value = set === 'proto' ? l.has.map((has, i) => i < GROUND_LAYERS.length + 3 ? 0 : has) : l.has;
    groundSetUniforms.uGroundSet.value = 1;
    listeners.forEach(fn => fn(set));
  });
}

export function setGroundSet(set: GroundSet): void {
  current = set;
  try { localStorage.setItem(KEY, set); } catch { /* the pick still applies to this page */ }
  apply();
}

/** The game's loading curtain must wait for the tile materials, not just the meshes. */
export async function ensureGroundSets(): Promise<void> {
  const source = current === 'proto' ? 'claude' : current;
  let job = loaded.get(source);
  if (!job) { job = loadSet(source); loaded.set(source, job); }
  await job;
}

/** Called by the terrain material once a renderer exists (anisotropy), then loads the saved pick. */
let started = false;
export function startGroundSets(renderer: THREE.WebGLRenderer): void {
  if (started) return;
  started = true;
  anisotropy = maxAnisotropy(renderer);
  apply();
}

/** How many of the 12 tiles the set actually has (for the editor's switch). */
export async function groundSetCount(set: Exclude<GroundSet, 'proto'>): Promise<number> {
  const found = await Promise.all([...GROUND_LAYERS, 'city', 'shallow-water', 'river-rocks', ...GROUND_VARIANT_LAYERS.map((entry) => entry.key)].map(async (terrain) => {
    const addedVariant = GROUND_VARIANT_LAYERS.find((entry) => entry.key === terrain);
    if (addedVariant) {
      try {
        const res = await fetch(`/game/ground-variant-tiles/${addedVariant.file}`, { method: 'HEAD' });
        return res.ok && (res.headers.get('content-type') ?? '').startsWith('image/') ? 1 : 0;
      } catch { return 0; }
    }
    for (const folder of folders(set, terrain)) {
      try {
        const res = await fetch(`${folder}/color.png`, { method: 'HEAD' });
        if (res.ok && (res.headers.get('content-type') ?? '').startsWith('image/')) return 1;
      } catch { /* missing */ }
    }
    return 0;
  }));
  return found.reduce<number>((a, b) => a + b, 0);
}
