import { MeadowGrass } from '../render/meadowGrass';
// Engine2 map editor — step A: open any of Ember's maps as a 3D diorama board.
// Same map files as Ember, same hex layout; terrain edges are the player-selectable look.

import * as THREE from 'three';
import { Stage, type TimeOfDay } from '../render/stage';
import { CameraRig } from '../render/cameraRig';
import { groundTextures } from '../render/textures';
import { makeTerrainMaterial } from '../render/terrainMaterial';
import { buildSmoothGround, buildHardGround, buildWater, groundHeightAt, type EdgeStyle } from '../render/terrainMesh';
import { Board, TERRAIN_INFO } from '../map/board';
import { listEmberMaps, saveMap, type MapEntry } from '../map/emberMap';
import { TerrainTools } from './terrainTools';
import { corner, hexAt } from '../core/hex';
import { CastShowcase } from './cast';
import { DecorLayer } from '../world/decor';
import { DecorationTools } from './decorationTools';
import { BackgroundTools } from './backgroundTools';
import { mapBackgroundUrl, resolvedMapBackground } from './mapBackgrounds';

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
const view = $('view'), status = $('status'), hud = $('hud');

const stage = new Stage(view);
const rig = new CameraRig(stage.camera, stage.renderer.domElement);
rig.mode = 'free'; // the editor spins freely to test angles; the campaign stays locked
const tex = groundTextures(stage.renderer);
const groundMat = makeTerrainMaterial(tex);
const waterMat = new THREE.MeshStandardMaterial({
  color: '#2f86a8', roughness: 0.12, metalness: 0.05, transparent: true, opacity: 0.74,
  normalMap: tex.ripple, normalScale: new THREE.Vector2(0.6, 0.6), depthWrite: false,
});
stage.onFrame((_dt, t) => tex.ripple.offset.set(t * 0.02, t * 0.013));


let board: Board | null = null;
let edges: EdgeStyle = 'smooth';
let ground: THREE.Mesh | null = null;
let water: THREE.Mesh | null = null;
let grid: THREE.LineSegments | null = null;
let activeBackdrop: THREE.Texture | null = null;
let activeBackdropId: string | null = null;
let backdropRequest = 0;
const backdropLoader = new THREE.TextureLoader();
function cropBackdropToView(texture: THREE.Texture): void {
  const image = texture.image as HTMLImageElement | undefined;
  if (!image?.width || !image?.height) return;
  const viewAspect = view.clientWidth / Math.max(1, view.clientHeight), imageAspect = image.width / image.height;
  texture.wrapS = texture.wrapT = THREE.ClampToEdgeWrapping;
  if (imageAspect > viewAspect) {
    const scale = viewAspect / imageAspect;
    texture.repeat.set(scale, 1); texture.offset.set((1 - scale) / 2, 0);
  } else {
    const scale = imageAspect / viewAspect;
    texture.repeat.set(1, scale); texture.offset.set(0, (1 - scale) / 2);
  }
  texture.updateMatrix();
}
new ResizeObserver(() => { if (activeBackdrop) cropBackdropToView(activeBackdrop); }).observe(view);
const markers = new THREE.Group();
stage.scene.add(markers);
const groundAt = (x: number, z: number) => (board ? groundHeightAt(board, edges, x, z) : 0);
const cast = new CastShowcase(stage, rig, groundAt);
const decor = new DecorLayer(stage.renderer, stage.scene);
stage.onFrame((_, time) => { decor.face(rig.facingYaw); decor.updateLights(time); });

// ---- camera angle readout + back to the normal view
const camAngle = $('cam-angle');
stage.onFrame(() => {
  const deg = (r: number) => Math.round(THREE.MathUtils.radToDeg(r));
  const text = `Giro ${deg(rig.yaw)}° · Inclinação ${deg(rig.pitch)}°`;
  if (camAngle.textContent !== text) camAngle.textContent = text;
});
$('cam-reset').addEventListener('click', () => rig.resetAngle());

async function applyMapBackground(draft: import('../map/emberMap').EmberMapDraft): Promise<void> {
  const item = resolvedMapBackground(draft);
  const request = ++backdropRequest;
  if (!item) {
    const old = activeBackdrop;
    activeBackdrop = null; activeBackdropId = null;
    stage.scene.background = new THREE.Color('#000000');
    old?.dispose();
    return;
  }
  if (activeBackdrop && activeBackdropId === item.id) { stage.scene.background = activeBackdrop; return; }
  try {
    const texture = await backdropLoader.loadAsync(mapBackgroundUrl(item));
    if (request !== backdropRequest) { texture.dispose(); return; }
    texture.colorSpace = THREE.SRGBColorSpace;
    cropBackdropToView(texture);
    texture.needsUpdate = true;
    const old = activeBackdrop;
    activeBackdrop = texture; activeBackdropId = item.id;
    stage.scene.background = texture;
    old?.dispose();
  } catch (error) {
    if (request === backdropRequest) {
      console.warn(`[engine2] could not load map background ${item.id}`, error);
      const old = activeBackdrop;
      activeBackdrop = null; activeBackdropId = null;
      stage.scene.background = new THREE.Color('#000000');
      old?.dispose();
    }
  }
}

// ---- hover ring (the hex only shows where the cursor is)
function hexRing(rIn: number, rOut: number): THREE.BufferGeometry {
  const pos: number[] = [], idx: number[] = [];
  for (let k = 0; k < 6; k++) { const [ox, oz] = corner(0, 0, k, rOut), [ix, iz] = corner(0, 0, k, rIn); pos.push(ox, 0, oz, ix, 0, iz); }
  for (let k = 0; k < 6; k++) { const a = k * 2, b = ((k + 1) % 6) * 2; idx.push(a, b, a + 1, b, b + 1, a + 1); }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  return g;
}
const hover = new THREE.Mesh(hexRing(0.8, 0.97), new THREE.MeshBasicMaterial({ color: new THREE.Color(2.4, 2.0, 1.0), side: THREE.DoubleSide, transparent: true, depthWrite: false }));
hover.visible = false;
hover.renderOrder = 5;
stage.scene.add(hover);

function disposeMesh(m: THREE.Mesh | THREE.LineSegments | null): void {
  if (!m) return;
  stage.scene.remove(m);
  m.geometry.dispose();
}

/** (Re)build the ground in the current edge style. Applied on load or when the setting changes. */
const meadow = new MeadowGrass();
stage.scene.add(meadow.group);

function buildGround(): void {
  if (!board) return;
  const t0 = performance.now();
  disposeMesh(ground); disposeMesh(water);
  ground = new THREE.Mesh(edges === 'smooth' ? buildSmoothGround(board) : buildHardGround(board), groundMat);
  ground.castShadow = ground.receiveShadow = true;
  stage.scene.add(ground);
  const wg = buildWater(board, edges);
  water = wg ? new THREE.Mesh(wg, waterMat) : null;
  if (water) { water.receiveShadow = true; water.renderOrder = 2; stage.scene.add(water); }
  meadow.rebuild(board, groundAt);
  placeMarkers();
  buildGrid();
  // decorations stand on the ground of the active edge style
  for (const c of decor.group.children) c.position.y = groundAt(c.position.x, c.position.z);
  console.info(`[engine2] ground (${edges}) built in ${Math.round(performance.now() - t0)} ms`);
}

function placeMarkers(): void {
  markers.clear();
  if (!board) return;
  const d = board.draft;
  const sides: [unknown[] | undefined, string][] = [[d.playerSpawns, '#4f8fe0'], [d.enemySpawns, '#d9483b'], [d.neutralSpawns, '#e0c050']];
  for (const [list, color] of sides) {
    const mat = new THREE.MeshStandardMaterial({ color, roughness: 0.5 });
    for (const s of (list ?? []) as { x: number; y: number }[]) {
      if (s.x < 0 || s.y < 0 || s.x >= d.cols || s.y >= d.rows) continue;
      const cell = board.cell(s.x, s.y), y = cell.water ? cell.waterY : groundHeightAt(board, edges, cell.x, cell.z);
      const pawn = new THREE.Mesh(new THREE.CylinderGeometry(0.22, 0.3, 0.9, 16), mat);
      pawn.position.set(cell.x, y + 0.45, cell.z);
      pawn.castShadow = true;
      markers.add(pawn);
    }
  }
}

function buildGrid(): void {
  const wasOn = grid?.visible ?? false;
  disposeMesh(grid);
  grid = null;
  if (!board) return;
  const pos: number[] = [];
  for (const c of board.cells) {
    if (c.surface < 0) continue;
    const y = (c.water ? c.waterY : c.groundY) + 0.03;
    for (let k = 0; k < 6; k++) { const [ax, az] = corner(c.x, c.z, k, 0.985), [bx, bz] = corner(c.x, c.z, k + 1, 0.985); pos.push(ax, y, az, bx, y, bz); }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  grid = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ color: '#000000', transparent: true, opacity: 0.25, depthWrite: false }));
  grid.renderOrder = 3;
  grid.visible = wasOn;
  stage.scene.add(grid);
  $('grid').classList.toggle('on', wasOn);
}

async function openMap(entry: MapEntry): Promise<void> {
  status.textContent = `Carregando ${entry.id}…`;
  await new Promise(r => requestAnimationFrame(() => r(null)));
  if (tools.dirty && !confirm('Há alterações não salvas neste mapa. Abrir outro mesmo assim?')) { status.textContent = ''; return; }
  const file = await entry.load();
  board = new Board(file.draft);
  currentEntry = entry;
  tools.dirty = false; tools.setDraft(board.draft);
  backgroundTools.setDraft(board.draft);
  decorTools.setMode('off');
  const b = board.bounds(), w = b.x1 - b.x0 + 2.4, dpt = b.z1 - b.z0 + 2.4;
  // the map's own Ember time of day; indoor maps soften the sun on top of it
  const tod = (['day', 'noon', 'dawn', 'dusk', 'brightNight', 'darkNight'] as TimeOfDay[]).includes(file.draft.timeOfDay as TimeOfDay) ? file.draft.timeOfDay as TimeOfDay : 'day';
  stage.timeOfDay = tod;
  stage.scene.background = new THREE.Color('#000000');
  stage.fitToBoard(w, dpt);
  await applyMapBackground(file.draft);
  if (file.draft.environment === 'indoor') { stage.sun.intensity *= 0.4; stage.hemi.intensity *= 1.4; }
  $<HTMLSelectElement>('tod').value = tod;
  buildGround();
  const avgY = board.cells.reduce((s, c) => s + c.groundY, 0) / board.cells.length;
  rig.frame(b, avgY);
  $('title').textContent = `${file.draft.title || entry.id} · ${file.draft.cols}×${file.draft.rows} · ${entry.file}`;
  status.textContent = '';
  markers.visible = false; // the cast stands on the spawns instead of the plain markers
  await decor.populate(board, groundAt);
  cast.blocked = decor.blocked;
  await cast.populate(board);
  console.info(`[engine2] decorations: ${decor.stats.placed} placed, ${decor.stats.skippedArchitecture} 3D architecture skipped, ${decor.stats.missingArt.length} without art`);
  for (const li of document.querySelectorAll<HTMLLIElement>('#maps li')) li.classList.toggle('on', li.dataset.id === entry.id);
  (window as unknown as { __engine2: unknown }).__engine2 = { ready: true, map: entry.id, cells: board.cells.length, edges, rig, setEdges, stage, THREE, decor: () => decor.stats, tools, board: () => board, rebuild: () => rebuildBoard(true), save, entry: () => currentEntry };
}

function setEdges(style: EdgeStyle): void {
  edges = style;
  for (const btn of document.querySelectorAll<HTMLButtonElement>('[data-edges]')) btn.classList.toggle('on', btn.dataset.edges === style);
  buildGround();
}

// ---- hover info
const ray = new THREE.Raycaster(), mouse = new THREE.Vector2();
stage.renderer.domElement.addEventListener('pointermove', (e) => {
  if (!board || !ground) return;
  const r = stage.renderer.domElement.getBoundingClientRect();
  mouse.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
  ray.setFromCamera(mouse, stage.camera);
  const hit = ray.intersectObjects(water ? [ground, water] : [ground], false)[0];
  if (!hit) { hover.visible = false; hud.textContent = 'Passe o mouse sobre um hex'; return; }
  const [c, row] = hexAt(board.layout, hit.point.x, hit.point.z), cell = board.cell(c, row);
  hover.position.set(cell.x, (cell.water ? cell.waterY : groundHeightAt(board, edges, cell.x, cell.z)) + 0.04, cell.z);
  hover.visible = true;
  const info = TERRAIN_INFO[cell.type];
  hud.innerHTML = `Hex <b>${c}, ${row}</b> · ${info?.name ?? cell.type} · Nível <b>${cell.level}</b>${info && !info.passable ? ' · bloqueado' : ''}`;
});

// ---- terrain editing
let currentEntry: MapEntry | null = null;
let rebuildQueued = false, lastRebuild = 0;
/** Rebuild the board from the edited draft; throttled while dragging so painting stays smooth. */
function rebuildBoard(force = false): void {
  if (!board) return;
  const now = performance.now();
  if (!force && now - lastRebuild < 110) {
    if (!rebuildQueued) { rebuildQueued = true; setTimeout(() => { rebuildQueued = false; rebuildBoard(true); }, 110 - (now - lastRebuild)); }
    return;
  }
  lastRebuild = now;
  board = new Board(board.draft);
  cast.setBoard(board);
  buildGround();
}
async function rebuildDecorations(): Promise<void> {
  if (!board) return;
  await decor.populate(board, groundAt);
  cast.blocked = decor.blocked;
}
function updateEditMode(): void {
  rig.strokeEnabled = tools.active || decorTools.active;
  hover.scale.setScalar(1 + (tools.size - 1) * 1.75);
}
const tools = new TerrainTools($('terrain-tools'),
  updateEditMode,
  () => void save(),
  () => { if (board && tools.undo(board)) { rebuildBoard(true); void rebuildDecorations(); } });
const decorTools = new DecorationTools($('decor-tools'), updateEditMode);
const backgroundTools = new BackgroundTools($<HTMLDetailsElement>('background-tools'), (choice) => {
  if (!board) return;
  if (choice === 'auto') delete board.draft.backgroundImage;
  else board.draft.backgroundImage = choice === 'none' ? '' : choice;
  tools.dirty = true;
  tools.render();
  backgroundTools.setDraft(board.draft);
  void applyMapBackground(board.draft);
});
async function save(): Promise<void> {
  if (!board) return;
  status.textContent = 'Salvando…';
  try {
    const saved = await saveMap(board.draft);
    tools.dirty = false; tools.render();
    currentEntry = saved;
    const i = entries.findIndex(e => e.id === saved.id);
    if (i >= 0) entries[i] = saved; else entries.push(saved);
    renderList();
    $('title').textContent = `${board.draft.title || saved.id} · ${board.draft.cols}×${board.draft.rows} · ${saved.file}`;
    status.textContent = `Salvo: ${saved.file}`;
  } catch (err) { status.textContent = `Erro ao salvar: ${String(err)}`; }
  setTimeout(() => { if (status.textContent?.startsWith('Salvo') || status.textContent?.startsWith('Erro')) status.textContent = ''; }, 2500);
}
function hexUnder(e: PointerEvent): [number, number] | null {
  if (!board || !ground) return null;
  const r = stage.renderer.domElement.getBoundingClientRect();
  mouse.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
  ray.setFromCamera(mouse, stage.camera);
  const hit = ray.intersectObjects(water ? [ground, water] : [ground], false)[0];
  return hit ? hexAt(board.layout, hit.point.x, hit.point.z) : null;
}
rig.onStroke = (e, phase) => {
  if (!board || (!tools.active && !decorTools.active)) return;
  if (phase === 'start') tools.begin(board);
  const h = hexUnder(e);
  const changed = h && (tools.active
    ? tools.apply(board, h[0], h[1])
    : decorTools.apply(board, h[0], h[1]));
  if (changed) {
    tools.dirty = true; tools.render();
    rebuildBoard(phase === 'end');
  }
  else if (phase === 'end') rebuildBoard(true);
  // Rebuild the sprite cards once the stroke ends, rather than reloading a full map of
  // decorations on every drag sample.
  if (decorTools.active && phase === 'end') void rebuildDecorations();
};

// ---- board clicks (a short left click; holding arms the pan)
rig.onClick = (e) => {
  if (!ground || !board) return;
  if (tools.active || decorTools.active) {
    const h = hexUnder(e);
    if (h) {
      tools.begin(board);
      const changed = tools.active ? tools.apply(board, h[0], h[1]) : decorTools.apply(board, h[0], h[1]);
      if (changed) {
        tools.dirty = true; tools.render(); rebuildBoard(true);
        if (decorTools.active) void rebuildDecorations();
      }
    }
    return;
  }
  const r = stage.renderer.domElement.getBoundingClientRect();
  mouse.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
  ray.setFromCamera(mouse, stage.camera);
  const g = ray.intersectObjects(water ? [ground, water] : [ground], false)[0];
  cast.click(ray, g ? g.point : null);
};

// ---- controls
for (const btn of document.querySelectorAll<HTMLButtonElement>('[data-edges]')) btn.addEventListener('click', () => setEdges(btn.dataset.edges as EdgeStyle));
const app = $('app'), mapListToggle = $<HTMLButtonElement>('map-list-toggle');
function setMapListVisible(visible: boolean): void {
  app.classList.toggle('map-list-hidden', !visible);
  mapListToggle.textContent = visible ? 'Ocultar mapas' : 'Mostrar mapas';
  mapListToggle.title = visible ? 'Ocultar a lista de mapas' : 'Mostrar a lista de mapas';
  mapListToggle.setAttribute('aria-expanded', String(visible));
}
mapListToggle.addEventListener('click', () => setMapListVisible(app.classList.contains('map-list-hidden')));
function wirePanelToggle(buttonId: string, panelId: string, panelLabel: string): void {
  const button = $<HTMLButtonElement>(buttonId), panel = $(panelId);
  const setVisible = (visible: boolean) => {
    panel.classList.toggle('panel-hidden', !visible);
    button.classList.toggle('on', visible);
    button.setAttribute('aria-expanded', String(visible));
    button.title = `${visible ? 'Ocultar' : 'Mostrar'} ${panelLabel}`;
  };
  button.addEventListener('click', () => setVisible(panel.classList.contains('panel-hidden')));
}
wirePanelToggle('tools-toggle', 'tools', 'ferramentas de terreno');
wirePanelToggle('cast-toggle', 'cast', 'elenco');
const toggleTilt = () => { stage.tiltShift = !stage.tiltShift; $('tilt').classList.toggle('on', stage.tiltShift); };
const toggleGrid = () => { if (grid) { grid.visible = !grid.visible; $('grid').classList.toggle('on', grid.visible); } };
$('tilt').addEventListener('click', toggleTilt);
$<HTMLSelectElement>('tod').addEventListener('change', (e) => {
  const next = (e.target as HTMLSelectElement).value as TimeOfDay;
  if (activeBackdrop) stage.scene.background = new THREE.Color('#000000');
  stage.setTimeOfDay(next);
  if (activeBackdrop) stage.scene.background = activeBackdrop;
});
$('grid').addEventListener('click', toggleGrid);
addEventListener('keydown', (e) => {
  if ((e.target as HTMLElement).tagName === 'INPUT') return;
  if (e.key === 't' || e.key === 'T') toggleTilt();
  if (e.key === 'g' || e.key === 'G') toggleGrid();
  if ((e.ctrlKey || e.metaKey) && (e.key === 'z' || e.key === 'Z')) { e.preventDefault(); if (board && tools.undo(board)) rebuildBoard(true); }
  if ((e.ctrlKey || e.metaKey) && (e.key === 's' || e.key === 'S')) { e.preventDefault(); void save(); }
});

// ---- map list
const entries = await listEmberMaps();
const list = $('maps'), search = $<HTMLInputElement>('search');
function renderList(): void {
  const q = search.value.trim().toLowerCase();
  const shown = entries.filter(e => !q || e.id.includes(q));
  list.innerHTML = '';
  for (const e of shown) {
    const li = document.createElement('li');
    li.dataset.id = e.id;
    li.innerHTML = `<span>${e.id}</span><small>#${String(e.serial).padStart(3, '0')}</small>`;
    li.addEventListener('click', () => void openMap(e));
    if (board?.id === e.id) li.classList.add('on');
    list.appendChild(li);
  }
  $('count').textContent = `${shown.length} de ${entries.length} mapas`;
}
search.addEventListener('input', renderList);
renderList();

const start = entries.find(e => e.id === new URLSearchParams(location.search).get('map')) ?? entries.find(e => e.id === 'ashen-forest-crossing') ?? entries[0];
if (start) void openMap(start);
