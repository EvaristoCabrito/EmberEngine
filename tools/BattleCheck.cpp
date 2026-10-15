#include "BattleCheck.h"

#include <algorithm>
#include <tuple>

#include "core/Log.h"
#include "world/Battle.h"

namespace ember {

bool checkBattle(const std::filesystem::path& battleJson) {
  std::optional<e2::Battle> battle = e2::Battle::load(battleJson);
  if (!battle) return false;
  auto key = [](const e2::ReachCell& c) { return std::tuple(c.y, c.x); };
  auto sorted = [&](std::vector<e2::ReachCell> v) {
    std::sort(v.begin(), v.end(), [&](const e2::ReachCell& a, const e2::ReachCell& b) { return key(a) < key(b); });
    return v;
  };
  size_t failed = 0, cells = 0;
  for (size_t i = 0; i < battle->units().size(); ++i) {
    const e2::BattleUnit& u = battle->units()[i];
    const std::vector<e2::ReachCell> ours = sorted(battle->reachable(i)), theirs = sorted(u.reference);
    cells += theirs.size();
    std::string problem;
    if (ours.size() != theirs.size()) problem = std::format("{} hexes, Engine2 {}", ours.size(), theirs.size());
    for (size_t k = 0; problem.empty() && k < ours.size(); ++k) {
      const e2::ReachCell &a = ours[k], &b = theirs[k];
      if (a.x != b.x || a.y != b.y)
        problem = std::format("hex ({}, {}) vs Engine2 ({}, {})", a.x, a.y, b.x, b.y);
      else if (a.cost != b.cost)
        problem = std::format("({}, {}) costs {}, Engine2 {}", a.x, a.y, a.cost, b.cost);
      else if (a.parentX != b.parentX || a.parentY != b.parentY)
        problem = std::format("({}, {}) reached from ({}, {}), Engine2 from ({}, {})", a.x, a.y, a.parentX, a.parentY, b.parentX, b.parentY);
    }
    if (!problem.empty()) {
      ++failed;
      logError("battle {}: {} ({} at {}, {}, mov {}): {}", battle->map(), u.name, u.classId, u.x, u.y, u.mov, problem);
    }
  }
  if (failed) {
    logError("battle {}: {} of {} units differ from Engine2", battle->map(), failed, battle->units().size());
    return false;
  }
  logInfo("battle {}: reach matches Engine2 ({} units, {} reachable hexes)", battle->map(), battle->units().size(), cells);
  return true;
}

}  // namespace ember
