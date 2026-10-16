#pragma once

// Engine2's two melee sweeps (src/vfx/legacy/three/VarreduraVFX.ts) as BattleSpells3D plays them, ported to the effect
// runtime. Both start 0.18 s into the swing (stepSpell queues them then).
//   CleaveSweepVFX = Cleave 01: a steel-blue crescent blade with a bright leading edge and a dim wake grows across the
//     struck arc (DEFAULT_VARREDURA_SETTINGS: 2.25 rad, radius 3.1) at weapon height while a spearhead runs the arc with
//     a trail and a light; stone debris kicks off each target and a light flashes on it as the arc reaches it.
//   VarreduraVFX = Varredura 01: a full circular pressure wave on the ground (radius 3.2): a bright outer ring, an inner
//     glowing ring, 42 shards riding it, a light orbiting its edge and a flash on each target as it arrives.
// Their math runs in Ember's frame (z up) as Engine2's does.

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class CleaveSweep {
 public:
  // origin / targets: the caster's and the struck units' anchors 0.7 up (weapon height). World space.
  CleaveSweep(glm::vec3 origin, const std::vector<glm::vec3>& targets, float start);
  static constexpr float kQueued = 0.18f;  // stepSpell: queued 0.18 s into the swing
  bool done(float now) const { return now - start_ > 1.25f + 0.28f; }
  float end() const { return start_ + 1.25f + 0.28f; }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  glm::vec3 spearAt(float t) const;  // the spearhead in the root's frame
  glm::vec3 origin_;                 // Ember frame
  std::vector<glm::vec3> targets_;   // Ember frame
  glm::vec2 direction_{1, 0};
  float start_ = 0;
  std::vector<VfxVertex> blade_, edge_, wake_;
};

class Varredura {
 public:
  // origin / targets: anchors 0.04 above the ground. World space.
  Varredura(glm::vec3 origin, const std::vector<glm::vec3>& targets, float start);
  bool done(float now) const { return now - start_ > CleaveSweep::kQueued + 1.18f; }
  float end() const { return start_ + CleaveSweep::kQueued + 1.18f; }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  glm::vec3 origin_;
  std::vector<glm::vec3> offsets_;
  float start_ = 0;
};

}  // namespace ember
