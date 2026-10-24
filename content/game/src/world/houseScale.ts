/** Door opening heights measured as shares of the preserved source canvases. */
export const HOUSE_DOOR_FRACTION: Readonly<Record<string, number>> = {
  'farmland-photographic-014-store-house': 0.265,
  'farmland-photographic-013-tavern-house': 0.265,
  'farmland-photographic-014-villagers-house': 0.215,
  'farmland-photographic-013-inn-house': 0.280,
  'farmland-photographic-013-smith-house': 0.235,
  'farmland-photographic-013-apothecary-house': 0.370,
};

const GROUNDED_DOOR_FRACTION: Readonly<Record<string, number>> = {"farmland-photographic-014-store-house": 0.335, "farmland-photographic-014-villagers-house": 0.292, "farmland-photographic-013-inn-house": 0.322, "farmland-photographic-013-tavern-house": 0.348, "farmland-photographic-013-smith-house": 0.29, "farmland-photographic-013-apothecary-house": 0.38};

// Ordinary humans stand roughly 2.4-2.65 world units high; a door gives modest headroom.
export const HOUSE_DOOR_HEIGHT = 2.8;
export function fitHouseToHumans(id: string, size: { w: number; h: number }, tile = 1): { w: number; h: number } {
  const baseId = id.replace(/-(grounded-v2|original-v1)$/, "");
  const door = id.endsWith("-original-v1") ? HOUSE_DOOR_FRACTION[baseId] : GROUNDED_DOOR_FRACTION[baseId] ?? HOUSE_DOOR_FRACTION[baseId];
  if (!door) return size;
  const scale = Math.min(1, HOUSE_DOOR_HEIGHT * tile / (size.h * door));
  return { w: size.w * scale, h: size.h * scale };
}
