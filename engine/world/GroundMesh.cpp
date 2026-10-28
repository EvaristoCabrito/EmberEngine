#include "world/GroundMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace ember {

namespace {

constexpr double kWarp = 1.25;
// How far the board's cut edge drops below its ground: a thin lip, no underground on show.
constexpr double kEdgeLip = 0.35;
constexpr int kSurfaces = 8;
// Ember's architecture stands on a square grid: walls 0.32 thick on the cell's centre line.
constexpr double kWallHalf = 0.16;

const std::array<std::array<double, 2>, 7> kRing7 = [] {
  std::array<std::array<double, 2>, 7> r{};
  r[0] = {0, 0};
  for (int k = 0; k < 6; ++k) r[k + 1] = {std::sin((k * kPi) / 3), std::cos((k * kPi) / 3)};
  return r;
}();

std::vector<float> toFloat(const std::vector<double>& v) {
  std::vector<float> out(v.size());
  for (size_t i = 0; i < v.size(); ++i) out[i] = static_cast<float>(v[i]);
  return out;
}

uint32_t strideOf(const Board& b) { return static_cast<uint32_t>(kSurfaces + b.data().groundLayerCount); }

// Surface and layer weights around a point, blended over real ground only; with no ground in reach
// the fallback cell's. Written as float, like Ember's Float32Array scratch.
void splat(const Board& b, double sx, double sz, double rad, float* out, const Cell* fallback) {
  const uint32_t stride = strideOf(b);
  std::vector<double> w(stride, 0.0);
  int n = 0;
  for (const auto& o : kRing7) {
    const Cell& cell = b.cellAt(sx + o[0] * rad, sz + o[1] * rad);
    if (cell.surface < 0) continue;
    n++;
    w[cell.surface] += 1;
    if (cell.ground >= 0) w[kSurfaces + cell.ground] += 1;
  }
  if (!n && fallback && fallback->surface >= 0) {
    n = 1;
    w[fallback->surface] = 1;
    if (fallback->ground >= 0) w[kSurfaces + fallback->ground] = 1;
  }
  for (uint32_t k = 0; k < stride; ++k) out[k] = static_cast<float>(w[k] / (n ? n : 1));
}

// three.js BufferGeometry.computeVertexNormals for an indexed mesh: area-weighted face normals summed
// into a float attribute, then normalised (unused vertices stay zero).
std::vector<float> vertexNormals(const std::vector<float>& pos, const std::vector<uint32_t>& index) {
  std::vector<float> n(pos.size(), 0.0f);
  for (size_t i = 0; i + 2 < index.size(); i += 3) {
    const uint32_t v[3] = {index[i], index[i + 1], index[i + 2]};
    double p[3][3];
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j) p[k][j] = pos[v[k] * 3 + j];
    const double cb[3] = {p[2][0] - p[1][0], p[2][1] - p[1][1], p[2][2] - p[1][2]};
    const double ab[3] = {p[0][0] - p[1][0], p[0][1] - p[1][1], p[0][2] - p[1][2]};
    const double c[3] = {cb[1] * ab[2] - cb[2] * ab[1], cb[2] * ab[0] - cb[0] * ab[2], cb[0] * ab[1] - cb[1] * ab[0]};
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j) n[v[k] * 3 + j] = static_cast<float>(static_cast<double>(n[v[k] * 3 + j]) + c[j]);
  }
  for (size_t v = 0; v < n.size(); v += 3) {
    const double x = n[v], y = n[v + 1], z = n[v + 2];
    double len = std::sqrt(x * x + y * y + z * z);
    if (len == 0) len = 1;
    n[v] = static_cast<float>(x / len);
    n[v + 1] = static_cast<float>(y / len);
    n[v + 2] = static_cast<float>(z / len);
  }
  return n;
}

// Where the board has ground: hex cells, except around Ember's architecture, where the ground follows
// the walls' square grid and stops at a wall's outer face where void lies beyond it.
std::function<bool(double, double)> groundFootprint(const Board& b) {
  const HexLayout& L = b.layout();
  std::unordered_set<int> walls;
  for (const MapDecoration& p : b.draft().decorations)
    if (b.data().isArchitecture(p.id) && inBoard(L, p.x, p.y)) walls.insert(p.y * L.cols + p.x);
  const auto hexSolid = [&b](double x, double z) { return b.cellAt(x, z).surface >= 0; };
  if (walls.empty()) return hexSolid;
  return [&b, walls = std::move(walls), hexSolid](double x, double z) {
    const HexLayout& L = b.layout();
    const auto isWall = [&](int c, int r) { return inBoard(L, c, r) && walls.contains(r * L.cols + c); };
    const auto voidAt = [&](int c, int r) { return inBoard(L, c, r) && b.cell(c, r).surface < 0; };
    const double fc = (x + L.ox) / kSqrt3, fr = (z + L.oz) / 1.5;
    const int c = static_cast<int>(jsRound(fc)), r = static_cast<int>(jsRound(fr));
    bool near = false;
    for (int dr = -1; dr <= 1 && !near; dr++)
      for (int dc = -1; dc <= 1; dc++)
        if (isWall(c + dc, r + dr)) {
          near = true;
          break;
        }
    if (!near) return hexSolid(x, z);
    if (!inBoard(L, c, r) || b.cell(c, r).surface < 0) return false;
    if (!isWall(c, r)) return true;
    // a wall cell: cut at the wall's face on every side that opens onto void
    const double ox = (fc - c) * kSqrt3, oz = (fr - r) * 1.5;
    if (ox < -kWallHalf && voidAt(c - 1, r)) return false;
    if (ox > kWallHalf && voidAt(c + 1, r)) return false;
    if (oz < -kWallHalf && voidAt(c, r - 1)) return false;
    if (oz > kWallHalf && voidAt(c, r + 1)) return false;
    return true;
  };
}

bool flatPlainsVariant(const Board& b, const Cell& cell) {
  if (cell.type != "plains" || !b.draft().hasVariant(cell.i)) return false;
  const int v = b.draft().variant(cell.i);
  return v >= 56 && v <= 61;
}

}  // namespace

std::pair<double, double> warp(double x, double z) {
  return {x + (fbm(x * 0.42 + 3.1, z * 0.42, 3) - 0.5) * kWarp, z + (fbm(x * 0.42, z * 0.42 + 7.7, 3) - 0.5) * kWarp};
}

GroundGeometry buildSmoothGround(const Board& b) {
  const Board::Bounds bd = b.bounds();
  const double x0 = bd.x0, x1 = bd.x1, z0 = bd.z0, z1 = bd.z1;
  const uint32_t stride = strideOf(b);
  const double res = std::max(0.125, std::sqrt(((x1 - x0) * (z1 - z0)) / 220000));
  const int nx = static_cast<int>(std::ceil((x1 - x0) / res)), nz = static_cast<int>(std::ceil((z1 - z0) / res));
  const int VX = nx + 3, VZ = nz + 3;  // +1 ring on each side forms the diorama's cut walls
  const int64_t N = int64_t{VX} * VZ;
  std::array<std::array<double, 2>, 9> SM{};
  SM[0] = {0, 0};
  for (int k = 0; k < 8; ++k) SM[k + 1] = {std::sin((k * kPi) / 4) * res * 1.1, std::cos((k * kPi) / 4) * res * 1.1};
  const auto solidAt = groundFootprint(b);
  std::vector<double> pos;
  std::vector<float> spl, tmp(stride);
  std::vector<double> lipOf;
  pos.reserve(size_t(N) * 3);
  spl.reserve(size_t(N) * stride);
  // One ground point; lip (0..1) sinks it toward the board's outer cut edge.
  const auto addPoint = [&](double x, double z, double lip) -> uint32_t {
    const auto [wx, wz] = warp(x, z);
    // only real ground counts toward the height
    double h = 0;
    int solid = 0;
    for (const auto& o : SM) {
      const Cell& s = b.cellAt(wx + o[0], wz + o[1]);
      if (s.surface >= 0) {
        h += s.groundY;
        solid++;
      }
    }
    const Cell& own = b.cellAt(x, z);
    h = solid ? h / solid : own.groundY;
    const Cell& cell = b.cellAt(wx, wz);
    const bool flatPlains = flatPlainsVariant(b, cell);
    const double top = h + b.reliefAt(x, z);
    pos.push_back(x);
    pos.push_back(lip > 0 ? top - kEdgeLip * lip : top + (flatPlains ? 0 : (vnoise(x * 2.3, z * 2.3) - 0.5) * 0.04));
    pos.push_back(z);
    splat(b, wx, wz, 0.3, tmp.data(), &own);
    spl.insert(spl.end(), tmp.begin(), tmp.end());
    lipOf.push_back(lip);
    return static_cast<uint32_t>(lipOf.size() - 1);
  };
  std::vector<uint8_t> inside(size_t(N), 0);
  for (int j = 0; j < VZ; j++)
    for (int i = 0; i < VX; i++) {
      const int gi = i - 1, gj = j - 1;
      const bool edge = gi < 0 || gj < 0 || gi > nx || gj > nz;
      const double x = std::min(x1, std::max(x0, x0 + gi * res)), z = std::min(z1, std::max(z0, z0 + gj * res));
      inside[addPoint(x, z, edge ? 1 : 0)] = solidAt(x, z) ? 1 : 0;
    }
  // Where a grid edge leaves the ground: the exact crossing, found by halving, shared by both sides.
  std::unordered_map<int64_t, uint32_t> cuts;
  const auto cut = [&](uint32_t s, uint32_t v) -> uint32_t {
    const int64_t key = s < v ? int64_t{s} * N + v : int64_t{v} * N + s;
    if (const auto it = cuts.find(key); it != cuts.end()) return it->second;
    const double sx = pos[s * 3], sz = pos[s * 3 + 2], dx = pos[v * 3] - sx, dz = pos[v * 3 + 2] - sz;
    double lo = 0, hi = 1;
    for (int k = 0; k < 10; k++) {
      const double m = (lo + hi) / 2;
      if (solidAt(sx + dx * m, sz + dz * m)) lo = m;
      else hi = m;
    }
    const uint32_t idx = addPoint(sx + dx * lo, sz + dz * lo, lipOf[s] + (lipOf[v] - lipOf[s]) * lo);
    cuts.emplace(key, idx);
    return idx;
  };
  std::vector<uint32_t> tris;
  const auto skirt = [&](uint32_t p, uint32_t q, uint32_t inner) {
    if (lipOf[p] > 0 || lipOf[q] > 0) return;  // the board's outer ring already drops its own lip
    const double dx = pos[q * 3] - pos[p * 3], dz = pos[q * 3 + 2] - pos[p * 3 + 2];
    // the face must look out into the void, away from the ground it hangs from
    if (dz * (pos[inner * 3] - pos[p * 3]) - dx * (pos[inner * 3 + 2] - pos[p * 3 + 2]) > 0) std::swap(p, q);
    const uint32_t base = static_cast<uint32_t>(lipOf.size());
    const std::pair<uint32_t, double> corners[4] = {{p, 0}, {q, 0}, {q, kEdgeLip}, {p, kEdgeLip}};
    for (const auto& [v, drop] : corners) {
      const double px = pos[v * 3], py = pos[v * 3 + 1] - drop, pz = pos[v * 3 + 2];
      pos.push_back(px);
      pos.push_back(py);
      pos.push_back(pz);
      for (uint32_t k = 0; k < stride; ++k) spl.push_back(spl[size_t(v) * stride + k]);
      lipOf.push_back(1);
    }
    tris.insert(tris.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };
  const auto clip = [&](const std::array<uint32_t, 3>& t) {
    const int n = inside[t[0]] + inside[t[1]] + inside[t[2]];
    if (n == 3) {
      tris.insert(tris.end(), {t[0], t[1], t[2]});
      return;
    }
    if (n == 0) return;
    std::vector<uint32_t> poly, edge;
    for (int k = 0; k < 3; k++) {
      const uint32_t p = t[k], q = t[(k + 1) % 3];
      if (inside[p]) poly.push_back(p);
      if (inside[p] != inside[q]) {
        const uint32_t c = inside[p] ? cut(p, q) : cut(q, p);
        poly.push_back(c);
        edge.push_back(c);
      }
    }
    for (size_t k = 1; k + 1 < poly.size(); k++) tris.insert(tris.end(), {poly[0], poly[k], poly[k + 1]});
    const uint32_t inner = inside[t[0]] ? t[0] : inside[t[1]] ? t[1] : t[2];
    skirt(edge[0], edge[1], inner);
  };
  for (int j = 0; j < VZ - 1; j++)
    for (int i = 0; i < VX - 1; i++) {
      const uint32_t a = static_cast<uint32_t>(j * VX + i), bb = a + 1, c = a + static_cast<uint32_t>(VX), d = c + 1;
      clip({a, c, bb});
      clip({bb, c, d});
    }
  GroundGeometry g;
  g.stride = stride;
  g.positions = toFloat(pos);
  g.weights = std::move(spl);
  g.indices = std::move(tris);
  g.normals = vertexNormals(g.positions, g.indices);
  return g;
}

void closeBoardSides(GroundGeometry& g, double base) {
  if (g.indices.empty() || g.stride == 0) return;
  // Every open edge of the ground (used by one triangle only: the board's cut edge, a void's rim) and the vertex across from it.
  struct Open {
    uint32_t p, q, inner;
    int uses;
  };
  std::unordered_map<uint64_t, Open> edges;
  for (size_t i = 0; i + 2 < g.indices.size(); i += 3)
    for (int k = 0; k < 3; ++k) {
      const uint32_t p = g.indices[i + k], q = g.indices[i + (k + 1) % 3], inner = g.indices[i + (k + 2) % 3];
      const uint64_t key = (uint64_t{std::min(p, q)} << 32) | std::max(p, q);
      auto [it, fresh] = edges.try_emplace(key, Open{p, q, inner, 0});
      ++it->second.uses;
    }
  const auto P = [&](uint32_t v, int j) { return static_cast<double>(g.positions[size_t(v) * 3 + j]); };
  for (const auto& [key, e] : edges) {
    if (e.uses != 1) continue;
    uint32_t p = e.p, q = e.q;
    if (P(p, 1) <= base + 1e-4 && P(q, 1) <= base + 1e-4) continue;
    // the face looks out into the air, away from the ground it hangs from (the void skirt's rule above)
    const double dx = P(q, 0) - P(p, 0), dz = P(q, 2) - P(p, 2);
    if (dz * (P(e.inner, 0) - P(p, 0)) - dx * (P(e.inner, 2) - P(p, 2)) > 0) std::swap(p, q);
    const uint32_t first = static_cast<uint32_t>(g.vertexCount());
    const double corners[4][3] = {{P(p, 0), P(p, 1), P(p, 2)}, {P(q, 0), P(q, 1), P(q, 2)}, {P(q, 0), base, P(q, 2)}, {P(p, 0), base, P(p, 2)}};
    const double u[3] = {corners[1][0] - corners[0][0], corners[1][1] - corners[0][1], corners[1][2] - corners[0][2]};
    const double w[3] = {corners[2][0] - corners[0][0], corners[2][1] - corners[0][1], corners[2][2] - corners[0][2]};
    double n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
    const double l = std::max(std::hypot(n[0], n[1], n[2]), 1e-12);
    for (const uint32_t from : {p, q, q, p}) {
      for (int j = 0; j < 3; ++j) g.normals.push_back(static_cast<float>(n[j] / l));
      const std::vector<float> own(g.weights.begin() + std::ptrdiff_t(from) * g.stride, g.weights.begin() + std::ptrdiff_t(from + 1) * g.stride);
      g.weights.insert(g.weights.end(), own.begin(), own.end());  // a copy first: inserting may move the vector
    }
    for (const auto& c : corners)
      for (int j = 0; j < 3; ++j) g.positions.push_back(static_cast<float>(c[j]));
    g.indices.insert(g.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
  }
}

GroundGeometry buildHardGround(const Board& b) {
  const HexLayout& L = b.layout();
  const uint32_t stride = strideOf(b);
  std::vector<double> pos, nor;
  std::vector<float> spl, tmp(stride);
  using P = std::array<double, 3>;
  const auto face = [&](const P& a, const P& bb, const P& c) {
    const double ux = bb[0] - a[0], uy = bb[1] - a[1], uz = bb[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
    const double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
    double l = std::hypot(nx, ny, nz);
    if (l == 0) l = 1;
    for (const P* p : {&a, &bb, &c}) {
      pos.insert(pos.end(), p->begin(), p->end());
      nor.insert(nor.end(), {nx / l, ny / l, nz / l});
      splat(b, (*p)[0], (*p)[2], 0.35, tmp.data(), nullptr);
      spl.insert(spl.end(), tmp.begin(), tmp.end());
    }
  };
  const auto quad = [&](const P& a0, const P& b0, const P& b1, const P& a1) {
    face(a0, b0, b1);
    face(a0, b1, a1);
  };
  const auto mid = [](const P& p, const P& q) { return P{(p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2}; };
  for (const Cell& cell : b.cells()) {
    const double y1 = cell.groundY;
    const P C{cell.x, y1, cell.z};
    std::vector<const Cell*> nbs;
    for (const auto& [nc, nr] : neighbors(L, cell.c, cell.r)) nbs.push_back(&b.cell(nc, nr));
    for (int k = 0; k < 6; k++) {
      const auto [ax, az] = corner(cell.x, cell.z, k);
      const auto [bx, bz] = corner(cell.x, cell.z, k + 1);
      const P A{ax, y1, az}, Bc{bx, y1, bz}, mA = mid(C, A), mB = mid(C, Bc), mAB = mid(A, Bc);
      face(C, mA, mB);
      face(mA, A, mAB);
      face(mA, mAB, mB);
      face(mB, mAB, Bc);
      const double na = ((k + 0.5) * kPi) / 3, cx = cell.x + kSqrt3 * std::sin(na), cz = cell.z + kSqrt3 * std::cos(na);
      const Cell* nb = nullptr;
      for (const Cell* o : nbs)
        if (std::abs(o->x - cx) < 0.1 && std::abs(o->z - cz) < 0.1) {
          nb = o;
          break;
        }
      const double y0 = nb ? nb->groundY : 0;
      if (y1 > y0) quad({ax, y0, az}, {bx, y0, bz}, Bc, A);
    }
  }
  GroundGeometry g;
  g.stride = stride;
  g.positions = toFloat(pos);
  g.normals = toFloat(nor);
  g.weights = std::move(spl);
  return g;
}

GroundGeometry buildTabletopGround(const Board& b) {
  constexpr double kTop = 0.93, kSide = 0.985, kBevel = 0.07;  // diorama.html buildHexTerrain(true)
  const uint32_t stride = strideOf(b);
  std::vector<double> pos, nor;
  std::vector<float> spl, tmp(stride);
  using P = std::array<double, 3>;
  const auto face = [&](const P& a, const P& bb, const P& c) {
    const double ux = bb[0] - a[0], uy = bb[1] - a[1], uz = bb[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
    const double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
    double l = std::hypot(nx, ny, nz);
    if (l == 0) l = 1;
    for (const P* p : {&a, &bb, &c}) {
      pos.insert(pos.end(), p->begin(), p->end());
      nor.insert(nor.end(), {nx / l, ny / l, nz / l});
      splat(b, (*p)[0], (*p)[2], 0.2, tmp.data(), nullptr);
      spl.insert(spl.end(), tmp.begin(), tmp.end());
    }
  };
  const auto quad = [&](const P& a0, const P& b0, const P& b1, const P& a1) {
    face(a0, b0, b1);
    face(a0, b1, a1);
  };
  const auto mid = [](const P& p, const P& q) { return P{(p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2}; };
  const auto at = [](const Cell& c, int k, double r, double y) {
    const auto [x, z] = corner(c.x, c.z, k, kHexRadius * r);
    return P{x, y, z};
  };
  double base = 0;  // the board's base: the hard look's outer walls end at 0 too
  for (const Cell& cell : b.cells()) base = std::min(base, cell.groundY);
  for (const Cell& cell : b.cells()) {
    const double y1 = cell.groundY, yBevel = y1 - kBevel;
    const P C{cell.x, y1, cell.z};
    for (int k = 0; k < 6; k++) {
      const P A = at(cell, k, kTop, y1), Bc = at(cell, k + 1, kTop, y1), mA = mid(C, A), mB = mid(C, Bc), mAB = mid(A, Bc);
      face(C, mA, mB);
      face(mA, A, mAB);
      face(mA, mAB, mB);
      face(mB, mAB, Bc);
      quad(at(cell, k, kSide, yBevel), at(cell, k + 1, kSide, yBevel), Bc, A);  // the bevel
      if (yBevel > base) quad(at(cell, k, kSide, base), at(cell, k + 1, kSide, base), at(cell, k + 1, kSide, yBevel), at(cell, k, kSide, yBevel));
    }
  }
  GroundGeometry g;
  g.stride = stride;
  g.positions = toFloat(pos);
  g.normals = toFloat(nor);
  g.weights = std::move(spl);
  return g;
}

double groundHeightAt(const Board& b, EdgeStyle style, double x, double z) {
  if (style != EdgeStyle::Smooth) return b.cellAt(x, z).groundY;  // hard and tabletop: the cell's own level
  const auto [wx, wz] = warp(x, z);
  return b.cellAt(wx, wz).groundY + b.reliefAt(x, z);
}

WaterGeometry buildWater(const Board& b, EdgeStyle style) {
  WaterGeometry g;
  if (!b.hasWater()) return g;
  const HexLayout& L = b.layout();
  const Board::Bounds bd = b.bounds();
  const double x0 = bd.x0, x1 = bd.x1, z0 = bd.z0, z1 = bd.z1;
  const double res = std::max(0.25, std::sqrt(((x1 - x0) * (z1 - z0)) / 60000));
  const int nx = static_cast<int>(std::ceil((x1 - x0) / res)), nz = static_cast<int>(std::ceil((z1 - z0) / res));
  const int VX = nx + 1, VZ = nz + 1;
  std::vector<float> pos(size_t(VX) * VZ * 3);
  // The sheet rises to meet the bank beside water (capped at that water's level) and sinks below all
  // the ground everywhere else.
  double lowestGround = std::numeric_limits<double>::infinity();
  for (const Cell& c : b.cells()) lowestGround = std::min(lowestGround, c.groundY);
  std::unordered_map<int, double> nearWater;
  for (const Cell& c : b.cells()) {
    double level = -std::numeric_limits<double>::infinity();
    if (c.water) level = std::max(level, c.waterY);
    for (const auto& [nc, nr] : neighbors(L, c.c, c.r)) {
      const Cell& o = b.cell(nc, nr);
      if (o.water) level = std::max(level, o.waterY);
    }
    if (level > -std::numeric_limits<double>::infinity()) nearWater[c.i] = level;
  }
  const auto sample = [&](double x, double z) -> const Cell& {
    if (style == EdgeStyle::Smooth) {
      const auto [sx, sz] = warp(x, z);
      return b.cellAt(sx, sz);
    }
    return b.cellAt(x, z);
  };
  for (int j = 0; j < VZ; j++)
    for (int i = 0; i < VX; i++) {
      const double x = std::min(x1, x0 + i * res), z = std::min(z1, z0 + j * res);
      const Cell& cell = sample(x, z);
      const size_t v = size_t(j) * VX + i;
      const auto shore = nearWater.find(cell.i);
      pos[v * 3] = static_cast<float>(x);
      pos[v * 3 + 2] = static_cast<float>(z);
      pos[v * 3 + 1] = static_cast<float>(cell.water ? cell.waterY
                                          : shore != nearWater.end() ? std::min(cell.groundY - 0.35, shore->second + 0.05)
                                                                     : lowestGround - 1);
    }
  for (int j = 0; j < nz; j++)
    for (int i = 0; i < nx; i++) {
      const uint32_t a = static_cast<uint32_t>(j * VX + i), bb = a + 1, c = a + static_cast<uint32_t>(VX), d = c + 1;
      const double x = (static_cast<double>(pos[a * 3]) + pos[d * 3]) / 2, z = (static_cast<double>(pos[a * 3 + 2]) + pos[d * 3 + 2]) / 2;
      const Cell& cell = sample(x, z);
      // only real water and the banks beside it: never a sheet under dry land
      if (cell.surface < 0 || (!cell.water && !nearWater.contains(cell.i))) continue;
      g.indices.insert(g.indices.end(), {a, c, bb, bb, c, d});
    }
  g.uvs.resize(size_t(VX) * VZ * 2);
  for (size_t v = 0; v < size_t(VX) * VZ; v++) {
    g.uvs[v * 2] = static_cast<float>(pos[v * 3] / 4.0);
    g.uvs[v * 2 + 1] = static_cast<float>(pos[v * 3 + 2] / 4.0);
  }
  g.normals = vertexNormals(pos, g.indices);
  g.positions = std::move(pos);
  return g;
}

}  // namespace ember
