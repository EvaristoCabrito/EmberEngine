// Battle rules for one Ember map, from Engine2's own BattleEngine run headless (read-only): every cell's terrain as the
// movement rules see it (the decorations' blocking folded in: hexprops.hexDef), every unit the battle starts with
// (stats, size, body shape) and, as the reference EmberEngine's port is checked against (Cooker check-battle), each
// unit's movement reach from Engine2's computeReachable (cost and parent of every reachable hex). For combat: each
// unit's combat profile as Engine2 resolves it (weapon, mastery, staff magic, off-hand, resistances, facing), the
// party's affinity bonuses, and reference answers from Engine2's combat.ts: forecasts (makeForecast + canHitFrom) for
// every pair of foes, from where the attacker stands and from each hex around the defender, and seeded rolls
// (rollDamage with mulberry32 + the engine's arcane / staff element step). For turns: each unit's initiative bonus,
// and each enemy's opening decision from Engine2's own runAiFor (a fresh battle per enemy): its walk and its target.
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
const data = await import(`${E2}/src/game/data.ts`);
const combat = await import(`${E2}/src/game/combat.ts`);
const skills = await import(`${E2}/src/game/weaponSkills.ts`);
const resist = await import(`${E2}/src/game/resistances.ts`);
const staff = await import(`${E2}/src/ember/mageStaffMagic.ts`);
const affinity = await import(`${E2}/src/game/affinity.ts`);

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
    const raw = data.TERRAIN[e.tiles[y * e.cols + x]] ?? data.TERRAIN.plains;
    cells.push({ terrain: e.tiles[y * e.cols + x], passable: !!def.passable, moveCost: def.moveCost, def: def.def ?? 0, atk: def.atk ?? 0,
      blocked: (bits & hexprops.HEX_BLOCKED) !== 0,
      // combat: the tile's own terrain (combat.ts terrainBonus, effectiveMaxRange) and, for shots, the cell with the
      // decorations folded in (shotBlocker)
      raw: { def: raw.def ?? 0, atk: raw.atk ?? 0, height: raw.height ?? 0, cover: raw.cover ?? 0, blocksShot: !!raw.blocksShot },
      shot: { height: def.height ?? 0, blocksShot: !!def.blocksShot } });
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
  const w = u.weaponId ? data.WEAPONS[u.weaponId] : undefined;
  const magic = e.staffMagic(u);
  const element = magic?.attackElement ?? 'arcane';
  const off = u.offHandId ? data.EQUIPMENT[u.offHandId] : undefined;
  units.push({ id: u.id, side: u.side, name: u.name, classId: u.classId, sprite: u.sprite, x: u.x, y: u.y, alive: !!u.alive,
    mov: e.effectiveUnitForReach(u).mov, hp: u.hp, maxHp: u.maxHp, atk: u.atk, def: u.def, mag: u.mag, dex: u.dex ?? 0,
    minRange: u.minRange ?? 1, maxRange: u.maxRange ?? 1, size: u.size ?? 1, summoned: !!u.summoned, level: u.level ?? 1,
    footprintOffsets: u.footprintOffsets ?? null, footprintW: u.footprintW ?? null, footprintH: u.footprintH ?? null, reach: cellsOut,
    initiativeBonus: (u.initiative ?? 0) - (u.initiativeRoll ?? 0),
    combat: {
      weapon: w ? { dice: w.dice, faces: w.faces, bonus: w.bonus ?? 0, ranged: !!w.ranged, classBonus: w.bonusClass === u.classId } : null,
      weaponEnh: u.weaponEnh ?? 0,
      mastery: skills.weaponModifiers(u, skills.equippedWeaponType(u)),
      offMastery: skills.weaponModifiers(u, skills.equippedWeaponType(u, true)),
      offHand: off ? { kind: off.kind, minRange: off.minRange ?? 1, maxRange: off.maxRange ?? 1, dice: off.dice ?? 1, faces: off.faces ?? 4, bonus: off.bonus ?? 0 } : null,
      staff: magic ? { element, bonusPct: magic.elementalBonusPct?.[element] ?? 0 } : null,
      arcane: e.isArcaneCaster(u),
      arrow: e.isArrowAttack(u),
      resistances: u.resistances ?? {},
      faceDx: u.faceDx ?? null, faceDy: u.faceDy ?? null, facing: u.facing ?? 1,
    } });
}

// The party's affinity bonus for each pair of names (affinity.ts affinityBonus of the pair's score).
const affinityBonus: Record<string, Record<string, number>> = {};
for (const a of e.units) for (const b of e.units)
  if (a.side === 'player' && b.side === 'player' && a.name !== b.name) {
    const bonus = affinity.affinityBonus(affinity.affinityScore(e.affinityScores, a.name, b.name));
    if (bonus) (affinityBonus[a.name] ??= {})[b.name] = bonus;
  }

// Reference answers: every attacker against every foe, from its own hex and from each hex around the foe.
const forecasts: unknown[] = [];
const rolls: unknown[] = [];
let seed = 1;
for (let ai = 0; ai < e.units.length; ai++)
  for (let bi = 0; bi < e.units.length; bi++) {
    const a = e.units[ai], b = e.units[bi];
    if (a.side === b.side || !a.alive || !b.alive) continue;
    const spots = [{ x: a.x, y: a.y }, ...pathfinding.hexNeighbors(b.x, b.y).filter((p: { x: number; y: number }) => pathfinding.inBounds(p.x, p.y, e.cols, e.rows))];
    for (const spot of spots) {
      const fake = { ...a, x: spot.x, y: spot.y };
      const att = e.affinityUnit(fake), def = e.affinityUnit(b);
      const attTile = pathfinding.tileAt(e.tiles, e.cols, spot.x, spot.y), defTile = pathfinding.tileAt(e.tiles, e.cols, b.x, b.y);
      const f = combat.makeForecast(att, def, attTile, defTile, e.tiles, e.cols);
      const canHit = pathfinding.canHitFrom(fake, fake, b, e.tiles, e.cols, e.decorOverlay);
      forecasts.push([ai, bi, spot.x, spot.y, f.dmgOut, f.dmgBack, f.hitOut, f.hitBack, f.canCounter ? 1 : 0, f.kill ? 1 : 0, canHit ? 1 : 0]);
      // A real hit (BattleEngine "hit" stage, plain attack): rollDamage, then the arcane bolt / staff element step.
      const s = seed++;
      const r = combat.rollDamage(att, def, attTile, defTile, combat.mulberry32(s));
      let final = r.dmg;
      const usesArcane = !e.isArrowAttack(fake) && e.isArcaneCaster(fake);
      const m = e.staffMagic(fake);
      if (r.landed && (usesArcane || m)) {
        const el = m?.attackElement ?? 'arcane';
        final = Math.floor(resist.elementalDamage(staff.staffElementalDamage(final, m, el), b.resistances?.[el] ?? 0, att.mag));
      }
      rolls.push([ai, bi, spot.x, spot.y, s, r.dmg, r.crit ? 1 : 0, r.landed ? 1 : 0, r.hitChance, r.preCritDmg, final]);
    }
  }

// Each enemy's opening move from Engine2's AI, on a fresh battle so no other decision changes the board.
const ai: unknown[] = [];
for (let i = 0; i < e.units.length; i++) {
  if (e.units[i].side !== 'enemy' || !e.units[i].alive) continue;
  const fresh = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels: {} }, 1) as unknown as Record<string, any>;
  const u = fresh.units[i];
  fresh.queue = [];
  fresh.runAiFor(u);
  let path: number[][] = [], target = -1, other = '';
  for (const q of fresh.queue) {
    if (q.type === 'move' && q.id === u.id) path = q.path.map((p: { x: number; y: number }) => [p.x, p.y]);
    else if (q.type === 'combat' && q.att === u.id) target = fresh.units.findIndex((v: { id: string }) => v.id === q.def);
    else if (q.type !== 'delay') other = q.type;
  }
  ai.push({ unit: i, path, target, other });
}
// The same enemies placed two hexes from the first party member (the first free, open hex at that distance), so the
// attack decision is checked too.
const lead = e.units.find((u: { side: string; alive: boolean }) => u.side === 'player' && u.alive);
const near: unknown[] = [];
for (let i = 0; lead && i < e.units.length; i++) {
  if (e.units[i].side !== 'enemy' || !e.units[i].alive || (e.units[i].size ?? 1) > 1) continue;
  const fresh = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels: {} }, 1) as unknown as Record<string, any>;
  const u = fresh.units[i];
  const occ = pathfinding.occupancy(fresh.units);
  let spot: { x: number; y: number } | null = null;
  for (let y = 0; y < fresh.rows && !spot; y++)
    for (let x = 0; x < fresh.cols && !spot; x++)
      if (pathfinding.hexDist({ x, y }, lead) === 2 && hexprops.hexDef(fresh.tiles, fresh.cols, x, y, fresh.decorOverlay).passable && !occ.has(pathfinding.key(x, y))) spot = { x, y };
  if (!spot) continue;
  u.x = spot.x, u.y = spot.y, u.drawX = spot.x, u.drawY = spot.y;
  fresh.queue = [];
  fresh.runAiFor(u);
  let path: number[][] = [], target = -1, other = '';
  for (const q of fresh.queue) {
    if (q.type === 'move' && q.id === u.id) path = q.path.map((p: { x: number; y: number }) => [p.x, p.y]);
    else if (q.type === 'combat' && q.att === u.id) target = fresh.units.findIndex((v: { id: string }) => v.id === q.def);
    else if (q.type !== 'delay') other = q.type;
  }
  near.push({ unit: i, at: [spot.x, spot.y], path, target, other });
}

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ source: 'Engine2 src/game/engine.ts BattleEngine via tools/scripts/engine2/export_battle.ts',
  map: draft.id, cols: e.cols, rows: e.rows, cells, units, affinityBonus, forecasts, rolls, ai, aiNear: near }));
console.log(`${draft.id}: ${e.cols}x${e.rows}, ${units.length} units (${units.map((u) => `${u.name} mov ${u.mov}`).join(', ')})`);
process.exit(0);  // the engine leaves timers running
