// Builds the ground for a board in one of two player-selectable looks (PLAN: Terrain edges):
//  - smooth: one continuous heightfield; each point reads the hex under a gently warped
//    position, so terraces and shores wander naturally around the hex layout.
//  - hard:   every height step is a crisp hex cliff; same-level ground still flows seamlessly.
// Both use the same cells, so gameplay is identical. No tile seams or grid lines in either.

import * as THREE from 'three';
import { fbm, vnoise } from '../core/noise';
import { type HexLayout, hexAt, neighbors, corner, SQ3, STEP } from '../core/hex';
import type { Board, Cell } from '../map/board';
import { terrainReliefAt } from './terrainRelief';
import { GROUND_LAYER_COUNT, GROUND_WEIGHT_ATTRIBUTES } from '../map/board';
import { DECORATIONS } from '../ember/data';

export type EdgeStyle = 'smooth' | 'hard';

const WARP = 1.25;
/** How far the board's cut edge drops below its ground: a thin lip, no underground on show. */
const EDGE_LIP = 0.35;
export function warp(x: number, z: number): [number, number] {
  return [x + (fbm(x * 0.42 + 3.1, z * 0.42, 3) - 0.5) * WARP, z + (fbm(x * 0.42, z * 0.42 + 7.7, 3) - 0.5) * WARP];
}

const cellAt = (b: Board, L: HexLayout, x: number, z: number): Cell => { const [c, r] = hexAt(L, x, z); return b.cell(c, r); };

const RING7 = [[0, 0], ...Array.from({ length: 6 }, (_, k) => [Math.sin((k * Math.PI) / 3), Math.cos((k * Math.PI) / 3)])];

/** Weights per vertex: 8 shared surfaces plus the baked terrain layers, including V3 City. */
const STRIDE = 8 + GROUND_LAYER_COUNT;
const WEIGHT_ATTRIBUTES = ['splatA', 'splatB', ...GROUND_WEIGHT_ATTRIBUTES];

/** Surface and tile weights around a point: softly blended over real ground only, so the ground
 *  beside a void keeps its full colour. With no ground in reach it takes the fallback cell's. */
function splat(b: Board, L: HexLayout, sx: number, sz: number, rad: number, out: Float32Array, o: number, fallback?: Cell): void {
  const w = new Array<number>(STRIDE).fill(0);
  let n = 0;
  for (const [ox, oz] of RING7) {
    const cell = cellAt(b, L, sx + ox * rad, sz + oz * rad);
    if (cell.surface < 0) continue;
    n++; w[cell.surface] += 1;
    if (cell.ground >= 0) w[8 + cell.ground] += 1;
  }
  if (!n && fallback && fallback.surface >= 0) {
    n = 1; w[fallback.surface] = 1;
    if (fallback.ground >= 0) w[8 + fallback.ground] = 1;
  }
  for (let k = 0; k < STRIDE; k++) w[k] /= n || 1;
  out.set(w, o * STRIDE);
}

function finish(pos: Float32Array, splats: Float32Array, index: Uint32Array | null, normals: Float32Array | null): THREE.BufferGeometry {
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  const n = pos.length / 3, parts = Array.from({ length: Math.ceil(STRIDE / 4) }, () => new Float32Array(n * 4));
  for (let v = 0; v < n; v++) parts.forEach((part, k) => {
    const start = v * STRIDE + k * 4, end = Math.min(start + 4, (v + 1) * STRIDE);
    if (end > start) part.set(splats.subarray(start, end), v * 4);
  });
  parts.forEach((part, index) => g.setAttribute(WEIGHT_ATTRIBUTES[index]!, new THREE.BufferAttribute(part, 4)));
  if (index) g.setIndex(new THREE.BufferAttribute(index, 1));
  if (normals) g.setAttribute('normal', new THREE.BufferAttribute(normals, 3));
  else g.computeVertexNormals();
  g.computeBoundingSphere();
  return g;
}

/** Ember's architecture (decor.ts addArchitecture) stands on a square grid: every row at the same
 *  x, no odd-row half-hex shift, walls 0.32 thick on the cell's centre line. */
const ARCHITECTURE = new Set(['wall', 'doorway', 'door', 'secretDoor']);
const WALL_HALF = 0.16;

/** Where the board has ground. Hex cells everywhere, except around walls: there the ground follows
 *  the walls' square grid and stops at a wall's outer face where void lies beyond it, so no
 *  half-hexes poke out past the wall in steps. */
function groundFootprint(b: Board): (x: number, z: number) => boolean {
  const L = b.layout;
  const inBoard = (c: number, r: number) => c >= 0 && r >= 0 && c < L.cols && r < L.rows;
  const walls = new Set<number>();
  for (const p of b.draft.decorations ?? []) {
    const def = DECORATIONS[p.id];
    if (def?.model3d && ARCHITECTURE.has(def.model3d) && !def.propModel && inBoard(p.x, p.y)) walls.add(p.y * L.cols + p.x);
  }
  const hexSolid = (x: number, z: number) => cellAt(b, L, x, z).surface >= 0;
  if (!walls.size) return hexSolid;
  const isWall = (c: number, r: number) => inBoard(c, r) && walls.has(r * L.cols + c);
  const voidAt = (c: number, r: number) => inBoard(c, r) && b.cell(c, r).surface < 0;
  return (x: number, z: number) => {
    const fc = (x + L.ox) / SQ3, fr = (z + L.oz) / 1.5;
    const c = Math.round(fc), r = Math.round(fr);
    let near = false;
    for (let dr = -1; dr <= 1 && !near; dr++) for (let dc = -1; dc <= 1; dc++) if (isWall(c + dc, r + dr)) { near = true; break; }
    if (!near) return hexSolid(x, z);
    if (!inBoard(c, r) || b.cell(c, r).surface < 0) return false;
    if (!isWall(c, r)) return true;
    // a wall cell: cut at the wall's face on every side that opens onto void
    const ox = (fc - c) * SQ3, oz = (fr - r) * 1.5;
    if (ox < -WALL_HALF && voidAt(c - 1, r)) return false;
    if (ox > WALL_HALF && voidAt(c + 1, r)) return false;
    if (oz < -WALL_HALF && voidAt(c, r - 1)) return false;
    if (oz > WALL_HALF && voidAt(c, r + 1)) return false;
    return true;
  };
}

/** Smooth look: a dense heightfield. Resolution adapts so big maps stay light.
 *  Where the map turns to void the ground is cut exactly on the hex edge (not square by square,
 *  which left saw teeth) and a short lip drops from the cut, like the board's outer edge. */
export function buildSmoothGround(b: Board): THREE.BufferGeometry {
  const L = b.layout, { x0, x1, z0, z1 } = b.bounds();
  const res = Math.max(0.125, Math.sqrt(((x1 - x0) * (z1 - z0)) / 220000));
  const nx = Math.ceil((x1 - x0) / res), nz = Math.ceil((z1 - z0) / res);
  const VX = nx + 3, VZ = nz + 3, N = VX * VZ; // +1 ring on each side forms the diorama's cut walls
  const SM = [[0, 0], ...Array.from({ length: 8 }, (_, k) => [Math.sin((k * Math.PI) / 4) * res * 1.1, Math.cos((k * Math.PI) / 4) * res * 1.1])];
  const solidAt = groundFootprint(b);
  const pos: number[] = [], spl: number[] = [], lipOf: number[] = [], tmp = new Float32Array(STRIDE);
  // One ground point. `lip` (0..1) sinks it toward the board's outer cut edge.
  const addPoint = (x: number, z: number, lip: number): number => {
    const [wx, wz] = warp(x, z);
    // only real ground counts toward the height: averaging a void's height 0 in hung a slope into it
    let h = 0, solid = 0;
    for (const [ox, oz] of SM) { const s = cellAt(b, L, wx + ox, wz + oz); if (s.surface >= 0) { h += s.groundY; solid++; } }
    const own = cellAt(b, L, x, z);
    h = solid ? h / solid : own.groundY;
    // Only the new smooth plains version omits decorative vertex noise.
    const cell = cellAt(b, L, wx, wz);
    const flatPlains = cell.type === 'plains' && (b.draft.tileVariants?.[cell.i] === 56 || b.draft.tileVariants?.[cell.i] === 57 || b.draft.tileVariants?.[cell.i] === 58 || b.draft.tileVariants?.[cell.i] === 59 || b.draft.tileVariants?.[cell.i] === 60 || b.draft.tileVariants?.[cell.i] === 61);
    // the cut wall is a short lip under the ground at the board's edge, never a cliff hanging down to 0
    const top = h + terrainReliefAt(b, x, z);
    pos.push(x, lip > 0 ? top - EDGE_LIP * lip : top + (flatPlains ? 0 : (vnoise(x * 2.3, z * 2.3) - 0.5) * 0.04), z);
    splat(b, L, wx, wz, 0.3, tmp, 0, own);
    for (let k = 0; k < STRIDE; k++) spl.push(tmp[k]);
    lipOf.push(lip);
    return lipOf.length - 1;
  };
  const inside = new Uint8Array(N);
  for (let j = 0; j < VZ; j++) for (let i = 0; i < VX; i++) {
    const gi = i - 1, gj = j - 1, edge = gi < 0 || gj < 0 || gi > nx || gj > nz;
    const x = Math.min(x1, Math.max(x0, x0 + gi * res)), z = Math.min(z1, Math.max(z0, z0 + gj * res));
    inside[addPoint(x, z, edge ? 1 : 0)] = solidAt(x, z) ? 1 : 0;
  }
  // where a grid edge leaves the ground: the exact crossing, found by halving, shared by both sides
  const cuts = new Map<number, number>();
  const cut = (s: number, v: number): number => {
    const key = s < v ? s * N + v : v * N + s;
    const hit = cuts.get(key);
    if (hit !== undefined) return hit;
    const sx = pos[s * 3], sz = pos[s * 3 + 2], dx = pos[v * 3] - sx, dz = pos[v * 3 + 2] - sz;
    let lo = 0, hi = 1;
    for (let k = 0; k < 10; k++) { const m = (lo + hi) / 2; if (solidAt(sx + dx * m, sz + dz * m)) lo = m; else hi = m; }
    const idx = addPoint(sx + dx * lo, sz + dz * lo, lipOf[s] + (lipOf[v] - lipOf[s]) * lo);
    cuts.set(key, idx);
    return idx;
  };
  const tris: number[] = [];
  const skirt = (p: number, q: number, inner: number) => {
    if (lipOf[p] > 0 || lipOf[q] > 0) return; // the board's outer ring already drops its own lip
    const dx = pos[q * 3] - pos[p * 3], dz = pos[q * 3 + 2] - pos[p * 3 + 2];
    // the face must look out into the void, away from the ground it hangs from
    if (dz * (pos[inner * 3] - pos[p * 3]) - dx * (pos[inner * 3 + 2] - pos[p * 3 + 2]) > 0) [p, q] = [q, p];
    const base = lipOf.length;
    for (const [v, drop] of [[p, 0], [q, 0], [q, EDGE_LIP], [p, EDGE_LIP]]) {
      pos.push(pos[v * 3], pos[v * 3 + 1] - drop, pos[v * 3 + 2]);
      for (let k = 0; k < STRIDE; k++) spl.push(spl[v * STRIDE + k]);
      lipOf.push(1);
    }
    tris.push(base, base + 1, base + 2, base, base + 2, base + 3);
  };
  const clip = (t: number[]) => {
    const n = inside[t[0]] + inside[t[1]] + inside[t[2]];
    if (n === 3) { tris.push(t[0], t[1], t[2]); return; }
    if (n === 0) return;
    const poly: number[] = [], edge: number[] = [];
    for (let k = 0; k < 3; k++) {
      const p = t[k], q = t[(k + 1) % 3];
      if (inside[p]) poly.push(p);
      if (inside[p] !== inside[q]) { const c = inside[p] ? cut(p, q) : cut(q, p); poly.push(c); edge.push(c); }
    }
    for (let k = 1; k + 1 < poly.length; k++) tris.push(poly[0], poly[k], poly[k + 1]);
    skirt(edge[0], edge[1], t.find(v => inside[v])!);
  };
  for (let j = 0; j < VZ - 1; j++) for (let i = 0; i < VX - 1; i++) {
    const a = j * VX + i, bb = a + 1, c = a + VX, d = c + 1;
    clip([a, c, bb]); clip([bb, c, d]);
  }
  return finish(new Float32Array(pos), new Float32Array(spl), new Uint32Array(tris), null);
}

/** Hard look: crisp hex cliffs at every height step, seamless tops. */
export function buildHardGround(b: Board): THREE.BufferGeometry {
  const L = b.layout, pos: number[] = [], nor: number[] = [], spl: number[] = [];
  const tmp = new Float32Array(STRIDE);
  const face = (a: number[], bb: number[], c: number[]) => {
    const ux = bb[0] - a[0], uy = bb[1] - a[1], uz = bb[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
    const nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx, l = Math.hypot(nx, ny, nz) || 1;
    for (const p of [a, bb, c]) {
      pos.push(p[0], p[1], p[2]); nor.push(nx / l, ny / l, nz / l);
      splat(b, L, p[0], p[2], 0.35, tmp, 0); for (let k = 0; k < STRIDE; k++) spl.push(tmp[k]);
    }
  };
  const quad = (a0: number[], b0: number[], b1: number[], a1: number[]) => { face(a0, b0, b1); face(a0, b1, a1); };
  const mid = (p: number[], q: number[]) => [(p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2];
  for (const cell of b.cells) {
    const y1 = cell.groundY, C = [cell.x, y1, cell.z];
    const nbs = neighbors(L, cell.c, cell.r).map(([c, r]) => b.cell(c, r));
    for (let k = 0; k < 6; k++) {
      const [ax, az] = corner(cell.x, cell.z, k), [bx, bz] = corner(cell.x, cell.z, k + 1);
      const A = [ax, y1, az], Bc = [bx, y1, bz], mA = mid(C, A), mB = mid(C, Bc), mAB = mid(A, Bc);
      face(C, mA, mB); face(mA, A, mAB); face(mA, mAB, mB); face(mB, mAB, Bc);
      const na = ((k + 0.5) * Math.PI) / 3, cx = cell.x + SQ3 * Math.sin(na), cz = cell.z + SQ3 * Math.cos(na);
      const nb = nbs.find(o => Math.abs(o.x - cx) < 0.1 && Math.abs(o.z - cz) < 0.1);
      const y0 = nb ? nb.groundY : 0;
      if (y1 > y0) quad([ax, y0, az], [bx, y0, bz], Bc, A);
    }
  }
  const p = new Float32Array(pos), s = new Float32Array(spl);
  return finish(p, s, null, new Float32Array(nor));
}

/** Height of the ground at a point, for placing things on it in the given look. */
export function groundHeightAt(b: Board, style: EdgeStyle, x: number, z: number): number {
  if (style === 'hard') return cellAt(b, b.layout, x, z).groundY;
  const [wx, wz] = warp(x, z);
  return cellAt(b, b.layout, wx, wz).groundY + terrainReliefAt(b, x, z);
}

/** Water surface: one sheet that follows the shore of the active look; hidden under dry land. */
export function buildWater(b: Board, style: EdgeStyle): THREE.BufferGeometry | null {
  if (!b.hasWater) return null;
  const L = b.layout, { x0, x1, z0, z1 } = b.bounds();
  const res = Math.max(0.25, Math.sqrt(((x1 - x0) * (z1 - z0)) / 60000));
  const nx = Math.ceil((x1 - x0) / res), nz = Math.ceil((z1 - z0) / res), VX = nx + 1, VZ = nz + 1;
  const pos = new Float32Array(VX * VZ * 3);
  // Under dry land the sheet used to sit just under the ground everywhere: on a tall bank or block
  // it climbed the cliff with it, and its coarser grid poked out through the cliff faces as blue
  // slivers. Now it only rises to meet the bank beside water (capped at that water's level), and
  // everywhere else sinks below all the ground.
  let lowestGround = Infinity;
  for (const c of b.cells) lowestGround = Math.min(lowestGround, c.groundY);
  const nearWater = new Map<Cell, number>();
  for (const c of b.cells) {
    let level = -Infinity;
    for (const o of [c, ...neighbors(L, c.c, c.r).map(([nc, nr]) => b.cell(nc, nr))]) if (o?.water) level = Math.max(level, o.waterY);
    if (level > -Infinity) nearWater.set(c, level);
  }
  for (let j = 0; j < VZ; j++) for (let i = 0; i < VX; i++) {
    const x = Math.min(x1, x0 + i * res), z = Math.min(z1, z0 + j * res);
    const [sx, sz] = style === 'smooth' ? warp(x, z) : [x, z];
    const cell = cellAt(b, L, sx, sz), v = j * VX + i;
    const shore = nearWater.get(cell);
    pos[v * 3] = x; pos[v * 3 + 2] = z;
    pos[v * 3 + 1] = cell.water ? cell.waterY : shore !== undefined ? Math.min(cell.groundY - 0.35, shore + 0.05) : lowestGround - 1;
  }
  const index = new Uint32Array(nx * nz * 6);
  let t = 0;
  for (let j = 0; j < nz; j++) for (let i = 0; i < nx; i++) {
    const a = j * VX + i, bb = a + 1, c = a + VX, d = c + 1;
    const x = (pos[a * 3] + pos[d * 3]) / 2, z = (pos[a * 3 + 2] + pos[d * 3 + 2]) / 2;
    const [sx, sz] = style === 'smooth' ? warp(x, z) : [x, z];
    const cell = cellAt(b, L, sx, sz);
    // only real water and the banks beside it: never a sheet under dry land
    if (cell.surface < 0 || (!cell.water && !nearWater.has(cell))) continue;
    index[t++] = a; index[t++] = c; index[t++] = bb; index[t++] = bb; index[t++] = c; index[t++] = d;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  const uv = new Float32Array(VX * VZ * 2);
  for (let v = 0; v < VX * VZ; v++) { uv[v * 2] = pos[v * 3] / 4; uv[v * 2 + 1] = pos[v * 3 + 2] / 4; }
  g.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  g.setIndex(new THREE.BufferAttribute(index.subarray(0, t), 1));
  g.computeVertexNormals();
  return g;
}

/** Re-export so callers needn't import the hex module just for a step size. */
export { STEP };
