import * as THREE from "three";

export type ImpactSettings = {
  seed: number;
  flashIntensity: number; flashSize: number; flashDuration: number; flashOffset: number;
  primaryCount: number; primarySize: number; primarySpeed: number; primaryDrag: number; primaryLife: number; verticalSpread: number; primaryEmission: number; primaryOffset: number;
  secondaryCount: number; secondarySize: number; secondarySpeed: number; buoyancy: number; secondaryLife: number; secondaryOffset: number;
  radialCount: number; radialSpeed: number; radialOffset: number;
  sparkCount: number; sparkSpeed: number; sparkGravity: number; sparkDrag: number; sparkLife: number; sparkOffset: number;
  emberCount: number; emberSpeed: number; emberGravity: number; emberLife: number; emberOffset: number;
  smokeCount: number; smokeSize: number; smokeRise: number; smokeExpansion: number; smokeOpacity: number; smokeLife: number; smokeOffset: number;
  peakLight: number; lightRadius: number; lightDecay: number;
  flipbookFps: number;
};

export const DEFAULT_IMPACT_SETTINGS: ImpactSettings = {
  seed: 1337, flashIntensity: 18, flashSize: 0.72, flashDuration: 0.07, flashOffset: 0,
  primaryCount: 64, primarySize: 0.68, primarySpeed: 3.4, primaryDrag: 5.5, primaryLife: 0.42, verticalSpread: 0.48, primaryEmission: 2.4, primaryOffset: 0.02,
  secondaryCount: 34, secondarySize: 0.98, secondarySpeed: 1.6, buoyancy: 0.8, secondaryLife: 0.72, secondaryOffset: 0.08,
  radialCount: 52, radialSpeed: 2.9, radialOffset: 0.07,
  sparkCount: 112, sparkSpeed: 6.8, sparkGravity: 8.8, sparkDrag: 0.65, sparkLife: 0.65, sparkOffset: 0.03,
  emberCount: 30, emberSpeed: 2.1, emberGravity: 3.5, emberLife: 0.9, emberOffset: 0.08,
  smokeCount: 16, smokeSize: 0.72, smokeRise: 0.68, smokeExpansion: 0.5, smokeOpacity: 0.22, smokeLife: 1.4, smokeOffset: 0.18,
  peakLight: 22, lightRadius: 5.4, lightDecay: 0.96, flipbookFps: 18,
};

let activeImpactSettings: ImpactSettings = { ...DEFAULT_IMPACT_SETTINGS };
const IMPACT_SETTINGS_STORAGE_KEY = "emberash:fireball-impact-settings";
let impactSettingsLoaded = false;

/** Shared tuning used by Dev Controls and by the next real Fireball cast. */
export function getActiveImpactSettings(): ImpactSettings {
  if (!impactSettingsLoaded && typeof window !== "undefined") {
    try {
      const raw = window.localStorage.getItem(IMPACT_SETTINGS_STORAGE_KEY);
      if (raw) {
        const parsed = JSON.parse(raw) as Partial<ImpactSettings>;
        const saved = { ...DEFAULT_IMPACT_SETTINGS };
        for (const key of Object.keys(saved) as (keyof ImpactSettings)[]) {
          const value = parsed[key];
          if (typeof value === "number" && Number.isFinite(value)) saved[key] = value;
        }
        activeImpactSettings = saved;
      }
    } catch { /* Keep defaults if local storage is unavailable or malformed. */ }
    impactSettingsLoaded = true;
  }
  return { ...activeImpactSettings };
}

export function setActiveImpactSettings(settings: ImpactSettings): void {
  activeImpactSettings = { ...settings };
  if (typeof window !== "undefined") {
    try { window.localStorage.setItem(IMPACT_SETTINGS_STORAGE_KEY, JSON.stringify(activeImpactSettings)); }
    catch { /* The live preview still updates if persistent storage is unavailable. */ }
  }
}

type Kind = "primary" | "secondary" | "radial" | "spark" | "ember" | "smoke";
type Particle = { kind: Kind; delay: number; life: number; size: number; phase: number; spin: number; rotation: number; drag: number; gravity: number; initial: THREE.Vector3; velocity: THREE.Vector3 };
const CAPACITY = 512;

function seeded(seed: number) {
  let value = seed >>> 0;
  return () => { value = (value * 1664525 + 1013904223) >>> 0; return value / 4294967296; };
}

function smooth(a: number, b: number, x: number) { const t = THREE.MathUtils.clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }

/** One pooled, instanced draw for every layer of a seeded impact; restarting rewinds the same data. */
export class FireballImpactVFX {
  readonly geometry: THREE.InstancedBufferGeometry;
  readonly material: THREE.ShaderMaterial;
  readonly mesh: THREE.Mesh;
  readonly light = new THREE.PointLight(0xff7624, 0, 1, 1.45);
  readonly flash: THREE.Mesh<THREE.PlaneGeometry, THREE.ShaderMaterial>;
  private particles: Particle[] = [];
  private elapsed = 0;
  private duration = 1.45;
  private complete = false;
  private origin = new THREE.Vector3(0, 0.19, 0);
  private readonly flashMaterial: THREE.ShaderMaterial;

  constructor(texture: THREE.Texture, depth: THREE.DepthTexture | null, near: number, far: number, viewport: THREE.Vector2, worldOcclusion = false) {
    this.light.castShadow = true;
    this.light.shadow.mapSize.set(512, 512);
    this.light.shadow.camera.near = 0.1;
    this.light.shadow.camera.far = 400;
    this.light.shadow.bias = -0.001;
    const quad = new THREE.PlaneGeometry(1, 1);
    this.geometry = new THREE.InstancedBufferGeometry();
    this.geometry.index = quad.index;
    this.geometry.setAttribute("position", quad.getAttribute("position"));
    this.geometry.setAttribute("uv", quad.getAttribute("uv"));
    const attr = (name: string, size: number) => this.geometry.setAttribute(name, new THREE.InstancedBufferAttribute(new Float32Array(CAPACITY * size), size).setUsage(THREE.DynamicDrawUsage));
    attr("aCenter", 3); attr("aSize", 1); attr("aRotation", 1); attr("aFrame", 1); attr("aAlpha", 1); attr("aIntensity", 1); attr("aTint", 3); attr("aKind", 1);
    this.geometry.instanceCount = 0;
    this.material = new THREE.ShaderMaterial({
      uniforms: { uFlipbook: { value: texture }, uDepth: { value: depth }, uNear: { value: near }, uFar: { value: far }, uViewport: { value: viewport }, uFps: { value: DEFAULT_IMPACT_SETTINGS.flipbookFps }, uSoftDistance: { value: 0.28 }, uSoftParticles: { value: depth ? 1 : 0 } },
      vertexShader: `attribute vec3 aCenter; attribute float aSize; attribute float aRotation; attribute float aFrame; attribute float aAlpha; attribute float aIntensity; attribute vec3 aTint; attribute float aKind; uniform float uFps; varying vec2 vUv; varying float vFrame; varying float vAlpha; varying float vIntensity; varying vec3 vTint; varying float vKind; varying float vDepth; void main(){ vec4 c=modelViewMatrix*vec4(aCenter,1.0); float cs=cos(aRotation), sn=sin(aRotation); vec2 p=vec2(position.x*cs-position.y*sn,position.x*sn+position.y*cs)*aSize; c.xy+=p; gl_Position=projectionMatrix*c; vUv=uv; vFrame=aFrame; vAlpha=aAlpha; vIntensity=aIntensity; vTint=aTint; vKind=aKind; vDepth=-c.z; }`,
      fragmentShader: `uniform sampler2D uFlipbook; uniform sampler2D uDepth; uniform float uNear; uniform float uFar; uniform vec2 uViewport; uniform float uSoftDistance; uniform float uSoftParticles; varying vec2 vUv; varying float vFrame; varying float vAlpha; varying float vIntensity; varying vec3 vTint; varying float vKind; varying float vDepth; float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); } float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f); return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y); } void main(){ vec2 uv=vUv; vec4 texel; float alpha; vec3 color=vTint; if(vKind<0.5){ float frame=floor(mod(vFrame,16.0)); vec2 tile=vec2(mod(frame,4.0),3.0-floor(frame/4.0)); vec2 atlas=(clamp(uv,vec2(0.015),vec2(0.985))+tile)*0.25; texel=texture2D(uFlipbook,atlas); alpha=texel.a*vAlpha; color*=texel.rgb; } else if(vKind<1.5){ vec2 q=(uv-0.5)*vec2(1.0,1.8); float d=length(q); alpha=(1.0-smoothstep(0.02,0.5,d))*vAlpha; color*=mix(vec3(1.0,0.98,0.78),vec3(1.0,0.18,0.015),smoothstep(0.05,0.42,d)); } else { vec2 q=(uv-0.5)*vec2(1.0,1.35); float n=noise(uv*5.3+vec2(vFrame*0.008,-vFrame*0.013))*0.55+noise(uv*11.0+vFrame*0.004)*0.25; float d=length(q); float cloud=1.0-smoothstep(0.18+n*0.12,0.58+n*0.2,d); alpha=cloud*vAlpha; color*=0.62+n*0.55; } if(uSoftParticles>0.5){ vec2 suv=gl_FragCoord.xy/uViewport; float z=texture2D(uDepth,suv).x; float scene=(2.0*uNear*uFar)/(uFar+uNear-(z*2.0-1.0)*(uFar-uNear)); float soft=clamp((scene-vDepth)/uSoftDistance,0.0,1.0); alpha*=soft; } if(alpha<0.004) discard; gl_FragColor=vec4(color*vIntensity,alpha);
#include <tonemapping_fragment>
#include <colorspace_fragment>
}`, 
      transparent: true, depthTest: worldOcclusion, depthWrite: false, blending: THREE.NormalBlending, toneMapped: false,
    });
    this.mesh = new THREE.Mesh(this.geometry, this.material);
    this.mesh.frustumCulled = false;
    this.flashMaterial = new THREE.ShaderMaterial({ uniforms: { uOpacity: { value: 0 }, uIntensity: { value: 18 }, uTint: { value: new THREE.Color(0xfff2c1) } }, vertexShader: `varying vec2 vUv; void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}`, fragmentShader: `uniform float uOpacity; uniform float uIntensity; uniform vec3 uTint; varying vec2 vUv; void main(){float d=length((vUv-0.5)*vec2(1.0,1.15));float core=exp(-d*18.0);float halo=exp(-d*5.0);float a=(core+halo*0.36)*uOpacity;if(a<0.005)discard;vec3 c=mix(vec3(1.0,0.46,0.04),uTint,core);gl_FragColor=vec4(c*a*uIntensity,a);}` , transparent: true, depthTest: worldOcclusion, depthWrite: false, blending: THREE.AdditiveBlending, toneMapped: false });
    this.flash = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), this.flashMaterial);
    this.flash.frustumCulled = false;
    this.build(DEFAULT_IMPACT_SETTINGS);
  }

  get finished() { return this.complete; }
  setOrigin(point: THREE.Vector3) { this.origin.copy(point); this.origin.y += 0.02; this.light.position.copy(this.origin); this.flash.position.copy(this.origin).y += 0.08; }
  restart(settings: ImpactSettings) { this.elapsed = 0; this.complete = false; this.build(settings); }
  randomizeSeed(settings: ImpactSettings) { settings.seed = Math.floor(Math.random() * 999999) + 1; this.restart(settings); }

  update(dt: number, s: ImpactSettings, looping: boolean, camera: THREE.Camera, viewport?: THREE.Vector2, worldScale = 1, aoeRadius = Number.POSITIVE_INFINITY) {
    if (this.complete && !looping) return;
    this.elapsed += dt;
    const end = this.duration;
    if (this.elapsed >= end) {
      if (looping) { this.elapsed %= end; this.build(s); }
      else { this.elapsed = end; this.complete = true; }
    }
    const t = this.elapsed;
    const flashAge = t - s.flashOffset;
    const flashT = flashAge / Math.max(0.015, s.flashDuration);
    const flashPulse = flashAge >= 0 && flashT < 1 ? Math.pow(1 - flashT, 2.4) : 0;
    this.flash.visible = flashPulse > 0.003;
    this.flash.quaternion.copy(camera.quaternion);
    const flashScale = s.flashSize * (0.35 + Math.max(0, flashT) * 3.2);
    // The original impact's opening flash now reaches the resolved AoE edge immediately.
    this.flash.scale.setScalar(flashScale * worldScale);
    this.flashMaterial.uniforms.uOpacity!.value = flashPulse * Math.min(1, s.flashIntensity / 18);
    this.flashMaterial.uniforms.uIntensity!.value = s.flashIntensity;
    this.light.position.copy(this.origin);
    const lightAttack = smooth(0, 0.045, t) * (1 - smooth(0.1, Math.max(0.12, s.lightDecay), t));
    const residual = t > 0.36 && t < s.lightDecay ? 0.13 * (0.65 + 0.35 * Math.sin(t * 49)) * (1 - smooth(0.55, s.lightDecay, t)) : 0;
    // Match the preview's illumination after scaling its world-unit effect for battle.
    this.light.intensity = s.peakLight * worldScale * worldScale * (lightAttack + residual);
    this.light.distance = Math.min(aoeRadius, s.lightRadius * worldScale * (0.62 + 0.58 * smooth(0, 0.09, t) * (1 - smooth(0.18, 0.62, t))));
    this.light.decay = 2;
    this.light.shadow.camera.far = Math.max(0.2, this.light.distance);
    this.light.shadow.camera.updateProjectionMatrix();
    this.material.uniforms.uFps!.value = s.flipbookFps;
    if (viewport) (this.material.uniforms.uViewport!.value as THREE.Vector2).copy(viewport);
    this.upload(t, s, camera, worldScale, aoeRadius);
  }

  dispose() { this.geometry.dispose(); this.material.dispose(); this.flash.geometry.dispose(); this.flashMaterial.dispose(); }

  private build(s: ImpactSettings) {
    const rand = seeded(s.seed);
    const list: Particle[] = [];
    const add = (kind: Kind, count: number, delay: number, life: number, size: number, speed: number, drag: number, gravity: number, height: number, mode: "sphere" | "ground" | "up") => {
      for (let i = 0; i < count && list.length < CAPACITY; i++) {
        const angle = rand() * Math.PI * 2;
        const y = mode === "ground" ? 0.015 + rand() * 0.11 : mode === "up" ? 0.1 + rand() * 0.36 : (rand() - 0.5) * height;
        const radius = mode === "up" ? 0.12 + rand() * 0.52 : mode === "ground" ? rand() * 0.17 : rand() * 0.24;
        const hAngle = Math.acos(2 * rand() - 1);
        const dir = mode === "ground" ? new THREE.Vector3(Math.cos(angle), 0, Math.sin(angle)) : mode === "up" ? new THREE.Vector3(Math.cos(angle) * 0.7, 0.35 + rand() * 0.8, Math.sin(angle) * 0.7).normalize() : new THREE.Vector3(Math.sin(hAngle) * Math.cos(angle), Math.cos(hAngle), Math.sin(hAngle) * Math.sin(angle));
        if (kind === "primary" && dir.y < -0.15) dir.y = Math.abs(dir.y) * 0.48;
        if (kind === "secondary" && dir.y < 0) dir.y *= 0.28;
        if (kind === "smoke") dir.set((rand() - 0.5) * 0.4, 0.2 + rand() * 0.4, (rand() - 0.5) * 0.4);
        const v = speed * (0.62 + rand() * 0.7);
        const p: Particle = { kind, delay: Math.max(0, delay + (rand() - 0.5) * Math.min(0.045, life * 0.08)), life: life * (0.78 + rand() * 0.42), size: size * (0.62 + rand() * 0.8), phase: rand() * 16, spin: (rand() - 0.5) * (kind === "smoke" ? 0.8 : 4), rotation: rand() * Math.PI * 2, drag: Math.max(0, drag * (0.72 + rand() * 0.56)), gravity: gravity * (0.7 + rand() * 0.6), initial: new THREE.Vector3(Math.cos(angle) * radius, y, Math.sin(angle) * radius), velocity: dir.multiplyScalar(v) };
        list.push(p);
      }
    };
    // Smoke is submitted first so the hot combustion remains legible in front of the cloud.
    add("smoke", s.smokeCount, 0.18 + s.smokeOffset, s.smokeLife, s.smokeSize, s.smokeRise, 0.8, -0.24, 0.5, "up");
    add("primary", s.primaryCount, 0.02 + s.primaryOffset, s.primaryLife, s.primarySize, s.primarySpeed, s.primaryDrag, 1.6, s.verticalSpread, "sphere");
    add("secondary", s.secondaryCount, 0.08 + s.secondaryOffset, s.secondaryLife, s.secondarySize, s.secondarySpeed, 2.6, -s.buoyancy, 0.5, "up");
    add("radial", s.radialCount, 0.07 + s.radialOffset, 0.4, 0.36, s.radialSpeed, 4.3, -1.8, 0.1, "ground");
    add("spark", s.sparkCount, 0.03 + s.sparkOffset, s.sparkLife, 0.075, s.sparkSpeed, s.sparkDrag, s.sparkGravity, 1.4, "sphere");
    add("ember", s.emberCount, 0.08 + s.emberOffset, s.emberLife, 0.16, s.emberSpeed, 0.9, s.emberGravity, 1.0, "sphere");
    this.particles = list;
    // Particle delays and lifetimes are randomized independently. Keep updating until the
    // last smoke/ember has reached alpha zero, or its final visible frame would freeze.
    this.duration = Math.max(1.45, ...list.map((particle) => particle.delay + particle.life));
    this.geometry.instanceCount = list.length;
  }

  private upload(time: number, s: ImpactSettings, camera: THREE.Camera, worldScale = 1, aoeRadius = Number.POSITIVE_INFINITY) {
    const centers = this.geometry.getAttribute("aCenter") as THREE.InstancedBufferAttribute;
    const sizes = this.geometry.getAttribute("aSize") as THREE.InstancedBufferAttribute;
    const rotations = this.geometry.getAttribute("aRotation") as THREE.InstancedBufferAttribute;
    const frames = this.geometry.getAttribute("aFrame") as THREE.InstancedBufferAttribute;
    const alphas = this.geometry.getAttribute("aAlpha") as THREE.InstancedBufferAttribute;
    const intensities = this.geometry.getAttribute("aIntensity") as THREE.InstancedBufferAttribute;
    const tints = this.geometry.getAttribute("aTint") as THREE.InstancedBufferAttribute;
    const kinds = this.geometry.getAttribute("aKind") as THREE.InstancedBufferAttribute;
    const q = camera.quaternion;
    this.flash.quaternion.copy(q);
    this.particles.forEach((p, i) => {
      const age = time - p.delay;
      const active = age >= 0 && age < p.life;
      const t = THREE.MathUtils.clamp(age / p.life, 0, 1);
      const dragTerm = p.drag < 0.001 ? age : (1 - Math.exp(-p.drag * Math.max(0, age))) / p.drag;
      const fall = 0.5 * p.gravity * age * age;
      let offsetX = (p.initial.x + p.velocity.x * dragTerm) * worldScale;
      let offsetY = (p.initial.y + p.velocity.y * dragTerm - fall) * worldScale;
      let scale = p.size * worldScale;
      let alpha = active ? Math.sin(Math.PI * Math.max(0, t)) ** (p.kind === "smoke" ? 0.45 : 0.72) : 0;
      let emission = 1;
      let color = new THREE.Color(0xffac3d);
      let kind = 0;
      // Combat Fireball impacts use procedural flames, never the old flipbook sprite.
      if (p.kind === "primary") { kind = 1; scale *= 0.68 + Math.sin(Math.PI * t) * 0.92; emission = s.primaryEmission * (1.35 - t * 0.62); color.setRGB(1, 0.9 - t * 0.58, 0.48 - t * 0.38); }
      if (p.kind === "secondary") { kind = 1; scale *= 0.72 + t * 0.7; emission = 1.25 * (1 - t * 0.5); color.setRGB(1, 0.72 - t * 0.46, 0.24 - t * 0.17); }
      if (p.kind === "radial") { kind = 0; scale *= 0.58 + Math.sin(Math.PI * t) * 0.45; emission = 1.8 * (1 - t); color.setRGB(1, 0.66 - t * 0.42, 0.16); }
      if (p.kind === "spark") { kind = 1; scale *= 0.7 + (1 - t) * 0.45; emission = 5.2 * (1 - t); color.setRGB(1, 0.97 - t * 0.68, 0.74 - t * 0.58); }
      if (p.kind === "ember") { kind = 1; scale *= 0.7 + Math.sin(Math.PI * t) * 0.25; emission = 2.4 * (1 - t); color.setRGB(1, 0.56 - t * 0.38, 0.12 + (1 - t) * 0.1); }
      if (p.kind === "smoke") { kind = 2; scale *= 0.65 + t * s.smokeExpansion * 2.2; alpha *= s.smokeOpacity * (1 - t) ** 0.65; emission = 0.42; color.setRGB(0.24, 0.22, 0.2); }
      // Keep every visible particle inside the same screen-space radius as Fireball's actual
      // damage cells. This is derived from the resolved hex list in the combat engine.
      const centerDistance = Math.hypot(offsetX, offsetY);
      const halfExtent = scale * 0.7072;
      const available = Math.max(0, aoeRadius - halfExtent);
      if (centerDistance > available && centerDistance > 0) {
        const fit = available / centerDistance;
        offsetX *= fit;
        offsetY *= fit;
      }
      scale = Math.min(scale, Math.max(0.001, aoeRadius * 1.4144));
      centers.setXYZ(i, this.origin.x + offsetX, this.origin.y + offsetY, this.origin.z + (p.initial.z + p.velocity.z * dragTerm) * worldScale);
      sizes.setX(i, scale); rotations.setX(i, p.rotation + p.spin * Math.max(0, age)); frames.setX(i, (p.phase + Math.max(0, age) * s.flipbookFps) % 16); alphas.setX(i, alpha); intensities.setX(i, emission); tints.setXYZ(i, color.r, color.g, color.b); kinds.setX(i, kind);
    });
    for (const name of ["aCenter", "aSize", "aRotation", "aFrame", "aAlpha", "aIntensity", "aTint", "aKind"]) (this.geometry.getAttribute(name) as THREE.InstancedBufferAttribute).needsUpdate = true;
  }
}
