#include "platform/Window.h"

#include <SDL3/SDL.h>

#include "core/Log.h"

namespace ember {

Window::Window(const WindowDesc& desc) {
  EMBER_CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD), SDL_GetError());
  window_ = SDL_CreateWindow(desc.title, static_cast<int>(desc.width), static_cast<int>(desc.height),
                             SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  EMBER_CHECK(window_, SDL_GetError());
}

Window::~Window() {
  if (window_) SDL_DestroyWindow(window_);
  SDL_Quit();
}

bool Window::pumpEvents() {
  SDL_Event event;
  bool running = true;
  while (SDL_PollEvent(&event)) {
    if (hook_) hook_(event);
    switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        running = false;
        break;
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        resized_ = true;
        break;
      default:
        break;
    }
  }
  return running;
}

void Window::pixelSize(uint32_t& width, uint32_t& height) const {
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(window_, &w, &h);
  width = static_cast<uint32_t>(w);
  height = static_cast<uint32_t>(h);
}

bool Window::consumeResized() {
  const bool was = resized_;
  resized_ = false;
  return was;
}

}  // namespace ember
