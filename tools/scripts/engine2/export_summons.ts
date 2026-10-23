import { engineRoot } from './engineRoot';
// The conjurer's summons from Engine2's class table (src/game/data.ts CLASSES familiar..familiar4, zombieDog) and the
// charge tables their summoning reads (familiarSpellCharges, familiarMagicMissileCharges, familiarLifeDrainCharges), into
// content/engine2/summons.json: the C++ spawns a familiar exactly as BattleEngine.castSummonFamiliar does.
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <cache> tools/scripts/engine2/export_summons.ts -- <out.json>
//
// EMBER_GAME_DATA_ROOT=<folder> selects the local game-data snapshot (default content/game).
import { writeFileSync } from 'node:fs';

const E2 = engineRoot;
const out = process.argv[process.argv.length - 1];
const data = await import(`${E2}/src/game/data.ts`);

const classes: Record<string, unknown> = {};
for (const id of ['familiar', 'familiar2', 'familiar3', 'familiar4', 'zombieDog']) {
  const c = data.CLASSES[id];
  // engine.ts initiativeBonus: 11 - init for everything but the assassin, rogue and archers
  classes[id] = { id: c.id, name: c.name, role: c.role, sprite: c.sprite, mov: c.mov, minRange: c.minRange, maxRange: c.maxRange, size: c.size,
    footprintOffsets: c.footprintOffsets ?? null, footprintW: c.footprintW ?? null, footprintH: c.footprintH ?? null, resistances: c.resistances ?? {},
    initiativeBonus: 11 - (c.init ?? 10) };
}
const levels = Array.from({ length: 30 }, (_, i) => i + 1);
const tables = {
  familiarSpellCharges: levels.map((l) => data.familiarSpellCharges(l)),
  familiarMagicMissileCharges: levels.map((l) => data.familiarMagicMissileCharges(l)),
  familiarLifeDrainCharges: levels.map((l) => data.familiarLifeDrainCharges(l)),
};
writeFileSync(out, JSON.stringify({ source: 'Engine2 src/game/data.ts (summon classes, familiar charge tables)', classes, tables, zombieDogVenomCharges: data.SUMMON_ZOMBIE_DOG.causticVenomCharges }, null, 1) + '\n');
console.log(`${Object.keys(classes).length} summon classes -> ${out}`);
