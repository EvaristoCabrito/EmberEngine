#include "render/vfx/TurnUndead.h"

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
constexpr float kPull = 1.3f, kGroundPull = 0.7f;  // as CombatFx / WarriorFx: in front of the card, which is drawn 1.2 units nearer
constexpr float kHex = 1.7320508f;                 // the distance between neighbouring hex centres, world units
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f;  // Combat material
constexpr float kHolyShaft = 0.0f, kHolyPool = 1.0f, kHolyMote = 2.0f, kHolyLine = 4.0f;     // Holy material
constexpr float kSpread = 0.5f;  // the wave takes this long from the caster to the edge of the area

constexpr glm::vec3 kGold{1.0f, 0.82f, 0.48f}, kCore{1.0f, 0.97f, 0.88f}, kPale{1.0f, 0.93f, 0.7f};
constexpr glm::vec3 kAsh{0.2f, 0.18f, 0.16f};

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
// The ring that marks a frightened body: a flat torus of radius 1 lying flat (u around).
const std::vector<VfxVertex>& haloGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> ring = vfxgeo::torus(1.0f, 0.06f, 8, 64, kTau);
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
// The fixed sigil (radius 1): the outer ring, its tick marks, a ring just inside.
const std::vector<VfxVertex>& sigilGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> out;
    circle(out, 1.0f, 0.018f, 96);
    circle(out, 0.9f, 0.008f, 96);
    for (int i = 0; i < 36; ++i) {
      const float a = static_cast<float>(i) / 36.0f * kTau, inner = (i % 3 == 0) ? 0.88f : 0.93f;
      ribbon(out, {std::cos(a) * inner, std::sin(a) * inner}, {std::cos(a) * 0.985f, std::sin(a) * 0.985f}, 0.01f, 0.0f, 1.0f);
    }
    return out;
  }();
  return g;
}
// The turning sigil (radius 1): a hexagram (the board is hexes) and the ring inside it.
const std::vector<VfxVertex>& starGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> out;
    constexpr float kStar = 0.86f;
    for (int tri = 0; tri < 2; ++tri)
      for (int i = 0; i < 3; ++i) {
        const float a0 = (static_cast<float>(i) * 2.0f + static_cast<float>(tri)) / 6.0f * kTau, a1 = (static_cast<float>(i + 1) * 2.0f + static_cast<float>(tri)) / 6.0f * kTau;
        ribbon(out, {std::cos(a0) * kStar, std::sin(a0) * kStar}, {std::cos(a1) * kStar, std::sin(a1) * kStar}, 0.014f, 0.0f, 1.0f);
      }
    circle(out, kStar / std::sqrt(3.0f), 0.012f, 64);
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

}  // namespace

void TurnUndead::cast(glm::vec3 feet, float height, float radius, std::vector<Target> undead, float impact) {
  Effect e;
  e.feet = feet, e.height = bodyHeight(height), e.radius = std::clamp(radius, 2.0f * kHex, 8.0f * kHex), e.impact = impact, e.undead = std::move(undead);
  e.seed = 0x7d3au + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

bool TurnUndead::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

float TurnUndead::latestEnd() const {
  float end = -1.0f;
  for (const Effect& e : effects_) end = std::max(end, e.end());
  return end;
}

void TurnUndead::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  columnFirst_ = frame.addVertices(columnGeometry()), columnCount_ = static_cast<uint32_t>(columnGeometry().size());
  haloFirst_ = frame.addVertices(haloGeometry()), haloCount_ = static_cast<uint32_t>(haloGeometry().size());
  sigilFirst_ = frame.addVertices(sigilGeometry()), sigilCount_ = static_cast<uint32_t>(sigilGeometry().size());
  starFirst_ = frame.addVertices(starGeometry()), starCount_ = static_cast<uint32_t>(starGeometry().size());
  streaks_.clear(), puffs_.clear(), motes_.clear();
  for (const Effect& e : effects_)
    if (now >= e.impact - kLead) buildOne(e, now, camera, frame, lights);

  // The particles of everything in play, one draw each: the ash first (it covers), then the light.
  const glm::vec3 sight = effects_.front().feet;
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

void TurnUndead::buildOne(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float R = e.radius, k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.55f);
  const float begin = e.impact - kLead, age = now - e.impact;
  const float t = now - begin;
  const float gather = ease(begin, e.impact, now);

  // --- The sigil on the ground: it draws itself in and grows to the edge of the area; the hexagram turns against its ring; it
  //     flares as the wave leaves and fades. A pool of light under it. ---
  {
    const float appear = ease(begin, begin + 0.5f, now), fade = 1.0f - ease(e.impact + 0.9f, e.impact + 1.9f, now);
    const float flare = age >= 0.0f ? 1.0f + 1.2f * (1.0f - ease(0.0f, 0.5f, age)) : 1.0f + 0.4f * gather;
    const float opacity = appear * fade;
    if (opacity > 0.002f) {
      const float rad = R * (0.4f + 0.6f * easeOut(begin, begin + 0.7f, now));
      const glm::vec3 at = pulled(e.feet + kUp * 0.05f, eye, kGroundPull);
      const glm::mat4 base = glm::translate(glm::mat4(1.0f), at);
      holy(sigilFirst_, sigilCount_, base * glm::rotate(glm::mat4(1.0f), t * 0.22f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rad)), kHolyLine, t, 0.8f * flare,
           opacity, kGold, kCore, 0.0f, e.feet);
      holy(starFirst_, starCount_, base * glm::rotate(glm::mat4(1.0f), -t * 0.55f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rad)), kHolyLine, t, 0.85f * flare,
           opacity, kPale, kCore, 0.0f, e.feet);
      const float ring = age >= 0.0f ? 0.3f + 0.7f * easeOut(0.0f, 0.5f, age) : 0.3f;
      holy(discFirst_, discCount_, base * glm::scale(glm::mat4(1.0f), {rad, 1.0f, rad}), kHolyPool, t, 0.42f * flare, 0.7f * opacity, kGold, kCore, ring, e.feet);
    }
  }

  // --- A shaft of light comes down onto the caster, holds through the burst and fades. ---
  {
    const float out = 1.0f - ease(e.impact + 0.35f, e.impact + 1.1f, now);
    if (out > 0.002f) {
      const float H = e.height * 2.4f, r = 0.4f * k * (1.0f + 0.04f * std::sin(t * 6.0f));
      const float lowered = 1.0f - gather;
      const float opacity = out * (0.35f + 0.65f * gather);
      const glm::vec3 base = e.feet + glm::vec3(0.0f, lowered * H, 0.0f);
      holy(columnFirst_, columnCount_, glm::translate(glm::mat4(1.0f), base) * glm::scale(glm::mat4(1.0f), {r, H * (1.0f - lowered) + 1e-3f, r}), kHolyShaft, t, 0.3f,
           opacity, kGold, kCore, 0.0f, e.feet);
      holy(columnFirst_, columnCount_, glm::translate(glm::mat4(1.0f), base) * glm::scale(glm::mat4(1.0f), {r * 0.45f, H * (1.0f - lowered) * 0.85f + 1e-3f, r * 0.45f}),
           kHolyShaft, t, 0.26f, opacity * 0.7f, glm::mix(kGold, kCore, 0.5f), kCore, 0.0f, e.feet);
    }
  }

  // --- Motes spiral in from the sigil to the caster's chest while it gathers. ---
  {
    const float env = gather * (1.0f - ease(e.impact, e.impact + 0.2f, now));
    for (int i = 0; i < 30 && env > 0.0f; ++i) {
      const float a0 = hash01(e.seed, static_cast<uint32_t>(i), 1), r0 = hash01(e.seed, static_cast<uint32_t>(i), 2), p0 = hash01(e.seed, static_cast<uint32_t>(i), 3);
      const float phase = std::fmod(p0 + t * 0.8f, 1.0f);
      const float angle = a0 * kTau + phase * 3.0f;
      const float radius = glm::mix(R * (0.5f + 0.5f * r0), 0.2f, phase);
      const glm::vec3 p = e.feet + glm::vec3(std::cos(angle) * radius, glm::mix(0.1f, e.height * 0.55f, phase), std::sin(angle) * radius);
      const float alpha = std::sin(kPi * phase) * env;
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.14f * k * (1.0f - 0.3f * phase), alpha, (i & 1) ? kPale : kGold));
    }
  }

  // --- The light on the caster and the board: it swells, flares on the burst, settles and goes. ---
  {
    const float swell = 3.0f + 7.0f * gather;
    const float burst = age >= 0.0f ? 22.0f * (1.0f - ease(0.0f, 0.35f, age)) + 6.0f * (1.0f - ease(0.35f, 1.4f, age)) : 0.0f;
    const float intensity = age >= 0.0f ? burst : swell;
    const float reach = glm::mix(3.5f * k, R * 1.1f, age >= 0.0f ? ease(0.0f, 0.5f, age) : 0.0f);
    if (intensity > 0.01f) lights.push_back({chest, {1.0f, 0.84f, 0.52f}, intensity, reach});
  }

  if (age >= 0.0f) {
    // --- The burst: a flash at the chest, the pressure shell, two rings across the ground. ---
    if (age < 0.22f) {
      const float f = 1.0f - ease(0.0f, 0.22f, age);
      streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.3f * k * (0.6f + 0.4f * f), f, kCore, 2.6f));
    }
    if (age < kSpread + 0.15f) {
      const float r = 0.3f * k + (R - 0.3f * k) * easeOut(0.0f, kSpread + 0.05f, age);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), chest) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      combat(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.55f, 0.5f * (1.0f - ease(0.1f, kSpread + 0.15f, age)), kGold, 0.04f, glm::vec4(kCore, 0.0f), e.feet,
             VfxBlend::Additive);
    }
    for (int ring = 0; ring < 2; ++ring) {
      const float a = age - 0.12f * static_cast<float>(ring);
      if (a < 0.0f || a > 0.85f) continue;
      const float r = 0.4f * k + (R * (1.04f - 0.06f * static_cast<float>(ring)) - 0.4f * k) * easeOut(0.0f, 0.6f, a);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.045f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      combat(discFirst_, discCount_, m, kModeRing, a, ring == 0 ? 0.6f : 0.4f, (ring == 0 ? 0.8f : 0.55f) * (1.0f - ease(0.2f, 0.85f, a)),
             ring == 0 ? kGold : glm::vec3(1.0f, 0.95f, 0.8f), 0.0f, glm::vec4(kCore, 0.9f), e.feet, VfxBlend::Additive);
    }
    // Rays of light run out across the ground, each a streak with its head at the wave.
    constexpr int kRays = 18;
    if (age < 0.8f) {
      for (int i = 0; i < kRays; ++i) {
        const uint32_t n = static_cast<uint32_t>(i) + 100u;
        const float a = (static_cast<float>(i) + hash01(e.seed, n, 1) * 0.7f) / kRays * kTau;
        const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
        const float reach = R * (0.9f + 0.15f * hash01(e.seed, n, 2));
        const float head = 0.3f + reach * easeOut(0.0f, 0.6f, age);
        const float len = std::min(head - 0.2f, 0.9f + 0.9f * hash01(e.seed, n, 3));
        if (len <= 0.05f) continue;
        const glm::vec3 centre = e.feet + dir * (head - len * 0.5f) + kUp * 0.12f;
        streaks_.push_back(streak(pulled(centre, eye, kGroundPull), dir * len, 0.07f, 1.0f - ease(0.25f, 0.8f, age), kGold, 2.2f));
      }
    }
    // Motes spray out and up from the chest, then drift down.
    for (int i = 0; i < 28 && age < 1.5f; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 300u;
      const float a = hash01(e.seed, n, 1) * kTau, speed = 0.4f + 0.6f * hash01(e.seed, n, 2), lift = 0.4f + 0.9f * hash01(e.seed, n, 3);
      const float u = age / 1.5f;
      const glm::vec3 p = chest + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * (R * 0.8f * speed * easeOut(0.0f, 1.0f, u)) +
                          kUp * (lift * std::sin(kPi * std::min(u * 1.3f, 1.0f)) - 0.3f * u);
      const float alpha = ease(0.0f, 0.06f, u) * (1.0f - ease(0.35f, 1.0f, u));
      if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.13f * k, alpha, (i & 1) ? kPale : kGold));
    }
  }

  // --- On every undead the wave reaches: radiance, repelled, ash, then the mark of fear. ---
  int lit = 0;
  for (size_t ui = 0; ui < e.undead.size(); ++ui) {
    const Target& u = e.undead[ui];
    const float h = bodyHeight(u.height), ku = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(u.feet.x - e.feet.x, u.feet.z - e.feet.z));
    const float tau = age - std::min(dist / R, 1.0f) * kSpread;
    if (tau < 0.0f) continue;
    const uint32_t seed = e.seed + static_cast<uint32_t>(ui) * 7919u;
    const glm::vec3 away = flatDir(u.feet - e.feet, glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::vec3 body = u.feet + kUp * (h * 0.5f);
    const float env = ease(0.0f, 0.1f, tau) * (1.0f - ease(0.55f, 1.4f, tau));

    // The radiance: a column of white-gold light that rises and thins out.
    if (env > 0.002f) {
      const float rise = easeOut(0.0f, 0.25f, tau);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(u.feet, eye, kPull * 0.6f)) * glm::scale(glm::mat4(1.0f), {0.34f * ku, h * 1.5f * rise + 1e-3f, 0.34f * ku});
      holy(columnFirst_, columnCount_, m, kHolyShaft, tau, 0.5f, env, kGold, kCore, 0.0f, u.feet);
      const glm::mat4 inner = glm::translate(glm::mat4(1.0f), pulled(u.feet, eye, kPull * 0.6f)) * glm::scale(glm::mat4(1.0f), {0.15f * ku, h * 1.35f * rise + 1e-3f, 0.15f * ku});
      holy(columnFirst_, columnCount_, inner, kHolyShaft, tau, 0.45f, env * 0.8f, kCore, kCore, 0.0f, u.feet);
    }
    // A flash on the body.
    if (tau < 0.22f) {
      const float f = 1.0f - ease(0.0f, 0.22f, tau);
      streaks_.push_back(streak(pulled(body, eye, kPull), glm::vec3(0.0f), 1.1f * ku * (0.5f + 0.5f * f), f, kCore, 2.8f));
    }
    // The ring at its feet, pushed outward.
    if (tau < 0.55f) {
      const float r = (0.3f + 1.0f * easeOut(0.0f, 0.45f, tau)) * ku;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(u.feet + glm::vec3(0.0f, 0.05f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      combat(discFirst_, discCount_, m, kModeRing, tau, 0.5f, 0.8f * (1.0f - ease(0.1f, 0.55f, tau)), kGold, 0.0f, glm::vec4(kCore, 0.85f), u.feet, VfxBlend::Additive);
    }
    // Sparks driven away from the caster: it is repelled.
    if (tau < 0.7f) {
      for (int j = 0; j < 10; ++j) {
        const uint32_t n = static_cast<uint32_t>(j) + 600u;
        const float spread = (hash01(seed, n, 1) - 0.5f) * 1.0f, lift = 0.15f + 0.5f * hash01(seed, n, 2);
        const glm::vec3 dir = glm::normalize(glm::vec3(away.x * std::cos(spread) - away.z * std::sin(spread), lift, away.x * std::sin(spread) + away.z * std::cos(spread)));
        const glm::vec3 axis = dir * (0.4f + 0.3f * hash01(seed, n, 3));
        const glm::vec3 head = body + dir * (0.25f + (1.6f + 1.0f * hash01(seed, n, 4)) * easeOut(0.0f, 0.55f, tau));
        streaks_.push_back(streak(pulled(head - axis * 0.5f, eye, kPull), axis, 0.05f * ku, 1.0f - ease(0.15f, 0.7f, tau), kGold, 2.0f));
      }
    }
    // Ash lifting off the body, and embers rising from it.
    for (int j = 0; j < 8; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 700u;
      const float life = 1.1f * (0.8f + 0.4f * hash01(seed, n, 1)), s = tau / life;
      if (s > 1.0f) continue;
      const float a = hash01(seed, n, 2) * kTau;
      const glm::vec3 p = u.feet + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * (0.22f * ku) + kUp * (h * (0.25f + 0.7f * easeOut(0.0f, 1.0f, s))) + away * (0.35f * s);
      puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.3f + 0.5f * s) * ku, a + tau, hash01(seed, n, 3) * 37.0f, 0.34f * ease(0.0f, 0.1f, s) * (1.0f - ease(0.3f, 1.0f, s)), kAsh));
    }
    for (int j = 0; j < 10; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 800u;
      const float phase = std::fmod(hash01(seed, n, 1) + tau * 1.0f, 1.0f), ang = hash01(seed, n, 2) * kTau + tau * 2.0f, r = (0.15f + 0.25f * hash01(seed, n, 3)) * ku;
      const glm::vec3 p = u.feet + glm::vec3(std::cos(ang) * r, h * (0.1f + phase), std::sin(ang) * r);
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.05f * ku, std::sin(kPi * phase) * env, kPale, 2.2f));
    }
    // The mark of fear: a ring of pale light shivering over its head.
    {
      const float ringEnv = ease(0.12f, 0.4f, tau) * (1.0f - ease(1.3f, 2.1f, tau));
      if (ringEnv > 0.002f) {
        const float shiver = 0.04f * std::sin(tau * 15.0f) * (1.0f - ease(0.4f, 2.0f, tau));
        const glm::vec3 at = u.feet + kUp * (h * 1.1f + shiver);
        const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(at, eye, kPull)) * glm::rotate(glm::mat4(1.0f), tau * 3.0f, kUp) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(0.42f * ku * (0.85f + 0.15f * easeOut(0.12f, 0.5f, tau))));
        holy(haloFirst_, haloCount_, m, kHolyLine, tau, 1.0f, ringEnv, kPale, kCore, 0.0f, u.feet);
      }
    }
    if (lit < 4 && env > 0.0f) {
      ++lit;
      lights.push_back({body, {1.0f, 0.82f, 0.5f}, 9.0f * env, 2.6f * ku});
    }
  }
}

}  // namespace ember
