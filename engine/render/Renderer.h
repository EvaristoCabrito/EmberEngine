#pragma once

#include <array>
#include <cmath>
#include <span>
#include <string>
#include <vector>

#include "render/BoardGround.h"
#include "render/FogLooks.h"
#include "render/Camera.h"
#include "render/MeshScene.h"
#include "render/SpriteDraw.h"
#include "rhi/Bindless.h"
#include "rhi/GpuTimer.h"
#include "rhi/Image.h"
#include "rhi/VkCommon.h"
#include "world/FogOfWar.h"

namespace ember {

class VulkanContext;

// One local light (Engine2's CarriedLight: a three.js point light). Matches LocalLight in shaders/scene.hlsli.
struct LocalLight {
  glm::vec3 position;
  float range;     // influence ends here
  glm::vec3 color; // linear colour x intensity
  float decay;     // distance falloff exponent (Engine2: 1.25)
};
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
  // One sun and a sky/ground hemisphere until the sky exists (ARCHITECTURE.md §7.1b). Defaults are the
  // floor: Engine2's diorama prototype (sun #fff0d4 x3 from (-30, 42, 20), hemisphere #d4e6ff / #5d4b33 x1).
  // Colours are linear; srgb() converts authored hex colours.
  static glm::vec3 srgb(uint32_t hex) {
    const auto c = [](uint32_t v) {
      const float x = static_cast<float>(v) / 255.0f;
      return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
    };
    return {c((hex >> 16) & 255), c((hex >> 8) & 255), c(hex & 255)};
  }
  struct Lighting {
    glm::vec3 sunDirection = glm::normalize(glm::vec3(-30.0f, 42.0f, 20.0f));  // towards the sun
    glm::vec3 sunColor = srgb(0xfff0d4);
    float sunIntensity = 3.0f;
    glm::vec3 skyColor = srgb(0xd4e6ff);
    glm::vec3 groundColor = srgb(0x5d4b33);
    float ambientIntensity = 1.0f;
    float exposure = 1.0f;  // multiplied into every light
  };
  void setMeshes(const MeshSceneView& view, const Lighting& lighting = {}) {
    meshView_ = view;
    lighting_ = lighting;
  }
  // What the GPU did with the meshes, read back from a frame that finished (a few frames old).
  struct MeshStats {
    uint32_t visibleInstances = 0, clusters = 0, queueOverflow = 0, clusterOverflow = 0;
    uint32_t earlyClusters = 0, occludedInstances = 0, retestedClusters = 0, triangles = 0;
  };
  MeshStats meshStats() const { return meshStats_; }
  // Measurement only: 0 normal, 1 task shaders only, 2 mesh shaders without rasterisation.
  void setCostSplit(uint32_t mode) { costSplit_ = mode; }
  // Measurement only: 0 shaded, 1 geometric normals, 2 shading normals (into the scene colour, background black).
  void setDebugView(uint32_t view) { debugView_ = view; }
  // LOD threshold in pixels (1 = the engine's rule; ~0 forces full detail for reference images).
  void setLodPixelError(float pixels) { lodPixelError_ = pixels; }

  // The next frame's GPU work waits for `value` on `timeline` (texture uploads it samples).
  void waitFor(VkSemaphore timeline, uint64_t value) {
    waitTimeline_ = timeline;
    waitValue_ = value;
  }

  // The board's ground (BoardGround::view); replaces the placeholder ground plane. Empty = none.
  void setTerrain(const TerrainView& terrain) { terrain_ = terrain; }
  // The board's water (BoardGround::water); empty = none.
  void setWater(const WaterView& water) { water_ = water; }
  // Local lights for the next frame (at most kMaxLights; the rest are dropped).
  void setLights(std::span<const LocalLight> lights) { lights_.assign(lights.begin(), lights.begin() + std::min<size_t>(lights.size(), kMaxLights)); }
  static constexpr uint32_t kMaxLights = 1024;
  // The sun's shadow map covers this sphere (world space); radius 0 = no sun shadows.
  void setSunShadowBounds(glm::vec3 center, float radius) {
    shadowCenter_ = center;
    shadowRadius_ = radius;
  }
  // Lighting layers switched on (shaders/lighting.hlsli kLayer*: 1 sun, 2 shadows, 4 indirect), for
  // measuring each alone.
  void setLightingLayers(uint32_t layers) { lightingLayers_ = layers; }
  uint32_t lightingLayers() const { return lightingLayers_; }
  // Tactical visibility (fog of war, ARCHITECTURE §7.0 R7): Engine2's darkness mask over the board
  // (world/FogOfWar.h). Every lit surface, card, water, shadow caster and local light reads it, none
  // bypasses it: never seen = black and casts nothing, remembered = darkened without local light, in
  // sight = full lighting. Uploaded when `version` changes; null = no fog of war.
  void setFogOfWar(const e2::FogMask* mask, uint64_t version);
  // Volumetric fog (R7's literal fog: how the visible air looks, never what may be seen; shaders/volume.hlsl).
  // Defaults are a first look, to be tuned against the vision.
  struct VolumetricFog {
    bool enabled = false;
    glm::vec3 wind{0.25f, 0.02f, 0.12f};      // drift, world units per second
    glm::vec3 albedo{0.90f, 0.93f, 0.97f};    // scattering / extinction (slightly cool)
    float density = 0.035f;                   // extinction per world unit at the ground
    float scaleHeight = 1.5f;                 // falls by e over this height above the ground
    float valley = 0.6f;                      // extra density per unit the ground lies below its surroundings
    float boundaryDensity = 0.6f;             // the dense fog along the edge of never-seen ground
    float boundaryHeight = 2.5f;
    float noiseScale = 0.3f;
    float anisotropy = 0.35f;                 // Henyey-Greenstein g (forward scattering: shafts towards the sun)
    float historyWeight = 0.9f;               // temporal smoothing
    float nearDepth = 1.0f, farDepth = 200.0f;  // froxel slices span these view depths
    float meanGround = 0.0f;                  // ground height where the board has none
  };
  void setVolumetricFog(const VolumetricFog& fog) { volumetric_ = fog; }
  // The fog of war's mist (R7): one of the numbered looks in render/FogLooks.h (each frozen in its own
  // shader, shaders/fog_lookNNN.hlsl), drawn only while a fog-of-war mask is set.
  struct FogBank {
    bool enabled = false;
    uint32_t look = 2;  // index into fogLooks(); 2 = look 003 (the one the user kept)
    FogScene scene;
  };
  void setFogBank(const FogBank& bank) { fogBank_ = bank; }
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
    Buffer shadowFrame;   // MeshFrame of the meshes' pass from the sun (when the cached shadow map is redrawn)
    Buffer lights;        // this frame's LocalLight[] (host-visible)
    Buffer meshCounters;  // the work buffer's counters, copied back for meshStats()
    Buffer fogStaging;    // the fog mask's texels on their way to fogMask_ (host-visible, grown on demand)
    Buffer volumeFrame;   // VolumeFrame for the volumetric fog passes (device-local)
    Buffer cloudFrame;    // CloudFrame for the fog bank passes (device-local)
    bool countersPending = false;
  };

  void createTargets(VkExtent2D size);
  void writeFrame(Frame& frame, VkCommandBuffer cmd, float time);
  void recordMeshes(Frame& frame, VkCommandBuffer cmd);
  void cullPass(VkCommandBuffer cmd, VkDeviceAddress frameAddress, VkPipeline instances, uint32_t drawPass, uint32_t instanceCount);
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
  VkPipeline terrainPipeline_ = VK_NULL_HANDLE;
  VkPipeline terrainShadowPipeline_ = VK_NULL_HANDLE;
  VkPipeline waterPipeline_ = VK_NULL_HANDLE;
  VkPipeline meshShadowPipeline_ = VK_NULL_HANDLE, spriteShadowPipeline_ = VK_NULL_HANDLE, lightsPipeline_ = VK_NULL_HANDLE;
  std::vector<LocalLight> lights_;
  Buffer clusters_;
  uint32_t clusterBufferIndex_ = kNoIndex;
  Image cardShadowMap_;
  uint32_t cardShadowMapIndex_ = kNoIndex;
  bool cardShadowsThisFrame_ = false;
  uint64_t shadowMeshKey_ = 0;
  alignas(16) std::array<std::byte, 656> shadowFrameStorage_{};
  // Volumetric fog: two froxel volumes (this frame's and last frame's, swapped), the integrated one.
  VolumetricFog volumetric_;
  Image volumes_[2], volumeIntegrated_;
  uint32_t volumeStorage_[2] = {kNoIndex, kNoIndex}, volumeSampled_[2] = {kNoIndex, kNoIndex};
  uint32_t volumeIntegratedStorage_ = kNoIndex, volumeIntegratedSampled_ = kNoIndex;
  VkPipeline volumeInjectPipeline_ = VK_NULL_HANDLE, volumeIntegratePipeline_ = VK_NULL_HANDLE;
  uint32_t volumeCurrent_ = 0, volumeFrameIndex_ = 0;
  bool volumesReady_ = false, volumeThisFrame_ = false, volumeHistoryValid_ = false;
  glm::mat4 previousVolumeViewProjection_{1.0f};
  // Fog bank: the tiling noise volume (made once), the half-resolution march result.
  FogBank fogBank_;
  Image bankMarch_;
  Image bankNoise_[static_cast<size_t>(FogNoise::Count)];
  Buffer bankNoiseStaging_[static_cast<size_t>(FogNoise::Count)];
  uint32_t bankNoiseIndex_[static_cast<size_t>(FogNoise::Count)] = {kNoIndex, kNoIndex, kNoIndex};
  bool bankNoiseUploaded_[static_cast<size_t>(FogNoise::Count)] = {};
  uint32_t bankMarchStorage_ = kNoIndex, bankMarchSampled_ = kNoIndex;
  std::vector<VkPipeline> bankMarchPipelines_, bankCompositePipelines_;  // per look
  bool bankThisFrame_ = false;
  uint32_t bankLook_ = 0;  // the look this frame draws
  uint32_t bankFrameIndex_ = 0;
  // Fog of war: the CPU copy of the mask (also used to dim local lights by their source's sight), its image.
  bool fogOn_ = false, fogUploadPending_ = false;
  e2::FogMask fog_;
  uint64_t fogVersion_ = 0, shadowFogVersion_ = 0;
  Image fogMask_;
  uint32_t fogMaskIndex_ = kNoIndex;
  float fogSight(glm::vec3 position) const;  // 1 = in sight .. 0 = remembered or never seen (lighting.hlsli)
  WaterView water_;
  Image shadowMap_;
  uint32_t shadowMapIndex_ = kNoIndex;
  VkSampler shadowSampler_ = VK_NULL_HANDLE;
  uint32_t shadowSamplerIndex_ = 0;
  glm::vec3 shadowCenter_{0.0f};
  float shadowRadius_ = 0.0f;
  bool shadowsThisFrame_ = false;          // the shadow map is drawn this frame
  std::array<float, 8> shadowKey_{};       // what the cached shadow map was drawn for
  VkDeviceAddress shadowTerrain_ = 0;
  uint32_t lightingLayers_ = 0xFFFFFFFFu;
  TerrainView terrain_;
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
  bool hizValid_ = false;
  uint32_t costSplit_ = 0;
  uint32_t debugView_ = 0;
  float lodPixelError_ = 1.0f;  // the pyramid holds last frame's depth (false after a resize)
  Camera previousCamera_;
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
