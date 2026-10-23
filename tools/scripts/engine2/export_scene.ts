import { engineRoot } from './engineRoot';
// Map importer: one Ember map's scene as Engine2 places it (decorations, 3D models, architecture, lights,
// units), resolved by Engine2's own rules and written as EmberEngine data. Engine2's game design is the
// reference (ARCHITECTURE §8): its tables and functions are run here, read-only; only the two small
// module-private rules decor.ts keeps to itself (decorSize, the unit mirroring rule) are copied verbatim.
// Image-based measurements (the art's opaque base, the painted flame) are made by EmberEngine from the art.
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_scene.ts -- <map.json> <out.scene.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = engineRoot;
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [mapPath, outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!mapPath || !outPath) throw new Error('usage: export_scene.ts -- <map.json> <out.scene.json>');

const boardMod = await import(`${E2}/src/map/board.ts`);
const mesh = await import(`${E2}/src/render/terrainMesh.ts`);
const relief = await import(`${E2}/src/render/terrainRelief.ts`);
const data = await import(`${E2}/src/ember/data.ts`);
const { fitHouseToHumans } = await import(`${E2}/src/world/houseScale.ts`);
const { MODELS } = await import(`${E2}/src/world/models.ts`);
const lighting = await import(`${E2}/src/ember/lighting.ts`);
const catalog = await import(`${E2}/src/units/catalog.ts`);
const visual = await import(`${E2}/src/units/visual.ts`);
const backgrounds = await import(`${E2}/src/editor/mapBackgrounds.ts`);
const decorFiles: string[] = JSON.parse(readFileSync(`${E2}/src/world/decor-files.json`, 'utf8'));
const prepared: Record<string, { n: number; natW: number; natH: number; x: number; y: number; w: number; h: number }> =
  JSON.parse(readFileSync(`${E2}/src/units/prepared.json`, 'utf8'));

type Def = Record<string, any>;
const DECORATIONS: Record<string, Def> = data.DECORATIONS;
const SQRT3 = Math.sqrt(3);
const ARCHITECTURE = new Set(['wall', 'doorway', 'door', 'secretDoor']);
const STAND_ON_DECOR = new Set(['fallen-log', 'wilds-fallen-log', 'forest-photographic-011-log', 'forest-mossy-trunk', 'wilds-snowy-log',
  'wilds-mossy-log', 'wilds-mossy-charred-log', 'wilds-hollow-log', 'wilds-winter-mossy-log']);
const FILES = new Set(decorFiles);
const fileOf = (url: string) => url.split('?')[0].split('/').pop()!;
const hasArt = (file: string) => FILES.has(fileOf(data.decorationImage(file)));

// --- copied verbatim from Engine2 src/world/decor.ts (module-private there) ---
function decorSize(id: string, def: Def, tile = 1): { w: number; h: number } {
  if (id === 'stone-stairs-up-001' || id === 'stone-stairs-down-001') return { w: tile * SQRT3, h: tile * 3.5 };
  if (def.propModel && !def.propModel.startsWith('tavern-')) {
    const pm = def.propModel === 'grey-outcrop' ? 'rocky-outcrop' : def.propModel;
    if (DECORATIONS[pm]) return decorSize(pm, DECORATIONS[pm]!, tile);
  }
  if (def.treeModel) return { w: tile * 3.2, h: tile * 4.8 };
  let minDx = 0, maxDx = 0, minDy = 0, maxDy = 0;
  for (const { dx, dy } of def.footprint) { minDx = Math.min(minDx, dx); maxDx = Math.max(maxDx, dx); minDy = Math.min(minDy, dy); maxDy = Math.max(maxDy, dy); }
  const one = def.footprint.length === 1, item = data.CHEST_DECOR_IDS.has(id), tree = id === 'dead-tree', log = id === 'fallen-log', wall = id === 'barricade';
  const waypoint = !!def.exitKind, anyHouse = data.HOUSE_DECOR_IDS.has(id) || data.BIG_HOUSE_DECOR_IDS.has(id);
  const w = tree ? tile * 1.28 : log ? tile * SQRT3 * 2.05 : wall ? tile * 1.42 : anyHouse ? tile * 1.45 * 3 : waypoint ? tile * SQRT3 * (one ? 1 : 2)
    : item ? tile * 0.92 : one ? tile * 1.55 : tile * SQRT3 * (maxDx - minDx + 1.7);
  const baseH = tree ? tile * 2.55 : log ? tile * 0.82 : wall ? tile * 1.18 : anyHouse ? tile * 1.58 * 3 : waypoint ? tile * 2
    : item ? tile * 0.72 : one ? tile * 1.65 : tile * (1.5 * (maxDy - minDy) + 2.3);
  const h = def.artAspect ? w / def.artAspect : baseH * (def.heightScale ?? 1);
  const s = waypoint ? 1 : (anyHouse ? data.HOUSE_ART_SCALE : data.DECOR_ART_SCALE) * (def.artScale ?? 1);
  return fitHouseToHumans(id, { w: w * s, h: h * s }, tile);
}
// --- end copy ---

const file = JSON.parse(readFileSync(mapPath, 'utf8'));
const draft = file.draft ?? file;
const b = new boardMod.Board(draft);
await relief.prepareTerrainRelief(b);
const L = b.layout;
const inBoard = (c: number, r: number) => c >= 0 && r >= 0 && c < L.cols && r < L.rows;
const heights = (x: number, z: number) => ({ smooth: mesh.groundHeightAt(b, 'smooth', x, z), hard: mesh.groundHeightAt(b, 'hard', x, z) });
const centroid = (p: { id: string; x: number; y: number; rot?: number }) => {
  let sx = 0, sz = 0, n = 0;
  for (const { dx, dy } of data.placedFootprint(p)) { if (!inBoard(p.x + dx, p.y + dy)) continue; const c = b.cell(p.x + dx, p.y + dy); sx += c.x; sz += c.z; n++; }
  return n ? { x: sx / n, z: sz / n } : null;
};

const placements = [...(draft.decorations ?? []), ...data.barricadeDecor(draft.tiles, draft.cols, draft.rows, draft.decorations ?? [])];
const decor: Record<string, unknown>[] = [];
const skipped: Record<string, number> = {};
const skip = (why: string) => { skipped[why] = (skipped[why] ?? 0) + 1; };
for (const p of placements) {
  const def = DECORATIONS[p.id];
  if (!def) { skip(`no definition: ${p.id}`); continue; }
  const hexes = data.placedFootprint(p).filter((f: { dx: number; dy: number }) => inBoard(p.x + f.dx, p.y + f.dy)).map((f: { dx: number; dy: number }) => [p.x + f.dx, p.y + f.dy]);
  if (def.model3d && ARCHITECTURE.has(def.model3d) && !def.propModel) {
    decor.push({ kind: 'architecture', id: p.id, at: [p.x, p.y], rot: p.rot ?? 0, model3d: def.model3d, hexes });
    continue;
  }
  const model = def.treeModel ?? def.propModel;
  if (model && MODELS[model]) {
    const c = centroid(p);
    if (!c) { skip('off the board'); continue; }
    decor.push({ kind: 'model', id: p.id, model, file: `/game/models/${MODELS[model].file}`, height: MODELS[model].height, x: c.x, z: c.z,
      ground: heights(c.x, c.z), rotY: -(p.rot ?? 0) * Math.PI / 2, hexes });
    continue;
  }
  const rot = ((p.rot ?? 0) + (def.mirrorAlternate && p.x >= draft.cols / 2 ? 3 : 0)) % 6;
  const artId = data.decorationPlacementArt(p);
  const facing = data.decorationFacing(artId, rot, hasArt);
  const artFile = facing.own ? facing.file : artId;
  if (!hasArt(artFile)) { skip(`no art: ${p.id}`); continue; }
  const c = centroid(p);
  if (!c) { skip('off the board'); continue; }
  const { w, h } = decorSize(p.id, def);
  const lightDef = lighting.LIGHT_DEFS[p.id];
  const onFloor = !!def.exitKind;
  decor.push({
    kind: 'card', id: p.id, image: data.decorationImage(artFile).split('?')[0], x: c.x, z: c.z, ground: heights(c.x, c.z), w, h,
    mirror: facing.mirror !== !!p.mirrorX, onFloor, castShadow: !lightDef && !onFloor,
    lockedYaw: data.BARRICADE_LIKE_DECOR.has(p.id) ? -((p.rot ?? 0) * Math.PI) / 3 : null,
    nudge: (def.unitLayer === 'behind' || STAND_ON_DECOR.has(p.id) ? -0.35 : lightDef ? 0.4 : def.foreground || def.unitLayer === 'front' ? 0.2 : 0) + (p.x % 20) * 0.002,
    standOn: STAND_ON_DECOR.has(p.id),
    light: lightDef ? { color: lightDef.color, intensity: lightDef.intensity, radius: lightDef.radius * lighting.LIGHT_RADIUS_MUL,
      flicker: lightDef.flicker, minHeight: lighting.MAP_LIGHT_MIN_HEIGHT, seed: (p.x * 7.31 + p.y * 3.17) % 6.28 } : null,
    hexes,
  });
}

// Units: Engine2's dioramaView.placeUnits + UnitActor (idle pose).
const units: Record<string, unknown>[] = [];
const sides: [string, 'player' | 'enemy' | 'neutral'][] = [['playerSpawns', 'player'], ['enemySpawns', 'enemy'], ['neutralSpawns', 'neutral']];
for (const [side, kind] of sides) {
  for (const s of draft[side] ?? []) {
    if (s.x < 0 || s.y < 0 || s.x >= draft.cols || s.y >= draft.rows) continue;
    const def = catalog.unitForSpawn(s, kind);
    if (!def) { skip(`no unit: ${s.classId}`); continue; }
    // Ember's footprintCentroid: the front row of the footprint
    const front: [number, number][] = def.footprint
      ? def.footprint.filter((o: { dy: number }) => o.dy === 0).map((o: { dx: number; dy: number }) => [s.x + o.dx + ((o.dy & 1) && (s.y & 1) ? 1 : 0), s.y + o.dy])
      : [[s.x, s.y]];
    let x = 0, z = 0, n = 0;
    for (const [fc, fr] of front) { if (!inBoard(fc, fr)) continue; const cell = b.cell(fc, fr); x += cell.x; z += cell.z; n++; }
    if (!n) { const cell = b.cell(s.x, s.y); x = cell.x; z = cell.z; n = 1; }
    x /= n; z /= n;
    const pose = 'idle';
    const src = def.poses[pose];
    if (!src) { skip(`no idle: ${def.key}`); continue; }
    const info = prepared[`${src.dir}|${src.prefix}`];
    const natW = info?.natW ?? 0, natH = info?.natH ?? 0;
    const box = visual.unitBox(def, pose, 0, natW, natH);
    const facing = kind === 'enemy' ? -1 : 1;
    // UnitActor.drawnReversed() for the idle pose (copied verbatim, private there)
    const sprite = def.sprite;
    const reversed = !!def.drawnFacingLeft || def.classId === 'familiar' || sprite === 'defaultWarrior' || sprite === 'cobalt-blue-deer' || sprite === 'kaelFinal';
    const frames = info?.n ?? src.n ?? 1;
    // Engine2's frame pacing for the idle, sampled: frame shown at each 1/240 s over 6 s.
    const pace: number[] = [];
    for (let i = 0; i < 1440; i++) pace.push(visual.framePace(def, pose, frames, i / 240).frame);
    units.push({ side: kind, name: s.name, classId: s.classId, key: def.key, sprite, set: src.dir, clip: src.prefix.replace(/-$/, '') || 'base',
      x, z, ground: heights(x, z), facing, mirror: (facing === -1) !== reversed, box, nat: [natW, natH], crop: info ? [info.x, info.y, info.w, info.h] : null,
      frames, pace, light: lighting.UNIT_LIGHT_DEFS[def.classId] ?? null });
  }
}

const out = {
  source: `Engine2 map ${mapPath}, resolved by tools/scripts/engine2/export_scene.ts`,
  map: draft.id, title: draft.title, cols: draft.cols, rows: draft.rows,
  lighting: { timeOfDay: draft.timeOfDay ?? 'day', sunIntensity: draft.sunIntensity ?? null, ambientIntensity: draft.ambientIntensity ?? null,
    environment: draft.environment ?? 'outdoor', bloomIntensity: draft.bloomIntensity ?? null, mistType: draft.mistType ?? null },
  // The painted backdrop behind the board (Engine2 editor/mapBackgrounds.ts resolvedMapBackground: the map's own
  // pick, else Ember's routing by map id), a file in public/game/map-backgrounds; null = none.
  backdrop: backgrounds.resolvedMapBackground(draft)?.file ?? null,
  decor, units, skipped,
};
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out, null, 1));
const count = (k: string) => decor.filter((d) => d.kind === k).length;
console.log(`${draft.id}: ${count('card')} cards, ${count('model')} models, ${count('architecture')} architecture, ${units.length} units; skipped ${JSON.stringify(skipped)}`);
