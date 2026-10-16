#pragma once

// Spell and strike visuals on the battle board, built from Engine2's effect art (the fx clips: flipbook sheets of
// arcane / lightning / frost / holy / fire / ... in three versions, bolts, an arrow, impact sparks; cooked into the
// battle pack, set "fx", by tools/scripts/fx_sprites.py). Each cast becomes a few timed elements (a charge at the
// caster, a projectile or a strike from the sky, an eruption and a burst at the target), each a flipbook played on
// a quad (render/EffectDraw.h) with a light of its own (the clustered local lights: spells light the board and the
// units around them). The look is ours: Engine2 drew these spells with canvas strokes; only its art and its
// timings (SPELL_TRAVEL, LIGHTNING_*_DUR, holyDuration, FROST_FLASH) are used.
//
// Times are on the stage's clock. The rules decide when a hit lands (the step's impact); a cast is laid out so its
// projectile or strike arrives exactly then. Main thread only.

#include <array>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/EffectDraw.h"
#include "render/SpriteSystem.h"
#include "render/VfxDraw.h"
#include "render/vfx/Bless.h"
#include "render/vfx/CausticVenom.h"
#include "render/vfx/Fireball.h"
#include "render/vfx/MagicMissileV2.h"
#include "render/vfx/PhantasmalForce.h"

namespace ember {

class SpellEffects {
 public:
  explicit SpellEffects(SpriteSystem& sprites) : sprites_(sprites) {}

  // Where an effect meets a unit or a hex: its feet on the ground and the middle of its body (a hex with nobody on
  // it: a body's height above the ground).
  struct Spot {
    glm::vec3 feet{0.0f};
    glm::vec3 body{0.0f};
  };
  // A light an effect gives off this frame, in Engine2's terms (the stage turns it into a LocalLight).
  struct Light {
    glm::vec3 at{0.0f};
    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    float intensity = 0.0f, radius = 3.0f, height = 1.0f;
  };

  // Every spell's saved versions ("looks"), numbered from 1 per spell and never renumbered or removed (the Spells menu
  // shows them as "Míssil Mágico 01", ...; content/fx/README.md lists what each one is). 0 = OFF. Version 01 of every
  // spell that has one is this file's flipbook-card set (2026-10-16).
  static int looks(const std::string& kind);
  // The version that is Engine2's own effect, ported (render/vfx/*), or 0 when the spell has none yet: the heroes'
  // default (user, 2026-10-16).
  static int engine2Look(const std::string& kind);
  // A spell or weapon skill (Engine2 kinds) in version `look`: the caster's pose starts at `start`, its first hit
  // lands at `impact`. `targets` are the hexes / units it reaches in the rules' order (Frost: its whole area). False =
  // nothing drawn (OFF, or no such version).
  bool cast(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets, float start, float impact);
  // One fx clip on its own (the Spells menu's loose effects): played once at the spot, placed by what the art is
  // (a pillar stands on the feet, a ring lies around them, a burst sits on the body), with a light of its colour.
  void play(const std::string& clipName, const Spot& at, float start);
  // A weapon blow that landed (a plain attack, a counter, a blade skill's strike): sparks on the body.
  void strike(const Spot& target, float at, bool crit);
  // Lightning's echo striking again at the start of the struck unit's turn.
  void echo(const Spot& target, float at);

  // This frame's quads (sorted far to near) and lights. `flatTop`: in the 2D view (Tática) every quad lies face up
  // at this height instead of standing.
  // Engine2's ported effects (render/vfx/*) are drawn from vfx() (Renderer::setVfx), filled by the same call.
  void build(float now, const Camera& camera, bool flat, float flatTop, std::vector<EffectInstance>& out, std::vector<Light>& lights);
  const VfxFrame& vfx() const { return vfx_; }
  bool active(float now) const;
  void clear() { elements_.clear(), missiles_.clear(), fireballs_.clear(), blesses_.clear(), venoms_.clear(), phantasms_.clear(); }
  // A version that keeps Engine2's own timeline decides when the hit lands: seconds from the cast (Míssil Mágico 02:
  // Engine2's charge, then the flight). Below 0: the version lands on the caller's impact (the pose's).
  float impactDelay(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets) const;

 private:
  enum class Facing : uint8_t {
    Upright,  // stands on the board turned to the camera (a unit's way): pillars, rings, bursts, bolts
    Along,    // lies in the screen along its flight (an arrow)
  };
  struct Element {
    SpriteClipId clip = kNoSpriteClip;
    std::vector<SpriteClipId> flicker;  // alternatives swapped every kFlickerStep (a bolt's shapes)
    Facing facing = Facing::Upright;
    float start = 0.0f, end = 0.0f;      // alive
    float moveStart = 0.0f, moveEnd = 0.0f;  // travel from -> to (outside: holds the nearer end)
    glm::vec3 from{0.0f}, to{0.0f};
    float arc = 0.0f;                    // height of the flight's arch at its middle
    glm::vec2 pivot{0.5f, 0.0f};         // the point of the art (u, v up, of the whole cell) placed on the position
    float height = 1.0f, endHeight = -1.0f;  // world size of the whole cell (endHeight < 0: same)
    float fps = 0.0f;                    // > 0: loops at this rate; 0: plays its frames once over its life
    float fadeIn = 0.0f, fadeOut = 0.0f;
    glm::vec3 color{1.0f};
    float opacity = 0.0f;                // 0 additive .. 1 covers
    float pull = 0.0f;                   // drawn this much nearer the camera (in depth only, same size on screen)
    float softness = 0.0f;
    float angle = 0.0f;                  // Along: the art's own axis, radians from +u (the arrow art points up-right)
    bool mirror = false, lit = false;
    // Its light (intensity 0 = none), following it and fading with it.
    std::array<float, 3> lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 0.0f, lightRadius = 3.0f, lightDecay = 0.0f;  // decay: intensity x (1 - age)^decay
  };

  SpriteClipId clip(const char* name) const { return sprites_.find("fx", name); }
  Element& add(const char* clipName, float start, float duration);
  glm::vec3 position(const Element& e, float now) const;
  void burst(const char* clipName, glm::vec3 at, float start, float duration, float height, glm::vec3 color, float pull = 1.2f);

  void magicMissile(const Spot& caster, const Spot& target, float start, float impact);
  void lightning(const Spot& caster, const std::vector<Spot>& targets, float start, float impact, bool shock);
  void bolt(const Spot& target, float impact, float scale);
  void frost(const Spot& caster, const std::vector<Spot>& targets, float start, float impact);
  void cure(const Spot& caster, const std::vector<Spot>& targets, float start, float impact);
  void arrow(const Spot& caster, const std::vector<Spot>& targets, float start, float impact, bool piercing);
  void bladeSkill(const Spot& caster, float start, float impact);

  SpriteSystem& sprites_;
  std::vector<Element> elements_;
  // Engine2's ported effects in play (Míssil Mágico 02 = MagicMissileV2VFX) and this frame's draws.
  std::vector<MagicMissileV2> missiles_;
  std::vector<Fireball> fireballs_;  // Bola de Fogo 01 = FireballVFX
  std::vector<Bless> blesses_;       // Bênção 01 = BlessVFX
  std::vector<CausticVenom> venoms_; // Veneno Cáustico 01 = CausticVenomVFX
  std::vector<PhantasmalForce> phantasms_;  // Força Fantasmagórica 01 = PhantasmalForceVFX
  // An fx clip of the battle pack as the effect runtime samples it (requested on screen; empty while not loaded).
  VfxFlipbook flipbook(const char* clipName);
  ElementSpritesV2 elementV2(VfxElement element);
  // Bless's allies as BlessVFX takes them: anchor + 0.9, hex distance from the caster.
  static std::vector<Bless::Ally> blessAllies(const Spot& caster, const std::vector<Spot>& targets);
  // Fireball's flight goes to the target hex's ground (BattleSpells3D at(): 0.04 up); its area's radius (the farthest
  // struck cell + 1; one target alone = Engine2's FIREBALL.size, 2 hexes around it).
  static glm::vec3 fireballTarget(const std::vector<Spot>& targets) { return targets.front().feet + glm::vec3(0.0f, 0.04f, 0.0f); }
  static float fireballArea(const std::vector<Spot>& targets);
  VfxFrame vfx_;
  // Engine2's caster point (battleSpells3d unit(): the anchor, 0.9 above the ground).
  static glm::vec3 engine2Origin(const Spot& caster) { return caster.feet + glm::vec3(0.0f, 0.9f, 0.0f); }
  uint32_t seed_ = 0x9e3779b9u;
  float random();  // 0..1, the visuals' own sequence (never the rules')
};

}  // namespace ember
