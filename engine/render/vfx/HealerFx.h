#pragma once

// The Curandeiro's spells (docs/SKILLS_HEALER.md, 2026-10-20), ours, in 3D on the effect runtime (the Holy and Combat
// materials: shafts of light, pools, sigils, motes, rings, energy shells, streaks, dust), one look each, the same on every
// cast:
//
//   Círculo de Cura  a sigil draws itself on the ground over the whole area while the light gathers; at the impact a wave
//                    of light runs from its centre to the edge, and on every ally it reaches a shaft of warm light comes
//                    down, a halo settles from the head to the feet, motes rise, the ground glows under it; a light at the
//                    centre and on the healed;
//   Ressurreição     motes spiral in and down onto the fallen body while a thin pillar from the sky brightens over it and a
//                    star draws under it; at the impact a flash at the chest, the pillar flares wide and withdraws upward,
//                    rings run across the ground, rays of light run out, two wings of light unfurl from the back and hold
//                    while a halo rises from the feet past the head, motes burst up like feathers; a strong light that
//                    lingers;
//   Julgamento       a sigil turns high in the air over the aimed hex while motes stream up into it; a blade of white light
//                    drives down through it into the ground at the impact: a flash, a pressure shell out to the edge of the
//                    area, rings and rays across the ground, dust lifting off every hex; on each foe a column of holy fire
//                    rises, sparks fly up, a ring burns at its feet; the blade stands a moment, then rises and fades.
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class HealerFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = not known: a body's height is assumed)
  };
  // Seconds each spell's light gathers before its impact (the rules' hit lands on the impact).
  static float leadIn(const std::string& kind);

  // Círculo de Cura over the area round `centre` (`radius` to its edge), on the allies it reaches; the caster at `from`.
  void circle(glm::vec3 from, float fromHeight, glm::vec3 centre, float radius, std::vector<Target> allies, float impact);
  // Ressurreição on the fallen body; the caster at `from`.
  void resurrection(glm::vec3 from, float fromHeight, const Target& fallen, float impact);
  // Julgamento over the area round `centre`, on the foes standing in it.
  void judgment(glm::vec3 from, float fromHeight, glm::vec3 centre, float radius, std::vector<Target> foes, float impact);

  bool active(float now) const;
  float latestEnd() const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Circle, Resurrection, Judgment };
  struct Effect {
    Kind kind;
    glm::vec3 from{0.0f}, centre{0.0f};  // the caster's feet; the area's centre (the fallen body's feet)
    float fromHeight = 1.8f, height = 1.8f, radius = 2.0f;
    float impact = 0.0f;
    std::vector<Target> bodies;
    uint32_t seed = 0;
    float end() const;
  };
  void buildCircle(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildResurrection(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildJudgment(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, columnFirst_ = 0, haloFirst_ = 0, sigilFirst_ = 0, starFirst_ = 0, bladeFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, columnCount_ = 0, haloCount_ = 0, sigilCount_ = 0, starCount_ = 0, bladeCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, motes_;
};

}  // namespace ember
