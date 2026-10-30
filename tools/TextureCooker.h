#pragma once

#include <cstdint>
#include <vector>

#include "SpriteCooker.h"
#include "render/TextureFormat.h"

namespace ember {

class JobSystem;

struct Rgba8 {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> pixels;  // RGBA8, row-major
};

// Builds the whole mip chain and block-compresses it. Mips are filtered the way each format is read:
//   Bc7Srgb   colour averaged in linear light, alpha linear
//   Bc7Unorm  plain average (data channels)
//   Bc5Unorm  tangent-space normals (R, G = X, Y): averaged as vectors and renormalised
// Returns the cooked texture asset (TextureHeader + data). Spread over every core.
std::vector<uint8_t> cookTexture(JobSystem& jobs, const Rgba8& image, TextureFormat format, Bc7Quality quality);

// The 3D objects' textures are stored at most this size (the user, 2026-10-29: Alta 2K, Média 1K, Leve 512, all from one file).
constexpr uint32_t kMeshTextureMaxSize = 2048;
// Halves both sides (one power of two, the aspect kept) until the larger is at most maxSize, filtered (Mitchell) the way the format
// is read: colour in linear light, data plain, normals as vectors renormalised. Smaller images come back unchanged.
Rgba8 capTextureSize(Rgba8 image, TextureFormat format, uint32_t maxSize);

}  // namespace ember
