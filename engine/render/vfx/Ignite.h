#pragma once

// Ignite (ours, 3D; the user, 2026-11-17: "make a basic flame spell in 3D for monsters only, with proper scaling, but the aoe is
// always radius 2", "ignite does not need a projectile"; 2026-11-25: "diminuir essa porra de área", now radius 1): nothing flies.
// The heat gathers over the aimed area (embers drifting up off the ground, a red glow growing), then the area catches fire, ring by
// ring outward from the aimed hex. The user, 2026-11-25: "esse foguinho muito vagabundo": one smooth teardrop per hex read as a
// candle, so every hex now burns as a real blaze, all of its fire 3D volume (VfxMaterial::FireVolume; the user: "fogo de sprites"
// is wrong, no flipbook): a burst of fire bursting up off the ground as it catches (a dome of fire tearing apart, body 2), a low bed of
// fire boiling on the ground all the while it burns (body 2 held young), a cluster of tongues of fire of different heights dancing on
// it (body 0, torn at the top); sparks thrown up, embers climbing all through it and smoke rising over it as it burns out. Flames rise from below, never from above; nothing drawn past the area's hexes, the light reaching only their edge.
// One light at the centre: a glow while the heat gathers, a flare as it ignites, a flicker while it burns. World space throughout.

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
  struct Flame {  // one tongue of fire
    glm::vec3 foot;
    float delay, life, height, radius, strength, turbulence, seed;
  };
  struct Burst {  // the fire bursting up off a hex as it catches, and the bed of fire left boiling on it
    glm::vec3 at;
    float delay, life, radius, seed, bedLife;
  };
  struct Particle {
    int kind;  // 1 an ember / spark (procedural flame), 2 smoke
    float at, life, size, spin, rotation, emission;
    glm::vec3 from, velocity, color;
    float gravity, drag;
  };
  std::vector<Flame> flames_;
  std::vector<Burst> bursts_;
  std::vector<Particle> particles_;
  glm::vec3 centre_{0.0f};
  float reach_ = 1.0f;  // the farthest hex's distance from the centre (world)
  float burn_ = 1.0f;   // how long the area burns, from the impact to the last flame's end
  float impact_ = 0.0f, duration_ = 2.0f;
};

}  // namespace ember
