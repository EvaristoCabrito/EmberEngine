import * as THREE from "three";
import { DEFAULT_FIRE_EMITTER, loadFireFlipbook, ParticleEmitter, type FireEmitterSettings } from "./ThreeVfxSystem";

export interface BurningHandsV3Settings {
  /** Compatibility name retained for saved FX Lab settings; now controls overlapping flame count. */
  tongueCount: number;
  flameLength: number;
  flameWidth: number;
  turbulence: number;
  lightIntensity: number;
  lightRadius: number;
  /** Legacy geometry/particle toggles are migrated to one honest visibility control. */
  visuals: boolean;
  distortion: boolean;
  emissive: boolean;
  lights: boolean;
  bloom: boolean;
}

export const DEFAULT_BURNING_HANDS_V3_SETTINGS: BurningHandsV3Settings = {
  tongueCount: 10,
  flameLength: 1,
  flameWidth: 1,
  turbulence: 0.2,
  lightIntensity: 18,
  lightRadius: 5.2,
  visuals: true,
  distortion: true,
  emissive: true,
  lights: true,
  bloom: true,
};

const SETTINGS_KEY = "emberash:burning-hands-v3-settings";
export function getActiveBurningHandsV3Settings(): BurningHandsV3Settings {
  if (typeof window === "undefined") return { ...DEFAULT_BURNING_HANDS_V3_SETTINGS };
  try {
    const raw = window.localStorage.getItem(SETTINGS_KEY);
    if (!raw) return { ...DEFAULT_BURNING_HANDS_V3_SETTINGS };
    const saved = JSON.parse(raw) as Partial<BurningHandsV3Settings> & { geometry?: boolean; particles?: boolean };
    const currentSettings = { ...saved };
    delete currentSettings.geometry;
    delete currentSettings.particles;
    delete currentSettings.visuals;
    return {
      ...DEFAULT_BURNING_HANDS_V3_SETTINGS,
      ...currentSettings,
      // Persisted FX Lab isolation toggles must not make the real spell or a fresh preview blank.
      visuals: true,
    };
  } catch { return { ...DEFAULT_BURNING_HANDS_V3_SETTINGS }; }
}
export function setActiveBurningHandsV3Settings(settings: BurningHandsV3Settings): void {
  if (typeof window === "undefined") return;
  try { window.localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch { /* storage may be disabled */ }
}

export interface BurningHandsV3Cast {
  id: string;
  origin: THREE.Vector3;
  direction: THREE.Vector2;
  length: number;
  width: number;
  worldScale: number;
  /** World-space centers of the resolved Burning Hands cells. */
  targetPositions?: THREE.Vector3[];
  settings?: BurningHandsV3Settings;
  onRelease: () => void;
  onComplete: () => void;
}

// The spell still releases its targets on the established combat frame. Its visual is one
// uninterrupted outward spray; only particle emission stops before the embers fade away.
const SPRAY_DURATION = 0.72;
const VISUAL_FINISH = 2.35;
const GAMEPLAY_RELEASE = 0.56;
const GAMEPLAY_COMPLETE = 2.35;
const FIRE_LIGHT_COUNT = 5;
const MAX_PARTICLES_PER_EMITTER = 96;

type FlameEmitter = { emitter: ParticleEmitter; started: boolean };

/** Burning Hands V2 reuses the FX Lab's real 4x4 flipbook flame and particle shader. Combat
 * remains authoritative for target cells, damage, release and completion events. */
export class BurningHandsV3VFX {
  readonly group = new THREE.Group();
  private readonly lights: THREE.PointLight[] = [];
  private readonly emitters: FlameEmitter[] = [];
  private texture: THREE.Texture | null = null;
  private elapsed = 0;
  private releaseSent = false;
  private completeSent = false;
  private disposed = false;
  private settings: BurningHandsV3Settings;
  private readonly direction: THREE.Vector2;
  private readonly sideways: THREE.Vector2;
  private readonly length: number;
  private readonly width: number;
  private readonly tile: number;

  get finished(): boolean { return this.disposed; }

  constructor(private readonly scene: THREE.Scene, private readonly cast: BurningHandsV3Cast) {
    this.settings = { ...DEFAULT_BURNING_HANDS_V3_SETTINGS, ...cast.settings };
    this.direction = cast.direction.clone().normalize();
    if (this.direction.lengthSq() < 0.001) this.direction.set(0, 1);
    this.sideways = new THREE.Vector2(-this.direction.y, this.direction.x);
    this.tile = Math.max(1, cast.worldScale);
    this.length = Math.max(this.tile, cast.length) * this.settings.flameLength;
    this.width = Math.max(this.tile * 0.7, cast.width) * this.settings.flameWidth;
    this.group.position.copy(cast.origin);
    this.scene.add(this.group);

    // A compact pool of real THREE.PointLights: both hands stay lit, while three points move
    // along the live flame front and fade with the last particles.
    for (let i = 0; i < FIRE_LIGHT_COUNT; i++) {
      const light = new THREE.PointLight(i < 2 ? 0xffff82 : 0xff8a2b, 0, this.settings.lightRadius * this.tile, 2);
      light.castShadow = false;
      this.lights.push(light);
      this.group.add(light);
    }

    void loadFireFlipbook().then((texture) => {
      if (this.disposed) { texture.dispose(); return; }
      this.texture = texture;
      this.createFlameEmitters();
      this.update(0);
    }).catch(() => { /* The FX Lab fire atlas is required for this effect. */ });
  }

  setSettings(settings: BurningHandsV3Settings): void {
    this.settings = { ...settings };
    for (const entry of this.emitters) {
      entry.emitter.mesh.scale.setScalar(this.tile);
      entry.emitter.mesh.visible = this.visualsEnabled() && entry.started && this.elapsed < VISUAL_FINISH;
      entry.emitter.material.uniforms.uIntensity!.value = this.settings.emissive ? DEFAULT_FIRE_EMITTER.coreIntensity : 1;
    }
    for (const light of this.lights) light.distance = Math.max(0.1, this.settings.lightRadius * this.tile);
  }

  update(dt: number): void {
    if (this.disposed) return;
    this.elapsed = Math.min(GAMEPLAY_COMPLETE, this.elapsed + Math.max(0, Math.min(dt, 0.08)));
    const time = this.elapsed;
    const visualsOn = this.visualsEnabled() && time < VISUAL_FINISH;

    for (const entry of this.emitters) {
      if (!entry.started) {
        entry.started = true;
        entry.emitter.reset(this.emitterSettings(), true);
      }
      if (entry.started) {
        entry.emitter.update(dt, this.emitterSettings(), time < SPRAY_DURATION);
      }
      entry.emitter.mesh.visible = visualsOn && entry.started;
    }

    this.updateWorldLights(time);
    if (!this.releaseSent && time >= GAMEPLAY_RELEASE) {
      this.releaseSent = true;
      this.cast.onRelease();
    }
    if (!this.completeSent && time >= GAMEPLAY_COMPLETE) {
      this.completeSent = true;
      this.cast.onComplete();
      this.dispose();
    }
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.scene.remove(this.group);
    for (const light of this.lights) this.group.remove(light);
    for (const entry of this.emitters) {
      entry.emitter.geometry.dispose();
      entry.emitter.material.dispose();
      entry.emitter.light.removeFromParent();
    }
    this.texture?.dispose();
  }

  private createFlameEmitters(): void {
    if (!this.texture || this.disposed) return;
    this.addEmitter();
  }

  private addEmitter(): void {
    if (!this.texture) return;
    const emitter = new ParticleEmitter(this.texture, null, {
      sceneNear: 0.1,
      sceneFar: 2000,
      viewport: new THREE.Vector2(1, 1),
      intensity: DEFAULT_FIRE_EMITTER.coreIntensity,
    });
    emitter.setSeed(this.seedFor("spray", 0));
    emitter.mesh.position.set(0, 0, this.tile * 0.2);
    emitter.mesh.scale.setScalar(this.tile);
    emitter.mesh.renderOrder = 90;
    emitter.mesh.frustumCulled = false;
    emitter.mesh.visible = false;
    emitter.light.visible = false;
    this.group.add(emitter.mesh);
    this.emitters.push({ emitter, started: false });
  }

  private emitterSettings(): FireEmitterSettings {
    const settings = this.settings;
    const count = Math.max(48, Math.min(MAX_PARTICLES_PER_EMITTER, Math.round(settings.tongueCount * 8)));
    return {
      ...DEFAULT_FIRE_EMITTER,
      particleCount: count,
      // Particle quad sizes are shader-space values; scale the actual flames to map pixels.
      particleScale: DEFAULT_FIRE_EMITTER.particleScale * this.tile * 1.5 * settings.flameLength,
      velocity: DEFAULT_FIRE_EMITTER.velocity * 1.25,
      turbulence: settings.turbulence * (settings.distortion ? 1.25 : 0.9),
      coreIntensity: settings.emissive ? DEFAULT_FIRE_EMITTER.coreIntensity * 1.5 : DEFAULT_FIRE_EMITTER.coreIntensity,
      lightIntensity: 0,
      lifetimeScale: 1.7,
      spread: 0.75 * settings.flameWidth,
      flowDirection: [this.direction.x, this.direction.y, 0],
      flowSpeed: 3.5,
      flowSpread: Math.min(1.0, Math.atan2(this.width * 0.5, this.length) * 1.5),
    };
  }

  private visualsEnabled(): boolean {
    // The FX Lab has one honest flame-visibility control; there is no retired cone geometry.
    return this.settings.visuals;
  }

  private updateWorldLights(time: number): void {
    const fadeOut = 1 - THREE.MathUtils.smoothstep(time, SPRAY_DURATION, VISUAL_FINISH);
    const front = this.length * THREE.MathUtils.smoothstep(time, 0, SPRAY_DURATION);
    const flicker = 0.92 + Math.sin(time * 21) * 0.045 + Math.sin(time * 37 + 1.2) * 0.035;
    const sourceWeights = [0.72, 0.8, 0.88, 0.8, 0.72];
    const frontRatios = [0.12, 0.32, 0.54, 0.76, 0.96];
    for (let index = 0; index < this.lights.length; index++) {
      const light = this.lights[index]!;
      const ratio = frontRatios[index]!;
      const forward = front * ratio;
      const sideways = ((index / (this.lights.length - 1)) - 0.5) * this.width * ratio;
      const x = this.direction.x * forward + this.sideways.x * sideways;
      const y = this.direction.y * forward + this.sideways.y * sideways;
      light.position.set(x, y, this.tile * 0.22);
      const heat = 0.2 - Math.abs(index - 2) * 0.045;
      light.color.setRGB(1, 0.48 + heat, 0.16 + heat * 0.35);
      light.distance = Math.max(0.1, this.settings.lightRadius * this.tile);
      light.intensity = this.settings.lights ? this.settings.lightIntensity * sourceWeights[index]! * fadeOut * flicker : 0;
      light.visible = this.settings.lights && fadeOut > 0.01;
    }
  }

  private seedFor(phase: string, index: number): number {
    const base = Math.floor((this.settings.tongueCount * 2654435761 + this.cast.id.length * 2246822519) >>> 0);
    return (base ^ (phase === "area" ? 0x9e3779b9 : 0x85ebca6b) ^ Math.imul(index + 1, 0x27d4eb2f)) >>> 0 || 1;
  }
}
