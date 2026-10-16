#pragma once

// The Map Editor's Locais and R-Encounter tables (Engine2 src/campaign/mapstore.ts: map-order.json, map-slots.json,
// location-order.json, location-submaps.json, random-encounters.json and the browser's "ember-locais-local" copy that
// wins over them). Engine2's shipped tables come through content/engine2/editor-catalog.json; EmberEngine's own edits
// are written to content/maps/locais.json and content/maps/random-encounters.json (Engine2's folder is never
// written). When the editor has saved Locais, the campaign and the world map play that order (locationsForOrder), as
// Engine2's game does once its Locais were saved.

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "world/Campaign.h"  // Json

namespace ember::e2 {

struct LocaisFloor {
  std::string missionId;
  int floor = 0;
};

struct EncounterRegion {
  std::string id, name;
  std::vector<std::string> encounterIds;
};

struct Locais {
  // Engine2's ALL_LOCATIONS (each location's missions as Engine2 ships them) and DEFAULT_LOCATION_SUBMAPS.
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
  bool own = false;  // content/maps/locais.json exists (the editor saved Locais)

  std::vector<EncounterRegion> regions;  // R-Encounter
  bool ownRegions = false;

  static std::filesystem::path file() { return "content/maps/locais.json"; }
  static std::filesystem::path regionsFile() { return "content/maps/random-encounters.json"; }

  // Engine2's tables from the editor catalog, then EmberEngine's own files over them.
  static Locais load(const std::filesystem::path& catalogFile);
  // saveLocaisLocal (stamps knownMissionIds with every mission Engine2's base names). False when the write failed.
  bool save();
  bool saveRegions();

  // mapstore locationsForOrder: every location in campaign order with its missions (submap floors left out).
  std::vector<Base> locations() const;
  // The editor's opening order (MapEditorScreen: the saved copy through locationsForOrder, else ALL_LOCATIONS).
  std::map<std::string, std::vector<std::string>> editorOrder() const;
  int slotsFor(const std::string& location) const;
};

// The world map and the campaign follow the saved Locais (no-op when the editor never saved them): each location's
// missions replaced and the locations put in the saved order. New missions (ids the campaign does not know: maps made
// in the editor) are added from their newest saved file in `savedMaps`.
void applyLocais(CampaignData& data, const Locais& locais, const std::filesystem::path& savedMaps);

}  // namespace ember::e2
