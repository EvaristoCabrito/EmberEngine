# As magias do Mago Negro (Voss): Nuvem de Adagas, Dissipar Magia, Globo de Invulnerabilidade, Identificar

Tempestade de Gelo (`iceStorm`) também pertence à lista normal de Voss: T4, aprendida no nível 8, quando o tier abre.
O aprendizado passa por `grantClassSpells`, inclusive nas promoções do Mago Negro; o lançamento normal gasta um uso de T4.
Seu dano, campo, duração e efeito visual são os mesmos usados no teste; a prévia de efeitos não define o acesso da classe.

2026-10-21, the user's specs (pasted; "Criar essa magia e seu efeito visual", then "depois criar" the other three): the Mago Negro (`mage`;
Voss's own class id is `voss` in most battle files; the Bruxo and the Elementalista are his promotions, `emberClassFamily` → `mage`, so they
keep them). Every number below is the user's; where the user named the levels a number changes on, it changes there and nowhere else (no
generic 14-step track: `emberNumbers` reads the level directly, or a FIXED track in `tools/scripts/spells/tracks.py`).

| Nível · tier | Magia (kind) | Alvo | Efeito | Escala |
|---|---|---|---|---|
| 6 · T3 | Dissipar Magia (`dispelMagic`) | hex a até 6, raio 1, sem linha de tiro | sem dano. Nos inimigos da área, tira os efeitos bons de magia: Bênção, Grito de Guerra, Vigília; apaga as zonas criadas por inimigos que tocam a área (teias, tempestades de gelo, auras, chão em chamas, Mortalha, a marca de um Meteoro). Nos aliados (o Voss também), tira sono, medo, silêncio, marcas (Marca da Ruína) e maldições (Maldição da Decadência); veneno e sangramento ficam. "Dissipado" em cada um que perdeu algo; "Zona dissipada" no Voss | raio 2 no nível 10 |
| 8 · T4 | Nuvem de Adagas (`cloudOfDaggers`) | ele mesmo (instantânea) | 3 adagas girando em volta do Voss, à vista de todos, até serem usadas ou a batalha acabar; lançar de novo repõe o total (nunca somam). Cada uma reage sozinha, sem gastar a ação dele, e é gasta na reação | 4 adagas no nível 12, 5 no 15 (`kSpellTracks`: `- - - c1 - - c1 ...`, escrito à mão em `tools/scripts/spells/tracks.py` FIXED) |
| 11 · T5 | Globo de Invulnerabilidade (`globeOfInvulnerability`) | em volta de si (a área é mostrada, depois Confirmar); a bolha fica parada onde foi lançada | bolha de raio 1 por 2 rodadas (contadas no começo de cada turno do Voss; morto ele, a cada rodada nova). Magias de tier 1 a 3 lançadas de fora para dentro falham em quem está dentro, dos dois lados, curas também ("Bloqueado"; o uso é gasto; numa área, quem está fora sofre normal); magia de tier mais alto atravessa. Zonas não fazem efeito nos hexes de dentro (teias, tempestades, auras, chão em chamas, Mortalha, Zero Absoluto). Golpes, flechas, virotes e lanças (toda habilidade de arma) passam. Quem está dentro lança para fora normalmente; qualquer um entra e sai. A IA inimiga não gasta magia em alvo que a bolha protege (o uso volta, ela ataca com a arma) | raio 2, 3 rodadas e até tier 4 no nível 14 |
| campo · 1 · T1 | Identificar (`identify`) | um item desconhecido das bolsas do grupo | o item passa a mostrar o nome e os atributos para sempre; 1 item por uso | — |

## As reações

Each reaction spends one dagger; the strike is a spell step of its own (`daggerStrike`, "Adaga", arcane, never learned), MAG/2 × 0.8 + 1D6,
the arcane resistance, no counter. The daggers answer only units of the other side (never neutrals), standing beside their caster
(one hex: the daggers reach whoever stands against him).

- **Inimigo chega perto.** When an enemy ends its walk beside the caster (`EnemyTurn::walked`, the AI's walk done), a dagger flies at it
  before it acts (`daggerInterrupts(..., arrived = true)`), played as an interrupt exchange (the Vigília's own path: `playInterrupts`,
  nobody's action, the enemy's turn goes on after it; a dead attacker never strikes).
- **Ataque corpo a corpo.** When an enemy's action is a blow or a weapon skill aimed at the caster (`melee`: the plain attack's target, a
  Combat step's defender, a weapon ability's ids) and the enemy is no archer, another dagger hits it before the blow. An enemy that walks
  up and strikes takes two (the arrival's and the blow's), the second only if the first left it standing (steps on the dead are skipped).
- **Flecha ou virote.** An arrow or a crossbow bolt aimed at the caster (a plain shot or counter by a unit whose attack is an arrow,
  `combat.arrow`; the arrow skills: Tiro Longo, Tiro Perfurante, Tiro Sangrento, Tiro Múltiplo, the Arqueiro's four, `arrowKind`) is met
  by a dagger and never lands ("Interceptado": a miss for the shooter, no roll, no damage, no rider); the dagger is spent
  (`daggerIntercepts`, in `resolveStrikeImpl` and the spell loop of `resolveStepImpl`).

The sheet shows what is left (`conditionsText`: "Adagas ×N"; the HUD's Status line too); the snapshot keeps it (`cond.daggers`). Enemy
counters in melee throw no dagger (the user's three cases are the enemy's own actions). Spells aimed at the caster are not reactions.

Code: `engine/world/Spells.cpp` (`kEmberSpells`, `emberClassFamily` (voss / warlock / elementalist → mage), `arrowKind`, `emberNumbers`,
`castEmber`, the Effect branch of `resolveStep`, the dagger spent at the top of a `daggerStrike` step, `daggerIntercepts`, `daggerInterrupts`,
`spellTooltip`, `spellInstant`, `spellAimValid`), `Spells.h` (`BattleStep::daggers`), `Battle.h` (`Conditions::daggers`), `BattleSave.cpp`,
`apps/sandbox/MapStage.h` (`EnemyTurn::walked`, the interrupt block of `updateTurns`, `playInterrupts(attacker, targets, melee, arrived)`,
`syncDaggers`, the dagger step with no pose, the "Interceptado" dart in `updateExchange`, script steps `dagger` / `shoot`). Icon: the user's
art, `content/ui/spells/cloudOfDaggers.jpg` (from `Attachments/MoreSpells`).

## Identificar and the unknown items

The rule (the user, 2026-10-21): part of what comes out of the chests is unknown: weapons and gear above a power line, and some potions
(`content/unknown-items.json`: `weaponPowerAbove` / `gearPowerAbove` 3 and `potions` potent / manaMid / manaLarge are first values, the
user settles them later; `sellShare` 0.1 = what the Inn pays). An unknown item shows only its type ("Espada desconhecida", "Equipamento
desconhecido (Cabeça)", "Poção desconhecida (Voss)"), a "?" in place of its art and no stats; it cannot be equipped or drunk (the pickers
and `InnRules::equipWeapon / equipItem` skip it); it can be thrown away (the Mochila's sheet) or sold to Vargan ("Itens desconhecidos" in
the forge, `sellShare` of the price, min 1 Gold). Identificar (every Mago Negro from level 1, a tier-1 use, one item per use) opens the
list of the party's unknown items on the world map's field-spells menu; a click reveals the chosen one for good (the real name in the
hint). Saved in the save: `unknownWeapons` [ids, each also in `weapons`], `unknownEquipment` {id: copies, among `looseEquipment`'s},
`unknownPotions` {hero: {kind: count}} (apart from `bags`: an identified potion goes into the hero's bag).

Where: `UnknownItemRules::shared()` (Battle.h / Battle.cpp) reads the file; `useLockpick` (Spells.cpp) marks the drops (`Battle::lootUnknown`,
`lootUnknownPotions`, in the snapshot; the chest popup names them "Arma desconhecida" / "Poção desconhecida" with a "?"; off while the
Cooker checks against Engine2: `progressionEnabled()`); `finishCampaignFight` (main.cpp) writes the marks into the save; `InnRules`
(`unknownItems / weaponUnknown / gearUnknown / potionsUnknown / sellUnknown / identify / discardUnknown`, `InnWeapon::weaponType` from
`content/data/weapons.json`); `FieldSpells::identifiers / canIdentify / castIdentify`; `InventoryScreen.h` (the entries, the "?", the
picker), `InnScreen.h` (the forge's list), `OverworldStage.h` (the row and `drawIdentifyMenu`; `--map-ui identify:<hero>`). Kill drops are not
kept by the game yet, so only chests make unknown items.

## Dissipar Magia and the Globo in the code

`Spells.cpp`: `emberNumbers` (the levels), `castEmber` (the area / the bubble's hexes), the Effect branches of `resolveStepImpl` (what each
strips; the foes' zones erased: `WebZone` now carries `side`), `globeBlocksHit` / `globeFilter` (in `resolveStep`: the shielded targets
dropped from a spell, heal, cure or effect step before it resolves, "Bloqueado"; `globeBlocks` public, the script's check), the zone hooks
(`zoneEffects`, `iceStormsStrike`, `tileHazard`, the web's sleep roll and clamp, the web's cast roll; `Battle::weakenAt / inZoneOf /
zoneDamageMul` read `shielded()`), the countdown at the top of `startOfTurnEffectsImpl`, `planEnemyCast` (the AI's plan dropped and its
use given back when the bubble shields every target). `Battle.h`: `Battle::Globe` (`globes()`, `addGlobe`, `globeAt`, `shielded`), in
the snapshot (`BattleSave.cpp` "globes"). `MapStage.h`: `syncGlobes` (the standing bubble, `MageFx::globeStart / globeEnd`), the
"Bloqueado" flash (`globeRipple`), the dispel's spots (bodies with card heights), script steps `afflict:<unit>:bless|rally|vigil|sleep|
fear|silence|mark|decay` and the checks of `cast:Voss:dispelMagic:<target>` / `cast:Voss:globeOfInvulnerability:Voss` / a spell at a
shielded target (the `cast` step leaves a shielded target where it stands).

## O efeito (version 01, `render/vfx/DaggerFx`, 3D on the Standard + Combat materials)

- **The cast:** violet motes gather from the ground to the chest while the pose plays, a flash and a ring on the ground at the impact; the
  daggers materialize one after another out of the flash (`summon`, then the cloud from `syncDaggers`).
- **The cloud:** real daggers lit by the scene (a steel blade with a point, a bronze guard and pommel, a dark grip) circling the body at
  chest height, points forward along the orbit, rolling slowly and bobbing; a violet glint on each point, a short arcane wake behind each
  blade; a faint violet ring turning at the feet; a soft violet light on the body. The daggers on the eye's side are drawn over the card,
  the far ones behind it. The count follows the rules (`setCount`: newcomers materialize, extra ones dissolve into motes).
- **A dagger thrown:** one dagger leaves its orbit, darts at the attacker on a shallow arc spinning on its axis with an arcane wake, and
  buries itself in the body: a flash with a cross glint, steel sparks, a violet ring square to the flight, a light; then it dissolves
  (`strike`, cast as the `daggerStrike` step: the caster plays no pose, the dagger's own sound is the dagger slash).
- **An interception:** the same dart, but it meets the arrow a step in front of its caster: a white clash, sparks both ways, the dagger
  shattering into spinning steel shards (`intercept`, played when a step's effects carry "Interceptado").

Tests: `--promote Voss=mage:12` (4 daggers), `--battle-script "cast:Voss:cloudOfDaggers:Voss; dagger:Piqueiro:Voss; far:Besteiro:Voss:3;
shoot:Besteiro:Voss"` (scratchpad `run_daggers.ps1`); `--fx-test cloudOfDaggers` / `daggerStrike` for the visuals alone.

## Os efeitos de Dissipar Magia e do Globo (version 01, `render/vfx/MageFx`, 3D on the Combat material)

- **Dissipar Magia:** motes of arcane light drawn in from the area's rim to its centre while the cast gathers (`kDispelLead`); at the impact
  a shell of violet light bursts out of the centre through the air to the area's edge, a ring runs out on the ground (a second, slower, after
  it), a column of light stands a moment at the centre; on every body the wave reaches (in order of distance) a shaft of white-violet light
  and the magic torn off it flung up and away as motes; a violet light.
- **Globo:** the bubble rises out of the ground as a shell of violet light (a flash, a ring) and stands for as long as the rules keep it: a
  faint glassy dome with a bright rim, breathing, motes drifting slowly on its surface, a ring turning at its base, a soft light inside; it
  fades when the rules drop it. A spell it stops flashes on its surface over the one it was aimed at (`globeRipple`). The cast itself plays
  the Nuvem's conjuring flash on the caster (the bubble is the stage's, from the rules' globe).

Tests: `--battle-script "afflict:Piqueiro:bless; afflict:Piqueiro:rally; cast:Voss:dispelMagic:Piqueiro; afflict:Kael:fear; afflict:Kael:silence;
afflict:Kael:mark; cast:Voss:dispelMagic:Kael"` and `"hurt; cast:Voss:globeOfInvulnerability:Voss; far:Salazar:Voss:2; cast:Salazar:cureMinor:Voss;
cast:Salazar:cureMinor:Kael"` (the cure on Voss blocked, the one on Kael landing).
