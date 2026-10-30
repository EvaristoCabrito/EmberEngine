# Water: saved looks

The user's rule (2026-10-14): every look is saved in a numbered series and kept until the user deletes it after seeing it
live. A change is a new look, never an edit of a saved one.

- **The default water is look 003** (the user, 2026-10-26: "eu quero essa da foto como padrão mas manter a original no jogo"): every
  water hex draws 003 unless the map paints it otherwise (Map Editor → Terreno → Água → Versões: "Água 01" / "Água 02" / "Água 03" /
  "Água original" = tile variants 31..34, BoardGround.h `waterLookOfVariant`; the ground under them is the 001 tile's). The original
  stays in the game as that version and as a panel choice.
- See them live: panel "Escolhas" → **Água** (padrão / original / 01 / 02 / 03) in a battle; `--water N` (0 padrão = as the map has
  it, 1 the original, 2..4 = looks 001..003). One choice for every water of the map (the user, 2026-10-26: "eu quero todas no mesmo
  lugar"). **The game's water is its water tiles only (the user, 2026-10-30: "water 3D não deveria existir, só a água normal do
  jogo"):** the Map Editor's painted sheet (the old Água 3D brush: world/Water3D, shaders/water3d.hlsl, the editor's brush, Cooker
  check-water3d) was removed from the game that day; a map's `waterLevels` / `waterFootprints` / `waterPatches` are ignored.
- Each look: `shaders/water_lookNNN.hlsli` (frozen; called by water.hlsl for the board's water), captures here (`NNN/*.png`, local,
  git-ignored), git tag `water-look-NNN`.
- "Engine2" = Engine2's diorama water (water.hlsl: #2f86a8, roughness 0.5, the 128 px ripple tile scrolling).
- Captures: `Sandbox --map C:/Engine2/maps/ember/random-rimewater-shrine001.json --ground build/ground/ground-v3.pak --scene
  build/scenes/random-rimewater-shrine001.scene.json --sprites build/sprites/battle.pak --meshes build/meshes/props-v2.pak
  --background 1 --zoom 3 --offset -0.18 -0.29 --capture x.png` (the pond; `--tilt -15` puts the sky in the frame).

| Look | What | Verdict |
|---|---|---|
| 001 | "ember" (2026-10-19; the user: "preciso de ganhos na água desesperadamente", "aceito novos tiles de água"): no texture tile. Six directional waves (5.2 .. 0.37 units) with analytic slopes, the finest fading where they would alias, plus a drifting noise ripple; the body Engine2's river blue in the shallows deepening to a dark teal with the depth below (Beer's law, where the sheet knows its depth); lit by the contract: sun highlight (GGX, roughness 0.10, a touch rougher far away), the sky's reflection map by Fresnel (clouds, dusk, moon), the sky's light on the body, the cloud shadows; Fresnel decides how much of the ground shows through (steep views see in, grazing ones see sky); shore foam where the depth is known. | not seen yet |
| 002 | "correnteza" (2026-10-23; the user: "we really need better seamless moving water in this game"; the original's 128 px ripple tile is cut by `frac` every 4 units, so its edges show as straight seams, and it only slides): nothing tiles. Ten waves on incommensurate wavelengths (2.4 .. 0.09 units) and directions, each at its real speed (deep-water dispersion, omega = sqrt(g k)), their fronts bent by one slow warp field drifting with the current; two octaves of gradient noise on an integer hash lattice drifting down the current at two speeds; the flat normal is the world's up (the sheet's triangles never show as bands); waves finer than ~3 px fade and their slope variance widens the highlight (roughness) instead of sparkling. Body, see-through and shore foam as 001, the foam drifting with the current. One fixed current direction for every map (0.61 rad from +x, 0.16 units / s). | not seen yet |
| 003 | "decente" (2026-10-26; the user: "agua decente número 2 ??? seamless with motion", "eu não quero correnteza eu quero uma água que não seja uma bosta completa, faz a 03"): the first look that is the water BETWEEN the eye and the bed instead of a sheet painted over the ground. The Renderer copies the scene (ground, 3D pieces) before the water and samples the depth (only while a sheet draws look 003: `Renderer::waterRefracts`; every other look keeps one pass). Refraction: the ray through the wave normal (1.333) lands on the bed and the bed is read there (a read landing on a bank or a piece above the water takes the straight one). The real path through the water from the depth: the bed fades by Beer's law (extinction 1.45 / 0.80 / 0.70 per unit: red first), the water's own lit green-teal body (0.030, 0.085, 0.080) takes its place; at the shore the path is 0, so the water ends without an edge. Caustics: two nets of moving cells on the bed, with the sun's height, the surface's shadows and the depth. Surface: the sky's reflection and the sun's glint by Fresnel. Motion: three long swells at their real speed and five octaves of flow noise (gradients turning in place, each octave turned, drifting down the current, riding the slope of the ones before): no tile, no stiff sliding sheet. A thin broken foam band where the water is a few centimetres deep. Cost: one full-screen copy + the water pass (not measured yet). | not seen yet |
