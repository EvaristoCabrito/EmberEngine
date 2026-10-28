#include "render/WorldMapFx.h"

#include <imgui_impl_vulkan.h>
#include <nlohmann/json.hpp>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numbers>

#include "core/Log.h"
#include "rhi/Bindless.h"
#include "rhi/Pipeline.h"
#include "rhi/VulkanContext.h"

namespace ember {

namespace {

constexpr VkFormat kTargetFormat = VK_FORMAT_R8G8B8A8_UNORM;  // Renderer::outputFormat(): o alvo da UI

struct Push {
  glm::vec4 rect, view;
  uint32_t map, mask, reveal, samplerIndex;
  uint32_t flags;
  float time, rounding, glow;
  uint32_t paramsLo, paramsHi;
  uint32_t noise, repeatSampler;
};
static_assert(sizeof(Push) <= Bindless::kPushConstantBytes);

// Ruído de valor repetível numa grade de `period` células (uma volta da textura), suavizado.
float periodicNoise(float x, float y, int period, uint32_t seed) {
  auto hash = [&](int i, int j) {
    uint32_t h = uint32_t(((i % period + period) % period) * 374761393u + ((j % period + period) % period) * 668265263u + seed * 2246822519u);
    h ^= h >> 13, h *= 1274126177u, h ^= h >> 16;
    return float(h & 0xFFFFFFu) / float(0xFFFFFFu);
  };
  const int i = int(std::floor(x)), j = int(std::floor(y));
  float fx = x - float(i), fy = y - float(j);
  fx = fx * fx * (3.0f - 2.0f * fx), fy = fy * fy * (3.0f - 2.0f * fy);
  const float a = hash(i, j), b = hash(i + 1, j), c = hash(i, j + 1), d = hash(i + 1, j + 1);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

// fbm de `octaves` oitavas sobre a célula (x, y) de uma textura que repete a cada 8 células, normalizado ao 0..1.
float periodicFbm(float x, float y, int octaves, uint32_t seed) {
  float v = 0.0f, amp = 0.5f, sum = 0.0f;
  int period = 8;
  for (int o = 0; o < octaves; ++o) {
    v += amp * periodicNoise(x, y, period, seed + uint32_t(o) * 7919u);
    sum += amp;
    x = x * 2.0f + 3.7f, y = y * 2.0f + 1.3f, period *= 2, amp *= 0.5f;
  }
  return v / sum;
}

glm::vec3 vec3Of(const nlohmann::json& j, glm::vec3 d) {
  if (!j.is_array() || j.size() < 3) return d;
  return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}
glm::vec2 vec2Of(const nlohmann::json& j, glm::vec2 d) {
  if (!j.is_array() || j.size() < 2) return d;
  return {j[0].get<float>(), j[1].get<float>()};
}

float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

}  // namespace

WorldMapConfig WorldMapConfig::load(const std::filesystem::path& file, bool underworld) {
  WorldMapConfig c;
  if (!underworld) {  // as chaves padrão do mapa-múndi (o arquivo as substitui inteiras quando traz "hours")
    c.keys = {
        {0, {0.30f, 0.42f, 0.75f}, 0.55f, 1.15f, 0.75f, 0.10f, 1.05f, {0.2f, -0.98f}, 0.45f, 0.0f, {0.03f, 0.05f, 0.10f}, {0.30f, 0.38f, 0.55f}, 0.55f},
        {5.5f, {0.85f, 0.62f, 0.72f}, 0.80f, 1.10f, 0.90f, 0.04f, 1.0f, {-0.95f, -0.3f}, 0.70f, 0.6f, {0.14f, 0.11f, 0.16f}, {0.65f, 0.58f, 0.68f}, 0.60f},
        {9, {1.0f, 0.96f, 0.90f}, 1.0f, 1.08f, 1.0f, 0.0f, 1.0f, {-0.7f, -0.7f}, 0.60f, 0.4f, {0.12f, 0.12f, 0.12f}, {0.72f, 0.74f, 0.74f}, 0.40f},
        {13, {1.0f, 1.0f, 0.97f}, 1.05f, 1.05f, 1.0f, 0.0f, 1.0f, {0.0f, -1.0f}, 0.45f, 0.3f, {0.14f, 0.14f, 0.14f}, {0.78f, 0.80f, 0.80f}, 0.30f},
        {18, {1.0f, 0.62f, 0.38f}, 0.85f, 1.15f, 1.05f, 0.02f, 1.0f, {0.95f, -0.3f}, 0.80f, 0.8f, {0.22f, 0.12f, 0.07f}, {0.85f, 0.55f, 0.35f}, 0.55f},
        {21, {0.36f, 0.45f, 0.78f}, 0.60f, 1.15f, 0.80f, 0.08f, 1.05f, {0.4f, -0.9f}, 0.50f, 0.1f, {0.04f, 0.06f, 0.12f}, {0.32f, 0.40f, 0.58f}, 0.55f},
    };
  }
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    logWarn("worldmap: {} not found: defaults", file.string());
    return c;
  }
  const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
  if (!j.is_object()) {
    logError("worldmap: {} is not valid JSON: defaults", file.string());
    return c;
  }
  c.enabled = j.value("enabled", c.enabled);
  c.mask = j.value("mask", c.mask);
  const nlohmann::json fog = j.value("fogOfWar", nlohmann::json::object());
  c.fogDensity = fog.value("density", c.fogDensity);
  c.fogScale = fog.value("scale", c.fogScale);
  c.fogWind = fog.value("wind", c.fogWind);
  c.fogEdge = fog.value("edge", c.fogEdge);
  c.fogShade = fog.value("volume", c.fogShade);
  const nlohmann::json low = j.value("lowFog", nlohmann::json::object());
  c.lowScale = low.value("scale", c.lowScale);
  c.lowSpeed = low.value("speed", c.lowSpeed);
  c.lowHeight = low.value("height", c.lowHeight);
  c.lowThreshold = low.value("threshold", c.lowThreshold);
  const nlohmann::json g = j.value("grading", nlohmann::json::object());
  c.vignette = g.value("vignette", c.vignette);
  c.vignetteRadius = g.value("vignetteRadius", c.vignetteRadius);
  c.vignetteSoft = g.value("vignetteSoftness", c.vignetteSoft);
  c.shoulder = g.value("shoulder", c.shoulder);
  c.hourEase = j.value("hourEaseSeconds", c.hourEase);
  if (const nlohmann::json hours = j.value("hours", nlohmann::json::array()); hours.is_array() && !hours.empty()) {
    c.keys.clear();
    for (const nlohmann::json& k : hours) {
      HourKey h;
      h.hour = k.value("hour", 12.0f);
      h.tint = vec3Of(k.value("tint", nlohmann::json()), h.tint);
      h.exposure = k.value("exposure", h.exposure);
      h.contrast = k.value("contrast", h.contrast);
      h.saturation = k.value("saturation", h.saturation);
      h.lift = k.value("lift", h.lift);
      h.gamma = k.value("gamma", h.gamma);
      h.lightDir = vec2Of(k.value("lightDir", nlohmann::json()), h.lightDir);
      h.relief = k.value("relief", h.relief);
      h.rays = k.value("rays", h.rays);
      h.fogColor = vec3Of(k.value("fogColor", nlohmann::json()), h.fogColor);
      h.lowColor = vec3Of(k.value("lowFogColor", nlohmann::json()), h.lowColor);
      h.lowStrength = k.value("lowFogStrength", h.lowStrength);
      c.keys.push_back(h);
    }
    std::sort(c.keys.begin(), c.keys.end(), [](const HourKey& a, const HourKey& b) { return a.hour < b.hour; });
  }
  const nlohmann::json cycle = j.value("cycle", nlohmann::json::object());
  c.wakeHour = cycle.value("wakeHour", c.wakeHour);
  c.riseHours = cycle.value("riseHours", c.riseHours);
  c.glowHours = cycle.value("glowHours", c.glowHours);
  c.fadeHours = cycle.value("fadeHours", c.fadeHours);
  c.periodHours = cycle.value("periodHours", c.periodHours);
  const nlohmann::json glow = j.value("glow", nlohmann::json::object());
  c.glowColor = vec3Of(glow.value("color", nlohmann::json()), c.glowColor);
  c.glowRadius = glow.value("radius", c.glowRadius);
  c.peakContrast = glow.value("peakContrast", c.peakContrast);
  c.peakSaturation = glow.value("peakSaturation", c.peakSaturation);
  c.sleepSaturation = glow.value("sleepSaturation", c.sleepSaturation);
  c.glowAmbient = glow.value("ambient", c.glowAmbient);
  const nlohmann::json dark = j.value("dark", nlohmann::json::object());
  c.darkColor = vec3Of(dark.value("color", nlohmann::json()), c.darkColor);
  c.darkLevel = dark.value("level", c.darkLevel);
  const nlohmann::json cave = j.value("caveFog", nlohmann::json::object());
  c.caveColor = vec3Of(cave.value("color", nlohmann::json()), c.caveColor);
  c.caveDensity = cave.value("density", c.caveDensity);
  c.caveScale = cave.value("scale", c.caveScale);
  c.caveSpeed = cave.value("speed", c.caveSpeed);
  c.caveGlow = cave.value("glowShare", c.caveGlow);
  c.caveThreshold = cave.value("threshold", c.caveThreshold);
  return c;
}

WorldMapFx::WorldMapFx(VulkanContext& context, Bindless& bindless) : context_(context), bindless_(bindless) {
  GraphicsPipelineDesc desc;
  desc.vertexShader = "worldmap.VSMain";
  desc.fragmentShader = "worldmap.PSMain";
  desc.colorFormats = {kTargetFormat};
  desc.alphaBlend = true;
  desc.premultipliedAlpha = true;
  pipeline_ = createGraphicsPipeline(context, bindless.pipelineLayout(), desc);
  VkSamplerCreateInfo s{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  s.magFilter = s.minFilter = VK_FILTER_LINEAR;
  s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  s.addressModeU = s.addressModeV = s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  s.maxLod = VK_LOD_CLAMP_NONE;
  VK_CHECK(vkCreateSampler(context.device, &s, nullptr, &sampler_));
  samplerIndex_ = bindless.addSampler(sampler_);
  s.addressModeU = s.addressModeV = s.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VK_CHECK(vkCreateSampler(context.device, &s, nullptr, &repeatSampler_));
  repeatSamplerIndex_ = bindless.addSampler(repeatSampler_);
  for (Buffer& b : params_) b = createHostBuffer(context, sizeof(WorldMapParams), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  // A textura de ruído: 256 x 256, uma volta = 8 células; R fbm 5 oitavas, G 3, B 2 (outras sementes), A o ruído fino.
  constexpr uint32_t N = 256;
  std::vector<unsigned char> texels(size_t(N) * N * 4);
  for (uint32_t j = 0; j < N; ++j)
    for (uint32_t i = 0; i < N; ++i) {
      const float x = (float(i) + 0.5f) / float(N) * 8.0f, y = (float(j) + 0.5f) / float(N) * 8.0f;
      const float r = periodicFbm(x, y, 5, 11u), g = periodicFbm(x, y, 3, 29u), b = periodicFbm(x, y, 2, 47u), a = periodicNoise(x * 4.0f, y * 4.0f, 32, 83u);
      unsigned char* t = &texels[(size_t(j) * N + i) * 4];
      t[0] = static_cast<unsigned char>(std::lround(std::clamp(r, 0.0f, 1.0f) * 255.0f));
      t[1] = static_cast<unsigned char>(std::lround(std::clamp(g, 0.0f, 1.0f) * 255.0f));
      t[2] = static_cast<unsigned char>(std::lround(std::clamp(b, 0.0f, 1.0f) * 255.0f));
      t[3] = static_cast<unsigned char>(std::lround(std::clamp(a, 0.0f, 1.0f) * 255.0f));
    }
  noiseIndex_ = upload(texels.data(), N, N, VK_FORMAT_R8G8B8A8_UNORM, 4, noise_, kNoTexture);
}

WorldMapFx::~WorldMapFx() {
  vkDeviceWaitIdle(context_.device);
  for (Buffer& b : params_) destroyBuffer(context_, b);
  for (size_t i = 0; i < images_.size(); ++i) {
    bindless_.removeSampledImage(indices_[i]);
    destroyImage(context_, images_[i]);
  }
  if (revealIndex_ != kNoTexture) bindless_.removeSampledImage(revealIndex_), destroyImage(context_, reveal_);
  if (noiseIndex_ != kNoTexture) bindless_.removeSampledImage(noiseIndex_), destroyImage(context_, noise_);
  if (sampler_) vkDestroySampler(context_.device, sampler_, nullptr);
  if (repeatSampler_) vkDestroySampler(context_.device, repeatSampler_, nullptr);
  if (pipeline_) vkDestroyPipeline(context_.device, pipeline_, nullptr);
}

uint32_t WorldMapFx::upload(const unsigned char* pixels, uint32_t w, uint32_t h, VkFormat format, uint32_t bytesPerPixel, Image& into, uint32_t index) {
  const bool fresh = into.image == VK_NULL_HANDLE || into.extent.width != w || into.extent.height != h;
  if (fresh) {
    if (into.image) {
      vkDeviceWaitIdle(context_.device);
      textureBytes_ -= uint64_t(into.extent.width) * into.extent.height * bytesPerPixel;
      destroyImage(context_, into);
    }
    ImageDesc desc;
    desc.format = format;
    desc.extent = {w, h};
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    into = createImage(context_, desc);
    textureBytes_ += uint64_t(w) * h * bytesPerPixel;
  }
  const VkDeviceSize bytes = VkDeviceSize(w) * h * bytesPerPixel;
  Buffer staging = createHostBuffer(context_, std::max<VkDeviceSize>(bytes, 16), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  std::memcpy(staging.mapped, pixels, static_cast<size_t>(bytes));
  VK_CHECK(vmaFlushAllocation(context_.allocator, staging.allocation, 0, VK_WHOLE_SIZE));
  context_.immediateSubmit([&](VkCommandBuffer cmd) {
    transition(cmd, into.image, VK_IMAGE_ASPECT_COLOR_BIT,
               {fresh ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, staging.buffer, into.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transition(cmd, into.image, VK_IMAGE_ASPECT_COLOR_BIT,
               {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});
  });
  destroyBuffer(context_, staging);
  if (index == kNoTexture) return bindless_.addSampledImage(into.view);
  if (fresh) bindless_.setSampledImage(index, into.view);
  return index;
}

uint32_t WorldMapFx::loadTexture(const std::filesystem::path& file) {
  int w = 0, h = 0, n = 0;
  stbi_uc* pixels = stbi_load(file.string().c_str(), &w, &h, &n, 4);
  if (!pixels) {
    logError("worldmap: image not read: {}", file.string());
    return kNoTexture;
  }
  Image image;
  const uint32_t index = upload(pixels, static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM, 4, image, kNoTexture);
  stbi_image_free(pixels);
  images_.push_back(image);
  indices_.push_back(index);
  logInfo("worldmap: {} {}x{} on the GPU ({:.1f} MB)", file.filename().string(), w, h, double(w) * h * 4 / 1048576.0);
  return index;
}

void WorldMapFx::setReveal(const std::vector<uint8_t>& texels, uint32_t w, uint32_t h) {
  if (texels.size() < size_t(w) * h) return;
  revealIndex_ = upload(texels.data(), w, h, VK_FORMAT_R8_UNORM, 1, reveal_, revealIndex_);
}

void WorldMapFx::draw(ImDrawList* dl, const Draw& d) {
  if (d.map == kNoTexture) return;
  Slot& slot = slots_[next_ % kParamSlots];
  slot.self = this;
  slot.draw = d;
  slot.paramsSlot = next_ % kParamSlots;
  ++next_;
  std::memcpy(params_[slot.paramsSlot].mapped, &d.params, sizeof(WorldMapParams));
  dl->AddCallback(&WorldMapFx::callback, &slot);
  dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);  // the backend's own state back for the UI after it
}

void WorldMapFx::callback(const ImDrawList*, const ImDrawCmd* cmd) {
  auto* slot = static_cast<Slot*>(cmd->UserCallbackData);
  if (!slot || !slot->self) return;
  ImGui_ImplVulkan_RenderState* state = ImGui_ImplVulkan_GetRenderState();
  if (!state || !state->CommandBuffer) return;
  // O recorte do comando, em pixels do alvo (a regra do próprio backend).
  const ImDrawData* data = ImGui::GetDrawData();
  const ImVec2 off = data->DisplayPos, sc = data->FramebufferScale;
  const int fbW = static_cast<int>(data->DisplaySize.x * sc.x + 0.5f), fbH = static_cast<int>(data->DisplaySize.y * sc.y + 0.5f);
  float x0 = (cmd->ClipRect.x - off.x) * sc.x, y0 = (cmd->ClipRect.y - off.y) * sc.y, x1 = (cmd->ClipRect.z - off.x) * sc.x, y1 = (cmd->ClipRect.w - off.y) * sc.y;
  const Draw& d = slot->draw;
  // ... e o retângulo da pintura dentro dele: só os pixels da pintura passam pelo shader.
  x0 = std::max(x0, (d.rectMin.x - off.x) * sc.x), y0 = std::max(y0, (d.rectMin.y - off.y) * sc.y);
  x1 = std::min(x1, (d.rectMax.x - off.x) * sc.x), y1 = std::min(y1, (d.rectMax.y - off.y) * sc.y);
  x0 = std::clamp(x0, 0.0f, float(fbW)), y0 = std::clamp(y0, 0.0f, float(fbH)), x1 = std::clamp(x1, 0.0f, float(fbW)), y1 = std::clamp(y1, 0.0f, float(fbH));
  if (x1 <= x0 || y1 <= y0) return;
  VkCommandBuffer cb = state->CommandBuffer;
  WorldMapFx& self = *slot->self;
  self.bindless_.bind(cb, VK_PIPELINE_BIND_POINT_GRAPHICS);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, self.pipeline_);
  VkViewport vp{0.0f, 0.0f, float(fbW), float(fbH), 0.0f, 1.0f};
  vkCmdSetViewport(cb, 0, 1, &vp);
  VkRect2D scissor{{int32_t(x0), int32_t(y0)}, {uint32_t(x1 - x0), uint32_t(y1 - y0)}};
  vkCmdSetScissor(cb, 0, 1, &scissor);
  const VkDeviceAddress address = bufferAddress(self.context_, self.params_[slot->paramsSlot]);
  Push push{};
  push.rect = {(d.rectMin.x - off.x) * sc.x, (d.rectMin.y - off.y) * sc.y, (d.rectMax.x - d.rectMin.x) * sc.x, (d.rectMax.y - d.rectMin.y) * sc.y};
  push.view = {(d.viewMin.x - off.x) * sc.x, (d.viewMin.y - off.y) * sc.y, (d.viewMax.x - d.viewMin.x) * sc.x, (d.viewMax.y - d.viewMin.y) * sc.y};
  push.map = d.map;
  // Never an invalid index in the push: the shader reads a texture of a branch it does not take (DXC speculates the load) and
  // gTextures[kNoTexture] lost the device (2026-10-27). Absent = the noise texture + the flag off.
  const bool hasMask = d.mask != kNoTexture;
  const bool hasReveal = d.fogOfWar && !d.underworld && self.revealIndex_ != kNoTexture;
  push.mask = hasMask ? d.mask : self.noiseIndex_;
  push.reveal = hasReveal ? self.revealIndex_ : self.noiseIndex_;
  push.samplerIndex = self.samplerIndex_;
  push.flags = (d.underworld ? 1u : 0u) | (uint32_t(std::clamp(d.quality, 0, 2)) << 1) | (hasMask ? 8u : 0u) | (hasReveal ? 16u : 0u);
  push.time = d.time;
  push.rounding = d.rounding * sc.y;
  push.glow = d.glow;
  push.paramsLo = uint32_t(address & 0xFFFFFFFFu);
  push.paramsHi = uint32_t(address >> 32);
  push.noise = self.noiseIndex_;
  push.repeatSampler = self.repeatSamplerIndex_;
  vkCmdPushConstants(cb, self.bindless_.pipelineLayout(), Bindless::kStages, 0, sizeof(push), &push);
  vkCmdDraw(cb, 3, 1, 0, 0);
}

WorldMapParams WorldMapFx::overworldParams(const WorldMapConfig& c, float hour) {
  WorldMapParams p{};
  if (c.keys.empty()) return p;
  // A chave de antes e a de depois da hora (cíclico em 24 h), a mistura suave entre elas.
  hour = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
  const WorldMapConfig::HourKey* a = &c.keys.back();
  const WorldMapConfig::HourKey* b = &c.keys.front();
  float ha = a->hour - 24.0f, hb = b->hour;
  for (size_t i = 0; i < c.keys.size(); ++i) {
    const float h0 = c.keys[i].hour, h1 = i + 1 < c.keys.size() ? c.keys[i + 1].hour : c.keys.front().hour + 24.0f;
    if (hour >= h0 && hour < h1) a = &c.keys[i], b = i + 1 < c.keys.size() ? &c.keys[i + 1] : &c.keys.front(), ha = h0, hb = h1;
  }
  const float t = hb > ha ? smooth(std::clamp((hour - ha) / (hb - ha), 0.0f, 1.0f)) : 0.0f;
  auto mix = [&](float x, float y) { return x + (y - x) * t; };
  auto mix3 = [&](glm::vec3 x, glm::vec3 y) { return x + (y - x) * t; };
  const glm::vec3 tint = mix3(a->tint, b->tint);
  p.tint = {tint, mix(a->exposure, b->exposure)};
  p.grade = {mix(a->contrast, b->contrast), mix(a->saturation, b->saturation), mix(a->lift, b->lift), mix(a->gamma, b->gamma)};
  glm::vec2 dir = a->lightDir + (b->lightDir - a->lightDir) * t;
  const float len = std::hypot(dir.x, dir.y);
  if (len > 1e-4f) dir /= len;
  p.light = {dir.x, dir.y, mix(a->relief, b->relief), mix(a->rays, b->rays)};
  p.vignette = {c.vignette, c.vignetteRadius, c.vignetteSoft, c.shoulder};
  p.fogColor = {mix3(a->fogColor, b->fogColor), c.fogDensity};
  p.fogShape = {c.fogScale, c.fogWind, c.fogEdge, c.fogShade};
  p.lowColor = {mix3(a->lowColor, b->lowColor), mix(a->lowStrength, b->lowStrength)};
  p.lowShape = {c.lowScale, c.lowSpeed, c.lowHeight, c.lowThreshold};
  return p;
}

float WorldMapFx::underworldGlow(const WorldMapConfig& c, double totalHours) {
  const double period = std::max(0.1, double(c.periodHours));
  double t = std::fmod(totalHours - c.wakeHour, period);
  if (t < 0) t += period;
  const double rise = std::max(0.01, double(c.riseHours)), hold = std::max(0.0, double(c.glowHours)), fade = std::max(0.01, double(c.fadeHours));
  if (t < rise) return smooth(float(t / rise));
  if (t < rise + hold) return 1.0f;
  if (t < rise + hold + fade) return 1.0f - smooth(float((t - rise - hold) / fade));
  return 0.0f;
}

WorldMapParams WorldMapFx::underworldParams(const WorldMapConfig& c, float glow) {
  WorldMapParams p{};
  p.tint = {1, 1, 1, 1};
  p.grade = {1.0f + (c.peakContrast - 1.0f) * glow, c.sleepSaturation + (c.peakSaturation - c.sleepSaturation) * glow, 0.0f, 1.0f};
  p.light = {0, -1, 0, 0};
  p.vignette = {c.vignette, c.vignetteRadius, c.vignetteSoft, c.shoulder};
  p.glowColor = {c.glowColor, c.glowRadius};
  p.dark = {c.darkColor, c.darkLevel};
  p.caveColor = {c.caveColor, c.caveDensity};
  p.caveShape = {c.caveScale, c.caveSpeed, c.caveGlow, c.caveThreshold};
  p.extra[0] = {c.glowAmbient, 0, 0, 0};
  return p;
}

}  // namespace ember
