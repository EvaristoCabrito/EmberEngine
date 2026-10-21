#include "world/FieldSpells.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "world/Battle.h"
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

// --- Caçada (ours): the archer hunts game on the world map: rations for the party, one tier-2 use ---------------------------

FoodPower FieldSpells::huntPower(int level) {
  if (level >= 26) return {3, 6, 3, 0};
  if (level >= 18) return {2, 6, 2, 0};
  if (level >= 10) return {2, 4, 1, 0};
  return {1, 4, 1, 0};
}

std::string FieldSpells::huntFormula(int level, bool english) {
  const FoodPower p = huntPower(level);
  return std::format("{}D{}+{} {}", p.dice, p.faces, p.bonus, english ? "rations" : "rações");
}

std::vector<std::string> FieldSpells::hunters(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "archer" && levelOf(save, h) >= kHuntUnlockLevel) out.push_back(h);
  }
  return out;
}

std::optional<Json> FieldSpells::castHunt(const Json& save, const std::string& hero, const std::function<double()>& rng, int* rations) const {
  if (rulesClass(classOf(save, hero)) != "archer" || levelOf(save, hero) < kHuntUnlockLevel || !canHunt(save, hero)) return std::nullopt;
  const FoodPower power = huntPower(levelOf(save, hero));
  int gained = power.bonus;
  for (int i = 0; i < power.dice; ++i) gained += 1 + static_cast<int>(std::floor(rng() * power.faces));  // rollDice
  Json next = save;
  next["rations"] = number(num(save, "rations").value_or(0) + gained);
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kHuntTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  if (rations) *rations = gained;
  return next;
}

// --- The Curandeiro's (ours, docs/SKILLS_HEALER.md) ---------------------------------------------------------------------------

std::vector<std::string> FieldSpells::campers(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "healer" && levelOf(save, h) >= kCampUnlockLevel) out.push_back(h);
  }
  return out;
}

// Consagrar Acampamento heals every standing hero of the party to its whole (GameApp mapStatusUnit's max: class + gear, less hunger);
// nothing to do when nobody is hurt.
bool FieldSpells::canCamp(const Json& save, bool test) const {
  const InnRules rules(inn_, world_);
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rules.statusHp(save, h) < rules.statusMaxHp(save, h)) return true;
  }
  return false;
}

std::optional<Json> FieldSpells::castCamp(const Json& save, const std::string& hero, bool test, int* healed) const {
  if (rulesClass(classOf(save, hero)) != "healer" || levelOf(save, hero) < kCampUnlockLevel || !canCamp(save, hero, test)) return std::nullopt;
  const InnRules rules(inn_, world_);
  Json next = save;
  if (!next["unitHp"].is_object()) next["unitHp"] = Json::object();
  int count = 0;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    const int max = rules.statusMaxHp(save, h);
    if (rules.statusHp(save, h) >= max) continue;
    next["unitHp"][h] = max;
    ++count;
  }
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kCampTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  if (healed) *healed = count;
  return next;
}

int FieldSpells::prayerHexes(int level) { return level >= 30 ? 16 : level >= 23 ? 14 : level >= 16 ? 12 : 10; }

int FieldSpells::prayerLeft(const Json& save) { return std::max(0, static_cast<int>(num(save, kPrayerKey).value_or(0))); }

std::vector<std::string> FieldSpells::prayers(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "healer" && levelOf(save, h) >= kPrayerUnlockLevel) out.push_back(h);
  }
  return out;
}

// Prece do Caminho: the save counts the hexes it still covers (Overworld::step spends one per hex and reads it: no undead encounter,
// battles half as likely). A new prayer starts the count over at the caster's own reach (it never adds up).
std::optional<Json> FieldSpells::castPrayer(const Json& save, const std::string& hero) const {
  if (rulesClass(classOf(save, hero)) != "healer" || levelOf(save, hero) < kPrayerUnlockLevel || !canPray(save, hero)) return std::nullopt;
  Json next = save;
  next[kPrayerKey] = std::max(prayerLeft(save), prayerHexes(levelOf(save, hero)));
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kPrayerTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  return next;
}

// --- Identificar (ours, the Mago Negro's, docs/SKILLS_MAGE.md) ------------------------------------------------------------

std::vector<std::string> FieldSpells::identifiers(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "mage") out.push_back(h);
  }
  return out;
}

std::optional<Json> FieldSpells::castIdentify(const Json& save, const std::string& hero, const InnRules::UnknownItem& item) const {
  if (rulesClass(classOf(save, hero)) != "mage" || usesLeft(save, hero, kIdentifyTier) <= 0) return std::nullopt;
  const InnRules rules(inn_, world_);
  bool exists = false;
  for (const InnRules::UnknownItem& u : rules.unknownItems(save))
    if (u.kind == item.kind && u.id == item.id && u.hero == item.hero) exists = true;
  if (!exists) return std::nullopt;
  Json next = rules.identify(save, item);
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kIdentifyTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  return next;
}

// --- The Conjurador's runes (ours, docs/SKILLS_CONJURER.md) ---------------------------------------------------------------

std::vector<FieldSpells::Rune> FieldSpells::runes(const Json& save) {
  std::vector<Rune> out;
  const Json& list = child(save, kRunesKey);
  if (!list.is_array()) return out;
  for (const Json& r : list) {
    if (!r.is_object()) continue;
    Rune x;
    x.id = r.value("id", ""), x.place = r.value("place", ""), x.region = r.value("region", ""), x.note = r.value("note", "");
    x.col = r.value("col", 0), x.row = r.value("row", 0), x.day = r.value("day", 0);
    if (!x.id.empty()) out.push_back(std::move(x));
  }
  return out;
}

Json FieldSpells::runeJson(const Rune& r) {
  return Json{{"id", r.id}, {"place", r.place}, {"region", r.region}, {"note", r.note}, {"col", r.col}, {"row", r.row}, {"day", r.day}};
}

std::vector<std::string> FieldSpells::runeMarkers(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "conjurer" && levelOf(save, h) >= kRuneUnlockLevel) out.push_back(h);
  }
  return out;
}

std::vector<std::string> FieldSpells::recallers(const Json& save, bool test) const {
  std::vector<std::string> out;
  for (const char* h : kHeroes) {
    if (!(test || world_.heroRecruited(h, save)) || num(child(save, "unitHp"), h).value_or(1) <= 0) continue;
    if (rulesClass(classOf(save, h)) == "conjurer" && levelOf(save, h) >= kRecallUnlockLevel) out.push_back(h);
  }
  return out;
}

FieldSpells::Rune FieldSpells::runeHere(const Json& save) const {
  Rune r;
  const Json& pos = child(save, "overworldPos");
  r.col = static_cast<int>(num(pos, "col").value_or(0)), r.row = static_cast<int>(num(pos, "row").value_or(0));
  r.day = static_cast<int>(num(save, "gameClock").value_or(0));
  if (const OverworldCell* c = world_.cell(r.col, r.row)) r.region = c->region;
  // the nearest place (the one on the hex first)
  int best = 1 << 30;
  for (const MapLocation& l : world_.locations()) {
    const int d = Battle::distance({r.col, r.row}, {l.col, l.row});
    if (d < best) best = d, r.place = l.name;
  }
  if (best > 0 && !r.place.empty()) r.place = std::format("{} ({} {})", r.place, best, best == 1 ? "hex" : "hexes");
  r.id = std::format("rune-{}-{}-{}", r.col, r.row, r.day);
  return r;
}

bool FieldSpells::runeAt(const Json& save, int col, int row) {
  for (const Rune& r : runes(save))
    if (r.col == col && r.row == row) return true;
  return false;
}

bool FieldSpells::canMark(const Json& save, const std::string& hero, bool test) const {
  if (usesLeft(save, hero, kRuneTier) <= 0) return false;
  const Rune here = runeHere(save);
  if (runeAt(save, here.col, here.row)) return false;
  const InnRules rules(inn_, world_);
  return test || rules.bagUsed(save) < rules.bagCapacity(save);
}

std::optional<Json> FieldSpells::castMarkRune(const Json& save, const std::string& hero, bool test) const {
  if (rulesClass(classOf(save, hero)) != "conjurer" || levelOf(save, hero) < kRuneUnlockLevel || !canMark(save, hero, test)) return std::nullopt;
  Rune r = runeHere(save);
  Json next = save;
  if (!next[kRunesKey].is_array()) next[kRunesKey] = Json::array();
  // a second rune of the same hex on the same day gets its own id
  int serial = 0;
  for (const Rune& x : runes(save))
    if (x.id.starts_with(r.id)) ++serial;
  if (serial > 0) r.id += std::format("-{}", serial);
  next[kRunesKey].push_back(runeJson(r));
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kRuneTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  return next;
}

std::optional<Json> FieldSpells::castRecall(const Json& save, const std::string& hero, const std::string& runeId) const {
  if (rulesClass(classOf(save, hero)) != "conjurer" || levelOf(save, hero) < kRecallUnlockLevel || usesLeft(save, hero, kRecallTier) <= 0) return std::nullopt;
  const Rune* rune = nullptr;
  const std::vector<Rune> all = runes(save);
  for (const Rune& r : all)
    if (r.id == runeId) rune = &r;
  if (!rune || !world_.isCell(rune->col, rune->row)) return std::nullopt;
  Json next = save;
  next["overworldPos"] = Json{{"col", rune->col}, {"row", rune->row}};
  const std::string hex = std::format("{},{}", rune->col, rune->row);
  if (!next["exploredHexes"].is_array()) next["exploredHexes"] = Json::array();
  if (!has(next["exploredHexes"], hex)) next["exploredHexes"].push_back(hex);
  if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
  if (!next["spellUses"][hero].is_object()) next["spellUses"][hero] = Json::object();
  const std::string key = "tier" + std::to_string(kRecallTier);
  next["spellUses"][hero][key] = number(num(next["spellUses"][hero], key).value_or(0) + 1);
  next["pendingMission"] = nullptr;
  return next;
}

Json FieldSpells::setRuneNote(const Json& save, const std::string& runeId, const std::string& note) {
  Json next = save;
  if (!next[kRunesKey].is_array()) return next;
  for (Json& r : next[kRunesKey])
    if (r.is_object() && r.value("id", "") == runeId) r["note"] = note;
  return next;
}

Json FieldSpells::discardRune(const Json& save, const std::string& runeId) {
  Json next = save;
  if (!next[kRunesKey].is_array()) return next;
  Json kept = Json::array();
  for (const Json& r : next[kRunesKey])
    if (!(r.is_object() && r.value("id", "") == runeId)) kept.push_back(r);
  next[kRunesKey] = std::move(kept);
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
