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
#include "SpriteParade.h"
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
    else if (arg == "--budget" && hasValue) o.spriteBudgetMb = static_cast<uint64_t>(std::max(16, std::atoi(argv[++i])));
    else if (arg == "--size" && hasValue) parseSize(argv[++i], o);
    else if (arg == "--frames" && hasValue) o.frames = std::atoi(argv[++i]);
    else if (arg == "--io-depth" && hasValue) o.ioDepth = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    else if (arg == "--time" && hasValue) o.time = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--no-validation") o.validation = false;
    else fatal(std::format("Unknown argument {}", arg));
  }
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
    if (o.stream.empty()) return;
    if (!assets.mount(o.stream)) fatal(std::format("Cannot mount {}", o.stream));
    streamTest = std::make_unique<StreamTest>(assets, io, o.verify);
  }

  struct SpriteStage;

  // One frame of engine work around the render call. Returns the renderer's result.
  template <class Render>
  bool frame(DebugUi& ui, Renderer& renderer, SpriteStage* stage, VkExtent2D size, float dt, const Render& render);
};

// Sprite streaming on the GPU: needs the renderer, so it is created after it (and destroyed before it).
struct Services::SpriteStage {
  UploadQueue uploads;
  SpriteSystem sprites;
  SpriteParade parade;
  std::vector<SpriteInstance> instances;
  Clock::time_point start = Clock::now();

  SpriteStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, uint64_t budgetMb)
      : uploads(context), sprites(context, renderer.bindless(), assets, uploads, {budgetMb << 20}), parade(sprites) {
    sprites.start();
  }

  void frame(Renderer& renderer, VkExtent2D size) {
    const double now = std::chrono::duration<double>(Clock::now() - start).count();
    parade.request(now);
    sprites.update();
    const Camera camera = SpriteParade::camera(static_cast<float>(size.width) / static_cast<float>(size.height));
    parade.build(now, camera, instances);
    uploads.flush();
    renderer.waitFor(uploads.timeline(), sprites.uploadWaitValue());
    renderer.setScene(camera, instances);
  }
};

template <class Render>
bool Services::frame(DebugUi& ui, Renderer& renderer, SpriteStage* stage, VkExtent2D size, float dt, const Render& render) {
  const auto start = Clock::now();
  if (streamTest) streamTest->beginFrame();
  assets.update();
  if (stage) stage->frame(renderer, size);
  ui.newFrame(size, dt);
  perf.draw({&jobs, &io, &assets, &renderer});
  if (streamTest) streamTest->drawPanel();
  if (stage) stage->parade.drawPanel();
  const bool ok = render();
  const double frameMs = msSince(start);
  const double busyMs = std::max(0.0, frameMs - renderer.lastWaitMilliseconds());
  perf.addFrame(frameMs, busyMs);
  if (streamTest) streamTest->endFrame(busyMs);
  if (stage) stage->parade.endFrame(busyMs);
  return ok;
}

int runCapture(const Options& o) {
  Services services(o);
  VulkanContext context({o.validation, nullptr});
  bool passed = true;
  {
    DebugUi ui(context, nullptr, Renderer::outputFormat());
    Renderer renderer(context, nullptr, {o.width, o.height});
    renderer.setUi(&ui);
    std::unique_ptr<Services::SpriteStage> stage;
    if (!o.sprites.empty()) stage = std::make_unique<Services::SpriteStage>(context, renderer, services.assets, o.spriteBudgetMb);
    const VkExtent2D size{o.width, o.height};
    const auto testDone = [&] { return (services.streamTest && services.streamTest->done()) || (stage && stage->parade.done()); };
    if (services.streamTest || stage) {
      // Run until the test ends, plus a few frames so the overlay shows final numbers.
      // The sprite parade runs at 60 Hz like the game (streaming per second depends on the frame rate).
      int after = 0;
      auto next = Clock::now();
      while (after < kFramesInFlight + 2) {
        services.frame(ui, renderer, stage.get(), size, 1.0f / 60.0f, [&] { return renderer.renderFrame(o.time); });
        if (testDone()) ++after;
        if (stage) {
          next += std::chrono::microseconds(16667);
          const auto now = Clock::now();
          if (next > now) SDL_DelayPrecise(static_cast<Uint64>(std::chrono::nanoseconds(next - now).count()));
          else next = now;
        }
      }
      passed = services.streamTest ? services.streamTest->passed() : stage->parade.passed();
    } else {
      for (int i = 0; i < o.frames; ++i)
        services.frame(ui, renderer, nullptr, size, 1.0f / 60.0f, [&] { return renderer.renderFrame(o.time); });
    }
    ui.newFrame(size, 1.0f / 60.0f);
    services.perf.draw({&services.jobs, &services.io, &services.assets, &renderer});
    if (services.streamTest) services.streamTest->drawPanel();
    if (stage) stage->parade.drawPanel();
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
  std::unique_ptr<Services::SpriteStage> stage;
  if (!o.sprites.empty()) stage = std::make_unique<Services::SpriteStage>(context, renderer, services.assets, o.spriteBudgetMb);

  window.setEventHook([&](const SDL_Event& event) {
    if (ui.processEvent(event) || event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
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
  return options.capture.empty() ? runWindow(options) : runCapture(options);
}
