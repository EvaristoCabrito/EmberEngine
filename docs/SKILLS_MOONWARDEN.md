# Garran Holt: Guardião da Lua (Moonwarden)

The user, 2026-11-13: the design "Werewolf — Human Form" (10 skills, tiers 1–5, sword and shield), the name "Garran Holt — Moonwarden",
then "Guardião da Lua em Português, add him". This is his human form only; the werewolf form is not in the game yet.

His loop: take the hit meant for an ally, then punish the enemy that committed to it. Each tier gives him one blow and one guard.

## Where it lives

- Data: `content/ember-classes.json` `heroClasses.moonwarden` (the Guerreiro's stats, the Conjuração Média tier table, one-handed swords,
  a shield in the off hand, no Guerreiro skill), `content/data/weapons.json` / `inn.json` (every one-handed sword, every shield and piece
  of armour the Guerreiro uses; the class name, the tier uses), `overworld.json`, `class-stats.json`, `classes.json`,
  `action-points.json` (`spells.byKind`: each technique's AP), `content/progression.json` (Garran Holt: the Guerreiro's gains).
  Written by `tools/scripts/data/add_moonwarden_class.py` (running it again changes nothing).
- Rules: `engine/world/SpellsMoonwarden.inc` (the moon, the aims, the casts, Interpor's redirect, the reductions, the barrier, the
  immunity, the words), included by `SpellsVanguard.inc`: the same technique system as Kael's (`kTechniques`, ranks, cooldowns, one use
  of the tier). Hooks: `Spells.cpp` (`resolveStrikeImpl`, `resolveStep` / `resolveStrike`, the damage sites for the barrier),
  `Battle.h` (`Conditions::Martial` ward fields, `Battle::moonPhase`), `BattleSave.cpp`.
- Map: `MapStage.h` hands the battle the moon it shows (`tonightsMoon()`), runs Interpor as an attack step starts, plays his special pose
  (specialAttack, else attack2, else cast) and a moonlight impact (`CombatPalette::of("moon")`) on his blows and guards.
- Party: Garran Holt is on the Party screen's roster (`Overworld::kPartyHeroes`, `content/ember-classes.json` "heroes"), on the
  reserve in Modo teste (`kTestReserve`) at the test level, for the user to pick into the six. Not on any map's spawn list.
- One code path: no test-mode branch anywhere.

## The moon

Read from the world's clock as the map shows it (the real sky's phase; else the panel's / the campaign day's / the map's Lua; else the
full moon). Nova = new; Crescente = waxing crescent, first quarter, waxing gibbous; Cheia = full **and the blood moon**; Minguante =
waning gibbous, last quarter, waning crescent. A technique reads it once, as it commits, and keeps it (an armed Interpor keeps the moon
paid for). The skills window says what tonight's moon does to each technique.

## Common rules

- Damage: a share of an ordinary blow of his sword. Ranks: one a level from the level learned, 15 ranks (Kael's rule).
- Cooldown N blocks it for his next N turns. His guards and covers last until his next turn begins.
- Bundled steps are paid by the technique's AP; the hex is checked again as it resolves.
- Several of his reductions on one hit (Interpor's, Corte Guardado, Erguer o Escudo, Afronta, Segurar a Linha): the strongest applies, only
  its charge is spent. Kael's guards stay apart.
- Golpe Esmagador's penalty: the foe's next attack spends it (landed or not); the same action's other hits of that skill count too.

## Técnicas (T1–T5 of the Média table: 1, 4, 7, 10, 13)

| Tier · nível | Técnica (kind) | PA | Recarga | 1º → máx. posto |
|---|---|---|---|---|
| T1 · 1 | Corte Guardado (`guardedCut`) | 6 | — | 100 → 120 %; acertou: o próximo golpe físico de arma nele -10 → -15 % |
| T1 · 2 | Erguer o Escudo (`brace`) | 4 | 1 | os próximos 2 golpes físicos de arma nele -25 → -40 % cada |
| T2 · 4 | Pancada de Escudo (`wardenBash`) | 6 | 1 | 60 → 85 %, empurra 1 (bloqueado: o dano vem mesmo assim) |
| T2 · 5 | Interpor (`interpose`) | 5 | 1 | aliado ao lado: o 1º ataque corpo a corpo de alvo único nele vem para Garran (ainda ao lado e ao alcance do atacante); rola contra ele, pode errar |
| T3 · 7 | Estocada na Brecha (`gapThrust`) | 7 | 1 | 105 → 130 %, ignora 20 → 35 % da armadura |
| T3 · 8 | Afronta (`affront`) | 4 | 2 | inimigo a até 3: o 1º golpe de arma de alvo único dele em outro do grupo -25 → -40 %; em Garran, dano cheio |
| T4 · 10 | Golpe Esmagador (`crushingBlow`) | 8 | 2 | 120 → 150 %; acertou: o próximo ataque de arma dele -20 → -30 % em todos os alvos |
| T4 · 11 | Segurar a Linha (`holdTheLine`) | 6 | 3 | até 2 aliados ao lado: o próximo golpe corpo a corpo de arma em cada um -20 → -35 %, se ainda ao lado dele |
| T5 · 13 | Acerto de Contas (`reckoning`) | 9 | 3 | 130 → 160 %; +35 → +50 pontos se ele deteve esse inimigo (Interpor, Afronta, Segurar a Linha) desde o início do turno anterior; máx. 210 % |
| T5 · 14 | Inquebrável (`unbreakable`) | 6 | 4 | com 35 % da vida ou menos: barreira de 20 → 30 % da vida máxima, imune a novo atordoamento e enraizamento |

## O que a lua muda

| Lua | Técnicas |
|---|---|
| Nova | Erguer o Escudo cobre 3 golpes. Estocada na Brecha alcança um inimigo a 2 hexes com um passo antes. Segurar a Linha cobre golpes à distância também. |
| Crescente | Corte Guardado: um passo para o lado depois de acertar, ainda ao lado do mesmo inimigo. Afronta: quando reduz um golpe, o próximo passo do inimigo custa +1 PA. Acerto de Contas: um passo antes do golpe. |
| Cheia (e sangrenta) | Corte Guardado +20 pontos contra quem o acertou com arma desde o início do turno anterior. Pancada de Escudo: o próximo contra-ataque do alvo não vem (o desta pancada incluído). Golpe Esmagador -30 → -40 %. |
| Minguante | Erguer o Escudo: o primeiro empurrão nele anda 1 hex a menos. Interpor: o golpe redirecionado -20 → -30 %. Inquebrável: um aliado ao lado escolhido ganha barreira de 10 → 15 % da vida máxima dele. |

## How a pick works on screen

- A step technique (Estocada na Brecha nova, Acerto de Contas crescente): first the hex to step to (his own hex = no step), then the foe.
- Corte Guardado crescente: first the foe, then the sidestep's hex (his own = no step). The step comes only after a landed hit.
- Segurar a Linha: one pick per ally beside him, two at most (one when only one stands beside him).
- Inquebrável: his own hex; under the waning moon, click an ally beside him instead to give the ally a barrier too.

## Decided here (defaults, the user can change them)

- The blood moon counts as the full moon. A battle with no moon told (no sky, no Lua) is the full moon.
- "Pull": the game has no pull yet; Erguer o Escudo shortens pushes (Pancada / Golpe de Escudo's push, Investida, the techniques' pushes).
- Roots: the Peso Sepulcral's root is the root Inquebrável stops; stun is the game's `stunned`.
- Corte Guardado's guard comes on a hit that leaves the foe standing (a killing blow lays no guard).
- In the campaign nobody recruits him yet (no join mission); Modo teste has him. His sprite is the Guerreiro's until the user's own
  sheets come in.

## Look

- Sprites: the user's temporary sheets (`content/sprites/moonwarden-v01/human`, set `moonwarden-v01`, hero `garranHolt`): idle, walk,
  attack, special (his strike techniques), block (his guards), hit, fall.
- Effects: `engine/render/vfx/MoonwardenFx` (moonlight, ours, 3D), one look per technique, played from `MapStage` as the step lands.
- Icons: `content/ui/spells/<kind>.jpg`, from the user's `Attachments/Icons` (98gm6 Corte Guardado, 72aWB Erguer o Escudo, XunVQ
  Pancada de Escudo, U6vCA Interpor, VyGHz Estocada na Brecha, Cg1KH Afronta, sNZUZ Golpe Esmagador, zFOt6 Segurar a Linha, sz9Xs
  Acerto de Contas, Y9dBy Inquebrável).

## Not done

- Sounds of his own; the werewolf form (its sheets are in `content/sprites/moonwarden-v01/werewolf`, unused).
- The werewolf form.
- An enemy never uses them (no AI).
