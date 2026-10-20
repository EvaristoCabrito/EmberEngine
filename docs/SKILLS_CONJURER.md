# As magias do Conjurador: uma a cada dois níveis do 3 ao 15, mais Marcar Runa e Retornar

EmberEngine's own spells (2026-10-20; the user: "do the same process for these Conjurer spells", the list pasted, Detonar
Invocação in Drenar Vida's place: "o Malrec explode uma das próprias invocações. Todos a até 1 hex dela levam MAG/2×1.8+2D8, e
a invocação some"). Malrec (`conjurer`; the Arcanista and the Necromante are his promotions, `emberClassFamily` → `conjurer`) had
Engine2's five summons, Força Fantasmagórica (T1) and Teia dos Sonhos (T2), nearly all at level 1. These seven come one every two
levels, combat and utility by turns; the tier table (`inn.json` tierUses) opens T2 at 3, T3 at 6, T4 at 8.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `conjurerKind`, `summonsOf`, `ownSummonAt`, `boneLine`, `mirrorAbsorbs`,
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
| 3 · T1 | Troca de Lugar (`placeSwap`) | aliado ou invocação a até 6 (um hex cada) | os dois trocam de lugar; nada mais | alcance cresce com o nível |
| 5 · T2 | Lança de Osso (`boneLance`) | reta de 3 hexes a partir de si, qualquer ângulo | arcano, MAG/2 × 1.3 + 1D8 em todos na reta (os dois lados) | 4 hexes no 12, 2D8 no 15, ×1.5 no 19, 5 hexes no 23, 3D8 no 27, ×1.7 no 28 |
| 7 · T2 | Imagem Espelhada (`mirrorImage`) | aliado a até 6 (ele mesmo também) | 2 cópias ilusórias por 3 rodadas; cada ataque inimigo (golpe ou magia) tem 1 chance em 3 de acertar uma cópia, que some ("Ilusão") | 3 cópias no 16, 4 rodadas no 21, 4 cópias no 27 |
| 9 · T3 | Detonar Invocação (`detonateSummon`) | uma invocação sua a até 6 | ela explode e some; todos a até 1 hex dela (os dois lados) levam MAG/2 × 1.8 + 2D8 arcano | 3D8 no 17, ×2.1 no 20, raio 2 no 24, 4D8 no 26, ×2.4 no 28 |
| 11 · T3 | Elo de Sacrifício (`sacrificeBond`) | ele mesmo (instantânea) | até o fim da batalha, o dano de cada golpe ou magia que o Malrec recebe vai para a invocação dele mais próxima; sem invocação de pé, ele recebe | a invocação recebe 75 % no 19 e 50 % no 27 (o resto some) |
| 13 · T4 | Pesadelo (`nightmare`) | hex a até 6, raio 2, sem linha de tiro | arcano, MAG/2 × 1.5 + 2D8 em cada inimigo; quem dorme pela Teia dos Sonhos leva o dobro; cada atingido de pé tem 25 % de chance de ficar com medo por 1 turno | 3D8 no 18, 35 % no 21, ×1.8 no 24, raio 3 no 25, medo 2 turnos no 27, 4D8 no 30 |
| 15 · T4 | Fortalecer Invocações (`empowerSummons`) | ele mesmo (instantânea; precisa de invocação em campo) | todas as invocações dele: +25 % em todos os atributos (ATK, MAG, DEF, DEX, MOV) por 3 rodadas; os valores inteiros voltam ao fim | +30 % no 20, 4 rodadas no 24, +40 % no 28 |
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
- **Lança de Osso:** violet motes gather into the caster's hand (0.3 s); a lance of bone (a lit solid: tapered, jagged point, knobbed
  butt) flies from the hand along the line with a violet charge round it, shedding motes; each body it passes flashes, bone shards fly
  out and fall, a violet ring opens on the ground; the lance fades a second after the far end.
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

- Learned by level (`grantClassSpells`): a Conjurador / Arcanista / Necromante of level 3 has the Troca, 5 the Lança, 7 the Imagem, 9
  Detonar, 11 the Elo, 13 Pesadelo, 15 Fortalecer. Marcar Runa needs level 5 and T2 uses, Retornar level 7 and T3 uses; both are rows
  of the world map's field-spells menu ("Marcar", "Runas").
- HUD: the socket shows the user's icon; its tooltip the numbers at the hero's level (`spellTooltip`); the sheet shows "Imagem
  Espelhada ×N (R)", "Elo de Sacrifício N%", "Fortalecido +N% (R)" among the conditions.
- Tests: `--promote Malrec=conjurer:25`, `--fx-test <kind>`, the battle script's `summon:Malrec:summonFamiliar` (a familiar on a free hex
  beside him, no uses spent), then `cast:Malrec:detonateSummon:Familiar de Malrec`, `cast:Malrec:empowerSummons:Malrec`,
  `cast:Malrec:mirrorImage:Kael`, `cast:Malrec:placeSwap:Kael`, `cast:Malrec:boneLance:Piqueiro`, `cast:Malrec:nightmare:Piqueiro`,
  `cast:Malrec:sacrificeBond:Malrec`.

## Verified / not verified

See the commit message. Not verified: the look of each effect in the window (the user judges); the balance; an enemy never uses them
(no AI); the runes' rows, Retornar's list and the note typed in the Mochila in the window (captures only); the illusion's one-in-three
and the bond over many blows (the code path, not a statistic).
