# Battle grid: saved looks

The user's rule (2026-10-14): every look is saved in a numbered series and kept until the user deletes it after
seeing it live. A change is a new look, never an edit of a saved one.

- See them live: panel "Escolhas" → **Grade** (OFF / antiga / 001 ...) in a Modo teste battle; `--grid-look N`
  (0 OFF, 1 antiga, 2 = look 001).
- Each look: `shaders/grid_lookNNN.hlsli` (frozen; included by terrain.hlsl and painted by the terrain pass, so units,
  trees and houses in front cover it and it never shows on ground not yet seen), its capture here
  (`NNN/*.png`: aldeia015, Kael picked, a foe beside him, cursor on 14,19), git tag `grid-look-NNN`.
- "antiga" = the first C++ grid (Renderer::setGroundMarks: flat fills and rings per hex, Engine2's Battle3D colours).
- Captures: `Sandbox ... --battle-rules content/engine2/battles/aldeia015.battle.json
  --battle-script "near:Piqueiro:Kael; pick:Kael; aim:14:19" --zoom 2.2 --offset 0 0.25 --capture x.png`
  (`near` puts a unit beside another, `pick` hands the turn to a unit, `aim` holds the cursor on a hex,
  `spell:<unit>:<kind>` opens a spell's aim, `attack:<unit>` the weapon's). With the user's game open, add
  `--budget 160` (the RX 6400's 4 GB cannot hold two sprite budgets).
- 001's zones (the user, 2026-10-16: "how do I even know where to target?"): a spell opened shows its whole zone
  (e2::spellZone: range, min range, the shot's line, rays where valid) as the violet region even with nobody in it;
  hexes it may be aimed at right now get a violet lock; "Atacar" opens always and shows the weapon's zone from where
  the unit stands (ember, hazard stripes) plus red locks on foes it can strike after moving. Captures:
  `001/spell-longshot.png` (Neera, Tiro Longo), `001/spell-missile.png` (Voss, Míssil Mágico, a foe locked),
  `001/attack.png` (Kael, Atacar).

| Look | What | Verdict |
|---|---|---|
| 001 | "luminous": signed-distance drawing per hex from a state word (Renderer::GridCell). Movement = ice region: one glowing outline around the whole reach (not per hex), soft inner glow, faint lattice between its hexes, fainter towards the limit of the movement, breathing. Route = flowing chevrons through the hex centres to a turning reticle on the destination. Picked unit = three turning arcs on a circle + hex outline + a pool of light. Foes in reach of a strike = red hex with hazard stripes, outline and four turning brackets (brighter and faster under the cursor). A foe's own movement = ember region with hazard stripes. Spell aim = violet region, the hovered aim's area bright, chosen targets gold rings. Cursor = hex outline with bright corner brackets, breathing; ember with a cross where nothing can stand. Painted in display space (keeps its colours in any light) and capped under the bloom threshold so it never blooms "sparks" over the cards in front (the user's report). | not seen yet |
