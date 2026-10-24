import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

export type PropKind = 'gothic-cottage';
export interface PropOptions {seed?:number;scale?:number;variant?:number}
const cache=new Map<PropKind,Promise<THREE.Group>>();
const loaded=new Map<PropKind,THREE.Group>();
/** Preload once before calling the synchronous placement API. Clones share GPU resources. */
export function preloadProp(kind:PropKind):Promise<THREE.Group>{
  let promise=cache.get(kind);if(promise)return promise;
  const assetPath='/game/props/gothic-cottage/v04/gothic-cottage-v04.glb';
  // The live server excludes public/game from its watcher. Serve newly created
  // versions through the root static route in development without restarting it.
  promise=new GLTFLoader().loadAsync(import.meta.env.DEV?'/public'+assetPath:assetPath).then(gltf=>{
    const high=gltf.scene.getObjectByName('GothicCottage_LOD0');const low=gltf.scene.getObjectByName('GothicCottage_LOD1');
    if(!high||!low)throw new Error('Cottage asset is missing a named LOD');
    const lod=new THREE.LOD();lod.name='gothic-cottage-lod';lod.addLevel(high.clone(true),0);lod.addLevel(low.clone(true),24);
    const template=new THREE.Group();template.name=kind;template.add(lod);
    const glass=gltf.scene.getObjectByName('GothicCottage_Glass');if(glass)template.add(glass.clone(true));
    const bounds=new THREE.Box3().setFromObject(template);lod.position.y=-bounds.min.y;
    template.traverse(o=>{if(o instanceof THREE.Mesh){o.castShadow=o.receiveShadow=true;for(const mat of Array.isArray(o.material)?o.material:[o.material]){if(mat instanceof THREE.MeshStandardMaterial){for(const tex of [mat.map,mat.normalMap,mat.roughnessMap,mat.metalnessMap,mat.aoMap])if(tex)tex.anisotropy=4;}}}});
    template.traverse(o=>{if(o instanceof THREE.Mesh&&o.name==='GothicCottage_Glass'){o.castShadow=false;for(const mat of Array.isArray(o.material)?o.material:[o.material])mat.side=THREE.DoubleSide;}});
    template.userData.version='v04-closed-shutters';template.userData.lodDistance=24;loaded.set(kind,template);return template;
  });cache.set(kind,promise);return promise;
}
export function createProp(kind:PropKind,options:PropOptions={}):THREE.Object3D{
  const template=loaded.get(kind);if(!template)throw new Error(`Call await preloadProp('${kind}') before createProp`);
  const result=template.clone(true);result.scale.setScalar(options.scale??1);return result;
}
export function propMetrics(prop:THREE.Object3D){
  const counts:{name:string;triangles:number}[]=[];const textures=new Map<string,THREE.Texture>();
  prop.traverse(o=>{if(o instanceof THREE.Mesh){counts.push({name:o.name,triangles:(o.geometry.index?.count??o.geometry.getAttribute('position').count)/3});for(const mat of Array.isArray(o.material)?o.material:[o.material]){if(mat instanceof THREE.MeshStandardMaterial)for(const tex of [mat.map,mat.normalMap,mat.roughnessMap,mat.metalnessMap,mat.aoMap])if(tex)textures.set(tex.uuid,tex);}}});
  const images=[...textures.values()].map(t=>{const image=t.image as {width:number;height:number};return {width:image.width,height:image.height,mipmaps:t.generateMipmaps};});
  const glassTriangles=counts.filter(c=>c.name==='GothicCottage_Glass').reduce((s,c)=>s+c.triangles,0);
  return {lods:counts.filter(c=>c.name!=='GothicCottage_Glass').map(c=>({...c,triangles:c.triangles+glassTriangles})),glassTriangles,textures:images,gpuTextureMiB:images.reduce((s,t)=>s+t.width*t.height*4*(t.mipmaps?4/3:1),0)/1048576};
}
