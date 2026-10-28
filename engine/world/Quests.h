#pragma once

// Ember's Inn quests (src/game/quests.ts) on the world's quests table (content/data/overworld.json tables.quests, written
// by export_overworld.ts from Ember's QUESTS): status and progress, the pickups lying on a mission's map, and what a fight
// adds to the save (GameApp persistVictory: questItems, questKills). Quest data is keyed by mission id, never in the map or
// battle files (Ember's rule: the Map Editor would drop it on Save), so a battle file carries none of it.

#include <string>
#include <vector>

#include "world/Battle.h"
#include "world/Campaign.h"

namespace ember {

// quests.ts questProgress: kill / recruit 0 or 1 of 1, fetch the pickups held of the quest's pickups.
void questProgress(const Json& quest, const Json& save, int& have, int& total);
// quests.ts questStatus: "available" (not accepted), "active", "ready" (every objective met, to hand in) or "done".
std::string questStatus(const Json& quest, const Json& save);
// quests.ts activePickupsFor: the pickups lying on the mission's map now (accepted, unfinished fetch quests; none already held).
std::vector<QuestPickup> activeQuestPickups(const Json& quests, const Json& save, const std::string& missionId);
// The kill quests' target names on this mission, accepted or not (GameApp records every quest target that dies: a dungeon keeps
// its dead dead).
std::vector<std::string> questKillTargets(const Json& quests, const std::string& missionId);
// GameApp persistVictory: the pickups found join save.questItems and the fallen targets save.questKills, each once, order kept.
void foldQuestProgress(Json& save, const std::vector<std::string>& found, const std::vector<std::string>& killed);

// The English of the quests' own words (Ember has none: EmberEngine's own translation, content/data/quests-en.json, loaded
// once): a quest's "title" / "place" / "pickup" / "target" by quest id, a potion's name by id. The Portuguese when missing.
std::string questEnglish(const std::string& questId, const char* field, const std::string& portuguese);
std::string potionEnglish(const std::string& potionId, const std::string& portuguese);

}  // namespace ember
