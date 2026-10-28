#pragma once

// The Underworld's hex board over the Atlas do Submundo (content/data/underworld.json, written by
// tools/scripts/maps/make_underworld_board.py): the world map's own hexes (pointy-top, odd-r, Overworld::neighbors), laid on
// the atlas with Temple Descend's hex on the user's mark (the shaft under the temple). The party's place there is the save's
// "underworldPos". A step moves the party only: no time, hunger, encounters or fog there yet (nothing decided for them).

#include <filesystem>
#include <optional>
#include <vector>

#include "world/Overworld.h"

namespace ember {

class Underworld {
 public:
  bool load(const std::filesystem::path& file);

  const std::vector<OverworldCell>& cells() const { return cells_; }
  const OverworldCell* cell(int col, int row) const;
  bool isCell(int col, int row) const { return cell(col, row) != nullptr; }
  OverworldHex startHex() const { return start_; }
  const std::vector<MapLocation>& locations() const { return locations_; }
  const MapLocation* locationAt(int col, int row) const;

  // A hex's centre in percent of the atlas (the board's own spacing: the same size on screen as the Overworld's hexes), and
  // the hex under a point (cube rounding, odd-r).
  void hexToWorld(int col, int row, double& x, double& y) const;
  OverworldHex worldToHex(double x, double y) const;
  double hexWidth() const { return hexWidth_; }  // percent of the atlas's width
  double rowStep() const { return rowStep_; }    // percent of the atlas's height

  // The party's hex: the save's "underworldPos", else Temple Descend.
  OverworldHex position(const Json& save) const;
  // One step to a neighbouring cell; false (the save untouched) for any other hex.
  bool step(Json& save, OverworldHex to) const;

 private:
  std::vector<OverworldCell> cells_;
  std::vector<MapLocation> locations_;
  OverworldHex start_{};
  double hexWidth_ = 0, rowStep_ = 0, originX_ = 0, originY_ = 0;
};

}  // namespace ember
