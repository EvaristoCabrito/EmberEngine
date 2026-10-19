#pragma once

#include <cstdint>
#include <set>
#include <vector>

#include "rhi/Image.h"
#include "rhi/VkCommon.h"

struct ImDrawData;

struct SDL_Window;
struct SDL_Cursor;
union SDL_Event;

namespace ember {

class VulkanContext;

// Engine2's gauntlet instead of the arrow for this frame (the board or the world map being dragged). Call it every
// frame the drag lasts; the arrow comes back by itself on the first frame nobody asks.
void requestGrabCursor();

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

  // `Sandbox --cursor-check [dir]`: makes both Engine2 cursors at screen scales 1 / 1.25 / 1.5 / 2 without a window and
  // saves the images there. 0 = every one made.
  static int checkCursors(const char* outDir);

 private:
  void applyCursor();
  // ImGui's textures (font atlas, every UI image) made and updated by us inside the frame's command buffer, before the
  // UI draw: ImGui's Vulkan backend did each with its own submit and a vkQueueWaitIdle (20-50 ms a texture when one
  // appeared mid-battle, the hunt for the hitch 2026-10-19). Staging buffers and dropped images live until the GPU is
  // surely past the frame that used them.
  void uploadTextures(VkCommandBuffer cmd, ImDrawData* drawData);
  void destroyAllTextures();
  struct UiTexture {
    Image image;
    VkDescriptorSet set = VK_NULL_HANDLE;
  };
  struct Retired {
    Buffer staging;
    UiTexture* texture = nullptr;
    uint64_t record = 0;
  };
  VulkanContext& context_;
  std::set<UiTexture*> textures_;
  std::vector<Retired> retired_;
  uint64_t records_ = 0;

  // Engine2's cursors (content/ui/cursors, tools/scripts/engine2/export_cursors.py), made for the window's display
  // scale; remade when the window moves to a screen with another scale.
  struct Cursors {
    float scale = 0.0f;
    SDL_Cursor* pointer = nullptr;
    SDL_Cursor* grab = nullptr;
    SDL_Cursor* text = nullptr;
    SDL_Cursor* shown = nullptr;
  };
  SDL_Window* window_ = nullptr;
  Cursors cursors_;
  bool hasWindow_ = false;
  float zoom_ = 1.0f;
  bool frameOpen_ = false;
  VkFormat targetFormat_;
};

}  // namespace ember
