#include "world/Quests.h"

#include <algorithm>
#include <format>
#include <fstream>

#include "world/Spells.h"  // combatLogEnglish

namespace ember {

namespace {

const Json& english() {
  static const Json table = [] {
    std::ifstream in("content/data/quests-en.json", std::ios::binary);
    Json j = Json::parse(in, nullptr, false);
    return j.is_object() ? j : Json::object();
  }();
  return table;
}

std::string englishAt(const char* section, const std::string& id, const char* field, const std::string& portuguese) {
  const Json& t = english();
  if (!t.contains(section) || !t[section].is_object() || !t[section].contains(id)) return portuguese;
  const Json& e = t[section][id];
  if (!field) return e.is_string() ? e.get<std::string>() : portuguese;
  return e.is_object() && e.contains(field) && e[field].is_string() ? e[field].get<std::string>() : portuguese;
}

bool listHas(const Json& save, const char* key, const std::string& v) {
  if (!save.is_object() || !save.contains(key) || !save[key].is_array()) return false;
  for (const Json& x : save[key])
    if (x.is_string() && x.get<std::string>() == v) return true;
  return false;
}

}  // namespace

void questProgress(const Json& quest, const Json& save, int& have, int& total) {
  have = 0, total = 0;
  const std::string kind = quest.value("kind", ""), id = quest.value("id", "");
  if (kind == "kill") total = 1, have = listHas(save, "questKills", quest.value("targetName", "")) ? 1 : 0;
  else if (kind == "recruit") total = 1, have = listHas(save, "flags", "recruited:" + quest.value("targetName", "")) ? 1 : 0;
  else if (quest.contains("pickups") && quest["pickups"].is_array())
    for (const Json& p : quest["pickups"]) ++total, have += listHas(save, "questItems", id + ":" + p.value("id", "")) ? 1 : 0;
}

std::string questStatus(const Json& quest, const Json& save) {
  const std::string id = quest.value("id", "");
  if (listHas(save, "questsDone", id)) return "done";
  if (!listHas(save, "questsActive", id)) return "available";
  int have = 0, total = 0;
  questProgress(quest, save, have, total);
  return have >= total ? "ready" : "active";
}

std::vector<QuestPickup> activeQuestPickups(const Json& quests, const Json& save, const std::string& missionId) {
  std::vector<QuestPickup> out;
  if (!quests.is_array() || missionId.empty()) return out;
  for (const Json& q : quests) {
    const std::string id = q.value("id", "");
    if (q.value("kind", "") != "fetch" || q.value("missionId", "") != missionId || !listHas(save, "questsActive", id) || listHas(save, "questsDone", id)) continue;
    if (!q.contains("pickups") || !q["pickups"].is_array()) continue;
    for (const Json& p : q["pickups"]) {
      const std::string key = id + ":" + p.value("id", "");  // pickupKey
      if (!listHas(save, "questItems", key)) out.push_back({key, p.value("name", ""), p.value("x", 0), p.value("y", 0)});
    }
  }
  return out;
}

std::vector<std::string> questKillTargets(const Json& quests, const std::string& missionId) {
  std::vector<std::string> out;
  if (!quests.is_array() || missionId.empty()) return out;
  for (const Json& q : quests)
    if (q.value("kind", "") == "kill" && q.value("missionId", "") == missionId && !q.value("targetName", "").empty()) out.push_back(q.value("targetName", ""));
  return out;
}

void foldQuestProgress(Json& save, const std::vector<std::string>& found, const std::vector<std::string>& killed) {
  auto add = [&](const char* key, const std::vector<std::string>& values) {
    if (values.empty()) return;
    Json& list = save[key];
    if (!list.is_array()) list = Json::array();
    for (const std::string& v : values)
      if (std::find(list.begin(), list.end(), v) == list.end()) list.push_back(v);
  };
  add("questItems", found);
  add("questKills", killed);
}

std::string questEnglish(const std::string& questId, const char* field, const std::string& portuguese) {
  return englishAt("quests", questId, field, portuguese);
}

std::string potionEnglish(const std::string& potionId, const std::string& portuguese) { return englishAt("potions", potionId, nullptr, portuguese); }

// engine.ts collectQuestPickups (from evaluateEnd, after every walk): a living hero standing on a pickup's hex picks it up.
std::vector<QuestPickup> Battle::collectQuestPickups(const std::function<bool(size_t)>& settled) {
  std::vector<QuestPickup> found;
  if (questPickups_.empty()) return found;
  for (const QuestPickup& pickup : std::vector<QuestPickup>(questPickups_)) {
    size_t finder = units_.size();
    for (size_t i = 0; i < units_.size(); ++i) {
      const BattleUnit& u = units_[i];
      if (u.side == "player" && u.alive && u.x == pickup.x && u.y == pickup.y && (!settled || settled(i))) {
        finder = i;
        break;
      }
    }
    if (finder == units_.size()) continue;
    std::erase_if(questPickups_, [&](const QuestPickup& p) { return p.key == pickup.key; });
    questFound_.push_back(pickup.key);
    const std::string& who = units_[finder].name;
    const std::string questId = pickup.key.substr(0, pickup.key.find(':'));
    pushLog(combatLogEnglish() ? std::format("{} found: {}.", who, questEnglish(questId, "pickup", pickup.name))
                               : std::format("{} encontrou: {}.", who, pickup.name));
    found.push_back(pickup);
  }
  return found;
}

}  // namespace ember
