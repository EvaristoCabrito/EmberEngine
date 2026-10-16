#include "SpriteCooker.h"

#include <basisu_bc7e_scalar.h>
#include <bc7decomp.h>
#include <stb_image.h>
#include <stb_image_write.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <charconv>
#include <cstring>
#include <fstream>
#include <mutex>

#include <nlohmann/json.hpp>

#include "core/Jobs.h"
#include "core/Log.h"

namespace ember {

namespace {

// Bump whenever cooked pixels change for the same input: every clip then re-cooks.
constexpr uint32_t kCookerVersion = 1;
// Grid-sheet cells (FrameSpec) are part of the key through their frame names; bump this when their slicing changes.
constexpr uint32_t kCellVersion = 3;
// Bump whenever cooked relief pixels change for the same input (the relief chain has its own cache next to the colour's).
constexpr uint32_t kReliefVersion = 1;
constexpr uint32_t kTrimMargin = 4;      // pixels kept around the visible area (room for filtering)
constexpr uint32_t kBlocksPerTask = 2048;  // BC7 blocks encoded per job

struct Rgba8Image {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> pixels;  // RGBA, straight alpha, sRGB colour
};

// Everything about a cooked clip that depends only on its pixels. Stored next to the cooked
// textures, so timing and pack changes rewrite the small .clip header without re-cooking.
struct CookedLayout {
  uint64_t key;
  uint32_t frames, width, height, mipCount, tailFirstMip;
  uint32_t contentWidth, contentHeight, sourceWidth, sourceHeight;
  int32_t cropX, cropY;
  uint64_t fullBytes, tailBytes;
  double colorPsnr, alphaPsnr;
};

uint64_t fnv(uint64_t hash, const void* data, size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
  return hash;
}

// A frame as the manifest names it (SpriteSource.h): a file, optionally one cell of a grid sheet ("#CxR:i", cells
// row by row) and optionally lit from black ("!black": art on a black background, Engine2 deriveAlphaFromBlack).
struct FrameSpec {
  std::string file;
  uint32_t cols = 1, rows = 1, cell = 0;
  bool alphaFromBlack = false;
};

FrameSpec parseFrame(const std::string& frame) {
  FrameSpec spec;
  std::string rest = frame;
  if (rest.size() > 6 && rest.ends_with("!black")) {
    spec.alphaFromBlack = true;
    rest.resize(rest.size() - 6);
  }
  const size_t hash = rest.rfind('#');
  if (hash != std::string::npos) {
    uint32_t c = 0, r = 0, i = 0;
    const char* p = rest.data() + hash + 1;
    const char* end = rest.data() + rest.size();
    auto number = [&](uint32_t& v, char after) {
      const auto [next, error] = std::from_chars(p, end, v);
      if (error != std::errc{} || (after && (next == end || *next != after))) return false;
      p = after ? next + 1 : next;
      return true;
    };
    if (number(c, 'x') && number(r, ':') && number(i, 0) && p == end && c > 0 && r > 0 && i < c * r) {
      spec.cols = c, spec.rows = r, spec.cell = i;
      rest.resize(hash);
    }
  }
  spec.file = rest;
  return spec;
}

// Loads one frame as straight-alpha RGBA8: the file, its cell, its alpha.
bool loadFrame(const std::filesystem::path& root, const std::string& frame, Rgba8Image& out) {
  const FrameSpec spec = parseFrame(frame);
  int w = 0, h = 0, n = 0;
  uint8_t* data = stbi_load((root / spec.file).string().c_str(), &w, &h, &n, 4);
  if (!data) return false;
  // Cells start at floor(i * size / count) and all have the same size (floor(size / count)), so every frame keeps its
  // place when a sheet's size does not divide evenly (at most a pixel of a cell's edge is left out).
  const uint32_t cx = spec.cell % spec.cols, cy = spec.cell / spec.cols;
  const uint32_t x0 = cx * uint32_t(w) / spec.cols, x1 = x0 + uint32_t(w) / spec.cols;
  const uint32_t y0 = cy * uint32_t(h) / spec.rows, y1 = y0 + uint32_t(h) / spec.rows;
  out.width = x1 - x0;
  out.height = y1 - y0;
  out.pixels.resize(size_t{out.width} * out.height * 4);
  for (uint32_t y = 0; y < out.height; ++y)
    std::memcpy(&out.pixels[size_t{y} * out.width * 4], &data[(size_t{y + y0} * uint32_t(w) + x0) * 4], size_t{out.width} * 4);
  stbi_image_free(data);
  if (spec.alphaFromBlack) {
    // Alpha = the brightest channel x 4 (Engine2 used x 1, which left a dark arrow see-through over light ground);
    // colour divided by it, so drawn premultiplied it gives back exactly the art over black (and adds exactly the
    // art when drawn additively, as glows are).
    for (size_t i = 0; i < out.pixels.size(); i += 4) {
      uint8_t* p = &out.pixels[i];
      const uint8_t a = static_cast<uint8_t>(std::min(255, 4 * std::max({p[0], p[1], p[2]})));
      for (int c = 0; c < 3; ++c) p[c] = a ? static_cast<uint8_t>(std::min(255, (p[c] * 255 + a / 2) / a)) : 0;
      p[3] = a;
    }
  }
  return true;
}

uint64_t textureKey(const SpriteClipSource& clip, const SpriteCookOptions& options) {
  uint64_t hash = 0xcbf29ce484222325ull;
  hash = fnv(hash, &kCookerVersion, sizeof(kCookerVersion));
  hash = fnv(hash, &options.quality, sizeof(options.quality));
  for (const std::string& frame : clip.frames) {
    const FrameSpec spec = parseFrame(frame);
    const std::filesystem::path file = options.sourceRoot / spec.file;
    if (spec.cols * spec.rows > 1 || spec.alphaFromBlack) hash = fnv(hash, &kCellVersion, sizeof(kCellVersion));
    std::error_code error;
    const uint64_t size = std::filesystem::file_size(file, error);
    const auto time = std::filesystem::last_write_time(file, error).time_since_epoch().count();
    hash = fnv(hash, frame.data(), frame.size());
    hash = fnv(hash, &size, sizeof(size));
    hash = fnv(hash, &time, sizeof(time));
  }
  return hash;
}

// Gives every fully transparent pixel the colour of the nearest visible art (push-pull over a
// pyramid). Alpha is untouched. Filtering and mips can then never pull black or white into edges.
// `mask`, when given, says which pixels are art (its alpha > 0) instead of the pixels' own alpha: the relief
// maps carry height in alpha, so they are dilated by the colour frame's coverage.
void dilate(uint8_t* pixels, uint32_t width, uint32_t height, const uint8_t* mask = nullptr) {
  struct Level {
    uint32_t w, h;
    std::vector<float> sums;  // r*weight, g*weight, b*weight, weight
  };
  std::vector<Level> levels;
  levels.push_back({width, height, std::vector<float>(size_t{width} * height * 4)});
  bool anyVisible = false, anyHidden = false;
  const uint8_t* coverage = mask ? mask : pixels;
  for (size_t i = 0; i < size_t{width} * height; ++i) {
    float* s = &levels[0].sums[i * 4];
    if (coverage[i * 4 + 3] > 0) {
      s[0] = pixels[i * 4];
      s[1] = pixels[i * 4 + 1];
      s[2] = pixels[i * 4 + 2];
      s[3] = 1.0f;
      anyVisible = true;
    } else {
      anyHidden = true;
    }
  }
  if (!anyVisible || !anyHidden) return;

  // Push: each level sums its four children.
  while (levels.back().w > 1 || levels.back().h > 1) {
    const Level& child = levels.back();
    Level parent{(child.w + 1) / 2, (child.h + 1) / 2, {}};
    parent.sums.assign(size_t{parent.w} * parent.h * 4, 0.0f);
    for (uint32_t y = 0; y < child.h; ++y)
      for (uint32_t x = 0; x < child.w; ++x) {
        const float* c = &child.sums[(size_t{y} * child.w + x) * 4];
        float* p = &parent.sums[(size_t{y / 2} * parent.w + x / 2) * 4];
        for (int k = 0; k < 4; ++k) p[k] += c[k];
      }
    levels.push_back(std::move(parent));
  }

  // Pull: empty pixels take their parent's average colour, from the top down.
  for (size_t l = levels.size() - 1; l-- > 0;) {
    Level& level = levels[l];
    const Level& parent = levels[l + 1];
    for (uint32_t y = 0; y < level.h; ++y)
      for (uint32_t x = 0; x < level.w; ++x) {
        float* s = &level.sums[(size_t{y} * level.w + x) * 4];
        if (s[3] > 0.0f) continue;
        const float* p = &parent.sums[(size_t{y / 2} * parent.w + x / 2) * 4];
        for (int k = 0; k < 3; ++k) s[k] = p[k] / p[3];
        s[3] = 1.0f;
      }
  }
  const Level& top = levels[0];
  for (size_t i = 0; i < size_t{width} * height; ++i) {
    if (coverage[i * 4 + 3] > 0) continue;
    const float* s = &top.sums[i * 4];
    for (int k = 0; k < 3; ++k) pixels[i * 4 + k] = static_cast<uint8_t>(std::clamp(s[k] / s[3] + 0.5f, 0.0f, 255.0f));
  }
}

// The relief maps keep every channel (height in alpha) where there is no art: the nearest art's values, so BC7
// blocks and mips at the edge never mix in the void. Push-pull on all four channels.
void dilateRelief(uint8_t* pixels, uint32_t width, uint32_t height, const uint8_t* mask) {
  std::vector<uint8_t> swapped(pixels, pixels + size_t{width} * height * 4);
  // Dilate x, y, occlusion with the mask; then height, moved into a colour slot.
  dilate(pixels, width, height, mask);
  for (size_t i = 0; i < size_t{width} * height; ++i) swapped[i * 4] = pixels[i * 4 + 3];
  dilate(swapped.data(), width, height, mask);
  for (size_t i = 0; i < size_t{width} * height; ++i) pixels[i * 4 + 3] = swapped[i * 4];
}

void bc7Params(Bc7Quality quality, bc7e_scalar::bc7e_compress_block_params& params, bool perceptual = true) {
  static std::once_flag once;
  std::call_once(once, [] { bc7e_scalar::bc7e_compress_block_init(); });
  switch (quality) {
    case Bc7Quality::Fast: bc7e_scalar::bc7e_compress_block_params_init_veryfast(&params, perceptual); break;
    case Bc7Quality::Basic: bc7e_scalar::bc7e_compress_block_params_init_basic(&params, perceptual); break;
    case Bc7Quality::Slow: bc7e_scalar::bc7e_compress_block_params_init_slow(&params, perceptual); break;
  }
}

// Squared error over visible pixels, for PSNR.
struct ErrorSums {
  double color = 0.0, alpha = 0.0;
  uint64_t pixels = 0;
};

// Encodes blocks [first, first+count) of one mip image (row-major block order) into `out`.
// Pixels outside the image repeat the edge, as the GPU never samples them.
void encodeBlocks(const Rgba8Image& image, uint32_t first, uint32_t count, const bc7e_scalar::bc7e_compress_block_params& params,
                  uint8_t* out, ErrorSums* errors) {
  const uint32_t blocksX = (image.width + 3) / 4;
  std::vector<uint32_t> texels(size_t{count} * 16);
  for (uint32_t b = 0; b < count; ++b) {
    const uint32_t bx = (first + b) % blocksX, by = (first + b) / blocksX;
    for (uint32_t py = 0; py < 4; ++py)
      for (uint32_t px = 0; px < 4; ++px) {
        const uint32_t x = std::min(bx * 4 + px, image.width - 1), y = std::min(by * 4 + py, image.height - 1);
        std::memcpy(&texels[size_t{b} * 16 + py * 4 + px], &image.pixels[(size_t{y} * image.width + x) * 4], 4);
      }
  }
  bc7e_scalar::bc7e_compress_blocks(count, reinterpret_cast<uint64_t*>(out), texels.data(), &params);
  if (!errors) return;

  // Compare what the GPU will decode with the source, as the player sees it: premultiplied colour.
  bc7decomp::color_rgba decoded[16];
  for (uint32_t b = 0; b < count; ++b) {
    bc7decomp::unpack_bc7(out + size_t{b} * kBc7BlockBytes, decoded);
    const uint32_t bx = (first + b) % blocksX, by = (first + b) / blocksX;
    for (uint32_t i = 0; i < 16; ++i) {
      const uint32_t x = bx * 4 + i % 4, y = by * 4 + i / 4;
      if (x >= image.width || y >= image.height) continue;
      const uint8_t* s = reinterpret_cast<const uint8_t*>(&texels[size_t{b} * 16 + i]);
      const bc7decomp::color_rgba& d = decoded[i];
      if (s[3] == 0 && d.a == 0) continue;
      for (int c = 0; c < 3; ++c) {
        const double e = (s[c] * s[3] - d.m_comps[c] * d.a) / 255.0;
        errors->color += e * e;
      }
      const double ea = static_cast<double>(s[3]) - d.a;
      errors->alpha += ea * ea;
      ++errors->pixels;
    }
  }
}

double psnr(double squaredError, uint64_t samples) {
  if (samples == 0 || squaredError <= 0.0) return 99.0;
  return 10.0 * std::log10(255.0 * 255.0 / (squaredError / static_cast<double>(samples)));
}

bool writeFile(const std::filesystem::path& path, const void* data, size_t size) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  return static_cast<bool>(file);
}

bool readLayout(const std::filesystem::path& path, CookedLayout& out) {
  std::ifstream file(path, std::ios::binary);
  return file && file.read(reinterpret_cast<char*>(&out), sizeof(out)) && file.gcount() == sizeof(out);
}

std::string assetBase(const std::string& set, const std::string& clip) { return normalizeAssetPath("sprites/" + set + "/" + clip); }

// Loads one file per frame (a plain RGBA8 image, no cell or alpha rules), for the relief maps.
bool loadPlainImage(const std::filesystem::path& file, Rgba8Image& out) {
  int w = 0, h = 0, n = 0;
  uint8_t* data = stbi_load(file.string().c_str(), &w, &h, &n, 4);
  if (!data) return false;
  out.width = static_cast<uint32_t>(w);
  out.height = static_cast<uint32_t>(h);
  out.pixels.assign(data, data + size_t{out.width} * out.height * 4);
  stbi_image_free(data);
  return true;
}

// Puts every frame of a clip on one canvas: frames drawn at different sizes are placed on the largest one,
// anchored bottom-centre (where a character's feet are). `sourceW/H` is that canvas.
void placeOnCanvas(std::vector<Rgba8Image>& frames, uint32_t& sourceW, uint32_t& sourceH, bool& resized) {
  sourceW = sourceH = 0;
  resized = false;
  for (const Rgba8Image& frame : frames) {
    sourceW = std::max(sourceW, frame.width);
    sourceH = std::max(sourceH, frame.height);
  }
  for (Rgba8Image& frame : frames) {
    if (frame.width == sourceW && frame.height == sourceH) continue;
    resized = true;
    std::vector<uint8_t> canvas(size_t{sourceW} * sourceH * 4, 0);
    const uint32_t ox = (sourceW - frame.width) / 2, oy = sourceH - frame.height;
    for (uint32_t y = 0; y < frame.height; ++y)
      std::memcpy(&canvas[(size_t{y + oy} * sourceW + ox) * 4], &frame.pixels[size_t{y} * frame.width * 4], size_t{frame.width} * 4);
    frame = {sourceW, sourceH, std::move(canvas)};
  }
}

// Loads every frame of a clip onto its canvas (loadFrame + placeOnCanvas).
bool loadClipFrames(JobSystem& jobs, const std::string& label, const SpriteClipSource& clip, const SpriteCookOptions& options,
                    std::vector<Rgba8Image>& frames, uint32_t& sourceW, uint32_t& sourceH) {
  const size_t frameCount = clip.frames.size();
  frames.assign(frameCount, {});
  std::atomic<bool> loadFailed{false};
  jobs.parallelFor(static_cast<uint32_t>(frameCount), 1, [&](uint32_t begin, uint32_t end) {
    for (uint32_t i = begin; i < end; ++i) {
      if (!loadFrame(options.sourceRoot, clip.frames[i], frames[i])) {
        logError("{}: cannot read {} ({})", label, (options.sourceRoot / clip.frames[i]).string(), stbi_failure_reason());
        loadFailed = true;
      }
    }
  });
  if (loadFailed) return false;
  bool resized = false;
  placeOnCanvas(frames, sourceW, sourceH, resized);
  if (resized) logWarn("{}: frames are drawn at different sizes; placed on one {}x{} canvas, feet aligned", label, sourceW, sourceH);
  return true;
}

// Cooks one clip's textures into <cache>/<asset>.full / .tail and its layout into .layout.
bool cookClipTextures(JobSystem& jobs, const std::string& setName, const SpriteClipSource& clip,
                      const SpriteCookOptions& options, uint64_t key, CookedLayout& layout) {
  const std::string label = setName + "/" + clip.name;
  const size_t frameCount = clip.frames.size();

  // 1. Load every frame onto one canvas.
  std::vector<Rgba8Image> frames;
  uint32_t sourceW = 0, sourceH = 0;
  if (!loadClipFrames(jobs, label, clip, options, frames, sourceW, sourceH)) return false;

  // 2. Trim to the union of visible pixels over the whole clip, so frames keep their places.
  uint32_t x0 = sourceW, y0 = sourceH, x1 = 0, y1 = 0;
  for (const Rgba8Image& frame : frames)
    for (uint32_t y = 0; y < sourceH; ++y)
      for (uint32_t x = 0; x < sourceW; ++x)
        if (frame.pixels[(size_t{y} * sourceW + x) * 4 + 3] > 0) {
          x0 = std::min(x0, x); y0 = std::min(y0, y);
          x1 = std::max(x1, x); y1 = std::max(y1, y);
        }
  if (x0 > x1) {  // nothing visible at all
    x0 = y0 = 0;
    x1 = sourceW - 1;
    y1 = sourceH - 1;
  }
  x0 = x0 > kTrimMargin ? x0 - kTrimMargin : 0;
  y0 = y0 > kTrimMargin ? y0 - kTrimMargin : 0;
  x1 = std::min(sourceW - 1, x1 + kTrimMargin);
  y1 = std::min(sourceH - 1, y1 + kTrimMargin);
  const uint32_t contentW = x1 - x0 + 1, contentH = y1 - y0 + 1;
  const uint32_t width = (contentW + 3) & ~3u, height = (contentH + 3) & ~3u;
  uint32_t mipCount = 1;
  while ((width >> mipCount) > 0 || (height >> mipCount) > 0) ++mipCount;
  uint32_t tailFirstMip = 0;
  while (std::max(std::max(1u, width >> tailFirstMip), std::max(1u, height >> tailFirstMip)) > kSpriteTailSize) ++tailFirstMip;

  // 3. Per frame: crop into the texture, dilate, build every mip.
  std::vector<std::vector<Rgba8Image>> mips(frameCount);
  jobs.parallelFor(static_cast<uint32_t>(frameCount), 1, [&](uint32_t begin, uint32_t end) {
    for (uint32_t f = begin; f < end; ++f) {
      std::vector<Rgba8Image>& chain = mips[f];
      chain.resize(mipCount);
      Rgba8Image& base = chain[0];
      base.width = width;
      base.height = height;
      base.pixels.assign(size_t{width} * height * 4, 0);
      for (uint32_t y = 0; y < contentH; ++y)
        std::memcpy(&base.pixels[size_t{y} * width * 4], &frames[f].pixels[(size_t{y + y0} * sourceW + x0) * 4], size_t{contentW} * 4);
      frames[f].pixels = {};
      dilate(base.pixels.data(), width, height);
      for (uint32_t m = 1; m < mipCount; ++m) {
        Rgba8Image& mip = chain[m];
        mip.width = std::max(1u, width >> m);
        mip.height = std::max(1u, height >> m);
        mip.pixels.resize(size_t{mip.width} * mip.height * 4);
        // From the top level each time (sharper than chaining), alpha-weighted in linear light.
        stbir_resize(base.pixels.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width * 4),
                     mip.pixels.data(), static_cast<int>(mip.width), static_cast<int>(mip.height), static_cast<int>(mip.width * 4),
                     STBIR_RGBA, STBIR_TYPE_UINT8_SRGB, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL);
        dilate(mip.pixels.data(), mip.width, mip.height);
      }
    }
  });

  // 4. BC7, spread over every core: tasks of up to kBlocksPerTask blocks of one mip of one frame.
  std::vector<uint64_t> mipOffset(mipCount + 1, 0);
  for (uint32_t m = 0; m < mipCount; ++m) mipOffset[m + 1] = mipOffset[m] + bc7MipBytes(width, height, m, static_cast<uint32_t>(frameCount));
  std::vector<uint8_t> data(mipOffset[mipCount]);
  struct Task {
    uint32_t frame, mip, first, count;
  };
  std::vector<Task> tasks;
  for (uint32_t m = 0; m < mipCount; ++m) {
    const uint32_t blocks = ((std::max(1u, width >> m) + 3) / 4) * ((std::max(1u, height >> m) + 3) / 4);
    for (uint32_t f = 0; f < frameCount; ++f)
      for (uint32_t first = 0; first < blocks; first += kBlocksPerTask)
        tasks.push_back({f, m, first, std::min(kBlocksPerTask, blocks - first)});
  }
  bc7e_scalar::bc7e_compress_block_params params;
  bc7Params(options.quality, params);
  std::vector<ErrorSums> errors(tasks.size());
  jobs.parallelFor(static_cast<uint32_t>(tasks.size()), 1, [&](uint32_t begin, uint32_t end) {
    for (uint32_t t = begin; t < end; ++t) {
      const Task& task = tasks[t];
      const uint64_t layerBytes = bc7MipBytes(width, height, task.mip, 1);
      uint8_t* out = data.data() + mipOffset[task.mip] + task.frame * layerBytes + uint64_t{task.first} * kBc7BlockBytes;
      encodeBlocks(mips[task.frame][task.mip], task.first, task.count, params, out, task.mip == 0 ? &errors[t] : nullptr);
    }
  });
  ErrorSums total;
  for (const ErrorSums& e : errors) {
    total.color += e.color;
    total.alpha += e.alpha;
    total.pixels += e.pixels;
  }

  // 5. Write the full chain, the tail and the layout.
  const std::filesystem::path base = options.cacheDir / assetBase(setName, clip.name);
  const uint64_t tailOffset = mipOffset[tailFirstMip];
  layout = {key, static_cast<uint32_t>(frameCount), width, height, mipCount, tailFirstMip, contentW, contentH,
            sourceW, sourceH, static_cast<int32_t>(x0), static_cast<int32_t>(y0), data.size(), data.size() - tailOffset,
            psnr(total.color, total.pixels * 3), psnr(total.alpha, total.pixels)};
  return writeFile(std::filesystem::path(base).concat(".full"), data.data(), data.size()) &&
         writeFile(std::filesystem::path(base).concat(".tail"), data.data() + tailOffset, data.size() - tailOffset) &&
         writeFile(std::filesystem::path(base).concat(".layout"), &layout, sizeof(layout));
}

// --- Relief (tools/scripts/sprites/relief.py): a second chain with the colour's layout -------------------------

std::vector<std::filesystem::path> reliefFiles(const SpriteClipSource& clip, const SpriteCookOptions& options) {
  std::vector<std::filesystem::path> files;
  files.reserve(clip.frames.size());
  for (const std::string& frame : clip.frames) files.push_back(options.reliefDir / spriteReliefPath(frame));
  return files;
}

// The relief chain's cache key: the colour key (crop and canvas come from it), the relief files and the version.
uint64_t reliefKey(uint64_t colourKey, const std::vector<std::filesystem::path>& files) {
  uint64_t hash = fnv(0xcbf29ce484222325ull, &kReliefVersion, sizeof(kReliefVersion));
  hash = fnv(hash, &colourKey, sizeof(colourKey));
  for (const std::filesystem::path& file : files) {
    std::error_code error;
    const uint64_t size = std::filesystem::file_size(file, error);
    const auto time = std::filesystem::last_write_time(file, error).time_since_epoch().count();
    hash = fnv(hash, &size, sizeof(size));
    hash = fnv(hash, &time, sizeof(time));
  }
  return hash;
}

// Cooks a clip's relief maps into <cache>/<asset>.rfull / .rtail (the colour chain's layout: same crop, mips and
// bytes) and the key into .rkey. The colour frames give the coverage the relief is dilated by.
bool cookClipRelief(JobSystem& jobs, const std::string& setName, const SpriteClipSource& clip, const SpriteCookOptions& options,
                    const CookedLayout& layout, const std::vector<std::filesystem::path>& files, uint64_t key) {
  const std::string label = setName + "/" + clip.name;
  const size_t frameCount = clip.frames.size();
  std::vector<Rgba8Image> colour, relief(frameCount);
  uint32_t sourceW = 0, sourceH = 0;
  if (!loadClipFrames(jobs, label, clip, options, colour, sourceW, sourceH)) return false;
  if (sourceW != layout.sourceWidth || sourceH != layout.sourceHeight) {
    logError("{}: relief: the frames no longer match the cooked colour ({}x{} vs {}x{})", label, sourceW, sourceH, layout.sourceWidth,
             layout.sourceHeight);
    return false;
  }
  std::atomic<bool> failed{false};
  jobs.parallelFor(static_cast<uint32_t>(frameCount), 1, [&](uint32_t begin, uint32_t end) {
    for (uint32_t i = begin; i < end; ++i)
      if (!loadPlainImage(files[i], relief[i])) {
        logError("{}: relief: cannot read {}", label, files[i].string());
        failed = true;
      }
  });
  if (failed) return false;
  // Each relief map is the size of its own frame (before the canvas); place them the same way.
  bool resized = false;
  uint32_t rw = 0, rh = 0;
  placeOnCanvas(relief, rw, rh, resized);
  if (rw != sourceW || rh != sourceH) {
    logError("{}: relief maps are {}x{}, the frames {}x{}: regenerate them (relief.py)", label, rw, rh, sourceW, sourceH);
    return false;
  }

  const uint32_t width = layout.width, height = layout.height, mipCount = layout.mipCount;
  const uint32_t contentW = layout.contentWidth, contentH = layout.contentHeight;
  const uint32_t x0 = static_cast<uint32_t>(layout.cropX), y0 = static_cast<uint32_t>(layout.cropY);

  // Per frame: crop both, dilate the relief by the colour's coverage, build every mip the same way.
  std::vector<std::vector<Rgba8Image>> mips(frameCount);
  jobs.parallelFor(static_cast<uint32_t>(frameCount), 1, [&](uint32_t begin, uint32_t end) {
    for (uint32_t f = begin; f < end; ++f) {
      Rgba8Image mask, base;
      mask.width = base.width = width;
      mask.height = base.height = height;
      mask.pixels.assign(size_t{width} * height * 4, 0);
      base.pixels.assign(size_t{width} * height * 4, 0);
      for (uint32_t y = 0; y < contentH; ++y) {
        std::memcpy(&mask.pixels[size_t{y} * width * 4], &colour[f].pixels[(size_t{y + y0} * sourceW + x0) * 4], size_t{contentW} * 4);
        std::memcpy(&base.pixels[size_t{y} * width * 4], &relief[f].pixels[(size_t{y + y0} * sourceW + x0) * 4], size_t{contentW} * 4);
      }
      colour[f].pixels = {};
      relief[f].pixels = {};
      dilate(mask.pixels.data(), width, height);
      dilateRelief(base.pixels.data(), width, height, mask.pixels.data());
      std::vector<Rgba8Image>& chain = mips[f];
      chain.resize(mipCount);
      chain[0] = base;
      for (uint32_t m = 1; m < mipCount; ++m) {
        Rgba8Image& mip = chain[m];
        mip.width = std::max(1u, width >> m);
        mip.height = std::max(1u, height >> m);
        mip.pixels.resize(size_t{mip.width} * mip.height * 4);
        // Linear data, no alpha weighting (alpha is height); the coverage mip comes from the colour, as the colour cook makes it.
        stbir_resize(base.pixels.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width * 4),
                     mip.pixels.data(), static_cast<int>(mip.width), static_cast<int>(mip.height), static_cast<int>(mip.width * 4),
                     STBIR_4CHANNEL, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL);
        Rgba8Image maskMip;
        maskMip.width = mip.width;
        maskMip.height = mip.height;
        maskMip.pixels.resize(size_t{mip.width} * mip.height * 4);
        stbir_resize(mask.pixels.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width * 4),
                     maskMip.pixels.data(), static_cast<int>(mip.width), static_cast<int>(mip.height), static_cast<int>(mip.width * 4),
                     STBIR_RGBA, STBIR_TYPE_UINT8_SRGB, STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL);
        dilateRelief(mip.pixels.data(), mip.width, mip.height, maskMip.pixels.data());
      }
    }
  });

  // BC7 (not perceptual: these are vectors and heights), the colour chain's block order.
  std::vector<uint64_t> mipOffset(mipCount + 1, 0);
  for (uint32_t m = 0; m < mipCount; ++m) mipOffset[m + 1] = mipOffset[m] + bc7MipBytes(width, height, m, static_cast<uint32_t>(frameCount));
  std::vector<uint8_t> data(mipOffset[mipCount]);
  if (data.size() != layout.fullBytes) {
    logError("{}: relief: layout mismatch ({} vs {} bytes)", label, data.size(), layout.fullBytes);
    return false;
  }
  struct Task {
    uint32_t frame, mip, first, count;
  };
  std::vector<Task> tasks;
  for (uint32_t m = 0; m < mipCount; ++m) {
    const uint32_t blocks = ((std::max(1u, width >> m) + 3) / 4) * ((std::max(1u, height >> m) + 3) / 4);
    for (uint32_t f = 0; f < frameCount; ++f)
      for (uint32_t first = 0; first < blocks; first += kBlocksPerTask)
        tasks.push_back({f, m, first, std::min(kBlocksPerTask, blocks - first)});
  }
  bc7e_scalar::bc7e_compress_block_params params;
  bc7Params(options.quality == Bc7Quality::Slow ? Bc7Quality::Basic : options.quality, params, false);
  jobs.parallelFor(static_cast<uint32_t>(tasks.size()), 1, [&](uint32_t begin, uint32_t end) {
    for (uint32_t t = begin; t < end; ++t) {
      const Task& task = tasks[t];
      const uint64_t layerBytes = bc7MipBytes(width, height, task.mip, 1);
      uint8_t* out = data.data() + mipOffset[task.mip] + task.frame * layerBytes + uint64_t{task.first} * kBc7BlockBytes;
      encodeBlocks(mips[task.frame][task.mip], task.first, task.count, params, out, nullptr);
    }
  });

  const std::filesystem::path base = options.cacheDir / assetBase(setName, clip.name);
  const uint64_t tailOffset = mipOffset[layout.tailFirstMip];
  return writeFile(std::filesystem::path(base).concat(".rfull"), data.data(), data.size()) &&
         writeFile(std::filesystem::path(base).concat(".rtail"), data.data() + tailOffset, data.size() - tailOffset) &&
         writeFile(std::filesystem::path(base).concat(".rkey"), &key, sizeof(key));
}

bool readReliefKey(const std::filesystem::path& path, uint64_t& out) {
  std::ifstream file(path, std::ios::binary);
  return file && file.read(reinterpret_cast<char*>(&out), sizeof(out)) && file.gcount() == sizeof(out);
}

void copyName(char (&out)[48], const std::string& text) {
  std::memset(out, 0, sizeof(out));
  std::memcpy(out, text.data(), std::min(text.size(), sizeof(out) - 1));
}

}  // namespace

std::string spriteReliefPath(const std::string& frameSpec) {
  std::string path = frameSpec;
  for (char& c : path)
    if (c == '#' || c == ':' || c == '!') c = '_';
  return path + ".relief.png";
}

bool writeSpriteFrames(const SpriteManifest& manifest, const std::filesystem::path& out) {
  nlohmann::json clips = nlohmann::json::array();
  for (const SpriteSetSource& set : manifest.sets)
    for (const SpriteClipSource& clip : set.clips) {
      if (clip.frames.empty()) continue;
      nlohmann::json frames = nlohmann::json::array();
      for (const std::string& frame : clip.frames) {
        const FrameSpec spec = parseFrame(frame);
        frames.push_back({{"spec", frame}, {"file", spec.file}, {"cols", spec.cols}, {"rows", spec.rows}, {"cell", spec.cell},
                          {"black", spec.alphaFromBlack}, {"relief", spriteReliefPath(frame)}});
      }
      clips.push_back({{"set", set.name}, {"clip", clip.name}, {"frames", std::move(frames)}});
    }
  const nlohmann::json doc = {{"version", 1}, {"clips", std::move(clips)}};
  const std::string text = doc.dump(1);
  return writeFile(out, text.data(), text.size());
}

bool writeSpritePreview(const SpriteManifest& manifest, const SpriteCookOptions& options, const std::string& setName,
                        const std::string& clipName, uint32_t frame, const std::filesystem::path& out) {
  const SpriteClipSource* clip = nullptr;
  for (const SpriteSetSource& set : manifest.sets)
    if (set.name == setName)
      for (const SpriteClipSource& c : set.clips)
        if (c.name == clipName) clip = &c;
  if (!clip || frame >= clip->frames.size()) {
    logError("preview: no frame {} in {}/{}", frame, setName, clipName);
    return false;
  }
  const std::filesystem::path cached = options.cacheDir / assetBase(setName, clipName);
  CookedLayout layout{};
  std::ifstream fullFile(std::filesystem::path(cached).concat(".full"), std::ios::binary);
  if (!readLayout(std::filesystem::path(cached).concat(".layout"), layout) || !fullFile) {
    logError("preview: {}/{} is not cooked in {}", setName, clipName, options.cacheDir.string());
    return false;
  }
  std::vector<uint8_t> full(layout.fullBytes);
  fullFile.read(reinterpret_cast<char*>(full.data()), static_cast<std::streamsize>(full.size()));

  Rgba8Image sourceImage;
  if (!loadFrame(options.sourceRoot, clip->frames[frame], sourceImage)) return false;
  const uint8_t* source = sourceImage.pixels.data();
  const uint32_t sw = sourceImage.width;

  // Decodes one mip of the frame back to pixels, as the GPU will.
  const auto decode = [&](uint32_t mip, Rgba8Image& image) {
    image.width = std::max(1u, layout.width >> mip);
    image.height = std::max(1u, layout.height >> mip);
    image.pixels.assign(size_t{image.width} * image.height * 4, 0);
    uint64_t offset = 0;
    for (uint32_t m = 0; m < mip; ++m) offset += bc7MipBytes(layout.width, layout.height, m, layout.frames);
    offset += frame * bc7MipBytes(layout.width, layout.height, mip, 1);
    const uint32_t blocksX = (image.width + 3) / 4, blocksY = (image.height + 3) / 4;
    bc7decomp::color_rgba texels[16];
    for (uint32_t by = 0; by < blocksY; ++by)
      for (uint32_t bx = 0; bx < blocksX; ++bx) {
        bc7decomp::unpack_bc7(full.data() + offset + (size_t{by} * blocksX + bx) * kBc7BlockBytes, texels);
        for (uint32_t i = 0; i < 16; ++i) {
          const uint32_t x = bx * 4 + i % 4, y = by * 4 + i / 4;
          if (x < image.width && y < image.height) std::memcpy(&image.pixels[(size_t{y} * image.width + x) * 4], &texels[i], 4);
        }
      }
  };

  // Panels: source (trimmed area) | decoded mip 0 | decoded mip 1 | decoded mip 2; light row, dark row.
  Rgba8Image panels[4];
  panels[0].width = layout.width;
  panels[0].height = layout.height;
  panels[0].pixels.assign(size_t{layout.width} * layout.height * 4, 0);
  for (uint32_t y = 0; y < layout.contentHeight; ++y)
    std::memcpy(&panels[0].pixels[size_t{y} * layout.width * 4], &source[(size_t{y + uint32_t(layout.cropY)} * sw + uint32_t(layout.cropX)) * 4],
                size_t{layout.contentWidth} * 4);
  for (uint32_t i = 1; i < 4; ++i) decode(std::min(i - 1, layout.mipCount - 1), panels[i]);

  const uint32_t gap = 16, cellW = layout.width + gap, cellH = layout.height + gap;
  const uint32_t outW = cellW * 4 + gap, outH = cellH * 2 + gap;
  std::vector<uint8_t> image(size_t{outW} * outH * 3, 128);
  const uint8_t backgrounds[2][3] = {{214, 214, 210}, {24, 26, 32}};
  for (uint32_t row = 0; row < 2; ++row)
    for (uint32_t p = 0; p < 4; ++p) {
      const Rgba8Image& panel = panels[p];
      const uint32_t ox = gap + p * cellW, oy = gap + row * cellH;
      for (uint32_t y = 0; y < layout.height; ++y)
        for (uint32_t x = 0; x < layout.width; ++x) {
          uint8_t* d = &image[(size_t{oy + y} * outW + ox + x) * 3];
          // Smaller mips are shown at their own size (top-left), like the art seen further away.
          const bool inside = x < panel.width && y < panel.height;
          const uint8_t* s = inside ? &panel.pixels[(size_t{y} * panel.width + x) * 4] : nullptr;
          const float a = s ? s[3] / 255.0f : 0.0f;
          for (int c = 0; c < 3; ++c)
            d[c] = static_cast<uint8_t>(std::lround((s ? s[c] : 0) * a + backgrounds[row][c] * (1.0f - a)));
        }
    }
  std::filesystem::create_directories(out.parent_path().empty() ? "." : out.parent_path());
  return stbi_write_png(out.string().c_str(), static_cast<int>(outW), static_cast<int>(outH), 3, image.data(),
                        static_cast<int>(outW * 3)) != 0;
}

bool cookSprites(JobSystem& jobs, const SpriteManifest& manifest, const SpriteCookOptions& options,
                 std::vector<PakSource>& out, SpriteCookReport& report) {
  const auto start = std::chrono::steady_clock::now();
  report = {};
  std::vector<AssetId> roster;

  for (const SpriteSetSource& set : manifest.sets) {
    if (!options.only.empty() &&
        std::none_of(options.only.begin(), options.only.end(), [&](const std::string& p) { return set.name.starts_with(p); }))
      continue;
    for (const SpriteClipSource& clip : set.clips) {
      if (clip.frames.empty()) continue;
      const std::string base = assetBase(set.name, clip.name);
      const std::filesystem::path cached = options.cacheDir / base;
      const uint64_t key = textureKey(clip, options);

      CookedLayout layout{};
      const bool hit = readLayout(std::filesystem::path(cached).concat(".layout"), layout) && layout.key == key &&
                       std::filesystem::exists(std::filesystem::path(cached).concat(".full")) &&
                       std::filesystem::exists(std::filesystem::path(cached).concat(".tail"));
      if (hit) {
        ++report.fromCache;
      } else {
        const auto clipStart = std::chrono::steady_clock::now();
        if (!cookClipTextures(jobs, set.name, clip, options, key, layout)) {
          ++report.failed;
          continue;
        }
        ++report.cooked;
        report.quality.push_back({set.name + "/" + clip.name, layout.colorPsnr, layout.alphaPsnr});
        logInfo("cooked {}/{}: {} x {}x{} -> {:.1f} MB, colour {:.1f} dB, alpha {:.1f} dB, {:.1f}s", set.name, clip.name,
                layout.frames, layout.width, layout.height, static_cast<double>(layout.fullBytes) / (1 << 20), layout.colorPsnr,
                layout.alphaPsnr, std::chrono::duration<double>(std::chrono::steady_clock::now() - clipStart).count());
      }

      ++report.clips;
      report.frames += layout.frames;
      report.sourcePixels += uint64_t{layout.sourceWidth} * layout.sourceHeight * layout.frames;
      report.texturePixels += uint64_t{layout.width} * layout.height * layout.frames;

      // The relief chain, when every frame has its map (relief.py): its own cache next to the colour's.
      bool relief = false;
      if (!options.reliefDir.empty()) {
        const std::vector<std::filesystem::path> files = reliefFiles(clip, options);
        const bool complete = std::all_of(files.begin(), files.end(), [](const std::filesystem::path& f) { return std::filesystem::exists(f); });
        if (complete) {
          const uint64_t rkey = reliefKey(key, files);
          uint64_t cachedKey = 0;
          const bool rhit = readReliefKey(std::filesystem::path(cached).concat(".rkey"), cachedKey) && cachedKey == rkey &&
                            std::filesystem::exists(std::filesystem::path(cached).concat(".rfull")) &&
                            std::filesystem::exists(std::filesystem::path(cached).concat(".rtail"));
          if (rhit) {
            relief = true;
          } else {
            const auto reliefStart = std::chrono::steady_clock::now();
            relief = cookClipRelief(jobs, set.name, clip, options, layout, files, rkey);
            if (relief) {
              ++report.reliefCooked;
              logInfo("cooked relief {}/{}: {} frames, {:.1f}s", set.name, clip.name, layout.frames,
                      std::chrono::duration<double>(std::chrono::steady_clock::now() - reliefStart).count());
            } else {
              logWarn("{}/{}: relief not cooked; the clip stays flat", set.name, clip.name);
            }
          }
        }
        if (relief) ++report.reliefClips;
      }

      // The header carries timing and pack: rewritten every run, never part of the cache key.
      for (uint32_t copy = 1; copy <= std::max(1u, options.copies); ++copy) {
        const std::string setName = copy == 1 ? set.name : set.name + "~" + std::to_string(copy);
        const std::string asset = assetBase(setName, clip.name);
        SpriteClipHeader header{};
        header.magic = kSpriteClipMagic;
        header.version = kSpriteFormatVersion;
        header.frames = layout.frames;
        header.width = layout.width;
        header.height = layout.height;
        header.mipCount = layout.mipCount;
        header.tailFirstMip = layout.tailFirstMip;
        header.contentWidth = layout.contentWidth;
        header.contentHeight = layout.contentHeight;
        header.sourceWidth = layout.sourceWidth;
        header.sourceHeight = layout.sourceHeight;
        header.cropX = layout.cropX;
        header.cropY = layout.cropY;
        header.mode = clip.mode;
        header.fps = clip.fps;
        header.fullAsset = assetId(asset + ".full");
        header.tailAsset = assetId(asset + ".tail");
        header.fullBytes = layout.fullBytes;
        header.tailBytes = layout.tailBytes;
        copyName(header.pack, clip.pack);
        copyName(header.name, clip.name);
        header.reliefFullAsset = relief ? assetId(asset + ".rfull") : 0;
        header.reliefTailAsset = relief ? assetId(asset + ".rtail") : 0;
        const std::filesystem::path headerFile = std::filesystem::path(options.cacheDir / asset).concat(".clip");
        if (!writeFile(headerFile, &header, sizeof(header))) {
          logError("cannot write {}", headerFile.string());
          return false;
        }
        out.push_back({asset + ".clip", headerFile});
        out.push_back({asset + ".full", std::filesystem::path(cached).concat(".full")});
        out.push_back({asset + ".tail", std::filesystem::path(cached).concat(".tail")});
        if (relief) {
          out.push_back({asset + ".rfull", std::filesystem::path(cached).concat(".rfull")});
          out.push_back({asset + ".rtail", std::filesystem::path(cached).concat(".rtail")});
          report.fullBytes += layout.fullBytes;
          report.tailBytes += layout.tailBytes;
        }
        roster.push_back(assetId(asset + ".clip"));
        report.fullBytes += layout.fullBytes;
        report.tailBytes += layout.tailBytes;
      }
    }
  }

  std::vector<uint8_t> rosterBytes(sizeof(SpriteRosterHeader) + roster.size() * sizeof(AssetId));
  const SpriteRosterHeader rosterHeader{kSpriteRosterMagic, kSpriteFormatVersion, static_cast<uint32_t>(roster.size()), 0};
  std::memcpy(rosterBytes.data(), &rosterHeader, sizeof(rosterHeader));
  std::memcpy(rosterBytes.data() + sizeof(rosterHeader), roster.data(), roster.size() * sizeof(AssetId));
  const std::filesystem::path rosterFile = options.cacheDir / "sprites/roster";
  if (!writeFile(rosterFile, rosterBytes.data(), rosterBytes.size())) return false;
  out.push_back({"sprites/roster", rosterFile});

  std::sort(report.quality.begin(), report.quality.end(),
            [](const SpriteClipQuality& a, const SpriteClipQuality& b) { return a.colorPsnr < b.colorPsnr; });
  report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return report.failed == 0;
}

}  // namespace ember
