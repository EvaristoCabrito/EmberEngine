#pragma once

// The Map Editor's Locais and R-Encounter tables (Ember src/campaign/mapstore.ts: map-order.json, map-slots.json,
// location-order.json, location-submaps.json, random-encounters.json).
// The places are ONE list (the user, 2026-11-11: "they should never differ thats what im saying from start of this game"):
// content/data/locations.json, each place with its maps (missionIds), its floors (submaps), its planned slots, in campaign
// order. The world map, the campaign, Modo teste's classic map and this editor all read it, and the editor's Salvar writes
// it; there is no other copy (overworld*.json, campaign.json and editor-catalog.json no longer hold places). The
// R-Encounter regions: Ember's from content/data/editor-catalog.json, EmberEngine's edits in content/maps/random-encounters.json.

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "world/Campaign.h"  // Json

namespace ember {

struct LocaisFloor {
  std::string missionId;
  int floor = 0;
};

// The one floor order (the user, 2026-11-17: "não faz sentido nenhum começar a torre ou qualquer dungeon em qualquer andar que
// não seja térreo, depois é primeiro segundo e terceiro, seguido dos subsolos"): the ground floor (0), then 1, 2, 3 upward, then
// the basements -1, -2, -3 downward. Every list of floors (the place list, the Map Editor, Modo teste) goes in this order.
inline int floorRank(int floor) { return floor >= 0 ? floor : 100000 - floor; }
inline bool floorBefore(int a, int b) { return floorRank(a) < floorRank(b); }
inline void sortFloors(std::vector<LocaisFloor>& floors) {
  std::stable_sort(floors.begin(), floors.end(), [](const LocaisFloor& a, const LocaisFloor& b) { return floorBefore(a.floor, b.floor); });
}

struct EncounterRegion {
  std::string id, name;
  std::vector<std::string> encounterIds;
};

struct Locais {
  // Ember's ALL_LOCATIONS (each location's missions as Ember ships them) and DEFAULT_LOCATION_SUBMAPS.
  struct Base {
    std::string id, name;
    std::vector<std::string> missionIds;
  };
  std::vector<Base> base;
  std::map<std::string, std::vector<LocaisFloor>> defaultSubmaps;

  // The editable state (LocaisLocal): missions per location, planned slots, the locations' campaign order, submaps.
  std::map<std::string, std::vector<std::string>> order;
  std::map<std::string, int> slots;
  std::vector<std::string> locationOrder;
  std::map<std::string, std::vector<LocaisFloor>> submaps;
  std::optional<std::vector<std::string>> knownMissionIds;
  bool own = false;  // the place list was read (content/data/locations.json)

  std::vector<EncounterRegion> regions;  // R-Encounter
  bool ownRegions = false;

  static std::filesystem::path file() { return "content/data/locations.json"; }  // the one place list
  static std::filesystem::path regionsFile() { return "content/maps/random-encounters.json"; }

  // The places from file() (the one list); the R-Encounter regions from the editor catalog, EmberEngine's own file over them.
  static Locais load(const std::filesystem::path& catalogFile);
  // Writes the editor's state into file() itself: each place's maps, floors and slots, the places in locationOrder; every
  // other field of a place (its pin, its hex, its flags) is kept. False when the write failed.
  bool save();
  bool saveRegions();

  // mapstore locationsForOrder: every location in campaign order with its missions (submap floors left out).
  std::vector<Base> locations() const;
  // The editor's opening order (MapEditorScreen: the saved copy through locationsForOrder, else ALL_LOCATIONS).
  std::map<std::string, std::vector<std::string>> editorOrder() const;
  int slotsFor(const std::string& location) const;
};

// The campaign follows the place list: each location's missions and the locations' order. New missions (ids the campaign
// does not know: maps made in the editor) are added from their newest saved file in `savedMaps`.
void applyLocais(CampaignData& data, const Locais& locais, const std::filesystem::path& savedMaps);

}  // namespace ember
