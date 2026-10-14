#pragma once

#include "rhi/VkCommon.h"

namespace ember {

class Renderer;

// One sandbox test scene (sprite parade, mesh scene, ...): fed every frame by the sandbox loop.
struct Stage {
  virtual ~Stage() = default;
  // Before rendering: update streaming, set the renderer's scene.
  virtual void frame(Renderer& renderer, VkExtent2D size) = 0;
  // Inside the UI frame.
  virtual void drawPanel() {}
  // After rendering, with the frame's main-thread time (excluding waits on the GPU and the window).
  virtual void endFrame(double busyMs) { (void)busyMs; }
  // Off-screen runs stop once the stage is done; passed() is the exit code's verdict.
  virtual bool done() const { return false; }
  virtual bool passed() const { return true; }
};

}  // namespace ember
