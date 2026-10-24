// Ember tile-variant numbering (from Ember src/game/assets.ts, unchanged). Data only: maps store a
// variant index per hex, and these keep saved indices valid. The old tile art itself is not used.
import type { TerrainId } from "./types";

// Number of art variants available per terrain, e.g. plains001.png / plains002.png.
// Index 0 (the "001" file) is what every mission renders with unless it names a
// different variant in Mission.tileVariants — keep it as the tile that's safe
// for existing maps.
export const TILE_VARIANT_COUNT: Record<TerrainId, number> = {
  // 049 is the first numbered Engine2 PBR plains base. Keep every earlier index stable.
  // Claude's baked tiles remain at their original indices; the Engine2 V3 material versions append after them.
  plains: 66,
  woods: 66,
  ruins: 11,
  water: 31,
  ember: 9,
  hill: 8,
  flame: 7,
  column: 6,
  nave: 22,
  barricade: 3,
  door: 4,
  void: 1,
  // The Icelands section keeps legacy snow variants first, then the 12 supplied
  // cold-ground tiles (snow004–snow015) so saved maps retain their old indices.
  snow: 22,
};

/** Append-only photographic materials. Never reuse an existing saved-map variant index.
 * Each has separate normal/roughness data and geometric relief; source artwork stays intact. */
export const GROUND_VARIANT_LAYERS = [
  { key: 'variant-plains-meadow-v2', terrain: 'plains', variant: 52, file: 'plains-meadow-v2.png', label: 'Meadow II', relief: 0.08 },
  // moved from Barricada to Bosque on request (no saved map used it as barricade 2)
  // Plains 02: vivid flat grass, fine detail only, zero relief
  { key: 'variant-plains-vivid', terrain: 'plains', variant: 55, file: 'plains-vivid.png', label: 'Planície vívida', relief: 0 },
  { key: 'variant-barricade-earth-v2', terrain: 'woods', variant: 16, file: 'barricade-earth-v2.png', label: 'Barricade Earth II', relief: 0.12 },
  { key: 'variant-woods-floor-v2', terrain: 'woods', variant: 14, file: 'woods-floor-v2.png', label: 'Forest Floor II', relief: 0.10 },
  { key: 'photo-plains', terrain: 'plains', variant: 53, file: '../ground-engine2-v4/plains/color.png', label: 'Planície · Photo 3D', relief: 0.08 },
  { key: 'photo-city', terrain: 'plains', variant: 54, file: '../ground-engine2-v4/city/color.png', label: 'City · Photo 3D', relief: 0.06 },
  { key: 'photo-woods', terrain: 'woods', variant: 15, file: '../ground-engine2-v4/woods/color.png', label: 'Bosque · Photo 3D', relief: 0.12 },
  { key: 'photo-ruins', terrain: 'ruins', variant: 10, file: '../ground-engine2-v4/ruins/color.png', label: 'Ruínas · Photo 3D', relief: 0.16 },
  { key: 'photo-shallow-water', terrain: 'water', variant: 29, file: '../ground-engine2-v4/shallow-water/color.png', label: 'Água rasa · Photo 3D', relief: 0.10 },
  { key: 'photo-river-rocks', terrain: 'water', variant: 30, file: '../ground-engine2-v4/river-rocks/color.png', label: 'Pedras do vau · Photo 3D', relief: 0.24 },
  { key: 'photo-ember', terrain: 'ember', variant: 8, file: '../ground-engine2-v4/ember/color.png', label: 'Brasa · Photo 3D', relief: 0.10 },
  { key: 'photo-hill', terrain: 'hill', variant: 7, file: '../ground-engine2-v4/hill/color.png', label: 'Colina · Photo 3D', relief: 0.12 },
  { key: 'photo-flame', terrain: 'flame', variant: 6, file: '../ground-engine2-v4/flame/color.png', label: 'Chama · Photo 3D', relief: 0.10 },
  { key: 'photo-column', terrain: 'column', variant: 5, file: '../ground-engine2-v4/column/color.png', label: 'Coluna · Photo 3D', relief: 0.16 },
  { key: 'photo-nave', terrain: 'nave', variant: 21, file: '../ground-engine2-v4/nave/color.png', label: 'Laje · Photo 3D', relief: 0.06 },
  { key: 'photo-door', terrain: 'door', variant: 3, file: '../ground-engine2-v4/door/color.png', label: 'Porta · Photo 3D', relief: 0.06 },
  { key: 'variant-plains-smooth', terrain: 'plains', variant: 56, file: '../ground-claude/049-plains/color.png', label: 'Planicie suave 007', relief: 0 },
  { key: 'variant-plains-real-grass', terrain: 'plains', variant: 57, file: 'plains-real-grass-008-reference.png', label: 'Planicie real 008', relief: 0 },
  { key: 'variant-plains-real-grass-009', terrain: 'plains', variant: 58, file: 'plains-real-grass-009.png', label: 'Planicie real 009', relief: 0 },
  { key: 'variant-plains-meadow-3d', terrain: 'plains', variant: 59, file: 'plains-real-grass-008-reference.png', label: 'Prado 3D 010', relief: 0 },
  { key: 'variant-plains-tall-3d', terrain: 'plains', variant: 60, file: 'plains-real-grass-008-reference.png', label: 'Grama alta 3D 011', relief: 0 },
  // Explicit replacement of the duplicate Chama 005; its saved index remains 4.
  { key: 'variant-flame-005-fire', terrain: 'flame', variant: 4, file: 'flame-005-fire.png', label: '005', relief: 0 },
  { key: 'city-stone-paving', terrain: 'plains', variant: 50, file: 'city-stone-paving.png', label: 'Stone paving', relief: 0 },
  { key: 'city-indoor-wood', terrain: 'plains', variant: 61, file: 'city-indoor-wood.png', label: 'Indoor wood', relief: 0 },
  { key: 'laje-021-stone-slabs', terrain: 'nave', variant: 20, file: 'laje-021-stone-slabs.png', label: '021', relief: 0 },
  // Wisp Forest Revisited: three photographic, separately authored forest-floor materials.
  { key: 'variant-wisp-forest-leaf-litter-v01', terrain: 'woods', variant: 62, file: 'wisp-forest-leaf-litter-v01.png', label: 'Wisp · Folhas mortas', relief: 0 },
  { key: 'variant-wisp-forest-root-soil-v01', terrain: 'woods', variant: 63, file: 'wisp-forest-root-soil-v01.png', label: 'Wisp · Raízes expostas', relief: 0 },
  { key: 'variant-wisp-forest-ash-litter-v01', terrain: 'woods', variant: 64, file: 'wisp-forest-ash-litter-v01.png', label: 'Wisp · Cinza e folhas', relief: 0 },
  { key: 'variant-wisp-crossing-ground-plains-v02', terrain: 'plains', variant: 65, file: 'wisp-crossing-ground-v02.png', label: 'Wisp Crossing · Chão da arte original', relief: 0.035 },
  { key: 'variant-wisp-crossing-ground-woods-v02', terrain: 'woods', variant: 65, file: 'wisp-crossing-ground-v02.png', label: 'Wisp Crossing · Chão da arte original', relief: 0.035 },
  // Crossing uses the same authored forest floor across open clearings without changing walk terrain.
  { key: 'variant-wisp-crossing-leaf-litter-v01', terrain: 'plains', variant: 62, file: 'wisp-forest-leaf-litter-v01.png', label: 'Wisp Crossing · Folhas mortas', relief: 0 },
  { key: 'variant-wisp-crossing-root-soil-v01', terrain: 'plains', variant: 63, file: 'wisp-forest-root-soil-v01.png', label: 'Wisp Crossing · Raízes expostas', relief: 0 },
  { key: 'variant-wisp-crossing-ash-litter-v01', terrain: 'plains', variant: 64, file: 'wisp-forest-ash-litter-v01.png', label: 'Wisp Crossing · Cinza e folhas', relief: 0 },
] as const;

/** Stable lookup for new per-variant 3D material layers. */
export function groundVariantLayer(terrain: TerrainId, variant: number): number {
  const index = GROUND_VARIANT_LAYERS.findIndex((entry) => entry.terrain === terrain && entry.variant === variant);
  return index < 0 ? -1 : index;
}

/** One source of truth for the editor and gameplay material loader. */
export function groundVariantMaps(entry: (typeof GROUND_VARIANT_LAYERS)[number]) {
  const root = `/game/ground-variants-3d/${entry.key}`;
  return { color: `/game/ground-variant-tiles/${entry.file}`, normal: `${root}/normal.png`, rough: `${root}/rough.png`, height: `${root}/relief.json` };
}

/** New Engine2-generated, seamless Blender PBR tiles. City is a dedicated plains brush group. */
export const ENGINE2_V3_GROUND_VARIANTS = [
  { key: "flame", terrain: "flame", variant: 5, folder: "flame", label: "Chama" },
  { key: "city", terrain: "plains", variant: 50, folder: "city", label: "City" },
  { key: "hill", terrain: "hill", variant: 6, folder: "hill", label: "Colina" },
  { key: "nave", terrain: "nave", variant: 20, folder: "nave", label: "Laje" },
  { key: "door", terrain: "door", variant: 2, folder: "door", label: "Porta trancada" },
] as const;

export type Engine2V3GroundKey = (typeof ENGINE2_V3_GROUND_VARIANTS)[number]["key"];
export const ENGINE2_V3_CITY_VARIANT = ENGINE2_V3_GROUND_VARIANTS.find((entry) => entry.key === "city")!.variant;

/** Seamless authored ground variants use the terrain's 3D material layer even when they do not
 * have a dedicated PBR layer of their own. This keeps older map variants visible in the 3D
 * battlefield while preserving their saved indices and map data. */
export function isGroundTileVariant(id: TerrainId, variant: number): boolean {
  return id !== "column" && (
    ENGINE2_V3_GROUND_VARIANTS.some((entry) => entry.terrain === id && entry.variant === variant) ||
    GROUND_VARIANT_LAYERS.some((entry) => entry.terrain === id && entry.variant === variant) ||
    HEX_GROUND_001[id]?.variant === variant ||
    (id === "woods" && (variant === 10 || variant === 11)) ||
    (id === "water" && variant === 23) ||
    (id === "plains" && variant >= 39 && variant <= 47) ||
    (id === "snow" && variant >= 16 && variant <= 20) ||
    (id === "nave" && variant >= 3 && variant <= 18)
  );
}

/** Claude's baked tile per terrain (public/game/ground-claude/<terrain>/): appended after every
 * existing index, so saved maps keep their tiles; painted per hex from the editor's Versões. */
export const CLAUDE_VARIANT: Partial<Record<TerrainId, number>> = {
  plains: 49, woods: 12, ruins: 8, ember: 6, hill: 5, flame: 4,
  column: 3, nave: 19, barricade: 1, door: 1, snow: 21,
};

/** Append-only terrain set: previous saved-map indices keep their exact artwork. */
export const HEX_GROUND_001: Partial<Record<TerrainId, { variant: number; file: string }>> = {
  water: { variant: 22, file: "hex-ground-001-agua" },
  woods: { variant: 9, file: "hex-ground-001-bosque" },
  ember: { variant: 5, file: "hex-ground-001-brasa" },
  flame: { variant: 3, file: "hex-ground-001-chama" },
  plains: { variant: 38, file: "hex-ground-001-city" },
  hill: { variant: 4, file: "hex-ground-001-colina" },
  column: { variant: 2, file: "hex-ground-001-coluna" },
  nave: { variant: 2, file: "hex-ground-001-laje" },
  ruins: { variant: 7, file: "hex-ground-001-ruinas" },
  snow: { variant: 15, file: "hex-ground-001-neve" },
};

/** The art file a tile variant paints with, without path or cache-buster — "woods002".
 * Two variants of the same terrain differ only in art, so this is the only way to tell
 * from a painted map which of them a cell is actually using. */
export function tileVariantName(id: TerrainId, variant: number): string {
  const addedGround = GROUND_VARIANT_LAYERS.find((entry) => entry.terrain === id && entry.variant === variant);
  if (addedGround) return addedGround.label;
  const engine2V3 = ENGINE2_V3_GROUND_VARIANTS.find((entry) => entry.terrain === id && entry.variant === variant);
  if (engine2V3) return `engine2-v3-${engine2V3.folder}`;
  if (CLAUDE_VARIANT[id] === variant) return `claude-${id}`;
  if (id === "plains" && variant === 48) return "plains049";
  if (id === "plains" && variant === 46) return "hex-ground-013-farm-soil";
  if (id === "plains" && variant === 47) return "hex-ground-006-cave-earth";
  if (id === "nave" && variant === 18) return "hex-ground-013-farm-planks";
  if (id === "woods" && (variant === 10 || variant === 11)) return `hex-ground-011-forest-${variant === 10 ? "dark" : "needles"}`;
  if (id === "snow" && variant >= 18 && variant <= 20) return `hex-ground-010-snow-${["wind", "crust", "powder"][variant - 18]}`;
  if (id === "plains" && variant >= 43 && variant <= 47) return `hex-ground-009-${["plains", "meadow", "prairie"][variant - 43]}-photographic`;
  if (id === "nave" && variant >= 15 && variant <= 17) return `hex-ground-006-cave-${["earth", "cracked", "moss"][variant - 15]}`;
  if (id === "water" && variant === 23) return "hex-ground-006-cave-water";
  if (id === "nave" && variant === 13) return "hex-ground-004-cave-cracked";
  if (id === "nave" && variant === 14) return "hex-ground-004-cave-moss";
  if (id === "nave" && variant === 12) return "hex-ground-004-cave-basalt";
  if (id === "nave" && variant === 10) return "hex-ground-003-cave-slate";
  if (id === "nave" && variant === 11) return "hex-ground-003-cave-scree";
  if (id === "nave" && variant === 9) return "hex-ground-002-cave-crystals";
  if (id === "nave" && variant >= 3 && variant <= 8) {
    return `hex-ground-001-${["temple-limestone", "temple-basalt", "dungeon-flagstone", "dungeon-brick", "cave", "cave-crystals"][variant - 3]}`;
  }
  if (id === "plains" && variant === 41) return "hex-ground-001-high-grass";
  if (id === "plains" && variant === 42) return "hex-ground-001-dark-plains";
  if (id === "snow" && variant === 16) return "hex-ground-001-tundra";
  if (id === "snow" && variant === 17) return "hex-ground-001-tundra-snow";
  if (id === "plains" && variant === 39) return "hex-ground-001-planicie";
  if (id === "plains" && variant === 40) return "hex-ground-001-madeira";
  const ground = HEX_GROUND_001[id];
  if (ground && variant === ground.variant) return ground.file;
  // New ground materials are inserted ahead of the legacy plains without renaming
  // their on-disk files, so saved maps keep their original art available.
  if (id === "plains") {
    if (variant === 0) return "plains016";
    if (variant === 1) return "plains015";
    if (variant === 2) return "plains001";
    if (variant === 15) return "plains018";
    // Keep the old saved-map index valid while retiring that City tile.
    if (variant === 22) return "plains024";
    // 16-20 are the five existing ground variants; 21-37 are the City indices, with 22 retired.
    // These ranges continue the numbered art files at plains019.
    if (variant >= 16 && variant <= 37) return `plains${String(variant + 3).padStart(3, "0")}`;
    return `plains${String(variant).padStart(3, "0")}`;
  }
  if (id === "water" && variant === 0) return "water023";
  if (id === "woods") {
    if (variant === 0) return "woods005";
    if (variant === 1) return "woods006";
    if (variant === 6) return "woods007";
    if (variant === 7) return "woods009";
    if (variant === 8) return "woods010";
    return `woods${String(variant - 1).padStart(3, "0")}`;
  }
  if (id === "hill") return `hill${String(variant + 4).padStart(3, "0")}`;
  if (id === "ruins") {
    if (variant === 0) return "ruins005";
    if (variant <= 4) return `ruins${String(variant).padStart(3, "0")}`;
    return `ruins${String(variant + 1).padStart(3, "0")}`;
  }
  return `${id}${String(variant + 1).padStart(3, "0")}`;
}

export function isHexGroundVariant(id: TerrainId, variant: number): boolean {
  return id !== "column" && (ENGINE2_V3_GROUND_VARIANTS.some((entry) => entry.terrain === id && entry.variant === variant) || GROUND_VARIANT_LAYERS.some((entry) => entry.terrain === id && entry.variant === variant) || (id === "woods" && (variant === 10 || variant === 11)) || (id === "water" && variant === 23) || HEX_GROUND_001[id]?.variant === variant || (id === "plains" && variant >= 39 && variant <= 47) || (id === "snow" && variant >= 16 && variant <= 20) || (id === "nave" && variant >= 3 && variant <= 18));
}

/** Tile-picker previews fall back to the closest photoreal 3D surface when a legacy per-variant
 * image was removed with the retired 2D renderer. */
export function tileVariantSrc(id: TerrainId, variant: number): string {
  const addedGround = GROUND_VARIANT_LAYERS.find((entry) => entry.terrain === id && entry.variant === variant);
  if (addedGround) return groundVariantMaps(addedGround).color;
  const engine2V3 = ENGINE2_V3_GROUND_VARIANTS.find((entry) => entry.terrain === id && entry.variant === variant);
  if (engine2V3) return `/game/ground-engine2-v3/${engine2V3.folder}/color.png`;
  if (id === 'plains' && variant === 48) return '/game/ground-claude/049-plains/color.png';
  if (CLAUDE_VARIANT[id] === variant) return `/game/ground-claude/${id === 'plains' ? 'plains-v2' : id}/color.png`;
  if (id === 'plains' && variant >= 21 && variant <= 38 && variant !== 22) return '/game/ground-engine2-v4/city/color.png';
  const root = id === 'snow' || id === 'barricade' ? 'ground-claude' : 'ground-engine2-v4';
  return `/game/${root}/${id}/color.png`;
}
