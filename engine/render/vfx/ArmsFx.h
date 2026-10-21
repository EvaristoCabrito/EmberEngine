#pragma once

// The weapon skills, the auras and the summons that had no effect of their own (the user, 2026-10-20: "esses efeitos eram
// em webgl por isso não compatíveis, pode fazer novos efeitos 3D para todas que estão faltando"), ours, in 3D on the effect
// runtime: the Combat material's streaks, dust, energy shells, ground rings, trails and cut sheets, pieces lit by the scene
// (Standard) for what is thrown. One look each, version 01.
//
//   Provocar            a clang of steel at the warrior's weapon, a red challenge ring rolling out over the board; on every
//                       foe it reaches a red tether drawn taut to the warrior, then snapping, and a red sigil turning over
//                       its head;
//   Golpe de Escudo     the shield's face meets the chest: a vertical shock ripple, a gold flash, dust at the feet, a few
//                       chips; then gold stars circling over the stunned head;
//   Investida Touro     dust torn up behind the charge; on the blow a horned burst flung forward, a shock shell, a skid
//                       ring stretched the push's way, stones thrown forward, a strong light;
//   Golpe do Carrasco   a pillar of red light falls on the foe, a heavy dark-red cut comes down through it, a red spray,
//                       dark dust, a red ring on the ground, a red light that lingers;
//   Rasteira            a low crescent of steel at the ankles, dust kicked up, drops of blood falling, a grey ring;
//   Investida de Ombro  a wall of pressure pushed out over the hexes in front, dust rolling with it, stones, a flash on
//                       every body it meets;
//   Debandada           hoofbeats along the line: a ground ring, a burst of dust and stones on every hex one after the
//                       other, a dust wall rolling down the line, a flash on every body, a light running with it;
//   Ira Divina          a beam of holy light races down the line low over the ground, a column of white-gold light bursts
//                       on every hex as it passes, motes rising, a flash on every foe;
//   Aura de Proteção    a gold dome rises over the area and settles, a ring of gold glowing on the ground at its edge,
//                       motes rising inside, a column of light on every ally under it;
//   Presença Intimidante a dark crimson wave rolls out over the area, a dark red ring on the ground, a dark pulse on every
//                       foe under it;
//   the summons         a conjuring circle on the hex (a ring and a turning rune), motes drawn in, a column of light, a
//                       flash as the familiar comes - each in its own colour (Familiar violet, Maior blue, Radiante
//                       white-gold, Titã ember, Cão Zumbi sick green).
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class ArmsFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = a hex: a body's height is assumed)
  };
  enum class Summon { Familiar, Greater, Radiant, Titan, ZombieDog };

  // The warrior at `feet` (its card `height`) calls out at `impact`; `foes` are the ones it provokes.
  void provoke(glm::vec3 feet, float height, const std::vector<Target>& foes, float impact);
  // A blow on `foe` at `impact`, from the striker standing at `from`.
  void shieldBash(const Target& foe, glm::vec3 from, float impact);
  void bullRush(const Target& foe, glm::vec3 from, float impact);
  void executioner(const Target& foe, glm::vec3 from, float impact);
  void trip(const Target& foe, glm::vec3 from, float impact);
  // Over `hexes` (their ground points) from the striker at `from`; `bodies` = the units struck.
  void shoulderSmash(glm::vec3 from, float height, const std::vector<glm::vec3>& hexes, const std::vector<Target>& bodies, float impact);
  // Down the line `hexes` (in order from the striker), the first hex reached at `impact`.
  void stampede(glm::vec3 from, float height, const std::vector<glm::vec3>& hexes, const std::vector<Target>& bodies, float impact);
  void divineWrath(glm::vec3 from, float height, const std::vector<glm::vec3>& hexes, const std::vector<Target>& bodies, float impact);
  // Round the caster out to `radius` (world units); `bodies` = the allies it shields / the foes it cows.
  void aura(bool protection, glm::vec3 feet, float height, float radius, const std::vector<Target>& bodies, float impact);
  // The circle on the hex at `at`; the familiar comes at `impact`.
  void summon(Summon style, glm::vec3 at, float impact);

  bool active(float now) const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Provoke, ShieldBash, BullRush, Executioner, Trip, ShoulderSmash, Stampede, DivineWrath, Aura, Summon };
  struct Effect {
    Kind kind;
    bool protection = false;
    Summon style = Summon::Familiar;
    glm::vec3 feet{0.0f};             // the caster's, or the struck foe's
    glm::vec3 from{0.0f};             // the striker's feet
    glm::vec3 dir{0.0f, 0.0f, 1.0f};  // from the striker toward the struck, on the ground
    float height = 1.8f;
    float radius = 0.0f;
    float start = 0.0f, impact = 0.0f;
    std::vector<glm::vec3> hexes;
    std::vector<Target> targets;
    uint32_t seed = 0;
    float end() const;
  };
  void buildProvoke(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildShieldBash(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildBullRush(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildExecutioner(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildTrip(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildShoulderSmash(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildStampede(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildDivineWrath(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildAura(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildSummon(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  // Shared pieces: a flash with a cross glint, a ring on the ground, sparks, dust rolling out, stones / chips on arcs.
  void flash(glm::vec3 at, glm::vec3 across, float size, glm::vec3 colour, float age, float life, glm::vec3 eye, float pull);
  void groundRing(glm::vec3 feet, float from, float to, float grow, float life, float intensity, float opacity, glm::vec3 colour, glm::vec3 core, float age,
                  glm::vec3 eye, VfxFrame& frame, glm::vec3 sight);
  void sparks(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 hot, glm::vec3 cool, float age, glm::vec3 eye,
              uint32_t salt);
  void dustRing(const Effect& e, glm::vec3 at, glm::vec3 push, int count, float reach, float size, float age, float life, float alpha, glm::vec3 eye, uint32_t salt);
  void debris(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 colour, bool chips, float age, glm::vec3 eye, uint32_t salt);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, rockFirst_ = 0, chipFirst_ = 0, columnFirst_ = 0, runeFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, rockCount_ = 0, chipCount_ = 0, columnCount_ = 0, runeCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, rocks_, chips_;
};

}  // namespace ember
