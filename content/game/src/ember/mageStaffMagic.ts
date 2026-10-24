import type { Resistances, ResistanceElement } from "./types";

/** Passive magic belongs to the equipped staff; it never spends class spell charges. */
export interface MageStaffMagic {
  attackElement?: ResistanceElement;
  magBonus?: number;
  dexBonus?: number;
  resistances?: Resistances;
  elementalBonusPct?: Partial<Record<ResistanceElement, number>>;
  regeneration?: number;
  lifeStealPct?: number;
}

/** Former Healer staffs keep their artwork, IDs and damage dice, and now belong to mages. */
export const MAGE_STAFF_MAGIC: Readonly<Record<string, MageStaffMagic>> = {
  "cajado-da-renovacao": { magBonus: 1, regeneration: 2, resistances: { ice: 10 } },
  "cajado-da-esperanca": { attackElement: "holy", regeneration: 3, resistances: { darkness: 15 }, elementalBonusPct: { holy: 10 } },
  "cajado-da-graca": { magBonus: 2, dexBonus: 2, resistances: { holy: 15 }, elementalBonusPct: { arcane: 10 } },
  "cetro-da-luz": { attackElement: "holy", magBonus: 2, resistances: { darkness: 20 }, elementalBonusPct: { holy: 20 } },
  "bastao-da-purificacao": { attackElement: "holy", regeneration: 2, resistances: { poison: 25, fire: 10 }, elementalBonusPct: { holy: 10 } },
  "cajado-do-bispo": { attackElement: "holy", magBonus: 3, resistances: { holy: 25, darkness: 15 }, elementalBonusPct: { holy: 15, arcane: 10 } },
  "cajado-da-comunhao": { magBonus: 3, lifeStealPct: 10, resistances: { arcane: 20 }, elementalBonusPct: { arcane: 10 } },
  "cajado-da-fe": { attackElement: "holy", magBonus: 4, resistances: { darkness: 30, holy: 25, ember: 15 }, elementalBonusPct: { holy: 15 } },
  "cajado-da-justica": { attackElement: "holy", magBonus: 5, resistances: { darkness: 25, lightning: 20 }, elementalBonusPct: { holy: 25, arcane: 15 } },
  "bastao-purificacao-sombrio": { attackElement: "darkness", magBonus: 2, lifeStealPct: 10, resistances: { poison: 20, darkness: 20 }, elementalBonusPct: { darkness: 20 } },
  "cajado-da-vinha": { attackElement: "poison", regeneration: 2, resistances: { poison: 15 }, elementalBonusPct: { poison: 10 } },
  "cajado-da-galhada": { magBonus: 1, regeneration: 1, resistances: { poison: 10, ice: 10 } },
  "cajado-da-trepadeira": { attackElement: "poison", regeneration: 2, resistances: { poison: 20 }, elementalBonusPct: { poison: 15 } },
};

export function staffElementalDamage(baseDamage: number, magic: MageStaffMagic | undefined, element: ResistanceElement): number {
  return baseDamage * (1 + (magic?.elementalBonusPct?.[element] ?? 0) / 100);
}

/** Actual damage is capped to the victim's remaining HP before this is called. */
export function staffLifeSteal(actualDamage: number, missingHp: number, magic: MageStaffMagic | undefined): number {
  return Math.max(0, Math.min(missingHp, Math.floor(actualDamage * (magic?.lifeStealPct ?? 0) / 100)));
}
