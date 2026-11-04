#pragma once
#include <array>
#include <glm/glm.hpp>
#include "render/Camera.h"
#include "render/Volumetrics.h"
#include "rhi/Image.h"
namespace ember {
class VulkanContext; class Bindless; class GpuTimer;
class Clouds006 {
 public:
  struct Weather {
    glm::vec4 low{380,260,.4f,.7f}; // base, thickness, coverage, stratus-to-cumulus
    glm::vec4 upper{.15f,.1f,0,0}; // middle, high, vertical growth, rain
    glm::vec2 wind{8,3};
  };
  struct Output { VkDeviceAddress record; uint32_t colour; glm::vec2 size; };
  Clouds006(VulkanContext&,Bindless&);
  ~Clouds006();
  Output prepare(VkCommandBuffer,uint32_t,const Camera&,glm::uvec2,Volumetrics::Quality,const Weather&,float);
  void record(VkCommandBuffer,VkDeviceAddress,GpuTimer&);
  void invalidate(){valid_=false;}
 private:
  struct Texture {Image image;uint32_t sampled=~0u;std::array<VkImageView,6> views{};std::array<uint32_t,6> storage{};uint32_t levels=1,layers=1;bool ready=false;};
  Texture make(uint32_t,uint32_t,VkFormat=VK_FORMAT_R16G16B16A16_SFLOAT,uint32_t=1,uint32_t=1);
  Texture makeCube();
  void destroy(Texture&);void initialize(VkCommandBuffer,Texture&);
  VulkanContext& context_;Bindless& bindless_;
  Texture weather_,height_,atlas_,displacement_,probes_,cube_,water_[2],esmRaw_,esm_,raw_,depth_,colour_[2],historyDepth_[2];
  Buffer blue_;
  std::array<Buffer,kFramesInFlight> frames_{};
  std::array<VkPipeline,11> pipelines_{};
  glm::uvec2 size_{0};glm::mat4 previousVP_{1};glm::vec3 eye_{0},forward_{0,0,-1};
  uint32_t serial_=0,parity_=0,esmSize_=0;bool valid_=false;float time_=-1;VkDeviceAddress record_=0;
};
}
