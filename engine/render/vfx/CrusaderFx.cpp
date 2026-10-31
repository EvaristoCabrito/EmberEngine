#include "render/vfx/CrusaderFx.h"

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
constexpr float kPull = 1.3f, kGroundPull = 0.7f;  // as HealerFx: in front of the card, which is drawn 1.2 units nearer
constexpr float kHex = 1.7320508f;                 // the distance between neighbouring hex centres, world units
constexpr float kRingUnderFeet = 0.6f;  // a ring this near its unit's feet height lies on the ground
constexpr float kModeStreak = 0.0f, kModeEnergy = 2.0f, kModeRing = 3.0f;  // Combat material
constexpr float kHolyShaft = 0.0f, kHolyPool = 1.0f, kHolyMote = 2.0f, kHolyLine = 4.0f;  // Holy material
constexpr float kTouchLead = 0.6f, kMantleLead = 0.5f;
constexpr float kMantleSpread = 0.45f;  // the wave takes this long from the caster to the edge
constexpr float kStandFade = 0.6f;      // a standing effect fades in / out over this

constexpr glm::vec3 kGold{1.0f, 0.8f, 0.42f}, kCore{1.0f, 0.97f, 0.86f}, kPale{1.0f, 0.92f, 0.66f}, kEmber{1.0f, 0.64f, 0.24f};

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
// An open cylinder of radius 1 from y 0 to 1 (u around, v up), normals outward: a shaft of light, the outline's shell.
const std::vector<VfxVertex>& columnGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 32, up = 12;
    auto v = [](int i, int j) {
      const float a = static_cast<float>(i) / around * kTau, y = static_cast<float>(j) / up;
      return VfxVertex{{std::cos(a), y, std::sin(a)}, static_cast<float>(i) / around, {std::cos(a), 0.0f, std::sin(a)}, y};
    };
    std::vector<VfxVertex> out;
    for (int i = 0; i < around; ++i)
      for (int j = 0; j < up; ++j) out.insert(out.end(), {v(i, j), v(i + 1, j), v(i, j + 1), v(i + 1, j), v(i + 1, j + 1), v(i, j + 1)});
    return out;
  }();
  return g;
}
// A flat torus of radius 1 lying flat (u around): a halo.
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
glm::mat4 at(glm::vec3 p) { return glm::translate(glm::mat4(1.0f), p); }
glm::mat4 scaled(glm::vec3 p, glm::vec3 s) { return glm::translate(glm::mat4(1.0f), p) * glm::scale(glm::mat4(1.0f), s); }

// The weapon hand of a body at `feet` turned toward `toward`: a little in front of it, a little over half its height.
glm::vec3 weaponHand(glm::vec3 feet, float h, glm::vec3 toward) {
  return feet + flatDir(toward - feet, glm::vec3(0.0f, 0.0f, 1.0f)) * 0.45f + kUp * (h * 0.55f);
}

}  // namespace

float CrusaderFx::leadIn(const std::string& kind) {
  if (kind == "holyTouch") return kTouchLead;
  if (kind == "crusaderMantle") return kMantleLead;
  return 0.0f;  // Golpe Consagrado: the blow lands on the pose's own impact
}

void CrusaderFx::bladeGlow(glm::vec3 from, float fromHeight, glm::vec3 toward, float start, float impact) {
  Effect e;
  e.kind = Kind::Blade, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.at = toward;
  e.start = std::min(start, impact - 0.15f), e.impact = impact;
  e.seed = 0xc1au + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void CrusaderFx::holyImpact(const Target& struck, glm::vec3 from, float at) {
  Effect e;
  e.kind = Kind::Impact, e.from = from, e.at = struck.feet, e.height = bodyHeight(struck.height);
  e.start = e.impact = at;
  e.seed = 0xc2bu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void CrusaderFx::touch(glm::vec3 from, float fromHeight, const Target& ally, float impact) {
  Effect e;
  e.kind = Kind::Touch, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.at = ally.feet, e.height = bodyHeight(ally.height);
  e.start = impact - kTouchLead, e.impact = impact;
  e.seed = 0xc3cu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void CrusaderFx::mantle(glm::vec3 from, float fromHeight, float radius, std::vector<Target> allies, float impact) {
  Effect e;
  e.kind = Kind::Mantle, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.at = from;
  e.radius = std::clamp(radius, 1.2f * kHex, 6.0f * kHex), e.start = impact - kMantleLead, e.impact = impact, e.bodies = std::move(allies);
  e.seed = 0xc4du + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void CrusaderFx::spark(const Target& struck, glm::vec3 from, float at) {
  Effect e;
  e.kind = Kind::Spark, e.from = from, e.at = struck.feet, e.height = bodyHeight(struck.height);
  e.start = e.impact = at;
  e.seed = 0xc5eu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

int CrusaderFx::standStart(Stand kind, glm::vec3 feet, float height, float now) {
  Standing s;
  s.kind = kind, s.id = ++standSerial_, s.feet = feet, s.height = bodyHeight(height), s.start = now;
  s.seed = 0xd00u + 104729u * static_cast<uint32_t>(s.id);
  stands_.push_back(s);
  return s.id;
}

void CrusaderFx::standMove(int id, glm::vec3 feet, float height) {
  for (Standing& s : stands_)
    if (s.id == id) s.feet = feet, s.height = bodyHeight(height);
}

void CrusaderFx::standEnd(int id, float now) {
  for (Standing& s : stands_)
    if (s.id == id && s.finish < 0.0f) s.finish = now;
}

float CrusaderFx::Effect::end() const {
  switch (kind) {
    case Kind::Blade: return impact + 0.25f;
    case Kind::Impact: return impact + 0.9f;
    case Kind::Touch: return impact + 1.8f;
    case Kind::Mantle: return impact + 1.6f;
    case Kind::Spark: return impact + 0.5f;
  }
  return impact;
}

bool CrusaderFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

float CrusaderFx::latestEnd() const {
  float end = -1.0f;
  for (const Effect& e : effects_) end = std::max(end, e.end());
  return end;
}

void CrusaderFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  std::erase_if(stands_, [&](const Standing& s) { return s.finish >= 0.0f && now > s.finish + kStandFade; });
  if (effects_.empty() && stands_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  columnFirst_ = frame.addVertices(columnGeometry()), columnCount_ = static_cast<uint32_t>(columnGeometry().size());
  haloFirst_ = frame.addVertices(haloGeometry()), haloCount_ = static_cast<uint32_t>(haloGeometry().size());
  streaks_.clear(), motes_.clear();
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    if (e.kind == Kind::Blade) buildBlade(e, now, camera, frame, lights);
    else if (e.kind == Kind::Impact || e.kind == Kind::Spark) buildImpact(e, now, camera, frame, lights);
    else if (e.kind == Kind::Touch) buildTouch(e, now, camera, frame, lights);
    else buildMantle(e, now, camera, frame, lights);
  }
  for (const Standing& s : stands_) buildStanding(s, now, camera, frame, lights);

  // The particles of everything in play, one draw each.
  const glm::vec3 sight = !effects_.empty() ? effects_.front().at : stands_.front().feet;
  if (!motes_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), motes_.begin(), motes_.end());
    HolyDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kHolyMote, now, 1.0f, 1.0f, kGold, kCore, 0.0f, sight, first, static_cast<uint32_t>(motes_.size()));
  }
  if (!streaks_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    CombatDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(1.0f, 0.95f, 0.82f, 0.0f), sight,
                      VfxBlend::Additive, first, static_cast<uint32_t>(streaks_.size()));
  }
}

// --- Golpe Consagrado: the blade lights up --------------------------------------------------------------------------------

void CrusaderFx::buildBlade(const Effect& e, float now, const Camera& camera, VfxFrame&, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float h = e.fromHeight, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float t = now - e.start;
  const float glow = ease(e.start, e.impact, now) * (1.0f - ease(e.impact, e.impact + 0.2f, now));
  if (glow <= 0.002f) return;
  const glm::vec3 hand = weaponHand(e.from, h, e.at);
  // the blade: a bright stroke from the hand upward and toward the foe, swelling as the blow comes
  const glm::vec3 along = glm::normalize(kUp * 0.8f + flatDir(e.at - e.from, glm::vec3(0.0f, 0.0f, 1.0f)) * 0.6f);
  const float length = 0.9f * k;
  streaks_.push_back(streak(pulled(hand + along * (length * 0.5f), eye, kPull), along * length, 0.18f * k, 0.85f * glow, kGold, 2.2f));
  streaks_.push_back(streak(pulled(hand + along * (length * 0.5f), eye, kPull), along * length * 0.9f, 0.07f * k, glow, kCore, 2.6f));
  streaks_.push_back(streak(pulled(hand, eye, kPull), glm::vec3(0.0f), 0.55f * k * (0.7f + 0.3f * glow), 0.6f * glow, kGold, 1.8f));
  // motes drawn into the blade
  for (int i = 0; i < 14; ++i) {
    const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
    const float phase = std::fmod(p0 + t * 1.6f, 1.0f);
    const float angle = a0 * kTau + t * 2.0f;
    const float radius = glm::mix(0.7f * k * (0.6f + 0.4f * r0), 0.05f, phase);
    const glm::vec3 p = hand + along * (length * r0) + glm::vec3(std::cos(angle) * radius, (r0 - 0.5f) * 0.3f, std::sin(angle) * radius);
    const float alpha = std::sin(kPi * phase) * glow;
    if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.08f * k, alpha, (i & 1) ? kPale : kGold));
  }
  lights.push_back({hand, {1.0f, 0.82f, 0.45f}, 5.0f * glow, 2.6f * k});
}

// --- Golpe Consagrado's flash where the blow landed; the Manto's spark on a blow ----------------------------------------

void CrusaderFx::buildImpact(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const glm::vec3 eye = camera.position;
  const bool small = e.kind == Kind::Spark;
  const float h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f) * (small ? 0.55f : 1.0f);
  const float age = now - e.impact;
  const glm::vec3 chest = e.at + kUp * (e.height * 0.5f);
  const glm::vec3 away = flatDir(e.at - e.from, glm::vec3(0.0f, 0.0f, 1.0f));
  // the flash
  if (age < 0.22f) {
    const float f = 1.0f - ease(0.0f, 0.22f, age);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.3f * k * (0.6f + 0.4f * f), f, kCore, 2.8f));
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 2.0f * k * (0.6f + 0.4f * f), 0.5f * f, kGold, 1.8f));
  }
  // gold sparks thrown out, mostly away from the striker, falling as they fly
  const int sparks = small ? 8 : 22;
  for (int i = 0; i < sparks; ++i) {
    const float a = hash01(e.seed, static_cast<uint32_t>(i), 1), b = hash01(e.seed, static_cast<uint32_t>(i), 2), c = hash01(e.seed, static_cast<uint32_t>(i), 3);
    const float life = (small ? 0.3f : 0.45f) + 0.3f * c;
    if (age >= life) continue;
    const float s = age / life;
    const float yaw = (a - 0.5f) * 2.4f;
    const glm::vec3 side = glm::normalize(glm::cross(kUp, away));
    const glm::vec3 dir = glm::normalize(away * std::cos(yaw) + side * std::sin(yaw) + kUp * (0.3f + 0.9f * b));
    const float speed = (small ? 2.2f : 3.6f) * (0.6f + 0.6f * c);
    const glm::vec3 p = chest + dir * (speed * age) - kUp * (2.2f * age * age);
    const glm::vec3 v = dir * speed - kUp * (4.4f * age);
    streaks_.push_back(streak(pulled(p, eye, kPull), v * 0.05f, 0.05f * k, 1.0f - s, (i % 3) ? kGold : kCore, 2.4f));
  }
  // a ring on the ground at the feet
  if (!small && age < 0.6f) {
    const float r = 0.3f + 1.1f * k * easeOut(0.0f, 0.5f, age);
    combat(discFirst_, discCount_, scaled(pulled(e.at + kUp * 0.04f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.5f, 0.7f * (1.0f - ease(0.15f, 0.6f, age)),
           kGold, 0.0f, glm::vec4(kCore, 0.9f), e.at, VfxBlend::Additive);
  }
  const float light = (small ? 4.0f : 14.0f) * (1.0f - ease(0.0f, small ? 0.3f : 0.6f, age));
  if (light > 0.01f) lights.push_back({chest, {1.0f, 0.82f, 0.45f}, light, (small ? 2.0f : 3.5f) * k});
}

// --- Toque Sagrado ------------------------------------------------------------------------------------------------------

void CrusaderFx::buildTouch(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float age = now - e.impact, t = now - e.start;
  const float gather = ease(e.start, e.impact, now);
  // the hand at the ally's shoulder: on the ally's side toward the caster, at its shoulder
  const glm::vec3 toCaster = flatDir(e.from - e.at, glm::vec3(1.0f, 0.0f, 0.0f));
  const glm::vec3 shoulder = e.at + toCaster * 0.32f + kUp * (h * 0.78f);
  {
    const float glow = gather * (1.0f - ease(e.impact + 0.3f, e.impact + 0.9f, now));
    if (glow > 0.002f) {
      streaks_.push_back(streak(pulled(shoulder, eye, kPull), glm::vec3(0.0f), 0.6f * k * (0.7f + 0.3f * gather), 0.75f * glow, kGold, 2.0f));
      streaks_.push_back(streak(pulled(shoulder, eye, kPull), glm::vec3(0.0f), 0.25f * k, glow, kCore, 2.6f));
      lights.push_back({shoulder, {1.0f, 0.84f, 0.5f}, 4.0f * glow, 2.4f * k});
    }
  }
  if (age < 0.0f) return;
  const float env = ease(0.0f, 0.2f, age) * (1.0f - ease(1.0f, 1.8f, age));
  // a faint shaft round the body, rising from the feet
  if (env > 0.002f) {
    const float H = h * 1.25f * easeOut(0.0f, 0.6f, age) + 1e-3f, r = 0.36f * k;
    holy(columnFirst_, columnCount_, scaled(pulled(e.at, eye, kPull * 0.5f), {r, H, r}), kHolyShaft, t, 0.22f, 0.55f * env, kGold, kCore, 0.0f, e.at);
    const float radius = 0.7f * k * (0.6f + 0.4f * ease(0.0f, 0.4f, age));
    holy(discFirst_, discCount_, scaled(pulled(e.at + kUp * 0.035f, eye, kGroundPull), {radius, 1.0f, radius}), kHolyPool, t, 0.35f, 0.8f * env, kGold, kCore,
         0.3f + 0.5f * ease(0.0f, 0.4f, age), e.at);
  }
  // the halo climbing from the feet past the head
  {
    const float climb = easeOut(0.0f, 1.2f, age), haloEnv = ease(0.0f, 0.15f, age) * (1.0f - ease(0.9f, 1.4f, age));
    if (haloEnv > 0.002f) {
      const float y = glm::mix(0.1f, h * 1.12f, climb), radius = glm::mix(0.68f, 0.42f, climb) * k;
      holy(haloFirst_, haloCount_, scaled(pulled(e.at + kUp * y, eye, kPull), glm::vec3(radius)), kHolyLine, age, 0.8f, haloEnv, kPale, kCore, 0.0f, e.at);
    }
  }
  // motes going up the body
  for (int i = 0; i < 18 && env > 0.0f; ++i) {
    const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
    const float phase = std::fmod(p0 + age * 0.55f, 1.0f);
    const float angle = a0 * kTau + age * (a0 > 0.5f ? 1.4f : -1.4f);
    const float radius = (0.3f + 0.7f * r0) * 0.34f * k;
    const glm::vec3 p = e.at + glm::vec3(std::cos(angle) * radius, phase * h * 1.25f, std::sin(angle) * radius);
    const float alpha = std::sin(kPi * phase) * env;
    if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.09f * k * (1.0f - 0.4f * phase), alpha, (i & 1) ? kPale : kGold));
  }
  if (env > 0.0f) lights.push_back({e.at + kUp * (h * 0.5f), {1.0f, 0.82f, 0.5f}, 5.0f * env, 2.8f * k});
}

// --- Manto do Cruzado ---------------------------------------------------------------------------------------------------

void CrusaderFx::buildMantle(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float R = e.radius, h = e.fromHeight, kc = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float age = now - e.impact, t = now - e.start;
  const float gather = ease(e.start, e.impact, now);
  // the light gathers round him: motes drawn to his chest, a glow under his feet
  {
    const float env = gather * (1.0f - ease(e.impact, e.impact + 0.2f, now));
    const glm::vec3 chest = e.from + kUp * (h * 0.55f);
    for (int i = 0; i < 20 && env > 0.0f; ++i) {
      const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
      const float phase = std::fmod(p0 + t * 1.2f, 1.0f);
      const float angle = a0 * kTau + t * 1.5f;
      const float radius = glm::mix(1.2f * kc * (0.6f + 0.4f * r0), 0.08f, phase);
      const glm::vec3 p = chest + glm::vec3(std::cos(angle) * radius, (r0 - 0.5f) * h * 0.6f * (1.0f - phase), std::sin(angle) * radius);
      const float alpha = std::sin(kPi * phase) * env;
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.09f * kc, alpha, (i & 1) ? kPale : kGold));
    }
    if (env > 0.002f) {
      const float r = 0.8f * kc * (0.6f + 0.4f * gather);
      holy(discFirst_, discCount_, scaled(pulled(e.from + kUp * 0.04f, eye, kGroundPull), {r, 1.0f, r}), kHolyPool, t, 0.4f, 0.7f * env, kGold, kCore, 0.4f, e.from);
    }
  }
  const float light = age < 0.0f ? 2.0f + 5.0f * gather : 12.0f * (1.0f - ease(0.0f, 0.5f, age)) + 3.0f * (1.0f - ease(0.5f, 1.4f, age));
  if (light > 0.01f) lights.push_back({e.from + kUp * 1.0f, {1.0f, 0.82f, 0.48f}, light, glm::mix(2.5f, R * 1.1f, age >= 0.0f ? ease(0.0f, kMantleSpread, age) : 0.0f)});
  if (age < 0.0f) return;
  // the wave: a low shell and a ring running out across the radius, a pool under it
  if (age < kMantleSpread + 0.2f) {
    const float r = 0.3f + (R - 0.3f) * easeOut(0.0f, kMantleSpread + 0.05f, age);
    combat(sphereFirst_, sphereCount_, scaled(e.from + kUp * 0.3f, glm::vec3(r, r * 0.35f, r)), kModeEnergy, age, 0.45f,
           0.4f * (1.0f - ease(0.1f, kMantleSpread + 0.2f, age)), kGold, 0.03f, glm::vec4(kCore, 0.0f), e.from, VfxBlend::Additive);
  }
  if (age < 0.9f) {
    const float r = 0.4f + (R * 1.02f - 0.4f) * easeOut(0.0f, 0.6f, age);
    combat(discFirst_, discCount_, scaled(pulled(e.from + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.55f, 0.75f * (1.0f - ease(0.25f, 0.9f, age)),
           kGold, 0.0f, glm::vec4(kCore, 0.9f), e.from, VfxBlend::Additive);
    holy(discFirst_, discCount_, scaled(pulled(e.from + kUp * 0.03f, eye, kGroundPull), {r, 1.0f, r}), kHolyPool, age, 0.25f, 0.5f * (1.0f - ease(0.2f, 0.9f, age)),
         kGold, kCore, 0.85f, e.from);
  }
  // every ally it reaches flashes gold as the wave passes
  int lit = 0;
  for (size_t bi = 0; bi < e.bodies.size(); ++bi) {
    const Target& u = e.bodies[bi];
    const float bh = bodyHeight(u.height), k = std::clamp(bh / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(u.feet.x - e.from.x, u.feet.z - e.from.z));
    const float tau = age - std::min(dist / R, 1.0f) * kMantleSpread;
    if (tau < 0.0f || tau > 1.1f) continue;
    const float env = ease(0.0f, 0.1f, tau) * (1.0f - ease(0.4f, 1.1f, tau));
    const glm::vec3 chest = u.feet + kUp * (bh * 0.5f);
    holy(columnFirst_, columnCount_, scaled(pulled(u.feet, eye, kPull * 0.5f), {0.4f * k, bh * 1.15f, 0.4f * k}), kHolyShaft, tau, 0.3f, 0.6f * env, kGold, kCore, 0.0f, u.feet);
    if (tau < 0.25f) {
      const float f = 1.0f - ease(0.0f, 0.25f, tau);
      streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.0f * k * (0.5f + 0.5f * f), 0.65f * f, kCore, 2.0f));
    }
    if (lit < 4 && env > 0.0f) {
      ++lit;
      lights.push_back({chest, {1.0f, 0.82f, 0.48f}, 5.0f * env, 2.6f * k});
    }
  }
}

// --- Standing: the Juramento's aura, the Manto's outline ----------------------------------------------------------------

void CrusaderFx::buildStanding(const Standing& s, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = s.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float t = now - s.start;
  const float env = ease(s.start, s.start + kStandFade, now) * (s.finish >= 0.0f ? 1.0f - ease(s.finish, s.finish + kStandFade, now) : 1.0f);
  if (env <= 0.002f) return;
  if (s.kind == Stand::Mantle) {
    // a faint gold outline: a thin shell of light standing round the body, shimmering, a fine ring at the feet
    const float shimmer = 0.85f + 0.15f * std::sin(t * 3.1f + static_cast<float>(s.seed & 7u));
    holy(columnFirst_, columnCount_, scaled(pulled(s.feet, eye, kPull * 0.5f), {0.42f * k, h * 1.08f, 0.42f * k}), kHolyShaft, t, 0.12f, 0.32f * env * shimmer, kGold,
         kPale, 0.0f, s.feet);
    holy(haloFirst_, haloCount_, scaled(pulled(s.feet + kUp * 0.06f, eye, kPull), glm::vec3(0.5f * k)), kHolyLine, t, 0.5f, 0.45f * env, kGold, kCore, 0.0f, s.feet);
    return;
  }
  // the Juramento: a faint aura pulsing round him, gold embers rising off it, a soft light
  const float pulse = 0.5f + 0.5f * std::sin(t * 2.6f);
  holy(columnFirst_, columnCount_, scaled(pulled(s.feet, eye, kPull * 0.5f), {0.5f * k, h * 1.15f, 0.5f * k}), kHolyShaft, t, 0.16f, (0.18f + 0.18f * pulse) * env, kGold,
       kCore, 0.0f, s.feet);
  {
    const float r = 0.7f * k * (0.92f + 0.12f * pulse);
    holy(discFirst_, discCount_, scaled(pulled(s.feet + kUp * 0.035f, eye, kGroundPull), {r, 1.0f, r}), kHolyPool, t, 0.3f, (0.35f + 0.3f * pulse) * env, kGold, kCore,
         0.45f + 0.3f * pulse, s.feet);
  }
  for (int i = 0; i < 16; ++i) {
    const float a0 = hash01(s.seed, static_cast<uint32_t>(i), 1), r0 = hash01(s.seed, static_cast<uint32_t>(i), 2), p0 = hash01(s.seed, static_cast<uint32_t>(i), 3);
    const float phase = std::fmod(p0 + t * (0.32f + 0.18f * r0), 1.0f);
    const float angle = a0 * kTau + t * 0.7f + phase * 1.5f;
    const float radius = (0.25f + 0.6f * r0) * 0.45f * k * (1.0f + 0.3f * phase);
    const glm::vec3 p = s.feet + glm::vec3(std::cos(angle) * radius, 0.15f + phase * h * 1.35f, std::sin(angle) * radius);
    const float alpha = std::sin(kPi * phase) * env * 0.9f;
    if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.07f * k * (1.0f - 0.5f * phase), alpha, (i % 3) ? kEmber : kGold));
  }
  lights.push_back({s.feet + kUp * (h * 0.5f), {1.0f, 0.78f, 0.42f}, (1.5f + 1.5f * pulse) * env, 2.2f * k});
}

}  // namespace ember
