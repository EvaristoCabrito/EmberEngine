#pragma once

// The few three.js (r186) geometry builders Ember's architecture is made of, ported so EmberEngine builds the same
// solids: BoxGeometry, SphereGeometry, TorusGeometry, IcosahedronGeometry, ExtrudeGeometry of a Shape (lines, ellipse
// arcs, holes; triangulated by earcut 3.0.2 as three.js does), and the BufferGeometry operations they go through
// (matrix transforms with the normal matrix, toNonIndexed, mergeGeometries, computeVertexNormals). Attributes are kept
// in float32 like three's Float32BufferAttribute; the arithmetic between them is double, as in JavaScript.
//
// world/Architecture.cpp builds the walls, doors and gates from these; Cooker check-architecture compares the result
// with Ember's own geometry (tools/scripts/data/export_architecture_refs.ts).

#include <cstdint>
#include <vector>

namespace ember::three {

struct Box3 {
  double min[3] = {0, 0, 0}, max[3] = {0, 0, 0};
};

struct Geometry {
  std::vector<float> position, normal, uv;  // 3, 3 and 2 per vertex
  std::vector<uint32_t> index;              // empty when not indexed
  bool indexed = false;

  size_t count() const { return position.size() / 3; }
  // BufferGeometry.applyMatrix4 (m column-major, like Matrix4.elements): positions, and normals by the normal matrix.
  Geometry& applyMatrix(const double m[16]);
  Geometry& rotateX(double angle);
  Geometry& rotateY(double angle);
  Geometry& rotateZ(double angle);
  Geometry& translate(double x, double y, double z);
  Geometry& scale(double x, double y, double z);
  Geometry toNonIndexed() const;
  void computeVertexNormals();
  Box3 boundingBox() const;
  double px(size_t i) const { return position[i * 3]; }
  double py(size_t i) const { return position[i * 3 + 1]; }
  double pz(size_t i) const { return position[i * 3 + 2]; }
  double nx(size_t i) const { return normal[i * 3]; }
  double ny(size_t i) const { return normal[i * 3 + 1]; }
  double nz(size_t i) const { return normal[i * 3 + 2]; }
  void setUV(size_t i, double u, double v) { uv[i * 2] = static_cast<float>(u), uv[i * 2 + 1] = static_cast<float>(v); }
  void setY(size_t i, double y) { position[i * 3 + 1] = static_cast<float>(y); }
};

Geometry box(double width, double height, double depth);
// BoxGeometry with some of its six sides left out (0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z: Ember's omitBoxFaces).
Geometry boxWithout(double width, double height, double depth, const std::vector<int>& faces);
Geometry sphere(double radius, int widthSegments, int heightSegments);
Geometry torus(double radius, double tube, int radialSegments, int tubularSegments);
Geometry icosahedron(double radius, int detail);

// A Path / Shape outline: moveTo, lineTo, absellipse, closePath, as three.js records them (curves), and its holes.
class Path {
 public:
  Path& moveTo(double x, double y);
  Path& lineTo(double x, double y);
  Path& absellipse(double x, double y, double xRadius, double yRadius, double start, double end, bool clockwise);
  Path& closePath();
  // CurvePath.getPoints(divisions): every curve's points, consecutive duplicates dropped.
  std::vector<std::pair<double, double>> points(int divisions) const;

 private:
  struct Curve {
    bool ellipse = false;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;        // a line
    double ax = 0, ay = 0, rx = 0, ry = 0, a0 = 0, a1 = 0;  // an ellipse
    bool clockwise = false;
    std::pair<double, double> at(double t) const;
  };
  std::vector<Curve> curves_;
  double cx_ = 0, cy_ = 0;
};
struct Shape : Path {
  std::vector<Path> holes;
};

// ExtrudeGeometry(shape, {depth, bevelEnabled: false, curveSegments}): non-indexed, flat normals. Its UVs are left at
// zero: every Ember solid overwrites them.
Geometry extrude(const Shape& shape, double depth, int curveSegments);

// mergeGeometries: all indexed or none.
Geometry merge(const std::vector<Geometry>& parts);

// earcut 3.0.2 (mapbox/earcut, as copied into three.js): triangles of a polygon with holes (flat x, y pairs).
std::vector<uint32_t> earcut(const std::vector<double>& data, const std::vector<uint32_t>& holeIndices);

}  // namespace ember::three
