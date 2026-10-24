import * as THREE from "three";
import { pixelPreset, ProceduralElementEmitter } from "./ProceduralElementEmitter";

export interface CausticVenomCastOptions {
  id: string;
  origin: THREE.Vector3;
  target: THREE.Vector3;
  worldScale: number;
  impactHexes: THREE.Vector3[];
  onLaunch: () => void;
  onImpact: () => void;
  onComplete: () => void;
}

type Phase = "idle" | "charge" | "travel" | "impact";
type ImpactHex = {
  emitter: ProceduralElementEmitter;
  light: THREE.PointLight;
  smoke: Array<THREE.Mesh<THREE.SphereGeometry, THREE.ShaderMaterial>>;
  center: THREE.Vector3;
  seed: number;
};

const GREEN = 0x83ff22;
const DEEP_GREEN = 0x07561b;
const HIGHLIGHT = 0xd9ff83;
const POISON_V2_SCALE = 0.78;

const VENOM_SMOKE_VERTEX = /* glsl */ `
  varying vec3 vSmokePosition;
  void main() {
    vSmokePosition = position;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`;

const VENOM_SMOKE_FRAGMENT = /* glsl */ `
  uniform float uTime;
  uniform float uOpacity;
  uniform float uSeed;
  varying vec3 vSmokePosition;
  float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
  }
  float noise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash31(i), hash31(i + vec3(1.0, 0.0, 0.0)), f.x),
                   mix(hash31(i + vec3(0.0, 1.0, 0.0)), hash31(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
               mix(mix(hash31(i + vec3(0.0, 0.0, 1.0)), hash31(i + vec3(1.0, 0.0, 1.0)), f.x),
                   mix(hash31(i + vec3(0.0, 1.0, 1.0)), hash31(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);
  }
  float smokeField(vec3 p) {
    float n = noise3(p * 2.2) * 0.55;
    n += noise3(p * 4.6 + vec3(0.0, 0.0, uTime * 0.2)) * 0.3;
    n += noise3(p * 8.0 - vec3(uTime * 0.25, 0.0, 0.0)) * 0.15;
    return n;
  }
  void main() {
    // Procedural smoke from the actual 3D cloud-lobe surface; no sprite or texture sampling.
    vec3 p = vSmokePosition + vec3(uSeed * 2.7, uSeed * 0.83, uTime * 0.18);
    float field = smokeField(p);
    float radius = length(vSmokePosition);
    float raggedRadius = 0.72 + field * 0.44;
    float edge = 1.0 - smoothstep(raggedRadius, raggedRadius + 0.24, radius);
    float pockets = smoothstep(0.28, 0.62, field);
    float alpha = edge * pockets * uOpacity;
    if (alpha < 0.018) discard;
    float variation = smoothstep(0.28, 0.8, field);
    vec3 color = mix(vec3(0.04, 0.2, 0.03), vec3(0.2, 0.44, 0.075), variation);
    gl_FragColor = vec4(color, alpha);
  }
`;

/** World-space venom cast: 3D launch beam, then a full Poison V2 effect on every affected hex. */
export class CausticVenomVFX {
  private readonly root = new THREE.Group();
  private readonly charge = new THREE.Group();
  private readonly flight = new THREE.Group();
  private readonly light = new THREE.PointLight(GREEN, 0, 1, 1.8);
  private readonly chargeCore: THREE.Mesh<THREE.IcosahedronGeometry, THREE.MeshPhysicalMaterial>;
  private readonly chargeShell: THREE.Mesh<THREE.IcosahedronGeometry, THREE.MeshPhysicalMaterial>;
  private readonly head: THREE.Mesh<THREE.IcosahedronGeometry, THREE.MeshPhysicalMaterial>;
  private readonly ribbons: THREE.Mesh<THREE.TubeGeometry, THREE.MeshPhysicalMaterial>[] = [];
  private readonly impactHexes: ImpactHex[] = [];
  private readonly smokeGeometry = new THREE.SphereGeometry(1, 20, 14);
  private phase: Phase = "idle";
  private elapsed = 0;
  private travelDuration = 0;
  private worldScale = 1;
  private currentCast: CausticVenomCastOptions | null = null;
  private readonly start = new THREE.Vector3();
  private readonly target = new THREE.Vector3();
  private readonly pathStart = new THREE.Vector3();
  private readonly pathEnd = new THREE.Vector3();
  private readonly pathSide = new THREE.Vector3();
  private readonly chargeDuration = 0.38;
  private readonly impactDuration = 1.85;
  private flightArc = 0;
  private disposed = false;

  constructor(private readonly scene: THREE.Object3D) {
    const sphere = new THREE.IcosahedronGeometry(1, 3);
    this.chargeCore = new THREE.Mesh(sphere, this.makeMaterial(HIGHLIGHT, 0x69d51b, 1.9, 0.18));
    this.chargeShell = new THREE.Mesh(sphere, this.makeMaterial(GREEN, 0x37b509, 1.15, 0.12));
    this.chargeShell.scale.setScalar(1.48);
    this.head = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 3), this.makeMaterial(HIGHLIGHT, 0x60df12, 2.2, 0.12));
    this.head.castShadow = true;
    this.charge.add(this.chargeShell, this.chargeCore);
    this.root.add(this.charge, this.flight, this.light);
    this.light.castShadow = true;
    this.light.shadow.mapSize.set(512, 512);
    this.light.shadow.bias = -0.002;
    this.root.visible = false;
    this.scene.add(this.root);
  }

  cast(options: CausticVenomCastOptions): void {
    if (this.disposed) return;
    this.cancel();
    this.currentCast = options;
    this.start.copy(options.origin);
    this.target.copy(options.target);
    this.worldScale = Math.max(1, options.worldScale);
    this.travelDuration = THREE.MathUtils.clamp(this.start.distanceTo(this.target) / 235, 0.7, 1.65);
    this.elapsed = 0;
    this.phase = "charge";
    this.root.visible = true;
    this.charge.visible = true;
    this.charge.position.copy(this.start);
    this.flight.visible = false;
    this.light.visible = true;
    this.light.position.copy(this.start);
    this.setLight(this.light, 3.4, 0);
    this.buildFlightPath();
    options.onLaunch();
  }

  update(dt: number): void {
    if (this.disposed || this.phase === "idle") return;
    const step = Math.min(0.1, Math.max(0, dt));
    this.elapsed += step;
    if (this.phase === "charge") {
      const t = THREE.MathUtils.clamp(this.elapsed / this.chargeDuration, 0, 1);
      const pulse = 0.72 + Math.sin(this.elapsed * 26) * 0.12 + t * 0.28;
      this.charge.scale.setScalar(this.worldScale * (0.14 + 0.12 * t) * pulse);
      this.charge.rotation.set(this.elapsed * 2.3, this.elapsed * 1.6, this.elapsed * 2.9);
      this.light.position.copy(this.start);
      this.setLight(this.light, 3.8, 0.52 + 0.48 * t);
      if (t >= 1) {
        this.phase = "travel";
        this.elapsed = 0;
        this.flight.visible = true;
        this.charge.visible = false;
        this.head.visible = true;
        this.light.intensity = 0;
      }
      return;
    }
    if (this.phase === "travel") {
      const t = THREE.MathUtils.clamp(this.elapsed / this.travelDuration, 0, 1);
      const eased = t * t * (3 - 2 * t);
      const headPosition = this.pathPosition(eased);
      this.head.position.copy(headPosition);
      this.head.rotation.set(this.elapsed * 4.8, this.elapsed * 3.3, this.elapsed * 2.1);
      this.head.scale.set(this.worldScale * (0.22 + 0.06 * Math.sin(this.elapsed * 22)), this.worldScale * 0.17, this.worldScale * 0.14);
      this.light.position.copy(headPosition);
      this.setLight(this.light, 3.1, 0.88 + 0.12 * Math.sin(this.elapsed * 19));
      for (const ribbon of this.ribbons) {
        const max = ribbon.geometry.index?.count ?? 0;
        ribbon.geometry.setDrawRange(0, Math.floor(max * eased));
        ribbon.material.opacity = (ribbon.userData.baseOpacity as number) * (0.8 + 0.2 * Math.sin(this.elapsed * 15));
      }
      for (const bead of this.flight.children) {
        if (!bead.userData.travelBead) continue;
        const revealAt = bead.userData.revealAt as number;
        bead.visible = eased >= revealAt;
        bead.scale.setScalar(this.worldScale * (0.035 + (Math.round(revealAt * 7) % 3) * 0.009) * (0.84 + 0.16 * Math.sin(this.elapsed * 20 + revealAt * 9)));
      }
      if (t >= 1) this.beginImpact();
      return;
    }
    if (this.phase === "impact") {
      const t = THREE.MathUtils.clamp(this.elapsed / this.impactDuration, 0, 1);
      const settle = 1 - smoothstep(0.42, 1, t);
      this.updateImpactHexes(step, settle);
      if (t >= 1) {
        this.phase = "idle";
        this.root.visible = false;
        this.clearImpactHexes();
        this.currentCast?.onComplete();
        this.currentCast = null;
      }
    }
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.cancel();
    this.smokeGeometry.dispose();
    this.scene.remove(this.root);
    this.root.traverse((object) => {
      if (!(object instanceof THREE.Mesh)) return;
      object.geometry.dispose();
      const materials = Array.isArray(object.material) ? object.material : [object.material];
      for (const material of materials) material.dispose();
    });
  }

  private buildFlightPath(): void {
    for (const child of [...this.flight.children]) {
      this.flight.remove(child);
      if (child === this.head || !(child instanceof THREE.Mesh)) continue;
      child.geometry.dispose();
      const materials = Array.isArray(child.material) ? child.material : [child.material];
      for (const material of materials) material.dispose();
    }
    this.ribbons.length = 0;
    this.pathStart.copy(this.start);
    this.pathEnd.copy(this.target);
    const axis = this.pathEnd.clone().sub(this.pathStart).normalize();
    this.pathSide.set(-axis.y, axis.x, 0).normalize();
    if (this.pathSide.lengthSq() < 0.1) this.pathSide.set(0, 1, 0);
    const distance = this.start.distanceTo(this.target);
    this.flightArc = Math.max(this.worldScale * 0.38, Math.min(distance * 0.12, this.worldScale * 1.05));
    const layers = [
      { radius: this.worldScale * 0.12, turns: 2.4, phase: 0, opacity: 0.74, color: GREEN },
      { radius: this.worldScale * 0.075, turns: 3.2, phase: 2.05, opacity: 0.85, color: HIGHLIGHT },
      { radius: this.worldScale * 0.045, turns: 1.9, phase: 4.1, opacity: 0.9, color: DEEP_GREEN },
    ];
    for (const layer of layers) {
      const points: THREE.Vector3[] = [];
      for (let i = 0; i <= 72; i++) {
        const t = i / 72;
        const center = this.pathPosition(t);
        const angle = t * Math.PI * 2 * layer.turns + layer.phase;
        center.addScaledVector(this.pathSide, Math.sin(angle) * layer.radius);
        center.z += Math.cos(angle) * layer.radius * 0.85;
        points.push(center);
      }
      const geometry = new THREE.TubeGeometry(new THREE.CatmullRomCurve3(points, false, "centripetal"), 144, layer.radius * 0.52, 7, false);
      geometry.setDrawRange(0, 0);
      const material = this.makeMaterial(layer.color, 0x52d817, 1.4, layer.opacity);
      material.opacity = 0;
      const mesh = new THREE.Mesh(geometry, material);
      mesh.userData.baseOpacity = layer.opacity;
      mesh.renderOrder = 38;
      mesh.castShadow = true;
      mesh.frustumCulled = false;
      this.flight.add(mesh);
      this.ribbons.push(mesh);
    }
    this.head.position.copy(this.start);
    this.head.visible = false;
    this.head.renderOrder = 39;
    this.flight.add(this.head);
    for (let i = 0; i < 7; i++) {
      const bead = new THREE.Mesh(new THREE.SphereGeometry(1, 12, 8), this.makeMaterial(i % 2 ? GREEN : HIGHLIGHT, 0x7cf219, 1.8, 0.84));
      const revealAt = (i + 1) / 8;
      bead.position.copy(this.pathPosition(revealAt));
      bead.scale.setScalar(this.worldScale * (0.035 + (i % 3) * 0.009));
      bead.visible = false;
      bead.renderOrder = 39;
      bead.castShadow = true;
      bead.userData.travelBead = true;
      bead.userData.revealAt = revealAt;
      this.flight.add(bead);
    }
  }

  private pathPosition(t: number): THREE.Vector3 {
    const p = this.pathStart.clone().lerp(this.pathEnd, t);
    const span = this.pathStart.distanceTo(this.pathEnd);
    const side = new THREE.Vector3(-(this.pathEnd.y - this.pathStart.y), this.pathEnd.x - this.pathStart.x, 0).normalize();
    if (side.lengthSq() > 0.1) p.addScaledVector(side, Math.sin(t * Math.PI) * Math.min(this.worldScale * 0.1, span * 0.025));
    p.y += Math.sin(t * Math.PI) * this.flightArc;
    p.z += Math.sin(t * Math.PI) * this.worldScale * 0.16;
    return p;
  }

  private beginImpact(): void {
    if (!this.currentCast) return;
    const cast = this.currentCast;
    this.phase = "impact";
    this.elapsed = 0;
    this.flight.visible = false;
    for (const [index, center] of cast.impactHexes.entries()) {
      // Keep every layer of the authored V2 effect: main plume, ground layer and particle sheet.
      const emitter = new ProceduralElementEmitter(this.scene, pixelPreset("poison", "procedural_pixel_v2_poison"), {
        scale: POISON_V2_SCALE, intensity: 1.15, emissive: 1.35, opacity: 1, animationSpeed: 1.1,
        lightEnabled: false, seed: 7919 + index * 104729,
      });
      const light = new THREE.PointLight(GREEN, 0, this.worldScale * 1.8, 2);
      light.position.set(center.x, center.y, center.z + this.worldScale * 0.38);
      this.scene.add(light);
      const smoke = Array.from({ length: 6 }, (_, puffIndex) => {
        const seed = index * 7.31 + puffIndex * 2.17;
        const smokeMaterial = new THREE.ShaderMaterial({
          vertexShader: VENOM_SMOKE_VERTEX,
          fragmentShader: VENOM_SMOKE_FRAGMENT,
          transparent: true,
          depthWrite: false,
          depthTest: false,
          blending: THREE.NormalBlending,
          toneMapped: false,
          uniforms: {
            uTime: { value: 0 },
            uOpacity: { value: 0 },
            uSeed: { value: seed },
          },
        });
        const puff = new THREE.Mesh(this.smokeGeometry, smokeMaterial);
        const angle = puffIndex * Math.PI * 2 / 6 + index * 0.7;
        const radius = this.worldScale * (0.045 + (puffIndex % 2) * 0.055);
        puff.position.set(
          center.x + Math.cos(angle) * radius,
          center.y + puffIndex * this.worldScale * 0.13 + Math.sin(angle) * radius,
          center.z + this.worldScale * (0.18 + puffIndex * 0.085),
        );
        const size = this.worldScale * (puffIndex === 0 ? 0.37 : 0.275 + (puffIndex % 3) * 0.025);
        puff.scale.set(size, size * (1.1 + (puffIndex % 2) * 0.16), size * (0.72 + (puffIndex % 3) * 0.09));
        puff.rotation.set(angle * 0.45, angle * 0.3, angle);
        // Keep the authored Poison V2 sheets on top so their main body, ground layer, and motes
        // remain crisp and recognizable through the surrounding smoke.
        puff.renderOrder = -6 + puffIndex;
        puff.frustumCulled = false;
        puff.userData.seed = seed;
        this.scene.add(puff);
        return puff;
      });
      this.impactHexes.push({ emitter, light, smoke, center: center.clone(), seed: index * 2.17 });
    }
    this.light.visible = false;
    cast.onImpact();
  }

  private updateImpactHexes(dt: number, settle: number): void {
    for (const impact of this.impactHexes) {
      impact.emitter.update(dt, this.worldScale, this.elapsed, impact.center.x, -impact.center.y);
      impact.emitter.group.position.z = impact.center.z;
      const flash = 0.65 + 0.35 * Math.sin(Math.PI * Math.min(1, this.elapsed / 0.8));
      impact.light.intensity = this.worldScale * 2.4 * flash * settle;
      const age = this.elapsed / this.impactDuration;
      for (const [puffIndex, puff] of impact.smoke.entries()) {
        const seed = puff.userData.seed as number;
        const drift = age * this.worldScale * (0.12 + puffIndex * 0.018);
        puff.position.x += Math.sin(this.elapsed * 0.7 + seed) * this.worldScale * 0.0007;
        puff.position.y = impact.center.y + puffIndex * this.worldScale * 0.13 + Math.sin(seed) * this.worldScale * 0.045 + drift;
        puff.position.z = impact.center.z + this.worldScale * (0.18 + puffIndex * 0.085);
        puff.rotation.z += dt * (0.12 + puffIndex * 0.025);
        const expand = 1 + age * (0.24 + puffIndex * 0.025);
        const baseSize = this.worldScale * (puffIndex === 0 ? 0.37 : 0.275 + (puffIndex % 3) * 0.025);
        puff.scale.set(baseSize * expand, baseSize * (1.1 + (puffIndex % 2) * 0.16) * expand, baseSize * (0.72 + (puffIndex % 3) * 0.09) * expand);
        puff.material.uniforms.uTime!.value = this.elapsed;
        puff.material.uniforms.uOpacity!.value = (puffIndex === 0 ? 0.7 : 0.58) * settle;
      }
    }
  }

  private clearImpactHexes(): void {
    for (const impact of this.impactHexes) {
      impact.emitter.dispose();
      impact.light.removeFromParent();
      for (const puff of impact.smoke) {
        puff.removeFromParent();
        puff.material.dispose();
      }
    }
    this.impactHexes.length = 0;
  }

  private setLight(light: THREE.PointLight, radius: number, strength: number): void {
    light.intensity = 3.8 * this.worldScale * this.worldScale * strength;
    light.distance = this.worldScale * radius;
    light.decay = 2;
    light.shadow.camera.far = light.distance;
    light.shadow.camera.updateProjectionMatrix();
  }

  private makeMaterial(color: number, emissive: number, emissiveIntensity: number, opacity: number): THREE.MeshPhysicalMaterial {
    return new THREE.MeshPhysicalMaterial({ color, emissive, emissiveIntensity, roughness: 0.16, metalness: 0.08, clearcoat: 0.96, clearcoatRoughness: 0.1, transmission: 0.1, transparent: opacity < 0.99, opacity, depthWrite: opacity >= 0.99, side: THREE.DoubleSide });
  }

  /** Stops any cast at once and removes its impact objects (also ends the battle-start warm-up). */
  cancel(): void {
    this.phase = "idle";
    this.currentCast = null;
    this.root.visible = false;
    this.light.intensity = 0;
    this.flight.visible = false;
    this.charge.visible = false;
    this.clearImpactHexes();
  }
}

function smoothstep(a: number, b: number, x: number): number {
  const t = THREE.MathUtils.clamp((x - a) / (b - a), 0, 1);
  return t * t * (3 - 2 * t);
}
