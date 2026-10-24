import { AFFINITY_HEROES, type AffinityHero } from "./affinity.ts";
import type { Mission } from "./types";

/** The Party menu's formation board: a radius-2 hex (rows of 3-4-5-4-3), so a tank can take the
 * true center cell (index 9). Cells run front to back, left to right. */
export const FORMATION_SLOTS = 19;

/** Board cells front to back, left to right: a hero name, or "" for an empty cell. Heroes are
 * deduped; empty cells are kept (so a hero can sit in any cell) only as many as fit the board. */
export function cleanPartyFormation(raw: unknown): string[] {
  if (!Array.isArray(raw)) return [];
  const heroes = new Set<string>();
  for (const v of raw) if (typeof v === "string" && AFFINITY_HEROES.includes(v as typeof AFFINITY_HEROES[number])) heroes.add(v);
  let empties = Math.max(0, FORMATION_SLOTS - heroes.size);
  const seen = new Set<string>();
  const out: string[] = [];
  for (const v of raw) {
    if (v === "" && empties > 0) { out.push(""); empties--; }
    else if (typeof v === "string" && heroes.has(v) && !seen.has(v)) { seen.add(v); out.push(v); }
  }
  while (out.length && out[out.length - 1] === "") out.pop();
  return out;
}

/** The chosen party leader (Party menu), or undefined when none/invalid was saved. */
export function cleanPartyLeader(raw: unknown): AffinityHero | undefined {
  return typeof raw === "string" && AFFINITY_HEROES.includes(raw as AffinityHero) ? (raw as AffinityHero) : undefined;
}

/** Who walks the world map and free-roam maps: the saved leader while they are in the party,
 * otherwise Kael. */
export function partyLeaderOf(raw: unknown, recruited: (hero: string) => boolean): AffinityHero {
  const hero = cleanPartyLeader(raw);
  return hero && recruited(hero) ? hero : "Kael";
}

/** Hex distance on the odd-r grid (same math as pathfinding.ts's hexDist, kept local so this
 * module stays dependency-free for its node tests). */
function hexDistance(a: { x: number; y: number }, b: { x: number; y: number }): number {
  const aq = a.x - (a.y - (a.y & 1)) / 2, bq = b.x - (b.y - (b.y & 1)) / 2;
  return (Math.abs(aq - bq) + Math.abs(a.y - b.y) + Math.abs(-aq - a.y + bq + b.y)) / 2;
}

/** Assign heroes to the map's existing starting slots; never invent new hexes. Formation slot 1
 * is the start hex closest to the enemy (the Party menu's front row), the last slot the
 * farthest (its back row). A map without enemies keeps its authored slot order. */
export function applyPartyFormation(mission: Mission, raw: unknown, protectedStart = false): Mission {
  const order = cleanPartyFormation(raw);
  if (!order.some(Boolean) || protectedStart || mission.lockPartyFormation || mission.explore) return mission;
  const heroes = mission.playerSpawns.filter(spawn => AFFINITY_HEROES.includes(spawn.name as typeof AFFINITY_HEROES[number]));
  const ranked = [...heroes].sort((a, b) => {
    const ai = order.indexOf(a.name), bi = order.indexOf(b.name);
    return (ai < 0 ? order.length : ai) - (bi < 0 ? order.length : bi);
  });
  const enemies = mission.enemySpawns ?? [];
  const threat = (p: { x: number; y: number }) => (enemies.length ? Math.min(...enemies.map(e => hexDistance(p, e))) : 0);
  const slots = heroes.map((hero, i) => ({ x: hero.x, y: hero.y, i, d: threat(hero) })).sort((a, b) => a.d - b.d || a.i - b.i);
  const positions = new Map(ranked.map((hero, i) => [hero.name, { x: slots[i].x, y: slots[i].y }]));
  return { ...mission, playerSpawns: mission.playerSpawns.map(spawn => positions.has(spawn.name) ? { ...spawn, ...positions.get(spawn.name)! } : spawn) };
}
