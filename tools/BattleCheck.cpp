#include "BattleCheck.h"

#include <algorithm>
#include <map>
#include <tuple>

#include "core/Log.h"
#include "world/Battle.h"
#include "world/Combat.h"
#include "world/Spells.h"

namespace ember {

bool checkBattle(const std::filesystem::path& battleJson) {
  std::optional<ember::Battle> battle = ember::Battle::load(battleJson);
  if (!battle) return false;
  auto key = [](const ember::ReachCell& c) { return std::tuple(c.y, c.x); };
  auto sorted = [&](std::vector<ember::ReachCell> v) {
    std::sort(v.begin(), v.end(), [&](const ember::ReachCell& a, const ember::ReachCell& b) { return key(a) < key(b); });
    return v;
  };
  size_t failed = 0, cells = 0;
  for (size_t i = 0; i < battle->units().size(); ++i) {
    const ember::BattleUnit& u = battle->units()[i];
    const std::vector<ember::ReachCell> ours = sorted(battle->reachable(i)), theirs = sorted(u.reference);
    cells += theirs.size();
    std::string problem;
    if (ours.size() != theirs.size()) problem = std::format("{} hexes, Ember {}", ours.size(), theirs.size());
    for (size_t k = 0; problem.empty() && k < ours.size(); ++k) {
      const ember::ReachCell &a = ours[k], &b = theirs[k];
      if (a.x != b.x || a.y != b.y)
        problem = std::format("hex ({}, {}) vs Ember ({}, {})", a.x, a.y, b.x, b.y);
      else if (a.cost != b.cost)
        problem = std::format("({}, {}) costs {}, Ember {}", a.x, a.y, a.cost, b.cost);
      else if (a.parentX != b.parentX || a.parentY != b.parentY)
        problem = std::format("({}, {}) reached from ({}, {}), Ember from ({}, {})", a.x, a.y, a.parentX, a.parentY, b.parentX, b.parentY);
    }
    if (!problem.empty()) {
      ++failed;
      logError("battle {}: {} ({} at {}, {}, mov {}): {}", battle->map(), u.name, u.classId, u.x, u.y, u.mov, problem);
    }
  }
  if (failed) {
    logError("battle {}: {} of {} units differ from Ember", battle->map(), failed, battle->units().size());
    return false;
  }
  logInfo("battle {}: reach matches Ember ({} units, {} reachable hexes)", battle->map(), battle->units().size(), cells);

  // Combat: every reference forecast and seeded roll.
  size_t combatFailed = 0;
  // the first two differences of every kind of check, and a count of the rest at the end
  std::map<std::string, int> seenKinds;
  auto report = [&](const std::string& what) {
    ++combatFailed;
    const int n = ++seenKinds[what.substr(0, what.find(" from"))];
    if (n <= 2) logError("battle {}: {}", battle->map(), what);
  };
  for (const ember::ForecastReference& r : battle->forecastReferences()) {
    const ember::Forecast f = ember::forecast(*battle, static_cast<size_t>(r.attacker), {r.x, r.y}, static_cast<size_t>(r.defender));
    const bool hit = ember::canHitFrom(*battle, static_cast<size_t>(r.attacker), {r.x, r.y}, static_cast<size_t>(r.defender));
    if (f.dmgOut != r.dmgOut || f.dmgBack != r.dmgBack || f.hitOut != r.hitOut || f.hitBack != r.hitBack || f.canCounter != r.canCounter ||
        f.kill != r.kill || hit != r.canHit)
      report(std::format("forecast {} -> {} from ({}, {}): ours out {} {}% back {} {}% counter {} kill {} reach {}; Ember {} {}% {} {}% {} {} {}",
                         battle->units()[static_cast<size_t>(r.attacker)].name, battle->units()[static_cast<size_t>(r.defender)].name, r.x, r.y,
                         f.dmgOut, f.hitOut, f.dmgBack, f.hitBack, f.canCounter, f.kill, hit, r.dmgOut, r.hitOut, r.dmgBack, r.hitBack,
                         r.canCounter, r.kill, r.canHit));
  }
  for (const ember::RollReference& r : battle->rollReferences()) {
    const ember::Fighter a = ember::fighterAt(*battle, static_cast<size_t>(r.attacker), r.x, r.y);
    const ember::BattleUnit& du = battle->units()[static_cast<size_t>(r.defender)];
    const ember::Fighter d = ember::fighterAt(*battle, static_cast<size_t>(r.defender), du.x, du.y);
    ember::Mulberry32 rng(r.seed);
    const ember::Hit h = ember::rollHit(*battle, a, d, rng);
    if (h.rolled != r.dmg || h.crit != r.crit || h.landed != r.landed || h.hitChance != r.hitChance || h.preCritDmg != r.preCritDmg ||
        h.dmg != r.finalDmg)
      report(std::format("roll seed {} {} -> {}: ours {} crit {} landed {} {}% pre {} final {}; Ember {} {} {} {}% {} {}", r.seed,
                         battle->units()[static_cast<size_t>(r.attacker)].name, du.name, h.rolled, h.crit, h.landed, h.hitChance, h.preCritDmg,
                         h.dmg, r.dmg, r.crit, r.landed, r.hitChance, r.preCritDmg, r.finalDmg));
  }
  // The enemies' decisions (Ember runAiFor), at the start and placed near the party; casts are not ported yet.
  size_t aiChecked = 0, aiSkipped = 0;
  for (const ember::AiReference& r : battle->aiReferences()) {
    // a cast EmberEngine does not port yet (other monsters' spells): skipped
    const bool ported = std::all_of(r.casts.begin(), r.casts.end(), [](const ember::AiReference::Cast& c) {
      return c.kind == "lightning" || c.kind == "magicMissile" || c.kind == "shock" || c.kind == "fantomForce" || c.kind == "frost" || c.kind == "longShot" ||
             c.kind == "piercing" || c.kind == "bloodyShot" || c.kind == "multiShot" || c.kind == "causticVenom" || c.kind == "minorVenom" ||
             c.kind == "burningHands" || c.kind == "poisonBreath" || c.kind == "tendrilSwipe";
    });
    if (!ported || (r.spell && r.casts.empty())) {
      ++aiSkipped;
      continue;
    }
    ember::Battle copy = *battle;
    ember::BattleUnit& u = copy.units()[static_cast<size_t>(r.unit)];
    if (r.at) u.x = r.at->x, u.y = r.at->y;
    if (r.gates.size() == 2) copy.warpGates().push_back({r.gates[0], r.gates[1], 3, 12});
    const std::optional<ember::AiCast> cast = ember::planEnemyCast(copy, static_cast<size_t>(r.unit));
    if (cast || !r.casts.empty()) {
      ++aiChecked;
      std::string ours, theirs;
      if (cast) {
        for (const ember::BattleHex& p : cast->path) ours += std::format(" ({},{})", p.x, p.y);
        for (const ember::BattleStep& s : cast->steps) {
          ours += " " + s.kind + " ids";
          for (size_t i : s.ids) ours += std::format(" {}", i);
          ours += " tiles";
          for (const ember::BattleHex& t : s.tiles) ours += std::format(" ({},{})", t.x, t.y);
        }
      }
      for (const ember::BattleHex& p : r.plan.path) theirs += std::format(" ({},{})", p.x, p.y);
      for (const ember::AiReference::Cast& c : r.casts) {
        theirs += " " + c.kind + " ids";
        for (int i : c.ids) theirs += std::format(" {}", i);
        theirs += " tiles";
        for (const ember::BattleHex& t : c.tiles) theirs += std::format(" ({},{})", t.x, t.y);
      }
      if (ours != theirs) report(std::format("ai cast {} at ({}, {}): ours{}; Ember{}", u.name, u.x, u.y, ours, theirs));
      continue;
    }
    const ember::AiPlan plan = copy.planEnemy(static_cast<size_t>(r.unit));
    ++aiChecked;
    if (plan.path != r.plan.path || plan.target != r.plan.target) {
      std::string ours, theirs;
      for (const ember::BattleHex& p : plan.path) ours += std::format(" ({},{})", p.x, p.y);
      for (const ember::BattleHex& p : r.plan.path) theirs += std::format(" ({},{})", p.x, p.y);
      report(std::format("ai {} at ({}, {}): ours walk{} strikes {}; Ember walk{} strikes {}", u.name, u.x, u.y, ours, plan.target, theirs,
                         r.plan.target));
    }
  }
  // Spells: every reference cast (the aim's verdict; then the board afterwards and the random numbers drawn).
  size_t castsChecked = 0, castsValid = 0;
  // every cast starts from the battle as it opens, without the (very large) reference tables the copies would drag along
  ember::Battle pristine = *battle;
  pristine.clearReferences();
  for (const ember::CastReference& r : battle->castReferences()) {
    ember::Battle copy = pristine;
    if (r.hurt) ember::hurtScene(copy);
    ember::BattleUnit& u = copy.units()[static_cast<size_t>(r.caster)];
    u.x = r.at.x, u.y = r.at.y;
    if (r.patchTier > 0) u.spells[r.patchTier - 1] = r.patchValue;  // the reference gave the caster uses first
    ++castsChecked;
    std::optional<std::vector<ember::BattleStep>> steps;
    bool valid = false;
    if (r.kind == "offHand") {
      // the off-hand action: the caster is given the reference's item first, then strikes the foe standing on the cell
      const nlohmann::json& h = r.offHand;
      u.combat.offHandWeapon = h.value("kind", std::string()) == "weapon", u.combat.offHandShield = h.value("kind", std::string()) == "shield";
      u.combat.offMinRange = h.value("minRange", 1), u.combat.offMaxRange = h.value("maxRange", 1);
      u.combat.offDice = h.value("dice", 1), u.combat.offFaces = h.value("faces", 4), u.combat.offBonus = h.value("bonus", 0);
      u.combat.offShieldMul = h.value("dmgMul", 0.75);
      u.offWeaponType = h.contains("weaponType") && h["weaponType"].is_string() ? h["weaponType"].get<std::string>() : std::string();
      u.offTrainsWeapon = h.value("trains", false), u.offWeaponSkill = h.value("skill", 0.0);
      if (h.contains("mastery") && h["mastery"].is_object())
        u.combat.offAccuracy = h["mastery"].value("accuracy", 100.0), u.combat.offDamageMul = h["mastery"].value("damage", 1.0);
      const int foe = copy.unitAt(r.cell.x, r.cell.y);
      if (foe >= 0)
        if (std::optional<ember::BattleStep> s = ember::offHandStep(copy, static_cast<size_t>(r.caster), static_cast<size_t>(foe))) valid = true, steps = std::vector{*s};
    } else if (r.kind == "strike") {
      // a plain attack on the hero standing on the cell (its counter included)
      ember::BattleStep s;
      s.type = ember::BattleStep::Type::Combat;
      s.att = static_cast<size_t>(r.caster);
      s.def = static_cast<size_t>(copy.unitAt(r.cell.x, r.cell.y));
      valid = true, steps = std::vector{s};
    } else if (!r.potion.empty()) {
      // a potion: the actor carries one (export_battle.ts), then usePotion + applyPotion
      u.bag[ember::potionIndex(r.potion)] = 1;
      if (std::optional<ember::BattleStep> s = ember::usePotion(copy, static_cast<size_t>(r.caster), r.potion, r.cell)) valid = true, steps = std::vector{*s};
    } else {
      valid = ember::spellAimValid(copy, static_cast<size_t>(r.caster), r.kind, r.cell);
      // Ember records a cast that queued nothing (Choque without a player tier) as not valid.
      if (valid) {
        std::string why;
        // the reference casts straight through the cast method (no class / uses gate), but Criar Comida e Água's start
        // method checks its uses itself
        const int count = ember::spellTargetCount(copy, static_cast<size_t>(r.caster), r.kind);
        steps = ember::castSpell(copy, static_cast<size_t>(r.caster), r.kind, std::vector<ember::BattleHex>(static_cast<size_t>(count), r.cell), &why,
                              r.kind == "createFoodAndWater");
      }
    }
    const bool cast = valid && steps.has_value();
    if (cast != r.valid) {
      report(std::format("cast {} {} from ({},{}) at ({},{}): ours {}, Ember {}", u.name, r.kind, r.at.x, r.at.y, r.cell.x, r.cell.y, cast ? "cast" : "no cast",
                         r.valid ? "cast" : "no cast"));
      continue;
    }
    if (!cast) continue;
    ++castsValid;
    ember::Mulberry32 rng(r.seed);
    int calls = 0;
    std::vector<ember::BattleEffect> effects;
    if (r.skill) ember::playSteps(copy, *steps, rng, effects);
    else
      for (const ember::BattleStep& step : *steps) ember::resolveStep(copy, step, rng, effects);
    // how many numbers were drawn: advance a fresh generator until it matches the used one
    {
      ember::Mulberry32 probe(r.seed), used = rng;
      const double next = used();
      while (calls < 100000 && probe() != next) ++calls;
    }
    std::string diff;
    if (r.skill) {
      static const char* const kNames[] = {"hp", "alive", "x", "y", "stunned", "stunTurns", "asleep", "sleepTurns", "bleeding", "bleedRounds", "crippled", "fear",
                                           "atk", "mag", "def", "dex", "mov", "poisoned", "poisonTier", "poisonMag", "side", "xp", "level", "maxHp", "shock",
                                           "blessRounds", "blessPct", "charges", "drainCharges", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9", "t10"};
      if (static_cast<int>(copy.units().size()) != r.unitCount) diff += std::format(" [units {} vs {}]", copy.units().size(), r.unitCount);
      for (size_t i = 0; i < copy.units().size() && i < r.ext.size(); ++i) {
        const std::vector<double> mine = ember::unitStateNumbers(copy.units()[i]);
        for (size_t k = 0; k < mine.size() && k < r.ext[i].size(); ++k)
          if (std::abs(mine[k] - r.ext[i][k]) > 1e-9) diff += std::format(" [{} {} {} vs {}]", copy.units()[i].name, k < std::size(kNames) ? kNames[k] : "?", mine[k], r.ext[i][k]);
      }
      if (ember::zoneStateNumbers(copy) != r.zones) diff += std::format(" [zones {} vs {}]", ember::zoneStateNumbers(copy).dump(), r.zones.dump());
      const nlohmann::json en = ember::enmityNumbers(copy);
      bool sameEnmity = en.size() == r.enmity.size();
      for (size_t i = 0; sameEnmity && i < en.size(); ++i)
        for (size_t k = 0; k < 4; ++k)
          if (std::abs(en[i][k].get<double>() - r.enmity[i][k].get<double>()) > 1e-6) sameEnmity = false;
      if (!sameEnmity) diff += std::format(" [enmity {} vs {}]", en.dump(), r.enmity.dump());
      {  // the party's affinity scores (friendly fire, healing)
        std::map<std::string, double> theirs;
        if (r.affinity.is_object())
          for (const auto& [k, v] : r.affinity.items()) theirs[k] = v.get<double>();
        bool same = theirs.size() == copy.affinityScores().size();
        for (const auto& [k, v] : theirs) {
          const auto it = copy.affinityScores().find(k);
          if (it == copy.affinityScores().end() || std::abs(it->second - v) > 1e-9) same = false;
        }
        if (!same) diff += " [affinity]";
      }
      if (calls != r.calls) diff += std::format(" [draws {} vs {}]", calls, r.calls);
      if (!diff.empty())
        report(std::format("cast {} {} from ({},{}) at ({},{}) seed {}:{}", battle->units()[static_cast<size_t>(r.caster)].name, r.kind, r.at.x, r.at.y, r.cell.x,
                           r.cell.y, r.seed, diff));
      continue;
    }
    for (size_t i = 0; i < copy.units().size() && i < r.after.size(); ++i) {
      const ember::BattleUnit& v = copy.units()[i];
      const ember::CastReference::After& t = r.after[i];
      const bool echoSame = v.shock.has_value() == t.shock.has_value() &&
                            (!v.shock || (v.shock->dice == t.shock->dice && v.shock->faces == t.shock->faces && v.shock->bonus == t.shock->bonus &&
                                          std::abs(v.shock->mag - t.shock->mag) < 1e-9));
      if (v.hp != t.hp || v.alive != t.alive || !echoSame || v.maxHp != t.maxHp || v.level != t.level || v.xp != t.xp)
        diff += std::format(" [{} hp {} alive {} lvl {} xp {} vs {} {} {} {}]", v.name, v.hp, v.alive, v.level, v.xp, t.hp, t.alive, t.level, t.xp);
      if (t.hasStatus) {
        const double full = v.fullness ? *v.fullness : 100;
        if (v.atk != t.atk || v.mag != t.mag || v.def != t.def || v.dex != t.dex || v.mov != t.mov || v.diseased != t.diseased ||
            v.poisoned != t.poisoned || std::abs(full - t.fullness) > 1e-9)
          diff += std::format(" [{} atk {} mag {} def {} dex {} mov {} sick {} poison {} full {} vs {} {} {} {} {} {} {} {}]", v.name, v.atk, v.mag, v.def,
                              v.dex, v.mov, v.diseased, v.poisoned, full, t.atk, t.mag, t.def, t.dex, t.mov, t.diseased, t.poisoned, t.fullness);
        for (int k = 0; k < 10; ++k)
          if (v.spells[k] != t.spells[k]) diff += std::format(" [{} tier{} {} vs {}]", v.name, k + 1, v.spells[k], t.spells[k]);
        for (int k = 0; k < 7; ++k)
          if (v.bag[k] != t.bag[k]) diff += std::format(" [{} {} {} vs {}]", v.name, ember::kPotionIds[k], v.bag[k], t.bag[k]);
      }
    }
    if (r.hurt && copy.lootRations() != r.lootRations) diff += std::format(" [rations {} vs {}]", copy.lootRations(), r.lootRations);
    for (int t = 0; t < 10; ++t)
      if (u.spells[t] != r.spells[t]) diff += std::format(" [tier{} {} vs {}]", t + 1, u.spells[t], r.spells[t]);
    if (u.frostCharges != r.frostCharges) diff += std::format(" [frost {} vs {}]", u.frostCharges, r.frostCharges);
    if (calls != r.calls) diff += std::format(" [draws {} vs {}]", calls, r.calls);
    if (!diff.empty())
      report(std::format("cast {} {} from ({},{}) at ({},{}) seed {}:{}", battle->units()[static_cast<size_t>(r.caster)].name, r.kind, r.at.x, r.at.y,
                         r.cell.x, r.cell.y, r.seed, diff));
  }
  // The start of a turn under each condition (beginUnitTurn).
  size_t turnsChecked = 0;
  for (const ember::TurnStartReference& r : battle->turnStartReferences()) {
    ember::Battle copy = pristine;
    const size_t ui = static_cast<size_t>(r.unit);
    ember::BattleUnit& u = copy.units()[ui];
    auto around = [&]() {
      std::vector<ember::BattleHex> cells{{u.x, u.y}};
      for (const ember::BattleHex& n : ember::Battle::neighbors(u.x, u.y)) cells.push_back(n);
      return cells;
    };
    if (r.scenario == "stunned" || r.scenario == "mixed") u.stunned = true, u.stunTurns = 2;
    if (r.scenario == "asleep") u.asleep = true, u.sleepTurns = 2;
    if (r.scenario == "web") copy.webZones().push_back({around(), 3, {u.x, u.y}, 1, 0.6});
    if (r.scenario == "fear") {
      u.fearTurns = 2;
      std::optional<size_t> foe;
      for (size_t i = 0; i < copy.units().size(); ++i) {
        const ember::BattleUnit& v = copy.units()[i];
        if (!v.alive || v.side == u.side || v.side == "neutral") continue;
        if (!foe || ember::Battle::distance({u.x, u.y}, {v.x, v.y}) < ember::Battle::distance({u.x, u.y}, {copy.units()[*foe].x, copy.units()[*foe].y})) foe = i;
      }
      if (foe) u.fearSourceId = copy.units()[*foe].id;
    }
    if (r.scenario == "poison" || r.scenario == "mixed") u.poisoned = true, u.poisonTier = "lethal", u.cond.poisonMag = 40;
    if (r.scenario == "ice" || r.scenario == "mixed") {
      std::string caster;
      for (const ember::BattleUnit& v : copy.units())
        if (v.side == "player") {
          caster = v.id;
          break;
        }
      copy.iceZones().push_back({around(), 2, {u.x, u.y}, 1, 2, 6, 0.8, 30, 12, caster, "player"});
    }
    if (r.scenario == "echo") u.shock = ember::BattleUnit::Echo{1, 12, 2, 30};
    if (r.scenario == "regen" || r.scenario == "regenNear") {
      u.combat.staff = true, u.combat.staffRegen = 2;
      u.hp = std::max(1, u.maxHp - (r.scenario == "regen" ? 5 : 1));
    }
    ember::Mulberry32 rng(r.seed);
    std::vector<ember::BattleEffect> effects;
    u.moveUsed = 0;
    copy.beginTurn(ui);
    const ember::TurnStart ts = ember::beginUnitTurn(copy, ui, rng, effects);
    int calls = 0;
    {
      ember::Mulberry32 probe(r.seed), used = rng;
      const double next = used();
      while (calls < 100000 && probe() != next) ++calls;
    }
    ++turnsChecked;
    std::string diff;
    if (static_cast<int>(copy.units().size()) != static_cast<int>(r.ext.size())) diff += " [units]";
    for (size_t i = 0; i < copy.units().size() && i < r.ext.size(); ++i) {
      const std::vector<double> mine = ember::unitStateNumbers(copy.units()[i]);
      for (size_t k = 0; k < mine.size() && k < r.ext[i].size(); ++k)
        if (std::abs(mine[k] - r.ext[i][k]) > 1e-9) diff += std::format(" [{} #{} {} vs {}]", copy.units()[i].name, k, mine[k], r.ext[i][k]);
    }
    if (ember::zoneStateNumbers(copy) != r.zones) diff += " [zones]";
    if (copy.units()[ui].moved != r.moved || copy.units()[ui].acted != r.acted || copy.turnRestrained() != r.restrained)
      diff += std::format(" [moved {} acted {} restrained {} vs {} {} {}]", copy.units()[ui].moved, copy.units()[ui].acted, copy.turnRestrained(), r.moved, r.acted, r.restrained);
    if (ts.fleePath != r.flee) diff += std::format(" [flee {} vs {} hexes]", ts.fleePath.size(), r.flee.size());
    if (calls != r.calls) diff += std::format(" [draws {} vs {}]", calls, r.calls);
    if (!diff.empty()) report(std::format("turn start {} {} seed {}:{}", u.name, r.scenario, r.seed, diff));
  }
  if (turnsChecked && !combatFailed) logInfo("battle {}: turn starts match Ember ({} units under 10 conditions)", battle->map(), turnsChecked);
  // Opening each lock with a Gazua ("Arrombar"): the cells it changes, the loot, the bags, the draws.
  size_t locksChecked = 0;
  for (const ember::LockReference& r : battle->lockReferences()) {
    ember::Battle copy = pristine;
    const size_t ci = static_cast<size_t>(r.caster);
    ember::BattleUnit& u = copy.units()[ci];
    u.x = r.at.x, u.y = r.at.y;
    u.lockpick = 1;
    if (r.valid) copy.setTurnOrder(r.turnOrder);
    ember::Mulberry32 rng(r.seed);
    std::vector<ember::BattleEffect> effects;
    const bool ok = ember::useLockpick(copy, ci, rng, effects);
    if (ok != r.valid) {
      report(std::format("lock {} opened by {} at ({},{}): ours {}, Ember {}", r.lock, u.name, r.at.x, r.at.y, ok ? "opened" : "stayed shut", r.valid ? "opened" : "stayed shut"));
      continue;
    }
    if (!ok) continue;
    ++locksChecked;
    int calls = 0;
    {
      ember::Mulberry32 probe(r.seed), used = rng;
      const double next = used();
      while (calls < 100000 && probe() != next) ++calls;
    }
    std::string diff;
    for (size_t i = 0; i < copy.units().size() && i < r.ext.size(); ++i) {
      const std::vector<double> mine = ember::unitStateNumbers(copy.units()[i]);
      for (size_t k = 0; k < mine.size() && k < r.ext[i].size(); ++k)
        if (std::abs(mine[k] - r.ext[i][k]) > 1e-9) diff += std::format(" [{} #{} {} vs {}]", copy.units()[i].name, k, mine[k], r.ext[i][k]);
    }
    for (size_t i = 0; i < copy.units().size() && i < r.bags.size(); ++i) {
      const ember::BattleUnit& v = copy.units()[i];
      const nlohmann::json& theirs = r.bags[i];
      if (theirs.value("lockpick", 0) != v.lockpick) diff += std::format(" [{} Gazuas {} vs {}]", v.name, v.lockpick, theirs.value("lockpick", 0));
      for (int k = 0; k < 7; ++k)
        if (theirs.value(ember::kPotionIds[k], 0) != v.bag[k]) diff += std::format(" [{} {} {} vs {}]", v.name, ember::kPotionIds[k], v.bag[k], theirs.value(ember::kPotionIds[k], 0));
    }
    auto list = [](const nlohmann::json& a) {
      std::vector<std::string> out;
      for (const nlohmann::json& s : a) out.push_back(s.get<std::string>());
      return out;
    };
    if (copy.lootEmber() != r.loot.value("ember", 0)) diff += std::format(" [ember {} vs {}]", copy.lootEmber(), r.loot.value("ember", 0));
    if (copy.lootRations() != r.loot.value("rations", 0)) diff += std::format(" [rations {} vs {}]", copy.lootRations(), r.loot.value("rations", 0));
    if (copy.lootWeapons() != list(r.loot["weapons"])) diff += " [weapons]";
    if (copy.lootEquipment() != list(r.loot["equipment"])) diff += " [gear]";
    {
      std::map<std::string, double> theirs;
      for (const auto& [k, v] : r.affinity.items()) theirs[k] = v.get<double>();
      if (theirs != copy.affinityScores()) diff += " [affinity]";
    }
    if (calls != r.calls) diff += std::format(" [draws {} vs {}]", calls, r.calls);
    if (!diff.empty()) report(std::format("lock {} opened by {} at ({},{}) seed {}:{}", r.lock, u.name, r.at.x, r.at.y, r.seed, diff));
  }
  if (locksChecked && !combatFailed) logInfo("battle {}: locks open as in Ember ({} openings: loot, bags, draws)", battle->map(), locksChecked);
  if (combatFailed) {
    for (const auto& [kind, n] : seenKinds) logError("battle {}: {} x {}", battle->map(), n, kind.substr(0, 120));
    logError("battle {}: {} combat answers differ from Ember", battle->map(), combatFailed);
    return false;
  }
  if (castsChecked) logInfo("battle {}: spells match Ember ({} aims, {} casts played)", battle->map(), castsChecked, castsValid);
  if (aiChecked) logInfo("battle {}: enemy decisions match Ember ({} checked, {} unported casts skipped)", battle->map(), aiChecked, aiSkipped);
  if (!battle->forecastReferences().empty())
    logInfo("battle {}: combat matches Ember ({} forecasts, {} seeded rolls)", battle->map(), battle->forecastReferences().size(),
            battle->rollReferences().size());
  return true;
}

}  // namespace ember
