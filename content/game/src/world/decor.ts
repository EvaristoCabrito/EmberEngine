// Ember's decorations (photoreal 2D art) standing in the 3D world as upright cards, like the units:
// lit by the world, real shadows, full native resolution, facing the camera.
// Sizing is Ember's own decorSize (ThreeBattleRenderer.ts), anchoring is Ember's: footprint centroid,
// with the art's opaque base set on the ground. Side files and mirroring follow decorationFacing.
// Ember's 3D architecture (model3d walls/doors) is real 3D, built by Ember's own wall builder.

import * as THREE from 'three';
import { createWallGeometry } from '../game/gfx/three/ThreeWalls';
import { MODELS, loadModel } from './models';
import { fitHouseToHumans } from './houseScale';
import {
  DECORATIONS, CHEST_DECOR_IDS, HOUSE_DECOR_IDS, BIG_HOUSE_DECOR_IDS, DECOR_ART_SCALE, HOUSE_ART_SCALE,
  decorationImage, decorationPlacementArt, decorationFacing, placedFootprint, placedBlockingFootprint, barricadeDecor, BARRICADE_LIKE_DECOR,
} from '../ember/data';
import type { DecorationDef, DecorationPlacement, TerrainId } from '../ember/types';
import type { Board } from '../map/board';
import { makeSpriteMaterial, FLAME_EMIT, type SpriteEmit } from '../units/spriteMaterial';
import decorFiles from './decor-files.json';
import { LIGHT_DEFS, LIGHT_RADIUS_MUL, MAP_LIGHT_MIN_HEIGHT, flickerAt } from '../ember/lighting';
import { CarriedLight } from '../render/carriedLight';
import { FLAT_ROW_STEP } from '../render/cameraRig';
import { CARD_RENDER_ORDER } from '../render/drawOrder';
import { maxAnisotropy } from '../render/gpu';

const SQRT3 = Math.sqrt(3);
const FILES = new Set(decorFiles as string[]);
const ARCHITECTURE = new Set(['wall', 'doorway', 'door', 'secretDoor']);
/** Experiment (2026-10-13): every prop card stays put, standing square to the map's hex rows (as
 * the straight-on view sees it, never on a diagonal), instead of turning with the camera. All stay parallel, so whatever is in front stays in front; seen from
 * behind they show their backs, the art mirrored (cards are double-sided). false = cards face the
 * camera again. Barricades keep their own authored turn. */
const LOCK_PROPS = true;

/** Ember's decorSize, ported exactly (tile = hex radius = 1 world unit here). */
function decorSize(id: string, def: DecorationDef, tile = 1): { w: number; h: number } {
  if (id === 'stone-stairs-up-001' || id === 'stone-stairs-down-001') return { w: tile * SQRT3, h: tile * 3.5 };
  if (def.propModel && !def.propModel.startsWith('tavern-')) {
    const pm = def.propModel === 'grey-outcrop' ? 'rocky-outcrop' : def.propModel;
    if (DECORATIONS[pm]) return decorSize(pm, DECORATIONS[pm]!, tile);
  }
  if (def.treeModel) return { w: tile * 3.2, h: tile * 4.8 };
  let minDx = 0, maxDx = 0, minDy = 0, maxDy = 0;
  for (const { dx, dy } of def.footprint) { minDx = Math.min(minDx, dx); maxDx = Math.max(maxDx, dx); minDy = Math.min(minDy, dy); maxDy = Math.max(maxDy, dy); }
  const one = def.footprint.length === 1, item = CHEST_DECOR_IDS.has(id), tree = id === 'dead-tree', log = id === 'fallen-log', wall = id === 'barricade';
  const waypoint = !!def.exitKind, anyHouse = HOUSE_DECOR_IDS.has(id) || BIG_HOUSE_DECOR_IDS.has(id);
  const w = tree ? tile * 1.28 : log ? tile * SQRT3 * 2.05 : wall ? tile * 1.42 : anyHouse ? tile * 1.45 * 3 : waypoint ? tile * SQRT3 * (one ? 1 : 2)
    : item ? tile * 0.92 : one ? tile * 1.55 : tile * SQRT3 * (maxDx - minDx + 1.7);
  const baseH = tree ? tile * 2.55 : log ? tile * 0.82 : wall ? tile * 1.18 : anyHouse ? tile * 1.58 * 3 : waypoint ? tile * 2
    : item ? tile * 0.72 : one ? tile * 1.65 : tile * (1.5 * (maxDy - minDy) + 2.3);
  const h = def.artAspect ? w / def.artAspect : baseH * (def.heightScale ?? 1);
  const s = waypoint ? 1 : (anyHouse ? HOUSE_ART_SCALE : DECOR_ART_SCALE) * (def.artScale ?? 1);
  return fitHouseToHumans(id, { w: w * s, h: h * s }, tile);
}

type WallDef = Parameters<typeof createWallGeometry>[0];
/** Walls and doors next to Engine2's human units: Ember's height read about one human tall. */
const WALL_HUMAN_SCALE = 1.6;

/** Ember's wall textures, loaded once and shared (repeating masonry, mipmapped). */
const WALL_TEXTURES = new Map<string, THREE.Texture>();
function wallTexture(url: string, aniso: number): THREE.Texture {
  let t = WALL_TEXTURES.get(url);
  if (!t) {
    t = new THREE.TextureLoader().load(url);
    t.colorSpace = THREE.SRGBColorSpace;
    t.wrapS = t.wrapT = THREE.RepeatWrapping;
    t.anisotropy = aniso;
    WALL_TEXTURES.set(url, t);
  }
  return t;
}

/** URL → file name on disk (decorationImage adds cache-busting queries). */
const fileOf = (url: string) => url.split('?')[0].split('/').pop()!;
const hasArt = (file: string) => FILES.has(fileOf(decorationImage(file)));

interface Art {
  texture: THREE.Texture; base: { u: number; v: number }; flame: { u: number; v: number }; flameFound: boolean;
  /** Per column of a 256-wide copy: the art's highest opaque v (v up), -1 where empty. */
  tops: Float32Array;
}

/** Fallen trunks a unit stands on top of (its feet on the trunk, not in it). */
const STAND_ON_DECOR = new Set([
  'fallen-log', 'wilds-fallen-log', 'forest-photographic-011-log', 'forest-mossy-trunk', 'wilds-snowy-log',
  'wilds-mossy-log', 'wilds-mossy-charred-log', 'wilds-hollow-log', 'wilds-winter-mossy-log',
]);
/** The trunk's painted top is its far upper edge; feet land a little below it, on the top. */
const STAND_ON_TOP = 0.85;
const artCache = new Map<string, Promise<Art>>();

/** Load an image at full resolution and measure its opaque base (Ember's artBase idea). */
function loadArt(url: string, anisotropy: number): Promise<Art> {
  let p = artCache.get(url);
  if (!p) {
    p = (async () => {
      const blob = await (await fetch(url)).blob();
      const bm = await createImageBitmap(blob, { imageOrientation: 'flipY', premultiplyAlpha: 'none', colorSpaceConversion: 'none' });
      // measure on a small copy: lowest row with solid pixels, and the centre of the solid pixels near it
      const sw = Math.min(256, bm.width), sh = Math.max(1, Math.round((bm.height * sw) / bm.width));
      const cv = new OffscreenCanvas(sw, sh), g = cv.getContext('2d')!;
      g.drawImage(bm, 0, 0, sw, sh);
      const d = g.getImageData(0, 0, sw, sh).data;
      // Ember's brightness-weighted flame centroid; bitmap v runs upwards here.
      let weight = 0, fu = 0, fv = 0;
      for (let y = 0; y < sh; y++) for (let x = 0; x < sw; x++) {
        const i = (y * sw + x) * 4, r = d[i], green = d[i + 1], blue = d[i + 2];
        if (d[i + 3] < 128 || r < 190 || green < 90 || r < green || green < blue + 20) continue;
        const w = (r + green - blue) / 255;
        weight += w; fu += w * (x + 0.5); fv += w * (y + 0.5);
      }
      const flame = weight > 4 ? { u: fu / weight / sw, v: fv / weight / sh } : { u: 0.5, v: 0.7 };
      // the bitmap is flipped (row 0 = bottom of the art)
      let bottom = 0;
      outer: for (let y = 0; y < sh; y++) for (let x = 0; x < sw; x++) if (d[(y * sw + x) * 4 + 3] > 128) { bottom = y; break outer; }
      let sx = 0, n = 0;
      const band = Math.max(2, Math.round(sh * 0.08));
      for (let y = bottom; y < Math.min(sh, bottom + band); y++) for (let x = 0; x < sw; x++) if (d[(y * sw + x) * 4 + 3] > 128) { sx += x; n++; }
      const t = new THREE.Texture(bm);
      t.flipY = false; t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = anisotropy;
      t.minFilter = THREE.LinearMipmapLinearFilter; t.magFilter = THREE.LinearFilter; t.needsUpdate = true;
      const tops = new Float32Array(sw).fill(-1);
      for (let x = 0; x < sw; x++) for (let y = sh - 1; y >= 0; y--) if (d[(y * sw + x) * 4 + 3] > 128) { tops[x] = (y + 0.5) / sh; break; }
      return { texture: t, base: { u: n ? (sx / n + 0.5) / sw : 0.5, v: bottom / sh }, flame, flameFound: weight > 4, tops };
    })();
    artCache.set(url, p);
  }
  return p;
}

const cardGeo = new THREE.PlaneGeometry(1, 1).translate(0, 0.5, 0);
/** A waypoint lying on the floor sits this far above the ground, clear of it but never floating. */
const FLOOR_LIFT = 0.03;

export interface DecorStats { placed: number; skippedArchitecture: number; missingArt: string[] }

export class DecorLayer {
  readonly group = new THREE.Group();
  /** Hexes decorations block ("c,r"), from Ember's blocking footprints. */
  blocked = new Set<string>();
  stats: DecorStats = { placed: 0, skippedArchitecture: 0, missingArt: [] };
  /** Hex centres on a fallen trunk, and how high above that hex's ground its top is. */
  private pads: { x: number; z: number; h: number }[] = [];
  private cards: THREE.Group[] = [];
  private gen = 0;
  /** Draw Ember's GLB props as real 3D models, in the editor and the game (never their flat pictures). */
  realModels = true;

  constructor(private readonly renderer: THREE.WebGLRenderer, scene: THREE.Scene) { scene.add(this.group); }

  clear(): void {
    this.gen++;
    for (const c of this.cards) { this.group.remove(c); if (c.userData.shared) continue; c.traverse(o => { const m = o as THREE.Mesh; if (m.isMesh) { (m.material as THREE.Material).dispose(); if (m.userData.ownGeometry) m.geometry.dispose(); } if (o instanceof CarriedLight) o.dispose(); }); }
    this.cards = [];
    this.pads = [];
    this.blocked.clear();
    this.stats = { placed: 0, skippedArchitecture: 0, missingArt: [] };
  }

  /** Ember's 3D architecture (walls, doorways, doors, secret doors), built by Ember's own
   * createWallGeometry in its physical (Tática) form: real height, no camera shear. Ember works
   * Z-up with screen-up as +Y; here Y is up and rows run along +Z, so the piece is turned
   * upright (rotateX −90°) and Ember's +Y becomes our −Z. */
  private addArchitecture(b: Board, p: DecorationPlacement, def: DecorationDef, all: DecorationPlacement[], groundAt: (x: number, z: number) => number): void {
    const L = b.layout, inBoard = (c: number, r: number) => c >= 0 && r >= 0 && c < L.cols && r < L.rows;
    // Ember lines architecture up on a square grid (architectureWorld): every row at the same x,
    // no odd-row half-hex shift, so walls run straight and their joins meet
    const archAt = (c: number, r: number) => ({ x: SQRT3 * c - L.ox, z: b.cell(c, r).z });
    const cells = placedFootprint(p).filter(f => inBoard(p.x + f.dx, p.y + f.dy)).map(f => archAt(p.x + f.dx, p.y + f.dy));
    if (!cells.length) return;
    const x = cells.reduce((s, c) => s + c.x, 0) / cells.length, z = cells.reduce((s, c) => s + c.z, 0) / cells.length;
    // join every neighbouring architecture cell (Ember: left/right/up/down in its grid)
    const arch = new Set(all.filter(o => { const d = DECORATIONS[o.id]; return d?.model3d && !d.rockStyle && !d.treeModel && !d.propModel; })
      .flatMap(o => o.id === 'watchtower-stone-open-door-2hex' ? placedFootprint(o).map(f => `${o.x + f.dx},${o.y + f.dy}`) : [`${o.x},${o.y}`]));
    const connections = [[p.x - 1, p.y], [p.x + 1, p.y], [p.x, p.y - 1], [p.x, p.y + 1]]
      .filter(([c, r]) => inBoard(c, r) && arch.has(`${c},${r}`))
      .map(([c, r]) => { const n = archAt(c, r); return { x: n.x - x, y: -(n.z - z) }; });
    // Ember's physical height (×3.5), then sized to the human units: a door ~1.6 humans tall
    const spatial = { ...def, heightScale: (def.heightScale ?? 1) * 3.5 * WALL_HUMAN_SCALE } as unknown as WallDef;
    const geometry = createWallGeometry(spatial, 1, p.rot ?? 0, connections, { x, y: -z }, false, 3.5).rotateX(-Math.PI / 2);
    const metal = def.doorStyle === 'iron' || def.doorStyle === 'steel';
    const color = metal ? (def.doorStyle === 'iron' ? 0x555b60 : 0x9ca7b0) : def.model3d === 'door' ? 0x925b32 : def.model3d === 'doorway' ? 0xb7a27c : 0x8b8b86;
    const map = def.wallTexture ? wallTexture(def.wallTexture, maxAnisotropy(this.renderer)) : undefined;
    const material = new THREE.MeshStandardMaterial({ map, color: map ? 0xffffff : color, roughness: metal ? 0.52 : 0.94, metalness: metal ? 0.5 : 0, flatShading: true });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.position.set(x, groundAt(x, z), z);
    mesh.castShadow = mesh.receiveShadow = true;
    mesh.userData.ownGeometry = true;
    const holder = new THREE.Group();
    holder.userData = { wall: true, decor: p.id, at: { x: p.x, y: p.y }, hexes: placedFootprint(p).map(f => ({ x: p.x + f.dx, y: p.y + f.dy })) };
    holder.add(mesh);
    this.group.add(holder);
    this.cards.push(holder);
    this.stats.placed++;
  }

  /** One of Ember's GLB models standing on its footprint, turned like Ember (quarter turns). */
  private async addModel(b: Board, p: DecorationPlacement, model: string, groundAt: (x: number, z: number) => number, gen: number): Promise<void> {
    const L = b.layout;
    const cells = placedFootprint(p).filter(f => p.x + f.dx >= 0 && p.y + f.dy >= 0 && p.x + f.dx < L.cols && p.y + f.dy < L.rows).map(f => b.cell(p.x + f.dx, p.y + f.dy));
    if (!cells.length) return;
    const x = cells.reduce((s, c) => s + c.x, 0) / cells.length, z = cells.reduce((s, c) => s + c.z, 0) / cells.length;
    const object = await loadModel(model).catch(() => null);
    if (!object || gen !== this.gen) return;
    object.position.set(x, groundAt(x, z), z);
    object.rotation.y = -(p.rot ?? 0) * Math.PI / 2;
    // shared geometry and materials: clear() must not dispose them (models.ts caches the source)
    const holder = new THREE.Group();
    holder.userData = { wall: true, shared: true, decor: p.id, at: { x: p.x, y: p.y }, hexes: placedFootprint(p).map(f => ({ x: p.x + f.dx, y: p.y + f.dy })) };
    holder.add(object);
    this.group.add(holder);
    this.cards.push(holder);
    this.stats.placed++;
  }

  async populate(b: Board, groundAt: (x: number, z: number) => number, onProgress?: (loaded: number, total: number) => void): Promise<void> {
    this.clear();
    const gen = this.gen, d = b.draft, L = b.layout;
    const placements: DecorationPlacement[] = [
      ...((d.decorations ?? []) as unknown as DecorationPlacement[]),
      ...barricadeDecor(d.tiles as TerrainId[], d.cols, d.rows, (d.decorations ?? []) as { id: string; x: number; y: number }[]),
    ];
    const inBoard = (c: number, r: number) => c >= 0 && r >= 0 && c < L.cols && r < L.rows;
    const aniso = maxAnisotropy(this.renderer);
    const jobs: Promise<void>[] = [];
    for (const p of placements) {
      const def = DECORATIONS[p.id];
      if (!def) { this.stats.missingArt.push(p.id); continue; }
      for (const { dx, dy } of placedBlockingFootprint(p)) if (inBoard(p.x + dx, p.y + dy)) this.blocked.add(`${p.x + dx},${p.y + dy}`);
      if (def.model3d && ARCHITECTURE.has(def.model3d) && !def.propModel) { this.addArchitecture(b, p, def, placements, groundAt); continue; }
      // editor only (realModels): Ember's real 3D models (trees, grey outcrop, tavern props), never a card
      const model = def.treeModel ?? def.propModel;
      if (this.realModels && model && MODELS[model]) { jobs.push(this.addModel(b, p, model, groundAt, gen)); continue; }
      const rot = ((p.rot ?? 0) + (def.mirrorAlternate && p.x >= d.cols / 2 ? 3 : 0)) % 6;
      const artId = decorationPlacementArt(p);
      const facing = decorationFacing(artId, rot, hasArt);
      const file = facing.own ? facing.file : artId;
      if (!hasArt(file)) { this.stats.missingArt.push(p.id); continue; }
      // footprint centroid = where the base stands
      let sx = 0, sz = 0, n = 0;
      for (const { dx, dy } of placedFootprint(p)) { if (!inBoard(p.x + dx, p.y + dy)) continue; const c = b.cell(p.x + dx, p.y + dy); sx += c.x; sz += c.z; n++; }
      if (!n) continue;
      const x = sx / n, z = sz / n, { w, h } = decorSize(p.id, def);
      const mirror = facing.mirror !== !!p.mirrorX;
      jobs.push(loadArt(decorationImage(file), aniso).then(art => {
        if (gen !== this.gen) return;
        const lightDef = LIGHT_DEFS[p.id];
        // the lantern glass around the painted flame glows too (spriteMaterial GLOW_RADIUS)
        const emit = lightDef ? { color: lightDef.color, level: { value: FLAME_EMIT }, at: art.flameFound ? { u: art.flame.u, v: art.flame.v, aspect: w / h } : undefined } : undefined;
        const mat = makeSpriteMaterial(false, emit);
        mat.map = art.texture;
        const depth = new THREE.MeshDepthMaterial({ depthPacking: THREE.RGBADepthPacking, alphaTest: 0.5, map: art.texture });
        const card = new THREE.Mesh(cardGeo, mat);
        card.castShadow = true;
        card.renderOrder = CARD_RENDER_ORDER;
        card.customDepthMaterial = depth;
        card.scale.set(w, h, 1);
        card.position.set((0.5 - art.base.u) * w, -art.base.v * h, 0); // opaque base onto the anchor
        // waypoints (floor connectors, stairs, exits) lie flat on the floor, centred on their hexes
        const onFloor = !!def.exitKind;
        if (onFloor) card.position.set(0, -0.5 * h, 0);
        const holder = new THREE.Group();
        holder.add(card);
        if (lightDef) {
          const height = Math.max(MAP_LIGHT_MIN_HEIGHT, (art.flame.v - art.base.v) * h);
          const light = new CarriedLight(lightDef, (p.x * 7.31 + p.y * 3.17) % 6.28, lightDef.radius * LIGHT_RADIUS_MUL, height);
          light.position.x = (art.flame.u - art.base.u) * w;
          light.userData.emit = emit;
          holder.add(light);
          // A flame card must not block its own light.
          card.castShadow = false;
        }
        if (onFloor) card.castShadow = false;
        card.userData.castsShadow = card.castShadow;
        holder.position.set(x, groundAt(x, z), z);
        holder.userData.groundY = holder.position.y;
        holder.userData.onFloor = onFloor;
        holder.scale.x = mirror ? -1 : 1;
        holder.userData.decor = p.id;
        holder.userData.at = { x: p.x, y: p.y };
        // Ember's 3D rule (ThreeBattleRenderer, tactics camera): barricades stand in the authored
        // world direction — along the map, turned by the placement's rot in sixths — instead of
        // swivelling toward the camera, which would break a continuous fence line.
        holder.userData.lockedYaw = BARRICADE_LIKE_DECOR.has(p.id) ? -((p.rot ?? 0) * Math.PI) / 3 : null;
        // Cards of one row stand in one plane (LOCK_PROPS) and cut into each other. Ember's layers
        // say who is in front: "behind" walls step back, foreground pieces forward, lamps and other
        // lights furthest forward; a tiny per-column step keeps any two cards out of one plane.
        // All under half a row, so the next row (and its units) stays in front.
        holder.userData.card = card;
        // a trunk steps back too: the unit standing on it is drawn in front of it
        holder.userData.nudge = (def.unitLayer === 'behind' || STAND_ON_DECOR.has(p.id) ? -0.35 : lightDef ? 0.4 : def.foreground || def.unitLayer === 'front' ? 0.2 : 0) + (p.x % 20) * 0.002;
        holder.userData.hexes = placedFootprint(p).map(({ dx, dy }) => ({ x: p.x + dx, y: p.y + dy }));
        // A unit on a trunk's hex stands on its top: the art's opaque top over that hex's spot on
        // the card (cards stand square to the rows, LOCK_PROPS), relative to that hex's ground.
        if (STAND_ON_DECOR.has(p.id)) for (const { dx, dy } of placedFootprint(p)) {
          if (!inBoard(p.x + dx, p.y + dy)) continue;
          const c = b.cell(p.x + dx, p.y + dy), u = ((c.x - x) * (mirror ? -1 : 1)) / w + art.base.u;
          const n = art.tops.length, cols: number[] = [];
          for (let k = Math.floor((u - 0.06) * n); k <= Math.ceil((u + 0.06) * n); k++) if (k >= 0 && k < n && art.tops[k] >= 0) cols.push(art.tops[k]);
          if (!cols.length) continue;
          cols.sort((a, z) => a - z);
          const top = holder.position.y + (cols[cols.length >> 1] - art.base.v) * h * STAND_ON_TOP;
          this.pads.push({ x: c.x, z: c.z, h: Math.max(0, top - groundAt(c.x, c.z)) });
        }
        this.group.add(holder);
        this.cards.push(holder);
        this.stats.placed++;
      }).catch(() => { this.stats.missingArt.push(p.id); }));
    }
    // decoration images settled so far, for a loading bar (jobs never reject: see the catch above)
    let settled = 0;
    onProgress?.(0, jobs.length);
    if (onProgress) for (const job of jobs) void job.then(() => onProgress(++settled, jobs.length));
    await Promise.all(jobs);
  }

  /** Cards stand locked in place (LOCK_PROPS), or face the camera like the units. `flatTop` set = the old-school 2D view: cards lie flat
   * facing straight up like 2D sprites, raised over the terrain, nearer rows drawn on top. */
  face(yaw: number, flatTop: number | null = null): void {
    for (const c of this.cards) {
      if (c.userData.wall) continue; // 3D walls stand as built
      if (c.userData.onFloor) { c.rotation.set(-Math.PI / 2, 0, 0); c.position.y = c.userData.groundY + FLOOR_LIFT; (c.userData.card as THREE.Mesh).position.z = 0; continue; }
      const locked = (c.userData.lockedYaw as number | null) ?? (LOCK_PROPS ? 0 : null);
      c.rotation.set(flatTop === null ? 0 : -Math.PI / 2, locked !== null && flatTop === null ? locked : yaw, 0, 'YXZ');
      c.position.y = flatTop === null ? c.userData.groundY : flatTop + c.position.z * FLAT_ROW_STEP;
      // the layer step (populate) is along the card's facing; the flat 2D view keeps its row order
      (c.userData.card as THREE.Mesh).position.z = flatTop === null && LOCK_PROPS ? c.userData.nudge as number : 0;
      for (const o of c.children) if ((o as THREE.Mesh).isMesh) o.castShadow = flatTop === null && !!o.userData.castsShadow;
    }
  }

  /** Battle: remove the chest cards whose chest is gone from the engine's own decoration list
   * (opened with a lockpick, or already opened in a restored fight). */
  dropOpenedChests(present: readonly { id: string; x: number; y: number }[]): void {
    const keep = new Set(present.filter(d => CHEST_DECOR_IDS.has(d.id)).map(d => `${d.id}|${d.x},${d.y}`));
    this.cards = this.cards.filter(c => {
      const at = c.userData.at as { x: number; y: number } | undefined;
      if (!CHEST_DECOR_IDS.has(c.userData.decor) || !at || keep.has(`${c.userData.decor}|${at.x},${at.y}`)) return true;
      this.group.remove(c);
      c.traverse(o => { const m = o as THREE.Mesh; if (m.isMesh) (m.material as THREE.Material).dispose(); if (o instanceof CarriedLight) o.dispose(); });
      return false;
    });
  }

  /** How high above the ground a unit at (x, z) stands because it is on a fallen trunk: the
   * trunk's top on its hex, easing in over the hex's edge so a step up is not a jump. */
  standAt(x: number, z: number): number {
    let best = 0;
    for (const p of this.pads) {
      const d = Math.hypot(x - p.x, z - p.z);
      if (d < 0.95) best = Math.max(best, p.h * (1 - THREE.MathUtils.smoothstep(d, 0.55, 0.95)));
    }
    return best;
  }

  /** How many decoration cards are placed (they load in after the board). */
  get count(): number { return this.cards.length; }

  /** Fog of war (Ember's rule): a decoration shows once any hex of its footprint has been seen.
   * In 3D it stands up out of the fog sheet, so out of sight it is dimmed like the ground under
   * it. `fog(x, y)` is that hex's darkness: 0 in sight, Ember's 0.6 remembered, 1 never seen. */
  shade(fog: ((x: number, y: number) => number) | null): void {
    for (const c of this.cards) {
      const dark = fog ? Math.min(...(c.userData.hexes as { x: number; y: number }[]).map(h => fog(h.x, h.y))) : 0;
      c.visible = dark < 1;
      if (c.userData.wall) continue; // walls keep their own colour
      for (const o of c.children) {
        const m = (o as THREE.Mesh).material as THREE.MeshLambertMaterial | undefined;
        if ((o as THREE.Mesh).isMesh && m) m.color.setScalar(1 - dark);
        if (o instanceof CarriedLight) o.visible = dark < 1;
      }
    }
  }

  updateLights(time: number): void {
    this.group.traverse(o => {
      if (!(o instanceof CarriedLight)) return;
      o.update(time);
      // the painted flame flickers with its own light
      const emit = o.userData.emit as SpriteEmit | undefined;
      if (emit) emit.level.value = FLAME_EMIT * flickerAt(time, o.seed, o.definition.flicker);
    });
  }
}
