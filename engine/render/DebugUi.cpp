#include "render/DebugUi.h"
#include "render/UiArt.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

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

bool gGrabRequested = false;

// Engine2 shows both cursor images at 26.25 CSS px (their SVG wrappers), times the screen's scale in the browser.
constexpr float kCursorSize = 26.25f;

// An Engine2 cursor image at `size` pixels (RGBA, straight alpha); nullptr if the file is missing.
SDL_Surface* cursorImage(const char* file, int size) {
  int w = 0, h = 0, n = 0;
  stbi_uc* pixels = stbi_load(file, &w, &h, &n, 4);
  if (!pixels) {
    logError("Cursor image not found: {}", file);
    return nullptr;
  }
  std::vector<float> image(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
    const float a = pixels[i * 4 + 3] / 255.0f;
    for (int c = 0; c < 3; ++c) image[i * 4 + c] = pixels[i * 4 + c] / 255.0f * a;
    image[i * 4 + 3] = a;
  }
  stbi_image_free(pixels);
  const std::vector<float> wide = uiResampleAxis(image, w, h, size, true);
  const std::vector<float> square = uiResampleAxis(wide, size, h, size, false);
  SDL_Surface* surface = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32);
  if (!surface) return nullptr;
  for (int y = 0; y < size; ++y) {
    auto* row = static_cast<uint8_t*>(surface->pixels) + static_cast<size_t>(y) * surface->pitch;
    for (int x = 0; x < size; ++x) {
      const float* p = &square[(static_cast<size_t>(y) * size + x) * 4];
      const float a = std::clamp(p[3], 0.0f, 1.0f);
      for (int c = 0; c < 3; ++c) row[x * 4 + c] = static_cast<uint8_t>(std::lround(std::clamp(a > 0.0f ? p[c] / a : 0.0f, 0.0f, 1.0f) * 255.0f));
      row[x * 4 + 3] = static_cast<uint8_t>(std::lround(a * 255.0f));
    }
  }
  return surface;
}

// The cursors: Engine2's file, its hot spot in its 26.25 px box (styles.css).
struct CursorSpec {
  const char* name;
  const char* file;
  float hot;
};
constexpr CursorSpec kPointer{"pointer", "content/ui/cursors/pointer.png", 0.0f};
constexpr CursorSpec kGrab{"grab", "content/ui/cursors/grab.png", 13.0f};

int cursorSize(float scale) { return std::max(8, static_cast<int>(std::lround(kCursorSize * scale))); }

// `spec` for a screen at `scale`; nullptr (logged) when it cannot be made.
SDL_Cursor* makeCursor(const CursorSpec& spec, float scale) {
  const int size = cursorSize(scale);
  SDL_Surface* surface = cursorImage(spec.file, size);
  if (!surface) return nullptr;
  const int hot = std::clamp(static_cast<int>(spec.hot / kCursorSize * static_cast<float>(size)), 0, size - 1);
  SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hot, hot);
  SDL_DestroySurface(surface);
  if (!cursor) logError("Cursor {} not made: {}", spec.file, SDL_GetError());
  return cursor;
}

}  // namespace

void requestGrabCursor() { gGrabRequested = true; }

int DebugUi::checkCursors(const char* outDir) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    logError("Cursor check: SDL video failed: {}", SDL_GetError());
    return 1;
  }
  std::error_code error;
  std::filesystem::create_directories(outDir, error);
  int failures = 0;
  for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
    for (const CursorSpec* spec : {&kPointer, &kGrab}) {
      SDL_Cursor* cursor = makeCursor(*spec, scale);
      SDL_Surface* image = cursorImage(spec->file, cursorSize(scale));
      const std::string out = std::format("{}/{}-{:.2f}.png", outDir, spec->name, scale);
      const bool written = image && stbi_write_png(out.c_str(), image->w, image->h, 4, image->pixels, image->pitch) != 0;
      logInfo("Cursor {} at scale {:.2f}: {} px, cursor {}, image {}", spec->name, scale, cursorSize(scale), cursor ? "made" : "FAILED",
          written ? out : std::string("FAILED"));
      failures += (cursor && written) ? 0 : 1;
      if (image) SDL_DestroySurface(image);
      if (cursor) SDL_DestroyCursor(cursor);
    }
  }
  SDL_Quit();
  logInfo("Cursor check: {}", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}

DebugUi::DebugUi(VulkanContext& context, SDL_Window* window, VkFormat targetFormat)
    : window_(window), hasWindow_(window != nullptr), targetFormat_(targetFormat) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // layouts are saved deliberately by the editor, not on every exit
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // no keyboard navigation: the arrows and WASD always move the camera
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // the cursor is Engine2's own (applyCursor), not Windows' arrow
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
  if (cursors_.shown) SDL_SetCursor(SDL_GetDefaultCursor());
  for (SDL_Cursor* cursor : {cursors_.pointer, cursors_.grab, cursors_.text})
    if (cursor) SDL_DestroyCursor(cursor);
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

// Engine2's rule (ember-ui/styles.css): its arrow everywhere, buttons and disabled ones included, the gauntlet while
// dragging; the text bar over a text field (the browser's own, which Engine2 left alone).
void DebugUi::applyCursor() {
  const bool grab = gGrabRequested;
  gGrabRequested = false;
  if (!hasWindow_) return;
  const float scale = window_ ? std::max(SDL_GetWindowDisplayScale(window_), 0.5f) : 1.0f;
  if (scale != cursors_.scale) {
    SDL_Cursor* old[2] = {cursors_.pointer, cursors_.grab};
    Cursors made;
    made.scale = scale;
    made.pointer = makeCursor(kPointer, scale);
    made.grab = makeCursor(kGrab, scale);
    made.text = cursors_.text ? cursors_.text : SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    SDL_SetCursor(SDL_GetDefaultCursor());  // nothing shown is destroyed below
    for (SDL_Cursor* cursor : old)
      if (cursor) SDL_DestroyCursor(cursor);
    cursors_ = made;
  }
  SDL_Cursor* want = ImGui::GetMouseCursor() == ImGuiMouseCursor_TextInput ? cursors_.text : grab ? cursors_.grab : cursors_.pointer;
  if (!want) want = SDL_GetDefaultCursor();
  if (want != cursors_.shown) SDL_SetCursor(want), cursors_.shown = want;
}

void DebugUi::record(VkCommandBuffer cmd, VkImageView target, VkExtent2D size) {
  if (!frameOpen_) return;
  frameOpen_ = false;
  applyCursor();
  ImGui::Render();
  ImDrawData* drawData = ImGui::GetDrawData();
  refineUiImages(drawData);  // UI images at their size on screen (render/UiArt.h)

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
