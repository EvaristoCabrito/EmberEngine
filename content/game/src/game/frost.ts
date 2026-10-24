import type { Point } from "./types.ts";
import { cubeRound, cubeToOddr, hexDist, oddrToCube } from "./pathfinding.ts";
export const FROST = {
  name: "Frost",
  unlockLevel: 5,
  tier: 2,
  baseLength: 2,
  levelsPerHex: 4,
  mul: 1.1,
  faces: 6,
} as const;
export function frostPower(level: number) {
  const lv = Math.max(1, Math.min(30, Math.floor(level)));
  return {
    length: 2 + Math.floor(Math.max(0, lv - 5) / 4),
    dice: 1 + Math.floor(Math.max(0, lv - 5) / 6),
    faces: 6,
    mul: FROST.mul,
  };
}
export function frostCharges(level: number) {
  return 1 + Number(level >= 5) + Number(level >= 12) + Number(level >= 20);
}
/** One hex wide, starting next to the caster and running straight toward the aimed hex in any
 *  direction (not only the six hex axes), its length set by the caster's level. */
export function frostAreaTiles(
  origin: Point,
  through: Point,
  level: number,
  cols: number,
  rows: number,
): Point[] {
  const n = hexDist(origin, through);
  if (!n) return [];
  // a hair off-centre, so a line passing exactly between two hexes always picks the same side
  const A = oddrToCube(origin.x, origin.y), B = oddrToCube(through.x, through.y);
  const aq = A.q + 1e-6, ar = A.r + 2e-6, as = A.s - 3e-6;
  const out: Point[] = [];
  for (let i = 1; i <= frostPower(level).length; i++) {
    const t = i / n, c = cubeRound(aq + (B.q - A.q) * t, ar + (B.r - A.r) * t, as + (B.s - A.s) * t);
    const p = cubeToOddr(c.q, c.r);
    if (p.x < 0 || p.y < 0 || p.x >= cols || p.y >= rows) break;
    out.push(p);
  }
  return out;
}
