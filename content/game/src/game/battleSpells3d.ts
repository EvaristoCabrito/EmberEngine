// OG Ember's spell systems and combat callbacks, with only the world-coordinate boundary
// changed for Engine2's XZ terrain. Never route gameplay through the preview's substitute FX.
import * as THREE from "three";
import { ZOOM_RADII, type BattleEngine, type MagicMissileV2VfxRequest } from "./engine";
import type { DioramaView } from "../editor/ember/dioramaView";
import { EmberVfxAdapter } from "../vfx/adapter";
import { FireballVFX } from "../vfx/legacy/three/FireballVFX";
import { CausticVenomVFX } from "../vfx/legacy/three/CausticVenomVFX";
import { BlessVFX, getActiveBlessVfxSettings } from "../vfx/legacy/three/BlessVFX";
import { PhantasmalForceVFX, getActivePhantasmalForceSettings } from "../vfx/legacy/three/PhantasmalForceVFX";
import { MagicMissileV2VFX, getActiveMagicMissileV2Settings } from "../vfx/legacy/three/MagicMissileV2VFX";
import { BurningHandsV2VFX, getActiveBurningHandsV2Settings } from "../vfx/legacy/three/BurningHandsV2VFX";
import { CleaveSweepVFX, VarreduraVFX } from "../vfx/legacy/three/VarreduraVFX";
import { WebOfDreamsVFX } from "../vfx/legacy/three/WebOfDreamsVFX";
import { BLESS } from "./data";
import type { Unit } from "./types";

const FLAGS = ["fireballVfxAvailable", "causticVenomVfxAvailable", "phantasmalForceVfxAvailable", "blessVfxAvailable", "magicMissileV2VfxAvailable", "burningHandsV2VfxAvailable"] as const;

export class BattleSpells3D {
  private readonly adapter: EmberVfxAdapter;
  private readonly camera: THREE.Camera;
  private readonly fireball: FireballVFX;
  private readonly venom: CausticVenomVFX;
  private readonly bless: BlessVFX;
  private readonly phantasm: PhantasmalForceVFX;
  private readonly missile: MagicMissileV2VFX;
  private readonly web: WebOfDreamsVFX;
  private readonly transient: Array<BurningHandsV2VFX | CleaveSweepVFX | VarreduraVFX> = [];
  private readonly pending: MagicMissileV2VfxRequest[] = [];
  private missileId: string | null = null;

  /** `bodyAt`: a unit's body centre in the world (Battle3D's card), where Magic Missile strikes. */
  constructor(private readonly view: DioramaView, private readonly engine: BattleEngine, private readonly bodyAt?: (unitId: string) => THREE.Vector3 | null) {
    this.adapter = new EmberVfxAdapter(view.stage.scene);
    this.camera = this.adapter.cameraToEmber(view.stage.camera);
    const root = this.adapter.root;
    this.fireball = new FireballVFX(root, this.camera);
    this.venom = new CausticVenomVFX(root);
    // Ember's top-down camera arced these up the screen (+y, which is map-north here) and sized
    // their arc/speed in canvas pixels; in 3D they arc up (+z) and one hex is Ember's 34 px.
    for (const fx of [this.fireball, this.venom]) { fx.up.set(0, 0, 1); fx.pixel = 1 / ZOOM_RADII[1]!; }
    this.bless = new BlessVFX(root); this.bless.setSettings(getActiveBlessVfxSettings());
    this.phantasm = new PhantasmalForceVFX(root); this.phantasm.setSettings(getActivePhantasmalForceSettings());
    this.missile = new MagicMissileV2VFX(root); this.missile.setSettings(getActiveMagicMissileV2Settings());
    this.web = new WebOfDreamsVFX(root);
    for (const flag of FLAGS) engine[flag] = true;
  }

  private at(cell: { x: number; y: number }, height = 0.04): THREE.Vector3 {
    const p = this.view.board!.cell(cell.x, cell.y);
    return this.adapter.toEmber(new THREE.Vector3(p.x, this.view.groundAt(p.x, p.z) + height, p.z));
  }
  private unit(unit: Unit, height = 0.9): THREE.Vector3 {
    const a = this.engine.unitAnchor(unit), zero = this.engine.effectAnchor(0, 0);
    const layout = this.view.board!.layout;
    const x = (a.worldX - zero.worldX) / zero.tile - layout.ox;
    const z = (a.worldY - zero.worldY) / zero.tile - layout.oz;
    return this.adapter.toEmber(new THREE.Vector3(x, this.view.groundAt(x, z) + height, z));
  }
  private visible(cell: { x: number; y: number }): boolean { return !this.engine.fogged || this.engine.visible(cell.x, cell.y); }

  sync(dt: number): void {
    if (!this.view.board) return;
    const engine = this.engine, root = this.adapter.root;
    this.adapter.cameraToEmber(this.view.stage.camera);
    const canvas = this.view.stage.renderer.domElement;
    this.fireball.update(dt, canvas.width, canvas.height, 1);
    this.venom.update(dt); this.phantasm.update(dt); this.bless.update(dt);
    if (this.missileId) this.missile.update(dt);

    for (const request of engine.fireballVfxRequests.splice(0)) {
      const caster = engine.units.find(u => u.id === request.casterId);
      if (!caster || !this.visible(caster) || !this.visible(request.target)) {
        engine.fireballVfxEvents.push({ id: request.id, phase: "impact" }, { id: request.id, phase: "complete" }); continue;
      }
      const target = this.at(request.target);
      const aoeRadius = request.tiles.reduce((radius, cell) => Math.max(radius, this.at(cell).distanceTo(target) + 1), 1);
      this.fireball.cast({ id: request.id, origin: this.unit(caster), target, worldScale: 1, aoeRadius,
        onLaunch: () => engine.fireballVfxEvents.push({ id: request.id, phase: "launch" }),
        onImpact: () => engine.fireballVfxEvents.push({ id: request.id, phase: "impact" }),
        onComplete: () => engine.fireballVfxEvents.push({ id: request.id, phase: "complete" }) });
      this.fireball.update(0, canvas.width, canvas.height, 1);
    }
    for (const request of engine.causticVenomVfxRequests.splice(0)) {
      const caster = engine.units.find(u => u.id === request.casterId && u.alive);
      if (!caster || !this.visible(caster) || !this.visible(request.target)) {
        engine.causticVenomVfxEvents.push({ id: request.id, phase: "impact" }, { id: request.id, phase: "complete" }); continue;
      }
      this.venom.cast({ id: request.id, origin: this.unit(caster, caster.classId === "zombieDog" ? 0.55 : 0.9), target: this.at(request.target), worldScale: 1,
        impactHexes: request.tiles.map(c => this.at(c)), onLaunch: () => undefined,
        onImpact: () => engine.causticVenomVfxEvents.push({ id: request.id, phase: "impact" }),
        onComplete: () => engine.causticVenomVfxEvents.push({ id: request.id, phase: "complete" }) });
      this.venom.update(0);
    }
    for (const request of engine.phantasmalForceVfxRequests.splice(0)) {
      const target = engine.units.find(u => u.id === request.targetUnitId && u.alive);
      if (!target || !this.visible(request.target)) {
        engine.phantasmalForceVfxEvents.push({ id: request.id, phase: "impact" }, { id: request.id, phase: "complete" }); continue;
      }
      // it strikes the middle of the target's body (its card), like Magic Missile, not its legs
      const body = this.bodyAt?.(target.id);
      this.phantasm.restartAt(body ? this.adapter.toEmber(body) : this.unit(target), 1, {
        onImpact: () => engine.phantasmalForceVfxEvents.push({ id: request.id, phase: "impact" }),
        onComplete: () => engine.phantasmalForceVfxEvents.push({ id: request.id, phase: "complete" }) });
      this.phantasm.update(0);
    }
    for (const request of engine.blessVfxRequests.splice(0)) {
      this.bless.castSpell({ id: request.id, center: this.at(request.center), radiusWorld: BLESS.radius * Math.sqrt(3),
        allies: request.allies.flatMap(ally => { const unit = engine.units.find(u => u.id === ally.id && u.alive); return unit ? [{ id: ally.id, position: this.unit(unit), distanceHexes: ally.distanceHexes }] : []; }),
        onApply: unitId => engine.blessVfxEvents.push({ id: request.id, phase: "apply", unitId }),
        onComplete: () => engine.blessVfxEvents.push({ id: request.id, phase: "complete" }),
        onTimelineEvent: (event, unitId) => engine.blessTimelineEvents.push({ id: request.id, event, unitId }) });
      this.bless.update(0);
    }
    // OG queues complete hero missiles so a subsequent target cannot erase the pooled shot.
    this.pending.push(...engine.magicMissileV2VfxRequests.splice(0));
    while (!this.missileId && this.pending.length) {
      const request = this.pending.shift()!;
      const caster = engine.units.find(u => u.id === request.casterId && u.alive);
      const target = engine.units.find(u => u.id === request.targetUnitId && u.alive);
      if (!caster || !target || !this.visible(target)) {
        engine.magicMissileV2VfxEvents.push({ id: request.id, phase: "impact", index: 0 }, { id: request.id, phase: "complete" }); continue;
      }
      this.missileId = request.id;
      // it strikes the middle of the target's body, not a fixed height (low on a tall foe)
      const body = this.bodyAt?.(target.id);
      this.missile.castSpell({ id: request.id, origin: this.unit(caster), target: body ? this.adapter.toEmber(body) : this.unit(target), missileCount: 1, worldScale: 1,
        onImpact: index => engine.magicMissileV2VfxEvents.push({ id: request.id, phase: "impact", index }),
        onComplete: () => { engine.magicMissileV2VfxEvents.push({ id: request.id, phase: "complete" }); this.missileId = null; },
        onTimelineEvent: (event, index) => engine.magicMissileV2TimelineEvents.push({ id: request.id, event, index }) });
      this.missile.update(0);
    }
    for (const request of engine.burningHandsV2VfxRequests.splice(0)) {
      const caster = engine.units.find(u => u.id === request.casterId && u.alive);
      if (!caster || !request.tiles.length) {
        engine.burningHandsV2VfxEvents.push({ id: request.id, phase: "release" }, { id: request.id, phase: "complete" }); continue;
      }
      const origin = this.unit(caster), cells = request.tiles.map(c => this.at(c));
      const average = cells.reduce((sum, cell) => sum.add(cell), new THREE.Vector3()).divideScalar(cells.length);
      const direction = new THREE.Vector2(average.x - origin.x, average.y - origin.y).normalize();
      const length = Math.max(1, Math.hypot(average.x - origin.x, average.y - origin.y));
      const halfWidth = cells.reduce((width, cell) => Math.max(width, Math.abs(-direction.y * (cell.x - origin.x) + direction.x * (cell.y - origin.y))), 0.38);
      this.transient.push(new BurningHandsV2VFX(root, { id: request.id, poison: request.poison, origin, direction, length, width: halfWidth * 2, worldScale: 1,
        settings: { ...getActiveBurningHandsV2Settings(), visuals: true }, targetPositions: cells,
        onRelease: () => engine.burningHandsV2VfxEvents.push({ id: request.id, phase: "release" }),
        onComplete: () => engine.burningHandsV2VfxEvents.push({ id: request.id, phase: "complete" }) }));
    }
    for (const [requests, sweep] of [[engine.varreduraVfxRequests, true], [engine.cleaveVfxRequests, false]] as const) {
      for (const request of requests.splice(0)) {
        const caster = engine.units.find(u => u.id === request.casterId); if (!caster) continue;
        // Ember's z here was only depth for its top-down camera; in 3D the Cleave blade swings at
        // weapon height through its targets, while Varredura's shockwave runs along the ground.
        const height = sweep ? 0.04 : 0.7;
        const targets = request.targetIds.flatMap(id => { const unit = engine.units.find(u => u.id === id); return unit ? [{ id, position: this.unit(unit, height) }] : []; });
        if (!targets.length) targets.push(...request.tiles.map(c => ({ id: `tile-${c.x}-${c.y}`, position: this.at(c, height) })));
        this.transient.push(sweep ? new VarreduraVFX(root, this.unit(caster, height), targets, 1) : new CleaveSweepVFX(root, this.unit(caster, height), targets, 1));
      }
    }
    for (let i = this.transient.length - 1; i >= 0; i--) { const fx = this.transient[i]!; fx.update(dt); if (fx.finished) { fx.dispose(); this.transient.splice(i, 1); } }
    // Keep the original zone lifecycle, including release when combat removes the zone.
    this.web.update({ time: engine.time, webZones: engine.webZones, units: engine.units,
      effectAnchor: (x, y) => { const p = this.at({ x, y }); return { worldX: p.x, worldY: -p.y, worldZ: p.z }; },
      unitAnchor: unit => { const p = this.unit(unit as Unit); return { worldX: p.x, worldY: -p.y, worldZ: p.z }; },
      webZoneRadiusTiles: radius => engine.webZoneRadiusTiles(radius),
      unitVisual: () => ({ footY: 0, footOffset: 0, h: 0, scaleY: 1, sway: 0, bob: 0, lift: 0 }) }, 1, dt);
  }
  dispose(): void {
    for (const flag of FLAGS) this.engine[flag] = false;
    for (const fx of [this.fireball, this.venom, this.bless, this.phantasm, this.missile, this.web, ...this.transient]) fx.dispose();
    this.adapter.dispose();
  }
}
