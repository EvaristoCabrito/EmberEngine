#pragma once

// Engine2's CausticVenomVFX (src/vfx/legacy/three/CausticVenomVFX.ts) as BattleSpells3D plays it (pixel 1/34, up +z),
// ported to the effect runtime: Veneno Cáustico 01. A glassy green orb charges at the caster (0.38 s), then flies an
// arching, slightly bowed path (0.7-1.65 s) while three twisting glass ribbons and seven beads grow along it behind
// a pulsing head; on arrival every struck hex erupts with Engine2's Poison V2 element effect (ElementSpritesV2: plume,
// ground puddle, motes) and six procedural smoke lobes rising, under a green light, settling over 1.85 s. Its math
// runs in Ember's frame (z up) as Engine2's does.
//
// Not carried over: MeshPhysicalMaterial's clearcoat and transmission (drawn as MeshStandardMaterial with the same
// colour, emissive, roughness, metalness and opacity); the lights' shadows.

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ElementSprites.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class CausticVenom {
 public:
  // origin: the caster's anchor 0.9 up (0.55 for a zombie dog); target: the aimed hex's ground + 0.04; impactHexes: every
  // struck hex's ground + 0.04. World space.
  CausticVenom(glm::vec3 origin, glm::vec3 target, std::vector<glm::vec3> impactHexes, float start);

  static constexpr float kCharge = 0.38f, kImpact = 1.85f;
  static float travelSeconds(glm::vec3 origin, glm::vec3 target);
  float impactTime() const { return start_ + kCharge + travel_; }
  bool done(float now) const { return now - start_ >= kCharge + travel_ + kImpact; }
  float end() const { return start_ + kCharge + travel_ + kImpact; }

  // poison: the Poison V2 sheets (main, secondary, particles).
  void build(float now, const Camera& camera, const ElementSpritesV2& poison, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  glm::vec3 pathPosition(float t) const;

  glm::vec3 start3_, target_;
  std::vector<glm::vec3> hexes_;
  float start_ = 0, travel_ = 0, flightArc_ = 0;
  glm::vec3 pathSide_{0, 1, 0};
  std::vector<std::vector<VfxVertex>> ribbons_;  // the three glass ribbons (TubeGeometry, revealed with the flight)
};

}  // namespace ember
