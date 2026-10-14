#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

#include "core/Jobs.h"

namespace ember {

class AssetManager;
class IoQueue;
class Renderer;

// Engine performance panel (ImGui): frame and main-thread times, GPU passes, job threads, disk
// streaming and asset counts. Any source may be null; its section is skipped.
class PerfOverlay {
 public:
  struct Sources {
    const JobSystem* jobs = nullptr;
    const IoQueue* io = nullptr;
    const AssetManager* assets = nullptr;
    const Renderer* renderer = nullptr;
  };

  // frameMs: whole frame; busyMs: the part the main thread spent working (not waiting on GPU/vsync).
  void addFrame(double frameMs, double busyMs);
  void draw(const Sources& sources);

  bool visible = true;

 private:
  static constexpr size_t kHistory = 240;

  void sampleRates(const Sources& sources);

  std::array<float, kHistory> frameMs_{};
  std::array<float, kHistory> busyMs_{};
  size_t frames_ = 0;  // total frames added

  // Rates are averaged over half-second windows.
  std::chrono::steady_clock::time_point sampleTime_{};
  std::vector<JobSystem::ThreadStats> lastJobs_;
  uint64_t lastBytesRead_ = 0;
  std::vector<float> threadLoad_;  // 0..1 per thread
  double jobsPerSecond_ = 0.0;
  double readMegabytesPerSecond_ = 0.0;
};

}  // namespace ember
