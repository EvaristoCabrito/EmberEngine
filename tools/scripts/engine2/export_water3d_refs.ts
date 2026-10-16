// Reference sheets for Ember's 3D water (the Map Editor's Água 3D brush): Engine2's own ThreeWater.rebuild
// (src/game/gfx/three/ThreeWater.ts) called as ThreeBattleRenderer.syncWater calls it (tile 1; allowed = not void;
// board rectangle on straight-border maps), on real map boards with water written by this script: per-hex levels,
// per-hex brush footprints and free patches, round and square. The ground under it is a fixed formula, so the
// shore depths compare too. EmberEngine's port (engine/world/Water3D.cpp) is compared with it by
// Cooker check-water3d. Engine2 is run read-only.
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_water3d_refs.ts -- <out refs.json> <map.json>...

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
// three's TextureLoader asks the DOM for an <img>; the sheet does not need the textures.
(globalThis as any).document = {
  createElementNS: () => ({ addEventListener() {}, removeEventListener() {}, set src(_v: string) {}, style: {} }),
};

const args = process.argv.slice(2).filter((a) => a !== '--');
const [outPath, ...maps] = args;
if (!outPath || !maps.length) throw new Error('usage: export_water3d_refs.ts -- <out refs.json> <map.json>...');

const { ThreeWater } = await import(`${E2}/src/game/gfx/three/ThreeWater.ts`);
const { hasSquareMapBorder } = await import(`${E2}/src/game/mapFloor.ts`);
const SQ3 = Math.sqrt(3);
const BOARD_PAD_MUL = 2.4;
// the ground under the water (three's frame: x, -y), the same formula as Cooker check-water3d
const ground = (x: number, y: number) => 0.3 * Math.sin(0.7 * x) + 0.2 * Math.cos(0.5 * y) + 0.15;

const cases: unknown[] = [];
for (const mapPath of maps) {
  const file = JSON.parse(readFileSync(mapPath, 'utf8').replace(/^\uFEFF/, ''));
  const d = file.draft ?? file;
  const cols: number = d.cols, rows: number = d.rows, tiles: string[] = d.tiles;
  // water in a corner region (keeps the sheets small); every kind of input
  const c0 = Math.floor(cols / 4), r0 = Math.floor(rows / 4);
  const levels: (number | null)[] = new Array(cols * rows).fill(null);
  const footprints: ({ size: number; shape: 'round' | 'square' } | null)[] = new Array(cols * rows).fill(null);
  for (let r = r0; r < r0 + 6 && r < rows; r++)
    for (let c = c0; c < c0 + 7 && c < cols; c++) {
      if ((c * 7 + r * 3) % 4 === 0) continue;
      levels[r * cols + c] = ((c + 2 * r) % 5) * 0.5;
      if ((c + r) % 3 === 0) footprints[r * cols + c] = { size: 0.4 + ((c * 13 + r) % 7) * 0.1, shape: (c + r) % 2 ? 'square' : 'round' };
    }
  // an edge cell, so straight-border maps extend water to the cut
  levels[0] = 1; levels[1] = 0.5;
  const patches: { x: number; y: number; level: number; size: number; shape: 'round' | 'square' }[] = [];
  for (let k = 0; k < 14; k++) {
    patches.push({
      x: SQ3 * (c0 + 8) + Math.sin(k * 1.7) * 3.1,
      y: BOARD_PAD_MUL + 1.5 * (r0 + 2) + 1 + Math.cos(k * 2.3) * 2.6,
      level: (k % 7) * 0.45,
      size: 0.25 + (k % 4) * 0.25,
      shape: k % 3 ? 'round' : 'square',
    });
  }
  const variants: [string, (number | null)[], typeof footprints, typeof patches][] = [
    ['levels+patches', levels, footprints, patches],
    ['levels', levels, footprints, []],
    ['patches', new Array(cols * rows).fill(null), [], patches],
  ];
  for (const square of [false, true, undefined]) {
    for (const [name, lv, fp, pt] of variants) {
      const water = new ThreeWater();
      const board = hasSquareMapBorder(tiles, cols, rows, square)
        ? { minX: 0, maxX: SQ3 * cols, minY: BOARD_PAD_MUL + 0.25, maxY: BOARD_PAD_MUL + 0.25 + rows * 1.5 }
        : undefined;
      water.rebuild(cols, rows, 1, lv, (col: number, row: number) => (tiles[row * cols + col] ?? 'void') !== 'void', ground, fp, pt, board);
      const g = water.mesh.geometry;
      const pick = (attr: string) => (g.getAttribute(attr) ? Array.from(g.getAttribute(attr).array as Float32Array) : []);
      cases.push({
        map: d.id ?? mapPath, variant: name, squareTiles: square ?? null, square: !!board, cols, rows, tiles,
        levels: lv, footprints: fp, patches: pt,
        position: pick('position'), uv: pick('uv'), coverage: pick('coverage'), waterDepth: pick('waterDepth'),
        index: g.index ? Array.from(g.index.array as Uint32Array) : [],
      });
    }
  }
}
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ engine2: E2, ground: '0.3 sin(0.7 x) + 0.2 cos(0.5 y) + 0.15', cases }));
console.log(`${cases.length} cases -> ${outPath}`);
