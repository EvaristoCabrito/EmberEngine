#pragma once

// Arcane magic, ours, in 3D on the effect runtime (2026-11-13, the user's brief: "precise geometry with unstable energy inside
// it"). The energy is the Arcane material (shaders/vfx.hlsl): a noise warped by a second, swirling one and read through a ramp,
// near black violet at its edges, deep purple, magenta, pink, almost white at the core; the world behind it bent. Round it the
// order: the original spells' own sigil (vfx/SigilParts.h: the fine ring with its marks and the hexagram), two of them turning
// against each other. It moves in orbits and spirals; it is drawn in to the hand, stalls a beat, and lets go; its threads of
// energy snap between points in steps, never smoothly.
//
//   Missile     Míssil Mágico (a bolt of force on one foe in sight): the sigil stands in front of the caster's hand, light is
//               drawn in through it and stalls; the bolt lets go, a core of energy flying a tight spiral with wisps in orbit and
//               threads snapping off it; where it lands a flash, a sharp ring over the ground, the runes scattering;
//   Phantasmal  Força Fantasmagórica (an illusion that wounds a foe in sight): no flight: a sigil draws itself under the foe and a
//               second turns over its head; between them the energy gathers out of nothing round the body, threads snapping from
//               the rings into it; then it folds in on the foe all at once and is gone.
//
// World space (y up), one hex radius = 1. The hit lands at `impact`.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class ArcaneSpell {
 public:
  enum class Style { Missile, Phantasmal };
  static constexpr float kMissileSpeed = 20.0f;  // world units a second
  static constexpr float kGather = 0.55f;        // the pull in and the stall before the Missile lets go
  static constexpr float kPhantasm = 1.0f;       // the Phantasmal's gathering before it folds in
  // Seconds from the cast's start to the hit.
  static float lead(Style style, glm::vec3 casterFeet, glm::vec3 targetFeet);

  // It gathers from `start`; the hit lands at `impact`, or as soon after `start` as the gathering and the flight allow.
  ArcaneSpell(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, float start, float impact, uint32_t seed);

  // The Mago Negro's plain ranged attack: the Missile at `scale` of its size, let go at `release` and landing at `impact` (the
  // attack pose's own beats).
  void asAttack(float start, float release, float impact, float scale, int unit = -1);
  // A plain attack whose art shows the hand (content/fx/hand-bolts.json, look "arcane"): the energy gathers at the palm as drawn
  // this frame (`hand`, world) until it lets go; `unit`: the striker's scene unit, as given to asAttack.
  void aimFrom(int unit, glm::vec3 hand, float now);

  float end() const { return end_; }
  bool done(float now) const { return now >= end_; }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

 private:
  Style style_;
  glm::vec3 hand_{0.0f}, feet_{0.0f}, chest_{0.0f}, direction_{0.0f, 0.0f, 1.0f};
  float height_ = 2.0f, start_ = 0.0f, release_ = 0.0f, impact_ = 0.0f, end_ = 0.0f;
  float scale_ = 1.0f;
  int unit_ = -1;
  uint32_t seed_ = 0;
};

}  // namespace ember
