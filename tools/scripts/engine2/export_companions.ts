// Engine2's companion conversations for EmberEngine (src/game/companionDialogues.ts): every directed pair's
// conversation tree at every tier, as buildCompanionDialogue makes it for a first telling (no continuity prefix, no
// replay), the topics' labels, the tier names, the continuity prefixes, and each line's portrait (as the dialogs export).
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_companions.ts -- <out.companions.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_companions.ts -- <out.companions.json>');

const aff = await import(`${E2}/src/game/affinity.ts`);
const cd = await import(`${E2}/src/game/companionDialogues.ts`);
const assets = await import(`${E2}/src/game/assets.ts`);

const heroes: string[] = [...aff.AFFINITY_HEROES];
const full: Record<string, number> = {};
for (const a of heroes) for (const b of heroes) if (a !== b) full[aff.affinityPair(a, b)] = 100;

const trees: Record<string, unknown> = {};
const portraits: Record<string, unknown> = {};
const topics: Record<string, string[]> = {};
const maxTier: Record<string, number> = {};
for (const leader of heroes)
  for (const companion of heroes) {
    if (leader === companion) continue;
    const list = cd.companionTopics(leader, companion, full, {});
    maxTier[`${leader}:${companion}`] = list.length - 1;
    const labels = list.map((t: { label: string }) => t.label);
    if (!topics[companion] || labels.length > topics[companion].length) topics[companion] = labels;  // Malrec's pairs stop at three; keep the full list
    for (const t of list) {
      const tree = cd.buildCompanionDialogue(leader, companion, t.tier, full, {});
      if (!tree) continue;
      trees[tree.id] = tree;
      for (const line of tree.lines) portraits[`${tree.id}:${line.id}`] = line.portrait ? assets.portraitFor(line.portrait) : null;
    }
  }

// The continuity prefixes: the same scene told after a hard (-3) or a warm (+3) earlier one.
const probe = (previous: number) => {
  const tree = cd.buildCompanionDialogue('Kael', 'Neera', 1, full, { 'companion:Kael:Neera:0': previous });
  const base = trees['companion:Kael:Neera:1'] as { lines: { id: string; text: string }[] };
  const confide = tree.lines.find((l: { id: string }) => l.id === 'confide').text as string;
  const plain = base.lines.find((l) => l.id === 'confide')!.text;
  return confide.slice(0, confide.length - plain.length);
};

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({
  tiers: [...cd.COMPANION_TIERS], tierNames: [...cd.COMPANION_TIER_NAMES], heroes, topics, maxTier,
  continuity: { hard: probe(-3), warm: probe(3) }, trees, portraits,
}));
console.log(`companions: ${Object.keys(trees).length} conversations -> ${outPath}`);
