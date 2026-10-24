import * as THREE from 'three';
import { activeVfxCount, configureVfx, disposeVfx, playSpell, SPELL_EFFECTS, SPELL_KINDS, updateVfx, spellIconFor, spellLabel, spell3DEffectFor, type EffectKind, type ImpactEvent } from './index';

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(36, innerWidth / innerHeight, 0.1, 120);
camera.position.set(8, 8.5, 14); camera.lookAt(1.3, 0.8, 0);
const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'low-power', preserveDrawingBuffer: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5)); renderer.setSize(innerWidth, innerHeight);
renderer.shadowMap.enabled = true; renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.outputColorSpace = THREE.SRGBColorSpace; renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1;
document.body.appendChild(renderer.domElement);
const ambient = new THREE.HemisphereLight(0xc4d4dc, 0x222016, 2.1); scene.add(ambient);
const sun = new THREE.DirectionalLight(0xd8e0dd, 2.0); sun.position.set(-4, 8, 6); scene.add(sun);
const ground = new THREE.Mesh(new THREE.PlaneGeometry(30, 22), new THREE.MeshStandardMaterial({ color: 0x343a37, roughness: 1 })); ground.rotation.x = -Math.PI / 2; ground.position.y = -0.02; scene.add(ground);
const from = new THREE.Vector3(-2 * Math.sqrt(3), 0.03, 0);
const to = new THREE.Vector3(2 * Math.sqrt(3), 0.03, 0);
const targets = [{ id: 'target', position: to }];
const impacts: ImpactEvent[] = [];
let manual = false;
let active = false;
const status = document.querySelector<HTMLParagraphElement>('#status')!;
configureVfx(scene, { camera, viewport: new THREE.Vector2(innerWidth, innerHeight), onImpact: event => { impacts.push(event); status.textContent = `${event.kind}: impact ${event.targetId ?? event.targets.map(t => t.id).join(', ')}`; } });
async function character(dir: string, position: THREE.Vector3, flip = false): Promise<THREE.Sprite> {
  const texture = await new THREE.TextureLoader().loadAsync(`/game/sprites-e2/${dir}/1.png`);
  texture.colorSpace = THREE.SRGBColorSpace;
  const material = new THREE.SpriteMaterial({ map: texture, transparent: true, alphaTest: 0.025, toneMapped: false });
  const sprite = new THREE.Sprite(material);
  const image = texture.image as HTMLImageElement;
  sprite.scale.set(2.65 * image.width / image.height * (flip ? -1 : 1), 2.65, 1);
  sprite.position.copy(position).y += 2.65 / 2; scene.add(sprite); return sprite;
}
function mode(name: 'day' | 'night'): void {
  const night = name === 'night'; scene.background = new THREE.Color(night ? 0x101923 : 0x657277);
  ambient.intensity = night ? 0.45 : 2.1; sun.intensity = night ? 0.5 : 2; sun.color.set(night ? 0x89a4c4 : 0xd8e0dd); renderer.render(scene, camera);
}
document.querySelector('#day')!.addEventListener('click', () => mode('day'));
document.querySelector('#night')!.addEventListener('click', () => mode('night'));
async function cast(kind: EffectKind): Promise<void> {
  if (active) return; active = true; status.textContent = `${kind}: casting…`;
  try { await playSpell(kind, from, targets, scene); status.textContent = `${kind}: complete`; }
  finally { active = false; }
}
for (const kind of SPELL_KINDS) {
  const button = document.createElement('button');
  button.className = 'spell-card'; button.dataset.effect = kind; button.title = `${spellLabel(kind)} · 2D icon · 3D effect`;
  const icon = document.createElement('img'); icon.src = spellIconFor(kind); icon.alt = ''; icon.loading = 'lazy'; icon.width = 40; icon.height = 40;
  const label = document.createElement('span'); label.textContent = spellLabel(kind);
  const detail = document.createElement('small'); detail.textContent = `${spell3DEffectFor(kind)} · 2D icon`;
  button.append(icon, label, detail); button.addEventListener('click', () => void cast(kind)); document.querySelector('#spells')!.appendChild(button);
}
const memory = () => ({ geometries: renderer.info.memory.geometries, textures: renderer.info.memory.textures, programs: renderer.info.programs?.length ?? 0 });
const render = () => renderer.render(scene, camera);
const delay = (ms: number) => new Promise<void>(resolve => setTimeout(resolve, ms));
/** QA drives original simulation time without changing tuning or overlapping heavy casts. */
async function simulatedCast(kind: EffectKind, snapshotTime?: number): Promise<void> {
  configureVfx(scene, { manualUpdate: true }); manual = true;
  const promise = playSpell(kind, from, targets, scene);
  // Allow the original asynchronous atlas loaders to settle before advancing the timeline.
  await delay(160);
  let elapsed = 0;
  while (activeVfxCount(scene)) {
    updateVfx(scene, 1 / 30); elapsed += 1 / 30;
    render();
    if (snapshotTime != null && elapsed >= snapshotTime) return;
    if (elapsed > 30) { disposeVfx(scene); throw new Error(`${kind} failed to complete its original timeline`); }
    if (Math.round(elapsed * 30) % 12 === 0) await delay(0);
  }
  await promise; render();
}
async function drain(): Promise<void> { for (let n = 0; n < 900 && activeVfxCount(scene); n++) { updateVfx(scene, 1 / 30); render(); if (n % 12 === 0) await delay(0); } }
async function memoryTest(kind: EffectKind, count = 20) {
  await drain(); await simulatedCast(kind); await delay(160); render();
  const before = memory(); const observations = [];
  for (let n = 1; n <= count; n++) { await simulatedCast(kind); observations.push({ cast: n, ...memory() }); }
  await delay(160); render(); const after = memory();
  const result = { kind, count, before, after, observations, geometryDelta: after.geometries - before.geometries, textureDelta: after.textures - before.textures };
  console.log('renderer.info.memory before/after 20 casts', result); status.textContent = JSON.stringify(result, null, 2); return result;
}
document.querySelector('#memory')!.addEventListener('click', () => void memoryTest('fireball').finally(() => { manual = false; configureVfx(scene, { manualUpdate: false }); }));
addEventListener('resize', () => { camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight); configureVfx(scene, { viewport: new THREE.Vector2(innerWidth, innerHeight) }); });
addEventListener('pagehide', () => { disposeVfx(scene); scene.traverse(object => { if (object instanceof THREE.Sprite) { object.material.map?.dispose(); object.material.dispose(); } }); ground.geometry.dispose(); (ground.material as THREE.Material).dispose(); renderer.dispose(); });
function frame() { requestAnimationFrame(frame); if (!manual) render(); } frame(); mode('day');
await Promise.all([character('voss', from), character('militia-v2', to, true)]);
status.textContent = 'Ready — select a spell';
(window as unknown as { __vfx: unknown }).__vfx = { ready: true, scene, camera, renderer, effects: SPELL_EFFECTS, spells: SPELL_KINDS, impacts, from, to, mode, memory, memoryTest, simulatedCast, drain, render, dispose: () => disposeVfx(scene) };
