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
  // Experiment (proxy LOD study): replace every mesh by its voxel remesh at this resolution (4..256),
  // simplified on position error only. Positions and normals only: no UVs, so textures are not mapped.
  // 0 = off (the real meshes). Written as separate assets in a separate archive; never the shipped path.
  int remeshResolution = 0;
};

struct MeshCookReport {
  size_t meshes = 0, cooked = 0, fromCache = 0, failed = 0;
  uint64_t sourceTriangles = 0;   // full detail
  uint64_t dagTriangles = 0;      // every LOD level together
  uint64_t clusters = 0;
  uint64_t geometryBytes = 0;
  uint64_t textures = 0, textureBytes = 0;  // distinct textures in the output
  uint64_t sharedTextureUses = 0;           // further meshes using a texture already in the output
  double seconds = 0.0;
};

// Cooks every GLB (relative paths under options.sourceRoot) into meshes/<path without .glb>.mesh plus its material
// textures, named by content (meshes/textures/<hash>.tex: one asset per distinct image + format, shared by every mesh
// that uses it), and appends each asset to `out` once.
bool cookMeshes(JobSystem& jobs, const std::vector<std::filesystem::path>& sources, const MeshCookOptions& options,
                std::vector<PakSource>& out, MeshCookReport& report);

}  // namespace ember
