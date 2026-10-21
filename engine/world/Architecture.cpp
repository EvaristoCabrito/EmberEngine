#include "world/Architecture.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <numbers>

#include <nlohmann/json.hpp>

#include "core/Log.h"

namespace ember::e2 {

using three::Geometry;
using three::Shape;

namespace {

constexpr double kPi = std::numbers::pi;
const double kSqrt3 = std::sqrt(3.0);
using Conn = std::pair<double, double>;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

// The arched frame Ember extrudes for doorways, doors, gates and niches: a block of `width` x `height` with an opening
// `radius` wide whose top is a half ellipse from `spring` rising `rise`.
Shape archFrame(double width, double height, double radius, double spring, double rise) {
  Shape s;
  s.moveTo(-width / 2, 0);
  s.lineTo(-width / 2, height);
  s.lineTo(width / 2, height);
  s.lineTo(width / 2, 0);
  s.lineTo(radius, 0);
  s.lineTo(radius, spring);
  s.absellipse(0, spring, radius, rise, 0, kPi, false);
  s.lineTo(-radius, 0);
  s.closePath();
  return s;
}
// new ExtrudeGeometry(shape, {depth, bevelEnabled: false, curveSegments}).rotateX(PI / 2).translate(0, depth / 2, 0)
Geometry extruded(const Shape& shape, double depth, int curveSegments) {
  Geometry g = three::extrude(shape, depth, curveSegments);
  g.rotateX(kPi / 2).translate(0, depth / 2, 0);
  return g;
}

// --- ThreeCastle.ts ---------------------------------------------------------------------------------------------------

Geometry castleGeometry(const ArchitectureDef& def, double tile, double height, int rotation, const std::vector<Conn>& connections) {
  std::vector<Geometry> parts;
  const double depth = tile * 0.42 * def.wallThicknessScale;
  const double width = tile * ((rotation & 1) ? 1.5 : kSqrt3);
  const bool gate = def.castleStyle == "gate", ruined = def.castleStyle == "ruined";
  const double rect[4] = {def.thickWall ? 405.0 / 1168 : gate ? 210.0 / 1168 : ruined ? 55.0 / 1168 : 45.0 / 1712,
                          def.thickWall ? 228.0 / 784 : gate ? 190.0 / 784 : ruined ? 255.0 / 784 : 360.0 / 1152,
                          def.thickWall ? 550.0 / 1168 : gate ? 365.0 / 1168 : ruined ? 660.0 / 1168 : 1665.0 / 1712,
                          def.thickWall ? 354.0 / 784 : gate ? 640.0 / 784 : ruined ? 600.0 / 784 : 915.0 / 1152};
  auto texture = [&](Geometry part, bool frontGate = false) {
    const three::Box3 b = part.boundingBox();
    for (size_t i = 0; i < part.count(); ++i) {
      const bool front = std::abs(part.ny(i)) > 0.5;
      const bool axisX = front || std::abs(part.nz(i)) > 0.5;
      const double lo = axisX ? b.min[0] : b.min[1], span = (axisX ? b.max[0] : b.max[1]) - lo;
      const double u = ((axisX ? part.px(i) : part.py(i)) - lo) / (span != 0 ? span : 1);
      const double v = std::abs(part.nz(i)) > 0.5 ? (part.py(i) - b.min[1]) / ((b.max[1] - b.min[1]) != 0 ? b.max[1] - b.min[1] : 1)
                                                   : (part.pz(i) - b.min[2]) / ((b.max[2] - b.min[2]) != 0 ? b.max[2] - b.min[2] : 1);
      if (frontGate && front) part.setUV(i, (197 + (part.px(i) / width + 0.5) * 775) / 1168, 1 - (708 - part.pz(i) / height * 588) / 784);
      else part.setUV(i, rect[0] + u * (rect[2] - rect[0]), 1 - rect[3] + v * (rect[3] - rect[1]));
    }
    parts.push_back(part.toNonIndexed());
  };
  auto boxAt = [&](double w, double d, double h, double x, double y, double z, double angle = 0) {
    texture(three::box(w, d, h));
    parts.back().rotateZ(angle).translate(x, y, z);
  };
  const double merlonHeight = height * 0.16;
  auto battlements = [&](double length, double x, double y, double angle) {
    const int count = std::max(1, static_cast<int>(std::floor(length / (tile * 0.42) + 0.5)));
    const double spacing = length / count;
    for (int i = 0; i < count; ++i) {
      const double offset = -length / 2 + spacing * (i + 0.5);
      boxAt(spacing * 0.56, depth * 1.08, merlonHeight, x + std::cos(angle) * offset, y + std::sin(angle) * offset, height + merlonHeight / 2, angle);
    }
  };
  auto thickBody = [&](double length, double x, double y, double angle) {
    const double slit = std::min(tile * 0.09, length * 0.12), sideWidth = (length - slit) / 2;
    for (int sign : {-1, 1}) {
      const double offset = sign * (slit + sideWidth) / 2;
      boxAt(sideWidth, depth, height, x + std::cos(angle) * offset, y + std::sin(angle) * offset, height / 2, angle);
    }
    boxAt(slit, depth, height * 0.34, x, y, height * 0.17, angle);
    boxAt(slit, depth, height * 0.32, x, y, height * 0.84, angle);
    boxAt(length, depth * 1.18, height * 0.09, x, y, height * 0.045, angle);
    battlements(length, x, y, angle);
  };
  if (def.castleStyle == "tower") {
    const double towerWidth = tile * kSqrt3 + tile * 0.01, towerDepth = tile * 1.5 + tile * 0.01;
    for (int side = 0; side < 4; ++side) {
      const double angle = side * kPi / 2;
      const bool horizontal = side % 2 == 0;
      thickBody(horizontal ? towerWidth : towerDepth, -std::sin(angle) * (towerWidth - depth) / 2, std::cos(angle) * (towerDepth - depth) / 2, angle);
    }
  } else if (gate) {
    const double radius = width * 0.195, spring = height * 0.53, rise = height * 0.245;
    texture(extruded(archFrame(width, height, radius, spring, rise), depth, 24), true);
    if (def.model3d == "door") {
      Shape leaf;
      leaf.moveTo(-radius, 0);
      leaf.lineTo(radius, 0);
      leaf.lineTo(radius, spring);
      leaf.absellipse(0, spring, radius, rise, 0, kPi, false);
      leaf.lineTo(-radius, 0);
      leaf.closePath();
      texture(extruded(leaf, tile * 0.1, 24), true);
    }
    battlements(width, 0, 0, 0);
  } else if (ruined) {
    if (def.thickWall) boxAt(width, depth * 1.18, height * 0.09, 0, 0, height * 0.045);
    const int courses = 7;
    const double step = height / courses;
    for (int row = 0; row < courses; ++row) {
      const double length = width * (1 - row * 0.055);
      boxAt(length, depth, step, -width / 2 + length / 2, 0, step * (row + 0.5));
    }
    battlements(width * 0.55, -width * 0.225, 0, 0);
    for (int i = 0; i < 9; ++i)
      boxAt(tile * 0.15, tile * 0.17, step * 0.55, width * (0.18 + (i % 3) * 0.12), ((i % 2) ? 1 : -1) * depth * 0.6, step * (0.28 + (i / 3) * 0.2), i * 0.73);
  } else if (!connections.empty()) {
    boxAt(depth, depth, height, 0, 0, height / 2);
    for (const auto& [cx, cy] : connections) {
      const double angle = std::atan2(cy, cx), length = std::hypot(cx, cy) / 2 + tile * 0.01;
      if (def.thickWall) {
        thickBody(length, cx / 4, cy / 4, angle);
      } else {
        boxAt(length, depth, height, cx / 4, cy / 4, height / 2, angle);
        battlements(length, cx / 4, cy / 4, angle);
      }
    }
  } else if (def.thickWall) {
    thickBody(width, 0, 0, 0);
  } else {
    boxAt(width, depth, height, 0, 0, height / 2);
    battlements(width, 0, 0, 0);
  }
  Geometry g = three::merge(parts);
  if (def.castleStyle != "tower" && (gate || ruined || connections.empty())) g.rotateZ(-rotation * kPi / 2);
  return g;
}

// --- ThreeTemple.ts ---------------------------------------------------------------------------------------------------

Geometry templeGeometry(const ArchitectureDef& def, double tile, double height, int rotation, const std::vector<Conn>& connections) {
  std::vector<Geometry> parts;
  const double depth = tile * 0.42;
  const bool relief = def.templeStyle == "relief";
  const double crop[4] = {relief ? 130.0 / 1168 : 190.0 / 1712, relief ? 88.0 / 784 : 125.0 / 1152, relief ? 1037.0 / 1168 : 1520.0 / 1712,
                          relief ? 686.0 / 784 : 1014.0 / 1152};
  auto segment = [&](double width, double x, double y, double angle, bool niche) {
    auto add = [&](Geometry part) {
      for (size_t i = 0; i < part.count(); ++i) {
        const bool front = std::abs(part.ny(i)) > 0.5, top = std::abs(part.nz(i)) > 0.5;
        const double u = top ? 0.02 + clamp01(part.px(i) / width + 0.5) * 0.16 : front ? part.px(i) / width + 0.5 : 0.04 + (part.py(i) / depth + 0.5) * 0.1;
        const double v = top ? 0.18 + clamp01(part.py(i) / depth + 0.5) * 0.5 : part.pz(i) / height;
        part.setUV(i, crop[0] + clamp01(u) * (crop[2] - crop[0]), 1 - crop[3] + clamp01(v) * (crop[3] - crop[1]));
      }
      Geometry c = part.toNonIndexed();
      c.rotateZ(angle).translate(x, y, 0);
      parts.push_back(std::move(c));
    };
    auto boxAt = [&](double w, double d, double h, double cx, double cy, double z) {
      Geometry b = three::box(w, d, h);
      b.translate(cx, cy, z);
      add(std::move(b));
    };
    if (niche) {
      const double radius = width * 0.22, spring = height * 0.5, rise = height * 0.29;
      add(extruded(archFrame(width, height, radius, spring, rise), depth, 24));
      boxAt(width, depth * 0.3, height, 0, 0, height / 2);
      for (int side : {-1, 1})
        for (int sign : {-1, 1}) {
          boxAt(width * 0.073, depth * 0.12, height * 0.39, sign * width * 0.26, side * depth * 0.51, height * 0.3);
          boxAt(width * 0.082, depth * 0.15, height * 0.052, sign * width * 0.26, side * depth * 0.51, height * 0.5);
        }
    } else {
      boxAt(width, depth, height, 0, 0, height / 2);
    }
    boxAt(width, depth * 1.13, height * 0.063, 0, 0, height * 0.9685);
    boxAt(width, depth * 1.08, height * 0.1, 0, 0, height * 0.05);
  };
  if (def.templeStyle == "plain" && !connections.empty()) {
    segment(depth, 0, 0, 0, false);
    for (const auto& [cx, cy] : connections) segment(std::hypot(cx, cy) / 2 + tile * 0.01, cx / 4, cy / 4, std::atan2(cy, cx), false);
  } else {
    segment(tile * ((rotation & 1) ? 1.5 : kSqrt3), 0, 0, -rotation * kPi / 2, def.templeStyle != "plain");
  }
  return three::merge(parts);
}

// --- ThreeRocks.ts ------------------------------------------------------------------------------------------------------

Geometry rockGeometry(const ArchitectureDef& def, double tile, double height, int rotation) {
  std::vector<Geometry> parts;
  int stoneIndex = 0;
  auto stone = [&](double x, double y, double z, double w, double d, double h, double angle = 0) {
    Geometry g = three::icosahedron(1, 1);
    const int seed = ++stoneIndex;
    for (size_t i = 0; i < g.count(); ++i) {
      const double px = g.px(i), py = g.py(i), pz = g.pz(i);
      const double rough = 1 + 0.11 * std::sin(px * 17 + py * 23 + pz * 13 + seed * 3.1);
      g.position[i * 3] = static_cast<float>(px * rough), g.position[i * 3 + 1] = static_cast<float>(py * rough), g.position[i * 3 + 2] = static_cast<float>(pz * rough);
      g.setUV(i, (242 + (px + 1) * 50) / 1168, 1 - (293 + (pz + 1) * 16) / 784);
    }
    g.scale(w / 2, d / 2, h / 2).rotateY(angle).translate(x, y, z);
    g.computeVertexNormals();
    parts.push_back(std::move(g));
  };
  if (def.rockStyle == "arch") {
    const double spring = height * 0.48;
    for (int side : {-1, 1})
      for (int row = 0; row < 4; ++row) stone(side * tile * 0.56, 0, height * (0.075 + row * 0.12), tile * 0.45, tile * 0.55, height * 0.22);
    for (int i = 0; i <= 10; ++i) {
      const double angle = i / 10.0 * kPi;
      stone(std::cos(angle) * tile * 0.56, 0, spring + std::sin(angle) * height * 0.36, tile * 0.35, tile * 0.52, height * 0.25, -angle);
    }
  } else if (def.rockStyle == "layered") {
    for (int row = 0; row < 5; ++row)
      for (int i = 0; i < 3; ++i)
        stone((i - 1) * tile * 0.48 + std::sin(row * 2) * tile * 0.05, std::cos(row) * tile * 0.04, height * (0.1 + row * 0.185), tile * 0.66,
              tile * (0.65 - row * 0.04), height * 0.24, std::sin(i + row) * 0.06);
  } else {
    for (int i = 0; i < 5; ++i) stone((i - 2) * tile * 0.28, tile * 0.1, height * 0.48, tile * 0.38, tile * 0.45, height * (0.86 + std::sin(i * 4) * 0.1));
    for (int i = 0; i < 7; ++i)
      stone((i % 3 - 1) * tile * 0.38, -tile * (0.16 + (i / 3) * 0.16), height * 0.13, tile * 0.5, tile * 0.38, height * 0.22, i * 0.19);
    stone(0, -tile * 0.24, height * 0.29, tile * 1.1, tile * 0.6, height * 0.19, -0.25);
  }
  Geometry g = three::merge(parts);
  g.rotateZ(-rotation * kPi / 2);
  return g;
}

// --- ThreeReferenceDoors.ts ---------------------------------------------------------------------------------------------

Geometry referenceDoorGeometry(const ArchitectureDef& def, double tile, double height, int rotation) {
  const bool thick = def.doorStyle == "dungeonOak";
  const double width = tile * ((rotation & 1) ? 1.5 : kSqrt3) * def.architectureSpan.value_or(1);
  const double depth = tile * (thick ? 0.752 : 0.32);
  std::vector<Geometry> parts;
  const double stone[4] = {thick ? 537.0 : 776.0, thick ? 265.0 : 224.0, thick ? 575.0 : 850.0, thick ? 370.0 : 358.0};
  const double wood[4] = {thick ? 748.0 : 587.0, thick ? 292.0 : 323.0, thick ? 806.0 : 659.0, thick ? 458.0 : 435.0};
  const double metal[4] = {thick ? 761.0 : 589.0, thick ? 267.0 : 300.0, thick ? 780.0 : 613.0, thick ? 276.0 : 307.0};
  auto add = [&](Geometry g, const double* rect) {
    const three::Box3 b = g.boundingBox();
    for (size_t i = 0; i < g.count(); ++i) {
      const bool useX = std::abs(g.nx(i)) < 0.5;
      const double lo = useX ? b.min[0] : b.min[1], hi = useX ? b.max[0] : b.max[1];
      const double u = ((useX ? g.px(i) : g.py(i)) - lo) / ((hi - lo) != 0 ? hi - lo : 1);
      const double v = std::abs(g.nz(i)) > 0.5 ? (g.py(i) - b.min[1]) / ((b.max[1] - b.min[1]) != 0 ? b.max[1] - b.min[1] : 1)
                                               : (g.pz(i) - b.min[2]) / ((b.max[2] - b.min[2]) != 0 ? b.max[2] - b.min[2] : 1);
      g.setUV(i, (rect[0] + u * (rect[2] - rect[0])) / 1168, 1 - (rect[3] - v * (rect[3] - rect[1])) / 784);
    }
    parts.push_back(g.toNonIndexed());
  };
  auto boxAt = [&](double w, double d, double h, double x, double y, double z, const double* rect) {
    Geometry b = three::box(w, d, h);
    b.translate(x, y, z);
    add(std::move(b), rect);
  };
  const double cx = thick ? width * 0.23 : 0, radius = width * (thick ? 0.17 : 0.23);
  const double top = height * 0.87, bevel = thick ? height * 0.08 : 0;
  Shape frame;
  frame.moveTo(-width / 2, 0);
  frame.lineTo(-width / 2, height);
  frame.lineTo(width / 2, height);
  frame.lineTo(width / 2, 0);
  frame.lineTo(cx + radius, 0);
  frame.lineTo(cx + radius, top - bevel);
  frame.lineTo(cx + radius * 0.8, top);
  frame.lineTo(cx - radius * 0.8, top);
  frame.lineTo(cx - radius, top - bevel);
  frame.lineTo(cx - radius, 0);
  frame.closePath();
  if (thick) {
    three::Path hole;
    hole.moveTo(-width * 0.36, height * 0.26);
    hole.lineTo(-width * 0.36, height * 0.68);
    hole.lineTo(-width * 0.1, height * 0.68);
    hole.lineTo(-width * 0.1, height * 0.26);
    hole.closePath();
    frame.holes.push_back(hole);
    for (int i = 0; i < 4; ++i) boxAt(width * 0.017, tile * 0.055, height * 0.42, -width * 0.33 + i * width * 0.067, 0, height * 0.47, metal);
    for (double z : {0.38, 0.55}) boxAt(width * 0.26, tile * 0.065, height * 0.018, -width * 0.23, 0, height * z, metal);
  }
  add(extruded(frame, depth, 12), stone);
  boxAt(width, depth * 1.1, height * 0.065, 0, 0, height * 0.9675, stone);
  if (!thick) {
    for (int sign : {-1, 1}) boxAt(tile * 0.085, depth * 1.06, top, cx + sign * radius, 0, top / 2, wood);
    boxAt(radius * 2 + tile * 0.12, depth * 1.1, height * 0.065, cx, 0, top, wood);
  }
  const bool swungOpen = !thick && def.model3d == "doorway";
  if (def.model3d == "door" || swungOpen) {
    const size_t leafStart = parts.size();
    Shape leaf;
    leaf.moveTo(cx - radius, 0);
    leaf.lineTo(cx + radius, 0);
    leaf.lineTo(cx + radius, top - bevel);
    leaf.lineTo(cx + radius * 0.8, top);
    leaf.lineTo(cx - radius * 0.8, top);
    leaf.lineTo(cx - radius, top - bevel);
    leaf.closePath();
    add(extruded(leaf, tile * 0.12, 12), wood);
    for (int side : {-1, 1}) {
      for (double z : {0.19, 0.43, 0.73}) boxAt(radius * 1.8, tile * 0.025, height * 0.035, cx, side * tile * 0.073, height * z, metal);
      Geometry t = three::torus(tile * 0.055, tile * 0.012, 6, 16);
      t.rotateX(kPi / 2).translate(cx + radius * 0.58, side * tile * 0.092, height * 0.48);
      add(std::move(t), metal);
    }
    if (swungOpen) {
      const double hingeX = cx + radius, hingeY = -depth / 2;
      for (size_t i = leafStart; i < parts.size(); ++i) parts[i].translate(-hingeX, 0, 0).rotateZ(kPi / 2).translate(hingeX, hingeY, 0);
    }
  }
  Geometry g = three::merge(parts);
  g.rotateZ(-rotation * kPi / 2);
  return g;
}

// --- EmberEngine's own pieces (ArchitectureDef::emberStyle; content/models/architecture.json) ---------------------------------
//
// The watchtower's masonry (2026-10-21, the user: "create real windows and watchtower walls that actually feel like a real
// design"). Ember's frame: x along the board, y = -z of the board, z up. Every piece has a base course (a little thicker and
// kHeight * 0.065 tall) and a string course under its top, so runs read as one built wall; the masonry UVs are world-aligned
// (the tail of wallGeometry), so courses run through the joins.
//   wall    a run joined to its neighbours (core + an arm to each), or a straight segment when it stands alone
//   window  a straight segment pierced by a lancet window: the outer half of the wall holds the narrow light, the inner half a
//           wider embrasure (a real splay), a sill ledge inside; the light of the sun / moon passes through the narrow hole
//   slit    an arrow loop: a tall narrow outer slit, a wide inner splay
//   portal  an arched doorway two hexes wide (replaces the flat-coloured watchtower entrance)
// A window's or slit's inner side is local -y (the board's +z) before the quarter turns; MapStage turns the piece so the splay
// faces the room (the board's centre).
// The masonry's UVs: world-aligned square repeats (continuous across the joins), left alone on the tagged materials.
void emberMasonryUVs(Geometry& g, std::pair<double, double> origin, double repeat) {
  for (size_t i = 0; i < g.count(); ++i) {
    if (g.uv[i * 2] >= kEmberUvTag) continue;
    const double x = g.px(i) + origin.first, y = g.py(i) + origin.second, z = g.pz(i);
    if (std::abs(g.nz(i)) > 0.5) g.setUV(i, x / repeat, y / repeat);
    else g.setUV(i, (std::abs(g.nx(i)) > 0.5 ? y : x) / repeat, z / repeat);
  }
}
void tagMaterial(Geometry& g, int tag) {
  for (size_t i = 0; i < g.count(); ++i) g.setUV(i, kEmberUvTag * tag + 0.5, 0.5);
}

// The arched opening of the portal, the door and the gate: half the width, the spring of the arch and its rise.
void emberArchOf(const ArchitectureDef& def, double tile, double height, int rotation, double& width, double& radius, double& spring, double& rise) {
  const double cellSpan = tile * ((rotation & 1) ? 1.5 : kSqrt3);
  width = cellSpan * def.architectureSpan.value_or(2);
  radius = width * 0.29, spring = height * 0.5, rise = height * 0.3;
}
// One leaf of the door (side -1 left, +1 right), closed in the frame's own space (Ember's frame, before the piece's quarter turns),
// then swung `swing` radians inwards about its hinge (the jamb, at y = 0): two oak planks' worth of leaf (tag 1), plank seams
// (tag 4), iron straps and a ring handle (tag 2).
Geometry doorLeaf(const ArchitectureDef& def, double tile, double height, int rotation, int side, double swing) {
  double width, radius, spring, rise;
  emberArchOf(def, tile, height, rotation, width, radius, spring, rise);
  const double leafT = tile * 0.14, r = radius - tile * 0.02;
  std::vector<Geometry> parts;
  auto add = [&](Geometry g, int tag) {
    tagMaterial(g, tag);
    parts.push_back(g.toNonIndexed());
  };
  Shape leaf;  // one half of the arched opening, split at the middle
  if (side < 0) {
    leaf.moveTo(-r, 0);
    leaf.lineTo(0, 0);
    leaf.lineTo(0, spring + rise - tile * 0.02);
    leaf.absellipse(0, spring, r, rise - tile * 0.02, kPi / 2, kPi, false);
    leaf.lineTo(-r, 0);
  } else {
    leaf.moveTo(0, 0);
    leaf.lineTo(r, 0);
    leaf.lineTo(r, spring);
    leaf.absellipse(0, spring, r, rise - tile * 0.02, 0, kPi / 2, false);
    leaf.lineTo(0, 0);
  }
  leaf.closePath();
  add(extruded(leaf, leafT, 16), 1);
  auto boxAt = [&](double w, double d, double h, double x, double y, double z, int tag) {
    Geometry b = three::box(w, d, h);
    b.translate(x, y, z);
    add(std::move(b), tag);
  };
  for (int face : {-1, 1}) {
    for (int k = 1; k < 4; ++k) boxAt(tile * 0.012, tile * 0.012, spring + rise * 0.6, side * r * k / 4.0, face * (leafT / 2 + tile * 0.004), (spring + rise * 0.6) / 2, 4);
    for (double z : {0.24, 0.62}) boxAt(r - tile * 0.06, tile * 0.025, height * 0.045, side * r / 2, face * (leafT / 2 + tile * 0.01), spring * z * 1.6, 2);
  }
  Geometry ring = three::torus(tile * 0.07, tile * 0.012, 8, 18);
  ring.rotateX(kPi / 2).translate(side * r * 0.3, -(leafT / 2 + tile * 0.06), spring * 0.7);
  add(std::move(ring), 2);
  Geometry g = three::merge(parts);
  // the swing: about the hinge at the jamb, inwards (local -y)
  const double hx = side * r;
  g.translate(-hx, 0, 0).rotateZ(side * swing).translate(hx, 0, 0);
  return g;
}

Geometry emberGeometry(const ArchitectureDef& def, double tile, double height, int rotation, const std::vector<Conn>& connections, std::pair<double, double> origin) {
  const double T = tile * 0.32 * def.wallThicknessScale;
  const double plinthH = height * 0.065, plinthOut = T * 0.16;
  const double capH = height * 0.045, capOut = T * 0.08;
  std::vector<Geometry> parts;
  auto boxAt = [&](double w, double d, double h, double x, double y, double z, double angle = 0, int tag = 0) {
    Geometry b = three::box(w, d, h);
    b.rotateZ(angle).translate(x, y, z);
    if (tag) tagMaterial(b, tag);
    parts.push_back(b.toNonIndexed());
  };
  auto finish = [&](Geometry g) {
    emberMasonryUVs(g, origin, std::max(def.textureRepeat, 0.1));
    return g;
  };
  const double axis = emberAxisAngle(rotation, connections);
  // The torch bracket on a wall's inner face: two iron arms and a ring, an oak shaft leaning out, the flame (tag 3) above it.
  auto torch = [&](double angle) {
    const double y0 = -(T / 2), zb = height * 0.62;  // the bracket's height on the wall
    std::vector<Geometry> t;
    auto add = [&](Geometry g, int tag) {
      tagMaterial(g, tag);
      t.push_back(g.toNonIndexed());
    };
    for (int side : {-1, 1}) {
      Geometry arm = three::box(0.05, 0.42, 0.05);
      arm.translate(side * 0.11, y0 - 0.19, zb);
      add(std::move(arm), 2);
    }
    Geometry plate = three::box(0.34, 0.03, 0.34);
    plate.translate(0, y0 - 0.015, zb);
    add(std::move(plate), 2);
    Geometry ring = three::torus(0.11, 0.018, 8, 20);
    ring.translate(0, y0 - 0.40, zb + 0.02);
    add(std::move(ring), 2);
    Geometry shaft = three::box(0.12, 0.12, 1.05);
    shaft.rotateX(0.32).translate(0, y0 - 0.40, zb + 0.30);  // leaning out of the wall
    add(std::move(shaft), 1);
    Geometry head = three::box(0.2, 0.2, 0.22);
    head.rotateX(0.32).translate(0, y0 - 0.57, zb + 0.78);
    add(std::move(head), 4);
    Geometry flame = three::sphere(0.16, 10, 8);  // the flame: a stretched drop (emissive); its light is MapStage's
    flame.scale(1.0, 1.0, 1.8).translate(0, y0 - 0.62, zb + 1.12);
    add(std::move(flame), 3);
    Geometry all = three::merge(t);
    all.rotateZ(angle);
    parts.push_back(std::move(all));
  };
  // the courses of a run of `length` centred at (x, y) along `angle`
  auto courses = [&](double length, double x, double y, double angle) {
    boxAt(length, T + 2 * plinthOut, plinthH, x, y, plinthH / 2, angle);
    boxAt(length, T + 2 * capOut, capH, x, y, height - capH / 2, angle);
  };
  // a lancet: the opening's half width, its sill, the spring of its arch and the arch's rise (counter-clockwise: a hole)
  auto lancet = [&](double halfW, double sill, double spring, double rise) {
    three::Path h;
    h.moveTo(-halfW, sill);
    h.lineTo(halfW, sill);
    h.lineTo(halfW, spring);
    h.absellipse(0, spring, halfW, rise, 0, kPi, false);
    h.lineTo(-halfW, sill);
    h.closePath();
    return h;
  };
  // a slab of the wall `length` x `depth` (its centre plane at y), pierced by `hole`
  auto pierced = [&](double length, double depth, double y, const three::Path& hole) {
    Shape s;
    s.moveTo(-length / 2, 0);
    s.lineTo(-length / 2, height);
    s.lineTo(length / 2, height);
    s.lineTo(length / 2, 0);
    s.closePath();
    s.holes.push_back(hole);
    Geometry g = extruded(s, depth, 12);
    g.translate(0, y, 0);
    parts.push_back(g.toNonIndexed());
  };
  // A straight piece's span: to the midpoints towards its neighbours (their arms reach there), else the cell's own width.
  const bool horizontal = std::abs(std::cos(axis)) > 0.5;
  const double span = connections.empty() ? tile * (horizontal ? kSqrt3 : 1.5) : std::hypot(connections.front().first, connections.front().second);
  if (def.emberStyle == "window" || def.emberStyle == "slit") {
    const bool slit = def.emberStyle == "slit";
    const double length = span + tile * 0.02;
    // outer light (narrow) in the outer half (+y), the splayed embrasure in the inner half (-y)
    const double wo = slit ? 0.09 : 0.31, so = slit ? height * 0.34 : height * 0.345, po = slit ? height * 0.72 : height * 0.68, ro = slit ? 0.09 : 0.3;
    const double wi = slit ? 0.5 : 0.56, si = slit ? height * 0.3 : height * 0.31, pi_ = slit ? height * 0.7 : height * 0.68, ri = slit ? 0.5 : 0.55;
    pierced(length, T / 2, T / 4, lancet(wo, so, po, ro));
    pierced(length, T / 2, -T / 4, lancet(wi, si, pi_, ri));
    // the sill ledge inside, a little proud of the wall
    boxAt(wi * 2 + tile * 0.26, T * 0.34, height * 0.028, 0, -T / 2 - T * 0.12, si - height * 0.014);
    courses(length, 0, 0, 0);
    Geometry g = three::merge(parts);
    g.rotateZ(axis);
    return finish(std::move(g));
  }
  if (def.emberStyle == "portal" || def.emberStyle == "door" || def.emberStyle == "gate") {
    // The arched doorway (two hexes; the gate four): the frame, its courses stopping at the opening, a keystone. The door's leaves
    // are their own meshes (emberDoorLeafPiece: MapStage swings them); the gate's stand swung open, baked in; both have iron hinge
    // knuckles on the jambs.
    double width, radius, spring, rise;
    emberArchOf(def, tile, height, rotation, width, radius, spring, rise);
    parts.push_back(extruded(archFrame(width, height, radius, spring, rise), T, 24).toNonIndexed());
    const double post = width / 2 - radius;
    for (int side : {-1, 1}) {
      boxAt(post, T + 2 * plinthOut, plinthH, side * (width - post) / 2, 0, plinthH / 2);
      boxAt(post, T + 2 * capOut, capH, side * (width - post) / 2, 0, height - capH / 2);
    }
    boxAt(width, T + 2 * capOut, capH, 0, 0, height - capH / 2);
    boxAt(radius * 0.36, T + 2 * capOut, height * 0.12, 0, 0, spring + rise + height * 0.04);
    if (def.emberStyle != "portal") {
      const double leafT = tile * 0.14, r = radius - tile * 0.02;
      for (int side : {-1, 1})
        for (double z : {0.3, 0.75}) boxAt(tile * 0.07, tile * 0.07, height * 0.06, side * (r + tile * 0.03), -(leafT / 2 + tile * 0.02), spring * z * 1.6, 0, 2);
      if (def.emberStyle == "gate")
        for (int side : {-1, 1}) parts.push_back(doorLeaf(def, tile, height, rotation, side, kEmberDoorOpenRadians));
    }
    Geometry g = three::merge(parts);
    g.rotateZ(-rotation * kPi / 2);
    return finish(std::move(g));
  }
  // "wall" / "wall-torch": Ember's joined wall with its courses; the torch bracket on the inner face (emberAxisAngle)
  if (connections.empty()) {
    boxAt(span, T, height, 0, 0, height / 2);
    courses(span, 0, 0, 0);
    if (def.emberStyle == "wall-torch") torch(0.0);
    Geometry g = three::merge(parts);
    g.rotateZ(axis);
    return finish(std::move(g));
  }
  boxAt(T, T, height, 0, 0, height / 2);
  boxAt(T + 2 * plinthOut, T + 2 * plinthOut, plinthH, 0, 0, plinthH / 2);
  boxAt(T + 2 * capOut, T + 2 * capOut, capH, 0, 0, height - capH / 2);
  for (const auto& [cx, cy] : connections) {
    const double angle = std::atan2(cy, cx), length = std::hypot(cx, cy) / 2 + tile * 0.01;
    boxAt(length, T, height, cx / 4, cy / 4, height / 2, angle);
    courses(length, cx / 4, cy / 4, angle);
  }
  if (def.emberStyle == "wall-torch") torch(axis);
  return finish(three::merge(parts));
}

}  // namespace

three::Geometry emberDoorLeafPiece(const ArchitectureDef& def, int rotation, int side) {
  const double height = 1.0 * 0.75 * def.heightScale * 3.5 * 1.6;  // pieceGeometry's WALL_HUMAN_SCALE, tile 1
  Geometry g = doorLeaf(def, 1.0, height, rotation, side, 0.0);
  g.rotateZ(-rotation * kPi / 2);
  g.rotateX(-kPi / 2);
  return g.toNonIndexed();
}

std::array<double, 3> emberDoorHingePiece(const ArchitectureDef& def, int rotation, int side) {
  const double height = 1.0 * 0.75 * def.heightScale * 3.5 * 1.6;
  double width, radius, spring, rise;
  emberArchOf(def, 1.0, height, rotation, width, radius, spring, rise);
  // the jamb in the frame's space, through the piece's quarter turns (rotateZ(-rotation * pi / 2)) and up (rotateX(-pi / 2): (x, y, z) -> (x, z, -y))
  const double hx = side * (radius - 0.02), a = -rotation * kPi / 2;
  const double x = hx * std::cos(a), y = hx * std::sin(a);
  return {x, 0.0, -y};
}

double emberAxisAngle(int rotation, const std::vector<std::pair<double, double>>& connections) {
  double a;
  if (connections.empty()) {
    a = (rotation & 1) ? kPi / 2 : 0.0;
  } else {
    a = std::atan2(connections.front().second, connections.front().first);
    if (a < -1e-9) a += kPi;  // the run's direction, not the neighbour's side
    if (a >= kPi - 1e-9) a -= kPi;
  }
  return a + ((rotation & 2) ? kPi : 0.0);
}

// --- ThreeWalls.ts createWallGeometry -----------------------------------------------------------------------------------

Geometry wallGeometry(const ArchitectureDef& def, double tile, int rotation, const std::vector<Conn>& connections, std::pair<double, double> origin,
                      bool projectHeight, double textureHeightScale) {
  const double height = tile * 0.75 * def.heightScale;
  Geometry geometry;
  const bool referenceDoor = def.doorStyle == "stoneOak" || def.doorStyle == "dungeonOak";
  if (def.id == "watchtower-stone-open-door-2hex") {
    const double width = tile * ((rotation & 1) ? 1.5 : kSqrt3) * 2;
    const double depth = tile * 0.32 * def.wallThicknessScale;
    const double post = depth, lintel = height * 0.16;
    std::vector<Geometry> parts;
    Geometry l = three::box(width, depth, lintel);
    l.translate(0, 0, height - lintel / 2);
    parts.push_back(std::move(l));
    for (int side : {-1, 1}) {
      Geometry p = three::box(post, depth, height - lintel);
      p.translate(side * (width - post) / 2, 0, (height - lintel) / 2);
      parts.push_back(std::move(p));
    }
    geometry = three::merge(parts);
    geometry.rotateZ(-rotation * kPi / 2);
  } else if (!def.emberStyle.empty()) {
    geometry = emberGeometry(def, tile, height, rotation, connections, origin);
  } else if (referenceDoor) {
    geometry = referenceDoorGeometry(def, tile, height, rotation);
  } else if (!def.rockStyle.empty()) {
    geometry = rockGeometry(def, tile, height, rotation);
  } else if (!def.templeStyle.empty()) {
    geometry = templeGeometry(def, tile, height, rotation, connections);
  } else if (!def.castleStyle.empty()) {
    geometry = castleGeometry(def, tile, height, rotation, connections);
  } else if (def.model3d == "wall" || def.model3d == "secretDoor") {
    const double thickness = tile * 0.32 * def.wallThicknessScale;
    std::vector<Geometry> parts;
    if (connections.empty() || (def.architectureSpan && *def.architectureSpan != 0)) {
      const double length = tile * ((rotation & 1) ? 1.5 : kSqrt3) * def.architectureSpan.value_or(1);
      Geometry b = three::box(length, thickness, height);
      b.rotateZ(-rotation * kPi / 2).translate(0, 0, height / 2);
      parts.push_back(std::move(b));
    } else {
      std::vector<int> open;
      for (const auto& [x, y] : connections) open.push_back(std::abs(x) > std::abs(y) ? (x > 0 ? 0 : 1) : (y > 0 ? 2 : 3));
      Geometry core = three::boxWithout(thickness, thickness, height, open);
      core.translate(0, 0, height / 2);
      parts.push_back(std::move(core));
      for (const auto& [x, y] : connections) {
        const double length = std::hypot(x, y) / 2 + tile * 0.01;
        Geometry b = three::boxWithout(length, thickness, height, {0, 1});
        b.rotateZ(std::atan2(y, x)).translate(x / 4, y / 4, height / 2);
        parts.push_back(std::move(b));
      }
    }
    geometry = three::merge(parts);
  } else {
    const double width = tile * ((rotation & 1) ? 1.5 : kSqrt3) * def.architectureSpan.value_or(1), post = tile * 0.18,
                 depth = tile * 0.32 * def.wallThicknessScale;
    auto boxAt = [](double w, double h, double z, double x, double elevation) {
      Geometry b = three::box(w, h, z);
      b.translate(x, 0, elevation);
      return b;
    };
    const double radius = (width - post * 2) / 2;
    const double archRise = std::min(radius, height * 0.4);
    const double header = height * (projectHeight ? 0.1 : 0.18);
    const double spring = height - header - archRise;
    std::vector<Geometry> parts;
    parts.push_back(extruded(archFrame(width, height, radius, spring, archRise), depth, 16));
    if (def.model3d == "door") {
      const double leafRadius = radius - tile * 0.025;
      Shape leaf;
      leaf.moveTo(-leafRadius, 0);
      leaf.lineTo(leafRadius, 0);
      leaf.lineTo(leafRadius, spring);
      leaf.absellipse(0, spring, leafRadius, archRise - tile * 0.025, 0, kPi, false);
      leaf.lineTo(-leafRadius, 0);
      leaf.closePath();
      parts.push_back(extruded(leaf, tile * 0.12, 16));
      const bool metal = def.doorStyle == "iron" || def.doorStyle == "steel";
      const double strapHeight = height * (def.doorStyle == "reinforced" ? 0.09 : 0.06);
      parts.push_back(boxAt(leafRadius * 2, tile * 0.17, strapHeight, 0, spring * 0.25));
      parts.push_back(boxAt(leafRadius * 2, tile * 0.17, strapHeight, 0, spring * 0.8));
      parts.push_back(boxAt(tile * 0.08, tile * 0.23, height * 0.07, leafRadius * 0.65, spring * 0.5));
      if (def.doorStyle == "steel")
        for (double x : {-leafRadius * 0.5, 0.0, leafRadius * 0.5}) parts.push_back(boxAt(tile * 0.055, tile * 0.17, spring * 0.85, x, spring * 0.46));
      if (metal || def.doorStyle == "reinforced")
        for (double x : {-leafRadius * 0.83, leafRadius * 0.83})
          for (double z : {spring * 0.25, spring * 0.8})
            for (int side : {-1, 1}) {
              Geometry s = three::sphere(tile * 0.035, 6, 4);
              s.translate(x, side * tile * 0.095, z);
              parts.push_back(std::move(s));
            }
      if (def.doorStyle == "reinforced") parts.push_back(boxAt(tile * 0.09, tile * 0.19, spring * 0.92, 0, spring * 0.46));
    }
    std::vector<Geometry> compatible;
    for (const Geometry& p : parts) compatible.push_back(p.toNonIndexed());
    geometry = three::merge(compatible);
    geometry.rotateZ(-rotation * kPi / 2);
  }
  // The masonry in world coordinates (continuous across joins), or the dungeon reference photograph's regions.
  // EmberEngine's pieces map their own masonry (emberMasonryUVs: square repeats of textureRepeat units, their extra materials
  // tagged in the UVs); Ember's own stretch the texture 3.5 x in height.
  const double repeatSize = tile;
  three::Box3 bounds;
  if (def.dungeonReference) bounds = geometry.boundingBox();
  const bool ownUVs = referenceDoor || !def.castleStyle.empty() || !def.templeStyle.empty() || !def.rockStyle.empty() || !def.emberStyle.empty();
  for (size_t i = 0; !ownUVs && i < geometry.count(); ++i) {
    const double x = geometry.px(i) + origin.first, y = geometry.py(i) + origin.second, z = geometry.pz(i);
    if (def.dungeonReference) {
      const bool vertical = (rotation & 1) != 0;
      const bool front = std::abs(vertical ? geometry.nx(i) : geometry.ny(i)) > 0.5;
      const bool useX = front != vertical;
      const double local = useX ? geometry.px(i) : geometry.py(i);
      const double low = useX ? bounds.min[0] : bounds.min[1];
      const double span = (useX ? bounds.max[0] : bounds.max[1]) - low;
      const double u = (local - low) / std::max(0.001, span);
      if (std::abs(geometry.nz(i)) > 0.5) {
        const double capU = (geometry.px(i) - bounds.min[0]) / std::max(0.001, bounds.max[0] - bounds.min[0]);
        const double capV = (geometry.py(i) - bounds.min[1]) / std::max(0.001, bounds.max[1] - bounds.min[1]);
        geometry.setUV(i, (900 + capU * 165) / 1168, 1 - (480 - capV * 260) / 784);
        continue;
      }
      const double v = z / height;
      geometry.setUV(i, front ? (90 + u * 990) / 1168 : (900 + u * 165) / 1168, 1 - (655 - v * 535) / 784);
      continue;
    }
    if (std::abs(geometry.nz(i)) > 0.5) geometry.setUV(i, x / repeatSize, y / repeatSize);
    else geometry.setUV(i, (std::abs(geometry.nx(i)) > 0.5 ? y : x) / repeatSize, z / (repeatSize * textureHeightScale));
  }
  if (projectHeight)
    for (size_t i = 0; i < geometry.count(); ++i) geometry.setY(i, geometry.py(i) + geometry.pz(i) * 3.5);
  return geometry;
}

three::Geometry pieceGeometry(const ArchitectureDef& def, int rotation, const std::vector<Conn>& connections, std::pair<double, double> origin) {
  ArchitectureDef spatial = def;
  spatial.heightScale = def.heightScale * 3.5 * 1.6;  // WALL_HUMAN_SCALE
  Geometry g = wallGeometry(spatial, 1, rotation, connections, origin, false, 3.5);
  g.rotateX(-kPi / 2);
  return g.toNonIndexed();
}

int checkArchitecture(const std::filesystem::path& refs, const ArchitectureDefs& defs) {
  std::ifstream in(refs, std::ios::binary);
  const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
  if (!j.is_object() || !j.contains("cases")) {
    logError("check-architecture: cannot read {}", refs.string());
    return 1;
  }
  int bad = 0, cases = 0;
  double worst = 0;
  for (const nlohmann::json& c : j["cases"]) {
    ++cases;
    const std::string id = c.value("id", "");
    const ArchitectureDef* def = defs.find(id);
    if (!def) {
      ++bad, logError("check-architecture: {} is not in the catalog", id);
      continue;
    }
    std::vector<Conn> connections;
    for (const nlohmann::json& k : c["connections"]) connections.push_back({k.value("x", 0.0), k.value("y", 0.0)});
    const Geometry g = pieceGeometry(*def, c.value("rot", 0), connections, {c["origin"].value("x", 0.0), c["origin"].value("y", 0.0)});
    const std::string name = std::format("{} rot {} joins {}", id, c.value("rot", 0), connections.size());
    auto compare = [&](const char* what, const std::vector<float>& mine, double tolerance) {
      const nlohmann::json& ref = c[what];
      if (ref.size() != mine.size()) {
        ++bad, logError("check-architecture: {}: {} has {} values here, {} in Engine2", name, what, mine.size(), ref.size());
        return;
      }
      double d = 0;
      for (size_t i = 0; i < mine.size(); ++i) d = std::max(d, std::abs(static_cast<double>(mine[i]) - ref[i].get<double>()));
      worst = std::max(worst, d);
      if (d > tolerance) ++bad, logError("check-architecture: {}: {} differ by {:.6f}", name, what, d);
    };
    compare("position", g.position, 1e-4);
    compare("normal", g.normal, 1e-3);
    compare("uv", g.uv, 1e-4);
  }
  if (bad == 0) logInfo("check-architecture: {} cases, every piece matches Engine2 (largest difference {:.2e})", cases, worst);
  return bad;
}

// --- the catalog --------------------------------------------------------------------------------------------------------

bool ArchitectureDefs::load(const std::filesystem::path& catalogFile) {
  std::ifstream in(catalogFile, std::ios::binary);
  const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
  if (!j.is_object() || !j.contains("editor") || !j["editor"].contains("architectureDefs")) return false;
  auto s = [](const nlohmann::json& o, const char* k) { return o.contains(k) && o[k].is_string() ? o[k].get<std::string>() : std::string(); };
  auto n = [](const nlohmann::json& o, const char* k, double f) { return o.contains(k) && o[k].is_number() ? o[k].get<double>() : f; };
  for (const auto& [id, d] : j["editor"]["architectureDefs"].items()) {
    ArchitectureDef a;
    a.id = id, a.model3d = s(d, "model3d");
    a.heightScale = n(d, "heightScale", 1), a.wallThicknessScale = n(d, "wallThicknessScale", 1);
    if (d.contains("architectureSpan") && d["architectureSpan"].is_number()) a.architectureSpan = d["architectureSpan"].get<double>();
    a.doorStyle = s(d, "doorStyle"), a.rockStyle = s(d, "rockStyle"), a.templeStyle = s(d, "templeStyle"), a.castleStyle = s(d, "castleStyle");
    a.wallTexture = s(d, "wallTexture");
    a.thickWall = d.value("thickWall", false), a.dungeonReference = d.value("dungeonReference", false), a.connects = d.value("connects", true);
    a.emberStyle = s(d, "emberStyle"), a.textureRepeat = n(d, "textureRepeat", 3.6);
    defs.emplace(id, std::move(a));
  }
  return true;
}

const ArchitectureDef* ArchitectureDefs::find(const std::string& id) const {
  const auto it = defs.find(id);
  return it == defs.end() ? nullptr : &it->second;
}

}  // namespace ember::e2
