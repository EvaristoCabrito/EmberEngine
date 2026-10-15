// Every unit (the six heroes, summons, monsters, NPCs) with every pose it has, resolved by Engine2's
// own unit tables (src/units/catalog.ts, visual.ts), read-only: sprite set and clip, Engine2's box for the pose,
// the art's size and crop, and its frame pacing sampled at 240 Hz. EmberEngine's hero viewer reads this.
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_heroes.ts -- <out.heroes.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = 'C:/Engine2';
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_heroes.ts -- <out.heroes.json>');

const catalog = await import(`${E2}/src/units/catalog.ts`);
const visual = await import(`${E2}/src/units/visual.ts`);
const prepared: Record<string, { n: number; natW: number; natH: number; x: number; y: number; w: number; h: number }> =
  JSON.parse(readFileSync(`${E2}/src/units/prepared.json`, 'utf8'));

const heroes: Record<string, unknown>[] = [];
// Every unit Engine2 has: the heroes first, then summons, monsters and NPCs, in the catalog's order.
const order = ['hero', 'summon', 'monster', 'npc'];
const all = [...catalog.UNITS].sort((a: { group: string }, b: { group: string }) => order.indexOf(a.group) - order.indexOf(b.group));
for (const def of all) {
  const name = def.name;
  // UnitActor.drawnReversed() (private there; the same rule export_scene.ts copies)
  const sprite = def.sprite;
  const reversed = !!def.drawnFacingLeft || def.classId === 'familiar' || sprite === 'defaultWarrior' || sprite === 'cobalt-blue-deer' || sprite === 'kaelFinal';
  const poses: Record<string, unknown>[] = [];
  for (const [pose, src] of Object.entries(def.poses) as [string, { dir: string; prefix: string; n: number; still?: boolean }][]) {
    if (!src || src.still) continue;
    const info = prepared[`${src.dir}|${src.prefix}`];
    const natW = info?.natW ?? 0, natH = info?.natH ?? 0;
    const frames = info?.n ?? src.n ?? 1;
    const box = visual.unitBox(def, pose, 0, natW, natH);
    // Frame shown at each 1/240 s over 6 s; one-shot poses (attack, death) hold their last frame when done.
    const pace: number[] = [];
    let doneAt = -1;
    for (let i = 0; i < 1440; i++) {
      const p = visual.framePace(def, pose, frames, i / 240);
      pace.push(p.frame);
      if (p.done && doneAt < 0) doneAt = i;
    }
    poses.push({ pose, label: catalog.POSE_LABEL[pose], set: src.dir, clip: src.prefix.replace(/-$/, '') || 'base', box,
      nat: [natW, natH], crop: info ? [info.x, info.y, info.w, info.h] : null, frames, pace, doneAt });
  }
  if (poses.length) heroes.push({ name, key: def.key, group: def.group, sprite, reversed, poses });
}

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ source: 'Engine2 src/units/catalog.ts via tools/scripts/engine2/export_heroes.ts', heroes }, null, 1));
for (const h of heroes) console.log(`${h.name}: ${(h.poses as { pose: string }[]).map((p) => p.pose).join(', ')}`);
