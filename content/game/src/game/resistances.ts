import type { Resistances, ResistanceElement, SpellKind } from "./types";

export const RESISTANCE_ELEMENTS = ["fire", "lightning", "ice", "arcane", "darkness", "holy", "poison", "ember"] as const;
export const RESISTANCE_LABELS: Record<ResistanceElement, string> = {
  fire: "Fire", lightning: "Lightning", ice: "Ice", arcane: "Arcane",
  darkness: "Darkness", holy: "Holy", poison: "Poison", ember: "Ember",
};

/** Stored resistance stays uncapped: penetration is subtracted before clamping. */
export function effectiveResistance(resistance: number, mag = 0, penalty = 0): number {
  return Math.max(-50, Math.min(100, resistance - penalty - mag / 2));
}

/** Round once, after resistance; full immunity must be able to produce zero damage. */
export function elementalDamage(baseDamage: number, resistance: number, mag = 0, penalty = 0): number {
  return Math.max(0, baseDamage) * (1 - effectiveResistance(resistance, mag, penalty) / 100);
}

export function sumResistances(...sources: (Resistances | undefined)[]): Resistances {
  return Object.fromEntries(RESISTANCE_ELEMENTS.map(element => [element,
    sources.reduce((sum, source) => sum + (source?.[element] ?? 0), 0),
  ]));
}

/** Save compatibility: absent elements are zero; reject malformed/nonfinite values. */
export function cleanResistances(raw: unknown): Resistances | undefined {
  if (!raw || typeof raw !== "object") return undefined;
  const source = raw as Record<string, unknown>;
  return Object.fromEntries(RESISTANCE_ELEMENTS.map(element => {
    const value = source[element];
    return [element, typeof value === "number" && Number.isFinite(value) ? Math.max(-999, Math.min(999, value)) : 0];
  }));
}

/** Every SpellKind must be classified here. Elemental attacks use their matching resistance;
 * weapon techniques stay physical, and effects without a damage element stay utility. Keeping
 * the full list exhaustive means a newly added spell cannot silently bypass this decision. */
export type SpellClassification = ResistanceElement | "physical" | "utility";

export const SPELL_CLASSIFICATION: Record<SpellKind, SpellClassification> = {
  fireball: "fire",
  iceStorm: "ice",
  frost: "ice",
  bless: "utility",
  provoke: "utility",
  cureMinor: "utility",
  cureWounds: "utility",
  cureLight: "utility",
  longShot: "physical",
  bloodyShot: "physical",
  piercing: "physical",
  lightning: "lightning",
  lightningTier3: "lightning",
  magicMissile: "arcane",
  magicMissileV2: "arcane",
  causticVenom: "poison",
  divineBolt: "holy",
  minorVenom: "poison",
  doubleStrike: "physical",
  cleave: "physical",
  cureDisease: "utility",
  piercingThrust: "physical",
  sweep: "physical",
  trip: "physical",
  summonFamiliar: "utility",
  phantasmalForce: "arcane",
  fantomForce: "arcane",
  summonFamiliar2: "utility",
  summonFamiliar3: "utility",
  summonFamiliar4: "utility",
  summonZombieDog: "utility",
  lifeDrain: "darkness",
  webOfDreams: "arcane",
  warp: "arcane",
  multiShot: "physical",
  secondWind: "utility",
  auraOfProtection: "utility",
  divineWrath: "holy",
  shoulderSmash: "physical",
  intimidatingPresence: "utility",
  stampede: "physical",
  shock: "lightning",
  bullRush: "physical",
  executionerStrike: "physical",
  shieldBash: "physical",
  poisonBreath: "poison",
  tendrilSwipe: "physical",
  burningHands: "fire",
  createFoodAndWater: "utility",
  turnUndead: "holy",
};

export function spellElement(kind: SpellKind | null): ResistanceElement | undefined {
  if (!kind) return undefined;
  const classification = SPELL_CLASSIFICATION[kind];
  return classification === "physical" || classification === "utility" ? undefined : classification;
}
