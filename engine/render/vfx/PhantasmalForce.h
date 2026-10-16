#pragma once

// Engine2's PhantasmalForceVFX (src/vfx/legacy/three/PhantasmalForceVFX.ts) with DEFAULT_PHANTASMAL_FORCE_SETTINGS, as
// BattleSpells3D plays it (restartAt the struck body's middle, scale 1), ported to the effect runtime: Força
// Fantasmagórica 01. Eight tapered indigo tendrils and 260 violet motes swirl around the target and are drawn in to a
// growing, churning core (0.55-0.75 s); at 0.82 s it bursts: a rim-lit shell expands to 2 units in 0.2 s, the motes are
// thrown out, a violet light spikes and fades; all gone by 1.35 s. Its math runs in Ember's frame (z up) as Engine2's
// does (the motes' fall after the burst follows Engine2's own y, which lies along the board in that frame).
//
// Not carried over: the light's shadow (Engine2: a 256 px map; our local lights have none yet).

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class PhantasmalForce {
 public:
  PhantasmalForce(glm::vec3 target, float start);  // target: the struck body's middle, world space

  static constexpr float kImpactAt = 0.82f, kEndAt = 1.35f;
  bool done(float now) const { return now - start_ >= kEndAt; }
  float end() const { return start_ + kEndAt; }

  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Seed {
    glm::vec3 direction, tangent;
    float phase, speed, size, delay;
  };
  glm::vec3 target_;  // Ember frame
  float start_ = 0;
  std::vector<Seed> seeds_;
};

}  // namespace ember
