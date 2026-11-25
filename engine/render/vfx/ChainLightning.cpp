#include "render/vfx/ChainLightning.h"

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
constexpr float kPull = 1.3f;

// The Combat material's modes (shaders/vfx.hlsl).
constexpr float kStreak = 0.0f, kRing = 3.0f, kLightning = 6.0f;
constexpr float kStrokeSeconds = 0.05f, kFlickerSeconds = 0.035f;

constexpr glm::vec3 kLight{0.72f, 0.86f, 1.0f};   // scene dynamic light
constexpr glm::vec3 kEnergy{0.45f, 0.75f, 1.0f};  // the bolt's shell
constexpr glm::vec3 kCore{0.96f, 0.98f, 1.0f};    // its white-hot core

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
glm::vec3 pulled(glm::vec3 p, glm::vec3 eye, float by) {
  const glm::vec3 d = eye - p;
  const float len = glm::length(d);
  return len > by + 0.5f ? p + d * (by / len) : p;
}

const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, -0.5f, 0.0f}, 1.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f},
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f}, {{-0.5f, 0.5f, 0.0f}, 0.0f, {0, 0, 1}, 1.0f}};
  return g;
}

const std::vector<VfxVertex>& discGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 48, out = 8;
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

struct Draw {
  VfxFrame& frame;
  float time;
  void operator()(uint32_t first, size_t count, const glm::mat4& model, float mode, float intensity, float opacity, glm::vec3 color, glm::vec4 params1,
                  glm::vec3 sight, const std::vector<VfxInstance>* list = nullptr) const {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {time, mode, intensity, opacity};
    d.gpu.params1 = params1;
    d.gpu.color = glm::vec4(color, 0.0f);
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    if (list) {
      if (list->empty()) return;
      d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), list->begin(), list->end());
      d.instances = static_cast<uint32_t>(list->size());
    }
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
    d.gpu.sightAt = sight;
    d.blend = VfxBlend::Additive;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
};

VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}

three::CatmullRom jagged(glm::vec3 a, glm::vec3 b, int points, float amp, float wander, uint32_t seed, uint32_t variant, glm::vec3 eye, float pull = kPull) {
  three::CatmullRom path;
  const glm::vec3 d = glm::length(b - a) > 1e-4f ? glm::normalize(b - a) : glm::vec3(1, 0, 0);
  const glm::vec3 e1 = glm::normalize(glm::cross(d, std::abs(d.y) < 0.9f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f))), e2 = glm::cross(d, e1);
  const float phase = hash01(seed, 1, 90) * kTau, phase2 = hash01(seed, 2, 90) * kTau;
  for (int k = 0; k < points; ++k) {
    const float t = static_cast<float>(k) / static_cast<float>(points - 1);
    glm::vec3 p = glm::mix(a, b, t);
    if (k > 0 && k + 1 < points) {
      const float taper = std::pow(std::sin(kPi * t), 0.6f);
      const uint32_t i = static_cast<uint32_t>(k);
      const float big = (hash01(seed, i, 91) - 0.5f) * 2.0f, big2 = (hash01(seed, i, 92) - 0.5f) * 2.0f;
      const float small = (hash01(seed ^ (variant * 0x51ed27u), i, 93) - 0.5f) * 2.0f, small2 = (hash01(seed ^ (variant * 0x51ed27u), i, 94) - 0.5f) * 2.0f;
      p += (e1 * (std::sin(t * kTau * 1.3f + phase) * wander + (big * 0.7f + small * 0.5f) * amp) +
            e2 * (std::sin(t * kTau * 1.1f + phase2) * wander + (big2 * 0.7f + small2 * 0.5f) * amp)) *
           taper;
    }
    path.points.push_back(pull > 0.0f ? pulled(p, eye, pull) : p);
  }
  return path;
}

}  // namespace

ChainLightning::ChainLightning(glm::vec3 casterFeet, glm::vec3 casterBody, float casterHeight,
                               std::vector<Target> chain, float start, float impact, uint32_t seed)
    : casterFeet_(casterFeet), casterBody_(casterBody), casterHeight_(std::clamp(casterHeight, 1.0f, 4.0f)),
      chain_(std::move(chain)), start_(start), impact_(std::max(impact, start + 0.1f)), seed_(seed) {
  if (casterBody_ == glm::vec3(0.0f)) casterBody_ = casterFeet_ + kUp * (casterHeight_ * 0.6f);
}

void ChainLightning::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  buildCaster(now, camera, frame, lights);
  for (size_t i = 0; i < chain_.size(); ++i) {
    buildArc(i, now, camera, frame, lights);
  }
}

void ChainLightning::buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float release = impact_ + 0.15f;
  if (now >= release) return;
  const Draw draw{frame, now};
  const glm::vec3 eye = camera.position;
  const float f = ease(start_, impact_, now), fade = 1.0f - ease(impact_, release, now);
  const uint32_t quad = frame.addVertices(quadGeometry()), disc = frame.addVertices(discGeometry());
  std::vector<VfxInstance> streaks;
  const glm::vec3 hand = pulled(casterBody_, eye, kPull);
  const uint32_t flicker = static_cast<uint32_t>(std::max(0.0f, (now - start_) / 0.04f));

  // (no ring on the ground: never in any lightning; the user, 2026-11-25)

  // Central flare and electric corona at the caster's hands
  const float flare = (0.3f + 0.5f * f) * (0.8f + 0.2f * hash01(seed_, flicker, 5));
  streaks.push_back(streak(hand, glm::vec3(0.0f), flare, (0.5f + 0.5f * f) * fade, kCore, 2.8f));
  for (uint32_t i = 0; i < 6; ++i) {
    const glm::vec3 d = sphereDir(hash01(seed_, i + flicker * 7, 6), hash01(seed_, i + flicker * 7, 7));
    const float u = hash01(seed_, i + flicker * 7, 8);
    streaks.push_back(streak(hand + d * (0.2f + 0.6f * u), d * 0.12f, 0.025f, (1.0f - u) * fade, kEnergy, 2.5f));
  }

  // Small electric arcs jumping off the caster
  for (uint32_t a = 0; a < 2; ++a) {
    const glm::vec3 d = glm::normalize(sphereDir(hash01(seed_, a + flicker * 3, 9), hash01(seed_, a + flicker * 3, 10)) + kUp * 0.4f);
    const three::CatmullRom path = jagged(casterBody_, casterBody_ + d * (0.6f + 0.4f * f), 6, 0.1f, 0.0f, seed_ + a * 977u, flicker, eye);
    const std::vector<VfxVertex> tube = vfxgeo::tube(path, 16, 0.013f, 5);
    draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kLightning, 3.4f, (0.4f + 0.6f * f) * fade, kEnergy, glm::vec4(kCore, 1.2f), casterBody_);
  }
  draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(0.9f, 0.95f, 1.0f, 0.0f), casterBody_, &streaks);
  lights.push_back({casterBody_, kLight, (5.0f + 16.0f * f) * fade, 4.0f});
}

void ChainLightning::buildArc(size_t index, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float tStrike = impact_ + kStagger * static_cast<float>(index);
  const float age = now - tStrike;
  if (age < 0.0f || age > 0.48f) return;

  const Draw draw{frame, tStrike + age};
  const glm::vec3 eye = camera.position;
  const float scale = std::pow(0.88f, static_cast<float>(index));
  const Target& cur = chain_[index];
  const glm::vec3 to = cur.body;
  const glm::vec3 toFeet = cur.feet;
  const glm::vec3 from = (index == 0) ? casterBody_ : chain_[index - 1].body;
  const float dist = std::max(glm::length(to - from), 0.5f);

  const uint32_t seed = seed_ + static_cast<uint32_t>(index) * 0x19a3bu;
  const uint32_t variant = static_cast<uint32_t>(age / kFlickerSeconds) % 3u;

  const float lit = easeOut(0.0f, kStrokeSeconds, age) * 1.2f;
  const float stroke = std::exp(-std::pow((age - 0.16f) / 0.025f, 2.0f));
  const float opacity = std::clamp((1.0f - ease(0.10f, 0.44f, age) + 0.6f * stroke) * scale, 0.0f, 1.5f);

  if (opacity > 0.01f) {
    // 3D jagged main arc connecting the two bodies
    const int pts = std::clamp(static_cast<int>(dist * 4.0f), 12, 28);
    const float amp = std::clamp(dist * 0.08f, 0.15f, 0.45f) * scale;
    const three::CatmullRom bolt = jagged(from, to, pts, amp, 0.15f * scale, seed, variant, eye);

    // Inner core and outer glow halo tubes
    const std::vector<VfxVertex> core = vfxgeo::tube(bolt, pts * 3, 0.024f * scale, 5);
    const std::vector<VfxVertex> glow = vfxgeo::tube(bolt, pts * 3, 0.085f * scale, 6);
    draw(frame.addVertices(glow), glow.size(), glm::mat4(1.0f), kLightning, 1.8f, 0.55f * opacity, kEnergy, glm::vec4(kCore, lit), to);
    draw(frame.addVertices(core), core.size(), glm::mat4(1.0f), kLightning, 5.5f, opacity, kEnergy, glm::vec4(kCore, lit), to);

    // Branching forks flicking off the main arc
    for (uint32_t b = 0; b < 2; ++b) {
      const float t0 = 0.3f + 0.4f * (static_cast<float>(b) + hash01(seed, b, 21)) / 2.0f;
      const glm::vec3 forkFrom = bolt.getPoint(t0);
      const float a = hash01(seed, b, 22) * kTau, run = (0.8f + 1.1f * hash01(seed, b, 23)) * scale;
      const glm::vec3 forkTo = forkFrom + glm::vec3(std::cos(a) * run, (hash01(seed, b, 24) - 0.5f) * 1.2f * scale, std::sin(a) * run);
      const three::CatmullRom fork = jagged(forkFrom, forkTo, 6, 0.14f * scale, 0.0f, seed + 43u * (b + 1), variant, eye, 0.0f);
      const std::vector<VfxVertex> tube = vfxgeo::tube(fork, 18, 0.013f * scale, 5);
      const std::vector<VfxVertex> halo = vfxgeo::tube(fork, 18, 0.042f * scale, 6);
      draw(frame.addVertices(halo), halo.size(), glm::mat4(1.0f), kLightning, 1.3f, 0.45f * opacity, kEnergy, glm::vec4(kCore, lit), to);
      draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kLightning, 3.5f, 0.85f * opacity, kEnergy, glm::vec4(kCore, lit), to);
    }

    // Crawling arcs on the struck body
    if (age < 0.28f && age >= 0.02f) {
      const float arcFade = 1.0f - ease(0.08f, 0.28f, age);
      const float bodyH = std::max(cur.height, 1.4f);
      for (uint32_t a = 0; a < 3; ++a) {
        const glm::vec3 d0 = sphereDir(hash01(seed, a + variant * 5, 31), hash01(seed, a + variant * 5, 32));
        const glm::vec3 d1 = sphereDir(hash01(seed, a + variant * 5, 33), hash01(seed, a + variant * 5, 34));
        const glm::vec3 p0 = to + glm::vec3(d0.x * 0.22f, d0.y * bodyH * 0.35f, d0.z * 0.22f) * scale;
        const glm::vec3 p1 = to + glm::vec3(d1.x * 0.45f, d1.y * bodyH * 0.4f, d1.z * 0.45f) * scale;
        const three::CatmullRom arc = jagged(p0, p1, 5, 0.07f * scale, 0.0f, seed + 67u * (a + 1), variant, eye);
        const std::vector<VfxVertex> tube = vfxgeo::tube(arc, 14, 0.011f * scale, 5);
        draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kLightning, 3.2f, 0.8f * arcFade, kEnergy, glm::vec4(kCore, 1.2f), to);
      }
    }
  }

  // Impact flash, ground shockwave, and energetic electric sparks
  const uint32_t quad = frame.addVertices(quadGeometry()), disc = frame.addVertices(discGeometry());
  std::vector<VfxInstance> streaks;
  const glm::vec3 impactPoint = pulled(to, eye, kPull);

  // (no ring on the ground: never in any lightning; the user, 2026-11-25)

  // Camera-facing core impact flash
  const float flashFade = 1.0f - ease(0.0f, 0.18f, age);
  if (flashFade > 0.01f) {
    streaks.push_back(streak(impactPoint, glm::vec3(0.0f), 0.65f * scale, flashFade, kCore, 3.5f));
  }

  // 14 energetic electric sparks flung outward
  for (uint32_t s = 0; s < 14; ++s) {
    const float sparkLife = 0.2f + 0.18f * hash01(seed, s, 41);
    if (age > sparkLife) continue;
    const float tt = age / sparkLife;
    const float spd = (3.5f + 3.0f * hash01(seed, s, 42)) * scale;
    const glm::vec3 dir = sphereDir(hash01(seed, s, 43), hash01(seed, s, 44));
    const glm::vec3 pos = to + dir * (spd * age) - glm::vec3(0.0f, 4.5f * age * age, 0.0f);
    const glm::vec3 vel = dir * (spd * 0.04f);
    streaks.push_back(streak(pulled(pos, eye, kPull), vel, 0.024f * scale, (1.0f - tt) * scale, kEnergy, 2.8f));
  }

  if (!streaks.empty()) {
    draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(0.9f, 0.95f, 1.0f, 0.0f), to, &streaks);
  }

  // Point light at the hit body
  const float lightInt = 32.0f * scale * (1.0f - ease(0.0f, 0.35f, age));
  if (lightInt > 0.1f) {
    lights.push_back({to, kLight, lightInt, 4.5f * scale});
  }
}

}  // namespace ember
