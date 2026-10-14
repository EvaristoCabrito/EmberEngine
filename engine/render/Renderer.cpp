#include "render/Renderer.h"

#include <stb_image_write.h>

#include <chrono>
#include <cstring>

#include "render/DebugUi.h"
#include "rhi/Pipeline.h"
#include "rhi/Swapchain.h"
#include "rhi/VulkanContext.h"

namespace ember {

namespace {

constexpr VkFormat kSceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat kOutputFormat = Renderer::outputFormat();

struct TrianglePush {
  float time;
  float aspect;
};

struct TonemapPush {
  uint32_t source;
  uint32_t sampler;
  float exposure;
};

struct GroundPush {
  glm::mat4 viewProjection;
  float halfExtent;
};

struct SpritePush {
  glm::mat4 viewProjection;
  glm::vec3 cameraRight;
  uint32_t sampler;
  VkDeviceAddress instances;
};
static_assert(sizeof(SpritePush) <= Bindless::kPushConstantBytes);

constexpr VkShaderStageFlags kPushStages = Bindless::kStages;
constexpr VkFormat kVisibilityFormat = VK_FORMAT_R32_UINT;

// Matches MeshFrame in shaders/scene.hlsli.
struct MeshFrame {
  glm::mat4 viewProjection;
  glm::vec4 frustum[5];
  glm::vec3 cameraPosition;
  float lodScale;
  glm::vec2 screenSize;
  float zNear;
  uint32_t instanceCount;
  uint32_t meshCount;
  uint32_t maxQueue;
  uint32_t maxClusters;
  uint32_t materialSampler;
  uint32_t workBuffer;
  uint64_t meshes;
  uint64_t materials;
  uint64_t instances;
  glm::vec3 sunDirection;
  float sunIntensity;
  glm::vec3 sunColor;
  float ambientIntensity;
  glm::vec3 skyColor;
  uint32_t pad0;
  glm::vec3 groundColor;
  uint32_t pad1;
  glm::mat4 view;
  float projectionX;
  float projectionY;
  uint32_t hizTexture;
  uint32_t hizSampler;
  glm::vec2 hizSize;
  uint32_t instanceVisibility;
  uint32_t pad2;
  glm::mat4 previousView;
  float previousProjectionX;
  float previousProjectionY;
  uint32_t earlyOcclusion;
  uint32_t costSplit;
  uint32_t debugView;
  uint32_t pad4, pad5, pad6;
};
static_assert(sizeof(MeshFrame) == 480 && offsetof(MeshFrame, meshes) == 200 && offsetof(MeshFrame, previousView) == 384);

// Work buffer layout (shaders/scene.hlsli): counters, indirect arguments, two queues, the cluster list.
constexpr VkDeviceSize kWorkHeaderBytes = 128;
constexpr VkDeviceSize kTraverseArgsOffset = 64;
constexpr VkDeviceSize kDrawArgsOffset = 80;
constexpr VkDeviceSize kCounterBytes = 48;

struct HizPush {
  uint32_t source;
  uint32_t sampler;
  uint32_t destination;
  float sourceLevel;
  uint32_t width, height;
};

uint32_t previousPowerOfTwo(uint32_t v) {
  uint32_t r = 1;
  while (r * 2 <= v) r *= 2;
  return r;
}

struct CullPush {
  VkDeviceAddress frame;
  uint32_t pass;
};
struct MeshletPush {
  VkDeviceAddress frame;
  uint32_t pass;
};
struct ResolvePush {
  VkDeviceAddress frame;
  uint32_t visibilityImage;
  uint32_t colorImage;
  glm::vec4 clearColor;
};
static_assert(sizeof(ResolvePush) <= Bindless::kPushConstantBytes);

constexpr glm::vec4 kClearColor{0.012f, 0.014f, 0.02f, 1.0f};

void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                   VkAccessFlags2 dstAccess) {
  VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
  barrier.srcStageMask = srcStage;
  barrier.srcAccessMask = srcAccess;
  barrier.dstStageMask = dstStage;
  barrier.dstAccessMask = dstAccess;
  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency.memoryBarrierCount = 1;
  dependency.pMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(cmd, &dependency);
}

// World-space frustum planes (inside: dot(n, p) + d >= 0) of a reversed-Z infinite projection: left, right,
// top, bottom, near.
void frustumPlanes(const glm::mat4& m, glm::vec4 (&planes)[5]) {
  const auto row = [&](int i) { return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
  planes[0] = row(3) + row(0);
  planes[1] = row(3) - row(0);
  planes[2] = row(3) + row(1);
  planes[3] = row(3) - row(1);
  planes[4] = row(3) - row(2);
  for (glm::vec4& p : planes) p /= glm::length(glm::vec3(p));
}

double millisecondsSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void setViewport(VkCommandBuffer cmd, VkExtent2D size) {
  const VkViewport viewport{0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height), 0.0f, 1.0f};
  const VkRect2D scissor{{0, 0}, size};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

}  // namespace

Renderer::Renderer(VulkanContext& context, Swapchain* swapchain, VkExtent2D size)
    : context_(context), swapchain_(swapchain), bindless_(context), gpuTimer_(context) {
  for (Frame& frame : frames_) {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = context.queueFamily;
    VK_CHECK(vkCreateCommandPool(context.device, &poolInfo, nullptr, &frame.pool));
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = frame.pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(context.device, &allocInfo, &frame.cmd));
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(context.device, &fenceInfo, nullptr, &frame.done));
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &frame.imageAcquired));
    frame.sprites = createHostBuffer(context, sizeof(SpriteInstance) * kMaxSprites, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.meshFrame = createDeviceBuffer(context, sizeof(MeshFrame), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    frame.meshCounters = createReadbackBuffer(context, kCounterBytes);
  }
  meshWork_ = createDeviceBuffer(context, kWorkHeaderBytes + VkDeviceSize{kMaxQueue} * 8 * 2 + VkDeviceSize{kMaxClusters} * (8 + 4),
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  meshWorkIndex_ = bindless_.addStorageBuffer(meshWork_.buffer);
  instanceVisibility_ = createDeviceBuffer(context, VkDeviceSize{kMaxInstances} * 4,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  instanceVisibilityIndex_ = bindless_.addStorageBuffer(instanceVisibility_.buffer);

  VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
  VK_CHECK(vkCreateSampler(context.device, &samplerInfo, nullptr, &linearClamp_));
  linearClampIndex_ = bindless_.addSampler(linearClamp_);
  // Sprites are seen at many sizes: trilinear + anisotropic so frames stay crisp without shimmer.
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = std::min(16.0f, context.gpuProperties.limits.maxSamplerAnisotropy);
  VK_CHECK(vkCreateSampler(context.device, &samplerInfo, nullptr, &spriteSampler_));
  spriteSamplerIndex_ = bindless_.addSampler(spriteSampler_);
  // Materials tile: repeat, trilinear, anisotropic.
  samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VK_CHECK(vkCreateSampler(context.device, &samplerInfo, nullptr, &materialSampler_));
  materialSamplerIndex_ = bindless_.addSampler(materialSampler_);
  // Depth pyramid reads: a bilinear footprint reduced to its minimum (the farthest depth, reversed-Z).
  VkSamplerReductionModeCreateInfo reduction{VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO};
  reduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;
  VkSamplerCreateInfo minInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  minInfo.pNext = &reduction;
  minInfo.magFilter = minInfo.minFilter = VK_FILTER_LINEAR;
  minInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  minInfo.addressModeU = minInfo.addressModeV = minInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  minInfo.maxLod = VK_LOD_CLAMP_NONE;
  VK_CHECK(vkCreateSampler(context.device, &minInfo, nullptr, &minSampler_));
  minSamplerIndex_ = bindless_.addSampler(minSampler_);

  createTargets(size);

  GraphicsPipelineDesc triangle;
  triangle.vertexShader = "triangle.VSMain";
  triangle.fragmentShader = "triangle.PSMain";
  triangle.colorFormats = {kSceneColorFormat};
  triangle.depthFormat = kDepthFormat;
  triangle.depthTest = true;
  triangle.depthWrite = true;
  trianglePipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), triangle);

  GraphicsPipelineDesc tonemap;
  tonemap.vertexShader = "tonemap.VSMain";
  tonemap.fragmentShader = "tonemap.PSMain";
  tonemap.colorFormats = {kOutputFormat};
  tonemapPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), tonemap);

  GraphicsPipelineDesc ground;
  ground.vertexShader = "ground.VSMain";
  ground.fragmentShader = "ground.PSMain";
  ground.colorFormats = {kSceneColorFormat};
  ground.depthFormat = kDepthFormat;
  ground.depthTest = true;
  ground.depthWrite = true;
  groundPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), ground);

  // Sprites: depth-tested against the world, blended premultiplied, drawn back to front.
  GraphicsPipelineDesc sprites;
  sprites.vertexShader = "sprites.VSMain";
  sprites.fragmentShader = "sprites.PSMain";
  sprites.colorFormats = {kSceneColorFormat};
  sprites.depthFormat = kDepthFormat;
  sprites.depthTest = true;
  sprites.depthWrite = false;
  sprites.alphaBlend = true;
  sprites.premultipliedAlpha = true;
  spritePipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), sprites);

  // Mesh passes.
  const VkPipelineLayout layout = bindless_.pipelineLayout();
  cullInstancesPipeline_ = createComputePipeline(context, layout, "cull.CSInstancesEarly");
  cullInstancesLatePipeline_ = createComputePipeline(context, layout, "cull.CSInstancesLate");
  hizPipeline_ = createComputePipeline(context, layout, "hiz.CSDownsample");
  cullPreparePipeline_ = createComputePipeline(context, layout, "cull.CSPrepare");
  cullTraversePipeline_ = createComputePipeline(context, layout, "cull.CSTraverse");
  cullPrepareDrawPipeline_ = createComputePipeline(context, layout, "cull.CSPrepareDraw");
  resolvePipeline_ = createComputePipeline(context, layout, "resolve.CSResolve");
  GraphicsPipelineDesc meshlet;
  meshlet.taskShader = "meshlet.ASMain";
  meshlet.meshShader = "meshlet.MSMain";
  meshlet.fragmentShader = "meshlet.PSMain";
  meshlet.colorFormats = {kVisibilityFormat};
  meshlet.depthFormat = kDepthFormat;
  meshlet.depthTest = true;
  meshlet.depthWrite = true;
  meshletPipeline_ = createGraphicsPipeline(context, layout, meshlet);
}

void Renderer::setScene(const Camera& camera, std::span<const SpriteInstance> sprites) {
  hasScene_ = true;
  camera_ = camera;
  sprites_.assign(sprites.begin(), sprites.begin() + std::min<size_t>(sprites.size(), kMaxSprites));
}

Renderer::~Renderer() {
  vkDeviceWaitIdle(context_.device);
  vkDestroyPipeline(context_.device, trianglePipeline_, nullptr);
  vkDestroyPipeline(context_.device, tonemapPipeline_, nullptr);
  vkDestroyPipeline(context_.device, groundPipeline_, nullptr);
  vkDestroyPipeline(context_.device, spritePipeline_, nullptr);
  for (VkPipeline p : {cullInstancesPipeline_, cullInstancesLatePipeline_, hizPipeline_, cullPreparePipeline_,
                       cullTraversePipeline_, cullPrepareDrawPipeline_, meshletPipeline_, resolvePipeline_})
    vkDestroyPipeline(context_.device, p, nullptr);
  vkDestroySampler(context_.device, minSampler_, nullptr);
  bindless_.removeStorageBuffer(instanceVisibilityIndex_);
  destroyBuffer(context_, instanceVisibility_);
  vkDestroySampler(context_.device, materialSampler_, nullptr);
  bindless_.removeStorageBuffer(meshWorkIndex_);
  destroyBuffer(context_, meshWork_);
  vkDestroySampler(context_.device, spriteSampler_, nullptr);
  destroyTargets();
  vkDestroySampler(context_.device, linearClamp_, nullptr);
  for (Frame& frame : frames_) {
    vkDestroySemaphore(context_.device, frame.imageAcquired, nullptr);
    vkDestroyFence(context_.device, frame.done, nullptr);
    vkDestroyCommandPool(context_.device, frame.pool, nullptr);
    destroyBuffer(context_, frame.sprites);
    destroyBuffer(context_, frame.meshFrame);
    destroyBuffer(context_, frame.meshCounters);
  }
}

void Renderer::createTargets(VkExtent2D size) {
  size_ = size;
  sceneColor_ = createImage(context_, {kSceneColorFormat, size,
                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                           VK_IMAGE_USAGE_STORAGE_BIT});
  depth_ = createImage(context_, {kDepthFormat, size,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
  output_ = createImage(context_, {kOutputFormat, size,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT});
  visibility_ = createImage(context_, {kVisibilityFormat, size, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT});
  // Depth pyramid: power-of-two below the screen size, full mip chain, one storage view per level.
  hizSize_ = {previousPowerOfTwo(size.width), previousPowerOfTwo(size.height)};
  hizValid_ = false;
  uint32_t hizLevels = 1;
  while ((std::max(hizSize_.width, hizSize_.height) >> hizLevels) > 0) ++hizLevels;
  ImageDesc hizDesc{VK_FORMAT_R32_SFLOAT, hizSize_, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT};
  hizDesc.mipLevels = hizLevels;
  hiz_ = createImage(context_, hizDesc);
  for (uint32_t level = 0; level < hizLevels; ++level) {
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = hiz_.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1};
    VkImageView view;
    VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &view));
    hizLevelViews_.push_back(view);
    hizLevelIndices_.push_back(bindless_.addStorageImage(view));
  }
  // Targets keep their bindless slots for the renderer's lifetime; a resize repoints them.
  if (sceneColorIndex_ == kNoIndex) {
    hizIndex_ = bindless_.addSampledImage(hiz_.view, VK_IMAGE_LAYOUT_GENERAL);
    depthIndex_ = bindless_.addSampledImage(depth_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
    sceneColorIndex_ = bindless_.addSampledImage(sceneColor_.view);
    sceneColorStorageIndex_ = bindless_.addStorageImage(sceneColor_.view);
    visibilityIndex_ = bindless_.addStorageImage(visibility_.view);
  } else {
    bindless_.setSampledImage(hizIndex_, hiz_.view, VK_IMAGE_LAYOUT_GENERAL);
    bindless_.setSampledImage(depthIndex_, depth_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
    bindless_.setSampledImage(sceneColorIndex_, sceneColor_.view);
    bindless_.setStorageImage(sceneColorStorageIndex_, sceneColor_.view);
    bindless_.setStorageImage(visibilityIndex_, visibility_.view);
  }
}

void Renderer::destroyTargets() {
  destroyImage(context_, sceneColor_);
  destroyImage(context_, depth_);
  destroyImage(context_, output_);
  destroyImage(context_, visibility_);
  for (size_t i = 0; i < hizLevelViews_.size(); ++i) {
    bindless_.removeStorageImage(hizLevelIndices_[i]);
    vkDestroyImageView(context_.device, hizLevelViews_[i], nullptr);
  }
  hizLevelViews_.clear();
  hizLevelIndices_.clear();
  destroyImage(context_, hiz_);
}

void Renderer::resize(VkExtent2D size) {
  if (size.width == size_.width && size.height == size_.height) return;
  vkDeviceWaitIdle(context_.device);
  destroyTargets();
  createTargets(size);
}

Renderer::Frame& Renderer::beginFrame() {
  Frame& frame = frames_[frameIndex_];
  const auto start = std::chrono::steady_clock::now();
  VK_CHECK(vkWaitForFences(context_.device, 1, &frame.done, VK_TRUE, UINT64_MAX));
  lastWaitMs_ = millisecondsSince(start);
  gpuTimer_.beginFrame(frameIndex_);
  if (frame.countersPending) {
    uint32_t c[12];
    VK_CHECK(vmaInvalidateAllocation(context_.allocator, frame.meshCounters.allocation, 0, VK_WHOLE_SIZE));
    std::memcpy(c, frame.meshCounters.mapped, sizeof(c));
    meshStats_ = {c[5], std::min(c[2], kMaxClusters), c[3], c[4], c[6], c[7], c[8], c[9]};
    frame.countersPending = false;
  }
  return frame;
}

bool Renderer::renderFrame(float time) {
  Frame& frame = beginFrame();

  uint32_t imageIndex = 0;
  VkImage swapImage = VK_NULL_HANDLE;
  if (swapchain_) {
    const auto acquireStart = std::chrono::steady_clock::now();
    const VkResult acquired = vkAcquireNextImageKHR(context_.device, swapchain_->handle(), UINT64_MAX,
                                                    frame.imageAcquired, VK_NULL_HANDLE, &imageIndex);
    lastWaitMs_ += millisecondsSince(acquireStart);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) return false;
    if (acquired != VK_SUBOPTIMAL_KHR) VK_CHECK(acquired);
    swapImage = swapchain_->image(imageIndex);
  }

  VK_CHECK(vkResetFences(context_.device, 1, &frame.done));
  VK_CHECK(vkResetCommandPool(context_.device, frame.pool, 0));
  record(frame, time, swapImage, nullptr);

  VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  cmdInfo.commandBuffer = frame.cmd;
  std::array<VkSemaphoreSubmitInfo, 2> waits{};
  VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &cmdInfo;
  submit.waitSemaphoreInfoCount = gatherWaits(frame, swapchain_ != nullptr, waits);
  submit.pWaitSemaphoreInfos = waits.data();
  if (swapchain_) {
    signal.semaphore = swapchain_->renderFinished(imageIndex);
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal;
  }
  VK_CHECK(vkQueueSubmit2(context_.queue, 1, &submit, frame.done));
  frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;

  if (!swapchain_) return true;
  VkSemaphore finished = swapchain_->renderFinished(imageIndex);
  VkSwapchainKHR handle = swapchain_->handle();
  VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &finished;
  present.swapchainCount = 1;
  present.pSwapchains = &handle;
  present.pImageIndices = &imageIndex;
  const VkResult presented = vkQueuePresentKHR(context_.queue, &present);
  if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) return false;
  VK_CHECK(presented);
  return true;
}

void Renderer::captureFrame(float time, const std::string& path) {
  Frame& frame = beginFrame();
  VK_CHECK(vkResetFences(context_.device, 1, &frame.done));
  VK_CHECK(vkResetCommandPool(context_.device, frame.pool, 0));
  Buffer readback = createReadbackBuffer(context_, VkDeviceSize{size_.width} * size_.height * 4);
  record(frame, time, VK_NULL_HANDLE, &readback);

  VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  cmdInfo.commandBuffer = frame.cmd;
  std::array<VkSemaphoreSubmitInfo, 2> waits{};
  VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &cmdInfo;
  submit.waitSemaphoreInfoCount = gatherWaits(frame, false, waits);
  submit.pWaitSemaphoreInfos = waits.data();
  VK_CHECK(vkQueueSubmit2(context_.queue, 1, &submit, frame.done));
  VK_CHECK(vkWaitForFences(context_.device, 1, &frame.done, VK_TRUE, UINT64_MAX));
  frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;

  VK_CHECK(vmaInvalidateAllocation(context_.allocator, readback.allocation, 0, VK_WHOLE_SIZE));
  const int ok = stbi_write_png(path.c_str(), static_cast<int>(size_.width), static_cast<int>(size_.height), 4,
                                readback.mapped, static_cast<int>(size_.width * 4));
  destroyBuffer(context_, readback);
  EMBER_CHECK(ok, std::format("Could not write {}", path));
  logInfo("captured {}x{} -> {}", size_.width, size_.height, path);
}

void Renderer::recordMeshes(Frame& frame, VkCommandBuffer cmd) {
  // Frame constants and cleared counters.
  MeshFrame f{};
  f.viewProjection = camera_.viewProjection();
  frustumPlanes(f.viewProjection, f.frustum);
  f.cameraPosition = camera_.position;
  // A cluster is detailed enough once its simplification error is under lodPixelError_ pixels (1 by default).
  f.lodScale = std::abs(camera_.projection[1][1]) * 0.5f * static_cast<float>(size_.height) / lodPixelError_;
  f.screenSize = {static_cast<float>(size_.width), static_cast<float>(size_.height)};
  f.zNear = camera_.projection[3][2];
  f.instanceCount = std::min(meshView_.instanceCount, kMaxInstances);
  f.meshCount = meshView_.meshCount;
  f.maxQueue = kMaxQueue;
  f.maxClusters = kMaxClusters;
  f.materialSampler = materialSamplerIndex_;
  f.workBuffer = meshWorkIndex_;
  f.meshes = meshView_.meshes;
  f.materials = meshView_.materials;
  f.instances = meshView_.instances;
  f.sunDirection = glm::normalize(lighting_.sunDirection);
  f.sunIntensity = lighting_.sunIntensity;
  f.sunColor = lighting_.sunColor;
  f.ambientIntensity = lighting_.ambientIntensity;
  f.skyColor = lighting_.skyColor;
  f.groundColor = lighting_.groundColor;
  f.view = camera_.view;
  f.projectionX = camera_.projection[0][0];
  f.projectionY = std::abs(camera_.projection[1][1]);
  f.hizTexture = hizIndex_;
  f.hizSampler = minSamplerIndex_;
  f.hizSize = {static_cast<float>(hizSize_.width), static_cast<float>(hizSize_.height)};
  f.instanceVisibility = instanceVisibilityIndex_;
  f.previousView = previousCamera_.view;
  f.previousProjectionX = previousCamera_.projection[0][0];
  f.previousProjectionY = std::abs(previousCamera_.projection[1][1]);
  f.earlyOcclusion = hizValid_ ? 1u : 0u;
  f.costSplit = costSplit_;
  f.debugView = debugView_;
  previousCamera_ = camera_;
  // The previous frame's passes may still read these buffers.
  memoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
  vkCmdUpdateBuffer(cmd, frame.meshFrame.buffer, 0, sizeof(f), &f);
  vkCmdFillBuffer(cmd, meshWork_.buffer, 0, kWorkHeaderBytes, 0);
  // A new instance list: last frame's verdicts belong to other instances; everything starts unknown
  // (not visible), so the late pass tests it all against the pyramid.
  if (meshView_.instancesVersion != visibilityVersion_) {
    vkCmdFillBuffer(cmd, instanceVisibility_.buffer, 0, VK_WHOLE_SIZE, 0);
    visibilityVersion_ = meshView_.instancesVersion;
  }
  memoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT |
                    VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

  const VkDeviceAddress frameAddress = bufferAddress(context_, frame.meshFrame);
  const auto computeToCompute = [&] {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
  };
  // Instances (early or late), then the BVH traversal one level per pass, then the draw arguments.
  const auto cull = [&](VkPipeline instances, uint32_t drawPass) {
    CullPush push{frameAddress, 0};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, instances);
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (f.instanceCount + 63) / 64, 1, 1);
    for (uint32_t pass = 0; pass < MeshScene::kMaxTraversalDepth; ++pass) {
      push.pass = pass;
      computeToCompute();
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPreparePipeline_);
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
      vkCmdDispatch(cmd, 1, 1, 1);
      computeToCompute();
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullTraversePipeline_);
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
      vkCmdDispatchIndirect(cmd, meshWork_.buffer, kTraverseArgsOffset);
    }
    computeToCompute();
    push.pass = drawPass;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPrepareDrawPipeline_);
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, 1, 1, 1);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT |
                      VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                      VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
  };

  // --- Early: what was visible last frame ---------------------------------------------------
  uint32_t scope = gpuTimer_.begin(cmd, "cull early");
  cull(cullInstancesPipeline_, 0);
  gpuTimer_.end(cmd, scope);
  transition(cmd, visibility_.image, visibility_.aspect,
             {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
              VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
  transition(cmd, depth_.image, depth_.aspect,
             {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
  scope = gpuTimer_.begin(cmd, "draw early");
  recordVisibilityPass(cmd, frameAddress, 0);
  gpuTimer_.end(cmd, scope);

  // --- Depth pyramid of what the early pass drew -------------------------------------------------
  scope = gpuTimer_.begin(cmd, "depth pyramid");
  recordDepthPyramid(cmd);
  hizValid_ = true;
  gpuTimer_.end(cmd, scope);

  // --- Late: everything else, tested against the pyramid ------------------------------------
  memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT);
  vkCmdFillBuffer(cmd, meshWork_.buffer, 0, 8, 0);  // both traversal queues empty again
  memoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  scope = gpuTimer_.begin(cmd, "cull late");
  cull(cullInstancesLatePipeline_, 1);
  gpuTimer_.end(cmd, scope);
  // Counters back to the CPU for meshStats() once this frame's fence signals.
  const VkBufferCopy counters{0, 0, kCounterBytes};
  vkCmdCopyBuffer(cmd, meshWork_.buffer, frame.meshCounters.buffer, 1, &counters);
  frame.countersPending = true;
  transition(cmd, depth_.image, depth_.aspect,
             {VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
  memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
  scope = gpuTimer_.begin(cmd, "draw late");
  recordVisibilityPass(cmd, frameAddress, 1);
  gpuTimer_.end(cmd, scope);

  // The pyramid again, from the complete depth: next frame's early pass tests clusters against it. Built
  // from the early pass alone it misses what the late pass drew, and frames alternate between culling
  // too little and too much (measured: 15.8 / 5.6 ms, frame after frame, with a still camera).
  scope = gpuTimer_.begin(cmd, "depth pyramid (next frame)");
  recordDepthPyramid(cmd);
  gpuTimer_.end(cmd, scope);
  transition(cmd, depth_.image, depth_.aspect,
             {VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});

  // --- Resolve: materials shaded into the HDR scene colour -----------------------------------
  transition(cmd, visibility_.image, visibility_.aspect,
             {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT});
  // The previous frame may still be sampling scene colour in its tonemap pass.
  transition(cmd, sceneColor_.image, sceneColor_.aspect,
             {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
  scope = gpuTimer_.begin(cmd, "resolve");
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, resolvePipeline_);
  const ResolvePush resolvePush{frameAddress, visibilityIndex_, sceneColorStorageIndex_, kClearColor};
  vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(resolvePush), &resolvePush);
  vkCmdDispatch(cmd, (size_.width + 7) / 8, (size_.height + 7) / 8, 1);
  gpuTimer_.end(cmd, scope);
}

void Renderer::recordVisibilityPass(VkCommandBuffer cmd, VkDeviceAddress frameAddress, uint32_t pass) {
  const bool clear = pass == 0;
  VkRenderingAttachmentInfo vis{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  vis.imageView = visibility_.view;
  vis.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  vis.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  vis.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  vis.clearValue.color.uint32[0] = 0xFFFFFFFFu;  // empty
  VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  depth.imageView = depth_.view;
  depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth.clearValue.depthStencil = {0.0f, 0};
  VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
  rendering.renderArea = {{0, 0}, size_};
  rendering.layerCount = 1;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachments = &vis;
  rendering.pDepthAttachment = &depth;
  vkCmdBeginRendering(cmd, &rendering);
  setViewport(cmd, size_);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshletPipeline_);
  const MeshletPush push{frameAddress, pass};
  vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
  vkCmdDrawMeshTasksIndirectEXT(cmd, meshWork_.buffer, kDrawArgsOffset, 1, 12);
  vkCmdEndRendering(cmd);
}

void Renderer::recordDepthPyramid(VkCommandBuffer cmd) {
  transition(cmd, depth_.image, depth_.aspect,
             {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
  // Last frame's late culling sampled the pyramid; its contents are rebuilt from scratch.
  transition(cmd, hiz_.image, hiz_.aspect,
             {hizValid_ ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
              VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hizPipeline_);
  for (uint32_t level = 0; level < hizLevelIndices_.size(); ++level) {
    const uint32_t w = std::max(1u, hizSize_.width >> level), h = std::max(1u, hizSize_.height >> level);
    const HizPush push{level == 0 ? depthIndex_ : hizIndex_, minSamplerIndex_, hizLevelIndices_[level],
                       level == 0 ? 0.0f : static_cast<float>(level - 1), w, h};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (w + 7) / 8, (h + 7) / 8, 1);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  }
}

uint32_t Renderer::gatherWaits(Frame& frame, bool swapImage, std::array<VkSemaphoreSubmitInfo, 2>& waits) const {
  uint32_t count = 0;
  if (swapImage) {
    waits[count] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waits[count].semaphore = frame.imageAcquired;
    waits[count].stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    ++count;
  }
  if (waitTimeline_ && waitValue_) {
    // Already signalled when the textures were published: makes the copies visible, costs nothing.
    waits[count] = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waits[count].semaphore = waitTimeline_;
    waits[count].value = waitValue_;
    waits[count].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;  // textures and geometry, read by every stage
    ++count;
  }
  return count;
}

void Renderer::record(Frame& frame, float time, VkImage swapImage, const Buffer* readback) {
  VkCommandBuffer cmd = frame.cmd;
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
  bindless_.bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bindless_.bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
  const uint32_t frameScope = gpuTimer_.begin(cmd, "frame");

  // --- 3D meshes: cull, pick cluster LODs, rasterise visibility + depth, resolve materials -----
  const bool meshes = hasScene_ && static_cast<bool>(meshView_);
  if (meshes) {
    recordMeshes(frame, cmd);
    transition(cmd, sceneColor_.image, sceneColor_.aspect,
               {VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
  } else {
    // --- Scene pass: HDR colour + reversed-Z depth --------------------------------------------
    // The previous frame may still be sampling scene colour in its tonemap pass.
    transition(cmd, sceneColor_.image, sceneColor_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
    transition(cmd, depth_.image, depth_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
  }

  VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  color.imageView = sceneColor_.view;
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = meshes ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;  // keep the resolved 3D
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.clearValue.color = {{kClearColor.r, kClearColor.g, kClearColor.b, kClearColor.a}};
  VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  depth.imageView = depth_.view;
  depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth.loadOp = meshes ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;  // 3D depth occludes the cards
  depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth.clearValue.depthStencil = {0.0f, 0};  // reversed-Z: far plane is 0
  VkRenderingInfo scene{VK_STRUCTURE_TYPE_RENDERING_INFO};
  scene.renderArea = {{0, 0}, size_};
  scene.layerCount = 1;
  scene.colorAttachmentCount = 1;
  scene.pColorAttachments = &color;
  scene.pDepthAttachment = &depth;
  const uint32_t sceneScope = gpuTimer_.begin(cmd, "scene");
  vkCmdBeginRendering(cmd, &scene);
  setViewport(cmd, size_);
  if (hasScene_) {
    const glm::mat4 viewProjection = camera_.viewProjection();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, groundPipeline_);
    const GroundPush groundPush{viewProjection, groundHalfExtent_};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(groundPush), &groundPush);
    vkCmdDraw(cmd, 6, 1, 0, 0);
    if (!sprites_.empty()) {
      std::memcpy(frame.sprites.mapped, sprites_.data(), sprites_.size() * sizeof(SpriteInstance));
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipeline_);
      const SpritePush spritePush{viewProjection, camera_.right, spriteSamplerIndex_, bufferAddress(context_, frame.sprites)};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(spritePush), &spritePush);
      vkCmdDraw(cmd, 6, static_cast<uint32_t>(sprites_.size()), 0, 0);
    }
  } else {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline_);
    const TrianglePush trianglePush{time, static_cast<float>(size_.width) / static_cast<float>(size_.height)};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(trianglePush), &trianglePush);
    vkCmdDraw(cmd, 3, 1, 0, 0);
  }
  vkCmdEndRendering(cmd);
  gpuTimer_.end(cmd, sceneScope);

  // --- Tonemap pass: HDR -> display ---------------------------------------------------------
  transition(cmd, sceneColor_.image, sceneColor_.aspect,
             {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
  // The previous frame may still be copying the output image to the window.
  transition(cmd, output_.image, output_.aspect,
             {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_NONE,
              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
  VkRenderingAttachmentInfo out{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  out.imageView = output_.view;
  out.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  out.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  out.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo display{VK_STRUCTURE_TYPE_RENDERING_INFO};
  display.renderArea = {{0, 0}, size_};
  display.layerCount = 1;
  display.colorAttachmentCount = 1;
  display.pColorAttachments = &out;
  const uint32_t tonemapScope = gpuTimer_.begin(cmd, "tonemap");
  vkCmdBeginRendering(cmd, &display);
  setViewport(cmd, size_);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipeline_);
  const TonemapPush tonemapPush{sceneColorIndex_, linearClampIndex_, debugView_ != 0 ? -1.0f : 1.0f};
  vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0,
                     sizeof(tonemapPush), &tonemapPush);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
  gpuTimer_.end(cmd, tonemapScope);

  // --- UI on top -----------------------------------------------------------------------------
  if (ui_) {
    const uint32_t uiScope = gpuTimer_.begin(cmd, "ui");
    ui_->record(cmd, output_.view, size_);
    gpuTimer_.end(cmd, uiScope);
  }

  transition(cmd, output_.image, output_.aspect,
             {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT});

  // --- Copy to the window -------------------------------------------------------------------
  if (swapImage) {
    transition(cmd, swapImage, VK_IMAGE_ASPECT_COLOR_BIT,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});
    const VkExtent2D target = swapchain_->extent();
    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[1] = {static_cast<int32_t>(size_.width), static_cast<int32_t>(size_.height), 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffsets[1] = {static_cast<int32_t>(target.width), static_cast<int32_t>(target.height), 1};
    vkCmdBlitImage(cmd, output_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);
    transition(cmd, swapImage, VK_IMAGE_ASPECT_COLOR_BIT,
               {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE});
  }

  // --- Copy to the CPU (frame capture) ------------------------------------------------------
  if (readback) {
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {size_.width, size_.height, 1};
    vkCmdCopyImageToBuffer(cmd, output_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback->buffer, 1, &copy);
    VkMemoryBarrier2 toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    toHost.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    toHost.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toHost.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    toHost.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &toHost;
    vkCmdPipelineBarrier2(cmd, &dependency);
  }

  gpuTimer_.end(cmd, frameScope);
  VK_CHECK(vkEndCommandBuffer(cmd));
}

}  // namespace ember
