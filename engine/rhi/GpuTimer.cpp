#include "rhi/GpuTimer.h"

#include "rhi/VulkanContext.h"

namespace ember {

GpuTimer::GpuTimer(VulkanContext& context) : context_(context) {
  uint32_t familyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &familyCount, nullptr);
  std::vector<VkQueueFamilyProperties> families(familyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &familyCount, families.data());
  const uint32_t validBits = families[context.queueFamily].timestampValidBits;
  supported_ = validBits > 0 && context.gpuProperties.limits.timestampComputeAndGraphics;
  if (!supported_) {
    logWarn("GPU timestamps not supported; GPU pass timings disabled");
    return;
  }
  validMask_ = validBits >= 64 ? ~0ull : (1ull << validBits) - 1;
  nanosecondsPerTick_ = context.gpuProperties.limits.timestampPeriod;

  VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = kMaxScopes * 2;
  for (VkQueryPool& pool : pools_) {
    VK_CHECK(vkCreateQueryPool(context.device, &info, nullptr, &pool));
    vkResetQueryPool(context.device, pool, 0, info.queryCount);
  }
  logInfo("startup: GPU timer ready");
}

GpuTimer::~GpuTimer() {
  for (VkQueryPool pool : pools_)
    if (pool) vkDestroyQueryPool(context_.device, pool, nullptr);
}

void GpuTimer::beginFrame(uint32_t slot) {
  slot_ = slot;
  if (!supported_) return;
  std::vector<const char*>& names = names_[slot];
  if (!names.empty()) {
    std::array<uint64_t, kMaxScopes * 2> ticks{};
    const uint32_t queries = static_cast<uint32_t>(names.size()) * 2;
    const VkResult got = vkGetQueryPoolResults(context_.device, pools_[slot], 0, queries, queries * sizeof(uint64_t),
                                               ticks.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (got == VK_SUCCESS) {
      results_.clear();
      for (size_t i = 0; i < names.size(); ++i) {
        const uint64_t elapsed = ((ticks[i * 2 + 1] & validMask_) - (ticks[i * 2] & validMask_)) & validMask_;
        results_.push_back({names[i], static_cast<double>(elapsed) * nanosecondsPerTick_ * 1e-6});
      }
    }
    vkResetQueryPool(context_.device, pools_[slot], 0, queries);
    names.clear();
  }
}

uint32_t GpuTimer::begin(VkCommandBuffer cmd, const char* name) {
  std::vector<const char*>& names = names_[slot_];
  if (!supported_ || names.size() == kMaxScopes) return UINT32_MAX;
  const uint32_t scope = static_cast<uint32_t>(names.size());
  names.push_back(name);
  vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pools_[slot_], scope * 2);
  return scope;
}

void GpuTimer::end(VkCommandBuffer cmd, uint32_t scope) {
  if (scope == UINT32_MAX) return;
  vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pools_[slot_], scope * 2 + 1);
}

}  // namespace ember
