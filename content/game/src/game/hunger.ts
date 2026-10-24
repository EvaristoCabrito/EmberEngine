/** Fullness remaining, in percentage points. Travel costs half a bar per day. */
export const DAILY_HUNGER_COST = 50;
export function travelHungerCost(hours: number): number {
  return DAILY_HUNGER_COST * hours / 24;
}
export const ACTION_HUNGER_COST = 2;
export const INN_MEAL_PRICE = 2;
export const INN_FULLNESS = 120;

export function fullness(value: unknown): number {
  return typeof value === "number" && Number.isFinite(value) ? Math.max(0, Math.min(INN_FULLNESS, value)) : 100;
}

/** One ration feeds one living hero. Never waste food on an already full bar. */
export function useRation(save: SaveData, hero: string): SaveData {
  if (save.rations < 1 || (save.unitHp[hero] ?? 1) <= 0 || fullness(save.heroHunger[hero]) >= 100) return save;
  return { ...save, rations: save.rations - 1, heroHunger: { ...save.heroHunger, [hero]: 100 } };
}

export function buyInnMeal(save: SaveData, hero: string): SaveData {
  if (save.ember < INN_MEAL_PRICE || (save.unitHp[hero] ?? 1) <= 0 || fullness(save.heroHunger[hero]) >= INN_FULLNESS) return save;
  return { ...save, ember: save.ember - INN_MEAL_PRICE, heroHunger: { ...save.heroHunger, [hero]: INN_FULLNESS } };
}

export function drainHunger(value: unknown, cost: number): number {
  return Math.max(0, fullness(value) - cost);
}

export function cleanHunger(raw: unknown): Record<string, number> {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return {};
  return Object.fromEntries(Object.entries(raw).map(([hero, value]) => [hero, fullness(value)]));
}
import type { SaveData } from "./types";
