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
const available = await import(`${E2}/src/rules/available-spells.ts`);

const draft = JSON.parse(readFileSync(mapPath, 'utf8')).draft;
const mission = store.draftToMission(draft);
// No art is drawn: every art table reads as empty.
const art = new Proxy({}, { get: () => new Proxy({}, { get: () => [] }) });
// TEST_LEVEL=N: Engine2's Modo teste battle (GameApp startBattle with testMode): every party spawn at level N
// (DEFAULT_TEST_LEVEL is 15) and the engine's test flag (debugFreeCast). The map given is then the test-mode map
// (make_test_map.ts: the missing heroes added). NO_REFS=1 leaves out the reference answers (the game never reads them).
const testLevel = Number(process.env.TEST_LEVEL ?? 0);
const levels: Record<string, number> = testLevel ? Object.fromEntries(mission.playerSpawns.map((s: { name: string }) => [s.name, testLevel])) : {};
const noRefs = process.env.NO_REFS === '1';
// Test battles fight with a brand-new save's gear (GameApp freshTestOverworld + startBattle's roster): the starter weapon
// in hand, Neera's dagger in the off-hand, nothing else owned.
const gear: Record<string, unknown> = {};
if (testLevel) {
  const fresh = (await import(`${E2}/src/game/save.ts`)).emptySave(false);
  gear.weapons = Object.fromEntries(Object.entries(fresh.equipped as Record<string, string>).map(([hero, id]) => [hero, { id, enh: fresh.weapons[id] ?? 0 }]));
  gear.offHand = Object.fromEntries(Object.entries(fresh.equipment as Record<string, { offHand?: string }>).map(([hero, eq]) => [hero, eq.offHand] as const).filter((p) => !!p[1]));
  gear.equipment = fresh.equipment;
  gear.statPointAllocations = fresh.statPointAllocations;
  gear.ownedWeaponIds = Object.keys(fresh.weapons);
  gear.bags = fresh.bags;
  gear.promotions = fresh.promotions;
}
// The editor's preview battle (MapPreviewCanvas): fog off, a fresh party.
const engine = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, ...gear }, 1, testLevel > 0);
const e = engine as unknown as Record<string, any>;

const cells: Record<string, unknown>[] = [];
for (let y = 0; y < e.rows; y++)
  for (let x = 0; x < e.cols; x++) {
    const def = hexprops.hexDef(e.tiles, e.cols, x, y, e.decorOverlay);
    const bits = e.decorOverlay[y * e.cols + x] ?? 0;
    const raw = data.TERRAIN[e.tiles[y * e.cols + x]] ?? data.TERRAIN.plains;
    cells.push({ terrain: e.tiles[y * e.cols + x], passable: !!def.passable, moveCost: def.moveCost, def: def.def ?? 0, atk: def.atk ?? 0,
      blocked: (bits & hexprops.HEX_BLOCKED) !== 0, cover: def.cover ?? 0, hazardDice: def.hazardDice ?? 0, hazardFaces: def.hazardFaces ?? 8,
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
  units.push({ id: u.id, side: u.side, name: u.name, classId: u.classId, className: u.className ?? '', sprite: u.sprite, x: u.x, y: u.y, alive: !!u.alive,
    mov: e.effectiveUnitForReach(u).mov, hp: u.hp, maxHp: u.maxHp, atk: u.atk, def: u.def, mag: u.mag, dex: u.dex ?? 0,
    minRange: u.minRange ?? 1, maxRange: u.maxRange ?? 1, size: u.size ?? 1, summoned: !!u.summoned, level: u.level ?? 1,
    footprintOffsets: u.footprintOffsets ?? null, footprintW: u.footprintW ?? null, footprintH: u.footprintH ?? null, reach: cellsOut,
    initiativeBonus: (u.initiative ?? 0) - (u.initiativeRoll ?? 0),
    // Spells: what the unit may cast (classSpells), its charges, and what its weapon practises (weaponSkills.ts).
    spells: u.spells, spellList: available.classSpells(u.classId, u.level ?? 1, u.name), shockCharges: u.shockCharges ?? 0,
    frostCharges: u.frostCharges ?? 0, fantomForceCharges: u.fantomForceCharges ?? 0,
    fantomForceDice: data.fantomForceDice ? data.fantomForceDice(u.level ?? 1) : null,
    xp: u.xp ?? 0, guaranteedDrop: !!u.guaranteedDrop,
    weaponType: skills.equippedWeaponType(u) ?? null,
    trainsWeapon: !!(skills.equippedWeaponType(u) && skills.weaponTypesForClass(u.classId).includes(skills.equippedWeaponType(u))),
    weaponSkill: skills.equippedWeaponType(u) ? (u.weaponSkills?.[skills.equippedWeaponType(u)] ?? 0) : 0,
    healingSkill: u.healingSkill ?? 0,
    // Level-ups (levelUpUnit): the class's stats and spell uses at every level, and what gear adds over them.
    growth: u.side === 'player' ? Array.from({ length: 30 }, (_, i) => {
      const st = data.statsFor(u.classId, i + 1);
      return [st.hp, st.atk, st.mag, st.def, st.dex, ...Array.from({ length: 10 }, (_, t) => data.tierUses(u.classId, t + 1, i + 1)), st.mov];
    }) : null,
    combat: {
      weapon: w ? { dice: w.dice, faces: w.faces, bonus: w.bonus ?? 0, ranged: !!w.ranged, classBonus: w.bonusClass === u.classId } : null,
      weaponEnh: u.weaponEnh ?? 0,
      mastery: skills.weaponModifiers(u, skills.equippedWeaponType(u)),
      offMastery: skills.weaponModifiers(u, skills.equippedWeaponType(u, true)),
      offHand: off ? { kind: off.kind, minRange: off.minRange ?? 1, maxRange: off.maxRange ?? 1, dice: off.dice ?? 1, faces: off.faces ?? 4, bonus: off.bonus ?? 0 } : null,
      staff: magic ? { element, bonusPct: magic.elementalBonusPct?.[element] ?? 0, bonuses: magic.elementalBonusPct ?? {}, lifeSteal: magic.lifeStealPct ?? 0 } : null,
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
for (let ai = 0; !noRefs && ai < e.units.length; ai++)
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
for (let i = 0; !noRefs && i < e.units.length; i++) {
  if (e.units[i].side !== 'enemy' || !e.units[i].alive) continue;
  const fresh = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
  const u = fresh.units[i];
  fresh.queue = [];
  fresh.runAiFor(u);
  let path: number[][] = [], target = -1, other = '';
  const spells: unknown[] = [];
  for (const q of fresh.queue) {
    if (q.type === 'move' && q.id === u.id) path = q.path.map((p: { x: number; y: number }) => [p.x, p.y]);
    else if (q.type === 'combat' && q.att === u.id) target = fresh.units.findIndex((v: { id: string }) => v.id === q.def);
    else if (q.type === 'spell' && q.att === u.id) spells.push({ kind: q.spellKind, ids: q.ids.map((id: string) => fresh.units.findIndex((v: { id: string }) => v.id === id)), tiles: q.tiles.map((p: { x: number; y: number }) => [p.x, p.y]) });
    else if (q.type !== 'delay') other = q.type;
  }
  ai.push({ unit: i, path, target, other, spells });
}
// The same enemies placed two hexes from the first party member (the first free, open hex at that distance), so the
// attack decision is checked too.
const lead = e.units.find((u: { side: string; alive: boolean }) => u.side === 'player' && u.alive);
const near: unknown[] = [];
for (let i = 0; !noRefs && lead && i < e.units.length; i++) {
  if (e.units[i].side !== 'enemy' || !e.units[i].alive || (e.units[i].size ?? 1) > 1) continue;
  const fresh = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
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
  const spells: unknown[] = [];
  for (const q of fresh.queue) {
    if (q.type === 'move' && q.id === u.id) path = q.path.map((p: { x: number; y: number }) => [p.x, p.y]);
    else if (q.type === 'combat' && q.att === u.id) target = fresh.units.findIndex((v: { id: string }) => v.id === q.def);
    else if (q.type === 'spell' && q.att === u.id) spells.push({ kind: q.spellKind, ids: q.ids.map((id: string) => fresh.units.findIndex((v: { id: string }) => v.id === id)), tiles: q.tiles.map((p: { x: number; y: number }) => [p.x, p.y]) });
    else if (q.type !== 'delay') other = q.type;
  }
  near.push({ unit: i, at: [spot.x, spot.y], path, target, other, spells });
}

// Spell casts: every unit, every spell it may cast that EmberEngine ports, aimed at every hex within 9; Engine2 checks the
// aim (spellAimValid) and, when valid, casts it with a seeded mulberry32 and plays it out (tick) on a copy of the battle.
// Recorded: valid or not, how many random numbers it drew, and every unit's HP / life / lightning echo afterwards plus
// the caster's charges.
const PORTED = ['magicMissile', 'longShot', 'piercing', 'piercingThrust', 'doubleStrike', 'lightning', 'frost', 'cureMinor', 'shock'];
const CAST: Record<string, string> = { magicMissile: 'castMagicMissile', longShot: 'castLongShot', piercing: 'castPiercing', piercingThrust: 'castPiercingThrust',
  doubleStrike: 'castDoubleStrike', lightning: 'castLightning', frost: 'castFrost', shock: 'castShock' };
// The engine's effects draw from the battle's own random numbers; here they draw from Math.random instead, so only the
// rules count.
const FX = ['emitParticle', 'emitLevelUpFx', 'emitPotionZeroFx', 'emitHolyFx', 'queueElementalFx', 'emitFireballBurstFx', 'emitMissileFx',
  'emitLightningFx', 'emitPortalFx', 'emitWarpFx', 'spawnMiss', 'spawnHit', 'emitBladeFx'];
const casts: unknown[] = [];
// SPELL_REFS=1: the cast references for Cooker check-battle (large; the game itself does not read them).
if (process.env.SPELL_REFS === '1') {
  const fresh = () => new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
  const base = fresh();
  const pristine = structuredClone(base.units);
  let castSeed = 1000;
  for (let ci = 0; ci < base.units.length; ci++) {
    const me = base.units[ci];
    const kinds = new Set<string>(available.classSpells(me.classId, me.level ?? 1, me.name));
    if (me.side === 'enemy') {
      // enemies cast from their AI's own charge pools
      if ((me.spells?.tier1 ?? 0) > 0) { kinds.add('magicMissile'); kinds.add('longShot'); }
      if ((me.spells?.tier2 ?? 0) > 0) { kinds.add('lightning'); kinds.add('piercing'); }
      if ((me.frostCharges ?? 0) > 0) kinds.add('frost');
      if ((me.shockCharges ?? 0) > 0) kinds.add('shock');
    }
    // Where it casts from: where it stands, and (a size-1 caster) the first free open hex 2 from the nearest foe.
    const spots: { x: number; y: number }[] = [{ x: me.x, y: me.y }];
    if ((me.size ?? 1) === 1) {
      const occ = pathfinding.occupancy(base.units);
      let foe: any = null;
      for (const v of base.units) if (v.alive && v.side !== me.side && v.side !== 'neutral' && (!foe || pathfinding.hexDist(v, me) < pathfinding.hexDist(foe, me))) foe = v;
      let spot: { x: number; y: number } | null = null;
      for (let y = 0; foe && y < base.rows && !spot; y++)
        for (let x = 0; x < base.cols && !spot; x++)
          if (pathfinding.hexDist({ x, y }, foe) === 2 && hexprops.hexDef(base.tiles, base.cols, x, y, base.decorOverlay).passable && !occ.has(pathfinding.key(x, y))) spot = { x, y };
      if (spot) spots.push(spot);
    }
    for (const kind of kinds) {
      if (!PORTED.includes(kind)) continue;
      for (const at of spots)
        for (let y = 0; y < base.rows; y++)
          for (let x = 0; x < base.cols; x++) {
            if (pathfinding.hexDist({ x, y }, at) > 9) continue;
            const b = fresh();
            b.units = structuredClone(pristine);
            const u = b.units[ci];
            u.x = at.x, u.y = at.y, u.drawX = at.x, u.drawY = at.y;
            b.selectedId = u.id;
            b.spellKind = kind;
            b.mode = 'awaitSpell';
            b.missileTargets = [];
            if (!b.spellAimValid(u, { x, y })) { casts.push([ci, kind, x, y, 0, at.x, at.y]); continue; }
            const s = castSeed++;
            const gen = combat.mulberry32(s);
            let calls = 0;
            b.rng = () => { calls++; return gen(); };
            for (const name of FX) {
              const original = b[name];
              if (typeof original !== 'function') continue;
              b[name] = (...args: unknown[]) => { const keep = b.rng; b.rng = Math.random; try { return original.apply(b, args); } finally { b.rng = keep; } };
            }
            // Played out until the cast's last step completes (finishCombat with nothing queued): later ticks belong
            // to whatever the battle does next.
            let done = false, doneCalls = 0;
            const finish = b.finishCombat.bind(b);
            b.finishCombat = (att: unknown) => { finish(att); if (!b.queue.length && !done) { done = true; doneCalls = calls; } };
            b.queue = [];
            if (kind === 'cureMinor') b.castHeal(u, { x, y }, 'cureMinor');
            else if (kind === 'magicMissile') {
              // Every dart picked on the same hex (magicMissileCount(level) clicks, as a player repeats a target).
              for (let k = 0; k < 16 && !b.queue.length && !b.active && b.spellKind === 'magicMissile'; k++) b[CAST[kind]](u, { x, y });
            } else b[CAST[kind]](u, { x, y });
            if (!b.queue.length && !b.active) { castSeed--; casts.push([ci, kind, x, y, 0, at.x, at.y]); continue; }  // nothing cast
            for (let i = 0; i < 4000 && !done && (b.active || b.queue.length); i++) b.tick(0.05);
            if (done) calls = doneCalls;
            const after = b.units.map((v: any) => [v.hp, v.alive ? 1 : 0, v.shock ? [v.shock.dice, v.shock.faces, v.shock.bonus, v.shock.mag] : 0, v.maxHp, v.level, v.xp ?? 0]);
            casts.push([ci, kind, x, y, 1, at.x, at.y, s, calls, after, u.spells, u.shockCharges ?? 0, u.frostCharges ?? 0]);
          }
    }
  }
}

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ source: 'Engine2 src/game/engine.ts BattleEngine via tools/scripts/engine2/export_battle.ts',
  map: draft.id, cols: e.cols, rows: e.rows, debugFreeCast: testLevel > 0, cells, units, affinityBonus, forecasts, rolls, ai, aiNear: near, casts }));
console.log(`${draft.id}: ${e.cols}x${e.rows}, ${units.length} units (${units.map((u) => `${u.name} mov ${u.mov}`).join(', ')})`);
process.exit(0);  // the engine leaves timers running
