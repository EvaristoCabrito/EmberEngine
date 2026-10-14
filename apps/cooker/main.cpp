// Cooker: builds the game's cooked data.
//   Cooker pack <content dir> <out.pak> [--level 1..22] [--store]   pack every file under a folder
//   Cooker list <file.pak>                                           print an archive's contents
//   Cooker synth <out dir> [--count N] [--min BYTES] [--max BYTES] [--seed S]
//                                                                    write synthetic test files
//   Cooker import-engine2 <engine2 root> <out.sprites.json>         sprite manifest from Engine2's tables
//   Cooker sprites <manifest.sprites.json> <source root> <out.pak>  cook sprite clips (BC7) into an archive
//          [--only <set prefix>]... [--quality fast|basic|slow] [--copies N] [--cache <dir>]
//   Cooker sprite-preview <manifest> <source root> <cache dir> <set> <clip> <frame> <out.png>
//                                                                    source vs. GPU-decoded frame, side by side

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "PakWriter.h"
#include "SpriteCooker.h"
#include "SpriteSource.h"
#include "assets/Pak.h"
#include "core/Jobs.h"
#include "core/Log.h"

namespace {

using namespace ember;

uint64_t parseNumber(std::string_view text) {
  uint64_t value = 0;
  if (std::from_chars(text.data(), text.data() + text.size(), value).ec != std::errc{})
    fatal(std::format("Not a number: {}", text));
  return value;
}

double megabytes(uint64_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }

int pack(const std::vector<std::string_view>& args) {
  if (args.size() < 2) fatal("usage: Cooker pack <content dir> <out.pak> [--level N] [--store]");
  PakWriteOptions options;
  for (size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--level" && i + 1 < args.size()) options.zstdLevel = static_cast<int>(parseNumber(args[++i]));
    else if (args[i] == "--store") options.compress = false;
    else fatal(std::format("Unknown argument {}", args[i]));
  }
  const std::filesystem::path root(args[0]);
  if (!std::filesystem::is_directory(root)) fatal(std::format("Not a folder: {}", root.string()));

  JobSystem jobs;
  PakWriteReport report;
  if (!writePak(jobs, collectSources(root), args[1], options, report)) return 1;
  logInfo("packed {} assets ({} compressed): {:.1f} MB -> {:.1f} MB in {:.2f}s on {} threads", report.entries,
          report.compressedEntries, megabytes(report.sourceBytes), megabytes(report.storedBytes), report.seconds,
          jobs.workerCount() + 1);
  return 0;
}

int list(const std::vector<std::string_view>& args) {
  if (args.size() != 1) fatal("usage: Cooker list <file.pak>");
  PakToc toc;
  if (!readPakToc(args[0], toc)) return 1;
  uint64_t stored = 0, size = 0;
  for (const PakEntry& entry : toc.entries) {
    logInfo("{:016x} {:>12} {:>12} {} {}", entry.id, entry.size, entry.storedSize,
            entry.compression == PakCompression::Zstd ? "zstd" : "    ", toc.name(entry));
    stored += entry.storedSize;
    size += entry.size;
  }
  logInfo("{} assets, {:.1f} MB ({:.1f} MB on disk)", toc.entries.size(), megabytes(size), megabytes(stored));
  return 0;
}

// Deterministic, roughly 2:1 compressible content: runs of random bytes mixed with repeated text,
// like real cooked data. Not game content; used to test streaming at scale.
int synth(const std::vector<std::string_view>& args) {
  if (args.empty()) fatal("usage: Cooker synth <out dir> [--count N] [--min BYTES] [--max BYTES] [--seed S]");
  uint64_t count = 10000, minBytes = 4096, maxBytes = 65536, seed = 1;
  for (size_t i = 1; i < args.size(); ++i) {
    if (i + 1 >= args.size()) fatal(std::format("Missing value for {}", args[i]));
    if (args[i] == "--count") count = parseNumber(args[++i]);
    else if (args[i] == "--min") minBytes = parseNumber(args[++i]);
    else if (args[i] == "--max") maxBytes = parseNumber(args[++i]);
    else if (args[i] == "--seed") seed = parseNumber(args[++i]);
    else fatal(std::format("Unknown argument {}", args[i]));
  }
  if (minBytes > maxBytes) fatal("--min is larger than --max");

  const std::filesystem::path root(args[0]);
  JobSystem jobs;
  std::atomic<uint64_t> total{0};
  jobs.parallelFor(static_cast<uint32_t>(count), 64, [&](uint32_t begin, uint32_t end) {
    static constexpr std::string_view kText = "ember engine streaming test block. ";
    for (uint32_t index = begin; index < end; ++index) {
      uint64_t state = seed * 0x9e3779b97f4a7c15ull + index;
      const auto next = [&state] {  // splitmix64
        uint64_t z = (state += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
      };
      const uint64_t size = minBytes + next() % (maxBytes - minBytes + 1);
      std::string data;
      data.reserve(size);
      while (data.size() < size) {
        const size_t run = std::min<size_t>(64, size - data.size());
        if (next() & 1) {
          for (size_t i = 0; i < run; ++i) data.push_back(static_cast<char>(next()));
        } else {
          for (size_t i = 0; i < run; ++i) data.push_back(kText[i % kText.size()]);
        }
      }
      const std::filesystem::path folder = root / std::format("{:04}", index / 100);
      std::filesystem::create_directories(folder);
      std::ofstream(folder / std::format("{:06}.bin", index), std::ios::binary).write(data.data(), static_cast<std::streamsize>(data.size()));
      total.fetch_add(size, std::memory_order_relaxed);
    }
  });
  logInfo("wrote {} files, {:.1f} MB, under {}", count, megabytes(total.load()), root.string());
  return 0;
}

int importEngine2(const std::vector<std::string_view>& args) {
  if (args.size() != 2) fatal("usage: Cooker import-engine2 <engine2 root> <out.sprites.json>");
  SpriteManifest manifest;
  if (!importEngine2Sprites(std::filesystem::path(args[0]), manifest)) return 1;
  if (!saveSpriteManifest(std::filesystem::path(args[1]), manifest)) return 1;
  logInfo("wrote {}", args[1]);
  return 0;
}

int sprites(const std::vector<std::string_view>& args) {
  if (args.size() < 3) fatal("usage: Cooker sprites <manifest.sprites.json> <source root> <out.pak> [options]");
  SpriteCookOptions options;
  options.sourceRoot = std::filesystem::path(args[1]);
  const std::filesystem::path out(args[2]);
  options.cacheDir = (out.parent_path().empty() ? std::filesystem::path(".") : out.parent_path()) / "cook-cache";
  for (size_t i = 3; i < args.size(); ++i) {
    if (i + 1 >= args.size()) fatal(std::format("Missing value for {}", args[i]));
    const std::string_view value = args[++i];
    if (args[i - 1] == "--only") options.only.emplace_back(value);
    else if (args[i - 1] == "--copies") options.copies = static_cast<uint32_t>(parseNumber(value));
    else if (args[i - 1] == "--cache") options.cacheDir = std::filesystem::path(value);
    else if (args[i - 1] == "--quality") {
      if (value == "fast") options.quality = Bc7Quality::Fast;
      else if (value == "basic") options.quality = Bc7Quality::Basic;
      else if (value == "slow") options.quality = Bc7Quality::Slow;
      else fatal(std::format("Unknown quality {}", value));
    } else {
      fatal(std::format("Unknown argument {}", args[i - 1]));
    }
  }

  SpriteManifest manifest;
  if (!loadSpriteManifest(std::filesystem::path(args[0]), manifest)) return 1;
  JobSystem jobs;
  std::vector<PakSource> sources;
  SpriteCookReport report;
  const bool cooked = cookSprites(jobs, manifest, options, sources, report);
  logInfo("sprites: {} clips, {} frames ({} cooked, {} from cache, {} failed) in {:.1f}s", report.clips, report.frames,
          report.cooked, report.fromCache, report.failed, report.seconds);
  logInfo("  pixels: {:.2f} G at source size, {:.2f} G trimmed; GPU: {:.1f} MB full chains, {:.1f} MB tails",
          static_cast<double>(report.sourcePixels) / 1e9, static_cast<double>(report.texturePixels) / 1e9,
          megabytes(report.fullBytes), megabytes(report.tailBytes));
  for (size_t i = 0; i < std::min<size_t>(report.quality.size(), 10); ++i)
    logInfo("  lowest quality #{}: {} colour {:.1f} dB, alpha {:.1f} dB", i + 1, report.quality[i].name,
            report.quality[i].colorPsnr, report.quality[i].alphaPsnr);
  if (!cooked) return 1;

  PakWriteReport pak;
  if (!writePak(jobs, std::move(sources), out, {}, pak)) return 1;
  logInfo("packed {} assets: {:.1f} MB -> {:.1f} MB on disk in {:.1f}s", pak.entries, megabytes(pak.sourceBytes),
          megabytes(pak.storedBytes), pak.seconds);
  return 0;
}

int spritePreview(const std::vector<std::string_view>& args) {
  if (args.size() != 7) fatal("usage: Cooker sprite-preview <manifest> <source root> <cache dir> <set> <clip> <frame> <out.png>");
  SpriteManifest manifest;
  if (!loadSpriteManifest(std::filesystem::path(args[0]), manifest)) return 1;
  SpriteCookOptions options;
  options.sourceRoot = std::filesystem::path(args[1]);
  options.cacheDir = std::filesystem::path(args[2]);
  if (!writeSpritePreview(manifest, options, std::string(args[3]), std::string(args[4]),
                          static_cast<uint32_t>(parseNumber(args[5])), std::filesystem::path(args[6])))
    return 1;
  logInfo("wrote {}", args[6]);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    ember::logError("usage: Cooker <pack|list|synth> ...");
    return 1;
  }
  const std::string_view command = argv[1];
  const std::vector<std::string_view> args(argv + 2, argv + argc);
  if (command == "pack") return pack(args);
  if (command == "list") return list(args);
  if (command == "synth") return synth(args);
  if (command == "import-engine2") return importEngine2(args);
  if (command == "sprites") return sprites(args);
  if (command == "sprite-preview") return spritePreview(args);
  ember::logError("unknown command {}", command);
  return 1;
}
