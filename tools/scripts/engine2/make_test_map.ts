import { engineRoot } from './engineRoot';
// A campaign map used by normal play and Modo Teste: when CAMPAIGN=1, the six campaign heroes are added to battle maps
// as entry positions and the normal exporter builds their battle rules. Modo Teste raises its own full party to level 30
// in the C++ session; this map file does not assign special test levels. Free-roam maps remain unchanged.
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/make_test_map.ts -- <map.json> <out campaign map.json>
//
// Not applied (as in a fresh test session): the Party menu's formation (a fresh save has none), the travel clock's
// lighting (test mode never uses it), the leader swap on free-roam maps (the test leader is Kael).

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = engineRoot;
const g = globalThis as Record<string, unknown>;
g.window = globalThis;
g.matchMedia = () => ({ matches: false, addEventListener() {}, removeEventListener() {} });
g.requestAnimationFrame = () => 0;
g.cancelAnimationFrame = () => {};
g.localStorage = { getItem: () => null, setItem() {}, removeItem() {} };
g.Image = class { src = ''; width = 1; height = 1; onload: unknown = null; onerror: unknown = null; };
g.document = { createElement: () => ({ getContext: () => null, style: {} }), documentElement: { classList: { add() {}, remove() {} } } };

const [mapPath, outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!mapPath || !outPath) throw new Error('usage: make_test_map.ts -- <map.json> <out test map.json>');

const store = await import(`${E2}/src/game/mapstore.ts`);
const data = await import(`${E2}/src/game/data.ts`);
const pathfinding = await import(`${E2}/src/game/pathfinding.ts`);

// GameApp.tsx (module-private there, copied as written).
// campaignParty.ts: progression determines the party; map spawns only provide entry positions. Normal play and Modo
// Teste both use this same map. The test session raises its full roster to level 30 after loading the shared battle.
const CAMPAIGN_HERO_CLASS: Record<string, string> = (await import(`${E2}/src/game/campaignParty.ts`)).CAMPAIGN_HERO_CLASSES;

const file = JSON.parse(readFileSync(mapPath, 'utf8'));
const draft = file.draft;
// Older authored maps used the hero's name as a class id (Malrec: "malrec").
// Repair known party entries in the prepared copy, preserving the authored map and valid promotions.
for (const spawn of draft.playerSpawns ?? []) {
  const heroClass = CAMPAIGN_HERO_CLASS[spawn.name];
  if (!data.CLASSES[spawn.classId] && heroClass) spawn.classId = heroClass;
}
const mission = store.draftToMission(draft);

type Spawn = { name: string; classId: string; x: number; y: number; level?: number };
const added: Spawn[] = [];
if (!mission.explore) {
  // GameApp addAdditionalPartyHeroes(mission, TEST_PARTY_CLASS)
  const present = new Set(mission.playerSpawns.map((s: Spawn) => s.name));
  const missing = Object.keys(CAMPAIGN_HERO_CLASS).filter((name) => !present.has(name));
  const terrain = data.parseLayout(mission.layout);
  const occupied = new Set([...mission.playerSpawns, ...mission.enemySpawns, ...(mission.neutralSpawns ?? [])].map((s: Spawn) => pathfinding.key(s.x, s.y)));
  const anchor = mission.playerSpawns[0] ?? { x: 0, y: 0 };
  for (const name of missing) {
    const seen = new Set([pathfinding.key(anchor.x, anchor.y)]);
    const q: { x: number; y: number }[] = [anchor];
    let placed: { x: number; y: number } | null = null;
    while (q.length && !placed) {
      const cur = q.shift()!;
      for (const n of pathfinding.hexNeighbors(cur.x, cur.y)) {
        if (n.x < 0 || n.y < 0 || n.x >= mission.cols || n.y >= mission.rows) continue;
        const k = pathfinding.key(n.x, n.y);
        if (seen.has(k)) continue;
        seen.add(k);
        const terr = data.TERRAIN[terrain[n.y * mission.cols + n.x]];
        if (terr?.passable && !occupied.has(k)) {
          placed = n;
          break;
        }
        q.push(n);
      }
    }
    if (!placed) continue;
    occupied.add(pathfinding.key(placed.x, placed.y));
    added.push({ name, classId: CAMPAIGN_HERO_CLASS[name]!, x: placed.x, y: placed.y });
  }
}

draft.playerSpawns = [...draft.playerSpawns, ...added];
file.draft = draft;
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(file));
console.log(`${draft.id}: campaign map, ${draft.playerSpawns.length} heroes (${added.length ? 'added ' + added.map((s) => `${s.name} at ${s.x},${s.y}`).join(', ') : 'none added'})`);
process.exit(0);
