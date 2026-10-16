#pragma once

// Engine2's BlessVFX (src/vfx/legacy/three/BlessVFX.ts) with DEFAULT_BLESS_VFX_SETTINGS, as BattleSpells3D plays it,
// ported to the effect runtime: Bênção 01. A golden core gathers before the caster with rising motes (0.3 s), then a
// rippling amber-to-ivory wave ring spreads over the spell's radius (BLESS.radius 3 hexes) in 0.55 s; as it reaches
// each ally, eight golden strands coil down around them with orbiting motes and a warm light, and are absorbed
// (0.78 s). Everything fades by 1.75 s. Its math runs in Ember's frame (z up) as Engine2's does; where Engine2 used
// its own y as "down" for motes, core and strands (Ember's top-down canvas), that y lies along the board here too.
//
// Not carried over: the lights' shadows (Engine2: 256 px maps; our local lights have none yet).

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class Bless {
 public:
  struct Ally {
    glm::vec3 position;   // world: the unit's anchor, 0.9 above the ground (BattleSpells3D unit())
    float distanceHexes;  // from the caster
  };
  // center: the caster's hex, 0.04 above the ground; radiusWorld: the wave's reach (BLESS.radius x sqrt 3).
  Bless(glm::vec3 center, float radiusWorld, std::vector<Ally> allies, float start);

  static constexpr float kWaveRelease = 0.3f, kDuration = 1.5f;
  // When the wave reaches an ally this far away (Engine2 applies the blessing then).
  static float reachSeconds(float distanceHexes);
  bool done(float now) const { return now - start_ >= kDuration + 0.25f; }

  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  glm::vec3 center_;  // Ember frame
  float radius_ = 1.0f, start_ = 0.0f;
  struct Reception {
    glm::vec3 position;  // Ember frame
    float reachAt;
  };
  std::vector<Reception> receptions_;
  std::vector<float> seeds_;  // particleSeeds
};

}  // namespace ember
