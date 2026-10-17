// The portraits Engine2 shows beside a conversation's lines (DialogOverlay.tsx dialoguePortraitFor over assets.ts portraitFor),
// for any map's NPC conversations (Spawn.dialog), the Map Editor's own maps included: DialogOverlay's speaker table and
// portraitFor's answer for every class's sprite and every speaker's sprite. A sprite missing here takes portraitFor's own
// fallback (`/game/sprites/<sprite>/1.png`, unframed): MapStage / NpcTalk.h apply the same rule (npcPortrait).
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_portraits.ts -- content/engine2/portraits.json

import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const g = globalThis as Record<string, unknown>;
g.window = globalThis;
g.localStorage = { getItem: () => null, setItem() {}, removeItem() {} };
g.Image = class { src = ''; width = 1; height = 1; onload: unknown = null; onerror: unknown = null; };
g.document = { createElement: () => ({ getContext: () => null, style: {} }), documentElement: { classList: { add() {}, remove() {} } } };

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_portraits.ts -- <out.json>');

const assets = await import(`${E2}/src/game/assets.ts`);
const data = await import(`${E2}/src/ember/data.ts`);

// DialogOverlay.tsx speakerPortraits (a React module; copied as written, the same table export_dialogs.ts uses).
const speakers: Record<string, string> = {
  kael: 'kaelFinal', neera: 'neera', voss: 'voss', salazar: 'salazar', aldric: 'aldric', malrec: 'malrec',
  brue: 'brue', 'o mudo': 'mudinho', 'a hospede': 'crazyLady', 'mercador itinerante': 'travelingMerchant',
};
const sprites = new Set<string>(Object.values(speakers));
for (const c of Object.values(data.CLASSES as Record<string, { sprite?: string }>)) if (c?.sprite) sprites.add(c.sprite);
const out: Record<string, unknown> = {};
for (const s of [...sprites].sort()) out[s] = assets.portraitFor(s);
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ source: 'Engine2 src/game/assets.ts portraitFor + DialogOverlay.tsx speakerPortraits via tools/scripts/engine2/export_portraits.ts', speakers, vargan: { src: '/game/portraits/vargan.png', framed: true }, sprites: out }, null, 1));
console.log(`portraits: ${Object.keys(out).length} sprites -> ${outPath}`);
