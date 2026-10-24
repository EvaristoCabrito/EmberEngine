import * as THREE from 'three';

export const PROP_KINDS = ['pine','broadleaf-tree','dead-tree','stump','bush','rock','boulder-cluster','cottage','ruined-wall','stone-wall','wooden-fence','gate','wooden-bridge','well','torch','brazier','barrel','crate','chest','signpost','tent','campfire'] as const;
export type PropKind = typeof PROP_KINDS[number];
export interface PropOptions { seed?: number; scale?: number; variant?: number; /** Bridge length in world units. */ length?: number }
export interface PropTransform { position: THREE.Vector3 | [number,number,number]; rotation?: number; scale?: number; seed?: number; variant?: number }
type Surface = 'wood'|'bark'|'stone'|'plaster'|'roof'|'iron'|'cloth'|'leaf'|'needle'|'dark'|'fire';
const materials = new Map<Surface,THREE.MeshStandardMaterial>();
const geometries = new Map<string,THREE.BufferGeometry>();
const templates = new Map<string,THREE.Group>();
function random(seed: number) { let s=seed>>>0; return () => { s=(Math.imul(s,1664525)+1013904223)>>>0; return s/4294967296; }; }
function material(kind: Surface) {
  const cached=materials.get(kind); if(cached) return cached;
  const palette: Record<Surface,[number,number,number]>={wood:[104,78,51],bark:[65,55,42],stone:[121,118,109],plaster:[185,174,149],roof:[83,78,71],iron:[54,52,48],cloth:[145,135,107],leaf:[59,77,39],needle:[39,58,37],dark:[26,23,19],fire:[233,138,47]};
  const cv=document.createElement('canvas'); cv.width=cv.height=256; const ctx=cv.getContext('2d')!; const im=ctx.createImageData(256,256); const r=random(103+kind.length); const base=palette[kind];
  for(let y=0;y<256;y++) for(let x=0;x<256;x++) {
    const i=(y*256+x)*4; let n=(r()-.5)*22;
    if(kind==='wood'||kind==='bark') n+=14*Math.sin(x*.34+Math.sin(y*.035)*2)+7*Math.sin(x*1.4+Math.sin(y*.016));
    if(kind==='stone'||kind==='plaster') n+=9*Math.sin(x*.14+Math.sin(y*.11))+5*Math.cos(y*.29);
    if(kind==='roof') { const row=Math.floor(y/32); if(y%32<3||(x+(row%2)*24)%48<2)n-=34; }
    if(kind==='cloth')n+=4*((x%2)+(y%2)-1);
    let a=255;
    if(kind==='leaf'||kind==='needle') {
      const u=(x-128)/128,v=(y-128)/128;
      if(kind==='leaf') { if(u*u/.38+v*v>.95||Math.abs(u)>.64*(1-v*v))a=0; n+=Math.abs(u)<.02?19:7*Math.sin((u+Math.abs(v)*.8)*50); }
      else { a=0; for(let k=0;k<12;k++){ const yy=-.8+k*.13; const side=Math.abs(u); if(side<.82*(1-Math.abs(v)*.4)&&Math.abs(v-yy+side*.35)<.022)a=255; } if(Math.abs(u)<.018)a=255; }
    }
    im.data[i]=Math.max(0,base[0]+n);im.data[i+1]=Math.max(0,base[1]+n);im.data[i+2]=Math.max(0,base[2]+n);im.data[i+3]=a;
  }
  ctx.putImageData(im,0,0); const tex=new THREE.CanvasTexture(cv);tex.colorSpace=THREE.SRGBColorSpace;tex.wrapS=tex.wrapT=THREE.RepeatWrapping;tex.anisotropy=4;
  const foliage=kind==='leaf'||kind==='needle';
  const m=new THREE.MeshStandardMaterial({map:tex,roughness:kind==='iron'?.65:.95,metalness:kind==='iron'?.75:0,side:foliage||kind==='cloth'?THREE.DoubleSide:THREE.FrontSide,alphaTest:foliage?.5:0});
  if(!foliage&&kind!=='fire'){m.bumpMap=tex;m.bumpScale=kind==='stone'?.045:kind==='bark'?.04:.015;}
  if(kind==='fire'){m.emissive.set('#ef8c30');m.emissiveIntensity=1.8;}
  materials.set(kind,m);return m;
}
function geo(key:string,make:()=>THREE.BufferGeometry){let g=geometries.get(key);if(!g){g=make();geometries.set(key,g);}return g;}
const boxGeo=()=>geo('box',()=>new THREE.BoxGeometry(1,1,1));
const cylinderGeo=()=>geo('cylinder',()=>new THREE.CylinderGeometry(1,1,1,12));
function add(g:THREE.Group,geometry:THREE.BufferGeometry,surface:Surface,x:number,y:number,z:number,sx=1,sy=1,sz=1){const m:THREE.Mesh<THREE.BufferGeometry,THREE.Material|THREE.Material[]>=new THREE.Mesh(geometry,material(surface));m.position.set(x,y,z);m.scale.set(sx,sy,sz);m.castShadow=m.receiveShadow=true;g.add(m);return m;}
function box(g:THREE.Group,s:Surface,x:number,y:number,z:number,w:number,h:number,d:number){return add(g,boxGeo(),s,x,y,z,w,h,d);}
function pole(g:THREE.Group,s:Surface,x:number,y:number,z:number,r:number,h:number){return add(g,cylinderGeo(),s,x,y,z,r,h,r);}
function branch(g:THREE.Group,a:THREE.Vector3,b:THREE.Vector3,r:number){const m=pole(g,'bark',0,0,0,r,a.distanceTo(b));m.position.copy(a).add(b).multiplyScalar(.5);m.quaternion.setFromUnitVectors(new THREE.Vector3(0,1,0),b.clone().sub(a).normalize());return m;}
function roof(g:THREE.Group,y:number,w:number,h:number,d:number,s:Surface='roof') {
  const key=`gable:${w}:${h}:${d}`;const geometry=geo(key,()=>{const shape=new THREE.Shape();shape.moveTo(-w/2,0);shape.lineTo(w/2,0);shape.lineTo(0,h);shape.closePath();const gg=new THREE.ExtrudeGeometry(shape,{depth:d,bevelEnabled:false});gg.translate(0,0,-d/2);return gg;});const mesh=add(g,geometry,s,0,y,0);if(s==='roof')mesh.material=[material('plaster'),material('roof')];
}
function foliage(g:THREE.Group,r:()=>number,pine:boolean,bush=false){
  const card=geo('foliage-card',()=>new THREE.PlaneGeometry(1,1));
  const count=bush?140:pine?380:440;
  for(let i=0;i<count;i++) { const a=r()*Math.PI*2;const y=bush?.15+r()*.9:pine?1.1+r()*3.7:2.4+r()*2; const rad=bush?Math.sqrt(r())*.65:pine?(4.9-y)*.31*Math.sqrt(r()):Math.sqrt(r())*1.35*Math.sqrt(Math.max(.05,1-((y-3.4)/1.3)**2));const size=pine?.65:.19+r()*.22;
    const m=add(g,card,pine?'needle':'leaf',Math.cos(a)*rad,y,Math.sin(a)*rad,size,size,1);m.rotation.set(r()*Math.PI,r()*Math.PI,r()*Math.PI);
  }
}
function rock(g:THREE.Group,r:()=>number,variant:number,x=0,z=0,scale=1){const v=((variant%3)+3)%3;const geometry=geo(`rock-${v}`,()=>{const gg=new THREE.IcosahedronGeometry(1,2);const p=gg.getAttribute('position');for(let i=0;i<p.count;i++){const k=.88+.08*Math.sin(p.getX(i)*7+p.getY(i)*3+v)+.04*Math.sin(p.getZ(i)*11+p.getX(i)*4);p.setXYZ(i,p.getX(i)*k,p.getY(i)*k,p.getZ(i)*k);}gg.computeVertexNormals();return gg;});const m=add(g,geometry,'stone',x,0,z,scale*(v===1?1.2:.8),scale*(v===2?.4:.65),scale*.75);m.rotation.y=r()*6.28;geometry.computeBoundingBox();m.position.y=-geometry.boundingBox!.min.y*m.scale.y;}
function make(kind:PropKind,variant:number,seed:number,length:number){
  const g=new THREE.Group();const r=random(seed);g.name=kind;
  if(kind==='pine'||kind==='broadleaf-tree'||kind==='dead-tree'||kind==='stump'||kind==='bush') {
    if(kind!=='bush')pole(g,'bark',0,kind==='stump'?.24:1.55,0,kind==='stump'?.31:.16,kind==='stump'?.48:3.1);
    if(kind==='stump'){for(let i=0;i<5;i++){const a=i*1.256;branch(g,new THREE.Vector3(0,.2,0),new THREE.Vector3(Math.cos(a)*.5,.07,Math.sin(a)*.5),.1);}}
    else if(kind==='dead-tree'||kind==='broadleaf-tree'||kind==='pine') {for(let i=0;i<12;i++){const a=i*2.4;const y=.9+i*.19;const radius=kind==='pine'?(3.6-y)*.38: .7+r()*.6;branch(g,new THREE.Vector3(0,y,0),new THREE.Vector3(Math.cos(a)*radius,y+.35,Math.sin(a)*radius),.045);}}
    if(kind!=='dead-tree'&&kind!=='stump')foliage(g,r,kind==='pine',kind==='bush');
  } else if(kind==='rock')rock(g,r,variant);
  else if(kind==='boulder-cluster'){for(let i=0;i<5;i++)rock(g,r,i%3,(r()-.5)*1.6,(r()-.5)*1.6,.5+r()*.7);}
  else if(kind==='cottage') {
    box(g,'plaster',0,1.45,0,4.8,2.9,3.9);roof(g,2.9,5.3,1.6,4.4);box(g,'stone',1.5,3.9,.5,.6,1.7,.6);
    box(g,'wood',0,1.2,1.97,1.05,2.4,.08);box(g,'stone',0,.08,2.15,1.3,.16,.5);
    for(const x of [-.6,.6])box(g,'wood',x,1.24,2.03,.13,2.48,.13);box(g,'wood',0,2.47,2.03,1.3,.13,.13);box(g,'iron',.35,1.15,2.05,.06,.15,.06);
    for(const z of [-1.85,1.85])box(g,'wood',-2.42,1.45,z,.12,2.9,.15);box(g,'wood',0,2.8,2,4.8,.15,.15);
    for(const x of [-2.3,2.3])box(g,'wood',x,1.45,2,.17,2.9,.15);
    for(const x of [-1.45,1.45]){box(g,'dark',x,1.8,1.99,.85,.95,.07);box(g,'wood',x,1.8,2.05,.04,.95,.05);box(g,'wood',x,1.8,2.05,.85,.04,.05);}
  } else if(kind==='stone-wall'||kind==='ruined-wall') {
    for(let row=0;row<5;row++)for(let i=0;i<5;i++){if(kind==='ruined-wall'&&row>1&&r()<row*.16)continue;box(g,'stone',(i-2)*.58+(row%2)*.13,row*.36+.18,0,.55,.34,.48);}
  } else if(kind==='wooden-fence'||kind==='gate') {
    for(const x of [-1.35,1.35])box(g,'wood',x,.85,0,.18,1.7,.18);
    for(const y of [.55,1.2])box(g,'wood',0,y,0,2.8,.12,.12);
    if(kind==='gate'){for(let i=0;i<7;i++)box(g,'wood',-1.1+i*.36,.8,0,.14,1.35,.1);const brace=box(g,'wood',0,.85,.1,2.6,.12,.1);brace.rotation.z=.38;}
  } else if(kind==='wooden-bridge') {
    for(let i=0;i<Math.ceil(length/.26);i++)box(g,'wood',0,.13,-length/2+(i+.5)*length/Math.ceil(length/.26),2,.26,length/Math.ceil(length/.26)-.015);
    for(const x of [-.95,.95]){box(g,'wood',x,1.1,0,.12,.12,length);for(let i=0;i<=Math.ceil(length);i++)box(g,'wood',x,.65,-length/2+.07+i*(length-.14)/Math.ceil(length),.14,1.3,.14);}
  } else if(kind==='well') {
    const ring=geo('well-ring',()=>new THREE.CylinderGeometry(.8,.85,1,24,1,true));add(g,ring,'stone',0,.5,0);const inner=geo('well-inner',()=>new THREE.CylinderGeometry(.65,.65,.95,24,1,true));const innerMesh=add(g,inner,'stone',0,.5,0);innerMesh.material=material('stone');innerMesh.scale.x=-1;
    for(const x of [-.95,.95])box(g,'wood',x,1.35,0,.14,2.7,.14);roof(g,2.65,2.5,.6,1.8);const ax=pole(g,'wood',0,1.9,0,.07,2);ax.rotation.z=Math.PI/2;pole(g,'cloth',0,1.25,0,.025,1.3);
  } else if(kind==='torch'||kind==='brazier'||kind==='campfire') {
    const h=kind==='torch'?1.65:kind==='brazier'?.95:.2;
    if(kind==='torch')pole(g,'wood',0,h/2,0,.06,h);
    else if(kind==='brazier'){for(let i=0;i<3;i++){const a=i*2.094;pole(g,'iron',Math.cos(a)*.24,.35,Math.sin(a)*.24,.045,.7);}add(g,geo('bowl',()=>new THREE.SphereGeometry(.4,16,8,0,Math.PI*2,Math.PI/2,Math.PI/2)),'iron',0,.85,0);}
    else {for(let i=0;i<8;i++){const a=i*.785;rock(g,r,2,Math.cos(a)*.5,Math.sin(a)*.5,.25);}for(let i=0;i<3;i++){const log=pole(g,'bark',0,.17,0,.11,.8);log.rotation.set(Math.PI/2,0,i*1.05);}}
    for(let i=0;i<5;i++){const flame=add(g,geo('flame',()=>new THREE.SphereGeometry(1,8,6)),'fire',(r()-.5)*.16,h+.1+r()*.17,(r()-.5)*.16,.055,.16+r()*.13,.055);flame.castShadow=flame.receiveShadow=false;}
    const hook=new THREE.Object3D();hook.name='light-hook';hook.position.set(0,h+.35,0);g.add(hook);g.userData.lightPosition=[0,h+.35,0];
  } else if(kind==='barrel') {
    add(g,geo('barrel',()=>new THREE.LatheGeometry([new THREE.Vector2(0,0),new THREE.Vector2(.32,0),new THREE.Vector2(.4,.2),new THREE.Vector2(.42,.45),new THREE.Vector2(.4,.7),new THREE.Vector2(.32,.9),new THREE.Vector2(0,.9)],16)),'wood',0,0,0);
    for(const y of [.14,.75])add(g,geo('barrel-hoop',()=>new THREE.TorusGeometry(.385,.025,6,16)),'iron',0,y,0).rotation.x=Math.PI/2;
  } else if(kind==='crate'||kind==='chest') {
    const s=kind==='crate'?1:[.65,1,1.45][Math.abs(variant)%3];box(g,'wood',0,.35*s,0,1.1*s,.7*s,.7*s);for(const x of [-.42,.42])box(g,'iron',x*s,.36*s,.36*s,.055*s,.7*s,.025);
    if(kind==='chest'){box(g,'wood',0,.76*s,0,1.12*s,.14*s,.72*s);box(g,'iron',0,.55*s,.37*s,.12*s,.18*s,.04);}
    else for(const y of [.08,.63])box(g,'wood',0,y,.37,1.1,.1,.05);
  } else if(kind==='signpost') {pole(g,'wood',0,1,0,.08,2);box(g,'wood',.25,1.7,0,1.2,.36,.12);}
  else if(kind==='tent') {roof(g,.02,2.8,2.2,3.1,'cloth');box(g,'dark',0,.85,1.57,1.1,1.7,.02);for(const z of [-1.7,1.7])pole(g,'wood',0,1.1,z,.045,2.2);}
  // Rest all geometry on the ground, including rotated foliage and irregular rocks.
  const bounds=new THREE.Box3().setFromObject(g);if(Number.isFinite(bounds.min.y)&&bounds.min.y!==0){for(const child of g.children)child.position.y-=bounds.min.y;if(g.userData.lightPosition)g.userData.lightPosition[1]-=bounds.min.y;}
  return g;
}
/** Cached finite seed/variant buckets keep geometry/material ownership in the library. Do not dispose individual prop resources. */
export function createProp(kind:PropKind,options:PropOptions={}):THREE.Object3D {
  if(!PROP_KINDS.includes(kind))throw new Error(`Unknown prop kind: ${kind}`);
  const seed=((options.seed??1)>>>0)%16,variant=Math.trunc(options.variant??0);const length=Math.max(.5,Math.min(30,options.length??4));const key=`${kind}:${seed}:${variant%3}:${kind==='wooden-bridge'?length:0}`;
  let template=templates.get(key);if(!template){template=make(kind,variant,seed,length);templates.set(key,template);}
  const result=template.clone(true);result.scale.setScalar(options.scale??1);return result;
}
/** One InstancedMesh per shared component; no frame callbacks, allocations, or texture updates. */
export function createPropInstances(kind:PropKind,transforms:readonly PropTransform[]):THREE.Group {
  if(!['pine','broadleaf-tree','dead-tree','stump','bush','rock','boulder-cluster'].includes(kind))throw new Error(`${kind} does not support scatter instancing`);
  const result=new THREE.Group();result.name=`${kind}-instances`;
  const batches=new Map<string,{geometry:THREE.BufferGeometry;material:THREE.Material;matrices:THREE.Matrix4[]}>();
  const world=new THREE.Matrix4(),q=new THREE.Quaternion(),p=new THREE.Vector3(),s=new THREE.Vector3();
  for(const t of transforms){const prop=createProp(kind,{seed:t.seed,variant:t.variant});prop.updateMatrixWorld(true);Array.isArray(t.position)?p.fromArray(t.position):p.copy(t.position);q.setFromAxisAngle(THREE.Object3D.DEFAULT_UP,t.rotation??0);s.setScalar(t.scale??1);world.compose(p,q,s);
    prop.traverse(o=>{if(!(o instanceof THREE.Mesh))return;const mat=o.material as THREE.Material;const key=o.geometry.uuid+mat.uuid;let batch=batches.get(key);if(!batch){batch={geometry:o.geometry,material:mat,matrices:[]};batches.set(key,batch);}batch.matrices.push(new THREE.Matrix4().multiplyMatrices(world,o.matrixWorld));});
  }
  for(const batch of batches.values()){const mesh=new THREE.InstancedMesh(batch.geometry,batch.material,batch.matrices.length);batch.matrices.forEach((m,i)=>mesh.setMatrixAt(i,m));mesh.castShadow=mesh.receiveShadow=true;mesh.computeBoundingSphere();result.add(mesh);}return result;
}
export function triangleCount(prop:THREE.Object3D){let count=0;prop.traverse(o=>{if(o instanceof THREE.Mesh){const triangles=(o.geometry.index?.count??o.geometry.getAttribute('position').count)/3;count+=triangles*(o instanceof THREE.InstancedMesh?o.count:1);}});return count;}
