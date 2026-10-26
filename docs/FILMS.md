# Films: AV1 + Opus in WebM

The game's cutscenes play through `engine/media/VideoPlayer`. Since 2026-10-25 (the user: our own decoders, every PC the same,
AV1 about half of H.264's size; ~28 hours of film planned at 420p):

- **`.webm` = AV1 picture + Opus sound**, decoded by our own libraries: libwebm 1.0.0.32 (its Matroska reader only), dav1d 1.5.4
  (VideoLAN's AV1 decoder, compiled by `cmake/Dav1d.cmake`: C only, 8-bit, threaded) and Opus 1.6.1. Plays the same on every PC.
- **`.mp4` = H.264 + AAC**, through Windows Media Foundation, as before.
- A film asked for as `game/<name>.mp4` plays `game/<name>.webm` when that file lies beside it. The MP4 is kept; subtitles are
  found by the name, so nothing else changes.

**REJECTED (the user, 2026-10-25):** the intro encoded with the command below (CRF 30, from the game's own MP4s) "looks like
garbage" in play, although SSIM 0.98 / PSNR 43 dB and a still frame looked fine. Both copies were removed (319dbb6e) and no film
is encoded; the game plays the MP4s. Encode a film only on the user's order, from their master, with settings they judged in
motion; never trust SSIM / PSNR / stills for a film.

**In use (the user, 2026-10-26: "passar essa intro por esse sistema", then "coloca o AV1 1080p como a abertura"):** the new
campaign's intro `game/vau-intro.webm` = the user's 1080p master `Attachments/EmberMovieIntro.mp4` (same cut as vau-intro.mp4,
97.84 s) through the command below, unchanged (CRF 30): 142 MB -> 21.7 MB, 1.9 Mbps. `--film-check`: 81 pictures/s (25 needed),
16.6 ms a picture, worst 163 ms (absorbed by the picture queue: not seen in play). The 640x350 `vau-intro.mp4` stays beside it.
The 2026-10-25 rejection was the same settings from the 640x350 MP4; this one is from a 1080p master. The user, after watching it:
"the quality doesnt seem affected by the codec".

**In use (the user, 2026-10-26: "EmberINNarrival you can do this video now too"):** the first Inn visit's film
`game/inn-arrival.webm` = the user's 1080p master `Attachments/EmberINNarrival.mp4` (the same start as inn-arrival.mp4, its last
seconds cut by the user: 42.56 s vs 45.16 s; the subtitles end at 42.1 s), same command: 61.5 MB -> 13.3 MB. `--film-check`:
105 pictures/s, 12.7 ms a picture, worst 87 ms. The 640x350 `inn-arrival.mp4` stays beside it.

**In use (the user, 2026-10-26: "EmberMovieWISP1 now do this one"):** the first Wisp Forest mission's film
`game/wisp-entrance.webm` = the user's 1080p master `Attachments/EmberMovieWISP1.mp4` (12.56 s, the same length as
wisp-entrance.mp4), same command: 18.3 MB -> 3.8 MB. `--film-check`: 103 pictures/s, 12.6 ms a picture, worst 69 ms. The 640x350
`wisp-entrance.mp4` stays beside it.

**In use (the user, 2026-10-26: "EmberMovieBurningVillage will need subtiles resync"):** aldeia's briefing film
`game/aldeia-intro.webm` = the user's 1080p master `Attachments/EmberMovieBurningVillage.mp4`, a new cut of aldeia-intro.mp4
(9.66 s vs 14.40 s): the old 0-1.09 s and ~6.1-9.7 s are gone. Found by correlating the two films' sound: new = old - 1.088 s
before the middle cut, new = old - 4.713 s after it (the last line's sound matches at 0.96-0.99). Same command: 14.0 MB -> 2.9 MB;
`--film-check` 100 pictures/s, worst 72 ms. **The four `subtitles/aldeia-intro.*.vtt` were moved to the new cut** (times only;
"Corram!" / "Run!" keeps its last 0.6 s, 0-0.612, its start fell in the cut); the old timings are kept in
`subtitles/kept/aldeia-intro-640x350/` (the game does not read that folder). The 640x350 `aldeia-intro.mp4` stays beside it, but
its subtitles are now the new cut's.

## Encoding a film (the settings used)

From the best master there is (re-encoding an already compressed copy costs a little quality), with ffmpeg's SVT-AV1:

```
ffmpeg -i master.mp4 -map 0:v:0 -map 0:a:0? -c:v libsvtav1 -preset 4 -crf 30 -pix_fmt yuv420p -g 240 -svtav1-params tune=0 -c:a libopus -b:a 128k -ar 48000 -ac 2 <name>.webm
```

- `-pix_fmt yuv420p`: 8-bit 4:2:0 is what the game's dav1d build decodes (10-bit or 4:4:4 films are refused with a log line).
- Sound: mono or stereo Opus (more channels are not played yet).
- `-crf 30`: lower = better and bigger. `-preset 4`: slower = smaller at the same quality (2..6 are sensible).
- `-g 240`: a keyframe every ~10 s.

## Measured (2026-10-25, this PC: 32 CPU threads)

`Sandbox --film-check content/game/public/game/<name>.mp4 [still.png [picture]]` decodes the MP4 and its WebM twin flat out (no
window) and logs each one's size and cost; with a still it writes that picture from both, to compare colours.

| film | MP4 (H.264) | WebM (AV1 + Opus) | AV1 decode |
|---|---|---|---|
| vau-intro, 640x350, 98 s | 97.3 MB | 8.0 MB (8 %) | 556-693 pictures/s (25 needed) |
| title-open, 1472x800, 6 s | 8.7 MB | 4.9 MB (56 %) | 125-156 pictures/s (24 needed) |

The Vau intro's MP4 was encoded at ~8 Mbps, far more than 640x350 needs, hence the large saving; its AV1 copy measures SSIM
0.98 / PSNR 43 dB against it. Without assembly (NASM is not installed here) dav1d decodes 1080p at roughly 80-90 pictures a
second on this PC by the numbers above: plenty for 420p-1080p films, not enough for 4K (that would need dav1d's assembly).
