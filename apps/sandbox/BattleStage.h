#pragma once

// M3 exit test (docs/ARCHITECTURE.md §9): a large battle with real assets at 10x the densest measured
// Ember map, timed on this PC.
//   field     128 x 104 hexes, hex radius 1.18 m (a 1.8 m human is 1.53 hex radii tall, as in Ember)
//   3D        2.0 building pieces per hex: Ember's 3D pieces are wall and house segments (a hex or so
//             wide, about a storey tall), so each piece is one of the cooked cottage models (the stand-in
//             until the Meshy buildings exist) scaled to a 3 m segment, random yaw. Trees, rocks and props
//             are 2D cards in the game (the 2D/3D split), so they are cards here too.
//   2D cards  2.1 decor cards per hex from the 443 decoration images, Ember's single-hex card size
//   units     72 animated character cards (8 + 64), each cycling through character clips
// Phases: load -> zoomed out (whole field in view) -> default tactics zoom -> free-orbit sweep.
// Pass: every measured frame's main-thread time and GPU time <= 16.7 ms.
// Content can be limited (3D only / cards only) to isolate where GPU time goes; the exit test is "all".
// The report gives each GPU pass's average and worst time per phase.

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <format>
#include <random>
#include <string>
#include <vector>

#include "Stage.h"
#include "assets/AssetManager.h"
#include "core/Log.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "render/SpriteDraw.h"
#include "render/SpriteSystem.h"
#include "rhi/UploadQueue.h"

namespace ember {

class BattleStage final : public Stage {
 public:
  static constexpr int kCols = 128, kRows = 104;
  static constexpr float kHexRadius = 1.18f;
  static constexpr float kMeshesPerHex = 2.0f, kCardsPerHex = 2.1f;
  static constexpr int kUnits = 72;
  static constexpr float kSegment = 3.0f;  // metres: the widest side of a building piece
  static constexpr double kBudgetMs = 1000.0 / 60.0;

  enum class Content { All, Meshes, Cards };

  BattleStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, uint64_t spriteBudgetMb, Content content,
              uint32_t costSplit = 0)
      : renderer_(renderer),
        content_(content),
        uploads_(context),
        meshes_(context, renderer.bindless(), assets, uploads_, {.geometryBytes = 256ull << 20}),
        sprites_(context, renderer.bindless(), assets, uploads_, {spriteBudgetMb << 20}) {
    for (const AssetId id : assets.mountedAssets())
      if (assets.name(id).ends_with(".mesh")) meshIds_.push_back({meshes_.add(id), std::string(assets.name(id))});
    sprites_.start();
    renderer.setGroundExtent(std::max(width(), depth()) * 0.6f);
    renderer.setCostSplit(costSplit);
    logInfo("battle: {} meshes, sprite budget {} MB", meshIds_.size(), spriteBudgetMb);
  }

  void frame(Renderer& renderer, VkExtent2D size) override {
    const auto now = std::chrono::steady_clock::now();
    if (phase_ == Phase::Loading && !placed_ && sprites_.ready() &&
        std::all_of(meshIds_.begin(), meshIds_.end(), [&](const auto& m) { return meshes_.header(m.id) != nullptr; }))
      place();
    // Like a battle's loading screen: the map's decor streams in before the battle starts.
    if (phase_ == Phase::Loading && placed_) {
      bool decorResident = true;
      for (const SpriteClipId clip : decorClips_) {
        sprites_.request(clip, SpriteSystem::kSoon);
        decorResident = decorResident && sprites_.texture(clip).full;
      }
      if (decorResident && meshes_.ready() && (meshCount_ == 0 || meshes_.view().instanceCount == meshCount_))
        startPhase(Phase::ZoomedOut, now);
    }

    const double t = std::chrono::duration<double>(now - phaseStart_).count();
    if (phase_ != Phase::Loading && phase_ != Phase::Done && t >= phaseSeconds(phase_))
      startPhase(static_cast<Phase>(static_cast<int>(phase_) + 1), now);

    const float aspect = static_cast<float>(size.width) / static_cast<float>(size.height);
    camera_ = cameraFor(phase_, static_cast<float>(std::chrono::duration<double>(now - phaseStart_).count()), aspect);
    const double clock = std::chrono::duration<double>(now - start_).count();
    if (placed_) buildSprites(clock);
    sprites_.update();
    meshes_.update();
    uploads_.flush();
    renderer.setScene(camera_, drawList_);
    renderer.setMeshes(meshes_.view());
    renderer.waitFor(uploads_.timeline(), std::max(meshes_.uploadWaitValue(), sprites_.uploadWaitValue()));
  }

  void endFrame(double busyMs) override {
    if (phase_ == Phase::Loading || phase_ == Phase::Done) return;
    // The GPU result is a few frames old (GpuTimer); skip the first frames of a phase.
    if (++phaseFrames_ <= 4) return;
    double gpu = 0.0;
    for (const auto& r : renderer_.gpuTimer().results())
      if (std::string_view(r.name) == "frame") gpu = r.milliseconds;
    Measure& m = measures_[static_cast<int>(phase_) - 1];
    m.frames++;
    m.worstBusy = std::max(m.worstBusy, busyMs);
    m.worstGpu = std::max(m.worstGpu, gpu);
    m.sumGpu += gpu;
    m.gpuFrames.push_back(gpu);
    {
      const Renderer::MeshStats s = renderer_.meshStats();
      std::string row = std::format("{:.3f},{},{},{},{},{}", gpu, s.clusters, s.earlyClusters, s.retestedClusters, s.triangles,
                                    s.visibleInstances);
      for (const auto& r : renderer_.gpuTimer().results()) row += std::format(",{}={:.2f}", r.name, r.milliseconds);
      m.rows.push_back(row);
    }
    for (const auto& r : renderer_.gpuTimer().results()) {
      auto it = std::find_if(m.passes.begin(), m.passes.end(), [&](const PassTime& t) { return t.name == r.name; });
      if (it == m.passes.end()) it = m.passes.insert(m.passes.end(), {r.name, 0.0, 0.0, 0});
      it->sum += r.milliseconds;
      it->worst = std::max(it->worst, r.milliseconds);
      ++it->frames;
    }
    m.sumBusy += busyMs;
    const Renderer::MeshStats g = renderer_.meshStats();
    m.maxClusters = std::max(m.maxClusters, g.clusters);
    m.maxTriangles = std::max(m.maxTriangles, g.triangles);
    m.maxCards = std::max<uint32_t>(m.maxCards, static_cast<uint32_t>(drawList_.size()));
    m.overflow += g.queueOverflow + g.clusterOverflow;
  }

  void drawPanel() override {
    ImGui::SetNextWindowPos({12, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::Begin("Battle (M3)", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    const MeshScene::Stats ms = meshes_.stats();
    const SpriteSystem::Stats ss = sprites_.stats();
    const Renderer::MeshStats g = renderer_.meshStats();
    ImGui::Text("phase: %s", phaseName(phase_));
    ImGui::Text("3D: %u instances, %u in view, %u clusters drawn", ms.instances, g.visibleInstances, g.clusters);
    ImGui::Text("cards drawn: %zu (of %zu decor + %d units)", drawList_.size(), cards_.size(), kUnits);
    ImGui::Text("sprite GPU: %.0f / %.0f MB, %u resident, %u loading", ss.residentBytes / 1048576.0, ss.budgetBytes / 1048576.0,
                ss.resident, ss.loading + ss.uploading);
    for (const std::string& line : report_) ImGui::TextUnformatted(line.c_str());
    ImGui::End();
  }

  bool done() const override { return phase_ == Phase::Done; }
  bool passed() const override { return passed_; }

 private:
  enum class Phase { Loading, ZoomedOut, Default, Orbit, Done };
  struct PassTime {
    std::string name;
    double sum, worst;
    uint64_t frames;
  };
  struct Measure {
    std::vector<PassTime> passes;
    std::vector<double> gpuFrames;
    std::vector<std::string> rows;  // per frame: gpu ms, clusters, early clusters, retested, triangles, instances drawn
    uint64_t frames = 0;
    double worstBusy = 0.0, worstGpu = 0.0, sumBusy = 0.0, sumGpu = 0.0;
    uint32_t maxClusters = 0, maxCards = 0, maxTriangles = 0;
    uint64_t overflow = 0;
  };
  struct Card {
    glm::vec3 feet;
    SpriteClipId clip;
    float width;  // metres, the clip's full source frame
  };
  struct Unit {
    glm::vec3 feet;
    SpriteClipId current, next;
    double start, end;
  };

  static const char* phaseName(Phase p) {
    switch (p) {
      case Phase::Loading: return "loading";
      case Phase::ZoomedOut: return "zoomed out (whole field)";
      case Phase::Default: return "default tactics zoom";
      case Phase::Orbit: return "free-orbit sweep";
      case Phase::Done: return "done";
    }
    return "";
  }
  static double phaseSeconds(Phase p) { return p == Phase::Orbit ? 12.0 : 8.0; }

  static float width() { return kHexRadius * std::sqrt(3.0f) * (kCols + 0.5f); }
  static float depth() { return kHexRadius * 1.5f * (kRows - 1) + 2.0f * kHexRadius; }
  // Hex centre, odd rows shifted half a hex (Ember's layout); the field is centred on the origin.
  static glm::vec3 hexCenter(int col, int row) {
    const float x = kHexRadius * std::sqrt(3.0f) * (col + 0.5f * (row & 1));
    const float z = kHexRadius * 1.5f * row;
    return {x - width() * 0.5f, 0.0f, z - depth() * 0.5f};
  }

  Camera cameraFor(Phase p, float t, float aspect) const {
    const float fov = glm::radians(40.0f);
    auto orbitCamera = [&](float yawDeg, float pitchDeg, float distance, glm::vec3 target) {
      const float yaw = glm::radians(yawDeg), pitch = glm::radians(pitchDeg);
      const glm::vec3 eye = target + distance * glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
      return Camera::lookAt(eye, target, fov, aspect, 0.25f);
    };
    switch (p) {
      case Phase::ZoomedOut: return orbitCamera(30.0f, 50.0f, width() * 1.1f, {0, 0, 0});
      case Phase::Default: return orbitCamera(30.0f, 25.0f, 30.0f, {0, 0, 0});  // a tactics view ~25 hexes across
      case Phase::Orbit: return orbitCamera(30.0f + t * 30.0f, 30.0f, 70.0f, {0, 0, 0});
      default: return orbitCamera(30.0f, 50.0f, width() * 1.1f, {0, 0, 0});
    }
  }

  void place() {
    placed_ = true;
    std::mt19937 rng(0xBA771Eu);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::poisson_distribution<int> meshCount(kMeshesPerHex), cardCount(kCardsPerHex);

    std::vector<MeshId> houses;
    for (const auto& m : meshIds_)
      if (m.name.find("cottage") != std::string::npos) houses.push_back(m.id);
    std::vector<SpriteClipId> decor, characters;
    for (SpriteClipId c = 0; c < sprites_.clipCount(); ++c) {
      const SpriteClipHeader& h = sprites_.header(c);
      if (h.frames == 0) continue;
      (std::string_view(h.pack) == "decor" ? decor : characters).push_back(c);
    }

    const auto jitter = [&](glm::vec3 c) {
      return c + glm::vec3((unit(rng) - 0.5f) * kHexRadius * 1.4f, 0.0f, (unit(rng) - 0.5f) * kHexRadius * 1.4f);
    };
    std::vector<InstanceGpu> instances;
    for (int row = 0; row < kRows; ++row)
      for (int col = 0; col < kCols; ++col) {
        const glm::vec3 c = hexCenter(col, row);
        const int pieces = meshCount(rng);  // drawn always, so the layout is the same whatever the content
        for (int i = houses.empty() || content_ == Content::Cards ? 0 : pieces; i > 0; --i) {
          const MeshId m = houses[static_cast<size_t>(unit(rng) * houses.size()) % houses.size()];
          const MeshHeader& h = *meshes_.header(m);
          // A building segment: the model's widest side scaled to kSegment metres.
          const float extent = std::max(h.boundsMax[0] - h.boundsMin[0], h.boundsMax[2] - h.boundsMin[2]);
          const float s = kSegment / std::max(extent, 0.01f);
          const glm::vec3 p = jitter(c);
          const float yaw = unit(rng) * 6.2831853f, cy = std::cos(yaw) * s, sy = std::sin(yaw) * s;
          InstanceGpu inst{};
          inst.rows[0] = {cy, 0, sy, p.x};
          inst.rows[1] = {0, s, 0, -h.boundsMin[1] * s};
          inst.rows[2] = {-sy, 0, cy, p.z};
          inst.mesh = m;
          inst.scale = s;
          instances.push_back(inst);
        }
        const int cards = cardCount(rng);
        for (int i = content_ == Content::Meshes ? 0 : cards; i > 0 && !decor.empty(); --i) {
          const SpriteClipId clip = decor[static_cast<size_t>(unit(rng) * decor.size()) % decor.size()];
          cards_.push_back({jitter(c), clip, kHexRadius * 1.55f * 1.2f});  // Ember's single-hex card width
          if (std::find(decorClips_.begin(), decorClips_.end(), clip) == decorClips_.end()) decorClips_.push_back(clip);
        }
      }
    meshCount_ = static_cast<uint32_t>(instances.size());
    meshes_.setInstances(instances);
    // Two armies facing each other across the middle.
    for (int i = 0; i < kUnits && !characters.empty() && content_ != Content::Meshes; ++i) {
      const int col = kCols / 2 - 6 + (i % 12), row = kRows / 2 + (i < 36 ? -3 : 3) + (i % 36) / 12 * (i < 36 ? -1 : 1);
      Unit u{hexCenter(col, row), characters[static_cast<size_t>(unit(rng) * characters.size()) % characters.size()],
             characters[static_cast<size_t>(unit(rng) * characters.size()) % characters.size()], 0.0, 0.0};
      u.end = 2.0 + unit(rng) * 3.0;
      units_.push_back(u);
    }
    unitClips_ = std::move(characters);
    logInfo("battle: placed {} 3D building pieces ({} models), {} decor cards, {} units", meshCount_, houses.size(), cards_.size(),
            units_.size());
  }

  void buildSprites(double clock) {
    // CPU frustum test for cards (planes of the current camera), then back-to-front.
    const glm::mat4 vp = camera_.viewProjection();
    const auto row = [&](int i) { return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]); };
    glm::vec4 planes[5] = {row(3) + row(0), row(3) - row(0), row(3) + row(1), row(3) - row(1), row(3) - row(2)};
    for (glm::vec4& p : planes) p /= glm::length(glm::vec3(p));
    const auto inView = [&](glm::vec3 c, float r) {
      for (const glm::vec4& p : planes)
        if (glm::dot(glm::vec3(p), c) + p.w < -r) return false;
      return true;
    };

    drawList_.clear();
    const auto add = [&](glm::vec3 feet, SpriteClipId clip, float fullWidth, uint32_t frame) {
      const SpriteClipHeader& h = sprites_.header(clip);
      const float scale = fullWidth / static_cast<float>(h.sourceWidth);
      const float fullHeight = h.sourceHeight * scale;
      if (!inView(feet + glm::vec3(0, fullHeight * 0.5f, 0), std::max(fullWidth, fullHeight) * 0.6f)) return;
      sprites_.request(clip, SpriteSystem::kOnScreen);
      const SpriteSystem::Texture texture = sprites_.texture(clip);
      if (texture.index == kNoTexture) return;
      const float offsetX = (static_cast<float>(h.cropX) + h.contentWidth * 0.5f - h.sourceWidth * 0.5f) * scale;
      const float offsetY = static_cast<float>(h.sourceHeight - (h.cropY + h.contentHeight)) * scale;
      SpriteInstance s{};
      s.origin = feet + camera_.right * offsetX + glm::vec3(0.0f, offsetY, 0.0f);
      s.width = h.contentWidth * scale;
      s.height = h.contentHeight * scale;
      s.textureIndex = texture.index;
      s.layer = frame;
      s.uvScale = {static_cast<float>(h.contentWidth) / h.width, static_cast<float>(h.contentHeight) / h.height};
      drawList_.push_back(s);
    };
    for (const Card& c : cards_) add(c.feet, c.clip, c.width, 0);
    std::mt19937 rng(static_cast<uint32_t>(clock * 10.0));
    for (Unit& u : units_) {
      if (clock >= u.end) {
        u.current = u.next;
        u.next = unitClips_[rng() % unitClips_.size()];
        u.start = clock;
        u.end = clock + 2.0 + (rng() % 300) / 100.0;
      }
      if (clock >= u.end - 1.2) sprites_.request(u.next, SpriteSystem::kSoon);
      const SpriteClipHeader& h = sprites_.header(u.current);
      const uint32_t frame = h.frames > 1 ? static_cast<uint32_t>((clock - u.start) * std::max(1.0f, h.fps)) % h.frames : 0;
      // A character's canvas: body 1.53 hex radii tall, canvas about 1.3 bodies.
      add(u.feet, u.current, kHexRadius * 1.53f * 1.3f * h.sourceWidth / h.sourceHeight, frame);
    }
    const glm::vec3 eye = camera_.position, fwd = camera_.forward;
    std::sort(drawList_.begin(), drawList_.end(),
              [&](const SpriteInstance& a, const SpriteInstance& b) { return glm::dot(a.origin - eye, fwd) > glm::dot(b.origin - eye, fwd); });
  }

  void startPhase(Phase p, std::chrono::steady_clock::time_point now) {
    if (phase_ != Phase::Loading && phase_ != Phase::Done) {
      const Measure& m = measures_[static_cast<int>(phase_) - 1];
      const bool ok = m.frames > 0 && m.worstBusy <= kBudgetMs && m.worstGpu <= kBudgetMs && m.overflow == 0;
      passed_ = passed_ && ok;
      const std::string line = std::format(
          "{}: {} frames, main thread avg {:.2f} / worst {:.2f} ms, GPU avg {:.2f} / worst {:.2f} ms, up to {} clusters / {:.1f} M triangles, {} cards{} -> {}",
          phaseName(phase_), m.frames, m.sumBusy / std::max<uint64_t>(1, m.frames), m.worstBusy, m.sumGpu / std::max<uint64_t>(1, m.frames),
          m.worstGpu, m.maxClusters, m.maxTriangles / 1e6, m.maxCards, m.overflow ? std::format(", {} OVERFLOWED", m.overflow) : "", ok ? "PASS" : "FAIL");
      report_.push_back(line);
      std::vector<double> sorted = m.gpuFrames;
      std::sort(sorted.begin(), sorted.end());
      const auto at = [&](double q) { return sorted.empty() ? 0.0 : sorted[std::min(sorted.size() - 1, static_cast<size_t>(q * sorted.size()))]; };
      const auto over = std::count_if(sorted.begin(), sorted.end(), [](double v) { return v > kBudgetMs; });
      const std::string distribution = std::format("  GPU frame: median {:.2f}, 99th percentile {:.2f}, worst {:.2f} ms, {} of {} frames over {:.1f} ms",
                                                   at(0.5), at(0.99), sorted.empty() ? 0.0 : sorted.back(), over, sorted.size(), kBudgetMs);
      report_.push_back(distribution);
      // Every frame's GPU time, for looking at the pattern (build/battle-<phase>.csv).
      if (std::FILE* csv = std::fopen(std::format("build/battle-{}.csv", static_cast<int>(phase_)).c_str(), "w")) {
        std::fprintf(csv, "gpu_ms,clusters,early,retest,triangles,instances\n");
        for (const std::string& row : m.rows) std::fprintf(csv, "%s\n", row.c_str());
        std::fclose(csv);
      }
      logInfo("battle: {}", distribution);
      std::string passes = "  GPU passes (avg / worst ms):";
      for (const PassTime& t : m.passes)
        passes += std::format(" {} {:.2f} / {:.2f},", t.name, t.sum / std::max<uint64_t>(1, t.frames), t.worst);
      report_.push_back(passes);
      logInfo("battle: {}", passes);
      logInfo("battle: {}", line);
    } else if (phase_ == Phase::Loading) {
      logInfo("battle: loaded in {:.1f} s", std::chrono::duration<double>(now - start_).count());
    }
    phase_ = p;
    phaseStart_ = now;
    phaseFrames_ = 0;
    if (p == Phase::Done) logInfo("battle (M3 exit test): {}", passed_ ? "PASS" : "FAIL");
  }

  struct NamedMesh {
    MeshId id;
    std::string name;
  };

  Renderer& renderer_;
  Content content_;
  UploadQueue uploads_;
  MeshScene meshes_;
  SpriteSystem sprites_;
  std::vector<NamedMesh> meshIds_;
  std::vector<Card> cards_;
  std::vector<Unit> units_;
  std::vector<SpriteClipId> unitClips_;
  std::vector<SpriteClipId> decorClips_;  // every decor clip the map uses (prefetched while loading)
  std::vector<SpriteInstance> drawList_;
  uint32_t meshCount_ = 0;
  bool placed_ = false, passed_ = true;
  Phase phase_ = Phase::Loading;
  Camera camera_;
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now(), phaseStart_ = start_;
  uint64_t phaseFrames_ = 0;
  Measure measures_[3];
  std::vector<std::string> report_;
};

}  // namespace ember
