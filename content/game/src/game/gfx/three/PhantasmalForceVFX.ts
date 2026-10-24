import * as THREE from "three";

export interface PhantasmalForceSettings {
  radius: number;
  tendrilCount: number;
  tendrilThickness: number;
  tendrilTurbulence: number;
  splineNoise: number;
  particleCount: number;
  particleAttraction: number;
  spiralStrength: number;
  compressionDuration: number;
  coreSize: number;
  coreEmission: number;
  buildupLight: number;
  impactLight: number;
  lightRadius: number;
  shellRadius: number;
  shellSpeed: number;
  distortionStrength: number;
  residualLifetime: number;
  seed: number;
}

export const DEFAULT_PHANTASMAL_FORCE_SETTINGS: PhantasmalForceSettings = {
  radius: 1.8,
  tendrilCount: 8,
  tendrilThickness: 0.035,
  tendrilTurbulence: 0.18,
  splineNoise: 0.22,
  particleCount: 260,
  particleAttraction: 2.3,
  spiralStrength: 1.35,
  compressionDuration: 0.2,
  coreSize: 0.12,
  coreEmission: 7,
  buildupLight: 2.4,
  impactLight: 22,
  lightRadius: 4.8,
  shellRadius: 2,
  shellSpeed: 10,
  distortionStrength: 0.16,
  residualLifetime: 0.5,
  seed: 42017,
};

const IMPACT_AT = 0.82;
const END_AT = 1.35;
const TENDRIL_VERTS = 18;
const TENDRIL_SIDES = 6;
const SETTINGS_KEY = "emberash:phantasmal-force-settings";

export function getActivePhantasmalForceSettings(): PhantasmalForceSettings {
  if (typeof window === "undefined") return { ...DEFAULT_PHANTASMAL_FORCE_SETTINGS };
  try {
    const saved = window.localStorage.getItem(SETTINGS_KEY);
    return saved ? { ...DEFAULT_PHANTASMAL_FORCE_SETTINGS, ...JSON.parse(saved) as Partial<PhantasmalForceSettings> } : { ...DEFAULT_PHANTASMAL_FORCE_SETTINGS };
  } catch {
    return { ...DEFAULT_PHANTASMAL_FORCE_SETTINGS };
  }
}

export function setActivePhantasmalForceSettings(settings: PhantasmalForceSettings): void {
  if (typeof window === "undefined") return;
  try { window.localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch { /* private browsing may deny persistence */ }
}

function mulberry(seed: number): () => number {
  let state = seed >>> 0;
  return () => {
    state += 0x6d2b79f5;
    let value = state;
    value = Math.imul(value ^ (value >>> 15), value | 1);
    value ^= value + Math.imul(value ^ (value >>> 7), value | 61);
    return ((value ^ (value >>> 14)) >>> 0) / 4294967296;
  };
}

function tubeGeometry(seed: number, thickness: number): THREE.BufferGeometry {
  const random = mulberry(seed);
  const start = new THREE.Vector3(
    (random() * 2 - 1) * 0.8,
    (random() * 2 - 1) * 0.8,
    (random() * 2 - 1) * 0.8,
  );
  const middle = new THREE.Vector3(
    (random() * 2 - 1) * 0.65,
    (random() * 2 - 1) * 0.65,
    (random() * 2 - 1) * 0.65,
  );
  const points = [
    start.clone().multiplyScalar(1.05),
    start.clone().lerp(middle, 0.38),
    middle,
    middle.clone().multiplyScalar(0.48),
    new THREE.Vector3(0, 0, 0),
  ];
  const curve = new THREE.CatmullRomCurve3(points);
  const frames = curve.computeFrenetFrames(TENDRIL_VERTS, false);
  const positions: number[] = [];
  const normals: number[] = [];
  const along: number[] = [];
  const indices: number[] = [];
  const center = new THREE.Vector3();
  const radial = new THREE.Vector3();
  for (let row = 0; row <= TENDRIL_VERTS; row++) {
    const t = row / TENDRIL_VERTS;
    curve.getPointAt(t, center);
    const taper = thickness * (0.13 + 0.87 * Math.pow(1 - t, 0.72));
    for (let side = 0; side <= TENDRIL_SIDES; side++) {
      const angle = (side / TENDRIL_SIDES) * Math.PI * 2;
      radial.copy(frames.normals[row]!).multiplyScalar(Math.cos(angle))
        .addScaledVector(frames.binormals[row]!, Math.sin(angle));
      positions.push(center.x + radial.x * taper, center.y + radial.y * taper, center.z + radial.z * taper);
      normals.push(radial.x, radial.y, radial.z);
      along.push(t);
      if (row < TENDRIL_VERTS && side < TENDRIL_SIDES) {
        const a = row * (TENDRIL_SIDES + 1) + side;
        const b = a + TENDRIL_SIDES + 1;
        indices.push(a, b, a + 1, b, b + 1, a + 1);
      }
    }
  }
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
  geometry.setAttribute("normal", new THREE.Float32BufferAttribute(normals, 3));
  geometry.setAttribute("aAlong", new THREE.Float32BufferAttribute(along, 1));
  geometry.setIndex(indices);
  geometry.computeBoundingSphere();
  return geometry;
}

const TENDRIL_VERTEX = /* glsl */ `
  uniform float uTime;
  uniform float uCollapse;
  uniform float uTurbulence;
  uniform float uSplineNoise;
  attribute float aAlong;
  varying float vAlong;
  varying float vNoise;
  void main() {
    vAlong = aAlong;
    float wave = sin(uTime * 3.4 + aAlong * 18.0 + position.z * 4.0);
    float wave2 = cos(uTime * 2.1 - aAlong * 13.0 + position.x * 5.0);
    vNoise = wave * 0.5 + wave2 * 0.5;
    vec3 p = position;
    p.xy += vec2(wave, wave2) * uTurbulence * (0.25 + aAlong);
    p.xz += vec2(wave2, wave) * uSplineNoise * sin(aAlong * 3.14159);
    p *= mix(1.0, 0.07, uCollapse);
    gl_Position = projectionMatrix * modelViewMatrix * vec4(p, 1.0);
  }
`;

const TENDRIL_FRAGMENT = /* glsl */ `
  uniform float uTime;
  uniform float uOpacity;
  varying float vAlong;
  varying float vNoise;
  void main() {
    float pulse = smoothstep(0.0, 0.22, fract(uTime * 0.9 + vAlong));
    pulse *= 1.0 - smoothstep(0.28, 0.55, fract(uTime * 0.9 + vAlong));
    vec3 indigo = vec3(0.12, 0.08, 0.72);
    vec3 violet = vec3(0.42, 0.22, 1.0);
    vec3 lavender = vec3(0.77, 0.63, 1.0);
    vec3 color = mix(indigo, violet, smoothstep(0.0, 0.65, vAlong));
    color = mix(color, lavender, clamp(pulse * 0.82 + smoothstep(0.78, 1.0, vAlong) * 0.35, 0.0, 1.0));
    float flicker = 0.72 + 0.28 * sin(uTime * 11.0 + vNoise * 4.0);
    float alpha = uOpacity * flicker * (0.48 + pulse * 0.52);
    gl_FragColor = vec4(color * (1.5 + pulse * 2.5), alpha);
  }
`;

const CORE_VERTEX = /* glsl */ `
  uniform float uTime;
  uniform float uDistortion;
  varying vec3 vP;
  void main() {
    vec3 p = position;
    float n = sin(p.x * 18.0 + uTime * 9.0) * cos(p.y * 15.0 - uTime * 7.0) * sin(p.z * 17.0 + uTime * 5.0);
    p += normal * n * uDistortion;
    vP = p;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(p, 1.0);
  }
`;

const CORE_FRAGMENT = /* glsl */ `
  uniform float uTime;
  uniform float uEmission;
  varying vec3 vP;
  void main() {
    float turbulence = 0.5 + 0.5 * sin(vP.x * 24.0 + uTime * 11.0) * cos(vP.y * 22.0 - uTime * 8.0) * sin(vP.z * 25.0 + uTime * 6.0);
    vec3 violet = vec3(0.33, 0.13, 1.0);
    vec3 lavender = vec3(0.76, 0.61, 1.0);
    vec3 white = vec3(1.0, 0.96, 1.0);
    vec3 color = mix(violet, lavender, turbulence);
    color = mix(color, white, smoothstep(0.75, 1.0, turbulence));
    gl_FragColor = vec4(color * uEmission, 0.98);
  }
`;

const SHELL_VERTEX = /* glsl */ `
  uniform float uTime;
  uniform float uDistortion;
  varying vec3 vNormal;
  varying float vWarp;
  void main() {
    vec3 p = position;
    float n = sin(p.x * 9.0 + uTime * 13.0) * cos(p.y * 8.0 - uTime * 9.0) * sin(p.z * 10.0 + uTime * 7.0);
    p += normal * n * uDistortion;
    vNormal = normalize(normalMatrix * normal);
    vWarp = n;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(p, 1.0);
  }
`;

const SHELL_FRAGMENT = /* glsl */ `
  uniform float uOpacity;
  uniform float uDistortion;
  varying vec3 vNormal;
  varying float vWarp;
  void main() {
    float edge = pow(1.0 - abs(normalize(vNormal).z), 1.5);
    vec3 color = mix(vec3(0.28, 0.16, 0.96), vec3(0.86, 0.75, 1.0), clamp(edge + abs(vWarp) * uDistortion, 0.0, 1.0));
    gl_FragColor = vec4(color * 2.3, uOpacity * (0.2 + edge * 0.8));
  }
`;

type ParticleSeed = { direction: THREE.Vector3; tangent: THREE.Vector3; phase: number; speed: number; size: number; delay: number };

export class PhantasmalForceVFX {
  private readonly group = new THREE.Group();
  private readonly core: THREE.Mesh<THREE.IcosahedronGeometry, THREE.ShaderMaterial>;
  private readonly shell: THREE.Mesh<THREE.IcosahedronGeometry, THREE.ShaderMaterial>;
  private readonly light = new THREE.PointLight(0x9b75ff, 0, 5.5, 2);
  private readonly tendrils: THREE.Mesh<THREE.BufferGeometry, THREE.ShaderMaterial>[] = [];
  private particles: THREE.InstancedMesh<THREE.IcosahedronGeometry, THREE.MeshStandardMaterial>;
  private readonly particleGeo = new THREE.IcosahedronGeometry(1, 0);
  private readonly particleMat = new THREE.MeshStandardMaterial({
    color: 0x9f78ff,
    emissive: 0x8660ff,
    emissiveIntensity: 4,
    roughness: 0.5,
    metalness: 0.1,
  });
  private readonly shellMat: THREE.ShaderMaterial;
  private readonly coreMat: THREE.ShaderMaterial;
  private settings: PhantasmalForceSettings = { ...DEFAULT_PHANTASMAL_FORCE_SETTINGS };
  private seeds: ParticleSeed[] = [];
  private elapsed = 0;
  private worldScale = 1;
  private active = false;
  private impacted = false;
  private callbacks: { onImpact: () => void; onComplete: () => void } | null = null;
  private readonly dummy = new THREE.Object3D();
  private readonly color = new THREE.Color();
  private readonly particlePosition = new THREE.Vector3();
  private readonly particleOutward = new THREE.Vector3();
  private readonly particleOrbit = new THREE.Vector3();
  private readonly particleEject = new THREE.Vector3();
  private disposed = false;

  constructor(private readonly scene: THREE.Object3D) {
    this.coreMat = new THREE.ShaderMaterial({
      uniforms: { uTime: { value: 0 }, uEmission: { value: 1 }, uDistortion: { value: 0.08 } },
      vertexShader: CORE_VERTEX,
      fragmentShader: CORE_FRAGMENT,
      transparent: true,
      depthTest: true,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
    });
    this.shellMat = new THREE.ShaderMaterial({
      uniforms: { uOpacity: { value: 0 }, uDistortion: { value: 0.1 }, uTime: { value: 0 } },
      vertexShader: SHELL_VERTEX,
      fragmentShader: SHELL_FRAGMENT,
      transparent: true,
      depthTest: true,
      depthWrite: false,
      side: THREE.DoubleSide,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
    });
    const ico = new THREE.IcosahedronGeometry(0.5, 3);
    this.core = new THREE.Mesh(ico, this.coreMat);
    this.shell = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 4), this.shellMat);
    this.core.renderOrder = 8;
    this.shell.renderOrder = 7;
    this.core.castShadow = false;
    this.shell.castShadow = false;
    this.particles = new THREE.InstancedMesh(this.particleGeo, this.particleMat, this.settings.particleCount);
    this.particles.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    this.particles.frustumCulled = false;
    this.particles.castShadow = false;
    this.particles.receiveShadow = false;
    this.group.add(this.particles, this.core, this.shell, this.light);
    this.light.castShadow = true;
    this.light.shadow.mapSize.set(256, 256);
    this.light.shadow.camera.near = 0.05;
    this.light.visible = false;
    this.group.visible = false;
    this.scene.add(this.group);
    this.buildTendrils();
  }

  setSettings(settings: PhantasmalForceSettings): void {
    this.settings = { ...settings };
    if (this.disposed) return;
    const desired = Math.max(1, Math.round(settings.particleCount));
    if (desired !== this.particles.count) {
      const next = new THREE.InstancedMesh(this.particleGeo, this.particleMat, desired);
      next.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
      next.frustumCulled = false;
      next.castShadow = false;
      next.receiveShadow = false;
      next.visible = this.active;
      this.group.remove(this.particles);
      this.particles = next;
      this.group.add(this.particles);
    }
    this.buildTendrils();
  }

  hide(): void {
    this.active = false;
    this.group.visible = false;
    this.light.intensity = 0;
    this.light.visible = false;
  }

  restartAt(target: THREE.Vector3, worldScale = 1, callbacks: { onImpact?: () => void; onComplete?: () => void } = {}): void {
    if (this.disposed) return;
    this.setSettings(getActivePhantasmalForceSettings());
    this.worldScale = Math.max(0.001, worldScale);
    this.elapsed = 0;
    this.active = true;
    this.impacted = false;
    this.group.position.copy(target);
    this.group.scale.setScalar(this.worldScale);
    this.group.visible = true;
    this.shell.visible = false;
    this.light.visible = true;
    this.callbacks = { onImpact: callbacks.onImpact ?? (() => {}), onComplete: callbacks.onComplete ?? (() => {}) };
    const random = mulberry(this.settings.seed);
    this.seeds = Array.from({ length: this.particles.count }, () => {
      const z = random() * 2 - 1;
      const angle = random() * Math.PI * 2;
      const planar = Math.sqrt(Math.max(0, 1 - z * z));
      const direction = new THREE.Vector3(planar * Math.cos(angle), planar * Math.sin(angle), z).normalize();
      const tangent = new THREE.Vector3(-direction.y, direction.x, (random() - 0.5) * 0.8).normalize();
      return { direction, tangent, phase: random() * Math.PI * 2, speed: 0.55 + random() * 1.35, size: 0.012 + random() * 0.032, delay: random() * 0.2 };
    });
    for (const tendril of this.tendrils) tendril.visible = true;
    this.update(0);
  }

  update(dt: number): void {
    if (!this.active || this.disposed) return;
    this.elapsed += Math.min(0.05, Math.max(0, dt));
    const t = this.elapsed;
    const collapseStart = 0.55;
    const collapseEnd = Math.min(0.79, collapseStart + Math.max(0.08, this.settings.compressionDuration));
    const collapse = THREE.MathUtils.smoothstep(t, collapseStart, collapseEnd);
    const release = Math.max(0, t - IMPACT_AT);
    this.core.position.set(0, 0, 0.015);
    const coreScale = this.settings.coreSize * (0.35 + collapse * 1.9 + (this.impacted ? 0.18 : 0));
    this.core.scale.setScalar(coreScale);
    this.core.visible = t >= 0.12 && t < END_AT;
    this.coreMat.uniforms.uTime!.value = t;
    this.coreMat.uniforms.uEmission!.value = this.settings.coreEmission * (0.35 + collapse * 1.1 + (this.impacted ? 0.3 : 0));
    this.coreMat.uniforms.uDistortion!.value = 0.055 + collapse * 0.13;

    for (const strand of this.tendrils) {
      strand.material.uniforms.uTime!.value = t;
      strand.material.uniforms.uCollapse!.value = collapse;
      strand.material.uniforms.uTurbulence!.value = this.settings.tendrilTurbulence;
      strand.material.uniforms.uSplineNoise!.value = this.settings.splineNoise;
      strand.material.uniforms.uOpacity!.value = t < 0.55 ? Math.min(1, t / 0.15) * 0.9 : 0.9 * (1 - collapse * 0.75) * Math.max(0, 1 - Math.max(0, t - 0.98) / Math.max(0.08, this.settings.residualLifetime));
      strand.scale.setScalar(this.settings.radius);
      strand.visible = t < END_AT && strand.material.uniforms.uOpacity!.value > 0.005;
    }

    this.updateParticles(t, collapse, release);
    const buildup = THREE.MathUtils.smoothstep(t, 0.02, 0.5);
    const spike = this.impacted ? Math.max(0, 1 - release / 0.14) : 0;
    const fade = t > IMPACT_AT ? Math.max(0, 1 - release / Math.max(0.08, this.settings.residualLifetime)) : 1;
    this.light.distance = this.settings.lightRadius * this.worldScale;
    this.light.intensity = (this.settings.buildupLight * buildup * (1 - collapse * 0.45) + this.settings.impactLight * spike) * fade * Math.pow(this.worldScale, 1.5);
    this.light.decay = 2;
    this.light.shadow.camera.far = this.light.distance;
    this.light.shadow.camera.updateProjectionMatrix();
    this.shellMat.uniforms.uTime!.value = t;
    this.shellMat.uniforms.uDistortion!.value = this.settings.distortionStrength;
    if (this.impacted) {
      const shellRadius = Math.max(0.01, Math.min(this.settings.shellRadius, release * this.settings.shellSpeed));
      this.shell.scale.setScalar(shellRadius);
      this.shellMat.uniforms.uOpacity!.value = Math.max(0, 0.72 * (1 - shellRadius / Math.max(0.01, this.settings.shellRadius)));
      this.shell.visible = release < this.settings.shellRadius / Math.max(0.1, this.settings.shellSpeed) && this.shellMat.uniforms.uOpacity!.value > 0.005;
    }
    if (!this.impacted && t >= IMPACT_AT) {
      this.impacted = true;
      this.shell.visible = true;
      this.callbacks?.onImpact();
    }
    const duration = END_AT + (this.settings.residualLifetime - 0.5);
    if (t >= duration) {
      this.active = false;
      this.group.visible = false;
      this.light.intensity = 0;
      this.light.visible = false;
      this.callbacks?.onComplete();
      this.callbacks = null;
    }
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.active = false;
    this.scene.remove(this.group);
    this.group.clear();
    for (const tendril of this.tendrils) {
      tendril.geometry.dispose();
      tendril.material.dispose();
    }
    this.tendrils.length = 0;
    this.particleGeo.dispose();
    this.particleMat.dispose();
    this.core.geometry.dispose();
    this.coreMat.dispose();
    this.shell.geometry.dispose();
    this.shellMat.dispose();
  }

  private buildTendrils(): void {
    for (const strand of this.tendrils) {
      this.group.remove(strand);
      strand.geometry.dispose();
      strand.material.dispose();
    }
    this.tendrils.length = 0;
    const count = Math.max(1, Math.min(16, Math.round(this.settings.tendrilCount)));
    for (let i = 0; i < count; i++) {
      const uniforms = {
        uTime: { value: 0 },
        uCollapse: { value: 0 },
        uTurbulence: { value: this.settings.tendrilTurbulence },
        uSplineNoise: { value: this.settings.splineNoise },
        uOpacity: { value: 0 },
      };
      const material = new THREE.ShaderMaterial({
        uniforms,
        vertexShader: TENDRIL_VERTEX,
        fragmentShader: TENDRIL_FRAGMENT,
        transparent: true,
        depthTest: true,
        depthWrite: false,
        side: THREE.DoubleSide,
        blending: THREE.AdditiveBlending,
        toneMapped: false,
      });
      const mesh = new THREE.Mesh(tubeGeometry(this.settings.seed + i * 137, this.settings.tendrilThickness), material);
      mesh.renderOrder = 6;
      mesh.frustumCulled = false;
      mesh.visible = false;
      this.tendrils.push(mesh);
      this.group.add(mesh);
    }
  }

  private updateParticles(t: number, collapse: number, release: number): void {
    const radius = this.settings.radius * (1 - collapse * 0.94);
    for (let i = 0; i < this.particles.count; i++) {
      const particle = this.seeds[i];
      if (!particle) {
        this.dummy.position.set(0, 0, 0);
        this.dummy.scale.setScalar(0);
        this.dummy.updateMatrix();
        this.particles.setMatrixAt(i, this.dummy.matrix);
        continue;
      }
      const age = Math.max(0, t - particle.delay);
      const orbitAngle = particle.phase + age * this.settings.spiralStrength * particle.speed;
      const orbit = this.particleOrbit.copy(particle.tangent).multiplyScalar(Math.sin(orbitAngle) * radius * 0.3);
      const outward = this.particleOutward.copy(particle.direction).multiplyScalar(radius * (1 - Math.min(0.98, age * this.settings.particleAttraction * (0.4 + collapse * 7))));
      const position = this.particlePosition.copy(outward).add(orbit);
      if (this.impacted) {
        const eject = this.particleEject.copy(particle.direction).multiplyScalar(Math.min(this.settings.shellRadius * 0.72, release * 2.2 * particle.speed));
        position.add(eject);
        position.y += release * release * 1.5;
      }
      this.dummy.position.copy(position);
      const lifetime = this.impacted ? Math.max(0, 1 - release / Math.max(0.1, this.settings.residualLifetime)) : 1;
      const size = particle.size * (0.55 + collapse * 0.7) * lifetime;
      this.dummy.scale.setScalar(size);
      this.dummy.rotation.set(orbitAngle, orbitAngle * 0.7, orbitAngle * 0.37);
      this.dummy.updateMatrix();
      this.particles.setMatrixAt(i, this.dummy.matrix);
      this.color.setHSL(0.72 + collapse * 0.06, 0.78, 0.5 + collapse * 0.28);
      this.particles.setColorAt(i, this.color);
    }
    this.particles.instanceMatrix.needsUpdate = true;
    if (this.particles.instanceColor) this.particles.instanceColor.needsUpdate = true;
  }
}
