# Sky: saved looks (sun, volumetric clouds, glare, rays)

The user's rule (2026-10-14): every look is saved in a numbered series and kept until the user deletes it after seeing it
live. A change is a new look, never an edit of a saved one. The user's demand (2026-10-19): "um céu inteiro com 24 horas de
transição com sol, lua, tudo que existe na atmosfera, perfeição total, triple AAA"; "cadê o céu do Final Fantasy XV?".

- See them live: panel "Escolhas" on an outdoor map (light mode real) → **Sol** (real / cinema / 001), **Nuvens** (OFF /
  básica / volumétricas), **Clarão** (OFF / 001), **Raios de luz** (OFF / 001), **Hora** (the 24 h cycle), with **Fundo** = céu
  (or a panorama) and the camera tilted down to its lowest so the sky is in the frame.
  CLI: `--sun-disc 2 --clouds 2 --glare 1 --shafts 1` (the defaults), `--hour N`.
- Captures here: `NNN/*.png` (local, git-ignored); git tag `sky-look-NNN`.
- Capture line (aldeia015, the sky in the frame):
  `Sandbox --map C:/Engine2/maps/ember/aldeia015.json --ground build/ground/ground-v4.pak --scene
  build/engine2-scenes/aldeia015.scene.json --sprites build/sprites/battle.pak --meshes build/meshes/props-v2.pak --fog 0
  --background 1 --tilt -15 --hour N --capture x.png` (N: 3 amanhecer, 4 manhã, 5 meio-dia, 6 tarde, 7 pôr do sol, 8 crepúsculo,
  9 noite, 10 madrugada).

| Look | What | Verdict |
|---|---|---|
| 001 | **Sol** (`atmosphere.hlsli sunLook001`): a 0.9° disc at 60 × the sun's light through the air (white, blooming, red at the horizon) with the real limb darkening; a corona falling as 1 / angle² beyond the rim; the air's aureole (a narrow Mie lobe, g 0.96, on the Mie optical depth along the view: wide and soft, strongest low in the sky); the 24 h moon takes the same aureole, and the night moon's disc a halo. **Nuvens volumétricas** (`clouds.hlsli` density, `cloudsvol.hlsli` march, `cloudsvol.hlsl` passes): a 3D layer 380–640 units over the mean ground, density = Perlin-Worley base (128³, tiling) shaped by the sheets' weather (cover, a cloud-type noise: stratus low and flat to tall cumulus) and eroded by Worley detail (32³); marched at half resolution, 40 steps, no jitter; lit by the sun (and the cycle's moon) through 6 light taps with Wrenninge's three multiple-scattering octaves (silver linings), the powder term, the sky's irradiance as the fill (bright tops, dark bases), the lit ground from below; hazed into the sky with the distance. The same density gives the reflection map (12 steps) and the cloud shadow map on the board (8 taps). **Clarão + Raios de luz** (`sunglare.hlsl`, half resolution, added before the tonemap): the light's brightness read off the scene round its pixel (behind a cloud or a hill it is gone); 40-tap radial rays from everything brighter than the lit scene (the aureole, the lit cloud rims), cut by the clouds and the hills; a veiling glow, a six-ray starburst, two ghosts and a halo ring on the lens axis, in the light's own colour; the moon gets the same at night. | not seen yet |
