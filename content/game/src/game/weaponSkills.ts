import { EQUIPMENT, WEAPONS } from "./data.ts";
import type { ClassId, Unit, WeaponType, WeaponSkillValues } from "./types.ts";
import { WEAPON_TYPES, cleanWeaponSkill, weaponSkillAccuracy, weaponSkillDamageMultiplier } from "./weaponTypes.ts";
import type { HeroSkills } from "./skills.ts";

/** Derive the skill pool from the same permissions that govern equipping weapons. */
export function weaponTypesForClass(classId: ClassId): WeaponType[] {
  const pool = new Set<WeaponType>();
  for (const weapon of Object.values(WEAPONS)) if (weapon.usableBy.includes(classId)) pool.add(weapon.weaponType);
  for (const item of Object.values(EQUIPMENT)) {
    if (item.kind === "weapon" && item.weaponType && (!item.usableBy?.length || item.usableBy.includes(classId))) pool.add(item.weaponType);
  }
  return WEAPON_TYPES.filter(type => pool.has(type));
}

export function trainedWeaponSkills(skills: HeroSkills | undefined, hero: string, classId: ClassId): WeaponSkillValues {
  return Object.fromEntries(weaponTypesForClass(classId).map(type => [type, cleanWeaponSkill(skills?.[hero]?.[`${type}Weapon`])]));
}

export function equippedWeaponType(unit: Pick<Unit, "classId" | "weaponId" | "offHandId">, offHand = false): WeaponType | undefined {
  if (offHand) {
    const item = unit.offHandId ? EQUIPMENT[unit.offHandId] : undefined;
    return item?.kind === "weapon" && (!item.usableBy?.length || item.usableBy.includes(unit.classId)) ? item.weaponType : undefined;
  }
  const weapon = unit.weaponId ? WEAPONS[unit.weaponId] : undefined;
  return weapon?.usableBy.includes(unit.classId) ? weapon.weaponType : undefined;
}

export function weaponModifiers(unit: Pick<Unit, "side" | "summoned" | "classId" | "weaponSkills" | "blessedHitBonusPct" | "dex">, type: WeaponType | undefined): { accuracy: number; damage: number } {
  if (unit.side === "enemy") return { accuracy: 90 + (unit.dex ?? 0) + (unit.blessedHitBonusPct ?? 0) * 100, damage: 1 };
  if (!type || !weaponTypesForClass(unit.classId).includes(type)) return { accuracy: 100, damage: 1 };
  const value = unit.weaponSkills?.[type] ?? 0;
  return { accuracy: weaponSkillAccuracy(value) + (unit.blessedHitBonusPct ?? 0) * 100, damage: weaponSkillDamageMultiplier(value) };
}

/** Weapon abilities share mastery; ordinary magic, shield bashes and body attacks do not. */
export function isWeaponAbility(kind: string | null): boolean {
  return ["longShot", "bloodyShot", "multiShot", "piercing", "piercingThrust", "cleave", "doubleStrike", "sweep", "trip", "bullRush", "executionerStrike"].includes(kind ?? "");
}
