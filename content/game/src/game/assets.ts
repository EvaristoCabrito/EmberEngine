import { DECORATIONS, decorationImage, decorationImageWebp, decorationSideFile } from "./data";
import type { GameArt, SpriteId, TerrainId } from "./types";
import { ENCOUNTER_NPC_IDS } from "./encounter-npcs";
import enemyPortraitList from "./spritePortraits.json";
import { ENGINE2_V3_GROUND_VARIANTS, GROUND_VARIANT_LAYERS, TILE_VARIANT_COUNT as EMBER_TILE_VARIANT_COUNT, tileVariantSrc as editorTileVariantSrc } from "../ember/tileVariants";

// Number of art variants available per terrain, e.g. plains001.png / plains002.png.
// Index 0 (the "001" file) is what every mission renders with unless it names a
// different variant in Mission.tileVariants — keep it as the tile that's safe
// for existing maps.
export const TILE_VARIANT_COUNT = EMBER_TILE_VARIANT_COUNT;

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
  return id !== "column" && (GROUND_VARIANT_LAYERS.some((entry) => entry.terrain === id && entry.variant === variant) || ENGINE2_V3_GROUND_VARIANTS.some((entry) => entry.terrain === id && entry.variant === variant) || (id === "woods" && (variant === 10 || variant === 11)) || (id === "water" && variant === 23) || HEX_GROUND_001[id]?.variant === variant || (id === "plains" && variant >= 39 && variant <= 47) || (id === "snow" && variant >= 16 && variant <= 20) || (id === "nave" && variant >= 3 && variant <= 18));
}

export function tileVariantSrc(id: TerrainId, variant: number): string {
  return `${editorTileVariantSrc(id, variant)}?v=67`;
}

/** Framed portrait art for the sprites that have one; every other sprite falls back to its
 * own first battle-frame, unframed. */
const HERO_PORTRAIT: Partial<Record<string, string>> = {
  defaultWarrior: "/game/portraits/kael.png?v=2",
  kaelFinal: "/game/portraits/kael-final-face-001.jpg?v=1",
  neera: "/game/portraits/NeeraProperSide-profile.jpg",
  // Voss V2: a 2:3 bust cut from Desktop/Voss/vossMovie.jpg (the front view)
  voss: "/game/portraits/voss-v2-bust.jpg?v=1",
  // BossVoss (and the generic Mage) wear the original Voss art: his portrait goes with it.
  BossVoss: "/game/portraits/voss.png",
  // A 2:3 bust cut from salazar-v2-face.jpg (a near-square waist-up shot that every frame showed too far away).
  salazar: "/game/portraits/salazar-v2-bust.jpg?v=2",
  aldric: "/game/portraits/aldric-profile-001.jpg?v=3",
  defaultLancer: "/game/portraits/aldric-profile-001.jpg?v=3",
  sandoval: "/game/portraits/sandoval-001.jpg?v=1",
  conjurer: "/game/portraits/conjurer-002.png?v=2",
  // Malrec is the named Conjurer hero: always use his face portrait, never a battle sprite frame.
  malrec: "/game/portraits/conjurer-002.png?v=2",
  theButcher: "/game/portraits/the-butcher-portrait-001.jpg?v=1",
  // Familiar Titã uses a direct crop of the player's supplied concept art; no art is generated.
  familiar3: "/game/portraits/familiar3-profile.jpeg?v=3",
  // Familiar (1): a 2:3 bust cut from the player's supplied art (Attachments/frame_028.png).
  familiar: "/game/portraits/familiar1-profile.png?v=4",
  // Inn and village NPCs. Keyed by SPRITE, so every place a unit or dialog line shows a face
  // (dialogs, the unit inspect popup, the footer portrait, the map party list) picks the right
  // portrait from the NPC's sprite automatically; nothing per-NPC has to be wired again.
  // Files keep the names they were supplied with. Brue, Mudo and A Hóspede use the Adega photos.
  brue: "/game/portraits/brue.png",
  mudinho: "/game/portraits/mudo.png",
  crazyLady: "/game/portraits/porao.png",
  beberrao: "/game/portraits/c471772d-7d79-46bf-be64-77a6b662b929.jpg", // Regular
  shadyPatron: "/game/portraits/1beca9c9-789c-4b44-8930-c53b88c982ac.jpg", // O Encapuzado
  soupLady: "/game/portraits/101b6fe6-9fc2-4416-b8c7-642d3699c4f4.jpg", // A Velha
  peasant1: "/game/portraits/d8def2e7-59bb-42cc-8a8e-36c7df1c22fb.jpg", // Lavrador
  breadLady: "/game/portraits/738c99c0-0bfa-472c-ab94-d16b91d2b2d8.jpg", // Padeira
  oldHealer: "/game/portraits/29ef0f7e-4b87-4c0d-a55f-f16fb5a6a1ea.jpg", // Ancião
  woodsman: "/game/portraits/9c0b071f-d203-4ef3-9c46-e4dac49dd975.jpg", // Lenhador
  villagerF1: "/game/portraits/1d7928dc-3fec-4d11-92d3-bd7a2d1637fb.jpg", // Moça
};

/** The one place the portrait-or-sprite-frame fallback lives — used by the unit inspect
 * popup, the footer portrait button, and DialogOverlay. Every "framed" portrait renders in
 * the same fixed box via object-cover, so the frame is always the same size regardless of
 * the source image's own dimensions. */
/** Enemies without hand-made art: a 2:3 head-and-shoulders cut of their own sprite on black
 * (tools/build-sprite-portraits.mjs writes the files and this list). */
const ENEMY_PORTRAITS = new Set<string>(enemyPortraitList);

export function portraitFor(sprite: SpriteId): { src: string; framed: boolean; position?: string } {
  const framed = HERO_PORTRAIT[sprite];
  return sprite === "travelingMerchant"
    ? { src: "/game/portraits/traveling-merchant.png", framed: false }
    : framed
      ? { src: framed, framed: true }
      : ENEMY_PORTRAITS.has(sprite)
        ? { src: `/game/portraits/sprites/${encodeURIComponent(sprite)}.png?v=1`, framed: true }
        : { src: `/game/sprites/${sprite}/1.png`, framed: false };
}

const TILES = Object.keys(TILE_VARIANT_COUNT) as TerrainId[];
const SPRITES: SpriteId[] = ["defaultWarrior", "neera", "voss", "salazar", "aldric", "malrec", "defaultLancer", "soldier", "brigand", "captain", "sorcerer", "horror", "minor-horror-001", "Asherah", "pikeman", "BasePikeman", "BaseCrossbowman", "BrigandCrossbowman", "StagWolf", "BossVoss", "wardog", "wardog2", "EmberedWraith", "troll", "troll2", "RoccoTheBird", "morvenian-wolf", "mordavian-wolf", "mordavian-wolf-final", "punisher", "theButcher", "birolho", "birolho2", "birolho3", "BirolhoLegs", "BirolhoLegs2", "familiar", "familiar2", "familiar3", "familiar4", "zombieDog", "zombie", "zombie2", "undeadOx", "plague-bearing-cattle", "swamp-blue-calf", "cobalt-blue-deer", "big-blue-ox-002", "ancient-golem", "lancer", "sandoval", "kaelFinal", "kaelEarly", "conjurer", "cultist-v2", "militia-v2", "apparition", "jacare", "archerRecruit", "mageRecruit", "healerRecruit", "beberrao", "breadLady", "brue", "crazyLady", "mudinho", "oldHealer", "peasant1", "shadyPatron", "soupLady", "villagerF1", "woodsman", "travelingMerchant"];

SPRITES.push(...ENCOUNTER_NPC_IDS);

// Real load progress for the title screen's loading bar: every image request counts once when it
// is asked for and once when it settles (loaded or failed). loadGameArt requests its batches one
// after another, so "requested so far" alone would reach ~100% after the first batch; the ratio is
// taken against the whole expected total instead — the count the last full load actually made
// (remembered in localStorage), or ART_TOTAL_FALLBACK (measured) on a first-ever visit.
const ART_TOTAL_KEY = "ember.artLoadTotal";
const ART_TOTAL_FALLBACK = 1184;
function rememberedArtTotal(): number {
  try {
    const n = Number(localStorage.getItem(ART_TOTAL_KEY));
    return Number.isFinite(n) && n > 0 ? n : ART_TOTAL_FALLBACK;
  } catch {
    return ART_TOTAL_FALLBACK;
  }
}
const artExpected = rememberedArtTotal();
let artRequested = 0;
let artSettled = 0;
const artListeners = new Set<() => void>();
function artChanged(): void {
  for (const listener of artListeners) listener();
}
export function subscribeArtProgress(listener: () => void): () => void {
  artListeners.add(listener);
  return () => {
    artListeners.delete(listener);
  };
}
/** 0..1 share of the whole expected image load that has settled so far. */
export function artProgress(): number {
  return artSettled / Math.max(artRequested, artExpected);
}

const LOAD_POOL = 8;
let loadActive = 0;
const loadWait: (() => void)[] = [];

function acquireLoad(): Promise<void> {
  if (loadActive < LOAD_POOL) {
    loadActive++;
    return Promise.resolve();
  }
  return new Promise((resolve) => loadWait.push(() => {
    loadActive++;
    resolve();
  }));
}

function releaseLoad(): void {
  loadActive--;
  const next = loadWait.shift();
  if (next) next();
}

function spriteFrameSrc(id: SpriteId, frame: string, cacheBust = ""): string {
  // Conjurer's active art is kept as a complete, source-preserved serial. Talk drives idle; the former Idle sheet drives casting.
  const directory = id === "minor-horror-001" ? "minor-horror-003" : id === "big-blue-ox-002" ? "big-blue-ox-ai-006" : id === "conjurer" ? "conjurer/conjurer-complete-003" : id === "sandoval" ? "sandoval/sandoval-complete-001" : id === "kaelFinal" ? "Kael_Final/kael-final-002" : id === "kaelEarly" ? "kael" : id === "defaultWarrior" ? "kael-v2" : id === "salazar" ? "salazar-final" : id === "voss" ? "voss-final" : id === "BossVoss" ? "voss" : id;
  if (id === "neera" && /^(?:\d+|idle-\d+|atk-(?:left-)?\d+|atk-short-\d+|atk2-(?:left-)?\d+|move-(?:left-)?\d+)$/.test(frame)) {
    const currentFrame = /^\d+$/.test(frame) ? `idle-${frame}` : frame;
    // Every frame here was rebuilt in place from Attachments/Nerra V2 (feet on one ground line,
    // centred, same body size per pose), so they carry their own version.
    return `/game/sprites/neera/neera-v2-001/${currentFrame}.png?v=neera-v2-004`;
  }
  return `/game/sprites/${directory}/${frame}.png${cacheBust}`;
}
function loadImage(src: string): Promise<HTMLImageElement> {
  artRequested++;
  artChanged();
  return acquireLoad().then(
    () =>
      new Promise<HTMLImageElement>((resolve, reject) => {
        const img = new Image();
        img.crossOrigin = "anonymous";
        const fail = () => reject(new Error(`Falha ao carregar ${src}`));
        let settled = false;
        const t = window.setTimeout(() => {
          done();
          fail();
        }, 20000);
        const done = () => {
          if (settled) return;
          settled = true;
          window.clearTimeout(t);
          releaseLoad();
          artSettled++;
          artChanged();
        };
        img.onload = () => {
          done();
          resolve(img);
        };
        img.onerror = () => {
          done();
          fail();
        };
        img.src = src;
      }),
  );
}

// Sprites cut as a 12-frame idle rather than the 4-frame default — the heroes, the two
// big horrors, and the creatures cut from reference video (familiar, familiar2, ancient
// golem). loadGameArt rejects on any missing file, so this set and what is on disk have to
// move together.
const HERO_IDLE = new Set<SpriteId>(["defaultWarrior", "neera", "voss", "BossVoss", "salazar", "aldric", "defaultLancer", "horror", "Asherah", "familiar", "familiar2", "ancient-golem", "lancer", "sandoval", "kaelFinal", "kaelEarly", "conjurer", "malrec", "archerRecruit", "mageRecruit", "healerRecruit"]);

/** arrow-002.png is a moody product photo shot on black with no alpha channel; it was
 * originally drawn with a screen/lighter blend to fake-hide that background, which only
 * works when composited straight onto opaque battlefield pixels. renderUnitsAndOverlays
 * draws projectiles onto their own transparent per-frame canvas (see BattleCanvas), so
 * blending against nothing just paints a solid near-black square. Bake real alpha from
 * the image's own luminance once at load time so it composites correctly on any layer. */
function deriveAlphaFromBlack(img: HTMLImageElement): HTMLCanvasElement {
  const canvas = document.createElement("canvas");
  canvas.width = img.naturalWidth;
  canvas.height = img.naturalHeight;
  const c = canvas.getContext("2d")!;
  c.drawImage(img, 0, 0);
  const data = c.getImageData(0, 0, canvas.width, canvas.height);
  const px = data.data;
  for (let i = 0; i < px.length; i += 4) {
    px[i + 3] = Math.max(px[i], px[i + 1], px[i + 2]);
  }
  c.putImageData(data, 0, 0);
  return canvas;
}
// Attack cuts, per sprite: how many atk-*.png frames are on disk, and the cache-bust the
// set was last republished under. attackPose spreads whatever count it finds across the
// lunge/hit/recover stages, so a set only has to be listed here to animate.
const ATTACK_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  "minor-horror-001": { n: 36, bust: "?v=minor-horror-003" },
  "big-blue-ox-002": { n: 36, bust: "?v=big-blue-ox-ai-006" },
  "carnivorous-plant-001": { n: 36, bust: "" },
  "sapling-001": { n: 36, bust: "" },
  // Legacy Calf (swampBlueCalf) — right-facing frames cut from the user's video.
  // The renderer mirrors this pool for the opposite direction.
  "swamp-blue-calf": { n: 32, bust: "?v=big-blue-calf-001" },
  "cobalt-blue-deer": { n: 4, bust: "?v=cobalt-blue-deer-002" },
  // The generic/default warrior look (CLASSES.swordsman's own sprite), its own distinct
  // on-disk cut (kael-v2 — an old internal folder name, kept as-is on disk) — the MC
  // himself is a different unit entirely and plays as kaelFinal instead.
  defaultWarrior: { n: 12, bust: "?v=kael-v2" },
  kaelEarly: { n: 12, bust: "?v=kael-early" },
  neera: { n: 36, bust: "?v=neera-attack-002" },
  voss: { n: 64, bust: "?v=voss-v2-001" },
  BossVoss: { n: 4, bust: "" },
  // Salazar V2 (Attachments/Salazar, salazar-final/): every pose rescaled onto one shared canvas,
  // same body size and ground line. The old salazar/ cut lives on as healerRecruit (the Healer).
  // ATT is mirrored from the supplied sheet so, as drawn, he strikes to the right like every
  // other sprite (the renderer mirrors it for facing left).
  salazar: { n: 36, bust: "?v=salazar-atk-002" },
  // Generic-enemy "alter" sprites (see the SpriteId comment in types.ts) — same file
  // shape as the hero folder they started as a copy of, since they're literally that
  // copy for now.
  archerRecruit: { n: 4, bust: "" },
  mageRecruit: { n: 4, bust: "" },
  healerRecruit: { n: 4, bust: "" },
  aldric: { n: 36, bust: "?v=aldric-final-001" },
  // Malrec's own personally-named slot, seeded from a clean copy of the finished
  // conjurer 36-frame sheet (see HERO_SPRITE_BY_NAME in engine.ts) — never the old
  // broken "malrec" sheet, which was permanently deleted.
  malrec: { n: 36, bust: "" },
  defaultLancer: { n: 5, bust: "?v=sheet2" },
  familiar: { n: 8, bust: "?v=6" },
  // Familiar 2 — the crouch/lunge strike cut from reference video (see CAST_FRAMES and
  // WALK_FRAMES below for its casting and walk cuts).
  familiar2: { n: 36, bust: "?v=familiar2-36" },
  "ancient-golem": { n: 36, bust: "" },
  "morvenian-wolf": { n: 6, bust: "" },
  // Mordavian Wolf — the bigger cousin, sliced from its own reference sheet; the row
  // labeled "8 frames" only actually has 7 distinct poses (two columns share the "07" tag,
  // none reads "02"-"05" — a defect in that sheet's own generation, not a slicing choice).
  "mordavian-wolf": { n: 7, bust: "" },
  birolho: { n: 4, bust: "" },
  birolho2: { n: 4, bust: "" },
  // BirolhoLegs has no ATT footage of its own — atk-*.png is a copy of its cast cut.
  BirolhoLegs: { n: 36, bust: "" },
  BirolhoLegs2: { n: 36, bust: "" },
  troll2: { n: 36, bust: "" },
  RoccoTheBird: { n: 36, bust: "?v=f36" },
  EmberedWraith: { n: 36, bust: "?v=f36" },
  zombieDog: { n: 36, bust: "?v=f36" },
  wardog2: { n: 36, bust: "?v=f36" },
  // Zombie ATT: video 2 from 5 s, mirrored so the whole strike faces right (see its README).
  zombie: { n: 32, bust: "" },
  zombie2: { n: 10, bust: "" },
  // Undead Ox: head-down lunge from the Idle/ATT video (see its README).
  undeadOx: { n: 36, bust: "?v=ox-36" },
  "plague-bearing-cattle": { n: 36, bust: "?v=plague-cattle-001" },
  familiar4: { n: 36, bust: "" },
  "mordavian-wolf-final": { n: 36, bust: "" },
  punisher: { n: 4, bust: "" },
  // The Butcher — real 36-frame axe swing, a distinct unit/sprite from punisher/Carrasco
  // above (see WALK_FRAMES.theButcher below for the matching walk cut).
  theButcher: { n: 36, bust: "?v=the-butcher-001" },
  lancer: { n: 6, bust: "?v=3" },
  sandoval: { n: 6, bust: "?v=sandoval-complete-001" },
  // atk-*.png rebuilt from work/kael-atk-backup by work/kael_atk_build.py: the generated sheet
  // zoomed in ~13% and dropped his feet through the swing; every frame now keeps his idle size
  // and ground line.
  kaelFinal: { n: 36, bust: "?v=kael-final-005-steady" },
  conjurer: { n: 36, bust: "?v=conjurer-complete-003" },
  "cultist-v2": { n: 36, bust: "" },
  // Milícia V2's ATT, mirrored at cut time to face the same way as his idle (see
  // work/militia_v2_build.py); same shared canvas and scale as his other sheets.
  "militia-v2": { n: 36, bust: "" },
  BasePikeman: { n: 36, bust: "" },
  BaseCrossbowman: { n: 36, bust: "" },
  BrigandCrossbowman: { n: 36, bust: "" },
  StagWolf: { n: 36, bust: "" },
  // Apparition's ATT (TEK cut, work/apparition/build.py): 60 frames / 5 s, longer than the usual
  // 36 on purpose — see APPARITION_ATTACK_SECONDS in engine.ts. Faces right; mirrored for left.
  apparition: { n: 60, bust: "" },
  // Jacaré ATT (TEK cut, work/jacare/build.py): jaws open, lunge, roar. Its Special is ATTACK2_FRAMES.
  jacare: { n: 36, bust: "" },
  // Familiar 3's primary attack cut — see ATTACK2_FRAMES below for its alternate cut,
  // which Unit.idleAlt alternates into turn to turn (same flip as Malrec's idles2).
  familiar3: { n: 36, bust: "" },
};

// Cast pose: cast-*.png, same shape as the attack table — a sprite absent from here falls
// back to its attacks cut (the melee swing) for a spell just like it always did before this
// existed.
const CAST_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  voss: { n: 64, bust: "?v=voss-v2-001" },
  StagWolf: { n: 36, bust: "" },
  "minor-horror-001": { n: 36, bust: "?v=minor-horror-003" },
  "carnivorous-plant-001": { n: 36, bust: "" },
  "sapling-001": { n: 36, bust: "" },
  birolho: { n: 3, bust: "" },
  birolho2: { n: 3, bust: "" },
  birolho3: { n: 18, bust: "" },
  // 32 authored cast frames + 12 closing frames (the unfurl played in reverse) so he folds
  // his arms back in after the charge, the same way his ATT ends.
  BirolhoLegs: { n: 48, bust: "" },
  BirolhoLegs2: { n: 36, bust: "" },
  // Neera's supplied cast pose remains available for non-arrow spells. Her old ATT is the
  // placeholder animation for longShot, multiShot and piercing (see ATTACK2_FRAMES).
  neera: { n: 36, bust: "" },
  // The spell cast intentionally uses the former Idle sheet; ATT remains the physical attack.
  conjurer: { n: 36, bust: "?v=conjurer-complete-003" },
  // Malrec's own cast-*.png — a copy of conjurer's same dedicated cast sequence.
  malrec: { n: 36, bust: "" },
  // Salazar V2's Casting sheet: damage spells only. Every other spell plays HEAL_CAST_FRAMES.
  salazar: { n: 36, bust: "" },
  // Aldric's dedicated skill pose — plays only for his spell-typed pike skills (Piercing
  // Thrust, Sweep, ...), never for a plain attack, which stays on the ATT cut.
  aldric: { n: 36, bust: "?v=aldric-final-001" },
  "cultist-v2": { n: 36, bust: "" },
  // Apparition: hands together, energy orb builds (TEK cut, work/apparition/build.py).
  apparition: { n: 36, bust: "" },
  // Familiar 2's real rear-up/charge/beam-release windup — a distinct animation from its
  // ATT cut (the crouch/lunge), not a fallback.
  familiar2: { n: 36, bust: "?v=familiar2-36" },
  RoccoTheBird: { n: 36, bust: "?v=f36" },
  EmberedWraith: { n: 36, bust: "?v=f36" },
  zombieDog: { n: 36, bust: "?v=f36" },
  // Undead Ox: green rib-glow and breath, played for its Veneno Cáustico.
  undeadOx: { n: 36, bust: "?v=ox-36" },
  "plague-bearing-cattle": { n: 36, bust: "?v=plague-cattle-001" },
  "cobalt-blue-deer": { n: 4, bust: "?v=cobalt-blue-deer-002" },
  // Familiar 3's spellcasting windup (cast-*.png) — plays for its Fireball cast only
  // (attackPose falls back to `attacks` for a plain melee swing); see ATTACK_FRAMES/
  // ATTACK2_FRAMES above for its two melee attack cuts.
  familiar3: { n: 36, bust: "" },
};

// Healing pose: heal-*.png, played instead of the cast pose for spells that deal no damage
// (see GameArt.castsHeal).
const HEAL_CAST_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  salazar: { n: 36, bust: "" },
};

// Familiar 3's second, distinct attack cut (atk2-*.png) — the first case of a class having
// more than one attack cut, same "second pool, same idleAlt flip" shape as Malrec's idles2
// but for the attack pool instead of idle (see GameArt.attacks2's doc comment).
const ATTACK2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  // Jacaré's Special (TEK cut): jaws open, tail whips up — alternates with its ATT turn to turn.
  jacare: { n: 36, bust: "" },
  "big-blue-ox-002": { n: 36, bust: "?v=big-blue-ox-ai-006" },
  familiar3: { n: 36, bust: "" },
  // Neera V2's labeled Special sheet is reserved for arrow skills.
  neera: { n: 36, bust: "?v=neera-special-001" },
};

// Short-range (off-hand dagger/katar) attack cut — see GameArt.attacksShort.
const ATTACK_SHORT_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  neera: { n: 36, bust: "" },
};

// Left-facing counterpart to a handful of the CAST_FRAMES cuts above — same idea as
// attacksLeft/walksLeft: a sprite here skips the mirrored flip and plays this set instead
// when facing === -1.
const CAST_DIR_LEFT: SpriteId[] = ["aldric"];

// Counter pose: counter-*.png, same shape as the attack table — a sprite absent from here
// falls back to its attacks cut (the same swing used for a normal attack) for the
// defender's counter stages, same as every sprite did before this existed.
const COUNTER_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  voss: { n: 64, bust: "?v=voss-v2-001" },
  theButcher: { n: 36, bust: "?v=the-butcher-counter-001" },
};

// Walk cycles: move-*.png, same shape as the attack table. A sprite absent from here has
// no walk cut and falls back to its idle loop played faster, as every sprite used to.
const WALK_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  "minor-horror-001": { n: 36, bust: "?v=minor-horror-003" },
  "sapling-001": { n: 36, bust: "" },
  "big-blue-ox-002": { n: 36, bust: "?v=big-blue-ox-ai-006" },
  "swamp-blue-calf": { n: 32, bust: "?v=big-blue-calf-001" },
  "cobalt-blue-deer": { n: 4, bust: "?v=cobalt-blue-deer-002" },
  familiar: { n: 8, bust: "?v=6" },
  // Right-facing cut; see the dedicated walksLeft.familiar2 load below for its own
  // authored left-facing cut (real distinct footage, not the CSS mirror every other
  // sprite absent from walksLeft falls back to).
  familiar2: { n: 36, bust: "?v=familiar2-36" },
  // Right-facing cut (Nerra V2 Walk Right, move-*.png); her authored Walk Left plays from
  // walksLeft (move-left-*.png, loaded below) — neither is mirrored while she walks (see
  // dirActionWalk in engine.ts).
  neera: { n: 36, bust: "" },
  "ancient-golem": { n: 36, bust: "" },
  aldric: { n: 36, bust: "?v=aldric-final-001" },
  defaultLancer: { n: 6, bust: "?v=sheet2" },
  lancer: { n: 6, bust: "?v=3" },
  sandoval: { n: 6, bust: "?v=sandoval-complete-001" },
  // One authored right-facing walk. The renderer mirrors it for left-facing movement.
  // defaultWarrior (the generic/default warrior look) has no move-*.png cut of its own —
  // it falls back to its idle loop played faster, like every sprite absent from this table.
  kaelFinal: { n: 36, bust: "?v=kael-final-002" },
  conjurer: { n: 36, bust: "?v=conjurer-complete-003" },
  // Real authored Walk Right footage replaced the old cut here — bumped so browsers
  // holding the old move-*.png in cache actually fetch the new art (the old cut lives on
  // as idle2-*.png, his alternate-turn idle; see idles2.malrec above). Mirrored via the
  // regular CSS flip for left-facing movement, same as any sprite with one authored
  // direction — see the walksLeft.malrec comment below for why.
  malrec: { n: 36, bust: "?v=malrec-walk-002" },
  // Salazar V2's Walk Right; his authored Walk Left loads as walksLeft below.
  salazar: { n: 36, bust: "" },
  birolho3: { n: 12, bust: "" },
  // Walk Left footage only; move-*.png is its mirror, and the renderer mirrors this pool
  // for left-facing movement like any sprite with one authored direction.
  BirolhoLegs: { n: 36, bust: "" },
  BirolhoLegs2: { n: 36, bust: "" },
  // Walk Left footage only; move-*.png is its mirror, same as BirolhoLegs above.
  troll2: { n: 36, bust: "" },
  RoccoTheBird: { n: 36, bust: "?v=f36" },
  EmberedWraith: { n: 36, bust: "?v=f36" },
  zombieDog: { n: 36, bust: "?v=f36" },
  // Walk Left footage only; move-*.png is its mirror, same as troll2.
  wardog2: { n: 36, bust: "?v=f36" },
  // Zombie: side-on walk from video 2 (2.05-3.9 s, two real strides), 32 frames like every
  // other long sheet; move-*.png is its mirror and the renderer mirrors it back for
  // left-facing movement.
  zombie: { n: 32, bust: "" },
  zombie2: { n: 12, bust: "" },
  // Undead Ox: right-facing walk; the renderer mirrors it for leftward travel.
  undeadOx: { n: 36, bust: "?v=ox-36" },
  "plague-bearing-cattle": { n: 36, bust: "?v=plague-cattle-001" },
  // Right-facing dash (the video has no walk loop); the renderer mirrors it for leftward travel.
  familiar4: { n: 36, bust: "" },
  // Mordavian Wolf Final: right-facing walk (mirrored from the Walk Left footage); the
  // renderer mirrors it back for leftward travel.
  "mordavian-wolf-final": { n: 36, bust: "" },
  // Right-facing cut; see the dedicated walksLeft.theButcher load below for its own
  // authored left-facing cut (not the CSS mirror every other sprite here falls back to).
  theButcher: { n: 36, bust: "?v=the-butcher-001" },
  // Right-facing cut; see the dedicated walksLeft["cultist-v2"] load below.
  "cultist-v2": { n: 36, bust: "" },
  // Right-facing cut (Walk Right); his authored Walk Left loads as walksLeft below.
  "militia-v2": { n: 36, bust: "" },
  BasePikeman: { n: 36, bust: "" },
  BaseCrossbowman: { n: 36, bust: "" },
  BrigandCrossbowman: { n: 36, bust: "" },
  StagWolf: { n: 36, bust: "" },
  voss: { n: 64, bust: "?v=voss-v2-001" },
  // Apparition: one right-facing profile walk (TEK cut); the renderer mirrors it for leftward travel.
  apparition: { n: 36, bust: "" },
  // Jacaré: one right-facing walk (TEK cut); the renderer mirrors it for leftward travel.
  jacare: { n: 36, bust: "" },
  familiar3: { n: 36, bust: "" },
  // Right-facing cut, sliced from the Mordavian Puppy reference sheet's WALK RIGHT row;
  // see the dedicated walksLeft["morvenian-wolf"] load below for its own authored
  // left-facing cut (real distinct footage, 7 frames vs this row's 6 — not the CSS mirror
  // every other sprite absent from walksLeft falls back to).
  "morvenian-wolf": { n: 6, bust: "" },
  // Mordavian Wolf — one authored cut (its sheet has no separate left row like the
  // Puppy's); the renderer mirrors it for left-facing movement, same as most sprites here.
  "mordavian-wolf": { n: 8, bust: "" },
};

const DIR_LEFT: SpriteId[] = ["aldric", "defaultLancer", "lancer", "sandoval"];

// Hit-reaction sheets: hit-*.png, played whenever the unit takes damage (see GameArt.hits).
const HIT_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  "big-blue-ox-002": { n: 36, bust: "?v=big-blue-ox-ai-006" },
  "carnivorous-plant-001": { n: 36, bust: "" },
  "sapling-001": { n: 36, bust: "" },
  "mordavian-wolf-final": { n: 32, bust: "" },
  undeadOx: { n: 36, bust: "?v=ox-36" },
  // 528x321, wider than its other sheets — see computeUnitVisual's zombieDogWideSheet.
  zombieDog: { n: 36, bust: "?v=zd-hit-death-36" },
  "militia-v2": { n: 36, bust: "" },
  BasePikeman: { n: 20, bust: "" },
  BaseCrossbowman: { n: 22, bust: "" },
  BrigandCrossbowman: { n: 20, bust: "" },
  StagWolf: { n: 20, bust: "" },
  // Apparition hit react 1 (TEK cut, work/apparition/build.py); her react 2 is HIT2_FRAMES below.
  apparition: { n: 36, bust: "" },
  jacare: { n: 36, bust: "" },
};

// Second hit-reaction sheets: hit2-*.png, cycled hit, hit, hit2 (see GameArt.hits2).
const HIT2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  apparition: { n: 36, bust: "" },
};

// Death sheets: death-*.png, played once when the unit dies (see GameArt.deaths).
const DEATH_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  "minor-horror-001": { n: 36, bust: "?v=minor-horror-003" },
  "big-blue-ox-002": { n: 36, bust: "?v=big-blue-ox-ai-006" },
  "carnivorous-plant-001": { n: 36, bust: "" },
  "sapling-001": { n: 36, bust: "" },
  "swamp-blue-calf": { n: 32, bust: "?v=big-blue-calf-001" },
  "cobalt-blue-deer": { n: 4, bust: "?v=cobalt-blue-deer-002" },
  "mordavian-wolf-final": { n: 32, bust: "" },
  wardog2: { n: 36, bust: "?v=f36" },
  EmberedWraith: { n: 36, bust: "?v=f36" },
  // New 528x321 cut (same file names as the old one, hence the bust); the old cut lives on
  // as death2-*.png below.
  zombieDog: { n: 36, bust: "?v=zd-hit-death-36" },
  undeadOx: { n: 36, bust: "?v=ox-36" },
  "plague-bearing-cattle": { n: 36, bust: "?v=plague-cattle-001" },
  "militia-v2": { n: 36, bust: "" },
  BasePikeman: { n: 36, bust: "" },
  BaseCrossbowman: { n: 36, bust: "" },
  BrigandCrossbowman: { n: 36, bust: "" },
  StagWolf: { n: 36, bust: "" },
  apparition: { n: 36, bust: "" },
  jacare: { n: 36, bust: "" },
};

// Alternate death sheets: death2-*.png, played about one death in three (see GameArt.deaths2).
const DEATH2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  zombieDog: { n: 36, bust: "?v=f36" },
};

// Up/down walk cycles (move-up-*.png / move-down-*.png), opt-in per sprite — see
// GameArt.walksUp/walksDown. A sprite with only one of the two keeps its left/right walk
// for the other direction.
const WALK_UP_DOWN_FRAMES: Partial<Record<SpriteId, { up?: number; down?: number; bust: string }>> = {
  BasePikeman: { down: 36, bust: "" },
  BirolhoLegs: { up: 36, down: 36, bust: "" },
  "mordavian-wolf-final": { up: 36, down: 36, bust: "" },
  troll2: { up: 36, down: 36, bust: "" },
};

// Cosmetic alternate walk (see GameArt.walks2): its own right- and left-facing cuts.
const WALK2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  familiar3: { n: 36, bust: "" },
};

type SpritePoolKey = "hits" | "hits2" | "deaths" | "deaths2" | "sprites" | "attacks" | "attacks2" | "attacksShort" | "attacksLeft" | "attacks2Left" | "casts" | "castsLeft" | "castsHeal" | "counters" | "countersLeft" | "walks" | "walksLeft" | "idles" | "idles2" | "walkDirs" | "walksUp" | "walksDown" | "walks2" | "walksLeft2";
const SPRITE_POOL_KEYS: SpritePoolKey[] = ["hits", "hits2", "deaths", "deaths2", "sprites", "attacks", "attacks2", "attacksShort", "attacksLeft", "attacks2Left", "casts", "castsLeft", "castsHeal", "counters", "countersLeft", "walks", "walksLeft", "idles", "idles2", "walkDirs", "walksUp", "walksDown", "walks2", "walksLeft2"];

/** Loads every pool one sprite contributes to GameArt (idle, attack, cast, walk, ...) — the
 * same files, frame counts and cache-busts loadGameArt used to load for every sprite up front. */
async function loadSpritePools(id: SpriteId): Promise<Partial<Record<SpritePoolKey, unknown>>> {
  const cut = (n: number, frame: (i: number) => string, bust: string) =>
    Promise.all(Array.from({ length: n }, (_, i) => loadImage(spriteFrameSrc(id, frame(i + 1), bust))));
  const pools: Partial<Record<SpritePoolKey, unknown>> = {};
  const jobs: Promise<void>[] = [];
  const put = (key: SpritePoolKey, job: Promise<unknown>) => {
    jobs.push(job.then((value) => {
      pools[key] = value;
    }));
  };
  const n = id === "RoccoTheBird" || id === "wardog2" || id === "EmberedWraith" || id === "zombieDog" ? 36 : id === "undeadOx" || id === "plague-bearing-cattle" ? 36 : id === "minor-horror-001" ? 36 : id === "big-blue-ox-002" ? 36 : id === "carnivorous-plant-001" ? 36 : id === "sapling-001" ? 36 : id === "zombie2" ? 11 : id === "neera" || id === "conjurer" || id === "kaelFinal" || id === "aldric" || id === "cultist-v2" || id === "militia-v2" || id === "BasePikeman" || id === "BaseCrossbowman" || id === "BrigandCrossbowman" || id === "StagWolf" || id === "apparition" || id === "jacare" || id === "malrec" || id === "salazar" || id === "familiar3" || id === "familiar2" ? 36 : id === "sandoval" || id === "mordavian-wolf" ? 8 : id === "birolho2" ? 18 : id === "birolho3" ? 12 : id === "zombie" ? 32 : id === "BirolhoLegs" || id === "BirolhoLegs2" || id === "troll2" || id === "ancient-golem" || id === "familiar4" || id === "mordavian-wolf-final" ? 36 : id === "voss" ? 64 : HERO_IDLE.has(id) ? 12 : 4;
  const cacheBust = id === "neera" ? "?v=neera-idle-001" : id === "RoccoTheBird" || id === "wardog2" || id === "EmberedWraith" || id === "zombieDog" ? "?v=f36" : id === "undeadOx" ? "?v=ox-36-tail2" : id === "plague-bearing-cattle" ? "?v=plague-cattle-001" : id === "minor-horror-001" ? "?v=minor-horror-003" : id === "big-blue-ox-002" ? "?v=big-blue-ox-ai-006" : id === "troll" ? "?v=11" : id === "Asherah" ? "?v=3" : id === "familiar" ? "?v=6" : id === "aldric" ? "?v=aldric-final-001" : id === "defaultLancer" ? "?v=sheet2" : id === "lancer" ? "?v=3" : id === "sandoval" ? "?v=sandoval-complete-001" : id === "kaelFinal" ? "?v=kael-final-002" : id === "kaelEarly" ? "?v=kael-early" : id === "defaultWarrior" ? "?v=kael-v2" : id === "conjurer" ? "?v=conjurer-complete-003" : id === "familiar2" ? "?v=familiar2-36" : "";
  put("sprites", cut(n, (i) => (id === "conjurer" ? `talk-${i}` : `${i}`), cacheBust));
  const atk = ATTACK_FRAMES[id];
  if (atk) put("attacks", cut(atk.n, (i) => `atk-${i}`, atk.bust));
  const atk2 = ATTACK2_FRAMES[id];
  if (atk2) put("attacks2", cut(atk2.n, (i) => `atk2-${i}`, atk2.bust));
  if (id === "neera" && atk2) put("attacks2Left", cut(atk2.n, (i) => `atk2-left-${i}`, atk2.bust));
  const short = ATTACK_SHORT_FRAMES[id];
  if (short) put("attacksShort", cut(short.n, (i) => `atk-short-${i}`, short.bust));
  const cast = CAST_FRAMES[id];
  if (cast) put("casts", cut(cast.n, (i) => (id === "conjurer" ? `${i}` : `cast-${i}`), cast.bust));
  if (cast && CAST_DIR_LEFT.includes(id)) put("castsLeft", cut(cast.n, (i) => `cast-left-${i}`, cast.bust));
  const healCast = HEAL_CAST_FRAMES[id];
  if (healCast) put("castsHeal", cut(healCast.n, (i) => `heal-${i}`, healCast.bust));
  const counter = COUNTER_FRAMES[id];
  if (counter) put("counters", cut(counter.n, (i) => `counter-${i}`, counter.bust));
  const hit = HIT_FRAMES[id];
  if (hit) put("hits", cut(hit.n, (i) => `hit-${i}`, hit.bust));
  const hit2 = HIT2_FRAMES[id];
  if (hit2) put("hits2", cut(hit2.n, (i) => `hit2-${i}`, hit2.bust));
  const death = DEATH_FRAMES[id];
  if (death) put("deaths", cut(death.n, (i) => `death-${i}`, death.bust));
  const death2 = DEATH2_FRAMES[id];
  if (death2) put("deaths2", cut(death2.n, (i) => `death2-${i}`, death2.bust));
  const walk = WALK_FRAMES[id];
  if (walk) put("walks", cut(walk.n, (i) => id === "cobalt-blue-deer" ? `move-left-${i}` : `move-${i}`, walk.bust));
  if (DIR_LEFT.includes(id)) {
    const walkN = WALK_FRAMES[id]?.n ?? 6;
    const atkN = ATTACK_FRAMES[id]?.n ?? 5;
    const bust = id === "lancer" ? "?v=3" : id === "sandoval" ? "?v=sandoval-complete-001" : id === "aldric" ? "?v=aldric-final-001" : "?v=sheet2";
    put("walksLeft", cut(walkN, (i) => `move-left-${i}`, bust));
    put("attacksLeft", cut(atkN, (i) => `atk-left-${i}`, bust));
  }
  // Neera's new attack sheet includes its own mirrored left-facing cuts. Keep her walk
  // pool mirrored normally; only regular ATT uses this authored left-facing set.
  if (id === "neera" && atk) put("attacksLeft", cut(atk.n, (i) => `atk-left-${i}`, atk.bust));
  if (id === "neera" && walk) put("walksLeft", cut(walk.n, (i) => `move-left-${i}`, walk.bust));
  // The Butcher, Cultist V2, Milícia V2, Familiar 2, Familiar 3 and Salazar V2 each have their own authored
  // left-facing walk cut (same frame count as their right-facing one) but no dedicated
  // left-facing attack cut — their attack keeps mirroring the right-facing pool.
  if (walk && (id === "voss" || id === "theButcher" || id === "cultist-v2" || id === "militia-v2" || id === "BasePikeman" || id === "BaseCrossbowman" || id === "BrigandCrossbowman" || id === "StagWolf" || id === "familiar2" || id === "familiar3" || id === "salazar")) put("walksLeft", cut(walk.n, (i) => `move-left-${i}`, walk.bust));
  if (id === "cobalt-blue-deer" && walk) put("walksLeft", cut(walk.n, (i) => `move-${i}`, walk.bust));
  // Mordavian Puppy's WALK LEFT row has 7 frames vs its right-facing row's 6.
  if (id === "morvenian-wolf") put("walksLeft", cut(7, (i) => `move-left-${i}`, ""));
  // Malrec's sliced move-left-*.png frames are deliberately unused: the render loop's usual
  // CSS mirror-flip of `walks.malrec` handles left-facing movement instead.
  // defaultWarrior (the generic/default warrior look) reads its own kael-v2 stand cut;
  // kaelEarly keeps the original kael-folder 36-frame stand cut. See CLASSES.swordsman vs
  // CLASSES.kaelEarly.
  if (id === "defaultWarrior") put("idles", Promise.all(Array.from({ length: 12 }, (_, i) => loadImage(`/game/sprites/kael-v2/stand-${i + 1}.png?v=kael-v2`))));
  if (id === "neera") put("idles", cut(36, (i) => `idle-${i}`, "?v=neera-idle-001"));
  if (id === "kaelEarly") put("idles", Promise.all(Array.from({ length: 36 }, (_, i) => loadImage(`/game/sprites/kael/stand-${i + 1}.png?v=kael-early`))));
  // Malrec's second idle loop: his original walk-right cut, kept on disk as idle2-*.png
  // (see GameArt.idles2 / Unit.idleAlt).
  if (id === "malrec") put("idles2", Promise.all(Array.from({ length: 36 }, (_, i) => loadImage(`/game/sprites/malrec/idle2-${i + 1}.png`))));
  const upDown = WALK_UP_DOWN_FRAMES[id];
  if (upDown?.up) put("walksUp", cut(upDown.up, (i) => `move-up-${i}`, upDown.bust));
  if (upDown?.down) put("walksDown", cut(upDown.down, (i) => `move-down-${i}`, upDown.bust));
  const walk2 = WALK2_FRAMES[id];
  if (walk2) {
    put("walks2", cut(walk2.n, (i) => `move2-${i}`, walk2.bust));
    put("walksLeft2", cut(walk2.n, (i) => `move2-left-${i}`, walk2.bust));
  }
  // walkDirs: kael-v2 has no walk-front/back/side cut, so defaultWarrior has no entry. The
  // Birolhos and the punisher have no side-on art (the idle/front frame stands in).
  // theButcher has real walk-cycle frames instead — walkDirs would take priority over them.
  const dirs = (front: string, back: string, side: string) =>
    Promise.all([loadImage(front), loadImage(back), loadImage(side)]).then(([f, b, s]) => ({ front: f, back: b, side: s }));
  if (id === "kaelEarly") put("walkDirs", dirs("/game/sprites/kael/walk-front.png?v=kael-early", "/game/sprites/kael/walk-back.png?v=kael-early", "/game/sprites/kael/walk-side.png?v=kael-early"));
  if (id === "birolho") put("walkDirs", dirs("/game/sprites/birolho/1.png", "/game/sprites/birolho/back.png", "/game/sprites/birolho/1.png"));
  if (id === "birolho2") put("walkDirs", dirs("/game/sprites/birolho2/1.png", "/game/sprites/birolho2/back.png", "/game/sprites/birolho2/1.png"));
  if (id === "punisher") put("walkDirs", dirs("/game/sprites/punisher/front.png", "/game/sprites/punisher/back.png", "/game/sprites/punisher/front.png"));
  await Promise.all(jobs);
  return pools;
}

/** The main characters' sprites (see HERO_SPRITE_BY_NAME in engine.ts): loaded with the game
 * at startup and never released, so the heroes are ready on every screen right away. */
const MC_SPRITES: SpriteId[] = ["kaelFinal", "neera", "voss", "salazar", "aldric", "malrec"];

const spriteLoads = new WeakMap<GameArt, Map<SpriteId, Promise<void>>>();

/** Loads one sprite's art into `art`, once — repeat calls share the same request. Every pool
 * lands at the same moment, so a unit never plays a half-loaded set; until then its
 * `art.sprites` entry is simply absent. A failed load is logged and not retried. */
export function requestSpriteArt(art: GameArt, id: SpriteId): Promise<void> {
  let loads = spriteLoads.get(art);
  if (!loads) spriteLoads.set(art, (loads = new Map()));
  const pending = loads.get(id);
  if (pending) return pending;
  const job = loadSpritePools(id).then(
    (pools) => {
      for (const [key, value] of Object.entries(pools)) (art[key as SpritePoolKey] as Record<string, unknown>)[id] = value;
    },
    (err) => console.error(`[art] sprite ${id} failed to load`, err),
  );
  loads.set(id, job);
  return job;
}

type LoadProgressCallback = (settled: number, total: number) => void;

/** Loads every sprite a battle needs (see requestSpriteArt). Progress counts complete sprite pools. */
export function ensureSpriteArt(art: GameArt, ids: Iterable<SpriteId>, onProgress?: LoadProgressCallback): Promise<void> {
  const unique = [...new Set(ids)];
  let settled = 0;
  onProgress?.(settled, unique.length);
  return Promise.all(unique.map(async (id) => {
    await requestSpriteArt(art, id);
    onProgress?.(++settled, unique.length);
  })).then(() => undefined);
}

/** Drops every loaded sprite not in `keep`, so memory follows the current battle instead of
 * growing with every battle played. A dropped sprite reloads on demand (browser cache). */
export function releaseSpriteArt(art: GameArt, keep: Iterable<SpriteId>): void {
  const keepSet = new Set([...keep, ...MC_SPRITES]);
  const loads = spriteLoads.get(art);
  if (!loads) return;
  for (const id of [...loads.keys()]) {
    if (keepSet.has(id)) continue;
    loads.delete(id);
    for (const key of SPRITE_POOL_KEYS) delete (art[key] as Record<string, unknown>)[id];
  }
}

/** Wait for the painted variants before opening a battle; editor variants load on access. */
export async function ensureTerrainArt(art: GameArt, tiles: readonly TerrainId[], variants: readonly number[], onProgress?: LoadProgressCallback): Promise<void> {
  const images = new Set(tiles.map((id, index) => art.tiles[id][variants[index] ?? 0] ?? art.tiles[id][0]));
  let settled = 0;
  onProgress?.(settled, images.size);
  await Promise.all([...images].map(image => {
    const ready = image.complete ? Promise.resolve() : new Promise<void>(resolve => {
      const finish = () => { image.removeEventListener("load", finish); image.removeEventListener("error", finish); resolve(); };
      image.addEventListener("load", finish); image.addEventListener("error", finish);
    });
    return ready.then(() => onProgress?.(++settled, images.size));
  }));
}

/** Load only placed decorations, including their optional mirrored-facing artwork.
 * Reuse renderer-started image requests instead of fetching each asset twice. */
export async function ensureDecorationArt(art: GameArt, ids: Iterable<string>, onProgress?: LoadProgressCallback): Promise<void> {
  const files = new Set<string>();
  for (const id of ids) {
    if (id === "floor-connector") {
      files.add("floor-connector-down-red-v1");
      files.add("floor-connector-up-gold-v1");
    } else if (id === "stone-stairs-up-001" || id === "stone-stairs-down-001") {
      files.add("stone-stairs-up-001");
      files.add("stone-stairs-down-001");
    } else files.add(id);
    if (DECORATIONS[id]?.mirrorAlternate) files.add(decorationSideFile(id, 3));
  }
  let settled = 0;
  onProgress?.(settled, files.size);
  await Promise.all([...files].map(async id => {
    try {
      const existing = art.decorations[id];
      if (existing?.naturalWidth) return;
      if (existing && !existing.complete) {
        await new Promise<void>(resolve => {
          const finish = () => { existing.removeEventListener("load", finish); existing.removeEventListener("error", finish); resolve(); };
          existing.addEventListener("load", finish);
          existing.addEventListener("error", finish);
        });
        if (existing.naturalWidth) return;
      }
      art.decorations[id] = await loadImage(decorationImage(id))
        .catch(() => loadImage(decorationImageWebp(id)))
        .catch(() => new Image());
    } finally {
      onProgress?.(++settled, files.size);
    }
  }));
}

export async function loadGameArt(): Promise<GameArt> {
  // Engine2: the old 2D tile art is gone (the 3D map draws the ground), so no tile image loads.
  const tiles = Object.fromEntries(TILES.map((id) => [id, [] as HTMLImageElement[]])) as Record<TerrainId, HTMLImageElement[]>;
  const decorations = {} as Record<string, HTMLImageElement>;
  // Decorations load for the current map through ensureDecorationArt.
  // Unit sprites are NOT loaded here: every sprite pool starts empty and each battle loads
  // only the sprites its own units use (see ensureSpriteArt/requestSpriteArt above), instead
  // of every sprite in the game at the title screen.
  const sprites = {} as Record<SpriteId, HTMLImageElement[]>;
  const attacks: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const attacks2: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const attacks2Left: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const attacksShort: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const casts: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const castsLeft: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const castsHeal: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const counters: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  // No sprite has a dedicated left-facing counter cut yet — every counters entry mirrors
  // via the regular flip, same as attacksLeft does for a sprite absent from that table.
  const countersLeft: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const walks: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const walksLeft: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const attacksLeft: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const impact = await Promise.all([1, 2, 3, 4].map((n) => loadImage(`/game/fx/impact-${n}.png`)));
  // v2: real alpha-cutout comet art (ball + trailing wisps), replacing the old flattened
  // black-background v1 that only ever worked by additive-blending the black away.
  const fireballCore = await loadImage("/game/fx/fireball-core-v2.png?v=1");
  const causticVenomCore = await loadImage("/game/fx/caustic-venom-core-v2.png?v=1");
  const arrowCore = deriveAlphaFromBlack(await loadImage("/game/fx/arrow-002.png?v=1"));
  const lightningCores = await Promise.all([
    loadImage("/game/fx/lightning-core-v1.png?v=1"),
    loadImage("/game/fx/lightning-core-v2.png?v=1"),
    loadImage("/game/fx/lightning-core-v3.png?v=1"),
  ]);
  const webfloor = await loadImage("/game/fx/webfloor.png?v=1");
  const watchtowerUpperFloorsBackdrop = await loadImage("/game/assets/watchtower-entry-panorama-v02.png");
  const watchtowerDungeonBackdrop = await loadImage("/game/assets/watchtower-dungeon-bg.jpg");
  const backdrops: Record<string, HTMLImageElement> = {
    // Watchtower route: the entry courtyard, two lower dungeon floors, the second and third
    // upper floors, then the beacon on the fourth upper floor.
    "watchtower-gate-floor": watchtowerUpperFloorsBackdrop,
    "watchtower-undercroft": watchtowerDungeonBackdrop,
    "watchtower-prison": watchtowerDungeonBackdrop,
    "watchtower-barracks": watchtowerUpperFloorsBackdrop,
    "watchtower-command": watchtowerUpperFloorsBackdrop,
    "watchtower-beacon": watchtowerUpperFloorsBackdrop,
    "frozen-tundra-crossing": await loadImage("/game/assets/frozen-tundra-background.jpg"),
    profundezas: await loadImage("/game/assets/profundezas-bg.jpg?v=2"),
    thebridge: await loadImage("/game/assets/thebridge-bg-clean-v2.png"),
    "wisp-forest": await loadImage("/game/assets/wisp-forest-bg.jpg"),
    "wisp-forest-2": await loadImage("/game/assets/wisp-forest-bg.jpg"),
    "wisp-forest-crossing": await loadImage("/game/assets/wisp-forest-crossing-panorama-v04-alpha.png"),
    // Cemitério dos Esquecidos (the grounds): aerial view of the snowed-in graveyard. Files keep
    // their original names, so the paths are percent-encoded (spaces, accents, the ✕).
    "cemiterio-esquecidos": await loadImage(encodeURI("/game/assets/CemiteryBackground.jpg")),
    // Câmara Profunda: the burial vault with the skylight.
    "cemiterio-esquecidos-cripta-2": await loadImage(encodeURI("/game/assets/2Cemitério dos Esquecidos — Câmara Profunda✕.jpg")),
    // A Estalagem do Osso Seco (the walkable Inn): the tavern interior behind the board.
    estalagem: await loadImage("/game/assets/INNbackground-v02.png"),
    // As Profundezas Enevoadas (Misty Cave dungeon): the torch-lit cavern passage.
    "misty-cave-dungeon": await loadImage(encodeURI("/game/assets/As Profundezas EnevoadasBackground2.jpg")),
    // The other floors of the cemetery, each with its own picture (file names say which floor).
    "cemiterio-esquecidos-cripta": await loadImage(encodeURI("/game/assets/Andar 3Cemitério dos Esquecidos — Cripta✕.jpg")),
    "cemiterio-esquecidos-mausoleu": await loadImage(encodeURI("/game/assets/Andar 4Cemitério dos Esquecidos — Mausoléu✕.jpg")),
    "cemiterio-esquecidos-ruinas": await loadImage(encodeURI("/game/assets/Andar 5Cemitério dos Esquecidos — Ruínas Submersas.jpg")),
    "random-encounter-1": await loadImage("/game/assets/random-encounter-1-bg.jpg"),
    "random-encounter-2": await loadImage("/game/assets/random-encounter-2-bg.jpg"),
    "random-encounter-4": await loadImage("/game/assets/random-encounter-4-bg.jpg"),
    "random-encounter-5": await loadImage("/game/assets/random-encounter-5-bg.jpg"),
    "random-encounter-8": await loadImage("/game/assets/random-encounter-8-bg.jpg"),
    "random-encounter-11": await loadImage("/game/assets/merchant-road-background-001.jpg"),
    "random-encounter-14": await loadImage("/game/assets/merchant-snow-market-background-001.jpg"),
    // O Vau's campaign battlefield has its own ash-river vista. Keep Vau Raso on the earlier
    // backdrop below: its road encounter is a separate place and should not inherit this scene.
    vau: await loadImage("/game/assets/vau-1-bg-clean-v2.png"),
    "random-encounter-6": await loadImage("/game/assets/vau-bg.jpg"),
    aldeia: await loadImage("/game/assets/aldeia-bg-clean-v2.png"),
    bosque: await loadImage("/game/assets/bosque-bg-clean-v2.png"),
  };
  const idles: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const idles2: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const walksUp: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const walksDown: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const walks2: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const walksLeft2: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const walkDirs: GameArt["walkDirs"] = {};
  const deaths: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const deaths2: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const hits2: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const hits: Partial<Record<SpriteId, HTMLImageElement[]>> = {};
  const art: GameArt = { tiles, decorations, sprites, attacks, attacks2, attacks2Left, attacksShort, attacksLeft, casts, castsLeft, castsHeal, counters, countersLeft, walks, walksLeft, idles, idles2, walkDirs, walksUp, walksDown, walks2, walksLeft2, deaths, deaths2, hits, hits2, impact, fireballCore, causticVenomCore, arrowCore, lightningCores, webfloor, backdrops };
  await ensureSpriteArt(art, MC_SPRITES);
  try {
    localStorage.setItem(ART_TOTAL_KEY, String(artRequested));
  } catch {
    // No storage: the next load just falls back to ART_TOTAL_FALLBACK.
  }
  return art;
}
