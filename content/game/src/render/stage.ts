// Renderer, scene, lights and the one post-processing chain (bloom, optional tilt-shift, grade).

import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { ShaderPass } from 'three/examples/jsm/postprocessing/ShaderPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';
import { Pass, FullScreenQuad } from 'three/examples/jsm/postprocessing/Pass.js';

import { SPRITE_FLOOR } from '../units/spriteMaterial';
import { VolumetricFogPass } from './volumetricFog';
import { Moon } from './moon';

/** Ember's map time-of-day values (Mission.timeOfDay); undefined means "day". */
export type TimeOfDay = 'day' | 'noon' | 'dawn' | 'dusk' | 'brightNight' | 'darkNight';

interface LightPreset {
  sun: string; sunIntensity: number; azimuth: number; elevation: number;
  sky: string; ground: string; hemi: number; background: string; spriteFloor: number;
}

/** Every preset stays clearly readable (user: as much light as possible): dawn/dusk are
 * near-daylight with warm colour, nights are a bright blue moonlight plus a sprite floor. */
export const TIME_PRESETS: Record<TimeOfDay, LightPreset> = {
  day: { sun: '#fff0d4', sunIntensity: 3.0, azimuth: -0.95, elevation: 0.95, sky: '#d4e6ff', ground: '#5d4b33', hemi: 1.0, background: '#000000', spriteFloor: 0 },
  noon: { sun: '#ffffff', sunIntensity: 3.5, azimuth: -0.6, elevation: 1.25, sky: '#dcecff', ground: '#64523a', hemi: 1.1, background: '#000000', spriteFloor: 0 },
  dawn: { sun: '#ffd2a6', sunIntensity: 3.0, azimuth: -1.6, elevation: 0.45, sky: '#e2d0de', ground: '#6a5848', hemi: 1.05, background: '#000000', spriteFloor: 0.3 },
  dusk: { sun: '#ffb47e', sunIntensity: 2.9, azimuth: 1.6, elevation: 0.42, sky: '#c4b2d4', ground: '#5e4840', hemi: 1.0, background: '#000000', spriteFloor: 0.3 },
  // Nights are lit for real: only the moon (from its disc, moveMoon) and the lamps and spells on
  // the board. No sky fill and no sprite brightness floor.
  brightNight: { sun: '#c2d0ff', sunIntensity: 2.2, azimuth: -0.7, elevation: 0.9, sky: '#7d90ba', ground: '#3a3a48', hemi: 0, background: '#000000', spriteFloor: 0 },
  darkNight: { sun: '#b0c0f4', sunIntensity: 1.7, azimuth: -0.7, elevation: 0.9, sky: '#62759e', ground: '#2e2e3a', hemi: 0, background: '#000000', spriteFloor: 0 },
};

/** Tonight's moon: light strength (x the night preset), colour, and the disc's phase. */
export interface MoonLight { strength: number; color?: string; sky?: string; background?: string; lit?: number; waxing?: boolean; icon?: string }
/** Real seconds for the moon to cross the sky (rise to set); then the next night's moon rises. */
const MOON_CROSSING_SECONDS = 600;
/** The moon's light x the night preset's strength: a bright, playable moon (still x tonight's phase). */
const MOON_BRIGHTNESS = 1.5;

const FULLSCREEN_VS = 'varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }';

const TiltShiftShader = {
  uniforms: {
    tDiffuse: { value: null }, dir: { value: new THREE.Vector2(1, 0) }, res: { value: new THREE.Vector2(1, 1) },
    focus: { value: 0.5 }, band: { value: 0.13 }, amount: { value: 2.6 },
  },
  vertexShader: FULLSCREEN_VS,
  fragmentShader: `uniform sampler2D tDiffuse; uniform vec2 dir, res; uniform float focus, band, amount; varying vec2 vUv;
    void main(){
      float k = smoothstep(band, band + .34, abs(vUv.y - focus)) * amount;
      vec2 st = dir / res * k;
      vec4 c = texture2D(tDiffuse, vUv) * .2270270270;
      c += (texture2D(tDiffuse, vUv + st * 1.3846153846) + texture2D(tDiffuse, vUv - st * 1.3846153846)) * .3162162162;
      c += (texture2D(tDiffuse, vUv + st * 3.2307692308) + texture2D(tDiffuse, vUv - st * 3.2307692308)) * .0702702703;
      gl_FragColor = c;
    }`,
};

const GradeShader = {
  // neutral grade: the sprites keep their painted colours; only a soft vignette frames the diorama
  uniforms: { tDiffuse: { value: null }, sat: { value: 1.0 }, tint: { value: new THREE.Vector3(1, 1, 1) }, vig: { value: 0.15 } },
  vertexShader: FULLSCREEN_VS,
  fragmentShader: `uniform sampler2D tDiffuse; uniform float sat, vig; uniform vec3 tint; varying vec2 vUv;
    void main(){
      vec4 c = texture2D(tDiffuse, vUv);
      float l = dot(c.rgb, vec3(.2126, .7152, .0722));
      c.rgb = mix(vec3(l), c.rgb, sat) * tint;
      vec2 q = vUv - .5; c.rgb *= 1. - vig * dot(q, q) * 1.6;
      gl_FragColor = c;
    }`,
};

/** Right after the scene: copies its depth, as distance from the camera, for the depth overlay
 * (the scene's own depth buffer is written over by the passes that follow). */
class DepthGrabPass extends Pass {
  readonly target = new THREE.WebGLRenderTarget(1, 1, { type: THREE.FloatType, depthBuffer: false });
  private readonly quad = new FullScreenQuad(new THREE.ShaderMaterial({
    uniforms: { tDepth: { value: null }, cameraNear: { value: 0.1 }, cameraFar: { value: 100 } },
    vertexShader: FULLSCREEN_VS,
    fragmentShader: `#include <packing>
      uniform sampler2D tDepth; uniform float cameraNear, cameraFar; varying vec2 vUv;
      void main(){ gl_FragColor = vec4(-perspectiveDepthToViewZ(texture2D(tDepth, vUv).x, cameraNear, cameraFar), 0., 0., 1.); }`,
    depthTest: false, depthWrite: false,
  }));
  constructor(private readonly camera: THREE.PerspectiveCamera) { super(); this.needsSwap = false; this.enabled = false; }
  setSize(w: number, h: number): void { this.target.setSize(w, h); }
  render(renderer: THREE.WebGLRenderer, _write: THREE.WebGLRenderTarget, readBuffer: THREE.WebGLRenderTarget): void {
    const m = this.quad.material as THREE.ShaderMaterial;
    m.uniforms.tDepth.value = readBuffer.depthTexture;
    m.uniforms.cameraNear.value = this.camera.near; m.uniforms.cameraFar.value = this.camera.far;
    renderer.setRenderTarget(this.target);
    this.quad.render(renderer);
  }
}

/** Last, after the grade: a screen-sized picture (Ember's 2D effects canvas) laid over the frame
 * exactly as drawn, except where the scene is nearer the camera than `dist` — so a unit or prop
 * in front of a spell covers it, as in the board's layers. */
const DepthOverlayShader = {
  uniforms: { tDiffuse: { value: null }, tOverlay: { value: null }, tDist: { value: null }, dist: { value: 0 } },
  vertexShader: FULLSCREEN_VS,
  fragmentShader: `uniform sampler2D tDiffuse, tOverlay, tDist; uniform float dist; varying vec2 vUv;
    void main(){
      vec4 c = texture2D(tDiffuse, vUv), o = texture2D(tOverlay, vUv);
      float show = step(dist, texture2D(tDist, vUv).r);
      gl_FragColor = vec4(mix(c.rgb, o.rgb, o.a * show), c.a);
    }`,
};

export class Stage {
  readonly renderer: THREE.WebGLRenderer;
  readonly scene = new THREE.Scene();
  readonly camera: THREE.PerspectiveCamera;
  readonly sun: THREE.DirectionalLight;
  readonly hemi: THREE.HemisphereLight;
  private readonly composer: EffectComposer;
  /** The 3D mist (volumetricFog.ts): off until a mission's Névoa turns it on. */
  readonly mist: VolumetricFogPass;
  private readonly bloom: UnrealBloomPass;
  /** The moon in the night sky; the night presets' key light comes from it. */
  readonly moonDisc = new Moon();
  private readonly tilt: ShaderPass[] = [];
  private readonly depthGrab: DepthGrabPass;
  private readonly depthOverlay = new ShaderPass(DepthOverlayShader);
  private readonly frameHooks: ((dt: number, t: number) => void)[] = [];
  private readonly clock = new THREE.Clock();

  constructor(private readonly host: HTMLElement) {
    this.renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
    // Full screen resolution: quality is never capped to fit a GPU (PLAN.md: no graphics budget).
    this.renderer.setPixelRatio(devicePixelRatio);
    this.renderer.shadowMap.enabled = true;
    // three r186 removed PCFSoftShadowMap; its PCFShadowMap is the soft-filtered one now.
    this.renderer.shadowMap.type = THREE.PCFShadowMap;
    // Neutral keeps colours and saturation as authored (sprites look exactly as painted);
    // it only compresses highlights.
    this.renderer.toneMapping = THREE.NeutralToneMapping;
    host.appendChild(this.renderer.domElement);

    this.scene.background = new THREE.Color('#000000');
    this.camera = new THREE.PerspectiveCamera(30, 1, 0.5, 600);

    this.hemi = new THREE.HemisphereLight('#d4e6ff', '#5d4b33', 1.0);
    this.sun = new THREE.DirectionalLight('#fff0d4', 3.0);
    this.sun.castShadow = true;
    this.sun.shadow.mapSize.set(4096, 4096);
    // three r186 dropped PCFSoftShadowMap: its PCF filter speckled flat ground (shadow acne) with
    // the old bias, so the sun's shadow sits a little further off the surface and is softened
    this.sun.shadow.bias = -0.001;
    this.sun.shadow.normalBias = 0.08;
    this.sun.shadow.radius = 3;
    this.scene.add(this.hemi, this.sun, this.sun.target, this.moonDisc);
    this.moonDisc.setIcon('/game/ui/moon/moon-full-v2.png');

    // Multisampled target: real anti-aliasing survives the post-processing chain.
    const msaa = new THREE.WebGLRenderTarget(1, 1, { type: THREE.HalfFloatType, samples: 4 });
    // the scene's depth, for the 3D mist to place itself in the world per pixel
    msaa.depthTexture = new THREE.DepthTexture(1, 1, THREE.UnsignedIntType);
    this.composer = new EffectComposer(this.renderer, msaa);
    this.composer.addPass(new RenderPass(this.scene, this.camera));
    this.depthGrab = new DepthGrabPass(this.camera);
    this.composer.addPass(this.depthGrab);
    this.mist =new VolumetricFogPass(this.camera, this.scene, this.sun, this.hemi);
    this.composer.addPass(this.mist);
    // gentle: bright fires and spells glow a little, they don't flare (0.3 / 0.9 was too much)
    // threshold 2: surfaces lit by torches stay unbloomed; only the flames themselves (emissive,
    // spriteMaterial FLAME_EMIT) cross it, so the glow sits on the source, not on the lit ground
    this.bloom = new UnrealBloomPass(new THREE.Vector2(1, 1), 0.3, 0.55, 2.0);
    this.composer.addPass(this.bloom);
    for (let i = 0; i < 2; i++) for (const d of [[1, 0], [0, 1]]) {
      const p = new ShaderPass(TiltShiftShader);
      p.uniforms.dir.value.set(d[0], d[1]);
      p.uniforms.amount.value = 2.6 * (i + 1);
      p.enabled = false; // off in the editor by default; T toggles it
      this.composer.addPass(p);
      this.tilt.push(p);
    }
    this.composer.addPass(new ShaderPass(GradeShader));
    this.composer.addPass(new OutputPass());
    this.depthOverlay.enabled = false;
    this.depthOverlay.uniforms.tDist.value = this.depthGrab.target.texture;
    this.composer.addPass(this.depthOverlay);

    this.resizer.observe(host);
    this.resize();
    this.renderer.setAnimationLoop(() => this.frame());
  }

  private readonly resizer = new ResizeObserver(() => this.resize());

  /** Stop drawing and free the GPU context (a panel that hosts the stage is closing). */
  dispose(): void {
    this.renderer.setAnimationLoop(null);
    this.resizer.disconnect();
    this.frameHooks.length = 0;
    this.composer.dispose();
    this.renderer.dispose();
    this.renderer.domElement.remove();
  }

  private half = 20;
  timeOfDay: TimeOfDay = 'day';

  /** Fit the sun and its shadow to a board of the given world size. */
  fitToBoard(width: number, depth: number): void {
    const half = (this.half = Math.max(width, depth) / 2 + 4);
    Object.assign(this.sun.shadow.camera, { left: -half, right: half, top: half, bottom: -half, near: 1, far: half * 6 });
    this.sun.shadow.camera.updateProjectionMatrix();
    this.sun.target.position.set(0, 0, 0);
    this.scene.fog = new THREE.Fog(this.scene.background as THREE.Color, half * 3, half * 7);
    this.setTimeOfDay(this.timeOfDay);
  }

  /** Tonight's moon (the campaign's phase): scales and tints the night presets' moonlight. */
  private moon: MoonLight = { strength: 1 };

  /** Set the moon (see moonPhase.ts's moonlightFor) and re-light; only night presets use it. */
  setMoon(moon: MoonLight): void {
    this.moon = moon;
    this.moonDisc.setIcon(moon.icon ?? '/game/ui/moon/moon-full-v2.png');
    this.setTimeOfDay(this.timeOfDay);
  }

  /** Apply one of Ember's time-of-day presets: sun or moon, sky, background, sprite readability. */
  setTimeOfDay(t: TimeOfDay): void {
    const base = TIME_PRESETS[t] ?? TIME_PRESETS.day;
    const night = t === 'brightNight' || t === 'darkNight';
    // at night the "sun" is the moon: its phase sets how bright (and, for a blood moon, how red)
    const p = night ? {
      ...base,
      sun: this.moon.color ?? base.sun,
      sunIntensity: base.sunIntensity * this.moon.strength,
      sky: this.moon.sky ?? base.sky,
      background: this.moon.background ?? base.background,
      hemi: base.hemi,
    } : base;
    this.timeOfDay = t;
    this.sun.color.set(p.sun);
    this.sun.intensity = p.sunIntensity;
    this.keyIntensity = p.sunIntensity;
    this.placeLight(p.azimuth, p.elevation);
    // at night the key light is the moon: placed (and dimmed near the horizon) every frame as it moves
    // indoors there is no sky: the moon still lights the night, but its disc is never drawn
    this.moonDisc.visible = night && !this.indoor;
    if (night) this.moveMoon();
    this.hemi.color.set(p.sky);
    this.hemi.groundColor.set(p.ground);
    this.hemi.intensity = p.hemi;
    // a painted backdrop (a texture) keeps its own colours; a plain background takes the preset's
    if ((this.scene.background as THREE.Color | null)?.isColor) (this.scene.background as THREE.Color).set('#000000');
    if (this.scene.fog) (this.scene.fog as THREE.Fog).color.set('#000000');
    SPRITE_FLOOR.value = p.spriteFloor;
  }

  private keyIntensity = 3;
  /** Indoor map (Mission.environment): no moon disc in the sky. Set before timeOfDay. */
  indoor = false;
  /** How far tonight's moon is across the sky: 0 rising, 0.5 highest, 1 setting. */
  moonProgress = 0.35;

  private placeLight(azimuth: number, elevation: number): void {
    const d = this.half * 2.2, ce = Math.cos(elevation);
    this.sun.position.set(Math.sin(azimuth) * ce * d, Math.sin(elevation) * d, Math.cos(azimuth) * ce * d);
  }

  /** The moonlight shines from the camera's side of the board, at the disc's height: the disc
   * stands ahead of the view (behind the board), and light from there lit only the backs of
   * units, walls and trees, so every face the player sees got zero moonlight. */
  private moveMoon(): void {
    const pose = Moon.pose(this.moonProgress);
    this.moonDisc.place(pose, this.camera, this.half);
    const disc = this.moonDisc.position;
    this.sun.position.set(-disc.x, disc.y, -disc.z);
    this.sun.intensity = this.keyIntensity * MOON_BRIGHTNESS;
  }

  get tiltShift(): boolean { return this.tilt[0].enabled; }
  set tiltShift(on: boolean) { for (const p of this.tilt) p.enabled = on; }

  onFrame(hook: (dt: number, t: number) => void): void { this.frameHooks.push(hook); }

  /** Lay `picture` (a screen-sized texture, e.g. Ember's 2D effects canvas) over the finished
   * frame, hidden wherever the scene is nearer than `dist` (distance from the camera); null = off. */
  setDepthOverlay(picture: THREE.Texture | null, dist = 0): void {
    this.depthGrab.enabled = this.depthOverlay.enabled = !!picture;
    this.depthOverlay.uniforms.tOverlay.value = picture;
    this.depthOverlay.uniforms.dist.value = dist;
  }

  private resize(): void {
    const w = Math.max(1, this.host.clientWidth), h = Math.max(1, this.host.clientHeight);
    this.renderer.setSize(w, h);
    this.composer.setSize(w, h);
    this.composer.setPixelRatio(this.renderer.getPixelRatio());
    this.depthGrab.setSize(Math.floor(w * this.renderer.getPixelRatio()), Math.floor(h * this.renderer.getPixelRatio()));
    this.bloom.resolution.set(w, h);
    for (const p of this.tilt) p.uniforms.res.value.set(w, h);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  }

  /** Compile spell/actor materials while loading, including currently hidden effects. */
  async prepareMaterials(onProgress?: (ready: number, total: number) => void): Promise<void> {
    const parked = this.parkHiddenLights();
    this.balanceLightCount();
    try {
      this.scene.traverse(object => {
        const materials = (object as THREE.Mesh).material;
        if (!materials) return;
        for (const material of Array.isArray(materials) ? materials : [materials]) {
          for (const value of Object.values(material)) if ((value as THREE.Texture)?.isTexture) this.renderer.initTexture(value as THREE.Texture);
        }
      });
      const compiling = this.renderer.compileAsync(this.scene, this.camera);
      if (onProgress) {
        // compileAsync has just created every program it waits on: report how many the GPU
        // driver has finished (WebGLProgram.isReady, a non-blocking query) once per frame.
        const programs = [...(this.renderer.info.programs ?? [])] as unknown as { isReady(): boolean }[];
        let compiled = false;
        const poll = () => {
          if (compiled) return;
          let ready = 0;
          for (const program of programs) if (program.isReady()) ready++;
          onProgress(ready, programs.length);
          requestAnimationFrame(poll);
        };
        poll();
        const done = () => { compiled = true; onProgress(programs.length, programs.length); };
        compiling.then(done, done);
      }
      await compiling;
    } finally { this.restoreParkedLights(parked); }
  }

  private frame(): void {
    const dt = Math.min(this.clock.getDelta(), 0.05), t = this.clock.elapsedTime;
    for (const h of this.frameHooks) h(dt, t);
    if (this.moonDisc.visible) {
      this.moonProgress = (this.moonProgress + dt / MOON_CROSSING_SECONDS) % 1;
      this.moveMoon();
    }
    // Ember's ThreeBattleRenderer fix, carried over: spell effects, fog of war and hidden units
    // switch point lights on and off, and any change in the number of lights recompiles every
    // lit material on screen (a visible stall or flash when a spell starts or ends). For the
    // draw, hidden lights count as present but dark and filler lights pad the total to a fixed
    // budget, so the count the shaders see never changes; their own state is restored after.
    const parked = this.parkHiddenLights();
    this.balanceLightCount();
    this.drawMissingShadowMaps();
    try { this.composer.render(); } finally { this.restoreParkedLights(parked); }
  }

  /** three r186 samples every shadow-casting light's map as a depth texture. A light whose
   * shadow updates are off (filler and parked lights, idle spell lights) and that never drew
   * one left an empty slot, so every lit material failed its draw (GL_INVALID_OPERATION) and
   * the whole scene vanished. Each such light draws its map once. */
  private drawMissingShadowMaps(): void {
    this.scene.traverseVisible((o) => {
      const light = o as THREE.PointLight;
      if (light.isLight && light.castShadow && light.shadow && !light.shadow.map) light.shadow.needsUpdate = true;
    });
  }

  private lightBudget: { point: number; shadow: number } | null = null;
  private readonly fillerLights: { point: THREE.PointLight[]; shadow: THREE.PointLight[] } = { point: [], shadow: [] };

  /** Pads the drawn point lights up to a fixed budget with dark filler lights (Ember's
   * balanceLightCount). Grows, with one recompile, only if a battle ever needs more than the
   * first frame's count plus headroom. */
  private balanceLightCount(): void {
    for (const light of [...this.fillerLights.point, ...this.fillerLights.shadow]) light.visible = false;
    let point = 0, shadow = 0;
    this.scene.traverseVisible((o) => {
      if (!(o as THREE.PointLight).isPointLight) return;
      if ((o as THREE.PointLight).castShadow) shadow++; else point++;
    });
    if (!this.lightBudget) this.lightBudget = { point: point + LIGHT_BUDGET_HEADROOM, shadow: shadow + SHADOW_LIGHT_BUDGET_HEADROOM };
    this.lightBudget.point = Math.max(this.lightBudget.point, point);
    this.lightBudget.shadow = Math.max(this.lightBudget.shadow, shadow);
    const fill = (have: number, budget: number, pool: THREE.PointLight[], castsShadow: boolean) => {
      for (let i = 0; i < budget - have; i++) {
        let light = pool[i];
        if (!light) {
          light = new THREE.PointLight(0xffffff, 0, 1, 2);
          if (castsShadow) { light.castShadow = true; light.shadow.mapSize.set(16, 16); light.shadow.autoUpdate = false; }
          pool.push(light);
          this.scene.add(light);
        }
        light.visible = true;
      }
    };
    fill(point, this.lightBudget.point, this.fillerLights.point, false);
    fill(shadow, this.lightBudget.shadow, this.fillerLights.shadow, true);
  }

  /** Hidden lights become visible but dark for the draw (Ember's parkHiddenLights). */
  private parkHiddenLights(): { light: THREE.Light; intensity: number; autoUpdate: boolean }[] {
    const parked: { light: THREE.Light; intensity: number; autoUpdate: boolean }[] = [];
    this.scene.traverse((child) => {
      const light = child as THREE.Light;
      if (!light.isLight || light.visible || (light as THREE.AmbientLight).isAmbientLight || (light as THREE.HemisphereLight).isHemisphereLight) return;
      if (this.fillerLights.point.includes(light as THREE.PointLight) || this.fillerLights.shadow.includes(light as THREE.PointLight)) return;
      const shadow = (light as THREE.PointLight).shadow;
      parked.push({ light, intensity: light.intensity, autoUpdate: shadow ? shadow.autoUpdate : true });
      light.visible = true;
      light.intensity = 0;
      if (shadow) shadow.autoUpdate = false;
    });
    return parked;
  }

  private restoreParkedLights(parked: { light: THREE.Light; intensity: number; autoUpdate: boolean }[]): void {
    for (const { light, intensity, autoUpdate } of parked) {
      light.visible = false;
      light.intensity = intensity;
      const shadow = (light as THREE.PointLight).shadow;
      if (shadow) shadow.autoUpdate = autoUpdate;
    }
  }
}

/** Spare room in the fixed point-light budget for spell lights (Ember's values). */
const LIGHT_BUDGET_HEADROOM = 6;
const SHADOW_LIGHT_BUDGET_HEADROOM = 3;
