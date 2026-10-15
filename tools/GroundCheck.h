#pragma once

#include <filesystem>

namespace ember {

// Builds a map's board, both ground looks and the water in C++ and compares them with Engine2's own
// output for the same map (exported by tools/scripts/engine2/export_board.ts into <reference root>/<map id>):
// cells, vertex positions, normals, blend weights, indices, placement heights. True when they match.
bool checkGround(const std::filesystem::path& boardJson, const std::filesystem::path& mapJson, const std::filesystem::path& engine2Public,
                 const std::filesystem::path& referenceRoot);

}  // namespace ember
