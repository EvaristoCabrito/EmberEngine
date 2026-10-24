import * as THREE from "three";

export interface VfxParticleCurves {
  size: (age: number) => number;
  opacity: (age: number) => number;
  emissive: (age: number) => number;
  velocity: (age: number) => number;
  color: (age: number, target: THREE.Color) => THREE.Color;
}

export interface FireEmitterSettings {
  particleCount: number;
  particleScale: number;
  velocity: number;
  drag: number;
  turbulence: number;
  gravity: number;
  flipbookFps: number;
  coreIntensity: number;
  lightIntensity: number;
  lightRadius: number;
  /** Optional family controls; omitted values preserve the FX Lab's original fire. */
  lifetimeScale?: number;
  spread?: number;
  /** Optional steady jet flow. Defaults to the original ambient fire motion. */
  flowDirection?: [number, number, number];
  flowSpeed?: number;
  flowSpread?: number;
}

export const DEFAULT_FIRE_EMITTER: FireEmitterSettings = {
  particleCount: 26,
  particleScale: 1.18,
  velocity: 1.02,
  drag: 1.25,
  turbulence: 0.65,
  gravity: 0.35,
  flipbookFps: 18,
  coreIntensity: 1.15,
  lightIntensity: 6.5,
  lightRadius: 3.2,
};

type Particle = {
  age: number;
  life: number;
  phase: number;
  spin: number;
  rotation: number;
  size: number;
  riseSpeed: number;
  position: THREE.Vector3;
  velocity: THREE.Vector3;
};

const MAX_PARTICLES = 96;
const tempColor = new THREE.Color();
/** Reusable pooled emitter. Simulation state stays on the CPU; quads and frame selection are
 * uploaded as instanced attributes so the whole emitter is one draw call. */
export class ParticleEmitter {
  readonly particles: Particle[] = Array.from({ length: MAX_PARTICLES }, () => ({
    age: 0,
    life: 1,
    phase: 0,
    spin: 0,
    rotation: 0,
    size: 0,
    riseSpeed: 0,
    position: new THREE.Vector3(),
    velocity: new THREE.Vector3(),
  }));
  readonly geometry: THREE.InstancedBufferGeometry;
  readonly material: THREE.ShaderMaterial;
  readonly mesh: THREE.Mesh;
  readonly light = new THREE.PointLight(0xff7624, 0, 3.2, 1.6);
  readonly curves: VfxParticleCurves = {
    size: (t) => 0.42 + Math.sin(Math.PI * (0.12 + t * 0.76)) * 0.48,
    opacity: (t) => Math.min(1, t * 7) * Math.pow(1 - t, 0.8),
    emissive: (t) => 1.7 - t * 0.85,
    velocity: (t) => 1 - t * 0.42,
    color: (t, color) => color.setRGB(1, 0.94 - t * 0.56, 0.68 - t * 0.6),
  };
  private clock = 0;
  private random = Math.random;

  setSeed(seed: number): void {
    let state = seed >>> 0 || 1;
    this.random = () => {
      state ^= state << 13;
      state ^= state >>> 17;
      state ^= state << 5;
      return (state >>> 0) / 4294967296;
    };
  }

  constructor(texture: THREE.Texture, depthTexture: THREE.DepthTexture | null, uniforms: {
    sceneNear: number;
    sceneFar: number;
    viewport: THREE.Vector2;
    intensity: number;
  }, curves?: Partial<VfxParticleCurves>) {
    // The default curve set is the established stationary flame. Other atlas elements may
    // supply a tint while keeping the same particle, size, opacity, emissive, and motion curves.
    if (curves) Object.assign(this.curves, curves);
    const quad = new THREE.PlaneGeometry(1, 1);
    const geometry = new THREE.InstancedBufferGeometry();
    geometry.index = quad.index;
    geometry.setAttribute("position", quad.getAttribute("position"));
    geometry.setAttribute("uv", quad.getAttribute("uv"));
    const attr = (name: string, size: number) => {
      const a = new THREE.InstancedBufferAttribute(new Float32Array(MAX_PARTICLES * size), size);
      a.setUsage(THREE.DynamicDrawUsage);
      geometry.setAttribute(name, a);
      return a;
    };
    attr("aCenter", 3);
    attr("aSize", 1);
    attr("aRotation", 1);
    attr("aFrame", 1);
    attr("aOpacity", 1);
    attr("aEmissive", 1);
    attr("aTint", 3);
    geometry.instanceCount = 0;
    this.geometry = geometry;

    this.material = new THREE.ShaderMaterial({
      uniforms: {
        uFlipbook: { value: texture },
        uSceneDepth: { value: depthTexture },
        uNear: { value: uniforms.sceneNear },
        uFar: { value: uniforms.sceneFar },
        uViewport: { value: uniforms.viewport },
        uIntensity: { value: uniforms.intensity },
        uSoftDistance: { value: 0.34 },
        uSoftParticles: { value: depthTexture ? 1 : 0 },
      },
      vertexShader: /* glsl */ `
        attribute vec3 aCenter;
        attribute float aSize;
        attribute float aRotation;
        attribute float aFrame;
        attribute float aOpacity;
        attribute float aEmissive;
        attribute vec3 aTint;
        uniform float uIntensity;
        varying vec2 vUv;
        varying float vViewDepth;
        varying float vFrame;
        varying float vAlpha;
        varying float vEmissive;
        varying vec3 vTint;
        void main() {
          vec4 viewCenter = modelViewMatrix * vec4(aCenter, 1.0);
          float c = cos(aRotation);
          float s = sin(aRotation);
          vec2 p = vec2(position.x * c - position.y * s, position.x * s + position.y * c) * aSize;
          viewCenter.xy += p;
          gl_Position = projectionMatrix * viewCenter;
          vUv = uv;
          vViewDepth = -viewCenter.z;
          vFrame = mod(aFrame, 16.0);
          vAlpha = aOpacity;
          vEmissive = aEmissive * uIntensity;
          vTint = aTint;
        }
      `,
      fragmentShader: /* glsl */ `
        uniform sampler2D uFlipbook;
        uniform sampler2D uSceneDepth;
        uniform float uNear;
        uniform float uFar;
        uniform vec2 uViewport;
        uniform float uSoftDistance;
        uniform float uSoftParticles;
        varying vec2 vUv;
        varying float vViewDepth;
        varying float vFrame;
        varying float vAlpha;
        varying float vEmissive;
        varying vec3 vTint;
        void main() {
          float frame = floor(mod(vFrame, 16.0));
          vec2 tile = vec2(mod(frame, 4.0), 3.0 - floor(frame / 4.0));
          vec2 localUv = clamp(vUv, vec2(0.02), vec2(0.98));
          float turbulence = sin(localUv.y * 13.0 + vFrame * 0.37) * sin(localUv.x * 9.0 - vFrame * 0.29);
          localUv += vec2(turbulence * 0.014, turbulence * 0.006);
          vec2 atlasUv = (clamp(localUv, vec2(0.02), vec2(0.98)) + tile) * 0.25;
          vec4 flame = texture2D(uFlipbook, atlasUv);
          float alpha = flame.a * vAlpha;
          if (uSoftParticles > 0.5) {
            vec2 screenUv = gl_FragCoord.xy / uViewport;
            float depthSample = texture2D(uSceneDepth, screenUv).x;
            float sceneDepth = (2.0 * uNear * uFar) / (uFar + uNear - (depthSample * 2.0 - 1.0) * (uFar - uNear));
            float softIntersection = clamp((sceneDepth - vViewDepth) / uSoftDistance, 0.0, 1.0);
            alpha *= softIntersection;
          }
          if (alpha < 0.006) discard;
          vec3 radiance = flame.rgb * vTint * vEmissive;
          gl_FragColor = vec4(radiance, alpha);
          #include <tonemapping_fragment>
          #include <colorspace_fragment>
        }
      `,
      transparent: true,
      depthTest: false,
      depthWrite: false,
      blending: THREE.NormalBlending,
      toneMapped: false,
    });
    this.mesh = new THREE.Mesh(this.geometry, this.material);
    this.mesh.frustumCulled = false;
  }

  reset(settings: FireEmitterSettings, stagger = true): void {
    this.clock = 0;
    for (let i = 0; i < settings.particleCount; i++) {
      this.spawn(this.particles[i]!, settings, stagger ? this.random() : 0);
    }
    for (let i = settings.particleCount; i < MAX_PARTICLES; i++) this.particles[i]!.age = 99;
    this.upload(settings);
  }

  update(dt: number, settings: FireEmitterSettings, looping = true): void {
    this.clock += dt;
    for (let i = 0; i < settings.particleCount; i++) {
      const p = this.particles[i]!;
      p.age += dt;
      if (p.age >= p.life) {
        if (looping) this.spawn(p, settings, 0);
        else { p.age = p.life; p.velocity.set(0, 0, 0); }
      }
      const t = Math.min(1, p.age / p.life);
      const drag = Math.exp(-settings.drag * dt);
      p.velocity.multiplyScalar(drag);
      const wiggle = settings.turbulence * dt;
      p.velocity.y += (p.riseSpeed * this.curves.velocity(t) - p.velocity.y) * Math.min(1, dt * 3.2);
      p.velocity.x += (this.random() - 0.5) * wiggle * 3.4;
      p.velocity.z += (this.random() - 0.5) * wiggle * 3.4;
      p.velocity.y -= settings.gravity * dt;
      p.position.addScaledVector(p.velocity, dt);
      p.rotation += p.spin * dt;
    }
    this.upload(settings);
    const flicker = 0.94 + Math.sin(this.clock * 23) * 0.035 + Math.sin(this.clock * 37) * 0.025;
    this.light.intensity = settings.lightIntensity * flicker;
    this.light.distance = settings.lightRadius;
  }

  setFrame(settings: FireEmitterSettings): void {
    this.upload(settings);
  }

  dispose(): void {
    this.geometry.dispose();
    this.material.dispose();
  }

  private spawn(p: Particle, settings: FireEmitterSettings, initialAge: number): void {
    const r = this.random;
    const spread = settings.spread ?? 1;
    p.life = (0.62 + r() * 0.5) * (settings.lifetimeScale ?? 1);
    p.age = initialAge * p.life;
    p.phase = r() * 16;
    // Store a unitless size so pooled emitters can change scale with the camera at any time.
    p.size = 0.35 + r() * 0.33;
    p.rotation = r() * Math.PI * 2;
    p.spin = (r() - 0.5) * 1.4;
    p.position.set((r() - 0.5) * 0.28 * spread, 0.12 + r() * 0.22, (r() - 0.5) * 0.28 * spread);
    const angle = r() * Math.PI * 2;
    const radial = r() * 0.16 * spread;
    p.riseSpeed = settings.velocity * (0.7 + r() * 0.55);
    const flow = settings.flowDirection;
    const flowAngle = flow ? Math.atan2(flow[1], flow[0]) + (r() - 0.5) * (settings.flowSpread ?? 0) : 0;
    const flowSpeed = flow ? (settings.flowSpeed ?? 0) : 0;
    p.velocity.set(
      Math.cos(angle) * radial + (flow ? Math.cos(flowAngle) * flowSpeed : 0),
      p.riseSpeed + (flow ? Math.sin(flowAngle) * flowSpeed : 0),
      Math.sin(angle) * radial + (flow?.[2] ?? 0) * flowSpeed,
    );
    p.position.addScaledVector(p.velocity, p.age);
  }

  private upload(settings: FireEmitterSettings): void {
    const centers = this.geometry.getAttribute("aCenter") as THREE.InstancedBufferAttribute;
    const sizes = this.geometry.getAttribute("aSize") as THREE.InstancedBufferAttribute;
    const rotations = this.geometry.getAttribute("aRotation") as THREE.InstancedBufferAttribute;
    const frames = this.geometry.getAttribute("aFrame") as THREE.InstancedBufferAttribute;
    const opacities = this.geometry.getAttribute("aOpacity") as THREE.InstancedBufferAttribute;
    const emissives = this.geometry.getAttribute("aEmissive") as THREE.InstancedBufferAttribute;
    const tints = this.geometry.getAttribute("aTint") as THREE.InstancedBufferAttribute;
    for (let i = 0; i < settings.particleCount; i++) {
      const p = this.particles[i]!;
      const t = Math.min(1, p.age / p.life);
      centers.setXYZ(i, p.position.x, p.position.y, p.position.z);
      sizes.setX(i, p.size * settings.particleScale * this.curves.size(t));
      rotations.setX(i, p.rotation);
      frames.setX(i, (p.phase + p.age * settings.flipbookFps) % 16);
      opacities.setX(i, this.curves.opacity(t));
      emissives.setX(i, this.curves.emissive(t));
      const color = this.curves.color(t, tempColor);
      tints.setXYZ(i, color.r, color.g, color.b);
    }
    for (const a of [centers, sizes, rotations, frames, opacities, emissives, tints]) a.needsUpdate = true;
    this.geometry.instanceCount = settings.particleCount;
    this.material.uniforms.uIntensity!.value = settings.coreIntensity;
  }

}

export function loadFireFlipbook(): Promise<THREE.Texture> {
  return new THREE.TextureLoader().loadAsync("/game/fx/fire-flipbook-4x4.png?v=1").then((texture) => {
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.generateMipmaps = true;
    texture.minFilter = THREE.LinearMipmapLinearFilter;
    texture.magFilter = THREE.LinearFilter;
    texture.wrapS = THREE.ClampToEdgeWrapping;
    texture.wrapT = THREE.ClampToEdgeWrapping;
    return texture;
  });
}
