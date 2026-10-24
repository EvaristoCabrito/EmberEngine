import { getDefaultDevGfx, getDevGfx, setDevGfx, type DevGfxSettings } from "./gfx/three/devGfx";

export type GraphicsQuality = "low" | "medium" | "high";
const KEY = "emberash:graphicsQuality";
const profiles = {
  low: { maxDpr: 1, shadowResolution: 1024, realShadows: false, softShadows: false, contactShadows: false, ambientOcclusion: false, localLights: true, atmosphericFx: false },
  medium: { maxDpr: 1.5, shadowResolution: 2048, realShadows: true, softShadows: true, contactShadows: false, ambientOcclusion: true, localLights: true, atmosphericFx: true },
  high: { maxDpr: 2, shadowResolution: 4096, realShadows: true, softShadows: false, contactShadows: true, ambientOcclusion: true, localLights: true, atmosphericFx: true },
} as const;
const { maxDpr: _serverDpr, ...serverLighting } = profiles.high;
const SERVER_GFX: DevGfxSettings = { ...getDefaultDevGfx(), ...serverLighting };
let current: GraphicsQuality = "high";
try {
  const saved = localStorage.getItem(KEY);
  if (saved === "low" || saved === "medium" || saved === "high") current = saved;
} catch { /* Storage can be unavailable. */ }
const listeners = new Set<() => void>();
function apply(): void {
  const { maxDpr: _, ...lighting } = profiles[current];
  setDevGfx(lighting);
}
// Upgrade the earlier presets once; retain advanced overrides on later starts.
try {
  if (localStorage.getItem("emberash:graphicsProfilesVersion") !== "4") {
    apply(); localStorage.setItem("emberash:graphicsProfilesVersion", "4");
  }
} catch { /* Use session defaults. */ }
export function graphicsQualityIsCustom(gfx: DevGfxSettings = getDevGfx()): boolean {
  const { maxDpr: _, ...profile } = profiles[current];
  return Object.entries(profile).some(([key, value]) => gfx[key as keyof typeof gfx] !== value);
}
export function getGraphicsQualityServerGfx(): DevGfxSettings { return SERVER_GFX; }
export function getGraphicsQuality(): GraphicsQuality { return current; }
export function graphicsDpr(): number { return Math.min(window.devicePixelRatio || 1, profiles[current].maxDpr); }
export function setGraphicsQuality(quality: GraphicsQuality): void {
  current = quality;
  try { localStorage.setItem(KEY, quality); } catch { /* Session still works. */ }
  apply();
  listeners.forEach(listener => listener());
}
export function subscribeGraphicsQuality(listener: () => void): () => void {
  listeners.add(listener);
  return () => { listeners.delete(listener); };
}
