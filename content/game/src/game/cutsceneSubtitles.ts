import type { Translations } from "./gamePreferences";

/** Portuguese (Brazil) and English subtitle tracks for every cutscene. */
export const CUTSCENE_SUBTITLES: Record<string, Translations> = Object.fromEntries([
  "aldeia-intro", "asherah-rite", "temple-aftermath", "wisp-entrance", "inn-arrival", "smith-intro",
].flatMap(name => ["mp4", "webm"].map(extension => [
  `/game/${name}.${extension}`,
  {
    pt: `/game/subtitles/${name}.pt-BR.vtt`,
    en: `/game/subtitles/${name}.en-v2.vtt`,
  },
]))) as Record<string, Translations>;

/** Show only the selected language track; disable browser-selected tracks from earlier videos. */
export function syncSubtitles(tracks: TextTrackList | undefined, enabled: boolean, language: "pt" | "en") {
  if (!tracks) return;
  for (const track of Array.from(tracks)) {
    track.mode = enabled && track.language.toLowerCase().startsWith(language) ? "showing" : "disabled";
  }
}
