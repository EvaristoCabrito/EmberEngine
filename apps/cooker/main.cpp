// Cooker: builds the game's cooked data.
//   Cooker pack <content dir> <out.pak> [--level 1..22] [--store]   pack every file under a folder
//   Cooker list <file.pak>                                           print an archive's contents
//   Cooker synth <out dir> [--count N] [--min BYTES] [--max BYTES] [--seed S]
//                                                                    write synthetic test files
//   Cooker import-ember <ember root> <out.sprites.json>         sprite manifest from Ember's tables
//   Cooker sprites <manifest.sprites.json> <source root> <out.pak>  cook sprite clips (BC7) into an archive
//          [--only <set prefix>]... [--quality fast|basic|slow] [--copies N] [--cache <dir>]
//   Cooker meshes <source root> <out.pak> [--quality fast|basic|slow] [--cache <dir>] [--remesh N]
//          (--remesh: proxy-LOD experiment, every mesh replaced by its voxel remesh; separate archive)
//                                                                    cook every GLB under a folder (cluster LOD + textures)
//   Cooker ground <board.json> <ember public> <out.pak> [--cache <dir>] [--check <dir>] [--quality fast|basic|slow]
//                                                                    Ember's ground textures (procedural + photo layers)
//   Cooker check-ground <board.json> <map.json> <ember public> <reference dir>
//                                                                    compare the C++ board/ground with Ember's export
//   Cooker check-battle <map.battle.json>                            compare the C++ movement reach with Ember's export
//   Cooker check-battle-export <maps dir> <map.battle.json...>       the game's own battle export against the exporter's files
//   Cooker prepare-map <map.json> <out map.json>                     the campaign's party on a map (what the game does on opening it)
//   Cooker export-battle <map.json> <out.battle.json>                the game's own battle file of a prepared map
//   Cooker export-scene <board.json> <catalog.json> <public> <map.json> <out.scene.json>   its scene (SceneBuilder)
//   Cooker check-spell-tracks                                       every spell's numbers level by level: one change a level
//   Cooker check-abyssal-swap <map.battle.json>                      exercise the familiar's atomic enemy swap and resistance rules
//   Cooker check-twin-resonance <map.battle.json>                     exercise the Hipóstase's paired damage link and saves
//   Cooker check-campaign <campaign.json>                            compare the C++ save slots (slotProgress) with Ember's
//   Cooker check-overworld <overworld.json> <references.json>        compare the C++ world-map rules with Ember's answers
//   Cooker check-interlude <interlude.json> <overworld.json> <campaign.json>   the Interlude's gate / party switch / return on synthetic saves
//   Cooker check-scene-builder <board.json> <editor-catalog.json> <ember public> <scenes dir> <map.json...>
//                                                                    compare the C++ scene builder with export_scene.ts
//   Cooker sprite-preview <manifest> <source root> <cache dir> <set> <clip> <frame> <out.png>
//                                                                    source vs. GPU-decoded frame, side by side

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "world/Atmos2.h"
#include <chrono>
#include <cmath>
#include "world/SceneBuilder.h"
#include "world/MapDressing.h"
#include "ArchitectureCooker.h"
#include "world/Architecture.h"
#include "PakWriter.h"
#include "BattleCheck.h"
#include "world/Campaign.h"
#include "world/Overworld.h"
#include "world/Interlude.h"
#include "world/BattleExport.h"
#include "world/Progression.h"
#include "world/Spells.h"
#include "world/ActionPoints.h"
#include "world/Combat.h"
#include "GroundCheck.h"
#include "GroundCooker.h"
#include "MeshCooker.h"
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

int importSource(const std::vector<std::string_view>& args) {
  if (args.size() != 2) fatal("usage: Cooker import-ember <ember root> <out.sprites.json>");
  SpriteManifest manifest;
  if (!importSourceSprites(std::filesystem::path(args[0]), manifest)) return 1;
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

int meshes(const std::vector<std::string_view>& args) {
  if (args.size() < 2) fatal("usage: Cooker meshes <source root> <out.pak> [--quality fast|basic|slow] [--cache <dir>] [--remesh N]");
  MeshCookOptions options;
  options.sourceRoot = std::filesystem::path(args[0]);
  const std::filesystem::path out(args[1]);
  options.cacheDir = (out.parent_path().empty() ? std::filesystem::path(".") : out.parent_path()) / "cook-cache";
  for (size_t i = 2; i < args.size(); ++i) {
    if (i + 1 >= args.size()) fatal(std::format("Missing value for {}", args[i]));
    const std::string_view value = args[++i];
    if (args[i - 1] == "--cache") options.cacheDir = std::filesystem::path(value);
    else if (args[i - 1] == "--remesh") options.remeshResolution = static_cast<int>(parseNumber(value));
    else if (args[i - 1] == "--quality") {
      if (value == "fast") options.quality = Bc7Quality::Fast;
      else if (value == "basic") options.quality = Bc7Quality::Basic;
      else if (value == "slow") options.quality = Bc7Quality::Slow;
      else fatal(std::format("Unknown quality {}", value));
    } else {
      fatal(std::format("Unknown argument {}", args[i - 1]));
    }
  }
  if (options.remeshResolution != 0 && (options.remeshResolution < 4 || options.remeshResolution > 256))
    fatal("--remesh takes a resolution in 4..256");

  // Every .glb under the root, in a stable order.
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(options.sourceRoot)) {
    std::string ext = entry.path().extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (entry.is_regular_file() && ext == ".glb") files.push_back(std::filesystem::relative(entry.path(), options.sourceRoot));
  }
  std::sort(files.begin(), files.end());

  JobSystem jobs;
  std::vector<PakSource> sources;
  MeshCookReport report;
  const bool cooked = cookMeshes(jobs, files, options, sources, report);
  logInfo("meshes: {} ({} cooked, {} from cache, {} failed) in {:.1f}s", report.meshes, report.cooked, report.fromCache,
          report.failed, report.seconds);
  logInfo("  {} source triangles, {} clusters, {} triangles across all LOD levels; geometry {:.1f} MB, {} textures {:.1f} MB "
          "(+{} uses of a shared one)",
          report.sourceTriangles, report.clusters, report.dagTriangles, megabytes(report.geometryBytes), report.textures,
          megabytes(report.textureBytes), report.sharedTextureUses);
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

int ground(const std::vector<std::string_view>& args) {
  if (args.size() < 3) fatal("usage: Cooker ground <board.json> <ember public> <out.pak> [--cache <dir>] [--check <dir>] [--quality fast|basic|slow]");
  GroundCookOptions options;
  options.boardJson = std::filesystem::path(args[0]);
  options.artRoot = std::filesystem::path(args[1]);
  const std::filesystem::path out(args[2]);
  options.cacheDir = (out.parent_path().empty() ? std::filesystem::path(".") : out.parent_path()) / "cook-cache";
  for (size_t i = 3; i < args.size(); ++i) {
    if (i + 1 >= args.size()) fatal(std::format("Missing value for {}", args[i]));
    const std::string_view value = args[++i];
    if (args[i - 1] == "--cache") options.cacheDir = std::filesystem::path(value);
    else if (args[i - 1] == "--check") options.checkDir = std::filesystem::path(value);
    else if (args[i - 1] == "--quality") {
      if (value == "fast") options.quality = Bc7Quality::Fast;
      else if (value == "basic") options.quality = Bc7Quality::Basic;
      else if (value == "slow") options.quality = Bc7Quality::Slow;
      else fatal(std::format("Unknown quality {}", value));
    } else {
      fatal(std::format("Unknown argument {}", args[i - 1]));
    }
  }
  JobSystem jobs;
  std::vector<PakSource> sources;
  GroundCookReport report;
  const bool cooked = cookGround(jobs, options, sources, report);
  logInfo("ground: {} textures ({} cooked, {} from cache, {} failed), {:.1f} MB, in {:.1f}s", report.textures, report.cooked,
          report.fromCache, report.failed, megabytes(report.bytes), report.seconds);
  if (!cooked) return 1;
  PakWriteReport pak;
  if (!writePak(jobs, std::move(sources), out, {}, pak)) return 1;
  logInfo("packed {} assets: {:.1f} MB -> {:.1f} MB on disk in {:.1f}s", pak.entries, megabytes(pak.sourceBytes),
          megabytes(pak.storedBytes), pak.seconds);
  return 0;
}

int checkGroundCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 4) fatal("usage: Cooker check-ground <board.json> <map.json> <ember public> <reference dir>");
  return checkGround(std::filesystem::path(args[0]), std::filesystem::path(args[1]), std::filesystem::path(args[2]),
                     std::filesystem::path(args[3]))
             ? 0
             : 1;
}

int checkBattleCommand(const std::vector<std::string_view>& args) {
  if (args.empty()) fatal("usage: Cooker check-battle <map.battle.json>...");
  bool ok = true;
  for (std::string_view a : args) ok = checkBattle(std::filesystem::path(a)) && ok;
  return ok ? 0 : 1;
}

int checkSpellTracksCommand(const std::vector<std::string_view>& args) {
  std::vector<std::string> report;
  const std::vector<std::string> fails = ember::checkSpellTracks(args.empty() ? nullptr : &report);
  for (const std::string& line : report) ember::logInfo("{}", line);
  for (const std::string& f : fails) ember::logError("{}", f);
  ember::logInfo("spell tracks: {}", fails.empty() ? "one small improvement a level on every spell" : std::format("{} failures", fails.size()));
  return fails.empty() ? 0 : 1;
}

int checkAbyssalSwapCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 1) fatal("usage: Cooker check-abyssal-swap <map.battle.json>");
  std::optional<ember::Battle> loaded = ember::Battle::load(std::filesystem::path(args[0]));
  if (!loaded) return 1;
  ember::Battle& b = *loaded;
  for (int y = 0; y < b.rows(); ++y)
    for (int x = 0; x < b.cols(); ++x) b.setCell(y * b.cols() + x, ember::BattleCell{.terrain = "plains"});
  b.units().clear();
  ember::BattleUnit familiar, foe;
  familiar.id = "check-familiar", familiar.name = "Test Familiar", familiar.classId = "familiar2", familiar.className = "Familiar Médio";
  familiar.side = "player", familiar.alive = true, familiar.x = 3, familiar.y = 4, familiar.level = 4;
  familiar.mov = 5, familiar.mag = 0, familiar.hp = familiar.maxHp = 100, familiar.spellCharges = 2, familiar.ap = 100;
  ember::grantClassSpells(familiar);
  foe.id = "check-foe", foe.name = "Test Foe", foe.classId = "test", foe.side = "enemy", foe.alive = true;
  foe.x = 6, foe.y = 4, foe.level = 5, foe.mov = 5, foe.hp = foe.maxHp = 100;
  b.units().push_back(familiar), b.units().push_back(foe);
  auto require = [](bool ok, const char* why) {
    if (!ok) ember::logError("abyssal swap check: {}", why);
    return ok;
  };
  std::string castError;
  auto cast = [&]() { castError.clear(); return ember::castSpell(b, 0, "abyssalSwap", {{b.units()[1].x, b.units()[1].y}}, &castError); };
  auto knowsSwap = [&]() { return std::find(b.units()[0].spellList.begin(), b.units()[0].spellList.end(), "abyssalSwap") != b.units()[0].spellList.end(); };
  if (!require(!knowsSwap(), "the medium familiar should not learn the spell before level five")) return 1;
  b.units()[0].level = 4;
  ember::grantClassSpells(b.units()[0]);
  if (!require(!knowsSwap(), "level four must not unlock the spell")) return 1;
  b.units()[0].level = 5;
  ember::grantClassSpells(b.units()[0]);
  if (!require(knowsSwap(), "level five must unlock the spell")) return 1;
  if (!require(ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "visible target and both destinations should be valid")) return 1;
  if (!require(ember::spellUsesLeft(b, 0, "abyssalSwap") == 1, "initial stage should require two charges per cast")) return 1;
  auto steps = cast();
  if (!require(steps.has_value(), castError.empty() ? "cast should be accepted" : castError.c_str())) return 1;
  if (!require(b.units()[0].spellCharges == 0, "cast should spend both charges")) return 1;
  std::vector<ember::BattleEffect> effects;
  ember::Mulberry32 rng(123);
  ember::resolveStep(b, steps->front(), rng, effects);
  if (!require(b.units()[0].x == 6 && b.units()[1].x == 3 && b.unitAt(6, 4) == 0 && b.unitAt(3, 4) == 1,
               "successful cast must atomically exchange both occupied hexes")) return 1;

  b.units()[0].x = 3, b.units()[1].x = 6, b.units()[0].spellCharges = 2;
  b.units()[1].x = 7;
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {7, 4}), "initial stage must enforce its short range")) return 1;
  b.units()[1].x = 6;
  b.units()[1].combat.resistances = {{"arcane", 100.0}};
  steps = cast();
  if (!require(steps.has_value(), "fully resisting target must still be a valid cast")) return 1;
  effects.clear();
  ember::Mulberry32 resistRng(9);
  ember::resolveStep(b, steps->front(), resistRng, effects);
  if (!require(b.units()[0].x == 3 && b.units()[1].x == 6 && b.units()[0].spellCharges == 0 && b.units()[0].hp == 100 && b.units()[1].hp == 100 &&
                   std::any_of(effects.begin(), effects.end(), [](const ember::BattleEffect& e) { return e.miss; }),
               "resistance should keep positions and still consume cast charges")) return 1;

  b.units()[0].spellCharges = 2, b.units()[1].combat.resistances.clear();
  b.units()[1].size = 2;
  if (!require(ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "a valid large-creature footprint should be swappable")) return 1;
  steps = cast();
  if (!require(steps.has_value(), "a large target with valid footprints should cast")) return 1;
  effects.clear();
  ember::Mulberry32 largeRng(31);
  ember::resolveStep(b, steps->front(), largeRng, effects);
  if (!require(b.units()[0].x == 6 && b.units()[1].x == 3 && b.unitAt(3, 4) == 1 && b.unitAt(4, 4) == 1,
               "a large creature should swap as one body and occupy every destination hex")) return 1;
  b.units()[0].x = 3, b.units()[1].x = 6, b.units()[0].spellCharges = 2;
  ember::BattleUnit blocker;
  blocker.id = "check-blocker", blocker.side = "enemy", blocker.alive = true, blocker.x = 4, blocker.y = 4;
  b.units().push_back(blocker);
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "another unit in either destination footprint must reject the whole exchange")) return 1;
  b.units().back().alive = false;
  b.setCell(4 * b.cols() + 4, ember::BattleCell{.terrain = "plains", .blocked = true});
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "a blocked hex in a large footprint must invalidate the entire exchange")) return 1;
  b.setCell(4 * b.cols() + 4, ember::BattleCell{.terrain = "plains"});
  b.units()[1].size = 1;
  b.setCell(4 * b.cols() + 4, ember::BattleCell{.terrain = "plains", .shot = {.blocksShot = true}});
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "a blocked line of sight must reject targeting")) return 1;
  b.setCell(4 * b.cols() + 4, ember::BattleCell{.terrain = "plains"});
  b.units()[1].hidden = true;
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "hidden targets must not be valid")) return 1;
  b.units()[1].hidden = false;
  b.units()[1].size = 1, b.units()[1].mov = 0;
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "movement-immune targets must be rejected before casting")) return 1;
  b.units()[1].mov = 5, b.units()[1].cond.burialRootSources = {"test-anchor"};
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "rooted targets must be immune to teleport displacement")) return 1;
  b.units()[1].cond.burialRootSources.clear();
  b.setCell(4 * b.cols() + 3, ember::BattleCell{.terrain = "void", .passable = false});
  if (!require(!ember::spellAimValid(b, 0, "abyssalSwap", {6, 4}), "void at either destination must reject the exchange")) return 1;
  b.setCell(4 * b.cols() + 3, ember::BattleCell{.terrain = "plains"});

  b.units()[0].level = 12, b.units()[0].spellCharges = 2;
  steps = cast();
  if (!require(steps && steps->front().resistanceBonus == 10, "intermediate stage should improve the resistance check")) return 1;
  b.units()[0].level = 20, b.units()[0].spellCharges = 2, b.units()[1].x = 10;
  steps = cast();
  if (!require(steps && steps->front().resistanceBonus == 20 && b.units()[0].spellCharges == 1,
               "advanced stage should extend range, improve resistance and cost one charge")) return 1;

  b.units()[0].x = 3, b.units()[1].x = 6, b.units()[0].level = 5, b.units()[0].spellCharges = 2;
  b.setCell(4 * b.cols() + 3, ember::BattleCell{.terrain = "flame", .hazardDice = 1, .hazardFaces = 1});
  b.setCell(4 * b.cols() + 6, ember::BattleCell{.terrain = "ember", .hazardDice = 1, .hazardFaces = 1});
  steps = cast();
  if (!require(steps.has_value(), "dangerous but passable destinations should remain valid")) return 1;
  effects.clear();
  ember::Mulberry32 hazardRng(77);
  ember::resolveStep(b, steps->front(), hazardRng, effects);
  if (!require(b.units()[0].hp == 99 && b.units()[1].hp == 99,
               "each creature should enter its dangerous destination exactly once")) return 1;
  ember::logInfo("abyssal swap checks passed: success, resistance/cost, large footprint, blocked/void destinations, movement immunity, progression, hazard entry");
  return 0;
}

int checkTwinResonanceCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 1) fatal("usage: Cooker check-twin-resonance <map.battle.json>");
  std::optional<ember::Battle> loaded = ember::Battle::load(std::filesystem::path(args[0]));
  if (!loaded) return 1;
  ember::Battle& b = *loaded;
  for (int y = 0; y < b.rows(); ++y)
    for (int x = 0; x < b.cols(); ++x) b.setCell(y * b.cols() + x, ember::BattleCell{.terrain = "plains"});
  b.units().clear();
  ember::BattleUnit caster;
  caster.id = "check-hypostasis", caster.name = "Hipóstase de Teste", caster.classId = "hypostasis", caster.side = "player";
  caster.alive = true, caster.x = 2, caster.y = 2, caster.level = 1, caster.mag = 30, caster.hp = caster.maxHp = 100, caster.ap = 20;
  ember::grantClassSpells(caster);
  ember::BattleUnit a, d, attacker;
  a.id = "check-resonance-a", a.side = "enemy", a.alive = true, a.x = 4, a.y = 2, a.hp = a.maxHp = 100;
  d.id = "check-resonance-b", d.side = "enemy", d.alive = true, d.x = 4, d.y = 3, d.hp = d.maxHp = 100;
  attacker.id = "check-resonance-attacker", attacker.side = "player", attacker.alive = true, attacker.x = 1, attacker.y = 4;
  attacker.hp = attacker.maxHp = 100, attacker.mag = 30;
  b.units() = {caster, a, d, attacker};
  auto require = [](bool ok, const char* why) {
    if (!ok) ember::logError("twin resonance check: {}", why);
    return ok;
  };
  // MOV is no longer a battle movement budget: AP alone pays for each entered hex.
  ember::Battle movement = b;
  movement.units() = {caster};
  movement.units()[0].mov = 0, movement.units()[0].moveUsed = 0, movement.units()[0].ap = 6;
  for (int y = 0; y < movement.rows(); ++y)
    for (int x = 0; x < movement.cols(); ++x) movement.setCell(y * movement.cols() + x, ember::BattleCell{.terrain = "woods", .moveCost = 2});
  const auto moveReach = movement.reachable(0);
  if (!require(std::any_of(moveReach.begin(), moveReach.end(), [](const ember::ReachCell& c) { return c.x == 5 && c.y == 2 && c.ap == 6; }),
               "three woods hexes should be reachable when their 6 AP are available, regardless of MOV")) return 1;
  if (!require(std::none_of(moveReach.begin(), moveReach.end(), [](const ember::ReachCell& c) { return c.x == 6 && c.y == 2; }),
               "movement must stop at the AP budget")) return 1;
  const ember::BattleHex rayFrom{2, 2}, rayTo{6, 3};
  const auto ray = ember::hexLine(rayFrom, rayTo);
  auto reverseRay = ember::hexLine(rayTo, rayFrom);
  std::reverse(reverseRay.begin(), reverseRay.end());
  if (!require(ray == reverseRay, "line of sight must use the same hex ray from either direction")) return 1;
  if (ray.size() > 2) {
    const ember::BattleHex blocker = ray[1];
    b.setCell(blocker.y * b.cols() + blocker.x, ember::BattleCell{.terrain = "plains", .shot = {.blocksShot = true}});
    if (!require(!ember::clearShot(b, rayFrom, rayTo, false) && !ember::clearShot(b, rayTo, rayFrom, false),
                 "a blocker must interrupt line of sight in either direction")) return 1;
    b.setCell(blocker.y * b.cols() + blocker.x, ember::BattleCell{.terrain = "plains"});
  }
  if (!require(ember::apCost(b, b.units()[0], ember::ApAction::cast("twinResonance")) == 8, "each cast must cost exactly 8 AP")) return 1;
  if (!require(ember::spellTargetCount(b, 0, "twinResonance") == 2, "spell must keep two targets at every rank")) return 1;
  if (!require(ember::spellAimValid(b, 0, "twinResonance", {4, 2}) && ember::spellAimValid(b, 0, "twinResonance", {4, 3}),
               "visible enemies in range and clear line of sight should be valid")) return 1;
  if (!require(!ember::castSpell(b, 0, "twinResonance", {{4, 2}, {4, 2}}, nullptr), "same target cannot fill both picks")) return 1;
  b.units()[2].x = 7;
  if (!require(!ember::castSpell(b, 0, "twinResonance", {{4, 2}, {7, 3}}, nullptr), "the rank's target-gap limit must be enforced")) return 1;
  b.units()[2].x = 4;
  for (const auto [level, gap] : {std::pair{1, 3}, std::pair{16, 4}, std::pair{31, 5}, std::pair{46, 6}, std::pair{60, 7}}) {
    b.units()[0].level = level;
    if (!require(ember::twinResonanceTargetGap(b.units()[0]) == gap, "rank progression must advance the paired-target gap")) return 1;
  }
  b.units()[0].level = 1;
  auto castPair = [&](size_t first, size_t second) { return ember::castSpell(b, 0, "twinResonance", {{b.units()[first].x, b.units()[first].y}, {b.units()[second].x, b.units()[second].y}}, nullptr); };
  auto steps = castPair(1, 2);
  if (!require(steps.has_value(), "eligible pair should cast")) return 1;
  std::vector<ember::BattleEffect> fx;
  ember::Mulberry32 rng(701);
  ember::resolveStep(b, steps->front(), rng, fx);
  if (!require(b.units()[0].cond.twinResonancePercent == 20 && b.units()[0].cond.twinResonanceTargets.size() == 2,
               "two failed saves should establish the initial 20% link")) return 1;
  ember::BattleStep hit;
  hit.att = 3, hit.kind = "fireball", hit.ids = {1}, hit.tiles = {{4, 2}}, hit.dice = 2, hit.faces = 50, hit.spellMul = 1.0;
  const int beforeA = b.units()[1].hp, beforeD = b.units()[2].hp;
  fx.clear();
  ember::resolveStep(b, hit, rng, fx);
  const int lostA = beforeA - b.units()[1].hp, lostD = beforeD - b.units()[2].hp;
  if (!require(lostA > 0 && lostD > 0 && lostD <= lostA / 5 &&
                   std::any_of(fx.begin(), fx.end(), [](const ember::BattleEffect& e) { return e.text == "Ressonância Geminada"; }),
               "direct damage must echo a resisted fraction of actual HP loss")) return 1;
  hit.att = 1, hit.ids = {2}, hit.tiles = {{4, 3}};
  const int alliedEchoBefore = b.units()[1].hp;
  fx.clear();
  ember::resolveStep(b, hit, rng, fx);
  if (!require(b.units()[1].hp == alliedEchoBefore, "damage from the target's own side must not echo")) return 1;
  b.units()[1].hp = b.units()[1].maxHp, b.units()[1].alive = true;
  b.units()[2].hp = b.units()[2].maxHp, b.units()[2].alive = true;
  hit.att = 3, hit.ids = {1, 2}, hit.tiles = {{4, 2}, {4, 3}};
  fx.clear();
  ember::resolveStep(b, hit, rng, fx);
  if (!require(std::count_if(fx.begin(), fx.end(), [](const ember::BattleEffect& e) { return e.text == "Ressonância Geminada"; }) == 2,
               "an area impact on both linked targets should echo each original hit once")) return 1;
  b.units()[1].hp = b.units()[1].maxHp, b.units()[1].alive = true;
  b.units()[2].hp = b.units()[2].maxHp, b.units()[2].alive = true;
  b.units()[0].cond.twinResonancePercent = 0, b.units()[0].cond.twinResonanceTargets.clear();
  b.units()[1].combat.resistances = {{"arcane", 100.0}};
  steps = castPair(1, 2);
  if (!require(steps.has_value(), "an immune target remains a valid attempted cast")) return 1;
  fx.clear();
  ember::resolveStep(b, steps->front(), rng, fx);
  if (!require(b.units()[0].cond.twinResonancePercent == 0, "one target resisting must fizzle the entire new link")) return 1;
  b.units()[1].combat.resistances.clear();
  steps = castPair(1, 2);
  if (!require(steps.has_value(), "the pair should cast after the immunity is removed")) return 1;
  fx.clear();
  ember::resolveStep(b, steps->front(), rng, fx);
  if (!require(b.units()[0].cond.twinResonancePercent == 20, "successful saves should reestablish the pair")) return 1;
  b.units()[1].combat.resistances = {{"arcane", 100.0}};
  steps = castPair(1, 2);
  if (!require(steps.has_value(), "an immune target remains a valid attempted recast")) return 1;
  fx.clear();
  ember::resolveStep(b, steps->front(), rng, fx);
  if (!require(b.units()[0].cond.twinResonancePercent == 20, "a resisted recast must leave the previous successful link intact")) return 1;
  b.units()[1].combat.resistances.clear();
  ember::BattleUnit third;
  third.id = "check-resonance-c", third.side = "enemy", third.alive = true, third.x = 3, third.y = 3, third.hp = third.maxHp = 100;
  b.units().push_back(third);
  steps = castPair(2, 4);
  if (!require(steps.has_value(), "a replacement pair should cast")) return 1;
  fx.clear();
  ember::resolveStep(b, steps->front(), rng, fx);
  if (!require(b.units()[0].cond.twinResonanceTargets == std::vector<std::string>{b.units()[2].id, b.units()[4].id},
               "a successful new cast must replace the previous pair")) return 1;
  b.units()[2].hp = 5;
  hit.att = 3, hit.kind = "fireball", hit.ids = {2}, hit.tiles = {{4, 3}}, hit.dice = 4, hit.faces = 100;
  const int beforeFatalEcho = b.units()[4].hp;
  fx.clear();
  ember::resolveStep(b, hit, rng, fx);
  if (!require(!b.units()[2].alive && b.units()[4].hp == beforeFatalEcho - 1 && b.units()[0].cond.twinResonancePercent == 0,
               "a fatal hit must transmit its echo once and then end the link")) return 1;
  steps = castPair(1, 4);
  if (!require(steps.has_value(), "the Hipóstase can establish a fresh pair after a fatal link ends")) return 1;
  fx.clear();
  ember::resolveStep(b, steps->front(), rng, fx);
  ember::beginUnitTurn(b, 0, rng, fx);
  if (!require(b.units()[0].cond.twinResonancePercent == 0 && b.units()[0].cond.twinResonanceTargets.empty(),
               "the link must expire at the beginning of the Hipóstase's next turn")) return 1;
  ember::logInfo("twin resonance checks passed: AP cost, two picks, rank gap, direct echo, ally immunity, resisted replacement, next-turn expiry");
  return 0;
}

int checkCampaignCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 1) fatal("usage: Cooker check-campaign <campaign.json>");
  ember::Campaign campaign;
  const std::filesystem::path scratch = std::filesystem::temp_directory_path() / "ember-check-campaign.json";
  if (!campaign.open(std::filesystem::path(args[0]), scratch)) return 1;
  return ember::checkSlotProgress(campaign) == 0 ? 0 : 1;
}

int checkOverworldCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 2) fatal("usage: Cooker check-overworld <overworld.json> <references.json>");
  ember::Overworld world;
  if (!world.load(std::filesystem::path(args[0]))) return 1;
  return ember::checkOverworld(world, std::filesystem::path(args[1])) == 0 ? 0 : 1;
}

// The Interlude's campaign rules on synthetic saves (world/Interlude.h: the Ruins' gate, begin / advance / complete, Isolde's
// recruitment, an old save).
int checkInterludeCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 3) fatal("usage: Cooker check-interlude <interlude.json> <overworld.json> <campaign.json>");
  ember::InterludeData data;
  std::string why;
  if (!data.load(std::string(args[0]), &why)) {
    ember::logError("check-interlude: {}", why);
    return 1;
  }
  ember::Overworld world;
  if (!world.load(std::filesystem::path(args[1]))) return 1;
  ember::Campaign campaign;
  const std::filesystem::path scratch = std::filesystem::temp_directory_path() / "ember-check-interlude.json";
  if (!campaign.open(std::filesystem::path(args[2]), scratch)) return 1;
  return ember::checkInterlude(data, world, campaign.emptySave()) == 0 ? 0 : 1;
}

// The game's own map preparation by hand (what the game does when a mission opens: apps/sandbox/MapExport.h).
int prepareMapCommand(const std::vector<std::string_view>& args) {
  if (args.size() < 2) fatal("usage: Cooker prepare-map <map.json> <out map.json>");
  std::string note;
  if (!ember::prepareCampaignMap(std::filesystem::path(args[0]), std::filesystem::path(args[1]), &note)) return 1;
  logInfo("{}", note);
  return 0;
}

int exportBattleCommand(const std::vector<std::string_view>& args) {
  if (args.size() < 2) fatal("usage: Cooker export-battle <map.json> <out.battle.json> [--player-level N]");
  int playerLevel = 1;
  for (size_t i = 2; i + 1 < args.size(); ++i)
    if (args[i] == "--player-level") playerLevel = static_cast<int>(parseNumber(args[++i]));
  std::ifstream in(std::filesystem::path(args[0]), std::ios::binary);
  const nlohmann::json map = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
  if (!in || map.is_discarded()) fatal(std::format("cannot read {}", args[0]));
  std::string error;
  const std::optional<nlohmann::json> battle = ember::exportBattle(map, &error, playerLevel);
  if (!battle) fatal(std::format("{}: {}", args[0], error));
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(args[1]).parent_path(), ec);
  std::ofstream out(std::filesystem::path(args[1]), std::ios::binary);
  if (!out) fatal(std::format("cannot write {}", args[1]));
  out << battle->dump();
  logInfo("{}: {}x{}, {} units, {} locks, {} exits", (*battle)["map"].get<std::string>(), (*battle)["cols"].get<int>(), (*battle)["rows"].get<int>(),
          (*battle)["units"].size(), (*battle)["locks"].size(), (*battle)["exits"].size());
  return 0;
}

int exportSceneCommand(const std::vector<std::string_view>& args) {
  if (args.size() < 5) fatal("usage: Cooker export-scene <board.json> <editor-catalog.json> <ember public> <map.json> <out.scene.json>");
  const std::optional<ember::BoardData> data = ember::BoardData::load(std::filesystem::path(args[0]));
  ember::EditorCatalog catalog;
  if (!data || !catalog.load(std::filesystem::path(args[1]))) return 1;
  std::optional<ember::MapDraft> draft = ember::MapDraft::load(std::filesystem::path(args[3]));
  if (!draft) return 1;
  const ember::MapDraft copy = *draft;
  ember::Board board(*data, std::move(*draft));
  if (!board.loadRelief(std::filesystem::path(args[2]))) logWarn("{}: relief not read from {}", args[3], args[2]);
  std::map<std::string, int> skipped;
  const ember::SceneData scene = ember::buildScene(board, catalog, &skipped);
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(args[4]).parent_path(), ec);
  std::ofstream out(std::filesystem::path(args[4]), std::ios::binary);
  if (!out) fatal(std::format("cannot write {}", args[4]));
  out << ember::sceneJson(scene, copy, skipped).dump(1);
  logInfo("{}: {} cards, {} models, {} architecture, {} units; skipped {}", scene.map, scene.cards.size(), scene.models.size(), scene.architecture, scene.units.size(), nlohmann::json(skipped).dump());
  return 0;
}

// The game's own battle export (world/BattleExport.cpp) against the TypeScript exporter's files: <maps dir>/<stem>.json
// is each reference's map.
int checkBattleExportCommand(const std::vector<std::string_view>& args) {
  if (args.size() < 2) fatal("usage: Cooker check-battle-export <maps dir> [--player-level N] <reference.battle.json...>");
  std::vector<std::filesystem::path> references;
  int playerLevel = 1;
  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--player-level" && i + 1 < args.size()) playerLevel = static_cast<int>(parseNumber(args[++i]));
    else references.emplace_back(args[i]);
  }
  return ember::checkBattleExport(std::filesystem::path(args[0]), references, playerLevel) == 0 ? 0 : 1;
}

int checkSceneBuilderCommand(const std::vector<std::string_view>& args) {
  if (args.size() < 5) fatal("usage: Cooker check-scene-builder <board.json> <editor-catalog.json> <ember public> <scenes dir> <map.json...>");
  const std::optional<ember::BoardData> data = ember::BoardData::load(std::filesystem::path(args[0]));
  ember::EditorCatalog catalog;
  if (!data || !catalog.load(std::filesystem::path(args[1]))) return 1;
  std::vector<std::filesystem::path> maps;
  for (size_t i = 4; i < args.size(); ++i) maps.emplace_back(args[i]);
  return ember::checkSceneBuilder(*data, catalog, maps, std::filesystem::path(args[3]), std::filesystem::path(args[2])) == 0 ? 0 : 1;
}

int architecture(const std::vector<std::string_view>& args) {
  if (args.size() < 3) fatal("usage: Cooker architecture <editor-catalog.json> <ember public> <out.pak> [--cache <dir>] [--quality fast|basic|slow]");
  ArchitectureCookOptions options;
  options.catalog = std::filesystem::path(args[0]);
  options.artRoot = std::filesystem::path(args[1]);
  const std::filesystem::path out(args[2]);
  options.cacheDir = (out.parent_path().empty() ? std::filesystem::path(".") : out.parent_path()) / "cook-cache";
  for (size_t i = 3; i < args.size(); ++i) {
    if (i + 1 >= args.size()) fatal(std::format("Missing value for {}", args[i]));
    const std::string_view value = args[++i];
    if (args[i - 1] == "--cache") options.cacheDir = std::filesystem::path(value);
    else if (args[i - 1] == "--quality") options.quality = value == "fast" ? Bc7Quality::Fast : value == "basic" ? Bc7Quality::Basic : Bc7Quality::Slow;
    else fatal(std::format("Unknown argument {}", args[i - 1]));
  }
  JobSystem jobs;
  std::vector<PakSource> sources;
  if (!cookArchitecture(jobs, options, sources)) return 1;
  PakWriteReport pak;
  if (!writePak(jobs, std::move(sources), out, {}, pak)) return 1;
  logInfo("packed {} assets: {:.1f} MB -> {:.1f} MB on disk in {:.1f}s", pak.entries, megabytes(pak.sourceBytes), megabytes(pak.storedBytes), pak.seconds);
  return 0;
}

int checkArchitectureCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 2) fatal("usage: Cooker check-architecture <editor-catalog.json> <architecture-refs.json (export_architecture_refs.ts)>");
  ember::ArchitectureDefs defs;
  if (!defs.load(std::filesystem::path(args[0]))) return 1;
  return ember::checkArchitecture(std::filesystem::path(args[1]), defs) == 0 ? 0 : 1;
}

int checkDressCommand(const std::vector<std::string_view>& args) {
  if (args.size() != 2) fatal("usage: Cooker check-dress <editor-catalog.json> <dress-refs.json (export_dress_refs.ts)>");
  ember::EditorCatalog catalog;
  ember::DressTables tables;
  if (!catalog.load(std::filesystem::path(args[0])) || !tables.load(std::filesystem::path(args[0]))) return 1;
  return ember::checkDress(std::filesystem::path(args[1]), tables, catalog) == 0 ? 0 : 1;
}


// Atmos 2 (docs/ATMOS2.md): the living atmosphere's offline spin-up and acceptance tests.
int atmos2Spinup(const std::vector<std::string_view>& args) {
  if (args.empty()) { ember::logError("usage: Cooker atmos2-spinup <out.bin> [days=75]"); return 1; }
  const int days = args.size() > 1 ? std::stoi(std::string(args[1])) : 75;
  ember::atmos2::Core core;
  core.coldStart();
  const auto t0 = std::chrono::steady_clock::now();
  const std::int64_t steps = static_cast<std::int64_t>(days) * 86400 / static_cast<std::int64_t>(ember::atmos2::kDt);
  for (std::int64_t s = 0; s < steps; ++s) {
    core.tick();
    if (!core.finite()) { ember::logError("atmos2-spinup: NaN at step {} (day {:.2f})", core.step(), core.worldSeconds() / 86400.0); return 2; }
    if (s % 4800 == 4799)
      ember::logInfo("atmos2-spinup: day {:.0f}: mass {:.2f} hPa, max wind {:.1f} m/s, absorbed {:.1f} / emitted {:.1f} W/m2", core.worldSeconds() / 86400.0,
                     core.totalMass(), core.maxWind(), core.meanAbsorbed(), core.meanEmitted());
  }
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (!core.save(std::string(args[0]))) { ember::logError("atmos2-spinup: cannot write {}", args[0]); return 1; }
  ember::logInfo("atmos2-spinup: {} steps ({} days) in {:.1f} s = {:.2f} ms a step; state {} at step {}", steps, days, ms / 1000.0, ms / static_cast<double>(steps), args[0], core.step());
  return 0;
}

int atmos2Test(const std::vector<std::string_view>& args) {
  if (args.empty()) { ember::logError("usage: Cooker atmos2-test <state.bin> [days=90]"); return 1; }
  const int days = args.size() > 1 ? std::stoi(std::string(args[1])) : 90;
  ember::atmos2::Core a, b;
  if (!a.load(std::string(args[0]))) { ember::logError("atmos2-test: cannot read {}", args[0]); return 1; }
  b = a;
  bool pass = true;
  // Repeatable: two runs from the same state give the same bits
  a.run(200); b.run(200);
  const bool sameBits = a.exportState() == b.exportState();
  ember::logInfo("atmos2-test repeatable (200 steps, same bits): {}", sameBits ? "PASS" : "FAIL");
  pass &= sameBits;
  // Round trip: export -> import -> export identical
  ember::atmos2::Core c;
  const bool roundTrip = c.importState(a.exportState()) && c.exportState() == a.exportState();
  ember::logInfo("atmos2-test state round trip: {}", roundTrip ? "PASS" : "FAIL");
  pass &= roundTrip;
  // Stable: N days, no NaN, jets between 40 and 100 m/s; energy balance; moving storms at 45 deg; speed
  const std::int64_t steps = static_cast<std::int64_t>(days) * 86400 / static_cast<std::int64_t>(ember::atmos2::kDt);
  const int row45 = static_cast<int>((45.0 - ember::atmos2::kLatSouth) / ember::atmos2::kDlat);
  std::vector<double> psMin(ember::atmos2::kNx, 1e9), psMax(ember::atmos2::kNx, -1e9), psMean(ember::atmos2::kNx, -1.0);
  double absorbed = 0.0, emitted = 0.0, maxWind = 0.0, worstMs = 0.0, totalMs = 0.0;
  int lowCentreFirst = -1, lowCentreLast = -1;
  std::int64_t eastward = 0, westward = 0;
  int prevCentre = -1;
  for (std::int64_t s = 0; s < steps; ++s) {
    const auto t0 = std::chrono::steady_clock::now();
    a.tick();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    totalMs += ms; worstMs = std::max(worstMs, ms);
    if (!a.finite()) { ember::logError("atmos2-test: NaN at step {} (day {:.2f})", a.step(), a.worldSeconds() / 86400.0); return 2; }
    if (s % 20 == 0) {  // every hour
      maxWind = std::max(maxWind, a.maxWind());
      absorbed += a.meanAbsorbed(); emitted += a.meanEmitted();
      int centre = 0; double lowest = 1e9;
      for (int i = 0; i < ember::atmos2::kNx; ++i) {
        const double p = a.ps()[static_cast<size_t>(row45 * ember::atmos2::kNx + i)];
        psMin[static_cast<size_t>(i)] = std::min(psMin[static_cast<size_t>(i)], p); psMax[static_cast<size_t>(i)] = std::max(psMax[static_cast<size_t>(i)], p);
        double& mean = psMean[static_cast<size_t>(i)];
        mean = mean < 0.0 ? p : mean + (p - mean) / 240.0;  // a 10-day running mean
        const double anomaly = p - mean;
        if (anomaly < lowest) { lowest = anomaly; centre = i; }
      }
      if (prevCentre >= 0) {
        const int d = ((centre - prevCentre) % ember::atmos2::kNx + ember::atmos2::kNx + ember::atmos2::kNx / 2) % ember::atmos2::kNx - ember::atmos2::kNx / 2;
        if (d > 0 && d < 8) ++eastward; else if (d < 0 && d > -8) ++westward;
      }
      prevCentre = centre;
      if (lowCentreFirst < 0) lowCentreFirst = centre;
      lowCentreLast = centre;
    }
  }
  const double hours = static_cast<double>(steps / 20);
  double maxSwing = 0.0;
  for (int i = 0; i < ember::atmos2::kNx; ++i) maxSwing = std::max(maxSwing, psMax[static_cast<size_t>(i)] - psMin[static_cast<size_t>(i)]);
  const bool stable = maxWind >= 40.0 && maxWind <= 100.0;
  const bool energy = std::abs(absorbed / hours - emitted / hours) <= 5.0;
  const bool storms = maxSwing > 3.0 && eastward > westward;
  const bool speed = totalMs / static_cast<double>(steps) < 3.0;
  ember::logInfo("atmos2-test stable ({} days, no NaN; strongest wind {:.1f} m/s, want 40..100): {}", days, maxWind, stable ? "PASS" : "FAIL");
  ember::logInfo("atmos2-test energy balance (absorbed {:.1f}, emitted {:.1f} W/m2, within 5): {}", absorbed / hours, emitted / hours, energy ? "PASS" : "FAIL");
  ember::logInfo("atmos2-test moving storms at 45 deg (ps swing {:.1f} hPa > 3; low centre moved east {} h, west {} h; first cell {}, last {}): {}", maxSwing, eastward, westward, lowCentreFirst, lowCentreLast, storms ? "PASS" : "FAIL");
  ember::logInfo("atmos2-test speed ({:.2f} ms a step, worst {:.2f}; under 3): {}", totalMs / static_cast<double>(steps), worstMs, speed ? "PASS" : "FAIL");
  {
    static const char* kParts[10] = {"transport", "forces", "continuity", "vertical", "radiation", "surface", "convection", "phase", "rain", "smoothing"};
    std::string prof;
    for (int i = 0; i < 10; ++i) prof += std::format("{} {:.2f}  ", kParts[i], a.profile_[static_cast<size_t>(i)] / static_cast<double>(steps + 200));
    ember::logInfo("atmos2-test ms a step by part: {}", prof);
  }
  pass &= stable && energy && storms && speed;
  ember::logInfo("atmos2-test: {}", pass ? "PASS" : "FAIL");
  return pass ? 0 : 3;
}

// The weather at one point, hour by hour: what the game would read.
int atmos2Sample(const std::vector<std::string_view>& args) {
  if (args.size() < 3) { ember::logError("usage: Cooker atmos2-sample <state.bin> <lon> <lat> [hours=48]"); return 1; }
  ember::atmos2::Core core;
  if (!core.load(std::string(args[0]))) { ember::logError("atmos2-sample: cannot read {}", args[0]); return 1; }
  const double lon = std::stod(std::string(args[1])), lat = std::stod(std::string(args[2]));
  const int hours = args.size() > 3 ? std::stoi(std::string(args[3])) : 48;
  const int i = ((static_cast<int>(std::floor((lon + 180.0) / ember::atmos2::kDlon)) % ember::atmos2::kNx) + ember::atmos2::kNx) % ember::atmos2::kNx;
  const int j = std::min(ember::atmos2::kNy - 1, std::max(0, static_cast<int>(std::floor((lat - ember::atmos2::kLatSouth) / ember::atmos2::kDlat))));
  const int cell = j * ember::atmos2::kNx + i;
  ember::logInfo("atmos2-sample: cell ({}, {}) {} at {} m", i, j, core.land()[static_cast<size_t>(cell)] ? "land" : "sea", core.terrain()[static_cast<size_t>(cell)]);
  for (int h = 0; h < hours; ++h) {
    core.run(20);
    const size_t s = static_cast<size_t>(cell);
    ember::logInfo("h {:3}: T {:5.1f} C  Ts {:5.1f}  rh {:.2f}  ps {:6.1f}  wind {:5.1f} m/s  cover {:.2f}/{:.2f}  rain {:.2f} mm/h  conv {:.2f}  wet {:.2f}", h,
                   core.surfaceAirTemperature(cell) - 273.15, core.ts()[s] - 273.15, core.relativeHumidity(ember::atmos2::kLow, cell), core.ps()[s],
                   std::hypot(core.layer(0).u[s], core.layer(0).v[s]), core.cloudCover(0, cell), core.cloudCover(1, cell), core.diagnostics().rain[s],
                   core.diagnostics().convection[s], core.wet()[s]);
  }
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
  ember::setProgressionEnabled(false);  // the checks compare Ember's own numbers, not EmberEngine's growth
  if (command == "pack") return pack(args);
  if (command == "list") return list(args);
  if (command == "synth") return synth(args);
  if (command == "import-ember") return importSource(args);
  if (command == "sprites") return sprites(args);
  if (command == "sprite-preview") return spritePreview(args);
  if (command == "meshes") return meshes(args);
  if (command == "check-ground") return checkGroundCommand(args);
  if (command == "check-battle") return checkBattleCommand(args);
  if (command == "check-battle-export") return checkBattleExportCommand(args);
  if (command == "prepare-map") return prepareMapCommand(args);
  if (command == "atmos2-spinup") return atmos2Spinup(args);
  if (command == "atmos2-test") return atmos2Test(args);
  if (command == "atmos2-sample") return atmos2Sample(args);
  if (command == "export-battle") return exportBattleCommand(args);
  if (command == "export-scene") return exportSceneCommand(args);
  if (command == "check-campaign") return checkCampaignCommand(args);
  if (command == "check-spell-tracks") return checkSpellTracksCommand(args);
  if (command == "check-abyssal-swap") return checkAbyssalSwapCommand(args);
  if (command == "check-twin-resonance") return checkTwinResonanceCommand(args);
  if (command == "check-overworld") return checkOverworldCommand(args);
  if (command == "check-interlude") return checkInterludeCommand(args);
  if (command == "check-scene-builder") return checkSceneBuilderCommand(args);
  if (command == "check-dress") return checkDressCommand(args);
  if (command == "check-architecture") return checkArchitectureCommand(args);
  if (command == "architecture") return architecture(args);
  if (command == "ground") return ground(args);
  ember::logError("unknown command {}", command);
  return 1;
}
