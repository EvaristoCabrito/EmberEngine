#pragma once

// Kael's techniques (Vanguarda, Mestre da Espada, Arauto da Guerra; docs/SKILLS_VANGUARD.md), ours, in 3D on the effect runtime (the
// user, 2026-11-13: "never plain blade for special, not a single character", then "create the visual effects for these new skills").
// One look each, version 01, on the Combat material (shaders/vfx.hlsl kMaterialCombat): blade-light sheets drawn along the cut's own curve,
// sparks, flashes, ground rings, energy shells, smoke and dust, turning sigils, thrown grit (lit by the scene). Every look carries the
// violet of his special clip's lit sword ("the sword lit purple, the sweep with its dark smoke and the ring of smoke on the floor"); each
// technique has its own shape, after the design's "Animation and sound" column:
//
//   Vanguarda       Corte Medido (a compact diagonal cut), Passo Longo (a level cut, a boot scuff), Guarda Cruzada (two blades of light
//                   crossed before his chest), Golpe de Pomo (a dull knock: a grey shell, grit), Corte Baixo (a low sweep skimming the
//                   ground), Corte de Saída (a rising cut, dust at his heel), Talho por Cima (a great overhead cut, a crack in the ground),
//                   Pé Firme (his weight dropped: a ring of dust, a stone-grey ring at his feet), Desafio (the point of the blade: a line of
//                   light to the foe, a ring on it), Romper a Guarda (the guard wrenched aside, then the chop; a broken violet ring on it);
//   Mestre da Espada Finta (a cut begun and pulled away, half drawn; the Exposto sigil), Riposta (a ring of sparks at the parry, the blade
//                   snapping back down the line), Passo de Passagem (a cut on the turn, curved round the foe), Ponta de Agulha (a needle of
//                   light through the body, a narrow spray out the back), Segunda Intenção (the high cut, then the low return), Ler a
//                   Lâmina (the point held still: a thin blade of light, a turning ring at the guard, a sight line), Quebrar o Ritmo (two
//                   clacks on its weapon, then the cut across; a pulse that breaks), Corte Perfeito (one thin cut, very bright, its glint
//                   splitting after), Medida Mortal (the point tracking it: a line, a ring under its feet; the lunge when it breaks away),
//                   Golpe de Mestre (the whole weight in one diagonal: the widest sheet, a shock ring, a cut left hanging in the air);
//   Arauto da Guerra Corte Impulsionado (a heavy level cut, the stamp, the push's dust), Talho Largo (a broad sweep across the front arc),
//                   Desvio Lateral (a low-to-high cut, the slide's dust to where it goes), Firmar a Linha (the point dropped: the front arc
//                   drawn on the ground in light; the short cut when it comes), Ombrada (the shoulder's shock, then the cut), Esmagar na
//                   Pedra (the drive; against the stone, grit and a crack), Rachar a Fileira (a long through-cut down the line), Ceifar os
//                   Flancos (two cuts sweeping out from the centre), Avanço Rolante (the heavy cut and the stamp, then the second cut),
//                   Estilhaçar a Linha (the loaded sweep: a great central shock, lighter flashes on the flanks).
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class VanguardFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = a bare hex)
  };

  // A technique's look: Kael at `caster`; `targets` = the struck / marked body first, then the hexes it reaches (the arc's, the line's
  // second hex, the hex a foe is moved to); the blow lands at `impact` (`start`: the pose begins). False for a kind that is not his.
  bool technique(const std::string& kind, const Target& caster, const std::vector<Target>& targets, float start, float impact);
  // The Riposta: the parry's sparks on Kael, then his blade snapping back at the attacker, landing at `impact`.
  void riposte(const Target& kael, const Target& attacker, float impact);
  // A movement reaction's blow (Medida Mortal's lunge, Firmar a Linha's short cut) on the walker, landing at `impact`.
  void reaction(const std::string& kind, const Target& kael, const Target& walker, float impact);
  // Esmagar na Pedra's push stopped by the stone: grit and a crack behind the foe, `dir` the push's way, at `at`.
  void stone(const Target& foe, glm::vec3 dir, float at);

  static bool handles(const std::string& kind);
  bool active(float now) const;
  float latestEnd() const;
  void clear() { pieces_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Shape { Sheet, Flash, Sparks, Ring, Shell, Smoke, Line, Sigil, Debris, Light, Cross };
  // One timed element of a look. Its meaning by shape (the rest unused):
  //   Sheet   a cut of blade light along the curve a -> c (control b), `width` thick toward `thick`, drawn in over `sweep` s;
  //   Flash   a burst of light at a, `size` wide; Sparks: `count` sparks thrown from a toward `thick`, `speed`;
  //   Ring    a ring on the ground at a, radius `size` -> `size2`; Shell: a sphere at a, `size` -> `size2` (dark = smoke-coloured);
  //   Smoke   `count` puffs rolling out from a to `size2` (colour: dust or dark smoke); Line: a beam of light a -> c, `width` wide;
  //   Sigil   a turning broken ring at a, `size` wide; Debris: `count` pieces thrown from a toward `thick` (rock or steel);
  //   Light   a light at a, `size` reach; Cross: two crossed bars of light at a, `size` long, facing `thick`.
  struct Piece {
    Shape shape = Shape::Flash;
    glm::vec3 a{0.0f}, b{0.0f}, c{0.0f}, thick{0.0f, 1.0f, 0.0f};
    glm::vec3 color{1.0f}, core{1.0f};
    float start = 0.0f, life = 0.5f;  // seconds from `start`
    float sweep = 0.12f, width = 0.2f, size = 0.5f, size2 = 1.0f, intensity = 1.5f, alpha = 1.0f, speed = 3.0f;
    float ground = 0.0f;  // the ground's height where it lands (sparks, debris)
    int count = 0;
    bool dark = false, steel = false;
    uint32_t seed = 0;
    float end() const { return start + life; }
  };
  void add(Piece p);
  // Shorthands for the looks.
  void cut(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 thick, float width, float start, float sweep, float intensity = 1.6f, bool heavy = false);
  void flash(glm::vec3 at, float size, float when, float life = 0.14f, glm::vec3 color = glm::vec3(0.0f));
  void sparks(glm::vec3 at, glm::vec3 dir, int count, float speed, float when, float ground);
  void ring(glm::vec3 at, float from, float to, float when, float life, glm::vec3 color, float alpha = 0.7f);
  void shell(glm::vec3 at, float from, float to, float when, float life, glm::vec3 color, bool dark = false, float alpha = 0.5f);
  void smoke(glm::vec3 at, float out, int count, float when, float life, bool dark, float alpha = 0.45f);
  void line(glm::vec3 a, glm::vec3 c, float width, float when, float life, glm::vec3 color, float intensity = 1.8f);
  void sigil(glm::vec3 at, float size, float when, float life, glm::vec3 color);
  void debris(glm::vec3 at, glm::vec3 dir, int count, float speed, float when, float ground, bool steel);
  void light(glm::vec3 at, float reach, float when, float life, glm::vec3 color, float intensity);
  void crossed(glm::vec3 at, glm::vec3 facing, float size, float when, float life);
  // A blow's contact on a body: the flash, the sparks the blade's way, the light (`scale` 1 = an ordinary technique hit).
  void contact(const Target& foe, glm::vec3 dir, float when, float scale = 1.0f);

  void buildPiece(const Piece& p, float now, glm::vec3 eye, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Piece> pieces_;
  uint32_t serial_ = 0;
  // The arc / line techniques strike each foe as its own step: the sweep is drawn on the first, the later ones only add their contact.
  struct Recent {
    std::string kind;
    glm::vec3 caster{0.0f};
    float at = 0.0f;
    int step = 0;
  };
  std::vector<Recent> recent_;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, rockFirst_ = 0, chipFirst_ = 0, arcFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, rockCount_ = 0, chipCount_ = 0, arcCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, rocks_, chips_;
};

}  // namespace ember
