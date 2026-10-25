#pragma once

// The combat's own effects, ours, in 3D (user, 2026-10-17: the VFX overhaul, "não importa como eram feitos faz novos",
// "só quero uma porra consistente bem feita"): one look, the same every time, no switch. It replaces the random flipbook
// "impact-N" sparks of SpellEffects::strike.
//
//   Impact (shared by every blow; the struck unit's card recoil is the stage's, MapStage::Recoil):
//     Physical  a weapon's contact: a flash at the point, sparks thrown away from the striker in 3D (streaks along their
//               velocity, falling), a puff of dust at the feet, a short warm light;
//     Heavy     a critical or a killing blow: the same, stronger, plus a pressure ring on the ground, a dust ring rolling
//               out and debris chunks thrown on arcs (lit by the scene like the board), bouncing once;
//     Magical   a school's energy (Arcano = violet): a core that flares and collapses, a pressure shell spreading, energy
//               streaks bursting out in 3D, slow motes, a ground ring, a strong light of the school's colour.
//   Bolt (the mage's plain attack, Arcano; Black Mage and the arcane casters), on one clock (no charging at the hand:
//   the casters' sprites already gather the spell, user 2026-10-17) -
//     launch   at the pose's release: a flare, then the bolt accelerates to the target's body on a low arc, a trail behind it,
//              embers shed along the path, its light travelling with it;
//     impact   when it arrives (= the rules' impact: damage, numbers, the hit pose and the recoil land then): the Magical
//              impact above.
//   Ember bolt (Isolde's plain attack and counter; the user, 2026-10-24: "a different visual spell FX, weaker than regular
//   spells ... in 3D matching her ember colors"): the same clock and path, a burning cinder in place of the energy -
//     launch   a spit of sparks thrown forward from the staff;
//     flight   a hot coal (white-orange heart, crimson skin) that flickers, a thin glowing trail, a wisp of dark smoke left
//              hanging and rising behind it, cinders shed that rise on the heat and wobble as they die, a flickering
//              warm light;
//     impact   the Magical impact in the ember palette (CombatPalette::of("ember")).
//
// Everything is world space (y up), drawn with the Combat material (shaders/vfx.hlsl). Points near a unit's card are
// pulled toward the eye (kPull: same place on screen, in front of the card, which is itself drawn 1.2 units nearer).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

// A school's colours: the light, the hot core, the outer energy.
struct CombatPalette {
  glm::vec3 light{0.62f, 0.32f, 1.0f};
  glm::vec3 core{0.96f, 0.88f, 1.0f};
  glm::vec3 energy{0.55f, 0.26f, 1.0f};
  // By the element the rules use ("arcane", "fire", "frost", "lightning", "ember", ...): Arcano for anything else.
  static CombatPalette of(const char* element);
};

class CombatFx {
 public:
  // Physical / Heavy: a blunt weapon's blow (mace, club, a beast's weight). Slash / HeavySlash: a blade's cut (user,
  // 2026-10-17: "deixa aquele impacto para BLUNT weapon, faz um SLASH para espadas") - a sheet of steel light swept
  // along the swing's arc across the body (a mesh in 3D, its plane square to the blow), a few sparks. Thrust: a spear's
  // stab, the same light drawn straight along the blow. MagicalMiss: the bolt finds nothing (the rules missed): its
  // energy comes apart past the body.
  // ArrowHit: an arrow driven into the body - its point punching through (a short streak of light along the shot), a
  // pin of light at the tip, splinters of the shaft (lit 3D pieces) kicked back and falling. HeavyArrowHit: a critical.
  // LevelUp: a hero gains a level (the user, 2026-10-20: "a level up FX in 3D"): two golden rings racing out over the ground, a
  // soft pillar of light rising through the body, sparks spiralling from the feet to above the head, a crown ring of light
  // over it, a warm golden light; nothing aimed (`from` is ignored).
  enum class Kind { Physical, Heavy, Magical, MagicalMiss, Slash, HeavySlash, Thrust, ArrowHit, HeavyArrowHit, LevelUp };
  // The bolt's look: Arcane (the energy bolt) or Ember (Isolde's burning cinder).
  enum class BoltLook { Arcane, Ember };

  // A blow lands at `at` on a unit standing at `feet` with its card `height` tall; `from` = where the striker stands
  // (the blow's direction).
  // `strength` < 1: a lesser blow (smaller, dimmer, fewer particles; Voss's plain attack, below his Míssil Mágico).
  void impact(Kind kind, glm::vec3 feet, float height, glm::vec3 from, float at, const CombatPalette& palette = {}, float strength = 1.0f);
  // The Arcano bolt: the caster's pose starts at `start`, it lets go at `release`, the bolt lands at `impact` (the
  // stage asks flightSeconds for it).
  void bolt(glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, float start, float release, float impact,
            const CombatPalette& palette = {}, float strength = 1.0f, BoltLook look = BoltLook::Arcane);
  // Seconds the bolt flies between two units (accelerating: slower at short range than a constant speed would be).
  static float flightSeconds(glm::vec3 fromFeet, glm::vec3 toFeet);

  bool active(float now) const;
  void clear() { impacts_.clear(), bolts_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Impact {
    Kind kind;
    glm::vec3 point;   // the contact (the body, on the striker's side)
    glm::vec3 feet;
    glm::vec3 dir;     // from the striker toward the struck, on the ground plane
    float height;
    float at;
    CombatPalette palette;
    uint32_t seed;
    float strength;
    float end() const;
  };
  struct Bolt {
    glm::vec3 hand, target;  // the hand at the release, the target's body
    glm::vec3 dir;
    float start, release, impact;
    float casterHeight;
    CombatPalette palette;
    uint32_t seed;
    float strength = 1.0f;
    BoltLook look = BoltLook::Arcane;
    float end() const { return impact + (look == BoltLook::Ember ? 1.0f : 0.65f); }  // its embers (an ember bolt's smoke) outlive it
    glm::vec3 at(float s) const;  // the path, s 0..1 (hand -> target)
    float progress(float now) const;  // 0..1 along the path (accelerating)
  };
  void buildImpact(const Impact& i, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildBolt(const Bolt& b, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildEmberBolt(const Bolt& b, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Impact> impacts_;
  std::vector<Bolt> bolts_;
  uint32_t serial_ = 0;
  // This frame's shared geometry (added once per build) and particle batches.
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, rockFirst_ = 0, splinterFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, rockCount_ = 0, splinterCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, rocks_, splinters_;
};

}  // namespace ember
