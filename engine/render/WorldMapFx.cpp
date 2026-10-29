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
  uint32_t lutA, lutB;
  uint32_t tex[9];
};
static_assert(sizeof(Push) <= Bindless::kPushConstantBytes);

// Horas depois de `from`, no ciclo de 24 h (0..24).
float hoursAfter(float hour, float from) { return std::fmod(hour - from + 48.0f, 24.0f); }
// Uma rampa suave 0..1 que sobe de `from` a `to` (horas), fica em 1 até `holdUntil` e desce a 0 em `fallHours`.
float hourWindow(float hour, float from, float to, float holdUntil, float fallHours) {
  const float t = hoursAfter(hour, from);
  const float rise = std::max(to - from, 0.01f), hold = std::max(hoursAfter(holdUntil, from), rise);
  float x;
  if (t < rise) x = t / rise;
  else if (t < hold) x = 1.0f;
  else x = 1.0f - std::clamp((t - hold) / std::max(fallHours, 0.01f), 0.0f, 1.0f);
  return x * x * (3.0f - 2.0f * x);
}

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
  // Mapa-múndi, o look 2D (2026-10-28): "lut" (as faixas por hora), as máscaras e os números de cada parte.
  if (const nlohmann::json luts = j.value("lut", nlohmann::json::array()); luts.is_array()) {
    c.luts.clear();
    for (const nlohmann::json& k : luts)
      if (k.is_object() && k.contains("file")) c.luts.push_back({k.value("hour", 12.0f), k.value("file", std::string())});
    std::sort(c.luts.begin(), c.luts.end(), [](const LutKey& a, const LutKey& b) { return a.hour < b.hour; });
  }
  c.lutStrength = j.value("lutStrength", c.lutStrength);
  const nlohmann::json masks = j.value("masks", nlohmann::json::object());
  c.lightMask = masks.value("lights", c.lightMask);
  c.heightMap = masks.value("height", c.heightMap);
  c.waterMask = masks.value("water", c.waterMask);
  c.mistMask = masks.value("mist", c.mistMask);
  const nlohmann::json hours = j.value("dayNight", nlohmann::json::object());
  c.nightFrom = hours.value("nightFrom", c.nightFrom);
  c.nightFull = hours.value("nightFull", c.nightFull);
  c.dayFrom = hours.value("dayFrom", c.dayFrom);
  c.dayFull = hours.value("dayFull", c.dayFull);
  const nlohmann::json lights = j.value("lights", nlohmann::json::object());
  c.lightColor = vec3Of(lights.value("color", nlohmann::json()), c.lightColor);
  c.lightStrength = lights.value("strength", c.lightStrength);
  c.lightFlicker = lights.value("flicker", c.lightFlicker);
  c.wispScale = lights.value("wispScale", c.wispScale);
  c.wispSize = lights.value("wispSize", c.wispSize);
  c.wispSpeed = lights.value("wispSpeed", c.wispSpeed);
  c.lightGlowRadius = lights.value("glowRadius", c.lightGlowRadius);
  c.lightGlowDownsample = std::max(1, lights.value("glowDownsample", c.lightGlowDownsample));
  const nlohmann::json relief = j.value("relief", nlohmann::json::object());
  c.reliefStrength = relief.value("strength", c.reliefStrength);
  c.sunLean = relief.value("sunLean", c.sunLean);
  const nlohmann::json clouds = j.value("clouds", nlohmann::json::object());
  c.cloudStrength = clouds.value("strength", c.cloudStrength);
  c.cloudScale = clouds.value("scale", c.cloudScale);
  c.cloudSpeed = clouds.value("speed", c.cloudSpeed);
  const nlohmann::json mist = j.value("mist", nlohmann::json::object());
  c.mistColor = vec3Of(mist.value("color", nlohmann::json()), c.mistColor);
  c.mistStrength = mist.value("strength", c.mistStrength);
  c.mistScale = mist.value("scale", c.mistScale);
  c.mistSpeed = mist.value("speed", c.mistSpeed);
  c.mistThreshold = mist.value("threshold", c.mistThreshold);
  c.mistDawnFrom = mist.value("dawnFrom", c.mistDawnFrom);
  c.mistDawnPeak = mist.value("dawnPeak", c.mistDawnPeak);
  c.mistDawnEnd = mist.value("dawnEnd", c.mistDawnEnd);
  c.mistNight = mist.value("night", c.mistNight);
  const nlohmann::json water = j.value("water", nlohmann::json::object());
  c.waterShimmer = water.value("shimmer", c.waterShimmer);
  c.waterMoon = water.value("moon", c.waterMoon);
  c.waterScale = water.value("scale", c.waterScale);
  c.waterSpeed = water.value("speed", c.waterSpeed);
  c.moonColor = vec3Of(water.value("moonColor", nlohmann::json()), c.moonColor);
  const nlohmann::json party = j.value("partyLight", nlohmann::json::object());
  c.partyRadius = party.value("radius", c.partyRadius);
  c.partyDark = party.value("darkness", c.partyDark);
  c.lightsFrom = lights.value("from", c.lightsFrom);
  c.lightsFull = lights.value("full", c.lightsFull);
  c.waterDayColor = vec3Of(water.value("dayColor", nlohmann::json()), c.waterDayColor);
  c.waterDuskColor = vec3Of(water.value("duskColor", nlohmann::json()), c.waterDuskColor);
  c.duskFrom = hours.value("duskFrom", c.duskFrom);
  c.duskPeak = hours.value("duskPeak", c.duskPeak);
  c.duskEnd = hours.value("duskEnd", c.duskEnd);
  // Submundo, o look 2D no ciclo invertido.
  c.fungalMask = masks.value("fungal", c.fungalMask);
  c.plantMask = masks.value("plants", c.plantMask);
  c.windowMask = masks.value("windows", c.windowMask);
  c.emberMask = masks.value("ember", c.emberMask);
  c.heatMask = masks.value("heat", c.heatMask);
  c.sporeMask = masks.value("spores", c.sporeMask);
  const nlohmann::json fungal = j.value("fungal", nlohmann::json::object());
  c.fungalColor = vec3Of(fungal.value("color", nlohmann::json()), c.fungalColor);
  c.fungalStrength = fungal.value("strength", c.fungalStrength);
  c.breathe = fungal.value("breathe", c.breathe);
  c.underWaterTint = vec3Of(fungal.value("waterReflection", nlohmann::json()), c.underWaterTint);
  const nlohmann::json plants = j.value("plants", nlohmann::json::object());
  c.plantColor = vec3Of(plants.value("color", nlohmann::json()), c.plantColor);
  c.plantStrength = plants.value("strength", c.plantStrength);
  const nlohmann::json windows = j.value("windows", nlohmann::json::object());
  c.windowColor = vec3Of(windows.value("color", nlohmann::json()), c.windowColor);
  c.windowStrength = windows.value("strength", c.windowStrength);
  const nlohmann::json ember = j.value("ember", nlohmann::json::object());
  c.emberColor = vec3Of(ember.value("color", nlohmann::json()), c.emberColor);
  c.emberStrength = ember.value("strength", c.emberStrength);
  c.emberThrob = ember.value("throb", c.emberThrob);
  c.emberThrobSpeed = ember.value("throbSpeed", c.emberThrobSpeed);
  c.heatAmplitude = ember.value("heatAmplitude", c.heatAmplitude);
  c.heatSpeed = ember.value("heatSpeed", c.heatSpeed);
  const nlohmann::json spores = j.value("spores", nlohmann::json::object());
  c.sporeScale = spores.value("scale", c.sporeScale);
  c.sporeSpeed = spores.value("speed", c.sporeSpeed);
  c.sporeSize = spores.value("size", c.sporeSize);
  c.sporeStrength = spores.value("strength", c.sporeStrength);
  const nlohmann::json creatures = j.value("creatures", nlohmann::json::object());
  c.creatureColor = vec3Of(creatures.value("color", nlohmann::json()), c.creatureColor);
  c.creatureStrength = creatures.value("strength", c.creatureStrength);
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

std::pair<uint32_t, uint32_t> WorldMapFx::loadLightMask(const std::filesystem::path& file, int downsample, int radius) {
  int w = 0, h = 0, n = 0;
  stbi_uc* pixels = stbi_load(file.string().c_str(), &w, &h, &n, 4);
  if (!pixels) {
    logError("worldmap: light mask not read: {}", file.string());
    return {kNoTexture, kNoTexture};
  }
  Image sharpImage;
  const uint32_t sharp = upload(pixels, static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM, 4, sharpImage, kNoTexture);
  images_.push_back(sharpImage);
  indices_.push_back(sharp);
  // A cópia reduzida (média de blocos), depois três passes de caixa separáveis = um borrado quase gaussiano.
  const int ds = std::max(1, downsample);
  const int gw = std::max(1, w / ds), gh = std::max(1, h / ds);
  std::vector<float> small(size_t(gw) * gh * 3, 0.0f);
  for (int y = 0; y < gh; ++y)
    for (int x = 0; x < gw; ++x) {
      float acc[3] = {0, 0, 0};
      int count = 0;
      for (int sy = y * ds; sy < std::min(h, (y + 1) * ds); ++sy)
        for (int sx = x * ds; sx < std::min(w, (x + 1) * ds); ++sx) {
          const stbi_uc* p = pixels + (size_t(sy) * w + sx) * 4;
          acc[0] += p[0], acc[1] += p[1], acc[2] += p[2], ++count;
        }
      float* o = &small[(size_t(y) * gw + x) * 3];
      for (int k = 0; k < 3; ++k) o[k] = acc[k] / float(std::max(count, 1)) / 255.0f;
    }
  stbi_image_free(pixels);
  const int r = std::max(1, radius);
  std::vector<float> tmp(small.size());
  auto boxH = [&](const std::vector<float>& in, std::vector<float>& out) {
    for (int y = 0; y < gh; ++y)
      for (int k = 0; k < 3; ++k) {
        float sum = 0;
        for (int x = -r; x <= r; ++x) sum += in[(size_t(y) * gw + std::clamp(x, 0, gw - 1)) * 3 + k];
        for (int x = 0; x < gw; ++x) {
          out[(size_t(y) * gw + x) * 3 + k] = sum / float(2 * r + 1);
          sum += in[(size_t(y) * gw + std::clamp(x + r + 1, 0, gw - 1)) * 3 + k] - in[(size_t(y) * gw + std::clamp(x - r, 0, gw - 1)) * 3 + k];
        }
      }
  };
  auto boxV = [&](const std::vector<float>& in, std::vector<float>& out) {
    for (int x = 0; x < gw; ++x)
      for (int k = 0; k < 3; ++k) {
        float sum = 0;
        for (int y = -r; y <= r; ++y) sum += in[(size_t(std::clamp(y, 0, gh - 1)) * gw + x) * 3 + k];
        for (int y = 0; y < gh; ++y) {
          out[(size_t(y) * gw + x) * 3 + k] = sum / float(2 * r + 1);
          sum += in[(size_t(std::clamp(y + r + 1, 0, gh - 1)) * gw + x) * 3 + k] - in[(size_t(std::clamp(y - r, 0, gh - 1)) * gw + x) * 3 + k];
        }
      }
  };
  for (int pass = 0; pass < 3; ++pass) boxH(small, tmp), boxV(tmp, small);
  // O ganho do halo: o de UM ponto isolado (um texel a 1.0 pelo mesmo borrado) chega a kHaloPeak; um aglomerado denso soma e
  // satura em 1, nunca é normalizado pelo seu próprio pico (o usuário, 2026-10-29: o castelo inteiro virava uma mancha laranja).
  constexpr float kHaloPeak = 0.12f;  // 0.35 ainda engolia a capela inteira (o usuário, 2026-10-29: "I told its too intense")
  float unitPeak = 1.0f;
  {
    const int n = 4 * r + 9;
    std::vector<float> one(size_t(n) * n, 0.0f), t2(one.size());
    one[(size_t(n / 2) * n + n / 2)] = 1.0f;
    auto box = [&](const std::vector<float>& in, std::vector<float>& out, bool horizontal) {
      for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
          float sum = 0;
          for (int k = -r; k <= r; ++k) {
            const int xx = horizontal ? std::clamp(x + k, 0, n - 1) : x, yy = horizontal ? y : std::clamp(y + k, 0, n - 1);
            sum += in[size_t(yy) * n + xx];
          }
          out[size_t(y) * n + x] = sum / float(2 * r + 1);
        }
    };
    for (int pass = 0; pass < 3; ++pass) box(one, t2, true), box(t2, one, false);
    unitPeak = std::max(one[(size_t(n / 2) * n + n / 2)], 1e-6f);
  }
  const float gain = kHaloPeak / unitPeak;
  std::vector<unsigned char> texels(size_t(gw) * gh * 4, 255);
  for (size_t i = 0; i < size_t(gw) * gh; ++i)
    for (int k = 0; k < 3; ++k) texels[i * 4 + k] = static_cast<unsigned char>(std::lround(std::clamp(small[i * 3 + k] * gain, 0.0f, 1.0f) * 255.0f));
  Image glowImage;
  const uint32_t glow = upload(texels.data(), static_cast<uint32_t>(gw), static_cast<uint32_t>(gh), VK_FORMAT_R8G8B8A8_UNORM, 4, glowImage, kNoTexture);
  images_.push_back(glowImage);
  indices_.push_back(glow);
  logInfo("worldmap: light mask {} {}x{} + glow {}x{} (radius {}) on the GPU", file.filename().string(), w, h, gw, gh, r);
  return {sharp, glow};
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
  const bool hasLut = d.lutA != kNoTexture && d.lutB != kNoTexture;
  push.lutA = hasLut ? d.lutA : self.noiseIndex_;
  push.lutB = hasLut ? d.lutB : self.noiseIndex_;
  uint32_t texFlags = 0;
  // tex0 / tex1 (a máscara e o seu borrado) só valem juntos; tex6 / tex7 (o ember e o seu borrado) também.
  for (int i = 0; i < Draw::kTextures; ++i) {
    bool has = d.tex[i] != kNoTexture;
    if (i == 0 || i == 1) has = d.tex[0] != kNoTexture && d.tex[1] != kNoTexture;
    if (i == 6 || i == 7) has = d.tex[6] != kNoTexture && d.tex[7] != kNoTexture;
    push.tex[i] = has ? d.tex[i] : self.noiseIndex_;
    if (has) texFlags |= 64u << i;
  }
  push.flags = (d.underworld ? 1u : 0u) | (uint32_t(std::clamp(d.quality, 0, 2)) << 1) | (hasMask ? 8u : 0u) | (hasReveal ? 16u : 0u) |
               (hasLut ? 32u : 0u) | texFlags | (d.partyLight && !d.underworld ? 32768u : 0u);
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

WorldMapFx::LutMix WorldMapFx::lutMix(const WorldMapConfig& c, float hour) {
  LutMix m;
  if (c.luts.empty()) return m;
  hour = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
  const int n = static_cast<int>(c.luts.size());
  m.a = n - 1, m.b = 0;
  float ha = c.luts.back().hour - 24.0f, hb = c.luts.front().hour;
  for (int i = 0; i < n; ++i) {
    const float h0 = c.luts[i].hour, h1 = i + 1 < n ? c.luts[i + 1].hour : c.luts.front().hour + 24.0f;
    if (hour >= h0 && hour < h1) m.a = i, m.b = (i + 1) % n, ha = h0, hb = h1;
  }
  m.t = hb > ha ? smooth(std::clamp((hour - ha) / (hb - ha), 0.0f, 1.0f)) : 0.0f;
  return m;
}

WorldMapParams WorldMapFx::overworldParams(const WorldMapConfig& c, float hour, glm::vec2 partyUv) {
  WorldMapParams p{};
  // O look 2D pela hora (worldmap.hlsl 13..21): a mistura das LUTs, a noite e o dia como rampas, a névoa da madrugada (e uma
  // parte dela a noite inteira), o sol correndo de leste (x +) a oeste pelo sul (y +: o sul é para baixo na pintura).
  {
    const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
    const float night = hourWindow(h, c.nightFrom, c.nightFull, c.dayFrom, std::max(c.dayFull - c.dayFrom, 0.01f));
    const float day = 1.0f - night;
    const float dawn = hourWindow(h, c.mistDawnFrom, c.mistDawnPeak, c.mistDawnPeak, std::max(c.mistDawnEnd - c.mistDawnPeak, 0.01f));
    const float mistAmount = std::max(dawn, night * c.mistNight);
    p.hourMix = {lutMix(c, h).t, night, day, mistAmount};
    const float dayT = std::clamp((h - 6.0f) / 12.0f, 0.0f, 1.0f);  // 6 h leste .. 18 h oeste
    const float az = dayT * std::numbers::pi_v<float>;
    const float sinEl = std::max(std::sin(az), 0.08f);  // a altura do sol: baixa cedo e tarde, alta ao meio-dia
    const float cosEl = std::sqrt(std::max(1.0f - sinEl * sinEl, 0.0f));
    glm::vec3 sun{std::cos(az) * cosEl, c.sunLean * std::sin(az) * cosEl, sinEl};
    sun = glm::normalize(sun);
    p.sun = {sun.x, sun.y, sun.z, c.reliefStrength * day};
    p.lightColor = {c.lightColor, c.lightStrength};
    p.clouds = {c.cloudStrength, c.cloudScale, c.cloudSpeed, std::clamp(c.lightFlicker, 0.0f, 1.0f)};
    // As luzes sobem no entardecer e apagam com o dia; o reflexo da água segue a luz principal: o sol, o vermelho do
    // entardecer, a lua.
    const float lightsOn = hourWindow(h, c.lightsFrom, c.lightsFull, c.dayFrom, std::max(c.dayFull - c.dayFrom, 0.01f));
    const float dusk = hourWindow(h, c.duskFrom, c.duskPeak, c.duskPeak, std::max(c.duskEnd - c.duskPeak, 0.01f));
    glm::vec3 reflect = c.waterDayColor * day + c.moonColor * night;
    reflect = reflect * (1.0f - dusk) + c.waterDuskColor * dusk;
    p.mistShape = {c.mistScale, c.mistSpeed, c.mistThreshold, lightsOn};
    p.mistColor = {c.mistColor, c.mistStrength};
    p.water = {c.waterShimmer, c.waterMoon, c.waterScale, c.waterSpeed};
    p.party = {partyUv.x, partyUv.y, c.partyRadius, c.partyDark};
    p.moon = {reflect, c.lutStrength};
    p.spare = {c.wispScale, c.wispSize, c.wispSpeed, 0.0f};
  }
  if (c.keys.empty()) {
    p.tint = {1, 1, 1, 1};
    p.grade = {1, 1, 0, 1};
    return p;
  }
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

float WorldMapFx::bloomAt(const WorldMapConfig& c, float hour) {
  const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
  return hourWindow(h, c.nightFrom, c.nightFull, c.dayFrom, std::max(c.dayFull - c.dayFrom, 0.01f));
}

WorldMapParams WorldMapFx::underworldLookParams(const WorldMapConfig& c, float hour) {
  WorldMapParams p{};
  p.tint = {1, 1, 1, 1};
  p.grade = {1, 1, 0, 1};
  p.light = {0, -1, 0, 0};
  p.vignette = {c.vignette, c.vignetteRadius, c.vignetteSoft, c.shoulder};
  const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
  const float L = bloomAt(c, h);
  p.hourMix = {lutMix(c, h).t, L, 1.0f - L, 0.0f};
  p.sun = {c.fungalColor, c.fungalStrength};
  p.lightColor = {c.plantColor, c.plantStrength};
  p.clouds = {c.windowColor, c.windowStrength};
  p.mistShape = {c.emberColor, c.emberStrength};
  p.mistColor = {c.emberThrob, c.emberThrobSpeed, c.heatAmplitude, c.heatSpeed};
  p.water = {c.waterShimmer, c.waterMoon, c.waterScale, c.waterSpeed};
  p.party = {c.underWaterTint, c.breathe};
  p.moon = {c.sporeScale, c.sporeSpeed, c.sporeSize, c.sporeStrength};
  p.creatures = {c.creatureColor, c.creatureStrength};
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
  p.extra0 = {c.glowAmbient, 0, 0, 0};
  return p;
}

}  // namespace ember
