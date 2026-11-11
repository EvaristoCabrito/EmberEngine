#include "render/vfx/HandBoltFx.h"

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
// A unit's card is drawn nearer in depth: what happens on a body is pulled toward the eye to show over it (as vfx/Lightning).
constexpr float kPull = 1.3f;
// The Combat material's modes (shaders/vfx.hlsl).
constexpr float kStreak = 0.0f, kRing = 3.0f, kLightning = 6.0f;
constexpr float kFlicker = 1.0f / 24.0f;  // a new shape every 1/24 s (ApparitionLightningV1's phase)

struct Palette {
  glm::vec3 shell, core, light;
};
// The Aparição's: ApparitionLightningV1's #248cff / #39a9ff / #72d7ff shells round an #edfcff thread. Telesm's: the cyan of its video.
Palette paletteOf(HandBoltFx::Look look) {
  if (look == HandBoltFx::Look::Telesm) return {{0.16f, 0.92f, 0.95f}, {0.86f, 1.0f, 1.0f}, {0.35f, 1.0f, 0.95f}};
  return {{0.25f, 0.62f, 1.0f}, {0.93f, 0.99f, 1.0f}, {0.5f, 0.78f, 1.0f}};
}

float ease(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
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
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}

// A jagged line from `a` to `b` (vfx/Lightning's): each inner point pushed sideways by up to `amp`, the jitter from (seed, variant),
// pulled toward the eye (`pull`, 0 = not).
three::CatmullRom jagged(glm::vec3 a, glm::vec3 b, int points, float amp, uint32_t seed, uint32_t variant, glm::vec3 eye, float pull) {
  three::CatmullRom path;
  const glm::vec3 d = glm::length(b - a) > 1e-4f ? glm::normalize(b - a) : glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 e1 = glm::normalize(glm::cross(d, std::abs(d.y) < 0.9f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f))), e2 = glm::cross(d, e1);
  for (int k = 0; k < points; ++k) {
    const float t = static_cast<float>(k) / static_cast<float>(points - 1);
    glm::vec3 p = glm::mix(a, b, t);
    if (k > 0 && k + 1 < points) {
      const float taper = std::pow(std::sin(kPi * t), 0.6f);
      const uint32_t i = static_cast<uint32_t>(k), s = seed ^ (variant * 0x51ed27u);
      const float big = (hash01(seed, i, 91) - 0.5f) * 2.0f, big2 = (hash01(seed, i, 92) - 0.5f) * 2.0f;
      const float small = (hash01(s, i, 93) - 0.5f) * 2.0f, small2 = (hash01(s, i, 94) - 0.5f) * 2.0f;
      p += (e1 * (big * 0.6f + small * 0.6f) + e2 * (big2 * 0.6f + small2 * 0.6f)) * amp * taper;
    }
    path.points.push_back(pull > 0.0f ? pulled(p, eye, pull) : p);
  }
  return path;
}

// A point of the art (full-image uv as drawn facing right, v from the top) on the card as drawn (MapStage addCard).
glm::vec3 onCard(const HandBoltFx::Card& c, glm::vec2 uv) {
  const float x = ((uv.x - 0.5f) * c.width - c.drift) * (c.mirror ? -1.0f : 1.0f);
  return c.anchor + c.right * x + c.up * ((1.0f - uv.y) * c.height - c.foot);
}

}  // namespace

void HandBoltFx::start(int unit, const Track& track, const Card& card, glm::vec3 targetFeet, float targetHeight, float start, float length, float impact,
                       uint32_t seed) {
  if (track.hands.empty()) return;
  Effect e;
  e.unit = unit, e.track = track, e.card = card, e.targetFeet = targetFeet, e.targetHeight = std::clamp(targetHeight > 0.0f ? targetHeight : 1.8f, 1.0f, 4.0f);
  e.start = start, e.length = std::max(length, 0.1f), e.seed = seed;
  e.release = start + e.length * releaseShare(track);
  e.impact = std::max(impact, e.release + 0.02f);
  effects_.push_back(std::move(e));
}

void HandBoltFx::follow(int unit, const Card& card) {
  for (Effect& e : effects_)
    if (e.unit == unit) e.card = card;
}

bool HandBoltFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return now < e.end(); });
}
float HandBoltFx::latestEnd() const {
  float end = 0.0f;
  for (const Effect& e : effects_) end = std::max(end, e.end());
  return end;
}

void HandBoltFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return now >= e.end(); });
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    buildCharge(e, now, camera, frame, lights);
    if (now >= e.release) buildBolt(e, now, camera, frame, lights);
  }
}

// Arcs across the glow at the hands, from the charge frame until just after the release.
void HandBoltFx::buildCharge(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const int n = static_cast<int>(e.track.hands.size());
  const float charge = e.start + e.length * static_cast<float>(std::max(e.track.chargeFrame - 1, 0)) / static_cast<float>(n);
  const float stop = e.release + 0.08f;
  if (now < charge || now >= stop) return;
  const int f = std::clamp(static_cast<int>((now - e.start) / e.length * static_cast<float>(n)), 0, n - 1);
  const Palette pal = paletteOf(e.track.look);
  const Draw draw{frame, now};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.card.height / 2.8f, 0.5f, 1.6f);  // the Aparição's card is 2.8 units tall
  // ApparitionLightningV1: up to half strength over 1.5 s of charge, full as the hands let go.
  const float strength = (0.2f + 0.3f * ease(charge, charge + 1.5f, now) + 0.5f * ease(e.release - 0.4f, e.release, now)) * (1.0f - ease(e.release, stop, now));
  const glm::vec3 hand = onCard(e.card, e.track.hands[static_cast<size_t>(f)]);
  const glm::vec3 across = e.card.right * (e.card.mirror ? -1.0f : 1.0f);
  const uint32_t variant = static_cast<uint32_t>(std::max(0.0f, (now - charge) / kFlicker));

  // Four arcs joining the two hands (the glow's width), stacked a little, each a new shape every 1/24 s.
  for (uint32_t j = 0; j < 4; ++j) {
    const float lift = (static_cast<float>(j) - 1.5f) * 0.035f * k;
    const float span = (0.13f + 0.04f * hash01(e.seed, j + variant * 4, 3)) * e.card.width;
    const glm::vec3 a = hand - across * (span * 0.5f) + e.card.up * lift, b = hand + across * (span * 0.5f) - e.card.up * lift;
    const three::CatmullRom arc = jagged(a, b, 7, 0.07f * k, e.seed + j * 977u, variant, eye, kPull);
    const std::vector<VfxVertex> core = vfxgeo::tube(arc, 18, 0.007f * k, 5), halo = vfxgeo::tube(arc, 18, 0.026f * k, 6);
    draw(frame.addVertices(halo), halo.size(), glm::mat4(1.0f), kLightning, 1.4f, 0.4f * strength, pal.shell, glm::vec4(pal.core, 1.2f), hand);
    draw(frame.addVertices(core), core.size(), glm::mat4(1.0f), kLightning, 3.4f, strength, pal.shell, glm::vec4(pal.core, 1.2f), hand);
  }
  // A flare at the hands, a few sparks off them, the cold light.
  std::vector<VfxInstance> streaks;
  const glm::vec3 at = pulled(hand, eye, kPull);
  streaks.push_back(streak(at, glm::vec3(0.0f), (0.18f + 0.2f * strength) * k * (0.8f + 0.2f * hash01(e.seed, variant, 5)), 0.6f * strength, pal.core, 2.4f));
  for (uint32_t i = 0; i < 4; ++i) {
    const glm::vec3 d = sphereDir(hash01(e.seed, i + variant * 7, 6), hash01(e.seed, i + variant * 7, 7));
    const float u = hash01(e.seed, i + variant * 7, 8);
    streaks.push_back(streak(at + d * (0.12f + 0.25f * u) * k, d * 0.06f * k, 0.016f * k, (1.0f - u) * strength, pal.shell, 2.2f));
  }
  const uint32_t quad = frame.addVertices(quadGeometry());
  draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(0.9f, 0.95f, 1.0f, 0.0f), hand, &streaks);
  lights.push_back({hand, pal.light, 3.0f + 9.0f * strength, 2.5f * k});
}

// The bolt from the hands to the body, and the hit.
void HandBoltFx::buildBolt(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const Palette pal = paletteOf(e.track.look);
  const Draw draw{frame, now};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(e.card.height / 2.8f, 0.5f, 1.6f);
  const float age = now - e.release, flight = e.impact - e.release, after = now - e.impact;
  const size_t rf = static_cast<size_t>(std::clamp(e.track.releaseFrame - 1, 0, static_cast<int>(e.track.hands.size()) - 1));
  const glm::vec3 hand = onCard(e.card, e.track.hands[rf]);
  const glm::vec3 chest = e.targetFeet + kUp * (e.targetHeight * 0.55f);
  const uint32_t variant = static_cast<uint32_t>(age / kFlicker) % 4u;
  // Lit from the hands out over the flight, held, then fading (ApparitionLightningV1: 0.3 s to fade at the end).
  const float lit = std::clamp(age / std::max(flight, 1e-3f), 0.0f, 1.0f) * 1.15f;
  const float opacity = 1.0f - ease(0.25f, 0.6f, after);

  if (opacity > 0.01f) {
    const float reach = glm::length(chest - hand);
    const int points = std::clamp(static_cast<int>(reach / 0.18f), 8, 24);
    const three::CatmullRom bolt = jagged(hand, chest, points, std::min(0.16f * k, 0.1f * reach), e.seed, variant, eye, kPull);
    const std::vector<VfxVertex> core = vfxgeo::tube(bolt, points * 3, 0.02f * k, 6), glow = vfxgeo::tube(bolt, points * 3, 0.075f * k, 8);
    draw(frame.addVertices(glow), glow.size(), glm::mat4(1.0f), kLightning, 1.7f, 0.55f * opacity, pal.shell, glm::vec4(pal.core, lit), chest);
    draw(frame.addVertices(core), core.size(), glm::mat4(1.0f), kLightning, 5.0f, opacity, pal.shell, glm::vec4(pal.core, lit), chest);
    // Short forks off every third point, flicking out to the side (ApparitionLightningV1's side strokes).
    for (int p = 3; p + 1 < points; p += 3) {
      const float t = static_cast<float>(p) / static_cast<float>(points - 1);
      if (t > lit) break;
      const uint32_t b = static_cast<uint32_t>(p) + variant * 31u;
      const glm::vec3 from = bolt.points[static_cast<size_t>(p)];
      const glm::vec3 d = glm::normalize(sphereDir(hash01(e.seed, b, 21), hash01(e.seed, b, 22)) + kUp * 0.2f);
      const glm::vec3 to = from + d * (0.25f + 0.3f * hash01(e.seed, b, 23)) * k;
      const three::CatmullRom fork = jagged(from, to, 4, 0.06f * k, e.seed + b, variant, eye, 0.0f);  // it starts on the bolt, already pulled
      const std::vector<VfxVertex> tube = vfxgeo::tube(fork, 9, 0.009f * k, 5);
      draw(frame.addVertices(tube), tube.size(), glm::mat4(1.0f), kLightning, 3.0f, 0.75f * opacity, pal.shell, glm::vec4(pal.core, 1.2f), chest);
    }
    lights.push_back({glm::mix(hand, chest, 0.5f), pal.light, 10.0f * opacity * std::min(lit, 1.0f), 3.5f * k});
  }

  if (after < 0.0f) return;
  // The hit: a flash on the body, a small ring over the ground, sparks flung out and falling, a dazzling light dying away.
  const uint32_t quad = frame.addVertices(quadGeometry()), disc = frame.addVertices(discGeometry());
  std::vector<VfxInstance> streaks;
  const glm::vec3 point = pulled(chest, eye, kPull);
  if (after < 0.14f) streaks.push_back(streak(point, glm::vec3(0.0f), 1.1f * k * std::sqrt(1.0f - after / 0.14f), 1.0f - after / 0.14f, pal.core, 3.0f));
  if (after < 0.45f) {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), e.targetFeet + glm::vec3(0.0f, 0.04f, 0.0f)) *
                        glm::scale(glm::mat4(1.0f), glm::vec3((0.3f + 0.9f * ease(0.0f, 0.35f, after)) * k));
    draw(disc, discGeometry().size(), m, kRing, 1.8f, 1.0f - ease(0.08f, 0.45f, after), pal.shell, glm::vec4(pal.core, 0.88f), e.targetFeet);
  }
  for (uint32_t i = 0; i < 18; ++i) {
    const float life = 0.3f + 0.25f * hash01(e.seed, i, 41);
    if (after > life) continue;
    const float speed = (2.2f + 2.0f * hash01(e.seed, i, 42)) * k;
    glm::vec3 d = sphereDir(hash01(e.seed, i, 43), hash01(e.seed, i, 44));
    d.y = std::abs(d.y) * 0.8f + 0.15f;
    d = glm::normalize(d);
    const glm::vec3 p = chest + d * speed * (1.0f - std::exp(-6.0f * after)) / 6.0f - kUp * (1.5f * after * after);
    streaks.push_back(streak(pulled(p, eye, kPull), d * speed * std::exp(-6.0f * after) * 0.06f, 0.022f * k, 1.0f - after / life, i % 3 ? pal.shell : pal.core, 2.5f));
  }
  draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(0.9f, 0.95f, 1.0f, 0.0f), chest, &streaks);
  if (after < 0.7f) lights.push_back({chest, pal.light, 40.0f * k * std::exp(-6.0f * after), 6.0f * k});
}

}  // namespace ember
