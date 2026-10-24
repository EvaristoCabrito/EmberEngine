import type { WeaponType } from "../ember/types.ts";

export const WEAPON_TYPES = ["sword", "axe", "mace", "hammer", "staff", "spear", "bow", "crossbow", "dagger"] as const;
export const WEAPON_TYPE_LABELS: Record<WeaponType, string> = {
  sword: "Swords", axe: "Axes", mace: "Maces", hammer: "Hammers", staff: "Staves",
  spear: "Spears & Polearms", bow: "Bows", crossbow: "Crossbows", dagger: "Daggers",
};

export function weaponSkillAccuracy(value: number): number {
  return 75 + cleanWeaponSkill(value);
}

export function weaponSkillDamageMultiplier(value: number): number {
  return (100 + cleanWeaponSkill(value)) / 100;
}

/** Preserve small combat gains; old draft fractions can still be rounded up during migration. */
export function cleanWeaponSkill(value: unknown, roundLegacyUp = false): number {
  if (typeof value !== "number" || !Number.isFinite(value)) return 0;
  const bounded = Math.max(0, Math.min(100, value));
  return roundLegacyUp ? Math.ceil(bounded) : bounded;
}
