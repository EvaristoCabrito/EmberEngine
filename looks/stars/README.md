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
| 005 "andrômeda" | the night of a world inside the Andromeda galaxy (the user, 2026-11-03: "um céu de vista vindo de Andrômeda e que seja RICO", option 2: no Orion, the Milky Way a galaxy beside it). From real images (`tools/scripts/sky/make_andromeda_sky.py`, `content/sky/andromeda/`, `atmosphere.hlsli andromedaSky`): the host galaxy = Gaia EDR3's map of the whole sky, 16000 x 8000 in four tiles, mirrored, its Magellanic clouds and the smudges of Andromeda / Triangulum painted out; the Milky Way seen from outside (ESA's face-on map, tilted, ~3 deg across, 13 deg off the band); Triangulum (ESO's VST photo, ~5 deg across); M32 and M110 as Sérsic glows beside the bulge; the world's own 225 bright stars (the real sky's magnitudes and colours at new places, `content/sky/andromeda-stars.json`). Turns with the hour like 004. Images on the GPU only once chosen (~200 MB). | not judged |

Image credits (look 005): galaxy map "The colour of the sky from Gaia's Early Data Release 3", ESA/Gaia/DPAC, CC BY-SA 3.0
IGO, acknowledgement A. Moitinho; Milky Way "The best Milky Way map, by Gaia", ESA/Gaia/DPAC, Stefan Payne-Wardenaar, CC BY-SA
3.0 IGO; Triangulum eso1424a, ESO, CC BY 4.0. The cut images in `content/sky/andromeda/` are derived works under the same
licences (ShareAlike for the two Gaia ones).
