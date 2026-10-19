#include "render/Renderer.h"

#include <cstring>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>

#include <stb_image_write.h>

#include <chrono>
#include <cstring>
#include <string_view>

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
  float bloom;
  uint32_t bloomTexture;
};

// shaders/bloom.hlsl Push.
struct BloomPush {
  uint32_t source;
  uint32_t sampler;
  uint32_t destination;
  float sourceLevel;
  uint32_t width, height;
  float texelX, texelY;
  float weight;
  float sourceWeight;
};
constexpr uint32_t kBloomLevels = 5;
// Each level's share of the finished bloom (UnrealBloomPass's factors 1.0 .. 0.2 mixed by its radius 0.3, then normalised).
constexpr float kBloomWeights[kBloomLevels] = {0.25f, 0.23f, 0.20f, 0.17f, 0.15f};

struct GroundPush {
  glm::mat4 viewProjection;
  float halfExtent;
};

constexpr uint32_t kShadowSize = 4096;
constexpr VkFormat kShadowFormat = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat kCardShadowFormat = VK_FORMAT_D16_UNORM;  // cards: 16 bits is plenty, half the memory

// Clustered lights (shaders/scene.hlsli): 16 x 9 tiles x 24 slices, 63 lights per cluster.
constexpr uint32_t kClusterCount = 16 * 9 * 24;
constexpr VkDeviceSize kClusterBytes = VkDeviceSize{kClusterCount} * 64 * 4;
static_assert(sizeof(LocalLight) == 32);

struct LightsPush {
  VkDeviceAddress frame;
};

struct TerrainPush {
  VkDeviceAddress frame;
  VkDeviceAddress record;
  VkDeviceAddress vertices;
  VkDeviceAddress indices;
  float seamStep;
  uint32_t markCols;      // the board's columns (0 = no ground marks)
  VkDeviceAddress marks;  // per hex Renderer::kMarkLayers x uint2 (colour, half2 rOut / rIn)
  uint32_t markRows;
  uint32_t gridCols;      // the battle grid's board (0 = no grid look), its look number and clock
  uint32_t gridRows;
  uint32_t gridLook;
  VkDeviceAddress grid;   // per hex Renderer::GridCell
  float gridSeconds;
  uint32_t pad;
};
static_assert(sizeof(TerrainPush) == 80);

uint32_t packHalf2(float a, float b) { return glm::packHalf2x16(glm::vec2(a, b)); }

struct SpritePush {
  glm::mat4 viewProjection;
  glm::vec3 cameraRight;
  uint32_t sampler;
  VkDeviceAddress instances;
  VkDeviceAddress frame;  // MeshFrame: scene lighting for lit cards
  glm::vec3 cameraForward;
  uint32_t lean;          // Renderer::setCardLean: cards lean back with the camera's tilt (upright on screen)
};
static_assert(sizeof(SpritePush) == 112);

struct EffectPush {
  VkDeviceAddress instances;
  VkDeviceAddress frame;
  uint32_t sampler;
  uint32_t depthTexture;
  float brightness;
  uint32_t pad;
};
static_assert(sizeof(EffectInstance) == 96);

struct VfxPush {
  VkDeviceAddress vertices;
  VkDeviceAddress instances;
  VkDeviceAddress draws;
  VkDeviceAddress frame;
  uint32_t drawIndex;
  float brightness;
  uint32_t sampler;
  uint32_t pad;
};
static_assert(sizeof(VfxPush) <= Bindless::kPushConstantBytes);

struct WaterPush {
  VkDeviceAddress frame;
  VkDeviceAddress vertices;
  VkDeviceAddress indices;
  uint32_t look;  // shaders/water.hlsl: 0 Engine2, 1 look 001 "ember"
  uint32_t pad;
};

struct SkyReflectPush {  // shaders/skyreflect.hlsl
  VkDeviceAddress frame;
  uint32_t destination, level, width, height;
};
// The sky's reflection map: lat-long, this many levels (lighting.hlsli kSkyReflectLevels / kSkyReflectSize).
constexpr uint32_t kSkyReflectLevels = 6;
constexpr VkExtent2D kSkyReflectSize{256, 128};

struct Water3DPush {  // shaders/water3d.hlsl
  VkDeviceAddress frame, vertices, indices;
  uint32_t colorMap, normalMap, roughMap;
  float color[3];
  float opacity, normalScale, roughness, modern, v4, repeat;
  uint32_t look;
};
static_assert(offsetof(Water3DPush, look) == 72 && sizeof(Water3DPush) <= Bindless::kPushConstantBytes);
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
  float exposure;
  glm::vec3 groundColor;
  float litCap;
  glm::mat4 view;
  float projectionX;
  float projectionY;
  uint32_t hizTexture;
  uint32_t hizSampler;
  glm::vec2 hizSize;
  uint32_t instanceVisibility;
  uint32_t skyReflect;  // the sky's prefiltered reflection map (skyreflect.hlsl); kNoTextureIndex = none
  glm::mat4 previousView;
  float previousProjectionX;
  float previousProjectionY;
  uint32_t earlyOcclusion;
  uint32_t costSplit;
  uint32_t debugView;
  uint32_t shadowPass;
  uint32_t cardShadowMap;
  uint32_t cloudFlags;  // clouds.hlsli: 1 the layer exists, 2 it shadows the board
  glm::mat4 shadowMatrix;
  uint32_t shadowMap;
  uint32_t shadowSampler;
  float shadowSize;
  float shadowTexelWorld;
  float shadowFilterTexels;
  uint32_t lightingLayers;
  float time;
  uint32_t pad8;
  uint64_t lights;
  uint32_t lightCount;
  uint32_t clusterBuffer;
  float clusterNear;
  float clusterFar;
  uint64_t clusters;
  glm::vec4 fogRect;   // fog of war mask: world x0, z0, 1 / width, 1 / depth
  uint32_t fogMask;    // bindless RGBA16F texture (darkness, boundary, ground, valley); kNoTextureIndex = no fog of war
  uint32_t fogSampler;
  uint32_t pad9[2];
  uint32_t volume;     // integrated froxel volume (sampled); kNoTextureIndex = no volumetric fog
  uint32_t volumeSampler;
  float volumeNear;
  float volumeFar;
  uint64_t atmosphere;  // AtmosphereFrame (device address); 0 = the hemisphere, no sky, no aerial perspective
  uint32_t volumeSlices;  // slices of `volume` (kFroxelZ for volume.hlsl; Volumetrics' grid when it owns the volume)
  uint32_t volumeFlags;   // scene.hlsli kVolume*: what else reads the volume (0 for volume.hlsl)
  glm::mat4 localShadowMatrix;      // the spell light's shadow (Renderer::setLocalShadow): world -> its clip
  glm::vec3 localShadowPosition;    // the light it belongs to
  uint32_t localShadowMap;          // kNoTextureIndex = none this frame
  float localShadowSize;
  uint32_t pad10[3];
};
static_assert(sizeof(MeshFrame) == 768 && offsetof(MeshFrame, meshes) == 200 && offsetof(MeshFrame, previousView) == 384 &&
              offsetof(MeshFrame, shadowMatrix) == 480 && offsetof(MeshFrame, localShadowMatrix) == 672);

// Matches VolumeFrame in shaders/volume.hlsl.
struct VolumeFrame {
  glm::mat4 inverseViewProjection;
  glm::mat4 previousViewProjection;
  glm::vec3 wind;
  float time;
  glm::vec3 albedo;
  float density;
  float scaleHeight;
  float valley;
  float boundaryDensity;
  float boundaryHeight;
  float noiseScale;
  float anisotropy;
  float historyWeight;
  float jitter;
  uint32_t injected;
  uint32_t history;
  uint32_t integrated;
  uint32_t historyValid;
  float meanGround;
  uint32_t pad[3];
};
static_assert(sizeof(VolumeFrame) == 224);

struct VolumePush {
  VkDeviceAddress frame;
  VkDeviceAddress volume;
};

// Froxel grid (shaders/scene.hlsli kFroxel*).
constexpr uint32_t kFroxelX = 160, kFroxelY = 90, kFroxelZ = 64;

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
  uint32_t background;  // kNoTextureIndex: the sky (atmosphere) or clearColor; kBlackBackground: black; else a backdrop
  uint32_t backgroundMode;  // with a backdrop: 1 = the painting up to the horizon, the sky above (setBackground toHorizon)
  uint32_t pad[2];
};
static_assert(sizeof(ResolvePush) <= Bindless::kPushConstantBytes);

constexpr glm::vec4 kClearColor{0.012f, 0.014f, 0.02f, 1.0f};
// Under the fog of war, off the board is never seen (Engine2: the black must not trace the map's outline).
constexpr glm::vec4 kNeverSeenColor{0.0f, 0.0f, 0.0f, 1.0f};

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
    frame.effects = createHostBuffer(context, sizeof(EffectInstance) * kMaxEffects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.vfxVertices = createHostBuffer(context, sizeof(VfxVertex) * kMaxVfxVertices, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.vfxInstances = createHostBuffer(context, sizeof(VfxInstance) * kMaxVfxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.vfxDraws = createHostBuffer(context, sizeof(VfxDrawGpu) * kMaxVfxDraws, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.meshFrame = createDeviceBuffer(context, sizeof(MeshFrame), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    frame.shadowFrame = createDeviceBuffer(context, sizeof(MeshFrame), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    frame.lights = createHostBuffer(context, sizeof(LocalLight) * kMaxLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.marks = createHostBuffer(context, sizeof(uint32_t) * 2 * kMarkLayers * kMaxMarkCells, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.grid = createHostBuffer(context, sizeof(GridCell) * kMaxMarkCells, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.meshCounters = createReadbackBuffer(context, kCounterBytes);
    frame.volumeFrame = createDeviceBuffer(context, sizeof(VolumeFrame), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    frame.cloudFrame = createDeviceBuffer(context, kMaxFogLookFrameBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    frame.atmosphereFrame = createDeviceBuffer(context, sizeof(AtmosphereFrame), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  }
  // Atmosfera volumétrica v1: integration point 1 of 3 (docs/ATMOSPHERE.md §4).
  volumetrics_ = std::make_unique<Volumetrics>(context, bindless_);
  // The atmosphere's tables (GENERAL layout: written as storage, read as textures) and its SH ambient.
  {
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    atmoTransmittance_ = createImage(context, {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .extent = {256, 64}, .usage = usage});
    atmoMultiScattering_ = createImage(context, {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .extent = {32, 32}, .usage = usage});
    atmoSkyView_ = createImage(context, {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .extent = {192, 108}, .usage = usage});
    atmoAerial_ = createImage(context, {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .extent = {32, 32}, .usage = usage, .depth = 32});
    const Image* tables[4] = {&atmoTransmittance_, &atmoMultiScattering_, &atmoSkyView_, &atmoAerial_};
    for (int i = 0; i < 4; ++i) {
      atmoSampled_[i] = bindless_.addSampledImage(tables[i]->view, VK_IMAGE_LAYOUT_GENERAL);
      atmoStorage_[i] = bindless_.addStorageImage(tables[i]->view);
    }
    atmoAmbient_ = createDeviceBuffer(context, 9 * 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    // The sky's reflection map (skyreflect.hlsl): one storage view per level, one sampled view of the chain.
    ImageDesc reflectDesc{VK_FORMAT_R16G16B16A16_SFLOAT, kSkyReflectSize, usage};
    reflectDesc.mipLevels = kSkyReflectLevels;
    skyReflect_ = createImage(context, reflectDesc);
    for (uint32_t level = 0; level < kSkyReflectLevels; ++level) {
      VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      viewInfo.image = skyReflect_.image;
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
      viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1};
      VkImageView view;
      VK_CHECK(vkCreateImageView(context.device, &viewInfo, nullptr, &view));
      skyReflectLevelViews_.push_back(view);
      skyReflectLevelIndices_.push_back(bindless_.addStorageImage(view));
    }
    skyReflectSampled_ = bindless_.addSampledImage(skyReflect_.view, VK_IMAGE_LAYOUT_GENERAL);
    // The cloud shadow map over the board (clouds.hlsli cloudShadow).
    cloudShadow_ = createImage(context, {.format = VK_FORMAT_R32_SFLOAT, .extent = {kCloudShadowSize, kCloudShadowSize}, .usage = usage});
    cloudShadowStorage_ = bindless_.addStorageImage(cloudShadow_.view);
    cloudShadowSampled_ = bindless_.addSampledImage(cloudShadow_.view, VK_IMAGE_LAYOUT_GENERAL);
  }
  // The fog looks' noise volumes (filled the first time a look needs one).
  for (size_t k = 0; k < static_cast<size_t>(FogNoise::Count); ++k) {
    bankNoise_[k] = createImage(context, {.format = VK_FORMAT_R8_UNORM, .extent = {kFogNoiseSize, kFogNoiseSize},
                                          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, .depth = kFogNoiseSize});
    bankNoiseIndex_[k] = bindless_.addSampledImage(bankNoise_[k].view);
  }
  // Volumetric fog: froxel volumes, kept in GENERAL layout (written as storage, read as textures).
  for (int i = 0; i < 2; ++i) {
    volumes_[i] = createImage(context, {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .extent = {kFroxelX, kFroxelY}, .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, .depth = kFroxelZ});
    volumeStorage_[i] = bindless_.addStorageImage(volumes_[i].view);
    volumeSampled_[i] = bindless_.addSampledImage(volumes_[i].view, VK_IMAGE_LAYOUT_GENERAL);
  }
  volumeIntegrated_ = createImage(context, {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .extent = {kFroxelX, kFroxelY}, .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, .depth = kFroxelZ});
  volumeIntegratedStorage_ = bindless_.addStorageImage(volumeIntegrated_.view);
  volumeIntegratedSampled_ = bindless_.addSampledImage(volumeIntegrated_.view, VK_IMAGE_LAYOUT_GENERAL);
  meshWork_ = createDeviceBuffer(context, kWorkHeaderBytes + VkDeviceSize{kMaxQueue} * 8 * 2 + VkDeviceSize{kMaxClusters} * (8 + 4),
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  meshWorkIndex_ = bindless_.addStorageBuffer(meshWork_.buffer);
  instanceVisibility_ = createDeviceBuffer(context, VkDeviceSize{kMaxInstances} * 4,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  instanceVisibilityIndex_ = bindless_.addStorageBuffer(instanceVisibility_.buffer);
  clusters_ = createDeviceBuffer(context, kClusterBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  clusterBufferIndex_ = bindless_.addStorageBuffer(clusters_.buffer);

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
  // Sun shadow: comparison sampler (hardware 2x2 filtering of the depth test) and the map itself.
  VkSamplerCreateInfo shadowInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  shadowInfo.magFilter = shadowInfo.minFilter = VK_FILTER_LINEAR;
  shadowInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  shadowInfo.addressModeU = shadowInfo.addressModeV = shadowInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  shadowInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  shadowInfo.compareEnable = VK_TRUE;
  shadowInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  VK_CHECK(vkCreateSampler(context.device, &shadowInfo, nullptr, &shadowSampler_));
  shadowSamplerIndex_ = bindless_.addSampler(shadowSampler_);
  shadowMap_ = createImage(context, {kShadowFormat, {shadowSize_, shadowSize_}, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
  shadowMapIndex_ = bindless_.addSampledImage(shadowMap_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
  cardShadowMap_ = createImage(context, {kCardShadowFormat, {shadowSize_, shadowSize_}, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
  cardShadowMapIndex_ = bindless_.addSampledImage(cardShadowMap_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
  localShadowMap_ = createImage(context, {kCardShadowFormat, {kLocalShadowSize, kLocalShadowSize}, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
  localShadowMapIndex_ = bindless_.addSampledImage(localShadowMap_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);

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
  GraphicsPipelineDesc terrain;
  terrain.vertexShader = "terrain.VSMain";
  terrain.fragmentShader = "terrain.PSMain";
  terrain.colorFormats = {kSceneColorFormat};
  terrain.depthFormat = kDepthFormat;
  terrain.depthTest = true;
  terrain.depthWrite = true;
  terrainPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), terrain);

  GraphicsPipelineDesc terrainShadow;
  terrainShadow.vertexShader = "terrain.VSShadow";
  terrainShadow.fragmentShader = "terrain.PSShadow";  // never-seen ground casts no shadow (R7)
  terrainShadow.depthFormat = kShadowFormat;
  terrainShadow.depthTest = true;
  terrainShadow.depthWrite = true;
  terrainShadow.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
  terrainShadow.depthBiasConstant = 1.0f;
  terrainShadow.depthBiasSlope = 1.5f;
  terrainShadowPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), terrainShadow);

  GraphicsPipelineDesc water;
  water.vertexShader = "water.VSMain";
  water.fragmentShader = "water.PSMain";
  water.colorFormats = {kSceneColorFormat};
  water.depthFormat = kDepthFormat;
  water.depthTest = true;
  water.depthWrite = false;
  water.alphaBlend = true;
  water.premultipliedAlpha = true;
  waterPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), water);
  water.vertexShader = "water3d.VSMain";
  water.fragmentShader = "water3d.PSMain";
  water3dPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), water);

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
  // The cards' depth after all of them are drawn: the fog bank and effects stop at a character (sprites.PSDepth).
  sprites.fragmentShader = "sprites.PSDepth";
  sprites.depthWrite = true;
  spriteDepthPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), sprites);

  // Spell effects: over everything, depth-tested against it (read-only depth, also sampled for the soft fade).
  GraphicsPipelineDesc effects;
  effects.vertexShader = "effects.VSMain";
  effects.fragmentShader = "effects.PSMain";
  effects.colorFormats = {kSceneColorFormat};
  effects.depthFormat = kDepthFormat;
  effects.depthTest = true;
  effects.depthWrite = false;
  effects.alphaBlend = true;
  effects.premultipliedAlpha = true;
  effectPipeline_ = createGraphicsPipeline(context, bindless_.pipelineLayout(), effects);
  // Engine2's ported effects (shaders/vfx.hlsl): the same blend; depth-tested or drawn over everything (Engine2 draws
  // Magic Missile V2 in the foreground: depthTest false).
  for (int depthTested = 0; depthTested < 2; ++depthTested) {
    GraphicsPipelineDesc vfx = effects;
    vfx.vertexShader = "vfx.VSMain";
    vfx.fragmentShader = "vfx.PSMain";
    vfx.depthTest = depthTested == 1;
    vfxPipelines_[depthTested] = createGraphicsPipeline(context, bindless_.pipelineLayout(), vfx);
  }

  // Mesh passes.
  const VkPipelineLayout layout = bindless_.pipelineLayout();
  cullInstancesPipeline_ = createComputePipeline(context, layout, "cull.CSInstancesEarly");
  cullInstancesLatePipeline_ = createComputePipeline(context, layout, "cull.CSInstancesLate");
  hizPipeline_ = createComputePipeline(context, layout, "hiz.CSDownsample");
  bloomDownPipeline_ = createComputePipeline(context, layout, "bloom.CSDown");
  bloomUpPipeline_ = createComputePipeline(context, layout, "bloom.CSUp");
  cullPreparePipeline_ = createComputePipeline(context, layout, "cull.CSPrepare");
  cullTraversePipeline_ = createComputePipeline(context, layout, "cull.CSTraverse");
  cullPrepareDrawPipeline_ = createComputePipeline(context, layout, "cull.CSPrepareDraw");
  resolvePipeline_ = createComputePipeline(context, layout, "resolve.CSResolve");
  skyPipeline_ = createComputePipeline(context, layout, "resolve.CSSky");
  lightsPipeline_ = createComputePipeline(context, layout, "lights.CSClusters");
  volumeInjectPipeline_ = createComputePipeline(context, layout, "volume.CSInject");
  {
    const char* passes[5] = {"atmosphere.CSTransmittance", "atmosphere.CSMultiScattering", "atmosphere.CSSkyView", "atmosphere.CSAerial",
                             "atmosphere.CSAmbient"};
    for (int i = 0; i < 5; ++i) atmoPipelines_[i] = createComputePipeline(context, layout, passes[i]);
  }
  skyReflectBasePipeline_ = createComputePipeline(context, layout, "skyreflect.CSBase");
  skyReflectFilterPipeline_ = createComputePipeline(context, layout, "skyreflect.CSFilter");
  cloudShadowPipeline_ = createComputePipeline(context, layout, "skyreflect.CSCloudShadow");
  volumeIntegratePipeline_ = createComputePipeline(context, layout, "volume.CSIntegrate");
  for (const FogLook& look : fogLooks()) {
    bankMarchPipelines_.push_back(createComputePipeline(context, layout, std::string(look.shader) + ".CSMarch"));
    bankCompositePipelines_.push_back(createComputePipeline(context, layout, std::string(look.shader) + ".CSComposite"));
  }
  GraphicsPipelineDesc meshlet;
  meshlet.taskShader = "meshlet.ASMain";
  meshlet.meshShader = "meshlet.MSMain";
  meshlet.fragmentShader = "meshlet.PSMain";
  meshlet.colorFormats = {kVisibilityFormat};
  meshlet.depthFormat = kDepthFormat;
  meshlet.depthTest = true;
  meshlet.depthWrite = true;
  meshletPipeline_ = createGraphicsPipeline(context, layout, meshlet);
  GraphicsPipelineDesc meshShadow;
  meshShadow.taskShader = "meshlet.ASMain";
  meshShadow.meshShader = "meshlet.MSMain";
  meshShadow.fragmentShader = "meshlet.PSShadow";
  meshShadow.depthFormat = kShadowFormat;
  meshShadow.depthTest = true;
  meshShadow.depthWrite = true;
  meshShadow.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
  meshShadow.depthBiasConstant = 1.0f;
  meshShadow.depthBiasSlope = 1.5f;
  meshShadowPipeline_ = createGraphicsPipeline(context, layout, meshShadow);
  GraphicsPipelineDesc spriteShadow;
  spriteShadow.vertexShader = "sprites.VSShadow";
  spriteShadow.fragmentShader = "sprites.PSShadow";
  spriteShadow.depthFormat = kCardShadowFormat;
  spriteShadow.depthTest = true;
  spriteShadow.depthWrite = true;
  spriteShadow.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
  spriteShadow.depthBiasConstant = 1.0f;
  spriteShadow.depthBiasSlope = 1.5f;
  spriteShadowPipeline_ = createGraphicsPipeline(context, layout, spriteShadow);
}

void Renderer::setFogOfWar(const e2::FogMask* mask, uint64_t version, const std::vector<uint16_t>* texels) {
  fogOn_ = mask && mask->width > 0 && mask->height > 0;
  if (!fogOn_ || version == fogVersion_) return;
  fog_ = *mask;
  if (texels && texels->size() == mask->alpha.size() * 4) fogTexels_ = *texels;
  else fogTexels(*mask, fogTexels_);
  fogVersion_ = version;
  fogUploadPending_ = true;
}

void Renderer::fogTexels(const e2::FogMask& mask, std::vector<uint16_t>& out) {
  const size_t texels = mask.alpha.size();
  out.resize(texels * 4);
  for (size_t i = 0; i < texels; ++i) {
    out[i * 4 + 0] = glm::packHalf1x16(mask.alpha[i] / 255.0f);
    out[i * 4 + 1] = glm::packHalf1x16(mask.boundary.empty() ? 0.0f : mask.boundary[i]);
    out[i * 4 + 2] = glm::packHalf1x16(mask.ground.empty() ? 0.0f : mask.ground[i]);
    out[i * 4 + 3] = glm::packHalf1x16(mask.valley.empty() ? 0.0f : mask.valley[i]);
  }
}

// The mask as the shaders sample it (bilinear, clamped; outside it = never seen), as sight for local lights.
float Renderer::fogSight(glm::vec3 position) const {
  const float u = (position.x - fog_.x0) / fog_.worldWidth, v = (position.z - fog_.z0) / fog_.worldDepth;
  if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f) return 0.0f;
  const float x = std::clamp(u * fog_.width - 0.5f, 0.0f, fog_.width - 1.0f), y = std::clamp(v * fog_.height - 0.5f, 0.0f, fog_.height - 1.0f);
  const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
  const int x1 = std::min(x0 + 1, fog_.width - 1), y1 = std::min(y0 + 1, fog_.height - 1);
  const float fx = x - x0, fy = y - y0;
  const auto at = [&](int tx, int ty) { return fog_.alpha[static_cast<size_t>(ty) * fog_.width + tx] / 255.0f; };
  const float a = glm::mix(glm::mix(at(x0, y0), at(x1, y0), fx), glm::mix(at(x0, y1), at(x1, y1), fx), fy);
  return std::clamp(1.0f - a / e2::kExploredAlpha, 0.0f, 1.0f);
}

void Renderer::setScene(const Camera& camera, std::span<const SpriteInstance> sprites) {
  hasScene_ = true;
  camera_ = camera;
  sprites_.assign(sprites.begin(), sprites.begin() + std::min<size_t>(sprites.size(), kMaxSprites - kMaxFrontCards));
}

void Renderer::setShadowResolution(uint32_t size) {
  size = std::clamp(size, 512u, kShadowSize);
  if (size == shadowSize_) return;
  vkDeviceWaitIdle(context_.device);
  bindless_.removeSampledImage(shadowMapIndex_);
  destroyImage(context_, shadowMap_);
  bindless_.removeSampledImage(cardShadowMapIndex_);
  destroyImage(context_, cardShadowMap_);
  shadowSize_ = size;
  shadowMap_ = createImage(context_, {kShadowFormat, {shadowSize_, shadowSize_}, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
  shadowMapIndex_ = bindless_.addSampledImage(shadowMap_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
  cardShadowMap_ = createImage(context_, {kCardShadowFormat, {shadowSize_, shadowSize_}, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
  cardShadowMapIndex_ = bindless_.addSampledImage(cardShadowMap_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
  shadowKey_.fill(std::numeric_limits<float>::quiet_NaN());  // the new map is empty: draw it on the next frame
}

Renderer::~Renderer() {
  vkDeviceWaitIdle(context_.device);
  vkDestroyPipeline(context_.device, trianglePipeline_, nullptr);
  vkDestroyPipeline(context_.device, tonemapPipeline_, nullptr);
  vkDestroyPipeline(context_.device, groundPipeline_, nullptr);
  vkDestroyPipeline(context_.device, terrainPipeline_, nullptr);
  vkDestroyPipeline(context_.device, terrainShadowPipeline_, nullptr);
  vkDestroyPipeline(context_.device, waterPipeline_, nullptr);
  vkDestroyPipeline(context_.device, water3dPipeline_, nullptr);
  bindless_.removeSampledImage(shadowMapIndex_);
  destroyImage(context_, shadowMap_);
  bindless_.removeSampledImage(cardShadowMapIndex_);
  destroyImage(context_, cardShadowMap_);
  bindless_.removeSampledImage(localShadowMapIndex_);
  destroyImage(context_, localShadowMap_);
  vkDestroyPipeline(context_.device, meshShadowPipeline_, nullptr);
  vkDestroyPipeline(context_.device, lightsPipeline_, nullptr);
  destroyBuffer(context_, clusters_);
  vkDestroyPipeline(context_.device, spriteShadowPipeline_, nullptr);
  vkDestroySampler(context_.device, shadowSampler_, nullptr);
  vkDestroyPipeline(context_.device, spritePipeline_, nullptr);
  vkDestroyPipeline(context_.device, spriteDepthPipeline_, nullptr);
  vkDestroyPipeline(context_.device, effectPipeline_, nullptr);
  for (VkPipeline p : vfxPipelines_) vkDestroyPipeline(context_.device, p, nullptr);
  for (VkPipeline p : {cullInstancesPipeline_, cullInstancesLatePipeline_, hizPipeline_, bloomDownPipeline_, bloomUpPipeline_, cullPreparePipeline_,
                       cullTraversePipeline_, cullPrepareDrawPipeline_, meshletPipeline_, resolvePipeline_, skyPipeline_})
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
    destroyBuffer(context_, frame.effects);
    destroyBuffer(context_, frame.vfxVertices);
    destroyBuffer(context_, frame.vfxInstances);
    destroyBuffer(context_, frame.vfxDraws);
    destroyBuffer(context_, frame.meshFrame);
    destroyBuffer(context_, frame.meshCounters);
    destroyBuffer(context_, frame.shadowFrame);
    destroyBuffer(context_, frame.lights);
    destroyBuffer(context_, frame.marks);
    destroyBuffer(context_, frame.grid);
    destroyBuffer(context_, frame.fogStaging);
    destroyBuffer(context_, frame.volumeFrame);
    destroyBuffer(context_, frame.cloudFrame);
    destroyBuffer(context_, frame.atmosphereFrame);
  }
  for (VkPipeline p : atmoPipelines_) vkDestroyPipeline(context_.device, p, nullptr);
  for (Image* table : {&atmoTransmittance_, &atmoMultiScattering_, &atmoSkyView_, &atmoAerial_}) destroyImage(context_, *table);
  destroyBuffer(context_, atmoAmbient_);
  vkDestroyPipeline(context_.device, skyReflectBasePipeline_, nullptr);
  vkDestroyPipeline(context_.device, skyReflectFilterPipeline_, nullptr);
  for (size_t i = 0; i < skyReflectLevelViews_.size(); ++i) {
    bindless_.removeStorageImage(skyReflectLevelIndices_[i]);
    vkDestroyImageView(context_.device, skyReflectLevelViews_[i], nullptr);
  }
  bindless_.removeSampledImage(skyReflectSampled_);
  destroyImage(context_, skyReflect_);
  vkDestroyPipeline(context_.device, cloudShadowPipeline_, nullptr);
  bindless_.removeStorageImage(cloudShadowStorage_);
  bindless_.removeSampledImage(cloudShadowSampled_);
  destroyImage(context_, cloudShadow_);
  for (VkPipeline p : bankMarchPipelines_) vkDestroyPipeline(context_.device, p, nullptr);
  for (VkPipeline p : bankCompositePipelines_) vkDestroyPipeline(context_.device, p, nullptr);
  for (size_t k = 0; k < static_cast<size_t>(FogNoise::Count); ++k) {
    destroyImage(context_, bankNoise_[k]);
    destroyBuffer(context_, bankNoiseStaging_[k]);
  }
  vkDestroyPipeline(context_.device, volumeInjectPipeline_, nullptr);
  vkDestroyPipeline(context_.device, volumeIntegratePipeline_, nullptr);
  for (int i = 0; i < 2; ++i) destroyImage(context_, volumes_[i]);
  destroyImage(context_, volumeIntegrated_);
  if (fogMaskIndex_ != kNoIndex) bindless_.removeSampledImage(fogMaskIndex_);
  destroyImage(context_, fogMask_);
  volumetrics_.reset();
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
  // The fog bank's half-resolution march result.
  bankMarch_ = createImage(context_, {VK_FORMAT_R16G16B16A16_SFLOAT, {(size.width + 1) / 2, (size.height + 1) / 2},
                                      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
  // Bloom pyramid: half the screen, kBloomLevels levels, one storage view per level (see recordBloom).
  bloomSize_ = {std::max(1u, (size.width + 1) / 2), std::max(1u, (size.height + 1) / 2)};
  ImageDesc bloomDesc{VK_FORMAT_R16G16B16A16_SFLOAT, bloomSize_, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT};
  bloomDesc.mipLevels = kBloomLevels;
  bloomImage_ = createImage(context_, bloomDesc);
  for (uint32_t level = 0; level < kBloomLevels; ++level) {
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = bloomImage_.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1};
    VkImageView view;
    VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &view));
    bloomLevelViews_.push_back(view);
    bloomLevelIndices_.push_back(bindless_.addStorageImage(view));
  }
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
    bankMarchStorage_ = bindless_.addStorageImage(bankMarch_.view);
    bankMarchSampled_ = bindless_.addSampledImage(bankMarch_.view, VK_IMAGE_LAYOUT_GENERAL);
    bloomSampledIndex_ = bindless_.addSampledImage(bloomImage_.view, VK_IMAGE_LAYOUT_GENERAL);
  } else {
    bindless_.setSampledImage(hizIndex_, hiz_.view, VK_IMAGE_LAYOUT_GENERAL);
    bindless_.setSampledImage(depthIndex_, depth_.view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
    bindless_.setSampledImage(sceneColorIndex_, sceneColor_.view);
    bindless_.setStorageImage(sceneColorStorageIndex_, sceneColor_.view);
    bindless_.setStorageImage(visibilityIndex_, visibility_.view);
    bindless_.setStorageImage(bankMarchStorage_, bankMarch_.view);
    bindless_.setSampledImage(bankMarchSampled_, bankMarch_.view, VK_IMAGE_LAYOUT_GENERAL);
    bindless_.setSampledImage(bloomSampledIndex_, bloomImage_.view, VK_IMAGE_LAYOUT_GENERAL);
  }
}

void Renderer::destroyTargets() {
  destroyImage(context_, sceneColor_);
  destroyImage(context_, depth_);
  destroyImage(context_, output_);
  destroyImage(context_, visibility_);
  destroyImage(context_, bankMarch_);
  for (size_t i = 0; i < bloomLevelViews_.size(); ++i) {
    bindless_.removeStorageImage(bloomLevelIndices_[i]);
    vkDestroyImageView(context_.device, bloomLevelViews_[i], nullptr);
  }
  bloomLevelViews_.clear();
  bloomLevelIndices_.clear();
  destroyImage(context_, bloomImage_);
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
  for (const GpuTimer::Result& r : gpuTimer_.results())
    if (std::string_view(r.name) == "volumetrics") volumetrics_->reportGpuMilliseconds(r.milliseconds);
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
  const auto recordStart = std::chrono::steady_clock::now();
  record(frame, time, swapImage, nullptr);
  lastSplit_ = {millisecondsSince(recordStart), 0.0, 0.0};

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
  const auto submitStart = std::chrono::steady_clock::now();
  VK_CHECK(vkQueueSubmit2(context_.queue, 1, &submit, frame.done));
  lastSplit_.submit = millisecondsSince(submitStart);
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
  const auto presentStart = std::chrono::steady_clock::now();
  const VkResult presented = vkQueuePresentKHR(context_.queue, &present);
  lastSplit_.present = millisecondsSince(presentStart);
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

// The frame's constants (MeshFrame: camera, lighting, mesh-pass state), read by every scene pass.
void Renderer::writeFrame(Frame& frame, VkCommandBuffer cmd, float time) {
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
  // Exposure is multiplied into the lights (Luminous: keeps fp16 scene colour in range), never applied in post.
  f.exposure = lighting_.exposure;
  f.sunIntensity = lighting_.sunIntensity * lighting_.exposure;
  f.sunColor = lighting_.sunColor;
  f.ambientIntensity = lighting_.ambientIntensity * lighting_.exposure;
  f.skyColor = lighting_.skyColor;
  f.groundColor = lighting_.groundColor;
  f.litCap = lighting_.litScale * lighting_.exposure;
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
  f.lightingLayers = lightingLayers_;
  f.time = time;
  f.lights = bufferAddress(context_, frame.lights);
  f.clusterBuffer = clusterBufferIndex_;
  // The slices follow the near plane: the 2D view's long lens stands far above the board (MapStage).
  f.clusterNear = std::max(2.0f, camera_.projection[3][2]);
  f.clusterFar = f.clusterNear + 398.0f;
  f.clusters = bufferAddress(context_, clusters_);
  f.fogMask = kNoTextureIndex;
  if (fogOn_ && fogMaskIndex_ != kNoIndex) {
    f.fogMask = fogMaskIndex_;
    f.fogSampler = linearClampIndex_;
    f.fogRect = {fog_.x0, fog_.z0, 1.0f / fog_.worldWidth, 1.0f / fog_.worldDepth};
  }
  // Atmosfera volumétrica v1 (render/Volumetrics.h): when on, it owns the volume every surface reads; the old haze
  // (volume.hlsl) then stays off. Integration point 2 of 3 (docs/ATMOSPHERE.md §4).
  Volumetrics::FrameOut vo;
  volumetricsThisFrame_ = hasScene_ && volumetrics_->prepare(camera_, time, cmd, frameIndex_, vo);
  // Volumetric fog: this frame's froxel volume (written below), last frame's for the history.
  volumeThisFrame_ = volumetric_.enabled && !volumetricsThisFrame_;
  f.volume = kNoTextureIndex;
  f.volumeSampler = linearClampIndex_;
  f.volumeNear = volumetric_.nearDepth;
  f.volumeFar = volumetric_.farDepth;
  f.volumeSlices = kFroxelZ;
  f.volumeFlags = 0;
  if (volumetricsThisFrame_) {
    f.volume = vo.integrated;
    f.volumeNear = vo.near;
    f.volumeFar = vo.far;
    f.volumeSlices = vo.slices;
    f.volumeFlags = 3;  // kVolumeEffects | kVolumeBackground
  }
  // The atmosphere: this frame's record (the tables are rebuilt from it in record()).
  atmoThisFrame_ = lighting_.atmosphere;
  f.atmosphere = 0;
  if (atmoThisFrame_) {
    AtmosphereFrame a;
    a.inverseViewProjection = glm::inverse(camera_.viewProjection());
    a.sunDirection = glm::normalize(lighting_.sunDirection);
    a.sunIlluminance = lighting_.skyIlluminance;
    a.kmPerUnit = lighting_.kmPerUnit;
    a.skyScale = lighting_.skyScale;
    a.sunDisc = lighting_.sunDisc ? 1u : 0u;
    a.sunTanRadius = lighting_.sunRadius > 0.0f ? std::tan(lighting_.sunRadius) : 0.0f;
    a.stars = lighting_.stars;
    a.starLook = lighting_.starLook;
    a.moonDirection = glm::normalize(lighting_.moonDirection);
    a.moonTexture = lighting_.moonRadius > 0.0f ? lighting_.moonTexture : 0xFFFFFFFFu;
    a.moonTanRadius = std::tan(lighting_.moonRadius);
    a.moonGlow = lighting_.moonGlow;
    const glm::vec3 sky = lighting_.skyColor;
    a.ambientTarget = lighting_.ambientIntensity * lighting_.exposure * (0.2126f * sky.r + 0.7152f * sky.g + 0.0722f * sky.b);
    // A negative target = keep the sky's own irradiance, times -target (the physical light mode; exposure included).
    if (lighting_.physicalSky) a.ambientTarget = -lighting_.ambientMul * lighting_.exposure;
    a.transmittance = atmoSampled_[0], a.transmittanceStorage = atmoStorage_[0];
    a.multiScattering = atmoSampled_[1], a.multiScatteringStorage = atmoStorage_[1];
    a.skyView = atmoSampled_[2], a.skyViewStorage = atmoStorage_[2];
    a.aerial = atmoSampled_[3], a.aerialStorage = atmoStorage_[3];
    a.ambient = bufferAddress(context_, atmoAmbient_);
    a.linearSampler = linearClampIndex_;
    // Nuvens básicas (clouds.hlsli): the sheet's numbers; it shadows the board by day only (the key light is the sun), at
    // night it is still in the sky and the reflections.
    cloudShadowThisFrame_ = false;
    if (clouds_.enabled) {
      f.cloudFlags = 1u | (clouds_.shadows && lighting_.sunDisc ? 2u : 0u);
      a.cloudCoverage = clouds_.coverage;
      a.cloudAltitude = clouds_.altitude;
      a.cloudShadow = clouds_.shadow;
      a.cloudScale = clouds_.scale;
      a.cloudWind = clouds_.wind;
      // The shadow map covers the sun shadow's bounds (the board), at the board's middle height.
      if ((f.cloudFlags & 2u) && shadowRadius_ > 0.0f) {
        const float r = shadowRadius_;
        a.cloudShadowRect = {shadowCenter_.x - r, shadowCenter_.z - r, 0.5f / r, 0.5f / r};
        a.cloudShadowMap = cloudShadowSampled_;
        cloudShadowHeight_ = shadowCenter_.y;
        cloudShadowThisFrame_ = true;
      }
    }
    vkCmdUpdateBuffer(cmd, frame.atmosphereFrame.buffer, 0, sizeof(a), &a);
    f.atmosphere = bufferAddress(context_, frame.atmosphereFrame);
  }
  // The sky's reflection map (skyreflect.hlsl): needs the sky.
  skyReflectThisFrame_ = atmoThisFrame_ && skyReflection_;
  f.skyReflect = skyReflectThisFrame_ ? skyReflectSampled_ : kNoTextureIndex;
  VolumeFrame vf{};
  if (volumeThisFrame_) {
    f.volume = volumeIntegratedSampled_;
    const glm::mat4 viewProjection = camera_.viewProjection();
    vf.inverseViewProjection = glm::inverse(viewProjection);
    vf.previousViewProjection = previousVolumeViewProjection_;
    previousVolumeViewProjection_ = viewProjection;
    vf.wind = volumetric_.wind;
    vf.time = time;
    vf.albedo = volumetric_.albedo;
    vf.density = volumetric_.density;
    vf.scaleHeight = volumetric_.scaleHeight;
    vf.valley = volumetric_.valley;
    vf.boundaryDensity = volumetric_.boundaryDensity;
    vf.boundaryHeight = volumetric_.boundaryHeight;
    vf.noiseScale = volumetric_.noiseScale;
    vf.anisotropy = volumetric_.anisotropy;
    vf.historyWeight = volumetric_.historyWeight;
    // Halton (base 2) slice offsets: every frame samples the slices at a different depth.
    uint32_t n = ++volumeFrameIndex_ % 64 + 1;
    float halton = 0.0f, scale = 0.5f;
    for (; n; n >>= 1, scale *= 0.5f) halton += (n & 1) * scale;
    vf.jitter = halton - 0.5f;
    volumeCurrent_ ^= 1u;
    vf.injected = volumeStorage_[volumeCurrent_];
    vf.history = volumeSampled_[volumeCurrent_ ^ 1u];
    vf.integrated = volumeIntegratedStorage_;
    vf.historyValid = volumeHistoryValid_ ? 1u : 0u;
    vf.meanGround = volumetric_.meanGround;
  } else {
    volumeHistoryValid_ = false;
  }

  // Local lights through the fog of war (R7): a light reaches as far as its source is in sight. A torch or a
  // spell out of sight lights nothing, so light never reveals what the party cannot see.
  // Ground marks: every hex's layers cleared, then filled in the order given.
  if (markCols_ > 0 && markRows_ > 0 && static_cast<uint32_t>(markCols_ * markRows_) <= kMaxMarkCells) {
    auto* slots = static_cast<uint32_t*>(frame.marks.mapped);
    const size_t cells = static_cast<size_t>(markCols_) * static_cast<size_t>(markRows_);
    std::memset(slots, 0, cells * kMarkLayers * 2 * sizeof(uint32_t));
    std::vector<uint8_t> used(cells, 0);
    for (const GroundMark& m : marks_) {
      if (m.cell >= cells || used[m.cell] >= kMarkLayers || (m.color >> 24) == 0) continue;
      uint32_t* slot = slots + (static_cast<size_t>(m.cell) * kMarkLayers + used[m.cell]++) * 2;
      slot[0] = m.color;
      slot[1] = packHalf2(m.rOut, m.rIn);
    }
  }
  // The battle grid's cells, as given (the rest of the board cleared).
  if (gridOn()) {
    auto* cells = static_cast<GridCell*>(frame.grid.mapped);
    const size_t count = static_cast<size_t>(gridCols_) * static_cast<size_t>(gridRows_);
    std::memset(cells, 0, count * sizeof(GridCell));
    std::memcpy(cells, grid_.data(), std::min(grid_.size(), count) * sizeof(GridCell));
  }

  uint32_t lightCount = 0;
  auto* lights = static_cast<LocalLight*>(frame.lights.mapped);
  for (const LocalLight& light : lights_) {
    const float sight = f.fogMask != kNoTextureIndex ? fogSight(light.position) : 1.0f;
    if (sight <= 0.0f) continue;
    lights[lightCount] = light;
    lights[lightCount++].color *= sight;
  }
  f.lightCount = lightCount;
  f.shadowMap = kNoTextureIndex;
  if (shadowRadius_ > 0.0f && terrain_) {
    // An orthographic sun camera around the board's bounding sphere, pulled back along the sun direction.
    const glm::vec3 sun = glm::normalize(lighting_.sunDirection);
    const glm::vec3 up = std::abs(sun.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const glm::mat4 view = glm::lookAt(shadowCenter_ + sun * shadowRadius_ * 2.0f, shadowCenter_, up);
    const glm::mat4 projection = glm::ortho(-shadowRadius_, shadowRadius_, -shadowRadius_, shadowRadius_, 0.0f, shadowRadius_ * 4.0f);
    f.shadowMatrix = projection * view;
    f.shadowMap = shadowMapIndex_;
    f.shadowSampler = shadowSamplerIndex_;
    f.shadowSize = static_cast<float>(shadowSize_);
    f.shadowTexelWorld = 2.0f * shadowRadius_ / static_cast<float>(shadowSize_);
    f.shadowFilterTexels = shadowFilterTexels_;
  }
  // The static board's shadow is cached: drawn again only when the sun, the bounds or the ground change.
  const bool shadows = f.shadowMap != kNoTextureIndex && (lightingLayers_ & 2u) != 0;
  const std::array<float, 8> key{lighting_.sunDirection.x, lighting_.sunDirection.y, lighting_.sunDirection.z, shadowCenter_.x,
                                 shadowCenter_.y, shadowCenter_.z, shadowRadius_, static_cast<float>(terrain_.vertices % 1000003u)};
  // The 3D pieces belong to the cached map too: a new instance list (or new meshes) redraws it.
  const uint64_t meshKey = meshView_ ? (meshView_.instancesVersion * 1000003ull) ^ meshView_.meshes ^ meshView_.meshCount : 0;
  // Never-seen ground and pieces cast no shadow (R7): new sight redraws it too.
  const uint64_t fogKey = f.fogMask != kNoTextureIndex ? fogVersion_ : 0;
  shadowsThisFrame_ = shadows && (key != shadowKey_ || terrain_.vertices != shadowTerrain_ || meshKey != shadowMeshKey_ || fogKey != shadowFogVersion_);
  if (shadowsThisFrame_) {
    shadowFogVersion_ = fogKey;
    shadowKey_ = key;
    shadowTerrain_ = terrain_.vertices;
    shadowMeshKey_ = meshKey;
  }
  // Cards cast into their own map, every frame (units move and animate).
  cardShadowsThisFrame_ = shadows && std::any_of(sprites_.begin(), sprites_.end(), [](const SpriteInstance& c) { return (c.flags & kSpriteCastsShadow) != 0; });
  f.cardShadowMap = cardShadowsThisFrame_ ? cardShadowMapIndex_ : kNoTextureIndex;
  // The spell light's shadow: a perspective map looking straight down from the light, wide enough to reach its range on the
  // ground (at most 160 degrees), cards drawn into it every frame. The shaded surfaces find the light by its position.
  f.localShadowMap = kNoTextureIndex;
  localShadowThisFrame_ = false;
  if (localShadowLight_ && (lightingLayers_ & 2u) != 0 && !sprites_.empty() &&
      std::any_of(lights_.begin(), lights_.end(), [&](const LocalLight& l) { return glm::distance(l.position, *localShadowLight_) < 1e-3f; })) {
    const glm::vec3 at = *localShadowLight_;
    const float height = std::max(at.y - localShadowGround_, 0.3f);
    const float fov = std::min(2.0f * std::atan(std::max(localShadowRange_, 1.0f) / height), glm::radians(160.0f));
    const glm::mat4 view = glm::lookAt(at, at - glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    const float nearPlane = std::min(0.2f, height * 0.5f);
    localShadowMatrix_ = glm::perspective(fov, 1.0f, nearPlane, std::max(localShadowRange_, height + 1.0f) * 1.5f) * view;
    f.localShadowMatrix = localShadowMatrix_;
    f.localShadowPosition = at;
    f.localShadowMap = localShadowMapIndex_;
    f.localShadowSize = static_cast<float>(kLocalShadowSize);
    localShadowThisFrame_ = true;
  }
  if (shadowsThisFrame_ && meshView_) {
    // The meshes' pass from the sun: its orthographic view, frustum only, LOD at the shadow map's texel size.
    MeshFrame sf = f;
    const glm::vec3 sun = glm::normalize(lighting_.sunDirection);
    sf.viewProjection = f.shadowMatrix;
    frustumPlanes(sf.viewProjection, sf.frustum);
    sf.cameraPosition = shadowCenter_ + sun * shadowRadius_ * 2.0f;
    sf.lodScale = 2.0f * shadowRadius_ / f.shadowTexelWorld;
    sf.shadowPass = 1;
    sf.earlyOcclusion = 0;
    sf.costSplit = 0;
    sf.debugView = 0;
    static_assert(sizeof(sf) == sizeof(shadowFrameStorage_));
    std::memcpy(shadowFrameStorage_.data(), &sf, sizeof(sf));
  }
  // The previous frame's passes may still read these buffers.
  memoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
  vkCmdUpdateBuffer(cmd, frame.meshFrame.buffer, 0, sizeof(f), &f);
  if (volumeThisFrame_) vkCmdUpdateBuffer(cmd, frame.volumeFrame.buffer, 0, sizeof(vf), &vf);
  // The fog bank: only over a fog-of-war mask.
  bankThisFrame_ = fogBank_.enabled && f.fogMask != kNoTextureIndex && !fogLooks().empty();
  if (bankThisFrame_) {
    bankLook_ = std::min<uint32_t>(fogBank_.look, static_cast<uint32_t>(fogLooks().size()) - 1);
    const FogLook& look = fogLooks()[bankLook_];
    FogLookInputs in;
    in.inverseViewProjection = glm::inverse(camera_.viewProjection());
    in.time = time;
    in.frameIndex = ++bankFrameIndex_;
    in.noise = bankNoiseIndex_[static_cast<size_t>(look.noise)];
    in.depth = depthIndex_;
    in.march = bankMarchStorage_;
    in.marchSampled = bankMarchSampled_;
    in.color = sceneColorStorageIndex_;
    in.halfSize = {(size_.width + 1) / 2, (size_.height + 1) / 2};
    in.fullSize = {size_.width, size_.height};
    in.scene = fogBank_.scene;
    alignas(16) std::array<std::byte, kMaxFogLookFrameBytes> record{};
    look.write(in, record.data());
    vkCmdUpdateBuffer(cmd, frame.cloudFrame.buffer, 0, look.frameBytes, record.data());
  }
  if (shadowsThisFrame_ && meshView_) vkCmdUpdateBuffer(cmd, frame.shadowFrame.buffer, 0, sizeof(MeshFrame), shadowFrameStorage_.data());
  vkCmdFillBuffer(cmd, meshWork_.buffer, 0, kWorkHeaderBytes, 0);
  // A new instance list: last frame's verdicts belong to other instances; everything starts unknown
  // (not visible), so the late pass tests it all against the pyramid.
  if (meshView_.instancesVersion != visibilityVersion_) {
    vkCmdFillBuffer(cmd, instanceVisibility_.buffer, 0, VK_WHOLE_SIZE, 0);
    visibilityVersion_ = meshView_.instancesVersion;
  }
  memoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT |
                    VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

// Instances (early or late), then the BVH traversal one level per pass, then the draw arguments.
void Renderer::cullPass(VkCommandBuffer cmd, VkDeviceAddress frameAddress, VkPipeline instances, uint32_t drawPass, uint32_t instanceCount) {
  const auto computeToCompute = [&] {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
  };
  CullPush push{frameAddress, 0};
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, instances);
  vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
  vkCmdDispatch(cmd, (instanceCount + 63) / 64, 1, 1);
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
}

void Renderer::recordMeshes(Frame& frame, VkCommandBuffer cmd) {
  const uint32_t instanceCount = std::min(meshView_.instanceCount, kMaxInstances);
  const VkDeviceAddress frameAddress = bufferAddress(context_, frame.meshFrame);
  // --- Early: what was visible last frame ---------------------------------------------------
  uint32_t scope = gpuTimer_.begin(cmd, "cull early");
  cullPass(cmd, frameAddress, cullInstancesPipeline_, 0, instanceCount);
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
  cullPass(cmd, frameAddress, cullInstancesLatePipeline_, 1, instanceCount);
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
  const ResolvePush resolvePush{frameAddress, visibilityIndex_, sceneColorStorageIndex_, fogOn_ ? kNeverSeenColor : kClearColor, background_,
                                backgroundToHorizon_ ? 1u : 0u, {}};
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

// Engine2's bloom as a mip pyramid (shaders/bloom.hlsl): the part of the scene above the threshold comes down five levels,
// then goes back up with a tent filter, each level adding its own share; level 0 is the finished bloom the tonemap adds.
void Renderer::recordBloom(VkCommandBuffer cmd) {
  // Last frame's tonemap sampled the pyramid; it is rebuilt from scratch.
  transition(cmd, bloomImage_.image, bloomImage_.aspect,
             {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
  auto levelSize = [&](uint32_t level) { return VkExtent2D{std::max(1u, bloomSize_.width >> level), std::max(1u, bloomSize_.height >> level)}; };
  auto barrier = [&](VkPipelineStageFlags2 nextStage) {
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, nextStage,
                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  };
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomDownPipeline_);
  for (uint32_t level = 0; level < kBloomLevels; ++level) {
    const VkExtent2D size = levelSize(level);
    const VkExtent2D from = level == 0 ? size_ : levelSize(level - 1);
    const BloomPush push{level == 0 ? sceneColorIndex_ : bloomSampledIndex_, linearClampIndex_, bloomLevelIndices_[level],
                         level == 0 ? -1.0f : static_cast<float>(level - 1), size.width, size.height,
                         1.0f / static_cast<float>(from.width), 1.0f / static_cast<float>(from.height), 1.0f, 1.0f};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (size.width + 7) / 8, (size.height + 7) / 8, 1);
    barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
  }
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomUpPipeline_);
  for (uint32_t level = kBloomLevels - 1; level-- > 0;) {
    const VkExtent2D size = levelSize(level), from = levelSize(level + 1);
    const BloomPush push{bloomSampledIndex_, linearClampIndex_, bloomLevelIndices_[level], static_cast<float>(level + 1), size.width, size.height,
                         1.0f / static_cast<float>(from.width), 1.0f / static_cast<float>(from.height), kBloomWeights[level],
                         level + 2 == kBloomLevels ? kBloomWeights[kBloomLevels - 1] : 1.0f};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (size.width + 7) / 8, (size.height + 7) / 8, 1);
    barrier(level == 0 ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
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
  // This frame's cards (host-visible): read by the card shadow pass and the scene pass.
  if (hasScene_ && !sprites_.empty()) std::memcpy(frame.sprites.mapped, sprites_.data(), sprites_.size() * sizeof(SpriteInstance));
  if (hasScene_ && !frontCards_.empty())
    std::memcpy(static_cast<SpriteInstance*>(frame.sprites.mapped) + sprites_.size(), frontCards_.data(), frontCards_.size() * sizeof(SpriteInstance));
  if (hasScene_ && !effects_.empty()) std::memcpy(frame.effects.mapped, effects_.data(), effects_.size() * sizeof(EffectInstance));
  if (hasScene_ && !vfxDraws_.empty()) {
    std::memcpy(frame.vfxVertices.mapped, vfxVertices_.data(), vfxVertices_.size() * sizeof(VfxVertex));
    if (!vfxInstances_.empty()) std::memcpy(frame.vfxInstances.mapped, vfxInstances_.data(), vfxInstances_.size() * sizeof(VfxInstance));
    auto* draws = static_cast<VfxDrawGpu*>(frame.vfxDraws.mapped);
    for (size_t i = 0; i < vfxDraws_.size(); ++i) {
      draws[i] = vfxDraws_[i].gpu;
      draws[i].flags = (draws[i].flags & ~1u) | (vfxDraws_[i].blend == VfxBlend::Additive ? 1u : 0u);
    }
  }
  // --- Fog bank noise: made once, uploaded on first use --------------------------------------------------
  if (fogBank_.enabled && !fogLooks().empty()) {
    const size_t kind = static_cast<size_t>(fogLooks()[std::min<size_t>(fogBank_.look, fogLooks().size() - 1)].noise);
    if (!bankNoiseUploaded_[kind]) {
      bankNoiseUploaded_[kind] = true;
      const std::vector<uint8_t> noise = makeFogNoise(static_cast<FogNoise>(kind));
      bankNoiseStaging_[kind] = createHostBuffer(context_, noise.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
      std::memcpy(bankNoiseStaging_[kind].mapped, noise.data(), noise.size());
      Image& image = bankNoise_[kind];
      transition(cmd, image.image, image.aspect,
                 {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                  VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});
      VkBufferImageCopy copy{};
      copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      copy.imageExtent = {kFogNoiseSize, kFogNoiseSize, kFogNoiseSize};
      vkCmdCopyBufferToImage(cmd, bankNoiseStaging_[kind].buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      transition(cmd, image.image, image.aspect,
                 {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    }
  }
  // --- Fog of war: a new mask goes up before any pass reads it ------------------------------------------
  if (fogOn_ && fogUploadPending_) {
    fogUploadPending_ = false;
    const VkExtent2D extent{static_cast<uint32_t>(fog_.width), static_cast<uint32_t>(fog_.height)};
    if (fogMask_.extent.width != extent.width || fogMask_.extent.height != extent.height) {
      // A new board: the old mask may still be read by frames in flight.
      vkDeviceWaitIdle(context_.device);
      if (fogMaskIndex_ != kNoIndex) bindless_.removeSampledImage(fogMaskIndex_);
      destroyImage(context_, fogMask_);
      fogMask_ = createImage(context_, {VK_FORMAT_R16G16B16A16_SFLOAT, extent, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT});
      fogMaskIndex_ = bindless_.addSampledImage(fogMask_.view);
    }
    // Per texel, half floats: darkness (the tactical mask), then the volumetric fog's boundary, ground, valley.
    const size_t texels = fog_.alpha.size();
    if (frame.fogStaging.size < texels * 8) {
      destroyBuffer(context_, frame.fogStaging);
      frame.fogStaging = createHostBuffer(context_, texels * 8, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    }
    std::memcpy(frame.fogStaging.mapped, fogTexels_.data(), texels * 8);  // packed by setFogOfWar / fogTexels
    transition(cmd, fogMask_.image, fogMask_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {extent.width, extent.height, 1};
    vkCmdCopyBufferToImage(cmd, frame.fogStaging.buffer, fogMask_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    transition(cmd, fogMask_.image, fogMask_.aspect,
               {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
  }

  // --- 3D meshes: cull, pick cluster LODs, rasterise visibility + depth, resolve materials -----
  const bool meshes = hasScene_ && static_cast<bool>(meshView_);
  if (hasScene_) writeFrame(frame, cmd, time);
  // --- Local lights: each cluster's list (read by every lit surface) -------------------------------
  if (hasScene_ && !lights_.empty()) {
    const uint32_t lightScope = gpuTimer_.begin(cmd, "light clusters");
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lightsPipeline_);
    const LightsPush push{bufferAddress(context_, frame.meshFrame)};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (kClusterCount + 63) / 64, 1, 1);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    gpuTimer_.end(cmd, lightScope);
  }
  // --- The atmosphere: its tables and SH ambient, rebuilt every frame (read by every lit surface, the sky) ------
  if (hasScene_ && atmoThisFrame_) {
    const uint32_t atmoScope = gpuTimer_.begin(cmd, "atmosphere");
    if (!atmoReady_) {
      atmoReady_ = true;
      for (Image* table : {&atmoTransmittance_, &atmoMultiScattering_, &atmoSkyView_, &atmoAerial_})
        transition(cmd, table->image, table->aspect,
                   {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    }
    // Last frame's surfaces may still read the tables and the ambient.
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    const VolumePush push{bufferAddress(context_, frame.meshFrame), bufferAddress(context_, frame.atmosphereFrame)};
    const auto pass = [&](int i, uint32_t x, uint32_t y, uint32_t z) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, atmoPipelines_[i]);
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
      vkCmdDispatch(cmd, x, y, z);
      memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    };
    pass(0, 256 / 8, 64 / 8, 1);    // transmittance
    pass(1, 32 / 8, 32 / 8, 1);     // multiple scattering (reads transmittance)
    pass(2, 192 / 8, 108 / 8 + 1, 1);  // sky view (reads both)
    pass(3, 32 / 8, 32 / 8, 1);     // aerial perspective
    pass(4, 1, 1, 1);               // SH ambient (reads the sky view)
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    gpuTimer_.end(cmd, atmoScope);
  }
  // --- The cloud shadow map over the board (clouds.hlsli): the sun through the low sheet, once a frame ----------------------
  if (hasScene_ && atmoThisFrame_ && cloudShadowThisFrame_) {
    const uint32_t scope = gpuTimer_.begin(cmd, "cloud shadow");
    if (!cloudShadowReady_) {
      cloudShadowReady_ = true;
      transition(cmd, cloudShadow_.image, cloudShadow_.aspect,
                 {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    }
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cloudShadowPipeline_);
    uint32_t heightBits;
    std::memcpy(&heightBits, &cloudShadowHeight_, sizeof(heightBits));
    const SkyReflectPush push{bufferAddress(context_, frame.meshFrame), cloudShadowStorage_, heightBits, kCloudShadowSize, kCloudShadowSize};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, kCloudShadowSize / 8, kCloudShadowSize / 8, 1);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    gpuTimer_.end(cmd, scope);
  }
  // --- The sky's reflection map: the sky (clouds in) at level 0, then each level prefiltered for a rougher surface ----
  if (hasScene_ && skyReflectThisFrame_) {
    const uint32_t scope = gpuTimer_.begin(cmd, "sky reflect");
    if (!skyReflectReady_) {
      skyReflectReady_ = true;
      transition(cmd, skyReflect_.image, skyReflect_.aspect,
                 {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    }
    // Last frame's surfaces may still read the map.
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    for (uint32_t level = 0; level < kSkyReflectLevels; ++level) {
      const uint32_t width = std::max(1u, kSkyReflectSize.width >> level), height = std::max(1u, kSkyReflectSize.height >> level);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, level == 0 ? skyReflectBasePipeline_ : skyReflectFilterPipeline_);
      const SkyReflectPush push{bufferAddress(context_, frame.meshFrame), skyReflectLevelIndices_[level], level, width, height};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
      vkCmdDispatch(cmd, (width + 7) / 8, (height + 7) / 8, 1);
      memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    level + 1 == kSkyReflectLevels ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                                                   : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
    gpuTimer_.end(cmd, scope);
  }
  // --- Sun shadow: the board's ground from the sun (before the meshes: their resolve reads it) ---------------------------------------------
  if (shadowsThisFrame_) {
    const uint32_t shadowScope = gpuTimer_.begin(cmd, "shadow");
    const VkDeviceAddress shadowFrameAddress = bufferAddress(context_, frame.shadowFrame);
    if (meshes) cullPass(cmd, shadowFrameAddress, cullInstancesPipeline_, 0, std::min(meshView_.instanceCount, kMaxInstances));
    transition(cmd, shadowMap_.image, shadowMap_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
    VkRenderingAttachmentInfo shadowDepth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    shadowDepth.imageView = shadowMap_.view;
    shadowDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    shadowDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    shadowDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    shadowDepth.clearValue.depthStencil = {1.0f, 0};
    VkRenderingInfo shadowPass{VK_STRUCTURE_TYPE_RENDERING_INFO};
    shadowPass.renderArea = {{0, 0}, {shadowSize_, shadowSize_}};
    shadowPass.layerCount = 1;
    shadowPass.pDepthAttachment = &shadowDepth;
    vkCmdBeginRendering(cmd, &shadowPass);
    setViewport(cmd, {shadowSize_, shadowSize_});
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainShadowPipeline_);
    const TerrainPush push{bufferAddress(context_, frame.meshFrame), terrain_.record, terrain_.vertices, terrain_.indices, terrain_.seamStep, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDraw(cmd, terrain_.count, 1, 0, 0);
    if (meshes) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshShadowPipeline_);
      const MeshletPush meshPush{shadowFrameAddress, 0};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(meshPush), &meshPush);
      vkCmdDrawMeshTasksIndirectEXT(cmd, meshWork_.buffer, kDrawArgsOffset, 1, 12);
    }
    vkCmdEndRendering(cmd);
    if (meshes) {
      // The main pass starts from empty counters and queues again.
      memoryBarrier(cmd, VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                    VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
      vkCmdFillBuffer(cmd, meshWork_.buffer, 0, kWorkHeaderBytes, 0);
      memoryBarrier(cmd, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    transition(cmd, shadowMap_.image, shadowMap_.aspect,
               {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    gpuTimer_.end(cmd, shadowScope);
  }
  // --- Card shadows: every card that casts, every frame -----------------------------------------
  if (cardShadowsThisFrame_) {
    const uint32_t cardScope = gpuTimer_.begin(cmd, "card shadow");
    transition(cmd, cardShadowMap_.image, cardShadowMap_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
    VkRenderingAttachmentInfo cardDepth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    cardDepth.imageView = cardShadowMap_.view;
    cardDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    cardDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    cardDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    cardDepth.clearValue.depthStencil = {1.0f, 0};
    VkRenderingInfo cardPass{VK_STRUCTURE_TYPE_RENDERING_INFO};
    cardPass.renderArea = {{0, 0}, {shadowSize_, shadowSize_}};
    cardPass.layerCount = 1;
    cardPass.pDepthAttachment = &cardDepth;
    vkCmdBeginRendering(cmd, &cardPass);
    setViewport(cmd, {shadowSize_, shadowSize_});
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteShadowPipeline_);
    const SpritePush push{camera_.viewProjection(), camera_.right, spriteSamplerIndex_, bufferAddress(context_, frame.sprites),
                          bufferAddress(context_, frame.meshFrame), camera_.forward, cardLean_ ? 1u : 0u};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDraw(cmd, 6, static_cast<uint32_t>(sprites_.size()), 0, 0);
    vkCmdEndRendering(cmd);
    transition(cmd, cardShadowMap_.image, cardShadowMap_.aspect,
               {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    gpuTimer_.end(cmd, cardScope);
  }
  // --- The spell light's card shadows (setLocalShadow): the same cards, seen from the light (VSShadow, lean bit 2) ---
  if (localShadowThisFrame_) {
    const uint32_t localScope = gpuTimer_.begin(cmd, "spell shadow");
    transition(cmd, localShadowMap_.image, localShadowMap_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
    VkRenderingAttachmentInfo localDepth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    localDepth.imageView = localShadowMap_.view;
    localDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    localDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    localDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    localDepth.clearValue.depthStencil = {1.0f, 0};
    VkRenderingInfo localPass{VK_STRUCTURE_TYPE_RENDERING_INFO};
    localPass.renderArea = {{0, 0}, {kLocalShadowSize, kLocalShadowSize}};
    localPass.layerCount = 1;
    localPass.pDepthAttachment = &localDepth;
    vkCmdBeginRendering(cmd, &localPass);
    setViewport(cmd, {kLocalShadowSize, kLocalShadowSize});
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteShadowPipeline_);
    const SpritePush push{camera_.viewProjection(), camera_.right, spriteSamplerIndex_, bufferAddress(context_, frame.sprites),
                          bufferAddress(context_, frame.meshFrame), camera_.forward, 2u};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDraw(cmd, 6, static_cast<uint32_t>(sprites_.size()), 0, 0);
    vkCmdEndRendering(cmd);
    transition(cmd, localShadowMap_.image, localShadowMap_.aspect,
               {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    gpuTimer_.end(cmd, localScope);
  }

  // --- Atmosfera volumétrica v1: its froxels lit and integrated (shadows, clusters and the fog mask are ready) ---------
  // Integration point 3 of 3 (docs/ATMOSPHERE.md §4).
  if (hasScene_ && volumetricsThisFrame_) {
    const uint32_t scope = gpuTimer_.begin(cmd, "volumetrics");
    volumetrics_->record(cmd, bufferAddress(context_, frame.meshFrame), frameIndex_);
    gpuTimer_.end(cmd, scope);
  }

  // --- Volumetric fog: froxels lit (sun shadows, clusters and the fog mask are ready), then integrated ---------
  if (hasScene_ && volumeThisFrame_) {
    const uint32_t volumeScope = gpuTimer_.begin(cmd, "volume");
    if (!volumesReady_) {
      volumesReady_ = true;
      for (Image* v : {&volumes_[0], &volumes_[1], &volumeIntegrated_})
        transition(cmd, v->image, v->aspect,
                   {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    }
    // The previous frame's surfaces and passes may still read what is written now.
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    const VolumePush push{bufferAddress(context_, frame.meshFrame), bufferAddress(context_, frame.volumeFrame)};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, volumeInjectPipeline_);
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (kFroxelX + 7) / 8, (kFroxelY + 7) / 8, kFroxelZ);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, volumeIntegratePipeline_);
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (kFroxelX + 7) / 8, (kFroxelY + 7) / 8, 1);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    volumeHistoryValid_ = true;
    gpuTimer_.end(cmd, volumeScope);
  }

  if (meshes) {
    recordMeshes(frame, cmd);
    transition(cmd, sceneColor_.image, sceneColor_.aspect,
               {VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
  } else if (hasScene_ && (atmoThisFrame_ || background_ != kNoTextureIndex)) {
    // --- No 3D meshes, so no resolve: the sky alone, then the ground and cards over it -------------------------
    // The previous frame may still be sampling scene colour in its tonemap pass.
    transition(cmd, sceneColor_.image, sceneColor_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
    const uint32_t skyScope = gpuTimer_.begin(cmd, "sky");
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, skyPipeline_);
    const ResolvePush skyPush{bufferAddress(context_, frame.meshFrame), visibilityIndex_, sceneColorStorageIndex_, kClearColor, background_,
                              backgroundToHorizon_ ? 1u : 0u, {}};
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(skyPush), &skyPush);
    vkCmdDispatch(cmd, (size_.width + 7) / 8, (size_.height + 7) / 8, 1);
    gpuTimer_.end(cmd, skyScope);
    transition(cmd, sceneColor_.image, sceneColor_.aspect,
               {VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
    transition(cmd, depth_.image, depth_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
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
  const bool skyAlone = !meshes && hasScene_ && (atmoThisFrame_ || background_ != kNoTextureIndex);
  color.loadOp = meshes || skyAlone ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;  // keep the resolved 3D / sky
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  const glm::vec4 background = fogOn_ ? kNeverSeenColor : kClearColor;
  color.clearValue.color = {{background.r, background.g, background.b, background.a}};
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
  // Spell effects this frame: they draw over every unit's card (the units' depth is written after them, below).
  const bool effects = hasScene_ && (!effects_.empty() || !vfxDraws_.empty() || !frontCards_.empty());
  if (hasScene_) {
    const glm::mat4 viewProjection = camera_.viewProjection();
    if (terrain_) {
      // The board's ground (Engine2's terrain), depth-tested against the 3D meshes.
      const uint32_t terrainScope = gpuTimer_.begin(cmd, "terrain");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipeline_);
      const bool marks = markCols_ > 0 && markRows_ > 0 && static_cast<uint32_t>(markCols_ * markRows_) <= kMaxMarkCells;
      const bool grid = gridOn();
      const TerrainPush terrainPush{bufferAddress(context_, frame.meshFrame), terrain_.record, terrain_.vertices, terrain_.indices, terrain_.seamStep,
                                    marks ? static_cast<uint32_t>(markCols_) : 0u, marks ? bufferAddress(context_, frame.marks) : 0,
                                    static_cast<uint32_t>(markRows_),
                                    grid ? static_cast<uint32_t>(gridCols_) : 0u, grid ? static_cast<uint32_t>(gridRows_) : 0u, grid ? gridLook_ : 0u,
                                    grid ? bufferAddress(context_, frame.grid) : 0, gridSeconds_, 0};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(terrainPush), &terrainPush);
      vkCmdDraw(cmd, terrain_.count, 1, 0, 0);
      gpuTimer_.end(cmd, terrainScope);
    } else {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, groundPipeline_);
      const GroundPush groundPush{viewProjection, groundHalfExtent_};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(groundPush), &groundPush);
      vkCmdDraw(cmd, 6, 1, 0, 0);
    }
    if (terrain_ && water_) {
      // The board's water: transparent, over the ground, under the cards.
      const uint32_t waterScope = gpuTimer_.begin(cmd, "water");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipeline_);
      const WaterPush waterPush{bufferAddress(context_, frame.meshFrame), water_.vertices, water_.indices, water_.look, 0};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(waterPush), &waterPush);
      vkCmdDraw(cmd, water_.count, 1, 0, 0);
      gpuTimer_.end(cmd, waterScope);
    }
    if (terrain_ && water3d_) {
      // Ember's 3D water (the Map Editor's Agua 3D): transparent, over the ground and the board's water, under the cards.
      const uint32_t scope = gpuTimer_.begin(cmd, "water3d");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, water3dPipeline_);
      const Water3DView& w = water3d_;
      const Water3DPush push{bufferAddress(context_, frame.meshFrame), w.vertices, w.indices, w.colorMap, w.normalMap, w.roughMap,
                             {w.color[0], w.color[1], w.color[2]}, w.opacity, w.normalScale, w.roughness, w.modern, w.v4, w.repeat,
                             w.look};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
      vkCmdDraw(cmd, w.count, 1, 0, 0);
      gpuTimer_.end(cmd, scope);
    }
    if (!sprites_.empty()) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipeline_);
      const SpritePush spritePush{viewProjection, camera_.right, spriteSamplerIndex_, bufferAddress(context_, frame.sprites),
                                  bufferAddress(context_, frame.meshFrame), camera_.forward, cardLean_ ? 1u : 0u};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(spritePush), &spritePush);
      vkCmdDraw(cmd, 6, static_cast<uint32_t>(sprites_.size()), 0, 0);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteDepthPipeline_);
      if (effects) {  // the units' cards later, after the effects
        SpritePush depthPush = spritePush;
        depthPush.lean |= 4u;
        vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(depthPush), &depthPush);
      }
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

  // --- Spell effects: over the finished surfaces, depth read-only (tested and sampled for the soft fade) -----------
  // The units' cards are not in the depth yet: every effect draws over the characters (the user, 2026-10-18).
  if (effects) {
    const uint32_t effectScope = gpuTimer_.begin(cmd, "effects");
    transition(cmd, depth_.image, depth_.aspect,
               {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    VkRenderingAttachmentInfo fxColor{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    fxColor.imageView = sceneColor_.view;
    fxColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    fxColor.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    fxColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingAttachmentInfo fxDepth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    fxDepth.imageView = depth_.view;
    fxDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    fxDepth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    fxDepth.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
    VkRenderingInfo fxPass{VK_STRUCTURE_TYPE_RENDERING_INFO};
    fxPass.renderArea = {{0, 0}, size_};
    fxPass.layerCount = 1;
    fxPass.colorAttachmentCount = 1;
    fxPass.pColorAttachments = &fxColor;
    fxPass.pDepthAttachment = &fxDepth;
    vkCmdBeginRendering(cmd, &fxPass);
    setViewport(cmd, size_);
    if (!effects_.empty()) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, effectPipeline_);
      const EffectPush effectPush{bufferAddress(context_, frame.effects), bufferAddress(context_, frame.meshFrame), spriteSamplerIndex_,
                                  depthIndex_, effectBrightness_, 0};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(effectPush), &effectPush);
      vkCmdDraw(cmd, 6, static_cast<uint32_t>(effects_.size()), 0, 0);
    }
    // Engine2's ported effects, in the order the effects gave them (three's render order).
    VkPipeline bound = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < vfxDraws_.size(); ++i) {
      const VfxDraw& d = vfxDraws_[i];
      const VkPipeline pipeline = vfxPipelines_[d.depthTest ? 1 : 0];
      if (pipeline != bound) vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bound = pipeline);
      const VfxPush push{bufferAddress(context_, frame.vfxVertices), bufferAddress(context_, frame.vfxInstances), bufferAddress(context_, frame.vfxDraws),
                         bufferAddress(context_, frame.meshFrame), i, effectBrightness_, spriteSamplerIndex_, 0};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
      vkCmdDraw(cmd, d.gpu.vertexCount, d.gpu.firstInstance == 0xFFFFFFFFu ? 1u : d.instances, 0, 0);
    }
    // Cards over the effects (a caster turned away, in front of what leaves its hands): the same card drawing, read past
    // the frame's cards.
    if (!frontCards_.empty()) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipeline_);
      const SpritePush frontPush{camera_.viewProjection(), camera_.right, spriteSamplerIndex_,
                                 bufferAddress(context_, frame.sprites) + sprites_.size() * sizeof(SpriteInstance), bufferAddress(context_, frame.meshFrame),
                                 camera_.forward, cardLean_ ? 1u : 0u};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(frontPush), &frontPush);
      vkCmdDraw(cmd, 6, static_cast<uint32_t>(frontCards_.size()), 0, 0);
    }
    vkCmdEndRendering(cmd);
    // Now the units' depth (left out of the scene's card-depth pass), so the fog bank stops at a character again.
    if (!sprites_.empty()) {
      transition(cmd, depth_.image, depth_.aspect,
                 {VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                  VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                  VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
      fxDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
      fxDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      vkCmdBeginRendering(cmd, &fxPass);
      setViewport(cmd, size_);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteDepthPipeline_);
      const SpritePush unitDepth{camera_.viewProjection(), camera_.right, spriteSamplerIndex_, bufferAddress(context_, frame.sprites),
                                 bufferAddress(context_, frame.meshFrame), camera_.forward, (cardLean_ ? 1u : 0u) | 8u};
      vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(unitDepth), &unitDepth);
      vkCmdDraw(cmd, 6, static_cast<uint32_t>(sprites_.size()), 0, 0);
      vkCmdEndRendering(cmd);
    }
    gpuTimer_.end(cmd, effectScope);
  }
  const bool depthReadOnly = effects && sprites_.empty();  // the depth's layout after the effects

  // --- Fog bank (fog of war): marched at half resolution, composited over the finished scene ---------------
  const bool bank = hasScene_ && bankThisFrame_;
  if (bank) {
    const uint32_t bankScope = gpuTimer_.begin(cmd, "fog bank");
    transition(cmd, sceneColor_.image, sceneColor_.aspect,
               {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
    transition(cmd, depth_.image, depth_.aspect,
               {depthReadOnly ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    // Last frame's composite may still read the march result.
    transition(cmd, bankMarch_.image, bankMarch_.aspect,
               {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
    const VolumePush push{bufferAddress(context_, frame.meshFrame), bufferAddress(context_, frame.cloudFrame)};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bankMarchPipelines_[bankLook_]);
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, ((size_.width + 1) / 2 + 7) / 8, ((size_.height + 1) / 2 + 7) / 8, 1);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bankCompositePipelines_[bankLook_]);
    vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (size_.width + 7) / 8, (size_.height + 7) / 8, 1);
    gpuTimer_.end(cmd, bankScope);
  }

  // --- Tonemap pass: HDR -> display ---------------------------------------------------------
  transition(cmd, sceneColor_.image, sceneColor_.aspect,
             {bank ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              bank ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              bank ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
  const bool bloomOn = debugView_ == 0 && bloom_ > 0.0f;
  if (bloomOn) {
    const uint32_t bloomScope = gpuTimer_.begin(cmd, "bloom");
    recordBloom(cmd);
    gpuTimer_.end(cmd, bloomScope);
  }
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
  const TonemapPush tonemapPush{sceneColorIndex_, linearClampIndex_, debugView_ != 0 ? -1.0f : 1.0f, debugView_ != 0 ? 0.0f : bloom_, bloomSampledIndex_};
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
