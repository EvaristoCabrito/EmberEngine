#pragma once

// Every Vulkan-facing file includes this: volk (function loader), VMA, and error checking.
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include "core/Log.h"

#define VK_CHECK(expr)                                                                          \
  do {                                                                                          \
    const VkResult vkCheckResult_ = (expr);                                                     \
    if (vkCheckResult_ != VK_SUCCESS)                                                           \
      ::ember::fatal(std::format("{} -> {} ({}:{})", #expr, string_VkResult(vkCheckResult_),    \
                                 __FILE__, __LINE__));                                          \
  } while (0)

namespace ember {

// Frames the CPU may record ahead of the GPU.
inline constexpr uint32_t kFramesInFlight = 2;

}  // namespace ember
