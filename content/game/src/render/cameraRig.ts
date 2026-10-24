// Tactical diorama camera, designed for 3D from day one (no "mode" bolted onto a 2D view).
//
// DO NOT MESS WITH CONTROLS — Ember's protected map-editor rule, carried over as-is:
// the left button arms panning only after a 0.5 s hold, then shows the grabbing hand and pans
// while dragged; every corner of the map must be reachable. A short left click is a tool click.
// New for 3D: right-drag turns and tilts, the wheel zooms.
//
// Two modes (user decision): the campaign is LOCKED to the normal view (turned 30°, tilted 20°),
// zoom and pan only. The editor is FREE: a full 360° in every direction (turn, and tilt over the
// top and under the ground) to test which angles work best. Zoom and pan work in both.
// Units and decoration cards turn to face the camera every frame (`facingYaw`), from any side.

import * as THREE from 'three';

export interface RigBounds { x0: number; x1: number; z0: number; z1: number }
export type RigMode = 'locked' | 'free';

const HOLD_MS = 500;
const CLICK_SLOP = 5;
/** The normal view's turn: every board opens here, and the campaign stays here. */
export const START_YAW = THREE.MathUtils.degToRad(-30);
/** The normal view's tilt (angle above the horizon). */
export const START_PITCH = THREE.MathUtils.degToRad(20);

/** The normal (perspective) lens. */
const NORMAL_FOV = 30, NORMAL_NEAR = 0.5, NORMAL_FAR = 600;
/** The old-school 2D view (Ember's original battle camera: straight overhead, no perspective)
 * uses a very long lens from far away, so it stays a perspective camera for picking and panning. */
const FLAT_FOV = 1;
/** The 3D view's farthest zoom as a share of Tática's (user's reference shot, 2026-10-13). */
const MAX_3D_WIDTH_SHARE = 42 / 58;
/** In the 2D view each world unit of depth (z) raises a flat card this much, so nearer rows are
 * drawn over farther ones, like a 2D game's painter order. */
export const FLAT_ROW_STEP = 0.002;

const Y_AXIS = new THREE.Vector3(0, 1, 0), X_AXIS = new THREE.Vector3(1, 0, 0);
const qYaw = new THREE.Quaternion(), qPitch = new THREE.Quaternion(), back = new THREE.Vector3();
const probeRay = new THREE.Raycaster(), probeNdc = new THREE.Vector2();
/** The 3D view's pan reach, as screen points (−1..1) that must stay over the board: the board's
 * edge just shows: a 5% strip past it at each side of the screen, no further (user's call:
 * "see the edges of the map, that's the limit"). Lower = more panning room. */
const EDGE_PROBE_X = 0.9, EDGE_PROBE_Y = 0.9;

export class CameraRig {
  readonly target = new THREE.Vector3();
  /** 'locked' = campaign (normal view only); 'free' = editor (any angle). */
  mode: RigMode = 'locked';
  yaw = START_YAW;
  /** Angle above the horizon, radians. In free mode it wraps all the way round (over the top, under the ground). */
  pitch = START_PITCH;
  dist = 50;
  /** Old-school 2D view: straight overhead, no perspective, no turning (zoom and pan still work).
   * The turn and tilt underneath are kept, so leaving it returns to the same angle. */
  flat = false;
  minDist = 6;
  maxDist = 150;
  /** Gameplay limits (battles; null = free, as in the editor). The farthest zoom shows at most
   * this many world units across the screen (the user's Tática reference: ~58, about 33 hex
   * columns), and panning keeps the board's edge from coming further in than that reference:
   * ~14% of the screen width at the sides, ~23% of its height top and bottom (Tática: twice
   * that up and down). */
  maxViewWidth: number | null = null;
  panBounds: RigBounds | null = null;
  private holdTimer = 0;
  private panning = false;
  private turning = false;
  private down: { x: number; y: number; button: number } | null = null;
  private last = { x: 0, y: 0 };
  private panRight = new THREE.Vector3();
  private panUp = new THREE.Vector3();

  /** While a cinematic shot owns the camera (cinematicCamera.ts) the player's camera input
   * doesn't move it: any attempt calls `onCameraInput` instead (the director skips the shot and
   * restores the player's view). Clicks still reach the game. */
  inputLocked = false;
  onCameraInput: () => void = () => {};

  /** A short left click (not a pan): the tool acts on it. */
  onClick: (ev: PointerEvent) => void = () => {};
  /** When true, a left drag that starts moving before the 0.5 s hold arms the pan is a tool stroke
   * (painting) instead. Holding still for 0.5 s first still pans, exactly as before. */
  strokeEnabled = false;
  onStroke: (ev: PointerEvent, phase: 'start' | 'move' | 'end') => void = () => {};
  private stroking = false;

  constructor(private readonly camera: THREE.PerspectiveCamera, private readonly el: HTMLElement) {
    el.addEventListener('contextmenu', e => e.preventDefault());
    el.addEventListener('pointerdown', e => this.pointerDown(e));
    el.addEventListener('pointermove', e => this.pointerMove(e));
    el.addEventListener('pointerup', e => this.pointerUp(e));
    el.addEventListener('pointercancel', () => this.endPan());
    window.addEventListener('keydown', this.onKey);
    el.addEventListener('wheel', e => { e.preventDefault(); if (this.inputLocked) { this.onCameraInput(); return; } this.dist = THREE.MathUtils.clamp(this.dist * Math.pow(1.12, Math.sign(e.deltaY)), this.minDist, this.farthest); this.clampTarget(); this.apply(); }, { passive: false });
    this.apply();
  }

  /** The farthest zoom allowed right now (maxViewWidth depends on the screen's aspect). */
  get farthest(): number {
    if (this.maxViewWidth == null) return this.maxDist;
    const tan = Math.tan(THREE.MathUtils.degToRad(NORMAL_FOV / 2));
    // the 3D view stops closer than Tática: the user's 3D reference shows ~42 units across, not 58
    const width = this.flat ? this.maxViewWidth : this.maxViewWidth * MAX_3D_WIDTH_SHARE;
    return Math.min(this.maxDist, width / (2 * tan * Math.max(0.25, this.camera.aspect)));
  }

  /** The 3D view's edge limit. The aim point (screen centre) always stays on the board, and
   * the board's edge may come no further in than probe points 5% in from the screen's left,
   * right and bottom sides (EDGE_PROBE_X/Y) traced onto the ground. The top probe is left out:
   * at the low tilt it looks far past the board, which locked panning. A board narrower than
   * the probes on an axis just keeps the aim point over it there. Clamped per world axis, so a
   * pan into an edge slides along it, and a view pushed past the edge (zoom) comes straight back. */
  private clampTarget3D(b: RigBounds): void {
    this.apply();
    this.camera.updateMatrixWorld(true);
    const t0 = this.target;
    let minX = 0, maxX = 0, minZ = 0, maxZ = 0;
    for (const [nx, ny] of [[-EDGE_PROBE_X, 0], [EDGE_PROBE_X, 0], [0, -EDGE_PROBE_Y]]) {
      probeRay.setFromCamera(probeNdc.set(nx, ny), this.camera);
      const d = probeRay.ray.direction, o = probeRay.ray.origin;
      if (d.y > -1e-4) continue; // at or above the horizon: never meets the ground
      const t = (t0.y - o.y) / d.y, ox = o.x + d.x * t - t0.x, oz = o.z + d.z * t - t0.z;
      minX = Math.min(minX, ox); maxX = Math.max(maxX, ox); minZ = Math.min(minZ, oz); maxZ = Math.max(maxZ, oz);
    }
    const span = (lo: number, hi: number, v: number) => lo <= hi ? THREE.MathUtils.clamp(v, lo, hi) : v;
    t0.x = THREE.MathUtils.clamp(span(b.x0 - minX, b.x1 - maxX, t0.x), b.x0, b.x1);
    t0.z = THREE.MathUtils.clamp(span(b.z0 - minZ, b.z1 - maxZ, t0.z), b.z0, b.z1);
  }

  /** Keep the aim point where the board's edge stays within the reference strip (panBounds). */
  clampTarget(): void {
    const b = this.panBounds;
    if (!b) return;
    if (!this.flat) { this.clampTarget3D(b); return; }
    const halfH = this.dist * Math.tan(THREE.MathUtils.degToRad(NORMAL_FOV / 2)), halfW = halfH * Math.max(0.25, this.camera.aspect);
    // a tilted camera sees the ground stretched in depth by 1/sin(tilt) (straight down = 1)
    const depth = this.flat ? 1 : 1 / Math.max(0.2, Math.abs(Math.sin(this.pitch)));
    // Tática pans twice as far up and down as the normal view (user's call)
    const ix = halfW * (1 - 2 * 0.14), iz = halfH * depth * (1 - 2 * 0.23 * (this.flat ? 2 : 1));
    // a board smaller than the view still pans edge to edge (aim anywhere over it), never locks
    const span = (lo: number, hi: number, inset: number, v: number) => lo + inset > hi - inset ? THREE.MathUtils.clamp(v, lo, hi) : THREE.MathUtils.clamp(v, lo + inset, hi - inset);
    this.target.x = span(b.x0, b.x1, ix, this.target.x);
    this.target.z = span(b.z0, b.z1, iz, this.target.z);
  }

  /** Frame a board: centre on it, far enough back to see it whole. */
  frame(b: RigBounds, groundY: number): void {
    const w = b.x1 - b.x0, d = b.z1 - b.z0;
    this.target.set((b.x0 + b.x1) / 2, groundY, (b.z0 + b.z1) / 2);
    // opens on the normal view: turned 30°, low 20° diorama tilt
    this.yaw = START_YAW;
    this.pitch = START_PITCH;
    this.maxDist = Math.max(40, Math.max(w, d) * 2.2);
    this.dist = Math.min(this.maxDist, Math.max(w / (2 * Math.tan(THREE.MathUtils.degToRad(this.camera.fov / 2)) * this.camera.aspect), d * 1.35) * 1.05);
    this.apply();
  }

  /** Back to the normal view's angle (turned 30°, tilted 20°); zoom and pan stay where they are. */
  resetAngle(): void {
    this.yaw = START_YAW;
    this.pitch = START_PITCH;
    this.apply();
  }

  /** The turn cards should face: past straight-down the camera is on the far side, so it flips. */
  get facingYaw(): number {
    if (this.flat) return 0;
    return Math.cos(this.pitch) < 0 ? this.yaw + Math.PI : this.yaw;
  }

  /** Camera distance actually used: the 2D view's long lens stands back so the same `dist` frames the same area. */
  private get viewDist(): number {
    return this.flat ? this.dist * Math.tan(THREE.MathUtils.degToRad(NORMAL_FOV / 2)) / Math.tan(THREE.MathUtils.degToRad(FLAT_FOV / 2)) : this.dist;
  }

  apply(): void {
    if (this.mode === 'locked') { this.yaw = START_YAW; this.pitch = START_PITCH; }
    const wrap = (a: number) => THREE.MathUtils.euclideanModulo(a + Math.PI, Math.PI * 2) - Math.PI;
    this.yaw = wrap(this.yaw);
    this.pitch = wrap(this.pitch);
    // Orientation from turn then tilt (no lookAt), so going over the top or under the ground never flips or locks up.
    qYaw.setFromAxisAngle(Y_AXIS, this.flat ? 0 : this.yaw);
    qPitch.setFromAxisAngle(X_AXIS, this.flat ? -Math.PI / 2 : -this.pitch);
    this.camera.quaternion.copy(qYaw).multiply(qPitch);
    const d = this.viewDist;
    const [fov, near, far] = this.flat ? [FLAT_FOV, Math.max(NORMAL_NEAR, d - 200), d + 200] : [NORMAL_FOV, NORMAL_NEAR, NORMAL_FAR];
    if (this.camera.fov !== fov || this.camera.near !== near || this.camera.far !== far) {
      Object.assign(this.camera, { fov, near, far });
      this.camera.updateProjectionMatrix();
    }
    back.set(0, 0, d).applyQuaternion(this.camera.quaternion);
    this.camera.position.copy(this.target).add(back);
  }

  private pointerDown(e: PointerEvent): void {
    this.el.setPointerCapture(e.pointerId);
    this.down = { x: e.clientX, y: e.clientY, button: e.button };
    this.last = { x: e.clientX, y: e.clientY };
    if (e.button === 0) {
      clearTimeout(this.holdTimer);
      this.holdTimer = window.setTimeout(() => {
        if (this.inputLocked) { this.onCameraInput(); return; }
        this.panning = true; document.documentElement.classList.add('engine2-grabbing');
      }, HOLD_MS);
    } else if (e.button === 2 && this.mode === 'free' && !this.flat) {
      if (this.inputLocked) this.onCameraInput(); else this.turning = true;
    }
  }

  private pointerMove(e: PointerEvent): void {
    const dx = e.clientX - this.last.x, dy = e.clientY - this.last.y;
    this.last = { x: e.clientX, y: e.clientY };
    const d0 = this.down;
    if (!this.stroking && !this.panning && this.strokeEnabled && d0 && d0.button === 0 && Math.hypot(e.clientX - d0.x, e.clientY - d0.y) > CLICK_SLOP) {
      clearTimeout(this.holdTimer);
      this.stroking = true;
      this.onStroke(e, 'start');
      return;
    }
    if (this.stroking) { this.onStroke(e, 'move'); return; }
    if (this.panning) {
      this.panBy(dx, dy);
    } else if (this.turning) {
      // upside down (past straight-down), turning reverses so the board still follows the mouse
      this.yaw -= dx * 0.006 * Math.sign(Math.cos(this.pitch) || 1);
      this.pitch += dy * 0.005;
      this.apply();
    }
  }

  private pointerUp(e: PointerEvent): void {
    const d = this.down;
    clearTimeout(this.holdTimer);
    if (this.stroking) { this.stroking = false; this.down = null; this.onStroke(e, 'end'); return; }
    const wasPanning = this.panning;
    this.endPan();
    this.turning = false;
    this.down = null;
    if (d && d.button === 0 && !wasPanning && Math.hypot(e.clientX - d.x, e.clientY - d.y) <= CLICK_SLOP) this.onClick(e);
  }

  private endPan(): void {
    this.panning = false;
    document.documentElement.classList.remove('engine2-grabbing');
  }

  private readonly onKey = (e: KeyboardEvent) => this.keyPan(e);

  /** Drop the window-level key listener (the canvas listeners go away with the canvas). */
  dispose(): void { window.removeEventListener('keydown', this.onKey); clearTimeout(this.holdTimer); }

  private keyPan(e: KeyboardEvent): void {
    if (e.altKey || e.ctrlKey || e.metaKey) return;
    if (e.target instanceof HTMLElement && e.target.closest('input, textarea, select, [contenteditable="true"]')) return;
    const directions: Record<string, [number, number]> = {
      // Move the view in the pressed direction; the board scrolls the opposite way.
      ArrowLeft: [1, 0], ArrowRight: [-1, 0], ArrowUp: [0, 1], ArrowDown: [0, -1],
    };
    const direction = directions[e.key];
    if (!direction) return;
    e.preventDefault();
    if (this.inputLocked) { this.onCameraInput(); return; }
    const step = e.shiftKey ? 120 : 48;
    this.panBy(direction[0] * step, direction[1] * step);
  }

  /** Apply screen-space drag distance to the camera target; keyboard panning uses the same movement. */
  private panBy(dx: number, dy: number): void {
    const scale = (2 * this.viewDist * Math.tan(THREE.MathUtils.degToRad(this.camera.fov / 2))) / this.el.clientHeight;
    this.camera.updateMatrixWorld(true);
    this.panRight.setFromMatrixColumn(this.camera.matrixWorld, 0).setY(0).normalize();
    this.panUp.setFromMatrixColumn(this.camera.matrixWorld, 1).setY(0);
    const verticalScale = this.panUp.lengthSq();
    const moveX = -this.panRight.x * dx * scale + (verticalScale > 1e-6 ? this.panUp.x * dy * scale / verticalScale : 0);
    const moveZ = -this.panRight.z * dx * scale + (verticalScale > 1e-6 ? this.panUp.z * dy * scale / verticalScale : 0);
    // Do not clamp to the board AABB: that made screen-axis panning stop or skew at angled edges.
    this.target.x += moveX;
    this.target.z += moveZ;
    this.clampTarget();
    this.apply();
  }
}
