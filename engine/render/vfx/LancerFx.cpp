#include "render/vfx/LancerFx.h"

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
glm::mat4 alignY(glm::vec3 axis) {
  const glm::vec3 y = glm::normalize(axis);
  const glm::vec3 helper = std::abs(y.y) < 0.95f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 x = glm::normalize(glm::cross(helper, y)), z = glm::cross(x, y);
  glm::mat4 m(1.0f);
  m[0] = glm::vec4(x, 0.0f), m[1] = glm::vec4(y, 0.0f), m[2] = glm::vec4(z, 0.0f);
  return m;
}
float bodyHeight(float h) { return std::clamp(h > 0.0f ? h : 1.8f, 1.0f, 4.0f); }
// A flight's arc as MapStage draws the card (leapOffset): the ground line plus arc x 4 s (1 - s).
glm::vec3 arcAt(glm::vec3 from, glm::vec3 to, float arc, float s) { return glm::mix(from, to, s) + kUp * (arc * 4.0f * s * (1.0f - s)); }

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
// The lance's shaft: a unit box along y (stretched by its matrix); its head: a long steel point.
const std::vector<VfxVertex>& shaftGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(1.0f, 1.0f, 1.0f);
  return g;
}
std::vector<VfxVertex> flatShaded(std::vector<VfxVertex> r, glm::vec3 squash) {
  for (VfxVertex& v : r) v.position *= squash;
  for (size_t i = 0; i + 2 < r.size(); i += 3) {
    const glm::vec3 n = glm::normalize(glm::cross(r[i + 1].position - r[i].position, r[i + 2].position - r[i].position));
    r[i].normal = r[i + 1].normal = r[i + 2].normal = n;
  }
  return r;
}
const std::vector<VfxVertex>& headGeometry() {
  static const std::vector<VfxVertex> g = flatShaded(vfxgeo::octahedron(1.0f, 0), glm::vec3(0.28f, 1.0f, 0.1f));
  return g;
}
// A chipped stone, squashed a little.
const std::vector<VfxVertex>& rockGeometry() {
  static const std::vector<VfxVertex> g = flatShaded(vfxgeo::icosahedron(1.0f, 0), glm::vec3(1.0f, 0.7f, 0.85f));
  return g;
}
// A knocked-away weapon: a thin blade along y and its crossguard.
const std::vector<VfxVertex>& bladeGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(0.07f, 1.0f, 0.012f);
  return g;
}
const std::vector<VfxVertex>& guardGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(0.22f, 0.03f, 0.03f);
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
constexpr glm::vec3 kEmber{1.0f, 0.55f, 0.2f}, kEmberCore{1.0f, 0.86f, 0.6f};
constexpr glm::vec3 kGold{1.0f, 0.8f, 0.45f};
constexpr glm::vec3 kDust{0.46f, 0.4f, 0.33f}, kScrape{0.22f, 0.18f, 0.14f};
constexpr glm::vec3 kWood{0.36f, 0.25f, 0.15f}, kSteelLit{0.6f, 0.63f, 0.68f}, kGround{0.34f, 0.3f, 0.26f};

}  // namespace

float LancerFx::Effect::end() const {
  switch (kind) {
    case Kind::Toss: return impact + 0.7f;
    case Kind::TossFlight: return finish + 1.5f;
    case Kind::Hook: return finish;
    case Kind::HookPull: return finish + 0.7f;
    case Kind::Vault: return finish + 1.3f;
    case Kind::VaultBlow:
    case Kind::SwapBlow:
    case Kind::VigilStrike: return impact + 0.8f;
    case Kind::Disarm: return impact + 0.6f;
    case Kind::Disarmed: return impact + 3.6f;
    case Kind::Swap: return finish + 0.9f;
    case Kind::Impale: return impact + 0.5f;
    case Kind::Drag: return finish + 2.4f;
    case Kind::VigilRise: return impact + 1.0f;
    case Kind::Vigil: return finish < 0.0f ? 1e30f : finish + 0.6f;
    case Kind::Pierce: return finish + 1.2f;
  }
  return impact + 1.0f;
}

// --- The stage's calls --------------------------------------------------------------------------------------------------

void LancerFx::toss(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::Toss, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.12f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a01u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::tossFlight(glm::vec3 from, glm::vec3 to, float height, float arc, float start, float finish, bool collided) {
  Effect e;
  e.kind = Kind::TossFlight, e.feet = from, e.to = to, e.height = bodyHeight(height), e.arc = std::max(arc, 0.3f);
  e.start = start, e.impact = start, e.finish = std::max(finish, start + 0.1f), e.collided = collided;
  e.dir = flatDir(to - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a02u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::hook(glm::vec3 from, float height, const Target& foe, float release, float impact) {
  Effect e;
  e.kind = Kind::Hook, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.height2 = bodyHeight(height);
  e.start = release, e.impact = std::max(impact, release + 0.06f), e.finish = e.impact + 0.5f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a03u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::hookPull(glm::vec3 caster, float casterHeight, glm::vec3 from, glm::vec3 to, float height, float start, float finish) {
  // The thrown hook's chain gives way to the pull's (the latest hook on this body ends as the pull begins).
  for (Effect& h : effects_)
    if (h.kind == Kind::Hook && glm::length(h.feet - from) < 0.3f) h.finish = std::min(h.finish, start);
  Effect e;
  e.kind = Kind::HookPull, e.feet = from, e.to = to, e.from = caster, e.height = bodyHeight(height), e.height2 = bodyHeight(casterHeight);
  e.start = start, e.impact = start, e.finish = std::max(finish, start + 0.1f);
  e.dir = flatDir(to - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a04u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::vault(glm::vec3 from, float height, glm::vec3 over, glm::vec3 landing, float start, float land) {
  Effect e;
  e.kind = Kind::Vault, e.feet = from, e.to = landing, e.from = over, e.height = bodyHeight(height);
  e.start = start, e.impact = std::max(land, start + 0.1f), e.finish = e.impact;
  e.dir = flatDir(landing - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.arc = 2.0f;
  e.seed = 0x5a05u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::vaultBlow(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::VaultBlow, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.1f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a06u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::disarm(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::Disarm, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.1f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a07u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::disarmed(const Target& foe, glm::vec3 from, float at) {
  Effect e;
  e.kind = Kind::Disarmed, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = at, e.start = at;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a08u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::swap(glm::vec3 a, float aHeight, glm::vec3 b, float bHeight, float start, float finish) {
  Effect e;
  e.kind = Kind::Swap, e.feet = a, e.to = b, e.from = b, e.to2 = a, e.height = bodyHeight(aHeight), e.height2 = bodyHeight(bHeight);
  e.start = start, e.impact = start, e.finish = std::max(finish, start + 0.1f), e.arc = 0.55f;
  e.dir = flatDir(b - a, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a09u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::swapBlow(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::SwapBlow, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.1f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a0au + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::impale(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::Impale, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.1f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a0bu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::drag(glm::vec3 foeFrom, glm::vec3 foeTo, float foeHeight, glm::vec3 casterFrom, glm::vec3 casterTo, float casterHeight, float start, float finish) {
  Effect e;
  e.kind = Kind::Drag, e.feet = foeFrom, e.to = foeTo, e.from = casterFrom, e.to2 = casterTo, e.height = bodyHeight(foeHeight), e.height2 = bodyHeight(casterHeight);
  e.start = start, e.impact = start, e.finish = std::max(finish, start + 0.1f);
  e.dir = flatDir(foeTo - foeFrom, flatDir(foeFrom - casterFrom, glm::vec3(0.0f, 0.0f, 1.0f)));
  e.seed = 0x5a0cu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void LancerFx::vigilRise(glm::vec3 feet, float height, float radius, float impact) {
  Effect e;
  e.kind = Kind::VigilRise, e.feet = feet, e.height = bodyHeight(height), e.radius = std::max(radius, kHex), e.impact = impact, e.start = impact;
  e.seed = 0x5a0du + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
int LancerFx::vigilStart(glm::vec3 feet, float height, float radius, float now) {
  Effect e;
  e.kind = Kind::Vigil, e.id = ++vigilSerial_, e.feet = feet, e.height = bodyHeight(height), e.radius = std::max(radius, kHex);
  e.start = now, e.impact = now, e.finish = -1.0f;
  e.seed = 0x5a0eu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
  return vigilSerial_;
}
void LancerFx::vigilMove(int id, glm::vec3 feet) {
  for (Effect& e : effects_)
    if (e.kind == Kind::Vigil && e.id == id) e.feet = feet;
}
void LancerFx::vigilEnd(int id, float now) {
  for (Effect& e : effects_)
    if (e.kind == Kind::Vigil && e.id == id && e.finish < 0.0f) e.finish = now;
}
void LancerFx::vigilStrike(const Target& foe, glm::vec3 from, float impact) {
  Effect e;
  e.kind = Kind::VigilStrike, e.feet = foe.feet, e.from = from, e.height = bodyHeight(foe.height), e.impact = impact, e.start = impact - 0.08f;
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.seed = 0x5a0fu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void LancerFx::pierce(glm::vec3 from, float height, glm::vec3 end, std::vector<Target> bodies, float start, float impact) {
  Effect e;
  e.kind = Kind::Pierce, e.feet = from, e.from = from, e.to = end, e.height = e.height2 = bodyHeight(height);
  e.dir = flatDir(end - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.start = start, e.impact = std::max(impact, start + 0.05f);
  const auto along = [&](glm::vec3 p) { return glm::dot(glm::vec3(p.x - from.x, 0.0f, p.z - from.z), e.dir); };
  std::sort(bodies.begin(), bodies.end(), [&](const Target& a, const Target& b) { return along(a.feet) < along(b.feet); });
  // The point's speed: it reaches the first body on the impact (half a hex out when the line holds none), then runs on to the end.
  const float first = bodies.empty() ? 0.5f * kHex : std::max(along(bodies.front().feet) - 0.3f, 0.3f);
  e.radius = std::max(first / (e.impact - e.start), 25.0f);
  e.finish = e.start + (std::max(along(end), first) + 0.6f) / e.radius;
  e.bodies = std::move(bodies);
  e.seed = 0x5a10u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

bool LancerFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.kind != Kind::Vigil && e.end() > now; });
}

void LancerFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  shaftFirst_ = frame.addVertices(shaftGeometry()), shaftCount_ = static_cast<uint32_t>(shaftGeometry().size());
  headFirst_ = frame.addVertices(headGeometry()), headCount_ = static_cast<uint32_t>(headGeometry().size());
  rockFirst_ = frame.addVertices(rockGeometry()), rockCount_ = static_cast<uint32_t>(rockGeometry().size());
  bladeFirst_ = frame.addVertices(bladeGeometry()), bladeCount_ = static_cast<uint32_t>(bladeGeometry().size());
  guardFirst_ = frame.addVertices(guardGeometry()), guardCount_ = static_cast<uint32_t>(guardGeometry().size());
  streaks_.clear(), puffs_.clear(), shafts_.clear(), heads_.clear(), rocks_.clear(), blades_.clear(), guards_.clear();
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    switch (e.kind) {
      case Kind::Toss: buildToss(e, now, camera, frame, lights); break;
      case Kind::TossFlight: buildTossFlight(e, now, camera, frame, lights); break;
      case Kind::Hook: buildHook(e, now, camera, frame, lights); break;
      case Kind::HookPull: buildHookPull(e, now, camera, frame, lights); break;
      case Kind::Vault: buildVault(e, now, camera, frame, lights); break;
      case Kind::VaultBlow:
      case Kind::SwapBlow:
      case Kind::Disarm:
      case Kind::Impale:
      case Kind::VigilStrike: buildThrustBlow(e, now, camera, frame, lights); break;
      case Kind::Disarmed: buildDisarmed(e, now, camera, frame, lights); break;
      case Kind::Swap: buildSwap(e, now, camera, frame, lights); break;
      case Kind::Drag: buildDrag(e, now, camera, frame, lights); break;
      case Kind::VigilRise: buildVigilRise(e, now, camera, frame, lights); break;
      case Kind::Vigil: buildVigil(e, now, camera, frame, lights); break;
      case Kind::Pierce: buildPierce(e, now, camera, frame, lights); break;
    }
  }
  // The particles of everything in play, one draw each: the dust first (it covers), the lit pieces, then the light.
  const glm::vec3 sight = effects_.front().feet;
  const Draw draw{frame};
  if (!puffs_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
         static_cast<uint32_t>(puffs_.size()));
  }
  struct Lit {
    const std::vector<VfxInstance>* list;
    uint32_t first, count;
    float roughness, metal;
  };
  const Lit lit[] = {{&shafts_, shaftFirst_, shaftCount_, 0.8f, 0.0f},  {&heads_, headFirst_, headCount_, 0.35f, 0.7f},  {&rocks_, rockFirst_, rockCount_, 0.92f, 0.0f},
                     {&blades_, bladeFirst_, bladeCount_, 0.3f, 0.8f}, {&guards_, guardFirst_, guardCount_, 0.5f, 0.6f}};
  for (const Lit& l : lit) {
    if (l.list->empty()) continue;
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), l.list->begin(), l.list->end());
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {l.roughness, l.metal, 0.0f, 0.0f};
    d.gpu.params1 = glm::vec4(0.0f);
    d.gpu.color = glm::vec4(1.0f);
    d.gpu.firstVertex = l.first;
    d.gpu.vertexCount = l.count;
    d.gpu.firstInstance = first;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Standard);
    d.gpu.sightAt = sight;
    d.instances = static_cast<uint32_t>(l.list->size());
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

void LancerFx::thrustSheet(glm::vec3 a, glm::vec3 b, glm::vec3 side, float bow, float width, float intensity, glm::vec3 colour, float age, float sweep, glm::vec3 eye,
                           glm::vec3 sight, VfxFrame& frame) {
  constexpr float kHold = 0.04f, kFade = 0.22f;
  const float head = easeOut(0.0f, sweep, age);
  const float tail = ease(sweep * 0.6f, sweep + kHold + kFade, age);
  const float opacity = 1.0f - ease(sweep + kHold, sweep + kHold + kFade, age);
  if (opacity <= 0.0f || head <= tail) return;
  constexpr int kSegments = 20;
  std::vector<VfxVertex> sheet;
  sheet.reserve(kSegments * 6);
  std::vector<glm::vec3> edge(kSegments + 1), inner(kSegments + 1);
  for (int j = 0; j <= kSegments; ++j) {
    const float s = static_cast<float>(j) / kSegments;
    const float w = width * std::pow(std::sin(kPi * s), 0.5f) + 0.004f;
    const glm::vec3 p = glm::mix(a, b, s) + side * (bow * std::sin(kPi * s));
    edge[j] = pulled(p + kUp * (w * 0.5f), eye, kPull);
    inner[j] = pulled(p - kUp * (w * 0.5f), eye, kPull);
  }
  for (int j = 0; j < kSegments; ++j) {
    const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
    const VfxVertex a0{inner[j], s0, side, 0.0f}, a1{edge[j], s0, side, 1.0f}, b0{inner[j + 1], s1, side, 0.0f}, b1{edge[j + 1], s1, side, 1.0f};
    sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
  }
  const Draw draw{frame};
  draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, age, intensity, opacity, colour, tail, glm::vec4(kWhite, head), sight,
       VfxBlend::Additive);
}

void LancerFx::chain(glm::vec3 a, glm::vec3 b, float sag, float alpha, float age, glm::vec3 eye, glm::vec3 sight, VfxFrame& frame) {
  if (alpha <= 0.01f) return;
  three::CatmullRom path;
  constexpr int kPoints = 7;
  for (int j = 0; j <= kPoints; ++j) {
    const float s = static_cast<float>(j) / kPoints;
    path.points.push_back(pulled(glm::mix(a, b, s) - kUp * (sag * 4.0f * s * (1.0f - s)), eye, kPull));
  }
  const std::vector<VfxVertex> tube = vfxgeo::tube(path, 24, 0.035f, 6);
  const Draw draw{frame};
  draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, age, 1.3f, alpha, kEmber, 0.0f, glm::vec4(kEmberCore, 0.0f), sight,
       VfxBlend::Additive);
  // the links: bright beads along it
  const float len = glm::length(b - a);
  const int links = std::clamp(static_cast<int>(len / 0.22f), 2, 40);
  for (int j = 0; j <= links; ++j) {
    const float s = static_cast<float>(j) / links;
    const glm::vec3 p = glm::mix(a, b, s) - kUp * (sag * 4.0f * s * (1.0f - s));
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.05f, alpha * 0.9f, kEmberCore, 2.0f));
  }
}

void LancerFx::spear(glm::vec3 tip, glm::vec3 dir, float length, float alpha, glm::vec3 eye, float pull) {
  if (alpha <= 0.01f) return;
  const glm::vec3 d = glm::length(dir) > 1e-6f ? glm::normalize(dir) : kUp;
  const glm::vec3 p = pulled(tip, eye, pull);
  const float headLen = 0.24f;
  const glm::mat4 shaft = glm::translate(glm::mat4(1.0f), p - d * (headLen + (length - headLen) * 0.5f)) * alignY(d) *
                          glm::scale(glm::mat4(1.0f), {0.034f, length - headLen, 0.034f});
  shafts_.push_back({shaft, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(kWood * alpha, 1.0f)});
  const glm::mat4 head = glm::translate(glm::mat4(1.0f), p - d * (headLen * 0.5f)) * alignY(d) * glm::scale(glm::mat4(1.0f), glm::vec3(headLen * 0.5f));
  heads_.push_back({head, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(kSteelLit * alpha, 1.0f)});
}

void LancerFx::debris(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float size, glm::vec3 colour, float age, glm::vec3 eye, uint32_t salt) {
  if (age < 0.0f || age > 1.6f) return;
  const float ground = at.y;
  for (int i = 0; i < count; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + salt;
    const float a = hash01(e.seed, n, 1) * kTau;
    const glm::vec3 radial{std::cos(a), 0.0f, std::sin(a)};
    const glm::vec3 h = glm::normalize(bias + radial * 0.9f);
    const glm::vec3 vh = h * speed * (0.6f + 0.9f * hash01(e.seed, n, 3));
    const float vy = speed * (0.9f + 0.9f * hash01(e.seed, n, 4));
    const float t1 = 2.0f * vy / kGravity, vy2 = vy * 0.28f, t2 = 2.0f * vy2 / kGravity;
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
    rocks_.push_back({m, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(colour * (0.8f + 0.4f * hash01(e.seed, n, 9)), 1.0f)});
  }
}

void LancerFx::sparks(const Effect& e, glm::vec3 at, glm::vec3 bias, int count, float speed, float k, float age, glm::vec3 eye, uint32_t salt, glm::vec3 warm) {
  if (age < 0.0f) return;
  const glm::vec3 side = glm::normalize(glm::cross(kUp, flatDir(bias, glm::vec3(0.0f, 0.0f, 1.0f))));
  for (int i = 0; i < count; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + salt;
    const float life = 0.16f + 0.24f * hash01(e.seed, n, 5);
    const float tau = age - hash01(e.seed, n, 6) * 0.04f;
    if (tau < 0.0f || tau > life) continue;
    const glm::vec3 d = glm::normalize(bias * (0.3f + 0.6f * hash01(e.seed, n, 1)) + side * (hash01(e.seed, n, 2) - 0.5f) * 2.2f + kUp * (0.1f + 0.8f * hash01(e.seed, n, 3)));
    const glm::vec3 v0 = d * speed * (0.6f + 0.8f * hash01(e.seed, n, 4)) * k;
    const glm::vec3 p = at + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
    if (p.y < e.feet.y + 0.02f && p.y < at.y) continue;
    const float u = tau / life;
    streaks_.push_back(streak(pulled(p, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.05f, 0.032f * k, 1.0f - u * u,
                              glm::mix(glm::vec3(1.0f, 0.95f, 0.8f), warm, u), 3.2f));
  }
}

void LancerFx::dustRing(const Effect& e, glm::vec3 at, float k, int count, float reach, float born, float age, glm::vec3 eye, uint32_t salt) {
  for (int i = 0; i < count; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + salt;
    const float life = 1.1f * (0.85f + 0.3f * hash01(e.seed, n, 1));
    const float tau = age - born - hash01(e.seed, n, 5) * 0.1f;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float a = (static_cast<float>(i) + 0.4f * hash01(e.seed, n, 2)) / static_cast<float>(count) * kTau;
    const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
    const glm::vec3 p = at + out * ((0.25f + reach * easeOut(0.0f, 0.7f, u)) * k) + kUp * ((0.08f + 0.22f * u) * k);
    const float alpha = 0.45f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.25f, 1.0f, u));
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.3f + 0.6f * easeOut(0.0f, 1.0f, u)) * k, a + tau, hash01(e.seed, n, 3) * 37.0f, alpha,
                          kDust * (0.85f + 0.3f * hash01(e.seed, n, 4))));
  }
}

// --- Arremesso --------------------------------------------------------------------------------------------------------------

void LancerFx::buildToss(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const glm::vec3 hand = e.from + kUp * (e.height * 0.45f) + e.dir * 0.3f;
  const glm::vec3 low = e.feet + kUp * (0.18f * k) - e.dir * 0.1f;
  // --- The lance goes in low, under the body. ---
  thrustSheet(hand, low, side, 0.08f, 0.14f * k, 1.3f, kSteel, now - e.start, 0.1f, eye, e.feet, frame);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  // --- The lift: a flash at the feet, a streak straight up, dust bursting out, a warm light. ---
  if (age < 0.25f) {
    const float f = 1.0f - ease(0.0f, 0.25f, age);
    streaks_.push_back(streak(pulled(low, eye, kPull), glm::vec3(0.0f), 0.45f * k * (0.5f + 0.5f * f), f, kGold, 2.0f));
    const float rise = easeOut(0.0f, 0.2f, age);
    streaks_.push_back(streak(pulled(low + kUp * (0.5f * e.height * rise), eye, kPull), kUp * (0.9f * k * rise), 0.045f * k, f, kSteel, 2.6f));
  }
  dustRing(e, e.feet, k, 14, 1.0f, 0.0f, age, eye, 200u);
  sparks(e, low, e.dir * 0.3f + kUp * 0.6f, 10, 3.0f, k, age, eye, 100u, {1.0f, 0.6f, 0.2f});
  if (age < 0.3f) lights.push_back({low + kUp * 0.3f, {1.0f, 0.8f, 0.55f}, 9.0f * (1.0f - ease(0.0f, 0.3f, age)), 2.8f * k});
}

void LancerFx::buildTossFlight(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const float s = ease(e.start, e.finish, now);
  auto at = [&](float u) { return arcAt(e.feet, e.to, e.arc, u) + kUp * (e.height * 0.5f); };
  // --- The flight: a faint wake of air behind the body, a few streaks shed along it. ---
  if (now < e.finish) {
    const float tail = std::min(s, 0.3f);
    if (tail > 0.02f) {
      three::CatmullRom path;
      for (int j = 0; j <= 8; ++j) path.points.push_back(pulled(at(s - tail + tail * static_cast<float>(j) / 8.0f), eye, kPull));
      const std::vector<VfxVertex> tube = vfxgeo::tube(path, 16, 0.12f * k, 8);
      draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, now - e.start, 0.5f, 0.35f, {0.8f, 0.85f, 0.95f}, 0.0f,
           glm::vec4(kWhite, 0.0f), e.feet, VfxBlend::Additive);
    }
  }
  const float flight = e.finish - e.start;
  for (int i = 0; i < 10; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 100u;
    const float ub = (static_cast<float>(i) + 0.5f) / 10.0f;
    const float tau = now - (e.start + ub * flight), life = 0.25f + 0.2f * hash01(e.seed, n, 1);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 p = at(ub) + sphereDir(hash01(e.seed, n, 2), hash01(e.seed, n, 3)) * 0.5f * tau;
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.04f * k * (1.0f - 0.5f * u), (1.0f - u) * 0.6f, {0.85f, 0.88f, 0.95f}, 1.6f));
  }
  // --- The landing: a crater (a shell, a ground ring, dust rolling out, chips of ground); against a body a flash and sparks. ---
  const float age = now - e.finish;
  if (age < 0.0f) return;
  const glm::vec3 land = e.to;
  if (age < 0.12f) {
    const float f = 1.0f - ease(0.0f, 0.12f, age);
    streaks_.push_back(streak(pulled(land + kUp * 0.25f, eye, kGroundPull), glm::vec3(0.0f), 0.6f * k * (0.6f + 0.4f * f), f, {1.0f, 0.88f, 0.7f}, 2.0f));
  }
  if (age < 0.25f) {
    const float r = (0.2f + 1.2f * easeOut(0.0f, 0.22f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), land + kUp * 0.25f) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.35f, 0.35f * (1.0f - ease(0.05f, 0.25f, age)), {1.0f, 0.85f, 0.65f}, 0.04f, glm::vec4(kWhite, 0.0f), land,
         VfxBlend::Additive);
  }
  if (age < 0.5f) {
    const float r = (0.3f + 1.7f * easeOut(0.0f, 0.4f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(land + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.3f, 0.6f * (1.0f - ease(0.1f, 0.5f, age)), {0.95f, 0.72f, 0.48f}, 0.0f, glm::vec4(1.0f, 0.86f, 0.66f, 0.88f), land,
         VfxBlend::Additive);
  }
  dustRing(e, land, k, 16, 1.6f, 0.0f, age, eye, 200u);
  debris(e, land + kUp * 0.05f, kUp * 0.3f, 12, 2.4f * k, 0.05f * k, kGround, age, eye, 300u);
  if (age < 0.3f) lights.push_back({land + kUp * 0.5f, {1.0f, 0.8f, 0.55f}, 14.0f * (1.0f - ease(0.0f, 0.3f, age)), 3.5f * k});
  if (e.collided) {
    // the body it hit stands one hex on, in the flight's direction
    const glm::vec3 chest = land + e.dir * (kHex * 0.5f) + kUp * (e.height * 0.5f);
    if (age < 0.15f) {
      const float f = 1.0f - ease(0.0f, 0.15f, age);
      streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.7f * k * (0.6f + 0.4f * f), f, {1.0f, 0.75f, 0.45f}, 2.4f));
      streaks_.push_back(streak(pulled(chest, eye, kPull), glm::cross(kUp, e.dir) * 1.0f * k * f, 0.05f * k, f, kWhite, 2.6f));
    }
    sparks(e, chest, e.dir * 0.5f + kUp * 0.3f, 14, 3.4f, k, age, eye, 400u, {1.0f, 0.55f, 0.18f});
    if (age < 0.25f) lights.push_back({chest, {1.0f, 0.7f, 0.4f}, 12.0f * (1.0f - ease(0.0f, 0.25f, age)), 3.0f * k});
  }
}

// --- Gancho -----------------------------------------------------------------------------------------------------------------

void LancerFx::buildHook(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 hand = e.from + kUp * (e.height2 * 0.5f) + e.dir * 0.3f;
  const glm::vec3 chest = e.feet + kUp * (e.height * 0.5f) - e.dir * 0.15f;
  const float s = ease(e.start, e.impact, now);
  const float dist = glm::length(chest - hand);
  // --- The hook flies: a lit steel point on an ember chain unrolling behind it. ---
  if (now < e.impact) {
    const glm::vec3 tip = glm::mix(hand, chest, s) + kUp * (dist * 0.05f * 4.0f * s * (1.0f - s));
    spear(tip, chest - hand, 0.3f, 1.0f, eye, kPull);
    chain(hand, tip, 0.15f * s, 0.9f, now - e.start, eye, e.feet, frame);
    streaks_.push_back(streak(pulled(tip, eye, kPull), glm::vec3(0.0f), 0.12f * k, 0.8f, kEmberCore, 2.0f));
    lights.push_back({tip, {1.0f, 0.6f, 0.3f}, 5.0f, 2.5f * k});
    return;
  }
  // --- The bite: a flash, sparks, the chain taut (fading away unless the pull takes it over). ---
  const float age = now - e.impact;
  const float keep = 1.0f - ease(e.finish - e.impact - 0.3f, e.finish - e.impact, age);
  chain(hand, chest, 0.05f, 0.9f * keep, now - e.start, eye, e.feet, frame);
  spear(chest, chest - hand, 0.3f, keep, eye, kPull);
  if (age < 0.14f) {
    const float f = 1.0f - ease(0.0f, 0.14f, age);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.5f * k * (0.6f + 0.4f * f), f, kEmberCore, 2.2f));
  }
  sparks(e, chest, -e.dir * 0.4f + kUp * 0.3f, 10, 2.8f, k, age, eye, 100u, kEmber);
  if (age < 0.3f) lights.push_back({chest, {1.0f, 0.6f, 0.3f}, 10.0f * (1.0f - ease(0.0f, 0.3f, age)), 3.0f * k});
}

void LancerFx::buildHookPull(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 hand = e.from + kUp * (e.height2 * 0.5f) + flatDir(e.feet - e.from, e.dir) * 0.3f;
  const float s = ease(e.start, e.finish, now);
  const glm::vec3 body = glm::mix(e.feet, e.to, s);
  const glm::vec3 chest = body + kUp * (e.height * 0.42f);
  const float age = now - e.finish;
  const float keep = 1.0f - ease(0.0f, 0.4f, age);
  // --- The chain stays taut to the body as it is dragged in; the hook in its chest. ---
  chain(hand, chest, 0.03f, 0.95f * keep, now - e.start, eye, e.feet, frame);
  spear(chest, chest - hand, 0.3f, keep, eye, kPull);
  lights.push_back({chest, {1.0f, 0.6f, 0.3f}, 6.0f * keep, 2.8f * k});
  // --- Dust rolling up behind the body along the way. ---
  const float flight = e.finish - e.start;
  for (int i = 0; i < 18; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float ub = (static_cast<float>(i) + 0.5f) / 18.0f;
    const float tau = now - (e.start + ub * flight), life = 0.7f + 0.3f * hash01(e.seed, n, 1);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 side = glm::cross(kUp, e.dir);
    const glm::vec3 p = glm::mix(e.feet, e.to, ub) + side * ((hash01(e.seed, n, 2) - 0.5f) * 0.6f * k) + kUp * ((0.1f + 0.35f * easeOut(0.0f, 1.0f, u)) * k);
    const float alpha = 0.4f * ease(0.0f, 0.05f, tau) * (1.0f - ease(0.2f, 1.0f, u));
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.25f + 0.5f * easeOut(0.0f, 1.0f, u)) * k, tau + ub * 7.0f, hash01(e.seed, n, 3) * 37.0f, alpha,
                          kDust * (0.85f + 0.3f * hash01(e.seed, n, 4))));
  }
  // --- The end of the pull: a burst of dust at the feet, the chain goes out. ---
  if (age >= 0.0f) {
    dustRing(e, e.to, k, 10, 0.8f, 0.0f, age, eye, 300u);
    if (age < 0.12f) {
      const float f = 1.0f - ease(0.0f, 0.12f, age);
      streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.35f * k * f, f, kEmberCore, 2.0f));
    }
  }
}

// --- Salto de Vara ----------------------------------------------------------------------------------------------------------

void LancerFx::buildVault(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const float s = ease(e.start, e.impact, now);
  const glm::vec3 plant = e.from - e.dir * 0.55f;  // the lance planted on the near side of the foe
  // --- The pole: planted, leaning back at the take-off, carried over to lean forward as the vault passes; then it is
  // pulled out (fades). ---
  const float age = now - e.impact;
  const float keep = 1.0f - ease(0.3f, 0.7f, age);
  if (keep > 0.0f) {
    const float lean = glm::mix(-0.55f, 0.6f, s);
    const glm::vec3 poleDir = glm::normalize(kUp * std::cos(lean) + e.dir * std::sin(lean));
    const float length = 2.2f * k;
    spear(plant + poleDir * length, poleDir, length, keep, eye, kGroundPull);
    if (s > 0.02f && s < 0.98f) {
      const glm::vec3 top = plant + poleDir * length;
      streaks_.push_back(streak(pulled(top, eye, kPull), glm::vec3(0.0f), 0.12f * k, 0.7f, kSteel, 1.8f));
    }
  }
  // --- The flight: a trail of steel light over the arc, sparks shed, a light with it. ---
  auto at = [&](float u) { return arcAt(e.feet, e.to, e.arc, u) + kUp * (e.height * 0.5f); };  // the card's own arc (MapStage: 2.0 high)
  if (now < e.impact) {
    const float tail = std::min(s, 0.35f);
    if (tail > 0.01f) {
      three::CatmullRom path;
      for (int j = 0; j <= 8; ++j) path.points.push_back(pulled(at(s - tail + tail * static_cast<float>(j) / 8.0f), eye, kPull));
      const std::vector<VfxVertex> tube = vfxgeo::tube(path, 18, 0.055f * k, 8);
      draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, now - e.start, 1.2f, 0.8f, kSteel, 0.0f, glm::vec4(kWhite, 0.0f),
           e.feet, VfxBlend::Additive);
    }
    const glm::vec3 head = at(s);
    streaks_.push_back(streak(pulled(head, eye, kPull), glm::vec3(0.0f), 0.18f * k, 0.8f, kWhite, 2.0f));
    lights.push_back({head, {0.85f, 0.9f, 1.0f}, 7.0f, 3.0f * k});
  }
  // --- Dust at the plant (the take-off) and at the landing. ---
  dustRing(e, plant, k, 10, 0.7f, 0.0f, now - e.start, eye, 200u);
  if (age < 0.0f) return;
  if (age < 0.12f) {
    const float f = 1.0f - ease(0.0f, 0.12f, age);
    streaks_.push_back(streak(pulled(e.to + kUp * 0.25f, eye, kGroundPull), glm::vec3(0.0f), 0.5f * k * (0.6f + 0.4f * f), f, {1.0f, 0.88f, 0.7f}, 2.0f));
  }
  if (age < 0.45f) {
    const float r = (0.3f + 1.4f * easeOut(0.0f, 0.4f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.to + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.3f, 0.55f * (1.0f - ease(0.1f, 0.45f, age)), {0.9f, 0.75f, 0.55f}, 0.0f, glm::vec4(1.0f, 0.9f, 0.7f, 0.88f), e.to,
         VfxBlend::Additive);
  }
  dustRing(e, e.to, k, 14, 1.3f, 0.0f, age, eye, 300u);
  debris(e, e.to + kUp * 0.05f, kUp * 0.3f, 8, 2.0f * k, 0.045f * k, kGround, age, eye, 400u);
  if (age < 0.3f) lights.push_back({e.to + kUp * 0.5f, {1.0f, 0.85f, 0.6f}, 12.0f * (1.0f - ease(0.0f, 0.3f, age)), 3.5f * k});
}

// --- The thrusts: the vault's blow from behind, the swap's blow, Desarmar, Empalar, the vigil's interrupt ---------------------

void LancerFx::buildThrustBlow(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const float handH = e.kind == Kind::Disarm ? 0.42f : 0.5f;
  const glm::vec3 hand = e.from + kUp * (e.height * 0.5f) + e.dir * 0.3f;
  const glm::vec3 contact = e.feet + kUp * (e.height * handH) - e.dir * 0.15f;
  const bool vigil = e.kind == Kind::VigilStrike, impale = e.kind == Kind::Impale, vault = e.kind == Kind::VaultBlow;
  const glm::vec3 colour = vigil ? kEmber : kSteel;
  const glm::vec3 warm = vigil ? kEmber : glm::vec3(1.0f, 0.55f, 0.18f);
  // --- The lance goes out: a sheet from the hand to the contact, bowed a little to the side. ---
  thrustSheet(hand, contact, side, vigil ? 0.05f : 0.1f, (vigil ? 0.2f : 0.15f) * k, vigil ? 1.9f : 1.4f, colour, now - e.start, vigil ? 0.06f : 0.1f, eye, e.feet,
              frame);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  // --- The contact: a flash, a cross glint, sparks the lance's way (and back on a thrust that goes in deep). ---
  if (age < 0.14f) {
    const float f = 1.0f - ease(0.0f, 0.14f, age);
    streaks_.push_back(streak(pulled(contact, eye, kPull), glm::vec3(0.0f), (vigil || vault ? 0.65f : 0.5f) * k * (0.6f + 0.4f * f), f, vigil ? kEmberCore : kGold, 2.2f));
    streaks_.push_back(streak(pulled(contact, eye, kPull), side * (e.kind == Kind::Disarm ? 1.3f : 1.0f) * k * f, 0.045f * k, f, kWhite, 2.6f));
    if (e.kind == Kind::Disarm) streaks_.push_back(streak(pulled(contact, eye, kPull), kUp * 1.0f * k * f, 0.04f * k, f, kWhite, 2.6f));
  }
  sparks(e, contact, e.dir * 0.6f + kUp * 0.3f, vigil ? 18 : 12, 3.2f, k, age, eye, 100u, warm);
  if (impale) sparks(e, contact, -e.dir * 0.3f + kUp * 0.4f, 8, 2.6f, k, age, eye, 150u, warm);
  if (vigil && age < 0.3f) {
    // an ember shell bursting from the chest
    const float r = (0.2f + 0.9f * easeOut(0.0f, 0.28f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(contact, eye, kPull)) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.5f, 0.5f * (1.0f - ease(0.06f, 0.3f, age)), kEmber, 0.05f, glm::vec4(kEmberCore, 0.0f), e.feet,
         VfxBlend::Additive);
  }
  if (vault && age < 0.35f) {  // the blow from behind: a crack ring at the feet
    const float r = (0.3f + 0.8f * easeOut(0.0f, 0.3f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.4f, 0.6f * (1.0f - ease(0.1f, 0.35f, age)), {1.0f, 0.6f, 0.35f}, 0.0f, glm::vec4(1.0f, 0.85f, 0.6f, 0.85f), e.feet,
         VfxBlend::Additive);
  }
  if (age < 0.25f) lights.push_back({contact, vigil ? glm::vec3(1.0f, 0.6f, 0.3f) : glm::vec3(0.9f, 0.92f, 1.0f), (vigil ? 16.0f : 11.0f) * (1.0f - ease(0.0f, 0.25f, age)), 3.0f * k});
}

// --- Desarmar: the weapon knocked away --------------------------------------------------------------------------------------

void LancerFx::buildDisarmed(const Effect& e, float now, const Camera& camera, VfxFrame&, std::vector<VfxLight>&) {
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const glm::vec3 hands = e.feet + kUp * (e.height * 0.42f) - e.dir * 0.1f;
  // up, away from the striker and to one side; falls, bounces once, lies still, fades
  const float sideSign = hash01(e.seed, 1u, 1) < 0.5f ? -1.0f : 1.0f;
  const glm::vec3 vh = (e.dir * 0.9f + side * sideSign * 1.1f) * (1.6f * k);
  const float vy = 4.6f * k, ground = e.feet.y;
  const float drop = hands.y - ground;
  const float t1 = (vy + std::sqrt(vy * vy + 2.0f * kGravity * drop)) / kGravity, vy2 = vy * 0.22f, t2 = 2.0f * vy2 / kGravity;
  glm::vec3 p;
  float spin;
  if (age < t1) p = hands + vh * age + kUp * (vy * age - 0.5f * kGravity * age * age), spin = age * 14.0f;
  else if (age < t1 + t2) {
    const float s = age - t1;
    p = hands + vh * t1 + vh * 0.3f * s + kUp * (vy2 * s - 0.5f * kGravity * s * s), spin = t1 * 14.0f + s * 5.0f;
    p.y = std::max(p.y, ground + 0.03f);
  } else {
    p = hands + vh * t1 + vh * 0.3f * t2, spin = t1 * 14.0f + t2 * 5.0f;
    p.y = ground + 0.03f;
  }
  const float fade = 1.0f - ease(2.6f, 3.5f, age);
  if (fade <= 0.0f) return;
  const bool lying = age >= t1 + t2;
  const glm::vec3 axis = glm::normalize(side * 0.7f + e.dir * 0.3f + kUp * 0.1f);
  glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p, eye, lying ? kGroundPull : kPull));
  if (lying) m = m * alignY(glm::normalize(e.dir * 0.8f + side * sideSign * 0.5f));  // flat on the ground, pointing the way it flew
  else m = m * glm::rotate(glm::mat4(1.0f), spin, axis);
  const float len = 0.7f * k;
  blades_.push_back({m * glm::translate(glm::mat4(1.0f), {0.0f, len * 0.35f, 0.0f}) * glm::scale(glm::mat4(1.0f), {k, len, k}), glm::vec4(0.0f), glm::vec4(0.0f),
                     glm::vec4(kSteelLit * fade, 1.0f)});
  guards_.push_back({m * glm::scale(glm::mat4(1.0f), glm::vec3(k)), glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(glm::vec3(0.45f, 0.35f, 0.22f) * fade, 1.0f)});
  // a glint trailing it while it spins
  if (!lying) streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.1f * k, 0.6f * (1.0f - ease(0.0f, t1 + t2, age)), kWhite, 1.8f));
  if (age >= t1 && age < t1 + 0.5f) dustRing(e, glm::vec3(p.x, ground, p.z), k * 0.6f, 6, 0.5f, t1, age, eye, 200u);
}

// --- Troca de Posto -------------------------------------------------------------------------------------------------------

void LancerFx::buildSwap(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float s = ease(e.start, e.finish, now);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  // each takes its own side of the line, so they pass each other
  auto pathA = [&](float u) { return arcAt(e.feet, e.to, e.arc, u) + side * (0.35f * std::sin(kPi * u)) + kUp * (e.height * 0.5f); };
  auto pathB = [&](float u) { return arcAt(e.from, e.to2, e.arc, u) - side * (0.35f * std::sin(kPi * u)) + kUp * (e.height2 * 0.5f); };
  const float kA = std::clamp(e.height / 2.0f, 0.7f, 1.4f), kB = std::clamp(e.height2 / 2.0f, 0.7f, 1.4f);
  if (now < e.finish) {
    const float tail = std::min(s, 0.4f);
    if (tail > 0.01f) {
      for (int who = 0; who < 2; ++who) {
        three::CatmullRom path;
        for (int j = 0; j <= 8; ++j) {
          const float u = s - tail + tail * static_cast<float>(j) / 8.0f;
          path.points.push_back(pulled(who == 0 ? pathA(u) : pathB(u), eye, kPull));
        }
        const std::vector<VfxVertex> tube = vfxgeo::tube(path, 18, 0.05f * (who == 0 ? kA : kB), 8);
        draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, now - e.start, 1.1f, 0.8f, who == 0 ? kSteel : kGold, 0.0f,
             glm::vec4(kWhite, 0.0f), e.feet, VfxBlend::Additive);
      }
    }
    streaks_.push_back(streak(pulled(pathA(s), eye, kPull), glm::vec3(0.0f), 0.14f * kA, 0.8f, kWhite, 1.8f));
    streaks_.push_back(streak(pulled(pathB(s), eye, kPull), glm::vec3(0.0f), 0.14f * kB, 0.8f, kGold, 1.8f));
    lights.push_back({pathA(s), {0.85f, 0.9f, 1.0f}, 5.0f, 2.5f * kA});
    lights.push_back({pathB(s), {1.0f, 0.8f, 0.5f}, 5.0f, 2.5f * kB});
    // where they pass: a burst
    const float mid = e.start + (e.finish - e.start) * 0.5f;
    const float tau = now - mid;
    if (tau >= 0.0f && tau < 0.2f) {
      const float f = 1.0f - ease(0.0f, 0.2f, tau);
      const glm::vec3 c = (pathA(0.5f) + pathB(0.5f)) * 0.5f;
      streaks_.push_back(streak(pulled(c, eye, kPull), glm::vec3(0.0f), 0.6f * f, f, kGold, 2.0f));
    }
  }
  // dust at both take-offs and both landings
  dustRing(e, e.feet, kA, 8, 0.6f, 0.0f, now - e.start, eye, 200u);
  dustRing(e, e.from, kB, 8, 0.6f, 0.0f, now - e.start, eye, 300u);
  const float age = now - e.finish;
  if (age >= 0.0f) {
    dustRing(e, e.to, kA, 10, 0.8f, 0.0f, age, eye, 400u);
    dustRing(e, e.to2, kB, 10, 0.8f, 0.0f, age, eye, 500u);
  }
}

// --- Empalar e Arrastar ---------------------------------------------------------------------------------------------------

void LancerFx::buildDrag(const Effect& e, float now, const Camera& camera, VfxFrame&, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const float s = ease(e.start, e.finish, now);
  const glm::vec3 body = glm::mix(e.feet, e.to, s), lancer = glm::mix(e.from, e.to2, s);
  const glm::vec3 chest = body + kUp * (e.height * 0.45f), hand = lancer + kUp * (e.height2 * 0.5f);
  const float age = now - e.finish;
  // --- The lance in the body, held from behind, the whole way; drawn out at the end. ---
  const float held = 1.0f - ease(0.15f, 0.4f, age);
  if (held > 0.0f) {
    const glm::vec3 along = chest - hand;
    spear(chest + glm::normalize(along) * 0.25f, along, glm::length(along) + 0.25f, held, eye, kPull);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.1f * k, 0.6f * held, kSteel, 1.6f));
  }
  // --- Dust and sparks streaming behind the dragged body; dark scrapes left on the ground. ---
  const float flight = e.finish - e.start;
  const glm::vec3 side = glm::cross(kUp, e.dir);
  for (int i = 0; i < 26; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float ub = (static_cast<float>(i) + 0.5f) / 26.0f;
    const float tau = now - (e.start + ub * flight), life = 0.8f + 0.4f * hash01(e.seed, n, 1);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 p = glm::mix(e.feet, e.to, ub) + side * ((hash01(e.seed, n, 2) - 0.5f) * 0.7f * k) + kUp * ((0.1f + 0.45f * easeOut(0.0f, 1.0f, u)) * k);
    const float alpha = 0.45f * ease(0.0f, 0.05f, tau) * (1.0f - ease(0.2f, 1.0f, u));
    puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.3f + 0.6f * easeOut(0.0f, 1.0f, u)) * k, tau + ub * 7.0f, hash01(e.seed, n, 3) * 37.0f, alpha,
                          kDust * (0.85f + 0.3f * hash01(e.seed, n, 4))));
  }
  for (int i = 0; i < 12; ++i) {  // the scrapes: dark smudges lying along the way, there for a while
    const uint32_t n = static_cast<uint32_t>(i) + 400u;
    const float ub = (static_cast<float>(i) + 0.5f) / 12.0f;
    const float tau = now - (e.start + ub * flight);
    if (tau < 0.0f) continue;
    const float alpha = 0.55f * ease(0.0f, 0.05f, tau) * (1.0f - ease(1.4f, 2.3f, tau));
    if (alpha <= 0.0f) continue;
    const glm::vec3 p = glm::mix(e.feet, e.to, ub) + side * ((hash01(e.seed, n, 2) - 0.5f) * 0.35f * k) + kUp * 0.03f;
    puffs_.push_back(puff(pulled(p, eye, kGroundPull * 0.5f), (0.35f + 0.15f * hash01(e.seed, n, 5)) * k, hash01(e.seed, n, 6) * kTau, hash01(e.seed, n, 3) * 37.0f, alpha,
                          kScrape));
  }
  if (now < e.finish) {
    sparks(e, body + kUp * (0.15f * k), -e.dir * 0.6f + kUp * 0.2f, 10, 2.2f, k, std::fmod(now - e.start, 0.3f), eye, 100u + static_cast<uint32_t>((now - e.start) / 0.3f) * 50u,
           {1.0f, 0.55f, 0.18f});
    lights.push_back({chest, {0.9f, 0.9f, 1.0f}, 4.0f, 2.5f * k});
  }
  // --- The jolt at the end: a flash in the body, a burst of dust and chips, the lance pulled out. ---
  if (age < 0.0f) return;
  if (age < 0.15f) {
    const float f = 1.0f - ease(0.0f, 0.15f, age);
    streaks_.push_back(streak(pulled(chest, eye, kPull), glm::vec3(0.0f), 0.55f * k * (0.6f + 0.4f * f), f, kGold, 2.2f));
    streaks_.push_back(streak(pulled(chest, eye, kPull), side * 1.0f * k * f, 0.045f * k, f, kWhite, 2.6f));
  }
  sparks(e, chest, e.dir * 0.7f + kUp * 0.3f, 14, 3.2f, k, age, eye, 300u, {1.0f, 0.55f, 0.18f});
  dustRing(e, e.to, k, 12, 1.0f, 0.0f, age, eye, 500u);
  debris(e, e.to + kUp * 0.05f, e.dir * 0.4f + kUp * 0.3f, 8, 2.2f * k, 0.045f * k, kGround, age, eye, 600u);
  if (age < 0.3f) lights.push_back({chest, {1.0f, 0.85f, 0.6f}, 12.0f * (1.0f - ease(0.0f, 0.3f, age)), 3.2f * k});
}

// --- Vigília da Sentinela -------------------------------------------------------------------------------------------------

void LancerFx::buildVigilRise(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 point = e.feet + kUp * (e.height * 0.7f);
  const float age = now - e.impact;
  if (age < 0.0f) return;
  // --- The lance point flares; an ember ring runs out to the radius the stance covers. ---
  if (age < 0.3f) {
    const float f = 1.0f - ease(0.0f, 0.3f, age);
    streaks_.push_back(streak(pulled(point, eye, kPull), glm::vec3(0.0f), 0.7f * k * (0.5f + 0.5f * f), f, kEmberCore, 2.4f));
    streaks_.push_back(streak(pulled(point, eye, kPull), kUp * 1.2f * k * f, 0.05f * k, f, kEmberCore, 2.6f));
  }
  if (age < 0.7f) {
    const float r = 0.3f * k + (e.radius - 0.3f * k) * easeOut(0.0f, 0.55f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.45f, 0.7f * (1.0f - ease(0.2f, 0.7f, age)), kEmber, 0.0f, glm::vec4(kEmberCore, 0.9f), e.feet, VfxBlend::Additive);
  }
  // embers lifted off the ground as the ring passes
  for (int i = 0; i < 20; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 200u;
    const float ri = (0.3f + 0.7f * hash01(e.seed, n, 1)) * e.radius;
    const float born = ri / e.radius * 0.55f;
    const float life = 0.9f * (0.8f + 0.4f * hash01(e.seed, n, 2));
    const float tau = age - born;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float a = hash01(e.seed, n, 3) * kTau;
    const glm::vec3 p = e.feet + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * ri + kUp * (0.05f + 0.9f * easeOut(0.0f, 1.0f, u));
    streaks_.push_back(streak(pulled(p, eye, kGroundPull), kUp * 0.08f, 0.04f, (1.0f - u) * 0.9f, kEmber, 2.4f));
  }
  if (age < 0.6f) lights.push_back({point, {1.0f, 0.6f, 0.3f}, 14.0f * (1.0f - ease(0.0f, 0.6f, age)), 4.0f * k});
}

void LancerFx::buildVigil(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const float age = now - e.start;
  const float env = ease(0.0f, 0.5f, age) * (e.finish < 0.0f ? 1.0f : 1.0f - ease(e.finish, e.finish + 0.6f, now));
  if (env <= 0.0f) return;
  const glm::vec3 point = e.feet + kUp * (e.height * 0.7f);
  // --- A slow ember pulse at the feet, a faint ring at the covered radius turning. ---
  const float pulse = 0.5f + 0.5f * std::sin(age * 2.4f);
  {
    const float r = (0.55f + 0.1f * pulse) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.35f, (0.35f + 0.2f * pulse) * env, kEmber, 0.0f, glm::vec4(kEmberCore, 0.7f), e.feet, VfxBlend::Additive);
  }
  {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.03f, 0.0f), eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), age * 0.4f, kUp) *
                        glm::scale(glm::mat4(1.0f), {e.radius, 1.0f, e.radius});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.2f, 0.16f * env, kEmber, 0.0f, glm::vec4(kEmberCore, 0.95f), e.feet, VfxBlend::Additive);
  }
  // --- Motes rising round the body; the lance point aglow. ---
  for (int j = 0; j < 10; ++j) {
    const uint32_t n = static_cast<uint32_t>(j) + 300u;
    const float phase = std::fmod(hash01(e.seed, n, 1) + age * 0.45f, 1.0f);
    const float ang = hash01(e.seed, n, 2) * kTau + age * 1.2f;
    const float r = (0.25f + 0.3f * hash01(e.seed, n, 3)) * k;
    const glm::vec3 p = e.feet + glm::vec3(std::cos(ang) * r, phase * e.height * 1.1f, std::sin(ang) * r);
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.045f * k, std::sin(kPi * phase) * env * 0.9f, kEmber, 2.2f));
  }
  streaks_.push_back(streak(pulled(point, eye, kPull), glm::vec3(0.0f), (0.16f + 0.05f * pulse) * k, env * 0.85f, kEmberCore, 2.0f));
  lights.push_back({point, {1.0f, 0.6f, 0.3f}, (2.6f + 0.8f * pulse) * env, 3.0f * k});
}

// --- Investida Perfurante ----------------------------------------------------------------------------------------------------

void LancerFx::buildPierce(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const float speed = e.radius;
  const float reach = std::max(glm::dot(glm::vec3(e.to.x - e.from.x, 0.0f, e.to.z - e.from.z), e.dir), 0.3f) + 0.6f;
  // A point `d` out along the line at the lancer's chest height, the ground's rise followed from his hex to the last.
  auto axis = [&](float d) {
    glm::vec3 p = e.from + e.dir * d;
    p.y = glm::mix(e.from.y, e.to.y, std::clamp(d / reach, 0.0f, 1.0f)) + e.height * 0.5f;
    return p;
  };
  const float run = now - e.start;
  const float out = std::min(reach, speed * std::max(run, 0.0f));  // how far the point has gone
  const glm::vec3 hand = axis(0.3f), head = axis(std::max(out, 0.3f));
  const float gone = 1.0f - ease(e.finish, e.finish + 0.25f, now);
  // --- The lance's sheet of steel light down the whole line, a white-hot spike along its axis, the glowing point. ---
  thrustSheet(hand, axis(reach), side, 0.04f, 0.22f * k, 1.5f, kSteel, run, e.finish - e.start, eye, e.feet, frame);
  if (gone > 0.0f && out > 0.35f) {
    streaks_.push_back(streak(pulled((hand + head) * 0.5f, eye, kPull), head - hand, 0.05f * k, 0.9f * gone, kWhite, 2.6f));
    if (now < e.finish) streaks_.push_back(streak(pulled(head, eye, kPull), glm::vec3(0.0f), 0.26f * k, 0.9f, kWhite, 2.2f));
    lights.push_back({head, {0.85f, 0.9f, 1.0f}, 9.0f * gone, 3.0f * k});
  }
  // --- The air spiralling round the shaft behind the point. ---
  constexpr int kTwists = 30;
  for (int i = 0; i < kTwists; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 500u;
    const float d = 0.3f + (reach - 0.3f) * (static_cast<float>(i) + 0.5f) / kTwists;
    const float tau = now - (e.start + d / speed), life = 0.32f + 0.12f * hash01(e.seed, n, 1);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const float a = static_cast<float>(i) * 0.9f + tau * 9.0f;
    const glm::vec3 p = axis(d) + (side * std::cos(a) + kUp * std::sin(a)) * ((0.16f + 0.45f * u) * k);
    streaks_.push_back(streak(pulled(p, eye, kPull), e.dir * (0.2f * k * (1.0f - u)), 0.035f * k, 0.7f * (1.0f - u), (i & 1) ? kSteel : kWhite, 1.8f));
  }
  // --- Every body it runs through, in order as the point reaches it. ---
  const glm::vec3 warm{1.0f, 0.55f, 0.18f};
  for (size_t j = 0; j < e.bodies.size(); ++j) {
    const Target& b = e.bodies[j];
    const float d = glm::dot(glm::vec3(b.feet.x - e.from.x, 0.0f, b.feet.z - e.from.z), e.dir);
    const float age = now - (e.start + std::max(d - 0.3f, 0.3f) / speed);
    if (age < 0.0f) continue;
    const float h = bodyHeight(b.height), kb = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const glm::vec3 contact = b.feet + kUp * (h * 0.5f) - e.dir * 0.1f;
    if (age < 0.14f) {  // the puncture: a flash with a cross glint
      const float f = 1.0f - ease(0.0f, 0.14f, age);
      streaks_.push_back(streak(pulled(contact, eye, kPull), glm::vec3(0.0f), 0.55f * kb * (0.6f + 0.4f * f), f, kGold, 2.2f));
      streaks_.push_back(streak(pulled(contact, eye, kPull), side * 1.0f * kb * f, 0.045f * kb, f, kWhite, 2.6f));
      streaks_.push_back(streak(pulled(contact, eye, kPull), kUp * 0.9f * kb * f, 0.04f * kb, f, kWhite, 2.6f));
    }
    if (age < 0.35f) {  // a ring of pressure round the shaft, square to the line
      const float r = (0.15f + 0.75f * easeOut(0.0f, 0.3f, age)) * kb;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(contact, eye, kPull)) * alignY(e.dir) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      draw(discFirst_, discCount_, m, kModeRing, age, 0.45f, 0.7f * (1.0f - ease(0.08f, 0.35f, age)), kSteel, 0.0f, glm::vec4(kWhite, 0.85f), b.feet,
           VfxBlend::Additive);
    }
    sparks(e, contact + e.dir * 0.25f, e.dir * 0.9f + kUp * 0.2f, 14, 3.4f, kb, age, eye, 100u + 40u * static_cast<uint32_t>(j), warm);
    if (age < 0.22f) lights.push_back({contact, {0.9f, 0.92f, 1.0f}, 11.0f * (1.0f - ease(0.0f, 0.22f, age)), 3.0f * kb});
  }
  // --- Dust kicked up where the line ends. ---
  dustRing(e, e.to, k * 0.8f, 10, 0.9f, 0.0f, now - e.finish, eye, 400u);
}

}  // namespace ember
