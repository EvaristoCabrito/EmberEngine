#include "render/vfx/CombatFx.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
// Drawn this much nearer the eye than where they are (same place on screen): a unit's card is drawn 1.2 units nearer in
// depth, so whatever happens on a body must sit in front of it to be seen over it.
constexpr float kPull = 1.3f, kGroundPull = 0.7f;
constexpr float kGravity = 9.8f;
// The Combat material's modes (shaders/vfx.hlsl).
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f, kModeTrail = 4.0f, kModeSlash = 5.0f;

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
// A direction on the unit sphere from two 0..1 numbers.
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
// A rotation taking +y to `axis`.
glm::mat4 alignY(glm::vec3 axis) {
  const glm::vec3 y = glm::normalize(axis);
  const glm::vec3 helper = std::abs(y.y) < 0.95f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 x = glm::normalize(glm::cross(helper, y)), z = glm::cross(x, y);
  glm::mat4 m(1.0f);
  m[0] = glm::vec4(x, 0.0f), m[1] = glm::vec4(y, 0.0f), m[2] = glm::vec4(z, 0.0f);
  return m;
}

// A unit quad (-0.5..0.5, u across, v along): streaks and puffs, shaped by the shader.
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
// A chipped stone: an icosahedron with every face its own normal (flat shading), squashed a little.
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

// A splinter of an arrow's shaft: a unit box, stretched by its matrix.
const std::vector<VfxVertex>& splinterGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(1.0f, 1.0f, 1.0f);
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

// A streak (a spark along its motion, or a round dot when `axis` is shorter than its width): instance a = centre +
// width, b = axis (tail -> head) + alpha, c = tint + intensity.
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}
// A puff of dust (alpha-blended, lit by the scene): a = centre + size, b = rotation, seed, alpha, -, c = its colour.
VfxInstance puff(glm::vec3 centre, float size, float rotation, float seed, float alpha, glm::vec3 albedo) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(rotation, seed, alpha, 0.0f), glm::vec4(albedo, 0.0f)};
}

}  // namespace

CombatPalette CombatPalette::of(const char* element) {
  const auto is = [&](const char* e) { return element && std::strcmp(element, e) == 0; };
  if (is("fire")) return {{1.0f, 0.5f, 0.18f}, {1.0f, 0.92f, 0.7f}, {1.0f, 0.38f, 0.08f}};
  if (is("frost") || is("ice") || is("cold")) return {{0.5f, 0.78f, 1.0f}, {0.92f, 0.98f, 1.0f}, {0.4f, 0.7f, 1.0f}};
  if (is("lightning") || is("shock")) return {{0.62f, 0.75f, 1.0f}, {0.95f, 0.97f, 1.0f}, {0.5f, 0.62f, 1.0f}};
  return {};  // Arcano
}

float CombatFx::Impact::end() const {
  switch (kind) {
    case Kind::Physical: return at + 1.0f;
    case Kind::Heavy: return at + 1.7f;
    case Kind::Magical: return at + 1.3f;
    case Kind::MagicalMiss: return at + 0.8f;
    case Kind::Slash:
    case Kind::Thrust: return at + 0.5f;
    case Kind::HeavySlash: return at + 0.6f;
    case Kind::ArrowHit:
    case Kind::HeavyArrowHit: return at + 1.0f;
  }
  return at + 1.0f;
}

void CombatFx::impact(Kind kind, glm::vec3 feet, float height, glm::vec3 from, float at, const CombatPalette& palette, float strength) {
  height = std::clamp(height, 1.0f, 4.0f);
  const glm::vec3 dir = flatDir(feet - from, glm::vec3(0.0f, 0.0f, 1.0f));
  // The contact: the middle of the body, on the striker's side.
  const glm::vec3 point = feet + glm::vec3(0.0f, height * 0.5f, 0.0f) - dir * 0.15f;
  impacts_.push_back({kind, point, feet, dir, height, at, palette, 0x51ed27u + 7919u * ++serial_, std::clamp(strength, 0.1f, 1.0f)});
}

float CombatFx::flightSeconds(glm::vec3 fromFeet, glm::vec3 toFeet) {
  const float d = glm::length(glm::vec2(toFeet.x - fromFeet.x, toFeet.z - fromFeet.z));
  return std::clamp(0.16f + d * 0.085f, 0.22f, 1.1f);
}

void CombatFx::bolt(glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, float start, float release, float impact,
                    const CombatPalette& palette, float strength) {
  casterHeight = std::clamp(casterHeight, 1.0f, 4.0f), targetHeight = std::clamp(targetHeight, 1.0f, 4.0f);
  const glm::vec3 dir = flatDir(targetFeet - casterFeet, glm::vec3(0.0f, 0.0f, 1.0f));
  Bolt b;
  b.hand = casterFeet + glm::vec3(0.0f, casterHeight * 0.55f, 0.0f) + dir * 0.35f;
  b.target = targetFeet + glm::vec3(0.0f, targetHeight * 0.5f, 0.0f) - dir * 0.15f;
  b.dir = dir;
  b.start = start, b.release = std::max(release, start + 0.2f), b.impact = std::max(impact, b.release + 0.1f);
  b.casterHeight = casterHeight;
  b.palette = palette;
  b.seed = 0xb017u + 104729u * ++serial_;
  b.strength = std::clamp(strength, 0.1f, 1.0f);
  bolts_.push_back(b);
}

float CombatFx::Bolt::progress(float now) const {
  const float u = std::clamp((now - release) / (impact - release), 0.0f, 1.0f);
  return 0.3f * u + 0.7f * u * u;  // let go at a speed, then faster: it arrives hard
}

glm::vec3 CombatFx::Bolt::at(float s) const {
  const float dist = glm::length(target - hand);
  const float arc = std::clamp(dist * 0.08f, 0.05f, 0.45f);
  return glm::mix(hand, target, s) + kUp * (arc * 4.0f * s * (1.0f - s));
}

bool CombatFx::active(float now) const {
  return std::any_of(impacts_.begin(), impacts_.end(), [&](const Impact& i) { return i.end() > now; }) ||
         std::any_of(bolts_.begin(), bolts_.end(), [&](const Bolt& b) { return b.end() > now; });
}

void CombatFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(impacts_, [&](const Impact& i) { return i.end() <= now; });
  std::erase_if(bolts_, [&](const Bolt& b) { return b.end() <= now; });
  if (impacts_.empty() && bolts_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  rockFirst_ = frame.addVertices(rockGeometry()), rockCount_ = static_cast<uint32_t>(rockGeometry().size());
  splinterFirst_ = frame.addVertices(splinterGeometry()), splinterCount_ = static_cast<uint32_t>(splinterGeometry().size());
  streaks_.clear(), puffs_.clear(), rocks_.clear(), splinters_.clear();
  for (const Bolt& b : bolts_) buildBolt(b, now, camera, frame, lights);
  for (const Impact& i : impacts_) buildImpact(i, now, camera, frame, lights);

  // The particles of everything in play, one draw each: the dust first (it covers), then the rocks, then the light.
  const glm::vec3 sight = !impacts_.empty() ? impacts_.front().feet : bolts_.front().target;
  const Draw draw{frame};
  if (!puffs_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
         static_cast<uint32_t>(puffs_.size()));
  }
  // Lit by the scene like the board (three MeshStandardMaterial: rough, not metal, no glow): the debris, the splinters.
  for (int set = 0; set < 2; ++set) {
    const std::vector<VfxInstance>& pieces = set == 0 ? rocks_ : splinters_;
    if (pieces.empty()) continue;
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), pieces.begin(), pieces.end());
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {0.92f, 0.0f, 0.0f, 0.0f};
    d.gpu.params1 = glm::vec4(0.0f);
    d.gpu.color = glm::vec4(1.0f);
    d.gpu.firstVertex = set == 0 ? rockFirst_ : splinterFirst_;
    d.gpu.vertexCount = set == 0 ? rockCount_ : splinterCount_;
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

void CombatFx::buildImpact(const Impact& im, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float age = now - im.at;
  if (age < 0.0f) return;
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const glm::vec3 side = glm::normalize(glm::cross(kUp, im.dir));
  // A lesser blow (strength < 1): every size by its root, the light and the particle counts by it.
  const float k = std::clamp(im.height / 2.0f, 0.7f, 1.4f) * std::sqrt(im.strength);
  const glm::vec3 point = pulled(im.point, eye, kPull);
  const CombatPalette& pal = im.palette;
  const bool physical = im.kind == Kind::Physical || im.kind == Kind::Heavy;
  const bool heavy = im.kind == Kind::Heavy;

  if (physical) {
    // --- The contact: a white-hot flash and a cross glint, gone in a tenth of a second. ---
    if (age < 0.11f) {
      const float f = 1.0f - ease(0.0f, 0.11f, age);
      streaks_.push_back(streak(point, glm::vec3(0.0f), (heavy ? 0.5f : 0.34f) * k * (0.6f + 0.4f * f), f, {1.0f, 0.7f, 0.4f}, 1.8f));
      streaks_.push_back(streak(point, side * (heavy ? 1.1f : 0.75f) * k * f, 0.04f * k, f, {1.0f, 0.85f, 0.6f}, 2.6f));
      streaks_.push_back(streak(point, kUp * (heavy ? 0.7f : 0.45f) * k * f, 0.035f * k, f, {1.0f, 0.85f, 0.6f}, 2.6f));
    }
    // --- Sparks: thrown away from the striker in 3D, falling, cooling from white to orange. ---
    const int sparks = heavy ? 32 : 18;
    for (int i = 0; i < sparks; ++i) {
      const uint32_t n = static_cast<uint32_t>(i);
      const float delay = hash01(im.seed, n, 6) * 0.03f;
      const float life = 0.16f + 0.28f * hash01(im.seed, n, 5);
      const float tau = age - delay;
      if (tau < 0.0f || tau > life) continue;
      const glm::vec3 d = glm::normalize(im.dir * (0.6f + 0.5f * hash01(im.seed, n, 1)) + side * (hash01(im.seed, n, 2) - 0.5f) * 1.7f +
                                         kUp * (0.15f + 0.95f * hash01(im.seed, n, 3)));
      const float speed = (3.0f + 4.5f * hash01(im.seed, n, 4)) * k * (heavy ? 1.3f : 1.0f);
      const glm::vec3 v0 = d * speed;
      const float drag = 1.0f - 0.35f * tau / life;
      glm::vec3 p = im.point + v0 * tau * drag + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
      if (p.y < im.feet.y + 0.02f) continue;  // into the ground: gone
      const glm::vec3 v = v0 * (1.0f - 0.7f * tau / life) + glm::vec3(0.0f, -kGravity * tau, 0.0f);
      const float u = tau / life;
      const glm::vec3 tint = glm::mix(glm::vec3(1.0f, 0.93f, 0.72f), glm::vec3(1.0f, 0.42f, 0.1f), std::min(1.0f, u * 1.4f));
      streaks_.push_back(streak(pulled(p, eye, kPull), v * 0.06f, 0.038f * k, 1.0f - u * u, tint, 3.6f * (1.0f - 0.6f * u)));
    }
    // --- Dust kicked up at the feet, drifting the blow's way. ---
    const int dusts = heavy ? 8 : 5;
    for (int i = 0; i < dusts; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 100u;
      const float life = (heavy ? 1.3f : 0.9f) * (0.8f + 0.4f * hash01(im.seed, n, 1));
      const float tau = age - hash01(im.seed, n, 2) * 0.05f;
      if (tau < 0.0f || tau > life) continue;
      const float u = tau / life;
      const glm::vec3 base = im.feet + (im.dir * (0.1f + 0.3f * hash01(im.seed, n, 3)) + side * (hash01(im.seed, n, 4) - 0.5f) * 0.7f) * k;
      const glm::vec3 p = base + im.dir * (0.45f * easeOut(0.0f, 1.0f, u) * k) + kUp * ((0.1f + 0.35f * easeOut(0.0f, 1.0f, u)) * k);
      const float size = (0.25f + 0.6f * easeOut(0.0f, 1.0f, u)) * k * (heavy ? 1.3f : 1.0f);
      const float alpha = 0.5f * ease(0.0f, 0.08f, tau) * (1.0f - ease(0.2f, 1.0f, u));
      const glm::vec3 albedo = glm::vec3(0.46f, 0.4f, 0.33f) * (0.85f + 0.3f * hash01(im.seed, n, 5));
      puffs_.push_back(puff(pulled(p, eye, kGroundPull), size, hash01(im.seed, n, 6) * kTau + tau * 0.6f, hash01(im.seed, n, 7) * 37.0f, alpha, albedo));
    }
    // --- Its light: a warm flash on the struck and around. ---
    const float flash = heavy ? 0.24f : 0.14f;
    if (age < flash) lights.push_back({im.point, {1.0f, 0.74f, 0.45f}, (heavy ? 14.0f : 7.0f) * (1.0f - ease(0.0f, flash, age)), heavy ? 3.2f : 2.4f});
  }

  if (heavy) {
    // --- The pressure of the blow: a pale shell spreading from the contact, a ring rolling over the ground. ---
    if (age < 0.22f) {
      const float r = (0.15f + 0.8f * easeOut(0.0f, 0.2f, age)) * k;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), point) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, now, 0.3f, 0.3f * (1.0f - ease(0.04f, 0.22f, age)), {1.0f, 0.85f, 0.7f}, 0.04f,
           glm::vec4(1.0f, 0.97f, 0.92f, 0.0f), im.feet, VfxBlend::Additive);
    }
    if (age < 0.45f) {
      const float r = (0.3f + 1.6f * easeOut(0.0f, 0.4f, age)) * k;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(im.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) *
                          glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      draw(discFirst_, discCount_, m, kModeRing, now, 0.22f, 0.5f * (1.0f - ease(0.1f, 0.45f, age)), {0.95f, 0.72f, 0.48f}, 0.0f,
           glm::vec4(1.0f, 0.86f, 0.66f, 0.88f), im.feet, VfxBlend::Additive);
    }
    // --- A ring of dust rolling out from the feet. ---
    for (int i = 0; i < 14; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 200u;
      const float life = 1.2f * (0.85f + 0.3f * hash01(im.seed, n, 1));
      const float tau = age - 0.02f;
      if (tau < 0.0f || tau > life) continue;
      const float u = tau / life;
      const float a = (static_cast<float>(i) + 0.4f * hash01(im.seed, n, 2)) / 14.0f * kTau;
      const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
      const glm::vec3 p = im.feet + out * ((0.2f + 1.3f * easeOut(0.0f, 0.7f, u)) * k) + kUp * ((0.08f + 0.18f * u) * k);
      const float alpha = 0.45f * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.25f, 1.0f, u));
      puffs_.push_back(puff(pulled(p, eye, kGroundPull), (0.3f + 0.55f * easeOut(0.0f, 1.0f, u)) * k, a + tau, hash01(im.seed, n, 3) * 37.0f, alpha,
                            glm::vec3(0.45f, 0.39f, 0.32f) * (0.85f + 0.3f * hash01(im.seed, n, 4))));
    }
    // --- Debris: chips of the ground thrown on arcs, bouncing once, then lying still and fading. ---
    for (int i = 0; i < 10; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 300u;
      const float tau = age;
      if (tau > 1.6f) continue;
      const float a = hash01(im.seed, n, 1) * kTau;
      const glm::vec3 radial{std::cos(a), 0.0f, std::sin(a)};
      const glm::vec3 h = glm::normalize(im.dir * 0.9f + radial * 0.8f + side * (hash01(im.seed, n, 2) - 0.5f));
      const glm::vec3 vh = h * (1.0f + 1.8f * hash01(im.seed, n, 3)) * k;
      const float vy = (2.4f + 2.0f * hash01(im.seed, n, 4)) * k;
      const glm::vec3 start = im.feet + im.dir * 0.2f + glm::vec3(0.0f, 0.05f, 0.0f);
      // First flight until it meets the ground again, then a low hop, then rest.
      const float t1 = 2.0f * vy / kGravity, vy2 = vy * 0.3f, t2 = 2.0f * vy2 / kGravity;
      glm::vec3 p;
      float spin = tau;
      if (tau < t1) {
        p = start + vh * tau + glm::vec3(0.0f, vy * tau - 0.5f * kGravity * tau * tau, 0.0f);
      } else if (tau < t1 + t2) {
        const float s = tau - t1;
        p = start + vh * t1 + vh * 0.4f * s + glm::vec3(0.0f, vy2 * s - 0.5f * kGravity * s * s, 0.0f);
      } else {
        p = start + vh * t1 + vh * 0.4f * t2;
        spin = t1 + t2;
      }
      const float fade = 1.0f - ease(1.1f, 1.6f, tau);
      if (fade <= 0.0f) continue;
      const float size = (0.035f + 0.05f * hash01(im.seed, n, 5)) * k * fade;
      const glm::vec3 axis = sphereDir(hash01(im.seed, n, 6), hash01(im.seed, n, 7));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p, eye, kGroundPull)) *
                          glm::rotate(glm::mat4(1.0f), spin * (8.0f + 10.0f * hash01(im.seed, n, 8)), axis) * glm::scale(glm::mat4(1.0f), glm::vec3(size));
      VfxInstance r{m, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(glm::vec3(0.34f, 0.3f, 0.26f) * (0.75f + 0.5f * hash01(im.seed, n, 9)), 1.0f)};
      rocks_.push_back(r);
    }
  }

  if (im.kind == Kind::Slash || im.kind == Kind::HeavySlash || im.kind == Kind::Thrust) {
    // --- The cut: a sheet of steel light swept along the blade's path across the body. A slash runs on a diagonal
    // arc in the plane square to the blow (bowed toward the striker, so it has depth); a thrust runs straight along
    // the blow. The sheet is drawn in as the blade passes (head), its tail following, then it fades. ---
    const bool thrust = im.kind == Kind::Thrust, big = im.kind == Kind::HeavySlash;
    constexpr float kSweep = 0.09f, kHold = 0.06f, kFade = 0.22f;
    const float head = easeOut(0.0f, kSweep, age);
    const float tail = ease(kSweep * 0.5f, kSweep + kHold + kFade, age);
    const float opacity = 1.0f - ease(kSweep + kHold, kSweep + kHold + kFade, age);
    if (opacity > 0.0f && head > tail) {
      // Which way the blade goes: from the striker's upper side down across (mirrored by the seed, so a counter or a
      // second blow may cut the other way).
      const float hand = hash01(im.seed, 0, 31) < 0.5f ? 1.0f : -1.0f;
      constexpr int kSegments = 28;
      std::vector<VfxVertex> sheet;
      sheet.reserve(kSegments * 6);
      std::vector<glm::vec3> edge(kSegments + 1), inner(kSegments + 1);
      const float reach = (big ? 0.72f : 0.55f) * k, width = (big ? 0.2f : 0.14f) * k;
      const float tilt = glm::radians(38.0f);
      const glm::vec3 e1 = side * std::cos(tilt) * hand + kUp * std::sin(tilt);    // along the cut
      const glm::vec3 e2 = -side * std::sin(tilt) * hand + kUp * std::cos(tilt);   // across it, in the same plane
      for (int j = 0; j <= kSegments; ++j) {
        const float s = static_cast<float>(j) / kSegments;
        const float w = width * std::pow(std::sin(std::numbers::pi_v<float> * s), 0.6f) + 0.004f;
        glm::vec3 p, out;
        if (thrust) {
          // straight along the blow, from in front of the body through it
          p = im.point + im.dir * glm::mix(-0.75f * reach, 0.35f * reach, s);
          out = kUp;
        } else {
          // a 150 degree arc around the contact, from upper back to lower front
          const float a = glm::radians(glm::mix(75.0f, -75.0f, s));
          const glm::vec3 radial = e1 * std::sin(a) + e2 * std::cos(a);
          p = im.point + radial * reach - im.dir * (0.22f * k * std::cos(a));  // bowed toward the striker
          out = radial;
        }
        edge[j] = pulled(p, eye, kPull);
        inner[j] = pulled(p - out * w, eye, kPull);
      }
      const glm::vec3 normal = thrust ? side : im.dir;
      for (int j = 0; j < kSegments; ++j) {
        const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
        const VfxVertex a0{inner[j], s0, normal, 0.0f}, a1{edge[j], s0, normal, 1.0f}, b0{inner[j + 1], s1, normal, 0.0f}, b1{edge[j + 1], s1, normal, 1.0f};
        sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
      }
      const glm::vec3 steel{0.78f, 0.86f, 1.0f}, white{1.0f, 0.98f, 0.94f};
      // color.a = the tail, params1.a = the head (shares of the path)
      draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, now, big ? 1.5f : 1.2f, opacity, steel, tail,
           glm::vec4(white, head), im.feet, VfxBlend::Additive);
    }
    // --- A few sparks where the edge bites, thrown along the cut. ---
    const int sparks = big ? 10 : 6;
    for (int i = 0; i < sparks; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 700u;
      const float delay = 0.02f + hash01(im.seed, n, 6) * 0.04f;
      const float life = 0.14f + 0.18f * hash01(im.seed, n, 5);
      const float tau = age - delay;
      if (tau < 0.0f || tau > life) continue;
      const glm::vec3 d = glm::normalize(im.dir * (0.4f + 0.5f * hash01(im.seed, n, 1)) + side * (hash01(im.seed, n, 2) - 0.5f) * 2.0f +
                                         kUp * (0.1f + 0.7f * hash01(im.seed, n, 3)));
      const glm::vec3 v0 = d * (2.5f + 3.5f * hash01(im.seed, n, 4)) * k;
      const glm::vec3 p = im.point + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
      if (p.y < im.feet.y + 0.02f) continue;
      const glm::vec3 v = v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f);
      const float u = tau / life;
      streaks_.push_back(streak(pulled(p, eye, kPull), v * 0.05f, 0.03f * k, 1.0f - u * u,
                                glm::mix(glm::vec3(1.0f, 0.95f, 0.8f), glm::vec3(1.0f, 0.55f, 0.18f), u), 3.2f));
    }
    // --- A cold glint of light on the struck. ---
    if (age < 0.16f) lights.push_back({im.point, {0.85f, 0.9f, 1.0f}, (big ? 9.0f : 5.0f) * (1.0f - ease(0.0f, 0.16f, age)), 2.2f});
  }

  if (im.kind == Kind::ArrowHit || im.kind == Kind::HeavyArrowHit) {
    const bool big = im.kind == Kind::HeavyArrowHit;
    // --- The point punches through: a thin streak of light along the shot, from just in front of the body into it,
    // there and gone in a blink. ---
    constexpr float kSweep = 0.05f, kFade = 0.14f;
    const float head = easeOut(0.0f, kSweep, age), tail = ease(kSweep * 0.4f, kSweep + kFade, age);
    const float opacity = 1.0f - ease(kSweep, kSweep + kFade, age);
    if (opacity > 0.0f && head > tail) {
      constexpr int kSegments = 12;
      const float reach = (big ? 0.6f : 0.45f) * k, width = (big ? 0.06f : 0.045f) * k;
      std::vector<VfxVertex> sheet;
      sheet.reserve(kSegments * 6);
      // the sheet holds the shot's line and faces the eye as well as it can, so it never turns edge-on
      glm::vec3 across = glm::cross(im.dir, glm::normalize(eye - im.point));
      across = glm::dot(across, across) > 1e-6f ? glm::normalize(across) : kUp;
      for (int j = 0; j < kSegments; ++j) {
        const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
        auto at = [&](float s, float sideOf) {
          const float w = width * std::sin(std::numbers::pi_v<float> * s) + 0.003f;
          return pulled(im.point + im.dir * glm::mix(-0.8f * reach, 0.2f * reach, s) + across * (w * sideOf), eye, kPull);
        };
        const VfxVertex a0{at(s0, -1.0f), s0, im.dir, 0.0f}, a1{at(s0, 1.0f), s0, im.dir, 1.0f}, b0{at(s1, -1.0f), s1, im.dir, 0.0f},
            b1{at(s1, 1.0f), s1, im.dir, 1.0f};
        sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
      }
      draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, now, 1.1f, opacity, {0.95f, 0.9f, 0.82f}, tail,
           glm::vec4(1.0f, 0.98f, 0.94f, head), im.feet, VfxBlend::Additive);
    }
    // --- A pin of light where the point goes in. ---
    if (age < 0.08f) {
      const float f = 1.0f - ease(0.0f, 0.08f, age);
      streaks_.push_back(streak(point, glm::vec3(0.0f), (big ? 0.28f : 0.2f) * k, f, {1.0f, 0.86f, 0.62f}, 1.6f));
    }
    // --- Splinters of the shaft kicked back toward the archer, turning, falling to the ground, lying a moment. ---
    const int count = big ? 9 : 6;
    for (int i = 0; i < count && age < 0.9f; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 800u;
      const glm::vec3 d = glm::normalize(-im.dir * (0.5f + 0.6f * hash01(im.seed, n, 1)) + side * (hash01(im.seed, n, 2) - 0.5f) * 1.8f +
                                         kUp * (0.4f + 0.9f * hash01(im.seed, n, 3)));
      const glm::vec3 v0 = d * (1.2f + 1.6f * hash01(im.seed, n, 4)) * k;
      const float tLand = (v0.y + std::sqrt(v0.y * v0.y + 2.0f * kGravity * std::max(im.point.y - im.feet.y, 0.0f))) / kGravity;
      const float tau = std::min(age, tLand);
      const glm::vec3 p = im.point + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
      const float fade = 1.0f - ease(0.6f, 0.9f, age);
      const float len = (0.07f + 0.06f * hash01(im.seed, n, 5)) * k * fade, thick = 0.012f * k * fade;
      const glm::vec3 axis = sphereDir(hash01(im.seed, n, 6), hash01(im.seed, n, 7));
      const float spin = tau * (14.0f + 12.0f * hash01(im.seed, n, 8));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p, eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), spin, axis) *
                          glm::scale(glm::mat4(1.0f), {thick, len, thick});
      splinters_.push_back({m, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(glm::vec3(0.5f, 0.36f, 0.22f) * (0.8f + 0.4f * hash01(im.seed, n, 9)), 1.0f)});
    }
    if (age < 0.1f) lights.push_back({im.point, {1.0f, 0.82f, 0.6f}, (big ? 5.0f : 3.0f) * (1.0f - ease(0.0f, 0.1f, age)), 1.8f});
  }

  if (im.kind == Kind::Magical) {
    // --- The flash of the school's colour. ---
    if (age < 0.13f) {
      const float f = 1.0f - ease(0.0f, 0.13f, age);
      streaks_.push_back(streak(point, glm::vec3(0.0f), 1.0f * k * (0.5f + 0.5f * f), f, pal.core, 2.6f));
    }
    // --- The core: it flares out, then collapses into itself. ---
    if (age < 0.45f) {
      const float r = k * (0.12f + 0.34f * easeOut(0.0f, 0.08f, age)) * (1.0f - 0.85f * ease(0.1f, 0.42f, age));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), point) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, now, 1.5f, 1.0f - ease(0.28f, 0.45f, age), pal.energy, 0.14f, glm::vec4(pal.core, 1.0f), im.feet,
           VfxBlend::Additive);
    }
    // --- The pressure shell: a thin sphere of energy spreading through the space around the body. ---
    if (age < 0.38f) {
      const float r = k * (0.2f + 1.05f * easeOut(0.0f, 0.34f, age));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), point) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, now, 0.85f, 0.65f * (1.0f - ease(0.05f, 0.38f, age)), pal.energy, 0.07f, glm::vec4(pal.core, 0.0f),
           im.feet, VfxBlend::Additive);
    }
    // --- The ground answers: a ring of the school's light spreading from the feet. ---
    if (age < 0.5f) {
      const float r = (0.35f + 1.05f * easeOut(0.0f, 0.42f, age)) * k;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(im.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) *
                          glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
      draw(discFirst_, discCount_, m, kModeRing, now, 0.7f, 0.75f * (1.0f - ease(0.12f, 0.5f, age)), pal.energy, 0.0f, glm::vec4(pal.core, 0.85f), im.feet,
           VfxBlend::Additive);
    }
    // --- Energy bursting out in 3D, slowing as it spends itself (away from the caster more than toward it). ---
    for (int i = 0, bursts = static_cast<int>(28.0f * im.strength + 0.5f); i < bursts; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 400u;
      const float life = 0.3f + 0.35f * hash01(im.seed, n, 1);
      const float tau = age - hash01(im.seed, n, 2) * 0.025f;
      if (tau < 0.0f || tau > life) continue;
      const glm::vec3 d = glm::normalize(sphereDir(hash01(im.seed, n, 3), hash01(im.seed, n, 4)) + im.dir * 0.8f);
      const float speed = (2.6f + 4.2f * hash01(im.seed, n, 5)) * k;
      constexpr float kDrag = 5.0f;
      const glm::vec3 p = im.point + d * speed * (1.0f - std::exp(-kDrag * tau)) / kDrag + kUp * (0.3f * tau);
      const glm::vec3 v = d * speed * std::exp(-kDrag * tau);
      const float u = tau / life;
      streaks_.push_back(streak(pulled(p, eye, kPull), v * 0.06f, 0.03f * k, (1.0f - u) * (1.0f - u), glm::mix(pal.core, pal.energy, std::min(1.0f, u * 1.6f)),
                                2.8f));
    }
    // --- Motes left in the air, drifting up and out. ---
    for (int i = 0, motes = static_cast<int>(12.0f * im.strength + 0.5f); i < motes; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 500u;
      const float life = 0.6f + 0.5f * hash01(im.seed, n, 1);
      const float tau = age - 0.05f;
      if (tau < 0.0f || tau > life) continue;
      const float u = tau / life;
      const glm::vec3 d = sphereDir(hash01(im.seed, n, 2), hash01(im.seed, n, 3));
      const float a = tau * (2.0f + 2.0f * hash01(im.seed, n, 4));
      const glm::vec3 swirl = glm::vec3(std::cos(a), 0.0f, std::sin(a)) * 0.08f;
      const glm::vec3 p = im.point + d * (0.15f + 0.5f * easeOut(0.0f, 1.0f, u)) * k + swirl + kUp * (0.35f * u * k);
      streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.06f * k, std::sin(std::numbers::pi_v<float> * u), pal.energy, 2.2f));
    }
    // --- Its light: strong at the flash, dying out in half a second; it colours the struck unit and the ground. ---
    if (age < 0.6f) lights.push_back({im.point, pal.light, 18.0f * im.strength * std::exp(-age * 6.0f) * ease(0.0f, 0.02f, age + 0.01f), 3.6f * k});
  }

  if (im.kind == Kind::MagicalMiss) {
    // The bolt finds nothing: its energy comes apart past the body, a few sparks, a faint light.
    if (age < 0.25f) {
      const float r = k * 0.14f * (1.0f - ease(0.0f, 0.25f, age));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), point + im.dir * 0.3f) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(sphereFirst_, sphereCount_, m, kModeEnergy, now, 1.2f, 1.0f - ease(0.1f, 0.25f, age), pal.energy, 0.18f, glm::vec4(pal.core, 1.0f), im.feet,
           VfxBlend::Additive);
    }
    for (int i = 0; i < 12; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 600u;
      const float life = 0.35f + 0.3f * hash01(im.seed, n, 1);
      if (age > life) continue;
      const glm::vec3 d = glm::normalize(sphereDir(hash01(im.seed, n, 2), hash01(im.seed, n, 3)) + im.dir * 1.6f);
      const float speed = (1.5f + 2.0f * hash01(im.seed, n, 4)) * k;
      const glm::vec3 p = im.point + im.dir * 0.3f + d * speed * (1.0f - std::exp(-4.0f * age)) / 4.0f;
      const float u = age / life;
      streaks_.push_back(streak(pulled(p, eye, kPull), d * speed * std::exp(-4.0f * age) * 0.05f, 0.025f * k, (1.0f - u), pal.energy, 2.0f));
    }
    if (age < 0.3f) lights.push_back({im.point, pal.light, 6.0f * im.strength * (1.0f - ease(0.0f, 0.3f, age)), 2.5f});
  }
}

void CombatFx::buildBolt(const Bolt& b, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  if (now < b.start) return;
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const CombatPalette& pal = b.palette;
  const float k = std::clamp(b.casterHeight / 2.0f, 0.7f, 1.4f) * std::sqrt(b.strength);  // a lesser bolt: smaller (impact's rule)
  const glm::vec3 hand = pulled(b.hand, eye, kPull);

  // No charging at the hand: the casters' own sprites already gather the spell (user, 2026-10-17: "fica em dose dupla").

  // --- The launch: a flare at the hand as it lets go. ---
  if (now >= b.release && now < b.release + 0.14f) {
    const float f = 1.0f - ease(b.release, b.release + 0.14f, now);
    streaks_.push_back(streak(hand, glm::vec3(0.0f), 0.55f * k * (0.5f + 0.5f * f), f, pal.core, 2.6f));
    for (int i = 0; i < 6; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + 50u;
      const glm::vec3 d = glm::normalize(sphereDir(hash01(b.seed, n, 1), hash01(b.seed, n, 2)) - b.dir * 0.6f);
      const float tau = now - b.release;
      const glm::vec3 p = hand + d * (2.5f * k) * (1.0f - std::exp(-8.0f * tau)) / 8.0f;
      streaks_.push_back(streak(p, d * 0.12f * k * f, 0.02f * k, f, pal.energy, 2.4f));
    }
  }

  // --- The flight: the bolt, its shell, its trail, two motes spiralling round it, its light. ---
  if (now >= b.release && now < b.impact) {
    const float s = b.progress(now);
    const glm::vec3 head = b.at(s);
    const glm::vec3 ahead = b.at(std::min(1.0f, s + 0.02f));
    const glm::vec3 vdir = glm::length(ahead - head) > 1e-5f ? glm::normalize(ahead - head) : b.dir;
    const glm::vec3 headDrawn = pulled(head, eye, kPull);
    const float r = 0.085f * k;
    const glm::mat4 core = glm::translate(glm::mat4(1.0f), headDrawn) * alignY(vdir) * glm::scale(glm::mat4(1.0f), {r, r * 2.4f, r});
    draw(sphereFirst_, sphereCount_, core, kModeEnergy, now, 1.7f, 1.0f, pal.energy, 0.06f, glm::vec4(pal.core, 1.0f), head, VfxBlend::Additive);
    const glm::mat4 shell = glm::translate(glm::mat4(1.0f), headDrawn) * alignY(vdir) * glm::scale(glm::mat4(1.0f), {r * 2.2f, r * 3.4f, r * 2.2f});
    draw(sphereFirst_, sphereCount_, shell, kModeEnergy, now, 0.7f, 0.75f, pal.energy, 0.12f, glm::vec4(pal.core, 0.0f), head, VfxBlend::Additive);
    // The trail: the path behind the head, from tail to head (u 0 -> 1), as a tube.
    const float tail = std::min(s, 0.3f);
    if (tail > 0.01f) {
      three::CatmullRom path;
      for (int j = 0; j <= 8; ++j) path.points.push_back(pulled(b.at(s - tail + tail * static_cast<float>(j) / 8.0f), eye, kPull));
      const std::vector<VfxVertex> tube = vfxgeo::tube(path, 18, 0.045f * k, 8);
      draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, now, 1.1f, 0.85f, pal.energy, 0.0f,
           glm::vec4(pal.core, 0.0f), head, VfxBlend::Additive);
    }
    const glm::vec3 e1 = glm::normalize(glm::cross(vdir, std::abs(vdir.y) < 0.9f ? kUp : glm::vec3(1, 0, 0))), e2 = glm::cross(vdir, e1);
    for (int i = 0; i < 2; ++i) {
      const float phi = now * 24.0f + static_cast<float>(i) * std::numbers::pi_v<float>;
      const glm::vec3 off = (e1 * std::cos(phi) + e2 * std::sin(phi)) * 0.15f * k;
      const glm::vec3 tangent = (-e1 * std::sin(phi) + e2 * std::cos(phi)) * 0.15f * k * 24.0f;
      streaks_.push_back(streak(pulled(head - vdir * 0.05f + off, eye, kPull), tangent * 0.03f - vdir * 0.12f, 0.02f * k, 0.9f, pal.core, 2.6f));
    }
    lights.push_back({head, pal.light, 12.0f * b.strength, 3.2f * k});
  }

  // --- Embers shed along the path, drifting and dying after the bolt has passed. ---
  for (int i = 0, embers = static_cast<int>(16.0f * b.strength + 0.5f); i < embers; ++i) {
    const uint32_t n = static_cast<uint32_t>(i) + 70u;
    const float ub = (static_cast<float>(i) + 0.5f) / 16.0f;
    const float tb = b.release + ub * (b.impact - b.release);
    const float tau = now - tb, life = 0.35f + 0.25f * hash01(b.seed, n, 1);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 d = sphereDir(hash01(b.seed, n, 2), hash01(b.seed, n, 3));
    const glm::vec3 p = b.at(0.3f * ub + 0.7f * ub * ub) + d * 0.35f * tau + glm::vec3(0.0f, -0.4f * tau * tau, 0.0f);
    streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.04f * k * (1.0f - 0.5f * u), (1.0f - u) * 0.9f, pal.energy, 2.2f));
  }
}

}  // namespace ember
