# As habilidades do Arqueiro: T1 a T5 completos, mais a Caçada no mapa-múndi

EmberEngine's own skills (2026-10-18; the user: "criar as habilidades que estão faltando para arqueiro até o T5, duas de
cada tier no mínimo e uma habilidade de campo para usar no world map"). The Arqueiro (`archer`, Neera) has Engine2's
Tiro Longo (T1), Tiro Perfurante and Tiro Sangrento (T2), Tiro Múltiplo (T3); its tier table (`inn.json` tierUses) opens
T1 at 1, T2 at 4, T3 at 7, T4 at 10, T5 at 13. These six make two of every tier; the Caçada is the field skill. The
Patrulheiro and the Assassino (its promotions) keep them.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `archerShot` / `archerVolley`, `emberNumbers`, `castEmber`, the
Paralisado condition `applyHobble` / `liftHobble` and its countdown in `startOfTurnEffects`, `conditionsText`,
`spellTooltip`), `Battle.h` (`Conditions::hobble*`, `hobbleBase`), `BattleSave.cpp`, `engine/world/FieldSpells.*`
(Caçada), `apps/sandbox/OverworldStage.h` (its row in the field-spells menu), `engine/render/vfx/RangerFx.h/.cpp` (the
effects, 3D), `render/SpellEffects.cpp` (`castEffect`), `apps/sandbox/MapStage.h` (`archerKind`: the arrow's hit on each
struck body, `--fx-test`, the battle script's checks). Icons: none yet (the socket shows the initials on wood); put the
user's art in `content/ui/spells/<kind>.jpg` when it comes.

## The formula

A bow skill: the bow's own roll (ATK, mastery, hit chance against DEX, the 8 % critical) times the skill's weight, plus
the skill's bonus dice; as Tiro Sangrento does it (`dmgMul` + `weaponBonusDice` on an area step: no counter). MAG plays no
part. A miss is a miss. Shots at one foe need the bow's reach (Neera: 2 to 4 hexes, + the skill's extra) and a clear line
of fire; the volleys come from above: range only, no line of fire, the aimed hex needs ground.

| Tier · nível | Habilidade (kind) | Alvo | Dano | Efeito | Escala |
|---|---|---|---|---|---|
| T1 · 2 | Flecha Envenenada (`venomArrow`) | 1 inimigo, alcance do arco | tiro de arco + 1D4 | envenena: Veneno Menor | Veneno no 7, 1D6 no 8, Veneno Maior no 13, 1D8 no 14, 2D6 no 20, Veneno Mortal no 21, 2D8 no 26 |
| T3 · 7 | Chuva de Flechas (`arrowRain`) | hex a até 6, raio 1, sem linha de tiro | ×0.75 do arco + 1D4 em cada inimigo | — | 2D4 no 13, raio 2 no 16, ×0.9 no 20, 2D6 no 25 |
| T4 · 10 | Flecha Explosiva (`explosiveArrow`) | 1 inimigo, alcance do arco | ×1.3 do arco + 2D6 | os inimigos vizinhos do alvo levam 50 % da sua própria rolagem | ×1.5 no 14, 3D6 no 18, 70 % no 22, ×1.8 no 26, 4D6 no 30 |
| T4 · 12 | Tiro Paralisante (`cripplingShot`) | 1 inimigo, alcance do arco | tiro de arco + 1D8 | **Paralisado**: −50 % MOV por 2 turnos | 2D6 no 16, −60 % no 18, 3 turnos no 22, 2D8 no 24, −75 % no 27 |
| T5 · 13 | Tempestade de Flechas (`arrowStorm`) | hex a até 7, raio 2, sem linha de tiro | ×0.9 do arco + 1D6 em cada inimigo | — | ×1.1 no 20, 2D6 no 23, raio 3 no 25, ×1.3 no 28 |
| T5 · 15 | Tiro Mortal (`deathShot`) | 1 inimigo, alcance do arco + 2 | ×2.0 do arco + 2D8 | abaixo de 25 % do HP, morre ("Executado") | ×2.3 no 21, 3D8 no 24, 30 % no 26, ×2.6 no 29 |
| campo · T2 · 4 | Caçada (`hunt`, mapa-múndi) | — | — | a arqueira caça: 1D4+1 rações para o grupo, gasta um uso do T2 | 2D4+1 no 10, 2D6+2 no 18, 3D6+3 no 26 |

Paralisado: `mov` cut at once (the whole kept in `hobbleBase`), on while `hobblePct > 0`; `hobbleTurns` counts the foe's
own turn starts after the one it was hit in, the whole MOV given back as the turn after the last one begins. In the
sheet (`conditionsText`), in the snapshot. The poison is Engine2's own (`applyPoison` with the tier; the tick at the
victim's turn start, its resistance less the tier's penalty).

The Caçada lives in the world map's field-spells menu (the header's icon), beside Criar Comida e Água and Warp: every
standing archer of level 4+ gets a row with "Caçar"; it rolls the rations into `save.rations` and spends `spellUses`
tier2 of that hero (the same pool a battle cast uses). No capture of the world map was taken (the rule for the panel is
the same code path as the food's).

## The effects (version 01 of each, `render/vfx/RangerFx`, 3D on the Combat material + lit pieces)

A real arrow: a shaft of wood and a steel head (lit by the scene, metallic head) flying on a shallow arc from the bow to the
body with a glint at the point, released `flightSeconds` before the hit (1 / 22 s per unit, 0.12–0.4 s) so it lands on the
rules' impact; CombatFx's arrow hit (the pin of light, the splinters) lands on every struck body as for a plain shot.
- **Flecha Envenenada:** green motes shed along the flight, a green light; on the body a sick green shell bursts, drips
  fall to the feet, a green ring on the ground, a green light.
- **Flecha Explosiva:** embers shed upward along the flight; a flash, a shell of fire bursting out to the neighbouring
  hexes, 34 flames and embers flung up and out, a ring of fire on the ground, dust rolling out, a strong orange light.
- **Tiro Paralisante:** a grey trail; the arrow goes in low (the knees): a burst of dust at the feet, chips of ground, a
  dull grey ring pulsing under the foe for two seconds.
- **Tiro Mortal:** a long crimson trail (a tube behind the arrow over 60 % of the path), dark red motes; a pin of light
  driven through the body, a crimson shell, dark streaks flung out, a red ring on the ground, a red light that lingers.
- **Chuva / Tempestade de Flechas:** a faint ring over the area before the volley; three (five) arrows a hex come down at a
  slant from the archer's side (8 units of air in 0.32 s), the Tempestade in two waves; each lands with a glint and a puff
  of dust and stays stuck in the ground, leaning the way it came, fading over a second; a flash on every foe standing in it.

## How it reaches the game

- Learned by level (`grantClassSpells`): an Arqueiro / Patrulheiro / Assassino of level 2 has the Flecha Envenenada, 7 the
  Chuva, 10 the Explosiva, 12 the Paralisante, 13 the Tempestade, 15 the Tiro Mortal. The Caçada needs level 4 and T2 uses.
- HUD: the socket shows the initials on wood until there is art; its tooltip the numbers at the hero's level
  (`spellTooltip`); the status sheet lists them under their tier with the uses left.
- Tests: `--promote Neera=archer:25`, `--fx-test <kind>`, `--battle-script "near:Piqueiro:Neera; cast:Neera:venomArrow:Piqueiro"`
  (the `cast` step stands the target beside the caster: inside the bow's minimum range, so the shots need a target two
  hexes away: `goto:Neera:x:y; spell:Neera:deathShot; click:x:y; confirm`, or the volleys, which take any hex).

## Verified / not verified

See the commit message. Not verified: the look of each effect in the window (the user judges); the balance (the numbers
are the design's); an enemy never uses them (no AI); the Caçada's row in the window (code path = the food's).
