# Atmos 2: living atmosphere for Ember (the user's brief, 2026-11-05)

Pasted by the user on 2026-11-05 and built the same night on their order ("voce claramente ignorou isso aqui que cria toda uma
atmosfera real"). The brief itself follows, unchanged, after the status.

## What exists (2026-11-05)

- **Core** `engine/world/Atmos2.h/.cpp` (`ember::atmos2::Core`): the 96 x 40 grid between 78 S and 78 N, two layers (750 / 250 hPa),
  u, v, theta, q, qc per layer, ps, Ts, soil wet, terrain, land per cell, flat `std::vector<double>`, index j * 96 + i, row 0 south,
  longitude wrapping, latitude walled, fixed 180 s steps. `tick()` runs the ten parts of §3 in order: semi-Lagrangian cubic transport
  (x pass then y pass, clipped to the four points' range), hydrostatic pressure gradient (Phi_low = g h + R T_low ln(ps / 750),
  Phi_high = Phi_low + R avg(T) ln 3), exact Coriolis rotation by f dt, 1-day friction on the low layer, mass continuity (ps from the
  divergence of ps V in both layers, the leftover divergence as the vertical flow, ps rescaled to the exact total mass), upwind vertical
  exchange, sunlight from the world clock (tilt 23.44 deg, 365-day year, 24 h day; cloud albedo, ground albedo by land / sea / snow,
  two grey layers as the greenhouse; land 1.2e6 J/m2K, ocean 1.5e7), surface heat and evaporation growing with the wind, convection
  (moist static energy of the low layer against what the high layer holds), phase change with latent heat, rain (part evaporating on
  the way down, the rest wetting the soil), light diffusion + east-west smoothing above 55 deg. No random numbers: `exportState()` /
  `importState()` round-trip bit for bit, two runs from one state give the same bytes. Never compiled with fast-math (MSVC default
  /fp:precise). The forces act before the continuity (a forward-backward step): a plain forward step is unstable for the gravity
  waves of a 180 s step on this grid.
- **Planet**: the brief's "real heightmap" does not exist for Ember's world, so `Core::Terrain` is a deterministic value-noise planet
  (continents ~60 deg across, peaks capped at 2500 m and smoothed twice: §4's warning). The realm sits at the origin of
  `content/atmos2/config.json` (lon -8, lat 46, mid-latitude: the storms move east); a world-map hex (col, row) maps to
  lon = originLon + col * degreesPerCol, lat = originLat - row * degreesPerRow.
- **Starting state**: `content/atmos2/state-v01.bin` = a cold start run 75 simulated days ONCE by `Cooker atmos2-spinup <out> [days]`
  (~500 KB: doubles, so the game continues bit-exactly from the file). In the game the climate only ever advances with the game's
  clock (the user, 2026-11-05: "it doesn't move offline, only in game"): the file is the starting point of §4, nothing more.
- **WeatherService** `engine/world/Atmos2Service.h/.cpp` (`ember::atmos2::Service::instance()`): a worker thread steps the core until
  its newest snapshot is one step past the game clock (the campaign's milliseconds: day 0 00:00 = the shipped state's first step),
  then sleeps; each step publishes a `shared_ptr<const Frame>` (floats per cell: air and surface temperature, humidity, ps, both
  winds, both covers, rain mm/h, convection, soil wet); the last 40 are kept. `sample(worldMs)` reads the four surrounding cells
  bilinearly in the two snapshots around the time and blends by (t - prev) / 180, returning a `WeatherSample` (§6) with the §7 values:
  visibility (40 km dry, falling above 90 % humidity and in rain), traction (1 / 0.6 wet / 0.15 ice), speed of sound, sound
  absorption, conductivity, lightning risk (convection x rain x high cloud; a strike = a hash of (minute, cell)), feels-like (heat
  index above 27 C, wind chill below 10 C), snow (air under 0.5 C), ice. A save far ahead of the state catches up in the background
  (deterministic); the newest snapshot serves meanwhile ("alcançando" in the panel). The clock never runs backwards in the core: a
  test hour behind the newest snapshot gets the oldest kept one.
- **Into the game**: `Service::toTempo()` turns a sample into the `tempo::Sample` every existing reader takes (TempoConsumers'
  `controls()`: the clouds, the rain, the lightning, the fog multipliers, the world map's weather): nothing of the sky, sun, moon,
  stars or cloud code changed; they read the same struct from a different source. `MapStage::tempoNow()` and
  `OverworldStage::worldSampleNow()` use Atmos 2 when it runs and is on, else the seeded Tempo of world/Tempo.h (kept whole as the
  OFF). The world map hands the party's hex to the service (its position). `main.cpp runWindow` starts the service
  (`content/atmos2/state-v01.bin` + `config.json`) and stops it at exit; `--atmos2 N` (-1 never started, 0 OFF, 1 on, the default).
  Panel "Escolhas" (in a map): **Atmos 2**: OFF (Tempo semeado) / viva (default), with the reading now (state, temperature,
  feels-like, humidity, wind, rain, clouds, visibility, step).
- **Precipitation kind from the forecast** (the user, the same night: "cadê chuva fraca, cadê neve, cadê tempestade, vento?"): a map
  with no Clima of its own used to draw "rain" always, at its own wind slider. Now (MapStage, both for Atmos 2 and the old Tempo):
  snow when the air is under 0.5 C, a storm when the rain is heavy or lightning is about, else rain; the intensity from the rain; the
  wind from the forecast's wind speed (20 m/s = full). The world map's TEMPO label says Neve / Tempestade too. Ash stays a map's own
  choice. The sight penalty by weather (chuva 1, tempestade 2, cinzas 3, neve 4; MapStage::sightRadius) reads the map's Clima or the
  Tempo's rain as before.
- **Cooker**: `atmos2-spinup <out.bin> [days]`, `atmos2-test <state.bin> [days]` (§10: repeatable bits, round trip, N days without NaN
  with the strongest wind between 40 and 100 m/s, energy balance within 5 W/m2, moving storms at 45 deg (ps swing over 3 hPa, the low
  centre moving east more than west), speed under 3 ms a step), `atmos2-sample <state.bin> <lon> <lat> [hours]` (the weather at a
  point hour by hour).

## Not built / deviations (say so before calling anything done)

- §8 Vulkan side: no UBO, no 96 x 40 climate texture, no shader reads: this engine's consumers take CPU values through the Tempo
  struct (clouds, rain, lightning, fog multipliers), which is where the brief's values land here. Wet materials (darker albedo, lower
  roughness, ice) and the fog extinction 3.912 / visibility are NOT wired: the fog is never touched (the user's rule), and the
  ground / card materials have no wetness input yet. "The climate's sun direction replaces the engine's day/night cycle": NOT done
  and never will be: the game has one clock and one sky (locked); the climate reads that clock (§3.5) and its own sun direction is
  only in the sample for gameplay.
- §9 multiplayer / lockstep fixed-point: not built (single player; doubles).
- §4 real heightmap: none exists; the procedural planet above.
- **§10 acceptance, measured 2026-11-05 on state-v01.bin (150 spin-up days, then `atmos2-test` 90 days):** repeatable bits PASS,
  state round trip PASS, stable 90 days with no NaN and the strongest wind 74 m/s PASS, speed 1.8 ms a step (worst 3.8) PASS;
  **energy balance FAIL** (absorbed 228, emitted 240 W/m2: 12 apart, the planet still cooling slowly after 240 days; the ocean's
  1.5e7 J/m2K takes longer to settle); **moving storms: not proven** (ps swing 12.5 hPa at 45 deg; the detector follows the
  lowest pressure anomaly hour by hour, which jumps between systems: 345 h east, 347 h west; on a 20-day window from day 75 it
  read 79 h east, 6 h west). Both need another pass; the weather the user sees in the window is the judge.
- The tuning of the physics (cloud scale, rain threshold, exchange coefficients, emissivities) is first values that passed the
  tests; the weather the user sees in the window is the judge.

---

Here's a brief you can give your programmer as-is. It assumes C++17 and your Vulkan engine, Ember.

## Living atmosphere for Ember: implementation brief

### Goal
Weather is computed by physics on a world grid, with no random rolls and no scripted events. The same starting state always produces the same weather. The game only reads from the climate system; it never writes to it.

### 1. Architecture
Build three separate parts:

1. Atmosphere core: a pure C++ class with no engine dependencies. It holds the grid and has `tick()`, `exportState()` and `importState()`.
2. WeatherService: runs the core on a worker thread, publishes snapshots, interpolates between steps, and answers "what's the weather at this world position?".
3. Render and gameplay hooks: one uniform buffer (UBO) per frame, one small texture array, and plain values passed to physics, audio and AI.

### 2. The grid

* Cells: 96 × 40 latitude-longitude cells covering the planet between 78°S and 78°N. Each cell is 3.75° × 3.9°.
* Layers: two, one at about 750 hPa (low) and one at about 250 hPa (high).
* Per cell, per layer: wind east `u`, wind north `v`, potential temperature `θ`, water vapour `q`, cloud water `qc`.
* Per cell, once: surface pressure `ps`, ground or ocean temperature `Ts`, soil moisture `wet`, terrain height, land/sea flag.
* Storage: flat `std::vector<double>`, index `j*96 + i`. Row 0 is the south edge. Longitude wraps around; latitude has walls at the edges.
* Fixed timestep: 180 seconds of world time. Never use a variable dt.

### 3. One step, in order

1. Transport: move every field along the wind using semi-Lagrangian advection with cubic interpolation, done as an x pass then a y pass. Do not use bilinear interpolation. It smears the field so much that storms never form.
2. Forces on the wind:
   * Pressure gradient force from the hydrostatic height of each layer: `Φ_low = g·terrain + R·T_low·ln(1/0.75)`, and `Φ_high = Φ_low + R·avg(T)·ln(3)`.
   * Coriolis: rotate `(u, v)` exactly by the angle `f·dt`, where `f = 2Ω·sin(lat)`.
   * Surface friction on the low layer: time scale about 1 day.
3. Mass continuity:
   * Update `ps` from the divergence of `ps·wind` in both layers.
   * The leftover divergence gives the vertical air flow between the two layers.
   * Rescale `ps` every step so the planet's total air mass stays exactly constant.
4. Vertical exchange: rising air carries heat and moisture from the low layer to the high one; sinking air carries them back down. Use upwind differencing.
5. Sunlight:
   * The sun's position comes from the world clock: axial tilt 23.44°, 365-day year, 24-hour day.
   * Clouds reflect light, the ground absorbs the rest, and the two layers trap infrared radiation like a greenhouse.
   * Land heats fast (heat capacity 1.2e6 J/m²K); ocean heats slowly (1.5e7).
6. Surface exchange: the ground passes heat to the low layer, and evaporation adds vapour. Both grow with wind speed.
7. Convection: if the low layer's moist heat content is higher than what the high layer can hold, mix the two layers in proportion to the difference. This creates thunderstorms.
8. Phase change:
   * If `q` is above saturation, the excess becomes cloud water and releases latent heat.
   * If the air is below saturation and there is cloud water, the cloud evaporates and the air cools.
9. Rain:
   * Cloud water above a threshold falls as rain.
   * Part of the rain evaporates in the low layer on the way down.
   * The rest wets the soil.
10. Smoothing: apply light diffusion to every field, plus extra east-west smoothing above 55° latitude, where the grid cells become very narrow.

A working reference implementation exists for every equation and constant: the web build I made and its C++ port. Ask me for the source and I'll paste it here.

### 4. Starting state

* A cold start takes 60–90 simulated days to settle into a realistic climate.
* Run that once offline on your real heightmap, scaled down to 96×40, and ship the result as a binary state file (about 200 KB).
* Smooth sharp peaks first. Steep terrain makes the pressure maths unstable.

### 5. Threading and time

* Clock: world time = real time × `timeScale`. At 60, one game day lasts 24 real minutes, so the climate steps every 3 seconds. One step costs about 2 ms on one core.
* Worker thread: keeps stepping until the latest snapshot is one step ahead of the game clock.
* Snapshots: after each step, build a compact snapshot (smoothed values per cell) and publish it as `shared_ptr<const Frame>`. Keep the previous one as well.
* Game thread: reads both snapshots and blends them with `blend = (gameTime − prev.time) / 180`. Values move smoothly instead of jumping every step.
* Safety: test the threading under ThreadSanitizer.

### 6. Sampling at the player

* Convert the player's world position to longitude and latitude with an origin point and metres per degree.
* Read the four surrounding cells with bilinear interpolation, then blend the two snapshots.
* Return a `WeatherSample` with: air and surface temperature, humidity, dew point, pressure, wind (low and high), low and high cloud cover, rain in mm/h, convection, and the sun direction.

### 7. Values to derive for gameplay

* Visibility: about 40 km in dry air, dropping sharply above 90% humidity and in rain.
* Traction: 1.0 when dry, about 0.6 when wet, about 0.15 on ice (air below 0.5 °C and a wet surface).
* Speed of sound: `331.3·√(1 + T/273.15)` m/s.
* Sound absorption: higher in dry air.
* Conductivity: from surface wetness and humidity.
* Lightning risk: convection × rain × high cloud. Decide each strike with a hash of (step number, cell), never a random number.
* Feels-like temperature: heat index above 27 °C, wind chill below 10 °C.

### 8. Vulkan side

* UBO: one 128-byte std140 block per frame-in-flight. It holds the sun direction and colour, wind, cloud cover, rain, humidity, visibility, wetness and the blend factor.
* Climate texture: a 96×40 RGBA32F image with 4 layers (previous and current step × two channel sets). Linear sampling, longitude repeats, latitude clamps. Upload only when the step number changes. The shader blends the layers using the UBO's blend value.
* Volumetric clouds: use the cloud cover as the threshold for your detail noise. Low and high clouds are separate decks, and convection raises the cloud tops.
* Fog: extinction = 3.912 / visibility.
* Wet materials: darker albedo and lower roughness; ice is smoother still.
* Sun: the climate's sun direction replaces the engine's day/night cycle. Keep only one clock.

### 9. Saving and multiplayer

* Saving: the save file stores the state array and the step number.
* Multiplayer: the server runs the climate and owns it. A joining client receives the state; after that, each client only gets its own region's samples every few seconds.
* Lockstep: if every client runs the climate itself, the core must use fixed-point maths. `exp` and `pow` give slightly different results on different compilers and CPUs, and the worlds would drift apart.

### 10. Acceptance tests

* Repeatable: two runs from the same state give exactly the same bits.
* Stable: 90 simulated days with no NaN; strongest jet winds between 40 and 100 m/s.
* Energy balance: global sunlight absorbed roughly equals infrared emitted, within 5 W/m².
* Moving storms: at 45° latitude, surface pressure varies by more than 3 hPa over time, and low-pressure centres move east.
* Smooth output: the player sample changes continuously between steps, with no jumps.
* Speed: under 3 ms per step on one core.

Two warnings for whoever builds it:

* The usual way to clean up grid noise (filtering the divergence or the vertical flow) made this model explode in my tests. Use diffusion and polar smoothing instead.
* Don't compile the core with `-ffast-math`, or runs stop being repeatable.
