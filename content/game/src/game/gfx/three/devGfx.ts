/** Dev-only graphics toggles for ThreeBattleRenderer, flipped from the Dev Controls screen
 * (Modo teste → Dev Controls) and persisted per-browser in localStorage. The renderer reads
 * getDevGfx() every frame, so a change applies to the next battle frame with no reload. */
export interface DevGfxSettings {
  /** The sun's real cast shadows (units + props) at all — off gives a clean A/B baseline. */
  realShadows: boolean;
  /** Directional shadow depth-map edge; point lights use a quarter of this. */
  shadowResolution: 1024 | 2048 | 4096;
  /** Widens the PCF filter radius and reduces shadow strength for lighter, softer edges.
   * (PCFSoftShadowMap was removed in this Three.js version — radius is the knob now.) */
  softShadows: boolean;
  /** Ground receiver shadow comparison closes the bias gap using the full caster silhouette. */
  contactShadows: boolean;
  /** Environmental ambient occlusion in the terrain's lighting, from props and raised/
   * blocking terrain (see ThreeGroundAO.ts). */
  ambientOcclusion: boolean;
  /** Fog of war on maps that use it (Mission.fog). Off = the whole system is off for
   * comparison: every hex visible, every enemy shown. */
  fogOfWar: boolean;
  /** Shows the raw fog states as flat per-hex colors instead of the feathered mask:
   * green = visible, amber = explored, red = unexplored. */
  fogDebug: boolean;
  /** Environmental light from map light sources (braziers, burning houses, lanterns...) on
   * terrain, decorations and characters — see lighting.ts. */
  localLights: boolean;
  /** Mission mist, fog, wisps, embers, and screen-space fog vignettes. */
  atmosphericFx: boolean;
  /** Sun position: azimuth = screen direction its shadows fall (deg, 0 = right, 90 = down);
   * elevation = height above the horizon (deg). Defaults are the game's standing sun. */
  sunAzimuth: number;
  sunElevation: number;
  /** Moon position, same convention. */
  moonAzimuth: number;
  moonElevation: number;
}

const KEY = "emberash:devGfx";
const DEFAULTS: DevGfxSettings = { realShadows: true, shadowResolution: 2048, softShadows: false, contactShadows: true, ambientOcclusion: true, fogOfWar: true, fogDebug: false, localLights: true, atmosphericFx: true, sunAzimuth: 53.13, sunElevation: 45, moonAzimuth: 140, moonElevation: 35 };

function load(): DevGfxSettings {
  try {
    const raw = window.localStorage.getItem(KEY);
    if (raw) return { ...DEFAULTS, ...(JSON.parse(raw) as Partial<DevGfxSettings>) };
  } catch {
    // localStorage unavailable or corrupt — defaults
  }
  return { ...DEFAULTS };
}

let current: DevGfxSettings = load();
const listeners = new Set<() => void>();

export function getDevGfx(): DevGfxSettings {
  return current;
}

/** Stable server-render snapshot; browser-only saved overrides apply after hydration. */
export function getDefaultDevGfx(): DevGfxSettings {
  return DEFAULTS;
}

export function setDevGfx(patch: Partial<DevGfxSettings>): void {
  current = { ...current, ...patch };
  try {
    window.localStorage.setItem(KEY, JSON.stringify(current));
  } catch {
    // not persisted — still applies for this session
  }
  for (const l of listeners) l();
}

/** useSyncExternalStore-compatible subscribe. */
export function subscribeDevGfx(listener: () => void): () => void {
  listeners.add(listener);
  return () => listeners.delete(listener);
}
