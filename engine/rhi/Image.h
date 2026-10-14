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
  bool arrayView = false;       // 2D_ARRAY view even for one layer (shaders read it as Texture2DArray)
  bool sharedWithTransfer = false;  // written by the transfer queue, read by graphics (concurrent sharing)
  VmaPool pool = VK_NULL_HANDLE;    // allocate from this pre-made pool (no driver allocation at runtime)
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
// Like createImage, but returns false instead of stopping when memory (or the pool) is full.
bool tryCreateImage(VulkanContext& context, const ImageDesc& desc, Image& out);
// The VkImageCreateInfo createImage would use (for choosing memory types and pools).
VkImageCreateInfo imageCreateInfo(VulkanContext& context, const ImageDesc& desc, uint32_t (&families)[2]);
void destroyImage(VulkanContext& context, Image& image);

struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  void* mapped = nullptr;  // set for host-visible buffers
  VkDeviceSize size = 0;
};

// Host-visible buffer the CPU writes every frame and shaders read through its device address
// (per-frame instance data).
Buffer createHostBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage);
VkDeviceAddress bufferAddress(VulkanContext& context, const Buffer& buffer);

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
