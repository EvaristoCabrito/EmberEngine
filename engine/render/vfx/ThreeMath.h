#pragma once

// The bits of three.js math Ember's effects lean on, with three's exact behaviour (r16x src/math, src/extras/curves):
// smoothstep, the centripetal Catmull-Rom curve, setFromUnitVectors, Euler XYZ, colour hex (sRGB -> linear, as three's
// ColorManagement does for hex colours), Ember's LCG random.

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

  // Curve.getLengths (arcLengthDivisions 200), getUtoTmapping, getPointAt, getTangent, getTangentAt.
  std::vector<float> getLengths(int divisions = 200) const {
    std::vector<float> cache{0.0f};
    glm::vec3 last = getPoint(0.0f);
    float sum = 0.0f;
    for (int p = 1; p <= divisions; ++p) {
      const glm::vec3 current = getPoint(static_cast<float>(p) / divisions);
      sum += glm::length(current - last);
      cache.push_back(sum);
      last = current;
    }
    return cache;
  }
  float getUtoTmapping(float u, const std::vector<float>& arcLengths) const {
    const int il = static_cast<int>(arcLengths.size());
    const float target = u * arcLengths[il - 1];
    int low = 0, high = il - 1, i = 0;
    while (low <= high) {
      i = low + (high - low) / 2;
      const float comparison = arcLengths[i] - target;
      if (comparison < 0) low = i + 1;
      else if (comparison > 0) high = i - 1;
      else {
        high = i;
        break;
      }
    }
    i = high;
    if (arcLengths[i] == target) return static_cast<float>(i) / (il - 1);
    const float before = arcLengths[i], after = arcLengths[i + 1];
    return (i + (target - before) / (after - before)) / (il - 1);
  }
  glm::vec3 getTangent(float t) const {
    const float delta = 0.0001f;
    const float t1 = std::max(0.0f, t - delta), t2 = std::min(1.0f, t + delta);
    return normalize(getPoint(t2) - getPoint(t1));
  }
};

// Curve.computeFrenetFrames(segments, false): tangents by arc length, a first normal off the tangent's smallest axis,
// then parallel transport.
inline void frenetFrames(const CatmullRom& path, int segments, std::vector<glm::vec3>& tangents, std::vector<glm::vec3>& normals,
                         std::vector<glm::vec3>& binormals) {
  const std::vector<float> lengths = path.getLengths();
  tangents.assign(segments + 1, glm::vec3(0.0f));
  normals.assign(segments + 1, glm::vec3(0.0f));
  binormals.assign(segments + 1, glm::vec3(0.0f));
  for (int i = 0; i <= segments; ++i) tangents[i] = path.getTangent(path.getUtoTmapping(static_cast<float>(i) / segments, lengths));
  float min = 3.4e38f;
  glm::vec3 axis{0.0f};
  const float tx = std::abs(tangents[0].x), ty = std::abs(tangents[0].y), tz = std::abs(tangents[0].z);
  if (tx <= min) min = tx, axis = {1, 0, 0};
  if (ty <= min) min = ty, axis = {0, 1, 0};
  if (tz <= min) axis = {0, 0, 1};
  glm::vec3 vec = normalize(glm::cross(tangents[0], axis));
  normals[0] = glm::cross(tangents[0], vec);
  binormals[0] = glm::cross(tangents[0], normals[0]);
  for (int i = 1; i <= segments; ++i) {
    normals[i] = normals[i - 1];
    binormals[i] = binormals[i - 1];
    vec = glm::cross(tangents[i - 1], tangents[i]);
    if (glm::length(vec) > 1.1920929e-7f) {
      vec = glm::normalize(vec);
      const float theta = std::acos(std::clamp(glm::dot(tangents[i - 1], tangents[i]), -1.0f, 1.0f));
      normals[i] = glm::vec3(glm::rotate(glm::mat4(1.0f), theta, vec) * glm::vec4(normals[i], 0.0f));
    }
    binormals[i] = glm::cross(tangents[i], normals[i]);
  }
}
// Curve.getPointAt(u): by arc length.
inline glm::vec3 getPointAt(const CatmullRom& path, float u, const std::vector<float>& lengths) { return path.getPoint(path.getUtoTmapping(u, lengths)); }

// Ember's mulberry (PhantasmalForceVFX): state += 0x6d2b79f5, mixed, / 2^32.
struct Mulberry {
  uint32_t state = 0;
  float next() {
    state += 0x6d2b79f5u;
    uint32_t v = state;
    v = (v ^ (v >> 15)) * (v | 1u);
    v ^= v + (v ^ (v >> 7)) * (v | 61u);
    return static_cast<float>(static_cast<double>(v ^ (v >> 14)) / 4294967296.0);
  }
};

// Color.setHSL(h, s, l) (the values land in the working colour space as they are).
inline glm::vec3 hsl(float h, float s, float l) {
  auto hue2rgb = [](float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6 * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * 6 * (2.0f / 3.0f - t);
    return p;
  };
  h = h - std::floor(h);  // euclideanModulo(h, 1)
  s = std::clamp(s, 0.0f, 1.0f);
  l = std::clamp(l, 0.0f, 1.0f);
  if (s == 0) return glm::vec3(l);
  const float p = l <= 0.5f ? l * (1 + s) : l + s - l * s;
  const float q = 2 * l - p;
  return {hue2rgb(q, p, h + 1.0f / 3.0f), hue2rgb(q, p, h), hue2rgb(q, p, h - 1.0f / 3.0f)};
}

// TubeGeometry(path, tubularSegments, radius, radialSegments, closed = false): rings along the curve by arc length,
// oriented by three's Frenet frames (computeFrenetFrames: a first normal off the tangent's smallest axis, then
// parallel transport). Triangles (indices expanded).
inline std::vector<glm::vec3> tubeRings(const CatmullRom& path, int tubularSegments, float radius, int radialSegments,
                                         std::vector<glm::vec3>* normalsOut) {
  const std::vector<float> lengths = path.getLengths();
  std::vector<glm::vec3> tangents(tubularSegments + 1), normals(tubularSegments + 1), binormals(tubularSegments + 1);
  for (int i = 0; i <= tubularSegments; ++i) tangents[i] = path.getTangent(path.getUtoTmapping(static_cast<float>(i) / tubularSegments, lengths));
  float min = 3.4e38f;
  glm::vec3 axis{0.0f};
  const float tx = std::abs(tangents[0].x), ty = std::abs(tangents[0].y), tz = std::abs(tangents[0].z);
  if (tx <= min) min = tx, axis = {1, 0, 0};
  if (ty <= min) min = ty, axis = {0, 1, 0};
  if (tz <= min) axis = {0, 0, 1};
  glm::vec3 vec = normalize(glm::cross(tangents[0], axis));
  normals[0] = glm::cross(tangents[0], vec);
  binormals[0] = glm::cross(tangents[0], normals[0]);
  for (int i = 1; i <= tubularSegments; ++i) {
    normals[i] = normals[i - 1];
    binormals[i] = binormals[i - 1];
    vec = glm::cross(tangents[i - 1], tangents[i]);
    if (glm::length(vec) > 1.1920929e-7f) {
      vec = glm::normalize(vec);
      const float theta = std::acos(std::clamp(glm::dot(tangents[i - 1], tangents[i]), -1.0f, 1.0f));
      normals[i] = glm::vec3(glm::rotate(glm::mat4(1.0f), theta, vec) * glm::vec4(normals[i], 0.0f));
    }
    binormals[i] = glm::cross(tangents[i], normals[i]);
  }
  std::vector<glm::vec3> ringPoints, ringNormals;
  for (int i = 0; i <= tubularSegments; ++i) {
    const glm::vec3 p = path.getPoint(path.getUtoTmapping(static_cast<float>(i) / tubularSegments, lengths));
    for (int j = 0; j <= radialSegments; ++j) {
      const float v = static_cast<float>(j) / radialSegments * 2.0f * 3.14159265358979f;
      const float sn = std::sin(v), cs = -std::cos(v);
      const glm::vec3 n = normalize(cs * normals[i] + sn * binormals[i]);
      ringNormals.push_back(n);
      ringPoints.push_back(p + radius * n);
    }
  }
  if (normalsOut) *normalsOut = ringNormals;
  return ringPoints;
}

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

// Ember's effect random: an LCG over uint32 (Math.imul(rng, 1664525) + 1013904223), / 2^32.
struct Lcg {
  uint32_t state = 1;
  float next() {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<double>(state) / 4294967296.0);
  }
  void advance() { state = state * 1664525u + 1013904223u; }
};

}  // namespace ember::three
