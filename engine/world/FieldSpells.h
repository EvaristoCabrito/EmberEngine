#pragma once

// The world map's field spells, ported from Engine2 (GameApp castCreateFoodAndWater / castWarpOverworld, data.ts
// createFoodAndWaterPower / createFoodAndWaterFormula / WARP / tierUses, OverworldMapScreen's field-spells menu):
//   Criar Comida e Água (a healer's tier 4): the hero's own hunger bar full (120% from level 10) and 0-2D4+2 rations;
//   Warp (a mage's tier 3 from level 7): the party steps to an explored location, spending one use.
// Both draw on the same save.spellUses pool a battle cast uses. Each cast takes the save and returns the new save, or
// nothing when Engine2 would refuse.

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "world/Campaign.h"
#include "world/Inn.h"
#include "world/Overworld.h"

namespace ember::e2 {

struct FoodPower {
  int dice = 0, faces = 0, bonus = 0, fullness = 100;
};

class FieldSpells {
 public:
  static constexpr int kWarpUnlockLevel = 7;  // WARP.unlockLevel
  static constexpr int kWarpTier = 3;
  static constexpr int kFoodTier = 4;
  // Caçada (ours, docs/SKILLS_ARCHER.md): an archer's tier 2 from level 4: game hunted on the world map = rations.
  static constexpr int kHuntUnlockLevel = 4;
  static constexpr int kHuntTier = 2;
  // The Curandeiro's field spells (ours, docs/SKILLS_HEALER.md): Consagrar Acampamento (tier 2 from level 5: the party to full HP,
  // the tier uses untouched) and Prece do Caminho (tier 3 from level 9: for the next hexes no undead ambush and battles half as likely).
  static constexpr int kCampUnlockLevel = 5;
  static constexpr int kCampTier = 2;
  static constexpr int kPrayerUnlockLevel = 9;
  static constexpr int kPrayerTier = 3;
  static constexpr const char* kPrayerKey = "roadPrayer";  // the save's hexes left under the Prece

  FieldSpells(const InnData& inn, const Overworld& world) : inn_(inn), world_(world) {}

  // data.ts rulesClass: the progression class a promoted or hero-specific class follows.
  static std::string rulesClass(const std::string& classId);
  static FoodPower foodPower(int level, int innFullness = 120);
  static std::string foodFormula(int level, bool english = false);  // "Comida completa (120%) + 1D4+1 Rações"

  // The hero's class id (its promotion, else its own), level, and the uses left of a tier.
  std::string classOf(const Json& save, const std::string& hero) const;
  int levelOf(const Json& save, const std::string& hero) const;
  int usesLeft(const Json& save, const std::string& hero, int tier) const;

  // Who can cast: standing heroes in the party whose rules class is healer / mage (Warp from level 7).
  std::vector<std::string> healers(const Json& save, bool test) const;
  std::vector<std::string> mages(const Json& save, bool test) const;
  // The locations Warp may go to: another hex, explored or with a finished mission.
  std::vector<const MapLocation*> warpCities(const Json& save) const;
  // Whether Criar Comida e Água would do anything (uses left and a hunger bar below the spell's).
  bool canFeed(const Json& save, const std::string& hero) const;

  std::optional<Json> castFood(const Json& save, const std::string& hero, const std::function<double()>& rng) const;
  // Caçada: who can hunt (archers from level 4, standing), the rations it brings at a level ("1D4+1 rações"), the cast.
  static FoodPower huntPower(int level);
  static std::string huntFormula(int level, bool english = false);
  std::vector<std::string> hunters(const Json& save, bool test) const;
  bool canHunt(const Json& save, const std::string& hero) const { return usesLeft(save, hero, kHuntTier) > 0; }
  std::optional<Json> castHunt(const Json& save, const std::string& hero, const std::function<double()>& rng, int* rations = nullptr) const;
  std::optional<Json> castWarp(const Json& save, const std::string& hero, const std::string& cityId) const;
  // Consagrar Acampamento: who can (healers from level 5, standing), whether it would heal anyone, the cast (`healed` = how many).
  std::vector<std::string> campers(const Json& save, bool test) const;
  bool canCamp(const Json& save, bool test) const;
  bool canCamp(const Json& save, const std::string& hero, bool test) const { return usesLeft(save, hero, kCampTier) > 0 && canCamp(save, test); }
  std::optional<Json> castCamp(const Json& save, const std::string& hero, bool test, int* healed = nullptr) const;
  // Prece do Caminho: who can (healers from level 9, standing), the hexes it covers at a level, how many are left on the save, the cast.
  static int prayerHexes(int level);
  static int prayerLeft(const Json& save);
  std::vector<std::string> prayers(const Json& save, bool test) const;
  bool canPray(const Json& save, const std::string& hero) const { return usesLeft(save, hero, kPrayerTier) > 0; }
  std::optional<Json> castPrayer(const Json& save, const std::string& hero) const;

 private:
  const InnData& inn_;
  const Overworld& world_;
};

}  // namespace ember::e2
