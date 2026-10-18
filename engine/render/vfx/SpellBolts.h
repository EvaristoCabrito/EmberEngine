#pragma once

// Two bolt spells, ours, in 3D (the user, 2026-10-18: "do a new 3D effect for Divine Bolt and Magic Missile, the current
// ones are too bland"). One look each, the same on every cast; drawn with the Combat and Holy materials (shaders/vfx.hlsl).
// No charge at the hand (the casters' sprites already gather the spell, user 2026-10-17): it starts as the pose lets go.
//
//   Arcane  Míssil Mágico 03: a flare and a ring at the hand; the bolt (a white-hot core in a boiling violet shell) flies a
//           curving, swerving path, two ribbons of arcane light twisting round it in a helix behind a bright trail, rings
//           of force rippling off its wake, embers shed; on the body it implodes, then bursts: a flash, a shell of force,
//           three rings spinning apart like a gyroscope, energy streaks flung out in 3D, a ring racing over the ground,
//           motes spiralling up, a violet light that flares and dies.
//   Divine  Raio Divino 01: a golden flare and a halo at the hand; a lance of light (a long white-gold spear) thrown
//           straight at the target, a beam left hanging behind it, golden feathers of light drifting down off it; on the
//           hit a column of light slams down from the sky onto the struck hex, a radiant burst of rays, a shockwave of
//           gold racing over the whole struck area, every hex of the splash flaring as it reaches it, motes rising, a warm
//           light.
//
// World space (y up). The hit lands at `impact` (the rules' damage, numbers and hit pose land then).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class SpellBolt {
 public:
  enum class Style { Arcane, Divine };
  // casterFeet / casterHeight: the caster and its card's height; target: the struck body (feet + height); splash: every
  // struck hex's ground (Divine; the centre included), may be empty. It lets go at `release`, lands at `impact`.
  SpellBolt(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, std::vector<glm::vec3> splash,
            float release, float impact, uint32_t seed);

  // Seconds from the release to the hit.
  static float flightSeconds(Style style, glm::vec3 fromFeet, glm::vec3 toFeet);
  float end() const { return impact_ + (style_ == Style::Divine ? 1.6f : 1.2f); }
  bool done(float now) const { return now >= end(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

 private:
  glm::vec3 at(float s) const;          // the path, s 0..1 (hand -> body)
  float progress(float now) const;      // 0..1 along it (accelerating)
  void buildArcane(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildDivine(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  Style style_;
  glm::vec3 hand_{0.0f}, body_{0.0f}, feet_{0.0f}, dir_{0.0f, 0.0f, 1.0f}, side_{1.0f, 0.0f, 0.0f};
  float casterHeight_ = 2.0f, targetHeight_ = 2.0f;
  std::vector<glm::vec3> splash_;
  float release_ = 0.0f, impact_ = 0.0f;
  float swerve_ = 1.0f;  // which side the Arcane bolt curves to
  uint32_t seed_ = 0;
};

}  // namespace ember
