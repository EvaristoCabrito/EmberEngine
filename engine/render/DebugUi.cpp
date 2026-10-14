#include "render/DebugUi.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include "rhi/VulkanContext.h"

namespace ember {

namespace {

void checkVk(VkResult result) {
  if (result != VK_SUCCESS) fatal(std::format("ImGui Vulkan backend: {}", string_VkResult(result)));
}

}  // namespace

DebugUi::DebugUi(VulkanContext& context, SDL_Window* window, VkFormat targetFormat)
    : hasWindow_(window != nullptr), targetFormat_(targetFormat) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // layouts are saved deliberately by the editor, not on every exit
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  ImGui::StyleColorsDark();

  if (window) EMBER_CHECK(ImGui_ImplSDL3_InitForVulkan(window), "ImGui SDL3 backend failed to start");

  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion = VK_API_VERSION_1_3;
  info.Instance = context.instance;
  info.PhysicalDevice = context.gpu;
  info.Device = context.device;
  info.QueueFamily = context.queueFamily;
  info.Queue = context.queue;
  info.DescriptorPoolSize = 64;  // backend-owned pool: font atlas + debug images
  info.MinImageCount = kFramesInFlight;
  info.ImageCount = kFramesInFlight;
  info.UseDynamicRendering = true;
  info.PipelineInfoMain.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &targetFormat_;
  info.CheckVkResultFn = checkVk;
  EMBER_CHECK(ImGui_ImplVulkan_Init(&info), "ImGui Vulkan backend failed to start");
}

DebugUi::~DebugUi() {
  ImGui_ImplVulkan_Shutdown();
  if (hasWindow_) ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

bool DebugUi::processEvent(const SDL_Event& event) {
  ImGui_ImplSDL3_ProcessEvent(&event);
  const ImGuiIO& io = ImGui::GetIO();
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
      return io.WantCaptureKeyboard;
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
      return io.WantCaptureMouse;
    default:
      return false;
  }
}

void DebugUi::newFrame(VkExtent2D size, float deltaSeconds) {
  if (frameOpen_) ImGui::EndFrame();  // last frame was never drawn (e.g. the swapchain was out of date)
  ImGui_ImplVulkan_NewFrame();
  if (hasWindow_) {
    ImGui_ImplSDL3_NewFrame();
  } else {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {static_cast<float>(size.width), static_cast<float>(size.height)};
    io.DeltaTime = deltaSeconds > 0.0f ? deltaSeconds : 1.0f / 60.0f;
  }
  ImGui::NewFrame();
  frameOpen_ = true;
}

void DebugUi::record(VkCommandBuffer cmd, VkImageView target, VkExtent2D size) {
  if (!frameOpen_) return;
  frameOpen_ = false;
  ImGui::Render();
  ImDrawData* drawData = ImGui::GetDrawData();

  VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  color.imageView = target;
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
  rendering.renderArea = {{0, 0}, size};
  rendering.layerCount = 1;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachments = &color;
  vkCmdBeginRendering(cmd, &rendering);
  ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
  vkCmdEndRendering(cmd);
}

}  // namespace ember
