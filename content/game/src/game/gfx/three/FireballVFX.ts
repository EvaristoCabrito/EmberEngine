import * as THREE from "three";
import { OldFireBall } from "./OldFireBall";
import { getActiveImpactSettings, FireballImpactVFX } from "./FireballImpactVFX";
import { loadFireFlipbook } from "./ThreeVfxSystem";

export interface FireballCastOptions {
  id: string;
  origin: THREE.Vector3;
  target: THREE.Vector3;
  worldScale: number;
  /** World-space radius enclosing the exact cells that receive this cast's damage. */
  aoeRadius: number;
  onLaunch: () => void;
  onImpact: () => void;
  onComplete: () => void;
}

type Phase = "idle" | "travel" | "impact";

/** One reusable combat fireball timeline. It owns visuals only; combat owns targets and damage. */
export class FireballVFX {
  private readonly projectile: OldFireBall;
  private readonly impact: FireballImpactVFX;
  private fallbackImpactActive = false;
  private impactTexture: THREE.Texture;
  private impactSettings = getActiveImpactSettings();
  private phase: Phase = "idle";
  private elapsed = 0;
  private travelDuration = 0;
  private worldScale = 1;
  private impactScale = 1;
  private aoeRadius = 1;
  private completedImpact = false;
  private currentCast: FireballCastOptions | null = null;
  private readonly start = new THREE.Vector3();
  private readonly target = new THREE.Vector3();
  private readonly position = new THREE.Vector3();
  private readonly screenRight = new THREE.Vector3(1, 0, 0);
  private readonly screenUp = new THREE.Vector3(0, -1, 0);
  private disposed = false;

  constructor(
    private readonly scene: THREE.Object3D,
    private readonly camera: THREE.Camera,
    projectile?: OldFireBall,
  ) {
    const viewport = new THREE.Vector2(1, 1);
    // This is the actual 3D fireball model, traveling only between the caster and target.
    // It carries its PointLight as a child, so illumination follows the projectile.
    // Reuse the tavern preview's fireball model for the real cast. It is hidden while idle and
    // only follows the caster-to-target trajectory started by Fireball.cast().
    this.projectile = projectile ?? new OldFireBall(1.7);
    this.projectile.group.visible = false;
    this.projectile.light.visible = true;
    // Use the same original 3D tavern fireball model for the cast trajectory. No billboard or
    // autonomous map loop: this group is shown only between the caster and the spell target.
    this.projectile.coreMesh.visible = true;
    this.projectile.shellMesh.visible = true;
    this.projectile.coreMesh.renderOrder = 30;
    this.projectile.shellMesh.renderOrder = 31;
    // Live impact particles are all procedural (fire lobes, sparks and smoke), so they do not
    // need to wait for the editor's flipbook image before Fireball can become cast-ready.
    this.impactTexture = new THREE.DataTexture(new Uint8Array([255, 255, 255, 255]), 1, 1, THREE.RGBAFormat);
    this.impactTexture.needsUpdate = true;
    this.impact = new FireballImpactVFX(this.impactTexture, null, 0.1, 2000, viewport, true);
    this.impact.mesh.renderOrder = 32;
    this.impact.flash.renderOrder = 33;
    this.impact.mesh.visible = false;
    this.impact.flash.visible = false;
    this.impact.light.visible = false;
    // The original Etapa 02 impact remains the battle default, with its actual fire flipbook.
    this.scene.add(this.projectile.group, this.impact.mesh, this.impact.flash, this.impact.light);
    void loadFireFlipbook().then((texture) => {
      if (this.disposed) { texture.dispose(); return; }
      this.impactTexture.dispose();
      this.impactTexture = texture;
      this.setImpactFlipbook(texture);
    }).catch((error) => console.error("Falha ao carregar o flipbook original de Fireball", error));
  }

  cast(options: FireballCastOptions): void {
    if (this.disposed) return;
    this.cancel();
    this.currentCast = options;
    // Dev Controls tunes the same settings used by real combat impacts.
    this.impactSettings = getActiveImpactSettings();
    this.start.copy(options.origin);
    this.target.copy(options.target);
    this.worldScale = Math.max(1, options.worldScale);
    this.aoeRadius = Math.max(0, options.aoeRadius);
    // Etapa 02 was authored for a one-hex preview. Scale the original impact to the
    // radius-two combat footprint while keeping that footprint as its hard outer limit.
    this.impactScale = Math.max(this.worldScale, this.aoeRadius * 0.72);
    this.projectile.group.scale.setScalar(this.worldScale * 0.47);
    this.projectile.light.visible = true;
    const distance = this.start.distanceTo(this.target);
    // Long, readable spell travel. The previous 0.34–1.2s window made Fireball feel rushed
    // across a tactical board before its flame surface could be seen.
    this.travelDuration = THREE.MathUtils.clamp(distance / 260, 1.25, 2.5);
    this.elapsed = 0;
    this.phase = "travel";
    this.completedImpact = false;
    this.projectile.group.visible = true;
    this.impact.mesh.visible = false;
    this.impact.flash.visible = false;
    this.impact.light.visible = false;
    this.projectile.light.visible = true;
    options.onLaunch();
    this.updateProjectile(0);
  }

  /** Restore the same flame flipbook used by the isolated Etapa 02 impact preview. */
  setImpactFlipbook(texture: THREE.Texture): void {
    if (this.disposed) { texture.dispose(); return; }
    this.impact.material.uniforms.uFlipbook!.value = texture;
  }

  update(dt: number, viewportWidth: number, viewportHeight: number, tile: number): void {
    if (this.disposed || this.phase === "idle") return;
    this.worldScale = Math.max(1, tile);
    this.projectile.group.scale.setScalar(this.worldScale * 0.47);
    const safeDt = Math.max(0, Math.min(0.1, dt));
    if (this.phase === "travel") {
      this.elapsed = Math.min(this.travelDuration, this.elapsed + safeDt);
      this.updateProjectile(safeDt);
      if (this.elapsed >= this.travelDuration) this.beginImpact();
      return;
    }
    if (this.fallbackImpactActive) {
      // The preview uses one world unit per hex; the battle scene uses screen-pixel world
      // units. Scale the exact same Etapa 02 particles and light by the live tile size.
      this.impact.update(safeDt, this.impactSettings, false, this.camera, new THREE.Vector2(viewportWidth, viewportHeight), this.impactScale, this.aoeRadius);
      this.impact.light.intensity *= 1.5;
    }
    const impactFinished = this.fallbackImpactActive && this.impact.finished;
    if (impactFinished && !this.completedImpact) {
      this.completedImpact = true;
      this.phase = "idle";
      this.currentCast?.onComplete();
      this.currentCast = null;
    }
    // The impact system's own timeline owns light intensity. Keep the projectile light off.
    this.projectile.group.visible = false;
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.cancel();
    this.scene.remove(this.projectile.group, this.impact.mesh, this.impact.flash, this.impact.light);
    this.projectile.dispose();
    this.impact.dispose();
    this.impactTexture.dispose();
  }

  private updateProjectile(dt: number): void {
    const progress = this.travelDuration > 0 ? this.elapsed / this.travelDuration : 1;
    const eased = progress * progress * (3 - 2 * progress);
    this.position.lerpVectors(this.start, this.target, eased);
    const arc = Math.sin(Math.PI * progress) * Math.max(15, this.start.distanceTo(this.target) * 0.12);
    this.position.addScaledVector(this.screenUp, arc);
    this.projectile.group.position.copy(this.position);
    this.projectile.update(this.elapsed);
    this.projectile.light.intensity = 8 * 4.9 * this.worldScale * this.worldScale * (0.94 + Math.sin(this.elapsed * 7.2) * 0.06);
    this.projectile.light.distance = this.worldScale * 5;
    this.projectile.light.decay = 2;
    this.projectile.light.shadow.camera.far = this.projectile.light.distance;
    this.projectile.light.shadow.camera.updateProjectionMatrix();
  }

  private beginImpact(): void {
    if (!this.currentCast) return;
    this.phase = "impact";
    this.projectile.group.visible = false;
    this.projectile.light.visible = false;
    this.impact.mesh.visible = false;
    this.impact.flash.visible = false;
    this.impact.light.visible = false;
    this.fallbackImpactActive = false;
    const cast = this.currentCast;
    const startImpact = () => {
      if (this.disposed || this.phase !== "impact" || this.currentCast !== cast || !cast) return;
      // Keep the original Fireball impact; the removed V2 never participates in combat.
      this.fallbackImpactActive = true;
      this.impact.setOrigin(this.target);
      this.impact.restart(this.impactSettings);
      this.impact.mesh.visible = true;
      this.impact.light.visible = true;
      cast.onImpact();
    };
    startImpact();
  }

  private cancel(): void {
    this.phase = "idle";
    this.currentCast = null;
    this.projectile.group.visible = false;
    this.projectile.light.visible = false;
    this.impact.mesh.visible = false;
    this.impact.flash.visible = false;
    this.impact.light.intensity = 0;
    this.impact.light.visible = false;
    this.fallbackImpactActive = false;
  }
}
