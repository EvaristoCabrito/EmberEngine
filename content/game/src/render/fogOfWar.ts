// Ember's fog of war on the 3D map. Same look and rules as its ThreeFogMask: a soft darkness
// mask built from the engine's per-hex sight (never seen = opaque black, seen before = 60%
// dark, in sight = clear; the soft edge lives only inside remembered hexes). Ember laid that mask
// on one flat sheet above a flat board. In 3D the darkness is drawn as a copy of the terrain's
// own surfaces (ground and water) that looks the mask up by world position, so walls, cliffs and
// raised ground are covered exactly; the decoration cards standing in fog are darkened or hidden
// by DecorLayer.shade, and hidden enemies are not drawn at all.

import * as THREE from 'three';
import { MeshBasicNodeMaterial } from 'three/webgpu';
import { positionWorld, texture, uniform, vec3, float, or } from 'three/tsl';
import { WEBGPU } from './gpu';
import type { Board } from '../map/board';
import { hexAt } from '../core/hex';
import { FOG_RENDER_ORDER } from './drawOrder';
import { boxBlur } from '../game/gfx/three/ThreeGroundAO';
import { EXPLORED_ALPHA, UNSEEN_ALPHA, FOG_UNSEEN, FOG_EXPLORED, FOG_VISIBLE } from '../game/gfx/three/ThreeFogMask';

export { FOG_UNSEEN, FOG_EXPLORED, FOG_VISIBLE };

/** Mask texels per world unit (one hex radius), as Ember's RES. */
const RES = 8;
const BLUR_R = 2;
const BLUR_PASSES = 2;
/** Raised a little over the surface it copies, so it also covers the battle grid (0.05 + layers). */
const LIFT = 0.08;
/** Off-board margin, kept black. */
const MARGIN = 1.5;

const VERTEX = /* glsl */ `
  uniform float uLift;
  varying vec2 vWorldXZ;
  void main() {
    vec4 world = modelMatrix * vec4(position, 1.0);
    world.y += uLift;
    vWorldXZ = world.xz;
    gl_Position = projectionMatrix * viewMatrix * world;
  }
`;
const FRAGMENT = /* glsl */ `
  uniform sampler2D uMask;
  uniform vec4 uRect; // x0, z0, width, depth of the mask in world units
  varying vec2 vWorldXZ;
  void main() {
    vec2 uv = (vWorldXZ - uRect.xy) / uRect.zw;
    float a = (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) ? 1.0 : texture2D(uMask, uv).a;
    gl_FragColor = vec4(0.0, 0.0, 0.0, a);
  }
`;

export class FogOfWar {
  readonly group = new THREE.Group();
  private readonly material = new THREE.ShaderMaterial({
    vertexShader: VERTEX, fragmentShader: FRAGMENT, transparent: true, depthWrite: false,
    uniforms: { uMask: { value: null }, uRect: { value: new THREE.Vector4() }, uLift: { value: LIFT } },
  });
  /** WebGPU path (render/gpu.ts): the same mask lookup in TSL; the lift goes into each copy's matrix. */
  private readonly maskNode = texture(Object.assign(new THREE.DataTexture(new Uint8Array([0, 0, 0, 255]), 1, 1), { needsUpdate: true }));
  private readonly rectNode = uniform(new THREE.Vector4());
  private readonly nodeMaterial = WEBGPU ? this.makeNodeMaterial() : null;

  private makeNodeMaterial(): MeshBasicNodeMaterial {
    const m = new MeshBasicNodeMaterial({ transparent: true, depthWrite: false });
    const uvw = positionWorld.xz.sub(this.rectNode.xy).div(this.rectNode.zw);
    const outside = or(or(uvw.x.lessThan(0), uvw.y.lessThan(0)), or(uvw.x.greaterThan(1), uvw.y.greaterThan(1)));
    m.colorNode = vec3(0);
    m.opacityNode = outside.select(float(1), this.maskNode.sample(uvw).a);
    return m;
  }
  private texture: THREE.DataTexture | null = null;
  private key = '';
  private sources: THREE.Mesh[] = [];

  constructor() { this.group.visible = false; }

  hide(): void { this.group.visible = false; }

  /** Rebuild the mask when `key` changes. `surfaces` are the terrain meshes to darken;
   * `stateAt(col, row)` is the hex's fog state (FOG_*). */
  update(key: string, board: Board, surfaces: THREE.Mesh[], stateAt: (col: number, row: number) => number): void {
    this.group.visible = true;
    if (surfaces.length !== this.sources.length || surfaces.some((s, i) => s !== this.sources[i])) {
      this.sources = surfaces;
      this.group.clear();
      for (const s of surfaces) {
        const m = new THREE.Mesh(s.geometry, this.nodeMaterial ?? this.material);
        m.matrixAutoUpdate = false;
        m.matrix.copy(s.matrixWorld);
        if (this.nodeMaterial) m.matrix.premultiply(new THREE.Matrix4().makeTranslation(0, LIFT, 0));
        m.renderOrder = FOG_RENDER_ORDER; // over ground/grid; cards have their own fog shading
        this.group.add(m);
      }
    }
    if (key === this.key) return;
    this.key = key;
    this.paint(board, stateAt);
  }

  private cellIndex(board: Board, x: number, z: number): number {
    const L = board.layout;
    const [c, r] = hexAt(L, x, z);
    return c < 0 || r < 0 || c >= L.cols || r >= L.rows ? -1 : r * L.cols + c;
  }

  /** Ember's mask: per-texel state, feathered only inside remembered hexes. */
  private paint(board: Board, stateAt: (col: number, row: number) => number): void {
    const b = board.bounds();
    const x0 = b.x0 - MARGIN, z0 = b.z0 - MARGIN, w = b.x1 - b.x0 + 2 * MARGIN, d = b.z1 - b.z0 + 2 * MARGIN;
    const cols = board.layout.cols;
    const gw = Math.ceil(w * RES), gh = Math.ceil(d * RES), n = gw * gh;
    const state = new Int8Array(n);
    for (let y = 0; y < gh; y++) for (let x = 0; x < gw; x++) {
      const ci = this.cellIndex(board, x0 + (x + 0.5) / RES, z0 + (y + 0.5) / RES);
      state[y * gw + x] = ci < 0 ? -1 : stateAt(ci % cols, Math.floor(ci / cols));
    }
    const alpha = new Float32Array(n), tmp = new Float32Array(n);
    for (let i = 0; i < n; i++) { const s = state[i]; alpha[i] = s === FOG_UNSEEN || s < 0 ? UNSEEN_ALPHA : s === FOG_EXPLORED ? EXPLORED_ALPHA : 0; }
    for (let p = 0; p < BLUR_PASSES; p++) { boxBlur(alpha, tmp, gw, gh, BLUR_R, true); boxBlur(tmp, alpha, gw, gh, BLUR_R, false); }
    const data = new Uint8Array(n * 4);
    for (let i = 0; i < n; i++) {
      const s = state[i];
      const a = s === FOG_VISIBLE ? 0 : s === FOG_UNSEEN || s < 0 ? UNSEEN_ALPHA : Math.min(EXPLORED_ALPHA, alpha[i]);
      data[i * 4 + 3] = Math.round(255 * a); // texture row 0 = z0, matching the shader's uv.y
    }
    this.texture?.dispose();
    const tex = new THREE.DataTexture(data, gw, gh, THREE.RGBAFormat, THREE.UnsignedByteType);
    tex.minFilter = tex.magFilter = THREE.LinearFilter;
    tex.needsUpdate = true;
    this.texture = tex;
    this.material.uniforms.uMask.value = tex;
    (this.material.uniforms.uRect.value as THREE.Vector4).set(x0, z0, w, d);
    this.maskNode.value = tex;
    this.rectNode.value.set(x0, z0, w, d);
  }

  dispose(): void {
    this.texture?.dispose();
    this.material.dispose();
    this.nodeMaterial?.dispose();
  }
}
