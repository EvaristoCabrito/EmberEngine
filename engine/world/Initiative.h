#pragma once
// EmberEngine's own initiative bonuses (the user, 2026-10-18), replacing Ember's class `init` in play: the battle-opening
// roll stays 1d20 + bonus (Battle::rollInitiative). Rogues are the fastest, warriors sit in the middle, mages are the slowest.
// The heroes go by name (a promotion keeps them); the enemies by class; a class not listed keeps the battle file's own bonus.
// Only the game applies it (MapStage::setUpBattle): the battle files and the checks against Ember keep Ember's numbers.
#include "world/Battle.h"

#include <string_view>

namespace ember {

inline void applyEmberInitiative(BattleUnit& u) {
  struct Entry {
    std::string_view key;
    int bonus;
  };
  static constexpr Entry kHeroes[] = {
      {"Neera", 10}, {"Aldric", 8}, {"Kael", 7}, {"Salazar", 5}, {"Voss", 4}, {"Malrec", 4},
  };
  static constexpr Entry kEnemies[] = {
      // rogues
      {"assassin", 12}, {"rogue", 11}, {"brigand", 10}, {"brigandCrossbowman", 10}, {"archer", 10}, {"ranger", 10},
      // beasts (fast warriors)
      {"mordavianWolfFinal", 9}, {"wardog", 8}, {"wardog2", 8}, {"swampBlueCalf", 8},
      // warriors
      {"captain", 8}, {"baseCrossbowman", 7}, {"miliciaV2", 7}, {"lancer", 7}, {"sandoval", 7}, {"swordsman", 7},
      // heavy warriors
      {"basePikeman", 6}, {"punisher", 6}, {"theButcher", 6},
      // brutes
      {"horror", 5}, {"undeadOx", 5}, {"troll", 5}, {"troll2", 5},
      // slow brutes
      {"zombie", 4}, {"zombie2", 4}, {"sapling", 4}, {"ancientGolem", 4},
      // mages
      {"sorcerer", 4}, {"cultist", 3}, {"cultistV2", 3},
  };
  if (u.side == "player" && !u.summoned) {
    for (const Entry& e : kHeroes)
      if (u.name == e.key) u.initiativeBonus = e.bonus;
  } else if (u.side == "enemy" && !u.summoned) {
    for (const Entry& e : kEnemies)
      if (u.classId == e.key) u.initiativeBonus = e.bonus;
  }
}

}  // namespace ember
