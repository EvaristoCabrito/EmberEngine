#pragma once

// Garam Holt's techniques, the Guardião da Lua (docs/SKILLS_MOONWARDEN.md, 2026-11-13), ours, in 3D on the effect runtime: the Combat
// material's sheets, shells, ground rings, columns and streaks (shaders/vfx.hlsl kMaterialCombat) in moonlight - pale silver light, a
// white core, a cold blue edge. One look each, version 01 (the user: "maybe you should create the visual effects for these new skills").
//
// The T1-T10 set (the user, 2026-11-13). His blows (on the struck foe, as the blow lands):
//   Golpe Firme        a silver crescent swept across the chest, a ring of light on Garam's shield;
//   Passo de Flanco,   a lance of moonlight driven straight through the body, sparks where it breaks the armour;
//   Vigiar a Brecha    (the reaction blow)
//   Quebrar o Passo    a heavy crescent brought straight down, a ground ring, dust, a dim ring settling on the foe (its next step weighed);
//   Golpe Seguido      on each foe: the shield's face, a flat shell bursting at the chest, a ground ring, moon dust;
//   Acerto de Contas   a full moon flaring behind the foe, a great crescent across it, a column of light.
// His guards (on whoever they mark):
//   Firmar o Pé        a shell of moonlight round Garam, a crescent ring at his feet;
//   Cerrar Fileiras,   a thread of light from Garam to the ally, a ring at the ally's feet;
//   Puxar para Longe
//   Vigiar a Brecha    a crescent sigil on the ground of the watched hex;
//   Assumir o Peso     a barrier sphere round Garam and the ally he covers, a column of light rising through it.
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

  // One of his blows (`kind`: steadyStrike, flankStep, breakStride, watchTheGap, followThrough, reckoning) on `foe` at `impact`, Garam
  // standing at `from` (his card `height`); `landed`: the blow found the body (a miss draws the swing only).
  void strike(const std::string& kind, const Target& foe, const Target& from, float impact, bool landed);
  // One of his guards (`kind`: digIn, closeRanks, pullClear, watchTheGap, takeTheWeight) from Garam (`self`) on `marked` at `at`.
  void ward(const std::string& kind, const Target& self, const std::vector<Target>& marked, float at);

  bool active(float now) const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { GuardedCut, Bash, Thrust, Crush, Reckoning, Brace, Interpose, Affront, Hold, Unbreakable };
  struct Effect {
    Kind kind;
    Target at;      // the struck foe, or Garam for a guard
    Target self;    // Garam
    glm::vec3 dir{0.0f, 0.0f, 1.0f};  // from Garam toward the struck, on the ground
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
