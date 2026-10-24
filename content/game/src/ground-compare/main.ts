// Ground comparison: the 12 basic Ember terrains side by side in the 3D world, lit by Engine2's stage,
// with Kael for scale. "Claude" shows public/game/ground-claude/<terrain>/, "GPT" shows GPT's Task 9
// set from public/game/ground/<family>/ (missing ones say so). Same texture repeat as the terrain
// shader: one texture per 4 world units.

import * as THREE from 'three';
import { Stage, type TimeOfDay } from '../render/stage';
import { CameraRig } from '../render/cameraRig';
import { UnitActor } from '../units/actor';
import { UNITS } from '../units/catalog';
import { initFrames, pumpUploads } from '../units/frames';
import { groundTextures } from '../render/textures';
import { TERRAIN_INFO } from '../map/board';
import type { TerrainId } from '../ember/types';
import { maxAnisotropy } from '../render/gpu';

const TERRAINS: { id: string; name: string; gpt: string[] }[] = [
  { id: 'plains', name: 'Planície', gpt: ['grass-plains', 'grass_plains', 'plains'] },
  { id: 'woods', name: 'Bosque', gpt: ['forest-floor', 'forest_floor', 'woods'] },
  { id: 'ruins', name: 'Ruínas', gpt: ['ruin-pavers', 'ruin_pavers', 'ruins'] },
  { id: 'water', name: 'Água (leito)', gpt: ['river-bed', 'river_bed', 'water'] },
  { id: 'ember', name: 'Brasa', gpt: ['ash-embers', 'ash_embers', 'ember'] },
  { id: 'hill', name: 'Colina', gpt: ['hill-grass', 'hill_grass', 'hill'] },
  { id: 'flame', name: 'Chama', gpt: ['ash-embers', 'ash_embers', 'flame'] },
  { id: 'column', name: 'Coluna', gpt: ['column-rock', 'column_rock', 'column'] },
  { id: 'nave', name: 'Laje', gpt: ['nave-hall-slabs', 'nave_hall_slabs', 'nave'] },
  { id: 'barricade', name: 'Barricada', gpt: ['barricade-dirt', 'barricade_dirt', 'barricade'] },
  { id: 'door', name: 'Porta', gpt: ['dungeon-flagstone', 'dungeon_flagstone', 'door'] },
  { id: 'snow', name: 'Neve', gpt: ['fresh-snow', 'fresh_snow', 'snow'] },
];
const RADIUS = 2.6, COLS = 4, GAP = 5.6, REPEAT = 4;

const view = document.getElementById('view')!;
const stage = new Stage(view);
const rig = new CameraRig(stage.camera, stage.renderer.domElement);
rig.mode = 'free';
stage.fitToBoard(COLS * GAP + 4, 3 * GAP + 4);
rig.frame({ x0: -COLS * GAP / 2, x1: COLS * GAP / 2, z0: -1.5 * GAP, z1: 1.5 * GAP }, 0);
stage.scene.background = new THREE.Color('#1a1714');

const loader = new THREE.TextureLoader();
const cache = new Map<string, Promise<THREE.Texture | null>>();
function tex(url: string, srgb: boolean): Promise<THREE.Texture | null> {
  if (!cache.has(url)) cache.set(url, loader.loadAsync(url).then(t => {
    t.wrapS = t.wrapT = THREE.RepeatWrapping;
    t.colorSpace = srgb ? THREE.SRGBColorSpace : THREE.NoColorSpace;
    t.anisotropy = maxAnisotropy(stage.renderer);
    return t;
  }, () => null));
  return cache.get(url)!;
}

interface Tile { mesh: THREE.Mesh; label: HTMLDivElement; t: typeof TERRAINS[number] }
const tiles: Tile[] = [];
const labels = document.getElementById('labels')!;
TERRAINS.forEach((t, i) => {
  const cx = (i % COLS - (COLS - 1) / 2) * GAP, cz = (Math.floor(i / COLS) - 1) * GAP;
  const g = new THREE.CircleGeometry(RADIUS, 6, Math.PI / 6).rotateX(-Math.PI / 2).translate(cx, 0, cz);
  // world-space UVs, exactly like the terrain shader (vWP.xz / 4)
  const p = g.attributes.position, uv = g.attributes.uv;
  for (let k = 0; k < p.count; k++) uv.setXY(k, p.getX(k) / REPEAT, -p.getZ(k) / REPEAT);
  g.computeTangents?.();
  const mesh = new THREE.Mesh(g, new THREE.MeshStandardMaterial({ color: '#333' }));
  mesh.receiveShadow = true;
  stage.scene.add(mesh);
  const label = document.createElement('div');
  label.className = 'label';
  labels.appendChild(label);
  tiles.push({ mesh, label, t });
});

let source: 'claude' | 'gpt' | 'proto' = 'claude';
// The prototype's look: Engine2's procedural ground textures, chosen by terrain exactly as the board does.
const proto = groundTextures(stage.renderer) as unknown as Record<string, THREE.Texture>;
async function applySource(): Promise<void> {
  for (const tile of tiles) {
    if (source === 'proto') {
      const mat = tile.mesh.material as THREE.MeshStandardMaterial;
      const surface = TERRAIN_INFO[tile.t.id as TerrainId]?.surface as string;
      const t = proto[surface] ?? null;
      if (t) { t.wrapS = t.wrapT = THREE.RepeatWrapping; }
      mat.map = t; mat.normalMap = null; mat.roughnessMap = null; mat.roughness = 0.92;
      mat.color.set(t ? '#ffffff' : '#2a2724'); mat.needsUpdate = true;
      tile.label.textContent = tile.t.name;
      continue;
    }
    const folders = source === 'claude' ? [`/game/ground-claude/${tile.t.id}`] : tile.t.gpt.map(f => `/game/ground/${f}`);
    let found: [THREE.Texture, THREE.Texture | null, THREE.Texture | null] | null = null;
    for (const folder of folders) {
      const color = await tex(`${folder}/color.png`, true);
      if (color) { found = [color, await tex(`${folder}/normal.png`, false), await tex(`${folder}/rough.png`, false)]; break; }
    }
    const mat = tile.mesh.material as THREE.MeshStandardMaterial;
    mat.map = found?.[0] ?? null;
    mat.normalMap = found?.[1] ?? null;
    mat.roughnessMap = found?.[2] ?? null;
    mat.roughness = 1;
    mat.color.set(found ? '#ffffff' : '#2a2724');
    mat.needsUpdate = true;
    tile.label.textContent = `${tile.t.name}${found ? '' : ' — ainda não existe'}`;
  }
  for (const b of document.querySelectorAll<HTMLButtonElement>('[data-source]')) b.classList.toggle('on', b.dataset.source === source);
}
void applySource();

// Kael for scale, at the front
initFrames(stage.renderer);
const kael = new UnitActor(UNITS.find(u => u.key === 'kael')!);
kael.x = 0; kael.z = 1.5 * GAP + 0.5;
stage.scene.add(kael.mesh);
void kael.ready();

const v = new THREE.Vector3();
stage.onFrame(dt => {
  pumpUploads();
  kael.update(dt, rig.facingYaw, () => 0);
  const r = stage.renderer.domElement.getBoundingClientRect();
  for (const tile of tiles) {
    const pos = (tile.mesh.geometry as THREE.BufferGeometry).boundingSphere ?? (tile.mesh.geometry.computeBoundingSphere(), tile.mesh.geometry.boundingSphere!);
    v.copy(pos.center).add(new THREE.Vector3(0, 0, RADIUS)).project(stage.camera);
    tile.label.style.transform = `translate(${(v.x * 0.5 + 0.5) * r.width}px, ${(-v.y * 0.5 + 0.5) * r.height}px) translate(-50%, 0)`;
    tile.label.style.display = v.z < 1 ? '' : 'none';
  }
});

for (const b of document.querySelectorAll<HTMLButtonElement>('[data-source]')) b.addEventListener('click', () => { source = b.dataset.source as 'claude' | 'gpt' | 'proto'; void applySource(); });
for (const b of document.querySelectorAll<HTMLButtonElement>('[data-tod]')) b.addEventListener('click', () => {
  stage.setTimeOfDay(b.dataset.tod as TimeOfDay);
  for (const o of document.querySelectorAll<HTMLButtonElement>('[data-tod]')) o.classList.toggle('on', o === b);
});
document.getElementById('reset')!.addEventListener('click', () => rig.resetAngle());
