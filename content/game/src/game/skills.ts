import { RESISTANCE_ELEMENTS } from "./resistances.ts";
import { WEAPON_TYPES, WEAPON_TYPE_LABELS, cleanWeaponSkill } from "./weaponTypes.ts";
import type { Resistances, ResistanceElement, WeaponType } from "./types";
/** Per-hero skills (party menu "Skills" tab). Each one climbs slowly with use, Ultima Online
 * style: every check rolls a chance to gain SKILL_GAIN points, and that chance shrinks the
 * closer the skill is to SKILL_CAP, so the last points are the hardest to earn. */
export type SkillId = `${ResistanceElement}Resistance` | `${WeaponType}Weapon` | "healing";
export type HeroSkillValues = Partial<Record<SkillId, number>>;
export type HeroSkills = Record<string, HeroSkillValues>;

export const SKILLS: Record<SkillId, { name: string; description: string }> = {
  healing: { name: "Healing", description: "Each skill point adds 1% to the total healing effect. Improves through effective healing." },
  ...Object.fromEntries(WEAPON_TYPES.map(type => [`${type}Weapon`, { name: WEAPON_TYPE_LABELS[type], description: "Improves weapon accuracy and damage through combat use." }])) as Record<`${WeaponType}Weapon`, { name: string; description: string }>,
  fireResistance: { name: "Fire Resistance", description: "Reduces fire damage. Improves by using or being hit by fire magic." },
  lightningResistance: { name: "Lightning Resistance", description: "Reduces lightning and its delayed damage. Improves by using or being hit by lightning magic." },
  iceResistance: { name: "Ice Resistance", description: "Reduces ice damage. Improves by using or being hit by ice magic." },
  arcaneResistance: { name: "Arcane Resistance", description: "Reduces arcane damage. Improves by using or being hit by arcane magic." },
  darknessResistance: { name: "Darkness Resistance", description: "Reduces darkness damage. Improves by using or being hit by darkness magic." },
  holyResistance: { name: "Holy Resistance", description: "Reduces holy damage. Improves by using or being hit by holy magic." },
  poisonResistance: { name: "Poison Resistance", description: "Reduces poison damage and application chance. Improves by using or being hit by poison magic." },
  emberResistance: { name: "Ember Resistance", description: "Reduces ember damage. Improves by using or being hit by ember magic." },
};

export function skillResistances(skills: HeroSkills | undefined, hero: string): Resistances {
  return Object.fromEntries(RESISTANCE_ELEMENTS.map(element => [element, skillValue(skills, hero, `${element}Resistance`)]));
}

export const SKILL_IDS = Object.keys(SKILLS) as SkillId[];
export const SKILL_CAP = 100;
export const SKILL_GAIN = 0.1;

export function skillValue(skills: HeroSkills | undefined, hero: string, id: SkillId): number {
  return skills?.[hero]?.[id] ?? 0;
}

/** Retain the exact skill percentage; only the final HP result is rounded down. */
export function healingAmount(base: number, skill: number): number {
  const points = Number.isFinite(skill) ? Math.max(0, Math.min(SKILL_CAP, skill)) : 0;
  return Math.floor(Math.max(0, base) * (1 + points / 100) + 1e-9);
}

/** Chance (0–1) that one use of the skill raises it: 100% at 0, 50% at 50, none at the cap. */
export function skillGainChance(value: number): number {
  return Math.max(0, Math.min(1, (SKILL_CAP - value) / SKILL_CAP));
}

/** One use of the skill. Returns the new value when it went up, otherwise null. */
export function rollSkillGain(value: number, rng: () => number, amount = SKILL_GAIN): number | null {
  if (rng() >= skillGainChance(value)) return null;
  return Math.min(SKILL_CAP, Math.round((value + amount) * 100) / 100);
}

/** Travel training: each hero may practise one chosen skill on the road, gaining a flat
 * SKILL_GAIN for every TRAVEL_TRAINING_HOURS of travel. Nothing trains until a skill is picked. */
export const TRAVEL_TRAINING_HOURS = 12;
/** save.flags entry set once the player has seen (or acted on) the travel-training hint. */
export const TRAVEL_TRAINING_HINT_FLAG = "hint:travel-training";

/** All skills preserve fractional gains and earn 0.1 per 12 hours of travel. */
export function travelTrainingStep(_id: SkillId): { hours: number; gain: number } {
  return { hours: TRAVEL_TRAINING_HOURS, gain: SKILL_GAIN };
}

/** Banks `hours` of road time for every listed hero with a chosen skill and pays out its
 * travelTrainingStep gain per full step; leftover hours carry to the next step. */
export function advanceTravelTraining(
  skills: HeroSkills | undefined,
  training: Partial<Record<string, SkillId>> | undefined,
  banked: Record<string, number> | undefined,
  hours: number,
  heroes: readonly string[],
): { heroSkills: HeroSkills; travelTrainingHours: Record<string, number> } {
  const heroSkills: HeroSkills = { ...skills };
  const travelTrainingHours: Record<string, number> = { ...banked };
  for (const hero of heroes) {
    const id = training?.[hero];
    if (!id) continue;
    const step = travelTrainingStep(id);
    const total = (travelTrainingHours[hero] ?? 0) + Math.max(0, hours);
    const gains = Math.floor(total / step.hours);
    travelTrainingHours[hero] = total - gains * step.hours;
    if (gains > 0) {
      const next = Math.min(SKILL_CAP, Math.round((skillValue(heroSkills, hero, id) + gains * step.gain) * 10) / 10);
      heroSkills[hero] = { ...heroSkills[hero], [id]: next };
    }
  }
  return { heroSkills, travelTrainingHours };
}

export function cleanTravelTraining(raw: unknown, heroes: readonly string[]): Partial<Record<string, SkillId>> {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return {};
  const out: Partial<Record<string, SkillId>> = {};
  for (const hero of heroes) {
    const id = (raw as Record<string, unknown>)[hero];
    if (typeof id === "string" && (SKILL_IDS as string[]).includes(id)) out[hero] = id as SkillId;
  }
  return out;
}

export function cleanHeroSkills(raw: unknown, heroes: readonly string[], allowedWeapon?: (hero: string, type: WeaponType) => boolean, roundLegacyWeaponFractions = false): HeroSkills {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return {};
  const out: HeroSkills = {};
  for (const hero of heroes) {
    const values = (raw as Record<string, unknown>)[hero];
    if (!values || typeof values !== "object") continue;
    const clean: HeroSkillValues = {};
    for (const id of SKILL_IDS) {
      if (id.endsWith("Weapon") && allowedWeapon && !allowedWeapon(hero, id.slice(0, -6) as WeaponType)) continue;
      const v = (values as Record<string, unknown>)[id];
      if (typeof v === "number" && Number.isFinite(v)) clean[id] = id.endsWith("Weapon") ? cleanWeaponSkill(v, roundLegacyWeaponFractions) : Math.max(0, Math.min(SKILL_CAP, Math.round(v * 100) / 100));
    }
    if (Object.keys(clean).length) out[hero] = clean;
  }
  return out;
}

/** Weapon proficiency gains tenths, retaining smaller gains against lower-level enemies. */
export function rollWeaponSkillGain(value: number, rng: () => number, amount = SKILL_GAIN): number | null {
  const current = cleanWeaponSkill(value);
  if (current >= SKILL_CAP || rng() >= skillGainChance(current)) return null;
  return Math.min(SKILL_CAP, Math.round((current + amount) * 100) / 100);
}
