// Reference runs of the Map Editor's "Gerar terreno" (Engine2 src/editor/ember/MapEditorScreen.tsx: dressMap over
// draftToMission({...draft, autoTactics: true, decorations: []}), then barricadeDecor and the clamped tile variants)
// on the newest saved draft of every map, with Math.random replaced by mulberry32(seed) so EmberEngine's port
// (engine/world/MapDressing.cpp) can draw the same numbers. Engine2 is run read-only.
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_dress_refs.ts -- <out refs.json>
//   Cooker check-dress <out refs.json>

import { readFileSync, readdirSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_dress_refs.ts -- <out refs.json>');

const data = await import(`${E2}/src/ember/data.ts`);
const mapstore = await import(`${E2}/src/campaign/mapstore.ts`);
const tv = await import(`${E2}/src/ember/tileVariants.ts`);

function mulberry32(seed: number): () => number {
  let a = seed | 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// The newest file of every map id, read as the editor opens it (the raw draft).
const dir = `${E2}/maps/ember`;
const newest = new Map<string, { serial: number; draft: any }>();
for (const f of readdirSync(dir)) {
  if (!f.endsWith('.json')) continue;
  const file = JSON.parse(readFileSync(`${dir}/${f}`, 'utf8'));
  const draft = file.draft ?? file;
  if (!draft?.id || !Array.isArray(draft.tiles)) continue;
  const cur = newest.get(draft.id);
  if (!cur || file.serial > cur.serial) newest.set(draft.id, { serial: file.serial ?? 0, draft });
}

const realRandom = Math.random;
const cases: unknown[] = [];
const excludeSome = Object.keys(data.DECORATIONS).filter((_, i) => i % 3 === 0);
let k = 0;
for (const { draft } of [...newest.values()].sort((a, b) => a.draft.id.localeCompare(b.draft.id))) {
  for (const [seed, exclude] of [[1000 + k, [] as string[]], [7 + 31 * k, k % 4 === 0 ? excludeSome : []]] as const) {
    Math.random = mulberry32(seed);
    const filled = data.dressMap(mapstore.draftToMission({ ...draft, autoTactics: true, decorations: [] }), new Set(exclude));
    Math.random = realRandom;
    const tiles = data.parseLayout(filled.layout);
    const scattered = filled.decorations ?? [];
    const decorations = [...scattered, ...data.barricadeDecor(tiles, draft.cols, draft.rows, scattered)];
    const tileVariants = tiles.map((t: string, i: number) => Math.min(draft.tileVariants?.[i] ?? 0, (tv.TILE_VARIANT_COUNT[t] ?? 1) - 1));
    cases.push({ seed: seed >>> 0, exclude, draft, tiles, tileVariants, decorations });
  }
  ++k;
}
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ source: 'export_dress_refs.ts', cases }));
console.log(`dress refs: ${cases.length} cases over ${newest.size} maps -> ${outPath}`);
process.exit(0);
