#pragma once

#include "rhi/VkCommon.h"

struct SDL_Window;
union SDL_Event;

namespace ember {

class VulkanContext;

// Dear ImGui on the engine's renderer: debug overlays now, the editor later. Between newFrame() and
// the renderer's next frame, any code may issue ImGui calls; the renderer then draws them on top of
// the finished image. Works off-screen too (no window: no input, size set per frame).
class DebugUi {
 public:
  DebugUi(VulkanContext& context, SDL_Window* window, VkFormat targetFormat);
  ~DebugUi();
  DebugUi(const DebugUi&) = delete;
  DebugUi& operator=(const DebugUi&) = delete;

  // Feeds window input to the UI. Returns true when the UI wants the event for itself.
  bool processEvent(const SDL_Event& event);

  void newFrame(VkExtent2D size, float deltaSeconds);

  // Renderer: draws this frame's UI into `target` (in COLOR_ATTACHMENT_OPTIMAL). Does nothing if
  // newFrame() was not called since the last record.
  void record(VkCommandBuffer cmd, VkImageView target, VkExtent2D size);

  // Every screen is laid out on a canvas kUiHeight tall; the UI is drawn zoomed to the real height (user,
  // 2026-10-15: menus too small to read), so text, buttons and plates grow together with the window.
  static constexpr float kUiHeight = 720.0f;

 private:
  bool hasWindow_ = false;
  float zoom_ = 1.0f;
  bool frameOpen_ = false;
  VkFormat targetFormat_;
};

}  // namespace ember
