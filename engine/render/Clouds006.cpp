#include "render/Clouds006.h"
#include "rhi/Bindless.h"
#include "rhi/GpuTimer.h"
#include "rhi/Pipeline.h"
#include "rhi/VulkanContext.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "render/Clouds006BlueNoise.inl"
namespace ember {
namespace {
struct Record {glm::mat4 previousVP;glm::vec4 rect,low,upper,windTime;glm::uvec4 fields,history,settings,sampling,extras,extras2;};
static_assert(sizeof(Record)==224);
struct Push{VkDeviceAddress frame;uint32_t destination,extra,source,mip,width,height;};
void sync(VkCommandBuffer cmd){
 VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};b.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
 b.srcAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;
 b.dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
 b.dstAccessMask=VK_ACCESS_2_SHADER_SAMPLED_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
 VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.memoryBarrierCount=1;d.pMemoryBarriers=&b;vkCmdPipelineBarrier2(cmd,&d);
}
}
Clouds006::Texture Clouds006::make(uint32_t x,uint32_t y,VkFormat format,uint32_t levels,uint32_t depth){
 Texture t;t.levels=levels;
 t.image=createImage(context_,{.format=format,.extent={x,y},.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,.mipLevels=levels,.depth=depth});
 t.sampled=bindless_.addSampledImage(t.image.view,VK_IMAGE_LAYOUT_GENERAL);
 for(uint32_t i=0;i<levels;++i){VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  info.image=t.image.image;info.viewType=depth>1?VK_IMAGE_VIEW_TYPE_3D:VK_IMAGE_VIEW_TYPE_2D;info.format=format;info.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,i,1,0,1};
  VK_CHECK(vkCreateImageView(context_.device,&info,nullptr,&t.views[i]));t.storage[i]=bindless_.addStorageImage(t.views[i]);}
 return t;
}
Clouds006::Texture Clouds006::makeCube(){
 Texture t;t.layers=6;t.image.format=VK_FORMAT_R16G16B16A16_SFLOAT;t.image.extent={32,32};t.image.aspect=VK_IMAGE_ASPECT_COLOR_BIT;
 VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.flags=VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
 image.imageType=VK_IMAGE_TYPE_2D;image.format=t.image.format;image.extent={32,32,1};image.mipLevels=1;image.arrayLayers=6;
 image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;
 image.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
 VmaAllocationCreateInfo allocation{};allocation.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
 VK_CHECK(vmaCreateImage(context_.allocator,&image,&allocation,&t.image.image,&t.image.allocation,nullptr));
 VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=t.image.image;v.format=t.image.format;
 v.viewType=VK_IMAGE_VIEW_TYPE_CUBE;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,6};
 VK_CHECK(vkCreateImageView(context_.device,&v,nullptr,&t.image.view));
 t.sampled=bindless_.addSampledImage(t.image.view,VK_IMAGE_LAYOUT_GENERAL);v.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;
 VK_CHECK(vkCreateImageView(context_.device,&v,nullptr,&t.views[0]));t.storage[0]=bindless_.addStorageImage(t.views[0]);return t;
}
void Clouds006::destroy(Texture& t){if(!t.image.image)return;bindless_.removeSampledImage(t.sampled);
 for(uint32_t i=0;i<t.levels;++i){bindless_.removeStorageImage(t.storage[i]);vkDestroyImageView(context_.device,t.views[i],nullptr);}
 destroyImage(context_,t.image);t={};}
void Clouds006::initialize(VkCommandBuffer cmd,Texture& t){if(t.ready)return;t.ready=true;
 VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
 b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.dstStageMask=VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
 b.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;b.image=t.image.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,t.levels,0,t.layers};
 VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.imageMemoryBarrierCount=1;d.pImageMemoryBarriers=&b;vkCmdPipelineBarrier2(cmd,&d);
 VkClearColorValue clear{{0,0,0,1}};if(t.image.format==VK_FORMAT_R16_SFLOAT)clear.float32[0]=std::exp(8.f);
 vkCmdClearColorImage(cmd,t.image.image,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&b.subresourceRange);sync(cmd);
}
Clouds006::Clouds006(VulkanContext& c,Bindless& b):context_(c),bindless_(b){
 const char* names[]={"clouds006.CSWeather","clouds006.CSHeight","clouds006.CSEsm","clouds006.CSFilter","clouds006.CSMarch","clouds006.CSResolve",
  "clouds006.CSAtlas","clouds006.CSDisplacement","clouds006.CSProbes","clouds006.CSCube","clouds006.CSWater"};
 for(uint32_t i=0;i<11;++i)pipelines_[i]=createComputePipeline(c,b.pipelineLayout(),names[i]);
 blue_=createHostBuffer(c,sizeof(kCloud006BlueNoise),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
 std::memcpy(blue_.mapped,kCloud006BlueNoise,sizeof(kCloud006BlueNoise));
 for(auto& f:frames_)f=createDeviceBuffer(c,sizeof(Record),VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}
Clouds006::~Clouds006(){for(auto p:pipelines_)vkDestroyPipeline(context_.device,p,nullptr);for(auto& f:frames_)destroyBuffer(context_,f);destroyBuffer(context_,blue_);
 for(Texture* t:{&weather_,&height_,&atlas_,&displacement_,&probes_,&cube_,&water_[0],&water_[1],&esmRaw_,&esm_,&raw_,&depth_,&colour_[0],&colour_[1],&historyDepth_[0],&historyDepth_[1]})destroy(*t);}
Clouds006::Output Clouds006::prepare(VkCommandBuffer cmd,uint32_t slot,const Camera& camera,glm::uvec2 framebuffer,Volumetrics::Quality quality,const Weather& weather,float time){
 const uint32_t q=std::clamp(static_cast<uint32_t>(quality),0u,3u),divisor=q==0?4u:2u;
 const glm::uvec2 wanted=glm::max((framebuffer+divisor-1u)/divisor,glm::uvec2(1));
 if(!weather_.image.image){weather_=make(512,512,VK_FORMAT_R16G16_SFLOAT);height_=make(1,128);atlas_=make(128,12);
  displacement_=make(256,256,VK_FORMAT_R16G16_SFLOAT);probes_=make(32,32,VK_FORMAT_R16G16B16A16_SFLOAT,1,9);cube_=makeCube();
  for(auto& water:water_)water=make(512,256);
 }
 const uint32_t shadowSize=q==0?256u:q==1?384u:q==2?512u:768u;
 if(esmSize_!=shadowSize){if(esmSize_)VK_CHECK(vkDeviceWaitIdle(context_.device));destroy(esmRaw_);destroy(esm_);
  esmRaw_=make(shadowSize,shadowSize,VK_FORMAT_R16_SFLOAT);esm_=make(shadowSize,shadowSize,VK_FORMAT_R16_SFLOAT,6);esmSize_=shadowSize;valid_=false;}
 if(wanted!=size_){if(size_.x)VK_CHECK(vkDeviceWaitIdle(context_.device));
  for(Texture* t:{&raw_,&depth_,&colour_[0],&colour_[1],&historyDepth_[0],&historyDepth_[1]})destroy(*t);
  raw_=make(wanted.x,wanted.y);depth_=make(wanted.x,wanted.y);
  for(auto& t:colour_)t=make(wanted.x,wanted.y);for(auto& t:historyDepth_)t=make(wanted.x,wanted.y);size_=wanted;valid_=false;}
 const bool reset=!valid_||glm::length(camera.position-eye_)>10.f||glm::dot(camera.forward,forward_)<.9f||time<time_||time-time_>.5f;
 parity_=serial_&1u;
 static constexpr uint32_t steps[]={40,56,72,112},lightSteps[]={2,3,4,6},stride[]={2,2,2,2};
 const VkDeviceAddress blueAddress=bufferAddress(context_,blue_);
 Record r{previousVP_,{-8192,-8192,1.f/16384,1.f/16384},weather.low,weather.upper,
  {weather.wind,time,std::clamp(time-time_,0.f,.25f)},
  {weather_.sampled,height_.sampled,esm_.sampled,atlas_.sampled},
  {colour_[parity_^1u].sampled,historyDepth_[parity_^1u].sampled,raw_.sampled,depth_.sampled},
  {serial_,reset?0u:1u,steps[q],lightSteps[q]}, {stride[q],size_.x,size_.y,q},
  {probes_.sampled,cube_.sampled,water_[parity_].sampled,water_[parity_^1u].sampled},
  {displacement_.sampled,static_cast<uint32_t>(blueAddress),static_cast<uint32_t>(blueAddress>>32),historyDepth_[parity_].sampled}};
 sync(cmd);vkCmdUpdateBuffer(cmd,frames_[slot].buffer,0,sizeof(r),&r);sync(cmd);record_=bufferAddress(context_,frames_[slot]);
 previousVP_=camera.viewProjection();eye_=camera.position;forward_=camera.forward;time_=time;
 return {record_,colour_[parity_].sampled,glm::vec2(size_)};
}
void Clouds006::record(VkCommandBuffer cmd,VkDeviceAddress frame,GpuTimer& timer){
 for(Texture* t:{&weather_,&height_,&atlas_,&displacement_,&probes_,&cube_,&water_[0],&water_[1],&esmRaw_,&esm_,&raw_,&depth_,&colour_[0],&colour_[1],&historyDepth_[0],&historyDepth_[1]})initialize(cmd,*t);
 const auto pass=[&](uint32_t pipeline,const char* name,Texture& t,uint32_t extra=0,uint32_t source=0,uint32_t mip=0,uint32_t z=1){
  const uint32_t scope=timer.begin(cmd,name);sync(cmd);
  const uint32_t x=std::max(1u,t.image.extent.width>>mip),y=std::max(1u,t.image.extent.height>>mip);
  const Push p{frame,t.storage[mip],extra,source,mip,x,y};vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines_[pipeline]);
  vkCmdPushConstants(cmd,bindless_.pipelineLayout(),Bindless::kStages,0,sizeof(p),&p);vkCmdDispatch(cmd,(x+7)/8,(y+7)/8,z);sync(cmd);timer.end(cmd,scope);
 };
 pass(6,"cloud 006 height atlas",atlas_);pass(7,"cloud 006 displacement",displacement_);
 pass(0,"cloud 006 map",weather_);pass(1,"cloud 006 height",height_);pass(2,"cloud 006 shadow",esmRaw_);
 for(uint32_t mip=0;mip<6;++mip)pass(3,"cloud 006 shadow filter",esm_,0,mip?esm_.sampled:esmRaw_.sampled,mip);
 pass(4,"cloud 006 march",raw_,depth_.storage[0]);pass(5,"cloud 006 reconstruction",colour_[parity_],historyDepth_[parity_].storage[0]);
 pass(8,"cloud 006 sky probes",probes_);pass(9,"cloud 006 cube reflection",cube_,0,0,0,6);pass(10,"cloud 006 water reflection",water_[parity_]);
 valid_=true;++serial_;
}
}
