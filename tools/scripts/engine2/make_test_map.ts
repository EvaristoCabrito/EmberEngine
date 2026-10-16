// A campaign map as Engine2's Modo teste plays it (GameApp startBattle with testMode, read-only): the heroes a mission's
// own spawns leave out are added (addAdditionalPartyHeroes with TEST_PARTY_CLASS: each missing hero of the six on the
// nearest free passable hex, breadth-first from the first party spawn, never on a wall, water or another unit), and
// every party spawn is at DEFAULT_TEST_LEVEL (15). Free-roam maps (explore) are left as they are, as Engine2 does.
// The result is a map file like Engine2's own, for export_scene.ts / measure_art.py and export_battle.ts (TEST_LEVEL=15).
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/make_test_map.ts -- <map.json> <out test map.json>
//
// Not applied (as in a fresh test session): the Party menu's formation (a fresh save has none), the travel clock's
// lighting (test mode never uses it), the leader swap on free-roam maps (the test leader is Kael).

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
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
const DEFAULT_TEST_LEVEL = 15;
const TEST_PARTY_CLASS: Record<string, string> = { Kael: 'kaelFinal', Neera: 'neera', Voss: 'voss', Salazar: 'salazar', Aldric: 'aldric', Malrec: 'conjurer' };

const file = JSON.parse(readFileSync(mapPath, 'utf8'));
const draft = file.draft;
const mission = store.draftToMission(draft);

type Spawn = { name: string; classId: string; x: number; y: number; level?: number };
const added: Spawn[] = [];
if (!mission.explore) {
  // GameApp addAdditionalPartyHeroes(mission, TEST_PARTY_CLASS)
  const present = new Set(mission.playerSpawns.map((s: Spawn) => s.name));
  const missing = Object.keys(TEST_PARTY_CLASS).filter((name) => !present.has(name));
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
    added.push({ name, classId: TEST_PARTY_CLASS[name]!, x: placed.x, y: placed.y });
  }
}

draft.playerSpawns = [...draft.playerSpawns, ...added].map((s: Spawn) => ({ ...s, level: DEFAULT_TEST_LEVEL }));
file.draft = draft;
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(file));
console.log(`${draft.id}: test map, ${draft.playerSpawns.length} heroes (${added.length ? 'added ' + added.map((s) => `${s.name} at ${s.x},${s.y}`).join(', ') : 'none added'})`);
process.exit(0);
