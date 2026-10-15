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
