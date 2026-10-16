# Effects library

Everything about spell and skill effects, in one place. Nothing here is ever deleted or renumbered: a new idea is a
new version with the next number (user, 2026-10-16: "save everything in an organized manner, easy to find and
reference"; "Magic Missile 01, make it serial").

## Where things are

| What | Where |
|---|---|
| The menu (in game) | the **Magias** window in every battle map: each spell's dropdown (OFF, `<name> 01`, `<name> 02`, ...) and **Testar**; **Efeitos soltos** plays any effect image alone. Code: `apps/sandbox/SpellsMenu.h` |
| Which version each spell plays | `content/fx/spell-looks.json`: `heroes` and `enemies` (the menu's "Para"), written when a choice changes. A spell not chosen plays its default: **for heroes, Engine2's own ported effect where one exists** (user, 2026-10-16: Míssil Mágico 02, Bola de Fogo 01, Bênção 01, Veneno Cáustico 01, Força Fantasmagórica 01, Teia dos Sonhos 01, Mãos Flamejantes 01, Sopro Venenoso 01, Cleave 01, Varredura 01 today; `SpellEffects::engine2Look`), else its 01; for enemies its 01 |
| Every effect image, numbered | `content/fx/effects-list.md` (generated: `python -I tools/scripts/fx_sprites.py`) |
| The images | Engine2 `public/game/fx` (95) + `public/game/effects` (3), read only; cooked into `build/sprites/battle.pak`, sprite set `fx`, clip = file name without extension (`content/fx.sprites.json`) |
| The spell versions' code | `engine/render/SpellEffects.cpp` (one function per recipe; `SpellEffects::looks()` = how many versions each spell has) |
| How they are drawn | flipbook cards: `engine/render/EffectDraw.h`, `shaders/effects.hlsl`; Engine2's ported effects: the effect runtime `engine/render/VfxDraw.h` (draw lists), `VfxGeometry.h` (three.js shapes), `vfx/ThreeMath.h` (three.js curves, quaternions, colours), `shaders/vfx.hlsl` (Engine2's GLSL, one material per shader); both in the renderer pass "effects" |
| Spells and skills listed | every Engine2 SpellKind (`src/vfx/spell-catalog.ts`): `allSpells()` in `SpellsMenu.h`; names marked `[ours]` there have no Portuguese name in Engine2 |

Test without the menu: `Sandbox --map ... --fx-test KIND|all --scene-time T --capture x.png` (the party's first casts
at the nearest foe every 3 s), `--tatica` (2D view), `--spells-menu-open` (the Magias window opened).

## Spell versions

| Spell (Engine2 kind) | Version | What it is | Code |
|---|---|---|---|
| Míssil Mágico (`magicMissile`) | 01 | flipbook cards: arcane ring and gather at the caster, the arcane orb (Arcano 01) flying in an arch with a spark trail, burst + ring + violet flash on the body | `magicMissile()` |
| Míssil Mágico V2 (`magicMissileV2`) | 01 | the same as Míssil Mágico 01 | `magicMissile()` |
| Míssil Mágico, Míssil Mágico V2 | 02 | **Engine2's own MagicMissileV2VFX, ported**: the bolt gathers above the caster for 0.95 s (a distorted violet core in a banded shell, three arcane threads), flies a curved seeking path (0.8-1.3 s) trailing a fragmented ribbon, 28 crystal shards and a violet light, and hits with a burst ring, four spinning fracture arcs, swirling shards and a white flash (0.78 s). In battle the hit lands when the bolt arrives, as in Engine2. Drawn over everything (Engine2: depthTest false). Not carried over: its lights' shadows | `render/vfx/MagicMissileV2.cpp` |
| Relâmpago (`lightning`) | 01 | flipbook cards: lightning ring at the caster; on the target the three bolt images swapped every 50 ms from the sky, the lightning eruption, ring, sparks, blue-white flash | `lightning()`, `bolt()` |
| Relâmpago Maior (`lightningTier3`) | 01 | the same as Relâmpago 01 | `lightning()` |
| Choque (`shock`) | 01 | Relâmpago 01 at 3/4 size | `lightning()` |
| Frost (`frost`) | 01 | flipbook cards: cold on the caster; ice eruption + ring + icy flash on every hex of the line, nearest first | `frost()` |
| Cura Menor (`cureMinor`) | 01 | flipbook cards: gold ring under the caster; ring, pillar of light and motes on the healed unit, warm glow | `cure()` |
| Tiro Longo (`longShot`) | 01 | the arrow (solid, lit by the scene) in a shallow arch, pointing where it flies; sparks on a hit | `arrow()` |
| Tiro Perfurante (`piercing`) | 01 | Tiro Longo 01 flying on through the line | `arrow()` |
| Bola de Fogo (`fireball`) | 01 | **Engine2's own FireballVFX, ported**: OldFireBall (a boiling icosphere of fire inside a flaring shell, a fire light) arcs from the caster to the target hex in 1.25-2.5 s; FireballImpactVFX: a flash, then 308 seeded particles (flames, rising flames, a ground ring of fire-flipbook flames, sparks, embers, smoke) held inside the area (2 hexes), with a flaring, flickering light. In battle the hit lands on arrival. As in Engine2, the burst is centred 0.04 above the ground and depth-tested, so the ground cuts its lower half | `render/vfx/Fireball.cpp` |
| Bênção (`bless`) | 01 | **Engine2's own BlessVFX, ported**: a golden core gathers with rising motes (0.3 s), a rippling amber-to-ivory wave ring spreads over 3 hexes (0.55 s); each ally it reaches gets eight golden strands, orbiting motes and a warm light, absorbed in 0.78 s. Lights without shadows | `render/vfx/Bless.cpp` |
| Veneno Cáustico (`causticVenom`) | 01 | **Engine2's own CausticVenomVFX, ported**: a glassy green orb charges (0.38 s), flies an arching bowed path (0.7-1.65 s) as three twisting glass ribbons and seven beads grow behind the head; every struck hex erupts with Engine2's Poison V2 element effect (plume, ground puddle, motes: `render/vfx/ElementSprites`) and six procedural smoke lobes, under a green light, settling over 1.85 s. Not carried over: the glass's clearcoat / transmission (drawn as a lit standard surface), the lights' shadows | `render/vfx/CausticVenom.cpp` |
| Força Fantasmagórica (`phantasmalForce`) | 01 | **Engine2's own PhantasmalForceVFX, ported**: eight tapered indigo tendrils and 260 violet motes swirl around the target's body and are drawn in to a churning core (0.55-0.75 s); at 0.82 s it bursts (rim-lit shell to 2 units in 0.2 s, motes thrown out, a violet light spikes); gone by 1.35 s. In battle the hit lands at the burst. The light casts no shadow | `render/vfx/PhantasmalForce.cpp` |
| Teia dos Sonhos (`webOfDreams`) | 01 | **Engine2's own WebOfDreamsVFX (WebZoneVFX), ported**: after the shot's flight (0.85 s; the shot is Engine2's 2D canvas art, not ported) a net forms over the zone: eight knots on its edge joined by a seeded lattice of 24 tapered threads that grow in, wave and carry pulses, tightening with a binding flash at 1.42 s; glowing knots, 32 shards, a central and two travelling lights; every unit inside wrapped by its own strands. Held while the zone lasts (Testar: until 2.6 s, as Engine2's preview), released over 0.9 s. The units' strands are seeded by their place in the zone (Engine2: by unit id) | `render/vfx/WebOfDreams.cpp` |
| Mãos Flamejantes (`burningHands`) | 01 | **Engine2's own BurningHandsV2VFX, ported**: one ParticleEmitter (80 fire-flipbook flames, Engine2's ThreeVfxSystem simulation: spawn, drag, rise, wiggle, a steady jet toward the struck cells) sprays for 0.72 s and burns out by 2.35 s; five flickering lights (hands + the advancing front). The rules release at 0.56 s | `render/vfx/BurningHands.cpp`, `render/vfx/ParticleEmitter.cpp` |
| Sopro Venenoso (`poisonBreath`) | 01 | the same with Engine2's poison: the poison-v2-main sheet, green colour and dim emissive curves, green lights | `render/vfx/BurningHands.cpp` |
| Cleave (`cleave`) | 01 | **Engine2's own CleaveSweepVFX, ported**: from 0.18 s into the swing a steel-blue crescent blade (bright edge, dim wake) grows across the struck arc at weapon height while a spearhead runs it with a trail and a light; stone debris kicks off each target and a light flashes on it as the arc reaches it | `render/vfx/Sweeps.cpp` |
| Varredura (`sweep`) | 01 | **Engine2's own VarreduraVFX, ported**: a circular pressure wave on the ground (radius 3.2): bright outer ring, glowing inner ring, 42 shards riding it, a light orbiting its edge, a flash on each target as it arrives | `render/vfx/Sweeps.cpp` |
| Investida Perfurante (`piercingThrust`), Golpe Duplo (`doubleStrike`) | 01 | an ember glint on the swing; sparks on every blow that lands | `bladeSkill()`, `strike()` |
| every landed weapon blow | (always) | an impact spark (Impacto 01-04) on the body, embers on a critical, a warm flash | `strike()` |
| lightning echo (start of the struck unit's turn) | (with Relâmpago) | Relâmpago 01's strike at 0.6 size | `echo()` |

All the other spells: OFF only (no version yet).

## Engine2's own spell effects (not ported yet)

Engine2's spell effects are not just its images: most are code (three.js meshes, shaders, curves, instanced
particles, point lights) that also use the images. Each one, when ported, becomes a new numbered version of its spells
(e.g. `Míssil Mágico 02` = Engine2's MagicMissileV2VFX). Source, all in `C:\Engine2\src`:

| Engine2 effect | File | Engine2 used it for |
|---|---|---|
| MagicMissileV2VFX | `vfx/legacy/three/MagicMissileV2VFX.ts` (+ `game/gfx/three/MagicMissileForeground.ts`) | Míssil Mágico V2 (the 3D one in battle); preview: long/bloody/piercing/multi shot, lightning, shock. **Ported: Míssil Mágico 02** (the Foreground variant not yet) |
| legacy magic bolt | `game/engine.ts` missileFx / drawMissileFx | Míssil Mágico, arcane bolt (plain attack of casters), Força Fantasma |
| FireballVFX + FireballImpactVFX | `vfx/legacy/three/` | Bola de Fogo. **Ported: Bola de Fogo 01** |
| OldFireBall | `vfx/legacy/three/OldFireBall.ts` | (older fireball) |
| CausticVenomVFX | `vfx/legacy/three/` | Veneno Cáustico; preview: Tempestade de Gelo, Frost, Veneno Menor. **Ported: Veneno Cáustico 01** |
| BlessVFX | `vfx/legacy/three/` | Bênção; preview: the cures, Raio Divino, Turn Undead, auras. **Ported: Bênção 01** |
| PhantasmalForceVFX | `vfx/legacy/three/` | Força Fantasmagórica; preview: summons, Dreno de Vida. **Ported: Força Fantasmagórica 01** |
| WebOfDreamsVFX | `vfx/legacy/three/` | Teia dos Sonhos; preview: Provocar, Portal, Presença Intimidadora. **Ported: Teia dos Sonhos 01** |
| BurningHandsV2VFX / V3 | `vfx/legacy/three/` | Mãos Flamejantes, Sopro Venenoso (V2 with poison). **Ported: V2 = Mãos Flamejantes 01 / Sopro Venenoso 01**. V3 is the same code as V2 without the poison option (same settings, same simulation): it would look identical in play, so it is not a separate version |
| CleaveSweepVFX, VarreduraVFX | `vfx/legacy/three/VarreduraVFX.ts` | Cleave, Varredura; preview: other blade skills. **Ported: Cleave 01, Varredura 01** |
| ProceduralElementEmitter + ElementFlipbookAtlas | `vfx/legacy/three/` | the element flipbooks (most of the images); reached from Engine2's effects preview (dev controls). **Version 2 presets ported** (`render/vfx/ElementSprites`: the three sheets composed as sprites; used by Veneno Cáustico 01); version 1 presets (particle emitters) not yet |
| ParticleEmitter (fire) | `vfx/legacy/three/ThreeVfxSystem.ts` | fire particles (fire flipbook). **Ported** (`render/vfx/ParticleEmitter`, stepped at 1/60 s from its start) |
| EffectsRenderer + WebGL2DRenderer shaders | `vfx/legacy/two-d/` | element shader bursts (acid, holy, ...), Frost, Blizzard (Tempestade de Gelo), the web shot |
| Frost2D, Blizzard2D | `vfx/legacy/two-d/` | Frost, Tempestade de Gelo |
| ProvokeVFX, TurnUndeadV1-V4, ApparitionLightningV1 | `vfx/legacy/two-d/` | Provocar, Expulsar Mortos-Vivos (4 versions), an apparition's lightning |
| lightning strikes | `game/engine.ts` emitLightningFx / drawLightningFx | Choque, Relâmpago, Relâmpago Maior, Raio Divino (+ splash) |
| holy light | `game/engine.ts` emitHolyFx | Cura Menor, Cura Média, Mãos Curativas, Curar Doença, potions, food |
| blade swooshes | `game/engine.ts` emitBladeFx (arc, ring, dash, rushTrail, rushImpact, execution, tripSweep, shockRing) | every warrior / lancer / knight skill |
| portals | `game/engine.ts` emitPortalFx / emitWarpFx + `effects/warp-portal-*.png` | summons, Portal |
| ground bursts | `game/engine.ts` emitFireballBurstFx | Bola de Fogo, Veneno Cáustico, Veneno Menor, Sopro Venenoso |
| web floor | `game/engine.ts` (webfloor.png per hex) | Teia dos Sonhos zones |
| sparks, level-up stars | `game/engine.ts` emitParticle, LevelUpSpark | hits, level-ups |
| spell-to-effect table | `vfx/index.ts` SPELL_VFX_FOR, SPELL_ELEMENT | which effect every spell borrows in the preview |
