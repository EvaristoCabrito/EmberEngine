#include "PakWriter.h"

#include <zstd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>

#include "assets/Pak.h"
#include "core/Jobs.h"
#include "core/Log.h"

namespace ember {

namespace {

constexpr uint64_t kBatchBytes = 512ull << 20;  // source bytes compressed at once
constexpr size_t kBatchFiles = 4096;

struct Block {
  std::vector<char> bytes;
  uint64_t sourceSize = 0;
  PakCompression compression = PakCompression::None;
  bool ok = false;
};

bool readWhole(const std::filesystem::path& path, std::vector<char>& out) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return false;
  out.resize(static_cast<size_t>(file.tellg()));
  file.seekg(0);
  return static_cast<bool>(file.read(out.data(), static_cast<std::streamsize>(out.size())));
}

void cookBlock(const PakSource& source, const PakWriteOptions& options, Block& block) {
  std::vector<char> raw;
  if (!readWhole(source.filePath, raw)) return;
  block.sourceSize = raw.size();
  block.ok = true;
  if (options.compress && !raw.empty()) {
    struct Context {
      ZSTD_CCtx* handle = ZSTD_createCCtx();
      ~Context() { ZSTD_freeCCtx(handle); }
    };
    thread_local Context context;
    std::vector<char> packed(ZSTD_compressBound(raw.size()));
    const size_t size = ZSTD_compressCCtx(context.handle,packed.data(), packed.size(), raw.data(), raw.size(), options.zstdLevel);
    if (!ZSTD_isError(size) && static_cast<double>(size) <= static_cast<double>(raw.size()) * (1.0 - options.minSaving)) {
      packed.resize(size);
      block.bytes = std::move(packed);
      block.compression = PakCompression::Zstd;
      return;
    }
  }
  block.bytes = std::move(raw);
}

}  // namespace

std::vector<PakSource> collectSources(const std::filesystem::path& root) {
  std::vector<PakSource> sources;
  for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
    if (!item.is_regular_file()) continue;
    sources.push_back({normalizeAssetPath(std::filesystem::relative(item.path(), root).generic_string()), item.path()});
  }
  // Deterministic archives: same input, same bytes.
  std::sort(sources.begin(), sources.end(), [](const PakSource& a, const PakSource& b) { return a.assetPath < b.assetPath; });
  return sources;
}

bool writePak(JobSystem& jobs, std::vector<PakSource> sources, const std::filesystem::path& out,
              const PakWriteOptions& options, PakWriteReport& report) {
  const auto start = std::chrono::steady_clock::now();
  report = {};

  // Ids must be unique: two paths hashing alike would make one asset unreachable.
  {
    std::vector<std::pair<AssetId, size_t>> ids(sources.size());
    for (size_t i = 0; i < sources.size(); ++i) ids[i] = {assetId(sources[i].assetPath), i};
    std::sort(ids.begin(), ids.end());
    for (size_t i = 1; i < ids.size(); ++i) {
      if (ids[i].first == ids[i - 1].first) {
        logError("asset id collision: '{}' and '{}' (rename one)", sources[ids[i - 1].second].assetPath,
                 sources[ids[i].second].assetPath);
        return false;
      }
    }
  }

  std::filesystem::create_directories(out.parent_path().empty() ? "." : out.parent_path());
  const std::filesystem::path temp = std::filesystem::path(out).concat(".tmp");
  std::ofstream file(temp, std::ios::binary | std::ios::trunc);
  if (!file) {
    logError("cannot write {}", temp.string());
    return false;
  }

  PakHeader header{};
  std::memcpy(header.magic, kPakMagic, sizeof(kPakMagic));
  header.version = kPakVersion;
  file.write(reinterpret_cast<const char*>(&header), sizeof(header));
  uint64_t offset = sizeof(header);

  std::vector<PakEntry> entries;
  entries.reserve(sources.size());
  std::string names;

  for (size_t first = 0; first < sources.size();) {
    // Batch by count and by source size so memory stays bounded.
    size_t last = first;
    uint64_t batchBytes = 0;
    while (last < sources.size() && last - first < kBatchFiles && batchBytes < kBatchBytes) {
      std::error_code error;
      batchBytes += std::filesystem::file_size(sources[last].filePath, error);
      ++last;
    }

    std::vector<Block> blocks(last - first);
    jobs.parallelFor(static_cast<uint32_t>(blocks.size()), 1, [&](uint32_t begin, uint32_t end) {
      for (uint32_t i = begin; i < end; ++i) cookBlock(sources[first + i], options, blocks[i]);
    });

    for (size_t i = 0; i < blocks.size(); ++i) {
      const PakSource& source = sources[first + i];
      Block& block = blocks[i];
      if (!block.ok) {
        logError("cannot read {}", source.filePath.string());
        return false;
      }
      if (names.size() + source.assetPath.size() > UINT32_MAX) {
        logError("too many asset paths for one archive");
        return false;
      }
      PakEntry entry{};
      entry.id = assetId(source.assetPath);
      entry.offset = offset;
      entry.storedSize = block.bytes.size();
      entry.size = block.sourceSize;
      entry.nameOffset = static_cast<uint32_t>(names.size());
      entry.nameSize = static_cast<uint32_t>(source.assetPath.size());
      entry.compression = block.compression;
      entries.push_back(entry);
      names += source.assetPath;

      file.write(block.bytes.data(), static_cast<std::streamsize>(block.bytes.size()));
      offset += block.bytes.size();
      report.sourceBytes += block.sourceSize;
      report.storedBytes += block.bytes.size();
      if (block.compression != PakCompression::None) ++report.compressedEntries;
    }
    first = last;
  }

  std::sort(entries.begin(), entries.end(), [](const PakEntry& a, const PakEntry& b) { return a.id < b.id; });
  header.entryCount = static_cast<uint32_t>(entries.size());
  header.dataSize = offset - sizeof(header);
  header.entriesOffset = offset;
  file.write(reinterpret_cast<const char*>(entries.data()), static_cast<std::streamsize>(entries.size() * sizeof(PakEntry)));
  header.namesOffset = offset + entries.size() * sizeof(PakEntry);
  header.namesSize = names.size();
  file.write(names.data(), static_cast<std::streamsize>(names.size()));
  file.seekp(0);
  file.write(reinterpret_cast<const char*>(&header), sizeof(header));
  file.close();
  if (!file) {
    logError("write failed: {}", temp.string());
    return false;
  }

  std::error_code error;
  std::filesystem::rename(temp, out, error);
  if (error) {
    logError("cannot replace {}: {}", out.string(), error.message());
    return false;
  }

  report.entries = entries.size();
  report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return true;
}

}  // namespace ember
