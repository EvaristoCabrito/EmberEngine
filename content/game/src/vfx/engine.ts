/** Structural boundary for Ember's preserved WebOfDreams renderer, not combat logic. */
export const WEB_SHOT_TRAVEL = 0.85;
export interface BattleEngine {
  time: number;
  webZones: Array<{ center?: { x: number; y: number }; createdAt?: number; radius?: number; cells: Set<string> }>;
  units: Array<{ id: string; x: number; y: number; alive: boolean }>;
  effectAnchor(x: number, y: number): { worldX: number; worldY: number; worldZ?: number };
  webZoneRadiusTiles(radius: number): number;
  unitAnchor(unit: BattleEngine['units'][number]): { worldX: number; worldY: number; worldZ?: number };
  unitVisual(unit: BattleEngine['units'][number], tile: number): { footY: number; footOffset: number; h: number; scaleY: number; sway: number; bob: number; lift: number };
}
