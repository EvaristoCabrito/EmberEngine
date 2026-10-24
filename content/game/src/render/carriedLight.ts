import * as THREE from 'three';
import { flickerAt, type LightDef } from '../ember/lighting';

/** Softer than inverse-square so a fire lights several hexes, still brightest at the flame. */
const DECAY = 1.25;
/** Three's range window cuts light to zero at `distance`; set well past the def's radius so the
 * tail fades out on its own instead of ending in a hard-edged disc. */
const TAIL = 2.5;
/** Irradiance scale (was 4.9 with decay 1.25: every pool saturated into a flat white disc). */
const STRENGTH = 3.2;

export class CarriedLight extends THREE.PointLight {
  /** Options → Graphics → Local lights: off = every lamp, fire and carried torch gives no light
   * (the flames themselves still burn). Set by the battle each frame; the editor leaves it on. */
  static enabled = true;

  constructor(readonly definition: LightDef, readonly seed: number, radius = definition.radius, height = 1) {
    super(0xffffff, 0, Math.hypot(radius, height) * TAIL, DECAY);
    this.color.setRGB(...definition.color);
    this.position.y = height;
    this.name = 'Ember environmental point light';
  }

  update(time: number, fade = 1): void {
    this.intensity = !CarriedLight.enabled ? 0 : this.definition.intensity * STRENGTH * flickerAt(time, this.seed, this.definition.flicker) * Math.max(0, Math.min(1, fade));
  }
}
