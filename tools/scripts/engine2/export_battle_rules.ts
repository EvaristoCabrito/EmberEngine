// The rule tables the game's own map preparation (engine/world/BattleExport.cpp) needs, written once as data:
// terrain, classes (stats, spell tiers, spell lists at every level, charges, starter weapons), enemy spell pools,
// decorations (footprints, solidity, doors, exits), weapons and gear, the sound cues and the music themes. Run it
// again whenever the rule source changes; the game itself never runs this (no node at run time).
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> --config content/game/vite.export.config.mjs \
//        tools/scripts/engine2/export_battle_rules.ts -- content/engine2/battle-rules.json

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

// The game's rule source snapshot (EMBER_GAME_DATA_ROOT, default content/game).
const E2 = resolve(process.env.EMBER_GAME_DATA_ROOT ?? 'content/game').replaceAll('\\', '/');
const g = globalThis as Record<string, unknown>;
g.window = globalThis;
g.matchMedia = () => ({ matches: false, addEventListener() {}, removeEventListener() {} });
g.requestAnimationFrame = () => 0;
g.cancelAnimationFrame = () => {};
g.localStorage = { getItem: () => null, setItem() {}, removeItem() {} };
g.Image = class { src = ''; width = 1; height = 1; onload: unknown = null; onerror: unknown = null; };
g.document = { createElement: () => ({ getContext: () => null, style: {} }), documentElement: { classList: { add() {}, remove() {} } }, querySelectorAll: () => [] };
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

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_battle_rules.ts -- <out.json>');

const data = await import(`${E2}/src/game/data.ts`);
const skills = await import(`${E2}/src/game/weaponSkills.ts`);
const available = await import(`${E2}/src/rules/available-spells.ts`);
const audio = await import(`${E2}/src/game/audio.ts`);
const resist = await import(`${E2}/src/game/resistances.ts`);
const quests = await import(`${E2}/src/game/quests.ts`);
const party = await import(`${E2}/src/game/campaignParty.ts`);
const frost = await import(`${E2}/src/game/frost.ts`);
const hunger = await import(`${E2}/src/game/hunger.ts`);
const watchtower = await import(`${E2}/src/game/watchtowerDungeon.ts`);
const affinity = await import(`${E2}/src/game/affinity.ts`);

const MAX_LEVEL: number = data.MAX_LEVEL;
const levels = Array.from({ length: MAX_LEVEL }, (_, i) => i + 1);

// engine.ts initiativeBonus (module-private there, copied as written)
const initiativeBonus = (classId: string): number => {
  if (classId === 'assassin') return 12;
  if (classId === 'rogue') return 11;
  if (classId === 'archer' || classId === 'neera' || classId === 'ranger') return 10;
  return 11 - (data.CLASSES[classId].init ?? 10);
};

const classes: Record<string, unknown> = {};
for (const [id, cls] of Object.entries(data.CLASSES as Record<string, any>)) {
  const stats = levels.map((lv) => { const s = data.statsFor(id, lv); return [s.hp, s.atk, s.mag, s.def, s.dex]; });
  const tiers = levels.map((lv) => Array.from({ length: 10 }, (_, t) => data.tierUses(id, t + 1, lv)));
  const spells = levels.map((lv) => available.classSpells(id, lv));
  const spellsSalazar = levels.map((lv) => available.classSpells(id, lv, 'Salazar'));
  const same = JSON.stringify(spells) === JSON.stringify(spellsSalazar);
  classes[id] = {
    name: cls.name, role: cls.role ?? '', sprite: cls.sprite, size: cls.size ?? 1,
    footprintW: cls.footprintW ?? null, footprintH: cls.footprintH ?? null, footprintOffsets: cls.footprintOffsets ?? null,
    resistances: cls.resistances ?? {}, minRange: cls.minRange, maxRange: cls.maxRange, mov: cls.mov,
    boss: !!data.isBossClass(id), undead: !!data.isUndeadClass(id), summon: !!data.isSummonClass(id),
    initiativeBonus: initiativeBonus(id), shockCharges: data.shockChargesFor(id), fantomForceCharges: data.fantomForceChargesFor(id),
    starterWeapon: data.starterWeaponFor(id), weaponTypes: skills.weaponTypesForClass(id),
    stats, tiers, spells, ...(same ? {} : { spellsSalazar }),
  };
}

const enemyPools = {
  cultist: levels.map((lv) => { const u = data.cultistSpellUses(lv); return [u.magicMissile, u.lightning]; }),
  brigand: levels.map((lv) => { const u = data.brigandSpellUses(lv); return [u.longShot, u.piercing]; }),
  birolho: levels.map((lv) => { const u = data.birolhoSpellUses(lv); return [u.magicMissile, u.causticVenom, u.lightning]; }),
  frost: levels.map((lv) => frost.frostCharges(lv)),
  fantomDice: levels.map((lv) => { const d = data.fantomForceDice(lv); return [d.dice, d.faces]; }),
};

const decorations: Record<string, unknown> = {};
for (const [id, def] of Object.entries(data.DECORATIONS as Record<string, any>))
  decorations[id] = { footprint: def.footprint, blockingFootprint: def.blockingFootprint ?? null, architectureSpan: def.architectureSpan ?? 0,
    model3d: def.model3d ?? null, doorStyle: def.doorStyle ?? null, tile: def.tile ?? null, exitKind: def.exitKind ?? null, rockStyle: def.rockStyle ?? null };
const set = (s: Set<string>) => [...s].sort();

const weapons: Record<string, unknown> = {};
for (const [id, w] of Object.entries(data.WEAPONS as Record<string, any>))
  weapons[id] = { weaponType: w.weaponType, usableBy: w.usableBy, dice: w.dice, faces: w.faces, bonus: w.bonus ?? 0, minRange: w.minRange, maxRange: w.maxRange,
    ranged: !!w.ranged, twoHanded: !!w.twoHanded, bonusClass: w.bonusClass ?? null, magic: w.magic ?? null };
const equipment: Record<string, unknown> = {};
for (const [id, q] of Object.entries(data.EQUIPMENT as Record<string, any>))
  equipment[id] = { kind: q.kind, slot: q.slot ?? null, weaponType: q.weaponType ?? null, usableBy: q.usableBy ?? [], minRange: q.minRange ?? 1, maxRange: q.maxRange ?? 1,
    dice: q.dice ?? 1, faces: q.faces ?? 4, bonus: q.bonus ?? 0, dmgMul: q.dmgMul ?? 0.75, hp: q.hp ?? 0, atk: q.atk ?? 0, mag: q.mag ?? 0, def: q.def ?? 0, dex: q.dex ?? 0, mov: q.mov ?? 0,
    resistances: q.resistances ?? {} };

// --- sound cues (audio.ts through the recording Audio element; setTimeout runs at once, its delay noted) ---
const round = (v: number) => Math.round(v * 1000) / 1000;
function cues(fn: () => void) {
  played.length = 0;
  const timer = g.setTimeout;
  g.setTimeout = ((cb: () => void, ms?: number) => { const before = playDelay; playDelay = (ms ?? 0) / 1000; cb(); playDelay = before; return 0; }) as unknown;
  try { fn(); } finally { g.setTimeout = timer; }
  return played.map((x) => ({ file: decodeURIComponent(x.src.split('/').pop() ?? ''), volume: round(x.volume), offset: round(x.offset), delay: round(x.delay), exclusive: x.exclusive }));
}
const engineSource = readFileSync(`${E2}/src/game/engine.ts`, 'utf8');
const constant = (name: string) => Number(new RegExp(`const ${name} = ([0-9.]+);`).exec(engineSource)?.[1]);
const LONG_ANIM_SECONDS = constant('LONG_ANIM_SECONDS'), KAEL_BLADE_SOUND_START = constant('KAEL_BLADE_SOUND_START');
const sfx = audio.sfxPlay as Record<string, (...a: unknown[]) => unknown>;
const simple = ['spell', 'daggerAttack', 'kaelBladeSkill', 'minorHorrorAttack', 'minorHorrorCast', 'minorHorrorWalk', 'carnivorousPlantAttack', 'carnivorousPlantCast',
  'carnivorousPlantHit', 'carnivorousPlantDeath', 'saplingAttack', 'saplingCast', 'saplingHit', 'saplingDeath', 'saplingWalk', 'plagueCattleAttack', 'plagueCattleCast',
  'plagueCattleDeath', 'plagueCattleWalk', 'cultistV2Attack', 'cultistV2Spellcast', 'cultistV2WalkLeft', 'cultistV2WalkRight', 'magicAttack', 'heal', 'move', 'levelUp'];
const cueTable: Record<string, unknown> = {};
for (const name of simple) cueTable[name] = cues(() => sfx[name]());
cueTable['meleeAttack:blunt'] = cues(() => sfx.meleeAttack(false, 0));
cueTable['meleeAttack:blade'] = cues(() => sfx.meleeAttack(true, 0));
cueTable['meleeAttack:blade:kael'] = cues(() => sfx.meleeAttack(true, KAEL_BLADE_SOUND_START));
cueTable['arrowAttack:other'] = cues(() => sfx.arrowAttack(false, 0));
cueTable['arrowAttack:other:long'] = cues(() => sfx.arrowAttack(false, LONG_ANIM_SECONDS));
cueTable['arrowAttack:neera'] = cues(() => sfx.arrowAttack(true, 0));
cueTable['arrowAttack:neera:long'] = cues(() => sfx.arrowAttack(true, LONG_ANIM_SECONDS));
cueTable['arrowRelease:other'] = cues(() => sfx.arrowRelease(false));
cueTable['arrowRelease:neera'] = cues(() => sfx.arrowRelease(true));
// Every sprite's own cues (audio.ts MONSTER_SFX, module-private: read through sfxPlay.monster for every sprite the classes name
// and every hero sprite; the source is scanned too so a key named nowhere else is not lost).
const kinds = ['attack', 'attack2', 'cast', 'heal', 'walk', 'hit', 'death', 'death2'];
const sprites = new Set<string>([...Object.values(data.CLASSES as Record<string, any>).map((c) => c.sprite), 'kaelFinal', 'kaelEarly', 'neera', 'voss', 'BossVoss', 'salazar', 'aldric', 'conjurer', 'malrec']);
const audioSource = readFileSync(`${E2}/src/game/audio.ts`, 'utf8');
const block = /const MONSTER_SFX[^=]*=\s*\{([\s\S]*?)\n\};/.exec(audioSource)?.[1] ?? '';
for (const m of block.matchAll(/^\s*"?([A-Za-z0-9_-]+)"?:\s*\{/gm)) sprites.add(m[1]);
const monster: Record<string, Record<string, unknown>> = {};
for (const sprite of [...sprites].sort())
  for (const kind of kinds)
    if (audio.hasMonsterSfx(sprite, kind)) (monster[sprite] ??= {})[kind] = cues(() => sfx.monster(sprite, kind));

// GameApp's battle music chain, as export_battle.ts wrote it
const THEMES: Record<string, [string, number]> = {
  temple: ['temple.mp3', 0.42], aldeia: ['EMBERBattle01-Balanced-High.mp3', 0.4], siege: ['Absolute Mental Descruction 00-Balanced-High.mp3', 0.4],
  inn: ['Connard The Barbarian 10.mp3', 0.4], hill: ['Long Space _Opera DJ  UMN+  022-Balanced-High.mp3', 0.4],
  portao: ['TempleEntrance-Balanced-High.mp3', 0.4], early: ['LANDR-Absolute Mental Descruction umn3 10 (3)-Balanced-High.mp3', 0.4],
  cemetery: ['Ember Ashes Ambient OST 0-Balanced-High.mp3', 0.4],
  watchtower: ['OldTowwerDrone  trax 4 Persona 884-Balanced-High.mp3', 0.4],
};

const out = {
  source: 'the game rules (content/game/src: data.ts, engine.ts spawnUnit, available-spells.ts, weaponSkills.ts, audio.ts) through tools/scripts/engine2/export_battle_rules.ts',
  maxLevel: MAX_LEVEL,
  terrain: data.TERRAIN,
  tileChar: Object.fromEntries(Object.entries(data.TILE_CHAR as Record<string, string>).map(([id, ch]) => [ch, id])),
  classes,
  enemyPools,
  notoriousLevelBonus: quests.NOTORIOUS_LEVEL_BONUS,
  startingBag: data.STARTING_BAG, emptyBag: data.EMPTY_BAG,
  mageRangeBonusClasses: ['mage', 'elementalist', 'warlock'],  // engine.ts MAGE_RANGE_BONUS_CLASSES (module-private)
  heroSprites: { Kael: 'kaelFinal', Neera: 'neera', Voss: 'voss', Salazar: 'salazar', Aldric: 'aldric', Malrec: 'malrec' },  // engine.ts HERO_SPRITE_BY_NAME
  campaignHeroClasses: party.CAMPAIGN_HERO_CLASSES,
  campaignHeroOrder: Object.keys(party.CAMPAIGN_HERO_CLASSES),  // the order the missing heroes are placed in
  enemyLevelFor: Array.from({ length: 40 }, (_, i) => data.enemyLevelFor(i)),
  legacyClassIds: { butcher: 'punisher' },  // mapstore.ts legacyClassId
  diseaseStatPenalty: data.DISEASE.statPenalty,
  innFullness: hunger.INN_FULLNESS,
  decorations,
  decorSets: { chest: set(data.CHEST_DECOR_IDS), house: set(data.HOUSE_DECOR_IDS), bigHouse: set(data.BIG_HOUSE_DECOR_IDS), solidHouse: set(data.SOLID_HOUSE_DECOR_IDS),
    solidCart: set(data.SOLID_CART_DECOR_IDS), solidRock: set(data.SOLID_ROCK_DECOR_IDS), lowBlocker: set(data.LOW_BLOCKER_DECOR_IDS), barricadeLike: set(data.BARRICADE_LIKE_DECOR) },
  doorVariants: data.THREE_D_DOOR_VARIANTS,
  weapons, equipment,
  weaponSkill: { accuracy: skills.weaponModifiers({ side: 'player', classId: 'swordsman', weaponSkills: { sword: 0 }, dex: 0 }, 'sword').accuracy,
    damage: skills.weaponModifiers({ side: 'player', classId: 'swordsman', weaponSkills: { sword: 0 }, dex: 0 }, 'sword').damage },
  spellClass: resist.SPELL_CLASSIFICATION,
  affinityBonus: [[90, affinity.affinityBonus(90)], [50, affinity.affinityBonus(50)], [25, affinity.affinityBonus(25)]],
  watchtowerFloors: watchtower.WATCHTOWER_FLOORS,
  sounds: { longAnimSeconds: LONG_ANIM_SECONDS, kaelBladeSoundStart: KAEL_BLADE_SOUND_START, cues: cueTable, monster },
  music: { tracks: data.MUSIC_TRACKS, themes: THEMES },
};
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out, null, 1));
console.log(`${Object.keys(classes).length} classes, ${Object.keys(decorations).length} decorations, ${Object.keys(weapons).length} weapons, ${Object.keys(equipment).length} gear, ${Object.keys(monster).length} sprites with own cues`);
process.exit(0);
