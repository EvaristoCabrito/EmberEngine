# Kael: Vanguarda, Mestre da Espada e Arauto da Guerra

EmberEngine's own classes (the user, 2026-11-13: the design "Kael's martial skills", then "these are going to be new classes, i will make
another warrior to use shield skills" and "Vanguard only use Main Hand, or Two Handed Swords"). Kael is a **Vanguarda** (`vanguard`) and
becomes a **Mestre da Espada** (`swordmaster`) or an **Arauto da Guerra** (`warbringer`) at 15 (world map → Grupo → Caminho de classe).
The Guerreiro, the Paladino and the Cavaleiro Pesado stay as they were, for the shield warrior still to come.

The user's answers (2026-11-13): every technique costs its **AP + its cooldown + one use of its tier**; a **rank is one level**: rank 1 on
the level it is learned, rank 15 fourteen levels later, every number of the technique a little at each rank; the unlock levels below;
Kael keeps **Golpe Duplo** of the Guerreiro's skills and nothing else.

## Where it lives

- Data: `content/ember-classes.json` `heroClasses` (the three classes: the Guerreiro's stats, the Conjuração Média tier table, swords only,
  `twoHandedSwords`, `offHandLocked`) and `heroBaseClasses` (Kael's maps still say `swordsman`: he plays as a Vanguarda);
  `content/data/weapons.json` / `inn.json` (every sword usable by the three, `offHandLockedClasses`, the class names and tier uses),
  `overworld.json` (Kael's base class), `action-points.json` (`spells.byKind`: each technique's AP), `class-stats.json`, `classes.json`,
  `content/progression.json`. Written by `tools/scripts/data/add_vanguard_classes.py` (running it again changes nothing).
- Rules: `engine/world/SpellsVanguard.inc` (the techniques, their ranks, the steps they make, the guards / marks / stances, the movement
  reactions, the tooltips), included by `Spells.cpp`; the strike (`resolveStrikeImpl`: accuracy, armor bypass, parry, Riposta, pushes,
  Quebrar o Ritmo), `Combat.cpp` (`StrikeMods`, `martialAccuracy`), `Battle.h` (`Conditions::Martial`, `cooldowns`), `BattleSave.cpp`,
  `Battle.cpp` (Corte Baixo's AP), `ActionPoints.cpp` (a technique that opens with its step pays its AP), `Loadout.cpp` / `Inn.cpp`
  (the off hand locked), `MapStage.h` (the walk's reactions, the cooldown on the hotbar), `OverworldStage.h` (the class path).
- One code path: no test-mode branch anywhere; Modo teste plays the same rules.

## The common rules

- **Damage**: a share of an ordinary blow of his sword (`dmgMul`), the game's own blow (ATK, the weapon's roll and mastery, the 8 %
  critical, DEF). Every hit rolls its own hit chance. **Ignore X % of armor** = X % less of the defender's DEF part of the mitigation
  (floor(DEF / 2)); the strongest bypass on a hit applies; capped at 100 %.
- **Ranks**: `value = first + (max - first) × (rank - 1) / 14`, rounded half up (integer arithmetic). A prestige technique learned after
  16 keeps growing in Ember Ashes II (the game's rule for every spell).
- **Cooldown N**: blocks it for his next N turns (he acts once a round; `BattleUnit::cooldowns`). "—" = none.
- **Self buffs** end as his next turn begins. **Foe debuffs** last through the end of the foe's next full turn (laid with 2 of its turn
  starts). Reapplying keeps the stronger value. One **guard slot** (Guarda Cruzada, Ler a Lâmina): a new guard replaces the old.
- **Parry**: the next parryable blow that lands (a melee blow of a foe beside him; no arrow, no bolt, no magic staff) takes the guard's
  share off and counts as a parry. **Pé Firme** takes its own share off every physical hit.
- **Pushes**: straight away from Kael, one hex; only a one-hex body moves (the board's displacement rule); a body or an obstacle stops it;
  Pé Firme makes the first push on him one hex shorter. A push that does not happen ends the chain that needs it (Corte Impulsionado's
  follow, Avanço Rolante).
- **Bundled steps**: paid by the technique's AP; the hex is checked again as the step resolves (taken: he stays, the follow-up ends).
- Multi-target techniques (Talho Largo, Rachar a Fileira, Ceifar os Flancos, Estilhaçar a Linha) draw no counter, as the game's other area
  weapon skills; each target is its own blow and roll.

## Vanguarda (T1-T5 of the Média table: 1, 4, 7, 10, 13)

| Tier · nível | Técnica (kind) | PA | Recarga | 1º → máx. posto |
|---|---|---|---|---|
| T1 · 1 | Corte Medido (`measuredCut`) | 6 | — | inimigo ao lado: 100 → 115 %, +5 → +15 de acerto |
| T1 · 2 | Passo Longo (`longstepSlash`) | 7 | — | um passo para um hex livre ao lado, depois um inimigo ao lado dele: 90 → 115 % |
| T2 · 4 | Guarda Cruzada (`crossguard`) | 4 | — | o próximo golpe corpo a corpo que acertar: -30 → -55 % e conta como aparado |
| T2 · 5 | Golpe de Pomo (`pommelCheck`) | 5 | 1 | 45 → 65 %; o próximo ataque dele -10 → -25 de acerto |
| T3 · 7 | Corte Baixo (`lowCut`) | 6 | — | 80 → 105 %; o 1º hex do próximo movimento dele +1 PA (o 2º também do posto 8) |
| T3 · 8 | Corte de Saída (`partingCut`) | 7 | — | 90 → 115 %, depois um passo para um hex livre ao lado e fora do alcance dele (mesmo errando) |
| T4 · 10 | Talho por Cima (`overheadHew`) | 8 | — | 120 → 145 %, ignora 10 → 25 % da armadura |
| T4 · 11 | Pé Firme (`steadyFooting`) | 4 | 1 | -10 → -20 % de dano físico; o 1º empurrão nele anda 1 hex a menos |
| T5 · 13 | Desafio (`challenge`) | 5 | 1 | inimigo a até 2: o 1º ataque dele num aliado de Kael (não nele) -10 → -25 de acerto |
| T5 · 14 | Romper a Guarda (`breakGuard`) | 8 | 1 | 105 → 130 %; o próximo golpe físico que o acertar ignora 15 → 30 % da armadura |

## Mestre da Espada (from 15: T5 at 15, T6 16 / 17, T7 19 / 20, T8 22 / 23 / 25 / 27)

| Tier · nível | Técnica (kind) | PA | Recarga | 1º → máx. posto |
|---|---|---|---|---|
| T5 · 15 | Finta (`feint`) | 4 | — | Exposto 10 → 25 %: o próximo golpe de técnica de Kael nele ignora isso da armadura |
| T5 · 15 | Riposta (`riposte`) | 6 (o contra-ataque) | — | passiva: depois de aparar um golpe ao lado, o contra-ataque dele é a Riposta, 100 → 130 %, deixa Exposto 10 %; gasta um uso de T5 |
| T6 · 16 | Passo de Passagem (`passingStep`) | 7 | — | passo para um hex ao lado dele e do inimigo, 90 → 115 % |
| T6 · 17 | Ponta de Agulha (`needlePoint`) | 7 | — | 90 → 110 %, ignora 30 → 50 % |
| T7 · 19 | Segunda Intenção (`secondIntention`) | 9 | — | dois golpes de 50 → 65 %; o 1º aparado: o 2º +20 de acerto e ignora 20 % |
| T7 · 20 | Ler a Lâmina (`readTheBlade`) | 4 | 1 | o próximo golpe desse inimigo: -15 → -35 %, aparado, ele Exposto 15 % |
| T8 · 22 | Quebrar o Ritmo (`severTheRhythm`) | 8 | 2 | 95 → 125 %; o próximo contra-ataque dele não vem (este golpe incluído) |
| T8 · 23 | Corte Perfeito (`perfectCut`) | 9 | 1 | 115 → 145 %; Exposto: +35 → +45 pontos e o desvio da marca |
| T8 · 25 | Medida Mortal (`deadMeasure`) | 5 | 2 | marca um inimigo ao lado; o 1º passo dele para fora do alcance: Kael golpeia antes, 105 → 135 % (6 PA); acertou: o passo não acontece |
| T8 · 27 | Golpe de Mestre (`masterstroke`) | 11 | 3 | 145 → 180 %, ignora 25 → 40 %; Exposto: +20 de acerto |

Exposto: only Kael's technique hits (and the Riposta) use it; the first landed one spends it; a technique that lays it does so after its
own hit; on a landed Riposta the old mark is spent first, then the Riposta's laid.

## Arauto da Guerra (from 15, the same levels)

| Tier · nível | Técnica (kind) | PA | Recarga | 1º → máx. posto |
|---|---|---|---|---|
| T5 · 15 | Corte Impulsionado (`drivingCut`) | 8 | — | 90 → 115 %, empurra 1; saiu: Kael avança para o hex dele |
| T5 · 15 | Talho Largo (`wideCleave`) | 8 | — | arco de 3 hexes, 70 → 95 % em cada inimigo |
| T6 · 16 | Desvio Lateral (`sidecast`) | 7 | — | 75 → 100 %, o inimigo vai para um hex escolhido ao lado de Kael e do hex dele |
| T6 · 17 | Firmar a Linha (`plantTheLine`) | 5 | 1 | postura: o 1º inimigo que chegar ao lado andando leva 80 → 110 % (6 PA); acertou: o movimento dele acaba |
| T7 · 19 | Ombrada (`shoulderThrough`) | 8 | 1 | inimigo a 2 em linha reta, o meio livre: Kael entra no meio, 85 → 110 %, empurra 1 |
| T7 · 20 | Esmagar na Pedra (`crushAgainstStone`) | 9 | 1 | 110 → 135 %, empurra 1; contra parede / obstáculo / borda: +30 → 50 % do golpe, sem rolagem |
| T8 · 22 | Rachar a Fileira (`splitTheRank`) | 9 | 1 | reta de 2: o vizinho 95 → 120 %; acertou: o do 2º hex 65 → 90 % |
| T8 · 23 | Ceifar os Flancos (`reapTheFlanks`) | 9 | 1 | os 2 hexes de fora do arco, 80 → 105 %; acertou: empurra 1 |
| T8 · 25 | Avanço Rolante (`rollingAdvance`) | 10 | 2 | 90 → 115 % e empurra; saiu: avança, 70 → 95 % e empurra de novo; Kael fica no 1º hex |
| T8 · 27 | Estilhaçar a Linha (`shatterTheLine`) | 11 | 3 | arco: o centro 130 → 160 % e empurra 1; os lados 85 → 110 % e -10 → -20 de acerto no próximo ataque |

Movement reactions (Medida Mortal before a voluntary step, Firmar a Linha after one): checked hex by hex on every voluntary walk
(`MapStage::walkHexes` → `ember::reactBeforeStep / reactAfterStep`), at most one answer of his per step, his AP checked as it commits; a
blow that lands stops only a one-hex body; the hexes not walked are not paid.

## Decided here (defaults, the user can change them)

- The reactions (Riposta, Medida Mortal, Firmar a Linha) fire by themselves when he has the AP (and, for the Riposta, a T5 use): no
  question to the player during the enemy's turn. The Riposta replaces his plain counter for that event.
- Corte Impulsionado always follows when the push lands (the design's "may follow" has no choice on screen yet).
- Class names: Vanguarda / Mestre da Espada / Arauto da Guerra (Vanguard / Swordmaster / Warbringer in English); the technique names are
  ours in Portuguese, the design's in English.
- Desafio's points off count on the first hit of that action; a multi-hit action's later hits roll normally.
- Golpe de Pomo / Estilhaçar a Linha: a counter is the foe's next weapon attack, so a foe that strikes back at once takes the points
  off on that counter and the mark is spent (the user, 2026-11-13: "Yes, keep it").

## Check

`build\release\bin\VanguardCheck.exe` (tools/VanguardCheck.cpp) from the project root: the classes, the ranks, every technique's AP and
use, each technique played on aldeia015, the guards and the Riposta, the reactions, the cooldowns, Corte Baixo's AP, the off hand and the
snapshot round trip. 101 checks, all passing on 2026-11-13.

## Not done

- Effects, sounds and icons of their own: the techniques play Kael's attack pose and the ordinary blade impact.
- Passo de Passagem's "suppresses the selected enemy's movement reactions": no enemy has a movement reaction yet, so nothing to suppress.
- Action XP is per hit as for every other multi-hit skill (the design: one use per technique).
- An enemy never uses them (no AI).
