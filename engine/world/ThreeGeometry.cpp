#include "world/ThreeGeometry.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <numbers>

namespace ember::three {

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kEps = std::numeric_limits<double>::epsilon();

// Matrix4 (column-major elements) helpers.
void identity(double m[16]) {
  for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0 : 0.0;
}
// Matrix4.set takes rows; elements are columns.
void setRows(double m[16], double n11, double n12, double n13, double n14, double n21, double n22, double n23, double n24, double n31, double n32,
             double n33, double n34) {
  m[0] = n11, m[4] = n12, m[8] = n13, m[12] = n14;
  m[1] = n21, m[5] = n22, m[9] = n23, m[13] = n24;
  m[2] = n31, m[6] = n32, m[10] = n33, m[14] = n34;
  m[3] = 0, m[7] = 0, m[11] = 0, m[15] = 1;
}

// Matrix3.getNormalMatrix(m): the 3x3 part inverted (Matrix3.invert) and transposed.
void normalMatrix(const double m[16], double out[9]) {
  const double n11 = m[0], n21 = m[1], n31 = m[2], n12 = m[4], n22 = m[5], n32 = m[6], n13 = m[8], n23 = m[9], n33 = m[10];
  const double t11 = n33 * n22 - n32 * n23, t12 = n32 * n13 - n33 * n12, t13 = n23 * n12 - n22 * n13;
  const double det = n11 * t11 + n21 * t12 + n31 * t13;
  double te[9];
  if (det == 0) {
    for (double& v : te) v = 0;
  } else {
    const double inv = 1 / det;
    te[0] = t11 * inv;
    te[1] = (n31 * n23 - n33 * n21) * inv;
    te[2] = (n32 * n21 - n31 * n22) * inv;
    te[3] = t12 * inv;
    te[4] = (n33 * n11 - n31 * n13) * inv;
    te[5] = (n31 * n12 - n32 * n11) * inv;
    te[6] = t13 * inv;
    te[7] = (n21 * n13 - n23 * n11) * inv;
    te[8] = (n22 * n11 - n21 * n12) * inv;
  }
  // transpose (column-major 3x3)
  out[0] = te[0], out[1] = te[3], out[2] = te[6];
  out[3] = te[1], out[4] = te[4], out[5] = te[7];
  out[6] = te[2], out[7] = te[5], out[8] = te[8];
}

void normalize(double& x, double& y, double& z) {
  double l = std::sqrt(x * x + y * y + z * z);
  if (l == 0) l = 1;
  x /= l, y /= l, z /= l;
}

}  // namespace

// --- BufferGeometry -----------------------------------------------------------------------------------------------------

Geometry& Geometry::applyMatrix(const double m[16]) {
  for (size_t i = 0; i < count(); ++i) {
    const double x = position[i * 3], y = position[i * 3 + 1], z = position[i * 3 + 2];
    const double w = 1 / (m[3] * x + m[7] * y + m[11] * z + m[15]);
    position[i * 3] = static_cast<float>((m[0] * x + m[4] * y + m[8] * z + m[12]) * w);
    position[i * 3 + 1] = static_cast<float>((m[1] * x + m[5] * y + m[9] * z + m[13]) * w);
    position[i * 3 + 2] = static_cast<float>((m[2] * x + m[6] * y + m[10] * z + m[14]) * w);
  }
  if (!normal.empty()) {
    double nm[9];
    normalMatrix(m, nm);
    for (size_t i = 0; i < normal.size() / 3; ++i) {
      const double x = normal[i * 3], y = normal[i * 3 + 1], z = normal[i * 3 + 2];
      double ox = nm[0] * x + nm[3] * y + nm[6] * z, oy = nm[1] * x + nm[4] * y + nm[7] * z, oz = nm[2] * x + nm[5] * y + nm[8] * z;
      normalize(ox, oy, oz);
      normal[i * 3] = static_cast<float>(ox), normal[i * 3 + 1] = static_cast<float>(oy), normal[i * 3 + 2] = static_cast<float>(oz);
    }
  }
  return *this;
}
Geometry& Geometry::rotateX(double a) {
  double m[16];
  const double c = std::cos(a), s = std::sin(a);
  setRows(m, 1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0);
  return applyMatrix(m);
}
Geometry& Geometry::rotateY(double a) {
  double m[16];
  const double c = std::cos(a), s = std::sin(a);
  setRows(m, c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0);
  return applyMatrix(m);
}
Geometry& Geometry::rotateZ(double a) {
  double m[16];
  const double c = std::cos(a), s = std::sin(a);
  setRows(m, c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0);
  return applyMatrix(m);
}
Geometry& Geometry::translate(double x, double y, double z) {
  double m[16];
  setRows(m, 1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z);
  return applyMatrix(m);
}
Geometry& Geometry::scale(double x, double y, double z) {
  double m[16];
  setRows(m, x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0);
  return applyMatrix(m);
}

Geometry Geometry::toNonIndexed() const {
  if (!indexed) return *this;
  Geometry g;
  for (uint32_t i : index) {
    g.position.insert(g.position.end(), position.begin() + i * 3, position.begin() + i * 3 + 3);
    if (!normal.empty()) g.normal.insert(g.normal.end(), normal.begin() + i * 3, normal.begin() + i * 3 + 3);
    if (!uv.empty()) g.uv.insert(g.uv.end(), uv.begin() + i * 2, uv.begin() + i * 2 + 2);
  }
  return g;
}

void Geometry::computeVertexNormals() {
  normal.assign(position.size(), 0.0f);
  auto face = [&](uint32_t a, uint32_t b, uint32_t c, bool add) {
    const double pax = position[a * 3], pay = position[a * 3 + 1], paz = position[a * 3 + 2];
    const double pbx = position[b * 3], pby = position[b * 3 + 1], pbz = position[b * 3 + 2];
    const double pcx = position[c * 3], pcy = position[c * 3 + 1], pcz = position[c * 3 + 2];
    const double cbx = pcx - pbx, cby = pcy - pby, cbz = pcz - pbz, abx = pax - pbx, aby = pay - pby, abz = paz - pbz;
    const double nx = cby * abz - cbz * aby, ny = cbz * abx - cbx * abz, nz = cbx * aby - cby * abx;
    for (uint32_t v : {a, b, c}) {
      if (add) {
        normal[v * 3] = static_cast<float>(normal[v * 3] + nx), normal[v * 3 + 1] = static_cast<float>(normal[v * 3 + 1] + ny),
                 normal[v * 3 + 2] = static_cast<float>(normal[v * 3 + 2] + nz);
      } else {
        normal[v * 3] = static_cast<float>(nx), normal[v * 3 + 1] = static_cast<float>(ny), normal[v * 3 + 2] = static_cast<float>(nz);
      }
    }
  };
  if (indexed)
    for (size_t i = 0; i + 2 < index.size(); i += 3) face(index[i], index[i + 1], index[i + 2], true);
  else
    for (uint32_t i = 0; i + 2 < count(); i += 3) face(i, i + 1, i + 2, false);
  for (size_t i = 0; i < count(); ++i) {
    double x = normal[i * 3], y = normal[i * 3 + 1], z = normal[i * 3 + 2];
    normalize(x, y, z);
    normal[i * 3] = static_cast<float>(x), normal[i * 3 + 1] = static_cast<float>(y), normal[i * 3 + 2] = static_cast<float>(z);
  }
}

Box3 Geometry::boundingBox() const {
  Box3 b;
  for (int k = 0; k < 3; ++k) b.min[k] = std::numeric_limits<double>::infinity(), b.max[k] = -std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < count(); ++i)
    for (int k = 0; k < 3; ++k) b.min[k] = std::min<double>(b.min[k], position[i * 3 + k]), b.max[k] = std::max<double>(b.max[k], position[i * 3 + k]);
  return b;
}

Geometry merge(const std::vector<Geometry>& parts) {
  Geometry g;
  g.indexed = !parts.empty() && parts[0].indexed;
  for (const Geometry& p : parts) {
    const uint32_t base = static_cast<uint32_t>(g.count());
    g.position.insert(g.position.end(), p.position.begin(), p.position.end());
    g.normal.insert(g.normal.end(), p.normal.begin(), p.normal.end());
    g.uv.insert(g.uv.end(), p.uv.begin(), p.uv.end());
    if (g.indexed)
      for (uint32_t i : p.index) g.index.push_back(i + base);
  }
  return g;
}

// --- primitives ---------------------------------------------------------------------------------------------------------

Geometry boxWithout(double width, double height, double depth, const std::vector<int>& omit) {
  Geometry g;
  g.indexed = true;
  uint32_t vertices = 0;
  int plane = 0;
  // buildPlane(u, v, w, udir, vdir, width, height, depth): one segment per side
  auto build = [&](int u, int v, int w, double udir, double vdir, double pw, double ph, double pd) {
    const double hw = pw / 2, hh = ph / 2, hd = pd / 2;
    for (int iy = 0; iy < 2; ++iy) {
      const double y = iy * ph - hh;
      for (int ix = 0; ix < 2; ++ix) {
        const double x = ix * pw - hw;
        double vec[3];
        vec[u] = x * udir, vec[v] = y * vdir, vec[w] = hd;
        for (double c : vec) g.position.push_back(static_cast<float>(c));
        double n[3];
        n[u] = 0, n[v] = 0, n[w] = pd > 0 ? 1 : -1;
        for (double c : n) g.normal.push_back(static_cast<float>(c));
        g.uv.push_back(static_cast<float>(ix)), g.uv.push_back(static_cast<float>(1 - iy));
      }
    }
    const uint32_t a = vertices, b = vertices + 2, c = vertices + 3, d = vertices + 1;
    if (std::find(omit.begin(), omit.end(), plane) == omit.end()) g.index.insert(g.index.end(), {a, b, d, b, c, d});
    vertices += 4;
    ++plane;
  };
  build(2, 1, 0, -1, -1, depth, height, width);   // px
  build(2, 1, 0, 1, -1, depth, height, -width);   // nx
  build(0, 2, 1, 1, 1, width, depth, height);     // py
  build(0, 2, 1, 1, -1, width, depth, -height);   // ny
  build(0, 1, 2, 1, -1, width, height, depth);    // pz
  build(0, 1, 2, -1, -1, width, height, -depth);  // nz
  return g;
}
Geometry box(double width, double height, double depth) { return boxWithout(width, height, depth, {}); }

Geometry sphere(double radius, int widthSegments, int heightSegments) {
  Geometry g;
  g.indexed = true;
  widthSegments = std::max(3, widthSegments), heightSegments = std::max(2, heightSegments);
  const double thetaStart = 0, thetaLength = kPi, phiStart = 0, phiLength = kPi * 2;
  const double thetaEnd = std::min(thetaStart + thetaLength, kPi);
  std::vector<std::vector<uint32_t>> grid;
  uint32_t index = 0;
  for (int iy = 0; iy <= heightSegments; ++iy) {
    std::vector<uint32_t> row;
    const double v = static_cast<double>(iy) / heightSegments;
    const double theta = thetaStart + v * thetaLength;
    const double y = radius * std::cos(theta);
    const double ring = std::sqrt(radius * radius - y * y);
    double uOffset = 0;
    if (iy == 0 && thetaStart == 0) uOffset = 0.5 / widthSegments;
    else if (iy == heightSegments && thetaEnd == kPi) uOffset = -0.5 / widthSegments;
    for (int ix = 0; ix <= widthSegments; ++ix) {
      const double u = static_cast<double>(ix) / widthSegments;
      const double phi = phiStart + u * phiLength;
      const double x = -ring * std::cos(phi), z = ring * std::sin(phi);
      g.position.insert(g.position.end(), {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
      double nx = x, ny = y, nz = z;
      normalize(nx, ny, nz);
      g.normal.insert(g.normal.end(), {static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)});
      g.uv.insert(g.uv.end(), {static_cast<float>(u + uOffset), static_cast<float>(1 - v)});
      row.push_back(index++);
    }
    grid.push_back(row);
  }
  for (int iy = 0; iy < heightSegments; ++iy)
    for (int ix = 0; ix < widthSegments; ++ix) {
      const uint32_t a = grid[iy][ix + 1], b = grid[iy][ix], c = grid[iy + 1][ix], d = grid[iy + 1][ix + 1];
      if (iy != 0 || thetaStart > 0) g.index.insert(g.index.end(), {a, b, d});
      if (iy != heightSegments - 1 || thetaEnd < kPi) g.index.insert(g.index.end(), {b, c, d});
    }
  return g;
}

Geometry torus(double radius, double tube, int radialSegments, int tubularSegments) {
  Geometry g;
  g.indexed = true;
  const double arc = kPi * 2, thetaStart = 0, thetaLength = kPi * 2;
  for (int j = 0; j <= radialSegments; ++j) {
    const double v = thetaStart + (static_cast<double>(j) / radialSegments) * thetaLength;
    for (int i = 0; i <= tubularSegments; ++i) {
      const double u = static_cast<double>(i) / tubularSegments * arc;
      const double x = (radius + tube * std::cos(v)) * std::cos(u), y = (radius + tube * std::cos(v)) * std::sin(u), z = tube * std::sin(v);
      g.position.insert(g.position.end(), {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
      double nx = x - radius * std::cos(u), ny = y - radius * std::sin(u), nz = z;
      normalize(nx, ny, nz);
      g.normal.insert(g.normal.end(), {static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)});
      g.uv.insert(g.uv.end(), {static_cast<float>(static_cast<double>(i) / tubularSegments), static_cast<float>(static_cast<double>(j) / radialSegments)});
    }
  }
  for (int j = 1; j <= radialSegments; ++j)
    for (int i = 1; i <= tubularSegments; ++i) {
      const uint32_t a = (tubularSegments + 1) * j + i - 1, b = (tubularSegments + 1) * (j - 1) + i - 1, c = (tubularSegments + 1) * (j - 1) + i,
                     d = (tubularSegments + 1) * j + i;
      g.index.insert(g.index.end(), {a, b, d, b, c, d});
    }
  return g;
}

// PolyhedronGeometry (positions; UVs and normals as three makes them for detail > 0: smooth = the position direction).
Geometry icosahedron(double radius, int detail) {
  const double t = (1 + std::sqrt(5.0)) / 2;
  const double v[] = {-1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, 0, 0, -1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, t, 0, -1, t, 0, 1, -t, 0, -1, -t, 0, 1};
  const int ix[] = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
                    3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
  struct V3 {
    double x, y, z;
  };
  auto lerp = [](V3 a, V3 b, double alpha) { return V3{a.x + (b.x - a.x) * alpha, a.y + (b.y - a.y) * alpha, a.z + (b.z - a.z) * alpha}; };
  std::vector<double> buf;
  auto push = [&](V3 p) { buf.insert(buf.end(), {p.x, p.y, p.z}); };
  for (size_t i = 0; i < std::size(ix); i += 3) {
    const V3 a{v[ix[i] * 3], v[ix[i] * 3 + 1], v[ix[i] * 3 + 2]}, b{v[ix[i + 1] * 3], v[ix[i + 1] * 3 + 1], v[ix[i + 1] * 3 + 2]},
             c{v[ix[i + 2] * 3], v[ix[i + 2] * 3 + 1], v[ix[i + 2] * 3 + 2]};
    const int cols = detail + 1;
    std::vector<std::vector<V3>> grid(static_cast<size_t>(cols + 1));
    for (int r = 0; r <= cols; ++r) {
      const V3 aj = lerp(a, c, static_cast<double>(r) / cols), bj = lerp(b, c, static_cast<double>(r) / cols);
      const int rows = cols - r;
      for (int j = 0; j <= rows; ++j) grid[static_cast<size_t>(r)].push_back(j == 0 && r == cols ? aj : lerp(aj, bj, static_cast<double>(j) / rows));
    }
    for (int r = 0; r < cols; ++r)
      for (int j = 0; j < 2 * (cols - r) - 1; ++j) {
        const int k = j / 2;
        if (j % 2 == 0) push(grid[r][k + 1]), push(grid[r + 1][k]), push(grid[r][k]);
        else push(grid[r][k + 1]), push(grid[r + 1][k + 1]), push(grid[r + 1][k]);
      }
  }
  Geometry g;
  for (size_t i = 0; i < buf.size(); i += 3) {
    double x = buf[i], y = buf[i + 1], z = buf[i + 2];
    normalize(x, y, z);
    x *= radius, y *= radius, z *= radius;
    g.position.insert(g.position.end(), {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
    double nx = x, ny = y, nz = z;
    normalize(nx, ny, nz);
    g.normal.insert(g.normal.end(), {static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)});
    g.uv.insert(g.uv.end(), {0.0f, 0.0f});  // three's spherical UVs: Ember's rocks overwrite them
  }
  if (detail == 0) g.computeVertexNormals();
  return g;
}

// --- paths and extrusion ------------------------------------------------------------------------------------------------

std::pair<double, double> Path::Curve::at(double t) const {
  if (!ellipse) {
    if (t == 1) return {x2, y2};
    return {(x2 - x1) * t + x1, (y2 - y1) * t + y1};
  }
  // EllipseCurve.getPoint (no rotation)
  const double twoPi = kPi * 2;
  double delta = a1 - a0;
  const bool same = std::abs(delta) < kEps;
  while (delta < 0) delta += twoPi;
  while (delta > twoPi) delta -= twoPi;
  if (delta < kEps) delta = same ? 0 : twoPi;
  if (clockwise && !same) delta = delta == twoPi ? -twoPi : delta - twoPi;
  const double angle = a0 + t * delta;
  return {ax + rx * std::cos(angle), ay + ry * std::sin(angle)};
}
Path& Path::moveTo(double x, double y) {
  cx_ = x, cy_ = y;
  return *this;
}
Path& Path::lineTo(double x, double y) {
  Curve c;
  c.x1 = cx_, c.y1 = cy_, c.x2 = x, c.y2 = y;
  curves_.push_back(c);
  cx_ = x, cy_ = y;
  return *this;
}
Path& Path::absellipse(double x, double y, double xRadius, double yRadius, double start, double end, bool clockwise) {
  Curve c;
  c.ellipse = true, c.ax = x, c.ay = y, c.rx = xRadius, c.ry = yRadius, c.a0 = start, c.a1 = end, c.clockwise = clockwise;
  if (!curves_.empty()) {
    const auto first = c.at(0);
    if (!(first.first == cx_ && first.second == cy_)) lineTo(first.first, first.second);
  }
  curves_.push_back(c);
  const auto last = c.at(1);
  cx_ = last.first, cy_ = last.second;
  return *this;
}
Path& Path::closePath() {
  if (curves_.empty()) return *this;
  const auto start = curves_.front().at(0), end = curves_.back().at(1);
  if (!(start == end)) {
    Curve c;
    c.x1 = end.first, c.y1 = end.second, c.x2 = start.first, c.y2 = start.second;
    curves_.push_back(c);
  }
  return *this;
}
std::vector<std::pair<double, double>> Path::points(int divisions) const {
  std::vector<std::pair<double, double>> out;
  for (const Curve& c : curves_) {
    const int resolution = c.ellipse ? divisions * 2 : 1;
    for (int d = 0; d <= resolution; ++d) {
      const auto p = c.at(static_cast<double>(d) / resolution);
      if (!out.empty() && out.back() == p) continue;
      out.push_back(p);
    }
  }
  return out;
}

namespace {
using Pt = std::pair<double, double>;
double shapeArea(const std::vector<Pt>& c) {
  double a = 0;
  for (size_t p = c.size() - 1, q = 0; q < c.size(); p = q++) a += c[p].first * c[q].second - c[q].first * c[p].second;
  return a * 0.5;
}
// ExtrudeGeometry's mergeOverlappingPoints (in place, comparing each point with the previous kept one, wrapping).
void mergeOverlapping(std::vector<Pt>& points) {
  if (points.empty()) return;
  const double th2 = 1e-10 * 1e-10;
  Pt prev = points[0];
  for (size_t i = 1; i <= points.size(); ++i) {
    const size_t current = i % points.size();
    const Pt cur = points[current];
    const double dx = cur.first - prev.first, dy = cur.second - prev.second;
    const double s = std::max({std::abs(cur.first), std::abs(cur.second), std::abs(prev.first), std::abs(prev.second)});
    if (dx * dx + dy * dy <= th2 * s * s) {
      points.erase(points.begin() + static_cast<long long>(current));
      --i;
      continue;
    }
    prev = cur;
  }
}
}  // namespace

Geometry extrude(const Shape& shape, double depth, int curveSegments) {
  std::vector<Pt> vertices = shape.points(curveSegments);
  std::vector<std::vector<Pt>> holes;
  for (const Path& h : shape.holes) holes.push_back(h.points(curveSegments));
  const bool reverse = !(shapeArea(vertices) < 0);
  if (reverse) {
    std::reverse(vertices.begin(), vertices.end());
    for (auto& h : holes)
      if (shapeArea(h) < 0) std::reverse(h.begin(), h.end());
  }
  mergeOverlapping(vertices);
  for (auto& h : holes) mergeOverlapping(h);
  std::vector<Pt> contour = vertices;
  for (const auto& h : holes) vertices.insert(vertices.end(), h.begin(), h.end());
  // ShapeUtils.triangulateShape (removeDupEndPts, earcut)
  auto dropEnd = [](std::vector<Pt>& p) {
    if (p.size() > 2 && p.back() == p.front()) p.pop_back();
  };
  std::vector<Pt> tc = contour;
  std::vector<std::vector<Pt>> th = holes;
  dropEnd(tc);
  std::vector<double> flat;
  for (const Pt& p : tc) flat.push_back(p.first), flat.push_back(p.second);
  std::vector<uint32_t> holeIndices;
  uint32_t holeIndex = static_cast<uint32_t>(tc.size());
  for (auto& h : th) {
    dropEnd(h);
    holeIndices.push_back(holeIndex);
    holeIndex += static_cast<uint32_t>(h.size());
    for (const Pt& p : h) flat.push_back(p.first), flat.push_back(p.second);
  }
  const std::vector<uint32_t> faces = earcut(flat, holeIndices);
  // placeholder: the bottom layer (z 0) then the top (z depth)
  const size_t vlen = vertices.size();
  std::vector<double> placeholder;
  for (const Pt& p : vertices) placeholder.insert(placeholder.end(), {p.first, p.second, 0.0});
  for (const Pt& p : vertices) placeholder.insert(placeholder.end(), {p.first, p.second, depth});
  Geometry g;
  auto add = [&](size_t i) {
    for (int k = 0; k < 3; ++k) g.position.push_back(static_cast<float>(placeholder[i * 3 + k]));
    g.uv.push_back(0.0f), g.uv.push_back(0.0f);
  };
  for (size_t i = 0; i + 2 < faces.size(); i += 3) add(faces[i + 2]), add(faces[i + 1]), add(faces[i]);
  for (size_t i = 0; i + 2 < faces.size(); i += 3) add(faces[i] + vlen), add(faces[i + 1] + vlen), add(faces[i + 2] + vlen);
  auto sidewalls = [&](size_t len, size_t layeroffset) {
    for (long long i = static_cast<long long>(len) - 1; i >= 0; --i) {
      const size_t j = static_cast<size_t>(i);
      const size_t k = i - 1 < 0 ? len - 1 : static_cast<size_t>(i - 1);
      const size_t a = layeroffset + j, b = layeroffset + k, c = layeroffset + k + vlen, d = layeroffset + j + vlen;
      add(a), add(b), add(d), add(b), add(c), add(d);
    }
  };
  size_t layer = 0;
  sidewalls(contour.size(), layer);
  layer += contour.size();
  for (const auto& h : holes) sidewalls(h.size(), layer), layer += h.size();
  g.computeVertexNormals();
  return g;
}

// --- earcut 3.0.2 -------------------------------------------------------------------------------------------------------

namespace {

struct Node {
  uint32_t i = 0;
  double x = 0, y = 0;
  Node *prev = nullptr, *next = nullptr;
  int32_t z = 0;
  Node *prevZ = nullptr, *nextZ = nullptr;
  bool steiner = false;
};

class Earcut {
 public:
  std::deque<Node> pool;
  std::vector<uint32_t> triangles;

  Node* createNode(uint32_t i, double x, double y) {
    pool.push_back(Node{});
    Node* n = &pool.back();
    n->i = i, n->x = x, n->y = y;
    return n;
  }
  Node* insertNode(uint32_t i, double x, double y, Node* last) {
    Node* p = createNode(i, x, y);
    if (!last) {
      p->prev = p, p->next = p;
    } else {
      p->next = last->next, p->prev = last;
      last->next->prev = p, last->next = p;
    }
    return p;
  }
  static void removeNode(Node* p) {
    p->next->prev = p->prev;
    p->prev->next = p->next;
    if (p->prevZ) p->prevZ->nextZ = p->nextZ;
    if (p->nextZ) p->nextZ->prevZ = p->prevZ;
  }
  static double signedArea(const std::vector<double>& d, size_t start, size_t end) {
    double sum = 0;
    for (size_t i = start, j = end - 2; i < end; i += 2) {
      sum += (d[j] - d[i]) * (d[i + 1] + d[j + 1]);
      j = i;
    }
    return sum;
  }
  static double area(const Node* p, const Node* q, const Node* r) { return (q->y - p->y) * (r->x - q->x) - (q->x - p->x) * (r->y - q->y); }
  static bool equals(const Node* a, const Node* b) { return a->x == b->x && a->y == b->y; }
  Node* linkedList(const std::vector<double>& d, size_t start, size_t end, bool clockwise) {
    Node* last = nullptr;
    if (clockwise == (signedArea(d, start, end) > 0)) {
      for (size_t i = start; i < end; i += 2) last = insertNode(static_cast<uint32_t>(i / 2), d[i], d[i + 1], last);
    } else {
      for (long long i = static_cast<long long>(end) - 2; i >= static_cast<long long>(start); i -= 2)
        last = insertNode(static_cast<uint32_t>(i / 2), d[static_cast<size_t>(i)], d[static_cast<size_t>(i) + 1], last);
    }
    if (last && equals(last, last->next)) {
      removeNode(last);
      last = last->next;
    }
    return last;
  }
  static Node* filterPoints(Node* start, Node* end = nullptr) {
    if (!start) return start;
    if (!end) end = start;
    Node* p = start;
    bool again;
    do {
      again = false;
      if (!p->steiner && (equals(p, p->next) || area(p->prev, p, p->next) == 0)) {
        removeNode(p);
        p = end = p->prev;
        if (p == p->next) break;
        again = true;
      } else {
        p = p->next;
      }
    } while (again || p != end);
    return end;
  }
  static bool pointInTriangle(double ax, double ay, double bx, double by, double cx, double cy, double px, double py) {
    return (cx - px) * (ay - py) >= (ax - px) * (cy - py) && (ax - px) * (by - py) >= (bx - px) * (ay - py) && (bx - px) * (cy - py) >= (cx - px) * (by - py);
  }
  static bool pointInTriangleExceptFirst(double ax, double ay, double bx, double by, double cx, double cy, double px, double py) {
    return !(ax == px && ay == py) && pointInTriangle(ax, ay, bx, by, cx, cy, px, py);
  }
  static int32_t zOrder(double x0, double y0, double minX, double minY, double invSize) {
    int32_t x = static_cast<int32_t>((x0 - minX) * invSize), y = static_cast<int32_t>((y0 - minY) * invSize);
    x = (x | (x << 8)) & 0x00FF00FF, x = (x | (x << 4)) & 0x0F0F0F0F, x = (x | (x << 2)) & 0x33333333, x = (x | (x << 1)) & 0x55555555;
    y = (y | (y << 8)) & 0x00FF00FF, y = (y | (y << 4)) & 0x0F0F0F0F, y = (y | (y << 2)) & 0x33333333, y = (y | (y << 1)) & 0x55555555;
    return x | (y << 1);
  }
  static bool isEar(Node* ear) {
    const Node *a = ear->prev, *b = ear, *c = ear->next;
    if (area(a, b, c) >= 0) return false;
    const double ax = a->x, bx = b->x, cx = c->x, ay = a->y, by = b->y, cy = c->y;
    const double x0 = std::min({ax, bx, cx}), y0 = std::min({ay, by, cy}), x1 = std::max({ax, bx, cx}), y1 = std::max({ay, by, cy});
    const Node* p = c->next;
    while (p != a) {
      if (p->x >= x0 && p->x <= x1 && p->y >= y0 && p->y <= y1 && pointInTriangleExceptFirst(ax, ay, bx, by, cx, cy, p->x, p->y) && area(p->prev, p, p->next) >= 0)
        return false;
      p = p->next;
    }
    return true;
  }
  static bool isEarHashed(Node* ear, double minX, double minY, double invSize) {
    const Node *a = ear->prev, *b = ear, *c = ear->next;
    if (area(a, b, c) >= 0) return false;
    const double ax = a->x, bx = b->x, cx = c->x, ay = a->y, by = b->y, cy = c->y;
    const double x0 = std::min({ax, bx, cx}), y0 = std::min({ay, by, cy}), x1 = std::max({ax, bx, cx}), y1 = std::max({ay, by, cy});
    const int32_t minZ = zOrder(x0, y0, minX, minY, invSize), maxZ = zOrder(x1, y1, minX, minY, invSize);
    const Node *p = ear->prevZ, *n = ear->nextZ;
    auto blocks = [&](const Node* q) {
      return q->x >= x0 && q->x <= x1 && q->y >= y0 && q->y <= y1 && q != a && q != c && pointInTriangleExceptFirst(ax, ay, bx, by, cx, cy, q->x, q->y) &&
             area(q->prev, q, q->next) >= 0;
    };
    while (p && p->z >= minZ && n && n->z <= maxZ) {
      if (blocks(p)) return false;
      p = p->prevZ;
      if (blocks(n)) return false;
      n = n->nextZ;
    }
    while (p && p->z >= minZ) {
      if (blocks(p)) return false;
      p = p->prevZ;
    }
    while (n && n->z <= maxZ) {
      if (blocks(n)) return false;
      n = n->nextZ;
    }
    return true;
  }
  static int sign(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
  static bool onSegment(const Node* p, const Node* q, const Node* r) {
    return q->x <= std::max(p->x, r->x) && q->x >= std::min(p->x, r->x) && q->y <= std::max(p->y, r->y) && q->y >= std::min(p->y, r->y);
  }
  static bool intersects(const Node* p1, const Node* q1, const Node* p2, const Node* q2) {
    const int o1 = sign(area(p1, q1, p2)), o2 = sign(area(p1, q1, q2)), o3 = sign(area(p2, q2, p1)), o4 = sign(area(p2, q2, q1));
    if (o1 != o2 && o3 != o4) return true;
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;
    if (o4 == 0 && onSegment(p2, q1, q2)) return true;
    return false;
  }
  static bool intersectsPolygon(const Node* a, const Node* b) {
    const Node* p = a;
    do {
      if (p->i != a->i && p->next->i != a->i && p->i != b->i && p->next->i != b->i && intersects(p, p->next, a, b)) return true;
      p = p->next;
    } while (p != a);
    return false;
  }
  static bool locallyInside(const Node* a, const Node* b) {
    return area(a->prev, a, a->next) < 0 ? area(a, b, a->next) >= 0 && area(a, a->prev, b) >= 0 : area(a, b, a->prev) < 0 || area(a, a->next, b) < 0;
  }
  static bool middleInside(const Node* a, const Node* b) {
    const Node* p = a;
    bool inside = false;
    const double px = (a->x + b->x) / 2, py = (a->y + b->y) / 2;
    do {
      if (((p->y > py) != (p->next->y > py)) && p->next->y != p->y && (px < (p->next->x - p->x) * (py - p->y) / (p->next->y - p->y) + p->x)) inside = !inside;
      p = p->next;
    } while (p != a);
    return inside;
  }
  static bool isValidDiagonal(const Node* a, const Node* b) {
    return a->next->i != b->i && a->prev->i != b->i && !intersectsPolygon(a, b) &&
           ((locallyInside(a, b) && locallyInside(b, a) && middleInside(a, b) && (area(a->prev, a, b->prev) != 0 || area(a, b->prev, b) != 0)) ||
            (equals(a, b) && area(a->prev, a, a->next) > 0 && area(b->prev, b, b->next) > 0));
  }
  Node* splitPolygon(Node* a, Node* b) {
    Node* a2 = createNode(a->i, a->x, a->y);
    Node* b2 = createNode(b->i, b->x, b->y);
    Node* an = a->next;
    Node* bp = b->prev;
    a->next = b, b->prev = a;
    a2->next = an, an->prev = a2;
    b2->next = a2, a2->prev = b2;
    bp->next = b2, b2->prev = bp;
    return b2;
  }
  Node* cureLocalIntersections(Node* start) {
    Node* p = start;
    do {
      Node *a = p->prev, *b = p->next->next;
      if (!equals(a, b) && intersects(a, p, p->next, b) && locallyInside(a, b) && locallyInside(b, a)) {
        triangles.insert(triangles.end(), {a->i, p->i, b->i});
        removeNode(p);
        removeNode(p->next);
        p = start = b;
      }
      p = p->next;
    } while (p != start);
    return filterPoints(p);
  }
  void splitEarcut(Node* start, double minX, double minY, double invSize) {
    Node* a = start;
    do {
      Node* b = a->next->next;
      while (b != a->prev) {
        if (a->i != b->i && isValidDiagonal(a, b)) {
          Node* c = splitPolygon(a, b);
          a = filterPoints(a, a->next);
          c = filterPoints(c, c->next);
          earcutLinked(a, minX, minY, invSize, 0);
          earcutLinked(c, minX, minY, invSize, 0);
          return;
        }
        b = b->next;
      }
      a = a->next;
    } while (a != start);
  }
  static Node* sortLinked(Node* list) {
    int numMerges;
    int inSize = 1;
    do {
      Node* p = list;
      Node* e = nullptr;
      list = nullptr;
      Node* tail = nullptr;
      numMerges = 0;
      while (p) {
        ++numMerges;
        Node* q = p;
        int pSize = 0;
        for (int i = 0; i < inSize; ++i) {
          ++pSize;
          q = q->nextZ;
          if (!q) break;
        }
        int qSize = inSize;
        while (pSize > 0 || (qSize > 0 && q)) {
          if (pSize != 0 && (qSize == 0 || !q || p->z <= q->z)) {
            e = p, p = p->nextZ, --pSize;
          } else {
            e = q, q = q->nextZ, --qSize;
          }
          if (tail) tail->nextZ = e;
          else list = e;
          e->prevZ = tail;
          tail = e;
        }
        p = q;
      }
      tail->nextZ = nullptr;
      inSize *= 2;
    } while (numMerges > 1);
    return list;
  }
  static void indexCurve(Node* start, double minX, double minY, double invSize) {
    Node* p = start;
    do {
      if (p->z == 0) p->z = zOrder(p->x, p->y, minX, minY, invSize);
      p->prevZ = p->prev, p->nextZ = p->next;
      p = p->next;
    } while (p != start);
    p->prevZ->nextZ = nullptr;
    p->prevZ = nullptr;
    sortLinked(p);
  }
  void earcutLinked(Node* ear, double minX, double minY, double invSize, int pass) {
    if (!ear) return;
    if (!pass && invSize != 0) indexCurve(ear, minX, minY, invSize);
    Node* stop = ear;
    while (ear->prev != ear->next) {
      Node *prev = ear->prev, *next = ear->next;
      if (invSize != 0 ? isEarHashed(ear, minX, minY, invSize) : isEar(ear)) {
        triangles.insert(triangles.end(), {prev->i, ear->i, next->i});
        removeNode(ear);
        ear = next->next;
        stop = next->next;
        continue;
      }
      ear = next;
      if (ear == stop) {
        if (!pass) {
          earcutLinked(filterPoints(ear), minX, minY, invSize, 1);
        } else if (pass == 1) {
          ear = cureLocalIntersections(filterPoints(ear));
          earcutLinked(ear, minX, minY, invSize, 2);
        } else if (pass == 2) {
          splitEarcut(ear, minX, minY, invSize);
        }
        break;
      }
    }
  }
  static Node* getLeftmost(Node* start) {
    Node *p = start, *leftmost = start;
    do {
      if (p->x < leftmost->x || (p->x == leftmost->x && p->y < leftmost->y)) leftmost = p;
      p = p->next;
    } while (p != start);
    return leftmost;
  }
  static bool sectorContainsSector(const Node* m, const Node* p) { return area(m->prev, m, p->prev) < 0 && area(p->next, m, m->next) < 0; }
  static Node* findHoleBridge(Node* hole, Node* outerNode) {
    Node* p = outerNode;
    const double hx = hole->x, hy = hole->y;
    double qx = -std::numeric_limits<double>::infinity();
    Node* m = nullptr;
    if (equals(hole, p)) return p;
    do {
      if (equals(hole, p->next)) return p->next;
      if (hy <= p->y && hy >= p->next->y && p->next->y != p->y) {
        const double x = p->x + (hy - p->y) * (p->next->x - p->x) / (p->next->y - p->y);
        if (x <= hx && x > qx) {
          qx = x;
          m = p->x < p->next->x ? p : p->next;
          if (x == hx) return m;
        }
      }
      p = p->next;
    } while (p != outerNode);
    if (!m) return nullptr;
    Node* stop = m;
    const double mx = m->x, my = m->y;
    double tanMin = std::numeric_limits<double>::infinity();
    p = m;
    do {
      if (hx >= p->x && p->x >= mx && hx != p->x && pointInTriangle(hy < my ? hx : qx, hy, mx, my, hy < my ? qx : hx, hy, p->x, p->y)) {
        const double tan = std::abs(hy - p->y) / (hx - p->x);
        if (locallyInside(p, hole) && (tan < tanMin || (tan == tanMin && (p->x > m->x || (p->x == m->x && sectorContainsSector(m, p)))))) {
          m = p;
          tanMin = tan;
        }
      }
      p = p->next;
    } while (p != stop);
    return m;
  }
  Node* eliminateHole(Node* hole, Node* outerNode) {
    Node* bridge = findHoleBridge(hole, outerNode);
    if (!bridge) return outerNode;
    Node* bridgeReverse = splitPolygon(bridge, hole);
    filterPoints(bridgeReverse, bridgeReverse->next);
    return filterPoints(bridge, bridge->next);
  }
  Node* eliminateHoles(const std::vector<double>& d, const std::vector<uint32_t>& holes, Node* outerNode) {
    std::vector<Node*> queue;
    for (size_t i = 0; i < holes.size(); ++i) {
      const size_t start = holes[i] * 2, end = i + 1 < holes.size() ? holes[i + 1] * 2 : d.size();
      Node* list = linkedList(d, start, end, false);
      if (list == list->next) list->steiner = true;
      queue.push_back(getLeftmost(list));
    }
    // queue.sort(compareXYSlope): V8's sort is stable; the comparator is a total preorder for distinct holes
    std::stable_sort(queue.begin(), queue.end(), [](const Node* a, const Node* b) {
      double r = a->x - b->x;
      if (r == 0) {
        r = a->y - b->y;
        if (r == 0) r = (a->next->y - a->y) / (a->next->x - a->x) - (b->next->y - b->y) / (b->next->x - b->x);
      }
      return r < 0;
    });
    for (Node* h : queue) outerNode = eliminateHole(h, outerNode);
    return outerNode;
  }
};

}  // namespace

std::vector<uint32_t> earcut(const std::vector<double>& data, const std::vector<uint32_t>& holeIndices) {
  Earcut e;
  const bool hasHoles = !holeIndices.empty();
  const size_t outerLen = hasHoles ? holeIndices[0] * 2 : data.size();
  Node* outer = e.linkedList(data, 0, outerLen, true);
  if (!outer || outer->next == outer->prev) return {};
  double minX = 0, minY = 0, invSize = 0;
  if (hasHoles) outer = e.eliminateHoles(data, holeIndices, outer);
  if (data.size() > 80 * 2) {
    minX = data[0], minY = data[1];
    double maxX = minX, maxY = minY;
    for (size_t i = 2; i < outerLen; i += 2) {
      minX = std::min(minX, data[i]), minY = std::min(minY, data[i + 1]);
      maxX = std::max(maxX, data[i]), maxY = std::max(maxY, data[i + 1]);
    }
    invSize = std::max(maxX - minX, maxY - minY);
    invSize = invSize != 0 ? 32767 / invSize : 0;
  }
  e.earcutLinked(outer, minX, minY, invSize, 0);
  return e.triangles;
}

}  // namespace ember::three
