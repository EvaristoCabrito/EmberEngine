#include "render/SpellEffects.h"

#include <algorithm>
#include <cmath>

#include "core/Log.h"

namespace ember {

namespace {

constexpr float kFlickerStep = 0.05f;  // a bolt changes shape this often (seconds)

// Engine2's colours for these lights (battleSpellOverlay3d / emitters): arcane violet, lightning blue-white, frost
// (0xb6e7ff), holy gold, a struck blow's warm spark.
constexpr std::array<float, 3> kViolet{0.62f, 0.32f, 1.0f};
constexpr std::array<float, 3> kBolt{0.72f, 0.86f, 1.0f};
constexpr std::array<float, 3> kFrost{0.71f, 0.91f, 1.0f};
constexpr std::array<float, 3> kHoly{1.0f, 0.86f, 0.58f};
constexpr std::array<float, 3> kSpark{1.0f, 0.78f, 0.52f};

float smooth(float x) { return x * x * (3.0f - 2.0f * x); }

}  // namespace

float SpellEffects::random() {
  seed_ ^= seed_ << 13;
  seed_ ^= seed_ >> 17;
  seed_ ^= seed_ << 5;
  return static_cast<float>(seed_ & 0xFFFFFF) / static_cast<float>(0x1000000);
}

SpellEffects::Element& SpellEffects::add(const char* clipName, float start, float duration) {
  Element& e = elements_.emplace_back();
  e.clip = clipName ? clip(clipName) : kNoSpriteClip;
  e.start = start;
  e.end = start + std::max(duration, 0.01f);
  e.moveStart = e.moveEnd = start;
  return e;
}

// A burst standing at a point: its middle on the point, drawn a little nearer than the body it lands on.
void SpellEffects::burst(const char* clipName, glm::vec3 at, float start, float duration, float height, glm::vec3 color, float pull) {
  Element& e = add(clipName, start, duration);
  e.from = e.to = at;
  e.pivot = {0.5f, 0.5f};
  e.height = height;
  e.color = color;
  e.fadeOut = duration * 0.35f;
  e.pull = pull;
  e.softness = 0.25f;
  e.mirror = random() < 0.5f;
}

glm::vec3 SpellEffects::position(const Element& e, float now) const {
  if (e.moveEnd <= e.moveStart) return e.to;
  const float t = std::clamp((now - e.moveStart) / (e.moveEnd - e.moveStart), 0.0f, 1.0f);
  return glm::mix(e.from, e.to, t) + glm::vec3(0.0f, e.arc * 4.0f * t * (1.0f - t), 0.0f);
}

int SpellEffects::looks(const std::string& kind) {
  // Our 3D light (HolyLight, 2026-10-17): Cura Menor 02 after its flipbook cards; the first version of the other cures,
  // the meal and the potion.
  if (kind == "cureMinor") return 2;
  if (kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "createFoodAndWater" || kind == "potion") return 1;
  // 02: Engine2's own effect, ported (render/vfx/*).
  // 03: ours in 3D (vfx/SpellBolts; the user, 2026-10-18: "the current ones are too bland"). 04: 03 small (the enemies').
  if (kind == "magicMissile" || kind == "magicMissileV2") return 4;
  if (kind == "divineBolt") return 1;  // 01: ours in 3D (vfx/SpellBolts)
  if (kind == "turnUndead") return 1;  // 01: ours in 3D (vfx/TurnUndead)
  if (isLightning(kind, 2)) return 3;  // 01 the flipbook cards, 02 ours in 3D (vfx/Lightning), 03 02 small (the enemies')
  // 01 Engine2's BurningHandsV2VFX as it was, 02 its cone opening outward (ours).
  if (kind == "burningHands" || kind == "poisonBreath") return 2;
  // Tempestade de Gelo: 01 Engine2's Blizzard2D (the enemies'), 02 ours in 3D (the heroes'): vfx/IceStorm.
  if (kind == "iceStorm") return 2;
  if (kind == "acidCloud") return 1;
  if (kind == "frost") return 2;  // 01 the flipbook cards, 02 ours in 3D (vfx/FrostLine)
  // Teia dos Sonhos: 01 Engine2's WebOfDreamsVFX as ported (the net alone), 02 ours in 3D with the whole spell (vfx/DreamWeb).
  if (kind == "webOfDreams") return 2;
  // 01 of a spell with no flipbook-card version: Engine2's own effect, ported.
  if (kind == "causticVenom") return 2;  // 01 Engine2's CausticVenomVFX, 02 its impact in 3D (vfx/CausticVenom: smoke, light)
  if (kind == "fireball" || kind == "bless" || kind == "phantasmalForce" ||
      kind == "webOfDreams" || kind == "burningHands" || kind == "poisonBreath" || kind == "cleave" || kind == "sweep")
    return 1;
  if (kind == "longShot" || kind == "piercing") return 2;  // 01 the arrow card, 02 RangerFx's 3D arrow (the enemies')
  if (kind == "piercingThrust") return 2;  // 01 the flipbook burst, 02 ours in 3D (vfx/LancerFx: the thrust down the line)
  static const char* kVersion01[] = {"lightning", "lightningTier3", "shock", "cureMinor", "doubleStrike"};
  for (const char* k : kVersion01)
    if (kind == k) return 1;
  // EmberEngine's own spells (SpellEffectsEmber.cpp): version 01, flipbook cards.
  static const char* kEmber[] = {"markOfRuin",   "creepingPlague", "lifeDrain",    "shroudOfShadows", "hemorrhage",     "profaneSilence",  "blackPestilence",
                                 "curseOfDecay", "soulHarvest",    "devouringVoid", "pillarOfFlame",  "iceLance",       "thunderstorm",    "meteor",
                                 "absoluteZero", "chainLightning", "eruption",      "glacialGale",     "wrathOfTheSkies", "primordialEmber",
                                 // the Guerreiro's (vfx/WarriorFx, ours, 3D)
                                 "steelWhirlwind", "guardBreak", "warCry", "leapingStrike",
                                 // the Arqueiro's (vfx/RangerFx, ours, 3D)
                                 "venomArrow", "arrowRain", "explosiveArrow", "cripplingShot", "arrowStorm", "deathShot",
                                 // the Corrosive Slimes' Milf's Acid Arrow: Neera's own arrow (RangerFx Venom), the user 2026-10-22
                                 "acidArrow",
                                 // the Lanceiro's (vfx/LancerFx, ours, 3D)
                                 "spearToss", "hookPull", "poleVault", "disarm", "swapPost", "impaleDrag", "sentinelVigil", "sentinelStrike",
                                 // the Curandeiro's (vfx/HealerFx, ours, 3D)
                                 "circleOfHealing", "resurrection", "judgment",
                                 // the Cruzado's (vfx/CrusaderFx, ours, 3D)
                                 "holyStrike", "holyTouch", "crusaderMantle",
                                 // Selene's (vfx/SeleneFx, ours, 3D)
                                 "arcaneBlade", "arcaneMarkBurst",
                                 // the Conjurador's (vfx/ConjurerFx, ours, 3D)
                                 "placeSwap", "boneLance", "mirrorImage", "detonateSummon", "sacrificeBond", "nightmare", "empowerSummons",
                                 // the ones that had none (vfx/ArmsFx, the two new arrows of vfx/RangerFx; ours, 3D, 2026-10-20)
                                 "provoke", "shieldBash", "bullRush", "executionerStrike", "trip", "shoulderSmash", "stampede", "divineWrath",
                                 "auraOfProtection", "intimidatingPresence", "bloodyShot", "multiShot", "summonFamiliar", "summonFamiliar2",
                                 "summonFamiliar3", "summonFamiliar4", "summonZombieDog", "summonSwampDeer", "arcaneReflection", "fantomForce", "spectralBolt",
                                 // the Mago Negro's (vfx/DaggerFx, ours, 3D)
                                 "cloudOfDaggers", "daggerStrike", "dispelMagic", "globeOfInvulnerability"};
  for (const char* k : kEmber)
    if (kind == k) return 1;
  return 0;
}

int SpellEffects::engine2Look(const std::string& kind) {
  if (kind == "cureMinor") return 2;  // HolyLight (Engine2's own was 2D)
  if (kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "createFoodAndWater" || kind == "potion") return 1;
  if (kind == "magicMissile" || kind == "magicMissileV2") return 3;  // ours in 3D (SpellBolts; 02 = Engine2's MagicMissileV2VFX)
  if (kind == "divineBolt") return 1;                                // ours in 3D (SpellBolts)
  if (kind == "turnUndead") return 1;                                // ours in 3D (vfx/TurnUndead)
  if (isLightning(kind, 2)) return 2;                                // Raio, ours in 3D (vfx/Lightning; 01 = the cards, the enemies')
  if (kind == "fireball") return 1;                                  // FireballVFX
  if (kind == "bless") return 1;                                     // BlessVFX
  if (kind == "causticVenom") return 1;                              // CausticVenomVFX
  if (kind == "phantasmalForce") return 1;                           // PhantasmalForceVFX
  if (kind == "webOfDreams") return 2;                               // ours in 3D (DreamWeb: shot, floor, net; 01 = WebOfDreamsVFX)
  if (kind == "burningHands" || kind == "poisonBreath") return 2;    // BurningHandsV2VFX, its cone opening outward (02)
  if (kind == "cleave") return 1;                                    // CleaveSweepVFX
  if (kind == "piercingThrust") return 2;                            // ours in 3D (vfx/LancerFx; 01 = the flipbook burst)
  if (kind == "sweep") return 1;                                     // VarreduraVFX
  if (kind == "iceStorm") return 2;  // ours in 3D (Engine2's Blizzard2D, 01, is the enemies': content/fx/spell-looks.json)
  if (kind == "acidCloud") return 1;
  if (kind == "frost") return 2;     // Frost, ours in 3D (vfx/FrostLine; 01 = the cards, the enemies')
  return 0;
}

VfxFlipbook SpellEffects::flipbook(const char* clipName) {
  VfxFlipbook f;
  const SpriteClipId id = clip(clipName);
  if (id == kNoSpriteClip) return f;
  sprites_.request(id, SpriteSystem::kOnScreen);
  const SpriteSystem::Texture tex = sprites_.texture(id);
  if (tex.index == kNoTexture) return f;
  const SpriteClipHeader& h = sprites_.header(id);
  return {tex.index, static_cast<float>(h.sourceWidth), static_cast<float>(h.sourceHeight), static_cast<float>(h.cropX), static_cast<float>(h.cropY),
          static_cast<float>(h.width), static_cast<float>(h.height)};
}

ElementSpritesV2 SpellEffects::elementV2(VfxElement element, int version) {
  ElementSpritesV2 e = ElementSpritesV2::defaults(element);
  const std::string name = vfxElementName(element);
  const std::string prefix = name + (version == 3 ? "-v3-" : "-v2-");
  e.main = flipbook((prefix + "main-flipbook-4x4").c_str());
  e.secondary = flipbook((prefix + "secondary-flipbook-4x4").c_str());
  e.particles = flipbook((prefix + "particles-flipbook-4x4").c_str());
  return e;
}

std::vector<Bless::Ally> SpellEffects::blessAllies(const Spot& caster, const std::vector<Spot>& targets) {
  std::vector<Bless::Ally> allies;
  for (const Spot& s : targets) {
    const glm::vec2 d{s.feet.x - caster.feet.x, s.feet.z - caster.feet.z};
    allies.push_back({engine2Origin(s), std::round(glm::length(d) / std::sqrt(3.0f))});
  }
  return allies;
}

float SpellEffects::impactDelay(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets) const {
  if ((kind == "magicMissile" || kind == "magicMissileV2") && look == 2 && !targets.empty())
    return MagicMissileV2::impactDelay(engine2Origin(caster), targets.front().body);
  if (kind == "fireball" && look == 1 && !targets.empty()) return Fireball::travelSeconds(engine2Origin(caster), fireballTarget(targets));
  if (kind == "phantasmalForce" && look == 1) return PhantasmalForce::kImpactAt;
  if (kind == "spectralBolt" && !targets.empty()) return ArmsFx::spectralFlight(caster.feet, areaCentre(targets).feet);
  if (kind == "webOfDreams" && look >= 1) return look == 2 ? DreamWeb::kShotTravel : WebOfDreams::kShotTravel;
  if ((kind == "burningHands" || kind == "poisonBreath") && look >= 1) return BurningHands::kRelease;
  if (isBolt(kind, look) && !targets.empty()) {
    const SpellBolt::Style style = kind == "divineBolt" ? SpellBolt::Style::Divine : SpellBolt::Style::Arcane;
    return SpellBolt::flightSeconds(style, caster.feet, (kind == "divineBolt" ? areaCentre(targets) : targets.front()).feet);
  }
  if (kind == "causticVenom" && look >= 1 && !targets.empty())
    return CausticVenom::kCharge + CausticVenom::travelSeconds(engine2Origin(caster), aimedHex(targets));
  if (kind == "bless" && look == 1) {
    // The blessing lands on the nearest ally as the wave reaches it.
    float first = Bless::reachSeconds(3.0f);
    for (const Bless::Ally& a : blessAllies(caster, targets)) first = std::min(first, Bless::reachSeconds(a.distanceHexes));
    return first;
  }
  return -1.0f;
}

float SpellEffects::leadIn(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets) const {
  if (const float delay = impactDelay(kind, look, caster, targets); delay > 0.0f) return delay;
  if (isHoly(kind, look)) return HolyLight::leadIn(HolyLight::Style::Minor);
  if (kind == "turnUndead" && look >= 1) return TurnUndead::kLead;  // the sigil gathers, then the burst
  if (isHealer(kind)) return HealerFx::leadIn(kind);                // the Curandeiro's: the light gathers, then lands
  if (isCrusader(kind) && kind != "holyStrike") return CrusaderFx::leadIn(kind);  // the Cruzado's
  if (kind == "arcaneMarkBurst") return SeleneFx::leadIn(kind);      // Selene's mark: the rune flares, then the burst
  if (isConjurer(kind)) return ConjurerFx::leadIn(kind);            // the Conjurador's
  if (kind == "dispelMagic") return MageFx::kDispelLead;             // the Mago Negro's: the light gathers, then the wave
  if (kind.rfind("summon", 0) == 0) return 0.9f;                     // the summoning circle, then the familiar
  return kKindLead;
}

bool SpellEffects::isHoly(const std::string& kind, int look) {
  return (kind == "cureMinor" && look == 2) || kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" ||
         kind == "createFoodAndWater" || kind == "potion";
}

float SpellEffects::fireballArea(const std::vector<Spot>& targets) {
  if (targets.size() < 2) return 1.0f + 2.0f * std::sqrt(3.0f);
  float radius = 1.0f;
  const glm::vec3 centre = fireballTarget(targets);
  for (const Spot& s : targets) radius = std::max(radius, glm::length(s.feet + glm::vec3(0.0f, 0.04f, 0.0f) - centre) + 1.0f);
  return radius;
}

bool SpellEffects::potion(const std::string& effect, int look, const Spot& target, float start, float impact, int owner) {
  potionEffect_ = effect;
  const bool made = cast("potion", look, target, {target}, start, impact, owner);
  potionEffect_ = "heal";
  return made;
}

float SpellEffects::castEnd(int owner) const {
  float end = -1.0f;
  for (const Cast& c : casts_)
    if (c.owner == owner) end = std::max(end, c.end);
  return end;
}

bool SpellEffects::cast(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets, float start, float impact,
                        int owner) {
  // When everything this cast adds is over (the newest element or ported effect of each list), for castEnd.
  const size_t before[] = {elements_.size(), missiles_.size(), fireballs_.size(), blesses_.size(), venoms_.size(), phantasms_.size(),
                           webs_.size(),     burns_.size(),    cleaves_.size(),   sweeps_.size(),  holies_.size(),    dreamWebs_.size()};
  const bool made = castEffect(kind, look, caster, targets, start, impact);
  if (made && owner >= 0) {
    float end = start;
    for (size_t i = before[0]; i < elements_.size(); ++i) end = std::max(end, elements_[i].end);
    auto latest = [&](const auto& list, size_t from) {
      for (size_t i = from; i < list.size(); ++i) end = std::max(end, list[i].end());
    };
    latest(missiles_, before[1]), latest(fireballs_, before[2]), latest(blesses_, before[3]), latest(venoms_, before[4]);
    latest(phantasms_, before[5]), latest(webs_, before[6]), latest(cleaves_, before[8]), latest(sweeps_, before[9]);
    latest(holies_, before[10]), latest(dreamWebs_, before[11]);
    if (kind == "turnUndead") end = std::max(end, turnUndead_.latestEnd());
    if (isHealer(kind)) end = std::max(end, healer_.latestEnd());
    if (isCrusader(kind)) end = std::max(end, crusader_.latestEnd());
    if (isSelene(kind)) end = std::max(end, selene_.latestEnd());
    if (isConjurer(kind)) end = std::max(end, conjurer_.latestEnd());
    if (isDagger(kind)) end = std::max(end, daggers_.latestEnd());
    if (isMage(kind)) end = std::max(end, mage_.latestEnd());
    for (size_t i = before[7]; i < burns_.size(); ++i) end = std::max(end, burns_[i].first.end());
    if (kind == "acidCloud") end = std::max(end, start + 0.55f);
    if (kind == "iceStorm" && !storms_.empty()) end = std::max(end, storms_.back().start() + 1.0f);  // its first second, not the whole storm
    if (isBolt(kind, look) && !bolts_.empty()) end = std::max(end, bolts_.back().end());
    if (isLightning(kind, look) && !lightnings_.empty()) end = std::max(end, lightnings_.back().end());
    if (isFrost(kind, look) && !frosts_.empty()) end = std::max(end, frosts_.back().end());
    std::erase_if(casts_, [&](const Cast& c) { return c.end <= start; });
    casts_.push_back({owner, end});
  }
  return made;
}

bool SpellEffects::castEffect(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets, float start, float impact) {
  if (look < 1 || look > looks(kind)) return false;
  impact = std::max(impact, start + 0.05f);
  if (targets.empty() && kind != "piercingThrust" && kind != "doubleStrike" && kind != "steelWhirlwind" && kind != "warCry" && kind != "sentinelVigil" &&
      kind != "provoke")
    return false;
  // The cures', the meal's and the potion's light (HolyLight), on every unit it reaches.
  if (isHoly(kind, look)) {
    using Style = HolyLight::Style;
    const Style style = kind == "cureMinor" ? Style::Minor : kind == "cureWounds" ? Style::Medium : kind == "cureLight" ? Style::Hands
                        : kind == "cureDisease" ? Style::Disease : kind == "createFoodAndWater" ? Style::Food
                        : potionEffect_ == "disease" ? Style::PotionCure : potionEffect_ == "mana" ? Style::PotionMana : Style::PotionHeal;
    std::vector<HolyLight::Target> reached;
    for (const Spot& s : targets)  // its card's height, else what the body point says (Spot: body = 0.45 of the card)
      reached.push_back({s.feet, s.cardHeight > 0.0f ? s.cardHeight : std::max(1.2f, (s.body.y - s.feet.y) / 0.45f)});
    holies_.emplace_back(style, std::move(reached), impact, static_cast<uint32_t>(random() * 4294967295.0f));
    return true;
  }
  // The Mago Negro's Nuvem de Adagas (DaggerFx, ours, 3D): the conjuring on the caster; the thrown dagger on the attacker (the cloud
  // standing round the caster lends it: MapStage syncDaggers keeps the cloud).
  if (kind == "cloudOfDaggers") {
    daggers_.summon(caster.feet, caster.cardHeight, impact);
    return true;
  }
  if (kind == "daggerStrike") {
    daggers_.strike(caster.feet, {targets.front().feet, targets.front().cardHeight}, start, impact);
    return true;
  }
  // Dissipar Magia (MageFx): the wave over the area's hexes (the aimed one first), the magic torn off the bodies among them (card heights);
  // the Globo's rise on the caster (its standing bubble is the stage's: syncGlobes).
  if (kind == "dispelMagic" || kind == "globeOfInvulnerability") {
    const Spot& centre = kind == "dispelMagic" ? areaCentre(targets) : caster;
    float radius = 0.0f;
    for (const Spot& s : targets) radius = std::max(radius, glm::length(glm::vec2(s.feet.x - centre.feet.x, s.feet.z - centre.feet.z)));
    radius += 1.0f;  // out to the far hexes' own edge
    if (kind == "dispelMagic") {
      std::vector<MageFx::Target> bodies;
      for (const Spot& s : targets)
        if (s.cardHeight > 0.0f) bodies.push_back({s.feet, s.cardHeight});
      mage_.dispel(centre.feet, radius, std::move(bodies), impact);
    } else {
      daggers_.summon(caster.feet, caster.cardHeight, impact);  // the cast's flash on the caster; the bubble itself rises from the stage (syncGlobes)
    }
    return true;
  }
  // The Guerreiro's skills (WarriorFx, ours, 3D). Targets: the foes round the caster (Redemoinho), the struck foe (Quebra-Guarda, the
  // Salto's blow), the allies rallied (Grito).
  auto warriorTarget = [](const Spot& s) { return WarriorFx::Target{s.feet, s.cardHeight}; };
  if (kind == "steelWhirlwind" || kind == "guardBreak" || kind == "warCry" || kind == "leapingStrike") {
    std::vector<WarriorFx::Target> hit;
    for (const Spot& s : targets)
      if (kind != "steelWhirlwind" || s.cardHeight > 0.0f) hit.push_back(warriorTarget(s));  // the blade flashes on bodies, not on bare hexes
    if (kind == "steelWhirlwind") warrior_.whirlwind(caster.feet, caster.cardHeight, hit, impact);
    else if (kind == "guardBreak") warrior_.guardBreak(hit.front(), caster.feet, impact);
    else if (kind == "leapingStrike") warrior_.leapBlow(hit.front(), caster.feet, impact);
    else {
      float radius = 0.0f;
      for (const Spot& s : targets) radius = std::max(radius, glm::length(glm::vec2(s.feet.x - caster.feet.x, s.feet.z - caster.feet.z)));
      warrior_.warCry(caster.feet, caster.cardHeight, hit, radius + 1.2f, impact);
    }
    return true;
  }
  // The weapon skills, the auras and the summons that had no effect (ArmsFx, ours, 3D). Targets: the struck foe (the blows), the
  // provoked foes, the hexes struck in order (Investida de Ombro, Debandada, Ira Divina: the bodies struck keep their card height),
  // the aura's hexes (the bodies it reaches keep their card height), the summon's hex.
  auto armsTarget = [](const Spot& s) { return ArmsFx::Target{s.feet, s.cardHeight}; };
  auto bodiesOf = [&](const std::vector<Spot>& spots) {
    std::vector<ArmsFx::Target> out;
    for (const Spot& s : spots)
      if (s.cardHeight > 0.0f && glm::length(s.feet - caster.feet) > 0.3f) out.push_back(armsTarget(s));
    return out;
  };
  if (kind == "provoke") {
    arms_.provoke(caster.feet, caster.cardHeight, bodiesOf(targets), impact);
    return true;
  }
  if (kind == "shieldBash" || kind == "bullRush" || kind == "executionerStrike" || kind == "trip") {
    const ArmsFx::Target foe = armsTarget(targets.front());
    if (kind == "shieldBash") arms_.shieldBash(foe, caster.feet, impact);
    else if (kind == "bullRush") arms_.bullRush(foe, caster.feet, impact);
    else if (kind == "executionerStrike") arms_.executioner(foe, caster.feet, impact);
    else arms_.trip(foe, caster.feet, impact);
    return true;
  }
  if (kind == "shoulderSmash" || kind == "stampede" || kind == "divineWrath") {
    std::vector<glm::vec3> hexes;
    for (const Spot& s : targets) hexes.push_back(s.feet);
    if (kind == "shoulderSmash") arms_.shoulderSmash(caster.feet, caster.cardHeight, hexes, bodiesOf(targets), impact);
    else if (kind == "stampede") arms_.stampede(caster.feet, caster.cardHeight, hexes, bodiesOf(targets), impact);
    else arms_.divineWrath(caster.feet, caster.cardHeight, hexes, bodiesOf(targets), impact);
    return true;
  }
  if (kind == "auraOfProtection" || kind == "intimidatingPresence") {
    float radius = 0.0f;
    for (const Spot& s : targets) radius = std::max(radius, glm::length(glm::vec2(s.feet.x - caster.feet.x, s.feet.z - caster.feet.z)));
    std::vector<ArmsFx::Target> bodies;
    for (const Spot& s : targets)
      if (s.cardHeight > 0.0f) bodies.push_back(armsTarget(s));
    arms_.aura(kind == "auraOfProtection", caster.feet, caster.cardHeight, radius + 0.9f, bodies, impact);
    return true;
  }
  if (kind == "spectralBolt") {  // Raio Espectral: the teal-green spectral bolt over its 3 hexes
    std::vector<glm::vec3> hexes;
    for (const Spot& s : targets) hexes.push_back(s.feet);
    const float release = std::max(start, impact - ArmsFx::spectralFlight(caster.feet, areaCentre(targets).feet));
    arms_.spectralBolt(caster.feet, caster.cardHeight, hexes, bodiesOf(targets), release, impact);
    return true;
  }
  if (kind == "fantomForce") {  // Força Fantasma: the lesser Força Fantasmagórica, red and magenta
    arms_.phantom(armsTarget(targets.front()), impact);
    return true;
  }
  if (kind == "arcaneReflection") {  // the ward on the caster (its bounce comes from the stage: reflectHit)
    arms_.reflectWard(caster.feet, caster.cardHeight, impact);
    return true;
  }
  if (kind.rfind("summon", 0) == 0) {
    // the Cervo Azul do Pântano (2026-10-27) comes in the Radiante's blue light, like its antlers
    const ArmsFx::Summon style = kind == "summonFamiliar2" ? ArmsFx::Summon::Greater : kind == "summonFamiliar4" || kind == "summonSwampDeer" ? ArmsFx::Summon::Radiant
                                 : kind == "summonFamiliar3" ? ArmsFx::Summon::Titan : kind == "summonZombieDog" ? ArmsFx::Summon::ZombieDog
                                                             : ArmsFx::Summon::Familiar;
    arms_.summon(style, targets.front().feet, impact);
    return true;
  }
  // Tiro Longo / Tiro Perfurante 02 (the plain shot too): the real 3D arrow of RangerFx instead of the 01 card, the enemies' (the
  // crossbowmen's bolt, the user 2026-10-20: "an arrow to be drawn for his ATT and skills"). The piercing one flies to the last body.
  if ((kind == "longShot" || kind == "piercing") && look == 2) {
    const Spot* foe = &targets.front();
    if (kind == "piercing")
      for (const Spot& s : targets)
        if (s.cardHeight > 0.0f && glm::length(s.feet - caster.feet) > 0.3f) foe = &s;
    const float release = std::max(start, impact - RangerFx::flightSeconds(caster.feet, foe->feet));
    ranger_.shot(RangerFx::Shot::Swift, caster.feet, caster.cardHeight, {foe->feet, foe->cardHeight}, release, impact);
    return true;
  }
  if (kind == "bloodyShot" || kind == "multiShot") {
    const Spot& foe = targets.front();
    const float release = std::max(start, impact - RangerFx::flightSeconds(caster.feet, foe.feet));
    ranger_.shot(kind == "bloodyShot" ? RangerFx::Shot::Bloody : RangerFx::Shot::Swift, caster.feet, caster.cardHeight, {foe.feet, foe.cardHeight}, release, impact);
    return true;
  }
  // Expulsar Mortos-Vivos (TurnUndead, ours, 3D). Targets: the hexes of the area (cardHeight 0) and the undead standing in it (the
  // stage gives bodies a card height only for the undead it strikes); the caster's own hex is no target.
  if (kind == "turnUndead") {
    float radius = 0.0f;
    std::vector<TurnUndead::Target> undead;
    for (const Spot& s : targets) {
      const float d = glm::length(glm::vec2(s.feet.x - caster.feet.x, s.feet.z - caster.feet.z));
      radius = std::max(radius, d);
      if (s.cardHeight > 0.0f && d > 0.3f) undead.push_back({s.feet, s.cardHeight});
    }
    turnUndead_.cast(caster.feet, caster.cardHeight, radius + 1.0f, std::move(undead), impact);
    return true;
  }
  // The Cruzado's (CrusaderFx, ours, 3D). Targets: Golpe Consagrado the foe (its holy flash is the stage's, on a landed blow),
  // Toque Sagrado the ally, Manto do Cruzado the allies it reaches (the caster among them).
  if (isCrusader(kind)) {
    if (kind == "holyStrike") {
      if (!targets.empty()) crusader_.bladeGlow(caster.feet, caster.cardHeight, targets.front().feet, start, impact);
    } else if (kind == "holyTouch") {
      if (!targets.empty()) crusader_.touch(caster.feet, caster.cardHeight, {targets.front().feet, heightOf(targets.front())}, impact);
    } else {
      std::vector<CrusaderFx::Target> allies;
      float radius = 0.0f;
      for (const Spot& s : targets) {
        allies.push_back({s.feet, heightOf(s)});
        radius = std::max(radius, glm::length(glm::vec2(s.feet.x - caster.feet.x, s.feet.z - caster.feet.z)));
      }
      crusader_.mantle(caster.feet, caster.cardHeight, std::max(radius + 1.0f, 2.5f * 1.7320508f), std::move(allies), impact);
    }
    return true;
  }
  // Selene's (SeleneFx, ours, 3D). Targets: Lâmina Arcana the foe (the mark it lays is the stage's, on a landed blow); the mark's burst
  // the marked body.
  if (isSelene(kind)) {
    if (targets.empty()) return true;
    if (kind == "arcaneBlade") selene_.bladeGlow(caster.feet, caster.cardHeight, targets.front().feet, start, impact);
    else selene_.burst({targets.front().feet, heightOf(targets.front())}, impact);
    return true;
  }
  // The Curandeiro's spells (HealerFx, ours, 3D). Targets: the area's hexes (cardHeight 0) with the bodies it reaches keeping their
  // card height (the stage gives it to the allies healed / the foes struck only); the Ressurreição's one target is the fallen body.
  if (isHealer(kind)) {
    if (kind == "resurrection") {
      const Spot& fallen = targets.front();
      healer_.resurrection(caster.feet, caster.cardHeight, {fallen.feet, heightOf(fallen)}, impact);
      return true;
    }
    const Spot& centre = areaCentre(targets);
    float radius = 0.0f;
    std::vector<HealerFx::Target> bodies;
    for (const Spot& s : targets) {
      radius = std::max(radius, glm::length(glm::vec2(s.feet.x - centre.feet.x, s.feet.z - centre.feet.z)));
      if (s.cardHeight > 0.0f) bodies.push_back({s.feet, s.cardHeight});
    }
    if (kind == "circleOfHealing") healer_.circle(caster.feet, caster.cardHeight, centre.feet, radius + 1.0f, std::move(bodies), impact);
    else healer_.judgment(caster.feet, caster.cardHeight, centre.feet, radius + 1.0f, std::move(bodies), impact);
    return true;
  }
  // The Conjurador's spells (ConjurerFx, ours, 3D). Targets: Troca de Lugar the ally's hex then the caster's own; Lança de Osso the
  // line's hexes (the bodies struck keep their card height); Imagem Espelhada the ally; Detonar Invocação the blast's hexes (the
  // summon's first, the bodies in it with a card height); Elo / Fortalecer the summons; Pesadelo the area's hexes.
  if (isConjurer(kind)) {
    std::vector<ConjurerFx::Target> bodies;
    for (const Spot& s : targets)
      if (s.cardHeight > 0.0f) bodies.push_back({s.feet, s.cardHeight});
    if (kind == "placeSwap") {
      if (targets.size() >= 2) conjurer_.swap({targets[1].feet, heightOf(targets[1])}, {targets[0].feet, heightOf(targets[0])}, impact, impact + 0.42f);
      else conjurer_.swap({caster.feet, caster.cardHeight}, {targets.front().feet, heightOf(targets.front())}, impact, impact + 0.42f);
    } else if (kind == "boneLance") {
      std::vector<glm::vec3> hexes;
      for (const Spot& s : targets) hexes.push_back(s.feet);
      conjurer_.lance(caster.feet, caster.cardHeight, hexes, std::move(bodies), std::max(start, impact - ConjurerFx::lanceSeconds(hexes.size())), impact);
    } else if (kind == "mirrorImage") {
      conjurer_.mirrorBurst({targets.front().feet, heightOf(targets.front())}, impact);
    } else if (kind == "detonateSummon") {
      const Spot& centre = areaCentre(targets);
      float radius = 0.0f;
      for (const Spot& s : targets) radius = std::max(radius, glm::length(glm::vec2(s.feet.x - centre.feet.x, s.feet.z - centre.feet.z)));
      std::erase_if(bodies, [&](const ConjurerFx::Target& b) { return glm::length(b.feet - centre.feet) < 0.3f; });  // the summon itself is the blast
      conjurer_.detonate({centre.feet, heightOf(centre)}, radius + 1.0f, std::move(bodies), impact);
    } else if (kind == "sacrificeBond") {
      std::erase_if(bodies, [&](const ConjurerFx::Target& b) { return glm::length(b.feet - caster.feet) < 0.3f; });
      conjurer_.bond(caster.feet, caster.cardHeight, std::move(bodies), impact);
    } else if (kind == "nightmare") {
      const Spot& centre = areaCentre(targets);
      float radius = 0.0f;
      for (const Spot& s : targets) radius = std::max(radius, glm::length(glm::vec2(s.feet.x - centre.feet.x, s.feet.z - centre.feet.z)));
      conjurer_.nightmare(centre.feet, radius + 1.0f, std::move(bodies), impact);
    } else {
      std::erase_if(bodies, [&](const ConjurerFx::Target& b) { return glm::length(b.feet - caster.feet) < 0.3f; });
      conjurer_.empower(caster.feet, caster.cardHeight, std::move(bodies), impact);
    }
    return true;
  }
  // The Arqueiro's skills (RangerFx, ours, 3D): one arrow at the struck foe (the first target; the burst's neighbours follow it),
  // or a volley over every hex of the area (the bodies in it flash).
  if (kind == "venomArrow" || kind == "acidArrow" || kind == "explosiveArrow" || kind == "cripplingShot" || kind == "deathShot") {
    const RangerFx::Shot style = kind == "venomArrow" || kind == "acidArrow" ? RangerFx::Shot::Venom : kind == "explosiveArrow" ? RangerFx::Shot::Explosive
                                 : kind == "cripplingShot" ? RangerFx::Shot::Crippling : RangerFx::Shot::Death;
    const Spot& foe = targets.front();
    const float release = std::max(start, impact - RangerFx::flightSeconds(caster.feet, foe.feet));
    ranger_.shot(style, caster.feet, caster.cardHeight, {foe.feet, foe.cardHeight}, release, impact);
    return true;
  }
  if (kind == "arrowRain" || kind == "arrowStorm") {
    std::vector<glm::vec3> hexes;
    std::vector<RangerFx::Target> foes;
    for (const Spot& s : targets) {
      hexes.push_back(s.feet);
      if (s.cardHeight > 0.0f) foes.push_back({s.feet, s.cardHeight});
    }
    ranger_.volley(caster.feet, hexes, foes, start, impact, kind == "arrowStorm");
    return true;
  }
  // The Lanceiro's skills (LancerFx, ours, 3D): the lance's blow on the first target (the struck foe; the swap's targets are
  // the ally then the caster's own hex); the board moves follow from the stage (lancerFlight, swapFlight, vaultFlight).
  if (kind == "spearToss" || kind == "hookPull" || kind == "poleVault" || kind == "disarm" || kind == "swapPost" || kind == "impaleDrag" ||
      kind == "sentinelVigil" || kind == "sentinelStrike") {
    auto lancerTarget = [](const Spot& s) { return LancerFx::Target{s.feet, s.cardHeight}; };
    if (kind == "sentinelVigil") {
      float radius = 0.0f;
      for (const Spot& s : targets) radius = std::max(radius, glm::length(glm::vec2(s.feet.x - caster.feet.x, s.feet.z - caster.feet.z)));
      lancer_.vigilRise(caster.feet, caster.cardHeight, radius + 0.6f, impact);
      return true;
    }
    const LancerFx::Target foe = lancerTarget(targets.front());
    if (kind == "swapPost") {
      // The Effect step aims two hexes (the ally's, then the caster's own): the dash of both as the rules swap them at the impact
      // (the cards fly from the stage, 0.42 s); its blow is a Combat step on one foe.
      if (targets.size() >= 2) lancer_.swap(targets[1].feet, targets[1].cardHeight, targets[0].feet, targets[0].cardHeight, impact, impact + 0.42f);
      else lancer_.swapBlow(foe, caster.feet, impact);
      return true;
    }
    if (kind == "spearToss") lancer_.toss(foe, caster.feet, impact);
    else if (kind == "hookPull") lancer_.hook(caster.feet, caster.cardHeight, foe, std::max(start, impact - RangerFx::flightSeconds(caster.feet, foe.feet)), impact);
    else if (kind == "poleVault") lancer_.vaultBlow(foe, caster.feet, impact);
    else if (kind == "disarm") lancer_.disarm(foe, caster.feet, impact);
    else if (kind == "impaleDrag") lancer_.impale(foe, caster.feet, impact);
    else lancer_.vigilStrike(foe, caster.feet, impact);
    return true;
  }
  if (isFrost(kind, look)) {
    // Frost 02, ours, in 3D (vfx/FrostLine): ice erupts on every hex of the line, nearest first.
    std::vector<glm::vec3> hexes;
    for (const Spot& s : targets) hexes.push_back(s.feet);
    frosts_.emplace_back(caster.feet, heightOf(caster), std::move(hexes), start, impact, 0xf2057u);
  } else if (isLightning(kind, look)) {
    // Relâmpago 02, ours, in 3D (vfx/Lightning): a bolt from the sky on every struck body; Choque is the same at 3/4 size.
    std::vector<Lightning::Target> struck;
    for (const Spot& s : targets) struck.push_back({s.feet, heightOf(s)});
    lightnings_.emplace_back(caster.feet, heightOf(caster), std::move(struck), start, impact,
                             (kind == "shock" ? 0.75f : 1.0f) * (look == 3 ? kTinyFx : 1.0f), 0x11417u);
  } else if (isBolt(kind, look)) {
    // Ours, in 3D (vfx/SpellBolts): one look, the same on every cast (fixed seeds).
    if (kind == "divineBolt") {
      const Spot& centre = areaCentre(targets);
      std::vector<glm::vec3> splash;
      for (const Spot& s : targets) splash.push_back(s.feet);
      bolts_.emplace_back(SpellBolt::Style::Divine, caster.feet, heightOf(caster), centre.feet, heightOf(centre), std::move(splash), start, impact, 0xd1b1e7u);
    } else {
      bolts_.emplace_back(SpellBolt::Style::Arcane, caster.feet, heightOf(caster), targets.front().feet, heightOf(targets.front()), std::vector<glm::vec3>{},
                          start, impact, 0xa11ce5u, look == 4 ? kTinyFx : 1.0f);
    }
  } else if ((kind == "magicMissile" || kind == "magicMissileV2") && look == 2) missiles_.emplace_back(engine2Origin(caster), targets.front().body, start);
  else if (kind == "fireball") fireballs_.emplace_back(engine2Origin(caster), fireballTarget(targets), fireballArea(targets), start);
  else if (kind == "phantasmalForce") phantasms_.emplace_back(targets.front().body, start);
  else if (kind == "cleave" || kind == "sweep") {
    // BattleSpells3D: Cleave's blade swings at weapon height (0.7) through its targets, Varredura's wave runs on the
    // ground (0.04); both queued 0.18 s into the swing.
    const float height = kind == "sweep" ? 0.04f : 0.7f;
    std::vector<glm::vec3> hit;
    for (const Spot& s : targets) hit.push_back(s.feet + glm::vec3(0.0f, height, 0.0f));
    if (kind == "sweep") sweeps_.emplace_back(caster.feet + glm::vec3(0.0f, height, 0.0f), hit, start);
    // Cleave's blade comes in on the pose's own slash (the hit), not 0.18 s after the pose begins (the user, 2026-10-20: it
    // showed before the sword moved).
    else cleaves_.emplace_back(caster.feet + glm::vec3(0.0f, height, 0.0f), hit, std::max(start, impact - CleaveSweep::kQueued));
  } else if (kind == "burningHands" || kind == "poisonBreath") {
    std::vector<glm::vec3> cells;
    for (const Spot& s : targets) cells.push_back(s.feet + glm::vec3(0.0f, 0.04f, 0.0f));
    const bool poison = kind == "poisonBreath";
    burns_.emplace_back(BurningHands(engine2Origin(caster), cells, poison, start, look), poison);
  } else if (kind == "webOfDreams" && look == 2 && !targets.empty()) {
    // Ours (vfx/DreamWeb): the shot from the caster, the floor on every hex, the net over the zone's middle hex (the rules list the
    // area row by row: the nearest hex to its mean, not the first), each unit in it wrapped.
    const glm::vec3 centre = areaCentre(targets).feet;
    std::vector<glm::vec3> hexes;
    float reach = 0.0f;
    for (const Spot& s : targets) {
      hexes.push_back(s.feet);
      reach = std::max(reach, glm::length(glm::vec2(s.feet.x - centre.x, s.feet.z - centre.z)));
    }
    std::vector<DreamWeb::Occupant> inside;
    for (const WebUnit& u : webPending_) inside.push_back({u.key, u.at.feet, heightOf(u.at)});
    if (webPending_.empty())  // no rules (--fx-test): the spots that are units
      for (size_t i = 0; i < targets.size(); ++i)
        if (targets[i].cardHeight > 0.0f) inside.push_back({static_cast<int>(i), targets[i].feet, heightOf(targets[i])});
    webPending_.clear();
    dreamWebs_.emplace_back(caster.body, centre, std::move(hexes), std::clamp(static_cast<int>(std::lround(reach / std::sqrt(3.0f))), 1, 3), std::move(inside), start,
                            false);
    dreamWebs_.back().id = ++webSerial_;
  } else if (kind == "webOfDreams") {
    // The zone on the aimed hex (WEB_OF_DREAMS.size 1: webZoneRadiusTiles = sqrt 3 + 1); the units in it wrapped. It holds
    // as Engine2's preview does (2.6 s after the cast), then releases.
    std::vector<glm::vec3> wrapped;
    for (const Spot& s : targets) wrapped.push_back(engine2Origin(s));
    webs_.emplace_back(targets.front().feet + glm::vec3(0.0f, 0.04f, 0.0f), std::sqrt(3.0f) + 1.0f, wrapped, start, start + 2.6f);
  }
  else if (kind == "iceStorm") {
    // The storm over the whole field (targets = its hexes), from the impact (vfx/IceStorm).
    std::vector<glm::vec3> hexes;
    for (const Spot& s : targets) hexes.push_back(s.feet);
    storms_.emplace_back(look, std::move(hexes), impact);
    storms_.back().id = ++stormSerial_;
  } else if (kind == "acidCloud") {
    // The persistent, lit 3D field begins through zoneStart when the rules add its area.
  } else if (kind == "causticVenom") {
    std::vector<glm::vec3> hexes;
    for (const Spot& s : targets) hexes.push_back(s.feet + glm::vec3(0.0f, 0.04f, 0.0f));
    // 01 Engine2's CausticVenomVFX (the heroes' default); 02 the same flight, the impact in 3D (green smoke lit by the scene, a green
    // light, droplets).
    venoms_.emplace_back(look >= 2 ? 2 : 1, engine2Origin(caster), aimedHex(targets), hexes, start);
  } else if (kind == "bless") blesses_.emplace_back(caster.feet + glm::vec3(0.0f, 0.04f, 0.0f), 3.0f * std::sqrt(3.0f), blessAllies(caster, targets), start);
  else if (kind == "magicMissile" || kind == "magicMissileV2") magicMissile(caster, targets.front(), start, impact);
  else if (kind == "lightning" || kind == "lightningTier3") lightning(caster, targets, start, impact, false);
  else if (kind == "shock") lightning(caster, targets, start, impact, true);
  else if (kind == "frost") frost(caster, targets, start, impact);
  else if (kind == "cureMinor") cure(caster, targets, start, impact);
  else if (kind == "longShot") arrow(caster, targets, start, impact, false);
  else if (kind == "piercing") arrow(caster, targets, start, impact, true);
  else if (kind == "piercingThrust" && look == 2) {
    // Investida Perfurante 02 (the user, 2026-10-21: the old one "is not compatible with 3D"): the thrust down the whole line, a flash
    // on every body it runs through (targets: the line's hexes; the bodies struck keep their card height).
    std::vector<LancerFx::Target> bodies;
    for (const Spot& s : targets)
      if (s.cardHeight > 0.0f) bodies.push_back({s.feet, s.cardHeight});
    lancer_.pierce(caster.feet, caster.cardHeight, targets.empty() ? caster.feet : targets.back().feet, std::move(bodies), std::max(start, impact - 0.1f), impact);
  } else if (kind == "piercingThrust" || kind == "doubleStrike") bladeSkill(caster, start, impact);
  else if (emberCast(kind, caster, targets, start, impact)) return true;
  else return false;
  return true;
}

// Míssil Mágico: arcane gathers in a ring at the caster's feet and in the hand, an orb (the arcane sheet: a swirling
// sphere) flies to the target's body trailing sparks, and bursts there with a violet flash. It arrives on the hit.
void SpellEffects::magicMissile(const Spot& caster, const Spot& target, float start, float impact) {
  const glm::vec3 d = target.body - caster.body;
  const float dist = glm::length(d);
  const glm::vec3 dir = dist > 1e-3f ? d / dist : glm::vec3(1, 0, 0);
  const float travel = std::clamp(dist / 14.0f, 0.25f, 0.6f);  // Engine2 SPELL_TRAVEL 0.6 at most
  const float launch = std::max(start + 0.05f, impact - travel);

  Element& ring = add("arcane-v3-secondary-flipbook-4x4", start, launch - start + 0.3f);
  ring.from = ring.to = caster.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 2.0f;
  ring.fadeIn = 0.1f, ring.fadeOut = 0.25f;
  ring.color = glm::vec3(1.1f);
  ring.pull = 0.8f, ring.softness = 0.3f;
  ring.lightColor = kViolet, ring.lightIntensity = 4.0f, ring.lightRadius = 2.5f;
  burst("arcane-v3-particles-flipbook-4x4", caster.body + dir * 0.35f, std::max(start, launch - 0.35f), 0.45f, 1.2f, glm::vec3(1.2f), 0.6f);

  Element& orb = add("arcane-flipbook-4x4", launch, impact - launch);
  orb.from = caster.body + dir * 0.4f;
  orb.to = target.body;
  orb.moveStart = launch, orb.moveEnd = impact;
  orb.arc = std::min(dist * 0.12f, 1.2f);
  orb.pivot = {0.5f, 0.5f};
  orb.height = 1.2f;
  orb.fps = 24.0f;
  orb.fadeIn = 0.05f;
  orb.color = glm::vec3(1.5f);
  orb.pull = 0.3f;
  orb.lightColor = kViolet, orb.lightIntensity = 10.0f, orb.lightRadius = 3.5f;
  const Element flight = orb;  // the trail reads the orb's path (the vector may grow below)
  for (float t = launch; t < impact; t += 0.03f) {
    Element& spark = add("arcane-v3-particles-flipbook-4x4", t, 0.4f);
    spark.from = spark.to = position(flight, t) + glm::vec3(random() - 0.5f, random() - 0.5f, random() - 0.5f) * 0.12f;
    spark.pivot = {0.5f, 0.5f};
    spark.height = 0.55f, spark.endHeight = 0.2f;
    spark.fadeOut = 0.3f;
    spark.color = glm::vec3(0.9f);
    spark.pull = 0.3f;
    spark.mirror = random() < 0.5f;
  }

  burst("arcane-v3-particles-flipbook-4x4", target.body, impact, 0.6f, 2.8f, glm::vec3(1.6f));
  Element& hitRing = add("arcane-v3-secondary-flipbook-4x4", impact, 0.7f);
  hitRing.from = hitRing.to = target.feet;
  hitRing.pivot = {0.5f, 0.45f};
  hitRing.height = 2.2f;
  hitRing.fadeOut = 0.3f;
  hitRing.color = glm::vec3(1.2f);
  hitRing.pull = 0.8f, hitRing.softness = 0.3f;
  Element& flash = add(nullptr, impact, 0.5f);
  flash.from = flash.to = target.body;
  flash.lightColor = kViolet, flash.lightIntensity = 24.0f, flash.lightRadius = 4.5f, flash.lightDecay = 2.0f;
}

// One strike from the sky (Relâmpago, Choque, the echo): the bolt art changing shape every 50 ms, the lightning
// eruption and ring at the feet, sparks on the body, a blue-white flash. scale 1 = Relâmpago.
void SpellEffects::bolt(const Spot& target, float at, float scale) {
  Element& core = add("lightning-core-v1", at, (scale >= 1.0f ? 0.72f : 0.42f) * 0.6f);  // Engine2 LIGHTNING_RAIO / STRIKE_DUR
  core.flicker = {clip("lightning-core-v1"), clip("lightning-core-v2"), clip("lightning-core-v3")};
  core.from = core.to = target.feet;
  core.pivot = {0.5f, 0.03f};
  core.height = 6.5f * scale;
  core.fadeOut = 0.12f;
  core.color = glm::vec3(1.6f);
  core.pull = 1.4f, core.softness = 0.2f;
  core.mirror = random() < 0.5f;

  Element& rise = add("lightning-v3-main-flipbook-4x4", at, 0.75f);
  rise.from = rise.to = target.feet;
  rise.pivot = {0.5f, 0.06f};
  rise.height = 2.8f * scale;
  rise.fadeOut = 0.2f;
  rise.color = glm::vec3(1.3f);
  rise.pull = 1.3f, rise.softness = 0.3f;
  rise.mirror = random() < 0.5f;

  Element& ring = add("lightning-v3-secondary-flipbook-4x4", at, 0.7f);
  ring.from = ring.to = target.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 2.2f * scale;
  ring.fadeOut = 0.25f;
  ring.color = glm::vec3(1.2f);
  ring.pull = 0.9f, ring.softness = 0.3f;

  burst("lightning-v3-particles-flipbook-4x4", target.body, at + 0.02f, 0.5f, 1.8f * scale, glm::vec3(1.4f));
  Element& flash = add(nullptr, at, 0.45f);
  flash.from = flash.to = target.feet + glm::vec3(0.0f, 2.0f, 0.0f);
  flash.lightColor = kBolt, flash.lightIntensity = 40.0f * scale, flash.lightRadius = 6.0f, flash.lightDecay = 1.5f;
}

// Relâmpago / Choque: the caster gathers lightning in a ring, then each struck hex takes a bolt (a beat apart).
void SpellEffects::lightning(const Spot& caster, const std::vector<Spot>& targets, float start, float impact, bool shock) {
  Element& ring = add("lightning-v3-secondary-flipbook-4x4", start, impact - start + 0.15f);
  ring.from = ring.to = caster.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 1.9f;
  ring.fadeIn = 0.1f, ring.fadeOut = 0.2f;
  ring.pull = 0.8f, ring.softness = 0.3f;
  ring.lightColor = kBolt, ring.lightIntensity = 4.0f, ring.lightRadius = 2.5f;
  burst("lightning-v3-particles-flipbook-4x4", caster.body, std::max(start, impact - 0.3f), 0.4f, 1.2f, glm::vec3(1.2f), 0.6f);
  for (size_t i = 0; i < targets.size(); ++i) bolt(targets[i], impact + 0.05f * static_cast<float>(i), shock ? 0.75f : 1.0f);
}

void SpellEffects::echo(const Spot& target, float at) { bolt(target, at, 0.6f); }

// Frost: cold gathers on the caster; ice erupts on every hex of the area, nearest first, each with its ring and an
// icy flash (Engine2 FROST_FLASH: a strong lantern, gone in a fraction of a second).
void SpellEffects::frost(const Spot& caster, const std::vector<Spot>& targets, float start, float impact) {
  burst("frost-v3-particles-flipbook-4x4", caster.body, start + 0.1f, std::max(impact - start, 0.3f), 1.3f, glm::vec3(1.1f), 0.6f);
  Element& gather = add("frost-v3-secondary-flipbook-4x4", start, impact - start + 0.2f);
  gather.from = gather.to = caster.feet;
  gather.pivot = {0.5f, 0.45f};
  gather.height = 1.8f;
  gather.fadeIn = 0.1f, gather.fadeOut = 0.2f;
  gather.pull = 0.8f, gather.softness = 0.3f;

  std::vector<size_t> order(targets.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return glm::length(targets[a].feet - caster.feet) < glm::length(targets[b].feet - caster.feet);
  });
  for (size_t rank = 0; rank < order.size(); ++rank) {
    const Spot& t = targets[order[rank]];
    const float at = impact + 0.05f * static_cast<float>(rank);
    Element& ice = add("frost-v3-main-flipbook-4x4", at - 0.05f, 0.95f);
    ice.from = ice.to = t.feet;
    ice.pivot = {0.5f, 0.06f};
    ice.height = 2.3f;
    ice.fadeOut = 0.25f;
    ice.color = glm::vec3(1.1f);
    ice.opacity = 0.35f;  // ice is half solid: it veils what stands behind it
    ice.pull = 0.9f, ice.softness = 0.3f;
    ice.mirror = random() < 0.5f;
    Element& ring = add("frost-v3-secondary-flipbook-4x4", at, 0.9f);
    ring.from = ring.to = t.feet;
    ring.pivot = {0.5f, 0.45f};
    ring.height = 2.0f;
    ring.fadeOut = 0.3f;
    ring.pull = 0.8f, ring.softness = 0.3f;
    ring.lightColor = kFrost, ring.lightIntensity = 8.0f, ring.lightRadius = 3.2f, ring.lightDecay = 2.0f;
  }
}

// Cura Menor: a gold ring under the caster while it prays; on the healed unit a ring of light, a pillar rising
// through it and motes on its body, with a warm glow (Engine2 holyDuration: about a second).
void SpellEffects::cure(const Spot& caster, const std::vector<Spot>& targets, float start, float impact) {
  Element& pray = add("holy-v2-secondary-flipbook-4x4", start, impact - start + 0.2f);
  pray.from = pray.to = caster.feet;
  pray.pivot = {0.5f, 0.45f};
  pray.height = 1.7f;
  pray.fadeIn = 0.15f, pray.fadeOut = 0.25f;
  pray.pull = 0.8f, pray.softness = 0.3f;
  pray.lightColor = kHoly, pray.lightIntensity = 3.0f, pray.lightRadius = 2.5f;
  for (const Spot& t : targets) {
    Element& ring = add("holy-v3-secondary-flipbook-4x4", impact - 0.15f, 1.0f);
    ring.from = ring.to = t.feet;
    ring.pivot = {0.5f, 0.45f};
    ring.height = 2.0f;
    ring.fadeOut = 0.3f;
    ring.pull = 0.8f, ring.softness = 0.3f;
    Element& pillar = add("holy-v3-main-flipbook-4x4", impact - 0.05f, 0.94f);
    pillar.from = pillar.to = t.feet;
    pillar.pivot = {0.5f, 0.06f};
    pillar.height = 2.6f;
    pillar.fadeOut = 0.3f;
    pillar.color = glm::vec3(0.85f);
    pillar.pull = 1.3f, pillar.softness = 0.3f;
    pillar.lightColor = kHoly, pillar.lightIntensity = 12.0f, pillar.lightRadius = 3.5f, pillar.lightDecay = 1.0f;
    burst("holy-v3-particles-flipbook-4x4", t.body, impact + 0.1f, 0.8f, 1.6f, glm::vec3(1.1f));
  }
}

// Tiro Longo / Tiro Perfurante: the arrow (solid, lit by the scene) flies along a shallow arch, pointing where it
// goes, and lands on the hit; a piercing shot carries on through the line to the last body it strikes (its targets are
// the line's every hex to the board's edge: the bodies on it are the ones with a card; the user, 2026-10-20: "it doesnt end
// at his target and just bounces off all the way out of the screen").
void SpellEffects::arrow(const Spot& caster, const std::vector<Spot>& targets, float start, float impact, bool piercing) {
  const Spot* firstBody = &targets.front();
  const Spot* lastBody = nullptr;
  for (const Spot& s : targets)
    if (s.cardHeight > 0.0f && glm::length(s.feet - caster.feet) > 0.3f) {
      if (!lastBody) firstBody = &s;
      lastBody = &s;
    }
  const glm::vec3 first = (piercing ? *firstBody : targets.front()).body;
  const glm::vec3 last = piercing ? (lastBody ? lastBody->body : first) : first;
  const float firstDist = glm::length(first - caster.body), lastDist = glm::length(last - caster.body);
  const glm::vec3 dir = firstDist > 1e-3f ? (first - caster.body) / firstDist : glm::vec3(1, 0, 0);
  const float travel = std::clamp(firstDist / 22.0f, 0.12f, 0.4f);
  const float launch = std::max(start, impact - travel);
  const float arrive = launch + (impact - launch) * (firstDist > 1e-3f ? std::max(lastDist / firstDist, 1.0f) : 1.0f);
  Element& a = add("arrow-002", launch, arrive - launch);
  a.facing = Facing::Along;
  a.from = caster.body + dir * 0.3f;
  a.to = last;
  a.moveStart = launch, a.moveEnd = arrive;
  a.arc = lastDist * 0.06f;
  a.pivot = {0.5f, 0.5f};
  a.height = 1.3f;
  a.angle = std::atan2(0.6f, 0.74f);  // the art points from its feathers (lower left) to its head (upper right)
  a.opacity = 1.0f;
  a.lit = true;
  a.pull = 0.3f;
}

// Blade skills (Golpe Duplo, Investida Perfurante): a glint of embers on the swing; the blows' sparks come from
// strike() when each one lands.
void SpellEffects::bladeSkill(const Spot& caster, float start, float impact) {
  burst("ember-v3-particles-flipbook-4x4", caster.body, std::max(start, impact - 0.2f), 0.35f, 1.0f, glm::vec3(0.9f), 0.8f);
}

void SpellEffects::play(const std::string& clipName, const Spot& at, float start) {
  const SpriteClipId id = clip(clipName.c_str());
  if (id == kNoSpriteClip) return;
  const auto has = [&](const char* s) { return clipName.find(s) != std::string::npos; };
  Element& e = add(clipName.c_str(), start, 1.2f);
  e.fadeOut = 0.25f;
  e.pull = 1.2f, e.softness = 0.3f;
  if (has("secondary") || has("webfloor") || has("portal")) {  // rings, the web on the floor, a gate: around the feet
    e.from = e.to = at.feet;
    e.pivot = {0.5f, 0.45f};
    e.height = 2.2f;
  } else if (has("particles") || has("impact") || has("arrow") || has("core-v2") || clipName == "arcane-flipbook-4x4") {
    e.from = e.to = at.body;  // bursts, sparks, an orb, a projectile's art: on the body
    e.pivot = {0.5f, 0.5f};
    e.height = 1.8f;
    e.opacity = has("arrow") ? 1.0f : 0.0f;
    e.lit = has("arrow");
  } else if (has("lightning-core")) {
    e.from = e.to = at.feet;
    e.pivot = {0.5f, 0.03f};
    e.height = 6.5f;
  } else {  // pillars, flames, smoke, cores, references: standing on the feet
    e.from = e.to = at.feet;
    e.pivot = {0.5f, 0.06f};
    e.height = 2.6f;
  }
  // Its light: the element's colour (Engine2's light colours), a moderate glow.
  std::array<float, 3> color{1.0f, 0.9f, 0.75f};
  if (has("arcane") || has("warp")) color = kViolet;
  else if (has("lightning")) color = kBolt;
  else if (has("frost") || has("blizzard")) color = kFrost;
  else if (has("holy")) color = kHoly;
  else if (has("fire") || has("ember")) color = {1.0f, 0.58f, 0.26f};
  else if (has("poison") || has("venom")) color = {0.55f, 1.0f, 0.35f};
  else if (has("shadow")) color = {0.45f, 0.3f, 0.75f};
  if (!has("arrow")) e.lightColor = color, e.lightIntensity = 10.0f, e.lightRadius = 3.5f, e.lightDecay = 1.0f;
}

void SpellEffects::previewElement(VfxElement element, int version, const Spot& at, float start) {
  if (version != 2 && version != 3) return;
  elementPreviews_.clear();
  elementPreviews_.push_back({element, version, at.feet + glm::vec3(0.0f, 0.04f, 0.0f), start});
}

void SpellEffects::strike(const Spot& target, float at, bool crit) {
  static const char* kImpacts[4] = {"impact-1", "impact-2", "impact-3", "impact-4"};
  const glm::vec3 jitter{(random() - 0.5f) * 0.3f, (random() - 0.5f) * 0.3f, 0.0f};
  burst(kImpacts[static_cast<int>(random() * 4.0f) & 3], target.body + jitter, at, 0.24f, crit ? 1.6f : 1.05f, glm::vec3(crit ? 1.6f : 1.25f),
        1.4f);
  if (crit) burst("ember-v3-particles-flipbook-4x4", target.body, at, 0.5f, 1.8f, glm::vec3(1.3f), 1.3f);
  Element& flash = add(nullptr, at, 0.25f);
  flash.from = flash.to = target.body;
  flash.lightColor = kSpark, flash.lightIntensity = crit ? 14.0f : 6.0f, flash.lightRadius = 2.5f, flash.lightDecay = 2.0f;
}

const SpellEffects::Spot& SpellEffects::areaCentre(const std::vector<Spot>& hexes) {
  glm::vec3 mean{0.0f};
  for (const Spot& s : hexes) mean += s.feet;
  mean /= static_cast<float>(std::max<size_t>(hexes.size(), 1));
  const Spot* best = &hexes.front();
  for (const Spot& s : hexes)
    if (glm::length(glm::vec2(s.feet.x - mean.x, s.feet.z - mean.z)) < glm::length(glm::vec2(best->feet.x - mean.x, best->feet.z - mean.z))) best = &s;
  return *best;
}

// The hex an area was aimed at: the rules list the area row by row, so its first hex is a corner of it, not the aim. The aim is
// the hex whose farthest hex of the area is nearest (a disc's centre, also when the board's edge clips it); a 3-hex area (the aim
// and two beside it) lists the aim first. Ties: the one nearest the mean.
glm::vec3 SpellEffects::aimedHex(const std::vector<Spot>& targets) {
  if (targets.size() <= 3) return targets.front().feet + glm::vec3(0.0f, 0.04f, 0.0f);
  const Spot& mean = areaCentre(targets);
  const Spot* best = &mean;
  auto reach = [&](const Spot& c) {
    float far = 0.0f;
    for (const Spot& s : targets) far = std::max(far, glm::length(glm::vec2(s.feet.x - c.feet.x, s.feet.z - c.feet.z)));
    return far;
  };
  float bestReach = reach(mean);
  for (const Spot& s : targets)
    if (const float r = reach(s); r < bestReach - 1e-3f) best = &s, bestReach = r;
  return best->feet + glm::vec3(0.0f, 0.04f, 0.0f);
}

void SpellEffects::webUnits(int id, const std::vector<WebUnit>& units) {
  std::vector<DreamWeb::Occupant> inside;
  for (const WebUnit& u : units) inside.push_back({u.key, u.at.feet, heightOf(u.at)});
  for (DreamWeb& w : dreamWebs_)
    if (w.id == id) w.setOccupants(inside);
}
void SpellEffects::webStand(int id) {
  for (DreamWeb& w : dreamWebs_)
    if (w.id == id) w.stand();
}
void SpellEffects::webEnd(int id, float now) {
  for (DreamWeb& w : dreamWebs_)
    if (w.id == id) w.release(now);
}

void SpellEffects::stormEnd(int id, float now) {
  for (IceStorm& s : storms_)
    if (s.id == id) s.stop(now);
}
void SpellEffects::stormStand(int id) {
  for (IceStorm& s : storms_)
    if (s.id == id) s.stand();
}

void SpellEffects::leapFlight(const Spot& caster, const Spot& landing, float start, float impact, int owner) {
  warrior_.leap(caster.feet, caster.cardHeight, landing.feet, start, impact);
  if (owner >= 0) casts_.push_back({owner, impact + 1.8f});
}
void SpellEffects::warCryFear(const std::vector<Spot>& foes, float impact) {
  std::vector<WarriorFx::Target> hit;
  for (const Spot& s : foes) hit.push_back({s.feet, s.cardHeight});
  warrior_.warCryFear(hit, impact);
}
void SpellEffects::vaultFlight(const Spot& caster, const Spot& over, const Spot& landing, float start, float land, int owner) {
  lancer_.vault(caster.feet, caster.cardHeight, over.feet, landing.feet, start, land);
  if (owner >= 0) casts_.push_back({owner, land + 1.3f});
}
void SpellEffects::lancerFlight(const std::string& kind, const Spot& from, const Spot& to, float arc, float start, float finish, const Spot& caster,
                                const Spot& casterTo, bool collided) {
  if (kind == "spearToss") lancer_.tossFlight(from.feet, to.feet, from.cardHeight, arc, start, finish, collided);
  else if (kind == "hookPull") lancer_.hookPull(caster.feet, caster.cardHeight, from.feet, to.feet, from.cardHeight, start, finish);
  else if (kind == "impaleDrag") lancer_.drag(from.feet, to.feet, from.cardHeight, caster.feet, casterTo.feet, caster.cardHeight, start, finish);
}
void SpellEffects::swapFlight(const Spot& a, const Spot& b, float start, float finish) { lancer_.swap(a.feet, a.cardHeight, b.feet, b.cardHeight, start, finish); }
void SpellEffects::disarmed(const Spot& foe, const Spot& from, float at) { lancer_.disarmed({foe.feet, foe.cardHeight}, from.feet, at); }

bool SpellEffects::active(float now) const {
  if (combat_.active(now) || warrior_.active(now) || arms_.active(now) || ranger_.active(now) || lancer_.active(now) || turnUndead_.active(now) || healer_.active(now) ||
      conjurer_.active(now) || daggers_.active(now) || mage_.active(now) || crusader_.active(now) || selene_.active(now))
    return true;
  if (std::any_of(missiles_.begin(), missiles_.end(), [&](const MagicMissileV2& m) { return !m.done(now); })) return true;
  if (std::any_of(fireballs_.begin(), fireballs_.end(), [&](const Fireball& f) { return !f.done(now); })) return true;
  if (std::any_of(blesses_.begin(), blesses_.end(), [&](const Bless& b) { return !b.done(now); })) return true;
  if (std::any_of(holies_.begin(), holies_.end(), [&](const HolyLight& h) { return !h.done(now); })) return true;
  if (std::any_of(venoms_.begin(), venoms_.end(), [&](const CausticVenom& v) { return !v.done(now); })) return true;
  if (std::any_of(phantasms_.begin(), phantasms_.end(), [&](const PhantasmalForce& p) { return !p.done(now); })) return true;
  if (std::any_of(webs_.begin(), webs_.end(), [&](const WebOfDreams& w) { return !w.done(now); })) return true;
  if (std::any_of(dreamWebs_.begin(), dreamWebs_.end(), [&](const DreamWeb& w) { return !w.done(now); })) return true;
  if (std::any_of(burns_.begin(), burns_.end(), [&](const auto& b) { return !b.first.done(now); })) return true;
  if (std::any_of(cleaves_.begin(), cleaves_.end(), [&](const CleaveSweep& c) { return !c.done(now); })) return true;
  if (std::any_of(sweeps_.begin(), sweeps_.end(), [&](const Varredura& s) { return !s.done(now); })) return true;
  if (std::any_of(storms_.begin(), storms_.end(), [&](const IceStorm& s) { return !s.done(now); })) return true;
  if (std::any_of(bolts_.begin(), bolts_.end(), [&](const SpellBolt& b) { return !b.done(now); })) return true;
  if (std::any_of(lightnings_.begin(), lightnings_.end(), [&](const Lightning& l) { return !l.done(now); })) return true;
  if (std::any_of(frosts_.begin(), frosts_.end(), [&](const FrostLine& f) { return !f.done(now); })) return true;
  return std::any_of(elements_.begin(), elements_.end(), [&](const Element& e) { return e.end > now; });
}

void SpellEffects::build(float now, const Camera& camera, bool flat, float flatTop, std::vector<EffectInstance>& out,
                         std::vector<Light>& lights) {
  std::erase_if(elements_, [&](const Element& e) { return e.end <= now; });
  // Engine2's ported effects: their own geometry and lights.
  std::erase_if(missiles_, [&](const MagicMissileV2& m) { return m.done(now); });
  std::erase_if(fireballs_, [&](const Fireball& f) { return f.done(now); });
  std::erase_if(blesses_, [&](const Bless& b) { return b.done(now); });
  std::erase_if(holies_, [&](const HolyLight& h) { return h.done(now); });
  vfx_.clear();
  std::vector<VfxLight> vfxLights;
  for (const ElementPreview& preview : elementPreviews_)
    elementV2(preview.element, preview.version).build(preview.at, now - preview.start, 1.0f, vfx_);
  for (Bless& b : blesses_) b.build(now, camera, vfx_, vfxLights);
  for (HolyLight& h : holies_) h.build(now, camera, vfx_, vfxLights);
  std::erase_if(venoms_, [&](const CausticVenom& v) { return v.done(now); });
  std::erase_if(phantasms_, [&](const PhantasmalForce& p) { return p.done(now); });
  for (PhantasmalForce& p : phantasms_) p.build(now, camera, vfx_, vfxLights);
  std::erase_if(webs_, [&](const WebOfDreams& w) { return w.done(now); });
  for (WebOfDreams& w : webs_) w.build(now, camera, vfx_, vfxLights);
  std::erase_if(dreamWebs_, [&](const DreamWeb& w) { return w.done(now); });
  for (DreamWeb& w : dreamWebs_) w.build(now, camera, vfx_, vfxLights);
  std::erase_if(cleaves_, [&](const CleaveSweep& c) { return c.done(now); });
  for (CleaveSweep& c : cleaves_) c.build(now, camera, vfx_, vfxLights);
  std::erase_if(sweeps_, [&](const Varredura& s) { return s.done(now); });
  for (Varredura& s : sweeps_) s.build(now, camera, vfx_, vfxLights);
  std::erase_if(burns_, [&](const auto& b) { return b.first.done(now); });
  if (!burns_.empty()) {
    const VfxFlipbook fireSheet = flipbook("fire-flipbook-4x4"), poisonSheet = flipbook("poison-v2-main-flipbook-4x4");
    for (auto& [burn, poison] : burns_) burn.build(now, camera, poison ? poisonSheet : fireSheet, vfx_, vfxLights);
  }
  if (!venoms_.empty()) {
    const ElementSpritesV2 poison = elementV2(VfxElement::Poison);
    for (CausticVenom& v : venoms_) v.build(now, camera, poison, vfx_, vfxLights);
  }
  std::erase_if(acidClouds_, [&](const AcidCloud& cloud) { return cloud.done(now); });
  for (const AcidCloud& cloud : acidClouds_) cloud.build(now, camera, vfx_, vfxLights);
  for (MagicMissileV2& m : missiles_) m.build(now, camera, vfx_, vfxLights);
  if (!fireballs_.empty()) {
    // The impact's fire flipbook (Engine2 loadFireFlipbook: fx/fire-flipbook-4x4.png), from the battle pack.
    const VfxFlipbook fire = flipbook("fire-flipbook-4x4");
    for (Fireball& f : fireballs_) f.build(now, camera, fire, vfx_, vfxLights);
  }
  std::erase_if(bolts_, [&](const SpellBolt& b) { return b.done(now); });
  for (const SpellBolt& b : bolts_) b.build(now, camera, vfx_, vfxLights);
  std::erase_if(lightnings_, [&](const Lightning& l) { return l.done(now); });
  for (const Lightning& l : lightnings_) l.build(now, camera, vfx_, vfxLights);
  std::erase_if(frosts_, [&](const FrostLine& f) { return f.done(now); });
  for (const FrostLine& f : frosts_) f.build(now, vfx_, vfxLights);
  std::erase_if(storms_, [&](const IceStorm& s) { return s.done(now); });
  if (!storms_.empty()) {
    // Engine2's two sheets, only when a storm of version 01 is on the board.
    const bool engine2 = std::any_of(storms_.begin(), storms_.end(), [](const IceStorm& s) { return s.version() == 1; });
    const VfxFlipbook ice = engine2 ? flipbook("blizzard-impact-reference") : VfxFlipbook{};
    const VfxFlipbook wind = engine2 ? flipbook("blizzard-wind-reference") : VfxFlipbook{};
    for (IceStorm& s : storms_) s.build(now, camera, ice, wind, vfx_, vfxLights);
  }
  combat_.build(now, camera, vfx_, vfxLights);
  warrior_.build(now, camera, vfx_, vfxLights);
  arms_.build(now, camera, vfx_, vfxLights);
  ranger_.build(now, camera, vfx_, vfxLights);
  lancer_.build(now, camera, vfx_, vfxLights);
  turnUndead_.build(now, camera, vfx_, vfxLights);
  healer_.build(now, camera, vfx_, vfxLights);
  crusader_.build(now, camera, vfx_, vfxLights);
  selene_.build(now, camera, vfx_, vfxLights);
  conjurer_.build(now, camera, vfx_, vfxLights);
  status_.build(now, camera, vfx_, vfxLights);
  daggers_.build(now, camera, vfx_, vfxLights);
  mage_.build(now, camera, vfx_, vfxLights);
  for (const VfxLight& l : vfxLights)
    if (l.intensity > 0.0f) lights.push_back({l.at, {l.color.r, l.color.g, l.color.b}, l.intensity, l.distance, std::max(l.at.y, 1.0f)});
  const size_t first = out.size();
  const glm::vec3 eye = camera.position, forward = camera.forward;
  glm::vec3 cameraRight{camera.right.x, 0.0f, camera.right.z};
  cameraRight = glm::dot(cameraRight, cameraRight) > 1e-8f ? glm::normalize(cameraRight) : glm::vec3(1, 0, 0);
  for (const Element& e : elements_) {
    if (now < e.start) continue;
    const float life = e.end - e.start, age = std::clamp((now - e.start) / life, 0.0f, 1.0f);
    float envelope = 1.0f;
    if (e.fadeIn > 0.0f) envelope *= smooth(std::clamp((now - e.start) / e.fadeIn, 0.0f, 1.0f));
    if (e.fadeOut > 0.0f) envelope *= smooth(std::clamp((e.end - now) / e.fadeOut, 0.0f, 1.0f));
    glm::vec3 at = position(e, now);
    if (e.lightIntensity > 0.0f) {
      const float decay = e.lightDecay > 0.0f ? std::pow(1.0f - age, e.lightDecay) : 1.0f;
      float waver = 1.0f;
      if (e.flickerLight > 0.0f) {  // flames: two slow sines and a fast one, never below 1 - share
        const float t = now * 7.3f + e.start * 13.0f;
        waver = 1.0f - e.flickerLight * (0.5f + 0.25f * std::sin(t) + 0.15f * std::sin(t * 2.7f) + 0.1f * std::sin(t * 6.1f));
      }
      lights.push_back({at, e.lightColor, e.lightIntensity * envelope * decay * waver, e.lightRadius, std::max(at.y, 1.0f)});
    }
    SpriteClipId id = e.clip;
    if (!e.flicker.empty()) id = e.flicker[static_cast<size_t>((now - e.start) / kFlickerStep) % e.flicker.size()];
    if (id == kNoSpriteClip) continue;  // a light alone (add(nullptr, ...)), or art missing from the pack
    sprites_.request(id, SpriteSystem::kOnScreen);
    const SpriteSystem::Texture tex = sprites_.texture(id);
    if (tex.index == kNoTexture) {
      ++notResident_;
      continue;
    }
    ++drawn_;
    const SpriteClipHeader& c = sprites_.header(id);

    // The flipbook frame: looping at its rate, or once over the element's life.
    const float frames = static_cast<float>(c.frames);
    float f = e.fps > 0.0f ? std::fmod((now - e.start) * e.fps, frames) : std::min(age * frames, frames - 1.0f);
    const uint32_t layer = std::min(static_cast<uint32_t>(f), c.frames - 1);
    const uint32_t next = e.fps > 0.0f ? (layer + 1) % c.frames : std::min(layer + 1, c.frames - 1);

    // The quad's axes: standing turned to the camera, along the flight on screen, or face up in the 2D view.
    const float h = e.endHeight >= 0.0f ? glm::mix(e.height, e.endHeight, age) : e.height;
    const float sw = static_cast<float>(c.sourceWidth), sh = static_cast<float>(c.sourceHeight);
    const float w = h * sw / sh;
    glm::vec3 right = cameraRight, up{0.0f, 1.0f, 0.0f};
    if (e.facing == Facing::Along) {
      // The flight's direction on screen; the art's own axis (angle) turned onto it.
      const float dt = 0.01f;
      glm::vec3 v = position(e, std::min(now + dt, e.moveEnd)) - position(e, std::min(now + dt, e.moveEnd) - dt);
      v -= forward * glm::dot(v, forward);
      const glm::vec3 d = glm::dot(v, v) > 1e-10f ? glm::normalize(v) : camera.right;
      const glm::vec3 p = glm::normalize(glm::cross(d, forward));
      const float ca = std::cos(e.angle), sa = std::sin(e.angle);
      right = ca * d - sa * p;
      up = sa * d + ca * p;
    }
    if (flat) {
      right = {1.0f, 0.0f, 0.0f};
      up = {0.0f, 0.0f, -1.0f};
      at.y = flatTop;
    }
    const float cu = (static_cast<float>(c.cropX) + c.contentWidth * 0.5f) / sw;
    const float cv = 1.0f - (static_cast<float>(c.cropY) + c.contentHeight * 0.5f) / sh;
    const float side = e.mirror ? -1.0f : 1.0f;
    glm::vec3 center = at + right * ((cu - e.pivot.x) * w * side) + up * ((cv - e.pivot.y) * h);
    glm::vec3 axisX = right * (0.5f * w * c.contentWidth / sw), axisY = up * (0.5f * h * c.contentHeight / sh);
    if (e.pull > 0.0f && !flat) {
      // Nearer the camera along its line of sight, shrunk by as much: the same pixels, a nearer depth (so a burst on
      // a unit is drawn over the unit's card instead of cut by it).
      const glm::vec3 toCenter = center - eye;
      const float dist = glm::length(toCenter);
      const float k = 1.0f - std::min(e.pull / std::max(dist, 1e-3f), 0.9f);
      center = eye + toCenter * k;
      axisX *= k;
      axisY *= k;
    }
    EffectInstance q{};
    q.center = center;
    q.softness = e.softness;
    q.axisX = axisX;
    q.textureIndex = tex.index;
    q.axisY = axisY;
    q.layer = layer;
    q.color = e.lit ? e.color : e.color * envelope;
    q.opacity = e.opacity * (e.lit ? 1.0f : envelope);
    q.uvScale = {static_cast<float>(c.contentWidth) / c.width, static_cast<float>(c.contentHeight) / c.height};
    q.nextLayer = next;
    q.blend = f - std::floor(f);
    q.sightAt = at;
    q.flags = (e.mirror ? kEffectMirrored : 0u) | (e.lit ? kEffectLit : 0u);
    out.push_back(q);
  }
  std::sort(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(), [&](const EffectInstance& a, const EffectInstance& b) {
    return glm::dot(a.center - eye, forward) > glm::dot(b.center - eye, forward);
  });
  // Once a second while anything is alive and some card could not be drawn (its texture not resident yet: the sprite
  // budget starving the effect clips), say so: an invisible spell is otherwise silent.
  if (!elements_.empty() && now - lastReport_ >= 1.0f) {
    lastReport_ = now;
    if (notResident_ > 0) logWarn("effects: {} cards drawn, {} waiting for their texture ({} elements alive)", drawn_, notResident_, elements_.size());
    drawn_ = notResident_ = 0;
  }
}

}  // namespace ember
