#include "render/vfx/SpellBolts.h"

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
// A unit's card is drawn 1.2 units nearer in depth: what happens on a body is pulled toward the eye to show over it (CombatFx).
constexpr float kPull = 1.3f;
// The Combat material's modes (shaders/vfx.hlsl) and the Holy material's.
constexpr float kStreak = 0.0f, kEnergy = 2.0f, kRing = 3.0f, kTrail = 4.0f;
constexpr float kShaft = 0.0f, kGlow = 1.0f, kMote = 2.0f;

struct Palette {
  glm::vec3 light, core, energy;
};
constexpr Palette kArcane{{0.62f, 0.32f, 1.0f}, {0.97f, 0.9f, 1.0f}, {0.55f, 0.24f, 1.0f}};
constexpr Palette kDivine{{1.0f, 0.8f, 0.48f}, {1.0f, 0.97f, 0.86f}, {1.0f, 0.68f, 0.24f}};

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
// A rotation taking +y to `axis`.
glm::mat4 alignY(glm::vec3 axis) {
  const glm::vec3 y = glm::normalize(axis);
  const glm::vec3 helper = std::abs(y.y) < 0.95f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 x = glm::normalize(glm::cross(helper, y)), z = glm::cross(x, y);
  glm::mat4 m(1.0f);
  m[0] = glm::vec4(x, 0.0f), m[1] = glm::vec4(y, 0.0f), m[2] = glm::vec4(z, 0.0f);
  return m;
}

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
// An open cylinder of radius 1 from y 0 to 1 (u around, v up), normals outward: the column of light.
const std::vector<VfxVertex>& shaftGeometry() {
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

// One draw of the Combat or Holy material (params0 = time, mode, intensity, opacity).
struct Draw {
  VfxFrame& frame;
  VfxMaterial material;
  float time;
  void operator()(uint32_t first, size_t count, const glm::mat4& model, float mode, float intensity, float opacity, glm::vec3 color, float colorA,
                  glm::vec4 params1, glm::vec3 sight, VfxBlend blend = VfxBlend::Additive, const std::vector<VfxInstance>* list = nullptr) const {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {time, mode, intensity, opacity};
    d.gpu.params1 = params1;
    d.gpu.color = glm::vec4(color, colorA);
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    if (list) {
      if (list->empty()) return;
      d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), list->begin(), list->end());
      d.instances = static_cast<uint32_t>(list->size());
    }
    d.gpu.material = static_cast<uint32_t>(material);
    d.gpu.sightAt = sight;
    d.blend = blend;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
};
// A Combat streak (a spark along its motion, or a dot when `axis` is shorter than its width).
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}
// A Holy mote (a soft dot of light facing the eye).
VfxInstance mote(glm::vec3 centre, float size, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, size), glm::vec4(0.0f, 0.0f, alpha, intensity), glm::vec4(tint, 0.0f)};
}

}  // namespace

SpellBolt::SpellBolt(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, std::vector<glm::vec3> splash,
                     float release, float impact, uint32_t seed, float scale)
    : style_(style), splash_(std::move(splash)), release_(release), impact_(std::max(impact, release + 0.1f)), seed_(seed),
      scale_(std::clamp(scale, 0.2f, 1.0f)) {
  casterHeight_ = std::clamp(casterHeight, 1.0f, 4.0f), targetHeight_ = std::clamp(targetHeight, 1.0f, 4.0f);
  dir_ = flatDir(targetFeet - casterFeet, glm::vec3(0.0f, 0.0f, 1.0f));
  side_ = glm::normalize(glm::cross(kUp, dir_));
  // The Arcano bolt leaves the hand at the chest; the lance of light is thrown from above the head.
  hand_ = casterFeet + kUp * (casterHeight_ * (style == Style::Divine ? 0.85f : 0.55f)) + dir_ * 0.35f;
  body_ = targetFeet + kUp * (targetHeight_ * 0.5f) - dir_ * 0.15f;
  feet_ = targetFeet;
  swerve_ = hash01(seed, 1, 1) < 0.5f ? -1.0f : 1.0f;
}

float SpellBolt::flightSeconds(Style style, glm::vec3 fromFeet, glm::vec3 toFeet) {
  const float d = glm::length(glm::vec2(toFeet.x - fromFeet.x, toFeet.z - fromFeet.z));
  return style == Style::Divine ? std::clamp(0.18f + d * 0.05f, 0.25f, 0.65f) : std::clamp(0.3f + d * 0.09f, 0.45f, 1.15f);
}

float SpellBolt::progress(float now) const {
  const float u = std::clamp((now - release_) / (impact_ - release_), 0.0f, 1.0f);
  return style_ == Style::Divine ? 0.55f * u + 0.45f * u * u : 0.25f * u + 0.75f * u * u;
}

glm::vec3 SpellBolt::at(float s) const {
  const float dist = glm::length(body_ - hand_);
  const float bump = 4.0f * s * (1.0f - s);
  if (style_ == Style::Divine) return glm::mix(hand_, body_, s) + kUp * (std::min(dist * 0.03f, 0.25f) * bump);
  // Arcano: an arc up and a swerve to one side, the swerve easing back in as it homes.
  const float arc = std::clamp(dist * 0.1f, 0.15f, 0.8f), sway = std::clamp(dist * 0.12f, 0.2f, 0.9f);
  return glm::mix(hand_, body_, s) + kUp * (arc * bump) + side_ * (swerve_ * sway * std::sin(kPi * s) * (1.0f - 0.35f * s));
}

void SpellBolt::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < release_ || done(now)) return;
  if (style_ == Style::Divine) buildDivine(now, camera, frame, lights);
  else buildArcane(now, camera, frame, lights);
}

void SpellBolt::buildArcane(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const Palette& pal = kArcane;
  const Draw draw{frame, VfxMaterial::Combat, now};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(casterHeight_ / 2.0f, 0.7f, 1.4f) * scale_;  // a small bolt: every size with it
  const uint32_t quad = frame.addVertices(quadGeometry()), sphere = frame.addVertices(sphereGeometry()), disc = frame.addVertices(discGeometry());
  const size_t quadN = quadGeometry().size(), sphereN = sphereGeometry().size(), discN = discGeometry().size();
  std::vector<VfxInstance> streaks;
  auto tangent = [&](float s) {
    const glm::vec3 d = at(std::min(1.0f, s + 0.01f)) - at(std::max(0.0f, s - 0.01f));
    return glm::dot(d, d) > 1e-10f ? glm::normalize(d) : dir_;
  };
  auto ring = [&](glm::vec3 centre, glm::vec3 normal, float radius, float share, float opacity, float intensity, glm::vec3 sight) {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), centre) * alignY(normal) * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
    draw(disc, discN, m, kRing, intensity, opacity, pal.energy, 0.0f, glm::vec4(pal.core, share), sight);
  };
  const glm::vec3 hand = pulled(hand_, eye, kPull);

  // --- The launch: a flare, a ring of force round the hand, sparks. ---
  if (now < release_ + 0.22f) {
    const float f = 1.0f - ease(release_, release_ + 0.22f, now);
    streaks.push_back(streak(hand, glm::vec3(0.0f), 0.6f * k * (0.4f + 0.6f * f), f, pal.core, 2.8f));
    ring(hand, tangent(0.0f), (0.1f + 0.5f * easeOut(release_, release_ + 0.22f, now)) * k, 0.82f, f, 1.6f, hand_);
    for (uint32_t i = 0; i < 8; ++i) {
      const glm::vec3 d = glm::normalize(sphereDir(hash01(seed_, i, 3), hash01(seed_, i, 4)) + dir_ * 0.5f);
      const float tau = now - release_;
      streaks.push_back(streak(hand + d * (2.8f * k) * (1.0f - std::exp(-9.0f * tau)) / 9.0f, d * 0.14f * k * f, 0.022f * k, f, pal.energy, 2.4f));
    }
  }

  // --- The flight. ---
  if (now < impact_) {
    const float s = progress(now);
    const glm::vec3 head = at(s), vdir = tangent(s);
    const glm::vec3 headDrawn = pulled(head, eye, kPull);
    const float r = 0.09f * k;
    // The core (white-hot, stretched along the flight) inside a boiling shell.
    draw(sphere, sphereN, glm::translate(glm::mat4(1.0f), headDrawn) * alignY(vdir) * glm::scale(glm::mat4(1.0f), {r, r * 2.6f, r}), kEnergy, 1.9f, 1.0f,
         pal.energy, 0.06f, glm::vec4(pal.core, 1.0f), head);
    draw(sphere, sphereN, glm::translate(glm::mat4(1.0f), headDrawn) * alignY(vdir) * glm::scale(glm::mat4(1.0f), {r * 2.4f, r * 3.6f, r * 2.4f}), kEnergy,
         0.75f, 0.8f, pal.energy, 0.14f, glm::vec4(pal.core, 0.0f), head);
    // The trail, then two ribbons twisting round the path behind the head (a helix, widening toward the head).
    const float tail = std::min(s, 0.32f);
    if (tail > 0.01f) {
      three::CatmullRom path;
      for (int j = 0; j <= 10; ++j) path.points.push_back(pulled(at(s - tail + tail * static_cast<float>(j) / 10.0f), eye, kPull));
      const std::vector<VfxVertex> tube = vfxgeo::tube(path, 20, 0.05f * k, 8);
      draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kTrail, 1.2f, 0.85f, pal.energy, 0.0f, glm::vec4(pal.core, 0.0f), head);
    }
    const float helixTail = std::min(s, 0.45f);
    if (helixTail > 0.02f)
      for (int i = 0; i < 2; ++i) {
        three::CatmullRom path;
        for (int j = 0; j <= 24; ++j) {
          const float a = static_cast<float>(j) / 24.0f, q = s - helixTail + helixTail * a;
          const glm::vec3 t = tangent(q);
          const glm::vec3 e1 = glm::normalize(glm::cross(t, std::abs(t.y) < 0.9f ? kUp : glm::vec3(1, 0, 0))), e2 = glm::cross(t, e1);
          const float phi = q * 46.0f - now * 9.0f + static_cast<float>(i) * kPi;
          path.points.push_back(pulled(at(q) + (e1 * std::cos(phi) + e2 * std::sin(phi)) * (0.15f * k * (0.3f + 0.7f * a)), eye, kPull));
        }
        const std::vector<VfxVertex> tube = vfxgeo::tube(path, 40, 0.017f * k, 5);
        draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kTrail, 1.5f, 0.9f, glm::mix(pal.energy, pal.core, 0.35f), 0.0f,
             glm::vec4(pal.core, 0.0f), head);
      }
    // Rings of force rippling off the wake, one every 0.08 s.
    for (int w = 0; w < 20; ++w) {
      const float tw = release_ + 0.05f + 0.08f * static_cast<float>(w);
      if (tw >= now || tw >= impact_) break;
      const float age = now - tw;
      if (age > 0.32f) continue;
      const float sw = progress(tw);
      ring(pulled(at(sw), eye, kPull), tangent(sw), (0.08f + 0.32f * easeOut(0.0f, 0.32f, age)) * k, 0.85f, (1.0f - age / 0.32f) * 0.75f, 1.3f, at(sw));
    }
    lights.push_back({head, pal.light, 14.0f * scale_, 3.4f * k});
  }

  // --- Embers shed along the path, drifting and dying behind it. ---
  for (uint32_t i = 0; i < 24; ++i) {
    const float ub = (static_cast<float>(i) + 0.5f) / 24.0f;
    const float tb = release_ + ub * (impact_ - release_);
    const float tau = now - tb, life = 0.35f + 0.3f * hash01(seed_, i, 7);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life;
    const glm::vec3 d = sphereDir(hash01(seed_, i, 8), hash01(seed_, i, 9));
    const glm::vec3 p = at(progress(tb)) + d * 0.4f * tau + glm::vec3(0.0f, -0.35f * tau * tau, 0.0f);
    streaks.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.04f * k * (1.0f - 0.5f * u), (1.0f - u) * 0.9f, pal.energy, 2.2f));
  }

  // --- The hit: a flash, a shell of force, three gyroscope rings, energy flung out, a ground ring, motes, the light. ---
  const float age = now - impact_;
  if (age >= 0.0f) {
    const glm::vec3 point = pulled(body_, eye, kPull);
    if (age < 0.18f) streaks.push_back(streak(point, glm::vec3(0.0f), 1.1f * k * std::sqrt(1.0f - age / 0.18f), 1.0f - age / 0.18f, pal.core, 3.0f));
    if (age < 0.45f) {
      const float r = (0.15f + 0.95f * easeOut(0.0f, 0.45f, age)) * k;
      draw(sphere, sphereN, glm::translate(glm::mat4(1.0f), point) * glm::scale(glm::mat4(1.0f), glm::vec3(r)), kEnergy, 1.2f,
           1.0f - ease(0.05f, 0.45f, age), pal.energy, 0.1f, glm::vec4(pal.core, 0.0f), body_);
    }
    if (age < 0.7f)
      for (uint32_t i = 0; i < 3; ++i) {
        glm::vec3 axis = sphereDir(0.2f + 0.6f * hash01(seed_, i, 11), hash01(seed_, i, 12));
        const float spin = age * (5.0f + 2.0f * static_cast<float>(i)) * (i == 1 ? -1.0f : 1.0f);
        axis = glm::vec3(glm::rotate(glm::mat4(1.0f), spin, kUp) * glm::vec4(axis, 0.0f));
        ring(point, axis, (0.2f + 1.0f * easeOut(0.0f, 0.6f, age)) * k, 0.9f, 1.0f - ease(0.1f, 0.7f, age), 1.6f, body_);
      }
    for (uint32_t i = 0; i < 28; ++i) {
      const float life = 0.4f + 0.25f * hash01(seed_, i, 13);
      if (age > life) continue;
      const float u = age / life, speed = (2.6f + 2.0f * hash01(seed_, i, 14)) * k;
      const glm::vec3 d = sphereDir(hash01(seed_, i, 15), hash01(seed_, i, 16));
      const glm::vec3 p = point + d * speed * (1.0f - std::exp(-6.0f * age)) / 6.0f;
      streaks.push_back(streak(p, d * speed * std::exp(-6.0f * age) * 0.06f, 0.03f * k, 1.0f - u, i % 3 ? pal.energy : pal.core, 2.4f));
    }
    if (age < 0.6f) ring(feet_ + glm::vec3(0.0f, 0.04f, 0.0f), kUp, (0.3f + 1.4f * easeOut(0.0f, 0.6f, age)) * k, 0.88f, 1.0f - ease(0.15f, 0.6f, age), 1.4f, feet_);
    for (uint32_t i = 0; i < 14; ++i) {
      const float life = 1.1f;
      if (age > life) continue;
      const float a0 = hash01(seed_, i, 17) * kTau + age * 5.0f, rad = (0.25f + 0.5f * age) * k;
      const glm::vec3 p = body_ + glm::vec3(std::cos(a0) * rad, -0.3f * k + age * 1.2f, std::sin(a0) * rad);
      streaks.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.045f * k, std::sin(kPi * age / life) * 0.85f, pal.core, 2.0f));
    }
    if (age < 1.0f) lights.push_back({body_, pal.light, 26.0f * scale_ * std::exp(-5.0f * age), 4.2f * k});
  }
  draw(quad, quadN, glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(1.0f, 0.97f, 0.95f, 0.0f), body_, VfxBlend::Additive, &streaks);
}

void SpellBolt::buildDivine(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const Palette& pal = kDivine;
  const Draw combat{frame, VfxMaterial::Combat, now}, holy{frame, VfxMaterial::Holy, now};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(casterHeight_ / 2.0f, 0.7f, 1.4f);
  const uint32_t quad = frame.addVertices(quadGeometry()), sphere = frame.addVertices(sphereGeometry()), disc = frame.addVertices(discGeometry());
  const uint32_t shaft = frame.addVertices(shaftGeometry());
  const size_t quadN = quadGeometry().size(), sphereN = sphereGeometry().size(), discN = discGeometry().size(), shaftN = shaftGeometry().size();
  std::vector<VfxInstance> streaks, motes;
  const glm::vec3 hand = pulled(hand_, eye, kPull);
  const glm::vec3 flight = glm::normalize(body_ - hand_);

  // --- The launch: a golden flare and a halo square to the throw. ---
  if (now < release_ + 0.25f) {
    const float f = 1.0f - ease(release_, release_ + 0.25f, now);
    streaks.push_back(streak(hand, glm::vec3(0.0f), 0.7f * k * (0.4f + 0.6f * f), f, pal.core, 3.0f));
    const float r = (0.15f + 0.45f * easeOut(release_, release_ + 0.25f, now)) * k;
    combat(disc, discN, glm::translate(glm::mat4(1.0f), hand) * alignY(flight) * glm::scale(glm::mat4(1.0f), glm::vec3(r)), kRing, 1.5f, f, pal.energy,
           0.0f, glm::vec4(pal.core, 0.86f), hand_);
    for (uint32_t i = 0; i < 6; ++i) {
      const glm::vec3 d = glm::normalize(sphereDir(hash01(seed_, i, 3), hash01(seed_, i, 4)) + flight * 0.4f);
      const float tau = now - release_;
      streaks.push_back(streak(hand + d * (2.0f * k) * (1.0f - std::exp(-8.0f * tau)) / 8.0f, d * 0.1f * k * f, 0.022f * k, f, pal.core, 2.6f));
    }
  }

  // --- The flight: a lance of light, the beam it leaves hanging, feathers of light drifting down off it. ---
  const float s = progress(now);
  const glm::vec3 head = at(s);
  if (now < impact_) {
    const glm::vec3 headDrawn = pulled(head, eye, kPull);
    const float r = 0.06f * k;
    combat(sphere, sphereN, glm::translate(glm::mat4(1.0f), headDrawn - flight * (r * 3.0f)) * alignY(flight) * glm::scale(glm::mat4(1.0f), {r, r * 7.0f, r}),
           kEnergy, 2.2f, 1.0f, pal.energy, 0.03f, glm::vec4(pal.core, 1.0f), head);
    combat(sphere, sphereN,
           glm::translate(glm::mat4(1.0f), headDrawn - flight * (r * 3.0f)) * alignY(flight) * glm::scale(glm::mat4(1.0f), {r * 2.4f, r * 8.5f, r * 2.4f}),
           kEnergy, 0.8f, 0.8f, pal.energy, 0.05f, glm::vec4(pal.core, 0.0f), head);
    lights.push_back({head, pal.light, 12.0f, 3.2f * k});
  }
  // The beam from the hand to the head, lingering a moment after the hit.
  const float beamFade = now < impact_ ? 1.0f : 1.0f - ease(impact_, impact_ + 0.35f, now);
  if (beamFade > 0.0f && s > 0.02f) {
    three::CatmullRom path;
    for (int j = 0; j <= 10; ++j) path.points.push_back(pulled(at(s * static_cast<float>(j) / 10.0f), eye, kPull));
    const std::vector<VfxVertex> tube = vfxgeo::tube(path, 24, 0.032f * k, 8);
    combat(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kTrail, 1.25f, 0.85f * beamFade, pal.energy, 0.0f, glm::vec4(pal.core, 0.0f), head);
  }
  for (uint32_t i = 0; i < 20; ++i) {
    const float ub = (static_cast<float>(i) + 0.5f) / 20.0f;
    const float tb = release_ + ub * (impact_ - release_);
    const float tau = now - tb, life = 0.7f + 0.3f * hash01(seed_, i, 21);
    if (tau < 0.0f || tau > life) continue;
    const float u = tau / life, sway = std::sin(tau * 6.0f + hash01(seed_, i, 22) * kTau) * 0.08f;
    const glm::vec3 p = at(progress(tb)) + side_ * sway + glm::vec3(0.0f, -0.35f * tau, 0.0f);
    motes.push_back(mote(pulled(p, eye, kPull), 0.075f * k * (1.0f - 0.4f * u), std::sin(kPi * u) * 0.9f, i & 1 ? pal.core : pal.energy, 1.4f));
  }

  // --- The hit. ---
  const float age = now - impact_;
  if (age >= 0.0f) {
    float reach = 1.2f;  // the struck area's radius
    for (const glm::vec3& h : splash_) reach = std::max(reach, glm::length(glm::vec2(h.x - feet_.x, h.z - feet_.z)) + 0.9f);
    // A column of light slams down from the sky onto the struck hex, then fades.
    if (age < 0.8f) {
      const float h = 7.0f, lowered = 1.0f - easeOut(0.0f, 0.12f, age);
      const float r = 0.55f * k * (1.0f + 0.35f * ease(0.0f, 0.25f, age));
      const float opacity = 1.0f - ease(0.25f, 0.8f, age);
      const glm::vec3 base = feet_ + kUp * (lowered * h);
      holy(shaft, shaftN, glm::translate(glm::mat4(1.0f), base) * glm::scale(glm::mat4(1.0f), {r, h * (1.0f - lowered) + 1e-3f, r}), kShaft, 0.24f, opacity,
           pal.energy, 1.0f, glm::vec4(pal.core, 0.0f), feet_);
      holy(shaft, shaftN, glm::translate(glm::mat4(1.0f), base) * glm::scale(glm::mat4(1.0f), {r * 0.4f, (h * (1.0f - lowered) + 1e-3f) * 0.9f, r * 0.4f}),
           kShaft, 0.22f, opacity * 0.8f, glm::mix(pal.energy, pal.core, 0.6f), 1.0f, glm::vec4(pal.core, 0.0f), feet_);
    }
    // A radiant burst on the body: a flash and rays flung out low and wide.
    const glm::vec3 point = pulled(body_, eye, kPull);
    if (age < 0.2f) streaks.push_back(streak(point, glm::vec3(0.0f), 1.3f * k * std::sqrt(1.0f - age / 0.2f), 1.0f - age / 0.2f, pal.core, 3.2f));
    for (uint32_t i = 0; i < 24; ++i) {
      const float life = 0.35f + 0.2f * hash01(seed_, i, 31);
      if (age > life) continue;
      const float u = age / life, a = (static_cast<float>(i) + 0.5f * hash01(seed_, i, 32)) / 24.0f * kTau;
      const glm::vec3 d = glm::normalize(glm::vec3(std::cos(a), 0.2f + 0.7f * hash01(seed_, i, 33), std::sin(a)));
      const float speed = (3.0f + 1.5f * hash01(seed_, i, 34)) * k;
      const glm::vec3 p = point + d * speed * (1.0f - std::exp(-5.0f * age)) / 5.0f;
      streaks.push_back(streak(p, d * speed * std::exp(-5.0f * age) * 0.09f, 0.035f * k, (1.0f - u), i % 2 ? pal.energy : pal.core, 2.6f));
    }
    // A shockwave of gold racing over the struck area, the ground glowing under it.
    const glm::vec3 ground = feet_ + glm::vec3(0.0f, 0.04f, 0.0f);
    if (age < 0.65f)
      combat(disc, discN, glm::translate(glm::mat4(1.0f), ground) * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f + reach * easeOut(0.0f, 0.45f, age))), kRing,
             1.6f, 1.0f - ease(0.2f, 0.65f, age), pal.energy, 0.0f, glm::vec4(pal.core, 0.9f), feet_);
    if (age < 1.4f)
      holy(disc, discN, glm::translate(glm::mat4(1.0f), ground) * glm::scale(glm::mat4(1.0f), glm::vec3(reach * 0.9f)), kGlow, 0.3f,
           ease(0.0f, 0.08f, age) * (1.0f - ease(0.4f, 1.4f, age)), pal.energy, 1.0f, glm::vec4(pal.core, 0.25f + 0.6f * ease(0.0f, 0.45f, age)), feet_);
    // Each hex of the splash flares as the wave reaches it, motes rising off it.
    uint32_t n = 0;
    for (const glm::vec3& h : splash_) {
      const float d = glm::length(glm::vec2(h.x - feet_.x, h.z - feet_.z));
      if (d < 0.5f) continue;
      const float local = age - 0.45f * std::min(1.0f, d / reach);
      if (local < 0.0f || local > 0.9f) continue;
      holy(disc, discN, glm::translate(glm::mat4(1.0f), h + glm::vec3(0.0f, 0.045f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.85f)), kGlow, 0.28f,
           ease(0.0f, 0.06f, local) * (1.0f - ease(0.15f, 0.9f, local)), pal.energy, 1.0f, glm::vec4(pal.core, 0.3f + 0.5f * ease(0.0f, 0.4f, local)), h);
      for (uint32_t j = 0; j < 3; ++j, ++n) {
        const float a = hash01(seed_, n, 41) * kTau, r = 0.2f + 0.5f * hash01(seed_, n, 42);
        motes.push_back(mote(h + glm::vec3(std::cos(a) * r, 0.1f + local * 1.3f, std::sin(a) * r), 0.07f, std::sin(kPi * local / 0.9f) * 0.8f, pal.core, 1.4f));
      }
    }
    for (uint32_t i = 0; i < 22; ++i) {
      const float life = 1.2f + 0.3f * hash01(seed_, i, 51);
      if (age > life) continue;
      const float u = age / life, a = hash01(seed_, i, 52) * kTau + age * 1.5f, r = (0.2f + 0.7f * hash01(seed_, i, 53)) * k;
      const glm::vec3 p = feet_ + glm::vec3(std::cos(a) * r, 0.15f + u * 1.8f * k, std::sin(a) * r);
      motes.push_back(mote(pulled(p, eye, kPull * 0.6f), 0.08f * k, std::sin(kPi * u) * 0.9f, i & 1 ? pal.core : pal.energy, 1.5f));
    }
    if (age < 1.2f) lights.push_back({body_, pal.light, 30.0f * std::exp(-3.5f * age), 5.0f * k});
  }
  combat(quad, quadN, glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(1.0f, 0.98f, 0.9f, 0.0f), body_, VfxBlend::Additive, &streaks);
  holy(quad, quadN, glm::mat4(1.0f), kMote, 0.6f, 1.0f, pal.energy, 1.0f, glm::vec4(pal.core, 0.0f), body_, VfxBlend::Additive, &motes);
}

}  // namespace ember
