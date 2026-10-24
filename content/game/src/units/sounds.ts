// Pose sounds, from Ember's audio.ts (files copied to /game/SoundFX; sound effects are kept apart from music).
import type { Pose, UnitDef } from './catalog';

type Kind = 'attack' | 'attack2' | 'cast' | 'heal' | 'walk' | 'hit' | 'death' | 'death2';

const MONSTER_SFX: Record<string, Partial<Record<Kind, string>>> = {
  undeadOx: { attack: 'UndeadOxATT001.mp3', cast: 'UndeadOxCast001.mp3', walk: 'UndeadOxWalk001.mp3', hit: 'UndeadOxHit001.mp3', death: 'UndeadOxDeath001.mp3' },
  zombieDog: { attack: 'ZombieDogATT001.mp3', cast: 'ZombieDogCast001.mp3', walk: 'ZombieDogWalk001.mp3', hit: 'ZombieDogHit001.mp3', death: 'ZombieDogDeath001.mp3', death2: 'ZombieDogDeath2001.mp3' },
  apparition: { cast: 'ApparitionCast001.mp3', hit: 'ApparitionHit001.mp3' },
  jacare: { attack: 'JacareATT001.mp3', attack2: 'JacareSpecial001.mp3', walk: 'JacareWalk001.mp3', hit: 'JacareHit001.mp3', death: 'JacareDeath001.mp3' },
  'militia-v2': { attack: 'FootmanLameSword.mp3', walk: 'FootSteps1.mp3', hit: 'HitreactArmor.mp3', death: 'DeadArmor.mp3' },
  // hit: the impact from its own Veo video (CrossbowmanHitReactDeath.mp4, 0.53–1.2 s)
  BaseCrossbowman: { hit: 'BaseCrossbowmanHit001.mp3' },
  'carnivorous-plant-001': { attack: 'CarnivorousPlantATT001.mp3', cast: 'CarnivorousPlantCast001.mp3', hit: 'CarnivorousPlantHit001.mp3', death: 'CarnivorousPlantDeath001.mp3' },
  'sapling-001': { attack: 'SaplingATT001.mp3', cast: 'SaplingCast001.mp3', walk: 'SaplingWalk001.mp3', hit: 'SaplingHit001.mp3', death: 'SaplingDeath001.mp3' },
  // the rest of Ember's MONSTER_SFX and its per-monster cues (audio.ts), for the units imported later
  wardog2: { attack: 'WarDog2ATT001.mp3', walk: 'WarDog2Walk001.mp3', death: 'WarDog2Death001.mp3' },
  RoccoTheBird: { cast: 'RoccoTheBirdCast001.mp3', walk: 'RoccoTheBirdWalk001.mp3' },
  'mordavian-wolf-final': { attack: 'MordavianWolfFinalATT001.mp3', hit: 'MordavianWolfFinalHit001.mp3', death: 'MordavianWolfFinalDeath001.mp3' },
  troll2: { attack: 'CaveTroll2ATT001.mp3', walk: 'CaveTroll2Walk001.mp3' },
  BirolhoLegs: { attack: 'BirolhoLegsATT001.mp3', cast: 'BirolhoLegsCast001.mp3' },
  BirolhoLegs2: { cast: 'BirolhoLegs2Cast001.mp3', walk: 'BirolhoLegs2Walk001.mp3' },
  EmberedWraith: { attack: 'EmberedWraithATT001.mp3', cast: 'EmberedWraithCast001.mp3', walk: 'EmberedWraithWalk001.mp3' },
  zombie: { attack: 'ZombieATT001.mp3' },
  'big-blue-ox-002': { attack: 'BigBlueOxATT001.mp3', walk: 'BigBlueOxWalk001.mp3', hit: 'BigBlueOxHit001.mp3', death: 'BigBlueOxDeath001.mp3' },
  'plague-bearing-cattle': { attack: 'PlagueCattleATT001.mp3', cast: 'PlagueCattleCast001.mp3', walk: 'PlagueCattleWalk001.mp3', death: 'PlagueCattleDeath001.mp3' },
  'minor-horror-001': { attack: 'MinorHorrorATT001.mp3', cast: 'MinorHorrorCasting001.mp3', walk: 'MinorHorrorWalk001.mp3' },
  // Ember picks CultistV2WalkLeft/Right by direction; Engine2 has one walk cue per unit for now
  salazar: { attack: 'Blunt2.mp3', cast: 'ElecticCasting01-Balanced-High.mp3', heal: 'Healing01-Balanced-High.mp3' },
  'cultist-v2': { attack: 'CultistV2Attack.mp3', cast: 'CultistV2Spellcast.mp3', walk: 'CultistV2WalkRight.mp3' },
};

// Heroes use Ember's generic cues: Kael's blade, Neera's bow, spellcasting for casters.
// Every main character walks with Neera's walk sound.
const HERO_SFX: Record<string, Partial<Record<Kind, [string, number?]>>> = {
  kaelFinal: { attack: ['BladeSlash1Dagger.mp3', 1.12], walk: ['NeeraWalk.mp3'] },
  neera: { attack: ['NeeraBowRelease.mp3'], attack2: ['NeeraBowRelease.mp3'], cast: ['Spellcast01.mp3'], walk: ['NeeraWalk.mp3'] },
  aldric: { attack: ['ATT01Blunt.mp3'], cast: ['ATT01Blunt.mp3'], walk: ['NeeraWalk.mp3'] },
  // Voss V2: the sounds supplied with his sheets (attack/counter: spell02, casting: Fireball)
  voss: { attack: ['VossAttack001.mp3'], cast: ['VossCast001.mp3'], walk: ['NeeraWalk.mp3'] },
  // BossVoss: the original Voss cues
  BossVoss: { attack: ['Spellcast01.mp3'], cast: ['Spellcast01.mp3'], walk: ['NeeraWalk.mp3'] },
  malrec: { attack: ['ATT01Blunt.mp3'], cast: ['Spellcast01.mp3'], walk: ['NeeraWalk.mp3'] },
  salazar: { walk: ['NeeraWalk.mp3'] },
};

const KIND: Partial<Record<Pose, Kind>> = {
  attack: 'attack', attackLeft: 'attack', attackShort: 'attack', attack2: 'attack2', attack2Left: 'attack2',
  cast: 'cast', castLeft: 'cast', heal: 'heal', counter: 'attack', walk: 'walk', walkLeft: 'walk', walkUp: 'walk', walkDown: 'walk', hit: 'hit', hit2: 'hit', death: 'death', death2: 'death2',
};

const playing = new Map<string, HTMLAudioElement>();
const SETTINGS_KEY = 'engine2:audio-settings';
const clampVolume = (value: number) => Math.max(0, Math.min(1, value));
let musicVolume = 1, sfxVolume = 1, cutsceneVolume = 1;
export let muted = false;
try {
  const saved = JSON.parse(localStorage.getItem(SETTINGS_KEY) ?? '{}') as Partial<{ muted: boolean; music: number; sfx: number; cutscene: number }>;
  muted = saved.muted === true;
  if (typeof saved.music === 'number') musicVolume = clampVolume(saved.music);
  if (typeof saved.sfx === 'number') sfxVolume = clampVolume(saved.sfx);
  if (typeof saved.cutscene === 'number') cutsceneVolume = clampVolume(saved.cutscene);
} catch { /* Defaults keep sound available when storage is blocked. */ }

function saveSettings(): void {
  try { localStorage.setItem(SETTINGS_KEY, JSON.stringify({ muted, music: musicVolume, sfx: sfxVolume, cutscene: cutsceneVolume })); } catch { /* Current-session settings still apply. */ }
}

export function getAudioSettings(): { muted: boolean; music: number; sfx: number; cutscene: number } {
  return { muted, music: musicVolume, sfx: sfxVolume, cutscene: cutsceneVolume };
}

export function setMuted(m: boolean): void {
  muted = m;
  if (m) for (const a of playing.values()) a.pause();
  document.querySelectorAll<HTMLAudioElement>('audio[data-engine2-music-base], audio[data-engine2-cutscene-base]').forEach((audio) => { audio.muted = m; });
  saveSettings();
}

export function setMusicVolume(value: number): void {
  musicVolume = clampVolume(value);
  document.querySelectorAll<HTMLAudioElement>('audio[data-engine2-music-base]').forEach((audio) => { audio.volume = clampVolume(Number(audio.dataset.engine2MusicBase ?? 1) * musicVolume); });
  saveSettings();
}

export function setSfxVolume(value: number): void {
  sfxVolume = clampVolume(value);
  for (const [file, audio] of playing) audio.volume = file.toLowerCase().includes('walk') ? 0.45 * sfxVolume : 0.55 * sfxVolume;
  saveSettings();
}

export function setCutsceneVolume(value: number): void {
  cutsceneVolume = clampVolume(value);
  document.querySelectorAll<HTMLAudioElement>('audio[data-engine2-cutscene-base]').forEach((audio) => { audio.volume = clampVolume(Number(audio.dataset.engine2CutsceneBase ?? 1) * cutsceneVolume); });
  saveSettings();
}

export function playPoseSound(u: UnitDef, pose: Pose): void {
  const kind = KIND[pose];
  if (!kind || muted) return;
  const m = MONSTER_SFX[u.sprite]?.[kind] ?? (kind === 'death2' ? MONSTER_SFX[u.sprite]?.death : undefined);
  const h = HERO_SFX[u.sprite]?.[kind === 'death2' ? 'death' : kind];
  const file = m ?? h?.[0];
  if (!file) return;
  let a = playing.get(file);
  if (!a) { a = new Audio(`/game/SoundFX/${file}`); playing.set(file, a); }
  a.volume = (kind === 'walk' ? 0.45 : 0.55) * sfxVolume;
  a.currentTime = m ? 0 : h?.[1] ?? 0;
  void a.play().catch(() => {});
}

export function stopWalkSound(u: UnitDef): void {
  const file = MONSTER_SFX[u.sprite]?.walk ?? HERO_SFX[u.sprite]?.walk?.[0];
  if (file) playing.get(file)?.pause();
}
