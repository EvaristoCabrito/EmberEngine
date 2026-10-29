#include "render/vfx/DaggerFx.h"

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
constexpr float kPull = 1.3f, kGroundPull = 0.7f;  // as CombatFx: in front of the card, which is drawn 1.2 units nearer
constexpr float kModeStreak = 0.0f, kModeRing = 3.0f;
constexpr float kAppear = 0.35f, kDissolve = 0.45f;  // seconds a dagger takes to materialize / to dissolve
constexpr float kBury = 0.22f;                       // seconds a thrown dagger stays in the body before it dissolves
constexpr float kOrbitSpeed = 1.5f;                  // radians a second
constexpr glm::vec3 kViolet{0.62f, 0.4f, 1.0f}, kVioletCore{0.9f, 0.82f, 1.0f}, kWhite{1.0f, 0.98f, 0.94f};
constexpr glm::vec3 kSteelLit{0.62f, 0.65f, 0.7f}, kBronze{0.55f, 0.4f, 0.2f}, kGrip{0.15f, 0.11f, 0.09f}, kSpark{1.0f, 0.88f, 0.6f};

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
// A point round a body: on the eye's side it is pulled over the card, on the far side it stays behind it.
glm::vec3 wrapped(glm::vec3 p, glm::vec3 feet, glm::vec3 eye) {
  glm::vec3 off = p - feet, toEye = eye - feet;
  off.y = 0.0f, toEye.y = 0.0f;
  const float r = glm::length(off), te = glm::length(toEye);
  const float facing = r > 1e-4f && te > 1e-4f ? glm::dot(off, toEye) / (r * te) : 1.0f;
  return pulled(p, eye, kPull * ease(-0.3f, 0.3f, facing));
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
float sizeOf(float h) { return std::clamp(h / 2.0f, 0.7f, 1.4f); }

const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, -0.5f, 0.0f}, 1.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f},
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f}, {{-0.5f, 0.5f, 0.0f}, 0.0f, {0, 0, 1}, 1.0f}};
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
std::vector<VfxVertex> flatShaded(std::vector<VfxVertex> r, glm::vec3 squash) {
  for (VfxVertex& v : r) v.position *= squash;
  for (size_t i = 0; i + 2 < r.size(); i += 3) {
    const glm::vec3 n = glm::normalize(glm::cross(r[i + 1].position - r[i].position, r[i + 2].position - r[i].position));
    r[i].normal = r[i + 1].normal = r[i + 2].normal = n;
  }
  return r;
}
// The dagger (its guard at the origin, the point up +y, sizes for k = 1): the blade a thin box from the guard up, its point a
// squashed octahedron, the guard a short bar, the grip a dark stick, the pommel a bead.
const std::vector<VfxVertex>& bladeGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(0.05f, 0.24f, 0.011f);
  return g;
}
const std::vector<VfxVertex>& tipGeometry() {
  static const std::vector<VfxVertex> g = flatShaded(vfxgeo::octahedron(1.0f, 0), glm::vec3(0.025f, 0.075f, 0.0055f));
  return g;
}
const std::vector<VfxVertex>& guardGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(0.15f, 0.022f, 0.03f);
  return g;
}
const std::vector<VfxVertex>& gripGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(0.03f, 0.1f, 0.03f);
  return g;
}
const std::vector<VfxVertex>& pommelGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::sphere(0.024f, 10, 7);
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
VfxInstance lit(const glm::mat4& m, glm::vec3 colour) { return {m, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(colour, 1.0f)}; }

}  // namespace

// --- The stage's calls ----------------------------------------------------------------------------------------------------

void DaggerFx::summon(glm::vec3 feet, float height, float impact) {
  bursts_.push_back({feet, bodyHeight(height), impact, 0xda66u + 7919u * ++seedSerial_});
}

int DaggerFx::start(glm::vec3 feet, float height, int count, float now) {
  Cloud c;
  c.id = ++serial_, c.feet = feet, c.height = bodyHeight(height), c.start = now;
  c.seed = 0xda67u + 7919u * ++seedSerial_;
  for (int i = 0; i < count; ++i) {
    Slot s;
    s.born = now + 0.08f * static_cast<float>(i);  // one after another out of the flash
    c.slots.push_back(s);
  }
  clouds_.push_back(std::move(c));
  return serial_;
}
void DaggerFx::move(int id, glm::vec3 feet, float height) {
  for (Cloud& c : clouds_)
    if (c.id == id) c.feet = feet, c.height = bodyHeight(height);
}
int DaggerFx::liveCount(int id, float now) const {
  int n = 0;
  for (const Cloud& c : clouds_)
    if (c.id == id)
      for (const Slot& s : c.slots)
        if (!s.thrown && s.gone < 0.0f) ++n;
  for (const Throw& t : throws_)
    if (t.cloud == id && t.impact > now) ++n;
  return n;
}
void DaggerFx::setCount(int id, int count, float now) {
  for (Cloud& c : clouds_) {
    if (c.id != id) continue;
    int live = liveCount(id, now);
    for (int i = 0; live < count; ++live, ++i) {
      Slot s;
      s.born = now + 0.08f * static_cast<float>(i);
      c.slots.push_back(s);
    }
    for (size_t i = c.slots.size(); live > count && i-- > 0;) {
      Slot& s = c.slots[i];
      if (s.thrown || s.gone >= 0.0f) continue;
      s.gone = now;
      --live;
    }
  }
}
void DaggerFx::end(int id, float now) {
  for (Cloud& c : clouds_) {
    if (c.id != id || c.finish >= 0.0f) continue;
    c.finish = now;
    for (Slot& s : c.slots)
      if (!s.thrown && s.gone < 0.0f) s.gone = now;
  }
}
void DaggerFx::strike(glm::vec3 feet, const Target& foe, float start, float impact) { launch(feet, foe, start, impact, false); }
void DaggerFx::intercept(glm::vec3 feet, const Target& foe, float start, float impact) { launch(feet, foe, start, impact, true); }

void DaggerFx::launch(glm::vec3 feet, const Target& foe, float start, float impact, bool intercept) {
  Throw t;
  t.start = start, t.impact = std::max(impact, start + 0.08f), t.intercept = intercept;
  t.seed = 0xda68u + 7919u * ++seedSerial_;
  const float h = bodyHeight(foe.height), kf = sizeOf(h);
  // The nearest cloud with a dagger standing lends it (the dagger leaves from where it circles); none: one out of the body itself.
  Cloud* best = nullptr;
  Slot* slot = nullptr;
  float bestD = 0.6f;
  for (Cloud& c : clouds_) {
    const float d = glm::length(glm::vec2(c.feet.x - feet.x, c.feet.z - feet.z));
    if (d > bestD) continue;
    for (Slot& s : c.slots)
      if (!s.thrown && s.gone < 0.0f) {
        best = &c, slot = &s, bestD = d;
        break;
      }
  }
  float casterHeight = 1.8f;
  if (best && slot) {
    slot->thrown = true, slot->gone = start;
    t.from = orbitPoint(*best, *slot, start), t.cloud = best->id, casterHeight = best->height;
  } else {
    t.from = feet + kUp * (1.8f * 0.55f);
  }
  const glm::vec3 chest = foe.feet + kUp * (h * 0.5f);
  t.dir = flatDir(chest - (feet + kUp * (h * 0.5f)), glm::vec3(0.0f, 0.0f, 1.0f));
  // A strike ends in the chest (a little short of the body's axis, on the striker's side); an interception meets the shot a
  // step out in front of the caster.
  t.to = intercept ? feet + kUp * (casterHeight * 0.55f) + t.dir * (0.7f * sizeOf(casterHeight)) : chest - t.dir * (0.1f * kf);
  t.toHeight = h;
  throws_.push_back(t);
}

bool DaggerFx::active(float now) const {
  return std::any_of(throws_.begin(), throws_.end(), [&](const Throw& t) { return t.end() > now; }) ||
         std::any_of(bursts_.begin(), bursts_.end(), [&](const Burst& b) { return b.end() > now; });
}
float DaggerFx::latestEnd() const {
  float end = -1.0f;
  for (const Throw& t : throws_) end = std::max(end, t.end());
  for (const Burst& b : bursts_) end = std::max(end, b.end());
  return end;
}

// --- The orbit -------------------------------------------------------------------------------------------------------------

glm::vec3 DaggerFx::orbitPoint(const Cloud& c, const Slot& s, float now) const {
  const float k = sizeOf(c.height);
  const float a = s.angle + now * kOrbitSpeed;
  const float bob = 0.05f * k * std::sin(now * 2.3f + s.angle * 1.7f);
  return c.feet + glm::vec3(std::cos(a) * 0.52f * k, c.height * 0.52f + bob, std::sin(a) * 0.52f * k);
}
glm::vec3 DaggerFx::orbitTangent(const Cloud& c, const Slot& s, float now) const {
  (void)c;
  const float a = s.angle + now * kOrbitSpeed;
  return glm::normalize(glm::vec3(-std::sin(a), 0.0f, std::cos(a)));
}

void DaggerFx::dagger(const glm::mat4& m, float k, float fade, bool behind) {
  const glm::mat4 base = m * glm::scale(glm::mat4(1.0f), glm::vec3(k));
  (behind ? backBlades_ : blades_).push_back(lit(base * glm::translate(glm::mat4(1.0f), {0.0f, 0.125f, 0.0f}), kSteelLit * fade));
  (behind ? backTips_ : tips_).push_back(lit(base * glm::translate(glm::mat4(1.0f), {0.0f, 0.245f, 0.0f}), kSteelLit * fade));
  (behind ? backGuards_ : guards_).push_back(lit(base, kBronze * fade));
  (behind ? backGrips_ : grips_).push_back(lit(base * glm::translate(glm::mat4(1.0f), {0.0f, -0.06f, 0.0f}), kGrip * fade));
  (behind ? backPommels_ : pommels_).push_back(lit(base * glm::translate(glm::mat4(1.0f), {0.0f, -0.122f, 0.0f}), kBronze * fade));
}

void DaggerFx::emit(VfxFrame& frame, float now, glm::vec3 sight, std::vector<VfxInstance>& streaks, std::vector<VfxInstance>* pieces[5],
                    const glm::vec3* behindAt) {
  // The lit pieces, one draw each; then the arcane light over them.
  const uint32_t firsts[5] = {bladeFirst_, tipFirst_, guardFirst_, gripFirst_, pommelFirst_};
  const uint32_t counts[5] = {bladeCount_, tipCount_, guardCount_, gripCount_, pommelCount_};
  const float roughness[5] = {0.28f, 0.28f, 0.45f, 0.9f, 0.45f}, metal[5] = {0.85f, 0.85f, 0.7f, 0.0f, 0.7f};
  for (int i = 0; i < 5; ++i) {
    std::vector<VfxInstance>& list = *pieces[i];
    if (list.empty()) continue;
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), list.begin(), list.end());
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {roughness[i], metal[i], 0.0f, 0.0f};
    d.gpu.params1 = glm::vec4(0.0f);
    d.gpu.color = glm::vec4(1.0f);
    d.gpu.firstVertex = firsts[i];
    d.gpu.vertexCount = counts[i];
    d.gpu.firstInstance = first;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Standard);
    d.gpu.sightAt = sight;
    d.instances = static_cast<uint32_t>(list.size());
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    if (behindAt) d.behindCard = true, d.cardAt = *behindAt;
    frame.draws.push_back(d);
    list.clear();
  }
  if (!streaks.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks.begin(), streaks.end());
    const Draw draw{frame};
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(kVioletCore, 0.0f), sight, VfxBlend::Additive,
         first, static_cast<uint32_t>(streaks.size()));
    if (behindAt) frame.draws.back().behindCard = true, frame.draws.back().cardAt = *behindAt;
    streaks.clear();
  }
}

void DaggerFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  // Slots whose dissolve is over leave; a thrown slot leaves at once (its Throw draws it); an ended cloud with nothing left leaves.
  for (Cloud& c : clouds_)
    std::erase_if(c.slots, [&](const Slot& s) { return s.gone >= 0.0f && (s.thrown ? now >= s.gone : now > s.gone + kDissolve + 0.1f); });
  std::erase_if(clouds_, [&](const Cloud& c) { return c.finish >= 0.0f && c.slots.empty(); });
  std::erase_if(throws_, [&](const Throw& t) { return t.end() <= now; });
  std::erase_if(bursts_, [&](const Burst& b) { return b.end() <= now; });
  if (clouds_.empty() && throws_.empty() && bursts_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  bladeFirst_ = frame.addVertices(bladeGeometry()), bladeCount_ = static_cast<uint32_t>(bladeGeometry().size());
  tipFirst_ = frame.addVertices(tipGeometry()), tipCount_ = static_cast<uint32_t>(tipGeometry().size());
  guardFirst_ = frame.addVertices(guardGeometry()), guardCount_ = static_cast<uint32_t>(guardGeometry().size());
  gripFirst_ = frame.addVertices(gripGeometry()), gripCount_ = static_cast<uint32_t>(gripGeometry().size());
  pommelFirst_ = frame.addVertices(pommelGeometry()), pommelCount_ = static_cast<uint32_t>(pommelGeometry().size());
  streaks_.clear(), blades_.clear(), tips_.clear(), guards_.clear(), grips_.clear(), pommels_.clear();
  backStreaks_.clear(), backBlades_.clear(), backTips_.clear(), backGuards_.clear(), backGrips_.clear(), backPommels_.clear();
  glm::vec3 sight = !clouds_.empty() ? clouds_.front().feet : !throws_.empty() ? throws_.front().to : bursts_.front().feet;
  for (Cloud& c : clouds_) buildCloud(c, now, camera, frame, lights);
  for (const Throw& t : throws_)
    if (now >= t.start) buildThrow(t, now, camera, frame, lights);
  for (const Burst& b : bursts_)
    if (now >= b.impact - 0.5f) buildBurst(b, now, camera, frame, lights);
  std::vector<VfxInstance>* pieces[5] = {&blades_, &tips_, &guards_, &grips_, &pommels_};
  emit(frame, now, sight, streaks_, pieces, nullptr);
}

// --- The cloud ------------------------------------------------------------------------------------------------------------

void DaggerFx::buildCloud(Cloud& c, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = sizeOf(c.height);
  const float dt = c.lastNow < 0.0f ? 0.0f : std::clamp(now - c.lastNow, 0.0f, 0.1f);
  c.lastNow = now;
  // The standing daggers share the orbit evenly, in their order; a newcomer takes its place at once, the others ease over.
  std::vector<Slot*> standing;
  for (Slot& s : c.slots)
    if (!s.thrown && s.gone < 0.0f) standing.push_back(&s);
  const float base = hash01(c.seed, 0, 1) * kTau;
  for (size_t i = 0; i < standing.size(); ++i) {
    const float target = base + static_cast<float>(i) * kTau / static_cast<float>(standing.size());
    Slot& s = *standing[i];
    if (!s.placed) s.angle = target, s.placed = true;
    else {
      float d = std::fmod(target - s.angle + kPi, kTau);
      if (d < 0.0f) d += kTau;
      s.angle += (d - kPi) * std::min(1.0f, dt * 4.0f);
    }
  }
  const float env = c.finish < 0.0f ? 1.0f : 1.0f - ease(c.finish, c.finish + kDissolve, now);
  const float pulse = 0.5f + 0.5f * std::sin(now * 2.6f + base);
  int drawn = 0;
  for (const Slot& s : c.slots) {
    if (s.thrown) continue;
    const float age = now - s.born;
    if (age < 0.0f) continue;
    const float in = easeOut(0.0f, kAppear, age);
    const float out = s.gone < 0.0f ? 0.0f : ease(s.gone, s.gone + kDissolve, now);
    const float life = in * (1.0f - out);
    if (life <= 0.0f) continue;
    ++drawn;
    const glm::vec3 p = orbitPoint(c, s, now), tangent = orbitTangent(c, s, now);
    const glm::vec3 wp = wrapped(p, c.feet, eye);
    // The orbit's far half (away from the eye: the top of the circle on screen) goes behind the body's card; the near half
    // stays over it (the user, 2026-10-22).
    const bool behind = glm::dot(flatDir(p - c.feet, glm::vec3(0.0f)), flatDir(eye - c.feet, glm::vec3(0.0f))) < 0.0f;
    std::vector<VfxInstance>& glow = behind ? backStreaks_ : streaks_;
    // The dagger: point along the orbit, nosed a little outward and up, rolling slowly on its axis; it grows out of its flash
    // as it materializes and shrinks as it dissolves.
    const glm::vec3 outward = flatDir(p - c.feet, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 axis = glm::normalize(tangent + outward * 0.22f + kUp * 0.12f);
    const float roll = now * 1.9f + s.angle * 3.0f;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), wp) * alignY(axis) * glm::rotate(glm::mat4(1.0f), roll, kUp) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(0.3f + 0.7f * life));
    dagger(m, k, life, behind);
    // the violet glint on the point and the wake behind the blade
    const glm::vec3 point = wp + axis * (0.31f * k * (0.3f + 0.7f * life));
    glow.push_back(streak(point, glm::vec3(0.0f), (0.06f + 0.02f * pulse) * k, 0.55f * life, kVioletCore, 2.4f));
    glow.push_back(streak(wp - tangent * (0.16f * k), -tangent * (0.3f * k), 0.028f * k, 0.3f * life, kViolet, 2.0f));
    // materializing: a flash; dissolving: motes scattering
    if (age < kAppear) {
      const float f = 1.0f - ease(0.0f, kAppear, age);
      glow.push_back(streak(wp, glm::vec3(0.0f), 0.34f * k * (0.5f + 0.5f * f), 0.9f * f, kVioletCore, 2.6f));
    }
    if (s.gone >= 0.0f) {
      const float t = now - s.gone;
      for (int j = 0; j < 7; ++j) {
        const uint32_t n = static_cast<uint32_t>(j) + 40u;
        const glm::vec3 d = sphereDir(hash01(c.seed, n, 1), hash01(c.seed, n, 2));
        glow.push_back(streak(wp + d * (t * 1.3f * k), glm::vec3(0.0f), 0.04f * k, 0.7f * (1.0f - ease(0.0f, kDissolve, t)), kViolet, 2.2f));
      }
    }
  }
  // This cloud's far half: among the cards, right before its body's card.
  {
    std::vector<VfxInstance>* back[5] = {&backBlades_, &backTips_, &backGuards_, &backGrips_, &backPommels_};
    emit(frame, now, c.feet, backStreaks_, back, &c.feet);
  }
  if (drawn == 0 && env <= 0.0f) return;
  // The ring at the feet, turning; the light on the body.
  {
    const float r = (0.5f + 0.04f * pulse) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(c.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) *
                        glm::rotate(glm::mat4(1.0f), now * 0.6f, kUp) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, now, 0.4f, (0.2f + 0.1f * pulse) * env * std::min(1.0f, static_cast<float>(drawn) / 3.0f), kViolet, 0.0f,
         glm::vec4(kVioletCore, 0.8f), c.feet, VfxBlend::Additive);
    // Under the feet: drawn right before the body's card, so the figure covers it (the user, 2026-10-29).
    frame.draws.back().behindCard = true, frame.draws.back().cardAt = c.feet;
  }
  lights.push_back({c.feet + kUp * (c.height * 0.52f), kViolet, (0.9f + 0.4f * pulse) * env * std::min(1.0f, static_cast<float>(drawn) / 3.0f), 2.6f * k});
}

// --- A dagger thrown ------------------------------------------------------------------------------------------------------

void DaggerFx::buildThrow(const Throw& t, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = sizeOf(t.toHeight);
  const float arc = 0.18f * k;
  auto at = [&](float s) { return glm::mix(t.from, t.to, s) + kUp * (arc * 4.0f * s * (1.0f - s)); };
  const glm::vec3 side = glm::normalize(glm::cross(kUp, t.dir));
  if (now < t.impact) {
    // The dart: a quick ease-in, the blade along its velocity, rolling; a wake of arcane light behind it.
    const float u = std::clamp((now - t.start) / (t.impact - t.start), 0.0f, 1.0f);
    const float s = std::pow(u, 1.35f);
    const glm::vec3 p = at(s), ahead = at(std::min(1.0f, s + 0.02f));
    const glm::vec3 vel = glm::length(ahead - p) > 1e-5f ? glm::normalize(ahead - p) : t.dir;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p, eye, kPull)) * alignY(vel) * glm::rotate(glm::mat4(1.0f), u * 22.0f, kUp);
    dagger(m, k, 1.0f);
    streaks_.push_back(streak(pulled(p + vel * (0.3f * k), eye, kPull), glm::vec3(0.0f), 0.08f * k, 0.8f, kVioletCore, 2.4f));
    for (int j = 1; j <= 7; ++j) {
      const float back = s - 0.045f * static_cast<float>(j);
      if (back < 0.0f) break;
      const float f = 1.0f - static_cast<float>(j) / 8.0f;
      streaks_.push_back(streak(pulled(at(back), eye, kPull), vel * (0.12f * k), 0.045f * k * f, 0.5f * f, kViolet, 2.0f));
    }
    lights.push_back({p, kViolet, 2.2f, 2.0f * k});
    return;
  }
  const float age = now - t.impact;
  const glm::vec3 contact = pulled(t.to, eye, kPull);
  if (!t.intercept) {
    // Buried in the body: the dagger stays a moment, then dissolves; the flash with its cross glint, the ring square to the
    // flight, steel sparks out of the wound, the light.
    const float keep = 1.0f - ease(kBury, kBury + kDissolve, age);
    if (keep > 0.0f) {
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), contact - t.dir * (0.1f * k)) * alignY(t.dir) * glm::scale(glm::mat4(1.0f), glm::vec3(0.4f + 0.6f * keep));
      dagger(m, k, keep);
    }
    if (age < 0.16f) {
      const float f = 1.0f - ease(0.0f, 0.16f, age);
      streaks_.push_back(streak(contact, glm::vec3(0.0f), 0.5f * k * (0.6f + 0.4f * f), f, kVioletCore, 2.4f));
      streaks_.push_back(streak(contact, side * (0.9f * k * f), 0.04f * k, f, kWhite, 2.6f));
      streaks_.push_back(streak(contact, kUp * (0.8f * k * f), 0.035f * k, f, kWhite, 2.6f));
    }
    if (age < 0.3f) {
      const float r = (0.12f + 0.5f * easeOut(0.0f, 0.28f, age)) * k;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), contact) * alignY(t.dir) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      draw(discFirst_, discCount_, m, kModeRing, age, 0.45f, 0.65f * (1.0f - ease(0.06f, 0.3f, age)), kViolet, 0.0f, glm::vec4(kVioletCore, 0.85f), t.to,
           VfxBlend::Additive);
    }
    for (int j = 0; j < 10; ++j) {
      const uint32_t n = static_cast<uint32_t>(j) + 60u;
      const float life = 0.3f + 0.2f * hash01(t.seed, n, 1);
      if (age > life) continue;
      const glm::vec3 d = glm::normalize(-t.dir * 0.8f + sphereDir(hash01(t.seed, n, 2), hash01(t.seed, n, 3)) * 0.9f);
      const float speed = (2.2f + 2.0f * hash01(t.seed, n, 4)) * k;
      const glm::vec3 p = contact + d * (speed * age) - kUp * (4.9f * age * age);
      streaks_.push_back(streak(p, d * (0.1f * k), 0.028f * k, 0.9f * (1.0f - age / life), kSpark, 2.5f));
    }
    if (age > kBury) {  // the dagger dissolving into motes
      const float u = age - kBury;
      for (int j = 0; j < 6; ++j) {
        const uint32_t n = static_cast<uint32_t>(j) + 80u;
        const glm::vec3 d = sphereDir(hash01(t.seed, n, 1), hash01(t.seed, n, 2));
        streaks_.push_back(streak(contact + d * (u * 1.1f * k), glm::vec3(0.0f), 0.04f * k, 0.6f * (1.0f - ease(0.0f, kDissolve, u)), kViolet, 2.2f));
      }
    }
    lights.push_back({t.to, kViolet, 7.0f * (1.0f - ease(0.0f, 0.28f, age)), 2.6f * k});
    return;
  }
  // The clash in the air: a white flash, sparks both ways, the dagger shattering into shards that spin away and fall.
  if (age < 0.2f) {
    const float f = 1.0f - ease(0.0f, 0.2f, age);
    streaks_.push_back(streak(contact, glm::vec3(0.0f), 0.7f * k * (0.5f + 0.5f * f), f, kWhite, 2.8f));
    streaks_.push_back(streak(contact, side * (1.1f * k * f), 0.045f * k, f, kWhite, 2.6f));
    streaks_.push_back(streak(contact, kUp * (0.9f * k * f), 0.04f * k, f, kVioletCore, 2.6f));
  }
  for (int j = 0; j < 16; ++j) {
    const uint32_t n = static_cast<uint32_t>(j) + 100u;
    const float life = 0.3f + 0.25f * hash01(t.seed, n, 1);
    if (age > life) continue;
    const glm::vec3 d = glm::normalize(sphereDir(hash01(t.seed, n, 2), hash01(t.seed, n, 3)) + (j & 1 ? t.dir : -t.dir) * 0.6f);
    const float speed = (2.4f + 2.4f * hash01(t.seed, n, 4)) * k;
    const glm::vec3 p = contact + d * (speed * age) - kUp * (4.9f * age * age);
    streaks_.push_back(streak(p, d * (0.12f * k), 0.03f * k, 0.9f * (1.0f - age / life), j % 3 ? kSpark : kWhite, 2.5f));
  }
  for (int j = 0; j < 4; ++j) {  // the shards: pieces of the blade spinning off, darkening as they fall
    const uint32_t n = static_cast<uint32_t>(j) + 120u;
    const float life = 0.55f + 0.2f * hash01(t.seed, n, 1);
    if (age > life) continue;
    const glm::vec3 d = glm::normalize(sphereDir(hash01(t.seed, n, 2), hash01(t.seed, n, 3)) + kUp * 0.4f);
    const glm::vec3 p = contact + d * ((1.6f + 1.2f * hash01(t.seed, n, 4)) * k * age) - kUp * (4.9f * age * age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), p) * glm::rotate(glm::mat4(1.0f), age * (14.0f + 6.0f * hash01(t.seed, n, 5)), d) *
                        glm::scale(glm::mat4(1.0f), {0.9f, 0.3f + 0.2f * hash01(t.seed, n, 6), 0.9f});
    const float fade = 1.0f - ease(life * 0.6f, life, age);
    blades_.push_back(lit(m * glm::scale(glm::mat4(1.0f), glm::vec3(k)), kSteelLit * fade));
  }
  lights.push_back({t.to, kVioletCore, 9.0f * (1.0f - ease(0.0f, 0.25f, age)), 2.8f * k});
}

// --- The cast --------------------------------------------------------------------------------------------------------------

void DaggerFx::buildBurst(const Burst& b, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float k = sizeOf(b.height);
  const glm::vec3 chest = b.feet + kUp * (b.height * 0.52f);
  const float age = now - b.impact;  // negative while the light gathers
  if (age < 0.0f) {
    const float u = 1.0f + age / 0.5f;  // 0 .. 1 over the gathering
    for (int j = 0; j < 14; ++j) {
      const uint32_t n = static_cast<uint32_t>(j);
      const float a = hash01(b.seed, n, 1) * kTau + u * 3.0f;
      const float r = (0.9f - 0.8f * u) * k * (0.7f + 0.5f * hash01(b.seed, n, 2));
      const glm::vec3 p = b.feet + glm::vec3(std::cos(a) * r, b.height * 0.52f * std::pow(u, 1.3f), std::sin(a) * r);
      streaks_.push_back(streak(wrapped(p, b.feet, eye), glm::vec3(0.0f), 0.05f * k, 0.8f * ease(0.0f, 0.15f, u), kViolet, 2.3f));
    }
    lights.push_back({chest, kViolet, 2.0f * u, 2.4f * k});
    return;
  }
  const glm::vec3 c = pulled(chest, eye, kPull);
  if (age < 0.3f) {
    const float f = 1.0f - ease(0.0f, 0.3f, age);
    streaks_.push_back(streak(c, glm::vec3(0.0f), 0.9f * k * (0.5f + 0.5f * f), f, kVioletCore, 2.6f));
  }
  {
    const float r = (0.3f + 1.1f * easeOut(0.0f, 0.5f, age)) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(b.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.5f, 0.7f * (1.0f - ease(0.1f, 0.6f, age)), kViolet, 0.0f, glm::vec4(kVioletCore, 0.85f), b.feet, VfxBlend::Additive);
  }
  lights.push_back({chest, kVioletCore, 8.0f * (1.0f - ease(0.0f, 0.5f, age)), 3.0f * k});
}

}  // namespace ember
