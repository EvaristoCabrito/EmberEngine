#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace ember {

// One sprite card to draw this frame. Matches SpriteInstance in shaders/sprites.hlsl (48 bytes).
struct SpriteInstance {
  glm::vec3 origin;  // world position of the picture's bottom centre
  float width;       // world size of the trimmed picture
  float height;
  uint32_t textureIndex;
  uint32_t layer;    // frame
  uint32_t flags;    // kSpriteMirrored
  glm::vec2 uvScale;
  glm::vec2 pad{0.0f};
};
static_assert(sizeof(SpriteInstance) == 48);

inline constexpr uint32_t kSpriteMirrored = 1;

}  // namespace ember
