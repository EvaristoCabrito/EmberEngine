import * as THREE from "three";

export type VarreduraTarget = { id:string; position:THREE.Vector3 };
export type VarreduraSettings = { angle:number; radius:number; waveSpeed:number; thickness:number; height:number; leadingEdge:number; turbulence:number; breakup:number; trailLength:number; trailThickness:number; dustAmount:number; debrisCount:number; debrisVelocity:number; impactSize:number; spearLight:number; spearRadius:number; impactLight:number; impactRadius:number; residualDuration:number; seed:number; geometryEnabled:boolean; debrisEnabled:boolean; lightEnabled:boolean };
export const DEFAULT_VARREDURA_SETTINGS:VarreduraSettings={angle:2.25,radius:3.1,waveSpeed:1,thickness:.42,height:1.2,leadingEdge:.055,turbulence:.2,breakup:.45,trailLength:1.25,trailThickness:.16,dustAmount:1,debrisCount:48,debrisVelocity:1.2,impactSize:.28,spearLight:3.4,spearRadius:1.7,impactLight:5,impactRadius:1.35,residualDuration:.28,seed:1977,geometryEnabled:true,debrisEnabled:true,lightEnabled:true};
const STORAGE_KEY="emberashes.vfx.varredura.v2";
export function getActiveVarreduraSettings():VarreduraSettings{try{const stored=localStorage.getItem(STORAGE_KEY);if(stored)return{...DEFAULT_VARREDURA_SETTINGS,...JSON.parse(stored) as Partial<VarreduraSettings>};}catch{}return{...DEFAULT_VARREDURA_SETTINGS};}
export function setActiveVarreduraSettings(settings:VarreduraSettings):void{try{localStorage.setItem(STORAGE_KEY,JSON.stringify(settings));}catch{}}

/** Lancer's directional spear sweep, assigned to Cleave. Visual-only: the caller supplies combat's exact
 * target list; this class never queries targets, causes damage, or changes hit rules. */
/** A fixed set of point lights that always stay in the scene. Effects borrow from it instead of
 * adding/removing their own: changing the scene's light count makes Three.js recompile every lit
 * material on screen, which was the stall on every Cleave/Sweep. */
export type VfxLightPool = { take(color: number, distance: number, decay: number): THREE.PointLight; give(light: THREE.PointLight): void };

export class CleaveSweepVFX {
  private readonly root=new THREE.Group(); private readonly blade:THREE.Mesh; private readonly edge:THREE.Mesh; private readonly wake:THREE.Mesh; private readonly debris:THREE.InstancedMesh;
  private readonly spearhead:THREE.Mesh; private readonly trail:THREE.Line; private readonly trailPositions:THREE.Vector3[]=[];
  private readonly spearLight:THREE.PointLight; private readonly impactLights:THREE.PointLight[]=[]; private readonly impactAt:number[]=[];
  private readonly dummy=new THREE.Object3D(); private readonly targets:VarreduraTarget[]; private elapsed=0; private disposed=false; private readonly settings:VarreduraSettings; private readonly origin:THREE.Vector3; private readonly direction:THREE.Vector2; private rng:number; private readonly tile:number;
  constructor(private readonly scene:THREE.Object3D, origin:THREE.Vector3, targets:VarreduraTarget[], tile:number, settings:Partial<VarreduraSettings>={}, private readonly lights?:VfxLightPool){
    this.settings={...DEFAULT_VARREDURA_SETTINGS,...settings}; this.targets=targets; this.origin=origin.clone(); this.rng=this.settings.seed>>>0||1; this.tile=tile;
    const avg=new THREE.Vector2(); for(const target of targets)avg.add(new THREE.Vector2(target.position.x-origin.x,target.position.y-origin.y)); if(avg.lengthSq()<.001)avg.set(1,0); this.direction=avg.normalize();
    this.root.position.copy(origin); scene.add(this.root);
    const surface=this.makeCrescent(tile,0.54,1.18,1); const mat=new THREE.MeshStandardMaterial({color:0xc8e6f2,emissive:0x7ca9b9,emissiveIntensity:1.8,roughness:.3,metalness:.08,transparent:true,opacity:.82,side:THREE.DoubleSide,depthWrite:false});
    this.blade=new THREE.Mesh(surface,mat); this.blade.position.z=tile*.17; this.root.add(this.blade);
    const edgeGeo=this.makeCrescent(tile,.94,1.03,Math.max(.08,this.settings.leadingEdge/this.settings.thickness)); const edgeMat=new THREE.MeshBasicMaterial({color:0xf2fbff,transparent:true,opacity:.95,side:THREE.DoubleSide,depthWrite:false}); this.edge=new THREE.Mesh(edgeGeo,edgeMat); this.edge.position.z=tile*.23; this.root.add(this.edge);
    const wakeGeo=this.makeCrescent(tile,.46*this.settings.trailLength,.87,Math.max(.08,this.settings.trailThickness/this.settings.thickness)); const wakeMat=new THREE.MeshStandardMaterial({color:0x9aaeb5,emissive:0x597882,emissiveIntensity:.5,transparent:true,opacity:.46,side:THREE.DoubleSide,depthWrite:false}); this.wake=new THREE.Mesh(wakeGeo,wakeMat); this.wake.position.z=tile*.08; this.root.add(this.wake);
    const spearMat=new THREE.MeshBasicMaterial({color:0xf4fbff,toneMapped:false}); this.spearhead=new THREE.Mesh(new THREE.IcosahedronGeometry(tile*.075,1),spearMat); this.root.add(this.spearhead);
    const trailGeometry=new THREE.BufferGeometry(); trailGeometry.setFromPoints([new THREE.Vector3(),new THREE.Vector3()]); const trailMaterial=new THREE.LineBasicMaterial({color:0xdff7ff,transparent:true,opacity:.82,toneMapped:false,depthWrite:false}); this.trail=new THREE.Line(trailGeometry,trailMaterial); this.root.add(this.trail);
    const box=new THREE.BoxGeometry(.065,.075,.065); const debrisMat=new THREE.MeshStandardMaterial({color:0x9a9487,roughness:1}); const n=Math.min(120,Math.max(1,Math.round(this.settings.debrisCount*this.settings.dustAmount))); this.debris=new THREE.InstancedMesh(box,debrisMat,n); this.debris.count=n; this.root.add(this.debris);
    for(let i=0;i<n;i++){this.dummy.position.set(this.random()*tile*.4, this.random()*tile*.28, tile*(.08+this.random()*.35)); this.dummy.scale.setScalar(.6+this.random()*1.5); this.dummy.updateMatrix(); this.debris.setMatrixAt(i,this.dummy.matrix);} this.debris.instanceMatrix.needsUpdate=true;
    const take=(color:number,distance:number,decay:number)=>{if(this.lights)return this.lights.take(color,distance,decay);const l=new THREE.PointLight(color,0,distance,decay);this.scene.add(l);return l;};
    this.spearLight=take(0xe8f8ff,tile*this.settings.spearRadius,2);
    for(let i=0;i<targets.length;i++){const light=take(0xf3fbff,tile*this.settings.impactRadius,2); this.impactLights.push(light); this.impactAt.push(-1);}
  }
  get finished():boolean{return this.elapsed>1.25+this.settings.residualDuration+Math.max(0,.46/Math.max(.25,this.settings.waveSpeed)-.46);}
  update(dt:number):void{
    if(this.disposed)return; this.elapsed+=Math.max(0,Math.min(.08,dt)); const t=this.elapsed; const waveDuration=.46/Math.max(.25,this.settings.waveSpeed); const waveStart=.36; const waveEnd=waveStart+waveDuration; const grow=THREE.MathUtils.smoothstep(t,waveStart,waveEnd); const collapse=1-THREE.MathUtils.smoothstep(t,waveEnd,waveEnd+.31+this.settings.residualDuration);
    const baseAngle=Math.atan2(this.direction.y,this.direction.x); this.root.rotation.z=baseAngle;
    this.blade.scale.set(grow,1,1); this.edge.scale.set(grow,1,1); this.wake.scale.set(grow,1,1);
    this.blade.visible=this.settings.geometryEnabled&&t>.34&&collapse>.01; this.edge.visible=this.blade.visible; this.wake.visible=this.settings.geometryEnabled&&t>.47&&collapse>.01; this.debris.visible=this.settings.geometryEnabled&&this.settings.debrisEnabled;
    (this.blade.material as THREE.MeshStandardMaterial).opacity=.76*collapse; (this.wake.material as THREE.MeshStandardMaterial).opacity=.42*collapse;
    const sweep=THREE.MathUtils.clamp((t-waveStart)/waveDuration,0,1); const sweepAngle=(sweep-.5)*this.settings.angle; const spearRadius=this.tile*(.36+this.settings.radius*.64*grow); const localX=Math.cos(sweepAngle)*spearRadius,localY=Math.sin(sweepAngle)*spearRadius; this.spearhead.position.set(localX,localY,this.tile*(.22+this.settings.height*.22*Math.sin(sweep*Math.PI)));
    this.spearhead.visible=this.settings.geometryEnabled&&t>waveStart&&t<waveEnd;
    this.trailPositions.push(this.spearhead.position.clone()); const maxTrailPoints=30; if(this.trailPositions.length>maxTrailPoints)this.trailPositions.splice(0,this.trailPositions.length-maxTrailPoints); const trailLength=Math.max(2,Math.round(this.trailPositions.length*Math.min(1,this.settings.trailLength/1.25))); const trailPoints=this.trailPositions.slice(-trailLength); (this.trail.geometry as THREE.BufferGeometry).setFromPoints(trailPoints); this.trail.visible=this.settings.geometryEnabled&&this.settings.trailThickness>0&&trailPoints.length>1&&t>waveStart&&t<waveEnd+.22;
    const cos=Math.cos(baseAngle),sin=Math.sin(baseAngle); this.spearLight.position.set(this.origin.x+localX*cos-localY*sin,this.origin.y+localX*sin+localY*cos,this.origin.z+this.spearhead.position.z); this.spearLight.intensity=this.settings.lightEnabled&&t>waveStart&&t<waveEnd?this.settings.spearLight*Math.sin(Math.PI*THREE.MathUtils.clamp((t-waveStart)/waveDuration,0,1)):0;
    const n=this.debris.count; for(let i=0;i<n;i++){const target=this.targets[i%Math.max(1,this.targets.length)]; const phase=i/n; const arrival=target? .5+THREE.MathUtils.clamp(new THREE.Vector2(target.position.x-this.origin.x,target.position.y-this.origin.y).dot(this.direction)/(this.settings.radius*this.tile),0,1)*.26:.5; const age=Math.max(0,t-arrival); const distance=age*this.settings.debrisVelocity*this.tile*1.4; const dx=target?target.position.x-this.origin.x:0,dy=target?target.position.y-this.origin.y:0; const radial=dx*this.direction.x+dy*this.direction.y,side=-dx*this.direction.y+dy*this.direction.x; this.dummy.position.x=radial+this.direction.x*distance+Math.sin(i*8.31+age*9)*this.settings.turbulence*this.tile*.25; this.dummy.position.y=side+Math.cos(i*5.17+age*7)*this.settings.turbulence*this.tile*.2; this.dummy.position.z=this.tile*(.14+Math.abs(Math.sin(age*9+i))*this.settings.height*.3); const fade=target?1-THREE.MathUtils.smoothstep(age,.2,.75):1-THREE.MathUtils.smoothstep(t,.72,1.1); this.dummy.scale.setScalar(Math.max(.001,fade*(.4+(i%4)*.15))); this.dummy.rotation.set(age*7+i,age*9,age*4); this.dummy.updateMatrix(); this.debris.setMatrixAt(i,this.dummy.matrix); } this.debris.instanceMatrix.needsUpdate=true;
    for(let i=0;i<this.targets.length;i++){const target=this.targets[i]!, along=new THREE.Vector2(target.position.x-this.origin.x,target.position.y-this.origin.y).dot(this.direction); const arrival=waveStart+THREE.MathUtils.clamp(along/(this.settings.radius*this.tile),0,1)*waveDuration; const light=this.impactLights[i]; if(!light)continue; if(this.impactAt[i]!<0&&t>=arrival)this.impactAt[i]=t; light.position.copy(target.position).add(new THREE.Vector3(0,0,this.tile*.5)); const age=this.impactAt[i]!<0?Infinity:t-this.impactAt[i]!; const amp=1-THREE.MathUtils.smoothstep(age,0,.14); light.distance=this.settings.impactRadius*this.tile*(1+this.settings.impactSize); light.intensity=this.settings.lightEnabled?this.settings.impactLight*amp:0; }
  }
  dispose():void{if(this.disposed)return;this.disposed=true;this.scene.remove(this.root);
    if(this.lights){this.lights.give(this.spearLight);for(const l of this.impactLights)this.lights.give(l);}else this.scene.remove(this.spearLight,...this.impactLights);
    // Geometry is freed; materials are not, so their compiled shaders stay cached for the next cast.
    for(const child of [this.blade,this.edge,this.wake,this.spearhead,this.trail])child.geometry.dispose(); this.debris.geometry.dispose();}
  private makeCrescent(tile:number,radiusScale:number,depth:number,thicknessScale:number):THREE.BufferGeometry{
    const segments=52, sides=8, vertices:number[]=[], indices:number[]=[]; const radius=tile*this.settings.radius*radiusScale; const start=-this.settings.angle*.56, end=this.settings.angle*.44;
    for(let i=0;i<=segments;i++){const u=i/segments,a=start+(end-start)*u,noise=(this.random()-.5)*this.settings.breakup*.055; const r=radius*(.88+.12*u+noise); const cx=Math.cos(a)*r, cy=Math.sin(a)*r; const thick=tile*this.settings.thickness*thicknessScale*(.75+.4*Math.sin(Math.PI*u)); for(let j=0;j<sides;j++){const v=j/sides*Math.PI*2; vertices.push(cx+Math.cos(v)*thick*.5,cy+Math.sin(v)*thick*.15,Math.sin(v)*thick*depth+tile*(.12+u*this.settings.height*.14));}}
    for(let i=0;i<segments;i++)for(let j=0;j<sides;j++){const a=i*sides+j,b=i*sides+(j+1)%sides,c=(i+1)*sides+j,d=(i+1)*sides+(j+1)%sides;indices.push(a,c,b,b,c,d);}
    const geo=new THREE.BufferGeometry();geo.setAttribute("position",new THREE.Float32BufferAttribute(vertices,3));geo.setIndex(indices);geo.computeVertexNormals();return geo;
  }
  private random():number{let x=this.rng;x^=x<<13;x^=x>>>17;x^=x<<5;this.rng=x>>>0;return this.rng/4294967296;}
}

/** Full circular pressure wave for Varredura. This is a radial shockwave, not a directional
 * weapon slash. Target positions come from the combat resolution and remain visual-only here. */
export class VarreduraVFX {
  private readonly root = new THREE.Group();
  private readonly outer: THREE.Mesh;
  private readonly inner: THREE.Mesh;
  private readonly debris: THREE.InstancedMesh;
  private readonly edgeLight: THREE.PointLight;
  private readonly targetLights: THREE.PointLight[] = [];
  private readonly impactAt: number[] = [];
  private readonly dummy = new THREE.Object3D();
  private readonly targetOffsets: THREE.Vector3[];
  private readonly radius: number;
  private elapsed = 0;
  private disposed = false;

  constructor(private readonly scene: THREE.Object3D, private readonly origin: THREE.Vector3, targets: VarreduraTarget[], tile: number, private readonly lights?: VfxLightPool) {
    this.radius = tile * 3.2;
    this.targetOffsets = targets.map((target) => target.position.clone().sub(origin));
    this.root.position.copy(origin);
    scene.add(this.root);

    const outerMaterial = new THREE.MeshBasicMaterial({ color: 0xe7fbff, transparent: true, opacity: 0.92, side: THREE.DoubleSide, depthWrite: false, toneMapped: false });
    this.outer = new THREE.Mesh(new THREE.TorusGeometry(this.radius, tile * 0.085, 8, 96), outerMaterial);
    this.outer.position.z = tile * 0.12;
    this.root.add(this.outer);
    const innerMaterial = new THREE.MeshStandardMaterial({ color: 0xa8d5e1, emissive: 0x7ac8dc, emissiveIntensity: 1.25, transparent: true, opacity: 0.56, side: THREE.DoubleSide, depthWrite: false });
    this.inner = new THREE.Mesh(new THREE.TorusGeometry(this.radius * 0.78, tile * 0.045, 10, 96), innerMaterial);
    this.inner.position.z = tile * 0.16;
    this.root.add(this.inner);

    const count = 42;
    this.debris = new THREE.InstancedMesh(new THREE.IcosahedronGeometry(tile * 0.035, 0), new THREE.MeshStandardMaterial({ color: 0xcbdce0, roughness: 0.9 }), count);
    this.debris.count = count;
    this.root.add(this.debris);
    const take = (color: number, distance: number, decay: number) => {
      if (lights) return lights.take(color, distance, decay);
      const light = new THREE.PointLight(color, 0, distance, decay);
      scene.add(light);
      return light;
    };
    this.edgeLight = take(0xdaf8ff, tile * 2.8, 2);
    for (let i = 0; i < targets.length; i++) {
      const light = take(0xe7fbff, tile * 1.4, 2);
      this.targetLights.push(light);
      this.impactAt.push(-1);
    }
  }

  get finished(): boolean { return this.elapsed > 1.18; }

  update(dt: number): void {
    if (this.disposed) return;
    this.elapsed += Math.max(0, Math.min(0.08, dt));
    const t = this.elapsed;
    const progress = THREE.MathUtils.clamp((t - 0.12) / 0.68, 0, 1);
    const radius = this.radius * THREE.MathUtils.smoothstep(progress, 0, 1);
    const fade = 1 - THREE.MathUtils.smoothstep(t, 0.72, 1.12);
    this.outer.scale.set(progress, progress, 1);
    this.inner.scale.set(progress, progress, 1);
    this.outer.visible = t >= 0.12 && fade > 0.01;
    this.inner.visible = t >= 0.18 && fade > 0.01;
    (this.outer.material as THREE.MeshBasicMaterial).opacity = 0.9 * fade;
    (this.inner.material as THREE.MeshStandardMaterial).opacity = 0.58 * fade;
    this.root.rotation.z = Math.sin(t * 5.5) * 0.035;

    for (let i = 0; i < this.debris.count; i++) {
      const angle = (i / this.debris.count) * Math.PI * 2 + Math.sin(t * 6 + i * 1.7) * 0.07;
      const travel = radius * (0.72 + (i % 5) * 0.07);
      this.dummy.position.set(Math.cos(angle) * travel, Math.sin(angle) * travel, 0.1 + Math.abs(Math.sin(t * 12 + i * 2.3)) * 0.36);
      const scale = (0.35 + (i % 4) * 0.16) * fade;
      this.dummy.scale.setScalar(Math.max(0.001, scale));
      this.dummy.rotation.set(t * 6 + i, t * 4 - i, t * 7 + i * 0.3);
      this.dummy.updateMatrix();
      this.debris.setMatrixAt(i, this.dummy.matrix);
    }
    this.debris.instanceMatrix.needsUpdate = true;
    const orbit = t * 9.5;
    this.edgeLight.position.set(this.origin.x + Math.cos(orbit) * radius, this.origin.y + Math.sin(orbit) * radius, this.origin.z + 0.42);
    this.edgeLight.intensity = t > 0.12 && t < 0.86 ? 3.2 * (0.55 + 0.45 * Math.sin(orbit * 0.5)) : 0;

    for (let i = 0; i < this.targetOffsets.length; i++) {
      const offset = this.targetOffsets[i]!;
      const distance = Math.hypot(offset.x, offset.y);
      const arrival = 0.12 + Math.min(1, distance / this.radius) * 0.68;
      if (this.impactAt[i] === -1 && t >= arrival) this.impactAt[i] = t;
      const light = this.targetLights[i]!;
      light.position.copy(this.origin).add(offset).add(new THREE.Vector3(0, 0, 0.45));
      const age = this.impactAt[i] === -1 ? Infinity : t - this.impactAt[i]!;
      light.intensity = 5.2 * (1 - THREE.MathUtils.smoothstep(age, 0, 0.12));
    }
  }

  dispose(): void {
    if (this.disposed) return;
    this.disposed = true;
    this.scene.remove(this.root);
    if (this.lights) {
      this.lights.give(this.edgeLight);
      for (const light of this.targetLights) this.lights.give(light);
    } else this.scene.remove(this.edgeLight, ...this.targetLights);
    // Geometry is freed; materials are not, so their compiled shaders stay cached for the next cast.
    for (const mesh of [this.outer, this.inner, this.debris]) mesh.geometry.dispose();
  }
}
