#pragma once

// The light of the cures, the meal and the potions: ours, in 3D (user, 2026-10-16: "make new similar ones now in 3d",
// after Engine2's 2D holy art, engine.ts drawDivineLight / drawPotionBurst, was set aside on branch wip/2d-canvas-fx).
// Cura Menor 02, Cura Média 01, Mãos Curativas 01, Curar Doença 01, Criar Comida e Água 01, Poção 01.
//
// On every unit it reaches, a shaft of light comes down from above (a volume the eye sees through, brightest at its
// heart, rays rising inside it), the ground under the unit glows with a widening ring, motes of light rise around it,
// and a real light of the spell's colour shines on the board and the units nearby. Each style adds its own:
//   Cura Menor       a slim golden shaft, a few motes;
//   Cura Média       a taller, wider shaft and a halo that settles from above the head down to the feet;
//   Mãos Curativas   a short warm shaft and two cupped arcs of light closing around the body, motes drawn inward;
//   Curar Doença     a pale teal shaft; the sickness drawn out of the body as dark motes; a clean ring spreading wide;
//   Criar Comida e Água  a cool shaft on each fed ally, bread-gold and water-blue motes spiralling into it;
//   Poção            no shaft: a burst at the chest, bubbles rising, a splash ring (red-orange heals, green cures,
//                    blue restores spells).
// The light lands on `impact` (the rules apply the heal then); it begins leadIn() before. Drawn with the Holy material
// (shaders/vfx.hlsl) over the scene; everything here is world space (y up), metres.

#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class HolyLight {
 public:
  enum class Style { Minor, Medium, Hands, Disease, Food, PotionHeal, PotionCure, PotionMana };
  struct Target {
    glm::vec3 feet{0.0f};  // where the unit stands
    float height = 2.0f;   // its card as drawn: the light is sized to it
  };
  HolyLight(Style style, std::vector<Target> targets, float impact, uint32_t seed);

  static float leadIn(Style style);
  float end() const;
  bool done(float now) const { return now >= end(); }

  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  Style style_;
  std::vector<Target> targets_;
  float impact_ = 0.0f;
  uint32_t seed_ = 0;
};

}  // namespace ember
