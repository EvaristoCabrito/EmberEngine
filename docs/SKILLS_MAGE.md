# As magias do Mago Negro (Voss): Nuvem de Adagas

2026-10-21, the user's spec (pasted, "Este bloco substitui o item 2 do prompt"; "Criar essa magia e seu efeito visual"): the Mago Negro
(`mage`; Voss's own class id is `voss` in most battle files; the Bruxo and the Elementalista are his promotions, `emberClassFamily` → `mage`,
so they keep it) gets a Tier 4 arcane spell at level 8 whose daggers react on their own. Every number below is the user's; the dagger's
blow has no scaling track (MAG/2 × 0.8 + 1D6 at every level), only the count grows, on the levels the user named.

| Nível · tier | Magia (kind) | Alvo | Efeito | Escala |
|---|---|---|---|---|
| 8 · T4 | Nuvem de Adagas (`cloudOfDaggers`) | ele mesmo (instantânea) | 3 adagas girando em volta do Voss, à vista de todos, até serem usadas ou a batalha acabar; lançar de novo repõe o total (nunca somam). Cada uma reage sozinha, sem gastar a ação dele, e é gasta na reação | 4 adagas no nível 12, 5 no 15 (`kSpellTracks`: `- - - c1 - - c1 ...`, escrito à mão em `tools/scripts/spells/tracks.py` FIXED) |

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

Tests: `--promote Voss=mage:12` (4 daggers), `--battle-script "cast:Voss:cloudOfDaggers:Voss; dagger:Piqueiro:Voss; far:Besteiro:3:Voss;
shoot:Besteiro:Voss"` (scratchpad `run_daggers.ps1`); `--fx-test cloudOfDaggers` / `daggerStrike` for the visuals alone.
