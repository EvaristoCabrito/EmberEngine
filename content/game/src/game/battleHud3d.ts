// Ember's unit HUD on the 3D map: the HP bar and HP number over every visible unit, and the
// floating combat text (damage, "Missed", heals, level-up labels). Ember drew both on a flat
// screen canvas over its renderer (ThreeBattleRenderer.renderUnitHealthHud and the engine's own
// renderFloatingTextHud); here the same drawing is placed from the real 3D unit cards and
// terrain, and sized by how large a hex is on screen right now, so it follows any zoom or angle.

import * as THREE from "three";
import type { BattleEngine } from "./engine";
import type { Unit } from "./types";
import type { DioramaView } from "../editor/ember/dioramaView";
import type { UnitActor } from "../units/actor";
import { isBossClass } from "./data";
import { unitSize } from "./pathfinding";

type Any = Record<string, any>;
const SQRT3 = Math.sqrt(3);

export class BattleHud3D {
  private readonly ctx: CanvasRenderingContext2D;
  private readonly v = new THREE.Vector3();
  private readonly w = new THREE.Vector3();
  private readonly right = new THREE.Vector3();

  constructor(private readonly canvas: HTMLCanvasElement, private readonly view: DioramaView, private readonly engine: BattleEngine) {
    this.ctx = canvas.getContext("2d")!;
  }

  /** World point → CSS pixel on the battle view. */
  private screen(p: THREE.Vector3, cssW: number, cssH: number): { x: number; y: number; ok: boolean } {
    this.v.copy(p).project(this.view.stage.camera);
    return { x: (this.v.x + 1) * cssW / 2, y: (1 - this.v.y) * cssH / 2, ok: this.v.z < 1 };
  }

  /** Screen pixels per world unit (one hex radius) at a point, across the view. */
  private pxPerUnit(p: THREE.Vector3, cssW: number, cssH: number): number {
    const cam = this.view.stage.camera;
    this.right.setFromMatrixColumn(cam.matrixWorld, 0).normalize();
    const a = this.screen(p, cssW, cssH);
    const b = this.screen(this.w.copy(p).add(this.right), cssW, cssH);
    return Math.hypot(b.x - a.x, b.y - a.y);
  }

  render(actors: Map<string, { actor: UnitActor }>): void {
    const board = this.view.board;
    const cssW = this.canvas.clientWidth, cssH = this.canvas.clientHeight, dpr = devicePixelRatio || 1;
    if (this.canvas.width !== Math.round(cssW * dpr) || this.canvas.height !== Math.round(cssH * dpr)) {
      this.canvas.width = Math.round(cssW * dpr); this.canvas.height = Math.round(cssH * dpr);
    }
    const ctx = this.ctx;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssW, cssH);
    if (!board) return;
    const e = this.engine as unknown as Any;
    const tile = (e.effectAnchor(0, 0) as { tile: number }).tile;
    this.healthBars(actors, cssW, cssH, tile);
    this.floatingText(cssW, cssH, tile);
  }

  /** Ember's renderUnitHealthHud: bar + HP over each unit's head. Its sizes are in Ember pixels
   * of a `tile`-radius hex; here they scale with that unit's hex size on screen. */
  private healthBars(actors: Map<string, { actor: UnitActor }>, cssW: number, cssH: number, tile: number): void {
    const ctx = this.ctx, engine = this.engine;
    const animating = engine.isAnimating();
    for (const unit of engine.units as Unit[]) {
      if (!unit.alive || (unit.fade ?? 1) <= 0 || engine.unitHidden(unit)) continue;
      const actor = actors.get(unit.id)?.actor;
      if (!actor?.mesh.visible) continue;
      const head = actor.pickMesh.localToWorld(this.v.set(0, 1, 0)).clone();
      const at = this.screen(head, cssW, cssH);
      if (!at.ok) continue;
      const scale = this.pxPerUnit(head, cssW, cssH) / tile; // Ember px → screen px
      const cell = tile * SQRT3 * scale;
      const gap = Math.max(8 * scale, cell * 0.12);
      const size = unitSize(unit), boss = isBossClass(unit.classId);
      const width = cell * (size >= 4 ? 1.35 : size === 2 ? 0.9 : boss ? 0.68 : 0.62);
      const height = Math.max(3, cell * 0.07);
      const left = at.x - width / 2, top = at.y - gap - height;
      ctx.save();
      ctx.globalAlpha = (unit.fade ?? 1) * (unit.moved && unit.side === "player" && engine.phase === "player" && !animating ? 0.9 : 1);
      ctx.fillStyle = "rgba(12,11,10,0.82)";
      ctx.fillRect(left - 1, top - 1, width + 2, height + 2);
      ctx.fillStyle = "#2c2824";
      ctx.fillRect(left, top, width, height);
      ctx.fillStyle = unit.side === "player" ? "#c8c4bc" : unit.side === "neutral" ? "#5f9e52" : "#b54a32";
      ctx.fillRect(left, top, width * Math.max(0, unit.hp / unit.maxHp), height);
      // Ember shows the number once a hex is at least 32 px wide
      if (cell >= 32) {
        const fontPx = Math.max(1, Math.round(cell * 0.22));
        ctx.font = `600 ${fontPx}px Figtree, sans-serif`;
        ctx.textAlign = "center";
        ctx.textBaseline = "bottom";
        ctx.lineJoin = "round";
        ctx.lineWidth = Math.max(1, 3 * Math.min(scale, 1.5));
        ctx.strokeStyle = "rgba(12,11,10,0.9)";
        ctx.fillStyle = "#f0ebe3";
        ctx.strokeText(`${unit.hp}`, at.x, top - 1);
        ctx.fillText(`${unit.hp}`, at.x, top - 1);
      }
      ctx.restore();
    }
  }

  /** The engine's own floating text, placed on the 3D board: an Ember screen point (board
   * pixels minus its camera) becomes that spot on the terrain, lifted `up` Ember pixels. */
  private floatingText(cssW: number, cssH: number, tile: number): void {
    const e = this.engine as unknown as Any;
    const board = this.view.board!;
    const a0 = e.effectAnchor(0, 0) as { worldX: number; worldY: number };
    const L = board.layout;
    const p = new THREE.Vector3();
    // the same board-pixel → world mapping Battle3D places the units with
    const toScreen = (cx: number, cy: number, up: number) => {
      const x = (e.camX + cx - a0.worldX) / tile - L.ox, z = (e.camY + cy - a0.worldY) / tile - L.oz;
      p.set(x, this.view.groundAt(x, z) + up / tile, z);
      const s = this.screen(p, cssW, cssH);
      return { x: s.x, y: s.y };
    };
    // one scale for the frame: a hex at the board's centre
    const b = board.bounds();
    const mid = new THREE.Vector3((b.x0 + b.x1) / 2, 0, (b.z0 + b.z1) / 2);
    mid.y = this.view.groundAt(mid.x, mid.z);
    const scale = THREE.MathUtils.clamp(this.pxPerUnit(mid, cssW, cssH) / tile, 0.45, 2.2);
    e.renderFloatingTextHud(this.ctx, toScreen, scale);
  }
}
