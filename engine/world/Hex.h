#pragma once

// Engine2's hex layout (C:/Engine2/src/core/hex.ts), which is Ember's: pointy-top hexes, odd rows shifted
// half a hex right; columns along +x, rows along +z, the board centred on the origin. One world unit = one
// hex radius. Ported exactly (doubles, JavaScript rounding).

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "world/Noise.h"

namespace ember::e2 {

inline constexpr double kHexRadius = 1.0;
inline const double kSqrt3 = std::sqrt(3.0);
inline constexpr double kPi = 3.14159265358979323846;
// World height of one elevation level.
inline constexpr double kStep = 1.0;

struct HexLayout {
  int cols = 0, rows = 0;
  double ox = 0, oz = 0;  // half extents of the hex-centre rectangle
};

inline HexLayout makeLayout(int cols, int rows) {
  return {cols, rows, (kSqrt3 * kHexRadius * (cols - 0.5)) / 2, 0.75 * kHexRadius * (rows - 1)};
}

inline double hexX(const HexLayout& L, int c, int r) { return kSqrt3 * kHexRadius * (c + 0.5 * (r & 1)) - L.ox; }
inline double hexZ(const HexLayout& L, int r) { return 1.5 * kHexRadius * r - L.oz; }
inline int indexOf(const HexLayout& L, int c, int r) { return r * L.cols + c; }
inline bool inBoard(const HexLayout& L, int c, int r) { return c >= 0 && r >= 0 && c < L.cols && r < L.rows; }

// Logical hex under a world point, clamped to the board.
inline std::pair<int, int> hexAt(const HexLayout& L, double x, double z) {
  const double rf = (z + L.oz) / (1.5 * kHexRadius), qf = (x + L.ox) / (kSqrt3 * kHexRadius) - rf / 2, sf = -qf - rf;
  double q = jsRound(qf), r = jsRound(rf);
  const double s = jsRound(sf);
  const double dq = std::abs(q - qf), dr = std::abs(r - rf), ds = std::abs(s - sf);
  if (dq > dr && dq > ds) q = -r - s;
  else if (dr > ds) r = -q - s;
  const int ri = static_cast<int>(r);
  const int c = static_cast<int>(q + (r - (ri & 1)) / 2);
  return {std::max(0, std::min(L.cols - 1, c)), std::max(0, std::min(L.rows - 1, ri))};
}

inline std::vector<std::pair<int, int>> neighbors(const HexLayout& L, int c, int r) {
  static constexpr int odd[6][2] = {{1, 0}, {-1, 0}, {1, -1}, {0, -1}, {1, 1}, {0, 1}};
  static constexpr int even[6][2] = {{1, 0}, {-1, 0}, {0, -1}, {-1, -1}, {0, 1}, {-1, 1}};
  const auto& o = (r & 1) ? odd : even;
  std::vector<std::pair<int, int>> out;
  for (const auto& d : o)
    if (inBoard(L, c + d[0], r + d[1])) out.emplace_back(c + d[0], r + d[1]);
  return out;
}

// Corner k (0-5) of a hex: corner 0 points to +z, clockwise seen from above.
inline std::pair<double, double> corner(double cx, double cz, int k, double rr = kHexRadius) {
  const double a = (k * kPi) / 3;
  return {cx + rr * std::sin(a), cz + rr * std::cos(a)};
}

}  // namespace ember::e2
