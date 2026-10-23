import { engineRoot } from './engineRoot';
// Engine2's mission dialogs for EmberEngine: each mission's intro and outro conversation (mapstore ALL_MISSIONS
// introDialog / outroDialog, with their enabled switches) and, for every line, the portrait Engine2 shows beside it
// (DialogOverlay.tsx dialoguePortraitFor, with the mission's spawns standing in for the battle's units).
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_dialogs.ts -- <out.dialogs.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = engineRoot;
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_dialogs.ts -- <out.dialogs.json>');

const mapstore = await import(`${E2}/src/campaign/mapstore.ts`);
const assets = await import(`${E2}/src/game/assets.ts`);
const data = await import(`${E2}/src/ember/data.ts`);

// DialogOverlay.tsx (a React module; its portrait rule is copied here as written).
const speakerKey = (name: string) => name.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().trim();
const speakerPortraits: Record<string, string> = {
  kael: 'kaelFinal', neera: 'neera', voss: 'voss', salazar: 'salazar', aldric: 'aldric', malrec: 'malrec',
  brue: 'brue', 'o mudo': 'mudinho', 'a hospede': 'crazyLady', 'mercador itinerante': 'travelingMerchant',
};
type Line = { id: string; speaker: string; portrait?: string };
type Tree = { id: string; startId: string; lines: Line[] };
function portraitOf(line: Line, tree: Tree, characters: { name: string; sprite: string }[]) {
  const speaker = speakerKey(line.speaker);
  const character = characters.find((c) => speakerKey(c.name) === speaker) ?? characters.find((c) => speakerKey(c.name).split(',')[0] === speaker.split(',')[0]);
  const same = tree.lines.find((l) => l.portrait && speakerKey(l.speaker) === speaker);
  const sprite = speakerPortraits[speaker] ?? line.portrait ?? character?.sprite ?? same?.portrait;
  return speaker === 'vargan' ? { src: '/game/portraits/vargan.png', framed: true } : sprite ? assets.portraitFor(sprite) : null;
}

const out: Record<string, unknown> = {};
for (const m of mapstore.ALL_MISSIONS as Record<string, any>[]) {
  if (!m.introDialog && !m.outroDialog) continue;
  const spawns = [...(m.playerSpawns ?? []), ...(m.enemySpawns ?? []), ...(m.neutralSpawns ?? [])] as { name: string; classId: string; sprite?: string }[];
  const characters = spawns.map((s) => ({ name: s.name, sprite: s.sprite ?? (data.CLASSES as Record<string, { sprite?: string }>)[s.classId]?.sprite ?? '' }));
  const portraits: Record<string, unknown> = {};
  for (const tree of [m.introDialog, m.outroDialog] as (Tree | undefined)[])
    if (tree) for (const line of tree.lines) portraits[`${tree.id}:${line.id}`] = portraitOf(line, tree, characters);
  out[m.id] = {
    intro: m.introDialog ?? null, introEnabled: m.introDialogEnabled !== false,
    outro: m.outroDialog ?? null, outroEnabled: m.outroDialogEnabled !== false,
    portraits,
  };
}
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out));
console.log(`dialogs: ${Object.keys(out).length} missions with an intro or outro -> ${outPath}`);
