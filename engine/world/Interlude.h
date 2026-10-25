#pragma once

// The Interlude (docs/INTERLUDE.md): the historical chapter 25 years before the campaign, played in the same city the party later
// explores as the Ruins, by a party of its own (Isolde, young Salazar as a Cruzado, Selene). This is the campaign side of it:
// the Ruins' quest gate, the chapter's three states, the party-context switch (the present party's state put aside while the
// historical one plays, given back whole at the end), Isolde's progression kept for her later recruitment, and the Cooker's
// check of the whole cycle. Everything is read from content/interlude.json; the maps, the party's final builds and the
// quests come from the user later. Nothing here knows how a cutscene plays (MapStage's controller) or how the world map
// draws a pin (OverworldStage): the stages ask these functions and act.
//
// State in the save (SaveData v19 + one object, older saves without it = NOT_STARTED):
//   save.interlude = { "state": "active" | "completed", "mission": N (the index of the chapter's mission in play),
//                      "present": { <the present party's fields, while active> },
//                      "isolde": { "level", "xp", "heroSkills", "statPointAllocations", "promotion" } (once completed),
//                      "isoldeRecruited": bool }
// The party's fields (kPartyKeys) are the ones the campaign writes about heroes and the party's purse / bags / gear / road
// state; campaign-wide fields (completed, flags, quests, the clock, the world position) stay as they are in both eras.

#include <optional>
#include <string>
#include <vector>

#include "world/Campaign.h"

namespace ember::e2 {

class Overworld;

struct InterludeHero {
  std::string name, classId, weapon;
  int level = 1;
  bool persistent = false;  // Isolde: her progression outlives the chapter
  bool temporary = false;   // Selene: never joins the present party
};

struct InterludeData {
  std::string ruinsLocation = "ruins";        // the world-map location whose first entry starts the chapter
  std::vector<std::string> requiredQuests;    // Salazar's quests that must be done (questCompleted) before the Ruins open; empty = locked
  std::vector<InterludeHero> party;           // exactly three
  std::string leader;                         // who walks on free-roam maps
  std::vector<std::string> missions;          // the chapter's maps (mission ids = map ids in content/maps/ember), in order
  std::string persistentHero = "Isolde";

  static constexpr const char* kFormat = "ember-interlude-1";
  bool load(const std::string& file, std::string* why = nullptr);
  bool parse(const Json& j, std::string* why = nullptr);
  bool hasMission(const std::string& id) const;
  const InterludeHero* hero(const std::string& name) const;
};

enum class InterludeState { NotStarted, Active, Completed };

InterludeState interludeState(const Json& save);
const char* interludeStateName(InterludeState s);

// The Ruins' gate: open in Modo teste, with the developer flag "interlude:ruins-open" in save.flags, or once every required
// quest is done; an empty / unconfigured list keeps them shut.
bool ruinsOpen(const InterludeData& data, const Overworld& world, const Json& save, bool test = false);
// Why they are shut (a hint for the world map), empty when open.
std::string ruinsLockedReason(const InterludeData& data, const Overworld& world, const Json& save);

// The chapter's mission in play (its id), empty when not active.
std::string interludeMission(const InterludeData& data, const Json& save);

// Starts the chapter on `save` (NOT_STARTED only): the present party's fields put aside, the historical party's written,
// the first mission pending. Returns false (save untouched) when it cannot start.
bool interludeBegin(const InterludeData& data, Json& save, std::string* why = nullptr);
// A chapter mission is done (won, or its free-roam map left by its waypoint): the next one becomes pending, or the chapter
// completes (interludeComplete). Returns true when `save` changed.
bool interludeAdvance(const InterludeData& data, Json& save, const std::string& missionDone);
// Ends the chapter (ACTIVE only): Isolde's progression kept under interlude.isolde, the present party's fields restored
// whole, the historical heroes' recruitment flags removed, state COMPLETED. Atomic on the JSON: either all of it or nothing.
bool interludeComplete(const InterludeData& data, Json& save, std::string* why = nullptr);
// The present campaign recruits Isolde (her later story event): her Interlude level / XP / skills / points / promotion into the
// party's tables and the recruitment flag; once only (false when already recruited or the chapter never completed).
bool recruitIsolde(const InterludeData& data, Json& save, std::string* why = nullptr);

// The party's fields the context switch moves (public for the check).
const std::vector<std::string>& interludePartyKeys();

// Cooker check-interlude: the whole cycle on synthetic saves (the gate shut when unconfigured, open once the quests are done,
// begin / fight / complete / recruit, an old save). Returns the number of failed checks, each logged.
int checkInterlude(const InterludeData& data, const Overworld& world, const Json& emptySave);

}  // namespace ember::e2
