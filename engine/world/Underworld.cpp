#include "world/Underworld.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "core/Log.h"

namespace ember {

bool Underworld::load(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    logError("underworld data not found: {}", file.string());
    return false;
  }
  const Json j = Json::parse(in, nullptr, false);
  if (j.is_discarded() || !j.contains("cells")) {
    logError("underworld data unreadable: {}", file.string());
    return false;
  }
  atlas_ = j.value("atlas", std::string());
  hexWidth_ = j.value("hexWidth", 0.0), rowStep_ = j.value("rowStep", 0.0);
  originX_ = j["origin"].value("x", 0.0), originY_ = j["origin"].value("y", 0.0);
  start_ = {j["startHex"].value("col", 0), j["startHex"].value("row", 0)};
  for (const Json& c : j["cells"]) {
    OverworldCell cell;
    cell.col = c.value("col", 0), cell.row = c.value("row", 0), cell.x = c.value("x", 0.0), cell.y = c.value("y", 0.0);
    cells_.push_back(std::move(cell));
  }
  for (const Json& l : j.value("locations", Json::array())) {
    MapLocation m;
    m.id = l.value("id", ""), m.name = l.value("name", "");
    m.col = l.value("col", 0), m.row = l.value("row", 0), m.x = l.value("x", 0.0), m.y = l.value("y", 0.0);
    locations_.push_back(std::move(m));
  }
  logInfo("underworld: {} cells, {} locations, start ({},{})", cells_.size(), locations_.size(), start_.col, start_.row);
  return hexWidth_ > 0 && rowStep_ > 0 && isCell(start_.col, start_.row);
}

const OverworldCell* Underworld::cell(int col, int row) const {
  for (const OverworldCell& c : cells_)
    if (c.col == col && c.row == row) return &c;
  return nullptr;
}

const MapLocation* Underworld::locationAt(int col, int row) const {
  const MapLocation* found = nullptr;
  for (const MapLocation& l : locations_)
    if (l.col == col && l.row == row) found = &l;
  return found;
}

void Underworld::hexToWorld(int col, int row, double& x, double& y) const {
  x = originX_ + (col + 0.5 * (row & 1)) * hexWidth_;
  y = originY_ + row * rowStep_;
}

OverworldHex Underworld::worldToHex(double x, double y) const {
  // In units of the hex's side (the Overworld's worldToHex with the board's own spacing and origin).
  const double ux = (x - originX_) / hexWidth_ * std::sqrt(3.0), uy = (y - originY_) / rowStep_ * 1.5;
  const double fr = 2.0 / 3.0 * uy, fq = std::sqrt(3.0) / 3.0 * ux - uy / 3.0, fs = -fq - fr;
  double rq = std::round(fq), rr = std::round(fr), rs = std::round(fs);
  if (std::abs(rq - fq) > std::abs(rr - fr) && std::abs(rq - fq) > std::abs(rs - fs)) rq = -rr - rs;
  else if (std::abs(rr - fr) > std::abs(rs - fs)) rr = -rq - rs;
  const int row = static_cast<int>(rr);
  return {static_cast<int>(rq) + (row - (row & 1)) / 2, row};
}

OverworldHex Underworld::position(const Json& save) const {
  const auto it = save.find("underworldPos");
  if (it == save.end() || !it->is_object()) return start_;
  const OverworldHex p{it->value("col", start_.col), it->value("row", start_.row)};
  return isCell(p.col, p.row) ? p : start_;
}

bool Underworld::step(Json& save, OverworldHex to) const {
  const OverworldHex from = position(save);
  const std::vector<OverworldHex> around = Overworld::neighbors(from.col, from.row);
  if (!isCell(to.col, to.row) || std::find(around.begin(), around.end(), to) == around.end()) return false;
  save["underworldPos"] = Json{{"col", to.col}, {"row", to.row}};
  // Time passes here as on the Overworld (the user, 2026-10-28: "we need the time and moon to count on the underworld map too"):
  // a step takes the Overworld's 6 hours on the campaign's clock, and every day crossed moves gameClock, which the moon follows.
  constexpr int kStepHours = 6;
  const int totalHours = Overworld::campaignHour(save) + kStepHours;
  const double days = (save.contains("gameClock") && save["gameClock"].is_number() ? save["gameClock"].get<double>() : 0.0) + totalHours / 24;
  save["gameClock"] = days == std::floor(days) ? Json(static_cast<long long>(days)) : Json(days);
  save["gameHour"] = totalHours % 24;
  return true;
}

}  // namespace ember
