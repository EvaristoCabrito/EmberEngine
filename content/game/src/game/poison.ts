import type { PoisonTier } from "./types";

/** Every poison tier: the dice it rolls each tick, and how many points it strips off the
 * target's Poison Resistance (Resistência a Veneno skill) before anything else. */
export const POISON_TIERS: Record<PoisonTier, { name: string; dice: number; faces: number; penalty: number }> = {
  lesser: { name: "Veneno Menor", dice: 1, faces: 4, penalty: 0 },
  poison: { name: "Veneno", dice: 1, faces: 10, penalty: 10 },
  greater: { name: "Veneno Maior", dice: 1, faces: 10, penalty: 20 },
  deadly: { name: "Veneno Mortal", dice: 2, faces: 8, penalty: 40 },
  lethal: { name: "Veneno Letal", dice: 3, faces: 6, penalty: 50 },
};

const POISON_TIER_ORDER: PoisonTier[] = ["lesser", "poison", "greater", "deadly", "lethal"];

/** Floor of the effective resistance: at −50% a poison deals 150% of its rolled damage. */
export const POISON_RESIST_MIN = -50;
export const POISON_RESIST_MAX = 100;

/** Effective Poison Resistance = clamp(PR − tier penalty − attacker MAG / 2, −50, 100).
 * Every point counts as 1%; below zero the poison hits harder than its dice. */
export function effectivePoisonResistance(poisonResist: number, tier: PoisonTier, mag: number): number {
  return Math.max(POISON_RESIST_MIN, Math.min(POISON_RESIST_MAX, poisonResist - POISON_TIERS[tier].penalty - mag / 2));
}

/** Percentage chance to apply poison on a landed hit: 100 − effective resistance, 0–100. */
export function poisonChance(effectiveResist: number): number {
  return Math.max(0, Math.min(100, 100 - effectiveResist));
}

/** Final tick damage = rolled poison damage × (1 − effective resistance / 100), rounded. */
export function poisonTickDamage(rolled: number, effectiveResist: number): number {
  return Math.max(0, Math.round(rolled * (1 - effectiveResist / 100)));
}

/** A new poison never downgrades one already in the target — the stronger tier stays. */
export function strongerPoison(current: PoisonTier | undefined, incoming: PoisonTier): PoisonTier {
  if (!current) return incoming;
  return POISON_TIER_ORDER.indexOf(current) >= POISON_TIER_ORDER.indexOf(incoming) ? current : incoming;
}

/** "1D4", "2D8", … for a tier's tick. */
export function poisonDice(tier: PoisonTier): string {
  const t = POISON_TIERS[tier];
  return `${t.dice}D${t.faces}`;
}

/** Reads a saved poison value. Older saves stored `true`/4 (Lesser) or 10 (old 1D10 = Poison). */
export function poisonTierOf(value: unknown): PoisonTier | undefined {
  if (value === true || value === 4) return "lesser";
  if (value === 10) return "poison";
  return typeof value === "string" && (POISON_TIER_ORDER as string[]).includes(value) ? (value as PoisonTier) : undefined;
}
