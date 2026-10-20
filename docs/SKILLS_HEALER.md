# As magias do Curandeiro: T4 e T5 completos, mais dois feitiços de campo

EmberEngine's own spells (2026-10-20; the user: "create these new healer spells with 3D effects and proper scaling according to the
game rules", the formulas written the way Salazar's own are). Salazar (`salazar`; the Clérigo and the Bispo are his promotions,
`emberClassFamily` → `healer`) has Engine2's two spells in each of T1–T3 and Raio Divino in T4 (Criar Comida e Água counts as his
field spell); T5 was empty. His tier table (`inn.json` tierUses) opens T2 at 3, T3 at 6, T4 at 8, T5 at 11.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `healerKind`, `fallenAllyAt`, `emberNumbers`, `castEmber`: the circle and the
resurrection as Heal steps, the judgment through the area branch with the undead's weight in `perTargetMul`; `resolveStep`'s Heal
branch heals every id of `ids`, its `resurrection` branch stands the fallen up; `spellTooltip`, `spellPose`, `spellZone`,
`spellAimValid`), `Spells.h` (`BattleEffect::revived`), `engine/world/FieldSpells.*` (the two field spells),
`engine/world/Overworld.cpp` (`step`: the Prece's hexes), `content/engine2/undead-encounters.json`
(`tools/scripts/engine2/undead_encounters.py`), `engine/render/vfx/HealerFx.*` (the effects, 3D), `render/SpellEffects.cpp`
(`castEffect`), `apps/sandbox/MapStage.h` (the areas' spots, the card back on a revival, `--fx-test`, the battle script's `kill` step and
checks), `apps/sandbox/OverworldStage.h` (the field-spells menu's rows). Icons: the user's art (`Attachments/HealerIcons`) in
`content/ui/spells/<kind>.jpg` (784 × 1168, centre-cropped in the HUD socket and heading the field rows).

## The formula

The cures' own: floor(floor(MAG / 2) × weight) + dice, then +1 % per point of the healing skill (`healingPower`), halved on a unit
under Hemorragia; a heal trains the skill and builds affinity (+0.2) with each one healed, as Cura Menor does. Julgamento is a holy
spell: the game's damage formula (floor(floor(MAG / 2) × weight) + dice, never worse than a plain attack, the holy resistance, half
the caster's MAG of penetration, the staff's holy bonus), every foe in the area, the undead at a heavier weight; the area's XP rule
(the first full, the rest half). The reach of the two areas grows with the level like every other mage's (`reachGrowth`: +1 at 7,
+2 at 15, +3 at 23).

| Tier · nível | Magia (kind) | Alvo | Efeito | Escala |
|---|---|---|---|---|
| T4 · 9 | Círculo de Cura (`circleOfHealing`) | hex a até 4, raio 2 | cada aliado ferido na área (o conjurador também) cura MAG/2 × 1.2 + 1D8, cada um com a própria rolagem | 2D8 no 15, ×1.4 no 19, raio 3 no 23, 3D8 no 26, ×1.6 no 29 |
| T5 · 12 | Ressurreição (`resurrection`) | aliado caído ao lado (alcance 1; ninguém em cima dele) | volta com MAG/2 × 1.5 + 2D8 HP (nunca mais que o máximo), livre de todas as condições, e age no seu próximo turno (o turno desta rodada já passou) | ×1.8 no 18, 3D8 no 22, ×2.1 no 26, 4D8 no 29 |
| T5 · 14 | Julgamento (`judgment`) | hex a até 6, raio 2, sem linha de tiro | MAG/2 × 1.8 + 2D8 de dano sagrado em cada inimigo; mortos-vivos ×1.5 | 3D8 no 18, ×2.1 no 21, raio 3 no 24, mortos-vivos ×1.75 no 26, 4D8 no 28, ×2.4 no 30 |
| campo · T2 · 5 | Consagrar Acampamento (`consecrateCamp`) | o grupo, no mapa-múndi | todo herói de pé com HP cheio (classe + equipamento, menos a fome, como a ficha do mapa); os usos de tier NÃO voltam: a estalagem continua necessária; gasta um uso do T2 | — (a especificação do usuário: só o HP) |
| campo · T3 · 9 | Prece do Caminho (`prayerOfTheRoad`) | os próximos hexes no mapa-múndi | enquanto durar, nenhum encontro com mortos-vivos é sorteado e uma batalha fica pela metade da chance; gasta um uso do T3 | 10 hexes; 12 no 16, 14 no 23, 16 no 30 |

Ressurreição: the fallen keep the hex they fell on (the rules never move the dead); the aim needs that hex within 1 of the caster
(its own hex too) and nobody standing on it. What death left on the body is wiped (poison, bleeding, stun, sleep, fear, the echo,
our conditions; a Paralisado / Desarmado / Decadência's stats given back), and it stands with `moved` and `acted` set, so the next
round's order picks it up. A summon cannot be raised. In a campaign the revived hero leaves the fight with the HP it has, like
anyone else (`finishCampaignFight`).

Círculo de Cura: cast on a hex (no line of fire), the allies of the area at less than full HP are healed, each its own roll; a hex
with nobody hurt in reach is refused ("Não há aliado ferido nessa área."); in Modo teste (debugFreeCast) everyone in it counts.

Prece do Caminho: `save.roadPrayer` = the hexes left; each `Overworld::step` spends one, leaves the undead encounters
(`undead-encounters.json`: 8 of the 42 road missions, every one with zombies: blightwood-verge, broken-antler-grove, cairnkeepers-rise,
random-encounter-13, forest011-hollow-grove, mora-rootwatch, plains-silent-harvest, rimewater-shrine) out of the roll and halves
`battleChance` (the text road encounters — the loot bags, the rain — are untouched). A new prayer starts the count over at the
caster's own reach, never adding up. A save without the key behaves exactly as before (Cooker check-overworld unchanged). The
list is made from the newest map of each encounter id (Engine2's folder, then `content/maps/ember`): re-run the script after a
road map changes.

## The effects (version 01 of each, `render/vfx/HealerFx`, 3D on the Holy + Combat materials)

- **Círculo de Cura:** a sigil (a ring, six rays, six small circles) draws itself on the ground over the whole area while the light
  gathers (0.5 s), turning slowly, motes rising across it; at the impact a flash, a wave of light runs from the centre to the edge
  with a ring across the ground, and on every healed ally, as the wave reaches it, a shaft of warm light comes down, a halo settles
  from the head to the feet, motes rise round the body, the ground glows under it; a light at the centre and on each healed body.
- **Ressurreição:** motes spiral in and down onto the fallen body for 0.9 s while a thin pillar from the sky brightens over it and a
  star draws itself under it; at the impact a flash at the chest, the pillar flares wide and withdraws upward, two rings run across
  the ground and rays of light run out, two wings of light (tubes with feathers) unfurl from the back and hold while a halo rises
  from the feet past the head, motes burst up like feathers and drift down; a strong warm light that lingers 2 s.
- **Julgamento:** a hexagram sigil turns high in the air (5 units) over the aimed hex while motes stream up into it (0.6 s); a blade
  of white light drives down through it into the ground over the last 0.16 s and lands on the impact: a flash, a pressure shell out
  to the edge of the area, two rings and 24 rays across the ground, dust lifting off the hexes; on each foe, as the wave reaches it,
  a column of holy fire rises, a flash, sparks fly up, a ring burns at its feet; the blade stands half a second, then rises and fades.

## How it reaches the game

- Learned by level (`grantClassSpells`): a Curandeiro / Clérigo / Bispo of level 9 has the Círculo, 12 the Ressurreição, 14 the
  Julgamento. The field spells need level 5 (Consagrar, T2 uses) and 9 (Prece, T3 uses); they are rows of the world map's
  field-spells menu (the header's icon), beside Criar Comida e Água, the Caçada and Warp, each with its icon and "Acampar" / "Rezar".
- HUD: the socket shows the user's icon; its tooltip the numbers at the hero's level (`spellTooltip`); the status sheet lists them
  under their tier with the uses left. The combat log names them ("Salazar curou Kael com Círculo de Cura: +18 HP"; "Kael:
  Ressuscitado").
- Tests: `--promote Salazar=salazar:25`, `--fx-test circleOfHealing|resurrection|judgment`,
  `--battle-script "hurt; cast:Salazar:circleOfHealing:Kael"`, `"kill:Kael; cast:Salazar:resurrection:Kael"` (the new `kill:<unit>`
  step fells a unit where it stands), `"near:Piqueiro:Salazar; cast:Salazar:judgment:Piqueiro"`.

## Verified / not verified

See the commit message. Not verified: the look of each effect in the window (the user judges); the balance (the numbers are the
design's); an enemy never uses them (no AI); the field rows in the window (the code path is the Caçada's); the Prece against a real
road roll (only the save field and the roll's pool were read in code).
