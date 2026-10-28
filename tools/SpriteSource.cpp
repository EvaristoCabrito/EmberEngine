#include "SpriteSource.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>

#include "core/Log.h"

namespace ember {

namespace {

using json = nlohmann::json;

const char* modeName(SpritePlayMode mode) {
  switch (mode) {
    case SpritePlayMode::Loop: return "loop";
    case SpritePlayMode::PingPong: return "pingpong";
    case SpritePlayMode::Once: return "once";
  }
  return "loop";
}

bool parseMode(const std::string& name, SpritePlayMode& out) {
  if (name == "loop") out = SpritePlayMode::Loop;
  else if (name == "pingpong") out = SpritePlayMode::PingPong;
  else if (name == "once") out = SpritePlayMode::Once;
  else return false;
  return true;
}

bool readJson(const std::filesystem::path& path, json& out) {
  std::ifstream file(path);
  if (!file) {
    logError("cannot open {}", path.string());
    return false;
  }
  try {
    out = json::parse(file);
  } catch (const json::exception& e) {
    logError("{}: {}", path.string(), e.what());
    return false;
  }
  return true;
}

// --- Ember pacing (src/units/visual.ts framePace), as fps + play mode ---------------------------

enum class Kind { Idle, Walk, OneShot };

Kind kindOf(const std::string& prefix) {
  if (prefix.empty() || prefix == "idle-" || prefix == "idle2-" || prefix == "stand-" || prefix == "talk-") return Kind::Idle;
  if (prefix.starts_with("move") || prefix == "walk-") return Kind::Walk;
  return Kind::OneShot;
}

// Battle moves load with the battle; everything else is out-of-combat.
std::string packOf(const std::string& prefix) {
  static const char* kCombat[] = {"atk", "cast", "heal", "hit", "death", "counter"};
  for (const char* p : kCombat)
    if (prefix.starts_with(p)) return "combat";
  return "field";
}

void pace(const std::string& dir, const std::string& prefix, uint32_t n, SpritePlayMode& mode, float& fps) {
  const std::string sprite = dir.substr(0, dir.find('/'));
  const bool longSheet = n >= 24;
  const double slow = sprite == "malrec" ? 0.9 : 1.0;  // Conjurer/Malrec sheets play 10% slower
  switch (kindOf(prefix)) {
    case Kind::Idle: {
      mode = SpritePlayMode::Loop;
      if (sprite == "neera") { fps = static_cast<float>(1.0 / 0.098); return; }
      if (sprite == "militia-v2") { fps = static_cast<float>(1.0 / 0.119); return; }
      if (sprite == "familiar") { fps = 5.5f; return; }
      if (sprite == "familiar2") { fps = static_cast<float>(5.5 * n / 12.0); return; }
      mode = SpritePlayMode::PingPong;
      const double cycle = std::max(2.0, n * 2.0 - 2.0);
      fps = static_cast<float>((longSheet ? n / 3.0 : cycle / 2.6) * slow);
      return;
    }
    case Kind::Walk: {
      mode = SpritePlayMode::Loop;
      // Video-cut walks loop at the footage's own length.
      static const std::map<std::string, std::map<std::string, double>> kFootage = {
          {"BasePikeman", {{"move-", 1.75}, {"move-left-", 5.0}, {"move-down-", 2.33}}},
          {"BaseCrossbowman", {{"move-", 4.75}, {"move-left-", 4.75}}},
          {"BrigandCrossbowman", {{"move-", 3.25}, {"move-left-", 3.25}}},
          {"StagWolf", {{"move-", 4.0}, {"move-left-", 4.0}}},
          {"voss-final", {{"move-", 3.07}, {"move-left-", 3.07}}},
      };
      if (const auto s = kFootage.find(sprite); s != kFootage.end())
        if (const auto p = s->second.find(prefix); p != s->second.end()) {
          fps = static_cast<float>(n / p->second);
          return;
        }
      fps = static_cast<float>(longSheet ? n / (sprite == "salazar-final" ? 1.9 : 1.5) : std::min(n / 2.0, 6.0) * slow / 0.22);
      return;
    }
    case Kind::OneShot: {
      mode = SpritePlayMode::Once;
      const double seconds = sprite == "apparition" && prefix == "atk-" ? 5.0 : longSheet ? 3.0 / slow : 0.8;
      fps = static_cast<float>(n / seconds);
      return;
    }
  }
}

}  // namespace

bool loadSpriteManifest(const std::filesystem::path& path, SpriteManifest& out) {
  json doc;
  if (!readJson(path, doc)) return false;
  try {
    if (doc.at("version").get<int>() != 1) {
      logError("{}: unsupported version", path.string());
      return false;
    }
    out.sets.clear();
    for (const json& set : doc.at("sets")) {
      SpriteSetSource s;
      s.name = set.at("name").get<std::string>();
      for (const json& clip : set.at("clips")) {
        SpriteClipSource c;
        c.name = clip.at("name").get<std::string>();
        c.pack = clip.value("pack", std::string("field"));
        c.fps = clip.value("fps", 12.0f);
        if (!parseMode(clip.value("mode", std::string("loop")), c.mode)) {
          logError("{}: {}/{} has an unknown mode", path.string(), s.name, c.name);
          return false;
        }
        const json& frames = clip.at("frames");
        if (frames.contains("file")) {
          c.frames.push_back(frames.at("file").get<std::string>());
        } else if (frames.contains("pattern")) {
          const std::string pattern = frames.at("pattern").get<std::string>();
          const size_t at = pattern.find("{}");
          const int count = frames.at("count").get<int>();
          if (at == std::string::npos || count <= 0) {
            logError("{}: {}/{} has a bad frame pattern", path.string(), s.name, c.name);
            return false;
          }
          for (int i = 1; i <= count; ++i)
            c.frames.push_back(pattern.substr(0, at) + std::to_string(i) + pattern.substr(at + 2));
        } else {
          for (const json& file : frames.at("list")) c.frames.push_back(file.get<std::string>());
        }
        s.clips.push_back(std::move(c));
      }
      out.sets.push_back(std::move(s));
    }
  } catch (const json::exception& e) {
    logError("{}: {}", path.string(), e.what());
    return false;
  }
  return true;
}

bool saveSpriteManifest(const std::filesystem::path& path, const SpriteManifest& manifest) {
  json sets = json::array();
  for (const SpriteSetSource& set : manifest.sets) {
    json clips = json::array();
    for (const SpriteClipSource& clip : set.clips) {
      json frames;
      if (clip.frames.size() == 1) {
        frames["file"] = clip.frames.front();
      } else {
        // Numbered frames ("idle-1.png".."idle-36.png") are written as a pattern.
        const std::string& first = clip.frames.front();
        const size_t one = first.rfind("1.");
        bool numbered = one != std::string::npos;
        const std::string stem = numbered ? first.substr(0, one) : "", ext = numbered ? first.substr(one + 1) : "";
        for (size_t i = 0; numbered && i < clip.frames.size(); ++i)
          numbered = clip.frames[i] == stem + std::to_string(i + 1) + ext;
        if (numbered) frames = {{"pattern", stem + "{}" + ext}, {"count", clip.frames.size()}};
        else frames["list"] = clip.frames;
      }
      clips.push_back({{"name", clip.name}, {"pack", clip.pack}, {"mode", modeName(clip.mode)},
                       {"fps", std::round(static_cast<double>(clip.fps) * 1000.0) / 1000.0}, {"frames", frames}});
    }
    sets.push_back({{"name", set.name}, {"clips", clips}});
  }
  std::filesystem::create_directories(path.parent_path().empty() ? "." : path.parent_path());
  std::ofstream file(path);
  file << json{{"version", 1}, {"sets", sets}}.dump(1) << '\n';
  return static_cast<bool>(file);
}

bool importSourceSprites(const std::filesystem::path& sourceRoot, SpriteManifest& out) {
  json manifest, cleanup;
  if (!readJson(sourceRoot / "src/units/manifest.json", manifest)) return false;
  if (!readJson(sourceRoot / "tools/sprite-cleanup.json", cleanup)) return false;
  const std::set<std::string> cleaned = cleanup.at("decontaminate").get<std::set<std::string>>();
  const std::filesystem::path game = sourceRoot / "public/game";

  // Single stills Ember shows while walking (tools/prepare-sprites.mjs STILLS).
  const std::map<std::string, std::vector<std::string>> stills = {
      {"kael", {"walk-front", "walk-back", "walk-side"}}, {"birolho", {"back"}}, {"birolho2", {"back"}}, {"punisher", {"front", "back"}}};

  out.sets.clear();
  size_t missing = 0;
  for (const auto& [dir, prefixes] : manifest.items()) {
    SpriteSetSource set;
    set.name = dir;
    const std::string base = (cleaned.contains(dir) ? "sprites-clean/" : "sprites/") + dir + "/";
    for (const auto& [prefix, countJson] : prefixes.items()) {
      const uint32_t n = countJson.get<uint32_t>();
      SpriteClipSource clip;
      clip.name = prefix.empty() ? "base" : prefix.substr(0, prefix.size() - (prefix.back() == '-' ? 1 : 0));
      clip.pack = packOf(prefix);
      pace(dir, prefix, n, clip.mode, clip.fps);
      bool complete = true;
      for (uint32_t i = 1; i <= n; ++i) {
        clip.frames.push_back(base + prefix + std::to_string(i) + ".png");
        if (!std::filesystem::exists(game / clip.frames.back())) complete = false;
      }
      if (!complete) {
        logWarn("ember {}{}: frames missing on disk, clip skipped", base, prefix);
        ++missing;
        continue;
      }
      set.clips.push_back(std::move(clip));
    }
    if (const auto s = stills.find(dir); s != stills.end()) {
      for (const std::string& name : s->second) {
        const std::string file = base + name + ".png";
        if (!std::filesystem::exists(game / file)) continue;
        set.clips.push_back({name, "field", SpritePlayMode::Loop, 1.0f, {file}});
      }
    }
    if (!set.clips.empty()) out.sets.push_back(std::move(set));
  }
  std::sort(out.sets.begin(), out.sets.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
  size_t clips = 0, frames = 0;
  for (const auto& set : out.sets) {
    clips += set.clips.size();
    for (const auto& clip : set.clips) frames += clip.frames.size();
  }
  logInfo("ember: {} sprite sets, {} clips, {} frames ({} clips skipped for missing frames)", out.sets.size(), clips,
          frames, missing);
  return true;
}

}  // namespace ember
