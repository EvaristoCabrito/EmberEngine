#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace ember {

// One effect quad to draw this frame (spell art: flipbook frames of the fx clips). Matches EffectInstance in
// shaders/effects.hlsl (96 bytes). The quad spans center +- axisX +- axisY in the world, so it may face the camera,
// lie on the ground or stretch along a bolt; the CPU decides. Drawn after every surface, depth-tested, never writing
// depth, blended premultiplied: `opacity` 1 covers what is behind by the art's alpha, 0 only adds light (a glow).
struct EffectInstance {
  glm::vec3 center;
  float softness;       // world distance over which it fades into the surface behind it (0 = a hard edge)
  glm::vec3 axisX;      // half the quad along the art's u (left to right)
  uint32_t textureIndex;
  glm::vec3 axisY;      // half the quad along the art's v (bottom to top)
  uint32_t layer;       // flipbook frame
  glm::vec3 color;      // linear tint x strength, display-relative (1 = the art as painted at the scene's white)
  float opacity;
  glm::vec2 uvScale;    // the trimmed picture's share of the texture
  uint32_t nextLayer;   // the frame it is turning into
  float blend;          // 0 = layer .. 1 = nextLayer
  glm::vec3 sightAt;    // where the fog of war is read (never seen: not drawn; remembered: dimmed)
  uint32_t flags;       // kEffect*
};
static_assert(sizeof(EffectInstance) == 96);

inline constexpr uint32_t kEffectMirrored = 1;
inline constexpr uint32_t kEffectLit = 2;  // solid art (an arrow): lit by the scene like a card, colour = albedo tint

}  // namespace ember
