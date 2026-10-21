#pragma once

// The Arqueiro's skills (docs/SKILLS_ARCHER.md, 2026-10-18), ours, in 3D on the effect runtime: a real arrow (a shaft of
// wood and a head of steel lit by the scene, a glint at the point) flies on a shallow arc from the bow to the body, each
// skill with its own trail and its own impact (the Combat material's streaks, shells, rings, trails, dust); the volleys
// fall from the sky onto every hex of their area and stay stuck in the ground a moment.
//
//   Flecha Envenenada  a green venom trail of motes; on the body a sick green burst, drips falling, a green light;
//   Flecha Explosiva   embers shed along the flight; on the body a flash, a fire shell bursting out over the neighbouring
//                      hexes, flames and embers flung up, a ring of fire on the ground, dust, a strong orange light;
//   Tiro Paralisante   a grey trail; the arrow goes in low: a burst of dust at the feet, grey shards, a dull grey ring
//                      pulsing under the foe while it is hobbled;
//   Tiro Mortal        a long crimson trail; a heavy pierce: a pin of light driven through, a crimson shell, dark streaks
//                      flung out, a red ring on the ground, a red light that lingers;
//   Tiro Sangrento     a dark red trail; on the body a red flash, a spray of blood flung the shot's way and falling, a dark stain
//                      spreading at the feet, a red light (2026-10-20);
//   Tiro Múltiplo      quick arrows, each with a thin silver-blue trail; a sharp white flash and a cross glint on the body,
//                      a few sparks (2026-10-20);
//   Chuva / Tempestade de Flechas  arrows come down at a slant from the archer's side onto every hex (three or five a hex,
//                      the Tempestade in two waves), a puff of dust and a glint where each lands, a flash on every foe.
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class RangerFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = a hex: a body's height is assumed)
  };
  enum class Shot { Venom, Explosive, Crippling, Death, Bloody, Swift };

  // One arrow from the archer standing at `from` (its card `height`) to `foe`: it lets go at `release`, lands at `impact`.
  void shot(Shot style, glm::vec3 from, float height, const Target& foe, float release, float impact);
  // Seconds an arrow flies between two units (SpellEffects::arrow's pace: 1 / 22 s per unit, 0.12..0.4 s).
  static float flightSeconds(glm::vec3 fromFeet, glm::vec3 toFeet);
  // A volley over `hexes` (their ground points), from the archer at `from`: the first arrows land at `impact`, the rest
  // just after; `foes` = the bodies standing in it (a flash on each); `big` = the Tempestade (more arrows, two waves).
  void volley(glm::vec3 from, const std::vector<glm::vec3>& hexes, const std::vector<Target>& foes, float start, float impact, bool big);

  bool active(float now) const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Shot, Volley };
  struct Effect {
    Kind kind;
    Shot style = Shot::Venom;
    bool big = false;
    glm::vec3 hand{0.0f}, body{0.0f}, feet{0.0f};  // the bow, the struck body, the struck feet (Shot)
    glm::vec3 from{0.0f};                         // the archer's feet (Volley)
    glm::vec3 dir{0.0f, 0.0f, 1.0f};
    float height = 1.8f;
    float start = 0.0f, release = 0.0f, impact = 0.0f;
    std::vector<glm::vec3> hexes;
    std::vector<Target> foes;
    uint32_t seed = 0;
    float end() const;
    glm::vec3 at(float s) const;  // the arrow's path, s 0..1 (hand -> body)
  };
  void buildShot(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildVolley(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  // An arrow (shaft + head, lit) at `at` pointing along `dir`, `scale` long (1 = a full arrow), faded by `alpha`.
  void arrow(glm::vec3 at, glm::vec3 dir, float scale, float alpha, glm::vec3 eye, float pull);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, shaftFirst_ = 0, headFirst_ = 0, chipFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, shaftCount_ = 0, headCount_ = 0, chipCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, shafts_, heads_, chips_;
};

}  // namespace ember
