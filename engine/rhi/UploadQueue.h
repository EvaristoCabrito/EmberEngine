#pragma once

#include <array>
#include <deque>
#include <span>
#include <vector>

#include "rhi/Image.h"
#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;

// One upload, owned by the caller, who keeps it, the destination and the source bytes alive until
// finished() or cancel(). Either
//  - an image: the whole mip chain of a (possibly array) block-compressed texture, from bytes in the
//    engine's mip-major layout (per mip, every layer contiguous), or
//  - a buffer (`buffer` set, `image` null): `data` copied to `bufferOffset`, split across frames like images.
struct ImageUpload {
  VkImage image = VK_NULL_HANDLE;
  uint32_t width = 0, height = 0, mipCount = 1, layers = 1;
  uint32_t blockBytes = 16;  // bytes per 4x4 block (BC7: 16)
  std::span<const std::byte> data;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceSize bufferOffset = 0;

  // Progress, managed by the queue.
  uint32_t nextMip = 0, nextLayer = 0;
  uint64_t dataOffset = 0;
  uint64_t signalValue = 0;  // timeline value of the submission holding the last copy; 0 until then
  bool queued = false;
};

// Streams texture data to the GPU on the transfer queue (the GPU's copy engine when it has one).
// Data passes through a fixed staging ring in host memory; each frame flush() copies at most
// `bytesPerFrame`, splitting big textures across frames by mip and layer, so no frame ever pays for a
// whole clip. A timeline semaphore reports completion; the renderer waits on it (already signalled,
// so it costs nothing) the first time it uses a newly finished texture. Main thread only.
class UploadQueue {
 public:
  UploadQueue(VulkanContext& context, VkDeviceSize stagingBytes = 64ull << 20, VkDeviceSize bytesPerFrame = 8ull << 20);
  ~UploadQueue();
  UploadQueue(const UploadQueue&) = delete;
  UploadQueue& operator=(const UploadQueue&) = delete;

  void enqueue(ImageUpload& upload);
  // Drops an upload that has not finished staging. If copies were already submitted, wait for the GPU
  // to go idle before destroying the image.
  void cancel(ImageUpload& upload);
  // From the completion value read at the last flush(): cheap enough to ask every frame per texture.
  bool finished(const ImageUpload& upload) const { return upload.signalValue != 0 && completed_ >= upload.signalValue; }

  // Once per frame: stages and submits the next slice of work.
  void flush();

  VkSemaphore timeline() const { return timeline_; }

  struct Stats {
    uint64_t bytesUploaded = 0;  // since start
    uint64_t bytesLastFlush = 0;
    uint32_t queued = 0;         // uploads not yet fully staged
  };
  Stats stats() const { return {bytesUploaded_, bytesLastFlush_, static_cast<uint32_t>(pending_.size())}; }

 private:
  static constexpr uint32_t kSlots = 8;  // submissions in flight

  struct Slot {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    uint64_t value = 0;      // timeline value it signals
    uint64_t ringEnd = 0;    // ring head after its data (frees the ring up to here)
    uint64_t ringBytes = 0;  // bytes it holds in the ring
  };

  // Reserves `bytes` in the ring (16-byte aligned); false if the space is still in use.
  bool reserve(uint64_t bytes, uint64_t& offset);
  void reclaim();

  VulkanContext& context_;
  VkDeviceSize stagingBytes_, bytesPerFrame_;
  Buffer staging_;
  uint64_t head_ = 0;  // next write position
  uint64_t used_ = 0;  // bytes held by unfinished submissions
  std::array<Slot, kSlots> slots_{};
  uint32_t nextSlot_ = 0;
  VkSemaphore timeline_ = VK_NULL_HANDLE;
  uint64_t lastSignal_ = 0;
  uint64_t completed_ = 0;
  std::deque<ImageUpload*> pending_;
  uint64_t bytesUploaded_ = 0, bytesLastFlush_ = 0;
};

}  // namespace ember
