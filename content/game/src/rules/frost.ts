import type { Point } from "../ember/types.ts";
import { axisDir, hexLine, hexRay } from "./pathfinding.ts";
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
/** One hex wide: adjacent to the caster, extending along one of the six exact hex axes. */
export function frostAreaTiles(
  origin: Point,
  through: Point,
  level: number,
  cols: number,
  rows: number,
): Point[] {
  const first = hexLine(origin, through)[1];
  const direction = first ? axisDir(origin, first) : null;
  return direction ? hexRay(origin, direction, cols, rows).slice(0, frostPower(level).length) : [];
}

