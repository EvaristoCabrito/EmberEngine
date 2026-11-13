#pragma once

// Garran Holt's techniques, the Guardião da Lua (docs/SKILLS_MOONWARDEN.md, 2026-11-13), ours, in 3D on the effect runtime: the Combat
// material's sheets, shells, ground rings, columns and streaks (shaders/vfx.hlsl kMaterialCombat) in moonlight - pale silver light, a
// white core, a cold blue edge. One look each, version 01 (the user: "maybe you should create the visual effects for these new skills").
//
// His blows (on the struck foe, as the blow lands):
//   Corte Guardado     a silver crescent swept across the chest, a ring of light on Garran's shield as the guard comes up;
//   Pancada de Escudo  the shield's face: a flat shell bursting at the foe's chest toward the push, a ground ring, moon dust;
//   Estocada na Brecha a lance of moonlight driven straight through the body, sparks where it breaks the armour;
//   Golpe Esmagador    a heavy crescent brought straight down, a ground ring, dust, a dim ring settling on the foe (its next blow
//                      weighed down);
//   Acerto de Contas   a full moon flaring behind the foe, a great crescent across it, a column of light.
// His guards (on whoever they mark):
//   Erguer o Escudo    a shell of moonlight round Garran, a crescent ring at his feet;
//   Interpor           a thread of light from Garran to the ally he covers, a ring at the ally's feet;
//   Afronta            a thread of light to the challenged foe, a crescent sigil on the ground under it;
//   Segurar a Linha    a ring under every ally he holds, threads to each;
//   Inquebrável        a barrier sphere round Garran (and the ally, under the waning moon), a column of light rising through it.
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class MoonwardenFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height
  };

  // One of his blows (`kind`: guardedCut, wardenBash, gapThrust, crushingBlow, reckoning) on `foe` at `impact`, Garran standing at
  // `from` (his card `height`); `landed`: the blow found the body (a miss draws the swing only).
  void strike(const std::string& kind, const Target& foe, const Target& from, float impact, bool landed);
  // One of his guards (`kind`: brace, interpose, affront, holdTheLine, unbreakable) from Garran (`self`) on `marked` at `at`.
  void ward(const std::string& kind, const Target& self, const std::vector<Target>& marked, float at);

  bool active(float now) const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { GuardedCut, Bash, Thrust, Crush, Reckoning, Brace, Interpose, Affront, Hold, Unbreakable };
  struct Effect {
    Kind kind;
    Target at;      // the struck foe, or Garran for a guard
    Target self;    // Garran
    glm::vec3 dir{0.0f, 0.0f, 1.0f};  // from Garran toward the struck, on the ground
    std::vector<Target> targets;      // a guard's marked units
    float start = 0.0f;
    bool landed = true;
    uint32_t seed = 0;
    float end() const;
  };
  void buildStrike(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildWard(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  // A crescent sheet swept along an arc round `centre` (in the plane of `u` and `v`), radius `r`, from angle a0 to a1.
  void crescent(glm::vec3 centre, glm::vec3 u, glm::vec3 v, float r, float a0, float a1, float width, float intensity, float age, float sweep,
                glm::vec3 eye, glm::vec3 sight, VfxFrame& frame);
  void ring(glm::vec3 feet, float radius, float opacity, float age, glm::vec3 eye, glm::vec3 sight, VfxFrame& frame);
  void shell(glm::vec3 centre, glm::vec3 scale, float opacity, float age, glm::vec3 sight, VfxFrame& frame);
  void column(glm::vec3 feet, float radius, float height, float opacity, float age, glm::vec3 eye, glm::vec3 sight, VfxFrame& frame);
  void thread(glm::vec3 a, glm::vec3 b, float width, float alpha, float age, uint32_t seed, glm::vec3 eye);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, columnFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, columnCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_;
};

}  // namespace ember
