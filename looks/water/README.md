# Water: saved looks

The user's rule (2026-10-14): every look is saved in a numbered series and kept until the user deletes it after seeing it
live. A change is a new look, never an edit of a saved one.

- See them live: panel "Escolhas" → **Água** (Engine2 / ember) in a Modo teste battle; `--water N` (0 Engine2, 1 = look 001).
  "Água 3D" → rio follows the board's water look; leito stays Engine2's ThreeWater.
- Each look: `shaders/water_lookNNN.hlsli` (frozen; called by water.hlsl for the board's water and by water3d.hlsl for the
  Map Editor's sheet, which also knows its depth), captures here (`NNN/*.png`, local, git-ignored), git tag `water-look-NNN`.
- "Engine2" = Engine2's diorama water (water.hlsl: #2f86a8, roughness 0.5, the 128 px ripple tile scrolling).
- Captures: `Sandbox --map C:/Engine2/maps/ember/random-rimewater-shrine001.json --ground build/ground/ground-v3.pak --scene
  build/engine2-scenes/random-rimewater-shrine001.scene.json --sprites build/sprites/battle.pak --meshes build/meshes/props-v2.pak
  --background 1 --zoom 3 --offset -0.18 -0.29 --capture x.png` (the pond; `--tilt -15` puts the sky in the frame).

| Look | What | Verdict |
|---|---|---|
| 001 | "ember" (2026-10-19; the user: "preciso de ganhos na água desesperadamente", "aceito novos tiles de água"): no texture tile. Six directional waves (5.2 .. 0.37 units) with analytic slopes, the finest fading where they would alias, plus a drifting noise ripple; the body Engine2's river blue in the shallows deepening to a dark teal with the depth below (Beer's law, where the sheet knows its depth); lit by the contract: sun highlight (GGX, roughness 0.10, a touch rougher far away), the sky's reflection map by Fresnel (clouds, dusk, moon), the sky's light on the body, the cloud shadows; Fresnel decides how much of the ground shows through (steep views see in, grazing ones see sky); shore foam where the depth is known. | not seen yet |
