# Enemy ranks (classes of strength, F .. S++)

The user's design, 2026-10-18. EmberEngine's own rule, not Engine2's. Game data: `content/enemy-ranks.json`
(the game reads this file; change the numbers or the classes there). Code: `engine/world/Progression.cpp` (`applyRank`, run on
every unit not on the party's side when a battle is loaded; off in the Cooker's checks).

## The rule

Every enemy (and neutral) unit has a rank. Its skills = **rank's value per level × its level**, capped at 100. That one value is:

- its **weapon skill**: accuracy 75 + skill and dice × (100 + skill) / 100, exactly as a hero's weapon mastery
  (it replaces Engine2's enemy accuracy of 90 + DEX);
- its **Esquiva** (Dodging): −0.5 % to the hit chance of blows aimed at it per point;
- every **resistance** (fire, lightning, ice, arcane, darkness, holy, poison, ember): + skill over its class's own.

Hit chance for every weapon blow: clamp(0..100, accuracy + 10 from behind + (attacker DEX − defender DEX) / 3 − defender's
Esquiva / 2). Escape: min(100, 60 + DEX / 3).

## Value per level

| Rank | Per level | Lv 10 | Lv 20 | Lv 30 |
|---|---|---|---|---|
| F | 0.5 | 5 | 10 | 15 |
| E | 1.0 | 10 | 20 | 30 |
| D | 1.5 | 15 | 30 | 45 |
| C | 2.0 | 20 | 40 | 60 |
| B | 2.5 | 25 | 50 | 75 |
| A | 3.0 | 30 | 60 | 90 |
| S | 3.5 | 35 | 70 | 100 |
| S+ | 4.0 | 40 | 80 | 100 |
| S++ | 4.5 | 45 | 90 | 100 |

## Classification (the user's)

- **F**: every human unit (militia, crossbowmen, pikemen, lancers, cultists, captain, rogue, assassin, archers, sorcerers,
  executioner, ...). F is also the default for any class not listed.
- **E**: War Dog (wardog, wardog2), Horror, Mordavian Wolf, Muda (sapling), Zombie (zombie, zombie2), and every other
  non-human: Cobalt Blue Deer (swampBlueCalf).
- **D**: the big ones: Undead Ox (undeadOx), Cave Troll (troll, troll2), Ancient Golem (ancientGolem); and The Butcher (theButcher).
- **B**: Carnivorous Plant (carnivorousPlant).
- **A**: Birolho (birolho, birolho2, birolho3, birolhoLegs, birolhoLegs2).
- **S**: Asherah.
- **S+ / S++**: empty for now, for future bosses.

## Where it shows

The battle status sheet of an enemy: "Classe X" beside its level, and "Perícias da classe X: N/100".
