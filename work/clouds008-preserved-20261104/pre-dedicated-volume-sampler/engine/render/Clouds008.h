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
#include "render/CloudsRdr2.h"
#include "rhi/Image.h"
#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;
class Bindless;
class GpuTimer;

// The record the shaders read by byte offset (clouds_rdr2.hlsli kR2*).
class Clouds008 {
 public:
  // What the clouds receive (the user's prompt §6): from the weather (sliders in the panel until a weather exists) and from
  // the caller's knowledge of the board.
  using Params = CloudsRdr2::Params;
  using Output = CloudsRdr2::Output;
  using Timing = CloudsRdr2::Timing;

  Clouds008(VulkanContext& context, Bindless& bindless, uint32_t samplerRepeat, uint32_t samplerClamp);
  ~Clouds008();
  Clouds008(const Clouds008&) = delete;
  Clouds008& operator=(const Clouds008&) = delete;

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
