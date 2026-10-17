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
  std::optional<Json> castWarp(const Json& save, const std::string& hero, const std::string& cityId) const;

 private:
  const InnData& inn_;
  const Overworld& world_;
};

}  // namespace ember::e2
