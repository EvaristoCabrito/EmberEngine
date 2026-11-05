# Atmos 2: living atmosphere for Ember (the user's brief, 2026-11-05; NOT built)

Pasted by the user on 2026-11-05 ("salva a nova como atmos2"). Kept as a future order: nothing of it exists in the game yet.
It starts only on an explicit "faz", one thing at a time. The current systems (world/Tempo.h, the campaign clock, the Map
Editor's Clima, the Atmosfera volumétrica v1 in docs/ATMOSPHERE.md) stay as they are.

Rules already decided that stand above this brief (CLAUDE.md): one sky, sun, moon and clock for the whole game; the moons and
the Andromeda sky are permanent; nothing is reverted, everything builds on top. Item 8's "the climate's sun direction replaces
the engine's day/night cycle" can only mean the climate READS the game's clock (as item 3.5 says), never a second clock.

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
