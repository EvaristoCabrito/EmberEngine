#include "world/Overworld.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>
#include <set>

#include "core/Log.h"
#include "world/Battle.h"
#include "world/Combat.h"

namespace ember::e2 {

namespace {

constexpr double kRecoveryPct = 0.08;
constexpr double kBattleEncounterChance = 0.15;
constexpr double kTextEncounterChance = 0.25;
constexpr double kTravelDiseaseChance = 0.02;
constexpr int kHungerGraceDays = 3;
constexpr double kHungerPenaltyPerDay = 0.1;
constexpr double kHungerPenaltyMax = 0.9;
const char* const kHeroes[] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};  // HERO_BASE_CLASS order

// overworld.ts ENCOUNTERS (module-private there, so copied as written).
struct RoadEncounter {
  const char* text;
  int rationsDice = 0, ember = 0, goldLossDice = 0;
  bool lootBag = false, losePotion = false, loseLockpick = false;
  double diseaseChance = 0;
  int alertDays = 0;
};
const RoadEncounter kEncounters[] = {
    {"Um bando de corvos assusta a coluna e parte das rações se perde na correria.", 8},
    {"Vestígios de um acampamento abandonado — e uma bolsa esquecida.", 0, 0, 0, true},
    {"Chuva forte atrasa a marcha, mas ninguém se machuca.", 0, 0, 0, false, false, false, 0.05},
    {"Pegadas grandes demais cruzam o caminho. O grupo segue mais alerta.", 0, 0, 0, false, false, false, 0, 3},
    {"Uma carroça atolada cede de vez; comida e mantimentos caem no barro.", 4},
    {"Um frasco se solta durante a descida e se quebra nas pedras.", 0, 0, 0, false, true},
    {"Ladrões passam pelo acampamento durante a noite e levam uma gazua.", 0, 0, 0, false, false, true},
    {"A ponte podre arrebenta sob a carga: parte do ouro e das rações vai para o rio.", 3, 0, 8},
};

double jsRound(double v) { return std::floor(v + 0.5); }

// `obj?.[key]` as a number: nullopt when absent, null or not a number (JS `?? fallback` then applies).
std::optional<double> num(const Json& obj, const std::string& key) {
  if (!obj.is_object()) return std::nullopt;
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_number()) return std::nullopt;
  return it->get<double>();
}
const Json& child(const Json& obj, const std::string& key) {
  static const Json kNull;
  if (!obj.is_object()) return kNull;
  const auto it = obj.find(key);
  return it == obj.end() ? kNull : *it;
}
bool has(const Json& array, const std::string& value) {
  if (!array.is_array()) return false;
  for (const Json& v : array)
    if (v.is_string() && v.get<std::string>() == value) return true;
  return false;
}
// JS truthiness of obj[key].
bool truthy(const Json& obj, const std::string& key) {
  const Json& v = child(obj, key);
  if (v.is_null()) return false;
  if (v.is_boolean()) return v.get<bool>();
  if (v.is_number()) return v.get<double>() != 0.0;
  if (v.is_string()) return !v.get<std::string>().empty();
  return true;
}
std::string key(int col, int row) { return std::to_string(col) + "," + std::to_string(row); }
// A JS number as JSON: integral values stay integers (as JSON.parse reads them back).
Json number(double v) {
  if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 9.0e15) return Json(static_cast<long long>(v));
  return Json(v);
}
std::string jsNumber(double v) {
  if (v == std::floor(v) && std::abs(v) < 9.0e15) return std::to_string(static_cast<long long>(v));
  return std::format("{}", v);
}

// poison.ts poisonTierOf
std::string poisonTierOf(const Json& value) {
  if (value.is_boolean() && value.get<bool>()) return "lesser";
  if (value.is_number()) {
    const double v = value.get<double>();
    if (v == 4) return "lesser";
    if (v == 10) return "poison";
    return {};
  }
  if (value.is_string()) {
    const std::string s = value.get<std::string>();
    if (s == "lesser" || s == "poison" || s == "greater" || s == "deadly" || s == "lethal") return s;
  }
  return {};
}

double hungerPenaltyFor(double streak) {
  const double daysPast = std::max(0.0, streak - kHungerGraceDays);
  return std::min(kHungerPenaltyMax, daysPast * kHungerPenaltyPerDay);
}

}  // namespace

// --- loading -------------------------------------------------------------------------------------------------------

bool Overworld::load(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    logError("overworld data not found: {}", file.string());
    return false;
  }
  const Json j = Json::parse(in, nullptr, false);
  if (j.is_discarded()) {
    logError("overworld data unreadable: {}", file.string());
    return false;
  }
  const Json& board = j["board"];
  auto hexOf = [](const Json& h) { return OverworldHex{h.value("col", 0), h.value("row", 0)}; };
  start_ = hexOf(board["startHex"]), inn_ = hexOf(board["innHex"]), ashenBlocked_ = hexOf(board["ashenBlocked"]);
  if (board.contains("wispHex") && board["wispHex"].is_object()) wisp_ = hexOf(board["wispHex"]);
  westEdgeCol_ = board.value("westEdgeCol", 0);
  for (const Json& id : board.value("stoneBridgeMissionIds", Json::array())) stoneBridgeMissionIds_.push_back(id.get<std::string>());
  for (const Json& c : board["cells"]) {
    OverworldCell cell;
    cell.col = c.value("col", 0), cell.row = c.value("row", 0), cell.hours = c.value("hours", 6);
    cell.region = c.value("region", ""), cell.x = c.value("x", 0.0), cell.y = c.value("y", 0.0);
    for (const Json& e : c.value("encounters", Json::array())) cell.encounters.push_back(e.get<std::string>());
    cells_.push_back(std::move(cell));
  }
  for (const Json& l : j["locations"]) {
    MapLocation m;
    m.id = l.value("id", ""), m.name = l.value("name", "");
    m.x = l.value("x", 0.0), m.y = l.value("y", 0.0), m.col = l.value("col", 0), m.row = l.value("row", 0);
    for (const Json& id : l.value("missionIds", Json::array())) m.missionIds.push_back(id.get<std::string>());
    m.openAccess = l.value("openAccess", false), m.encountersAllowed = l.value("encountersAllowed", false), m.warpCity = l.value("warpCity", false);
    if (l.contains("deadlineDay") && l["deadlineDay"].is_number()) m.deadlineDay = l["deadlineDay"].get<double>();
    locations_.push_back(std::move(m));
  }
  for (const Json& m : j["missions"]) {
    MapMission x;
    x.id = m.value("id", ""), x.title = m.value("title", ""), x.index = m.value("index", 0);
    x.win = m.contains("win") && m["win"].is_string() ? m["win"].get<std::string>() : std::string();
    x.hub = m.value("hub", false), x.crossing = m.value("crossing", false), x.gated = m.value("gated", false);
    missions_.push_back(std::move(x));
  }
  tables_ = j["tables"];
  for (const Json& w : tables_["weapons"])
    loot_.push_back({true, w["id"].get<std::string>(), w["name"].get<std::string>(), w["price"].get<double>(), w["power"].get<int>(), w["starting"].get<bool>()});
  for (const Json& e : tables_["equipment"])
    loot_.push_back({false, e["id"].get<std::string>(), e["name"].get<std::string>(), e["price"].get<double>(), e["power"].get<int>(), e["starting"].get<bool>()});
  logInfo("overworld: {} cells, {} locations, {} missions", cells_.size(), locations_.size(), missions_.size());
  return true;
}

// --- the board -----------------------------------------------------------------------------------------------------

const OverworldCell* Overworld::cell(int col, int row) const {
  for (const OverworldCell& c : cells_)
    if (c.col == col && c.row == row) return &c;
  return nullptr;
}

std::vector<OverworldHex> Overworld::neighbors(int col, int row) {
  std::vector<OverworldHex> out;
  for (const BattleHex& h : Battle::neighbors(col, row)) out.push_back({h.x, h.y});
  return out;
}

// overworld.ts hexToWorld (odd-r offset to cube, pointy-top, hex size 5 % of the image).
void Overworld::hexToWorld(int col, int row, double& x, double& y) {
  const double q = col - (row - (row & 1)) / 2, r = row;
  x = 5.0 * std::sqrt(3.0) * (q + r / 2);
  y = 5.0 * 1.5 * r;
}

const MapLocation* Overworld::locationAt(int col, int row) const {
  const MapLocation* found = nullptr;
  for (const MapLocation& l : locations_)
    if (l.col == col && l.row == row) found = &l;
  return found;
}

const MapMission* Overworld::mission(const std::string& id) const {
  for (const MapMission& m : missions_)
    if (m.id == id) return &m;
  return nullptr;
}

std::vector<const MapMission*> Overworld::missionsOf(const MapLocation& location) const {
  std::vector<const MapMission*> out;
  for (const std::string& id : location.missionIds)
    if (const MapMission* m = mission(id)) out.push_back(m);
  return out;
}

int Overworld::missionIndex(const std::string& id) const {
  const MapMission* m = mission(id);
  return m ? m->index : -1;
}

// --- small rules ---------------------------------------------------------------------------------------------------

std::string Overworld::travelTimeLabel(int hours) {
  return hours == 24 ? "1 dia" : hours == 36 ? "1 dia e 12 horas" : std::to_string(hours) + " horas";
}

double Overworld::fullness(const Json& value) {
  return value.is_number() && std::isfinite(value.get<double>()) ? std::max(0.0, std::min(120.0, value.get<double>())) : 100.0;
}

int Overworld::campaignHour(const Json& save) {
  const std::optional<double> h = num(save, "gameHour");
  return h ? static_cast<int>(*h) : 8;
}

std::string Overworld::timeOfDay(int hour) {
  if (hour < 5) return "darkNight";
  if (hour < 8) return "dawn";
  if (hour < 11) return "day";
  if (hour < 15) return "noon";
  if (hour < 18) return "day";
  if (hour < 21) return "dusk";
  return "brightNight";
}

// data.ts heroRecruited (HERO_JOIN_INDEX exported per hero; null = never through a mission).
bool Overworld::heroRecruited(const std::string& hero, const Json& save) const {
  if (has(child(save, "flags"), "recruited:" + hero)) return true;
  const Json& join = child(tables_["joinIndex"], hero);
  if (!join.is_number()) return false;
  const int joinIndex = join.get<int>();
  if (joinIndex <= 0) return true;
  for (const Json& id : child(save, "completed"))
    if (id.is_string() && missionIndex(id.get<std::string>()) >= joinIndex - 1) return true;
  return false;
}

int Overworld::classHp(const std::string& classId, int level) const {
  const Json& c = child(tables_["classes"], classId);
  if (!c.is_object()) return 0;
  const Json& hp = c["hp"];
  const int i = std::clamp(level, 1, static_cast<int>(hp.size())) - 1;  // statsFor: n = max(0, min(MAX, level) - 1)
  return hp[static_cast<size_t>(i)].get<int>();
}

double Overworld::classResistance(const std::string& classId, const std::string& element) const {
  return num(child(child(tables_["classes"], classId), "resistances"), element).value_or(0.0);
}

// overworld.ts maxHpFor: the hero's class (promotion first), statsFor(class, level).hp; 0 without a class.
int Overworld::maxHp(const Json& save, const std::string& hero) const {
  std::string classId;
  const Json& promo = child(child(save, "promotions"), hero);
  if (promo.is_string()) classId = promo.get<std::string>();
  else if (const Json& base = child(tables_["baseClass"], hero); base.is_string()) classId = base.get<std::string>();
  if (classId.empty()) return 0;
  return classHp(classId, static_cast<int>(num(child(save, "levels"), hero).value_or(1.0)));
}

// gearStatBonus(Object.values(save.equipment[hero])).resistances[element]: 0 (no weapon), then each item in order.
double Overworld::gearResistance(const Json& save, const std::string& hero, const std::string& element) const {
  double total = 0.0;
  total = total + 0.0;  // the weapon's magic (none here)
  const Json& slots = child(child(save, "equipment"), hero);
  if (!slots.is_object()) return total;
  for (const auto& [slot, id] : slots.items()) {
    if (!id.is_string()) continue;
    for (const Json& e : tables_["equipment"])
      if (e["id"].get<std::string>() == id.get<std::string>()) {
        total = total + num(e["resistances"], element).value_or(0.0);
        break;
      }
  }
  return total;
}

// data.ts partyBagHasRoom: loose weapons + loose gear + ration stacks against 10 per recruited hero + pouches.
bool Overworld::bagHasRoom(const Json& save, int extra) const {
  std::set<std::string> worn;
  for (const auto& [hero, id] : child(save, "equipped").items())
    if (id.is_string()) worn.insert(id.get<std::string>());
  int used = 0;
  for (const auto& [id, level] : child(save, "weapons").items())
    if (!worn.contains(id)) ++used;
  double gear = 0;
  for (const auto& [id, qty] : child(save, "looseEquipment").items()) gear += qty.is_number() ? qty.get<double>() : 0.0;
  const double rations = num(save, "rations").value_or(0.0);
  const double stacks = std::ceil(rations / tables_.value("rationStackMax", 30));
  int heroes = 0;
  for (const char* h : kHeroes)
    if (heroRecruited(h, save)) ++heroes;
  double pouches = 0;
  for (const auto& [hero, slots] : child(save, "equipment").items()) {
    const Json& waist = child(slots, "waist");
    if (!waist.is_string()) continue;
    for (const Json& e : tables_["equipment"])
      if (e["id"].get<std::string>() == waist.get<std::string>()) pouches += e.value("pouch", 0);
  }
  const double capacity = tables_.value("partyBagPerHero", 10) * std::max(1, heroes) + pouches;
  return used + gear + stacks + extra <= capacity;
}

// --- travel --------------------------------------------------------------------------------------------------------

bool Overworld::wispForestUncleared(const Json& save) const {
  for (const MapLocation& l : locations_)
    if (l.id == "wisp-forest") {
      for (const MapMission* m : missionsOf(l))
        if (!has(child(save, "completed"), m->id)) return true;
      return false;
    }
  return false;
}

// overworld.ts canStepOverworld (test = false).
bool Overworld::canStep(const Json& save, OverworldHex from, OverworldHex to) const {
  if (!isCell(to.col, to.row)) return false;
  if (to == ashenBlocked_) return false;
  const Json& completed = child(save, "completed");
  if (!has(completed, "vau")) return false;
  if (wisp_ && from.col <= wisp_->col && to.col > wisp_->col && wispForestUncleared(save)) return false;
  if (has(completed, "estalagem") || from == inn_) return true;
  if (to.row != start_.row || to.col < start_.col || to.col > inn_.col) return false;
  const bool atStart = from == start_;
  if (to.col < from.col) return true;
  if (atStart) return to.row == from.row && to.col == from.col + 1;
  for (const std::string& id : stoneBridgeMissionIds_)
    if (!has(completed, id)) return false;
  return to.col > from.col;
}

// overworld.ts stepOverworld (test = false).
Json Overworld::step(const Json& save, OverworldHex to, Mulberry32& rng, std::optional<OverworldEvent>& event) const {
  event.reset();
  const Json& pos = child(save, "overworldPos");
  const OverworldHex from{pos.value("col", 0), pos.value("row", 0)};
  const std::vector<OverworldHex> around = neighbors(from.col, from.row);
  if (!isCell(to.col, to.row) || std::find(around.begin(), around.end(), to) == around.end() || !canStep(save, from, to)) return save;

  const Json& completed = child(save, "completed");
  Json heroHunger = child(save, "heroHunger").is_object() ? child(save, "heroHunger") : Json::object();
  const int travelHours = cell(to.col, to.row)->hours;
  const int totalHours = campaignHour(save) + travelHours;
  const std::string arrivalTime = timeOfDay(totalHours % 24);
  const int elapsedDays = totalHours / 24;
  auto ages = [&](const std::string& hero) {
    if (!heroRecruited(hero, save)) return false;
    const std::optional<double> hp = num(child(save, "unitHp"), hero);
    return (hp ? *hp : static_cast<double>(maxHp(save, hero))) > 0;
  };
  for (const char* hero : kHeroes)
    if (ages(hero)) heroHunger[hero] = number(std::max(0.0, fullness(child(heroHunger, hero)) - travelHungerCost(travelHours)));

  // Half the spent spell charges come back per day crossed.
  Json spellUses = Json::object();
  for (const auto& [hero, tiers] : child(save, "spellUses").items()) {
    Json next = Json::object();
    if (tiers.is_object())
      for (const auto& [tier, spent] : tiers.items()) {
        const double recovered = std::floor((spent.is_number() ? spent.get<double>() : 0.0) / std::pow(2.0, elapsedDays));
        if (recovered > 0) next[tier] = number(recovered);
      }
    if (!next.empty()) spellUses[hero] = std::move(next);
  }

  std::vector<std::string> travelling;
  for (const char* hero : kHeroes)
    if (ages(hero)) travelling.push_back(hero);
  bool fed = true;
  for (const std::string& hero : travelling)
    if (!(num(heroHunger, hero).value_or(100.0) > 0)) fed = false;
  const double rations = num(save, "rations").value_or(0.0);
  const double saveStreak = num(save, "hungerStreak").value_or(0.0);
  const double prevPenalty = hungerPenaltyFor(saveStreak);
  double startingFullness = std::numeric_limits<double>::infinity();
  for (const std::string& hero : travelling) startingFullness = std::min(startingFullness, fullness(child(child(save, "heroHunger"), hero)));
  const double newlyHungryHours = std::max(0.0, travelHours - startingFullness * 24 / tables_.value("dailyHungerCost", 50.0));
  const double priorHours = num(save, "hungerHours").value_or(saveStreak * 24);
  const double hungerHours = fed ? 0.0 : (startingFullness > 0 ? 0.0 : priorHours) + newlyHungryHours;
  const double hungerStreak = std::floor(hungerHours / 24);

  Json unitHp = child(save, "unitHp").is_object() ? child(save, "unitHp") : Json::object();
  if (fed && elapsedDays > 0)
    for (const char* hero : kHeroes) {
      const int max = maxHp(save, hero);
      if (max <= 0) continue;
      const double current = num(unitHp, hero).value_or(max);
      if (current <= 0 || current >= max) continue;
      unitHp[hero] = number(std::min(static_cast<double>(max), jsRound(current + max * kRecoveryPct * elapsedDays)));
    }
  // Poison ticks on the road (never below 1 HP).
  for (const char* hero : kHeroes) {
    const std::string tier = poisonTierOf(child(child(save, "heroPoisons"), hero));
    if (tier.empty() || !ages(hero)) continue;
    const int max = maxHp(save, hero);
    const double current = num(unitHp, hero).value_or(max);
    if (current <= 0) continue;
    const Json& t = tables_["poisonTiers"][tier];
    const int dice = t["dice"].get<int>(), faces = t["faces"].get<int>();
    double dmg = 0;
    for (int i = 0; i < dice; ++i) dmg += 1 + std::floor(rng() * faces);
    std::string cls;
    const Json& promo = child(child(save, "promotions"), hero);
    cls = promo.is_string() ? promo.get<std::string>() : child(tables_["baseClass"], hero).get<std::string>();
    double resist = 0.0;
    resist = resist + classResistance(cls, "poison");
    resist = resist + gearResistance(save, hero, "poison");
    resist = resist + num(child(child(save, "heroSkills"), hero), "poisonResistance").value_or(0.0);
    const double mag = num(child(save, "heroPoisonMag"), hero).value_or(0.0);
    const double effective = std::max(-50.0, std::min(100.0, resist - t["penalty"].get<double>() - mag / 2));
    dmg = std::max(0.0, jsRound(dmg * (1 - effective / 100)));
    unitHp[hero] = number(std::max(1.0, current - dmg));
  }

  const double newPenalty = hungerPenaltyFor(hungerStreak);
  if (newPenalty > prevPenalty) {
    event = newPenalty >= kHungerPenaltyMax
                ? OverworldEvent{"hungry", "O grupo desmaia de fome. Preciso retornar a uma estalagem.", ""}
                : OverworldEvent{"hungry", std::format("Fome: o grupo já passa {} dias sem comer (−{}% nos atributos).", jsNumber(hungerStreak),
                                                       jsNumber(jsRound(newPenalty * 100))),
                                 ""};
  }

  double rationsDelta = 0, emberDelta = 0;
  Json weapons = child(save, "weapons"), looseEquipment = child(save, "looseEquipment"), bags = child(save, "bags");
  Json heroDiseases = child(save, "heroDiseases").is_object() ? child(save, "heroDiseases") : Json::object();
  std::vector<std::string> healthy;
  for (const char* hero : kHeroes)
    if (ages(hero) && !truthy(heroDiseases, hero)) healthy.push_back(hero);
  std::string diseaseText;
  if (elapsedDays > 0 && !healthy.empty() && rng() < 1 - std::pow(1 - kTravelDiseaseChance, elapsedDays)) {
    const std::string hero = healthy[static_cast<size_t>(std::floor(rng() * static_cast<double>(healthy.size())))];
    heroDiseases[hero] = true;
    diseaseText = hero + " contraiu uma doença na estrada (−10% nos atributos até ser curado).";
  }
  const MapLocation* landed = locationAt(to.col, to.row);
  const Json emptyList = Json::array();
  const Json& seenRoad = child(save, "roadEncountersSeen").is_array() ? child(save, "roadEncountersSeen") : emptyList;
  const bool merchantCanAppear = !has(completed, "thebridge") || arrivalTime == "brightNight" || arrivalTime == "darkNight";
  std::vector<std::string> travelIds;
  for (const std::string& id : cell(to.col, to.row)->encounters) {
    if (id == "random-encounter-11" || id == "random-encounter-14") {
      if (merchantCanAppear) travelIds.push_back(id);
    } else if (!has(completed, id) && !has(seenRoad, id)) {
      travelIds.push_back(id);
    }
  }
  const double saveAlert = num(save, "alertStreak").value_or(std::numeric_limits<double>::quiet_NaN());
  const double battleChance = saveAlert > 0 ? kBattleEncounterChance * 2 : kBattleEncounterChance;
  double alertStreak = std::isnan(saveAlert) ? saveAlert : std::max(0.0, saveAlert - elapsedDays);
  const Json& lastRoad = child(save, "lastRoadEncounterId");
  std::optional<Json> lastRoadEncounterId;
  if (save.contains("lastRoadEncounterId")) lastRoadEncounterId = lastRoad;
  Json roadEncountersSeen = seenRoad;
  if (!event && (!landed || landed->encountersAllowed) && !travelIds.empty() && rng() < battleChance) {
    std::vector<std::string> pool = travelIds;
    if (travelIds.size() > 1)
      std::erase_if(pool, [&](const std::string& id) { return lastRoad.is_string() && lastRoad.get<std::string>() == id; });
    const std::string missionId = pool[static_cast<size_t>(std::floor(rng() * static_cast<double>(pool.size())))];
    lastRoadEncounterId = missionId;
    if (!has(roadEncountersSeen, missionId)) roadEncountersSeen.push_back(missionId);
    event = OverworldEvent{"battle", "", missionId};
  }
  if (!event && !landed && rng() < kTextEncounterChance) {
    const RoadEncounter& pick = kEncounters[static_cast<size_t>(std::floor(rng() * static_cast<double>(std::size(kEncounters))))];
    if (pick.diseaseChance > 0 && rng() < pick.diseaseChance) {
      std::vector<std::string> still;
      for (const char* hero : kHeroes)
        if (ages(hero) && !truthy(heroDiseases, hero)) still.push_back(hero);
      if (!still.empty()) {
        const std::string hero = still[static_cast<size_t>(std::floor(rng() * static_cast<double>(still.size())))];
        heroDiseases[hero] = true;
        const std::string text = hero + " pegou um resfriado na chuva (−10% nos atributos até ser curado).";
        diseaseText = diseaseText.empty() ? text : diseaseText + " " + text;
      }
    }
    const Json& chest = tables_["chestLoot"];
    if (pick.lootBag) {
      // A small chest's roll: Ember, a weighted potion for the first hero with room, maybe gear, maybe rations.
      const double gain = chest["emberBase"].get<double>() + std::floor(rng() * chest["emberDice"].get<double>());
      emberDelta = gain;
      std::vector<std::string> found;
      std::string potionKind;
      {
        double total = 0;
        for (const Json& p : tables_["potions"]) total += p["weight"].get<double>();
        double roll = rng() * total;
        for (const Json& p : tables_["potions"]) {
          roll -= p["weight"].get<double>();
          if (roll <= 0) {
            potionKind = p["id"].get<std::string>();
            break;
          }
        }
        if (potionKind.empty()) potionKind = tables_["potions"].back()["id"].get<std::string>();
      }
      const Json* potion = nullptr;
      for (const Json& p : tables_["potions"])
        if (p["id"].get<std::string>() == potionKind) potion = &p;
      for (const char* hero : kHeroes) {
        if (!heroRecruited(hero, save)) continue;
        if (num(child(bags, hero), potionKind).value_or(0.0) >= (*potion)["carryMax"].get<double>()) continue;
        Json bag = child(bags, hero).is_object() ? child(bags, hero) : tables_["emptyBag"];
        bag[potionKind] = number(num(child(bags, hero), potionKind).value_or(0.0) + 1);
        bags[hero] = std::move(bag);
        found.push_back((*potion)["name"].get<std::string>());
        break;
      }
      if (rng() < chest["gearChance"].get<double>()) {
        // data.ts weightedLootPick(rng, MAX_LEVEL, owned weapons): rarer as the price climbs.
        std::set<std::string> owned;
        for (const auto& [id, level] : child(save, "weapons").items()) owned.insert(id);
        const int maxLevel = tables_.value("maxLevel", 30);
        std::vector<std::pair<const LootEntry*, double>> entries;
        for (const LootEntry& e : loot_)
          if (!e.starting && e.power <= maxLevel && !(e.weapon && owned.contains(e.id))) entries.push_back({&e, 1 / std::sqrt(std::max(1.0, e.price))});
        double total = 0;
        for (const auto& [e, w] : entries) total += w;
        double roll = rng() * total;
        const LootEntry* drop = entries.back().first;
        for (const auto& [e, w] : entries) {
          roll -= w;
          if (roll <= 0) {
            drop = e;
            break;
          }
        }
        Json probe = save;
        if (drop->weapon) {
          probe["weapons"] = weapons;
          probe["weapons"][drop->id] = 0;
        } else {
          probe["looseEquipment"] = looseEquipment;
          probe["looseEquipment"][drop->id] = number(num(looseEquipment, drop->id).value_or(0.0) + 1);
        }
        if (bagHasRoom(probe, 0)) {
          if (drop->weapon) weapons[drop->id] = 0;
          else looseEquipment[drop->id] = number(num(looseEquipment, drop->id).value_or(0.0) + 1);
          found.push_back(drop->name);
        }
      }
      if (rng() < 0.4) {
        const double qty = 1 + std::floor(rng() * 4);
        rationsDelta += qty;
        found.push_back("Rações ×" + jsNumber(qty));
      }
      std::string text = std::string(pick.text) + " +" + jsNumber(gain) + " Gold";
      if (!found.empty()) {
        text += " · achou ";
        for (size_t i = 0; i < found.size(); ++i) text += (i ? ", " : "") + found[i];
      }
      event = OverworldEvent{"encounter", text, ""};
    } else {
      const double rationsLost = pick.rationsDice ? 1 + std::floor(rng() * pick.rationsDice) : 0;
      rationsDelta = -rationsLost;
      const double goldLost = pick.goldLossDice ? std::min(num(save, "ember").value_or(0.0), 1 + std::floor(rng() * pick.goldLossDice)) : 0;
      emberDelta = pick.ember - goldLost;
      std::string lostItem;
      if (pick.losePotion) {
        std::vector<std::pair<std::string, std::string>> carried;  // hero, kind: bags in key order, POTIONS order within
        for (const auto& [hero, bag] : bags.items())
          for (const Json& kind : tables_["potionOrder"])
            if (num(bag, kind.get<std::string>()).value_or(0.0) > 0) carried.push_back({hero, kind.get<std::string>()});
        const double r = rng();
        if (!carried.empty()) {
          const auto& [hero, kind] = carried[static_cast<size_t>(std::floor(r * static_cast<double>(carried.size())))];
          bags[hero][kind] = number(std::max(0.0, num(bags[hero], kind).value_or(0.0) - 1));
          for (const Json& p : tables_["potions"])
            if (p["id"].get<std::string>() == kind) lostItem = "−1 " + p["name"].get<std::string>();
        }
      }
      if (pick.loseLockpick) {
        std::vector<std::string> holders;
        for (const auto& [hero, bag] : bags.items())
          if (num(bag, "lockpick").value_or(0.0) > 0) holders.push_back(hero);
        const double r = rng();
        if (!holders.empty()) {
          const std::string hero = holders[static_cast<size_t>(std::floor(r * static_cast<double>(holders.size())))];
          bags[hero]["lockpick"] = number(std::max(0.0, num(bags[hero], "lockpick").value_or(0.0) - 1));
          lostItem = "−1 Gazua";
        }
      }
      if (pick.alertDays) alertStreak = pick.alertDays;
      std::vector<std::string> parts{pick.text};
      if (rationsLost > 0) parts.push_back("−" + jsNumber(rationsLost) + (rationsLost == 1 ? " ração" : " rações"));
      if (goldLost > 0) parts.push_back("−" + jsNumber(goldLost) + " Gold");
      if (!lostItem.empty()) parts.push_back(lostItem);
      if (pick.alertDays) parts.push_back("chance de emboscada dobrada por " + std::to_string(pick.alertDays) + " dias");
      std::string text;
      for (size_t i = 0; i < parts.size(); ++i) text += (i ? " · " : "") + parts[i];
      event = OverworldEvent{"encounter", text, ""};
    }
  }
  if (!diseaseText.empty() && (!event || event->kind != "battle")) {
    if (event) event->text += " · " + diseaseText;
    else event = OverworldEvent{"encounter", diseaseText, ""};
  }

  Json exploredHexes = child(save, "exploredHexes").is_array() ? child(save, "exploredHexes") : Json::array();
  if (!has(exploredHexes, key(to.col, to.row))) exploredHexes.push_back(key(to.col, to.row));
  // skills.ts advanceTravelTraining: each travelling hero with a chosen skill banks the step's hours, +0.1 per 12 h.
  Json heroSkills = child(save, "heroSkills").is_object() ? child(save, "heroSkills") : Json::object();
  Json trainingHours = child(save, "travelTrainingHours").is_object() ? child(save, "travelTrainingHours") : Json::object();
  const double stepHours = tables_.value("travelTrainingHours", 12.0), gain = tables_.value("skillGain", 0.1), cap = tables_.value("skillCap", 100.0);
  for (const std::string& hero : travelling) {
    const Json& id = child(child(save, "travelTraining"), hero);
    if (!id.is_string() || id.get<std::string>().empty()) continue;
    const double total = num(trainingHours, hero).value_or(0.0) + std::max(0, travelHours);
    const double gains = std::floor(total / stepHours);
    trainingHours[hero] = number(total - gains * stepHours);
    if (gains > 0) {
      const double next = std::min(cap, jsRound((num(child(heroSkills, hero), id.get<std::string>()).value_or(0.0) + gains * gain) * 10) / 10);
      Json skills = child(heroSkills, hero).is_object() ? child(heroSkills, hero) : Json::object();
      skills[id.get<std::string>()] = number(next);
      heroSkills[hero] = std::move(skills);
    }
  }

  Json out = save;
  out["overworldPos"] = Json{{"col", to.col}, {"row", to.row}};
  out["exploredHexes"] = std::move(exploredHexes);
  out["gameClock"] = number(num(save, "gameClock").value_or(0.0) + elapsedDays);
  out["gameHour"] = totalHours % 24;
  out["overworldMoveBudgetUsed"] = number(num(save, "overworldMoveBudgetUsed").value_or(0.0) + 1);
  out["heroHunger"] = std::move(heroHunger);
  out["heroDiseases"] = std::move(heroDiseases);
  out["rations"] = number(std::max(0.0, rations + rationsDelta));
  out["ember"] = number(std::max(0.0, num(save, "ember").value_or(0.0) + emberDelta));
  out["hungerStreak"] = number(hungerStreak);
  out["hungerHours"] = number(hungerHours);
  out["alertStreak"] = std::isnan(alertStreak) ? Json() : number(alertStreak);
  if (lastRoadEncounterId) out["lastRoadEncounterId"] = *lastRoadEncounterId;
  out["roadEncountersSeen"] = std::move(roadEncountersSeen);
  out["unitHp"] = std::move(unitHp);
  if (!weapons.is_null()) out["weapons"] = std::move(weapons);
  if (!looseEquipment.is_null()) out["looseEquipment"] = std::move(looseEquipment);
  if (!bags.is_null()) out["bags"] = std::move(bags);
  out["spellUses"] = std::move(spellUses);
  out["heroSkills"] = std::move(heroSkills);
  out["travelTrainingHours"] = std::move(trainingHours);
  return out;
}

// --- progression (progression.ts, quests.ts) and access (access.ts) -----------------------------------------------

int Overworld::questStage(const std::string& questId, const Json& save) const {
  for (const Json& q : tables_["quests"]) {
    if (q.value("id", "") != questId) continue;
    if (has(child(save, "questsDone"), questId)) return 3;
    if (!has(child(save, "questsActive"), questId)) return 0;
    int haveCount = 0, total = 0;
    const std::string kind = q.value("kind", "");
    if (kind == "kill") total = 1, haveCount = has(child(save, "questKills"), q.value("targetName", "")) ? 1 : 0;
    else if (kind == "recruit") total = 1, haveCount = has(child(save, "flags"), "recruited:" + q.value("targetName", "")) ? 1 : 0;
    else
      for (const Json& p : q.value("pickups", Json::array())) {
        ++total;
        if (has(child(save, "questItems"), questId + ":" + p.value("id", ""))) ++haveCount;
      }
    return haveCount >= total ? 2 : 1;
  }
  return 0;
}

bool Overworld::evaluate(const Json& c, const Json& save, bool storedChapter) const {
  if (c.is_null()) return true;
  if (c.contains("all")) {
    for (const Json& x : c["all"])
      if (!evaluate(x, save, storedChapter)) return false;
    return true;
  }
  if (c.contains("any")) {
    for (const Json& x : c["any"])
      if (evaluate(x, save, storedChapter)) return true;
    return false;
  }
  if (c.contains("not")) return !evaluate(c["not"], save, storedChapter);
  if (c.contains("chapterReached")) {
    const int chapter = storedChapter ? std::max(1, static_cast<int>(num(save, "chapter").value_or(1))) : currentChapter(save);
    return chapter >= c["chapterReached"].get<int>();
  }
  if (c.contains("missionCompleted")) return has(child(save, "completed"), c["missionCompleted"].get<std::string>());
  if (c.contains("npcTalkedTo")) return has(child(save, "npcTalked"), c["npcTalkedTo"].get<std::string>());
  if (c.contains("flagSet")) return has(child(save, "flags"), c["flagSet"].get<std::string>());
  if (c.contains("itemOwned")) {
    const std::string id = c["itemOwned"].get<std::string>();
    if (child(save, "weapons").contains(id) || child(save, "looseEquipment").contains(id)) return true;
    for (const auto& [hero, slots] : child(save, "equipment").items())
      for (const auto& [slot, item] : slots.items())
        if (item.is_string() && item.get<std::string>() == id) return true;
    return false;
  }
  if (c.contains("partyMemberPresent")) return heroRecruited(c["partyMemberPresent"].get<std::string>(), save);
  if (c.contains("questDiscovered")) {
    const std::string id = c["questDiscovered"].get<std::string>();
    return questStage(id, save) >= 1 || has(child(save, "questsDiscovered"), id);
  }
  if (c.contains("questActive")) {
    const int stage = questStage(c["questActive"].get<std::string>(), save);
    return stage == 1 || stage == 2;
  }
  if (c.contains("questStage")) return questStage(c["questStage"][0].get<std::string>(), save) >= c["questStage"][1].get<int>();
  if (c.contains("questCompleted")) return questStage(c["questCompleted"].get<std::string>(), save) >= 3;
  return false;
}

int Overworld::currentChapter(const Json& save) const {
  int chapter = std::max(1, static_cast<int>(num(save, "chapter").value_or(1)));
  for (const Json& def : tables_["progression"].value("chapters", Json::array())) {
    const int id = def.value("id", 0);
    if (id > chapter && def.contains("reachedWhen") && !def["reachedWhen"].is_null() && evaluate(def["reachedWhen"], save, true)) chapter = id;
  }
  return chapter;
}

std::string Overworld::missionAccess(const std::string& id, const Json& save) const {
  if (has(child(save, "completed"), id)) return "completed";
  const Json& gates = tables_["progression"]["missionGates"];
  if (!gates.contains(id)) return "available";
  const Json& gate = gates[id];
  if (!evaluate(child(gate, "visible"), save, false)) return "hidden";
  if (!evaluate(child(gate, "unlock"), save, false)) return "revealed";
  return "available";
}

const MapLocation* Overworld::previousPopulatedLocation(const MapLocation& location) const {
  const auto at = std::find_if(locations_.begin(), locations_.end(), [&](const MapLocation& l) { return l.id == location.id; });
  for (auto it = at; it != locations_.begin();) {
    --it;
    for (const MapMission* m : missionsOf(*it))
      if (!m->gated) return &*it;
  }
  return nullptr;
}

bool Overworld::lockedMission(const std::string& id, const Json& save, const std::vector<std::string>& order) const {
  const Json& completed = child(save, "completed");
  if (has(completed, id)) return true;
  if (const MapMission* m = mission(id); m && m->gated) return missionAccess(id, save) != "available";
  const MapLocation* location = nullptr;
  for (const MapLocation& l : locations_)
    if (std::find(l.missionIds.begin(), l.missionIds.end(), id) != l.missionIds.end()) {
      location = &l;
      break;
    }
  if (location && location->openAccess) return false;
  if (!location) {
    const auto at = std::find(order.begin(), order.end(), id);
    if (at == order.end()) return true;
    for (auto it = order.begin(); it != at; ++it)
      if (!has(completed, *it)) return true;
    return false;
  }
  std::vector<std::string> ids;
  for (const MapMission* m : missionsOf(*location))
    if (!m->gated) ids.push_back(m->id);
  const auto at = std::find(ids.begin(), ids.end(), id);
  if (at == ids.end()) return true;
  if (at != ids.begin()) {
    for (auto it = ids.begin(); it != at; ++it)
      if (!has(completed, *it)) return true;
    return false;
  }
  const MapLocation* previous = previousPopulatedLocation(*location);
  if (!previous) return false;
  for (const MapMission* m : missionsOf(*previous))
    if (!m->gated && !has(completed, m->id)) return true;
  return false;
}

std::string Overworld::missionStatus(const std::string& id, const Json& save, const std::vector<std::string>& order) const {
  if (has(child(save, "completed"), id)) return "done";
  return lockedMission(id, save, order) ? "locked" : "available";
}

std::vector<const MapMission*> Overworld::visibleMissionsOf(const MapLocation& location, const Json& save) const {
  std::vector<const MapMission*> out;
  for (const MapMission* m : missionsOf(location))
    if (missionAccess(m->id, save) != "hidden") out.push_back(m);
  return out;
}

bool Overworld::locationVisible(const MapLocation& location, const Json& save) const {
  for (const std::string& id : location.missionIds)
    if (missionAccess(id, save) != "hidden") return true;
  return false;
}

std::string Overworld::locationStatus(const MapLocation& location, const Json& save) const {
  const std::vector<const MapMission*> missions = visibleMissionsOf(location, save);
  std::vector<const MapMission*> pending;
  for (const MapMission* m : missions)
    if (!has(child(save, "completed"), m->id)) pending.push_back(m);
  if (pending.empty()) return missions.empty() ? "locked" : "done";
  std::vector<std::string> order;
  for (const MapMission* m : missionsOf(location)) order.push_back(m->id);
  for (const MapMission* m : pending)
    if (missionStatus(m->id, save, order) != "locked") return "available";
  return "locked";
}

// --- the check against Engine2 -------------------------------------------------------------------------------------

namespace {

// Deep equality as JSON.stringify would see it (key order ignored, numbers compared by value).
bool same(const Json& a, const Json& b) {
  if (a.is_number() && b.is_number()) return a.get<double>() == b.get<double>();
  if (a.type() != b.type()) return false;
  if (a.is_object()) {
    if (a.size() != b.size()) return false;
    for (const auto& [k, v] : a.items())
      if (!b.contains(k) || !same(v, b[k])) return false;
    return true;
  }
  if (a.is_array()) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
      if (!same(a[i], b[i])) return false;
    return true;
  }
  return a == b;
}

std::string firstDifference(const Json& a, const Json& b, const std::string& path = "") {
  if (a.is_object() && b.is_object()) {
    for (const auto& [k, v] : a.items()) {
      if (!b.contains(k)) return path + "." + k + " (only in C++)";
      if (!same(v, b[k])) return firstDifference(v, b[k], path + "." + k);
    }
    for (const auto& [k, v] : b.items())
      if (!a.contains(k)) return path + "." + k + " (only in Engine2)";
  }
  return path + ": C++ " + a.dump() + " vs Engine2 " + b.dump();
}

}  // namespace

int checkOverworld(const Overworld& world, const std::filesystem::path& references) {
  std::ifstream in(references, std::ios::binary);
  const Json refs = Json::parse(in, nullptr, false);
  if (refs.is_discarded()) {
    logError("overworld references unreadable: {}", references.string());
    return 1;
  }
  int differences = 0;
  auto report = [&](const std::string& what) {
    if (++differences <= 20) logError("overworld differs: {}", what);
  };
  // Location and mission status.
  int statusChecks = 0;
  for (const Json& r : refs["locations"]) {
    const Json& save = r["save"];
    if (world.currentChapter(save) != r["chapter"].get<int>()) report("chapter");
    for (const Json& l : r["locations"]) {
      const MapLocation* loc = nullptr;
      for (const MapLocation& x : world.locations())
        if (x.id == l["id"].get<std::string>()) loc = &x;
      if (!loc) {
        report("location " + l["id"].get<std::string>() + " missing");
        continue;
      }
      ++statusChecks;
      if (world.locationStatus(*loc, save) != l["status"].get<std::string>())
        report(std::format("location {} status: C++ {} vs Engine2 {}", loc->id, world.locationStatus(*loc, save), l["status"].get<std::string>()));
      if (world.locationVisible(*loc, save) != l["visible"].get<bool>()) report("location " + loc->id + " visibility");
      std::vector<std::string> order;
      for (const MapMission* m : world.missionsOf(*loc)) order.push_back(m->id);
      const std::vector<const MapMission*> visible = world.visibleMissionsOf(*loc, save);
      if (visible.size() != l["missions"].size()) report("location " + loc->id + " visible missions");
      for (const Json& m : l["missions"]) {
        const std::string id = m["id"].get<std::string>();
        if (world.missionAccess(id, save) != m["access"].get<std::string>()) report("mission " + id + " access");
        if (world.missionStatus(id, save, order) != m["status"].get<std::string>()) report("mission " + id + " status");
      }
    }
  }
  // Allowed steps.
  int stepPairs = 0;
  for (const Json& r : refs["canStep"]) {
    std::set<std::string> expected;
    for (const Json& a : r["allowed"]) expected.insert(a.get<std::string>());
    for (const OverworldCell& c : world.cells())
      for (const OverworldHex& n : Overworld::neighbors(c.col, c.row)) {
        const std::string k = std::format("{},{}>{},{}", c.col, c.row, n.col, n.row);
        ++stepPairs;
        if (world.canStep(r["save"], {c.col, c.row}, n) != expected.contains(k)) report("canStep " + k);
      }
  }
  // Seeded steps: the save afterwards, the event, and how many random numbers were drawn.
  int steps = 0, events = 0;
  for (const Json& r : refs["steps"]) {
    ++steps;
    Mulberry32 base(r["seed"].get<uint32_t>());
    int calls = 0;
    Mulberry32 rng(r["seed"].get<uint32_t>());
    std::optional<OverworldEvent> event;
    const Json after = world.step(r["save"], {r["to"]["col"].get<int>(), r["to"]["row"].get<int>()}, rng, event);
    // How many draws the port made: step a second generator until it matches the first one's next value.
    {
      const double next = rng();
      for (calls = 0; calls < 64 && base() != next; ++calls) {
      }
    }
    if (calls != r["calls"].get<int>()) report(std::format("step {}: {} random numbers vs Engine2 {}", steps, calls, r["calls"].get<int>()));
    if (!same(after, r["after"])) report(std::format("step {}: save {}", steps, firstDifference(after, r["after"])));
    const Json& e = r["event"];
    if (e.is_null() != !event.has_value()) {
      report(std::format("step {}: event {} vs Engine2 {}", steps, event ? event->text : "none", e.dump()));
    } else if (event) {
      ++events;
      if (event->kind != e.value("kind", "") || event->text != e.value("text", "") || event->missionId != e.value("missionId", ""))
        report(std::format("step {}: event '{}' vs Engine2 '{}'", steps, event->text, e.value("text", "")));
    }
  }
  if (!differences)
    logInfo("overworld matches Engine2 ({} location checks, {} step pairs, {} seeded steps with {} events)", statusChecks, stepPairs, steps, events);
  else
    logError("overworld: {} differences", differences);
  return differences;
}

}  // namespace ember::e2
