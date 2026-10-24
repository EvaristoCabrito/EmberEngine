// "Elenco": the imported heroes, summons and monsters standing on the 3D board, with every pose
// playable from a panel and click-to-walk. A test bench for sprites in the diorama world.

import * as THREE from 'three';
import type { Stage } from '../render/stage';
import type { CameraRig } from '../render/cameraRig';
import type { Board } from '../map/board';
import { TERRAIN_INFO } from '../map/board';
import { neighbors, hexAt, corner } from '../core/hex';
import { UNITS, POSE_LABEL, unitForSpawn, type Pose, type UnitDef, type Footprint } from '../units/catalog';
import { UnitActor } from '../units/actor';
import { initFrames, pumpUploads, spriteMemory } from '../units/frames';
import { muted, setMuted } from '../units/sounds';

interface Placed { actor: UnitActor; c: number; r: number }

const POSE_ORDER: Pose[] = ['idle', 'idle2', 'walk', 'attack', 'attack2', 'attackShort', 'counter', 'cast', 'heal', 'hit', 'hit2', 'death', 'death2'];
const GROUP_LABEL: Record<UnitDef['group'], string> = { hero: 'Heróis', summon: 'Invocações', monster: 'Monstros', npc: 'NPCs' };
const GROUPS: UnitDef['group'][] = ['hero', 'summon', 'monster', 'npc'];

/** Ember's footprint(): offsets authored for an even anchor row; odd-dy rows shift with the anchor's parity. */
function footprintCells(f: Footprint | undefined, c: number, r: number): [number, number][] {
  if (!f) return [[c, r]];
  const shift = r & 1;
  return f.map(o => [c + o.dx + (o.dy & 1 ? shift : 0), r + o.dy] as [number, number]);
}

export class CastShowcase {
  private placed: Placed[] = [];
  private selected: Placed | null = null;
  private readonly group = new THREE.Group();
  private readonly ring: THREE.Mesh;
  private board: Board | null = null;
  /** Hexes blocked by decorations ("c,r"); units never stand or walk there. */
  blocked: Set<string> = new Set();
  private readonly panel: HTMLElement;

  constructor(private readonly stage: Stage, private readonly rig: CameraRig, private readonly groundAt: (x: number, z: number) => number) {
    initFrames(stage.renderer);
    stage.onFrame(() => pumpUploads());
    stage.scene.add(this.group);
    const pos: number[] = [], idx: number[] = [];
    for (let k = 0; k < 6; k++) { const [ox, oz] = corner(0, 0, k, 0.98), [ix, iz] = corner(0, 0, k, 0.78); pos.push(ox, 0, oz, ix, 0, iz); }
    for (let k = 0; k < 6; k++) { const a = k * 2, b = ((k + 1) % 6) * 2; idx.push(a, b, a + 1, b, b + 1, a + 1); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx);
    this.ring = new THREE.Mesh(g, new THREE.MeshBasicMaterial({ color: new THREE.Color(0.9, 1.9, 2.8), side: THREE.DoubleSide, transparent: true, depthWrite: false }));
    this.ring.renderOrder = 6; this.ring.visible = false;
    stage.scene.add(this.ring);
    this.panel = document.getElementById('cast')!;
    stage.onFrame(dt => this.tick(dt));
  }

  /** Front-row centroid of a footprint (Ember's footprintCentroid): where the feet stand. */
  private anchor(def: UnitDef, c: number, r: number): { x: number; z: number } {
    const b = this.board!;
    const front = def.footprint ? def.footprint.filter(o => o.dy === 0).map(o => [c + o.dx, r] as [number, number]) : [[c, r] as [number, number]];
    let x = 0, z = 0, n = 0;
    for (const [fc, fr] of front) { if (fc < 0 || fc >= b.layout.cols) continue; const cell = b.cell(fc, fr); x += cell.x; z += cell.z; n++; }
    return n ? { x: x / n, z: z / n } : { x: b.cell(c, r).x, z: b.cell(c, r).z };
  }

  private fits(def: UnitDef, c: number, r: number, ignore?: Placed): boolean {
    const b = this.board!;
    for (const [fc, fr] of footprintCells(def.footprint, c, r)) {
      if (fc < 0 || fr < 0 || fc >= b.layout.cols || fr >= b.layout.rows) return false;
      const cell = b.cell(fc, fr);
      if (!TERRAIN_INFO[cell.type]?.passable || this.blocked.has(`${fc},${fr}`)) return false;
      for (const p of this.placed) {
        if (p === ignore) continue;
        if (footprintCells(p.actor.def.footprint, p.c, p.r).some(([pc, pr]) => pc === fc && pr === fr)) return false;
      }
    }
    return true;
  }

  /** Nearest spot (spiralling out over hexes) where this unit's whole body fits. */
  private findSpot(def: UnitDef, c: number, r: number): [number, number] | null {
    const b = this.board!, seen = new Set<number>(), q: [number, number][] = [[c, r]];
    seen.add(r * b.layout.cols + c);
    while (q.length) {
      const [qc, qr] = q.shift()!;
      if (this.fits(def, qc, qr)) return [qc, qr];
      for (const [nc, nr] of neighbors(b.layout, qc, qr)) { const k = nr * b.layout.cols + nc; if (!seen.has(k)) { seen.add(k); q.push([nc, nr]); } }
      if (seen.size > 4000) break;
    }
    return null;
  }

  async populate(board: Board): Promise<void> {
    this.clear();
    this.board = board;
    // Ember's own spawns: each map stands exactly the units it was authored with, and only their art loads.
    const d = board.draft;
    const sides = [['player', d.playerSpawns], ['enemy', d.enemySpawns], ['neutral', d.neutralSpawns]] as const;
    for (const [side, spawns] of sides) {
      for (const s of spawns ?? []) {
        const def = unitForSpawn(s, side);
        if (def) this.place(def, s.x, s.y, side === 'enemy' ? -1 : 1);
        else console.warn('[engine2] no unit for spawn', s);
      }
    }
    this.renderPanel();
    await Promise.all(this.placed.map(p => p.actor.ready().catch(e => console.warn('[engine2] sprite', p.actor.def.key, e))));
    // warm every pose in the background, one unit at a time, so the first play never waits
    void (async () => { for (const p of this.placed) await p.actor.preloadAll(); })();
    (window as unknown as { __cast: unknown }).__cast = {
      memory: () => spriteMemory(),
      actor: (k: string) => this.placed.find(p => p.actor.def.key === k)?.actor, units: this.placed.map(p => p.actor.def.key), select: (k: string) => this.select(this.placed.find(p => p.actor.def.key === k) ?? null, true), play: (p: Pose) => void this.selected?.actor.play(p) };
  }

  /** Stand a unit at (or nearest free spot to) a hex. */
  private place(def: UnitDef, c: number, r: number, facing: 1 | -1): Placed | null {
    const spot = this.findSpot(def, c, r);
    if (!spot) return null;
    const actor = new UnitActor(def);
    const a = this.anchor(def, spot[0], spot[1]);
    actor.x = a.x; actor.z = a.z;
    actor.facing = facing;
    const p = { actor, c: spot[0], r: spot[1] };
    this.placed.push(p);
    this.group.add(actor.mesh);
    return p;
  }

  /** Test any of Ember's units: drop it near the middle of the board and select it. */
  private add(def: UnitDef): void {
    const L = this.board!.layout;
    const p = this.place(def, Math.floor(L.cols / 2), Math.floor(L.rows / 2), def.group === 'monster' ? -1 : 1);
    if (!p) return;
    void p.actor.ready().then(() => p.actor.preloadAll()).catch(e => console.warn('[engine2] sprite', def.key, e));
    this.select(p, true);
  }

  /** The board was rebuilt after a terrain edit: keep the units, read heights from the new board. */
  setBoard(b: Board): void { this.board = b; }

  clear(): void {
    for (const p of this.placed) { this.group.remove(p.actor.mesh); p.actor.dispose(); }
    this.placed = [];
    this.selected = null;
    this.ring.visible = false;
  }

  /** A board click: select the unit under the cursor, or walk the selected unit to the clicked hex. */
  click(ray: THREE.Raycaster, groundHit: THREE.Vector3 | null): boolean {
    const hit = ray.intersectObjects(this.placed.map(p => p.actor.pickMesh), false)[0];
    if (hit) { this.select(this.placed.find(p => p.actor.pickMesh === hit.object) ?? null); return true; }
    if (this.selected && groundHit && this.board) {
      const [c, r] = hexAt(this.board.layout, groundHit.x, groundHit.z);
      this.walkTo(this.selected, c, r);
      return true;
    }
    return false;
  }

  private walkTo(p: Placed, tc: number, tr: number): void {
    const b = this.board!;
    if (!this.fits(p.actor.def, tc, tr, p)) return;
    // breadth-first path over cells the whole body fits on
    const key = (c: number, r: number) => r * b.layout.cols + c;
    const prev = new Map<number, number>([[key(p.c, p.r), -1]]), q: [number, number][] = [[p.c, p.r]];
    while (q.length && !prev.has(key(tc, tr))) {
      const [c, r] = q.shift()!;
      for (const [nc, nr] of neighbors(b.layout, c, r)) {
        const k = key(nc, nr);
        if (prev.has(k) || !this.fits(p.actor.def, nc, nr, p)) continue;
        prev.set(k, key(c, r)); q.push([nc, nr]);
      }
    }
    if (!prev.has(key(tc, tr))) return;
    const cells: [number, number][] = [];
    for (let k = key(tc, tr); k !== -1 && k !== key(p.c, p.r); k = prev.get(k)!) cells.unshift([k % b.layout.cols, Math.floor(k / b.layout.cols)]);
    p.c = tc; p.r = tr;
    void p.actor.walk(cells.map(([c, r]) => this.anchor(p.actor.def, c, r)));
  }

  /** Select a unit; from the list, also bring the camera to it. */
  select(p: Placed | null, focus = false): void {
    this.selected = p;
    if (p && focus) {
      this.rig.target.set(p.actor.x, this.groundAt(p.actor.x, p.actor.z) + 1, p.actor.z);
      this.rig.apply();
    }
    this.renderPanel();
  }

  private tick(dt: number): void {
    const yaw = this.rig.facingYaw;
    for (const p of this.placed) p.actor.update(dt, yaw, this.groundAt);
    const s = this.selected;
    if (s) {
      this.ring.position.set(s.actor.x, this.groundAt(s.actor.x, s.actor.z) + 0.05, s.actor.z);
      this.ring.visible = true;
    } else this.ring.visible = false;
  }

  private renderPanel(): void {
    const el = this.panel;
    el.innerHTML = '';
    const head = document.createElement('div');
    head.className = 'cast-head';
    head.innerHTML = `<b>Elenco</b>`;
    const mute = document.createElement('button');
    mute.textContent = muted ? 'Som: off' : 'Som: on';
    mute.addEventListener('click', () => { setMuted(!muted); this.renderPanel(); });
    head.appendChild(mute);
    el.appendChild(head);
    const pick = document.createElement('select');
    pick.className = 'cast-add';
    // Ember reuses class names (three "Lanceiro"s): those also show their class id to tell them apart
    const named = new Map<string, number>();
    for (const u of UNITS) named.set(u.name, (named.get(u.name) ?? 0) + 1);
    const label = (u: UnitDef) => (named.get(u.name)! > 1 ? `${u.name} (${u.key})` : u.name);
    pick.innerHTML = `<option value="">+ Adicionar unidade (${UNITS.length})</option>` + GROUPS.map(g =>
      `<optgroup label="${GROUP_LABEL[g]}">${UNITS.filter(u => u.group === g).map(u => `<option value="${u.key}">${label(u)}</option>`).join('')}</optgroup>`).join('');
    pick.addEventListener('change', () => { const def = UNITS.find(u => u.key === pick.value); if (def && this.board) this.add(def); });
    el.appendChild(pick);
    for (const group of GROUPS) {
      const members = this.placed.filter(q => q.actor.def.group === group);
      if (!members.length) continue;
      const h = document.createElement('div'); h.className = 'cast-group'; h.textContent = GROUP_LABEL[group]; el.appendChild(h);
      for (const p of members) {
        const row = document.createElement('div');
        row.className = 'cast-unit' + (p === this.selected ? ' on' : '');
        row.textContent = p.actor.def.name;
        row.addEventListener('click', () => this.select(p, true));
        el.appendChild(row);
        if (p !== this.selected) continue;
        const poses = document.createElement('div'); poses.className = 'cast-poses';
        for (const pose of POSE_ORDER) {
          if (!p.actor.has(pose)) continue;
          const b = document.createElement('button'); b.textContent = POSE_LABEL[pose];
          b.addEventListener('click', () => void p.actor.play(pose));
          poses.appendChild(b);
        }
        const turn = document.createElement('button'); turn.textContent = 'Virar';
        turn.addEventListener('click', () => { p.actor.facing = p.actor.facing === 1 ? -1 : 1; void p.actor.play('idle'); });
        const revive = document.createElement('button'); revive.textContent = 'Reviver';
        revive.addEventListener('click', () => p.actor.revive());
        poses.append(turn, revive);
        const hint = document.createElement('div'); hint.className = 'muted cast-hint'; hint.textContent = 'Clique num hex para andar até lá.';
        poses.appendChild(hint);
        el.appendChild(poses);
      }
    }
  }
}
