let ctx: AudioContext | null = null;
let master: GainNode | null = null;
let sfx: GainNode | null = null;
let music: GainNode | null = null;
let muted = false;
const activeSfx = new Set<HTMLAudioElement>();
let musicPaused = false;
const AUDIO_SETTINGS_KEY = "ember-ashes-audio-v1";
const clampVolume = (value: number) => Math.max(0, Math.min(1, value));
let musicVolume = 1;
let sfxVolume = 1;
// Cutscenes carry their own dialogue/sfx track with separate persisted volume and mute state.
let cutsceneVolume = 1;
let cutsceneEnabled = true;
let musicTimer = 0;
let retryTimer = 0;
const endedMusicTracks = new WeakSet<HTMLAudioElement>();

if (typeof window !== "undefined") {
  try {
    const saved = JSON.parse(window.localStorage.getItem(AUDIO_SETTINGS_KEY) ?? "{}") as { music?: unknown; sfx?: unknown; cutscene?: unknown; cutsceneEnabled?: unknown };
    if (typeof saved.music === "number") musicVolume = clampVolume(saved.music);
    if (typeof saved.sfx === "number") sfxVolume = clampVolume(saved.sfx);
    if (typeof saved.cutscene === "number") cutsceneVolume = clampVolume(saved.cutscene);
    if (typeof saved.cutsceneEnabled === "boolean") cutsceneEnabled = saved.cutsceneEnabled;
  } catch {
    // Audio preferences are optional; defaults keep the game playable when storage is blocked.
  }
}

function persistAudioSettings(): void {
  if (typeof window === "undefined") return;
  try {
    window.localStorage.setItem(AUDIO_SETTINGS_KEY, JSON.stringify({ music: musicVolume, sfx: sfxVolume, cutscene: cutsceneVolume, cutsceneEnabled }));
  } catch {
    // A private-storage browser can still use the current session's settings.
  }
}

function applyMusicVolume(): void {
  if (typeof document === "undefined") return;
  document.querySelectorAll<HTMLAudioElement>("audio[data-ember-music-base]").forEach((el) => {
    const base = Number(el.dataset.emberMusicBase ?? "0.4");
    el.volume = clampVolume(base * musicVolume);
  });
}

function applyCutsceneAudio(): void {
  if (typeof document === "undefined") return;
  document.querySelectorAll<HTMLVideoElement>("video[data-ember-cutscene]").forEach((video) => {
    video.volume = cutsceneVolume;
    video.muted = !cutsceneEnabled;
  });
}

export function getAudioVolumes(): { music: number; sfx: number; cutscene: number } {
  return { music: musicVolume, sfx: sfxVolume, cutscene: cutsceneVolume };
}

export function getCutsceneAudioSettings(): { enabled: boolean; volume: number } {
  return { enabled: cutsceneEnabled, volume: cutsceneVolume };
}

export function setMusicVolume(value: number): void {
  musicVolume = clampVolume(value);
  applyMusicVolume();
  persistAudioSettings();
}

export function setCutsceneVolume(value: number): void {
  cutsceneVolume = clampVolume(value);
  applyCutsceneAudio();
  persistAudioSettings();
}

export function setCutsceneEnabled(enabled: boolean): void {
  cutsceneEnabled = enabled;
  applyCutsceneAudio();
  persistAudioSettings();
}

export function setSfxVolume(value: number): void {
  sfxVolume = clampVolume(value);
  if (sfx && ctx) sfx.gain.setTargetAtTime(sfxVolume, ctx.currentTime, 0.02);
  persistAudioSettings();
}

if (typeof window !== "undefined") {
  const g = window as Window & { __brasaMusic?: number };
  if (g.__brasaMusic) {
    clearInterval(g.__brasaMusic);
    g.__brasaMusic = 0;
  }
}

function ac(): AudioContext | null {
  if (typeof window === "undefined") return null;
  if (!ctx) {
    const C = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    if (!C) return null;
    ctx = new C({ latencyHint: "interactive" });
    master = ctx.createGain();
    sfx = ctx.createGain();
    music = ctx.createGain();
    sfx.gain.value = sfxVolume;
    music.gain.value = 0.35;
    sfx.connect(master);
    music.connect(master);
    master.connect(ctx.destination);
    master.gain.value = muted ? 0 : 1;
  }
  return ctx;
}

function silentTick(c: AudioContext): void {
  const buf = c.createBuffer(1, 1, c.sampleRate);
  const src = c.createBufferSource();
  src.buffer = buf;
  src.connect(c.destination);
  try {
    src.start(0);
  } catch {
    // ignore
  }
}

export function unlockAudio(): void {
  const c = ac();
  if (!c) return;
  if (c.state === "suspended") {
    void c.resume().then(() => {
      if (ctx && ctx.state === "running") silentTick(ctx);
    });
  }
  silentTick(c);
}

export function setMuted(next: boolean): void {
  muted = next;
  for (const el of activeSfx) el.muted = next;
  if (typeof document !== "undefined") {
    document.querySelectorAll<HTMLMediaElement>("audio, video").forEach(el => { el.muted = next; });
  }
  if (master && ctx) {
    master.gain.setTargetAtTime(next ? 0 : 1, ctx.currentTime, 0.02);
  }
  if (!next) {
    unlockAudio();
  } else {
    stopMusic();
  }
}

export function isMuted(): boolean {
  return muted;
}

/** One-shot effect from a file in public/game/SoundFX (SFX_DIR), layered over the music
 * rather than replacing it — unlike playFile, this never
 * touches the theme/track elements, so it can't interrupt them. A fresh Audio() per call: the
 * previous play is left to finish on its own instead of being cut short by the next one. */
/** Preloaded copy of a one-shot file, cloned per play so a cue starts from already-fetched data. */
/** Engine2 keeps sound effects apart from music, in public/game/SoundFX (Ember: MUSIC/SoundFX). */
const SFX_DIR = "/game/SoundFX";
const sfxTemplates = new Map<string, HTMLAudioElement>();
function sfxTemplate(file: string): HTMLAudioElement {
  let el = sfxTemplates.get(file);
  if (!el) {
    el = new Audio(`${SFX_DIR}/${file}`);
    el.preload = "auto";
    sfxTemplates.set(file, el);
    el.load();
  }
  return el;
}
function playSfxFile(file: string, volume = 0.55): void {
  if (muted || typeof Audio === "undefined") return;
  const el = sfxTemplate(file).cloneNode(true) as HTMLAudioElement;
  activeSfx.add(el);
  el.addEventListener("ended", () => activeSfx.delete(el), { once: true });
  el.addEventListener("error", () => activeSfx.delete(el), { once: true });
  el.muted = muted;
  el.volume = volume * sfxVolume;
  el.play().catch(() => {});
}

/** One persistent element per file, reused instead of a fresh Audio() per call. A retrigger
 * while the previous play is still going seeks back to 0 and restarts it rather than layering
 * a second copy on top — so a generic cue fired several times in quick succession (a flurry of
 * basic attacks, a multi-target skill) never stacks into a buzzing chord of itself. Only worth
 * it for a cue reused across many different actions (the shared attack/cast recordings); a
 * one-off cue tied to a single distinct moment (LevelUp, Cultist V2's own cuts) has nothing to
 * overlap with itself and keeps using playSfxFile's layered fresh-Audio() behavior. */
const exclusiveSfxEls = new Map<string, HTMLAudioElement>();
function preloadExclusiveSfx(file: string): void {
  if (typeof Audio === "undefined" || exclusiveSfxEls.has(file)) return;
  const el = new Audio(`${SFX_DIR}/${file}`);
  el.preload = "auto";
  exclusiveSfxEls.set(file, el);
  el.load();
}

// Bow cues must be ready before an attack starts: loading either MP3 on its first play can
// make the shot sound arrive after its projectile. These two small files are used by every
// ranged attack, so warm them while the game initializes.
// Shared bow recordings have draw and release cues; Neera's supplied shot cue plays at the
// start of her attack so its timing stays aligned with her animation.
for (const file of ["ShortArrowsDraw.mp3", "ShortArrowsRelease.mp3", "NeeraBowRelease.mp3"]) preloadExclusiveSfx(file);
// Every other attack/cast cue is warmed too, so none of them loads on its first use.
for (const file of ["ATT01Blunt.mp3", "BladeSlash1Dagger.mp3", "Blade3.mp3", "Attack2.mp3", "Spellcast01.mp3", "ShieldBash.mp3"]) preloadExclusiveSfx(file);
for (const file of ["CarnivorousPlantATT001.mp3", "CarnivorousPlantCast001.mp3", "CarnivorousPlantHit001.mp3", "CarnivorousPlantDeath001.mp3"]) preloadExclusiveSfx(file);
for (const file of ["SaplingATT001.mp3", "SaplingCast001.mp3", "SaplingHit001.mp3", "SaplingDeath001.mp3", "SaplingWalk001.mp3"]) preloadExclusiveSfx(file);
for (const file of ["PlagueCattleATT001.mp3", "PlagueCattleCast001.mp3", "PlagueCattleDeath001.mp3", "PlagueCattleWalk001.mp3"]) preloadExclusiveSfx(file);
if (typeof Audio !== "undefined") {
  for (const file of ["CultistV2Attack.mp3", "CultistV2Spellcast.mp3", "MinorHorrorATT001.mp3", "MinorHorrorCasting001.mp3"]) sfxTemplate(file);
}

/** Where NeeraBowRelease.mp3's string snap begins (measured: silence/draw until a sharp
 * transient at 2.77 s, peaking at 2.774 s). */
const NEERA_BOW_SNAP = 2.77;

function playSfxFileExclusive(file: string, volume = 0.55, startAt = 0): void {
  if (muted || typeof Audio === "undefined") return;
  let el = exclusiveSfxEls.get(file);
  if (!el) {
    el = new Audio(`${SFX_DIR}/${file}`);
    el.preload = "auto";
    exclusiveSfxEls.set(file, el);
    activeSfx.add(el);
    el.load();
  }
  el.volume = volume * sfxVolume;
  el.currentTime = startAt;
  el.muted = muted;
  el.play().catch(() => {});
}

/** Monsters whose sounds are cut from their own source videos (work/monster-sfx/cut.py): each
 * clip is time-warped to play in step with its sheet the way the game plays it, and starts on
 * the sheet's first frame. Only videos without a music bed were used; idle never has sound.
 * Keyed by sprite id. A sprite/kind absent here keeps the game's generic cue. */
/** "attack2" is the cue for a sprite's second attack sheet (GameArt.attacks2), when it has one. */
/** "heal" is the cue for a support spell's healing sheet (GameArt.castsHeal), when it has one. */
export type MonsterSfxKind = "attack" | "attack2" | "cast" | "heal" | "walk" | "hit" | "death" | "death2";
const MONSTER_SFX: Record<string, Partial<Record<MonsterSfxKind, string>>> = {
  undeadOx: { attack: "UndeadOxATT001.mp3", cast: "UndeadOxCast001.mp3", walk: "UndeadOxWalk001.mp3", hit: "UndeadOxHit001.mp3", death: "UndeadOxDeath001.mp3" },
  wardog2: { attack: "WarDog2ATT001.mp3", walk: "WarDog2Walk001.mp3", death: "WarDog2Death001.mp3" },
  zombieDog: { attack: "ZombieDogATT001.mp3", cast: "ZombieDogCast001.mp3", walk: "ZombieDogWalk001.mp3", hit: "ZombieDogHit001.mp3", death: "ZombieDogDeath001.mp3", death2: "ZombieDogDeath2001.mp3" },
  RoccoTheBird: { cast: "RoccoTheBirdCast001.mp3", walk: "RoccoTheBirdWalk001.mp3" },
  "mordavian-wolf-final": { attack: "MordavianWolfFinalATT001.mp3", hit: "MordavianWolfFinalHit001.mp3", death: "MordavianWolfFinalDeath001.mp3" },
  troll2: { attack: "CaveTroll2ATT001.mp3", walk: "CaveTroll2Walk001.mp3" },
  BirolhoLegs: { attack: "BirolhoLegsATT001.mp3", cast: "BirolhoLegsCast001.mp3" },
  BirolhoLegs2: { cast: "BirolhoLegs2Cast001.mp3", walk: "BirolhoLegs2Walk001.mp3" },
  EmberedWraith: { attack: "EmberedWraithATT001.mp3", cast: "EmberedWraithCast001.mp3", walk: "EmberedWraithWalk001.mp3" },
  zombie: { attack: "ZombieATT001.mp3" },
  "big-blue-ox-002": { attack: "BigBlueOxATT001.mp3", walk: "BigBlueOxWalk001.mp3", hit: "BigBlueOxHit001.mp3", death: "BigBlueOxDeath001.mp3" },
  // Milícia V2: the sounds supplied with his sheets (Attachments/MilitiaV2), file names kept.
  // Apparition: her videos' own audio, each the same 3 s window as its sheet. The hit cue is cut
  // from hit react 2's video and plays for both of her hit reacts (as asked); death has none.
  apparition: { cast: "ApparitionCast001.mp3", hit: "ApparitionHit001.mp3" },
  // Jacaré: each clip is its sheet's own 3 s window of the video (work/jacare/build.py); idle has none.
  jacare: { attack: "JacareATT001.mp3", attack2: "JacareSpecial001.mp3", walk: "JacareWalk001.mp3", hit: "JacareHit001.mp3", death: "JacareDeath001.mp3" },
  "militia-v2": { attack: "FootmanLameSword.mp3", walk: "FootSteps1.mp3", hit: "HitreactArmor.mp3", death: "DeadArmor.mp3" },
  BaseCrossbowman: { hit: "BaseCrossbowmanHit001.mp3" },
  // Salazar V2: the sounds supplied with his sheets (Attachments/Salazar), file names kept. Cast is
  // for damage spells, heal for every other spell (heals, Bless, Create Food and Water, ...).
  salazar: { attack: "Blunt2.mp3", cast: "ElecticCasting01-Balanced-High.mp3", heal: "Healing01-Balanced-High.mp3", walk: "NeeraWalk.mp3" },
  // Voss V2: the sounds supplied with his sheets (Desktop/Voss).
  voss: { attack: "VossAttack001.mp3", cast: "VossCast001.mp3", walk: "NeeraWalk.mp3" },
};
// Every main character walks with Neera's walk sound (Attachments/WalkRight/NeeraWalk.mp3), both
// directions. Malrec's sprite here is his class's, "conjurer".
for (const sprite of ["kaelFinal", "kaelEarly", "neera", "voss", "BossVoss", "salazar", "aldric", "conjurer"]) MONSTER_SFX[sprite] = { ...MONSTER_SFX[sprite], walk: "NeeraWalk.mp3" };
for (const cues of Object.values(MONSTER_SFX)) for (const file of Object.values(cues)) preloadExclusiveSfx(file);

/** Whether a sprite has its own cue of this kind (see MONSTER_SFX). */
export function hasMonsterSfx(sprite: string | undefined, kind: MonsterSfxKind): boolean {
  return !!sprite && !!MONSTER_SFX[sprite]?.[kind];
}

/** Stops an exclusive cue early with a short fade, so cutting it off never clicks. */
function stopSfxFileExclusive(file: string, fadeMs = 120): void {
  const el = exclusiveSfxEls.get(file);
  if (!el || el.paused) return;
  const from = el.volume;
  const started = performance.now();
  const step = () => {
    const k = Math.min(1, (performance.now() - started) / fadeMs);
    el.volume = from * (1 - k);
    if (k < 1) requestAnimationFrame(step);
    else el.pause();
  };
  requestAnimationFrame(step);
}

// Sound cues without a supplied recording stay silent until a real asset is authored.
export const sfxPlay = {
  select: () => {},
  move: () => {},
  ui: () => {},
  purchase: () => {},
  hit: () => {},
  crit: () => {},
  death: () => {},
  turn: () => {},
  win: () => {},
  lose: () => {},
  spell: () => playSfxFileExclusive("Spellcast01.mp3", 0.55),
  dreamingWeb: () => {},
  summonFamiliar: () => {},
  // Weapon cues share the exclusive player so rapid repeats never stack.
  // bladeStartAt: seconds into the blade recording to start from (Kael's long swing skips its
  // silent lead-in so the swoosh lands on his slash). The blunt cue always starts at 0.
  meleeAttack: (blade = false, bladeStartAt = 0) => playSfxFileExclusive(blade ? "BladeSlash1Dagger.mp3" : "ATT01Blunt.mp3", 0.55, blade ? bladeStartAt : 0),
  daggerAttack: () => playSfxFileExclusive("Blade3.mp3", 0.55),
  kaelBladeSkill: () => playSfxFileExclusive("Attack2.mp3", 0.55),
  shieldBash: () => playSfxFileExclusive("ShieldBash.mp3", 0.55),
  // Cultist V2's own authored cues (see attachments/Cultist-V2), one per animation set —
  // played instead of the (now silent) generic attack/cast/move cues whenever the acting
  // unit's sprite is "cultist-v2" (see stepCombat/stepSpell/startSeq in engine.ts).
  minorHorrorAttack: () => playSfxFileExclusive("MinorHorrorATT001.mp3", 0.55),
  minorHorrorCast: () => playSfxFileExclusive("MinorHorrorCasting001.mp3", 0.55),
  minorHorrorWalk: () => playSfxFileExclusive("MinorHorrorWalk001.mp3", 0.45),
  // Carnivorous Plant: each clip is cut from the same 3 s window as its 36-frame sheet, so it
  // starts with the sheet and stays in sync. Idle has no sound on purpose.
  carnivorousPlantAttack: () => playSfxFileExclusive("CarnivorousPlantATT001.mp3", 0.55),
  carnivorousPlantCast: () => playSfxFileExclusive("CarnivorousPlantCast001.mp3", 0.55),
  carnivorousPlantHit: () => playSfxFileExclusive("CarnivorousPlantHit001.mp3", 0.55),
  carnivorousPlantDeath: () => playSfxFileExclusive("CarnivorousPlantDeath001.mp3", 0.55),
  // Sapling: same scheme — each clip starts on the first frame of its sheet. The walk clip runs
  // 3 s (two passes of the 1.5 s walk loop) so a normal move keeps its footsteps.
  saplingAttack: () => playSfxFileExclusive("SaplingATT001.mp3", 0.55),
  saplingCast: () => playSfxFileExclusive("SaplingCast001.mp3", 0.55),
  saplingHit: () => playSfxFileExclusive("SaplingHit001.mp3", 0.55),
  saplingDeath: () => playSfxFileExclusive("SaplingDeath001.mp3", 0.55),
  saplingWalk: () => playSfxFileExclusive("SaplingWalk001.mp3", 0.45),
  // Plague Bearing Cattle: each clip is cut from its sheet's own video window and stretched to
  // the sheet's in-game length (3 s actions, 1.5 s walk loop x2), starting on its first frame.
  // The walk is stopped when the move ends (its moves are short). Idle has no sound on purpose.
  plagueCattleAttack: () => playSfxFileExclusive("PlagueCattleATT001.mp3", 0.55),
  plagueCattleCast: () => playSfxFileExclusive("PlagueCattleCast001.mp3", 0.55),
  plagueCattleDeath: () => playSfxFileExclusive("PlagueCattleDeath001.mp3", 0.55),
  plagueCattleWalk: () => playSfxFileExclusive("PlagueCattleWalk001.mp3", 0.45),
  plagueCattleWalkStop: () => stopSfxFileExclusive("PlagueCattleWalk001.mp3"),
  /** A monster's own cue from MONSTER_SFX; returns false (and plays nothing) when it has none,
   * so the caller can fall back to the generic cue. */
  monster: (sprite: string | undefined, kind: MonsterSfxKind): boolean => {
    const file = sprite ? MONSTER_SFX[sprite]?.[kind] : undefined;
    if (!file) return false;
    playSfxFileExclusive(file, kind === "walk" ? 0.45 : 0.55);
    return true;
  },
  /** Fades out a monster's walk cue when its move ends (moves are shorter than the clip). */
  monsterWalkStop: (sprite: string | undefined) => {
    const file = sprite ? MONSTER_SFX[sprite]?.walk : undefined;
    if (file) stopSfxFileExclusive(file);
  },
  cultistV2Attack: () => playSfxFile("CultistV2Attack.mp3", 0.55),
  cultistV2Spellcast: () => playSfxFile("CultistV2Spellcast.mp3", 0.55),
  cultistV2WalkLeft: () => playSfxFile("CultistV2WalkLeft.mp3", 0.45),
  cultistV2WalkRight: () => playSfxFile("CultistV2WalkRight.mp3", 0.45),
  // `releaseIn`: seconds from now until Neera's arrow actually leaves the bow (her full 3 s
  // draw, or 0 with no wind-up). NeeraBowRelease.mp3's string snap starts NEERA_BOW_SNAP s
  // into the file, so it is delayed (or started part-way in) to land the snap on the release.
  arrowAttack: (neera = false, releaseIn = 0) => {
    if (!neera) {
      playSfxFileExclusive("ShortArrowsDraw.mp3", 0.55);
      return;
    }
    const lead = releaseIn - NEERA_BOW_SNAP;
    if (lead > 0) window.setTimeout(() => playSfxFileExclusive("NeeraBowRelease.mp3", 0.55), lead * 1000);
    else playSfxFileExclusive("NeeraBowRelease.mp3", 0.55, -lead);
  },
  arrowRelease: (neera = false) => { if (!neera) playSfxFileExclusive("ShortArrowsRelease.mp3", 0.55); },
  magicAttack: () => playSfxFileExclusive("Spellcast01.mp3", 0.55),
  heal: () => playSfxFileExclusive("Spellcast01.mp3", 0.55),
  /** The caster's own healing cue (MONSTER_SFX "heal") when it has one, else the generic heal. */
  healBy: (sprite: string | undefined) => {
    if (!sfxPlay.monster(sprite, "heal")) sfxPlay.heal();
  },
  stun: () => {},
  miss: () => {},
  chest: () => {},
  loot: () => {},
  thrust: (blade = false) => playSfxFileExclusive(blade ? "BladeSlash1Dagger.mp3" : "ATT01Blunt.mp3", 0.55),
  sweep: (blade = false) => playSfxFileExclusive(blade ? "BladeSlash1Dagger.mp3" : "ATT01Blunt.mp3", 0.55),
  trip: (blade = false) => playSfxFileExclusive(blade ? "BladeSlash1Dagger.mp3" : "ATT01Blunt.mp3", 0.55),
  levelUp: () => playSfxFile("LevelUp.mp3", 0.6),
};

let introEl: HTMLAudioElement | null = null;
let battleEl: HTMLAudioElement | null = null;
let earlyEl: HTMLAudioElement | null = null;
let templeEl: HTMLAudioElement | null = null;
let aldeiaEl: HTMLAudioElement | null = null;
let siegeEl: HTMLAudioElement | null = null;
let innEl: HTMLAudioElement | null = null;
let hillEl: HTMLAudioElement | null = null;
let portaoEl: HTMLAudioElement | null = null;
let worldMapEl: HTMLAudioElement | null = null;
type Theme = "intro" | "battle" | "early" | "temple" | "aldeia" | "siege" | "inn" | "hill" | "portao" | "worldMap";
let currentTheme: Theme = "intro";
let currentFile: string | null = null;

if (typeof window !== "undefined") {
  const g = window as Window & { __emberIntro?: HTMLAudioElement };
  if (g.__emberIntro) introEl = g.__emberIntro;
}

function attachTrack(el: HTMLAudioElement, volume: number): HTMLAudioElement {
  el.loop = true;
  el.addEventListener("ended", () => endedMusicTracks.add(el));
  el.preload = "auto";
  el.dataset.emberMusicBase = String(volume);
  el.volume = volume * musicVolume;
  if (typeof document !== "undefined" && document.body && !el.isConnected) {
    el.setAttribute("playsinline", "");
    document.body.appendChild(el);
  }
  return el;
}

function getTrack(theme: Theme): HTMLAudioElement | null {
  if (typeof Audio === "undefined") return null;
  if (theme === "intro") return menuElement();
  if (theme === "temple") {
    if (!templeEl) templeEl = attachTrack(new Audio("/game/MUSIC/temple.mp3"), 0.42);
    return templeEl;
  }
  if (theme === "aldeia") {
    if (!aldeiaEl) aldeiaEl = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent("EMBERBattle01-Balanced-High.mp3")}`), 0.4);
    return aldeiaEl;
  }
  if (theme === "siege") {
    if (!siegeEl) siegeEl = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent("Absolute Mental Descruction 00-Balanced-High.mp3")}`), 0.4);
    return siegeEl;
  }
  if (theme === "inn") {
    if (!innEl) innEl = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent("Connard The Barbarian 10.mp3")}`), 0.4);
    return innEl;
  }
  if (theme === "hill") {
    if (!hillEl) hillEl = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent("Long Space _Opera DJ  UMN+  022-Balanced-High.mp3")}`), 0.4);
    return hillEl;
  }
  if (theme === "portao") {
    if (!portaoEl) portaoEl = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent("TempleEntrance-Balanced-High.mp3")}`), 0.4);
    return portaoEl;
  }
  if (theme === "early") {
    if (!earlyEl) earlyEl = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent("LANDR-Absolute Mental Descruction umn3 10 (3)-Balanced-High.mp3")}`), 0.4);
    return earlyEl;
  }
  if (theme === "worldMap") {
    // The world map's own piece, under the name it was delivered as. Encoded because that
    // name carries spaces.
    if (!worldMapEl)
      worldMapEl = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent("Tragic Arqui TruePersonaWorldMapEmber-Balanced-High.mp3")}`), 0.4);
    return worldMapEl;
  }
  // The battle theme's own track, recovered from an earlier build's output where it was the
  // only copy left — it had gone missing from public/game/MUSIC while the code still asked
  // for it, which is why nothing played here.
  if (!battleEl) battleEl = attachTrack(new Audio("/game/MUSIC/music.mp3"), 0.4);
  return battleEl;
}

function menuElement(): HTMLAudioElement | null {
  if (typeof Audio === "undefined") return null;
  if (typeof document !== "undefined") {
    document.querySelectorAll("audio").forEach((node) => {
      if (node === introEl) return;
      if (node.id === "ember-intro" || /\/game\/music\/intro\.(wav|mp3)/.test(node.src)) {
        node.pause();
        node.remove();
      }
    });
  }
  if (introEl) return introEl;
  const node = new Audio(`/game/MUSIC/${encodeURIComponent("LANDR-AsheraIntrol-Balanced-High.mp3")}`);
  node.id = "ember-intro";
  node.loop = true;
  node.addEventListener("ended", () => endedMusicTracks.add(node));
  node.preload = "auto";
  node.dataset.emberMusicBase = "0.7";
  node.volume = 0.7 * musicVolume;
  if (typeof document !== "undefined" && document.body) {
    node.setAttribute("playsinline", "");
    document.body.appendChild(node);
  }
  introEl = node;
  if (typeof window !== "undefined") {
    (window as Window & { __emberIntro?: HTMLAudioElement }).__emberIntro = node;
  }
  return node;
}

/** HMR and older builds may leave a detached intro element behind. Keep the title track
 * exclusive even when it is no longer the module's current introEl reference. */
function pauseIntroTracks(except: HTMLAudioElement | null = null): void {
  if (typeof document !== "undefined") {
    document.querySelectorAll("audio").forEach((node) => {
      if (node === except) return;
      if (/\/game\/music\/intro\.mp3(?:[?#]|$)/i.test(node.src)) node.pause();
    });
  }
  if (introEl && introEl !== except) introEl.pause();
}

function kickPlay(el: HTMLAudioElement): void {
  if (muted || musicPaused) return;
  if (endedMusicTracks.has(el)) return;
  if (!el.paused && !el.ended) return;
  el.muted = false;
  el.defaultMuted = false;
  const baseVolume = Number(el.dataset.emberMusicBase ?? (el === introEl ? "0.7" : el === templeEl ? "0.42" : "0.4"));
  el.volume = clampVolume(baseVolume * musicVolume);
  const tryOnce = () => {
    if (muted || musicPaused) return;
    if (endedMusicTracks.has(el)) {
      if (retryTimer) {
        clearInterval(retryTimer);
        retryTimer = 0;
      }
      return;
    }
    if (!el.paused && !el.ended) {
      if (retryTimer) {
        clearInterval(retryTimer);
        retryTimer = 0;
      }
      return;
    }
    void el.play().then(() => {
      if (retryTimer) {
        clearInterval(retryTimer);
        retryTimer = 0;
      }
    }).catch(() => {
      if (muted || retryTimer) return;
      retryTimer = window.setInterval(() => {
        if (muted || musicPaused) {
          clearInterval(retryTimer);
          retryTimer = 0;
          return;
        }
        const want = currentFile ? getFileTrack(currentFile) : currentTheme === "intro" ? menuElement() : getTrack(currentTheme);
        if (!want || endedMusicTracks.has(want) || (!want.paused && !want.ended)) {
          if (retryTimer) {
            clearInterval(retryTimer);
            retryTimer = 0;
          }
          return;
        }
        void want.play().then(() => {
          if (retryTimer) {
            clearInterval(retryTimer);
            retryTimer = 0;
          }
        }).catch(() => {});
      }, 400);
    });
  };
  tryOnce();
}

/** Tracks played by file name rather than by theme — what a mission's own music setting
 * asks for. One element per file, made once and kept, same as the fixed themes. */
const fileEls = new Map<string, HTMLAudioElement>();

function getFileTrack(file: string): HTMLAudioElement | null {
  if (typeof Audio === "undefined") return null;
  const have = fileEls.get(file);
  if (have) return have;
  const el = attachTrack(new Audio(`/game/MUSIC/${encodeURIComponent(file)}`), 0.4);
  fileEls.set(file, el);
  return el;
}

/** Plays one specific file from public/game/MUSIC, silencing everything else — the escape
 * hatch from the fixed Theme list, so a mission can name its own track. */
export function playFile(file: string): void {
  const themeChanged = currentFile !== file;
  currentFile = file;
  if (muted) return;
  musicPaused = false;
  const want = getFileTrack(file);
  if (themeChanged && want) endedMusicTracks.delete(want);
  pauseIntroTracks(want === introEl ? introEl : null);
  silenceAllBut(want);
  if (!want) return;
  kickPlay(want);
}
/** Stops every track except the one asked for, themes and per-file alike. */
function silenceAllBut(want: HTMLAudioElement | null): void {
  const known = [introEl, battleEl, earlyEl, templeEl, aldeiaEl, siegeEl, innEl, hillEl, portaoEl, worldMapEl, ...fileEls.values()];
  const all = new Set(known.filter((el): el is HTMLAudioElement => !!el));
  if (typeof document !== "undefined") {
    document.querySelectorAll<HTMLAudioElement>("audio").forEach((el) => all.add(el));
  }
  for (const el of all) if (el !== want) el.pause();
}

export function playTheme(theme: Theme): void {
  const themeChanged = currentTheme !== theme || currentFile !== null;
  currentTheme = theme;
  currentFile = null;
  if (muted) return;
  musicPaused = false;
  const want = getTrack(theme);
  if (themeChanged && want) endedMusicTracks.delete(want);
  silenceAllBut(want);
  if (!want) return;
  kickPlay(want);
}

export function preloadMenuMusic(): void {
  menuElement();
}

export function playMenuMusic(): void {
  const themeChanged = currentTheme !== "intro" || currentFile !== null;
  currentTheme = "intro";
  currentFile = null;
  if (muted) return;
  musicPaused = false;
  const el = menuElement();
  if (!el) return;
  if (themeChanged) endedMusicTracks.delete(el);
  // Returning to the title is another hard boundary: custom mission tracks must not linger.
  silenceAllBut(el);
  kickPlay(el);
}

export function startMusic(): void {
  if (musicPaused) return;
  if (currentFile) playFile(currentFile);
  else playTheme(currentTheme);
}

/** Pause all background tracks together while preserving the current theme and time. */
export function pauseMusic(): void {
  musicPaused = true;
  stopMusic();
}

/** Resume the track selected by the current screen. */
export function resumeMusic(): void {
  musicPaused = false;
  const want = currentFile ? getFileTrack(currentFile) : currentTheme === "intro" ? menuElement() : getTrack(currentTheme);
  // A deliberate Resume starts a finished recording again. Autoplay unlocks still
  // respect endedMusicTracks, so ordinary clicks never turn single plays into a loop.
  if (want && (want.ended || endedMusicTracks.has(want))) {
    endedMusicTracks.delete(want);
    want.currentTime = 0;
  }
  startMusic();
}

export function stopMusic(): void {
  // A later click/key used to call the global autoplay unlock and restart the last
  // selected track underneath music-free screens (including cutscenes). Explicitly
  // stopped music stays stopped until a screen selects a theme or the user resumes it.
  musicPaused = true;
  if (musicTimer) {
    clearInterval(musicTimer);
    musicTimer = 0;
  }
  if (retryTimer) {
    clearInterval(retryTimer);
    retryTimer = 0;
  }
  if (typeof window !== "undefined") {
    const g = window as Window & { __brasaMusic?: number };
    if (g.__brasaMusic) {
      clearInterval(g.__brasaMusic);
      g.__brasaMusic = 0;
    }
  }
  introEl?.pause();
  battleEl?.pause();
  earlyEl?.pause();
  templeEl?.pause();
  aldeiaEl?.pause();
  siegeEl?.pause();
  innEl?.pause();
  hillEl?.pause();
  portaoEl?.pause();
  worldMapEl?.pause();
  for (const el of fileEls.values()) el.pause();
  if (typeof document !== "undefined") {
    document.querySelectorAll<HTMLAudioElement>("audio").forEach((el) => el.pause());
  }
}

export function resumeAudio(): void {
  unlockAudio();
}

export function installAudioUnlock(): () => void {
  if (typeof window === "undefined") return () => {};
  const audioWindow = window as Window & { __emberAudioUnlockCleanup?: () => void };
  // Remove the previous module's listener during hot reloads. Its stale theme state
  // could otherwise restart the intro over the currently selected game track.
  audioWindow.__emberAudioUnlockCleanup?.();
  const arm = () => {
    unlockAudio();
  };
  const opts: AddEventListenerOptions = { capture: true };
  window.addEventListener("pointerdown", arm, opts);
  window.addEventListener("keydown", arm, opts);
  const cleanup = () => {
    window.removeEventListener("pointerdown", arm, opts);
    window.removeEventListener("keydown", arm, opts);
    if (audioWindow.__emberAudioUnlockCleanup === cleanup) delete audioWindow.__emberAudioUnlockCleanup;
  };
  audioWindow.__emberAudioUnlockCleanup = cleanup;
  return cleanup;
}



