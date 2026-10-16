# Effects library

Everything about spell and skill effects, in one place. Nothing here is ever deleted or renumbered: a new idea is a
new version with the next number (user, 2026-10-16: "save everything in an organized manner, easy to find and
reference"; "Magic Missile 01, make it serial").

## Where things are

| What | Where |
|---|---|
| The menu (in game) | the **Magias** window in every battle map: each spell's dropdown (OFF, `<name> 01`, `<name> 02`, ...) and **Testar**; **Efeitos soltos** plays any effect image alone. Code: `apps/sandbox/SpellsMenu.h` |
| Which version each spell plays | `content/fx/spell-looks.json` (written by the menu when a choice changes; a spell not listed plays its 01) |
| Every effect image, numbered | `content/fx/effects-list.md` (generated: `python -I tools/scripts/fx_sprites.py`) |
| The images | Engine2 `public/game/fx` (95) + `public/game/effects` (3), read only; cooked into `build/sprites/battle.pak`, sprite set `fx`, clip = file name without extension (`content/fx.sprites.json`) |
| The spell versions' code | `engine/render/SpellEffects.cpp` (one function per recipe; `SpellEffects::looks()` = how many versions each spell has) |
| How they are drawn | `engine/render/EffectDraw.h`, `shaders/effects.hlsl` (renderer pass "effects") |
| Spells and skills listed | every Engine2 SpellKind (`src/vfx/spell-catalog.ts`): `allSpells()` in `SpellsMenu.h`; names marked `[ours]` there have no Portuguese name in Engine2 |

Test without the menu: `Sandbox --map ... --fx-test KIND|all --scene-time T --capture x.png` (the party's first casts
at the nearest foe every 3 s), `--tatica` (2D view), `--spells-menu-open` (the Magias window opened).

## Spell versions

| Spell (Engine2 kind) | Version | What it is | Code |
|---|---|---|---|
| Míssil Mágico (`magicMissile`) | 01 | flipbook cards: arcane ring and gather at the caster, the arcane orb (Arcano 01) flying in an arch with a spark trail, burst + ring + violet flash on the body | `magicMissile()` |
| Míssil Mágico V2 (`magicMissileV2`) | 01 | the same as Míssil Mágico 01 | `magicMissile()` |
| Relâmpago (`lightning`) | 01 | flipbook cards: lightning ring at the caster; on the target the three bolt images swapped every 50 ms from the sky, the lightning eruption, ring, sparks, blue-white flash | `lightning()`, `bolt()` |
| Relâmpago Maior (`lightningTier3`) | 01 | the same as Relâmpago 01 | `lightning()` |
| Choque (`shock`) | 01 | Relâmpago 01 at 3/4 size | `lightning()` |
| Frost (`frost`) | 01 | flipbook cards: cold on the caster; ice eruption + ring + icy flash on every hex of the line, nearest first | `frost()` |
| Cura Menor (`cureMinor`) | 01 | flipbook cards: gold ring under the caster; ring, pillar of light and motes on the healed unit, warm glow | `cure()` |
| Tiro Longo (`longShot`) | 01 | the arrow (solid, lit by the scene) in a shallow arch, pointing where it flies; sparks on a hit | `arrow()` |
| Tiro Perfurante (`piercing`) | 01 | Tiro Longo 01 flying on through the line | `arrow()` |
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
| MagicMissileV2VFX | `vfx/legacy/three/MagicMissileV2VFX.ts` (+ `game/gfx/three/MagicMissileForeground.ts`) | Míssil Mágico V2 (the 3D one in battle); preview: long/bloody/piercing/multi shot, lightning, shock |
| legacy magic bolt | `game/engine.ts` missileFx / drawMissileFx | Míssil Mágico, arcane bolt (plain attack of casters), Força Fantasma |
| FireballVFX + FireballImpactVFX | `vfx/legacy/three/` | Bola de Fogo |
| OldFireBall | `vfx/legacy/three/OldFireBall.ts` | (older fireball) |
| CausticVenomVFX | `vfx/legacy/three/` | Veneno Cáustico; preview: Tempestade de Gelo, Frost, Veneno Menor |
| BlessVFX | `vfx/legacy/three/` | Bênção; preview: the cures, Raio Divino, Turn Undead, auras |
| PhantasmalForceVFX | `vfx/legacy/three/` | Força Fantasmagórica; preview: summons, Dreno de Vida |
| WebOfDreamsVFX | `vfx/legacy/three/` | Teia dos Sonhos; preview: Provocar, Portal, Presença Intimidadora |
| BurningHandsV2VFX / V3 | `vfx/legacy/three/` | Mãos Flamejantes, Sopro Venenoso (V2 with poison) |
| CleaveSweepVFX, VarreduraVFX | `vfx/legacy/three/VarreduraVFX.ts` | Cleave, Varredura; preview: other blade skills |
| ProceduralElementEmitter + ElementFlipbookAtlas | `vfx/legacy/three/` | the element flipbooks (most of the images); reached from Engine2's effects preview (dev controls) |
| ParticleEmitter (fire) | `vfx/legacy/three/ThreeVfxSystem.ts` | fire particles (fire flipbook) |
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
