import { engineRoot } from './engineRoot';
// Engine2's Inn (src/game/InnScreen.tsx and GameApp's Inn handlers) as data for EmberEngine: prices, the potions and
// the lockpick as the Adega sells them, and every weapon and piece of gear with what Vargan's smith shows of it (icon,
// dice, range, tooltip, price, enhancement costs, sell value per enhancement), read-only from Engine2's own modules.
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_inn.ts -- <out.inn.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = engineRoot;
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_inn.ts -- <out.inn.json>');

const data = await import(`${E2}/src/ember/data.ts`);
const hunger = await import(`${E2}/src/campaign/hunger.ts`);
// InnScreen.tsx itself is a React module (UI imports); its three exported prices are read from its source text.
const innSource = readFileSync(`${E2}/src/game/InnScreen.tsx`, 'utf8');
const innPrice = (name: string) => Number(new RegExp('export const ' + name + ' = ([0-9]+);').exec(innSource)?.[1]);
const inn = { NIGHT_REST_PRICE: innPrice('NIGHT_REST_PRICE'), HEALER_CAST_PRICE: innPrice('HEALER_CAST_PRICE'), HEALER_AILMENT_PRICE: innPrice('HEALER_AILMENT_PRICE') };

type Weapon = { id: string; name: string; price: number; questOnly?: boolean; usableBy: string[]; recommendedLevel?: number; bonusClass?: string; magic?: unknown; twoHanded?: boolean };
type Gear = { id: string; name: string; price?: number; slot: string; usableBy?: string[]; hp?: number; atk?: number; mag?: number; def?: number; dex?: number; mov?: number; kind?: string; dmgMul?: number; dice?: number; faces?: number };
const maxEnh = data.WEAPON_MAX_ENH as number;
const heroClasses = [...new Set(['swordsman', 'archer', 'mage', 'healer', 'aldric', 'conjurer', ...Object.values(data.PROMOTIONS as Record<string, string[]>).flat()])]
  .filter((c) => (data.CLASSES as Record<string, unknown>)[c]);
const weapons = Object.values(data.WEAPONS as Record<string, Weapon>).map((w) => ({
  id: w.id, name: w.name, price: w.price, ...(w.questOnly ? { questOnly: true } : {}), usableBy: w.usableBy, recommendedLevel: w.recommendedLevel ?? null,
  bonusClass: w.bonusClass && data.isPlayableClassForDisplay(w.bonusClass) ? w.bonusClass : null,
  bonusClassName: w.bonusClass && data.isPlayableClassForDisplay(w.bonusClass) ? (data.CLASSES as Record<string, { name: string }>)[w.bonusClass]?.name ?? null : null,
  magic: w.magic ? data.weaponMagicSummary(w) : null,
  magBonus: (w.magic as { magBonus?: number } | undefined)?.magBonus ?? 0, dexBonus: (w.magic as { dexBonus?: number } | undefined)?.dexBonus ?? 0,
  twoHanded: !!w.twoHanded, power: data.weaponPower(w), dice: data.weaponDiceLabel(w.id), range: data.weaponRangeLabel(w.id), icon: data.weaponIcon(w.id),
  tooltip: Array.from({ length: maxEnh + 1 }, (_, e) => data.weaponTooltip(w, e)),
  sell: Array.from({ length: maxEnh + 1 }, (_, e) => data.weaponSellValue(w.id, e)),
}));
const equipment = Object.values(data.EQUIPMENT as Record<string, Gear>).map((e) => ({
  id: e.id, name: e.name, price: e.price ?? 0, slot: e.slot, usableBy: e.usableBy ?? null, hp: e.hp ?? 0, atk: e.atk ?? 0, mag: e.mag ?? 0, def: e.def ?? 0, dex: e.dex ?? 0, mov: e.mov ?? 0, pouch: data.isPouch(e.id),
  typeSlot: data.equipmentTypeSlotName(e), tooltip: data.equipmentTooltip(e), icon: data.equipmentIcon(e.id),
  kind: e.kind ?? null, dmgMul: e.dmgMul ?? null, dice: e.dice ?? null, faces: e.faces ?? null, summary: data.equipmentStatSummary(e),
}));
const potionIds = Object.keys(data.POTIONS);
const potions = potionIds.map((id) => ({
  id, label: data.potionLabel(id), tooltip: data.potionTooltip(id), effect: (data.POTIONS as Record<string, { effect: string }>)[id].effect,
  dice: (data.POTIONS as Record<string, { dice: number }>)[id].dice, faces: (data.POTIONS as Record<string, { faces: number }>)[id].faces,
  bonus: (data.POTIONS as Record<string, { bonus: number }>)[id].bonus, manaRestore: (data.POTIONS as Record<string, { manaRestore?: number }>)[id].manaRestore ?? 0, price: (data.POTION_PRICE as Record<string, number>)[id],
  carryMax: (data.POTION_CARRY_MAX as Record<string, number>)[id],
}));

const out = {
  prices: {
    meal: hunger.INN_MEAL_PRICE, innFullness: hunger.INN_FULLNESS, rest: inn.NIGHT_REST_PRICE, healerCast: inn.HEALER_CAST_PRICE,
    healerAilment: inn.HEALER_AILMENT_PRICE, lockpick: data.LOCKPICK_PRICE, rations: data.RATIONS_PRICE,
  },
  bagMax: data.BAG_MAX, rationStackMax: data.RATION_STACK_MAX, weaponMaxEnh: maxEnh,
  enhCost: Array.from({ length: maxEnh }, (_, i) => data.weaponEnhCost(i + 1)),
  potions, lockpickTooltip: data.lockpickTooltip(), rationsIcon: data.RATIONS_ICON, bagIcon: data.pouchIcon(null),
  weapons, equipment,
  equipmentSlots: data.EQUIPMENT_SLOTS,
  // tierUses(class, tier, level) for every hero class (base and promoted): [class][level - 1][tier - 1].
  tierUses: Object.fromEntries(heroClasses.map((c) => [c, Array.from({ length: data.MAX_LEVEL as number }, (_, l) => Array.from({ length: 10 }, (_, t) => data.tierUses(c, t + 1, l + 1)))])),
  classNames: Object.fromEntries(Object.entries(data.CLASSES as Record<string, { name: string }>).map(([k, v]) => [k, v.name])),
};
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out));
console.log(`inn: ${weapons.length} weapons, ${equipment.length} gear, ${potions.length} potions -> ${outPath}`);
