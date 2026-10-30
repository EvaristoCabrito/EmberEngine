#pragma once

// Nuvem de Adagas (the Mago Negro's, docs/SKILLS_MAGE.md, 2026-10-21), ours, in 3D on the effect runtime (the user: every
// spell effect in 3D, never flipbook cards): real daggers lit by the scene (Standard material: a steel blade with a point, a
// bronze guard and pommel, a dark grip), the Combat material's streaks, rings and motes for the arcane light, a violet light.
//
//   the cast          violet motes gather from the ground to the caster's chest, a flash, a ring on the ground; the daggers
//                     materialize one after another out of the flash and take their orbit;
//   the cloud         3 to 5 daggers circling the body at chest height, points forward along the orbit, each rolling slowly
//                     and bobbing, a violet glint on every point and a short arcane wake behind each blade; a faint violet
//                     ring turning at the feet; a soft violet light on the body. The daggers on the eye's side are drawn over
//                     the card, those on the far half among the cards right before it (VfxDraw::behindCard: the body covers
//                     them, since effects are otherwise drawn over every card), so they go round the figure. Standing for as long as the
//                     rules keep them (MapStage syncDaggers): start() / move() / setCount() / end();
//   a dagger thrown   one dagger leaves its orbit and darts at the attacker on a shallow arc, spinning on its axis, an arcane
//                     wake behind it, and buries itself in the body: a flash with a cross glint, steel sparks, a small violet
//                     ring square to the flight, a light; then it dissolves into motes (strike());
//   an interception   the same dart, but it meets the arrow in front of its caster: a white clash, sparks both ways, the
//                     dagger shatters into spinning steel shards (intercept()).
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.
//
// The dagger itself (the user, 2026-10-29: "Liga a v02"): the Poly Haven ornate_medieval_dagger without its sheath, arcane v02
// (content/models/polyhaven/ornate_medieval_dagger-v02.glb, cooked into build/meshes/effects.pak), its own textures (Textured
// material, drawn solid: VfxDraw::solid), handed over by the stage every frame (setModel). Until it is loaded, or without the
// effects pack, the dagger is the box pieces above (kept as they were).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class DaggerFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = not known: a body's height is assumed)
  };

  // The cast: the arcane light gathering at the caster (feet, card height) and flashing at `impact` (the cloud itself is
  // started by the stage as the rules place the daggers).
  void summon(glm::vec3 feet, float height, float impact);
  // The standing cloud: `count` daggers circling the body from `now`; returns its id. move() follows the unit; setCount()
  // brings the live daggers to `count` (new ones materialize, extra ones dissolve); end() dissolves them all from `now`.
  int start(glm::vec3 feet, float height, int count, float now);
  void move(int id, glm::vec3 feet, float height);
  void setCount(int id, int count, float now);
  void end(int id, float now);
  // One dagger of the cloud standing at `feet` (the nearest cloud; none there: a dagger out of the body itself) leaves its
  // orbit at `start` and reaches `foe` at `impact`: strike() buries it in the body; intercept() meets the shot in front of
  // its caster and knocks it down.
  void strike(glm::vec3 feet, const Target& foe, float start, float impact);
  void intercept(glm::vec3 feet, const Target& foe, float start, float impact);
  // The daggers of cloud `id` standing (not thrown, not dissolving) plus those in flight whose impact is still ahead of `now`.
  int liveCount(int id, float now) const;

  bool active(float now) const;  // a cast's burst or a throw still playing (a standing cloud is not "active")
  float latestEnd() const;
  void clear() { clouds_.clear(), throws_.clear(), bursts_.clear(); }
  // The dagger's model this frame (MeshScene::effect: mesh-space triangles, glTF +Y up, metres; bindless texture indices, which
  // change as they stream); no triangles or no base colour yet = the box pieces.
  void setModel(const std::vector<VfxVertex>* triangles, uint32_t baseColor, uint32_t normal, uint32_t orm, uint32_t emissive) {
    model_ = {triangles, baseColor, normal, orm, emissive};
  }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Slot {
    float born = 0.0f, gone = -1.0f;  // materializes from `born`; dissolves from `gone` (-1 = standing)
    bool thrown = false;              // left its orbit (drawn by a Throw from `gone` on)
    float angle = 0.0f;               // its place on the orbit (radians), eased toward an even spacing
    bool placed = false;
  };
  struct Cloud {
    int id = 0;
    glm::vec3 feet{0.0f};
    float height = 1.8f;
    float start = 0.0f, finish = -1.0f, lastNow = -1.0f;
    uint32_t seed = 0;
    std::vector<Slot> slots;
  };
  struct Throw {
    glm::vec3 from{0.0f}, to{0.0f};  // where it leaves the orbit; the point it reaches (the body's chest / the clash)
    glm::vec3 dir{0.0f, 0.0f, 1.0f};
    float toHeight = 1.8f;
    float start = 0.0f, impact = 0.0f;
    bool intercept = false;
    int cloud = 0;
    uint32_t seed = 0;
    float end() const { return impact + (intercept ? 0.9f : 0.8f); }
  };
  struct Burst {
    glm::vec3 feet{0.0f};
    float height = 1.8f;
    float impact = 0.0f;
    uint32_t seed = 0;
    float end() const { return impact + 0.8f; }
  };

  void launch(glm::vec3 feet, const Target& foe, float start, float impact, bool intercept);
  glm::vec3 orbitPoint(const Cloud& c, const Slot& s, float now) const;
  glm::vec3 orbitTangent(const Cloud& c, const Slot& s, float now) const;
  // The dagger's pieces at `m` (its guard at the origin, the point up +y), `k` its size, `fade` darkening it away; `behind`:
  // on the orbit's far half, into back_ (drawn behind the body's card).
  void dagger(const glm::mat4& m, float k, float fade, bool behind = false);
  // The pieces gathered in these lists as draws (one per piece, then the streaks); behindAt: drawn among the cards, right
  // before the card standing there (VfxDraw::behindCard).
  void emit(VfxFrame& frame, float now, glm::vec3 sight, std::vector<VfxInstance>& streaks, std::vector<VfxInstance>* pieces[5],
            std::vector<VfxInstance>& models, const glm::vec3* behindAt);
  struct Model {
    const std::vector<VfxVertex>* triangles = nullptr;
    uint32_t baseColor = 0xFFFFFFFFu, normal = 0xFFFFFFFFu, orm = 0xFFFFFFFFu, emissive = 0xFFFFFFFFu;
  };
  bool modelReady() const { return model_.triangles && !model_.triangles->empty() && model_.baseColor != 0xFFFFFFFFu; }
  void buildCloud(Cloud& c, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildThrow(const Throw& t, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildBurst(const Burst& b, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Cloud> clouds_;
  std::vector<Throw> throws_;
  std::vector<Burst> bursts_;
  int serial_ = 0;
  uint32_t seedSerial_ = 0;
  uint32_t quadFirst_ = 0, discFirst_ = 0, bladeFirst_ = 0, tipFirst_ = 0, guardFirst_ = 0, gripFirst_ = 0, pommelFirst_ = 0;
  uint32_t quadCount_ = 0, discCount_ = 0, bladeCount_ = 0, tipCount_ = 0, guardCount_ = 0, gripCount_ = 0, pommelCount_ = 0;
  std::vector<VfxInstance> streaks_, blades_, tips_, guards_, grips_, pommels_;
  // A cloud's daggers on the far half of the orbit (the user, 2026-10-22: "only the top of the dagger circle should go behind
  // him"), emitted per cloud behind its body's card.
  std::vector<VfxInstance> backStreaks_, backBlades_, backTips_, backGuards_, backGrips_, backPommels_;
  // The model: one solid draw of all its instances (front half / far half).
  Model model_;
  uint32_t modelFirst_ = 0, modelCount_ = 0;
  std::vector<VfxInstance> models_, backModels_;
};

}  // namespace ember
