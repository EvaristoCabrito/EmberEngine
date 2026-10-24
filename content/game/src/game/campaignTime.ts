import type { MapTimeOfDay, SaveData } from "./types";

export function usesTravelClock(save: SaveData): boolean {
  return save.completed.includes("thebridge");
}
export function campaignHour(save: SaveData): number {
  return save.gameHour ?? 8;
}
export function campaignTimeOfDay(hour: number): MapTimeOfDay {
  if (hour < 5) return "darkNight";
  if (hour < 8) return "dawn";
  if (hour < 11) return "day";
  if (hour < 15) return "noon";
  if (hour < 18) return "day";
  if (hour < 21) return "dusk";
  return "brightNight";
}
