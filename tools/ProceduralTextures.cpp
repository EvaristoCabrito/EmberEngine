#include "ProceduralTextures.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

#include "world/Noise.h"

namespace ember {

namespace {

using namespace ember;
using RGB = std::array<double, 3>;

constexpr RGB rgb(uint32_t hex) { return {double((hex >> 16) & 255), double((hex >> 8) & 255), double(hex & 255)}; }
RGB mix(const RGB& a, const RGB& b, double t) { return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t}; }
RGB mul(const RGB& a, double k) { return {a[0] * k, a[1] * k, a[2] * k}; }
double grain(double x, double y, double a) { return 1 - a / 2 + a * hash(x * 3 + 1, y * 7 + 2); }

// ImageData stores through a Uint8ClampedArray: clamp, then round half to even.
uint8_t clamped(double v) {
  if (!(v > 0)) return 0;
  if (v >= 255) return 255;
  const double f = std::floor(v);
  if (f + 0.5 < v) return static_cast<uint8_t>(f + 1);
  if (v < f + 0.5) return static_cast<uint8_t>(f);
  return static_cast<uint8_t>(static_cast<int>(f) % 2 == 0 ? f : f + 1);
}

constexpr double S = 256;
const RGB g1 = rgb(0x3d7329), g2 = rgb(0x79ab47), gDry = rgb(0xa39f55), gTip = rgb(0xb9d777);
const RGB f1 = rgb(0x2c4f1f), f2 = rgb(0x55783a), leaf = rgb(0x7a5a2c);
const RGB d1 = rgb(0x4f3622), d2 = rgb(0x8a6440);
const RGB r1 = rgb(0x57524d), r2 = rgb(0xa19a8f), moss = rgb(0x56713c);
const RGB n1 = rgb(0xc9d5e2), n2 = rgb(0xffffff);
const RGB s1 = rgb(0x6f6b64), s2 = rgb(0xa39e94), mortar = rgb(0x3f3c38);
const RGB a1 = rgb(0x2a2624), a2 = rgb(0x5b534d), coal = rgb(0xff7a2a);

RGB grassAt(double x, double y) {
  const double na = pfbm(x / 32, y / 32, S / 32, S / 32), nb = pfbm(x / 6, y / 6, S / 6, S / 6, 3);
  RGB c = mix(g1, g2, sstep(na));
  if (na > 0.62) c = mix(c, gDry, (na - 0.62) * 1.6);
  c = mul(c, 0.8 + 0.38 * nb);
  if (hash(x, y) > 0.982) c = mix(c, gTip, 0.55);
  return mul(c, grain(x, y, 0.1));
}

RGB rockAt(double x, double y, bool strata) {
  const double layers = strata ? pfbm(x / 64, y / 9, S / 64, S / 9, 4) : pfbm(x / 24, y / 24, S / 24, S / 24, 4);
  RGB c = mix(r1, r2, sstep(layers));
  if (std::abs(pfbm(x / 20, y / 20, S / 20, S / 20, 4) - 0.5) < 0.012) c = mul(c, 0.55);
  const double m = pfbm(x / 48 + 9, y / 48, S / 48, S / 48, 3);
  if (m > 0.6) c = mix(c, moss, std::min(0.45, (m - 0.6) * 2.2));
  return mul(c, grain(x, y, 0.16));
}

RGB forestAt(double x, double y) {
  const double n = pfbm(x / 28, y / 28, S / 28, S / 28), l = pnoise(x / 5, y / 5, S / 5, S / 5);
  RGB c = mul(mix(f1, f2, sstep(n)), 0.85 + 0.3 * pfbm(x / 6, y / 6, S / 6, S / 6, 3));
  if (l > 0.74) c = mix(c, leaf, std::min(0.8, (l - 0.74) * 4));
  return mul(c, grain(x, y, 0.12));
}

RGB paversAt(double x, double y) {
  // irregular stone slabs, ~0.8 world units each
  const double row = std::floor(y / 51.2), off = std::fmod(row, 2) * 25.6, col = std::floor((x + off) / 51.2);
  const double lx = std::fmod(x + off, 51.2), ly = std::fmod(y, 51.2);
  const double edge = std::min({lx, 51.2 - lx, ly, 51.2 - ly});
  const double tone = hash(col, row);
  RGB c = mul(mix(s1, s2, 0.35 + tone * 0.4 + (pfbm(x / 14, y / 14, S / 14, S / 14, 3) - 0.5) * 0.5), grain(x, y, 0.14));
  if (edge < 2.2) c = mix(mortar, c, smooth(0.6, 2.2, edge));
  return c;
}

RGB snowAt(double x, double y) {
  RGB c = mix(n1, n2, sstep(pfbm(x / 40, y / 40, S / 40, S / 40)));
  if (hash(x, y) > 0.993) c = {255, 255, 255};
  return mul(c, grain(x, y, 0.05));
}

RGB gravelAt(double x, double y) {
  const double pb = pnoise(x / 4, y / 4, S / 4, S / 4), n = pfbm(x / 30, y / 30, S / 30, S / 30);
  return mul(mix(rgb(0x4d4a44), rgb(0x8d887d), pb * 0.7 + n * 0.3), grain(x, y, 0.15));
}

RGB ashAt(double x, double y) {
  const double n = pfbm(x / 22, y / 22, S / 22, S / 22);
  RGB c = mix(a1, a2, sstep(n));
  const double e = pnoise(x / 3, y / 3, S / 3, S / 3);
  if (e > 0.86 && n < 0.5) c = mix(c, coal, (e - 0.86) * 5);
  return mul(c, grain(x, y, 0.16));
}

RGB earthAt(double x, double y) {
  RGB c = mix(d1, d2, pfbm(x / 24, y / 10, S / 24, S / 10, 4));
  const double st = pnoise(x / 9, y / 9, S / 9, S / 9);
  if (st > 0.72) c = mix(c, r2, std::min(1.0, (st - 0.72) * 3));
  if (std::abs(pnoise(x / 14, y / 5, S / 14, S / 5) - 0.5) < 0.025) c = mul(c, 0.6);
  return mul(c, grain(x, y, 0.14));
}

// Planicie 007: continuous grass colour, without clumps, stones, grain or baked shading.
RGB smoothPlainsAt(double x, double y) {
  const double n = pfbm(x / 128, y / 128, 8, 8, 2);
  return mix(g1, g2, 0.35 + n * 0.4);
}

void paint(uint32_t size, const std::function<RGB(double, double)>& fn, Rgba8& out) {
  out.width = out.height = size;
  out.pixels.assign(size_t{size} * size * 4, 255);
  for (uint32_t y = 0; y < size; ++y)
    for (uint32_t x = 0; x < size; ++x) {
      const RGB c = fn(x, y);
      uint8_t* p = &out.pixels[(size_t{y} * size + x) * 4];
      for (int k = 0; k < 3; ++k) p[k] = clamped(c[k]);
    }
}

}  // namespace

bool classicProceduralTexture(std::string_view name, Rgba8& out) {
  const uint32_t s = static_cast<uint32_t>(S);
  if (name == "grass") paint(s, grassAt, out);
  else if (name == "forest") paint(s, forestAt, out);
  else if (name == "pavers") paint(s, paversAt, out);
  else if (name == "snow") paint(s, snowAt, out);
  else if (name == "rock") paint(s, [](double x, double y) { return rockAt(x, y, false); }, out);
  else if (name == "gravel") paint(s, gravelAt, out);
  else if (name == "ash") paint(s, ashAt, out);
  else if (name == "earth") paint(s, earthAt, out);
  else if (name == "cliff") paint(s, [](double x, double y) { return rockAt(x, y, true); }, out);
  else if (name == "smooth-plains") paint(1024, smoothPlainsAt, out);
  else return false;
  return true;
}

}  // namespace ember
