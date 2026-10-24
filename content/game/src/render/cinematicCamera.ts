// Cinematic camera director (Ember Ashes — Cinematic Combat Camera System, stage 1: foundation).
//
// Works alongside the player's camera (CameraRig), never replaces it:
//  - capture: before a shot, the player's exact view is saved (target, turn, tilt, distance);
//  - shot: an eased move to a framing computed from the subjects' world positions, the current
//    view direction, the field of view and the viewport, checked against the terrain so the
//    camera never ends up inside it; an optional impact shake, computed fresh each frame from
//    the shot's clock (never accumulated);
//  - restore: an eased return to the saved view, written back exactly at the end — no drift;
//  - priority: a shot only replaces a running one of lower priority (or one already returning);
//  - safety: every shot has a hard maximum duration; skip, the player touching the camera, a
//    lost subject or disposal all restore the saved view at once and give control back.
// The director only reads the battle; it never changes timing, damage or animation.

import * as THREE from 'three';
import type { CameraRig } from './cameraRig';

export type CinematicMode = 'tactical' | 'cinematic' | 'full';
export interface CinematicSettings { mode: CinematicMode; reduceShake: boolean; intensity: number }

const SETTINGS_KEY = 'engine2:cinematic-camera';
export function getCinematicSettings(): CinematicSettings {
  const fallback: CinematicSettings = { mode: 'cinematic', reduceShake: false, intensity: 1 };
  try { return { ...fallback, ...JSON.parse(localStorage.getItem(SETTINGS_KEY) ?? '{}') }; } catch { return fallback; }
}
export function setCinematicSettings(next: Partial<CinematicSettings>): CinematicSettings {
  const merged = { ...getCinematicSettings(), ...next };
  try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(merged)); } catch { /* applies to this page only */ }
  return merged;
}

export enum ShotPriority { Low = 1, Medium = 2, High = 3, Boss = 4 }

/** One camera shot. Times in seconds; angles in degrees, relative to the player's view. */
export interface Shot {
  name: string;
  priority: ShotPriority;
  /** World points that must stay in frame (a unit's feet and head, an impact, an area's corners). */
  subjects: () => THREE.Vector3[] | null;
  /** Seconds to move in, hold, and return. */
  inTime: number; hold: number; outTime: number;
  /** Share of the way from the player's distance to the tight framing distance (0 none, 1 full). */
  push: number;
  /** Turn and tilt offsets from the player's view at full push. */
  yaw?: number; pitch?: number;
  /** Impact shake: amplitude (world units), when (s after the shot starts), how long. */
  shake?: { amp: number; at: number; dur: number };
  /** Extra room around the subjects (share of their radius). */
  margin?: number;
}

interface Pose { target: THREE.Vector3; yaw: number; pitch: number; dist: number }

const ease = (t: number) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2); // cubic in-out
/** Shortest signed angle from a to b. */
const turn = (a: number, b: number) => THREE.MathUtils.euclideanModulo(b - a + Math.PI, Math.PI * 2) - Math.PI;
/** Hard cap on any shot, whatever its settings say. */
const MAX_SHOT = 4;
const MIN_DIST = 3.5;

export class CinematicDirector {
  settings: CinematicSettings = getCinematicSettings();
  private saved: Pose | null = null;
  private shot: Shot | null = null;
  private shotPose: Pose | null = null;
  private t = 0;
  private readonly ray = new THREE.Raycaster();
  private readonly shake = new THREE.Vector3();

  constructor(
    private readonly rig: CameraRig,
    private readonly camera: THREE.PerspectiveCamera,
    /** Meshes the camera must stay outside (terrain, water). */
    private readonly obstacles: () => THREE.Object3D[],
    private readonly groundAt: (x: number, z: number) => number,
  ) {
    rig.onCameraInput = () => this.skip();
  }

  get active(): boolean { return !!this.shot; }
  get current(): string | null { return this.shot?.name ?? null; }

  /** Ask for a shot. Returns false when cinematics are off, a stronger shot is running, the view
   * is the overhead 2D one, or the shot can't be framed safely (the tactical view stays). */
  play(shot: Shot): boolean {
    if (this.settings.mode === 'tactical' || this.rig.flat) return false;
    if (this.shot) {
      const returning = this.t > this.shot.inTime + this.shot.hold;
      if (shot.priority <= this.shot.priority && !returning) return false;
    }
    const from = this.saved ?? this.capture();
    const pose = this.frame(shot, from);
    if (!pose) return false;
    // a replacement starts from where the camera is now, so nothing jumps
    if (this.shot) this.saved = from;
    this.shot = shot;
    this.shotPose = pose;
    this.t = 0;
    this.startFrom = this.currentPose();
    this.rig.inputLocked = true;
    return true;
  }

  private startFrom: Pose | null = null;

  /** Player skip: end the shot now and give the exact view back. Combat is untouched. */
  skip(): void { this.restore(); }

  /** Emergency: scene unload, battle end, cancellation. */
  cancel(): void { this.restore(); }

  update(dt: number): void {
    if (!this.shot || !this.saved || !this.shotPose || !this.startFrom) return;
    const s = this.shot;
    this.t += Math.max(0, dt);
    const total = s.inTime + s.hold + s.outTime;
    if (this.t >= Math.min(total, MAX_SHOT)) { this.restore(); return; }
    // a subject gone mid-shot (unit removed, effect ended): straight back to the tactical view
    if (this.t < s.inTime + s.hold && !s.subjects()?.length) { this.t = s.inTime + s.hold; }
    let pose: Pose;
    if (this.t < s.inTime) pose = this.mix(this.startFrom, this.shotPose, ease(this.t / s.inTime));
    else if (this.t < s.inTime + s.hold) pose = this.shotPose;
    else pose = this.mix(this.shotPose, this.saved, ease((this.t - s.inTime - s.hold) / Math.max(s.outTime, 1e-3)));
    this.write(pose);
    // impact shake: from the shot clock only, so it can never build up between frames
    this.shake.set(0, 0, 0);
    if (s.shake) {
      const k = (this.t - s.shake.at) / s.shake.dur;
      if (k >= 0 && k < 1) {
        const amp = s.shake.amp * this.settings.intensity * (this.settings.reduceShake ? 0.25 : 1) * (1 - k) * (1 - k);
        const w = this.t * 70;
        this.shake.set(Math.sin(w) * amp, Math.sin(w * 1.37 + 1.1) * amp * 0.6, Math.sin(w * 0.83 + 2.3) * amp);
        this.camera.position.add(this.shake);
      }
    }
  }

  dispose(): void { this.restore(); this.rig.onCameraInput = () => {}; }

  private capture(): Pose {
    this.saved = this.currentPose();
    return this.saved;
  }

  private currentPose(): Pose {
    return { target: this.rig.target.clone(), yaw: this.rig.yaw, pitch: this.rig.pitch, dist: this.rig.dist };
  }

  private restore(): void {
    if (this.saved) this.write(this.saved);
    this.saved = null;
    this.shot = this.shotPose = this.startFrom = null;
    this.t = 0;
    this.rig.inputLocked = false;
  }

  private write(p: Pose): void {
    this.rig.target.copy(p.target);
    this.rig.yaw = p.yaw;
    this.rig.pitch = p.pitch;
    this.rig.dist = p.dist;
    this.rig.apply();
  }

  private mix(a: Pose, b: Pose, k: number): Pose {
    return {
      target: a.target.clone().lerp(b.target, k),
      yaw: a.yaw + turn(a.yaw, b.yaw) * k,
      pitch: a.pitch + turn(a.pitch, b.pitch) * k,
      dist: a.dist + (b.dist - a.dist) * k,
    };
  }

  /** The shot's camera: centred on the subjects' bounding sphere, at a distance that keeps the
   * whole sphere (plus margin) in view for this field of view and aspect, pushed in from the
   * player's own distance by `push`, never closer than the subjects allow. Pulled in, or turned
   * away from, terrain in between; null if no safe framing exists. */
  private frame(shot: Shot, from: Pose): Pose | null {
    const points = shot.subjects();
    if (!points?.length) return null;
    const sphere = new THREE.Sphere().setFromPoints(points);
    const radius = Math.max(1.2, sphere.radius) * (1 + (shot.margin ?? 0.35));
    const vFov = THREE.MathUtils.degToRad(this.camera.fov) / 2;
    const hFov = Math.atan(Math.tan(vFov) * this.camera.aspect);
    const fit = radius / Math.sin(Math.min(vFov, hFov));
    const push = THREE.MathUtils.clamp(shot.push * this.settings.intensity, 0, 1);
    const dist = Math.max(MIN_DIST, fit, from.dist + (Math.min(fit, from.dist) - from.dist) * push);
    const yaw = from.yaw + THREE.MathUtils.degToRad(shot.yaw ?? 0) * push;
    const tryPitch = (pitchDeg: number): Pose | null => {
      const pose: Pose = { target: sphere.center.clone(), yaw, pitch: from.pitch + THREE.MathUtils.degToRad(pitchDeg), dist };
      return this.clear(pose);
    };
    // the intended angle first, then a little higher (over whatever is in the way)
    return tryPitch((shot.pitch ?? 0) * push) ?? tryPitch((shot.pitch ?? 0) * push + 10) ?? tryPitch(15);
  }

  /** Keep the camera outside the terrain: ray from the focus to the camera; if blocked, move in
   * to just short of the obstruction, as long as the subjects still fit; also clear the ground. */
  private clear(pose: Pose): Pose | null {
    const q = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 1, 0), pose.yaw)
      .multiply(new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(1, 0, 0), -pose.pitch));
    const back = new THREE.Vector3(0, 0, 1).applyQuaternion(q);
    this.ray.set(pose.target, back);
    this.ray.far = pose.dist;
    const hit = this.ray.intersectObjects(this.obstacles(), false)[0];
    let dist = pose.dist;
    if (hit) {
      dist = hit.distance - 0.8;
      if (dist < MIN_DIST) return null;
    }
    const cam = pose.target.clone().addScaledVector(back, dist);
    if (cam.y < this.groundAt(cam.x, cam.z) + 1) return null;
    return { ...pose, dist };
  }
}
