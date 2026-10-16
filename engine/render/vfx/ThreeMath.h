#pragma once

// The bits of three.js math Engine2's effects lean on, with three's exact behaviour (r16x src/math, src/extras/curves):
// smoothstep, the centripetal Catmull-Rom curve, setFromUnitVectors, Euler XYZ, colour hex (sRGB -> linear, as three's
// ColorManagement does for hex colours), Engine2's LCG random.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ember::three {

// THREE.MathUtils.smoothstep(x, min, max)
inline float smoothstep(float x, float min, float max) {
  if (x <= min) return 0.0f;
  if (x >= max) return 1.0f;
  x = (x - min) / (max - min);
  return x * x * (3.0f - 2.0f * x);
}
inline float clamp(float x, float lo, float hi) { return std::max(lo, std::min(hi, x)); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Vector3.normalize(): a zero vector stays zero (divides by length || 1).
inline glm::vec3 normalize(glm::vec3 v) {
  const float l = glm::length(v);
  return l > 0.0f ? v / l : v;
}

// CatmullRomCurve3 (curveType 'centripetal', open), getPoint(t) and getPoints(divisions).
struct CatmullRom {
  std::vector<glm::vec3> points;

  glm::vec3 getPoint(float t) const {
    const int l = static_cast<int>(points.size());
    const float p = (l - 1) * t;
    int intPoint = static_cast<int>(std::floor(p));
    float weight = p - intPoint;
    if (weight == 0.0f && intPoint == l - 1) {
      intPoint = l - 2;
      weight = 1.0f;
    }
    const glm::vec3 p0 = intPoint > 0 ? points[intPoint - 1] : (points[0] - points[1]) + points[0];
    const glm::vec3 p1 = points[intPoint % l], p2 = points[(intPoint + 1) % l];
    const glm::vec3 p3 = intPoint + 2 < l ? points[intPoint + 2] : (points[l - 1] - points[l - 2]) + points[l - 1];
    auto d2 = [](glm::vec3 a, glm::vec3 b) { const glm::vec3 d = a - b; return glm::dot(d, d); };
    float dt0 = std::pow(d2(p0, p1), 0.25f), dt1 = std::pow(d2(p1, p2), 0.25f), dt2 = std::pow(d2(p2, p3), 0.25f);
    if (dt1 < 1e-4f) dt1 = 1.0f;
    if (dt0 < 1e-4f) dt0 = dt1;
    if (dt2 < 1e-4f) dt2 = dt1;
    glm::vec3 out;
    for (int k = 0; k < 3; ++k) {
      const float x0 = p0[k], x1 = p1[k], x2 = p2[k], x3 = p3[k];
      float t1 = (x1 - x0) / dt0 - (x2 - x0) / (dt0 + dt1) + (x2 - x1) / dt1;
      float t2 = (x2 - x1) / dt1 - (x3 - x1) / (dt1 + dt2) + (x3 - x2) / dt2;
      t1 *= dt1;
      t2 *= dt1;
      const float c0 = x1, c1 = t1, c2 = -3 * x1 + 3 * x2 - 2 * t1 - t2, c3 = 2 * x1 - 2 * x2 + t1 + t2;
      out[k] = c0 + c1 * weight + c2 * weight * weight + c3 * weight * weight * weight;
    }
    return out;
  }
  std::vector<glm::vec3> getPoints(int divisions) const {
    std::vector<glm::vec3> out;
    for (int d = 0; d <= divisions; ++d) out.push_back(getPoint(static_cast<float>(d) / divisions));
    return out;
  }
};

// Quaternion.setFromUnitVectors(from, to)
inline glm::quat fromUnitVectors(glm::vec3 from, glm::vec3 to) {
  float r = glm::dot(from, to) + 1.0f;
  glm::quat q;
  if (r < 1e-8f) {
    r = 0.0f;
    if (std::abs(from.x) > std::abs(from.z)) q = glm::quat(r, -from.y, from.x, 0.0f);
    else q = glm::quat(r, 0.0f, -from.z, from.y);
  } else {
    const glm::vec3 c = glm::cross(from, to);
    q = glm::quat(r, c.x, c.y, c.z);
  }
  return glm::normalize(q);
}

// Object3D.rotation.set(x, y, z) with the default order 'XYZ' as a matrix (Rx * Ry * Rz).
inline glm::mat4 eulerXYZ(float x, float y, float z) {
  const glm::mat4 rx = glm::rotate(glm::mat4(1.0f), x, glm::vec3(1, 0, 0));
  const glm::mat4 ry = glm::rotate(glm::mat4(1.0f), y, glm::vec3(0, 1, 0));
  const glm::mat4 rz = glm::rotate(glm::mat4(1.0f), z, glm::vec3(0, 0, 1));
  return rx * ry * rz;
}

// A hex colour as three uses it in a material (sRGB -> linear working colour space).
inline glm::vec3 hex(uint32_t h) {
  const auto c = [](uint32_t v) {
    const float x = static_cast<float>(v) / 255.0f;
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
  };
  return {c((h >> 16) & 255), c((h >> 8) & 255), c(h & 255)};
}

// Engine2's effect random: an LCG over uint32 (Math.imul(rng, 1664525) + 1013904223), / 2^32.
struct Lcg {
  uint32_t state = 1;
  float next() {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<double>(state) / 4294967296.0);
  }
  void advance() { state = state * 1664525u + 1013904223u; }
};

}  // namespace ember::three
