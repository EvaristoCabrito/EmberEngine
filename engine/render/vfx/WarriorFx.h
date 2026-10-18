#pragma once

// The Guerreiro's skills (docs/SKILLS_WARRIOR.md, 2026-10-18), ours, in 3D on the effect runtime (the user: every spell
// effect in 3D, never flipbook cards): the Combat material's streaks, dust, energy shells, ground rings, trails and blade
// sheets (shaders/vfx.hlsl kMaterialCombat), pieces lit by the scene (Standard) for what is thrown. One look each, version 01.
//
//   Redemoinho de Aço  a sheet of steel light turns a full circle round the caster at weapon height, its edge white-hot,
//                      sparks flung off it, a ring of dust rolling out, a flash on every foe as the edge reaches it, a
//                      light riding the edge;
//   Quebra-Guarda      the blade comes down (a vertical cut sheet); on the impact armour shards (lit chips of steel)
//                      burst from the chest and fall, bouncing once, a crack ring on the ground, an orange flash; then a
//                      broken ring of dull red light turns over the chest for a few seconds;
//   Grito de Guerra    a pressure shell bursts from the chest out to the radius, the ground ring with it, dust lifted off
//                      every hex it passes; a thin column of red-gold light rises on every ally with a warm light; on the
//                      foes a dark pulse;
//   Salto Devastador   the flight: a trail of steel light behind the weapon along the arc; the landing: a crater - a
//                      pressure shell, a ground ring, a ring of dust, debris on arcs; then the blow: a big downward cut
//                      sheet over the foe, sparks, a strong light.
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class WarriorFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = a hex: a body's height is assumed)
  };

  // The caster at `feet` (its card `height`); the blade turns from impact - 0.22 s and lands on `foes` on the way.
  void whirlwind(glm::vec3 feet, float height, const std::vector<Target>& foes, float impact);
  // The blow on `foe` at `impact`, from the striker standing at `from`.
  void guardBreak(const Target& foe, glm::vec3 from, float impact);
  // The cry at `impact`: the shell out to `radius` (world units), every ally's column (the caster among them).
  void warCry(glm::vec3 feet, float height, const std::vector<Target>& allies, float radius, float impact);
  // ... and the dark pulse on the foes it frightens.
  void warCryFear(const std::vector<Target>& foes, float impact);
  // The leap from `from` to `landing` over [start, impact] (the crater at impact), the caster's card `height`.
  void leap(glm::vec3 from, float height, glm::vec3 landing, float start, float impact);
  // The blow after the landing: on `foe` at `impact`, from `from`.
  void leapBlow(const Target& foe, glm::vec3 from, float impact);

  bool active(float now) const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Whirlwind, GuardBreak, WarCry, Fear, Leap, LeapBlow };
  struct Effect {
    Kind kind;
    glm::vec3 feet{0.0f};   // the caster's (Whirlwind, WarCry, Leap: where it takes off) or the struck foe's
    glm::vec3 to{0.0f};     // Leap: the landing
    glm::vec3 dir{0.0f, 0.0f, 1.0f};  // from the striker toward the struck, on the ground
    float height = 1.8f;
    float radius = 0.0f;    // WarCry: the shell's reach
    float start = 0.0f, impact = 0.0f;
    std::vector<Target> targets;
    uint32_t seed = 0;
    float end() const;
  };
  void buildWhirlwind(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildGuardBreak(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildWarCry(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildFear(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildLeap(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildLeapBlow(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  // A cut sheet drawn in from `top` down to `bottom` (bowed toward the striker), as the blade passes.
  void downwardCut(glm::vec3 top, glm::vec3 bottom, glm::vec3 dir, float width, float intensity, float age, float sweep, glm::vec3 eye, glm::vec3 sight,
                   VfxFrame& frame);
  // Debris thrown from `at` on arcs, bouncing once, lying still and fading: chips of the ground, or shards of steel.
  void debris(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 colour, bool chips, float age, glm::vec3 eye, uint32_t salt);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, rockFirst_ = 0, chipFirst_ = 0, columnFirst_ = 0, arcFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, rockCount_ = 0, chipCount_ = 0, columnCount_ = 0, arcCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, rocks_, chips_;
};

}  // namespace ember
