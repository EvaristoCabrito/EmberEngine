# Garran Holt: Guardião da Lua (Moonwarden)

The user, 2026-11-13: "Garran Holt — Moonwarden", "Guardião da Lua em Português", then the design "Moonwarden — Permanent Skills,
T1–T10", which replaced the first moon-phase draft (ten skills over T1-T5, three changed by the moon): one class from level 1 to 30,
one permanent skill a tier. The skills work in every form (human, intermediate, werewolf, full wolf); only the human form is in the
game. His loop: guard an ally, take part of the hit, punish the foe responsible.

## Where it lives

- Data: `content/ember-classes.json` `heroClasses.moonwarden` (the Guerreiro's stats, the casters' tier table - T1-T10 open at 1, 3,
  6, 8, 11, 13, 16, 18, 21, 23 - one-handed swords, a shield in the off hand), `content/data/weapons.json` / `inn.json`,
  `overworld.json`, `class-stats.json`, `classes.json`, `action-points.json` (`spells.byKind`), `content/progression.json` (Garran
  Holt: the Guerreiro's gains). Written by `tools/scripts/data/add_moonwarden_class.py` (running it again changes nothing).
- Rules: `engine/world/SpellsMoonwarden.inc` (aims, casts, the guards, Assumir o Peso's transfer, Vigiar a Brecha's reaction, the
  words), included by `SpellsVanguard.inc`: the same technique system as Kael's (`kTechniques`, ranks, cooldowns, one use of the
  tier). Hooks: `Spells.cpp` `resolveStrikeImpl` (Puxar para Longe's aim, the reductions, the transfer), `Battle.h`
  (`Conditions::Martial`), `BattleSave.cpp`.
- Party: on the Party screen's roster (`Overworld::kPartyHeroes`, `content/ember-classes.json` "heroes"), on the reserve in Modo teste.
- One code path: no test-mode branch anywhere.

## Common rules

- Damage: a share of an ordinary blow of his weapon. Ranks: one a level from the level learned, 15 ranks (Kael's rule).
- Cooldown N blocks it for his next N turns. His guards last until his next turn begins.
- Several reductions of his on one hit: the strongest applies, only its charge is spent. Kael's guards stay apart.

## Técnicas

| Tier · nível | Técnica (kind) | PA | Recarga | 1º → máx. posto |
|---|---|---|---|---|
| T1 · 1 | Golpe Firme (`steadyStrike`) | 6 | — | inimigo ao lado: 100 → 115 %, +5 → +15 de acerto |
| T2 · 3 | Firmar o Pé (`digIn`) | 4 | 1 | o primeiro golpe físico nele -20 → -35 %; o primeiro empurrão, puxão ou arrasto nele anda 1 hex a menos (cada um a sua carga) |
| T3 · 6 | Passo de Flanco (`flankStep`) | 7 | — | um passo para um hex livre ao lado, depois um inimigo ao lado dele: 85 → 115 % |
| T4 · 8 | Cerrar Fileiras (`closeRanks`) | 5 | 1 | aliado a até 2; até um passo para terminar ao lado dele; o primeiro golpe de arma direto nele -20 → -35 %, enquanto ao lado |
| T5 · 11 | Quebrar o Passo (`breakStride`) | 6 | 1 | 75 → 100 %; o primeiro passo do próximo turno dele +1 PA (os dois primeiros no posto máximo) |
| T6 · 13 | Vigiar a Brecha (`watchTheGap`) | 4 (+6 na reação) | 2 | vigia um hex ao lado; o primeiro inimigo que entrar nele andando leva 75 → 105 % (6 PA); acertou: o movimento acaba |
| T7 · 16 | Golpe Seguido (`followThrough`) | 9 | 2 | dois inimigos diferentes ao lado: 100 → 125 % e 55 → 80 %, cada um rola sozinho |
| T8 · 18 | Puxar para Longe (`pullClear`) | 5 | 2 | um aliado ao lado vai 1 hex para um hex livre também ao lado dele; o próximo ataque de arma do aliado +15 → +25 de acerto |
| T9 · 21 | Assumir o Peso (`takeTheWeight`) | 6 | 3 | aliado ao lado: no primeiro golpe de arma de alvo único que ele levar, ainda ao lado, 30 → 45 % do dano (depois da mitigação) vai para Garran, direto |
| T10 · 23 | Acerto de Contas (`reckoning`) | 9 | 3 | 125 → 155 %; +35 → +50 pontos se Cerrar Fileiras ou Assumir o Peso protegeu um aliado desse inimigo desde o início do turno anterior; máx. 205 % |

## How a pick works on screen

- Passo de Flanco: the hex first, then the foe beside it. Golpe Seguido: the first foe, then the second.
- Cerrar Fileiras: the ally first, then where he ends (his own hex: no step). Puxar para Longe: the ally, then the hex it goes to.
- Vigiar a Brecha: the watched hex.

## Decided here (defaults, the user can change them)

- Golpe Seguido's two blows draw no counter (as every multi-target weapon skill of the game).
- Cerrar Fileiras' "direct weapon hit": any weapon hit on the ally, melee or ranged.
- Firmar o Pé's push or pull: the first hostile push (the techniques', Empurrão de Escudo, Investida), the Lanceiro's Gancho pull or
  his Empalar e Arrastar drag on Garran goes 1 hex shorter (the Lanceiro following the drag stops 1 hex shorter too); a 1-hex pull or
  drag does not move him.
- Quebrar o Passo's tax and Puxar para Longe's aim last through the end of the foe's / ally's next turn.
- In the campaign nobody recruits him yet; Modo teste has him.

## Look

- Sprites: the user's temporary sheets (`content/sprites/moonwarden-v01/human`, set `moonwarden-v01`, hero `garranHolt`): idle, walk,
  attack, special (his blows), block (his guards), hit, fall.
- Effects: `engine/render/vfx/MoonwardenFx` (moonlight, ours, 3D), one look per skill, played from `MapStage` as the step lands.
- Icons: `content/ui/spells/<kind>.jpg`, from the user's `Attachments/Icons` (98gm6 Golpe Firme, 72aWB Firmar o Pé, VyGHz Passo de
  Flanco, zFOt6 Cerrar Fileiras, sNZUZ Quebrar o Passo, Cg1KH Vigiar a Brecha, LSx9Q Golpe Seguido, U6vCA Puxar para Longe, Y9dBy
  Assumir o Peso, sz9Xs Acerto de Contas).

## Not done

- The other forms (intermediate, werewolf, full wolf) and transforming; the werewolf sheets are in
  `content/sprites/moonwarden-v01/werewolf`, unused. Sounds of his own. An enemy never uses them (no AI).
