// The imported cast: every one of Ember's unit classes. Class data (name, size, footprint, boss)
// comes straight from Ember's own rules snapshot. The first units (heroes, summons, newest
// monsters) were set up by hand from the files on disk (manifest.json, tools/build-manifest.mjs);
// every other class takes its poses from Ember's own sprite tables (src/ember/spriteTables.ts).

import { CLASSES } from '../ember/data';
import { ENCOUNTER_NPC_IDS } from '../ember/encounter-npcs';
import type { ClassId, SpriteId } from '../ember/types';
import * as T from '../ember/spriteTables';
import manifest from './manifest.json';

export type Pose =
  | 'idle' | 'idle2' | 'walk' | 'walkLeft'
  | 'attack' | 'attackLeft' | 'attack2' | 'attack2Left' | 'attackShort'
  | 'cast' | 'castLeft'
  // a support spell's healing sheet (heal-*.png, Ember's castsHeal); a unit without one casts
  | 'heal'
  | 'hit' | 'hit2' | 'death' | 'death2'
  // Ember's counter cut (counter-*.png); a unit without one counters with its attack
  | 'counter'
  // Ember's up/down walk cycles (move-up-/move-down-*.png): walking away from / towards the camera
  | 'walkUp' | 'walkDown'
  // Ember's walkDirs: one still image per direction, shown while moving (Birolhos, Carrasco, early Kael)
  | 'walkBack' | 'walkFront' | 'walkSide';

/** Frames are `${prefix}1.png`…`${prefix}${n}.png`; a `still` is the single file `${prefix}.png`. */
export interface PoseSrc {
  dir: string; prefix: string; n: number; still?: boolean;
  /** Keep the body on the hex centre in every frame: the drawing's body drifts sideways inside
   * its canvas (Neera's dagger swing slides ~0.2 hex), so each frame is shifted back by it. */
  centerBody?: boolean;
}
export type Footprint = readonly { dx: number; dy: number }[];

export interface UnitDef {
  key: string;
  name: string;
  classId: ClassId;
  sprite: string;
  group: 'hero' | 'summon' | 'monster' | 'npc';
  poses: Partial<Record<Pose, PoseSrc>>;
  size: number;
  footprint?: Footprint;
  boss: boolean;
  /** Ember: the familiar's art faces left as drawn, the opposite of every other sheet. */
  drawnFacingLeft?: boolean;
}

const PREFIX_POSE: Record<string, Pose> = {
  '': 'idle', 'idle-': 'idle', 'idle2-': 'idle2', 'move-': 'walk', 'move-left-': 'walkLeft',
  'atk-': 'attack', 'atk-left-': 'attackLeft', 'atk2-': 'attack2', 'atk2-left-': 'attack2Left', 'atk-short-': 'attackShort',
  'cast-': 'cast', 'cast-left-': 'castLeft', 'heal-': 'heal', 'hit-': 'hit', 'hit2-': 'hit2', 'death-': 'death', 'death2-': 'death2',
  'counter-': 'counter',
};
const MANIFEST = manifest as Record<string, Record<string, number>>;

function posesIn(dir: string, only?: Pose[], skip: Pose[] = []): Partial<Record<Pose, PoseSrc>> {
  const out: Partial<Record<Pose, PoseSrc>> = {};
  const found = MANIFEST[dir] ?? {};
  // an explicit idle-*.png set wins over the numbered frames (Neera V2)
  const prefixes = Object.keys(found).sort((a, b) => (a === 'idle-' ? -1 : b === 'idle-' ? 1 : 0));
  for (const prefix of prefixes) {
    const pose = PREFIX_POSE[prefix];
    if (!pose || out[pose] || skip.includes(pose) || (only && !only.includes(pose))) continue;
    out[pose] = { dir, prefix, n: found[prefix] };
  }
  return out;
}

function unit(key: string, name: string, classId: ClassId, group: UnitDef['group'], poses: Partial<Record<Pose, PoseSrc>>, extra: Partial<UnitDef> = {}): UnitDef {
  const cls = CLASSES[classId];
  return {
    key, name, classId, group, poses,
    sprite: extra.sprite ?? cls?.sprite ?? key,
    size: cls?.size ?? 1,
    footprint: cls?.footprintOffsets,
    boss: !!cls?.boss,
    ...extra,
  };
}

/** A sprite's poses exactly as Ember's loadSpritePools() loads them (assets.ts), from its tables.
 * Ember-only pools Engine2 has no pose for yet (counter, up/down walks, walks2, walkDirs) are left out. */
function emberPoses(id: SpriteId): Partial<Record<Pose, PoseSrc>> {
  const dir = T.spriteDirectory(id);
  const out: Partial<Record<Pose, PoseSrc>> = {};
  const set = (pose: Pose, prefix: string, n: number | undefined) => { if (n) out[pose] = { dir, prefix, n }; };
  // idle: the numbered frames, except where Ember loads a dedicated idle cut over them
  if (id === 'defaultWarrior') set('idle', 'stand-', 12);
  else if (id === 'kaelEarly') set('idle', 'stand-', 36);
  else if (id === 'neera') set('idle', 'idle-', 36);
  else set('idle', id === 'conjurer' ? 'talk-' : '', T.idleFrameCount(id));
  set('attack', 'atk-', T.ATTACK_FRAMES[id]?.n);
  set('attack2', 'atk2-', T.ATTACK2_FRAMES[id]?.n);
  if (id === 'neera') set('attack2Left', 'atk2-left-', T.ATTACK2_FRAMES[id]?.n);
  set('attackShort', 'atk-short-', T.ATTACK_SHORT_FRAMES[id]?.n);
  set('cast', id === 'conjurer' ? '' : 'cast-', T.CAST_FRAMES[id]?.n);
  if (T.CAST_DIR_LEFT.includes(id)) set('castLeft', 'cast-left-', T.CAST_FRAMES[id]?.n);
  set('hit', 'hit-', T.HIT_FRAMES[id]?.n);
  set('hit2', 'hit2-', T.HIT2_FRAMES[id]?.n);
  set('death', 'death-', T.DEATH_FRAMES[id]?.n);
  set('death2', 'death2-', T.DEATH2_FRAMES[id]?.n);
  const walk = T.WALK_FRAMES[id];
  set('counter', 'counter-', T.COUNTER_FRAMES[id]?.n);
  set('walk', id === 'cobalt-blue-deer' ? 'move-left-' : 'move-', walk?.n);
  set('walkUp', 'move-up-', T.WALK_UP_DOWN_FRAMES[id]?.up);
  set('walkDown', 'move-down-', T.WALK_UP_DOWN_FRAMES[id]?.down);
  // walkDirs, exactly the files Ember's loadSpritePools() names
  const still = (pose: Pose, folder: string, file: string) => { out[pose] = { dir: folder, prefix: file, n: 1, still: true }; };
  const dirs: Partial<Record<SpriteId, [string, string, string, string]>> = {
    kaelEarly: ['kael', 'walk-front', 'walk-back', 'walk-side'],
    birolho: ['birolho', '1', 'back', '1'],
    birolho2: ['birolho2', '1', 'back', '1'],
    punisher: ['punisher', 'front', 'back', 'front'],
  };
  const wd = dirs[id];
  if (wd) { still('walkFront', wd[0], wd[1]); still('walkBack', wd[0], wd[2]); still('walkSide', wd[0], wd[3]); }
  // authored left-facing cuts: only the sprites Ember plays them for; everyone else is mirrored
  if (T.DIR_LEFT.includes(id)) {
    set('walkLeft', 'move-left-', walk?.n ?? 6);
    set('attackLeft', 'atk-left-', T.ATTACK_FRAMES[id]?.n ?? 5);
  }
  if (id === 'neera') { set('attackLeft', 'atk-left-', T.ATTACK_FRAMES[id]?.n); set('walkLeft', 'move-left-', walk?.n); }
  if (walk && ['theButcher', 'cultist-v2', 'militia-v2', 'BasePikeman', 'BaseCrossbowman', 'BrigandCrossbowman', 'StagWolf', 'familiar2', 'familiar3'].includes(id)) set('walkLeft', 'move-left-', walk.n);
  if (id === 'cobalt-blue-deer') set('walkLeft', 'move-', walk?.n);
  if (id === 'morvenian-wolf') set('walkLeft', 'move-left-', 7);
  return out;
}

const NPC_CLASSES = new Set<string>([...ENCOUNTER_NPC_IDS, 'beberrao', 'breadLady', 'brue', 'crazyLady', 'mudinho', 'oldHealer', 'peasant1', 'shadyPatron', 'soupLady', 'villagerF1', 'woodsman', 'travelingMerchant']);

const HAND_SET: UnitDef[] = [
  // Heroes: Ember pins each to its own sprite (HERO_SPRITE_BY_NAME) whatever their class.
  unit('kael', 'Kael', 'swordsman', 'hero', posesIn('Kael_Final/kael-final-002'), { sprite: 'kaelFinal' }),
  unit('neera', 'Neera', 'archer', 'hero', (() => {
    const poses = { ...posesIn('neera/neera-v2-001'), ...posesIn('neera', ['cast']) };
    // her off-hand dagger sheet drifts sideways mid-swing: hold her body on her hex
    if (poses.attackShort) poses.attackShort = { ...poses.attackShort, centerBody: true };
    return poses;
  })(), { sprite: 'neera' }),
  // Voss V2 (voss-final/, tools/build-voss-v2.mjs): idle, ATT, counter, casting and a placeholder walk
  unit('voss', 'Voss', 'mage', 'hero', posesIn('voss-final'), { sprite: 'voss' }),
  // Salazar V2 (salazar-final/); the old salazar/ cut is the generic Healer's (healerRecruit)
  unit('salazar', 'Salazar', 'healer', 'hero', posesIn('salazar-final'), { sprite: 'salazar' }),
  unit('aldric', 'Aldric', 'aldric', 'hero', posesIn('aldric'), { sprite: 'aldric' }),
  // Malrec's sliced move-left frames are deliberately unused in Ember: his walk is mirrored instead.
  unit('malrec', 'Malrec', 'conjurer', 'hero', posesIn('malrec', undefined, ['walkLeft']), { sprite: 'malrec' }),
  // Malrec's summons
  unit('familiar', 'Familiar', 'familiar', 'summon', posesIn('familiar'), { drawnFacingLeft: true }),
  unit('familiar2', 'Familiar Maior', 'familiar2', 'summon', posesIn('familiar2')),
  unit('familiar3', 'Familiar Titã', 'familiar3', 'summon', posesIn('familiar3')),
  unit('familiar4', 'Familiar Radiante', 'familiar4', 'summon', posesIn('familiar4')),
  unit('zombieDog', 'Cão Zumbi', 'zombieDog', 'summon', posesIn('zombieDog')),
  // Monsters, newest work
  unit('miliciaV2', 'Milícia', 'miliciaV2', 'monster', posesIn('militia-v2')),
  unit('apparition', 'Apparition', 'apparition', 'monster', posesIn('apparition')),
  unit('sapling', 'Muda Carnívora', 'sapling', 'monster', posesIn('sapling-001')),
  unit('jacare', 'Jacaré', 'jacare', 'monster', posesIn('jacare')),
  unit('undeadOx', 'Boi Morto-vivo', 'undeadOx', 'monster', posesIn('undeadOx')),
  unit('carnivorousPlant', 'Planta Carnívora', 'carnivorousPlant', 'monster', posesIn('carnivorous-plant-001')),
];

// Every other Ember class, by class id. Classes whose sprite is a hero's own (kaelFinal, neera, voss,
// salazar, aldric) are that hero already and aren't listed twice.
const HERO_SPRITES = new Set(HAND_SET.filter(u => u.group === 'hero').map(u => u.sprite));
const FROM_EMBER: UnitDef[] = (Object.keys(CLASSES) as ClassId[])
  .filter(id => !HAND_SET.some(u => u.key === id) && !HERO_SPRITES.has(CLASSES[id].sprite))
  .map(id => unit(id, CLASSES[id].name, id, NPC_CLASSES.has(id) ? 'npc' : 'monster', emberPoses(CLASSES[id].sprite)));

export const UNITS: UnitDef[] = [...HAND_SET, ...FROM_EMBER];

/** The unit a map spawn stands as: a named hero is that hero, anyone else is their class. */
export function unitForSpawn(s: { name?: string; classId?: string }, side: 'player' | 'enemy' | 'neutral'): UnitDef | undefined {
  const hero = side === 'player' && s.name ? HERO_NAMES[s.name] : undefined;
  if (hero) return UNITS.find(u => u.key === hero);
  const sprite = CLASSES[s.classId as ClassId]?.sprite;
  return UNITS.find(u => u.key === s.classId) ?? UNITS.find(u => u.classId === s.classId) ?? UNITS.find(u => u.sprite === sprite);
}

export const HERO_NAMES: Record<string, string> = { Kael: 'kael', Neera: 'neera', Voss: 'voss', Salazar: 'salazar', Aldric: 'aldric', Malrec: 'malrec' };

export function frameUrl(src: PoseSrc, i: number): string {
  return src.still ? `/game/sprites/${src.dir}/${src.prefix}.png` : `/game/sprites/${src.dir}/${src.prefix}${i + 1}.png`;
}

export const POSE_LABEL: Record<Pose, string> = {
  idle: 'Parado', idle2: 'Parado 2', walk: 'Andar', walkLeft: 'Andar ←', attack: 'Atacar', attackLeft: 'Atacar ←',
  attack2: 'Especial', attack2Left: 'Especial ←', attackShort: 'Mão secundária', cast: 'Conjurar', castLeft: 'Conjurar ←', heal: 'Curar',
  hit: 'Dano', hit2: 'Dano 2', death: 'Morte', death2: 'Morte 2', counter: 'Contra-ataque',
  walkUp: 'Andar ↑', walkDown: 'Andar ↓', walkBack: 'Andar (costas)', walkFront: 'Andar (frente)', walkSide: 'Andar (lado)',
};
