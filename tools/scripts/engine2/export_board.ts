import { engineRoot } from './engineRoot';
// Exports Engine2's board and ground rules as data, plus Engine2's own output for one map, so
// EmberEngine's C++ port can be checked against it. Runs Engine2's TypeScript read-only (nothing
// under content\game is written; vite-node's cache goes to --root).
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_board.ts -- <map.json> <data out dir> <reference out dir>
//
// Data (committed, EmberEngine reads it): <data out>/board.json — terrain table, surfaces, ground
// layers (and which source files feed each, for the cooker), variant tables, constants.
// Reference (build output, verification only): <reference out>/<map id>/ cells.json and the smooth,
// hard and water geometry as raw little-endian arrays.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const E2 = engineRoot;
const PUBLIC = `${E2}/public`;

// Engine2 fetches its relief grids from the dev server; here they come straight from disk.
globalThis.fetch = (async (url: string) => {
  const path = `${PUBLIC}${url}`;
  if (!existsSync(path)) return { ok: false, status: 404, json: async () => null, headers: new Map() };
  const text = readFileSync(path, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [mapPath, dataOut, refOut] = process.argv.slice(2).filter((a) => a !== '--');
if (!mapPath || !dataOut || !refOut) throw new Error('usage: export_board.ts -- <map.json> <data out dir> <reference out dir>');

const board = await import(`${E2}/src/map/board.ts`);
const hex = await import(`${E2}/src/core/hex.ts`);
const variants = await import(`${E2}/src/ember/tileVariants.ts`);
const mesh = await import(`${E2}/src/render/terrainMesh.ts`);
const relief = await import(`${E2}/src/render/terrainRelief.ts`);
const emberData = await import(`${E2}/src/ember/data.ts`);

// ---- data -----------------------------------------------------------------------------------------

// Which files feed each ground layer in the set the game binds by default ("claude", groundSets.ts:
// `let current: GroundSet = 'claude'`), resolved the way loadSet() does: a variant layer has its own
// files; otherwise the first Claude folder with a colour map. Missing files = the layer is absent
// (Engine2 then keeps the procedural ground there).
const exists = (url: string) => existsSync(`${PUBLIC}${url}`);
const groundVariantLayers = variants.GROUND_VARIANT_LAYERS;
const terrains: string[] = [...board.GROUND_LAYERS, 'city', 'shallow-water', 'river-rocks', ...groundVariantLayers.map((e: { key: string }) => e.key)];
const COLOR_ONLY = new Set(['variant-plains-meadow-natural-v01', 'variant-plains-meadow-natural-v02', 'variant-plains-meadow-scrub-rare-v01', 'variant-plains-real-grass', 'variant-plains-real-grass-009',
  'variant-plains-vau-grass-v01',
  'variant-flame-005-fire', 'city-stone-paving', 'city-indoor-wood', 'laje-021-stone-slabs']);
const groundLayers = terrains.map((terrain, index) => {
  const variant = groundVariantLayers.find((e: { key: string }) => e.key === terrain);
  if (variant) {
    const paths = variants.groundVariantMaps(variant);
    const colorOnly = COLOR_ONLY.has(variant.key);
    const layer = { index, key: terrain, relief: variant.relief, color: paths.color,
      normal: colorOnly ? null : paths.normal, rough: colorOnly ? null : paths.rough, reliefGrid: variant.relief > 0 ? paths.height : null };
    for (const p of [layer.color, layer.normal, layer.rough]) if (p && !exists(p)) throw new Error(`missing ${p}`);
    return layer;
  }
  const folders = [...(terrain === 'plains' ? ['/game/ground-claude/plains-v2'] : []), `/game/ground-claude/${terrain}`];
  for (const folder of folders) {
    if (!exists(`${folder}/color.png`)) continue;
    return { index, key: terrain, color: `${folder}/color.png`, normal: exists(`${folder}/normal.png`) ? `${folder}/normal.png` : null,
      rough: exists(`${folder}/rough.png`) ? `${folder}/rough.png` : null };
  }
  return { index, key: terrain, absent: true };
});

const data = {
  source: 'Exported from Engine2 (src/map/board.ts, src/ember/tileVariants.ts, src/render/groundSets.ts) by tools/scripts/engine2/export_board.ts',
  constants: { R: hex.R, STEP: hex.STEP, MOUNTAIN_ROCK: board.MOUNTAIN_ROCK, MOUNTAIN_SNOW: board.MOUNTAIN_SNOW, LEVEL_MIN: board.LEVEL_MIN,
    LEVEL_MAX: board.LEVEL_MAX, BASE_LEVELS: 2 },
  surfaces: board.SURFACE,
  terrain: board.TERRAIN_INFO,
  groundLayerTerrains: board.GROUND_LAYERS,
  groundLayerCount: board.GROUND_LAYER_COUNT,
  groundLayers,
  groundVariantLayers,
  engine2V3GroundVariants: variants.ENGINE2_V3_GROUND_VARIANTS,
  engine2V3CityVariant: variants.ENGINE2_V3_CITY_VARIANT,
  claudeVariant: variants.CLAUDE_VARIANT,
  // terrainMesh.ts groundFootprint(): decorations whose 3D wall/door grid reshapes the ground's edge.
  // wall-3d-low: taken out on the user's order (tools/scripts/engine2/removed_decorations.py)
  architectureDecorations: Object.values(emberData.DECORATIONS as Record<string, { id: string; model3d?: string; propModel?: string }>).filter((d) => d.id !== 'wall-3d-low')
    .filter((d) => d.model3d && ['wall', 'doorway', 'door', 'secretDoor'].includes(d.model3d) && !d.propModel).map((d) => d.id).sort(),
};
mkdirSync(dataOut, { recursive: true });
writeFileSync(join(dataOut, 'board.json'), JSON.stringify(data, null, 1));

// ---- reference output for one map ------------------------------------------------------------------

const file = JSON.parse(readFileSync(mapPath, 'utf8'));
const draft = file.draft ?? file;
const b = new board.Board(draft);
await relief.prepareTerrainRelief(b);
const dir = join(refOut, draft.id);
mkdirSync(dir, { recursive: true });
writeFileSync(join(dir, 'cells.json'), JSON.stringify(b.cells.map((c: Record<string, unknown>) => ({
  c: c.c, r: c.r, x: c.x, z: c.z, type: c.type, level: c.level, surface: c.surface, ground: c.ground, groundY: c.groundY, water: c.water, waterY: c.waterY,
}))));

type Geometry = { attributes: Record<string, { array: Float32Array; itemSize: number }>; index: { array: Uint32Array } | null };
const dump = (name: string, g: Geometry | null) => {
  if (!g) return { name, present: false };
  const parts: Record<string, unknown> = { name, present: true, attributes: {} as Record<string, number> };
  for (const [key, attr] of Object.entries(g.attributes)) {
    writeFileSync(join(dir, `${name}.${key}.f32`), Buffer.from(attr.array.buffer, attr.array.byteOffset, attr.array.byteLength));
    (parts.attributes as Record<string, number>)[key] = attr.itemSize;
  }
  if (g.index) writeFileSync(join(dir, `${name}.index.u32`), Buffer.from(g.index.array.buffer, g.index.array.byteOffset, g.index.array.byteLength));
  parts.indexed = !!g.index;
  return parts;
};
const manifest = {
  map: draft.id,
  source: mapPath,
  geometry: [
    dump('smooth', mesh.buildSmoothGround(b)),
    dump('hard', mesh.buildHardGround(b)),
    dump('water-smooth', mesh.buildWater(b, 'smooth')),
    dump('water-hard', mesh.buildWater(b, 'hard')),
  ],
  // A few heights for placement checks.
  heights: b.cells.filter((_: unknown, i: number) => i % 7 === 0).map((c: { x: number; z: number }) => ({
    x: c.x + 0.31, z: c.z - 0.17,
    smooth: mesh.groundHeightAt(b, 'smooth', c.x + 0.31, c.z - 0.17),
    hard: mesh.groundHeightAt(b, 'hard', c.x + 0.31, c.z - 0.17),
  })),
};
writeFileSync(join(dir, 'manifest.json'), JSON.stringify(manifest, null, 1));
console.log(`exported board data (${groundLayers.length} ground layers) and ${draft.id}: ${b.cells.length} cells`);
