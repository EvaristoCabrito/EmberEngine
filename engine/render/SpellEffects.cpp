#include "render/SpellEffects.h"

#include <algorithm>
#include <cmath>

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
  static const char* kVersion01[] = {"magicMissile", "magicMissileV2", "lightning", "lightningTier3", "shock", "frost", "cureMinor",
                                     "longShot", "piercing", "piercingThrust", "doubleStrike"};
  for (const char* k : kVersion01)
    if (kind == k) return 1;
  return 0;
}

bool SpellEffects::cast(const std::string& kind, int look, const Spot& caster, const std::vector<Spot>& targets, float start, float impact) {
  if (look < 1 || look > looks(kind)) return false;
  impact = std::max(impact, start + 0.05f);
  if (targets.empty() && kind != "piercingThrust" && kind != "doubleStrike") return false;
  if (kind == "magicMissile" || kind == "magicMissileV2") magicMissile(caster, targets.front(), start, impact);
  else if (kind == "lightning" || kind == "lightningTier3") lightning(caster, targets, start, impact, false);
  else if (kind == "shock") lightning(caster, targets, start, impact, true);
  else if (kind == "frost") frost(caster, targets, start, impact);
  else if (kind == "cureMinor") cure(caster, targets, start, impact);
  else if (kind == "longShot") arrow(caster, targets, start, impact, false);
  else if (kind == "piercing") arrow(caster, targets, start, impact, true);
  else if (kind == "piercingThrust" || kind == "doubleStrike") bladeSkill(caster, start, impact);
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

bool SpellEffects::active(float now) const {
  return std::any_of(elements_.begin(), elements_.end(), [&](const Element& e) { return e.end > now; });
}

void SpellEffects::build(float now, const Camera& camera, bool flat, float flatTop, std::vector<EffectInstance>& out,
                         std::vector<Light>& lights) {
  std::erase_if(elements_, [&](const Element& e) { return e.end <= now; });
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
      lights.push_back({at, e.lightColor, e.lightIntensity * envelope * decay, e.lightRadius, std::max(at.y, 1.0f)});
    }
    SpriteClipId id = e.clip;
    if (!e.flicker.empty()) id = e.flicker[static_cast<size_t>((now - e.start) / kFlickerStep) % e.flicker.size()];
    if (id == kNoSpriteClip) continue;
    sprites_.request(id, SpriteSystem::kOnScreen);
    const SpriteSystem::Texture tex = sprites_.texture(id);
    if (tex.index == kNoTexture) continue;
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
}

}  // namespace ember
