#pragma once

// Engine2's MagicMissileV2VFX (src/vfx/legacy/three/MagicMissileV2VFX.ts, as BattleSpells3D plays it: one missile,
// the default settings), ported to the effect runtime: Míssil Mágico 02. One bolt gathers above the caster for 0.95 s
// (a distorted violet core inside a banded shell, three arcane threads trailing from it), then flies a curved, seeking
// path to the target's body (0.8-1.3 s), trailing a fragmented ribbon and 28 crystal shards, a violet light riding with
// it; on the hit it collapses into a burst ring, four spinning fracture arcs and a swirl of shards, with a white flash
// fading over 0.78 s. Drawn in the foreground (Engine2: depthTest false). Its math runs in Ember's frame (z up), as
// Engine2's does, and is turned into ours at the end.
//
// Not carried over: the two lights cast no shadows here (Engine2 gave them 256 px shadow maps; our local lights have
// none yet); three.js lines (1 px) are drawn as thin ribbons.

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class MagicMissileV2 {
 public:
  // DEFAULT_MAGIC_MISSILE_V2_SETTINGS (the ones used in battle).
  struct Settings {
    float missileScale = 0.42f, projectileSpeed = 6.0f, acceleration = 1.45f, trajectoryCurvature = 0.36f, trajectoryHeight = 0.48f;
    float seekingStrength = 0.76f, turbulence = 0.2f, shellDistortion = 0.48f, trailLength = 0.52f, trailThickness = 0.12f;
    float trailFragmentation = 0.34f, emissive = 5.8f, travelLightIntensity = 14.0f, travelLightRadius = 3.8f, impactSize = 0.62f;
    float impactLightIntensity = 22.0f, impactLightRadius = 4.8f, finalImpactMultiplier = 1.16f;
    int particleCount = 28;
    uint32_t seed = 44721;
  };

  // origin: Engine2's caster point (its anchor, 0.9 above the ground); target: the struck body's middle. World space.
  MagicMissileV2(glm::vec3 origin, glm::vec3 target, float start);

  static constexpr float kLaunchAt = 0.95f;   // bolt.launchAt
  static constexpr float kImpactLife = 0.78f;  // the hit's burst; complete after it
  // Seconds from the cast to the hit (launch + flight: distance / speed, 0.8-1.3 s).
  static float impactDelay(glm::vec3 origin, glm::vec3 target);
  float impactTime() const { return start_ + impactAt_; }
  bool done(float now) const { return now - start_ > impactAt_ + kImpactLife; }
  float end() const { return start_ + impactAt_ + kImpactLife; }

  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  glm::vec3 formation(float t) const;  // formationPosition(origin, 0, 1, t)
  glm::vec3 flightPoint(float t, glm::vec3* velocity) const;  // the bolt's place at t (launched), Ember frame

  Settings s_;
  glm::vec3 origin_, target_;  // Ember frame
  float start_ = 0.0f, impactAt_ = 0.0f, flight_ = 0.0f;
  float seed_ = 0.0f;
  three::CatmullRom curve_;
  three::Lcg rng_;
};

}  // namespace ember
