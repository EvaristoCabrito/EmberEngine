#include "render/vfx/SeleneFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPi;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
constexpr float kPull = 1.3f, kGroundPull = 0.7f;  // as CrusaderFx: in front of the card, which is drawn 1.2 units nearer
constexpr float kRingUnderFeet = 0.6f;  // a ring this near its unit's feet height lies on the ground
constexpr float kModeStreak = 0.0f, kModeEnergy = 2.0f, kModeRing = 3.0f;  // Combat material
constexpr float kHolyPool = 1.0f, kHolyMote = 2.0f, kHolyLine = 4.0f;       // Holy material, in violet
constexpr float kBurstLead = 0.3f;  // the rune flares this long before the burst's hit
constexpr float kStandFade = 0.5f;  // the standing mark fades in / out over this

constexpr glm::vec3 kViolet{0.62f, 0.4f, 1.0f}, kCore{0.9f, 0.82f, 1.0f}, kDeep{0.4f, 0.2f, 0.8f}, kWhite{0.97f, 0.95f, 1.0f};
constexpr glm::vec3 kLight{0.66f, 0.45f, 1.0f};

float ease(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}
float hash01(uint32_t seed, uint32_t i, uint32_t salt) {
  uint32_t h = seed ^ (i * 0x9e3779b9u) ^ (salt * 0x85ebca6bu);
  h ^= h >> 16, h *= 0x7feb352du, h ^= h >> 15, h *= 0x846ca68bu, h ^= h >> 16;
  return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}
glm::vec3 flatDir(glm::vec3 v, glm::vec3 fallback) {
  v.y = 0.0f;
  return glm::dot(v, v) > 1e-8f ? glm::normalize(v) : fallback;
}
glm::vec3 pulled(glm::vec3 p, glm::vec3 eye, float by) {
  const glm::vec3 d = eye - p;
  const float len = glm::length(d);
  return len > by + 0.5f ? p + d * (by / len) : p;
}
float bodyHeight(float h) { return std::clamp(h > 0.0f ? h : 1.8f, 1.0f, 4.0f); }

const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, -0.5f, 0.0f}, 1.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f},
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f}, {{-0.5f, 0.5f, 0.0f}, 0.0f, {0, 0, 1}, 1.0f}};
  return g;
}
const std::vector<VfxVertex>& sphereGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::sphere(1.0f, 28, 18);
  return g;
}
// A flat disc of radius 1 on y 0 (u around, v out), facing up.
const std::vector<VfxVertex>& discGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 64, out = 12;
    auto v = [](int i, int j) {
      const float a = static_cast<float>(i) / around * kTau, r = static_cast<float>(j) / out;
      return VfxVertex{{std::cos(a) * r, 0.0f, std::sin(a) * r}, static_cast<float>(i) / around, {0.0f, 1.0f, 0.0f}, r};
    };
    std::vector<VfxVertex> tris;
    for (int i = 0; i < around; ++i)
      for (int j = 0; j < out; ++j) tris.insert(tris.end(), {v(i, j), v(i, j + 1), v(i + 1, j), v(i + 1, j), v(i, j + 1), v(i + 1, j + 1)});
    return tris;
  }();
  return g;
}
// A flat torus of radius 1 lying flat (u around): the rune's band.
const std::vector<VfxVertex>& haloGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> ring = vfxgeo::torus(1.0f, 0.05f, 8, 64, kTau);
    const glm::mat4 flat = glm::rotate(glm::mat4(1.0f), -kPi * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    for (VfxVertex& v : ring) {
      v.position = glm::vec3(flat * glm::vec4(v.position, 1.0f));
      v.normal = glm::vec3(flat * glm::vec4(v.normal, 0.0f));
    }
    return ring;
  }();
  return g;
}

struct CombatDraw {
  VfxFrame& frame;
  void operator()(uint32_t first, uint32_t count, const glm::mat4& model, float mode, float time, float intensity, float opacity, glm::vec3 color, float colorA,
                  glm::vec4 params1, glm::vec3 sight, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) const {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {time, mode, intensity, opacity};
    d.gpu.params1 = params1;
    d.gpu.color = glm::vec4(color, colorA);
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = count;
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
    d.gpu.sightAt = sight;
    // A ring at the feet lies under every card (the user, 2026-10-30: "o poison tinha que estar abaixo de todos os pés").
    if (mode == kModeRing && std::abs(model[3].y - sight.y) < kRingUnderFeet) d.underCards = true;
    d.instances = instances;
    d.blend = blend;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
};
struct HolyDraw {
  VfxFrame& frame;
  void operator()(uint32_t first, uint32_t count, const glm::mat4& model, float mode, float time, float intensity, float opacity, glm::vec3 color, glm::vec3 core,
                  float ring, glm::vec3 sight, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) const {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {time, mode, intensity, opacity};
    d.gpu.params1 = glm::vec4(core, ring);
    d.gpu.color = glm::vec4(color, 1.0f);
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = count;
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
    d.gpu.sightAt = sight;
    // A ring at the feet lies under every card (the user, 2026-10-30: "o poison tinha que estar abaixo de todos os pés").
    if (mode == kModeRing && std::abs(model[3].y - sight.y) < kRingUnderFeet) d.underCards = true;
    d.instances = instances;
    d.blend = VfxBlend::Additive;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
};
// Combat streak (mode 0): a streak along `axis` (zero = a round dot) centred on `centre`.
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}
// Holy mote (mode 2): a soft dot of light facing the camera.
VfxInstance mote(glm::vec3 centre, float size, float alpha, glm::vec3 tint) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(0.0f, 0.0f, alpha, 1.4f), glm::vec4(tint, 0.0f)};
}
glm::mat4 scaled(glm::vec3 p, glm::vec3 s) { return glm::translate(glm::mat4(1.0f), p) * glm::scale(glm::mat4(1.0f), s); }

// The weapon hand of a body at `feet` turned toward `toward`: a little in front of it, a little over half its height.
glm::vec3 weaponHand(glm::vec3 feet, float h, glm::vec3 toward) {
  return feet + flatDir(toward - feet, glm::vec3(0.0f, 0.0f, 1.0f)) * 0.45f + kUp * (h * 0.55f);
}

}  // namespace

float SeleneFx::leadIn(const std::string& kind) { return kind == "arcaneMarkBurst" ? kBurstLead : 0.0f; }

void SeleneFx::bladeGlow(glm::vec3 from, float fromHeight, glm::vec3 toward, float start, float impact) {
  Effect e;
  e.kind = Kind::Blade, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.at = toward;
  e.start = std::min(start, impact - 0.15f), e.impact = impact;
  e.seed = 0x5e1u + 7919u * ++serial_;
  effects_.push_back(e);
}

void SeleneFx::markImpact(const Target& struck, glm::vec3 from, float at) {
  Effect e;
  e.kind = Kind::Mark, e.from = from, e.at = struck.feet, e.height = bodyHeight(struck.height);
  e.start = e.impact = at;
  e.seed = 0x5e2u + 7919u * ++serial_;
  effects_.push_back(e);
}

void SeleneFx::burst(const Target& marked, float impact) {
  Effect e;
  e.kind = Kind::Burst, e.from = marked.feet, e.at = marked.feet, e.height = bodyHeight(marked.height);
  e.start = impact - kBurstLead, e.impact = impact;
  e.seed = 0x5e3u + 7919u * ++serial_;
  effects_.push_back(e);
}

int SeleneFx::standStart(glm::vec3 feet, float height, float now) {
  Standing s;
  s.id = ++standSerial_, s.feet = feet, s.height = bodyHeight(height), s.start = now;
  s.seed = 0x5f0u + 104729u * static_cast<uint32_t>(s.id);
  stands_.push_back(s);
  return s.id;
}

void SeleneFx::standMove(int id, glm::vec3 feet, float height) {
  for (Standing& s : stands_)
    if (s.id == id) s.feet = feet, s.height = bodyHeight(height);
}

void SeleneFx::standEnd(int id, float now) {
  for (Standing& s : stands_)
    if (s.id == id && s.finish < 0.0f) s.finish = now;
}

float SeleneFx::Effect::end() const {
  switch (kind) {
    case Kind::Blade: return impact + 0.25f;
    case Kind::Mark: return impact + 0.9f;
    case Kind::Burst: return impact + 1.1f;
  }
  return impact;
}

bool SeleneFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

float SeleneFx::latestEnd() const {
  float end = -1.0f;
  for (const Effect& e : effects_) end = std::max(end, e.end());
  return end;
}

void SeleneFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  std::erase_if(stands_, [&](const Standing& s) { return s.finish >= 0.0f && now > s.finish + kStandFade; });
  if (effects_.empty() && stands_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  haloFirst_ = frame.addVertices(haloGeometry()), haloCount_ = static_cast<uint32_t>(haloGeometry().size());
  streaks_.clear(), motes_.clear();
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    if (e.kind == Kind::Blade) buildBlade(e, now, camera, lights);
    else if (e.kind == Kind::Mark) buildMark(e, now, camera, frame, lights);
    else buildBurst(e, now, camera, frame, lights);
  }
  for (const Standing& s : stands_) buildStanding(s, now, camera, frame, lights);

  // The particles of everything in play, one draw each.
  const glm::vec3 sight = !effects_.empty() ? effects_.front().at : stands_.front().feet;
  if (!motes_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), motes_.begin(), motes_.end());
    HolyDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kHolyMote, now, 1.0f, 1.0f, kViolet, kCore, 0.0f, sight, first, static_cast<uint32_t>(motes_.size()));
  }
  if (!streaks_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    CombatDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(kCore, 0.0f), sight,
                      VfxBlend::Additive, first, static_cast<uint32_t>(streaks_.size()));
  }
}

// --- the blade lights up ----------------------------------------------------------------------------------------------------

void SeleneFx::buildBlade(const Effect& e, float now, const Camera& camera, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float h = e.fromHeight, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float t = now - e.start;
  const float glow = ease(e.start, e.impact, now) * (1.0f - ease(e.impact, e.impact + 0.2f, now));
  if (glow <= 0.002f) return;
  const glm::vec3 hand = weaponHand(e.from, h, e.at);
  // a dagger: a short bright stroke from the hand toward the foe, a little down, swelling as the blow comes
  const glm::vec3 along = glm::normalize(flatDir(e.at - e.from, glm::vec3(0.0f, 0.0f, 1.0f)) * 0.85f - kUp * 0.2f);
  const float length = 0.55f * k;
  streaks_.push_back(streak(pulled(hand + along * (length * 0.5f), eye, kPull), along * length, 0.15f * k, 0.85f * glow, kViolet, 2.3f));
  streaks_.push_back(streak(pulled(hand + along * (length * 0.5f), eye, kPull), along * length * 0.9f, 0.05f * k, glow, kWhite, 2.6f));
  streaks_.push_back(streak(pulled(hand, eye, kPull), glm::vec3(0.0f), 0.45f * k * (0.7f + 0.3f * glow), 0.55f * glow, kDeep, 1.8f));
  // motes drawn into the blade
  for (int i = 0; i < 12; ++i) {
    const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
    const float phase = std::fmod(p0 + t * 1.8f, 1.0f);
    const float angle = a0 * kTau + t * 2.4f;
    const float radius = glm::mix(0.6f * k * (0.6f + 0.4f * r0), 0.05f, phase);
    const glm::vec3 p = hand + along * (length * r0) + glm::vec3(std::cos(angle) * radius, (r0 - 0.5f) * 0.3f, std::sin(angle) * radius);
    const float alpha = std::sin(kPi * phase) * glow;
    if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.07f * k, alpha, (i & 1) ? kCore : kViolet));
  }
  lights.push_back({hand, kLight, 4.0f * glow, 2.4f * k});
}

// --- the mark laid where the blow landed --------------------------------------------------------------------------------------

void SeleneFx::buildMark(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float age = now - e.impact;
  const glm::vec3 chest = e.at + kUp * (h * 0.5f);
  const glm::vec3 away = flatDir(e.at - e.from, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 side = glm::normalize(glm::cross(kUp, away));
  // the cut: a slanted violet slash across the body, then a flash
  if (age < 0.25f) {
    const float f = 1.0f - ease(0.0f, 0.25f, age);
    const glm::vec3 slash = glm::normalize(side + kUp * 0.7f) * (1.1f * k);
    streaks_.push_back(streak(pulled(chest, eye, kPull), slash, 0.12f * k, f, kViolet, 2.6f));
    streaks_.push_back(streak(pulled(chest, eye, kPull), slash * 0.9f, 0.04f * k, f, kWhite, 3.0f));
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.2f * k * (0.6f + 0.4f * f), 0.45f * f, kViolet, 1.8f));
  }
  // violet shards thrown out, mostly away from Selene
  for (int i = 0; i < 16; ++i) {
    const float a = hash01(e.seed, static_cast<uint32_t>(i), 1), b = hash01(e.seed, static_cast<uint32_t>(i), 2), c = hash01(e.seed, static_cast<uint32_t>(i), 3);
    const float life = 0.35f + 0.3f * c;
    if (age >= life) continue;
    const float s = age / life;
    const float yaw = (a - 0.5f) * 2.4f;
    const glm::vec3 dir = glm::normalize(away * std::cos(yaw) + side * std::sin(yaw) + kUp * (0.2f + 0.8f * b));
    const float speed = 3.0f * (0.6f + 0.6f * c);
    const glm::vec3 p = chest + dir * (speed * age) - kUp * (2.0f * age * age);
    streaks_.push_back(streak(pulled(p, eye, kPull), dir * speed * 0.05f, 0.045f * k, 1.0f - s, (i % 3) ? kViolet : kCore, 2.4f));
  }
  // the rune drawn on the ground: a ring closing in on the feet, the pool lighting under it
  if (age < 0.9f) {
    const float close = easeOut(0.0f, 0.5f, age);
    const float r = glm::mix(1.4f, 0.75f, close) * k;
    const float fade = 1.0f - ease(0.5f, 0.9f, age);
    combat(discFirst_, discCount_, scaled(pulled(e.at + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.55f, 0.75f * fade, kViolet, 0.0f,
           glm::vec4(kCore, 0.9f), e.at, VfxBlend::Additive);
    holy(discFirst_, discCount_, scaled(pulled(e.at + kUp * 0.035f, eye, kGroundPull), {0.8f * k, 1.0f, 0.8f * k}), kHolyPool, age, 0.35f, 0.7f * close * fade,
         kViolet, kCore, 0.8f, e.at);
  }
  const float light = 9.0f * (1.0f - ease(0.0f, 0.5f, age));
  if (light > 0.01f) lights.push_back({chest, kLight, light, 3.0f * k});
}

// --- the burst --------------------------------------------------------------------------------------------------------------

void SeleneFx::buildBurst(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float age = now - e.impact, t = now - e.start;
  const glm::vec3 chest = e.at + kUp * (h * 0.5f);
  // the rune flares under the feet as the burst gathers, then fades
  {
    const float flare = ease(e.start, e.impact, now) * (1.0f - ease(e.impact + 0.1f, e.impact + 0.6f, now));
    if (flare > 0.002f) {
      const float r = 0.8f * k * (0.9f + 0.2f * flare);
      holy(discFirst_, discCount_, scaled(pulled(e.at + kUp * 0.035f, eye, kGroundPull), {r, 1.0f, r}), kHolyPool, t, 0.6f, flare, kViolet, kCore, 0.8f, e.at);
      holy(haloFirst_, haloCount_, scaled(pulled(e.at + kUp * 0.06f, eye, kPull), glm::vec3(r * 0.95f)), kHolyLine, t, 1.0f, flare, kViolet, kCore, 0.0f, e.at);
    }
  }
  const float light = age < 0.0f ? 3.0f + 6.0f * ease(e.start, e.impact, now) : 18.0f * (1.0f - ease(0.0f, 0.5f, age)) + 3.0f * (1.0f - ease(0.5f, 1.0f, age));
  if (light > 0.01f) lights.push_back({chest, kLight, light, 3.8f * k});
  if (age < 0.0f) return;
  // the flash and the shell bursting out of the body
  if (age < 0.2f) {
    const float f = 1.0f - ease(0.0f, 0.2f, age);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.6f * k * (0.6f + 0.4f * f), f, kCore, 3.0f));
  }
  if (age < 0.55f) {
    const float r = (0.4f + 1.6f * easeOut(0.0f, 0.45f, age)) * k;
    combat(sphereFirst_, sphereCount_, scaled(chest, glm::vec3(r, r * 0.9f, r)), kModeEnergy, age, 0.6f, 0.55f * (1.0f - ease(0.1f, 0.55f, age)), kViolet, 0.03f,
           glm::vec4(kCore, 0.0f), e.at, VfxBlend::Additive);
  }
  // shards flying out every way, falling as they fly
  for (int i = 0; i < 26; ++i) {
    const float a = hash01(e.seed, static_cast<uint32_t>(i), 1), b = hash01(e.seed, static_cast<uint32_t>(i), 2), c = hash01(e.seed, static_cast<uint32_t>(i), 3);
    const float life = 0.45f + 0.35f * c;
    if (age >= life) continue;
    const float s = age / life;
    const float yaw = a * kTau;
    const glm::vec3 dir = glm::normalize(glm::vec3(std::cos(yaw), 0.2f + 0.9f * b, std::sin(yaw)));
    const float speed = 4.0f * (0.6f + 0.6f * c);
    const glm::vec3 p = chest + dir * (speed * age) - kUp * (2.4f * age * age);
    const glm::vec3 v = dir * speed - kUp * (4.8f * age);
    streaks_.push_back(streak(pulled(p, eye, kPull), v * 0.05f, 0.05f * k, 1.0f - s, (i % 3) ? kViolet : kWhite, 2.5f));
  }
  // a ring running out over the ground
  if (age < 0.8f) {
    const float r = (0.5f + 2.2f * easeOut(0.0f, 0.6f, age)) * k;
    combat(discFirst_, discCount_, scaled(pulled(e.at + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.6f, 0.8f * (1.0f - ease(0.2f, 0.8f, age)),
           kViolet, 0.0f, glm::vec4(kCore, 0.9f), e.at, VfxBlend::Additive);
  }
}

// --- the mark standing on the foe ---------------------------------------------------------------------------------------------

void SeleneFx::buildStanding(const Standing& s, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = s.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float t = now - s.start;
  const float env = ease(s.start, s.start + kStandFade, now) * (s.finish >= 0.0f ? 1.0f - ease(s.finish, s.finish + kStandFade, now) : 1.0f);
  if (env <= 0.002f) return;
  const float pulse = 0.5f + 0.5f * std::sin(t * 2.4f + static_cast<float>(s.seed & 7u));
  const float r = 0.75f * k;
  // the rune circle at the feet: a soft pool with its ring, a fine band round it
  holy(discFirst_, discCount_, scaled(pulled(s.feet + kUp * 0.035f, eye, kGroundPull), {r, 1.0f, r}), kHolyPool, t, 0.3f, (0.4f + 0.25f * pulse) * env, kDeep, kViolet,
       0.82f, s.feet);
  holy(haloFirst_, haloCount_, scaled(pulled(s.feet + kUp * 0.05f, eye, kPull), glm::vec3(r)), kHolyLine, t, 0.6f, 0.55f * env, kViolet, kCore, 0.0f, s.feet);
  // six points of light running round the ring
  for (int i = 0; i < 6; ++i) {
    const float angle = static_cast<float>(i) / 6.0f * kTau + t * 0.9f;
    const glm::vec3 p = s.feet + glm::vec3(std::cos(angle) * r, 0.08f, std::sin(angle) * r);
    motes_.push_back(mote(pulled(p, eye, kPull), 0.1f * k, (0.6f + 0.4f * pulse) * env, (i & 1) ? kCore : kViolet));
  }
  // a faint violet band about the chest, breathing
  {
    const float br = 0.42f * k * (0.95f + 0.1f * pulse);
    holy(haloFirst_, haloCount_, scaled(pulled(s.feet + kUp * (h * 0.55f), eye, kPull), glm::vec3(br)), kHolyLine, t, 0.45f, (0.25f + 0.25f * pulse) * env, kViolet,
         kCore, 0.0f, s.feet);
  }
  lights.push_back({s.feet + kUp * 0.4f, kLight, (1.2f + 1.0f * pulse) * env, 2.0f * k});
}

}  // namespace ember
