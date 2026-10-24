// Ember's Névoas, remade for the 3D map on the volumetric mist (src/render/volumetricFog.ts).
// Each keeps the character Ember's map editor describes for it, built as real mist that lies on
// the terrain (thicker in the low places), surrounds units and trees, and glows around lights:
//
//   mist2  Névoa 2  — soft, smooth haze over the whole world
//   mist3  Névoa 3  — Ember's original blotchy noise over the whole world
//   mist4  Névoa 4  — a slow vortex around the map's edges, the centre always clear
//   fog1   Névoa 01 — only over unrevealed ground and the backdrop, clear on the revealed map
//   fog5   Névoa 5  — low creeping ground fog in thin wisps and veils
//
// The Map Editor's "Névoa" (intensity) and "Vel. da névoa" (speed) sliders drive them, as before.
// The Vinhetas stay Ember's own screen overlays (BattleVignettes.tsx).

import * as THREE from "three";
import type { BattleEngine } from "./engine";
import type { Board } from "../map/board";
import type { VolumetricFogPass } from "../render/volumetricFog";
import { hexAt } from "../core/hex";

export type Mist3DType = "mist2" | "mist3" | "mist4" | "fog1" | "fog5";
export const MIST_3D_TYPES: readonly Mist3DType[] = ["mist2", "mist3", "mist4", "fog1", "fog5"];

interface Preset {
  /** Layer thickness above the local ground. */
  height: number;
  /** Density at intensity 1. */
  density: number;
  scale: [number, number, number];
  wind: [number, number, number];
  contrast: [number, number];
  floor: number;
  heightPow: number;
}

const PRESETS: Record<Mist3DType, Preset> = {
  mist2: { height: 3.2, density: 0.6, scale: [0.12, 0.3, 0.12], wind: [0.05, 0.01, 0.03], contrast: [0.15, 0.85], floor: 0.45, heightPow: 1.2 },
  mist3: { height: 2.6, density: 0.9, scale: [0.3, 0.5, 0.3], wind: [0.08, 0.015, 0.05], contrast: [0.42, 0.68], floor: 0.05, heightPow: 1.6 },
  mist4: { height: 3.5, density: 0.9, scale: [0.18, 0.35, 0.18], wind: [0.03, 0.01, 0.02], contrast: [0.3, 0.75], floor: 0.2, heightPow: 1.3 },
  fog1: { height: 3.0, density: 1.4, scale: [0.2, 0.4, 0.2], wind: [0.04, 0.01, 0.03], contrast: [0.25, 0.8], floor: 0.4, heightPow: 1.0 },
  fog5: { height: 0.9, density: 2.2, scale: [0.09, 1.6, 0.45], wind: [0.1, 0.0, 0.02], contrast: [0.45, 0.7], floor: 0.0, heightPow: 2.5 },
};

/** Neutral grey mist (user: "more gray"; Ember's was a greenish 0xaab4ad). */
const MIST_COLOR = 0xa9aaab;
/** Névoa 01's mask resolution: texels per world unit. */
const MASK_RES = 4;
/** Terrain height map resolution under the mist: texels per world unit. */
const GROUND_RES = 4;

export function isMist3D(type: string | undefined): type is Mist3DType {
  return !!type && (MIST_3D_TYPES as readonly string[]).includes(type);
}

export class Mist3D {
  private maskTex: THREE.DataTexture | null = null;
  private maskKey = "";
  private ground: { board: Board; tex: THREE.DataTexture; rect: THREE.Vector4; lo: number; hi: number } | null = null;

  constructor(private readonly pass: VolumetricFogPass) {}

  /** Each frame: on for the mission's Névoa (and Dev Controls' atmospheric FX), off otherwise. */
  sync(engine: BattleEngine, board: Board, groundAt: (x: number, z: number) => number, enabled: boolean): void {
    const type = engine.mission.mistType;
    const intensity = engine.mission.mistIntensity ?? 0.2;
    if (!enabled || !isMist3D(type) || intensity <= 0) { this.pass.enabled = false; return; }
    const p = PRESETS[type], s = this.pass.settings;
    const b = board.bounds();
    this.pass.enabled = true;
    const ground = this.groundMap(board, groundAt);
    s.ground = ground.tex;
    s.groundRect.copy(ground.rect);
    s.groundRange.set(ground.lo, ground.hi);
    s.base = ground.lo;
    s.top = ground.hi + p.height;
    s.thickness = p.height;
    s.pool = 0.8;
    // like Ember's power curve: low slider values stay genuinely subtle, max still means max
    s.density = p.density * Math.pow(intensity, 1.5);
    s.speed = engine.mission.mistSpeed ?? 1;
    s.albedo.set(MIST_COLOR);
    s.scale.set(...p.scale);
    s.wind.set(...p.wind);
    s.contrast = p.contrast;
    s.floor = p.floor;
    s.heightPow = p.heightPow;
    s.center.set((b.x0 + b.x1) / 2, (b.z0 + b.z1) / 2);
    const half = Math.min(b.x1 - b.x0, b.z1 - b.z0) / 2;
    s.swirl = type === "mist4" ? 0.12 : 0;
    s.clear = type === "mist4" ? half * 0.45 : 0;
    s.edge = type === "mist4" ? half * 1.1 : 0;
    s.mask = type === "fog1" ? this.revealMask(engine, board) : null;
    s.board.set(b.x0, b.z0, b.x1, b.z1);
    s.reach = type === "fog1" ? 14 : 10;
  }

  /** The terrain's height under the mist (built once per board), so the layer follows the ground. */
  private groundMap(board: Board, groundAt: (x: number, z: number) => number) {
    if (this.ground?.board === board) return this.ground;
    const b = board.bounds(), m = 2;
    const rect = new THREE.Vector4(b.x0 - m, b.z0 - m, b.x1 - b.x0 + 2 * m, b.z1 - b.z0 + 2 * m);
    const gw = Math.ceil(rect.z * GROUND_RES), gh = Math.ceil(rect.w * GROUND_RES);
    const heights = new Float32Array(gw * gh);
    let lo = Infinity, hi = -Infinity;
    for (let y = 0; y < gh; y++) for (let x = 0; x < gw; x++) {
      const v = groundAt(rect.x + (x + 0.5) / GROUND_RES, rect.y + (y + 0.5) / GROUND_RES);
      heights[y * gw + x] = v; lo = Math.min(lo, v); hi = Math.max(hi, v);
    }
    const data = new Uint8Array(gw * gh * 4);
    for (let i = 0; i < heights.length; i++) data[i * 4 + 3] = Math.round(255 * (heights[i] - lo) / Math.max(hi - lo, 1e-3));
    this.ground?.tex.dispose();
    const tex = new THREE.DataTexture(data, gw, gh, THREE.RGBAFormat, THREE.UnsignedByteType);
    tex.minFilter = tex.magFilter = THREE.LinearFilter;
    tex.needsUpdate = true;
    this.ground = { board, tex, rect, lo, hi };
    return this.ground;
  }

  /** Névoa 01: mist only where the party has not revealed the map (and everywhere off the board). */
  private revealMask(engine: BattleEngine, board: Board): THREE.Texture {
    const b = board.bounds(), m = 2;
    const x0 = b.x0 - m, z0 = b.z0 - m, w = b.x1 - b.x0 + 2 * m, d = b.z1 - b.z0 + 2 * m;
    this.pass.settings.maskRect.set(x0, z0, w, d);
    const key = `${engine.mission.id}:${engine.visVersion}:${engine.fogged}`;
    if (key === this.maskKey && this.maskTex) return this.maskTex;
    this.maskKey = key;
    const gw = Math.ceil(w * MASK_RES), gh = Math.ceil(d * MASK_RES);
    const data = new Uint8Array(gw * gh * 4);
    const L = board.layout;
    for (let y = 0; y < gh; y++) for (let x = 0; x < gw; x++) {
      const [c, r] = hexAt(L, x0 + (x + 0.5) / MASK_RES, z0 + (y + 0.5) / MASK_RES);
      const off = c < 0 || r < 0 || c >= L.cols || r >= L.rows;
      data[(y * gw + x) * 4 + 3] = off || !engine.explored(c, r) ? 255 : 0;
    }
    this.maskTex?.dispose();
    const tex = new THREE.DataTexture(data, gw, gh, THREE.RGBAFormat, THREE.UnsignedByteType);
    tex.minFilter = tex.magFilter = THREE.LinearFilter;
    tex.needsUpdate = true;
    this.maskTex = tex;
    return tex;
  }

  dispose(): void {
    this.pass.enabled = false;
    this.maskTex?.dispose();
    this.ground?.tex.dispose();
  }
}
