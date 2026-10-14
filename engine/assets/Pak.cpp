#include "assets/Pak.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "core/Log.h"

namespace ember {

const PakEntry* PakToc::find(AssetId id) const {
  const auto it = std::lower_bound(entries.begin(), entries.end(), id,
                                   [](const PakEntry& entry, AssetId key) { return entry.id < key; });
  return it != entries.end() && it->id == id ? &*it : nullptr;
}

bool readPakToc(const std::filesystem::path& path, PakToc& out) {
  const auto fail = [&](std::string_view why) {
    logError("pak {}: {}", path.string(), why);
    return false;
  };

  std::ifstream file(path, std::ios::binary);
  if (!file) return fail("cannot open");
  std::error_code error;
  const uint64_t fileSize = std::filesystem::file_size(path, error);
  if (error) return fail("cannot read size");

  PakHeader header{};
  if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) return fail("truncated header");
  if (std::memcmp(header.magic, kPakMagic, sizeof(kPakMagic)) != 0) return fail("not a pak file");
  if (header.version != kPakVersion) return fail(std::format("version {} (engine reads {})", header.version, kPakVersion));

  const uint64_t entriesBytes = uint64_t{header.entryCount} * sizeof(PakEntry);
  if (header.entriesOffset + entriesBytes > fileSize || header.namesOffset + header.namesSize > fileSize)
    return fail("table of contents past end of file");

  out.path = path;
  out.entries.resize(header.entryCount);
  out.names.resize(header.namesSize);
  file.seekg(static_cast<std::streamoff>(header.entriesOffset));
  if (!file.read(reinterpret_cast<char*>(out.entries.data()), static_cast<std::streamsize>(entriesBytes)))
    return fail("truncated entry table");
  file.seekg(static_cast<std::streamoff>(header.namesOffset));
  if (!file.read(out.names.data(), static_cast<std::streamsize>(header.namesSize))) return fail("truncated names");

  for (size_t i = 0; i < out.entries.size(); ++i) {
    const PakEntry& entry = out.entries[i];
    if (i > 0 && out.entries[i - 1].id >= entry.id) return fail("entry table not sorted");
    if (entry.offset + entry.storedSize > fileSize) return fail("asset block past end of file");
    if (uint64_t{entry.nameOffset} + entry.nameSize > header.namesSize) return fail("bad name offset");
    if (entry.compression == PakCompression::None && entry.storedSize != entry.size)
      return fail("uncompressed block with mismatched size");
    if (entry.compression != PakCompression::None && entry.compression != PakCompression::Zstd)
      return fail("unknown compression");
  }
  return true;
}

}  // namespace ember
