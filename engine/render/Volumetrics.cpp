#include "render/Volumetrics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "rhi/Bindless.h"
#include "rhi/Pipeline.h"
#include "rhi/VulkanContext.h"

namespace ember {

namespace {

// Matches VolumetricsFrame in shaders/volumetrics_v1.hlsl: 272 bytes, then the local volumes (32 bytes each).
struct VolumetricsFrame {
  glm::mat4 inverseViewProjection;
  glm::mat4 previousViewProjection;
  glm::uvec3 grid;
  float jitter;
  glm::vec3 wind;
  float time;
  glm::vec3 albedo;
  float extinction;
  float scaleHeight;
  float valley;
  float noiseScale;
  float noiseStrength;
  float anisotropy;
  float backLobe;
  float sunScale;
  float skyScale;
  float localScale;
  float historyWeight;
  float meanGround;
  float bankDensity;
  float bankHeight;
  uint32_t sunTaps;
  uint32_t occlusionTaps;
  uint32_t historyValid;
  uint32_t injected;
  uint32_t history;
  uint32_t integrated;
  uint32_t volumeCount;
  float nearFade;
  float shadowSoftness;
  float pad[2];
};
static_assert(sizeof(VolumetricsFrame) == 272);
static_assert(sizeof(Volumetrics::LocalVolume) == 32);
constexpr VkDeviceSize kRecordBytes = sizeof(VolumetricsFrame) + sizeof(Volumetrics::LocalVolume) * Volumetrics::kMaxVolumes;

struct Push {
  VkDeviceAddress frame;
  VkDeviceAddress volumetrics;
};

constexpr VkShaderStageFlags kPushStages = Bindless::kStages;

void barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
  VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
  b.srcStageMask = srcStage;
  b.srcAccessMask = srcAccess;
  b.dstStageMask = dstStage;
  b.dstAccessMask = dstAccess;
  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency.memoryBarrierCount = 1;
  dependency.pMemoryBarriers = &b;
  vkCmdPipelineBarrier2(cmd, &dependency);
}

float readFloat(const nlohmann::json& j, const char* key, float fallback) {
  const auto it = j.find(key);
  return it != j.end() && it->is_number() ? it->get<float>() : fallback;
}
glm::vec3 readVec3(const nlohmann::json& j, const char* key, glm::vec3 fallback) {
  const auto it = j.find(key);
  if (it == j.end() || !it->is_array() || it->size() != 3) return fallback;
  for (const auto& v : *it)
    if (!v.is_number()) return fallback;
  return {(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()};
}

}  // namespace

Volumetrics::Level Volumetrics::level(Quality q) {
  switch (q) {
    case Quality::Cinematic: return {{192, 108, 96}, 4, 3, 0.90f, 2.5f};
    case Quality::High: return {{160, 90, 64}, 1, 2, 0.90f, 1.6f};
    default: return {{96, 54, 48}, 1, 0, 0.93f, 1.0f};
  }
}

Volumetrics::Profile Volumetrics::mix(const Profile& a, const Profile& b, float t) {
  Profile p = a;
  const auto m = [t](float x, float y) { return x + (y - x) * t; };
  p.visibility = m(a.visibility, b.visibility);
  p.scaleHeight = m(a.scaleHeight, b.scaleHeight);
  p.valley = m(a.valley, b.valley);
  p.albedo = glm::mix(a.albedo, b.albedo, t);
  p.anisotropy = m(a.anisotropy, b.anisotropy);
  p.backLobe = m(a.backLobe, b.backLobe);
  p.wind = glm::mix(a.wind, b.wind, t);
  p.noiseScale = m(a.noiseScale, b.noiseScale);
  p.noiseStrength = m(a.noiseStrength, b.noiseStrength);
  p.sunScale = m(a.sunScale, b.sunScale);
  p.skyScale = m(a.skyScale, b.skyScale);
  p.localScale = m(a.localScale, b.localScale);
  p.maxDistance = m(a.maxDistance, b.maxDistance);
  p.tacticalScale = m(a.tacticalScale, b.tacticalScale);
  p.nearFade = m(a.nearFade, b.nearFade);
  p.bankDensity = m(a.bankDensity, b.bankDensity);
  p.bankHeight = m(a.bankHeight, b.bankHeight);
  return p;
}

std::vector<Volumetrics::Profile> Volumetrics::loadProfiles(const std::string& path) {
  std::vector<Profile> out;
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    logWarn("volumetrics: no profiles at {} (the default profile only)", path);
    return out;
  }
  const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
  if (!j.is_object() || j.value("version", 0) != static_cast<int>(kVersion) || !j.contains("profiles") || !j["profiles"].is_array()) {
    logWarn("volumetrics: {} is not a version {} profile file", path, kVersion);
    return out;
  }
  for (const nlohmann::json& e : j["profiles"]) {
    if (!e.is_object()) continue;
    Profile p;
    p.name = e.value("name", p.name);
    p.label = e.value("label", p.name);
    if (const auto it = e.find("maps"); it != e.end() && it->is_array())
      for (const auto& m : *it)
        if (m.is_string()) p.maps.push_back(m.get<std::string>());
    p.visibility = std::max(readFloat(e, "visibility", p.visibility), 1.0f);
    p.scaleHeight = std::max(readFloat(e, "scaleHeight", p.scaleHeight), 0.05f);
    p.valley = readFloat(e, "valley", p.valley);
    p.albedo = readVec3(e, "albedo", p.albedo);
    p.anisotropy = std::clamp(readFloat(e, "anisotropy", p.anisotropy), -0.95f, 0.95f);
    p.backLobe = std::clamp(readFloat(e, "backLobe", p.backLobe), 0.0f, 1.0f);
    p.wind = readVec3(e, "wind", p.wind);
    p.noiseScale = readFloat(e, "noiseScale", p.noiseScale);
    p.noiseStrength = std::clamp(readFloat(e, "noiseStrength", p.noiseStrength), 0.0f, 1.0f);
    p.sunScale = readFloat(e, "sunScale", p.sunScale);
    p.skyScale = readFloat(e, "skyScale", p.skyScale);
    p.localScale = readFloat(e, "localScale", p.localScale);
    p.maxDistance = std::max(readFloat(e, "maxDistance", p.maxDistance), 10.0f);
    p.tacticalScale = readFloat(e, "tacticalScale", p.tacticalScale);
    p.nearFade = readFloat(e, "nearFade", p.nearFade);
    p.bankDensity = readFloat(e, "bankDensity", p.bankDensity);
    p.bankHeight = std::max(readFloat(e, "bankHeight", p.bankHeight), 0.05f);
    out.push_back(std::move(p));
  }
  logInfo("volumetrics: {} profiles from {}", out.size(), path);
  return out;
}

Volumetrics::Volumetrics(VulkanContext& context, Bindless& bindless) : context_(context), bindless_(bindless) {
  for (Buffer& b : frames_) b = createDeviceBuffer(context, kRecordBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  inject_ = createComputePipeline(context, bindless.pipelineLayout(), "volumetrics_v1.CSInject");
  integrate_ = createComputePipeline(context, bindless.pipelineLayout(), "volumetrics_v1.CSIntegrate");
  target_ = from_ = current_;
}

Volumetrics::~Volumetrics() {
  destroyVolumes();
  vkDestroyPipeline(context_.device, inject_, nullptr);
  vkDestroyPipeline(context_.device, integrate_, nullptr);
  for (Buffer& b : frames_) destroyBuffer(context_, b);
}

void Volumetrics::createVolumes() {
  const ImageDesc desc{.format = VK_FORMAT_R16G16B16A16_SFLOAT,
                       .extent = {grid_.x, grid_.y},
                       .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       .depth = grid_.z};
  for (int i = 0; i < 2; ++i) {
    froxels_[i] = createImage(context_, desc);
    froxelStorage_[i] = bindless_.addStorageImage(froxels_[i].view);
    froxelSampled_[i] = bindless_.addSampledImage(froxels_[i].view, VK_IMAGE_LAYOUT_GENERAL);
  }
  integrated_ = createImage(context_, desc);
  integratedStorage_ = bindless_.addStorageImage(integrated_.view);
  integratedSampled_ = bindless_.addSampledImage(integrated_.view, VK_IMAGE_LAYOUT_GENERAL);
  volumesReady_ = false;
  historyValid_ = false;
}

void Volumetrics::destroyVolumes() {
  if (grid_.x == 0) return;
  for (int i = 0; i < 2; ++i) {
    bindless_.removeStorageImage(froxelStorage_[i]);
    bindless_.removeSampledImage(froxelSampled_[i]);
    destroyImage(context_, froxels_[i]);
  }
  bindless_.removeStorageImage(integratedStorage_);
  bindless_.removeSampledImage(integratedSampled_);
  destroyImage(context_, integrated_);
  grid_ = glm::uvec3(0);
}

void Volumetrics::setQuality(Quality q) {
  if (q == quality_) return;
  quality_ = q;
  effective_ = q;
  overBudgetFrames_ = underBudgetFrames_ = 0;
}

void Volumetrics::setProfile(const Profile& target, float seconds) {
  if (target.name == target_.name && blendT_ >= 1.0f) {
    target_ = target;  // the same profile retuned: take its numbers (the file may have been re-read)
    current_ = target;
    return;
  }
  if (target.name == target_.name) {
    target_ = target;
    return;
  }
  from_ = current_;
  target_ = target;
  blendSeconds_ = std::max(seconds, 0.0f);
  blendT_ = blendSeconds_ > 0.0f ? 0.0f : 1.0f;
  if (blendT_ >= 1.0f) current_ = target_;
}

void Volumetrics::setLocalVolumes(std::span<const LocalVolume> volumes) {
  volumes_.assign(volumes.begin(), volumes.begin() + std::min<size_t>(volumes.size(), kMaxVolumes));
}

void Volumetrics::reportGpuMilliseconds(double ms) {
  if (!adaptive_ || !thisFrame_) return;
  const Level lv = level(effective_);
  if (ms > lv.budgetMs) {
    underBudgetFrames_ = 0;
    if (++overBudgetFrames_ >= 90 && effective_ != Quality::Performance) {  // 1.5 s over budget: one level down
      effective_ = static_cast<Quality>(static_cast<int>(effective_) - 1);
      overBudgetFrames_ = 0;
      logInfo("volumetrics: {:.2f} ms over the {:.1f} ms budget: quality down to {}", ms, lv.budgetMs, static_cast<int>(effective_));
    }
  } else {
    overBudgetFrames_ = 0;
    if (ms < lv.budgetMs * 0.5 && effective_ < quality_) {
      if (++underBudgetFrames_ >= 600) {  // 10 s well under: one level up, never past the chosen level
        effective_ = static_cast<Quality>(static_cast<int>(effective_) + 1);
        underBudgetFrames_ = 0;
        logInfo("volumetrics: {:.2f} ms: quality back up to {}", ms, static_cast<int>(effective_));
      }
    } else {
      underBudgetFrames_ = 0;
    }
  }
}

bool Volumetrics::prepare(const Camera& camera, float time, VkCommandBuffer cmd, uint32_t frameSlot, FrameOut& out) {
  thisFrame_ = enabled_;
  if (!enabled_) {
    historyValid_ = false;
    lastTime_ = time;
    return false;
  }
  const Level lv = level(effective_);
  if (lv.grid != grid_) {
    vkDeviceWaitIdle(context_.device);
    destroyVolumes();
    grid_ = lv.grid;
    createVolumes();
  }
  // The profile blend.
  const float dt = lastTime_ < 0.0f ? 0.0f : std::clamp(time - lastTime_, 0.0f, 0.25f);
  lastTime_ = time;
  if (blendT_ < 1.0f) {
    blendT_ = blendSeconds_ > 0.0f ? std::min(blendT_ + dt / blendSeconds_, 1.0f) : 1.0f;
    const float s = blendT_ * blendT_ * (3.0f - 2.0f * blendT_);
    current_ = mix(from_, target_, s);
  }
  // A camera cut: the history would smear the old view over the new one.
  const float moved = glm::length(camera.position - previousEye_);
  const float turned = glm::dot(glm::normalize(camera.forward), glm::normalize(previousForward_));
  if (moved > 25.0f || turned < 0.85f) historyValid_ = false;
  previousEye_ = camera.position;
  previousForward_ = camera.forward;

  const float near = std::max(camera.projection[3][2], 0.25f);
  const float far = near + (tactical_ ? std::max(current_.maxDistance, 450.0f) : current_.maxDistance);

  VolumetricsFrame v{};
  const glm::mat4 viewProjection = camera.viewProjection();
  v.inverseViewProjection = glm::inverse(viewProjection);
  v.previousViewProjection = previousViewProjection_;
  previousViewProjection_ = viewProjection;
  v.grid = grid_;
  // Halton (base 2) slice offsets: every frame samples the slices at a different depth.
  uint32_t n = ++frameCounter_ % 64 + 1;
  float halton = 0.0f, scale = 0.5f;
  for (; n; n >>= 1, scale *= 0.5f) halton += static_cast<float>(n & 1) * scale;
  v.jitter = halton - 0.5f;
  v.wind = current_.wind;
  v.time = time;
  v.albedo = current_.albedo;
  v.extinction = 3.912f / current_.visibility * (tactical_ ? current_.tacticalScale : 1.0f);
  v.scaleHeight = current_.scaleHeight;
  v.valley = current_.valley;
  v.noiseScale = current_.noiseScale;
  v.noiseStrength = current_.noiseStrength;
  v.anisotropy = current_.anisotropy;
  v.backLobe = current_.backLobe;
  v.sunScale = current_.sunScale;
  v.skyScale = current_.skyScale;
  v.localScale = current_.localScale;
  v.historyWeight = lv.historyWeight;
  v.meanGround = meanGround_;
  v.bankDensity = current_.bankDensity * (tactical_ ? current_.tacticalScale : 1.0f);
  v.bankHeight = current_.bankHeight;
  v.sunTaps = lv.sunTaps;
  v.occlusionTaps = lv.occlusionTaps;
  v.historyValid = historyValid_ ? 1u : 0u;
  current_slot_ ^= 1u;
  v.injected = froxelStorage_[current_slot_];
  v.history = froxelSampled_[current_slot_ ^ 1u];
  v.integrated = integratedStorage_;
  v.volumeCount = static_cast<uint32_t>(volumes_.size());
  v.nearFade = std::max(current_.nearFade, 0.1f);
  v.shadowSoftness = 0.6f;

  std::array<std::byte, kRecordBytes> record{};
  std::memcpy(record.data(), &v, sizeof(v));
  if (!volumes_.empty()) std::memcpy(record.data() + sizeof(v), volumes_.data(), volumes_.size() * sizeof(LocalVolume));
  // The previous frame's passes may still read this slot's record.
  barrier(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
  vkCmdUpdateBuffer(cmd, frames_[frameSlot].buffer, 0, kRecordBytes, record.data());

  out.integrated = integratedSampled_;
  out.slices = grid_.z;
  out.near = near;
  out.far = far;
  return true;
}

void Volumetrics::record(VkCommandBuffer cmd, VkDeviceAddress meshFrame, uint32_t frameSlot) {
  if (!thisFrame_) return;
  if (!volumesReady_) {
    volumesReady_ = true;
    for (Image* v : {&froxels_[0], &froxels_[1], &integrated_})
      transition(cmd, v->image, v->aspect,
                 {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
  }
  // The record's upload, and the previous frame's surfaces still reading what is written now.
  barrier(cmd, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  const Push push{meshFrame, bufferAddress(context_, frames_[frameSlot])};
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, inject_);
  vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
  vkCmdDispatch(cmd, (grid_.x + 7) / 8, (grid_.y + 7) / 8, grid_.z);
  barrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, integrate_);
  vkCmdPushConstants(cmd, bindless_.pipelineLayout(), kPushStages, 0, sizeof(push), &push);
  vkCmdDispatch(cmd, (grid_.x + 7) / 8, (grid_.y + 7) / 8, 1);
  barrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
          VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  historyValid_ = true;
}

}  // namespace ember
