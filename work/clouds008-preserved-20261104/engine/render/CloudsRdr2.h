#pragma once

// Nuvens RDR2 (2026-11-04; the user's pasted prompt "nuvens volumétricas 3D no estilo Red Dead Redemption 2, qualidade máxima"
// + "faz"): the clouds of Rockstar's SIGGRAPH 2019 talk (Fabian Bauer, "Creating the Atmospheric World of Red Dead
// Redemption 2") as the game's clouds. This class owns what the passes read and write (shaders/clouds_rdr2.hlsl):
//   the weather map (top-down: coverage, type, cirrus, growth), the march at the preset's resolution, the two resolved
//   images (this frame's and the history), the shadow map over the board (raw, blurred), the blue noise, the record.
// The shape and the light live in shaders/clouds_rdr2.hlsli / cloudsvol_rdr2.hlsli; the noise volumes are the ones
// cloudsvol.hlsl makes (the Renderer passes their indices). The Renderer calls prepare() while it writes the frame
// (the record's address goes into the AtmosphereFrame at byte 512, the resolved image into volMarch, the blurred shadow
// into cloudShadowMap) and record() after the sky tables and the ambient, before anything reads the shadows.
// Presets (the user's prompt: "o Low é o Ultra mais barato, nunca um visual diferente"): only the resolution, the step counts
// and the map sizes change; the shape, the types, the light and the shadow are the same.

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "render/Camera.h"
#include "rhi/Image.h"
#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;
class Bindless;
class GpuTimer;

// The record the shaders read by byte offset (clouds_rdr2.hlsli kR2*).
struct Rdr2Frame {
  uint32_t weatherMap = 0xFFFFFFFFu, noiseBase = 0xFFFFFFFFu, noiseDetail = 0xFFFFFFFFu, samplerRepeat = 0, samplerClamp = 0;
  uint32_t march = 0xFFFFFFFFu, history = 0xFFFFFFFFu, blueNoise = 0;  // 0..31 (blueNoise unused: the address below)
  glm::vec4 layer{0.0f};          // 32: base, top, anvil top, cirrus height (world y)
  glm::vec4 weather{0.0f};        // 48: coverage, cirrus coverage, type, growth
  glm::vec4 wind{0.0f};           // 64: wind x, wind z, time, rain
  glm::vec4 marchInfo{0.0f};      // 80: march width, height, frame index, quality
  glm::mat4 previousViewProj{1.0f};  // 96
  glm::vec4 steps{0.0f};          // 160: march steps, light taps, octaves, lightning
  glm::vec4 worldRect{0.0f};      // 176: weather map world rect x0, z0, 1 / w, 1 / h
  glm::vec4 flash{0.0f};          // 192: the flash: xyz, strength
  glm::vec4 screen{0.0f};         // 208: screen w, h, history valid, mean ground
  glm::vec4 shadow{0.0f};         // 224: shadow rect x0, z0, 1 / w, 1 / h
  glm::vec4 shadowInfo{0.0f};     // 240: shadow size, strength, height, light taps
  uint64_t blueNoiseAddress = 0;  // 256
  uint64_t pad = 0;               // 264
  glm::uvec4 textures{0};         // 272: raw depth, history depth, resolved depth, height strip
  glm::uvec4 maps{0};             // 288: height atlas, displacement, middle weather, water colour
  glm::uvec4 indirect{0};         // 304: cube array, previous water, shadow levels, ray stride
  glm::vec4 temporal{0};          // 320: elapsed time, valid history, indirect period, flash count
  glm::vec4 middle{0};            // 336: base, top, coverage, reserved
  glm::vec4 probeRect{0};         // 352: x0,z0,1/width,1/height
  uint64_t probes = 0;            // 368: 32 x 32 x 9 float4 SH irradiance, unscaled
  uint64_t ambient = 0;           // 376: reduced clear-sky illumination
  std::array<glm::vec4, 4> flashes{}; // 384: simultaneous lightning positions/strengths
  glm::vec4 controls{0};          // 448: shadow strength, shadow receiver height, reserved

};
static_assert(sizeof(Rdr2Frame) == 464 && offsetof(Rdr2Frame, layer) == 32 && offsetof(Rdr2Frame, previousViewProj) == 96 &&
              offsetof(Rdr2Frame, steps) == 160 && offsetof(Rdr2Frame, shadowInfo) == 240 && offsetof(Rdr2Frame, blueNoiseAddress) == 256 && offsetof(Rdr2Frame, textures) == 272 &&
              offsetof(Rdr2Frame, temporal) == 320 && offsetof(Rdr2Frame, probes) == 368 &&
              offsetof(Rdr2Frame, flashes) == 384 && offsetof(Rdr2Frame, controls) == 448);

class CloudsRdr2 {
 public:
  // What the clouds receive (the user's prompt §6): from the weather (sliders in the panel until a weather exists) and from
  // the caller's knowledge of the board.
  struct Params {
    int quality = 0;              // 0 Ultra (default), 1 High, 2 Medium, 3 Low
    float base = 380.0f, top = 640.0f, anvilTop = 1100.0f, cirrusHeight = 2600.0f;  // world y (the caller adds the mean ground)
    float coverage = 0.45f;       // the low layer, 0 clear .. 1 overcast
    float cirrusCoverage = 0.25f, middleCoverage = 0.15f;
    float kind = 0.65f;           // 0 stratus .. 1 cumulus
    float growth = 0.0f;          // 0 .. 1 towards the cumulonimbus
    float rain = 0.0f;            // 0 .. 1: darker, denser
    float lightning = 0.0f;       // 0 none .. 1 frequent
    glm::vec2 wind{8.0f, 3.0f};   // world units per second
    bool shadows = true;
    float shadowStrength = 0.75f;
    glm::vec4 shadowRect{0.0f};   // over the board: x0, z0, 1 / w, 1 / h (0 = no shadow map this frame)
    float shadowHeight = 0.0f;    // world y where the shadow lands
    float meanGround = 0.0f;
    float keyLux = 0.0f;          // the key light's illuminance (the flashes scale to it)
  };
  struct Output {
    VkDeviceAddress record = 0;
    uint32_t colour = 0xFFFFFFFFu;  // the resolved clouds (sampled): rgb radiance raw, a transmittance
    glm::vec2 size{1.0f};           // its size in texels
    uint32_t shadowMap = 0xFFFFFFFFu;  // the blurred shadow map (sampled), or none
  };
  struct Timing {  // the last frame's GPU scopes (the panel shows them as information)
    double weather = 0.0, march = 0.0, resolve = 0.0, shadow = 0.0, height = 0.0, ambient = 0.0, probes = 0.0, reflections = 0.0;
  };

  CloudsRdr2(VulkanContext& context, Bindless& bindless, uint32_t samplerRepeat, uint32_t samplerClamp);
  ~CloudsRdr2();
  CloudsRdr2(const CloudsRdr2&) = delete;
  CloudsRdr2& operator=(const CloudsRdr2&) = delete;

  Output prepare(VkCommandBuffer cmd, uint32_t frameIndex, const Camera& camera, glm::uvec2 screen, const Params& params, float time,
                 uint32_t noiseBase, uint32_t noiseDetail);
  void record(VkCommandBuffer cmd, VkDeviceAddress meshFrame, GpuTimer& timer);
  void invalidate() { historyValid_ = false; }
  const Timing& timing(const GpuTimer& timer);
  static constexpr const char* kQualityNames[4] = {"Ultra", "Alta", "Média", "Baixa"};

 private:
  struct Texture {
    Image image;
    uint32_t sampled = 0xFFFFFFFFu, storage = 0xFFFFFFFFu;
    uint32_t width = 0, height = 0;
    std::array<VkImageView, 6> views{};
    std::array<uint32_t, 6> mipStorage{};
    uint32_t levels = 1, layers = 1;
    bool ready = false;  // transitioned to GENERAL once
  };
  Texture make(uint32_t width, uint32_t height, VkFormat format, uint32_t levels = 1, uint32_t layers = 1);
  void destroy(Texture& t);
  void initialize(VkCommandBuffer cmd, Texture& t);
  void resize(glm::uvec2 screen, int quality);

  VulkanContext& context_;
  Bindless& bindless_;
  uint32_t samplerRepeat_, samplerClamp_;
  Texture weather_, march_, resolved_[2], shadowRaw_, shadowTemp_, shadow_;
  Texture depth_, historyDepth_[2], height_, atlas_, displacement_, middle_, cube_, water_[2];
  Buffer blue_, probes_, ambient_;
  std::array<Buffer, kFramesInFlight> records_{};
  VkPipeline weatherPipeline_ = VK_NULL_HANDLE, marchPipeline_ = VK_NULL_HANDLE, resolvePipeline_ = VK_NULL_HANDLE,
             shadowPipeline_ = VK_NULL_HANDLE, blurPipeline_ = VK_NULL_HANDLE;
  VkPipeline atlasPipeline_{}, heightPipeline_{}, displacementPipeline_{}, mipPipeline_{}, ambientPipeline_{},
             probesPipeline_{}, cubePipeline_{}, waterPipeline_{};
  glm::uvec2 screen_{0};
  int quality_ = -1;
  uint32_t weatherSize_ = 0, shadowSize_ = 0;
  glm::mat4 previousViewProj_{1.0f};
  bool historyValid_ = false, blueReady_ = false, shadowThisFrame_ = false, pending_ = false;
  uint32_t frame_ = 0, parity_ = 0;
  Rdr2Frame record_{};
  VkDeviceAddress recordAddress_ = 0;
  // Lightning: when the next flash strikes, where, and when the current one began.
  float nextFlash_ = -1.0f, flashBegan_ = -100.0f;
  float previousTime_ = -1.0f;
  glm::vec3 previousEye_{0}, previousForward_{0,0,-1};
  Params previousParams_{};
  std::array<float,4> flashStarted_{{-100,-100,-100,-100}};
  std::array<glm::vec3,4> flashPositions_{};
  uint32_t flashSlot_ = 0;
  glm::vec3 flashAt_{0.0f};
  uint32_t flashSeed_ = 20261104u;
  Timing timing_{};
};

}  // namespace ember
