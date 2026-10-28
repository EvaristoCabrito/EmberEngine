// EmberEngine's own spells' visuals (the Bruxo's and the Elementalista's T6-T10, world/Spells.cpp kEmberSpells), version
// 01: flipbook cards from Ember's fx sheets (shadow, poison, fire, frost, lightning, ember, warp, holy) with lights of
// their own, laid out on the stage's clock so the hit lands on `impact` (the pose's), plus the zones that stay on the
// board (burning ground, the Mortalha de Sombras, a Meteoro's mark) and the Meteoro's fall. The look is ours; the art is
// Ember's (content/fx/effects-list.md). Numbers and timings here are visuals only: the rules decide what happens.

#include "render/SpellEffects.h"

#include <algorithm>
#include <cmath>

namespace ember {

namespace {

constexpr std::array<float, 3> kShadow{0.52f, 0.30f, 0.90f};  // Escuridão: violet-black
constexpr std::array<float, 3> kBlood{1.0f, 0.22f, 0.14f};
constexpr std::array<float, 3> kPoison{0.55f, 1.0f, 0.35f};
constexpr std::array<float, 3> kFire{1.0f, 0.58f, 0.26f};
constexpr std::array<float, 3> kEmberGlow{1.0f, 0.42f, 0.12f};
constexpr std::array<float, 3> kIce{0.71f, 0.91f, 1.0f};
constexpr std::array<float, 3> kStorm{0.72f, 0.86f, 1.0f};
constexpr std::array<float, 3> kSoul{0.85f, 0.95f, 1.0f};

glm::vec3 tint(const std::array<float, 3>& c, float k = 1.0f) { return {c[0] * k, c[1] * k, c[2] * k}; }
glm::vec3 mean(const std::vector<SpellEffects::Spot>& spots) {
  glm::vec3 m{0.0f};
  for (const SpellEffects::Spot& s : spots) m += s.feet;
  return spots.empty() ? m : m / static_cast<float>(spots.size());
}
float hexDistance(const glm::vec3& a, const glm::vec3& b) { return glm::length(glm::vec2(a.x - b.x, a.z - b.z)) / 1.7320508f; }

}  // namespace

// --- Shared pieces ---------------------------------------------------------------------------------------------------------

void SpellEffects::stream(const char* clipName, glm::vec3 from, glm::vec3 to, float start, float duration, int count, glm::vec3 color, float size) {
  for (int i = 0; i < count; ++i) {
    const float t0 = start + duration * 0.6f * random();
    const float flight = duration * (0.35f + 0.25f * random());
    Element& s = add(clipName, t0, flight + 0.15f);
    const glm::vec3 jitter{(random() - 0.5f) * 0.5f, (random() - 0.5f) * 0.4f, (random() - 0.5f) * 0.5f};
    s.from = from + jitter, s.to = to;
    s.moveStart = t0, s.moveEnd = t0 + flight;
    s.arc = 0.3f + random() * 0.5f;
    s.pivot = {0.5f, 0.5f};
    s.height = size, s.endHeight = size * 0.35f;
    s.fadeIn = 0.05f, s.fadeOut = 0.15f;
    s.color = color;
    s.pull = 0.4f;
    s.mirror = random() < 0.5f;
  }
}

int SpellEffects::zoneStart(const std::string& kind, const std::string& element, const std::vector<Spot>& hexes, float now) {
  if (hexes.empty()) return 0;
  const int id = ++zoneSerial_;
  constexpr float kForever = 1.0e9f;
  const glm::vec3 centre = hexes.front().feet;
  if (kind == "acidCloud") {
    std::vector<glm::vec3> cells;
    cells.reserve(hexes.size());
    for (const Spot& h : hexes) cells.push_back(h.feet);
    acidClouds_.emplace_back(std::move(cells), now);
    acidClouds_.back().id = id;
  } else if (kind == "burning") {
    // Small flames on every hex, each at its own phase, the ground glowing; one wavering light over the whole patch.
    const bool ember = element == "ember";
    for (const Spot& h : hexes) {
      Element& flame = add(ember ? "ember-flipbook-4x4" : "fire-flipbook-4x4", now - random() * 0.5f, kForever);
      flame.zone = id;
      flame.from = flame.to = h.feet + glm::vec3((random() - 0.5f) * 0.3f, 0.0f, (random() - 0.5f) * 0.3f);
      flame.pivot = {0.5f, 0.08f};
      flame.height = ember ? 0.8f : 1.0f + random() * 0.3f;
      flame.fps = 14.0f + random() * 4.0f;
      flame.fadeIn = 0.3f, flame.fadeOut = 0.6f;
      flame.color = ember ? glm::vec3(1.3f, 0.9f, 0.7f) : glm::vec3(1.1f);
      flame.pull = 0.6f, flame.softness = 0.25f;
      flame.mirror = random() < 0.5f;
      Element& glow = add(ember ? "ember-v3-secondary-flipbook-4x4" : "fire-v3-secondary-flipbook-4x4", now, kForever);
      glow.zone = id;
      glow.from = glow.to = h.feet;
      glow.pivot = {0.5f, 0.45f};
      glow.height = 1.6f;
      glow.fps = 10.0f;
      glow.fadeIn = 0.4f, glow.fadeOut = 0.6f;
      glow.color = glm::vec3(0.55f);
      glow.pull = 0.5f, glow.softness = 0.3f;
    }
    Element& light = add(nullptr, now, kForever);
    light.zone = id;
    light.from = light.to = mean(hexes) + glm::vec3(0.0f, 1.2f, 0.0f);
    light.fadeIn = 0.4f, light.fadeOut = 0.6f;
    light.lightColor = ember ? kEmberGlow : kFire;
    light.lightIntensity = 6.0f + 1.2f * static_cast<float>(std::min<size_t>(hexes.size(), 37));
    light.lightRadius = 3.0f + 0.9f * std::sqrt(static_cast<float>(hexes.size()));
    light.flickerLight = 0.35f;
  } else if (kind == "shroudOfShadows") {
    // A shroud of smoke over the area: dark smoke columns on every hex, drifting rings on the ground, a dim violet light.
    for (const Spot& h : hexes) {
      Element& smoke = add("shadow-v2-main-flipbook-4x4", now - random() * 0.8f, kForever);
      smoke.zone = id;
      smoke.from = smoke.to = h.feet + glm::vec3((random() - 0.5f) * 0.4f, 0.0f, (random() - 0.5f) * 0.4f);
      smoke.pivot = {0.5f, 0.08f};
      smoke.height = 2.2f + random() * 0.6f;
      smoke.fps = 7.0f + random() * 3.0f;
      smoke.fadeIn = 0.6f, smoke.fadeOut = 0.9f;
      smoke.color = glm::vec3(0.35f, 0.28f, 0.5f);
      smoke.opacity = 0.55f;
      smoke.pull = 0.5f, smoke.softness = 0.35f;
      smoke.mirror = random() < 0.5f;
      Element& ring = add("shadow-v3-secondary-flipbook-4x4", now, kForever);
      ring.zone = id;
      ring.from = ring.to = h.feet;
      ring.pivot = {0.5f, 0.45f};
      ring.height = 1.8f;
      ring.fps = 6.0f;
      ring.fadeIn = 0.6f, ring.fadeOut = 0.9f;
      ring.color = glm::vec3(0.6f, 0.45f, 0.9f) * 0.7f;
      ring.pull = 0.5f, ring.softness = 0.3f;
    }
    Element& light = add(nullptr, now, kForever);
    light.zone = id;
    light.from = light.to = centre + glm::vec3(0.0f, 1.5f, 0.0f);
    light.fadeIn = 0.6f, light.fadeOut = 0.9f;
    light.lightColor = kShadow;
    light.lightIntensity = 5.0f;
    light.lightRadius = 2.5f + 1.2f * std::sqrt(static_cast<float>(hexes.size()));
  } else if (kind == "meteor") {
    // The mark where it will fall: a red ring pulsing on the centre, faint embers over the area.
    Element& mark = add("fire-v3-secondary-flipbook-4x4", now, kForever);
    mark.zone = id;
    mark.from = mark.to = centre;
    mark.pivot = {0.5f, 0.45f};
    mark.height = 2.4f;
    mark.fps = 12.0f;
    mark.fadeIn = 0.3f, mark.fadeOut = 0.3f;
    mark.color = glm::vec3(1.4f, 0.35f, 0.2f);
    mark.pull = 0.6f, mark.softness = 0.3f;
    mark.lightColor = {1.0f, 0.3f, 0.15f}, mark.lightIntensity = 5.0f, mark.lightRadius = 4.0f, mark.flickerLight = 0.5f;
    for (const Spot& h : hexes) {
      if (&h == &hexes.front()) continue;
      Element& e = add("ember-particles-flipbook-4x4", now - random(), kForever);
      e.zone = id;
      e.from = e.to = h.feet + glm::vec3(0.0f, 0.3f, 0.0f);
      e.pivot = {0.5f, 0.5f};
      e.height = 0.7f;
      e.fps = 8.0f;
      e.fadeIn = 0.4f, e.fadeOut = 0.3f;
      e.color = glm::vec3(1.2f, 0.5f, 0.3f) * 0.6f;
      e.pull = 0.4f;
    }
  }
  return id;
}

void SpellEffects::zoneEnd(int id, float now) {
  for (AcidCloud& cloud : acidClouds_)
    if (cloud.id == id) cloud.stop(now);
  for (Element& e : elements_)
    if (e.zone == id && e.end > now) e.end = now + std::max(e.fadeOut, 0.3f), e.zone = 0;
}

// --- Bruxo -------------------------------------------------------------------------------------------------------------

bool SpellEffects::emberCast(const std::string& kind, const Spot& caster, const std::vector<Spot>& targets, float start, float impact) {
  if (targets.empty()) return false;
  const Spot& first = targets.front();
  if (kind == "markOfRuin") markOfRuin(caster, first, start, impact);
  else if (kind == "creepingPlague") creepingPlague(caster, first, start, impact);
  else if (kind == "lifeDrain") lifeDrain(caster, first, start, impact);
  else if (kind == "shroudOfShadows") shroudCast(caster, targets, start, impact);
  else if (kind == "hemorrhage") hemorrhage(caster, first, start, impact);
  else if (kind == "profaneSilence") profaneSilence(caster, targets, start, impact);
  else if (kind == "blackPestilence") blackPestilence(caster, targets, start, impact);
  else if (kind == "curseOfDecay") curseOfDecay(caster, first, start, impact);
  else if (kind == "soulHarvest") soulHarvest(caster, targets, start, impact);
  else if (kind == "devouringVoid") devouringVoid(caster, first, start, impact);
  else if (kind == "pillarOfFlame") pillarOfFlame(caster, targets, start, impact);
  else if (kind == "iceLance") iceLance(caster, targets, start, impact);
  else if (kind == "thunderstorm") thunderstorm(caster, targets, start, impact);
  else if (kind == "meteor") meteorMark(caster, targets, start, impact);
  else if (kind == "absoluteZero") absoluteZero(caster, targets, start, impact);
  else if (kind == "chainLightning") chainLightning(caster, targets, start, impact);
  else if (kind == "eruption") eruption(caster, targets, start, impact);
  else if (kind == "glacialGale") glacialGale(caster, targets, start, impact);
  else if (kind == "wrathOfTheSkies") wrathOfTheSkies(caster, targets, start, impact);
  else if (kind == "primordialEmber") primordialEmber(caster, targets, start, impact);
  else return false;
  return true;
}

// Shadow gathers at the caster's hand, a dark bolt flies, and a violet seal closes over the chest and keeps turning,
// faint, while the mark lasts (a few seconds on screen).
void SpellEffects::markOfRuin(const Spot& caster, const Spot& target, float start, float impact) {
  const glm::vec3 d = target.body - caster.body;
  const float dist = glm::length(d);
  const glm::vec3 dir = dist > 1e-3f ? d / dist : glm::vec3(1, 0, 0);
  const float travel = std::clamp(dist / 16.0f, 0.2f, 0.5f);
  const float launch = std::max(start + 0.05f, impact - travel);
  burst("shadow-v3-particles-flipbook-4x4", caster.body + dir * 0.3f, start, launch - start + 0.2f, 1.1f, tint(kShadow, 1.3f), 0.6f);
  Element& bolt = add("shadow-flipbook-4x4", launch, impact - launch);
  bolt.from = caster.body + dir * 0.4f, bolt.to = target.body;
  bolt.moveStart = launch, bolt.moveEnd = impact;
  bolt.arc = std::min(dist * 0.08f, 0.8f);
  bolt.pivot = {0.5f, 0.5f};
  bolt.height = 1.0f;
  bolt.fps = 20.0f;
  bolt.color = glm::vec3(0.9f, 0.6f, 1.3f);
  bolt.opacity = 0.35f;
  bolt.pull = 0.3f;
  bolt.lightColor = kShadow, bolt.lightIntensity = 6.0f, bolt.lightRadius = 3.0f;
  burst("shadow-v3-particles-flipbook-4x4", target.body, impact, 0.5f, 2.2f, tint(kShadow, 1.6f));
  Element& seal = add("shadow-v3-secondary-flipbook-4x4", impact, 3.2f);
  seal.from = seal.to = target.body + glm::vec3(0.0f, 0.1f, 0.0f);
  seal.pivot = {0.5f, 0.5f};
  seal.height = 1.5f;
  seal.fps = 9.0f;
  seal.fadeIn = 0.1f, seal.fadeOut = 1.2f;
  seal.color = glm::vec3(1.1f, 0.7f, 1.5f) * 0.8f;
  seal.pull = 1.2f, seal.softness = 0.3f;
  Element& flash = add(nullptr, impact, 0.6f);
  flash.from = flash.to = target.body;
  flash.lightColor = kShadow, flash.lightIntensity = 16.0f, flash.lightRadius = 4.0f, flash.lightDecay = 1.5f;
}

// A sick green orb arcs in and bursts into pustules at the feet: a plume, a puddle ring, bubbles on the body.
void SpellEffects::creepingPlague(const Spot& caster, const Spot& target, float start, float impact) {
  const glm::vec3 d = target.body - caster.body;
  const float dist = glm::length(d);
  const glm::vec3 dir = dist > 1e-3f ? d / dist : glm::vec3(1, 0, 0);
  const float travel = std::clamp(dist / 12.0f, 0.3f, 0.7f);
  const float launch = std::max(start + 0.05f, impact - travel);
  burst("poison-v3-particles-flipbook-4x4", caster.body + dir * 0.3f, start, launch - start + 0.2f, 1.0f, tint(kPoison, 1.1f), 0.6f);
  Element& orb = add("poison-flipbook-4x4", launch, impact - launch);
  orb.from = caster.body + dir * 0.4f, orb.to = target.feet + glm::vec3(0.0f, 0.3f, 0.0f);
  orb.moveStart = launch, orb.moveEnd = impact;
  orb.arc = std::min(dist * 0.2f, 1.8f);
  orb.pivot = {0.5f, 0.5f};
  orb.height = 0.9f;
  orb.fps = 18.0f;
  orb.color = glm::vec3(1.2f);
  orb.opacity = 0.4f;
  orb.pull = 0.3f;
  orb.lightColor = kPoison, orb.lightIntensity = 7.0f, orb.lightRadius = 3.0f;
  Element& plume = add("poison-v2-main-flipbook-4x4", impact, 1.1f);
  plume.from = plume.to = target.feet;
  plume.pivot = {0.5f, 0.06f};
  plume.height = 2.2f;
  plume.fadeOut = 0.35f;
  plume.color = glm::vec3(1.1f);
  plume.opacity = 0.3f;
  plume.pull = 1.0f, plume.softness = 0.3f;
  plume.mirror = random() < 0.5f;
  Element& puddle = add("poison-v2-secondary-flipbook-4x4", impact, 1.6f);
  puddle.from = puddle.to = target.feet;
  puddle.pivot = {0.5f, 0.45f};
  puddle.height = 2.0f;
  puddle.fadeOut = 0.6f;
  puddle.pull = 0.8f, puddle.softness = 0.3f;
  puddle.lightColor = kPoison, puddle.lightIntensity = 12.0f, puddle.lightRadius = 3.5f, puddle.lightDecay = 1.2f;
  burst("poison-v3-particles-flipbook-4x4", target.body, impact + 0.1f, 0.7f, 1.6f, tint(kPoison, 1.2f));
}

// Red threads are pulled from the target's chest to the caster's hand; a red glow swells on the caster as they arrive.
void SpellEffects::lifeDrain(const Spot& caster, const Spot& target, float start, float impact) {
  Element& reach = add("shadow-v3-secondary-flipbook-4x4", start, impact - start + 0.2f);
  reach.from = reach.to = caster.feet;
  reach.pivot = {0.5f, 0.45f};
  reach.height = 1.8f;
  reach.fadeIn = 0.1f, reach.fadeOut = 0.2f;
  reach.color = glm::vec3(1.2f, 0.5f, 0.6f);
  reach.pull = 0.8f, reach.softness = 0.3f;
  reach.lightColor = kBlood, reach.lightIntensity = 3.0f, reach.lightRadius = 2.5f;
  Element& grip = add("shadow-v3-secondary-flipbook-4x4", impact - 0.1f, 1.0f);
  grip.from = grip.to = target.feet;
  grip.pivot = {0.5f, 0.45f};
  grip.height = 2.0f;
  grip.fadeOut = 0.3f;
  grip.color = glm::vec3(1.3f, 0.4f, 0.5f);
  grip.pull = 0.8f, grip.softness = 0.3f;
  burst("shadow-v2-particles-flipbook-4x4", target.body, impact, 0.5f, 1.8f, glm::vec3(1.5f, 0.45f, 0.5f));
  stream("shadow-v2-particles-flipbook-4x4", target.body, caster.body, impact, 0.9f, 22, glm::vec3(1.6f, 0.4f, 0.45f), 0.5f);
  Element& swell = add(nullptr, impact + 0.3f, 0.9f);
  swell.from = swell.to = caster.body;
  swell.fadeIn = 0.3f;
  swell.lightColor = kBlood, swell.lightIntensity = 18.0f, swell.lightRadius = 4.0f, swell.lightDecay = 1.0f;
  Element& hurt = add(nullptr, impact, 0.4f);
  hurt.from = hurt.to = target.body;
  hurt.lightColor = kBlood, hurt.lightIntensity = 10.0f, hurt.lightRadius = 3.0f, hurt.lightDecay = 2.0f;
}

// The cast of the Mortalha: shadow gathers, then a burst of dark smoke at the centre; the zone itself (zoneStart) rolls
// in and stays.
void SpellEffects::shroudCast(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  burst("shadow-v3-particles-flipbook-4x4", caster.body, start, impact - start + 0.2f, 1.2f, tint(kShadow, 1.2f), 0.6f);
  const glm::vec3 centre = hexes.front().feet;
  Element& puff = add("shadow-v2-main-flipbook-4x4", impact, 1.4f);
  puff.from = puff.to = centre;
  puff.pivot = {0.5f, 0.06f};
  puff.height = 3.6f;
  puff.fadeOut = 0.5f;
  puff.color = glm::vec3(0.5f, 0.4f, 0.7f);
  puff.opacity = 0.6f;
  puff.pull = 0.9f, puff.softness = 0.35f;
  Element& flash = add(nullptr, impact, 0.6f);
  flash.from = flash.to = centre + glm::vec3(0.0f, 1.5f, 0.0f);
  flash.lightColor = kShadow, flash.lightIntensity = 14.0f, flash.lightRadius = 5.0f, flash.lightDecay = 1.5f;
}

// A shadow slash across the body, blood dripping to the feet, a red ring on the ground.
void SpellEffects::hemorrhage(const Spot& caster, const Spot& target, float start, float impact) {
  burst("shadow-v3-particles-flipbook-4x4", caster.body, std::max(start, impact - 0.35f), 0.4f, 1.0f, glm::vec3(1.3f, 0.5f, 0.7f), 0.6f);
  Element& slash = add("impact-2", impact, 0.3f);
  slash.from = slash.to = target.body;
  slash.pivot = {0.5f, 0.5f};
  slash.height = 1.7f;
  slash.fadeOut = 0.15f;
  slash.color = glm::vec3(1.6f, 0.25f, 0.2f);
  slash.opacity = 0.45f;
  slash.pull = 1.4f;
  slash.mirror = random() < 0.5f;
  for (int i = 0; i < 9; ++i) {
    const float t0 = impact + 0.05f + 0.08f * static_cast<float>(i);
    Element& drop = add("shadow-v2-particles-flipbook-4x4", t0, 0.6f);
    drop.from = target.body + glm::vec3((random() - 0.5f) * 0.4f, (random() - 0.3f) * 0.3f, 0.0f);
    drop.to = glm::vec3(drop.from.x, target.feet.y + 0.05f, drop.from.z);
    drop.moveStart = t0, drop.moveEnd = t0 + 0.5f;
    drop.pivot = {0.5f, 0.5f};
    drop.height = 0.35f, drop.endHeight = 0.15f;
    drop.fadeOut = 0.15f;
    drop.color = glm::vec3(1.6f, 0.3f, 0.3f);
    drop.pull = 1.2f;
  }
  Element& ring = add("shadow-v3-secondary-flipbook-4x4", impact + 0.2f, 1.2f);
  ring.from = ring.to = target.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 1.8f;
  ring.fadeIn = 0.2f, ring.fadeOut = 0.5f;
  ring.color = glm::vec3(1.3f, 0.35f, 0.35f);
  ring.pull = 0.8f, ring.softness = 0.3f;
  ring.lightColor = kBlood, ring.lightIntensity = 10.0f, ring.lightRadius = 3.0f, ring.lightDecay = 1.0f;
}

// Rings of runes close over the heads in the area and go out one by one; warp motes swirl round them.
void SpellEffects::profaneSilence(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  burst("warp-v3-particles-flipbook-4x4", caster.body, start, impact - start + 0.2f, 1.1f, tint(kShadow, 1.2f), 0.6f);
  const glm::vec3 centre = hexes.front().feet;
  Element& wave = add("holy-v3-secondary-flipbook-4x4", impact, 0.9f);
  wave.from = wave.to = centre;
  wave.pivot = {0.5f, 0.45f};
  wave.height = 1.5f, wave.endHeight = 2.0f * std::sqrt(3.0f) * 1.9f;
  wave.fadeOut = 0.4f;
  wave.color = glm::vec3(0.75f, 0.45f, 1.2f);
  wave.pull = 0.8f, wave.softness = 0.3f;
  wave.lightColor = kShadow, wave.lightIntensity = 12.0f, wave.lightRadius = 5.0f, wave.lightDecay = 1.5f;
  for (const Spot& h : hexes) {
    if (h.cardHeight <= 0.0f) continue;  // the runes seal only those standing there
    const float at = impact + 0.1f + 0.05f * hexDistance(h.feet, centre);
    Element& runes = add("holy-v3-secondary-flipbook-4x4", at, 1.8f);
    runes.from = runes.to = h.feet + glm::vec3(0.0f, h.cardHeight * 0.8f, 0.0f);
    runes.pivot = {0.5f, 0.5f};
    runes.height = 1.3f;
    runes.fps = 8.0f;
    runes.fadeIn = 0.15f, runes.fadeOut = 0.9f;
    runes.color = glm::vec3(0.8f, 0.5f, 1.3f);
    runes.pull = 1.3f, runes.softness = 0.3f;
    burst("warp-v3-particles-flipbook-4x4", h.body, at, 0.7f, 1.4f, glm::vec3(0.9f, 0.6f, 1.3f));
  }
}

// A black-green orb arcs to the centre; the cloud rolls out over every hex (plumes staggered by distance, puddles on
// the ground, dark smoke rising), under a strong sick light.
void SpellEffects::blackPestilence(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  const glm::vec3 centre = hexes.front().feet;
  const glm::vec3 d = centre - caster.body;
  const float dist = glm::length(d);
  const glm::vec3 dir = dist > 1e-3f ? d / dist : glm::vec3(1, 0, 0);
  const float travel = std::clamp(dist / 11.0f, 0.35f, 0.8f);
  const float launch = std::max(start + 0.05f, impact - travel);
  burst("poison-v3-particles-flipbook-4x4", caster.body + dir * 0.3f, start, launch - start + 0.2f, 1.2f, glm::vec3(0.6f, 1.1f, 0.4f), 0.6f);
  Element& orb = add("poison-flipbook-4x4", launch, impact - launch);
  orb.from = caster.body + dir * 0.4f, orb.to = centre + glm::vec3(0.0f, 0.4f, 0.0f);
  orb.moveStart = launch, orb.moveEnd = impact;
  orb.arc = std::min(dist * 0.25f, 2.5f);
  orb.pivot = {0.5f, 0.5f};
  orb.height = 1.2f;
  orb.fps = 18.0f;
  orb.color = glm::vec3(0.7f, 1.0f, 0.5f);
  orb.opacity = 0.5f;
  orb.pull = 0.3f;
  orb.lightColor = kPoison, orb.lightIntensity = 9.0f, orb.lightRadius = 3.5f;
  burst("poison-flipbook-4x4", centre + glm::vec3(0.0f, 1.0f, 0.0f), impact, 0.8f, 3.4f, glm::vec3(0.8f, 1.2f, 0.5f), 0.9f);
  for (const Spot& h : hexes) {
    const float at = impact + 0.06f * hexDistance(h.feet, centre) + random() * 0.05f;
    Element& puddle = add("poison-v2-secondary-flipbook-4x4", at, 2.2f);
    puddle.from = puddle.to = h.feet;
    puddle.pivot = {0.5f, 0.45f};
    puddle.height = 1.9f;
    puddle.fadeIn = 0.1f, puddle.fadeOut = 0.8f;
    puddle.color = glm::vec3(0.6f, 0.9f, 0.45f);
    puddle.pull = 0.6f, puddle.softness = 0.3f;
    Element& plume = add("poison-v2-main-flipbook-4x4", at, 1.3f + random() * 0.4f);
    plume.from = plume.to = h.feet + glm::vec3((random() - 0.5f) * 0.3f, 0.0f, (random() - 0.5f) * 0.3f);
    plume.pivot = {0.5f, 0.06f};
    plume.height = 2.0f + random() * 0.6f;
    plume.fadeOut = 0.5f;
    plume.color = glm::vec3(0.55f, 0.8f, 0.4f);
    plume.opacity = 0.4f;
    plume.pull = 0.8f, plume.softness = 0.35f;
    plume.mirror = random() < 0.5f;
    Element& smoke = add("fire-v2-smoke-4x4", at + 0.3f, 2.0f);
    smoke.from = h.feet + glm::vec3(0.0f, 0.5f, 0.0f), smoke.to = h.feet + glm::vec3(0.0f, 2.2f, 0.0f);
    smoke.moveStart = at + 0.3f, smoke.moveEnd = at + 2.3f;
    smoke.pivot = {0.5f, 0.5f};
    smoke.height = 1.6f, smoke.endHeight = 2.4f;
    smoke.fadeIn = 0.3f, smoke.fadeOut = 0.9f;
    smoke.color = glm::vec3(0.2f, 0.35f, 0.18f);
    smoke.opacity = 0.5f;
    smoke.pull = 0.5f, smoke.softness = 0.4f;
  }
  Element& light = add(nullptr, impact, 2.4f);
  light.from = light.to = centre + glm::vec3(0.0f, 1.5f, 0.0f);
  light.lightColor = kPoison, light.lightIntensity = 30.0f, light.lightRadius = 3.0f + 1.8f * std::sqrt(static_cast<float>(hexes.size())), light.lightDecay = 1.0f;
}

// A dark pillar closes round the target and grey motes fall from the head to the feet as the body wastes.
void SpellEffects::curseOfDecay(const Spot& caster, const Spot& target, float start, float impact) {
  burst("shadow-v3-particles-flipbook-4x4", caster.body, start, impact - start + 0.2f, 1.1f, tint(kShadow, 1.1f), 0.6f);
  Element& pillar = add("shadow-v3-main-flipbook-4x4", impact - 0.05f, 1.4f);
  pillar.from = pillar.to = target.feet;
  pillar.pivot = {0.5f, 0.06f};
  pillar.height = std::max(2.6f, target.cardHeight * 1.15f);
  pillar.fadeOut = 0.5f;
  pillar.color = glm::vec3(0.55f, 0.45f, 0.75f);
  pillar.opacity = 0.5f;
  pillar.pull = 1.1f, pillar.softness = 0.3f;
  pillar.lightColor = kShadow, pillar.lightIntensity = 10.0f, pillar.lightRadius = 3.5f, pillar.lightDecay = 1.0f;
  Element& ring = add("shadow-v3-secondary-flipbook-4x4", impact, 1.2f);
  ring.from = ring.to = target.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 2.0f;
  ring.fadeOut = 0.4f;
  ring.color = glm::vec3(0.8f, 0.6f, 1.1f);
  ring.pull = 0.8f, ring.softness = 0.3f;
  const float head = target.cardHeight > 0.0f ? target.cardHeight : 1.8f;
  for (int i = 0; i < 14; ++i) {
    const float t0 = impact + 0.1f + random() * 0.8f;
    Element& mote = add("shadow-v2-particles-flipbook-4x4", t0, 0.9f);
    mote.from = target.feet + glm::vec3((random() - 0.5f) * 0.7f, head * (0.6f + 0.4f * random()), (random() - 0.5f) * 0.3f);
    mote.to = glm::vec3(mote.from.x, target.feet.y + 0.05f, mote.from.z);
    mote.moveStart = t0, mote.moveEnd = t0 + 0.8f;
    mote.pivot = {0.5f, 0.5f};
    mote.height = 0.3f, mote.endHeight = 0.15f;
    mote.fadeIn = 0.1f, mote.fadeOut = 0.2f;
    mote.color = glm::vec3(0.5f, 0.5f, 0.55f);
    mote.pull = 1.2f;
  }
}

// Scythes of shadow sweep the area and pale souls stream from every struck body to the caster.
void SpellEffects::soulHarvest(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  Element& gather = add("shadow-v3-secondary-flipbook-4x4", start, impact - start + 0.3f);
  gather.from = gather.to = caster.feet;
  gather.pivot = {0.5f, 0.45f};
  gather.height = 2.0f;
  gather.fadeIn = 0.1f, gather.fadeOut = 0.3f;
  gather.color = glm::vec3(0.9f, 0.6f, 1.3f);
  gather.pull = 0.8f, gather.softness = 0.3f;
  gather.lightColor = kShadow, gather.lightIntensity = 5.0f, gather.lightRadius = 3.0f;
  const glm::vec3 centre = hexes.front().feet;
  // the sweep: a wide shadow sheet crossing the area at waist height
  const glm::vec3 across{1.0f, 0.0f, 0.0f};
  const float width = 1.7320508f * 2.0f * 2.6f;
  Element& scythe = add("shadow-flipbook-4x4", impact - 0.25f, 0.6f);
  scythe.from = centre + glm::vec3(0.0f, 0.8f, 0.0f) - across * width * 0.5f;
  scythe.to = centre + glm::vec3(0.0f, 0.8f, 0.0f) + across * width * 0.5f;
  scythe.moveStart = impact - 0.25f, scythe.moveEnd = impact + 0.3f;
  scythe.pivot = {0.5f, 0.5f};
  scythe.height = 2.6f;
  scythe.fps = 24.0f;
  scythe.fadeIn = 0.1f, scythe.fadeOut = 0.2f;
  scythe.color = glm::vec3(0.8f, 0.55f, 1.2f);
  scythe.opacity = 0.35f;
  scythe.pull = 0.6f, scythe.softness = 0.4f;
  scythe.lightColor = kShadow, scythe.lightIntensity = 14.0f, scythe.lightRadius = 5.0f;
  for (const Spot& h : hexes) {
    const float at = impact + 0.03f * hexDistance(h.feet, centre);
    Element& ring = add("shadow-v3-secondary-flipbook-4x4", at, 0.9f);
    ring.from = ring.to = h.feet;
    ring.pivot = {0.5f, 0.45f};
    ring.height = 1.6f;
    ring.fadeOut = 0.4f;
    ring.color = glm::vec3(0.7f, 0.5f, 1.0f) * 0.8f;
    ring.pull = 0.6f, ring.softness = 0.3f;
    if (h.cardHeight <= 0.0f) continue;
    burst("shadow-v3-particles-flipbook-4x4", h.body, at, 0.5f, 1.8f, tint(kShadow, 1.5f));
    stream("holy-v2-particles-flipbook-4x4", h.body, caster.body, at + 0.15f, 0.9f, 10, tint(kSoul, 1.4f), 0.45f);
  }
  Element& receive = add(nullptr, impact + 0.5f, 0.9f);
  receive.from = receive.to = caster.body;
  receive.fadeIn = 0.3f;
  receive.lightColor = kSoul, receive.lightIntensity = 14.0f, receive.lightRadius = 3.5f, receive.lightDecay = 1.0f;
}

// A point of black swells over the body and drinks the light, then collapses into a vortex and throws shards out.
void SpellEffects::devouringVoid(const Spot& caster, const Spot& target, float start, float impact) {
  burst("warp-v3-particles-flipbook-4x4", caster.body, start, impact - start + 0.2f, 1.2f, tint(kShadow, 1.3f), 0.6f);
  const float swell = std::min(0.8f, impact - start);
  Element& dot = add("shadow-flipbook-4x4", impact - swell, swell);
  dot.from = dot.to = target.body;
  dot.pivot = {0.5f, 0.5f};
  dot.height = 0.3f, dot.endHeight = 2.8f;
  dot.fps = 16.0f;
  dot.fadeIn = 0.1f;
  dot.color = glm::vec3(0.15f, 0.1f, 0.25f);
  dot.opacity = 0.85f;
  dot.pull = 1.2f, dot.softness = 0.35f;
  for (int i = 0; i < 10; ++i) {  // light drawn in: motes falling into the point
    const float t0 = impact - swell + random() * swell * 0.7f;
    Element& m = add("warp-v3-particles-flipbook-4x4", t0, 0.4f);
    const float a = random() * 6.2832f, r = 1.6f + random();
    m.from = target.body + glm::vec3(std::cos(a) * r, (random() - 0.5f) * 1.5f, std::sin(a) * r);
    m.to = target.body;
    m.moveStart = t0, m.moveEnd = t0 + 0.4f;
    m.pivot = {0.5f, 0.5f};
    m.height = 0.5f, m.endHeight = 0.1f;
    m.color = glm::vec3(0.9f, 0.7f, 1.4f);
    m.pull = 1.0f;
  }
  Element& vortex = add("warp-v3-main-flipbook-4x4", impact, 0.6f);
  vortex.from = vortex.to = target.body;
  vortex.pivot = {0.5f, 0.5f};
  vortex.height = 2.6f, vortex.endHeight = 0.6f;
  vortex.fadeOut = 0.2f;
  vortex.color = glm::vec3(0.8f, 0.5f, 1.3f);
  vortex.opacity = 0.5f;
  vortex.pull = 1.3f, vortex.softness = 0.3f;
  burst("shadow-v3-particles-flipbook-4x4", target.body, impact + 0.05f, 0.6f, 3.0f, tint(kShadow, 1.8f));
  Element& ring = add("warp-v3-secondary-flipbook-4x4", impact, 0.7f);
  ring.from = ring.to = target.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 1.2f, ring.endHeight = 3.2f;
  ring.fadeOut = 0.3f;
  ring.color = glm::vec3(0.9f, 0.6f, 1.4f);
  ring.pull = 0.8f, ring.softness = 0.3f;
  Element& flash = add(nullptr, impact, 0.5f);
  flash.from = flash.to = target.body;
  flash.lightColor = kShadow, flash.lightIntensity = 36.0f, flash.lightRadius = 6.0f, flash.lightDecay = 1.5f;
}

// --- Elementalista -------------------------------------------------------------------------------------------------------

// Fire gathers at the caster; a pillar of flame comes down on the centre and the ring catches, the ground glows and
// smoke rises; the burning zone (zoneStart) takes over afterwards.
void SpellEffects::pillarOfFlame(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  Element& gather = add("fire-v3-secondary-flipbook-4x4", start, impact - start + 0.2f);
  gather.from = gather.to = caster.feet;
  gather.pivot = {0.5f, 0.45f};
  gather.height = 1.9f;
  gather.fadeIn = 0.1f, gather.fadeOut = 0.2f;
  gather.pull = 0.8f, gather.softness = 0.3f;
  gather.lightColor = kFire, gather.lightIntensity = 5.0f, gather.lightRadius = 2.5f, gather.flickerLight = 0.3f;
  burst("fire-v3-particles-flipbook-4x4", caster.body, std::max(start, impact - 0.3f), 0.4f, 1.2f, glm::vec3(1.2f), 0.6f);
  const glm::vec3 centre = hexes.front().feet;
  for (const Spot& h : hexes) {
    const float dist = hexDistance(h.feet, centre);
    const float at = impact + 0.05f * dist;
    Element& pillar = add("fire-v3-main-flipbook-4x4", at, dist < 0.5f ? 1.1f : 0.9f);
    pillar.from = pillar.to = h.feet;
    pillar.pivot = {0.5f, 0.06f};
    pillar.height = dist < 0.5f ? 4.2f : 2.4f;
    pillar.fadeOut = 0.3f;
    pillar.color = glm::vec3(1.3f);
    pillar.pull = 1.0f, pillar.softness = 0.3f;
    pillar.mirror = random() < 0.5f;
    Element& ring = add("fire-v3-secondary-flipbook-4x4", at, 1.0f);
    ring.from = ring.to = h.feet;
    ring.pivot = {0.5f, 0.45f};
    ring.height = 2.0f;
    ring.fadeOut = 0.4f;
    ring.pull = 0.8f, ring.softness = 0.3f;
    Element& smoke = add("fire-v2-smoke-4x4", at + 0.5f, 1.6f);
    smoke.from = h.feet + glm::vec3(0.0f, 1.0f, 0.0f), smoke.to = h.feet + glm::vec3(0.0f, 3.0f, 0.0f);
    smoke.moveStart = at + 0.5f, smoke.moveEnd = at + 2.1f;
    smoke.pivot = {0.5f, 0.5f};
    smoke.height = 1.4f, smoke.endHeight = 2.2f;
    smoke.fadeIn = 0.3f, smoke.fadeOut = 0.8f;
    smoke.color = glm::vec3(0.3f);
    smoke.opacity = 0.45f;
    smoke.pull = 0.5f, smoke.softness = 0.4f;
  }
  Element& flash = add(nullptr, impact, 0.8f);
  flash.from = flash.to = centre + glm::vec3(0.0f, 2.0f, 0.0f);
  flash.lightColor = kFire, flash.lightIntensity = 45.0f, flash.lightRadius = 7.0f, flash.lightDecay = 1.2f, flash.flickerLight = 0.3f;
}

// Cold gathers; a streak of ice shoots along the whole line and every unit on it is caught in an eruption of ice.
void SpellEffects::iceLance(const Spot& caster, const std::vector<Spot>& line, float start, float impact) {
  burst("frost-v3-particles-flipbook-4x4", caster.body, start + 0.1f, std::max(impact - start, 0.3f), 1.3f, glm::vec3(1.1f), 0.6f);
  const glm::vec3 last = line.back().feet + glm::vec3(0.0f, 0.8f, 0.0f);
  const glm::vec3 first = line.front().feet + glm::vec3(0.0f, 0.8f, 0.0f);
  const float firstDist = glm::length(first - caster.body), lastDist = glm::length(last - caster.body);
  const float travel = std::clamp(firstDist / 24.0f, 0.1f, 0.3f);
  const float launch = std::max(start, impact - travel);
  const float arrive = launch + (impact - launch) * (firstDist > 1e-3f ? std::max(lastDist / firstDist, 1.0f) : 1.0f);
  Element& lance = add("frost-flipbook-4x4", launch, arrive - launch + 0.1f);
  lance.facing = Facing::Along;
  lance.from = caster.body, lance.to = last;
  lance.moveStart = launch, lance.moveEnd = arrive;
  lance.pivot = {0.5f, 0.5f};
  lance.height = 0.9f;
  lance.fps = 30.0f;
  lance.color = glm::vec3(1.4f);
  lance.opacity = 0.5f;
  lance.pull = 0.3f;
  lance.lightColor = kIce, lance.lightIntensity = 9.0f, lance.lightRadius = 3.0f;
  for (const Spot& t : line) {
    const float dist = glm::length(t.feet + glm::vec3(0.0f, 0.8f, 0.0f) - caster.body);
    const float at = launch + (arrive - launch) * (lastDist > 1e-3f ? dist / lastDist : 1.0f);
    Element& ice = add("frost-v3-main-flipbook-4x4", at - 0.03f, t.cardHeight > 0.0f ? 0.95f : 0.6f);
    ice.from = ice.to = t.feet;
    ice.pivot = {0.5f, 0.06f};
    ice.height = t.cardHeight > 0.0f ? 2.4f : 1.4f;
    ice.fadeOut = 0.25f;
    ice.color = glm::vec3(1.1f);
    ice.opacity = 0.35f;
    ice.pull = 0.9f, ice.softness = 0.3f;
    ice.mirror = random() < 0.5f;
    if (t.cardHeight <= 0.0f) continue;
    Element& ring = add("frost-v3-secondary-flipbook-4x4", at, 0.9f);
    ring.from = ring.to = t.feet;
    ring.pivot = {0.5f, 0.45f};
    ring.height = 2.0f;
    ring.fadeOut = 0.3f;
    ring.pull = 0.8f, ring.softness = 0.3f;
    ring.lightColor = kIce, ring.lightIntensity = 10.0f, ring.lightRadius = 3.2f, ring.lightDecay = 2.0f;
    burst("frost-v3-particles-flipbook-4x4", t.body, at, 0.5f, 1.8f, glm::vec3(1.3f));
  }
}

// A dark cloud forms over the area; bolts fall on everyone standing in it (and a few empty hexes), echoes crackle on
// their neighbours a beat later.
void SpellEffects::thunderstorm(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  const glm::vec3 centre = hexes.front().feet;
  burst("lightning-v3-particles-flipbook-4x4", caster.body, std::max(start, impact - 0.4f), 0.5f, 1.2f, glm::vec3(1.2f), 0.6f);
  const float radius = 1.0f;
  for (const Spot& h : hexes) {
    if (hexDistance(h.feet, centre) > radius + 0.1f && random() > 0.35f) continue;
    Element& cloud = add("fire-v2-smoke-4x4", start, impact - start + 1.4f);
    cloud.from = cloud.to = h.feet + glm::vec3((random() - 0.5f) * 0.6f, 4.2f + random() * 0.8f, (random() - 0.5f) * 0.6f);
    cloud.pivot = {0.5f, 0.5f};
    cloud.height = 2.6f + random();
    cloud.fps = 6.0f;
    cloud.fadeIn = 0.5f, cloud.fadeOut = 0.8f;
    cloud.color = glm::vec3(0.12f, 0.13f, 0.2f);
    cloud.opacity = 0.65f;
    cloud.pull = 0.3f, cloud.softness = 0.5f;
    cloud.mirror = random() < 0.5f;
  }
  int strikes = 0, empties = 0;
  for (const Spot& h : hexes) {
    const bool stands = h.cardHeight > 0.0f;
    if (!stands && (empties >= 3 || random() > 0.3f)) continue;
    if (!stands) ++empties;
    bolt(h, impact + 0.06f * static_cast<float>(strikes++), stands ? 1.0f : 0.6f);
  }
  // echoes: smaller strikes beside the struck
  int echoes = 0;
  for (const Spot& h : hexes) {
    if (h.cardHeight > 0.0f || echoes >= 4 || random() > 0.4f) continue;
    bolt(h, impact + 0.35f + 0.05f * static_cast<float>(echoes++), 0.45f);
  }
  Element& flash = add(nullptr, impact, 0.7f);
  flash.from = flash.to = centre + glm::vec3(0.0f, 3.0f, 0.0f);
  flash.lightColor = kStorm, flash.lightIntensity = 40.0f, flash.lightRadius = 9.0f, flash.lightDecay = 1.5f;
}

// The cast of Meteoro: the caster points, embers rise, and the mark burns on the ground (zoneStart draws it) until the
// rock falls at the caster's next turn (meteorFall).
void SpellEffects::meteorMark(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  burst("ember-v3-particles-flipbook-4x4", caster.body, start, impact - start + 0.3f, 1.4f, glm::vec3(1.3f, 0.8f, 0.6f), 0.6f);
  const glm::vec3 centre = hexes.front().feet;
  Element& ring = add("fire-v3-secondary-flipbook-4x4", impact, 1.2f);
  ring.from = ring.to = centre;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 1.0f, ring.endHeight = 2.4f;
  ring.fadeOut = 0.5f;
  ring.color = glm::vec3(1.5f, 0.4f, 0.25f);
  ring.pull = 0.6f, ring.softness = 0.3f;
  ring.lightColor = {1.0f, 0.3f, 0.15f}, ring.lightIntensity = 12.0f, ring.lightRadius = 4.0f, ring.lightDecay = 1.0f;
}

void SpellEffects::meteorFall(const std::vector<Spot>& hexes, float start, float impact) {
  if (hexes.empty()) return;
  const glm::vec3 centre = hexes.front().feet;
  const glm::vec3 sky = centre + glm::vec3(5.0f, 16.0f, -7.0f);
  const float fall = std::max(impact - start, 0.3f);
  Element& rock = add("fireball-core-v1", start, fall);
  rock.from = sky, rock.to = centre + glm::vec3(0.0f, 0.3f, 0.0f);
  rock.moveStart = start, rock.moveEnd = impact;
  rock.pivot = {0.5f, 0.5f};
  rock.height = 1.2f, rock.endHeight = 3.2f;
  rock.fadeIn = 0.1f;
  rock.color = glm::vec3(1.6f, 1.1f, 0.8f);
  rock.opacity = 0.8f;
  rock.pull = 0.3f;
  rock.lightColor = kFire, rock.lightIntensity = 30.0f, rock.lightRadius = 8.0f, rock.flickerLight = 0.25f;
  const Element flight = rock;
  for (float t = start; t < impact; t += 0.025f) {  // the trail
    Element& spark = add("fire-v3-particles-flipbook-4x4", t, 0.5f);
    spark.from = spark.to = position(flight, t) + glm::vec3(random() - 0.5f, random() - 0.5f, random() - 0.5f) * 0.6f;
    spark.pivot = {0.5f, 0.5f};
    spark.height = 1.4f, spark.endHeight = 0.4f;
    spark.fadeOut = 0.35f;
    spark.color = glm::vec3(1.2f, 0.8f, 0.5f);
    spark.pull = 0.3f;
    spark.mirror = random() < 0.5f;
  }
  Element& column = add("fire-v3-main-flipbook-4x4", impact, 1.3f);
  column.from = column.to = centre;
  column.pivot = {0.5f, 0.06f};
  column.height = 6.0f;
  column.fadeOut = 0.4f;
  column.color = glm::vec3(1.5f);
  column.pull = 1.0f, column.softness = 0.3f;
  Element& wave = add("fire-v3-secondary-flipbook-4x4", impact, 0.9f);
  wave.from = wave.to = centre;
  wave.pivot = {0.5f, 0.45f};
  wave.height = 2.0f, wave.endHeight = 1.7320508f * 2.0f * 3.2f;
  wave.fadeOut = 0.4f;
  wave.color = glm::vec3(1.3f);
  wave.pull = 0.6f, wave.softness = 0.3f;
  for (const Spot& h : hexes) {
    const float dist = hexDistance(h.feet, centre);
    const float at = impact + 0.04f * dist;
    burst("fire-flipbook-4x4", h.feet + glm::vec3(0.0f, 0.6f, 0.0f), at, 0.7f + random() * 0.3f, dist < 0.5f ? 3.0f : 1.8f, glm::vec3(1.3f), 0.7f);
    Element& smoke = add("fire-v2-smoke-4x4", at + 0.4f, 2.2f);
    smoke.from = h.feet + glm::vec3(0.0f, 1.0f, 0.0f), smoke.to = h.feet + glm::vec3(0.0f, 3.5f, 0.0f);
    smoke.moveStart = at + 0.4f, smoke.moveEnd = at + 2.6f;
    smoke.pivot = {0.5f, 0.5f};
    smoke.height = 1.6f, smoke.endHeight = 2.8f;
    smoke.fadeIn = 0.3f, smoke.fadeOut = 1.0f;
    smoke.color = glm::vec3(0.25f);
    smoke.opacity = 0.5f;
    smoke.pull = 0.5f, smoke.softness = 0.45f;
  }
  Element& flash = add(nullptr, impact, 1.2f);
  flash.from = flash.to = centre + glm::vec3(0.0f, 2.5f, 0.0f);
  flash.lightColor = kFire, flash.lightIntensity = 90.0f, flash.lightRadius = 12.0f, flash.lightDecay = 1.3f, flash.flickerLight = 0.25f;
}

// The area crystallises from the centre outward: a frost ring runs out, ice erupts on every hex, white motes hang a
// moment over the bodies and fall as dust.
void SpellEffects::absoluteZero(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  burst("frost-v3-particles-flipbook-4x4", caster.body, start + 0.1f, std::max(impact - start, 0.3f), 1.3f, glm::vec3(1.1f), 0.6f);
  const glm::vec3 centre = hexes.front().feet;
  float radius = 1.0f;
  for (const Spot& h : hexes) radius = std::max(radius, hexDistance(h.feet, centre));
  Element& wave = add("frost-v3-secondary-flipbook-4x4", impact, 0.8f);
  wave.from = wave.to = centre;
  wave.pivot = {0.5f, 0.45f};
  wave.height = 1.6f, wave.endHeight = 1.7320508f * 2.0f * (radius + 0.6f);
  wave.fadeOut = 0.35f;
  wave.color = glm::vec3(1.3f);
  wave.pull = 0.6f, wave.softness = 0.3f;
  wave.lightColor = kIce, wave.lightIntensity = 20.0f, wave.lightRadius = 7.0f, wave.lightDecay = 1.0f;
  for (const Spot& h : hexes) {
    const float dist = hexDistance(h.feet, centre);
    const float at = impact + 0.07f * dist;
    Element& ice = add("frost-v3-main-flipbook-4x4", at, 1.1f);
    ice.from = ice.to = h.feet + glm::vec3((random() - 0.5f) * 0.3f, 0.0f, (random() - 0.5f) * 0.3f);
    ice.pivot = {0.5f, 0.06f};
    ice.height = h.cardHeight > 0.0f ? 2.6f : 1.5f + random() * 0.5f;
    ice.fadeOut = 0.4f;
    ice.color = glm::vec3(1.15f);
    ice.opacity = 0.4f;
    ice.pull = 0.9f, ice.softness = 0.3f;
    ice.mirror = random() < 0.5f;
    if (h.cardHeight <= 0.0f) continue;
    burst("frost-v3-particles-flipbook-4x4", h.body, at + 0.05f, 0.7f, 1.9f, glm::vec3(1.4f));
    Element& glow = add(nullptr, at, 0.8f);
    glow.from = glow.to = h.body;
    glow.lightColor = kIce, glow.lightIntensity = 10.0f, glow.lightRadius = 3.0f, glow.lightDecay = 1.5f;
  }
}

// One bolt from the hand to the first target, then arcs that jump from body to body, each a little thinner.
void SpellEffects::chainLightning(const Spot& caster, const std::vector<Spot>& chain, float start, float impact) {
  Element& ring = add("lightning-v3-secondary-flipbook-4x4", start, impact - start + 0.15f);
  ring.from = ring.to = caster.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 1.9f;
  ring.fadeIn = 0.1f, ring.fadeOut = 0.2f;
  ring.pull = 0.8f, ring.softness = 0.3f;
  ring.lightColor = kStorm, ring.lightIntensity = 5.0f, ring.lightRadius = 2.5f;
  glm::vec3 from = caster.body;
  float scale = 1.0f;
  for (size_t i = 0; i < chain.size(); ++i) {
    const glm::vec3 to = chain[i].body;
    const float at = impact + 0.13f * static_cast<float>(i);
    // the arc: the bolt art stretched between the two bodies, flickering through its three shapes
    Element& arc = add("lightning-core-v1", at - 0.06f, 0.2f);
    arc.flicker = {clip("lightning-core-v1"), clip("lightning-core-v2"), clip("lightning-core-v3")};
    arc.facing = Facing::Along;
    // Laid along from -> to and all but still: it creeps 4 cm over its life so the facing reads its direction.
    const glm::vec3 mid = (from + to) * 0.5f;
    const glm::vec3 dir = glm::length(to - from) > 1e-3f ? glm::normalize(to - from) : glm::vec3(1, 0, 0);
    arc.from = mid - dir * 0.02f, arc.to = mid + dir * 0.02f;
    arc.moveStart = at - 0.06f, arc.moveEnd = at + 0.14f;
    arc.pivot = {0.5f, 0.5f};
    arc.height = std::max(glm::length(to - from), 0.5f);
    arc.angle = 1.5708f;  // the bolt art runs top to bottom: its length turned onto the line between the two bodies
    arc.fadeOut = 0.08f;
    arc.color = glm::vec3(1.5f) * scale;
    arc.pull = 0.5f, arc.softness = 0.2f;
    arc.mirror = random() < 0.5f;
    Element& rise = add("lightning-v3-main-flipbook-4x4", at, 0.6f);
    rise.from = rise.to = chain[i].feet;
    rise.pivot = {0.5f, 0.06f};
    rise.height = 2.4f * scale;
    rise.fadeOut = 0.2f;
    rise.color = glm::vec3(1.3f);
    rise.pull = 1.3f, rise.softness = 0.3f;
    burst("lightning-v3-particles-flipbook-4x4", to, at, 0.45f, 1.7f * scale, glm::vec3(1.4f));
    Element& flash = add(nullptr, at, 0.4f);
    flash.from = flash.to = to;
    flash.lightColor = kStorm, flash.lightIntensity = 30.0f * scale, flash.lightRadius = 5.0f, flash.lightDecay = 1.5f;
    from = to;
    scale *= 0.85f;
  }
}

// The ground cracks round the caster: a ring of fire runs out, flames rise on the outer ring and burst inward; the
// caster stands in the middle of it, embers rising; the ring keeps burning (zoneStart).
void SpellEffects::eruption(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  burst("ember-v3-particles-flipbook-4x4", caster.body, start, impact - start + 0.3f, 1.6f, glm::vec3(1.3f, 0.9f, 0.6f), 0.6f);
  Element& stand = add("fire-v3-secondary-flipbook-4x4", start, impact - start + 0.2f);
  stand.from = stand.to = caster.feet;
  stand.pivot = {0.5f, 0.45f};
  stand.height = 2.0f;
  stand.fadeIn = 0.1f, stand.fadeOut = 0.2f;
  stand.color = glm::vec3(1.3f, 0.7f, 0.5f);
  stand.pull = 0.8f, stand.softness = 0.3f;
  stand.lightColor = kFire, stand.lightIntensity = 6.0f, stand.lightRadius = 3.0f, stand.flickerLight = 0.3f;
  float radius = 1.0f;
  for (const Spot& h : hexes) radius = std::max(radius, hexDistance(h.feet, caster.feet));
  Element& wave = add("fire-v3-secondary-flipbook-4x4", impact, 0.9f);
  wave.from = wave.to = caster.feet;
  wave.pivot = {0.5f, 0.45f};
  wave.height = 1.5f, wave.endHeight = 1.7320508f * 2.0f * (radius + 0.6f);
  wave.fadeOut = 0.4f;
  wave.color = glm::vec3(1.4f);
  wave.pull = 0.6f, wave.softness = 0.3f;
  for (const Spot& h : hexes) {
    const float dist = hexDistance(h.feet, caster.feet);
    if (dist < 0.5f) continue;
    const float at = impact + 0.07f * dist;
    const bool rim = dist > radius - 0.6f;
    Element& flame = add("fire-v3-main-flipbook-4x4", at, rim ? 1.0f : 0.7f);
    flame.from = flame.to = h.feet + glm::vec3((random() - 0.5f) * 0.3f, 0.0f, (random() - 0.5f) * 0.3f);
    flame.pivot = {0.5f, 0.06f};
    flame.height = rim ? 3.0f : 1.6f + random() * 0.5f;
    flame.fadeOut = 0.3f;
    flame.color = glm::vec3(1.3f);
    flame.pull = 1.0f, flame.softness = 0.3f;
    flame.mirror = random() < 0.5f;
    if (h.cardHeight > 0.0f) burst("fire-v3-particles-flipbook-4x4", h.body, at, 0.6f, 1.8f, glm::vec3(1.3f));
  }
  Element& flash = add(nullptr, impact, 1.0f);
  flash.from = flash.to = caster.feet + glm::vec3(0.0f, 2.0f, 0.0f);
  flash.lightColor = kFire, flash.lightIntensity = 55.0f, flash.lightRadius = 9.0f, flash.lightDecay = 1.2f, flash.flickerLight = 0.3f;
}

// A gust of snow and wind rushes down the cone from the caster's hands; ice dust bursts on every hex as it passes and
// the struck slide back (the stage moves their cards).
void SpellEffects::glacialGale(const Spot& caster, const std::vector<Spot>& cone, float start, float impact) {
  Element& gather = add("frost-v3-secondary-flipbook-4x4", start, impact - start + 0.2f);
  gather.from = gather.to = caster.feet;
  gather.pivot = {0.5f, 0.45f};
  gather.height = 1.8f;
  gather.fadeIn = 0.1f, gather.fadeOut = 0.2f;
  gather.pull = 0.8f, gather.softness = 0.3f;
  gather.lightColor = kIce, gather.lightIntensity = 5.0f, gather.lightRadius = 2.5f;
  float reach = 1.0f;
  for (const Spot& h : cone) reach = std::max(reach, hexDistance(h.feet, caster.feet));
  for (const Spot& h : cone) {
    const float dist = hexDistance(h.feet, caster.feet);
    const float at = impact - 0.1f + 0.09f * dist;
    const glm::vec3 away = glm::length(h.feet - caster.feet) > 1e-3f ? glm::normalize(h.feet - caster.feet) : glm::vec3(1, 0, 0);
    Element& gust = add("frost-v2-main-flipbook-4x4", at, 0.7f);
    gust.from = h.feet + glm::vec3(0.0f, 0.6f, 0.0f) - away * 0.6f, gust.to = h.feet + glm::vec3(0.0f, 0.8f, 0.0f) + away * 0.9f;
    gust.moveStart = at, gust.moveEnd = at + 0.7f;
    gust.pivot = {0.5f, 0.5f};
    gust.height = 1.6f + 0.4f * random();
    gust.fps = 20.0f;
    gust.fadeIn = 0.1f, gust.fadeOut = 0.3f;
    gust.color = glm::vec3(1.2f);
    gust.opacity = 0.25f;
    gust.pull = 0.6f, gust.softness = 0.35f;
    gust.mirror = random() < 0.5f;
    burst("frost-v3-particles-flipbook-4x4", h.feet + glm::vec3(0.0f, 0.7f, 0.0f), at + 0.1f, 0.5f, 1.5f, glm::vec3(1.3f), 0.6f);
    if (h.cardHeight > 0.0f) {
      Element& ice = add("frost-v3-main-flipbook-4x4", at + 0.05f, 0.8f);
      ice.from = ice.to = h.feet;
      ice.pivot = {0.5f, 0.06f};
      ice.height = 2.2f;
      ice.fadeOut = 0.3f;
      ice.color = glm::vec3(1.1f);
      ice.opacity = 0.3f;
      ice.pull = 0.9f, ice.softness = 0.3f;
    }
  }
  Element& flash = add(nullptr, impact, 0.9f);
  flash.from = flash.to = caster.body + glm::vec3(0.0f, 0.5f, 0.0f);
  flash.lightColor = kIce, flash.lightIntensity = 18.0f, flash.lightRadius = 3.0f + reach * 1.7f, flash.lightDecay = 1.0f;
}

// The sky darkens for a breath; four (or five) columns of lightning fall almost at once, their echoes round them.
void SpellEffects::wrathOfTheSkies(const Spot& caster, const std::vector<Spot>& targets, float start, float impact) {
  Element& ring = add("lightning-v3-secondary-flipbook-4x4", start, impact - start + 0.15f);
  ring.from = ring.to = caster.feet;
  ring.pivot = {0.5f, 0.45f};
  ring.height = 2.2f;
  ring.fadeIn = 0.1f, ring.fadeOut = 0.2f;
  ring.color = glm::vec3(1.2f);
  ring.pull = 0.8f, ring.softness = 0.3f;
  ring.lightColor = kStorm, ring.lightIntensity = 6.0f, ring.lightRadius = 3.0f;
  for (const Spot& t : targets) {
    Element& cloud = add("fire-v2-smoke-4x4", start, impact - start + 1.0f);
    cloud.from = cloud.to = t.feet + glm::vec3(0.0f, 5.0f, 0.0f);
    cloud.pivot = {0.5f, 0.5f};
    cloud.height = 3.2f;
    cloud.fps = 6.0f;
    cloud.fadeIn = 0.4f, cloud.fadeOut = 0.6f;
    cloud.color = glm::vec3(0.1f, 0.11f, 0.18f);
    cloud.opacity = 0.7f;
    cloud.pull = 0.3f, cloud.softness = 0.5f;
  }
  for (size_t i = 0; i < targets.size(); ++i) bolt(targets[i], impact + 0.08f * static_cast<float>(i), 1.25f);
  for (size_t i = 0; i < targets.size(); ++i) bolt(targets[i], impact + 0.4f + 0.06f * static_cast<float>(i), 0.5f);
  Element& flash = add(nullptr, impact, 0.8f);
  flash.from = flash.to = mean(targets) + glm::vec3(0.0f, 3.0f, 0.0f);
  flash.lightColor = kStorm, flash.lightIntensity = 60.0f, flash.lightRadius = 11.0f, flash.lightDecay = 1.5f;
}

// Embers rise from the ground over the whole area, the air shimmers, then an orange-white blast and every hex left
// smouldering (zoneStart keeps the embers).
void SpellEffects::primordialEmber(const Spot& caster, const std::vector<Spot>& hexes, float start, float impact) {
  Element& gather = add("ember-v3-secondary-flipbook-4x4", start, impact - start + 0.2f);
  gather.from = gather.to = caster.feet;
  gather.pivot = {0.5f, 0.45f};
  gather.height = 2.2f;
  gather.fadeIn = 0.1f, gather.fadeOut = 0.2f;
  gather.color = glm::vec3(1.3f, 0.9f, 0.7f);
  gather.pull = 0.8f, gather.softness = 0.3f;
  gather.lightColor = kEmberGlow, gather.lightIntensity = 8.0f, gather.lightRadius = 3.0f, gather.flickerLight = 0.3f;
  const glm::vec3 centre = hexes.front().feet;
  float radius = 1.0f;
  for (const Spot& h : hexes) radius = std::max(radius, hexDistance(h.feet, centre));
  const float rise = std::min(0.9f, std::max(impact - start - 0.1f, 0.2f));
  for (const Spot& h : hexes) {  // embers rising from the ground before the blast
    if (random() > 0.6f) continue;
    const float t0 = impact - rise + random() * rise * 0.6f;
    Element& e = add("ember-particles-flipbook-4x4", t0, rise);
    e.from = h.feet + glm::vec3((random() - 0.5f) * 0.6f, 0.1f, (random() - 0.5f) * 0.6f);
    e.to = e.from + glm::vec3(0.0f, 1.5f + random(), 0.0f);
    e.moveStart = t0, e.moveEnd = t0 + rise;
    e.pivot = {0.5f, 0.5f};
    e.height = 0.6f;
    e.fps = 10.0f;
    e.fadeIn = 0.2f, e.fadeOut = 0.2f;
    e.color = glm::vec3(1.4f, 0.7f, 0.4f);
    e.pull = 0.4f;
  }
  Element& column = add("ember-v2-main-flipbook-4x4", impact, 1.4f);
  column.from = column.to = centre;
  column.pivot = {0.5f, 0.06f};
  column.height = 5.0f;
  column.fadeOut = 0.5f;
  column.color = glm::vec3(1.5f, 1.1f, 0.8f);
  column.pull = 1.0f, column.softness = 0.3f;
  Element& wave = add("ember-v3-secondary-flipbook-4x4", impact, 1.0f);
  wave.from = wave.to = centre;
  wave.pivot = {0.5f, 0.45f};
  wave.height = 2.0f, wave.endHeight = 1.7320508f * 2.0f * (radius + 0.7f);
  wave.fadeOut = 0.4f;
  wave.color = glm::vec3(1.4f, 1.0f, 0.7f);
  wave.pull = 0.6f, wave.softness = 0.3f;
  for (const Spot& h : hexes) {
    const float dist = hexDistance(h.feet, centre);
    const float at = impact + 0.05f * dist;
    burst("ember-flipbook-4x4", h.feet + glm::vec3(0.0f, 0.7f, 0.0f), at, 0.8f + random() * 0.3f, dist < 0.5f ? 3.2f : 2.0f, glm::vec3(1.4f, 1.0f, 0.8f), 0.7f);
    Element& plume = add("ember-v3-main-flipbook-4x4", at + 0.05f, 1.0f + random() * 0.4f);
    plume.from = plume.to = h.feet + glm::vec3((random() - 0.5f) * 0.3f, 0.0f, (random() - 0.5f) * 0.3f);
    plume.pivot = {0.5f, 0.06f};
    plume.height = 1.8f + random() * 0.6f;
    plume.fadeOut = 0.4f;
    plume.color = glm::vec3(1.3f, 0.95f, 0.75f);
    plume.pull = 0.9f, plume.softness = 0.3f;
    plume.mirror = random() < 0.5f;
    if (h.cardHeight > 0.0f) burst("ember-v3-particles-flipbook-4x4", h.body, at, 0.6f, 1.9f, glm::vec3(1.4f, 1.0f, 0.7f));
  }
  Element& flash = add(nullptr, impact, 1.3f);
  flash.from = flash.to = centre + glm::vec3(0.0f, 2.5f, 0.0f);
  flash.lightColor = kEmberGlow, flash.lightIntensity = 100.0f, flash.lightRadius = 4.0f + radius * 2.2f, flash.lightDecay = 1.2f, flash.flickerLight = 0.25f;
}

}  // namespace ember
