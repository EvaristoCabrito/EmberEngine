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

constexpr VkShaderStageFlags kPushStages = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;

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
  }

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
  vkDestroySampler(context_.device, spriteSampler_, nullptr);
  destroyTargets();
  vkDestroySampler(context_.device, linearClamp_, nullptr);
  for (Frame& frame : frames_) {
    vkDestroySemaphore(context_.device, frame.imageAcquired, nullptr);
    vkDestroyFence(context_.device, frame.done, nullptr);
    vkDestroyCommandPool(context_.device, frame.pool, nullptr);
    destroyBuffer(context_, frame.sprites);
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
  // The scene colour keeps one bindless slot for the renderer's lifetime; a resize repoints it.
  if (sceneColorIndex_ == kNoIndex) sceneColorIndex_ = bindless_.addSampledImage(sceneColor_.view);
  else bindless_.setSampledImage(sceneColorIndex_, sceneColor_.view);
}

void Renderer::destroyTargets() {
  destroyImage(context_, sceneColor_);
  destroyImage(context_, depth_);
  destroyImage(context_, output_);
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
    waits[count].stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
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
  const uint32_t frameScope = gpuTimer_.begin(cmd, "frame");

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

  VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  color.imageView = sceneColor_.view;
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.clearValue.color = {{0.012f, 0.014f, 0.02f, 1.0f}};
  VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  depth.imageView = depth_.view;
  depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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
    const GroundPush groundPush{viewProjection, 80.0f};
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
  const TonemapPush tonemapPush{sceneColorIndex_, linearClampIndex_, 1.0f};
  vkCmdPushConstants(cmd, bindless_.pipelineLayout(), VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0,
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
