#pragma once

#include <filesystem>
#include <vector>

#include "PakWriter.h"
#include "SpriteCooker.h"

namespace ember {

class JobSystem;

struct GroundCookOptions {
  std::filesystem::path boardJson;      // content/data/board.json
  std::filesystem::path artRoot;  // public (read only)
  std::filesystem::path cacheDir;       // cooked textures kept here; unchanged sources are not re-cooked
  std::filesystem::path checkDir;       // optional: Ember's own procedural textures (export_textures.ts)
  Bc7Quality quality = Bc7Quality::Slow;
};

struct GroundCookReport {
  size_t textures = 0, cooked = 0, fromCache = 0, failed = 0;
  uint64_t bytes = 0;
  double seconds = 0;
};

// Cooks every ground texture (world/GroundAssets.h) at its source resolution and appends them to `out`.
// With checkDir set, the procedural surfaces are first compared byte for byte with Ember's.
bool cookGround(JobSystem& jobs, const GroundCookOptions& options, std::vector<PakSource>& out, GroundCookReport& report);

}  // namespace ember
