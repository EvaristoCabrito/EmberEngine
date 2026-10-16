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

// --- the editor's palettes (Engine2 src/editor/ember/MapEditorScreen.tsx; its module-private tables copied as written) ---
const tv = await import(`${E2}/src/ember/tileVariants.ts`);
const battleLight = await import(`${E2}/src/ember/battleLight.ts`);
const npcs = await import(`${E2}/src/ember/encounter-npcs.ts`);
const byName = (a: string, b: string) => a.localeCompare(b, 'pt-BR', { sensitivity: 'base' });
const BUILDER_TERRAIN = ['plains', 'woods', 'water', 'ruins', 'ember', 'hill', 'flame', 'nave', 'column', 'door', 'void', 'snow'];
const TERRAIN_SWATCH: Record<string, string> = { plains: '#9c8f6f', woods: '#3f5c3a', ruins: '#6b6560', water: '#2c5f7a', ember: '#7a2c2c', hill: '#8a7a4f',
  flame: '#b5501f', column: '#4a4a52', nave: '#26262c', barricade: '#5a4630', door: '#4a3524', void: '#000000', snow: '#d8dee2' };
const HIDDEN_VARIANTS: Record<string, number[]> = { plains: [15, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38], water: [3, 7] };
// thumbFor (the procedural plains 001 has no file: the swatch stands in) and groundVersions
const thumbFor = (terrain: string, variant: number): string | null =>
  terrain === 'void' ? null : terrain === 'plains' && variant === 48 ? '/game/ground-claude/049-plains/color.png'
    : terrain === 'plains' && variant === 0 ? null : tv.tileVariantSrc(terrain, variant);
function groundVersions(terrain: string) {
  const list: { variant: number; thumb: string | null; label?: string }[] = [{ variant: 0, thumb: thumbFor(terrain, 0) }];
  if (terrain === 'plains') list.push({ variant: 48, thumb: thumbFor('plains', 48) });
  const claude = tv.CLAUDE_VARIANT[terrain];
  if (claude !== undefined) list.push({ variant: claude, thumb: thumbFor(terrain, claude) });
  for (const entry of tv.ENGINE2_V3_GROUND_VARIANTS.filter((item: any) => item.terrain === terrain && item.key !== 'city'))
    list.push({ variant: entry.variant, thumb: thumbFor(terrain, entry.variant) });
  for (const entry of tv.GROUND_VARIANT_LAYERS.filter((item: any) => item.terrain === terrain && item.key !== 'photo-city' && item.key !== 'city-stone-paving' && item.key !== 'city-indoor-wood' && item.key !== 'variant-flame-005-fire' && item.key !== 'laje-021-stone-slabs'))
    list.push({ variant: entry.variant, thumb: thumbFor(terrain, entry.variant) });
  if (terrain === 'plains') { const k = list.findIndex((v) => v.variant === 55); if (k > 1) list.splice(1, 0, ...list.splice(k, 1)); }
  if (terrain === 'plains') return list.map((v, i) => ({ ...v, label: String(i + 1).padStart(3, '0') }));
  return list.map((v) => ({ ...v, label: tv.GROUND_VARIANT_LAYERS.find((item: any) => item.terrain === terrain && item.variant === v.variant)?.label ?? String(v.variant + 1).padStart(3, '0') }));
}
// decorationSectionFor (as written in the editor)
const decorationSectionFor = (id: string) => {
  if (DECORATIONS[id]?.exitKind) return 'Waypoints';
  if (id === 'merchant-covered-cart-001' || id === 'city-market-stall' || id === 'city-market-stall-2' || id === 'city-market-wagon-new') return 'Shops';
  if (lighting.LIGHT_DEFS[id]) return 'Lights';
  if (data.HOUSE_DECOR_IDS.has(id) || data.BIG_HOUSE_DECOR_IDS.has(id)) return 'Houses';
  if (data.DEADWOODS_DECOR_IDS.has(id)) return 'Madeira Morta';
  if (data.FOREST_DECOR_IDS.has(id)) return 'Forest';
  if (['barricade', 'wooden-barricade-1', 'city-spike-barricade-low', 'city-palisade-frame', 'city-wattle-fence', 'city-wooden-barricade', 'city-palisade-banner',
    'city-spike-barricade-large', 'city-spike-barricade', 'city-banner-barricade', 'city-stone-banner-wall'].includes(id)) return 'Barricada';
  if (id.startsWith('wilds-')) return 'Wilds';
  if (id.startsWith('cave-')) return 'Cave';
  if (id.startsWith('torture-')) return 'Torture';
  if (id.startsWith('city-')) return 'City';
  if (id.includes('bridge') || id.includes('ember-channels')) return 'Pontes';
  if (id.includes('mountain') || id.includes('ridge') || id.includes('rock') || id.includes('boulder') || id.includes('spike') || id.includes('cliff')) return 'Pedras e relevo';
  if (id.includes('tree') || id.includes('forest') || id.includes('wood') || id.includes('log') || id.includes('mossy')) return 'Natureza';
  if (id.includes('ruined') || id.includes('tower') || id.includes('mansion') || id.includes('wall') || id.includes('gate') || id.includes('shrine') || id.includes('house') || id.includes('hut') || id.includes('hamlet')) return 'Ruínas e construções';
  return 'Objetos';
};
const terrainHint = (t: string) => {
  const d = data.TERRAIN[t];
  const parts = [d.name, d.passable ? `Mov ${d.moveCost}` : 'Intransponível', `Def +${d.def}`, `Atk +${d.atk}`];
  if (d.blocksShot) parts.push('bloqueia tiro/visão');
  if (d.hazardDice) parts.push(`dano ${d.hazardDice}D${d.hazardFaces} ao entrar e a cada turno`);
  const note = data.terrainNote(t);
  return note ? `${parts.join(' · ')} — ${note}` : parts.join(' · ');
};
const editor = {
  minGrid: data.MIN_GRID, maxGrid: data.MAX_GRID, defaultTestLevel: 15, enemyLevel0: data.enemyLevelFor(0),
  terrains: BUILDER_TERRAIN.map((t) => ({ id: t, name: data.TERRAIN[t].name, swatch: TERRAIN_SWATCH[t], variantCount: tv.TILE_VARIANT_COUNT[t] ?? 1,
    height: data.TERRAIN[t].height ?? 0, hint: terrainHint(t), hidden: HIDDEN_VARIANTS[t] ?? [], versions: groundVersions(t) })),
  terrainInfo: Object.fromEntries(Object.keys(data.TERRAIN).map((t) => [t, { name: data.TERRAIN[t].name, height: data.TERRAIN[t].height ?? 0,
    passable: !!data.TERRAIN[t].passable, variantCount: tv.TILE_VARIANT_COUNT[t] ?? 1 }])),
  decorationOrder: Object.values(DECORATIONS).filter((d: Def) => !d.model3d).sort((a: Def, b: Def) => byName(a.name, b.name)).map((d: Def) => d.id),
  decorationSections: ['Todas', 'Barricada', 'Cave', 'City', 'Forest', 'Houses', 'Lights', 'Madeira Morta', 'Natureza', 'Objetos', 'Pedras e relevo', 'Pontes',
    'Ruínas e construções', 'Shops', 'Torture', 'Waypoints', 'Wilds'],
  decorationInfo: Object.fromEntries(Object.keys(DECORATIONS).map((id) => [id, { section: decorationSectionFor(id), tile: DECORATIONS[id].tile ?? null,
    thumb: data.decorationImage(id).split('?')[0],
    blocksByDefault: data.BARRICADE_LIKE_DECOR.has(id) || data.HOUSE_DECOR_IDS.has(id) || data.BIG_HOUSE_DECOR_IDS.has(id) || data.SOLID_HOUSE_DECOR_IDS.has(id) ||
      data.SOLID_CART_DECOR_IDS.has(id) || data.SOLID_ROCK_DECOR_IDS.has(id) }])),
  classes: Object.fromEntries(Object.keys(data.CLASSES).map((c) => [c, { name: data.CLASSES[c].name, role: data.CLASSES[c].role ?? '', summon: !!data.CLASSES[c].summon }])),
  classOrder: Object.keys(data.CLASSES).sort((a, b) => byName(data.CLASSES[a].name, data.CLASSES[b].name)),
  editorHeroes: [{ name: 'Kael', classId: 'kaelFinal' }, { name: 'Neera', classId: 'neera' }, { name: 'Voss', classId: 'voss' }, { name: 'Salazar', classId: 'salazar' },
    { name: 'Aldric', classId: 'aldric' }, { name: 'Malrec', classId: 'conjurer' }].sort((a, b) => byName(a.name, b.name)),
  npcs: ['breadLady', ...npcs.ENCOUNTER_NPC_IDS].map((id: string) => ({ id, name: data.CLASSES[id]?.name ?? id,
    spawn: id === 'breadLady' ? null : npcs.encounterNpcSpawn(id, 0, 0) })),
  timeOfDay: Object.fromEntries(Object.entries(battleLight.TIME_OF_DAY_LIGHT).map(([k, v]: [string, any]) => [k, { label: v.label, moon: v.moon, key: v.key, ambient: v.ambient }])),
  defaults: { sun: battleLight.DEFAULT_SUN_INTENSITY, ambient: battleLight.DEFAULT_AMBIENT_INTENSITY, bloom: battleLight.DEFAULT_BLOOM_INTENSITY },
  backgrounds: backgrounds.MAP_BACKGROUNDS.map((b: any) => ({ id: b.id, label: b.label, file: b.file })),
  music: [...data.MUSIC_TRACKS].sort(byName),
  // the City brush: plains with the city layers (Stone paving = ENGINE2_V3_CITY_VARIANT, then photo-city / indoor wood)
  city: [{ variant: tv.ENGINE2_V3_CITY_VARIANT, label: 'Stone paving', thumb: thumbFor('plains', tv.ENGINE2_V3_CITY_VARIANT) },
    ...tv.GROUND_VARIANT_LAYERS.filter((e: any) => e.key === 'photo-city' || e.key === 'city-indoor-wood').map((e: any) => ({ variant: e.variant, label: e.label, thumb: thumbFor('plains', e.variant) }))],
  terrainThumb: Object.fromEntries(BUILDER_TERRAIN.map((t) => [t, thumbFor(t, 0)])),
};

const out = {
  source: 'Engine2 tables via tools/scripts/engine2/export_editor_catalog.ts',
  decorations, facings, heroes, classes, editor,
  backgrounds: { byMap: Object.fromEntries(backgrounds.MAP_BACKGROUNDS.flatMap((b: any) => (b.mapIds ?? []).map((m: string) => [m, b.file]))),
    byId: Object.fromEntries(backgrounds.MAP_BACKGROUNDS.map((b: any) => [b.id, b.file])) },
};
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out));
console.log(`catalog: ${Object.keys(decorations).length} decorations, ${artIds.size} art ids, ${Object.keys(heroes).length} heroes, ${Object.keys(classes).length} classes -> ${outPath}`);
process.exit(0);
