#include "render/vfx/RangerFx.h"

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
constexpr float kPull = 1.3f, kGroundPull = 0.7f;
constexpr float kGravity = 9.8f;
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f, kModeTrail = 4.0f;
constexpr float kHex = 1.7320508f;

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
// The arrow's shaft: a unit box along y (stretched by its matrix); its head: a flat steel point.
const std::vector<VfxVertex>& shaftGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(1.0f, 1.0f, 1.0f);
  return g;
}
const std::vector<VfxVertex>& headGeometry() {
  static const std::vector<VfxVertex> g = [] {
    // a four-sided point: an icosahedron squashed into a spearhead
    std::vector<VfxVertex> r = vfxgeo::icosahedron(1.0f, 0);
    for (VfxVertex& v : r) v.position *= glm::vec3(0.35f, 1.0f, 0.12f);
    for (size_t i = 0; i + 2 < r.size(); i += 3) {
      const glm::vec3 n = glm::normalize(glm::cross(r[i + 1].position - r[i].position, r[i + 2].position - r[i].position));
      r[i].normal = r[i + 1].normal = r[i + 2].normal = n;
    }
    return r;
  }();
  return g;
}
const std::vector<VfxVertex>& chipGeometry() {
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

struct Palette {
  glm::vec3 light, core, energy;
};
constexpr Palette kVenom{{0.45f, 1.0f, 0.35f}, {0.85f, 1.0f, 0.7f}, {0.3f, 0.95f, 0.25f}};
constexpr Palette kFire{{1.0f, 0.55f, 0.2f}, {1.0f, 0.93f, 0.72f}, {1.0f, 0.4f, 0.1f}};
constexpr Palette kGrey{{0.75f, 0.78f, 0.8f}, {0.95f, 0.95f, 0.95f}, {0.6f, 0.62f, 0.66f}};
constexpr Palette kCrimson{{1.0f, 0.2f, 0.15f}, {1.0f, 0.75f, 0.65f}, {0.85f, 0.08f, 0.1f}};
constexpr Palette kBlood{{1.0f, 0.15f, 0.1f}, {1.0f, 0.6f, 0.5f}, {0.7f, 0.03f, 0.05f}};
constexpr Palette kSwift{{0.75f, 0.85f, 1.0f}, {0.95f, 0.98f, 1.0f}, {0.6f, 0.72f, 0.95f}};
const Palette& paletteOf(RangerFx::Shot s) {
  switch (s) {
    case RangerFx::Shot::Venom: return kVenom;
    case RangerFx::Shot::Explosive: return kFire;
    case RangerFx::Shot::Crippling: return kGrey;
    case RangerFx::Shot::Death: return kCrimson;
    case RangerFx::Shot::Bloody: return kBlood;
    case RangerFx::Shot::Swift: return kSwift;
  }
  return kGrey;
}
constexpr glm::vec3 kWood{0.42f, 0.3f, 0.18f}, kSteel{0.6f, 0.63f, 0.68f}, kDust{0.46f, 0.4f, 0.33f};

}  // namespace

float RangerFx::Effect::end() const {
  if (kind == Kind::Volley) return impact + (big ? 1.9f : 1.5f);
  switch (style) {
    case Shot::Venom: return impact + 1.0f;
    case Shot::Explosive: return impact + 1.4f;
    case Shot::Crippling: return impact + 2.2f;
    case Shot::Death: return impact + 1.3f;
    case Shot::Bloody: return impact + 1.6f;
    case Shot::Swift: return impact + 0.6f;
  }
  return impact + 1.0f;
}
glm::vec3 RangerFx::Effect::at(float s) const {
  const float dist = glm::length(body - hand);
  return glm::mix(hand, body, s) + kUp * (dist * 0.06f * 4.0f * s * (1.0f - s));
}

float RangerFx::flightSeconds(glm::vec3 fromFeet, glm::vec3 toFeet) { return std::clamp(glm::length(toFeet - fromFeet) / 22.0f, 0.12f, 0.4f); }

void RangerFx::shot(Shot style, glm::vec3 from, float height, const Target& foe, float release, float impact) {
  Effect e;
  e.kind = Kind::Shot, e.style = style;
  const float h = bodyHeight(height), th = bodyHeight(foe.height);
  e.dir = flatDir(foe.feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.hand = from + kUp * (h * 0.55f) + e.dir * 0.4f;
  e.feet = foe.feet;
  e.body = foe.feet + kUp * (th * (style == Shot::Crippling ? 0.22f : 0.5f)) - e.dir * 0.15f;
  e.height = th;
  e.start = release, e.release = release, e.impact = std::max(impact, release + 0.08f);
  e.seed = 0x4a5fu + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

void RangerFx::volley(glm::vec3 from, const std::vector<glm::vec3>& hexes, const std::vector<Target>& foes, float start, float impact, bool big) {
  if (hexes.empty()) return;
  Effect e;
  e.kind = Kind::Volley, e.big = big;
  e.from = from, e.hexes = hexes, e.foes = foes;
  glm::vec3 centre{0.0f};
  for (const glm::vec3& h : hexes) centre += h;
  centre /= static_cast<float>(hexes.size());
  e.feet = centre;
  e.dir = flatDir(centre - from, glm::vec3(0.0f, 0.0f, 1.0f));
  e.start = start, e.release = start, e.impact = std::max(impact, start + 0.1f);
  e.seed = 0x7c21u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

bool RangerFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

void RangerFx::arrow(glm::vec3 at, glm::vec3 dir, float scale, float alpha, glm::vec3 eye, float pull) {
  if (alpha <= 0.01f) return;
  const glm::vec3 d = glm::length(dir) > 1e-6f ? glm::normalize(dir) : kUp;
  const float len = 0.95f * scale;
  const glm::vec3 p = pulled(at, eye, pull);
  // the shaft, its middle a little behind the point; the head at the point
  const glm::mat4 shaft = glm::translate(glm::mat4(1.0f), p - d * (len * 0.5f)) * alignY(d) * glm::scale(glm::mat4(1.0f), {0.022f * scale, len, 0.022f * scale});
  shafts_.push_back({shaft, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(kWood * alpha, 1.0f)});
  const glm::mat4 head = glm::translate(glm::mat4(1.0f), p - d * (0.05f * scale)) * alignY(d) * glm::scale(glm::mat4(1.0f), glm::vec3(0.075f * scale));
  heads_.push_back({head, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(kSteel * alpha, 1.0f)});
}

void RangerFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  shaftFirst_ = frame.addVertices(shaftGeometry()), shaftCount_ = static_cast<uint32_t>(shaftGeometry().size());
  headFirst_ = frame.addVertices(headGeometry()), headCount_ = static_cast<uint32_t>(headGeometry().size());
  chipFirst_ = frame.addVertices(chipGeometry()), chipCount_ = static_cast<uint32_t>(chipGeometry().size());
  streaks_.clear(), puffs_.clear(), shafts_.clear(), heads_.clear(), chips_.clear();
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    if (e.kind == Kind::Shot) buildShot(e, now, camera, frame, lights);
    else buildVolley(e, now, camera, frame, lights);
  }
  const glm::vec3 sight = effects_.front().feet;
  const Draw draw{frame};
  if (!puffs_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
         static_cast<uint32_t>(puffs_.size()));
  }
  // Lit by the scene (Standard): the shafts (wood), the heads (steel, a little metallic), the chips of ground.
  struct Lit {
    const std::vector<VfxInstance>* list;
    uint32_t first, count;
    float roughness, metal;
  };
  const Lit lit[] = {{&shafts_, shaftFirst_, shaftCount_, 0.8f, 0.0f}, {&heads_, headFirst_, headCount_, 0.35f, 0.7f}, {&chips_, chipFirst_, chipCount_, 0.92f, 0.0f}};
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

// --- One arrow: the flight, then the skill's own impact ------------------------------------------------------------------

void RangerFx::buildShot(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const Palette& pal = paletteOf(e.style);
  const float k = std::clamp(e.height / 2.0f, 0.7f, 1.4f);
  const glm::vec3 side = glm::normalize(glm::cross(kUp, e.dir));
  const float flight = e.impact - e.release;
  const float s = std::clamp((now - e.release) / flight, 0.0f, 1.0f);

  // --- The flight: the arrow along the arc, a glint at its point, the skill's trail behind it, its light. ---
  if (now < e.impact) {
    const glm::vec3 head = e.at(s), ahead = e.at(std::min(1.0f, s + 0.02f));
    const glm::vec3 v = glm::length(ahead - head) > 1e-5f ? glm::normalize(ahead - head) : e.dir;
    arrow(head, v, 1.0f, 1.0f, eye, kPull * 0.5f);
    streaks_.push_back(streak(pulled(head, eye, kPull * 0.5f), v * 0.18f, 0.03f * k, 0.8f, pal.core, 2.2f));
    if (e.style == Shot::Death) {  // a long crimson trail
      const float tail = std::min(s, 0.6f);
      if (tail > 0.02f) {
        three::CatmullRom path;
        for (int j = 0; j <= 8; ++j) path.points.push_back(pulled(e.at(s - tail + tail * static_cast<float>(j) / 8.0f), eye, kPull * 0.5f));
        const std::vector<VfxVertex> tube = vfxgeo::tube(path, 18, 0.05f * k, 8);
        draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, now - e.release, 1.3f, 0.9f, pal.energy, 0.0f,
             glm::vec4(pal.core, 0.0f), e.feet, VfxBlend::Additive);
      }
    }
    lights.push_back({head, pal.light, e.style == Shot::Death ? 9.0f : 5.0f, 2.4f * k});
  }
  // --- Shed along the path: venom motes, embers, grey dust, dark red motes (each born as the arrow passed). ---
  const int shed = e.style == Shot::Crippling || e.style == Shot::Swift ? 8 : 16;
  for (int i = 0; i < shed; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 70u;
    const float ub = (static_cast<float>(i) + 0.5f) / static_cast<float>(shed);
    const float tb = e.release + ub * flight;
    const float life = 0.3f + 0.3f * hash01(e.seed, n, 1);
    const float tau = now - tb;
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 d = sphereDir(hash01(e.seed, n, 2), hash01(e.seed, n, 3));
    const glm::vec3 p = e.at(ub) + d * 0.3f * tau + kUp * ((e.style == Shot::Explosive ? 0.6f : -0.3f) * tau * tau);
    const float size = (e.style == Shot::Explosive ? 0.045f : 0.035f) * k * (1.0f - 0.5f * u);
    streaks_.push_back(streak(pulled(p, eye, kPull * 0.5f), glm::vec3(0.0f), size, (1.0f - u) * 0.9f, pal.energy, e.style == Shot::Crippling ? 1.2f : 2.2f));
  }

  const float age = now - e.impact;
  if (age < 0.0f) return;
  const glm::vec3 point = pulled(e.body, eye, kPull);

  if (e.style == Shot::Venom) {
    // --- A sick green burst on the body, drips falling to the feet, a green ring, a green light. ---
    if (age < 0.12f) streaks_.push_back(streak(point, glm::vec3(0.0f), 0.6f * k * (1.0f - 0.4f * ease(0.0f, 0.12f, age)), 1.0f - ease(0.0f, 0.12f, age), pal.core, 2.2f));
    if (age < 0.4f) {
      const float r = k * (0.15f + 0.6f * easeOut(0.0f, 0.3f, age));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), point) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.2f, 0.8f * (1.0f - ease(0.1f, 0.4f, age)), pal.energy, 0.1f, glm::vec4(pal.core, 0.35f), e.feet,
           VfxBlend::Additive);
    }
    for (int i = 0; i < 14; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float life = 0.5f + 0.4f * hash01(e.seed, n, 1);
      const float tau = age - hash01(e.seed, n, 2) * 0.08f;
      if (tau < 0.0f || tau > life) continue;
      const float a = hash01(e.seed, n, 3) * kTau;
      const glm::vec3 d{std::cos(a) * 0.35f, 0.2f + 0.5f * hash01(e.seed, n, 4), std::sin(a) * 0.35f};
      const glm::vec3 p = e.body + d * tau * 1.5f * k + glm::vec3(0.0f, -0.5f * kGravity * 0.35f * tau * tau, 0.0f);
      if (p.y < e.feet.y + 0.02f) continue;
      const float u = tau / life;
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f, -0.08f, 0.0f) * k, 0.045f * k, 1.0f - u * u, pal.energy, 2.0f));
    }
    if (age < 0.6f) {
      const float r = (0.3f + 0.7f * easeOut(0.0f, 0.4f, age)) * k;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      draw(discFirst_, discCount_, m, kModeRing, age, 0.5f, 0.6f * (1.0f - ease(0.15f, 0.6f, age)), pal.energy, 0.0f, glm::vec4(pal.core, 0.85f), e.feet, VfxBlend::Additive);
    }
    if (age < 0.5f) lights.push_back({e.body, pal.light, 9.0f * (1.0f - ease(0.0f, 0.5f, age)), 2.6f * k});
  } else if (e.style == Shot::Explosive) {
    // --- A flash, a shell of fire bursting out over the neighbouring hexes, flames and embers flung up, a ring of fire on
    // the ground, dust, a strong orange light. ---
    if (age < 0.14f) streaks_.push_back(streak(point, glm::vec3(0.0f), 1.1f * k * (0.5f + 0.5f * (1.0f - ease(0.0f, 0.14f, age))), 1.0f - ease(0.0f, 0.14f, age), pal.core, 2.8f));
    if (age < 0.45f) {
      const float r = 0.2f * k + 1.9f * easeOut(0.0f, 0.38f, age);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), e.body) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.1f, 0.8f * (1.0f - ease(0.08f, 0.45f, age)), pal.energy, 0.1f, glm::vec4(pal.core, 0.35f), e.feet,
           VfxBlend::Additive);
    }
    if (age < 0.6f) {
      const float r = 0.3f + 1.9f * easeOut(0.0f, 0.45f, age);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      draw(discFirst_, discCount_, m, kModeRing, age, 0.8f, 0.8f * (1.0f - ease(0.15f, 0.6f, age)), pal.energy, 0.0f, glm::vec4(pal.core, 0.86f), e.feet, VfxBlend::Additive);
    }
    for (int i = 0; i < 34; ++i) {  // flames and embers flung up and out
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float life = 0.35f + 0.5f * hash01(e.seed, n, 1);
      const float tau = age - hash01(e.seed, n, 2) * 0.05f;
      if (tau < 0.0f || tau > life) continue;
      const glm::vec3 d = glm::normalize(sphereDir(hash01(e.seed, n, 3), hash01(e.seed, n, 4)) + kUp * 0.9f);
      const float speed = (2.5f + 4.0f * hash01(e.seed, n, 5)) * k;
      constexpr float kDrag = 3.5f;
      const glm::vec3 p = e.body + d * speed * (1.0f - std::exp(-kDrag * tau)) / kDrag + kUp * (0.5f * tau);
      const float u = tau / life;
      streaks_.push_back(streak(pulled(p, eye, kPull), d * speed * std::exp(-kDrag * tau) * 0.05f, (0.05f + 0.04f * hash01(e.seed, n, 6)) * k, (1.0f - u) * (1.0f - u),
                                glm::mix(pal.core, pal.energy, std::min(1.0f, u * 1.5f)), 2.8f));
    }
    for (int i = 0; i < 14; ++i) {  // dust rolling out
      const uint32_t n = static_cast<uint32_t>(i) + 200u;
      const float life = 1.1f * (0.85f + 0.3f * hash01(e.seed, n, 1));
      const float tau = age - 0.03f;
      if (tau < 0.0f || tau > life) continue;
      const float u = tau / life;
      const float a = (static_cast<float>(i) + 0.4f * hash01(e.seed, n, 2)) / 14.0f * kTau;
      const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
      const glm::vec3 p = e.feet + out * (0.3f + 1.6f * easeOut(0.0f, 0.7f, u)) + kUp * (0.1f + 0.3f * u);
      puffs_.push_back(puff(pulled(p, eye, kGroundPull), 0.35f + 0.6f * easeOut(0.0f, 1.0f, u), a + tau, hash01(e.seed, n, 3) * 37.0f,
                            0.45f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.25f, 1.0f, u)), glm::vec3(0.3f, 0.26f, 0.22f) * (0.85f + 0.3f * hash01(e.seed, n, 4))));
    }
    if (age < 0.7f) lights.push_back({e.body, pal.light, 22.0f * (1.0f - ease(0.05f, 0.7f, age)), 4.2f * k});
  } else if (e.style == Shot::Crippling) {
    // --- The arrow goes in low: a burst of dust at the feet, grey chips, a dull grey ring pulsing under the foe. ---
    if (age < 0.1f) streaks_.push_back(streak(point, glm::vec3(0.0f), 0.4f * k, 1.0f - ease(0.0f, 0.1f, age), pal.core, 1.8f));
    for (int i = 0; i < 8; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float life = 0.8f * (0.8f + 0.4f * hash01(e.seed, n, 1));
      const float tau = age - hash01(e.seed, n, 2) * 0.05f;
      if (tau < 0.0f || tau > life) continue;
      const float u = tau / life;
      const glm::vec3 base = e.feet + (e.dir * 0.2f + side * (hash01(e.seed, n, 3) - 0.5f) * 0.8f) * k;
      const glm::vec3 p = base + e.dir * (0.4f * easeOut(0.0f, 1.0f, u) * k) + kUp * ((0.1f + 0.3f * easeOut(0.0f, 1.0f, u)) * k);
      puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.25f + 0.5f * easeOut(0.0f, 1.0f, u)) * k, hash01(e.seed, n, 4) * kTau + tau, hash01(e.seed, n, 5) * 37.0f,
                            0.5f * ease(0.0f, 0.08f, tau) * (1.0f - ease(0.2f, 1.0f, u)), kDust * (0.85f + 0.3f * hash01(e.seed, n, 6))));
    }
    for (int i = 0; i < 8 && age < 1.2f; ++i) {  // chips of the ground
      const uint32_t n = static_cast<uint32_t>(i) + 300u;
      const float a = hash01(e.seed, n, 1) * kTau;
      const glm::vec3 vh = glm::normalize(e.dir * 0.8f + glm::vec3(std::cos(a), 0.0f, std::sin(a))) * (0.8f + 1.4f * hash01(e.seed, n, 3)) * k;
      const float vy = (1.6f + 1.4f * hash01(e.seed, n, 4)) * k;
      const float t1 = 2.0f * vy / kGravity;
      const float tau = std::min(age, t1);
      glm::vec3 p = e.feet + e.dir * 0.15f + vh * tau + glm::vec3(0.0f, 0.04f + vy * tau - 0.5f * kGravity * tau * tau, 0.0f);
      const float fade = 1.0f - ease(0.8f, 1.2f, age);
      const float size = (0.03f + 0.04f * hash01(e.seed, n, 5)) * k * fade;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p, eye, kGroundPull)) *
                          glm::rotate(glm::mat4(1.0f), tau * (8.0f + 10.0f * hash01(e.seed, n, 8)), sphereDir(hash01(e.seed, n, 6), hash01(e.seed, n, 7))) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(size));
      chips_.push_back({m, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(glm::vec3(0.34f, 0.3f, 0.26f) * (0.75f + 0.5f * hash01(e.seed, n, 9)), 1.0f)});
    }
    {  // the ring under the foe: on at once, pulsing, gone after two seconds
      const float env = ease(0.0f, 0.15f, age) * (1.0f - ease(1.6f, 2.2f, age));
      if (env > 0.0f) {
        const float r = (0.55f + 0.05f * std::sin(age * 7.0f)) * k;
        const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
        draw(discFirst_, discCount_, m, kModeRing, age, 0.35f, (0.45f + 0.2f * std::sin(age * 7.0f)) * env, pal.energy, 0.0f, glm::vec4(pal.core, 0.9f), e.feet,
             VfxBlend::Additive);
      }
    }
    if (age < 0.25f) lights.push_back({e.feet + kUp * 0.4f, pal.light, 5.0f * (1.0f - ease(0.0f, 0.25f, age)), 2.0f * k});
  } else if (e.style == Shot::Bloody) {
    // --- Tiro Sangrento: a red flash, blood sprayed the shot's way and falling, a dark stain spreading at the feet, a red light. ---
    if (age < 0.12f) streaks_.push_back(streak(point, glm::vec3(0.0f), 0.55f * k * (1.0f - 0.4f * ease(0.0f, 0.12f, age)), 1.0f - ease(0.0f, 0.12f, age), pal.core, 2.2f));
    for (int i = 0; i < 18; ++i) {  // drops: dark, falling, a little light caught on them
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float life = 0.35f + 0.35f * hash01(e.seed, n, 1);
      const float tau = age - hash01(e.seed, n, 2) * 0.04f;
      if (tau < 0.0f || tau > life) continue;
      const glm::vec3 d = glm::normalize(e.dir * 1.1f + side * (hash01(e.seed, n, 3) - 0.5f) * 1.4f + kUp * (0.2f + 0.6f * hash01(e.seed, n, 4)));
      const glm::vec3 v0 = d * (1.8f + 2.6f * hash01(e.seed, n, 5)) * k;
      const glm::vec3 p = e.body + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
      if (p.y < e.feet.y + 0.02f) continue;
      streaks_.push_back(streak(pulled(p, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.04f, 0.032f * k, 0.9f, {0.7f, 0.05f, 0.05f}, 1.2f));
    }
    for (int i = 0; i < 6; ++i) {  // a fine red mist where it went in
      const uint32_t n = static_cast<uint32_t>(i) + 200u;
      const float life = 0.6f * (0.8f + 0.4f * hash01(e.seed, n, 1));
      const float tau = age - 0.02f;
      if (tau < 0.0f || tau > life) continue;
      const float u = tau / life;
      const glm::vec3 p = e.body + (e.dir * 0.5f + side * (hash01(e.seed, n, 2) - 0.5f) * 0.6f + kUp * (hash01(e.seed, n, 3) - 0.3f) * 0.4f) * (0.4f + 0.6f * u) * k;
      puffs_.push_back(puff(pulled(p, eye, kPull), (0.12f + 0.25f * u) * k, hash01(e.seed, n, 4) * kTau, hash01(e.seed, n, 5) * 37.0f,
                            0.5f * ease(0.0f, 0.05f, tau) * (1.0f - ease(0.2f, 1.0f, u)), {0.4f, 0.03f, 0.04f}));
    }
    if (age < 1.5f) {  // the stain at the feet
      const float r = (0.15f + 0.45f * easeOut(0.05f, 0.7f, age)) * k;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.03f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 0.02f * r, r});
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.0f, 0.55f * (1.0f - ease(1.0f, 1.5f, age)), {0.4f, 0.02f, 0.03f}, 0.4f, glm::vec4(0.4f, 0.02f, 0.03f, 0.4f),
           e.feet, VfxBlend::Alpha);
    }
    if (age < 0.5f) lights.push_back({e.body, pal.light, 8.0f * (1.0f - ease(0.0f, 0.5f, age)), 2.4f * k});
  } else if (e.style == Shot::Swift) {
    // --- Tiro Múltiplo: a sharp white flash and a cross glint, a few sparks. ---
    if (age < 0.1f) {
      const float f = 1.0f - ease(0.0f, 0.1f, age);
      streaks_.push_back(streak(point, glm::vec3(0.0f), 0.38f * k * (0.6f + 0.4f * f), f, pal.core, 2.2f));
      streaks_.push_back(streak(point, side * 0.7f * k * f, 0.03f * k, f, pal.core, 2.6f));
      streaks_.push_back(streak(point, kUp * 0.5f * k * f, 0.025f * k, f, pal.core, 2.4f));
    }
    for (int i = 0; i < 8; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float life = 0.14f + 0.16f * hash01(e.seed, n, 1);
      const float tau = age - hash01(e.seed, n, 2) * 0.02f;
      if (tau < 0.0f || tau > life) continue;
      const glm::vec3 d = glm::normalize(sphereDir(hash01(e.seed, n, 3), hash01(e.seed, n, 4)) + e.dir);
      const glm::vec3 v0 = d * (2.5f + 3.0f * hash01(e.seed, n, 5)) * k;
      const glm::vec3 p = e.body + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
      const float u = tau / life;
      streaks_.push_back(streak(pulled(p, eye, kPull), v0 * 0.04f, 0.025f * k, 1.0f - u * u, glm::mix(pal.core, pal.energy, u), 2.8f));
    }
    if (age < 0.15f) lights.push_back({e.body, pal.light, 5.0f * (1.0f - ease(0.0f, 0.15f, age)), 1.8f * k});
  } else {
    // --- Tiro Mortal: a pin of light driven through the body, a crimson shell, dark streaks flung out, a red ring, a red light. ---
    if (age < 0.18f) {
      const float f = 1.0f - ease(0.0f, 0.18f, age);
      streaks_.push_back(streak(point, e.dir * 1.4f * k * f, 0.06f * k, f, pal.core, 3.0f));
      streaks_.push_back(streak(point, glm::vec3(0.0f), 0.9f * k * (0.5f + 0.5f * f), f, pal.core, 2.6f));
    }
    if (age < 0.4f) {
      const float r = k * (0.15f + 0.9f * easeOut(0.0f, 0.32f, age));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), point) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.0f, 0.7f * (1.0f - ease(0.06f, 0.4f, age)), pal.energy, 0.08f, glm::vec4(pal.core, 0.15f), e.feet,
           VfxBlend::Additive);
    }
    for (int i = 0; i < 24; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float life = 0.3f + 0.35f * hash01(e.seed, n, 1);
      const float tau = age - hash01(e.seed, n, 2) * 0.03f;
      if (tau < 0.0f || tau > life) continue;
      const glm::vec3 d = glm::normalize(sphereDir(hash01(e.seed, n, 3), hash01(e.seed, n, 4)) + e.dir * 1.2f);
      const float speed = (3.0f + 4.0f * hash01(e.seed, n, 5)) * k;
      constexpr float kDrag = 5.0f;
      const glm::vec3 p = e.body + d * speed * (1.0f - std::exp(-kDrag * tau)) / kDrag;
      const float u = tau / life;
      streaks_.push_back(streak(pulled(p, eye, kPull), d * speed * std::exp(-kDrag * tau) * 0.06f, 0.03f * k, (1.0f - u) * (1.0f - u),
                                glm::mix(pal.core, glm::vec3(0.4f, 0.02f, 0.05f), std::min(1.0f, u * 1.6f)), 2.6f));
    }
    if (age < 0.6f) {
      const float r = (0.3f + 1.1f * easeOut(0.0f, 0.45f, age)) * k;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      draw(discFirst_, discCount_, m, kModeRing, age, 0.7f, 0.7f * (1.0f - ease(0.12f, 0.6f, age)), pal.energy, 0.0f, glm::vec4(pal.core, 0.85f), e.feet, VfxBlend::Additive);
    }
    if (age < 1.2f) lights.push_back({e.body, pal.light, 16.0f * std::exp(-age * 3.5f), 3.4f * k});
  }
}

// --- A volley: arrows from the sky onto every hex ----------------------------------------------------------------------------

void RangerFx::buildVolley(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const int perHex = e.big ? 5 : 3;
  const float spread = e.big ? 0.55f : 0.3f;  // seconds the landings spread over, from the first at `impact`
  const float fall = 0.32f;                   // seconds an arrow falls
  // The volley comes down at a slant from the archer's side: from high above and a little back toward it.
  const glm::vec3 slant = glm::normalize(kUp * 6.0f - e.dir * 1.6f);
  int lit = 0;
  // --- A faint ring over the area before it lands (the whistle of the volley). ---
  if (now < e.impact) {
    const float r = (e.big ? 3.0f : 2.0f) * kHex * 0.5f + 0.5f;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.05f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, now - e.start, 0.25f, 0.4f * ease(e.start, e.start + 0.15f, now), {0.8f, 0.82f, 0.85f}, 0.0f, glm::vec4(0.95f, 0.95f, 0.95f, 0.92f),
         e.feet, VfxBlend::Additive);
  }
  for (size_t h = 0; h < e.hexes.size(); ++h) {
    const glm::vec3 hex = e.hexes[h];
    for (int i = 0; i < perHex; ++i) {
      const uint32_t n = static_cast<uint32_t>(h) * 31u + static_cast<uint32_t>(i) + 1u;
      const float wave = e.big && hash01(e.seed, n, 9) > 0.5f ? 0.45f : 0.0f;
      const float land = e.impact + hash01(e.seed, n, 1) * spread + wave;
      const glm::vec3 at = hex + glm::vec3((hash01(e.seed, n, 2) - 0.5f) * 1.1f, 0.0f, (hash01(e.seed, n, 3) - 0.5f) * 1.1f);
      const glm::vec3 d = -glm::normalize(slant + glm::vec3((hash01(e.seed, n, 4) - 0.5f) * 0.5f, 0.0f, (hash01(e.seed, n, 5) - 0.5f) * 0.5f));
      const float tau = now - (land - fall);
      if (tau < 0.0f) continue;
      if (now < land) {  // falling: 8 units of air in `fall` seconds, a glint at the point
        const float u = tau / fall;
        const glm::vec3 p = at - d * (8.0f * (1.0f - u * u));
        arrow(p, d, 1.0f, 1.0f, eye, kGroundPull);
        if (u > 0.5f) streaks_.push_back(streak(pulled(p, eye, kGroundPull), d * 0.2f, 0.025f, 0.7f * (u - 0.5f) * 2.0f, {0.95f, 0.95f, 1.0f}, 2.0f));
      } else {  // stuck in the ground, leaning the way it came, fading away
        const float since = now - land;
        const float fade = 1.0f - ease(0.7f, 1.2f, since);
        if (fade > 0.0f) arrow(at + glm::vec3(0.0f, 0.02f, 0.0f) - d * 0.15f, d, 0.8f * fade + 0.2f, fade, eye, kGroundPull);
        if (since < 0.08f) streaks_.push_back(streak(pulled(at + kUp * 0.1f, eye, kGroundPull), glm::vec3(0.0f), 0.25f * (1.0f - ease(0.0f, 0.08f, since)), 1.0f - ease(0.0f, 0.08f, since), {1.0f, 0.9f, 0.7f}, 2.0f));
        if (since < 0.9f) {
          const float u = since / 0.9f;
          const glm::vec3 p = at + kUp * (0.1f + 0.3f * easeOut(0.0f, 1.0f, u)) + glm::vec3(std::cos(n * 2.1f), 0.0f, std::sin(n * 2.1f)) * (0.1f + 0.25f * easeOut(0.0f, 1.0f, u));
          puffs_.push_back(puff(pulled(p, eye, kGroundPull), 0.2f + 0.35f * easeOut(0.0f, 1.0f, u), n * 0.7f + since, hash01(e.seed, n, 6) * 37.0f,
                                0.4f * ease(0.0f, 0.05f, since) * (1.0f - ease(0.2f, 1.0f, u)), kDust * (0.85f + 0.3f * hash01(e.seed, n, 7))));
        }
        if (since < 0.1f && lit < 6) {
          ++lit;
          lights.push_back({at + kUp * 0.4f, {1.0f, 0.85f, 0.6f}, 4.0f * (1.0f - ease(0.0f, 0.1f, since)), 1.6f});
        }
      }
    }
  }
  // --- A flash on every foe standing in it as the first arrows land. ---
  for (const Target& f : e.foes) {
    const float tau = now - e.impact;
    if (tau < 0.0f || tau > 0.22f) continue;
    const float h = bodyHeight(f.height);
    const float amp = 1.0f - ease(0.0f, 0.22f, tau);
    streaks_.push_back(streak(pulled(f.feet + kUp * (h * 0.5f), eye, kPull), glm::vec3(0.0f), 0.45f * amp, amp, {1.0f, 0.85f, 0.6f}, 2.2f));
  }
}

}  // namespace ember
