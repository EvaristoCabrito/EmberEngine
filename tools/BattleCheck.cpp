#include "BattleCheck.h"

#include <algorithm>
#include <tuple>

#include "core/Log.h"
#include "world/Battle.h"
#include "world/Combat.h"

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

  // Combat: every reference forecast and seeded roll.
  size_t combatFailed = 0;
  auto report = [&](const std::string& what) {
    if (++combatFailed <= 12) logError("battle {}: {}", battle->map(), what);
  };
  for (const e2::ForecastReference& r : battle->forecastReferences()) {
    const e2::Forecast f = e2::forecast(*battle, static_cast<size_t>(r.attacker), {r.x, r.y}, static_cast<size_t>(r.defender));
    const bool hit = e2::canHitFrom(*battle, static_cast<size_t>(r.attacker), {r.x, r.y}, static_cast<size_t>(r.defender));
    if (f.dmgOut != r.dmgOut || f.dmgBack != r.dmgBack || f.hitOut != r.hitOut || f.hitBack != r.hitBack || f.canCounter != r.canCounter ||
        f.kill != r.kill || hit != r.canHit)
      report(std::format("forecast {} -> {} from ({}, {}): ours out {} {}% back {} {}% counter {} kill {} reach {}; Engine2 {} {}% {} {}% {} {} {}",
                         battle->units()[static_cast<size_t>(r.attacker)].name, battle->units()[static_cast<size_t>(r.defender)].name, r.x, r.y,
                         f.dmgOut, f.hitOut, f.dmgBack, f.hitBack, f.canCounter, f.kill, hit, r.dmgOut, r.hitOut, r.dmgBack, r.hitBack,
                         r.canCounter, r.kill, r.canHit));
  }
  for (const e2::RollReference& r : battle->rollReferences()) {
    const e2::Fighter a = e2::fighterAt(*battle, static_cast<size_t>(r.attacker), r.x, r.y);
    const e2::BattleUnit& du = battle->units()[static_cast<size_t>(r.defender)];
    const e2::Fighter d = e2::fighterAt(*battle, static_cast<size_t>(r.defender), du.x, du.y);
    e2::Mulberry32 rng(r.seed);
    const e2::Hit h = e2::rollHit(*battle, a, d, rng);
    if (h.rolled != r.dmg || h.crit != r.crit || h.landed != r.landed || h.hitChance != r.hitChance || h.preCritDmg != r.preCritDmg ||
        h.dmg != r.finalDmg)
      report(std::format("roll seed {} {} -> {}: ours {} crit {} landed {} {}% pre {} final {}; Engine2 {} {} {} {}% {} {}", r.seed,
                         battle->units()[static_cast<size_t>(r.attacker)].name, du.name, h.rolled, h.crit, h.landed, h.hitChance, h.preCritDmg,
                         h.dmg, r.dmg, r.crit, r.landed, r.hitChance, r.preCritDmg, r.finalDmg));
  }
  // The enemies' decisions (Engine2 runAiFor), at the start and placed near the party; casts are not ported yet.
  size_t aiChecked = 0, aiSkipped = 0;
  for (const e2::AiReference& r : battle->aiReferences()) {
    if (r.spell) {
      ++aiSkipped;
      continue;
    }
    e2::Battle copy = *battle;
    e2::BattleUnit& u = copy.units()[static_cast<size_t>(r.unit)];
    if (r.at) u.x = r.at->x, u.y = r.at->y;
    const e2::AiPlan plan = copy.planEnemy(static_cast<size_t>(r.unit));
    ++aiChecked;
    if (plan.path != r.plan.path || plan.target != r.plan.target) {
      std::string ours, theirs;
      for (const e2::BattleHex& p : plan.path) ours += std::format(" ({},{})", p.x, p.y);
      for (const e2::BattleHex& p : r.plan.path) theirs += std::format(" ({},{})", p.x, p.y);
      report(std::format("ai {} at ({}, {}): ours walk{} strikes {}; Engine2 walk{} strikes {}", u.name, u.x, u.y, ours, plan.target, theirs,
                         r.plan.target));
    }
  }
  if (combatFailed) {
    logError("battle {}: {} combat answers differ from Engine2", battle->map(), combatFailed);
    return false;
  }
  if (aiChecked) logInfo("battle {}: enemy decisions match Engine2 ({} checked, {} casts skipped)", battle->map(), aiChecked, aiSkipped);
  if (!battle->forecastReferences().empty())
    logInfo("battle {}: combat matches Engine2 ({} forecasts, {} seeded rolls)", battle->map(), battle->forecastReferences().size(),
            battle->rollReferences().size());
  return true;
}

}  // namespace ember
