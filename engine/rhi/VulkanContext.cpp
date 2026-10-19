#include "rhi/VulkanContext.h"

#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <string>
#include <vector>

namespace ember {

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

bool hasLayer(const char* name) {
  uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> layers(count);
  vkEnumerateInstanceLayerProperties(&count, layers.data());
  for (const auto& layer : layers)
    if (std::strcmp(layer.layerName, name) == 0) return true;
  return false;
}

bool hasDeviceExtension(VkPhysicalDevice gpu, const char* name) {
  uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, extensions.data());
  for (const auto& extension : extensions)
    if (std::strcmp(extension.extensionName, name) == 0) return true;
  return false;
}

// The features EmberEngine is built on. Kept in one place so selection and creation agree.
struct FeatureChain {
  VkPhysicalDeviceFeatures2 core{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  VkPhysicalDeviceVulkan11Features v11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  VkPhysicalDeviceVulkan12Features v12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  VkPhysicalDeviceVulkan13Features v13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceMeshShaderFeaturesEXT mesh{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};

  FeatureChain() {
    core.pNext = &v11;
    v11.pNext = &v12;
    v12.pNext = &v13;
    v13.pNext = &mesh;
  }
  FeatureChain(const FeatureChain&) = delete;
  FeatureChain& operator=(const FeatureChain&) = delete;
};

// Fills `wanted` with the required features; returns the first one `have` lacks, or null.
const char* requireFeatures(const FeatureChain& have, FeatureChain& wanted) {
#define EMBER_REQUIRE(level, field)               \
  if (!have.level.field) return #level "." #field; \
  wanted.level.field = VK_TRUE;
  EMBER_REQUIRE(core.features, samplerAnisotropy)
  EMBER_REQUIRE(core.features, textureCompressionBC)
  EMBER_REQUIRE(core.features, multiDrawIndirect)
  EMBER_REQUIRE(core.features, drawIndirectFirstInstance)
  EMBER_REQUIRE(core.features, fillModeNonSolid)
  EMBER_REQUIRE(core.features, depthClamp)
  EMBER_REQUIRE(core.features, imageCubeArray)
  EMBER_REQUIRE(core.features, independentBlend)
  EMBER_REQUIRE(core.features, shaderInt64)
  EMBER_REQUIRE(v11, shaderDrawParameters)
  EMBER_REQUIRE(v12, bufferDeviceAddress)
  EMBER_REQUIRE(v12, descriptorIndexing)
  EMBER_REQUIRE(v12, runtimeDescriptorArray)
  EMBER_REQUIRE(v12, descriptorBindingPartiallyBound)
  EMBER_REQUIRE(v12, descriptorBindingVariableDescriptorCount)
  EMBER_REQUIRE(v12, descriptorBindingSampledImageUpdateAfterBind)
  EMBER_REQUIRE(v12, descriptorBindingStorageImageUpdateAfterBind)
  EMBER_REQUIRE(v12, descriptorBindingStorageBufferUpdateAfterBind)
  EMBER_REQUIRE(v12, descriptorBindingUpdateUnusedWhilePending)
  EMBER_REQUIRE(v12, shaderSampledImageArrayNonUniformIndexing)
  EMBER_REQUIRE(v12, shaderStorageImageArrayNonUniformIndexing)
  EMBER_REQUIRE(v12, shaderStorageBufferArrayNonUniformIndexing)
  EMBER_REQUIRE(v12, timelineSemaphore)
  EMBER_REQUIRE(v12, scalarBlockLayout)
  EMBER_REQUIRE(v12, drawIndirectCount)
  EMBER_REQUIRE(v12, hostQueryReset)
  EMBER_REQUIRE(v12, samplerFilterMinmax)  // depth pyramid: one sample = the farthest of 2x2
  EMBER_REQUIRE(v13, dynamicRendering)
  EMBER_REQUIRE(v13, synchronization2)
  EMBER_REQUIRE(v13, maintenance4)
  EMBER_REQUIRE(v13, shaderDemoteToHelperInvocation)  // HLSL clip()/discard
  EMBER_REQUIRE(mesh, taskShader)                      // GPU-driven geometry (VK_EXT_mesh_shader)
  EMBER_REQUIRE(mesh, meshShader)
#undef EMBER_REQUIRE
  return nullptr;
}

}  // namespace

VulkanContext::VulkanContext(const VulkanContextDesc& desc) {
  VK_CHECK(volkInitialize());
  createInstance(desc);
  pickGpu();
  createDevice();
}

VulkanContext::~VulkanContext() {
  if (device) vkDeviceWaitIdle(device);
  if (immediateFence_) vkDestroyFence(device, immediateFence_, nullptr);
  if (immediatePool_) vkDestroyCommandPool(device, immediatePool_, nullptr);
  if (allocator) vmaDestroyAllocator(allocator);
  if (device) vkDestroyDevice(device, nullptr);
  if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
  if (messenger_) vkDestroyDebugUtilsMessengerEXT(instance, messenger_, nullptr);
  if (instance) vkDestroyInstance(instance, nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                             VkDebugUtilsMessageTypeFlagsEXT,
                                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                             void* user) {
  auto* self = static_cast<VulkanContext*>(user);
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    ++self->validationErrors_;
    logError("vulkan: {}", data->pMessage);
  } else {
    logWarn("vulkan: {}", data->pMessage);
  }
  return VK_FALSE;
}

void VulkanContext::createInstance(const VulkanContextDesc& desc) {
  std::vector<const char*> extensions;
  if (desc.window) {
    Uint32 count = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&count);
    EMBER_CHECK(sdlExtensions, SDL_GetError());
    extensions.assign(sdlExtensions, sdlExtensions + count);
  }
  const bool validation = desc.validation && hasLayer(kValidationLayer);
  if (desc.validation && !validation) logWarn("validation layer not installed; running without it");
  if (validation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "Ember";
  app.pEngineName = "EmberEngine";
  app.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  app.apiVersion = VK_API_VERSION_1_3;

  VkDebugUtilsMessengerCreateInfoEXT messengerInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  messengerInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messengerInfo.pfnUserCallback = debugCallback;
  messengerInfo.pUserData = this;

  VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  info.pApplicationInfo = &app;
  info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  info.ppEnabledExtensionNames = extensions.data();
  if (validation) {
    info.enabledLayerCount = 1;
    info.ppEnabledLayerNames = &kValidationLayer;
    info.pNext = &messengerInfo;  // also reports problems during instance creation
  }
  VK_CHECK(vkCreateInstance(&info, nullptr, &instance));
  volkLoadInstanceOnly(instance);

  if (validation) VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &messengerInfo, nullptr, &messenger_));
  if (desc.window) EMBER_CHECK(SDL_Vulkan_CreateSurface(desc.window, instance, nullptr, &surface), SDL_GetError());
}

void VulkanContext::pickGpu() {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance, &count, nullptr);
  std::vector<VkPhysicalDevice> gpus(count);
  vkEnumeratePhysicalDevices(instance, &count, gpus.data());

  uint64_t bestScore = 0;
  for (VkPhysicalDevice candidate : gpus) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(candidate, &props);
    if (props.apiVersion < VK_API_VERSION_1_3) continue;
    if (surface && !hasDeviceExtension(candidate, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
    if (!hasDeviceExtension(candidate, VK_EXT_MESH_SHADER_EXTENSION_NAME)) continue;

    FeatureChain have, wanted;
    vkGetPhysicalDeviceFeatures2(candidate, &have.core);
    if (const char* missing = requireFeatures(have, wanted)) {
      logWarn("GPU {} skipped: lacks {}", props.deviceName, missing);
      continue;
    }

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());
    int family = -1;
    for (uint32_t i = 0; i < familyCount; ++i) {
      if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) continue;
      VkBool32 present = VK_TRUE;
      if (surface) vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &present);
      if (present) { family = static_cast<int>(i); break; }
    }
    if (family < 0) continue;

    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(candidate, &memory);
    uint64_t vram = 0;
    for (uint32_t i = 0; i < memory.memoryHeapCount; ++i)
      if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) vram += memory.memoryHeaps[i].size;
    const uint64_t score = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? (1ull << 62) : 0) + vram;
    if (score > bestScore) {
      bestScore = score;
      gpu = candidate;
      gpuProperties = props;
      queueFamily = static_cast<uint32_t>(family);
    }
  }
  EMBER_CHECK(gpu, "No GPU with Vulkan 1.3 and the required features was found");
  logInfo("GPU: {} (Vulkan {}.{}.{}, driver {:#x})", gpuProperties.deviceName,
          VK_API_VERSION_MAJOR(gpuProperties.apiVersion), VK_API_VERSION_MINOR(gpuProperties.apiVersion),
          VK_API_VERSION_PATCH(gpuProperties.apiVersion), gpuProperties.driverVersion);
}

void VulkanContext::createDevice() {
  FeatureChain have, wanted;
  vkGetPhysicalDeviceFeatures2(gpu, &have.core);
  requireFeatures(have, wanted);

  // A transfer-only family is the GPU's DMA engine: copies there don't take time from rendering.
  uint32_t familyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familyCount, nullptr);
  std::vector<VkQueueFamilyProperties> families(familyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familyCount, families.data());
  transferFamily = queueFamily;
  for (uint32_t i = 0; i < familyCount; ++i) {
    const VkQueueFlags flags = families[i].queueFlags;
    if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))) {
      transferFamily = i;
      break;
    }
  }

  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queueInfos[2]{{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}, {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}};
  queueInfos[0].queueFamilyIndex = queueFamily;
  queueInfos[0].queueCount = 1;
  queueInfos[0].pQueuePriorities = &priority;
  queueInfos[1].queueFamilyIndex = transferFamily;
  queueInfos[1].queueCount = 1;
  queueInfos[1].pQueuePriorities = &priority;

  std::vector<const char*> extensions;
  if (surface) extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
  // The driver's real budget for this process (other programs' use counted), read through VMA (vmaGetHeapBudgets): the
  // sprite system decides from it whether Personagens Alta fits.
  const bool memoryBudget = hasDeviceExtension(gpu, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
  if (memoryBudget) extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
  else logWarn("VK_EXT_memory_budget missing: GPU memory budgets are estimates");

  VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  info.pNext = &wanted.core;
  info.queueCreateInfoCount = dedicatedTransfer() ? 2 : 1;
  info.pQueueCreateInfos = queueInfos;
  info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  info.ppEnabledExtensionNames = extensions.data();
  VK_CHECK(vkCreateDevice(gpu, &info, nullptr, &device));
  volkLoadDevice(device);
  vkGetDeviceQueue(device, queueFamily, 0, &queue);
  vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);
  logInfo("transfer queue: {}", dedicatedTransfer() ? std::format("dedicated (family {})", transferFamily)
                                                    : std::string("shared with graphics"));

  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | (memoryBudget ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0);
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorInfo.physicalDevice = gpu;
  allocatorInfo.device = device;
  allocatorInfo.instance = instance;
  allocatorInfo.pVulkanFunctions = &functions;
  VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator));

  VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  poolInfo.queueFamilyIndex = queueFamily;
  VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &immediatePool_));
  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &immediateFence_));
}

void VulkanContext::immediateSubmit(const std::function<void(VkCommandBuffer)>& record) {
  VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocInfo.commandPool = immediatePool_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  VkCommandBuffer cmd;
  VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd));

  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
  record(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  cmdInfo.commandBuffer = cmd;
  VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &cmdInfo;
  VK_CHECK(vkQueueSubmit2(queue, 1, &submit, immediateFence_));
  VK_CHECK(vkWaitForFences(device, 1, &immediateFence_, VK_TRUE, UINT64_MAX));
  VK_CHECK(vkResetFences(device, 1, &immediateFence_));
  vkFreeCommandBuffers(device, immediatePool_, 1, &cmd);
}

}  // namespace ember
