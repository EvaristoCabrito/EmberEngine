#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ember {

// Assets are referenced by a 64-bit id: the hash of their source path relative to the content root,
// lower-cased with '/' separators ("characters/neera/idle.png"). The cooker refuses two paths with
// the same id, so an id names exactly one asset.
using AssetId = uint64_t;

constexpr char normalizePathChar(char c) {
  if (c == '\\') return '/';
  if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
  return c;
}

// 64-bit FNV-1a over the normalised path.
constexpr AssetId assetId(std::string_view path) {
  uint64_t hash = 0xcbf29ce484222325ull;
  for (const char c : path) {
    hash ^= static_cast<uint8_t>(normalizePathChar(c));
    hash *= 0x100000001b3ull;
  }
  return hash;
}

inline std::string normalizeAssetPath(std::string_view path) {
  std::string out(path);
  for (char& c : out) c = normalizePathChar(c);
  return out;
}

}  // namespace ember
