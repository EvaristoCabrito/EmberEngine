import { DECORATIONS, placedFootprint, floorConnectorDirection } from "../ember/data";
import type { DecorationPlacement, TerrainId } from "../ember/types";
export const WATCHTOWER_FLOORS: Record<string, number> = { "watchtower-gate-floor": 0, "watchtower-barracks": 1, "watchtower-command": 2, "watchtower-beacon": 3, "watchtower-undercroft": -1, "watchtower-prison": -2 };

/** Placing an entrance replaces the wall records across both of its cells. */
export function removeWallsUnderWatchtowerEntrances(placements: DecorationPlacement[]): DecorationPlacement[] {
  const openings = new Set(placements.filter(p => p.id === "watchtower-stone-open-door-2hex")
    .flatMap(p => placedFootprint(p).map(f => `${p.x + f.dx},${p.y + f.dy}`)));
  return placements.filter(p => DECORATIONS[p.id]?.model3d !== "wall" ||
    !placedFootprint(p).some(f => openings.has(`${p.x + f.dx},${p.y + f.dy}`)));
}

/** Complete the existing floor boundary, including older activated editor drafts. */
export function closeWatchtowerWalls(id: string, tiles: TerrainId[], cols: number, rows: number, placements: DecorationPlacement[]): DecorationPlacement[] {
  if (!id.startsWith("watchtower-")) return placements;
  const floor = (x: number, y: number) => x >= 0 && y >= 0 && x < cols && y < rows && tiles[y * cols + x] !== "void";
  const boundary = (x: number, y: number) => floor(x, y) && (!floor(x - 1, y) || !floor(x + 1, y) || !floor(x, y - 1) || !floor(x, y + 1));
  // Only the entrance returns outside. Every other floor uses up/down connectors.
  let result: DecorationPlacement[] = placements.filter(p => !p.waypointStairs && (id === "watchtower-gate-floor" || !DECORATIONS[p.id]?.exitKind || DECORATIONS[p.id]?.exitKind === "connector"))
    .map(p => {
      const floors = WATCHTOWER_FLOORS;
      const connectorDirection = p.connectorDirection ?? (p.id === "floor-connector" && p.targetMapId && floors[p.targetMapId] != null && floors[id] != null
        ? floors[p.targetMapId] > floors[id] ? "up" as const : "down" as const : undefined);
      return { ...p, connectorDirection, id: p.id === "city-stone-banner-wall" ? "wall-3d-dungeon" : p.id };
    })
    .filter((p, i, all) => DECORATIONS[p.id]?.model3d !== "wall" || all.findIndex(a => DECORATIONS[a.id]?.model3d === "wall" && a.x === p.x && a.y === p.y) === i);
  const cellsOf = (p: DecorationPlacement) => placedFootprint(p).map(c => ({ x: p.x + c.dx, y: p.y + c.dy }));
  const entranceCells = new Set(result.filter(p => p.id === "watchtower-stone-open-door-2hex").flatMap(p => cellsOf(p).map(c => `${c.x},${c.y}`)));
  // The open entrance replaces the generated perimeter pieces underneath its span.
  result = removeWallsUnderWatchtowerEntrances(result);
  const occupied = new Set(result.filter(p => DECORATIONS[p.id]?.model3d).map(p => `${p.x},${p.y}`));
  for (const cell of entranceCells) occupied.add(cell);
  const wallId = result.find(p => DECORATIONS[p.id]?.model3d === "wall")?.id ?? "wall-3d-dungeon";
  for (let y = 0; y < rows; y++) for (let x = 0; x < cols; x++) {
    if (boundary(x, y) && !occupied.has(`${x},${y}`)) { result.push({ id: wallId, x, y }); occupied.add(`${x},${y}`); }
  }
  const degree = (x: number, y: number) => [[x-1,y],[x+1,y],[x,y-1],[x,y+1]].filter(([a,b]) => occupied.has(`${a},${b}`)).length;
  const ends = result.filter(p => DECORATIONS[p.id]?.model3d === "wall" && degree(p.x,p.y) < 2);
  for (const a of ends) for (const b of ends) {
    if (Math.abs(a.x-b.x)!==1 || Math.abs(a.y-b.y)!==1) continue;
    const join = [{x:a.x,y:b.y},{x:b.x,y:a.y}].find(p => floor(p.x,p.y) && !occupied.has(`${p.x},${p.y}`));
    if (join) { result.push({id:wallId,...join}); occupied.add(`${join.x},${join.y}`); }
  }
  // Saved waypoint positions and rotations are authored in the editor. Repairing
  // the enclosure must never silently move the objects shown in its preview.
  for (const p of result.filter(p => DECORATIONS[p.id]?.exitKind === "connector")) {
    const stairId = floorConnectorDirection(p) === "up" ? "stone-stairs-up-001" : "stone-stairs-down-001";
    // The stair is the waypoint; there is no separate marker on its landing.
    Object.assign(p, { id: stairId, waypointStairs: undefined, connectorDirection: floorConnectorDirection(p) });
  }
  return result;
}
