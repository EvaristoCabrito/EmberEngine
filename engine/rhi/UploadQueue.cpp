#include "rhi/UploadQueue.h"

#include <algorithm>
#include <cstring>

#include "rhi/VulkanContext.h"

namespace ember {

namespace {

constexpr uint64_t kAlignment = 16;  // a BC block; also satisfies the 4-byte copy rule

uint64_t alignUp(uint64_t value) { return (value + kAlignment - 1) & ~(kAlignment - 1); }

}  // namespace

UploadQueue::UploadQueue(VulkanContext& context, VkDeviceSize stagingBytes, VkDeviceSize bytesPerFrame)
    : context_(context), stagingBytes_(stagingBytes), bytesPerFrame_(std::min(bytesPerFrame, stagingBytes)) {
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = stagingBytes;
  info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
  alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  VmaAllocationInfo allocInfo;
  VK_CHECK(vmaCreateBuffer(context.allocator, &info, &alloc, &staging_.buffer, &staging_.allocation, &allocInfo));
  staging_.mapped = allocInfo.pMappedData;
  staging_.size = stagingBytes;

  for (Slot& slot : slots_) {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = context.transferFamily;
    VK_CHECK(vkCreateCommandPool(context.device, &poolInfo, nullptr, &slot.pool));
    VkCommandBufferAllocateInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdInfo.commandPool = slot.pool;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(context.device, &cmdInfo, &slot.cmd));
  }

  VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
  type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  semaphoreInfo.pNext = &type;
  VK_CHECK(vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &timeline_));
}

UploadQueue::~UploadQueue() {
  if (lastSignal_) {
    VkSemaphoreWaitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wait.semaphoreCount = 1;
    wait.pSemaphores = &timeline_;
    wait.pValues = &lastSignal_;
    vkWaitSemaphores(context_.device, &wait, UINT64_MAX);
  }
  vkDestroySemaphore(context_.device, timeline_, nullptr);
  for (Slot& slot : slots_) vkDestroyCommandPool(context_.device, slot.pool, nullptr);
  destroyBuffer(context_, staging_);
}


void UploadQueue::enqueue(ImageUpload& upload) {
  upload.nextMip = upload.nextLayer = 0;
  upload.dataOffset = 0;
  upload.signalValue = 0;
  upload.queued = true;
  pending_.push_back(&upload);
}

void UploadQueue::cancel(ImageUpload& upload) {
  if (!upload.queued) return;
  pending_.erase(std::find(pending_.begin(), pending_.end(), &upload));
  upload.queued = false;
}


void UploadQueue::reclaim() {
  VK_CHECK(vkGetSemaphoreCounterValue(context_.device, timeline_, &completed_));
  for (Slot& slot : slots_) {
    if (slot.ringBytes && slot.value <= completed_) {
      used_ -= slot.ringBytes;
      slot.ringBytes = 0;
    }
  }
}

bool UploadQueue::reserve(uint64_t bytes, uint64_t& offset) {
  const uint64_t size = alignUp(bytes);
  // Data never wraps: a reservation that would cross the end starts again at 0, and the skipped end
  // counts as used until this submission finishes. The ring is first-in, first-out, so with
  // used_ + total <= capacity the space from head_ onwards is free.
  const uint64_t skip = head_ + size > stagingBytes_ ? stagingBytes_ - head_ : 0;
  if (size > stagingBytes_ || used_ + skip + size > stagingBytes_) return false;
  if (skip) head_ = 0;
  offset = head_;
  head_ = (head_ + size) % stagingBytes_;
  used_ += skip + size;
  slots_[nextSlot_].ringBytes += skip + size;
  return true;
}

void UploadQueue::flush() {
  bytesLastFlush_ = 0;
  reclaim();
  if (pending_.empty()) return;
  Slot& slot = slots_[nextSlot_];
  if (slot.ringBytes != 0 || slot.value > completed_) return;  // every slot in flight

  VK_CHECK(vkResetCommandPool(context_.device, slot.pool, 0));
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK(vkBeginCommandBuffer(slot.cmd, &begin));

  std::vector<ImageUpload*> completedUploads;
  uint64_t budget = bytesPerFrame_;
  auto* staging = static_cast<std::byte*>(staging_.mapped);
  while (!pending_.empty() && budget > 0) {
    ImageUpload& up = *pending_.front();
    if (up.buffer) {
      // Plain bytes: as much as the frame's budget and the ring allow.
      uint64_t bytes = std::min<uint64_t>(up.data.size() - up.dataOffset, budget), offset = 0;
      while (bytes > 0 && !reserve(bytes, offset)) bytes /= 2;
      if (bytes == 0) break;  // ring full: the rest goes next frame
      std::memcpy(staging + offset, up.data.data() + up.dataOffset, bytes);
      const VkBufferCopy region{offset, up.bufferOffset + up.dataOffset, bytes};
      vkCmdCopyBuffer(slot.cmd, staging_.buffer, up.buffer, 1, &region);
      up.dataOffset += bytes;
      budget -= bytes;
      bytesLastFlush_ += bytes;
      if (up.dataOffset == up.data.size()) {
        pending_.pop_front();
        completedUploads.push_back(&up);
      }
      continue;
    }
    const uint32_t mipW = std::max(1u, up.width >> up.nextMip), mipH = std::max(1u, up.height >> up.nextMip);
    const uint64_t layerBytes = uint64_t{(mipW + 3) / 4} * ((mipH + 3) / 4) * up.blockBytes;
    // As many whole layers of this mip as the frame's budget allows; at least one per frame, so a
    // layer bigger than the budget still moves (alone).
    const uint32_t layersLeft = up.layers - up.nextLayer;
    uint32_t count = static_cast<uint32_t>(std::min<uint64_t>(layersLeft, std::max<uint64_t>(1, budget / layerBytes)));
    if (bytesLastFlush_ > 0 && count * layerBytes > budget) break;
    uint64_t offset = 0;
    while (count > 0 && !reserve(count * layerBytes, offset)) count /= 2;
    if (count == 0) break;  // ring full: the rest goes next frame

    const uint64_t bytes = count * layerBytes;
    if (up.dataOffset == 0)
      transition(slot.cmd, up.image, VK_IMAGE_ASPECT_COLOR_BIT,
                 {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                  VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});
    std::memcpy(staging + offset, up.data.data() + up.dataOffset, bytes);
    VkBufferImageCopy region{};
    region.bufferOffset = offset;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, up.nextMip, up.nextLayer, count};
    region.imageExtent = {mipW, mipH, 1};
    vkCmdCopyBufferToImage(slot.cmd, staging_.buffer, up.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    up.dataOffset += bytes;
    up.nextLayer += count;
    if (up.nextLayer == up.layers) {
      up.nextLayer = 0;
      ++up.nextMip;
    }
    budget = bytes >= budget ? 0 : budget - bytes;
    bytesLastFlush_ += bytes;

    if (up.nextMip == up.mipCount) {
      // Ready for sampling. The graphics queue's wait on the timeline makes the copy visible.
      transition(slot.cmd, up.image, VK_IMAGE_ASPECT_COLOR_BIT,
                 {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                  VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE});
      pending_.pop_front();
      completedUploads.push_back(&up);
    }
  }
  VK_CHECK(vkEndCommandBuffer(slot.cmd));
  if (bytesLastFlush_ == 0) return;  // nothing fit; the recorded (empty) buffer is simply reset next time

  slot.value = ++lastSignal_;
  VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  cmdInfo.commandBuffer = slot.cmd;
  VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  signal.semaphore = timeline_;
  signal.value = slot.value;
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &cmdInfo;
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
  VK_CHECK(vkQueueSubmit2(context_.transferQueue, 1, &submit, VK_NULL_HANDLE));
  for (ImageUpload* up : completedUploads) {
    up->signalValue = slot.value;
    up->queued = false;
  }
  bytesUploaded_ += bytesLastFlush_;
  nextSlot_ = (nextSlot_ + 1) % kSlots;
}

}  // namespace ember
