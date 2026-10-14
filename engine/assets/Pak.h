#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "assets/AssetId.h"

namespace ember {

// .pak archive: the cooked game data on disk.
//
//   [PakHeader][asset blocks, packed back to back][PakEntry table, sorted by id][path strings]
//
// Blocks are packed without padding; a streaming read covers the disk sectors around a block, so
// small assets waste no space. The entry table sits at the end so the cooker can write blocks as they
// finish compressing. All integers are little-endian.

inline constexpr char kPakMagic[8] = {'E', 'M', 'B', 'E', 'R', 'P', 'A', 'K'};
inline constexpr uint32_t kPakVersion = 1;

enum class PakCompression : uint32_t {
  None = 0,
  Zstd = 1,
};

struct PakHeader {
  char magic[8];
  uint32_t version;
  uint32_t entryCount;
  uint64_t entriesOffset;
  uint64_t namesOffset;
  uint64_t namesSize;
  uint64_t dataSize;  // total bytes of asset blocks
  uint64_t reserved[3];
};
static_assert(sizeof(PakHeader) == 72);

struct PakEntry {
  AssetId id;
  uint64_t offset;      // of the block, from the start of the file
  uint64_t storedSize;  // bytes on disk
  uint64_t size;        // bytes once decompressed
  uint32_t nameOffset;  // into the path strings
  uint32_t nameSize;
  PakCompression compression;
  uint32_t flags;  // reserved
};
static_assert(sizeof(PakEntry) == 48);

// An archive's table of contents, read once when it is mounted.
struct PakToc {
  std::filesystem::path path;
  std::vector<PakEntry> entries;  // sorted by id
  std::string names;

  const PakEntry* find(AssetId id) const;
  std::string_view name(const PakEntry& entry) const { return {names.data() + entry.nameOffset, entry.nameSize}; }
};

// Reads the header and table of contents. Fails (returns false, logs why) on a damaged archive.
bool readPakToc(const std::filesystem::path& path, PakToc& out);

}  // namespace ember
