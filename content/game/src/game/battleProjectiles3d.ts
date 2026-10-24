// Ember's arrow on the 3D map. Ember's engine still fires it (emitMissileFx "longShot": every
// bow attack and arrow skill) and times it (ARROW_TRAVEL, afterglow); its 2D renderer drew the
// approved arrow art (arrow-002.png, alpha baked from black — engine.art.arrowCore) flying hex to
// hex, a faint wake of two rings behind it, and for Neera a blood-red wake, ember motes and a
// burst of red rays on impact. Here the same art and wake fly in 3D: from bow height to the
// target's chest on a slight arc, the arrow pointing along its path and turned to face the
// camera from any angle.
//
// The mage's normal attack (emitMissileFx "arcaneBolt") rides the same flights: Ember drew it as
// two thin purple pressure-wave rings behind a small pink core on a gently weaving path, and a
// burst of eight pink rays at the target.

import * as THREE from "three";
import type { BattleEngine } from "./engine";
import type { DioramaView } from "../editor/ember/dioramaView";

type Any = Record<string, any>;

interface MissileFx { live: boolean; fromX: number; fromY: number; toX: number; toY: number; t: number; travel: number; max: number; neeraArrow: boolean; kind: string; seed: number }

/** Ember's MISSILE_AFTERGLOW: how long the wake lingers after the arrow lands. */
const AFTERGLOW = 0.2;
/** Release and impact heights above the ground (world units = hex radii). */
const RELEASE_Y = 0.95, IMPACT_Y = 0.8;
/** Arc height as a share of the flight distance. */
const ARC = 0.08;
/** Ember drew the arrow image 1.35 hexes square (25% larger than first, to read over the grid). */
const ARROW_SIZE = 1.35;
/** Neera's impact has 7 rays, the mage bolt's 8. */
const SPARKS = 5, RAYS = 8;

/** A soft ellipse ring (Ember's wake) or a round mote, drawn once. */
function ringTexture(): THREE.CanvasTexture {
  const c = document.createElement("canvas"); c.width = 128; c.height = 32;
  const g = c.getContext("2d")!;
  g.strokeStyle = "#fff"; g.lineWidth = 3;
  g.beginPath(); g.ellipse(64, 16, 60, 13, 0, 0, Math.PI * 2); g.stroke();
  return new THREE.CanvasTexture(c);
}
function moteTexture(): THREE.CanvasTexture {
  const c = document.createElement("canvas"); c.width = c.height = 32;
  const g = c.getContext("2d")!;
  const r = g.createRadialGradient(16, 16, 0, 16, 16, 16);
  r.addColorStop(0, "rgba(255,255,255,1)"); r.addColorStop(1, "rgba(255,255,255,0)");
  g.fillStyle = r; g.fillRect(0, 0, 32, 32);
  return new THREE.CanvasTexture(c);
}

interface Flight {
  arrow: THREE.Mesh;
  wake: THREE.Mesh[];
  sparks: THREE.Sprite[];
  rays: THREE.Mesh[];
}

export class ArrowFx3D {
  private readonly group = new THREE.Group();
  private readonly flights: Flight[] = [];
  private readonly arrowGeo: THREE.PlaneGeometry;
  private readonly rayGeo = new THREE.PlaneGeometry(1, 0.06).translate(0.5, 0, 0);
  private readonly ringTex = ringTexture();
  private readonly moteTex = moteTexture();
  private arrowTex: THREE.CanvasTexture | null = null;
  private readonly tmp = { a: new THREE.Vector3(), b: new THREE.Vector3(), dir: new THREE.Vector3(), view: new THREE.Vector3(), up: new THREE.Vector3(), side: new THREE.Vector3(), m: new THREE.Matrix4() };

  constructor(private readonly view: DioramaView, private readonly engine: BattleEngine) {
    // the source art points north-east (45°); turn the quad so the arrow lies along +x
    this.arrowGeo = new THREE.PlaneGeometry(ARROW_SIZE, ARROW_SIZE).rotateZ(-Math.PI / 4);
    this.group.renderOrder = 20;
    view.stage.scene.add(this.group);
  }

  private flight(i: number): Flight {
    let f = this.flights[i];
    if (f) return f;
    if (!this.arrowTex) {
      this.arrowTex = new THREE.CanvasTexture((this.engine as unknown as Any).art.arrowCore as HTMLCanvasElement);
      this.arrowTex.colorSpace = THREE.SRGBColorSpace;
    }
    const additive = (tex: THREE.Texture) => new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide, toneMapped: false });
    const arrow = new THREE.Mesh(this.arrowGeo, new THREE.MeshBasicMaterial({ map: this.arrowTex, transparent: true, depthWrite: false, side: THREE.DoubleSide, alphaTest: 0.02 }));
    arrow.renderOrder = 21;
    const wake = [0, 1].map(() => { const m = new THREE.Mesh(new THREE.PlaneGeometry(1, 0.25), additive(this.ringTex)); m.renderOrder = 20; return m; });
    const sparks = Array.from({ length: SPARKS }, () => {
      const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: this.moteTex, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, toneMapped: false }));
      s.renderOrder = 20; return s;
    });
    const rays = Array.from({ length: RAYS }, () => { const m = new THREE.Mesh(this.rayGeo, additive(this.moteTex)); m.renderOrder = 20; return m; });
    f = { arrow, wake, sparks, rays };
    this.group.add(arrow, ...wake, ...sparks, ...rays);
    this.flights[i] = f;
    return f;
  }

  /** World point of a hex at a height above its ground. */
  private at(col: number, row: number, height: number, out: THREE.Vector3): THREE.Vector3 {
    const c = this.view.board!.cell(col, row);
    return out.set(c.x, this.view.groundAt(c.x, c.z) + height, c.z);
  }

  /** The arrow's point `k` (0..1) along its arc, and its direction there. `weave` (the shot's
   * seed) adds Ember's sideways weave of the mage bolt: sin(k·2.4π + seed)·0.16·(1 − 0.6k) hexes. */
  private along(from: THREE.Vector3, to: THREE.Vector3, k: number, pos: THREE.Vector3, dir?: THREE.Vector3, weave?: number): void {
    const dist = from.distanceTo(to);
    pos.lerpVectors(from, to, k);
    pos.y += Math.sin(Math.PI * k) * ARC * dist;
    if (weave !== undefined) {
      const side = this.tmp.side.set(-(to.z - from.z), 0, to.x - from.x).normalize();
      pos.addScaledVector(side, Math.sin(k * Math.PI * 2.4 + weave) * 0.16 * (1 - k * 0.6));
    }
    if (dir) {
      dir.subVectors(to, from);
      dir.y += Math.cos(Math.PI * k) * Math.PI * ARC * dist;
      dir.normalize();
    }
  }

  /** Lay a flat quad (local +x = length) along `dir`, turned about that axis to face the camera. */
  private orient(obj: THREE.Object3D, pos: THREE.Vector3, dir: THREE.Vector3): void {
    const { view, up, m } = this.tmp;
    view.subVectors(this.view.stage.camera.position, pos);
    view.addScaledVector(dir, -view.dot(dir)).normalize();
    up.crossVectors(view, dir).normalize();
    m.makeBasis(dir, up, view);
    obj.quaternion.setFromRotationMatrix(m);
    obj.position.copy(pos);
  }

  sync(): void {
    const list = (this.engine as unknown as Any).missileFx as MissileFx[] | undefined;
    if (!list || !this.view.board) { this.group.visible = false; return; }
    this.group.visible = true;
    const { a: from, b: to, dir } = this.tmp;
    const pos = new THREE.Vector3(), back = new THREE.Vector3(), backDir = new THREE.Vector3();
    list.forEach((m, i) => {
      const live = m.live && (m.kind === "longShot" || m.kind === "arcaneBolt");
      const arcane = m.kind === "arcaneBolt", weave = arcane ? m.seed : undefined;
      if (!live && !this.flights[i]) return;
      const f = this.flight(i);
      const show = (on: boolean) => { f.arrow.visible = on; for (const o of [...f.wake, ...f.sparks, ...f.rays]) o.visible = on; };
      if (!live) { show(false); return; }
      this.at(m.fromX, m.fromY, RELEASE_Y, from);
      this.at(m.toX, m.toY, IMPACT_Y, to);
      const kHead = Math.min(1, m.t / m.travel);
      const afterglow = Math.max(0, (m.t - m.travel) / AFTERGLOW);
      const fade = 1 - afterglow;

      this.along(from, to, kHead, pos, dir, weave);
      this.orient(f.arrow, pos, dir);
      f.arrow.visible = !arcane && fade > 0;
      (f.arrow.material as THREE.MeshBasicMaterial).opacity = fade;

      // the wake: two rings behind the head, red for Neera, pale for every other archer, purple
      // pressure waves behind the mage's bolt
      f.wake.forEach((ring, r) => {
        const bk = Math.max(0, kHead - (r + 1) * 0.1);
        this.along(from, to, bk, back, backDir, weave);
        this.orient(ring, back, backDir);
        ring.scale.set(0.2 + (r + 1) * 0.07, 1 + r * 0.4, 1);
        const mat = ring.material as THREE.MeshBasicMaterial;
        mat.color.set(arcane ? 0xca5cff : m.neeraArrow ? 0xdc2636 : 0xd7dee2);
        mat.opacity = fade * (arcane ? 0.72 : m.neeraArrow ? 0.42 : 0.24);
        ring.visible = fade > 0;
      });

      // Neera: ember-red motes follow the arrow, and red rays flare out where it lands
      const time = (this.engine as unknown as Any).time as number ?? 0;
      f.sparks.forEach((s, k) => {
        if (arcane) {
          // the bolt's small pink core
          s.visible = k === 0 && fade > 0;
          if (!s.visible) return;
          s.position.copy(pos);
          s.scale.set(0.2, 0.2, 1);
          (s.material as THREE.SpriteMaterial).color.setRGB(244 / 255, 150 / 255, 1);
          (s.material as THREE.SpriteMaterial).opacity = fade * 0.92;
          return;
        }
        s.visible = m.neeraArrow && fade > 0;
        if (!s.visible) return;
        const sk = Math.max(0, kHead - k * 0.055);
        this.along(from, to, sk, back);
        const phase = m.seed + k * 2.4 + time * 5, spread = 0.025 + k * 0.012;
        s.position.set(back.x + Math.cos(phase) * spread, back.y + Math.sin(phase) * spread, back.z);
        const size = (0.018 + (k % 2) * 0.008) * 4;
        s.scale.set(size, size, 1);
        (s.material as THREE.SpriteMaterial).color.setRGB(1, (58 + k * 14) / 255, (48 + k * 8) / 255);
        (s.material as THREE.SpriteMaterial).opacity = fade * (0.85 - k * 0.11);
      });
      f.rays.forEach((ray, k) => {
        const count = arcane ? 8 : 7;
        ray.visible = (arcane || m.neeraArrow) && k < count && kHead >= 1 && fade > 0;
        if (!ray.visible) return;
        const a = m.seed + k * (Math.PI * 2 / count);
        // rays fan out in the plane facing the camera
        const { view, up } = this.tmp;
        view.subVectors(this.view.stage.camera.position, pos).normalize();
        up.set(0, 1, 0).addScaledVector(view, -view.y).normalize();
        const right = new THREE.Vector3().crossVectors(up, view).normalize();
        const rayDir = right.multiplyScalar(Math.cos(a)).addScaledVector(up, Math.sin(a)).normalize();
        const inner = arcane ? 0.05 : 0.035, outer = arcane ? 0.12 + 0.09 * afterglow : 0.12 + afterglow * 0.12;
        this.orient(ray, pos.clone().addScaledVector(rayDir, inner), rayDir);
        ray.scale.set(arcane ? outer - inner : outer, 1, 1);
        const mat = ray.material as THREE.MeshBasicMaterial;
        if (arcane) mat.color.setRGB(1, 110 / 255, 220 / 255); else mat.color.setRGB(1, 75 / 255, 62 / 255);
        mat.opacity = fade * (arcane ? 0.88 : 0.8);
      });
    });
  }

  dispose(): void {
    this.view.stage.scene.remove(this.group);
    this.group.traverse(o => { const m = o as THREE.Mesh; if (m.isMesh || (o as THREE.Sprite).isSprite) (m.material as THREE.Material).dispose(); });
    this.arrowGeo.dispose(); this.rayGeo.dispose(); this.ringTex.dispose(); this.moteTex.dispose(); this.arrowTex?.dispose();
  }
}
