#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>

#include "render/Camera.h"
#include "render/MeshScene.h"
#include "render/SpriteDraw.h"
#include "rhi/Bindless.h"
#include "rhi/GpuTimer.h"
#include "rhi/Image.h"
#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;
class Swapchain;
class DebugUi;

// Frame renderer. Everything is drawn into the renderer's own HDR targets (scene colour in
// half-float, reversed-Z depth); a final pass tonemaps into an 8-bit output image that is copied
// to the window, or read back to disk for captures.
class Renderer {
 public:
  // `swapchain` may be null: the renderer then works off-screen (tools, automated captures).
  Renderer(VulkanContext& context, Swapchain* swapchain, VkExtent2D size);
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void resize(VkExtent2D size);

  // Draws and presents one frame. False when the swapchain is out of date (recreate, then resize).
  bool renderFrame(float time);

  // Draws one frame and saves it as a PNG (waits for the GPU; never presents).
  void captureFrame(float time, const std::string& path);

  // UI drawn on top of every frame (null = none). Must outlive the renderer or be cleared first.
  void setUi(DebugUi* ui) { ui_ = ui; }
  static constexpr VkFormat outputFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }

  // The 3D scene for the next frame: camera and sprite cards (drawn in the given order, so pass them
  // back to front). Without a scene the renderer draws its bring-up triangle.
  void setScene(const Camera& camera, std::span<const SpriteInstance> sprites);
  // 3D meshes for the next frame (drawn GPU-driven: culling, cluster LOD, visibility buffer). An empty
  // view draws none.
  struct Lighting {  // provisional: one sun and a sky/ground ambient until the lighting model (D1)
    glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.45f, 0.75f, 0.5f));  // towards the sun
    glm::vec3 sunColor{1.0f, 0.95f, 0.88f};
    float sunIntensity = 3.0f;
    glm::vec3 skyColor{0.45f, 0.55f, 0.7f};
    glm::vec3 groundColor{0.25f, 0.22f, 0.18f};
    float ambientIntensity = 1.0f;
  };
  void setMeshes(const MeshSceneView& view, const Lighting& lighting = {}) {
    meshView_ = view;
    lighting_ = lighting;
  }
  // What the GPU did with the meshes, read back from a frame that finished (a few frames old).
  struct MeshStats {
    uint32_t visibleInstances = 0, clusters = 0, queueOverflow = 0, clusterOverflow = 0;
    uint32_t earlyClusters = 0, occludedInstances = 0;
  };
  MeshStats meshStats() const { return meshStats_; }

  // The next frame's GPU work waits for `value` on `timeline` (texture uploads it samples).
  void waitFor(VkSemaphore timeline, uint64_t value) {
    waitTimeline_ = timeline;
    waitValue_ = value;
  }

  // Half the side of the square ground plane under the scene (metres, centred on the origin).
  void setGroundExtent(float halfExtent) { groundHalfExtent_ = halfExtent; }

  Bindless& bindless() { return bindless_; }
  static constexpr uint32_t kMaxSprites = 1u << 16;

  const GpuTimer& gpuTimer() const { return gpuTimer_; }
  // Time the last frame's CPU spent blocked on the GPU (fence) and on the window (acquire).
  double lastWaitMilliseconds() const { return lastWaitMs_; }

 private:
  struct Frame {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence done = VK_NULL_HANDLE;
    VkSemaphore imageAcquired = VK_NULL_HANDLE;
    Buffer sprites;  // this frame's SpriteInstance[] (host-visible, read by device address)
    Buffer meshFrame;     // MeshFrame for the mesh passes (device-local, written by vkCmdUpdateBuffer)
    Buffer meshCounters;  // the work buffer's counters, copied back for meshStats()
    bool countersPending = false;
  };

  void createTargets(VkExtent2D size);
  void recordMeshes(Frame& frame, VkCommandBuffer cmd);
  void recordDepthPyramid(VkCommandBuffer cmd);
  void recordVisibilityPass(VkCommandBuffer cmd, VkDeviceAddress frameAddress, uint32_t pass);
  void destroyTargets();
  Frame& beginFrame();
  void record(Frame& frame, float time, VkImage swapImage, const Buffer* readback);
  // Waits for the frame: the swapchain image (if any) and the texture-upload timeline (if set).
  uint32_t gatherWaits(Frame& frame, bool swapImage, std::array<VkSemaphoreSubmitInfo, 2>& waits) const;

  VulkanContext& context_;
  Swapchain* swapchain_;
  Bindless bindless_;
  GpuTimer gpuTimer_;
  DebugUi* ui_ = nullptr;
  double lastWaitMs_ = 0.0;
  std::array<Frame, kFramesInFlight> frames_{};
  uint32_t frameIndex_ = 0;

  static constexpr uint32_t kNoIndex = UINT32_MAX;

  VkExtent2D size_{};
  Image sceneColor_, depth_, output_;
  uint32_t sceneColorIndex_ = kNoIndex;
  VkSampler linearClamp_ = VK_NULL_HANDLE;
  uint32_t linearClampIndex_ = 0;

  VkPipeline trianglePipeline_ = VK_NULL_HANDLE;
  VkPipeline tonemapPipeline_ = VK_NULL_HANDLE;
  VkPipeline groundPipeline_ = VK_NULL_HANDLE;
  VkPipeline spritePipeline_ = VK_NULL_HANDLE;
  VkSampler spriteSampler_ = VK_NULL_HANDLE;
  uint32_t spriteSamplerIndex_ = 0;

  // Mesh passes.
  static constexpr uint32_t kMaxQueue = 1u << 21;     // traversal items per queue (instance, BVH node)
  static constexpr uint32_t kMaxClusters = 1u << 22;  // clusters drawn per frame
  Image visibility_;
  uint32_t visibilityIndex_ = kNoIndex, sceneColorStorageIndex_ = kNoIndex;
  Buffer meshWork_;
  uint32_t meshWorkIndex_ = kNoIndex;
  VkSampler materialSampler_ = VK_NULL_HANDLE;
  uint32_t materialSamplerIndex_ = 0;
  // Occlusion culling: depth pyramid of the early pass, and each instance's verdict from last frame.
  static constexpr uint32_t kMaxInstances = 1u << 18;  // = MeshScene::Capacity::instances
  Image hiz_;
  std::vector<VkImageView> hizLevelViews_;
  std::vector<uint32_t> hizLevelIndices_;
  uint32_t hizIndex_ = kNoIndex, depthIndex_ = kNoIndex;
  VkExtent2D hizSize_{};
  VkSampler minSampler_ = VK_NULL_HANDLE;
  uint32_t minSamplerIndex_ = 0;
  Buffer instanceVisibility_;
  uint32_t instanceVisibilityIndex_ = kNoIndex;
  uint64_t visibilityVersion_ = ~0ull;
  VkPipeline cullInstancesLatePipeline_ = VK_NULL_HANDLE, hizPipeline_ = VK_NULL_HANDLE;
  VkPipeline cullInstancesPipeline_ = VK_NULL_HANDLE, cullPreparePipeline_ = VK_NULL_HANDLE,
             cullTraversePipeline_ = VK_NULL_HANDLE, cullPrepareDrawPipeline_ = VK_NULL_HANDLE,
             meshletPipeline_ = VK_NULL_HANDLE, resolvePipeline_ = VK_NULL_HANDLE;
  MeshSceneView meshView_;
  Lighting lighting_;
  MeshStats meshStats_;

  float groundHalfExtent_ = 80.0f;
  bool hasScene_ = false;
  Camera camera_;
  std::vector<SpriteInstance> sprites_;
  VkSemaphore waitTimeline_ = VK_NULL_HANDLE;
  uint64_t waitValue_ = 0;
};

}  // namespace ember
