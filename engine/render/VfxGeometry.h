#pragma once

// three.js geometries rebuilt exactly (r16x src/geometries): the shapes Ember's effects are made of, as triangle lists
// (indices expanded) of VfxVertex. Built once and kept (each call returns the same vector).

#include <cmath>
#include <numbers>
#include <vector>

#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember::vfxgeo {

namespace detail {

// PolyhedronGeometry: each face split into (detail + 1)^2 triangles, every vertex pushed onto the sphere of `radius`.
// detail 0 keeps flat face normals (three: computeVertexNormals on the unindexed faces), else normals = directions.
inline std::vector<VfxVertex> polyhedron(const std::vector<glm::vec3>& verts, const std::vector<int>& indices, float radius, int detail) {
  std::vector<glm::vec3> out;
  for (size_t f = 0; f + 2 < indices.size(); f += 3) {
    const glm::vec3 a = verts[indices[f]], b = verts[indices[f + 1]], c = verts[indices[f + 2]];
    const int cols = detail + 1;
    std::vector<std::vector<glm::vec3>> v(cols + 1);
    for (int i = 0; i <= cols; ++i) {
      const glm::vec3 aj = glm::mix(a, c, static_cast<float>(i) / cols), bj = glm::mix(b, c, static_cast<float>(i) / cols);
      const int rows = cols - i;
      v[i].resize(rows + 1);
      for (int j = 0; j <= rows; ++j) v[i][j] = (j == 0 && i == cols) ? aj : glm::mix(aj, bj, static_cast<float>(j) / rows);
    }
    for (int i = 0; i < cols; ++i)
      for (int j = 0; j < 2 * (cols - i) - 1; ++j) {
        const int k = j / 2;
        if (j % 2 == 0) out.insert(out.end(), {v[i][k + 1], v[i + 1][k], v[i][k]});
        else out.insert(out.end(), {v[i][k + 1], v[i + 1][k + 1], v[i + 1][k]});
      }
  }
  std::vector<VfxVertex> result(out.size());
  for (size_t i = 0; i < out.size(); ++i) {
    const glm::vec3 p = glm::normalize(out[i]) * radius;
    result[i].position = p;
    result[i].normal = glm::normalize(p);
    // three's spherical uv (azimuth / inclination); the ported shaders do not read it
    result[i].u = std::atan2(p.z, -p.x) / (2.0f * std::numbers::pi_v<float>) + 0.5f;
    result[i].v = std::atan2(-p.y, std::sqrt(p.x * p.x + p.z * p.z)) / std::numbers::pi_v<float> + 0.5f;
  }
  if (detail == 0)
    for (size_t i = 0; i + 2 < result.size(); i += 3) {
      const glm::vec3 n = glm::normalize(glm::cross(result[i + 1].position - result[i].position, result[i + 2].position - result[i].position));
      result[i].normal = result[i + 1].normal = result[i + 2].normal = n;
    }
  return result;
}

}  // namespace detail

// IcosahedronGeometry(radius, detail).
inline std::vector<VfxVertex> icosahedron(float radius, int detailLevel) {
  const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
  const std::vector<glm::vec3> v = {{-1, t, 0}, {1, t, 0},  {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                                    {0, -1, -t}, {0, 1, -t}, {t, 0, -1},  {t, 0, 1},  {-t, 0, -1}, {-t, 0, 1}};
  const std::vector<int> i = {0, 11, 5, 0, 5, 1,  0, 1, 7,  0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
                              3, 9,  4, 3, 4, 2,  3, 2, 6,  3, 6, 8,  3, 8,  9,  4, 9, 5, 2, 4,  11, 6,  2,  10, 8, 6, 7, 9, 8, 1};
  return detail::polyhedron(v, i, radius, detailLevel);
}

// TetrahedronGeometry(radius, detail).
inline std::vector<VfxVertex> tetrahedron(float radius, int detailLevel) {
  const std::vector<glm::vec3> v = {{1, 1, 1}, {-1, -1, 1}, {-1, 1, -1}, {1, -1, -1}};
  const std::vector<int> i = {2, 1, 0, 0, 3, 2, 1, 3, 0, 2, 3, 1};
  return detail::polyhedron(v, i, radius, detailLevel);
}

// OctahedronGeometry(radius, detail).
inline std::vector<VfxVertex> octahedron(float radius, int detailLevel) {
  const std::vector<glm::vec3> v = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  const std::vector<int> i = {0, 2, 4, 0, 4, 3, 0, 3, 5, 0, 5, 2, 1, 2, 5, 1, 5, 3, 1, 3, 4, 1, 4, 2};
  return detail::polyhedron(v, i, radius, detailLevel);
}

// TorusGeometry(radius, tube, radialSegments, tubularSegments, arc): in the xy plane, around +z.
inline std::vector<VfxVertex> torus(float radius, float tube, int radialSegments, int tubularSegments, float arc) {
  std::vector<VfxVertex> grid;
  for (int j = 0; j <= radialSegments; ++j)
    for (int i = 0; i <= tubularSegments; ++i) {
      const float u = static_cast<float>(i) / tubularSegments * arc;
      const float v = static_cast<float>(j) / radialSegments * 2.0f * std::numbers::pi_v<float>;
      VfxVertex x{};
      x.position = {(radius + tube * std::cos(v)) * std::cos(u), (radius + tube * std::cos(v)) * std::sin(u), tube * std::sin(v)};
      x.normal = glm::normalize(x.position - glm::vec3(radius * std::cos(u), radius * std::sin(u), 0.0f));
      x.u = static_cast<float>(i) / tubularSegments;
      x.v = static_cast<float>(j) / radialSegments;
      grid.push_back(x);
    }
  std::vector<VfxVertex> out;
  const int row = tubularSegments + 1;
  for (int j = 1; j <= radialSegments; ++j)
    for (int i = 1; i <= tubularSegments; ++i) {
      const int a = row * j + i - 1, b = row * (j - 1) + i - 1, c = row * (j - 1) + i, d = row * j + i;
      out.insert(out.end(), {grid[a], grid[b], grid[d], grid[b], grid[c], grid[d]});
    }
  return out;
}

// An indexed mesh expanded to triangles with three's computeVertexNormals (each vertex: the sum of its faces'
// unnormalised normals, normalised).
inline std::vector<VfxVertex> expandSmooth(const std::vector<glm::vec3>& positions, const std::vector<int>& indices) {
  std::vector<glm::vec3> normals(positions.size(), glm::vec3(0.0f));
  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    const glm::vec3 a = positions[indices[i]], b = positions[indices[i + 1]], c = positions[indices[i + 2]];
    const glm::vec3 n = glm::cross(c - b, a - b);
    normals[indices[i]] += n, normals[indices[i + 1]] += n, normals[indices[i + 2]] += n;
  }
  std::vector<VfxVertex> out;
  for (int k : indices) out.push_back({positions[k], 0.0f, three::normalize(normals[k]), 0.0f});
  return out;
}

// BoxGeometry(width, height, depth) (one segment per side): six flat faces.
inline std::vector<VfxVertex> box(float width, float height, float depth) {
  std::vector<VfxVertex> out;
  // buildPlane(u, v, w, udir, vdir, width, height, depth) for px, nx, py, ny, pz, nz.
  auto plane = [&](int u, int v, int w, float udir, float vdir, float pw, float ph, float pd) {
    VfxVertex grid[2][2];
    for (int iy = 0; iy < 2; ++iy)
      for (int ix = 0; ix < 2; ++ix) {
        glm::vec3 p{0.0f}, n{0.0f};
        p[u] = (ix * pw - pw * 0.5f) * udir;
        p[v] = (iy * ph - ph * 0.5f) * vdir;
        p[w] = pd * 0.5f;
        n[w] = pd > 0 ? 1.0f : -1.0f;
        grid[iy][ix] = {p, static_cast<float>(ix), n, 1.0f - iy};
      }
    const VfxVertex a = grid[0][0], b = grid[1][0], c = grid[1][1], d = grid[0][1];
    out.insert(out.end(), {a, b, d, b, c, d});
  };
  plane(2, 1, 0, -1, -1, depth, height, width);
  plane(2, 1, 0, 1, -1, depth, height, -width);
  plane(0, 2, 1, 1, 1, width, depth, height);
  plane(0, 2, 1, 1, -1, width, depth, -height);
  plane(0, 1, 2, 1, -1, width, height, depth);
  plane(0, 1, 2, -1, -1, width, height, -depth);
  return out;
}

// A three.js Line (1 px) as a camera-facing ribbon of the given half width (world points).
inline std::vector<VfxVertex> lineRibbon(const std::vector<glm::vec3>& points, glm::vec3 eye, float halfWidth) {
  std::vector<VfxVertex> out;
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    const glm::vec3 a = points[i], b = points[i + 1];
    const glm::vec3 side = three::normalize(glm::cross(b - a, eye - (a + b) * 0.5f)) * halfWidth;
    const VfxVertex v0{a - side, 0, {0, 0, 1}, 0}, v1{a + side, 0, {0, 0, 1}, 1}, v2{b - side, 1, {0, 0, 1}, 0}, v3{b + side, 1, {0, 0, 1}, 1};
    out.insert(out.end(), {v0, v1, v2, v1, v3, v2});
  }
  return out;
}

// SphereGeometry(radius, widthSegments, heightSegments) (full sphere).
inline std::vector<VfxVertex> sphere(float radius, int widthSegments, int heightSegments) {
  std::vector<std::vector<VfxVertex>> grid;
  for (int iy = 0; iy <= heightSegments; ++iy) {
    const float v = static_cast<float>(iy) / heightSegments;
    const float theta = v * std::numbers::pi_v<float>;
    const float y = radius * std::cos(theta);
    const float ringRadius = std::sqrt(std::max(0.0f, radius * radius - y * y));
    float uOffset = 0.0f;
    if (iy == 0) uOffset = 0.5f / widthSegments;
    else if (iy == heightSegments) uOffset = -0.5f / widthSegments;
    std::vector<VfxVertex> row;
    for (int ix = 0; ix <= widthSegments; ++ix) {
      const float u = static_cast<float>(ix) / widthSegments;
      const float phi = u * 2.0f * std::numbers::pi_v<float>;
      const glm::vec3 p{-ringRadius * std::cos(phi), y, ringRadius * std::sin(phi)};
      row.push_back({p, u + uOffset, glm::length(p) > 0.0f ? glm::normalize(p) : glm::vec3(0, 1, 0), 1.0f - v});
    }
    grid.push_back(row);
  }
  std::vector<VfxVertex> out;
  for (int iy = 0; iy < heightSegments; ++iy)
    for (int ix = 0; ix < widthSegments; ++ix) {
      const VfxVertex a = grid[iy][ix + 1], b = grid[iy][ix], c = grid[iy + 1][ix], d = grid[iy + 1][ix + 1];
      if (iy != 0) out.insert(out.end(), {a, b, d});
      if (iy != heightSegments - 1) out.insert(out.end(), {b, c, d});
    }
  return out;
}

// TubeGeometry(path, tubularSegments, radius, radialSegments, false) as triangles (three::tubeRings for the rings).
inline std::vector<VfxVertex> tube(const three::CatmullRom& path, int tubularSegments, float radius, int radialSegments) {
  std::vector<glm::vec3> normals;
  const std::vector<glm::vec3> rings = three::tubeRings(path, tubularSegments, radius, radialSegments, &normals);
  auto vertex = [&](int i, int j) {
    const int k = i * (radialSegments + 1) + j;
    return VfxVertex{rings[k], static_cast<float>(i) / tubularSegments, normals[k], static_cast<float>(j) / radialSegments};
  };
  std::vector<VfxVertex> out;
  for (int j = 1; j <= tubularSegments; ++j)
    for (int i = 1; i <= radialSegments; ++i) {
      const VfxVertex a = vertex(j - 1, i - 1), b = vertex(j, i - 1), c = vertex(j, i), d = vertex(j - 1, i);
      out.insert(out.end(), {a, b, d, b, c, d});
    }
  return out;
}

}  // namespace ember::vfxgeo
