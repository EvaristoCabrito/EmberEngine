# O Cruzado (young Salazar's class)

EmberEngine's own hero class (the user, 2026-10-25: "a new class called Cruzader ... a bit weaker than warrior and a bit stronger than
healer, a bit hibrid, only goes to level 10", the spell list pasted; then "never uses a shield ... the Cruzado's T2 should end up as Cleave
and Toque Sagrado"). Young Salazar's interlude class, apart from the adult Salazar's Curandeiro.

## The class

- Data: `content/ember-classes.json` `heroClasses.crusader` (the same fields as `content/data/battle-rules.json`'s classes; `engine/world/
  BattleExport.cpp` Rules merges it in, and every melee sword / mace / hammer becomes usable by it). Never a shield: no shield, no other
  gear, no Golpe de Escudo (`grantClassSpells` removes it from his list).
- Level 10 at most (`Spells.cpp classLevelCap`: experience stops there; the exporter places him at 10 at most).
- Stats: each one the Guerreiro's and the Curandeiro's mean at the level (rounded), levels 1 to 10: HP 30 → 62, ATK 7 → 16, MAG 4 → 9,
  DEF 5 → 23, DEX 5 → 18; MOV 5, initiative +3. Starter weapon: Espada Larga.
- Tiers: the Arqueiro's table (the middle one): T1 at 1, T2 at 4, T3 at 7, nothing above T3 (uses at 10: 5 / 4 / 2).
- Placeholder art: `kaelEarly` (the first Kael art, set `kael`, idle + attack; on no map). The Map Editor lists the class "Cruzado" and the
  hero "Salazar Jovem" (`tools/scripts/sprites/catalog_ember_classes.py`).
- Test battle: `content/maps/ember/cruzadotest001.json` (the Isolde test's board and enemies, one war dog made a Zumbi for the holy
  against the undead; Salazar Jovem in Isolde's place). Map Editor → cruzadotest → Testar.

## The spells (no scaling track: the class stops at 10; the user's numbers)

| Nível · tier | Magia (kind) | Alvo | Efeito |
|---|---|---|---|
| 1 · T1 | Bênção (`bless`) | as the Curandeiro's | igual à atual |
| 1 · T1 | Golpe Consagrado (`holyStrike`) | inimigo ao alcance da arma | golpe da arma (a rolagem, o acerto e a maestria dela) + 1D8 sagrado quando acerta; contra morto-vivo o sagrado ×1.5; o sagrado contra a resistência sagrada, com o MAG dele de penetração |
| 4 · T2 | Cleave (`cleave`) | as the Guerreiro's | igual ao do Guerreiro |
| 4 · T2 | Toque Sagrado (`holyTouch`) | aliado ao lado (alcance 1, ferido) | cura MAG/2 × 0.9 + 1D6 (a fórmula das curas, +1 % por ponto de Cura) |
| 7 · T3 | Juramento (`crusaderOath`) | passiva | abaixo de 25 % do HP: +20 % ATK até voltar a passar de 25 % (`oathActive`, Combat.cpp `fighterAt`); não gasta uso, não tem soquete; diz "Juramento +20% ATK" a primeira vez que acorda na luta |
| 7 · T3 | Manto do Cruzado (`crusaderMantle`) | em volta de si, raio 2 | ele e cada aliado no raio somam 1D4 sagrado a cada golpe de arma (ataques, contra-ataques, habilidades de arma) por 2 turnos (contados como o Grito de Guerra) |

Not built (the paste's optional): Expulsar Mortos-Vivos as his T3.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `crusaderKind`, `emberNumbers`, `castEmber`, the holy part of `resolveStrikeImpl`, the
Manto's Effect step and countdown, `noteOaths`, `grantClassSpells`, `classLevelCap`, `spellPassive`, zones / aims / tooltip / names),
`Battle.h` (`Conditions::mantle* / oathShown`, `oathActive`, `kOathHpPct / kOathAtkPct`), `Spells.h` (`BattleStep::holy* / mantle*`),
`BattleSave.cpp` (the snapshot), `BattleExport.cpp` (the class, the level cap).

## The effects (version 01 of each, `render/vfx/CrusaderFx`, 3D on the Holy + Combat materials, gold)

- **Golpe Consagrado:** the blade lights up in gold before the blow (a bright stroke at the weapon hand toward the foe, motes drawn into
  it, a warm light); where it lands, a short flash with gold sparks thrown out away from him and a ring on the ground (only on a hit).
- **Toque Sagrado:** his hand glows gold at the ally's shoulder; a soft light climbs the ally (a faint shaft, a halo rising from the feet
  past the head, motes going up, the ground glowing).
- **Juramento:** while it holds, a faint gold aura pulsing round him with gold embers rising off it and a soft light (MapStage
  `syncCrusader`).
- **Manto do Cruzado:** the light gathers round him, then a wave of gold spreads across the radius; every ally it reaches flashes gold and
  keeps a faint gold outline (a thin shell of light, a fine ring at the feet) while the mantle lasts; each blow with its holy dice throws a
  gold spark where it lands.

## Verified / not verified

See the commit message. Not verified: any of it in the window (the user judges the look); the balance.
