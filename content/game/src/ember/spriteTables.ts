// Ember sprite tables: which poses each sprite has, how many frames, and the left-facing cuts.
// Copied unchanged from Ember src/game/assets.ts (only `export` added) so Engine2 plays every unit
// exactly as Ember does. Read-only snapshot: re-copy from Ember, never hand-edit.

import type { SpriteId } from "./types";

// Sprites cut as a 12-frame idle rather than the 4-frame default — the heroes, the two
// big horrors, and the creatures cut from reference video (familiar, familiar2, ancient
// golem). loadGameArt rejects on any missing file, so this set and what is on disk have to
// move together.
export const HERO_IDLE = new Set<SpriteId>(["defaultWarrior", "neera", "voss", "BossVoss", "salazar", "aldric", "defaultLancer", "horror", "Asherah", "familiar", "familiar2", "ancient-golem", "lancer", "sandoval", "kaelFinal", "kaelEarly", "conjurer", "malrec", "archerRecruit", "mageRecruit", "healerRecruit"]);

// Attack cuts, per sprite: how many atk-*.png frames are on disk, and the cache-bust the
// set was last republished under. attackPose spreads whatever count it finds across the
// lunge/hit/recover stages, so a set only has to be listed here to animate.
export const ATTACK_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
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
  salazar: { n: 4, bust: "" },
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
export const CAST_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
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

// Familiar 3's second, distinct attack cut (atk2-*.png) — the first case of a class having
// more than one attack cut, same "second pool, same idleAlt flip" shape as Malrec's idles2
// but for the attack pool instead of idle (see GameArt.attacks2's doc comment).
export const ATTACK2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  // Jacaré's Special (TEK cut): jaws open, tail whips up — alternates with its ATT turn to turn.
  jacare: { n: 36, bust: "" },
  "big-blue-ox-002": { n: 36, bust: "?v=big-blue-ox-ai-006" },
  familiar3: { n: 36, bust: "" },
  // Neera V2's labeled Special sheet is reserved for arrow skills.
  neera: { n: 36, bust: "?v=neera-special-001" },
};

// Short-range (off-hand dagger/katar) attack cut — see GameArt.attacksShort.
export const ATTACK_SHORT_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  neera: { n: 36, bust: "" },
};

// Left-facing counterpart to a handful of the CAST_FRAMES cuts above — same idea as
// attacksLeft/walksLeft: a sprite here skips the mirrored flip and plays this set instead
// when facing === -1.
export const CAST_DIR_LEFT: SpriteId[] = ["aldric"];

// Counter pose: counter-*.png, same shape as the attack table — a sprite absent from here
// falls back to its attacks cut (the same swing used for a normal attack) for the
// defender's counter stages, same as every sprite did before this existed.
export const COUNTER_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  voss: { n: 64, bust: "?v=voss-v2-001" },
  theButcher: { n: 36, bust: "?v=the-butcher-counter-001" },
};

// Walk cycles: move-*.png, same shape as the attack table. A sprite absent from here has
// no walk cut and falls back to its idle loop played faster, as every sprite used to.
export const WALK_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
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

export const DIR_LEFT: SpriteId[] = ["aldric", "defaultLancer", "lancer", "sandoval"];

// Hit-reaction sheets: hit-*.png, played whenever the unit takes damage (see GameArt.hits).
export const HIT_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
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
export const HIT2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  apparition: { n: 36, bust: "" },
};

// Death sheets: death-*.png, played once when the unit dies (see GameArt.deaths).
export const DEATH_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
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
export const DEATH2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  zombieDog: { n: 36, bust: "?v=f36" },
};

// Up/down walk cycles (move-up-*.png / move-down-*.png), opt-in per sprite — see
// GameArt.walksUp/walksDown. A sprite with only one of the two keeps its left/right walk
// for the other direction.
export const WALK_UP_DOWN_FRAMES: Partial<Record<SpriteId, { up?: number; down?: number; bust: string }>> = {
  BasePikeman: { down: 36, bust: "" },
  BirolhoLegs: { up: 36, down: 36, bust: "" },
  "mordavian-wolf-final": { up: 36, down: 36, bust: "" },
  troll2: { up: 36, down: 36, bust: "" },
};

// Cosmetic alternate walk (see GameArt.walks2): its own right- and left-facing cuts.
export const WALK2_FRAMES: Partial<Record<SpriteId, { n: number; bust: string }>> = {
  familiar3: { n: 36, bust: "" },
};

// From Ember spriteFrameSrc(): the folder each sprite id reads its frames from.
export function spriteDirectory(id: SpriteId): string {
  return id === "minor-horror-001" ? "minor-horror-003" : id === "big-blue-ox-002" ? "big-blue-ox-ai-006" : id === "conjurer" ? "conjurer/conjurer-complete-003" : id === "sandoval" ? "sandoval/sandoval-complete-001" : id === "kaelFinal" ? "Kael_Final/kael-final-002" : id === "kaelEarly" ? "kael" : id === "defaultWarrior" ? "kael-v2" : id === "voss" ? "voss-final" : id === "BossVoss" ? "voss" : id;
}

// From Ember loadSpritePools(): how many idle (numbered) frames each sprite loads.
export function idleFrameCount(id: SpriteId): number {
  return id === "RoccoTheBird" || id === "wardog2" || id === "EmberedWraith" || id === "zombieDog" ? 36 : id === "undeadOx" || id === "plague-bearing-cattle" ? 36 : id === "minor-horror-001" ? 36 : id === "big-blue-ox-002" ? 36 : id === "carnivorous-plant-001" ? 36 : id === "sapling-001" ? 36 : id === "zombie2" ? 11 : id === "neera" || id === "conjurer" || id === "kaelFinal" || id === "aldric" || id === "cultist-v2" || id === "militia-v2" || id === "BasePikeman" || id === "BaseCrossbowman" || id === "BrigandCrossbowman" || id === "StagWolf" || id === "apparition" || id === "jacare" || id === "malrec" || id === "familiar3" || id === "familiar2" ? 36 : id === "sandoval" || id === "mordavian-wolf" ? 8 : id === "birolho2" ? 18 : id === "birolho3" ? 12 : id === "zombie" ? 32 : id === "BirolhoLegs" || id === "BirolhoLegs2" || id === "troll2" || id === "ancient-golem" || id === "familiar4" || id === "mordavian-wolf-final" ? 36 : id === "voss" ? 64 : HERO_IDLE.has(id) ? 12 : 4;
}

export const MC_SPRITES: SpriteId[] = ["kaelFinal", "neera", "voss", "salazar", "aldric", "malrec"];
