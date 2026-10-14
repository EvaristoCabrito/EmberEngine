#pragma once

// Sprite streaming exit test (M2): plays every clip of every character, a few stage slots at a time,
// under a GPU budget smaller than the roster. Each slot requests its next clip ahead of time, the
// way a cutscene timeline or a battle would; the test counts every on-screen frame that still had to
// draw from a tail, and the frame times, GPU memory and failures along the way.

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <random>
#include <string>
#include <vector>

#include "core/Log.h"
#include "render/Camera.h"
#include "render/SpriteDraw.h"
#include "render/SpriteSystem.h"

namespace ember {

class SpriteParade {
 public:
  static constexpr uint32_t kColumns = 6, kRows = 2;
  static constexpr double kLeadSeconds = 1.2;      // the next clip is requested this long before it shows
  static constexpr double kMinShow = 1.0, kMaxShow = 2.5;
  static constexpr double kFrameBudgetMs = 1000.0 / 60.0;

  explicit SpriteParade(SpriteSystem& sprites) : sprites_(sprites) {}

  bool done() const { return finished_; }
  bool passed() const {
    return finished_ && !order_.empty() && lateFrames_ == 0 && worstBusy_ <= kFrameBudgetMs &&
           maxResident_ <= sprites_.stats().budgetBytes;
  }

  // Before SpriteSystem::update(): says what is on screen and what comes next.
  void request(double now) {
    if (!started_) {
      if (!sprites_.ready()) return;
      start(now);
    }
    uint64_t demand = 0;  // full-clip bytes asked for this frame (a budget below this cannot keep up)
    for (Slot& slot : slots_) {
      if (slot.current != kNoSpriteClip && now >= slot.end) advance(slot, now);
      if (slot.current != kNoSpriteClip) {
        sprites_.request(slot.current, SpriteSystem::kOnScreen);
        demand += sprites_.header(slot.current).fullBytes;
      }
      if (slot.next != kNoSpriteClip && now >= slot.end - kLeadSeconds) {
        sprites_.request(slot.next, SpriteSystem::kSoon);
        demand += sprites_.header(slot.next).fullBytes;
        slot.nextAskedEarly = true;
      }
    }
    if (!finished_) maxDemand_ = std::max(maxDemand_, demand);
    finished_ = std::all_of(slots_.begin(), slots_.end(), [](const Slot& s) { return s.current == kNoSpriteClip; });
    if (finished_ && !reported_) report();
  }

  // After SpriteSystem::update(): the cards to draw, back to front.
  void build(double now, const Camera& camera, std::vector<SpriteInstance>& out) {
    out.clear();
    if (!started_) return;
    for (uint32_t i = 0; i < slots_.size(); ++i) {
      Slot& slot = slots_[i];
      if (slot.current == kNoSpriteClip) continue;
      const SpriteClipHeader& h = sprites_.header(slot.current);
      const SpriteSystem::Texture texture = sprites_.texture(slot.current);
      if (texture.index == kNoTexture) continue;
      if (!texture.full) {
        ++tailFrames_;
        if (slot.prefetched) ++lateFrames_;  // it was asked for in time and still was not there
      }
      // Each character stands 2 m tall in its own canvas; the trimmed picture keeps its place in it.
      const float scale = 2.0f / static_cast<float>(h.sourceHeight);
      const float offsetX = (static_cast<float>(h.cropX) + h.contentWidth * 0.5f - h.sourceWidth * 0.5f) * scale;
      const float offsetY = static_cast<float>(h.sourceHeight - (h.cropY + h.contentHeight)) * scale;
      const glm::vec3 feet = slotPosition(i);
      SpriteInstance s{};
      s.origin = feet + camera.right * offsetX + glm::vec3(0.0f, offsetY, 0.0f);
      s.width = h.contentWidth * scale;
      s.height = h.contentHeight * scale;
      s.textureIndex = texture.index;
      s.layer = frameAt(h, now - slot.start);
      s.uvScale = {static_cast<float>(h.contentWidth) / h.width, static_cast<float>(h.contentHeight) / h.height};
      out.push_back(s);
    }
    std::sort(out.begin(), out.end(), [&](const SpriteInstance& a, const SpriteInstance& b) {
      return glm::dot(a.origin - camera.position, camera.forward) > glm::dot(b.origin - camera.position, camera.forward);
    });
  }

  void endFrame(double busyMs) {
    if (!started_ || finished_) return;
    worstBusy_ = std::max(worstBusy_, busyMs);
    maxResident_ = std::max(maxResident_, sprites_.stats().residentBytes);
    ++frames_;
  }

  static Camera camera(float aspect) {
    return Camera::lookAt({0.0f, 3.2f, 11.5f}, {0.0f, 1.1f, -1.5f}, glm::radians(40.0f), aspect);
  }

  void drawPanel() const {
    const SpriteSystem::Stats s = sprites_.stats();
    const double mb = 1024.0 * 1024.0;
    ImGui::SetNextWindowPos({12, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::Begin("Sprite parade", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    if (!started_) {
      ImGui::Text("loading roster and tails...");
    } else {
      ImGui::ProgressBar(order_.empty() ? 1.0f : static_cast<float>(cursor_) / static_cast<float>(order_.size()), {360, 0},
                         std::format("{} / {} clips", std::min(cursor_, order_.size()), order_.size()).c_str());
    }
    ImGui::Text("GPU: %.0f / %.0f MB full clips (peak %.0f), %.1f MB tails", s.residentBytes / mb, s.budgetBytes / mb,
                maxResident_ / mb, s.tailBytes / mb);
    ImGui::Text("clips: %u resident, %u loading, %u uploading, %llu evicted", s.resident, s.loading, s.uploading,
                static_cast<unsigned long long>(s.evictions));
    ImGui::Text("tail frames on screen: %llu (late, after asking in time: %llu)", static_cast<unsigned long long>(tailFrames_),
                static_cast<unsigned long long>(lateFrames_));
    if (finished_) {
      ImGui::Text("%s", passed() ? "PASS" : "FAIL");
      for (const std::string& line : lines_) ImGui::TextUnformatted(line.c_str());
    }
    ImGui::End();
  }

 private:
  struct Slot {
    SpriteClipId current = kNoSpriteClip, next = kNoSpriteClip;
    double start = 0.0, end = 0.0;
    bool prefetched = false;      // current was requested ahead of time
    bool nextAskedEarly = false;  // next has been requested
  };

  glm::vec3 slotPosition(uint32_t i) const {
    const uint32_t column = i % kColumns, row = i / kColumns;
    return {(static_cast<float>(column) - (kColumns - 1) * 0.5f) * 2.6f, 0.0f, -static_cast<float>(row) * 3.0f + (row % 2) * 0.0f};
  }

  static double showSeconds(const SpriteClipHeader& h) {
    const double length = h.fps > 0.0f ? h.frames / h.fps : 1.0;
    return std::clamp(length, kMinShow, kMaxShow);
  }

  static uint32_t frameAt(const SpriteClipHeader& h, double t) {
    if (h.frames <= 1) return 0;
    const auto step = static_cast<uint64_t>(std::max(0.0, t) * h.fps);
    switch (h.mode) {
      case SpritePlayMode::Loop: return static_cast<uint32_t>(step % h.frames);
      case SpritePlayMode::Once: return static_cast<uint32_t>(std::min<uint64_t>(step, h.frames - 1));
      case SpritePlayMode::PingPong: {
        const uint64_t cycle = uint64_t{h.frames} * 2 - 2, x = step % cycle;
        return static_cast<uint32_t>(x < h.frames ? x : cycle - x);
      }
    }
    return 0;
  }

  SpriteClipId take() { return cursor_ < order_.size() ? order_[cursor_++] : kNoSpriteClip; }

  void start(double now) {
    started_ = true;
    for (SpriteClipId id = 0; id < sprites_.clipCount(); ++id)
      if (sprites_.header(id).frames > 0) order_.push_back(id);
    // A fixed-seed shuffle mixes characters and sizes like a real scene. Roster order would put a set's
    // clips (and a test pack's copies of one clip) on stage together, which no scene does.
    std::mt19937 random(0xE3BE5u);
    for (size_t i = order_.size(); i > 1; --i)
      std::swap(order_[i - 1], order_[std::uniform_int_distribution<size_t>(0, i - 1)(random)]);
    slots_.resize(kColumns * kRows);
    // Stagger the slots so clip changes spread out over time, like a real scene.
    for (uint32_t i = 0; i < slots_.size(); ++i) {
      Slot& slot = slots_[i];
      slot.current = take();
      slot.next = take();
      slot.start = now;
      slot.end = now + (slot.current == kNoSpriteClip ? 0.0 : showSeconds(sprites_.header(slot.current))) * (0.5 + 0.5 * i / slots_.size());
    }
    logInfo("sprite parade: {} clips over {} slots", order_.size(), slots_.size());
  }

  void advance(Slot& slot, double now) {
    slot.current = slot.next;
    slot.prefetched = slot.nextAskedEarly;
    slot.nextAskedEarly = false;
    slot.next = take();
    slot.start = now;
    slot.end = now + (slot.current == kNoSpriteClip ? 0.0 : showSeconds(sprites_.header(slot.current)));
  }

  void report() {
    reported_ = true;
    const SpriteSystem::Stats s = sprites_.stats();
    const double mb = 1024.0 * 1024.0;
    lines_ = {
        std::format("{} clips played over {} frames", order_.size(), frames_),
        std::format("GPU budget {:.0f} MB, peak use {:.0f} MB, {} evictions", s.budgetBytes / mb, maxResident_ / mb, s.evictions),
        std::format("peak demand (on screen + next, one frame) {:.0f} MB", maxDemand_ / mb),
        std::format("frames drawn from a tail: {} in total, {} after the clip was asked for in time", tailFrames_, lateFrames_),
        std::format("worst main-thread frame {:.2f} ms (budget {:.2f} ms)", worstBusy_, kFrameBudgetMs),
        std::format("requests that found no room: {}", s.starvedRequests),
        std::format("full-clip pool {:.0f} MB, {} creations retried (fragmented)", s.poolBytes / mb, s.poolRetries),
    };
    logInfo("sprite parade: {}", passed() ? "PASS" : "FAIL");
    for (const std::string& line : lines_) logInfo("  {}", line);
  }

  SpriteSystem& sprites_;
  std::vector<SpriteClipId> order_;
  size_t cursor_ = 0;
  std::vector<Slot> slots_;
  bool started_ = false, finished_ = false, reported_ = false;
  uint64_t frames_ = 0, tailFrames_ = 0, lateFrames_ = 0, maxResident_ = 0, maxDemand_ = 0;
  double worstBusy_ = 0.0;
  std::vector<std::string> lines_;
};

}  // namespace ember
