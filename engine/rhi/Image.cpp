#include "rhi/Image.h"

#include "rhi/VulkanContext.h"

namespace ember {

namespace {
bool isDepthFormat(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D16_UNORM;
}
}  // namespace

VkImageCreateInfo imageCreateInfo(VulkanContext& context, const ImageDesc& desc, uint32_t (&families)[2]) {
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = desc.format;
  info.extent = {desc.extent.width, desc.extent.height, 1};
  info.mipLevels = desc.mipLevels;
  info.arrayLayers = desc.layers;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = desc.usage;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  families[0] = context.queueFamily;
  families[1] = context.transferFamily;
  if (desc.sharedWithTransfer && context.dedicatedTransfer()) {
    // Concurrent sharing: no queue-family ownership transfers between the copy and the first use.
    info.sharingMode = VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount = 2;
    info.pQueueFamilyIndices = families;
  }
  return info;
}

bool tryCreateImage(VulkanContext& context, const ImageDesc& desc, Image& out) {
  out = {};
  out.format = desc.format;
  out.extent = desc.extent;
  out.aspect = isDepthFormat(desc.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

  uint32_t families[2];
  const VkImageCreateInfo info = imageCreateInfo(context, desc, families);
  VmaAllocationCreateInfo alloc{};
  alloc.pool = desc.pool;
  if (!desc.pool) {
    alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    // Render targets get their own memory block (the driver can then compress them better).
    if (desc.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
      alloc.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  }
  const VkResult result = vmaCreateImage(context.allocator, &info, &alloc, &out.image, &out.allocation, nullptr);
  if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_HOST_MEMORY) {
    out = {};
    return false;
  }
  VK_CHECK(result);

  VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = out.image;
  viewInfo.viewType = desc.layers > 1 || desc.arrayView ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = desc.format;
  viewInfo.subresourceRange = {out.aspect, 0, desc.mipLevels, 0, desc.layers};
  VK_CHECK(vkCreateImageView(context.device, &viewInfo, nullptr, &out.view));
  return true;
}

Image createImage(VulkanContext& context, const ImageDesc& desc) {
  Image out;
  EMBER_CHECK(tryCreateImage(context, desc, out), "Out of GPU memory creating an image");
  return out;
}

void destroyImage(VulkanContext& context, Image& image) {
  if (image.view) vkDestroyImageView(context.device, image.view, nullptr);
  if (image.image) vmaDestroyImage(context.allocator, image.image, image.allocation);
  image = {};
}

Buffer createReadbackBuffer(VulkanContext& context, VkDeviceSize size) {
  Buffer out;
  out.size = size;
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = size;
  info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
  alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  VmaAllocationInfo allocInfo;
  VK_CHECK(vmaCreateBuffer(context.allocator, &info, &alloc, &out.buffer, &out.allocation, &allocInfo));
  out.mapped = allocInfo.pMappedData;
  return out;
}

Buffer createHostBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage) {
  Buffer out;
  out.size = size;
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = size;
  info.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_AUTO;
  alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  VmaAllocationInfo allocInfo;
  VK_CHECK(vmaCreateBuffer(context.allocator, &info, &alloc, &out.buffer, &out.allocation, &allocInfo));
  out.mapped = allocInfo.pMappedData;
  return out;
}

Buffer createDeviceBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, bool sharedWithTransfer) {
  Buffer out;
  out.size = size;
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = size;
  info.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  const uint32_t families[2] = {context.queueFamily, context.transferFamily};
  if (sharedWithTransfer && context.dedicatedTransfer()) {
    info.sharingMode = VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount = 2;
    info.pQueueFamilyIndices = families;
  }
  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  VK_CHECK(vmaCreateBuffer(context.allocator, &info, &alloc, &out.buffer, &out.allocation, nullptr));
  return out;
}

VkDeviceAddress bufferAddress(VulkanContext& context, const Buffer& buffer) {
  VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
  info.buffer = buffer.buffer;
  return vkGetBufferDeviceAddress(context.device, &info);
}

void destroyBuffer(VulkanContext& context, Buffer& buffer) {
  if (buffer.buffer) vmaDestroyBuffer(context.allocator, buffer.buffer, buffer.allocation);
  buffer = {};
}

void transition(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, const Transition& t) {
  VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  barrier.srcStageMask = t.srcStage;
  barrier.srcAccessMask = t.srcAccess;
  barrier.dstStageMask = t.dstStage;
  barrier.dstAccessMask = t.dstAccess;
  barrier.oldLayout = t.from;
  barrier.newLayout = t.to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = {aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(cmd, &dependency);
}

}  // namespace ember
