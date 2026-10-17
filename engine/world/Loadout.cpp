#include "world/Loadout.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "core/Log.h"

namespace ember::e2 {
namespace {

std::string text(const Json& j, const char* key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); }
int integer(const Json& j, const char* key, int fallback) { return j.contains(key) && j[key].is_number() ? j[key].get<int>() : fallback; }
double number(const Json& j, const char* key, double fallback) { return j.contains(key) && j[key].is_number() ? j[key].get<double>() : fallback; }
bool yes(const Json& j, const char* key) { return j.contains(key) && j[key].is_boolean() && j[key].get<bool>(); }
std::vector<std::string> strings(const Json& j, const char* key) {
  std::vector<std::string> out;
  if (j.contains(key) && j[key].is_array())
    for (const Json& v : j[key])
      if (v.is_string()) out.push_back(v.get<std::string>());
  return out;
}
bool has(const std::vector<std::string>& list, const std::string& v) { return std::find(list.begin(), list.end(), v) != list.end(); }

// weaponTypes.ts cleanWeaponSkill: a finite number bounded to 0..100, anything else 0.
double cleanSkill(const Json& v) { return v.is_number() && std::isfinite(v.get<double>()) ? std::clamp(v.get<double>(), 0.0, 100.0) : 0.0; }

}  // namespace

bool Loadouts::load(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  const Json j = in ? Json::parse(in, nullptr, false) : Json();
  if (!j.is_object()) {
    logWarn("weapons: {} could not be read", file.generic_string());
    return false;
  }
  mageRangeBonus_ = strings(j, "mageRangeBonus");
  if (j.contains("weapons") && j["weapons"].is_object())
    for (const auto& [id, w] : j["weapons"].items()) {
      WeaponDef d;
      d.id = id, d.weaponType = text(w, "weaponType"), d.bonusClass = text(w, "bonusClass"), d.usableBy = strings(w, "usableBy");
      d.dice = integer(w, "dice", 0), d.faces = integer(w, "faces", 0), d.bonus = integer(w, "bonus", 0);
      d.minRange = integer(w, "minRange", 1), d.maxRange = integer(w, "maxRange", 1);
      d.ranged = yes(w, "ranged"), d.twoHanded = yes(w, "twoHanded");
      if (w.contains("magic") && w["magic"].is_object()) {
        const Json& m = w["magic"];
        d.magic = true;
        d.attackElement = text(m, "attackElement");
        if (m.contains("elementalBonusPct") && m["elementalBonusPct"].is_object())
          for (const auto& [e, v] : m["elementalBonusPct"].items())
            if (v.is_number()) d.elementalBonusPct.emplace_back(e, v.get<double>());
        d.lifeStealPct = number(m, "lifeStealPct", 0), d.regeneration = number(m, "regeneration", 0);
      }
      weapons_[id] = std::move(d);
    }
  if (j.contains("offHand") && j["offHand"].is_object())
    for (const auto& [id, e] : j["offHand"].items()) {
      OffHandDef d;
      d.id = id, d.kind = text(e, "kind"), d.weaponType = text(e, "weaponType"), d.usableBy = strings(e, "usableBy");
      d.minRange = integer(e, "minRange", 1), d.maxRange = integer(e, "maxRange", 1);
      d.dice = integer(e, "dice", 1), d.faces = integer(e, "faces", 4), d.bonus = integer(e, "bonus", 0), d.dmgMul = number(e, "dmgMul", 0.75);
      offHand_[id] = std::move(d);
    }
  if (j.contains("classes") && j["classes"].is_object())
    for (const auto& [id, c] : j["classes"].items()) {
      ClassArms a;
      a.starter = text(c, "starter"), a.weaponTypes = strings(c, "weaponTypes");
      a.minRange = integer(c, "minRange", 1), a.maxRange = integer(c, "maxRange", 1);
      classes_[id] = std::move(a);
    }
  logInfo("weapons: {} main-hand, {} off-hand, {} classes", weapons_.size(), offHand_.size(), classes_.size());
  return !weapons_.empty();
}

const WeaponDef* Loadouts::weapon(const std::string& id) const {
  const auto it = weapons_.find(id);
  return it == weapons_.end() ? nullptr : &it->second;
}
const OffHandDef* Loadouts::offHand(const std::string& id) const {
  const auto it = offHand_.find(id);
  return it == offHand_.end() ? nullptr : &it->second;
}
const ClassArms* Loadouts::arms(const std::string& classId) const {
  const auto it = classes_.find(classId);
  return it == classes_.end() ? nullptr : &it->second;
}
bool Loadouts::mageRangeBonus(const std::string& classId) const { return has(mageRangeBonus_, classId); }

bool equipFromSave(BattleUnit& u, const Loadouts& arms, const Json& save) {
  const ClassArms* cls = arms.arms(u.classId);
  if (!cls) return false;
  // GameApp startBattle: weapons[hero] = {id: save.equipped[hero], enh: save.weapons[id] ?? 0}; spawnUnit: the class's starter when
  // the hero has none, or wields one its class cannot
  std::string weaponId;
  int enh = 0;
  const Json equipped = save.value("equipped", Json::object());
  if (equipped.is_object() && equipped.contains(u.name) && equipped[u.name].is_string()) {
    weaponId = equipped[u.name].get<std::string>();
    const Json owned = save.value("weapons", Json::object());
    enh = owned.is_object() && owned.contains(weaponId) && owned[weaponId].is_number() ? owned[weaponId].get<int>() : 0;
  } else {
    weaponId = cls->starter;
  }
  if (const WeaponDef* w = arms.weapon(weaponId); !weaponId.empty() && (!w || !has(w->usableBy, u.classId))) weaponId = cls->starter, enh = 0;
  const WeaponDef* w = weaponId.empty() ? nullptr : arms.weapon(weaponId);
  // reach: the weapon's own (the class's without one), +1 for the mage classes
  u.minRange = w ? w->minRange : cls->minRange;
  u.maxRange = (w ? w->maxRange : cls->maxRange) + (arms.mageRangeBonus(u.classId) ? 1 : 0);
  // the off hand: save.equipment[hero].offHand, never beside a two-handed weapon
  const OffHandDef* off = nullptr;
  if (!(w && w->twoHanded)) {
    const Json equipment = save.value("equipment", Json::object());
    if (equipment.is_object() && equipment.contains(u.name) && equipment[u.name].is_object())
      if (const std::string id = text(equipment[u.name], "offHand"); !id.empty()) off = arms.offHand(id);
  }
  CombatProfile& p = u.combat;
  p.hasWeapon = w != nullptr;
  p.dice = w ? w->dice : 0, p.faces = w ? w->faces : 0, p.bonus = w ? w->bonus : 0, p.enh = enh;
  p.ranged = w && w->ranged, p.classBonus = w && w->bonusClass == u.classId;
  p.offHandWeapon = off && off->kind == "weapon", p.offHandShield = off && off->kind == "shield";
  p.offMinRange = off ? off->minRange : 1, p.offMaxRange = off ? off->maxRange : 1;
  p.offDice = off ? off->dice : 1, p.offFaces = off ? off->faces : 4, p.offBonus = off ? off->bonus : 0, p.offShieldMul = off ? off->dmgMul : 0.75;
  // staff magic (staffMagic: the weapon's magic while its class wields it)
  p.staff = w && w->magic && has(w->usableBy, u.classId);
  p.staffElement = p.staff && !w->attackElement.empty() ? w->attackElement : "arcane";
  p.staffBonusPct = 0, p.staffBonuses.clear(), p.staffLifeStealPct = 0, p.staffRegen = 0;
  if (p.staff) {
    p.staffBonuses = w->elementalBonusPct;
    for (const auto& [e, v] : w->elementalBonusPct)
      if (e == p.staffElement) p.staffBonusPct = v;
    p.staffLifeStealPct = w->lifeStealPct, p.staffRegen = w->regeneration;
  }
  // isArrowAttack: reach weapons never shoot; a weapon shoots when it is ranged; without one the archer classes do
  static const std::vector<std::string> kReach = {"pikeman", "basePikeman", "lancer", "aldric", "sandoval", "sentinel", "templar"};
  static const std::vector<std::string> kCrossbow = {"brigand", "baseCrossbowman", "brigandCrossbowman"};
  static const std::vector<std::string> kArchers = {"archer", "ranger", "assassin"};
  p.arrow = has(kReach, u.classId) ? false : has(kCrossbow, u.classId) ? true : !weaponId.empty() ? (w && w->ranged) : has(kArchers, u.classId);
  // mastery (weaponSkills.ts weaponModifiers for a hero; trainedWeaponSkills from save.heroSkills)
  const Json skills = save.value("heroSkills", Json::object());
  const Json mine = skills.is_object() && skills.contains(u.name) && skills[u.name].is_object() ? skills[u.name] : Json::object();
  auto skillOf = [&](const std::string& type) { return has(cls->weaponTypes, type) && mine.contains(type + "Weapon") ? cleanSkill(mine[type + "Weapon"]) : 0.0; };
  const std::string mainType = w && has(w->usableBy, u.classId) ? w->weaponType : std::string();
  const std::string offType = off && off->kind == "weapon" && (off->usableBy.empty() || has(off->usableBy, u.classId)) ? off->weaponType : std::string();
  auto mastery = [&](const std::string& type, double& accuracy, double& damage) {
    if (type.empty() || !has(cls->weaponTypes, type)) {
      accuracy = 100, damage = 1;
      return;
    }
    const double v = skillOf(type);
    accuracy = 75 + v + u.blessedHitBonusPct * 100, damage = (100 + v) / 100;
  };
  mastery(mainType, p.accuracy, p.damageMul);
  mastery(offType, p.offAccuracy, p.offDamageMul);
  u.weaponType = mainType, u.trainsWeapon = !mainType.empty() && has(cls->weaponTypes, mainType), u.weaponSkill = mainType.empty() ? 0 : skillOf(mainType);
  u.offWeaponType = offType, u.offTrainsWeapon = !offType.empty() && has(cls->weaponTypes, offType), u.offWeaponSkill = offType.empty() ? 0 : skillOf(offType);
  return true;
}

}  // namespace ember::e2
