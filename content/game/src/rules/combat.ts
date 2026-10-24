import { dexAccuracy } from "./dexterity.ts";
import { equippedWeaponType, weaponModifiers } from "./weaponSkills.ts";
import { EQUIPMENT, isProjectile, rollDice, TERRAIN, WEAPONS } from "../ember/data.ts";
import { canHitFrom, hexDist, hexNeighbors } from "./pathfinding.ts";
import type { Forecast, TerrainId, TerrainDef, Unit } from "../ember/types.ts";

/** Two adjacent hexes behind the defender's board heading, independent of camera rotation. */
export function isRearAttack(attacker: Unit, defender: Unit): boolean {
  if (!Number.isFinite(attacker.x) || !Number.isFinite(defender.x) || hexDist(attacker, defender) !== 1) return false;
  const dx = defender.faceDx ?? defender.facing ?? 1;
  const dy = defender.faceDy ?? 0;
  if (Math.hypot(dx, dy) < 1e-6) return false;
  const rear = Math.atan2(-dy, -dx);
  const originX = defender.x + (defender.y & 1) * .5;
  const sectors = hexNeighbors(defender.x, defender.y).map(cell => {
    const angle = Math.atan2((cell.y - defender.y) * Math.sqrt(3) / 2, cell.x + (cell.y & 1) * .5 - originX);
    const distance = Math.abs(Math.atan2(Math.sin(angle - rear), Math.cos(angle - rear)));
    return { cell, distance };
  }).sort((a, b) => Math.round((a.distance - b.distance) * 1e9) || a.cell.y - b.cell.y || a.cell.x - b.cell.x);
  return sectors.slice(0, 2).some(({ cell }) => cell.x === attacker.x && cell.y === attacker.y);
}

/** What a unit's own stat contributes to a hit: half of ATK, or half of MAG for a caster.
 *
 * Halved because the stat used to be the whole story — a point of ATK was a point of
 * damage, so levels drowned out what a unit was carrying. At half weight the weapon and
 * spell dice decide as much as the stat does. Rounded down; there are no half points. */
export function powerOf(unit: Unit): number {
  return Math.floor((unit.mag > 0 ? unit.mag : unit.atk) / 2);
}

/** Physical attacks subtract half DEF. Elemental resistances protect against magic. */
export function protOf(attacker: Unit, defender: Unit): number {
  return Math.floor((attacker.mag > 0 ? 0 : defender.def) / 2);
}

/** High ground's attack bonus, as a fraction of the attacker's own ATK (or MAG for a
 * caster) instead of the old flat +2 — so it keeps mattering as a character grows instead
 * of shrinking to nothing at high levels. The range bonus (effectiveMaxRange /
 * effectiveMaxRangeAt) is untouched: still a flat +1 for a ranged weapon. */
const HIGH_GROUND_ATK_PCT = 0.1;

function terrainBonus(attacker: Unit, defender: Unit, attTile: TerrainId, defTile: TerrainId, ground?: {att:TerrainDef;def:TerrainDef}): { atk: number; def: number } {
  const attT = ground?.att ?? TERRAIN[attTile];
  const defT = ground?.def ?? TERRAIN[defTile];
  let def = defT.def;
  if (isProjectile(attacker) && defT.cover) def += defT.cover;
  const atk = attT.height ? Math.round((attacker.mag > 0 ? attacker.mag : attacker.atk) * HIGH_GROUND_ATK_PCT) : attT.atk;
  return { atk, def };
}

/** A weapon tuned for one specific class (a staff named after a school of magic, say) hits
 * 10% harder in that class's own hands — everyone else in its usableBy pool can still wield
 * it at no penalty, just without this. */
const WEAPON_CLASS_BONUS_MUL = 1.1;
function weaponClassBonusMul(attacker: Unit): number {
  const weapon = attacker.weaponId ? WEAPONS[attacker.weaponId] : null;
  return weapon?.bonusClass === attacker.classId ? WEAPON_CLASS_BONUS_MUL : 1;
}

export function rollDamage(
  attacker: Unit,
  defender: Unit,
  attTile: TerrainId,
  defTile: TerrainId,
  rng: () => number,
  useWeaponSkill = true,
  ground?: {att:TerrainDef;def:TerrainDef},
): { dmg: number; crit: boolean; landed: boolean; hitChance: number; preCritDmg: number } {
  const b = terrainBonus(attacker, defender, attTile, defTile, ground);
  const rear = useWeaponSkill && isRearAttack(attacker, defender);
  const mastery = useWeaponSkill ? weaponModifiers(attacker, equippedWeaponType(attacker)) : { accuracy: 100, damage: 1 };
  const definition = attacker.weaponId ? WEAPONS[attacker.weaponId] : undefined;
  const weapon = definition ? rollDice(definition.dice, definition.faces, 0, rng) * mastery.damage + definition.bonus + attacker.weaponEnh : 0;
  const hitChance = useWeaponSkill ? dexAccuracy(mastery.accuracy + (rear ? 10 : 0), defender.dex ?? 0) : 100;
  const raw = powerOf(attacker) + weapon + b.atk - protOf(attacker, defender) - b.def;
  const preCritDmg = Math.max(1, Math.floor(Math.max(1, raw) * weaponClassBonusMul(attacker) * (rear ? 1.1 : 1)));
  let dmg = preCritDmg;
  const crit = rng() < 0.08;
  if (crit) dmg = Math.max(1, Math.floor(dmg * 1.5));
  return { dmg, crit, landed: hitChance >= 100 || rng() * 100 < hitChance, hitChance, preCritDmg };
}

/** Same formula as rollDamage, but rolling explicit dice instead of the attacker's
 * equipped main-hand weapon — for an off-hand weapon attack, whose dice come from the
 * EquipmentDef in the offHand slot rather than WEAPONS[attacker.weaponId]. */
export function rollDamageCustom(
  attacker: Unit,
  defender: Unit,
  attTile: TerrainId,
  defTile: TerrainId,
  dice: number,
  faces: number,
  bonus: number,
  rng: () => number,
  useWeaponSkill = true,
  ground?: {att:TerrainDef;def:TerrainDef},
): { dmg: number; crit: boolean; landed: boolean; hitChance: number; preCritDmg: number } {
  const b = terrainBonus(attacker, defender, attTile, defTile, ground);
  const rear = useWeaponSkill && isRearAttack(attacker, defender);
  const mastery = useWeaponSkill ? weaponModifiers(attacker, equippedWeaponType(attacker, true)) : { accuracy: 100, damage: 1 };
  const weapon = rollDice(dice, faces, 0, rng) * mastery.damage + bonus;
  const hitChance = useWeaponSkill ? dexAccuracy(mastery.accuracy + (rear ? 10 : 0), defender.dex ?? 0) : 100;
  const raw = powerOf(attacker) + weapon + b.atk - protOf(attacker, defender) - b.def;
  const preCritDmg = Math.max(1, Math.floor(Math.max(1, raw) * (rear ? 1.1 : 1)));
  let dmg = preCritDmg;
  const crit = rng() < 0.08;
  if (crit) dmg = Math.max(1, Math.floor(dmg * 1.5));
  return { dmg, crit, landed: hitChance >= 100 || rng() * 100 < hitChance, hitChance, preCritDmg };
}

export function previewDamage(
  attacker: Unit,
  defender: Unit,
  attTile: TerrainId,
  defTile: TerrainId,
  offHand = false,
  useWeaponSkill = true,
): { dmg: number; hitChance: number } {
  const b = terrainBonus(attacker, defender, attTile, defTile);
  const rear = useWeaponSkill && isRearAttack(attacker, defender);
  const item = offHand && attacker.offHandId ? EQUIPMENT[attacker.offHandId] : undefined;
  const mastery = useWeaponSkill ? weaponModifiers(attacker, equippedWeaponType(attacker, offHand)) : { accuracy: 100, damage: 1 };
  const definition = attacker.weaponId ? WEAPONS[attacker.weaponId] : undefined;
  const weapon = offHand
    ? item?.kind === "weapon" ? (item.dice ?? 1) * ((item.faces ?? 4) + 1) / 2 * mastery.damage + (item.bonus ?? 0) : 0
    : definition ? definition.dice * (definition.faces + 1) / 2 * mastery.damage + definition.bonus + attacker.weaponEnh : 0;
  const hitChance = useWeaponSkill ? dexAccuracy(mastery.accuracy + (rear ? 10 : 0), defender.dex ?? 0) : 100;
  const raw = powerOf(attacker) + weapon + b.atk - protOf(attacker, defender) - b.def;
  const dmg = Math.max(1, Math.floor(Math.max(1, raw) * (offHand ? 1 : weaponClassBonusMul(attacker)) * (rear ? 1.1 : 1)));
  return { dmg, hitChance };
}

export function canCounter(
  attacker: Unit,
  defender: Unit,
  from: { x: number; y: number },
  tiles: TerrainId[],
  cols: number,
): boolean {
  if (!defender.alive) return false;
  const target = { ...attacker, x: from.x, y: from.y };
  if (canHitFrom(defender, { x: defender.x, y: defender.y }, target, tiles, cols, undefined, true)) return true;
  const offHand = defender.offHandId ? EQUIPMENT[defender.offHandId] : null;
  if (offHand?.kind !== "weapon") return false;
  const meleeCounter = { ...defender, minRange: offHand.minRange ?? 1, maxRange: offHand.maxRange ?? 1 };
  return canHitFrom(meleeCounter, { x: defender.x, y: defender.y }, target, tiles, cols, undefined, true);
}

export function makeForecast(
  attacker: Unit,
  defender: Unit,
  attTile: TerrainId,
  defTile: TerrainId,
  tiles: TerrainId[],
  cols: number,
  offHand = false,
  useWeaponSkill = true,
): Forecast {
  const out = previewDamage(attacker, defender, attTile, defTile, offHand, useWeaponSkill);
  const counter = canCounter(attacker, defender, { x: attacker.x, y: attacker.y }, tiles, cols);
  const counterWeapon = defender.offHandId ? EQUIPMENT[defender.offHandId] : undefined;
  const daggerCounter = counterWeapon?.kind === "weapon" && hexDist(defender, attacker) <= (counterWeapon.maxRange ?? 1);
  const facingAttacker = { ...attacker, faceDx: defender.x + (defender.y & 1) * .5 - attacker.x - (attacker.y & 1) * .5, faceDy: (defender.y - attacker.y) * Math.sqrt(3) / 2 };
  const back = counter ? previewDamage(defender, facingAttacker, defTile, attTile, daggerCounter) : null;
  return {
    attacker: attacker.id,
    defender: defender.id,
    dmgOut: out.dmg,
    dmgBack: back?.dmg ?? 0,
    hitOut: out.hitChance,
    hitBack: back?.hitChance ?? 0,
    canCounter: counter,
    critOut: false,
    kill: out.dmg >= defender.hp,
  };
}

export function mulberry32(seed: number): () => number {
  let a = seed | 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
