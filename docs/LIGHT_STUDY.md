# Light study: interiors lit from outside, fire, the camera (2026-10-21)

Why this exists: the user, on the Torre de Vigia gate floor by day: "fora da torre UM SOL ... refletindo nas paredes de pedra como se
fossem um espelho e dentro um raiozinho tímido de luz e não dá para ver um palmo na frente da janela"; "estudar como a luz se comporta
de verdade, estudar como engines tratam a luz 3D de verdade". This is that study, and what it says about EmberEngine.

## 1. How the light really behaves

- **Levels** (Unity HDRP's physical light unit tables, from lighting handbooks): direct sun 110 000-120 000 lux; blue sky at midday
  20 000; overcast midday 1 000-2 000; a moonlit clear night < 1; lived-in rooms 100-750 lux (bedroom 150-300, classroom 300-500).
  A candle is 12.57 lm, a decorative lamp 200-300 lm, a room's ceiling lamp 400-800 lm.
- **A room by day is lit mostly by bounced light, not by the sun patch.** Daylighting splits the light at a point indoors into the sky
  component (sky seen straight through the window), the externally reflected component (light off the ground and buildings outside)
  and the **internally reflected component (light that entered and bounced off the floor, walls and ceiling)**; their sum is the
  illuminance there (IES VE / BRE daylight factor). A typical room gets about 2 % of the outdoor overcast light (rule of thumb
  D = 0.1 x glazing % of floor), i.e. tens to hundreds of lux, everywhere in the room, not only in the window's beam. A sun patch on
  the floor (10^5 lux on a 30 % stone floor) is itself a lamp of thousands of lux that lights the whole room by its bounce.
- **The eye adapts to what it looks at.** Vision covers about 10^-6 .. 10^8 cd/m2 by adapting (photopic above ~3-5 cd/m2, mesopic
  0.005-5, scotopic below; CIE 191:2010). Inside a stone room the eye adapts to the room, so the room reads, a torch reads, and the
  windows and the sun patches read as very bright, not the other way round.
- **Fire does not change with the sun.** A torch emits the same flux at noon and at midnight; what changes is only the adaptation of
  the eye looking at it. Outdoors at noon it is lost in 10^5 lux of sun; in a dim room by day it lights its wall as at night.

## 2. How engines treat it

- **Unreal Engine 5:** physical light units (lux, candela, lumens) with **auto exposure measured from the rendered frame**: a 64-bin
  histogram of the frame's log luminance (default metering), low / high percent 70-80 / 80-95 to ignore the darkest and brightest
  pixels, min / max EV100, an exposure compensation curve by scene EV100, adaptation speed up / down in f-stops per second, an optional
  centre-weighted metering mask. **Local exposure** (base / detail split, highlight and shadow contrast 0.6-1) "should always be set
  up when using Lumen GI": it keeps the bright window and the dim room both readable.
  **Lumen GI**: infinite diffuse bounces (the sun patch lights the room), sky shadowing (interiors darker than outside, as they are),
  warm sunlight bounce. Epic, Fortnite Chapter 4: with correct GI "indoor areas become significantly darker than outdoors ... which
  requires Automatic Exposure to be enabled". "Skylight Leaking" exists only as a non-physical art knob.
- **Frostbite (Lagarde & de Rousiers, "Moving Frostbite to PBR", SIGGRAPH 2014):** physical light units throughout plus a physically
  based camera (EV100, aperture / shutter / ISO) driven by auto exposure, so the same lights work outdoors and in.
- **Unity HDRP:** the same: physical units, histogram auto exposure with a metering mask.
- **Real-time GI without hardware ray tracing:** irradiance probes on a 3D grid, with **visibility per probe** so light does not leak
  through walls (DDGI, Majercik et al. 2019, JCGT; ~6 ms on an RTX 2080 Ti with RT; there are SDF-traced variants without RT).

- **Epic's own Lumen course (Wright et al., "Lumen: Real-time Global Illumination in Unreal Engine 5", SIGGRAPH 2022 Advances), read
  2026-10-21:** "we need multibounce diffuse for indoor scenes" ("without them half of the scene is black"); an unshadowed sky light
  "adds an unnatural blue hue to the interior" (the blue veil seen here on 2026-10-21 with the sky's ambient at 0.08 over the room);
  every ray that misses the scene samples the sky (the sky enters only where geometry lets it); irradiance fields (a probe grid) are
  the "most popular approach" but leak / over-occlude and look flat; Lumen traces from screen probes and caches multi-bounce light on
  the surfaces instead. Fortnite: with correct GI "indoor areas become significantly darker than outdoors", which "requires Automatic
  Exposure". Final Fantasy XV (its SIGGRAPH 2016 abstract, the only public Luminous lighting source found) used a probe grid with PRT.

## 3. What EmberEngine does today, against that (the gate floor, Luz das janelas)

| Piece | Physics / engines | EmberEngine today | Verdict |
|---|---|---|---|
| Sun and moon | real, physical units, the game's clock | real (128 000 lux sun, the clock) | right |
| Sun into the room | through the openings only | shadow-only roof + skirts + plinth | right (geometry) |
| **Bounce light (GI)** | the main light of a room by day | **none**: no probes, no bounce | **missing: why the room is black** |
| Sky through the windows | sky component | `kWindowSkyShare = 0` (zeroed while chasing the blue veil) + 25 small "opening" point lights (my invention: `kSkyShareOfKey` 0.2, reach 3-10) | invented, too weak |
| **Camera** | metered from the rendered frame (histogram), local exposure | **4 % of the outdoor reading** (`kWindowMeterShare`), floor 2.7 lux (`kWindowMeterFloor`): invented numbers | **wrong: the sun patches blow out white, the room stays black** |
| Fire | constant flux, read through the adapted eye | constant flux (lumens, 1/d^2); since today a floor `kFireExposure` (my number) | the floor is a patch over the wrong camera |

## 4. What doing it right means here (not built; needs the user's "faz")

1. **Auto exposure from the frame:** a log-luminance histogram of the HDR scene each frame (compute, 64 bins), percent window, EV100
   limits, speed up / down, centre-weighted mask; replaces the incident meter everywhere, `kWindowMeterShare` / `kWindowMeterFloor`
   go. Then the room sets the camera, the torches read by day, the sun patches are bright but not a white mirror, a cloud changes the
   room as it should. `kFireExposure` becomes unnecessary.
2. **Local exposure** (base / detail split in the tonemap) so the window and the room are both readable, as UE pairs with GI.
3. **Diffuse GI by probes with visibility** (the plan's phase 5): a probe grid over the board, each probe's irradiance gathered by
   rays against the scene (no hardware RT here: SDF / voxel / the depth of the shadow map), updated a few probes per frame; the sun
   patch, the sky through the openings and the torches bounce into the room. The 25 "opening" point lights go.
