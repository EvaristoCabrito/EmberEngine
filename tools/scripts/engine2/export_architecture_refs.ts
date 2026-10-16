// Reference solids for Ember's 3D architecture: Engine2's own createWallGeometry (src/game/gfx/three/ThreeWalls.ts and
// the castle / temple / rock / reference-door builders) called as its 3D map calls it (src/world/decor.ts
// addArchitecture: tile 1, height x 3.5 x WALL_HUMAN_SCALE 1.6, physical form, texture height 3.5, turned upright),
// for every architecture piece, every quarter turn and the joins a wall can have. EmberEngine's port
// (engine/world/Architecture.cpp) is compared with it by Cooker check-architecture. Engine2 is run read-only.
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_architecture_refs.ts -- <out refs.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${E2}/public${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map() };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_architecture_refs.ts -- <out refs.json>');

const data = await import(`${E2}/src/ember/data.ts`);
const walls = await import(`${E2}/src/game/gfx/three/ThreeWalls.ts`);
const ARCHITECTURE = new Set(['wall', 'doorway', 'door', 'secretDoor']);
const SQ3 = Math.sqrt(3);
// decor.ts archAt deltas to a neighbour, as {x, y: -dz}: east, west, the row above, the row below
const E = { x: SQ3, y: 0 }, W = { x: -SQ3, y: 0 }, N = { x: 0, y: 1.5 }, S = { x: 0, y: -1.5 };
const joins: { x: number; y: number }[][] = [[], [E], [W], [N], [S], [E, W], [N, S], [E, N], [W, S], [E, N, S], [E, W, N, S]];
const origin = { x: 3.7, y: -2.1 };

const cases: unknown[] = [];
for (const def of Object.values(data.DECORATIONS) as any[]) {
  if (!def.model3d || !ARCHITECTURE.has(def.model3d) || def.propModel) continue;
  const spatial = { ...def, heightScale: (def.heightScale ?? 1) * 3.5 * 1.6 };
  for (let rot = 0; rot < 4; rot++) {
    for (const connections of joins) {
      const g = walls.createWallGeometry(spatial, 1, rot, connections, origin, false, 3.5).rotateX(-Math.PI / 2);
      const flat = g.index ? g.toNonIndexed() : g;
      const pick = (name: string) => Array.from(flat.getAttribute(name).array as Float32Array);
      cases.push({ id: def.id, rot, connections, origin, position: pick('position'), normal: pick('normal'), uv: pick('uv') });
    }
  }
}
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ source: 'export_architecture_refs.ts', cases }));
console.log(`architecture refs: ${cases.length} cases -> ${outPath}`);
process.exit(0);
