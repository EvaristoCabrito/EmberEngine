#include "render/vfx/WarriorFx.h"

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
constexpr float kPull = 1.3f, kGroundPull = 0.7f;  // as CombatFx: in front of the card, which is drawn 1.2 units nearer
constexpr float kGravity = 9.8f;
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f, kModeTrail = 4.0f, kModeSlash = 5.0f;
constexpr float kHex = 1.7320508f;  // the distance between neighbouring hex centres, world units

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
glm::vec3 sphereDir(float a, float b) {
  const float z = 2.0f * a - 1.0f, r = std::sqrt(std::max(0.0f, 1.0f - z * z)), phi = b * kTau;
  return {r * std::cos(phi), z, r * std::sin(phi)};
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
// A chipped stone (flat shaded), squashed a little.
const std::vector<VfxVertex>& rockGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> r = vfxgeo::icosahedron(1.0f, 0);
    for (VfxVertex& v : r) v.position *= glm::vec3(1.0f, 0.7f, 0.85f);
    for (size_t i = 0; i + 2 < r.size(); i += 3) {
      const glm::vec3 n = glm::normalize(glm::cross(r[i + 1].position - r[i].position, r[i + 2].position - r[i].position));
      r[i].normal = r[i + 1].normal = r[i + 2].normal = n;
    }
    return r;
  }();
  return g;
}
// A shard of armour: a thin plate.
const std::vector<VfxVertex>& chipGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(1.0f, 0.22f, 0.8f);
  return g;
}
// An open cylinder of radius 1 from y 0 to 1 (u around, v up), normals outward: an ally's column of light.
const std::vector<VfxVertex>& columnGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 32, up = 8;
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
// The broken ring over a sundered chest: two arcs of a thin torus (a gap between them), lying flat (around +y).
const std::vector<VfxVertex>& arcGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> out;
    const glm::mat4 flat = glm::rotate(glm::mat4(1.0f), -kPi * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    for (int k = 0; k < 2; ++k) {
      std::vector<VfxVertex> arc = vfxgeo::torus(1.0f, 0.085f, 6, 40, 2.35f);
      const glm::mat4 m = flat * glm::rotate(glm::mat4(1.0f), static_cast<float>(k) * kPi + 0.4f, glm::vec3(0.0f, 0.0f, 1.0f));
      for (VfxVertex& v : arc) {
        v.position = glm::vec3(m * glm::vec4(v.position, 1.0f));
        v.normal = glm::vec3(m * glm::vec4(v.normal, 0.0f));
        out.push_back(v);
      }
    }
    return out;
  }();
  return g;
}

struct Draw {
  VfxFrame& frame;
  void operator()(uint32_t first, uint32_t count, const glm::mat4& model, float mode, float time, float intensity, float opacity, glm::vec3 color,
                  float colorA, glm::vec4 params1, glm::vec3 sight, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) const {
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
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}
VfxInstance puff(glm::vec3 centre, float size, float rotation, float seed, float alpha, glm::vec3 albedo) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(rotation, seed, alpha, 0.0f), glm::vec4(albedo, 0.0f)};
}

constexpr glm::vec3 kSteel{0.78f, 0.86f, 1.0f}, kWhite{1.0f, 0.98f, 0.94f};
constexpr glm::vec3 kEmberLight{1.0f, 0.74f, 0.45f};
constexpr glm::vec3 kDust{0.46f, 0.4f, 0.33f};

}  // namespace

float WarriorFx::Effect::end() const {
  switch (kind) {
    case Kind::Whirlwind: return impact + 1.3f;
    case Kind::GuardBreak: return impact + 3.2f;
    case Kind::WarCry: return impact + 2.0f;
    case Kind::Fear: return impact + 0.7f;
    case Kind::Leap: return impact + 1.8f;
    case Kind::LeapBlow: return impact + 0.8f;
  }
  return impact + 1.0f;
}

void WarriorFx::whirlwind(glm::vec3 feet, float height, const std::vector<Target>& foes, float impact) {
  Effect e;
  e.kind = Kind::Whirlwind, e.feet = feet, e.height = bodyHeight(height), e.impact = impact, e.start = impact - 0.22f, e.targets = foes;
  e.seed = 0x7a11u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void WarriorFx::guardBreak(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::GuardBreak, e.feet = foe.feet, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.1f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x6b2cu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void WarriorFx::warCry(glm::vec3 feet, float height, const std::vector<Target>& allies, float radius, float impact) {
  Effect e;
  e.kind = Kind::WarCry, e.feet = feet, e.height = bodyHeight(height), e.radius = std::max(radius, 2.0f * kHex), e.impact = impact, e.start = impact;
  e.targets = allies;
  e.seed = 0x3c9du + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void WarriorFx::warCryFear(const std::vector<Target>& foes, float impact) {
  if (foes.empty()) return;
  Effect e;
  e.kind = Kind::Fear, e.feet = foes.front().feet, e.impact = impact + 0.25f, e.start = e.impact, e.targets = foes;
  e.seed = 0x51f3u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void WarriorFx::leap(glm::vec3 from, float height, glm::vec3 landing, float start, float impact) {
  Effect e;
  e.kind = Kind::Leap, e.feet = from, e.to = landing, e.height = bodyHeight(height), e.start = start, e.impact = std::max(impact, start + 0.1f);
  e.dir = flatDir(landing - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x2e77u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void WarriorFx::leapBlow(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::LeapBlow, e.feet = foe.feet, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.12f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x1d05u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

bool WarriorFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

void WarriorFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  rockFirst_ = frame.addVertices(rockGeometry()), rockCount_ = static_cast<uint32_t>(rockGeometry().size());
  chipFirst_ = frame.addVertices(chipGeometry()), chipCount_ = static_cast<uint32_t>(chipGeometry().size());
  columnFirst_ = frame.addVertices(columnGeometry()), columnCount_ = static_cast<uint32_t>(columnGeometry().size());
  arcFirst_ = frame.addVertices(arcGeometry()), arcCount_ = static_cast<uint32_t>(arcGeometry().size());
  streaks_.clear(), puffs_.clear(), rocks_.clear(), chips_.clear();
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    switch (e.kind) {
      case Kind::Whirlwind: buildWhirlwind(e, now, camera, frame, lights); break;
      case Kind::GuardBreak: buildGuardBreak(e, now, camera, frame, lights); break;
      case Kind::WarCry: buildWarCry(e, now, camera, frame, lights); break;
      case Kind::Fear: buildFear(e, now, camera, frame, lights); break;
      case Kind::Leap: buildLeap(e, now, camera, frame, lights); break;
      case Kind::LeapBlow: buildLeapBlow(e, now, camera, frame, lights); break;
    }
  }
  // The particles of everything in play, one draw each: the dust first (it covers), the thrown pieces, then the light.
  const glm::vec3 sight = effects_.front().feet;
  const Draw draw{frame};
  if (!puffs_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
         static_cast<uint32_t>(puffs_.size()));
  }
  for (int set = 0; set < 2; ++set) {
    const std::vector<VfxInstance>& pieces = set == 0 ? rocks_ : chips_;
    if (pieces.empty()) continue;
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), pieces.begin(), pieces.end());
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {set == 0 ? 0.92f : 0.45f, set == 0 ? 0.0f : 0.6f, 0.0f, 0.0f};  // roughness, metalness
    d.gpu.params1 = glm::vec4(0.0f);
    d.gpu.color = glm::vec4(1.0f);
    d.gpu.firstVertex = set == 0 ? rockFirst_ : chipFirst_;
    d.gpu.vertexCount = set == 0 ? rockCount_ : chipCount_;
    d.gpu.firstInstance = first;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Standard);
    d.gpu.sightAt = sight;
    d.instances = static_cast<uint32_t>(pieces.size());
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
  if (!streaks_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(1.0f, 0.97f, 0.9f, 0.0f), sight,
         VfxBlend::Additive, first, static_cast<uint32_t>(streaks_.size()));
  }
}

// --- Shared pieces ---------------------------------------------------------------------------------------------------------

void WarriorFx::downwardCut(glm::vec3 top, glm::vec3 bottom, glm::vec3 dir, float width, float intensity, float age, float sweep, glm::vec3 eye,
                            glm::vec3 sight, VfxFrame& frame) {
  constexpr float kHold = 0.05f, kFade = 0.24f;
  const float head = easeOut(0.0f, sweep, age);
  const float tail = ease(sweep * 0.5f, sweep + kHold + kFade, age);
  const float opacity = 1.0f - ease(sweep + kHold, sweep + kHold + kFade, age);
  if (opacity <= 0.0f || head <= tail) return;
  constexpr int kSegments = 24;
  const glm::vec3 side = glm::normalize(glm::cross(kUp, dir));
  std::vector<VfxVertex> sheet;
  sheet.reserve(kSegments * 6);
  std::vector<glm::vec3> edge(kSegments + 1), inner(kSegments + 1);
  for (int j = 0; j <= kSegments; ++j) {
    const float s = static_cast<float>(j) / kSegments;
    const float w = width * std::pow(std::sin(kPi * s), 0.6f) + 0.004f;
    // straight down, bowed toward the striker in the middle; the sheet's width runs along the blow
    const glm::vec3 p = glm::mix(top, bottom, s) - dir * (0.25f * std::sin(kPi * s));
    edge[j] = pulled(p, eye, kPull);
    inner[j] = pulled(p + dir * w, eye, kPull);
  }
  for (int j = 0; j < kSegments; ++j) {
    const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
    const VfxVertex a0{inner[j], s0, side, 0.0f}, a1{edge[j], s0, side, 1.0f}, b0{inner[j + 1], s1, side, 0.0f}, b1{edge[j + 1], s1, side, 1.0f};
    sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
  }
  const Draw draw{frame};
  draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, age, intensity, opacity, kSteel, tail, glm::vec4(kWhite, head),
       sight, VfxBlend::Additive);
}

void WarriorFx::debris(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 colour, bool chips, float age, glm::vec3 eye,
                       uint32_t salt) {
  if (age < 0.0f || age > 1.6f) return;
  const float ground = e.feet.y;
  for (int i = 0; i < count; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + salt;
    const float a = hash01(e.seed, n, 1) * kTau;
    const glm::vec3 radial{std::cos(a), 0.0f, std::sin(a)};
    const glm::vec3 h = glm::normalize(bias + radial * 0.9f);
    const glm::vec3 vh = h * speed * (0.6f + 0.9f * hash01(e.seed, n, 3));
    const float vy = speed * (0.9f + 0.9f * hash01(e.seed, n, 4));
    const float drop = std::max(at.y - ground, 0.0f);
    // the first flight until it meets the ground, a low hop, then rest
    const float t1 = (vy + std::sqrt(vy * vy + 2.0f * kGravity * drop)) / kGravity, vy2 = vy * 0.28f, t2 = 2.0f * vy2 / kGravity;
    glm::vec3 p;
    float spin = age;
    if (age < t1) p = at + vh * age + glm::vec3(0.0f, vy * age - 0.5f * kGravity * age * age, 0.0f);
    else if (age < t1 + t2) {
      const float s = age - t1;
      p = at + vh * t1 + vh * 0.4f * s + glm::vec3(0.0f, vy2 * s - 0.5f * kGravity * s * s, 0.0f);
      p.y = std::max(p.y, ground + 0.02f);
    } else {
      p = at + vh * t1 + vh * 0.4f * t2;
      p.y = ground + 0.02f;
      spin = t1 + t2;
    }
    const float fade = 1.0f - ease(1.1f, 1.6f, age);
    if (fade <= 0.0f) continue;
    const float sz = size * (0.7f + 0.6f * hash01(e.seed, n, 5)) * fade;
    const glm::vec3 axis = sphereDir(hash01(e.seed, n, 6), hash01(e.seed, n, 7));
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p, eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), spin * (8.0f + 10.0f * hash01(e.seed, n, 8)), axis) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(sz));
    const VfxInstance inst{m, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(colour * (0.8f + 0.4f * hash01(e.seed, n, 9)), 1.0f)};
    (chips ? chips_ : rocks_).push_back(inst);
  }
}

// --- Redemoinho de Aço -------------------------------------------------------------------------------------------------------

void WarriorFx::buildWhirlwind(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  constexpr float kTurn = 0.32f;  // one full circle
  const float t = now - e.start;  // the sweep runs 0..kTurn
  // The blade starts toward the first foe (else away from the camera) and turns the camera's way round.
  glm::vec3 first = e.targets.empty() ? flatDir(e.feet - eye, glm::vec3(0.0f, 0.0f, 1.0f)) : flatDir(e.targets.front().feet - e.feet, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 side0 = glm::normalize(glm::cross(kUp, first));
  auto around = [&](float u) { return first * std::cos(u * kTau) + side0 * std::sin(u * kTau); };
  const float radius = 1.6f * k, y = 0.5f * e.height;
  const float head = std::clamp(t / kTurn, 0.0f, 1.0f);
  const float tail = std::clamp((t - 0.2f) / kTurn, 0.0f, 1.0f);
  const float opacity = 1.0f - ease(kTurn + 0.02f, kTurn + 0.3f, t);

  // --- The sheet of steel light: a ring strip at weapon height (u around, v from the inner side to the edge), with a
  // slight helix so it never reads as a flat disc; drawn in as the blade passes. A wider, dimmer wake under it gives it
  // body. ---
  if (opacity > 0.0f && head > tail) {
    constexpr int kSegments = 72;
    auto ring = [&](float width, float drop) {
      std::vector<VfxVertex> sheet;
      sheet.reserve(kSegments * 6);
      std::vector<glm::vec3> edge(kSegments + 1), inner(kSegments + 1);
      for (int j = 0; j <= kSegments; ++j) {
        const float u = static_cast<float>(j) / kSegments;
        const glm::vec3 r = around(u);
        const float lift = 0.08f * k * std::sin(u * kTau * 2.0f + 0.5f) - drop;
        const glm::vec3 p = e.feet + r * radius + kUp * (y + lift);
        edge[j] = pulled(p, eye, kPull);
        inner[j] = pulled(p - r * width - kUp * (drop * 0.5f), eye, kPull);
      }
      for (int j = 0; j < kSegments; ++j) {
        const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
        const VfxVertex a0{inner[j], s0, kUp, 0.0f}, a1{edge[j], s0, kUp, 1.0f}, b0{inner[j + 1], s1, kUp, 0.0f}, b1{edge[j + 1], s1, kUp, 1.0f};
        sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
      }
      return sheet;
    };
    const std::vector<VfxVertex> blade = ring(0.55f * k, 0.0f), wake = ring(0.9f * k, 0.12f * k);
    draw(frame.addVertices(wake), static_cast<uint32_t>(wake.size()), glm::mat4(1.0f), kModeSlash, t, 0.9f, opacity * 0.6f, {0.5f, 0.62f, 0.8f}, std::max(0.0f, tail - 0.08f),
         glm::vec4(kSteel, head), e.feet, VfxBlend::Additive);
    draw(frame.addVertices(blade), static_cast<uint32_t>(blade.size()), glm::mat4(1.0f), kModeSlash, t, 1.9f, opacity, kSteel, tail, glm::vec4(kWhite, head), e.feet,
         VfxBlend::Additive);
  }
  // --- The edge's glint and its light riding the circle. ---
  if (t >= 0.0f && t < kTurn) {
    const glm::vec3 r = around(head);
    const glm::vec3 p = e.feet + r * radius + kUp * y;
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.22f * k, 0.9f, kWhite, 2.4f));
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::cross(kUp, r) * 0.5f * k, 0.035f * k, 0.9f, kSteel, 2.6f));
    lights.push_back({p, {0.85f, 0.9f, 1.0f}, 9.0f, 2.8f * k});
  }
  // --- Sparks flung off the edge, tangent and out, falling. ---
  for (int i = 0; i < 30; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 100u;
    const float born = hash01(e.seed, n, 1) * kTurn;
    const float life = 0.22f + 0.3f * hash01(e.seed, n, 2);
    const float tau = t - born;
    if (tau < 0.0f || tau > life) continue;
    const glm::vec3 r = around(born / kTurn);
    const glm::vec3 tangent = glm::cross(kUp, r);
    const glm::vec3 d = glm::normalize(tangent * 0.9f + r * (0.3f + 0.6f * hash01(e.seed, n, 3)) + kUp * (0.1f + 0.5f * hash01(e.seed, n, 4)));
    const glm::vec3 v0 = d * (3.0f + 4.0f * hash01(e.seed, n, 5)) * k;
    const glm::vec3 p = e.feet + r * radius + kUp * y + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
    if (p.y < e.feet.y + 0.02f) continue;
    const glm::vec3 v = v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f);
    const float u = tau / life;
    streaks_.push_back(streak(pulled(p, eye, kPull), v * 0.05f, 0.032f * k, 1.0f - u * u, glm::mix(glm::vec3(1.0f, 0.95f, 0.8f), glm::vec3(1.0f, 0.5f, 0.15f), u), 3.2f));
  }
  // --- A ring of dust rolling out from the feet. ---
  for (int i = 0; i < 16; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float life = 1.1f * (0.85f + 0.3f * hash01(e.seed, n, 1));
    const float tau = t - 0.08f - hash01(e.seed, n, 5) * 0.12f;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float a = (static_cast<float>(i) + 0.4f * hash01(e.seed, n, 2)) / 16.0f * kTau;
    const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
    const glm::vec3 p = e.feet + out * ((0.4f + 1.5f * easeOut(0.0f, 0.7f, u)) * k) + kUp * ((0.08f + 0.2f * u) * k);
    const float alpha = 0.42f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.25f, 1.0f, u));
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.3f + 0.55f * easeOut(0.0f, 1.0f, u)) * k, a + tau, hash01(e.seed, n, 3) * 37.0f, alpha,
                          kDust * (0.85f + 0.3f * hash01(e.seed, n, 4))));
  }
  // --- A flash on every foe as the edge reaches it. ---
  for (size_t i = 0; i < e.targets.size(); ++i) {
    const Target& f = e.targets[i];
    const glm::vec3 d = flatDir(f.feet - e.feet, first);
    float u = std::atan2(glm::dot(d, side0), glm::dot(d, first)) / kTau;
    if (u < 0.0f) u += 1.0f;
    const float arrival = u * kTurn;
    const float tau = t - arrival;
    if (tau < 0.0f || tau > 0.2f) continue;
    const float h = bodyHeight(f.height);
    const glm::vec3 chest = f.feet + kUp * (h * 0.45f) - d * 0.15f;
    const float amp = 1.0f - ease(0.0f, 0.2f, tau);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.4f * amp, amp, {1.0f, 0.8f, 0.55f}, 2.2f));
    lights.push_back({chest, {1.0f, 0.78f, 0.5f}, 7.0f * amp, 2.2f});
  }
}

// --- Quebra-Guarda -------------------------------------------------------------------------------------------------------

void WarriorFx::buildGuardBreak(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.5f) - e.dir * 0.15f;
  const float age = now - e.impact;  // the cut starts at e.start = impact - 0.1
  // --- The blade comes down: a cut from above the head to the chest, drawn in as it falls. ---
  downwardCut(e.feet + kUp * (e.height * 1.15f) - e.dir * 0.1f, e.feet + kUp * (e.height * 0.38f) - e.dir * 0.2f, e.dir, 0.17f * k, 1.3f, now - e.start, 0.1f, eye,
              e.feet, frame);
  if (age < 0.0f) return;
  // --- The contact: a white-orange flash and a cross glint. ---
  if (age < 0.12f) {
    const float f = 1.0f - ease(0.0f, 0.12f, age);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.5f * k * (0.6f + 0.4f * f), f, {1.0f, 0.72f, 0.42f}, 2.0f));
    streaks_.push_back(streak(pulled(chest, eye, kPull), side * 1.0f * k * f, 0.04f * k, f, {1.0f, 0.85f, 0.6f}, 2.6f));
  }
  // --- Sparks where the edge bites. ---
  for (int i = 0; i < 12; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 100u;
    const float life = 0.16f + 0.2f * hash01(e.seed, n, 5);
    const float tau = age - hash01(e.seed, n, 6) * 0.04f;
    if (tau < 0.0f || tau > life) continue;
    const glm::vec3 d = glm::normalize(e.dir * (0.3f + 0.5f * hash01(e.seed, n, 1)) + side * (hash01(e.seed, n, 2) - 0.5f) * 2.0f + kUp * (0.1f + 0.7f * hash01(e.seed, n, 3)));
    const glm::vec3 v0 = d * (2.5f + 3.5f * hash01(e.seed, n, 4)) * k;
    const glm::vec3 p = chest + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
    if (p.y < e.feet.y + 0.02f) continue;
    const float u = tau / life;
    streaks_.push_back(streak(pulled(p, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.05f, 0.03f * k, 1.0f - u * u,
                              glm::mix(glm::vec3(1.0f, 0.95f, 0.8f), glm::vec3(1.0f, 0.55f, 0.18f), u), 3.2f));
  }
  // --- Armour shards burst from the chest: lit plates of steel, thrown the blow's way and out, bouncing once. ---
  debris(e, chest, e.dir * 0.8f + kUp * 0.2f, 16, 2.2f * k, 0.06f * k, glm::vec3(0.52f, 0.55f, 0.6f), true, age, eye, 300u);
  // --- A crack ring on the ground, orange, gone in half a second. ---
  if (age < 0.5f) {
    const float r = (0.3f + 0.9f * easeOut(0.0f, 0.3f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.45f, 0.7f * (1.0f - ease(0.1f, 0.5f, age)), {1.0f, 0.5f, 0.25f}, 0.0f, glm::vec4(1.0f, 0.85f, 0.6f, 0.85f), e.feet,
         VfxBlend::Additive);
  }
  if (age < 0.25f) lights.push_back({chest, {1.0f, 0.6f, 0.35f}, 12.0f * (1.0f - ease(0.0f, 0.25f, age)), 3.0f * k});
  // --- The broken guard: two arcs of dull red light turn over the chest for a few seconds, then go out. ---
  constexpr float kRingFrom = 0.1f, kRingHold = 2.0f, kRingFade = 0.7f;
  const float ringEnv = ease(kRingFrom, kRingFrom + 0.25f, age) * (1.0f - ease(kRingFrom + kRingHold, kRingFrom + kRingHold + kRingFade, age));
  if (ringEnv > 0.0f) {
    const float r = 0.42f * k;
    const glm::vec3 at = pulled(e.feet + kUp * (e.height * 0.55f), eye, kPull);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), at) * glm::rotate(glm::mat4(1.0f), age * 1.7f, kUp) *
                        glm::rotate(glm::mat4(1.0f), glm::radians(24.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(r, r, r));
    draw(arcFirst_, arcCount_, m, kModeTrail, age, 1.9f, 0.9f * ringEnv, {1.0f, 0.22f, 0.1f}, 0.0f, glm::vec4(1.0f, 0.55f, 0.3f, 0.0f), e.feet, VfxBlend::Additive);
    lights.push_back({e.feet + kUp * (e.height * 0.55f), {1.0f, 0.3f, 0.18f}, 3.5f * ringEnv, 1.8f * k});
  }
}

// --- Grito de Guerra -----------------------------------------------------------------------------------------------------

void WarriorFx::buildWarCry(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.6f);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  const float R = e.radius;
  constexpr float kSpread = 0.45f;  // the shell reaches the radius in this long
  const glm::vec3 gold{1.0f, 0.72f, 0.38f}, core{1.0f, 0.92f, 0.75f};
  // --- The cry: a flash at the chest, a strong warm light. ---
  if (age < 0.18f) {
    const float f = 1.0f - ease(0.0f, 0.18f, age);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.9f * k * (0.5f + 0.5f * f), f, core, 2.4f));
  }
  if (age < 0.6f) lights.push_back({chest, {1.0f, 0.7f, 0.4f}, 16.0f * (1.0f - ease(0.0f, 0.6f, age)), 4.0f * k});
  // --- The pressure shell out to the radius, the ground ring with it. ---
  if (age < kSpread + 0.1f) {
    const float r = 0.3f * k + (R - 0.3f * k) * easeOut(0.0f, kSpread, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), chest) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.5f, 0.55f * (1.0f - ease(0.08f, kSpread + 0.1f, age)), gold, 0.05f, glm::vec4(core, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  if (age < 0.65f) {
    const float r = 0.4f * k + (R - 0.4f * k) * easeOut(0.0f, 0.5f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.4f, 0.7f * (1.0f - ease(0.15f, 0.65f, age)), {1.0f, 0.62f, 0.3f}, 0.0f, glm::vec4(core, 0.9f), e.feet,
         VfxBlend::Additive);
  }
  // --- Dust lifted off the ground as the ring passes. ---
  for (int i = 0; i < 24; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float ri = (0.4f + 0.6f * hash01(e.seed, n, 1)) * R;
    const float born = ri / R * 0.5f;
    const float life = 0.9f * (0.8f + 0.4f * hash01(e.seed, n, 2));
    const float tau = age - born;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float a = hash01(e.seed, n, 3) * kTau;
    const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
    const glm::vec3 p = e.feet + out * (ri + 0.5f * easeOut(0.0f, 1.0f, u)) + kUp * (0.08f + 0.45f * easeOut(0.0f, 1.0f, u));
    const float alpha = 0.38f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.3f, 1.0f, u));
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), 0.35f + 0.5f * easeOut(0.0f, 1.0f, u), a + tau * 0.7f, hash01(e.seed, n, 4) * 37.0f, alpha,
                          kDust * (0.85f + 0.3f * hash01(e.seed, n, 5))));
  }
  // --- Every ally: a column of red-gold light rises as the shell reaches it, motes in it, a warm light. ---
  int lit = 0;
  for (size_t i = 0; i < e.targets.size(); ++i) {
    const Target& a = e.targets[i];
    const float h = bodyHeight(a.height), ka = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(a.feet.x - e.feet.x, a.feet.z - e.feet.z));
    const float born = std::min(dist / R, 1.0f) * kSpread;
    const float tau = age - born;
    if (tau < 0.0f) continue;
    const float env = ease(0.0f, 0.18f, tau) * (1.0f - ease(0.65f, 1.25f, tau));
    if (env <= 0.0f) continue;
    const float rise = easeOut(0.0f, 0.3f, tau);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(a.feet, eye, kPull * 0.6f)) * glm::scale(glm::mat4(1.0f), {0.3f * ka, h * 1.15f * rise + 1e-3f, 0.3f * ka});
    draw(columnFirst_, columnCount_, m, kModeEnergy, tau, 0.5f, 0.6f * env, {1.0f, 0.45f, 0.15f}, 0.0f, glm::vec4(1.0f, 0.75f, 0.4f, 0.2f), a.feet, VfxBlend::Additive);
    const uint32_t seed = e.seed + static_cast<uint32_t>(i) * 7919u;
    for (int j = 0; j < 10; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 400u;
      const float phase = std::fmod(hash01(seed, n, 1) + tau * 0.9f, 1.0f);
      const float ang = hash01(seed, n, 2) * kTau + tau * 2.0f;
      const float r = (0.12f + 0.2f * hash01(seed, n, 3)) * ka;
      const glm::vec3 p = a.feet + glm::vec3(std::cos(ang) * r, phase * h * 1.05f, std::sin(ang) * r);
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.05f * ka, std::sin(kPi * phase) * env, {1.0f, 0.78f, 0.45f}, 2.2f));
    }
    if (lit < 4) {
      ++lit;
      lights.push_back({a.feet + kUp * (h * 0.5f), {1.0f, 0.7f, 0.4f}, 6.0f * env, 2.6f * ka});
    }
  }
}

void WarriorFx::buildFear(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>&) {
  // A dark pulse on each frightened foe: a shell that darkens what is behind it swells from the chest and thins out.
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float age = now - e.impact;
  if (age < 0.0f || age > 0.6f) return;
  for (const Target& f : e.targets) {
    const float h = bodyHeight(f.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const glm::vec3 chest = pulled(f.feet + kUp * (h * 0.5f), eye, kPull);
    const float r = (0.2f + 0.75f * easeOut(0.0f, 0.4f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), chest) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.0f, 0.75f * (1.0f - ease(0.15f, 0.6f, age)), {0.12f, 0.03f, 0.06f}, 0.1f, glm::vec4(0.35f, 0.06f, 0.1f, 0.3f),
         f.feet, VfxBlend::Alpha);
    for (int j = 0; j < 8; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 500u;
      const float phase = std::fmod(hash01(e.seed, n, 1) + age * 1.4f, 1.0f);
      const float ang = hash01(e.seed, n, 2) * kTau;
      const glm::vec3 p = f.feet + glm::vec3(std::cos(ang) * 0.3f * k, h * (0.2f + 0.8f * phase), std::sin(ang) * 0.3f * k);
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.06f * k, std::sin(kPi * phase) * (1.0f - ease(0.3f, 0.6f, age)), {0.55f, 0.1f, 0.15f}, 1.4f));
    }
  }
}

// --- Salto Devastador ------------------------------------------------------------------------------------------------------

void WarriorFx::buildLeap(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const float dist = glm::length(glm::vec2(e.to.x - e.feet.x, e.to.z - e.feet.z));
  const float arc = 1.2f + 0.25f * dist / kHex;  // the card's own arc (MapStage leapLift): the trail follows the weapon
  auto at = [&](float s) { return glm::mix(e.feet, e.to, s) + kUp * (arc * 4.0f * s * (1.0f - s) + e.height * 0.5f); };
  const float flight = e.impact - e.start;
  const float s = ease(e.start, e.impact, now);
  // --- The flight: a trail of steel light behind the weapon, sparks shed along it, a light with it. ---
  if (now < e.impact) {
    const float tail = std::min(s, 0.35f);
    if (tail > 0.01f) {
      three::CatmullRom path;
      for (int j = 0; j <= 8; ++j) path.points.push_back(pulled(at(s - tail + tail * static_cast<float>(j) / 8.0f), eye, kPull));
      const std::vector<VfxVertex> tube = vfxgeo::tube(path, 18, 0.06f * k, 8);
      draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, now - e.start, 1.2f, 0.85f, kSteel, 0.0f, glm::vec4(kWhite, 0.0f),
           e.feet, VfxBlend::Additive);
    }
    const glm::vec3 head = at(s);
    streaks_.push_back(streak(pulled(head, eye, kPull), glm::vec3(0.0f), 0.2f * k, 0.8f, kWhite, 2.0f));
    lights.push_back({head, {0.85f, 0.9f, 1.0f}, 8.0f, 3.0f * k});
  }
  for (int i = 0; i < 14; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 100u;
    const float ub = (static_cast<float>(i) + 0.5f) / 14.0f;
    const float tb = e.start + ub * flight;
    const float tau = now - tb, life = 0.3f + 0.25f * hash01(e.seed, n, 1);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 d = sphereDir(hash01(e.seed, n, 2), hash01(e.seed, n, 3));
    const glm::vec3 p = at(ub) + d * 0.4f * tau + glm::vec3(0.0f, -0.6f * tau * tau, 0.0f);
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.04f * k * (1.0f - 0.5f * u), (1.0f - u) * 0.9f, kSteel, 2.2f));
  }
  // --- The landing: the crater. ---
  const float age = now - e.impact;
  if (age < 0.0f) return;
  const glm::vec3 land = e.to;
  if (age < 0.12f) {
    const float f = 1.0f - ease(0.0f, 0.12f, age);
    streaks_.push_back(streak(pulled(land + kUp * 0.3f, eye, kGroundPull), glm::vec3(0.0f), 0.7f * k * (0.6f + 0.4f * f), f, {1.0f, 0.85f, 0.6f}, 2.2f));
    streaks_.push_back(streak(pulled(land + kUp * 0.3f, eye, kGroundPull), kUp * 1.4f * k * f, 0.05f * k, f, {1.0f, 0.85f, 0.6f}, 2.6f));
  }
  if (age < 0.28f) {
    const float r = (0.2f + 1.5f * easeOut(0.0f, 0.25f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), land + kUp * 0.3f) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.35f, 0.4f * (1.0f - ease(0.05f, 0.28f, age)), {1.0f, 0.82f, 0.62f}, 0.04f, glm::vec4(kWhite, 0.0f), land,
         VfxBlend::Additive);
  }
  if (age < 0.55f) {
    const float r = (0.3f + 2.1f * easeOut(0.0f, 0.45f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(land + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.3f, 0.65f * (1.0f - ease(0.1f, 0.55f, age)), {0.95f, 0.7f, 0.45f}, 0.0f, glm::vec4(1.0f, 0.86f, 0.66f, 0.88f), land,
         VfxBlend::Additive);
  }
  // --- A ring of dust rolling out, debris thrown on arcs. ---
  for (int i = 0; i < 18; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float life = 1.3f * (0.85f + 0.3f * hash01(e.seed, n, 1));
    const float tau = age - 0.02f;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float a = (static_cast<float>(i) + 0.4f * hash01(e.seed, n, 2)) / 18.0f * kTau;
    const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
    const glm::vec3 p = land + out * ((0.2f + 1.9f * easeOut(0.0f, 0.7f, u)) * k) + kUp * ((0.08f + 0.22f * u) * k);
    const float alpha = 0.5f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.25f, 1.0f, u));
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.35f + 0.7f * easeOut(0.0f, 1.0f, u)) * k, a + tau, hash01(e.seed, n, 3) * 37.0f, alpha,
                          kDust * (0.85f + 0.3f * hash01(e.seed, n, 4))));
  }
  {
    Effect ground = e;
    ground.feet = land;
    debris(ground, land + kUp * 0.05f, kUp * 0.3f, 14, 2.6f * k, 0.055f * k, glm::vec3(0.34f, 0.3f, 0.26f), false, age, eye, 300u);
  }
  if (age < 0.35f) lights.push_back({land + kUp * 0.5f, {1.0f, 0.78f, 0.5f}, 20.0f * (1.0f - ease(0.0f, 0.35f, age)), 4.0f * k});
}

void WarriorFx::buildLeapBlow(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.5f) - e.dir * 0.15f;
  // --- The big downward cut: from well above the head down to the knees, drawn in as the blade falls. ---
  downwardCut(e.feet + kUp * (e.height * 1.3f) - e.dir * 0.1f, e.feet + kUp * (e.height * 0.15f) - e.dir * 0.25f, e.dir, 0.26f * k, 1.6f, now - e.start, 0.12f, eye,
              e.feet, frame);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  if (age < 0.12f) {
    const float f = 1.0f - ease(0.0f, 0.12f, age);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.6f * k * (0.6f + 0.4f * f), f, {1.0f, 0.8f, 0.55f}, 2.2f));
    streaks_.push_back(streak(pulled(chest, eye, kPull), side * 1.2f * k * f, 0.045f * k, f, kWhite, 2.6f));
  }
  for (int i = 0; i < 16; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 100u;
    const float life = 0.16f + 0.22f * hash01(e.seed, n, 5);
    const float tau = age - hash01(e.seed, n, 6) * 0.04f;
    if (tau < 0.0f || tau > life) continue;
    const glm::vec3 d = glm::normalize(e.dir * (0.3f + 0.5f * hash01(e.seed, n, 1)) + side * (hash01(e.seed, n, 2) - 0.5f) * 2.2f + kUp * (0.1f + 0.7f * hash01(e.seed, n, 3)));
    const glm::vec3 v0 = d * (3.0f + 4.0f * hash01(e.seed, n, 4)) * k;
    const glm::vec3 p = chest + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
    if (p.y < e.feet.y + 0.02f) continue;
    const float u = tau / life;
    streaks_.push_back(streak(pulled(p, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.05f, 0.032f * k, 1.0f - u * u,
                              glm::mix(glm::vec3(1.0f, 0.95f, 0.8f), glm::vec3(1.0f, 0.55f, 0.18f), u), 3.4f));
  }
  if (age < 0.22f) lights.push_back({chest, {0.9f, 0.92f, 1.0f}, 12.0f * (1.0f - ease(0.0f, 0.22f, age)), 3.0f * k});
}

}  // namespace ember
