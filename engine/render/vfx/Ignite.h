#pragma once

// Ignite (ours, 3D; the user, 2026-11-17: "make a basic flame spell in 3D for monsters only, with proper scaling, but the aoe is
// always radius 2", "ignite does not need a projectile"): nothing flies. The heat gathers over the aimed area (embers drifting up
// off the ground, a red glow growing), then the air itself catches fire: on every hex of the area a flame bursts out of the air a
// little above the ground, drops onto it and roars up, ring by ring outward from the aimed hex, then lifts off and burns out,
// throwing sparks and leaving smoke. Each flame is a real volume of fire (VfxMaterial::FireVolume, body 0, the torches' flame);
// the embers, sparks and smoke are camera-facing particles (VfxMaterial::ImpactParticle, procedural flame / smoke). One light at
// the centre: a glow while the heat gathers, a flare as it ignites, a flicker as it dies. World space throughout.

#include <cstdint>
#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class Ignite {
 public:
  static constexpr float kLead = 0.55f;  // the heat gathers this long before the air ignites (the hit lands then)
  // cells: the area's hexes (their feet, world); centre: the aimed hex's feet; impact: when the air catches fire.
  Ignite(std::vector<glm::vec3> cells, glm::vec3 centre, float impact, uint32_t seed);

  bool done(float now) const { return now > end(); }
  float end() const { return impact_ + duration_; }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

 private:
  struct Flame {
    glm::vec3 foot;
    float delay, life, height, radius, seed;
  };
  struct Particle {
    int kind;  // 1 an ember / spark (procedural flame), 2 smoke
    float at, life, size, spin, rotation;
    glm::vec3 from, velocity;
    float gravity, drag;
  };
  std::vector<Flame> flames_;
  std::vector<Particle> particles_;
  glm::vec3 centre_{0.0f};
  float reach_ = 1.0f;  // the farthest hex's distance from the centre (world)
  float impact_ = 0.0f, duration_ = 2.0f;
};

}  // namespace ember
