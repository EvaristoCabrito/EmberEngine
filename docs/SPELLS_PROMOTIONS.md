# As promoções do Mago Negro: Bruxo e Elementalista (magias T6–T10)

EmberEngine's own spells (2026-10-17; the user: "come up with all the missing warlock spells, effects and scaling from
level 15 to 30", "2 magias por tier do 6 ao 10", "warlock é mais focado em enfeeblement and damage over time;
elementalist em DMG puro e AOE com os elementos clássicos", then "pode criar todas elas os efeitos e wire into the
game"). Engine2 defines the two classes (`editor-catalog.json`: "Promovido ← Mago Negro"), their HP and tier tables
(the Mago Negro's: `inn.json` tierUses, `overworld.json` classes) and no spell of its own for either; the help text
lists the Bruxo under "Conjuração Média" but its data table is the Rápida one, which this follows.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `emberNumbers`, `castEmber`, the condition ticks in
`startOfTurnEffects`), `engine/world/Battle.h` (`BattleUnit::Conditions`, `Battle::Zone`),
`engine/render/SpellEffectsEmber.cpp` (version 01 of every effect, flipbook cards), `apps/sandbox/MapStage.h` (HUD
sockets, zones on screen, the Escolhas panel's class switch in test mode), `apps/sandbox/OverworldStage.h` (Party →
Grupo → "Caminho de classe": the campaign's choice, written to `save.promotions`).

## The game's own formula

Damage = ⌊⌊MAG / 2⌋ × peso⌋ + dados, never below a plain attack, reduced by the element's resistance with MAG / 2 of
penetration (`elementalDamage`). Shown below at MAG 52 / 67 / 82 / 97 (levels 15 / 20 / 25 / 30 of the Mago Negro's
table: MAG 10 + 3 per level). Tier uses (Rápida): T6 opens at 13, T7 at 16, T8 at 18, T9 at 21, T10 at 23.

Conditions count down in the victim's own turns (applied at the start of its turn, then one less). Poison ticks at the
start of the victim's turn with Engine2's `poisonTiers` (Veneno Maior 1D10 / −20, Mortal 2D8 / −40, Letal 3D6 / −50:
the tier's dice, the victim's Poison resistance less the tier's penalty, the poisoner's MAG / 2 penetrating), as the
help text describes; this is the first time poison ticks in EmberEngine (Engine2's lesser poison from a save ticks too).

## Bruxo (enfraquecimento e dano no tempo)

| Tier · nível | Magia (kind) | Alvo | Dano Escuridão / Veneno a MAG 52 / 67 / 82 / 97 | Efeito | Escala |
|---|---|---|---|---|---|
| T6 · 15 | Marca da Ruína (`markOfRuin`) | 1 inimigo, alc 6 | ×1.15 + 1D6 → 29 / 37 / 47 / 55 | +20 % de dano de qualquer fonte por 3 turnos | +25 % no 19, +30 % no 23, +35 % no 27; 4 turnos no 22 |
| T6 · 15 | Praga Rastejante (`creepingPlague`) | 1 inimigo, alc 6 | ×1.3 + 1D8 → 33 / 42 / 53 / 62 | Veneno Maior; passa a 1 vizinho do mesmo lado por turno | 2 vizinhos no 21; Veneno Mortal no 25 |
| T7 · 16 | Dreno de Vida (`lifeDrain`) | 1 inimigo, alc 5 | ×1.6 + 2D6 → 41 / 52 / 65 / 76 | cura o Bruxo em 50 % do dano | 2D8 no 20, 2D10 no 26; 75 % no 22, 100 % no 28 |
| T7 · 17 | Mortalha de Sombras (`shroudOfShadows`) | hex, alc 6, zona | tick ×0.7 + 1D6 → 18 / 23 / 28 / 33 por turno | zona: −20 % ATK e MAG a quem está dentro (`fighterAt`), tick no início do turno; os dois lados | 17–19 raio 1, 2 rodadas · 20–23 raio 2, 3, −25 % · 24–27 raio 2, 4, −30 % · 28–30 raio 3, 4, −35 % |
| T8 · 18 | Hemorragia (`hemorrhage`) | 1 inimigo, alc 5 | ×1.4 + 2D6 → 36 / 46 / 57 / 67 | sangra 2D6 por turno durante 1D6 turnos; curas recebidas pela metade | tick 2D8 no 22, 3D6 no 26, 3D8 no 30; 2D4 turnos no 24 |
| T8 · 19 | Silêncio Profano (`profaneSilence`) | hex, alc 6, raio 1 | ×1.1 + 1D8 → 28 / 36 / 45 / 52 | inimigos não conjuram por 2 turnos (a IA bate com a arma); −1 uso do maior tier | 3 turnos no 24; raio 2 no 26 |
| T9 · 21 | Peste Negra (`blackPestilence`) | hex, alc 7, raio 2 | ×1.2 + 2D6 → 31 / 39 / 49 / 57 | Veneno Mortal em todos (os dois lados); quem morre envenenado contagia os vizinhos do seu lado com Veneno Maior | raio 3 no 25; Veneno Letal no 29 |
| T9 · 22 | Maldição da Decadência (`curseOfDecay`) | 1 inimigo, alc 6 | ×1.5 + 2D8 → 39 / 49 / 61 / 72; tick ×0.5 + 1D8 | −10 % de todos os atributos por turno, acumulando até −30 %; devolvidos no fim | teto −40 % no 25, −50 % no 29; 4 turnos no 27 |
| T10 · 23 | Colheita de Almas (`soulHarvest`) | hex, alc 6, raio 2 | ×1.7 + 3D6 → 44 / 56 / 69 / 81 em cada inimigo | veneno, sangria e maldição dos atingidos batem mais uma vez agora; cura 10 % do HP máx. por inimigo | 3D8 no 26, 4D6 no 29; ×1.9 no 27 |
| T10 · 25 | Vazio Devorador (`devouringVoid`) | 1 inimigo, alc 7 | ×2.2 + 3D8 → 57 / 72 / 90 / 105 | −30 de resistência a Escuridão pelo resto da batalha; abaixo de 15 % do HP, morre ("Executado") | 20 % no 27, 25 % no 30; 4D8 no 28 |

## Elementalista (dano puro e área)

| Tier · nível | Magia (kind) | Alvo | Dano a MAG 52 / 67 / 82 / 97 (cada atingido) | Efeito | Escala |
|---|---|---|---|---|---|
| T6 · 15 | Coluna de Fogo (`pillarOfFlame`) | hex, alc 6, raio 1 | ×2.0 + 2D8 → 52 / 66 / 82 / 96 | inimigos; o chão arde 1D8 por 1 rodada (zona `burning`, fogo) | 3D8 no 19; raio 2 no 23; 2 rodadas no 27 |
| T6 · 15 | Lança de Gelo (`iceLance`) | reta, alc 7 | ×2.4 + 2D10 → 62 / 79 / 98 / 115 | atravessa até à borda (linha do Tiro Perfurante); os seguintes recebem metade | inteiro no 21; 3D10 no 25; ×2.6 no 28 |
| T7 · 16 | Tempestade Elétrica (`thunderstorm`) | hex, alc 6, raio 2 | ×1.8 + 2D12 → 46 / 59 / 73 / 86 | inimigos; cada atingido ecoa 1D12+4 num vizinho não atingido, já | 3D12 no 21; eco 2D12+4 no 24; raio 3 no 27 |
| T7 · 17 | Meteoro (`meteor`) | hex, alc 8, raio 2 | centro ×3.0 + 4D6 → 78 / 99 / 123 / 144; anel ×2.0 + 2D6 | cai no início do próximo turno do conjurador (zona `meteor`, marca no chão); os dois lados | 5D6 no 22; raio 3 no 26; ×3.3 no 29 |
| T8 · 18 | Zero Absoluto (`absoluteZero`) | hex, alc 6, raio 2 | ×2.2 + 3D8 → 57 / 72 / 90 / 105 | inimigos; ×1.5 em quem está numa zona de gelo (gancho para a Tempestade de Gelo, não portada) | 4D8 no 23; ×2.5 no 27; raio 3 no 30 |
| T8 · 19 | Cadeia de Raios (`chainLightning`) | 1 inimigo, alc 7 | ×2.8 + 3D10 → 72 / 92 / 114 / 134 | salta até 3 vezes para o inimigo mais próximo a 3 hexes ainda não atingido, −15 % por salto | 4 saltos no 22, 5 no 26; 4D10 no 24; −10 % no 28 |
| T9 · 21 | Erupção (`eruption`) | em volta de si, raio 3 | ×2.6 + 4D8 → 67 / 85 / 106 / 124 | inimigos; o anel exterior arde 1D8 por 2 rodadas | raio 4 no 25; 5D8 no 27; ×2.9 no 30 |
| T9 · 22 | Vendaval Glacial (`glacialGale`) | cone, 5 hexes | ×2.4 + 4D6 → 62 / 79 / 98 / 115 | os dois lados; empurra 1 hex para longe (chão livre e passável) | 6 hexes no 24; 5D6 no 26; empurra 2 no 29 |
| T10 · 23 | Fúria dos Céus (`wrathOfTheSkies`) | 4 hexes à escolha, alc 8 | ×2.6 + 3D12 por raio → 67 / 85 / 106 / 124 | mira múltipla (como o Míssil); cada raio deixa o eco 2D12+8 do Relâmpago Maior | 5 raios no 26; 4D12 no 28; ×2.9 no 30 |
| T10 · 25 | Brasa Primordial (`primordialEmber`) | hex, alc 7, raio 3 | ×3.4 + 4D10 → 88 / 112 / 139 / 163 | elemento Brasa (quase ninguém resiste); os dois lados; o chão fica em brasa 2D8 por 2 rodadas | 5D10 no 27; ×3.7 no 29; raio 4 no 30 |

## How it reaches the game

- **Campaign:** world map → Party → Grupo → "Caminho de classe": a Mago Negro of level 15 chooses once; the save's
  `promotions[hero]` becomes the hero's class in every fight (`MapStageOptions::heroClass`, `e2::setPromotion`: class id,
  class name, spell list). Level-ups mid-fight learn the level's spell (`grantClassSpells`).
- **Modo teste:** the Escolhas panel lists every black mage of the party with "Mago Negro / Elementalista / Bruxo", live.
  CLI: `--promote Voss=warlock:30` (class and level for the fight), `--fx-test <kind>`, `--battle-script "cast:Voss:markOfRuin:Piqueiro"`.
- **HUD:** the sockets show the spell's initials on its element's colour (no Engine2 icon exists); the tooltip gives the
  numbers at the caster's level (`spellTooltip`); the unit sheet lists its conditions (`conditionsText`).
- **Effects:** version 01 of each (`content/fx/README.md` lists them); zones are drawn for as long as the rules keep them
  (`MapStage::syncZones`); a Meteoro's numbers wait for the rock to land (0.9 s).
- **Save / Load:** conditions, zones, class and spell list go into the battle snapshot (`BattleSave.cpp`).

## Verified (2026-10-17)

- Debug + Vulkan validation, exit 0: all 20 spells cast through `--battle-script` on the aldeia015 test battle with Voss
  promoted at level 25 (`captures/promo/*.log`): the marks, poisons, bleeding, silence, curse and zones land, the
  pushes move the card, the hits take HP, the uses are spent, the turn is spent.
- `Sandbox --battle-save-check` PASS (the snapshot round trip with the new fields); `Cooker check-battle` aldeia015 still
  matches Engine2 (reach, 462 forecasts, 462 seeded rolls): Engine2's own spells are untouched.
- Effects seen in captures with the atmosphere on (Coluna de Fogo, Peste Negra, Cadeia de Raios, Vazio Devorador) after
  the shader fix below. The other sixteen share the same card path and the same clips; the user judges every one in the
  window.
- **Found on the way:** since the Atmosfera volumétrica (2026-10-16 afternoon) every spell effect in the game was
  invisible while it was on (the default): `volumeTransmittance` in `shaders/lighting.hlsli` read 0 (the DXC inline
  SampleLevel gotcha). Fixed with the `volumeSampleAt` helper; Engine2's Relâmpago served as the control.

## Not done

- Enemy AI never casts these (Engine2's Bruxo enemies keep Engine2's kit).
- No Engine2 reference exists for any of this (`Cooker check-battle` covers Engine2's spells only); the numbers are
  the design's, checked by the battle script and by hand.
- Tempestade de Gelo (Engine2's) is not ported, so Zero Absoluto's ×1.5 never fires yet.
- The other promotions (Clérigo, Bispo, Arcanista, Necromante, Patrulheiro, Sentinela, Templário) have no spells of
  their own here.
