/**
 * Environmental light sources — map decorations that illuminate their surroundings
 * (braziers, burning buildings, lantern posts...). Each becomes a real THREE.PointLight in
 * ThreeBattleRenderer (see syncLights), placed at the flame in 3D and lighting the terrain,
 * decorations and characters through their lit (MeshLambertMaterial) materials.
 *
 * World units are the battle's world pixels (hexWorld / unitAnchor space, y-down); a light's
 * range is given in hex radii (`tile`) so zoom never changes which area it lights.
 */

export interface LightDef {
  /** Linear RGB of the light, before intensity. */
  color: [number, number, number];
  /** Strength: irradiance one hex radius from the flame, as a multiple of the ground's normal
   * sun + sky irradiance (1 = as much light again as the scene already has there). */
  intensity: number;
  /** PointLight.distance, in hex radii: Three's attenuation reaches exactly zero here. */
  radius: number;
  /** Flicker amount (0 = steady light, 1 = full fire flicker). */
  flicker: number;
}

const FIRE: [number, number, number] = [1.0, 0.58, 0.26];
const LANTERN: [number, number, number] = [1.0, 0.7, 0.36];

// Three lights in linear space and the screen shows gamma-encoded color: +70% linear
// irradiance reads as only ~+25% on screen (measured). Strong fixtures use several times the
// baseline irradiance so their warm point lights visibly reach the surrounding scene.
const NORMAL_FIRE: LightDef = { color: FIRE, intensity: 8, radius: 3.6, flicker: 1 };
const NORMAL_LANTERN: LightDef = { color: LANTERN, intensity: 7, radius: 3.3, flicker: 0.4 };
const WEAK_FIRE: LightDef = { color: FIRE, intensity: 3, radius: 2.4, flicker: 1 };

/** Which decorations emit light, and how strongly (per the user's list). */
export const LIGHT_DEFS: Record<string, LightDef> = {
  "farmland-photographic-013-forge": { ...NORMAL_FIRE, intensity: 3 },
  "cave-basalt-006-brazier": { ...NORMAL_FIRE, intensity: 4 },
  "cave-basalt-006-campfire": { ...NORMAL_FIRE, intensity: 4 },
  "cave-basalt-005-brazier": { ...NORMAL_FIRE, intensity: 4 },
  "cave-basalt-004-brazier": NORMAL_FIRE,
  "cave-basalt-004-campfire": NORMAL_FIRE,
  // Open flames and braziers share one reach; intensity carries their visual difference.
  "city-brazier": NORMAL_FIRE,
  Brazier3: NORMAL_FIRE,
  "wilds-brazier-tripod": NORMAL_FIRE,
  "city-campfire": NORMAL_FIRE,
  "wilds-campfire-cauldron": NORMAL_FIRE,
  "city-forge": { ...NORMAL_FIRE, intensity: 11 },
  "ember-channels-001": WEAK_FIRE,
  "burning-house": NORMAL_FIRE,
  "burnt-house-ruins": NORMAL_FIRE,
  "burning-hamlet": NORMAL_FIRE,
  // Candles/incense use the same short reach; multi-flame fixtures are brighter, not wider.
  "light-candle": { color: [1.0, 0.66, 0.34], intensity: 3, radius: 2.4, flicker: 0.6 },
  "wilds-incense-burner": { ...WEAK_FIRE, intensity: 2.4 },
  "wilds-candle-menhir": NORMAL_LANTERN,
  "city-shrineCandle": NORMAL_LANTERN,
  Chandelier: NORMAL_LANTERN,
  "city-lantern-post": NORMAL_LANTERN,
  "wilds-lantern-post": NORMAL_LANTERN,
  // Marco's marker lantern gives a local pool of light without the wide reach of a full
  // street lantern or a brazier.
  "wilds-lantern-signpost": { color: LANTERN, intensity: 4, radius: 1.35, flicker: 0.35 },
  // Bioluminescent Wisp Forest mushrooms cast a compact, steady blue pool.
  "wilds-wisp-mushrooms": { color: [0.34, 0.62, 1.0], intensity: 2.2, radius: 2.2, flicker: 0.12 },
  // Red wisps mark the waystone, forgotten shrine, and hanging marker lantern.
  "wilds-wisp-waystone": { color: [1.0, 0.12, 0.16], intensity: 2.5, radius: 2.0, flicker: 0.2 },
  "wilds-wisp-shrine": { color: [1.0, 0.12, 0.16], intensity: 2.5, radius: 2.0, flicker: 0.2 },
  "wilds-wisp-lantern-marker": { color: [1.0, 0.12, 0.16], intensity: 2.5, radius: 2.0, flicker: 0.2 },
  lamppost: NORMAL_LANTERN,
  // Wall torches and bowls share normal-fire reach; the fireplace has extra intensity only.
  "light-wall-torch": { ...NORMAL_FIRE, intensity: 11 },
  "light-brazier-bowl": NORMAL_FIRE,
  "light-fireplace": { ...NORMAL_FIRE, intensity: 14, flicker: 0.8 },
  "inn-fireplace": { ...NORMAL_FIRE, intensity: 14, flicker: 0.8 },
};

/** Reach multiplier for every light-emitting decoration in LIGHT_DEFS, applied in
 * ThreeBattleRenderer.syncLights — per direct request, all light props get a larger radius. */
export const LIGHT_RADIUS_MUL = 1.3;

/** Units that carry their own light and walk with it, keyed by classId — same LightDef as the
 * props above. Swamp Blue Calf: a soft pale-blue glow, between WEAK_FIRE and NORMAL_FIRE in
 * strength and reach, with a gentle slow pulse instead of a fire's flicker. */
export const UNIT_LIGHT_DEFS: Record<string, LightDef> = {
  // Summoned familiars carry compact magical light pools. The stronger tiers reach two
  // hexes; Familiar Radiante keeps the base familiar's cool glow, with a little more strength,
  // while Familiar Titã spreads a restrained red glow over each occupied body hex so its full
  // footprint and adjacent target area receive the light.
  familiar: { color: [0.68, 0.82, 1.0], intensity: 1.5, radius: 1, flicker: 0.08 },
  familiar2: { color: [0.64, 0.76, 1.0], intensity: 1.8, radius: 2, flicker: 0.08 },
  familiar4: { color: [0.68, 0.82, 1.0], intensity: 1.8, radius: 2, flicker: 0.08 },
  familiar3: { color: [1.0, 0.08, 0.1], intensity: 0.7, radius: 2, flicker: 0.04 },
  swampBlueCalf: { color: [0.55, 0.75, 1.0], intensity: 1.8, radius: 3, flicker: 0.3 },
};

/** One live light this frame (world pixels, y-down), already scaled by zoom and flicker. */
export interface EnvLight {
  /** Ground position under the flame. */
  x: number;
  y: number;
  /** Flame height above the ground, world pixels. */
  h: number;
  /** Reach, world pixels. */
  r: number;
  /** Optional Three.js attenuation exponent; defaults to the map lighting standard. */
  decay?: number;
  /** Exponent the intensity is normalized with (irradiance one hex away = LightDef.intensity);
   * defaults to LIGHT_DECAY. Map props and unit lights use MAP_LIGHT_DECAY for both. */
  normDecay?: number;
  /** color x intensity x flicker. */
  rgb: [number, number, number];
}

/** PointLight.decay. 2 is physical inverse-square; 1.5 keeps a lit pool around a fire instead
 * of only a hot spot at its base. */
export const LIGHT_DECAY = 1.5;

/** Attenuation for map light props and unit lights (spells keep LIGHT_DECAY). Softer than 1.5 so
 * a lamp's light keeps reaching the ground a few hexes out instead of dying within one hex. */
export const MAP_LIGHT_DECAY = 1.25;
/** Lowest a map light's flame may sit above the ground, in hex radii. Measured flames were only
 * 0.7–0.9 hex up; a light that close to a flat floor pours almost everything into the hex under
 * it (1 hex away got ~30% of the center). Raising it spreads the same light over a wider pool. */
export const MAP_LIGHT_MIN_HEIGHT = 1;
/** Bounce fill: each of the nearest map lights also drives a wide, dim, high light standing in
 * for light bouncing off the lit ground onto its surroundings. Irradiance straight under it is
 * this fraction of the main light's one-hex value, reaching BOUNCE_RADIUS_MUL x its range. */
export const BOUNCE_FRACTION = 0.12;
export const BOUNCE_RADIUS_MUL = 2;
/** Bounce light height above the ground, in hex radii — high, so it spreads flat and even. */
export const BOUNCE_HEIGHT = 2.5;

/** Smooth, non-repeating-looking fire variation around 1 — a few incommensurate slow sines,
 * never a per-frame random jump. `seed` decorrelates neighbouring fires. */
export function flickerAt(time: number, seed: number, amount: number): number {
  if (amount <= 0) return 1;
  const v = 0.06 * Math.sin(time * 5.3 + seed) + 0.045 * Math.sin(time * 8.9 + seed * 1.7) + 0.03 * Math.sin(time * 13.7 + seed * 2.9) + 0.03 * Math.sin(time * 2.1 + seed * 0.6);
  return 1 + v * amount;
}
