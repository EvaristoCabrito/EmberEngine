#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace ember {

// One sprite card to draw this frame. Matches SpriteInstance in shaders/sprites.hlsl (80 bytes).
struct SpriteInstance {
  glm::vec3 origin;  // world position of the trimmed picture's bottom centre
  float width;       // world size of the trimmed picture
  float height;
  uint32_t textureIndex;
  uint32_t layer;    // frame
  uint32_t flags;    // kSprite*
  glm::vec2 uvScale;
  glm::vec2 right{0.0f};      // the card's horizontal axis (x, z); zero = face the camera
  glm::vec3 emitColor{0.0f};  // painted light (a flame): the colour its pixels are close to, linear
  float emitLevel = 0.0f;     // 0 = nothing glows; Engine2 FLAME_EMIT = 2.5 at full flicker
  glm::vec3 emitAt{0.0f};     // where the light sits on the card (texture u, v); z: texture u -> card heights
  float emitScaleV = 0.0f;    // texture v -> card heights (the glow's radius is 0.1 card heights)
};
static_assert(sizeof(SpriteInstance) == 80);

inline constexpr uint32_t kSpriteMirrored = 1;
inline constexpr uint32_t kSpriteLit = 2;    // lit by the scene (lighting.hlsli; Engine2's card model)
inline constexpr uint32_t kSpriteFront = 4;  // a unit: drawn 1.2 units nearer the camera in depth only

}  // namespace ember
