// Ember's hex layout: pointy-top hexes, odd rows shifted half a hex to the right
// (same as Ember's editor: cx = (x + 0.5 * (y & 1) + 0.5) * √3·R, cy = (1.5y + 1)·R).
// In 3D, columns run along +x and rows along +z; the board is centred on the origin.

export const R = 1;
export const SQ3 = Math.sqrt(3);
/** World height of one elevation level (Ember's own 3D view used 1.5; 0.5 made terraces ankle-high
 * next to Ember's 1.53-hex-width humans). */
export const STEP = 1.0;

export interface HexLayout {
  cols: number;
  rows: number;
  /** Half extents of the hex-centre rectangle, used to centre the board. */
  ox: number;
  oz: number;
}

export function makeLayout(cols: number, rows: number): HexLayout {
  return { cols, rows, ox: (SQ3 * R * (cols - 0.5)) / 2, oz: 0.75 * R * (rows - 1) };
}

export const hexX = (L: HexLayout, c: number, r: number): number => SQ3 * R * (c + 0.5 * (r & 1)) - L.ox;
export const hexZ = (L: HexLayout, r: number): number => 1.5 * R * r - L.oz;
export const indexOf = (L: HexLayout, c: number, r: number): number => r * L.cols + c;
export const inBoard = (L: HexLayout, c: number, r: number): boolean => c >= 0 && r >= 0 && c < L.cols && r < L.rows;

/** Logical hex under a world point (clamped to the board). Returns [col, row]. */
export function hexAt(L: HexLayout, x: number, z: number): [number, number] {
  const rf = (z + L.oz) / (1.5 * R), qf = (x + L.ox) / (SQ3 * R) - rf / 2, sf = -qf - rf;
  let q = Math.round(qf), r = Math.round(rf);
  const s = Math.round(sf);
  const dq = Math.abs(q - qf), dr = Math.abs(r - rf), ds = Math.abs(s - sf);
  if (dq > dr && dq > ds) q = -r - s;
  else if (dr > ds) r = -q - s;
  const c = q + (r - (r & 1)) / 2;
  return [Math.max(0, Math.min(L.cols - 1, c)), Math.max(0, Math.min(L.rows - 1, r))];
}

export function neighbors(L: HexLayout, c: number, r: number): [number, number][] {
  const o = r & 1
    ? [[1, 0], [-1, 0], [1, -1], [0, -1], [1, 1], [0, 1]]
    : [[1, 0], [-1, 0], [0, -1], [-1, -1], [0, 1], [-1, 1]];
  return o.map(([dc, dr]) => [c + dc, r + dr] as [number, number]).filter(([a, b]) => inBoard(L, a, b));
}

/** Corner k (0-5) of a hex of radius rr: corner 0 points to +z, going clockwise seen from above. */
export function corner(cx: number, cz: number, k: number, rr = R): [number, number] {
  const a = (k * Math.PI) / 3;
  return [cx + rr * Math.sin(a), cz + rr * Math.cos(a)];
}
