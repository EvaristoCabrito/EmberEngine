#pragma once

// A hero's weapons as the save has them, into a fight (Engine2 GameApp startBattle's roster -> BattleEngine spawnUnit): the main
// hand (save.equipped, its enhancement save.weapons[id]; the class's starter when none or one the class cannot wield), the off
// hand (save.equipment[hero].offHand; none beside a two-handed weapon), the reach the weapon gives, its staff magic, and the
// mastery the save's weapon skills give (save.heroSkills[hero]["<type>Weapon"]). Data: content/engine2/weapons.json
// (tools/scripts/engine2/export_weapons.ts).

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "world/Battle.h"
#include "world/Campaign.h"

namespace ember::e2 {

struct WeaponDef {
  std::string id, weaponType, bonusClass;
  std::vector<std::string> usableBy;
  int dice = 0, faces = 0, bonus = 0, minRange = 1, maxRange = 1;
  bool ranged = false, twoHanded = false;
  bool magic = false;  // MageStaffMagic
  std::string attackElement;
  std::vector<std::pair<std::string, double>> elementalBonusPct;
  double lifeStealPct = 0, regeneration = 0;
};
struct OffHandDef {
  std::string id, kind, weaponType;  // kind: "weapon" / "shield"
  std::vector<std::string> usableBy;
  int minRange = 1, maxRange = 1, dice = 1, faces = 4, bonus = 0;
  double dmgMul = 0.75;
};
struct ClassArms {
  std::string starter;                   // starterWeaponFor ("" = none)
  std::vector<std::string> weaponTypes;  // weaponTypesForClass
  int minRange = 1, maxRange = 1;        // the class's own reach (statsFor), used without a weapon
};

class Loadouts {
 public:
  bool load(const std::filesystem::path& file);
  const WeaponDef* weapon(const std::string& id) const;
  const OffHandDef* offHand(const std::string& id) const;
  const ClassArms* arms(const std::string& classId) const;
  bool mageRangeBonus(const std::string& classId) const;

 private:
  std::map<std::string, WeaponDef> weapons_;
  std::map<std::string, OffHandDef> offHand_;
  std::map<std::string, ClassArms> classes_;
  std::vector<std::string> mageRangeBonus_;
};

// spawnUnit's weapon part on a hero already in a battle: its weapons from `save`, reach, staff magic, the arrow / arcane basic
// attack, and both hands' mastery. Returns false (the unit untouched) when the class is unknown.
bool equipFromSave(BattleUnit& u, const Loadouts& arms, const Json& save);

}  // namespace ember::e2
