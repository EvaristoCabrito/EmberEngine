// Battle rules for one Ember map, from Engine2's own BattleEngine run headless (read-only): every cell's terrain as the
// movement rules see it (the decorations' blocking folded in: hexprops.hexDef), every unit the battle starts with
// (stats, size, body shape) and, as the reference EmberEngine's port is checked against (Cooker check-battle), each
// unit's movement reach from Engine2's computeReachable (cost and parent of every reachable hex).
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_battle.ts -- <map.json> <out.battle.json>
//
// ENGINE2=<folder> points at another Engine2 checkout (default C:/Engine2).

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';

// The engine module touches a few browser globals while it loads and builds a battle; it never draws here.
const g = globalThis as Record<string, unknown>;
g.window = globalThis;
g.matchMedia = () => ({ matches: false, addEventListener() {}, removeEventListener() {} });
g.requestAnimationFrame = () => 0;
g.cancelAnimationFrame = () => {};
g.localStorage = { getItem: () => null, setItem() {}, removeItem() {} };
g.Image = class { src = ''; width = 1; height = 1; onload: unknown = null; onerror: unknown = null; };
g.document = { createElement: () => ({ getContext: () => null, style: {} }), documentElement: { classList: { add() {}, remove() {} } } };

const [mapPath, outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!mapPath || !outPath) throw new Error('usage: export_battle.ts -- <map.json> <out.battle.json>');

const store = await import(`${E2}/src/game/mapstore.ts`);
const { BattleEngine } = await import(`${E2}/src/game/engine.ts`);
const hexprops = await import(`${E2}/src/game/hexprops.ts`);
const pathfinding = await import(`${E2}/src/game/pathfinding.ts`);

const draft = JSON.parse(readFileSync(mapPath, 'utf8')).draft;
const mission = store.draftToMission(draft);
// No art is drawn: every art table reads as empty.
const art = new Proxy({}, { get: () => new Proxy({}, { get: () => [] }) });
// The editor's preview battle (MapPreviewCanvas): fog off, a fresh party.
const engine = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels: {} }, 1);
const e = engine as unknown as Record<string, any>;

const cells: Record<string, unknown>[] = [];
for (let y = 0; y < e.rows; y++)
  for (let x = 0; x < e.cols; x++) {
    const def = hexprops.hexDef(e.tiles, e.cols, x, y, e.decorOverlay);
    const bits = e.decorOverlay[y * e.cols + x] ?? 0;
    cells.push({ terrain: e.tiles[y * e.cols + x], passable: !!def.passable, moveCost: def.moveCost, def: def.def ?? 0, atk: def.atk ?? 0,
      blocked: (bits & hexprops.HEX_BLOCKED) !== 0 });
  }

const units: Record<string, unknown>[] = [];
for (const u of e.units) {
  // The reach the battle shows for this unit (BattleEngine.effectiveUnitForReach + computeReachable).
  const reach = pathfinding.computeReachable(e.effectiveUnitForReach(u), e.tiles, e.cols, e.rows, e.units, true, e.decorOverlay);
  const cellsOut: number[][] = [];
  for (const c of reach.values()) {
    const parent = c.parent ? c.parent.split(',').map(Number) : [-1, -1];
    cellsOut.push([c.x, c.y, c.cost, parent[0], parent[1]]);
  }
  units.push({ id: u.id, side: u.side, name: u.name, classId: u.classId, sprite: u.sprite, x: u.x, y: u.y, alive: !!u.alive,
    mov: e.effectiveUnitForReach(u).mov, hp: u.hp, maxHp: u.maxHp, atk: u.atk, def: u.def, mag: u.mag, dex: u.dex,
    minRange: u.minRange ?? 1, maxRange: u.maxRange ?? 1, size: u.size ?? 1,
    footprintOffsets: u.footprintOffsets ?? null, footprintW: u.footprintW ?? null, footprintH: u.footprintH ?? null, reach: cellsOut });
}

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ source: 'Engine2 src/game/engine.ts BattleEngine via tools/scripts/engine2/export_battle.ts',
  map: draft.id, cols: e.cols, rows: e.rows, cells, units }));
console.log(`${draft.id}: ${e.cols}x${e.rows}, ${units.length} units (${units.map((u) => `${u.name} mov ${u.mov}`).join(', ')})`);
process.exit(0);  // the engine leaves timers running
