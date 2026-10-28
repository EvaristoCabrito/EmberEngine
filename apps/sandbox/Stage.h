#pragma once

#include <functional>
#include <string>

#include "rhi/VkCommon.h"

union SDL_Event;

namespace ember {

class Renderer;

// One sandbox test scene (sprite parade, mesh scene, ...): fed every frame by the sandbox loop.
struct Stage {
  virtual ~Stage() = default;
  // Before rendering: update streaming, set the renderer's scene.
  virtual void frame(Renderer& renderer, VkExtent2D size) = 0;
  // Window input the UI did not take (window mode only).
  virtual void event(const SDL_Event& e) { (void)e; }
  // Inside the UI frame.
  virtual void drawPanel() {}
  // After rendering, with the frame's main-thread time (excluding waits on the GPU and the window).
  virtual void endFrame(double busyMs) { (void)busyMs; }
  // Off-screen runs stop once the stage is done; passed() is the exit code's verdict.
  virtual bool done() const { return false; }
  virtual bool passed() const { return true; }
  // The music this screen plays (Ember GameApp's music chain, audio.ts): a file under Ember's game/MUSIC and its base
  // volume; an empty file is silence. The game loop hands it to AudioMixer every frame.
  struct Music {
    std::string file;
    float base = 0.0f;
  };
  virtual Music music() const { return {}; }
  // What the screen still loads, for Ember's loading curtain over it (LoadingScreens.h): the step it is on (each with
  // its own count of real units, `done` of `total`; 0 / 0 when there is nothing to count) and whether it is ready.
  struct Loading {
    int step = 0;
    uint32_t done = 0, total = 0;
    bool ready = true;
  };
  virtual Loading loading() const { return {}; }
  // Drawn over the stage's own panel in the same UI frame (a campaign dialog over a fight, say); set by its owner.
  std::function<void()> overlay;
};

}  // namespace ember
