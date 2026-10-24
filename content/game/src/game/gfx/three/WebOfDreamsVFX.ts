import * as THREE from "three";
import { WEB_SHOT_TRAVEL, type BattleEngine } from "../../engine";

export interface WebOfDreamsVfxSettings {
  radius: number;
  seedCount: number;
  filamentCount: number;
  targetFilamentCount: number;
  filamentThickness: number;
  curvature: number;
  verticalSpread: number;
  displacement: number;
  formationSpeed: number;
  pulseSpeed: number;
  pulseBrightness: number;
  nodeCount: number;
  nodeSize: number;
  tightening: number;
  centralLightIntensity: number;
  centralLightRadius: number;
  secondaryLightCount: number;
  secondaryLightIntensity: number;
  secondaryLightRadius: number;
  bindingFlashIntensity: number;
  sustainedLightIntensity: number;
  seed: number;
  geometry: boolean;
  nodes: boolean;
  particles: boolean;
  distortion: boolean;
  emissive: boolean;
  lights: boolean;
}

export const DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS: WebOfDreamsVfxSettings = {
  radius: 1,
  seedCount: 8,
  filamentCount: 24,
  targetFilamentCount: 7,
  filamentThickness: 0.027,
  curvature: 0.42,
  verticalSpread: 1.05,
  displacement: 0.055,
  formationSpeed: 1,
  pulseSpeed: 1.25,
  pulseBrightness: 2.8,
  nodeCount: 8,
  nodeSize: 0.075,
  tightening: 0.3,
  centralLightIntensity: 12,
  centralLightRadius: 5.5,
  secondaryLightCount: 2,
  secondaryLightIntensity: 6,
  secondaryLightRadius: 4.2,
  bindingFlashIntensity: 20,
  sustainedLightIntensity: 2.2,
  seed: 50827,
  geometry: true,
  nodes: true,
  particles: true,
  distortion: true,
  emissive: true,
  lights: true,
};

const SETTINGS_KEY = "emberash:web-of-dreams-v2-settings";
const FORMATION_SECONDS = 1.6;
const RELEASE_SECONDS = 0.9;
const TUBE_SIDES = 5;
const TUBE_SEGMENTS = 18;
const MAX_SECONDARY_LIGHTS = 3;

export function getActiveWebOfDreamsVfxSettings(): WebOfDreamsVfxSettings {
  if (typeof window === "undefined") return { ...DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS };
  try {
    const saved = window.localStorage.getItem(SETTINGS_KEY);
    return saved
      ? { ...DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS, ...JSON.parse(saved) as Partial<WebOfDreamsVfxSettings> }
      : { ...DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS };
  } catch {
    return { ...DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS };
  }
}

export function setActiveWebOfDreamsVfxSettings(settings: WebOfDreamsVfxSettings): void {
  if (typeof window === "undefined") return;
  try { window.localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch { /* storage may be disabled */ }
}

function seeded(seed: number): () => number {
  let state = seed >>> 0;
  return () => {
    state += 0x6d2b79f5;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

type TubeData = { points: THREE.Vector3[]; delay: number; phase: number; width: number };

function tubeBundle(strands: TubeData[], thickness: number): THREE.BufferGeometry {
  const positions: number[] = [];
  const normals: number[] = [];
  const along: number[] = [];
  const delays: number[] = [];
  const phases: number[] = [];
  const indices: number[] = [];
  const center = new THREE.Vector3();
  const radial = new THREE.Vector3();

  for (const strand of strands) {
    if (strand.points.length < 2) continue;
    const curve = new THREE.CatmullRomCurve3(strand.points, false, "centripetal", 0.35);
    const frames = curve.computeFrenetFrames(TUBE_SEGMENTS, false);
    const base = positions.length / 3;
    for (let row = 0; row <= TUBE_SEGMENTS; row++) {
      const t = row / TUBE_SEGMENTS;
      curve.getPointAt(t, center);
      const taper = thickness * strand.width * (0.4 + 0.6 * Math.sin(Math.PI * t));
      for (let side = 0; side <= TUBE_SIDES; side++) {
        const angle = (side / TUBE_SIDES) * Math.PI * 2;
        radial.copy(frames.normals[row]!).multiplyScalar(Math.cos(angle))
          .addScaledVector(frames.binormals[row]!, Math.sin(angle));
        positions.push(center.x + radial.x * taper, center.y + radial.y * taper, center.z + radial.z * taper);
        normals.push(radial.x, radial.y, radial.z);
        along.push(t);
        delays.push(strand.delay);
        phases.push(strand.phase);
        if (row < TUBE_SEGMENTS && side < TUBE_SIDES) {
          const a = base + row * (TUBE_SIDES + 1) + side;
          const b = a + TUBE_SIDES + 1;
          indices.push(a, b, a + 1, b, b + 1, a + 1);
        }
      }
    }
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
  geometry.setAttribute("normal", new THREE.Float32BufferAttribute(normals, 3));
  geometry.setAttribute("aAlong", new THREE.Float32BufferAttribute(along, 1));
  geometry.setAttribute("aDelay", new THREE.Float32BufferAttribute(delays, 1));
  geometry.setAttribute("aPhase", new THREE.Float32BufferAttribute(phases, 1));
  geometry.setIndex(indices);
  geometry.computeBoundingSphere();
  return geometry;
}

const THREAD_VERTEX = /* glsl */ `
  uniform float uTime;
  uniform float uPulseSpeed;
  uniform float uFormation;
  uniform float uTightening;
  uniform float uDisplacement;
  attribute float aAlong;
  attribute float aDelay;
  attribute float aPhase;
  varying float vAlong;
  varying float vVisible;
  varying float vPulse;
  varying vec3 vNormal;
  varying vec3 vViewDirection;
  void main() {
    vAlong = aAlong;
    float grown = smoothstep(aDelay, aDelay + 0.18, uFormation - aAlong * 0.2);
    vVisible = grown;
    float wave = sin(uTime * 2.7 + aAlong * 17.0 + aPhase) * uDisplacement;
    float wave2 = cos(uTime * 1.9 - aAlong * 12.0 + aPhase * 1.7) * uDisplacement;
    vec3 p = position;
    p.x += wave * (0.25 + aAlong);
    p.y += wave2 * (0.35 + aAlong);
    p.z += sin(uTime * 2.1 + aAlong * 9.0 + aPhase) * uDisplacement * 1.6;
    p.xy *= mix(1.0, 1.0 - uTightening, smoothstep(0.68, 0.92, uFormation));
    float pulse = fract(uTime * uPulseSpeed * 0.27 + aAlong + aPhase);
    vPulse = smoothstep(0.0, 0.07, pulse) * (1.0 - smoothstep(0.11, 0.19, pulse));
    vec4 viewPosition = modelViewMatrix * vec4(p, 1.0);
    vNormal = normalize(normalMatrix * normal);
    vViewDirection = normalize(-viewPosition.xyz);
    gl_Position = projectionMatrix * viewPosition;
  }
`;

const THREAD_FRAGMENT = /* glsl */ `
  uniform float uEnergy;
  uniform float uOpacity;
  varying float vAlong;
  varying float vVisible;
  varying float vPulse;
  varying vec3 vNormal;
  varying vec3 vViewDirection;
  void main() {
    vec3 indigo = vec3(0.12, 0.075, 0.68);
    vec3 violet = vec3(0.42, 0.18, 0.94);
    vec3 lavender = vec3(0.78, 0.66, 1.0);
    vec3 cyanLavender = vec3(0.57, 0.78, 1.0);
    vec3 color = mix(indigo, violet, smoothstep(0.02, 0.72, vAlong));
    color = mix(color, lavender, clamp(vPulse * 0.92 + smoothstep(0.76, 1.0, vAlong) * 0.16, 0.0, 1.0));
    color = mix(color, cyanLavender, vPulse * 0.12);
    float bevel = 0.42 + 0.58 * abs(dot(normalize(vNormal), normalize(vec3(-0.36, 0.48, 0.8))));
    float rim = pow(1.0 - abs(dot(normalize(vNormal), normalize(vViewDirection))), 2.0);
    color = color * bevel + lavender * rim * 0.62;
    float alpha = uOpacity * vVisible * (0.48 + vPulse * 0.5);
    gl_FragColor = vec4(color * uEnergy * (0.72 + vPulse * 1.6), alpha);
  }
`;

export type WebTarget = { id: string; position: THREE.Vector3 };

class WebZoneVFX {
  readonly group = new THREE.Group();
  private readonly webMaterial: THREE.ShaderMaterial;
  private readonly targetMaterial: THREE.ShaderMaterial;
  private readonly nodeGeometry = new THREE.IcosahedronGeometry(0.5, 1);
  private readonly nodeMaterial: THREE.MeshStandardMaterial;
  private readonly nodeMesh: THREE.InstancedMesh;
  private readonly particleGeometry = new THREE.TetrahedronGeometry(0.5, 0);
  private readonly particleMaterial: THREE.MeshStandardMaterial;
  private readonly particleMesh: THREE.InstancedMesh;
  private readonly centralLight = new THREE.PointLight(0x8667ff, 0, 1, 2);
  private readonly secondaryLights: THREE.PointLight[] = [];
  private readonly dummy = new THREE.Object3D();
  private readonly phase: number[] = [];
  private readonly nodePositions: THREE.Vector3[] = [];
  private readonly strandsByUnit = new Map<string, THREE.Mesh<THREE.BufferGeometry, THREE.ShaderMaterial>>();
  private netMesh: THREE.Mesh<THREE.BufferGeometry, THREE.ShaderMaterial>;
  private elapsed: number;
  private releaseElapsed = 0;
  private releasing = false;
  private disposed = false;
  private readonly settings: WebOfDreamsVfxSettings;

  constructor(
    private readonly scene: THREE.Object3D,
    private readonly worldCenter: THREE.Vector3,
    private tile: number,
    radius: number,
    settings: WebOfDreamsVfxSettings,
    age = 0,
  ) {
    this.settings = { ...settings };
    this.elapsed = Math.max(0, age);
    const random = seeded(settings.seed + Math.round(worldCenter.x * 11) + Math.round(worldCenter.y * 7));
    const extent = Math.max(0.7, radius * 0.95) * settings.radius;
    const count = Math.max(4, Math.min(12, Math.round(settings.seedCount)));
    for (let i = 0; i < count; i++) {
      // Place the anchor knots around the perimeter so the spell reads as a net/web,
      // rather than a loose cluster of unrelated wisps.
      const angle = (i / count) * Math.PI * 2 + (random() - 0.5) * 0.12;
      const r = extent * (0.9 + random() * 0.08);
      this.nodePositions.push(new THREE.Vector3(
        Math.cos(angle) * r,
        Math.sin(angle) * r * 0.72,
        (random() - 0.5) * settings.verticalSpread * 1.45,
      ));
      this.phase.push(random() * Math.PI * 2);
    }
    this.group.position.copy(worldCenter);
    this.group.scale.setScalar(tile);

    const uniforms = {
      uTime: { value: 0 }, uPulseSpeed: { value: settings.pulseSpeed }, uFormation: { value: Math.min(2, this.elapsed) }, uTightening: { value: settings.tightening },
      uDisplacement: { value: settings.displacement }, uEnergy: { value: settings.emissive ? 1 : 0.42 }, uOpacity: { value: 0.94 },
    };
    this.webMaterial = new THREE.ShaderMaterial({
      uniforms,
      vertexShader: THREAD_VERTEX,
      fragmentShader: THREAD_FRAGMENT,
      transparent: true,
      depthTest: true,
      depthWrite: false,
      side: THREE.DoubleSide,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
    });
    this.targetMaterial = new THREE.ShaderMaterial({
      uniforms: {
        uTime: uniforms.uTime, uPulseSpeed: uniforms.uPulseSpeed, uFormation: uniforms.uFormation, uTightening: uniforms.uTightening,
        uDisplacement: uniforms.uDisplacement, uEnergy: uniforms.uEnergy, uOpacity: uniforms.uOpacity,
      },
      vertexShader: THREAD_VERTEX,
      fragmentShader: THREAD_FRAGMENT,
      transparent: true,
      depthTest: true,
      depthWrite: false,
      side: THREE.DoubleSide,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
    });
    const network: TubeData[] = [];
    const filamentCount = Math.max(6, Math.min(40, Math.round(settings.filamentCount)));
    // Connect the scattered 3D dream seeds as an irregular lattice. Neighbor links
    // establish the web silhouette; skip links create cross-bracing, while raised and
    // recessed control points make the crossings occupy different depth planes.
    const addThread = (from: number, to: number, delay: number, width: number) => {
      const start = this.nodePositions[from]!.clone();
      const end = this.nodePositions[to]!.clone();
      const midpoint = start.clone().lerp(end, 0.5);
      const bend = new THREE.Vector3(
        (random() - 0.5) * settings.curvature * 0.8,
        (random() - 0.5) * settings.curvature * 0.8,
        (random() < 0.5 ? -1 : 1) * (0.16 + random() * 0.44) * settings.verticalSpread,
      );
      const first = start.clone().lerp(midpoint, 0.58).addScaledVector(bend, 0.55);
      const second = midpoint.clone().lerp(end, 0.42).add(bend);
      network.push({
        points: [start, first, midpoint.clone().add(bend), second, end],
        delay: delay + random() * 0.22,
        phase: random() * 6.28,
        width: width * (0.82 + random() * 0.34),
      });
    };
    const seedCount = this.nodePositions.length;
    const usedEdges = new Set<string>();
    const connect = (from: number, to: number, delay: number, width: number) => {
      const key = `${Math.min(from, to)}:${Math.max(from, to)}`;
      if (from === to || usedEdges.has(key)) return;
      usedEdges.add(key);
      addThread(from, to, delay, width);
    };
    for (let i = 0; i < seedCount; i++) connect(i, (i + 1) % seedCount, 0.12, 0.82);
    for (let attempt = 0; network.length < filamentCount && attempt < 180; attempt++) {
      const from = Math.floor(random() * seedCount);
      const offset = 2 + Math.floor(random() * Math.max(1, seedCount - 3));
      connect(from, (from + offset) % seedCount, 0.22 + random() * 0.2, 0.62 + random() * 0.18);
    }
    // Thin, incomplete tendrils reach beyond a few seeds and fade into empty space.
    for (let i = 0; i < Math.min(5, Math.floor(filamentCount / 5)); i++) {
      const start = this.nodePositions[(i * 3 + 1) % seedCount]!.clone();
      const angle = Math.atan2(start.y, start.x) + (random() - 0.5) * 0.8;
      const end = start.clone().add(new THREE.Vector3(Math.cos(angle), Math.sin(angle) * 0.72, (random() - 0.5) * settings.verticalSpread).multiplyScalar(extent * (0.16 + random() * 0.16)));
      const mid = start.clone().lerp(end, 0.54).add(new THREE.Vector3((random() - 0.5) * settings.curvature, (random() - 0.5) * settings.curvature, (random() - 0.5) * settings.verticalSpread));
      network.push({ points: [start, start.clone().lerp(mid, 0.62), mid, end], delay: 0.56 + random() * 0.2, phase: random() * 6.28, width: 0.48 + random() * 0.28 });
    }
    this.netMesh = new THREE.Mesh(tubeBundle(network, settings.filamentThickness), this.webMaterial);
    this.netMesh.renderOrder = 3;
    this.netMesh.frustumCulled = false;
    this.group.add(this.netMesh);

    this.nodeMaterial = new THREE.MeshStandardMaterial({
      color: 0xb8a2ff,
      emissive: 0x7959ff,
      emissiveIntensity: settings.emissive ? 2.2 : 0.08,
      roughness: 0.25,
      metalness: 0.08,
      transparent: true,
      opacity: 0.92,
    });
    const nodeCount = Math.max(2, Math.min(this.nodePositions.length, Math.round(settings.nodeCount)));
    this.nodeMesh = new THREE.InstancedMesh(this.nodeGeometry, this.nodeMaterial, nodeCount);
    this.nodeMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.nodeMesh.frustumCulled = false;
    this.nodeMesh.renderOrder = 3;
    this.group.add(this.nodeMesh);

    const fragmentCount = Math.max(0, Math.min(120, Math.round(settings.seedCount * 4)));
    this.particleMaterial = new THREE.MeshStandardMaterial({ color: 0xb9a5ff, emissive: 0x8061ff, emissiveIntensity: settings.emissive ? 1.6 : 0.04, roughness: 0.38, metalness: 0.05, transparent: true, depthWrite: false });
    this.particleMesh = new THREE.InstancedMesh(this.particleGeometry, this.particleMaterial, Math.max(1, fragmentCount));
    this.particleMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.particleMesh.frustumCulled = false;
    this.particleMesh.renderOrder = 3;
    this.group.add(this.particleMesh);

    this.centralLight.castShadow = true;
    this.centralLight.shadow.mapSize.set(256, 256);
    this.centralLight.shadow.camera.near = 0.05;
    this.centralLight.shadow.bias = -0.001;
    this.scene.add(this.centralLight);
    for (let i = 0; i < MAX_SECONDARY_LIGHTS; i++) {
      const light = new THREE.PointLight(0x9276ff, 0, 1, 2);
      light.visible = i < settings.secondaryLightCount;
      this.secondaryLights.push(light);
      this.scene.add(light);
    }
    this.scene.add(this.group);
    this.update(0, []);
  }

  update(dt: number, targets: WebTarget[]): void {
    if (this.disposed) return;
    if (this.releasing) this.releaseElapsed += Math.min(0.05, Math.max(0, dt));
    else this.elapsed += Math.min(0.05, Math.max(0, dt)) * this.settings.formationSpeed;
    const release = this.releasing ? THREE.MathUtils.smoothstep(this.releaseElapsed, 0, RELEASE_SECONDS) : 0;
    const formation = Math.min(2, this.elapsed / FORMATION_SECONDS);
    const bind = Math.exp(-Math.pow((this.elapsed - 1.42) / 0.13, 2));
    const sustain = this.elapsed > FORMATION_SECONDS ? 1 : 0;
    const liveOpacity = this.releasing ? 1 - release : 1;
    this.webMaterial.uniforms.uTime!.value = this.elapsed;
    this.webMaterial.uniforms.uPulseSpeed!.value = this.settings.pulseSpeed;
    this.webMaterial.uniforms.uFormation!.value = formation;
    this.webMaterial.uniforms.uTightening!.value = this.settings.tightening * THREE.MathUtils.smoothstep(this.elapsed, 1.04, 1.48);
    this.webMaterial.uniforms.uDisplacement!.value = this.settings.distortion ? this.settings.displacement : 0;
    this.webMaterial.uniforms.uEnergy!.value = (this.settings.emissive ? 0.9 : 0.18) * (1 + bind * this.settings.pulseBrightness * 0.42);
    this.webMaterial.uniforms.uOpacity!.value = 0.9 * liveOpacity;
    this.targetMaterial.uniforms.uTime!.value = this.elapsed;
    this.targetMaterial.uniforms.uPulseSpeed!.value = this.settings.pulseSpeed;
    this.targetMaterial.uniforms.uFormation!.value = formation;
    this.targetMaterial.uniforms.uTightening!.value = this.settings.tightening * THREE.MathUtils.smoothstep(this.elapsed, 1.04, 1.48);
    this.targetMaterial.uniforms.uDisplacement!.value = this.settings.distortion ? this.settings.displacement : 0;
    this.targetMaterial.uniforms.uEnergy!.value = (this.settings.emissive ? 1.1 : 0.18) * (1 + bind * this.settings.pulseBrightness * 0.5);
    this.targetMaterial.uniforms.uOpacity!.value = 0.98 * liveOpacity;
    this.netMesh.visible = this.settings.geometry && liveOpacity > 0.01;
    this.nodeMesh.visible = this.settings.nodes && liveOpacity > 0.01;
    this.particleMesh.visible = this.settings.particles && liveOpacity > 0.01;
    this.nodeMaterial.opacity = 0.92 * liveOpacity;
    this.particleMaterial.opacity = liveOpacity;

    this.updateNodes();
    this.updateParticles();
    this.updateTargets(targets, liveOpacity);

    const lightOn = this.settings.lights && liveOpacity > 0.01;
    this.centralLight.visible = lightOn;
    const formationLight = this.settings.centralLightIntensity * THREE.MathUtils.smoothstep(this.elapsed, 0.02, 0.54);
    const bindingFlash = bind * this.settings.bindingFlashIntensity;
    const sustained = sustain ? this.settings.sustainedLightIntensity : 0;
    this.centralLight.intensity = lightOn ? (formationLight + bindingFlash + sustained) * liveOpacity : 0;
    this.centralLight.distance = this.tile * this.settings.centralLightRadius;
    this.centralLight.position.set(this.worldCenter.x, this.worldCenter.y, this.worldCenter.z + this.tile * 0.12);
    this.centralLight.shadow.camera.far = this.centralLight.distance;
    this.centralLight.shadow.camera.updateProjectionMatrix();
    for (let i = 0; i < this.secondaryLights.length; i++) {
      const light = this.secondaryLights[i]!;
      const enabled = lightOn && i < this.settings.secondaryLightCount;
      light.visible = enabled;
      const node = this.nodePositions[(i * 3 + 1) % this.nodePositions.length]!;
      light.position.set(
        this.worldCenter.x + node.x * this.tile,
        this.worldCenter.y + node.y * this.tile,
        this.worldCenter.z + (node.z + 0.16) * this.tile,
      );
      const travelPulse = 0.5 + 0.5 * Math.sin(this.elapsed * this.settings.pulseSpeed + this.phase[(i * 3 + 1) % this.phase.length]!);
      light.intensity = enabled ? (this.settings.secondaryLightIntensity * (0.35 + travelPulse * 0.65) + bind * this.settings.bindingFlashIntensity * 0.35 + sustained * 0.28) * liveOpacity : 0;
      light.distance = this.tile * this.settings.secondaryLightRadius;
    }
  }

  setAnchor(center: THREE.Vector3, tile: number): void {
    this.worldCenter.copy(center);
    this.tile = tile;
    this.group.position.copy(center);
    this.group.scale.setScalar(tile);
  }

  beginRelease(): void {
    if (this.releasing) return;
    this.releasing = true;
    this.releaseElapsed = 0;
  }

  get finished(): boolean { return this.releasing && this.releaseElapsed >= RELEASE_SECONDS; }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.scene.remove(this.group);
    this.scene.remove(this.centralLight);
    for (const light of this.secondaryLights) this.scene.remove(light);
    this.group.clear();
    this.netMesh.geometry.dispose();
    for (const mesh of this.strandsByUnit.values()) mesh.geometry.dispose();
    this.strandsByUnit.clear();
    this.webMaterial.dispose();
    this.targetMaterial.dispose();
    this.nodeGeometry.dispose();
    this.nodeMaterial.dispose();
    this.nodeMesh.dispose();
    this.particleGeometry.dispose();
    this.particleMaterial.dispose();
    this.particleMesh.dispose();
  }

  private updateNodes(): void {
    const count = this.nodeMesh.count;
    for (let i = 0; i < count; i++) {
      const position = this.nodePositions[i % this.nodePositions.length]!;
      const pulse = 1 + 0.18 * Math.sin(this.elapsed * 2.2 + this.phase[i]!);
      const bind = 1 - this.settings.tightening * 0.24 * THREE.MathUtils.smoothstep(this.elapsed, 1.08, 1.5);
      const size = this.settings.nodeSize * pulse * bind;
      this.dummy.position.set(position.x, position.y, position.z + Math.sin(this.elapsed * 1.6 + this.phase[i]!) * 0.035);
      this.dummy.rotation.set(this.elapsed * 0.24 + this.phase[i]!, this.elapsed * 0.31, this.phase[i]! * 0.4);
      this.dummy.scale.set(size * (1 + Math.sin(this.elapsed * 1.3 + i) * 0.14), size, size * 0.74);
      this.dummy.updateMatrix();
      this.nodeMesh.setMatrixAt(i, this.dummy.matrix);
    }
    this.nodeMesh.instanceMatrix.needsUpdate = true;
    this.nodeMaterial.emissiveIntensity = this.settings.emissive ? 1.8 + Math.exp(-Math.pow((this.elapsed - 1.42) / 0.14, 2)) * 4 : 0.02;
  }

  private updateParticles(): void {
    for (let i = 0; i < this.particleMesh.count; i++) {
      const phase = this.phase[i % this.phase.length]! + i * 0.31;
      const radius = 0.28 + ((i * 37) % 100) / 100 * 0.72;
      const angle = phase + this.elapsed * (0.16 + (i % 5) * 0.035);
      const pull = THREE.MathUtils.smoothstep(this.elapsed, 0.8, 1.45) * 0.58;
      const x = Math.cos(angle) * radius * (1 - pull);
      const y = Math.sin(angle * 1.17) * radius * 0.75 * (1 - pull);
      const z = Math.sin(angle * 0.73 + phase) * this.settings.verticalSpread * 0.45;
      const size = (0.018 + (i % 4) * 0.006) * (0.7 + 0.3 * Math.sin(this.elapsed * 3 + phase));
      this.dummy.position.set(x, y, z);
      this.dummy.rotation.set(angle, angle * 0.7, phase);
      this.dummy.scale.setScalar(size);
      this.dummy.updateMatrix();
      this.particleMesh.setMatrixAt(i, this.dummy.matrix);
    }
    this.particleMesh.instanceMatrix.needsUpdate = true;
    this.particleMaterial.emissiveIntensity = this.settings.emissive ? 1.25 : 0;
  }

  private updateTargets(targets: WebTarget[], liveOpacity: number): void {
    const seen = new Set<string>();
    for (const target of targets) {
      seen.add(target.id);
      let mesh = this.strandsByUnit.get(target.id);
      if (!mesh) {
        const random = seeded(this.settings.seed + target.id.split("").reduce((sum, char) => sum + char.charCodeAt(0), 0));
        const strands: TubeData[] = [];
        const wraps = Math.min(3, Math.max(1, Math.round(this.settings.targetFilamentCount / 3)));
        for (let wrap = 0; wrap < wraps; wrap++) {
          const points: THREE.Vector3[] = [];
          const offset = (wrap / wraps) * Math.PI * 2 + random() * 0.28;
          for (let step = 0; step <= 12; step++) {
            const t = step / 12;
            const angle = offset + t * Math.PI * 2 * 1.15;
            const radius = 0.3 + Math.sin(t * Math.PI) * 0.07;
            points.push(new THREE.Vector3(
              Math.cos(angle) * radius,
              -0.5 + t * 1.28,
              Math.sin(angle) * 0.22 + (random() - 0.5) * 0.04,
            ));
          }
          strands.push({ points, delay: 0.12 + wrap * 0.08, phase: random() * 6.28, width: 1.25 });
        }
        const count = Math.max(3, Math.min(12, Math.round(this.settings.targetFilamentCount)));
        for (let i = 0; i < count; i++) {
          const angle = (i / count) * Math.PI * 2 + random() * 0.24;
          const radius = 0.22 + random() * 0.18;
          const start = new THREE.Vector3(Math.cos(angle) * radius, -0.38 + random() * 0.2, (random() - 0.5) * 0.32);
          const over = new THREE.Vector3(Math.cos(angle + 1.15) * radius * 0.6, 0.12 + random() * 0.58, (random() - 0.5) * 0.52);
          const far = new THREE.Vector3(Math.cos(angle + 2.55) * radius, -0.12 + random() * 0.68, (random() - 0.5) * 0.46);
          const end = new THREE.Vector3(Math.cos(angle + Math.PI) * radius * 0.82, -0.42 + random() * 0.9, (random() - 0.5) * 0.36);
          strands.push({ points: [start, start.clone().lerp(over, 0.6), over, far, end], delay: 0.52 + random() * 0.42, phase: random() * 6.28, width: 0.72 + random() * 0.62 });
        }
        mesh = new THREE.Mesh(tubeBundle(strands, this.settings.filamentThickness * 1.2), this.targetMaterial);
        mesh.renderOrder = 4;
        mesh.frustumCulled = false;
        this.strandsByUnit.set(target.id, mesh);
        this.group.add(mesh);
      }
      mesh.position.copy(target.position).sub(this.worldCenter);
      mesh.position.multiplyScalar(1 / this.tile);
      mesh.visible = this.settings.geometry && liveOpacity > 0.01;
    }
    for (const [id, mesh] of this.strandsByUnit) {
      if (seen.has(id)) continue;
      this.group.remove(mesh);
      mesh.geometry.dispose();
      this.strandsByUnit.delete(id);
    }
  }
}

export class WebOfDreamsVFX {
  private fields = new Map<object, WebZoneVFX>();
  private retiring: WebZoneVFX[] = [];
  private previewField: WebZoneVFX | null = null;
  private previewComplete = false;
  private settings: WebOfDreamsVfxSettings = getActiveWebOfDreamsVfxSettings();

  constructor(private readonly scene: THREE.Object3D) {}

  setSettings(settings: WebOfDreamsVfxSettings): void {
    this.settings = { ...settings };
    this.resetPreview();
  }

  previewAt(center: THREE.Vector3, tile: number, radius: number, targets: WebTarget[], dt: number): void {
    if (this.previewComplete) return;
    if (!this.previewField) this.previewField = new WebZoneVFX(this.scene, center, tile, radius, this.settings);
    this.previewField.update(dt, targets);
    if (this.previewField.finished) {
      this.previewField.dispose();
      this.previewField = null;
      this.previewComplete = true;
    }
  }

  releasePreview(): void {
    this.previewField?.beginRelease();
  }

  resetPreview(): void {
    this.previewField?.dispose();
    this.previewField = null;
    this.previewComplete = false;
  }

  update(engine: BattleEngine, tile: number, dt: number): void {
    const active = new Set<object>();
    for (const zone of engine.webZones) {
      if (!zone.center) continue;
      const ageSinceCast = zone.createdAt == null ? FORMATION_SECONDS : Math.max(0, engine.time - zone.createdAt);
      if (ageSinceCast < WEB_SHOT_TRAVEL) continue;
      active.add(zone);
      let field = this.fields.get(zone);
      if (!field) {
        const anchor = engine.effectAnchor(zone.center.x, zone.center.y);
        const center = new THREE.Vector3(anchor.worldX, -anchor.worldY, spriteDepthZ(anchor.worldY, tile));
        const radius = engine.webZoneRadiusTiles(zone.radius ?? 1);
        const age = zone.createdAt == null ? FORMATION_SECONDS : Math.max(0, ageSinceCast - WEB_SHOT_TRAVEL);
        field = new WebZoneVFX(this.scene, center, tile, radius, this.settings, age);
        this.fields.set(zone, field);
      }
      const anchor = engine.effectAnchor(zone.center.x, zone.center.y);
      field.setAnchor(new THREE.Vector3(anchor.worldX, -anchor.worldY, spriteDepthZ(anchor.worldY, tile)), tile);
      const targets: WebTarget[] = [];
      for (const unit of engine.units) {
        if (!unit.alive || !zone.cells.has(`${unit.x},${unit.y}`)) continue;
        const visual = engine.unitVisual(unit, tile);
        const anchor = engine.unitAnchor(unit);
        const groundY = anchor.worldY + visual.footY;
        const centerLocalY = (visual.footOffset - visual.h * 0.46) * visual.scaleY;
        targets.push({
          id: unit.id,
          position: new THREE.Vector3(
            anchor.worldX + visual.sway,
            -(groundY + visual.bob - visual.lift + centerLocalY),
            spriteDepthZ(groundY, tile) + UNIT_DEPTH_TIE,
          ),
        });
      }
      field.update(dt, targets);
    }
    for (const [zone, field] of this.fields) {
      if (active.has(zone)) continue;
      field.beginRelease();
      field.update(dt, []);
      if (field.finished) {
        field.dispose();
        this.fields.delete(zone);
      }
    }
    for (let i = this.retiring.length - 1; i >= 0; i--) {
      const field = this.retiring[i]!;
      field.update(dt, []);
      if (field.finished) {
        field.dispose();
        this.retiring.splice(i, 1);
      }
    }
  }

  dispose(): void {
    this.resetPreview();
    for (const field of this.fields.values()) field.dispose();
    for (const field of this.retiring) field.dispose();
    this.fields.clear();
    this.retiring = [];
  }
}

const UNIT_DEPTH_TIE = 0.015;
function spriteDepthZ(groundWy: number, tile: number): number {
  return 0.82 + groundWy * 0.00012 + tile * 0.0001;
}
