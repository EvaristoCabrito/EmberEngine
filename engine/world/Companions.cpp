#include "world/Companions.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "world/Noise.h"

namespace ember {

namespace {

constexpr const char* kHeroes[] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};

bool isHero(const std::string& name) { return std::any_of(std::begin(kHeroes), std::end(kHeroes), [&](const char* h) { return name == h; }); }

std::string pairKey(const std::string& a, const std::string& b) { return a < b ? a + "|" + b : b + "|" + a; }

double clampScore(double v) { return std::max(-100.0, std::min(100.0, v)); }  // negative affinity exists (user, 2026-10-17)

Json readJson(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return Json();
  return Json::parse(in, nullptr, false);
}

const Json* memoryOf(const Json& save) {
  const auto it = save.find("companionConversations");
  return it != save.end() && it->is_object() ? &*it : nullptr;
}

bool told(const Json& save, const std::string& id) {
  const Json* m = memoryOf(save);
  return m && m->contains(id) && (*m)[id].is_number();
}

}  // namespace

double Companions::affinityScore(const Json& save, const std::string& a, const std::string& b) {
  const auto it = save.find("affinityScores");
  if (it == save.end() || !it->is_object()) return 0.0;
  const std::string key = pairKey(a, b);
  return it->contains(key) && (*it)[key].is_number() ? (*it)[key].get<double>() : 0.0;
}

bool Companions::load(const std::filesystem::path& companions, const std::filesystem::path& portuguese) {
  data_ = readJson(companions);
  if (!data_.is_object() || !data_.contains("trees") || !data_.contains("portraits")) return loaded_ = false;
  portraits_ = data_["portraits"];
  portuguese_.clear();
  const Json pt = readJson(portuguese);
  if (pt.is_object())
    for (const auto& [en, tr] : pt.items())
      if (tr.is_string()) portuguese_[en] = tr.get<std::string>();
  return loaded_ = true;
}

std::string Companions::text(const std::string& english, bool pt) const {
  if (!pt) return english;
  const auto it = portuguese_.find(english);
  return it != portuguese_.end() ? it->second : english;
}

int Companions::maxTier(const std::string& leader, const std::string& companion) const {
  if (!loaded_ || !data_.contains("maxTier")) return -1;
  const std::string key = leader + ":" + companion;
  return data_["maxTier"].contains(key) ? data_["maxTier"][key].get<int>() : -1;
}

std::string Companions::tierName(int tier, bool english) const {
  if (!loaded_ || tier < 0 || tier >= static_cast<int>(data_["tierNames"].size())) return {};
  return text(data_["tierNames"][static_cast<size_t>(tier)].get<std::string>(), !english);
}

std::vector<Companions::Topic> Companions::topics(const std::string& leader, const std::string& companion, const Json& save, bool english) const {
  std::vector<Topic> out;
  if (!loaded_ || leader == companion || !isHero(leader) || !isHero(companion)) return out;
  const int last = maxTier(leader, companion);
  const double points = affinityScore(save, leader, companion);
  const Json& labels = data_["topics"][companion];
  for (int tier = 0; tier <= last && tier < static_cast<int>(data_["tiers"].size()); ++tier) {
    Topic t;
    t.tier = tier;
    t.threshold = data_["tiers"][static_cast<size_t>(tier)].get<int>();
    if (tier >= static_cast<int>(labels.size())) break;
    t.label = text(labels[static_cast<size_t>(tier)].get<std::string>(), !english);
    t.resolved = told(save, sceneId(leader, companion, tier));
    t.unlocked = points >= t.threshold || t.resolved;
    out.push_back(std::move(t));
  }
  return out;
}

Json Companions::build(const std::string& leader, const std::string& companion, int tier, const Json& save, bool english) const {
  const std::vector<Topic> list = topics(leader, companion, save, english);
  const auto topic = std::find_if(list.begin(), list.end(), [&](const Topic& t) { return t.tier == tier; });
  if (topic == list.end() || !topic->unlocked) return Json();
  const std::string id = sceneId(leader, companion, tier);
  if (!data_["trees"].contains(id)) return Json();
  Json tree = data_["trees"][id];
  const bool replay = topic->resolved;
  const Json* memory = memoryOf(save);
  std::string prefix;
  if (tier > 0 && memory) {
    const std::string before = sceneId(leader, companion, tier - 1);
    if (memory->contains(before) && (*memory)[before].is_number()) {
      const double v = (*memory)[before].get<double>();
      if (v == -3) prefix = text(data_["continuity"]["hard"].get<std::string>(), !english);
      else if (v == 3) prefix = text(data_["continuity"]["warm"].get<std::string>(), !english);
    }
  }
  for (Json& line : tree["lines"]) {
    line["text"] = (line.value("id", "") == "confide" ? prefix : std::string()) + text(line.value("text", ""), !english);
    if (line.contains("replies"))
      for (Json& reply : line["replies"]) {
        reply["text"] = text(reply.value("text", ""), !english);
        if (replay && reply.contains("affinity")) reply["affinity"]["delta"] = 0;  // a replay never awards or removes points
      }
  }
  return tree;
}

bool Companions::resolveReply(Json& save, const Json& reply, const std::string& leader) const {
  if (!loaded_ || !reply.is_object() || !reply.contains("companionScene") || !reply.contains("affinity")) return false;
  const std::string id = reply.value("companionScene", "");
  const Json& af = reply["affinity"];
  if (id.empty() || !af.is_object() || told(save, id) || af.value("from", "") != leader) return false;
  const std::string companion = af.value("to", "");
  if (!isHero(companion) || companion == leader) return false;
  int tier = -1;
  if (const size_t colon = id.rfind(':'); colon != std::string::npos) tier = std::atoi(id.c_str() + colon + 1);
  if (tier < 0 || id != sceneId(leader, companion, tier)) return false;
  // unlocked by points alone (resolveCompanionReply passes no memory)
  if (tier > maxTier(leader, companion) || affinityScore(save, leader, companion) < data_["tiers"][static_cast<size_t>(tier)].get<int>()) return false;
  const double raw = af.value("delta", 0.0);
  const double score = affinityScore(save, leader, companion);
  // the leader catch-up bonus of positive changes; losses are never amplified
  const double delta = raw > 0 ? jsRound(raw * (2 - std::max(0.0, score) / 100) * 100) / 100 : raw;
  Json& scores = save["affinityScores"];
  if (!scores.is_object()) scores = Json::object();
  scores[pairKey(leader, companion)] = clampScore(jsRound((score + delta) * 10) / 10);
  Json& memory = save["companionConversations"];
  if (!memory.is_object()) memory = Json::object();
  memory[id] = raw;
  return true;
}

}  // namespace ember
