#pragma once

// The campaign's save files, ported from Engine2 (src/campaign/save.ts): a bank of six slots, each one SaveData in
// Engine2's own JSON shape (version 19), kept whole so every campaign screen reads the same fields Engine2 does.
// The empty save, the missions in campaign order (with each one's map file) and the world-map locations come from
// Engine2's modules through tools/scripts/engine2/export_campaign.ts (content/engine2/campaign.json).
//
// Engine2 kept the bank in the browser's localStorage ("ember-save-bank"); EmberEngine keeps the same JSON in a file,
// written whole through a temporary file and a rename so a crash or a power cut never leaves half a save.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ember::e2 {

// A save as Engine2 holds it: JSON that keeps its keys in insertion order (JS objects do, and Engine2's rules read
// some of them in that order, e.g. which hero's bag loses a potion on the road).
using Json = nlohmann::ordered_json;

struct CampaignMission {
  std::string id, title, place, briefing, objective, win, mapFile;  // mapFile: the newest saved map for the id, if any
  int index = 0;
  bool own = false;  // a mission only the Map Editor knows (placed in Locais): its map is the editor's newest save
};

struct WorldLocation {
  std::string id, name;
  int col = 0, row = 0;  // its hex on the world map (overworld.ts worldToHex)
  std::vector<std::string> missionIds;
};

// Engine2's campaign tables.
struct CampaignData {
  int saveVersion = 19, slotCount = 6;
  Json emptySave;  // emptySave() with updatedAt 0
  std::vector<CampaignMission> missions;
  std::vector<WorldLocation> locations;
  // As campaign.json ships them (applyLocais starts again from these when the editor's Locais change).
  std::vector<CampaignMission> shippedMissions;
  std::vector<WorldLocation> shippedLocations;
  Json slotProgressCases;  // export_campaign.ts reference answers

  bool load(const std::filesystem::path& file);
  const CampaignMission* mission(const std::string& id) const;
  const WorldLocation* locationAt(int col, int row) const;
};

// save.ts SaveBank: six slots (empty = null), the last one used, sound muted.
struct SaveBank {
  int lastSlot = 0;
  bool muted = false;
  std::vector<std::optional<Json>> slots;
};

// save.ts slotProgress: what a slot card says.
struct SlotProgress {
  std::string title, detail;
};

class Campaign {
 public:
  // `data`: content/engine2/campaign.json; `bankFile`: where the bank lives (default: savePath()).
  bool open(const std::filesystem::path& data, const std::filesystem::path& bankFile);
  // %LOCALAPPDATA%\Ember\ember-save-bank.json (the folder is made when the bank is first written).
  static std::filesystem::path savePath();

  const CampaignData& data() const { return data_; }
  CampaignData& data() { return data_; }  // the Map Editor's Locais re-applied (applyLocais)
  // Set when "Nova campanha" has just written a slot: the campaign's opening films play once (GameApp boot ->
  // vauIntro), then it is cleared by whoever plays them.
  bool freshCampaign = false;
  const SaveBank& bank() const { return bank_; }

  Json emptySave() const;
  // save.ts slotOccupied / hasAnySave / isSlotEmpty / slotProgress / activeSave.
  static bool occupied(const std::optional<Json>& slot);
  bool hasAnySave() const;
  SlotProgress progress(const std::optional<Json>& slot) const;
  Json activeSave() const;

  // save.ts writeSlot / selectSlot: the bank is written to disk at once; false when the file could not be written.
  bool writeSlot(int index, Json save);
  bool selectSlot(int index);
  // The active slot rewritten with `save` (writeSlot on lastSlot).
  bool saveActive(Json save) { return writeSlot(bank_.lastSlot, std::move(save)); }

  // Engine2 formatSaveDate (pt-BR, dateStyle medium + timeStyle short): "15 de out. de 2026, 21:40".
  static std::string formatSaveDate(double updatedAtMs);
  static double nowMs();

 private:
  bool persist();
  CampaignData data_;
  SaveBank bank_;
  std::filesystem::path bankFile_;
};

// The C++ slotProgress against Engine2's answers (Cooker check-campaign). Returns the number of differences.
int checkSlotProgress(const Campaign& campaign);

}  // namespace ember::e2
