#include "world/FieldSpells.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "world/Noise.h"

namespace ember::e2 {

namespace {

const Json& child(const Json& obj, const std::string& key) {
  static const Json kNull;
  if (!obj.is_object()) return kNull;
  const auto it = obj.find(key);
  return it == obj.end() ? kNull : *it;
}
std::optional<double> num(const Json& obj, const std::string& key) {
  if (!obj.is_object()) return std::nullopt;
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_number()) return std::nullopt;
  return it->get<double>();
}
Json number(double v) {
  if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 9.0e15) return Json(static_cast<long long>(v));
  return Json(v);
}
bool has(const Json& array, const std::string& value) {
  if (!array.is_array()) return false;
  for (const Json& v : array)
    if (v.is_string() && v.get<std::string>() == value) return true;
  return false;
}
constexpr const char* kHeroes[] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};

}  // namespace

std::string FieldSpells::rulesClass(const std::string& classId) {
  static const std::pair<const char*, const char*> kBase[] = {
      {"elementalist", "mage"}, {"warlock", "mage"},   {"sorcerer", "conjurer"},   {"necromancer", "conjurer"},
      {"bishop", "healer"},     {"cleric", "healer"},  {"paladin", "swordsman"},   {"heavyKnight", "swordsman"},
      {"ranger", "archer"},     {"assassin", "archer"}, {"sentinel", "lancer"},    {"templar", "lancer"}};
  std::string base = classId;
  for (const auto& [from, to] : kBase)
    if (classId == from) base = to;
  if (base == "voss") return "mage";
  if (base == "salazar") return "healer";
  if (base == "neera") return "archer";
  if (base == "kaelFinal" || base == "kaelEarly") return "swordsman";
  return base;
}

FoodPower FieldSpells::foodPower(int level, int innFullness) {
  const int fullness = level >= 10 ? innFullness : 100;
  if (level >= 15) return {2, 4, 2, fullness};
  if (level >= 13) return {1, 6, 2, fullness};
  if (level >= 11) return {1, 6, 1, fullness};
  if (level >= 9) return {1, 4, 1, fullness};
  if (level >= 7) return {1, 4, 0, fullness};
  if (level >= 5) return {1, 3, 0, fullness};
  if (level >= 3) return {1, 2, 0, fullness};
  return {0, 0, 0, fullness};
}

std::string FieldSpells::foodFormula(int level, bool english) {
  const FoodPower p = foodPower(level);
  std::string food = english ? "Full meal" : "Comida completa";
  if (p.fullness > 100) food += std::format(" ({}%)", p.fullness);
  if (p.dice <= 0) return food;
  const std::string dice = p.bonus ? std::format("{}D{}+{}", p.dice, p.faces, p.bonus) : std::format("{}D{}", p.dice, p.faces);
  return std::format("{} + {} {}", food, dice, english ? "Rations" : "Rações");
}

std::string FieldSpells::classOf(const Json& save, const std::string& hero) const {
  const Json& promo = child(child(save, "promotions"), hero);
  if (promo.is_string()) return promo.get<std::string>();
  const Json& base = child(world_.tables()["baseClass"], hero);
  return base.is_string() ? base.get<std::string>() : std::string();
}

int FieldSpells::levelOf(const Json& save, const std::string& hero) const { return static_cast<int>(num(child(save, "levels"), hero).value_or(1)); }

int FieldSpells::usesLeft(const Json& save, const std::string& hero, int tier) const {
  const std::string cls = classOf(save, hero);
  if (cls.empty() || cls == "swampBlueCalf") return 0;
  auto table = inn_.tierUses.find(cls);
  if (table == inn_.tierUses.end()) table = inn_.tierUses.find(rulesClass(cls));
  if (table == inn_.tierUses.end()) return 0;
  const size_t row = static_cast<size_t>(std::clamp(levelOf(save, hero), 1, 30) - 1);
  if (row >= table->second.size() || tier < 1 || static_cast<size_t>(tier) > table->second[row].size()) return 0;
  const Json& used = child(child(save, "spellUses"), hero);
  const double done = num(used, "tier" + std::to_string(tier)).value_or(0);
  return std::max(0, table->second[row][static_cast<size_t>(tier - 1)] - static_cast<int>(done));
}

std::vector<std::string> FieldSpells::healers(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save))) continue;
    if (num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "healer") out.push_back(h);
  }
  return out;
}

std::vector<std::string> FieldSpells::mages(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "mage" && levelOf(save, h) >= kWarpUnlockLevel) out.push_back(h);
  }
  return out;
}

std::vector<const MapLocation*> FieldSpells::warpCities(const Json& save) const {
  std::vector<const MapLocation*> out;
  const Json& pos = child(save, "overworldPos");
  const int col = static_cast<int>(num(pos, "col").value_or(0)), row = static_cast<int>(num(pos, "row").value_or(0));
  const Json& explored = child(save, "exploredHexes");
  for (const MapLocation& l : world_.locations()) {
    if (l.col == col && l.row == row) continue;
    bool visited = has(explored, std::format("{},{}", l.col, l.row));
    for (const std::string& id : l.missionIds) visited = visited || has(child(save, "completed"), id);
    if (visited) out.push_back(&l);
  }
  return out;
}

bool FieldSpells::canFeed(const Json& save, const std::string& hero) const {
  if (usesLeft(save, hero, kFoodTier) <= 0) return false;
  const FoodPower power = foodPower(levelOf(save, hero), inn_.innFullness);
  return Overworld::fullness(child(child(save, "heroHunger"), hero)) < power.fullness;
}

std::optional<Json> FieldSpells::castFood(const Json& save, const std::string& hero, const std::function<double()>& rng) const {
  if (rulesClass(classOf(save, hero)) != "healer" || !canFeed(save, hero)) return std::nullopt;
  const FoodPower power = foodPower(levelOf(save, hero), inn_.innFullness);
  int gained = 0;
  if (power.dice > 0) {
    gained = power.bonus;
    for (int i = 0; i < power.dice; ++i) gained += 1 + static_cast<int>(std::floor(rng() * power.faces));  // rollDice
  }
  Json next = save;
  if (!next["heroHunger"].is_object()) next["heroHunger"] = Json::object();
  next["heroHunger"][hero] = power.fullness;
  next["rations"] = number(num(save, "rations").value_or(0) + gained);
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kFoodTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  return next;
}

std::optional<Json> FieldSpells::castWarp(const Json& save, const std::string& hero, const std::string& cityId) const {
  if (rulesClass(classOf(save, hero)) != "mage" || levelOf(save, hero) < kWarpUnlockLevel) return std::nullopt;
  const MapLocation* city = nullptr;
  for (const MapLocation* l : warpCities(save))
    if (l->id == cityId) city = l;
  if (!city || usesLeft(save, hero, kWarpTier) <= 0) return std::nullopt;
  Json next = save;
  next["overworldPos"] = Json{{"col", city->col}, {"row", city->row}};
  const std::string hex = std::format("{},{}", city->col, city->row);
  if (!next["exploredHexes"].is_array()) next["exploredHexes"] = Json::array();
  if (!has(next["exploredHexes"], hex)) next["exploredHexes"].push_back(hex);
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kWarpTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  next["pendingMission"] = nullptr;
  return next;
}

}  // namespace ember::e2
