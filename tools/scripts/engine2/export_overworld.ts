// Engine2's world map (the RPG overworld) for EmberEngine, read-only from its own modules: the hex board
// (src/campaign/overworld.ts: which cells exist, travel hours, encounter regions), the map's locations
// (mapstore ALL_LOCATIONS), mission gating (progression.json, quests.ts, access.ts), and every table one travel
// step reads (class HP and resistances per level, poison tiers, potions, chest loot, the loot pool with each
// item's power level, gear resistances, bag capacity, hero recruitment). Reference answers are written alongside:
// location / mission status, which steps are allowed, and stepOverworld() itself with seeded random numbers, so
// EmberEngine's port is checked against Engine2 (Cooker check-overworld).
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_overworld.ts -- <out.overworld.json> <out.references.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath, refPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath || !refPath) throw new Error('usage: export_overworld.ts -- <out.overworld.json> <out.references.json>');

const save = await import(`${E2}/src/campaign/save.ts`);
const mapstore = await import(`${E2}/src/campaign/mapstore.ts`);
const ow = await import(`${E2}/src/campaign/overworld.ts`);
const data = await import(`${E2}/src/ember/data.ts`);
const access = await import(`${E2}/src/campaign/access.ts`);
const progression = await import(`${E2}/src/campaign/progression.ts`);
const quests = await import(`${E2}/src/campaign/quests.ts`);
const poison = await import(`${E2}/src/campaign/poison.ts`);
const skills = await import(`${E2}/src/campaign/skills.ts`);
const hunger = await import(`${E2}/src/campaign/hunger.ts`);
const combat = await import(`${E2}/src/game/combat.ts`);
const resist = await import(`${E2}/src/ember/resistances.ts`);
const progressionConfig = JSON.parse(readFileSync(`${E2}/src/campaign/progression.json`, 'utf8'));

const clone = <T>(v: T): T => JSON.parse(JSON.stringify(v));
const empty = save.emptySave(false);
empty.updatedAt = 0;

// --- the board -----------------------------------------------------------------------------------------------------
const cells: { col: number; row: number; hours: number; region: string; encounters: string[]; x: number; y: number }[] = [];
for (let row = -2; row <= 40; ++row)
  for (let col = -2; col <= 40; ++col)
    if (ow.isOverworldCell(col, row)) {
      const w = ow.hexToWorld(col, row);
      cells.push({ col, row, hours: ow.travelHoursForHex(col, row, mapstore.ALL_LOCATIONS), region: ow.travelRegionAt(col, row), encounters: ow.travelEncounterIds(col, row), x: w.x, y: w.y });
    }
const hexOf = (l: { x: number; y: number }) => ow.worldToHex(l.x, l.y);
const locations = (mapstore.ALL_LOCATIONS as Record<string, unknown>[]).map((l) => {
  const h = hexOf(l as { x: number; y: number });
  return { ...clone(l), col: h.x, row: h.y };
});
const inn = (data.WORLD_LOCATIONS as { id: string; x: number; y: number }[]).find((l) => l.id === 'estalagem')!;
const stoneBridge = (data.WORLD_LOCATIONS as { id: string; missionIds: string[] }[]).find((l) => l.id === 'stonebridge');
const wisp = (mapstore.ALL_LOCATIONS as { id: string; missionIds: string[] }[]).find((l) => l.id === 'wisp-forest');

// --- missions as the map needs them --------------------------------------------------------------------------------
const missionInfo = (mapstore.ALL_MISSIONS as Record<string, unknown>[]).map((m) => ({
  id: m.id, index: m.index, title: m.title, win: m.win ?? null, hub: !!m.hub,
  crossing: mapstore.isCrossingDungeon(m), gated: progression.isGatedMission(m.id as string),
  locationMissions: undefined,
}));

// --- tables --------------------------------------------------------------------------------------------------------
const heroes = data.ALL_HERO_NAMES as readonly string[];
const baseClass: Record<string, string> = { Kael: 'swordsman', Neera: 'archer', Voss: 'mage', Salazar: 'healer', Aldric: 'aldric', Malrec: 'conjurer' };
const classIds = new Set<string>(Object.values(baseClass));
for (const pair of Object.values(data.PROMOTIONS as Record<string, string[]>)) for (const c of pair ?? []) classIds.add(c);
const classes: Record<string, { hp: number[]; resistances: Record<string, number> }> = {};
for (const c of classIds) {
  if (!(data.CLASSES as Record<string, unknown>)[c]) continue;
  const hp: number[] = [];
  for (let level = 1; level <= data.MAX_LEVEL; ++level) hp.push(data.statsFor(c, level).hp);
  classes[c] = { hp, resistances: clone(data.statsFor(c, 1).resistances ?? {}) };
}
const joinIndex: Record<string, number | null> = {};
for (const h of heroes) {
  // heroRecruited's HERO_JOIN_INDEX, read back through heroRecruited itself on synthetic saves.
  let found: number | null = null;
  if (data.heroRecruited(h, [], [])) found = 0;
  else {
    const sorted = [...(mapstore.ALL_MISSIONS as { id: string; index: number }[])].sort((a, b) => a.index - b.index);
    for (const m of sorted) if (data.heroRecruited(h, [m.id], [])) { found = m.index + 1; break; }
  }
  joinIndex[h] = found;
}
const startingWeapons = new Set<string>(['cajado-da-galhada', 'punhal-curvo']);
for (const c of Object.keys(data.CLASSES)) { const id = data.starterWeaponFor(c); if (id) startingWeapons.add(id); }
const weapons = Object.values(data.WEAPONS as Record<string, { id: string; name: string; price: number }>).map((w) => ({
  id: w.id, name: w.name, price: w.price, power: data.gearPowerLevel(w.price), starting: startingWeapons.has(w.id),
}));
const equipment = Object.values(data.EQUIPMENT as Record<string, { id: string; name: string; price?: number; slot: string; resistances?: Record<string, number> }>).map((e) => ({
  id: e.id, name: e.name, price: e.price ?? 60, power: data.gearPowerLevel(e.price ?? 60), starting: startingWeapons.has(e.id), slot: e.slot,
  resistances: clone(e.resistances ?? {}), pouch: (data.POUCH_UPGRADE_BONUS as Record<string, number>)[e.id] ?? 0,
}));
const potions = Object.entries(data.POTION_LOOT_WEIGHT as Record<string, number>).map(([id, weight]) => ({
  id, weight, name: (data.POTIONS as Record<string, { name: string }>)[id]?.name ?? id, carryMax: (data.POTION_CARRY_MAX as Record<string, number>)[id] ?? 5,
}));

const tables = {
  maxLevel: data.MAX_LEVEL,
  heroes, baseClass, classes, joinIndex,
  resistanceElements: resist.RESISTANCE_ELEMENTS,
  poisonTiers: poison.POISON_TIERS,
  potions, potionOrder: Object.keys(data.POTIONS), emptyBag: data.EMPTY_BAG,
  chestLoot: data.CHEST_LOOT,
  weapons, equipment,
  partyBagPerHero: data.PARTY_BAG_PER_HERO, rationStackMax: data.RATION_STACK_MAX,
  dailyHungerCost: hunger.DAILY_HUNGER_COST, innFullness: hunger.INN_FULLNESS,
  skillIds: skills.SKILL_IDS, skillNames: Object.fromEntries(Object.entries(skills.SKILLS as Record<string, { name: string }>).map(([k, v]) => [k, v.name])),
  skillCap: skills.SKILL_CAP, skillGain: skills.SKILL_GAIN, travelTrainingHours: skills.TRAVEL_TRAINING_HOURS,
  progression: progressionConfig,
  quests: clone(quests.QUESTS),
};

// --- references ----------------------------------------------------------------------------------------------------
const ids = (mapstore.ALL_MISSIONS as { id: string }[]).map((m) => m.id);
const order = ids;
type Save = Record<string, any>;
const withSave = (patch: Save): Save => ({ ...clone(empty), mapMode: 'rpg', ...patch });
const stoneIds = stoneBridge?.missionIds ?? [];
const wispIds = wisp?.missionIds ?? [];
const innHex = ow.worldToHex(inn.x, inn.y);
const saves: Save[] = [
  withSave({ pendingMission: 'vau' }),
  withSave({ completed: ['vau'] }),
  withSave({ completed: ['vau', ...stoneIds] }),
  withSave({ completed: [...new Set(['vau', ...stoneIds, ...wispIds])], overworldPos: { col: innHex.x, row: innHex.y } }),
  withSave({ completed: [...new Set(['vau', ...stoneIds, ...wispIds, 'estalagem'])] }),
  withSave({ completed: ids.slice(0, 20) }),
  withSave({ completed: ids.slice(0, 40), questsActive: ['brue-birolho'], questsDone: ['mudo-medalhoes'], flags: ['recruited:Aldric'] }),
];
const accessFor = (s: Save) => (id: string) => progression.missionAccess(id, s, access.progressionExtras(s), false);
const locationRefs = saves.map((s) => {
  const acc = accessFor(s);
  return {
    save: s,
    chapter: progression.currentChapter(s, access.progressionExtras(s)),
    locations: (mapstore.ALL_LOCATIONS as Record<string, any>[]).map((l) => ({
      id: l.id,
      status: access.locationStatus(l, s.completed, false, mapstore.ALL_LOCATIONS, acc),
      visible: l.missionIds.some((id: string) => acc(id) !== 'hidden'),
      missions: mapstore.missionsForLocation(l).filter((m: { id: string }) => acc(m.id) !== 'hidden').map((m: { id: string }) => ({
        id: m.id, access: acc(m.id),
        status: access.missionStatus(m.id, s.completed, false, mapstore.ALL_LOCATIONS, mapstore.missionsForLocation(l).map((x: { id: string }) => x.id), acc),
      })),
    })),
  };
});
const stepRefs: unknown[] = [];
const canStepRefs = saves.map((s) => {
  const allowed: string[] = [];
  for (const c of cells)
    for (const n of ow.neighborsOf(c.col, c.row))
      if (ow.canStepOverworld(s, { x: c.col, y: c.row }, n, false)) allowed.push(`${c.col},${c.row}>${n.x},${n.y}`);
  return { save: s, allowed };
});
// stepOverworld from many places, with seeded random numbers; saves vary hunger, poison, disease, spell uses,
// travel training, alert streak, hour, rations, HP.
let seed = 12345;
const variants: Save[] = [];
for (const base of saves) {
  variants.push(base);
  variants.push({ ...clone(base), heroHunger: { Kael: 5, Neera: 30, Voss: 0, Salazar: 80 }, hungerStreak: 4, hungerHours: 100, gameHour: 22 });
  variants.push({ ...clone(base), heroPoisons: { Kael: 'deadly', Voss: 'lesser' }, heroPoisonMag: { Kael: 12 }, unitHp: { Kael: 9, Neera: 3, Voss: 0 }, gameHour: 23 });
  variants.push({ ...clone(base), spellUses: { Voss: { tier1: 3, tier2: 1 }, Salazar: { tier1: 4 } }, travelTraining: { Kael: 'swordWeapon', Neera: 'healing' }, travelTrainingHours: { Kael: 7 }, alertStreak: 2, gameHour: 18, rations: 3, ember: 5 });
  variants.push({ ...clone(base), heroDiseases: { Kael: true }, lastRoadEncounterId: null, roadEncountersSeen: [], gameHour: 3, ember: 40 });
}
for (const v of variants) {
  const from = v.overworldPos as { col: number; row: number };
  const starts = [from, ...cells.filter((_, i) => i % 23 === 0).map((c) => ({ col: c.col, row: c.row }))];
  for (const start of starts) {
    const s = { ...clone(v), overworldPos: start };
    for (const n of ow.neighborsOf(start.col, start.row)) {
      if (!ow.canStepOverworld(s, { x: start.col, y: start.row }, n, false)) continue;
      for (let k = 0; k < 3; ++k) {
        seed = (seed * 1103515245 + 12345) >>> 0;
        let calls = 0;
        const rng0 = combat.mulberry32(seed);
        const rng = () => { calls++; return rng0(); };
        const { save: after, event } = ow.stepOverworld(clone(s), n.x, n.y, mapstore.ALL_LOCATIONS, false, rng);
        stepRefs.push({ save: s, to: { col: n.x, row: n.y }, seed, calls, after, event });
      }
    }
  }
}

const out = {
  board: {
    startHex: { col: ow.OVERWORLD_START_HEX.x, row: ow.OVERWORLD_START_HEX.y },
    westEdgeCol: ow.OVERWORLD_WEST_EDGE_COL,
    ashenBlocked: { col: ow.ASHEN_FOREST_BLOCKED_HEX.x, row: ow.ASHEN_FOREST_BLOCKED_HEX.y },
    innHex: { col: innHex.x, row: innHex.y },
    wispHex: wisp ? (() => { const h = hexOf(wisp as unknown as { x: number; y: number }); return { col: h.x, row: h.y }; })() : null,
    stoneBridgeMissionIds: stoneIds,
    hexSize: 5,
    cells,
  },
  locations,
  missions: missionInfo,
  tables,
};
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out));
mkdirSync(dirname(refPath), { recursive: true });
writeFileSync(refPath, JSON.stringify({ locations: locationRefs, canStep: canStepRefs, steps: stepRefs }));
console.log(`overworld: ${cells.length} cells, ${locations.length} locations, ${Object.keys(classes).length} classes, ${stepRefs.length} reference steps -> ${outPath}`);
