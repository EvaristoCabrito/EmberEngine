#pragma once

// The campaign's world map (Ember's RPG overworld), ported from src/campaign/overworld.ts with the rules it reads
// (access.ts, progression.ts, quests.ts, hunger.ts, poison.ts, skills.ts and the loot / bag helpers of ember/data.ts).
// The board, the locations, the missions and every table come from tools/scripts/data/export_overworld.ts
// (content/data/overworld.json); Cooker check-overworld compares this port with Ember's own answers, step for
// step and random number for random number.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "world/Campaign.h"

namespace ember {

// The Overworld's painting, the one picture every screen draws it with: the RPG map, Modo teste's classic map and the title's
// preload (the user, 2026-10-28: "mudar em tudo"). v04 = mordavia_mapa_completo_4096_v10 (2026-10-29; v03 = its v3, kept); the pins are in percent of it.
inline constexpr const char* kWorldMapPicture = "content/worldmap/world-map-v04.jpg";

class Mulberry32;

struct OverworldCell {
  int col = 0, row = 0;
  int hours = 6;                         // travelHoursForHex
  std::string region;                    // travelRegionAt
  std::vector<std::string> encounters;   // travelEncounterIds
  double x = 0, y = 0;                   // hexToWorld: percent of the world-map image
};

struct MapLocation {
  std::string id, name;
  double x = 0, y = 0;  // percent of the world-map image
  int col = 0, row = 0;
  std::vector<std::string> missionIds;
  bool openAccess = false, encountersAllowed = false, warpCity = false;
  std::optional<double> deadlineDay;
};

struct MapMission {
  std::string id, title, win;
  int index = 0;
  bool hub = false, crossing = false, gated = false;
  bool keepsSpawns = false;  // mapstore keepsDefeatedSpawns: a dungeon, enterable again once done
};

// A travel event (overworld.ts OverworldEvent): kind "encounter" / "hungry" / "battle" (battle: missionId to launch).
struct OverworldEvent {
  std::string kind, text, missionId;
};

struct OverworldHex {
  int col = 0, row = 0;
  bool operator==(const OverworldHex&) const = default;
};

struct Locais;
struct CampaignData;

class Overworld {
 public:
  bool load(const std::filesystem::path& file);
  // The Map Editor's saved Locais (missions per location, campaign order) and R-Encounter regions (each travel cell's
  // encounters by its region), over the shipped tables; missions only the editor knows come from `campaign`.
  void applyLocais(const Locais& locais, const CampaignData& campaign);

  // --- the board ---------------------------------------------------------------------------------------------------
  const std::vector<OverworldCell>& cells() const { return cells_; }
  const OverworldCell* cell(int col, int row) const;
  bool isCell(int col, int row) const { return cell(col, row) != nullptr; }
  static std::vector<OverworldHex> neighbors(int col, int row);  // pathfinding.ts hexNeighbors (odd-r)
  static void hexToWorld(int col, int row, double& x, double& y);
  OverworldHex startHex() const { return start_; }
  std::optional<OverworldHex> wispHex() const { return wisp_; }

  const std::vector<MapLocation>& locations() const { return locations_; }
  const MapLocation* locationAt(int col, int row) const;  // the later of two on one hex
  const MapMission* mission(const std::string& id) const;
  std::vector<const MapMission*> missionsOf(const MapLocation& location) const;  // missionsForLocation

  // --- rules -------------------------------------------------------------------------------------------------------
  bool heroRecruited(const std::string& hero, const Json& save) const;
  int maxHp(const Json& save, const std::string& hero) const;  // statsFor(class, level).hp, 0 without a class
  int classHp(const std::string& classId, int level) const;    // statsFor(classId, level).hp
  bool wispForestUncleared(const Json& save) const;
  // `test` everywhere below: Ember's Modo teste (GameApp testMode): every hex walkable, every hero counts as in the
  // party, every mission and location open.
  bool canStep(const Json& save, OverworldHex from, OverworldHex to, bool test = false) const;  // canStepOverworld
  // stepOverworld: the save after one hex step and what happened on the way; the save unchanged (and no event, no
  // random number) when the step is not allowed.
  Json step(const Json& save, OverworldHex to, Mulberry32& rng, std::optional<OverworldEvent>& event, bool test = false) const;
  // teleportOverworld (Modo teste only): straight to any hex, no adjacency check, no day / ration / HP cost.
  static Json teleport(const Json& save, OverworldHex to);

  // progression.ts / access.ts
  int currentChapter(const Json& save) const;
  std::string missionAccess(const std::string& id, const Json& save, bool test = false) const;  // hidden / revealed / available / completed
  std::string missionStatus(const std::string& id, const Json& save, const std::vector<std::string>& order, bool test = false) const;  // locked / available / done
  std::string locationStatus(const MapLocation& location, const Json& save, bool test = false) const;
  std::vector<const MapMission*> visibleMissionsOf(const MapLocation& location, const Json& save, bool test = false) const;
  bool locationVisible(const MapLocation& location, const Json& save, bool test = false) const;  // mapVisibleLocations

  // Small helpers the map screen shows.
  static std::string travelTimeLabel(int hours);
  static double travelHungerCost(int hours) { return 50.0 * hours / 24.0; }
  static double fullness(const Json& value);  // hunger.ts fullness: 0..120, 100 when unset
  static double hungerPenalty(double streak);  // hunger.ts hungerPenaltyFor: a starving hero's stat loss
  static int campaignHour(const Json& save);
  static std::string timeOfDay(int hour);     // campaignTime.ts campaignTimeOfDay

  const Json& tables() const { return tables_; }
  // data.ts partyBagHasRoom (save, extra, test): test mode counts all six heroes' bags.
  bool bagHasRoom(const Json& save, int extra, bool test = false) const;
  // progression.ts evaluate: a quest's availability, a gate's condition.
  bool conditionHolds(const Json& condition, const Json& save) const { return evaluate(condition, save, false); }

 private:
  std::vector<MapLocation> shippedLocations_;
  std::vector<MapMission> shippedMissions_;
  std::vector<std::vector<std::string>> shippedEncounters_;
  struct LootEntry {
    bool weapon = true;
    std::string id, name;
    double price = 0;
    int power = 1;
    bool starting = false;
  };
  double classResistance(const std::string& classId, const std::string& element) const;
  double gearResistance(const Json& save, const std::string& hero, const std::string& element) const;
  int missionIndex(const std::string& id) const;
  bool evaluate(const Json& condition, const Json& save, bool storedChapter) const;
  int questStage(const std::string& questId, const Json& save) const;
  bool lockedMission(const std::string& id, const Json& save, const std::vector<std::string>& order, bool test) const;
  const MapLocation* previousPopulatedLocation(const MapLocation& location) const;

  std::vector<OverworldCell> cells_;
  std::vector<MapLocation> locations_;
  std::vector<MapMission> missions_;
  OverworldHex start_{}, inn_{}, ashenBlocked_{};
  std::optional<OverworldHex> wisp_;
  int westEdgeCol_ = 0;
  std::vector<std::string> stoneBridgeMissionIds_;
  std::vector<LootEntry> loot_;  // weapons then equipment, in Ember's table order
  Json tables_;
  // The road encounters that field undead (content/data/undead-encounters.json, beside the world data; tools/scripts/data/
  // undead_encounters.py): never rolled while the Prece do Caminho holds (world/FieldSpells, docs/SKILLS_HEALER.md).
  std::set<std::string> undeadEncounters_;
};

// Cooker check-overworld: location / mission status, allowed steps and seeded steps against Ember's answers.
// Returns the number of differences.
int checkOverworld(const Overworld& world, const std::filesystem::path& references);

}  // namespace ember
