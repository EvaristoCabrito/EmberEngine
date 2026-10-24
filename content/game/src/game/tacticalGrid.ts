/** Quiet tactical colors shared by the spatial and legacy renderers. */
export function tacticalGridStyle(fill: string): { fill: string; edge: string } {
  const m = /rgba?\(([^,]+),([^,]+),([^,]+)(?:,([^)]+))?\)/.exec(fill);
  const rgb = m ? `${m[1]},${m[2]},${m[3]}` : "220,230,238";
  const alpha = m?.[4] ? Number(m[4]) : 1;
  // Target markers retain their full strength; only movement uses the faint wash.
  if (rgb === "220,226,235" || rgb === "110,0,8") return { fill, edge: `rgba(${rgb},1)` };
  return { fill: `rgba(${rgb},${Math.min(0.12, alpha * 0.16)})`, edge: `rgba(${rgb},${Math.min(0.52, 0.18 + alpha * 0.36)})` };
}
export const GRID_MOVE = "rgba(112,174,220,0.5)";
export const GRID_ROUTE = "rgba(220,226,235,1)";
export const GRID_ALLY = "rgba(220,226,235,1)";
export const GRID_ENEMY = "rgba(231,133,115,1)";
export const GRID_ENEMY_TARGET = "rgba(110,0,8,0.85)";
export const GRID_ENEMY_GLOW = "rgba(205,24,38,0.78)";
export const GRID_OFFHAND_TARGET = "rgba(245,166,74,0.9)";

/** Quiet floor markings v2. Legacy palette and styling above remain available. */
export function tacticalGridStyleQuiet(fill: string): { fill: string; edge: string } {
  if (fill === GRID_MOVE) return { fill: "rgba(226,221,203,0.035)", edge: "rgba(226,221,203,0.26)" };
  if (fill === GRID_ENEMY_TARGET) return { fill: "rgba(214,162,102,0.065)", edge: "rgba(226,178,119,0.58)" };
  if (fill === GRID_OFFHAND_TARGET) return { fill: "rgba(245,166,74,0.18)", edge: "rgba(255,205,126,0.96)" };
  return tacticalGridStyle(fill);
}
