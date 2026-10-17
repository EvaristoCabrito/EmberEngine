// Battle rules for one Ember map, from Engine2's own BattleEngine run headless (read-only): every cell's terrain as the
// movement rules see it (the decorations' blocking folded in: hexprops.hexDef), every unit the battle starts with
// (stats, size, body shape) and, as the reference EmberEngine's port is checked against (Cooker check-battle), each
// unit's movement reach from Engine2's computeReachable (cost and parent of every reachable hex). For combat: each
// unit's combat profile as Engine2 resolves it (weapon, mastery, staff magic, off-hand, resistances, facing), the
// party's affinity bonuses, and reference answers from Engine2's combat.ts: forecasts (makeForecast + canHitFrom) for
// every pair of foes, from where the attacker stands and from each hex around the defender, and seeded rolls
// (rollDamage with mulberry32 + the engine's arcane / staff element step). For turns: each unit's initiative bonus,
// and each enemy's opening decision from Engine2's own runAiFor (a fresh battle per enemy): its walk and its target.
// Sound: each unit's cues as Engine2's engine.ts picks them through audio.ts (attack, counter, every spell it may cast,
// heal, walk, hit, death; which file, volume, start offset, delay and player), recorded by a stand-in for the browser's
// Audio element; the level-up cue; the mission's music (GameApp's music chain).
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
// audio.ts plays through HTMLAudioElement: this recorder stands in for it (a play is noted with its file, volume, start
// offset, the delay a setTimeout put before it, and whether it is a fresh copy (cloneNode: playSfxFile) or the file's
// one reused element (playSfxFileExclusive)).
type Played = { src: string; volume: number; offset: number; delay: number; exclusive: boolean };
const played: Played[] = [];
let playDelay = 0;
class RecordingAudio {
  src: string; volume = 1; currentTime = 0; muted = false; paused = true; preload = ''; loop = false; layered = false;
  dataset: Record<string, string> = {};
  constructor(src = '') { this.src = src; }
  load() {} addEventListener() {} setAttribute() {} pause() { this.paused = true; }
  cloneNode() { const c = new RecordingAudio(this.src); c.layered = true; return c; }
  play() { played.push({ src: this.src, volume: this.volume, offset: this.currentTime, delay: playDelay, exclusive: !this.layered }); this.paused = false; return Promise.resolve(); }
}
g.Audio = RecordingAudio;

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
const audio = await import(`${E2}/src/game/audio.ts`);
const gameResist = await import(`${E2}/src/game/resistances.ts`);

const draft = JSON.parse(readFileSync(mapPath, 'utf8')).draft;
const mission = store.draftToMission(draft);
// PLANT_TEST=1: the map's enemies swapped for Carnivorous Plants and Saplings (no proof map has any), for the plants' AI references
// (tryAiPlantSwipe / CausticVenom / PoisonBreath); the units, their spells and the decisions all come from this variant.
if (process.env.PLANT_TEST === '1')
  mission.enemySpawns.forEach((s: any, i: number) => { if (i % 3 === 0) s.classId = 'carnivorousPlant'; else if (i % 3 === 1) s.classId = 'sapling'; });
// No art is drawn: every art table reads as empty.
const art = new Proxy({}, { get: () => new Proxy({}, { get: () => [] }) });
// TEST_LEVEL=N: Engine2's Modo teste battle (GameApp startBattle with testMode): every party spawn at level N
// (DEFAULT_TEST_LEVEL is 15) and the engine's test flag (debugFreeCast). The map given is then the test-mode map
// (make_test_map.ts: the missing heroes added). NO_REFS=1 leaves out the reference answers (the game never reads them).
const testLevel = Number(process.env.TEST_LEVEL ?? 0);
// LEVELS_FROM_DRAFT=1 (the map editor's Testar): each spawn's own level from the draft (MapEditorScreen onPlaytest:
// heroes by name, enemies and neutrals by their index in their list), TEST_LEVEL for a hero without one.
const fromDraft = process.env.LEVELS_FROM_DRAFT === '1';
const levels: Record<string, number> = fromDraft
  ? Object.fromEntries(draft.playerSpawns.map((s: { name: string; level?: number }) => [s.name, s.level ?? testLevel]))
  : testLevel ? Object.fromEntries(mission.playerSpawns.map((s: { name: string }) => [s.name, testLevel])) : {};
const enemyLevels = fromDraft ? Object.fromEntries(draft.enemySpawns.map((s: { level?: number }, i: number) => [i, s.level])) : undefined;
const neutralLevels = fromDraft ? Object.fromEntries((draft.neutralSpawns ?? []).map((s: { level?: number }, i: number) => [i, s.level])) : undefined;
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
// PROMOTIONS='{"Kael":"paladin"}': heroes promoted as a save carries them (save.promotions), for references of a
// promoted class's spells (Mãos Curativas is the paladin's).
if (process.env.PROMOTIONS) gear.promotions = { ...(gear.promotions as Record<string, string> ?? {}), ...JSON.parse(process.env.PROMOTIONS) };
// SAVE='{"unitHp":{..},"bags":{..},"heroDiseases":{..},"heroPoisons":{..},"heroHunger":{..},"hungerStreak":N}': the heroes as a
// campaign save brings them (GameApp startBattle's roster: hp, bags, heroDiseases, heroPoisons, heroHunger and
// hungerPenaltyFor(hungerStreak)), for Sandbox --roster-check.
if (process.env.SAVE) {
  const s = JSON.parse(process.env.SAVE);
  const ow = await import(`${E2}/src/game/overworld.ts`);
  if (s.unitHp) gear.hp = s.unitHp;
  if (s.bags) gear.bags = s.bags;
  gear.heroDiseases = s.heroDiseases ?? {}, gear.heroPoisons = s.heroPoisons ?? {}, gear.heroHunger = s.heroHunger ?? {};
  gear.hungerPenaltyPct = ow.hungerPenaltyFor(s.hungerStreak ?? 0);
}
// The editor's preview battle (MapPreviewCanvas): fog off, a fresh party.
const engine = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0);
const e = engine as unknown as Record<string, any>;

// One hex as the battle sees it (also the "after" of a lock that opens).
const cellRecord = (eng: any, x: number, y: number): Record<string, unknown> => {
  const def = hexprops.hexDef(eng.tiles, eng.cols, x, y, eng.decorOverlay);
  const bits = eng.decorOverlay[y * eng.cols + x] ?? 0;
  const raw = data.TERRAIN[eng.tiles[y * eng.cols + x]] ?? data.TERRAIN.plains;
  return { terrain: eng.tiles[y * eng.cols + x], passable: !!def.passable, moveCost: def.moveCost, def: def.def ?? 0, atk: def.atk ?? 0,
    blocked: (bits & hexprops.HEX_BLOCKED) !== 0, cover: def.cover ?? 0, hazardDice: def.hazardDice ?? 0, hazardFaces: def.hazardFaces ?? 8,
    // combat: the tile's own terrain (combat.ts terrainBonus, effectiveMaxRange) and, for shots, the cell with the
    // decorations folded in (shotBlocker)
    raw: { def: raw.def ?? 0, atk: raw.atk ?? 0, height: raw.height ?? 0, cover: raw.cover ?? 0, blocksShot: !!raw.blocksShot },
    shot: { height: def.height ?? 0, blocksShot: !!def.blocksShot } };
};
const cells: Record<string, unknown>[] = [];
for (let y = 0; y < e.rows; y++)
  for (let x = 0; x < e.cols; x++) cells.push(cellRecord(e, x, y));

// The mission's waypoints (Engine2 evaluateEnd / exitDecorationHere): a living hero standing on one of an exit decoration's cells offers to
// end the mission (an escape marker with its DEX chance, a dungeon exit or a floor connector outright).
const exits = e.decorations.filter((d: any) => data.DECORATIONS[d.id]?.exitKind).map((d: any) => ({ id: d.id, kind: data.DECORATIONS[d.id].exitKind, x: d.x, y: d.y,
  targetMapId: d.targetMapId ?? null, cells: data.placedFootprint(d).map((f: any) => [d.x + f.dx, d.y + f.dy]) }));

// The locks (Engine2 adjacentLock / useLockpick): the door tiles, the door decorations and the chests, in the order adjacentLock looks at a
// hex, each with the cells its opening changes (Engine2's own useLockpick, run on a copy with a hero set beside it and a Gazua in hand).
const doorDeco = (d: any) => data.DECORATIONS[d.id]?.model3d === 'door' || data.DECORATIONS[d.id]?.model3d === 'secretDoor';
const lockList: { x: number; y: number; kind: string; id: string; tier: string }[] = [];
for (let y = 0; y < e.rows; y++) for (let x = 0; x < e.cols; x++) if (e.tiles[y * e.cols + x] === 'door') lockList.push({ x, y, kind: 'doorTile', id: 'door', tier: 'base' });
for (const d of e.decorations) if (doorDeco(d)) lockList.push({ x: d.x, y: d.y, kind: 'doorDecor', id: d.id, tier: 'base' });
for (const d of e.decorations) if (data.CHEST_DECOR_IDS.has(d.id)) {
  const betterSpot = mission.betterChests?.some((c: any) => c.x === d.x && c.y === d.y) ?? false;
  lockList.push({ x: d.x, y: d.y, kind: 'chest', id: d.id, tier: d.id === 'chest-large' ? 'best' : d.id === 'chest-medium' || betterSpot ? 'better' : 'base' });
}
const freshEngine = () => new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
// free, passable hexes around a lock where a hero could stand
const standSpots = (eng: any, lk: { x: number; y: number }) => {
  const occ = pathfinding.occupancy(eng.units);
  return pathfinding.hexNeighbors(lk.x, lk.y).filter((p: any) => pathfinding.inBounds(p.x, p.y, eng.cols, eng.rows) && hexprops.hexDef(eng.tiles, eng.cols, p.x, p.y, eng.decorOverlay).passable && !occ.has(pathfinding.key(p.x, p.y)));
};
const locks: unknown[] = [];
for (const lk of lockList) {
  const f = freshEngine();
  const spots = standSpots(f, lk);
  const hero = f.units.find((v: any) => v.side === 'player' && v.alive && (v.size ?? 1) === 1);
  let after: unknown[] = [];
  if (spots.length && hero) {
    hero.x = spots[0].x, hero.y = spots[0].y, hero.drawX = hero.x, hero.drawY = hero.y;
    hero.bag.lockpick = 1;
    f.selectedId = hero.id, f.mode = 'selected', f.phase = 'player';
    f.useLockpick();
    for (let i = 0; i < e.cols * e.rows; i++) {
      const rec = cellRecord(f, i % e.cols, Math.floor(i / e.cols));
      if (JSON.stringify(rec) !== JSON.stringify(cells[i])) after.push([i, rec]);
    }
  }
  locks.push({ ...lk, after });
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
    // mov: the reach the battle shows (an explore mission's free roam: the whole board); baseMov: the unit's own.
    mov: e.effectiveUnitForReach(u).mov, baseMov: u.mov, hp: u.hp, maxHp: u.maxHp, atk: u.atk, def: u.def, mag: u.mag, dex: u.dex ?? 0,
    minRange: u.minRange ?? 1, maxRange: u.maxRange ?? 1, size: u.size ?? 1, summoned: !!u.summoned, level: u.level ?? 1,
    footprintOffsets: u.footprintOffsets ?? null, footprintW: u.footprintW ?? null, footprintH: u.footprintH ?? null, reach: cellsOut,
    undead: !!data.isUndeadClass(u.classId), boss: !!data.isBossClass(u.classId), stunned: !!u.stunned, stunTurns: u.stunTurns ?? 0, asleep: !!u.asleep, sleepTurns: u.sleepTurns ?? 0,
    bleeding: !!u.bleeding, bleedRoundsLeft: u.bleedRoundsLeft ?? null, crippled: !!u.crippled, fearTurns: u.fearTurns ?? 0, fearSourceId: u.fearSourceId ?? null,
    blessedHitBonusPct: u.blessedHitBonusPct ?? 0, blessedRoundsLeft: u.blessedRoundsLeft ?? 0, summonerId: u.summonerId ?? null, spellCharges: u.spellCharges ?? 0,
    lifeDrainCharges: u.lifeDrainCharges ?? null,
    initiativeBonus: (u.initiative ?? 0) - (u.initiativeRoll ?? 0),
    // Spells: what the unit may cast (classSpells), its charges, and what its weapon practises (weaponSkills.ts).
    spells: u.spells, spellList: available.classSpells(u.classId, u.level ?? 1, u.name), shockCharges: u.shockCharges ?? 0,
    frostCharges: u.frostCharges ?? 0, fantomForceCharges: u.fantomForceCharges ?? 0,
    fantomForceDice: data.fantomForceDice ? data.fantomForceDice(u.level ?? 1) : null,
    xp: u.xp ?? 0, guaranteedDrop: !!u.guaranteedDrop,
    weaponType: skills.equippedWeaponType(u) ?? null,
    trainsWeapon: !!(skills.equippedWeaponType(u) && skills.weaponTypesForClass(u.classId).includes(skills.equippedWeaponType(u))),
    weaponSkill: skills.equippedWeaponType(u) ? (u.weaponSkills?.[skills.equippedWeaponType(u)] ?? 0) : 0,
    offWeaponType: skills.equippedWeaponType(u, true) ?? null,
    offTrainsWeapon: !!(skills.equippedWeaponType(u, true) && skills.weaponTypesForClass(u.classId).includes(skills.equippedWeaponType(u, true))),
    offWeaponSkill: skills.equippedWeaponType(u, true) ? (u.weaponSkills?.[skills.equippedWeaponType(u, true)] ?? 0) : 0,
    healingSkill: u.healingSkill ?? 0,
    // What it carries into the fight and its conditions (potions, disease, poison, hunger).
    bag: u.bag ?? null, diseased: !!u.diseased, diseaseBase: u.diseaseBase ?? null, poisoned: !!u.poisoned, poisonTier: u.poisonTier ?? null,
    fullness: typeof u.fullness === 'number' ? u.fullness : null, hungerPenaltyPct: u.hungerPenaltyPct ?? 0,
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
      offHand: off ? { kind: off.kind, minRange: off.minRange ?? 1, maxRange: off.maxRange ?? 1, dice: off.dice ?? 1, faces: off.faces ?? 4, bonus: off.bonus ?? 0, dmgMul: off.dmgMul ?? 0.75 } : null,
      staff: magic ? { element, bonusPct: magic.elementalBonusPct?.[element] ?? 0, bonuses: magic.elementalBonusPct ?? {}, lifeSteal: magic.lifeStealPct ?? 0, regeneration: magic.regeneration ?? 0 } : null,
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
  const fresh = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
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
  if (e.units[i].side !== 'enemy' || !e.units[i].alive || ((e.units[i].size ?? 1) > 1 && process.env.PLANT_TEST !== '1')) continue;
  const fresh = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
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

// With a Warp gate open (runAiFor: human-sized hostiles pursue the party through either mouth): the same enemies, two mouths set on free
// hexes 3 and 5 away from the first party member.
const aiWarp: unknown[] = [];
for (let i = 0; !noRefs && lead && i < e.units.length; i++) {
  if (e.units[i].side !== 'enemy' || !e.units[i].alive) continue;
  const fresh = new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
  const u = fresh.units[i];
  const occ = pathfinding.occupancy(fresh.units);
  const freeAt = (d: number, not?: { x: number; y: number }) => {
    for (let y = 0; y < fresh.rows; y++)
      for (let x = 0; x < fresh.cols; x++)
        if (pathfinding.hexDist({ x, y }, lead) === d && hexprops.hexDef(fresh.tiles, fresh.cols, x, y, fresh.decorOverlay).passable && !occ.has(pathfinding.key(x, y)) && !(not && not.x === x && not.y === y)) return { x, y };
    return null;
  };
  const ga = freeAt(3), gb = ga ? freeAt(5, ga) : null;
  if (!ga || !gb) continue;
  fresh.warpGates.push({ a: { ...ga }, b: { ...gb }, roundsLeft: 3, level: 12, casterId: lead.id });
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
  aiWarp.push({ unit: i, gates: [[ga.x, ga.y], [gb.x, gb.y]], path, target, other, spells });
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
  const fresh = () => new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
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
            // the rules of the cast alone: no later turn begins (its poison, its zones) before the board is read
            b.beginUnitTurn = () => {}, b.startNewRound = () => {};
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


// Skill casts: the rest of the weapon skills and spells EmberEngine ports (SPELL_REFS=1). Every one is cast by any unit that
// has it in its class list (and by the first four units, so classes this map lacks are covered) at every hex within 9, from
// where it stands and from a second spot; recorded: the aim's verdict, the random numbers drawn and the board afterwards in
// full (position, statuses, stats, charges of every unit, the zones, every enmity entry).
const E2_KINDS = ['bloodyShot', 'multiShot', 'trip', 'executionerStrike', 'shieldBash', 'cleave', 'shoulderSmash', 'sweep', 'divineWrath', 'stampede',
  'burningHands', 'poisonBreath', 'fireball', 'causticVenom', 'divineBolt', 'minorVenom', 'iceStorm', 'turnUndead', 'lightningTier3', 'lifeDrain',
  'phantasmalForce', 'provoke', 'bullRush', 'bless', 'webOfDreams', 'auraOfProtection', 'intimidatingPresence', 'warp',
  'summonFamiliar', 'summonFamiliar2', 'summonFamiliar3', 'summonFamiliar4', 'summonZombieDog'];
const E2_CAST: Record<string, (b: any, u: any, c: { x: number; y: number }) => void> = {
  bloodyShot: (b, u, c) => b.castBloodyShot(u, c),
  multiShot: (b, u, c) => { for (let k = 0; k < 16 && !b.queue.length && !b.active && b.spellKind === 'multiShot'; k++) b.castMultiShot(u, c); },
  trip: (b, u, c) => b.castTrip(u, c),
  executionerStrike: (b, u, c) => b.castExecutionerStrike(u, c),
  shieldBash: (b, u, c) => b.castShieldBash(u, c),
  cleave: (b, u, c) => b.castCleave(u, c),
  shoulderSmash: (b, u, c) => b.castShoulderSmash(u, c),
  sweep: (b, u) => b.confirmSweep(),
  divineWrath: (b, u, c) => b.castDivineWrath(u, c),
  stampede: (b, u, c) => b.castStampede(u, c),
  burningHands: (b, u, c) => b.castBurningHands(u, c, 'burningHands'),
  poisonBreath: (b, u, c) => b.castBurningHands(u, c, 'poisonBreath'),
  fireball: (b, u, c) => b.castFireball(u, c),
  causticVenom: (b, u, c) => b.castCausticVenom(u, c),
  divineBolt: (b, u, c) => b.castDivineBolt(u, c),
  minorVenom: (b, u, c) => b.castMinorVenom(u, c),
  iceStorm: (b, u, c) => b.castIceStorm(u, c),
  turnUndead: (b, u) => b.castTurnUndead(u),
  lightningTier3: (b, u, c) => b.castLightningTier3(u, c),
  lifeDrain: (b, u, c) => b.castLifeDrain(u, c),
  phantasmalForce: (b, u, c) => b.castPhantasmalForce(u, c),
  provoke: (b, u, c) => b.castProvoke(u, c),
  bullRush: (b, u, c) => b.castBullRush(u, c),
  bless: (b) => b.startBless(),
  webOfDreams: (b, u, c) => b.castWebOfDreams(u, c),
  auraOfProtection: (b) => b.startAuraOfProtection(),
  intimidatingPresence: (b) => b.startIntimidatingPresence(),
  warp: (b, u, c) => b.castWarp(u, c),
  summonFamiliar: (b, u, c) => b.castSummonFamiliar(u, c, 1),
  summonFamiliar2: (b, u, c) => b.castSummonFamiliar(u, c, 2),
  summonFamiliar3: (b, u, c) => b.castSummonFamiliar(u, c, 3),
  summonFamiliar4: (b, u, c) => b.castSummonFamiliar(u, c, 4),
  summonZombieDog: (b, u, c) => b.castSummonFamiliar(u, c, 5),
};
// The tier a start-method checks before it casts: given to the caster when it has none, and recorded so the check does the same.
const E2_NEEDS_TIER: Record<string, [number, number]> = { bless: [1, 2], auraOfProtection: [5, 2], intimidatingPresence: [5, 2], warp: [3, 2], bullRush: [1, 2] };
// Self-centred casts: aimed at the caster's own hex only (their JS aim check is not used).
const E2_SELF = new Set(['turnUndead', 'bless', 'auraOfProtection', 'intimidatingPresence']);
const sideIdx = (v: any) => (v.side === 'player' ? 0 : v.side === 'enemy' ? 1 : 2);
const tierIdx = (v: any) => (v.poisoned ? ['lesser', 'poison', 'greater', 'deadly', 'lethal'].indexOf(v.poisonTier ?? 'lesser') + 1 : 0);
const unitExt = (v: any) => [v.hp, v.alive ? 1 : 0, v.x, v.y, v.stunned ? 1 : 0, v.stunTurns ?? 0, v.asleep ? 1 : 0, v.sleepTurns ?? 0, v.bleeding ? 1 : 0,
  v.bleedRoundsLeft ?? -1, v.crippled ? 1 : 0, v.fearTurns ?? 0, v.atk, v.mag, v.def, v.dex, v.mov, v.poisoned ? 1 : 0, tierIdx(v), v.poisonMag ?? 0, sideIdx(v),
  v.xp ?? 0, v.level, v.maxHp, v.shock ? 1 : 0, v.blessedRoundsLeft ?? 0, v.blessedHitBonusPct ?? 0, v.spellCharges ?? 0, v.lifeDrainCharges ?? -1,
  ...[1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map((t) => v.spells?.[`tier${t}`] ?? 0)];
const cellHash = (cells: Iterable<string>) => { let h = 0, n = 0; for (const k of cells) { const [x, y] = k.split(',').map(Number); h += (x * 1009 + y * 7919) % 1000003; n++; } return [n, h]; };
const zonesExt = (b: any) => ({
  ice: b.iceStormZones.map((z: any) => [z.roundsLeft, ...cellHash(z.cells), z.damageDice, z.damageFaces, z.damageMul, z.casterMag, z.casterLevel, z.side === 'player' ? 0 : z.side === 'enemy' ? 1 : 2]),
  web: b.webZones.map((z: any) => [z.roundsLeft, ...cellHash(z.cells), z.sleepChance ?? 0.25]),
  warp: b.warpGates.map((g: any) => [g.roundsLeft, g.a.x, g.a.y, g.b.x, g.b.y, g.level]),
  aura: b.auraZones.map((z: any) => [z.roundsLeft, ...cellHash(z.cells), z.kind === 'protection' ? 0 : 1, z.side === 'player' ? 0 : z.side === 'enemy' ? 1 : 2, z.pct]),
});
const enmityExt = (b: any) => {
  const out: number[][] = [];
  for (const [foeId, row] of b.enmity as Map<string, Map<string, { ce: number; ve: number }>>) {
    const fi = b.units.findIndex((v: any) => v.id === foeId);
    for (const [heroId, entry] of row) out.push([fi, b.units.findIndex((v: any) => v.id === heroId), entry.ce, entry.ve]);
  }
  return out.sort((a, c) => a[0] - c[0] || a[1] - c[1]);
};
const skillCasts: unknown[] = [];
if (process.env.SPELL_REFS === '1') {
  const fresh = () => new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
  const base = fresh();
  const pristine = structuredClone(base.units);
  let castSeed = 200000;
  for (const kind of E2_KINDS) {
    const kindOut = kind === 'lifeDrain' ? 'familiarLifeDrain' : kind;  // Ember's warlock has a spell of the same id
    for (let ci = 0; ci < base.units.length; ci++) {
      const me = base.units[ci];
      const has = available.classSpells(me.classId, me.level ?? 1, me.name).includes(kind);
      if (!has && (ci >= 4 || kind === 'bullRush' || kind === 'turnUndead' || kind === 'bless')) continue;
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
      for (const at of spots)
        for (let y = 0; y < base.rows; y++)
          for (let x = 0; x < base.cols; x++) {
            if (pathfinding.hexDist({ x, y }, at) > 9) continue;
            if (E2_SELF.has(kind) && (x !== at.x || y !== at.y)) continue;
            const b = fresh();
            b.units = structuredClone(pristine);
            const u = b.units[ci];
            u.x = at.x, u.y = at.y, u.drawX = at.x, u.drawY = at.y;
            b.selectedId = u.id;
            b.spellKind = kind;
            b.mode = 'awaitSpell';
            b.missileTargets = [];
            if (!E2_SELF.has(kind) && !b.spellAimValid(u, { x, y })) { skillCasts.push([ci, kindOut, x, y, 0, at.x, at.y]); continue; }
            const s = castSeed++;
            const gen = combat.mulberry32(s);
            let calls = 0;
            b.rng = () => { calls++; return gen(); };
            for (const name of FX) {
              const original = b[name];
              if (typeof original !== 'function') continue;
              b[name] = (...args: unknown[]) => { const keep = b.rng; b.rng = Math.random; try { return original.apply(b, args); } finally { b.rng = keep; } };
            }
            let done = false, doneCalls = 0;
            const finish = b.finishCombat.bind(b);
            b.finishCombat = (att: unknown) => { finish(att); if (!b.queue.length && !done) { done = true; doneCalls = calls; } };
            b.queue = [];
            // the rules of the cast alone: no later turn begins (its poison, its zones) before the board is read
            b.beginUnitTurn = () => {}, b.startNewRound = () => {};
            const need = E2_NEEDS_TIER[kind];
            if (need && (u.spells[`tier${need[0]}`] ?? 0) <= 0) u.spells[`tier${need[0]}`] = need[1];
            const patched = need && JSON.stringify(u.spells) !== JSON.stringify(pristine[ci].spells);
            const tiersBefore = JSON.stringify(u.spells), zonesBefore = JSON.stringify(zonesExt(b));
            const trace = process.env.CAST_TRACE === `${u.name}:${kind}:${at.x},${at.y}:${x},${y}`;
            if (trace) { const sh = b.spawnHit.bind(b); b.spawnHit = (t: any, d: number, c: boolean, m: boolean) => { console.log(`TRACE hit ${t.name}@${t.x},${t.y} dmg ${d} hp ${t.hp}`); return sh(t, d, c, m); }; }
            E2_CAST[kind](b, u, { x, y });
            const cast = !!b.queue.length || !!b.active || JSON.stringify(u.spells) !== tiersBefore || JSON.stringify(zonesExt(b)) !== zonesBefore;
            if (!cast) { castSeed--; skillCasts.push([ci, kindOut, x, y, 0, at.x, at.y]); continue; }
            for (let i = 0; i < 6000 && !done && (b.active || b.queue.length); i++) b.tick(0.05);
            if (done) calls = doneCalls;
            skillCasts.push([ci, kindOut, x, y, 1, at.x, at.y, s, calls, b.units.map(unitExt), zonesExt(b), enmityExt(b), b.units.length, patched ? need : null, { ...b.affinityScores }]);
          }
    }
  }
}

// The off-hand action (commitOffHandAction, "Mão Secundária"): each party member that can wield a dagger or a shield is given one, then
// strikes the two foes nearest to it from where it stands and from the hexes at 1 and 2 from the foe. Recorded like a skill cast, kind
// "offHand", plus the item's numbers (index 15) so the check can give its own copy the same item.
if (process.env.SPELL_REFS === '1') {
  const fresh = () => new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
  const base = fresh();
  const pristine = structuredClone(base.units);
  let offSeed = 600000;
  const items = [Object.values(data.EQUIPMENT).find((q: any) => q.kind === 'weapon' && q.slot === 'offHand'), data.EQUIPMENT.broquel].filter(Boolean) as any[];
  for (let ci = 0; ci < base.units.length; ci++) {
    const me = base.units[ci];
    if (me.side !== 'player' || !me.alive || (me.size ?? 1) > 1) continue;
    for (const item of items) {
      if (item.usableBy?.length && !item.usableBy.includes(me.classId)) continue;
      const foes = base.units.filter((v: any) => v.alive && v.side === 'enemy').sort((p: any, q: any) => pathfinding.hexDist(me, p) - pathfinding.hexDist(me, q)).slice(0, 2);
      for (const foe of foes) {
        const occ = pathfinding.occupancy(base.units);
        const free = (d: number) => {
          for (let y = 0; y < base.rows; y++)
            for (let x = 0; x < base.cols; x++)
              if (pathfinding.hexDist({ x, y }, foe) === d && hexprops.hexDef(base.tiles, base.cols, x, y, base.decorOverlay).passable && !occ.has(pathfinding.key(x, y))) return { x, y };
          return null;
        };
        const spots = [{ x: me.x, y: me.y }, free(1), free(2)].filter(Boolean) as { x: number; y: number }[];
        for (const at of spots) {
          const b = fresh();
          b.units = structuredClone(pristine);
          const u = b.units[ci];
          u.x = at.x, u.y = at.y, u.drawX = at.x, u.drawY = at.y;
          u.offHandId = item.id;
          const foeNow = b.units.find((v: any) => v.id === foe.id);
          b.selectedId = u.id;
          b.mode = 'awaitAction';
          const type = skills.equippedWeaponType(u, true);
          const patch = { kind: item.kind, minRange: item.minRange ?? 1, maxRange: item.maxRange ?? 1, dice: item.dice ?? 1, faces: item.faces ?? 4, bonus: item.bonus ?? 0,
            dmgMul: item.dmgMul ?? 0.75, weaponType: type ?? null, trains: !!(type && skills.weaponTypesForClass(u.classId).includes(type)), skill: type ? (u.weaponSkills?.[type] ?? 0) : 0,
            mastery: skills.weaponModifiers(u, type) };
          const s = offSeed++;
          const gen = combat.mulberry32(s);
          let calls = 0;
          b.rng = () => { calls++; return gen(); };
          for (const name of FX) {
            const original = b[name];
            if (typeof original !== 'function') continue;
            b[name] = (...args: unknown[]) => { const keep = b.rng; b.rng = Math.random; try { return original.apply(b, args); } finally { b.rng = keep; } };
          }
          let done = false, doneCalls = 0;
          const finish = b.finishCombat.bind(b);
          b.finishCombat = (att: unknown) => { finish(att); if (!b.queue.length && !done) { done = true; doneCalls = calls; } };
          b.queue = [];
          b.beginUnitTurn = () => {}, b.startNewRound = () => {};
          b.commitOffHandAction(u, foeNow, { x: at.x, y: at.y });
          if (!b.queue.length && !b.active) { offSeed--; skillCasts.push([ci, 'offHand', foe.x, foe.y, 0, at.x, at.y, null, null, null, null, null, null, null, null, patch]); continue; }
          for (let i = 0; i < 6000 && !done && (b.active || b.queue.length); i++) b.tick(0.05);
          if (done) calls = doneCalls;
          skillCasts.push([ci, 'offHand', foe.x, foe.y, 1, at.x, at.y, s, calls, b.units.map(unitExt), zonesExt(b), enmityExt(b), b.units.length, null, { ...b.affinityScores }, patch]);
        }
      }
    }
  }
}

// Opening each lock ("Arrombar"): a hero with one Gazua stands on each free hex beside it (two heroes, three hexes), seeded; recorded: every unit
// afterwards, the bags (potions and Gazuas), what the fight now holds (Ember, rations, weapons, gear), the affinity scores and the draws.
const lockCasts: unknown[] = [];
if (process.env.SPELL_REFS === '1') {
  let lockSeed = 700000;
  const potionIds = Object.keys(data.POTIONS);
  const bagOf = (v: any) => Object.fromEntries(['lockpick', ...potionIds].map((k) => [k, v.bag?.[k] ?? 0]));
  for (let li = 0; li < lockList.length; li++) {
    const lk = lockList[li];
    const heroes = freshEngine().units.map((v: any, i: number) => [v, i]).filter(([v]: any) => v.side === 'player' && v.alive && (v.size ?? 1) === 1).slice(0, 2);
    for (const [, ci] of heroes as any[]) {
      const spotsAll = standSpots(freshEngine(), lk).slice(0, 3);
      for (const at of spotsAll) {
        const b = freshEngine();
        const u = b.units[ci];
        u.x = at.x, u.y = at.y, u.drawX = at.x, u.drawY = at.y;
        u.bag.lockpick = 1;
        b.selectedId = u.id, b.mode = 'selected', b.phase = 'player';
        const turnOrder = b.turnOrder.map((id: string) => b.units.findIndex((v: any) => v.id === id));
        const s = lockSeed++;
        const gen = combat.mulberry32(s);
        let calls = 0;
        b.rng = () => { calls++; return gen(); };
        for (const name of FX) {
          const original = b[name];
          if (typeof original !== 'function') continue;
          b[name] = (...args: unknown[]) => { const keep = b.rng; b.rng = Math.random; try { return original.apply(b, args); } finally { b.rng = keep; } };
        }
        b.useLockpick();
        const valid = u.bag.lockpick === 0;
        if (!valid) { lockSeed--; lockCasts.push({ lock: li, caster: ci, at: [at.x, at.y], valid: false }); continue; }
        lockCasts.push({ lock: li, caster: ci, at: [at.x, at.y], valid: true, seed: s, calls, turnOrder, ext: b.units.map(unitExt), bags: b.units.map(bagOf),
          loot: { ember: b.lootEmber, rations: b.lootRations, weapons: [...b.lootWeapons], equipment: [...b.lootEquipment] }, affinity: { ...b.affinityScores } });
      }
    }
  }
}

// The start of a unit's own turn (beginUnitTurn): each unit under each condition, the board read right after (SPELL_REFS=1):
// stunned, asleep, standing in a web, in fear, poisoned, in an ice storm, carrying a lightning echo, several at once.
const TURN_SCENARIOS = ['stunned', 'asleep', 'web', 'fear', 'poison', 'ice', 'echo', 'mixed', 'regen', 'regenNear'];
const turnStarts: unknown[] = [];
if (process.env.SPELL_REFS === '1') {
  const fresh = () => new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
  const base = fresh();
  const pristine = structuredClone(base.units);
  let turnSeed = 400000;
  for (let ci = 0; ci < base.units.length; ci++) {
    if (!base.units[ci].alive || base.units[ci].side === 'neutral') continue;
    for (const scenario of TURN_SCENARIOS) {
      const b = fresh();
      b.units = structuredClone(pristine);
      const u = b.units[ci];
      const foe = b.units.filter((v: any) => v.alive && v.side !== u.side && v.side !== 'neutral').sort((p: any, q: any) => pathfinding.hexDist(u, p) - pathfinding.hexDist(u, q))[0];
      const firstPlayer = b.units.find((v: any) => v.side === 'player');
      const around = [{ x: u.x, y: u.y }, ...pathfinding.hexNeighbors(u.x, u.y)];
      const web = () => b.webZones.push({ cells: new Set(around.map((p: any) => pathfinding.key(p.x, p.y))), roundsLeft: 3, createdAt: 0, center: { x: u.x, y: u.y }, radius: 1, sleepChance: 0.6 });
      const ice = () => b.iceStormZones.push({ cells: new Set(around.map((p: any) => pathfinding.key(p.x, p.y))), roundsLeft: 2, createdAt: 0, center: { x: u.x, y: u.y }, radius: 1,
        damageDice: 2, damageFaces: 6, damageMul: 0.8, casterMag: 30, casterLevel: 12, casterId: firstPlayer.id, side: 'player' });
      if (scenario === 'stunned' || scenario === 'mixed') u.stunned = true, u.stunTurns = 2;
      if (scenario === 'asleep') u.asleep = true, u.sleepTurns = 2;
      if (scenario === 'web') web();
      if (scenario === 'fear') u.fearTurns = 2, u.fearSourceId = foe?.id;
      if (scenario === 'poison' || scenario === 'mixed') u.poisoned = true, u.poisonTier = 'lethal', u.poisonMag = 40;
      if (scenario === 'ice' || scenario === 'mixed') ice();
      if (scenario === 'echo') u.shock = { dice: 1, faces: 12, bonus: 2, mag: 30 };
      // a staff with regeneration 2 in hand, hurt by 5 (or by 1: the heal is capped to the missing HP)
      if (scenario === 'regen' || scenario === 'regenNear') { u.weaponId = 'cajado-da-renovacao'; b.staffMagic = () => ({ regeneration: 2 }); u.hp = Math.max(1, u.maxHp - (scenario === 'regen' ? 5 : 1)); }
      const s = turnSeed++;
      const gen = combat.mulberry32(s);
      let calls = 0;
      b.rng = () => { calls++; return gen(); };
      for (const name of FX) {
        const original = b[name];
        if (typeof original !== 'function') continue;
        b[name] = (...args: unknown[]) => { const keep = b.rng; b.rng = Math.random; try { return original.apply(b, args); } finally { b.rng = keep; } };
      }
      b.runAiFor = () => {};  // the turn's own rules, not what the AI then does
      b.queue = [];
      u.moveBudgetUsed = 0;
      b.beginUnitTurn(u);
      const move = b.queue.find((q: any) => q.type === 'move');
      turnStarts.push([ci, scenario, s, calls, b.units.map(unitExt), zonesExt(b), enmityExt(b), [u.moved ? 1 : 0, u.acted ? 1 : 0, b.turnRestrained ? 1 : 0], move ? move.path.map((p: any) => [p.x, p.y]) : []]);
    }
  }
}

// The support casts (the cures, Curar Doença, Criar Comida e Água) and the potions, in a hurt scene so they have work to
// do: every living unit at half HP, the party's even-indexed units diseased (maybeInflictDisease's own change), every
// third unit poisoned, everyone hungry, every tier's uses halved (EmberEngine Spells hurtScene does the same). Recorded as
// the casts above, plus each unit's stats, conditions, hunger, uses and potions afterwards and the rations conjured.
const SUPPORT = ['cureMinor', 'cureWounds', 'cureLight', 'cureDisease', 'createFoodAndWater'];
const supportCasts: unknown[] = [];
if (process.env.SPELL_REFS === '1') {
  const fresh = () => new BattleEngine({ ...mission, fog: false }, art, { hp: {}, levels, enemyLevels, neutralLevels, ...gear }, 1, testLevel > 0) as unknown as Record<string, any>;
  const base = fresh();
  const pristine = structuredClone(base.units);
  const hurtScene = (list: any[]) => list.forEach((v, i) => {
    if (v.alive) v.hp = Math.max(1, Math.floor(v.maxHp / 2));
    if (i % 2 === 0 && v.side === 'player') {
      v.diseased = true;
      v.diseaseBase = { atk: v.atk, mag: v.mag, def: v.def, dex: v.dex, mov: v.mov };
      const pen = (n: number) => Math.round(n * (1 - data.DISEASE.statPenalty));
      v.atk = pen(v.atk), v.mag = pen(v.mag), v.def = pen(v.def), v.dex = pen(v.dex), v.mov = Math.max(1, pen(v.mov));
    }
    if (i % 3 === 0) { v.poisoned = true; v.poisonTier = 'lesser'; }
    v.fullness = 0;
    for (const k of Object.keys(v.spells ?? {})) v.spells[k] = Math.floor(v.spells[k] / 2);
  });
  let castSeed = 50000;
  // One run: `act` casts or uses on battle `b` (its random numbers counted); played out to the action's end.
  const run = (ci: number, at: { x: number; y: number }, setup: (b: any, u: any) => void, act: (b: any, u: any) => void) => {
    const b = fresh();
    b.units = structuredClone(pristine);
    hurtScene(b.units);
    const u = b.units[ci];
    u.x = at.x, u.y = at.y, u.drawX = at.x, u.drawY = at.y;
    b.selectedId = u.id;
    setup(b, u);
    const s = castSeed++;
    const gen = combat.mulberry32(s);
    let calls = 0;
    b.rng = () => { calls++; return gen(); };
    for (const name of FX) {
      const original = b[name];
      if (typeof original !== 'function') continue;
      b[name] = (...args: unknown[]) => { const keep = b.rng; b.rng = Math.random; try { return original.apply(b, args); } finally { b.rng = keep; } };
    }
    let done = false, doneCalls = 0;
    const finish = b.finishCombat.bind(b);
    b.finishCombat = (att: unknown) => { finish(att); if (!b.queue.length && !done) { done = true; doneCalls = calls; } };
    // the action's end (finishAction) belongs to the turn loop, not to the rules checked here
    b.finishAction = () => { if (!done) { done = true; doneCalls = calls; } };
    b.queue = [];
    b.beginUnitTurn = () => {}, b.startNewRound = () => {};  // the rules of the action alone: no later turn begins before the board is read
    const spellsBefore = JSON.stringify(u.spells), bagBefore = JSON.stringify(u.bag);
    act(b, u);
    for (let i = 0; i < 4000 && !done && (b.active || b.queue.length); i++) b.tick(0.05);
    if (done) calls = doneCalls;
    const acted = JSON.stringify(u.spells) !== spellsBefore || JSON.stringify(u.bag) !== bagBefore;
    const after = b.units.map((v: any) => [v.hp, v.alive ? 1 : 0, v.shock ? [v.shock.dice, v.shock.faces, v.shock.bonus, v.shock.mag] : 0, v.maxHp, v.level, v.xp ?? 0,
      v.atk, v.mag, v.def, v.dex, v.mov, v.diseased ? 1 : 0, v.poisoned ? 1 : 0, typeof v.fullness === 'number' ? v.fullness : null, v.spells ?? {}, v.bag ?? {}]);
    return { acted, s, calls, after, u, b };
  };
  for (let ci = 0; ci < base.units.length; ci++) {
    const me = base.units[ci];
    if (me.side !== 'player' || !me.alive) continue;
    const spots: { x: number; y: number }[] = [{ x: me.x, y: me.y }];
    if ((me.size ?? 1) === 1) {
      // beside the nearest ally (so cures and potions reach someone else)
      const occ = pathfinding.occupancy(base.units);
      let ally: any = null;
      for (const v of base.units) if (v !== me && v.alive && v.side === 'player' && (!ally || pathfinding.hexDist(v, me) < pathfinding.hexDist(ally, me))) ally = v;
      let spot: { x: number; y: number } | null = null;
      for (let y = 0; ally && y < base.rows && !spot; y++)
        for (let x = 0; x < base.cols && !spot; x++)
          if (pathfinding.hexDist({ x, y }, ally) === 1 && hexprops.hexDef(base.tiles, base.cols, x, y, base.decorOverlay).passable && !occ.has(pathfinding.key(x, y))) spot = { x, y };
      if (spot) spots.push(spot);
    }
    const kinds = new Set<string>(available.classSpells(me.classId, me.level ?? 1, me.name));
    for (const kind of SUPPORT) {
      if (!kinds.has(kind)) continue;
      for (const at of spots) {
        if (kind === 'createFoodAndWater') {
          const r = run(ci, at, () => {}, (b) => b.startCreateFoodAndWater());
          if (!r.acted) { castSeed--; supportCasts.push([ci, kind, at.x, at.y, 0, at.x, at.y]); continue; }
          supportCasts.push([ci, kind, at.x, at.y, 1, at.x, at.y, r.s, r.calls, r.after, r.u.spells, r.u.shockCharges ?? 0, r.u.frostCharges ?? 0, r.b.lootRations ?? 0, null]);
          continue;
        }
        for (let y = 0; y < base.rows; y++)
          for (let x = 0; x < base.cols; x++) {
            if (pathfinding.hexDist({ x, y }, at) > 4) continue;
            let aimValid = false;
            const r = run(ci, at, (b) => { b.spellKind = kind; b.mode = 'awaitSpell'; b.missileTargets = []; }, (b, u) => {
              aimValid = b.spellAimValid(u, { x, y });
              if (!aimValid) return;
              if (kind === 'cureDisease') b.castCureDisease(u, { x, y });
              else b.castHeal(u, { x, y }, kind);
            });
            if (!aimValid || !r.acted) { castSeed--; supportCasts.push([ci, kind, x, y, 0, at.x, at.y]); continue; }
            supportCasts.push([ci, kind, x, y, 1, at.x, at.y, r.s, r.calls, r.after, r.u.spells, r.u.shockCharges ?? 0, r.u.frostCharges ?? 0, r.b.lootRations ?? 0, null]);
          }
      }
    }
    // Every potion, on the actor's own hex and every hex around it.
    for (const kind of Object.keys(data.POTIONS)) {
      for (const at of spots) {
        const cells = [at, ...pathfinding.hexNeighbors(at.x, at.y).filter((p: { x: number; y: number }) => pathfinding.inBounds(p.x, p.y, base.cols, base.rows))];
        for (const c of cells) {
          const r = run(ci, at, (_b, u) => { u.bag = { ...data.EMPTY_BAG, ...(u.bag ?? {}), [kind]: 1 }; }, (b, u) => { b.potionAim = kind; b.confirmPotionAt(u, c); });
          if (!r.acted) { castSeed--; supportCasts.push([ci, 'potion', c.x, c.y, 0, at.x, at.y, 0, 0, [], {}, 0, 0, 0, kind]); continue; }
          supportCasts.push([ci, 'potion', c.x, c.y, 1, at.x, at.y, r.s, r.calls, r.after, r.u.spells, r.u.shockCharges ?? 0, r.u.frostCharges ?? 0, r.b.lootRations ?? 0, kind]);
        }
      }
    }
  }
  // Bites and zombie blows (maybeInflictDisease): each such enemy placed beside each hero of the hurt party, striking it
  // with a plain attack (kind "strike", the cell = the hero's hex).
  const SICKENING = ['wardog', 'wardog2', 'zombie', 'zombie2', 'undeadOx', 'plagueBearingCattle'];
  const occ = pathfinding.occupancy(base.units);
  for (let ci = 0; ci < base.units.length; ci++) {
    const me = base.units[ci];
    if (me.side !== 'enemy' || !me.alive || !SICKENING.includes(me.classId)) continue;
    for (let hi = 0; hi < base.units.length; hi++) {
      const h = base.units[hi];
      if (h.side !== 'player' || !h.alive) continue;
      const spot = pathfinding.hexNeighbors(h.x, h.y).find((p: { x: number; y: number }) => pathfinding.inBounds(p.x, p.y, base.cols, base.rows) &&
        hexprops.hexDef(base.tiles, base.cols, p.x, p.y, base.decorOverlay).passable && !occ.has(pathfinding.key(p.x, p.y)));
      if (!spot) continue;
      const r = run(ci, spot, () => {}, (b, u) => { b.queue.push({ type: 'combat', att: u.id, def: b.units[hi].id }); });
      supportCasts.push([ci, 'strike', h.x, h.y, 1, spot.x, spot.y, r.s, r.calls, r.after, r.u.spells, r.u.shockCharges ?? 0, r.u.frostCharges ?? 0, r.b.lootRations ?? 0, null]);
    }
  }
}

mkdirSync(dirname(outPath), { recursive: true });
// --- sound -------------------------------------------------------------------------------------------------------------
// engine.ts's constants (module-private), read from its source.
const engineSource = readFileSync(`${E2}/src/game/engine.ts`, 'utf8');
const constant = (name: string) => Number(new RegExp(`const ${name} = ([0-9.]+);`).exec(engineSource)?.[1]);
const LONG_ANIM_SECONDS = constant('LONG_ANIM_SECONDS'), KAEL_BLADE_SOUND_START = constant('KAEL_BLADE_SOUND_START');
const LONG_ARROW_RELEASE_SECONDS = LONG_ANIM_SECONDS;  // engine.ts: const LONG_ARROW_RELEASE_SECONDS = LONG_ANIM_SECONDS
const sfx = audio.sfxPlay as Record<string, (...a: unknown[]) => unknown>;
const round = (v: number) => Math.round(v * 1000) / 1000;
// What `fn` plays, through audio.ts (setTimeout runs its callback at once, its delay noted).
function cues(fn: () => void) {
  played.length = 0;
  const timer = g.setTimeout;
  g.setTimeout = ((cb: () => void, ms?: number) => { const before = playDelay; playDelay = (ms ?? 0) / 1000; cb(); playDelay = before; return 0; }) as unknown;
  try { fn(); } finally { g.setTimeout = timer; }
  return played.map((x) => ({ file: decodeURIComponent(x.src.split('/').pop() ?? ''), volume: round(x.volume), offset: round(x.offset),
    delay: round(x.delay), exclusive: x.exclusive }));
}
const crossbows = ['brigand', 'baseCrossbowman', 'brigandCrossbowman'];
const arrowSpells = ['longShot', 'bloodyShot', 'multiShot', 'piercing'];
const meleeSkills = ['cleave', 'sweep', 'shoulderSmash', 'stampede', 'piercingThrust', 'trip', 'doubleStrike'];
const monsterKind = (u: Record<string, any>, kind: string) => !!audio.hasMonsterSfx(u.sprite, kind);
function soundsFor(u: Record<string, any>) {
  const arrow = crossbows.includes(u.classId) || e.isArrowAttack(u), arcane = !arrow && e.isArcaneCaster(u);
  // startSeq, a plain combat step (no custom dice, no skill): its cue as the attack begins.
  const attackStart = cues(() => {
    if (arrow) sfx.arrowAttack(u.sprite === 'neera', LONG_ARROW_RELEASE_SECONDS);
    else if (e.isArcaneCaster(u)) { if (u.sprite === 'cultist-v2') sfx.cultistV2Attack(); else if (!sfx.monster(u.sprite, 'attack')) sfx.magicAttack(); }
    else if (u.sprite !== 'kaelFinal') {
      if (u.sprite === 'minor-horror-001') sfx.minorHorrorAttack();
      else if (u.sprite === 'carnivorous-plant-001') sfx.carnivorousPlantAttack();
      else if (u.sprite === 'sapling-001') sfx.saplingAttack();
      else if (u.sprite === 'plague-bearing-cattle') sfx.plagueCattleAttack();
      else if (e.monsterAttackCue(u)) { /* its own attack cue */ }
      else e.playMeleeCue(u, false, undefined);
    }
  });
  // stepCombat's lunge end: Kael's long swing is cued at the strike, KAEL_BLADE_SOUND_START into the recording.
  const strike = u.sprite === 'kaelFinal' && !arrow && !arcane ? cues(() => e.playMeleeCue(u, false, undefined, KAEL_BLADE_SOUND_START)) : [];
  // The counter (stepCombat, as the counter begins). A bow counter winds up like the unit's own shot.
  const counterStart = cues(() => {
    if (e.isArrowAttack(u)) sfx.arrowAttack(u.sprite === 'neera', LONG_ARROW_RELEASE_SECONDS);
    else if (e.isArcaneCaster(u)) { if (u.sprite === 'cultist-v2') sfx.cultistV2Attack(); else if (!sfx.monster(u.sprite, 'attack')) sfx.magicAttack(); }
    else if (u.sprite === 'minor-horror-001') sfx.minorHorrorAttack();
    else if (u.sprite === 'carnivorous-plant-001') sfx.carnivorousPlantAttack();
    else if (u.sprite === 'sapling-001') sfx.saplingAttack();
    else if (u.sprite === 'plague-bearing-cattle') sfx.plagueCattleAttack();
    else if (e.monsterAttackCue(u)) { /* its own attack cue */ }
    else if (u.sprite !== 'kaelFinal') e.playMeleeCue(u, false);
  });
  // startSeq, a spell step: every spell the unit may cast.
  const spells: Record<string, unknown> = {};
  for (const kind of new Set<string>([...available.classSpells(u.classId, u.level ?? 1, u.name), 'shock', 'frost', 'fantomForce'])) {
    spells[kind] = cues(() => {
      if (arrowSpells.includes(kind)) sfx.arrowAttack(u.sprite === 'neera', LONG_ANIM_SECONDS);
      else if (kind === 'tendrilSwipe') sfx.carnivorousPlantAttack();
      else if (meleeSkills.includes(kind)) e.playMeleeCue(u, false, kind);
      else if (kind === 'bless') sfx.healBy(u.sprite);
      else {
        const support = gameResist.SPELL_CLASSIFICATION[kind] === 'utility';
        if (support && monsterKind(u, 'heal')) sfx.healBy(u.sprite);
        else if (u.sprite === 'minor-horror-001') sfx.minorHorrorCast();
        else if (u.sprite === 'carnivorous-plant-001') sfx.carnivorousPlantCast();
        else if (u.sprite === 'sapling-001') sfx.saplingCast();
        else if (u.sprite === 'plague-bearing-cattle') sfx.plagueCattleCast();
        else if (sfx.monster(u.sprite, 'cast')) { /* its own cast cue */ }
        else if (u.sprite === 'cultist-v2') sfx.cultistV2Spellcast();
        else sfx.spell();
      }
    });
  }
  // emitMissileFx: Tiro Longo's arrow leaving the bow.
  const release = cues(() => sfx.arrowRelease(u.sprite === 'neera'));
  // startSeq, a heal / cure step.
  const heal = cues(() => sfx.healBy(u.sprite));
  // startSeq, a move: the walk cue (Cultist V2 by the first step's direction); faded out when the move ends.
  const walkOf = (left: boolean) => cues(() => {
    if (u.sprite === 'minor-horror-001') sfx.minorHorrorWalk();
    else if (u.sprite === 'sapling-001') sfx.saplingWalk();
    else if (u.sprite === 'plague-bearing-cattle') sfx.plagueCattleWalk();
    else if (sfx.monster(u.sprite, 'walk')) { /* its own walk cue */ }
    else if (u.sprite === 'cultist-v2') { if (left) sfx.cultistV2WalkLeft(); else sfx.cultistV2WalkRight(); }
    else sfx.move();
  });
  const walkFade = u.sprite === 'plague-bearing-cattle' || monsterKind(u, 'walk');
  // The hit and death cues (the engine's per-frame watch): plants, the cattle, MONSTER_SFX.
  const plant = u.sprite === 'carnivorous-plant-001' || u.sprite === 'sapling-001';
  const hit = cues(() => {
    if (plant) { if (u.sprite === 'sapling-001') sfx.saplingHit(); else sfx.carnivorousPlantHit(); }
    if (monsterKind(u, 'hit')) sfx.monster(u.sprite, 'hit');
  });
  const death = cues(() => {
    if (plant) { if (u.sprite === 'sapling-001') sfx.saplingDeath(); else sfx.carnivorousPlantDeath(); }
    if (u.sprite === 'plague-bearing-cattle') sfx.plagueCattleDeath();
    if (monsterKind(u, 'death')) sfx.monster(u.sprite, 'death');
  });
  // The Big Blue Calf skips its hit cue when the blow kills it.
  return { attack: attackStart, strike, counter: counterStart, spells, release, heal, walk: walkOf(false), walkLeft: walkOf(true), walkFade, hit,
    hitOnDeath: u.classId !== 'bigBlueCalf', death };
}
for (let i = 0; i < e.units.length; i++) (units[i] as Record<string, unknown>).sounds = soundsFor(e.units[i]);
const levelUp = cues(() => sfx.levelUp());
// GameApp's music chain for a battle (its mission id is the map's draft id): the mission's own track when it names one
// that exists, else the theme for its id (audio.ts getTrack: file and base volume).
const THEMES: Record<string, [string, number]> = {
  temple: ['temple.mp3', 0.42], aldeia: ['EMBERBattle01-Balanced-High.mp3', 0.4], siege: ['Absolute Mental Descruction 00-Balanced-High.mp3', 0.4],
  inn: ['Connard The Barbarian 10.mp3', 0.4], hill: ['Long Space _Opera DJ  UMN+  022-Balanced-High.mp3', 0.4],
  portao: ['TempleEntrance-Balanced-High.mp3', 0.4], early: ['LANDR-Absolute Mental Descruction umn3 10 (3)-Balanced-High.mp3', 0.4],
};
const missionMusic = (() => {
  const named = mission.music;
  if (named && (data.MUSIC_TRACKS as string[]).includes(named)) return { file: named, base: 0.4 };
  const id = draft.id as string;
  const theme = id === 'templo' ? 'temple' : id === 'aldeia' ? 'aldeia' : ['vau', 'bosque', 'cripta', 'vertente'].includes(id) ? 'early'
    : id === 'estalagem' ? 'inn' : ['muralha', 'fortaleza'].includes(id) ? 'siege' : ['colina', 'passagem'].includes(id) ? 'hill'
    : id === 'portao' ? 'portao' : 'early';
  return { file: THEMES[theme][0], base: THEMES[theme][1], theme };
})();

writeFileSync(outPath, JSON.stringify({ source: 'Engine2 src/game/engine.ts BattleEngine via tools/scripts/engine2/export_battle.ts',
  map: draft.id, cols: e.cols, rows: e.rows, debugFreeCast: testLevel > 0, explore: !!mission.explore, cells, units, affinityBonus, win: mission.win ?? 'rout', exits, affinityScores: e.affinityScores ?? {}, forecasts, rolls, ai, aiNear: near, aiWarp, locks, ownedWeapons: [...e.ownedWeapons], lockCasts, casts, supportCasts, skillCasts, turnStarts,
  sounds: { levelUp }, music: missionMusic }));
console.log(`${draft.id}: ${e.cols}x${e.rows}, ${units.length} units (${units.map((u) => `${u.name} mov ${u.mov}`).join(', ')})`);
process.exit(0);  // the engine leaves timers running
