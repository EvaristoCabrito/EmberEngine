import * as THREE from "three";
import { boxBlur } from "./ThreeGroundAO";

/**
 * Fog-of-war overlay: ONE world-aligned plane over the whole board, textured with a small
 * per-texel darkness mask built from the engine's per-hex visibility (UNSEEN / EXPLORED /
 * VISIBLE). It is the only visual part of fog of war — gameplay visibility (which hexes are
 * seen, which enemies show) stays in the engine and never reads this mask.
 *
 *   VISIBLE    transparent
 *   EXPLORED   black at EXPLORED_ALPHA (terrain and static props stay readable underneath)
 *   UNEXPLORED opaque black — genuinely black, on purpose: fog-of-war's real use is dungeons,
 *              where a solid, opaque unseen area is the correct look (you cannot see into an
 *              unlit room at all), not something to lighten or blend with a painted backdrop.
 *
 * The mask is blurred a little so the VISIBLE/EXPLORED boundary reads as a soft edge rather than
 * a row of hexagons; UNEXPLORED stays a flat, hard-edged fill (no bleed into or out of it) — the
 * fix for "a black edge shows up right next to something" is ThreeBattleRenderer.syncFog's own
 * clearAround set (no fog-of-war within one hex of any living player unit), not softening the
 * edge once it's already on screen.
 *
 * Rebuilt only when the caller's key changes (the engine bumps visVersion when visibility
 * actually changes); between rebuilds it is one static textured quad.
 */

/** Mask texels per hex radius (`tile`). Sized in hex units, so zoom only rescales the plane.
 * 8, not 4: at 4 the linear filter bled up to a quarter-hex of explored darkness into the
 * edge of visible hexes (measured: 17 of 821 samples shaded, up to 25 luminance). */
const RES = 8;
/** Feather: box-blur radius (texels) x passes ≈ gaussian sigma ~0.3 hex radius. */
const BLUR_R = 2;
const BLUR_PASSES = 2;
/** Darkness over explored-but-not-visible ground. */
export const EXPLORED_ALPHA = 0.6;
/** Darkness over never-seen ground — genuinely opaque, per direct request: fog-of-war's real
 * use is dungeons, where solid black is the correct look, not a lighter translucent wash. */
export const UNSEEN_ALPHA = 1;

export const FOG_UNSEEN = 0;
export const FOG_EXPLORED = 1;
export const FOG_VISIBLE = 2;

/** Debug colors (RGBA 0-255): raw per-hex states, no feathering. */
const DEBUG_COLORS: Record<number, [number, number, number, number]> = {
  [FOG_VISIBLE]: [40, 220, 90, 70],
  [FOG_EXPLORED]: [240, 170, 30, 140],
  [FOG_UNSEEN]: [220, 30, 40, 200],
};

export class FogMask {
  readonly mesh: THREE.Mesh;
  private material: THREE.MeshBasicMaterial;
  private geometry = new THREE.PlaneGeometry(1, 1);
  private texture: THREE.DataTexture | null = null;
  private key = "";
  private gw = 1;
  private gh = 1;

  constructor() {
    this.material = new THREE.MeshBasicMaterial({
      transparent: true,
      depthWrite: false,
      // Nothing else writes depth above z=50, except each house's fogCut (ThreeBattleRenderer),
      // so this only skips the fog over a house's own art.
      depthTest: true,
    });
    this.mesh = new THREE.Mesh(this.geometry, this.material);
    // Above everything in the scene — terrain, decorations (z=1), unit meshes (z=2) and the
    // atmosphere's mist/embers (z>3) — so unexplored ground is black with nothing drawn on top.
    this.mesh.renderOrder = 100;
    this.mesh.visible = false;
  }

  /**
   * `cellAt(x, y)` maps a tile-normalized world point (hexWorld units, y-down) to the hex it
   * lies in, or -1 off the board; `stateAt(index)` gives that hex's fog state.
   */
  update(
    key: string,
    cols: number,
    rows: number,
    padMul: number,
    tile: number,
    debug: boolean,
    cellAt: (x: number, y: number) => number,
    stateAt: (index: number) => number,
  ): void {
    if (key !== this.key) {
      this.key = key;
      this.build(cols, rows, padMul, debug, cellAt, stateAt);
    }
    const w = (tile * this.gw) / RES;
    const h = (tile * this.gh) / RES;
    this.mesh.scale.set(w, h, 1);
    // Y negated like every other mesh here (see ThreeBattleRenderer's module comment).
    this.mesh.position.set(w / 2, -h / 2, 50);
    this.mesh.visible = true;
  }

  hide(): void {
    this.mesh.visible = false;
  }

  private build(
    cols: number,
    rows: number,
    padMul: number,
    debug: boolean,
    cellAt: (x: number, y: number) => number,
    stateAt: (index: number) => number,
  ): void {
    const gw = Math.ceil(Math.sqrt(3) * (cols + 1.5) * RES);
    const gh = Math.ceil((2 * padMul + 1.5 * rows + 2) * RES);
    this.gw = gw;
    this.gh = gh;
    const n = gw * gh;
    const state = new Int8Array(n);
    for (let y = 0; y < gh; y++) {
      for (let x = 0; x < gw; x++) {
        const c = cellAt((x + 0.5) / RES, (y + 0.5) / RES);
        state[y * gw + x] = c < 0 ? -1 : stateAt(c);
      }
    }
    const data = new Uint8Array(n * 4);
    // Texture row 0 is the plane's BOTTOM edge; the mask is built top-down (y-down world),
    // so rows are written flipped.
    const put = (x: number, y: number, r: number, g: number, b: number, a: number) => {
      const i = ((gh - 1 - y) * gw + x) * 4;
      data[i] = r;
      data[i + 1] = g;
      data[i + 2] = b;
      data[i + 3] = a;
    };
    if (debug) {
      for (let y = 0; y < gh; y++) {
        for (let x = 0; x < gw; x++) {
          const s = state[y * gw + x]!;
          if (s < 0) continue;
          const [r, g, b, a] = DEBUG_COLORS[s]!;
          put(x, y, r, g, b, a);
        }
      }
    } else {
      let alpha = new Float32Array(n);
      for (let i = 0; i < n; i++) {
        const s = state[i]!;
        // Off the board (s < 0) counts as unexplored: the black must not end in a row of hex
        // teeth that shows the backdrop/mist and so traces the map's outline.
        alpha[i] = s === FOG_UNSEEN || s < 0 ? UNSEEN_ALPHA : s === FOG_EXPLORED ? EXPLORED_ALPHA : 0;
      }
      const tmp = new Float32Array(n);
      for (let pass = 0; pass < BLUR_PASSES; pass++) {
        boxBlur(alpha, tmp, gw, gh, BLUR_R, true);
        boxBlur(tmp, alpha, gw, gh, BLUR_R, false);
      }
      for (let y = 0; y < gh; y++) {
        for (let x = 0; x < gw; x++) {
          const i = y * gw + x;
          const s = state[i]!;
          // Visible hexes are fully revealed (never shaded by a neighbour's feather) and
          // unexplored hexes stay flat opaque black (no bleed in either direction); the soft
          // edge lives only inside explored hexes, fading from clear at the edge of sight up
          // to their normal darkness.
          const a = s === FOG_VISIBLE ? 0 : s === FOG_UNSEEN || s < 0 ? UNSEEN_ALPHA : Math.min(EXPLORED_ALPHA, alpha[i]!);
          put(x, y, 0, 0, 0, Math.round(255 * a));
        }
      }
      alpha = new Float32Array(0);
    }
    this.texture?.dispose();
    const tex = new THREE.DataTexture(data, gw, gh, THREE.RGBAFormat, THREE.UnsignedByteType);
    tex.minFilter = debug ? THREE.NearestFilter : THREE.LinearFilter;
    tex.magFilter = debug ? THREE.NearestFilter : THREE.LinearFilter;
    tex.wrapS = THREE.ClampToEdgeWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    tex.needsUpdate = true;
    this.texture = tex;
    this.material.map = tex;
    this.material.needsUpdate = true;
  }

  dispose(): void {
    this.texture?.dispose();
    this.material.dispose();
    this.geometry.dispose();
  }
}
