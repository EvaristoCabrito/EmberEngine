#pragma once

#include <filesystem>

namespace ember {

// Loads a battle exported by tools/scripts/data/export_battle.ts and recomputes every unit's movement reach with
// EmberEngine's port (world/Battle.h); compares each with Ember's own (same hexes, costs and parents). True when
// every unit matches.
bool checkBattle(const std::filesystem::path& battleJson);

}  // namespace ember
