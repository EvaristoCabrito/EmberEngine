#include "GroundCheck.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "world/GroundMesh.h"

namespace ember {

namespace {

template <class T>
bool readRaw(const std::filesystem::path& path, std::vector<T>& out) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return false;
  const size_t bytes = static_cast<size_t>(in.tellg());
  out.resize(bytes / sizeof(T));
  in.seekg(0);
  return static_cast<bool>(in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size() * sizeof(T))));
}

struct Diff {
  double worst = 0;
  size_t over = 0;
};

Diff compare(const std::vector<float>& ours, const std::vector<float>& theirs, double tolerance) {
  Diff d;
  for (size_t i = 0; i < ours.size() && i < theirs.size(); ++i) {
    const double e = std::abs(static_cast<double>(ours[i]) - theirs[i]);
    d.worst = std::max(d.worst, e);
    if (e > tolerance) ++d.over;
  }
  return d;
}

// Ember splits the per-vertex weights into vec4 attributes: splatA, splatB, groundA, groundB, ...
// Its `stride` weights per vertex land in rows of `ours` (EmberEngine's own ground layers after Ember's stay 0).
bool loadWeights(const std::filesystem::path& dir, const std::string& name, uint32_t stride, uint32_t ours, size_t vertices, std::vector<float>& out) {
  out.assign(vertices * ours, 0.0f);
  for (uint32_t part = 0; part * 4 < stride; ++part) {
    const std::string attr = part == 0 ? "splatA" : part == 1 ? "splatB" : std::string("ground") + static_cast<char>('A' + part - 2);
    std::vector<float> data;
    if (!readRaw(dir / (name + "." + attr + ".f32"), data) || data.size() != vertices * 4) return false;
    for (size_t v = 0; v < vertices; ++v)
      for (uint32_t k = 0; k < 4 && part * 4 + k < stride; ++k) out[v * ours + part * 4 + k] = data[v * 4 + k];
  }
  return true;
}

bool checkOne(const std::filesystem::path& dir, const std::string& name, const ember::GroundGeometry& g, uint32_t classicStride) {
  std::vector<float> pos, nor, weights;
  std::vector<uint32_t> index;
  if (!readRaw(dir / (name + ".position.f32"), pos) || !readRaw(dir / (name + ".normal.f32"), nor)) {
    logError("  {}: reference missing", name);
    return false;
  }
  const size_t vertices = pos.size() / 3;
  bool ok = true;
  if (vertices != g.vertexCount()) {
    logError("  {}: {} vertices, Ember {}", name, g.vertexCount(), vertices);
    return false;
  }
  if (std::filesystem::exists(dir / (name + ".index.u32"))) {
    readRaw(dir / (name + ".index.u32"), index);
    if (index != g.indices) {
      size_t first = 0;
      while (first < index.size() && first < g.indices.size() && index[first] == g.indices[first]) ++first;
      logError("  {}: indices differ ({} vs Ember {}, first difference at {})", name, g.indices.size(), index.size(), first);
      ok = false;
    }
  } else if (!g.indices.empty()) {
    logError("  {}: indexed here, not in Ember", name);
    ok = false;
  }
  if (!loadWeights(dir, name, classicStride, g.stride, vertices, weights)) {
    logError("  {}: reference weights missing", name);
    return false;
  }
  const Diff p = compare(g.positions, pos, 1e-5), n = compare(g.normals, nor, 1e-4), w = compare(g.weights, weights, 1e-6);
  logInfo("  {}: {} vertices, {} triangles; worst difference: position {:.2e} ({} over 1e-5), normal {:.2e} ({} over 1e-4), weight {:.2e} ({} over 1e-6)",
          name, vertices, (g.indices.empty() ? vertices : g.indices.size()) / 3, p.worst, p.over, n.worst, n.over, w.worst, w.over);
  return ok && p.over == 0 && n.over == 0 && w.over == 0;
}

}  // namespace

bool checkGround(const std::filesystem::path& boardJson, const std::filesystem::path& mapJson, const std::filesystem::path& artRoot,
                 const std::filesystem::path& referenceRoot) {
  const std::optional<ember::BoardData> data = ember::BoardData::load(boardJson);
  std::optional<ember::MapDraft> draft = ember::MapDraft::load(mapJson);
  if (!data || !draft) return false;
  const std::filesystem::path dir = referenceRoot / draft->id;
  ember::Board board(*data, std::move(*draft));
  if (!board.loadRelief(artRoot)) return false;
  logInfo("{}: {}x{} hexes", board.draft().id, board.layout().cols, board.layout().rows);

  // Cells.
  std::ifstream in(dir / "cells.json");
  const nlohmann::json cells = nlohmann::json::parse(in, nullptr, false);
  bool ok = true;
  if (cells.is_discarded() || cells.size() != board.cells().size()) {
    logError("  cells: reference missing or a different size");
    return false;
  }
  size_t cellErrors = 0;
  for (size_t i = 0; i < cells.size(); ++i) {
    const nlohmann::json& r = cells[i];
    const ember::Cell& c = board.cells()[i];
    const bool same = r["type"] == c.type && r["level"].get<double>() == c.level && r["surface"].get<int>() == c.surface &&
                      r["ground"].get<int>() == c.ground && std::abs(r["groundY"].get<double>() - c.groundY) < 1e-12 &&
                      r["water"].get<bool>() == c.water && std::abs(r["waterY"].get<double>() - c.waterY) < 1e-12 &&
                      std::abs(r["x"].get<double>() - c.x) < 1e-12 && std::abs(r["z"].get<double>() - c.z) < 1e-12;
    if (!same && cellErrors++ < 5) logError("  cell {} ({}, {}) differs", i, c.c, c.r);
  }
  logInfo("  cells: {} of {} match", cells.size() - cellErrors, cells.size());
  ok = ok && cellErrors == 0;

  const uint32_t classicStride = 8 + static_cast<uint32_t>(board.data().classicLayerCount);  // 8 surfaces (GroundMesh)
  ok = checkOne(dir, "smooth", ember::buildSmoothGround(board), classicStride) && ok;
  ok = checkOne(dir, "hard", ember::buildHardGround(board), classicStride) && ok;

  // Water (no weights).
  for (const auto& [name, style] : {std::pair{"water-smooth", ember::EdgeStyle::Smooth}, std::pair{"water-hard", ember::EdgeStyle::Hard}}) {
    const ember::WaterGeometry w = ember::buildWater(board, style);
    std::vector<float> pos, nor;
    std::vector<uint32_t> index;
    const bool present = readRaw(dir / (std::string(name) + ".position.f32"), pos);
    if (!present) {
      if (!w.positions.empty()) logError("  {}: built here, absent in Ember", name), ok = false;
      continue;
    }
    readRaw(dir / (std::string(name) + ".normal.f32"), nor);
    readRaw(dir / (std::string(name) + ".index.u32"), index);
    const Diff p = compare(w.positions, pos, 1e-5), n = compare(w.normals, nor, 1e-4);
    const bool same = pos.size() == w.positions.size() && index == w.indices && p.over == 0 && n.over == 0;
    logInfo("  {}: {} vertices, {} triangles; position {:.2e}, normal {:.2e}{}", name, w.positions.size() / 3, w.indices.size() / 3, p.worst,
            n.worst, same ? "" : "  MISMATCH");
    ok = ok && same;
  }

  // Placement heights.
  std::ifstream mf(dir / "manifest.json");
  const nlohmann::json manifest = nlohmann::json::parse(mf, nullptr, false);
  double worst = 0;
  for (const nlohmann::json& h : manifest["heights"]) {
    const double x = h["x"], z = h["z"];
    worst = std::max({worst, std::abs(ember::groundHeightAt(board, ember::EdgeStyle::Smooth, x, z) - h["smooth"].get<double>()),
                      std::abs(ember::groundHeightAt(board, ember::EdgeStyle::Hard, x, z) - h["hard"].get<double>())});
  }
  logInfo("  heights: {} points, worst difference {:.2e}", manifest["heights"].size(), worst);
  ok = ok && worst < 1e-9;
  if (ok) logInfo("ground matches Ember");
  else logError("ground DIFFERS from Ember");
  return ok;
}

}  // namespace ember
