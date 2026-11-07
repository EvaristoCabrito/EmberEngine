// Action Points (PA): the one resolver of maxima, costs and spends (world/ActionPoints.h). The rules come from
// content/data/action-points.json; the design's own numbers are the defaults here when the file is missing.

#include "world/ActionPoints.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

#include "core/Log.h"
#include "world/Combat.h"
#include "world/Spells.h"

namespace ember {

namespace {

using Json = nlohmann::json;

ApRules gRules;
bool gRulesRead = false;

int roundBy(double v, const std::string& mode) {
  if (mode == "ceil") return static_cast<int>(std::ceil(v - 1e-9));
  if (mode == "round") return static_cast<int>(std::floor(v + 0.5));
  return static_cast<int>(std::floor(v + 1e-9));
}

// The design's defaults: the file's values when it is missing.
ApRules defaults() {
  ApRules r;
  r.armor.categories = {{"unarmored", 5}, {"cloth", 4}, {"lightLeather", 3}, {"reinforcedLeather", 2}, {"chainmail", 1}, {"heavyPlate", 0}};
  r.weapons.categories = {{"dagger", "dagger"}, {"sword", "sword"},   {"axe", "axe"},         {"mace", "blunt"},    {"hammer", "blunt"},
                          {"club", "blunt"},    {"staff", "staff"},   {"spear", "polearm"},   {"lance", "polearm"}, {"pike", "polearm"},
                          {"halberd", "polearm"}, {"polearm", "polearm"}, {"bow", "bow"},     {"crossbow", "bow"}};
  r.weapons.attackCost = {{"dagger", 5}, {"sword", 6}, {"axe", 6}, {"blunt", 6}, {"polearm", 7}, {"bow", 7}, {"staff", 6}, {"unarmed", 6}};
  r.counters.byType = {{"staff", 6}, {"spell", 7}};
  r.examine.categories = {{"easy", "Alvo fácil", "Easy Target", 0.5},
                          {"moderate", "Desafio moderado", "Moderate Challenge", 1.0},
                          {"difficult", "Oponente difícil", "Difficult Opponent", 2.0},
                          {"deadly", "Ameaça mortal", "Deadly Threat", std::nullopt}};
  return r;
}

ApRules::Bonus bonusOf(const Json& j) {
  ApRules::Bonus b;
  if (!j.is_object()) return b;
  b.flat = j.value("flat", 0), b.pct = j.value("pct", 0.0);
  b.movementFlat = j.value("movementFlat", 0), b.movementPct = j.value("movementPct", 0.0);
  return b;
}

ApRules readRules() {
  ApRules r = defaults();
  std::ifstream in("content/data/action-points.json", std::ios::binary);
  if (!in) {
    logError("action points: cannot read content/data/action-points.json (the design's defaults are in force)");
    return r;
  }
  Json j = Json::parse(in, nullptr, false);
  if (j.is_discarded() || !j.is_object()) {
    logError("action points: content/data/action-points.json is not valid JSON (the design's defaults are in force)");
    return r;
  }
  r.loaded = true;
  r.baseAp = j.value("baseAp", r.baseAp), r.dexPerAp = std::max(1, j.value("dexPerAp", r.dexPerAp));
  r.maxAp = std::max(1, j.value("maxAp", r.maxAp));
  r.rounding = j.value("rounding", r.rounding);
  if (const Json& a = j.value("armor", Json::object()); a.is_object()) {
    r.armor.slot = a.value("slot", r.armor.slot), r.armor.defaultCategory = a.value("default", r.armor.defaultCategory);
    if (a.contains("categories") && a["categories"].is_object()) {
      r.armor.categories.clear();
      for (const auto& [k, v] : a["categories"].items())
        if (v.is_number()) r.armor.categories.emplace_back(k, v.get<int>());
    }
    for (const char* key : {"items", "classes"})
      if (a.contains(key) && a[key].is_object())
        for (const auto& [k, v] : a[key].items())
          if (v.is_string()) (std::string(key) == "items" ? r.armor.items : r.armor.classes)[k] = v.get<std::string>();
    if (a.contains("names") && a["names"].is_object())
      for (const auto& [k, v] : a["names"].items())
        if (v.is_object()) r.armor.names[k] = {v.value("pt", k), v.value("en", k)};
  }
  if (const Json& w = j.value("weapons", Json::object()); w.is_object()) {
    for (const char* key : {"categories", "classWeapons"})
      if (w.contains(key) && w[key].is_object()) {
        std::map<std::string, std::string>& into = std::string(key) == "categories" ? r.weapons.categories : r.weapons.classWeapons;
        into.clear();
        for (const auto& [k, v] : w[key].items())
          if (v.is_string()) into[k] = v.get<std::string>();
      }
    if (w.contains("attackCost") && w["attackCost"].is_object()) {
      r.weapons.attackCost.clear();
      for (const auto& [k, v] : w["attackCost"].items())
        if (v.is_number()) r.weapons.attackCost[k] = v.get<int>();
    }
    r.weapons.defaultCategory = w.value("default", r.weapons.defaultCategory);
  }
  if (const Json& a = j.value("actions", Json::object()); a.is_object()) r.potion = a.value("potion", r.potion), r.lockpick = a.value("lockpick", r.lockpick);
  if (const Json& s = j.value("spells", Json::object()); s.is_object()) {
    r.spells.defaultCost = s.value("default", r.spells.defaultCost);
    r.spells.weaponSkillsUseWeaponCost = s.value("weaponSkillsUseWeaponCost", r.spells.weaponSkillsUseWeaponCost);
    if (s.contains("byTier") && s["byTier"].is_object())
      for (const auto& [k, v] : s["byTier"].items())
        if (v.is_number()) r.spells.byTier[std::atoi(k.c_str())] = v.get<int>();
    if (s.contains("byKind") && s["byKind"].is_object())
      for (const auto& [k, v] : s["byKind"].items())
        if (v.is_number()) r.spells.byKind[k] = v.get<int>();
  }
  if (const Json& c = j.value("counters", Json::object()); c.is_object()) {
    r.counters.byType.clear();
    for (const auto& [k, v] : c.items())
      if (v.is_number()) r.counters.byType[k] = v.get<int>();
    if (c.contains("byCategory") && c["byCategory"].is_object())
      for (const auto& [k, v] : c["byCategory"].items())
        if (v.is_number()) r.counters.byType[k] = v.get<int>();
  }
  if (const Json& m = j.value("movement", Json::object()); m.is_object()) {
    r.movement.minimum = m.value("minimum", r.movement.minimum), r.movement.rounding = m.value("rounding", r.movement.rounding);
    r.movement.cappedByMov = m.value("cappedByMov", r.movement.cappedByMov), r.movement.freeWhileRoaming = m.value("freeWhileRoaming", r.movement.freeWhileRoaming);
    if (m.contains("terrain") && m["terrain"].is_object())
      for (const auto& [k, v] : m["terrain"].items()) {
        if (!v.is_object()) continue;
        ApRules::Movement::Terrain t;
        if (v.contains("apCost") && v["apCost"].is_number()) t.apCost = v["apCost"].get<int>();
        t.modifier = v.value("modifier", 0), t.category = v.value("category", std::string());
        r.movement.terrain[k] = t;
      }
    if (m.contains("zones") && m["zones"].is_object())
      for (const auto& [k, v] : m["zones"].items())
        if (v.is_number()) r.movement.zones[k] = v.get<int>();
  }
  if (const Json& f = j.value("refresh", Json::object()); f.is_object())
    r.refresh.policy = f.value("policy", r.refresh.policy), r.refresh.accumulateCap = f.value("accumulateCap", r.refresh.accumulateCap);
  for (const char* key : {"status", "talents", "equipment"})
    if (j.contains(key) && j[key].is_object()) {
      std::map<std::string, ApRules::Bonus>& into = std::string(key) == "status" ? r.status : std::string(key) == "talents" ? r.talents : r.equipment;
      for (const auto& [k, v] : j[key].items()) into[k] = bonusOf(v);
    }
  if (const Json& e = j.value("examine", Json::object()); e.is_object()) {
    r.examine.cost = e.value("cost", r.examine.cost), r.examine.spendsAction = e.value("spendsAction", r.examine.spendsAction);
    if (e.contains("categories") && e["categories"].is_array()) {
      r.examine.categories.clear();
      for (const Json& c : e["categories"]) {
        if (!c.is_object()) continue;
        ApRules::Examine::Category cat;
        cat.id = c.value("id", std::string()), cat.pt = c.value("pt", cat.id), cat.en = c.value("en", cat.id);
        if (c.contains("maxRatio") && c["maxRatio"].is_number()) cat.maxRatio = c["maxRatio"].get<double>();
        r.examine.categories.push_back(cat);
      }
    }
  }
  if (const Json& a = j.value("ai", Json::object()); a.is_object()) r.ai.counterReserveWeight = a.value("counterReserveWeight", r.ai.counterReserveWeight);
  return r;
}

// The unit's conditions as the rules' status keys name them.
std::vector<std::string> statusKeys(const BattleUnit& u) {
  std::vector<std::string> keys;
  if (u.stunned) keys.push_back("stunned");
  if (u.asleep) keys.push_back("asleep");
  if (u.bleeding || u.cond.bleedTurns > 0) keys.push_back("bleeding");
  if (u.crippled) keys.push_back("crippled");
  if (u.fearTurns > 0) keys.push_back("fear");
  if (u.poisoned) keys.push_back("poisoned");
  if (u.diseased) keys.push_back("diseased");
  if (u.hungerPenaltyPct > 0) keys.push_back("hungry");
  if (u.cond.hobblePct > 0) keys.push_back("hobble");
  if (u.cond.sunderPct > 0) keys.push_back("sunder");
  if (u.cond.rallyPct > 0) keys.push_back("rally");
  if (u.cond.disarmPct > 0) keys.push_back("disarm");
  if (u.cond.staggered) keys.push_back("staggered");
  if (u.cond.silenceTurns > 0) keys.push_back("silence");
  if (u.cond.markTurns > 0) keys.push_back("mark");
  if (u.cond.decayTurns > 0) keys.push_back("decay");
  return keys;
}

// Whether a modifier of scope Action covers the action (its filter against the action's names).
bool covers(const ApModifier& m, const ApAction& a, const std::string& weaponCategory) {
  if (m.scope != ApModifier::Scope::Action) return false;
  if (m.filter.empty()) return true;
  switch (a.kind) {
    case ApAction::Kind::Attack: return m.filter == "attack" || m.filter == weaponCategory;
    case ApAction::Kind::OffHand: return m.filter == "offhand" || m.filter == "attack" || m.filter == weaponCategory;
    case ApAction::Kind::Spell: return m.filter == "spell" || m.filter == a.spell;
    case ApAction::Kind::Potion: return m.filter == "potion";
    case ApAction::Kind::Lockpick: return m.filter == "lockpick";
    case ApAction::Kind::Examine: return m.filter == "examine";
    case ApAction::Kind::Counter: return m.filter == "counter" || m.filter == "counter:" + a.counterType || m.filter == a.counterType;
  }
  return false;
}

int finish(double v, double pct, const std::string& rounding, int floor) {
  if (pct != 0) v = v * (1.0 + pct / 100.0);
  return std::max(floor, roundBy(v, rounding));
}

}  // namespace

const char* apSourceName(ApModifierSource s) {
  switch (s) {
    case ApModifierSource::Attribute: return "attribute";
    case ApModifierSource::Armor: return "armor";
    case ApModifierSource::Equipment: return "equipment";
    case ApModifierSource::Talent: return "talent";
    case ApModifierSource::PassiveAbility: return "passiveAbility";
    case ApModifierSource::ActiveAbility: return "activeAbility";
    case ApModifierSource::Buff: return "buff";
    case ApModifierSource::Debuff: return "debuff";
    case ApModifierSource::StatusEffect: return "statusEffect";
    case ApModifierSource::Terrain: return "terrain";
    case ApModifierSource::EnvironmentalEffect: return "environmentalEffect";
    case ApModifierSource::Other: break;
  }
  return "other";
}

ApModifierSource apSourceFromName(const std::string& name) {
  for (int i = 0; i <= static_cast<int>(ApModifierSource::Other); ++i)
    if (name == apSourceName(static_cast<ApModifierSource>(i))) return static_cast<ApModifierSource>(i);
  return ApModifierSource::Other;
}

nlohmann::json ApModifier::toJson() const {
  return {{"source", apSourceName(source)}, {"scope", scope == Scope::Max ? "max" : scope == Scope::Action ? "action" : "movement"},
          {"id", id}, {"filter", filter}, {"flat", flat}, {"pct", pct}, {"turns", turns}};
}

std::optional<ApModifier> ApModifier::fromJson(const nlohmann::json& j) {
  if (!j.is_object()) return std::nullopt;
  ApModifier m;
  m.source = apSourceFromName(j.value("source", std::string("other")));
  const std::string scope = j.value("scope", std::string("max"));
  m.scope = scope == "action" ? Scope::Action : scope == "movement" ? Scope::Movement : Scope::Max;
  m.id = j.value("id", std::string()), m.filter = j.value("filter", std::string());
  m.flat = j.value("flat", 0), m.pct = j.value("pct", 0.0), m.turns = j.value("turns", -1);
  return m;
}

const ApRules& apRules() {
  if (!gRulesRead) gRules = readRules(), gRulesRead = true;
  return gRules;
}

void reloadApRules() { gRules = readRules(), gRulesRead = true; }

int ApRules::armorBonus(const std::string& category) const {
  const std::string& c = category.empty() ? armor.defaultCategory : category;
  for (const auto& [id, bonus] : armor.categories)
    if (id == c) return bonus;
  for (const auto& [id, bonus] : armor.categories)
    if (id == armor.defaultCategory) return bonus;
  return 0;
}

std::string ApRules::weaponCategory(const std::string& weaponType) const {
  const auto it = weapons.categories.find(weaponType);
  return it == weapons.categories.end() ? weaponType : it->second;
}

int ApRules::attackCost(const std::string& category) const {
  if (const auto it = weapons.attackCost.find(category); it != weapons.attackCost.end()) return it->second;
  if (const auto it = weapons.attackCost.find("unarmed"); it != weapons.attackCost.end()) return it->second;
  return 6;
}

// --- the resolver -----------------------------------------------------------------------------------------------------

int apArmorBonus(const BattleUnit& u) {
  const ApRules& r = apRules();
  std::string category = u.armorCategory;
  if (category.empty())
    if (const auto it = r.armor.classes.find(u.classId); it != r.armor.classes.end()) category = it->second;
  return r.armorBonus(category);
}

int apMax(const Battle&, const BattleUnit& u) {
  const ApRules& r = apRules();
  double v = r.baseAp + std::max(0, u.dex) / r.dexPerAp + apArmorBonus(u);
  double pct = 0;
  for (const std::string& item : u.wornItems)
    if (const auto it = r.equipment.find(item); it != r.equipment.end()) v += it->second.flat, pct += it->second.pct;
  for (const std::string& kind : u.spellList)
    if (const auto it = r.talents.find(kind); it != r.talents.end()) v += it->second.flat, pct += it->second.pct;
  for (const std::string& key : statusKeys(u))
    if (const auto it = r.status.find(key); it != r.status.end()) v += it->second.flat, pct += it->second.pct;
  for (const ApModifier& m : u.apMods)
    if (m.scope == ApModifier::Scope::Max) v += m.flat, pct += m.pct;
  return std::min(r.maxAp, finish(v, pct, r.rounding, 0));  // never over the scale's ceiling
}

std::string apWeaponCategory(const BattleUnit& u) {
  const ApRules& r = apRules();
  if (!u.weaponType.empty()) return r.weaponCategory(u.weaponType);
  if (const auto it = r.weapons.classWeapons.find(u.classId); it != r.weapons.classWeapons.end()) return it->second;
  if (u.combat.arrow) return r.weaponCategory("bow");
  return r.weapons.defaultCategory;
}

std::string apOffHandCategory(const BattleUnit& u) {
  if (!u.combat.offHandWeapon) return {};
  return apRules().weaponCategory(u.offWeaponType.empty() ? std::string("dagger") : u.offWeaponType);
}

int apCost(const Battle& b, const BattleUnit& u, const ApAction& a) {
  const ApRules& r = apRules();
  const std::string category = apWeaponCategory(u);
  double v = 0;
  switch (a.kind) {
    case ApAction::Kind::Attack: v = r.attackCost(category); break;
    case ApAction::Kind::OffHand: {
      const std::string off = apOffHandCategory(u);
      v = off.empty() ? r.attackCost(category) : r.attackCost(off);  // a shield's bash prices as the main weapon's blow
      break;
    }
    case ApAction::Kind::Spell: {
      if (const auto it = r.spells.byKind.find(a.spell); it != r.spells.byKind.end()) v = it->second;
      else if (r.spells.weaponSkillsUseWeaponCost && spellWeaponAbility(a.spell)) v = r.attackCost(category);
      else if (const auto t = r.spells.byTier.find(spellTierNumber(a.spell)); t != r.spells.byTier.end()) v = t->second;
      else v = r.spells.defaultCost;
      break;
    }
    case ApAction::Kind::Potion: v = r.potion; break;
    case ApAction::Kind::Lockpick: v = r.lockpick; break;
    case ApAction::Kind::Examine: v = r.examine.cost; break;
    case ApAction::Kind::Counter: {
      if (const auto it = r.counters.byType.find(a.counterType); it != r.counters.byType.end()) v = it->second;
      else v = r.attackCost(a.counterType.empty() ? category : a.counterType);
      break;
    }
  }
  double pct = 0;
  for (const ApModifier& m : u.apMods)
    if (covers(m, a, category)) v += m.flat, pct += m.pct;
  (void)b;
  return finish(v, pct, r.rounding, 0);
}

int apStepCost(const Battle& b, const BattleUnit& u, int x, int y, int effectiveMoveCost) {
  const ApRules& r = apRules();
  const BattleCell& c = b.cell(x, y);
  const ApRules::Movement::Terrain* t = r.terrainOf(c.terrain);
  double v = t && t->apCost ? *t->apCost : effectiveMoveCost;
  double pct = 0;
  if (t) v += t->modifier;
  // the spells' ground: the zones laid on the hex
  auto has = [&](const std::vector<BattleHex>& cells) { return std::find(cells.begin(), cells.end(), BattleHex{x, y}) != cells.end(); };
  auto zone = [&](const char* key) { const auto it = r.movement.zones.find(key); return it == r.movement.zones.end() ? 0 : it->second; };
  if (std::any_of(b.webZones().begin(), b.webZones().end(), [&](const Battle::WebZone& z) { return has(z.cells); })) v += zone("web");
  if (std::any_of(b.iceZones().begin(), b.iceZones().end(), [&](const Battle::IceZone& z) { return has(z.cells); })) v += zone("iceStorm");
  if (std::any_of(b.auraZones().begin(), b.auraZones().end(), [&](const Battle::AuraZone& z) { return has(z.cells); })) v += zone("aura");
  if (std::any_of(b.zones().begin(), b.zones().end(), [&](const Battle::Zone& z) { return z.kind != "meteor" && z.covers({x, y}); })) v += zone("zone");
  // the unit: its conditions and its movement modifiers
  for (const std::string& key : statusKeys(u))
    if (const auto it = r.status.find(key); it != r.status.end()) v += it->second.movementFlat, pct += it->second.movementPct;
  for (const std::string& item : u.wornItems)
    if (const auto it = r.equipment.find(item); it != r.equipment.end()) v += it->second.movementFlat, pct += it->second.movementPct;
  for (const std::string& kind : u.spellList)
    if (const auto it = r.talents.find(kind); it != r.talents.end()) v += it->second.movementFlat, pct += it->second.movementPct;
  for (const ApModifier& m : u.apMods)
    if (m.scope == ApModifier::Scope::Movement && (m.filter.empty() || m.filter == c.terrain || (t && m.filter == t->category))) v += m.flat, pct += m.pct;
  return finish(v, pct, r.movement.rounding, r.movement.minimum);
}

int apCheapestStep(const Battle& b, const BattleUnit& u) {
  const ApRules& r = apRules();
  int best = std::numeric_limits<int>::max();
  for (int y = 0; y < b.rows(); ++y)
    for (int x = 0; x < b.cols(); ++x) {
      const BattleCell& c = b.cell(x, y);
      if (!c.passable) continue;
      best = std::min(best, apStepCost(b, u, x, y, c.moveCost));
    }
  return best == std::numeric_limits<int>::max() ? r.movement.minimum : best;
}

std::string apCounterType(const Battle& b, const BattleUnit& d, const BattleUnit& a, BattleHex from) {
  (void)b;
  const int dist = Battle::distance({d.x, d.y}, from);
  if (d.combat.offHandWeapon && dist <= d.combat.offMaxRange) return apOffHandCategory(d);  // the dagger beside the bow
  const std::string category = apWeaponCategory(d);
  const bool bolt = !d.combat.arrow && (d.combat.arcane || d.combat.staff);
  if (category == "staff" && dist <= 1) return "staff";  // its staff on the foe beside it (MapStage's counterStaff pose)
  if (bolt) return "spell";                               // its arcane bolt from afar
  (void)a;
  return category;
}

int apCounterCost(const Battle& b, const BattleUnit& d, const BattleUnit& a, BattleHex from) {
  return apCost(b, d, ApAction::counter(apCounterType(b, d, a, from)));
}

int apOwnCounterCost(const Battle& b, const BattleUnit& u) {
  const std::string category = apWeaponCategory(u);
  const bool bolt = !u.combat.arrow && (u.combat.arcane || u.combat.staff);
  const std::string type = category == "staff" ? "staff" : bolt ? "spell" : category;
  return apCost(b, u, ApAction::counter(type));
}

bool apCanCounter(const Battle& b, const BattleUnit& d, const BattleUnit& a, BattleHex from) {
  return apCanAfford(d, apCounterCost(b, d, a, from));
}

bool apCanAfford(const BattleUnit& u, int cost) { return cost <= u.ap; }

bool apTrySpend(BattleUnit& u, int cost) {
  if (cost < 0) cost = 0;
  if (cost > u.ap) return false;
  u.ap -= cost;
  return true;
}

int apStepsCost(const Battle& b, size_t owner, const std::vector<BattleStep>& steps) {
  if (steps.empty() || owner >= b.units().size()) return 0;
  const BattleUnit& u = b.units()[owner];
  const BattleStep& s = steps.front();
  switch (s.type) {
    case BattleStep::Type::Combat:
      if (s.kind.empty()) return apCost(b, u, s.customDice ? ApAction::offHand() : ApAction::attack());
      if (s.kind == "shieldBash" && s.stunChance > 0 && !s.customDice && s.att == owner && steps.size() == 1 && s.dmgMul == u.combat.offShieldMul)
        return apCost(b, u, ApAction::offHand());  // the off hand's bash (offHandStep), not the Golpe de Escudo skill
      return apCost(b, u, ApAction::cast(s.kind));
    case BattleStep::Type::Potion: return apCost(b, u, ApAction::potion());
    case BattleStep::Type::Move: return 0;  // a walk is paid hex by hex (Battle::move / MapStage walkAlong)
    default: return apCost(b, u, ApAction::cast(s.kind));
  }
}

bool apCommitSteps(Battle& b, size_t owner, const std::vector<BattleStep>& steps) {
  if (owner >= b.units().size()) return false;
  const int cost = apStepsCost(b, owner, steps);
  return apTrySpend(b.units()[owner], cost);
}

void apRefresh(Battle& b, size_t unit) {
  if (unit >= b.units().size()) return;
  BattleUnit& u = b.units()[unit];
  if (u.apRefreshRound == b.round()) return;  // once per round, whatever begins the turn again (a fight resumed)
  u.apRefreshRound = b.round();
  // the expiring modifiers: one own turn less; gone at 0
  for (ApModifier& m : u.apMods)
    if (m.turns > 0) --m.turns;
  std::erase_if(u.apMods, [](const ApModifier& m) { return m.turns == 0; });
  const ApRules& r = apRules();
  const int mx = apMax(b, u);
  if (r.refresh.policy == "accumulate") u.ap = std::min(static_cast<int>(std::floor(mx * r.refresh.accumulateCap)), std::max(0, u.ap) + mx);
  else u.ap = mx;
}

void apOnMaxChanged(BattleUnit& u, int oldMax, int newMax) {
  u.ap = std::max(0, std::min(newMax, u.ap + (newMax - oldMax)));
}

ApExamine apExamine(const Battle& b, size_t examiner, size_t target) {
  ApExamine e;
  if (examiner >= b.units().size() || target >= b.units().size()) return e;
  const BattleUnit &me = b.units()[examiner], &foe = b.units()[target];
  const Forecast out = forecast(b, examiner, {me.x, me.y}, target), in = forecast(b, target, {foe.x, foe.y}, examiner);
  e.dmgOut = out.dmgOut, e.hitOut = out.hitOut, e.dmgIn = in.dmgOut, e.hitIn = in.hitOut;
  const double mine = std::max(0.0, out.dmgOut * out.hitOut / 100.0), theirs = std::max(0.0, in.dmgOut * in.hitOut / 100.0);
  // turns the examiner needs to down it (its HP over my expected blow) over the turns it needs to down the examiner
  const double myTurns = mine > 0 ? std::max(1, foe.hp) / mine : std::numeric_limits<double>::infinity();
  const double itsTurns = theirs > 0 ? std::max(1, me.hp) / theirs : std::numeric_limits<double>::infinity();
  if (std::isinf(myTurns) && std::isinf(itsTurns)) e.ratio = 1;
  else if (std::isinf(myTurns)) e.ratio = std::numeric_limits<double>::infinity();
  else if (std::isinf(itsTurns)) e.ratio = 0;
  else e.ratio = myTurns / itsTurns;
  const ApRules& r = apRules();
  for (const ApRules::Examine::Category& c : r.examine.categories) {
    e.categoryId = c.id, e.labelPt = c.pt, e.labelEn = c.en;
    if (!c.maxRatio || e.ratio <= *c.maxRatio) break;
  }
  e.weaponCategory = apWeaponCategory(foe);
  e.attackCost = apCost(b, foe, ApAction::attack());
  e.counterCost = apOwnCounterCost(b, foe);
  e.apMaxTarget = apMax(b, foe);
  return e;
}

void apWriteSnapshot(const BattleUnit& u, nlohmann::json& s) {
  s["ap"] = u.ap;
  s["apRefreshRound"] = u.apRefreshRound;
  s["armorCategory"] = u.armorCategory;
  s["wornItems"] = u.wornItems;
  nlohmann::json mods = nlohmann::json::array();
  for (const ApModifier& m : u.apMods) mods.push_back(m.toJson());
  s["apMods"] = std::move(mods);
}

void apReadSnapshot(const Battle& b, BattleUnit& u, const nlohmann::json& s, int round) {
  if (s.contains("armorCategory") && s["armorCategory"].is_string()) u.armorCategory = s["armorCategory"].get<std::string>();
  if (s.contains("wornItems") && s["wornItems"].is_array()) {
    u.wornItems.clear();
    for (const nlohmann::json& i : s["wornItems"])
      if (i.is_string()) u.wornItems.push_back(i.get<std::string>());
  }
  u.apMods.clear();
  if (s.contains("apMods") && s["apMods"].is_array())
    for (const nlohmann::json& j : s["apMods"])
      if (const std::optional<ApModifier> m = ApModifier::fromJson(j)) u.apMods.push_back(*m);
  u.apRefreshRound = s.value("apRefreshRound", -1);
  // A snapshot saved before AP existed: the full maximum, once (the round's refresh is then taken as done so the turn in
  // progress does not refresh it again).
  if (s.contains("ap") && s["ap"].is_number_integer()) u.ap = std::max(0, s["ap"].get<int>());
  else u.ap = apMax(b, u), u.apRefreshRound = round;
}

std::string apArmorCategoryOfItem(const std::string& itemId) {
  const ApRules& r = apRules();
  if (itemId.empty()) return r.armor.defaultCategory;
  const auto it = r.armor.items.find(itemId);
  return it == r.armor.items.end() ? r.armor.defaultCategory : it->second;
}

std::string apArmorName(const std::string& category, bool en) {
  const ApRules& r = apRules();
  const std::string& c = category.empty() ? r.armor.defaultCategory : category;
  const auto it = r.armor.names.find(c);
  if (it == r.armor.names.end()) return c;
  return en ? it->second.second : it->second.first;
}

}  // namespace ember
