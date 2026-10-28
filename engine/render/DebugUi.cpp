#include "render/DebugUi.h"
#include "render/UiArt.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
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

// Ember shows both cursor images at 26.25 CSS px (their SVG wrappers), times the screen's scale in the browser.
constexpr float kCursorSize = 26.25f;

// An Ember cursor image at `size` pixels (RGBA, straight alpha); nullptr if the file is missing.
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

// The cursors: Ember's file, its hot spot in its 26.25 px box (styles.css).
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
    : context_(context), window_(window), hasWindow_(window != nullptr), targetFormat_(targetFormat) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // layouts are saved deliberately by the editor, not on every exit
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // no keyboard navigation: the arrows and WASD always move the camera
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // the cursor is Ember's own (applyCursor), not Windows' arrow
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
  loadUiFonts();  // Ember's own fonts for the menus and HUD (render/UiArt.h)
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
  // Backend-owned pool: one set per UI image (font atlas, Ember's art: portraits, every weapon and gear icon the shops
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
  destroyAllTextures();  // ours, before the backend's shutdown looks at them
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

// Ember's rule (ember-ui/styles.css): its arrow everywhere, buttons and disabled ones included, the gauntlet while
// dragging; the text bar over a text field (the browser's own, which Ember left alone).
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
  using Clock = std::chrono::steady_clock;
  const auto ms = [](Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
  const Clock::time_point t0 = Clock::now();
  ImGui::Render();
  ImDrawData* drawData = ImGui::GetDrawData();
  const Clock::time_point t1 = Clock::now();
  refineUiImages(drawData);  // UI images at their size on screen (render/UiArt.h)
  const Clock::time_point t2 = Clock::now();
  int texUpdates = 0, texW = 0, texH = 0;
  if (drawData && drawData->Textures)
    for (ImTextureData* t : *drawData->Textures)
      if (t->Status != ImTextureStatus_OK) ++texUpdates, texW = std::max(texW, t->Width), texH = std::max(texH, t->Height);
  uploadTextures(cmd, drawData);

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
  // Slow UI frames (the hunt for the rare 140-220 ms hitch, 2026-10-19): ImGui's own render, the image resampling, the
  // backend (texture uploads: how many textures changed, the largest).
  const Clock::time_point t3 = Clock::now();
  if (ms(t0, t3) > 20.0)
    logWarn("slow ui record: {:.1f} ms = imgui render {:.1f}, refine images {:.1f}, textures + draw {:.1f} ({} textures changed, largest {}x{})",
            ms(t0, t3), ms(t0, t1), ms(t1, t2), ms(t2, t3), texUpdates, texW, texH);
}

void DebugUi::uploadTextures(VkCommandBuffer cmd, ImDrawData* drawData) {
  ++records_;
  // What the GPU is surely done with: kFramesInFlight frames recorded since.
  std::erase_if(retired_, [&](Retired& r) {
    if (r.record + kFramesInFlight + 1 > records_) return false;
    if (r.staging.buffer) destroyBuffer(context_, r.staging);
    if (r.texture) {
      destroyImage(context_, r.texture->image);
      delete r.texture;
    }
    return true;
  });
  if (!drawData || !drawData->Textures) return;
  for (ImTextureData* tex : *drawData->Textures) {
    if (tex->Status == ImTextureStatus_OK || tex->Status == ImTextureStatus_Destroyed) continue;
    if (tex->Status == ImTextureStatus_WantDestroy) {
      // Unused for as many frames as can be in flight (the backend's own rule): its descriptor and image can go.
      if (tex->UnusedFrames < static_cast<int>(kFramesInFlight)) continue;
      auto* t = static_cast<UiTexture*>(tex->BackendUserData);
      if (t && textures_.erase(t)) {
        ImGui_ImplVulkan_RemoveTexture(t->set);
        retired_.push_back({{}, t, records_});
      }
      tex->SetTexID(ImTextureID_Invalid);
      tex->BackendUserData = nullptr;
      tex->SetStatus(ImTextureStatus_Destroyed);
      continue;
    }
    const bool create = tex->Status == ImTextureStatus_WantCreate;
    if (create) {
      auto* t = new UiTexture();
      ImageDesc desc;
      desc.format = VK_FORMAT_R8G8B8A8_UNORM;
      desc.extent = {static_cast<uint32_t>(tex->Width), static_cast<uint32_t>(tex->Height)};
      desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      t->image = createImage(context_, desc);
      t->set = ImGui_ImplVulkan_AddTexture(t->image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      textures_.insert(t);
      tex->SetTexID(reinterpret_cast<ImTextureID>(t->set));
      tex->BackendUserData = t;
    }
    auto* t = static_cast<UiTexture*>(tex->BackendUserData);
    if (!t || !textures_.contains(t)) continue;  // not ours (never: every texture is made here)
    const int x = create ? 0 : tex->UpdateRect.x, y = create ? 0 : tex->UpdateRect.y;
    const int w = create ? tex->Width : tex->UpdateRect.w, h = create ? tex->Height : tex->UpdateRect.h;
    const VkDeviceSize pitch = static_cast<VkDeviceSize>(w) * tex->BytesPerPixel;
    Buffer staging = createHostBuffer(context_, std::max<VkDeviceSize>(pitch * h, 16), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    for (int row = 0; row < h; ++row)
      std::memcpy(static_cast<std::byte*>(staging.mapped) + pitch * row, tex->GetPixelsAt(x, y + row), static_cast<size_t>(pitch));
    VK_CHECK(vmaFlushAllocation(context_.allocator, staging.allocation, 0, VK_WHOLE_SIZE));
    transition(cmd, t->image.image, VK_IMAGE_ASPECT_COLOR_BIT,
               {create ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT});
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {x, y, 0};
    region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    vkCmdCopyBufferToImage(cmd, staging.buffer, t->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transition(cmd, t->image.image, VK_IMAGE_ASPECT_COLOR_BIT,
               {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT});
    retired_.push_back({staging, nullptr, records_});
    tex->SetStatus(ImTextureStatus_OK);
  }
}

void DebugUi::destroyAllTextures() {
  vkDeviceWaitIdle(context_.device);
  for (Retired& r : retired_) {
    if (r.staging.buffer) destroyBuffer(context_, r.staging);
    if (r.texture) {
      destroyImage(context_, r.texture->image);
      delete r.texture;
    }
  }
  retired_.clear();
  for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
    auto* t = static_cast<UiTexture*>(tex->BackendUserData);
    if (!t || !textures_.contains(t)) continue;
    ImGui_ImplVulkan_RemoveTexture(t->set);
    destroyImage(context_, t->image);
    textures_.erase(t);
    delete t;
    tex->SetTexID(ImTextureID_Invalid);
    tex->BackendUserData = nullptr;
    tex->SetStatus(ImTextureStatus_Destroyed);
  }
  for (UiTexture* t : textures_) {  // user textures no longer listed
    ImGui_ImplVulkan_RemoveTexture(t->set);
    destroyImage(context_, t->image);
    delete t;
  }
  textures_.clear();
}

}  // namespace ember
