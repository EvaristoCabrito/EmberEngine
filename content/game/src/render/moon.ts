// The moon as a real object in the night sky: tonight's phase icon (public/game/ui/moon) on a
// disc that travels across the sky through the night, rising on one side, arcing over the board's
// far side and setting on the other. The stage's moonlight follows it (direction, height,
// strength), so light and shadows always match where the player sees the moon.
// The disc IS the light's source: it stands on the moonlight's own line from the board's centre,
// so the light, shadows and water reflection all point exactly at it. It rides high (see
// LIGHT_LOW/HIGH) so the board is really lit by it; the normal view (20° down) sees only up to
// ~5° below its own eye line, so the disc shows when the camera tilts up.

import * as THREE from 'three';
import { START_YAW } from './cameraRig';

/** Straight ahead of the normal view (its yaw looking along -z). */
const AHEAD = Math.atan2(-Math.sin(START_YAW), -Math.cos(START_YAW));
/** The disc's path: from this far to one side of ahead to as far to the other (radians). */
const SWEEP = 0.5;
/** The moon's angle above the horizon (radians): low at rise/set, highest at mid-night. High
 * enough (26°–43°) that its light reaches the ground instead of skimming it; the normal view's
 * frame then mostly sits below the disc (tilt the camera up to see it). */
const LIGHT_LOW = 0.45, LIGHT_HIGH = 0.75;
/** Its distance from the board's centre and its radius, as multiples of the board's half size. */
const DISTANCE = 1.2, RADIUS = 0.14;
/** Its brightest parts just cross bloom's threshold (2): a soft glow, surface still readable. */
const GLOW = 2.3;

const loader = new THREE.TextureLoader();
const textures = new Map<string, THREE.Texture>();
function iconTexture(url: string): THREE.Texture {
  let t = textures.get(url);
  if (!t) { t = loader.load(url); t.colorSpace = THREE.SRGBColorSpace; textures.set(url, t); }
  return t;
}

export interface MoonPose { azimuth: number; lightElevation: number; height: number }

export class Moon extends THREE.Mesh<THREE.PlaneGeometry, THREE.ShaderMaterial> {
  constructor() {
    super(new THREE.PlaneGeometry(2, 2), new THREE.ShaderMaterial({
      uniforms: { uMap: { value: null }, uGlow: { value: GLOW } },
      vertexShader: 'varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
      fragmentShader: `uniform sampler2D uMap; uniform float uGlow; varying vec2 vUv;
        void main(){
          vec4 t = texture2D(uMap, vUv);
          if (t.a < 0.01) discard;
          gl_FragColor = vec4(t.rgb * uGlow, t.a);
        }`,
      transparent: true, depthWrite: false, fog: false,
    }));
    this.name = 'Moon';
    this.renderOrder = -10;
    this.frustumCulled = false;
    this.visible = false;
  }

  /** Tonight's phase art (the blood moon's icon is already red). */
  setIcon(url: string): void { this.material.uniforms.uMap.value = iconTexture(url); }

  /** Where the moon is at `progress` through the night (0 = rising, 0.5 = highest, 1 = setting). */
  static pose(progress: number): MoonPose {
    const p = THREE.MathUtils.clamp(progress, 0, 1), height = Math.sin(Math.PI * p);
    return { azimuth: AHEAD + SWEEP * (1 - 2 * p), lightElevation: LIGHT_LOW + (LIGHT_HIGH - LIGHT_LOW) * height, height };
  }

  /** Put the disc on the light's line (board centre at the origin), facing the camera. */
  place(pose: MoonPose, camera: THREE.Camera, boardHalf: number): void {
    const e = pose.lightElevation, ce = Math.cos(e), d = boardHalf * DISTANCE;
    this.position.set(Math.sin(pose.azimuth) * ce * d, Math.sin(e) * d, Math.cos(pose.azimuth) * ce * d);
    // the icons' disc fills ~80% of the 256px image
    this.scale.setScalar((boardHalf * RADIUS) / 0.8);
    this.quaternion.copy(camera.quaternion);
  }
}
