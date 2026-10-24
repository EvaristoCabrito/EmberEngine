/** Pure versions of GameApp's real campaign shop actions, with Ember's prices/caps. */
import { BAG_MAX, EQUIPMENT, LOCKPICK_PRICE, POTION_CARRY_MAX, POTION_PRICE, RATIONS_PRICE, RATION_STACK_MAX, WEAPON_MAX_ENH, WEAPONS, partyBagHasRoom, startingBags, weaponEnhCost, weaponSellValue } from '../ember/data';
import type { PotionId, SaveData } from '../ember/types';

export function buyWeapon(save: SaveData, id: string): SaveData | null {
  const weapon = WEAPONS[id];
  if (!weapon || save.weapons[id] != null || !partyBagHasRoom(save, 1) || save.ember < weapon.price) return null;
  return { ...save, ember: save.ember - weapon.price, emberSeeded: true, weapons: { ...save.weapons, [id]: 0 }, pendingMission: null };
}
export function buyEquipment(save: SaveData, id: string): SaveData | null {
  const item = EQUIPMENT[id]; const price = item?.price ?? 0;
  if (!item || price <= 0 || !partyBagHasRoom(save, 1) || save.ember < price) return null;
  return { ...save, ember: save.ember - price, emberSeeded: true, looseEquipment: { ...save.looseEquipment, [id]: (save.looseEquipment[id] ?? 0) + 1 }, pendingMission: null };
}
export function upgradeWeapon(save: SaveData, id: string): SaveData | null {
  const enhancement = save.weapons[id];
  if (!WEAPONS[id] || enhancement == null || enhancement >= WEAPON_MAX_ENH) return null;
  const cost = weaponEnhCost(enhancement + 1);
  if (save.ember < cost) return null;
  return { ...save, ember: save.ember - cost, emberSeeded: true, weapons: { ...save.weapons, [id]: enhancement + 1 }, pendingMission: null };
}
export function sellWeapon(save: SaveData, id: string): SaveData | null {
  const enhancement = save.weapons[id];
  if (!WEAPONS[id] || enhancement == null) return null;
  const weapons = { ...save.weapons }; delete weapons[id];
  const equipped = { ...save.equipped };
  for (const hero of Object.keys(equipped)) if (equipped[hero] === id) delete equipped[hero];
  return { ...save, weapons, equipped, ember: save.ember + weaponSellValue(id, enhancement), emberSeeded: true, pendingMission: null };
}
export function buyConsumables(save: SaveData, hero: string, cart: Partial<Record<PotionId, number>>, lockpicks = 0): SaveData | null {
  const base = save.bags[hero] ?? startingBags()[hero];
  if (!base || !Number.isInteger(lockpicks) || lockpicks < 0) return null;
  const bag = { ...base }; let cost = 0;
  for (const kind of Object.keys(cart) as PotionId[]) {
    const qty = cart[kind] ?? 0;
    if (!Number.isInteger(qty) || qty < 0 || !(kind in POTION_PRICE)) return null;
    if (qty === 0) continue;
    if ((bag[kind] ?? 0) + qty > POTION_CARRY_MAX[kind]) return null;
    cost += POTION_PRICE[kind] * qty; bag[kind] = (bag[kind] ?? 0) + qty;
  }
  if (bag.lockpick + lockpicks > BAG_MAX) return null;
  bag.lockpick += lockpicks; cost += LOCKPICK_PRICE * lockpicks;
  if (cost <= 0 || save.ember < cost) return null;
  return { ...save, ember: save.ember - cost, emberSeeded: true, bags: { ...save.bags, [hero]: bag }, pendingMission: null };
}
export function buyRations(save: SaveData, qty: number): SaveData | null {
  if (!Number.isInteger(qty) || qty <= 0) return null;
  const slots = Math.ceil((save.rations + qty) / RATION_STACK_MAX) - Math.ceil(save.rations / RATION_STACK_MAX);
  const cost = RATIONS_PRICE * qty;
  if (!partyBagHasRoom(save, slots) || save.ember < cost) return null;
  return { ...save, ember: save.ember - cost, emberSeeded: true, rations: save.rations + qty, pendingMission: null };
}
