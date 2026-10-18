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
  // 03: ours in 3D (vfx/SpellBolts; the user, 2026-10-18: "the current ones are too bland").
  if (kind == "magicMissile" || kind == "magicMissileV2") return 3;
  if (kind == "divineBolt") return 1;  // 01: ours in 3D (vfx/SpellBolts)
  // 01 Engine2's BurningHandsV2VFX as it was, 02 its cone opening outward (ours).
  if (kind == "burningHands" || kind == "poisonBreath") return 2;
  // Tempestade de Gelo: 01 Engine2's Blizzard2D (the enemies'), 02 ours in 3D (the heroes'): vfx/IceStorm.
  if (kind == "iceStorm") return 2;
  // 01 of a spell with no flipbook-card version: Engine2's own effect, ported.
  if (kind == "fireball" || kind == "bless" || kind == "causticVenom" || kind == "phantasmalForce" ||
      kind == "webOfDreams" || kind == "burningHands" || kind == "poisonBreath" || kind == "cleave" || kind == "sweep")
    return 1;
  static const char* kVersion01[] = {"lightning", "lightningTier3", "shock", "frost", "cureMinor", "longShot", "piercing", "piercingThrust",
                                     "doubleStrike"};
  for (const char* k : kVersion01)
    if (kind == k) return 1;
  // EmberEngine's own spells (SpellEffectsEmber.cpp): version 01, flipbook cards.
  static const char* kEmber[] = {"markOfRuin",   "creepingPlague", "lifeDrain",    "shroudOfShadows", "hemorrhage",     "profaneSilence",  "blackPestilence",
                                 "curseOfDecay", "soulHarvest",    "devouringVoid", "pillarOfFlame",  "iceLance",       "thunderstorm",    "meteor",
                                 "absoluteZero", "chainLightning", "eruption",      "glacialGale",     "wrathOfTheSkies", "primordialEmber",
                                 // the Guerreiro's (vfx/WarriorFx, ours, 3D)
                                 "steelWhirlwind", "guardBreak", "warCry", "leapingStrike",
                                 // the Arqueiro's (vfx/RangerFx, ours, 3D)
                                 "venomArrow", "arrowRain", "explosiveArrow", "cripplingShot", "arrowStorm", "deathShot"};
  for (const char* k : kEmber)
    if (kind == k) return 1;
  return 0;
}

int SpellEffects::engine2Look(const std::string& kind) {
  if (kind == "cureMinor") return 2;  // HolyLight (Engine2's own was 2D)
  if (kind == "cureWounds" || kind == "cureLight" || kind == "cureDisease" || kind == "createFoodAndWater" || kind == "potion") return 1;
  if (kind == "magicMissile" || kind == "magicMissileV2") return 3;  // ours in 3D (SpellBolts; 02 = Engine2's MagicMissileV2VFX)
  if (kind == "divineBolt") return 1;                                // ours in 3D (SpellBolts)
  if (kind == "fireball") return 1;                                  // FireballVFX
  if (kind == "bless") return 1;                                     // BlessVFX
  if (kind == "causticVenom") return 1;                              // CausticVenomVFX
  if (kind == "phantasmalForce") return 1;                           // PhantasmalForceVFX
  if (kind == "webOfDreams") return 1;                               // WebOfDreamsVFX
  if (kind == "burningHands" || kind == "poisonBreath") return 2;    // BurningHandsV2VFX, its cone opening outward (02)
  if (kind == "cleave") return 1;                                    // CleaveSweepVFX
  if (kind == "sweep") return 1;                                     // VarreduraVFX
  if (kind == "iceStorm") return 2;  // ours in 3D (Engine2's Blizzard2D, 01, is the enemies': content/fx/spell-looks.json)
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

ElementSpritesV2 SpellEffects::elementV2(VfxElement element) {
  ElementSpritesV2 e = ElementSpritesV2::defaults(element);
  const std::string name = vfxElementName(element);
  e.main = flipbook((name + "-v2-main-flipbook-4x4").c_str());
  e.secondary = flipbook((name + "-v2-secondary-flipbook-4x4").c_str());
  e.particles = flipbook((name + "-v2-particles-flipbook-4x4").c_str());
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
  if (kind == "webOfDreams" && look == 1) return WebOfDreams::kShotTravel;
  if ((kind == "burningHands" || kind == "poisonBreath") && look >= 1) return BurningHands::kRelease;
  if (isBolt(kind, look) && !targets.empty()) {
    const SpellBolt::Style style = kind == "divineBolt" ? SpellBolt::Style::Divine : SpellBolt::Style::Arcane;
    return SpellBolt::flightSeconds(style, caster.feet, (kind == "divineBolt" ? areaCentre(targets) : targets.front()).feet);
  }
  if (kind == "causticVenom" && look == 1 && !targets.empty())
    return CausticVenom::kCharge + CausticVenom::travelSeconds(engine2Origin(caster), fireballTarget(targets));
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
                           webs_.size(),     burns_.size(),    cleaves_.size(),   sweeps_.size(),  holies_.size()};
  const bool made = castEffect(kind, look, caster, targets, start, impact);
  if (made && owner >= 0) {
    float end = start;
    for (size_t i = before[0]; i < elements_.size(); ++i) end = std::max(end, elements_[i].end);
    auto latest = [&](const auto& list, size_t from) {
      for (size_t i = from; i < list.size(); ++i) end = std::max(end, list[i].end());
    };
    latest(missiles_, before[1]), latest(fireballs_, before[2]), latest(blesses_, before[3]), latest(venoms_, before[4]);
    latest(phantasms_, before[5]), latest(webs_, before[6]), latest(cleaves_, before[8]), latest(sweeps_, before[9]);
    latest(holies_, before[10]);
    for (size_t i = before[7]; i < burns_.size(); ++i) end = std::max(end, burns_[i].first.end());
    if (kind == "iceStorm" && !storms_.empty()) end = std::max(end, storms_.back().start() + 1.0f);  // its first second, not the whole storm
    if (isBolt(kind, look) && !bolts_.empty()) end = std::max(end, bolts_.back().end());
    std::erase_if(casts_, [&](const Cast& c) { return c.end <= start; });
    casts_.push_back({owner, end});
  }
  return made;
}

bool SpellEffects::castEffect(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets, float start, float impact) {
  if (look < 1 || look > looks(kind)) return false;
  impact = std::max(impact, start + 0.05f);
  if (targets.empty() && kind != "piercingThrust" && kind != "doubleStrike" && kind != "steelWhirlwind" && kind != "warCry") return false;
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
  // The Arqueiro's skills (RangerFx, ours, 3D): one arrow at the struck foe (the first target; the burst's neighbours follow it),
  // or a volley over every hex of the area (the bodies in it flash).
  if (kind == "venomArrow" || kind == "explosiveArrow" || kind == "cripplingShot" || kind == "deathShot") {
    const RangerFx::Shot style = kind == "venomArrow" ? RangerFx::Shot::Venom : kind == "explosiveArrow" ? RangerFx::Shot::Explosive
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
  if (isBolt(kind, look)) {
    // Ours, in 3D (vfx/SpellBolts): one look, the same on every cast (fixed seeds).
    if (kind == "divineBolt") {
      const Spot& centre = areaCentre(targets);
      std::vector<glm::vec3> splash;
      for (const Spot& s : targets) splash.push_back(s.feet);
      bolts_.emplace_back(SpellBolt::Style::Divine, caster.feet, heightOf(caster), centre.feet, heightOf(centre), std::move(splash), start, impact, 0xd1b1e7u);
    } else {
      bolts_.emplace_back(SpellBolt::Style::Arcane, caster.feet, heightOf(caster), targets.front().feet, heightOf(targets.front()), std::vector<glm::vec3>{},
                          start, impact, 0xa11ce5u);
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
    else cleaves_.emplace_back(caster.feet + glm::vec3(0.0f, height, 0.0f), hit, start);
  } else if (kind == "burningHands" || kind == "poisonBreath") {
    std::vector<glm::vec3> cells;
    for (const Spot& s : targets) cells.push_back(s.feet + glm::vec3(0.0f, 0.04f, 0.0f));
    const bool poison = kind == "poisonBreath";
    burns_.emplace_back(BurningHands(engine2Origin(caster), cells, poison, start, look), poison);
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
  } else if (kind == "causticVenom") {
    std::vector<glm::vec3> hexes;
    for (const Spot& s : targets) hexes.push_back(s.feet + glm::vec3(0.0f, 0.04f, 0.0f));
    venoms_.emplace_back(engine2Origin(caster), fireballTarget(targets), hexes, start);
  } else if (kind == "bless") blesses_.emplace_back(caster.feet + glm::vec3(0.0f, 0.04f, 0.0f), 3.0f * std::sqrt(3.0f), blessAllies(caster, targets), start);
  else if (kind == "magicMissile" || kind == "magicMissileV2") magicMissile(caster, targets.front(), start, impact);
  else if (kind == "lightning" || kind == "lightningTier3") lightning(caster, targets, start, impact, false);
  else if (kind == "shock") lightning(caster, targets, start, impact, true);
  else if (kind == "frost") frost(caster, targets, start, impact);
  else if (kind == "cureMinor") cure(caster, targets, start, impact);
  else if (kind == "longShot") arrow(caster, targets, start, impact, false);
  else if (kind == "piercing") arrow(caster, targets, start, impact, true);
  else if (kind == "piercingThrust" || kind == "doubleStrike") bladeSkill(caster, start, impact);
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
// goes, and lands on the hit; a piercing shot carries on through the line to its last target.
void SpellEffects::arrow(const Spot& caster, const std::vector<Spot>& targets, float start, float impact, bool piercing) {
  const glm::vec3 first = targets.front().body;
  const glm::vec3 last = piercing ? targets.back().body : first;
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

void SpellEffects::stormEnd(int id, float now) {
  for (IceStorm& s : storms_)
    if (s.id == id) s.stop(now);
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

bool SpellEffects::active(float now) const {
  if (combat_.active(now) || warrior_.active(now) || ranger_.active(now)) return true;
  if (std::any_of(missiles_.begin(), missiles_.end(), [&](const MagicMissileV2& m) { return !m.done(now); })) return true;
  if (std::any_of(fireballs_.begin(), fireballs_.end(), [&](const Fireball& f) { return !f.done(now); })) return true;
  if (std::any_of(blesses_.begin(), blesses_.end(), [&](const Bless& b) { return !b.done(now); })) return true;
  if (std::any_of(holies_.begin(), holies_.end(), [&](const HolyLight& h) { return !h.done(now); })) return true;
  if (std::any_of(venoms_.begin(), venoms_.end(), [&](const CausticVenom& v) { return !v.done(now); })) return true;
  if (std::any_of(phantasms_.begin(), phantasms_.end(), [&](const PhantasmalForce& p) { return !p.done(now); })) return true;
  if (std::any_of(webs_.begin(), webs_.end(), [&](const WebOfDreams& w) { return !w.done(now); })) return true;
  if (std::any_of(burns_.begin(), burns_.end(), [&](const auto& b) { return !b.first.done(now); })) return true;
  if (std::any_of(cleaves_.begin(), cleaves_.end(), [&](const CleaveSweep& c) { return !c.done(now); })) return true;
  if (std::any_of(sweeps_.begin(), sweeps_.end(), [&](const Varredura& s) { return !s.done(now); })) return true;
  if (std::any_of(storms_.begin(), storms_.end(), [&](const IceStorm& s) { return !s.done(now); })) return true;
  if (std::any_of(bolts_.begin(), bolts_.end(), [&](const SpellBolt& b) { return !b.done(now); })) return true;
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
  for (Bless& b : blesses_) b.build(now, camera, vfx_, vfxLights);
  for (HolyLight& h : holies_) h.build(now, camera, vfx_, vfxLights);
  std::erase_if(venoms_, [&](const CausticVenom& v) { return v.done(now); });
  std::erase_if(phantasms_, [&](const PhantasmalForce& p) { return p.done(now); });
  for (PhantasmalForce& p : phantasms_) p.build(now, camera, vfx_, vfxLights);
  std::erase_if(webs_, [&](const WebOfDreams& w) { return w.done(now); });
  for (WebOfDreams& w : webs_) w.build(now, camera, vfx_, vfxLights);
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
  for (MagicMissileV2& m : missiles_) m.build(now, camera, vfx_, vfxLights);
  if (!fireballs_.empty()) {
    // The impact's fire flipbook (Engine2 loadFireFlipbook: fx/fire-flipbook-4x4.png), from the battle pack.
    const VfxFlipbook fire = flipbook("fire-flipbook-4x4");
    for (Fireball& f : fireballs_) f.build(now, camera, fire, vfx_, vfxLights);
  }
  std::erase_if(bolts_, [&](const SpellBolt& b) { return b.done(now); });
  for (const SpellBolt& b : bolts_) b.build(now, camera, vfx_, vfxLights);
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
  ranger_.build(now, camera, vfx_, vfxLights);
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
