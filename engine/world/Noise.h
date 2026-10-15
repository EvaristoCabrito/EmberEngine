#pragma once

// Engine2's deterministic noise (C:/Engine2/src/core/noise.ts), ported bit for bit: the ground's shape
// and its procedural textures depend on it. Arithmetic follows JavaScript: doubles everywhere,
// Math.imul's 32-bit wrap, ToInt32 on bitwise operands, % with the sign of the dividend.

#include <cmath>
#include <cstdint>

namespace ember::e2 {

// ECMAScript ToInt32.
inline uint32_t toUint32(double v) {
  if (!std::isfinite(v)) return 0;
  double m = std::fmod(std::trunc(v), 4294967296.0);
  if (m < 0) m += 4294967296.0;
  return static_cast<uint32_t>(m);
}

inline double hash(double x, double y) {
  uint32_t h = toUint32(x) * 374761393u + toUint32(y) * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return h / 4294967296.0;
}

inline double sstep(double t) { return t * t * (3 - 2 * t); }

inline double smooth(double a, double b, double x) {
  double t = (x - a) / (b - a);
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return t * t * (3 - 2 * t);
}

inline double vnoise(double x, double y) {
  const double xi = std::floor(x), yi = std::floor(y), u = sstep(x - xi), v = sstep(y - yi);
  const double a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

inline double fbm(double x, double y, int octaves = 4) {
  double t = 0, a = 0.5, f = 1, n = 0;
  for (int i = 0; i < octaves; i++) {
    t += a * vnoise(x * f, y * f);
    n += a;
    a *= 0.5;
    f *= 2;
  }
  return t / n;
}

// Tileable value noise: repeats every (px, py).
inline double pnoise(double x, double y, double px, double py) {
  const double xi = std::floor(x), yi = std::floor(y), u = sstep(x - xi), v = sstep(y - yi);
  const auto H = [&](double a, double b) { return hash(std::fmod(std::fmod(a, px) + px, px), std::fmod(std::fmod(b, py) + py, py)); };
  const double a = H(xi, yi), b = H(xi + 1, yi), c = H(xi, yi + 1), d = H(xi + 1, yi + 1);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

inline double pfbm(double x, double y, double px, double py, int octaves = 5) {
  double t = 0, a = 0.5, n = 0;
  for (int i = 0; i < octaves; i++) {
    t += a * pnoise(x, y, px, py);
    n += a;
    x *= 2;
    y *= 2;
    px *= 2;
    py *= 2;
    a *= 0.5;
  }
  return t / n;
}

// JavaScript's Math.round (halves round up, towards +infinity).
inline double jsRound(double v) { return std::floor(v + 0.5); }

}  // namespace ember::e2
