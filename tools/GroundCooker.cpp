#include "GroundCooker.h"

#include <stb_image.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <fstream>
#include <optional>
#include <set>

#include <nlohmann/json.hpp>

#include "ProceduralTextures.h"
#include "TextureCooker.h"
#include "core/Log.h"
#include "render/TextureFormat.h"
#include "world/Board.h"
#include "world/GroundAssets.h"
#include "world/TimeOfDay.h"

namespace ember {

namespace {

// Bump when the cooked output changes for the same source.
constexpr uint32_t kGroundCookerRevision = 1;

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

// One texture to cook: from a PNG under Ember's public folder, or procedural.
struct Job {
  std::string asset;
  TextureFormat format;
  std::filesystem::path file;  // empty when procedural
  std::string procedural;
};

std::filesystem::path publicFile(const std::filesystem::path& root, const std::string& url) {
  return root / std::filesystem::path(url.substr(url.starts_with('/') ? 1 : 0));
}

bool decode(const std::vector<uint8_t>& bytes, Rgba8& image) {
  int w = 0, h = 0, channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 4);
  if (!pixels) return false;
  image.width = static_cast<uint32_t>(w);
  image.height = static_cast<uint32_t>(h);
  image.pixels.assign(pixels, pixels + size_t(w) * h * 4);
  stbi_image_free(pixels);
  return true;
}

// Byte-for-byte comparison of the procedural surfaces with Ember's (export_textures.ts).
bool checkProcedural(const std::filesystem::path& dir) {
  std::vector<std::string_view> names(std::begin(kClassicProtoLayers), std::end(kClassicProtoLayers));
  names.push_back("smooth-plains");
  bool ok = true;
  for (const std::string_view name : names) {
    Rgba8 ours;
    std::vector<uint8_t> theirs;
    classicProceduralTexture(name, ours);
    if (!readFile(dir / (std::string(name) + ".rgba"), theirs) || theirs.size() != ours.pixels.size()) {
      logError("  {}: Ember reference missing or a different size", name);
      ok = false;
      continue;
    }
    size_t differ = 0;
    int worst = 0;
    for (size_t i = 0; i < theirs.size(); ++i)
      if (theirs[i] != ours.pixels[i]) {
        ++differ;
        worst = std::max(worst, std::abs(int(theirs[i]) - int(ours.pixels[i])));
      }
    if (differ) logError("  {}: {} of {} bytes differ from Ember (worst by {})", name, differ, theirs.size(), worst);
    else logInfo("  {}: {}x{} identical to Ember", name, ours.width, ours.height);
    ok = ok && differ == 0;
  }
  return ok;
}

}  // namespace

bool cookGround(JobSystem& jobs, const GroundCookOptions& options, std::vector<PakSource>& out, GroundCookReport& report) {
  const auto start = std::chrono::steady_clock::now();
  report = {};
  const std::optional<ember::BoardData> data = ember::BoardData::load(options.boardJson);
  if (!data) return false;
  if (!options.checkDir.empty() && !checkProcedural(options.checkDir)) return false;

  std::vector<Job> work;
  for (const std::string_view surface : kClassicProtoLayers)
    work.push_back({ember::groundProtoAsset(surface), TextureFormat::Bc7Srgb, {}, std::string(surface)});
  const std::filesystem::path plains049 = options.artRoot / "game/ground-claude/049-plains";
  work.push_back({ember::groundPlains049Asset("color"), TextureFormat::Bc7Srgb, plains049 / "color.png", {}});
  work.push_back({ember::groundPlains049Asset("normal"), TextureFormat::Bc5Unorm, plains049 / "normal.png", {}});
  work.push_back({ember::groundPlains049Asset("rough"), TextureFormat::Bc7Unorm, plains049 / "rough.png", {}});
  // The painted map backgrounds and editor-selectable briefing backgrounds (basic BC7 is plenty, and much less work than slow).
  std::vector<std::string> backdropAssets;
  {
    std::set<std::string> editorBackgroundFiles;
    std::ifstream catalogFile("content/data/editor-catalog.json");
    const nlohmann::json catalog = catalogFile ? nlohmann::json::parse(catalogFile, nullptr, false) : nlohmann::json();
    if (catalog.is_object() && catalog.contains("backgrounds") && catalog["backgrounds"].is_object() && catalog["backgrounds"].contains("byId") && catalog["backgrounds"]["byId"].is_object())
      for (const auto& [_, file] : catalog["backgrounds"]["byId"].items())
        if (file.is_string()) editorBackgroundFiles.insert(file.get<std::string>());
    std::set<std::string> cookedBackdropFiles;
    // Keep the project's originals in their existing folders; the Ember source folder is never read.
    for (const std::filesystem::path& folder : {std::filesystem::path("content/backdrops"), std::filesystem::path("content/briefings")}) {
      std::error_code error;
      for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
        const std::string ext = entry.path().extension().string();
        if (ext != ".jpg" && ext != ".jpeg" && ext != ".png") continue;
        const std::u8string name = entry.path().filename().u8string();
        const std::string file(name.begin(), name.end());
        if (folder == "content/briefings" && !editorBackgroundFiles.contains(file)) continue;
        if (folder == "content/backdrops") {  // title, shop and briefing UI art remains in its own UI path
          std::string lower(name.begin(), name.end());
          std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
          if (lower.find("brief") != std::string::npos || lower.rfind("title-bg", 0) == 0 || lower.rfind("backpack-bg", 0) == 0 || lower.rfind("farmlands-", 0) == 0) continue;
        }
        if (!cookedBackdropFiles.insert(file).second) continue;
        work.push_back({ember::backdropAsset(file), TextureFormat::Bc7Srgb, entry.path(), {}});
        backdropAssets.push_back(work.back().asset);
      }
    }
  }
  for (const char* phase : ember::kMoonPhases)
    work.push_back({ember::skyMoonAsset(phase), TextureFormat::Bc7Srgb, publicFile(options.artRoot, ember::moonPhaseIcon(phase)), {}});
  for (const ember::GroundLayerSource& layer : data->groundLayers) {
    if (layer.absent) continue;
    if (!layer.file.empty()) {  // EmberEngine's own tile, a project file (content/ground)
      work.push_back({ember::groundLayerAsset(layer.key, "color"), TextureFormat::Bc7Srgb, layer.file, {}});
      continue;
    }
    if (!layer.procedural.empty()) {
      work.push_back({ember::groundLayerAsset(layer.key, "color"), TextureFormat::Bc7Srgb, {}, layer.procedural});
      continue;
    }
    work.push_back({ember::groundLayerAsset(layer.key, "color"), TextureFormat::Bc7Srgb, publicFile(options.artRoot, layer.color), {}});
    if (!layer.normal.empty())
      work.push_back({ember::groundLayerAsset(layer.key, "normal"), TextureFormat::Bc5Unorm, publicFile(options.artRoot, layer.normal), {}});
    if (!layer.rough.empty())
      work.push_back({ember::groundLayerAsset(layer.key, "rough"), TextureFormat::Bc7Unorm, publicFile(options.artRoot, layer.rough), {}});
  }

  for (const Job& job : work) {
    ++report.textures;
    std::vector<uint8_t> source;
    if (job.procedural.empty() && !readFile(job.file, source)) {
      logError("ground: cannot read {}", job.file.string());
      ++report.failed;
      continue;
    }
    uint64_t key = fnv(0xcbf29ce484222325ull, source.data(), source.size());
    key = fnv(key, job.procedural.data(), job.procedural.size());
    const uint32_t versions[] = {kGroundCookerRevision, kTextureFormatVersion, static_cast<uint32_t>(job.format), static_cast<uint32_t>(options.quality)};
    key = fnv(key, versions, sizeof(versions));
    const std::filesystem::path cooked = options.cacheDir / job.asset;
    const std::filesystem::path keyFile = std::filesystem::path(cooked).concat(".key");
    std::vector<uint8_t> stored;
    if (readFile(keyFile, stored) && stored.size() == sizeof(key) && std::memcmp(stored.data(), &key, sizeof(key)) == 0 &&
        std::filesystem::exists(cooked)) {
      ++report.fromCache;
    } else {
      Rgba8 image;
      const bool decoded = job.procedural.empty() ? decode(source, image) : classicProceduralTexture(job.procedural, image);
      if (!decoded) {
        logError("ground: cannot decode {}", job.procedural.empty() ? job.file.string() : job.procedural);
        ++report.failed;
        continue;
      }
      const auto t0 = std::chrono::steady_clock::now();
      const bool backdrop = std::find(backdropAssets.begin(), backdropAssets.end(), job.asset) != backdropAssets.end();
      const std::vector<uint8_t> texture = cookTexture(jobs, image, job.format, backdrop ? Bc7Quality::Basic : options.quality);
      if (!writeFile(cooked, texture.data(), texture.size()) || !writeFile(keyFile, &key, sizeof(key))) {
        logError("ground: cannot write to the cache {}", options.cacheDir.string());
        ++report.failed;
        continue;
      }
      ++report.cooked;
      logInfo("{}: {}x{} -> {:.1f} MB in {:.1f}s", job.asset, image.width, image.height, texture.size() / 1048576.0,
              std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    report.bytes += std::filesystem::file_size(cooked);
    out.push_back({job.asset, cooked});
  }
  report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return report.failed == 0;
}

}  // namespace ember
