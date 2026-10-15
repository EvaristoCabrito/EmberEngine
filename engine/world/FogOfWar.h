#pragma once

// Ember's fog of war (ARCHITECTURE §7.0 R7).
//   states  Engine2's (src/rules/fog.ts): never seen, remembered, in sight; remembered never falls back to
//           never seen. Every living party unit is an eye with its own vision range (SIGHT_RADIUS 7).
//   sight   EmberEngine's own line of sight in the 3D board (Engine2's was never finished; the user,
//           2026-10-14: occlusion-based). A ray from the eye (kEyeHeight above its hex) to a point
//           kTargetHeight above the target hex's ground; a hex the ray passes whose top (ground + whatever
//           stands on it: buildings, trees, tall cards) rises above the ray blocks it. The blocker itself is
//           seen (you see the wall, not past it). Three rays (centre and either side of the hexes) so a
//           line grazing a corner is not decided by rounding; any clear ray sees. Gameplay heights (cell
//           ground), so all board looks see the same (R8).
//   mask    Engine2's look (src/render/fogOfWar.ts + battleAtmosphere3d.ts syncFog): per-texel darkness
//           (never seen = 1, remembered = 0.6, in sight = 0), feathered only inside remembered hexes, and
//           never any fog within one hex of a party unit.
// The renderer reads the mask as tactical visibility (shaders/lighting.hlsli tacticalSight); every light,
// shadow, card and effect goes through it.

#include <cstdint>
#include <span>
#include <vector>

#include "world/Board.h"

namespace ember::e2 {

inline constexpr uint8_t kFogUnseen = 0, kFogExplored = 1, kFogVisible = 2;
inline constexpr int kSightRadius = 7;            // Engine2 SIGHT_RADIUS
inline constexpr float kExploredAlpha = 0.6f;      // Engine2 EXPLORED_ALPHA (darkness over remembered ground)
inline constexpr float kEyeHeight = 1.6f;          // eyes above their hex's ground (world units = hex radii)
inline constexpr float kTargetHeight = 0.8f;       // a hex is seen when a body standing there is (half its height)
inline constexpr float kBlockHeight = 1.2f;        // a standing card at least this tall blocks sight

struct HexPoint {
  int c = 0, r = 0;
  bool operator==(const HexPoint&) const = default;
};

// Engine2's odd-r hex distance and hex line (src/rules/pathfinding.ts).
int hexDistance(HexPoint a, HexPoint b);
std::vector<HexPoint> hexLine(HexPoint a, HexPoint b);

struct Eye {
  HexPoint at;
  int radius = kSightRadius;
};

// The darkness mask over the board: texel (x, y) covers world x0 + (x + 0.5) / kRes, z0 + (y + 0.5) / kRes.
struct FogMask {
  static constexpr int kRes = 8;  // texels per world unit (one hex radius)
  int width = 0, height = 0;
  float x0 = 0, z0 = 0, worldWidth = 0, worldDepth = 0;
  std::vector<uint8_t> alpha;     // 0 = in sight, ~153 = remembered, 255 = never seen
  // For the volumetric fog (its look, not visibility): where the dense fog gathers.
  std::vector<float> boundary;    // never-seen ground blurred over about a hex and a half: 1 deep inside, 0.5 at its edge
  std::vector<float> ground;      // ground height; never-seen ground reads the board's mean (reveals no shape)
  std::vector<float> valley;      // how far the ground lies below its surroundings (about 3 hexes around), >= 0
};

class FogOfWar {
 public:
  // Ground heights from the board's cells; nothing stands on them until block() says so.
  explicit FogOfWar(const Board& board);

  // Something `height` tall stands at a world point: its hex's top rises to at least ground + height.
  void block(double x, double z, float height);
  // Whether `to` is in sight from an eye at `from` (the line-of-sight rule above; range not checked).
  bool sightReaches(HexPoint from, HexPoint to) const;

  // What was visible becomes remembered, then every hex within an eye's radius that it can see is visible.
  void relight(std::span<const Eye> eyes);
  uint8_t state(int c, int r) const { return cells_[static_cast<size_t>(indexOf(layout_, c, r))]; }
  bool visible(int c, int r) const { return state(c, r) == kFogVisible; }
  // Bumped by every relight: the mask (and the shadows that read it) must be redrawn.
  uint64_t version() const { return version_; }

  // Engine2's mask for the current states; `party` are the hexes the party stands on (clear within one hex).
  FogMask paint(const Board& board, std::span<const HexPoint> party) const;

 private:
  bool rayClear(double ax, double az, double ay, double bx, double bz, double by, int fromIndex, int toIndex) const;

  HexLayout layout_;
  std::vector<uint8_t> cells_;
  std::vector<float> ground_, top_;  // per hex: ground height, and the highest thing standing on it
  uint64_t version_ = 0;
};

}  // namespace ember::e2
