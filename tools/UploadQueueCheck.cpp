#include "rhi/VulkanContext.h"
#include "rhi/UploadQueue.h"
#include "rhi/Image.h"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

int main() {
  using namespace ember;
  VulkanContext context({true, nullptr});
  UploadQueue queue(context);
  auto drain = [&](ImageUpload& up) {
    for (unsigned frame = 0; frame < 256; ++frame) {
      queue.flush();
      VK_CHECK(vkQueueWaitIdle(context.transferQueue));
      queue.flush();
      if (queue.finished(up)) return true;
    }
    return false;
  };
  // Leave the write head nonzero, then reclaim the first reservation.
  std::array<std::byte,16> primer{};
  Buffer small = createDeviceBuffer(context, primer.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
  ImageUpload first{}; first.buffer=small.buffer; first.data=primer;
  queue.enqueue(first);
  if (!drain(first)) return 1;
  // A real full-capacity BC7 image followed by another resource, just like the map load.
  constexpr uint64_t bytes=64ull<<20;
  std::vector<std::byte> source(bytes);
  for (size_t i=0;i<source.size();++i) source[i]=std::byte((i*17+31)&255);
  ImageDesc desc; desc.format=VK_FORMAT_BC7_SRGB_BLOCK; desc.extent={8192,8192};
  desc.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  desc.sharedWithTransfer=true;
  Image image=createImage(context,desc);
  ImageUpload large{image.image,8192,8192,1,1,16,source};
  ImageUpload tail{};tail.buffer=small.buffer;tail.data=primer;
  queue.enqueue(large);queue.enqueue(tail);
  if (!drain(tail) || !queue.finished(large)) {
    std::cerr<<"FAIL: full-capacity image blocked the resources behind it\n";
    queue.cancel(large);queue.cancel(tail);
    destroyImage(context,image);destroyBuffer(context,small);return 2;
  }
  Buffer readback=createReadbackBuffer(context,bytes);
  context.immediateSubmit([&](VkCommandBuffer cmd) {
    transition(cmd,image.image,VK_IMAGE_ASPECT_COLOR_BIT,{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT});
    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={8192,8192,1};
    vkCmdCopyImageToBuffer(cmd,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
  });
  VK_CHECK(vmaInvalidateAllocation(context.allocator,readback.allocation,0,bytes));
  const bool match=std::memcmp(source.data(),readback.mapped,source.size())==0;
  const auto stats=queue.stats();
  destroyBuffer(context,readback);destroyImage(context,image);destroyBuffer(context,small);
  if (!match || context.validationErrors()) return 3;
  std::cout<<"PASS: nonzero head -> full 64 MiB 8192x8192 texture -> following resource; GPU readback matched all "
           <<bytes<<" bytes; queued="<<stats.queued<<"; Vulkan validation errors="<<context.validationErrors()<<"\n";
  return 0;
}
