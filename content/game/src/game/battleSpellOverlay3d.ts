// Original Ember's canvas spell art (lightning, healing, Turn Undead, Provoke, portals,
// status effects) and Frost/Blizzard WebGL pass, projected onto Engine2's battle camera.
import * as THREE from "three";
import type { BattleEngine } from "./engine";
import type { DioramaView } from "../editor/ember/dioramaView";
import { WebGL2DRenderer } from "./gfx/WebGL2DRenderer";
import { EffectsRenderer } from "../vfx/legacy/two-d/EffectsRenderer";
import { footprintFrontRow } from "./pathfinding";
import { CARD_RENDER_ORDER, FOG_RENDER_ORDER, UNIT_RENDER_ORDER } from "../render/drawOrder";

type PortalItem = { live: boolean; x: number; y: number; body: { dx: number; dy: number }[] | null };
/** After the ground and terrain fog, before unit/decoration cards: portals sit under the units. */
const PORTAL_RENDER_ORDER = (FOG_RENDER_ORDER + CARD_RENDER_ORDER) / 2;
const PORTAL_TEX_W = 512, PORTAL_TEX_H = 512;
/** A Warp gate is a standing thing, not a mark on the floor: it is depth-tested and drawn after
 * the units, so whatever stands nearer covers it and whatever is farther stays behind it. The
 * whole sheet takes the depth of the hex it stands on (gl_FragDepth): tested plane by plane, the
 * camera-facing gate cut through prop cards standing square to the hex rows. */
const WARP_GATE_RENDER_ORDER = UNIT_RENDER_ORDER + 0.1;
/** Frost's strike: an icy flash on each hex as the ice lands, about a strong lantern's light,
 * fading in a fraction of a second (the pulse decay in sync). */
const FROST_FLASH = 8;
/** A potion burst's glow (Ember: 0.95 tile around the chest) as a share of the unit card's height. */
const POTION_GLOW_OF_CARD = 0.55;

export class BattleSpellOverlay3D {
  private readonly legacy: WebGL2DRenderer;
  private readonly effects: EffectsRenderer;
  private readonly blizzards = new Map<number, number>();
  private webShot: number | null = null;
  private readonly portalSheets = new Map<PortalItem, { mesh: THREE.Mesh; canvas: HTMLCanvasElement; texture: THREE.CanvasTexture; depth?: { value: number }; outline?: THREE.Vector3[] }>();
  /** The effects canvas as a texture, while an Ice Storm puts it in the board's layers. */
  private readonly fxTexture: THREE.CanvasTexture;
  private readonly pulses =Array.from({ length: 4 }, () => ({ age: 1, light: new THREE.PointLight(0xb6e7ff, 0, 3.2, 1.8) }));
  /** `cardAt`: a unit's 3D card (body centre and height), where potion bursts are centred. */
  constructor(private readonly artCanvas: HTMLCanvasElement, private readonly fxCanvas: HTMLCanvasElement, private readonly view: DioramaView, private readonly engine: BattleEngine,
    private readonly cardAt?: (unitId: string) => { centre: THREE.Vector3; height: number } | null) {
    this.legacy = new WebGL2DRenderer(artCanvas, true);
    this.effects = new EffectsRenderer(fxCanvas, true);
    this.fxTexture = new THREE.CanvasTexture(fxCanvas);
    (engine as unknown as { warpFloorHalo: boolean }).warpFloorHalo = false;
    for (const pulse of this.pulses) view.stage.scene.add(pulse.light);
  }
  /** `intensity` overrides the blizzard-scaled 0.012 x strength (Frost's strike is a real flash). */
  private pulse(cell: { x: number; y: number }, dx: number, dy: number, strength: number, intensity = 0.012 * strength): void {
    const pulse = this.pulses.reduce((old, p) => p.age > old.age ? p : old);
    const point = this.view.board!.cell(cell.x, cell.y);
    pulse.light.position.set(point.x + dx, this.view.groundAt(point.x, point.z) + 0.5, point.z + dy);
    pulse.light.intensity = intensity; pulse.age = 0;
  }
  private projected(x: number, y: number): THREE.Vector3 {
    const a = this.engine.effectAnchor(0, 0), p = this.view.board!.cell(0, 0);
    const wx = p.x + (x - a.x) / a.tile, wz = p.z + (y - a.y) / a.tile;
    return new THREE.Vector3(wx, this.view.groundAt(wx, wz) + 0.05, wz).project(this.view.stage.camera);
  }
  /** Canvas → world for one bolt: Ember's path (caster to target hex, lifted 0.3 tile) is laid
   * between the two units' chests, its sideways axis turned to face the camera, 1 tile = 1 hex. */
  private boltSheet(m: { fromX: number; fromY: number; toX: number; toY: number }, tile: number): THREE.Matrix4 {
    const camera = this.view.stage.camera;
    const f = this.engine.effectAnchor(m.fromX, m.fromY), t = this.engine.effectAnchor(m.toX, m.toY);
    const chest = (x: number, y: number) => { const c = this.view.board!.cell(x, y); return new THREE.Vector3(c.x, this.view.groundAt(c.x, c.z) + 0.9, c.z); };
    const A = chest(m.fromX, m.fromY), B = chest(m.toX, m.toY);
    const fx = f.x, fy = f.y - tile * 0.3;
    let dx = t.x - f.x, dy = t.y - f.y;
    const len = Math.hypot(dx, dy);
    if (len < 1e-3) { dx = tile; dy = 0; B.copy(A).add(new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0)); }
    const n = Math.hypot(dx, dy), nx = -dy / n, ny = dx / n;
    const D = B.clone().sub(A), dir = D.clone().normalize();
    const view = camera.position.clone().sub(A.clone().add(B).multiplyScalar(0.5));
    view.addScaledVector(dir, -view.dot(dir));
    const w = new THREE.Vector3().crossVectors(view, dir).normalize();
    // keep Ember's handedness on screen (canvas y points down), so the art is never mirrored
    const a = A.clone().project(camera), b = B.clone().project(camera), s = A.clone().add(w).project(camera);
    if ((b.x - a.x) * -(s.y - a.y) - -(b.y - a.y) * (s.x - a.x) < 0) w.negate();
    w.divideScalar(tile);
    const det = dx * ny - nx * dy;
    const Lx = D.clone().multiplyScalar(ny).addScaledVector(w, -dy).divideScalar(det);
    const Ly = D.clone().multiplyScalar(-nx).addScaledVector(w, dx).divideScalar(det);
    const O = A.clone().addScaledVector(Lx, -fx).addScaledVector(Ly, -fy);
    const N = new THREE.Vector3().crossVectors(Lx, Ly).normalize();
    return new THREE.Matrix4().set(Lx.x, Ly.x, N.x, O.x, Lx.y, Ly.y, N.y, O.y, Lx.z, Ly.z, N.z, O.z, 0, 0, 0, 1);
  }
  /** Canvas → world for an effect on one hex: a sheet facing the camera, standing on the hex's
   * ground (or water) with the hex centre at its foot, canvas up = camera up, 1 tile = 1 hex.
   * `footPx`: Ember drew a unit's feet that far below its hex centre (UnitVisual.footY); effects
   * aimed at a unit's body put that point on the ground, where the 3D card's feet are. */
  private standingSheet(cell: { x: number; y: number }, tile: number, footPx = 0, shift = { dx: 0, dy: 0 }): THREE.Matrix4 {
    const camera = this.view.stage.camera, at = this.engine.effectAnchor(cell.x, cell.y), c = this.view.board!.cell(cell.x, cell.y);
    // `shift`: the effect is centred that many canvas px off the hex centre (a multi-hex body).
    // Canvas x/y are the board's x/z at 1 tile = 1 unit, so that point goes on the ground, not up the sheet.
    const gx = c.x + shift.dx / tile, gz = c.z + shift.dy / tile;
    const G = new THREE.Vector3(gx, (c.water ? c.waterY : this.view.groundAt(gx, gz)) + 0.05, gz);
    const R = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0).divideScalar(tile);
    const D = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 1).divideScalar(-tile);
    const N = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 2);
    const O = G.clone().addScaledVector(R, -(at.x + shift.dx)).addScaledVector(D, -(at.y + shift.dy + footPx));
    return new THREE.Matrix4().set(R.x, D.x, N.x, O.x, R.y, D.y, N.y, O.y, R.z, D.z, N.z, O.z, 0, 0, 0, 1);
  }
  /** Summon Familiar / Warp portals: Ember's drawPortalFx art, painted into a texture on the same
   * camera-facing sheet as the other standing effects, but inside the 3D scene and drawn the way
   * the mist sheets are (no depth test, before the cards): over the ground, under the units —
   * Ember's layer, so the familiar steps out in front of its portal. */
  private syncPortals(items: PortalItem[], shiftOf: (q: PortalItem) => { dx: number; dy: number } | undefined, tile: number): void {
    const eng = this.engine as unknown as Record<string, any>;
    const used = new Set<PortalItem>();
    for (const q of items) {
      if (q.x < 0 || q.y < 0 || q.x >= this.engine.cols || q.y >= this.engine.rows) continue;
      used.add(q);
      let sheet = this.portalSheets.get(q);
      if (!sheet) {
        const canvas = document.createElement("canvas");
        canvas.width = PORTAL_TEX_W; canvas.height = PORTAL_TEX_H;
        const texture = new THREE.CanvasTexture(canvas);
        texture.colorSpace = THREE.SRGBColorSpace;
        const gate = !!(q as unknown as { warp?: boolean }).warp;
        const material = new THREE.MeshBasicMaterial({ map: texture, transparent: true, depthTest: gate, depthWrite: false, blending: THREE.AdditiveBlending, toneMapped: false, fog: false, side: THREE.DoubleSide });
        const depth = gate ? { value: 0.5 } : undefined;
        if (depth) material.onBeforeCompile = (sh) => {
          sh.uniforms.uGateDepth = depth;
          sh.fragmentShader = `uniform float uGateDepth;\n${sh.fragmentShader}`.replace("#include <dithering_fragment>", "#include <dithering_fragment>\n  gl_FragDepth = uGateDepth;");
        };
        const mesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), material);
        mesh.matrixAutoUpdate = false;
        mesh.frustumCulled = false;
        mesh.renderOrder = gate ? WARP_GATE_RENDER_ORDER : PORTAL_RENDER_ORDER;
        this.view.stage.scene.add(mesh);
        sheet = { mesh, canvas, texture, depth };
        this.portalSheets.set(q, sheet);
      }
      // The box of Ember canvas px the art is painted in, around the portal's centre: wide enough
      // for Familiar Titã's three-hex circle, tall enough for the light column / Warp gate.
      const shift = shiftOf(q) ?? { dx: 0, dy: 0 };
      const at = this.engine.effectAnchor(q.x, q.y);
      const cx = at.x + shift.dx, cy = at.y + shift.dy;
      // a big Warp gate (wide party radius) stands taller than the familiar's circle: room for all of it
      const gateR = (q as unknown as { warp?: boolean; radius?: number }).warp ? tile * (0.9 + ((q as unknown as { radius?: number }).radius ?? 1) * 0.52) : 0;
      const halfW = Math.max(tile * 4.2, gateR * 1.4), up = Math.max(tile * 5, gateR * 2.9), down = tile * 2.6;
      const ctx = sheet.canvas.getContext("2d")!;
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, PORTAL_TEX_W, PORTAL_TEX_H);
      ctx.setTransform(PORTAL_TEX_W / (halfW * 2), 0, 0, PORTAL_TEX_H / (up + down), -(cx - halfW) * PORTAL_TEX_W / (halfW * 2), -(cy - up) * PORTAL_TEX_H / (up + down));
      for (const other of items) other.live = other === q;
      // Tática (2D, straight down) shows Ember's own gate: centred on its hex, its floor glow on
      // the floor, under the units, no depth test (a single-depth sheet lying on the floor tied
      // with it and broke apart). The 3D view stands it on its hex, depth-sorted, no floor halo.
      const flat = this.view.rig.flat, gateSheet = sheet.depth !== undefined;
      eng.warpFloorHalo = flat;
      try { eng.drawPortalFx(ctx, tile); } finally { for (const other of items) other.live = true; eng.warpFloorHalo = false; }
      if (gateSheet) {
        const material = sheet.mesh.material as THREE.MeshBasicMaterial;
        if (material.depthTest === flat) { material.depthTest = !flat; material.needsUpdate = true; }
        sheet.mesh.renderOrder = flat ? PORTAL_RENDER_ORDER : WARP_GATE_RENDER_ORDER;
      }
      sheet.texture.needsUpdate = true;
      // the unit plane, stretched over that box in canvas px, then the standing sheet (canvas px → world)
      const box = new THREE.Matrix4().makeTranslation(cx, cy + (down - up) / 2, 0).multiply(new THREE.Matrix4().makeScale(halfW * 2, -(up + down), 1));
      // Ember drew the gate (warp-v3 art, drawPortalFx) hanging 0.93 r below its hex centre; depth-
      // tested, the floor would cut that off, so the open gate's bottom edge stands on the hex
      const standing = this.standingSheet(q, tile, flat ? 0 : gateR * 0.93, shift);
      sheet.mesh.matrix.copy(standing).multiply(box);
      if (sheet.depth) {
        const c = this.view.board!.cell(q.x, q.y), gx = c.x + shift.dx / tile, gz = c.z + shift.dy / tile;
        sheet.depth.value = new THREE.Vector3(gx, this.view.groundAt(gx, gz), gz).project(this.view.stage.camera).z * 0.5 + 0.5;
        // the gate's body (drawPortalFx: centred 0.88 r over the hex, 1.42 r tall, art to 0.93 r below), for clicks
        sheet.outline = flat ? undefined : [[-0.5, -2.3], [0.5, -2.3], [0.5, 0.93], [-0.5, 0.93]].map(([u, v]) => new THREE.Vector3(cx + u * gateR, cy + v * gateR, 0).applyMatrix4(standing));
      }
      sheet.mesh.matrixWorldNeedsUpdate = true;
      sheet.mesh.visible = true;
    }
    for (const [q, sheet] of this.portalSheets) {
      if (used.has(q)) continue;
      sheet.mesh.visible = false;
      if (!q.live) { this.disposePortal(sheet); this.portalSheets.delete(q); }
    }
  }
  /** The Warp gate under a screen point (client px), if the pointer is on a gate's body: it
   * stands above its hex, so a click on it should mean that hex, not the floor behind it. */
  gateAt(clientX: number, clientY: number): { x: number; y: number } | null {
    const rect = this.view.stage.renderer.domElement.getBoundingClientRect(), camera = this.view.stage.camera;
    for (const [q, sheet] of this.portalSheets) {
      if (!sheet.mesh.visible || !sheet.outline) continue;
      const pts = sheet.outline.map(p => { const s = p.clone().project(camera); return { x: rect.left + (s.x + 1) / 2 * rect.width, y: rect.top + (1 - s.y) / 2 * rect.height }; });
      let inside = true;
      for (let i = 0; i < 4 && inside; i++) {
        const a = pts[i]!, b = pts[(i + 1) % 4]!;
        const cross = (b.x - a.x) * (clientY - a.y) - (b.y - a.y) * (clientX - a.x);
        const turn = (pts[1]!.x - pts[0]!.x) * (pts[2]!.y - pts[0]!.y) - (pts[1]!.y - pts[0]!.y) * (pts[2]!.x - pts[0]!.x);
        if (cross * turn < 0) inside = false;
      }
      if (inside) return { x: q.x, y: q.y };
    }
    return null;
  }
  private disposePortal(sheet: { mesh: THREE.Mesh; texture: THREE.Texture }): void {
    sheet.mesh.removeFromParent();
    sheet.mesh.geometry.dispose();
    (sheet.mesh.material as THREE.Material).dispose();
    sheet.texture.dispose();
  }
  sync(dt: number): void {
    if (!this.view.board) return;
    for (const pulse of this.pulses) { pulse.age += dt; pulse.light.intensity *= Math.exp(-dt * 6); if (pulse.age > 1) pulse.light.intensity = 0; }
    const width = this.artCanvas.clientWidth, height = this.artCanvas.clientHeight, dpr = devicePixelRatio || 1;
    const w = Math.max(1, Math.round(width * dpr)), h = Math.max(1, Math.round(height * dpr));
    if (this.artCanvas.width !== w || this.artCanvas.height !== h) { this.artCanvas.width = w; this.artCanvas.height = h; this.legacy.setSize(w, h); }
    this.effects.resize(width, height, dpr);
    this.legacy.clear();
    const a = this.engine.effectAnchor(0, 0), p = this.view.board.cell(0, 0), camera = this.view.stage.camera;
    const world = new THREE.Matrix4().set(1 / a.tile, 0, 0, p.x - a.x / a.tile, 0, 0, 1, 0.05, 0, 1 / a.tile, 0, p.z - a.y / a.tile, 0, 0, 0, 1);
    const projection = new THREE.Matrix4().multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse).multiply(world);
    this.legacy.setProjection(projection.elements);
    // The engine still draws its actual art. Arrows already have their own camera-facing 3D
    // pass, so hide just those canvas shots for this synchronous draw and restore immediately.
    const e = this.engine as unknown as { missileFx: Array<{ live: boolean; kind: string; fromX: number; fromY: number; toX: number; toY: number }>; bladeFx: Array<{ live: boolean; x: number; y: number }>; drawBladeFx: (ctx: unknown, tile: number) => void; drawMissileFx: (ctx: unknown, tile: number) => void };
    // The mage's basic bolt also flies in that 3D pass (battleProjectiles3d.ts).
    const arrows = e.missileFx.filter(m => m.live && (m.kind === "longShot" || m.kind === "arcaneBolt"));
    // Spell bolts (enemy Magic Missile, Phantasmal Force, venom…) fly hex to hex at chest height;
    // on the flat ground sheet they landed off their caster and target, so each gets its own sheet.
    const bolts = e.missileFx.filter(m => m.live && !arrows.includes(m));
    // Melee slashes (Double Strike's cross, Cleave's arc, Sweep's ring…) are drawn on the flat
    // canvas sheet at height 0; on raised ground that put them visibly off the struck hex. They
    // are left out of the shared pass and each drawn below on its own hex's ground height.
    const blades = e.bladeFx.filter(b => b.live);
    // Ember's hex-anchored spell art (lightning, bursts, holy light, hit sparks, level-up stars,
    // portals, Provoke) was painted for a 2D view: "up" on the canvas is up in the air. On the
    // flat ground sheet that laid them along the floor at height 0, off their hex, so each is
    // drawn on its own camera-facing sheet standing on its hex instead (Ember's 2D look, in place).
    const eng = this.engine as unknown as Record<string, any>, tile = a.tile;
    const potionCard = (fx: { unitId?: string }) => fx.unitId ? this.cardAt?.(fx.unitId) ?? null : null;
    const unitCell = (id?: string) => { const u = id ? this.engine.units.find(n => n.id === id) : undefined; return u ? { x: u.x, y: u.y } : null; };
    const standing = ([
      [eng.lightningFx, (l: any) => l, () => eng.drawLightningFx(this.legacy, tile), false],
      [eng.fireballBurstFx, (b: any) => b, () => eng.drawFireballBurstFx(this.legacy, tile), false],
      // a potion on a unit with a card is drawn on its own, centred on the card (below)
      [eng.holyFx, (fx: any) => fx.kind === "potion" && potionCard(fx) ? null : unitCell(fx.unitId) ?? fx, () => eng.drawHolyFx(this.legacy, tile), false],
      [eng.particles, (q: any) => ({ x: Math.round(q.x), y: Math.round(q.y) }), () => eng.drawParticleFx(this.legacy, tile, true), true],
      [eng.levelUpFx, (s: any) => unitCell(s.unitId), () => eng.drawLevelUpFx(this.legacy, tile, true), true],
      [eng.portalFx, (q: any) => q, () => eng.drawPortalFx(this.legacy, tile), false],
    ] as Array<[Array<{ live: boolean }>, (fx: any) => { x: number; y: number } | null, () => void, boolean]>)
      .map(([list, cell, draw, onUnit]) => ({ items: list.filter(fx => fx.live), cell, draw, onUnit }));
    // Summon Familiar's circle centres on the whole body that steps out (drawPortalFx): for a
    // multi-hex familiar (Familiar Titã) that is off its anchor hex, which on the standing sheet
    // pushed the circle down off the familiar. Its sheet stands where the familiar's 3D card has
    // its feet instead: the centre of the body's front row (engine.footprintCentroidWorld).
    const portals = standing[standing.length - 1]!.items;
    const portalShift = (q: { x: number; y: number; body: { dx: number; dy: number }[] | null }) => {
      if (!q.body) return undefined;
      const centre = eng.hexCenter(q.x, q.y) as { cx: number; cy: number };
      const cells = footprintFrontRow({ x: q.x, y: q.y, footprintOffsets: q.body }).map(c => eng.hexCenter(c.x, c.y) as { cx: number; cy: number });
      return { dx: cells.reduce((s, c) => s + c.cx, 0) / cells.length - centre.cx, dy: cells.reduce((s, c) => s + c.cy, 0) / cells.length - centre.cy };
    };
    this.syncPortals(portals as unknown as PortalItem[], portalShift, tile);
    // hit sparks, level-up stars and Provoke sit on a unit's body: anchor them at its Ember feet
    const footAt = (cell: { x: number; y: number }) => { const u = this.engine.units.find(n => n.alive && n.x === cell.x && n.y === cell.y); return u ? eng.unitVisual(u, tile).footY as number : tile * Math.sqrt(3) * 0.42; };
    // Turn Undead and Provoke keep plain lists (no live flag): swapped out while they're drawn alone.
    const provoke: Array<{ unitId: string }> = eng.provokeFx, turnUndead: Array<{ tiles: { x: number; y: number }[] }> = eng.turnUndeadFx;
    for (const shot of [...arrows, ...bolts]) shot.live = false;
    for (const blade of blades) blade.live = false;
    for (const pool of standing) for (const fx of pool.items) (fx as { live: boolean }).live = false;
    eng.provokeFx = []; eng.turnUndeadFx = [];
    // Poison / disease (drawStatusFx): Ember's haze and motes rise from a unit's feet to its head.
    // On the flat ground sheet (height 0) they lay along the floor, off the unit, so they are
    // left out of the shared pass and drawn below on each unit's own standing sheet.
    const statusUnits = this.engine.units.filter(u => (u.poisoned || u.diseased) && u.fade > 0 && !eng.unitHidden(u));
    eng.drawStatusFx = () => undefined;
    try { this.engine.renderUnitsAndOverlays(this.legacy, 1280, 800, undefined, true, true, true, true, true, false, true, true); }
    finally {
      for (const shot of [...arrows, ...bolts]) shot.live = true; for (const blade of blades) blade.live = true;
      for (const pool of standing) for (const fx of pool.items) (fx as { live: boolean }).live = true;
      eng.provokeFx = provoke; eng.turnUndeadFx = turnUndead;
      delete eng.drawStatusFx;
    }
    const viewProj = new THREE.Matrix4().multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse);
    for (const pool of standing) {
      // effects on the same hex share one sheet and one draw
      const byCell = new Map<string, { cell: { x: number; y: number }; fx: object[] }>();
      for (const fx of pool.items) {
        const cell = pool.cell(fx);
        if (!cell || cell.x < 0 || cell.y < 0 || cell.x >= this.engine.cols || cell.y >= this.engine.rows) continue;
        const key = `${cell.x},${cell.y}`, group = byCell.get(key);
        if (group) group.fx.push(fx); else byCell.set(key, { cell: { x: cell.x, y: cell.y }, fx: [fx] });
      }
      if (pool.items === portals) continue; // drawn in the 3D scene, under the units (syncPortals)
      for (const { cell, fx } of byCell.values()) {
        this.legacy.setProjection(viewProj.clone().multiply(this.standingSheet(cell, tile, pool.onUnit ? footAt(cell) : 0)).elements);
        for (const other of pool.items) (other as { live: boolean }).live = fx.includes(other);
        try { pool.draw(); } finally { for (const other of pool.items) (other as { live: boolean }).live = true; }
      }
    }
    // Potion bursts: Ember drew them around a small sprite's chest (half a tile over the hex
    // centre), which sat low and small on the tall 3D card. Each goes on a camera-facing sheet
    // centred on the card's body, its glow sized to the card (POTION_GLOW_OF_CARD).
    const holy = standing[2]!.items as unknown as Array<{ live: boolean; kind: string; unitId?: string }>;
    for (const fx of holy) {
      if (fx.kind !== "potion") continue;
      const card = potionCard(fx), u = card ? this.engine.units.find(n => n.id === fx.unitId) : undefined;
      if (!card || !u) continue;
      const centre = eng.hexCenter(u.drawX, u.drawY) as { cx: number; cy: number };
      const size = card.height * POTION_GLOW_OF_CARD / 0.95 / tile;
      const R = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0).multiplyScalar(size);
      const D = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 1).multiplyScalar(-size);
      const N = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 2);
      const O = card.centre.clone().addScaledVector(R, -centre.cx).addScaledVector(D, -(centre.cy - tile * 0.5));
      this.legacy.setProjection(viewProj.clone().multiply(new THREE.Matrix4().set(R.x, D.x, N.x, O.x, R.y, D.y, N.y, O.y, R.z, D.z, N.z, O.z, 0, 0, 0, 1)).elements);
      for (const other of holy) other.live = other === fx;
      try { eng.drawHolyFx(this.legacy, tile); } finally { for (const other of holy) other.live = true; }
    }
    // each poisoned / diseased unit: its feet (where Ember drew them) on the ground under its card
    for (const u of statusUnits) {
      if (u.x < 0 || u.y < 0 || u.x >= this.engine.cols || u.y >= this.engine.rows) continue;
      const at = this.engine.effectAnchor(u.x, u.y), px = eng.unitPixel(u) as { cx: number; cy: number };
      const v = eng.unitVisual(u, tile) as { sway: number; bob: number; footY: number; scaleX: number; scaleY: number };
      this.legacy.setProjection(viewProj.clone().multiply(this.standingSheet(u, tile, v.footY, { dx: px.cx - at.x, dy: px.cy - at.y })).elements);
      const ctx = this.legacy as unknown as Record<string, any>, cell = tile * Math.sqrt(3);
      ctx.save();
      ctx.globalAlpha = u.fade;
      ctx.translate(px.cx + v.sway, px.cy + v.footY + v.bob);
      ctx.scale(v.scaleX, v.scaleY);
      try { eng.drawStatusFx(ctx, u, cell * 1.11 * 1.2, cell * 1.42 * 1.2); } finally { ctx.restore(); }
    }
    try {
      for (const fx of provoke) {
        const cell = unitCell(fx.unitId); if (!cell) continue;
        this.legacy.setProjection(viewProj.clone().multiply(this.standingSheet(cell, tile, footAt(cell))).elements);
        eng.provokeFx = [fx]; eng.drawProvokeFx(this.legacy, tile);
      }
      // Turn Undead marks the struck hexes on the floor: a ground sheet at their average height.
      for (const fx of turnUndead) {
        const heights = fx.tiles.filter(c => c.x >= 0 && c.y >= 0 && c.x < this.engine.cols && c.y < this.engine.rows).map(c => { const g = this.view.board!.cell(c.x, c.y); return g.water ? g.waterY : this.view.groundAt(g.x, g.z); });
        const lift = (heights.length ? heights.reduce((s, h) => s + h, 0) / heights.length : 0) + 0.05;
        const sheet = new THREE.Matrix4().set(1 / tile, 0, 0, p.x - a.x / tile, 0, 0, 1, lift, 0, 1 / tile, 0, p.z - a.y / tile, 0, 0, 0, 1);
        this.legacy.setProjection(viewProj.clone().multiply(sheet).elements);
        eng.turnUndeadFx = [fx]; eng.drawTurnUndeadFx(this.legacy, tile);
      }
    } finally { eng.provokeFx = provoke; eng.turnUndeadFx = turnUndead; }
    for (const bolt of bolts) {
      this.legacy.setProjection(new THREE.Matrix4().multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse).multiply(this.boltSheet(bolt, a.tile)).elements);
      for (const other of bolts) other.live = other === bolt;
      try { e.drawMissileFx(this.legacy, a.tile); } finally { for (const other of bolts) other.live = true; }
    }
    for (const blade of blades) {
      if (blade.x < 0 || blade.y < 0 || blade.x >= this.engine.cols || blade.y >= this.engine.rows) continue;
      const c = this.view.board.cell(blade.x, blade.y);
      const lift = (c.water ? c.waterY : this.view.groundAt(c.x, c.z)) + 0.05;
      const sheet = new THREE.Matrix4().set(1 / a.tile, 0, 0, p.x - a.x / a.tile, 0, 0, 1, lift, 0, 1 / a.tile, 0, p.z - a.y / a.tile, 0, 0, 0, 1);
      this.legacy.setProjection(new THREE.Matrix4().multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse).multiply(sheet).elements);
      for (const other of blades) other.live = other === blade;
      try { e.drawBladeFx(this.legacy, a.tile); } finally { for (const other of blades) other.live = true; }
    }
    this.legacy.setProjection(projection.elements);
    const anchor = (x: number, y: number) => {
      const original = this.engine.effectAnchor(x, y), s = this.projected(original.x, original.y);
      const right = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0);
      const cell = this.view.board!.cell(x, y);
      const center = new THREE.Vector3(cell.x, this.view.groundAt(cell.x, cell.z) + 0.05, cell.z);
      const edge = center.clone().add(right).project(camera);
      return { ...original, x: (s.x + 1) * width / 2, y: (1 - s.y) * height / 2, tile: Math.hypot((edge.x - s.x) * width / 2, (edge.y - s.y) * height / 2) };
    };
    for (const request of this.engine.frostVfxRequests.splice(0)) this.effects.spawnFrost({ ...request, onImpact: cell => this.pulse(cell, 0, 0, 0, FROST_FLASH) });
    const live = new Set(this.engine.iceStormZones.map(zone => zone.createdAt));
    for (const [key, id] of this.blizzards) if (!live.has(key)) { this.effects.removeBlizzard(id); this.blizzards.delete(key); }
    for (const zone of this.engine.iceStormZones) if (!this.blizzards.has(zone.createdAt)) {
      const cells = [...zone.cells].map(key => { const [x, y] = key.split(",").map(Number); return { x, y }; }).filter(c => !this.engine.fogged || this.engine.visible(c.x, c.y));
      if (cells.length) this.blizzards.set(zone.createdAt, this.effects.spawnBlizzard({ cells, duration: 9, seed: Math.floor(zone.createdAt * 1000), particleDensity: 1.3, groundScaleY: this.view.rig.flat ? 1 : Math.abs(Math.sin(this.view.rig.pitch)), onImpact: (cell, dx, dy, strength) => this.pulse(cell, dx, dy, strength) }));
    }
    const beam = this.engine.webShotBeam();
    if (beam) {
      if (this.webShot === null) this.webShot = this.effects.spawnEffect("webShot", 0, 0, { radiusTiles: 0.01 });
      const s = this.projected(beam.x, beam.y), end = this.projected(beam.x + Math.cos(beam.angle) * beam.length / 2, beam.y + Math.sin(beam.angle) * beam.length / 2);
      const dx = (end.x - s.x) * width / 2, dy = -(end.y - s.y) * height / 2;
      this.effects.updateOverride(this.webShot, { x: (s.x + 1) * width / 2, y: (1 - s.y) * height / 2, worldX: beam.worldX, worldY: beam.worldY, tile: beam.tile, halfLengthPx: Math.max(1, Math.hypot(dx, dy)), halfWidthPx: anchor(0, 0).tile * 0.4, rotation: Math.atan2(dy, dx) });
    } else if (this.webShot !== null) { this.effects.removeEffect(this.webShot); this.webShot = null; }
    for (const request of this.engine.elementalFxRequests.splice(0)) if (!this.engine.fogged || this.engine.visible(request.x, request.y)) this.effects.spawnEffect(request.kind, request.x, request.y, { duration: request.duration });
    if (this.effects.hasEffects()) { this.fxCanvas.style.display = "block"; this.effects.render(this.view.stage.renderer.domElement, dt, anchor); }
    else this.fxCanvas.style.display = "none";
    // An Ice Storm stands in the board's layers: the effects canvas is laid over the frame by the
    // stage, hidden wherever a unit or prop is nearer the camera than the storm's front row
    // (half a hex of margin, so a unit inside the storm is still covered by it).
    const stormDist = this.blizzards.size && this.effects.hasEffects() ? this.stormFront() : null;
    if (stormDist !== null) {
      this.fxCanvas.style.visibility = "hidden";
      this.fxTexture.needsUpdate = true;
      this.view.stage.setDepthOverlay(this.fxTexture, stormDist);
    } else {
      this.fxCanvas.style.visibility = "";
      this.view.stage.setDepthOverlay(null);
    }
  }
  /** Distance from the camera of the nearest Ice Storm hex, less half a hex; null if none is on the board. */
  private stormFront(): number | null {
    const camera = this.view.stage.camera, p = new THREE.Vector3();
    let near = Infinity;
    for (const zone of this.engine.iceStormZones) for (const key of zone.cells) {
      const [x, y] = key.split(",").map(Number);
      if (x < 0 || y < 0 || x >= this.engine.cols || y >= this.engine.rows) continue;
      const c = this.view.board!.cell(x, y);
      near = Math.min(near, -p.set(c.x, this.view.groundAt(c.x, c.z), c.z).applyMatrix4(camera.matrixWorldInverse).z);
    }
    return Number.isFinite(near) ? near - 0.5 : null;
  }
  dispose(): void { this.view.stage.setDepthOverlay(null); this.fxTexture.dispose();for (const sheet of this.portalSheets.values()) this.disposePortal(sheet); this.portalSheets.clear(); this.effects.dispose(); this.legacy.dispose(); for (const pulse of this.pulses) pulse.light.removeFromParent(); }
}
