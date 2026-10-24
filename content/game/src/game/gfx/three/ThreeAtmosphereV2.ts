/** Atmosphere V2 — the battle scene's air: rolling ground mist, slanting sun shafts, drifting
 * dust motes and the mission's rising wisps. Replaces ThreeAtmosphere.ts in ThreeBattleRenderer
 * (that file is kept, unused, as the previous version).
 *
 * Built to cost almost nothing per frame:
 *  - Every mesh, material and buffer is created ONCE in the constructor, so the battle's shader
 *    warm-up compiles all of them and nothing ever compiles mid-fight.
 *  - Nothing is rebuilt on zoom, pan, spell casts or setting changes. Particle positions are
 *    stored normalized and placed by uniforms; particle counts change through mesh.count.
 *  - All motion is computed on the GPU from a time uniform. No texture or buffer is re-uploaded
 *    while playing — the one noise texture is baked once and shared by every layer.
 *  - Four draw calls total (mist, shafts, dust, wisps), each hidden when it has nothing to show.
 *
 * Reads the same Mission fields the Map Editor already writes, so every saved map works as-is:
 *  - mistType: "none" turns the whole atmosphere off (the author's explicit kill switch).
 *    "mist4" keeps its border-only meaning (the playable interior stays clear). The other
 *    world-space types (mist2/mist3/fog1/fog5) all use the new full-field mist. The screen
 *    vignettes ("vignette".."vignette4") stay in BattleCanvas.tsx; with them, the world mist is
 *    off but shafts, dust and wisps still play.
 *  - mistIntensity / mistSpeed: mist strength and drift pace.
 *  - wispIntensity / wispSpeed / wispColor: rising wisps, as before.
 *
 * World convention matches ThreeBattleRenderer: x right, y = -screen y (pre-negated), z up. */

import * as THREE from "three";
import type { BattleEngine } from "../../engine";

const SQRT3 = Math.sqrt(3);
/** Must match BattleEngine's boardPad() (tile * 2.4) — same constant ThreeAtmosphere.ts uses. */
const BOARD_PAD_MUL = 2.4;
/** wispIntensity = 1 maps to this many wisps (same scale as ThreeAtmosphere.ts). */
const MAX_WISPS = 2000;
const DUST_COUNT = 280;
const DEFAULT_MIST_COLOR = 0xaab4ad;
const DEFAULT_WISP_COLOR = 0xffa552;
const WARM_SHAFT = new THREE.Color(1, 0.9, 0.7);
const WARM_DUST = new THREE.Color(1, 0.95, 0.85);

/** Same layering the old mist used: under character billboards (2) and under lit props (15). */
const MIST_ORDER = 1.5;
const DUST_ORDER = 13;
const WISP_ORDER = 14;
/** Light shafts are the air between camera and ground, so they sit over units and props, below
 * the tactical overlays (100+) and above-fog scenery (110). */
const SHAFT_ORDER = 16;

function boardSize(cols: number, rows: number, tile: number): { w: number; h: number } {
  return { w: tile * SQRT3 * (cols + 0.5), h: tile * (1.5 * (rows - 1) + 2) + tile * BOARD_PAD_MUL };
}

// ---------------------------------------------------------------------------------------------
// One shared tileable noise texture: R/G/B hold three frequencies of smooth value noise.

function hash(x: number, y: number, seed: number): number {
  const h = Math.sin(x * 127.1 + y * 311.7 + seed * 74.7) * 43758.5453123;
  return h - Math.floor(h);
}

function tileableNoise(u: number, v: number, freq: number, seed: number): number {
  const xf = u * freq, yf = v * freq;
  const x0 = Math.floor(xf), y0 = Math.floor(yf);
  const tx = xf - x0, ty = yf - y0;
  const w = (n: number) => ((n % freq) + freq) % freq;
  const a = hash(w(x0), w(y0), seed), b = hash(w(x0 + 1), w(y0), seed);
  const c = hash(w(x0), w(y0 + 1), seed), d = hash(w(x0 + 1), w(y0 + 1), seed);
  const sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
  const top = a + (b - a) * sx, bottom = c + (d - c) * sx;
  return top + (bottom - top) * sy;
}

function buildNoiseTexture(size = 256): THREE.DataTexture {
  const data = new Uint8Array(size * size * 4);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const u = x / size, v = y / size, i = (y * size + x) * 4;
      // Two octaves per channel so each one is already soft and cloud-like on its own.
      data[i] = Math.round(255 * (tileableNoise(u, v, 4, 1.7) * 0.7 + tileableNoise(u, v, 8, 2.9) * 0.3));
      data[i + 1] = Math.round(255 * (tileableNoise(u, v, 8, 5.3) * 0.7 + tileableNoise(u, v, 16, 6.1) * 0.3));
      data[i + 2] = Math.round(255 * (tileableNoise(u, v, 16, 11.1) * 0.7 + tileableNoise(u, v, 32, 13.7) * 0.3));
      data[i + 3] = 255;
    }
  }
  const tex = new THREE.DataTexture(data, size, size, THREE.RGBAFormat);
  tex.wrapS = THREE.RepeatWrapping;
  tex.wrapT = THREE.RepeatWrapping;
  tex.magFilter = THREE.LinearFilter;
  tex.minFilter = THREE.LinearMipmapLinearFilter;
  tex.generateMipmaps = true;
  tex.needsUpdate = true;
  return tex;
}

// ---------------------------------------------------------------------------------------------
// Shaders

const PLANE_VERTEX = /* glsl */ `
  varying vec2 vXY;
  void main() {
    vec4 world = modelMatrix * vec4(position, 1.0);
    vXY = world.xy;
    gl_Position = projectionMatrix * viewMatrix * world;
  }
`;

/** Rolling ground mist: two drifting layers of domain-warped noise, lit by the sky and sun. */
const MIST_FRAGMENT = /* glsl */ `
  uniform sampler2D uNoise;
  uniform float uTime;
  uniform float uAlpha;
  uniform float uScale;
  uniform float uEdgeMode;
  uniform float uBorder;
  uniform vec2 uBoardCenter;
  uniform vec2 uBoardHalf;
  uniform vec3 uColor;
  uniform vec3 uSky;
  uniform vec3 uSun;
  varying vec2 vXY;

  float fbm(vec2 p) {
    return texture2D(uNoise, p).r * 0.55 + texture2D(uNoise, p * 1.9 + 0.37).g * 0.3 + texture2D(uNoise, p * 3.7 + 0.71).b * 0.15;
  }

  void main() {
    vec2 p = vXY * uScale;
    // Slow large-scale warp makes the banks curl and roll instead of sliding as a flat texture.
    vec2 warp = vec2(texture2D(uNoise, p * 0.35 + uTime * vec2(0.0040, 0.0021)).g,
                     texture2D(uNoise, p * 0.35 + vec2(0.5, 0.2) + uTime * vec2(-0.0027, 0.0036)).b) - 0.5;
    float far = fbm(p * 0.7 + warp * 0.55 + uTime * vec2(0.0060, -0.0025));
    float near = fbm(p * 1.45 - warp * 0.8 + uTime * vec2(0.0115, -0.0060) + 0.5);
    float body = far * 0.62 + near * 0.5;
    float density = smoothstep(0.42, 0.9, body);

    vec2 inside = uBoardHalf - abs(vXY - uBoardCenter);
    float fromEdge = min(inside.x, inside.y);
    // Border-only mode: the playable interior is always fully clear.
    float edgeMask = pow(1.0 - smoothstep(0.0, uBorder, fromEdge), 1.6);
    // Full mode: whole field, a little thicker beyond the board's edge.
    float fullMask = mix(1.0, 0.82, smoothstep(-uBorder * 0.3, uBorder * 0.6, fromEdge));
    float mask = mix(fullMask, edgeMask, uEdgeMode);

    // Thin parts catch the sun, thick cores stay in the sky's cooler fill.
    vec3 lit = uColor * (0.55 + uSky * 0.45) + uSun * (0.18 + 0.32 * (1.0 - density));
    gl_FragColor = vec4(lit, density * mask * uAlpha);
  }
`;

/** Sun shafts: soft bands running along the sun's travel direction, broken up along their length
 * and swaying slowly. Additive, so they only ever add light. */
const SHAFT_FRAGMENT = /* glsl */ `
  uniform sampler2D uNoise;
  uniform float uTime;
  uniform float uStrength;
  uniform float uScale;
  uniform vec2 uDir;
  uniform vec3 uColor;
  varying vec2 vXY;

  void main() {
    vec2 across = vec2(-uDir.y, uDir.x);
    float s = dot(vXY, across) * uScale;
    float t = dot(vXY, uDir) * uScale;
    float wide = texture2D(uNoise, vec2(s * 0.55 + uTime * 0.0045, 0.13)).r;
    float fine = texture2D(uNoise, vec2(s * 1.6 - uTime * 0.0080, 0.61)).g;
    float beams = smoothstep(0.5, 0.92, wide * 0.7 + fine * 0.42);
    // Each beam fades in and out along its length so shafts never run as endless stripes.
    float run = smoothstep(0.3, 0.75, texture2D(uNoise, vec2(s * 0.22 + 0.4, t * 0.16 + uTime * 0.0030)).b);
    float shimmer = 0.85 + 0.15 * sin(uTime * 0.6 + s * 9.0);
    gl_FragColor = vec4(uColor, beams * run * shimmer * uStrength);
  }
`;

const PARTICLE_FRAGMENT = /* glsl */ `
  uniform vec3 uColor;
  uniform float uOpacity;
  varying vec2 vUv;
  varying float vAlpha;
  void main() {
    float d = length(vUv - 0.5) * 2.0;
    float a = smoothstep(1.0, 0.0, d);
    a *= a;
    gl_FragColor = vec4(uColor, a * vAlpha * uOpacity);
  }
`;

/** Dust motes: world-anchored, wrapped into a window around the camera so the field is endless
 * without moving with the view. They drift on a light breeze and twinkle as they turn. */
const DUST_VERTEX = /* glsl */ `
  attribute vec4 aSeed;
  uniform float uTime;
  uniform float uSize;
  uniform float uHeight;
  uniform vec2 uOrigin;
  uniform vec2 uArea;
  uniform vec2 uWind;
  varying vec2 vUv;
  varying float vAlpha;
  void main() {
    vUv = uv;
    vec2 world = aSeed.xy * uArea + uWind * uTime * (0.6 + aSeed.z * 0.8)
      + vec2(sin(uTime * 0.21 + aSeed.w * 31.0), cos(uTime * 0.17 + aSeed.z * 23.0)) * uSize * 3.0;
    vec2 xy = uOrigin + mod(world - uOrigin, uArea);
    float z = uHeight * (0.15 + aSeed.z * 0.85) + sin(uTime * 0.35 + aSeed.w * 19.0) * uSize * 2.0;
    float twinkle = 0.5 + 0.5 * sin(uTime * (0.5 + aSeed.w * 0.9) + aSeed.z * 40.0);
    // Soft fade at the window's edge so wrapping motes never pop in.
    vec2 e = min(xy - uOrigin, uOrigin + uArea - xy) / (uArea * 0.08);
    vAlpha = (0.25 + 0.75 * twinkle * twinkle) * clamp(min(e.x, e.y), 0.0, 1.0);
    vec4 mv = viewMatrix * vec4(xy, z, 1.0);
    mv.xy += position.xy * uSize * (0.55 + aSeed.w * 0.9);
    gl_Position = projectionMatrix * mv;
  }
`;

/** Wisps: rise from the board (and its surroundings) in a slow sway, glow, and fade out. */
const WISP_VERTEX = /* glsl */ `
  attribute vec4 aSeed;
  uniform float uTime;
  uniform float uSize;
  uniform float uRise;
  uniform vec2 uOrigin;
  uniform vec2 uArea;
  varying vec2 vUv;
  varying float vAlpha;
  void main() {
    vUv = uv;
    float phase = fract(aSeed.w + uTime * 0.055 * (0.6 + aSeed.z * 0.8));
    vec2 xy = uOrigin + aSeed.xy * uArea
      + vec2(sin(uTime * 0.7 + aSeed.z * 40.0), cos(uTime * 0.5 + aSeed.w * 30.0)) * uSize * 2.5 * phase;
    float z = phase * uRise;
    vAlpha = sin(phase * 3.14159) * (0.6 + 0.4 * sin(uTime * 3.0 + aSeed.z * 60.0));
    vec4 mv = viewMatrix * vec4(xy, z, 1.0);
    mv.xy += position.xy * uSize * (0.7 + aSeed.z * 0.6);
    gl_Position = projectionMatrix * mv;
  }
`;

function seeds(count: number, rng: () => number): THREE.InstancedBufferAttribute {
  const a = new Float32Array(count * 4);
  for (let i = 0; i < a.length; i++) a[i] = rng();
  return new THREE.InstancedBufferAttribute(a, 4);
}

/** Small deterministic RNG so the field looks the same every battle (no Math.random churn). */
function mulberry32(seed: number): () => number {
  let s = seed >>> 0;
  return () => {
    s = (s + 0x6d2b79f5) >>> 0;
    let t = s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function particleMesh(vertex: string, count: number, uniforms: Record<string, THREE.IUniform>, blending: THREE.Blending, seed: number): THREE.InstancedMesh {
  const geo = new THREE.PlaneGeometry(1, 1);
  geo.setAttribute("aSeed", seeds(count, mulberry32(seed)));
  const material = new THREE.ShaderMaterial({
    vertexShader: vertex,
    fragmentShader: PARTICLE_FRAGMENT,
    uniforms,
    transparent: true,
    depthWrite: false,
    depthTest: false,
    blending,
  });
  const mesh = new THREE.InstancedMesh(geo, material, count);
  // Positions come from the shader, never from instanceMatrix/the mesh bounds.
  mesh.frustumCulled = false;
  return mesh;
}

// ---------------------------------------------------------------------------------------------

export class ThreeAtmosphereV2 {
  readonly group = new THREE.Group();

  private readonly noise = buildNoiseTexture();
  private readonly plane = new THREE.PlaneGeometry(1, 1);

  private readonly mistMaterial = new THREE.ShaderMaterial({
    vertexShader: PLANE_VERTEX,
    fragmentShader: MIST_FRAGMENT,
    transparent: true,
    depthWrite: false,
    depthTest: false,
    uniforms: {
      uNoise: { value: this.noise },
      uTime: { value: 0 },
      uAlpha: { value: 0 },
      uScale: { value: 0.004 },
      uEdgeMode: { value: 0 },
      uBorder: { value: 1 },
      uBoardCenter: { value: new THREE.Vector2() },
      uBoardHalf: { value: new THREE.Vector2(1, 1) },
      uColor: { value: new THREE.Color(DEFAULT_MIST_COLOR) },
      uSky: { value: new THREE.Color(1, 1, 1) },
      uSun: { value: new THREE.Color(1, 1, 1) },
    },
  });
  private readonly mist = new THREE.Mesh(this.plane, this.mistMaterial);

  private readonly shaftMaterial = new THREE.ShaderMaterial({
    vertexShader: PLANE_VERTEX,
    fragmentShader: SHAFT_FRAGMENT,
    transparent: true,
    depthWrite: false,
    depthTest: false,
    blending: THREE.AdditiveBlending,
    uniforms: {
      uNoise: { value: this.noise },
      uTime: { value: 0 },
      uStrength: { value: 0 },
      uScale: { value: 0.001 },
      uDir: { value: new THREE.Vector2(0.6, -0.8) },
      uColor: { value: new THREE.Color(1, 0.92, 0.75) },
    },
  });
  private readonly shafts = new THREE.Mesh(this.plane, this.shaftMaterial);

  private readonly dustUniforms = {
    uTime: { value: 0 },
    uSize: { value: 4 },
    uHeight: { value: 100 },
    uOrigin: { value: new THREE.Vector2() },
    uArea: { value: new THREE.Vector2(1, 1) },
    uWind: { value: new THREE.Vector2(6, -2.5) },
    uColor: { value: new THREE.Color(1, 0.94, 0.8) },
    uOpacity: { value: 0 },
  };
  private readonly dust = particleMesh(DUST_VERTEX, DUST_COUNT, this.dustUniforms, THREE.AdditiveBlending, 0x5eed1);

  private readonly wispUniforms = {
    uTime: { value: 0 },
    uSize: { value: 6 },
    uRise: { value: 90 },
    uOrigin: { value: new THREE.Vector2() },
    uArea: { value: new THREE.Vector2(1, 1) },
    uColor: { value: new THREE.Color(DEFAULT_WISP_COLOR) },
    uOpacity: { value: 0.9 },
  };
  private readonly wisps = particleMesh(WISP_VERTEX, MAX_WISPS, this.wispUniforms, THREE.AdditiveBlending, 0x5eed2);

  private readonly sunDir = new THREE.Vector3();

  constructor() {
    this.mist.renderOrder = MIST_ORDER;
    this.shafts.renderOrder = SHAFT_ORDER;
    this.dust.renderOrder = DUST_ORDER;
    this.wisps.renderOrder = WISP_ORDER;
    this.mist.frustumCulled = false;
    this.shafts.frustumCulled = false;
    this.group.add(this.mist, this.shafts, this.dust, this.wisps);
  }

  /** Wisps (and only them) feed bloom — same rule as ThreeAtmosphere.markBloomLayer. */
  markBloomLayer(layer: number): void {
    this.wisps.layers.enable(layer);
  }

  sync(
    engine: BattleEngine,
    tile: number,
    dt: number,
    sunLight: THREE.DirectionalLight,
    hemiLight: THREE.HemisphereLight,
    viewport?: { cssW: number; cssH: number; camX: number; camY: number },
    _cellAt?: (x: number, y: number) => number,
    _unitFeet?: { x: number; y: number; halfW: number; band: number }[],
    enabled = true,
  ): void {
    const mission = engine.mission;
    const mistType = mission.mistType ?? "none";
    this.group.visible = enabled && mistType !== "none";
    if (!this.group.visible) return;

    const screenVignette = mistType === "vignette" || mistType === "vignette2" || mistType === "vignette3" || mistType === "vignette4";
    const mistIntensity = screenVignette ? 0 : Math.max(0, mission.mistIntensity ?? 0.2);
    const mistSpeed = mission.mistSpeed ?? 1;
    const wispIntensity = Math.max(0, Math.min(1, mission.wispIntensity ?? 0.02));
    const wispSpeed = mission.wispSpeed ?? 1;

    const { w: boardW, h: boardH } = boardSize(engine.cols, engine.rows, tile);
    const padTop = tile * BOARD_PAD_MUL;
    const centerX = boardW / 2;
    const centerY = -(padTop + boardH) / 2;

    // The air sheets cover the camera's footprint on the ground (wider when the tactics camera
    // tilts/turns the board), centered on the view — same coverage rule the old mist used.
    const cssW = viewport?.cssW ?? boardW, cssH = viewport?.cssH ?? boardH;
    const viewX = viewport ? viewport.camX + cssW / 2 : centerX;
    const viewY = viewport ? -viewport.camY - cssH / 2 : centerY;
    let coverW = cssW, coverH = cssH;
    if (engine.tacticsCamera) {
      const pitch = THREE.MathUtils.degToRad(THREE.MathUtils.clamp(engine.cameraTilt, 0, 55));
      const azimuth = THREE.MathUtils.degToRad(engine.cameraTiltSide);
      const groundH = cssH / Math.cos(pitch);
      const cosA = Math.abs(Math.cos(azimuth)), sinA = Math.abs(Math.sin(azimuth));
      coverW = cssW * cosA + groundH * sinA;
      coverH = cssW * sinA + groundH * cosA;
    }
    const sheetW = coverW * 1.3, sheetH = coverH * 1.3;

    // Light: the sun's tint and strength drive the shafts and the lit side of the mist; at night
    // (sun off) they fade out on their own and the mist takes the sky's cooler fill.
    const sunOn = sunLight.visible && sunLight.intensity > 0;
    const sunStrength = sunOn ? Math.min(1, sunLight.intensity / 3.5) : 0;
    const sunColor = this.mistMaterial.uniforms.uSun!.value as THREE.Color;
    sunColor.copy(sunLight.color).multiplyScalar(sunStrength * 0.9);
    const skyColor = this.mistMaterial.uniforms.uSky!.value as THREE.Color;
    skyColor.copy(hemiLight.color).multiplyScalar(Math.min(1.2, hemiLight.intensity * 0.8));

    // Mist
    const mistOn = mistIntensity > 0;
    this.mist.visible = mistOn;
    if (mistOn) {
      const u = this.mistMaterial.uniforms;
      // sqrt curve: the editor's small values (0.02-0.2) still read as real mist, high values
      // stay below a full white-out.
      u.uAlpha!.value = Math.min(0.85, Math.sqrt(mistIntensity) * 0.9);
      u.uTime!.value += dt * mistSpeed * 60;
      u.uScale!.value = 1 / (tile * 9);
      u.uEdgeMode!.value = mistType === "mist4" ? 1 : 0;
      u.uBorder!.value = tile * 5;
      (u.uBoardCenter!.value as THREE.Vector2).set(centerX, centerY);
      (u.uBoardHalf!.value as THREE.Vector2).set(boardW / 2, (boardH - padTop) / 2);
      this.mist.scale.set(sheetW, sheetH, 1);
      this.mist.position.set(viewX, viewY, tile * 0.25);
    }

    // Sun shafts
    const shaftStrength = 0.11 * sunStrength;
    this.shafts.visible = shaftStrength > 0.005;
    if (this.shafts.visible) {
      const u = this.shaftMaterial.uniforms;
      this.sunDir.subVectors(sunLight.target.position, sunLight.position);
      const len = Math.hypot(this.sunDir.x, this.sunDir.y);
      if (len > 1e-3) (u.uDir!.value as THREE.Vector2).set(this.sunDir.x / len, this.sunDir.y / len);
      u.uStrength!.value = shaftStrength;
      u.uTime!.value += dt * 60;
      u.uScale!.value = 1 / (tile * 16);
      (u.uColor!.value as THREE.Color).copy(sunLight.color).lerp(WARM_SHAFT, 0.4);
      this.shafts.scale.set(sheetW, sheetH, 1);
      this.shafts.position.set(viewX, viewY, tile * 1.5);
    }

    // Dust motes: always present in the air, brighter in sunlight.
    {
      const u = this.dustUniforms;
      u.uTime.value += dt;
      u.uSize.value = tile * 0.06;
      u.uHeight.value = tile * 2.2;
      u.uOrigin.value.set(viewX - sheetW / 2, viewY - sheetH / 2);
      u.uArea.value.set(sheetW, sheetH);
      u.uWind.value.set(tile * 0.12, -tile * 0.05);
      u.uOpacity.value = 0.22 + 0.4 * sunStrength;
      u.uColor.value.copy(sunOn ? sunLight.color : hemiLight.color).lerp(WARM_DUST, 0.5);
    }

    // Wisps
    const wispCount = Math.round(wispIntensity * MAX_WISPS);
    this.wisps.visible = wispCount > 0;
    if (wispCount > 0) {
      this.wisps.count = wispCount;
      const u = this.wispUniforms;
      u.uTime.value += dt * wispSpeed;
      u.uSize.value = tile * 0.1;
      u.uRise.value = tile * 1.6;
      const pad = tile * 3;
      u.uOrigin.value.set(-pad, -boardH - pad);
      u.uArea.value.set(boardW + pad * 2, boardH - padTop + pad * 2);
      u.uColor.value.setHex(mission.wispColor ?? DEFAULT_WISP_COLOR);
    }
  }

  dispose(): void {
    this.noise.dispose();
    this.plane.dispose();
    this.mistMaterial.dispose();
    this.shaftMaterial.dispose();
    for (const mesh of [this.dust, this.wisps]) {
      mesh.geometry.dispose();
      (mesh.material as THREE.Material).dispose();
    }
  }
}
