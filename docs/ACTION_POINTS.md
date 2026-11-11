# Pontos de Ação (PA) — Ember Ashes

EmberEngine's own (the user, 2026-11-06). Every unit in a fight acts from one reserve of Action Points: attacks, weapon skills,
spells, potions, Arrombar, Examinar, every hex walked and every counter-attack come out of the same pool. Code:
`engine/world/ActionPoints.h/.cpp` (the one resolver); data: `content/data/action-points.json`; check: `tools/ActionPointsCheck.cpp`.

## The rules in force

| Rule | Value | Status |
|---|---|---|
| Base AP | 12 | design |
| Ceiling of any maximum (`maxAp`) | 34; the HUD bar runs 1 to 34 | design (the user, 2026-11-06) |
| DEX | +1 per 10 complete points (`floor(DEX / 10)`) | design |
| Armor: unarmored / heavy plate | +5 / +0 | design (confirmed) |
| Armor: cloth, light leather, reinforced leather, chainmail | +4 / +3 / +2 / +1 | provisional (configuration defaults) |
| Dagger attack | 5 | design |
| Sword / axe / blunt (mace, hammer, club) attack | 6 | design |
| Polearm (spear, lance, pike, halberd) / bow / crossbow attack | 7 | design |
| Staff ordinary blow | 6 | provisional |
| Unarmed / no weapon data | 6 | provisional |
| Spells and special skills (weapon skills included) | the caster's own attack cost in tiers 1-2, then +1 every two tiers (T3-4 +1, T5-6 +2, T7-8 +3, T9-10 +4); `spells.fromAttack` | design (the user, 2026-11-10) |
| Potion | 5 | design |
| Staff Counter / Spell Counter | 6 / 7 | design |
| Any other counter | the defender's own weapon attack cost | provisional |
| Flat hex (plains) | 2 | design (the user, 2026-11-10) |
| Other terrain | the tile's own Ember `moveCost` +1 (road/ruins/snow 2, woods 3, hill 3, ember 3, flame 4 ...) | design (the user, 2026-11-10: +1 on every terrain), configurable per terrain id |
| Examinar | 3 | provisional (value not established) |
| Arrombar | 5 | provisional |
| Enemy weapon category by class | milícia, captains, brigands, rogues, the Butcher = sword; pikemen, lancers = polearm; crossbowmen = bow; cultists, sorcerers = staff; the rest blunt | provisional (Ember exports no weapons for enemies; the same split `MapStage::weaponStyle` draws) |
| Enemy armor | the default (unarmored, +5) unless `armor.classes` names the class | provisional |

Formulas (one path, `ActionPoints.cpp`): flat parts are summed first, then the percentages, one rounding (`rounding`: floor for
the maximum and the actions, round for movement), never under zero (movement never under `movement.minimum`).

```
MaxAP          = max(0, 12 + floor(DEX/10) + armor + equipment + talents + status + modifiers(Max))        then %
ActionCost     = max(0, base(action) + modifiers(Action covering it))                                       then %
HexCost        = max(minimum, terrain apCost ?? tile moveCost + terrain modifier + zones + status + modifiers(Movement))  then %
```

## Where it lives in the engine

- `BattleUnit::ap`, `apRefreshRound`, `armorCategory`, `wornItems`, `apMods` (`Battle.h`); `ReachCell::ap` is the AP of the walk to a hex.
- Movement: `Battle::stepCost` prices each hex (`apStepCost`); `Battle::reachable` keeps only hexes whose movement *and* AP can be
  paid, ordered by AP first (identical to Ember's order while the terrain table is at its defaults, which the BattleCheck reach
  comparison still verifies); `Battle::move` / `MapStage::walkAlong` spend the walk's AP; `legalVoluntaryPath` refuses an unpayable
  route. Free roam (explore maps, fog maps before contact) walks for nothing (`movement.freeWhileRoaming`). The MOV cap stays
  (`movement.cappedByMov`): a turn's walk is limited by both.
- Actions: `MapStage::play` is the single commit (`apCommitSteps`): validated and spent once, before any pose plays; an
  unaffordable action plays nothing and spends nothing. `attack` refuses before `play`; `castSpell`, `usePotion` refuse before a
  use or a potion is spent; `useLockpick` spends at its own commit. Interrupts (a sentinel's thrust, the dagger cloud, a bursting
  mark) are nobody's action and cost nothing.
- Counters: `strikesBack` (and `forecast.canCounter`) require the defender's AP for the counter it would make (`apCounterType`:
  "staff" beside the attacker with a staff, "spell" for an arcane bolt from afar, the off-hand dagger when it reaches, else its weapon's
  category); `resolveStrike(counter = true)` is the spend. NO AP, NO COUNTER, no borrowing, no negative balance.
- Refresh: `Battle::beginTurn` → `apRefresh`: expiring modifiers count down, then the policy (`refresh.policy`: replace = the full
  maximum; accumulate = added, capped). Once per round (`apRefreshRound`), so a resumed fight never refreshes twice. What is left
  at the end of a turn stays for counters through the other units' turns.
- Armor swaps in the fight (`MapStage::syncInventory`): the reserve moves by the change of the maximum (`apOnMaxChanged`), clamped
  to [0, max]: swapping back and forth gains nothing.
- AI: `Battle::planEnemy` only strikes from hexes where the walk and the blow are payable; the score adds exactly the AP the strike
  leaves in reserve (times `ai.counterReserveWeight`, 1), so the enemy prefers the blow that keeps it more AP for counters. `planEnemyCast` refunds and drops a cast the enemy cannot pay. Enemies then walk,
  strike and cast through the same `walkAlong` / `attack` / `play` as the party.
- UI (`MapStage.h`): "PA x/y" on the HUD line, each bar command shows its cost and is dead when unaffordable, the hovered route shows
  "Caminho N PA", the forecast shows the blow's cost, the target's AP and the counter's cost (or "sem PA"), hotbar sockets need the AP and show it (top right, and "Custo: N PA" in the tooltip), the Skills sheet shows each one's PA,
  the status sheet has a PA cell (tooltip: the formula and the armor category). "Examinar · N PA" opens a click on a visible foe.
- Examinar (`apExamine`): the category from the real forecasts both ways (turns the hero needs to down it over turns it needs to down
  the hero: ≤0.5 Alvo fácil, ≤1 Desafio moderado, ≤2 Oponente difícil, else Ameaça mortal; thresholds in `examine.categories`),
  the foe's numbers, weapon category, attack and counter costs, resistances, conditions. Does not spend the turn's action
  (`examine.spendsAction`). The foe stays examined for the fight (snapshot `examined`).
- Save: `apWriteSnapshot` / `apReadSnapshot` (`BattleSave.cpp`): `ap`, `apRefreshRound`, `armorCategory`, `wornItems`, `apMods`,
  and the battle's `examined`. A snapshot saved before AP resumes with the full maximum once.

## Modifiers (`ApModifier`)

Source (`ApModifierSource`: attribute, armor, equipment, talent, passiveAbility, activeAbility, buff, debuff, statusEffect, terrain,
environmentalEffect, other), scope (Max / Action / Movement), a filter (an action: "" all, a spell kind, a weapon category, "attack",
"offhand", "spell", "potion", "examine", "lockpick", "counter", "counter:staff"; a terrain id or category for Movement), flat, pct,
turns (-1 persistent; counted down at the unit's own turns). Flat before pct, one rounding, clamp at zero. The configuration also
takes per-status, per-talent and per-equipment bonuses (`status`, `talents`, `equipment` in the JSON): all empty, none designed yet.

## Checks

`build\release\bin\ActionPointsCheck.exe` from the project root: the 25 checks of the design plus spells / skills at 8, enemy
class weapons, Examinar, an older snapshot. BattleCheck (Cooker check-battle) still compares every unit's reach with Ember's.

## Not done / to decide

- The intermediate armor bonuses, the chest-piece category mapping, enemy weapon and armor classes, the staff blow, unarmed,
  Arrombar, Examinar's cost and the terrain table are all provisional defaults in the JSON.
- No existing condition, talent or piece of gear changes AP yet (the hooks are there, the tables are empty).
- Movement keeps Ember's MOV cap on top of AP (`movement.cappedByMov: true`); set it false to let AP alone limit a walk.
