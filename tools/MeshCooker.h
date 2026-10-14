#pragma once

#include <filesystem>
#include <vector>

#include "PakWriter.h"
#include "SpriteCooker.h"

namespace ember {

class JobSystem;

struct MeshCookOptions {
  std::filesystem::path sourceRoot;  // GLB paths are relative to this; it sets the asset names
  std::filesystem::path cacheDir;    // cooked meshes are kept here; unchanged sources are never re-cooked
  Bc7Quality quality = Bc7Quality::Slow;
};

struct MeshCookReport {
  size_t meshes = 0, cooked = 0, fromCache = 0, failed = 0;
  uint64_t sourceTriangles = 0;   // full detail
  uint64_t dagTriangles = 0;      // every LOD level together
  uint64_t clusters = 0;
  uint64_t geometryBytes = 0;
  uint64_t textures = 0, textureBytes = 0;
  double seconds = 0.0;
};

// Cooks every GLB (relative paths under options.sourceRoot) into meshes/<path without .glb>.mesh plus one
// texture asset per material texture (meshes/<path>/<role><n>.tex), and appends them to `out`.
bool cookMeshes(JobSystem& jobs, const std::vector<std::filesystem::path>& sources, const MeshCookOptions& options,
                std::vector<PakSource>& out, MeshCookReport& report);

}  // namespace ember
