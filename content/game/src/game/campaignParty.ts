import { heroRecruited } from "./data.ts";
import { partyLeaderOf } from "./partyFormation.ts";
import type { ClassId, Mission, SaveData } from "./types.ts";

export const CAMPAIGN_HERO_CLASSES: Record<string, ClassId> = {
  Kael: "swordsman", Neera: "archer", Voss: "mage", Salazar: "healer", Aldric: "aldric", Malrec: "conjurer",
};

/** Campaign progression defines the party; editor spawns only provide entry positions. */
export function campaignPartySetup(mission: Mission, save: SaveData): { mission: Mission; roster: Record<string, ClassId> } {
  const recruited = (name: string) => heroRecruited(name, save.completed, save.flags);
  const roster = Object.fromEntries(Object.entries(CAMPAIGN_HERO_CLASSES)
    .filter(([name]) => recruited(name))
    .map(([name, base]) => [name, save.promotions[name] ?? base]));
  if (mission.explore) {
    const leader = partyLeaderOf(save.partyLeader, recruited);
    const entry = mission.playerSpawns[0];
    return { roster: {}, mission: { ...mission,
      playerSpawns: entry ? [{ name: leader, classId: roster[leader] ?? CAMPAIGN_HERO_CLASSES[leader], x: entry.x, y: entry.y }] : [],
      neutralSpawns: mission.neutralSpawns?.filter(spawn => spawn.name !== leader),
    } };
  }
  const seen = new Set<string>();
  const playerSpawns = mission.playerSpawns.filter(spawn => {
    if (!roster[spawn.name] || seen.has(spawn.name)) return false;
    seen.add(spawn.name);
    return true;
  }).map(spawn => ({ name: spawn.name, classId: roster[spawn.name], x: spawn.x, y: spawn.y }));
  // If an editor used generic names, keep its first entry hex as the party's anchor.
  if (!playerSpawns.length && mission.playerSpawns[0]) {
    const name = partyLeaderOf(save.partyLeader, recruited);
    const entry = mission.playerSpawns[0];
    playerSpawns.push({ name, classId: roster[name] ?? CAMPAIGN_HERO_CLASSES[name], x: entry.x, y: entry.y });
  }
  return { roster, mission: { ...mission, playerSpawns,
    neutralSpawns: mission.neutralSpawns?.filter(spawn => !roster[spawn.name]),
  } };
}
