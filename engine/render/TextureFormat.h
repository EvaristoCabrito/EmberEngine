#pragma once

#include <cstdint>

namespace ember {

// A cooked 2D texture: TextureHeader followed by every mip (largest first), each a row-major grid of
// 16-byte blocks (BC5 and BC7 both use 16 bytes per 4x4 block).

inline constexpr uint32_t kTextureMagic = 0x52584554;  // "TEXR"
inline constexpr uint32_t kTextureFormatVersion = 1;

enum class TextureFormat : uint32_t {
  Bc7Srgb = 0,   // colour
  Bc7Unorm = 1,  // linear data (occlusion / roughness / metallic)
  Bc5Unorm = 2,  // two channels (tangent-space normal X, Y)
};

struct TextureHeader {
  uint32_t magic;
  uint32_t version;
  TextureFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t mipCount;
  uint64_t dataBytes;
};
static_assert(sizeof(TextureHeader) == 32);

constexpr uint64_t textureMipBytes(uint32_t width, uint32_t height, uint32_t mip) {
  const uint32_t w = width >> mip ? width >> mip : 1;
  const uint32_t h = height >> mip ? height >> mip : 1;
  return uint64_t{(w + 3) / 4} * ((h + 3) / 4) * 16;
}

}  // namespace ember
