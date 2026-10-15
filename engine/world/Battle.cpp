#include "world/Battle.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Log.h"

namespace ember::e2 {

namespace {

using json = nlohmann::json;

int integer(const json& j, const char* key, int fallback = 0) { return j.contains(key) && j[key].is_number() ? j[key].get<int>() : fallback; }
std::string text(const json& j, const char* key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); }
bool yes(const json& j, const char* key) { return j.contains(key) && j[key].is_boolean() && j[key].get<bool>(); }

// pathfinding.ts footprintFrontRow for a unit with no authored shape: `width` hexes centred on (x, y).
void frontRow(int x, int y, int width, std::vector<BattleHex>& out) {
  const int start = x - (width - 1) / 2;
  for (int i = 0; i < width; ++i) out.push_back({start + i, y});
}

// pathfinding.ts footprint for a body of `size` with an optional authored shape.
std::vector<BattleHex> bodyAt(int size, const std::optional<std::vector<BattleHex>>& offsets, int width, int height, int x, int y) {
  const int s = std::max(1, size);
  if (s <= 1) return {{x, y}};
  std::vector<BattleHex> out;
  if (offsets) {
    // Offsets are authored for an even anchor row; on an odd one an odd-dy row shifts one column right.
    const int shift = y & 1;
    for (const BattleHex& o : *offsets) out.push_back({x + o.x + ((o.y & 1) ? shift : 0), y + o.y});
    return out;
  }
  if (s >= 4) {
    for (int dy = 0; dy > -height; --dy) frontRow(dy % 2 != 0 ? x - 1 : x, y + dy, width, out);
    return out;
  }
  out.push_back({x, y});
  const size_t want = s <= 2 ? 2 : static_cast<size_t>(std::min(4, s));
  for (const BattleHex& n : Battle::neighbors(x, y)) {
    out.push_back(n);
    if (out.size() >= want) break;
  }
  return out;
}

bool fliesOverWater(const BattleUnit& u, const BattleCell& c) {
  // Only these two classes cross water; a blocking decoration on the water still stops them.
  return c.terrain == "water" && (u.classId == "swampBlueCalf" || u.classId == "roccoTheBird") && !c.blocked;
}

bool isTroll(const BattleUnit& u) { return u.classId == "troll" || u.classId == "troll2"; }

}  // namespace

std::optional<Battle> Battle::load(const std::filesystem::path& battleJson) {
  std::ifstream in(battleJson, std::ios::binary);
  const json j = in ? json::parse(in, nullptr, false) : json();
  if (!in || j.is_discarded() || !j.contains("cells") || !j.contains("units")) {
    logError("battle: cannot read {} (export it with tools/scripts/engine2/export_battle.ts)", battleJson.string());
    return std::nullopt;
  }
  Battle b;
  b.map_ = text(j, "map");
  b.cols_ = integer(j, "cols");
  b.rows_ = integer(j, "rows");
  for (const json& c : j["cells"]) {
    BattleCell o;
    o.terrain = text(c, "terrain");
    o.passable = yes(c, "passable");
    o.moveCost = integer(c, "moveCost", 1);
    o.def = integer(c, "def");
    o.atk = integer(c, "atk");
    o.blocked = yes(c, "blocked");
    b.cells_.push_back(std::move(o));
  }
  if (b.cells_.size() != static_cast<size_t>(b.cols_ * b.rows_)) {
    logError("battle: {} has {} cells for a {}x{} board", battleJson.string(), b.cells_.size(), b.cols_, b.rows_);
    return std::nullopt;
  }
  for (const json& u : j["units"]) {
    BattleUnit o;
    o.id = text(u, "id");
    o.side = text(u, "side");
    o.name = text(u, "name");
    o.classId = text(u, "classId");
    o.sprite = text(u, "sprite");
    o.x = integer(u, "x");
    o.y = integer(u, "y");
    o.alive = yes(u, "alive");
    o.mov = integer(u, "mov");
    o.hp = integer(u, "hp");
    o.maxHp = integer(u, "maxHp");
    o.atk = integer(u, "atk");
    o.def = integer(u, "def");
    o.mag = integer(u, "mag");
    o.dex = integer(u, "dex");
    o.minRange = integer(u, "minRange", 1);
    o.maxRange = integer(u, "maxRange", 1);
    o.size = integer(u, "size", 1);
    if (u.contains("footprintOffsets") && u["footprintOffsets"].is_array()) {
      std::vector<BattleHex> offsets;
      for (const json& f : u["footprintOffsets"]) offsets.push_back({integer(f, "dx"), integer(f, "dy")});
      o.footprintOffsets = std::move(offsets);
    }
    o.footprintW = integer(u, "footprintW", 2);
    o.footprintH = integer(u, "footprintH", 4);
    if (u.contains("reach") && u["reach"].is_array())
      for (const json& r : u["reach"])
        if (r.is_array() && r.size() == 5)
          o.reference.push_back({r[0].get<int>(), r[1].get<int>(), r[2].get<int>(), r[3].get<int>(), r[4].get<int>()});
    b.units_.push_back(std::move(o));
  }
  return b;
}

std::vector<BattleHex> Battle::neighbors(int x, int y) {
  static constexpr int even[6][2] = {{1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}};
  static constexpr int odd[6][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {0, 1}, {1, 1}};
  const auto& d = (y & 1) ? odd : even;
  std::vector<BattleHex> out;
  out.reserve(6);
  for (const auto& o : d) out.push_back({x + o[0], y + o[1]});
  return out;
}

int Battle::distance(BattleHex a, BattleHex b) {
  const int aq = a.x - (a.y - (a.y & 1)) / 2, bq = b.x - (b.y - (b.y & 1)) / 2;
  const int dq = aq - bq, dr = a.y - b.y;
  return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

std::vector<BattleHex> Battle::footprint(const BattleUnit& u, int x, int y) {
  return bodyAt(u.size, u.footprintOffsets, u.footprintW, u.footprintH, x, y);
}

std::vector<int> Battle::occupancy() const {
  std::vector<int> occ(cells_.size(), -1);
  for (size_t i = 0; i < units_.size(); ++i) {
    if (!units_[i].alive) continue;
    for (const BattleHex& p : footprint(units_[i], units_[i].x, units_[i].y))
      if (inBounds(p.x, p.y)) occ[static_cast<size_t>(p.y * cols_ + p.x)] = static_cast<int>(i);  // a later unit wins, as there
  }
  return occ;
}

int Battle::unitAt(int x, int y) const {
  for (size_t i = 0; i < units_.size(); ++i) {
    if (!units_[i].alive) continue;
    for (const BattleHex& p : footprint(units_[i], units_[i].x, units_[i].y))
      if (p.x == x && p.y == y) return static_cast<int>(i);
  }
  return -1;
}

// pathfinding.ts footprintCost: what stepping the unit's anchor onto (x, y) costs, or nothing when it cannot.
std::optional<int> Battle::stepCost(int x, int y, const BattleUnit& self, const std::vector<int>& occ, bool stop) const {
  // A body with an authored shape checks only its front row for movement (its trailing rows never turn with the
  // walk, and would lock it in place near an edge); others check their whole body (the default size rectangle).
  std::vector<BattleHex> body;
  if (self.footprintOffsets) {
    for (const BattleHex& o : *self.footprintOffsets)
      if (o.y == 0) body.push_back({x + o.x, y});
  } else {
    body = bodyAt(self.size, std::nullopt, 2, 4, x, y);
  }
  int cost = 1;
  for (const BattleHex& p : body) {
    if (!inBounds(p.x, p.y)) return std::nullopt;
    const BattleCell& c = cell(p.x, p.y);
    const bool flies = fliesOverWater(self, c);
    if (!c.passable && !flies && !(c.terrain == "barricade" && isTroll(self))) return std::nullopt;
    const int here = flies ? 1 : c.terrain == "barricade" && isTroll(self) ? 2 : c.moveCost;
    cost = std::max(cost, here);
    const int who = occ[static_cast<size_t>(p.y * cols_ + p.x)];
    if (who < 0 || units_[static_cast<size_t>(who)].id == self.id) continue;
    const BattleUnit& other = units_[static_cast<size_t>(who)];
    if (other.side != self.side) return std::nullopt;
    // A big creature's body is never walked into or through, friend or foe.
    if (other.footprintOffsets) return std::nullopt;
    if (stop) return std::nullopt;
  }
  return cost;
}

std::vector<ReachCell> Battle::reachable(size_t unit, bool pruneStops) const {
  const BattleUnit& u = units_[unit];
  const int mov = std::max(0, u.mov - u.moveUsed);
  const std::vector<int> occ = occupancy();
  // The reach in Engine2's insertion order (a Map there): `at` finds a hex's entry.
  std::vector<ReachCell> result;
  std::vector<int> at(cells_.size(), -1);
  auto slot = [&](int x, int y) -> int* { return inBounds(x, y) ? &at[static_cast<size_t>(y * cols_ + x)] : nullptr; };
  result.push_back({u.x, u.y, 0, -1, -1});
  if (int* s = slot(u.x, u.y)) *s = 0;
  // Dijkstra exactly as there: the queue re-sorted by cost before every pop (a stable sort: ties keep their order).
  std::vector<ReachCell> queue{result.front()};
  while (!queue.empty()) {
    std::stable_sort(queue.begin(), queue.end(), [](const ReachCell& a, const ReachCell& b) { return a.cost < b.cost; });
    const ReachCell cur = queue.front();
    queue.erase(queue.begin());
    if (cur.cost >= mov) continue;
    for (const BattleHex& n : neighbors(cur.x, cur.y)) {
      const std::optional<int> step = stepCost(n.x, n.y, u, occ, false);
      if (!step) continue;
      const int next = cur.cost + *step;
      if (next > mov) continue;
      int* s = slot(n.x, n.y);  // a legal step is always on the board
      if (*s >= 0 && result[static_cast<size_t>(*s)].cost <= next) continue;
      const ReachCell c{n.x, n.y, next, cur.x, cur.y};
      if (*s >= 0) {
        result[static_cast<size_t>(*s)] = c;  // Map.set keeps the key's place
      } else {
        *s = static_cast<int>(result.size());
        result.push_back(c);
      }
      queue.push_back(c);
    }
  }
  if (!pruneStops) return result;
  // Hexes it may pass through but not stop on (a friend stands there) leave the reach.
  std::vector<ReachCell> kept;
  for (const ReachCell& c : result)
    if ((c.x == u.x && c.y == u.y) || stepCost(c.x, c.y, u, occ, true)) kept.push_back(c);
  return kept;
}

std::vector<BattleHex> Battle::path(const std::vector<ReachCell>& reach, int x, int y) {
  std::vector<BattleHex> out;
  auto find = [&](int cx, int cy) -> const ReachCell* {
    for (const ReachCell& c : reach)
      if (c.x == cx && c.y == cy) return &c;
    return nullptr;
  };
  for (const ReachCell* c = find(x, y); c && out.size() <= reach.size(); c = c->parentX >= 0 ? find(c->parentX, c->parentY) : nullptr)
    out.push_back({c->x, c->y});
  std::reverse(out.begin(), out.end());
  return out;
}

bool Battle::move(size_t unit, int x, int y) {
  BattleUnit& u = units_[unit];
  for (const ReachCell& c : reachable(unit))
    if (c.x == x && c.y == y) {
      u.x = x, u.y = y;
      u.moveUsed += c.cost;
      return true;
    }
  return false;
}

void Battle::newTurn() {
  for (BattleUnit& u : units_) u.moveUsed = 0;
}

}  // namespace ember::e2
