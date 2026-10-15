#include "world/FogOfWar.h"

#include <algorithm>
#include <cmath>

namespace ember::e2 {

namespace {

struct Cube {
  double q, r, s;
};

Cube oddrToCube(HexPoint p) {
  const double q = p.c - (p.r - (p.r & 1)) / 2;
  return {q, static_cast<double>(p.r), -q - p.r};
}

// Engine2's cubeRound (JavaScript Math.round).
Cube cubeRound(double q, double r, double s) {
  double rq = jsRound(q), rr = jsRound(r), rs = jsRound(s);
  const double dq = std::abs(rq - q), dr = std::abs(rr - r), ds = std::abs(rs - s);
  if (dq > dr && dq > ds) rq = -rr - rs;
  else if (dr > ds) rr = -rq - rs;
  else rs = -rq - rr;
  return {rq, rr, rs};
}

// Engine2's boxBlur (src/game/gfx/three/ThreeGroundAO.ts): one pass along rows or columns, edges clamped.
void boxBlur(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int r, bool horizontal) {
  const int len = horizontal ? w : h, lines = horizontal ? h : w, step = horizontal ? 1 : w;
  const float norm = 1.0f / static_cast<float>(2 * r + 1);
  for (int line = 0; line < lines; line++) {
    const int base = horizontal ? line * w : line;
    float sum = 0;
    for (int k = -r; k <= r; k++) sum += src[base + std::min(len - 1, std::max(0, k)) * step];
    for (int i = 0; i < len; i++) {
      dst[base + i * step] = sum * norm;
      sum += src[base + std::min(len - 1, i + r + 1) * step] - src[base + std::max(0, i - r) * step];
    }
  }
}

}  // namespace

int hexDistance(HexPoint a, HexPoint b) {
  const Cube A = oddrToCube(a), B = oddrToCube(b);
  return static_cast<int>((std::abs(A.q - B.q) + std::abs(A.r - B.r) + std::abs(A.s - B.s)) / 2);
}

std::vector<HexPoint> hexLine(HexPoint a, HexPoint b) {
  const int n = hexDistance(a, b);
  if (n == 0) return {a};
  const Cube A = oddrToCube(a), B = oddrToCube(b);
  std::vector<HexPoint> out;
  for (int i = 0; i <= n; i++) {
    const double t = static_cast<double>(i) / n;
    const Cube c = cubeRound(A.q + (B.q - A.q) * t, A.r + (B.r - A.r) * t, A.s + (B.s - A.s) * t);
    const int r = static_cast<int>(c.r);
    const HexPoint p{static_cast<int>(c.q) + (r - (r & 1)) / 2, r};
    if (!out.empty() && out.back() == p) continue;
    out.push_back(p);
  }
  return out;
}

FogOfWar::FogOfWar(const Board& board) : layout_(board.layout()), cells_(board.cells().size(), kFogUnseen) {
  for (const Cell& c : board.cells()) ground_.push_back(static_cast<float>(c.groundY));
  top_ = ground_;
}

void FogOfWar::block(double x, double z, float height) {
  const auto [c, r] = hexAt(layout_, x, z);
  const size_t i = static_cast<size_t>(indexOf(layout_, c, r));
  top_[i] = std::max(top_[i], ground_[i] + height);
}

// One ray, sampled every fifth of a hex radius: blocked where a hex it crosses (other than the eye's and the
// target's) stands above it.
bool FogOfWar::rayClear(double ax, double az, double ay, double bx, double bz, double by, int fromIndex, int toIndex) const {
  constexpr double kStep = 0.2;
  const double length = std::hypot(bx - ax, bz - az);
  const int samples = std::max(1, static_cast<int>(std::ceil(length / kStep)));
  for (int k = 1; k < samples; k++) {
    const double t = static_cast<double>(k) / samples;
    const auto [c, r] = hexAt(layout_, ax + (bx - ax) * t, az + (bz - az) * t);
    const int i = indexOf(layout_, c, r);
    if (i == fromIndex || i == toIndex) continue;
    if (top_[static_cast<size_t>(i)] > ay + (by - ay) * t + 1e-4) return false;
  }
  return true;
}

bool FogOfWar::sightReaches(HexPoint from, HexPoint to) const {
  if (from == to) return true;
  const int fi = indexOf(layout_, from.c, from.r), ti = indexOf(layout_, to.c, to.r);
  const double ax = hexX(layout_, from.c, from.r), az = hexZ(layout_, from.r);
  const double bx = hexX(layout_, to.c, to.r), bz = hexZ(layout_, to.r);
  const double ay = ground_[static_cast<size_t>(fi)] + kEyeHeight, by = ground_[static_cast<size_t>(ti)] + kTargetHeight;
  const double length = std::hypot(bx - ax, bz - az);
  // Sideways, about half the hex's inner radius: rays grazing a corner are not settled by rounding.
  const double px = -(bz - az) / length * 0.45, pz = (bx - ax) / length * 0.45;
  for (const double side : {0.0, 1.0, -1.0})
    if (rayClear(ax + px * side, az + pz * side, ay, bx + px * side, bz + pz * side, by, fi, ti)) return true;
  return false;
}

void FogOfWar::relight(std::span<const Eye> eyes) {
  for (uint8_t& v : cells_)
    if (v == kFogVisible) v = kFogExplored;
  for (const Eye& eye : eyes) {
    const int x0 = std::max(0, eye.at.c - eye.radius), x1 = std::min(layout_.cols - 1, eye.at.c + eye.radius);
    const int y0 = std::max(0, eye.at.r - eye.radius), y1 = std::min(layout_.rows - 1, eye.at.r + eye.radius);
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        uint8_t& v = cells_[static_cast<size_t>(indexOf(layout_, x, y))];
        if (v != kFogVisible && hexDistance(eye.at, {x, y}) <= eye.radius && sightReaches(eye.at, {x, y})) v = kFogVisible;
      }
  }
  ++version_;
}

FogMask FogOfWar::paint(const Board& board, std::span<const HexPoint> party) const {
  constexpr float kMargin = 1.5f;  // off-board margin (Engine2 MARGIN)
  constexpr int kBlurRadius = 2, kBlurPasses = 2;
  // No fog within one hex of a living party unit, ever (Engine2 syncFog's clear set).
  std::vector<uint8_t> state = cells_;
  for (const HexPoint& p : party) {
    if (!inBoard(layout_, p.c, p.r)) continue;
    state[static_cast<size_t>(indexOf(layout_, p.c, p.r))] = kFogVisible;
    for (const auto& [c, r] : neighbors(layout_, p.c, p.r)) state[static_cast<size_t>(indexOf(layout_, c, r))] = kFogVisible;
  }
  const Board::Bounds b = board.bounds();
  FogMask m;
  m.x0 = static_cast<float>(b.x0) - kMargin;
  m.z0 = static_cast<float>(b.z0) - kMargin;
  m.worldWidth = static_cast<float>(b.x1 - b.x0) + 2 * kMargin;
  m.worldDepth = static_cast<float>(b.z1 - b.z0) + 2 * kMargin;
  m.width = static_cast<int>(std::ceil(m.worldWidth * FogMask::kRes));
  m.height = static_cast<int>(std::ceil(m.worldDepth * FogMask::kRes));
  const size_t n = static_cast<size_t>(m.width) * static_cast<size_t>(m.height);
  // Each texel's hex state (Engine2's hexAt clamps to the board, so the margin takes the edge hex's state).
  std::vector<uint8_t> texelState(n);
  for (int y = 0; y < m.height; y++)
    for (int x = 0; x < m.width; x++) {
      const auto [c, r] = hexAt(layout_, m.x0 + (x + 0.5) / FogMask::kRes, m.z0 + (y + 0.5) / FogMask::kRes);
      texelState[static_cast<size_t>(y) * m.width + x] = state[static_cast<size_t>(indexOf(layout_, c, r))];
    }
  std::vector<float> alpha(n), tmp(n);
  for (size_t i = 0; i < n; i++)
    alpha[i] = texelState[i] == kFogUnseen ? 1.0f : texelState[i] == kFogExplored ? kExploredAlpha : 0.0f;
  for (int p = 0; p < kBlurPasses; p++) {
    boxBlur(alpha, tmp, m.width, m.height, kBlurRadius, true);
    boxBlur(tmp, alpha, m.width, m.height, kBlurRadius, false);
  }
  // In sight stays clear and never-seen stays flat black; the soft edge lives only inside remembered hexes.
  m.alpha.resize(n);
  for (size_t i = 0; i < n; i++) {
    const float a = texelState[i] == kFogVisible ? 0.0f : texelState[i] == kFogUnseen ? 1.0f : std::min(kExploredAlpha, alpha[i]);
    m.alpha[i] = static_cast<uint8_t>(jsRound(255.0 * a));
  }

  // The volumetric fog's fields. Boundary: never seen, blurred about a hex and a half (fog fades across it).
  const auto blur = [&](std::vector<float>& v, int radius, int passes) {
    for (int p = 0; p < passes; p++) {
      boxBlur(v, tmp, m.width, m.height, radius, true);
      boxBlur(tmp, v, m.width, m.height, radius, false);
    }
  };
  m.boundary.resize(n);
  for (size_t i = 0; i < n; i++) m.boundary[i] = texelState[i] == kFogUnseen ? 1.0f : 0.0f;
  blur(m.boundary, FogMask::kRes * 3 / 4, 3);
  // Ground: each texel's hex, softened a little; never-seen hexes read the board's mean height.
  double mean = 0;
  for (const float g : ground_) mean += g;
  mean /= std::max<size_t>(1, ground_.size());
  m.ground.resize(n);
  for (int y = 0; y < m.height; y++)
    for (int x = 0; x < m.width; x++) {
      const size_t i = static_cast<size_t>(y) * m.width + x;
      const auto [c, r] = hexAt(layout_, m.x0 + (x + 0.5) / FogMask::kRes, m.z0 + (y + 0.5) / FogMask::kRes);
      m.ground[i] = texelState[i] == kFogUnseen ? static_cast<float>(mean) : ground_[static_cast<size_t>(indexOf(layout_, c, r))];
    }
  blur(m.ground, FogMask::kRes / 4, 2);
  // Valleys: the ground's surroundings (about 3 hexes) above it.
  m.valley = m.ground;
  blur(m.valley, FogMask::kRes * 3 / 2, 2);
  for (size_t i = 0; i < n; i++) m.valley[i] = std::max(0.0f, m.valley[i] - m.ground[i]);
  return m;
}

}  // namespace ember::e2
