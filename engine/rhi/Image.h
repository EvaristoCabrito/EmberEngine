#pragma once

#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;

struct ImageDesc {
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  VkExtent2D extent{};
  VkImageUsageFlags usage = 0;
  uint32_t mipLevels = 1;
  uint32_t layers = 1;
};

// A GPU image with its default view and VMA allocation.
struct Image {
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{};
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

Image createImage(VulkanContext& context, const ImageDesc& desc);
void destroyImage(VulkanContext& context, Image& image);

struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  void* mapped = nullptr;  // set for host-visible buffers
  VkDeviceSize size = 0;
};

// Host-visible buffer the CPU reads after the GPU writes it (frame capture, GPU queries).
Buffer createReadbackBuffer(VulkanContext& context, VkDeviceSize size);
void destroyBuffer(VulkanContext& context, Buffer& buffer);

// One synchronization2 image layout transition.
struct Transition {
  VkImageLayout from;
  VkImageLayout to;
  VkPipelineStageFlags2 srcStage;
  VkAccessFlags2 srcAccess;
  VkPipelineStageFlags2 dstStage;
  VkAccessFlags2 dstAccess;
};
void transition(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, const Transition& t);

}  // namespace ember
