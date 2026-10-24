import * as THREE from "three";
import { ELEMENT_LABELS, PLACEABLE_ELEMENT_KINDS, type PlaceableElementKind } from "../params";
import { DEFAULT_FIRE_EMITTER, loadFireFlipbook, ParticleEmitter, type FireEmitterSettings } from "./ThreeVfxSystem";
import { loadElementFlipbook, type FlipbookElement, type FlipbookLayer } from "./ElementFlipbookAtlas";
import type { EnvLight } from "../../lighting";

export type PixelElement = "fire" | FlipbookElement;
export type PixelElementSettings = {
  scale: number; intensity: number; density: number; spawnRate: number; particleCount: number;
  lifetime: number; duration: number; velocity: number; verticalForce: number; spread: number;
  drag: number; turbulence: number; rotation: number; emissive: number; opacity: number;
  lightEnabled: boolean; lightIntensity: number; lightRadius: number; lightDecay: number;
  flickerAmount: number; flickerSpeed: number; animationSpeed: number; seed: number; loop: boolean;
  visualsEnabled: boolean;
};
export type PixelElementPreset = {
  id: string; family: "procedural_pixel"; element: PixelElement; version: 1 | 2;
  label: string; color: number; light: number; radius: number; flicker: number;
  motion: "fire" | "frost" | "electric" | "poison" | "arcane" | "holy" | "shadow" | "ember";
};

const pixelEntries: PixelElementPreset[] = [
  { id:"procedural_pixel_fire",family:"procedural_pixel",element:"fire",version:1,label:"Procedural Pixel Fire",color:0xff7624,light:1,radius:3.2,flicker:0.36,motion:"fire" },
  { id:"procedural_pixel_frost",family:"procedural_pixel",element:"frost",version:1,label:"Procedural Pixel Frost",color:0x79e7ff,light:1.35,radius:2.1,flicker:0.1,motion:"frost" },
  { id:"procedural_pixel_lightning",family:"procedural_pixel",element:"lightning",version:1,label:"Procedural Pixel Lightning",color:0x9ccaff,light:2.3,radius:2.7,flicker:0.75,motion:"electric" },
  { id:"procedural_pixel_poison",family:"procedural_pixel",element:"poison",version:1,label:"Procedural Pixel Poison",color:0xa4ef39,light:0.7,radius:1.8,flicker:0.18,motion:"poison" },
  { id:"procedural_pixel_arcane",family:"procedural_pixel",element:"arcane",version:1,label:"Procedural Pixel Arcane",color:0xbd7aff,light:1.8,radius:2.25,flicker:0.26,motion:"arcane" },
  { id:"procedural_pixel_holy",family:"procedural_pixel",element:"holy",version:1,label:"Procedural Pixel Holy",color:0xffd875,light:1.8,radius:2.25,flicker:0.08,motion:"holy" },
  { id:"procedural_pixel_shadow",family:"procedural_pixel",element:"shadow",version:1,label:"Procedural Pixel Shadow",color:0x9b66cb,light:0.55,radius:1.8,flicker:0.2,motion:"shadow" },
  { id:"procedural_pixel_ember",family:"procedural_pixel",element:"ember",version:1,label:"Procedural Pixel Ember",color:0xf23943,light:0.9,radius:1.65,flicker:0.3,motion:"ember" },
];
const pixelV2Entries = pixelEntries.map((entry): PixelElementPreset => ({
  ...entry, id:`procedural_pixel_v2_${entry.element}`, version:2, label:`${entry.label} V2`,
}));

export const PIXEL_ELEMENT_IDS = pixelEntries.map((entry) => entry.element) as readonly PixelElement[];
export const PIXEL_ELEMENT_PRESETS: readonly PixelElementPreset[] = Object.freeze([...pixelEntries, ...pixelV2Entries]);
export const pixelPresetsFor = (element: PixelElement): readonly PixelElementPreset[] => PIXEL_ELEMENT_PRESETS.filter((entry) => entry.element === element).sort((a,b) => b.version-a.version);
export const pixelPreset = (element: PixelElement, presetId?: string): PixelElementPreset =>
  PIXEL_ELEMENT_PRESETS.find((entry) => entry.element === element && (!presetId || entry.id === presetId)) ??
  PIXEL_ELEMENT_PRESETS.find((entry) => entry.element === element && entry.version === 1)!;

export const DEFAULT_PIXEL_SETTINGS: PixelElementSettings = {
  scale:1,intensity:1,density:1,spawnRate:1,particleCount:40,lifetime:1.8,duration:0,
  velocity:0.55,verticalForce:0.12,spread:0.42,drag:0.4,turbulence:0.35,rotation:1,
  emissive:2,opacity:1,lightEnabled:true,lightIntensity:1.3,lightRadius:1,lightDecay:2,
  flickerAmount:0.2,flickerSpeed:5,animationSpeed:1,seed:713,loop:true,visualsEnabled:true,
};

const PIXEL_DEFAULTS: Record<PixelElement, Partial<PixelElementSettings>> = {
  // Fire keeps its existing settings and rendering path. The other values tune the shared flame
  // particle simulation for the silhouettes and pacing in each supplied atlas.
  fire: { particleCount:26, lifetime:1, velocity:1.02, verticalForce:0.12, spread:1, drag:1.25, turbulence:0.65, emissive:1.15, lightIntensity:1, lightRadius:1, flickerAmount:0.36, flickerSpeed:23 },
  frost: { particleCount:28, lifetime:1.8, velocity:0.78, verticalForce:0.02, spread:0.66, turbulence:0.22, emissive:1.35, lightIntensity:1.2, lightRadius:1, flickerAmount:0.1, flickerSpeed:3 },
  lightning: { particleCount:20, lifetime:0.44, velocity:1.15, verticalForce:0.02, spread:0.58, turbulence:1.0, emissive:1.9, lightIntensity:1.5, lightRadius:1, flickerAmount:0.65, flickerSpeed:28 },
  poison: { particleCount:25, lifetime:2.0, velocity:0.5, verticalForce:0.08, spread:0.58, turbulence:0.2, emissive:1.2, lightIntensity:0.7, lightRadius:0.9, flickerAmount:0.14, flickerSpeed:4 },
  arcane: { particleCount:26, lifetime:1.8, velocity:0.68, verticalForce:0.06, spread:0.52, turbulence:0.3, emissive:1.65, lightIntensity:1.3, lightRadius:1, flickerAmount:0.2, flickerSpeed:4 },
  holy: { particleCount:24, lifetime:1.8, velocity:0.62, verticalForce:0.09, spread:0.5, turbulence:0.16, emissive:1.5, lightIntensity:1.2, lightRadius:1, flickerAmount:0.08, flickerSpeed:2 },
  shadow: { particleCount:26, lifetime:1.9, velocity:0.54, verticalForce:0.04, spread:0.62, turbulence:0.25, emissive:0.92, lightIntensity:0.55, lightRadius:0.8, flickerAmount:0.18, flickerSpeed:4 },
  ember: { particleCount:24, lifetime:2.1, velocity:0.58, verticalForce:0.1, spread:0.6, turbulence:0.4, emissive:1.25, lightIntensity:0.72, lightRadius:0.75, flickerAmount:0.26, flickerSpeed:7 },
};
const PIXEL_V2_DEFAULTS: Partial<Record<PixelElement, Partial<PixelElementSettings>>> = {
  // Holy V2 is a compact, steady light column rooted at its halo seal; it should not billow like smoke.
  holy: { particleCount:12, lifetime:0.72, velocity:0.16, verticalForce:0.02, spread:0.08, drag:1.4, turbulence:0.02, emissive:1.05, opacity:0.7, animationSpeed:0.8 },
  fire: { particleCount:12, lifetime:0.8, velocity:0.18, verticalForce:0.02, spread:0.08, drag:1.35, turbulence:0.03, emissive:1.15, opacity:0.9, animationSpeed:0.95 },
};
export const pixelDefaults = (element: PixelElement, version: 1 | 2 = 1): PixelElementSettings => ({ ...DEFAULT_PIXEL_SETTINGS, ...PIXEL_DEFAULTS[element], ...(version === 2 ? PIXEL_V2_DEFAULTS[element] : {}) });

/** Registry drives the editor's family, element and preset selectors. Old placements have no
 * family field and remain on their original EffectsRenderer path. */
export const ELEMENT_FX_REGISTRY = Object.freeze([
  ...PLACEABLE_ELEMENT_KINDS.map((element: PlaceableElementKind) => ({ family:"regular" as const, element, preset:`regular_${element}`, label:ELEMENT_LABELS[element], factory:"EffectsRenderer" as const, defaults:{} })),
  ...pixelEntries.map((entry) => ({ ...entry, factory:"ProceduralElementEmitter" as const, defaults:pixelDefaults(entry.element) })),
]);

const ELEMENT_TINTS: Record<FlipbookElement, readonly [number, number, number]> = {
  fire: [1, 0.84, 0.66],
  frost: [0.84, 0.96, 1],
  lightning: [0.84, 0.92, 1],
  poison: [0.9, 1, 0.82],
  arcane: [0.94, 0.86, 1],
  holy: [1, 0.96, 0.84],
  shadow: [0.9, 0.84, 1],
  ember: [1, 0.9, 0.84],
};

/** V1 atlas elements reuse the flame particle system. V2 atlases are composed as authored sprites
 * so each complete effect remains grounded while all three animation sheets stay visible. */
export class ProceduralElementEmitter {
  readonly group = new THREE.Group();
  private readonly preset: PixelElementPreset;
  private settings: PixelElementSettings;
  private time = 0;
  private seed: number;
  private lightPriority = true;
  private lightActivity = 0;
  private active = true;
  private disposed = false;
  private particleEmitter: ParticleEmitter | null = null;
  private particleSettings: FireEmitterSettings | null = null;
  private readonly elementEmitters: ParticleEmitter[] = [];
  private readonly v2Sprites: { sprite: THREE.Sprite; texture: THREE.Texture; phase: number; opacity: number; rotation: number }[] = [];
  private elementLayerSettings: FireEmitterSettings[] = [];
  private fireEmitter: ParticleEmitter | null = null;
  private fireTexture: THREE.Texture | null = null;
  private fireSettings: FireEmitterSettings | null = null;
  private fireDt = 0;

  constructor(private readonly scene:THREE.Object3D, preset:PixelElementPreset, settings:Partial<PixelElementSettings>={}) {
    this.preset = preset;
    this.settings = { ...pixelDefaults(preset.element, preset.version), ...settings };
    this.seed = this.settings.seed >>> 0 || 1;
    this.group.scale.setScalar(this.settings.scale);
    this.scene.add(this.group);
    if (preset.element === "fire" && preset.version === 1) {
      void loadFireFlipbook().then((texture) => {
        if (this.disposed) { texture.dispose(); return; }
        this.fireTexture = texture;
        this.fireEmitter = new ParticleEmitter(texture, null, {
          sceneNear:0.1, sceneFar:2000, viewport:new THREE.Vector2(1,1), intensity:this.settings.emissive*this.settings.intensity,
        });
        this.fireEmitter.setSeed(this.seed);
        this.fireSettings = this.toFireSettings();
        this.fireEmitter.reset(this.fireSettings, true);
        this.group.add(this.fireEmitter.mesh);
      }).catch(() => { /* Keep the rest of the family available if the fire atlas cannot load. */ });
      return;
    }
    const element = preset.element as FlipbookElement;
    const layers: FlipbookLayer[] = ["main", "secondary", "particles"];
    void Promise.all(layers.map((layer) => loadElementFlipbook(element, layer, preset.version))).then((textures) => {
      if (this.disposed) { textures.forEach((texture) => texture.dispose()); return; }
      const [r, g, b] = ELEMENT_TINTS[element];
      if (preset.version === 2) {
        this.createElementV2Sprites(element, textures);
        return;
      }
      this.elementEmitters.push(...textures.map((texture, index) => {
        const layerCurves = index === 0 ? {
          opacity: (age: number) => Math.min(1, age * 7) * Math.pow(1 - age, 0.8) * this.settings.opacity,
          color: (_age: number, color: THREE.Color) => color.setRGB(r, g, b),
        } : index === 1 ? {
          size: (age: number) => (0.42 + Math.sin(Math.PI * (0.12 + age * 0.76)) * 0.48) * 0.82,
          opacity: (age: number) => Math.min(1, age * 5) * Math.pow(1 - age, 0.9) * this.settings.opacity * 0.46,
          emissive: (age: number) => (1.7 - age * 0.85) * 0.78,
          color: (_age: number, color: THREE.Color) => color.setRGB(r, g, b),
        } : {
          size: (age: number) => (0.42 + Math.sin(Math.PI * (0.12 + age * 0.76)) * 0.48) * 0.46,
          opacity: (age: number) => Math.min(1, age * 8) * Math.pow(1 - age, 1.35) * this.settings.opacity * 0.85,
          emissive: (age: number) => (1.7 - age * 0.85) * 1.2,
          velocity: (age: number) => 1.18 - age * 0.6,
          color: (_age: number, color: THREE.Color) => color.setRGB(r, g, b),
        };
        const emitter = new ParticleEmitter(texture, null, {
          sceneNear:0.1, sceneFar:2000, viewport:new THREE.Vector2(1,1), intensity:this.settings.emissive*this.settings.intensity,
        }, layerCurves);
        emitter.light.color.setHex(preset.color);
        emitter.setSeed((this.seed + index * 7919) >>> 0);
        this.group.add(emitter.mesh);
        return emitter;
      }));
      this.particleEmitter = this.elementEmitters[0] ?? null;
      this.refreshElementLayers(true);
    }).catch((error) => console.error(`Element flipbook failed to load: ${preset.element}`, error));
  }

  setSettings(settings:Partial<PixelElementSettings>):void {
    this.settings = { ...this.settings, ...settings };
    if (this.preset.element === "fire" && this.preset.version === 1 && this.fireEmitter) {
      const nextSeed = this.settings.seed >>> 0 || 1;
      const nextFireSettings = this.toFireSettings();
      if (this.fireSettings?.particleCount !== nextFireSettings.particleCount || this.fireSettings?.lifetimeScale !== nextFireSettings.lifetimeScale || nextSeed !== this.seed) {
        this.seed = nextSeed;
        this.fireEmitter.setSeed(this.seed);
        this.fireEmitter.reset(nextFireSettings, true);
      }
      this.fireSettings = nextFireSettings;
      this.fireEmitter.material.uniforms.uIntensity!.value = this.settings.emissive * this.settings.intensity;
      return;
    }
    if (this.particleEmitter) this.refreshElementLayers();
  }

  update(dt:number,tile:number,time:number,x=0,y=0):void {
    if (this.disposed) return;
    const s=this.settings;
    this.time=time*s.animationSpeed;
    const active=s.loop||s.duration<=0||time<s.duration;
    this.active=active;
    this.group.position.set(x,-y,0);
    this.group.scale.setScalar(Math.max(1,tile)*s.scale);
    if (this.preset.element === "fire" && this.preset.version === 1) {
      this.fireDt += Math.max(0,Math.min(dt,0.08));
      if (this.fireEmitter && this.fireSettings) {
        this.fireEmitter.mesh.position.set(0,0,0.035);
        // The containing group already scales the FX Lab's unit-space particles to one hex.
        this.fireEmitter.mesh.visible=active&&s.visualsEnabled;
        this.fireEmitter.light.visible=false;
        this.fireEmitter.update(dt,this.toFireSettings(tile),s.loop&&active);
        this.lightActivity=active&&s.lightEnabled&&this.lightPriority?this.fireEmitter.light.intensity:0;
      } else this.lightActivity=0;
      return;
    }
    if (this.v2Sprites.length) {
      const frame = Math.floor(this.time * 12) % 16;
      for (const layer of this.v2Sprites) {
        const currentFrame = (frame + layer.phase) % 16;
        layer.texture.offset.set((currentFrame % 4) * 0.25, 1 - (Math.floor(currentFrame / 4) + 1) * 0.25);
        layer.sprite.material.opacity = layer.opacity * s.opacity;
        layer.sprite.material.color.setScalar(s.emissive * s.intensity);
        layer.sprite.rotation.z = layer.rotation + Math.sin(this.time * 1.8 + layer.phase) * 0.035;
        layer.sprite.visible = active && s.visualsEnabled;
      }
      this.lightActivity=active&&s.lightEnabled&&this.lightPriority?5.5*this.preset.light*s.lightIntensity*s.intensity:0;
      return;
    }
    if (!this.particleEmitter) { this.lightActivity=0; return; }
    const baseSettings = this.toElementSettings(tile);
    const layerSettings = [
      baseSettings,
      { ...baseSettings, particleCount:Math.max(8,Math.round(baseSettings.particleCount*0.58)), particleScale:baseSettings.particleScale*0.68, velocity:baseSettings.velocity*0.76, spread:(baseSettings.spread ?? 1)*1.25, turbulence:baseSettings.turbulence*1.18, lifetimeScale:baseSettings.lifetimeScale!*1.12 },
      { ...baseSettings, particleCount:Math.max(8,Math.round(baseSettings.particleCount*0.38)), particleScale:baseSettings.particleScale*0.36, velocity:baseSettings.velocity*1.28, spread:(baseSettings.spread ?? 1)*1.5, turbulence:baseSettings.turbulence*1.4, lifetimeScale:baseSettings.lifetimeScale!*0.68 },
    ];
    this.elementEmitters.forEach((emitter, index) => {
      emitter.mesh.position.set(0,0,0.035 + index * 0.002);
      emitter.mesh.visible=active&&s.visualsEnabled;
      emitter.light.visible=false;
      emitter.update(dt,layerSettings[index]!,s.loop&&active);
    });
    this.elementLayerSettings = layerSettings;
    this.particleSettings = layerSettings[0]!;
    this.lightActivity=active&&s.lightEnabled&&this.lightPriority?this.particleEmitter.light.intensity:0;
  }

  getLightSample(x:number,y:number,tile:number):EnvLight|null {
    if(!this.active||!this.settings.lightEnabled||!this.lightPriority||this.lightActivity<=0)return null;
    const c=new THREE.Color(this.preset.color);
    const intensity=this.lightActivity/(4.9*Math.pow(Math.max(1,tile),1.5));
    return {x,y,h:Math.max(1,tile)*0.28,r:this.settings.lightRadius*this.preset.radius*Math.max(1,tile),decay:this.settings.lightDecay,rgb:[c.r*intensity,c.g*intensity,c.b*intensity]};
  }

  dispose():void {
    if(this.disposed)return;
    this.disposed=true;
    this.scene.remove(this.group);
    this.fireEmitter?.geometry.dispose();
    this.fireEmitter?.material.dispose();
    this.fireEmitter?.light.removeFromParent();
    this.fireTexture?.dispose();
    this.particleEmitter?.geometry.dispose();
    this.particleEmitter?.material.dispose();
    for (const emitter of this.elementEmitters.slice(1)) {
      emitter.geometry.dispose();
      emitter.material.dispose();
    }
    for (const layer of this.v2Sprites) {
      layer.sprite.material.dispose();
      layer.texture.dispose();
    }
  }
  setLightPriority(enabled:boolean):void{this.lightPriority=enabled;}

  private toFireSettings(tile=1):FireEmitterSettings {
    const s=this.settings;
    return {
      particleCount:Math.max(12,Math.min(96,Math.round(s.particleCount*s.density*s.spawnRate))),
      particleScale:s.scale*tile*DEFAULT_FIRE_EMITTER.particleScale,velocity:s.velocity,
      drag:s.drag,turbulence:s.turbulence,gravity:DEFAULT_FIRE_EMITTER.gravity,
      flipbookFps:DEFAULT_FIRE_EMITTER.flipbookFps*s.animationSpeed,
      coreIntensity:s.emissive,lightIntensity:DEFAULT_FIRE_EMITTER.lightIntensity*s.lightIntensity*s.intensity,
      lightRadius:DEFAULT_FIRE_EMITTER.lightRadius*s.lightRadius,lifetimeScale:s.lifetime,spread:s.spread,
    };
  }

  private toElementSettings(tile=1):FireEmitterSettings {
    return { ...this.toFireSettings(tile), gravity:Math.max(0,DEFAULT_FIRE_EMITTER.gravity-this.settings.verticalForce) };
  }

  private refreshElementLayers(forceReset = false):void {
    const nextSeed = this.settings.seed >>> 0 || 1;
    const base = this.toElementSettings();
    const nextLayers = [
      base,
      { ...base, particleCount:Math.max(8,Math.round(base.particleCount*0.58)), particleScale:base.particleScale*0.68, velocity:base.velocity*0.76, spread:(base.spread ?? 1)*1.25, turbulence:base.turbulence*1.18, lifetimeScale:base.lifetimeScale!*1.12 },
      { ...base, particleCount:Math.max(8,Math.round(base.particleCount*0.38)), particleScale:base.particleScale*0.36, velocity:base.velocity*1.28, spread:(base.spread ?? 1)*1.5, turbulence:base.turbulence*1.4, lifetimeScale:base.lifetimeScale!*0.68 },
    ];
    const reset = forceReset || this.particleSettings?.particleCount !== base.particleCount || this.particleSettings?.lifetimeScale !== base.lifetimeScale || nextSeed !== this.seed;
    if (nextSeed !== this.seed) this.seed = nextSeed;
    this.elementEmitters.forEach((emitter, index) => {
      if (reset) {
        emitter.setSeed((this.seed + index * 7919) >>> 0);
        emitter.reset(nextLayers[index]!, true);
      }
      emitter.material.uniforms.uIntensity!.value = this.settings.emissive * this.settings.intensity;
    });
    this.elementLayerSettings = nextLayers;
    this.particleSettings = base;
  }

  /** Compose the three authored V2 atlases as one floor-rooted effect, its ground layer and
   * several independent motes. The complete main frames no longer get scattered as smoke. */
  private createElementV2Sprites(element: FlipbookElement, textures: THREE.Texture[]):void {
    const profiles: Record<FlipbookElement, { width:number; height:number; groundWidth:number; groundHeight:number; mainY:number; motes:readonly [number,number,number,number][] }> = {
      fire: { width:0.98,height:1.22,groundWidth:0.96,groundHeight:0.3,mainY:-0.02,motes:[[-0.3,0.32,0.15,1],[0.28,0.49,0.14,5],[-0.2,0.72,0.12,9],[0.22,0.91,0.1,13]] },
      frost: { width:0.92,height:1.2,groundWidth:1.02,groundHeight:0.32,mainY:-0.08,motes:[[-0.3,0.33,0.16,1],[0.29,0.49,0.14,5],[-0.2,0.72,0.12,9],[0.22,0.91,0.1,13]] },
      lightning: { width:0.82,height:1.32,groundWidth:0.9,groundHeight:0.28,mainY:-0.02,motes:[[-0.26,0.4,0.14,1],[0.24,0.58,0.12,5],[-0.18,0.8,0.1,9],[0.2,1.02,0.1,13]] },
      poison: { width:1.02,height:1.16,groundWidth:1.08,groundHeight:0.36,mainY:-0.05,motes:[[-0.32,0.3,0.15,1],[0.3,0.45,0.15,5],[-0.2,0.67,0.13,9],[0.23,0.88,0.12,13]] },
      arcane: { width:1.04,height:1.22,groundWidth:1.06,groundHeight:0.34,mainY:0,motes:[[-0.3,0.34,0.14,1],[0.29,0.52,0.13,5],[-0.2,0.75,0.12,9],[0.22,0.96,0.11,13]] },
      holy: { width:1.0,height:1.28,groundWidth:0.96,groundHeight:0.34,mainY:-0.23,motes:[[-0.28,0.32,0.15,1],[0.28,0.48,0.13,5],[-0.2,0.73,0.12,9],[0.23,0.88,0.1,13]] },
      shadow: { width:1.08,height:1.15,groundWidth:1.08,groundHeight:0.38,mainY:0,motes:[[-0.31,0.28,0.15,1],[0.29,0.45,0.13,5],[-0.21,0.69,0.12,9],[0.22,0.88,0.1,13]] },
      ember: { width:1.08,height:1.18,groundWidth:0.98,groundHeight:0.3,mainY:-0.12,motes:[[-0.3,0.34,0.15,1],[0.3,0.5,0.14,5],[-0.2,0.74,0.12,9],[0.22,0.94,0.1,13]] },
    };
    const profile = profiles[element];
    const add = (source: THREE.Texture, x:number, y:number, z:number, width:number, height:number, phase:number, opacity:number, rotation=0, floorAnchored=false) => {
      const texture=source.clone();
      texture.repeat.set(0.25,0.25);
      texture.offset.set((phase%4)*0.25,1-(Math.floor(phase/4)+1)*0.25);
      texture.needsUpdate=true;
      const material=new THREE.SpriteMaterial({ map:texture, color:0xffffff, transparent:true, opacity, alphaTest:0.02, depthWrite:false, blending:THREE.AdditiveBlending, toneMapped:false });
      const sprite=new THREE.Sprite(material);
      sprite.center.set(0.5,floorAnchored?0:0.5);
      sprite.scale.set(width,height,1);
      sprite.position.set(x,y,z);
      sprite.rotation.z=rotation;
      this.group.add(sprite);
      this.v2Sprites.push({sprite,texture,phase,opacity,rotation});
    };
    // The first sheet's complete frames are bottom-anchored to the map floor.
    add(textures[0]!,0,profile.mainY,0.035,profile.width,profile.height,0,0.88,0,true);
    // The second sheet is a compact ground plane halo, seal, puddle or impact ring.
    add(textures[1]!,0,0.025,0.04,profile.groundWidth,profile.groundHeight,3,0.74);
    // Animate four independently placed particles from the third sheet around the plume.
    profile.motes.forEach(([x,y,size,phase],index) => {
      add(textures[2]!,x,y,0.045+index*0.001,size,size,phase,0.9-index*0.055,(index%2?1:-1)*0.1);
    });
  }
}
