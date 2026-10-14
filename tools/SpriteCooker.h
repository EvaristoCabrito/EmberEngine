#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "PakWriter.h"
#include "SpriteSource.h"

namespace ember {

class JobSystem;

enum class Bc7Quality { Fast, Basic, Slow };

struct SpriteCookOptions {
  std::filesystem::path sourceRoot;  // frame paths in the manifest are relative to this
  std::filesystem::path cacheDir;    // cooked clips are kept here; unchanged clips are never re-cooked
  Bc7Quality quality = Bc7Quality::Slow;
  std::vector<std::string> only;     // cook only sets whose name starts with one of these (empty = all)
  // Stand-in for a bigger game: every clip is added `copies` times under distinct names
  // ("<set>~2", "<set>~3", ...). Cooked once, stored N times. For streaming tests only.
  uint32_t copies = 1;
};

struct SpriteClipQuality {
  std::string name;
  double colorPsnr = 0.0;  // premultiplied colour over visible pixels, top mip, dB
  double alphaPsnr = 0.0;
};

struct SpriteCookReport {
  size_t clips = 0, frames = 0, cooked = 0, fromCache = 0, failed = 0;
  uint64_t sourcePixels = 0;   // frames at their original size
  uint64_t texturePixels = 0;  // trimmed, top mip
  uint64_t fullBytes = 0, tailBytes = 0;
  double seconds = 0.0;
  std::vector<SpriteClipQuality> quality;  // clips cooked in this run, worst first
};

// Writes a side-by-side check image for one cooked frame: source vs. what the GPU decodes, at the top
// mip and two smaller mips, over a light and a dark background. Reads the clip from the cook cache.
bool writeSpritePreview(const SpriteManifest& manifest, const SpriteCookOptions& options, const std::string& set,
                        const std::string& clip, uint32_t frame, const std::filesystem::path& out);

// Cooks every clip and appends the cooked files (clip headers, tails, full chains, the roster) to
// `out`, ready for writePak(). Clips are cooked one after another, each spread over every core.
bool cookSprites(JobSystem& jobs, const SpriteManifest& manifest, const SpriteCookOptions& options,
                 std::vector<PakSource>& out, SpriteCookReport& report);

}  // namespace ember
