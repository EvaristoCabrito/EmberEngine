// One unit on the board: an upright card that always faces the camera, at full native resolution,
// lit by the world (sun, sky, torches, spell lights, shadows — see spriteMaterial.ts), with soft
// blended edges and a real shadow cast only by its solid pixels. Ember's sizes, timings and
// left-facing cuts.

import * as THREE from 'three';
import type { Pose, UnitDef } from './catalog';
import { loadPose, type PoseFrames } from './frames';
import { unitBox, framePace } from './visual';
import { playPoseSound, stopWalkSound } from './sounds';
import { makeSpriteMaterial, UNIT_GLOW } from './spriteMaterial';
import { UNIT_LIGHT_DEFS } from '../ember/lighting';
import { CarriedLight } from '../render/carriedLight';
import { alignUnitLights } from './lightFrame';
import { FLAT_ROW_STEP } from '../render/cameraRig';
import { UNIT_RENDER_ORDER } from '../render/drawOrder';

const LEFT_CUT: Partial<Record<Pose, Pose>> = { walk: 'walkLeft', attack: 'attackLeft', attack2: 'attack2Left', cast: 'castLeft' };
/** Authored left-facing cuts: shown as drawn, never mirrored. */
const LEFT_CUTS = new Set<Pose>(['walkLeft', 'attackLeft', 'attack2Left', 'castLeft']);
const ATTACK_POSES = new Set<Pose>(['attack', 'attackLeft', 'attack2', 'attack2Left', 'attackShort', 'cast', 'castLeft', 'heal', 'counter']);
/** Ember's dirActionWalk: sprites with authored left/right walks; never mirrored while moving. */
const DIR_ACTION_WALK = new Set(['aldric', 'defaultLancer', 'lancer', 'sandoval', 'theButcher', 'familiar2', 'familiar3', 'cultist-v2', 'militia-v2', 'salazar', 'cobalt-blue-deer', 'neera']);
const ONE_SHOT: Pose[] = ['attack', 'attackLeft', 'attack2', 'attack2Left', 'attackShort', 'cast', 'castLeft', 'heal', 'counter', 'hit', 'hit2', 'death', 'death2'];
/** Ember's walkPose: moving up the map ("back"), down it ("front") or along a row ("side").
 * In 3D "up the map" is away from the camera. */
type WalkDir = 'back' | 'front' | 'side';
const WALK_STILL: Record<WalkDir, Pose> = { back: 'walkBack', front: 'walkFront', side: 'walkSide' };
const WALK_POSES: Pose[] = ['walk', 'walkLeft', 'walkUp', 'walkDown', 'walkBack', 'walkFront', 'walkSide'];
const STEP_SECONDS = 0.22; // Ember's normal walk speed per hex

const cardGeo = new THREE.PlaneGeometry(1, 1).translate(0, 0.5, 0);

export class UnitActor {
  /** Anchor at the feet: turns to face the camera, mirrors for facing. */
  readonly mesh: THREE.Group;
  readonly def: UnitDef;
  /** Screen-relative facing: 1 = right, -1 = left. */
  facing: 1 | -1 = 1;
  x = 0;
  z = 0;
  lightFade = 1;
  /** No pose sounds of its own: in battle Ember's engine plays every cue (audio.ts), timed to the action. */
  silent = false;
  private readonly lights = new THREE.Group();
  private readonly card: THREE.Mesh;
  private pose: Pose = 'idle';
  private shown: Pose = 'idle';
  private t = Math.random() * 3;
  private frames = new Map<Pose, PoseFrames>();
  private readonly mat: THREE.MeshLambertMaterial;
  private readonly depth: THREE.MeshDepthMaterial;
  private path: { x: number; z: number }[] = [];
  private dead = false;
  private walkDir: WalkDir = 'side';
  onPoseEnd: (pose: Pose) => void = () => {};

  constructor(def: UnitDef) {
    this.def = def;
    // art painted as glowing (the deer's crystal antlers) lights itself (UNIT_GLOW)
    this.mat = makeSpriteMaterial(true, UNIT_GLOW[def.sprite]);
    // frames.ts loads unit frames premultiplied: blend them as such (no second multiply by alpha)
    Object.assign(this.mat, { blending: THREE.CustomBlending, blendSrc: THREE.OneFactor, blendDst: THREE.OneMinusSrcAlphaFactor, blendSrcAlpha: THREE.OneFactor, blendDstAlpha: THREE.OneMinusSrcAlphaFactor });
    this.depth = new THREE.MeshDepthMaterial({ depthPacking: THREE.RGBADepthPacking, alphaTest: 0.5 });
    this.card = new THREE.Mesh(cardGeo, this.mat);
    this.card.castShadow = true;
    this.card.renderOrder = UNIT_RENDER_ORDER;
    // Cards don't receive shadow-map shadows: a flat card shadows itself whenever the sun is behind it.
    // Standing in a building's or tree's shadow will be tested at the unit's position instead (TODO).
    this.card.receiveShadow = false;
    this.card.customDepthMaterial = this.depth;
    this.card.userData.actor = this;
    this.mesh = new THREE.Group();
    this.mesh.add(this.card);
    this.mesh.add(this.lights);
    const lightDef = UNIT_LIGHT_DEFS[def.classId];
    if (lightDef) {
      const offsets = def.classId === 'familiar3'
        ? (def.footprint ?? [{ dx: 0, dy: 0 }]).map(o => ({ x: Math.sqrt(3) * (o.dx + ((o.dy & 1) ? 0.5 : 0)), z: o.dy * 1.5, radius: lightDef.radius }))
        : def.classId === 'familiar4'
          ? [{ x: 0, z: 0, radius: 3 }, { x: -Math.sqrt(3) / 2, z: -1.5, radius: 2 }, { x: Math.sqrt(3) / 2, z: -1.5, radius: 2 }]
          : [{ x: 0, z: 0, radius: lightDef.radius }];
      const front = offsets.filter(o => o.z === 0);
      const center = def.classId === 'familiar3' ? front.reduce((s, o) => s + o.x / front.length, 0) : 0;
      for (const offset of offsets) {
        const light = new CarriedLight(lightDef, this.t, offset.radius);
        light.position.x = offset.x - center; light.position.z = offset.z;
        this.lights.add(light);
      }
    }
    this.mesh.visible = false;
  }

  /** The mesh to raycast against when picking this unit. */
  get pickMesh(): THREE.Mesh { return this.card; }

  has(pose: Pose): boolean { return !!this.def.poses[pose]; }

  private keepSet(): Set<string> {
    const s = new Set<string>();
    for (const p of [this.pose, this.shown, 'idle' as Pose]) { const src = this.def.poses[p]; if (src) s.add(`${src.dir}|${src.prefix}`); }
    return s;
  }

  private async ensure(pose: Pose): Promise<PoseFrames | undefined> {
    const src = this.def.poses[pose];
    if (!src) return undefined;
    const f = await loadPose(src, this.keepSet());
    this.frames.set(pose, f);
    return f;
  }

  /** Load the idle so the unit can appear; warm its walk cuts in the background. */
  async ready(): Promise<void> {
    await this.ensure('idle');
    this.mesh.visible = true;
    // the walk cuts load now, not on first use: a 64-frame walk started mid-move glided on frames
    // still uploading (3 per screen frame) for its first hexes. Only the walks: every pose of
    // every unit overran the sprite budget (Neera alone is ~770 MB) and thrashed it.
    for (const p of WALK_POSES) if (this.def.poses[p]) await this.ensure(p).catch(() => undefined);
  }

  async preloadAll(): Promise<void> {
    for (const p of Object.keys(this.def.poses) as Pose[]) await this.ensure(p).catch(() => undefined);
  }

  /** Play a pose once (attacks, casts, hits, death) or switch the looping pose (idle, walk). */
  async play(pose: Pose): Promise<void> {
    // Ember: a unit with no counter cut of its own counters with its normal attack swing
    if (pose === 'counter' && !this.has('counter')) pose = 'attack';
    if (!this.has(pose)) return;
    const left = this.facing === -1 ? LEFT_CUT[pose] : undefined;
    const use = left && this.has(left) ? left : pose;
    await this.ensure(use);
    if (pose !== 'walk' && !this.silent) stopWalkSound(this.def);
    this.pose = pose;
    this.shown = use;
    this.t = 0;
    this.dead = pose === 'death' || pose === 'death2';
    if (pose !== 'idle' && pose !== 'idle2' && !this.silent) playPoseSound(this.def, pose);
  }

  revive(): void { this.dead = false; this.driven = null; void this.play('idle'); }

  /** Battle: Ember's engine picks the frame (BattleEngine.unitVisual), timed to its wind-ups,
   * releases, hits, counters and deaths; show that cut at that point `k` (0..1) of its sheet
   * instead of running a clock of our own. null hands timing back to play(). */
  private driven: number | null = null;
  drive(cut: Pose, k: number | null): void {
    if (k === null) { this.driven = null; return; }
    if (!this.has(cut)) return;
    if (this.shown !== cut) { this.shown = cut; void this.ensure(cut); }
    // the pose the mirroring rules read: the cut without its left/direction variant
    this.pose = cut.startsWith('walk') ? 'walk' : (cut.replace(/Left$/, '') as Pose);
    this.dead = cut === 'death' || cut === 'death2';
    this.driven = k;
  }

  /** Walk along world points (hex centres), one hex per Ember step. */
  async walk(points: { x: number; z: number }[]): Promise<void> {
    if (!points.length) return;
    this.path = points;
    await this.play(this.has('walk') ? 'walk' : 'idle');
  }

  get walking(): boolean { return this.path.length > 0; }

  /** What a moving unit shows, Ember's order: a still for this direction (walkDirs), else the up/down
   * cycle, else the side walk (its authored left cut when facing left). */
  private walkShown(): Pose {
    const still = WALK_STILL[this.walkDir];
    if (this.has(still)) return still;
    if (this.walkDir === 'back' && this.has('walkUp')) return 'walkUp';
    if (this.walkDir === 'front' && this.has('walkDown')) return 'walkDown';
    // Ember: the Lancer's move/move-left cuts read backwards against their own facing, so they swap
    const wantLeft = this.def.sprite === 'lancer' ? this.facing === 1 : this.facing === -1;
    if (wantLeft && this.has('walkLeft')) return 'walkLeft';
    return this.has('walk') ? 'walk' : this.pose;
  }

  /** `flatTop` set = the old-school 2D view (straight overhead): the card lies flat facing the
   * camera like a 2D sprite, raised to that height so terrain never covers it, nearer rows on top. */
  update(dt: number, cameraYaw: number, groundAt: (x: number, z: number) => number, flatTop: number | null = null): void {
    this.t += dt;
    if (this.path.length) {
      const next = this.path[0], dx = next.x - this.x, dz = next.z - this.z, dist = Math.hypot(dx, dz);
      const screenX = dx * Math.cos(cameraYaw) - dz * Math.sin(cameraYaw);
      if (Math.abs(screenX) > 0.05) this.facing = screenX > 0 ? 1 : -1;
      // Ember changes walkPose on any row change (a 60° hex step); same here, relative to the camera
      const away = -(dx * Math.sin(cameraYaw) + dz * Math.cos(cameraYaw));
      if (dist > 1e-4) this.walkDir = Math.abs(away) > dist * 0.5 ? (away > 0 ? 'back' : 'front') : 'side';
      if (this.pose === 'walk' || this.pose === 'idle') {
        const s = this.walkShown();
        if (s !== this.shown) { this.shown = s; void this.ensure(s); }
      }
      const move = Math.min(dist, (Math.sqrt(3) / STEP_SECONDS) * dt);
      if (dist > 1e-4) { this.x += (dx / dist) * move; this.z += (dz / dist) * move; }
      if (dist - move < 1e-3) { this.path.shift(); if (!this.path.length) { if (!this.silent) stopWalkSound(this.def); void this.play('idle'); } }
    }

    const f = this.frames.get(this.shown);
    // an evicted pose is never drawn: its textures would re-upload outside the budget and leak
    if (f?.evicted) void this.ensure(this.shown);
    if (!f || f.evicted) return;
    f.lastUsed = performance.now();
    const paced = this.driven === null ? framePace(this.def, this.pose, f.n, this.t, this.shown) : null;
    const frame = paced ? paced.frame : Math.min(f.n - 1, Math.max(0, Math.floor(this.driven! * f.n)));
    if (paced?.done && ONE_SHOT.includes(this.pose) && !this.dead) { const ended = this.pose; void this.play('idle'); this.onPoseEnd(ended); }

    const tex = f.textures[frame];
    if (this.mat.map !== tex) { this.mat.map = tex; this.depth.map = tex; this.mat.needsUpdate = true; this.depth.needsUpdate = true; }

    // Ember's box for this pose and frame; the trimmed frame sits exactly where it was inside it
    const box = unitBox(this.def, this.shown, frame, f.natW, f.natH);
    const cw = f.crop.w / f.natW, ch = f.crop.h / f.natH, u0 = f.crop.x / f.natW, vBottom = 1 - (f.crop.y + f.crop.h) / f.natH;
    this.card.scale.set(box.w * cw, box.h * ch, 1);
    // the pose's body shift (frames.ts measureBodyShift), so the legs stand on the hex
    const drift = f.bodyShift ? (f.bodyShift[frame] ?? 0) * (box.w * cw) / f.crop.w : 0;
    this.card.position.set(box.w * (u0 + cw / 2 - 0.5) - drift, box.h * vBottom - box.foot, 0);

    // In battle the engine moves the unit (no path of its own) while the pose is 'walk': keep the
    // walk cut matching its current facing, as Ember picks it every frame, so a unit that turns
    // mid-move never walks backwards on the other side's cut.
    const walking = this.path.length > 0 || this.pose === 'walk';
    if (!this.path.length && this.pose === 'walk' && this.driven === null) {
      const s = this.walkShown();
      if (s !== this.shown && this.has(s)) { this.shown = s; void this.ensure(s); }
    }
    const usesLeftCut = LEFT_CUTS.has(this.shown) || (walking && DIR_ACTION_WALK.has(this.def.sprite));
    const mirror = !usesLeftCut && ((this.facing === -1) !== this.drawnReversed());
    this.mesh.scale.set(mirror ? -1 : 1, 1, 1);
    const ground = groundAt(this.x, this.z), lean = flatTop === null ? 0 : Math.PI / 2;
    const y = flatTop === null ? ground : flatTop + this.z * FLAT_ROW_STEP + FLAT_ROW_STEP / 2; // over decor on its own row
    this.mesh.position.set(this.x, y, this.z);
    this.mesh.rotation.set(-lean, cameraYaw, 0, 'YXZ');
    this.card.castShadow = flatTop === null;
    alignUnitLights(this.lights, this.def.classId, cameraYaw, mirror, lean, ground, y);
    for (const light of this.lights.children as CarriedLight[]) light.update(this.t * 0.5, this.dead ? 0 : this.lightFade);
  }

  /** Ember's facing reversals (computeUnitVisual): art drawn facing the opposite way for this pose. */
  private drawnReversed(): boolean {
    const s = this.def.sprite, atk = ATTACK_POSES.has(this.pose);
    return !!this.def.drawnFacingLeft || this.def.classId === 'familiar' ||
      (s === 'defaultWarrior' && !atk) ||       // kael-v2 stand cut shot facing left, its ATT facing right
      s === 'cobalt-blue-deer' ||
      // Kael Final's idle is turned to the left; his walk and attack face right. In battle the
      // engine moves him (no path of his own), so the walk pose itself is what says he is walking.
      (s === 'kaelFinal' && !atk && this.pose !== 'walk' && !this.path.length);
  }

  dispose(): void { this.mat.dispose(); this.depth.dispose(); for (const light of this.lights.children as CarriedLight[]) light.dispose(); }
}
