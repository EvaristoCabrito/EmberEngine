#pragma once

#include <array>
#include <vector>

#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;

// Measures GPU time per pass with timestamp queries. Each frame slot has its own query pool; a slot's
// results are read when the renderer has waited on that slot's fence, so reading never stalls.
// Results are therefore kFramesInFlight frames old.
class GpuTimer {
 public:
  static constexpr uint32_t kMaxScopes = 32;

  explicit GpuTimer(VulkanContext& context);
  ~GpuTimer();
  GpuTimer(const GpuTimer&) = delete;
  GpuTimer& operator=(const GpuTimer&) = delete;

  // Call once the slot's previous submission has finished: collects its results, then reuses it.
  void beginFrame(uint32_t slot);

  // `name` must outlive the frame (string literals).
  uint32_t begin(VkCommandBuffer cmd, const char* name);
  void end(VkCommandBuffer cmd, uint32_t scope);

  struct Result {
    const char* name;
    double milliseconds;
  };
  // Latest finished frame, in the order the scopes began.
  const std::vector<Result>& results() const { return results_; }
  bool supported() const { return supported_; }

 private:
  VulkanContext& context_;
  bool supported_ = false;
  double nanosecondsPerTick_ = 1.0;
  uint64_t validMask_ = ~0ull;
  std::array<VkQueryPool, kFramesInFlight> pools_{};
  std::array<std::vector<const char*>, kFramesInFlight> names_{};
  uint32_t slot_ = 0;
  std::vector<Result> results_;
};

}  // namespace ember
