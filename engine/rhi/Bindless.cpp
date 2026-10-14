#include "rhi/Bindless.h"

#include <algorithm>
#include <array>

#include "rhi/VulkanContext.h"

namespace ember {

namespace {
constexpr uint32_t kWantedSampledImages = 1u << 18;  // 262,144 textures
constexpr uint32_t kWantedStorageImages = 1u << 14;
constexpr uint32_t kWantedSamplers = 256;
constexpr uint32_t kWantedStorageBuffers = 1u << 12;
}  // namespace

uint32_t Bindless::Slots::take() {
  if (!free.empty()) {
    const uint32_t index = free.back();
    free.pop_back();
    return index;
  }
  EMBER_CHECK(next < capacity, "Bindless descriptor table is full");
  return next++;
}

Bindless::Bindless(VulkanContext& context) : context_(context) {
  VkPhysicalDeviceVulkan12Properties v12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
  VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  props.pNext = &v12;
  vkGetPhysicalDeviceProperties2(context.gpu, &props);

  sampled_.capacity = sampledCapacity_ =
      std::min({kWantedSampledImages, v12.maxDescriptorSetUpdateAfterBindSampledImages,
                v12.maxPerStageDescriptorUpdateAfterBindSampledImages});
  storage_.capacity = std::min({kWantedStorageImages, v12.maxDescriptorSetUpdateAfterBindStorageImages,
                                v12.maxPerStageDescriptorUpdateAfterBindStorageImages});
  samplers_.capacity = std::min({kWantedSamplers, v12.maxDescriptorSetUpdateAfterBindSamplers,
                                 v12.maxPerStageDescriptorUpdateAfterBindSamplers});
  buffers_.capacity = std::min({kWantedStorageBuffers, v12.maxDescriptorSetUpdateAfterBindStorageBuffers,
                                v12.maxPerStageDescriptorUpdateAfterBindStorageBuffers});
  logInfo("bindless: {} textures, {} storage images, {} samplers, {} storage buffers", sampled_.capacity, storage_.capacity,
          samplers_.capacity, buffers_.capacity);

  const std::array<VkDescriptorSetLayoutBinding, 4> bindings{{
      {kSampledImageBinding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampled_.capacity, kStages, nullptr},
      {kSamplerBinding, VK_DESCRIPTOR_TYPE_SAMPLER, samplers_.capacity, kStages, nullptr},
      {kStorageImageBinding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storage_.capacity, kStages, nullptr},
      {kStorageBufferBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers_.capacity, kStages, nullptr},
  }};
  const VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                         VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                         VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
  const std::array<VkDescriptorBindingFlags, 4> bindingFlags{flags, flags, flags, flags};
  VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
  flagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
  flagsInfo.pBindingFlags = bindingFlags.data();
  VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.pNext = &flagsInfo;
  layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();
  VK_CHECK(vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &setLayout_));

  const std::array<VkDescriptorPoolSize, 4> sizes{{
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampled_.capacity},
      {VK_DESCRIPTOR_TYPE_SAMPLER, samplers_.capacity},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storage_.capacity},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers_.capacity},
  }};
  VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
  poolInfo.pPoolSizes = sizes.data();
  VK_CHECK(vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &pool_));

  VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfo.descriptorPool = pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &setLayout_;
  VK_CHECK(vkAllocateDescriptorSets(context.device, &allocInfo, &set_));

  const VkPushConstantRange push{kStages, 0, kPushConstantBytes};
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &setLayout_;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &push;
  VK_CHECK(vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout_));
}

Bindless::~Bindless() {
  vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
  vkDestroyDescriptorPool(context_.device, pool_, nullptr);
  vkDestroyDescriptorSetLayout(context_.device, setLayout_, nullptr);
}

void Bindless::write(uint32_t binding, uint32_t index, VkDescriptorType type, VkImageView view,
                     VkImageLayout layout, VkSampler sampler) {
  VkDescriptorImageInfo image{sampler, view, layout};
  VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet = set_;
  write.dstBinding = binding;
  write.dstArrayElement = index;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pImageInfo = &image;
  vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
}

uint32_t Bindless::addSampledImage(VkImageView view, VkImageLayout layout) {
  const uint32_t index = sampled_.take();
  setSampledImage(index, view, layout);
  return index;
}

void Bindless::setSampledImage(uint32_t index, VkImageView view, VkImageLayout layout) {
  write(kSampledImageBinding, index, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, view, layout, VK_NULL_HANDLE);
}

void Bindless::removeSampledImage(uint32_t index) { sampled_.free.push_back(index); }

uint32_t Bindless::addStorageImage(VkImageView view) {
  const uint32_t index = storage_.take();
  setStorageImage(index, view);
  return index;
}

void Bindless::setStorageImage(uint32_t index, VkImageView view) {
  write(kStorageImageBinding, index, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, view, VK_IMAGE_LAYOUT_GENERAL,
        VK_NULL_HANDLE);
}

void Bindless::removeStorageImage(uint32_t index) { storage_.free.push_back(index); }

uint32_t Bindless::addSampler(VkSampler sampler) {
  const uint32_t index = samplers_.take();
  write(kSamplerBinding, index, VK_DESCRIPTOR_TYPE_SAMPLER, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, sampler);
  return index;
}

uint32_t Bindless::addStorageBuffer(VkBuffer buffer, VkDeviceSize size) {
  const uint32_t index = buffers_.take();
  VkDescriptorBufferInfo info{buffer, 0, size};
  VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet = set_;
  write.dstBinding = kStorageBufferBinding;
  write.dstArrayElement = index;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  write.pBufferInfo = &info;
  vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
  return index;
}

void Bindless::removeStorageBuffer(uint32_t index) { buffers_.free.push_back(index); }

void Bindless::bind(VkCommandBuffer cmd, VkPipelineBindPoint point) const {
  vkCmdBindDescriptorSets(cmd, point, pipelineLayout_, 0, 1, &set_, 0, nullptr);
}

}  // namespace ember
