#pragma once

// A battle on an Ember map: the board as the movement rules see it, the units and their movement reach. The rules are
// Ember's (Engine2 src/game/pathfinding.ts: footprint, footprintCost, computeReachable, reconstructPath), ported
// step for step so a unit reaches exactly the hexes, at the costs and along the paths, it does there; Cooker
// check-battle compares every unit's reach with Engine2's own (tools/scripts/engine2/export_battle.ts).
//
// Hexes are (column, row) on Ember's odd-r board: odd rows sit half a hex to the right.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ember::e2 {

struct BattleHex {
  int x = 0, y = 0;  // column, row
  bool operator==(const BattleHex&) const = default;
};

// One cell as movement reads it (Engine2 hexprops.hexDef: the terrain with the decorations' blocking folded in).
struct BattleCell {
  std::string terrain;  // the tile's terrain id ("plains", "water", "barricade", ...)
  bool passable = true;
  int moveCost = 1;
  int def = 0, atk = 0;
  bool blocked = false;  // a decoration stands here and stops walking (HEX_BLOCKED)
};

// One reachable hex: the movement it costs and the hex it is reached from (none for the start).
struct ReachCell {
  int x = 0, y = 0, cost = 0;
  int parentX = -1, parentY = -1;
};

struct BattleUnit {
  std::string id, side, name, classId, sprite;
  int x = 0, y = 0;  // the anchor hex: for a big body, its front row (closest to the player)
  bool alive = true;
  int mov = 0, moveUsed = 0;  // movement this turn and what is spent of it
  int hp = 0, maxHp = 0, atk = 0, def = 0, mag = 0, dex = 0, minRange = 1, maxRange = 1;
  int size = 1;
  std::optional<std::vector<BattleHex>> footprintOffsets;  // an authored body shape (dx, dy), else by size
  int footprintW = 2, footprintH = 4;                       // the size >= 4 rectangle
  std::vector<ReachCell> reference;                         // Engine2's reach at the start (the check's reference)
};

class Battle {
 public:
  static std::optional<Battle> load(const std::filesystem::path& battleJson);

  int cols() const { return cols_; }
  int rows() const { return rows_; }
  const std::string& map() const { return map_; }
  const BattleCell& cell(int x, int y) const { return cells_[static_cast<size_t>(y * cols_ + x)]; }
  bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < cols_ && y < rows_; }
  std::vector<BattleUnit>& units() { return units_; }
  const std::vector<BattleUnit>& units() const { return units_; }

  // The hexes a unit's body covers with its anchor at (x, y) (pathfinding.ts footprint).
  static std::vector<BattleHex> footprint(const BattleUnit& u, int x, int y);
  // The living unit whose body covers (x, y), or -1.
  int unitAt(int x, int y) const;
  // Every hex the unit can end its move on with the movement it has left (computeReachable with stop points
  // pruned), the start included at cost 0. pruneStops = false keeps hexes it may only pass through.
  std::vector<ReachCell> reachable(size_t unit, bool pruneStops = true) const;
  // The walk from the unit's hex to (x, y) through a reach map, start first; empty when (x, y) is not in it.
  static std::vector<BattleHex> path(const std::vector<ReachCell>& reach, int x, int y);
  // Moves the unit to (x, y) along a reachable path, spending its cost. False when (x, y) is not reachable.
  bool move(size_t unit, int x, int y);
  // A new turn: every unit's movement is whole again.
  void newTurn();

  // Ember's hex neighbours in its own order (east, then counter-clockwise from the top); not clipped to the board.
  static std::vector<BattleHex> neighbors(int x, int y);
  static int distance(BattleHex a, BattleHex b);

 private:
  std::optional<int> stepCost(int x, int y, const BattleUnit& self, const std::vector<int>& occupancy, bool stop) const;
  std::vector<int> occupancy() const;  // per cell: the unit standing there, or -1

  std::string map_;
  int cols_ = 0, rows_ = 0;
  std::vector<BattleCell> cells_;
  std::vector<BattleUnit> units_;
};

}  // namespace ember::e2
