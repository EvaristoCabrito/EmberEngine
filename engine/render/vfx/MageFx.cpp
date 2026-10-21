#include "render/vfx/MageFx.h"

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
constexpr float kModeStreak = 0.0f, kModeEnergy = 2.0f, kModeRing = 3.0f;
constexpr glm::vec3 kViolet{0.62f, 0.4f, 1.0f}, kVioletCore{0.9f, 0.82f, 1.0f}, kWhite{1.0f, 0.98f, 0.94f}, kDeep{0.4f, 0.2f, 0.8f};

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
const std::vector<VfxVertex>& sphereGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::sphere(1.0f, 40, 24);
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

}  // namespace

void MageFx::dispel(glm::vec3 centre, float radius, std::vector<Target> bodies, float impact) {
  Dispel d;
  d.centre = centre, d.radius = std::max(radius, 1.0f), d.impact = impact, d.bodies = std::move(bodies);
  d.seed = 0x3a9eu + 7919u * ++seedSerial_;
  dispels_.push_back(std::move(d));
}
int MageFx::globeStart(glm::vec3 centre, float radius, float now) {
  Globe g;
  g.id = ++serial_, g.centre = centre, g.radius = std::max(radius, 1.0f), g.start = now;
  g.seed = 0x3a9fu + 7919u * ++seedSerial_;
  globes_.push_back(g);
  return g.id;
}
void MageFx::globeEnd(int id, float now) {
  for (Globe& g : globes_)
    if (g.id == id && g.finish < 0.0f) g.finish = now;
}
void MageFx::globeRipple(glm::vec3 at, float now) { ripples_.push_back({at, now, 0x3aa0u + 7919u * ++seedSerial_}); }

bool MageFx::active(float now) const {
  return std::any_of(dispels_.begin(), dispels_.end(), [&](const Dispel& d) { return d.end() > now; }) ||
         std::any_of(ripples_.begin(), ripples_.end(), [&](const Ripple& r) { return r.end() > now; }) ||
         std::any_of(globes_.begin(), globes_.end(), [&](const Globe& g) { return now < g.start + 0.8f; });
}
float MageFx::latestEnd() const {
  float end = -1.0f;
  for (const Dispel& d : dispels_) end = std::max(end, d.end());
  for (const Ripple& r : ripples_) end = std::max(end, r.end());
  for (const Globe& g : globes_) end = std::max(end, g.start + 0.8f);
  return end;
}

void MageFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(dispels_, [&](const Dispel& d) { return d.end() <= now; });
  std::erase_if(ripples_, [&](const Ripple& r) { return r.end() <= now; });
  std::erase_if(globes_, [&](const Globe& g) { return g.finish >= 0.0f && now > g.finish + 0.8f; });
  if (dispels_.empty() && globes_.empty() && ripples_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  streaks_.clear();
  for (const Dispel& d : dispels_)
    if (now >= d.impact - kDispelLead) buildDispel(d, now, camera, frame, lights);
  for (const Globe& g : globes_) buildGlobe(g, now, camera, frame, lights);
  for (const Ripple& r : ripples_) buildRipple(r, now, camera, frame, lights);
  if (!streaks_.empty()) {
    const glm::vec3 sight = !dispels_.empty() ? dispels_.front().centre : !globes_.empty() ? globes_.front().centre : ripples_.front().at;
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    const Draw draw{frame};
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(kVioletCore, 0.0f), sight, VfxBlend::Additive,
         first, static_cast<uint32_t>(streaks_.size()));
  }
}

// --- Dissipar Magia -------------------------------------------------------------------------------------------------------

void MageFx::buildDispel(const Dispel& d, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float R = d.radius;
  const glm::vec3 ground = d.centre + glm::vec3(0.0f, 0.04f, 0.0f);
  const float age = now - d.impact;
  if (age < 0.0f) {
    // The gathering: motes drawn in from the rim to the centre, low over the ground, a light growing.
    const float u = 1.0f + age / kDispelLead;  // 0 .. 1
    for (int j = 0; j < 22; ++j) {
      const uint32_t n = static_cast<uint32_t>(j);
      const float a = hash01(d.seed, n, 1) * kTau + u * 1.2f;
      const float r = R * (1.05f - 0.95f * std::pow(u, 1.4f)) * (0.85f + 0.3f * hash01(d.seed, n, 2));
      const glm::vec3 p = d.centre + glm::vec3(std::cos(a) * r, 0.15f + 0.5f * u + 0.3f * hash01(d.seed, n, 3), std::sin(a) * r);
      streaks_.push_back(streak(pulled(p, eye, kGroundPull), (d.centre - p) * 0.12f, 0.05f, 0.8f * ease(0.0f, 0.15f, u), kViolet, 2.3f));
    }
    lights.push_back({d.centre + kUp * 0.8f, kViolet, 2.5f * u, R});
    return;
  }
  // The shell through the air: a sphere of violet light bursting from the centre out to the area's edge, thin and fading.
  if (age < 0.45f) {
    const float r = 0.2f + (R + 0.4f) * easeOut(0.0f, 0.4f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), d.centre + kUp * 0.1f) * glm::scale(glm::mat4(1.0f), glm::vec3(r, r * 0.75f, r));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, now, 1.1f, 0.7f * (1.0f - ease(0.1f, 0.45f, age)), kViolet, 0.05f, glm::vec4(kVioletCore, 0.0f), d.centre,
         VfxBlend::Additive);
  }
  // The ring on the ground running out to the edge; a second, slower, fainter.
  if (age < 0.7f) {
    const float r = 0.2f + (R + 0.3f) * easeOut(0.0f, 0.5f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(ground, eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.6f, 0.85f * (1.0f - ease(0.2f, 0.7f, age)), kViolet, 0.0f, glm::vec4(kVioletCore, 0.92f), d.centre,
         VfxBlend::Additive);
  }
  if (age < 1.0f) {
    const float r = 0.2f + (R + 0.3f) * easeOut(0.0f, 0.9f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(ground, eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), age * 1.5f, kUp) *
                        glm::scale(glm::mat4(1.0f), {r, 1.0f, r});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.35f, 0.4f * (1.0f - ease(0.3f, 1.0f, age)), kDeep, 0.0f, glm::vec4(kViolet, 0.85f), d.centre, VfxBlend::Additive);
  }
  // The column at the centre: a tall streak of white-violet, then gone.
  if (age < 0.5f) {
    const float f = 1.0f - ease(0.1f, 0.5f, age);
    streaks_.push_back(streak(pulled(d.centre + kUp * 1.4f, eye, kPull), kUp * (2.6f * f), 0.3f * (0.5f + 0.5f * f), 0.9f * f, kVioletCore, 2.6f));
    streaks_.push_back(streak(pulled(d.centre + kUp * 0.3f, eye, kPull), glm::vec3(0.0f), 0.9f * f, 0.8f * f, kVioletCore, 2.4f));
  }
  // On each body: the wave reaches it as the shell passes; a shaft of light, the magic torn off flung up and away.
  for (size_t j = 0; j < d.bodies.size(); ++j) {
    const Target& b = d.bodies[j];
    const float dist = glm::length(glm::vec2(b.feet.x - d.centre.x, b.feet.z - d.centre.z));
    const float reach = 0.4f * std::clamp(dist / (R + 0.4f), 0.0f, 1.0f);  // when the shell gets there
    const float t = age - reach;
    if (t < 0.0f) continue;
    const float h = bodyHeight(b.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
    const glm::vec3 chest = b.feet + kUp * (h * 0.5f);
    const uint32_t salt = 100u + 50u * static_cast<uint32_t>(j);
    if (t < 0.6f) {
      const float f = 1.0f - ease(0.15f, 0.6f, t);
      streaks_.push_back(streak(wrapped(b.feet + kUp * (h * 0.55f), b.feet, eye), kUp * (h * 1.15f * f), 0.26f * k, 0.85f * f, kVioletCore, 2.4f));
      streaks_.push_back(streak(wrapped(chest, b.feet, eye), glm::vec3(0.0f), 0.5f * k * f, 0.9f * (1.0f - ease(0.0f, 0.2f, t)), kWhite, 2.6f));
    }
    for (int i = 0; i < 12; ++i) {
      const uint32_t n = salt + static_cast<uint32_t>(i);
      const float life = 0.5f + 0.35f * hash01(d.seed, n, 1);
      if (t > life) continue;
      const glm::vec3 dir = glm::normalize(sphereDir(hash01(d.seed, n, 2), hash01(d.seed, n, 3)) + kUp * 0.9f);
      const glm::vec3 p = b.feet + kUp * (h * (0.2f + 0.6f * hash01(d.seed, n, 4))) + dir * ((0.9f + 1.4f * hash01(d.seed, n, 5)) * k * t);
      streaks_.push_back(streak(wrapped(p, b.feet, eye), dir * (0.14f * k), 0.045f * k, 0.8f * (1.0f - t / life), i % 3 ? kViolet : kVioletCore, 2.2f));
    }
    if (t < 0.3f) lights.push_back({chest, kVioletCore, 4.0f * (1.0f - ease(0.0f, 0.3f, t)), 2.4f * k});
  }
  lights.push_back({d.centre + kUp * 0.8f, kViolet, 9.0f * (1.0f - ease(0.0f, 0.6f, age)), R + 1.5f});
}

// --- Globo de Invulnerabilidade ------------------------------------------------------------------------------------------------

void MageFx::buildGlobe(const Globe& g, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float age = now - g.start;
  if (age < 0.0f) return;
  const float rise = easeOut(0.0f, 0.55f, age);
  const float env = g.finish < 0.0f ? 1.0f : 1.0f - ease(g.finish, g.finish + 0.7f, now);
  if (env <= 0.0f) return;
  const float R = g.radius * (0.2f + 0.8f * rise) * (0.85f + 0.15f * env);
  const float breathe = 0.5f + 0.5f * std::sin(age * 1.3f + hash01(g.seed, 0, 1) * kTau);
  const glm::vec3 ground = g.centre + glm::vec3(0.0f, 0.04f, 0.0f);
  // The dome: a faint glassy shell with a bright rim, breathing; its bottom is under the ground (the depth test hides it).
  {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), g.centre - kUp * (R * 0.12f)) * glm::scale(glm::mat4(1.0f), glm::vec3(R));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, now * 0.25f, 0.55f + 0.12f * breathe, (0.5f + 0.1f * breathe) * env, kViolet, 0.012f,
         glm::vec4(kVioletCore, 0.1f), g.centre, VfxBlend::Additive);
  }
  // The rising flash: a brighter shell leaving the ground, and a burst at the centre.
  if (age < 0.8f) {
    const float f = 1.0f - ease(0.2f, 0.8f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), g.centre - kUp * (R * 0.12f)) * glm::scale(glm::mat4(1.0f), glm::vec3(R * 1.02f));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, now, 1.2f, 0.7f * f, kVioletCore, 0.03f, glm::vec4(kWhite, 0.0f), g.centre, VfxBlend::Additive);
    streaks_.push_back(streak(pulled(g.centre + kUp * 0.9f, eye, kPull), glm::vec3(0.0f), 1.2f * f, 0.9f * f, kVioletCore, 2.6f));
  }
  // The ring at the base, turning; a thinner one the other way.
  {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(ground, eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), age * 0.35f, kUp) *
                        glm::scale(glm::mat4(1.0f), {R, 1.0f, R});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.5f, (0.4f + 0.12f * breathe) * env, kViolet, 0.0f, glm::vec4(kVioletCore, 0.96f), g.centre, VfxBlend::Additive);
  }
  {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(ground, eye, kGroundPull)) * glm::rotate(glm::mat4(1.0f), -age * 0.2f, kUp) *
                        glm::scale(glm::mat4(1.0f), {R * 0.9f, 1.0f, R * 0.9f});
    draw(discFirst_, discCount_, m, kModeRing, age, 0.3f, 0.18f * env, kDeep, 0.0f, glm::vec4(kViolet, 0.9f), g.centre, VfxBlend::Additive);
  }
  // Motes drifting slowly over the surface of the dome.
  for (int j = 0; j < 26; ++j) {
    const uint32_t n = static_cast<uint32_t>(j) + 20u;
    const float a = hash01(g.seed, n, 1) * kTau + age * (0.08f + 0.1f * hash01(g.seed, n, 2)) * (j & 1 ? 1.0f : -1.0f);
    const float el = (0.12f + 0.8f * hash01(g.seed, n, 3)) * (kPi * 0.5f) + 0.08f * std::sin(age * 0.7f + static_cast<float>(j));
    const glm::vec3 p = g.centre - kUp * (R * 0.12f) + glm::vec3(std::cos(a) * std::cos(el), std::sin(el), std::sin(a) * std::cos(el)) * R;
    if (p.y < g.centre.y + 0.08f) continue;
    const float tw = 0.5f + 0.5f * std::sin(age * 2.1f + hash01(g.seed, n, 4) * kTau);
    streaks_.push_back(streak(p, glm::vec3(0.0f), 0.05f + 0.03f * tw, (0.35f + 0.4f * tw) * env * rise, j % 4 ? kViolet : kVioletCore, 2.2f));
  }
  lights.push_back({g.centre + kUp * (R * 0.5f), kViolet, (1.6f + 0.5f * breathe) * env * rise, R + 1.0f});
}

void MageFx::buildRipple(const Ripple& r, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const glm::vec3 eye = camera.position;
  const float age = now - r.start;
  if (age < 0.0f) return;
  // A flash spreading over the surface where the spell struck, sparks of violet light off it.
  const float f = 1.0f - ease(0.0f, 0.42f, age);
  {
    const float s = 0.25f + 0.9f * easeOut(0.0f, 0.35f, age);
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(r.at, eye, kPull)) * glm::scale(glm::mat4(1.0f), glm::vec3(s));
    draw(sphereFirst_, sphereCount_, m, kModeEnergy, now, 1.3f, 0.8f * f, kVioletCore, 0.05f, glm::vec4(kWhite, 0.2f), r.at, VfxBlend::Additive);
  }
  streaks_.push_back(streak(pulled(r.at, eye, kPull), glm::vec3(0.0f), 0.6f * (0.6f + 0.4f * f), f, kWhite, 2.6f));
  for (int j = 0; j < 10; ++j) {
    const uint32_t n = static_cast<uint32_t>(j);
    const float life = 0.25f + 0.17f * hash01(r.seed, n, 1);
    if (age > life) continue;
    const glm::vec3 d = sphereDir(hash01(r.seed, n, 2), hash01(r.seed, n, 3));
    const glm::vec3 p = r.at + d * ((1.8f + 1.5f * hash01(r.seed, n, 4)) * age);
    streaks_.push_back(streak(pulled(p, eye, kPull), d * 0.1f, 0.035f, 0.9f * (1.0f - age / life), j % 3 ? kViolet : kVioletCore, 2.4f));
  }
  lights.push_back({r.at, kVioletCore, 7.0f * f, 2.5f});
}

}  // namespace ember
