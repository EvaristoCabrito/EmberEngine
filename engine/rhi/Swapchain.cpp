#include "rhi/Swapchain.h"

#include <algorithm>

#include "rhi/VulkanContext.h"

namespace ember {

Swapchain::Swapchain(VulkanContext& context, uint32_t width, uint32_t height, bool vsync)
    : context_(context), vsync_(vsync) {
  create(width, height, VK_NULL_HANDLE);
}

Swapchain::~Swapchain() {
  destroyPerImage();
  if (swapchain_) vkDestroySwapchainKHR(context_.device, swapchain_, nullptr);
}

void Swapchain::recreate(uint32_t width, uint32_t height) {
  vkDeviceWaitIdle(context_.device);
  destroyPerImage();
  VkSwapchainKHR old = swapchain_;
  create(width, height, old);
  vkDestroySwapchainKHR(context_.device, old, nullptr);
}

void Swapchain::destroyPerImage() {
  for (VkSemaphore semaphore : renderFinished_) vkDestroySemaphore(context_.device, semaphore, nullptr);
  renderFinished_.clear();
  images_.clear();
}

void Swapchain::create(uint32_t width, uint32_t height, VkSwapchainKHR old) {
  VkPhysicalDevice gpu = context_.gpu;
  VkSurfaceKHR surface = context_.surface;

  VkSurfaceCapabilitiesKHR caps;
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps));

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, formats.data());
  // UNORM on purpose: the final pass writes already-encoded sRGB values, copied as-is.
  VkSurfaceFormatKHR chosen = formats[0];
  for (const auto& f : formats)
    if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }

  uint32_t modeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modeCount, nullptr);
  std::vector<VkPresentModeKHR> modes(modeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modeCount, modes.data());
  VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;  // always available
  if (!vsync_) {
    for (VkPresentModeKHR m : modes)
      if (m == VK_PRESENT_MODE_MAILBOX_KHR) mode = m;
  }

  extent_ = caps.currentExtent.width != UINT32_MAX
                ? caps.currentExtent
                : VkExtent2D{std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
                             std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)};
  uint32_t imageCount = std::max(caps.minImageCount + 1, 3u);
  if (caps.maxImageCount) imageCount = std::min(imageCount, caps.maxImageCount);

  VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  info.surface = surface;
  info.minImageCount = imageCount;
  info.imageFormat = chosen.format;
  info.imageColorSpace = chosen.colorSpace;
  info.imageExtent = extent_;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.preTransform = caps.currentTransform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = mode;
  info.clipped = VK_TRUE;
  info.oldSwapchain = old;
  VK_CHECK(vkCreateSwapchainKHR(context_.device, &info, nullptr, &swapchain_));
  format_ = chosen.format;

  uint32_t count = 0;
  vkGetSwapchainImagesKHR(context_.device, swapchain_, &count, nullptr);
  images_.resize(count);
  vkGetSwapchainImagesKHR(context_.device, swapchain_, &count, images_.data());
  renderFinished_.resize(count);
  VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (VkSemaphore& semaphore : renderFinished_)
    VK_CHECK(vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &semaphore));
}

}  // namespace ember
