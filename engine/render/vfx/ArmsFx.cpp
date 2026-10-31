#include "render/vfx/ArmsFx.h"

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
constexpr float kRingUnderFeet = 0.6f;  // a ring this near its unit's feet height lies on the ground
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
float scaleOf(float h) { return std::clamp(h / 2.0f, 0.7f, 1.4f); }

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
// A chip of wood or metal: a thin plate.
const std::vector<VfxVertex>& chipGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(1.0f, 0.22f, 0.8f);
  return g;
}
// An open cylinder of radius 1 from y 0 to 1 (u around, v up), normals outward: a column of light.
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
// A rune ring: three arcs of a thin torus with gaps, lying flat (around +y); the sigil over a provoked head, the summoning rune.
const std::vector<VfxVertex>& runeGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> out;
    const glm::mat4 flat = glm::rotate(glm::mat4(1.0f), -kPi * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    for (int k = 0; k < 3; ++k) {
      std::vector<VfxVertex> arc = vfxgeo::torus(1.0f, 0.06f, 6, 32, 1.75f);
      const glm::mat4 m = flat * glm::rotate(glm::mat4(1.0f), static_cast<float>(k) * kTau / 3.0f, glm::vec3(0.0f, 0.0f, 1.0f));
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
    // A ring at the feet lies under every card (the user, 2026-10-30: "o poison tinha que estar abaixo de todos os pés").
    if (mode == kModeRing && std::abs(model[3].y - sight.y) < kRingUnderFeet) d.underCards = true;
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
// A frame whose y axis is `y` (x and z square to it): a disc stood up to face `y`, a column laid along it.
glm::mat4 basis(glm::vec3 y) {
  y = glm::normalize(y);
  const glm::vec3 ref = std::abs(y.y) < 0.9f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 x = glm::normalize(glm::cross(ref, y)), z = glm::cross(x, y);
  return glm::mat4(glm::vec4(x, 0.0f), glm::vec4(y, 0.0f), glm::vec4(z, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

constexpr glm::vec3 kSteel{0.78f, 0.86f, 1.0f}, kWhite{1.0f, 0.98f, 0.94f};
constexpr glm::vec3 kDust{0.46f, 0.4f, 0.33f}, kStone{0.34f, 0.3f, 0.26f};
constexpr glm::vec3 kRed{1.0f, 0.16f, 0.1f}, kRedCore{1.0f, 0.62f, 0.5f};
constexpr glm::vec3 kGold{1.0f, 0.74f, 0.32f}, kGoldCore{1.0f, 0.95f, 0.78f};
constexpr glm::vec3 kBlood{0.45f, 0.03f, 0.04f};

struct SummonPalette {
  glm::vec3 energy, core, light;
};
SummonPalette summonPalette(ArmsFx::Summon s) {
  switch (s) {
    case ArmsFx::Summon::Familiar: return {{0.62f, 0.38f, 1.0f}, {0.9f, 0.82f, 1.0f}, {0.7f, 0.5f, 1.0f}};
    case ArmsFx::Summon::Greater: return {{0.28f, 0.55f, 1.0f}, {0.8f, 0.92f, 1.0f}, {0.4f, 0.65f, 1.0f}};
    case ArmsFx::Summon::Radiant: return {{1.0f, 0.86f, 0.5f}, {1.0f, 1.0f, 0.92f}, {1.0f, 0.9f, 0.65f}};
    case ArmsFx::Summon::Titan: return {{1.0f, 0.42f, 0.12f}, {1.0f, 0.86f, 0.6f}, {1.0f, 0.55f, 0.25f}};
    case ArmsFx::Summon::ZombieDog: return {{0.45f, 0.85f, 0.2f}, {0.85f, 1.0f, 0.65f}, {0.5f, 0.9f, 0.3f}};
  }
  return {{0.62f, 0.38f, 1.0f}, {0.9f, 0.82f, 1.0f}, {0.7f, 0.5f, 1.0f}};
}

}  // namespace

float ArmsFx::Effect::end() const {
  switch (kind) {
    case Kind::Provoke: return impact + 2.0f;
    case Kind::ShieldBash: return impact + 1.6f;
    case Kind::BullRush: return impact + 1.7f;
    case Kind::Executioner: return impact + 1.6f;
    case Kind::Trip: return impact + 1.4f;
    case Kind::ShoulderSmash: return impact + 1.6f;
    case Kind::Stampede: return impact + 0.09f * static_cast<float>(hexes.size()) + 1.6f;
    case Kind::DivineWrath: return impact + 0.06f * static_cast<float>(hexes.size()) + 1.4f;
    case Kind::Aura: return impact + 2.2f;
    case Kind::Summon: return impact + 1.0f;
    case Kind::ReflectWard: return impact + 1.9f;
    case Kind::ReflectHit: return impact + 0.9f;
    case Kind::Phantom: return impact + 0.5f;
    case Kind::Spectral: return impact + 1.3f;
  }
  return impact + 1.0f;
}

void ArmsFx::provoke(glm::vec3 feet, float height, const std::vector<Target>& foes, float impact) {
  Effect e;
  e.kind = Kind::Provoke, e.feet = feet, e.height = bodyHeight(height), e.impact = impact, e.start = impact, e.targets = foes;
  e.seed = 0x51a7u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::shieldBash(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::ShieldBash, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5b11u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::bullRush(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::BullRush, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.45f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x8b07u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::executioner(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::Executioner, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.3f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0xe3c5u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::trip(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::Trip, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.14f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x7419u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::shoulderSmash(glm::vec3 from, float height, const std::vector<glm::vec3>& hexes, const std::vector<Target>& bodies, float impact) {
  if (hexes.empty()) return;
  Effect e;
  e.kind = Kind::ShoulderSmash, e.from = from, e.height = bodyHeight(height), e.impact = impact, e.start = impact - 0.08f;
  e.hexes = hexes, e.targets = bodies;
  glm::vec3 centre{0.0f};
  for (const glm::vec3& h : hexes) centre += h;
  centre /= static_cast<float>(hexes.size());
  e.feet = centre;
  e.dir = flatDir(centre - from, glm::vec3(0.0f, 0.0f, 1.0f));
  float half = 0.0f;
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  for (const glm::vec3& h : hexes) half = std::max(half, std::abs(glm::dot(h - centre, side)));
  e.radius = half + 0.9f;  // the wall's half width
  e.seed = 0x5d0fu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::stampede(glm::vec3 from, float height, const std::vector<glm::vec3>& hexes, const std::vector<Target>& bodies, float impact) {
  if (hexes.empty()) return;
  Effect e;
  e.kind = Kind::Stampede, e.from = from, e.feet = hexes.front(), e.height = bodyHeight(height), e.impact = impact, e.start = impact - 0.1f;
  e.hexes = hexes, e.targets = bodies;
  e.dir = flatDir(hexes.back() - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x57a3u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::divineWrath(glm::vec3 from, float height, const std::vector<glm::vec3>& hexes, const std::vector<Target>& bodies, float impact) {
  if (hexes.empty()) return;
  Effect e;
  e.kind = Kind::DivineWrath, e.from = from, e.feet = hexes.front(), e.height = bodyHeight(height), e.impact = impact, e.start = impact - 0.15f;
  e.hexes = hexes, e.targets = bodies;
  e.dir = flatDir(hexes.back() - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0xd1a9u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::aura(bool protection, glm::vec3 feet, float height, float radius, const std::vector<Target>& bodies, float impact) {
  Effect e;
  e.kind = Kind::Aura, e.protection = protection, e.feet = feet, e.height = bodyHeight(height), e.radius = std::max(radius, 1.2f * kHex);
  e.impact = impact, e.start = impact - 0.2f, e.targets = bodies;
  e.seed = (protection ? 0xa0f1u : 0x1a7eu) + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::summon(Summon style, glm::vec3 at, float impact) {
  Effect e;
  e.kind = Kind::Summon, e.style = style, e.feet = at, e.impact = impact, e.start = impact - 0.9f;
  e.seed = 0x5c11u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void ArmsFx::reflectWard(glm::vec3 feet, float height, float impact) {
  Effect e;
  e.kind = Kind::ReflectWard, e.feet = feet, e.height = bodyHeight(height), e.impact = impact, e.start = impact - 0.35f;
  e.seed = 0x7ef1u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::reflectHit(glm::vec3 from, float fromHeight, glm::vec3 to, float toHeight, float start, float impact) {
  Effect e;
  e.kind = Kind::ReflectHit, e.from = from, e.radius = bodyHeight(fromHeight), e.feet = to, e.height = bodyHeight(toHeight);
  e.start = start, e.impact = std::max(impact, start + 0.12f);
  e.dir = flatDir(to - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x3ef9u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

float ArmsFx::spectralFlight(glm::vec3 from, glm::vec3 to) { return std::clamp(glm::length(to - from) / 14.0f, 0.22f, 0.6f); }
void ArmsFx::spectralBolt(glm::vec3 from, float height, const std::vector<glm::vec3>& hexes, const std::vector<Target>& bodies, float start, float impact) {
  if (hexes.empty()) return;
  Effect e;
  e.kind = Kind::Spectral, e.from = from, e.height = bodyHeight(height), e.hexes = hexes, e.targets = bodies;
  glm::vec3 centre{0.0f};
  for (const glm::vec3& h : hexes) centre += h;
  e.feet = centre / static_cast<float>(hexes.size());
  e.start = start, e.impact = std::max(impact, start + 0.15f);
  e.dir = flatDir(e.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5bc7u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ArmsFx::phantom(const Target& foe, float impact) {
  Effect e;
  e.kind = Kind::Phantom, e.feet = foe.feet, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.55f;
  e.seed = 0xf47au + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

bool ArmsFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

void ArmsFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  rockFirst_ = frame.addVertices(rockGeometry()), rockCount_ = static_cast<uint32_t>(rockGeometry().size());
  chipFirst_ = frame.addVertices(chipGeometry()), chipCount_ = static_cast<uint32_t>(chipGeometry().size());
  columnFirst_ = frame.addVertices(columnGeometry()), columnCount_ = static_cast<uint32_t>(columnGeometry().size());
  runeFirst_ = frame.addVertices(runeGeometry()), runeCount_ = static_cast<uint32_t>(runeGeometry().size());
  streaks_.clear(), puffs_.clear(), rocks_.clear(), chips_.clear();
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    switch (e.kind) {
      case Kind::Provoke: buildProvoke(e, now, camera, frame, lights); break;
      case Kind::ShieldBash: buildShieldBash(e, now, camera, frame, lights); break;
      case Kind::BullRush: buildBullRush(e, now, camera, frame, lights); break;
      case Kind::Executioner: buildExecutioner(e, now, camera, frame, lights); break;
      case Kind::Trip: buildTrip(e, now, camera, frame, lights); break;
      case Kind::ShoulderSmash: buildShoulderSmash(e, now, camera, frame, lights); break;
      case Kind::Stampede: buildStampede(e, now, camera, frame, lights); break;
      case Kind::DivineWrath: buildDivineWrath(e, now, camera, frame, lights); break;
      case Kind::Aura: buildAura(e, now, camera, frame, lights); break;
      case Kind::Summon: buildSummon(e, now, camera, frame, lights); break;
      case Kind::ReflectWard: buildReflectWard(e, now, camera, frame, lights); break;
      case Kind::ReflectHit: buildReflectHit(e, now, camera, frame, lights); break;
      case Kind::Phantom: buildPhantom(e, now, camera, frame, lights); break;
      case Kind::Spectral: buildSpectral(e, now, camera, frame, lights); break;
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
    d.gpu.params0 = {set == 0 ? 0.92f : 0.5f, set == 0 ? 0.0f : 0.4f, 0.0f, 0.0f};  // roughness, metalness
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

void ArmsFx::flash(glm::vec3 at, glm::vec3 across, float size, glm::vec3 colour, float age, float life, glm::vec3 eye, float pull) {
  if (age < 0.0f || age > life) return;
  const float f = 1.0f - ease(0.0f, life, age);
  streaks_.push_back(streak(pulled(at, eye, pull), glm::vec3(0.0f), size * (0.6f + 0.4f * f), f, colour, 2.2f));
  if (glm::dot(across, across) > 1e-6f) streaks_.push_back(streak(pulled(at, eye, pull), across * f, 0.045f * size, f, kWhite, 2.6f));
}

void ArmsFx::groundRing(glm::vec3 feet, float from, float to, float grow, float life, float intensity, float opacity, glm::vec3 colour, glm::vec3 core, float age,
                        glm::vec3 eye, VfxFrame& frame, glm::vec3 sight) {
  if (age < 0.0f || age > life) return;
  const float r = from + (to - from) * easeOut(0.0f, grow, age);
  const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
  const Draw draw{frame};
  draw(discFirst_, discCount_, m, kModeRing, age, intensity, opacity * (1.0f - ease(life * 0.3f, life, age)), colour, 0.0f, glm::vec4(core, 0.86f), sight,
       VfxBlend::Additive);
}

void ArmsFx::sparks(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 hot, glm::vec3 cool, float age, glm::vec3 eye,
                    uint32_t salt) {
  for (int i = 0; i < count; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + salt;
    const float life = 0.16f + 0.24f * hash01(e.seed, n, 5);
    const float tau = age - hash01(e.seed, n, 6) * 0.04f;
    if (tau < 0.0f || tau > life) continue;
    const glm::vec3 d = glm::normalize(bias + sphereDir(hash01(e.seed, n, 1), hash01(e.seed, n, 2)) * 0.8f + kUp * (0.2f + 0.5f * hash01(e.seed, n, 3)));
    const glm::vec3 v0 = d * speed * (0.6f + 0.8f * hash01(e.seed, n, 4));
    const glm::vec3 p = at + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
    if (p.y < e.feet.y + 0.02f) continue;
    const float u = tau / life;
    streaks_.push_back(streak(pulled(p, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.05f, size, 1.0f - u * u, glm::mix(hot, cool, u), 3.2f));
  }
}

void ArmsFx::dustRing(const Effect& e, glm::vec3 at, glm::vec3 push, int count, float reach, float size, float age, float life, float alpha, glm::vec3 eye,
                      uint32_t salt) {
  for (int i = 0; i < count; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + salt;
    const float lf = life * (0.85f + 0.3f * hash01(e.seed, n, 1));
    const float tau = age - hash01(e.seed, n, 5) * 0.08f;
    if (tau < 0.0f || tau > lf) continue;
    const float u = tau / lf;
    const float a = (static_cast<float>(i) + 0.4f * hash01(e.seed, n, 2)) / static_cast<float>(count) * kTau;
    const glm::vec3 out = glm::normalize(glm::vec3(std::cos(a), 0.0f, std::sin(a)) + push);
    const glm::vec3 p = at + out * (0.25f + reach * easeOut(0.0f, 0.7f, u)) + kUp * (0.08f + 0.25f * u);
    const float al = alpha * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.25f, 1.0f, u));
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), size * (0.6f + 1.0f * easeOut(0.0f, 1.0f, u)), a + tau, hash01(e.seed, n, 3) * 37.0f, al,
                          kDust * (0.85f + 0.3f * hash01(e.seed, n, 4))));
  }
}

void ArmsFx::debris(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 colour, bool chips, float age, glm::vec3 eye,
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

// --- Provocar ----------------------------------------------------------------------------------------------------------------

void ArmsFx::buildProvoke(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.55f);
  const glm::vec3 toEye = flatDir(eye - e.feet, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 side = glm::normalize(glm::cross(kUp, toEye));
  // --- The clang: a white-red flash at the weapon, a cross glint, sparks, a red light. ---
  flash(chest + toEye * 0.3f, side * 1.1f * k, 0.6f * k, kRedCore, age, 0.16f, eye, kPull);
  sparks(e, chest + toEye * 0.3f, toEye * 0.3f, 14, 3.5f * k, 0.03f * k, kWhite, {1.0f, 0.35f, 0.15f}, age, eye, 100u);
  if (age < 0.5f) lights.push_back({chest, {1.0f, 0.35f, 0.2f}, 12.0f * (1.0f - ease(0.0f, 0.5f, age)), 3.2f * k});
  // --- The challenge rolls out: a red shell and a ring over the ground, out to the farthest foe (else three hexes). ---
  float reach = 3.0f * kHex;
  for (const Target& f : e.targets) reach = std::max(reach, glm::length(glm::vec2(f.feet.x - e.feet.x, f.feet.z - e.feet.z)) + 0.8f);
  constexpr float kSpread = 0.5f;
  if (age < kSpread + 0.1f) {
    const float r = 0.3f * k + (reach - 0.3f * k) * easeOut(0.0f, kSpread, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), e.feet + kUp * 0.6f) * glm::scale(glm::mat4(1.0f), {r, 0.45f * r, r});
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.55f, 0.5f * (1.0f - ease(0.1f, kSpread + 0.1f, age)), kRed, 0.0f, glm::vec4(kRedCore, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  groundRing(e.feet, 0.4f * k, reach, kSpread, 0.75f, 0.5f, 0.75f, kRed, kRedCore, age, eye, frame, e.feet);
  // --- Every foe it reaches: the tether drawn taut to the warrior, then snapping; a red sigil turning over the head. ---
  int lit = 0;
  for (size_t i = 0; i < e.targets.size(); ++i) {
    const Target& f = e.targets[i];
    const float h = bodyHeight(f.height), kf = scaleOf(h);
    const float dist = glm::length(glm::vec2(f.feet.x - e.feet.x, f.feet.z - e.feet.z));
    const float tau = age - std::min(dist / reach, 1.0f) * kSpread;
    if (tau < 0.0f) continue;
    const glm::vec3 theirChest = f.feet + kUp * (h * 0.55f);
    // the tether: on in 0.12 s, taut for 0.35 s, then it snaps back to the foe and goes out
    const float on = easeOut(0.0f, 0.12f, tau), snap = ease(0.47f, 0.62f, tau);
    if (snap < 1.0f) {
      const float reachFrom = snap, reachTo = on;
      if (reachTo > reachFrom + 0.02f) {
        three::CatmullRom path;
        for (int j = 0; j <= 10; ++j) {
          const float s = reachFrom + (reachTo - reachFrom) * static_cast<float>(j) / 10.0f;
          const float sag = 0.25f * std::sin(kPi * s) * (1.0f - on * 0.8f);
          path.points.push_back(pulled(glm::mix(theirChest, chest, s) - kUp * sag, eye, kPull));
        }
        const std::vector<VfxVertex> tube = vfxgeo::tube(path, 20, 0.035f * kf, 6);
        draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, tau, 1.6f, 0.9f * (1.0f - snap), kRed, 0.0f,
             glm::vec4(kRedCore, 0.0f), f.feet, VfxBlend::Additive);
      }
    }
    // the sigil over the head: two seconds, turning
    const float env = ease(0.0f, 0.2f, tau) * (1.0f - ease(1.1f, 1.45f, tau));
    if (env > 0.0f) {
      const float r = 0.36f * kf;
      const glm::vec3 at = pulled(f.feet + kUp * (h * 1.08f), eye, kPull);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), at) * glm::rotate(glm::mat4(1.0f), tau * 2.2f, kUp) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(18.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(runeFirst_, runeCount_, m, kModeTrail, tau, 2.0f, 0.95f * env, kRed, 0.0f, glm::vec4(kRedCore, 0.0f), f.feet, VfxBlend::Additive);
    }
    flash(theirChest, glm::vec3(0.0f), 0.4f * kf, kRedCore, tau, 0.14f, eye, kPull);
    if (lit < 4 && env > 0.0f) {
      ++lit;
      lights.push_back({f.feet + kUp * (h * 1.0f), {1.0f, 0.25f, 0.15f}, 4.0f * env, 1.8f * kf});
    }
  }
}

// --- Golpe de Escudo ---------------------------------------------------------------------------------------------------------

void ArmsFx::buildShieldBash(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.5f) - e.dir * 0.3f;
  // --- The shield's face meets the chest: a gold flash, a vertical shock ripple facing the blow (two rings). ---
  flash(chest, side * 1.0f * k, 0.75f * k, kGoldCore, age, 0.14f, eye, kPull);
  for (int ring = 0; ring < 2; ++ring) {
    const float tau = age - 0.06f * static_cast<float>(ring);
    if (tau < 0.0f || tau > 0.42f) continue;
    const float r = (0.25f + 0.95f * easeOut(0.0f, 0.32f, tau)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(chest, eye, kPull)) * basis(e.dir) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, tau, 0.9f, 0.85f * (1.0f - ease(0.1f, 0.42f, tau)), kGold, 0.0f, glm::vec4(kGoldCore, 0.8f), e.feet, VfxBlend::Additive);
  }
  if (age < 0.3f) {
    const float r = (0.2f + 0.6f * easeOut(0.0f, 0.25f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), chest) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.5f, 0.55f * (1.0f - ease(0.05f, 0.3f, age)), kGold, 0.08f, glm::vec4(kGoldCore, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  sparks(e, chest, e.dir * 0.6f, 12, 3.0f * k, 0.03f * k, kWhite, {1.0f, 0.6f, 0.2f}, age, eye, 100u);
  // --- Dust at the feet pushed the blow's way, a few chips of the guard. ---
  dustRing(e, e.feet, e.dir * 0.6f, 10, 1.1f * k, 0.3f * k, age, 0.9f, 0.4f, eye, 200u);
  debris(e, chest, e.dir * 0.8f + kUp * 0.2f, 6, 1.8f * k, 0.05f * k, glm::vec3(0.5f, 0.45f, 0.38f), true, age, eye, 300u);
  if (age < 0.25f) lights.push_back({chest, {1.0f, 0.78f, 0.4f}, 12.0f * (1.0f - ease(0.0f, 0.25f, age)), 3.0f * k});
  // --- Stunned: gold stars circling over the head. ---
  const float env = ease(0.12f, 0.3f, age) * (1.0f - ease(1.2f, 1.6f, age));
  if (env > 0.0f) {
    const glm::vec3 crown = e.feet + kUp * (e.height * 1.05f);
    for (int j = 0; j < 5; ++j) {
      const float a = static_cast<float>(j) / 5.0f * kTau + age * 4.0f;
      const glm::vec3 p = crown + glm::vec3(std::cos(a) * 0.32f * k, 0.06f * std::sin(a * 2.0f + age * 6.0f) * k, std::sin(a) * 0.32f * k);
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.09f * k, env, kGoldCore, 2.4f));
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(std::cos(a + 1.6f), 0.0f, std::sin(a + 1.6f)) * 0.12f * k, 0.02f * k, env * 0.8f, kGold, 2.0f));
    }
    lights.push_back({crown, {1.0f, 0.85f, 0.45f}, 2.5f * env, 1.4f * k});
  }
}

// --- Investida Touro ---------------------------------------------------------------------------------------------------------

void ArmsFx::buildBullRush(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  // --- Dust torn up behind the charge: puffs along the last hexes of the run, left as the striker passed. ---
  const float run = now - e.start;
  for (int i = 0; i < 14; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 400u;
    const float s = static_cast<float>(i) / 13.0f;  // 0 = far back, 1 = beside the foe
    const float born = s * 0.42f;
    const float tau = run - born;
    const float life = 0.9f * (0.8f + 0.4f * hash01(e.seed, n, 1));
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 base = e.feet - e.dir * (0.9f + (1.0f - s) * 2.2f * kHex) + side * (hash01(e.seed, n, 2) - 0.5f) * 0.9f;
    const glm::vec3 p = base + kUp * ((0.1f + 0.45f * easeOut(0.0f, 1.0f, u)) * k) - e.dir * (0.3f * u);
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.3f + 0.55f * easeOut(0.0f, 1.0f, u)) * k, hash01(e.seed, n, 3) * kTau + tau, hash01(e.seed, n, 4) * 37.0f,
                          0.42f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.3f, 1.0f, u)), kDust * (0.85f + 0.3f * hash01(e.seed, n, 5))));
  }
  const float age = now - e.impact;
  if (age < 0.0f) return;
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.45f) - e.dir * 0.3f;
  // --- The blow: a flash, a horned burst flung forward (two fans of streaks), a shock shell, a skid ring stretched the push's way. ---
  flash(chest, side * 1.3f * k, 0.9f * k, {1.0f, 0.85f, 0.6f}, age, 0.14f, eye, kPull);
  for (int i = 0; i < 26; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 100u;
    const float life = 0.22f + 0.25f * hash01(e.seed, n, 1);
    const float tau = age - hash01(e.seed, n, 2) * 0.03f;
    if (tau < 0.0f || tau > life) continue;
    const float horn = i % 2 == 0 ? 1.0f : -1.0f;  // two fans, either side of the push
    const glm::vec3 d = glm::normalize(e.dir * 1.2f + side * horn * (0.4f + 0.5f * hash01(e.seed, n, 3)) + kUp * (0.1f + 0.5f * hash01(e.seed, n, 4)));
    const float speed = (4.0f + 4.0f * hash01(e.seed, n, 5)) * k;
    constexpr float kDrag = 5.0f;
    const glm::vec3 p = chest + d * speed * (1.0f - std::exp(-kDrag * tau)) / kDrag;
    const float u = tau / life;
    streaks_.push_back(streak(pulled(p, eye, kPull), d * speed * std::exp(-kDrag * tau) * 0.06f, 0.035f * k, (1.0f - u) * (1.0f - u),
                              glm::mix(kWhite, glm::vec3(1.0f, 0.6f, 0.3f), u), 2.6f));
  }
  if (age < 0.3f) {
    const float r = (0.25f + 1.2f * easeOut(0.0f, 0.26f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), chest + e.dir * 0.3f) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.4f, 0.45f * (1.0f - ease(0.05f, 0.3f, age)), {1.0f, 0.85f, 0.65f}, 0.04f, glm::vec4(kWhite, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  if (age < 0.6f) {
    const float r = (0.3f + 1.4f * easeOut(0.0f, 0.4f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + e.dir * 0.5f + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) *
                        glm::mat4(glm::vec4(side, 0.0f), glm::vec4(kUp, 0.0f), glm::vec4(e.dir, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) *
                        glm::scale(glm::mat4(1.0f), {r * 0.75f, 1.0f, r * 1.35f});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.35f, 0.65f * (1.0f - ease(0.12f, 0.6f, age)), {0.95f, 0.72f, 0.45f}, 0.0f, glm::vec4(1.0f, 0.86f, 0.66f, 0.85f),
         e.feet, VfxBlend::Additive);
  }
  // --- Dust and stones thrown forward. ---
  dustRing(e, e.feet, e.dir * 0.9f, 14, 1.7f * k, 0.35f * k, age, 1.2f, 0.48f, eye, 200u);
  debris(e, e.feet + kUp * 0.1f, e.dir * 1.1f + kUp * 0.2f, 12, 2.4f * k, 0.055f * k, kStone, false, age, eye, 300u);
  if (age < 0.35f) lights.push_back({chest, {1.0f, 0.8f, 0.55f}, 16.0f * (1.0f - ease(0.0f, 0.35f, age)), 3.6f * k});
}

// --- Golpe do Carrasco -------------------------------------------------------------------------------------------------------

void ArmsFx::buildExecutioner(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.5f) - e.dir * 0.2f;
  const float lead = now - e.start;  // the pillar falls over [start, impact]
  // --- A pillar of red light falls on the foe from above, thin, as the blade is raised. ---
  if (lead >= 0.0f && now < e.impact + 0.25f) {
    const float fall = easeOut(0.0f, e.impact - e.start, lead);
    const float out = 1.0f - ease(e.impact, e.impact + 0.25f, now);
    const float top = e.height * 2.6f, bottom = top - (top - e.height * 0.2f) * fall;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + kUp * bottom, eye, kPull)) *
                        glm::scale(glm::mat4(1.0f), {0.16f * k, std::max(top - bottom, 1e-3f), 0.16f * k});
    draw(columnFirst_, columnCount_, m, kModeEnergy, lead, 1.1f, 0.7f * out, kRed, 0.0f, glm::vec4(kRedCore, 0.25f), e.feet, VfxBlend::Additive);
  }
  // --- The heavy cut: a dark-red sheet from above the head to the knees, drawn in as the blade falls. ---
  {
    const float cutAge = now - (e.impact - 0.12f);
    constexpr float kSweep = 0.12f, kHold = 0.06f, kFade = 0.3f;
    const float head = easeOut(0.0f, kSweep, cutAge);
    const float tail = ease(kSweep * 0.5f, kSweep + kHold + kFade, cutAge);
    const float opacity = 1.0f - ease(kSweep + kHold, kSweep + kHold + kFade, cutAge);
    if (cutAge >= 0.0f && opacity > 0.0f && head > tail) {
      constexpr int kSegments = 24;
      const glm::vec3 top = e.feet + kUp * (e.height * 1.3f) - e.dir * 0.1f, bottom = e.feet + kUp * (e.height * 0.12f) - e.dir * 0.25f;
      std::vector<VfxVertex> sheet;
      std::vector<glm::vec3> edge(kSegments + 1), inner(kSegments + 1);
      for (int j = 0; j <= kSegments; ++j) {
        const float s = static_cast<float>(j) / kSegments;
        const float w = 0.3f * k * std::pow(std::sin(kPi * s), 0.6f) + 0.004f;
        const glm::vec3 p = glm::mix(top, bottom, s) - e.dir * (0.3f * std::sin(kPi * s));
        edge[j] = pulled(p, eye, kPull);
        inner[j] = pulled(p + e.dir * w, eye, kPull);
      }
      for (int j = 0; j < kSegments; ++j) {
        const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
        const VfxVertex a0{inner[j], s0, side, 0.0f}, a1{edge[j], s0, side, 1.0f}, b0{inner[j + 1], s1, side, 0.0f}, b1{edge[j + 1], s1, side, 1.0f};
        sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
      }
      draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, cutAge, 1.8f, opacity, {0.85f, 0.08f, 0.08f}, tail,
           glm::vec4(kRedCore, head), e.feet, VfxBlend::Additive);
    }
  }
  const float age = now - e.impact;
  if (age < 0.0f) return;
  // --- The blow lands: a red flash, a red spray darkening as it flies, dark dust, a red ring, a red light that lingers. ---
  flash(chest, side * 1.2f * k, 0.75f * k, kRedCore, age, 0.16f, eye, kPull);
  for (int i = 0; i < 22; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 100u;
    const float life = 0.3f + 0.35f * hash01(e.seed, n, 1);
    const float tau = age - hash01(e.seed, n, 2) * 0.03f;
    if (tau < 0.0f || tau > life) continue;
    const glm::vec3 d = glm::normalize(sphereDir(hash01(e.seed, n, 3), hash01(e.seed, n, 4)) + e.dir * 1.1f + kUp * 0.3f);
    const glm::vec3 v0 = d * (2.5f + 3.5f * hash01(e.seed, n, 5)) * k;
    const glm::vec3 p = chest + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
    if (p.y < e.feet.y + 0.02f) continue;
    const float u = tau / life;
    streaks_.push_back(streak(pulled(p, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.05f, 0.035f * k, (1.0f - u) * (1.0f - u),
                              glm::mix(kRedCore, glm::vec3(0.5f, 0.03f, 0.05f), std::min(1.0f, u * 1.4f)), 2.6f));
  }
  for (int i = 0; i < 8; ++i) {  // dark dust at the feet
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float life = 1.0f * (0.85f + 0.3f * hash01(e.seed, n, 1));
    const float tau = age - 0.02f;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float a = (static_cast<float>(i) + 0.4f * hash01(e.seed, n, 2)) / 8.0f * kTau;
    const glm::vec3 p = e.feet + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * ((0.25f + 1.0f * easeOut(0.0f, 0.7f, u)) * k) + kUp * ((0.1f + 0.2f * u) * k);
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.3f + 0.5f * easeOut(0.0f, 1.0f, u)) * k, a + tau, hash01(e.seed, n, 3) * 37.0f,
                          0.45f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.25f, 1.0f, u)), glm::vec3(0.22f, 0.16f, 0.15f)));
  }
  groundRing(e.feet, 0.3f * k, 1.4f * k, 0.4f, 0.7f, 0.6f, 0.75f, kRed, kRedCore, age, eye, frame, e.feet);
  if (age < 1.2f) lights.push_back({chest, {1.0f, 0.2f, 0.12f}, 16.0f * std::exp(-age * 3.2f), 3.4f * k});
}

// --- Rasteira ----------------------------------------------------------------------------------------------------------------

void ArmsFx::buildTrip(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  // --- A low crescent of steel at the ankles: an arc in front of the foe, swept from one side to the other. ---
  {
    const float t = now - e.start;
    constexpr float kSweep = 0.16f;
    const float head = easeOut(0.0f, kSweep, t);
    const float tail = ease(kSweep * 0.4f, kSweep + 0.3f, t);
    const float opacity = 1.0f - ease(kSweep + 0.05f, kSweep + 0.3f, t);
    if (t >= 0.0f && opacity > 0.0f && head > tail) {
      constexpr int kSegments = 32;
      const glm::vec3 centre = e.feet - e.dir * 0.35f + kUp * (0.22f * k);
      const float radius = 0.85f * k;
      std::vector<VfxVertex> sheet;
      std::vector<glm::vec3> edge(kSegments + 1), inner(kSegments + 1);
      for (int j = 0; j <= kSegments; ++j) {
        const float s = static_cast<float>(j) / kSegments;
        const float a = (-0.62f + 1.24f * s) * kPi;  // from one side round the front to the other
        const glm::vec3 r = side * std::sin(a) - e.dir * std::cos(a);  // round the striker's side of the foe
        const glm::vec3 p = centre + r * radius + kUp * (0.05f * std::sin(kPi * s));
        edge[j] = pulled(p, eye, kGroundPull);
        inner[j] = pulled(p - r * (0.32f * k), eye, kGroundPull);
      }
      for (int j = 0; j < kSegments; ++j) {
        const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
        const VfxVertex a0{inner[j], s0, kUp, 0.0f}, a1{edge[j], s0, kUp, 1.0f}, b0{inner[j + 1], s1, kUp, 0.0f}, b1{edge[j + 1], s1, kUp, 1.0f};
        sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
      }
      draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, t, 1.7f, opacity, kSteel, tail, glm::vec4(kWhite, head), e.feet,
           VfxBlend::Additive);
    }
  }
  const float age = now - e.impact;
  if (age < 0.0f) return;
  const glm::vec3 ankle = e.feet + kUp * (0.25f * k) - e.dir * 0.2f;
  // --- The bite at the ankles: a flash, sparks low along the ground, dust kicked up sideways. ---
  flash(ankle, side * 0.8f * k, 0.45f * k, {1.0f, 0.85f, 0.7f}, age, 0.12f, eye, kGroundPull);
  sparks(e, ankle, side * 0.5f - kUp * 0.1f, 10, 2.6f * k, 0.028f * k, kWhite, {1.0f, 0.55f, 0.2f}, age, eye, 100u);
  dustRing(e, e.feet, glm::vec3(0.0f), 12, 1.0f * k, 0.3f * k, age, 1.0f, 0.42f, eye, 200u);
  // --- Blood: dark drops falling from the cut, a dark stain spreading on the ground. ---
  for (int i = 0; i < 14; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 300u;
    const float life = 0.35f + 0.3f * hash01(e.seed, n, 1);
    const float tau = age - hash01(e.seed, n, 2) * 0.1f;
    if (tau < 0.0f || tau > life) continue;
    const float a = hash01(e.seed, n, 3) * kTau;
    const glm::vec3 v0 = glm::vec3(std::cos(a) * 0.6f, 0.8f + 1.2f * hash01(e.seed, n, 4), std::sin(a) * 0.6f) * k;
    const glm::vec3 p = ankle + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
    if (p.y < e.feet.y + 0.02f) continue;
    streaks_.push_back(streak(pulled(p, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.04f, 0.03f * k, 0.9f, {0.75f, 0.06f, 0.06f}, 1.3f));
  }
  if (age < 1.3f) {
    const float r = (0.15f + 0.4f * easeOut(0.05f, 0.6f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.03f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(sphereFirst_, sphereCount_, glm::scale(m, glm::vec3(1.0f, 0.02f, 1.0f)), kModeEnergy, age, 1.0f, 0.55f * (1.0f - ease(0.9f, 1.3f, age)), kBlood, 0.4f,
         glm::vec4(kBlood, 0.4f), e.feet, VfxBlend::Alpha);
  }
  groundRing(e.feet, 0.3f * k, 0.9f * k, 0.3f, 0.5f, 0.35f, 0.55f, {0.6f, 0.62f, 0.66f}, {0.95f, 0.95f, 0.95f}, age, eye, frame, e.feet);
  if (age < 0.2f) lights.push_back({ankle, {1.0f, 0.85f, 0.7f}, 6.0f * (1.0f - ease(0.0f, 0.2f, age)), 2.0f * k});
}

// --- Investida de Ombro ------------------------------------------------------------------------------------------------------

void ArmsFx::buildShoulderSmash(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const float age = now - e.impact;
  if (age < 0.0f) return;
  // --- The wall of pressure: a flattened shell standing across the hexes, pushed out the blow's way and thinning. ---
  const glm::vec3 base = e.from + e.dir * (0.9f * kHex);
  if (age < 0.5f) {
    const float push = easeOut(0.0f, 0.45f, age) * 1.6f * kHex;
    const glm::vec3 at = base + e.dir * push + kUp * (0.7f * k);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), at) * glm::mat4(glm::vec4(side, 0.0f), glm::vec4(kUp, 0.0f), glm::vec4(e.dir, 0.0f), glm::vec4(0, 0, 0, 1)) *
                        glm::scale(glm::mat4(1.0f), {e.radius, 0.85f * k, 0.22f + 0.25f * age});
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.6f, 0.6f * (1.0f - ease(0.15f, 0.5f, age)), {1.0f, 0.82f, 0.6f}, 0.05f, glm::vec4(kWhite, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  flash(base + kUp * (0.8f * k), side * 1.6f * k, 0.9f * k, {1.0f, 0.88f, 0.65f}, age, 0.14f, eye, kPull);
  // --- Dust rolling out with it over every hex, stones thrown forward. ---
  for (size_t h = 0; h < e.hexes.size(); ++h) {
    Effect at = e;
    at.feet = e.hexes[h];
    dustRing(at, e.hexes[h], e.dir * 1.1f, 6, 1.2f * k, 0.32f * k, age - 0.04f * static_cast<float>(h), 1.1f, 0.45f, eye, 200u + 17u * static_cast<uint32_t>(h));
  }
  debris(e, e.feet + kUp * 0.1f, e.dir * 1.1f + kUp * 0.15f, 12, 2.3f * k, 0.05f * k, kStone, false, age, eye, 300u);
  // --- A flash on every body the wall meets. ---
  for (const Target& b : e.targets) {
    const float h = bodyHeight(b.height), kb = scaleOf(h);
    const float reach = glm::dot(b.feet - base, e.dir) / (1.6f * kHex);
    const float tau = age - std::clamp(reach, 0.0f, 1.0f) * 0.2f;
    flash(b.feet + kUp * (h * 0.5f) - e.dir * 0.2f, side * 0.9f * kb, 0.55f * kb, {1.0f, 0.85f, 0.6f}, tau, 0.14f, eye, kPull);
  }
  if (age < 0.35f) lights.push_back({base + e.dir * kHex + kUp, {1.0f, 0.8f, 0.55f}, 16.0f * (1.0f - ease(0.0f, 0.35f, age)), 4.0f * k});
}

// --- Debandada ---------------------------------------------------------------------------------------------------------------

void ArmsFx::buildStampede(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  constexpr float kPerHex = 0.09f;  // the hoofbeats' pace down the line
  const float age = now - e.impact;
  // --- On every hex in turn: a ground ring, a burst of dust and stones, a thud of light. ---
  for (size_t h = 0; h < e.hexes.size(); ++h) {
    const float tau = age - kPerHex * static_cast<float>(h);
    if (tau < 0.0f) continue;
    Effect at = e;
    at.feet = e.hexes[h];
    at.seed = e.seed + 131u * static_cast<uint32_t>(h);
    groundRing(e.hexes[h], 0.2f * k, 1.1f * k, 0.28f, 0.5f, 0.35f, 0.65f, {0.95f, 0.72f, 0.45f}, {1.0f, 0.86f, 0.66f}, tau, eye, frame, e.hexes[h]);
    dustRing(at, e.hexes[h], e.dir * 0.8f, 7, 1.0f * k, 0.36f * k, tau, 1.1f, 0.5f, eye, 200u);
    debris(at, e.hexes[h] + kUp * 0.08f, e.dir * 0.7f + kUp * 0.3f, 4, 1.9f * k, 0.05f * k, kStone, false, tau, eye, 300u);
    if (tau < 0.12f) streaks_.push_back(streak(pulled(e.hexes[h] + kUp * 0.25f, eye, kGroundPull), side * 0.9f * k, 0.05f * k, 1.0f - tau / 0.12f, {1.0f, 0.85f, 0.6f}, 2.2f));
  }
  // --- The dust wall rolling down the line ahead of the charge. ---
  const float span = kPerHex * static_cast<float>(e.hexes.size());
  for (int i = 0; i < 16; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 500u;
    const float born = (static_cast<float>(i) / 15.0f) * span;
    const float tau = age - born;
    const float life = 0.8f * (0.8f + 0.4f * hash01(e.seed, n, 1));
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float s = std::clamp(born / std::max(span, 1e-3f), 0.0f, 1.0f);
    const size_t at = std::min(e.hexes.size() - 1, static_cast<size_t>(s * static_cast<float>(e.hexes.size())));
    const glm::vec3 p = e.hexes[at] + side * (hash01(e.seed, n, 2) - 0.5f) * 1.6f * k + kUp * ((0.3f + 0.7f * easeOut(0.0f, 1.0f, u)) * k) + e.dir * (0.4f * u);
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.5f + 0.8f * easeOut(0.0f, 1.0f, u)) * k, hash01(e.seed, n, 3) * kTau + tau, hash01(e.seed, n, 4) * 37.0f,
                          0.5f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.3f, 1.0f, u)), kDust * (0.85f + 0.3f * hash01(e.seed, n, 5))));
  }
  // --- A flash on every body as the charge reaches it; the light runs with the charge. ---
  for (const Target& b : e.targets) {
    const float h = bodyHeight(b.height), kb = scaleOf(h);
    float hex = 0.0f, best = 1e9f;
    for (size_t j = 0; j < e.hexes.size(); ++j)
      if (const float d = glm::length(e.hexes[j] - b.feet); d < best) best = d, hex = static_cast<float>(j);
    flash(b.feet + kUp * (h * 0.5f) - e.dir * 0.2f, side * 1.0f * kb, 0.6f * kb, {1.0f, 0.85f, 0.6f}, age - kPerHex * hex, 0.15f, eye, kPull);
  }
  if (age >= 0.0f && age < span + 0.3f) {
    const float s = std::clamp(age / std::max(span, 1e-3f), 0.0f, 1.0f);
    const size_t at = std::min(e.hexes.size() - 1, static_cast<size_t>(s * static_cast<float>(e.hexes.size())));
    lights.push_back({e.hexes[at] + kUp * 0.6f, {1.0f, 0.78f, 0.5f}, 12.0f * (1.0f - ease(span, span + 0.3f, age)), 3.2f * k});
  }
}

// --- Ira Divina --------------------------------------------------------------------------------------------------------------

void ArmsFx::buildDivineWrath(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  constexpr float kPerHex = 0.06f;
  const float span = kPerHex * static_cast<float>(e.hexes.size());
  const float run = now - e.start;  // the beam leaves at start, reaches the first hex at impact
  // --- The beam races down the line low over the ground (a tube drawn in from the paladin), then fades. ---
  {
    const float total = (e.impact - e.start) + span;
    const float head = std::clamp(run / std::max(total, 1e-3f), 0.0f, 1.0f);
    const float opacity = 1.0f - ease(total + 0.1f, total + 0.55f, run);
    if (head > 0.02f && opacity > 0.0f) {
      const glm::vec3 a = e.from + e.dir * 0.4f + kUp * (0.55f * k), b = e.hexes.back() + kUp * (0.45f * k);
      three::CatmullRom path;
      for (int j = 0; j <= 12; ++j) path.points.push_back(pulled(glm::mix(a, b, head * static_cast<float>(j) / 12.0f), eye, kPull * 0.6f));
      const std::vector<VfxVertex> tube = vfxgeo::tube(path, 30, 0.09f * k, 8);
      draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, run, 1.8f, 0.9f * opacity, kGold, 0.0f, glm::vec4(kGoldCore, 0.0f),
           e.feet, VfxBlend::Additive);
      lights.push_back({glm::mix(a, b, head), {1.0f, 0.88f, 0.6f}, 10.0f * opacity, 3.2f * k});
    }
  }
  const float age = now - e.impact;
  // --- A column of white-gold light bursts on every hex as the beam passes, motes rising off it. ---
  for (size_t h = 0; h < e.hexes.size(); ++h) {
    const float tau = age - kPerHex * static_cast<float>(h);
    if (tau < 0.0f || tau > 0.9f) continue;
    const float env = ease(0.0f, 0.08f, tau) * (1.0f - ease(0.3f, 0.9f, tau));
    const float rise = easeOut(0.0f, 0.2f, tau);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.hexes[h], eye, kGroundPull)) *
                        glm::scale(glm::mat4(1.0f), {0.4f * k, 2.6f * k * rise + 1e-3f, 0.4f * k});
    draw(columnFirst_, columnCount_, m, kModeEnergy, tau, 0.8f, 0.6f * env, kGold, 0.0f, glm::vec4(kGoldCore, 0.25f), e.hexes[h], VfxBlend::Additive);
    groundRing(e.hexes[h], 0.2f * k, 0.8f * k, 0.25f, 0.6f, 0.5f, 0.7f, kGold, kGoldCore, tau, eye, frame, e.hexes[h]);
    const uint32_t seed = e.seed + 977u * static_cast<uint32_t>(h);
    for (int j = 0; j < 6; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 600u;
      const float phase = std::fmod(hash01(seed, n, 1) + tau * 1.3f, 1.0f);
      const float ang = hash01(seed, n, 2) * kTau;
      const glm::vec3 p = e.hexes[h] + glm::vec3(std::cos(ang) * 0.3f * k, phase * 2.4f * k, std::sin(ang) * 0.3f * k);
      streaks_.push_back(streak(pulled(p, eye, kPull * 0.6f), kUp * 0.08f * k, 0.04f * k, std::sin(kPi * phase) * env, kGoldCore, 2.2f));
    }
  }
  // --- A flash on every foe struck. ---
  for (const Target& b : e.targets) {
    const float h = bodyHeight(b.height), kb = scaleOf(h);
    float hex = 0.0f, best = 1e9f;
    for (size_t j = 0; j < e.hexes.size(); ++j)
      if (const float d = glm::length(e.hexes[j] - b.feet); d < best) best = d, hex = static_cast<float>(j);
    flash(b.feet + kUp * (h * 0.5f), side * 0.9f * kb, 0.6f * kb, kGoldCore, age - kPerHex * hex, 0.16f, eye, kPull);
  }
}

// --- Aura de Proteção / Presença Intimidante ---------------------------------------------------------------------------------

void ArmsFx::buildAura(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const float age = now - e.impact;
  const glm::vec3 energy = e.protection ? kGold : glm::vec3(0.55f, 0.05f, 0.08f);
  const glm::vec3 core = e.protection ? kGoldCore : glm::vec3(0.9f, 0.3f, 0.25f);
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.55f);
  // --- The call: light gathers at the chest just before. ---
  if (now >= e.start && age < 0.15f) streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), (0.3f + 0.5f * ease(e.start, e.impact, now)) * k, 0.8f, core, 2.0f));
  if (age < 0.0f) return;
  constexpr float kSpread = 0.55f;
  if (e.protection) {
    // --- A gold dome rises over the area and settles: a shell that grows to the radius, then holds faint and goes out. ---
    const float r = 0.4f * k + (e.radius - 0.4f * k) * easeOut(0.0f, kSpread, age);
    const float env = (1.0f - ease(0.9f, 1.8f, age));
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), e.feet) * glm::scale(glm::mat4(1.0f), {r, 0.55f * r, r});
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.6f, 0.5f * env, energy, 0.0f, glm::vec4(core, 0.0f), e.feet, VfxBlend::Additive);
  } else {
    // --- A dark crimson wave rolls out over the area: a darkening shell (alpha), thinning as it goes. ---
    if (age < kSpread + 0.4f) {
      const float r = 0.3f * k + (e.radius - 0.3f * k) * easeOut(0.0f, kSpread, age);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), e.feet + kUp * 0.3f) * glm::scale(glm::mat4(1.0f), {r, 0.5f * r, r});
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.0f, 0.65f * (1.0f - ease(0.2f, kSpread + 0.4f, age)), {0.12f, 0.02f, 0.04f}, 0.1f,
           glm::vec4(0.4f, 0.05f, 0.08f, 0.3f), e.feet, VfxBlend::Alpha);
    }
    flash(chest, glm::vec3(0.0f), 0.7f * k, core, age, 0.2f, eye, kPull);
  }
  // --- The ring on the ground at the area's edge: it grows out, then glows there a while. ---
  groundRing(e.feet, 0.4f * k, e.radius, kSpread, 1.9f, 0.5f, 0.75f, energy, core, age, eye, frame, e.feet);
  // --- Motes rising inside the area (gold sparks, or dark red embers). ---
  for (int i = 0; i < 26; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 700u;
    const float born = hash01(e.seed, n, 1) * 1.0f;
    const float tau = age - born;
    const float life = 0.8f + 0.5f * hash01(e.seed, n, 2);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float ang = hash01(e.seed, n, 3) * kTau, rr = std::sqrt(hash01(e.seed, n, 4)) * e.radius * 0.9f;
    const glm::vec3 p = e.feet + glm::vec3(std::cos(ang) * rr, (0.1f + 1.6f * u) * k, std::sin(ang) * rr);
    streaks_.push_back(streak(pulled(p, eye, kPull * 0.6f), kUp * 0.1f * k, 0.035f * k, std::sin(kPi * u) * 0.85f, e.protection ? kGoldCore : core, 2.0f));
  }
  // --- Every body under it: allies get a column of gold light; the cowed foes a dark pulse. ---
  int lit = 0;
  for (const Target& b : e.targets) {
    const float h = bodyHeight(b.height), kb = scaleOf(h);
    const float dist = glm::length(glm::vec2(b.feet.x - e.feet.x, b.feet.z - e.feet.z));
    const float tau = age - std::min(dist / e.radius, 1.0f) * kSpread;
    if (tau < 0.0f) continue;
    if (e.protection) {
      const float env = ease(0.0f, 0.18f, tau) * (1.0f - ease(0.6f, 1.2f, tau));
      if (env <= 0.0f) continue;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(b.feet, eye, kPull * 0.6f)) *
                          glm::scale(glm::mat4(1.0f), {0.32f * kb, h * 1.1f * easeOut(0.0f, 0.3f, tau) + 1e-3f, 0.32f * kb});
      draw(columnFirst_, columnCount_, m, kModeEnergy, tau, 0.5f, 0.55f * env, kGold, 0.0f, glm::vec4(kGoldCore, 0.2f), b.feet, VfxBlend::Additive);
      if (lit < 4) ++lit, lights.push_back({b.feet + kUp * (h * 0.5f), {1.0f, 0.85f, 0.5f}, 5.0f * env, 2.4f * kb});
    } else {
      if (tau > 0.6f) continue;
      const float r = (0.2f + 0.75f * easeOut(0.0f, 0.4f, tau)) * kb;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(b.feet + kUp * (h * 0.5f), eye, kPull)) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, tau, 1.0f, 0.75f * (1.0f - ease(0.15f, 0.6f, tau)), {0.12f, 0.03f, 0.06f}, 0.1f,
           glm::vec4(0.35f, 0.06f, 0.1f, 0.3f), b.feet, VfxBlend::Alpha);
    }
  }
  if (age < 0.8f) lights.push_back({chest, e.protection ? glm::vec3(1.0f, 0.82f, 0.45f) : glm::vec3(0.9f, 0.15f, 0.12f), 12.0f * (1.0f - ease(0.0f, 0.8f, age)), 4.0f * k});
}

// --- The summons -------------------------------------------------------------------------------------------------------------

void ArmsFx::buildSummon(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const SummonPalette pal = summonPalette(e.style);
  const float t = now - e.start, lead = e.impact - e.start;
  const float age = now - e.impact;
  const float big = e.style == Summon::Titan ? 1.3f : e.style == Summon::Familiar ? 0.9f : 1.0f;
  // --- The circle: a ring on the ground and a rune turning over it, lit while the familiar is called, gone after. ---
  const float env = ease(0.0f, 0.25f, t) * (1.0f - ease(lead + 0.2f, lead + 0.8f, t));
  if (env > 0.0f) {
    const float r = (0.95f + 0.05f * std::sin(t * 9.0f)) * big;
    const glm::mat4 ring = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, ring, kModeRing, t, 0.7f, 0.85f * env, pal.energy, 0.0f, glm::vec4(pal.core, 0.86f), e.feet, VfxBlend::Additive);
    const glm::mat4 rune = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.12f, 0.0f), eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), t * 1.8f, kUp) *
                           glm::scale(glm::mat4(1.0f), glm::vec3(0.7f * big));
    draw(runeFirst_, runeCount_, rune, kModeTrail, t, 1.6f, 0.9f * env, pal.energy, 0.0f, glm::vec4(pal.core, 0.0f), e.feet, VfxBlend::Additive);
    lights.push_back({e.feet + kUp * 0.4f, pal.light, 5.0f * env, 2.4f * big});
  }
  // --- Motes drawn in from round the circle, spiralling to its middle. ---
  for (int i = 0; i < 24; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 800u;
    const float born = hash01(e.seed, n, 1) * (lead * 0.8f);
    const float tau = t - born;
    const float life = 0.5f;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float ang = hash01(e.seed, n, 2) * kTau + u * 2.2f;
    const float rr = (1.6f - 1.4f * easeOut(0.0f, 1.0f, u)) * big;
    const glm::vec3 p = e.feet + glm::vec3(std::cos(ang) * rr, (0.15f + 0.8f * hash01(e.seed, n, 3)) * (1.0f - u) + 0.3f * u, std::sin(ang) * rr);
    streaks_.push_back(streak(pulled(p, eye, kPull * 0.6f), glm::vec3(0.0f), 0.045f * big, std::sin(kPi * u), pal.core, 2.2f));
  }
  if (age < 0.0f) return;
  // --- The familiar comes: a column of light, a flash, a shell bursting out. ---
  if (age < 0.6f) {
    const float rise = easeOut(0.0f, 0.18f, age), out = 1.0f - ease(0.2f, 0.6f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet, eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {0.45f * big, 2.4f * big * rise + 1e-3f, 0.45f * big});
    draw(columnFirst_, columnCount_, m, kModeEnergy, age, 0.9f, 0.7f * out, pal.energy, 0.0f, glm::vec4(pal.core, 0.3f), e.feet, VfxBlend::Additive);
  }
  flash(e.feet + kUp * (0.8f * big), glm::vec3(0.0f), 1.0f * big, pal.core, age, 0.2f, eye, kPull);
  if (age < 0.35f) {
    const float r = (0.3f + 1.1f * easeOut(0.0f, 0.3f, age)) * big;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), e.feet + kUp * (0.7f * big)) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.6f, 0.5f * (1.0f - ease(0.05f, 0.35f, age)), pal.energy, 0.0f, glm::vec4(pal.core, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  if (age < 0.5f) lights.push_back({e.feet + kUp * 0.8f, pal.light, 14.0f * (1.0f - ease(0.0f, 0.5f, age)), 3.2f * big});
}

// --- Reflexo Arcano (the user's icon: golden rune rings in violet flame) -------------------------------------------------------

namespace {
constexpr glm::vec3 kRuneGold{1.0f, 0.74f, 0.26f}, kRuneCore{1.0f, 0.95f, 0.72f}, kFlameViolet{0.58f, 0.3f, 1.0f}, kFlameCore{0.85f, 0.7f, 1.0f};
}  // namespace

void ArmsFx::buildReflectWard(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  const float t = now - e.start, age = now - e.impact;
  const glm::vec3 heart = e.feet + kUp * (e.height * 0.55f);
  const glm::vec3 at = pulled(heart, eye, kPull * 0.6f);
  const glm::vec3 toEye = flatDir(eye - e.feet, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 side = glm::normalize(glm::cross(kUp, toEye));
  // in over the lead, held, then dissipating from 0.75 s after the impact
  const float form = easeOut(0.0f, 0.45f, t), gone = ease(0.75f, 1.6f, age);
  const float env = form * (1.0f - gone);
  if (env > 0.0f) {
    // --- Two golden rune rings crossing like an eight (tilted either way about the axis toward the eye), turning against each other. ---
    const float r = e.height * 0.42f * (0.6f + 0.4f * form) * (1.0f + 0.25f * gone);
    for (int ring = 0; ring < 2; ++ring) {
      const float tilt = glm::radians(ring == 0 ? 62.0f : -62.0f);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), at + kUp * ((ring == 0 ? 0.22f : -0.22f) * e.height)) * glm::rotate(glm::mat4(1.0f), tilt, toEye) *
                          glm::rotate(glm::mat4(1.0f), (ring == 0 ? 1.0f : -1.0f) * t * 2.4f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(runeFirst_, runeCount_, m, kModeTrail, t, 2.2f, 0.95f * env, kRuneGold, 0.0f, glm::vec4(kRuneCore, 0.0f), e.feet, VfxBlend::Additive);
    }
    // --- The violet shell holding them, its rim only (a wave in the air), swelling and thinning as it dissipates. ---
    const float rs = e.height * 0.62f * (0.7f + 0.3f * form) * (1.0f + 0.4f * gone);
    const glm::mat4 shell = glm::translate(glm::mat4(1.0f), heart) * glm::scale(glm::mat4(1.0f), {rs * 0.8f, rs, rs * 0.8f});
    draw(sphereFirst_, sphereCount_, shell, kModeEnergy, t, 0.8f, 0.55f * env, kFlameViolet, 0.0f, glm::vec4(kFlameCore, 0.0f), e.feet, VfxBlend::Additive);
    lights.push_back({heart, {1.0f, 0.78f, 0.5f}, 8.0f * env, 3.0f * k});
  }
  // --- Violet flames licking up round him while it stands. ---
  for (int i = 0; i < 34; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 900u;
    const float born = hash01(e.seed, n, 1) * 1.2f;
    const float tau = t - born, life = 0.45f + 0.3f * hash01(e.seed, n, 2);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float ang = hash01(e.seed, n, 3) * kTau + u * 1.4f;
    const float rr = (0.35f + 0.25f * hash01(e.seed, n, 4)) * e.height * 0.5f;
    const glm::vec3 p = e.feet + glm::vec3(std::cos(ang) * rr, (0.1f + 1.0f * u) * e.height * 0.9f, std::sin(ang) * rr);
    const float alpha = std::sin(kPi * u) * (1.0f - gone);
    streaks_.push_back(streak(pulled(p, eye, kPull * 0.6f), kUp * (0.18f * k), (0.06f + 0.05f * (1.0f - u)) * k, alpha, glm::mix(kFlameCore, kFlameViolet, u), 2.2f));
  }
  // --- The golden star at his heart as the ward closes (a flash and its cross glint), a gold light. ---
  flash(heart, side * 0.9f * k, 0.8f * k, kRuneCore, age, 0.3f, eye, kPull);
  if (age >= 0.0f && age < 0.3f) {
    const float f = 1.0f - ease(0.0f, 0.3f, age);
    streaks_.push_back(streak(at, kUp * 0.9f * k * f, 0.04f * k, f, kRuneCore, 2.6f));
    lights.push_back({heart, {1.0f, 0.82f, 0.55f}, 10.0f * f, 3.2f * k});
  }
  // --- Golden motes flying off as it dissipates. ---
  for (int i = 0; i < 26; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 1000u;
    const float tau = age - 0.75f - hash01(e.seed, n, 1) * 0.3f, life = 0.6f + 0.3f * hash01(e.seed, n, 2);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 d = sphereDir(hash01(e.seed, n, 3), hash01(e.seed, n, 4));
    const glm::vec3 p = heart + d * (e.height * 0.55f + 1.2f * easeOut(0.0f, 1.0f, u) * k) + kUp * (0.3f * u);
    streaks_.push_back(streak(pulled(p, eye, kPull * 0.6f), glm::vec3(0.0f), 0.045f * k, (1.0f - u) * 0.9f, kRuneCore, 2.2f));
  }
}

void ArmsFx::buildReflectHit(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height), kf = scaleOf(e.radius);
  const glm::vec3 a = e.from + kUp * (e.radius * 0.55f), b = e.feet + kUp * (e.height * 0.5f);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const float t = now - e.start, flight = e.impact - e.start;
  // --- On the warded one: a golden rune ring flashes as the spell turns back. ---
  if (t < 0.4f) {
    const float f = 1.0f - ease(0.0f, 0.4f, t);
    const float r = e.radius * (0.3f + 0.25f * easeOut(0.0f, 0.3f, t));
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(a, eye, kPull)) * glm::rotate(glm::mat4(1.0f), glm::radians(80.0f), side) *
                        glm::rotate(glm::mat4(1.0f), t * 4.0f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(runeFirst_, runeCount_, m, kModeTrail, t, 2.4f, 0.95f * f, kRuneGold, 0.0f, glm::vec4(kRuneCore, 0.0f), e.from, VfxBlend::Additive);
    flash(a, glm::vec3(0.0f), 0.6f * kf, kRuneCore, t, 0.2f, eye, kPull);
  }
  // --- The streak back to its caster: a golden core in a violet sheath, drawn in as it flies. ---
  if (now < e.impact + 0.1f) {
    const float head = std::clamp(t / std::max(flight, 1e-3f), 0.0f, 1.0f), tail = std::clamp((t - 0.15f) / std::max(flight, 1e-3f), 0.0f, 1.0f);
    if (head > tail + 0.02f) {
      const float lift = 0.12f * glm::length(b - a);
      three::CatmullRom path;
      for (int j = 0; j <= 10; ++j) {
        const float s = tail + (head - tail) * static_cast<float>(j) / 10.0f;
        path.points.push_back(pulled(glm::mix(a, b, s) + kUp * (lift * 4.0f * s * (1.0f - s)), eye, kPull * 0.7f));
      }
      const std::vector<VfxVertex> sheath = vfxgeo::tube(path, 18, 0.09f * k, 8), core = vfxgeo::tube(path, 18, 0.035f * k, 6);
      draw(frame.addVertices(sheath), static_cast<uint32_t>(sheath.size()), glm::mat4(1.0f), kModeTrail, t, 1.4f, 0.75f, kFlameViolet, 0.0f, glm::vec4(kFlameCore, 0.0f),
           e.feet, VfxBlend::Additive);
      draw(frame.addVertices(core), static_cast<uint32_t>(core.size()), glm::mat4(1.0f), kModeTrail, t, 2.4f, 0.95f, kRuneGold, 0.0f, glm::vec4(kRuneCore, 0.0f), e.feet,
           VfxBlend::Additive);
      const glm::vec3 tip = glm::mix(a, b, head) + kUp * (lift * 4.0f * head * (1.0f - head));
      streaks_.push_back(streak(pulled(tip, eye, kPull * 0.7f), glm::vec3(0.0f), 0.28f * k, 0.9f, kRuneCore, 2.4f));
      lights.push_back({tip, {1.0f, 0.82f, 0.55f}, 8.0f, 2.6f * k});
    }
  }
  const float age = now - e.impact;
  if (age < 0.0f) return;
  // --- The arcane blow on the caster: a golden star with its glint, a violet shell, a golden ring breaking out, sparks, a gold light. ---
  flash(b, side * 1.1f * k, 0.8f * k, kRuneCore, age, 0.16f, eye, kPull);
  if (age < 0.4f) {
    const float r = (0.2f + 0.9f * easeOut(0.0f, 0.32f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), b) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.0f, 0.7f * (1.0f - ease(0.06f, 0.4f, age)), kFlameViolet, 0.12f, glm::vec4(kFlameCore, 0.3f), e.feet,
         VfxBlend::Additive);
    const float rr = (0.3f + 0.8f * easeOut(0.0f, 0.35f, age)) * k;
    const glm::mat4 ring = glm::translate(glm::mat4(1.0f), pulled(b, eye, kPull)) * glm::rotate(glm::mat4(1.0f), glm::radians(80.0f), side) *
                           glm::rotate(glm::mat4(1.0f), age * 3.0f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rr));
    draw(runeFirst_, runeCount_, ring, kModeTrail, age, 2.2f, 0.9f * (1.0f - ease(0.1f, 0.4f, age)), kRuneGold, 0.0f, glm::vec4(kRuneCore, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  sparks(e, b, -e.dir * 0.2f, 18, 3.2f * k, 0.03f * k, kRuneCore, kFlameViolet, age, eye, 1100u);
  groundRing(e.feet, 0.3f * k, 1.0f * k, 0.35f, 0.6f, 0.5f, 0.65f, kFlameViolet, kRuneCore, age, eye, frame, e.feet);
  if (age < 0.45f) lights.push_back({b, {1.0f, 0.75f, 0.5f}, 14.0f * (1.0f - ease(0.0f, 0.45f, age)), 3.0f * k});
}

// --- Força Fantasma: a lesser Força Fantasmagórica, red and magenta ---------------------------------------------------------

void ArmsFx::buildPhantom(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  constexpr glm::vec3 kMagenta{1.0f, 0.14f, 0.55f}, kRose{1.0f, 0.7f, 0.85f}, kCrimsonDeep{0.8f, 0.05f, 0.18f};
  const glm::vec3 heart = e.feet + kUp * (e.height * 0.5f);
  const glm::vec3 at = pulled(heart, eye, kPull);
  const float t = now - e.start, lead = e.impact - e.start, age = now - e.impact;
  const float collapse = ease(lead * 0.6f, lead, t);  // drawn in to the core over the last part of the gathering
  const float radius = 1.05f * k * (1.0f - 0.9f * collapse);
  // --- Five thin tendrils curling in from round the foe to its heart (tubes drawn in from their tips), fading as it bursts. ---
  const float tendrilAlpha = std::min(1.0f, t / 0.12f) * (1.0f - ease(0.0f, 0.2f, age));
  if (tendrilAlpha > 0.01f)
    for (int i = 0; i < 5; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 1200u;
      const glm::vec3 d = sphereDir(0.25f + 0.5f * hash01(e.seed, n, 1), hash01(e.seed, n, 2));
      const glm::vec3 sideways = glm::normalize(glm::cross(d, kUp) + glm::vec3(1e-4f));
      three::CatmullRom path;
      for (int j = 0; j <= 6; ++j) {
        const float s = static_cast<float>(j) / 6.0f;  // 0 = the tip far out, 1 = the heart
        const float r = radius * (1.0f - s);
        const float swirl = (t * 3.0f + s * 2.4f + hash01(e.seed, n, 3) * kTau);
        path.points.push_back(pulled(heart + d * r + sideways * (std::sin(swirl) * 0.22f * k * (1.0f - s)), eye, kPull));
      }
      const std::vector<VfxVertex> tube = vfxgeo::tube(path, 14, 0.025f * k, 6);
      draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, t, 1.5f, 0.75f * tendrilAlpha, kMagenta, 0.0f,
           glm::vec4(kRose, 0.0f), e.feet, VfxBlend::Additive);
    }
  // --- The motes spiralling in (and thrown out at the burst). ---
  for (int i = 0; i < 60; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 1300u;
    const glm::vec3 d = sphereDir(hash01(e.seed, n, 1), hash01(e.seed, n, 2));
    const float phase = hash01(e.seed, n, 3) * kTau + t * (2.0f + 1.5f * hash01(e.seed, n, 4));
    const glm::vec3 tangent = glm::normalize(glm::cross(d, kUp) + glm::vec3(1e-4f));
    glm::vec3 p = heart + d * (radius * (0.4f + 0.6f * hash01(e.seed, n, 5))) + tangent * (std::sin(phase) * radius * 0.25f);
    float alpha = std::min(1.0f, t / 0.15f);
    if (age >= 0.0f) {
      p = heart + d * (0.2f + 2.6f * age) * k + kUp * (0.6f * age * age);
      alpha = 1.0f - ease(0.0f, 0.4f, age);
    }
    if (alpha <= 0.01f) continue;
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), (0.03f + 0.03f * collapse) * k, alpha, glm::mix(kMagenta, kRose, collapse), 2.2f));
  }
  // --- The core churning brighter as it is drawn in. ---
  if (t >= 0.08f && age < 0.12f) {
    const float r = (0.06f + 0.16f * collapse) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), at) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, t, 1.6f, 0.9f, kCrimsonDeep, 0.8f, glm::vec4(kRose, 0.8f), e.feet, VfxBlend::Additive);
  }
  // --- The burst: a flash, a magenta shell out to 1.3 units, the red light spiking and fading. ---
  flash(heart, glm::vec3(0.0f), 0.7f * k, kRose, age, 0.14f, eye, kPull);
  if (age >= 0.0f && age < 0.3f) {
    const float r = (0.15f + 1.15f * easeOut(0.0f, 0.2f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), heart) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.9f, 0.6f * (1.0f - ease(0.05f, 0.3f, age)), kMagenta, 0.0f, glm::vec4(kRose, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  const float build = std::min(1.0f, t / 0.4f) * 2.0f, spike = age >= 0.0f ? 14.0f * (1.0f - ease(0.0f, 0.35f, age)) : 0.0f;
  if (age < 0.35f) lights.push_back({heart, {1.0f, 0.25f, 0.5f}, build + spike, 3.0f * k});
}

// --- Raio Espectral: teal-green, spectral --------------------------------------------------------------------------------------

void ArmsFx::buildSpectral(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = scaleOf(e.height);
  constexpr glm::vec3 kTeal{0.2f, 0.95f, 0.75f}, kPale{0.82f, 1.0f, 0.92f}, kDeep{0.05f, 0.45f, 0.4f};
  const glm::vec3 a = e.from + kUp * (e.height * 0.6f) + e.dir * 0.4f, b = e.feet + kUp * 0.9f;
  const float flight = e.impact - e.start, t = now - e.start, age = now - e.impact;
  const float s = std::clamp(t / std::max(flight, 1e-3f), 0.0f, 1.0f);
  const float lift = 0.15f * glm::length(b - a);
  auto path = [&](float u) { return glm::mix(a, b, u) + kUp * (lift * 4.0f * u * (1.0f - u)); };
  // --- The flight: a ghostly orb (a shell with a pale heart) and a wailing wake of pale green wisps behind it. ---
  if (now < e.impact) {
    const glm::vec3 head = path(s);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(head, eye, kPull * 0.7f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.22f * k));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, t, 1.6f, 0.95f, kTeal, 0.6f, glm::vec4(kPale, 0.6f), e.feet, VfxBlend::Additive);
    const float tail = std::min(s, 0.45f);
    if (tail > 0.02f)
      for (int w = 0; w < 3; ++w) {  // three wisps twisting round each other behind the orb
        three::CatmullRom wake;
        for (int j = 0; j <= 8; ++j) {
          const float u = s - tail + tail * static_cast<float>(j) / 8.0f;
          const float twist = u * 18.0f + static_cast<float>(w) * kTau / 3.0f - t * 8.0f;
          const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
          const float r = 0.12f * k * (static_cast<float>(j) / 8.0f);
          wake.points.push_back(pulled(path(u) + (side * std::cos(twist) + kUp * std::sin(twist)) * r, eye, kPull * 0.7f));
        }
        const std::vector<VfxVertex> tube = vfxgeo::tube(wake, 14, 0.03f * k, 6);
        draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, t, 1.4f, 0.7f, kTeal, 0.0f, glm::vec4(kPale, 0.0f), e.feet,
             VfxBlend::Additive);
      }
    lights.push_back({head, {0.4f, 1.0f, 0.8f}, 7.0f, 2.6f * k});
  }
  for (int i = 0; i < 14; ++i) {  // motes shed along the flight
    const uint32_t n = static_cast<uint32_t>(i) + 1400u;
    const float ub = (static_cast<float>(i) + 0.5f) / 14.0f, tb = e.start + ub * flight;
    const float tau = now - tb, life = 0.4f + 0.3f * hash01(e.seed, n, 1);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 p = path(ub) + sphereDir(hash01(e.seed, n, 2), hash01(e.seed, n, 3)) * (0.25f * tau) + kUp * (0.4f * tau);
    streaks_.push_back(streak(pulled(p, eye, kPull * 0.7f), glm::vec3(0.0f), 0.04f * k * (1.0f - 0.5f * u), (1.0f - u) * 0.8f, kPale, 2.0f));
  }
  if (age < 0.0f) return;
  // --- The burst over the 3 hexes: a flash, a teal shell, a ring on the ground, a cold green light. ---
  flash(b, glm::vec3(0.0f), 0.9f * k, kPale, age, 0.16f, eye, kPull);
  if (age < 0.45f) {
    const float r = (0.3f + 1.6f * easeOut(0.0f, 0.35f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), e.feet + kUp * 0.5f) * glm::scale(glm::mat4(1.0f), {r, 0.6f * r, r});
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.9f, 0.6f * (1.0f - ease(0.08f, 0.45f, age)), kTeal, 0.0f, glm::vec4(kPale, 0.0f), e.feet, VfxBlend::Additive);
  }
  groundRing(e.feet, 0.3f * k, 1.9f * k, 0.35f, 0.8f, 0.5f, 0.75f, kTeal, kPale, age, eye, frame, e.feet);
  // --- Spectral faces of mist rising off each hex and fading (pale alpha puffs, dark eyes as two dark puffs), wisps flung up. ---
  for (size_t h = 0; h < e.hexes.size(); ++h) {
    const uint32_t seed = e.seed + 613u * static_cast<uint32_t>(h);
    const float tau = age - 0.05f * static_cast<float>(h);
    const float life = 1.0f;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 face = e.hexes[h] + kUp * ((0.5f + 1.3f * easeOut(0.0f, 1.0f, u)) * k);
    const float alpha = 0.5f * ease(0.0f, 0.12f, tau) * (1.0f - ease(0.35f, 1.0f, u));
    puffs_.push_back(puff(pulled(face, eye, kPull * 0.8f), (0.45f + 0.25f * u) * k, hash01(seed, 1, 1) * kTau, hash01(seed, 1, 2) * 37.0f, alpha, {0.6f, 0.95f, 0.85f}));
    const glm::vec3 across = glm::normalize(glm::cross(kUp, flatDir(eye - e.hexes[h], glm::vec3(0, 0, 1))));
    for (int eyeSide = -1; eyeSide <= 1; eyeSide += 2)
      puffs_.push_back(puff(pulled(face + across * (0.11f * k * static_cast<float>(eyeSide)) + kUp * (0.06f * k), eye, kPull * 0.85f), 0.08f * k, 0.0f,
                            hash01(seed, 2, static_cast<uint32_t>(eyeSide + 2)) * 37.0f, alpha * 1.4f, {0.02f, 0.08f, 0.07f}));
  }
  for (int i = 0; i < 22; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 1500u;
    const float tau = age - hash01(e.seed, n, 1) * 0.1f, life = 0.5f + 0.4f * hash01(e.seed, n, 2);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 d = glm::normalize(sphereDir(hash01(e.seed, n, 3), hash01(e.seed, n, 4)) + kUp * 1.2f);
    const glm::vec3 p = e.feet + kUp * 0.4f + d * ((0.4f + 2.0f * easeOut(0.0f, 1.0f, u)) * k);
    streaks_.push_back(streak(pulled(p, eye, kPull), d * 0.12f * k, 0.035f * k, (1.0f - u) * 0.9f, glm::mix(kPale, kTeal, u), 2.2f));
  }
  // --- A pale flash on every body struck. ---
  for (const Target& body : e.targets) flash(body.feet + kUp * (bodyHeight(body.height) * 0.5f), glm::vec3(0.0f), 0.5f * scaleOf(body.height), kPale, age - 0.04f, 0.16f, eye, kPull);
  if (age < 0.6f) lights.push_back({e.feet + kUp * 0.8f, {0.35f, 1.0f, 0.75f}, 15.0f * (1.0f - ease(0.0f, 0.6f, age)), 4.0f * k});
}

}  // namespace ember
