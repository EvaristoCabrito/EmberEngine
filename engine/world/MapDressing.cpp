#include "world/MapDressing.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>
#include <optional>

#include "core/Log.h"

namespace ember::e2 {

namespace {

using ojson = nlohmann::ordered_json;

struct Cell {
  int x = 0, y = 0;
};
std::string key(int x, int y) { return std::format("{},{}", x, y); }

// data.ts hexAdj: the six odd-r neighbours, in Engine2's order (it matters to nothing but is kept).
std::vector<Cell> hexAdj(int x, int y) {
  static const int even[6][2] = {{1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}};
  static const int odd[6][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {0, 1}, {1, 1}};
  const auto& d = (y & 1) ? odd : even;
  std::vector<Cell> out;
  for (const auto& o : d) out.push_back({x + o[0], y + o[1]});
  return out;
}
int oddrDist(int ax, int ay, int bx, int by) {
  const int aq = ax - (ay - (ay & 1)) / 2, bq = bx - (by - (by & 1)) / 2;
  return (std::abs(aq - bq) + std::abs(ay - by) + std::abs(-aq - ay - (-bq - by))) / 2;
}

// The draft as the passes see it (Engine2's Mission: the layout, spawns, decorations).
struct Mission {
  std::string id;
  int index = 0, cols = 0, rows = 0;
  bool hub = false;
  std::vector<std::string> layout;  // one string per row, Engine2's terrain letters
  std::vector<Cell> player, enemy, neutral;
  ojson decorations = ojson::array();
};

std::vector<Cell> spawnsOf(const ojson& d, const char* k) {
  std::vector<Cell> out;
  if (d.contains(k) && d[k].is_array())
    for (const ojson& s : d[k]) out.push_back({s.value("x", 0), s.value("y", 0)});
  return out;
}

class Dresser {
 public:
  Dresser(const DressTables& t, const EditorCatalog& c) : t_(t), c_(c) {
    for (const auto& [terrain, ch] : t_.tileChar)
      if (!ch.empty()) char_[ch[0]] = terrain;
  }

  std::string terrainOf(char ch) const {
    const auto it = char_.find(ch);
    return it == char_.end() ? std::string("plains") : it->second;
  }
  char charOf(const std::string& terrain) const {
    const auto it = t_.tileChar.find(terrain);
    return it == t_.tileChar.end() || it->second.empty() ? '.' : it->second[0];
  }
  bool passable(const std::string& terrain) const {
    const auto it = t_.passable.find(terrain);
    return it != t_.passable.end() && it->second;
  }
  std::vector<std::string> parseLayout(const std::vector<std::string>& layout) const {
    std::vector<std::string> tiles;
    for (const std::string& row : layout)
      for (char ch : row) tiles.push_back(terrainOf(ch));
    return tiles;
  }
  std::vector<FootOffset> footprint(const ojson& p) const {
    const CatalogDecoration* def = c_.decoration(p.value("id", ""));
    if (!def) return {};
    const double rot = p.contains("rot") && p["rot"].is_number() ? p["rot"].get<double>() : 0.0;
    return placedFootprint(*def, p.value("x", 0), p.value("y", 0), rot);
  }
  std::set<std::string> decorationCells(const ojson& placements) const {
    std::set<std::string> out;
    for (const ojson& p : placements)
      for (const FootOffset& f : footprint(p)) out.insert(key(p.value("x", 0) + f.dx, p.value("y", 0) + f.dy));
    return out;
  }

  // watchtowerDungeon.ts closeWatchtowerWalls with no placements (what Gerar terreno hands draftToMission): the floor's
  // boundary walled, diagonal ends joined.
  ojson watchtowerWalls(const std::string& id, const std::vector<std::string>& tiles, int cols, int rows) const {
    ojson result = ojson::array();
    if (!id.starts_with("watchtower-")) return result;
    auto floor = [&](int x, int y) { return x >= 0 && y >= 0 && x < cols && y < rows && tiles[y * cols + x] != "void"; };
    auto boundary = [&](int x, int y) { return floor(x, y) && (!floor(x - 1, y) || !floor(x + 1, y) || !floor(x, y - 1) || !floor(x, y + 1)); };
    std::set<std::string> occupied;
    const std::string wallId = "wall-3d-dungeon";
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < cols; ++x)
        if (boundary(x, y) && !occupied.contains(key(x, y))) {
          result.push_back(ojson{{"id", wallId}, {"x", x}, {"y", y}});
          occupied.insert(key(x, y));
        }
    auto degree = [&](int x, int y) {
      return static_cast<int>(occupied.contains(key(x - 1, y))) + occupied.contains(key(x + 1, y)) + occupied.contains(key(x, y - 1)) + occupied.contains(key(x, y + 1));
    };
    std::vector<Cell> ends;
    for (const ojson& p : result) {
      const DressTables::Deco* d = t_.deco(p.value("id", ""));
      if (d && d->model3d == "wall" && degree(p.value("x", 0), p.value("y", 0)) < 2) ends.push_back({p.value("x", 0), p.value("y", 0)});
    }
    for (const Cell& a : ends)
      for (const Cell& b : ends) {
        if (std::abs(a.x - b.x) != 1 || std::abs(a.y - b.y) != 1) continue;
        for (const Cell& j : {Cell{a.x, b.y}, Cell{b.x, a.y}})
          if (floor(j.x, j.y) && !occupied.contains(key(j.x, j.y))) {
            result.push_back(ojson{{"id", wallId}, {"x", j.x}, {"y", j.y}});
            occupied.insert(key(j.x, j.y));
            break;
          }
      }
    return result;
  }

  // data.ts clearRockColumnTiles.
  std::vector<std::string> clearRockColumnTiles(std::vector<std::string> tiles, int cols, int rows, const ojson& decorations,
                                                const std::optional<std::string>& baseTile) const {
    const bool hasNave = std::find(tiles.begin(), tiles.end(), "nave") != tiles.end();
    const bool hasSnow = std::find(tiles.begin(), tiles.end(), "snow") != tiles.end();
    const std::string ground = baseTile && passable(*baseTile) && *baseTile != "water" && *baseTile != "void" ? *baseTile
                               : hasNave                                                                       ? "nave"
                               : hasSnow                                                                       ? "snow"
                                                                                                               : "plains";
    for (const ojson& p : decorations) {
      const DressTables::Deco* d = t_.deco(p.value("id", ""));
      if (!d || !(d->solidRock || (d->rockStyle && d->model3d == "wall"))) continue;
      for (const FootOffset& f : footprint(p)) {
        const int x = p.value("x", 0) + f.dx, y = p.value("y", 0) + f.dy;
        if (x < 0 || y < 0 || x >= cols || y >= rows) continue;
        if (tiles[y * cols + x] == "column") tiles[y * cols + x] = ground;
      }
    }
    return tiles;
  }

  // MapEditorScreen's draftToMission({...draft, autoTactics: true, decorations: []}) (mapstore normalizeDraft first).
  Mission missionOf(const ojson& d) const {
    Mission m;
    m.id = d.value("id", "");
    m.index = d.value("index", 0);
    m.cols = d.value("cols", 0), m.rows = d.value("rows", 0);
    m.hub = d.contains("hub") && d["hub"].is_boolean() && d["hub"].get<bool>();
    std::vector<std::string> tiles;
    for (const ojson& t : d["tiles"]) tiles.push_back(t.is_string() ? t.get<std::string>() : "plains");
    tiles.resize(static_cast<size_t>(m.cols * m.rows), "plains");
    const std::optional<std::string> base = d.contains("baseTile") && d["baseTile"].is_string() ? std::optional(d["baseTile"].get<std::string>()) : std::nullopt;
    m.decorations = watchtowerWalls(m.id, tiles, m.cols, m.rows);
    tiles = clearRockColumnTiles(clearRockColumnTiles(tiles, m.cols, m.rows, m.decorations, base), m.cols, m.rows, m.decorations, base);
    // draftToMission writes TILE_CHAR[tile] with no fallback: a retired terrain id becomes the text "undefined", which
    // the passes then read as nine letters (scatterTactics re-squares the rows afterwards). Kept, as Engine2 does it.
    for (int r = 0; r < m.rows; ++r) {
      std::string row;
      for (int c = 0; c < m.cols; ++c) {
        const std::string& t = tiles[r * m.cols + c];
        row += t_.tileChar.contains(t) ? std::string(1, charOf(t)) : std::string("undefined");
      }
      m.layout.push_back(row);
    }
    m.player = spawnsOf(d, "playerSpawns"), m.enemy = spawnsOf(d, "enemySpawns"), m.neutral = spawnsOf(d, "neutralSpawns");
    return m;
  }

  // data.ts scatterTactics: up to three barricade walls between the two sides (no hills are placed any more).
  void scatterTactics(Mission& m) const {
    std::vector<std::string> tiles = parseLayout(m.layout);
    std::set<std::string> blocked;
    for (const auto* list : {&m.player, &m.enemy, &m.neutral})
      for (const Cell& s : *list) {
        blocked.insert(key(s.x, s.y));
        for (const Cell& n : hexAdj(s.x, s.y)) blocked.insert(key(n.x, n.y));
      }
    std::vector<Cell> cand;
    for (int y = 1; y < m.rows - 1; ++y)
      for (int x = 1; x < m.cols - 1; ++x) {
        const std::string& t = tiles[y * m.cols + x];
        if ((t == "plains" || t == "nave" || t == "woods") && !blocked.contains(key(x, y))) cand.push_back({x, y});
      }
    uint32_t seed = static_cast<uint32_t>((m.index + 1) * 9973);
    auto rnd = [&] {
      seed = seed * 1664525u + 1013904223u;
      return static_cast<double>(seed) / 4294967296.0;
    };
    for (int i = static_cast<int>(cand.size()) - 1; i > 0; --i) {
      const int j = static_cast<int>(std::floor(rnd() * (i + 1)));
      std::swap(cand[static_cast<size_t>(i)], cand[static_cast<size_t>(j)]);
    }
    std::vector<Cell> taken;
    auto canWalk = [&](const std::vector<std::string>& now) {
      if (m.player.empty() || m.enemy.empty()) return true;
      const Cell from = m.player[0], to = m.enemy[0];
      std::set<std::string> seen{key(from.x, from.y)};
      std::vector<Cell> q{from};
      while (!q.empty()) {
        const Cell p = q.back();
        q.pop_back();
        if (oddrDist(p.x, p.y, to.x, to.y) <= 1) return true;
        for (const Cell& n : hexAdj(p.x, p.y)) {
          if (n.x < 0 || n.y < 0 || n.x >= m.cols || n.y >= m.rows) continue;
          if (seen.contains(key(n.x, n.y)) || !passable(now[n.y * m.cols + n.x])) continue;
          seen.insert(key(n.x, n.y));
          q.push_back(n);
        }
      }
      return false;
    };
    auto okBase = [&](int x, int y) {
      if (x < 1 || y < 1 || x >= m.cols - 1 || y >= m.rows - 1 || blocked.contains(key(x, y))) return false;
      const std::string& t = tiles[y * m.cols + x];
      return t == "plains" || t == "nave" || t == "woods";
    };
    auto avg = [](const std::vector<Cell>& list) {
      double sx = 0, sy = 0;
      for (const Cell& p : list) sx += p.x, sy += p.y;
      const double n = std::max<double>(1, static_cast<double>(list.size()));
      return std::pair{sx / n, sy / n};
    };
    const auto [px, py] = avg(m.player);
    const auto [ex, ey] = avg(m.enemy);
    const double frontX = (px + ex) / 2, frontY = (py + ey) / 2, minY = std::min(py, ey), maxY = std::max(py, ey);
    struct Wall {
      std::vector<Cell> cells;
      double score = 0;
    };
    std::vector<Wall> walls;
    static const int cubeDirs[6][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1}};
    for (const Cell& p : cand) {
      if (!okBase(p.x, p.y)) continue;
      const int aq = p.x - (p.y - (p.y & 1)) / 2, ar = p.y;
      for (const auto& dir : cubeDirs) {
        std::vector<Cell> cells{p};
        int q = aq, r = ar;
        bool good = true;
        for (int k = 0; k < 2; ++k) {
          q += dir[0], r += dir[1];
          const Cell n{q + (r - (r & 1)) / 2, r};
          if (!okBase(n.x, n.y)) {
            good = false;
            break;
          }
          cells.push_back(n);
        }
        if (!good) continue;
        const double mx = static_cast<double>(cells[0].x + cells[1].x + cells[2].x) / 3, my = static_cast<double>(cells[0].y + cells[1].y + cells[2].y) / 3;
        const bool between = my > minY + 1.2 && my < maxY - 1.2;
        const double sameRow = cells[1].y == cells[0].y && cells[2].y == cells[0].y ? 3 : 0;
        const double dFront = std::abs(mx - frontX) * 0.3 + std::abs(my - frontY);
        const double central = 1 - std::abs(mx - (m.cols - 1) / 2.0) / (m.cols / 2.0);
        walls.push_back({cells, (between ? 12 : 0) + sameRow + central * 4 - dFront});
      }
    }
    const double density = static_cast<double>(m.cols * m.rows) / 288;
    const int wantWalls = std::min(3, std::max(1, static_cast<int>(std::floor(2 * density + 0.5))));
    std::vector<Cell> wallCenters;
    std::vector<Wall> remaining = walls;
    int placed = 0;
    auto center = [](const Wall& w) {
      return Cell{static_cast<int>(std::floor((w.cells[0].x + w.cells[1].x + w.cells[2].x) / 3.0 + 0.5)),
                  static_cast<int>(std::floor((w.cells[0].y + w.cells[1].y + w.cells[2].y) / 3.0 + 0.5))};
    };
    while (placed < wantWalls && !remaining.empty()) {
      int bestIdx = -1;
      double bestScore = -std::numeric_limits<double>::infinity();
      for (size_t idx = 0; idx < remaining.size(); ++idx) {
        const Wall& w = remaining[idx];
        if (std::any_of(w.cells.begin(), w.cells.end(), [&](const Cell& c) {
              return std::any_of(taken.begin(), taken.end(), [&](const Cell& q) { return oddrDist(c.x, c.y, q.x, q.y) < 3; });
            }))
          continue;
        const Cell mc = center(w);
        double spread = 0;
        if (!wallCenters.empty()) {
          spread = std::numeric_limits<double>::infinity();
          for (const Cell& p : wallCenters) spread = std::min<double>(spread, oddrDist(mc.x, mc.y, p.x, p.y));
        }
        const double eff = w.score + spread * 2.2;
        if (eff > bestScore) bestScore = eff, bestIdx = static_cast<int>(idx);
      }
      if (bestIdx < 0) break;
      const Wall w = remaining[static_cast<size_t>(bestIdx)];
      remaining.erase(remaining.begin() + bestIdx);
      std::vector<std::string> prev;
      for (const Cell& c : w.cells) prev.push_back(tiles[c.y * m.cols + c.x]);
      for (const Cell& c : w.cells) tiles[c.y * m.cols + c.x] = "barricade";
      if (!canWalk(tiles)) {
        for (size_t i = 0; i < w.cells.size(); ++i) tiles[w.cells[i].y * m.cols + w.cells[i].x] = prev[i];
        continue;
      }
      taken.insert(taken.end(), w.cells.begin(), w.cells.end());
      wallCenters.push_back(center(w));
      ++placed;
    }
    m.layout.clear();
    for (int y = 0; y < m.rows; ++y) {
      std::string row;
      for (int x = 0; x < m.cols; ++x) row += charOf(tiles[y * m.cols + x]);
      m.layout.push_back(row);
    }
  }

  // data.ts rockifyColumns: column pairs become rocks; every column becomes floor.
  void rockifyColumns(Mission& m) const {
    if (m.hub) return;
    std::vector<std::string>& grid = m.layout;
    const bool indoor = std::any_of(grid.begin(), grid.end(), [](const std::string& r) { return r.find('n') != std::string::npos; });
    const char fallback = indoor ? 'n' : '.';
    std::set<std::string> claimed;
    for (int y = 0; y < m.rows; ++y)
      for (int x = 0; x < m.cols - 1; ++x) {
        if (grid[y][x] != 'c' || claimed.contains(key(x, y))) continue;
        if (grid[y][x + 1] != 'c' || claimed.contains(key(x + 1, y))) continue;
        claimed.insert(key(x, y)), claimed.insert(key(x + 1, y));
        m.decorations.push_back(ojson{{"id", "spike-rocks"}, {"x", x}, {"y", y}});
      }
    for (std::string& row : grid)
      for (char& ch : row)
        if (ch == 'c') ch = fallback;
  }

  // data.ts connectivityOk.
  bool connectivityOk(const std::vector<std::string>& grid, int cols, int rows, const std::set<std::string>& blocked, const std::vector<Cell>& spawns) const {
    if (spawns.empty()) return true;
    auto pass = [&](int x, int y) { return !blocked.contains(key(x, y)) && passable(terrainOf(grid[y][x])); };
    std::set<std::string> seen{key(spawns[0].x, spawns[0].y)};
    std::vector<Cell> stack{spawns[0]};
    while (!stack.empty()) {
      const Cell c = stack.back();
      stack.pop_back();
      for (const Cell& n : hexAdj(c.x, c.y))
        if (n.x >= 0 && n.x < cols && n.y >= 0 && n.y < rows && !seen.contains(key(n.x, n.y)) && pass(n.x, n.y)) {
          seen.insert(key(n.x, n.y));
          stack.push_back(n);
        }
    }
    return std::all_of(spawns.begin(), spawns.end(), [&](const Cell& s) { return seen.contains(key(s.x, s.y)); });
  }

  static uint32_t seedFromId(const std::string& id) {
    uint32_t h = 2166136261u;
    for (unsigned char c : id) h ^= c, h *= 16777619u;
    return h;
  }

  // data.ts placeChests + decorateOpenTerrain: one to three locked chests toward the enemy, never cutting a spawn off.
  void decorateOpenTerrain(Mission& m) const {
    if (m.hub) return;
    const int cols = m.cols, rows = m.rows;
    std::vector<std::string>& grid = m.layout;
    std::set<std::string> spawnSet;
    for (const auto* list : {&m.player, &m.enemy})
      for (const Cell& s : *list) spawnSet.insert(key(s.x, s.y));
    const bool indoor = std::any_of(grid.begin(), grid.end(), [](const std::string& r) { return r.find('n') != std::string::npos; });
    const char floorChar = indoor ? 'n' : '.';
    std::set<std::string> blockedExtra;
    const bool authored = std::any_of(m.decorations.begin(), m.decorations.end(), [&](const ojson& d) {
      const DressTables::Deco* x = t_.deco(d.value("id", ""));
      return x && x->chest;
    });
    if (!authored) {
      Mulberry32 rng(seedFromId(m.id));
      std::vector<Cell> spawns = m.player;
      spawns.insert(spawns.end(), m.enemy.begin(), m.enemy.end());
      std::vector<Cell> candidates;
      for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
          if (!spawnSet.contains(key(x, y)) && !blockedExtra.contains(key(x, y)) && grid[y][x] == floorChar) candidates.push_back({x, y});
      auto minDist = [](const Cell& c, const std::vector<Cell>& pts) {
        double best = std::numeric_limits<double>::infinity();
        for (const Cell& p : pts) best = std::min<double>(best, std::max(std::abs(c.x - p.x), std::abs(c.y - p.y)));
        return best;
      };
      auto score = [&](const Cell& c) { return minDist(c, m.player) - minDist(c, m.enemy); };
      // Array.prototype.sort with (a, b) => scoreB - scoreA: stable, NaN reads as equal.
      std::stable_sort(candidates.begin(), candidates.end(), [&](const Cell& a, const Cell& b) {
        const double v = score(b) - score(a);
        return !std::isnan(v) && v < 0;
      });
      auto heroHexes = [&](const Cell& c) {
        double best = std::numeric_limits<double>::infinity();
        for (const Cell& sp : m.player) best = std::min<double>(best, oddrDist(c.x, c.y, sp.x, sp.y));
        return best;
      };
      std::vector<Cell> farEnough;
      for (const Cell& c : candidates)
        if (heroHexes(c) >= 4) farEnough.push_back(c);
      const std::vector<Cell>& usable = farEnough.empty() ? candidates : farEnough;
      std::vector<Cell> pool(usable.begin(), usable.begin() + std::min(usable.size(), std::max<size_t>(1, (usable.size() + 1) / 2)));
      for (int i = static_cast<int>(pool.size()) - 1; i > 0; --i) {
        const int j = static_cast<int>(std::floor(rng() * (i + 1)));
        std::swap(pool[static_cast<size_t>(i)], pool[static_cast<size_t>(j)]);
      }
      const int budget = std::min(3, std::max(1, (cols * rows) / 150));
      std::vector<Cell> placed;
      for (const Cell& c : pool) {
        if (static_cast<int>(placed.size()) >= budget) break;
        if (std::any_of(placed.begin(), placed.end(), [&](const Cell& p) { return std::max(std::abs(p.x - c.x), std::abs(p.y - c.y)) < 3; })) continue;
        std::set<std::string> candidate = blockedExtra;
        candidate.insert(key(c.x, c.y));
        if (connectivityOk(grid, cols, rows, candidate, spawns)) {
          blockedExtra.insert(key(c.x, c.y));
          placed.push_back(c);
        }
      }
      for (const Cell& c : placed) m.decorations.push_back(ojson{{"id", "locked-chest"}, {"x", c.x}, {"y", c.y}});
    }
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < cols; ++x) {
        if (grid[y][x] != 'k') continue;
        if (std::any_of(m.decorations.begin(), m.decorations.end(), [&](const ojson& d) { return d.value("id", "") == "locked-chest" && d.value("x", -1) == x && d.value("y", -1) == y; }))
          continue;
        m.decorations.push_back(ojson{{"id", "locked-chest"}, {"x", x}, {"y", y}});
      }
  }

  // data.ts scatterDecor: random scenery on open ground, clear of spawns and each other, never cutting the map in two.
  void scatterDecor(Mission& m, const std::set<std::string>& exclude, const std::function<double()>& random) const {
    const std::vector<std::string> tiles = parseLayout(m.layout);
    std::set<std::string> taken = decorationCells(m.decorations);
    for (const auto* list : {&m.player, &m.enemy})
      for (const Cell& s : *list) {
        taken.insert(key(s.x, s.y));
        for (const Cell& n : hexAdj(s.x, s.y)) taken.insert(key(n.x, n.y));
      }
    auto walkable = [&](int x, int y) { return passable(tiles[y * m.cols + x]); };
    auto reaches = [&](const std::set<std::string>& extra) {
      if (m.player.empty() || m.enemy.empty()) return true;
      const Cell from = m.player[0], to = m.enemy[0];
      std::set<std::string> seen{key(from.x, from.y)};
      std::vector<Cell> q{from};
      while (!q.empty()) {
        const Cell p = q.back();
        q.pop_back();
        if (oddrDist(p.x, p.y, to.x, to.y) <= 1) return true;
        for (const Cell& n : hexAdj(p.x, p.y)) {
          if (n.x < 0 || n.y < 0 || n.x >= m.cols || n.y >= m.rows) continue;
          const std::string k = key(n.x, n.y);
          if (seen.contains(k) || extra.contains(k) || !walkable(n.x, n.y)) continue;
          seen.insert(k);
          q.push_back(n);
        }
      }
      return false;
    };
    std::vector<const CatalogDecoration*> ids;
    for (const DressTables::Deco& d : t_.decorations)
      if (d.model3d.empty() && !d.chest && !d.manual && !exclude.contains(d.id))
        if (const CatalogDecoration* def = c_.decoration(d.id)) ids.push_back(def);
    const int want = std::max(4, static_cast<int>(std::floor(static_cast<double>(m.cols * m.rows) / 288 * 14 + 0.5)));
    if (ids.empty()) return;
    for (int tries = 0, done = 0; tries < want * 40 && done < want; ++tries) {
      const CatalogDecoration* def = ids[static_cast<size_t>(std::floor(random() * static_cast<double>(ids.size())))];
      const int ax = static_cast<int>(std::floor(random() * m.cols));
      const int ay = static_cast<int>(std::floor(random() * m.rows));
      std::vector<Cell> cells;
      for (const FootOffset& f : def->footprint) cells.push_back({ax + f.dx, ay + f.dy});
      if (std::any_of(cells.begin(), cells.end(), [&](const Cell& c) { return c.x < 0 || c.y < 0 || c.x >= m.cols || c.y >= m.rows; })) continue;
      if (std::any_of(cells.begin(), cells.end(), [&](const Cell& c) { return taken.contains(key(c.x, c.y)) || !walkable(c.x, c.y); })) continue;
      std::set<std::string> keys;
      for (const Cell& c : cells) keys.insert(key(c.x, c.y));
      std::set<std::string> extra = taken;
      extra.insert(keys.begin(), keys.end());
      if (!reaches(extra)) continue;
      m.decorations.push_back(ojson{{"id", def->id}, {"x", ax}, {"y", ay}});
      taken.insert(keys.begin(), keys.end());
      for (const Cell& c : cells)
        for (const Cell& n : hexAdj(c.x, c.y)) taken.insert(key(n.x, n.y));
      ++done;
    }
  }

  // data.ts clearScrappedGroundTiles (the retired deadtree / highwood / highruin letters: hill + a prop).
  void clearScrappedGroundTiles(Mission& m) const {
    if (m.hub) return;
    std::set<std::string> claimed = decorationCells(m.decorations);
    for (const auto& [ch, id] : std::vector<std::pair<char, const char*>>{{'t', "wilds-fallen-log"}, {'d', "wilds-dead-oak"}, {'s', "wilds-ruined-wayside-shrine"}})
      for (int y = 0; y < m.rows; ++y)
        for (int x = 0; x < m.cols; ++x) {
          if (m.layout[y][x] != ch) continue;
          m.layout[y][x] = 'h';
          if (claimed.contains(key(x, y))) continue;
          m.decorations.push_back(ojson{{"id", id}, {"x", x}, {"y", y}});
          claimed.insert(key(x, y));
        }
  }

 private:
  const DressTables& t_;
  const EditorCatalog& c_;
  std::map<char, std::string> char_;
};

}  // namespace

bool DressTables::load(const std::filesystem::path& catalogFile) {
  std::ifstream in(catalogFile, std::ios::binary);
  const ojson j = in ? ojson::parse(in, nullptr, false) : ojson();
  if (j.is_discarded() || !j.contains("editor") || !j["editor"].contains("decoMeta")) return false;
  const ojson& e = j["editor"];
  for (const auto& [t, info] : e["terrainInfo"].items()) passable[t] = info.value("passable", false), variantCount[t] = info.value("variantCount", 1);
  for (const auto& [t, ch] : e["tileChar"].items()) tileChar[t] = ch.get<std::string>();
  auto s = [](const ojson& o, const char* k) { return o.contains(k) && o[k].is_string() ? o[k].get<std::string>() : std::string(); };
  for (const auto& [id, d] : e["decoMeta"].items())
    decorations.push_back({id, s(d, "model3d"), s(d, "exitKind"), d.value("chest", false), d.value("manual", false), d.value("solidRock", false), d.value("rockStyle", false)});
  return true;
}

const DressTables::Deco* DressTables::deco(const std::string& id) const {
  for (const Deco& d : decorations)
    if (d.id == id) return &d;
  return nullptr;
}

DressResult dressDraft(const nlohmann::ordered_json& draft, const DressTables& tables, const EditorCatalog& catalog,
                       const std::set<std::string>& exclude, const std::function<double()>& random) {
  const Dresser d(tables, catalog);
  Mission m = d.missionOf(draft);
  // dressMap: clearScrappedGroundTiles(scatterDecor(decorateOpenTerrain(rockifyColumns(scatterTactics(m))), exclude))
  d.scatterTactics(m);
  d.rockifyColumns(m);
  d.decorateOpenTerrain(m);
  d.scatterDecor(m, exclude, random);
  d.clearScrappedGroundTiles(m);
  DressResult r;
  r.tiles = d.parseLayout(m.layout);
  // the editor: the scatter's barricade tiles get their prop (data.ts barricadeDecor)
  r.decorations = m.decorations;
  for (int y = 0; y < m.rows; ++y)
    for (int x = 0; x < m.cols; ++x) {
      if (r.tiles[y * m.cols + x] != "barricade") continue;
      const bool has = std::any_of(m.decorations.begin(), m.decorations.end(), [&](const ojson& p) {
        const std::string id = p.value("id", "");
        return (id == "barricade" || id == "barricade-2" || id == "wooden-barricade" || id == "wooden-barricade-1") && p.value("x", -1) == x && p.value("y", -1) == y;
      });
      if (!has) r.decorations.push_back(ojson{{"id", "barricade"}, {"x", x}, {"y", y}});
    }
  const ojson& old = draft.contains("tileVariants") && draft["tileVariants"].is_array() ? draft["tileVariants"] : ojson::array();
  for (size_t i = 0; i < r.tiles.size(); ++i) {
    const int was = i < old.size() && old[i].is_number() ? old[i].get<int>() : 0;
    const auto vc = tables.variantCount.find(r.tiles[i]);
    r.tileVariants.push_back(std::min(was, (vc == tables.variantCount.end() ? 1 : vc->second) - 1));
    if (r.tiles[i] != "plains") ++r.counts[r.tiles[i]];
  }
  return r;
}

int checkDress(const std::filesystem::path& refs, const DressTables& tables, const EditorCatalog& catalog) {
  std::ifstream in(refs, std::ios::binary);
  const ojson j = in ? ojson::parse(in, nullptr, false) : ojson();
  if (j.is_discarded() || !j.contains("cases")) {
    logError("check-dress: cannot read {}", refs.string());
    return 1;
  }
  int bad = 0, cases = 0;
  for (const ojson& c : j["cases"]) {
    ++cases;
    std::set<std::string> exclude;
    for (const ojson& id : c.value("exclude", ojson::array())) exclude.insert(id.get<std::string>());
    Mulberry32 rng(c.value("seed", 0u));
    const DressResult r = dressDraft(c["draft"], tables, catalog, exclude, [&] { return rng(); });
    const std::string name = std::format("{} (seed {})", c["draft"].value("id", "?"), c.value("seed", 0u));
    if (nlohmann::json(r.tiles) != nlohmann::json::parse(c["tiles"].dump())) ++bad, logError("check-dress: {}: tiles differ", name);
    if (nlohmann::json(r.tileVariants) != nlohmann::json::parse(c["tileVariants"].dump())) ++bad, logError("check-dress: {}: tile variants differ", name);
    if (nlohmann::json::parse(r.decorations.dump()) != nlohmann::json::parse(c["decorations"].dump())) {
      ++bad;
      logError("check-dress: {}: decorations differ ({} here, {} in Engine2)", name, r.decorations.size(), c["decorations"].size());
    }
    if (const char* dump = std::getenv("EMBER_DRESS_DUMP"); dump && (nlohmann::json(r.tiles) != nlohmann::json::parse(c["tiles"].dump()) ||
                                                                       nlohmann::json::parse(r.decorations.dump()) != nlohmann::json::parse(c["decorations"].dump()))) {
      std::filesystem::create_directories(dump);
      std::ofstream(std::filesystem::path(dump) / std::format("{}-{}.json", c["draft"].value("id", "x"), c.value("seed", 0u)))
          << ojson{{"tiles", r.tiles}, {"decorations", r.decorations}, {"refTiles", c["tiles"]}, {"refDecorations", c["decorations"]}}.dump();
    }
  }
  if (bad == 0) logInfo("check-dress: {} cases, Gerar terreno matches Engine2", cases);
  return bad;
}

}  // namespace ember::e2
