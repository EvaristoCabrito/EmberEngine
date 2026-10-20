#include "render/vfx/Lightning.h"

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
// The Combat material's modes (shaders/vfx.hlsl).
constexpr float kStreak = 0.0f, kRing = 3.0f, kLightning = 6.0f;
constexpr float kStrokeSeconds = 0.06f, kFlickerSeconds = 0.04f;

constexpr glm::vec3 kLight{0.72f, 0.86f, 1.0f};   // the light it throws
constexpr glm::vec3 kEnergy{0.5f, 0.72f, 1.0f};   // the bolt's shell
constexpr glm::vec3 kCore{0.96f, 0.98f, 1.0f};    // its white-hot thread

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

// One draw of the Combat material (params0 = time, mode, intensity, opacity).
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
// A Combat streak (a spark along its motion, or a dot when `axis` is shorter than its width).
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}

// A jagged line from `a` to `b`: `points` points, each pushed sideways by up to `amp` (zero at both ends, wandering slowly as a whole:
// `wander`), the jitter drawn from (seed, variant). Every point pulled toward the eye by `pull` so the bolt shows over the cards (0 = not).
three::CatmullRom jagged(glm::vec3 a, glm::vec3 b, int points, float amp, float wander, uint32_t seed, uint32_t variant, glm::vec3 eye, float pull = kPull) {
  three::CatmullRom path;
  const glm::vec3 d = glm::normalize(b - a);
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

Lightning::Lightning(glm::vec3 casterFeet, float casterHeight, std::vector<Target> targets, float start, float impact, float scale, uint32_t seed)
    : feet_(casterFeet), casterHeight_(std::clamp(casterHeight, 1.0f, 4.0f)), scale_(scale), start_(start), impact_(std::max(impact, start + 0.1f)),
      targets_(std::move(targets)), seed_(seed) {
  hand_ = casterFeet + kUp * (casterHeight_ * 0.7f);
}

void Lightning::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  buildCaster(now, camera, frame, lights);
  for (size_t i = 0; i < targets_.size(); ++i) {
    const float age = now - (impact_ + kStagger * static_cast<float>(i));
    if (age >= 0.0f) buildStrike(i, age, camera, frame, lights);
  }
}

// The cast gathers: a ring on the ground, a crackling flare and arcs at the hand, sparks, a cold light rising.
void Lightning::buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float release = impact_ + 0.12f;
  if (now >= release) return;
  const Draw draw{frame, now};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(casterHeight_ / 2.0f, 0.7f, 1.4f) * scale_;
  const float f = ease(start_, impact_, now), fade = 1.0f - ease(impact_, release, now);
  const uint32_t quad = frame.addVertices(quadGeometry()), disc = frame.addVertices(discGeometry());
  std::vector<VfxInstance> streaks;
  const glm::vec3 hand = pulled(hand_, eye, kPull);
  const uint32_t flicker = static_cast<uint32_t>(std::max(0.0f, (now - start_) / 0.05f));

  draw(disc, discGeometry().size(), glm::translate(glm::mat4(1.0f), feet_ + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3((0.35f + 0.5f * f) * k)),
       kRing, 1.4f, (0.35f + 0.65f * f) * fade, kEnergy, glm::vec4(kCore, 0.86f), feet_);
  const float flare = (0.25f + 0.45f * f) * k * (0.75f + 0.25f * hash01(seed_, flicker, 5));
  streaks.push_back(streak(hand, glm::vec3(0.0f), flare, (0.5f + 0.5f * f) * fade, kCore, 2.6f));
  for (uint32_t i = 0; i < 5; ++i) {
    const glm::vec3 d = sphereDir(hash01(seed_, i + flicker * 7, 6), hash01(seed_, i + flicker * 7, 7));
    const float u = hash01(seed_, i + flicker * 7, 8);
    streaks.push_back(streak(hand + d * (0.25f + 0.5f * u) * k, d * 0.1f * k, 0.02f * k, (1.0f - u) * fade, kEnergy, 2.4f));
  }
  // Two small arcs jumping off the hand, a new shape every 0.05 s.
  for (uint32_t a = 0; a < 2; ++a) {
    const glm::vec3 d = glm::normalize(sphereDir(hash01(seed_, a + flicker * 3, 9), hash01(seed_, a + flicker * 3, 10)) + kUp * 0.5f);
    const three::CatmullRom path = jagged(hand_, hand_ + d * (0.55f + 0.4f * f) * k, 6, 0.09f * k, 0.0f, seed_ + a * 977u, flicker, eye);
    const std::vector<VfxVertex> tube = vfxgeo::tube(path, 18, 0.012f * k, 5);
    draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kLightning, 3.2f, (0.4f + 0.6f * f) * fade, kEnergy, glm::vec4(kCore, 1.2f), hand_);
  }
  draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(0.9f, 0.95f, 1.0f, 0.0f), hand_, &streaks);
  lights.push_back({hand_, kLight, (4.0f + 14.0f * f) * fade, 3.5f * k});
}

// One bolt on one body.
void Lightning::buildStrike(size_t index, float age, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const Target& target = targets_[index];
  const Draw draw{frame, impact_ + age};
  const glm::vec3 eye = camera.position;
  const float k = scale_;
  const float height = std::clamp(target.height, 1.0f, 4.0f);
  const uint32_t seed = seed_ + static_cast<uint32_t>(index) * 0x9e37u;
  const glm::vec3 feet = target.feet;
  const glm::vec3 body = feet + kUp * (height * 0.5f);
  const glm::vec3 head = feet + kUp * (height * 0.9f);
  const uint32_t variant = static_cast<uint32_t>(age / kFlickerSeconds) % 3u;

  // How far down the bolt has come (it falls from the sky in a few hundredths of a second) and how bright it is: it holds, flashes
  // again (the return stroke) and fades.
  const float lit = easeOut(0.0f, kStrokeSeconds, age) * 1.15f;
  const float stroke = std::exp(-std::pow((age - 0.19f) / 0.025f, 2.0f));
  const float opacity = std::clamp(1.0f - ease(0.12f, 0.42f, age) + 0.7f * stroke, 0.0f, 1.5f);

  if (opacity > 0.01f) {
    // The bolt: from a point high in the sky, leaning to one side, to the head of the body.
    const float side = (hash01(seed, 1, 1) - 0.5f) * 3.0f;
    const glm::vec3 lean = glm::normalize(glm::vec3(hash01(seed, 2, 1) - 0.5f, 0.0f, hash01(seed, 3, 1) - 0.5f) + glm::vec3(1e-3f)) * side;
    const glm::vec3 top = feet + kUp * ((7.0f + 2.0f * hash01(seed, 4, 1)) * k + height) + lean;
    const three::CatmullRom bolt = jagged(top, head, 19, 0.28f * k, 0.5f * k, seed, variant, eye);
    const std::vector<VfxVertex> core = vfxgeo::tube(bolt, 57, 0.028f * k, 6), glow = vfxgeo::tube(bolt, 57, 0.1f * k, 8);
    draw(frame.addVertices(glow), glow.size(), glm::mat4(1.0f), kLightning, 1.7f, 0.55f * opacity, kEnergy, glm::vec4(kCore, lit), body);
    draw(frame.addVertices(core), core.size(), glm::mat4(1.0f), kLightning, 5.5f, opacity, kEnergy, glm::vec4(kCore, lit), body);

    // Forks: three branches leaving the bolt in its upper two thirds, running outward and down, thinner and dimmer.
    for (uint32_t b = 0; b < 3; ++b) {
      const float t0 = 0.25f + 0.4f * (static_cast<float>(b) + hash01(seed, b, 21)) / 3.0f;
      const glm::vec3 from = bolt.getPoint(t0);
      const float a = hash01(seed, b, 22) * kTau, run = (1.2f + 1.3f * hash01(seed, b, 23)) * k;
      const glm::vec3 to = from + glm::vec3(std::cos(a) * run, -(0.9f + 1.3f * hash01(seed, b, 24)) * k, std::sin(a) * run);
      const three::CatmullRom fork = jagged(from, to, 7, 0.16f * k, 0.0f, seed + 31u * (b + 1), variant, eye, 0.0f);  // it starts on the bolt, already pulled
      const std::vector<VfxVertex> tube = vfxgeo::tube(fork, 21, 0.014f * k, 5), halo = vfxgeo::tube(fork, 21, 0.045f * k, 6);
      const float forkLit = std::clamp((lit - t0) / 0.3f, 0.0f, 1.2f);
      draw(frame.addVertices(halo), halo.size(), glm::mat4(1.0f), kLightning, 1.3f, 0.45f * opacity, kEnergy, glm::vec4(kCore, forkLit), body);
      draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kLightning, 3.6f, 0.9f * opacity, kEnergy, glm::vec4(kCore, forkLit), body);
    }

    // Arcs crawling over the body while the bolt holds.
    if (age < 0.3f && age >= kStrokeSeconds * 0.8f) {
      const float arcFade = 1.0f - ease(0.14f, 0.3f, age);
      for (uint32_t a = 0; a < 4; ++a) {
        const glm::vec3 d0 = sphereDir(hash01(seed, a + variant * 5, 31), hash01(seed, a + variant * 5, 32));
        const glm::vec3 d1 = sphereDir(hash01(seed, a + variant * 5, 33), hash01(seed, a + variant * 5, 34));
        const glm::vec3 p0 = body + glm::vec3(d0.x * 0.25f, d0.y * height * 0.4f, d0.z * 0.25f) * k;
        const glm::vec3 p1 = body + glm::vec3(d1.x * 0.5f, d1.y * height * 0.45f, d1.z * 0.5f) * k;
        const three::CatmullRom arc = jagged(p0, p1, 5, 0.08f * k, 0.0f, seed + 53u * (a + 1), variant, eye);
        const std::vector<VfxVertex> tube = vfxgeo::tube(arc, 15, 0.011f * k, 5);
        draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kLightning, 3.4f, 0.85f * arcFade, kEnergy, glm::vec4(kCore, 1.2f), body);
      }
    }
  }

  // The flash on the body, a ring and a pool of light racing over the ground, sparks flung out and falling.
  const uint32_t quad = frame.addVertices(quadGeometry()), disc = frame.addVertices(discGeometry());
  std::vector<VfxInstance> streaks;
  const glm::vec3 point = pulled(body, eye, kPull);
  if (age < 0.16f) streaks.push_back(streak(point, glm::vec3(0.0f), 1.8f * k * std::sqrt(1.0f - age / 0.16f), 1.0f - age / 0.16f, kCore, 3.2f));
  if (age < 0.55f) {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), feet + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3((0.35f + 1.6f * easeOut(0.0f, 0.4f, age)) * k));
    draw(disc, discGeometry().size(), m, kRing, 2.3f, 1.0f - ease(0.1f, 0.55f, age), kEnergy, glm::vec4(kCore, 0.88f), feet);
  }
  for (uint32_t i = 0; i < 26; ++i) {
    const float life = 0.35f + 0.3f * hash01(seed, i, 41);
    if (age > life) continue;
    const float u = age / life, speed = (2.8f + 2.6f * hash01(seed, i, 42)) * k;
    glm::vec3 d = sphereDir(hash01(seed, i, 43), hash01(seed, i, 44));
    d.y = std::abs(d.y) * 0.8f + 0.15f;
    d = glm::normalize(d);
    const glm::vec3 p = body + d * speed * (1.0f - std::exp(-6.0f * age)) / 6.0f - kUp * (1.5f * age * age);
    streaks.push_back(streak(pulled(p, eye, kPull), d * speed * std::exp(-6.0f * age) * 0.06f, 0.026f * k, 1.0f - u, i % 3 ? kEnergy : kCore, 2.6f));
  }
  draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(0.9f, 0.95f, 1.0f, 0.0f), body, &streaks);

  // The light it throws: dazzling at the strike, dying away, and the sky above the body lit with it.
  if (age < 0.9f) {
    lights.push_back({body, kLight, 80.0f * k * std::exp(-6.0f * age) + 6.0f * stroke, 9.0f * k});
    lights.push_back({body + kUp * (3.0f * k), kLight, 28.0f * k * std::exp(-9.0f * age), 7.0f * k});
  }
}

}  // namespace ember
