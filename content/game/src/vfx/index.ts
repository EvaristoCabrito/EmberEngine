import * as THREE from 'three';
import type { SpellKind } from '../ember/types';
import { WEB_SHOT_TRAVEL } from './engine';
import { EmberVfxAdapter } from './adapter';
import { FireballVFX } from './legacy/three/FireballVFX';
import { CausticVenomVFX } from './legacy/three/CausticVenomVFX';
import { BlessVFX } from './legacy/three/BlessVFX';
import { MagicMissileV2VFX } from './legacy/three/MagicMissileV2VFX';
import { WebOfDreamsVFX } from './legacy/three/WebOfDreamsVFX';
import { PhantasmalForceVFX } from './legacy/three/PhantasmalForceVFX';
import { BurningHandsV2VFX } from './legacy/three/BurningHandsV2VFX';
import { BurningHandsV3VFX } from './legacy/three/BurningHandsV3VFX';
import { CleaveSweepVFX, VarreduraVFX, DEFAULT_VARREDURA_SETTINGS } from './legacy/three/VarreduraVFX';
import { ProceduralElementEmitter, pixelPreset } from './legacy/three/ProceduralElementEmitter';
import type { FlipbookElement } from './legacy/three/ElementFlipbookAtlas';
export { EmberVfxAdapter } from './adapter';
export { ProceduralElementEmitter, pixelPreset, ELEMENT_FX_REGISTRY } from './legacy/three/ProceduralElementEmitter';
export { ParticleEmitter, DEFAULT_FIRE_EMITTER } from './legacy/three/ThreeVfxSystem';
export { EffectsRenderer, ELEMENT_KINDS, EFFECT_PARAMS } from './legacy/two-d/EffectsRenderer';
export { drawApparitionLightningV1 } from './legacy/two-d/ApparitionLightningV1';
export { drawProvokeVFX } from './legacy/two-d/ProvokeVFX';
export { drawTurnUndeadV1 } from './legacy/two-d/TurnUndeadV1';
export { drawTurnUndeadV2 } from './legacy/two-d/TurnUndeadV2';
export { drawTurnUndeadV3 } from './legacy/two-d/TurnUndeadV3';
export { drawTurnUndeadV4 } from './legacy/two-d/TurnUndeadV4';
export { spellIconFor, spellLabel, SPELL_ICON_ID } from './spell-catalog';

export type EffectKind = SpellKind | 'burningHandsV2' | 'burningHandsV3';
export const SPELL_EFFECTS = ['fireball', 'causticVenom', 'bless', 'magicMissileV2', 'webOfDreams', 'phantasmalForce', 'burningHandsV2', 'burningHandsV3', 'cleave', 'sweep', 'poisonBreath'] as const;
/** Every playable Ember spell is exposed by the Engine2 VFX preview. Ember has dedicated
 * preserved 3D systems for the named effects above; spells without bespoke renderers use their
 * matching preserved elemental atlas or the closest authored action effect. */
export const SPELL_VFX_FOR: Readonly<Record<SpellKind, EffectKind>> = {
  fireball: 'fireball', iceStorm: 'causticVenom', frost: 'causticVenom', bless: 'bless',
  cureMinor: 'bless', cureWounds: 'bless', cureLight: 'bless', longShot: 'magicMissileV2',
  bloodyShot: 'magicMissileV2', provoke: 'webOfDreams', piercing: 'magicMissileV2',
  lightning: 'magicMissileV2', lightningTier3: 'magicMissileV2', magicMissile: 'magicMissileV2',
  magicMissileV2: 'magicMissileV2', causticVenom: 'causticVenom', divineBolt: 'bless',
  minorVenom: 'causticVenom', doubleStrike: 'cleave', cleave: 'cleave', cureDisease: 'bless',
  piercingThrust: 'sweep', sweep: 'sweep', trip: 'cleave', summonFamiliar: 'phantasmalForce',
  phantasmalForce: 'phantasmalForce', fantomForce: 'phantasmalForce', summonFamiliar2: 'phantasmalForce',
  summonFamiliar3: 'phantasmalForce', summonFamiliar4: 'phantasmalForce', summonZombieDog: 'phantasmalForce',
  lifeDrain: 'phantasmalForce', webOfDreams: 'webOfDreams', warp: 'webOfDreams', multiShot: 'magicMissileV2',
  secondWind: 'bless', auraOfProtection: 'bless', divineWrath: 'bless', shoulderSmash: 'cleave',
  intimidatingPresence: 'webOfDreams', stampede: 'sweep', shock: 'magicMissileV2', bullRush: 'sweep',
  executionerStrike: 'cleave', shieldBash: 'cleave', poisonBreath: 'poisonBreath', tendrilSwipe: 'sweep',
  burningHands: 'burningHandsV2', turnUndead: 'bless', createFoodAndWater: 'bless',
};
export const SPELL_KINDS = Object.keys(SPELL_VFX_FOR) as SpellKind[];
/** Use Ember's actual element atlases wherever it has no dedicated 3D spell renderer. */
const SPELL_ELEMENT: Partial<Record<SpellKind, FlipbookElement>> = {
  iceStorm: 'frost', frost: 'frost', lightning: 'lightning', lightningTier3: 'lightning', shock: 'lightning',
  divineBolt: 'holy', minorVenom: 'poison', lifeDrain: 'shadow', turnUndead: 'holy', createFoodAndWater: 'holy',
  auraOfProtection: 'holy', intimidatingPresence: 'shadow', secondWind: 'holy',
};
const BESPOKE_EFFECTS = new Set<SpellKind>([
  'fireball', 'causticVenom', 'bless', 'magicMissileV2', 'webOfDreams', 'phantasmalForce',
  'burningHands', 'cleave', 'sweep', 'poisonBreath',
]);
export function spell3DEffectFor(kind: SpellKind): string {
  const element = !BESPOKE_EFFECTS.has(kind) ? SPELL_ELEMENT[kind] : undefined;
  return element ? `${element} flipbook` : SPELL_VFX_FOR[kind];
}
export interface VfxTarget { id: string; position: THREE.Vector3 }
export interface ImpactEvent { kind: EffectKind; castId: string; targets: VfxTarget[]; targetId?: string; missileIndex?: number }
export interface VfxOptions {
  camera?: THREE.Camera;
  viewport?: THREE.Vector2;
  /** When true the caller advances visual time with updateVfx; otherwise a private RAF advances it. */
  manualUpdate?: boolean;
  onImpact?: (event: ImpactEvent) => void;
  /** Web fields persist until the rules release their zone; standalone casts preview a finite field. */
  webDuration?: number;
}
type Effect = { update(dt: number): void; dispose(): void };
type Job = { root: THREE.Object3D; effect: Effect; resolve: () => void; reject: (error: unknown) => void; complete: boolean; resources: Set<{ dispose(): void }> };
type Context = { adapter: EmberVfxAdapter; options: VfxOptions; jobs: Set<Job>; frame: number; last: number };
const contexts = new WeakMap<THREE.Scene, Context>();
let serial = 0;

export function configureVfx(scene: THREE.Scene, options: VfxOptions): void {
  let context = contexts.get(scene);
  if (!context) { context = { adapter: new EmberVfxAdapter(scene), options: {}, jobs: new Set(), frame: 0, last: 0 }; contexts.set(scene, context); }
  context.options = { ...context.options, ...options };
}
function collect(job: Job): void {
  job.root.traverse(object => {
    const mesh = object as THREE.Mesh;
    if (mesh.geometry) job.resources.add(mesh.geometry);
    const materials = mesh.material ? (Array.isArray(mesh.material) ? mesh.material : [mesh.material]) : [];
    for (const material of materials) {
      job.resources.add(material);
      for (const value of Object.values(material)) if (value instanceof THREE.Texture) job.resources.add(value);
      if (material instanceof THREE.ShaderMaterial) for (const uniform of Object.values(material.uniforms)) if (uniform.value instanceof THREE.Texture) job.resources.add(uniform.value);
    }
    if (object instanceof THREE.InstancedMesh) job.resources.add(object);
    if (object instanceof THREE.PointLight && object.shadow) job.resources.add(object.shadow);
  });
}
function finish(context: Context, job: Job): void {
  collect(job);
  job.effect.dispose();
  // Ember's Cleave intentionally retained newly-created materials for shader caching. This
  // owner releases them too: retaining per-cast materials is unsafe on a 4 GB device.
  for (const resource of job.resources) resource.dispose();
  job.resources.clear();
  job.root.removeFromParent();
  context.jobs.delete(job);
  job.resolve();
}
export function updateVfx(scene: THREE.Scene, dt: number): void {
  const context = contexts.get(scene); if (!context) return;
  for (const job of [...context.jobs]) {
    try { collect(job); job.effect.update(Math.max(0, Math.min(dt, 0.08))); collect(job); if (job.complete) finish(context, job); }
    catch (error) { job.reject(error); finish(context, job); }
  }
}
function run(scene: THREE.Scene, context: Context): void {
  if (context.frame || context.options.manualUpdate) return;
  context.last = performance.now();
  const frame = (now: number) => {
    context.frame = 0;
    updateVfx(scene, (now - context.last) / 1000); context.last = now;
    if (context.jobs.size) context.frame = requestAnimationFrame(frame);
  };
  context.frame = requestAnimationFrame(frame);
}
/** Visual promise resolves after the original effect timeline. Combat applies rules immediately
 * and can use onImpact solely to synchronize presentation (never await visuals for rules). */
export function playSpell(kind: EffectKind, from: THREE.Vector3, targets: readonly (VfxTarget | THREE.Vector3)[], scene: THREE.Scene, options: VfxOptions = {}): Promise<void> {
  const visualKind = kind in SPELL_VFX_FOR ? SPELL_VFX_FOR[kind as SpellKind] : kind;
  if (!SPELL_EFFECTS.includes(visualKind as typeof SPELL_EFFECTS[number]) && visualKind !== 'burningHands') return Promise.reject(new Error(`No preserved 3D effect registered for ${kind}`));
  configureVfx(scene, options);
  const context = contexts.get(scene)!;
  const opts = { ...context.options, ...options };
  const root = new THREE.Scene(); context.adapter.root.add(root);
  const origin = context.adapter.toEmber(from);
  const resolved = targets.map((target, index): VfxTarget => target instanceof THREE.Vector3 ? { id: `target-${index}`, position: target.clone() } : { id: target.id, position: target.position.clone() });
  const localTargets = resolved.map(target => ({ id: target.id, position: context.adapter.toEmber(target.position) }));
  const target = localTargets[0]?.position ?? origin.clone();
  const castId = `engine2-vfx-${++serial}`;
  const impact = (targetId?: string, missileIndex?: number) => opts.onImpact?.({ kind, castId, targets: resolved, targetId, missileIndex });
  return new Promise<void>((resolve, reject) => {
    const job: Job = { root, effect: undefined as unknown as Effect, complete: false, resources: new Set(), resolve, reject };
    const done = () => { job.complete = true; };
    const tile = 1 / context.adapter.scale;
    try {
      const pixelElement = !BESPOKE_EFFECTS.has(kind as SpellKind) ? SPELL_ELEMENT[kind as SpellKind] : undefined;
      if (pixelElement) {
        // Engine2's imported flipbook emitters are Y-up; keep them outside Ember's Z-up adapter.
        const elementRoot = new THREE.Group();
        elementRoot.name = `spell-${kind}-flipbook`;
        scene.add(elementRoot);
        const emitter = new ProceduralElementEmitter(elementRoot, pixelPreset(pixelElement, `procedural_pixel_v2_${pixelElement}`), {
          duration: 1.5, loop: false, seed: ++serial, lightEnabled: false,
        });
        elementRoot.position.copy(resolved[0]?.position ?? from);
        job.root.removeFromParent();
        job.root = elementRoot;
        let elapsed = 0;
        let applied = false;
        job.effect = {
          update(dt) {
            elapsed += dt;
            emitter.update(dt, tile, elapsed, 0, 0);
            if (!applied && elapsed >= 0.48) { applied = true; impact(resolved[0]?.id); }
            if (elapsed >= 2.2) done();
          },
          dispose: () => emitter.dispose(),
        };
      } else if (visualKind === 'fireball') {
        const camera = context.adapter.cameraToEmber(opts.camera ?? new THREE.PerspectiveCamera(45, 1, 0.1, 1000));
        const effect = new FireballVFX(root, camera);
        effect.cast({ id: castId, origin, target, worldScale: tile, aoeRadius: tile * 2 * Math.sqrt(3), onLaunch: () => {}, onImpact: () => impact(), onComplete: done });
        job.effect = { update(dt) { if (opts.camera) { const current = context.adapter.cameraToEmber(opts.camera); camera.quaternion.copy(current.quaternion); camera.matrixWorld.copy(current.matrixWorld); camera.matrixWorldInverse.copy(current.matrixWorldInverse); } effect.update(dt, opts.viewport?.x ?? 1600, opts.viewport?.y ?? 1000, tile); }, dispose: () => effect.dispose() };
      } else if (visualKind === 'causticVenom') {
        const effect = new CausticVenomVFX(root);
        effect.cast({ id: castId, origin, target, worldScale: tile, impactHexes: localTargets.map(t => t.position), onLaunch: () => {}, onImpact: () => impact(), onComplete: done }); job.effect = effect;
      } else if (visualKind === 'bless') {
        const effect = new BlessVFX(root);
        effect.castSpell({ id: castId, center: origin, radiusWorld: Math.max(tile * 4 * Math.sqrt(3), ...localTargets.map(t => t.position.distanceTo(origin))), allies: localTargets.map(t => ({ ...t, distanceHexes: t.position.distanceTo(origin) / (Math.sqrt(3) * tile) })), onApply: id => impact(id), onComplete: done }); job.effect = effect;
      } else if (visualKind === 'magicMissileV2') {
        const effect = new MagicMissileV2VFX(root);
        effect.castSpell({ id: castId, origin, target, worldScale: tile, onImpact: index => impact(localTargets[0]?.id, index), onComplete: done }); job.effect = effect;
      } else if (visualKind === 'phantasmalForce') {
        const effect = new PhantasmalForceVFX(root); effect.restartAt(target, tile, { onImpact: () => impact(), onComplete: done }); job.effect = effect;
      } else if (visualKind === 'webOfDreams') {
        const effect = new WebOfDreamsVFX(root); let elapsed = 0; let applied = false; let released = false;
        const duration = Math.max(1.6, opts.webDuration ?? 2.6);
        job.effect = { update(dt) { elapsed += dt; if (elapsed >= WEB_SHOT_TRAVEL) effect.previewAt(target, tile, 1, localTargets, dt); if (!applied && elapsed >= WEB_SHOT_TRAVEL) { applied = true; impact(); } if (!released && elapsed >= duration) { released = true; effect.releasePreview(); } if (elapsed >= duration + 0.95) done(); }, dispose: () => effect.dispose() };
      } else if (visualKind === 'cleave' || visualKind === 'sweep') {
        const effect = visualKind === 'sweep' ? new VarreduraVFX(root, origin, localTargets, tile) : new CleaveSweepVFX(root, origin, localTargets, tile);
        let elapsed = 0;
        const direction = new THREE.Vector2(); for (const t of localTargets) direction.add(new THREE.Vector2(t.position.x - origin.x, t.position.y - origin.y)); if (direction.lengthSq() < 0.001) direction.set(1, 0); direction.normalize();
        // Neither original melee renderer has a callback. Match its actual per-target light
        // arrival exactly, instead of introducing a new visual/gameplay timing constant.
        const arrivals = localTargets.map(t => visualKind === 'sweep' ? 0.12 + Math.min(1, t.position.distanceTo(origin) / (tile * 3.2)) * 0.68 : 0.36 + THREE.MathUtils.clamp(new THREE.Vector2(t.position.x - origin.x, t.position.y - origin.y).dot(direction) / (DEFAULT_VARREDURA_SETTINGS.radius * tile), 0, 1) * (0.46 / Math.max(0.25, DEFAULT_VARREDURA_SETTINGS.waveSpeed)));
        const applied = new Set<number>();
        job.effect = { update(dt) { elapsed += dt; effect.update(dt); for (let index = 0; index < arrivals.length; index++) if (!applied.has(index) && elapsed >= arrivals[index]!) { applied.add(index); impact(localTargets[index]!.id); } if (effect.finished) done(); }, dispose: () => effect.dispose() };
      } else {
        const direction = new THREE.Vector2(target.x - origin.x, target.y - origin.y).normalize();
        const cast = { id: castId, origin, direction, length: origin.distanceTo(target), width: tile * Math.sqrt(3), worldScale: tile, targetPositions: localTargets.map(t => t.position), onRelease: () => impact(), onComplete: done };
        job.effect = visualKind === 'burningHandsV3' ? new BurningHandsV3VFX(root, cast) : new BurningHandsV2VFX(root, { ...cast, poison: visualKind === 'poisonBreath' });
      }
      collect(job); context.jobs.add(job); run(scene, context);
    } catch (error) {
      collect(job);
      if (job.effect) job.effect.dispose();
      for (const resource of job.resources) resource.dispose();
      job.root.removeFromParent(); reject(error);
    }
  });
}
/** Cancel visuals on map unload; settles outstanding visual promises and frees all owned GPU resources. */
export function disposeVfx(scene: THREE.Scene): void {
  const context = contexts.get(scene); if (!context) return;
  if (context.frame) cancelAnimationFrame(context.frame);
  for (const job of [...context.jobs]) finish(context, job);
  context.adapter.dispose(); contexts.delete(scene);
}
export function activeVfxCount(scene: THREE.Scene): number { return contexts.get(scene)?.jobs.size ?? 0; }
