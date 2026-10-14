#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ember {

class JobSystem;

struct PakSource {
  std::string assetPath;           // relative to the content root; hashed into the asset id
  std::filesystem::path filePath;  // where to read it
};

struct PakWriteOptions {
  bool compress = true;
  int zstdLevel = 9;
  double minSaving = 0.03;  // store a block uncompressed unless zstd saves at least this fraction
};

struct PakWriteReport {
  size_t entries = 0;
  size_t compressedEntries = 0;
  uint64_t sourceBytes = 0;
  uint64_t storedBytes = 0;
  double seconds = 0.0;
};

// Every file under `root`, with asset paths relative to it.
std::vector<PakSource> collectSources(const std::filesystem::path& root);

// Compresses on every core and writes the archive. Sources are processed in bounded batches, so
// packing many gigabytes never holds more than one batch in memory. Fails (logs why) on an id
// collision or an unreadable source.
bool writePak(JobSystem& jobs, std::vector<PakSource> sources, const std::filesystem::path& out,
              const PakWriteOptions& options, PakWriteReport& report);

}  // namespace ember
