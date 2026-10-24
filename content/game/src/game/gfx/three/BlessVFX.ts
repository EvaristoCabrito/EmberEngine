import * as THREE from "three";

export interface BlessVfxAlly { id: string; position: THREE.Vector3; distanceHexes: number }
export interface BlessVfxCast {
  id: string;
  center: THREE.Vector3;
  radiusWorld: number;
  allies: BlessVfxAlly[];
  onApply: (unitId: string) => void;
  onComplete: () => void;
  onTimelineEvent?: (event: BlessVfxTimelineEvent, unitId?: string) => void;
}
export type BlessVfxTimelineEvent = "bless_charge" | "bless_release" | "bless_wave" | "bless_unit_receive" | "bless_absorb" | "bless_complete";

export interface BlessVfxSettings {
  waveSpeed: number; waveRadius: number; waveHeight: number; waveThickness: number; waveTurbulence: number;
  casterParticles: number; allyParticles: number; strandCount: number; strandThickness: number; strandHeight: number;
  strandCurvature: number; absorptionSpeed: number; emissive: number; casterLight: number; casterLightRadius: number;
  allyLight: number; allyLightRadius: number; lightDecay: number; duration: number; geometry: boolean; particles: boolean;
  emissiveEnabled: boolean; lights: boolean; bloom: boolean; seed: number;
}

export const DEFAULT_BLESS_VFX_SETTINGS: BlessVfxSettings = {
  waveSpeed: 1, waveRadius: 1, waveHeight: 0.42, waveThickness: 0.12, waveTurbulence: 0.22,
  casterParticles: 32, allyParticles: 28, strandCount: 8, strandThickness: 0.025, strandHeight: 0.85,
  strandCurvature: 0.42, absorptionSpeed: 1, emissive: 4.5, casterLight: 8, casterLightRadius: 5.2,
  allyLight: 3.5, allyLightRadius: 2.4, lightDecay: 1, duration: 1.5, geometry: true, particles: true,
  emissiveEnabled: true, lights: true, bloom: true, seed: 31037,
};

const SETTINGS_KEY = "emberash:bless-vfx-settings";
export function getActiveBlessVfxSettings(): BlessVfxSettings {
  if (typeof window === "undefined") return { ...DEFAULT_BLESS_VFX_SETTINGS };
  try { return { ...DEFAULT_BLESS_VFX_SETTINGS, ...JSON.parse(window.localStorage.getItem(SETTINGS_KEY) ?? "{}") as Partial<BlessVfxSettings> }; }
  catch { return { ...DEFAULT_BLESS_VFX_SETTINGS }; }
}
export function setActiveBlessVfxSettings(settings: BlessVfxSettings): void {
  try { if (typeof window !== "undefined") window.localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch { /* storage may be unavailable */ }
}

const WAVE_VERTEX = /* glsl */ `
  uniform float uTime; uniform float uRadius; uniform float uThickness; uniform float uNoise; uniform float uHeight;
  varying float vEnergy;
  void main(){
    float a = atan(position.y, position.x);
    float n = sin(a*11.0 + uTime*7.0)*0.48 + sin(a*23.0-uTime*4.0)*0.22 + cos(a*37.0+uTime*3.0)*0.12;
    vec3 p = position;
    p.xy *= uRadius + n*uNoise;
    p.z += sin(a*9.0-uTime*6.0)*uNoise*0.16 + sin(a*7.0+uTime*3.0)*uHeight*0.12;
    vEnergy = 0.52 + 0.48*sin(a*13.0-uTime*8.0+n*4.0);
    gl_Position = projectionMatrix*modelViewMatrix*vec4(p,1.0);
  }`;
const WAVE_FRAGMENT = /* glsl */ `
  uniform float uOpacity; uniform float uEmissive; uniform float uTime; uniform float uThickness;
  varying float vEnergy;
  void main(){ float streak=smoothstep(0.26,0.88,vEnergy); vec3 amber=vec3(1.0,0.28,0.025); vec3 gold=vec3(1.0,0.67,0.16); vec3 ivory=vec3(1.0,0.91,0.62); vec3 c=mix(amber,gold,streak); c=mix(c,ivory,pow(streak,5.0)); float flicker=0.76+0.24*sin(uTime*15.0+vEnergy*8.0); gl_FragColor=vec4(c*uEmissive*flicker,uOpacity*(0.35+streak*0.65)); }`;

type Strand = { mesh: THREE.Mesh<THREE.TubeGeometry, THREE.MeshStandardMaterial>; unitId: string; phase: number; seed: number };
type Reception = { ally: BlessVfxAlly; light: THREE.PointLight; received: boolean; startedAt: number; absorbEventSent: boolean };

/** A pooled, self-cleaning party-wide Bless wave. Coordinates match Battle's XY ground plane. */
export class BlessVFX {
  private readonly group = new THREE.Group();
  private readonly ring: THREE.Mesh<THREE.BufferGeometry, THREE.ShaderMaterial>;
  private readonly core: THREE.Mesh<THREE.IcosahedronGeometry, THREE.MeshStandardMaterial>;
  private readonly centerLight = new THREE.PointLight(0xffbd50, 0, 5.2, 1.8);
  private readonly particleGeometry = new THREE.IcosahedronGeometry(0.045, 1);
  private readonly particleMaterial = new THREE.MeshStandardMaterial({ color: 0xffcf67, emissive: 0xffae36, emissiveIntensity: 3.4, roughness: 0.42, metalness: 0.05 });
  private particles: THREE.InstancedMesh<THREE.IcosahedronGeometry, THREE.MeshStandardMaterial>;
  private readonly allyLights: THREE.PointLight[] = [];
  private strands: Strand[] = [];
  private readonly dummy = new THREE.Object3D();
  private settings = getActiveBlessVfxSettings();
  private cast: BlessVfxCast | null = null;
  private receptions: Reception[] = [];
  private elapsed = 0;
  private waveRelease = 0.3;
  private releaseEventSent = false;
  private waveEventSent = false;
  private disposed = false;
  private randState = 1;
  private particleSeeds: number[] = [];

  constructor(private readonly scene: THREE.Object3D) {
    const positions: number[] = [], indices: number[] = [];
    const segments = 160, sides = 8;
    for (let i=0;i<=segments;i++) { const a=i/segments*Math.PI*2; for(let j=0;j<=sides;j++){ const b=j/sides*Math.PI*2; const r=1+0.07*Math.cos(b); positions.push(Math.cos(a)*r,Math.sin(a)*r,0.07*Math.sin(b)); if(i<segments&&j<sides){const k=i*(sides+1)+j; indices.push(k,k+sides+1,k+1,k+1,k+sides+1,k+sides+2);} } }
    const geo = new THREE.BufferGeometry(); geo.setAttribute("position",new THREE.Float32BufferAttribute(positions,3)); geo.setIndex(indices); geo.computeVertexNormals();
    const waveMaterial = new THREE.ShaderMaterial({ uniforms:{uTime:{value:0},uRadius:{value:0.05},uThickness:{value:0.12},uNoise:{value:0.2},uHeight:{value:0.42},uOpacity:{value:0},uEmissive:{value:4}}, vertexShader:WAVE_VERTEX, fragmentShader:WAVE_FRAGMENT, transparent:true, depthTest:true, depthWrite:false, side:THREE.DoubleSide, blending:THREE.AdditiveBlending, toneMapped:false });
    this.ring = new THREE.Mesh(geo,waveMaterial); this.ring.renderOrder=9; this.ring.frustumCulled=false;
    this.core = new THREE.Mesh(new THREE.IcosahedronGeometry(0.18,2),new THREE.MeshStandardMaterial({color:0xffd77e,emissive:0xffb83e,emissiveIntensity:4,roughness:0.32,metalness:0.08}));
    this.core.renderOrder=10; this.core.castShadow=false;
    this.particles = new THREE.InstancedMesh(this.particleGeometry,this.particleMaterial,Math.max(1,this.settings.casterParticles+this.settings.allyParticles*8));
    this.particles.instanceMatrix.setUsage(THREE.DynamicDrawUsage); this.particles.frustumCulled=false;
    this.group.add(this.ring,this.core,this.particles,this.centerLight); this.scene.add(this.group);
    this.centerLight.castShadow=true; this.centerLight.shadow.mapSize.set(256,256); this.centerLight.shadow.bias=-0.003; this.centerLight.visible=false;
    for(let i=0;i<8;i++){ const light=new THREE.PointLight(0xffc85c,0,2.4,2); light.visible=false; if(i===0){light.castShadow=true;light.shadow.mapSize.set(256,256);light.shadow.bias=-0.003;} this.allyLights.push(light); this.scene.add(light); }
    this.group.visible=false;
  }

  setSettings(settings: BlessVfxSettings): void {
    this.settings={...settings};
    if(this.particles.count!==this.particleCapacity()){ this.group.remove(this.particles); this.particles=new THREE.InstancedMesh(this.particleGeometry,this.particleMaterial,this.particleCapacity()); this.particles.instanceMatrix.setUsage(THREE.DynamicDrawUsage); this.particles.frustumCulled=false; this.group.add(this.particles); }
    this.particleMaterial.emissiveIntensity=settings.emissive;
    if(this.cast){this.randState=this.settings.seed>>>0;this.particleSeeds=Array.from({length:this.particles.count},()=>this.nextRandom());this.rebuildStrands();}
  }
  hide(): void { this.group.visible=false; this.centerLight.intensity=0; this.centerLight.visible=false; for(const r of this.receptions){r.light.intensity=0;r.light.visible=false;} this.clearStrands(); this.cast=null; }
  castSpell(cast: BlessVfxCast): void {
    if(this.disposed)return; this.hide(); this.setSettings(getActiveBlessVfxSettings());
    this.cast=cast; this.elapsed=0; this.releaseEventSent=false; this.waveEventSent=false; this.randState=this.settings.seed>>>0; this.receptions=cast.allies.map((ally,index)=>({ally,light:this.allyLights[index%this.allyLights.length]!,received:false,startedAt:-1,absorbEventSent:false}));
    this.particleSeeds=Array.from({length:this.particles.count},()=>this.nextRandom());
    this.group.position.copy(cast.center); this.group.visible=true; this.centerLight.visible=true;
    this.receptions.forEach((r)=>{r.light.visible=true;r.light.intensity=0;});
    cast.onTimelineEvent?.("bless_charge");
    this.rebuildStrands(); this.update(0);
  }
  update(dt:number):void {
    if(!this.cast||this.disposed)return;
    this.elapsed+=Math.min(0.05,Math.max(0,dt)); const t=this.elapsed; const release=Math.max(0,t-this.waveRelease); const waveTravel=Math.max(0.08,(this.settings.duration-this.waveRelease-0.65)/Math.max(0.1,this.settings.waveSpeed)); const arrive=THREE.MathUtils.smoothstep(release,0,waveTravel);
    if(t>=this.waveRelease&&!this.releaseEventSent){this.releaseEventSent=true;this.cast.onTimelineEvent?.("bless_release");}
    if(t>=this.waveRelease&&!this.waveEventSent){this.waveEventSent=true;this.cast.onTimelineEvent?.("bless_wave");}
    const waveRadius=this.cast.radiusWorld*this.settings.waveRadius*arrive; const fade=t>this.settings.duration?Math.max(0,1-(t-this.settings.duration)/0.25):1;
    this.ring.scale.set(waveRadius,waveRadius,1); this.ring.position.z=0.025; const wu=this.ring.material.uniforms; wu.uTime!.value=t*this.settings.waveSpeed; wu.uRadius!.value=1; wu.uThickness!.value=this.settings.waveThickness; wu.uNoise!.value=this.settings.waveTurbulence; wu.uHeight!.value=this.settings.waveHeight; wu.uOpacity!.value=this.settings.geometry?fade*(t<this.waveRelease?0:0.88):0; wu.uEmissive!.value=this.settings.emissiveEnabled?this.settings.emissive:0;
    this.ring.visible=this.settings.geometry&&t>=this.waveRelease&&arrive>0.001;
    const buildup=THREE.MathUtils.smoothstep(t,0,0.28); const releasePulse=t>=this.waveRelease?Math.max(0,1-(t-this.waveRelease)/0.36):0; const lightFade=t>this.settings.duration?Math.max(0,1-(t-this.settings.duration)/0.25):1;
    this.centerLight.position.set(0,-0.38,0.2); this.centerLight.distance=this.settings.casterLightRadius; this.centerLight.intensity=this.settings.lights?(this.settings.casterLight*(buildup*0.24+releasePulse)*lightFade):0;
    const cScale=0.3+buildup*1.1+releasePulse*0.42; this.core.visible=this.settings.geometry&&t<this.settings.duration+0.2; this.core.scale.setScalar(cScale); this.core.position.set(0,-0.3+Math.sin(t*8)*0.035,0.06); this.core.material.emissiveIntensity=this.settings.emissiveEnabled?this.settings.emissive*(0.45+releasePulse):0;
    for(const reception of this.receptions){
      const unitDist=Math.max(0,Math.min(1,reception.ally.distanceHexes/3)); const reachAt=this.waveRelease+unitDist*waveTravel;
      if(!reception.received&&t>=reachAt){reception.received=true;reception.startedAt=t;this.cast.onTimelineEvent?.("bless_unit_receive",reception.ally.id);this.cast.onApply(reception.ally.id);}
      const local=reception.received?Math.max(0,t-reception.startedAt):-1; const localProgress=local*this.settings.absorptionSpeed; const localFade=local<0?0:Math.max(0,1-localProgress/0.78); const distance=this.settings.allyLightRadius;
      if(local>=0.5&&!reception.absorbEventSent){reception.absorbEventSent=true;this.cast.onTimelineEvent?.("bless_absorb",reception.ally.id);}
      reception.light.position.copy(reception.ally.position); reception.light.position.y-=0.35; reception.light.distance=distance; reception.light.intensity=this.settings.lights&&local>=0?this.settings.allyLight*Math.min(1,local*7)*localFade*this.settings.lightDecay:0;
      const groupStrands=this.strands.filter(s=>s.unitId===reception.ally.id); for(const strand of groupStrands){ strand.mesh.visible=this.settings.geometry&&local>=0&&localProgress<0.78; const absorb=THREE.MathUtils.smoothstep(localProgress,0.48,0.78); strand.mesh.scale.setScalar(Math.max(0.025,1-absorb*0.95)); strand.mesh.material.opacity=this.settings.emissiveEnabled?Math.max(0,localFade*(0.85-absorb*0.45)):0; strand.mesh.material.emissiveIntensity=this.settings.emissive*(0.65+Math.sin(t*9+strand.phase)*0.18); strand.mesh.rotation.y=t*0.3+strand.phase; }
    }
    this.updateParticles(t,arrive,fade);
    if(t>=this.settings.duration+0.25){const active=this.cast; active?.onTimelineEvent?.("bless_complete"); this.hide(); active?.onComplete();}
  }
  dispose():void { if(this.disposed)return; this.disposed=true; this.hide(); this.scene.remove(this.group); for(const l of this.allyLights){this.scene.remove(l);} this.clearStrands(); this.ring.geometry.dispose(); this.ring.material.dispose(); this.core.geometry.dispose(); this.core.material.dispose(); this.particleGeometry.dispose(); this.particleMaterial.dispose(); }
  private particleCapacity():number{return Math.max(1,Math.round(this.settings.casterParticles+this.settings.allyParticles*8));}
  private nextRandom():number{this.randState=(Math.imul(this.randState,1664525)+1013904223)>>>0;return this.randState/4294967296;}
  private updateParticles(t:number,wave:number,fade:number):void {
    const count=this.particles.count; const capCaster=Math.min(count,Math.max(0,Math.round(this.settings.casterParticles))); const perAlly=Math.max(0,Math.round(this.settings.allyParticles));
    this.particleMaterial.emissiveIntensity=this.settings.emissiveEnabled?this.settings.emissive:0;
    for(let i=0;i<count;i++){let p:THREE.Vector3;let s=0;
      if(i<capCaster){const a=i*2.39996+(this.particleSeeds[i]??0)*0.45;const age=(t*0.72+(i%17)/17)%1;const radius=(0.18+0.7*(1-age))*(0.5+wave*0.45);p=new THREE.Vector3(Math.cos(a+t*0.9)*radius,-0.12-age*this.settings.waveHeight,Math.sin(a+t*0.9)*radius*0.55);s=(1-age)*fade*0.13;}
      else if(i<capCaster+perAlly*8){const j=i-capCaster; const reception=this.receptions[Math.floor(j/Math.max(1,perAlly))]; const particleIndex=j%Math.max(1,perAlly); if(!reception?.received||perAlly===0){this.dummy.scale.setScalar(0);this.dummy.updateMatrix();this.particles.setMatrixAt(i,this.dummy.matrix);continue;} const local=Math.max(0,t-reception.startedAt)*this.settings.absorptionSpeed; const a=particleIndex*2.39996+(this.particleSeeds[i]??0)*0.45+local*1.5; const age=(local*1.6+(particleIndex%11)/11)%1; const radius=0.14+age*0.28; const allyLocal=reception.ally.position.clone().sub(this.cast!.center); p=allyLocal.clone().add(new THREE.Vector3(Math.cos(a)*radius,-0.12-age*this.settings.strandHeight,Math.sin(a)*radius*0.6)); const absorb=THREE.MathUtils.smoothstep(local,0.48,0.78); p.lerp(allyLocal,absorb*0.75); s=(1-age)*Math.max(0,1-local/0.78)*0.1;}
      else {this.dummy.scale.setScalar(0);this.dummy.updateMatrix();this.particles.setMatrixAt(i,this.dummy.matrix);continue;}
      this.dummy.position.copy(p);this.dummy.scale.setScalar(this.settings.particles?s:0);this.dummy.rotation.set(t+i,t*0.6+i*0.37,t*0.3);this.dummy.updateMatrix();this.particles.setMatrixAt(i,this.dummy.matrix);
    }
    this.particles.instanceMatrix.needsUpdate=true; this.particles.visible=this.settings.particles;
  }
  private rebuildStrands():void {
    this.clearStrands(); for(const r of this.receptions){for(let i=0;i<Math.min(10,Math.max(1,Math.round(this.settings.strandCount)));i++){const a=i/this.settings.strandCount*Math.PI*2;const curve=new THREE.CatmullRomCurve3([new THREE.Vector3(Math.cos(a)*0.05,0,Math.sin(a)*0.05),new THREE.Vector3(Math.cos(a+this.settings.strandCurvature)*0.2,-this.settings.strandHeight*0.35,Math.sin(a+this.settings.strandCurvature)*0.2),new THREE.Vector3(Math.cos(a+Math.PI)*0.17,-this.settings.strandHeight*0.72,Math.sin(a+Math.PI)*0.17),new THREE.Vector3(Math.cos(a+Math.PI*1.5)*0.1,-this.settings.strandHeight,Math.sin(a+Math.PI*1.5)*0.1)]);const material=new THREE.MeshStandardMaterial({color:0xffd374,emissive:0xffbf4c,emissiveIntensity:this.settings.emissive,roughness:0.36,metalness:0.05,transparent:true,opacity:0,depthWrite:false});const mesh=new THREE.Mesh(new THREE.TubeGeometry(curve,14,this.settings.strandThickness,5,false),material);mesh.position.copy(r.ally.position);mesh.renderOrder=10;mesh.frustumCulled=false;this.strands.push({mesh,unitId:r.ally.id,phase:a,seed:i});this.scene.add(mesh);}}
  }
  private clearStrands():void{for(const s of this.strands){this.scene.remove(s.mesh);s.mesh.geometry.dispose();s.mesh.material.dispose();}this.strands=[];}
}
