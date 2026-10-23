import { engineRoot } from './engineRoot';
// Every class's own movement, from Engine2's class table (src/game/data.ts CLASSES: statsFor's `mov` is the class's
// own, the same at every level), into one file the engine reads for any battle: content/engine2/classes.json.
// A battle file written before `baseMov` existed (an explore mission's units carried the free-roam reach as their
// movement) gets each unit's movement from here, so every map behaves the same whenever its file was made.
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <cache> tools/scripts/engine2/export_classes.ts -- <out.json>
//
// EMBER_GAME_DATA_ROOT=<folder> selects the local game-data snapshot (default content/game).
import { writeFileSync } from 'node:fs';

const E2 = engineRoot;
const out = process.argv[process.argv.length - 1];
const data = await import(`${E2}/src/game/data.ts`);

const classes: Record<string, { mov: number }> = {};
for (const id of Object.keys(data.CLASSES).sort()) classes[id] = { mov: data.statsFor(id, 1).mov };
writeFileSync(out, JSON.stringify({ source: 'Engine2 src/game/data.ts CLASSES (statsFor mov)', classes }, null, 1) + '\n');
console.log(`${Object.keys(classes).length} classes -> ${out}`);
