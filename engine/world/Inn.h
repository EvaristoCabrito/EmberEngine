#pragma once

// The Inn's rules, ported from Ember's GameApp Inn handlers (src/game/GameApp.tsx InnScreen props), hunger.ts
// buyInnMeal and inventory.ts equipSharedWeapon / unequipSharedWeapon: meals, the Adega's potions and lockpicks,
// rations, a night's rest, the healer's Cura Média and cures, Vargan's weapons and gear (buy, enhance, sell, equip),
// the NPCs' quests (talk, accept, hand in). Each action takes the save and returns the new save, or nothing when Ember
// would refuse. Prices and item tables come from tools/scripts/data/export_inn.ts (content/data/inn.json).

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "world/Campaign.h"
#include "world/Overworld.h"

namespace ember {

struct InnPotion {
  std::string id, label, tooltip, effect;  // effect: heal / mana / disease
  int price = 0, carryMax = 5, dice = 0, faces = 0, bonus = 0, manaRestore = 0;
};
struct InnWeapon {
  std::string id, name, bonusClass, bonusClassName, magic, dice, range, icon;
  int price = 0;
  bool questOnly = false;
  std::optional<int> recommendedLevel;
  double power = 0;
  bool twoHanded = false;
  int magBonus = 0, dexBonus = 0;              // a staff's MAG / DEX while its class wields it (weapon.magic)
  std::vector<std::string> usableBy, tooltip;  // tooltip per enhancement 0..max
  std::vector<int> sell;                       // sell value per enhancement 0..max
  std::string weaponType;                      // ours: "sword", "bow", ... (content/data/weapons.json), the unknown item's only word
};
struct InnGear {
  std::string id, name, slot, typeSlot, tooltip, icon, kind, summary;  // kind: "", "weapon" or "shield" (off-hand)
  int price = 0, hp = 0, atk = 0, mag = 0, def = 0, dex = 0, mov = 0, dice = 0, faces = 0;  // the worn stats (EQUIPMENT)
  double dmgMul = 0.75;
  bool pouch = false;
  std::optional<std::vector<std::string>> usableBy;
  std::string weaponType;  // ours: an off-hand weapon's type ("dagger"; content/data/weapons.json offHand)
};

struct InnData {
  int mealPrice = 2, innFullness = 120, restPrice = 8, healerCastPrice = 5, healerAilmentPrice = 7, lockpickPrice = 6, rationsPrice = 3;
  int bagMax = 5, rationStackMax = 30, weaponMaxEnh = 5;
  std::vector<int> enhCost;  // cost of rank 1..max
  std::vector<InnPotion> potions;
  std::string lockpickTooltip, rationsIcon, bagIcon;
  std::vector<InnWeapon> weapons;
  std::vector<InnGear> equipment;
  std::map<std::string, std::string> classNames;
  std::vector<std::pair<std::string, std::string>> equipmentSlots;  // id, label (EQUIPMENT_SLOTS)
  std::map<std::string, std::vector<std::vector<int>>> tierUses;  // [class][level - 1][tier - 1]

  bool load(const std::filesystem::path& file);
  const InnPotion* potion(const std::string& id) const;
  const InnWeapon* weapon(const std::string& id) const;
  const InnGear* gear(const std::string& id) const;
};

class InnRules {
 public:
  InnRules(const InnData& data, const Overworld& world) : data_(data), world_(world) {}

  // The class the Inn and the inventory read for a hero (GameApp DEFAULT_HEROES / InventoryScreens HERO_BASE_CLASS:
  // kaelFinal, neera, voss, salazar, aldric, conjurer), a promotion first.
  std::string heroClass(const Json& save, const std::string& hero) const;
  // GameApp mapStatusUnit: max HP (class at the hero's level + gear, less hunger when the hero's bar is empty), HP now.
  int statusMaxHp(const Json& save, const std::string& hero) const;
  int statusHp(const Json& save, const std::string& hero) const;
  bool diseased(const Json& save, const std::string& hero) const;
  bool poisoned(const Json& save, const std::string& hero) const;

  std::optional<Json> buyMeal(const Json& save, const std::string& hero) const;  // hunger.ts buyInnMeal
  std::optional<Json> pay(const Json& save, const std::string& hero, const std::map<std::string, int>& cart, int lockpicks) const;
  std::optional<Json> buyRations(const Json& save, int qty) const;
  // A night: everyone in `heroes` pays, a day passes (08:00 on the travel clock), 75 % of lost HP back, spells restored.
  std::optional<Json> passNight(const Json& save, const std::vector<std::string>& heroes, int& healed) const;
  std::optional<Json> healerCast(const Json& save, const std::string& hero, int& amount) const;  // Cura Média: 25 % max HP
  std::optional<Json> cureAilments(const Json& save, const std::vector<std::string>& heroes, int& count) const;
  std::optional<Json> buyWeapon(const Json& save, const std::string& weaponId) const;
  std::optional<Json> buyEquipment(const Json& save, const std::string& itemId) const;
  std::optional<Json> upgradeWeapon(const Json& save, const std::string& weaponId) const;
  std::optional<Json> sellWeapon(const Json& save, const std::string& weaponId, int& value) const;
  std::optional<Json> equipWeapon(const Json& save, const std::string& hero, const std::string& weaponId) const;  // "" = unequip
  // --- the inventory (inventory.ts, GameApp's Mochila handlers) ---
  static bool fitsSlot(const InnGear& item, const std::string& slot);  // equipmentFitsSlot
  // ... for this hero: a hero with arms of her own (world/Loadout.h heroArms; Selene) takes a piece of her types in "mainHand"
  static bool fitsHeroSlot(const InnGear& item, const std::string& hero, const std::string& slot);
  bool offHandBlocked(const Json& save, const std::string& hero) const;  // a two-handed main weapon
  // equipSharedItem (itemId) / unequipSharedItem ("" : the piece back into the stash, if the bag has room).
  std::optional<Json> equipItem(const Json& save, const std::string& hero, const std::string& slot, const std::string& itemId) const;
  std::optional<Json> discardWeapon(const Json& save, const std::string& weaponId) const;
  std::optional<Json> discardEquipment(const Json& save, const std::string& itemId, bool unknownCopy = false) const;
  std::optional<Json> discardRation(const Json& save) const;
  std::optional<Json> discardBagItem(const Json& save, const std::string& hero, const std::string& kind) const;
  // useHeroPotion: heal (dice + healing skill, capped), mana (spent tier uses back), disease (cure); healing may train.
  std::optional<Json> usePotion(const Json& save, const std::string& hero, const std::string& kind, const std::function<double()>& rng) const;
  // GameApp consumeRation: hunger.ts useRation, the party's hunger streak cleared once everyone has eaten.
  std::optional<Json> useRation(const Json& save, const std::string& hero) const;
  // data.ts gearStatBonus: what a hero's worn pieces (and a staff's magic, for its class) add to each stat, and the lines
  // the status sheet names ("Capuz +2": the piece and its amount) for one stat.
  struct GearBonus {
    int hp = 0, atk = 0, mag = 0, def = 0, dex = 0, mov = 0;
  };
  GearBonus gearBonus(const Json& save, const std::string& hero, const std::string& classId) const;
  std::vector<std::string> gearLines(const Json& save, const std::string& hero, const std::string& stat) const;  // stat: hp atk mag def dex mov
  int bagUsed(const Json& save) const;      // partyBagUsed
  int bagCapacity(const Json& save) const;  // partyBagCapacity

  // --- Itens desconhecidos (ours, 2026-10-21, docs/SKILLS_MAGE.md): part of the chest loot comes unknown (content/unknown-items.json) and
  // shows only its type ("Espada desconhecida", "Poção desconhecida"), cannot be equipped or drunk, sells cheap at the Inn, and is revealed
  // for good by Identificar (the Mago Negro's field spell). The marks: save.unknownWeapons [ids] (each also in save.weapons),
  // save.unknownEquipment {id: copies} (among save.looseEquipment's), save.unknownPotions {hero: {kind: count}} (apart from the bags). ---
  struct UnknownItem {
    std::string kind;   // "weapon" | "gear" | "potion"
    std::string id;     // the weapon's / gear's id, the potion's kind
    std::string hero;   // a potion's holder
    std::string label, labelEn;  // what the player sees ("Espada desconhecida")
    std::string realName;        // what Identificar reveals
    int sell = 0;                // what the Inn pays for it unknown
  };
  std::vector<UnknownItem> unknownItems(const Json& save) const;
  bool weaponUnknown(const Json& save, const std::string& weaponId) const;
  int gearUnknown(const Json& save, const std::string& itemId) const;  // the unknown copies among looseEquipment[itemId]
  int potionsUnknown(const Json& save, const std::string& hero, const std::string& kind) const;
  std::optional<Json> sellUnknown(const Json& save, const UnknownItem& item, int& value) const;
  Json identify(const Json& save, const UnknownItem& item) const;  // the mark removed (a potion goes into the hero's bag)
  Json discardUnknown(const Json& save, const UnknownItem& item) const;

  Json talkToNpc(const Json& save, const std::string& npc) const;
  bool questOffered(const Json& save, const std::string& questId) const;
  std::optional<Json> acceptQuest(const Json& save, const std::string& questId) const;
  std::optional<Json> turnInQuest(const Json& save, const std::string& questId) const;
  // quests.ts questStatus / questProgress.
  std::string questStatus(const Json& save, const Json& quest) const;
  void questProgress(const Json& save, const Json& quest, int& have, int& total) const;
  std::vector<const Json*> questsFor(const std::string& npc) const;

 private:
  int ember(const Json& save) const;
  const InnData& data_;
  const Overworld& world_;
};

}  // namespace ember
