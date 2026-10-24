/**
 * Enmity (aggro), after Final Fantasy XI. Every enemy keeps a score for each hero/ally in two
 * parts:
 *  - cumulative (CE): builds slowly, lasts; worn down when that enemy hurts the hero.
 *  - volatile (VE): builds fast, drains every round.
 * An enemy targets whoever has the highest CE + VE on its own table; an enemy with an empty
 * table keeps its usual targeting. All tuning lives here.
 */
export const ENMITY = {
  /** Per point of damage dealt with a weapon (attacks, counters, weapon skills). */
  weapon: { ce: 1, ve: 3 },
  /** Per point of spell damage — magic draws more attention than a sword. */
  spell: { ce: 1.5, ve: 6 },
  /** Per HP healed, on every enemy that is aware of the party. */
  heal: { ce: 0.5, ve: 3 },
  /** Flat, per use of a support/status skill (buffs, debuffs, summons, cures), on every aware enemy. */
  support: { ce: 20, ve: 120 },
  /** Provoke, on each enemy it reaches. */
  provoke: { ce: 1, ve: 1800 },
  /** Cumulative enmity a hero loses with an enemy per point of damage that enemy deals them. */
  damageTakenCe: 2,
  /** Volatile enmity drained from every entry at the start of each round. */
  volatileDecayPerRound: 300,
  /** Neither part grows past this. */
  cap: 10000,
} as const;

export type EnmityEntry = { ce: number; ve: number };
/** enemy unit id → (hero unit id → entry). */
export type EnmityTable = Map<string, Map<string, EnmityEntry>>;

export function enmityTotal(entry: EnmityEntry | undefined): number {
  return entry ? entry.ce + entry.ve : 0;
}

export function addToEntry(entry: EnmityEntry | undefined, ce: number, ve: number): EnmityEntry {
  return {
    ce: Math.max(0, Math.min(ENMITY.cap, (entry?.ce ?? 0) + ce)),
    ve: Math.max(0, Math.min(ENMITY.cap, (entry?.ve ?? 0) + ve)),
  };
}

/** Snapshot form: { enemyId: { heroId: [ce, ve] } }. */
export type EnmitySnapshot = Record<string, Record<string, [number, number]>>;

export function enmityToSnapshot(table: EnmityTable): EnmitySnapshot {
  const out: EnmitySnapshot = {};
  for (const [enemy, row] of table) {
    const entries = [...row].filter(([, e]) => e.ce > 0 || e.ve > 0);
    if (entries.length) out[enemy] = Object.fromEntries(entries.map(([hero, e]) => [hero, [Math.round(e.ce * 10) / 10, Math.round(e.ve * 10) / 10]]));
  }
  return out;
}

export function enmityFromSnapshot(raw: unknown): EnmityTable {
  const table: EnmityTable = new Map();
  if (!raw || typeof raw !== "object") return table;
  for (const [enemy, row] of Object.entries(raw as Record<string, unknown>)) {
    if (!row || typeof row !== "object") continue;
    const entries = new Map<string, EnmityEntry>();
    for (const [hero, pair] of Object.entries(row as Record<string, unknown>)) {
      if (!Array.isArray(pair) || pair.length !== 2) continue;
      const [ce, ve] = pair.map(Number);
      if (Number.isFinite(ce) && Number.isFinite(ve)) entries.set(hero, addToEntry(undefined, ce!, ve!));
    }
    if (entries.size) table.set(enemy, entries);
  }
  return table;
}
