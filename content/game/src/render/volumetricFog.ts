// "Névoa 3D": a volumetric ground mist that only a real 3D scene can have. A screen pass reads
// the depth of every pixel, rebuilds its world position, and marches the view ray through a
// layer of drifting 3D noise that lies on the ground — thick in the low places, thinning out
// with height, clear above. Because it is computed per pixel in world space:
//  - it pools in valleys and riverbeds and thins over hills (density follows world height),
//  - units and trees stand in it: feet sink in, nearer mist covers what is farther away,
//  - the sun/moon tint it, and every light in the scene (torches, braziers, glowing spells,
//    carried lights) scatters in it as a soft halo,
//  - it drifts slowly in 3D, never as a flat sheet.
// Ember's Névoas (2, 3, 4, 01, 5) are each remade on it as a preset (src/game/mist3d.ts): the
// same character, built as real mist. Off until a mission's Névoa turns it on.
//
// Cost: the march runs at half resolution into its own target (glow + transmittance) and is
// laid over the full-resolution frame with a depth-aware upsample, so silhouettes keep clean
// mist edges. Its noise is one baked, tileable 3D texture (built once) instead of hashing every
// lattice corner per step. Same 28 steps, same density rules and settings as before.

import * as THREE from 'three';
import { Pass, FullScreenQuad } from 'three/examples/jsm/postprocessing/Pass.js';

const MAX_LIGHTS = 12;
/** Lattice cells per side of the baked noise volume (one texel = one value-noise lattice cell). */
const NOISE_SIZE = 64;

/** Tileable 3D value-noise lattice: each texel is one random corner value; the shader's smooth
 * filtering (smoothstep between texels) turns it into the same smooth value noise as before. */
function buildNoiseVolume(): THREE.Data3DTexture {
  const n = NOISE_SIZE, data = new Uint8Array(n * n * n);
  let s = 0x9e3779b9;
  for (let i = 0; i < data.length; i++) {
    s ^= s << 13; s ^= s >>> 17; s ^= s << 5;
    data[i] = (s >>> 0) & 255;
  }
  const tex = new THREE.Data3DTexture(data, n, n, n);
  tex.format = THREE.RedFormat;
  tex.type = THREE.UnsignedByteType;
  tex.wrapS = tex.wrapT = tex.wrapR = THREE.RepeatWrapping;
  tex.minFilter = tex.magFilter = THREE.LinearFilter;
  tex.generateMipmaps = false;
  tex.unpackAlignment = 1;
  tex.needsUpdate = true;
  return tex;
}

const VERTEX = /* glsl */ `
  varying vec2 vUv;
  void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }
`;

const FRAGMENT = /* glsl */ `
  #define MAX_LIGHTS ${MAX_LIGHTS}
  #define STEPS 28
  #define NOISE_SIZE ${NOISE_SIZE}.0
  uniform sampler3D tNoise;
  uniform sampler2D tDepth;
  uniform mat4 uProjInv;
  uniform mat4 uCamWorld;
  uniform vec3 uCamPos;
  uniform vec2 uTexel;         // one pixel of the depth buffer
  uniform float uTime, uSpeed, uDensity, uBase, uTop, uMaxDist;
  uniform vec3 uScale, uWind;
  uniform vec2 uContrast;      // noise → wisps: smoothstep(lo, hi)
  uniform float uFloor, uHeightPow;
  uniform vec2 uCenter;        // board centre (xz): swirl and clear centre
  uniform float uSwirl, uClear, uEdge; // swirl rate; density 0 inside uClear, full beyond uEdge (0,0 = off)
  uniform sampler2D tMask;     // > 0.5 where the mist may be (Névoa 01: unrevealed ground)
  uniform vec4 uMaskRect;      // x0, z0, width, depth; outside it counts as masked-in
  uniform float uUseMask;
  uniform vec4 uBoard;         // board x0, z0, x1, z1: the mist fades out past it
  uniform float uReach;        // how far past the board it still reaches
  uniform sampler2D tGround;   // terrain height under the mist (alpha 0..1 → uGroundRange)
  uniform vec4 uGroundRect;    // x0, z0, width, depth
  uniform vec2 uGroundRange;   // lowest, highest ground
  uniform float uUseGround, uThick, uPool; // layer thickness above the local ground; extra density in low ground
  uniform vec3 uAlbedo, uAmbient, uSunDir, uSunColor;
  uniform int uLightCount;
  uniform vec3 uLightPos[MAX_LIGHTS];
  uniform vec3 uLightColor[MAX_LIGHTS];
  uniform float uLightRange[MAX_LIGHTS];
  varying vec2 vUv;

  // smooth value noise from the baked lattice: one filtered fetch, with the fetch point bent
  // by smoothstep so it interpolates as smoothly as the old per-corner hash version
  float noise(vec3 x) {
    // texel k's centre is (k + 0.5) / NOISE_SIZE: shift by one so f = 0 lands exactly on it
    vec3 p = x + 1.0, i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return texture(tNoise, (i + f - 0.5) / NOISE_SIZE).r;
  }
  float fbm(vec3 p) { return noise(p) * 0.55 + noise(p * 2.03 + 7.1) * 0.3 + noise(p * 4.1 + 3.7) * 0.15; }

  float groundAt(vec2 xz) {
    if (uUseGround < 0.5) return uBase;
    vec2 guv = (xz - uGroundRect.xy) / uGroundRect.zw;
    if (guv.x < 0.0 || guv.y < 0.0 || guv.x > 1.0 || guv.y > 1.0) return uBase;
    return mix(uGroundRange.x, uGroundRange.y, texture2D(tGround, guv).a);
  }

  // the mist's density at a world point: a layer of uThick over the local ground, densest at the
  // ground and thicker in the low places (it pools), broken up by drifting noise, shaped by the preset
  float density(vec3 p) {
    float g = groundAt(p.xz);
    float h = clamp((uThick - (p.y - g)) / max(uThick, 0.001), 0.0, 1.0);
    if (h <= 0.0) return 0.0;
    float low = (uGroundRange.y - g) / max(uGroundRange.y - uGroundRange.x, 0.001);
    h *= 1.0 + uPool * low;
    vec2 rel = p.xz - uCenter;
    float r = length(rel);
    float mask = 1.0;
    if (uEdge > 0.0) mask = smoothstep(uClear, uEdge, r);
    if (uUseMask > 0.5) {
      vec2 muv = (p.xz - uMaskRect.xy) / uMaskRect.zw;
      float m = (muv.x < 0.0 || muv.y < 0.0 || muv.x > 1.0 || muv.y > 1.0) ? 1.0 : texture2D(tMask, muv).a;
      mask *= smoothstep(0.55, 0.95, m);
    }
    // it belongs to the battlefield: fades out past the board instead of banding to the horizon
    vec2 out2 = max(max(uBoard.xy - p.xz, p.xz - uBoard.zw), 0.0);
    mask *= 1.0 - smoothstep(uReach * 0.35, uReach, length(out2));
    if (mask <= 0.0) return 0.0;
    vec3 q = p;
    if (uSwirl != 0.0) {
      // a slow vortex around the board: the farther out, the slower it turns
      float a = uSwirl * uTime * uSpeed / (1.0 + r * 0.08);
      float c = cos(a), s = sin(a);
      q.xz = uCenter + vec2(c * rel.x - s * rel.y, s * rel.x + c * rel.y);
    }
    float n = fbm(q * uScale + uWind * uTime * uSpeed);
    float wisps = smoothstep(uContrast.x, uContrast.y, n);
    return uDensity * pow(h, uHeightPow) * mask * (uFloor + (1.0 - uFloor) * wisps);
  }

  // output: rgb = mist glow, a = how much of the scene still shows through (transmittance)
  void main() {
    // a unit or prop card's soft edge is mostly see-through but still writes the card's depth:
    // take the farthest depth around the pixel so those edge pixels get the mist of what shows
    // through them, not an un-misted outline in the backdrop's colour
    float depth = texture2D(tDepth, vUv).x;
    depth = max(depth, texture2D(tDepth, vUv + vec2(uTexel.x, 0.0)).x);
    depth = max(depth, texture2D(tDepth, vUv - vec2(uTexel.x, 0.0)).x);
    depth = max(depth, texture2D(tDepth, vUv + vec2(0.0, uTexel.y)).x);
    depth = max(depth, texture2D(tDepth, vUv - vec2(0.0, uTexel.y)).x);
    vec4 view = uProjInv * vec4(vUv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    view /= view.w;
    vec3 world = (uCamWorld * vec4(view.xyz, 1.0)).xyz;
    vec3 rd = normalize(world - uCamPos);
    float tMax = depth >= 0.99999 ? uMaxDist : min(length(world - uCamPos), uMaxDist);

    // only march where the ray is inside the mist layer
    float lo = min(uBase, uGroundRange.x) - 0.5, hi = uTop;
    float t0 = 0.0, t1 = tMax;
    if (abs(rd.y) > 1e-4) {
      float ta = (lo - uCamPos.y) / rd.y, tb = (hi - uCamPos.y) / rd.y;
      t0 = max(t0, min(ta, tb)); t1 = min(t1, max(ta, tb));
    } else if (uCamPos.y < lo || uCamPos.y > hi) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    if (t1 <= t0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

    float dt = (t1 - t0) / float(STEPS);
    // per-pixel jitter (interleaved gradient noise) hides step banding
    float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float cosSun = dot(rd, uSunDir);
    // a little forward glow toward the sun (0.9 flared a white blob under a low moon in view)
    float sunPhase = 0.6 + 0.25 * pow(max(cosSun, 0.0), 6.0);
    vec3 T = vec3(1.0), glow = vec3(0.0);
    for (int i = 0; i < STEPS; i++) {
      vec3 p = uCamPos + rd * (t0 + (float(i) + jitter) * dt);
      float d = density(p);
      if (d < 1e-4) continue;
      // mist reads grey: the sky and sun set its brightness and barely tint it
      vec3 light = uAmbient + uSunColor * sunPhase;
      light = mix(vec3(dot(light, vec3(0.299, 0.587, 0.114))), light, 0.1);
      for (int k = 0; k < MAX_LIGHTS; k++) {
        if (k >= uLightCount) break;
        vec3 v = uLightPos[k] - p;
        float r = uLightRange[k], q = dot(v, v) / (r * r);
        light += uLightColor[k] * max(0.0, 1.0 - q) / (1.0 + 6.0 * q);
      }
      // spell lights can be extremely bright: the mist only ever glows within a sane range,
      // so the bloom after it never receives an overflow (it turned that into a black box)
      // (lowered on request: a fire should light the room's haze a bit, not flare)
      light = clamp(light, vec3(0.0), vec3(2.0));
      float a = 1.0 - exp(-d * dt);
      glow += T * a * uAlbedo * light;
      T *= 1.0 - a;
      if (T.x < 0.01) break;
    }
    // never hand an invalid value on (NaN fails every comparison with itself)
    if (!(glow.r == glow.r) || !(glow.g == glow.g) || !(glow.b == glow.b) || !(T.x == T.x)) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    gl_FragColor = vec4(min(glow, vec3(64.0)), T.x);
  }
`;

/** Full resolution: lays the half-resolution mist over the scene. Each pixel blends its four
 * nearest mist texels, weighting down any whose depth differs from its own, so mist never bleeds
 * across a unit's or a tree's silhouette. */
const COMPOSITE = /* glsl */ `
  uniform sampler2D tDiffuse;
  uniform sampler2D tDepth;
  uniform sampler2D tFog;
  uniform vec2 uFogSize;
  uniform float uNear, uFar;
  varying vec2 vUv;

  float viewZ(float d) { float z = d * 2.0 - 1.0; return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear)); }

  void main() {
    vec4 scene = texture2D(tDiffuse, vUv);
    float z = viewZ(texture2D(tDepth, vUv).x);
    vec2 f = vUv * uFogSize - 0.5, i = floor(f), w = f - i;
    vec4 acc = vec4(0.0);
    float sum = 0.0;
    for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) {
      vec2 uv = (i + vec2(float(x), float(y)) + 0.5) / uFogSize;
      float zs = viewZ(texture2D(tDepth, uv).x);
      float bw = (x == 0 ? 1.0 - w.x : w.x) * (y == 0 ? 1.0 - w.y : w.y);
      float dw = 1.0 / (0.02 + abs(zs - z) / max(z, 1e-3) * 50.0);
      float k = bw * dw + 1e-5;
      acc += texture2D(tFog, uv) * k;
      sum += k;
    }
    vec4 fog = acc / sum;
    vec3 outC = scene.rgb * fog.a + fog.rgb;
    if (!(outC.r == outC.r) || !(outC.g == outC.g) || !(outC.b == outC.b)) outC = scene.rgb;
    gl_FragColor = vec4(min(outC, vec3(64.0)), scene.a);
  }
`;

export interface VolumetricFogSettings {
  /** Lowest ground height (world y, also used off the terrain map) and the highest the mist reaches. */
  base: number; top: number;
  /** Layer thickness above the local ground, and extra density where the ground is low (0 = none). */
  thickness: number; pool: number;
  /** Terrain height map (alpha 0..1 → groundRange) over groundRect (x0, z0, width, depth); null = flat at `base`. */
  ground: THREE.Texture | null; groundRect: THREE.Vector4; groundRange: THREE.Vector2;
  /** Extinction per world unit at the densest point. */
  density: number;
  /** Drift speed multiplier (Ember's "Vel. da névoa"). */
  speed: number;
  /** Mist colour (linear). */
  albedo: THREE.Color;
  /** Noise frequency per axis (world units⁻¹): stretch x/z for veils, y for layering. */
  scale: THREE.Vector3;
  /** Noise drift per second (before `speed`). */
  wind: THREE.Vector3;
  /** Noise → mist: smoothstep(lo, hi); narrow = hard wisps, wide = soft haze. */
  contrast: [number, number];
  /** Share of the density present even where the noise is empty (0 = only wisps, 1 = even haze). */
  floor: number;
  /** How fast it thins with height: 1 = linear, 2+ = hugs the ground. */
  heightPow: number;
  /** Board centre (xz) for swirl and the clear centre. */
  center: THREE.Vector2;
  /** Vortex turn rate (0 = none). */
  swirl: number;
  /** No mist within `clear` of the centre, full beyond `edge` (edge 0 = whole world). */
  clear: number; edge: number;
  /** Where the mist may be (alpha > 0.5), over `maskRect` (x0, z0, width, depth); null = everywhere. */
  mask: THREE.Texture | null;
  maskRect: THREE.Vector4;
  /** Board bounds (x0, z0, x1, z1) and how far past them the mist still reaches. */
  board: THREE.Vector4;
  reach: number;
}

export class VolumetricFogPass extends Pass {
  readonly settings: VolumetricFogSettings = {
    base: 0, top: 2, thickness: 2, pool: 0, ground: null, groundRect: new THREE.Vector4(0, 0, 1, 1), groundRange: new THREE.Vector2(),
    density: 0.6, speed: 1, albedo: new THREE.Color(0xaab4ad),
    scale: new THREE.Vector3(0.32, 0.55, 0.32), wind: new THREE.Vector3(0.06, 0.012, 0.035), contrast: [0.32, 0.78],
    floor: 0.15, heightPow: 2, center: new THREE.Vector2(), swirl: 0, clear: 0, edge: 0, mask: null, maskRect: new THREE.Vector4(0, 0, 1, 1),
    board: new THREE.Vector4(-1e4, -1e4, 1e4, 1e4), reach: 10,
  };
  private readonly quad: FullScreenQuad;
  private readonly material: THREE.ShaderMaterial;
  private readonly noise = buildNoiseVolume();
  /** Half-resolution mist: rgb = glow, a = transmittance. */
  private readonly half = new THREE.WebGLRenderTarget(1, 1, { type: THREE.HalfFloatType, depthBuffer: false });
  private readonly compositeMaterial = new THREE.ShaderMaterial({
    vertexShader: VERTEX, fragmentShader: COMPOSITE, depthTest: false, depthWrite: false,
    uniforms: {
      tDiffuse: { value: null }, tDepth: { value: null }, tFog: { value: null },
      uFogSize: { value: new THREE.Vector2(1, 1) }, uNear: { value: 0.5 }, uFar: { value: 600 },
    },
  });
  private readonly compositeQuad = new FullScreenQuad(this.compositeMaterial);
  private lights: THREE.PointLight[] = [];
  private lightScan = 0;

  constructor(private readonly camera: THREE.PerspectiveCamera, private readonly scene: THREE.Scene,
    private readonly sun: THREE.DirectionalLight, private readonly hemi: THREE.HemisphereLight) {
    super();
    this.enabled = false;
    this.material = new THREE.ShaderMaterial({
      vertexShader: VERTEX, fragmentShader: FRAGMENT, depthTest: false, depthWrite: false,
      uniforms: {
        tNoise: { value: this.noise }, tDepth: { value: null },
        uProjInv: { value: new THREE.Matrix4() }, uTexel: { value: new THREE.Vector2() }, uCamWorld: { value: new THREE.Matrix4() }, uCamPos: { value: new THREE.Vector3() },
        uTime: { value: 0 }, uSpeed: { value: 1 }, uDensity: { value: 0.6 }, uBase: { value: 0 }, uTop: { value: 2 }, uMaxDist: { value: 400 },
        uAlbedo: { value: new THREE.Color() }, uAmbient: { value: new THREE.Color() },
        uSunDir: { value: new THREE.Vector3(0, 1, 0) }, uSunColor: { value: new THREE.Color() },
        uScale: { value: new THREE.Vector3() }, uWind: { value: new THREE.Vector3() }, uContrast: { value: new THREE.Vector2() },
        uFloor: { value: 0 }, uHeightPow: { value: 2 }, uCenter: { value: new THREE.Vector2() },
        uSwirl: { value: 0 }, uClear: { value: 0 }, uEdge: { value: 0 },
        tMask: { value: null }, uMaskRect: { value: new THREE.Vector4() }, uUseMask: { value: 0 },
        uBoard: { value: new THREE.Vector4() }, uReach: { value: 10 },
        tGround: { value: null }, uGroundRect: { value: new THREE.Vector4() }, uGroundRange: { value: new THREE.Vector2() },
        uUseGround: { value: 0 }, uThick: { value: 2 }, uPool: { value: 0 },
        uLightCount: { value: 0 },
        uLightPos: { value: Array.from({ length: MAX_LIGHTS }, () => new THREE.Vector3()) },
        uLightColor: { value: Array.from({ length: MAX_LIGHTS }, () => new THREE.Color()) },
        uLightRange: { value: new Array(MAX_LIGHTS).fill(1) },
      },
    });
    this.quad = new FullScreenQuad(this.material);
  }

  render(renderer: THREE.WebGLRenderer, writeBuffer: THREE.WebGLRenderTarget, readBuffer: THREE.WebGLRenderTarget, deltaTime?: number): void {
    const u = this.material.uniforms, s = this.settings, cam = this.camera;
    u.tDepth.value = readBuffer.depthTexture;
    u.uTexel.value.set(1 / readBuffer.width, 1 / readBuffer.height);
    u.uProjInv.value.copy(cam.projectionMatrixInverse);
    u.uCamWorld.value.copy(cam.matrixWorld);
    u.uCamPos.value.setFromMatrixPosition(cam.matrixWorld);
    u.uTime.value += deltaTime ?? 0.016;
    u.uSpeed.value = s.speed;
    u.uDensity.value = s.density;
    u.uBase.value = s.base;
    u.uTop.value = s.top;
    u.uMaxDist.value = cam.far;
    u.uAlbedo.value.copy(s.albedo);
    u.uScale.value.copy(s.scale);
    u.uWind.value.copy(s.wind);
    u.uContrast.value.set(s.contrast[0], s.contrast[1]);
    u.uFloor.value = s.floor;
    u.uHeightPow.value = s.heightPow;
    u.uCenter.value.copy(s.center);
    u.uSwirl.value = s.swirl;
    u.uClear.value = s.clear;
    u.uEdge.value = s.edge;
    u.tMask.value = s.mask;
    u.uMaskRect.value.copy(s.maskRect);
    u.uUseMask.value = s.mask ? 1 : 0;
    u.uBoard.value.copy(s.board);
    u.tGround.value = s.ground;
    u.uUseGround.value = s.ground ? 1 : 0;
    u.uGroundRect.value.copy(s.groundRect);
    u.uGroundRange.value.copy(s.groundRange);
    u.uThick.value = s.thickness;
    u.uPool.value = s.pool;
    u.uReach.value = s.reach;
    // ambient from the sky light, direct from the sun or moon, both as the scene sets them now
    u.uAmbient.value.copy(this.hemi.color).multiplyScalar(this.hemi.intensity * 0.55);
    u.uSunColor.value.copy(this.sun.color).multiplyScalar(this.sun.intensity * 0.12);
    u.uSunDir.value.subVectors(this.sun.position, this.sun.target.position).normalize();
    this.syncLights();
    // 1) march the mist at half resolution
    renderer.setRenderTarget(this.half);
    this.quad.render(renderer);
    // 2) lay it over the full-resolution frame
    const c = this.compositeMaterial.uniforms;
    c.tDiffuse.value = readBuffer.texture;
    c.tDepth.value = readBuffer.depthTexture;
    c.tFog.value = this.half.texture;
    c.uFogSize.value.set(this.half.width, this.half.height);
    c.uNear.value = cam.near;
    c.uFar.value = cam.far;
    renderer.setRenderTarget(this.renderToScreen ? null : writeBuffer);
    this.compositeQuad.render(renderer);
  }

  /** The brightest lit point lights near the mist, re-listed a few times a second. */
  private syncLights(): void {
    if (--this.lightScan <= 0) {
      this.lightScan = 15;
      const found: THREE.PointLight[] = [];
      this.scene.traverseVisible(o => { if ((o as THREE.PointLight).isPointLight) found.push(o as THREE.PointLight); });
      this.lights = found;
    }
    const u = this.material.uniforms, s = this.settings;
    const lit = this.lights
      .filter(l => l.intensity > 0 && l.visible)
      .map(l => ({ l, p: l.getWorldPosition(new THREE.Vector3()) }))
      .filter(({ p }) => p.y < s.top + 3)
      .sort((a, b) => b.l.intensity - a.l.intensity)
      .slice(0, MAX_LIGHTS);
    u.uLightCount.value = lit.length;
    lit.forEach(({ l, p }, i) => {
      u.uLightPos.value[i].copy(p);
      // a tight, faint haze at the flame (0.015 / 0.8 spread a blinding amber cloud over hexes)
      u.uLightColor.value[i].copy(l.color).multiplyScalar(Math.min(l.intensity, 60) * 0.006);
      u.uLightRange.value[i] = Math.max(1.5, l.distance || 4) * 0.45;
    });
  }

  /** The composer hands over the full frame size in pixels; the mist marches at half of it. */
  setSize(width: number, height: number): void {
    this.half.setSize(Math.max(1, Math.ceil(width / 2)), Math.max(1, Math.ceil(height / 2)));
  }

  dispose(): void {
    this.material.dispose(); this.quad.dispose();
    this.compositeMaterial.dispose(); this.compositeQuad.dispose();
    this.half.dispose(); this.noise.dispose();
  }
}
