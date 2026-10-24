/** MILESTONE 3 — real world-space fog + particles, built as actual scene geometry (not a
 * screen-space Canvas2D/CSS overlay like the old, rejected AtmosphereFX — see
 * gfx/AtmosphereRenderer.ts's own comment and THREEJS_MILESTONE2_HANDOFF.md's "Old atmosphere
 * system" section).
 *
 * WHY NOT scene.fog / THREE.Fog / THREE.FogExp2: this orthographic camera now has a shallow
 * orbit angle, so built-in fog would grade pixels by camera depth. That still gives no authored
 * control over where the mist sits or how it layers around the board's specific props. The
 * atmosphere stays as explicit world-space sheets and particles, with their own color, height,
 * motion and render order. `scene.fog` is deliberately never set anywhere in this renderer.
 *
 * A consequence worth stating plainly: the orbit gives raised geometry real depth, but these
 * atmosphere sheets intentionally remain above the ordinary board layers. Their render order
 * (10-12 mist layers, 13 dust, 14 embers) keeps them deterministic instead of trying to interleave
 * one broad mist volume behind some transparent sprites and in front of others.
 *
 * Neither system casts or receives shadows (both default false, left untouched) — a
 * PCF-filtered shadow lookup against a huge, additively-blended, constantly-drifting transparent
 * volume would be visually meaningless and a real, avoidable cost on top of the existing
 * 2048x2048 shadow map.
 *
 * Fog-of-war: tile meshes already render unconditionally with no per-tile visibility check today
 * (only decorations get that treatment — see ThreeBattleRenderer.syncDecorVisibility), so a
 * board-wide mist/particle spread is consistent with existing behavior, not a regression. No
 * explored-cell masking here — real, non-trivial extra work out of proportion to this milestone;
 * flagged as a known follow-up if a fogged mission ever gets a non-zero tier. */

import * as THREE from "three";
import type { BattleEngine } from "../../engine";
import { boxBlur } from "./ThreeGroundAO";
import { shaderMaterial } from "../../../render/shaderNode";
import { mist2Nodes, mist3Nodes, mist4Nodes, fog5Nodes, exteriorFogNodes, revealFogNodes, dustNodes, emberNodes } from "./atmosphereNodes";

/** Mist tuning for one frame — built fresh from Mission.mistIntensity in ThreeAtmosphere.sync()
 * (see its own comment) rather than a hardcoded per-mission-id table, so the Map Editor's
 * "Névoa" slider is the one real source of this, not a file only a developer can edit. */
interface AtmosphereTier {
  mistIntensity: number;
  mistHeight: number;
  mistColor: number;
  dustCount: number;
  emberCount: number;
  emberRiseHeight: number;
}

/** wispIntensity=1 (the editor slider's max) maps to this many ember instances — deliberately
 * high enough to be genuinely overwhelming at max, on purpose (see Mission.wispIntensity). */
const MAX_EMBER_COUNT = 2000;

const SQRT3 = Math.sqrt(3);
/** Must match BattleEngine's private boardPad()/boardSize() (tile * 2.4) — duplicated, not
 * imported, same precedent ThreeBattleRenderer.ts's own hexWorld already set for this exact
 * formula. Kept in sync by hand; check against engine.ts's boardSize if board-edge coverage ever
 * looks off. */
const BOARD_PAD_MUL = 2.4;

function boardSize(cols: number, rows: number, tile: number): { w: number; h: number } {
  return {
    w: tile * SQRT3 * (cols + 0.5),
    h: tile * (1.5 * (rows - 1) + 2) + tile * BOARD_PAD_MUL,
  };
}

// ---------------------------------------------------------------------------------------------
// Ground mist, take 3: back to the original layered-plane structure, but the noise itself is now
// a pre-baked TEXTURE sampled with bilinear filtering (ported from the old, rejected
// gfx/noiseTexture.ts's exact tileable value-noise algorithm — that file's actual math was never
// the problem, only its old screen-space-overlay delivery was, per direct instruction to reuse
// it), not hand-rolled per-pixel hash noise. Hash noise has an inherent cellular/grid structure
// that read as a "blobby camo pattern" no matter how its contrast was tuned; texture-sampled fbm
// is naturally smooth because the GPU's bilinear filtering interpolates between texels for free.
// This mirrors gfx/atmosphereShaders.ts's FRAG_GROUND_HAZE technique exactly (same fbm weights,
// same "just multiply by density, no extra contrast curve" approach), adapted from a screen-space
// pass into real world-anchored plane geometry.

function hash2(x: number, y: number, seed: number): number {
  const h = Math.sin(x * 127.1 + y * 311.7 + seed * 74.7) * 43758.5453123;
  return h - Math.floor(h);
}

function valueNoiseTileable(u: number, v: number, freq: number, seed: number): number {
  const xf = u * freq;
  const yf = v * freq;
  const x0 = Math.floor(xf);
  const y0 = Math.floor(yf);
  const tx = xf - x0;
  const ty = yf - y0;
  const wrap = (n: number) => ((n % freq) + freq) % freq;
  const h00 = hash2(wrap(x0), wrap(y0), seed);
  const h10 = hash2(wrap(x0 + 1), wrap(y0), seed);
  const h01 = hash2(wrap(x0), wrap(y0 + 1), seed);
  const h11 = hash2(wrap(x0 + 1), wrap(y0 + 1), seed);
  const sx = tx * tx * (3 - 2 * tx);
  const sy = ty * ty * (3 - 2 * ty);
  const a = h00 + (h10 - h00) * sx;
  const b = h01 + (h11 - h01) * sx;
  return a + (b - a) * sy;
}

/** Builds the exact same tileable-noise RGB data gfx/noiseTexture.ts's buildNoiseTexture bakes
 * for the old (rejected) system, as a THREE.DataTexture instead of a raw WebGL2 texture — the
 * noise algorithm is reused verbatim, only the texture object type changes. */
function buildMistNoiseTexture(size = 256): THREE.DataTexture {
  const data = new Uint8Array(size * size * 4);
  for (let y = 0; y < size; y++) {
    const v = y / size;
    for (let x = 0; x < size; x++) {
      const u = x / size;
      const r = valueNoiseTileable(u, v, 4, 1.7);
      const g = valueNoiseTileable(u, v, 9, 5.3);
      const b = valueNoiseTileable(u, v, 17, 11.1);
      const i = (y * size + x) * 4;
      data[i] = Math.round(r * 255);
      data[i + 1] = Math.round(g * 255);
      data[i + 2] = Math.round(b * 255);
      data[i + 3] = 255;
    }
  }
  const tex = new THREE.DataTexture(data, size, size, THREE.RGBAFormat);
  tex.wrapS = THREE.RepeatWrapping;
  tex.wrapT = THREE.RepeatWrapping;
  tex.minFilter = THREE.LinearFilter;
  tex.magFilter = THREE.LinearFilter;
  tex.needsUpdate = true;
  return tex;
}

const MIST_VERTEX = /* glsl */ `
  varying vec2 vWorldXY;
  void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldXY = worldPos.xy;
    gl_Position = projectionMatrix * viewMatrix * worldPos;
  }
`;

const MIST_FRAGMENT = /* glsl */ `
  uniform sampler2D uNoiseTex;
  uniform float uTime;
  uniform float uScale;
  uniform vec2 uDrift;
  uniform vec3 uColor;
  uniform vec3 uSunColor;
  uniform float uAlpha;
  varying vec2 vWorldXY;

  // Same three-octave weighted sum as atmosphereShaders.ts's FRAG_GROUND_HAZE fbm() — three
  // texture fetches at different scales/offsets, texture filtering does the smoothing for free.
  float fbm(vec2 uv) {
    return texture2D(uNoiseTex, uv).r * 0.55
         + texture2D(uNoiseTex, uv * 2.3 + 3.1).g * 0.3
         + texture2D(uNoiseTex, uv * 4.7 + 9.4).b * 0.15;
  }

  void main() {
    vec2 uv = vWorldXY * uScale + uTime * uDrift;
    float n = clamp(fbm(uv), 0.0, 1.0);
    // No contrast-curve reshaping here on purpose (unlike every earlier hash-noise attempt) —
    // real fbm from filtered texture samples already looks like soft haze, not a mathematical
    // pattern; a smoothstep/pow curve was only ever needed to fight hash noise's harder edges.
    vec3 color = mix(uColor, uSunColor, 0.35);
    gl_FragColor = vec4(color, n * uAlpha);
  }
`;

class GroundMist {
  readonly group = new THREE.Group();
  private geo = new THREE.PlaneGeometry(1, 1);
  private noiseTex = buildMistNoiseTexture();
  private materials: THREE.ShaderMaterial[] = [];
  private meshes: THREE.Mesh[] = [];
  private builtKey = "";

  // Original layered structure: denser near the ground, each layer drifting independently so
  // they never read as one plane repeated.
  private static readonly LAYER_FALLOFF = [1.0, 0.6, 0.32];
  private static readonly LAYER_Z_FRAC = [0.08, 0.4, 0.85];
  // These magnitudes are the CONFIRMED-visible drift speed found earlier tonight (~18x the
  // original 0.008-0.014 values, which were technically animating but far too slow to perceive
  // as movement over a normal glance — verified by comparing two frames several seconds apart).
  // Cut roughly in half from the "confirmed visible" values above — smooth texture-sampled noise
  // reads motion much more legibly than the old hash noise did, so the same raw speed that was
  // barely perceptible before now reads as "too speedy" (direct feedback). The editor's "Vel. da
  // névoa" slider still scales from this new baseline if a specific map wants it faster/slower.
  private static readonly LAYER_DRIFT: [number, number][] = [
    [0.07, 0.11],
    [-0.095, 0.05],
    [0.045, -0.12],
  ];
  /** Strictly above the unit sprite band (max ~3) — see module comment on Z-ordering. Fixed
   * per-layer values, NOT derived from any per-instance random seed — the puff system's bug was
   * exactly that mistake (an unbounded per-instance value used as a Z coordinate, pushing most
   * instances behind the camera). These three are hand-picked constants, always in view. */
  /** Above the normal scene band, letting Fog 2 remain its own foreground weather treatment. */
  private static readonly BASE_Z = 5;

  constructor() {
    for (let i = 0; i < 3; i++) {
      const material = shaderMaterial({
        vertexShader: MIST_VERTEX,
        fragmentShader: MIST_FRAGMENT,
        transparent: true,
        depthWrite: false,
        depthTest: false,
        uniforms: {
          uNoiseTex: { value: this.noiseTex },
          uTime: { value: 0 },
          uScale: { value: 0.003 },
          uDrift: { value: new THREE.Vector2(...GroundMist.LAYER_DRIFT[i]!) },
          uColor: { value: new THREE.Color(0xaab4ad) },
          uSunColor: { value: new THREE.Color(0xffffff) },
          uAlpha: { value: 0 },
        },
      }, mist2Nodes);
      const mesh = new THREE.Mesh(this.geo, material);
      // Below the character billboards (order 2) so ground mist never paints over a unit.
      mesh.renderOrder = 1.5 + i * 0.1;
      this.materials.push(material);
      this.meshes.push(mesh);
      this.group.add(mesh);
    }
  }

  rebuild(cols: number, rows: number, tile: number, missionId: string, tier: AtmosphereTier): void {
    const key = `${missionId}:${cols}:${rows}:${tile}`;
    if (key !== this.builtKey) {
      this.builtKey = key;
      const { w, h } = boardSize(cols, rows, tile);
      const cx = w / 2;
      const cy = h / 2;
      for (let i = 0; i < this.meshes.length; i++) {
        const mesh = this.meshes[i]!;
        // 1.15x overscan so panning to the board edge doesn't reveal a hard mist boundary.
        mesh.scale.set(w * 1.15, h * 1.15, 1);
        mesh.position.set(cx, -cy, GroundMist.BASE_Z + tier.mistHeight * GroundMist.LAYER_Z_FRAC[i]!);
      }
    }
    // Power curve, not a direct multiply — the raw linear mapping made 0.2 (meant to be a gentle
    // low setting) already read as "unbearable" (direct complaint: "where is the MIN option, all
    // you gave me was MAX"). Squaring pushes low slider values down much further than high ones
    // (0.2 -> 0.04, 0.5 -> 0.25, 1.0 -> 1.0 unchanged) — max still means max, but the bottom of
    // the range is now genuinely subtle instead of already-strong.
    const shapedIntensity = tier.mistIntensity * tier.mistIntensity;
    for (let i = 0; i < this.materials.length; i++) {
      const material = this.materials[i]!;
      (material.uniforms.uColor!.value as THREE.Color).setHex(tier.mistColor);
      material.uniforms.uAlpha!.value = shapedIntensity * GroundMist.LAYER_FALLOFF[i]!;
    }
    this.group.visible = tier.mistIntensity > 0;
  }

  /** Mist 2 is a viewport-filling weather layer. Keeping it camera-locked after its normal
   * board setup lets the same drifting field cover the painted mission backdrop as well as the
   * hexes, with a small overscan so no edge appears while panning. */
  coverViewport(cssW: number, cssH: number, camX: number, camY: number, coverW = cssW, coverH = cssH): void {
    if (!this.group.visible) return;
    for (const mesh of this.meshes) {
      mesh.scale.set(coverW * 1.16, coverH * 1.16, 1);
      mesh.position.x = camX + cssW / 2;
      mesh.position.y = -camY - cssH / 2;
    }
  }

  sync(dt: number, sunLight: THREE.DirectionalLight, speed: number): void {
    for (const material of this.materials) {
      material.uniforms.uTime!.value += dt * speed;
      (material.uniforms.uSunColor!.value as THREE.Color).copy(sunLight.color).multiplyScalar(Math.min(1.5, sunLight.intensity * 0.6));
    }
  }

  dispose(): void {
    this.geo.dispose();
    this.noiseTex.dispose();
    for (const material of this.materials) material.dispose();
  }
}

// ---------------------------------------------------------------------------------------------
// "Mist 3" — the actual original first version (the puff-based InstancedMesh attempt was a
// LATER rewrite, mislabeled as "3" once before — this is the real one): raw per-pixel hash noise
// (no pre-baked texture), 3 octaves blended 0.6/0.3/0.1, a narrow smoothstep(0.45,0.88) contrast
// band, small uScale — kept selectable exactly as it originally was, for direct comparison
// against Mist 2's texture-based approach. Drift speed uses the same confirmed-visible magnitude
// found earlier tonight (the original 0.008-0.014 values animated too slowly to ever perceive).

const MIST3_VERTEX = /* glsl */ `
  varying vec2 vWorldXY;
  void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldXY = worldPos.xy;
    gl_Position = projectionMatrix * viewMatrix * worldPos;
  }
`;

const MIST3_FRAGMENT = /* glsl */ `
  uniform float uTime;
  uniform vec2 uDrift;
  uniform vec3 uColor;
  uniform vec3 uSunColor;
  uniform float uAlpha;
  varying vec2 vWorldXY;

  vec2 hash(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
  }
  float noise(vec2 p) {
    const float K1 = 0.366025404;
    const float K2 = 0.211324865;
    vec2 i = floor(p + (p.x + p.y) * K1);
    vec2 a = p - i + (i.x + i.y) * K2;
    vec2 o = a.x > a.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec2 b = a - o + K2;
    vec2 c = a - 1.0 + 2.0 * K2;
    vec3 h = max(0.5 - vec3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    vec3 n = h * h * h * h * vec3(dot(a, hash(i)), dot(b, hash(i + o)), dot(c, hash(i + 1.0)));
    return dot(n, vec3(70.0));
  }

  void main() {
    vec2 p = vWorldXY * 0.012 + uTime * uDrift;
    float n = noise(p) * 0.6 + noise(p * 2.03 + 11.0) * 0.3 + noise(p * 4.1 + 71.0) * 0.1;
    n = clamp(n * 0.5 + 0.5, 0.0, 1.0);
    n = smoothstep(0.45, 0.88, n);
    vec3 color = mix(uColor, uSunColor, 0.35);
    gl_FragColor = vec4(color, n * uAlpha);
  }
`;

class GroundMist3 {
  readonly group = new THREE.Group();
  private geo = new THREE.PlaneGeometry(1, 1);
  private materials: THREE.ShaderMaterial[] = [];
  private meshes: THREE.Mesh[] = [];
  private builtKey = "";

  private static readonly LAYER_FALLOFF = [1.0, 0.6, 0.32];
  private static readonly LAYER_Z_FRAC = [0.08, 0.4, 0.85];
  private static readonly LAYER_DRIFT: [number, number][] = [
    [0.14, 0.22],
    [-0.19, 0.1],
    [0.09, -0.24],
  ];
  private static readonly BASE_Z = 5;

  constructor() {
    for (let i = 0; i < 3; i++) {
      const material = shaderMaterial({
        vertexShader: MIST3_VERTEX,
        fragmentShader: MIST3_FRAGMENT,
        transparent: true,
        depthWrite: false,
        depthTest: false,
        uniforms: {
          uTime: { value: 0 },
          uDrift: { value: new THREE.Vector2(...GroundMist3.LAYER_DRIFT[i]!) },
          uColor: { value: new THREE.Color(0xaab4ad) },
          uSunColor: { value: new THREE.Color(0xffffff) },
          uAlpha: { value: 0 },
        },
      }, mist3Nodes);
      const mesh = new THREE.Mesh(this.geo, material);
      // Below the character billboards (order 2) so ground mist never paints over a unit.
      mesh.renderOrder = 1.5 + i * 0.1;
      this.materials.push(material);
      this.meshes.push(mesh);
      this.group.add(mesh);
    }
  }

  rebuild(cols: number, rows: number, tile: number, missionId: string, tier: AtmosphereTier): void {
    const key = `${missionId}:${cols}:${rows}:${tile}`;
    if (key !== this.builtKey) {
      this.builtKey = key;
      const { w, h } = boardSize(cols, rows, tile);
      const cx = w / 2;
      const cy = h / 2;
      for (let i = 0; i < this.meshes.length; i++) {
        const mesh = this.meshes[i]!;
        mesh.scale.set(w * 1.15, h * 1.15, 1);
        mesh.position.set(cx, -cy, GroundMist3.BASE_Z + tier.mistHeight * GroundMist3.LAYER_Z_FRAC[i]!);
      }
    }
    for (let i = 0; i < this.materials.length; i++) {
      const material = this.materials[i]!;
      (material.uniforms.uColor!.value as THREE.Color).setHex(tier.mistColor);
      material.uniforms.uAlpha!.value = tier.mistIntensity * GroundMist3.LAYER_FALLOFF[i]!;
    }
    this.group.visible = tier.mistIntensity > 0;
  }

  /** Keep Mist 3's original noise treatment, but let its weather field extend over the
   * complete visible scene rather than stopping at the board edge. */
  coverViewport(cssW: number, cssH: number, camX: number, camY: number, coverW = cssW, coverH = cssH): void {
    if (!this.group.visible) return;
    for (const mesh of this.meshes) {
      mesh.scale.set(coverW * 1.16, coverH * 1.16, 1);
      mesh.position.x = camX + cssW / 2;
      mesh.position.y = -camY - cssH / 2;
    }
  }

  sync(dt: number, sunLight: THREE.DirectionalLight, speed: number): void {
    for (const material of this.materials) {
      material.uniforms.uTime!.value += dt * speed;
      (material.uniforms.uSunColor!.value as THREE.Color).copy(sunLight.color).multiplyScalar(Math.min(1.5, sunLight.intensity * 0.6));
    }
  }

  dispose(): void {
    this.geo.dispose();
    for (const material of this.materials) material.dispose();
  }
}

// ---------------------------------------------------------------------------------------------
// "Fog 4" — real swirling vortex motion (what the user actually wanted from the vignette
// attempt), but built as world-space geometry keyed to the BOARD's own edges rather than the
// screen's corners, since the CSS vignette approach never worked reliably. Confined to a border
// band around the board's outer edge via a mask baked directly into the shader (smoothstep on
// normalized distance from board center) — the interior, where the actual fighting happens, is
// always completely alpha-zero regardless of intensity, camera position, or pan. Reuses Mist 2's
// exact noise texture technique (proven to render correctly) with an added per-fragment swirl
// rotation for the "spirals/vortices descending" look, instead of Mist 3's raw hash noise.

const MIST4_VERTEX = /* glsl */ `
  varying vec2 vWorldXY;
  void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldXY = worldPos.xy;
    gl_Position = projectionMatrix * viewMatrix * worldPos;
  }
`;

const MIST4_FRAGMENT = /* glsl */ `
  uniform sampler2D uNoiseTex;
  uniform float uTime;
  uniform float uScale;
  uniform vec3 uColor;
  uniform vec3 uSunColor;
  uniform float uAlpha;
  uniform vec2 uBoardCenter;
  uniform vec2 uBoardHalfSize;
  uniform float uBorderThickness;
  varying vec2 vWorldXY;

  float fbm(vec2 uv) {
    return texture2D(uNoiseTex, uv).r * 0.55
         + texture2D(uNoiseTex, uv * 2.3 + 3.1).g * 0.3
         + texture2D(uNoiseTex, uv * 4.7 + 9.4).b * 0.15;
  }

  void main() {
    // THE BUG (first attempt): this used a fraction of the board's own size for the border band,
    // so on a small board the "outer 38%" was still a huge absolute area, swallowing most of the
    // visible battlefield — the opposite of what was asked. Fixed: distFromEdge is measured in
    // real world-px from each edge, and uBorderThickness is a fixed pixel width (tile-scaled, not
    // board-scaled) — the border band stays genuinely thin regardless of how big the map is.
    vec2 distFromCenter = abs(vWorldXY - uBoardCenter);
    vec2 distFromEdge = uBoardHalfSize - distFromCenter; // positive = inside the board
    float minDistFromEdge = min(distFromEdge.x, distFromEdge.y);
    // pow() on top of the smoothstep — a plain smoothstep still reads as a defined edge ("an
    // invisible wall", direct feedback) because most of its ramp happens over a short middle
    // stretch. Squaring pushes the falloff into a long, gradual tail instead, and the final
    // 0.55 multiply caps how strong it ever gets even right at the true edge — weak and natural,
    // not a wall, per direct instruction.
    float rawMask = 1.0 - smoothstep(0.0, uBorderThickness, minDistFromEdge);
    float borderMask = pow(rawMask, 2.2) * 0.55;

    // Swirl: rotate the noise-sampling UV by an angle that depends on distance-from-edge and
    // time — real spiraling motion, strongest right at the edge, fading out with the mask.
    float edgeT = clamp(1.0 - minDistFromEdge / uBorderThickness, 0.0, 1.0);
    float swirlAngle = 2.2 * edgeT * sin(uTime * 0.15 - edgeT * 3.0);
    float s = sin(swirlAngle);
    float c = cos(swirlAngle);
    vec2 p = vWorldXY * uScale;
    vec2 swirled = vec2(p.x * c - p.y * s, p.x * s + p.y * c) + uTime * vec2(0.02, -0.015);

    float n = clamp(fbm(swirled), 0.0, 1.0);
    vec3 color = mix(uColor, uSunColor, 0.35);
    gl_FragColor = vec4(color, n * uAlpha * borderMask);
  }
`;

class GroundMist4 {
  readonly group = new THREE.Group();
  private geo = new THREE.PlaneGeometry(1, 1);
  private noiseTex = buildMistNoiseTexture();
  private material: THREE.ShaderMaterial;
  private mesh: THREE.Mesh;
  private builtKey = "";

  private static readonly BASE_Z = 6;

  constructor() {
    this.material = shaderMaterial({
      vertexShader: MIST4_VERTEX,
      fragmentShader: MIST4_FRAGMENT,
      transparent: true,
      depthWrite: false,
      depthTest: false,
      uniforms: {
        uNoiseTex: { value: this.noiseTex },
        uTime: { value: 0 },
        uScale: { value: 0.004 },
        uColor: { value: new THREE.Color(0xaab4ad) },
        uSunColor: { value: new THREE.Color(0xffffff) },
        uAlpha: { value: 0 },
        uBoardCenter: { value: new THREE.Vector2(0, 0) },
        uBoardHalfSize: { value: new THREE.Vector2(1, 1) },
        uBorderThickness: { value: 1 },
      },
    }, mist4Nodes);
    this.mesh = new THREE.Mesh(this.geo, this.material);
    // Below the character billboards (order 2) so ground mist never paints over a unit.
    this.mesh.renderOrder = 1.5;
    this.group.add(this.mesh);
  }

  rebuild(cols: number, rows: number, tile: number, missionId: string, tier: AtmosphereTier): void {
    const key = `${missionId}:${cols}:${rows}:${tile}`;
    if (key !== this.builtKey) {
      this.builtKey = key;
      const { w, h } = boardSize(cols, rows, tile);
      const cx = w / 2;
      const cy = h / 2;
      // THE BUG: boardSize()'s h includes an empty top-only padding strip (boardPad = tile *
      // BOARD_PAD_MUL, above row 0 — there's no equivalent bottom pad) that isn't real terrain
      // at all. Using cy/h/2 as the mask's vertical center/half-size treated that empty strip as
      // part of the "interior", which is why the border showed up sitting over a void gap
      // instead of hugging the actual edge of the playable hexes ("a whole void hex on top",
      // direct feedback). trueTopY/trueHalfHeightY below are computed from the REAL terrain
      // extent instead, excluding that padding.
      const trueTopY = tile * BOARD_PAD_MUL;
      const trueCenterY = (trueTopY + h) / 2;
      const trueHalfHeightY = (h - trueTopY) / 2;
      // Overscan is much larger than Mist 2/3's 1.15x — the visible fog only ever lives in the
      // outer border band anyway (masked in-shader), so this just needs to comfortably cover
      // that band at any board size, not hug the board tightly.
      this.mesh.scale.set(w * 1.6, h * 1.6, 1);
      this.mesh.position.set(cx, -cy, GroundMist4.BASE_Z + tier.mistHeight * 0.3);
      (this.material.uniforms.uBoardCenter!.value as THREE.Vector2).set(cx, -trueCenterY);
      (this.material.uniforms.uBoardHalfSize!.value as THREE.Vector2).set(w / 2, trueHalfHeightY);
      // Widened from 2.5x to 5x tile — combined with the fragment shader's now-squared falloff
      // curve, this spreads the fade over a much longer, gentler stretch instead of a short,
      // defined band that read as a hard edge. Still scales with zoom (tile), not board size.
      this.material.uniforms.uBorderThickness!.value = tile * 5;
    }
    (this.material.uniforms.uColor!.value as THREE.Color).setHex(tier.mistColor);
    this.material.uniforms.uAlpha!.value = tier.mistIntensity;
    this.group.visible = tier.mistIntensity > 0;
  }

  /** The border mask remains anchored to the real hex-board edges, while its fog plane covers
   * the entire camera view. This also fogs the painted backdrop outside the playable map. */
  coverViewport(cssW: number, cssH: number, camX: number, camY: number, coverW = cssW, coverH = cssH): void {
    if (!this.group.visible) return;
    this.mesh.scale.set(coverW * 1.16, coverH * 1.16, 1);
    this.mesh.position.x = camX + cssW / 2;
    this.mesh.position.y = -camY - cssH / 2;
  }

  sync(dt: number, sunLight: THREE.DirectionalLight, speed: number): void {
    this.material.uniforms.uTime!.value += dt * speed;
    (this.material.uniforms.uSunColor!.value as THREE.Color).copy(sunLight.color).multiplyScalar(Math.min(1.5, sunLight.intensity * 0.6));
  }

  dispose(): void {
    this.geo.dispose();
    this.noiseTex.dispose();
    this.material.dispose();
  }
}

// ---------------------------------------------------------------------------------------------
// "Fog 5" — thin cold ground mist: long feathered strands and shallow veils, mostly clear air.
// Three independent flow layers (own direction, speed, stretch, height parallax) built from
// anisotropic ridged multi-scale noise, so structure is fine filaments rather than blobs. Where
// it collects is driven by a per-hex terrain mask baked from the map (water/woods/ruins/walls and
// decoration cells denser, hills clearer) — see Fog5Density. Non-emissive: a restrained cold grey
// with a slight blue cast, lightly tinted by the scene sun.

const FOG5_RES = 3;
const FOG5_DENSITY: Partial<Record<string, number>> = {
  water: 1,
  woods: 0.85,
  ruins: 0.8,
  column: 0.75,
  nave: 0.6,
  barricade: 0.75,
  ember: 0.55,
  plains: 0.5,
  snow: 0.45,
  door: 0.5,
  flame: 0.3,
  hill: 0.12,
  void: 0,
};

/** Per-hex "how much low ground mist gathers here" mask, blurred so it reads as soft pooling. */
class Fog5Density {
  private texture: THREE.DataTexture | null = null;
  private key = "";

  get tex(): THREE.Texture | null {
    return this.texture;
  }

  update(key: string, engine: BattleEngine, tile: number, boardW: number, boardH: number, cellAt: (x: number, y: number) => number): void {
    if (key === this.key) return;
    this.key = key;
    const cols = engine.cols;
    const rows = engine.rows;
    const cell = new Float32Array(cols * rows);
    for (let i = 0; i < cell.length; i++) cell[i] = FOG5_DENSITY[engine.tiles[i] ?? "plains"] ?? 0.5;
    for (const d of engine.decorations) {
      if (d.x < 0 || d.y < 0 || d.x >= cols || d.y >= rows) continue;
      const i = d.y * cols + d.x;
      cell[i] = Math.min(1, cell[i]! + 0.3);
    }
    const gw = Math.max(1, Math.ceil((boardW / tile) * FOG5_RES));
    const gh = Math.max(1, Math.ceil((boardH / tile) * FOG5_RES));
    const n = gw * gh;
    const field = new Float32Array(n);
    for (let y = 0; y < gh; y++) {
      for (let x = 0; x < gw; x++) {
        const c = cellAt((x + 0.5) / FOG5_RES, (y + 0.5) / FOG5_RES);
        field[y * gw + x] = c < 0 || c >= cell.length ? 0.4 : cell[c]!;
      }
    }
    const tmp = new Float32Array(n);
    for (let pass = 0; pass < 2; pass++) {
      boxBlur(field, tmp, gw, gh, 2, true);
      boxBlur(tmp, field, gw, gh, 2, false);
    }
    const data = new Uint8Array(n * 4);
    for (let i = 0; i < n; i++) {
      const v = Math.round(255 * Math.max(0, Math.min(1, field[i]!)));
      data[i * 4] = v;
      data[i * 4 + 1] = v;
      data[i * 4 + 2] = v;
      data[i * 4 + 3] = 255;
    }
    this.texture?.dispose();
    const tex = new THREE.DataTexture(data, gw, gh, THREE.RGBAFormat, THREE.UnsignedByteType);
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    tex.wrapS = THREE.ClampToEdgeWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    tex.flipY = false; // same row convention as RevealMask: row y is written in plain order
    tex.needsUpdate = true;
    this.texture = tex;
  }

  dispose(): void {
    this.texture?.dispose();
  }
}

const FOG5_VERTEX = /* glsl */ `
  varying vec2 vWorldXY;
  void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldXY = worldPos.xy;
    gl_Position = projectionMatrix * viewMatrix * worldPos;
  }
`;

const FOG5_FRAGMENT = /* glsl */ `
  uniform sampler2D uDensity;
  uniform float uHasDensity;
  uniform vec2 uBoardCenter;
  uniform vec2 uBoardHalfSize;
  uniform float uTime;
  uniform vec2 uCam;
  uniform float uScale;
  uniform vec2 uDrift;
  uniform float uStretch;
  uniform float uAngle;
  uniform float uWarp;
  uniform float uParallax;
  uniform float uSeed;
  uniform vec3 uColor;
  uniform vec3 uLight;
  uniform float uAlpha;
  // Unit feet: xy = foot centre (scene space), z = half-width, w = height of the mist band that
  // veils the unit's boots/lower legs. uFootPass = 1 draws ONLY that veil (on the unit sprite);
  // pass 0 is the ordinary mist, which the depth test hides behind unit silhouettes.
  uniform vec4 uUnits[24];
  uniform int uUnitCount;
  uniform float uFootPass;
  varying vec2 vWorldXY;

  float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
  }
  float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
  }
  float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    mat2 r = mat2(0.8, -0.6, 0.6, 0.8);
    for (int i = 0; i < 4; i++) {
      v += a * vnoise(p);
      p = r * p * 2.05 + 11.7;
      a *= 0.5;
    }
    return v;
  }

  void main() {
    float t = uTime;
    vec2 world = vWorldXY + uCam * uParallax;
    vec2 p = world * uScale + uSeed;
    float ca = cos(uAngle);
    float sa = sin(uAngle);
    p = vec2(p.x * ca - p.y * sa, p.x * sa + p.y * ca);

    // Prevailing drift plus a slower, independent turbulence field bending the strands.
    // Smoke-style tendrils: a doubly domain-warped noise field, then only a thin band around
    // one iso-level of it is drawn. Contours of a warped field become curling, splitting,
    // reconnecting ribbons — mist that swirls instead of straight lines or soft blobs.
    vec2 q = p + uDrift * t;
    q.y *= uStretch * 0.3;
    vec2 w1 = vec2(fbm(q * 0.8 + vec2(0.0, t * 0.03)), fbm(q * 0.8 + vec2(5.2, -t * 0.026)));
    vec2 w2 = vec2(fbm(q + w1 * uWarp * 1.6 + vec2(1.7, 9.2) + t * 0.02),
                   fbm(q + w1 * uWarp * 1.6 + vec2(8.3, 2.8) - t * 0.018));
    float field = fbm(q * 1.3 + w2 * uWarp * 2.2);

    // Two iso-levels at different heights give two families of ribbons that cross and merge.
    float c1 = 1.0 - smoothstep(0.0, 0.085, abs(field - 0.5));
    float c2 = 1.0 - smoothstep(0.0, 0.06, abs(field - 0.62));
    float strand = (c1 * 0.75 + c2 * 0.5);
    // Fine wisp texture inside each ribbon so its edges feather apart.
    strand *= 0.45 + 0.85 * fbm(q * 5.0 + w2 * 3.0);
    strand = clamp(strand, 0.0, 1.0);

    // Barely-there translucent veil hugging the ribbons.
    float veil = smoothstep(0.3, 0.0, abs(field - 0.55)) * 0.08;

    // Mist only exists in intermittent patches; most of the map stays clear.
    float clumps = fbm(p * 0.28 + vec2(t * 0.005, -t * 0.004) + 21.0);

    float density = 0.5;
    if (uHasDensity > 0.5) {
      vec2 uv = vec2(
        (vWorldXY.x - (uBoardCenter.x - uBoardHalfSize.x)) / (2.0 * uBoardHalfSize.x),
        -vWorldXY.y / (2.0 * uBoardHalfSize.y)
      );
      density = texture2D(uDensity, clamp(uv, 0.0, 1.0)).r;
    }
    // Low / cluttered ground lowers the patch threshold (more mist), high ground raises it.
    float thresh = mix(0.58, 0.36, density);
    float pm = smoothstep(thresh, thresh + 0.14, clumps);

    float a = clamp(strand + veil, 0.0, 1.0) * pm;
    if (uFootPass > 0.5) {
      // Mist rises only a little way up each unit: full at the soles, gone by the knees.
      float foot = 0.0;
      for (int i = 0; i < 24; i++) {
        if (i >= uUnitCount) break;
        vec4 u = uUnits[i];
        vec2 d = vWorldXY - u.xy;
        float hx = abs(d.x) / u.z;
        float up = d.y / u.w;
        float m = (1.0 - smoothstep(0.7, 1.2, hx)) * (1.0 - smoothstep(0.0, 1.0, up)) * smoothstep(-0.5, 0.0, up);
        foot = max(foot, m);
      }
      a *= foot * 0.85;
    }
    vec3 color = mix(uColor, uLight, 0.18);
    gl_FragColor = vec4(color, a * uAlpha);
  }
`;

interface Fog5LayerSpec {
  z: number;
  scale: number;
  drift: [number, number];
  stretch: number;
  angle: number;
  warp: number;
  parallax: number;
  seed: number;
  alpha: number;
}

const FOG5_LAYERS: Fog5LayerSpec[] = [
  // Lowest, closest to the ground: slowest, most present.
  { z: 6, scale: 0.0034, drift: [0.07, 0.012], stretch: 5.0, angle: 0.18, warp: 1.2, parallax: 0.0, seed: 0.0, alpha: 0.6 },
  // Slightly higher: crosses the first at a different heading and speed.
  { z: 7, scale: 0.0048, drift: [-0.095, 0.03], stretch: 7.0, angle: -0.42, warp: 1.5, parallax: 0.04, seed: 40.0, alpha: 0.45 },
  // Highest sliver of the shallow volume: finest, faintest, most parallax.
  { z: 8, scale: 0.0068, drift: [0.05, -0.042], stretch: 9.0, angle: 0.95, warp: 1.0, parallax: 0.09, seed: 90.0, alpha: 0.3 },
];

class VolumetricFog5 {
  readonly group = new THREE.Group();
  private readonly geo = new THREE.PlaneGeometry(1, 1);
  private readonly materials: THREE.ShaderMaterial[] = [];
  private readonly meshes: THREE.Mesh[] = [];
  private readonly density = new Fog5Density();
  private builtKey = "";

  private readonly layerOf: number[] = [];
  private readonly unitsUniform = { value: Array.from({ length: 24 }, () => new THREE.Vector4()) };
  private readonly unitCountUniform = { value: 0 };

  // Real depth, using the renderer's existing depth-only "fogCut" silhouettes (units and houses
  // write depth at z=60, see ThreeBattleRenderer's syncUnits). Pass 0 sits at z=50 with the depth
  // test ON, so mist is hidden exactly where a unit stands. Pass 1 sits at z=60 with an EQUAL
  // depth test, so it draws only on those unit silhouettes, and is masked to the feet.
  private static readonly BEHIND_Z = 50;
  private static readonly UNIT_Z = 60;

  constructor() {
    FOG5_LAYERS.forEach((spec, i) => {
      for (const footPass of [0, 1]) {
      const material = shaderMaterial({
        vertexShader: FOG5_VERTEX,
        fragmentShader: FOG5_FRAGMENT,
        transparent: true,
        depthWrite: false,
        depthTest: true,
        depthFunc: footPass ? THREE.EqualDepth : THREE.LessEqualDepth,
        uniforms: {
          uUnits: this.unitsUniform,
          uUnitCount: this.unitCountUniform,
          uFootPass: { value: footPass },
          uDensity: { value: null },
          uHasDensity: { value: 0 },
          uBoardCenter: { value: new THREE.Vector2() },
          uBoardHalfSize: { value: new THREE.Vector2(1, 1) },
          uTime: { value: 0 },
          uCam: { value: new THREE.Vector2() },
          uScale: { value: spec.scale },
          uDrift: { value: new THREE.Vector2(spec.drift[0], spec.drift[1]) },
          uStretch: { value: spec.stretch },
          uAngle: { value: spec.angle },
          uWarp: { value: spec.warp },
          uParallax: { value: spec.parallax },
          uSeed: { value: spec.seed },
          // Cold neutral grey, very slight blue cast.
          uColor: { value: new THREE.Color(0x9aa6ae) },
          uLight: { value: new THREE.Color(0xffffff) },
          uAlpha: { value: 0 },
        },
      }, fog5Nodes);
      const mesh = new THREE.Mesh(this.geo, material);
      // Must draw after the units' fogCut depth writers (renderOrder 99).
      mesh.renderOrder = 101 + i;
      mesh.position.z = footPass ? VolumetricFog5.UNIT_Z : VolumetricFog5.BEHIND_Z;
      this.materials.push(material);
      this.meshes.push(mesh);
      this.layerOf.push(i);
      this.group.add(mesh);
      }
    });
    this.group.visible = false;
  }

  /** Foot positions (scene space) of the units on screen; drives the boots-in-mist veil. */
  setUnits(feet: { x: number; y: number; halfW: number; band: number }[]): void {
    const n = Math.min(24, feet.length);
    for (let i = 0; i < n; i++) {
      const f = feet[i]!;
      this.unitsUniform.value[i]!.set(f.x, f.y, Math.max(1, f.halfW), Math.max(1, f.band));
    }
    this.unitCountUniform.value = n;
  }

  rebuild(cols: number, rows: number, tile: number, missionId: string, tier: AtmosphereTier): void {
    const key = `${missionId}:${cols}:${rows}:${tile}`;
    if (key !== this.builtKey) {
      this.builtKey = key;
      const { w, h } = boardSize(cols, rows, tile);
      for (const m of this.materials) {
        (m.uniforms.uBoardCenter!.value as THREE.Vector2).set(w / 2, -h / 2);
        (m.uniforms.uBoardHalfSize!.value as THREE.Vector2).set(w / 2, h / 2);
      }
      // Default footprint (covers the board) for when no viewport is supplied; coverViewport
      // overrides it every frame during real battles.
      for (const mesh of this.meshes) {
        mesh.scale.set(w * 1.3, h * 1.3, 1);
        mesh.position.x = w / 2;
        mesh.position.y = -h / 2;
      }
    }
    this.group.visible = tier.mistIntensity > 0;
    this.materials.forEach((m, i) => {
      // sqrt so the editor's default/low slider values still show; other fogs square theirs.
      m.uniforms.uAlpha!.value = FOG5_LAYERS[this.layerOf[i]!]!.alpha * Math.sqrt(tier.mistIntensity);
    });
  }

  /** Bakes the terrain-driven density mask; only rebuilds when the map itself changes. */
  syncDensity(engine: BattleEngine, tile: number, cellAt: (x: number, y: number) => number): void {
    if (!this.group.visible) return;
    const { w, h } = boardSize(engine.cols, engine.rows, tile);
    this.density.update(`${engine.mission.id}:${engine.cols}x${engine.rows}:${tile}:${engine.decorations.length}`, engine, tile, w, h, cellAt);
    for (const m of this.materials) {
      m.uniforms.uDensity!.value = this.density.tex;
      m.uniforms.uHasDensity!.value = this.density.tex ? 1 : 0;
    }
  }

  coverViewport(cssW: number, cssH: number, camX: number, camY: number, coverW = cssW, coverH = cssH): void {
    if (!this.group.visible) return;
    for (let i = 0; i < this.meshes.length; i++) {
      const mesh = this.meshes[i]!;
      mesh.scale.set(coverW * 1.16, coverH * 1.16, 1);
      mesh.position.x = camX + cssW / 2;
      mesh.position.y = -camY - cssH / 2;
      (this.materials[i]!.uniforms.uCam!.value as THREE.Vector2).set(camX, -camY);
    }
  }

  sync(dt: number, sunLight: THREE.DirectionalLight, speed: number): void {
    if (!this.group.visible) return;
    for (const material of this.materials) {
      material.uniforms.uTime!.value += dt * speed;
      (material.uniforms.uLight!.value as THREE.Color).copy(sunLight.color).multiplyScalar(Math.min(1.2, sunLight.intensity * 0.5));
    }
  }

  dispose(): void {
    this.geo.dispose();
    this.density.dispose();
    for (const material of this.materials) material.dispose();
  }
}

// ---------------------------------------------------------------------------------------------
// "Vinheta 3" — a full-viewport painted fog field that exists ONLY outside the board. The board
// is cut out in the fragment shader, leaving the tactical action clean while the surrounding
// painted backdrop receives the atmosphere.

const EXTERIOR_FOG_VERTEX = /* glsl */ `
  varying vec2 vWorldXY;
  varying vec2 vUv;
  void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldXY = worldPos.xy;
    vUv = uv;
    gl_Position = projectionMatrix * viewMatrix * worldPos;
  }
`;

const EXTERIOR_FOG_FRAGMENT = /* glsl */ `
  uniform sampler2D uFog;
  uniform float uOpacity;
  uniform float uTime;
  uniform vec2 uBoardCenter;
  uniform vec2 uBoardHalfSize;
  varying vec2 vWorldXY;
  varying vec2 vUv;
  void main() {
    vec2 over = abs(vWorldXY - uBoardCenter) - uBoardHalfSize;
    // Outside of the board rectangle is 1; a small feather prevents a hard cut at its edge.
    float exterior = smoothstep(-14.0, 14.0, max(over.x, over.y));
    // One artwork layer only. This is a tiny non-repeating drift within the source image, not a
    // tiled/repeated texture; the board cutout below remains fixed and fully transparent.
    vec2 driftedUv = vUv + vec2(sin(uTime * 0.11), cos(uTime * 0.08)) * 0.006;
    vec4 fog = texture2D(uFog, driftedUv);
    gl_FragColor = vec4(fog.rgb, fog.a * uOpacity * exterior);
  }
`;

class BoardFogArtwork {
  readonly group = new THREE.Group();
  private readonly geometry = new THREE.PlaneGeometry(1, 1);
  private readonly texture = new THREE.TextureLoader().load("/game/assets/vinheta-3-fog.png");
  private readonly material: THREE.ShaderMaterial;
  private readonly mesh: THREE.Mesh;
  private builtKey = "";

  constructor() {
    this.texture.colorSpace = THREE.SRGBColorSpace;
    this.texture.minFilter = THREE.LinearFilter;
    this.texture.magFilter = THREE.LinearFilter;
    this.material = shaderMaterial({
      vertexShader: EXTERIOR_FOG_VERTEX,
      fragmentShader: EXTERIOR_FOG_FRAGMENT,
      transparent: true,
      depthWrite: false,
      depthTest: false,
      uniforms: {
        uFog: { value: this.texture },
        uOpacity: { value: 0 },
        uTime: { value: 0 },
        uBoardCenter: { value: new THREE.Vector2() },
        uBoardHalfSize: { value: new THREE.Vector2(1, 1) },
      },
    }, exteriorFogNodes);
    this.mesh = new THREE.Mesh(this.geometry, this.material);
    this.mesh.renderOrder = 30;
    this.group.add(this.mesh);
  }

  rebuild(cols: number, rows: number, tile: number, missionId: string, intensity: number): void {
    const key = `${missionId}:${cols}:${rows}:${tile}`;
    if (key !== this.builtKey) {
      this.builtKey = key;
      const { w, h } = boardSize(cols, rows, tile);
      (this.material.uniforms.uBoardCenter!.value as THREE.Vector2).set(w / 2, -h / 2);
      (this.material.uniforms.uBoardHalfSize!.value as THREE.Vector2).set(w / 2, h / 2);
    }
    this.material.uniforms.uOpacity!.value = intensity;
    this.group.visible = intensity > 0;
  }

  coverViewport(cssW: number, cssH: number, camX: number, camY: number, coverW = cssW, coverH = cssH): void {
    if (!this.group.visible) return;
    this.mesh.scale.set(coverW * 1.16, coverH * 1.16, 1);
    this.mesh.position.set(camX + cssW / 2, -camY - cssH / 2, 8);
  }

  sync(dt: number, speed: number): void {
    this.material.uniforms.uTime!.value += dt * speed;
  }

  dispose(): void {
    this.geometry.dispose();
    this.material.dispose();
    this.texture.dispose();
  }
}

// ---------------------------------------------------------------------------------------------
// "Fog 01" — Fog 2's own smooth drifting mist, but masked by the party's fog-of-war: it only
// ever shows over ground they haven't revealed yet, plus the exterior backdrop past the board's
// edge (reusing BoardFogArtwork's own board-rectangle test for that half). Real explored/visible
// terrain stays completely clear underneath it, so it never washes out ground you can already see.

const REVEAL_RES = 3;
const REVEAL_BLUR_R = 2;
const REVEAL_BLUR_PASSES = 2;

/** Per-hex "has the party revealed this ground" mask, baked into a small DataTexture — same
 * board-local-grid idea ThreeFogMask.ts uses for the real fog-of-war sheet (same `cellAt`
 * callback, same per-hex state lookup), but coarser (RES=3, not 8) since this only ever feeds
 * a soft, already-blurred atmospheric layer, never a crisp gameplay edge — and sampled by hand
 * in the fragment shader (see REVEAL_FOG_FRAGMENT's revealUv) instead of through a plain mesh
 * UV, so `flipY` is turned off here and the data written in plain row order: no implicit "which
 * convention is this texture in" to get backwards. Deliberately sized to boardSize()'s own
 * (w,h) rather than ThreeFogMask's slightly larger padded grid, so its UV mapping can share
 * BoardFogArtwork's uBoardCenter/uBoardHalfSize uniforms instead of tracking a second,
 * mismatched rectangle. */
class RevealMask {
  private texture: THREE.DataTexture | null = null;
  private key = "";

  get tex(): THREE.Texture | null {
    return this.texture;
  }

  update(
    key: string,
    tile: number,
    boardW: number,
    boardH: number,
    cellAt: (x: number, y: number) => number,
    revealedAt: (index: number) => boolean,
  ): void {
    if (key === this.key) return;
    this.key = key;
    const gw = Math.max(1, Math.ceil((boardW / tile) * REVEAL_RES));
    const gh = Math.max(1, Math.ceil((boardH / tile) * REVEAL_RES));
    const n = gw * gh;
    let alpha = new Float32Array(n);
    for (let y = 0; y < gh; y++) {
      for (let x = 0; x < gw; x++) {
        const c = cellAt((x + 0.5) / REVEAL_RES, (y + 0.5) / REVEAL_RES);
        alpha[y * gw + x] = c < 0 || !revealedAt(c) ? 1 : 0;
      }
    }
    const tmp = new Float32Array(n);
    for (let pass = 0; pass < REVEAL_BLUR_PASSES; pass++) {
      boxBlur(alpha, tmp, gw, gh, REVEAL_BLUR_R, true);
      boxBlur(tmp, alpha, gw, gh, REVEAL_BLUR_R, false);
    }
    const data = new Uint8Array(n * 4);
    for (let y = 0; y < gh; y++) {
      for (let x = 0; x < gw; x++) {
        const v = Math.round(255 * alpha[y * gw + x]!);
        // No row flip here (unlike ThreeFogMask): flipY below is explicitly off, so texel row
        // y (source y, increasing = further down the y-down world) samples at v = y/gh
        // directly — see the fragment shader's revealUv derivation from -vWorldXY.y.
        const j = (y * gw + x) * 4;
        data[j] = v;
        data[j + 1] = v;
        data[j + 2] = v;
        data[j + 3] = 255;
      }
    }
    this.texture?.dispose();
    const tex = new THREE.DataTexture(data, gw, gh, THREE.RGBAFormat, THREE.UnsignedByteType);
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    tex.wrapS = THREE.ClampToEdgeWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    // Explicit and load-bearing: Texture.flipY defaults to true, which would silently reverse
    // the row order a second time on upload (on top of the deliberate CPU-side layout above),
    // sampling every hex's reveal state from its mirror-image row instead of its own.
    tex.flipY = false;
    tex.needsUpdate = true;
    this.texture = tex;
    alpha = new Float32Array(0);
  }

  dispose(): void {
    this.texture?.dispose();
  }
}

const REVEAL_FOG_VERTEX = /* glsl */ `
  varying vec2 vWorldXY;
  void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldXY = worldPos.xy;
    gl_Position = projectionMatrix * viewMatrix * worldPos;
  }
`;

const REVEAL_FOG_FRAGMENT = /* glsl */ `
  uniform sampler2D uNoiseTex;
  uniform sampler2D uReveal;
  uniform float uHasReveal;
  uniform float uTime;
  uniform float uScale;
  uniform vec2 uDrift;
  uniform vec3 uColor;
  uniform vec3 uSunColor;
  uniform float uAlpha;
  uniform vec2 uBoardCenter;
  uniform vec2 uBoardHalfSize;
  varying vec2 vWorldXY;

  float fbm(vec2 uv) {
    return texture2D(uNoiseTex, uv).r * 0.55
         + texture2D(uNoiseTex, uv * 2.3 + 3.1).g * 0.3
         + texture2D(uNoiseTex, uv * 4.7 + 9.4).b * 0.15;
  }

  void main() {
    vec2 uv = vWorldXY * uScale + uTime * uDrift;
    // Two independent octaves multiplied together, not just one fbm sample like Fog 2 — more
    // depth/variation for the same cost (still only texture fetches, no new noise math).
    float n1 = fbm(uv);
    float n2 = fbm(uv * -1.6 + 7.0 - uTime * uDrift * 0.35);
    float n = clamp(n1 * 0.6 + n1 * n2 * 0.9, 0.0, 1.0);

    vec2 over = abs(vWorldXY - uBoardCenter) - uBoardHalfSize;
    float exterior = smoothstep(-14.0, 14.0, max(over.x, over.y));
    // uReveal's texel row y was written directly (no flip, flipY off — see RevealMask), so
    // v = 0 is the y-down world's TOP (scene Y = 0) and v = 1 is its BOTTOM (scene Y = -h);
    // scene Y is already negated from the y-down "wy" the mask was built from, hence -vWorldXY.y.
    vec2 revealUv = vec2(
      (vWorldXY.x - (uBoardCenter.x - uBoardHalfSize.x)) / (2.0 * uBoardHalfSize.x),
      -vWorldXY.y / (2.0 * uBoardHalfSize.y)
    );
    float unrevealed = uHasReveal > 0.5 ? texture2D(uReveal, revealUv).r : 1.0;
    // Only ever shows past the board's edge (the exterior backdrop) or over ground the party
    // hasn't revealed yet — real explored/visible terrain stays completely clear.
    float mask = max(exterior, unrevealed);

    vec3 color = mix(uColor, uSunColor, 0.35);
    gl_FragColor = vec4(color, n * uAlpha * mask);
  }
`;

class RevealFog {
  readonly group = new THREE.Group();
  private readonly geometry = new THREE.PlaneGeometry(1, 1);
  private readonly noiseTex = buildMistNoiseTexture();
  private readonly reveal = new RevealMask();
  private readonly material: THREE.ShaderMaterial;
  private readonly mesh: THREE.Mesh;
  private builtKey = "";

  constructor() {
    this.material = shaderMaterial({
      vertexShader: REVEAL_FOG_VERTEX,
      fragmentShader: REVEAL_FOG_FRAGMENT,
      transparent: true,
      depthWrite: false,
      depthTest: false,
      uniforms: {
        uNoiseTex: { value: this.noiseTex },
        uReveal: { value: null },
        uHasReveal: { value: 0 },
        uTime: { value: 0 },
        uScale: { value: 0.0032 },
        uDrift: { value: new THREE.Vector2(0.05, 0.08) },
        uColor: { value: new THREE.Color(0xaab4ad) },
        uSunColor: { value: new THREE.Color(0xffffff) },
        uAlpha: { value: 0 },
        uBoardCenter: { value: new THREE.Vector2() },
        uBoardHalfSize: { value: new THREE.Vector2(1, 1) },
      },
    }, revealFogNodes);
    this.mesh = new THREE.Mesh(this.geometry, this.material);
    // This layer is the atmosphere that softens the game's completely opaque unseen fill.
    // ThreeFogMask draws at order 100; placing Fog 01 behind it made the shader technically
    // correct but entirely invisible over every unrevealed hex. Draw above that mask so the
    // fog reads over undiscovered terrain/background, while the reveal texture still keeps
    // every explored or visible hex completely clear.
    this.mesh.renderOrder = 101;
    this.group.add(this.mesh);
  }

  rebuild(cols: number, rows: number, tile: number, missionId: string, tier: AtmosphereTier): void {
    const key = `${missionId}:${cols}:${rows}:${tile}`;
    if (key !== this.builtKey) {
      this.builtKey = key;
      const { w, h } = boardSize(cols, rows, tile);
      (this.material.uniforms.uBoardCenter!.value as THREE.Vector2).set(w / 2, -h / 2);
      (this.material.uniforms.uBoardHalfSize!.value as THREE.Vector2).set(w / 2, h / 2);
    }
    const shapedIntensity = tier.mistIntensity * tier.mistIntensity;
    (this.material.uniforms.uColor!.value as THREE.Color).setHex(tier.mistColor);
    this.material.uniforms.uAlpha!.value = shapedIntensity;
    this.group.visible = tier.mistIntensity > 0;
  }

  /** Rebuilds the reveal mask only when the party's explored state actually changed
   * (visVersion) — same rebuild-gating idea as ThreeFogMask. Skipped (mask left fully "show
   * fog everywhere") on a mission with no fog-of-war, so Fog 01 without fog just looks like a
   * plain Fog 2 covering the whole board. */
  syncReveal(engine: BattleEngine, tile: number, cellAt: (x: number, y: number) => number): void {
    if (!this.group.visible) return;
    if (!engine.fogged) {
      this.material.uniforms.uHasReveal!.value = 0;
      return;
    }
    const { w, h } = boardSize(engine.cols, engine.rows, tile);
    const key = `${engine.mission.id}:${engine.cols}x${engine.rows}:${engine.visVersion}`;
    this.reveal.update(key, tile, w, h, cellAt, (i) => {
      const x = i % engine.cols;
      const y = (i - x) / engine.cols;
      return engine.explored(x, y);
    });
    this.material.uniforms.uReveal!.value = this.reveal.tex;
    this.material.uniforms.uHasReveal!.value = 1;
  }

  coverViewport(cssW: number, cssH: number, camX: number, camY: number, coverW = cssW, coverH = cssH): void {
    if (!this.group.visible) return;
    this.mesh.scale.set(coverW * 1.16, coverH * 1.16, 1);
    this.mesh.position.set(camX + cssW / 2, -camY - cssH / 2, 8);
  }

  sync(dt: number, sunLight: THREE.DirectionalLight, speed: number): void {
    this.material.uniforms.uTime!.value += dt * speed;
    (this.material.uniforms.uSunColor!.value as THREE.Color).copy(sunLight.color).multiplyScalar(Math.min(1.5, sunLight.intensity * 0.6));
  }

  dispose(): void {
    this.geometry.dispose();
    this.material.dispose();
    this.noiseTex.dispose();
    this.reveal.dispose();
  }
}

// ---------------------------------------------------------------------------------------------
// Drift particles: GPU-driven InstancedMesh quads. Position is computed per-vertex on the GPU
// from a per-instance world-anchor + seed and the single uTime uniform — the only per-frame CPU
// work is updating that one float (plus a cheap CPU Color.lerp for light-tinting, see sync()).
// This is deliberately NOT the CPU pooled-particle pattern gfx/particles.ts uses for spell FX
// (mesh.setMatrixAt() every frame for thousands of instances would be a CPU bottleneck, not a
// GPU one) — instanced attributes + a GPU drift function is the actual "spend the GPU, not the
// CPU" approach the user asked for.

const PARTICLE_FRAGMENT = /* glsl */ `
  uniform vec3 uColor;
  uniform float uOpacity;
  varying vec2 vUv;
  varying float vAlpha;
  void main() {
    float d = distance(vUv, vec2(0.5));
    float mask = smoothstep(0.5, 0.15, d);
    gl_FragColor = vec4(uColor, mask * vAlpha * uOpacity);
  }
`;

const DUST_VERTEX = /* glsl */ `
  attribute vec2 aBase;
  attribute float aSeed;
  attribute float aSize;
  uniform float uTime;
  varying vec2 vUv;
  varying float vAlpha;
  void main() {
    vUv = uv;
    float t = uTime + aSeed * 37.0;
    vec2 wander = vec2(sin(t * 0.35 + aSeed * 6.0), cos(t * 0.27 + aSeed * 9.0)) * 9.0;
    float z = 6.0 + sin(t * 0.5 + aSeed * 3.0) * 3.0;
    // This camera is orthographic with zero perspective (see module header) — height alone
    // produces NO visible size/position cue on its own, a mote at z=3 looks pixel-identical to
    // one at z=9 otherwise. zNorm fakes the depth cue instead: dimmer/smaller near the ground,
    // brighter/bigger near the top of its float range, so height actually reads as height.
    float zNorm = clamp((z - 3.0) / 6.0, 0.0, 1.0);
    vec3 worldPos = vec3(aBase + wander, z);
    vec4 center = modelViewMatrix * vec4(worldPos, 1.0);
    float worldScale = length(modelMatrix[0].xyz);
    center.xy += position.xy * aSize * mix(0.55, 1.0, zNorm) * worldScale;
    gl_Position = projectionMatrix * center;
    vAlpha = mix(0.3, 1.0, zNorm) * (0.5 + 0.5 * sin(t * 0.6 + aSeed * 5.0));
  }
`;

const EMBER_VERTEX = /* glsl */ `
  attribute vec2 aBase;
  attribute float aSeed;
  attribute float aSize;
  uniform float uTime;
  uniform float uRiseHeight;
  varying vec2 vUv;
  varying float vAlpha;
  void main() {
    vUv = uv;
    // Baseline pace for uSpeed/wispSpeed = 1.0 (see ParticleField.sync/Mission.wispSpeed) — a
    // ~64s cycle (the previous value) looked frozen over a normal glance, and the original
    // 14.0 read as darting; this is the middle ground: clearly, gently drifting. The editor's
    // "Velocidade" slider scales this up or down from here — no shader edit needed to retune.
    const float RISE_SPEED = 6.0;
    float cycle = uRiseHeight / RISE_SPEED;
    float t = mod(uTime * RISE_SPEED + aSeed * cycle, cycle);
    float rise01 = t / cycle;
    float z = 4.0 + t;
    vec2 sway = vec2(sin(uTime * 0.4 + aSeed * 10.0), cos(uTime * 0.32 + aSeed * 7.0)) * 6.0;
    vec3 worldPos = vec3(aBase + sway, z);
    // Same "fake the height cue" reasoning as dust's zNorm — an ember starts small/dim at the
    // ground and swells as it climbs, echoing a real spark catching more open air, instead of
    // popping into existence as a full-size dot with no sense of where it started.
    float sizeMul = mix(0.5, 1.2, smoothstep(0.0, 0.55, rise01));
    // The parent converts Ember's pixel coordinates and Z-up into the battle world.
    // Offset in camera space so the card keeps its shape in tactical and normal views.
    vec4 center = modelViewMatrix * vec4(worldPos, 1.0);
    float worldScale = length(modelMatrix[0].xyz);
    center.xy += position.xy * aSize * sizeMul * worldScale;
    gl_Position = projectionMatrix * center;
    // smoothstep both ends so the loop-back to the ground is invisible, not a pop.
    vAlpha = smoothstep(0.0, 0.08, rise01) * smoothstep(1.0, 0.85, rise01);
  }
`;

type ParticleKind = "dust" | "ember";

class ParticleField {
  readonly group = new THREE.Group();
  private geo: THREE.PlaneGeometry | null = null;
  private material: THREE.ShaderMaterial | null = null;
  private mesh: THREE.InstancedMesh | null = null;
  private builtKey = "";
  private readonly baseColor: THREE.Color;
  /** Set via setBloomLayer() (see ThreeBattleRenderer's selective-bloom setup) — applied to the
   * mesh at creation time here in rebuild(), and re-applied every time rebuild() makes a NEW
   * mesh (mission/board change), since a fresh InstancedMesh starts on the default layer only. */
  private bloomLayer: number | null = null;

  constructor(private readonly kind: ParticleKind) {
    this.baseColor = kind === "ember" ? new THREE.Color(0xffa552) : new THREE.Color(0xd6d2bf);
  }

  setBloomLayer(layer: number): void {
    this.bloomLayer = layer;
    this.mesh?.layers.enable(layer);
  }

  private rebuild(boardW: number, boardH: number, tile: number, count: number, riseHeight: number): void {
    this.teardown();
    if (count <= 0) return;

    const geo = new THREE.PlaneGeometry(1, 1);
    const aBase = new Float32Array(count * 2);
    const aSeed = new Float32Array(count);
    const aSize = new Float32Array(count);
    // Wisps are atmosphere, not a board-only gameplay marker. Give the ember field a generous
    // margin so some of them drift over the surrounding painted space as well as the map.
    const exteriorPad = this.kind === "ember" ? tile * 5 : 0;
    for (let i = 0; i < count; i++) {
      let x: number;
      let y: number;
      if (this.kind === "ember" && Math.random() < 0.72) {
        // Pick from the expanded rectangle, rejecting the board's own interior. This gives the
        // exterior its visibly denser field without duplicating any particle or using a frame.
        do {
          x = -exteriorPad + Math.random() * (boardW + exteriorPad * 2);
          y = exteriorPad - Math.random() * (boardH + exteriorPad * 2);
        } while (x >= 0 && x <= boardW && y <= 0 && y >= -boardH);
      } else {
        x = Math.random() * boardW;
        y = -Math.random() * boardH;
      }
      aBase[i * 2] = x;
      aBase[i * 2 + 1] = y; // pre-negated — see module Y-convention note
      aSeed[i] = Math.random() * 1000;
      // Small — the earlier 0.16-0.38x tile sizing at thousands of instances produced a field of
      // uniformly bright dots that read as a 2D confetti overlay, not ambient atmosphere (see
      // ThreeAtmosphere.ts's module comment history / user feedback). Kept modest on purpose.
      aSize[i] = tile * (this.kind === "ember" ? 0.07 + Math.random() * 0.06 : 0.05 + Math.random() * 0.06);
    }
    geo.setAttribute("aBase", new THREE.InstancedBufferAttribute(aBase, 2));
    geo.setAttribute("aSeed", new THREE.InstancedBufferAttribute(aSeed, 1));
    geo.setAttribute("aSize", new THREE.InstancedBufferAttribute(aSize, 1));

    const material = shaderMaterial({
      vertexShader: this.kind === "ember" ? EMBER_VERTEX : DUST_VERTEX,
      fragmentShader: PARTICLE_FRAGMENT,
      transparent: true,
      depthWrite: false,
      depthTest: false,
      blending: this.kind === "ember" ? THREE.AdditiveBlending : THREE.NormalBlending,
      uniforms: {
        uTime: { value: 0 },
        uColor: { value: this.baseColor.clone() },
        uOpacity: { value: this.kind === "ember" ? 0.9 : 0.5 },
        ...(this.kind === "ember" ? { uRiseHeight: { value: riseHeight } } : {}),
      },
    }, this.kind === "ember" ? emberNodes : dustNodes);

    const mesh = new THREE.InstancedMesh(geo, material, count);
    // Anchors and drift live in custom attributes, outside the PlaneGeometry's bounds.
    // Three's automatic bounding sphere therefore cannot enclose the particle field.
    mesh.frustumCulled = false;
    mesh.renderOrder = this.kind === "ember" ? 14 : 13;
    // instanceMatrix is never read by our custom vertex shaders (no <project_vertex> chunk, no
    // reference to it) — filled with identity anyway as cheap, one-time insurance rather than
    // leaving it zero-initialized.
    const identity = new THREE.Matrix4();
    for (let i = 0; i < count; i++) mesh.setMatrixAt(i, identity);
    mesh.instanceMatrix.needsUpdate = true;
    if (this.bloomLayer !== null) mesh.layers.enable(this.bloomLayer);

    this.geo = geo;
    this.material = material;
    this.mesh = mesh;
    this.group.add(mesh);
  }

  sync(
    boardW: number,
    boardH: number,
    tile: number,
    rebuildKey: string,
    count: number,
    riseHeight: number,
    dt: number,
    lightColor: THREE.Color,
    speed: number,
    fixedColor: THREE.Color | null,
  ): void {
    const fullKey = `${rebuildKey}:${count}:${riseHeight.toFixed(2)}`;
    if (fullKey !== this.builtKey) {
      this.builtKey = fullKey;
      this.rebuild(boardW, boardH, tile, count, riseHeight);
    }
    if (!this.material) return;
    // Every time-driven motion in the vertex shaders (rise cycle, sway) derives from uTime alone
    // — scaling how fast uTime itself accumulates scales all of it together, so a "speed" dial
    // needs no shader change, just a different dt multiplier here.
    this.material.uniforms.uTime!.value += dt * speed;
    const uColor = this.material.uniforms.uColor!.value as THREE.Color;
    // Fixed color, no automatic light-mixing, on direct instruction ("pick a fucking color or
    // allow me to pick, not mixing") — only falls back to the old light-lerp tint when no fixed
    // color is given (dust, not currently exposed to the editor).
    if (fixedColor) uColor.copy(fixedColor);
    else uColor.copy(this.baseColor).lerp(lightColor, 0.3);
  }

  private teardown(): void {
    if (this.mesh) this.group.remove(this.mesh);
    this.geo?.dispose();
    this.material?.dispose();
    this.geo = null;
    this.material = null;
    this.mesh = null;
  }

  dispose(): void {
    this.teardown();
  }
}

// ---------------------------------------------------------------------------------------------

export class ThreeAtmosphere {
  readonly group = new THREE.Group();

  /** Marks the wisp embers (and only them) as bloom-eligible — see ThreeBattleRenderer's
   * selective-bloom setup. Mist/dust/tiles/decor/units/the movement-highlight overlay must
   * NEVER bloom: the active-turn ring's existing intentional alpha "breathing" pulse (see
   * engine.ts's activeTurnHighlight) crossed a naive full-scene bloom's brightness threshold on
   * every cycle, turning a gentle pulse into a hard on/off blink — direct user complaint. */
  markBloomLayer(layer: number): void {
    this.embers.setBloomLayer(layer);
  }

  private readonly mist2 = new GroundMist();
  private readonly mist3 = new GroundMist3();
  private readonly mist4 = new GroundMist4();
  private readonly fog5 = new VolumetricFog5();
  private readonly revealFog = new RevealFog();
  private readonly fogArtwork = new BoardFogArtwork();
  private readonly dust = new ParticleField("dust");
  private readonly embers = new ParticleField("ember");
  private readonly scratchSunColor = new THREE.Color();
  private readonly scratchHemiColor = new THREE.Color();
  private readonly scratchWispColor = new THREE.Color();

  constructor() {
    this.group.add(
      this.mist2.group,
      this.mist3.group,
      this.mist4.group,
      this.fog5.group,
      this.revealFog.group,
      this.fogArtwork.group,
      this.dust.group,
      this.embers.group,
    );
  }

  sync(
    engine: BattleEngine,
    tile: number,
    dt: number,
    sunLight: THREE.DirectionalLight,
    hemiLight: THREE.HemisphereLight,
    viewport?: { cssW: number; cssH: number; camX: number; camY: number },
    cellAt?: (x: number, y: number) => number,
    unitFeet?: { x: number; y: number; halfW: number; band: number }[],
    enabled = true,
  ): void {
    this.group.visible = enabled;
    if (!enabled) return;
    // Mission-authored, not a hardcoded per-id table (see Mission.mistIntensity/wispIntensity/
    // wispSpeed in types.ts) — the Map Editor's "Névoa"/"Wisps"/"Velocidade" sliders are the one
    // real source of this. Full range, deliberately: sliders go from "off" to genuinely extreme
    // on purpose (per direct instruction — max means max), not pre-limited "for their own good".
    // Dust stays off (count 0) for now — only wisps (embers) were asked for; dust's plumbing is
    // left in place, unused, for whenever it is.
    // Defaults match the user's own tuned "O Vau" setup (vau016.json) — the standard daytime
    // look for every mission that doesn't set its own values, per direct instruction.
    const wisp = engine.mission.wispIntensity ?? 0.02;
    const wispSpeed = engine.mission.wispSpeed ?? 1;
    const mistSpeed = engine.mission.mistSpeed ?? 1;
    // "vignette" mistType means neither world-space mist implementation should render at all —
    // that look comes entirely from BattleCanvas.tsx's screen-space CSS vignette instead.
    // "none" is an explicit author override: no mist, world-space or screen-space, whatever
    // mistIntensity is set to.
    const mistType = engine.mission.mistType ?? "none";
    // "none" ("Sem névoa") is a full atmosphere kill-switch, not just the mist layer — per
    // direct correction, it means everything off: mist, vignette, AND wisps/embers, whatever
    // their own sliders are set to.
    const noFog = mistType === "none";
    const worldMistIntensity =
      noFog || mistType === "vignette" || mistType === "vignette2" || mistType === "vignette3" || mistType === "vignette4" ? 0 : (engine.mission.mistIntensity ?? 0.2);
    const tier: AtmosphereTier = {
      // Plain default, not a forced floor — a floor would override an explicit 0 the author
      // deliberately set to turn mist off on a specific map ("if I don't want it somewhere I'll
      // remove it myself"), which defeats the point of a real per-map control. The editor's
      // blankDraft/missionToDraft (GameApp.tsx) now default the slider itself to 0.5 for any
      // mission that's never explicitly set this, so "on everywhere by default" comes from
      // the actual authored/displayed value, not a hidden runtime override fighting it.
      mistIntensity: worldMistIntensity,
      mistHeight: 40,
      mistColor: 0xaab4ad,
      dustCount: 0,
      emberCount: noFog ? 0 : Math.round(wisp * MAX_EMBER_COUNT),
      emberRiseHeight: 90,
    };
    const key = `${engine.mission.id}:${engine.cols}:${engine.rows}:${tile}`;
    const { w: boardW, h: boardH } = boardSize(engine.cols, engine.rows, tile);

    // Only the selected implementation gets a nonzero tier — the other's own rebuild() sees
    // mistIntensity <= 0 via a zeroed-out copy and tears itself down/stays hidden, the same as
    // if the author had just set the slider to 0 on that one.
    // The tactics camera turns and tilts the board, so a ground sheet sized to the screen no
    // longer fills it (hard diagonal edges). Size it to the screen's footprint on the ground,
    // using the same pitch clamp as ThreeBattleRenderer.updateCamera.
    let coverW = viewport?.cssW ?? 0, coverH = viewport?.cssH ?? 0;
    if (viewport && engine.tacticsCamera) {
      const pitch = THREE.MathUtils.degToRad(THREE.MathUtils.clamp(engine.cameraTilt, 0, 55));
      const azimuth = THREE.MathUtils.degToRad(engine.cameraTiltSide);
      const groundH = viewport.cssH / Math.cos(pitch);
      const cosA = Math.abs(Math.cos(azimuth)), sinA = Math.abs(Math.sin(azimuth));
      coverW = viewport.cssW * cosA + groundH * sinA;
      coverH = viewport.cssW * sinA + groundH * cosA;
    }
    const mist2Tier: AtmosphereTier = { ...tier, mistIntensity: mistType === "mist2" ? worldMistIntensity : 0 };
    const mist3Tier: AtmosphereTier = { ...tier, mistIntensity: mistType === "mist3" ? worldMistIntensity : 0 };
    const mist4Tier: AtmosphereTier = { ...tier, mistIntensity: mistType === "mist4" ? worldMistIntensity : 0 };
    const fog1Tier: AtmosphereTier = { ...tier, mistIntensity: mistType === "fog1" ? worldMistIntensity : 0 };
    this.mist2.rebuild(engine.cols, engine.rows, tile, engine.mission.id, mist2Tier);
    if (mistType === "mist2" && viewport) this.mist2.coverViewport(viewport.cssW, viewport.cssH, viewport.camX, viewport.camY, coverW, coverH);
    this.mist2.sync(dt, sunLight, mistSpeed);
    this.mist3.rebuild(engine.cols, engine.rows, tile, engine.mission.id, mist3Tier);
    if (mistType === "mist3" && viewport) this.mist3.coverViewport(viewport.cssW, viewport.cssH, viewport.camX, viewport.camY, coverW, coverH);
    this.mist3.sync(dt, sunLight, mistSpeed);
    this.mist4.rebuild(engine.cols, engine.rows, tile, engine.mission.id, mist4Tier);
    if (mistType === "mist4" && viewport) this.mist4.coverViewport(viewport.cssW, viewport.cssH, viewport.camX, viewport.camY, coverW, coverH);
    this.mist4.sync(dt, sunLight, mistSpeed);
    this.fog5.rebuild(engine.cols, engine.rows, tile, engine.mission.id, { ...tier, mistIntensity: mistType === "fog5" ? worldMistIntensity : 0 });
    if (mistType === "fog5" && viewport) this.fog5.coverViewport(viewport.cssW, viewport.cssH, viewport.camX, viewport.camY, coverW, coverH);
    if (mistType === "fog5" && cellAt) this.fog5.syncDensity(engine, tile, cellAt);
    if (mistType === "fog5") this.fog5.setUnits(unitFeet ?? []);
    this.fog5.sync(dt, sunLight, mistSpeed);
    this.revealFog.rebuild(engine.cols, engine.rows, tile, engine.mission.id, fog1Tier);
    if (mistType === "fog1" && viewport) this.revealFog.coverViewport(viewport.cssW, viewport.cssH, viewport.camX, viewport.camY, coverW, coverH);
    if (mistType === "fog1" && cellAt) this.revealFog.syncReveal(engine, tile, cellAt);
    this.revealFog.sync(dt, sunLight, mistSpeed);
    this.fogArtwork.rebuild(engine.cols, engine.rows, tile, engine.mission.id, 0);
    this.fogArtwork.sync(dt, mistSpeed);

    this.scratchSunColor.copy(sunLight.color).multiplyScalar(Math.min(1.5, sunLight.intensity * 0.6));
    this.scratchHemiColor.copy(hemiLight.color).multiplyScalar(Math.min(1.5, hemiLight.intensity * 1.2));

    this.scratchWispColor.setHex(engine.mission.wispColor ?? 0xffa552);

    this.dust.sync(boardW, boardH, tile, key, tier.dustCount, tier.emberRiseHeight, dt, this.scratchHemiColor, 1, null);
    this.embers.sync(boardW, boardH, tile, key, tier.emberCount, tier.emberRiseHeight, dt, this.scratchSunColor, wispSpeed, this.scratchWispColor);
  }

  dispose(): void {
    this.mist2.dispose();
    this.mist3.dispose();
    this.mist4.dispose();
    this.fog5.dispose();
    this.revealFog.dispose();
    this.fogArtwork.dispose();
    this.dust.dispose();
    this.embers.dispose();
  }
}
