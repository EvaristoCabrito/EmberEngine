// The map editor's catalog: everything export_scene.ts reads from Engine2 to place a map's scene, resolved per
// decoration and per unit instead of per map, so EmberEngine can build a scene from any draft on the fly
// (engine/world/SceneBuilder: the editor's live preview). Engine2 is run read-only; decorSize is copied from
// export_scene.ts (itself a verbatim copy of Engine2's module-private src/world/decor.ts rule).
// Image measurements (the art's base and flame) are added by measure_art.py --catalog.
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_editor_catalog.ts -- <out catalog.json>
//
// Cooker check-scene-builder compares the C++ builder's scenes with export_scene.ts's on the campaign maps.

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_editor_catalog.ts -- <out catalog.json>');

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

// --- copied verbatim from export_scene.ts (Engine2 src/world/decor.ts, module-private there) ---
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

// Every art id a placement can show: each decoration's own, and the connector / stairs variants
// (decorationPlacementArt).
const artIds = new Set<string>(Object.keys(DECORATIONS));
for (const a of ['floor-connector-up-gold-v1', 'floor-connector-down-red-v1', 'stone-stairs-up-001', 'stone-stairs-down-001']) artIds.add(a);
// Each art id's six facings (decorationFacing): the file shown, mirrored or not, whether it has art, its image URL.
const facings: Record<string, unknown[]> = {};
for (const art of artIds) {
  facings[art] = [0, 1, 2, 3, 4, 5].map((rot) => {
    const f = data.decorationFacing(art, rot, hasArt);
    const file = f.own ? f.file : art;
    return { file, mirror: f.mirror, art: hasArt(file), image: data.decorationImage(file).split('?')[0] };
  });
}

const decorations: Record<string, unknown> = {};
for (const [id, def] of Object.entries(DECORATIONS)) {
  const model = def.treeModel ?? def.propModel;
  const lightDef = lighting.LIGHT_DEFS[id];
  decorations[id] = {
    name: def.name ?? id,
    footprint: def.footprint, blockingFootprint: def.blockingFootprint ?? null, architectureSpan: !!def.architectureSpan,
    // export_scene.ts kinds: architecture (walls / doors), a real 3D model, else a card
    architecture: !!(def.model3d && ARCHITECTURE.has(def.model3d) && !def.propModel),
    model3d: def.model3d ?? null,
    model: model && MODELS[model] ? { model, file: `/game/models/${MODELS[model].file}`, height: MODELS[model].height } : null,
    mirrorAlternate: !!def.mirrorAlternate,
    exitKind: def.exitKind ?? null,
    size: decorSize(id, def),
    light: lightDef ? { color: lightDef.color, intensity: lightDef.intensity, radius: lightDef.radius * lighting.LIGHT_RADIUS_MUL,
      flicker: lightDef.flicker, minHeight: lighting.MAP_LIGHT_MIN_HEIGHT } : null,
    standOn: STAND_ON_DECOR.has(id),
    lockYaw: data.BARRICADE_LIKE_DECOR.has(id),
    nudge: def.unitLayer === 'behind' || STAND_ON_DECOR.has(id) ? -0.35 : lightDef ? 0.4 : def.foreground || def.unitLayer === 'front' ? 0.2 : 0,
  };
}

// Units: Engine2's dioramaView.placeUnits + UnitActor (idle pose), per catalog entry (unitForSpawn picks the entry from
// a hero's name on the party's side, else from the class).
function unitEntry(def: any) {
  const pose = 'idle';
  const src = def.poses[pose];
  if (!src) return null;
  const info = prepared[`${src.dir}|${src.prefix}`];
  const natW = info?.natW ?? 0, natH = info?.natH ?? 0;
  const sprite = def.sprite;
  const reversed = !!def.drawnFacingLeft || def.classId === 'familiar' || sprite === 'defaultWarrior' || sprite === 'cobalt-blue-deer' || sprite === 'kaelFinal';
  const frames = info?.n ?? src.n ?? 1;
  const pace: number[] = [];
  for (let i = 0; i < 1440; i++) pace.push(visual.framePace(def, pose, frames, i / 240).frame);
  return { key: def.key, sprite, set: src.dir, clip: src.prefix.replace(/-$/, '') || 'base', box: visual.unitBox(def, pose, 0, natW, natH),
    nat: [natW, natH], frames, pace, reversed, light: lighting.UNIT_LIGHT_DEFS[def.classId] ?? null,
    // the footprint's front row (Ember's footprintCentroid), as offsets from the anchor
    front: def.footprint ? def.footprint.filter((o: { dy: number }) => o.dy === 0).map((o: { dx: number }) => o.dx) : null };
}
const heroes: Record<string, unknown> = {};
for (const name of Object.keys(catalog.HERO_NAMES)) {
  const def = catalog.unitForSpawn({ name, classId: '' }, 'player');
  if (def) heroes[name] = { classId: def.classId, ...unitEntry(def) };
}
const classes: Record<string, unknown> = {};
for (const classId of Object.keys(data.CLASSES)) {
  const def = catalog.unitForSpawn({ classId }, 'enemy');
  const e = def ? unitEntry(def) : null;
  if (e) classes[classId] = e;
}

const out = {
  source: 'Engine2 tables via tools/scripts/engine2/export_editor_catalog.ts',
  decorations, facings, heroes, classes,
  backgrounds: { byMap: Object.fromEntries(backgrounds.MAP_BACKGROUNDS.flatMap((b: any) => (b.mapIds ?? []).map((m: string) => [m, b.file]))),
    byId: Object.fromEntries(backgrounds.MAP_BACKGROUNDS.map((b: any) => [b.id, b.file])) },
};
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out));
console.log(`catalog: ${Object.keys(decorations).length} decorations, ${artIds.size} art ids, ${Object.keys(heroes).length} heroes, ${Object.keys(classes).length} classes -> ${outPath}`);
process.exit(0);
