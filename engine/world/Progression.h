#pragma once

// EmberEngine's own character progression (content/progression.json) over Ember's class table
// (content/data/class-stats.json, Ember's statsFor).
//
// Heroes: level 1 is Ember's; every level after adds the hero's own gains (hp atk mag def dex), whatever class it is
// promoted to. Enemies (every unit not on the party's side, summons excepted) keep their class's own fixed start and
// fixed gain per level exactly as the class table has them (the user, 2026-10-19: no multiplier), plus 3 attribute points
// per level after the first, rolled per unit as the battle loads by what it uses (Progression.cpp enemyPoints), and their rank's skills
// (content/enemy-ranks.json).
//
// On by default; the Cooker's checks against Ember turn it off (they compare Ember's own numbers).

#include <string>

namespace ember {

struct BattleUnit;

void setProgressionEnabled(bool on);
bool progressionEnabled();

// A unit just read from a battle file (stats, level and growth as Ember wrote them): moved onto the progression.
// Gear and points over the class stats are kept; a hero's growth rows are rewritten for its level-ups.
void applyProgression(BattleUnit& u);

// The hero class's HP at a level under the progression (Ember's when the class is no hero's, or it is off);
// `classicHp` = Ember's statsFor(classId, level).hp. Used outside battle (camp HP, the world map, the Inn).
int progressionClassHp(const std::string& classId, int level, int classicHp);

}  // namespace ember
