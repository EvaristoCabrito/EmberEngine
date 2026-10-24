import type { WeaponDef } from "./types";

/** Additive Healer quarterstaff family. Uses Ember's nine existing damage/price rungs;
 * levels describe the campaign progression, without adding equipment level restrictions. */
export const HEALER_BLUNT_STAFF_TIERS = [
  { id: "bastao-curandeiro-01-freixo", name: "Bastão de Freixo", recommendedLevel: 1, dice: 1, faces: 4, price: 40 },
  { id: "bastao-curandeiro-02-carvalho", name: "Bastão de Carvalho Reforçado", recommendedLevel: 4, dice: 1, faces: 6, price: 90 },
  { id: "bastao-curandeiro-03-bronze", name: "Bastão de Bronze", recommendedLevel: 8, dice: 1, faces: 8, price: 180 },
  { id: "bastao-curandeiro-04-aco", name: "Bastão de Aço", recommendedLevel: 11, dice: 1, faces: 10, price: 320 },
  { id: "bastao-curandeiro-05-ebano", name: "Bastão de Ébano Ferrado", recommendedLevel: 15, dice: 1, faces: 12, price: 520 },
  { id: "bastao-curandeiro-06-ferro-gravado", name: "Bastão de Ferro Gravado", recommendedLevel: 19, dice: 2, faces: 6, price: 800 },
  { id: "bastao-curandeiro-07-prata", name: "Bastão de Prata Envelhecida", recommendedLevel: 22, dice: 2, faces: 8, price: 1200 },
  { id: "bastao-curandeiro-08-aco-negro", name: "Bastão de Aço Negro", recommendedLevel: 26, dice: 2, faces: 10, price: 1800 },
  { id: "bastao-curandeiro-09-mestre", name: "Bastão do Mestre Curandeiro", recommendedLevel: 30, dice: 2, faces: 12, price: 2600 },
] as const;

/** Upper-tier reinforced alternatives; the original nine entries and starter remain intact. */
export const HEALER_BLUNT_STAFF_ELITE = [
  { id: "bastao-curandeiro-10-carvalho-blindado", name: "Bastão de Carvalho Blindado", recommendedLevel: 20, dice: 2, faces: 6, bonus: 1, price: 860 },
  { id: "bastao-curandeiro-11-ferro-rivetado", name: "Bastão de Ferro Rivetado", recommendedLevel: 21, dice: 2, faces: 6, bonus: 2, price: 920 },
  { id: "bastao-curandeiro-12-bronze-martelado", name: "Bastão de Bronze Martelado", recommendedLevel: 23, dice: 2, faces: 8, bonus: 1, price: 1260 },
  { id: "bastao-curandeiro-13-aco-canelado", name: "Bastão de Aço Canelado", recommendedLevel: 24, dice: 2, faces: 8, bonus: 2, price: 1320 },
  { id: "bastao-curandeiro-14-ebano-encouracado", name: "Bastão de Ébano Encouraçado", recommendedLevel: 26, dice: 2, faces: 10, bonus: 1, price: 1860 },
  { id: "bastao-curandeiro-15-prata-forjada", name: "Bastão de Prata Forjada", recommendedLevel: 27, dice: 2, faces: 10, bonus: 2, price: 1920 },
  { id: "bastao-curandeiro-16-aco-damasco", name: "Bastão de Aço Damasco", recommendedLevel: 29, dice: 2, faces: 12, bonus: 1, price: 2660 },
  { id: "bastao-curandeiro-17-mestre-relicario", name: "Bastão Relicário do Mestre", recommendedLevel: 30, dice: 2, faces: 12, bonus: 2, price: 2720 },
] as const;

export const HEALER_BLUNT_STAFF_ALL = [...HEALER_BLUNT_STAFF_TIERS, ...HEALER_BLUNT_STAFF_ELITE] as const;

export const HEALER_BLUNT_STAFF_WEAPONS: Record<string, WeaponDef> = Object.fromEntries(
  HEALER_BLUNT_STAFF_ALL.map(tier => [tier.id, {
    ...tier, weaponType: "staff", usableBy: ["healer", "salazar", "bishop", "cleric"],
    bonus: "bonus" in tier ? tier.bonus : 0, minRange: 1, maxRange: 1,
  } satisfies WeaponDef]),
);
