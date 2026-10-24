import { campaignStorage } from "./storage";
import { savedDex } from "./dexterity";
import { weaponTypesForClass } from "./weaponSkills";
import { WEAPON_TYPES, cleanWeaponSkill } from "./weaponTypes";
import { cleanResistances } from "../ember/resistances";
import { cleanPartyFormation, cleanPartyLeader } from "./partyFormation";
import { EQUIPMENT, EXP_TO_LEVEL, expToLevel, MAX_GRID, MAX_LEVEL, POTION_CARRY_MAX, BAG_MAX, PROMOTIONS, STAT_POINTS_PER_LEVEL, WEAPONS, WORLD_LOCATIONS, emberFromCompleted, equipmentFitsSlot, starterWeaponFor, startingBags } from "../ember/data";
import { ALL_MISSIONS } from "./mapstore";
import { OVERWORLD_START_HEX, locationAt, worldToHex } from "./overworld";
import { cleanHunger, fullness } from "./hunger";
import { cleanAffinityScores } from "./affinity";
import { enmityFromSnapshot, enmityToSnapshot } from "./enmity";
import { cleanConversationMemory } from "./companionDialogues";
import { poisonTierOf } from "./poison";
import { cleanHeroSkills, cleanTravelTraining, SKILL_CAP, TRAVEL_TRAINING_HOURS } from "./skills";
import { TIER_KEYS } from "../ember/types";
import type { Bag, BattleSnapshot, BattleUnitSnap, ClassId, DialogAction, DialogLine, DialogTree, EquipSlot, Phase, PoisonTier, SaveBank, SaveData, Side, SpriteId, StatPointAllocation, StatPointAttribute, TerrainId, TierKey } from "../ember/types";

/** Fresh parties begin one hex left of Stone Bridge, on the map's west edge. */
const START_HEX = OVERWORLD_START_HEX;

export const SLOT_COUNT = 6;
export const SAVE_VERSION = 19;
const BANK_KEY = "ember-save-bank";
const SAVE_KEY = "ember-save";
const SAVE_BAK_KEY = "ember-save.bak";
const LEGACY_KEY = "brasa-save";

export const DEFAULT_LEVELS: Record<string, number> = { Kael: 1, Neera: 1, Voss: 1, Salazar: 1 };
export const DEFAULT_XP: Record<string, number> = { Kael: 0, Neera: 0, Voss: 0, Salazar: 0 };

const HEROES = ["Kael", "Neera", "Voss", "Salazar"] as const;
/** RPG map only: 5 days' worth per starting party member, so a fresh party isn't already
 * on the clock the moment it can travel. */
const STARTING_RATIONS = 5 * HEROES.length;
const STAT_POINT_ATTRIBUTES: StatPointAttribute[] = ["hp", "atk", "mag", "def", "dex"];
const MISSION_IDS = new Set(ALL_MISSIONS.map((m) => m.id));

const HERO_BASE_CLASS: Record<(typeof HEROES)[number], ClassId> = {
  Kael: "swordsman",
  Neera: "archer",
  Voss: "mage",
  Salazar: "healer",
};

/** Aldric and Malrec join later in the story (see HERO_NAMES vs ALL_HERO_NAMES in data.ts)
 * but their starter gear is seeded into a fresh save from day one anyway, same as everyone
 * else's — otherwise their cheapest weapon would sit in the Smith's for-sale list instead
 * of already being owned and equipped the moment they're actually recruited. */
const LATE_HERO_BASE_CLASS: Record<string, ClassId> = {
  Aldric: "aldric",
  Malrec: "conjurer",
};

/** Inn-quest lists (accepted/done/picked-up/kills): plain string ids, deduplicated. */
function cleanQuestList(raw: unknown): string[] {
  if (!Array.isArray(raw)) return [];
  return [...new Set(raw.filter((id): id is string => typeof id === "string" && id.length > 0 && id.length <= 80))].slice(0, 200);
}

/** SaveData.dialogsSeen — unlike the quest lists, not capped at 200: every conversation in
 * the campaign must stay recorded once played. */
function cleanDialogsSeen(raw: unknown): string[] {
  if (!Array.isArray(raw)) return [];
  return [...new Set(raw.filter((id): id is string => typeof id === "string" && id.length > 0 && id.length <= 240))];
}

/** The one key a scripted conversation is recorded under in SaveData.dialogsSeen. */
export function dialogSeenKey(kind: "intro" | "outro", missionId: string, treeId: string): string {
  return `${kind}:${missionId}:${treeId}`;
}

function clampInt(value: unknown, min: number, max: number): number {
  const n = Math.floor(Number(value));
  if (!Number.isFinite(n)) return min;
  return Math.min(max, Math.max(min, n));
}

/** A save from before exploredHexes existed has no real travel history to recover — this
 * gives it a reasonable one instead of dropping it into total fog: the starting ford, its
 * current position, and every location whose scenario is already completed (they had to
 * have walked there to play it). Real exploredHexes (an actual array) are trusted as-is. */
function cleanExploredHexes(raw: unknown, completed: string[], overworldPos: { col: number; row: number }): string[] {
  if (Array.isArray(raw)) {
    const seen = new Set<string>();
    for (const h of raw) {
      if (typeof h === "string" && /^-?\d+,-?\d+$/.test(h)) seen.add(h);
    }
    if (seen.size > 0) return [...seen];
  }
  const set = new Set<string>([`${START_HEX.x},${START_HEX.y}`, `${overworldPos.col},${overworldPos.row}`]);
  for (const loc of WORLD_LOCATIONS) {
    if (!loc.missionIds.some((id) => completed.includes(id))) continue;
    const hex = worldToHex(loc.x, loc.y);
    set.add(`${hex.x},${hex.y}`);
  }
  return [...set];
}

function cloneBags(src?: Record<string, Bag>): Record<string, Bag> {
  const base = startingBags();
  if (!src) return base;
  for (const name of Object.keys(base)) {
    const b = src[name];
    if (!b) continue;
    base[name] = {
      mid: clampInt(b.mid, 0, POTION_CARRY_MAX.mid),
      weak: clampInt(b.weak ?? (b as { high?: number }).high, 0, POTION_CARRY_MAX.weak),
      potent: clampInt(b.potent, 0, POTION_CARRY_MAX.potent),
      disease: clampInt(b.disease, 0, POTION_CARRY_MAX.disease),
      manaSmall: clampInt(b.manaSmall, 0, POTION_CARRY_MAX.manaSmall),
      manaMid: clampInt(b.manaMid, 0, POTION_CARRY_MAX.manaMid),
      manaLarge: clampInt(b.manaLarge, 0, POTION_CARRY_MAX.manaLarge),
      lockpick: clampInt(b.lockpick, 0, BAG_MAX),
    };
  }
  return base;
}

function renameHero<T>(map: Record<string, T> | undefined, from: string, to: string): void {
  if (!map) return;
  if (map[from] != null && map[to] == null) {
    map[to] = map[from];
    delete map[from];
  }
}

function cleanStringList(value: unknown, allowed?: Set<string>): string[] {
  if (!Array.isArray(value)) return [];
  const out: string[] = [];
  for (const item of value) {
    if (typeof item !== "string" || !item) continue;
    if (allowed && !allowed.has(item)) continue;
    if (!out.includes(item)) out.push(item);
  }
  return out;
}

function cleanLevels(raw: unknown): Record<string, number> {
  const levels = { ...DEFAULT_LEVELS };
  if (!raw || typeof raw !== "object") return levels;
  for (const [k, v] of Object.entries(raw as Record<string, unknown>)) {
    if (!HEROES.includes(k as (typeof HEROES)[number])) continue;
    levels[k] = clampInt(v, 1, MAX_LEVEL);
  }
  return levels;
}

function cleanXp(raw: unknown, levels: Record<string, number>): Record<string, number> {
  const xp = { ...DEFAULT_XP };
  if (!raw || typeof raw !== "object") return xp;
  for (const [k, v] of Object.entries(raw as Record<string, unknown>)) {
    if (!HEROES.includes(k as (typeof HEROES)[number])) continue;
    xp[k] = clampInt(v, 0, expToLevel(levels[k] ?? 1) - 1);
  }
  return xp;
}

/** Keeps old saves safe while immediately granting their heroes every point they had earned
 * before this system was introduced. The spend cap is derived from level, never trusted from
 * storage, so malformed saves cannot manufacture extra permanent stats. */
function cleanUnitStatPoints(raw: unknown, level: number): StatPointAllocation {
  if (!raw || typeof raw !== "object") return {};
  const source = raw as Record<string, unknown>;
  let left = Math.max(0, (level - 1) * STAT_POINTS_PER_LEVEL);
  const points: StatPointAllocation = {};
  for (const stat of STAT_POINT_ATTRIBUTES) {
    const amount = clampInt(stat === "dex" ? savedDex(source) : source[stat], 0, left);
    if (amount > 0) points[stat] = amount;
    left -= amount;
  }
  return points;
}

function cleanStatPointAllocations(raw: unknown, levels: Record<string, number>): Record<string, StatPointAllocation> {
  const out: Record<string, StatPointAllocation> = {};
  if (!raw || typeof raw !== "object") return out;
  for (const hero of HEROES) {
    const source = (raw as Record<string, unknown>)[hero];
    if (!source || typeof source !== "object") continue;
    let left = Math.max(0, (levels[hero] - 1) * STAT_POINTS_PER_LEVEL);
    const allocated: StatPointAllocation = {};
    for (const stat of STAT_POINT_ATTRIBUTES) {
      const amount = clampInt(stat === "dex" ? savedDex(source as Record<string, unknown>) : (source as Record<string, unknown>)[stat], 0, left);
      if (amount > 0) allocated[stat] = amount;
      left -= amount;
    }
    if (Object.keys(allocated).length) out[hero] = allocated;
  }
  return out;
}

function cleanPromotions(raw: unknown): Record<string, ClassId> {
  const out: Record<string, ClassId> = {};
  if (!raw || typeof raw !== "object") return out;
  for (const [k, v] of Object.entries(raw as Record<string, unknown>)) {
    if (!HEROES.includes(k as (typeof HEROES)[number])) continue;
    const base = HERO_BASE_CLASS[k as (typeof HEROES)[number]];
    const options = PROMOTIONS[base];
    if (options && typeof v === "string" && (options as string[]).includes(v)) out[k] = v as ClassId;
  }
  return out;
}

function cleanWeapons(raw: unknown): Record<string, number> {
  const out: Record<string, number> = {};
  if (!raw || typeof raw !== "object") return out;
  for (const [id, v] of Object.entries(raw as Record<string, unknown>)) {
    if (!WEAPONS[id]) continue;
    out[id] = clampInt(v, 0, 5);
  }
  return out;
}

function cleanEquipped(raw: unknown, owned: Record<string, number>): Record<string, string> {
  const out: Record<string, string> = {};
  if (!raw || typeof raw !== "object") return out;
  for (const [hero, v] of Object.entries(raw as Record<string, unknown>)) {
    if (!HEROES.includes(hero as (typeof HEROES)[number])) continue;
    if (typeof v === "string" && WEAPONS[v] && owned[v] != null) out[hero] = v;
  }
  return out;
}

function cleanEquipment(raw: unknown): Record<string, Partial<Record<EquipSlot, string>>> {
  const out: Record<string, Partial<Record<EquipSlot, string>>> = {};
  if (!raw || typeof raw !== "object") return out;
  for (const [hero, slots] of Object.entries(raw as Record<string, unknown>)) {
    if (!HEROES.includes(hero as (typeof HEROES)[number]) || !slots || typeof slots !== "object") continue;
    const cleanSlots: Partial<Record<EquipSlot, string>> = {};
    for (const [slot, itemId] of Object.entries(slots as Record<string, unknown>)) {
      const def = typeof itemId === "string" ? EQUIPMENT[itemId] : undefined;
      if (!def) continue;
      const target = (slot === "back" ? "shoulders" : slot) as EquipSlot;
      if (equipmentFitsSlot(def, target) && !cleanSlots[target]) cleanSlots[target] = itemId as string;
    }
    if (Object.keys(cleanSlots).length > 0) out[hero] = cleanSlots;
  }
  return out;
}

function cleanLooseEquipment(raw: unknown): Record<string, number> {
  const out: Record<string, number> = {};
  if (!raw || typeof raw !== "object") return out;
  for (const [id, v] of Object.entries(raw as Record<string, unknown>)) {
    if (!EQUIPMENT[id]) continue;
    const n = clampInt(v, 0, 99);
    if (n > 0) out[id] = n;
  }
  return out;
}

function cleanSpellUses(raw: unknown): Record<string, Partial<Record<TierKey, number>>> {
  const out: Record<string, Partial<Record<TierKey, number>>> = {};
  if (!raw || typeof raw !== "object") return {};
  for (const [hero, tiers] of Object.entries(raw as Record<string, unknown>)) {
    if (!HEROES.includes(hero as (typeof HEROES)[number]) || !tiers || typeof tiers !== "object") continue;
    const cleanTiers: Partial<Record<TierKey, number>> = {};
    for (const [key, v] of Object.entries(tiers as Record<string, unknown>)) {
      if (!TIER_KEYS.includes(key as TierKey)) continue;
      const n = clampInt(v, 0, 99);
      if (n > 0) cleanTiers[key as TierKey] = n;
    }
    if (Object.keys(cleanTiers).length > 0) out[hero] = cleanTiers;
  }
  return out;
}

function cleanBag(raw: unknown): Bag {
  const b = (raw && typeof raw === "object" ? raw : {}) as Partial<Bag>;
  return {
    mid: clampInt(b.mid, 0, POTION_CARRY_MAX.mid),
    weak: clampInt(b.weak, 0, POTION_CARRY_MAX.weak),
    potent: clampInt(b.potent, 0, POTION_CARRY_MAX.potent),
    disease: clampInt(b.disease, 0, POTION_CARRY_MAX.disease),
    manaSmall: clampInt(b.manaSmall, 0, POTION_CARRY_MAX.manaSmall),
    manaMid: clampInt(b.manaMid, 0, POTION_CARRY_MAX.manaMid),
    manaLarge: clampInt(b.manaLarge, 0, POTION_CARRY_MAX.manaLarge),
    lockpick: clampInt(b.lockpick, 0, BAG_MAX),
  };
}

/** A saved unit's dialog tree, or the pending one on a saved battle — validated loosely
 * (unlike the stat fields above): a dangling next/startId reference just closes the popup
 * early (see DialogOverlay's missing-line fallback) rather than anything that can corrupt
 * combat state, so this only guards against the shape being outright wrong. */
function cleanDialogTree(raw: unknown): DialogTree | null {
  if (!raw || typeof raw !== "object") return null;
  const t = raw as Record<string, unknown>;
  if (typeof t.id !== "string" || typeof t.startId !== "string" || !Array.isArray(t.lines)) return null;
  const lines: DialogLine[] = [];
  for (const item of t.lines) {
    if (!item || typeof item !== "object") continue;
    const l = item as Record<string, unknown>;
    if (typeof l.id !== "string" || typeof l.speaker !== "string" || typeof l.text !== "string") continue;
    const replies = Array.isArray(l.replies)
      ? (l.replies as unknown[]).flatMap((r) => {
          if (!r || typeof r !== "object") return [];
          const rr = r as Record<string, unknown>;
          if (typeof rr.text !== "string") return [];
          const action = rr.action === "tavern" || rr.action === "smith" || rr.action === "healer" || rr.action === "merchant" || rr.action === "merchantGear" || rr.action === "recruitAldric" || rr.action === "acceptSuspectHostageQuest" ? (rr.action as DialogAction) : undefined;
          const af = rr.affinity as { from?: unknown; to?: unknown; delta?: unknown } | undefined;
          const affinity = af && typeof af.from === "string" && typeof af.to === "string" && (af.delta === -3 || af.delta === 0 || af.delta === 3)
            ? { from: af.from, to: af.to, delta: af.delta as -3 | 0 | 3 } : undefined;
          return [{ text: rr.text, next: typeof rr.next === "string" ? rr.next : null, action, affinity }];
        })
      : undefined;
    lines.push({
      id: l.id,
      speaker: l.speaker,
      text: l.text,
      portrait: typeof l.portrait === "string" ? (l.portrait as SpriteId) : undefined,
      next: typeof l.next === "string" ? l.next : null,
      replies: replies && replies.length > 0 ? replies : undefined,
    });
  }
  if (lines.length === 0) return null;
  return { id: t.id, startId: t.startId, lines };
}

function cleanBattleUnit(raw: unknown, roundLegacyWeaponFractions = false): BattleUnitSnap | null {
  if (!raw || typeof raw !== "object") return null;
  const u = raw as Record<string, unknown>;
  if (typeof u.id !== "string" || typeof u.name !== "string" || typeof u.classId !== "string") return null;
  const side = u.side === "player" || u.side === "enemy" || u.side === "neutral" ? (u.side as Side) : null;
  if (!side) return null;
  const facing: 1 | -1 = u.facing === -1 ? -1 : 1;
  const shock =
    u.shock && typeof u.shock === "object"
      ? {
          dice: clampInt((u.shock as { dice?: unknown }).dice, 0, 20),
          faces: clampInt((u.shock as { faces?: unknown }).faces, 1, 20),
          bonus: clampInt((u.shock as { bonus?: unknown }).bonus, 0, 40),
          mag: clampInt((u.shock as { mag?: unknown }).mag, 0, 999),
        }
      : null;
  const diseaseBase =
    u.diseaseBase && typeof u.diseaseBase === "object"
      ? {
          atk: clampInt((u.diseaseBase as { atk?: unknown }).atk, 0, 99),
          mag: clampInt((u.diseaseBase as { mag?: unknown }).mag, 0, 99),
          def: clampInt((u.diseaseBase as { def?: unknown }).def, 0, 99),
          dex: clampInt(savedDex(u.diseaseBase as Record<string, unknown>), 0, 999),
          mov: clampInt((u.diseaseBase as { mov?: unknown }).mov, 0, 20),
        }
      : null;
  const gear: Partial<Record<EquipSlot, string>> = {};
  if (u.gear && typeof u.gear === "object") {
    for (const [slot, itemId] of Object.entries(u.gear as Record<string, unknown>)) {
      if (typeof itemId === "string" && EQUIPMENT[itemId]?.slot === slot) gear[slot as EquipSlot] = itemId;
    }
  }
  const spellsRaw = (u.spells && typeof u.spells === "object" ? u.spells : {}) as Record<string, unknown>;
  const spells = {
    tier1: clampInt(spellsRaw.tier1, 0, 99),
    tier2: clampInt(spellsRaw.tier2, 0, 99),
    tier3: clampInt(spellsRaw.tier3, 0, 99),
    tier4: clampInt(spellsRaw.tier4, 0, 99),
    tier5: clampInt(spellsRaw.tier5, 0, 99),
    tier6: clampInt(spellsRaw.tier6, 0, 99),
    tier7: clampInt(spellsRaw.tier7, 0, 99),
    tier8: clampInt(spellsRaw.tier8, 0, 99),
    tier9: clampInt(spellsRaw.tier9, 0, 99),
    tier10: clampInt(spellsRaw.tier10, 0, 99),
  };
  return {
    id: u.id,
    name: u.name,
    classId: u.classId as ClassId,
    side,
    // Bounded by the board ceiling, not a literal: a 64 here silently walked units
    // and props back onto column 64 the moment boards could be wider than that.
    x: clampInt(u.x, 0, MAX_GRID - 1),
    y: clampInt(u.y, 0, MAX_GRID - 1),
    hp: clampInt(u.hp, 0, 999),
    escaped: u.escaped === true,
    maxHp: clampInt(u.maxHp, 1, 999),
    atk: clampInt(u.atk, 0, 99),
    mag: clampInt(u.mag, 0, 99),
    def: clampInt(u.def, 0, 99),
    dex: clampInt(savedDex(u), 0, 999),
    statPointAllocation: cleanUnitStatPoints(u.statPointAllocation, clampInt(u.level, 1, MAX_LEVEL)),
    resistances: cleanResistances(u.resistances),
    weaponSkills: Object.fromEntries(WEAPON_TYPES.filter(type => weaponTypesForClass(u.classId as ClassId).includes(type)).map(type => [type, cleanWeaponSkill((u.weaponSkills as Record<string, unknown> | undefined)?.[type], roundLegacyWeaponFractions)])),
    mov: clampInt(u.mov, 0, 20),
    minRange: clampInt(u.minRange, 0, 20),
    maxRange: clampInt(u.maxRange, 0, 20),
    moved: u.moved === true,
    acted: u.acted === true,
    facing,
    faceDx: typeof u.faceDx === "number" && Number.isFinite(u.faceDx) ? u.faceDx : undefined,
    faceDy: typeof u.faceDy === "number" && Number.isFinite(u.faceDy) ? u.faceDy : undefined,
    alive: u.alive !== false,
    fade: Math.min(1, Math.max(0, Number(u.fade) || 1)),
    level: clampInt(u.level, 1, MAX_LEVEL),
    xp: clampInt(u.xp, 0, expToLevel(clampInt(u.level, 1, MAX_LEVEL)) - 1),
    bag: cleanBag(u.bag),
    spells,
    weaponId: typeof u.weaponId === "string" && WEAPONS[u.weaponId] ? u.weaponId : null,
    weaponEnh: clampInt(u.weaponEnh, 0, 5),
    shock,
    shockCharges: clampInt(u.shockCharges, 0, 9),
    diseased: u.diseased === true,
    diseaseBase,
    poisoned: u.poisoned === true,
    poisonTier: poisonTierOf(u.poisonTier) ?? poisonTierOf(u.poisonFaces) ?? "lesser",
    poisonMag: clampInt(u.poisonMag, 0, 999),
    poisonResist: typeof u.poisonResist === "number" && Number.isFinite(u.poisonResist) ? Math.max(0, Math.min(SKILL_CAP, Math.round(u.poisonResist * 100) / 100)) : 0,
    stunned: u.stunned === true,
    stunTurns: clampInt(u.stunTurns, 0, 9),
    crippled: u.crippled === true,
    fullness: fullness(u.fullness),
    hungerPenaltyPct: typeof u.hungerPenaltyPct === "number" ? Math.max(0, Math.min(0.9, u.hungerPenaltyPct)) : 0,
    offHandId: typeof u.offHandId === "string" && EQUIPMENT[u.offHandId] ? u.offHandId : null,
    gear,
    summoned: u.summoned === true,
    asleep: u.asleep === true,
    sleepTurns: clampInt(u.sleepTurns, 0, 20),
    guaranteedDrop: u.guaranteedDrop === true,
    dialog: cleanDialogTree(u.dialog),
    moveBudgetUsed: clampInt(u.moveBudgetUsed, 0, 20),
  };
}

function cleanBattle(raw: unknown, pendingMission: string | null, roundLegacyWeaponFractions = false): BattleSnapshot | null {
  if (!raw || typeof raw !== "object") return null;
  const b = raw as Record<string, unknown>;
  const missionId = typeof b.missionId === "string" && MISSION_IDS.has(b.missionId) ? b.missionId : pendingMission;
  if (!missionId || !MISSION_IDS.has(missionId)) return null;
  if (!Array.isArray(b.units) || !Array.isArray(b.tiles)) return null;
  const units: BattleUnitSnap[] = [];
  for (const item of b.units) {
    const u = cleanBattleUnit(item, roundLegacyWeaponFractions);
    if (u) units.push(u);
  }
  if (units.length === 0) return null;
  const tiles = (b.tiles as unknown[]).filter((t): t is TerrainId => typeof t === "string") as TerrainId[];
  const decorations = Array.isArray(b.decorations)
    ? (b.decorations as unknown[])
        .filter((d): d is Record<string, unknown> => !!d && typeof d === "object" && typeof (d as { id?: unknown }).id === "string")
        .map((d) => ({
          id: d.id as string,
          x: clampInt(d.x, 0, MAX_GRID - 1),
          y: clampInt(d.y, 0, MAX_GRID - 1),
          rot: typeof d.rot === "number" ? clampInt(d.rot, 0, 5) : undefined,
          mirrorX: d.mirrorX === true ? true : undefined,
          blocksPath: d.blocksPath === true ? true : undefined,
          yieldsHighGround: d.yieldsHighGround === true ? true : undefined,
          targetMapId: typeof d.targetMapId === "string" ? d.targetMapId : undefined,
          returnConnector: d.returnConnector === true ? (true as const) : undefined,
        }))
    : [];
  const turnOrder = Array.isArray(b.turnOrder) ? (b.turnOrder as unknown[]).filter((id): id is string => typeof id === "string") : units.map((u) => u.id);
  const webZones = Array.isArray(b.webZones)
    ? (b.webZones as unknown[]).flatMap((z) => {
        if (!z || typeof z !== "object") return [];
        const cells = Array.isArray((z as { cells?: unknown }).cells)
          ? ((z as { cells: unknown[] }).cells.filter((c): c is string => typeof c === "string"))
          : [];
        return [{ cells, roundsLeft: clampInt((z as { roundsLeft?: unknown }).roundsLeft, 0, 20) }];
      })
    : [];
  const auraZones = Array.isArray(b.auraZones)
    ? (b.auraZones as unknown[]).flatMap((z) => {
        if (!z || typeof z !== "object") return [];
        const kindRaw = (z as { kind?: unknown }).kind;
        const sideRaw = (z as { side?: unknown }).side;
        const kind: "protection" | "intimidation" | null = kindRaw === "protection" || kindRaw === "intimidation" ? kindRaw : null;
        const side: Side | null = sideRaw === "player" || sideRaw === "enemy" || sideRaw === "neutral" ? sideRaw : null;
        if (!kind || !side) return [];
        const cells = Array.isArray((z as { cells?: unknown }).cells)
          ? ((z as { cells: unknown[] }).cells.filter((c): c is string => typeof c === "string"))
          : [];
        return [{ cells, roundsLeft: clampInt((z as { roundsLeft?: unknown }).roundsLeft, 0, 20), kind, side, pct: Math.min(1, Math.max(0, Number((z as { pct?: unknown }).pct) || 0)) }];
      })
    : [];
  const chestLootRaw = b.chestLoot && typeof b.chestLoot === "object" ? (b.chestLoot as Record<string, unknown>) : null;
  const chestLoot = chestLootRaw && typeof chestLootRaw.unitName === "string"
    ? {
        unitName: chestLootRaw.unitName,
        ember: clampInt(chestLootRaw.ember, 0, 999),
        items: Array.isArray(chestLootRaw.items)
          ? (chestLootRaw.items as unknown[]).flatMap((item) => {
              if (!item || typeof item !== "object" || typeof (item as { name?: unknown }).name !== "string") return [];
              const it = item as { name: string; icon?: unknown; tip?: unknown };
              return [{ name: it.name, icon: typeof it.icon === "string" ? it.icon : "", tip: typeof it.tip === "string" ? it.tip : undefined }];
            })
          : [],
      }
    : null;
  const phase: Phase = b.phase === "enemy" ? "enemy" : "player";
  return {
    missionId,
    mapKey: typeof b.mapKey === "string" ? b.mapKey : undefined,
    introDialogDone: typeof b.introDialogDone === "boolean" ? b.introDialogDone : undefined,
    enmity: b.enmity && typeof b.enmity === "object" ? enmityToSnapshot(enmityFromSnapshot(b.enmity)) : undefined,
    affinityScores: b.affinityScores == null ? undefined : cleanAffinityScores(b.affinityScores),
    heroSkills: b.heroSkills == null ? undefined : cleanHeroSkills(b.heroSkills, [...HEROES, ...Object.keys(LATE_HERO_BASE_CLASS)], (hero, type) => weaponTypesForClass(units.find(unit => unit.name === hero)?.classId ?? ({ ...HERO_BASE_CLASS, ...LATE_HERO_BASE_CLASS } as Record<string, ClassId>)[hero]).includes(type), roundLegacyWeaponFractions),
    turn: clampInt(b.turn, 1, 999),
    phase,
    units,
    tiles,
    decorations,
    turnOrder,
    activeUnitId: typeof b.activeUnitId === "string" ? b.activeUnitId : null,
    selectedId: typeof b.selectedId === "string" ? b.selectedId : null,
    lootEmber: clampInt(b.lootEmber, 0, 9999),
    lootRations: clampInt(b.lootRations, 0, 9999),
    questFound: cleanQuestList(b.questFound),
    lootWeapons: Array.isArray(b.lootWeapons) ? (b.lootWeapons as unknown[]).filter((id): id is string => typeof id === "string" && !!WEAPONS[id]) : [],
    lootEquipment: Array.isArray(b.lootEquipment) ? (b.lootEquipment as unknown[]).filter((id): id is string => typeof id === "string" && !!EQUIPMENT[id]) : [],
    ownedWeapons: Array.isArray(b.ownedWeapons) ? (b.ownedWeapons as unknown[]).filter((id): id is string => typeof id === "string") : [],
    webZones,
    auraZones,
    log: Array.isArray(b.log) ? (b.log as unknown[]).filter((line): line is string => typeof line === "string").slice(-200) : [],
    winAvailable: b.winAvailable === true,
    chestLoot,
    pendingDialog: cleanDialogTree(b.pendingDialog),
    turnRestrained: b.turnRestrained === true,
    turnBegan: b.turnBegan !== false,
    // Carried through as an opaque string: the engine owns the packing and is the only
    // thing that can judge the length against a board, so validating it here would
    // just be a second, weaker copy of that check.
    explored: typeof b.explored === "string" ? b.explored : undefined,
    awake: Array.isArray(b.awake) ? (b.awake as unknown[]).filter((id): id is string => typeof id === "string") : undefined,
  };
}

function cleanHp(raw: unknown): Record<string, number> {
  if (!raw || typeof raw !== "object") return {};
  const out: Record<string, number> = {};
  for (const [k, v] of Object.entries(raw as Record<string, unknown>)) {
    if (!HEROES.includes(k as (typeof HEROES)[number])) continue;
    const n = Math.floor(Number(v));
    if (!Number.isFinite(n) || n < 0) continue;
    out[k] = n;
  }
  return out;
}

function cleanHeroPoisons(raw: unknown): Record<string, PoisonTier> {
  if (!raw || typeof raw !== "object") return {};
  const out: Record<string, PoisonTier> = {};
  for (const hero of [...HEROES, ...Object.keys(LATE_HERO_BASE_CLASS)]) {
    const tier = poisonTierOf((raw as Record<string, unknown>)[hero]);
    if (tier) out[hero] = tier;
  }
  return out;
}

function cleanHeroPoisonMag(raw: unknown): Record<string, number> {
  if (!raw || typeof raw !== "object") return {};
  const out: Record<string, number> = {};
  for (const hero of [...HEROES, ...Object.keys(LATE_HERO_BASE_CLASS)]) {
    const value = (raw as Record<string, unknown>)[hero];
    if (typeof value === "number" && Number.isFinite(value)) out[hero] = clampInt(value, 0, 999);
  }
  return out;
}

function cleanHeroDiseases(raw: unknown): Record<string, boolean> {
  if (!raw || typeof raw !== "object") return {};
  const out: Record<string, boolean> = {};
  for (const hero of [...HEROES, ...Object.keys(LATE_HERO_BASE_CLASS)]) {
    if ((raw as Record<string, unknown>)[hero] === true) out[hero] = true;
  }
  return out;
}

/** Every hero starts equipped with their class's cheapest weapon — free, already owned. */
function starterEquipment(): { weapons: Record<string, number>; equipped: Record<string, string>; equipment: Record<string, Partial<Record<EquipSlot, string>>> } {
  const weapons: Record<string, number> = {};
  const equipped: Record<string, string> = {};
  const equipment: Record<string, Partial<Record<EquipSlot, string>>> = {};
  for (const hero of HEROES) {
    const id = hero === "Neera" ? "arco-composto" : hero === "Salazar" ? "cajado-da-galhada" : starterWeaponFor(HERO_BASE_CLASS[hero]);
    if (!id) continue;
    weapons[id] = 0;
    equipped[hero] = id;
  }
  equipment.Neera = { offHand: "punhal-curvo" };
  for (const [hero, classId] of Object.entries(LATE_HERO_BASE_CLASS)) {
    const id = starterWeaponFor(classId);
    if (!id) continue;
    weapons[id] = 0;
    equipped[hero] = id;
  }
  return { weapons, equipped, equipment };
}

export function emptySave(muted = false): SaveData {
  return {
    version: SAVE_VERSION,
    completed: [],
    crossingDefeatedSpawns: {},
    unitHp: {},
    levels: { ...DEFAULT_LEVELS },
    xp: { ...DEFAULT_XP },
    statPointAllocations: {},
    bags: startingBags(),
    promotions: {},
    ...starterEquipment(),
    looseEquipment: {},
    spellUses: {},
    ember: 0,
    emberSeeded: true,
    muted,
    updatedAt: Date.now(),
    pendingMission: null,
    battle: null,
    seenSmithIntro: false,
    seenOverworldIntro: false,
    seenWispForestIntro: false,
    seenInnArrivalIntro: false,
    overworldPos: { col: START_HEX.x, row: START_HEX.y },
    gameClock: 0,
    affinityScores: {},
    overworldMoveBudgetUsed: 0,
    heroHunger: {},
    heroDiseases: {},
    heroPoisons: {},
    rations: STARTING_RATIONS,
    hungerStreak: 0,
    alertStreak: 0,
    lastRoadEncounterId: null,
    roadEncountersSeen: [],
    exploredHexes: [`${START_HEX.x},${START_HEX.y}`],
  };
}

export function emptyBank(): SaveBank {
  return {
    version: SAVE_VERSION,
    lastSlot: 0,
    muted: false,
    slots: Array.from({ length: SLOT_COUNT }, () => null),
  };
}

export function migrateRecord(raw: Record<string, unknown>, muted: boolean): SaveData {
  renameHero(raw.levels as Record<string, number> | undefined, "Nira", "Neera");
  renameHero(raw.unitHp as Record<string, number> | undefined, "Nira", "Neera");
  renameHero(raw.bags as Record<string, Bag> | undefined, "Nira", "Neera");
  renameHero(raw.xp as Record<string, number> | undefined, "Nira", "Neera");
  renameHero(raw.statPointAllocations as Record<string, StatPointAllocation> | undefined, "Nira", "Neera");
  renameHero(raw.levels as Record<string, number> | undefined, "Silas", "Salazar");
  renameHero(raw.unitHp as Record<string, number> | undefined, "Silas", "Salazar");
  renameHero(raw.bags as Record<string, Bag> | undefined, "Silas", "Salazar");
  renameHero(raw.xp as Record<string, number> | undefined, "Silas", "Salazar");
  renameHero(raw.statPointAllocations as Record<string, StatPointAllocation> | undefined, "Silas", "Salazar");

  const version = clampInt(raw.version, 0, SAVE_VERSION);
  const levels = cleanLevels(raw.levels);
  if (!raw.levels || typeof raw.levels !== "object") {
    const n = 1 + cleanStringList(raw.completed).length;
    for (const k of Object.keys(levels)) levels[k] = Math.min(MAX_LEVEL, n);
  }

  let pending: string | null = null;
  if (typeof raw.pendingMission === "string" && MISSION_IDS.has(raw.pendingMission)) pending = raw.pendingMission;
  else if (raw.battle && typeof raw.battle === "object") {
    const id = (raw.battle as { missionId?: string }).missionId;
    if (typeof id === "string" && MISSION_IDS.has(id)) pending = id;
  }

  const completed = cleanStringList(raw.completed, MISSION_IDS);
  const crossingDefeatedSpawns: Record<string, string[]> = {};
  if (raw.crossingDefeatedSpawns && typeof raw.crossingDefeatedSpawns === "object") {
    for (const [missionId, ids] of Object.entries(raw.crossingDefeatedSpawns as Record<string, unknown>)) {
      if (!MISSION_IDS.has(missionId) || !Array.isArray(ids)) continue;
      const cleanIds = ids.filter((id): id is string => typeof id === "string" && id.length > 0);
      if (cleanIds.length) crossingDefeatedSpawns[missionId] = [...new Set(cleanIds)];
    }
  }
  // v18 (applied to every save, not only older ones — a save written by the running game off-hand-only equipment right after the change would carry the removed id under the new version number):
  // daggers/katars became off-hand-only equipment (same ids, see OFFHAND_DAGGERS in data.ts)
  // and "adaga-secundaria" was removed. Owned daggers move from the weapon list to the loose
  // equipment stash, and the removed item becomes the weakest dagger, so nothing is lost.
  let rawWeapons = raw.weapons;
  let rawLoose = raw.looseEquipment;
  let rawEquipment = raw.equipment;
  {
    const daggerIds = ["punhal-curvo", "katar", "adaga-sombria", "adaga-de-veneno", "adaga-viperina", "misericordia-sombria", "punhal-do-salteador", "katar-sepulcral"];
    const loose: Record<string, unknown> = rawLoose && typeof rawLoose === "object" ? { ...(rawLoose as Record<string, unknown>) } : {};
    const addLoose = (id: string) => { loose[id] = (typeof loose[id] === "number" ? (loose[id] as number) : 0) + 1; };
    if (rawWeapons && typeof rawWeapons === "object") {
      const kept: Record<string, unknown> = { ...(rawWeapons as Record<string, unknown>) };
      for (const id of daggerIds) if (id in kept) { delete kept[id]; addLoose(id); }
      rawWeapons = kept;
    }
    if (typeof loose["adaga-secundaria"] === "number") {
      const n = loose["adaga-secundaria"] as number;
      delete loose["adaga-secundaria"];
      for (let i = 0; i < n; i++) addLoose("punhal-curvo");
    }
    rawLoose = loose;
    if (rawEquipment && typeof rawEquipment === "object") {
      const swapped: Record<string, unknown> = {};
      for (const [hero, slots] of Object.entries(rawEquipment as Record<string, unknown>)) {
        swapped[hero] = slots && typeof slots === "object" && (slots as Record<string, unknown>).offHand === "adaga-secundaria"
          ? { ...(slots as Record<string, unknown>), offHand: "punhal-curvo" }
          : slots;
      }
      rawEquipment = swapped;
    }
  }
  const weapons = cleanWeapons(rawWeapons);
  const equipped = cleanEquipped(raw.equipped, weapons);
  const equipment = cleanEquipment(rawEquipment);
  // v17 removes an accidentally seeded Besta Leve from untouched new-game saves, including
  // saves that were already migrated by v15 before the cleanup covered the current version.
  // It is found or bought during play, never granted as starting equipment.
  if (version < 17 && completed.length === 0 && weapons["besta-leve"] != null) {
    delete weapons["besta-leve"];
    for (const [hero, weaponId] of Object.entries(equipped)) {
      if (weaponId === "besta-leve") delete equipped[hero];
    }
  }
  // v14 corrects Neera's intended starting kit for existing saves too: Composite Bow in
  // the main hand and a light dagger in the secondary hand for adjacent counters.
  if (version < 14) {
    weapons["arco-composto"] = weapons["arco-composto"] ?? 0;
    equipped.Neera = "arco-composto";
    equipment.Neera = { ...equipment.Neera, offHand: equipment.Neera?.offHand ?? "punhal-curvo" };
  }
  // Backfill: any hero with nothing equipped yet (old save, predates weapons) gets their
  // class's free starter weapon, same as a brand new save already does.
  for (const hero of HEROES) {
    if (equipped[hero]) continue;
    const id = hero === "Salazar" ? "cajado-da-galhada" : starterWeaponFor(HERO_BASE_CLASS[hero]);
    if (!id) continue;
    weapons[id] = weapons[id] ?? 0;
    equipped[hero] = id;
  }
  // Aldric and Malrec were added after the original roster. Backfill their starter
  // weapons too, so existing saves place the weapon in the character's equipped slot
  // when they join instead of leaving it as an unassigned Mochila item.
  for (const [hero, classId] of Object.entries(LATE_HERO_BASE_CLASS)) {
    if (equipped[hero]) continue;
    const id = starterWeaponFor(classId);
    if (!id) continue;
    weapons[id] = weapons[id] ?? 0;
    equipped[hero] = id;
  }
  let ember = clampInt(raw.ember, 0, 9999);
  let emberSeeded = raw.emberSeeded === true;
  if (!emberSeeded) {
    ember += emberFromCompleted(completed);
    emberSeeded = true;
  }
  const overworldPos = cleanOverworldPos(raw.overworldPos);

  return {
    version: SAVE_VERSION,
    completed,
    crossingDefeatedSpawns,
    questsActive: cleanQuestList(raw.questsActive),
    questsDone: cleanQuestList(raw.questsDone),
    questsDiscovered: cleanQuestList(raw.questsDiscovered),
    chapter: clampInt(raw.chapter ?? 1, 1, 99),
    flags: cleanQuestList(raw.flags),
    dialogsSeen: cleanDialogsSeen(raw.dialogsSeen),
    npcTalked: cleanQuestList(raw.npcTalked),
    questItems: cleanQuestList(raw.questItems),
    questKills: cleanQuestList(raw.questKills),
    unitHp: cleanHp(raw.unitHp),
    levels,
    xp: cleanXp(raw.xp, levels),
    statPointAllocations: cleanStatPointAllocations(raw.statPointAllocations, levels),
    bags: version < 4 ? startingBags() : cloneBags(raw.bags as Record<string, Bag>),
    promotions: cleanPromotions(raw.promotions),
    weapons,
    equipped,
    equipment,
    looseEquipment: cleanLooseEquipment(rawLoose),
    spellUses: cleanSpellUses(raw.spellUses),
    ember,
    emberSeeded,
    muted: raw.muted === true || muted,
    updatedAt: typeof raw.updatedAt === "number" && raw.updatedAt > 0 ? raw.updatedAt : Date.now(),
    pendingMission: pending,
    battle: cleanBattle(raw.battle, pending, version < 19),
    seenSmithIntro: raw.seenSmithIntro === true,
    seenOverworldIntro: raw.seenOverworldIntro === true,
    seenWispForestIntro: raw.seenWispForestIntro === true,
    seenInnArrivalIntro: raw.seenInnArrivalIntro === true,
    mapMode: raw.mapMode === "classic" || raw.mapMode === "rpg" ? raw.mapMode : undefined,
    overworldPos,
    gameClock: clampInt(raw.gameClock, 0, 999999),
    gameHour: clampInt(raw.gameHour ?? 8, 0, 23),
    affinityScores: cleanAffinityScores(raw.affinityScores),
    companionConversations: cleanConversationMemory(raw.companionConversations),
    partyFormation: cleanPartyFormation(raw.partyFormation),
    partyLeader: cleanPartyLeader(raw.partyLeader),
    overworldMoveBudgetUsed: clampInt(raw.overworldMoveBudgetUsed ?? raw.gameClock, 0, 999999),
    heroHunger: cleanHunger(raw.heroHunger),
    heroDiseases: cleanHeroDiseases(raw.heroDiseases),
    heroPoisons: cleanHeroPoisons(raw.heroPoisons),
    heroPoisonMag: cleanHeroPoisonMag(raw.heroPoisonMag),
    heroSkills: cleanHeroSkills(raw.heroSkills, [...HEROES, ...Object.keys(LATE_HERO_BASE_CLASS)], (hero, type) => weaponTypesForClass(cleanPromotions(raw.promotions)[hero] ?? ({ ...HERO_BASE_CLASS, ...LATE_HERO_BASE_CLASS } as Record<string, ClassId>)[hero]).includes(type), version < 19),
    travelTraining: cleanTravelTraining(raw.travelTraining, [...HEROES, ...Object.keys(LATE_HERO_BASE_CLASS)]),
    travelTrainingHours: Object.fromEntries(Object.entries(raw.travelTrainingHours && typeof raw.travelTrainingHours === "object" ? raw.travelTrainingHours : {})
      .filter(([, h]) => typeof h === "number" && Number.isFinite(h) && h > 0).map(([hero, h]) => [hero, Math.min(TRAVEL_TRAINING_HOURS * 10, h as number)])),
    rations: typeof raw.rations === "number" ? clampInt(raw.rations, 0, 999999) : STARTING_RATIONS,
    hungerStreak: clampInt(raw.hungerStreak, 0, 999999),
    hungerHours: typeof raw.hungerHours === "number" && Number.isFinite(raw.hungerHours) ? Math.max(0, raw.hungerHours) : undefined,
    alertStreak: clampInt(raw.alertStreak, 0, 999999),
    lastRoadEncounterId: typeof raw.lastRoadEncounterId === "string" ? raw.lastRoadEncounterId : null,
    roadEncountersSeen: cleanStringList(raw.roadEncountersSeen, MISSION_IDS),
    exploredHexes: cleanExploredHexes(raw.exploredHexes, completed, overworldPos),
  };
}

function cleanOverworldPos(raw: unknown): { col: number; row: number } {
  if (raw && typeof raw === "object" && typeof (raw as { col?: unknown }).col === "number" && typeof (raw as { row?: unknown }).row === "number") {
    return { col: Math.floor((raw as { col: number }).col), row: Math.floor((raw as { row: number }).row) };
  }
  return { col: START_HEX.x, row: START_HEX.y };
}

export function parseRecord(text: string | null): SaveData | null {
  if (!text) return null;
  try {
    const parsed = JSON.parse(text) as unknown;
    if (!parsed || typeof parsed !== "object") return null;
    return migrateRecord(parsed as Record<string, unknown>, false);
  } catch {
    return null;
  }
}

function readKey(key: string): string | null {
  try {
    return campaignStorage!.getItem(key);
  } catch {
    return null;
  }
}

function writeKey(key: string, value: string): boolean {
  try {
    campaignStorage!.setItem(key, value);
    return true;
  } catch {
    return false;
  }
}

function slotOccupied(s: SaveData | null): boolean {
  if (!s) return false;
  return s.completed.length > 0 || Object.keys(s.unitHp).length > 0 || !!s.pendingMission || !!s.battle || Object.keys(s.companionConversations ?? {}).length > 0 || s.mapMode === "classic" || s.mapMode === "rpg";
}

function migrateLegacyIntoBank(): SaveBank {
  const bank = emptyBank();
  const legacy = parseRecord(readKey(BANK_KEY) ? null : readKey(SAVE_KEY)) ?? parseRecord(readKey(SAVE_BAK_KEY)) ?? parseRecord(readKey(LEGACY_KEY));
  if (legacy && slotOccupied(legacy)) {
    bank.slots[0] = legacy;
    bank.lastSlot = 0;
    bank.muted = legacy.muted;
  }
  return bank;
}

export function parseBank(text: string | null): SaveBank | null {
  if (!text) return null;
  try {
    const parsed = JSON.parse(text) as unknown;
    if (!parsed || typeof parsed !== "object") return null;
    const raw = parsed as Record<string, unknown>;
    const rawSlots = Array.isArray(raw.slots) ? raw.slots : [];
    const muted = raw.muted === true;
    const slots: Array<SaveData | null> = Array.from({ length: SLOT_COUNT }, (_, i) => {
      const item = rawSlots[i];
      if (!item || typeof item !== "object") return null;
      const rec = migrateRecord(item as Record<string, unknown>, muted);
      return slotOccupied(rec) ? rec : null;
    });
    const lastSlot = clampInt(raw.lastSlot, 0, SLOT_COUNT - 1);
    return { version: SAVE_VERSION, lastSlot: slots[lastSlot] ? lastSlot : slots.findIndex(Boolean) === -1 ? 0 : Math.max(0, slots.findIndex(Boolean)), muted, slots };
  } catch {
    return null;
  }
}

export function loadBank(): SaveBank {
  const rawBank = readKey(BANK_KEY);
  const bank = parseBank(rawBank);
  if (bank) return bank;
  const migrated = migrateLegacyIntoBank();
  persistBank(migrated);
  return migrated;
}

/**
 * Size and outcome of the last bank write.
 *
 * A board carries `tiles`, `tileVariants` and `tileRots` in full inside every
 * battle snapshot, so a bank holding a few large-board slots is the first thing
 * that can push localStorage past its quota — roughly 200 KB of tiles alone at
 * the 160-square ceiling (see `MAX_GRID` in ./data), against a budget that is
 * usually about 5 MB. `setItem` throwing there was previously swallowed, which
 * reaches the player as "the game stopped saving" with nothing to go on.
 */
let lastWrite: { bytes: number; ok: boolean } = { bytes: 0, ok: true };

/** Whether the last bank write actually landed, and how big it was. */
export function lastSaveWrite(): { bytes: number; ok: boolean } {
  return lastWrite;
}

function persistBank(bank: SaveBank): boolean {
  const payload = JSON.stringify({ ...bank, version: SAVE_VERSION });
  const ok = writeKey(BANK_KEY, payload);
  lastWrite = { bytes: payload.length, ok };
  if (!ok) {
    console.error(
      `[save] localStorage refused ${(payload.length / 1024).toFixed(0)} KB — progress was NOT saved.`,
    );
  }
  return ok;
}

export function writeBank(bank: SaveBank): SaveBank {
  const next: SaveBank = {
    version: SAVE_VERSION,
    lastSlot: clampInt(bank.lastSlot, 0, SLOT_COUNT - 1),
    muted: bank.muted === true,
    slots: Array.from({ length: SLOT_COUNT }, (_, i) => bank.slots[i] ?? null),
  };
  persistBank(next);
  return next;
}

export function activeSave(bank: SaveBank): SaveData {
  return bank.slots[bank.lastSlot] ?? emptySave(bank.muted);
}

export function writeSlot(bank: SaveBank, index: number, data: SaveData): SaveBank {
  const i = clampInt(index, 0, SLOT_COUNT - 1);
  const slots = [...bank.slots];
  slots[i] = { ...data, version: SAVE_VERSION, muted: bank.muted, updatedAt: Date.now() };
  return writeBank({ ...bank, lastSlot: i, slots });
}

export function selectSlot(bank: SaveBank, index: number): SaveBank {
  const i = clampInt(index, 0, SLOT_COUNT - 1);
  return writeBank({ ...bank, lastSlot: i });
}

export function setMutedBank(bank: SaveBank, muted: boolean): SaveBank {
  return writeBank({ ...bank, muted });
}

export function formatStamp(ts: number): string {
  try {
    return new Intl.DateTimeFormat("pt-BR", {
      timeZone: "America/Sao_Paulo",
      day: "2-digit",
      month: "2-digit",
      year: "numeric",
      hour: "2-digit",
      minute: "2-digit",
    }).format(new Date(ts));
  } catch {
    return "";
  }
}

export function slotProgress(slot: SaveData | null): { title: string; detail: string } {
  if (!slot || !slotOccupied(slot)) return { title: "Vazio", detail: "Nenhuma campanha" };
  if (slot.battle) {
    const m = ALL_MISSIONS.find((x) => x.id === slot.battle!.missionId);
    return { title: m ? m.title : slot.battle.missionId, detail: `Em combate · turno ${slot.battle.turn}` };
  }
  if (slot.pendingMission) {
    const m = ALL_MISSIONS.find((x) => x.id === slot.pendingMission);
    return { title: m ? m.title : slot.pendingMission, detail: "Início do combate" };
  }
  // On the RPG map the party has a real position: name the place it is standing on (or the
  // nearest one, when it is out on the road) instead of guessing from completed missions.
  if (slot.mapMode === "rpg" && slot.overworldPos) {
    const { col, row } = slot.overworldPos;
    const here = locationAt(WORLD_LOCATIONS, col, row);
    if (here) return { title: here.name, detail: `Dia ${slot.gameClock ?? 0} · mapa` };
    const toCube = (x: number, y: number) => { const q = x - (y - (y & 1)) / 2; return { q, r: y, s: -q - y }; };
    const a = toCube(col, row);
    let nearest: { name: string; d: number } | null = null;
    for (const loc of WORLD_LOCATIONS) {
      const h = worldToHex(loc.x, loc.y);
      const b = toCube(h.x, h.y);
      const d = (Math.abs(a.q - b.q) + Math.abs(a.r - b.r) + Math.abs(a.s - b.s)) / 2;
      if (!nearest || d < nearest.d) nearest = { name: loc.name, d };
    }
    if (nearest) return { title: `Estrada perto de ${nearest.name}`, detail: `Dia ${slot.gameClock ?? 0} · mapa` };
  }
  if (slot.completed.length === 0) return { title: "Campanha nova", detail: "Mapa de cenários" };
  const lastId = slot.completed[slot.completed.length - 1]!;
  const last = ALL_MISSIONS.find((x) => x.id === lastId);
  const next = ALL_MISSIONS.find((x) => last && x.index === last.index + 1);
  if (next) return { title: next.title, detail: `Após ${last?.title ?? lastId}` };
  return { title: last?.title ?? lastId, detail: "Campanha concluída" };
}

export function hasAnySave(bank: SaveBank): boolean {
  return bank.slots.some(slotOccupied);
}

export function isSlotEmpty(slot: SaveData | null): boolean {
  return !slotOccupied(slot);
}
