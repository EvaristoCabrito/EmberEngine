// Actual vertex displacement for the appended photographic tiles, shared by game and editor.
// Gameplay elevations stay unchanged; actors, picking and decorations use the same relief.
import type { Board } from '../map/board';
import { GROUND_LAYERS } from '../map/board';
import { hexAt } from '../core/hex';
import { GROUND_VARIANT_LAYERS, groundVariantMaps } from '../ember/tileVariants';

export interface ReliefGrid { size: number; values: number[] }
const grids = new Map<number, ReliefGrid>();
const pending = new Map<number, Promise<void>>();

export async function prepareTerrainRelief(board: Board): Promise<void> {
  const used = new Set(board.cells.map(c => c.ground - GROUND_LAYERS.length - 3).filter(i => i >= 0 && GROUND_VARIANT_LAYERS[i]?.relief > 0));
  await Promise.all([...used].map(i => {
    let job = pending.get(i);
    if (!job) {
      job = fetch(groundVariantMaps(GROUND_VARIANT_LAYERS[i]).height).then(async response => {
        if (!response.ok) throw new Error(`Missing 3D tile relief: ${GROUND_VARIANT_LAYERS[i].key}`);
        const grid = await response.json() as ReliefGrid;
        if (grid.size < 2 || grid.values.length !== grid.size * grid.size) throw new Error('Invalid tile relief');
        grids.set(i, grid);
      });
      pending.set(i, job);
    }
    return job;
  }));
}

export function sampleRelief(grid: ReliefGrid, x: number, z: number): number {
  const u = ((x / 4 % 1) + 1) % 1 * grid.size, v = ((z / 4 % 1) + 1) % 1 * grid.size;
  const ix = Math.floor(u), iz = Math.floor(v), fx = u - ix, fz = v - iz;
  const at = (dx: number, dz: number) => grid.values[((iz + dz) % grid.size) * grid.size + (ix + dx) % grid.size] / 255;
  return (at(0, 0) * (1 - fx) + at(1, 0) * fx) * (1 - fz) + (at(0, 1) * (1 - fx) + at(1, 1) * fx) * fz;
}

export function terrainReliefAt(board: Board, x: number, z: number): number {
  // Match the seven-sample material blend so relief doesn't create gaps between tile types.
  let result = 0;
  for (let k = 0; k < 7; k++) {
    const a = (k - 1) * Math.PI / 3;
    const [c, r] = hexAt(board.layout, x + (k ? Math.sin(a) * 0.3 : 0), z + (k ? Math.cos(a) * 0.3 : 0));
    const index = board.cell(c, r).ground - GROUND_LAYERS.length - 3;
    const grid = grids.get(index), entry = GROUND_VARIANT_LAYERS[index];
    if (grid && entry) result += sampleRelief(grid, x, z) * entry.relief / 7;
  }
  return result;
}
