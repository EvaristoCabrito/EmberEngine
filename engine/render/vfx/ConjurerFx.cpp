#include "render/vfx/ConjurerFx.h"

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
constexpr float kHex = 1.7320508f;
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f, kModeTrail = 4.0f;  // Combat material
constexpr float kHolyShaft = 0.0f, kHolyPool = 1.0f, kHolyMote = 2.0f, kHolyTube = 3.0f, kHolyLine = 4.0f;   // Holy material
constexpr float kSwapLead = 0.3f, kLanceLead = 0.3f, kMirrorLead = 0.5f, kDetonateLead = 0.6f, kBondLead = 0.5f, kNightmareLead = 0.6f, kEmpowerLead = 0.5f;
constexpr float kDetonateSpread = 0.3f, kNightmareSpread = 0.45f;

constexpr glm::vec3 kViolet{0.62f, 0.42f, 1.0f}, kCore{0.92f, 0.86f, 1.0f}, kIndigo{0.34f, 0.2f, 0.72f}, kBone{0.93f, 0.9f, 0.8f};
constexpr glm::vec3 kShadow{0.09f, 0.05f, 0.14f}, kAsh{0.2f, 0.16f, 0.24f}, kEye{1.0f, 0.55f, 0.75f};

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
// The bone lance along +z, length 1: a tapered shaft with a jagged point and a knobbed butt (a lit solid: the Standard material).
const std::vector<VfxVertex>& lanceGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 10, along = 14;
    auto width = [](float s) {
      const float point = std::clamp((1.0f - s) / 0.22f, 0.0f, 1.0f);  // the tip at s = 1
      const float butt = 1.0f + 0.5f * std::clamp((0.08f - s) / 0.08f, 0.0f, 1.0f);
      const float ridges = 1.0f + 0.12f * std::sin(s * 40.0f);
      return point * butt * ridges;
    };
    auto v = [&](int i, int j) {
      const float a = static_cast<float>(i) / around * kTau, s = static_cast<float>(j) / along;
      const float w = width(s);
      return VfxVertex{{std::cos(a) * w, std::sin(a) * w, s}, static_cast<float>(i) / around, glm::normalize(glm::vec3(std::cos(a), std::sin(a), 0.35f)), s};
    };
    std::vector<VfxVertex> out;
    for (int i = 0; i < around; ++i)
      for (int j = 0; j < along; ++j) out.insert(out.end(), {v(i, j), v(i + 1, j), v(i, j + 1), v(i + 1, j), v(i + 1, j + 1), v(i, j + 1)});
    return out;
  }();
  return g;
}
// A hooded phantom (height 1, feet at y 0): a body that narrows at the waist and rounds at the head, hollow (an energy shell).
const std::vector<VfxVertex>& phantomGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 24, up = 18;
    auto width = [](float y) {
      const float skirt = 0.42f * (1.0f - y) + 0.2f;                      // wide at the hem
      const float waist = 1.0f - 0.25f * std::exp(-std::pow((y - 0.55f) / 0.12f, 2.0f));
      const float head = y > 0.8f ? std::sqrt(std::max(0.0f, 1.0f - std::pow((y - 0.8f) / 0.2f, 2.0f))) : 1.0f;
      return skirt * waist * head;
    };
    auto v = [&](int i, int j) {
      const float a = static_cast<float>(i) / around * kTau, y = static_cast<float>(j) / up;
      const float w = width(y);
      return VfxVertex{{std::cos(a) * w, y, std::sin(a) * w * 0.55f}, static_cast<float>(i) / around, glm::normalize(glm::vec3(std::cos(a), 0.2f, std::sin(a))), y};
    };
    std::vector<VfxVertex> out;
    for (int i = 0; i < around; ++i)
      for (int j = 0; j < up; ++j) out.insert(out.end(), {v(i, j), v(i + 1, j), v(i, j + 1), v(i + 1, j), v(i + 1, j + 1), v(i, j + 1)});
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
// A lit solid (three MeshStandardMaterial): base colour, roughness, metalness, emissive.
void standardDraw(VfxFrame& frame, uint32_t first, uint32_t count, const glm::mat4& model, glm::vec3 color, float roughness, float metal, glm::vec3 emissive,
                  glm::vec3 sight) {
  VfxDraw d;
  d.gpu.model = model;
  d.gpu.params0 = {roughness, metal, 0.0f, 0.0f};
  d.gpu.params1 = glm::vec4(emissive, 1.0f);
  d.gpu.color = glm::vec4(color, 1.0f);
  d.gpu.firstVertex = first;
  d.gpu.vertexCount = count;
  d.gpu.firstInstance = 0xFFFFFFFFu;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::Standard);
  d.gpu.sightAt = sight;
  d.instances = 1;
  d.blend = VfxBlend::Alpha;
  d.depthTest = true;
  frame.draws.push_back(d);
}
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}
VfxInstance puff(glm::vec3 centre, float size, float rotation, float seed, float alpha, glm::vec3 albedo) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(rotation, seed, alpha, 0.0f), glm::vec4(albedo, 0.0f)};
}
VfxInstance mote(glm::vec3 centre, float size, float alpha, glm::vec3 tint) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(0.0f, 0.0f, alpha, 1.4f), glm::vec4(tint, 0.0f)};
}
glm::mat4 at(glm::vec3 p) { return glm::translate(glm::mat4(1.0f), p); }
glm::mat4 scaled(glm::vec3 p, glm::vec3 s) { return glm::translate(glm::mat4(1.0f), p) * glm::scale(glm::mat4(1.0f), s); }
// A frame whose +z points along `dir` (y up as much as it can).
glm::mat4 along(glm::vec3 p, glm::vec3 dir, float length, float thick) {
  const glm::vec3 z = glm::normalize(dir);
  glm::vec3 x = glm::cross(kUp, z);
  x = glm::dot(x, x) > 1e-6f ? glm::normalize(x) : glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 y = glm::cross(z, x);
  glm::mat4 m(1.0f);
  m[0] = glm::vec4(x * thick, 0.0f), m[1] = glm::vec4(y * thick, 0.0f), m[2] = glm::vec4(z * length, 0.0f), m[3] = glm::vec4(p, 1.0f);
  return m;
}

}  // namespace

float ConjurerFx::leadIn(const std::string& kind) {
  if (kind == "placeSwap") return kSwapLead;
  if (kind == "boneLance") return kLanceLead;
  if (kind == "mirrorImage") return kMirrorLead;
  if (kind == "detonateSummon") return kDetonateLead;
  if (kind == "sacrificeBond") return kBondLead;
  if (kind == "nightmare") return kNightmareLead;
  return kEmpowerLead;
}
float ConjurerFx::lanceSeconds(size_t hexes) { return std::clamp(0.08f * static_cast<float>(hexes) + 0.1f, 0.18f, 0.5f); }

void ConjurerFx::swap(const Target& a, const Target& b, float start, float finish) {
  Effect e;
  e.kind = Kind::Swap, e.from = a.feet, e.fromHeight = bodyHeight(a.height), e.centre = b.feet, e.height = bodyHeight(b.height);
  e.start = start, e.impact = start, e.finish = finish, e.seed = 0x5a7u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ConjurerFx::lance(glm::vec3 from, float fromHeight, const std::vector<glm::vec3>& hexes, std::vector<Target> bodies, float release, float impact) {
  Effect e;
  e.kind = Kind::Lance, e.from = from, e.fromHeight = bodyHeight(fromHeight), e.hexes = hexes, e.bodies = std::move(bodies);
  e.start = release, e.impact = impact, e.finish = std::max(impact, release + 0.05f), e.seed = 0xb0e1u + 7919u * ++serial_;
  if (!hexes.empty()) e.centre = hexes.back();
  effects_.push_back(std::move(e));
}
void ConjurerFx::mirrorBurst(const Target& ally, float impact) {
  Effect e;
  e.kind = Kind::MirrorBurst, e.centre = ally.feet, e.height = bodyHeight(ally.height), e.impact = impact, e.seed = 0x3113u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
int ConjurerFx::mirrorStart(glm::vec3 feet, float height, int copies, float now) {
  Effect e;
  e.kind = Kind::Mirror, e.centre = feet, e.height = bodyHeight(height), e.copies = copies, e.start = now, e.impact = now, e.id = static_cast<int>(++serial_);
  e.seed = 0x3114u + 7919u * serial_;
  effects_.push_back(std::move(e));
  return effects_.back().id;
}
void ConjurerFx::mirrorUpdate(int id, glm::vec3 feet, int copies, float now) {
  for (Effect& e : effects_) {
    if (e.kind != Kind::Mirror || e.id != id) continue;
    e.centre = feet;
    while (copies < e.copies) e.shatters.emplace_back(now, --e.copies);  // the last slot shatters first
    e.copies = std::max(copies, 0);
  }
}
void ConjurerFx::mirrorEnd(int id, float now) {
  for (Effect& e : effects_)
    if (e.kind == Kind::Mirror && e.id == id && e.endAt < 0.0f) e.endAt = now;
}
void ConjurerFx::detonate(const Target& summon, float radius, std::vector<Target> bodies, float impact) {
  Effect e;
  e.kind = Kind::Detonate, e.centre = summon.feet, e.height = bodyHeight(summon.height), e.radius = std::clamp(radius, 1.2f * kHex, 5.0f * kHex);
  e.bodies = std::move(bodies), e.impact = impact, e.seed = 0xde70u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ConjurerFx::bond(glm::vec3 from, float fromHeight, std::vector<Target> summons, float impact) {
  Effect e;
  e.kind = Kind::Bond, e.from = from, e.centre = from, e.fromHeight = bodyHeight(fromHeight), e.height = e.fromHeight, e.bodies = std::move(summons);
  e.impact = impact, e.seed = 0xb0d0u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ConjurerFx::nightmare(glm::vec3 centre, float radius, std::vector<Target> bodies, float impact) {
  Effect e;
  e.kind = Kind::Nightmare, e.centre = centre, e.radius = std::clamp(radius, 1.2f * kHex, 6.0f * kHex), e.bodies = std::move(bodies), e.impact = impact;
  e.seed = 0x1d8eu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void ConjurerFx::empower(glm::vec3 from, float fromHeight, std::vector<Target> summons, float impact) {
  Effect e;
  e.kind = Kind::Empower, e.from = from, e.centre = from, e.fromHeight = bodyHeight(fromHeight), e.height = e.fromHeight, e.bodies = std::move(summons);
  e.impact = impact, e.seed = 0xe3b0u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

float ConjurerFx::Effect::end() const {
  switch (kind) {
    case Kind::Swap: return finish + 0.6f;
    case Kind::Lance: return finish + 1.3f;
    case Kind::MirrorBurst: return impact + 1.2f;
    case Kind::Mirror: return endAt < 0.0f ? 1e30f : endAt + 0.7f;
    case Kind::Detonate: return impact + 2.0f;
    case Kind::Bond: return impact + 2.0f;
    case Kind::Nightmare: return impact + 2.4f;
    case Kind::Empower: return impact + 2.0f;
  }
  return impact;
}

bool ConjurerFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.kind != Kind::Mirror && e.end() > now; });
}
float ConjurerFx::latestEnd() const {
  float end = -1.0f;
  for (const Effect& e : effects_)
    if (e.kind != Kind::Mirror) end = std::max(end, e.end());
  return end;
}

glm::vec3 ConjurerFx::copyFeet(glm::vec3 feet, int slot, int /*copies*/, const Camera& camera, float t) {
  const glm::vec3 side = flatDir(glm::vec3(camera.right.x, 0.0f, camera.right.z), glm::vec3(1.0f, 0.0f, 0.0f));
  const glm::vec3 toward = flatDir(camera.position - feet, glm::vec3(0.0f, 0.0f, 1.0f));
  // the copies flank the unit across the view: left, right, then behind on each side
  static const float kSide[4] = {-1.0f, 1.0f, -1.7f, 1.7f};
  static const float kBack[4] = {0.0f, 0.0f, -0.6f, -0.6f};
  const int k = std::clamp(slot, 0, 3);
  const float sway = 0.06f * std::sin(t * 1.3f + static_cast<float>(slot) * 2.1f);
  return feet + side * (kSide[k] * 0.95f + sway) + toward * (kBack[k] * 0.8f);
}

void ConjurerFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  columnFirst_ = frame.addVertices(columnGeometry()), columnCount_ = static_cast<uint32_t>(columnGeometry().size());
  haloFirst_ = frame.addVertices(haloGeometry()), haloCount_ = static_cast<uint32_t>(haloGeometry().size());
  lanceFirst_ = frame.addVertices(lanceGeometry()), lanceCount_ = static_cast<uint32_t>(lanceGeometry().size());
  phantomFirst_ = frame.addVertices(phantomGeometry()), phantomCount_ = static_cast<uint32_t>(phantomGeometry().size());
  streaks_.clear(), puffs_.clear(), motes_.clear(), shards_.clear();
  for (const Effect& e : effects_) {
    switch (e.kind) {
      case Kind::Swap: if (now >= e.start) buildSwap(e, now, camera, frame, lights); break;
      case Kind::Lance: if (now >= e.start - kLanceLead) buildLance(e, now, camera, frame, lights); break;
      case Kind::MirrorBurst: if (now >= e.impact - kMirrorLead) buildMirrorBurst(e, now, camera, frame, lights); break;
      case Kind::Mirror: buildMirror(e, now, camera, frame, lights); break;
      case Kind::Detonate: if (now >= e.impact - kDetonateLead) buildDetonate(e, now, camera, frame, lights); break;
      case Kind::Bond: if (now >= e.impact - kBondLead) buildBond(e, now, camera, frame, lights); break;
      case Kind::Nightmare: if (now >= e.impact - kNightmareLead) buildNightmare(e, now, camera, frame, lights); break;
      case Kind::Empower: if (now >= e.impact - kEmpowerLead) buildEmpower(e, now, camera, frame, lights); break;
    }
  }
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
    HolyDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kHolyMote, now, 1.0f, 1.0f, kViolet, kCore, 0.0f, sight, first, static_cast<uint32_t>(motes_.size()));
  }
  if (!shards_.empty()) {  // bone shards: lit puffs, pale (the Combat puff is shaded by the scene)
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), shards_.begin(), shards_.end());
    CombatDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
                      static_cast<uint32_t>(shards_.size()));
  }
  if (!streaks_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    CombatDraw{frame}(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(kCore, 0.0f), sight,
                      VfxBlend::Additive, first, static_cast<uint32_t>(streaks_.size()));
  }
}

// --- Troca de Lugar -------------------------------------------------------------------------------------------------------

void ConjurerFx::buildSwap(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const glm::vec3 eye = camera.position;
  const float t = now - e.start, span = std::max(0.1f, e.finish - e.start);
  const float u = std::clamp(t / span, 0.0f, 1.0f);
  const glm::vec3 a = e.from, b = e.centre;
  const float ha = e.fromHeight, hb = e.height;
  // two spirals of motes, each from one hex to the other, crossing in the middle on an arc
  for (int i = 0; i < 44; ++i) {
    const uint32_t n = static_cast<uint32_t>(i);
    const bool first = (i & 1) == 0;
    const float lag = hash01(e.seed, n, 1) * 0.25f, s = std::clamp((t - lag * span) / (span * 0.85f), 0.0f, 1.0f);
    if (t < lag * span || s >= 1.0f) continue;
    const glm::vec3 p0 = first ? a : b, p1 = first ? b : a;
    const float h0 = first ? ha : hb, h1 = first ? hb : ha;
    const glm::vec3 mid = glm::mix(p0, p1, s) + kUp * (glm::mix(h0, h1, s) * 0.5f + 1.2f * std::sin(kPi * s));
    const float ang = s * 9.0f + hash01(e.seed, n, 2) * kTau, r = 0.28f + 0.15f * hash01(e.seed, n, 3);
    const glm::vec3 side = flatDir(glm::cross(p1 - p0, kUp), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 p = mid + side * (std::cos(ang) * r) + kUp * (std::sin(ang) * r);
    const float alpha = std::sin(kPi * s);
    if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.11f, alpha, first ? kViolet : kCore));
  }
  // a flash and a ring at each end as the cards leave and land
  for (const bool end : {false, true}) {
    const glm::vec3 feet = end ? b : a;
    for (const float when : {0.0f, span}) {
      const float tau = t - when;
      if (tau < 0.0f || tau > 0.45f) continue;
      const float r = 0.3f + 0.9f * easeOut(0.0f, 0.4f, tau);
      combat(discFirst_, discCount_, scaled(pulled(feet + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, tau, 0.5f, 0.75f * (1.0f - ease(0.1f, 0.45f, tau)),
             kViolet, 0.0f, glm::vec4(kCore, 0.9f), feet, VfxBlend::Additive);
      if (tau < 0.18f) streaks_.push_back(streak(pulled(feet + kUp * 0.8f, eye, kPull), glm::vec3(0.0f), 0.9f * (1.0f - tau), 1.0f - ease(0.0f, 0.18f, tau), kCore, 2.2f));
    }
  }
  if (u < 1.0f) lights.push_back({glm::mix(a, b, u) + kUp * 1.2f, {0.6f, 0.42f, 1.0f}, 7.0f * std::sin(kPi * u) + 1.0f, 3.5f});
}

// --- Lança de Osso --------------------------------------------------------------------------------------------------------

void ConjurerFx::buildLance(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  if (e.hexes.empty()) return;
  const glm::vec3 hand = e.from + kUp * (e.fromHeight * 0.55f);
  const glm::vec3 tip = e.hexes.back() + kUp * 0.9f;
  const glm::vec3 dir = glm::normalize(tip - hand);
  // the lance forms in the hand (violet motes gathering into bone) during the lead, flies from `impact - flight`... the rules' first
  // hit is the impact: the lance leaves the hand at `start` and reaches the farthest hex at `finish`; bodies are struck as it passes.
  const float gather = ease(e.start - kLanceLead, e.start, now);
  if (now < e.start) {
    for (int i = 0; i < 26; ++i) {
      const uint32_t n = static_cast<uint32_t>(i);
      const float phase = std::fmod(hash01(e.seed, n, 1) + (now - (e.start - kLanceLead)) * 1.6f, 1.0f);
      const float ang = hash01(e.seed, n, 2) * kTau + phase * 3.0f, r = glm::mix(0.8f, 0.08f, phase);
      const glm::vec3 p = hand + glm::vec3(std::cos(ang) * r, (hash01(e.seed, n, 3) - 0.5f) * 0.6f * (1.0f - phase), std::sin(ang) * r);
      motes_.push_back(mote(pulled(p, eye, kPull), 0.09f, std::sin(kPi * phase) * gather, (i & 1) ? kViolet : kBone));
    }
    lights.push_back({hand, {0.6f, 0.42f, 1.0f}, 3.0f * gather, 2.5f});
  }
  const float s = std::clamp((now - e.start) / (e.finish - e.start), 0.0f, 1.0f);
  const float gone = ease(e.finish + 0.6f, e.finish + 1.1f, now);
  if (now >= e.start && gone < 1.0f) {
    const glm::vec3 p = glm::mix(hand, tip, s);
    const float len = 1.5f, thick = 0.09f;
    const glm::vec3 base = p - dir * (len * 0.5f);
    standardDraw(frame, lanceFirst_, lanceCount_, along(pulled(base, eye, kGroundPull), dir, len, thick), kBone * (1.0f - 0.5f * gone), 0.55f, 0.05f, kIndigo * 0.4f, e.from);
    // the violet charge along it and the motes it sheds
    combat(sphereFirst_, sphereCount_, scaled(pulled(p, eye, kGroundPull), glm::vec3(0.32f, 0.32f, 0.32f)), kModeEnergy, now, 0.6f, 0.55f * (1.0f - gone), kViolet, 0.25f,
           glm::vec4(kCore, 0.0f), e.from, VfxBlend::Additive);
    for (int i = 0; i < 18; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float back = hash01(e.seed, n, 1) * 1.4f, age = std::fmod(hash01(e.seed, n, 2) + now * 2.0f, 1.0f);
      const glm::vec3 q = p - dir * back + glm::vec3(hash01(e.seed, n, 3) - 0.5f, hash01(e.seed, n, 4) - 0.3f, hash01(e.seed, n, 5) - 0.5f) * 0.4f;
      motes_.push_back(mote(pulled(q, eye, kGroundPull), 0.08f, (1.0f - age) * (1.0f - gone), kViolet));
    }
    lights.push_back({p, {0.6f, 0.42f, 1.0f}, 6.0f * (1.0f - gone), 3.0f});
  }
  // on each body along the line: struck as the lance passes it
  int lit = 0;
  for (size_t bi = 0; bi < e.bodies.size(); ++bi) {
    const Target& u = e.bodies[bi];
    const float h = bodyHeight(u.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const float share = glm::clamp(glm::dot(u.feet - e.from, tip - hand) / std::max(1e-4f, glm::dot(tip - hand, tip - hand)), 0.0f, 1.0f);
    const float tau = now - (e.start + share * (e.finish - e.start));
    if (tau < 0.0f) continue;
    const uint32_t seed = e.seed + static_cast<uint32_t>(bi) * 7919u;
    const glm::vec3 body = u.feet + kUp * (h * 0.5f);
    if (tau < 0.22f) {
      const float f = 1.0f - ease(0.0f, 0.22f, tau);
      streaks_.push_back(streak(pulled(body, eye, kPull), glm::vec3(0.0f), 1.0f * k * (0.5f + 0.5f * f), f, kCore, 2.6f));
    }
    if (tau < 0.6f) {
      const float r = (0.3f + 0.9f * easeOut(0.0f, 0.5f, tau)) * k;
      combat(discFirst_, discCount_, scaled(pulled(u.feet + kUp * 0.05f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, tau, 0.5f, 0.8f * (1.0f - ease(0.1f, 0.6f, tau)), kViolet,
             0.0f, glm::vec4(kCore, 0.85f), u.feet, VfxBlend::Additive);
    }
    // bone shards flung out and down
    for (int j = 0; j < 12 && tau < 0.9f; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 300u;
      const float a = hash01(seed, n, 1) * kTau, speed = 1.2f + 1.6f * hash01(seed, n, 2), lift = 1.0f + 2.0f * hash01(seed, n, 3);
      const glm::vec3 p = body + glm::vec3(std::cos(a) * speed * tau, lift * tau - 4.0f * tau * tau, std::sin(a) * speed * tau);
      if (p.y < u.feet.y + 0.05f) continue;
      shards_.push_back(puff(pulled(p, eye, kPull), 0.09f + 0.05f * hash01(seed, n, 4), a + tau * 9.0f, hash01(seed, n, 5) * 37.0f, 0.9f * (1.0f - ease(0.5f, 0.9f, tau)), kBone));
    }
    if (lit < 4 && tau < 0.8f) {
      ++lit;
      lights.push_back({body, {0.6f, 0.42f, 1.0f}, 8.0f * (1.0f - ease(0.1f, 0.8f, tau)), 2.6f * k});
    }
  }
}

// --- Imagem Espelhada --------------------------------------------------------------------------------------------------

void ConjurerFx::buildMirrorBurst(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 chest = e.centre + kUp * (h * 0.5f);
  const float begin = e.impact - kMirrorLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);
  // motes orbit the ally while it gathers, then a flash and a ring as the copies step out
  for (int i = 0; i < 30; ++i) {
    const uint32_t n = static_cast<uint32_t>(i);
    const float phase = std::fmod(hash01(e.seed, n, 1) + t * 0.9f, 1.0f), ang = hash01(e.seed, n, 2) * kTau + t * 3.0f;
    const float r = (0.3f + 0.6f * hash01(e.seed, n, 3)) * k * (age >= 0.0f ? 1.0f + 2.5f * easeOut(0.0f, 0.5f, age) : 1.0f);
    const glm::vec3 p = e.centre + glm::vec3(std::cos(ang) * r, h * (0.1f + phase * 1.0f), std::sin(ang) * r);
    const float alpha = std::sin(kPi * phase) * (age >= 0.0f ? 1.0f - ease(0.3f, 1.0f, age) : gather);
    if (alpha > 0.002f) motes_.push_back(mote(pulled(p, eye, kPull), 0.1f * k, alpha, (i & 1) ? kViolet : kCore));
  }
  if (age >= 0.0f) {
    if (age < 0.22f) streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.3f * k * (1.0f - age), 1.0f - ease(0.0f, 0.22f, age), kCore, 2.4f));
    if (age < 0.7f) {
      const float r = (0.3f + 1.6f * easeOut(0.0f, 0.55f, age)) * k;
      combat(discFirst_, discCount_, scaled(pulled(e.centre + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.55f, 0.8f * (1.0f - ease(0.15f, 0.7f, age)), kViolet,
             0.0f, glm::vec4(kCore, 0.9f), e.centre, VfxBlend::Additive);
    }
  }
  const float intensity = age >= 0.0f ? 14.0f * (1.0f - ease(0.0f, 0.5f, age)) + 3.0f * (1.0f - ease(0.5f, 1.2f, age)) : 2.0f + 5.0f * gather;
  if (intensity > 0.01f) lights.push_back({chest, {0.62f, 0.45f, 1.0f}, intensity, 3.0f * k});
}

void ConjurerFx::buildMirror(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const float t = now - e.start;
  const float in = ease(e.start, e.start + 0.4f, now), out = e.endAt < 0.0f ? 1.0f : 1.0f - ease(e.endAt, e.endAt + 0.6f, now);
  const float env = in * out;
  const glm::vec3 toward = flatDir(camera.position - e.centre, glm::vec3(0.0f, 0.0f, 1.0f));
  for (int slot = 0; slot < e.copies && env > 0.002f; ++slot) {
    const glm::vec3 feet = copyFeet(e.centre, slot, e.copies, camera, t);
    const float breathe = 1.0f + 0.03f * std::sin(t * 2.2f + static_cast<float>(slot));
    const float hover = 0.06f + 0.04f * std::sin(t * 1.7f + static_cast<float>(slot) * 1.3f);
    // the hooded shape, facing the eye, hollow violet energy with a brighter heart
    const glm::vec3 base = pulled(feet + kUp * hover, eye, kGroundPull);
    const float yaw = std::atan2(toward.x, toward.z);
    const glm::mat4 m = at(base) * glm::rotate(glm::mat4(1.0f), yaw, kUp) * glm::scale(glm::mat4(1.0f), {0.42f * k * breathe, h * 0.95f, 0.42f * k * breathe});
    combat(phantomFirst_, phantomCount_, m, kModeEnergy, t, 0.5f, 0.55f * env, kViolet, 0.18f, glm::vec4(kCore, 0.0f), e.centre, VfxBlend::Additive);
    const glm::mat4 inner = at(base + kUp * 0.1f) * glm::rotate(glm::mat4(1.0f), yaw, kUp) * glm::scale(glm::mat4(1.0f), {0.26f * k, h * 0.8f, 0.26f * k});
    combat(phantomFirst_, phantomCount_, inner, kModeEnergy, t + 3.0f, 0.4f, 0.35f * env, kIndigo, 0.5f, glm::vec4(kViolet, 0.0f), e.centre, VfxBlend::Additive);
    // the halo at its feet and a few motes drifting off it
    holy(haloFirst_, haloCount_, at(pulled(feet + kUp * 0.06f, eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), t * 0.8f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f * k)),
         kHolyLine, t, 0.7f, 0.6f * env, kViolet, kCore, 0.0f, e.centre);
    for (int i = 0; i < 6; ++i) {
      const uint32_t n = static_cast<uint32_t>(i + slot * 10);
      const float phase = std::fmod(hash01(e.seed, n, 1) + t * 0.35f, 1.0f), ang = hash01(e.seed, n, 2) * kTau + t * 0.9f, r = 0.3f * k;
      const glm::vec3 p = feet + glm::vec3(std::cos(ang) * r, h * (0.2f + phase * 1.0f), std::sin(ang) * r);
      motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.07f * k, std::sin(kPi * phase) * env * 0.8f, kViolet));
    }
  }
  // a copy shattering: motes flung out from where it stood
  for (const auto& [when, slot] : e.shatters) {
    const float age = now - when;
    if (age < 0.0f || age > 0.9f) continue;
    const glm::vec3 feet = copyFeet(e.centre, slot, std::max(e.copies, slot + 1), camera, t);
    const glm::vec3 body = feet + kUp * (h * 0.5f);
    if (age < 0.2f) streaks_.push_back(streak(pulled(body, eye, kPull), glm::vec3(0.0f), 1.1f * k * (1.0f - age), 1.0f - ease(0.0f, 0.2f, age), kCore, 2.4f));
    for (int i = 0; i < 24; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 500u + static_cast<uint32_t>(slot) * 100u;
      const float a = hash01(e.seed, n, 1) * kTau, lift = (hash01(e.seed, n, 2) - 0.3f) * h, speed = 0.8f + 1.2f * hash01(e.seed, n, 3);
      const glm::vec3 p = body + glm::vec3(std::cos(a) * speed * age, lift * age * 2.0f - 1.5f * age * age, std::sin(a) * speed * age);
      motes_.push_back(mote(pulled(p, eye, kPull), 0.09f * k, (1.0f - ease(0.3f, 0.9f, age)), (i & 1) ? kViolet : kCore));
    }
    lights.push_back({body, {0.62f, 0.45f, 1.0f}, 8.0f * (1.0f - ease(0.0f, 0.5f, age)), 2.5f});
  }
  if (e.copies > 0 && env > 0.0f) lights.push_back({e.centre + kUp * (h * 0.6f), {0.5f, 0.35f, 0.9f}, 2.2f * env, 3.0f * k});
}

// --- Detonar Invocação -----------------------------------------------------------------------------------------------------

void ConjurerFx::buildDetonate(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float R = e.radius, h = e.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 body = e.centre + kUp * (h * 0.5f);
  const float begin = e.impact - kDetonateLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);
  if (age < 0.0f) {
    // energy drawn into the summon: motes converging, its shell swelling and brightening
    for (int i = 0; i < 40; ++i) {
      const uint32_t n = static_cast<uint32_t>(i);
      const float phase = std::fmod(hash01(e.seed, n, 1) + t * 1.4f, 1.0f), ang = hash01(e.seed, n, 2) * kTau + phase * 2.0f;
      const float r = glm::mix(R * 0.8f, 0.1f, phase);
      const glm::vec3 p = e.centre + glm::vec3(std::cos(ang) * r, glm::mix(0.1f, h * 0.5f, phase), std::sin(ang) * r);
      motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.1f, std::sin(kPi * phase) * gather, (i & 1) ? kViolet : kCore));
    }
    const float swell = 0.45f * k * (1.0f + 0.6f * gather) * (1.0f + 0.05f * std::sin(t * 18.0f));
    combat(sphereFirst_, sphereCount_, scaled(pulled(body, eye, kPull * 0.5f), glm::vec3(swell, swell * 1.3f, swell)), kModeEnergy, t, 0.5f * (0.4f + 0.6f * gather), 0.5f * gather,
           kViolet, 0.3f, glm::vec4(kCore, 0.0f), e.centre, VfxBlend::Additive);
    lights.push_back({body, {0.62f, 0.45f, 1.0f}, 2.0f + 10.0f * gather, 3.0f * k});
    return;
  }
  // the blast: a flash, a dark pressure shell to the edge, two rings, rays, ash lifting, embers of violet; the light flares
  if (age < 0.25f) streaks_.push_back(streak(pulled(body, eye, kPull), glm::vec3(0.0f), 2.0f * k * (0.6f + 0.4f * (1.0f - age / 0.25f)), 1.0f - ease(0.0f, 0.25f, age), kCore, 3.0f));
  if (age < kDetonateSpread + 0.2f) {
    const float r = 0.3f + (R - 0.3f) * easeOut(0.0f, kDetonateSpread + 0.05f, age);
    combat(sphereFirst_, sphereCount_, scaled(body, glm::vec3(r, r * 0.8f, r)), kModeEnergy, age, 0.6f, 0.6f * (1.0f - ease(0.1f, kDetonateSpread + 0.2f, age)), kViolet, 0.03f,
           glm::vec4(kCore, 0.0f), e.centre, VfxBlend::Additive);
  }
  for (int ring = 0; ring < 2; ++ring) {
    const float a = age - 0.1f * static_cast<float>(ring);
    if (a < 0.0f || a > 0.85f) continue;
    const float r = 0.4f + (R * (1.05f - 0.08f * static_cast<float>(ring)) - 0.4f) * easeOut(0.0f, 0.5f, a);
    combat(discFirst_, discCount_, scaled(pulled(e.centre + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, a, ring == 0 ? 0.65f : 0.4f,
           (ring == 0 ? 0.85f : 0.5f) * (1.0f - ease(0.2f, 0.85f, a)), ring == 0 ? kViolet : kIndigo, 0.0f, glm::vec4(kCore, 0.9f), e.centre, VfxBlend::Additive);
  }
  constexpr int kRays = 20;
  if (age < 0.8f)
    for (int i = 0; i < kRays; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float a = (static_cast<float>(i) + hash01(e.seed, n, 1) * 0.7f) / kRays * kTau;
      const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
      const float reach = R * (0.9f + 0.2f * hash01(e.seed, n, 2));
      const float head = 0.3f + reach * easeOut(0.0f, 0.5f, age);
      const float len = std::min(head - 0.2f, 0.8f + 0.9f * hash01(e.seed, n, 3));
      if (len <= 0.05f) continue;
      streaks_.push_back(streak(pulled(e.centre + dir * (head - len * 0.5f) + kUp * 0.12f, eye, kGroundPull), dir * len, 0.07f, 1.0f - ease(0.25f, 0.8f, age), kViolet, 2.4f));
    }
  for (int i = 0; i < 24; ++i) {  // ash
    const uint32_t n = static_cast<uint32_t>(i) + 400u;
    const float a = hash01(e.seed, n, 1) * kTau, r = R * (0.1f + 0.8f * hash01(e.seed, n, 2));
    const float life = 1.6f * (0.8f + 0.4f * hash01(e.seed, n, 3)), s = (age - (r / R) * kDetonateSpread) / life;
    if (s < 0.0f || s > 1.0f) continue;
    const glm::vec3 p = e.centre + glm::vec3(std::cos(a) * r, 0.2f + 1.4f * easeOut(0.0f, 1.0f, s), std::sin(a) * r);
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), 0.5f + 0.8f * s, a + age * 0.6f, hash01(e.seed, n, 4) * 37.0f, 0.45f * ease(0.0f, 0.1f, s) * (1.0f - ease(0.3f, 1.0f, s)), kAsh));
  }
  for (int i = 0; i < 30 && age < 1.4f; ++i) {  // embers of violet thrown up and out
    const uint32_t n = static_cast<uint32_t>(i) + 600u;
    const float a = hash01(e.seed, n, 1) * kTau, out = 0.6f + 1.4f * hash01(e.seed, n, 2), lift = 1.5f + 2.5f * hash01(e.seed, n, 3);
    const glm::vec3 p = body + glm::vec3(std::cos(a) * out * age * 1.5f, lift * age - 3.0f * age * age, std::sin(a) * out * age * 1.5f);
    if (p.y < e.centre.y + 0.05f) continue;
    motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.1f, 1.0f - ease(0.5f, 1.4f, age), (i & 1) ? kViolet : kCore));
  }
  lights.push_back({body, {0.65f, 0.45f, 1.0f}, 28.0f * (1.0f - ease(0.0f, 0.4f, age)) + 6.0f * (1.0f - ease(0.4f, 1.6f, age)), glm::mix(3.0f, R * 1.3f, ease(0.0f, kDetonateSpread, age))});
  // a flash on every body the blast reaches
  for (size_t bi = 0; bi < e.bodies.size(); ++bi) {
    const Target& u = e.bodies[bi];
    const float hu = bodyHeight(u.height), ku = std::clamp(hu / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(u.feet.x - e.centre.x, u.feet.z - e.centre.z));
    const float tau = age - std::min(dist / R, 1.0f) * kDetonateSpread;
    if (tau < 0.0f || tau > 0.6f) continue;
    const glm::vec3 b = u.feet + kUp * (hu * 0.5f);
    if (tau < 0.2f) streaks_.push_back(streak(pulled(b, eye, kPull), glm::vec3(0.0f), 1.1f * ku * (1.0f - tau), 1.0f - ease(0.0f, 0.2f, tau), kCore, 2.6f));
    const float r = (0.3f + 0.9f * easeOut(0.0f, 0.5f, tau)) * ku;
    combat(discFirst_, discCount_, scaled(pulled(u.feet + kUp * 0.05f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, tau, 0.5f, 0.8f * (1.0f - ease(0.1f, 0.6f, tau)), kViolet, 0.0f,
           glm::vec4(kCore, 0.85f), u.feet, VfxBlend::Additive);
  }
}

// --- Elo de Sacrifício -----------------------------------------------------------------------------------------------------

void ConjurerFx::buildBond(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.fromHeight, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 chest = e.from + kUp * (h * 0.55f);
  const float begin = e.impact - kBondLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);
  const float env = gather * (1.0f - ease(e.impact + 1.2f, e.impact + 2.0f, now));
  // the halo closing round the caster, dark motes drawn to its chest
  if (env > 0.002f) {
    const float r = glm::mix(1.1f, 0.45f, ease(begin, e.impact + 0.2f, now)) * k;
    holy(haloFirst_, haloCount_, at(pulled(chest, eye, kPull)) * glm::rotate(glm::mat4(1.0f), t * 1.5f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(r)), kHolyLine, t, 1.0f, env,
         kIndigo, kViolet, 0.0f, e.from);
  }
  for (int i = 0; i < 20; ++i) {
    const uint32_t n = static_cast<uint32_t>(i);
    const float phase = std::fmod(hash01(e.seed, n, 1) + t * 1.1f, 1.0f), ang = hash01(e.seed, n, 2) * kTau + phase * 2.5f, r = glm::mix(1.2f, 0.1f, phase) * k;
    const glm::vec3 p = e.from + glm::vec3(std::cos(ang) * r, glm::mix(0.1f, h * 0.55f, phase), std::sin(ang) * r);
    motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.09f * k, std::sin(kPi * phase) * env, (i & 1) ? kIndigo : kViolet));
  }
  // the chains: a tube from the caster's chest to each summon's chest, a pulse running along it at the impact
  for (size_t bi = 0; bi < e.bodies.size() && age >= -0.2f; ++bi) {
    const Target& u = e.bodies[bi];
    const float hu = bodyHeight(u.height);
    const glm::vec3 to = u.feet + kUp * (hu * 0.5f);
    const glm::vec3 mid = glm::mix(chest, to, 0.5f) + kUp * (0.6f + 0.15f * std::sin(t * 2.0f + static_cast<float>(bi)));
    const three::CatmullRom curve{{chest, glm::mix(chest, mid, 0.5f), mid, glm::mix(mid, to, 0.5f), to}};
    const std::vector<VfxVertex> tube = vfxgeo::tube(curve, 36, 0.045f * k, 6);
    const float chainEnv = ease(e.impact - 0.2f, e.impact + 0.1f, now) * (1.0f - ease(e.impact + 1.3f, e.impact + 2.0f, now));
    holy(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kHolyTube, t, 0.9f, chainEnv, kIndigo, kViolet, 0.0f, e.from);
    // the pulse: a bright bead running from the caster to the summon
    if (age >= 0.0f && age < 0.8f) {
      const glm::vec3 bead = curve.getPoint(easeOut(0.0f, 0.8f, age));
      streaks_.push_back(streak(pulled(bead, eye, kGroundPull), glm::vec3(0.0f), 0.3f, 1.0f - ease(0.6f, 0.8f, age), kCore, 2.6f));
    }
    if (age >= 0.7f && age < 1.2f) {  // it lands: a ring at the summon's feet
      const float tau = age - 0.7f, r = 0.3f + 0.7f * easeOut(0.0f, 0.4f, tau);
      combat(discFirst_, discCount_, scaled(pulled(u.feet + kUp * 0.05f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, tau, 0.5f, 0.7f * (1.0f - ease(0.1f, 0.5f, tau)), kViolet,
             0.0f, glm::vec4(kCore, 0.85f), u.feet, VfxBlend::Additive);
    }
  }
  if (env > 0.0f) lights.push_back({chest, {0.45f, 0.3f, 0.9f}, 6.0f * env + (age >= 0.0f ? 8.0f * (1.0f - ease(0.0f, 0.4f, age)) : 0.0f), 3.5f * k});
}

// --- Pesadelo ----------------------------------------------------------------------------------------------------------

void ConjurerFx::buildNightmare(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float R = e.radius;
  const float begin = e.impact - kNightmareLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);
  const float env = ease(begin, begin + 0.3f, now) * (1.0f - ease(e.impact + 1.4f, e.impact + 2.4f, now));
  // the cloud: dark puffs rolling low over the whole area, denser as it gathers, lifting and thinning as it goes
  for (int i = 0; i < 46; ++i) {
    const uint32_t n = static_cast<uint32_t>(i);
    const float a = hash01(e.seed, n, 1) * kTau + t * 0.25f * (hash01(e.seed, n, 2) > 0.5f ? 1.0f : -1.0f), r = R * (0.1f + 0.85f * hash01(e.seed, n, 3));
    const float y = 0.5f + 0.9f * hash01(e.seed, n, 4) + 0.15f * std::sin(t * 1.4f + static_cast<float>(i)) + (age > 0.0f ? 0.5f * ease(0.8f, 2.4f, age) : 0.0f);
    const float size = 1.0f + 1.0f * hash01(e.seed, n, 5);
    const glm::vec3 p = e.centre + glm::vec3(std::cos(a) * r, y, std::sin(a) * r);
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), size, a + t * 0.3f, hash01(e.seed, n, 6) * 37.0f, 0.55f * env * (age >= 0.0f ? 1.0f : 0.5f + 0.5f * gather), kShadow));
  }
  // the pool of violet under the cloud and its ring at the edge
  if (env > 0.002f) {
    const float rad = R * (0.5f + 0.5f * easeOut(begin, begin + 0.5f, now));
    holy(discFirst_, discCount_, scaled(pulled(e.centre + kUp * 0.045f, eye, kGroundPull), {rad, 1.0f, rad}), kHolyPool, t, 0.35f * (1.0f + (age >= 0.0f ? 1.5f * (1.0f - ease(0.0f, 0.5f, age)) : 0.0f)),
         0.6f * env, kIndigo, kViolet, age >= 0.0f ? 0.3f + 0.7f * easeOut(0.0f, kNightmareSpread, age) : 0.3f, e.centre);
  }
  // eyes of light opening in the dark: pairs of dots blinking
  for (int i = 0; i < 7; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float open = ease(begin + 0.1f + hash01(e.seed, n, 1) * 0.4f, begin + 0.5f + hash01(e.seed, n, 1) * 0.4f, now);
    const float blink = std::fmod(t * 0.7f + hash01(e.seed, n, 2), 1.0f) < 0.85f ? 1.0f : 0.0f;
    const float a = hash01(e.seed, n, 3) * kTau, r = R * (0.2f + 0.7f * hash01(e.seed, n, 4));
    const glm::vec3 p = e.centre + glm::vec3(std::cos(a) * r, 0.9f + 0.7f * hash01(e.seed, n, 5), std::sin(a) * r);
    const glm::vec3 side = flatDir(glm::vec3(camera.right.x, 0.0f, camera.right.z), glm::vec3(1.0f, 0.0f, 0.0f));
    const float alpha = open * blink * env;
    if (alpha > 0.002f) {
      streaks_.push_back(streak(pulled(p - side * 0.09f, eye, kGroundPull), glm::vec3(0.0f), 0.08f, alpha, kEye, 2.6f));
      streaks_.push_back(streak(pulled(p + side * 0.09f, eye, kGroundPull), glm::vec3(0.0f), 0.08f, alpha, kEye, 2.6f));
    }
  }
  // the light: violet, pulsing, flaring at the impact
  {
    const float pulse = 0.8f + 0.2f * std::sin(t * 7.0f);
    const float intensity = (2.0f + 5.0f * gather) * pulse + (age >= 0.0f ? 16.0f * (1.0f - ease(0.0f, 0.4f, age)) + 4.0f * (1.0f - ease(0.4f, 2.0f, age)) : 0.0f);
    if (env > 0.0f) lights.push_back({e.centre + kUp * 1.2f, {0.55f, 0.35f, 1.0f}, intensity * env, R * 1.2f});
  }
  if (age >= 0.0f) {
    if (age < 0.25f) streaks_.push_back(streak(pulled(e.centre + kUp * 0.9f, eye, kGroundPull), glm::vec3(0.0f), 1.8f * (1.0f - age), 1.0f - ease(0.0f, 0.25f, age), kViolet, 2.6f));
    if (age < 0.9f) {
      const float r = 0.4f + (R * 1.03f - 0.4f) * easeOut(0.0f, 0.55f, age);
      combat(discFirst_, discCount_, scaled(pulled(e.centre + kUp * 0.05f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.55f, 0.8f * (1.0f - ease(0.2f, 0.9f, age)), kViolet,
             0.0f, glm::vec4(kCore, 0.9f), e.centre, VfxBlend::Additive);
    }
  }
  // on each foe the terror reaches: tendrils of shadow rise round it, a flash, its own flicker of violet
  int lit = 0;
  for (size_t bi = 0; bi < e.bodies.size(); ++bi) {
    const Target& u = e.bodies[bi];
    const float h = bodyHeight(u.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(u.feet.x - e.centre.x, u.feet.z - e.centre.z));
    const float tau = age - std::min(dist / R, 1.0f) * kNightmareSpread;
    if (tau < 0.0f) continue;
    const uint32_t seed = e.seed + static_cast<uint32_t>(bi) * 7919u;
    const glm::vec3 body = u.feet + kUp * (h * 0.5f);
    const float tenv = ease(0.0f, 0.15f, tau) * (1.0f - ease(1.0f, 1.7f, tau));
    if (tau < 0.22f) streaks_.push_back(streak(pulled(body, eye, kPull), glm::vec3(0.0f), 1.1f * k * (1.0f - tau), 1.0f - ease(0.0f, 0.22f, tau), kViolet, 2.6f));
    for (int j = 0; j < 4 && tenv > 0.002f; ++j) {
      const float a = hash01(seed, static_cast<uint32_t>(j) + 300u, 1) * kTau + tau * 0.8f, r = 0.35f * k;
      const float rise = easeOut(0.0f, 0.5f, tau) * h * (1.1f + 0.2f * hash01(seed, static_cast<uint32_t>(j) + 300u, 2));
      const glm::vec3 root = u.feet + glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
      const glm::vec3 tip = u.feet + glm::vec3(std::cos(a + 1.2f) * r * 0.5f, rise, std::sin(a + 1.2f) * r * 0.5f);
      const three::CatmullRom curve{{root, glm::mix(root, tip, 0.35f) + glm::vec3(std::cos(a + 2.0f), 0.0f, std::sin(a + 2.0f)) * (0.2f * k), glm::mix(root, tip, 0.7f), tip}};
      const std::vector<VfxVertex> tube = vfxgeo::tube(curve, 16, 0.035f * k, 5);
      holy(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kHolyTube, tau, 0.8f, tenv * 0.9f, kIndigo, kViolet, 0.0f, u.feet);
    }
    for (int j = 0; j < 8; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 500u;
      const float phase = std::fmod(hash01(seed, n, 1) + tau * 0.9f, 1.0f), ang = hash01(seed, n, 2) * kTau + tau * 2.0f, r = (0.2f + 0.2f * hash01(seed, n, 3)) * k;
      const glm::vec3 p = u.feet + glm::vec3(std::cos(ang) * r, h * (0.1f + phase * 1.1f), std::sin(ang) * r);
      motes_.push_back(mote(pulled(p, eye, kPull), 0.08f * k, std::sin(kPi * phase) * tenv, kViolet));
    }
    if (lit < 4 && tenv > 0.0f) {
      ++lit;
      lights.push_back({body, {0.55f, 0.35f, 1.0f}, 7.0f * tenv * (0.8f + 0.2f * std::sin(tau * 11.0f)), 2.6f * k});
    }
  }
}

// --- Fortalecer Invocações ---------------------------------------------------------------------------------------------------

void ConjurerFx::buildEmpower(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const CombatDraw combat{frame};
  const HolyDraw holy{frame};
  const glm::vec3 eye = camera.position;
  const float h = e.fromHeight, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 chest = e.from + kUp * (h * 0.55f);
  const float begin = e.impact - kEmpowerLead, age = now - e.impact, t = now - begin;
  const float gather = ease(begin, e.impact, now);
  // the caster: motes gathering at the chest, a pulse out across the ground at the impact
  for (int i = 0; i < 22 && age < 0.2f; ++i) {
    const uint32_t n = static_cast<uint32_t>(i);
    const float phase = std::fmod(hash01(e.seed, n, 1) + t * 1.2f, 1.0f), ang = hash01(e.seed, n, 2) * kTau + phase * 2.5f, r = glm::mix(1.0f, 0.1f, phase) * k;
    const glm::vec3 p = e.from + glm::vec3(std::cos(ang) * r, glm::mix(0.1f, h * 0.55f, phase), std::sin(ang) * r);
    motes_.push_back(mote(pulled(p, eye, kGroundPull), 0.09f * k, std::sin(kPi * phase) * gather * (1.0f - ease(0.0f, 0.2f, age)), (i & 1) ? kViolet : kCore));
  }
  if (age >= 0.0f && age < 0.9f) {
    float reach = 2.0f;
    for (const Target& u : e.bodies) reach = std::max(reach, glm::length(glm::vec2(u.feet.x - e.from.x, u.feet.z - e.from.z)) + 1.0f);
    const float r = 0.3f + (reach - 0.3f) * easeOut(0.0f, 0.6f, age);
    combat(discFirst_, discCount_, scaled(pulled(e.from + kUp * 0.045f, eye, kGroundPull), {r, 1.0f, r}), kModeRing, age, 0.5f, 0.7f * (1.0f - ease(0.2f, 0.9f, age)), kViolet, 0.0f,
           glm::vec4(kCore, 0.9f), e.from, VfxBlend::Additive);
    if (age < 0.2f) streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 1.1f * k * (1.0f - age), 1.0f - ease(0.0f, 0.2f, age), kCore, 2.4f));
  }
  lights.push_back({chest, {0.62f, 0.45f, 1.0f}, 2.0f + 6.0f * gather * (1.0f - ease(0.3f, 1.0f, age)), 3.0f * k});
  // on each summon as the pulse reaches it: a violet shaft rises, a halo climbs the body, motes converge, a light
  int lit = 0;
  for (size_t bi = 0; bi < e.bodies.size(); ++bi) {
    const Target& u = e.bodies[bi];
    const float hu = bodyHeight(u.height), ku = std::clamp(hu / 2.0f, 0.7f, 1.4f);
    const float dist = glm::length(glm::vec2(u.feet.x - e.from.x, u.feet.z - e.from.z));
    const float tau = age - std::min(dist / 6.0f, 1.0f) * 0.4f;
    if (tau < 0.0f) continue;
    const uint32_t seed = e.seed + static_cast<uint32_t>(bi) * 7919u;
    const glm::vec3 body = u.feet + kUp * (hu * 0.5f);
    const float env = ease(0.0f, 0.12f, tau) * (1.0f - ease(1.0f, 1.8f, tau));
    if (env > 0.002f) {
      const float rise = easeOut(0.0f, 0.3f, tau), r = 0.34f * ku * (1.0f + 0.04f * std::sin(tau * 6.0f));
      holy(columnFirst_, columnCount_, scaled(pulled(u.feet, eye, kPull * 0.5f), {r, hu * 1.5f * rise + 1e-3f, r}), kHolyShaft, tau, 0.35f, env, kViolet, kCore, 0.0f, u.feet);
      holy(columnFirst_, columnCount_, scaled(pulled(u.feet, eye, kPull * 0.5f), {r * 0.45f, hu * 1.35f * rise + 1e-3f, r * 0.45f}), kHolyShaft, tau, 0.3f, env * 0.8f, kCore,
           kCore, 0.0f, u.feet);
      const float climb = ease(0.1f, 1.0f, tau);
      holy(haloFirst_, haloCount_, at(pulled(u.feet + kUp * glm::mix(0.08f, hu * 1.1f, climb), eye, kPull)) * glm::rotate(glm::mat4(1.0f), tau * 2.5f, kUp) *
                                       glm::scale(glm::mat4(1.0f), glm::vec3(glm::mix(0.7f, 0.4f, climb) * ku)),
           kHolyLine, tau, 1.0f, env, kViolet, kCore, 0.0f, u.feet);
      const float grow = ease(0.0f, 0.4f, tau), radius = 0.7f * ku * (0.6f + 0.4f * grow);
      holy(discFirst_, discCount_, scaled(pulled(u.feet + kUp * 0.035f, eye, kGroundPull), {radius, 1.0f, radius}), kHolyPool, tau, 0.4f, env, kViolet, kCore, 0.25f + 0.6f * grow,
           u.feet);
    }
    if (tau < 0.25f) streaks_.push_back(streak(pulled(body, eye, kPull), glm::vec3(0.0f), 1.0f * ku * (1.0f - tau), 1.0f - ease(0.0f, 0.25f, tau), kCore, 2.4f));
    for (int i = 0; i < 16; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 200u;
      const float phase = std::fmod(hash01(seed, n, 1) + tau * 0.8f, 1.0f), ang = hash01(seed, n, 2) * kTau + phase * 3.0f, r = glm::mix(0.9f, 0.1f, phase) * ku;
      const glm::vec3 p = u.feet + glm::vec3(std::cos(ang) * r, glm::mix(hu * 1.2f, hu * 0.5f, phase), std::sin(ang) * r);
      motes_.push_back(mote(pulled(p, eye, kPull), 0.09f * ku, std::sin(kPi * phase) * env, (i & 1) ? kViolet : kCore));
    }
    if (lit < 4 && env > 0.0f) {
      ++lit;
      lights.push_back({body, {0.62f, 0.45f, 1.0f}, 7.0f * env, 2.8f * ku});
    }
  }
}

}  // namespace ember
