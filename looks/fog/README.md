# Fog of war mist: saved looks

The user's rule (2026-10-14): every look is saved in a numbered series and kept until the user deletes it
after seeing it live. A change is a new look, never an edit of a saved one.

- See them live: `Sandbox --map ... --fog 1 --scout --moving-light --spell-light --fog-look N`; in a window,
  F9 cycles all looks (the Map panel shows the number and note).
- Each look: `shaders/fog_lookNNN.hlsl` (frozen), its settings and noise in `engine/render/FogLooks.cpp`,
  its capture here (`NNN/capture.png`: aldeia015, scout at 12.6 s), git tag `fog-look-NNN`.

| Look | What | Verdict |
|---|---|---|
| 001 | thick puffy billows, self-shadowed, white | user: too puffy, cartoony, hard edges |
| 002 | soft even grey sheet | user: looks fake |
| 003 | soft banks and gaps | user: kept ("don't throw it away") |
| 004 | Engine2 fog1 model at full intensity: an opaque slab | not seen |
| 005 | 004 + Claude's swirl round the party: an opaque slab | not seen |
| 006 | Engine2 mist4 as in the user's reference (vortex round the board, clear centre, intensity 0.3) | not seen |
| 007 | Claude's swirl round the party on mist4's real values, fog-of-war coverage | not seen |
| 008 | 003 thinning away near the camera (nothing from 2 to 14 units out, then 003's bank): no white / yellow veil "ping-ponging" over the screen when the camera comes close and its eye drops inside the bank (wisp-forest-crossing, the user, 2026-10-16). Default since then; 003 stays in the list | not seen |
| 010 | a map's own fog, no fog of war (MapDraft::emberWindFog; A Ponte de Pedra 023): a pool on the gorge floor and a wind sheet at deck height streaming past the walls, wind along the gorge with gusts, the deck clear. Panel "Névoa do vento" OFF / forte, `--wind-fog N`; not in the "Névoa" list | not seen |
