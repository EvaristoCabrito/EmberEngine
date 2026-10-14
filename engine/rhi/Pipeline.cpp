#include "rhi/Pipeline.h"

#include <SDL3/SDL_filesystem.h>

#include <array>
#include <fstream>

#include "rhi/VulkanContext.h"

namespace ember {

VkShaderModule loadShader(VulkanContext& context, const std::string& name) {
  const std::string path = std::string(SDL_GetBasePath()) + "shaders/" + name + ".spv";
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  EMBER_CHECK(file, std::format("Missing shader {}", path));
  const auto size = static_cast<size_t>(file.tellg());
  EMBER_CHECK(size % 4 == 0 && size > 0, std::format("Corrupt shader {}", path));
  std::vector<uint32_t> code(size / 4);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));

  VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  info.codeSize = size;
  info.pCode = code.data();
  VkShaderModule module;
  VK_CHECK(vkCreateShaderModule(context.device, &info, nullptr, &module));
  return module;
}

VkPipeline createGraphicsPipeline(VulkanContext& context, VkPipelineLayout layout, const GraphicsPipelineDesc& desc) {
  VkShaderModule vs = loadShader(context, desc.vertexShader);
  VkShaderModule fs = loadShader(context, desc.fragmentShader);
  // DXC keeps the HLSL entry point name in the SPIR-V.
  const std::string vsEntry = desc.vertexShader.substr(desc.vertexShader.find('.') + 1);
  const std::string fsEntry = desc.fragmentShader.substr(desc.fragmentShader.find('.') + 1);
  const std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, vsEntry.c_str(), nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, fsEntry.c_str(), nullptr},
  }};

  VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  assembly.topology = desc.topology;
  VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = desc.cullMode;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depth.depthTestEnable = desc.depthTest;
  depth.depthWriteEnable = desc.depthWrite;
  depth.depthCompareOp = desc.depthCompare;

  std::vector<VkPipelineColorBlendAttachmentState> attachments(desc.colorFormats.size());
  for (auto& a : attachments) {
    a.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                       VK_COLOR_COMPONENT_A_BIT;
    if (desc.alphaBlend) {
      a.blendEnable = VK_TRUE;
      a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      a.colorBlendOp = VK_BLEND_OP_ADD;
      a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      a.alphaBlendOp = VK_BLEND_OP_ADD;
    }
  }
  VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blend.attachmentCount = static_cast<uint32_t>(attachments.size());
  blend.pAttachments = attachments.data();

  const std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamic.pDynamicStates = dynamicStates.data();

  VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  rendering.colorAttachmentCount = static_cast<uint32_t>(desc.colorFormats.size());
  rendering.pColorAttachmentFormats = desc.colorFormats.data();
  rendering.depthAttachmentFormat = desc.depthFormat;

  VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  info.pNext = &rendering;
  info.stageCount = static_cast<uint32_t>(stages.size());
  info.pStages = stages.data();
  info.pVertexInputState = &vertexInput;
  info.pInputAssemblyState = &assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &raster;
  info.pMultisampleState = &multisample;
  info.pDepthStencilState = &depth;
  info.pColorBlendState = &blend;
  info.pDynamicState = &dynamic;
  info.layout = layout;
  VkPipeline pipeline;
  VK_CHECK(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
  vkDestroyShaderModule(context.device, vs, nullptr);
  vkDestroyShaderModule(context.device, fs, nullptr);
  return pipeline;
}

}  // namespace ember
