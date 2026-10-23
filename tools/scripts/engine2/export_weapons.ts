import { engineRoot } from './engineRoot';
// Engine2's weapons as a fight reads them (GameApp startBattle -> BattleEngine spawnUnit, read-only): every main-hand weapon
// (src/game/data.ts WEAPONS: dice, reach, the hands it takes, the class it is tuned for, its staff magic), every off-hand item
// (EQUIPMENT of kind weapon / shield), and per hero class its starter weapon (starterWeaponFor), the weapon types it trains
// (weaponSkills.ts weaponTypesForClass) and its own reach. EmberEngine equips the save's weapons with it at the start of a
// campaign fight (engine/world/Loadout).
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_weapons.ts -- content/engine2/weapons.json
//
// EMBER_GAME_DATA_ROOT=<folder> selects the local game-data snapshot (default content/game).
import { readFileSync, writeFileSync } from 'node:fs';

const E2 = engineRoot;
const g = globalThis as Record<string, unknown>;
g.window = globalThis;
g.matchMedia = () => ({ matches: false, addEventListener() {}, removeEventListener() {} });
g.localStorage = { getItem: () => null, setItem() {}, removeItem() {} };
g.fetch = (async (url: string) => {
  const text = readFileSync(`${E2}/public${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map() };
}) as unknown as typeof fetch;

const out = process.argv[process.argv.length - 1];
const data = await import(`${E2}/src/game/data.ts`);
const skills = await import(`${E2}/src/game/weaponSkills.ts`);

const weapons: Record<string, unknown> = {};
for (const [id, w] of Object.entries(data.WEAPONS as Record<string, any>)) {
  const m = w.magic;
  weapons[id] = {
    weaponType: w.weaponType, usableBy: w.usableBy, dice: w.dice, faces: w.faces, bonus: w.bonus ?? 0, minRange: w.minRange, maxRange: w.maxRange,
    ranged: !!w.ranged, twoHanded: !!w.twoHanded, bonusClass: w.bonusClass ?? null,
    magic: m ? { attackElement: m.attackElement ?? null, elementalBonusPct: m.elementalBonusPct ?? {}, lifeStealPct: m.lifeStealPct ?? 0, regeneration: m.regeneration ?? 0,
                 magBonus: m.magBonus ?? 0, dexBonus: m.dexBonus ?? 0, resistances: m.resistances ?? {} } : null,
  };
}
const offHand: Record<string, unknown> = {};
for (const [id, e] of Object.entries(data.EQUIPMENT as Record<string, any>))
  if (e.kind === 'weapon' || e.kind === 'shield')
    offHand[id] = { kind: e.kind, weaponType: e.weaponType ?? null, usableBy: e.usableBy ?? [], minRange: e.minRange ?? 1, maxRange: e.maxRange ?? 1,
                    dice: e.dice ?? 1, faces: e.faces ?? 4, bonus: e.bonus ?? 0, dmgMul: e.dmgMul ?? 0.75 };
const classes: Record<string, unknown> = {};
for (const id of Object.keys(data.CLASSES)) {
  const st = data.statsFor(id, 1);
  classes[id] = { starter: data.starterWeaponFor(id), weaponTypes: skills.weaponTypesForClass(id), minRange: st.minRange, maxRange: st.maxRange };
}
// engine.ts MAGE_RANGE_BONUS_CLASSES (module-private there, copied as written): +1 reach on any weapon.
const mageRangeBonus = ['mage', 'elementalist', 'warlock'];
writeFileSync(out, JSON.stringify({ source: 'Engine2 src/game/data.ts WEAPONS / EQUIPMENT, starterWeaponFor, weaponTypesForClass', mageRangeBonus, weapons, offHand, classes }) + '\n');
console.log(`${Object.keys(weapons).length} weapons, ${Object.keys(offHand).length} off-hand items, ${Object.keys(classes).length} classes -> ${out}`);
