#pragma once

// Poison and disease on a unit (ours, 3D on the effect runtime, 2026-10-20; the user: Ember drew them in WebGL and they were
// missing here, "recria algo similar em 3D"). Ember's drawStatusFx was a flat sheet at the unit's feet: a faint haze, eight
// motes rising feet to head and two thin wavering vapour lines, green and quick for poison, violet and slow for disease, both
// layered when a unit has both. Here the same idea stands in the world round the body:
//
//   a stain of light on the ground at the feet, pulsing (the Combat material's ring);
//   a vapour of the condition's colour rising and spreading round the body (alpha puffs lit by the scene), wrapping it:
//     the puffs on the eye's side are drawn over the card, the far ones behind it;
//   motes of light spiralling from the feet to the head and fading as they rise;
//   wisps of vapour (thin tubes of light) climbing and wavering up the sides;
//   a faint light of the colour on the body and the ground.
//
// One effect per condition per unit, standing for as long as the rules keep the condition (MapStage syncStatus): start()
// returns its id, move() follows the unit, end() fades it out. World space (y up).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class StatusFx {
 public:
  enum class Kind { Poison, Disease };

  // The condition standing on the body with its feet at `feet` and a card `height` tall (0 = a body's height), from `now`.
  int start(Kind kind, glm::vec3 feet, float height, float now);
  void move(int id, glm::vec3 feet, float height);
  void end(int id, float now);

  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Effect {
    Kind kind = Kind::Poison;
    int id = 0;
    glm::vec3 feet{0.0f};
    float height = 1.8f;
    float start = 0.0f, finish = -1.0f;  // finish: when it was ended (-1 = standing)
    uint32_t seed = 0;
  };
  void buildOne(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Effect> effects_;
  int serial_ = 0;
  uint32_t quadFirst_ = 0, quadCount_ = 0, discFirst_ = 0, discCount_ = 0;
  std::vector<VfxInstance> motes_, puffs_;
};

}  // namespace ember
