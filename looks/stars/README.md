# Night sky stars: saved looks

The user's rule (2026-10-14): every look is saved in a numbered series and kept until the user deletes it
after seeing it. A change is a new look, never an edit of a saved one.

- See them: `Sandbox --map C:/Engine2/maps/ember/cemiterio-esquecidos003.json ... --fog 0 --scene-time 0
  --tilt -18 --star-look N`; in a window, the Map panel's "star look" dropdown.
- Each look: a branch of `starField()` in `shaders/atmosphere.hlsli` (frozen), its capture here
  (`NNN/capture.png`: cemiterio-esquecidos003, bright night, camera at its lowest tilt), git tag `star-look-NNN`.
- Stars show at night (the key light is the moon) and as a day map's sun sets; the air's transmittance colours
  them (low stars are warmer, as real ones).

| Look | What | Verdict |
|---|---|---|
| 001 | first look (`de72f3c`): 0.25% of a 300-per-unit grid, each star a whole cell: ~6 px squares | user: "pixelated", "lacking in stars" |
| 002 | the same stars as soft points about a pixel wide | user: sparse ("lacking in stars") |
| 003 | dense: 2% of a 400-per-unit grid (~15x 002), most faint and a few bright, colour blue-white to orange; points | user (2026-10-19): "100% randômicas, sem nenhum pensamento, nenhuma tentativa de parecer um céu de verdade com constelações" |
| 004 "real" | the real sky (`content/sky/stars.json`, `render/StarSky.h`, `atmosphere.hlsli realStars`): the ~220 brightest stars at their true place, magnitude and colour (Orion, Crux, Scorpius, Sagittarius, Ursa Major, Cassiopeia, Cygnus, the Pleiades...), seen from latitude 23 S and turning with the hour (sidereal; the cycle's hour, 22 h on a night map), each a point about a pixel wide with its peak at its magnitude, the brightest with a small glow; look 003 at a quarter under them for the nameless faint ones; the Milky Way as a band about the galactic plane with its star clouds, brighter at the bulge, cut by the Great Rift; the Large and Small Magellanic clouds. Default. | not judged |
| 005 "andrômeda" | the night of a world inside the Andromeda galaxy: the user's own painting `Attachments/andromeda_ceu_repetivel.jpg` (5544 x 2048, horizontally repeatable; copy `content/sky/andromeda/ceu-andromeda-v01.jpg`, ground pack `sky/andromeda/ceu-andromeda-v01.jpg.tex`, `atmosphere.hlsli andromedaSky`), the whole sky: twice round along the galaxy's plane (once round it was 2.3 x enlarged: "too zoomed in"), the galaxy's core in the painting's middle, 33 deg either side of the plane at the same scale, mirrored and fading into its own edge colour beyond; shown as painted (its light divided by the exposure, `kPaintingWhite` 0.8); turns with the hour like 004; no catalogue stars over it. | not judged |

Rejected and kept, not in use (look 005's first version, commit 72d21200, the user, 2026-11-03: "isso ainda é a maldita via
láctea", "não são estrelas, são pixels quadrados"): Gaia EDR3's map mirrored (`content/sky/andromeda/galaxy-v01-0..3.png`),
the Milky Way from outside (`milkyway-v01.png`; the user: "QUE VIA LACTEA?!"), Triangulum (`triangulum-v01.png`), the world's own
stars (`content/sky/andromeda-stars.json`, still read for the galaxy frame's turning), `tools/scripts/sky/make_andromeda_sky.py`,
and a computed band of Andromeda's structure (`tools/scripts/sky/make_andromeda_band.py`, preview only). Their sources in
`build/sky/source`. Credits if ever used: ESA/Gaia/DPAC (CC BY-SA 3.0 IGO, A. Moitinho; S. Payne-Wardenaar), ESO (CC BY 4.0).
