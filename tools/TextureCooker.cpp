#include "TextureCooker.h"

#include <basisu_bc7e_scalar.h>
#include <rgbcx.h>
#include <stb_image_resize2.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

#include "core/Jobs.h"

namespace ember {

namespace {

constexpr uint32_t kBlocksPerTask = 256;

float srgbToLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
float linearToSrgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f; }
uint8_t toByte(float v) { return static_cast<uint8_t>(std::clamp(std::lround(v * 255.0f), 0l, 255l)); }

// Next mip: each output texel averages the 2x2 (or 2x1 / 1x2 at odd edges) texels it covers.
Rgba8 halve(const Rgba8& in, TextureFormat format) {
  Rgba8 out;
  out.width = std::max(1u, in.width / 2);
  out.height = std::max(1u, in.height / 2);
  out.pixels.resize(size_t{out.width} * out.height * 4);
  static float decodeSrgb[256];
  static std::once_flag once;
  std::call_once(once, [] {
    for (int i = 0; i < 256; ++i) decodeSrgb[i] = srgbToLinear(i / 255.0f);
  });
  for (uint32_t y = 0; y < out.height; ++y)
    for (uint32_t x = 0; x < out.width; ++x) {
      float sum[4] = {};
      int n = 0;
      for (uint32_t dy = 0; dy < 2; ++dy)
        for (uint32_t dx = 0; dx < 2; ++dx) {
          const uint32_t sx = std::min(x * 2 + dx, in.width - 1), sy = std::min(y * 2 + dy, in.height - 1);
          const uint8_t* p = &in.pixels[(size_t{sy} * in.width + sx) * 4];
          for (int c = 0; c < 4; ++c) {
            float v = p[c] / 255.0f;
            if (format == TextureFormat::Bc7Srgb && c < 3) v = decodeSrgb[p[c]];
            else if (format == TextureFormat::Bc5Unorm && c < 2) v = v * 2.0f - 1.0f;
            sum[c] += v;
          }
          ++n;
        }
      uint8_t* o = &out.pixels[(size_t{y} * out.width + x) * 4];
      for (int c = 0; c < 4; ++c) sum[c] /= static_cast<float>(n);
      if (format == TextureFormat::Bc5Unorm) {
        // Average of unit vectors, renormalised (Z rebuilt from X, Y in the shader).
        const float z2 = std::max(0.0f, 1.0f - sum[0] * sum[0] - sum[1] * sum[1]);
        const float len = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + z2);
        const float nx = len > 0.0f ? sum[0] / len : 0.0f, ny = len > 0.0f ? sum[1] / len : 0.0f;
        o[0] = toByte(nx * 0.5f + 0.5f);
        o[1] = toByte(ny * 0.5f + 0.5f);
        o[2] = 255;
        o[3] = 255;
      } else {
        for (int c = 0; c < 4; ++c)
          o[c] = toByte(format == TextureFormat::Bc7Srgb && c < 3 ? linearToSrgb(sum[c]) : sum[c]);
      }
    }
  return out;
}

}  // namespace

Rgba8 capTextureSize(Rgba8 image, TextureFormat format, uint32_t maxSize) {
  uint32_t width = image.width, height = image.height;
  while (std::max(width, height) > maxSize) width = std::max(1u, width / 2), height = std::max(1u, height / 2);
  if (width == image.width && height == image.height) return image;
  Rgba8 out;
  out.width = width;
  out.height = height;
  out.pixels.resize(size_t{width} * height * 4);
  stbir_resize(image.pixels.data(), static_cast<int>(image.width), static_cast<int>(image.height), static_cast<int>(image.width * 4),
               out.pixels.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width * 4), STBIR_RGBA,
               format == TextureFormat::Bc7Srgb ? STBIR_TYPE_UINT8_SRGB : STBIR_TYPE_UINT8, STBIR_EDGE_WRAP, STBIR_FILTER_MITCHELL);
  if (format == TextureFormat::Bc5Unorm)
    for (size_t i = 0; i < out.pixels.size(); i += 4) {
      // Filtered normals are shorter than one: renormalise with the source's own Z (blue).
      const float x = out.pixels[i] / 127.5f - 1.0f, y = out.pixels[i + 1] / 127.5f - 1.0f, z = out.pixels[i + 2] / 127.5f - 1.0f;
      const float len = std::sqrt(x * x + y * y + z * z);
      if (len <= 0.0f) continue;
      out.pixels[i] = toByte(x / len * 0.5f + 0.5f);
      out.pixels[i + 1] = toByte(y / len * 0.5f + 0.5f);
      out.pixels[i + 2] = toByte(z / len * 0.5f + 0.5f);
    }
  return out;
}

std::vector<uint8_t> cookTexture(JobSystem& jobs, const Rgba8& image, TextureFormat format, Bc7Quality quality) {
  static std::once_flag once;
  std::call_once(once, [] {
    bc7e_scalar::bc7e_compress_block_init();
    rgbcx::init();
  });
  bc7e_scalar::bc7e_compress_block_params params;
  const bool perceptual = format == TextureFormat::Bc7Srgb;
  switch (quality) {
    case Bc7Quality::Fast: bc7e_scalar::bc7e_compress_block_params_init_veryfast(&params, perceptual); break;
    case Bc7Quality::Basic: bc7e_scalar::bc7e_compress_block_params_init_basic(&params, perceptual); break;
    case Bc7Quality::Slow: bc7e_scalar::bc7e_compress_block_params_init_slow(&params, perceptual); break;
  }

  uint32_t mipCount = 1;
  while ((std::max(image.width, image.height) >> mipCount) > 0) ++mipCount;
  uint64_t dataBytes = 0;
  for (uint32_t m = 0; m < mipCount; ++m) dataBytes += textureMipBytes(image.width, image.height, m);

  std::vector<uint8_t> out(sizeof(TextureHeader) + dataBytes);
  const TextureHeader header{kTextureMagic, kTextureFormatVersion, format, image.width, image.height, mipCount, dataBytes};
  std::memcpy(out.data(), &header, sizeof(header));

  Rgba8 level = image;
  uint64_t offset = sizeof(TextureHeader);
  for (uint32_t m = 0; m < mipCount; ++m) {
    if (m > 0) level = halve(level, format);
    const uint32_t blocksX = (level.width + 3) / 4, blocksY = (level.height + 3) / 4, blocks = blocksX * blocksY;
    uint8_t* dst = out.data() + offset;
    const uint32_t tasks = (blocks + kBlocksPerTask - 1) / kBlocksPerTask;
    jobs.parallelFor(tasks, 1, [&](uint32_t begin, uint32_t end) {
      std::vector<uint32_t> texels(size_t{kBlocksPerTask} * 16);
      for (uint32_t t = begin; t < end; ++t) {
        const uint32_t first = t * kBlocksPerTask, count = std::min(kBlocksPerTask, blocks - first);
        // Texels outside the image repeat the edge (never sampled).
        for (uint32_t b = 0; b < count; ++b) {
          const uint32_t bx = (first + b) % blocksX, by = (first + b) / blocksX;
          for (uint32_t py = 0; py < 4; ++py)
            for (uint32_t px = 0; px < 4; ++px) {
              const uint32_t x = std::min(bx * 4 + px, level.width - 1), y = std::min(by * 4 + py, level.height - 1);
              std::memcpy(&texels[size_t{b} * 16 + py * 4 + px], &level.pixels[(size_t{y} * level.width + x) * 4], 4);
            }
        }
        uint8_t* blockOut = dst + size_t{first} * 16;
        if (format == TextureFormat::Bc5Unorm) {
          for (uint32_t b = 0; b < count; ++b)
            rgbcx::encode_bc5_hq(blockOut + size_t{b} * 16, reinterpret_cast<const uint8_t*>(&texels[size_t{b} * 16]), 0, 1, 4);
        } else {
          bc7e_scalar::bc7e_compress_blocks(count, reinterpret_cast<uint64_t*>(blockOut), texels.data(), &params);
        }
      }
    });
    offset += textureMipBytes(image.width, image.height, m);
  }
  return out;
}

}  // namespace ember
