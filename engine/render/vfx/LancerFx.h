#pragma once

// The Lanceiro's skills (docs/SKILLS_LANCER.md, 2026-10-18), ours, in 3D on the effect runtime (the user: every spell
// effect in 3D, never flipbook cards): the Combat material's streaks, dust, shells, rings, trails and sheets of light
// (shaders/vfx.hlsl kMaterialCombat), pieces lit by the scene (Standard) for the lance, the hook, a knocked-away weapon
// and the chips of ground. The element is in the effects only (ember light on the hook and the vigil, steel on the
// thrusts); the rules move bodies, the cards fly in MapStage (unitLeap_), these effects follow them. One look each, 01.
//
//   Arremesso           the lance goes in low under the foe (a thrust sheet to its feet, a burst of dust), the body is
//                       lifted (a streak up); the flight: a faint wake behind the card, the landing a crater (shell,
//                       ground ring, dust, chips), and against another body a flash of impact and sparks;
//   Gancho              the hook (a lit steel point) flies on an ember-lit chain from the hand to the chest, bites with
//                       a flash; the pull: the chain stays taut to the body as it is dragged, dust rolling behind it;
//   Salto de Vara       the lance planted beside the foe (a lit shaft leaning over as the vault goes), a trail of steel
//                       light over the arc, dust at the plant and the landing; the blow from behind: a thrust with a
//                       white-hot flash and sparks;
//   Desarmar            a thrust at the hands with a cross glint; on a hit the weapon (a lit blade) spins away on an
//                       arc, bounces and lies in the dust before it fades;
//   Troca de Posto      two trails cross (steel for the lancer, gold for the ally), dust at each take-off and landing,
//                       a burst where they pass; then the blow from the new post;
//   Empalar e Arrastar  the thrust into the body; the lance stays in it and is drawn between the two as they run,
//                       dust and sparks streaming behind the dragged body, dark scrapes left on the ground, a jolt at
//                       the end;
//   Vigília da Sentinela the stance rises (an ember ring out to the radius, a flash on the lance), then stands: a slow
//                       ember pulse at the feet, motes rising, the lance point aglow, a warm light; the interrupt: a
//                       white-hot thrust at the attacker with an ember burst.
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class LancerFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = a hex: a body's height is assumed)
  };

  // Arremesso: the lift of `foe` at `impact` by the lancer standing at `from`.
  void toss(const Target& foe, glm::vec3 from, float impact);
  // ... and the body's flight from `from` to `to` (feet) over [start, finish] on an arc `arc` high, `height` = its card;
  // `collided`: it lands against another body.
  void tossFlight(glm::vec3 from, glm::vec3 to, float height, float arc, float start, float finish, bool collided);
  // Gancho: the hook thrown from the lancer at `from` (card `height`) at `release`, biting `foe` at `impact`.
  void hook(glm::vec3 from, float height, const Target& foe, float release, float impact);
  // ... and the pull: the body dragged from `from` to `to` over [start, finish], the chain taut from the lancer at `caster`.
  void hookPull(glm::vec3 caster, float casterHeight, glm::vec3 from, glm::vec3 to, float height, float start, float finish);
  // Salto de Vara: the vault from `from` (card `height`) over the foe at `over` onto `landing`, over [start, land].
  void vault(glm::vec3 from, float height, glm::vec3 over, glm::vec3 landing, float start, float land);
  // ... and the blow from behind on `foe` at `impact`, from `from`.
  void vaultBlow(const Target& foe, glm::vec3 from, float impact);
  // Desarmar: the thrust at the hands of `foe` at `impact`; disarmed(): its weapon knocked away at `at` (on a hit).
  void disarm(const Target& foe, glm::vec3 from, float impact);
  void disarmed(const Target& foe, glm::vec3 from, float at);
  // Troca de Posto: `a` (card `aHeight`) and `b` change places over [start, finish]; swapBlow: the blow from the new post.
  void swap(glm::vec3 a, float aHeight, glm::vec3 b, float bHeight, float start, float finish);
  void swapBlow(const Target& foe, glm::vec3 from, float impact);
  // Empalar e Arrastar: the thrust into `foe` at `impact` from `from`; the drag over [start, finish]: the foe from
  // `foeFrom` to `foeTo` (card `foeHeight`), the lancer from `casterFrom` to `casterTo`.
  void impale(const Target& foe, glm::vec3 from, float impact);
  void drag(glm::vec3 foeFrom, glm::vec3 foeTo, float foeHeight, glm::vec3 casterFrom, glm::vec3 casterTo, float casterHeight, float start, float finish);
  // Vigília da Sentinela: the stance rising at `impact` (the ring out to `radius` world units); the standing effect from
  // vigilStart (its id; vigilMove follows the sentinel, vigilEnd fades it from `now`); the interrupt's thrust on `foe`.
  void vigilRise(glm::vec3 feet, float height, float radius, float impact);
  int vigilStart(glm::vec3 feet, float height, float radius, float now);
  void vigilMove(int id, glm::vec3 feet);
  void vigilEnd(int id, float now);
  void vigilStrike(const Target& foe, glm::vec3 from, float impact);

  bool active(float now) const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Toss, TossFlight, Hook, HookPull, Vault, VaultBlow, Disarm, Disarmed, Swap, SwapBlow, Impale, Drag, VigilRise, Vigil, VigilStrike };
  struct Effect {
    Kind kind;
    int id = 0;                        // Vigil: its handle
    glm::vec3 feet{0.0f}, to{0.0f};    // the struck body's feet (a flight: where it starts) / where the flight ends
    glm::vec3 from{0.0f}, to2{0.0f};   // the lancer's feet / the second mover's end (Drag: the lancer's; Swap: b's)
    glm::vec3 dir{0.0f, 0.0f, 1.0f};   // from the striker toward the struck, on the ground
    float height = 1.8f, height2 = 1.8f;
    float arc = 0.0f, radius = 0.0f;
    float start = 0.0f, impact = 0.0f, finish = 0.0f;  // finish: a flight's end; Vigil: when it was ended (-1 = on)
    bool collided = false;
    uint32_t seed = 0;
    float end() const;
  };
  void buildToss(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildTossFlight(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildHook(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildHookPull(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildVault(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildThrustBlow(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildDisarmed(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildSwap(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildDrag(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildVigilRise(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildVigil(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  // A sheet of light drawn from `a` to `b` (bowed sideways by `bow`) as the lance goes, `sweep` seconds to its head.
  void thrustSheet(glm::vec3 a, glm::vec3 b, glm::vec3 side, float bow, float width, float intensity, glm::vec3 colour, float age, float sweep, glm::vec3 eye,
                   glm::vec3 sight, VfxFrame& frame);
  // A chain from `a` to `b` sagging `sag` (a tube of ember light, links along it).
  void chain(glm::vec3 a, glm::vec3 b, float sag, float alpha, float age, glm::vec3 eye, glm::vec3 sight, VfxFrame& frame);
  // The lance (shaft and head, lit) with its point at `tip` pointing along `dir`, `length` long.
  void spear(glm::vec3 tip, glm::vec3 dir, float length, float alpha, glm::vec3 eye, float pull);
  // Chips of the ground (or shards of steel) thrown from `at` on arcs, bouncing once, lying still and fading.
  void debris(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 colour, float age, glm::vec3 eye, uint32_t salt);
  // Sparks flung from `at` the way of `bias`, falling.
  void sparks(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float k, float age, glm::vec3 eye, uint32_t salt, glm::vec3 warm);
  // A ring of dust rolling out of `at` from `born` on.
  void dustRing(const Effect& e, glm::vec3 at, float k, int count, float reach, float born, float age, glm::vec3 eye, uint32_t salt);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  int vigilSerial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, shaftFirst_ = 0, headFirst_ = 0, rockFirst_ = 0, bladeFirst_ = 0, guardFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, shaftCount_ = 0, headCount_ = 0, rockCount_ = 0, bladeCount_ = 0, guardCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, shafts_, heads_, rocks_, blades_, guards_;
};

}  // namespace ember
