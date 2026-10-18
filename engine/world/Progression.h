#pragma once

// EmberEngine's own character progression (content/progression.json) over Engine2's class table
// (content/engine2/class-stats.json, Engine2's statsFor).
//
// Heroes: level 1 is Engine2's; every level after adds the hero's own gains (hp atk mag def dex), whatever class it is
// promoted to. Enemies (every unit not on the party's side, summons excepted): level 1 is Engine2's; each level adds the
// class's own Engine2 gains times the party's change in that stat (the six heroes' new gains summed / their old gains
// summed), so the world grows the way the party now does.
//
// On by default; the Cooker's checks against Engine2 turn it off (they compare Engine2's own numbers).

#include <string>

namespace ember::e2 {

struct BattleUnit;

void setProgressionEnabled(bool on);
bool progressionEnabled();

// A unit just read from a battle file (stats, level and growth as Engine2 wrote them): moved onto the progression.
// Gear and points over the class stats are kept; a hero's growth rows are rewritten for its level-ups.
void applyProgression(BattleUnit& u);

// The hero class's HP at a level under the progression (Engine2's when the class is no hero's, or it is off);
// `engine2Hp` = Engine2's statsFor(classId, level).hp. Used outside battle (camp HP, the world map, the Inn).
int progressionClassHp(const std::string& classId, int level, int engine2Hp);

}  // namespace ember::e2
