// Engine2's campaign data for EmberEngine's save files and campaign screens, read-only from its own modules
// (src/campaign/save.ts, mapstore.ts, overworld.ts, src/ember/data.ts): the empty save (SaveData v19 with the starting
// bags and gear), the missions in campaign order, the world-map locations with their hexes, and slotProgress()
// answers for reference saves (EmberEngine's port is checked against them).
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_campaign.ts -- <out.campaign.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_campaign.ts -- <out.campaign.json>');

const save = await import(`${E2}/src/campaign/save.ts`);
const mapstore = await import(`${E2}/src/campaign/mapstore.ts`);
const overworld = await import(`${E2}/src/campaign/overworld.ts`);
const data = await import(`${E2}/src/ember/data.ts`);

const empty = save.emptySave(false);
empty.updatedAt = 0;

// Each mission's map file: the newest serial saved for its id (mapstore latestPerScenario); shipped missions with no
// saved map have none (EmberEngine plays maps from their files).
const newest = new Map<string, { file: string; serial: number }>();
for (const f of mapstore.savedMapFiles() as { file: string; serial: number; draft: { id: string } }[]) {
  const cur = newest.get(f.draft.id);
  if (!cur || f.serial > cur.serial) newest.set(f.draft.id, { file: f.file, serial: f.serial });
}
const keep = ['id', 'index', 'title', 'place', 'briefing', 'objective', 'win'];
const missions = (mapstore.ALL_MISSIONS as Record<string, unknown>[]).map((m) => {
  const o: Record<string, unknown> = {};
  for (const k of keep) if (m[k] !== undefined) o[k] = m[k];
  const f = newest.get(m.id as string);
  if (f) o.mapFile = f.file;
  return o as { id: string } & Record<string, unknown>;
});
const locations = (data.WORLD_LOCATIONS as { x: number; y: number }[]).map((l) => {
  const h = overworld.worldToHex(l.x, l.y);
  return { ...JSON.parse(JSON.stringify(l)), col: h.x, row: h.y };
});

// slotProgress for reference saves: empty, a pending mission, a battle in progress, standing on every location, out on
// the road, and after each completed mission (classic map).
const cases: { save: unknown; title: string; detail: string }[] = [];
const add = (s: Record<string, unknown>) => {
  const p = save.slotProgress(s);
  cases.push({ save: s, title: p.title, detail: p.detail });
};
add({ ...empty });
for (const m of missions.slice(0, 6)) add({ ...empty, pendingMission: m.id });
add({ ...empty, battle: { missionId: missions[0]?.id, turn: 3 } });
for (const l of locations) add({ ...empty, mapMode: 'rpg', overworldPos: { col: l.col, row: l.row }, gameClock: 4 });
for (const [col, row] of [[0, 0], [5, 9], [20, 14], [33, 2]]) add({ ...empty, mapMode: 'rpg', overworldPos: { col, row }, gameClock: 2 });
for (let i = 1; i <= Math.min(missions.length, 12); ++i) add({ ...empty, completed: missions.slice(0, i).map((m) => m.id) });

const out = {
  saveVersion: save.SAVE_VERSION,
  slotCount: save.SLOT_COUNT,
  startHex: { col: overworld.OVERWORLD_START_HEX.x, row: overworld.OVERWORLD_START_HEX.y },
  emptySave: empty,
  missions,
  locations,
  slotProgress: cases,
};
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out, null, 1));
console.log(`campaign: ${missions.length} missions, ${locations.length} locations, ${cases.length} slotProgress cases -> ${outPath}`);
