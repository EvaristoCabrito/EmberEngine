#pragma once

// Holy light, ours, in 3D on the effect runtime (2026-11-13, the user's brief: "holy light reads as convincing when it behaves
// like real light: it comes from above, it lights the scene around it, and it stays warm white instead of flat yellow"). Drawn
// with the Holy material's soft looks (shaders/vfx.hlsl modes 5..8: god rays, the ground's circle, the bloom, the beam) and its
// motes. Warm white at the core, pale gold in the middle, soft gold at the edges; everything feathered, nothing sharp; circles and
// radial rays; the light comes down and the sparkles go up; every one carries a real light on the scene.
//
// Each spell has its own shape, sized to what the rules do:
//   Minor    Cura Menor (one ally at range, a small heal): one soft pillar of light on the ally, a small circle, a few motes;
//   Medium   Cura Média (one ally at range, a strong heal): the sky opens over the ally (a ring of light high above), a wide fan of
//            god rays comes down through it, a halo settles from over the head to the feet, a great circle, a cloud of motes;
//   Hands    Mãos Curativas (an ally right beside the healer): no sky: the light pours from the healer's hands into the ally along
//            a soft arc, gathers in the ally's body from feet to head and breathes out of it;
//   Disease  Curar Doença (removes disease and poison): pale and clean: two rings of light sweep up through the body from the feet,
//            the sickness leaves it as dark motes thrown outward, a wide clean circle spreads;
//   Bolt     Raio Divino (the aimed hex struck hard, the hexes round it less): light gathers high over the hex and a thin beam
//            marks the spot; then the strike, a great white beam from the sky for a blink, a flash, a ring of light over the whole
//            area, sparks thrown out, the scene lit for a moment; the beam thins to a line and goes, a glow stays on the ground.
//
// World space (y up), one hex radius = 1. The light lands at `impact`.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class HolySpell {
 public:
  enum class Style { Minor, Medium, Hands, Disease, Bolt };
  static float leadIn(Style style) { return style == Style::Bolt ? 0.6f : style == Style::Medium ? 0.55f : 0.4f; }
  // The caster's feet and card height; the body the light lands on (its feet and height); the hexes round it (the Bolt's area);
  // the light lands at `impact`.
  HolySpell(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, std::vector<glm::vec3> area, float impact, uint32_t seed);

  float end() const { return end_; }
  bool done(float now) const { return now >= end_; }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

 private:
  Style style_;
  glm::vec3 caster_{0.0f}, feet_{0.0f};
  float casterHeight_ = 2.0f, height_ = 2.0f, impact_ = 0.0f, start_ = 0.0f, end_ = 0.0f, radius_ = 1.0f;
  std::vector<glm::vec3> area_;
  uint32_t seed_ = 0;
};

}  // namespace ember
