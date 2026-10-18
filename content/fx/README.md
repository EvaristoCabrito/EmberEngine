# Effects library

Everything about spell and skill effects, in one place. Nothing here is ever deleted or renumbered: a new idea is a
new version with the next number (user, 2026-10-16: "save everything in an organized manner, easy to find and
reference"; "Magic Missile 01, make it serial").

## Where things are

| What | Where |
|---|---|
| The menu (in game) | the **Magias** window in every battle map: each spell's dropdown (OFF, `<name> 01`, `<name> 02`, ...) and **Testar**; **Efeitos soltos** plays any effect image alone. Code: `apps/sandbox/SpellsMenu.h` |
| Which version each spell plays | `content/fx/spell-looks.json`: `heroes` and `enemies` (the menu's "Para"), written when a choice changes. A spell not chosen plays its default: **for heroes, Engine2's own ported effect where one exists** (user, 2026-10-16: Míssil Mágico 02, Bola de Fogo 01, Bênção 01, Veneno Cáustico 01, Força Fantasmagórica 01, Teia dos Sonhos 01, Mãos Flamejantes 02, Sopro Venenoso 02, Cleave 01, Varredura 01 today; `SpellEffects::engine2Look`), else its 01; for enemies its 01 |
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
| Míssil Mágico, Míssil Mágico V2 | 03 | **ours, 3D** (2026-10-18; the user: 02 "too bland"; the heroes' default): a flare and a ring of force at the hand as it lets go; the bolt (white-hot core in a boiling violet shell) flies a curving, swerving path with a trail and two ribbons twisting round it in a helix, rings of force rippling off its wake every 0.08 s, embers shed; on the body a flash, a shell of force, three rings spinning apart like a gyroscope, 28 energy streaks flung out, a ring racing over the ground, motes spiralling up, a violet light | `render/vfx/SpellBolts.cpp` (Arcane) |
| Raio Divino (`divineBolt`) | 01 | **ours, 3D** (2026-10-18): a golden flare and a halo at the hand; a lance of light thrown straight at the target, a beam left hanging behind it, golden feathers drifting off it; on the hit a column of light slams down from the sky onto the struck hex, a radiant burst of rays, a shockwave of gold racing over the whole struck area, each splash hex flaring as it reaches it, motes rising, a warm light | `render/vfx/SpellBolts.cpp` (Divine) |
| Tempestade de Gelo (`iceStorm`) | 01 | **Engine2's own Blizzard2D, its sprites stood in the board** (the enemies' default; spell-looks.json): wind wisps (blizzard-wind-reference), falling ice spikes and their shattering (blizzard-impact-reference), snow dots, the frosted floor; 9 s, three cold lights pulsing where spikes land | `render/vfx/IceStorm.cpp` (version 1) |
| Tempestade de Gelo (`iceStorm`) | 02 | **ours, 3D** (the heroes' default): on Blizzard2D's motes and timings, the floor frosts over from each hex's middle, bands of wind turn round the field, snow swirls down, ice crystals (meshes) fall slanted and shatter into splinters, frost rings and ice dust, the same pulsing light; fades early when the rules drop the field | `render/vfx/IceStorm.cpp` (version 2) |
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
| Mãos Flamejantes, Sopro Venenoso | 02 | **ours** (user: "the cone is reversed"): Engine2's flames shrink and fade as they age and its jet barely opens (+-5 degrees), so the spray read thick at the hands and thin at the target. 02 opens a real cone (+-26 degrees), the flames grow as they travel and fade late, and they rise up instead of drifting along the board. The heroes' default | `render/vfx/BurningHands.cpp` (version 2) |
| Sopro Venenoso (`poisonBreath`) | 01 | the same with Engine2's poison: the poison-v2-main sheet, green colour and dim emissive curves, green lights | `render/vfx/BurningHands.cpp` |
| Cleave (`cleave`) | 01 | **Engine2's own CleaveSweepVFX, ported**: from 0.18 s into the swing a steel-blue crescent blade (bright edge, dim wake) grows across the struck arc at weapon height while a spearhead runs it with a trail and a light; stone debris kicks off each target and a light flashes on it as the arc reaches it | `render/vfx/Sweeps.cpp` |
| Varredura (`sweep`) | 01 | **Engine2's own VarreduraVFX, ported**: a circular pressure wave on the ground (radius 3.2): bright outer ring, glowing inner ring, 42 shards riding it, a light orbiting its edge, a flash on each target as it arrives | `render/vfx/Sweeps.cpp` |
| Investida Perfurante (`piercingThrust`), Golpe Duplo (`doubleStrike`) | 01 | an ember glint on the swing; sparks on every blow that lands | `bladeSkill()`, `strike()` |
| Cura Menor (`cureMinor`) | 02 | **ours, 3D (HolyLight, 2026-10-17)**: a slim golden shaft of light comes down onto the healed unit (seen through, rays rising inside), the ground glows with a widening ring, motes rise round it, a warm real light. The heroes' default | `render/vfx/HolyLight.cpp` |
| Cura Média (`cureWounds`) | 01 | **ours, 3D (HolyLight)**: a taller, wider golden shaft and a halo that settles from above the head to the feet, more motes, a stronger light | `render/vfx/HolyLight.cpp` |
| Mãos Curativas (`cureLight`) | 01 | **ours, 3D (HolyLight)**: a short warm shaft and two cupped arcs of amber light closing round the body, motes drawn inward | `render/vfx/HolyLight.cpp` |
| Curar Doença (`cureDisease`) | 01 | **ours, 3D (HolyLight)**: a pale teal shaft; the sickness leaves the body as dark motes drifting up and away; a clean ring spreads wide on the ground | `render/vfx/HolyLight.cpp` |
| Criar Comida e Água (`createFoodAndWater`) | 01 | **ours, 3D (HolyLight)**: a cool shaft on every fed ally, bread-gold and water-blue motes spiralling into it | `render/vfx/HolyLight.cpp` |
| Poção (`potion`, Itens) | 01 | **ours, 3D (HolyLight)**: a burst of light at the chest, bubbles rising, a splash ring; red-orange to heal, green to cure, blue for mana | `render/vfx/HolyLight.cpp` |
| every landed weapon blow | (always) | an impact spark (Impacto 01-04) on the body, embers on a critical, a warm flash | `strike()` |
| lightning echo (start of the struck unit's turn) | (with Relâmpago) | Relâmpago 01's strike at 0.6 size | `echo()` |

| **EmberEngine's own, the Mago Negro's promotions (docs/SPELLS_PROMOTIONS.md)** | 01 | flipbook cards + lights, `engine/render/SpellEffectsEmber.cpp` (2026-10-17) | |
| Marca da Ruína (`markOfRuin`) | 01 | shadow gathers in the hand, a dark bolt flies, a violet seal closes over the chest and keeps turning while the mark lasts | `markOfRuin()` |
| Praga Rastejante (`creepingPlague`) | 01 | a sick green orb arcs in; a plume, a puddle ring and bubbles on the body | `creepingPlague()` |
| Dreno de Vida (`lifeDrain`) | 01 | red threads pulled from the target's chest to the caster's hand, a red glow swelling on the caster | `lifeDrain()` |
| Mortalha de Sombras (`shroudOfShadows`) | 01 | the cast: a burst of dark smoke at the centre; the zone (standing): dark smoke columns and drifting rings on every hex, a dim violet light | `shroudCast()`, `zoneStart("shroudOfShadows")` |
| Hemorragia (`hemorrhage`) | 01 | a red shadow slash across the body, drops falling to the feet, a red ring | `hemorrhage()` |
| Silêncio Profano (`profaneSilence`) | 01 | a violet wave over the area; rings of runes close over every head and go out | `profaneSilence()` |
| Peste Negra (`blackPestilence`) | 01 | a black-green orb arcs to the centre; plumes, puddles and dark smoke roll out over every hex under a sick light | `blackPestilence()` |
| Maldição da Decadência (`curseOfDecay`) | 01 | a dark pillar round the body, grey motes falling from head to feet | `curseOfDecay()` |
| Colheita de Almas (`soulHarvest`) | 01 | a shadow sheet sweeps the area at waist height; pale souls stream from each struck body to the caster | `soulHarvest()` |
| Vazio Devorador (`devouringVoid`) | 01 | a black point swells over the body drinking the light, collapses into a vortex, shards fly out | `devouringVoid()` |
| Coluna de Fogo (`pillarOfFlame`) | 01 | a pillar of fire on the centre, the ring catching, smoke; then the ground burns (zone `burning`: small flames, glow, a wavering light) | `pillarOfFlame()`, `zoneStart("burning")` |
| Lança de Gelo (`iceLance`) | 01 | a streak of ice along the whole line, an ice eruption on everyone on it | `iceLance()` |
| Tempestade Elétrica (`thunderstorm`) | 01 | a dark cloud over the area, bolts on everyone standing in it and a few empty hexes, smaller echoes a beat later | `thunderstorm()` |
| Meteoro (`meteor`) | 01 | the cast: a red mark on the ground (zone `meteor`, pulsing ring + embers); the fall (`meteorFall`): the rock from the sky with a trail, a column of fire, a wave, bursts on every hex, smoke, a huge flash | `meteorMark()`, `meteorFall()` |
| Zero Absoluto (`absoluteZero`) | 01 | the area crystallises from the centre out: a frost ring runs, ice erupts on every hex, white motes on the bodies | `absoluteZero()` |
| Cadeia de Raios (`chainLightning`) | 01 | a bolt from the hand to the first body, arcs jumping body to body (the bolt art stretched between them), each thinner | `chainLightning()` |
| Erupção (`eruption`) | 01 | the ground cracks round the caster, a ring of fire runs out, flames rise on the rim; the rim keeps burning (zone) | `eruption()` |
| Vendaval Glacial (`glacialGale`) | 01 | gusts of snow rush down the cone, ice dust on every hex, the struck slide back | `glacialGale()` |
| Fúria dos Céus (`wrathOfTheSkies`) | 01 | dark clouds over the targets, four or five columns of lightning almost at once, echoes round them | `wrathOfTheSkies()` |
| Brasa Primordial (`primordialEmber`) | 01 | embers rise over the whole area, an orange-white blast, plumes on every hex; the ground smoulders (zone `burning`, ember) | `primordialEmber()`, `zoneStart("burning")` |

| **EmberEngine's own, the Guerreiro's T4 / T5 (docs/SKILLS_WARRIOR.md)** | 01 | **ours, 3D** (`engine/render/vfx/WarriorFx.cpp`, 2026-10-18): the Combat material's streaks, dust, shells, rings, trails and blade sheets + pieces lit by the scene | |
| Redemoinho de Aço (`steelWhirlwind`) | 01 | a sheet of steel light turns a full circle round the caster at weapon height, white-hot edge with a glint and a light riding it, sparks flung off, a ring of dust, a flash on every foe as the edge reaches it | `WarriorFx::whirlwind` |
| Quebra-Guarda (`guardBreak`) | 01 | the blade comes down (a vertical cut sheet); flash, sparks, armour shards (lit steel plates) burst from the chest and bounce, a crack ring on the ground, an orange light; a broken ring of dull red light turns over the chest for two seconds | `WarriorFx::guardBreak` |
| Grito de Guerra (`warCry`) | 01 | a pressure shell and a ground ring burst out to the radius, dust lifted as they pass; a column of red-gold light with climbing motes on every ally, warm lights; a dark pulse on the frightened foes | `WarriorFx::warCry`, `warCryFear` |
| Salto Devastador (`leapingStrike`) | 01 | the card flies on an arc with a trail of steel light and sparks; the landing is a crater (shell, ground ring, dust ring, chips of ground thrown on arcs, a strong light); then a big downward cut over the foe with sparks | `WarriorFx::leap`, `leapBlow` |

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
| holy light | `game/engine.ts` emitHolyFx | Cura Menor, Cura Média, Mãos Curativas, Curar Doença, potions, food. Its 2D canvas port was tried and set aside (branch `wip/2d-canvas-fx`, not working); the user chose new 3D versions instead: HolyLight above |
| blade swooshes | `game/engine.ts` emitBladeFx (arc, ring, dash, rushTrail, rushImpact, execution, tripSweep, shockRing) | every warrior / lancer / knight skill |
| portals | `game/engine.ts` emitPortalFx / emitWarpFx + `effects/warp-portal-*.png` | summons, Portal |
| ground bursts | `game/engine.ts` emitFireballBurstFx | Bola de Fogo, Veneno Cáustico, Veneno Menor, Sopro Venenoso |
| web floor | `game/engine.ts` (webfloor.png per hex) | Teia dos Sonhos zones |
| sparks, level-up stars | `game/engine.ts` emitParticle, LevelUpSpark | hits, level-ups |
| spell-to-effect table | `vfx/index.ts` SPELL_VFX_FOR, SPELL_ELEMENT | which effect every spell borrows in the preview |
