#pragma once

// Cooker architecture: the wall, door and gate textures Ember's 3D architecture samples (the architectureDefs of
// content/engine2/editor-catalog.json, images from Engine2's public folder), cooked as BC7 sRGB with mips under
// e2::architectureTextureAsset names. The game mounts build/meshes/architecture.pak next to the mesh pack.

#include <filesystem>
#include <vector>

#include "PakWriter.h"
#include "SpriteCooker.h"

namespace ember {

class JobSystem;

struct ArchitectureCookOptions {
  std::filesystem::path catalog, engine2Public, cacheDir;
  Bc7Quality quality = Bc7Quality::Slow;
};

bool cookArchitecture(JobSystem& jobs, const ArchitectureCookOptions& options, std::vector<PakSource>& out);

}  // namespace ember
