/** Progression layer: chapters, quests and mission gating, driven by one generic condition
 * evaluator instead of per-map code.
 *
 * The idea is to separate "this mission exists" from "the party currently knows about it / can
 * enter it". A mission with NO entry in progression.json's `missionGates` behaves exactly as it
 * always has (the location's normal chapter-by-chapter chain). A gated mission is governed only
 * by its gate:
 *
 *   hidden     — `visible` is false: not on the map at all.
 *   revealed   — visible, but `unlock` is false: shown, cannot be entered yet.
 *   available  — visible and unlocked.
 *   completed  — already finished.
 *
 * Conditions are plain JSON so they can be authored/edited as data (see progression.json):
 *   { "all": [...] }  { "any": [...] }  { "not": {...} }
 *   { "chapterReached": 2 }            { "questDiscovered": "id" }   { "questActive": "id" }
 *   { "questStage": ["id", 2] }        { "questCompleted": "id" }    { "missionCompleted": "id" }
 *   { "npcTalkedTo": "brue" }          { "flagSet": "name" }         { "itemOwned": "weaponOrGearId" }
 *   { "partyMemberPresent": "Neera" }
 *
 * Quest stages: 0 = not taken, 1 = accepted, 2 = objectives met (ready to hand in), 3 = done.
 *
 * Keep this file free of runtime imports from the rest of the game (only ./quests, itself
 * dependency-free): it is unit-tested directly under node. */

import CONFIG from "./progression.json" with { type: "json" };
import { QUESTS, questById, questStage } from "./quests.ts";

export type Condition =
  | { all: Condition[] }
  | { any: Condition[] }
  | { not: Condition }
  | { chapterReached: number }
  | { questDiscovered: string }
  | { questActive: string }
  | { questStage: [string, number] }
  | { questCompleted: string }
  | { missionCompleted: string }
  | { npcTalkedTo: string }
  | { flagSet: string }
  | { itemOwned: string }
  | { partyMemberPresent: string };

/** Things a quest, a chapter trigger or (later) a dialogue/cutscene can do to progression. */
export type Effect = { setChapter: number } | { setFlag: string } | { discoverQuest: string };

export interface ChapterDef {
  id: number;
  name: string;
  /** Optional: the chapter counts as reached as soon as this holds (a completed mission or
   * quest, say). Chapters can also be set explicitly by a `setChapter` effect. */
  reachedWhen?: Condition | null;
}

export interface MissionGate {
  visible?: Condition | null;
  unlock?: Condition | null;
}

export interface Trigger {
  when: Condition;
  do: Effect[];
}

export type MissionAccess = "hidden" | "revealed" | "available" | "completed";

/** The slice of a save the progression layer reads and writes. */
export interface ProgressSave {
  completed: string[];
  chapter?: number;
  flags?: string[];
  npcTalked?: string[];
  questsDiscovered?: string[];
  questsActive?: string[];
  questsDone?: string[];
  questItems?: string[];
  questKills?: string[];
}

/** What the save alone cannot say: caller-supplied (see GameApp's progressionExtras). */
export interface ProgressExtras {
  /** Every weapon / gear id the party owns. */
  items?: string[];
  /** Heroes currently in the party. */
  party?: string[];
}

const CHAPTERS: ChapterDef[] = (CONFIG.chapters ?? []) as ChapterDef[];
const GATES: Record<string, MissionGate> = (CONFIG.missionGates ?? {}) as Record<string, MissionGate>;
const TRIGGERS: Trigger[] = (CONFIG.triggers ?? []) as Trigger[];

export function chapterDefs(): ChapterDef[] {
  return CHAPTERS;
}

export function missionGateFor(missionId: string): MissionGate | undefined {
  return GATES[missionId];
}

/** Whether a mission is governed by a gate at all (and so stays out of the location's chain). */
export function isGatedMission(missionId: string): boolean {
  return GATES[missionId] !== undefined;
}

export function evaluate(condition: Condition | null | undefined, save: ProgressSave, extras: ProgressExtras = {}): boolean {
  if (!condition) return true;
  if ("all" in condition) return condition.all.every((c) => evaluate(c, save, extras));
  if ("any" in condition) return condition.any.some((c) => evaluate(c, save, extras));
  if ("not" in condition) return !evaluate(condition.not, save, extras);
  if ("chapterReached" in condition) return currentChapter(save, extras) >= condition.chapterReached;
  if ("missionCompleted" in condition) return save.completed.includes(condition.missionCompleted);
  if ("npcTalkedTo" in condition) return (save.npcTalked ?? []).includes(condition.npcTalkedTo);
  if ("flagSet" in condition) return (save.flags ?? []).includes(condition.flagSet);
  if ("itemOwned" in condition) return (extras.items ?? []).includes(condition.itemOwned);
  if ("partyMemberPresent" in condition) return (extras.party ?? []).includes(condition.partyMemberPresent);
  if ("questDiscovered" in condition) return stageOf(condition.questDiscovered, save) >= 1 || (save.questsDiscovered ?? []).includes(condition.questDiscovered);
  if ("questActive" in condition) {
    const stage = stageOf(condition.questActive, save);
    return stage === 1 || stage === 2;
  }
  if ("questStage" in condition) return stageOf(condition.questStage[0], save) >= condition.questStage[1];
  if ("questCompleted" in condition) return stageOf(condition.questCompleted, save) >= 3;
  return false;
}

function stageOf(questId: string, save: ProgressSave): number {
  const quest = questById(questId);
  return quest ? questStage(save, quest) : 0;
}

/** The chapter the party is in: the explicitly set one, or any later chapter whose own
 * `reachedWhen` already holds. Never below 1. */
export function currentChapter(save: ProgressSave, extras: ProgressExtras = {}): number {
  let chapter = Math.max(1, save.chapter ?? 1);
  for (const def of CHAPTERS) {
    if (def.id > chapter && def.reachedWhen && evaluateNoChapter(def.reachedWhen, save, extras)) chapter = def.id;
  }
  return chapter;
}

// A chapter's own reachedWhen can't sensibly depend on chapterReached (that would recurse), so
// leaves of that kind read the explicitly stored chapter instead of the derived one.
function evaluateNoChapter(condition: Condition, save: ProgressSave, extras: ProgressExtras): boolean {
  if ("all" in condition) return condition.all.every((c) => evaluateNoChapter(c, save, extras));
  if ("any" in condition) return condition.any.some((c) => evaluateNoChapter(c, save, extras));
  if ("not" in condition) return !evaluateNoChapter(condition.not, save, extras);
  if ("chapterReached" in condition) return Math.max(1, save.chapter ?? 1) >= condition.chapterReached;
  return evaluate(condition, save, extras);
}

/** How a mission currently shows on the map. `test` (debug mode) opens everything. */
export function missionAccess(missionId: string, save: ProgressSave, extras: ProgressExtras = {}, test = false, gates: Record<string, MissionGate> = GATES): MissionAccess {
  if (save.completed.includes(missionId)) return "completed";
  const gate = gates[missionId];
  if (test || !gate) return "available";
  if (!evaluate(gate.visible, save, extras)) return "hidden";
  if (!evaluate(gate.unlock, save, extras)) return "revealed";
  return "available";
}

/** Applies effects to a save, returning the same object when nothing changed. */
export function applyEffects<T extends ProgressSave>(save: T, effects: Effect[] | undefined): T {
  if (!effects || effects.length === 0) return save;
  let next = save;
  for (const effect of effects) {
    if ("setChapter" in effect) {
      if ((next.chapter ?? 1) < effect.setChapter) next = { ...next, chapter: effect.setChapter };
    } else if ("setFlag" in effect) {
      if (!(next.flags ?? []).includes(effect.setFlag)) next = { ...next, flags: [...(next.flags ?? []), effect.setFlag] };
    } else if ("discoverQuest" in effect) {
      if (!(next.questsDiscovered ?? []).includes(effect.discoverQuest)) next = { ...next, questsDiscovered: [...(next.questsDiscovered ?? []), effect.discoverQuest] };
    }
  }
  return next;
}

/** Settles progression after any save change: fires every trigger whose condition holds and
 * every finished quest's own effects, until nothing more changes. All effects only ever add
 * (raise the chapter, set a flag, discover a quest), so this always converges. */
export function advanceProgression<T extends ProgressSave>(save: T, extras: ProgressExtras = {}, triggers: Trigger[] = TRIGGERS): T {
  let next = save;
  for (let pass = 0; pass < 8; pass++) {
    const before = next;
    for (const trigger of triggers) {
      if (evaluate(trigger.when, next, extras)) next = applyEffects(next, trigger.do);
    }
    for (const quest of QUESTS) {
      const stage = questStage(next, quest);
      if (stage >= 1) next = applyEffects(next, quest.onAccept);
      if (stage >= 3) next = applyEffects(next, quest.onComplete);
    }
    // Materialise a chapter that became reachable through its own `reachedWhen`.
    const derived = currentChapter(next, extras);
    if (derived > (next.chapter ?? 1)) next = { ...next, chapter: derived };
    if (next === before) break;
  }
  return next;
}
