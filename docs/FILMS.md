# Films: AV1 + Opus in WebM

The game's cutscenes play through `engine/media/VideoPlayer`. Since 2026-10-25 (the user: our own decoders, every PC the same,
AV1 about half of H.264's size; ~28 hours of film planned at 420p):

- **`.webm` = AV1 picture + Opus sound**, decoded by our own libraries: libwebm 1.0.0.32 (its Matroska reader only), dav1d 1.5.4
  (VideoLAN's AV1 decoder, compiled by `cmake/Dav1d.cmake`: C only, 8-bit, threaded) and Opus 1.6.1. Plays the same on every PC.
- **`.mp4` = H.264 + AAC**, through Windows Media Foundation, as before.
- A film asked for as `game/<name>.mp4` plays `game/<name>.webm` when that file lies beside it. The MP4 is kept; subtitles are
  found by the name, so nothing else changes.

## Encoding a film

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
