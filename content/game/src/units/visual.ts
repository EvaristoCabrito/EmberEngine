// Ember's per-unit draw sizes (engine.ts computeUnitVisual), ported exactly for the imported cast.
// EXISTING UNIT SIZES ARE LOCKED: these numbers were hand-fixed in Ember and must not be retuned.
// A "cell" is one hex width (√3 · R), the same unit Ember sizes sprites in.

import { FOOTPRINT_TYPE_7, FOOTPRINT_TYPE_8 } from '../ember/data';
import { SQ3, R } from '../core/hex';
import type { Pose, UnitDef } from './catalog';

const CELL = SQ3 * R;

export interface Box { w: number; h: number; /** how far the feet sit above the card's bottom edge */ foot: number }

const ATTACKING: Pose[] = ['attack', 'attackLeft', 'attack2', 'attack2Left', 'attackShort', 'cast', 'castLeft', 'heal', 'counter'];

/** Ember's on-screen box for this unit in this pose (frame index matters for one Malrec frame). */
export function unitBox(u: UnitDef, pose: Pose, frame: number, natW: number, natH: number): Box {
  const sprite = u.sprite, cls = u.classId;
  const s = cls === 'undeadOx' ? 2 : Math.max(1, u.size || 1);
  const atk = ATTACKING.includes(pose);
  const casting = pose === 'cast' || pose === 'castLeft';
  const walk = pose === 'walk' || pose === 'walkLeft' || pose === 'walkUp' || pose === 'walkDown' || pose === 'walkBack' || pose === 'walkFront' || pose === 'walkSide';
  const hitOrDeath = pose === 'hit' || pose === 'hit2' || pose === 'death';

  // per-pixel rules: one shared canvas scale per sheet family, 1.53-cell human body height
  const neeraV2Px: Partial<Record<Pose, number>> = { idle: 800, attack: 672, attackLeft: 672, attack2: 755, attack2Left: 755, attackShort: 675, walk: 471, walkLeft: 471 };
  if (sprite === 'neera' && neeraV2Px[pose]) {
    const k = (CELL * 1.53) / neeraV2Px[pose]!;
    return { w: natW * k, h: natH * k, foot: 0 };
  }
  // Malrec: one fixed scale per sheet (body incl. staff, frame 1) so every pose is his idle's size,
  // feet on the idle's ground line (12 px above its canvas bottom) — engine.ts MALREC_SHEET_PX
  const malrecPx: Partial<Record<Pose, [number, number]>> = { idle: [296, 12], idle2: [296, 12], attack: [244, 38], cast: [312, 4], walk: [668, 7], walkLeft: [630, 11] };
  if (sprite === 'malrec' && malrecPx[pose]) {
    const [body, feet] = malrecPx[pose]!;
    const k = (CELL * 1.42 * 1.2 * 296 / 320) / body;
    return { w: natW * k, h: natH * k, foot: (feet - 12) * k };
  }
  // Salazar V2: one shared 416x613 canvas for every sheet, idle body 480 px, feet 8 px up
  if (sprite === 'salazar') { const k = (CELL * 1.53) / 480; return { w: natW * k, h: natH * k, foot: 8 * k }; }
  // Voss V2: one shared 526x823 canvas for every sheet, hood to soles 500 px, soles 152 px up
  if (sprite === 'voss') { const k = (CELL * 1.53) / 500; return { w: natW * k, h: natH * k, foot: 152 * k }; }
  if (sprite === 'militia-v2') { const k = (CELL * 1.53) / 628; return { w: natW * k, h: natH * k, foot: 61 * k }; }
  // BasePikeman: one shared 1000x800 canvas for every sheet, standing body 600 px, soles 20 px up
  if (sprite === 'BasePikeman') { const k = (CELL * 1.53) / 600; return { w: natW * k, h: natH * k, foot: 20 * k }; }
  // The crossbowmen: same rule (BaseCrossbowman's canvas is 1200x800 so his fall lies full length)
  if (sprite === 'BaseCrossbowman' || sprite === 'BrigandCrossbowman') { const k = (CELL * 1.53) / 600; return { w: natW * k, h: natH * k, foot: 20 * k }; }
  // StagWolf: 1200x800 canvas, back/skull to paws 500 px (antlers above), paws 20 px up
  if (sprite === 'StagWolf') { const k = (CELL * 1.45) / 500; return { w: natW * k, h: natH * k, foot: 20 * k }; }
  if (sprite === 'apparition') { const k = (CELL * 1.53) / 597; return { w: natW * k, h: natH * k, foot: 34 * k }; }
  if (sprite === 'jacare') { const k = (CELL * 3.4) / 581; return { w: natW * k, h: natH * k, foot: 13 * k }; }

  const big = u.footprint === FOOTPRINT_TYPE_8 || u.footprint === FOOTPRINT_TYPE_7;
  const isLancer = cls === 'lancer' || sprite === 'lancer' || sprite === 'defaultLancer';
  const isCultistV2 = cls === 'cultistV2' || sprite === 'cultist-v2';
  const spriteScale = isLancer ? 1.4 : cls === 'sandoval' || sprite === 'sandoval' ? 1.2 : cls === 'familiar' || sprite === 'familiar' ? 0.5
    : sprite === 'kaelFinal' ? 0.9 : isCultistV2 ? 0.98 : sprite === 'neera' ? 0.9 : sprite === 'soldier' ? 0.9 : 1;
  let hm = s >= 4 ? 3.35 : s === 2 ? 1.72 : u.boss ? 1.44 : 1.42;
  let wm = s >= 4 ? 2.85 : s === 2 ? 1.85 : u.boss ? 1.12 : 1.11;
  hm *= 1.2 * (big ? 0.75 : 1) * spriteScale;
  wm *= 1.2 * (big ? 0.75 : 1) * spriteScale;

  // Kael Final's padded ATT canvas
  if (sprite === 'kaelFinal' && atk) { hm *= 1.135; wm *= 1.135; }
  // Neera's top-level cast sheet
  if (sprite === 'neera' && casting) { hm *= 1.18; wm *= 1.18; }
  // summons
  if (sprite === 'familiar2') { wm *= 2.544; if (walk) { hm *= 0.97; wm *= 0.97; } }
  if (cls === 'familiar3') { hm *= 1.4; wm *= 1.4 * 1.9; }
  if (sprite === 'familiar4') { hm *= 0.97; wm *= 2.61; }
  if (sprite === 'zombieDog') { hm *= 1.051; wm *= 1.33; if (hitOrDeath) wm *= 528 / 437; }
  // monsters
  if (sprite === 'carnivorous-plant-001') { hm *= 1.03; wm *= 2.15; }
  if (sprite === 'sapling-001') { hm *= 0.582; wm *= 1.264; }
  if (cls === 'undeadOx') { const k = 0.75 * 1.1 * 1.15; hm *= k; wm *= k; }
  if (sprite === 'undeadOx' || sprite === 'plague-bearing-cattle') { hm *= 1.283; wm *= 1.889; }
  if (sprite === 'big-blue-ox-002') { hm *= 0.85; wm *= 1.49; }
  // the rest of Ember's per-sprite corrections (computeUnitVisual), for the units imported later
  const cultistCasting = isCultistV2 && casting;
  if (cultistCasting) { hm *= 1.24; wm *= 1.06; } else if (isCultistV2 && atk) { hm *= 1.13; wm *= 1.13; }
  if (isCultistV2 && walk) { hm *= 1.02; wm *= 1.02; }
  if (sprite === 'BirolhoLegs') { hm *= 1.32; wm *= 2.17; }
  if (sprite === 'BirolhoLegs2') { hm *= 1.29; wm *= 2.75; }
  if (sprite === 'troll2') { hm *= 1.03; wm *= 1.59; }
  if (sprite === 'wardog2') { hm *= 0.896; wm *= 1.372; }
  if (sprite === 'minor-horror-001') wm *= 1.163;
  if (sprite === 'EmberedWraith') wm *= 1.037;
  if (sprite === 'RoccoTheBird') { hm *= 1.22; wm *= 2.55; }
  if (sprite === 'mordavian-wolf-final') wm *= 1.395;
  if (sprite === 'zombie') wm *= 1.705;
  if (sprite === 'zombie2') wm *= 1.085;

  const h = CELL * hm, w = CELL * wm;
  const foot = cultistCasting ? h * 0.127 : sprite === 'kaelFinal' && atk ? h * 0.025 : sprite === 'neera' && casting ? h * 0.045 : sprite === 'familiar2' ? h * 0.028 : 0;
  return { w, h, foot };
}

/** Ember's animation pacing. Returns the frame index for a pose that has played for `t` seconds. */
/** Veo-cut walks loop at the footage's own length (each cut spans a different stretch of video). */
const FOOTAGE_WALK_SECONDS: Record<string, Partial<Record<Pose, number>>> = {
  BasePikeman: { walk: 1.75, walkLeft: 5, walkDown: 2.33 },
  BaseCrossbowman: { walk: 4.75, walkLeft: 4.75 },
  BrigandCrossbowman: { walk: 3.25, walkLeft: 3.25 },
  StagWolf: { walk: 4, walkLeft: 4 },
  voss: { walk: 3.07, walkLeft: 3.07 },
};

export function framePace(u: UnitDef, pose: Pose, n: number, t: number, cut: Pose = pose): { frame: number; done: boolean } {
  const footage = FOOTAGE_WALK_SECONDS[u.sprite]?.[cut];
  if (footage && (pose === 'walk' || pose === 'walkLeft')) return { frame: Math.floor((t / footage) * n) % n, done: false };
  const long = n >= 24;
  const slow = u.sprite === 'malrec' ? 0.9 : 1; // Conjurer/Malrec sheets play 10% slower
  if (pose === 'idle' || pose === 'idle2') {
    if (u.sprite === 'neera') return { frame: Math.floor(t / 0.098) % n, done: false };
    if (u.sprite === 'militia-v2') return { frame: Math.floor(t / 0.119) % n, done: false };
    if (u.classId === 'familiar' || u.classId === 'familiar2') return { frame: Math.floor(t * 5.5 * (u.classId === 'familiar2' ? n / 12 : 1)) % n, done: false };
    const cycle = Math.max(2, n * 2 - 2);
    const pace = (long ? n / 3 : cycle / 2.6) * slow;
    const x = Math.floor(t * pace) % cycle;
    return { frame: x < n ? x : cycle - x, done: false };
  }
  if (pose === 'walk' || pose === 'walkLeft') {
    // Salazar V2's walk loop plays at 1.9 s (engine.ts SALAZAR_WALK_LOOP_SECONDS)
    const fps = long ? n / (u.sprite === 'salazar' ? 1.9 : 1.5) : Math.min(n / 2, 6) * slow / 0.22;
    return { frame: Math.floor(t * fps) % n, done: false };
  }
  // one-shot sheets: long sheets last 3 s (Apparition's 60-frame ATT lasts 5 s); short ones ~0.8 s
  const seconds = u.sprite === 'apparition' && pose === 'attack' ? 5 : long ? 3 / slow : 0.8;
  const frame = Math.min(n - 1, Math.floor((t / seconds) * n));
  return { frame, done: t >= seconds };
}
