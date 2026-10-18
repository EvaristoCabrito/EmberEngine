#include "world/Progression.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
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
  std::map<std::string, std::string> heroOfClass;   // a hero's classes -> the hero
  std::array<double, 5> enemyScale{1, 1, 1, 1, 1};  // the party's new gains / old gains, stat by stat
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
    std::array<double, 5> oldSum{}, newSum{};
    if (p.contains("heroes"))
      for (const auto& [hero, h] : p["heroes"].items()) {
        Row gain{};
        for (size_t k = 0; k < 5; ++k) gain[k] = h["gain"][k].get<int>();
        t.heroGain[hero] = gain;
        const json& classes = h["classes"];
        for (const json& c : classes) t.heroOfClass[c.get<std::string>()] = hero;
        const auto own = classes.empty() ? t.classes.end() : t.classes.find(classes[0].get<std::string>());
        if (own == t.classes.end() || own->second.size() < 2) {
          logError("progression: {}'s class is not in content/engine2/class-stats.json", hero);
          continue;
        }
        for (size_t k = 0; k < 5; ++k) oldSum[k] += own->second[1][k] - own->second[0][k], newSum[k] += gain[k];
      }
    for (size_t k = 0; k < 5; ++k) t.enemyScale[k] = oldSum[k] > 0 ? newSum[k] / oldSum[k] : 1.0;
    logInfo("progression: {} heroes; enemies' gains x hp {:.3f} atk {:.3f} mag {:.3f} def {:.3f} dex {:.3f}", t.heroGain.size(),
            t.enemyScale[0], t.enemyScale[1], t.enemyScale[2], t.enemyScale[3], t.enemyScale[4]);
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
Row enemyAt(const std::vector<Row>& rows, int level) {
  const Row& base = rows[0];
  const Row& next = rows.size() > 1 ? rows[1] : rows[0];
  Row r{};
  for (size_t k = 0; k < 5; ++k)
    r[k] = base[k] + static_cast<int>(std::lround((level - 1) * (next[k] - base[k]) * tables().enemyScale[k]));
  return r;
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
    const std::vector<Row>* rows = classRows(u.classId);
    if (!rows) return;
    const int level = clampLevel(*rows, u.level);
    from = (*rows)[static_cast<size_t>(level - 1)];
    to = enemyAt(*rows, level);
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
