#pragma once

#include <cstdint>

#include "assets/AssetId.h"

namespace ember {

// Cooked sprite data, shared by the cooker and the runtime. All little-endian.
//
// Per clip, three assets in the archive:
//   sprites/<set>/<clip>.clip   SpriteClipHeader: placement, timing, texture layout
//   sprites/<set>/<clip>.tail   BC7 blocks of the small mips (largest side <= kSpriteTailSize), always resident
//   sprites/<set>/<clip>.full   BC7 blocks of the whole mip chain, streamed on demand
// and, for a clip cooked with relief (tools/scripts/sprites/relief.py), two more with the same layout and byte
// counts, linear BC7: R, G = tangent-space normal x, y; B = occlusion; A = height (1 = nearest the viewer):
//   sprites/<set>/<clip>.rtail / .rfull
// plus one index for everything: sprites/roster (SpriteRosterHeader + AssetId[clipCount]).
// Version 2 appended the relief assets to the header; version 1 headers (192 bytes) still load, without relief.
//
// Texture data is mip-major: for each mip (largest first), every layer (frame) in order, each layer a
// row-major grid of 16-byte BC7 blocks. A mip's layers are contiguous, so one copy region per mip
// uploads all frames.

inline constexpr uint32_t kSpriteClipMagic = 0x50494C43;    // "CLIP"
inline constexpr uint32_t kSpriteRosterMagic = 0x52545352;  // "RSTR"
inline constexpr uint32_t kSpriteFormatVersion = 2;
inline constexpr uint32_t kSpriteFormatVersion1 = 1;  // no relief fields (192-byte header)
inline constexpr uint32_t kSpriteTailSize = 64;  // largest side of the always-resident version
inline constexpr uint32_t kBc7BlockBytes = 16;

enum class SpritePlayMode : uint32_t {
  Loop = 0,      // 0,1,..,n-1,0,1,..
  PingPong = 1,  // 0,1,..,n-1,n-2,..,1,0,..
  Once = 2,      // 0,1,..,n-1 then holds the last frame
};

struct SpriteClipHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t frames;     // array layers
  uint32_t width;      // texture size in pixels, a multiple of 4
  uint32_t height;
  uint32_t mipCount;   // full chain, down to 1x1
  uint32_t tailFirstMip;  // first mip stored in the .tail asset (its largest side <= kSpriteTailSize)
  uint32_t contentWidth;  // trimmed picture inside the texture (the rest is padding to block size)
  uint32_t contentHeight;
  uint32_t sourceWidth;   // the original frame the art was drawn in
  uint32_t sourceHeight;
  int32_t cropX;          // where the trimmed picture sits inside the original frame
  int32_t cropY;
  SpritePlayMode mode;
  float fps;
  uint32_t reserved;  // keeps the 64-bit fields aligned with no hidden padding
  AssetId fullAsset;
  AssetId tailAsset;
  uint64_t fullBytes;     // GPU bytes of the full chain
  uint64_t tailBytes;
  char pack[48];          // zero-terminated
  char name[48];
  // Version 2: the relief chain (same mips, layers and bytes as the colour); 0 = the clip has no relief.
  AssetId reliefFullAsset;
  AssetId reliefTailAsset;
};
static_assert(sizeof(SpriteClipHeader) == 208);
inline constexpr size_t kSpriteClipHeaderV1Bytes = 192;

struct SpriteRosterHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t clipCount;
  uint32_t reserved;
};

// Followed by clipCount AssetIds of .clip assets: every clip the archive holds, so the runtime can
// make every tail resident at start.

// Bytes of one mip level (all layers) of a BC7 array texture.
constexpr uint64_t bc7MipBytes(uint32_t width, uint32_t height, uint32_t mip, uint32_t layers) {
  const uint32_t w = width >> mip ? width >> mip : 1;
  const uint32_t h = height >> mip ? height >> mip : 1;
  return uint64_t{(w + 3) / 4} * ((h + 3) / 4) * kBc7BlockBytes * layers;
}

}  // namespace ember
