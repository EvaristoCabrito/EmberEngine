# As magias do Conjurador: aprendizado na abertura de cada tier, mais Marcar Runa e Retornar

Reflexo Arcano: duration 2 to 6 turns, at most 90 % reflected and 50 % absorbed, the excess taken until the talent opens.
Every number scales on its existing track (docs/SPELLS_SCALING.md); learning earlier does not move any improvement or talent level.

EmberEngine's own spells (2026-10-20; the user: "do the same process for these Conjurer spells", the list pasted, Detonar
Invocação in Drenar Vida's place: "o Malrec explode uma das próprias invocações. Todos a até 1 hex dela levam MAG/2×1.8+2D8, e
a invocação some"). Malrec (`conjurer`; the Arcanista and the Necromante are his promotions, `emberClassFamily` → `conjurer`) learns
the combat spells below when their tier opens in `inn.json` (`tierUses`): T1 at 1, T2 at 3, T3 at 6, T4 at 8 and T5 at 11.
Elo de Sacrifício is T5, costs one T5 use and keeps its battle-long effect. T3 contains Familiar Radiante; T5 contains
Elo de Sacrifício and Detonar Invocação (moved from T3, the user, 2026-10-25: learned at 11, costs a T5 use; its scaling
track keeps its dates). T1 keeps its three combat spells.

**No exclusives (the user, 2026-10-25, later the same day):** Reflexo Arcano is T3 (learned at 6, a T3 use) and Imagem Espelhada T2
(learned at 3), both for every Conjurador. They were Isolde's / Malrec's alone for a few hours (`EmberSpell::hero`, still in the code for
any later exclusive). T3 therefore holds Familiar Radiante and Reflexo Arcano.

**Lâmina Arcana (`arcaneBlade`), Selene's own T1 (the user, 2026-10-26: "magia T1 exclusiva da Selene. Ela acerta o alvo com a adaga
e deixa uma marca arcana nele, que explode causando 1D8 se o alvo se mover"; "criar uma escala até o nivel 10, fazer ela durar mais de
um turno conforme evoluir").** `EmberSpell::hero` "Selene": no other Conjurador learns it. A blow of her weapon (its own roll, hit
chance and mastery, like the Golpe Consagrado) on a foe beside her (range 1, a dagger); a miss is a miss. Landed, it leaves the
**Marca Arcana** (`Conditions::arcaneMark*`): when the marked foe walks (its own walk on its turn), the mark goes off before it acts
(`arcaneMarkInterrupts`, played with the Vigília and the Nuvem de Adagas), 1D8 arcane against its arcane resistance with Selene's MAG
of penetration (`arcaneMarkBurst`, never learned), and is spent. Unburst, it holds through N of the foe's own turns, counted like the
Quebra-Guarda: 1 turn, 2 from level 4, 3 from 7, 4 at 10 (her cap); 1D8 at every level (first values, the user judges). A new blow
on a marked foe lays the mark afresh. If Selene has fallen, the mark does not go off. Not done: a foe moved by another's skill
(thrown, pulled, dragged) does not set it off, only its own walk. Effect `render/vfx/SeleneFx` (3D: the dagger's violet glow, the
cut and the rune drawn on the ground, the rune standing at the marked foe's feet, the burst); icon `content/ui/spells/arcaneBlade.jpg`
(the user's `Attachments/Selene/LaminaArcana.jpg`). Script steps `cast:Selene:arcaneBlade:<foe>`, `markwalk:<foe>`.

**Selene's arms and ATK (the user, 2026-10-26: "she doesnt equip staves only Daggers, which are not normally used by Conjurers, she
gains 1 ATT bonus point on top of the regular gains every level", "adagas na main hand sim", "no nível 1 ela já tem 3 pontos de ATK
adicionais acima do Conjurer normal").** `content/hero-arms.json` (world/Loadout.h `heroArms`): her main hand takes only daggers (the
off-hand daggers: punhal-curvo, katar, adaga-sombria ...), kept in `save.equipment.Selene.mainHand`, never a staff (`InnRules::equipWeapon`
refuses one, `fitsHeroSlot`; the Equipar doll and the Mochila list her daggers for the main hand); with none, her starter `punhal-curvo`.
Her blow is a dagger's, at full damage, melee, no arcane bolt, training the dagger skill (Loadout `equipOwnArms`, BattleExport `ownWeapon`
for a freshly prepared battle). The Interlude gives her the punhal (`content/interlude.json`). ATK: `content/progression.json` Selene =
the Conjurador's gains (Malrec's [3, 0, 3, 1, 1]) + 1 ATK a level, and `baseBonus` +3 ATK at level 1 (`sharesClass`: Malrec stays the
conjurer class's hero). Not done: Vargan's shop still offers staves by class (she cannot wield them).

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `conjurerKind`, `summonsOf`, `ownSummonAt`, `mirrorAbsorbs`,
`bondRedirects`, `emberNumbers`, `castEmber`, the Effect branches of `resolveStep` (illusions, bond, empowerment, the swap shared with
Troca de Posto), the sacrifice before a spell's hits, the fear rider, `spellTooltip`, `spellZone`, `spellAimValid`, `spellArea`),
`Spells.h` (`BattleStep::sacrifice / fearPct / mirror* / bondPct / empower*`), `Battle.h` (`Conditions::mirror* / bondPct / empower*`,
`BattleUnit::empowerBase`), `BattleTurns.cpp` (the rounds' countdown), `BattleSave.cpp` (the snapshot), `engine/world/FieldSpells.*`
(the runes), `engine/world/Inn.cpp` (`bagUsed`: a rune is a slot), `apps/sandbox/InventoryScreen.h` (the rune in the Mochila, its note),
`apps/sandbox/OverworldStage.h` (the field rows, Retornar's list), `engine/render/vfx/ConjurerFx.*` (the effects, 3D),
`render/SpellEffects.cpp` (`castEffect`), `apps/sandbox/MapStage.h` (spots, `syncMirrors`, the swap's flight, `--fx-test`, the script's
`summon` step and checks). Icons: the user's art (`Attachments/ConjurerIcons`) in `content/ui/spells/<kind>.jpg`; the rune item is
`content/ui/spells/rune.png` (Fragmento de Pedra Rúnica Incandescente).

## The formula

The arcane ones use the game's damage formula: floor(floor(MAG / 2) × weight) + dice, never worse than a plain attack, the arcane
resistance, half the caster's MAG of penetration, the staff's arcane bonus; an area's XP rule (the first full, the rest half). The
reach of the aimed ones grows with the level like every other mage's (`reachGrowth`: +1 at 7, +2 at 15, +3 at 23).

| Nível · tier | Magia (kind) | Alvo | Efeito | Escala |
|---|---|---|---|---|
| 1 · T1 | Troca de Lugar (`placeSwap`) | aliado ou invocação a até 6 (um hex cada) | os dois trocam de lugar; nada mais | alcance cresce com o nível |
| 6 · T3 | Reflexo Arcano (`arcaneReflection`) | ela mesma (instantânea) | por 2 turnos dele, a primeira magia inimiga que o atingir volta 40 % para quem a lançou (sem resistência: é a magia dela) e a guarda acaba; o resto ele recebe. Talento Absorção (nível 8): do que não volta, 10 % vira cura em vez de ferir | a trilha (docs/SPELLS_SCALING.md): reflexo até 90 %, duração até 6 turnos, absorção até 50 % |
| 3 · T2 | Imagem Espelhada (`mirrorImage`) | aliado a até 6 (ele mesmo também) | 2 cópias ilusórias por 3 rodadas; cada ataque inimigo (golpe ou magia) tem 1 chance em 3 de acertar uma cópia, que some ("Ilusão") | 3 cópias no 16, 4 rodadas no 21, 4 cópias no 27 |
| 11 · T5 | Detonar Invocação (`detonateSummon`) | uma invocação sua a até 6 | ela explode e some; todos a até 1 hex dela (os dois lados) levam MAG/2 × 1.8 + 2D8 arcano | 3D8 no 17, ×2.1 no 20, raio 2 no 24, 4D8 no 26, ×2.4 no 28 |
| 8 · T4 | Pesadelo (`nightmare`) | hex a até 6, raio 2, sem linha de tiro | arcano, MAG/2 × 1.5 + 2D8 em cada inimigo; quem dorme pela Teia dos Sonhos leva o dobro; cada atingido de pé tem 25 % de chance de ficar com medo por 1 turno | 3D8 no 18, 35 % no 21, ×1.8 no 24, raio 3 no 25, medo 2 turnos no 27, 4D8 no 30 |
| 8 · T4 | Fortalecer Invocações (`empowerSummons`) | ele mesmo (instantânea; precisa de invocação em campo) | todas as invocações dele: +25 % em todos os atributos (ATK, MAG, DEF, DEX, MOV) por 3 rodadas; os valores inteiros voltam ao fim | +30 % no 20, 4 rodadas no 24, +40 % no 28 |
| 11 · T5 | Elo de Sacrifício (`sacrificeBond`) | ele mesmo (instantânea) | até o fim da batalha, o dano de cada golpe ou magia que o Malrec recebe vai para a invocação dele mais próxima; sem invocação de pé, ele recebe | a invocação recebe 75 % no 19 e 50 % no 27 (o resto some) |
| campo · 5 · T2 | Marcar Runa (`markRune`) | o hex onde o grupo está | uma runa (lugar mais próximo, região, hex, dia, nota) como item da mochila do grupo: um slot; quantas quiser, enquanto houver slot; nunca duas do mesmo hex | — |
| campo · 7 · T3 | Retornar (`recall`) | uma runa marcada | o grupo vai direto para o hex da runa; a runa fica; gasta um uso do T3 | — |

Imagem Espelhada and Fortalecer count rounds (`startNewRound`), not the unit's turns; Elo de Sacrifício never ends. A copy struck is
a miss for the attacker ("Ilusão", violet); poison, bleeding and the other riders of a blow that hit the copy do not land. The bond
redirects weapon blows, counters and spell hits; what eats at the unit at its turn start (poison, bleeding, hazards, a Mortalha) stays
its own. Detonar sends the summon's blast to both sides, the summon itself dead before the hits ("Detonado"), so Malrec's own party
beside it is struck too. Pesadelo's sleeper rule folds in the hit's own +25 % on a sleeper (×2 in all), and the sleeper wakes.

The runes: `save.runes` = [{id, place, region, col, row, day, note}]; `InnRules::bagUsed` counts each one. The Mochila lists them as
items ("Runa: <place>", the rune's picture): the sheet shows where it points, holds the note (typed in place) and Jogar Fora discards
it. Retornar's list (the field-spells menu → "Runas") shows place, region, hex, day and the note; a click takes the party there (no
day passes, no rations: a crossing, like Warp's) with the rune's picture over the map. Neither spell exists in a battle (they are
field spells only).

## The effects (version 01 of each, `render/vfx/ConjurerFx`, 3D on the Holy + Combat materials, the Conjurador's violet)

- **Troca de Lugar:** two spirals of violet motes cross between the two hexes on an arc as the cards fly (0.42 s), a flash and a ring
  at each hex as the cards leave and land; a light travelling between them.
- **Imagem Espelhada:** motes orbit the ally while it gathers (0.5 s); a flash and a ring as the copies step out. The copies stand beside
  it for as long as the rules keep them (`syncMirrors`): hooded shapes of hollow violet energy with a brighter heart, facing the eye,
  flanking the unit across the view (left, right, then behind on each side), swaying and breathing, a turning halo at the feet, motes
  drifting off; a struck copy shatters into motes with a flash.
- **Detonar Invocação:** violet energy is drawn into the summon while its shell swells and brightens (0.6 s); the blast: a flash, a
  pressure shell out to the edge, two rings and 20 rays across the ground, ash lifting, violet embers thrown up and out, a strong light;
  a flash and a ring on every body in it.
- **Elo de Sacrifício:** dark motes drawn to the caster's chest and a halo closing round it; a chain of light (a tube) from the chest to
  each summon's with a bright bead running down it at the impact and a ring where it lands; the chains hold 1.3 s and fade.
- **Pesadelo:** a cloud of dark puffs rolls low over the whole area, a violet pool under it, pairs of pink eyes opening and blinking in
  the dark, a pulsing violet light; at the impact a flash and a ring to the edge; on each foe tendrils of shadow (tubes) rise round it,
  a flash, motes; the cloud lifts and thins over 2.4 s.
- **Fortalecer Invocações:** motes gather at the caster's chest, a pulse rings out across the ground at the impact; on each summon as it
  arrives a violet shaft rises, a halo climbs from the feet past the head, motes converge, the ground glows, a light.

## How it reaches the game

- Learned by level (`grantClassSpells`): a Conjurador / Arcanista / Necromante learns Troca at 1, Imagem at 3, Reflexo at 6, Pesadelo and Fortalecer at 8, and Elo and Detonar at 11 (T5). Marcar Runa needs level 5 and T2 uses, Retornar level 7 and T3 uses; both are rows
  of the world map's field-spells menu ("Marcar", "Runas").
- HUD: the socket shows the user's icon; its tooltip the numbers at the hero's level (`spellTooltip`); the sheet shows "Imagem
  Espelhada ×N (R)", "Elo de Sacrifício N%", "Fortalecido +N% (R)" among the conditions.
- Tests: `--promote Malrec=conjurer:25`, `--fx-test <kind>`, the battle script's `summon:Malrec:summonFamiliar` (a familiar on a free hex
  beside him, no uses spent), then `cast:Malrec:detonateSummon:Familiar de Malrec`, `cast:Malrec:empowerSummons:Malrec`,
  `cast:Malrec:mirrorImage:Kael`, `cast:Malrec:placeSwap:Kael`, `cast:Malrec:nightmare:Piqueiro`,
  `cast:Malrec:sacrificeBond:Malrec`.

## Verified / not verified

See the commit message. Not verified: the look of each effect in the window (the user judges); the balance; an enemy never uses them
(no AI); the runes' rows, Retornar's list and the note typed in the Mochila in the window (captures only); the illusion's one-in-three
and the bond over many blows (the code path, not a statistic).

## Invocações das promoções (the user, 2026-10-27)

- **Numen** (`summonFamiliar3`, the old Familiar Titã, same creature and numbers): a T6 the Arcanista (`sorcerer`) and the Necromante share,
  learned at 15 (T6 opens); a plain Conjurador no longer has it (`grantClassSpells`).
- **Cão Zumbi** (`summonZombieDog`): the Necromante's T8, learned at 20 (T8 opens at 18; the user: "espaçar mais o ganho de familiares"); stats x1.25 of the caster (a first value, `kT8SummonScale`).
- **Cervo Azul do Pântano** (`summonSwampDeer`, class `swampBlueDeer`, content/engine2/summons.json): the Arcanista's T8, learned at 20,
  x1.25 of the caster, no spell of its own; comes in the Familiar Radiante's blue summon light (ArmsFx Radiant).
- Coming (the user): **Telesm**, a new defender familiar (T2 for Selene, T3 for the others; art to be scanned), and **Hipóstase**
  (Summon Hypostasis), the ultimate summon of Ember I. Neither is built.
