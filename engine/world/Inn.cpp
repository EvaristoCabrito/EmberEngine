#include "world/Inn.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "core/Log.h"
#include "world/Battle.h"  // UnknownItemRules (content/unknown-items.json)

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
  for (const Json& x : j["potions"])
    potions.push_back({x["id"], x["label"], x["tooltip"], x.value("effect", "heal"), x["price"].get<int>(), x["carryMax"].get<int>(), x.value("dice", 0),
                       x.value("faces", 0), x.value("bonus", 0), x.value("manaRestore", 0)});
  lockpickTooltip = j.value("lockpickTooltip", ""), rationsIcon = j.value("rationsIcon", ""), bagIcon = j.value("bagIcon", "");
  for (const Json& w : j["weapons"]) {
    InnWeapon x;
    x.id = w["id"], x.name = w["name"], x.price = w["price"].get<int>();
    if (w["recommendedLevel"].is_number()) x.recommendedLevel = w["recommendedLevel"].get<int>();
    if (w["bonusClass"].is_string()) x.bonusClass = w["bonusClass"], x.bonusClassName = w["bonusClassName"].is_string() ? w["bonusClassName"].get<std::string>() : "";
    if (w["magic"].is_string()) x.magic = w["magic"];
    x.twoHanded = w.value("twoHanded", false);
    x.magBonus = w.value("magBonus", 0), x.dexBonus = w.value("dexBonus", 0);
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
    x.atk = e.value("atk", 0), x.mag = e.value("mag", 0), x.def = e.value("def", 0), x.dex = e.value("dex", 0), x.mov = e.value("mov", 0);
    x.kind = e["kind"].is_string() ? e["kind"].get<std::string>() : "";
    x.summary = e.value("summary", "");
    if (e["dmgMul"].is_number()) x.dmgMul = e["dmgMul"].get<double>();
    if (e["dice"].is_number()) x.dice = e["dice"].get<int>();
    if (e["faces"].is_number()) x.faces = e["faces"].get<int>();
    if (e["usableBy"].is_array()) {
      x.usableBy.emplace();
      for (const Json& u : e["usableBy"]) x.usableBy->push_back(u);
    }
    equipment.push_back(std::move(x));
  }
  for (const auto& [k, v] : j["classNames"].items()) classNames[k] = v.get<std::string>();
  for (const Json& s : j.value("equipmentSlots", Json::array())) equipmentSlots.push_back({s["id"], s["label"]});
  const Json tiers = j.value("tierUses", Json::object());  // a named copy: items() of a temporary dangles
  for (const auto& [k, levels] : tiers.items()) {
    auto& t = tierUses[k];
    for (const Json& row : levels) {
      t.emplace_back();
      for (const Json& v : row) t.back().push_back(v.get<int>());
    }
  }
  // Ours: each weapon's type, from the loadouts' table beside the Inn's (an unknown weapon shows only that)
  if (std::ifstream arms(file.parent_path() / "weapons.json", std::ios::binary); arms) {
    const Json a = Json::parse(arms, nullptr, false);
    if (!a.is_discarded() && a.is_object() && a.contains("weapons") && a["weapons"].is_object())
      for (InnWeapon& w : weapons)
        if (a["weapons"].contains(w.id) && a["weapons"][w.id].is_object()) w.weaponType = a["weapons"][w.id].value("weaponType", "");
  }
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
  static const std::map<std::string, std::string> kBase = {{"Kael", "kaelFinal"}, {"Neera", "neera"}, {"Voss", "voss"},
                                                           {"Salazar", "salazar"}, {"Aldric", "aldric"}, {"Malrec", "conjurer"}};
  const auto it = kBase.find(hero);
  return it == kBase.end() ? "swordsman" : it->second;
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
    if (weaponUnknown(save, weaponId)) return std::nullopt;  // an unknown weapon is never wielded (Identificar first)
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

// --- the inventory -------------------------------------------------------------------------------------------------

bool InnRules::fitsSlot(const InnGear& item, const std::string& slot) {
  if (item.slot == slot) return true;
  if ((item.slot == "ring1" || item.slot == "ring2") && (slot == "ring1" || slot == "ring2")) return true;
  return item.slot == "shoulders" && slot == "back";
}

bool InnRules::offHandBlocked(const Json& save, const std::string& hero) const {
  const Json& id = child(child(save, "equipped"), hero);
  const InnWeapon* w = id.is_string() ? data_.weapon(id.get<std::string>()) : nullptr;
  return w && w->twoHanded;
}

// inventory.ts equipSharedItem / unequipSharedItem (and GameApp: unequipping needs room in the bag).
std::optional<Json> InnRules::equipItem(const Json& save, const std::string& hero, const std::string& slot, const std::string& itemId) const {
  Json next = save;
  if (!next["equipment"].is_object()) next["equipment"] = Json::object();
  if (!next["looseEquipment"].is_object()) next["looseEquipment"] = Json::object();
  const Json& worn = child(child(save, "equipment"), hero);
  const std::string current = child(worn, slot).is_string() ? child(worn, slot).get<std::string>() : "";
  if (itemId.empty()) {
    if (current.empty()) return std::nullopt;
    next["equipment"][hero].erase(slot);
    next["looseEquipment"][current] = static_cast<int>(num(next["looseEquipment"], current).value_or(0)) + 1;
    if (!world_.bagHasRoom(next, 0)) return std::nullopt;
    next["pendingMission"] = nullptr;
    return next;
  }
  const InnGear* item = data_.gear(itemId);
  if (!item || !fitsSlot(*item, slot)) return std::nullopt;
  const int total = static_cast<int>(num(child(save, "looseEquipment"), itemId).value_or(0));
  const int reserve = total - gearUnknown(save, itemId);  // the known copies only: an unknown piece is never worn (Identificar first)
  if (current == itemId) return save;
  // A spare in the stash first; else the piece moves off another slot (another hero's, or this hero's other finger).
  std::optional<std::pair<std::string, std::string>> source;
  if (reserve <= 0)
    for (const auto& [owner, slots] : child(save, "equipment").items()) {
      for (const auto& [usedSlot, id] : slots.items())
        if (id == itemId && !(owner == hero && usedSlot == slot)) {
          source = std::pair<std::string, std::string>{owner, usedSlot};
          break;
        }
      if (source) break;
    }
  if (!source && reserve <= 0) return std::nullopt;
  if (source) next["equipment"][source->first].erase(source->second);
  else if (total == 1) next["looseEquipment"].erase(itemId);
  else next["looseEquipment"][itemId] = total - 1;
  if (!current.empty()) next["looseEquipment"][current] = static_cast<int>(num(next["looseEquipment"], current).value_or(0)) + 1;
  if (!next["equipment"][hero].is_object()) next["equipment"][hero] = Json::object();
  next["equipment"][hero][slot] = itemId;
  next["pendingMission"] = nullptr;
  return next;
}

namespace {
// The unknown marks (ours): a weapon's taken off the list, a piece's copies one fewer, a potion's count one fewer.
void unmarkWeapon(Json& next, const std::string& weaponId) {
  if (!next["unknownWeapons"].is_array()) return;
  Json kept = Json::array();
  for (const Json& id : next["unknownWeapons"])
    if (!(id.is_string() && id.get<std::string>() == weaponId)) kept.push_back(id);
  next["unknownWeapons"] = std::move(kept);
}
void unmarkGear(Json& next, const std::string& itemId, int total) {
  if (!next["unknownEquipment"].is_object() || !next["unknownEquipment"].contains(itemId)) return;
  const int n = std::min(num(next["unknownEquipment"], itemId).value_or(0) - 1, static_cast<double>(total));
  if (n <= 0) next["unknownEquipment"].erase(itemId);
  else next["unknownEquipment"][itemId] = static_cast<int>(n);
}
void unmarkPotion(Json& next, const std::string& hero, const std::string& kind) {
  if (!next["unknownPotions"].is_object() || !next["unknownPotions"][hero].is_object()) return;
  const int n = static_cast<int>(num(next["unknownPotions"][hero], kind).value_or(0)) - 1;
  if (n <= 0) next["unknownPotions"][hero].erase(kind);
  else next["unknownPotions"][hero][kind] = n;
}
}  // namespace

std::optional<Json> InnRules::discardWeapon(const Json& save, const std::string& weaponId) const {
  if (!child(save, "weapons").contains(weaponId)) return std::nullopt;
  Json next = save;
  next["weapons"].erase(weaponId);
  unmarkWeapon(next, weaponId);
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::discardEquipment(const Json& save, const std::string& itemId, bool unknownCopy) const {
  const int reserve = static_cast<int>(num(child(save, "looseEquipment"), itemId).value_or(0));
  if (reserve <= 0) return std::nullopt;
  const int unknown = gearUnknown(save, itemId);
  if (unknownCopy ? unknown <= 0 : reserve - unknown <= 0) return std::nullopt;  // the copy thrown away is of the kind asked for
  Json next = save;
  if (reserve <= 1) next["looseEquipment"].erase(itemId);
  else next["looseEquipment"][itemId] = reserve - 1;
  if (unknownCopy) unmarkGear(next, itemId, reserve - 1);
  next["pendingMission"] = nullptr;
  return next;
}

// --- Itens desconhecidos (ours, docs/SKILLS_MAGE.md) --------------------------------------------------------------------------------

bool InnRules::weaponUnknown(const Json& save, const std::string& weaponId) const {
  const Json& list = child(save, "unknownWeapons");
  if (!list.is_array()) return false;
  for (const Json& id : list)
    if (id.is_string() && id.get<std::string>() == weaponId) return true;
  return false;
}
int InnRules::gearUnknown(const Json& save, const std::string& itemId) const {
  const int total = static_cast<int>(num(child(save, "looseEquipment"), itemId).value_or(0));
  return std::max(0, std::min(total, static_cast<int>(num(child(save, "unknownEquipment"), itemId).value_or(0))));
}
int InnRules::potionsUnknown(const Json& save, const std::string& hero, const std::string& kind) const {
  return std::max(0, static_cast<int>(num(child(child(save, "unknownPotions"), hero), kind).value_or(0)));
}
std::vector<InnRules::UnknownItem> InnRules::unknownItems(const Json& save) const {
  const UnknownItemRules& ur = UnknownItemRules::shared();
  auto sellValue = [&](int price) { return std::max(1, static_cast<int>(std::floor(price * ur.sellShare))); };
  static const std::pair<const char*, std::pair<const char*, const char*>> kTypes[] = {
      {"sword", {"Espada desconhecida", "Unknown sword"}},   {"bow", {"Arco desconhecido", "Unknown bow"}},       {"crossbow", {"Besta desconhecida", "Unknown crossbow"}},
      {"staff", {"Cajado desconhecido", "Unknown staff"}},   {"spear", {"Lança desconhecida", "Unknown spear"}},  {"axe", {"Machado desconhecido", "Unknown axe"}},
      {"hammer", {"Martelo desconhecido", "Unknown hammer"}}, {"mace", {"Maça desconhecida", "Unknown mace"}},   {"dagger", {"Adaga desconhecida", "Unknown dagger"}}};
  std::vector<UnknownItem> out;
  const Json& weapons = child(save, "unknownWeapons");
  if (weapons.is_array())
    for (const Json& id : weapons) {
      if (!id.is_string() || !child(save, "weapons").contains(id.get<std::string>())) continue;
      const InnWeapon* w = data_.weapon(id.get<std::string>());
      if (!w) continue;
      UnknownItem u;
      u.kind = "weapon", u.id = w->id, u.realName = w->name, u.sell = sellValue(w->price);
      u.label = "Arma desconhecida", u.labelEn = "Unknown weapon";
      for (const auto& [type, names] : kTypes)
        if (w->weaponType == type) u.label = names.first, u.labelEn = names.second;
      out.push_back(std::move(u));
    }
  const Json gear = child(save, "unknownEquipment");  // a named copy: items() of a temporary dangles
  if (gear.is_object())
    for (const auto& [id, nJ] : gear.items()) {
      const InnGear* g = data_.gear(id);
      const int n = gearUnknown(save, id);
      if (!g || n <= 0) continue;
      for (int k = 0; k < n; ++k) {
        UnknownItem u;
        u.kind = "gear", u.id = g->id, u.realName = g->name, u.sell = sellValue(g->price);
        u.label = "Equipamento desconhecido (" + g->typeSlot + ")", u.labelEn = "Unknown gear (" + g->typeSlot + ")";
        out.push_back(std::move(u));
      }
    }
  const Json potions = child(save, "unknownPotions");
  if (potions.is_object())
    for (const auto& [hero, kinds] : potions.items()) {
      if (!kinds.is_object()) continue;
      for (const auto& [kind, nJ] : kinds.items()) {
        const InnPotion* p = data_.potion(kind);
        const int n = nJ.is_number() ? nJ.get<int>() : 0;
        if (!p || n <= 0) continue;
        for (int k = 0; k < n; ++k) {
          UnknownItem u;
          u.kind = "potion", u.id = kind, u.hero = hero, u.realName = p->label, u.sell = sellValue(p->price);
          u.label = "Poção desconhecida (" + hero + ")", u.labelEn = "Unknown potion (" + hero + ")";
          out.push_back(std::move(u));
        }
      }
    }
  return out;
}
std::optional<Json> InnRules::sellUnknown(const Json& save, const UnknownItem& item, int& value) const {
  value = item.sell;
  Json next = save;
  if (item.kind == "weapon") {
    if (!weaponUnknown(save, item.id) || !child(save, "weapons").contains(item.id)) return std::nullopt;
    next["weapons"].erase(item.id);
    unmarkWeapon(next, item.id);
  } else if (item.kind == "gear") {
    const int reserve = static_cast<int>(num(child(save, "looseEquipment"), item.id).value_or(0));
    if (reserve <= 0 || gearUnknown(save, item.id) <= 0) return std::nullopt;
    if (reserve <= 1) next["looseEquipment"].erase(item.id);
    else next["looseEquipment"][item.id] = reserve - 1;
    unmarkGear(next, item.id, reserve - 1);
  } else if (item.kind == "potion") {
    if (potionsUnknown(save, item.hero, item.id) <= 0) return std::nullopt;
    unmarkPotion(next, item.hero, item.id);
  } else {
    return std::nullopt;
  }
  next["ember"] = ember(save) + value;
  next["emberSeeded"] = true;
  next["pendingMission"] = nullptr;
  return next;
}
Json InnRules::identify(const Json& save, const UnknownItem& item) const {
  Json next = save;
  if (item.kind == "weapon") unmarkWeapon(next, item.id);
  else if (item.kind == "gear") unmarkGear(next, item.id, static_cast<int>(num(child(save, "looseEquipment"), item.id).value_or(0)));
  else if (item.kind == "potion" && potionsUnknown(save, item.hero, item.id) > 0) {
    unmarkPotion(next, item.hero, item.id);
    if (!next["bags"].is_object()) next["bags"] = Json::object();
    if (!next["bags"][item.hero].is_object()) next["bags"][item.hero] = Json::object();
    next["bags"][item.hero][item.id] = static_cast<int>(num(next["bags"][item.hero], item.id).value_or(0)) + 1;  // into the hero's own bag now
  }
  return next;
}
Json InnRules::discardUnknown(const Json& save, const UnknownItem& item) const {
  if (item.kind == "weapon") return discardWeapon(save, item.id).value_or(save);
  if (item.kind == "gear") return discardEquipment(save, item.id, true).value_or(save);
  Json next = save;
  if (item.kind == "potion") unmarkPotion(next, item.hero, item.id);
  return next;
}

std::optional<Json> InnRules::discardRation(const Json& save) const {
  const int rations = static_cast<int>(num(save, "rations").value_or(0));
  if (rations <= 0) return std::nullopt;
  Json next = save;
  next["rations"] = rations - 1;
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::discardBagItem(const Json& save, const std::string& hero, const std::string& kind) const {
  const Json& bag = child(child(save, "bags"), hero);
  if (!bag.is_object() || num(bag, kind).value_or(0) <= 0) return std::nullopt;
  Json next = save;
  next["bags"][hero][kind] = static_cast<int>(num(bag, kind).value_or(0)) - 1;
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::usePotion(const Json& save, const std::string& hero, const std::string& kind, const std::function<double()>& rng) const {
  const Json& bag = child(child(save, "bags"), hero);
  if (!bag.is_object() || num(bag, kind).value_or(0) <= 0) return std::nullopt;
  const InnPotion* p = data_.potion(kind);
  if (!p) return std::nullopt;
  Json next = save;
  const int left = static_cast<int>(num(bag, kind).value_or(0)) - 1;
  auto trainHealing = [&] {
    // skills.ts rollSkillGain on Healing: chance (cap - value) / cap, +0.1 rounded to hundredths.
    const double value = num(child(child(save, "heroSkills"), hero), "healing").value_or(0);
    const double cap = world_.tables().value("skillCap", 100.0), gain = world_.tables().value("skillGain", 0.1);
    if (rng() >= std::max(0.0, std::min(1.0, (cap - value) / cap))) return;
    if (!next["heroSkills"].is_object()) next["heroSkills"] = Json::object();
    if (!next["heroSkills"][hero].is_object()) next["heroSkills"][hero] = Json::object();
    next["heroSkills"][hero]["healing"] = number(std::min(cap, jsRound((value + gain) * 100) / 100));
  };
  if (p->effect == "mana") {
    // GameApp MAP_STATUS_CLASS for the tier table (the rules class, not the sprite one).
    const Json& promo = child(child(save, "promotions"), hero);
    const Json& base = child(world_.tables()["baseClass"], hero);
    const std::string cls = promo.is_string() ? promo.get<std::string>() : base.is_string() ? base.get<std::string>() : "swordsman";
    const int level = static_cast<int>(num(child(save, "levels"), hero).value_or(1));
    const auto table = data_.tierUses.find(cls);
    Json spent = child(child(save, "spellUses"), hero).is_object() ? child(child(save, "spellUses"), hero) : Json::object();
    bool restored = false;
    for (int t = 1; t <= 10; ++t) {
      const std::string tk = "tier" + std::to_string(t);
      const int uses = table == data_.tierUses.end() ? 0 : table->second[static_cast<size_t>(std::clamp(level, 1, 30) - 1)][static_cast<size_t>(t - 1)];
      if (uses <= 0) continue;
      const double cur = num(spent, tk).value_or(0);
      if (cur <= 0) continue;
      const double after = std::max(0.0, cur - p->manaRestore);
      if (after != cur) restored = true;
      spent[tk] = number(after);
    }
    if (!restored) return std::nullopt;
    next["bags"][hero][kind] = left;
    if (!next["spellUses"].is_object()) next["spellUses"] = Json::object();
    next["spellUses"][hero] = std::move(spent);
  } else if (p->effect == "disease") {
    if (!diseased(save, hero) && !poisoned(save, hero)) return std::nullopt;
    if (next["heroDiseases"].is_object()) next["heroDiseases"].erase(hero);
    if (next["heroPoisons"].is_object()) next["heroPoisons"].erase(hero);
    trainHealing();
    next["bags"][hero][kind] = left;
  } else {
    // heroMaxHp: class + gear, less the party's hunger penalty.
    double gear = 0;
    for (const auto& [slot, id] : child(child(save, "equipment"), hero).items())
      if (id.is_string())
        if (const InnGear* g = data_.gear(id.get<std::string>())) gear += g->hp;
    const double streak = num(save, "hungerStreak").value_or(0);
    const int max = static_cast<int>(jsRound((world_.maxHp(save, hero) + gear) * (1 - std::min(0.9, std::max(0.0, streak - 3) * 0.1))));
    const int current = static_cast<int>(num(child(save, "unitHp"), hero).value_or(max));
    if (current >= max) return std::nullopt;
    int rolled = p->bonus;
    for (int i = 0; i < p->dice; ++i) rolled += 1 + static_cast<int>(std::floor(rng() * p->faces));
    const double skill = std::clamp(num(child(child(save, "heroSkills"), hero), "healing").value_or(0), 0.0, world_.tables().value("skillCap", 100.0));
    const int healed = static_cast<int>(std::floor(std::max(0, rolled) * (1 + skill / 100) + 1e-9));
    const int gained = std::min(healed, max - current);
    if (gained <= 0) return std::nullopt;
    trainHealing();
    if (!next["unitHp"].is_object()) next["unitHp"] = Json::object();
    next["unitHp"][hero] = current + gained;
    next["bags"][hero][kind] = left;
  }
  next["pendingMission"] = nullptr;
  return next;
}

std::optional<Json> InnRules::useRation(const Json& save, const std::string& hero) const {
  const int rations = static_cast<int>(num(save, "rations").value_or(0));
  if (rations < 1 || num(child(save, "unitHp"), hero).value_or(1) <= 0 || Overworld::fullness(child(child(save, "heroHunger"), hero)) >= 100) return std::nullopt;
  Json next = save;
  next["rations"] = rations - 1;
  if (!next["heroHunger"].is_object()) next["heroHunger"] = Json::object();
  next["heroHunger"][hero] = 100;
  if (num(next, "hungerStreak").value_or(0) > 0 || num(next, "hungerHours").value_or(0) > 0) {
    // overworld.ts partyIsFed: every recruited, standing hero has some fullness.
    bool fed = true;
    for (const char* h : {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"}) {
      if (!world_.heroRecruited(h, next)) continue;
      if (num(child(next, "unitHp"), h).value_or(world_.maxHp(next, h)) <= 0) continue;
      if (!(num(child(next, "heroHunger"), h).value_or(100) > 0)) fed = false;
    }
    if (fed) next["hungerStreak"] = 0, next["hungerHours"] = 0;
  }
  next["pendingMission"] = nullptr;
  return next;
}

InnRules::GearBonus InnRules::gearBonus(const Json& save, const std::string& hero, const std::string& classId) const {
  GearBonus total;
  // a staff's own magic counts while its class wields it (gearStatBonus: weapon.magic, usableBy the class)
  const Json& weaponId = child(child(save, "equipped"), hero);
  if (weaponId.is_string())
    if (const InnWeapon* w = data_.weapon(weaponId.get<std::string>()))
      if (classId.empty() || std::find(w->usableBy.begin(), w->usableBy.end(), classId) != w->usableBy.end()) total.mag += w->magBonus, total.dex += w->dexBonus;
  const Json& worn = child(child(save, "equipment"), hero);
  if (worn.is_object())
    for (const auto& [slot, id] : worn.items()) {
      if (!id.is_string()) continue;
      const InnGear* item = data_.gear(id.get<std::string>());
      if (!item) continue;
      total.hp += item->hp, total.atk += item->atk, total.mag += item->mag, total.def += item->def, total.dex += item->dex, total.mov += item->mov;
    }
  return total;
}

std::vector<std::string> InnRules::gearLines(const Json& save, const std::string& hero, const std::string& stat) const {
  std::vector<std::string> lines;
  const Json& worn = child(child(save, "equipment"), hero);
  if (!worn.is_object()) return lines;
  for (const auto& [slot, id] : worn.items()) {
    if (!id.is_string()) continue;
    const InnGear* item = data_.gear(id.get<std::string>());
    if (!item) continue;
    const int amount = stat == "hp" ? item->hp : stat == "atk" ? item->atk : stat == "mag" ? item->mag : stat == "def" ? item->def : stat == "dex" ? item->dex : item->mov;
    if (amount) lines.push_back(std::format("{} {}{}", item->name, amount > 0 ? "+" : "", amount));
  }
  return lines;
}

int InnRules::bagUsed(const Json& save) const {
  std::vector<std::string> worn;
  for (const auto& [h, id] : child(save, "equipped").items())
    if (id.is_string()) worn.push_back(id.get<std::string>());
  int used = 0;
  for (const auto& [id, enh] : child(save, "weapons").items())
    if (std::find(worn.begin(), worn.end(), id) == worn.end()) ++used;
  for (const auto& [id, qty] : child(save, "looseEquipment").items()) used += qty.is_number() ? qty.get<int>() : 0;
  // The Conjurador's runes (world/FieldSpells, docs/SKILLS_CONJURER.md): each one a slot of the party's bag
  if (const Json& runes = child(save, "runes"); runes.is_array()) used += static_cast<int>(runes.size());
  const int rations = static_cast<int>(num(save, "rations").value_or(0));
  return used + (rations + data_.rationStackMax - 1) / data_.rationStackMax;
}

int InnRules::bagCapacity(const Json& save) const {
  int heroes = 0;
  for (const char* h : {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"})
    if (world_.heroRecruited(h, save)) ++heroes;
  int pouches = 0;
  for (const auto& [hero, slots] : child(save, "equipment").items()) {
    const Json& waist = child(slots, "waist");
    if (!waist.is_string()) continue;
    for (const Json& e : world_.tables()["equipment"])
      if (e["id"] == waist) pouches += e.value("pouch", 0);
  }
  return world_.tables().value("partyBagPerHero", 10) * std::max(1, heroes) + pouches;
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
