#pragma once

// Ember's FireballVFX (src/vfx/legacy/three/FireballVFX.ts) as BattleSpells3D plays it, ported to the effect runtime:
// Bola de Fogo 01. The projectile is OldFireBall (OldFireBall.ts): a boiling icosphere of fire (fbm noise pushes its
// surface in and out, heat colours from white core to red edge) inside a softer flaring shell, carrying a fire light,
// arcing from the caster to the target hex over 1.25-2.5 s. On arrival, FireballImpactVFX (FireballImpactVFX.ts, its
// default "Etapa 02" settings): a camera-facing flash, then 308 seeded particles (procedural flames, rising
// secondary flames, a ring of fire-flipbook flames on the ground, sparks, embers, smoke) held inside the spell's area,
// with a light that flares and flickers out. Its math runs in Ember's frame (z up) as Ember's does, including where
// Ember's impact used its own y as up (inside the Ember frame that y lies along the board: kept as it is).
//
// Not carried over: the lights' shadows (Ember: 512 px shadow maps; our local lights have none yet); soft particles
// (Ember passes no depth texture in battle, so they are off there too).

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class Fireball {
 public:
  // origin: Ember's caster point (anchor + 0.9 up); target: the target hex's ground + 0.04; aoeRadius: the area's
  // radius (the farthest struck cell's distance + 1, BattleSpells3D). World space / world units.
  Fireball(glm::vec3 origin, glm::vec3 target, float aoeRadius, float start);

  static float travelSeconds(glm::vec3 origin, glm::vec3 target);  // the flight: 1.25-2.5 s
  float impactTime() const { return start_ + travel_; }
  bool done(float now) const { return now - start_ > travel_ + duration_; }
  float end() const { return start_ + travel_ + duration_; }

  void build(float now, const Camera& camera, const VfxFlipbook& fire, VfxFrame& frame, std::vector<VfxLight>& lights);

  // DEFAULT_IMPACT_SETTINGS.
  struct ImpactSettings {
    uint32_t seed = 1337;
    float flashIntensity = 18, flashSize = 0.72f, flashDuration = 0.07f, flashOffset = 0;
    int primaryCount = 64;
    float primarySize = 0.68f, primarySpeed = 3.4f, primaryDrag = 5.5f, primaryLife = 0.42f, verticalSpread = 0.48f, primaryEmission = 2.4f,
          primaryOffset = 0.02f;
    int secondaryCount = 34;
    float secondarySize = 0.98f, secondarySpeed = 1.6f, buoyancy = 0.8f, secondaryLife = 0.72f, secondaryOffset = 0.08f;
    int radialCount = 52;
    float radialSpeed = 2.9f, radialOffset = 0.07f;
    int sparkCount = 112;
    float sparkSpeed = 6.8f, sparkGravity = 8.8f, sparkDrag = 0.65f, sparkLife = 0.65f, sparkOffset = 0.03f;
    int emberCount = 30;
    float emberSpeed = 2.1f, emberGravity = 3.5f, emberLife = 0.9f, emberOffset = 0.08f;
    int smokeCount = 16;
    float smokeSize = 0.72f, smokeRise = 0.68f, smokeExpansion = 0.5f, smokeOpacity = 0.22f, smokeLife = 1.4f, smokeOffset = 0.18f;
    float peakLight = 22, lightRadius = 5.4f, lightDecay = 0.96f, flipbookFps = 18;
  };

 private:
  enum class Kind { Primary, Secondary, Radial, Spark, Ember, Smoke };
  struct Particle {
    Kind kind;
    float delay, life, size, phase, spin, rotation, drag, gravity;
    glm::vec3 initial, velocity;
  };
  void buildParticles();

  ImpactSettings s_;
  glm::vec3 start3_, target_, origin_;  // Ember frame: flight start, target, impact origin
  float aoe_ = 1, impactScale_ = 1;
  float start_ = 0, travel_ = 0, duration_ = 1.45f;
  std::vector<Particle> particles_;
};

}  // namespace ember
