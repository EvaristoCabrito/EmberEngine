#include "render/vfx/StatusFx.h"

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
constexpr float kRingUnderFeet = 0.6f;  // a ring this near its unit's feet height lies on the ground
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeRing = 3.0f, kModeTrail = 4.0f;

float ease(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
float hash01(uint32_t seed, uint32_t i, uint32_t salt) {
  uint32_t h = seed ^ (i * 0x9e3779b9u) ^ (salt * 0x85ebca6bu);
  h ^= h >> 16, h *= 0x7feb352du, h ^= h >> 15, h *= 0x846ca68bu, h ^= h >> 16;
  return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}
glm::vec3 pulled(glm::vec3 p, glm::vec3 eye, float by) {
  const glm::vec3 d = eye - p;
  const float len = glm::length(d);
  return len > by + 0.5f ? p + d * (by / len) : p;
}
// A point round the body: on the eye's side it is pulled over the card, on the far side it stays behind it, so the vapour wraps
// the figure instead of lying on it.
glm::vec3 wrapped(glm::vec3 p, glm::vec3 feet, glm::vec3 eye) {
  glm::vec3 off = p - feet, toEye = eye - feet;
  off.y = 0.0f, toEye.y = 0.0f;
  const float r = glm::length(off), te = glm::length(toEye);
  const float facing = r > 1e-4f && te > 1e-4f ? glm::dot(off, toEye) / (r * te) : 1.0f;
  return pulled(p, eye, kPull * ease(-0.3f, 0.3f, facing));
}
float bodyHeight(float h) { return std::clamp(h > 0.0f ? h : 1.8f, 1.0f, 4.0f); }

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

// Ember's two colours of each (drawStatusFx core / dark), a hot white-tinted heart for the motes, the vapour's own albedo.
struct Palette {
  glm::vec3 core, dark, hot, vapour;
};
constexpr Palette kPoison{{0.41f, 0.93f, 0.45f}, {0.09f, 0.44f, 0.25f}, {0.82f, 1.0f, 0.72f}, {0.2f, 0.42f, 0.16f}};
constexpr Palette kDisease{{0.69f, 0.36f, 0.96f}, {0.31f, 0.13f, 0.49f}, {0.92f, 0.8f, 1.0f}, {0.27f, 0.15f, 0.35f}};

}  // namespace

int StatusFx::start(Kind kind, glm::vec3 feet, float height, float now) {
  Effect e;
  e.kind = kind, e.id = ++serial_, e.feet = feet, e.height = height, e.start = now;
  e.seed = 0x51a7u + 7919u * static_cast<uint32_t>(serial_);
  effects_.push_back(e);
  return serial_;
}
void StatusFx::move(int id, glm::vec3 feet, float height) {
  for (Effect& e : effects_)
    if (e.id == id) e.feet = feet, e.height = height;
}
void StatusFx::end(int id, float now) {
  for (Effect& e : effects_)
    if (e.id == id && e.finish < 0.0f) e.finish = now;
}

void StatusFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.finish >= 0.0f && now > e.finish + 0.8f; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  for (const Effect& e : effects_) buildOne(e, now, camera, frame, lights);
}

void StatusFx::buildOne(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const bool poison = e.kind == Kind::Poison;
  const Palette& pal = poison ? kPoison : kDisease;
  const glm::vec3 eye = camera.position;
  const float H = bodyHeight(e.height), k = std::clamp(H / 2.0f, 0.7f, 1.4f);
  const float age = now - e.start;
  const float env = ease(0.0f, 0.6f, age) * (e.finish < 0.0f ? 1.0f : 1.0f - ease(e.finish, e.finish + 0.7f, now));
  if (env <= 0.0f) return;
  const float phase0 = hash01(e.seed, 0, 9) * kTau;
  const float pulse = 0.5f + 0.5f * std::sin(age * (poison ? 3.8f : 2.5f) + phase0);  // Ember's beat: acid quick, sickness slow
  const float speed = poison ? 0.72f : 0.48f;
  const Draw draw{frame};

  // --- The vapour: puffs of the colour rising and spreading round the body, lit by the scene (dark at night). Poison climbs
  // quick and thin; disease hangs heavier and lower round the chest. ---
  puffs_.clear();
  const int puffCount = poison ? 9 : 12;
  const float life = poison ? 2.2f : 3.4f;
  for (int j = 0; j < puffCount; ++j) {
    const uint32_t n = static_cast<uint32_t>(j);
    const float u = std::fmod(hash01(e.seed, n, 1) + age / life, 1.0f);
    const float ang = hash01(e.seed, n, 2) * kTau + age * (poison ? 0.5f : 0.3f) * (j % 2 ? 1.0f : -1.0f);
    const float r = (0.16f + 0.3f * u) * k * (0.7f + 0.6f * hash01(e.seed, n, 3));
    const float y = H * (poison ? 0.05f + 0.8f * u : 0.1f + 0.5f * u);
    const glm::vec3 p = e.feet + glm::vec3(std::cos(ang) * r, y, std::sin(ang) * r);
    const float size = (0.26f + 0.42f * u) * k * (poison ? 1.0f : 1.15f);
    const float alpha = std::pow(std::sin(kPi * u), 1.2f) * (poison ? 0.16f : 0.21f) * env;
    puffs_.push_back(puff(wrapped(p, e.feet, eye), size, hash01(e.seed, n, 4) * kTau + age * 0.3f, hash01(e.seed, n, 5) * 10.0f, alpha,
                          glm::mix(pal.vapour, pal.dark, 0.35f * hash01(e.seed, n, 6))));
  }
  {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), e.feet, VfxBlend::Alpha, first,
         static_cast<uint32_t>(puffs_.size()));
  }

  // --- The stain on the ground: a pool of the colour and a ring at its edge, beating; a second, wider and fainter, turning. ---
  {
    const float r = (0.48f + 0.05f * pulse) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) *
                        glm::rotate(glm::mat4(1.0f), age * 0.25f, kUp) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.55f, (0.3f + 0.16f * pulse) * env, pal.core, 0.0f, glm::vec4(pal.hot, 0.62f), e.feet, VfxBlend::Additive);
  }
  {
    const float r = (0.72f + 0.06f * pulse) * k;
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.feet + glm::vec3(0.0f, 0.03f, 0.0f), eye, kGroundPull)) *
                        glm::rotate(glm::mat4(1.0f), -age * 0.18f, kUp) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.35f, 0.14f * env, pal.dark, 0.0f, glm::vec4(pal.core, 0.9f), e.feet, VfxBlend::Additive);
  }

  // --- The wisps: thin tubes of vapour climbing the sides in a slow spiral, wavering (Ember's two vapour lines, three here,
  // round the body). The tube's bright head is its foot; its current is run backwards so it climbs. ---
  for (int w = 0; w < 3; ++w) {
    const float base = phase0 + static_cast<float>(w) * kTau / 3.0f + age * (poison ? 0.35f : 0.22f);
    const float breathe = 0.6f + 0.4f * std::sin(age * (poison ? 1.4f : 0.9f) + static_cast<float>(w) * 2.1f);
    three::CatmullRom path;
    constexpr int kPoints = 7;
    for (int j = kPoints; j >= 0; --j) {  // from the top (the tail) down to the foot (the head)
      const float p = static_cast<float>(j) / kPoints;
      const float ang = base + p * 1.1f;
      const float r = k * (0.24f + 0.1f * std::sin(age * (poison ? 2.2f : 1.45f) + p * 7.0f + phase0 + static_cast<float>(w)) + 0.06f * p);
      path.points.push_back(wrapped(e.feet + glm::vec3(std::cos(ang) * r, H * (0.06f + p * 0.72f), std::sin(ang) * r), e.feet, eye));
    }
    const std::vector<VfxVertex> tube = vfxgeo::tube(path, 20, 0.016f * k, 6);
    draw(frame.addVertices(tube), static_cast<uint32_t>(tube.size()), glm::mat4(1.0f), kModeTrail, -age * (poison ? 0.8f : 0.5f), 1.1f,
         (poison ? 0.32f : 0.24f) * breathe * env, pal.core, 0.0f, glm::vec4(pal.hot, 0.0f), e.feet, VfxBlend::Additive);
  }

  // --- The motes: dots of light spiralling from the feet to the head, bright at the bottom and fading as they rise. ---
  motes_.clear();
  constexpr int kMotes = 10;
  for (int i = 0; i < kMotes; ++i) {
    const float fi = static_cast<float>(i);
    const float rise = std::fmod(age * speed + fi * 0.137f + phase0 * 0.11f, 1.0f);
    const float wave = age * (1.8f + static_cast<float>(i % 3) * 0.21f) + fi * 2.37f + phase0;
    const float ang = fi * kTau / kMotes + wave * 0.35f;
    const float r = (0.2f + static_cast<float>(i % 4) * 0.06f) * k * (1.0f + 0.25f * std::sin(wave));
    const glm::vec3 p = e.feet + glm::vec3(std::cos(ang) * r, H * (0.08f + rise * 0.82f), std::sin(ang) * r);
    const float alpha = (0.18f + (1.0f - rise) * 0.38f) * (poison ? 1.0f : 0.82f) * ease(0.0f, 0.08f, rise) * env * 1.6f;
    motes_.push_back(streak(wrapped(p, e.feet, eye), glm::vec3(0.0f), (i % 3 == 0 ? 0.075f : 0.05f) * k, alpha, pal.core, 2.2f));
  }
  {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), motes_.begin(), motes_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(pal.hot, 0.0f), e.feet, VfxBlend::Additive, first,
         static_cast<uint32_t>(motes_.size()));
  }

  // --- A faint light of the colour on the body and the ground, beating with it. ---
  lights.push_back({e.feet + kUp * (H * 0.4f), pal.core, (1.0f + 0.5f * pulse) * env, 2.4f * k});
}

}  // namespace ember
