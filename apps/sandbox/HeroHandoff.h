#pragma once

// A campaign fight and the save (Engine2 GameApp startBattle's roster and finishBattle's merges): what each hero brings
// into the fight (HP, potions, disease, poison, hunger and the starving penalty) and what the fight leaves in the save.
// Used by the game (main.cpp) and by --roster-check, which compares the heroes it builds with Engine2's own.

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "MapStage.h"
#include "world/Overworld.h"

namespace ember {

// GameApp startBattle: the roster's hp (unitHp), bags, heroDiseases, heroPoisons, heroHunger, and
// hungerPenaltyFor(hungerStreak) for whoever is starving (spawnUnit), for every hero the save names.
inline std::map<std::string, MapStageOptions::HeroState> heroStatesFromSave(const e2::Json& save) {
  std::map<std::string, MapStageOptions::HeroState> out;
  const double penalty = e2::Overworld::hungerPenalty(save.value("hungerStreak", 0.0));
  const e2::Json hp = save.value("unitHp", e2::Json::object()), bags = save.value("bags", e2::Json::object());
  const e2::Json sick = save.value("heroDiseases", e2::Json::object()), poisons = save.value("heroPoisons", e2::Json::object());
  const e2::Json hunger = save.value("heroHunger", e2::Json::object()), levels = save.value("levels", e2::Json::object());
  std::vector<std::string> names;
  for (const e2::Json* table : {&levels, &hp, &bags, &sick, &poisons, &hunger})
    if (table->is_object())
      for (const auto& [name, value] : table->items())
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
  for (const std::string& name : names) {
    MapStageOptions::HeroState& s = out[name];
    if (hp.contains(name) && hp[name].is_number()) s.hp = hp[name].get<int>();
    if (bags.contains(name) && bags[name].is_object()) {
      std::array<int, 7> bag{};
      for (int k = 0; k < 7; ++k) bag[static_cast<size_t>(k)] = bags[name].value(e2::kPotionIds[k], 0);
      s.bag = bag;
    } else if (bags.is_object() && save.contains("bags")) {
      s.starterBag = true;
    }
    s.diseased = sick.contains(name) && sick[name] == true;
    if (poisons.contains(name) && poisons[name].is_string()) s.poison = poisons[name].get<std::string>();
    else if (poisons.contains(name) && !poisons[name].is_null() && poisons[name] != false) s.poison = "lesser";
    s.fullness = e2::Overworld::fullness(hunger.contains(name) ? hunger[name] : e2::Json());  // spawnUnit: always a number
    if (*s.fullness <= 0) s.hungerPenalty = std::min(0.9, std::max(0.0, penalty));
  }
  return out;
}

// finishBattle: each hero's potions left (remainingBags; the bag's lockpicks stay), disease and poison
// (mergeBattleDiseases / mergeBattlePoisons), hunger (battlePlayerHunger), and the rations conjured
// (save.rations + engine.lootRations). Levels, XP and HP are written by the caller.
inline void writeHeroConditions(e2::Json& save, const std::vector<HeroOutcome>& heroes, int lootRations) {
  for (const HeroOutcome& h : heroes) {
    e2::Json& bag = save["bags"][h.name];
    if (!bag.is_object()) bag = e2::Json::object();
    for (int k = 0; k < 7; ++k) bag[e2::kPotionIds[k]] = h.bag[static_cast<size_t>(k)];
    if (h.diseased) save["heroDiseases"][h.name] = true;
    else if (save["heroDiseases"].is_object()) save["heroDiseases"].erase(h.name);
    if (!h.poison.empty()) save["heroPoisons"][h.name] = h.poison;
    else if (save["heroPoisons"].is_object()) save["heroPoisons"].erase(h.name);
    if (h.fullness) save["heroHunger"][h.name] = *h.fullness;
  }
  if (lootRations > 0) save["rations"] = save.value("rations", 0) + lootRations;
}

}  // namespace ember
