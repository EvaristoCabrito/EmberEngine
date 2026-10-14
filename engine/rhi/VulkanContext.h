#pragma once

#include <functional>

#include "rhi/VkCommon.h"

struct SDL_Window;

namespace ember {

struct VulkanContextDesc {
  // Khronos validation layer + debug messages (development builds).
  bool validation = true;
  // Window to present to; null renders off-screen only (tools, frame capture).
  SDL_Window* window = nullptr;
};

// Instance, GPU, logical device, graphics queue and GPU memory allocator.
// Requires Vulkan 1.3 with dynamic rendering, synchronization2, bindless descriptors
// and buffer device addresses: the feature set every modern AAA renderer is built on.
class VulkanContext {
 public:
  explicit VulkanContext(const VulkanContextDesc& desc);
  ~VulkanContext();
  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;

  // Records commands, submits them and waits (uploads, readbacks, one-off setup).
  void immediateSubmit(const std::function<void(VkCommandBuffer)>& record);

  // Validation errors reported so far (frame captures fail if this is not zero).
  uint32_t validationErrors() const { return validationErrors_; }

  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice gpu = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties gpuProperties{};
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamily = 0;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VmaAllocator allocator = VK_NULL_HANDLE;

 private:
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                       VkDebugUtilsMessageTypeFlagsEXT types,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                       void* user);

  void createInstance(const VulkanContextDesc& desc);
  void pickGpu();
  void createDevice();

  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
  VkCommandPool immediatePool_ = VK_NULL_HANDLE;
  VkFence immediateFence_ = VK_NULL_HANDLE;
  uint32_t validationErrors_ = 0;
};

}  // namespace ember
