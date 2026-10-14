#pragma once

#include <string>
#include <vector>

#include "rhi/VkCommon.h"

namespace ember {

class VulkanContext;

// Loads <exe dir>/shaders/<name>.spv (built from shaders/*.hlsl by CMake).
VkShaderModule loadShader(VulkanContext& context, const std::string& name);

struct GraphicsPipelineDesc {
  std::string vertexShader;    // e.g. "triangle.VSMain"
  std::string fragmentShader;  // e.g. "triangle.PSMain"
  std::vector<VkFormat> colorFormats;
  VkFormat depthFormat = VK_FORMAT_UNDEFINED;
  bool depthTest = false;
  bool depthWrite = false;
  VkCompareOp depthCompare = VK_COMPARE_OP_GREATER_OR_EQUAL;  // reversed-Z: near = 1, far = 0
  VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
  VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  bool alphaBlend = false;
  bool premultipliedAlpha = false;  // with alphaBlend: colour is already multiplied by alpha
};

// Dynamic-rendering pipeline on the shared bindless layout; viewport and scissor are dynamic.
VkPipeline createGraphicsPipeline(VulkanContext& context, VkPipelineLayout layout, const GraphicsPipelineDesc& desc);

}  // namespace ember
