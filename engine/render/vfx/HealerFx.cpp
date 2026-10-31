#include "render/vfx/HealerFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPi;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
constexpr float kPull = 1.3f, kGroundPull = 0.7f;  // as CombatFx / TurnUndead: in front of the card, which is drawn 1.2 units nearer
constexpr float kHex = 1.7320508f;                 // the distance between neighbouring hex centres, world units
constexpr float kRingUnderFeet = 0.6f;  // a ring this near its unit's feet height lies on the ground
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f;  // Combat material
constexpr float kHolyShaft = 0.0f, kHolyPool = 1.0f, kHolyMote = 2.0f, kHolyTube = 3.0f, kHolyLine = 4.0f;  // Holy material
constexpr float kCircleLead = 0.5f, kResurrectionLead = 0.9f, kJudgmentLead = 0.6f;
constexpr float kCircleSpread = 0.4f, kJudgmentSpread = 0.35f;  // the wave takes this long from the centre to the edge

constexpr glm::vec3 kGold{1.0f, 0.82f, 0.48f}, kCore{1.0f, 0.97f, 0.88f}, kPale{1.0f, 0.93f, 0.7f};
constexpr glm::vec3 kWhite{1.0f, 0.95f, 0.82f}, kFire{1.0f, 0.72f, 0.3f};
constexpr glm::vec3 kDust{0.42f, 0.36f, 0.28f};

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
// An open cylinder of radius 1 from y 0 to 1 (u around, v up), normals outward: a shaft of light.
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
// The blade of Julgamento (radius 1 at its widest, from y 0 to 1): a tapered column, a point at its foot, a wide tang above.
const std::vector<VfxVertex>& bladeGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 24, up = 16;
    auto width = [](float y) {
      const float tip = std::clamp(y / 0.12f, 0.0f, 1.0f);        // the point
      const float top = 1.0f - std::clamp((y - 0.86f) / 0.14f, 0.0f, 1.0f) * 0.65f;  // the tang narrows
      return tip * top;
    };
    auto v = [&](int i, int j) {
      const float a = static_cast<float>(i) / around * kTau, y = static_cast<float>(j) / up;
      const float w = width(y);
      // flat: wider across x than z (a blade, not a rod)
      return VfxVertex{{std::cos(a) * w, y, std::sin(a) * w * 0.28f}, static_cast<float>(i) / around, {std::cos(a), 0.0f, std::sin(a)}, y};
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
// A flat ribbon on y 0 from a to b (xz), `half` wide each side, u along it from u0 to u1, v across.
void ribbon(std::vector<VfxVertex>& out, glm::vec2 a, glm::vec2 b, float half, float u0, float u1) {
  const glm::vec2 d = b - a;
  const float len = glm::length(d);
  if (len < 1e-6f) return;
  const glm::vec2 n = glm::vec2(-d.y, d.x) / len * half;
  auto vtx = [](glm::vec2 p, float u, float v) { return VfxVertex{{p.x, 0.0f, p.y}, u, {0.0f, 1.0f, 0.0f}, v}; };
  const VfxVertex a0 = vtx(a - n, u0, 0.0f), a1 = vtx(a + n, u0, 1.0f), b0 = vtx(b - n, u1, 0.0f), b1 = vtx(b + n, u1, 1.0f);
  out.insert(out.end(), {a0, a1, b0, a1, b1, b0});
}
void circle(std::vector<VfxVertex>& out, float radius, float half, int segments) {
  for (int i = 0; i < segments; ++i) {
    const float a0 = static_cast<float>(i) / segments * kTau, a1 = static_cast<float>(i + 1) / segments * kTau;
    ribbon(out, {std::cos(a0) * radius, std::sin(a0) * radius}, {std::cos(a1) * radius, std::sin(a1) * radius}, half, static_cast<float>(i) / segments,
           static_cast<float>(i + 1) / segments);
  }
}
// The sigil of the Círculo (radius 1): the outer ring, a ring inside, six rays to the rim between six small circles.
const std::vector<VfxVertex>& sigilGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> out;
    circle(out, 1.0f, 0.016f, 96);
    circle(out, 0.62f, 0.008f, 64);
    for (int i = 0; i < 6; ++i) {
      const float a = static_cast<float>(i) / 6.0f * kTau;
      ribbon(out, {std::cos(a) * 0.3f, std::sin(a) * 0.3f}, {std::cos(a) * 0.96f, std::sin(a) * 0.96f}, 0.009f, 0.0f, 1.0f);
      const float b = (static_cast<float>(i) + 0.5f) / 6.0f * kTau;
      const glm::vec2 c{std::cos(b) * 0.8f, std::sin(b) * 0.8f};
      for (int k = 0; k < 16; ++k) {
        const float c0 = static_cast<float>(k) / 16.0f * kTau, c1 = static_cast<float>(k + 1) / 16.0f * kTau;
        ribbon(out, c + glm::vec2(std::cos(c0), std::sin(c0)) * 0.08f, c + glm::vec2(std::cos(c1), std::sin(c1)) * 0.08f, 0.007f, 0.0f, 1.0f);
      }
    }
    return out;
  }();
  return g;
}
// The star (radius 1): a hexagram and the ring inside it (Ressurreição's mark under the body, Julgamento's sigil in the air).
const std::vector<VfxVertex>& starGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> out;
    constexpr float kStar = 0.9f;
    for (int tri = 0; tri < 2; ++tri)
      for (int i = 0; i < 3; ++i) {
        const float a0 = (static_cast<float>(i) * 2.0f + static_cast<float>(tri)) / 6.0f * kTau, a1 = (static_cast<float>(i + 1) * 2.0f + static_cast<float>(tri)) / 6.0f * kTau;
        ribbon(out, {std::cos(a0) * kStar, std::sin(a0) * kStar}, {std::cos(a1) * kStar, std::sin(a1) * kStar}, 0.014f, 0.0f, 1.0f);
      }
    circle(out, kStar / std::sqrt(3.0f), 0.01f, 64);
    circle(out, 1.0f, 0.012f, 96);
    return out;
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
VfxInstance puff(glm::vec3 centre, float size, float rotation, float seed, float alpha, glm::vec3 albedo) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(rotation, seed, alpha, 0.0f), glm::vec4(albedo, 0.0f)};
}
// Holy mote (mode 2): a soft dot of light facing the camera.
VfxInstance mote(glm::vec3 centre, float size, float alpha, glm::vec3 tint) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(0.0f, 0.0f, alpha, 1.4f), glm::vec4(tint, 0.0f)};
}
glm::mat4 at(glm::vec3 p) { return glm::translate(glm::mat4(1.0f), p); }
glm::mat4 scaled(glm::vec3 p, glm::vec3 s) { return glm::translate(glm::mat4(1.0f), p) * glm::scale(glm::mat4(1.0f), s); }

}  // namespace

float HealerFx::leadIn(const std::string& kind) {
  if (kind == "resurrection") return kResurrectionLead;
  if (kind == "judgment") return kJudgmentLead;
  return kCircleLead;
}

void HealerFx::circle(glm::vec3 from, float fromHeight, glm::vec3 centre, float radius, std::vector<Target> allies, float impact) {
  Effect e;
  e.kind = Kind::Circle, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.centre = centre;
  e.radius = std::clamp(radius, 1.2f * kHex, 6.0f * kHex), e.impact = impact, e.bodies = std::move(allies);
  e.seed = 0x5a1u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void HealerFx::resurrection(glm::vec3 from, float fromHeight, const Target& fallen, float impact) {
  Effect e;
  e.kind = Kind::Resurrection, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.centre = fallen.feet, e.height = bodyHeight(fallen.height);
  e.radius = 1.6f, e.impact = impact, e.bodies = {fallen};
  e.seed = 0x4e5u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void HealerFx::judgment(glm::vec3 from, float fromHeight, glm::vec3 centre, float radius, std::vector<Target> foes, float impact) {
  Effect e;
  e.kind = Kind::Judgment, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.centre = centre;
  e.radius = std::clamp(radius, 1.2f * kHex, 6.0f * kHex), e.impact = impact, e.bodies = std::move(foes);
  e.seed = 0x7d6u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

float HealerFx::Effect::end() const {
  switch (kind) {
    case Kind::Circle: return impact + 2.2f;
    case Kind::Resurrection: return impact + 2.6f;
    case Kind::Judgment: return impact + 2.3f;
  }
  return impact;
}

bool HealerFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

float HealerFx::latestEnd() const {
  float end = -1.0f;
  for (const Effect& e : effects_) end = std::max(end, e.end());
  return end;
}

void HealerFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  columnFirst_ = frame.addVertices(columnGeometry()), columnCount_ = static_cast<uint32_t>(columnGeometry().size());
  haloFirst_ = frame.addVertices(haloGeometry()), haloCount_ = static_cast<uint32_t>(haloGeometry().size());
  sigilFirst_ = frame.addVertices(sigilGeometry()), sigilCount_ = static_cast<uint32_t>(sigilGeometry().size());
  starFirst_ = frame.addVertices(starGeometry()), starCount_ = static_cast<uint32_t>(starGeometry().size());
  bladeFirst_ = frame.addVertices(bladeGeometry()), bladeCount_ = static_cast<uint32_t>(bladeGeometry().size());
  streaks_.clear(), puffs_.clear(), motes_.clear();
  for (const Effect& e : effects_) {
    const float lead = e.kind == Kind::Circle ? kCircleLead : e.kind == Kind::Resurrection ? kResurrectionLead : kJudgmentLead;
    if (now < e.impact - lead) continue;
    if (e.kind == Kind::Circle) buildCircle(e, now, camera, frame, lights);
    else if (e.kind == Kind::Resurrection) buildResurrection(e, now, camera, frame, lights);
    else buildJudgment(e, now, camera, frame, lights);
  }

  // The particles of everything in play, one draw each: the dust first (it covers), then the light.
  const glm::vec3 sight = effects_.front().centre;
  if (!puffs_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    CombatDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
                      static_cast<uint32_t>(puffs_.size()));
  }
  if (!motes_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), motes_.begin(), motes_.end());
    HolyDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kHolyMote, now, 1.0f, 1.0f, kGold, kCore, 0.0f, sight, first, static_cast<uint32_t>(motes_.size()));
  }
  if (!streaks_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    CombatDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(1.0f, 0.97f, 0.9f, 0.0f), sight,
                      VfxBlend::Additive, first, static_cast<uint32_t>(streaks_.size()));
  }
}

// --- Círculo de Cura --------------------------------------------------------------------------------------------------

void HealerFx::buildCircle(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float R = e.radius;
  const float begin = e.impact - kCircleLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);

  // --- The sigil on the ground over the whole area: it draws itself in while the light gathers, turns slowly, flares with the
  //     wave and fades. A pool of light under it. ---
  {
    const float appear = ease(begin, begin + 0.4f, now), fade = 1.0f - ease(e.impact + 1.0f, e.impact + 2.0f, now);
    const float flare = age >= 0.0f ? 1.0f + 1.0f * (1.0f - ease(0.0f, 0.5f, age)) : 1.0f + 0.3f * gather;
    const float opacity = appear * fade;
    if (opacity > 0.002f) {
      const float rad = R * (0.3f + 0.7f * easeOut(begin, begin + 0.5f, now));
      const glm::vec3 p = pulled(e.centre + kUp * 0.05f, eye, kGroundPull);
      holy(sigilFirst_, sigilCount_, at(p) * glm::rotate(glm::mat4(1.0f), t * 0.18f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rad)), kHolyLine, t, 0.8f * flare,
           opacity, kGold, kCore, 0.0f, e.centre);
      const float ring = age >= 0.0f ? 0.25f + 0.75f * easeOut(0.0f, kCircleSpread, age) : 0.25f;
      holy(discFirst_, discCount_, scaled(p, {rad, 1.0f, rad}), kHolyPool, t, 0.4f * flare, 0.65f * opacity, kGold, kCore, ring, e.centre);
    }
  }

  // --- Motes gather over the sigil while the light builds: rising from the ground across the area. ---
  {
    const float env = gather * (1.0f - ease(e.impact, e.impact + 0.3f, now));
    for (int i = 0; i < 36 && env > 0.0f; ++i) {
      const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
      const float phase = std::fmod(p0 + t * 0.7f, 1.0f);
      const float angle = a0 * kTau + t * 0.6f;
      const float radius = R * (0.15f + 0.85f * r0);
      const glm::vec3 p = e.centre + glm::vec3(std::cos(angle) * radius, 0.1f + phase * 1.4f, std::sin(angle) * radius);
      const float alpha = std::sin(kPi * phase) * env;
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.11f * (1.0f - 0.3f * phase), alpha, (i & 1) ? kPale : kGold));
    }
  }

  // --- The light at the centre: it swells, flares as the wave leaves, settles and goes. ---
  {
    const float swell = 2.0f + 6.0f * gather;
    const float burst = age >= 0.0f ? 16.0f * (1.0f - ease(0.0f, 0.4f, age)) + 5.0f * (1.0f - ease(0.4f, 1.8f, age)) : 0.0f;
    const float intensity = age >= 0.0f ? burst : swell;
    const float reach = glm::mix(2.5f, R * 1.2f, age >= 0.0f ? ease(0.0f, kCircleSpread, age) : 0.0f);
    if (intensity > 0.01f) lights.push_back({e.centre + kUp * 1.0f, {1.0f, 0.84f, 0.55f}, intensity, reach});
  }

  if (age >= 0.0f) {
    // --- The wave: a flash low over the centre, a shell of light running out to the edge, a ring across the ground. ---
    if (age < 0.2f) {
      const float f = 1.0f - ease(0.0f, 0.2f, age);
      streaks_.push_back(streak(pulled(e.centre + kUp * 0.6f, eye, kGroundPull), glm::vec3(0.0f), 1.4f * (0.6f + 0.4f * f), f, kCore, 2.2f));
    }
    if (age < kCircleSpread + 0.15f) {
      const float r = 0.3f + (R - 0.3f) * easeOut(0.0f, kCircleSpread + 0.05f, age);
      combat(sphereFirst_, sphereCount_, scaled(e.centre + kUp * 0.3f, glm::vec3(r, r * 0.45f, r)), kModeEnergy, age, 0.5f,
             0.45f * (1.0f - ease(0.1f, kCircleSpread + 0.15f, age)), kGold, 0.03f, glm::vec4(kCore, 0.0f), e.centre, VfxBlend::Additive);
    }
    if (age < 0.8f) {
      const float r = 0.4f + (R * 1.02f - 0.4f) * easeOut(0.0f, 0.55f, age);
      combat(discFirst_, discCount_, scaled(pulled(e.centre + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.55f, 0.75f * (1.0f - ease(0.2f, 0.8f, age)),
             kGold, 0.0f, glm::vec4(kCore, 0.9f), e.centre, VfxBlend::Additive);
    }
  }

  // --- On every ally the wave reaches: a shaft of warm light comes down, a halo settles from the head to the feet, motes rise,
  //     the ground glows, a light on it. ---
  int lit = 0;
  for (size_t bi = 0; bi < e.bodies.size(); ++bi) {
    const Target& u = e.bodies[bi];
    const float h = bodyHeight(u.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(u.feet.x - e.centre.x, u.feet.z - e.centre.z));
    const float tau = age - std::min(dist / R, 1.0f) * kCircleSpread;
    if (tau < 0.0f) continue;
    const uint32_t seed = e.seed + static_cast<uint32_t>(bi) * 7919u;
    const glm::vec3 chest = u.feet + kUp * (h * 0.5f);
    const float env = ease(0.0f, 0.12f, tau) * (1.0f - ease(0.9f, 1.7f, tau));
    if (env > 0.002f) {
      // the shaft: it comes down over 0.25 s, holds, fades
      const float H = h * 1.5f, r = 0.32f * k * (1.0f + 0.04f * std::sin(tau * 6.0f + static_cast<float>(bi)));
      const float lowered = 1.0f - easeOut(0.0f, 0.25f, tau);
      const glm::vec3 base = pulled(u.feet, eye, kPull * 0.5f) + kUp * (lowered * H);
      holy(columnFirst_, columnCount_, scaled(base, {r, H * (1.0f - lowered) + 1e-3f, r}), kHolyShaft, tau, 0.3f, env * (0.4f + 0.6f * ease(0.0f, 0.25f, tau)), kGold, kCore,
           0.0f, u.feet);
      holy(columnFirst_, columnCount_, scaled(base, {r * 0.45f, H * (1.0f - lowered) * 0.85f + 1e-3f, r * 0.45f}), kHolyShaft, tau, 0.26f, env * 0.7f,
           glm::mix(kGold, kCore, 0.5f), kCore, 0.0f, u.feet);
      // the ground under it
      const float grow = ease(0.0f, 0.4f, tau);
      const float radius = 0.75f * k * (0.6f + 0.4f * grow);
      holy(discFirst_, discCount_, scaled(pulled(u.feet + kUp * 0.035f, eye, kGroundPull), {radius, 1.0f, radius}), kHolyPool, tau, 0.4f, env, kGold, kCore,
           0.25f + 0.6f * grow, u.feet);
    }
    // the halo settling from above the head to the feet
    {
      const float settle = ease(0.0f, 0.9f, tau), haloEnv = ease(0.0f, 0.15f, tau) * (1.0f - ease(1.0f, 1.5f, tau));
      if (haloEnv > 0.002f) {
        const float y = glm::mix(h * 1.15f, 0.08f, settle), radius = glm::mix(0.5f, 0.72f, settle) * k;
        holy(haloFirst_, haloCount_, scaled(pulled(u.feet + kUp * y, eye, kPull), glm::vec3(radius)), kHolyLine, tau, 0.9f, haloEnv, kPale, kCore, 0.0f, u.feet);
      }
    }
    // motes rising round the body
    const float moteEnv = ease(0.0f, 0.15f, tau) * (1.0f - ease(0.9f, 1.5f, tau));
    for (int i = 0; i < 18 && moteEnv > 0.0f; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 200u;
      const float a0 = hash01(seed, n, 1), r0 = hash01(seed, n, 2), p0 = hash01(seed, n, 3);
      const float phase = std::fmod(p0 + tau * 0.6f, 1.0f);
      const float angle = a0 * kTau + tau * (a0 > 0.5f ? 1.6f : -1.6f);
      const float radius = (0.35f + 0.8f * r0) * 0.32f * k;
      const glm::vec3 p = u.feet + glm::vec3(std::cos(angle) * radius, phase * h * 1.3f, std::sin(angle) * radius);
      const float alpha = std::sin(kPi * phase) * moteEnv;
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.1f * k * (1.0f - 0.4f * phase), alpha, (i & 1) ? kPale : kGold));
    }
    // a soft flash on the body as the light lands
    if (tau < 0.3f) {
      const float f = 1.0f - ease(0.0f, 0.3f, tau);
      streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.9f * k * (0.5f + 0.5f * f), 0.7f * f, kCore, 2.0f));
    }
    if (lit < 4 && env > 0.0f) {
      ++lit;
      const float flare = 1.0f + 0.6f * (1.0f - ease(0.0f, 0.3f, tau));
      lights.push_back({chest, {1.0f, 0.8f, 0.5f}, 6.0f * env * flare, 3.0f * k});
    }
  }
}

// --- Ressurreição -----------------------------------------------------------------------------------------------------

void HealerFx::buildResurrection(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 feet = e.centre, chest = feet + kUp * (h * 0.5f);
  const float begin = e.impact - kResurrectionLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);
  // The camera's side, on the ground (the wings open across the view).
  const glm::vec3 side = flatDir(glm::vec3(camera.right.x, 0.0f, camera.right.z), glm::vec3(1.0f, 0.0f, 0.0f));
  const glm::vec3 toward = flatDir(camera.position - feet, glm::vec3(0.0f, 0.0f, 1.0f));

  // --- The star under the body: it draws itself in while the light gathers, flares at the impact, fades. ---
  {
    const float appear = ease(begin, begin + 0.5f, now), fade = 1.0f - ease(e.impact + 1.2f, e.impact + 2.4f, now);
    const float flare = age >= 0.0f ? 1.0f + 1.4f * (1.0f - ease(0.0f, 0.5f, age)) : 1.0f + 0.5f * gather;
    const float opacity = appear * fade;
    if (opacity > 0.002f) {
      const float rad = e.radius * (0.5f + 0.5f * easeOut(begin, begin + 0.7f, now));
      const glm::vec3 p = pulled(feet + kUp * 0.05f, eye, kGroundPull);
      holy(starFirst_, starCount_, at(p) * glm::rotate(glm::mat4(1.0f), -t * 0.4f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rad)), kHolyLine, t, 0.85f * flare,
           opacity, kPale, kCore, 0.0f, feet);
      const float ring = age >= 0.0f ? 0.3f + 0.7f * easeOut(0.0f, 0.5f, age) : 0.3f;
      holy(discFirst_, discCount_, scaled(p, {rad, 1.0f, rad}), kHolyPool, t, 0.45f * flare, 0.7f * opacity, kGold, kCore, ring, feet);
    }
  }

  // --- Motes spiral in and down onto the body while the light gathers. ---
  {
    const float env = gather * (1.0f - ease(e.impact, e.impact + 0.15f, now));
    for (int i = 0; i < 40 && env > 0.0f; ++i) {
      const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
      const float phase = std::fmod(p0 + t * 0.75f, 1.0f);
      const float angle = a0 * kTau + phase * 4.0f;
      const float radius = glm::mix(e.radius * (0.7f + 0.6f * r0), 0.12f, phase) * k;
      const glm::vec3 p = feet + glm::vec3(std::cos(angle) * radius, glm::mix(h * 1.6f, h * 0.35f, phase), std::sin(angle) * radius);
      const float alpha = std::sin(kPi * phase) * env;
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.13f * k * (1.0f - 0.3f * phase), alpha, (i & 1) ? kPale : kGold));
    }
  }

  // --- The pillar from the sky: thin and faint while it gathers, it flares wide at the impact and withdraws upward. ---
  {
    const float H = h * 4.0f;
    if (age < 0.0f) {
      const float r = 0.16f * k * (1.0f + 0.05f * std::sin(t * 7.0f));
      holy(columnFirst_, columnCount_, scaled(pulled(feet, eye, kPull * 0.5f), {r, H, r}), kHolyShaft, t, 0.55f, 0.15f + 0.6f * gather, kPale, kCore, 0.0f, feet);
    } else {
      const float out = 1.0f - ease(0.35f, 1.3f, age);
      if (out > 0.002f) {
        const float r = (0.55f * k) * (1.0f + 0.35f * (1.0f - ease(0.0f, 0.3f, age)));
        const float lift = easeOut(0.35f, 1.3f, age);  // the bottom rises away
        const glm::vec3 base = pulled(feet, eye, kPull * 0.5f) + kUp * (lift * H);
        holy(columnFirst_, columnCount_, scaled(base, {r, H * (1.0f - lift) + 1e-3f, r}), kHolyShaft, t, 0.42f, out, kGold, kCore, 0.0f, feet);
        holy(columnFirst_, columnCount_, scaled(base, {r * 0.4f, H * (1.0f - lift) * 0.9f + 1e-3f, r * 0.4f}), kHolyShaft, t, 0.4f, out * 0.8f, kCore, kCore, 0.0f, feet);
      }
    }
  }

  // --- The light: it swells while gathering, flares at the impact, lingers warm and goes. ---
  {
    const float swell = 2.0f + 8.0f * gather;
    const float burst = age >= 0.0f ? 26.0f * (1.0f - ease(0.0f, 0.4f, age)) + 7.0f * (1.0f - ease(0.4f, 2.3f, age)) : 0.0f;
    const float intensity = age >= 0.0f ? burst : swell;
    if (intensity > 0.01f) lights.push_back({chest, {1.0f, 0.85f, 0.55f}, intensity, 4.0f * k + 1.5f * (age >= 0.0f ? ease(0.0f, 0.4f, age) : 0.0f)});
  }

  if (age >= 0.0f) {
    // --- The flash at the chest, the rings across the ground, the rays. ---
    if (age < 0.25f) {
      const float f = 1.0f - ease(0.0f, 0.25f, age);
      streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.6f * k * (0.6f + 0.4f * f), f, kCore, 2.8f));
    }
    for (int ring = 0; ring < 2; ++ring) {
      const float a = age - 0.1f * static_cast<float>(ring);
      if (a < 0.0f || a > 0.8f) continue;
      const float r = 0.3f * k + (2.2f * k - 0.3f * k) * easeOut(0.0f, 0.55f, a);
      combat(discFirst_, discCount_, scaled(pulled(feet + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, a, ring == 0 ? 0.6f : 0.4f,
             (ring == 0 ? 0.8f : 0.5f) * (1.0f - ease(0.2f, 0.8f, a)), ring == 0 ? kGold : kPale, 0.0f, glm::vec4(kCore, 0.9f), feet, VfxBlend::Additive);
    }
    constexpr int kRays = 14;
    if (age < 0.7f) {
      for (int i = 0; i < kRays; ++i) {
        const uint32_t n = static_cast<uint32_t>(i) + 100u;
        const float a = (static_cast<float>(i) + hash01(e.seed, n, 1) * 0.7f) / kRays * kTau;
        const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
        const float reach = (1.8f + 0.5f * hash01(e.seed, n, 2)) * k;
        const float head = 0.25f + reach * easeOut(0.0f, 0.55f, age);
        const float len = std::min(head - 0.15f, 0.7f + 0.6f * hash01(e.seed, n, 3));
        if (len <= 0.05f) continue;
        streaks_.push_back(streak(pulled(feet + dir * (head - len * 0.5f) + kUp * 0.1f, eye, kGroundPull), dir * len, 0.06f, 1.0f - ease(0.2f, 0.7f, age), kGold, 2.2f));
      }
    }
    // --- The wings: two arcs of light unfurl from the back, hold, and fade. ---
    {
      const float open = easeOut(0.0f, 0.55f, age), wingEnv = ease(0.0f, 0.1f, age) * (1.0f - ease(1.3f, 2.0f, age));
      if (wingEnv > 0.002f) {
        for (const float hand : {-1.0f, 1.0f}) {
          auto p = [&](float x, float y, float z) { return feet + side * (x * hand * k) + kUp * (y * h) + toward * (z * k); };
          const float o = open;
          const three::CatmullRom curve{{p(0.1f, 0.55f, -0.15f), p(0.35f + 0.55f * o, 0.75f + 0.25f * o, -0.25f), p(0.6f + 1.1f * o, 0.95f + 0.45f * o, -0.3f),
                                         p(0.7f + 1.5f * o, 0.65f + 0.75f * o, -0.3f)}};
          const std::vector<VfxVertex> tube = vfxgeo::tube(curve, 28, 0.05f * k, 7);
          holy(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kHolyTube, age, 1.0f, wingEnv, kPale, kCore, 0.0f, feet);
          // the feathers: shorter tubes hanging from the arc
          for (int f = 0; f < 4; ++f) {
            const float s = 0.3f + 0.2f * static_cast<float>(f);
            const glm::vec3 root = curve.getPoint(s);
            const glm::vec3 tip = root + side * (hand * 0.25f * k * o) - kUp * ((0.35f + 0.1f * static_cast<float>(f)) * h * o);
            const three::CatmullRom feather{{root, glm::mix(root, tip, 0.4f) + side * (hand * 0.08f * k), glm::mix(root, tip, 0.75f), tip}};
            const std::vector<VfxVertex> ft = vfxgeo::tube(feather, 10, 0.03f * k, 5);
            holy(frame.addVertices(ft), static_cast<uint32_t>(ft.size()), glm::mat4(1.0f), kHolyTube, age, 0.8f, wingEnv * 0.8f, kPale, kCore, 0.0f, feet);
          }
        }
      }
    }
    // --- The halo rising from the feet past the head. ---
    {
      const float rise = ease(0.1f, 1.1f, age), haloEnv = ease(0.1f, 0.3f, age) * (1.0f - ease(1.4f, 2.0f, age));
      if (haloEnv > 0.002f) {
        const float y = glm::mix(0.08f, h * 1.2f, rise), radius = glm::mix(0.75f, 0.42f, rise) * k;
        holy(haloFirst_, haloCount_, at(pulled(feet + kUp * y, eye, kPull)) * glm::rotate(glm::mat4(1.0f), age * 2.0f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(radius)),
             kHolyLine, age, 1.0f, haloEnv, kPale, kCore, 0.0f, feet);
      }
    }
    // --- Motes burst up like feathers and drift down. ---
    for (int i = 0; i < 34 && age < 2.0f; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 300u;
      const float a = hash01(e.seed, n, 1) * kTau, speed = 0.3f + 0.7f * hash01(e.seed, n, 2), lift = 0.8f + 1.2f * hash01(e.seed, n, 3);
      const float u = age / 2.0f;
      const glm::vec3 p = chest + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * (1.3f * k * speed * easeOut(0.0f, 1.0f, u)) +
                          kUp * (lift * std::sin(kPi * std::min(u * 1.2f, 1.0f)) * h * 0.6f - 0.25f * u * h);
      const float alpha = ease(0.0f, 0.05f, u) * (1.0f - ease(0.4f, 1.0f, u));
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.12f * k, alpha, (i & 1) ? kPale : kGold));
    }
    // a wisp of light drifting up the body
    for (int i = 0; i < 12 && age < 1.8f; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 500u;
      const float phase = std::fmod(hash01(e.seed, n, 1) + age * 0.8f, 1.0f), ang = hash01(e.seed, n, 2) * kTau + age * 1.5f, r = (0.12f + 0.2f * hash01(e.seed, n, 3)) * k;
      const glm::vec3 p = feet + glm::vec3(std::cos(ang) * r, h * (0.05f + phase * 1.1f), std::sin(ang) * r);
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.05f * k, std::sin(kPi * phase) * (1.0f - ease(1.2f, 1.8f, age)), kCore, 2.2f));
    }
  }
}

// --- Julgamento -------------------------------------------------------------------------------------------------------

void HealerFx::buildJudgment(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float R = e.radius;
  const float begin = e.impact - kJudgmentLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);
  constexpr float kSigilHeight = 5.0f, kBladeLength = 7.5f, kFall = 0.16f;
  const glm::vec3 sky = e.centre + kUp * kSigilHeight;

  // --- The sigil high in the air: it draws itself in, turns, and shatters as the blade goes through it. ---
  {
    const float appear = ease(begin, begin + 0.35f, now), gone = 1.0f - ease(e.impact, e.impact + 0.25f, now);
    const float opacity = appear * gone;
    if (opacity > 0.002f) {
      const float rad = (0.8f + 1.2f * easeOut(begin, begin + 0.5f, now)) * std::min(1.0f, R / (2.0f * kHex));
      holy(starFirst_, starCount_, at(sky) * glm::rotate(glm::mat4(1.0f), t * 1.2f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rad)), kHolyLine, t, 1.1f, opacity, kWhite,
           kCore, 0.0f, e.centre);
      holy(discFirst_, discCount_, scaled(sky, {rad, 1.0f, rad}), kHolyPool, t, 0.35f, 0.5f * opacity, kPale, kCore, 0.2f + 0.6f * gather, e.centre);
    }
  }
  // --- Motes stream up from the area into the sigil while it gathers. ---
  {
    const float env = gather * (1.0f - ease(e.impact, e.impact + 0.15f, now));
    for (int i = 0; i < 40 && env > 0.0f; ++i) {
      const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
      const float phase = std::fmod(p0 + t * 0.9f, 1.0f);
      const float angle = a0 * kTau + phase * 2.0f;
      const float radius = glm::mix(R * (0.3f + 0.7f * r0), 0.3f, phase);
      const glm::vec3 p = e.centre + glm::vec3(std::cos(angle) * radius, glm::mix(0.1f, kSigilHeight, phase), std::sin(angle) * radius);
      const float alpha = std::sin(kPi * phase) * env;
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.12f * (1.0f - 0.3f * phase), alpha, (i & 1) ? kWhite : kGold));
    }
  }
  // --- The blade: it falls through the sigil into the ground in kFall seconds, stands a moment, then rises and fades. ---
  {
    const float fallStart = e.impact - kFall;
    if (now >= fallStart) {
      const float drop = easeOut(fallStart, e.impact, now);  // 0 above, 1 planted
      const float withdraw = age >= 0.5f ? ease(0.5f, 1.1f, age) : 0.0f;
      const float opacity = (1.0f - withdraw) * (age >= 0.0f ? 1.0f : 0.6f + 0.4f * drop);
      if (opacity > 0.002f) {
        // its point: from high above the sigil down to the ground, then back up as it withdraws
        const float pointY = glm::mix(kSigilHeight + 4.0f, 0.0f, drop) + withdraw * (kBladeLength + 2.0f);
        const float w = 0.26f * (1.0f + 0.5f * (age >= 0.0f ? 1.0f - ease(0.0f, 0.3f, age) : 0.0f));
        const glm::vec3 foot = pulled(e.centre + kUp * pointY, eye, kGroundPull);
        const glm::mat4 m = at(foot) * glm::rotate(glm::mat4(1.0f), 0.5f, kUp) * glm::scale(glm::mat4(1.0f), {w, kBladeLength, w});
        holy(bladeFirst_, bladeCount_, m, kHolyShaft, t, 0.9f, opacity, kWhite, kCore, 0.0f, e.centre);
        holy(bladeFirst_, bladeCount_, at(foot) * glm::rotate(glm::mat4(1.0f), 0.5f, kUp) * glm::scale(glm::mat4(1.0f), {w * 0.4f, kBladeLength * 0.95f, w * 0.4f}), kHolyShaft,
             t, 0.9f, opacity * 0.8f, kCore, kCore, 0.0f, e.centre);
        // the glow around it
        holy(columnFirst_, columnCount_, scaled(foot, {w * 2.2f, kBladeLength * 0.8f, w * 2.2f}), kHolyShaft, t, 0.3f, opacity * 0.6f, kGold, kCore, 0.0f, e.centre);
      }
    }
  }
  // --- The light: it gathers high, strikes hard at the impact, dies down. ---
  {
    const float swell = 2.0f + 6.0f * gather;
    const float burst = age >= 0.0f ? 30.0f * (1.0f - ease(0.0f, 0.35f, age)) + 7.0f * (1.0f - ease(0.35f, 1.6f, age)) : 0.0f;
    const float intensity = age >= 0.0f ? burst : swell;
    const glm::vec3 lightAt = age >= 0.0f ? e.centre + kUp * 1.2f : sky;
    const float reach = age >= 0.0f ? glm::mix(3.0f, R * 1.3f, ease(0.0f, kJudgmentSpread, age)) : 4.0f;
    if (intensity > 0.01f) lights.push_back({lightAt, {1.0f, 0.9f, 0.65f}, intensity, reach});
  }

  if (age >= 0.0f) {
    // --- The impact: a flash at the ground, the pressure shell out to the edge, two rings, rays across the ground, dust off every hex. ---
    if (age < 0.22f) {
      const float f = 1.0f - ease(0.0f, 0.22f, age);
      streaks_.push_back(streak(pulled(e.centre + kUp * 0.5f, eye, kGroundPull), glm::vec3(0.0f), 1.8f * (0.6f + 0.4f * f), f, kCore, 3.0f));
    }
    if (age < kJudgmentSpread + 0.15f) {
      const float r = 0.3f + (R - 0.3f) * easeOut(0.0f, kJudgmentSpread + 0.05f, age);
      combat(sphereFirst_, sphereCount_, scaled(e.centre + kUp * 0.4f, glm::vec3(r, r * 0.6f, r)), kModeEnergy, age, 0.6f,
             0.5f * (1.0f - ease(0.1f, kJudgmentSpread + 0.15f, age)), kWhite, 0.04f, glm::vec4(kCore, 0.0f), e.centre, VfxBlend::Additive);
    }
    for (int ring = 0; ring < 2; ++ring) {
      const float a = age - 0.1f * static_cast<float>(ring);
      if (a < 0.0f || a > 0.85f) continue;
      const float r = 0.4f + (R * (1.04f - 0.06f * static_cast<float>(ring)) - 0.4f) * easeOut(0.0f, 0.55f, a);
      combat(discFirst_, discCount_, scaled(pulled(e.centre + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, a, ring == 0 ? 0.65f : 0.4f,
             (ring == 0 ? 0.85f : 0.5f) * (1.0f - ease(0.2f, 0.85f, a)), ring == 0 ? kWhite : kGold, 0.0f, glm::vec4(kCore, 0.9f), e.centre, VfxBlend::Additive);
    }
    constexpr int kRays = 24;
    if (age < 0.8f) {
      for (int i = 0; i < kRays; ++i) {
        const uint32_t n = static_cast<uint32_t>(i) + 100u;
        const float a = (static_cast<float>(i) + hash01(e.seed, n, 1) * 0.7f) / kRays * kTau;
        const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
        const float reach = R * (0.9f + 0.2f * hash01(e.seed, n, 2));
        const float head = 0.3f + reach * easeOut(0.0f, 0.55f, age);
        const float len = std::min(head - 0.2f, 0.9f + 1.0f * hash01(e.seed, n, 3));
        if (len <= 0.05f) continue;
        streaks_.push_back(streak(pulled(e.centre + dir * (head - len * 0.5f) + kUp * 0.12f, eye, kGroundPull), dir * len, 0.075f, 1.0f - ease(0.25f, 0.8f, age), kGold, 2.4f));
      }
    }
    // dust lifted off the ground across the area, a little later the farther out
    for (int i = 0; i < 26; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 400u;
      const float a = hash01(e.seed, n, 1) * kTau, r = R * (0.2f + 0.8f * hash01(e.seed, n, 2));
      const float tau = age - (r / R) * kJudgmentSpread, life = 1.4f * (0.8f + 0.4f * hash01(e.seed, n, 3));
      const float s = tau / life;
      if (s < 0.0f || s > 1.0f) continue;
      const glm::vec3 p = e.centre + glm::vec3(std::cos(a) * r, 0.15f + 1.1f * easeOut(0.0f, 1.0f, s), std::sin(a) * r);
      puffs_.push_back(puff(pulled(p, eye, kGroundPull), 0.5f + 0.7f * s, a + tau * 0.7f, hash01(e.seed, n, 4) * 37.0f, 0.4f * ease(0.0f, 0.1f, s) * (1.0f - ease(0.3f, 1.0f, s)), kDust));
    }
    // sparks flung up from the point of the blade
    for (int i = 0; i < 20 && age < 0.9f; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 600u;
      const float a = hash01(e.seed, n, 1) * kTau, lift = 1.5f + 2.0f * hash01(e.seed, n, 2), out = 0.5f + 1.0f * hash01(e.seed, n, 3);
      const glm::vec3 dir = glm::normalize(glm::vec3(std::cos(a) * out, lift, std::sin(a) * out));
      const glm::vec3 axis = dir * (0.35f + 0.3f * hash01(e.seed, n, 4));
      const glm::vec3 head = e.centre + kUp * 0.2f + dir * (3.0f * easeOut(0.0f, 0.8f, age)) - kUp * (2.5f * age * age);
      streaks_.push_back(streak(pulled(head - axis * 0.5f, eye, kGroundPull), axis, 0.05f, 1.0f - ease(0.3f, 0.9f, age), kWhite, 2.4f));
    }
  }

  // --- On every foe the wave reaches: a column of holy fire rises, a flash, sparks fly up, a ring burns at its feet. ---
  int lit = 0;
  for (size_t bi = 0; bi < e.bodies.size(); ++bi) {
    const Target& u = e.bodies[bi];
    const float h = bodyHeight(u.height), ku = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(u.feet.x - e.centre.x, u.feet.z - e.centre.z));
    const float tau = age - std::min(dist / R, 1.0f) * kJudgmentSpread;
    if (tau < 0.0f) continue;
    const uint32_t seed = e.seed + static_cast<uint32_t>(bi) * 7919u;
    const glm::vec3 body = u.feet + kUp * (h * 0.5f);
    const float env = ease(0.0f, 0.1f, tau) * (1.0f - ease(0.6f, 1.5f, tau));
    if (env > 0.002f) {
      const float rise = easeOut(0.0f, 0.25f, tau);
      const float flicker = 1.0f + 0.08f * std::sin(tau * 23.0f + static_cast<float>(bi));
      holy(columnFirst_, columnCount_, scaled(pulled(u.feet, eye, kPull * 0.6f), {0.38f * ku * flicker, h * 1.6f * rise + 1e-3f, 0.38f * ku * flicker}), kHolyShaft, tau, 0.55f,
           env, kFire, kCore, 0.0f, u.feet);
      holy(columnFirst_, columnCount_, scaled(pulled(u.feet, eye, kPull * 0.6f), {0.17f * ku, h * 1.45f * rise + 1e-3f, 0.17f * ku}), kHolyShaft, tau, 0.5f, env * 0.85f,
           kWhite, kCore, 0.0f, u.feet);
    }
    if (tau < 0.22f) {
      const float f = 1.0f - ease(0.0f, 0.22f, tau);
      streaks_.push_back(streak(pulled(body, eye, kPull), glm::vec3(0.0f), 1.2f * ku * (0.5f + 0.5f * f), f, kCore, 3.0f));
    }
    if (tau < 0.6f) {
      const float r = (0.3f + 1.0f * easeOut(0.0f, 0.5f, tau)) * ku;
      combat(discFirst_, discCount_, scaled(pulled(u.feet + kUp * 0.05f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, tau, 0.55f, 0.85f * (1.0f - ease(0.1f, 0.6f, tau)), kFire,
             0.0f, glm::vec4(kCore, 0.85f), u.feet, VfxBlend::Additive);
    }
    // sparks driven up and out of the body
    if (tau < 0.8f) {
      for (int j = 0; j < 12; ++j) {
        const uint32_t n = static_cast<uint32_t>(j) + 700u;
        const float a = hash01(seed, n, 1) * kTau, lift = 0.6f + 0.9f * hash01(seed, n, 2), out = 0.3f + 0.5f * hash01(seed, n, 3);
        const glm::vec3 dir = glm::normalize(glm::vec3(std::cos(a) * out, lift, std::sin(a) * out));
        const glm::vec3 axis = dir * (0.3f + 0.3f * hash01(seed, n, 4));
        const glm::vec3 head = body + dir * (0.2f + (1.4f + 0.8f * hash01(seed, n, 5)) * easeOut(0.0f, 0.6f, tau)) - kUp * (1.2f * tau * tau);
        streaks_.push_back(streak(pulled(head - axis * 0.5f, eye, kPull), axis, 0.05f * ku, 1.0f - ease(0.2f, 0.8f, tau), kGold, 2.2f));
      }
    }
    // embers of white fire rising along the body
    for (int j = 0; j < 10; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 800u;
      const float phase = std::fmod(hash01(seed, n, 1) + tau * 1.1f, 1.0f), ang = hash01(seed, n, 2) * kTau + tau * 2.0f, r = (0.15f + 0.25f * hash01(seed, n, 3)) * ku;
      const glm::vec3 p = u.feet + glm::vec3(std::cos(ang) * r, h * (0.1f + phase * 1.2f), std::sin(ang) * r);
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.055f * ku, std::sin(kPi * phase) * env, kWhite, 2.4f));
    }
    if (lit < 4 && env > 0.0f) {
      ++lit;
      lights.push_back({body, {1.0f, 0.8f, 0.45f}, 10.0f * env, 2.8f * ku});
    }
  }
}

}  // namespace ember
