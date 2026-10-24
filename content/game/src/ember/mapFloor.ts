import { DECORATIONS, placedFootprint } from "./data";
import type { DecorationPlacement, TerrainId } from "./types";

export interface FloorRect { minX: number; minY: number; maxX: number; maxY: number; parts?: FloorRect[] }

/** Legacy boards infer an orthogonal perimeter from a filled rectangle;
 * authored interiors can explicitly enable or disable that mode. */
export function hasSquareMapBorder(tiles: TerrainId[], cols: number, rows: number, squareTiles?: boolean): boolean {
  if (squareTiles !== undefined) return squareTiles;
  let minX = cols, minY = rows, maxX = -1, maxY = -1, count = 0;
  for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) {
    if (!tiles[y * cols + x] || tiles[y * cols + x] === "void") continue;
    minX = Math.min(minX, x); maxX = Math.max(maxX, x);
    minY = Math.min(minY, y); maxY = Math.max(maxY, y); count++;
  }
  return count > 0 && count === (maxX - minX + 1) * (maxY - minY + 1);
}

export const floorRectParts = (rect: FloorRect): FloorRect[] => rect.parts ?? [rect];
export const floorRectContains = (rect: FloorRect, x: number, y: number): boolean => floorRectParts(rect).some(r => x >= r.minX && x <= r.maxX && y >= r.minY && y <= r.maxY);

/** Tile-normalized rectangular floor, ending under the outside face of authored walls. */
export function mapFloorRects(tiles: TerrainId[], cols: number, rows: number, decorations: DecorationPlacement[]): Map<number, FloorRect> {
  const rects = new Map<number, FloorRect>();
  const width = Math.sqrt(3);
  const floor = (x: number, y: number) => x >= 0 && y >= 0 && x < cols && y < rows && tiles[y * cols + x] !== "void";
  const walls = new Set(decorations.filter(p => DECORATIONS[p.id]?.model3d === "wall" || DECORATIONS[p.id]?.model3d === "secretDoor").map(p => `${p.x},${p.y}`));
  for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) if (floor(x, y))
    rects.set(y * cols + x, { minX: x * width, maxX: (x + 1) * width, minY: y * 1.5, maxY: (y + 1) * 1.5 });
  for (const placement of decorations) {
    const def = DECORATIONS[placement.id];
    if (def?.model3d !== "wall" && def?.model3d !== "secretDoor" && placement.id !== "watchtower-stone-open-door-2hex") continue;
    for (const cell of placedFootprint(placement)) {
      const p = { x: placement.x + cell.dx, y: placement.y + cell.dy };
      const rect = rects.get(p.y * cols + p.x);
      if (!rect) continue;
      const half = 0.16 * (def.wallThicknessScale ?? 1);
      const cx = (p.x + 0.5) * width, cy = (p.y + 0.5) * 1.5;
      if (!floor(p.x - 1, p.y)) rect.minX = Math.max(rect.minX, cx - half);
      if (!floor(p.x + 1, p.y)) rect.maxX = Math.min(rect.maxX, cx + half);
      if (!floor(p.x, p.y - 1)) rect.minY = Math.max(rect.minY, cy - half);
      if (!floor(p.x, p.y + 1)) rect.maxY = Math.min(rect.maxY, cy + half);
      // At an inward step, the corner tile has floor on all four sides. Its outside
      // diagonal is still void; stop that tile beneath the two joining wall arms.
      for (const dx of [-1, 1]) for (const dy of [-1, 1]) {
        if (floor(p.x + dx, p.y + dy) || !walls.has(`${p.x + dx},${p.y}`) || !walls.has(`${p.x},${p.y + dy}`)) continue;
        const cutX = cx + dx * half, cutY = cy + dy * half;
        // Remove only the outside quadrant. Trimming the whole rectangle also
        // erased the two inside arms, exposing the backdrop beside a closed wall.
        rect.parts = floorRectParts(rect).flatMap(r => {
          const vertical = { minX: dx < 0 ? Math.max(r.minX, cutX) : r.minX, maxX: dx > 0 ? Math.min(r.maxX, cutX) : r.maxX, minY: r.minY, maxY: r.maxY };
          const horizontal = { minX: dx > 0 ? Math.max(r.minX, cutX) : r.minX, maxX: dx < 0 ? Math.min(r.maxX, cutX) : r.maxX, minY: dy < 0 ? Math.max(r.minY, cutY) : r.minY, maxY: dy > 0 ? Math.min(r.maxY, cutY) : r.maxY };
          return [vertical, horizontal].filter(p => p.maxX > p.minX && p.maxY > p.minY);
        });
      }
    }
  }
  return rects;
}
