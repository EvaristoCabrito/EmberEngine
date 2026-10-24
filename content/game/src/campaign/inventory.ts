import { EQUIPMENT, equipmentFitsSlot } from "../ember/data";
import type { SaveData, EquipSlot } from "../ember/types";

/** Weapons belong to the party. Equipping one moves it from the previous wielder; only
 * potions and lockpicks stay in the individual bags. */
export function equipSharedWeapon(save: SaveData, hero: string, weaponId: string): SaveData | null {
  if (save.weapons[weaponId] == null) return null;
  const equipped = { ...save.equipped };
  for (const [owner, id] of Object.entries(equipped)) {
    if (owner !== hero && id === weaponId) delete equipped[owner];
  }
  equipped[hero] = weaponId;
  return { ...save, equipped };
}

/** Equipment is a physical party pool: select a reserve piece or transfer one from another
 * hero. Consumable bags are intentionally not part of this function. */
export function equipSharedItem(save: SaveData, hero: string, slot: EquipSlot, itemId: string): SaveData | null {
  const item = EQUIPMENT[itemId];
  if (!item || !equipmentFitsSlot(item, slot)) return null;
  const reserve = save.looseEquipment[itemId] ?? 0;
  const current = save.equipment[hero]?.[slot];
  if (current === itemId) return save;
  // Prefer a spare in the stash so two copies of the same ring can fill both fingers.
  // Only pull the piece off another slot when there is no reserve left.
  const source =
    reserve > 0
      ? undefined
      : Object.entries(save.equipment)
          .flatMap(([owner, slots]) => (Object.entries(slots) as [EquipSlot, string][]).map(([usedSlot, id]) => ({ owner, usedSlot, id })))
          .find((entry) => entry.id === itemId && !(entry.owner === hero && entry.usedSlot === slot));
  if (!source && reserve <= 0) return null;

  const equipment = Object.fromEntries(Object.entries(save.equipment).map(([owner, slots]) => [owner, { ...slots }])) as SaveData["equipment"];
  const looseEquipment = { ...save.looseEquipment };
  if (source) delete equipment[source.owner]![source.usedSlot];
  else if (reserve === 1) delete looseEquipment[itemId];
  else looseEquipment[itemId] = reserve - 1;
  if (current) looseEquipment[current] = (looseEquipment[current] ?? 0) + 1;
  equipment[hero] = { ...(equipment[hero] ?? {}), [slot]: itemId };
  return { ...save, equipment, looseEquipment };
}

export function unequipSharedItem(save: SaveData, hero: string, slot: EquipSlot): SaveData {
  const current = save.equipment[hero]?.[slot];
  if (!current) return save;
  const equipment = Object.fromEntries(Object.entries(save.equipment).map(([owner, slots]) => [owner, { ...slots }])) as SaveData["equipment"];
  const looseEquipment = { ...save.looseEquipment };
  delete equipment[hero]![slot];
  looseEquipment[current] = (looseEquipment[current] ?? 0) + 1;
  return { ...save, equipment, looseEquipment };
}

export function unequipSharedWeapon(save: SaveData, hero: string): SaveData {
  if (!save.equipped[hero]) return save;
  const equipped = { ...save.equipped };
  delete equipped[hero];
  return { ...save, equipped };
}

/** Discards one owned-but-unequipped weapon for good. Only ever offered on a weapon the
 * "Itens da party" grid already shows — which itself only lists weapons nobody currently
 * has equipped — so this never needs to touch save.equipped. */
export function discardSharedWeapon(save: SaveData, weaponId: string): SaveData {
  if (save.weapons[weaponId] == null) return save;
  const weapons = { ...save.weapons };
  delete weapons[weaponId];
  return { ...save, weapons };
}

/** Discards one spare copy of a piece of equipment from the party's shared stash. Only the
 * loose pool, never a copy someone currently has on — see the "Jogar Fora" gate in
 * InventoryScreens.tsx, which only enables when looseEquipment[itemId] > 0. */
export function discardSharedEquipment(save: SaveData, itemId: string): SaveData {
  const reserve = save.looseEquipment[itemId] ?? 0;
  if (reserve <= 0) return save;
  const looseEquipment = { ...save.looseEquipment };
  if (reserve <= 1) delete looseEquipment[itemId];
  else looseEquipment[itemId] = reserve - 1;
  return { ...save, looseEquipment };
}

/** Discards one ration from the party's shared stock. */
export function discardRation(save: SaveData): SaveData {
  if (save.rations <= 0) return save;
  return { ...save, rations: save.rations - 1 };
}

