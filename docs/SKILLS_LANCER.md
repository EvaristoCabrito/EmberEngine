# As habilidades do Lanceiro: T1 a T5 completos

EmberEngine's own skills (2026-10-18; the user: "pode aproveitar e criar as skills de Lancer que faltam? primeiro me informar do
seu plano", then, after several lists, the one the user drew the icons for: "Faz o conjunto dos ícones: Arremesso, Gancho,
Salto de Vara, Desarmar, Troca de Posto, Empalar e Arrastar, Vigília da Sentinela (mecânicas de tabuleiro, elemento só nos
efeitos)"). The Lanceiro (`aldric`, Aldric; rules class `lancer`) has Engine2's Investida Perfurante (T1), Varredura (T2) and
Rasteira (T3); its tier table (`inn.json` tierUses) opens T1 at 1, T2 at 4, T3 at 8, T4 at 11, T5 at 15. These seven make
two of every tier but T3 (one of T1-T3 each, two of T4 / T5). The Sentinela and the Templário (its promotions) keep them.
The user's rule for the set: no mechanic repeats one that already exists (the Guerreiro's leap, the pushes of Varredura /
Investida, the fall of Rasteira / Tiro Paralisante, the bleeds, the auras); every one moves the board.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `lancerKind`, `emberNumbers`, the plans `tossPlan` / `hookPlan` /
`vaultLanding` / `swapAlly` / `dragPlan`, `castEmber`, the riders in `resolveStrikeImpl` (rear sure, Desarmado) and in the
spell-step resolve (the throw, the collision, Desequilibrado, the drag), the Effect steps (the swap, the vigil),
`strikesBack`, the countdowns in `startOfTurnEffects`, `conditionsText`, `spellTooltip`, `vigilInterrupts`), `Spells.h`
(`BattleStep::throwTo / arc / collideWith / dragTo / rearSure / disarm* / stagger* / vigil*`, `BattleEffect::arc / delay`),
`Battle.h` (`Conditions::disarm* / staggered / vigil*`, `disarmBase`), `BattleSave.cpp`, `engine/render/vfx/LancerFx.h/.cpp`
(the effects, 3D), `render/SpellEffects.cpp` (`castEffect`, `vaultFlight`, `lancerFlight`, `swapFlight`, `disarmed`),
`apps/sandbox/MapStage.h` (`animateFlights`: the cards' flights, `playInterrupts` + the enemy turn's check, `syncVigils`,
`--fx-test`, the battle script's `cast` aims and checks, the `vigil` step). Icons: the user's art (`Attachments/LancerIcons`)
in `content/ui/spells/<kind>.jpg` (784 x 1168, centre-cropped in the HUD socket).

## The formula

A lance skill: the lance's own roll (ATK, mastery, hit chance against DEX, the 8 % critical, the rear bonus) times the skill's
weight, plus the skill's bonus dice (`dmgMul` + `weaponBonusDice` on a spell-type step: no counter; `bonusDice` on a Combat
step). MAG plays no part. A miss is a miss (the uses and the turn are spent, nobody moves). The board moves happen in the
rules at once; the cards fly to their hexes (`unitLeap_`, an arc per move) and the numbers that depend on a flight (a
collision) wait for it.

| Tier · nível | Habilidade (kind) | Alvo | Dano | Efeito | Escala |
|---|---|---|---|---|---|
| T1 · 2 | Arremesso (`spearToss`) | um hex a 1–2 de um inimigo vizinho (a direção), sem passar por você | ×0.9 da lança | o inimigo **voa** até lá, parando onde há obstáculo; se cair sobre outra unidade, **as duas levam 1D6 de colisão** (sem resistência) | +1D4 e ×1.0 no 8, 3 hexes e colisão 2D6 no 14, +1D6 no 16, ×1.1 no 20, +2D6 no 24, colisão 2D8 no 26 |
| T2 · 4 | Gancho (`hookPull`) | 1 inimigo a 2–3 hexes, linha de tiro livre, um hex livre na reta | ×0.8 da lança + 1D4 | o inimigo é **puxado** até o hex livre mais perto de você na reta; **Desequilibrado**: não contra-ataca até o próximo turno dele | 4 hexes no 12, ×0.9 no 15, 1D6 no 18, ×1.0 no 24, Desequilibrado por mais um turno a partir do 24, 5 hexes no 26, 2D6 no 28 |
| T3 · 8 | Salto de Vara (`poleVault`) | 1 inimigo vizinho com o hex atrás dele livre | ×1.2 da lança + 1D6 | ele **salta por cima** e cai nas costas do inimigo; o golpe é **pela retaguarda** (acerto +10, o inimigo vira de costas); sem contra-ataque; o salto não gasta movimento | ×1.4 no 14, ×1.6 e 2D6 no 20, ×1.8 no 26, 2D8 no 29 |
| T4 · 11 | Desarmar (`disarm`) | 1 inimigo ao alcance da lança | golpe de lança + 1D4 | **Desarmado**: −40 % ATK e não contra-ataca por 2 turnos (ele contra-ataca o próprio Desarmar se a lança errar) | 1D6 e 3 turnos no 17, −50 % no 22, 2D6 no 27, −60 % no 29 |
| T4 · 13 | Troca de Posto (`swapPost`) | 1 aliado a até 3 hexes (os dois de um hex só) | ×1.0 da lança + 1D6 no golpe | **troca de lugar** com o aliado; do novo posto golpeia o inimigo de menos HP ao alcance da lança (sem inimigo: só a troca); +0.2 de afinidade com o aliado | 4 hexes no 19, 2D6 no 21, ×1.15 no 25, 5 hexes no 28, ×1.3 no 30 |
| T5 · 15 | Empalar e Arrastar (`impaleDrag`) | um hex em linha reta atrás de um inimigo vizinho, a 1–3 hexes, o caminho livre | ×(1.0 + 0.25 por hex arrastado) da lança + 1D8 | o inimigo é **empalado e arrastado** até lá; o Lanceiro corre atrás e para no hex antes | +0.3 por hex no 21, 4 hexes no 24, 2D8 no 28 |
| T5 · 17 | Vigília da Sentinela (`sentinelVigil`) | instantâneo, em volta de si (raio 1) | — | **postura** até o próximo turno dele: quem atacar (golpe ou magia) ele ou um aliado no raio leva a **estocada dele antes** (×1.0 da lança, sem contra-ataque); se o atacante cair, o ataque nem acontece | ×1.2 no 20, ×1.4 e +1D6 no 23, raio 2 no 26, ×1.6 no 27 |

Conditions ("N turnos"): Desarmado is on while `disarmPct > 0` (the ATK cut at once, the whole kept in `disarmBase` and given
back when it lifts); `disarmTurns` counts the foe's own turn starts after the one it was hit in. Desequilibrado is on while
`staggered`; it lifts as the foe's next turn begins (`staggerTurns` more of them from level 24). Both block its counter
(`strikesBack`). The Vigília is on while `vigilPct > 0` (the thrust's weight ×100) and ends as the sentinel's next turn begins;
the enemy turn (MapStage `updateTurns`) asks `vigilInterrupts` before an enemy's blow or cast and plays the thrusts as an
exchange of their own (`Exchange::interrupt`: no bleed roll, no hunger, no turn spent); the attacker then strikes only if it
still stands. All in the sheet (`conditionsText`) and in the fight's snapshot.

What the rules refuse (the panel's tip): a body bigger than one hex cannot be thrown, pulled, vaulted over, swapped with or
dragged; a throw never passes through the thrower; the hook needs a free hex on the line; the vault needs the hex behind the
foe free (walkable ground, nobody on it); the drag needs every hex on the way free.

## The effects (version 01 of each, `render/vfx/LancerFx`, 3D on the Combat material + lit pieces)

The element is in the effects only (the user): ember light on the hook's chain and on the vigil, steel on the thrusts.
- **Arremesso:** the lance goes in low (a thrust sheet to the feet), a flash and a streak up as the body is lifted, dust
  bursting out; the flight: a faint wake behind the card, streaks shed along it; the landing: a crater (a shell, a ground
  ring, dust rolling out, chips of ground on arcs, a light); against another body a flash of impact at its chest and sparks.
- **Gancho:** the hook (a lit steel point) flies on an ember chain (a tube with bright links) unrolling from the hand to the
  chest, bites with a flash and sparks; the pull: the chain stays taut to the body as it is dragged in, dust rolling behind it,
  a burst where it stops.
- **Salto de Vara:** the lance planted on the near side of the foe (a lit shaft leaning back, carried over to lean forward as
  the vault passes, then pulled out), a trail of steel light over the arc with a light, dust at the plant and the landing, a
  ground ring; the blow from behind: a thrust sheet, a white-hot flash, a crack ring at the feet, sparks.
- **Desarmar:** a thrust at the hands with a cross glint and sparks; on a hit the weapon (a lit blade with its crossguard)
  spins away up and to the side, bounces once, lies in the dust and fades after three seconds.
- **Troca de Posto:** the two cards hop over each other (0.55 high), a trail of steel light under the lancer and of gold under
  the ally, each on its own side of the line, a burst where they pass, dust at every take-off and landing; the blow from the
  new post is a thrust with a flash and sparks.
- **Empalar e Arrastar:** the thrust into the body (sparks forward and back); the lance stays in it, drawn between the two as
  they run, dust streaming behind the dragged body, sparks where the point scrapes, dark scrapes lying on the ground for two
  seconds; the jolt at the end: a flash, dust, chips, the lance pulled out.
- **Vigília da Sentinela:** the rise: the lance point flares, an ember ring runs out to the radius, embers lifted as it passes;
  standing: a slow ember pulse at the feet, a faint turning ring at the covered radius, motes rising round the body, the point
  aglow, a warm light (following the sentinel, gone when the stance lifts); the interrupt: a fast white-hot thrust with an ember
  shell bursting from the attacker's chest, sparks, a strong light.

## How it reaches the game

- Learned by level (`grantClassSpells`): a Lanceiro / Sentinela / Templário of level 2 has the Arremesso, 4 the Gancho, 8 the
  Salto de Vara, 11 Desarmar, 13 the Troca de Posto, 15 Empalar e Arrastar, 17 the Vigília. The campaign's Aldric is level 1
  until he levels (the same as the Guerreiro's and the Arqueiro's).
- HUD: the socket shows the user's icon; its tooltip the numbers at the hero's level (`spellTooltip`); the confirm panel shows
  the lance's hit chance on the foe the cast would strike (the throw and the drag are aimed at a hex, the swap at an ally;
  the vault's shows +10 for the rear). The zone: the hexes a throw / a drag can end on, the hook's and the swap's reach, the
  neighbours a vault can pass over.
- Tests: `--promote Aldric=aldric:25`, `--fx-test <kind>` (the lance's blow and the move's effects at the nearest foe, the
  cards hopping in place), `--battle-script "near:Piqueiro:Aldric; cast:Aldric:spearToss:Piqueiro"` (the `cast` step aims the
  throw and the drag at the farthest valid hex past the target; the swap takes an ally: `cast:Aldric:swapPost:Kael`), and the
  interrupt: `cast:Aldric:sentinelVigil:Aldric; vigil:Piqueiro:Aldric` (scratchpad `run_lancer.ps1`).

## Verified / not verified

See the commit message. Not verified: the look of each effect in the window (the user judges); the balance (the numbers are
the design's); an enemy never uses them (no AI); the hit chance shown for the vault adds 10 on top of a forecast that may
already count the rear when the foe happens to face away.
