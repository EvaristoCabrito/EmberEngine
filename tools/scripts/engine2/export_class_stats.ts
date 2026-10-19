// Every class's stats at every level, from Engine2's class table (src/game/data.ts statsFor), into
// content/engine2/class-stats.json: { classes: { <id>: [[hp, atk, mag, def, dex] per level 1..MAX_LEVEL] } }.
// EmberEngine's own progression (content/progression.json) is measured against it: a hero's new gains replace its
// class's; an enemy keeps its class's own stats (engine/world/Progression).
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <cache> tools/scripts/engine2/export_class_stats.ts -- <out.json>
//
// ENGINE2=<folder> points at another Engine2 checkout (default C:/Engine2). Engine2 is only read.
import { writeFileSync } from 'node:fs';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const out = process.argv[process.argv.length - 1];
const data = await import(`${E2}/src/game/data.ts`);

const classes: Record<string, number[][]> = {};
for (const id of Object.keys(data.CLASSES).sort()) {
  const rows: number[][] = [];
  for (let level = 1; level <= data.MAX_LEVEL; ++level) {
    const s = data.statsFor(id, level);
    rows.push([s.hp, s.atk, s.mag, s.def, s.dex]);
  }
  classes[id] = rows;
}
const lines = Object.entries(classes).map(([id, rows]) => `  ${JSON.stringify(id)}: ${JSON.stringify(rows)}`);
writeFileSync(out, `{\n "source": "Engine2 src/game/data.ts statsFor (hp atk mag def dex, levels 1..${data.MAX_LEVEL})",\n "classes": {\n${lines.join(',\n')}\n }\n}\n`);
console.log(`${Object.keys(classes).length} classes -> ${out}`);
