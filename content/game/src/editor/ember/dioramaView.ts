// The 3D map inside Ember's map editor: Engine2's terrain, decorations and units, drawn from the
// editor's current mission. It only draws and answers "what is under this pointer"; every edit is
// made by Ember's editor through MapPreview3D's callbacks, exactly as with Ember's own preview.

import * as THREE from 'three';
import { MeadowGrass } from '../../render/meadowGrass';
import { Stage, type TimeOfDay } from '../../render/stage';
import { CameraRig, START_PITCH, START_YAW } from '../../render/cameraRig';
import { groundTextures } from '../../render/textures';
import { makeTerrainMaterial } from '../../render/terrainMaterial';
import { buildSmoothGround, buildWater, groundHeightAt } from '../../render/terrainMesh';
import { Board } from '../../map/board';
import { prepareTerrainRelief } from '../../render/terrainRelief';
import { ensureGroundSets } from '../../render/groundSets';
import type { EmberMapDraft } from '../../map/emberMap';
import { corner, hexAt, SQ3 } from '../../core/hex';
import { DecorLayer } from '../../world/decor';
import { UnitActor } from '../../units/actor';
import { unitForSpawn, type Footprint, type UnitDef } from '../../units/catalog';
import { initFrames, pumpUploads } from '../../units/frames';
import { mapBackgroundUrl, resolvedMapBackground } from '../mapBackgrounds';
import { TILE_CHAR, placedFootprint } from '../../ember/data';
import type { Mission, TerrainId } from '../../ember/types';

export type SpawnSide = 'playerSpawns' | 'enemySpawns' | 'neutralSpawns';
export interface PlacedUnit { side: SpawnSide; index: number; name: string; actor: UnitActor }

const TERRAIN_BY_CHAR = Object.fromEntries(Object.entries(TILE_CHAR).map(([id, ch]) => [ch, id])) as Record<string, TerrainId>;
const EDGES = 'smooth' as const;

/** Ember's Mission keeps terrain as layout rows of characters; Engine2's board reads a tile array. */
function draftFromMission(m: Mission): EmberMapDraft {
  const tiles: TerrainId[] = [];
  for (let r = 0; r < m.rows; r++) for (let c = 0; c < m.cols; c++) tiles.push(TERRAIN_BY_CHAR[m.layout[r]?.[c] ?? ''] ?? 'plains');
  return { ...(m as unknown as EmberMapDraft), tiles };
}

/** Ember's footprint(): offsets authored for an even anchor row; odd-dy rows shift with the anchor's parity. */
function footprintCells(f: Footprint | undefined, c: number, r: number): [number, number][] {
  if (!f) return [[c, r]];
  const shift = r & 1;
  return f.map(o => [c + o.dx + (o.dy & 1 ? shift : 0), r + o.dy] as [number, number]);
}

function hexRing(rIn: number, rOut: number): THREE.BufferGeometry {
  const pos: number[] = [], idx: number[] = [];
  for (let k = 0; k < 6; k++) { const [ox, oz] = corner(0, 0, k, rOut), [ix, iz] = corner(0, 0, k, rIn); pos.push(ox, 0, oz, ix, 0, iz); }
  for (let k = 0; k < 6; k++) { const a = k * 2, b = ((k + 1) % 6) * 2; idx.push(a, b, a + 1, b, b + 1, a + 1); }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  return g;
}

/** One battle-grid mark: a hex fill (no rIn) or hex border, radii in hex radii, rgba colour. */
export interface GridMark { x: number; y: number; color: string; rOut: number; rIn?: number; flat?: boolean }

export class DioramaView {
  readonly stage: Stage;
  readonly rig: CameraRig;
  board: Board | null = null;
  units: PlacedUnit[] = [];
  private readonly groundMat: THREE.MeshStandardMaterial;
  private readonly waterMat: THREE.MeshStandardMaterial;
  private readonly meadow = new MeadowGrass();
  private ground: THREE.Mesh | null = null;
  private water: THREE.Mesh | null = null;
  readonly decor: DecorLayer;
  private readonly unitGroup = new THREE.Group();
  private readonly hover: THREE.Mesh;
  private readonly marks = new THREE.Group();
  private readonly ringGeo = hexRing(0.8, 0.97);
  private readonly markMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0.9, 1.9, 2.8), side: THREE.DoubleSide, transparent: true, depthWrite: false });
  private readonly ray = new THREE.Raycaster();
  private readonly ndc = new THREE.Vector2();
  private mapKey = '';
  private baseDist = 50;
  private backdrop: THREE.Texture | null = null;
  private backdropId: string | null = null;
  private backdropRequest = 0;
  private gen = 0;

  constructor(host: HTMLElement) {
    this.stage = new Stage(host);
    // dev only: headless checks (tools/) drive the stage directly, e.g. moon phases
    if (import.meta.env.DEV) (window as unknown as { __stage?: Stage }).__stage = this.stage;
    this.rig = new CameraRig(this.stage.camera, this.stage.renderer.domElement);
    this.rig.mode = 'free'; // the editor spins freely to test angles; the campaign stays locked
    const tex = groundTextures(this.stage.renderer);
    this.groundMat = makeTerrainMaterial(tex);
    this.waterMat = new THREE.MeshStandardMaterial({
      // 0.12 focused a low moon's reflection into one blinding bloom blob; this is a soft shimmer
      color: '#2f86a8', roughness: 0.5, metalness: 0.05, transparent: true, opacity: 0.74,
      normalMap: tex.ripple, normalScale: new THREE.Vector2(0.6, 0.6), depthWrite: false,
    });
    this.stage.onFrame((_dt, t) => tex.ripple.offset.set(t * 0.02, t * 0.013));
    this.decor = new DecorLayer(this.stage.renderer, this.stage.scene);
    this.hover = new THREE.Mesh(this.ringGeo, new THREE.MeshBasicMaterial({ color: new THREE.Color(2.4, 2.0, 1.0), side: THREE.DoubleSide, transparent: true, depthWrite: false }));
    this.hover.visible = false;
    this.hover.renderOrder = 5;
    this.marks.renderOrder = 6;
    this.stage.scene.add(this.hover, this.marks, this.unitGroup, this.meadow.group);
    initFrames(this.stage.renderer);
    this.stage.onFrame((dt, time) => {
      pumpUploads();
      const yaw = this.rig.facingYaw;
      this.decor.face(yaw, this.flatTop);
      this.decor.updateLights(time);
      for (const u of this.units) u.actor.update(dt, yaw, this.unitGroundAt, this.flatTop);
    });
  }

  private flatFog: THREE.Fog | null = null;
  private boardTop = 0;

  /** Height the 2D view's flat cards lie at (above all terrain), or null in the normal 3D view. */
  get flatTop(): number | null { return this.rig.flat ? this.boardTop : null; }

  /** Old-school 2D view (Ember's original battle camera) on or off. Off returns to the exact angle it left. */
  setFlat(on: boolean): void {
    if (on === this.rig.flat) return;
    this.rig.flat = on;
    this.meadow.group.visible = !on;
    // distance fog would grey out the whole board from the long lens's far position
    if (on) { this.flatFog = this.stage.scene.fog as THREE.Fog | null; this.stage.scene.fog = null; }
    else if (this.flatFog) { this.stage.scene.fog = this.flatFog; this.flatFog = null; }
    this.rig.apply();
  }

  /** The terrain's surface meshes (ground, and water when the map has any). */
  get surfaces(): THREE.Mesh[] { return [this.ground, this.water].filter((m): m is THREE.Mesh => !!m); }

  readonly groundAt = (x: number, z: number): number => (this.board ? groundHeightAt(this.board, EDGES, x, z) : 0);
  /** Where a unit's feet are: the ground, or the top of a fallen trunk it stands on (decor.standAt). */
  readonly unitGroundAt = (x: number, z: number): number => this.groundAt(x, z) + this.decor.standAt(x, z);

  /** Draw the editor's current mission. The camera is kept while the same map is being edited. */
  async setMission(m: Mission, onDecorProgress?: (loaded: number, total: number) => void): Promise<void> {
    const gen = ++this.gen;
    const board = new Board(draftFromMission(m));
    this.board = board;
    this.boardTop = board.cells.reduce((top, c) => Math.max(top, c.groundY), 0) + 1;
    const key = `${m.id}|${m.cols}x${m.rows}`;
    const b = board.bounds(), w = b.x1 - b.x0 + 2.4, depth = b.z1 - b.z0 + 2.4;
    const tod = (['day', 'noon', 'dawn', 'dusk', 'brightNight', 'darkNight'] as TimeOfDay[]).includes(m.timeOfDay as TimeOfDay) ? m.timeOfDay as TimeOfDay : 'day';
    const indoor = m.environment === 'indoor';
    if (key !== this.mapKey || tod !== this.stage.timeOfDay || indoor !== this.stage.indoor) {
      this.stage.indoor = indoor;
      this.stage.timeOfDay = tod;
      this.stage.scene.background = new THREE.Color('#000000');
      this.stage.fitToBoard(w, depth);
      if (m.environment === 'indoor') { this.stage.sun.intensity *= 0.8; this.stage.hemi.intensity *= 1.6; }
    }
    await Promise.all([prepareTerrainRelief(board), ensureGroundSets()]);
    if (gen !== this.gen) return;
    this.buildGround();
    if (key !== this.mapKey) {
      const avgY = board.cells.reduce((s, c) => s + c.groundY, 0) / board.cells.length;
      this.rig.frame(b, avgY);
      this.baseDist = this.rig.dist;
      this.mapKey = key;
    }
    void this.applyBackdrop(board.draft);
    this.placeUnits(m);
    await this.decor.populate(board, this.groundAt, onDecorProgress);
    if (gen !== this.gen) return;
  }

  private buildGround(): void {
    const board = this.board!;
    for (const m of [this.ground, this.water]) if (m) { this.stage.scene.remove(m); m.geometry.dispose(); }
    this.ground = new THREE.Mesh(buildSmoothGround(board), this.groundMat);
    this.ground.castShadow = this.ground.receiveShadow = true;
    this.stage.scene.add(this.ground);
    this.meadow.rebuild(board, this.groundAt);
    const wg = buildWater(board, EDGES);
    this.water = wg ? new THREE.Mesh(wg, this.waterMat) : null;
    if (this.water) { this.water.receiveShadow = true; this.water.renderOrder = 2; this.stage.scene.add(this.water); }
  }

  private async applyBackdrop(draft: EmberMapDraft): Promise<void> {
    const request = ++this.backdropRequest;
    const item = resolvedMapBackground(draft);
    const scene = this.stage.scene;
    if (!item) { this.backdrop?.dispose(); this.backdrop = null; this.backdropId = null; scene.background = new THREE.Color('#000000'); return; }
    if (this.backdrop && this.backdropId === item.id) { scene.background = this.backdrop; return; }
    scene.background = new THREE.Color('#000000');
    try {
      const texture = await new THREE.TextureLoader().loadAsync(mapBackgroundUrl(item));
      if (request !== this.backdropRequest) { texture.dispose(); return; }
      texture.colorSpace = THREE.SRGBColorSpace;
      this.backdrop?.dispose();
      this.backdrop = texture; this.backdropId = item.id;
      scene.background = texture;
    } catch (error) { console.warn(`[engine2] could not load map background ${item.id}`, error); }
  }

  /** Ember's own spawns, each standing as its unit (heroes by name, everyone else by class). */
  private placeUnits(m: Mission): void {
    const board = this.board!;
    const old = new Map(this.units.map(u => [`${u.side}|${u.index}|${u.actor.def.key}`, u]));
    const next: PlacedUnit[] = [];
    const sides: [SpawnSide, 'player' | 'enemy' | 'neutral'][] = [['playerSpawns', 'player'], ['enemySpawns', 'enemy'], ['neutralSpawns', 'neutral']];
    for (const [side, kind] of sides) {
      (m[side] ?? []).forEach((s, index) => {
        if (s.x < 0 || s.y < 0 || s.x >= m.cols || s.y >= m.rows) return;
        const def = unitForSpawn(s, kind);
        if (!def) return;
        const k = `${side}|${index}|${def.key}`;
        let placed = old.get(k);
        if (placed) old.delete(k);
        else {
          const actor = new UnitActor(def);
          actor.facing = kind === 'enemy' ? -1 : 1;
          this.unitGroup.add(actor.mesh);
          void actor.ready().catch(e => console.warn('[engine2] sprite', def.key, e));
          placed = { side, index, name: s.name, actor };
        }
        placed.name = s.name;
        const a = this.anchor(def, s.x, s.y);
        placed.actor.x = a.x; placed.actor.z = a.z;
        next.push(placed);
      });
    }
    for (const gone of old.values()) { this.unitGroup.remove(gone.actor.mesh); gone.actor.dispose(); }
    this.units = next;
  }

  /** Front-row centroid of a footprint (Ember's footprintCentroid): where the feet stand. */
  private anchor(def: UnitDef, c: number, r: number): { x: number; z: number } {
    const b = this.board!;
    const front = def.footprint ? footprintCells(def.footprint, c, r).filter((_, i) => def.footprint![i].dy === 0) : [[c, r] as [number, number]];
    let x = 0, z = 0, n = 0;
    for (const [fc, fr] of front) { if (fc < 0 || fc >= b.layout.cols) continue; const cell = b.cell(fc, fr); x += cell.x; z += cell.z; n++; }
    return n ? { x: x / n, z: z / n } : { x: b.cell(c, r).x, z: b.cell(c, r).z };
  }

  private aim(clientX: number, clientY: number): void {
    const r = this.stage.renderer.domElement.getBoundingClientRect();
    this.ndc.set(((clientX - r.left) / r.width) * 2 - 1, -((clientY - r.top) / r.height) * 2 + 1);
    this.ray.setFromCamera(this.ndc, this.stage.camera);
  }

  /** The hex under a screen point, plus that point in Ember's flat map units (its click `point`). */
  cellAt(clientX: number, clientY: number): { x: number; y: number; point: { x: number; y: number } } | null {
    if (!this.board || !this.ground) return null;
    this.aim(clientX, clientY);
    const hit = this.ray.intersectObjects(this.water ? [this.ground, this.water] : [this.ground], false)[0];
    if (!hit) return null;
    const L = this.board.layout;
    const [x, y] = hexAt(L, hit.point.x, hit.point.z);
    // Ember's flat preview measures in hex radii from its board corner: centre of (x, y) is
    // (√3·(x + ½(y&1) + ½), 1.5·y + 3.4) — the same numbers the water brush offsets from.
    return { x, y, point: { x: hit.point.x + L.ox + SQ3 / 2, y: hit.point.z + L.oz + 3.4 } };
  }

  /** The unit whose drawn sprite is under a screen point (tall sprites reach well past their hex). */
  unitAt(clientX: number, clientY: number): PlacedUnit | null {
    this.aim(clientX, clientY);
    let best: PlacedUnit | null = null, bestD = Infinity;
    for (const u of this.units) {
      const hit = this.ray.intersectObject(u.actor.mesh, true)[0];
      if (hit && hit.distance < bestD) { best = u; bestD = hit.distance; }
    }
    return best;
  }

  /** Light up the hex under the pointer, or hide the ring. */
  setHover(cell: { x: number; y: number } | null): void {
    if (!cell || !this.board) { this.hover.visible = false; return; }
    const c = this.board.cell(cell.x, cell.y);
    this.hover.position.set(c.x, (c.water ? c.waterY : this.groundAt(c.x, c.z)) + 0.04, c.z);
    this.hover.visible = true;
  }

  /** Ring every hex of the selected placed decoration (Ember highlights it the same way). */
  setSelection(sel: { id: string; x: number; y: number; rot?: number } | null | undefined): void {
    this.marks.clear();
    if (!sel || !this.board) return;
    const L = this.board.layout;
    for (const f of placedFootprint(sel)) {
      const cx = sel.x + f.dx, cy = sel.y + f.dy;
      if (cx < 0 || cy < 0 || cx >= L.cols || cy >= L.rows) continue;
      const c = this.board.cell(cx, cy);
      const ring = new THREE.Mesh(this.ringGeo, this.markMat);
      ring.position.set(c.x, (c.water ? c.waterY : this.groundAt(c.x, c.z)) + 0.05, c.z);
      this.marks.add(ring);
    }
  }

  private readonly ringSets = new Map<string, { group: THREE.Group; mat: THREE.MeshBasicMaterial; key: string }>();

  /** A named set of coloured hex rings (battle: movement range, targets, selection). */
  setRings(name: string, cells: { x: number; y: number }[], color: THREE.ColorRepresentation): void {
    let set = this.ringSets.get(name);
    if (!set) {
      const mat = new THREE.MeshBasicMaterial({ side: THREE.DoubleSide, transparent: true, depthWrite: false });
      const group = new THREE.Group(); group.renderOrder = 6;
      this.stage.scene.add(group);
      set = { group, mat, key: '' };
      this.ringSets.set(name, set);
    }
    set.mat.color.set(color);
    const key = cells.map(c => `${c.x},${c.y}`).join(';');
    if (key === set.key || !this.board) return;
    set.key = key;
    set.group.clear();
    const L = this.board.layout;
    for (const { x, y } of cells) {
      if (x < 0 || y < 0 || x >= L.cols || y >= L.rows) continue;
      const c = this.board.cell(x, y);
      const ring = new THREE.Mesh(this.ringGeo, set.mat);
      ring.position.set(c.x, (c.water ? c.waterY : this.groundAt(c.x, c.z)) + 0.05, c.z);
      set.group.add(ring);
    }
  }

  private readonly gridGroup = new THREE.Group();
  private gridKey = '';
  private readonly gridMats = new Map<string, THREE.MeshBasicMaterial>();

  /** Ember's battle grid marks (movement wash, edges, targets, route, turn marker, cursor): hex
   * fills (rIn absent) and hex borders in exact rgba colours, level on each tile. Later marks
   * sit on top of earlier ones. Rebuilt only when the marks change. */
  setGrid(marks: GridMark[]): void {
    const key = marks.map(m => `${m.x},${m.y},${m.color},${m.rOut},${m.rIn ?? 0},${m.flat ? 1 : 0}`).join(';');
    if (key === this.gridKey || !this.board) return;
    this.gridKey = key;
    if (!this.gridGroup.parent) this.stage.scene.add(this.gridGroup);
    for (const child of this.gridGroup.children) (child as THREE.Mesh).geometry.dispose();
    this.gridGroup.clear();
    const L = this.board.layout;
    // One mesh per run of same-coloured marks (a whole layer is one draw), stacked in order.
    const batches = new Map<string, { pos: number[]; idx: number[]; order: number }>();
    let run = -1, lastColor = '';
    for (const m of marks) {
      if (m.x < 0 || m.y < 0 || m.x >= L.cols || m.y >= L.rows) continue;
      if (m.color !== lastColor) { run++; lastColor = m.color; }
      const c = this.board.cell(m.x, m.y);
      const lift = 0.05 + run * 0.002;
      // Corner samples can belong to a neighbouring raised/lowered tile. Connecting those
      // heights makes a grid border into a large vertical triangle across a cliff.
      const surfaceY = c.water ? c.waterY : this.groundAt(c.x, c.z);
      const at = (dx: number, dz: number, b: { pos: number[] }) =>
        b.pos.push(c.x + dx, surfaceY + lift, c.z + dz);
      const batchKey = `${m.color}|${run}`;
      let b = batches.get(batchKey);
      if (!b) { b = { pos: [], idx: [], order: run }; batches.set(batchKey, b); }
      const base = b.pos.length / 3;
      if (!m.rIn) {
        at(0, 0, b);
        for (let k = 0; k < 6; k++) { const [x, z] = corner(0, 0, k, m.rOut); at(x, z, b); }
        for (let k = 0; k < 6; k++) b.idx.push(base, base + 1 + k, base + 1 + ((k + 1) % 6));
      } else {
        for (let k = 0; k < 6; k++) {
          const [ox, oz] = corner(0, 0, k, m.rOut), [ix, iz] = corner(0, 0, k, m.rIn);
          at(ox, oz, b); at(ix, iz, b);
        }
        for (let k = 0; k < 6; k++) { const a = base + k * 2, n = base + ((k + 1) % 6) * 2; b.idx.push(a, n, a + 1, n, n + 1, a + 1); }
      }
    }
    for (const [batchKey, b] of batches) {
      const color = batchKey.slice(0, batchKey.lastIndexOf('|'));
      let mat = this.gridMats.get(color);
      if (!mat) {
        const p = /rgba?\(([^,]+),([^,]+),([^,)]+)(?:,([^)]+))?\)/.exec(color);
        mat = new THREE.MeshBasicMaterial({
          color: p ? new THREE.Color(Number(p[1]) / 255, Number(p[2]) / 255, Number(p[3]) / 255) : new THREE.Color(1, 1, 1),
          opacity: p?.[4] !== undefined ? Number(p[4]) : 1,
          transparent: true, depthWrite: false, side: THREE.DoubleSide, toneMapped: false,
        });
        this.gridMats.set(color, mat);
      }
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.Float32BufferAttribute(b.pos, 3));
      g.setIndex(b.idx);
      const mesh = new THREE.Mesh(g, mat);
      mesh.renderOrder = 6 + b.order * 0.001;
      this.gridGroup.add(mesh);
    }
  }

  /** Zoom as a share of the framed distance (Ember's preview percentage). */
  setZoom(zoom: number): void {
    this.rig.dist = THREE.MathUtils.clamp(this.baseDist / zoom, this.rig.minDist, this.rig.maxDist);
    this.rig.apply();
  }

  turn(degrees: number): void { this.rig.yaw += THREE.MathUtils.degToRad(degrees); this.rig.apply(); }
  tilt(degrees: number): void { this.rig.pitch += THREE.MathUtils.degToRad(degrees); this.rig.apply(); }
  /** Back to the normal view (turned 30°, tilted 20°). */
  resetAngle(): void { this.rig.yaw = START_YAW; this.rig.pitch = START_PITCH; this.rig.apply(); }

  dispose(): void {
    this.gen++;
    this.decor.clear();
    for (const u of this.units) u.actor.dispose();
    this.units = [];
    for (const m of [this.ground, this.water]) m?.geometry.dispose();
    this.meadow.dispose();
    this.groundMat.dispose(); this.waterMat.dispose(); this.backdrop?.dispose();
    for (const child of this.gridGroup.children) (child as THREE.Mesh).geometry.dispose();
    for (const mat of this.gridMats.values()) mat.dispose();
    this.rig.dispose();
    this.stage.dispose();
  }
}
