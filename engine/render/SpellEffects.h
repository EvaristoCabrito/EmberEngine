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
#include "render/vfx/BurningHands.h"
#include "render/vfx/CausticVenom.h"
#include "render/vfx/AcidCloud.h"
#include "render/vfx/CombatFx.h"
#include "render/vfx/Fireball.h"
#include "render/vfx/HolyLight.h"
#include "render/vfx/FrostLine.h"
#include "render/vfx/IceStorm.h"
#include "render/vfx/Lightning.h"
#include "render/vfx/SpellBolts.h"
#include "render/vfx/MagicMissileV2.h"
#include "render/vfx/PhantasmalForce.h"
#include "render/vfx/Sweeps.h"
#include "render/vfx/TurnUndead.h"
#include "render/vfx/ConjurerFx.h"
#include "render/vfx/HealerFx.h"
#include "render/vfx/CrusaderFx.h"
#include "render/vfx/LancerFx.h"
#include "render/vfx/StatusFx.h"
#include "render/vfx/DaggerFx.h"
#include "render/vfx/MageFx.h"
#include "render/vfx/RangerFx.h"
#include "render/vfx/WarriorFx.h"
#include "render/vfx/ArmsFx.h"
#include "render/vfx/DreamWeb.h"
#include "render/vfx/WebOfDreams.h"

namespace ember {

class SpellEffects {
 public:
  explicit SpellEffects(SpriteSystem& sprites) : sprites_(sprites) {}

  // Where an effect meets a unit or a hex: its feet on the ground and the middle of its body (a hex with nobody on
  // it: a body's height above the ground).
  struct Spot {
    glm::vec3 feet{0.0f};
    glm::vec3 body{0.0f};
    float cardHeight = 0.0f;  // the unit's card as drawn (0 = a hex, or not known)
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
  // The heroes' default version (user, 2026-10-16): Engine2's own effect, ported (render/vfx/*); for the cures, the meal
  // and the potion, whose Engine2 art was 2D only, our 3D light (HolyLight); 0 = none (the first version).
  static int engine2Look(const std::string& kind);
  // A spell or weapon skill (Engine2 kinds) in version `look`: the caster's pose starts at `start`, its first hit
  // lands at `impact`. `targets` are the hexes / units it reaches in the rules' order (Frost: its whole area). False =
  // nothing drawn (OFF, or no such version).
  // `owner`: who casts it (the stage's unit index, -1 = nobody): castEnd(owner) says when what it cast is over (the
  // stage holds a caster turned away from the camera until then).
  bool cast(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets, float start, float impact,
            int owner = -1);
  float castEnd(int owner) const;  // the latest end of `owner`'s casts (-1: none)
  bool castEffect(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets, float start, float impact);
  // One fx clip on its own (the Spells menu's loose effects): played once at the spot, placed by what the art is
  // (a pillar stands on the feet, a ring lies around them, a burst sits on the body), with a light of its colour.
  void play(const std::string& clipName, const Spot& at, float start);
  // A potion handed over or drunk (kind "potion", version `look`): its colour by what it does (Engine2 POTIONS effect:
  // "heal", "disease", "mana").
  bool potion(const std::string& effect, int look, const Spot& target, float start, float impact, int owner = -1);
  // A weapon blow that landed (a plain attack, a counter, a blade skill's strike): sparks on the body. The flipbook
  // version (random "impact-N" art); no longer played since 2026-10-17: the game uses combat() (CombatFx).
  void strike(const Spot& target, float at, bool crit);
  // The combat's own 3D effects (2026-10-17): every blow's impact and the mage's Arcano attack.
  CombatFx& combat() { return combat_; }
  // Lightning's echo striking again at the start of the struck unit's turn.
  void echo(const Spot& target, float at);
  // The Guerreiro's skills (vfx/WarriorFx, ours, 3D): cast() plays the blow of each; these are the parts the stage times
  // itself: the Salto Devastador's flight from the caster to the landing hex over [start, impact] (its crater at impact),
  // and the Grito de Guerra's dark pulse on the foes it frightens.
  void leapFlight(const Spot& caster, const Spot& landing, float start, float impact, int owner = -1);
  void warCryFear(const std::vector<Spot>& foes, float impact);
  // The Lanceiro's skills (vfx/LancerFx, ours, 3D): cast() plays the lance's blow of each; the stage times the board moves
  // itself once the rules decided them. lancer() for the standing vigil (vigilStart / vigilMove / vigilEnd) and the tests.
  LancerFx& lancer() { return lancer_; }
  // The vault from `caster` over `over` onto `landing` over [start, land] (the card flies in the stage).
  void vaultFlight(const Spot& caster, const Spot& over, const Spot& landing, float start, float land, int owner = -1);
  // A body's flight after a landed hit of `kind` ("spearToss", "hookPull", "impaleDrag"): from `from` to `to` over [start, finish]
  // on an arc `arc` high; `caster`: where the lancer stands (the hook's chain), or for the drag where it runs from and `casterTo`
  // where it ends; `collided`: the thrown body lands against another.
  void lancerFlight(const std::string& kind, const Spot& from, const Spot& to, float arc, float start, float finish, const Spot& caster, const Spot& casterTo,
                    bool collided);
  // Troca de Posto: `a` and `b` change places over [start, finish].
  void swapFlight(const Spot& a, const Spot& b, float start, float finish);
  // Desarmar landed: the foe's weapon knocked away at `at`, from the striker at `from`.
  void disarmed(const Spot& foe, const Spot& from, float at);

  // --- EmberEngine's own spells (SpellEffectsEmber.cpp: the Bruxo's and the Elementalista's, version 01 = flipbook cards) ---
  // A zone that stays on the board (burning ground, the Mortalha de Sombras, a Meteoro's mark): `kind` as the rules name
  // it ("burning" with its element, "shroudOfShadows", "meteor"), one spot per hex (the centre first). Returns its id;
  // zoneEnd fades it out from `now`.
  int zoneStart(const std::string& kind, const std::string& element, const std::vector<Spot>& hexes, float now);
  void zoneEnd(int id, float now);
  // Meteoro falls: the rock from the sky onto the centre (hexes.front()), the blast over every hex; it lands at `impact`.
  void meteorFall(const std::vector<Spot>& hexes, float start, float impact);

  // Tempestade de Gelo (vfx/IceStorm): cast("iceStorm", ...) with every hex of the field as the targets starts a storm at
  // the impact; newestStorm() is its id (0 = none yet), stormEnd fades it when the rules drop the field.
  int newestStorm() const { return stormSerial_; }
  void stormEnd(int id, float now);
  void stormStand(int id);  // held until stormEnd (the rules keep the field); without it the storm plays its 9 s (--fx-test)

  // Teia dos Sonhos 02 (vfx/DreamWeb): a web the rules keep for rounds. Before cast("webOfDreams", 2, ...) call webUnits() with the
  // units standing in the zone; after it, newestWeb() is its id (0 = none yet) and webStand(id) holds it until webEnd(id, now) (the
  // rules dropped the zone); webUnits(id, ...) keeps its occupants current. Without webStand it holds 2.6 s and lets go (--fx-test).
  struct WebUnit {
    int key = 0;
    Spot at;
  };
  void webUnits(const std::vector<WebUnit>& units) { webPending_ = units; }
  void webUnits(int id, const std::vector<WebUnit>& units);
  int newestWeb() const { return webSerial_; }
  void webStand(int id);
  void webEnd(int id, float now);

  // This frame's quads (sorted far to near) and lights. `flatTop`: in the 2D view (Tática) every quad lies face up
  // at this height instead of standing.
  // Engine2's ported effects (render/vfx/*) are drawn from vfx() (Renderer::setVfx), filled by the same call.
  void build(float now, const Camera& camera, bool flat, float flatTop, std::vector<EffectInstance>& out, std::vector<Light>& lights);
  const VfxFrame& vfx() const { return vfx_; }
  bool active(float now) const;
  void clear() { combat_.clear(), elements_.clear(), missiles_.clear(), fireballs_.clear(), blesses_.clear(), venoms_.clear(), phantasms_.clear(), webs_.clear(), dreamWebs_.clear(), burns_.clear(), cleaves_.clear(), sweeps_.clear(), holies_.clear(), storms_.clear(), bolts_.clear(), lightnings_.clear(), frosts_.clear(), warrior_.clear(), arms_.clear(), ranger_.clear(), lancer_.clear(), turnUndead_.clear(), healer_.clear(), conjurer_.clear(), status_.clear(), daggers_.clear(), mage_.clear(), crusader_.clear(); }
  // The Mago Negro's Nuvem de Adagas standing round its caster (vfx/DaggerFx, ours, 3D): the stage keeps one cloud per unit (MapStage
  // syncDaggers); cast() plays the conjuring, the thrown dagger ("daggerStrike") and, through intercept(), the shot knocked down.
  DaggerFx& daggers() { return daggers_; }
  // ... and his Dissipar Magia / Globo de Invulnerabilidade (vfx/MageFx): the stage keeps the standing globes (MapStage syncGlobes) and
  // flashes a blocked spell on the bubble (globeRipple); cast() plays the dispel's wave and the bubble's rise.
  MageFx& mage() { return mage_; }
  // The Conjurador's standing illusions (Imagem Espelhada) and the swap of Troca de Lugar: the stage times them (MapStage syncMirrors / animateFlights).
  ConjurerFx& conjurer() { return conjurer_; }
  ArmsFx& arms() { return arms_; }  // Reflexo Arcano's bounce, timed by the stage
  // Poison and disease standing on a unit (vfx/StatusFx, ours, 3D): the stage keeps one per condition per unit (MapStage syncStatus).
  StatusFx& status() { return status_; }
  // The Cruzado's (vfx/CrusaderFx, ours, 3D): cast() plays the casts; the stage times the holy flash and the mantle's sparks on the blows
  // that land and keeps the standing Juramento / Manto (MapStage syncCrusader).
  CrusaderFx& crusader() { return crusader_; }
  // A version that keeps Engine2's own timeline decides when the hit lands: seconds from the cast (Míssil Mágico 02:
  // Engine2's charge, then the flight). Below 0: the version lands on the caller's impact (the pose's).
  float impactDelay(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets) const;
  // A spell cast after its caster's whole pose (the user, 2026-10-17: the effect only appears once the cast animation is
  // over): seconds from the effect's start to its first hit. impactDelay's timeline when the version has one; the cures'
  // light gathers for HolyLight::leadIn; every other version kKindLead (Engine2's SPELL_TRAVEL at most).
  float leadIn(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets) const;
  static constexpr float kKindLead = 0.6f;

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
    int zone = 0;                         // part of a standing zone (zoneStart): lives until zoneEnd
    float flickerLight = 0.0f;            // > 0: the light wavers by this share (flames)
  };

  // The cures', the meal's and the potion's light (HolyLight) plays this version.
  static bool isHoly(const std::string& kind, int look);
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
  // Ours (SpellEffectsEmber.cpp). True when `kind` is one of them.
  bool emberCast(const std::string& kind, const Spot& caster, const std::vector<Spot>& targets, float start, float impact);
  void markOfRuin(const Spot& caster, const Spot& target, float start, float impact);
  void creepingPlague(const Spot& caster, const Spot& target, float start, float impact);
  void lifeDrain(const Spot& caster, const Spot& target, float start, float impact);
  void shroudCast(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void hemorrhage(const Spot& caster, const Spot& target, float start, float impact);
  void profaneSilence(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void blackPestilence(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void curseOfDecay(const Spot& caster, const Spot& target, float start, float impact);
  void soulHarvest(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void devouringVoid(const Spot& caster, const Spot& target, float start, float impact);
  void pillarOfFlame(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void iceLance(const Spot& caster, const std::vector<Spot>& line, float start, float impact);
  void thunderstorm(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void meteorMark(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void absoluteZero(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void chainLightning(const Spot& caster, const std::vector<Spot>& chain, float start, float impact);
  void eruption(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  void glacialGale(const Spot& caster, const std::vector<Spot>& cone, float start, float impact);
  void wrathOfTheSkies(const Spot& caster, const std::vector<Spot>& targets, float start, float impact);
  void primordialEmber(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact);
  // A streak of sparks flowing from one point to another (drains, harvested souls), `count` of them over the flight.
  void stream(const char* clipName, glm::vec3 from, glm::vec3 to, float start, float duration, int count, glm::vec3 color, float size);
  int zoneSerial_ = 0;

  SpriteSystem& sprites_;
  std::vector<Element> elements_;
  struct Cast {
    int owner;
    float end;
  };
  std::vector<Cast> casts_;  // who is casting, until when
  // Engine2's ported effects in play (Míssil Mágico 02 = MagicMissileV2VFX) and this frame's draws.
  std::vector<MagicMissileV2> missiles_;
  std::vector<Fireball> fireballs_;  // Bola de Fogo 01 = FireballVFX
  std::vector<Bless> blesses_;       // Bênção 01 = BlessVFX
  std::vector<CausticVenom> venoms_; // Veneno Cáustico 01 = CausticVenomVFX
  std::vector<AcidCloud> acidClouds_; // Névoa Ácida: low, persistent 3D field
  std::vector<PhantasmalForce> phantasms_;  // Força Fantasmagórica 01 = PhantasmalForceVFX
  std::vector<WebOfDreams> webs_;           // Teia dos Sonhos 01 = WebOfDreamsVFX
  std::vector<DreamWeb> dreamWebs_;         // Teia dos Sonhos 02 (ours, 3D): the shot, the floor, the net
  std::vector<WebUnit> webPending_;         // the units in the zone about to be cast
  int webSerial_ = 0;
  std::vector<CleaveSweep> cleaves_;  // Cleave 01 = CleaveSweepVFX
  std::vector<Varredura> sweeps_;     // Varredura 01 = VarreduraVFX
  std::vector<HolyLight> holies_;     // the cures', the meal's and the potions' light (ours, 3D)
  std::vector<IceStorm> storms_;      // Tempestade de Gelo 01 (Engine2's Blizzard2D) / 02 (ours, 3D)
  std::vector<SpellBolt> bolts_;      // Míssil Mágico 03, Raio Divino 01 (ours, 3D)
  std::vector<Lightning> lightnings_; // Relâmpago / Relâmpago Maior / Choque 02 (ours, 3D)
  std::vector<FrostLine> frosts_;     // Frost 02 (ours, 3D)
  // Frost's 3D version (02).
  static bool isFrost(const std::string& kind, int look) { return look == 2 && kind == "frost"; }
  // The lightning spells' 3D version (02).
  // Relâmpago / Choque 02, and 03 = the same at a small size (the enemies', the user 2026-10-20: "tiny ... thunder").
  static bool isLightning(const std::string& kind, int look) {
    return (look == 2 || look == 3) && (kind == "lightning" || kind == "lightningTier3" || kind == "shock");
  }
  // The bolt of Míssil Mágico 03 (04 = the same at a small size, the enemies') / Raio Divino 01, or none.
  static bool isBolt(const std::string& kind, int look) {
    return ((kind == "magicMissile" || kind == "magicMissileV2") && (look == 3 || look == 4)) || (kind == "divineBolt" && look == 1);
  }
  // The size of the small versions (Míssil Mágico 04, Relâmpago / Choque 03).
  static constexpr float kTinyFx = 0.55f;
  // Raio Divino's struck hex: the one nearest the middle of its area (the rules list the area row by row).
  static const Spot& areaCentre(const std::vector<Spot>& hexes);
  static glm::vec3 aimedHex(const std::vector<Spot>& targets);  // the area's aimed hex, ground + 0.04 (Veneno Cáustico's flight ends there)
  static float heightOf(const Spot& s) { return s.cardHeight > 0.0f ? s.cardHeight : std::max(1.2f, (s.body.y - s.feet.y) / 0.45f); }
  int stormSerial_ = 0;
  CombatFx combat_;
  WarriorFx warrior_;  // the Guerreiro's skills (ours, 3D)
  ArmsFx arms_;        // the weapon skills, auras and summons that had no effect (ours, 3D, 2026-10-20)
  RangerFx ranger_;    // the Arqueiro's skills (ours, 3D)
  LancerFx lancer_;    // the Lanceiro's skills (ours, 3D)
  TurnUndead turnUndead_;  // Expulsar Mortos-Vivos 01 (ours, 3D)
  HealerFx healer_;        // the Curandeiro's Círculo de Cura / Ressurreição / Julgamento (ours, 3D)
  static bool isHealer(const std::string& kind) { return kind == "circleOfHealing" || kind == "resurrection" || kind == "judgment"; }
  CrusaderFx crusader_;    // the Cruzado's Golpe Consagrado / Toque Sagrado / Juramento / Manto do Cruzado (ours, 3D)
  static bool isCrusader(const std::string& kind) { return kind == "holyStrike" || kind == "holyTouch" || kind == "crusaderMantle"; }
  ConjurerFx conjurer_;    // the Conjurador's seven (ours, 3D)
  StatusFx status_;        // poison / disease on the units (ours, 3D)
  DaggerFx daggers_;       // the Mago Negro's Nuvem de Adagas (ours, 3D)
  MageFx mage_;            // ... and his Dissipar Magia / Globo de Invulnerabilidade (ours, 3D)
  static bool isDagger(const std::string& kind) { return kind == "cloudOfDaggers" || kind == "daggerStrike"; }
  static bool isMage(const std::string& kind) { return kind == "dispelMagic" || kind == "globeOfInvulnerability"; }
  static bool isConjurer(const std::string& kind) {
    return kind == "placeSwap" || kind == "boneLance" || kind == "mirrorImage" || kind == "detonateSummon" || kind == "sacrificeBond" || kind == "nightmare" ||
           kind == "empowerSummons";
  }
  std::string potionEffect_ = "heal";  // the potion being cast (potion())
  std::vector<std::pair<BurningHands, bool>> burns_;  // Mãos Flamejantes 01 / Sopro Venenoso 01 (poison) = BurningHandsV2VFX
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
  // build()'s tally since the last report (logged once a second while something is not drawn)
  int drawn_ = 0, notResident_ = 0;
  float lastReport_ = -10.0f;
};

}  // namespace ember
