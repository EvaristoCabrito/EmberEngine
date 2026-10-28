#pragma once

// Cooker architecture: the wall, door and gate textures Ember's 3D architecture samples (the architectureDefs of
// content/data/editor-catalog.json, images from Ember's public folder), cooked as BC7 sRGB with mips under
// ember::architectureTextureAsset names. The game mounts build/meshes/architecture.pak next to the mesh pack.

#include <filesystem>
#include <vector>

#include "PakWriter.h"
#include "SpriteCooker.h"

namespace ember {

class JobSystem;

struct ArchitectureCookOptions {
  std::filesystem::path catalog, artRoot, cacheDir;
  Bc7Quality quality = Bc7Quality::Slow;
};

bool cookArchitecture(JobSystem& jobs, const ArchitectureCookOptions& options, std::vector<PakSource>& out);

}  // namespace ember
