#include "world/Inn.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "core/Log.h"

namespace ember::e2 {

namespace {

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
bool truthy(const Json& v) {
  if (v.is_null()) return false;
  if (v.is_boolean()) return v.get<bool>();
  if (v.is_number()) return v.get<double>() != 0.0;
  if (v.is_string()) return !v.get<std::string>().empty();
  return true;
}
bool has(const Json& array, const std::string& value) {
  if (!array.is_array()) return false;
  for (const Json& v : array)
    if (v.is_string() && v.get<std::string>() == value) return true;
  return false;
}
Json number(double v) {
  if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 9.0e15) return Json(static_cast<long long>(v));
  return Json(v);
}
double jsRound(double v) { return std::floor(v + 0.5); }

}  // namespace

// --- data ----------------------------------------------------------------------------------------------------------

bool InnData::load(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  const Json j = Json::parse(in, nullptr, false);
  if (!in || j.is_discarded()) {
    logError("inn data not found or unreadable: {}", file.string());
    return false;
  }
  const Json& p = j["prices"];
  mealPrice = p.value("meal", 2), innFullness = p.value("innFullness", 120), restPrice = p.value("rest", 8);
  healerCastPrice = p.value("healerCast", 5), healerAilmentPrice = p.value("healerAilment", 7);
  lockpickPrice = p.value("lockpick", 6), rationsPrice = p.value("rations", 3);
  bagMax = j.value("bagMax", 5), rationStackMax = j.value("rationStackMax", 30), weaponMaxEnh = j.value("weaponMaxEnh", 5);
  for (const Json& c : j["enhCost"]) enhCost.push_back(c.get<int>());
  for (const Json& x : j["potions"]) potions.push_back({x["id"], x["label"], x["tooltip"], x["price"].get<int>(), x["carryMax"].get<int>()});
  lockpickTooltip = j.value("lockpickTooltip", ""), rationsIcon = j.value("rationsIcon", ""), bagIcon = j.value("bagIcon", "");
  for (const Json& w : j["weapons"]) {
    InnWeapon x;
    x.id = w["id"], x.name = w["name"], x.price = w["price"].get<int>();
    if (w["recommendedLevel"].is_number()) x.recommendedLevel = w["recommendedLevel"].get<int>();
    if (w["bonusClass"].is_string()) x.bonusClass = w["bonusClass"], x.bonusClassName = w["bonusClassName"].is_string() ? w["bonusClassName"].get<std::string>() : "";
    if (w["magic"].is_string()) x.magic = w["magic"];
    x.power = w["power"].get<double>(), x.dice = w["dice"], x.range = w["range"], x.icon = w["icon"];
    for (const Json& u : w["usableBy"]) x.usableBy.push_back(u);
    for (const Json& t : w["tooltip"]) x.tooltip.push_back(t);
    for (const Json& s : w["sell"]) x.sell.push_back(s.get<int>());
    weapons.push_back(std::move(x));
  }
  for (const Json& e : j["equipment"]) {
    InnGear x;
    x.id = e["id"], x.name = e["name"], x.slot = e["slot"], x.typeSlot = e["typeSlot"], x.tooltip = e["tooltip"], x.icon = e["icon"];
    x.price = e["price"].get<int>(), x.hp = e.value("hp", 0), x.pouch = e.value("pouch", false);
    if (e["usableBy"].is_array()) {
      x.usableBy.emplace();
      for (const Json& u : e["usableBy"]) x.usableBy->push_back(u);
    }
    equipment.push_back(std::move(x));
  }
  for (const auto& [k, v] : j["classNames"].items()) classNames[k] = v.get<std::string>();
  logInfo("inn: {} weapons, {} gear, {} potions", weapons.size(), equipment.size(), potions.size());
  return true;
}

const InnPotion* InnData::potion(const std::string& id) const {
  for (const InnPotion& p : potions)
    if (p.id == id) return &p;
  return nullptr;
}
const InnWeapon* InnData::weapon(const std::string& id) const {
  for (const InnWeapon& w : weapons)
    if (w.id == id) return &w;
  return nullptr;
}
const InnGear* InnData::gear(const std::string& id) const {
  for (const InnGear& g : equipment)
    if (g.id == id) return &g;
  return nullptr;
}

// --- the party as the Inn sees it ------------------------------------------------------------------------------------

int InnRules::ember(const Json& save) const { return static_cast<int>(num(save, "ember").value_or(0)); }

std::string InnRules::heroClass(const Json& save, const std::string& hero) const {
  const Json& promo = child(child(save, "promotions"), hero);
  if (promo.is_string()) return promo.get<std::string>();
  const Json& base = child(world_.tables()["baseClass"], hero);
  return base.is_string() ? base.get<std::string>() : "swordsman";
}

int InnRules::statusMaxHp(const Json& save, const std::string& hero) const {
  double gear = 0;
  for (const auto& [slot, id] : child(child(save, "equipment"), hero).items())
    if (id.is_string())
      if (const InnGear* g = data_.gear(id.get<std::string>())) gear += g->hp;
  double keep = 1.0;
  if (Overworld::fullness(child(child(save, "heroHunger"), hero)) <= 0) {
    const double streak = num(save, "hungerStreak").value_or(0);
    keep = 1.0 - std::min(0.9, std::max(0.0, streak - 3) * 0.1);
  }
  return static_cast<int>(jsRound((world_.maxHp(save, hero) + gear) * keep));
}

int InnRules::statusHp(const Json& save, const std::string& hero) const {
  const int max = statusMaxHp(save, hero);
  return std::min(max, static_cast<int>(num(child(save, "unitHp"), hero).value_or(max)));
}

bool InnRules::diseased(const Json& save, const std::string& hero) const { return truthy(child(child(save, "heroDiseases"), hero)); }
bool InnRules::poisoned(const Json& save, const std::string& hero) const { return truthy(child(child(save, "heroPoisons"), hero)); }

// --- the Adega -------------------------------------------------------------------------------------------------------

std::optional<Json> InnRules::buyMeal(const Json& save, const std::string& hero) const {
  if (ember(save) < data_.mealPrice || num(child(save, "unitHp"), hero).value_or(1) <= 0 ||
      Overworld::fullness(child(child(save, "heroHunger"), hero)) >= data_.innFullness)
    return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - data_.mealPrice;
  if (!next["heroHunger"].is_object()) next["heroHunger"] = Json::object();
  next["heroHunger"][hero] = data_.innFullness;
  return next;
}

std::optional<Json> InnRules::pay(const Json& save, const std::string& hero, const std::map<std::string, int>& cart, int lockpicks) const {
  int cost = 0;
  Json bag = child(child(save, "bags"), hero).is_object() ? child(child(save, "bags"), hero) : Json::object();
  for (const char* kind : {"weak", "mid", "potent", "disease", "manaSmall", "manaMid", "manaLarge"}) {
    const auto it = cart.find(kind);
    const int qty = it == cart.end() ? 0 : it->second;
    if (qty <= 0) continue;
    const InnPotion* p = data_.potion(kind);
    const int have = static_cast<int>(num(bag, kind).value_or(0));
    if (!p || have + qty > p->carryMax) return std::nullopt;
    cost += p->price * qty;
    bag[kind] = have + qty;
  }
  if (lockpicks > 0) {
    const int have = static_cast<int>(num(bag, "lockpick").value_or(0));
    if (have + lockpicks > data_.bagMax) return std::nullopt;
    cost += data_.lockpickPrice * lockpicks;
    bag["lockpick"] = have + lockpicks;
  }
  if (cost <= 0 || ember(save) < cost) return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - cost;
  next["emberSeeded"] = true;
  if (!next["bags"].is_object()) next["bags"] = Json::object();
  next["bags"][hero] = std::move(bag);
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::buyRations(const Json& save, int qty) const {
  if (qty <= 0) return std::nullopt;
  const int rations = static_cast<int>(num(save, "rations").value_or(0));
  const int stack = data_.rationStackMax;
  const int extra = (rations + qty + stack - 1) / stack - (rations + stack - 1) / stack;
  if (!world_.bagHasRoom(save, extra)) return std::nullopt;
  const int cost = data_.rationsPrice * qty;
  if (ember(save) < cost) return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - cost;
  next["emberSeeded"] = true;
  next["rations"] = rations + qty;
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::passNight(const Json& save, const std::vector<std::string>& heroes, int& healed) const {
  healed = 0;
  const int cost = static_cast<int>(heroes.size()) * data_.restPrice;
  if (heroes.empty() || ember(save) < cost) return std::nullopt;
  Json unitHp = child(save, "unitHp").is_object() ? child(save, "unitHp") : Json::object();
  for (const std::string& hero : heroes) {
    const int max = statusMaxHp(save, hero), hp = statusHp(save, hero);
    const int missing = std::max(0, max - hp);
    if (missing <= 0) continue;
    const int amount = std::min(missing, static_cast<int>(std::ceil(missing * 0.75)));
    unitHp[hero] = hp + amount;
    healed += amount;
  }
  Json next = save;
  next["ember"] = ember(save) - cost;
  next["gameClock"] = number(num(save, "gameClock").value_or(0) + 1);
  if (has(child(save, "completed"), "thebridge")) next["gameHour"] = 8;
  next["unitHp"] = std::move(unitHp);
  next["spellUses"] = Json::object();
  return next;
}

std::optional<Json> InnRules::healerCast(const Json& save, const std::string& hero, int& amount) const {
  amount = 0;
  if (ember(save) < data_.healerCastPrice) return std::nullopt;
  const int max = statusMaxHp(save, hero), hp = statusHp(save, hero);
  const int missing = max - hp;
  if (missing <= 0 || hp <= 0) return std::nullopt;
  amount = std::min(std::max(1, static_cast<int>(std::ceil(max * 0.25))), missing);
  if (amount <= 0) return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - data_.healerCastPrice;
  if (!next["unitHp"].is_object()) next["unitHp"] = Json::object();
  next["unitHp"][hero] = hp + amount;
  return next;
}

std::optional<Json> InnRules::cureAilments(const Json& save, const std::vector<std::string>& heroes, int& count) const {
  count = 0;
  std::vector<std::string> affected;
  for (const std::string& h : heroes)
    if (diseased(save, h) || poisoned(save, h)) affected.push_back(h);
  if (affected.empty()) return save;
  const int cost = static_cast<int>(affected.size()) * data_.healerAilmentPrice;
  if (ember(save) < cost) return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - cost;
  for (const std::string& h : affected) {
    if (next["heroDiseases"].is_object()) next["heroDiseases"].erase(h);
    if (next["heroPoisons"].is_object()) next["heroPoisons"].erase(h);
  }
  count = static_cast<int>(affected.size());
  return next;
}

// --- Vargan ------------------------------------------------------------------------------------------------------------

std::optional<Json> InnRules::buyWeapon(const Json& save, const std::string& weaponId) const {
  const InnWeapon* w = data_.weapon(weaponId);
  if (!w || child(save, "weapons").contains(weaponId)) return std::nullopt;
  if (!world_.bagHasRoom(save, 1) || ember(save) < w->price) return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - w->price;
  next["emberSeeded"] = true;
  if (!next["weapons"].is_object()) next["weapons"] = Json::object();
  next["weapons"][weaponId] = 0;
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::buyEquipment(const Json& save, const std::string& itemId) const {
  const InnGear* g = data_.gear(itemId);
  if (!g || g->price <= 0 || !world_.bagHasRoom(save, 1) || ember(save) < g->price) return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - g->price;
  next["emberSeeded"] = true;
  if (!next["looseEquipment"].is_object()) next["looseEquipment"] = Json::object();
  next["looseEquipment"][itemId] = static_cast<int>(num(next["looseEquipment"], itemId).value_or(0)) + 1;
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::upgradeWeapon(const Json& save, const std::string& weaponId) const {
  const int enh = static_cast<int>(num(child(save, "weapons"), weaponId).value_or(0));
  if (enh >= data_.weaponMaxEnh || enh >= static_cast<int>(data_.enhCost.size())) return std::nullopt;
  const int cost = data_.enhCost[static_cast<size_t>(enh)];
  if (ember(save) < cost) return std::nullopt;
  Json next = save;
  next["ember"] = ember(save) - cost;
  next["emberSeeded"] = true;
  next["weapons"][weaponId] = enh + 1;
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::sellWeapon(const Json& save, const std::string& weaponId, int& value) const {
  value = 0;
  const std::optional<double> enh = num(child(save, "weapons"), weaponId);
  const InnWeapon* w = data_.weapon(weaponId);
  if (!enh || !w) return std::nullopt;
  value = w->sell[std::min(static_cast<size_t>(*enh), w->sell.size() - 1)];
  Json next = save;
  next["ember"] = ember(save) + value;
  next["emberSeeded"] = true;
  next["weapons"].erase(weaponId);
  if (next["equipped"].is_object()) {
    std::vector<std::string> drop;
    for (const auto& [hero, id] : next["equipped"].items())
      if (id == weaponId) drop.push_back(hero);
    for (const std::string& h : drop) next["equipped"].erase(h);
  }
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::equipWeapon(const Json& save, const std::string& hero, const std::string& weaponId) const {
  Json next = save;
  if (weaponId.empty()) {
    if (!truthy(child(child(save, "equipped"), hero))) return std::nullopt;
    next["equipped"].erase(hero);
    if (!world_.bagHasRoom(next, 0)) return std::nullopt;
  } else {
    if (!child(save, "weapons").contains(weaponId)) return std::nullopt;
    if (!next["equipped"].is_object()) next["equipped"] = Json::object();
    std::vector<std::string> drop;
    for (const auto& [owner, id] : next["equipped"].items())
      if (owner != hero && id == weaponId) drop.push_back(owner);
    for (const std::string& o : drop) next["equipped"].erase(o);
    next["equipped"][hero] = weaponId;
  }
  next["pendingMission"] = nullptr;
  return next;
}

// --- quests ------------------------------------------------------------------------------------------------------------

std::vector<const Json*> InnRules::questsFor(const std::string& npc) const {
  std::vector<const Json*> out;
  for (const Json& q : world_.tables()["quests"])
    if (q.value("npc", "") == npc) out.push_back(&q);
  return out;
}

void InnRules::questProgress(const Json& save, const Json& quest, int& have, int& total) const {
  have = 0, total = 0;
  const std::string kind = quest.value("kind", ""), id = quest.value("id", "");
  if (kind == "kill") total = 1, have = has(child(save, "questKills"), quest.value("targetName", "")) ? 1 : 0;
  else if (kind == "recruit") total = 1, have = has(child(save, "flags"), "recruited:" + quest.value("targetName", "")) ? 1 : 0;
  else
    for (const Json& p : quest.value("pickups", Json::array())) ++total, have += has(child(save, "questItems"), id + ":" + p.value("id", "")) ? 1 : 0;
}

std::string InnRules::questStatus(const Json& save, const Json& quest) const {
  const std::string id = quest.value("id", "");
  if (has(child(save, "questsDone"), id)) return "done";
  if (!has(child(save, "questsActive"), id)) return "available";
  int have = 0, total = 0;
  questProgress(save, quest, have, total);
  return have >= total ? "ready" : "active";
}

bool InnRules::questOffered(const Json& save, const std::string& questId) const {
  for (const Json& q : world_.tables()["quests"])
    if (q.value("id", "") == questId) return world_.conditionHolds(child(q, "availability"), save);
  return true;
}

Json InnRules::talkToNpc(const Json& save, const std::string& npc) const {
  Json next = save;
  if (!has(child(save, "npcTalked"), npc)) {
    if (!next["npcTalked"].is_array()) next["npcTalked"] = Json::array();
    next["npcTalked"].push_back(npc);
  }
  for (const Json* q : questsFor(npc)) {
    const std::string id = q->value("id", "");
    if (questStatus(next, *q) == "available" && world_.conditionHolds(child(*q, "availability"), next) && !has(child(next, "questsDiscovered"), id)) {
      if (!next["questsDiscovered"].is_array()) next["questsDiscovered"] = Json::array();
      next["questsDiscovered"].push_back(id);
    }
  }
  return next;
}

std::optional<Json> InnRules::acceptQuest(const Json& save, const std::string& questId) const {
  for (const Json& q : world_.tables()["quests"])
    if (q.value("id", "") == questId) {
      if (questStatus(save, q) != "available") return std::nullopt;
      Json next = save;
      if (!next["questsActive"].is_array()) next["questsActive"] = Json::array();
      next["questsActive"].push_back(questId);
      return next;
    }
  return std::nullopt;
}

std::optional<Json> InnRules::turnInQuest(const Json& save, const std::string& questId) const {
  const Json* quest = nullptr;
  for (const Json& q : world_.tables()["quests"])
    if (q.value("id", "") == questId) quest = &q;
  if (!quest || questStatus(save, *quest) != "ready") return std::nullopt;
  Json next = save;
  Json& bags = next["bags"];
  if (!bags.is_object()) bags = Json::object();
  // Reward potions go to whoever has room (mana potions try the casters first, the order otherwise kept).
  for (const Json& kindJ : quest->value("rewardPotions", Json::array())) {
    const std::string kind = kindJ.get<std::string>();
    std::vector<std::string> order;
    for (const auto& [hero, bag] : bags.items()) order.push_back(hero);
    if (kind == "manaMid" || kind == "manaSmall" || kind == "manaLarge")
      std::stable_sort(order.begin(), order.end(), [](const std::string& a, const std::string& b) {
        const bool ca = a == "Voss" || a == "Salazar", cb = b == "Voss" || b == "Salazar";
        return ca && !cb;
      });
    const InnPotion* p = data_.potion(kind);
    for (const std::string& hero : order)
      if (num(bags[hero], kind).value_or(0) < (p ? p->carryMax : 5)) {
        bags[hero][kind] = static_cast<int>(num(bags[hero], kind).value_or(0)) + 1;
        break;
      }
  }
  next["ember"] = ember(save) + quest->value("reward", 0);
  next["emberSeeded"] = true;
  Json active = Json::array();
  for (const Json& a : child(save, "questsActive"))
    if (a != questId) active.push_back(a);
  next["questsActive"] = std::move(active);
  if (!next["questsDone"].is_array()) next["questsDone"] = Json::array();
  next["questsDone"].push_back(questId);
  return next;
}

}  // namespace ember::e2
