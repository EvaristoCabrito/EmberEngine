#include "render/DebugUi.h"
#include "render/UiArt.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <filesystem>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "core/Log.h"

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
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // no keyboard navigation: the arrows and WASD always move the camera
  ImGui::StyleColorsDark();
  // A readable font (user, 2026-10-15: ImGui's pixel font refused): Windows' own Segoe UI, read from the system's
  // fonts folder at run time (nothing bundled); ImGui's built-in font if it is missing.
  // The fonts folder from Windows itself: the WINDIR variable is missing when the window is started from some
  // shells, which left the pixel font on screen (2026-10-15).
  char windows[MAX_PATH] = {};
  const UINT length = GetWindowsDirectoryA(windows, MAX_PATH);
  const std::string segoe = (length > 0 && length < MAX_PATH ? std::string(windows) : std::string("C:\\Windows")) + "\\Fonts\\segoeui.ttf";
  std::error_code error;
  if (std::filesystem::exists(segoe, error)) io.FontDefault = io.Fonts->AddFontFromFileTTF(segoe.c_str(), 18.0f);
  else logError("UI font not found: {}", segoe);
  loadUiFonts();  // Engine2's own fonts for the menus and HUD (render/UiArt.h)
  ImGuiStyle& style = ImGui::GetStyle();
  style.FramePadding = {10.0f, 6.0f};
  style.ItemSpacing = {10.0f, 8.0f};
  style.WindowPadding = {14.0f, 12.0f};
  style.FrameRounding = 4.0f;
  style.WindowRounding = 6.0f;

  if (window) EMBER_CHECK(ImGui_ImplSDL3_InitForVulkan(window), "ImGui SDL3 backend failed to start");

  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion = VK_API_VERSION_1_3;
  info.Instance = context.instance;
  info.PhysicalDevice = context.gpu;
  info.Device = context.device;
  info.QueueFamily = context.queueFamily;
  info.Queue = context.queue;
  // Backend-owned pool: one set per UI image (font atlas, Engine2's art: portraits, every weapon and gear icon the shops
  // and the inventory show).
  info.DescriptorPoolSize = 4096;
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
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    // The pointer in canvas units (newFrame's zoom).
    SDL_Event scaled = event;
    scaled.motion.x /= zoom_, scaled.motion.y /= zoom_;
    ImGui_ImplSDL3_ProcessEvent(&scaled);
  } else {
    ImGui_ImplSDL3_ProcessEvent(&event);
  }
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
  ImGuiIO& io = ImGui::GetIO();
  if (hasWindow_) {
    ImGui_ImplSDL3_NewFrame();
  } else {
    io.DisplaySize = {static_cast<float>(size.width), static_cast<float>(size.height)};
    io.DisplayFramebufferScale = {1.0f, 1.0f};
    io.DeltaTime = deltaSeconds > 0.0f ? deltaSeconds : 1.0f / 60.0f;
  }
  // The canvas: kUiHeight tall, drawn at the real pixel height (fonts are rasterised at that density, so they stay sharp).
  const float pixelHeight = io.DisplaySize.y * io.DisplayFramebufferScale.y;
  const float zoom = pixelHeight > 0.0f ? pixelHeight / kUiHeight : 1.0f;
  const float logical = zoom / io.DisplayFramebufferScale.y;  // window units per canvas unit
  if (io.DisplaySize.y > 0.0f) {
    io.DisplaySize = {io.DisplaySize.x / logical, io.DisplaySize.y / logical};
    io.DisplayFramebufferScale = {zoom, zoom};
  }
  zoom_ = logical > 0.0f ? logical : 1.0f;
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
