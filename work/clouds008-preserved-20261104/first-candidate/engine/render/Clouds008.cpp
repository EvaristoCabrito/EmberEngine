#include "render/Clouds008.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/Log.h"
#include "rhi/Bindless.h"
#include "rhi/GpuTimer.h"
#include "rhi/Pipeline.h"
#include "rhi/VulkanContext.h"

namespace ember {

namespace {

#include "render/CloudsRdr2BlueNoise.inl"

struct Rdr2Push {  // shaders/clouds_rdr2.hlsl Push
  VkDeviceAddress frame;
  uint32_t destination, width, height, source, mode;
};

void barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
             VkAccessFlags2 dstAccess) {
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
constexpr VkPipelineStageFlags2 kReaders = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

// The presets: march scale (1 = every pixel, 2 = a 2 x 2 block per ray, 4 = 4 x 4), march steps, light taps, scattering
// octaves, weather map size, shadow map size, shadow light taps.
struct Preset {
  uint32_t marchDivisor, steps, lightTaps, octaves, weather, shadow, shadowTaps;
};
constexpr Preset kPresets[4] = {
    {1, 160, 8, 3, 1024, 2048, 24},  // Ultra: the maximum, no time ceiling
    {2, 112, 6, 3, 1024, 2048, 16},   // High: the talk's own numbers
    {2, 80, 5, 3, 1024, 2048, 12},     // Medium
    {4, 56, 4, 3, 1024, 2048, 8},      // Low
};

float hash01(uint32_t& seed) {
  seed = seed * 1664525u + 1013904223u;
  return static_cast<float>((seed >> 8) & 0xFFFFFFu) / 16777216.0f;
}

}  // namespace

Clouds008::Clouds008(VulkanContext& context, Bindless& bindless, uint32_t samplerRepeat, uint32_t samplerClamp)
    : context_(context), bindless_(bindless), samplerRepeat_(samplerRepeat), samplerClamp_(samplerClamp) {
  const VkPipelineLayout layout = bindless_.pipelineLayout();
  weatherPipeline_ = createComputePipeline(context_, layout, "clouds008.CSWeather");
  marchPipeline_ = createComputePipeline(context_, layout, "clouds008.CSMarch");
  resolvePipeline_ = createComputePipeline(context_, layout, "clouds008.CSResolve");
  shadowPipeline_ = createComputePipeline(context_, layout, "clouds008.CSShadow");
  blurPipeline_ = createComputePipeline(context_, layout, "clouds008.CSShadowBlur");
  atlasPipeline_=createComputePipeline(context_,layout,"clouds008.CSAtlas");
  heightPipeline_=createComputePipeline(context_,layout,"clouds008.CSHeight");
  displacementPipeline_=createComputePipeline(context_,layout,"clouds008.CSDisplacement");
  mipPipeline_=createComputePipeline(context_,layout,"clouds008.CSShadowMip");
  ambientPipeline_=createComputePipeline(context_,layout,"clouds008.CSAmbient");
  probesPipeline_=createComputePipeline(context_,layout,"clouds008.CSProbes");
  cubePipeline_=createComputePipeline(context_,layout,"clouds008.CSCube");
  waterPipeline_=createComputePipeline(context_,layout,"clouds008.CSWater");
  probes_=createDeviceBuffer(context_,32*32*9*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  ambient_=createDeviceBuffer(context_,32,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  atlas_=make(128,4,VK_FORMAT_R16G16B16A16_SFLOAT);
  height_=make(1,128,VK_FORMAT_R16G16B16A16_SFLOAT);
  displacement_=make(256,256,VK_FORMAT_R16G16B16A16_SFLOAT);
  cube_=make(32,32,VK_FORMAT_R16G16B16A16_SFLOAT,1,6);
  for(auto& t:water_)t=make(128,64,VK_FORMAT_R16G16B16A16_SFLOAT);
  blue_ = createDeviceBuffer(context_, sizeof(float) * 4096, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  for (Buffer& b : records_)
    b = createDeviceBuffer(context_, sizeof(Rdr2Frame), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  logInfo("clouds 008: passes ready");
}

Clouds008::~Clouds008() {
  vkDeviceWaitIdle(context_.device);
  for (VkPipeline p : {weatherPipeline_, marchPipeline_, resolvePipeline_, shadowPipeline_, blurPipeline_, atlasPipeline_, heightPipeline_, displacementPipeline_, mipPipeline_, ambientPipeline_, probesPipeline_, cubePipeline_, waterPipeline_})
    if (p) vkDestroyPipeline(context_.device, p, nullptr);
  for (Texture* t : {&weather_, &march_, &resolved_[0], &resolved_[1], &shadowRaw_, &shadowTemp_, &shadow_}) destroy(*t);
  for(Texture* t:{&depth_,&historyDepth_[0],&historyDepth_[1],&height_,&atlas_,&displacement_,&middle_,&cube_,&water_[0],&water_[1]})destroy(*t);
  destroyBuffer(context_,probes_);destroyBuffer(context_,ambient_);
  destroyBuffer(context_, blue_);
  for (Buffer& b : records_) destroyBuffer(context_, b);
}

Clouds008::Texture Clouds008::make(uint32_t width, uint32_t height, VkFormat format, uint32_t levels, uint32_t layers) {
  Texture t;
  t.image = createImage(context_, {.format = format, .extent = {width, height}, .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, .mipLevels=levels, .layers=layers, .arrayView=layers>1});
  t.sampled = bindless_.addSampledImage(t.image.view, VK_IMAGE_LAYOUT_GENERAL);
  t.levels=levels;t.layers=layers;
  if(levels>1){
    for(uint32_t i=0;i<levels;++i){
      VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      info.image=t.image.image;info.viewType=VK_IMAGE_VIEW_TYPE_2D;info.format=format;
      info.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,i,1,0,1};
      VK_CHECK(vkCreateImageView(context_.device,&info,nullptr,&t.views[i]));
      t.mipStorage[i]=bindless_.addStorageImage(t.views[i]);
    }
    t.storage=t.mipStorage[0];
  }else t.storage = bindless_.addStorageImage(t.image.view);
  t.width = width;
  t.height = height;
  return t;
}

void Clouds008::destroy(Texture& t) {
  if (t.image.image == VK_NULL_HANDLE) return;
  bindless_.removeSampledImage(t.sampled);
  if(t.levels>1){for(uint32_t i=0;i<t.levels;++i){bindless_.removeStorageImage(t.mipStorage[i]);vkDestroyImageView(context_.device,t.views[i],nullptr);}}
  else bindless_.removeStorageImage(t.storage);
  destroyImage(context_, t.image);
  t = Texture{};
}

void Clouds008::initialize(VkCommandBuffer cmd, Texture& t) {
  if (t.ready) return;
  t.ready = true;
  transition(cmd, t.image.image, t.image.aspect,
             {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
}

void Clouds008::resize(glm::uvec2 screen, int quality) {
  const Preset& p = kPresets[quality];
  const uint32_t w = std::max(1u, (screen.x + p.marchDivisor - 1) / p.marchDivisor);
  const uint32_t h = std::max(1u, (screen.y + p.marchDivisor - 1) / p.marchDivisor);
  if (march_.width != w || march_.height != h) {
    vkDeviceWaitIdle(context_.device);  // the images in flight are read by the frames before this one
    for (Texture* t : {&march_, &resolved_[0], &resolved_[1], &depth_, &historyDepth_[0], &historyDepth_[1]}) destroy(*t);
    march_ = make(w, h, VK_FORMAT_R16G16B16A16_SFLOAT);
    resolved_[0] = make(w, h, VK_FORMAT_R16G16B16A16_SFLOAT);
    resolved_[1] = make(w, h, VK_FORMAT_R16G16B16A16_SFLOAT);
    depth_=make(w,h,VK_FORMAT_R16G16B16A16_SFLOAT);
    for(auto& t:historyDepth_)t=make(w,h,VK_FORMAT_R16G16B16A16_SFLOAT);
    historyValid_ = false;
  }
  if (weatherSize_ != p.weather) {
    vkDeviceWaitIdle(context_.device);
    destroy(weather_);destroy(middle_);
    middle_=make(p.weather,p.weather,VK_FORMAT_R16G16B16A16_SFLOAT);
    weather_ = make(p.weather, p.weather, VK_FORMAT_R16G16B16A16_SFLOAT);
    weatherSize_ = p.weather;
  }
  if (shadowSize_ != p.shadow) {
    vkDeviceWaitIdle(context_.device);
    for (Texture* t : {&shadowRaw_, &shadowTemp_, &shadow_}) destroy(*t);
    shadowRaw_ = make(p.shadow, p.shadow, VK_FORMAT_R16_SFLOAT);
    shadowTemp_ = make(p.shadow, p.shadow, VK_FORMAT_R16_SFLOAT);
    shadow_ = make(p.shadow, p.shadow, VK_FORMAT_R16_SFLOAT,6);
    shadowSize_ = p.shadow;
  }
  screen_ = screen;
  quality_ = quality;
}

Clouds008::Output Clouds008::prepare(VkCommandBuffer cmd, uint32_t frameIndex, const Camera& camera, glm::uvec2 screen,
                                       const Params& params, float time, uint32_t noiseBase, uint32_t noiseDetail) {
  const int quality = std::clamp(params.quality, 0, 3);
  if (screen != screen_ || quality != quality_) resize(screen, quality);
  const Preset& p = kPresets[quality];
  const float dt=previousTime_<0?0:time-previousTime_;
  if(dt<0 || dt>.5f || glm::length(camera.position-previousEye_)>150 || glm::dot(camera.forward,previousForward_)<.85f)historyValid_=false;
  previousTime_=time;previousEye_=camera.position;previousForward_=camera.forward;
  parity_ ^= 1u;
  Rdr2Frame& r = record_;
  r.weatherMap = weather_.sampled;
  r.noiseBase = noiseBase;
  r.noiseDetail = noiseDetail;
  r.samplerRepeat = samplerRepeat_;
  r.samplerClamp = samplerClamp_;
  r.march = march_.sampled;
  r.history = resolved_[parity_ ^ 1u].sampled;
  r.layer = {params.base, params.top, std::max(params.anvilTop, params.top + 1.0f), params.cirrusHeight};
  r.weather = {std::clamp(params.coverage, 0.0f, 1.0f), std::clamp(params.cirrusCoverage, 0.0f, 1.0f), std::clamp(params.kind, 0.0f, 1.0f),
               std::clamp(params.growth, 0.0f, 1.0f)};
  r.wind = {params.wind.x, params.wind.y, time, std::clamp(params.rain, 0.0f, 1.0f)};
  r.marchInfo = {static_cast<float>(march_.width), static_cast<float>(march_.height), static_cast<float>(frame_), static_cast<float>(quality)};
  r.previousViewProj = previousViewProj_;
  r.steps = {static_cast<float>(p.steps), static_cast<float>(p.lightTaps), static_cast<float>(p.octaves), params.lightning};
  // The weather map covers 20,000 units around the world's origin (the boards stand near it; the layer is drawn 9,000 out).
  r.worldRect = {-10000.0f, -10000.0f, 1.0f / 20000.0f, 1.0f / 20000.0f};
  // Lightning: a flash every few seconds (more often with the slider), somewhere in the layer within sight, a double pulse.
  r.flash = {0.0f, 0.0f, 0.0f, 0.0f};
  if (params.lightning > 0.0f) {
    if (nextFlash_ < 0.0f) nextFlash_ = time + 1.0f;
    if (time >= nextFlash_) {
      flashBegan_ = time;
      const float angle = hash01(flashSeed_) * 6.2831853f, distance = 600.0f + hash01(flashSeed_) * 2600.0f;
      flashAt_ = {camera.position.x + std::cos(angle) * distance, params.base + (params.top - params.base) * (0.3f + 0.4f * hash01(flashSeed_)),
                  camera.position.z + std::sin(angle) * distance};
      nextFlash_ = time + (14.0f - 11.5f * std::clamp(params.lightning, 0.0f, 1.0f)) * (0.4f + 1.2f * hash01(flashSeed_));
    }
    const float age = time - flashBegan_;
    if (age >= 0.0f && age < 0.6f) {
      const float pulse = age < 0.06f ? age / 0.06f : (age < 0.14f ? 1.0f : (age < 0.2f ? 0.35f : std::exp(-(age - 0.2f) * 9.0f)));
      const float strength = std::max(params.keyLux, 3000.0f) * 2.5f * pulse;
      r.flash = {flashAt_.x, flashAt_.y, flashAt_.z, strength};
    }
  } else {
    nextFlash_ = -1.0f;
  }
  r.screen = {static_cast<float>(screen.x), static_cast<float>(screen.y), historyValid_ ? 1.0f : 0.0f, params.meanGround};
  shadowThisFrame_ = params.shadows && params.shadowRect.z > 0.0f;
  r.shadow = params.shadowRect;
  r.shadowInfo = {static_cast<float>(shadowSize_), params.shadowStrength, params.shadowHeight, static_cast<float>(p.shadowTaps)};
  r.textures={depth_.sampled,historyDepth_[parity_^1u].sampled,historyDepth_[parity_].sampled,height_.sampled};
  r.maps={atlas_.sampled,displacement_.sampled,middle_.sampled,water_[parity_].sampled};
  r.indirect={cube_.sampled,water_[parity_^1u].sampled,6,quality==0?1u:2u};
  r.temporal={std::max(dt,0.f),historyValid_?1.f:0.f,16.f,r.flash.w>0?1.f:0.f};
  const float thickness=std::max(params.top-params.base,1.f);
  r.middle={params.top+thickness*.8f,params.top+thickness*1.5f,params.middleCoverage,0};
  r.probeRect=params.shadowRect.z>0?params.shadowRect:r.worldRect;
  r.probes=bufferAddress(context_,probes_);r.ambient=bufferAddress(context_,ambient_);
  r.flashes[0]=r.flash;r.controls={params.shadowStrength,params.shadowHeight,0,0};
  r.blueNoiseAddress = bufferAddress(context_, blue_);
  vkCmdUpdateBuffer(cmd, records_[frameIndex].buffer, 0, sizeof(r), &r);
  recordAddress_ = bufferAddress(context_, records_[frameIndex]);
  previousViewProj_ = camera.viewProjection();
  pending_ = true;
  Output out;
  out.record = recordAddress_;
  out.colour = resolved_[parity_].sampled;
  out.size = {static_cast<float>(march_.width), static_cast<float>(march_.height)};
  out.shadowMap = shadowThisFrame_ ? shadow_.sampled : 0xFFFFFFFFu;
  return out;
}

void Clouds008::record(VkCommandBuffer cmd, VkDeviceAddress meshFrame, GpuTimer& timer) {
  if (!pending_) return;
  pending_ = false;
  const VkPipelineLayout layout = bindless_.pipelineLayout();
  const VkShaderStageFlags stages = Bindless::kStages;
  if (!blueReady_) {
    blueReady_ = true;
    float values[4096];
    for (int i = 0; i < 4096; ++i) values[i] = (static_cast<float>(kCloudRdr2BlueNoise[i]) + 0.5f) / 256.0f;
    vkCmdUpdateBuffer(cmd, blue_.buffer, 0, sizeof(values), values);
    barrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
  }
  for (Texture* t : {&weather_, &march_, &resolved_[0], &resolved_[1], &shadowRaw_, &shadowTemp_, &shadow_}) initialize(cmd, *t);
  for(Texture* t:{&depth_,&historyDepth_[0],&historyDepth_[1],&height_,&atlas_,&displacement_,&middle_,&cube_,&water_[0],&water_[1]})initialize(cmd,*t);
  barrier(cmd,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,kReaders,VK_ACCESS_2_SHADER_READ_BIT);
  // Everything last frame's passes read (the history, the shadow, the resolved image the sky sampled) is done with.
  barrier(cmd, kReaders, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
  const auto dispatch = [&](VkPipeline pipeline, const Texture& destination, uint32_t source, uint32_t mode, uint32_t mip=0) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    const uint32_t w=std::max(1u,destination.width>>mip),h=std::max(1u,destination.height>>mip);
    const Rdr2Push push{meshFrame, destination.levels>1?destination.mipStorage[mip]:destination.storage, w,h, source, mode};
    vkCmdPushConstants(cmd, layout, stages, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (w + 7) / 8, (h + 7) / 8, destination.layers);
    barrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, kReaders, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
  };
  {
    const uint32_t scope = timer.begin(cmd, "clouds 008 weather");
    dispatch(atlasPipeline_,atlas_,0,0);
    dispatch(heightPipeline_,height_,0,0);
    dispatch(displacementPipeline_,displacement_,0,0);
    dispatch(weatherPipeline_, weather_, middle_.storage, 0);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,ambientPipeline_);
    const Rdr2Push ambientPush{meshFrame,0,1,1,0,0};
    vkCmdPushConstants(cmd,layout,stages,0,sizeof(ambientPush),&ambientPush);
    vkCmdDispatch(cmd,1,1,1);
    barrier(cmd,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,kReaders,VK_ACCESS_2_SHADER_READ_BIT);
    timer.end(cmd, scope);
  }
  {
    const uint32_t scope = timer.begin(cmd, "clouds 008 march");
    dispatch(marchPipeline_, march_, depth_.storage, 0);
    timer.end(cmd, scope);
  }
  {
    const uint32_t scope = timer.begin(cmd, "clouds 008 resolve");
    dispatch(resolvePipeline_, resolved_[parity_], historyDepth_[parity_].storage, 0);
    timer.end(cmd, scope);
  }
  if (shadowThisFrame_) {
    const uint32_t scope = timer.begin(cmd, "clouds 008 shadow");
    dispatch(shadowPipeline_, shadowRaw_, 0, 0);
    dispatch(blurPipeline_, shadowTemp_, shadowRaw_.sampled, 0);
    dispatch(blurPipeline_, shadow_, shadowTemp_.sampled, 1);
    for(uint32_t mip=1;mip<6;++mip)dispatch(mipPipeline_,shadow_,shadow_.sampled,mip-1,mip);
    timer.end(cmd, scope);
  }
  {
    const uint32_t scope=timer.begin(cmd,"clouds 008 reflections");
    Texture probeDispatch;probeDispatch.width=32;probeDispatch.height=32;
    dispatch(probesPipeline_,probeDispatch,0,0);
    dispatch(cubePipeline_,cube_,0,0);
    dispatch(waterPipeline_,water_[parity_],0,0);
    timer.end(cmd,scope);
  }
  historyValid_ = true;
  ++frame_;
}

const Clouds008::Timing& Clouds008::timing(const GpuTimer& timer) {
  for (const GpuTimer::Result& r : timer.results()) {
    if (std::strcmp(r.name, "clouds 008 weather") == 0) timing_.weather = r.milliseconds;
    else if (std::strcmp(r.name, "clouds 008 march") == 0) timing_.march = r.milliseconds;
    else if (std::strcmp(r.name, "clouds 008 resolve") == 0) timing_.resolve = r.milliseconds;
    else if (std::strcmp(r.name, "clouds 008 shadow") == 0) timing_.shadow = r.milliseconds;
    else if(std::strcmp(r.name,"clouds 008 reflections")==0)timing_.reflections=r.milliseconds;
  }
  return timing_;
}

}  // namespace ember

