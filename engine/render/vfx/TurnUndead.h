#pragma once

// Expulsar Mortos-Vivos (kind `turnUndead`, holy, a self area round the caster), version 01: ours, in 3D on the effect runtime
// (the user, 2026-10-19: "I also need a 3D spell effect for Turn Undead"). Drawn with the Holy material (the sigil, the shaft,
// the motes, the fear mark) and the Combat material (the shock shell, the ground rings, the rays, the sparks, the ash).
//
//   gathering (kLead before the impact)  a holy sigil draws itself on the ground round the caster and grows out to the area's
//                                        edge: two rings with tick marks, a hexagram (the board is hexes) turning against an
//                                        inner ring, a pool of light; a shaft of light comes down onto the caster and motes spiral
//                                        in to its chest; a warm light swells;
//   the impact                           a flash at the chest, a pressure shell and two ground rings burst out to the edge, rays
//                                        run across the ground, motes spray out, the sigil flares and fades;
//   on every undead struck               as the wave reaches it: a column of white-gold radiance, a flash, a ring at its feet,
//                                        sparks driven away from the caster (it is repelled), ash lifting off the body, embers,
//                                        then a ring of pale light shivering over its head (it is afraid), a warm light.
//
// World space (y up). Points on a body are pulled toward the eye so they show over the card, as CombatFx does.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class TurnUndead {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height as drawn
  };

  // The sigil gathers this long before the impact (SpellEffects::leadIn).
  static constexpr float kLead = 1.0f;

  // The caster at `feet` (its card `height`), the area reaching `radius` world units from it, the undead it strikes, the burst at `impact`.
  void cast(glm::vec3 feet, float height, float radius, std::vector<Target> undead, float impact);

  // Seconds from the impact until everything is over.
  static float tail() { return 2.7f; }
  bool active(float now) const;
  float latestEnd() const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Effect {
    glm::vec3 feet{0.0f};
    float height = 1.8f, radius = 3.5f;
    float impact = 0.0f;
    std::vector<Target> undead;
    uint32_t seed = 0;
    float end() const { return impact + tail(); }
  };
  void buildOne(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, columnFirst_ = 0, haloFirst_ = 0, sigilFirst_ = 0, starFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, columnCount_ = 0, haloCount_ = 0, sigilCount_ = 0, starCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, motes_;
};

}  // namespace ember
