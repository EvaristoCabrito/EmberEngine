import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {RoomEnvironment} from 'three/addons/environments/RoomEnvironment.js';
import {preloadProp,createProp,propMetrics} from './baked';

const scene=new THREE.Scene();
const renderer=new THREE.WebGLRenderer({antialias:true});renderer.setPixelRatio(Math.min(devicePixelRatio,1.5));renderer.setSize(innerWidth,innerHeight);renderer.outputColorSpace=THREE.SRGBColorSpace;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1;renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;document.body.append(renderer.domElement);
const camera=new THREE.PerspectiveCamera(42,innerWidth/innerHeight,.1,150);camera.position.set(10,7.5,12);const controls=new OrbitControls(camera,renderer.domElement);controls.target.set(0,2.65,0);controls.maxPolarAngle=Math.PI*.49;controls.minDistance=5;controls.maxDistance=65;controls.update();
const ambient=new THREE.HemisphereLight('#bac6d5','#343a32',2.0);scene.add(ambient);
const key=new THREE.DirectionalLight('#cfdbeb',2.0);key.position.set(-7,11,8);key.castShadow=true;key.shadow.mapSize.set(2048,2048);Object.assign(key.shadow.camera,{left:-8,right:8,top:10,bottom:-7,near:.5,far:35});key.shadow.normalBias=.015;scene.add(key);
const rim=new THREE.DirectionalLight('#6e88b0',.6);rim.position.set(6,8,-7);scene.add(rim);
// Preview-only warm light. Library assets never create PointLights.
const warm=new THREE.PointLight('#ffb065',0,12,2);warm.position.set(-.95,2.4,3.4);warm.castShadow=true;warm.shadow.mapSize.set(1024,1024);warm.shadow.normalBias=.055;warm.shadow.bias=-.0006;scene.add(warm);
const ground=new THREE.Mesh(new THREE.PlaneGeometry(120,120),new THREE.MeshStandardMaterial({color:'#444b46',roughness:.82}));ground.rotation.x=-Math.PI/2;ground.position.y=-.007;ground.receiveShadow=true;scene.add(ground);
const reference=new THREE.Mesh(new THREE.CylinderGeometry(.035,.035,2.65,12),new THREE.MeshStandardMaterial({color:'#abb3ad',roughness:.7}));reference.position.set(-3.9,1.325,2.9);reference.castShadow=reference.receiveShadow=true;scene.add(reference);
// Height markers are actual geometry and share the reference material.
for(const y of [.05,1,2,2.65]){const marker=new THREE.Mesh(new THREE.BoxGeometry(.18,.035,.04),reference.material);marker.position.set(-3.9,y,2.94);scene.add(marker);}
let prop:THREE.Object3D;let mode='day';
function render(){renderer.render(scene,camera);}
function lighting(next:string){mode=next;const night=next==='night';scene.background=new THREE.Color(night?'#111923':'#737f86');ambient.color.set(night?'#788da9':'#c5d0d9');ambient.groundColor.set(night?'#171b20':'#53594d');ambient.intensity=night?.7:2.2;key.color.set(night?'#86a8de':'#d6e0e8');key.intensity=night?1.25:2.2;rim.intensity=night?.5:.65;warm.intensity=night?28:0;renderer.toneMappingExposure=night?1.2:1;document.querySelectorAll<HTMLButtonElement>('[data-light]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.light===next)));render();}
function view(which:string){if(which==='side'){camera.position.set(15,4.5,0);controls.target.set(0,2.7,0);}else if(which==='front'){camera.position.set(0,5,15);controls.target.set(0,2.7,0);}else if(which==='far'){camera.position.set(26,19,32);controls.target.set(0,2.5,0);}else{camera.position.set(10,7.5,12);controls.target.set(0,2.65,0);}controls.update();render();}
controls.addEventListener('change',render);document.querySelectorAll<HTMLButtonElement>('[data-light]').forEach(b=>b.addEventListener('click',()=>lighting(b.dataset.light!)));document.querySelectorAll<HTMLButtonElement>('[data-view]').forEach(b=>b.addEventListener('click',()=>view(b.dataset.view!)));
addEventListener('resize',()=>{camera.aspect=innerWidth/innerHeight;camera.updateProjectionMatrix();renderer.setSize(innerWidth,innerHeight);render();});
try{
  await preloadProp('gothic-cottage');prop=createProp('gothic-cottage');scene.add(prop);
  const pmrem=new THREE.PMREMGenerator(renderer);const environment=pmrem.fromScene(new RoomEnvironment(),.04).texture;pmrem.dispose();
  prop.traverse(o=>{if(o instanceof THREE.Mesh&&o.name==='GothicCottage_Glass'){for(const mat of Array.isArray(o.material)?o.material:[o.material])if(mat instanceof THREE.MeshPhysicalMaterial){mat.envMap=environment;mat.envMapIntensity=.055;mat.needsUpdate=true;}}});
  const metrics=propMetrics(prop);const bounds=new THREE.Box3().setFromObject(prop);
  document.querySelector('#status')!.textContent=`LOD0 ${metrics.lods[0].triangles.toLocaleString()} · LOD1 ${metrics.lods[1].triangles.toLocaleString()} triangles · ${metrics.gpuTextureMiB.toFixed(1)} MiB shared maps`;
  Object.assign(window,{__propsV2:{ready:true,lighting,view,metrics,bounds:{min:bounds.min.toArray(),max:bounds.max.toArray()},renderer,scene,camera,prop,get mode(){return mode;}}});lighting('day');
}catch(error){document.querySelector('#status')!.textContent=String(error);console.error(error);}
