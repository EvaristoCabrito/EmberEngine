// Sandbox: opens the game window and runs the renderer.
//   Sandbox.exe                         interactive window (F1 toggles the performance overlay)
//   Sandbox.exe --capture out.png       off-screen: renders, saves a PNG, exits
//                [--size 1920x1080] [--frames 3] [--time 1.5] [--no-validation]
//   Sandbox.exe --stream test.pak [--verify <content dir>] [--io-depth N]
//        Streaming test: mounts the archive, requests every asset in it in one frame and streams them
//        while rendering, then reports load time, throughput and main-thread frame times. With
//        --verify, every loaded asset is compared byte for byte with its source file. --io-depth sets
//        how many reads the disk gets at once.
//        With --capture it saves the frame once the test ends and exits with code 3 if it failed;
//        in a window, R runs it again.
//   Sandbox.exe --battle --meshes meshes.pak --sprites battle.pak [--budget MB] [--content all|3d|cards] [--cost-split 0|1|2]
//        (--cost-split, measurement only: 1 task shaders only, 2 mesh shaders without rasterisation;
//         --unpaced: frames back to back instead of 60 Hz, so GPU times are not stretched by power saving)
//        M3 exit test: a 128 x 104-hex battle at 10x the measured Ember densities, timed in three
//        camera phases (see apps/sandbox/BattleStage.h). With --capture it exits with code 3 if it failed.
//   Sandbox.exe --compare <pakA> <meshA asset> <pakB> <meshB asset> <out dir>
//        Proxy-quality images: mesh A (reference = full detail, original = engine LOD) vs mesh B (proxy), at
//        several widths; tools/compare_meshes.py measures silhouette and normal error.
//   Sandbox.exe --map <Engine2 map.json> --ground <ground.pak> [--look smooth|hard|tabletop] [--board <board.json>] [--engine2 <public dir>]
//        D1 port: a real Ember map from Engine2, its board and ground (docs/ENGINE2_AUDIT.md).
//        [--scene <scene.json> --sprites <battle.pak> --meshes <props.pak>]: its decorations, 3D models and units.
//        Captures: [--zoom F] [--turn DEG] [--tilt DEG] [--offset X Z] (board widths/depths) [--layers MASK]
//        (lighting layers on: 1 sun, 2 shadows, 4 indirect; shaders/lighting.hlsli) [--sun AZIMUTH ELEVATION] (degrees)
//        [--moving-light] [--spell-light] (test lights; 8 in --layers = local lights)
//        [--measure SECONDS]: camera turn + zoom with everything animating; frame times; exit 3 if any frame > 16.7 ms.
//        Fog of war (R7): [--fog 0|1] (default: the map's own flag) [--scout] (the first party unit walks to the
//        farthest enemy and back). Pixel checks: [--scene-time T] (scene clock pinned) [--no-ui] [--debug-view 3]
//        (the fog mask each surface read), then tools/scripts/check_fog.py. [--volume 0]: no volumetric fog;
//        [--fog-look N]: which saved fog look (render/FogLooks.h; in a window, the Map panel's dropdown).
//        [--lighting 0|1|2]: old prototype day / Engine2's presets / physical atmosphere (default; also in the panel).
//        Night maps: [--moon-phase NAME] (Engine2's moonPhase.ts: new .. waningCrescent, blood) [--moon-path 0|1|2]
//        (where the disc and its light stand: MapStage.h moonPlacement) [--star-look N] (looks/stars).
//        [--heroes <roster.json>]: every unit's poses (tools/scripts/engine2/export_heroes.ts); the map's own units
//        play the pose chosen in the panel ("Animação"). [--battle-rules <map.battle.json>]: combat (tools/scripts/
//        engine2/export_battle.ts): pick a unit, its reach, walk it (MapStage.h). In a window
//        every setting is a dropdown / checkbox in the Map panel (no keyboard shortcuts).
//   Sandbox.exe --meshes meshes.pak [--grid N]
//        Mesh scene (M3 bring-up): every cooked mesh in the archive on an N x N grid, drawn GPU-driven.
//   Sandbox.exe --sprites roster.pak [--budget MB]
//        Sprite streaming test (M2): plays every clip of every character, a few at a time, under a GPU
//        budget (default 512 MB) smaller than the roster. With --capture it runs at 60 Hz, saves the
//        frame once every clip has played and exits with code 3 if it failed.
// A capture exits with code 2 if the Vulkan validation layer reported any error.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "assets/AssetManager.h"
#include "core/Jobs.h"
#include "core/Log.h"
#include "debug/PerfOverlay.h"
#include "platform/IoQueue.h"
#include "platform/Window.h"
#include "render/DebugUi.h"
#include "render/Renderer.h"
#include "render/SpriteSystem.h"
#include "rhi/Swapchain.h"
#include "rhi/UploadQueue.h"
#include "BattleStage.h"
#include "MeshCompare.h"
#include "MapStage.h"
#include "MeshStage.h"
#include "SpriteParade.h"
#include "Stage.h"
#include "rhi/VulkanContext.h"

namespace {

using namespace ember;
using Clock = std::chrono::steady_clock;

constexpr double kFrameBudgetMs = 1000.0 / 60.0;

struct Options {
  std::string capture;
  std::string stream;
  std::string verify;
  std::string sprites;
  uint64_t spriteBudgetMb = 512;
  std::string meshes;
  uint32_t meshGrid = 8;
  bool battle = false;
  std::string battleContent = "all";
  uint32_t costSplit = 0;
  bool unpaced = false;
  float mapZoom = 1.0f, mapTurn = 0.0f, mapTilt = 0.0f, mapOffsetX = 0.0f, mapOffsetZ = 0.0f;
  uint32_t lightingLayers = 0xFFFFFFFFu;
  float sunAzimuth = 0.0f, sunElevation = -1.0f;
  bool movingLight = false, spellLight = false;
  float measureSeconds = 0.0f;
  int fog = -1;
  bool scout = false, noUi = false, volume = true;
  int fogLook = 3;
  int lighting = 2;
  float sceneTime = -1.0f;
  std::string moonPhase;
  int moonPath = 0;
  int starLook = 3;
  std::string heroes;
  std::string battleRules;
  uint32_t debugView = 0;
  std::string scene;
  std::string map, ground, look = "smooth", boardData = "content/engine2/board.json", engine2 = "C:/Engine2/public";
  std::vector<std::string> compare;  // pakA assetA pakB assetB outDir  // measurement: frames back to back, so the GPU never idles and keeps its full clock
  bool budgetSet = false;
  uint32_t width = 1600, height = 900;
  int frames = 3;
  uint32_t ioDepth = 64;
  float time = 1.0f;
  bool validation = true;
};

// "1920x1080"
void parseSize(std::string_view text, Options& o) {
  const size_t x = text.find('x');
  const bool ok = x != std::string_view::npos &&
                  std::from_chars(text.data(), text.data() + x, o.width).ec == std::errc{} &&
                  std::from_chars(text.data() + x + 1, text.data() + text.size(), o.height).ec == std::errc{};
  if (!ok || !o.width || !o.height) fatal(std::format("Bad --size {}", text));
}

Options parse(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const bool hasValue = i + 1 < argc;
    if (arg == "--capture" && hasValue) o.capture = argv[++i];
    else if (arg == "--stream" && hasValue) o.stream = argv[++i];
    else if (arg == "--verify" && hasValue) o.verify = argv[++i];
    else if (arg == "--sprites" && hasValue) o.sprites = argv[++i];
    else if (arg == "--meshes" && hasValue) o.meshes = argv[++i];
    else if (arg == "--battle") o.battle = true;
    else if (arg == "--map" && hasValue) o.map = argv[++i];
    else if (arg == "--ground" && hasValue) o.ground = argv[++i];
    else if (arg == "--scene" && hasValue) o.scene = argv[++i];
    else if (arg == "--look" && hasValue) o.look = argv[++i];
    else if (arg == "--zoom" && hasValue) o.mapZoom = std::max(0.1f, static_cast<float>(std::atof(argv[++i])));
    else if (arg == "--turn" && hasValue) o.mapTurn = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--tilt" && hasValue) o.mapTilt = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--offset" && i + 2 < argc) o.mapOffsetX = static_cast<float>(std::atof(argv[++i])), o.mapOffsetZ = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--sun" && i + 2 < argc) o.sunAzimuth = static_cast<float>(std::atof(argv[++i])), o.sunElevation = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--moving-light") o.movingLight = true;
    else if (arg == "--measure" && hasValue) o.measureSeconds = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--spell-light") o.spellLight = true;
    else if (arg == "--fog" && hasValue) o.fog = std::atoi(argv[++i]);
    else if (arg == "--scout") o.scout = true;
    else if (arg == "--volume" && hasValue) o.volume = std::atoi(argv[++i]) != 0;
    else if (arg == "--fog-look" && hasValue) o.fogLook = std::atoi(argv[++i]);
    else if (arg == "--lighting" && hasValue) o.lighting = std::atoi(argv[++i]);
    else if (arg == "--scene-time" && hasValue) o.sceneTime = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--moon-phase" && hasValue) o.moonPhase = argv[++i];
    else if (arg == "--moon-path" && hasValue) o.moonPath = std::atoi(argv[++i]);
    else if (arg == "--star-look" && hasValue) o.starLook = std::atoi(argv[++i]);
    else if (arg == "--heroes" && hasValue) o.heroes = argv[++i];
    else if (arg == "--battle-rules" && hasValue) o.battleRules = argv[++i];
    else if (arg == "--debug-view" && hasValue) o.debugView = static_cast<uint32_t>(std::atoi(argv[++i]));
    else if (arg == "--no-ui") o.noUi = true;
    else if (arg == "--layers" && hasValue) o.lightingLayers = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 0));
    else if (arg == "--board" && hasValue) o.boardData = argv[++i];
    else if (arg == "--engine2" && hasValue) o.engine2 = argv[++i];
    else if (arg == "--content" && hasValue) o.battleContent = argv[++i];
    else if (arg == "--unpaced") o.unpaced = true;
    else if (arg == "--compare" && i + 5 < argc) o.compare.assign(argv + i + 1, argv + i + 6), i += 5;
    else if (arg == "--cost-split" && hasValue) o.costSplit = static_cast<uint32_t>(std::atoi(argv[++i]));
    else if (arg == "--grid" && hasValue) o.meshGrid = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    else if (arg == "--budget" && hasValue) o.spriteBudgetMb = static_cast<uint64_t>(std::max(16, std::atoi(argv[++i]))), o.budgetSet = true;
    else if (arg == "--size" && hasValue) parseSize(argv[++i], o);
    else if (arg == "--frames" && hasValue) o.frames = std::atoi(argv[++i]);
    else if (arg == "--io-depth" && hasValue) o.ioDepth = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    else if (arg == "--time" && hasValue) o.time = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--no-validation") o.validation = false;
    else fatal(std::format("Unknown argument {}", arg));
  }
  if (o.battle && (o.meshes.empty() || o.sprites.empty())) fatal("--battle needs --meshes <pak> and --sprites <pak>");
  if (!o.map.empty() && o.ground.empty()) fatal("--map needs --ground <ground.pak> (Cooker ground)");
  if (o.look != "smooth" && o.look != "hard" && o.look != "tabletop") fatal("--look is smooth, hard or tabletop");
  // The battle draws every decor card and 72 units at once: a larger sprite budget unless given.
  if (o.battle && !o.budgetSet) o.spriteBudgetMb = 1536;
  return o;
}

double msSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// --- Streaming test ------------------------------------------------------------------------------

class StreamTest {
 public:
  StreamTest(AssetManager& assets, const IoQueue& io, std::string verifyRoot)
      : assets_(assets), io_(io), verifyRoot_(std::move(verifyRoot)), ids_(assets.mountedAssets()) {}

  bool done() const { return phase_ == Phase::Done; }
  bool passed() const { return done() && failed_ == 0 && mismatches_ == 0 && worstBusy_ <= kFrameBudgetMs; }

  void restart() {
    for (AssetHandle handle : handles_) assets_.release(handle);
    handles_.clear();
    phase_ = Phase::Warmup;
    frames_ = 0;
    baselineBusy_.clear();
    streamingBusy_.clear();
  }

  // Before the asset manager's update. The request burst happens here, inside the measured frame.
  void beginFrame() {
    if (phase_ == Phase::Warmup && frames_ >= kWarmupFrames) {
      bytesAtStart_ = io_.stats().bytesRead;
      start_ = Clock::now();
      handles_.reserve(ids_.size());
      for (AssetId id : ids_) handles_.push_back(assets_.load(id));
      requestMs_ = msSince(start_);
      phase_ = Phase::Streaming;
    }
  }

  // After the frame, with the main thread's working time for it.
  void endFrame(double busyMs) {
    ++frames_;
    if (phase_ == Phase::Warmup) baselineBusy_.push_back(busyMs);
    if (phase_ != Phase::Streaming) return;
    streamingBusy_.push_back(busyMs);
    const AssetManager::Stats stats = assets_.stats();
    if (stats.queued + stats.loading > 0) return;
    loadSeconds_ = std::chrono::duration<double>(Clock::now() - start_).count();
    bytesRead_ = io_.stats().bytesRead - bytesAtStart_;
    finish(stats);
  }

  void drawPanel() const {
    ImGui::SetNextWindowPos({12, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::Begin("Streaming test  [R]", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::Text("%zu assets in archive", ids_.size());
    if (phase_ == Phase::Warmup) {
      ImGui::Text("warming up (%d/%d frames)", frames_, kWarmupFrames);
    } else if (phase_ == Phase::Streaming) {
      const AssetManager::Stats stats = assets_.stats();
      ImGui::ProgressBar(ids_.empty() ? 1.0f : static_cast<float>(stats.ready + stats.failed) / static_cast<float>(ids_.size()),
                         {320, 0});
    } else {
      ImGui::Text("%s", passed() ? "PASS" : "FAIL");
      for (const std::string& line : report_) ImGui::TextUnformatted(line.c_str());
    }
    ImGui::End();
  }

 private:
  enum class Phase { Warmup, Streaming, Done };
  static constexpr int kWarmupFrames = 60;

  static double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, static_cast<size_t>(p * static_cast<double>(values.size())))];
  }

  void finish(const AssetManager::Stats& stats) {
    phase_ = Phase::Done;
    failed_ = stats.failed;
    mismatches_ = verifyRoot_.empty() ? 0 : verify();
    worstBusy_ = streamingBusy_.empty() ? 0.0 : *std::max_element(streamingBusy_.begin(), streamingBusy_.end());

    uint64_t decompressed = 0;
    for (AssetHandle handle : handles_) decompressed += assets_.data(handle).size();
    const double mb = 1024.0 * 1024.0;
    report_.clear();
    report_.push_back(std::format("{} assets loaded, {} failed{}", stats.ready, stats.failed,
                                  verifyRoot_.empty() ? std::string(", not verified")
                                                      : std::format(", {} mismatched source files", mismatches_)));
    report_.push_back(std::format("all requested in one frame: {:.2f} ms for the {} load() calls", requestMs_, ids_.size()));
    report_.push_back(std::format("streamed in {:.3f} s over {} frames", loadSeconds_, streamingBusy_.size()));
    report_.push_back(std::format("disk {:.1f} MB at {:.0f} MB/s; {:.1f} MB after decompression ({:.0f} MB/s)",
                                  static_cast<double>(bytesRead_) / mb, static_cast<double>(bytesRead_) / mb / loadSeconds_,
                                  static_cast<double>(decompressed) / mb, static_cast<double>(decompressed) / mb / loadSeconds_));
    report_.push_back(std::format("main thread before streaming: median {:.3f} ms, worst {:.3f} ms",
                                  percentile(baselineBusy_, 0.5), percentile(baselineBusy_, 1.0)));
    report_.push_back(std::format("main thread while streaming:  median {:.3f} ms, p99 {:.3f} ms, worst {:.3f} ms",
                                  percentile(streamingBusy_, 0.5), percentile(streamingBusy_, 0.99), worstBusy_));
    report_.push_back(std::format("frame budget {:.2f} ms (60 fps): {}", kFrameBudgetMs,
                                  worstBusy_ <= kFrameBudgetMs ? "never exceeded" : "EXCEEDED"));
    logInfo("stream test: {}", passed() ? "PASS" : "FAIL");
    for (const std::string& line : report_) logInfo("  {}", line);
  }

  // Byte-for-byte comparison of every loaded asset with the file it was cooked from.
  uint32_t verify() const {
    uint32_t mismatches = 0;
    std::vector<char> source;
    for (size_t i = 0; i < ids_.size(); ++i) {
      const std::span<const std::byte> loaded = assets_.data(handles_[i]);
      std::ifstream file(std::filesystem::path(verifyRoot_) / std::string(assets_.name(ids_[i])), std::ios::binary | std::ios::ate);
      bool same = static_cast<bool>(file);
      if (same) {
        source.resize(static_cast<size_t>(file.tellg()));
        file.seekg(0);
        file.read(source.data(), static_cast<std::streamsize>(source.size()));
        same = source.size() == loaded.size() && std::memcmp(source.data(), loaded.data(), source.size()) == 0;
      }
      if (!same) {
        if (mismatches < 10) logError("mismatch: {}", assets_.name(ids_[i]));
        ++mismatches;
      }
    }
    return mismatches;
  }

  AssetManager& assets_;
  const IoQueue& io_;
  std::string verifyRoot_;
  std::vector<AssetId> ids_;
  std::vector<AssetHandle> handles_;
  Phase phase_ = Phase::Warmup;
  int frames_ = 0;
  Clock::time_point start_{};
  uint64_t bytesAtStart_ = 0, bytesRead_ = 0;
  double requestMs_ = 0.0, loadSeconds_ = 0.0, worstBusy_ = 0.0;
  uint32_t failed_ = 0, mismatches_ = 0;
  std::vector<double> baselineBusy_, streamingBusy_;
  std::vector<std::string> report_;
};

// --- Engine services shared by both modes --------------------------------------------------------

struct Services {
  JobSystem jobs;
  IoQueue io;
  AssetManager assets;
  std::unique_ptr<StreamTest> streamTest;
  PerfOverlay perf;

  explicit Services(const Options& o) : io(jobs, o.ioDepth), assets(jobs, io, {}) {
    if (!o.sprites.empty() && !assets.mount(o.sprites)) fatal(std::format("Cannot mount {}", o.sprites));
    if (!o.meshes.empty() && !assets.mount(o.meshes)) fatal(std::format("Cannot mount {}", o.meshes));
    if (!o.ground.empty() && !assets.mount(o.ground)) fatal(std::format("Cannot mount {}", o.ground));
    for (size_t k = 0; k < o.compare.size() && k < 4; k += 2)
      if (!assets.mount(o.compare[k])) fatal(std::format("Cannot mount {}", o.compare[k]));
    if (o.stream.empty()) return;
    if (!assets.mount(o.stream)) fatal(std::format("Cannot mount {}", o.stream));
    streamTest = std::make_unique<StreamTest>(assets, io, o.verify);
  }

  // One frame of engine work around the render call. Returns the renderer's result.
  template <class Render>
  bool frame(DebugUi& ui, Renderer& renderer, Stage* stage, VkExtent2D size, float dt, const Render& render);
};

// Sprite streaming on the GPU: needs the renderer, so it is created after it (and destroyed before it).
struct SpriteStage final : Stage {
  UploadQueue uploads;
  SpriteSystem sprites;
  SpriteParade parade;
  std::vector<SpriteInstance> instances;
  Clock::time_point start = Clock::now();

  SpriteStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, uint64_t budgetMb)
      : uploads(context), sprites(context, renderer.bindless(), assets, uploads, {budgetMb << 20}), parade(sprites) {
    sprites.start();
  }

  void frame(Renderer& renderer, VkExtent2D size) override {
    const double now = std::chrono::duration<double>(Clock::now() - start).count();
    parade.request(now);
    sprites.update();
    const Camera camera = SpriteParade::camera(static_cast<float>(size.width) / static_cast<float>(size.height));
    parade.build(now, camera, instances);
    uploads.flush();
    renderer.waitFor(uploads.timeline(), sprites.uploadWaitValue());
    renderer.setScene(camera, instances);
  }
  void drawPanel() override { parade.drawPanel(); }
  void endFrame(double busyMs) override { parade.endFrame(busyMs); }
  bool done() const override { return parade.done(); }
  bool passed() const override { return parade.passed(); }
};

std::unique_ptr<Stage> makeStage(const Options& o, VulkanContext& context, Renderer& renderer, AssetManager& assets, JobSystem& jobs) {
  if (o.battle) {
    const BattleStage::Content content = o.battleContent == "3d"      ? BattleStage::Content::Meshes
                                         : o.battleContent == "cards" ? BattleStage::Content::Cards
                                                                      : BattleStage::Content::All;
    return std::make_unique<BattleStage>(context, renderer, assets, o.spriteBudgetMb, content, o.costSplit);
  }
  if (!o.map.empty()) {
    MapStageOptions m;
    m.map = o.map;
    m.scene = o.scene;
    m.spriteBudgetMb = o.spriteBudgetMb;
    m.boardData = o.boardData;
    m.engine2Public = o.engine2;
    m.look = o.look == "hard" ? e2::EdgeStyle::Hard : o.look == "tabletop" ? e2::EdgeStyle::Tabletop : e2::EdgeStyle::Smooth;
    m.zoom = o.mapZoom;
    m.turnDegrees = o.mapTurn;
    m.tiltDegrees = o.mapTilt;
    m.offset = {o.mapOffsetX, o.mapOffsetZ};
    m.layers = o.lightingLayers;
    m.sunAzimuth = o.sunAzimuth;
    m.sunElevation = o.sunElevation;
    m.movingLight = o.movingLight;
    m.spellLight = o.spellLight;
    m.measureSeconds = o.measureSeconds;
    m.fog = o.fog;
    m.scout = o.scout;
    m.volume = o.volume;
    m.fogLook = o.fogLook;
    m.lighting = o.lighting;
    m.sceneTime = o.sceneTime;
    m.moonPhase = o.moonPhase;
    m.moonPath = o.moonPath;
    m.starLook = o.starLook;
    m.heroes = o.heroes;
    m.battle = o.battleRules;
    renderer.setDebugView(o.debugView);
    return std::make_unique<MapStage>(context, renderer, assets, jobs, m);
  }
  if (!o.sprites.empty() && o.map.empty()) return std::make_unique<SpriteStage>(context, renderer, assets, o.spriteBudgetMb);
  if (!o.meshes.empty()) return std::make_unique<MeshStage>(context, renderer, assets, o.meshGrid, o.capture.empty());
  return nullptr;
}

template <class Render>
bool Services::frame(DebugUi& ui, Renderer& renderer, Stage* stage, VkExtent2D size, float dt, const Render& render) {
  const auto start = Clock::now();
  if (streamTest) streamTest->beginFrame();
  assets.update();
  if (stage) stage->frame(renderer, size);
  ui.newFrame(size, dt);
  perf.draw({&jobs, &io, &assets, &renderer});
  if (streamTest) streamTest->drawPanel();
  if (stage) stage->drawPanel();
  const bool ok = render();
  const double frameMs = msSince(start);
  const double busyMs = std::max(0.0, frameMs - renderer.lastWaitMilliseconds());
  perf.addFrame(frameMs, busyMs);
  if (streamTest) streamTest->endFrame(busyMs);
  if (stage) stage->endFrame(busyMs);
  return ok;
}

int runCompare(const Options& o) {
  Services services(o);
  VulkanContext context({o.validation, nullptr});
  bool ok = false;
  {
    Renderer renderer(context, nullptr, {o.width, o.height});
    ok = runMeshCompare(context, renderer, services.assets, assetId(o.compare[1]), assetId(o.compare[3]),
                        std::filesystem::path(o.compare[4]), {o.width, o.height});
  }
  if (context.validationErrors()) {
    logError("{} validation error(s)", context.validationErrors());
    return 2;
  }
  return ok ? 0 : 1;
}

int runCapture(const Options& o) {
  Services services(o);
  VulkanContext context({o.validation, nullptr});
  bool passed = true;
  {
    DebugUi ui(context, nullptr, Renderer::outputFormat());
    Renderer renderer(context, nullptr, {o.width, o.height});
    if (!o.noUi) renderer.setUi(&ui);  // --no-ui: the scene alone (pixel checks)
    std::unique_ptr<Stage> stage = makeStage(o, context, renderer, services.assets, services.jobs);
    const VkExtent2D size{o.width, o.height};
    const auto testDone = [&] { return (services.streamTest && services.streamTest->done()) || (stage && stage->done()); };
    if (services.streamTest || stage) {
      // Run until the test ends, plus a few frames so the overlay shows final numbers.
      // The sprite parade runs at 60 Hz like the game (streaming per second depends on the frame rate).
      int after = 0;
      auto next = Clock::now();
      while (after < kFramesInFlight + 2) {
        services.frame(ui, renderer, stage.get(), size, 1.0f / 60.0f, [&] { return renderer.renderFrame(o.time); });
        if (testDone()) ++after;
        if (stage && !o.unpaced) {
          next += std::chrono::microseconds(16667);
          const auto now = Clock::now();
          if (next > now) SDL_DelayPrecise(static_cast<Uint64>(std::chrono::nanoseconds(next - now).count()));
          else next = now;
        }
      }
      passed = services.streamTest ? services.streamTest->passed() : stage->passed();
    } else {
      for (int i = 0; i < o.frames; ++i)
        services.frame(ui, renderer, nullptr, size, 1.0f / 60.0f, [&] { return renderer.renderFrame(o.time); });
    }
    ui.newFrame(size, 1.0f / 60.0f);
    services.perf.draw({&services.jobs, &services.io, &services.assets, &renderer});
    if (services.streamTest) services.streamTest->drawPanel();
    if (stage) stage->drawPanel();
    renderer.captureFrame(o.time, o.capture);
    stage.reset();
  }
  if (context.validationErrors()) {
    logError("{} validation error(s)", context.validationErrors());
    return 2;
  }
  return passed ? 0 : 3;
}

int runWindow(const Options& o) {
  Services services(o);
  Window window({"Ember", o.width, o.height});
  VulkanContext context({o.validation, window.handle()});
  uint32_t width = 0, height = 0;
  window.pixelSize(width, height);
  auto swapchain = std::make_unique<Swapchain>(context, width, height, true);
  DebugUi ui(context, window.handle(), Renderer::outputFormat());
  Renderer renderer(context, swapchain.get(), swapchain->extent());
  renderer.setUi(&ui);
  std::unique_ptr<Stage> stage = makeStage(o, context, renderer, services.assets, services.jobs);
  if (!o.map.empty()) services.perf.visible = false;  // a map window shows only its choices (MapStage::drawPanel)

  window.setEventHook([&](const SDL_Event& event) {
    if (ui.processEvent(event)) return;
    if (stage) stage->event(event);
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
    if (event.key.key == SDLK_F1) services.perf.visible = !services.perf.visible;
    if (event.key.key == SDLK_R && services.streamTest) services.streamTest->restart();
  });

  const auto start = Clock::now();
  auto last = start;
  while (window.pumpEvents()) {
    window.pixelSize(width, height);
    if (width == 0 || height == 0) continue;  // minimised
    const auto now = Clock::now();
    const float dt = std::chrono::duration<float>(now - last).count();
    last = now;
    const float time = std::chrono::duration<float>(now - start).count();
    const bool resized = window.consumeResized();
    const bool presented = !resized && services.frame(ui, renderer, stage.get(), swapchain->extent(), dt,
                                                      [&] { return renderer.renderFrame(time); });
    if (!presented && swapchain->recreate(width, height)) renderer.resize(swapchain->extent());
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const Options options = parse(argc, argv);
  if (!options.compare.empty()) return runCompare(options);
  return options.capture.empty() ? runWindow(options) : runCapture(options);
}
