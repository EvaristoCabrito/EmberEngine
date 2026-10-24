/** Subtract defender DEX from the complete accuracy score, then clamp once. */
export function dexAccuracy(baseAccuracy: number, defenderDex: number): number {
  return Math.max(0, Math.min(100, baseAccuracy - defenderDex));
}

/** Escape uses the escaping character's own DEX, retaining the exact division. */
export function dexEscapeChance(runnerDex: number): number {
  return Math.min(100, 60 + runnerDex / 3);
}

/** Legacy RES values and points become DEX; explicit DEX takes precedence. */
export function savedDex(source: Record<string, unknown>): unknown {
  return source.dex ?? source.res;
}
