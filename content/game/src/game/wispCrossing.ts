import type { Mission } from "./types.ts";
import { DECORATIONS } from "./data.ts";

export const WISP_CROSSING_ID = "wisp-forest-crossing";
export const WISP_BOSS_ID = "wisp-forest-crossing-boss";

/** Old saves that already finished the crossing also skip the new, one-time room. */
export function wispCrossingCompleted(completed: readonly string[]): boolean {
  return completed.includes(WISP_CROSSING_ID) || completed.includes(WISP_BOSS_ID);
}

/** The first crossing ends only at the boss. Revisited exits lead outside as usual. */
export function routeWispCrossing(mission: Mission, completed: readonly string[]): Mission {
  if (mission.id !== WISP_CROSSING_ID) return mission;
  const finished = wispCrossingCompleted(completed);
  return {
    ...mission,
    win: "escape",
    decorations: mission.decorations?.map(p => {
      if (DECORATIONS[p.id]?.exitKind !== "dungeon" && p.targetMapId !== WISP_BOSS_ID) return p;
      return finished
        ? { ...p, id: "dungeon-exit", targetMapId: undefined, connectorDirection: undefined, returnConnector: undefined }
        : { ...p, id: "floor-connector", targetMapId: WISP_BOSS_ID, connectorDirection: "up", returnConnector: undefined };
    }),
  };
}

export function completedAfterWispVictory(missionId: string, completed: string[]): string[] {
  if (missionId === WISP_CROSSING_ID && !wispCrossingCompleted(completed)) return completed;
  const ids = missionId === WISP_BOSS_ID ? [WISP_BOSS_ID, WISP_CROSSING_ID] : [missionId];
  return [...new Set([...completed, ...ids])];
}
