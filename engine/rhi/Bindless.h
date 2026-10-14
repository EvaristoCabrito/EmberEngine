#pragma once

#include <vector>

#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;

// One global descriptor set holding every texture, sampler and storage image in the game.
// Shaders pick resources by index (passed in push constants or GPU buffers), so the number of
// textures a scene can use is limited only by GPU memory, not by per-draw binding slots.
// Every pipeline shares one layout: this set + 128 bytes of push constants.
class Bindless {
 public:
  static constexpr uint32_t kSampledImageBinding = 0;
  static constexpr uint32_t kSamplerBinding = 1;
  static constexpr uint32_t kStorageImageBinding = 2;
  static constexpr uint32_t kPushConstantBytes = 128;

  explicit Bindless(VulkanContext& context);
  ~Bindless();
  Bindless(const Bindless&) = delete;
  Bindless& operator=(const Bindless&) = delete;

  uint32_t addSampledImage(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  void setSampledImage(uint32_t index, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  void removeSampledImage(uint32_t index);

  uint32_t addStorageImage(VkImageView view);
  void setStorageImage(uint32_t index, VkImageView view);
  void removeStorageImage(uint32_t index);

  uint32_t addSampler(VkSampler sampler);

  void bind(VkCommandBuffer cmd, VkPipelineBindPoint point) const;
  VkPipelineLayout pipelineLayout() const { return pipelineLayout_; }

  uint32_t sampledImageCapacity() const { return sampledCapacity_; }

 private:
  struct Slots {
    uint32_t next = 0;
    std::vector<uint32_t> free;
    uint32_t capacity = 0;
    uint32_t take();
  };
  void write(uint32_t binding, uint32_t index, VkDescriptorType type, VkImageView view, VkImageLayout layout,
             VkSampler sampler);

  VulkanContext& context_;
  uint32_t sampledCapacity_ = 0;
  VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkDescriptorSet set_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  Slots sampled_, storage_, samplers_;
};

}  // namespace ember
