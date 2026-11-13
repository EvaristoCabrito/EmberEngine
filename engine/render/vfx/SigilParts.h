#pragma once

// The holy sigil of the original spells (Expulsar Mortos-Vivos, the Curandeiro's: vfx/TurnUndead.cpp, vfx/HealerFx.cpp), shared so
// that the newer holy effects draw the very same marks: a fine outer ring with its tick marks and a ring just inside, and a
// hexagram (the board is hexes) with the ring inside it, turning against the first. Thin lines of light on the ground, radius 1,
// drawn with the Holy material's line look (shaders/vfx.hlsl mode 4).

#include <cmath>
#include <numbers>
#include <vector>

#include <glm/glm.hpp>

#include "render/VfxDraw.h"

namespace ember::sigil {

// A flat ribbon on y 0 from a to b (xz), `half` wide each side, u along it from u0 to u1, v across.
inline void ribbon(std::vector<VfxVertex>& out, glm::vec2 a, glm::vec2 b, float half, float u0, float u1) {
  const glm::vec2 d = b - a;
  const float len = glm::length(d);
  if (len < 1e-6f) return;
  const glm::vec2 n = glm::vec2(-d.y, d.x) / len * half;
  auto vtx = [](glm::vec2 p, float u, float v) { return VfxVertex{{p.x, 0.0f, p.y}, u, {0.0f, 1.0f, 0.0f}, v}; };
  const VfxVertex a0 = vtx(a - n, u0, 0.0f), a1 = vtx(a + n, u0, 1.0f), b0 = vtx(b - n, u1, 0.0f), b1 = vtx(b + n, u1, 1.0f);
  out.insert(out.end(), {a0, a1, b0, a1, b1, b0});
}
inline void circle(std::vector<VfxVertex>& out, float radius, float half, int segments) {
  constexpr float tau = 2.0f * std::numbers::pi_v<float>;
  for (int i = 0; i < segments; ++i) {
    const float a0 = static_cast<float>(i) / segments * tau, a1 = static_cast<float>(i + 1) / segments * tau;
    ribbon(out, {std::cos(a0) * radius, std::sin(a0) * radius}, {std::cos(a1) * radius, std::sin(a1) * radius}, half, static_cast<float>(i) / segments,
           static_cast<float>(i + 1) / segments);
  }
}
// The fixed sigil (radius 1): the outer ring, its tick marks, a ring just inside.
inline const std::vector<VfxVertex>& ringGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr float tau = 2.0f * std::numbers::pi_v<float>;
    std::vector<VfxVertex> out;
    circle(out, 1.0f, 0.018f, 96);
    circle(out, 0.9f, 0.008f, 96);
    for (int i = 0; i < 36; ++i) {
      const float a = static_cast<float>(i) / 36.0f * tau, inner = (i % 3 == 0) ? 0.88f : 0.93f;
      ribbon(out, {std::cos(a) * inner, std::sin(a) * inner}, {std::cos(a) * 0.985f, std::sin(a) * 0.985f}, 0.01f, 0.0f, 1.0f);
    }
    return out;
  }();
  return g;
}
// The turning sigil (radius 1): a hexagram and the ring inside it.
inline const std::vector<VfxVertex>& starGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr float tau = 2.0f * std::numbers::pi_v<float>;
    std::vector<VfxVertex> out;
    constexpr float kStar = 0.86f;
    for (int tri = 0; tri < 2; ++tri)
      for (int i = 0; i < 3; ++i) {
        const float a0 = (static_cast<float>(i) * 2.0f + static_cast<float>(tri)) / 6.0f * tau, a1 = (static_cast<float>(i + 1) * 2.0f + static_cast<float>(tri)) / 6.0f * tau;
        ribbon(out, {std::cos(a0) * kStar, std::sin(a0) * kStar}, {std::cos(a1) * kStar, std::sin(a1) * kStar}, 0.014f, 0.0f, 1.0f);
      }
    circle(out, kStar / std::sqrt(3.0f), 0.012f, 64);
    return out;
  }();
  return g;
}

}  // namespace ember::sigil
