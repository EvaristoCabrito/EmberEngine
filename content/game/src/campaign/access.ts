import { heroRecruited } from "../ember/data";
import type { SaveData, WorldLocation } from "../ember/types";
import { missionsForLocation } from "./mapstore";
import { isGatedMission, type MissionAccess, type ProgressExtras } from "./progression";
export type MissionAccessFn = (missionId: string) => MissionAccess;
/** What the save alone cannot tell progression conditions: owned gear and who is in the party. */
export function progressionExtras(save: SaveData): ProgressExtras {
  const items = [
    ...Object.keys(save.weapons ?? {}),
    ...Object.keys(save.looseEquipment ?? {}),
    ...Object.values(save.equipment ?? {}).flatMap((slots) => Object.values(slots).filter((id): id is string => typeof id === "string")),
  ];
  const party = ["Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"].filter((name) => heroRecruited(name, save.completed, save.flags));
  return { items, party };
}

/** The player advances through a location chapter-by-chapter. A new location becomes
 * available only when every chapter in the prior populated location is complete. Missions
 * governed by a progression gate (progression.json) sit outside that chain: they open through
 * their own quest/chapter conditions and never block the next location. */
export function previousPopulatedLocation(location: WorldLocation, locations: WorldLocation[]): WorldLocation | null {
  const at = locations.findIndex((candidate) => candidate.id === location.id);
  for (let i = at - 1; i >= 0; i -= 1) {
    const previous = locations[i]!;
    if (missionsForLocation(previous).some((mission) => !isGatedMission(mission.id))) return previous;
  }
  return null;
}

export function lockedMission(
  id: string,
  completed: string[],
  test: boolean,
  locations: WorldLocation[],
  fallbackOrder: string[],
  access?: MissionAccessFn,
): boolean {
  if (test) return false;
  if (completed.includes(id)) return true;
  if (access && isGatedMission(id)) return access(id) !== "available";

  const location = locations.find((candidate) => candidate.missionIds.includes(id));
  if (location?.openAccess) return false;
  if (!location) {
    const at = fallbackOrder.indexOf(id);
    return at < 0 || (at > 0 && !fallbackOrder.slice(0, at).every((previousId) => completed.includes(previousId)));
  }

  const ids = missionsForLocation(location).map((mission) => mission.id).filter((missionId) => !isGatedMission(missionId));
  const at = ids.indexOf(id);
  if (at < 0) return true;
  if (at > 0) return !ids.slice(0, at).every((previousId) => completed.includes(previousId));

  const previousLocation = previousPopulatedLocation(location, locations);
  return previousLocation !== null && !missionsForLocation(previousLocation).filter((mission) => !isGatedMission(mission.id)).every((mission) => completed.includes(mission.id));
}

export function missionStatus(
  id: string,
  completed: string[],
  test: boolean,
  locations: WorldLocation[],
  fallbackOrder: string[],
  access?: MissionAccessFn,
): "locked" | "available" | "done" {
  if (completed.includes(id)) return "done";
  return lockedMission(id, completed, test, locations, fallbackOrder, access) ? "locked" : "available";
}

export function locationStatus(
  location: WorldLocation,
  completed: string[],
  test: boolean,
  locations: WorldLocation[],
  access?: MissionAccessFn,
): "locked" | "available" | "done" {
  // Hidden missions are not on the map, so they never count toward what the location holds.
  const missions = missionsForLocation(location).filter((mission) => !access || test || access(mission.id) !== "hidden");
  const pending = missions.filter((mission) => !completed.includes(mission.id));
  // Debug mode opens every location, including ones with no missions assigned yet (the new
  // areas); the campaign keeps its chapter gating.
  if (pending.length === 0) return missions.length > 0 ? "done" : test ? "available" : "locked";
  const order = missionsForLocation(location).map((mission) => mission.id);
  return pending.some((mission) => missionStatus(mission.id, completed, test, locations, order, access) !== "locked") ? "available" : "locked";
}

