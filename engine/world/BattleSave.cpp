// A fight in progress, saved and resumed (Engine2 BattleEngine captureSnapshot / applySnapshot): the units' live
// state, the round, its initiative order and every enemy's enmity table, in Engine2's BattleSnapshot field names.

#include "world/Battle.h"
#include "world/Spells.h"

#include <algorithm>
#include <cstdlib>
#include <format>
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
        {"gear", {{"hp", u.gearHp}, {"atk", u.gearAtk}, {"mag", u.gearMag}, {"def", u.gearDef}, {"dex", u.gearDex}, {"mov", u.gearMov}}},
        {"points", {{"hp", u.pointHp}, {"atk", u.pointAtk}, {"mag", u.pointMag}, {"def", u.pointDef}, {"dex", u.pointDex}}}};
    // What it carries and its conditions (Engine2 Unit.bag / diseased / diseaseBase / poisoned / poisonTier / fullness /
    // hungerPenaltyPct); the off-hand weapon's trained value (ours, as weaponSkill).
    nlohmann::json bag = nlohmann::json::object();
    for (int k = 0; k < 7; ++k) bag[kPotionIds[k]] = u.bag[k];
    s["bag"] = bag;
    s["lockpick"] = u.lockpick;
    s["diseased"] = u.diseased;
    s["diseaseBase"] = u.diseaseBase ? nlohmann::json{{"atk", u.diseaseBase->atk}, {"mag", u.diseaseBase->mag}, {"def", u.diseaseBase->def},
                                                       {"dex", u.diseaseBase->dex}, {"mov", u.diseaseBase->mov}}
                                     : nlohmann::json();
    s["poisoned"] = u.poisoned;
    s["poisonTier"] = u.poisonTier.empty() ? nlohmann::json() : nlohmann::json(u.poisonTier);
    s["fullness"] = u.fullness ? nlohmann::json(*u.fullness) : nlohmann::json();
    s["hungerPenaltyPct"] = u.hungerPenaltyPct;
    s["offWeaponSkill"] = u.offWeaponSkill;
    // Engine2's status conditions (BattleUnitSnap names)
    s["stunned"] = u.stunned, s["stunTurns"] = u.stunTurns, s["asleep"] = u.asleep, s["sleepTurns"] = u.sleepTurns;
    s["bleeding"] = u.bleeding, s["crippled"] = u.crippled, s["fearTurns"] = u.fearTurns;
    if (u.bleedRoundsLeft) s["bleedRoundsLeft"] = *u.bleedRoundsLeft;
    if (u.bleedRoundMarker) s["bleedRoundMarker"] = *u.bleedRoundMarker;
    if (!u.fearSourceId.empty()) s["fearSourceId"] = u.fearSourceId;
    s["blessedHitBonusPct"] = u.blessedHitBonusPct, s["blessedRoundsLeft"] = u.blessedRoundsLeft;
    if (!u.summonerId.empty()) s["summonerId"] = u.summonerId;
    s["spellCharges"] = u.spellCharges;
    if (u.lifeDrainCharges) s["lifeDrainCharges"] = *u.lifeDrainCharges;
    if (u.faceDx) s["faceDx"] = *u.faceDx;
    if (u.faceDy) s["faceDy"] = *u.faceDy;
    s["shock"] = u.shock ? nlohmann::json{{"dice", u.shock->dice}, {"faces", u.shock->faces}, {"bonus", u.shock->bonus}, {"mag", u.shock->mag}}
                         : nlohmann::json();
    // EmberEngine's own: a promotion's class and spell list, the conditions our spells leave
    s["className"] = u.className;
    s["spellList"] = u.spellList;
    if (u.cond.any() || u.cond.poisonMag > 0) {
      const BattleUnit::Conditions& c = u.cond;
      s["cond"] = {{"markPct", c.markPct},       {"markTurns", c.markTurns},     {"plagueSpread", c.plagueSpread}, {"plagueBurst", c.plagueBurst},
                   {"poisonMag", c.poisonMag},   {"bleedDice", c.bleedDice},     {"bleedFaces", c.bleedFaces},     {"bleedTurns", c.bleedTurns},
                   {"silenceTurns", c.silenceTurns}, {"decayTurns", c.decayTurns}, {"decayPct", c.decayPct},       {"decayCap", c.decayCap},
                   {"decayStep", c.decayStep},   {"decayDice", c.decayDice},     {"decayFaces", c.decayFaces},     {"decayMul", c.decayMul},
                   {"decayMag", c.decayMag}};
    }
    if (u.decayBase)
      s["decayBase"] = {{"atk", u.decayBase->atk}, {"mag", u.decayBase->mag}, {"def", u.decayBase->def}, {"dex", u.decayBase->dex}, {"mov", u.decayBase->mov}};
    units.push_back(std::move(s));
  }
  nlohmann::json zones = nlohmann::json::array();
  for (const Zone& z : zones_)
    zones.push_back({{"id", z.id},           {"kind", z.kind},             {"x", z.centre.x},         {"y", z.centre.y},
                     {"radius", z.radius},   {"rounds", z.rounds},         {"ring", z.ring},          {"caster", units_[z.caster].id},
                     {"element", z.element}, {"hazardDice", z.hazardDice}, {"hazardFaces", z.hazardFaces}, {"dice", z.dice},
                     {"faces", z.faces},     {"mul", z.mul},               {"mag", z.mag},            {"statPct", z.statPct},
                     {"centreMul", z.centreMul}, {"ringMul", z.ringMul},   {"centreDice", z.centreDice}, {"centreFaces", z.centreFaces},
                     {"ringDice", z.ringDice}, {"ringFaces", z.ringFaces}});
  // Engine2's zones, in captureSnapshot's shapes
  auto cellsJson = [](const std::vector<BattleHex>& cells) {
    nlohmann::json a = nlohmann::json::array();
    for (const BattleHex& c : cells) a.push_back(std::format("{},{}", c.x, c.y));
    return a;
  };
  nlohmann::json webs = nlohmann::json::array(), ices = nlohmann::json::array(), auras = nlohmann::json::array(), gates = nlohmann::json::array();
  for (const WebZone& z : webZones_)
    webs.push_back({{"cells", cellsJson(z.cells)}, {"roundsLeft", z.roundsLeft}, {"center", {{"x", z.center.x}, {"y", z.center.y}}}, {"radius", z.radius},
                    {"sleepChance", z.sleepChance}});
  for (const IceZone& z : iceZones_)
    ices.push_back({{"cells", cellsJson(z.cells)}, {"roundsLeft", z.roundsLeft}, {"center", {{"x", z.center.x}, {"y", z.center.y}}}, {"radius", z.radius},
                    {"damageDice", z.damageDice}, {"damageFaces", z.damageFaces}, {"damageMul", z.damageMul}, {"casterMag", z.casterMag},
                    {"casterLevel", z.casterLevel}, {"casterId", z.casterId}, {"side", z.side}});
  for (const AuraZone& z : auraZones_)
    auras.push_back({{"cells", cellsJson(z.cells)}, {"roundsLeft", z.roundsLeft}, {"kind", z.kind}, {"side", z.side}, {"pct", z.pct}});
  for (const WarpGate& g : warpGates_)
    gates.push_back({{"a", {{"x", g.a.x}, {"y", g.a.y}}}, {"b", {{"x", g.b.x}, {"y", g.b.y}}}, {"roundsLeft", g.roundsLeft}, {"level", g.level}});
  nlohmann::json order = nlohmann::json::array();
  for (size_t i : turnOrder_) order.push_back(units_[i].id);
  // enmity.ts EnmitySnapshot: { enemyId: { heroId: [ce, ve] } }
  nlohmann::json enmity = nlohmann::json::object();
  for (const Enmity& e : enmity_) enmity[units_[e.enemy].id][units_[e.hero].id] = nlohmann::json::array({e.ce, e.ve});
  return {{"turn", round_}, {"units", units}, {"turnOrder", order}, {"enmity", enmity}, {"lootRations", lootRations_}, {"roaming", roaming_},
          {"zones", zones}, {"zoneSerial", zoneSerial_}, {"log", log_}, {"webZones", webs}, {"iceStormZones", ices}, {"auraZones", auras},
          {"warpGates", gates}, {"turnRestrained", turnRestrained_}};
}

bool Battle::applySnapshot(const nlohmann::json& snap) {
  if (!snap.is_object() || !snap.contains("units") || !snap["units"].is_array()) return false;
  const nlohmann::json& units = snap["units"];
  auto find = [&](const std::string& id) -> int {
    for (size_t i = 0; i < units_.size(); ++i)
      if (units_[i].id == id) return static_cast<int>(i);
    return -1;
  };
  // Every saved unit must be one of this battle's (a fight saved on another version of the map is not resumed), except the
  // familiars summoned during it: they are made again from their class (their numbers come from the snapshot below).
  std::vector<BattleUnit> summoned;
  for (const nlohmann::json& s : units) {
    if (!s.is_object()) return false;
    const std::string id = s.value("id", std::string());
    if (find(id) >= 0) continue;
    std::optional<BattleUnit> skeleton = s.value("summoned", false) ? summonSkeleton(id, s.value("classId", std::string())) : std::nullopt;
    if (!skeleton) return false;
    skeleton->name = s.value("name", id);
    summoned.push_back(std::move(*skeleton));
  }
  for (BattleUnit& f : summoned) units_.push_back(std::move(f));
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
    if (s.contains("points") && s["points"].is_object()) {
      const nlohmann::json& p = s["points"];
      u.pointHp = p.value("hp", 0), u.pointAtk = p.value("atk", 0), u.pointMag = p.value("mag", 0), u.pointDef = p.value("def", 0), u.pointDex = p.value("dex", 0);
    }
    if (s.contains("gear") && s["gear"].is_object()) {
      const nlohmann::json& g = s["gear"];
      u.gearHp = g.value("hp", u.gearHp), u.gearAtk = g.value("atk", u.gearAtk), u.gearMag = g.value("mag", u.gearMag);
      u.gearDef = g.value("def", u.gearDef), u.gearDex = g.value("dex", u.gearDex), u.gearMov = g.value("mov", u.gearMov);
    }
    u.lockpick = s.value("lockpick", u.lockpick);
    if (s.contains("bag") && s["bag"].is_object())
      for (int k = 0; k < 7; ++k) u.bag[k] = s["bag"].value(kPotionIds[k], u.bag[k]);
    u.diseased = s.value("diseased", u.diseased);
    if (s.contains("diseaseBase")) {
      const nlohmann::json& d = s["diseaseBase"];
      if (d.is_object()) u.diseaseBase = BattleUnit::Stats{d.value("atk", 0), d.value("mag", 0), d.value("def", 0), d.value("dex", 0), d.value("mov", 0)};
      else u.diseaseBase.reset();
    }
    u.poisoned = s.value("poisoned", u.poisoned);
    if (s.contains("poisonTier")) u.poisonTier = s["poisonTier"].is_string() ? s["poisonTier"].get<std::string>() : std::string();
    if (s.contains("fullness")) {
      if (s["fullness"].is_number()) u.fullness = s["fullness"].get<double>();
      else u.fullness.reset();
    }
    u.hungerPenaltyPct = s.value("hungerPenaltyPct", u.hungerPenaltyPct);
    u.offWeaponSkill = s.value("offWeaponSkill", u.offWeaponSkill);
    u.stunned = s.value("stunned", false), u.stunTurns = s.value("stunTurns", 0), u.asleep = s.value("asleep", false), u.sleepTurns = s.value("sleepTurns", 0);
    u.bleeding = s.value("bleeding", false), u.crippled = s.value("crippled", false), u.fearTurns = s.value("fearTurns", 0);
    u.bleedRoundsLeft.reset(), u.bleedRoundMarker.reset();
    if (s.contains("bleedRoundsLeft") && s["bleedRoundsLeft"].is_number()) u.bleedRoundsLeft = s["bleedRoundsLeft"].get<int>();
    if (s.contains("bleedRoundMarker") && s["bleedRoundMarker"].is_number()) u.bleedRoundMarker = s["bleedRoundMarker"].get<int>();
    u.fearSourceId = s.value("fearSourceId", std::string());
    u.blessedHitBonusPct = s.value("blessedHitBonusPct", 0.0), u.blessedRoundsLeft = s.value("blessedRoundsLeft", 0);
    u.summonerId = s.value("summonerId", std::string());
    u.spellCharges = s.value("spellCharges", 0);
    u.lifeDrainCharges.reset();
    if (s.contains("lifeDrainCharges") && s["lifeDrainCharges"].is_number()) u.lifeDrainCharges = s["lifeDrainCharges"].get<int>();
    if (s.contains("faceDx") && s["faceDx"].is_number()) u.faceDx = s["faceDx"].get<double>();
    if (s.contains("faceDy") && s["faceDy"].is_number()) u.faceDy = s["faceDy"].get<double>();
    u.shock.reset();
    if (s.contains("shock") && s["shock"].is_object()) {
      const nlohmann::json& e = s["shock"];
      u.shock = BattleUnit::Echo{e.value("dice", 0), e.value("faces", 0), e.value("bonus", 0), e.value("mag", 0.0)};
    }
    // EmberEngine's own
    if (s.contains("classId") && s["classId"].is_string()) u.classId = s["classId"].get<std::string>();
    if (s.contains("className") && s["className"].is_string()) u.className = s["className"].get<std::string>();
    if (s.contains("spellList") && s["spellList"].is_array()) {
      u.spellList.clear();
      for (const nlohmann::json& k : s["spellList"])
        if (k.is_string()) u.spellList.push_back(k.get<std::string>());
    }
    u.cond = BattleUnit::Conditions{};
    if (s.contains("cond") && s["cond"].is_object()) {
      const nlohmann::json& c = s["cond"];
      BattleUnit::Conditions& o = u.cond;
      o.markPct = c.value("markPct", 0), o.markTurns = c.value("markTurns", 0), o.plagueSpread = c.value("plagueSpread", 0);
      o.plagueBurst = c.value("plagueBurst", false), o.poisonMag = c.value("poisonMag", 0.0);
      o.bleedDice = c.value("bleedDice", 0), o.bleedFaces = c.value("bleedFaces", 0), o.bleedTurns = c.value("bleedTurns", 0);
      o.silenceTurns = c.value("silenceTurns", 0), o.decayTurns = c.value("decayTurns", 0), o.decayPct = c.value("decayPct", 0);
      o.decayCap = c.value("decayCap", 0), o.decayStep = c.value("decayStep", 0), o.decayDice = c.value("decayDice", 0);
      o.decayFaces = c.value("decayFaces", 0), o.decayMul = c.value("decayMul", 0.0), o.decayMag = c.value("decayMag", 0.0);
    }
    u.decayBase.reset();
    if (s.contains("decayBase") && s["decayBase"].is_object()) {
      const nlohmann::json& d = s["decayBase"];
      u.decayBase = BattleUnit::Stats{d.value("atk", 0), d.value("mag", 0), d.value("def", 0), d.value("dex", 0), d.value("mov", 0)};
    }
  }
  zones_.clear();
  zoneSerial_ = snap.value("zoneSerial", 0);
  if (snap.contains("zones") && snap["zones"].is_array())
    for (const nlohmann::json& j : snap["zones"]) {
      if (!j.is_object()) continue;
      const int caster = find(j.value("caster", std::string()));
      if (caster < 0) continue;
      Zone z;
      z.id = j.value("id", 0), z.kind = j.value("kind", std::string()), z.centre = {j.value("x", 0), j.value("y", 0)};
      z.radius = j.value("radius", 1), z.rounds = j.value("rounds", 1), z.ring = j.value("ring", false), z.caster = static_cast<size_t>(caster);
      z.element = j.value("element", std::string()), z.hazardDice = j.value("hazardDice", 0), z.hazardFaces = j.value("hazardFaces", 8);
      z.dice = j.value("dice", 0), z.faces = j.value("faces", 0), z.mul = j.value("mul", 0.0), z.mag = j.value("mag", 0.0), z.statPct = j.value("statPct", 0);
      z.centreMul = j.value("centreMul", 0.0), z.ringMul = j.value("ringMul", 0.0), z.centreDice = j.value("centreDice", 0);
      z.centreFaces = j.value("centreFaces", 0), z.ringDice = j.value("ringDice", 0), z.ringFaces = j.value("ringFaces", 0);
      zoneSerial_ = std::max(zoneSerial_, z.id);
      zones_.push_back(std::move(z));
    }
  // Engine2's zones
  auto readCells = [](const nlohmann::json& a) {
    std::vector<BattleHex> cells;
    if (a.is_array())
      for (const nlohmann::json& k : a)
        if (k.is_string()) {
          const std::string t = k.get<std::string>();
          const size_t comma = t.find(',');
          if (comma != std::string::npos) cells.push_back({std::atoi(t.c_str()), std::atoi(t.c_str() + comma + 1)});
        }
    return cells;
  };
  auto readPoint = [](const nlohmann::json& j, const char* key) {
    return j.contains(key) && j[key].is_object() ? BattleHex{j[key].value("x", 0), j[key].value("y", 0)} : BattleHex{};
  };
  webZones_.clear(), iceZones_.clear(), auraZones_.clear(), warpGates_.clear();
  if (snap.contains("webZones") && snap["webZones"].is_array())
    for (const nlohmann::json& j : snap["webZones"])
      if (j.is_object())
        webZones_.push_back({readCells(j.contains("cells") ? j["cells"] : nlohmann::json()), j.value("roundsLeft", 0), readPoint(j, "center"), j.value("radius", 0),
                             j.value("sleepChance", 0.25)});
  if (snap.contains("iceStormZones") && snap["iceStormZones"].is_array())
    for (const nlohmann::json& j : snap["iceStormZones"])
      if (j.is_object())
        iceZones_.push_back({readCells(j.contains("cells") ? j["cells"] : nlohmann::json()), j.value("roundsLeft", 0), readPoint(j, "center"), j.value("radius", 0),
                             j.value("damageDice", 1), j.value("damageFaces", 6), j.value("damageMul", 0.65), j.value("casterMag", 0.0),
                             j.value("casterLevel", 1), j.value("casterId", std::string()), j.value("side", std::string())});
  if (snap.contains("auraZones") && snap["auraZones"].is_array())
    for (const nlohmann::json& j : snap["auraZones"])
      if (j.is_object())
        auraZones_.push_back({readCells(j.contains("cells") ? j["cells"] : nlohmann::json()), j.value("roundsLeft", 0), j.value("kind", std::string()),
                              j.value("side", std::string()), j.value("pct", 0.0)});
  if (snap.contains("warpGates") && snap["warpGates"].is_array())
    for (const nlohmann::json& j : snap["warpGates"])
      if (j.is_object()) warpGates_.push_back({readPoint(j, "a"), readPoint(j, "b"), j.value("roundsLeft", 0), j.value("level", 1)});
  turnRestrained_ = snap.value("turnRestrained", false);
  round_ = snap.value("turn", round_);
  lootRations_ = snap.value("lootRations", lootRations_);
  log_.clear();
  if (snap.contains("log") && snap["log"].is_array())
    for (const nlohmann::json& line : snap["log"])
      if (line.is_string()) log_.push_back(line.get<std::string>());
  roaming_ = roaming_ && snap.value("roaming", true);
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
