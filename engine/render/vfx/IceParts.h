#pragma once

// The ice effects' shared parts (Tempestade de Gelo 02, Frost 02): the unit billboard, the hex, the frost-ring disc, the ice
// crystal mesh and the helpers that stand it along an axis. Drawn with the IceStorm material (shaders/vfx.hlsl), world space.

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <glm/glm.hpp>

#include "render/VfxDraw.h"

namespace ember::icefx {

inline float ease(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
inline float hash01(uint32_t i, uint32_t salt) {
  uint32_t h = (i * 0x9e3779b9u) ^ (salt * 0x85ebca6bu) ^ 0x5bd1e995u;
  h ^= h >> 16, h *= 0x7feb352du, h ^= h >> 15, h *= 0x846ca68bu, h ^= h >> 16;
  return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

inline VfxVertex vertex(glm::vec3 p, glm::vec3 n, float u, float v) { return {p, u, n, v}; }

// A unit quad (-0.5..0.5, uv 0..1, v up): every billboard.
inline const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      vertex({-0.5f, -0.5f, 0}, {0, 0, 1}, 0, 0), vertex({0.5f, -0.5f, 0}, {0, 0, 1}, 1, 0), vertex({0.5f, 0.5f, 0}, {0, 0, 1}, 1, 1),
      vertex({-0.5f, -0.5f, 0}, {0, 0, 1}, 0, 0), vertex({0.5f, 0.5f, 0}, {0, 0, 1}, 1, 1), vertex({-0.5f, 0.5f, 0}, {0, 0, 1}, 0, 1)};
  return g;
}
// A hex of radius 1 lying flat (pointy toward +z, as the board's: world/Hex.h corner), facing up.
inline const std::vector<VfxVertex>& hexGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr float pi = std::numbers::pi_v<float>;
    std::vector<VfxVertex> out;
    for (int k = 0; k < 6; ++k) {
      const float a = static_cast<float>(k) * pi / 3.0f, b = static_cast<float>(k + 1) * pi / 3.0f;
      out.push_back(vertex({0, 0, 0}, {0, 1, 0}, 0.5f, 0.5f));
      out.push_back(vertex({std::sin(b), 0, std::cos(b)}, {0, 1, 0}, 0, 0));
      out.push_back(vertex({std::sin(a), 0, std::cos(a)}, {0, 1, 0}, 0, 0));
    }
    return out;
  }();
  return g;
}
// A flat disc of radius 1 (u around, v out), facing up: the frost rings.
inline const std::vector<VfxVertex>& discGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 32, out = 6;
    constexpr float tau = 2.0f * std::numbers::pi_v<float>;
    auto v = [](int i, int j) {
      const float a = static_cast<float>(i) / around * tau, r = static_cast<float>(j) / out;
      return vertex({std::cos(a) * r, 0.0f, std::sin(a) * r}, {0, 1, 0}, static_cast<float>(i) / around, r);
    };
    std::vector<VfxVertex> tris;
    for (int i = 0; i < around; ++i)
      for (int j = 0; j < out; ++j) tris.insert(tris.end(), {v(i, j), v(i, j + 1), v(i + 1, j), v(i + 1, j), v(i, j + 1), v(i + 1, j + 1)});
    return tris;
  }();
  return g;
}
// An ice crystal along +y: a six-sided prism with a short point above and a long one below (it falls tip first),
// radius 1, 1 long, flat faces.
inline const std::vector<VfxVertex>& crystalGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr float pi = std::numbers::pi_v<float>;
    const glm::vec3 top{0, 0.5f, 0}, bottom{0, -0.5f, 0};
    std::vector<VfxVertex> out;
    auto tri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
      glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
      if (glm::dot(n, (a + b + c) / 3.0f) < 0.0f) std::swap(b, c), n = -n;
      out.insert(out.end(), {vertex(a, n, 0, 0), vertex(b, n, 1, 0), vertex(c, n, 0, 1)});
    };
    for (int k = 0; k < 6; ++k) {
      const float a = static_cast<float>(k) * pi / 3.0f, b = static_cast<float>(k + 1) * pi / 3.0f;
      const glm::vec3 ua{std::cos(a), 0.25f, std::sin(a)}, ub{std::cos(b), 0.25f, std::sin(b)};
      const glm::vec3 la{std::cos(a) * 0.9f, -0.05f, std::sin(a) * 0.9f}, lb{std::cos(b) * 0.9f, -0.05f, std::sin(b) * 0.9f};
      tri(top, ua, ub);
      tri(ua, la, lb), tri(ua, lb, ub);
      tri(la, bottom, lb);
    }
    return out;
  }();
  return g;
}

// A frame whose +y runs along `axis` (unit), turned `spin` about it.
inline glm::mat4 alongAxis(glm::vec3 axis, float spin) {
  const glm::vec3 helper = std::abs(axis.y) < 0.95f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
  const glm::vec3 x0 = glm::normalize(glm::cross(helper, axis)), z0 = glm::cross(x0, axis);
  const glm::vec3 x = x0 * std::cos(spin) + z0 * std::sin(spin), z = glm::cross(x, axis);
  glm::mat4 m(1.0f);
  m[0] = glm::vec4(x, 0), m[1] = glm::vec4(axis, 0), m[2] = glm::vec4(z, 0);
  return m;
}

}  // namespace ember::icefx
