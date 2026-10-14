#pragma once

#include <cstdint>

#include "core/Function.h"

struct SDL_Window;
union SDL_Event;

namespace ember {

struct WindowDesc {
  const char* title = "Ember";
  uint32_t width = 1600;
  uint32_t height = 900;
};

// The game window (SDL3). Owns SDL's video subsystem for its lifetime.
class Window {
 public:
  explicit Window(const WindowDesc& desc);
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  // Handles OS events; false once the player closes the window.
  bool pumpEvents();
  // Size of the drawable surface in pixels (0 while minimised).
  void pixelSize(uint32_t& width, uint32_t& height) const;
  // True once after the window changed size.
  bool consumeResized();

  // Sees every event pumpEvents() handles (UI input, debug keys).
  void setEventHook(InlineFunction<void(const SDL_Event&)> hook) { hook_ = std::move(hook); }

  SDL_Window* handle() const { return window_; }

 private:
  SDL_Window* window_ = nullptr;
  bool resized_ = false;
  InlineFunction<void(const SDL_Event&)> hook_;
};

}  // namespace ember
