#pragma once

// Atmosfera volumétrica v1 (docs/ATMOSPHERE.md): the visible air as a lit 3D medium, in froxels
// (shaders/volumetrics_v1.hlsl). A module of its own with its own lifecycle: the renderer only hands it the camera
// and the frame record, records its two compute passes, and points `MeshFrame.volume` at its result; every surface
// then reads the fog at its own depth through lighting.hlsli applyVolume (the depth-exact composition of the froxel
// method). Switched off, the renderer draws exactly as before (rollback = the OFF choice).
//   profile   a regional profile (content/atmosphere/v1/profiles.json): visibility, height falloff, colour, wind,
//             noise, the light's strengths, the fog of war's bank, Tática's readability scale. Profiles blend over
//             seconds, so a region change never jumps.
//   quality   cinematic / high / performance: froxel grid, soft shadow taps, relief occlusion taps, history. The
//             chosen level is a ceiling; adaptive mode steps down after sustained overruns of the level's budget and
//             back up only after a long run well under it (hysteresis).
//   history   temporal integration of the jittered slices; invalidated on camera cuts (Tática, teleports, a new map).

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "rhi/Image.h"
#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;
class Bindless;

class Volumetrics {
 public:
  static constexpr uint32_t kVersion = 1;
  static constexpr uint32_t kMaxVolumes = 16;

  enum class Quality : int { Performance = 0, High = 1, Cinematic = 2 };

  // A regional profile. Distances in world units (one hex radius, about a metre).
  struct Profile {
    std::string name = "padrao";  // the file's key
    std::string label = "padrão";  // the panel's word
    std::vector<std::string> maps;  // map keys (substrings) this profile belongs to
    float visibility = 90.0f;       // Koschmieder: sigma_t at the ground = 3.912 / visibility
    float scaleHeight = 3.0f;       // density falls by e over this height above the ground
    float valley = 0.5f;            // extra density per unit the ground lies below its surroundings
    glm::vec3 albedo{0.92f, 0.94f, 0.97f};
    float anisotropy = 0.45f;       // forward lobe g
    float backLobe = 0.15f;         // share of the backward lobe
    glm::vec3 wind{0.35f, 0.02f, 0.15f};
    float noiseScale = 0.12f;       // fbm frequency (per unit)
    float noiseStrength = 0.55f;    // 0 uniform .. 1 fully broken up
    float sunScale = 1.0f, skyScale = 1.0f, localScale = 1.0f;
    float maxDistance = 160.0f;     // the froxel grid's reach past the near plane
    float tacticalScale = 0.35f;    // density in Tática (seen from far above: readability first)
    float nearFade = 1.5f;          // no fog within this distance of the eye, fading in over as much again
    float bankDensity = 0.0f;       // the fog of war's bank over never-seen ground (0 = leave it to the fog looks)
    float bankHeight = 3.0f;
  };
  // Blends every number of two profiles (names and maps stay a's).
  static Profile mix(const Profile& a, const Profile& b, float t);
  // content/atmosphere/v1/profiles.json; empty when the file is missing or malformed (then only the default exists).
  static std::vector<Profile> loadProfiles(const std::string& path);

  // A box of extra fog (feathered edge): caves, clearings, corridors, mist over water.
  struct LocalVolume {
    glm::vec3 center;
    float density;  // sigma_t added inside
    glm::vec3 halfExtent;
    float feather;
  };

  Volumetrics(VulkanContext& context, Bindless& bindless);
  ~Volumetrics();
  Volumetrics(const Volumetrics&) = delete;
  Volumetrics& operator=(const Volumetrics&) = delete;

  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }
  // The level chosen (a ceiling when adaptive); a grid change waits for the GPU and remakes the volumes.
  void setQuality(Quality q);
  Quality quality() const { return quality_; }
  Quality effectiveQuality() const { return effective_; }
  void setAdaptive(bool on) { adaptive_ = on; }
  // The profile to reach, over `seconds` (0 = at once).
  void setProfile(const Profile& target, float seconds);
  const Profile& profile() const { return current_; }
  void setLocalVolumes(std::span<const LocalVolume> volumes);
  // The scene this frame: ground height where the mask has none; whether the camera is Tática's (density scaled, the
  // grid follows its far near plane).
  void setScene(float meanGround, bool tactical) {
    meanGround_ = meanGround;
    tactical_ = tactical;
  }
  // The next frame starts its history afresh (a camera cut: Tática on / off, a jump, a new map).
  void invalidateHistory() { historyValid_ = false; }
  // The subsystem's own GPU time (the renderer's "volumetrics" scope) for the adaptive quality.
  void reportGpuMilliseconds(double ms);

  // --- The renderer's side ---------------------------------------------------------------------------------
  struct FrameOut {
    uint32_t integrated = 0xFFFFFFFFu;  // sampled 3D texture to put in MeshFrame.volume
    uint32_t slices = 0;
    float near = 1.0f, far = 100.0f;    // MeshFrame.volumeNear / volumeFar
  };
  // Writes this frame's record (vkCmdUpdateBuffer on `cmd`, before the frame's passes) and says where the result
  // will be. False when off: nothing recorded, the renderer keeps its own volume fields.
  bool prepare(const Camera& camera, float time, VkCommandBuffer cmd, uint32_t frameSlot, FrameOut& out);
  // The two compute passes (after the sun and card shadow maps, the light clusters and the fog mask are ready).
  void record(VkCommandBuffer cmd, VkDeviceAddress meshFrame, uint32_t frameSlot);

 private:
  struct Level {
    glm::uvec3 grid;
    uint32_t sunTaps, occlusionTaps;
    float historyWeight, budgetMs;
  };
  static Level level(Quality q);
  void createVolumes();
  void destroyVolumes();

  VulkanContext& context_;
  Bindless& bindless_;
  bool enabled_ = false, adaptive_ = true;
  Quality quality_ = Quality::High, effective_ = Quality::High;
  int overBudgetFrames_ = 0, underBudgetFrames_ = 0;
  Profile current_, from_, target_;
  float blendSeconds_ = 0.0f, blendT_ = 1.0f;
  std::vector<LocalVolume> volumes_;
  float meanGround_ = 0.0f;
  bool tactical_ = false;
  float lastTime_ = -1.0f;

  glm::uvec3 grid_{0};
  Image froxels_[2], integrated_;
  uint32_t froxelStorage_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu}, froxelSampled_[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
  uint32_t integratedStorage_ = 0xFFFFFFFFu, integratedSampled_ = 0xFFFFFFFFu;
  bool volumesReady_ = false;  // layouts set
  uint32_t current_slot_ = 0, frameCounter_ = 0;
  bool historyValid_ = false, thisFrame_ = false;
  glm::mat4 previousViewProjection_{1.0f};
  glm::vec3 previousEye_{0.0f}, previousForward_{0.0f, 0.0f, -1.0f};
  std::array<Buffer, kFramesInFlight> frames_{};
  VkPipeline inject_ = VK_NULL_HANDLE, integrate_ = VK_NULL_HANDLE;
};

}  // namespace ember
