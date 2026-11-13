#include "render/vfx/IceStorm.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/IceParts.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

using namespace icefx;

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPi;
constexpr uint32_t kSeed = 38171;     // Blizzard2D's default seed: the same storm on every cast
constexpr float kDensity = 1.3f;      // battleSpellOverlay3d's particleDensity
constexpr float kPulseLight = 7.0f;   // a landing spike's light at full strength (pulseBlizzardLight; colour 0xb6e7ff)
constexpr float kPulseReach = 3.2f;   // its distance: 3.2 hex radii

// Blizzard2D's rng (xorshift32, 0..1).
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
  float operator()() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(static_cast<double>(s) / 4294967296.0);
  }
};
// A band of wind: an arc of a unit circle 0.9 rad long (from +x toward +z), one unit tall (u along, v across).
const std::vector<VfxVertex>& bandGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int along = 20;
    constexpr float span = 0.9f;
    auto v = [](int i, int j) {
      const float u = static_cast<float>(i) / along, phi = u * span, y = static_cast<float>(j);
      return vertex({std::cos(phi), y - 0.5f, std::sin(phi)}, {std::cos(phi), 0, std::sin(phi)}, u, y);
    };
    std::vector<VfxVertex> out;
    for (int i = 0; i < along; ++i) out.insert(out.end(), {v(i, 0), v(i + 1, 0), v(i + 1, 1), v(i, 0), v(i + 1, 1), v(i, 1)});
    return out;
  }();
  return g;
}

}  // namespace

IceStorm::IceStorm(int version, std::vector<glm::vec3> hexes, float start)
    : version_(version), hexes_(std::move(hexes)), start_(start), end_(start + kDuration) {
  if (hexes_.empty()) {
    end_ = start;
    return;
  }
  for (const glm::vec3& h : hexes_) centre_ += h;
  centre_ /= static_cast<float>(hexes_.size());
  for (const glm::vec3& h : hexes_) radius_ = std::max(radius_, glm::length(glm::vec2(h.x - centre_.x, h.z - centre_.z)) + 1.0f);
  // Blizzard2D's motes: uniform over each pointy-top hex (a triangle fan, adjacent edges join), every 24 = 5 wind, 2 spikes,
  // 17 snow; each spike also lands as an impact.
  Rng random(kSeed);
  const int count = std::min(1800, static_cast<int>(std::ceil(static_cast<float>(hexes_.size()) * 95.0f * kDensity)));
  for (int i = 0; i < count; ++i) {
    const int edge = static_cast<int>(std::floor(random() * 6.0f));
    const float a = (static_cast<float>(edge) * 60.0f - 30.0f) * kPi / 180.0f, b = a + kPi / 3.0f;
    const float r = std::sqrt(random()) * 0.94f, t = random();
    Mote m;
    m.dx = r * ((1 - t) * std::cos(a) + t * std::cos(b));
    m.dy = r * ((1 - t) * std::sin(a) + t * std::sin(b));
    m.kind = i % 24 < 5 ? 0 : i % 24 < 7 ? 1 : 2;
    m.layer = i % 3;
    m.cell = std::min(static_cast<int>(std::floor(random() * static_cast<float>(hexes_.size()))), static_cast<int>(hexes_.size()) - 1);
    m.phase = random();
    m.speed = 0.6f + random() * 0.9f;
    m.size = random();
    m.turn = random() * 6.28f;
    motes_.push_back(m);
  }
  const size_t made = motes_.size();
  for (size_t i = 0; i < made; ++i)
    if (motes_[i].kind == 1) {
      Mote impact = motes_[i];
      impact.kind = 3;
      motes_.push_back(impact);
    }
}

void IceStorm::build(float now, const Camera& camera, const VfxFlipbook& ice, const VfxFlipbook& wind, VfxFrame& frame, std::vector<VfxLight>& lights) {
  if (now < start_ || done(now) || hexes_.empty()) return;
  const float age = standing_ ? now - start_ : std::min(now - start_, kDuration);
  const float gather = std::min(1.0f, age / kGather);
  float fade = standing_ ? 1.0f : std::min(1.0f, (kDuration - age) / kFade);
  if (stopped_) fade = std::min(fade, std::max(0.0f, (end_ - now) / kStopFade));
  const float env = gather * fade;
  glm::vec3 right{camera.right.x, 0.0f, camera.right.z};  // Ember's gusts blow across the screen
  right = glm::dot(right, right) > 1e-8f ? glm::normalize(right) : glm::vec3(1, 0, 0);

  // A spike lands at the end of each of its cycles: the oldest of the three lights moves there (pulseBlizzardLight).
  for (Mote& m : motes_) {
    if (m.kind != 1) continue;
    const int cycle = static_cast<int>(std::floor(age * m.speed * 0.55f + m.phase));
    if (cycle <= m.lastCycle) continue;
    if (m.lastCycle >= 0) {
      Pulse& p = *std::min_element(pulses_.begin(), pulses_.end(), [](const Pulse& x, const Pulse& y) { return x.time < y.time; });
      p = {hexes_[static_cast<size_t>(m.cell)] + glm::vec3(m.dx, 0.5f, m.dy), now, fade};
    }
    m.lastCycle = cycle;
  }
  for (const Pulse& p : pulses_) {
    const float since = now - p.time;
    if (since >= 0.0f && since < 1.0f) lights.push_back({p.at, three::hex(0xb6e7ff), kPulseLight * p.strength * std::exp(-since * 6.0f), kPulseReach});
  }

  if (version_ == 1) buildClassic(age, env, right, ice, wind, frame);
  else buildOurs(now, age, env, right, frame);
}

// --- 01: Blizzard2D's sprites, placed in the board where its canvas drew them (a tile = one hex radius = 1 unit; its
// screen heights become heights above the ground, its ground offsets lie on the board, its gusts blow across the screen).
void IceStorm::buildClassic(float age, float env, glm::vec3 right, const VfxFlipbook& ice, const VfxFlipbook& wind, VfxFrame& frame) {
  if (!ice || !wind) return;
  const uint32_t quadFirst = frame.addVertices(quadGeometry());
  const uint32_t hexFirst = frame.addVertices(hexGeometry());
  auto draw = [&](const VfxFlipbook& sheet, float mode, uint32_t first, size_t count, const glm::mat4& model, float opacity, glm::vec3 sight,
                  const std::vector<VfxInstance>* list) {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {age, mode, 1.0f, opacity};
    d.gpu.params1 = {sheet.cellWidth, sheet.cellHeight, sheet.cropX, sheet.cropY};
    d.gpu.color = {1.0f / sheet.textureWidth, 1.0f / sheet.textureHeight, 0, 0};
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    if (list) {
      d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), list->begin(), list->end());
      d.instances = static_cast<uint32_t>(list->size());
    }
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::IceStorm);
    d.gpu.sightAt = sight;
    d.gpu.texture = sheet.texture;
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  };
  // The frosted floor (Blizzard2D's area pass: the wind sheet's cell (1, 2) over the field, 0.32 strong).
  for (const glm::vec3& h : hexes_)
    draw(wind, 4.0f, hexFirst, hexGeometry().size(), glm::translate(glm::mat4(1.0f), h + glm::vec3(0, 0.03f, 0)), env, h, nullptr);

  // The motes, one draw per hex and sheet (the fog of war is read per hex).
  std::vector<std::vector<VfxInstance>> windSprites(hexes_.size()), iceSprites(hexes_.size());
  for (const Mote& m : motes_) {
    const glm::vec3 ground = hexes_[static_cast<size_t>(m.cell)] + glm::vec3(m.dx, 0.0f, m.dy);
    const float cycle = age * m.speed * 0.55f + m.phase, f = cycle - std::floor(cycle);
    const float layer = static_cast<float>(m.layer);
    const float gust = std::sin(age * 1.5f + m.turn + layer);
    float side = 0.0f, height = 0.0f, sx = 0.0f, sy = 0.0f, angle = 0.0f, alpha = env, frameNo = 0.0f;
    if (m.kind == 0) {
      side = gust * 0.23f;
      sx = 0.68f + m.size * 0.55f, sy = 0.18f + m.size * 0.17f;
      height = 0.35f + layer * 0.65f + sy * 0.8f;
      angle = -0.25f + gust * 0.15f;
      alpha *= 0.24f;
      frameNo = std::floor(std::fmod(age * 18.0f + m.phase * 16.0f, 16.0f));
    } else if (m.kind == 1) {
      side = gust * 0.12f * (1.0f - f);
      sx = 0.07f + m.size * 0.07f, sy = 0.18f + m.size * 0.17f;
      height = (1.0f - f) * (1.8f + layer * 0.26f) + sy * 0.8f;
      angle = kPi + std::atan2(0.12f * (1.5f * std::cos(age * 1.5f + m.turn + layer) * (1.0f - f) - gust * m.speed * 0.55f),
                               m.speed * 0.55f * (1.8f + layer * 0.26f));
      alpha *= 0.92f;
      frameNo = 3.0f;
    } else if (m.kind == 2) {
      side = gust * 0.14f * (1.0f - f);
      height = (1.0f - f) * (1.9f + layer * 0.3f);
      sx = 0.009f + m.size * 0.016f, sy = sx * (1.0f + layer * 0.3f);
      angle = -0.2f;
      alpha *= 0.45f + layer * 0.18f;
    } else {
      if (f >= 0.28f) continue;
      sx = 0.35f + m.size * 0.24f, sy = 0.33f + m.size * 0.25f;
      height = sy * 0.85f;
      frameNo = std::min(15.0f, std::floor(f / 0.28f * 16.0f));
    }
    if (alpha <= 0.002f) continue;
    VfxInstance inst;
    inst.a = glm::vec4(ground + right * side + glm::vec3(0, height, 0), 0.0f);
    inst.b = {angle, frameNo, alpha, static_cast<float>(m.kind)};
    inst.c = {sx * 2.0f, sy * 2.0f, 0.0f, 0.0f};
    (m.kind == 0 ? windSprites : iceSprites)[static_cast<size_t>(m.cell)].push_back(inst);
  }
  for (size_t h = 0; h < hexes_.size(); ++h) {
    if (!windSprites[h].empty()) draw(wind, 5.0f, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, hexes_[h], &windSprites[h]);
    if (!iceSprites[h].empty()) draw(ice, 5.0f, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, hexes_[h], &iceSprites[h]);
  }
}

// --- 02: ours, in 3D, on Blizzard2D's motes and timings.
void IceStorm::buildOurs(float now, float age, float env, glm::vec3 right, VfxFrame& frame) {
  (void)now;
  const uint32_t quadFirst = frame.addVertices(quadGeometry());
  const uint32_t hexFirst = frame.addVertices(hexGeometry());
  const uint32_t discFirst = frame.addVertices(discGeometry());
  const uint32_t bandFirst = frame.addVertices(bandGeometry());
  const uint32_t crystalFirst = frame.addVertices(crystalGeometry());
  auto draw = [&](float mode, uint32_t first, size_t count, const glm::mat4& model, float opacity, glm::vec4 params1, glm::vec3 sight,
                  const std::vector<VfxInstance>* list) {
    // 03: its ice in the ice look of vfx/IceFx (shaders/vfx.hlsl modes 8..10): the frosted floor with its bright edge, the falling
    // ice as real bodies of ice, the ice dust as cold mist.
    if (version_ >= 3) mode = mode == 0.0f ? 9.0f : mode == 3.0f ? 8.0f : mode == 6.0f ? 10.0f : mode;
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {age, mode, 1.0f, opacity};
    d.gpu.params1 = params1;
    d.gpu.color = {1, 1, 1, 1};
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    if (list) {
      if (list->empty()) return;
      d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), list->begin(), list->end());
      d.instances = static_cast<uint32_t>(list->size());
    }
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::IceStorm);
    d.gpu.sightAt = sight;
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  };
  const float fade = env / std::max(std::min(1.0f, age / kGather), 1e-4f);
  // 03 (2026-11-13; the first 03 was 02's needles in the new ice, "the worst" of them): a storm of great shards. A third of the
  // spikes fall, each a blade of ice as long as a man is tall, from a bank of cloud high over the field; where one lands it stands
  // driven into the ground a moment and is eaten away, its pieces thrown out, cold pouring off it; a carpet of mist turns over the
  // whole field for as long as the storm stands.
  const bool big = version_ >= 3;
  auto falls = [&](const Mote& m) { return !big || m.phase * 7.31f - std::floor(m.phase * 7.31f) < 0.34f; };
  auto packed = [](float seed, float radius) { return std::min(seed, 0.999f) + std::floor(radius * 100.0f); };  // the ice look's seed + 100 x radius

  // The floor frosts over from each hex's middle as the storm gathers (the cover reaches the corners at 1.2 s).
  const float cover = 0.15f + 1.25f * ease(0.0f, kGather * 1.3f, age);
  for (const glm::vec3& h : hexes_)
    draw(0.0f, hexFirst, hexGeometry().size(), glm::translate(glm::mat4(1.0f), h + glm::vec3(0, 0.035f, 0)), fade, {cover, 0, 0, 0}, h, nullptr);

  std::vector<VfxInstance> bands;
  std::vector<std::vector<VfxInstance>> flakes(hexes_.size()), crystals(hexes_.size()), rings(hexes_.size()), puffs(hexes_.size());
  for (size_t i = 0; i < motes_.size(); ++i) {
    const Mote& m = motes_[i];
    const glm::vec3 ground = hexes_[static_cast<size_t>(m.cell)] + glm::vec3(m.dx, 0.0f, m.dy);
    const float speed = m.speed * 0.55f;
    const float cycle = age * speed + m.phase, f = cycle - std::floor(cycle);
    const float layer = static_cast<float>(m.layer);
    const float gust = std::sin(age * 1.5f + m.turn + layer);
    const size_t cell = static_cast<size_t>(m.cell);
    if (m.kind == 0) {
      // Every other wisp is a band of wind turning round the field's centre (all one way: a vortex), at its wisp's height.
      if (i % 2) continue;
      const glm::vec2 rel{ground.x - centre_.x, ground.z - centre_.z};
      const float r = std::max(0.45f, glm::length(rel) + 0.25f);
      const float theta = std::atan2(rel.y, rel.x) + age * (0.9f + 0.5f * m.speed) / r + gust * 0.08f;
      const float height = 0.25f + layer * 0.6f + m.size * 0.3f, tall = 0.12f + 0.18f * m.size;
      VfxInstance inst;
      inst.matrix = glm::translate(glm::mat4(1.0f), glm::vec3(centre_.x, ground.y + height, centre_.z)) *
                    glm::rotate(glm::mat4(1.0f), -theta, glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1.0f), std::sin(m.turn) * 0.15f, glm::vec3(1, 0, 0)) *
                    glm::scale(glm::mat4(1.0f), {r, tall, r});
      inst.c = {env * 0.3f, m.phase, 0, 0};
      bands.push_back(inst);
    } else if (m.kind == 2) {
      // Snow: falls as Blizzard2D's, swirled round the centre as it comes down.
      glm::vec2 rel{ground.x - centre_.x, ground.z - centre_.z};
      const float swirl = (1.0f - f) * 1.1f + age * 0.25f;
      rel = glm::vec2(rel.x * std::cos(swirl) - rel.y * std::sin(swirl), rel.x * std::sin(swirl) + rel.y * std::cos(swirl)) * (1.0f + 0.15f * (1.0f - f));
      const glm::vec3 at = glm::vec3(centre_.x + rel.x, ground.y + (1.0f - f) * (1.9f + layer * 0.3f), centre_.z + rel.y) + right * (gust * 0.14f * (1.0f - f));
      const float size = 0.03f + 0.04f * m.size;
      VfxInstance inst;
      inst.a = glm::vec4(at, 0.0f);
      inst.b = {0.0f, 0.0f, env * (0.5f + 0.15f * layer) * ease(0.0f, 0.08f, f), 0.0f};
      inst.c = {size, size, 0.0f, m.phase};
      flakes[cell].push_back(inst);
    } else if (m.kind == 1) {
      // An ice crystal falling tip first along its slanted path.
      if (!falls(m)) continue;
      const float drop = big ? 5.2f + layer * 0.5f : 1.8f + layer * 0.26f;
      const float length = big ? 1.4f + 1.0f * m.size : 0.3f + 0.3f * m.size, radius = big ? 0.14f + 0.08f * m.size : 0.025f + 0.02f * m.size;
      const float sideways = 0.12f * (1.5f * std::cos(age * 1.5f + m.turn + layer) * (1.0f - f) - gust * speed);
      const glm::vec3 velocity = right * sideways - glm::vec3(0, speed * drop, 0);
      const glm::vec3 up = -glm::normalize(velocity);
      const glm::vec3 tip = ground + right * (gust * 0.12f * (1.0f - f)) + glm::vec3(0, (1.0f - f) * drop, 0);
      VfxInstance inst;
      inst.matrix = glm::translate(glm::mat4(1.0f), tip + up * (length * 0.5f)) * alongAxis(up, m.turn + age * 2.0f) *
                    glm::scale(glm::mat4(1.0f), {radius, length, radius});
      inst.c = {env * 0.95f * ease(0.0f, 0.06f, f), big ? packed(m.phase, radius) : m.phase, 0, big ? 0.6f : 0.0f};
      crystals[cell].push_back(inst);
    } else if (big) {
      // 03: the blade stands where it struck, leaning, and is eaten away through the noise; its pieces fly; frost runs out over the
      // ground; the cold pours off it.
      if (!falls(m) || f >= 0.5f) continue;
      const float k = f / 0.5f, t = f / speed;
      const glm::vec3 at = ground + glm::vec3(0, 0.02f, 0);
      const float length = (1.4f + 1.0f * m.size) * 0.85f, radius = 0.14f + 0.08f * m.size;
      const glm::vec3 lean = glm::normalize(glm::vec3(std::cos(m.turn) * 0.35f, 1.0f, std::sin(m.turn) * 0.35f));
      VfxInstance stuck;
      stuck.matrix = glm::translate(glm::mat4(1.0f), at + lean * (length * 0.42f)) * alongAxis(-lean, m.turn) * glm::scale(glm::mat4(1.0f), {radius, length, radius});
      stuck.c = {env, packed(m.phase, radius), ease(0.35f, 1.0f, k), 0.7f * (1.0f - k)};
      crystals[cell].push_back(stuck);
      for (uint32_t j = 0; j < 5; ++j) {
        const float a = m.turn + static_cast<float>(j) * kTau / 5.0f + hash01(static_cast<uint32_t>(i), j) * 0.8f;
        const float out = 1.4f + 1.2f * hash01(static_cast<uint32_t>(i), j + 11), lift = 2.2f + 1.4f * hash01(static_cast<uint32_t>(i), j + 23);
        const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
        const float y = lift * t - 4.9f * t * t;
        if (y < -0.01f) continue;
        const float len = 0.24f + 0.2f * hash01(static_cast<uint32_t>(i), j + 37);
        VfxInstance inst;
        inst.matrix = glm::translate(glm::mat4(1.0f), at + dir * (0.2f + out * t) + glm::vec3(0, y, 0)) *
                      alongAxis(glm::normalize(glm::vec3(std::cos(a * 3.1f), 0.5f + std::sin(t * 8.0f), std::sin(a * 3.1f))), a * 3.0f + t * 9.0f) *
                      glm::scale(glm::mat4(1.0f), {0.055f, len, 0.055f});
        inst.c = {(1.0f - ease(0.6f, 1.0f, k)) * env, packed(hash01(static_cast<uint32_t>(i), j + 41), 0.055f), 0, 0.3f};
        crystals[cell].push_back(inst);
      }
      VfxInstance ring;
      ring.matrix = glm::translate(glm::mat4(1.0f), at + glm::vec3(0, 0.02f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.2f + 1.1f * ease(0.0f, 0.6f, k)));
      ring.c = {(1.0f - k) * env * 0.8f, m.phase, 0, 0};
      rings[cell].push_back(ring);
      VfxInstance puff;
      puff.a = glm::vec4(at + glm::vec3(0, 0.3f - 0.1f * k, 0), 0.0f);
      puff.b = {m.turn, k, 0.9f * env, 0.0f};
      puff.c = glm::vec4(1.1f + 1.9f * k, 0.8f + 0.6f * k, 0.0f, m.phase * 13.0f);
      puffs[cell].push_back(puff);
    } else {
      // It shatters: splinters thrown out, a frost ring spreading, a puff of ice dust.
      if (f >= 0.28f) continue;
      const float k = f / 0.28f, t = f / speed;
      const glm::vec3 at = ground + glm::vec3(0, 0.02f, 0);
      const float fadeOut = (1.0f - k) * env;
      for (uint32_t j = 0; j < 5; ++j) {
        const float a = m.turn + static_cast<float>(j) * kTau / 5.0f + hash01(static_cast<uint32_t>(i), j) * 0.8f;
        const float out = 1.0f + 0.7f * hash01(static_cast<uint32_t>(i), j + 11), lift = 1.6f + 0.9f * hash01(static_cast<uint32_t>(i), j + 23);
        const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
        const float y = lift * t - 4.9f * t * t;
        if (y < -0.01f) continue;
        const glm::vec3 p = at + dir * (out * t) + glm::vec3(0, y, 0);
        const glm::vec3 vel = dir * out + glm::vec3(0, lift - 9.8f * t, 0);
        const float len = 0.07f + 0.06f * hash01(static_cast<uint32_t>(i), j + 37);
        VfxInstance inst;
        inst.matrix = glm::translate(glm::mat4(1.0f), p) * alongAxis(-glm::normalize(vel), a * 3.0f) * glm::scale(glm::mat4(1.0f), {0.018f, len, 0.018f});
        inst.c = {fadeOut, 0, 0, 0};
        crystals[cell].push_back(inst);
      }
      VfxInstance ring;
      ring.matrix = glm::translate(glm::mat4(1.0f), at + glm::vec3(0, 0.02f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f + 0.5f * ease(0.0f, 1.0f, k)));
      ring.c = {fadeOut * 0.8f, m.phase, 0, 0};
      rings[cell].push_back(ring);
      VfxInstance puff;
      puff.a = glm::vec4(at + glm::vec3(0, 0.1f + 0.25f * k, 0), 0.0f);
      puff.b = {m.turn, 0.0f, 0.5f * std::pow(1.0f - k, 1.5f) * env, 0.0f};
      puff.c = glm::vec4(glm::vec2(0.3f + 0.6f * k), 0.0f, m.phase * 13.0f);
      puffs[cell].push_back(puff);
    }
  }
  draw(2.0f, bandFirst, bandGeometry().size(), glm::mat4(1.0f), 1.0f, {}, centre_, &bands);
  if (big) {
    // The storm's own body: a carpet of cold mist turning slowly over every hex, and the bank of cloud the ice falls out of; glints
    // in the air between.
    std::vector<VfxInstance> carpet, sparks;
    for (size_t h = 0; h < hexes_.size(); ++h) {
      const uint32_t salt = static_cast<uint32_t>(h) * 131u + 7u;
      for (uint32_t j = 0; j < 3; ++j) {
        const float a = age * (0.22f + 0.08f * static_cast<float>(j)) + hash01(salt, j) * kTau, r = 0.2f + 0.6f * hash01(salt, j + 10);
        VfxInstance low;
        low.a = glm::vec4(hexes_[h] + glm::vec3(std::cos(a) * r, 0.3f + 0.22f * static_cast<float>(j), std::sin(a) * r), 0.0f);
        low.b = {a, 0.3f, 0.6f * env, 0.0f};
        low.c = {2.6f, 1.2f, 0.0f, hash01(salt, j + 20) * 13.0f};
        carpet.push_back(low);
      }
      for (uint32_t j = 0; j < 2; ++j) {
        const float a = age * 0.12f + hash01(salt, j + 30) * kTau;
        VfxInstance high;
        high.a = glm::vec4(hexes_[h] + glm::vec3(std::cos(a) * 0.5f, 5.3f + 0.5f * static_cast<float>(j), std::sin(a) * 0.5f), 0.0f);
        high.b = {a, 0.15f, 0.5f * env, 0.0f};
        high.c = {3.6f, 1.7f, 0.0f, hash01(salt, j + 40) * 13.0f};
        carpet.push_back(high);
      }
      for (uint32_t j = 0; j < 3; ++j) {
        const float blink = std::fmod(age * (0.6f + hash01(salt, j + 50)) + hash01(salt, j + 60), 1.0f);
        const float a = hash01(salt, j + 70) * kTau, r = 0.8f * hash01(salt, j + 80);
        VfxInstance spark;
        spark.a = glm::vec4(hexes_[h] + glm::vec3(std::cos(a) * r, 0.4f + 3.6f * hash01(salt, j + 90), std::sin(a) * r), 0.0f);
        spark.b = {0.0f, 0.0f, env * ease(0.0f, 0.2f, blink) * (1.0f - ease(0.5f, 1.0f, blink)), 0.0f};
        spark.c = {0.26f, 0.26f, 0.0f, hash01(salt, j + 100)};
        sparks.push_back(spark);
      }
    }
    draw(10.0f, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, centre_, &carpet);
    draw(11.0f, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, centre_, &sparks);
  }
  for (size_t h = 0; h < hexes_.size(); ++h) {
    draw(7.0f, discFirst, discGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hexes_[h], &rings[h]);
    draw(3.0f, crystalFirst, crystalGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hexes_[h], &crystals[h]);
    draw(6.0f, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hexes_[h], &puffs[h]);
    draw(1.0f, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hexes_[h], &flakes[h]);
  }
}

}  // namespace ember
