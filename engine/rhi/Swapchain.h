#pragma once

#include <vector>

#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;

// The window's presentable images. The renderer never draws into these directly: it renders
// into its own HDR targets and copies the finished frame here.
class Swapchain {
 public:
  Swapchain(VulkanContext& context, uint32_t width, uint32_t height, bool vsync);
  ~Swapchain();
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  // False (and the old swapchain kept) while the surface has no area, e.g. mid-minimise.
  bool recreate(uint32_t width, uint32_t height);

  VkSwapchainKHR handle() const { return swapchain_; }
  VkFormat format() const { return format_; }
  VkExtent2D extent() const { return extent_; }
  VkImage image(uint32_t index) const { return images_[index]; }
  // Signalled when rendering into image `index` is finished; waited on by present.
  VkSemaphore renderFinished(uint32_t index) const { return renderFinished_[index]; }

 private:
  void create(uint32_t width, uint32_t height, VkSwapchainKHR old);
  void destroyPerImage();

  VulkanContext& context_;
  bool vsync_;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};
  std::vector<VkImage> images_;
  std::vector<VkSemaphore> renderFinished_;
};

}  // namespace ember
