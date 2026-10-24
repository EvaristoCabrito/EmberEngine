// Ember lighting presets, copied unchanged from Ember src/game/gfx/three/ThreeBattleRenderer.ts
// (only these constants; the old renderer itself is not imported).
import type { MapTimeOfDay } from "./types";

/** Per-map time of day (Mission.timeOfDay, picked in the Map Editor's "Iluminação"): which sky
 * light is the key light (Sun or Moon), the default key/ambient intensities the editor's sliders
 * snap to when the time is picked, the key light and sky-fill colors, and for dawn/dusk a low sun
 * elevation (long shadows) in place of Dev Controls' "Sol — altura". Dark night is the old Dev
 * Controls "Noite" (Moon 0.9, sky fill 12% of the daytime 2). */
export const TIME_OF_DAY_LIGHT: Record<MapTimeOfDay, { label: string; moon: boolean; key: number; ambient: number; keyColor: number; skyColor: number; elevation?: number }> = {
  day: { label: "Dia", moon: false, key: 3.5, ambient: 1.4, keyColor: 0xfff0d6, skyColor: 0xfff2df },
  noon: { label: "Meio-dia", moon: false, key: 5, ambient: 2, keyColor: 0xfff0d6, skyColor: 0xfff2df },
  dawn: { label: "Amanhecer", moon: false, key: 3.2, ambient: 1.3, keyColor: 0xffc8a8, skyColor: 0xf2d8d4, elevation: 20 },
  dusk: { label: "Entardecer", moon: false, key: 2.8, ambient: 1.1, keyColor: 0xffca9f, skyColor: 0xf7ddc3, elevation: 15 },
  brightNight: { label: "Noite clara", moon: true, key: 1.8, ambient: 0.6, keyColor: 0x9fb4ff, skyColor: 0xb4c0e4 },
  darkNight: { label: "Noite escura", moon: true, key: 0.9, ambient: 0.24, keyColor: 0x9fb4ff, skyColor: 0xb4c0e4 },
};

/** Default sun/ambient intensities, used whenever a mission doesn't set its own
 * `sunIntensity`/`ambientIntensity` (see types.ts) — also what the Map Editor's "Iluminação"
 * sliders default a new/untouched map to (see GameApp.tsx), so the editor's default and the
 * renderer's fallback can never drift apart. Set to match the exact values the user tuned by
 * hand on "O Vau" (saved as vau016.json). Those previous noon values stay available through
 * TIME_OF_DAY_LIGHT.noon; the everyday daytime default is intentionally softer. */
export const DEFAULT_SUN_INTENSITY = 3.5;
export const DEFAULT_AMBIENT_INTENSITY = 1.4;
/** MILESTONE 4 — real post-processing (UnrealBloomPass on the actual rendered scene, via
 * EffectComposer), not a CSS/canvas filter pretending to be one. Matches the user's own tuned
 * "O Vau" setup (vau016.json), the standard daytime default — see DEFAULT_SUN_INTENSITY's
 * comment. Bloom now applies to the whole scene (see render()'s own comment), not just wisp
 * embers, so a high intensity CAN wash out bright ground art too — that's expected now. */
export const DEFAULT_BLOOM_INTENSITY = 0.9;
