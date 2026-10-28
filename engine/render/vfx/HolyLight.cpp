#include "render/vfx/HolyLight.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
// How much each part adds over the scene (additive, display-relative): the shaft is seen through (both its faces
// add), so it stays well below white and the unit inside it shows.
constexpr float kShaftGain = 0.14f, kCoreGain = 0.12f, kGroundGain = 0.25f, kMoteGain = 0.6f, kLineGain = 0.3f;

// 0..1 between edge0 and edge1, eased (GLSL smoothstep).
float ease(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
// A stable 0..1 number per (seed, index, salt).
float hash01(uint32_t seed, uint32_t i, uint32_t salt) {
  uint32_t h = seed ^ (i * 0x9e3779b9u) ^ (salt * 0x85ebca6bu);
  h ^= h >> 16, h *= 0x7feb352du, h ^= h >> 15, h *= 0x846ca68bu, h ^= h >> 16;
  return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

enum class Motes { Rise, Converge, Bubbles };

struct StyleData {
  glm::vec3 color, core;     // the light and its hot heart
  bool shaft;
  float shaftHeight, shaftRadius;  // the shaft: x the unit's height, and metres
  float intensity, hold, fade;     // brightness; seconds held after the impact, then fading
  int motes;
  float moteSize;
  glm::vec3 moteColor, moteColor2;  // every other mote takes the second colour
  Motes moteMode;
  float ground;                     // the ground glow's radius (metres)
  float light;                      // its real light's intensity (Ember's PointLight terms)
  glm::vec3 lightColor;
};

const StyleData& styleData(HolyLight::Style s) {
  using S = HolyLight::Style;
  static const StyleData kMinor{{1.0f, 0.76f, 0.36f}, {1.0f, 0.96f, 0.86f}, true, 1.45f, 0.30f, 1.7f, 0.30f, 0.55f, 14, 0.07f,
                                {1.0f, 0.85f, 0.5f},  {1.0f, 0.92f, 0.7f},  Motes::Rise, 0.75f, 4.0f, {1.0f, 0.8f, 0.5f}};
  static const StyleData kMedium{{1.0f, 0.74f, 0.32f}, {1.0f, 0.97f, 0.88f}, true, 2.2f, 0.45f, 2.3f, 0.50f, 0.70f, 26, 0.085f,
                                 {1.0f, 0.84f, 0.46f}, {1.0f, 0.95f, 0.75f}, Motes::Rise, 1.0f, 7.0f, {1.0f, 0.8f, 0.48f}};
  static const StyleData kHands{{1.0f, 0.58f, 0.24f}, {1.0f, 0.9f, 0.72f}, true, 1.15f, 0.26f, 1.6f, 0.40f, 0.60f, 16, 0.07f,
                                {1.0f, 0.72f, 0.4f},  {1.0f, 0.86f, 0.6f},  Motes::Converge, 0.7f, 5.0f, {1.0f, 0.65f, 0.35f}};
  static const StyleData kDisease{{0.32f, 1.0f, 0.72f}, {0.86f, 1.0f, 0.95f}, true, 1.9f, 0.38f, 1.9f, 0.45f, 0.65f, 16, 0.07f,
                                  {0.55f, 1.0f, 0.82f}, {0.8f, 1.0f, 0.92f},  Motes::Rise, 1.1f, 5.0f, {0.45f, 1.0f, 0.75f}};
  static const StyleData kFood{{0.55f, 0.8f, 1.0f}, {0.95f, 0.98f, 1.0f}, true, 1.6f, 0.34f, 1.6f, 0.45f, 0.65f, 22, 0.075f,
                               {1.0f, 0.78f, 0.38f}, {0.55f, 0.82f, 1.0f}, Motes::Converge, 0.8f, 4.0f, {0.8f, 0.85f, 1.0f}};
  static const StyleData kPotionHeal{{1.0f, 0.42f, 0.2f}, {1.0f, 0.86f, 0.68f}, false, 0.0f, 0.0f, 1.8f, 0.25f, 0.60f, 12, 0.05f,
                                     {1.0f, 0.55f, 0.35f}, {1.0f, 0.75f, 0.55f}, Motes::Bubbles, 0.65f, 4.0f, {1.0f, 0.5f, 0.3f}};
  static const StyleData kPotionCure{{0.42f, 1.0f, 0.4f}, {0.88f, 1.0f, 0.85f}, false, 0.0f, 0.0f, 1.8f, 0.25f, 0.60f, 12, 0.05f,
                                     {0.55f, 1.0f, 0.5f}, {0.8f, 1.0f, 0.7f}, Motes::Bubbles, 0.65f, 4.0f, {0.5f, 1.0f, 0.45f}};
  static const StyleData kPotionMana{{0.35f, 0.55f, 1.0f}, {0.85f, 0.92f, 1.0f}, false, 0.0f, 0.0f, 1.8f, 0.25f, 0.60f, 12, 0.05f,
                                     {0.5f, 0.7f, 1.0f}, {0.75f, 0.85f, 1.0f}, Motes::Bubbles, 0.65f, 4.0f, {0.45f, 0.6f, 1.0f}};
  switch (s) {
    case S::Minor: return kMinor;
    case S::Medium: return kMedium;
    case S::Hands: return kHands;
    case S::Disease: return kDisease;
    case S::Food: return kFood;
    case S::PotionHeal: return kPotionHeal;
    case S::PotionCure: return kPotionCure;
    case S::PotionMana: return kPotionMana;
  }
  return kMinor;
}

bool isPotion(HolyLight::Style s) { return s == HolyLight::Style::PotionHeal || s == HolyLight::Style::PotionCure || s == HolyLight::Style::PotionMana; }

// An open cylinder of radius 1 from y 0 to 1 (u around, v up), normals outward.
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
// A flat disc of radius 1 on y 0 (u around, v out), facing up.
const std::vector<VfxVertex>& discGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 48, out = 10;
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
// A thin ring of radius 1 lying flat (u around).
const std::vector<VfxVertex>& haloGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> ring = vfxgeo::torus(1.0f, 0.035f, 8, 64, kTau);
    const glm::mat4 flat = glm::rotate(glm::mat4(1.0f), -std::numbers::pi_v<float> * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    for (VfxVertex& v : ring) {
      v.position = glm::vec3(flat * glm::vec4(v.position, 1.0f));
      v.normal = glm::vec3(flat * glm::vec4(v.normal, 0.0f));
    }
    return ring;
  }();
  return g;
}
// A unit quad (-0.5..0.5, uv 0..1): each mote, turned to the camera by the shader.
const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, -0.5f, 0.0f}, 1.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f},
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f}, {{-0.5f, 0.5f, 0.0f}, 0.0f, {0, 0, 1}, 1.0f}};
  return g;
}

}  // namespace

HolyLight::HolyLight(Style style, std::vector<Target> targets, float impact, uint32_t seed)
    : style_(style), targets_(std::move(targets)), impact_(impact), seed_(seed) {}

float HolyLight::leadIn(Style style) { return isPotion(style) ? 0.12f : 0.4f; }

float HolyLight::end() const {
  const StyleData& s = styleData(style_);
  return impact_ + s.hold + s.fade + (style_ == Style::Disease ? 0.4f : 0.1f);
}

void HolyLight::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const StyleData& s = styleData(style_);
  const float begin = impact_ - leadIn(style_);
  if (now < begin || done(now)) return;
  const float t = now - begin;
  const float in = ease(begin, impact_, now);                                             // gathering
  const float out = 1.0f - ease(impact_ + s.hold, impact_ + s.hold + s.fade, now);         // fading
  const float env = in * out;
  auto draw = [&](uint32_t first, size_t count, const glm::mat4& model, float mode, float intensity, float opacity, glm::vec3 color,
                  glm::vec3 core, float ring, glm::vec3 sight, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {t, mode, intensity, opacity};
    d.gpu.params1 = glm::vec4(core, ring);
    d.gpu.color = glm::vec4(color, 1.0f);
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
    d.gpu.sightAt = sight;
    d.instances = instances;
    d.blend = blend;
    d.depthTest = true;
    frame.draws.push_back(d);
  };
  const uint32_t shaftFirst = s.shaft ? frame.addVertices(shaftGeometry()) : 0;
  const uint32_t discFirst = s.ground > 0.0f ? frame.addVertices(discGeometry()) : 0;
  const uint32_t quadFirst = frame.addVertices(quadGeometry());
  const uint32_t haloFirst = style_ == Style::Medium ? frame.addVertices(haloGeometry()) : 0;

  // The camera's side and the way toward it, on the ground (the hands face the eye).
  glm::vec3 side{camera.right.x, 0.0f, camera.right.z};
  side = glm::dot(side, side) > 1e-8f ? glm::normalize(side) : glm::vec3(1.0f, 0.0f, 0.0f);

  std::vector<VfxInstance> glow, dark;
  int lit = 0;
  for (size_t ti = 0; ti < targets_.size(); ++ti) {
    const Target& target = targets_[ti];
    const float height = std::clamp(target.height, 1.0f, 4.0f);
    const float scale = std::clamp(height / 2.0f, 0.7f, 1.5f);
    const glm::vec3 feet = target.feet;
    const glm::vec3 chest = feet + glm::vec3(0.0f, height * 0.5f, 0.0f);
    const uint32_t seed = seed_ + static_cast<uint32_t>(ti) * 7919u;

    // --- The shaft: it comes down from above onto the unit, then holds and fades. An inner, hotter core inside it. ---
    if (s.shaft) {
      const float h = s.shaftHeight * height, r = s.shaftRadius * scale * (1.0f + 0.04f * std::sin(t * 6.0f + static_cast<float>(ti)));
      const float lowered = 1.0f - ease(begin, impact_, now);  // the bottom's height, a share of the whole
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), feet + glm::vec3(0.0f, lowered * h, 0.0f)) * glm::scale(glm::mat4(1.0f), {r, h * (1.0f - lowered) + 1e-3f, r});
      const float opacity = out * (0.35f + 0.65f * in);
      draw(shaftFirst, shaftGeometry().size(), m, 0.0f, s.intensity * kShaftGain, opacity, s.color, s.core, 0.0f, feet, VfxBlend::Additive);
      const glm::mat4 inner = glm::translate(glm::mat4(1.0f), feet + glm::vec3(0.0f, lowered * h, 0.0f)) *
                              glm::scale(glm::mat4(1.0f), {r * 0.45f, h * (1.0f - lowered) * 0.85f + 1e-3f, r * 0.45f});
      draw(shaftFirst, shaftGeometry().size(), inner, 0.0f, s.intensity * kCoreGain, opacity * 0.7f, glm::mix(s.color, s.core, 0.5f), s.core, 0.0f, feet,
           VfxBlend::Additive);
    }

    // --- The ground glows: a pool and a ring spreading out from the feet. ---
    if (s.ground > 0.0f) {
      const float grow = ease(impact_ - 0.1f, impact_ + (style_ == Style::Disease ? 0.7f : 0.35f), now);
      const float radius = s.ground * scale * (style_ == Style::Disease ? 0.6f + 0.9f * grow : 0.6f + 0.4f * grow);
      const float ring = 0.25f + 0.6f * grow;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), feet + glm::vec3(0.0f, 0.03f, 0.0f)) * glm::scale(glm::mat4(1.0f), {radius, 1.0f, radius});
      draw(discFirst, discGeometry().size(), m, 1.0f, s.intensity * kGroundGain, ease(begin, impact_ - 0.05f, now) * out, s.color, s.core, ring, feet,
           VfxBlend::Additive);
    }

    // --- Cura Média: a halo settles from above the head to the feet. ---
    if (style_ == Style::Medium) {
      const float k = ease(begin, impact_ + 0.3f, now);
      const float y = glm::mix(height * 1.15f, 0.08f, k), radius = glm::mix(0.55f, 0.75f, k) * scale;
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), feet + glm::vec3(0.0f, y, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
      draw(haloFirst, haloGeometry().size(), m, 4.0f, s.intensity * kLineGain, out * ease(begin, begin + 0.15f, now), s.color, s.core, 0.0f, feet,
           VfxBlend::Additive);
    }

    // --- Mãos Curativas: two cupped arcs of warm light close around the body. ---
    if (style_ == Style::Hands) {
      glm::vec3 toward = camera.position - feet;
      toward.y = 0.0f;
      toward = glm::dot(toward, toward) > 1e-8f ? glm::normalize(toward) : glm::vec3(0.0f, 0.0f, 1.0f);
      const float close = 1.25f - 0.45f * ease(begin, impact_ + 0.2f, now);
      for (const float hand : {-1.0f, 1.0f}) {
        auto at = [&](float x, float y, float z) { return feet + side * (x * hand * close * scale) + glm::vec3(0.0f, y * height, 0.0f) + toward * (z * scale); };
        const three::CatmullRom curve{{at(0.42f, 0.78f, 0.05f), at(0.6f, 0.55f, 0.15f), at(0.45f, 0.28f, 0.22f), at(0.14f, 0.36f, 0.28f)}};
        const std::vector<VfxVertex> tube = vfxgeo::tube(curve, 24, 0.035f * scale, 6);
        draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), 3.0f, s.intensity * kLineGain, env, s.color, s.core, 0.0f, feet, VfxBlend::Additive);
      }
    }

    // --- A potion: a burst at the chest. ---
    if (isPotion(style_)) {
      const float size = height * 0.9f * (0.4f + 0.6f * ease(impact_ - 0.08f, impact_ + 0.15f, now));
      const float alpha = ease(begin, impact_, now) * (1.0f - ease(impact_, impact_ + 0.55f, now));
      if (alpha > 0.0f) glow.push_back({glm::mat4(1.0f), glm::vec4(chest, size), {0.0f, 0.0f, alpha, 1.6f}, glm::vec4(s.color, 0.0f)});
    }

    // --- Motes: rising round the unit, drawn into it, or bubbling up from a potion. ---
    const float h = s.shaft ? s.shaftHeight * height : height;
    const float moteEnv = ease(begin, begin + 0.15f, now) * (1.0f - ease(impact_ + s.hold, impact_ + s.hold + s.fade * 0.8f, now));
    for (int i = 0; i < s.motes && moteEnv > 0.0f; ++i) {
      const float a0 = hash01(seed, static_cast<uint32_t>(i), 1), r0 = hash01(seed, static_cast<uint32_t>(i), 2), p0 = hash01(seed, static_cast<uint32_t>(i), 3);
      glm::vec3 p;
      float alpha = 0.0f, size = s.moteSize * scale * 1.6f;
      if (s.moteMode == Motes::Rise) {
        const float phase = std::fmod(p0 + t * 0.55f, 1.0f);
        const float angle = a0 * kTau + t * (a0 > 0.5f ? 1.6f : -1.6f);
        const float radius = (0.35f + 0.9f * r0) * s.shaftRadius * scale;
        p = feet + glm::vec3(std::cos(angle) * radius, phase * h * 0.9f, std::sin(angle) * radius);
        alpha = std::sin(std::numbers::pi_v<float> * phase);
        size *= 1.0f - 0.4f * phase;
      } else if (s.moteMode == Motes::Converge) {
        const float phase = std::fmod(p0 + t * 0.7f, 1.0f);
        const float angle = a0 * kTau + phase * 3.5f;
        const float radius = glm::mix(1.0f, 0.1f, phase) * (0.8f + 0.4f * r0) * scale;
        p = glm::vec3(feet.x, 0.0f, feet.z) + glm::vec3(std::cos(angle) * radius, glm::mix(feet.y + h * 0.9f, chest.y, phase), std::sin(angle) * radius);
        alpha = std::sin(std::numbers::pi_v<float> * phase);
      } else {
        const float phase = std::fmod(p0 + t * 0.45f, 1.0f);
        const float wobble = std::sin(t * 5.0f + a0 * kTau) * 0.06f;
        const float angle = a0 * kTau;
        const float radius = (0.12f + 0.22f * r0) * scale;
        p = chest + glm::vec3(std::cos(angle) * radius + wobble, phase * height * 0.75f - height * 0.15f, std::sin(angle) * radius);
        alpha = std::sin(std::numbers::pi_v<float> * phase) * ease(impact_ - 0.05f, impact_ + 0.1f, now);
        size *= 0.6f + 0.8f * r0;
      }
      alpha *= moteEnv;
      if (alpha <= 0.002f) continue;
      glow.push_back({glm::mat4(1.0f), glm::vec4(p, size), {0.0f, 0.0f, alpha, 1.4f}, glm::vec4((i & 1) ? s.moteColor2 : s.moteColor, 0.0f)});
    }

    // --- Curar Doença: the sickness leaves the body as dark motes, drifting up and away. ---
    if (style_ == Style::Disease) {
      const float darkEnv = ease(impact_ - 0.15f, impact_ + 0.05f, now) * (1.0f - ease(impact_ + 0.7f, impact_ + 1.2f, now));
      for (int i = 0; i < 14 && darkEnv > 0.0f; ++i) {
        const float a0 = hash01(seed, static_cast<uint32_t>(i), 11), p0 = hash01(seed, static_cast<uint32_t>(i), 12);
        const float phase = std::fmod(p0 + std::max(0.0f, now - impact_ + 0.15f) * 0.8f, 1.0f);
        const float angle = a0 * kTau + phase * 1.5f;
        const float radius = (0.12f + phase * 0.55f) * scale;
        const glm::vec3 p = feet + glm::vec3(std::cos(angle) * radius, height * (0.35f + phase * 0.8f), std::sin(angle) * radius);
        const float alpha = (1.0f - phase) * std::min(1.0f, phase * 6.0f) * darkEnv * 0.85f;
        if (alpha > 0.002f) dark.push_back({glm::mat4(1.0f), glm::vec4(p, 0.11f * scale * (1.0f + phase)), {0.0f, 0.0f, alpha, 1.0f}, glm::vec4(0.05f, 0.09f, 0.04f, 0.0f)});
      }
    }

    // --- Its light on the board and the units nearby (at most four per cast). ---
    if (lit < 4 && env > 0.0f) {
      ++lit;
      const float flare = 1.0f + 0.6f * (1.0f - ease(impact_, impact_ + 0.3f, now)) * ease(begin, impact_, now);
      lights.push_back({chest, s.lightColor, s.light * env * flare, 3.2f * scale});
    }
  }

  // The motes, all in one draw each (light; and the sickness, darkening what is behind it).
  if (!glow.empty()) {
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), glow.begin(), glow.end());
    draw(quadFirst, quadGeometry().size(), glm::mat4(1.0f), 2.0f, s.intensity * kMoteGain, 1.0f, s.color, s.core, 0.0f, targets_.front().feet, VfxBlend::Additive,
         firstInstance, static_cast<uint32_t>(glow.size()));
  }
  if (!dark.empty()) {
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), dark.begin(), dark.end());
    draw(quadFirst, quadGeometry().size(), glm::mat4(1.0f), 2.0f, 1.0f, 1.0f, glm::vec3(0.05f, 0.09f, 0.04f), glm::vec3(0.03f, 0.05f, 0.02f), 0.0f,
         targets_.front().feet, VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(dark.size()));
  }
}

}  // namespace ember
