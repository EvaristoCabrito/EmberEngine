#include "world/Progression.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <random>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "world/Battle.h"

namespace ember::e2 {

namespace {

using json = nlohmann::json;
using Row = std::array<int, 5>;  // hp atk mag def dex

bool gEnabled = true;

struct Tables {
  std::map<std::string, std::vector<Row>> classes;  // Engine2 statsFor per level 1..30
  std::map<std::string, Row> heroGain;              // by hero name
  std::map<std::string, Row> heroBase;              // a hero's own level-1 extra over her class's row (Selene: +3 ATK)
  std::map<std::string, std::string> heroOfClass;   // a hero's classes -> the hero
  std::map<std::string, std::string> heroClass;     // the hero -> its first class (its level-1 MAG)
};

json readJson(const char* path) {
  std::ifstream in(path, std::ios::binary);
  json j = in ? json::parse(in, nullptr, false) : json();
  if (!in || j.is_discarded()) {
    logError("progression: cannot read {}", path);
    return json::object();
  }
  return j;
}

const Tables& tables() {
  static const Tables t = [] {
    Tables t;
    const json cs = readJson("content/engine2/class-stats.json");
    if (cs.contains("classes"))
      for (const auto& [id, rows] : cs["classes"].items()) {
        std::vector<Row>& v = t.classes[id];
        for (const json& r : rows) v.push_back({r[0].get<int>(), r[1].get<int>(), r[2].get<int>(), r[3].get<int>(), r[4].get<int>()});
      }
    const json p = readJson("content/progression.json");
    if (p.contains("heroes"))
      for (const auto& [hero, h] : p["heroes"].items()) {
        Row gain{};
        for (size_t k = 0; k < 5; ++k) gain[k] = h["gain"][k].get<int>();
        t.heroGain[hero] = gain;
        if (h.contains("baseBonus") && h["baseBonus"].is_array() && h["baseBonus"].size() == 5) {
          Row extra{};
          for (size_t k = 0; k < 5; ++k) extra[k] = h["baseBonus"][k].get<int>();
          t.heroBase[hero] = extra;
        }
        const json& classes = h["classes"];
        // a hero who shares her class with another (Selene, a Conjuradora like Malrec) never stands for that class
        if (!h.value("sharesClass", false))
          for (const json& c : classes) t.heroOfClass[c.get<std::string>()] = hero;
        if (classes.empty() || !t.classes.count(classes[0].get<std::string>()))
          logError("progression: {}'s class is not in content/engine2/class-stats.json", hero);
        else
          t.heroClass[hero] = classes[0].get<std::string>();
      }
    logInfo("progression: {} heroes; enemies keep their class's own gains", t.heroGain.size());
    return t;
  }();
  return t;
}

const std::vector<Row>* classRows(const std::string& classId) {
  const auto it = tables().classes.find(classId);
  return it != tables().classes.end() && !it->second.empty() ? &it->second : nullptr;
}

int clampLevel(const std::vector<Row>& rows, int level) { return std::clamp(level, 1, static_cast<int>(rows.size())); }

// The progression's stats at a level, from the class's level-1 stats.
Row heroAt(const Row& base, const Row& gain, int level) {
  Row r{};
  for (size_t k = 0; k < 5; ++k) r[k] = base[k] + (level - 1) * gain[k];
  return r;
}

// Enemy ranks (ours, content/enemy-ranks.json; docs/ENEMY_RANKS.md).
struct Ranks {
  std::map<std::string, double> perLevel;
  std::map<std::string, std::string> classes;
  std::string fallback = "F";
  double cap = 100;
};

const Ranks& ranks() {
  static const Ranks r = [] {
    Ranks r;
    const json j = readJson("content/enemy-ranks.json");
    if (j.contains("perLevel") && j["perLevel"].is_object())
      for (const auto& [rank, v] : j["perLevel"].items())
        if (v.is_number()) r.perLevel[rank] = v.get<double>();
    if (j.contains("classes") && j["classes"].is_object())
      for (const auto& [id, rank] : j["classes"].items())
        if (rank.is_string()) r.classes[id] = rank.get<std::string>();
    if (j.contains("default") && j["default"].is_string()) r.fallback = j["default"].get<std::string>();
    if (j.contains("cap") && j["cap"].is_number()) r.cap = j["cap"].get<double>();
    logInfo("enemy ranks: {} ranks, {} classes ranked (others {})", r.perLevel.size(), r.classes.size(), r.fallback);
    return r;
  }();
  return r;
}

// The unit's rank and its skills at its level: the weapon's (accuracy 75 + skill and dice x (100 + skill) / 100, a hero's
// mastery), Esquiva, and every resistance.
void applyRank(BattleUnit& u) {
  const Ranks& r = ranks();
  const auto own = r.classes.find(u.classId);
  u.rank = own != r.classes.end() ? own->second : r.fallback;
  const auto rate = r.perLevel.find(u.rank);
  if (rate == r.perLevel.end()) {
    logWarn("enemy ranks: {} ({}) has rank {}, which has no perLevel entry", u.name, u.classId, u.rank);
    return;
  }
  const double skill = std::min(r.cap, std::round(rate->second * std::max(1, u.level) * 100) / 100);
  u.weaponSkill = skill;
  u.combat.accuracy = 75 + skill, u.combat.damageMul = (100 + skill) / 100;
  if (u.combat.offHandWeapon) u.offWeaponSkill = skill, u.combat.offAccuracy = 75 + skill, u.combat.offDamageMul = (100 + skill) / 100;
  bool dodge = false;
  for (auto& [id, v] : u.skills)
    if (id == "dodging") v = skill, dodge = true;
  if (!dodge) u.skills.emplace_back("dodging", skill);
  static const char* kElements[] = {"fire", "lightning", "ice", "arcane", "darkness", "holy", "poison", "ember"};
  for (const char* e : kElements) {
    bool found = false;
    for (auto& [id, v] : u.combat.resistances)
      if (id == e) v += skill, found = true;
    if (!found) u.combat.resistances.emplace_back(e, skill);
  }
}

// An enemy's attribute points (the user, 2026-10-19): kPointsPerLevel per level after the first on top of its class's own
// gains, +1 to the stat per point as a hero's, rolled for each unit when its battle loads (two units of one class differ).
// The split follows what the unit uses: each stat's weight = the class's own gain in it over the average class's gain in
// that stat (so HP's large raw gains do not swallow the points), the stat it strikes with x kStrikeWeight, ATK nothing for
// a unit that strikes with MAG; each unit's weights are then scaled by its own temper (x 0.5 .. 1.5 each), and every point
// is drawn by weight. A class that gains nothing puts them all into the stat it strikes with (MAG when it has any, else ATK).
constexpr int kPointsPerLevel = 3;
constexpr double kStrikeWeight = 1.5;

// The average class's gain per level in each stat (every class with growth rows).
const std::array<double, 5>& averageGain() {
  static const std::array<double, 5> avg = [] {
    std::array<double, 5> sum{};
    int n = 0;
    for (const auto& [id, rows] : tables().classes)
      if (rows.size() > 1) {
        for (size_t k = 0; k < 5; ++k) sum[k] += std::max(0, rows[1][k] - rows[0][k]);
        ++n;
      }
    for (double& s : sum) s = n > 0 && s > 0 ? s / n : 1.0;
    return sum;
  }();
  return avg;
}

Row enemyPoints(const BattleUnit& u) {
  Row out{};
  const int points = kPointsPerLevel * std::max(0, u.level - 1);
  if (points == 0) return out;
  const size_t strike = u.mag > 0 && !u.atkBlows ? 2 : 1;
  std::array<double, 5> weight{};
  if (const std::vector<Row>* rows = classRows(u.classId); rows && rows->size() > 1)
    for (size_t k = 0; k < 5; ++k) weight[k] = std::max(0, (*rows)[1][k] - (*rows)[0][k]) / averageGain()[k];
  if (strike == 2) weight[1] = 0;
  weight[strike] *= kStrikeWeight;
  double sum = 0;
  for (double w : weight) sum += w;
  if (sum <= 0) {
    out[strike] = points;
    return out;
  }
  thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<double> temper(0.5, 1.5);
  for (double& w : weight) w *= temper(rng);
  std::discrete_distribution<size_t> pick(weight.begin(), weight.end());
  for (int i = 0; i < points; ++i) ++out[pick(rng)];
  return out;
}

}  // namespace

void setProgressionEnabled(bool on) { gEnabled = on; }
bool progressionEnabled() { return gEnabled; }

void applyProgression(BattleUnit& u) {
  if (!gEnabled || u.summoned) return;
  const Tables& t = tables();
  Row from{}, to{};
  if (u.side == "player") {
    const auto gain = t.heroGain.find(u.name);
    if (gain == t.heroGain.end()) return;
    Row base{};
    if (!u.growth.empty()) {
      for (size_t k = 0; k < 5; ++k) base[k] = u.growth[0][k];
    } else if (const std::vector<Row>* rows = classRows(u.classId)) {
      base = (*rows)[0];
    } else {
      return;
    }
    // The level-1 MAG is the class table's own (its MAG column is ours, the heroes' starts included).
    if (const auto own = t.heroClass.find(u.name); own != t.heroClass.end())
      if (const std::vector<Row>* rows = classRows(own->second)) base[2] = (*rows)[0][2];
    // ... plus the hero's own level-1 extra (content/progression.json baseBonus; Selene's +3 ATK)
    if (const auto extra = t.heroBase.find(u.name); extra != t.heroBase.end())
      for (size_t k = 0; k < 5; ++k) base[k] += extra->second[k];
    // Engine2's stats at the unit's level (its growth rows, else its class), then the hero's own rows.
    const int level = std::max(1, u.level);
    if (!u.growth.empty() && static_cast<size_t>(level) <= u.growth.size()) {
      for (size_t k = 0; k < 5; ++k) from[k] = u.growth[static_cast<size_t>(level - 1)][k];
    } else if (const std::vector<Row>* rows = classRows(u.classId)) {
      from = (*rows)[static_cast<size_t>(clampLevel(*rows, level) - 1)];
    } else {
      return;
    }
    to = heroAt(base, gain->second, level);
    for (size_t i = 0; i < u.growth.size(); ++i) {
      const Row r = heroAt(base, gain->second, static_cast<int>(i) + 1);
      for (size_t k = 0; k < 5; ++k) u.growth[i][k] = r[k];
    }
  } else {
    applyRank(u);  // every class, also one without Engine2 growth rows
    to = enemyPoints(u);  // the class's own stats + the level's points (from stays 0)
    // The MAG column is ours (the battle file carries Engine2's): the class table's MAG at the unit's level.
    if (const std::vector<Row>* rows = classRows(u.classId)) to[2] += (*rows)[static_cast<size_t>(clampLevel(*rows, u.level) - 1)][2] - u.mag;
  }
  const int dHp = to[0] - from[0], dAtk = to[1] - from[1], dMag = to[2] - from[2], dDef = to[3] - from[3], dDex = to[4] - from[4];
  const int oldMax = u.maxHp;
  u.maxHp = std::max(1, u.maxHp + dHp);
  if (u.hp >= oldMax) u.hp = u.maxHp;
  else if (u.hp > 0) u.hp = std::clamp(u.hp + dHp, 1, u.maxHp);
  u.atk += dAtk, u.mag += dMag, u.def += dDef, u.dex += dDex;
  if (u.diseaseBase) u.diseaseBase->atk += dAtk, u.diseaseBase->mag += dMag, u.diseaseBase->def += dDef, u.diseaseBase->dex += dDex;
}

int progressionClassHp(const std::string& classId, int level, int engine2Hp) {
  if (!gEnabled) return engine2Hp;
  const Tables& t = tables();
  const auto hero = t.heroOfClass.find(classId);
  const std::vector<Row>* rows = classRows(classId);
  if (hero == t.heroOfClass.end() || !rows) return engine2Hp;
  return heroAt((*rows)[0], t.heroGain.at(hero->second), clampLevel(*rows, level))[0];
}

}  // namespace ember::e2
