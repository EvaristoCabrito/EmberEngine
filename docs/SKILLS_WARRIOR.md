# As habilidades do Guerreiro: Tier 4 e Tier 5

EmberEngine's own skills (2026-10-18; the user: "eu preciso de duas habilidades novas Tier 4 e duas Tier 5 para guerreiro,
with level scaling, and 3D FX", then "faz as quatro como propostas"). The Guerreiro (`swordsman`, Kael) has Engine2's
Golpe Duplo, Provocar, Investida (T1), Cleave, Golpe de Escudo (T2) and Golpe do Carrasco (T3); its tier table
(`inn.json` tierUses) opens T4 at level 11 and T5 at level 15, with nothing to spend them on. These four fill them. The
Paladino and the Cavaleiro Pesado (its promotions) keep them.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `emberNumbers`, `castEmber` + `leapLanding`, the conditions' countdown in
`startOfTurnEffects`, `conditionsText`, `spellTooltip`), `Battle.h` (`Conditions::sunder* / rally*`), `Combat.cpp`
(`fighterAt` applies them), `BattleSave.cpp` (in the snapshot), `engine/render/vfx/WarriorFx.h/.cpp` (the four effects,
3D), `render/SpellEffects.cpp` (`castEffect`, `leapFlight`, `warCryFear`), `apps/sandbox/MapStage.h` (the Salto's card
arc `leapOffset`, the Grito's allies / foes as the effect's targets, `--fx-test`, the battle script). Icons: the user's
art, `content/ui/spells/<kind>.jpg` (784 x 1168, drawn centre-cropped in the HUD socket).

## The formula

A weapon skill: the weapon's own roll (ATK, mastery, hit chance against DEX, the 8 % critical, rear bonus) plus the
skill's bonus dice, exactly as Golpe do Carrasco and Investida do it (`BattleStep::bonusDice` on a Combat step,
`weaponBonusDice` on an area step). MAG plays no part. A miss is a miss (the uses and the turn are spent). Tier uses:
T4 and T5 of the Guerreiro's table.

| Tier · nível | Habilidade (kind) | Alvo | Dano | Efeito | Escala |
|---|---|---|---|---|---|
| T4 · 11 | Redemoinho de Aço (`steelWhirlwind`) | em volta de si, os 6 hexes vizinhos (`spellSelfArea`: a área aparece, Confirmar) | golpe de arma + 1D6 em cada inimigo | sem contra-ataque | 1D8 no 14, 2D6 no 18, 2D8 no 22, 2D10 no 26, 3D8 no 30 |
| T4 · 13 | Quebra-Guarda (`guardBreak`) | 1 inimigo ao alcance da arma | golpe de arma + 1D8 | **Guarda quebrada**: −20 % DEF por 2 turnos; o inimigo contra-ataca se puder | −25 % no 17, 2D6 no 19, −30 % no 21, 3 turnos no 24, 2D8 no 25, −35 % no 28 |
| T5 · 15 | Grito de Guerra (`warCry`) | instantâneo, raio 2 em volta de si | — | **Grito de Guerra**: aliados no raio (ele também) +15 % ATK por 2 turnos | raio 3 no 19, +20 % no 21, inimigos no raio com medo 1 turno a partir do 22, 3 turnos no 24, +25 % no 27 |
| T5 · 17 | Salto Devastador (`leapingStrike`) | 1 inimigo visível: salta até 3 hexes (por cima de tudo) para o hex livre ao lado dele mais perto de si | ×1.5 da arma + 2D6 | sem contra-ataque; o salto não gasta movimento | alcance 4 no 21, ×1.75 no 23, 2D8 no 25, a partir do 25 **onda de choque**: os outros inimigos vizinhos do pouso levam metade de uma rolagem de arma cada, alcance 5 no 27, ×2.0 no 29 |

Conditions ("N turnos"): on while `pct > 0`; `turns` counts the unit's own turn starts after the one it was applied in;
it lifts as the turn after the last one begins (Quebra-Guarda −20 % for 2 turns = the foe defends weaker through its
next two turns and the party's turns between them). `fighterAt` applies them (so the forecast and the hit % show the
broken guard); the sheet lists them (`conditionsText`); both go into the battle snapshot. The fear is Engine2's own
(`fearTurns` + `fearSourceId`: the AI flees from the caster).

The Salto's landing (`leapLanding`): the caster's own hex when the weapon already reaches the foe (no move); else the
free, passable, non-void hex beside the foe within `range` hexes of the caster, nearest to it (the first in the
neighbours' order on a tie), from which the weapon reaches the foe. The aim is valid wherever such a landing exists
(the zone shows every hex within range + 1 whose foe can be reached). The move is a `Type::Move` step (the rules stand
the unit there at once, hazards underfoot included); the stage flies the card on an arc.

## The effects (version 01 of each, `render/vfx/WarriorFx`, 3D on the Combat material + lit pieces)

- **Redemoinho de Aço:** a sheet of steel light turns a full circle round the caster at weapon height (a helix, never a
  flat disc), its edge white-hot and a glint riding it with a light; sparks flung off tangentially, falling; a ring of
  dust rolling out from the feet; a flash and a light on every foe as the edge reaches it. The turn takes 0.32 s and
  starts 0.22 s before the blow lands.
- **Quebra-Guarda:** the blade comes down (a vertical cut sheet from above the head to the chest, drawn in as it falls);
  on the impact a white-orange flash, sparks, sixteen armour shards (thin plates of steel lit by the scene, metallic)
  burst from the chest the blow's way and fall, bouncing once; a crack ring on the ground; an orange light. Then two arcs
  of dull red light (a broken ring) turn over the chest for two seconds with a dim red light, and go out.
- **Grito de Guerra:** a flash at the chest and a strong warm light; a pressure shell bursts from the chest out to the
  radius in 0.45 s, the ground ring with it, dust lifted off the ground as the ring passes; on every ally (the caster
  among them) a thin column of red-gold light rises as the shell reaches it, motes climbing in it, a warm light (at most
  four). On the frightened foes (level 22+) a dark pulse: a shell that darkens what is behind it swells from the chest,
  dark red motes rise.
- **Salto Devastador:** the card flies to the landing on an arc (`MapStage::leapOffset`, 1.2 + 0.25 units per hex high,
  0.34 s + 0.05 s per hex); a trail of steel light follows the weapon along the arc, sparks shed behind it, a light with
  it; the landing is a crater: a flash, a pressure shell, a ground ring to 2.3 units, a ring of dust rolling out, fourteen
  chips of the ground thrown on arcs and bouncing, a strong white-orange light. Then the blow: a big downward cut sheet
  from above the foe's head to its knees, sparks, a cold flash; the shockwave's numbers follow at once.

## How it reaches the game

- Learned by level (`grantClassSpells`, at battle start and on a level-up in the fight): a Guerreiro / Paladino /
  Cavaleiro Pesado of level 11 has the Redemoinho, 13 the Quebra-Guarda, 15 the Grito, 17 the Salto. No promotion
  screen is involved (the user, 2026-10-17: never add what was not asked).
- HUD: the socket shows the user's icon; its tooltip the numbers at the hero's level (`spellTooltip`); the status
  sheet lists them under their tier with the uses left.
- Tests: `--promote Kael=swordsman:25` (the level for the fight), `--fx-test steelWhirlwind|guardBreak|warCry|leapingStrike`,
  `--battle-script "near:Piqueiro:Kael; cast:Kael:guardBreak:Piqueiro; cast:Kael:warCry:Neera; ..."` (the `cast` step
  aims a self-centred skill on the caster's own hex; the check accepts a weapon miss and logs it).

## Verified / not verified

See the commit message. Not verified: the look of each effect in the window (the user judges); the balance (the
numbers are the design's); an enemy never uses them (no AI).
