#include "world/Water3D.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>

#include "core/Log.h"

namespace ember::e2 {
namespace {

constexpr double kSqrt3 = 1.7320508075688772;
constexpr double kPad = 2.4;
constexpr double kStep = 0.1;
constexpr double kBoardPadMul = 2.4;  // ThreeBattleRenderer BOARD_PAD_MUL

double jsRound(double v) { return std::floor(v + 0.5); }  // Math.round
double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
int floorDiv2(double v) { return static_cast<int>(std::floor(v / 2)); }

}  // namespace

bool Water3DMap::any() const {
  if (!patches.empty()) return true;
  for (const auto& l : levels)
    if (l && std::isfinite(*l)) return true;
  return false;
}

Water3DMap Water3DMap::fromDraft(const nlohmann::json& d) {
  Water3DMap m;
  m.cols = d.contains("cols") && d["cols"].is_number() ? d["cols"].get<int>() : 0;
  m.rows = d.contains("rows") && d["rows"].is_number() ? d["rows"].get<int>() : 0;
  if (d.contains("tiles") && d["tiles"].is_array())
    for (const auto& t : d["tiles"]) m.tiles.push_back(t.is_string() ? t.get<std::string>() : std::string());
  if (d.contains("waterLevels") && d["waterLevels"].is_array())
    for (const auto& v : d["waterLevels"]) m.levels.push_back(v.is_number() ? std::optional<double>(v.get<double>()) : std::nullopt);
  if (d.contains("waterFootprints") && d["waterFootprints"].is_array())
    for (const auto& v : d["waterFootprints"]) {
      if (!v.is_object()) {
        m.footprints.push_back(std::nullopt);
        continue;
      }
      WaterFootprint f;
      if (v.contains("size") && v["size"].is_number()) f.size = v["size"].get<double>();
      f.square = v.contains("shape") && v["shape"].is_string() && v["shape"].get<std::string>() == "square";
      m.footprints.push_back(f);
    }
  if (d.contains("waterPatches") && d["waterPatches"].is_array())
    for (const auto& v : d["waterPatches"]) {
      if (!v.is_object()) continue;
      auto num = [&](const char* k) { return v.contains(k) && v[k].is_number() ? v[k].get<double>() : std::numeric_limits<double>::quiet_NaN(); };
      WaterPatch p{num("x"), num("y"), num("level"), num("size"), v.contains("shape") && v["shape"].is_string() && v["shape"].get<std::string>() == "square"};
      if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.level) || !std::isfinite(p.size)) continue;  // Engine2 skips them
      m.patches.push_back(p);
    }
  if (d.contains("squareTiles") && d["squareTiles"].is_boolean()) m.squareTiles = d["squareTiles"].get<bool>();
  if (d.contains("waterVersion") && d["waterVersion"].is_string()) m.version = d["waterVersion"].get<std::string>();
  return m;
}

WaterSample sampleWater(double x, double y, int cols, int rows, const std::vector<std::optional<double>>& levels,
                        const std::vector<std::optional<WaterFootprint>>& footprints, bool extendEdges) {
  const int row0 = static_cast<int>(jsRound((y - kPad - 1) / 1.5));
  double coverage = 0, weighted = 0, total = 0;
  for (int row = row0 - 1; row <= row0 + 1; row++) {
    if (!extendEdges && (row < 0 || row >= rows)) continue;
    const int sourceRow = std::max(0, std::min(rows - 1, row));
    const int col0 = static_cast<int>(jsRound(x / kSqrt3 - 0.5 * (row & 1) - 0.5));
    for (int col = col0 - 1; col <= col0 + 1; col++) {
      if (!extendEdges && (col < 0 || col >= cols)) continue;
      const int sourceCol = std::max(0, std::min(cols - 1, col));
      const size_t i = static_cast<size_t>(sourceRow) * cols + sourceCol;
      if (i >= levels.size() || !levels[i] || !std::isfinite(*levels[i])) continue;
      const double level = *levels[i];
      const double cx = kSqrt3 * (col + 0.5 * (row & 1) + 0.5), cy = kPad + 1.5 * row + 1;
      const std::optional<WaterFootprint>* footprint = i < footprints.size() ? &footprints[i] : nullptr;
      const bool has = footprint && footprint->has_value();
      const double size = clampd(has && (*footprint)->size ? *(*footprint)->size : 1.0, 0.25, 1.0);
      const double distance =
          (has && (*footprint)->square ? std::max(std::abs(x - cx), std::abs(y - cy)) : std::hypot(x - cx, y - cy)) / size;
      const double weight = std::max(0.0, 1 - distance / 2);
      if (!weight) continue;
      coverage = std::max(coverage, std::min(1.0, (1.25 - distance) / 0.2));
      weighted += clampd(level, 0, 12) * weight;
      total += weight;
    }
  }
  return {std::max(0.0, coverage), total ? weighted / total : 0};
}

WaterSample sampleWaterPatches(double x, double y, const std::vector<const WaterPatch*>& patches) {
  double coverage = 0, weighted = 0, total = 0;
  for (const WaterPatch* p : patches) {
    const double size = clampd(p->size, 0.25, 1.0);
    const double distance = (p->square ? std::max(std::abs(x - p->x), std::abs(y - p->y)) : std::hypot(x - p->x, y - p->y)) / size;
    const double weight = std::max(0.0, 1 - distance / 2);
    coverage = std::max(coverage, std::min(1.0, std::max(0.0, (1.25 - distance) / 0.2)));
    weighted += clampd(p->level, 0, 12) * weight;
    total += weight;
  }
  return {coverage, total ? weighted / total : 0};
}

bool hasSquareMapBorder(const std::vector<std::string>& tiles, int cols, int rows, std::optional<bool> squareTiles) {
  if (squareTiles) return *squareTiles;
  int minX = cols, minY = rows, maxX = -1, maxY = -1, count = 0;
  for (int y = 0; y < rows; y++)
    for (int x = 0; x < cols; x++) {
      const size_t i = static_cast<size_t>(y) * cols + x;
      if (i >= tiles.size() || tiles[i].empty() || tiles[i] == "void") continue;
      minX = std::min(minX, x), maxX = std::max(maxX, x), minY = std::min(minY, y), maxY = std::max(maxY, y);
      ++count;
    }
  return count > 0 && count == (maxX - minX + 1) * (maxY - minY + 1);
}

Water3DSheet buildWater3D(int cols, int rows, double tile, const std::vector<std::optional<double>>& levels,
                          const std::function<bool(int, int)>& allowed, const std::function<double(double, double)>& ground,
                          const std::vector<std::optional<WaterFootprint>>& footprints, const std::vector<WaterPatch>& patches,
                          const std::optional<Water3DBoard>& board) {
  Water3DSheet out;
  bool anyLevel = false;
  for (const auto& l : levels) anyLevel = anyLevel || (l && std::isfinite(*l));
  if (patches.empty() && !anyLevel) return out;
  double minX = std::numeric_limits<double>::infinity(), minY = minX, maxX = -minX, maxY = -minX;
  const double reach = board ? 1.4 + kSqrt3 : 1.4;
  for (int row = 0; row < rows; row++)
    for (int col = 0; col < cols; col++) {
      const size_t i = static_cast<size_t>(row) * cols + col;
      if (i >= levels.size() || !levels[i] || !std::isfinite(*levels[i]) || !allowed(col, row)) continue;
      const double x = kSqrt3 * (col + 0.5 * (row & 1) + 0.5), y = kPad + 1.5 * row + 1;
      minX = std::min(minX, x - reach), maxX = std::max(maxX, x + reach);
      minY = std::min(minY, y - reach), maxY = std::max(maxY, y + reach);
    }
  std::map<std::pair<int, int>, std::vector<const WaterPatch*>> buckets;  // keyed like Engine2's "bx:by" map
  for (const WaterPatch& p : patches) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.level) || !std::isfinite(p.size)) continue;
    minX = std::min(minX, p.x - 1.4), maxX = std::max(maxX, p.x + 1.4);
    minY = std::min(minY, p.y - 1.4), maxY = std::max(maxY, p.y + 1.4);
    buckets[{floorDiv2(p.x), floorDiv2(p.y)}].push_back(&p);
  }
  if (board) {
    minX = std::max(minX, board->minX), maxX = std::min(maxX, board->maxX);
    minY = std::max(minY, board->minY), maxY = std::min(maxY, board->maxY);
  }
  if (!std::isfinite(minX) || minX >= maxX || minY >= maxY) return out;
  const int nx = static_cast<int>(std::ceil((maxX - minX) / kStep)), ny = static_cast<int>(std::ceil((maxY - minY) / kStep));
  const size_t count = static_cast<size_t>(nx + 1) * (ny + 1);
  out.position.reserve(count * 3), out.uv.reserve(count * 2), out.coverage.reserve(count), out.depth.reserve(count);
  std::vector<const WaterPatch*> nearby;
  for (int iy = 0; iy <= ny; iy++)
    for (int ix = 0; ix <= nx; ix++) {
      const double x = board ? std::min(maxX, minX + ix * kStep) : minX + ix * kStep;
      const double y = board ? std::min(maxY, minY + iy * kStep) : minY + iy * kStep;
      WaterSample sample = sampleWater(x, y, cols, rows, levels, footprints, board.has_value());
      nearby.clear();
      const int bx = floorDiv2(x), by = floorDiv2(y);
      // Engine2 visits the nine buckets row by row (dy outer), each in insertion order: the sum's order matters
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          const auto it = buckets.find({bx + dx, by + dy});
          if (it != buckets.end()) nearby.insert(nearby.end(), it->second.begin(), it->second.end());
        }
      const WaterSample free = sampleWaterPatches(x, y, nearby);
      if (free.coverage > sample.coverage) sample = free;
      double best = std::numeric_limits<double>::infinity();
      int bc = -1, br = -1;
      const int row0 = static_cast<int>(jsRound((y - kPad - 1) / 1.5));
      for (int row = row0 - 1; row <= row0 + 1; row++) {
        const int col0 = static_cast<int>(jsRound(x / kSqrt3 - 0.5 * (row & 1) - 0.5));
        for (int col = col0 - 1; col <= col0 + 1; col++) {
          const double ddx = x - kSqrt3 * (col + 0.5 * (row & 1) + 0.5), ddy = y - kPad - 1.5 * row - 1;
          const double distance = ddx * ddx + ddy * ddy;
          if (distance < best) best = distance, bc = col, br = row;
        }
      }
      const bool valid = board ? allowed(std::max(0, std::min(cols - 1, bc)), std::max(0, std::min(rows - 1, br)))
                               : bc >= 0 && br >= 0 && bc < cols && br < rows && allowed(bc, br);
      const double z = (sample.level * 0.65 + 0.035) * tile;
      out.position.insert(out.position.end(), {static_cast<float>(x * tile), static_cast<float>(-y * tile), static_cast<float>(z)});
      out.uv.insert(out.uv.end(), {static_cast<float>(x * 0.25), static_cast<float>(-y * 0.25)});
      out.coverage.push_back(static_cast<float>(valid ? sample.coverage : 0));
      out.depth.push_back(static_cast<float>(std::max(0.0, (z - ground(x * tile, -y * tile)) / tile)));
    }
  for (int iy = 0; iy < ny; iy++)
    for (int ix = 0; ix < nx; ix++) {
      const uint32_t a = static_cast<uint32_t>(iy * (nx + 1) + ix), b = a + 1, c = a + nx + 1, d = c + 1;
      // the sum of four float32 coverages, as JS adds them (in double)
      if (static_cast<double>(out.coverage[a]) + out.coverage[b] + out.coverage[c] + out.coverage[d] > 0)
        out.indices.insert(out.indices.end(), {a, c, b, b, c, d});
    }
  return out;
}

Water3DSheet buildWater3D(const Water3DMap& map, const std::function<double(double, double)>& ground) {
  std::optional<Water3DBoard> board;
  if (hasSquareMapBorder(map.tiles, map.cols, map.rows, map.squareTiles))
    board = Water3DBoard{0, kSqrt3 * map.cols, kBoardPadMul + 0.25, kBoardPadMul + 0.25 + map.rows * 1.5};
  auto allowed = [&](int col, int row) {
    const size_t i = static_cast<size_t>(row) * map.cols + col;
    return i < map.tiles.size() && map.tiles[i] != "void";
  };
  return buildWater3D(map.cols, map.rows, 1.0, map.levels, allowed, ground, map.footprints, map.patches, board);
}

int checkWater3D(const std::string& refsPath) {
  std::ifstream in(refsPath, std::ios::binary);
  if (!in) {
    logError("check-water3d: cannot read {}", refsPath);
    return 1;
  }
  const nlohmann::json refs = nlohmann::json::parse(in);
  auto ground = [](double x, double y) { return 0.3 * std::sin(0.7 * x) + 0.2 * std::cos(0.5 * y) + 0.15; };
  int failures = 0;
  size_t vertices = 0, triangles = 0;
  for (const nlohmann::json& c : refs["cases"]) {
    nlohmann::json draft;
    draft["cols"] = c["cols"], draft["rows"] = c["rows"], draft["tiles"] = c["tiles"];
    draft["waterLevels"] = c["levels"], draft["waterFootprints"] = c["footprints"], draft["waterPatches"] = c["patches"];
    if (c["squareTiles"].is_boolean()) draft["squareTiles"] = c["squareTiles"];
    const Water3DMap map = Water3DMap::fromDraft(draft);
    const Water3DSheet s = buildWater3D(map, ground);
    const std::string name = c["map"].get<std::string>() + " " + c["variant"].get<std::string>() + " square=" + (c["square"].get<bool>() ? "1" : "0");
    auto compare = [&](const char* what, const std::vector<float>& mine, const nlohmann::json& theirs) {
      if (mine.size() != theirs.size()) {
        logError("{}: {} has {} values, Engine2 {}", name, what, mine.size(), theirs.size());
        return false;
      }
      for (size_t k = 0; k < mine.size(); ++k)
        if (mine[k] != theirs[k].get<float>()) {
          logError("{}: {}[{}] = {} vs Engine2 {}", name, what, k, mine[k], theirs[k].get<float>());
          return false;
        }
      return true;
    };
    bool ok = compare("position", s.position, c["position"]) && compare("uv", s.uv, c["uv"]) &&
              compare("coverage", s.coverage, c["coverage"]) && compare("waterDepth", s.depth, c["waterDepth"]);
    if (ok) {
      const std::vector<uint32_t> theirs = c["index"].get<std::vector<uint32_t>>();
      if (theirs != s.indices) {
        logError("{}: triangles differ ({} vs Engine2 {} indices)", name, s.indices.size(), theirs.size());
        ok = false;
      }
    }
    if (!ok) ++failures;
    vertices += s.coverage.size(), triangles += s.indices.size() / 3;
  }
  if (failures) logError("check-water3d: {} of {} cases differ", failures, refs["cases"].size());
  else logInfo("check-water3d: {} cases, {} vertices, {} triangles: water matches Engine2", refs["cases"].size(), vertices, triangles);
  return failures;
}

}  // namespace ember::e2
