#pragma once

// Ember's fog of war (ARCHITECTURE §7.0 R7).
//   states  Ember's (src/rules/fog.ts): never seen, remembered, in sight; remembered never falls back to
//           never seen. Every living party unit is an eye with its own vision range (kSightRadius; Ember's 7).
//   sight   EmberEngine's own line of sight in the 3D board (Ember's was never finished; the user,
//           2026-10-14: occlusion-based). A ray from the eye (kEyeHeight above its hex) to a point
//           kTargetHeight above the target hex's ground; a hex the ray passes whose top (ground + whatever
//           stands on it: buildings, trees, tall cards) rises above the ray blocks it. The blocker itself is
//           seen (you see the wall, not past it). Three rays (centre and either side of the hexes) so a
//           line grazing a corner is not decided by rounding; any clear ray sees. Gameplay heights (cell
//           ground), so all board looks see the same (R8). Only wide things block (kBlockWidth: walls,
//           houses, big trees; not lamp posts or signposts), and a hex every one of whose neighbours is in
//           sight is in sight too: nothing hides alone in the middle of open ground (the user, 2026-10-15);
//           an enclosed group of up to 3 never-seen hexes with only explored neighbours counts as seen
//           (in sight if they all are, remembered otherwise).
//   mask    Ember's look (src/render/fogOfWar.ts + battleAtmosphere3d.ts syncFog): per-texel darkness
//           (never seen = 1, remembered = 0.6, in sight = 0), feathered only inside remembered hexes, and
//           never any fog within one hex of a party unit.
// The renderer reads the mask as tactical visibility (shaders/lighting.hlsli tacticalSight); every light,
// shadow, card and effect goes through it.

#include <cstdint>
#include <span>
#include <vector>

#include "world/Board.h"

namespace ember {

inline constexpr uint8_t kFogUnseen = 0, kFogExplored = 1, kFogVisible = 2;
inline constexpr int kSightRadius = 12;           // the user, 2026-10-18: 9 (Ember's SIGHT_RADIUS 7), then 12 ("aumentar o line of sight")
// Ground the party has left stays drawn as in sight for this long, then darkens to remembered (the user, 2026-10-18: "a
// área que o grupo deixou continua clara como vista por 3 minutos, depois escurece"). The look only: what the party sees
// (enemies drawn, combat starting, aims) is still the sight of this moment.
inline constexpr double kLingerSeconds = 180.0;
inline constexpr float kExploredAlpha = 0.6f;      // Ember EXPLORED_ALPHA (darkness over remembered ground)
inline constexpr float kEyeHeight = 1.6f;          // eyes above their hex's ground (world units = hex radii)
inline constexpr float kTargetHeight = 0.8f;       // a hex is seen when a body standing there is (half its height)
inline constexpr float kBlockHeight = 1.2f;        // a standing card at least this tall blocks sight ...
inline constexpr float kBlockWidth = 1.2f;         // ... and at least this wide (most of a hex: a lamp post does not)

struct HexPoint {
  int c = 0, r = 0;
  bool operator==(const HexPoint&) const = default;
};

// Ember's odd-r hex distance and hex line (src/rules/pathfinding.ts).
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
  // A resumed fight's explored hexes (Ember restoreExplored): each marked seen comes back explored; what is in sight
  // falls out of the next relight.
  void restoreExplored(const std::vector<uint8_t>& seen) {
    for (size_t i = 0; i < cells_.size() && i < seen.size(); ++i)
      if (seen[i] && cells_[i] == kFogUnseen) cells_[i] = kFogExplored;  // dark at once: no time left in the light
  }
  // The clock (seconds) the next relight stamps on what it sees, and paint() measures the linger against.
  void setClock(double seconds) { clock_ = seconds; }
  // Whether some remembered hex's linger ran out in (since, clock]: the mask must be painted again.
  bool lingerEnded(double since) const;
  bool visible(int c, int r) const { return state(c, r) == kFogVisible; }
  // Bumped by every relight: the mask (and the shadows that read it) must be redrawn.
  uint64_t version() const { return version_; }

  // Ember's mask for the current states; `party` are the hexes the party stands on (clear within one hex). Remembered
  // hexes still inside their kLingerSeconds are painted as in sight.
  FogMask paint(const Board& board, std::span<const HexPoint> party) const;
  // The states the mask is painted from (paint's first step): in sight, plus remembered ground still inside its linger, plus the hexes
  // within one of a party unit. kFogVisible here = drawn lit: what stands on it is drawn (the user, 2026-10-19: an enemy on ground that
  // is drawn lit must be drawn too).
  std::vector<uint8_t> displayStates(std::span<const HexPoint> party) const;

 private:
  bool rayClear(double ax, double az, double ay, double bx, double bz, double by, int fromIndex, int toIndex) const;

  static constexpr double kNeverSeen = -1e30;

  HexLayout layout_;
  std::vector<uint8_t> cells_;
  std::vector<double> seenAt_;  // per hex: the clock when it was last in sight (kNeverSeen: no linger)
  double clock_ = 0.0;
  std::vector<float> ground_, top_;  // per hex: ground height, and the highest thing standing on it
  uint64_t version_ = 0;
};

}  // namespace ember
