#include "ArchitectureCooker.h"

#include <stb_image.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <set>

#include "TextureCooker.h"
#include "core/Jobs.h"
#include "core/Log.h"
#include "render/TextureFormat.h"
#include "world/Architecture.h"

namespace ember {

namespace {

constexpr uint32_t kArchitectureCookerRevision = 1;

uint64_t fnv(uint64_t hash, const void* data, size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ull;
  return hash;
}
bool readFile(const std::filesystem::path& path, std::vector<uint8_t>& out) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return false;
  out.resize(static_cast<size_t>(file.tellg()));
  file.seekg(0);
  return static_cast<bool>(file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size())));
}
bool writeFile(const std::filesystem::path& path, const void* data, size_t size) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  return static_cast<bool>(file);
}

}  // namespace

bool cookArchitecture(JobSystem& jobs, const ArchitectureCookOptions& options, std::vector<PakSource>& out) {
  e2::ArchitectureDefs defs;
  if (!defs.load(options.catalog)) {
    logError("architecture: {} has no architectureDefs (export_editor_catalog.ts)", options.catalog.string());
    return false;
  }
  std::set<std::string> textures;
  for (const auto& [id, d] : defs.defs)
    if (!d.wallTexture.empty()) textures.insert(d.wallTexture);
  bool ok = true;
  for (const std::string& url : textures) {
    const std::string asset = e2::architectureTextureAsset(url);
    // "/ember/..." = EmberEngine's own texture under content/ (world/Architecture.h architectureTextureAsset); else Engine2's public folder.
    const std::filesystem::path file = url.starts_with("/ember/") ? std::filesystem::path("content") / url.substr(7) : options.engine2Public / url.substr(1);
    std::vector<uint8_t> source;
    if (!readFile(file, source)) {
      logError("architecture: cannot read {}", file.string());
      ok = false;
      continue;
    }
    uint64_t key = fnv(0xcbf29ce484222325ull, source.data(), source.size());
    const uint32_t versions[] = {kArchitectureCookerRevision, kTextureFormatVersion, static_cast<uint32_t>(options.quality)};
    key = fnv(key, versions, sizeof(versions));
    const std::filesystem::path cooked = options.cacheDir / asset;
    const std::filesystem::path keyFile = std::filesystem::path(cooked).concat(".key");
    std::vector<uint8_t> stored;
    if (!(readFile(keyFile, stored) && stored.size() == sizeof(key) && std::memcmp(stored.data(), &key, sizeof(key)) == 0 && std::filesystem::exists(cooked))) {
      int w = 0, h = 0, channels = 0;
      stbi_uc* pixels = stbi_load_from_memory(source.data(), static_cast<int>(source.size()), &w, &h, &channels, 4);
      if (!pixels) {
        logError("architecture: cannot decode {}", file.string());
        ok = false;
        continue;
      }
      Rgba8 image;
      image.width = static_cast<uint32_t>(w), image.height = static_cast<uint32_t>(h);
      image.pixels.assign(pixels, pixels + size_t(w) * h * 4);
      stbi_image_free(pixels);
      const auto t0 = std::chrono::steady_clock::now();
      const std::vector<uint8_t> texture = cookTexture(jobs, image, TextureFormat::Bc7Srgb, options.quality);
      if (!writeFile(cooked, texture.data(), texture.size()) || !writeFile(keyFile, &key, sizeof(key))) {
        logError("architecture: cannot write to the cache {}", options.cacheDir.string());
        ok = false;
        continue;
      }
      logInfo("{}: {}x{} -> {:.1f} MB in {:.1f}s", asset, w, h, texture.size() / 1048576.0,
              std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    out.push_back({asset, cooked});
  }
  logInfo("architecture: {} textures", out.size());
  return ok;
}

}  // namespace ember
