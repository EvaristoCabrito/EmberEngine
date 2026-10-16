// A fight in progress, saved and resumed (Engine2 BattleEngine captureSnapshot / applySnapshot): the units' live
// state, the round, its initiative order and every enemy's enmity table, in Engine2's BattleSnapshot field names.

#include "world/Battle.h"

#include <algorithm>
#include <string>

namespace ember::e2 {

namespace {

const char* const kTiers[10] = {"tier1", "tier2", "tier3", "tier4", "tier5", "tier6", "tier7", "tier8", "tier9", "tier10"};

}  // namespace

nlohmann::json Battle::snapshot() const {
  nlohmann::json units = nlohmann::json::array();
  for (const BattleUnit& u : units_) {
    nlohmann::json spells = nlohmann::json::object();
    for (int t = 0; t < 10; ++t) spells[kTiers[t]] = u.spells[t];
    nlohmann::json skills = nlohmann::json::object();
    for (const auto& [id, value] : u.skills) skills[id] = value;
    nlohmann::json s = {
        {"id", u.id},       {"name", u.name},       {"classId", u.classId},     {"side", u.side},
        {"x", u.x},         {"y", u.y},             {"hp", u.hp},               {"maxHp", u.maxHp},
        {"atk", u.atk},     {"mag", u.mag},         {"def", u.def},             {"dex", u.dex},
        {"mov", u.mov},     {"minRange", u.minRange}, {"maxRange", u.maxRange}, {"moved", u.moved},
        {"acted", u.acted}, {"facing", u.facing},   {"alive", u.alive},         {"level", u.level},
        {"xp", u.xp},       {"spells", spells},     {"shockCharges", u.shockCharges},
        {"frostCharges", u.frostCharges},           {"fantomForceCharges", u.fantomForceCharges},
        {"initiative", u.initiative},               {"summoned", u.summoned},  {"guaranteedDrop", u.guaranteedDrop},
        {"moveBudgetUsed", u.moveUsed},             {"healingSkill", u.healingSkill},
        // EmberEngine's own: the trained weapon's value and every trained skill (Engine2 kept them in heroSkills)
        {"weaponSkill", u.weaponSkill},             {"skills", skills},
        // levelUpUnit's growth also moves these
        {"gear", {{"hp", u.gearHp}, {"atk", u.gearAtk}, {"mag", u.gearMag}, {"def", u.gearDef}, {"dex", u.gearDex}, {"mov", u.gearMov}}}};
    if (u.faceDx) s["faceDx"] = *u.faceDx;
    if (u.faceDy) s["faceDy"] = *u.faceDy;
    s["shock"] = u.shock ? nlohmann::json{{"dice", u.shock->dice}, {"faces", u.shock->faces}, {"bonus", u.shock->bonus}, {"mag", u.shock->mag}}
                         : nlohmann::json();
    units.push_back(std::move(s));
  }
  nlohmann::json order = nlohmann::json::array();
  for (size_t i : turnOrder_) order.push_back(units_[i].id);
  // enmity.ts EnmitySnapshot: { enemyId: { heroId: [ce, ve] } }
  nlohmann::json enmity = nlohmann::json::object();
  for (const Enmity& e : enmity_) enmity[units_[e.enemy].id][units_[e.hero].id] = nlohmann::json::array({e.ce, e.ve});
  return {{"turn", round_}, {"units", units}, {"turnOrder", order}, {"enmity", enmity}};
}

bool Battle::applySnapshot(const nlohmann::json& snap) {
  if (!snap.is_object() || !snap.contains("units") || !snap["units"].is_array()) return false;
  const nlohmann::json& units = snap["units"];
  auto find = [&](const std::string& id) -> int {
    for (size_t i = 0; i < units_.size(); ++i)
      if (units_[i].id == id) return static_cast<int>(i);
    return -1;
  };
  // Every saved unit must be one of this battle's (a fight saved on another version of the map is not resumed).
  for (const nlohmann::json& s : units)
    if (!s.is_object() || find(s.value("id", std::string())) < 0) return false;
  for (const nlohmann::json& s : units) {
    BattleUnit& u = units_[static_cast<size_t>(find(s.value("id", std::string())))];
    u.x = s.value("x", u.x), u.y = s.value("y", u.y);
    u.hp = s.value("hp", u.hp), u.maxHp = s.value("maxHp", u.maxHp);
    u.atk = s.value("atk", u.atk), u.mag = s.value("mag", u.mag), u.def = s.value("def", u.def), u.dex = s.value("dex", u.dex);
    u.mov = s.value("mov", u.mov), u.minRange = s.value("minRange", u.minRange), u.maxRange = s.value("maxRange", u.maxRange);
    u.moved = s.value("moved", u.moved), u.acted = s.value("acted", u.acted);
    u.facing = s.value("facing", u.facing) < 0 ? -1 : 1;
    u.alive = s.value("alive", u.alive);
    u.level = s.value("level", u.level), u.xp = s.value("xp", u.xp);
    if (s.contains("spells") && s["spells"].is_object())
      for (int t = 0; t < 10; ++t) u.spells[t] = s["spells"].value(kTiers[t], u.spells[t]);
    u.shockCharges = s.value("shockCharges", u.shockCharges);
    u.frostCharges = s.value("frostCharges", u.frostCharges);
    u.fantomForceCharges = s.value("fantomForceCharges", u.fantomForceCharges);
    u.initiative = s.value("initiative", u.initiative);
    u.guaranteedDrop = s.value("guaranteedDrop", u.guaranteedDrop);
    u.moveUsed = s.value("moveBudgetUsed", u.moveUsed);
    u.healingSkill = s.value("healingSkill", u.healingSkill);
    u.weaponSkill = s.value("weaponSkill", u.weaponSkill);
    if (s.contains("skills") && s["skills"].is_object()) {
      u.skills.clear();
      for (const auto& [id, value] : s["skills"].items())
        if (value.is_number()) u.skills.emplace_back(id, value.get<double>());
    }
    if (s.contains("gear") && s["gear"].is_object()) {
      const nlohmann::json& g = s["gear"];
      u.gearHp = g.value("hp", u.gearHp), u.gearAtk = g.value("atk", u.gearAtk), u.gearMag = g.value("mag", u.gearMag);
      u.gearDef = g.value("def", u.gearDef), u.gearDex = g.value("dex", u.gearDex), u.gearMov = g.value("mov", u.gearMov);
    }
    if (s.contains("faceDx") && s["faceDx"].is_number()) u.faceDx = s["faceDx"].get<double>();
    if (s.contains("faceDy") && s["faceDy"].is_number()) u.faceDy = s["faceDy"].get<double>();
    u.shock.reset();
    if (s.contains("shock") && s["shock"].is_object()) {
      const nlohmann::json& e = s["shock"];
      u.shock = BattleUnit::Echo{e.value("dice", 0), e.value("faces", 0), e.value("bonus", 0), e.value("mag", 0.0)};
    }
  }
  round_ = snap.value("turn", round_);
  if (snap.contains("turnOrder") && snap["turnOrder"].is_array()) {
    turnOrder_.clear();
    for (const nlohmann::json& id : snap["turnOrder"])
      if (id.is_string())
        if (const int i = find(id.get<std::string>()); i >= 0) turnOrder_.push_back(static_cast<size_t>(i));
  }
  enmity_.clear();
  if (snap.contains("enmity") && snap["enmity"].is_object())
    for (const auto& [enemy, table] : snap["enmity"].items()) {
      const int e = find(enemy);
      if (e < 0 || !table.is_object()) continue;
      for (const auto& [hero, pair] : table.items()) {
        const int h = find(hero);
        if (h < 0 || !pair.is_array() || pair.size() < 2) continue;
        enmity_.push_back({static_cast<size_t>(e), static_cast<size_t>(h), pair[0].get<double>(), pair[1].get<double>()});
      }
    }
  return true;
}

}  // namespace ember::e2
