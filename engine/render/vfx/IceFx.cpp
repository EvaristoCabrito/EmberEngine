#include "render/vfx/IceFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/IceParts.h"

namespace ember {

using namespace icefx;

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kIceLight{0.62f, 0.86f, 1.0f};  // the spell's own light: cyan
// The IceStorm material's ice look (shaders/vfx.hlsl).
constexpr float kBody = 8.0f, kGround = 9.0f, kMist = 10.0f, kGlint = 11.0f, kStreak = 12.0f, kFlash = 13.0f;

float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}
// Past its size and back (ice snaps up out of the ground).
float easeBack(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  const float c1 = 1.9f, c3 = c1 + 1.0f, u = t - 1.0f;
  return 1.0f + c3 * u * u * u + c1 * u * u;
}

// One batch of the ice look: `list` instances of geometry `first`, or one object at `model`.
void draw(VfxFrame& frame, float time, float mode, uint32_t first, size_t count, const glm::mat4& model, float opacity, glm::vec4 params1, glm::vec3 sight,
          const std::vector<VfxInstance>* list, float intensity = 1.0f) {
  VfxDraw d;
  d.gpu.model = model;
  d.gpu.params0 = {time, mode, intensity, opacity};
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
}

// A body of ice standing on `base`, its long point toward `up` (unit): c = alpha, seed, dissolve, glow.
VfxInstance shard(glm::vec3 base, glm::vec3 up, float length, float radius, float spin, float alpha, float seed, float dissolve, float glow) {
  VfxInstance inst;
  // (the mesh's long point is its -y end: the frame's +y runs down the crystal, so the point rises)
  inst.matrix = glm::translate(glm::mat4(1.0f), base + up * (length * 0.42f)) * alongAxis(-up, spin) * glm::scale(glm::mat4(1.0f), {radius, length, radius});
  inst.c = {alpha, std::min(seed, 0.999f) + std::floor(radius * 100.0f), dissolve, glow};  // (seed + 100 x radius: the shader's thickness)
  return inst;
}
// A loose piece of ice at `at`, its point along `axis`.
VfxInstance chip(glm::vec3 at, glm::vec3 axis, float length, float radius, float spin, float alpha, float seed, float dissolve = 0.0f, float glow = 0.3f) {
  VfxInstance inst;
  inst.matrix = glm::translate(glm::mat4(1.0f), at) * alongAxis(-axis, spin) * glm::scale(glm::mat4(1.0f), {radius, length, radius});
  inst.c = {alpha, std::min(seed, 0.999f) + std::floor(radius * 100.0f), dissolve, glow};
  return inst;
}
// A camera-facing card: b.y = its age 0..1 (the mist's), b.z alpha; c = size, seed.
VfxInstance card(glm::vec3 at, float angle, float age, float alpha, glm::vec2 size, float seed) {
  VfxInstance inst;
  inst.a = glm::vec4(at, 0.0f);
  inst.b = {angle, age, alpha, 0.0f};
  inst.c = {size.x, size.y, 0.0f, seed};
  return inst;
}
// A frost streak from `tail` to `head`, `width` wide, turned about its length to face the eye (u along, v across).
void streak(std::vector<VfxVertex>& out, glm::vec3 tail, glm::vec3 head, float width, glm::vec3 eye) {
  const glm::vec3 along = head - tail;
  glm::vec3 across = glm::cross(along, eye - (head + tail) * 0.5f);
  if (glm::dot(across, across) < 1e-8f) return;
  across = glm::normalize(across) * (width * 0.5f);
  const glm::vec3 n = glm::normalize(glm::cross(along, across));
  const VfxVertex a{tail - across, 0, n, 0}, b{head - across, 1, n, 0}, c{head + across, 1, n, 1}, d{tail + across, 0, n, 1};
  out.insert(out.end(), {a, b, c, a, c, d});
}

}  // namespace

IceSpell::Spec IceSpell::specOf(Style style) {
  switch (style) {
    // (each spell has its own body of ice, built in build(): of these only Frost's cluster reads its counts and lengths here)
    case Style::Lance: return {4, 1.9f, 1.2f, 0.0f, 1.25f, 1.0f, 1.6f};
    case Style::AbsoluteZero: return {8, 5.0f, 3.4f, 0.0f, 1.75f, 0.8f, 2.8f};
    case Style::Gale: return {5, 1.6f, 1.2f, 0.0f, 1.5f, 0.4f, 2.0f};
    case Style::Frost: break;
  }
  return {6, 2.1f, 1.15f, 0.1f, 1.4f, 0.8f, 1.1f};
}

IceSpell::IceSpell(Style style, glm::vec3 casterFeet, float casterHeight, std::vector<glm::vec3> hexes, float start, float impact, uint32_t seed)
    : style_(style),
      spec_(specOf(style)),
      feet_(casterFeet),
      hand_(casterFeet + glm::vec3(0.0f, casterHeight * 0.62f, 0.0f)),
      start_(start),
      impact_(std::max(impact, start + 0.3f)),
      hexes_(std::move(hexes)),
      seed_(seed) {
  auto flat = [](glm::vec3 a, glm::vec3 b) { return glm::length(glm::vec2(a.x - b.x, a.z - b.z)); };
  std::stable_sort(hexes_.begin(), hexes_.end(), [&](const glm::vec3& a, const glm::vec3& b) { return flat(a, feet_) < flat(b, feet_); });
  centre_ = feet_;
  if (!hexes_.empty()) {
    centre_ = glm::vec3(0.0f);
    for (const glm::vec3& h : hexes_) centre_ += h;
    centre_ /= static_cast<float>(hexes_.size());
    glm::vec2 d{hexes_.back().x - feet_.x, hexes_.back().z - feet_.z};
    if (glm::dot(d, d) > 1e-6f) {
      d = glm::normalize(d);
      direction_ = {d.x, 0.0f, d.y};
    }
    reach_ = flat(hexes_.back(), feet_);
    for (const glm::vec3& h : hexes_) radius_ = std::max(radius_, flat(h, centre_) + 1.0f);
  }
  float last = 0.0f;
  for (size_t i = 0; i < hexes_.size(); ++i) {
    float d = 0.0f;
    switch (style_) {
      case Style::Frost: d = 0.055f * static_cast<float>(i); break;
      case Style::Lance: d = std::max(0.0f, flat(hexes_[i], feet_) - 0.6f) / kLanceSpeed; break;
      case Style::AbsoluteZero: d = 0.05f * flat(hexes_[i], centre_) / 1.732f; break;
      case Style::Gale: d = 0.07f * flat(hexes_[i], feet_) / 1.732f; break;
    }
    d += 0.02f * hash01(seed_, static_cast<uint32_t>(i) + 900);  // no two hexes on the very same beat
    delay_.push_back(d);
    last = std::max(last, d);
  }
  end_ = impact_ + last + kErupt + spec_.linger + kExit + 0.15f;
}

void IceSpell::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  const uint32_t quadFirst = frame.addVertices(quadGeometry()), discFirst = frame.addVertices(discGeometry());
  const uint32_t crystalFirst = frame.addVertices(crystalGeometry());
  const float gather = impact_ - start_, age = now - start_, since = now - impact_;
  std::vector<VfxInstance> mist, ice, flashes, glints;
  std::vector<VfxVertex> streaks;
  const glm::vec3 side = glm::normalize(glm::cross(direction_, kUp));
  const float exitAt = kErupt + spec_.linger;  // on each hex, after its own impact
  auto flat = [](glm::vec3 a, glm::vec3 b) { return glm::length(glm::vec2(a.x - b.x, a.z - b.z)); };

  // ---- the ground, first: frost grown out from each hex's middle behind its bright edge, a crack mark struck into it; melted back
  // (the growth in reverse) at the exit. The Zero Absoluto freezes its whole area as one sheet, racing out from its middle.
  if (style_ == Style::AbsoluteZero && since > 0.0f) {
    const float hold = exitAt + 0.25f;
    draw(frame, since, kGround, discFirst, discGeometry().size(), glm::translate(glm::mat4(1.0f), centre_ + glm::vec3(0, 0.03f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(radius_ + 1.6f)),
         ease(0.0f, 0.06f, since), {0.05f + 1.2f * easeOut(0.0f, 0.4f, since), ease(hold, hold + kExit, since), 1.0f, hash01(seed_, 5)}, centre_, nullptr);
  }
  for (size_t i = 0; i < hexes_.size(); ++i) {
    const float t = since - delay_[i];
    if (t < 0.0f) continue;
    const float mark = style_ == Style::Lance && i + 1 != hexes_.size() ? spec_.mark * 0.5f : spec_.mark;
    draw(frame, t, kGround, discFirst, discGeometry().size(), glm::translate(glm::mat4(1.0f), hexes_[i] + glm::vec3(0, 0.04f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(spec_.frost)),
         ease(0.0f, 0.06f, t), {0.1f + 1.25f * easeOut(0.0f, 0.7f, t), ease(exitAt, exitAt + kExit, t), mark, hash01(seed_, static_cast<uint32_t>(i) + 10)}, hexes_[i], nullptr);
  }

  // ---- the build-up at the caster's hand: mist drawn in, small crystals forming, the blue glow growing; gone just after the release.
  if (since < 0.5f) {
    const float rise = ease(0.0f, 0.6f, age / gather), let = 1.0f - ease(0.0f, 0.3f, std::max(since, 0.0f));
    const float env = rise * let;
    for (uint32_t i = 0; i < 9; ++i) {
      // each puff of mist starts wide and low and is drawn up into the hand
      const float p = std::fmod(age / std::max(gather, 0.3f) * (0.8f + 0.5f * hash01(seed_, i + 20)) + hash01(seed_, i + 30), 1.0f);
      const float a = hash01(seed_, i + 40) * kTau + age * 1.3f, reach = (1.0f - p) * (1.0f - p) * (1.5f + 0.6f * hash01(seed_, i + 50));
      const glm::vec3 from = feet_ + glm::vec3(std::cos(a) * reach, 0.15f, std::sin(a) * reach);
      mist.push_back(card(glm::mix(from, hand_, p * p), a, 0.6f * (1.0f - p), 0.5f * env * ease(0.0f, 0.2f, p) * (1.0f - ease(0.8f, 1.0f, p)),
                          glm::vec2(1.1f - 0.6f * p), hash01(seed_, i + 60) * 13.0f));
    }
    if (style_ != Style::Lance)  // (the Lança holds its lance instead)
      for (uint32_t i = 0; i < 6; ++i) {
        const float a = static_cast<float>(i) * kTau / 6.0f + age * 2.6f;
        const float orbit = (0.5f - 0.14f * ease(0.7f, 1.0f, age / gather)) * rise;
        glm::vec3 at = hand_ + side * std::cos(a) * orbit + kUp * (std::sin(a) * 0.5f * orbit) + direction_ * (std::sin(a) * orbit * 0.7f);
        glm::vec3 axis = glm::normalize(side * -std::sin(a) + direction_ * std::cos(a) * 0.4f + kUp * 0.3f);
        float alpha = env, grown = easeBack(std::clamp(age / gather * 1.4f - 0.1f * static_cast<float>(i), 0.0f, 1.0f));
        if (since > 0.0f) {  // let go: thrown down the spell's way
          at += direction_ * (4.0f * since + 9.0f * since * since) + kUp * (0.2f * std::sin(since * 9.0f + a));
          axis = glm::normalize(direction_ + kUp * 0.05f);
          alpha = 1.0f - ease(0.15f, 0.5f, since);
        }
        if (alpha > 0.01f && grown > 0.0f) ice.push_back(chip(at, axis, 0.42f * grown, 0.07f * grown, a * 2.0f, alpha, hash01(seed_, i + 70), 0.0f, 1.0f));
      }
    flashes.push_back(card(hand_, 0.0f, 0.0f, 0.3f * rise * rise * let, glm::vec2(0.7f + 0.8f * rise), 0.0f));
    for (uint32_t i = 0; i < 6; ++i) {
      const float a = hash01(seed_, i + 80) * kTau + age * 0.7f, r = 0.25f + 0.4f * hash01(seed_, i + 90);
      glints.push_back(card(hand_ + glm::vec3(std::cos(a) * r, 0.3f * std::sin(age * 2.0f + a), std::sin(a) * r), 0.0f, 0.0f, 0.9f * env,
                            glm::vec2(0.18f + 0.14f * hash01(seed_, i + 100)), hash01(seed_, i + 110)));
    }
    lights.push_back({hand_ + glm::vec3(0, 0.1f, 0), kIceLight, (0.6f + 6.0f * rise * rise) * let, 3.4f});
  }

  // ---- the release: a short white flash at the hand and a burst of shards down the spell's way.
  if (since >= 0.0f && since < 0.45f) {
    if (since < 0.1f) {
      const float k = since / 0.1f;
      flashes.push_back(card(hand_, 0.0f, 0.0f, (1.0f - k) * (1.0f - k), glm::vec2(1.6f + 1.8f * k), 0.0f));
      lights.push_back({hand_, kIceLight, 12.0f * (1.0f - k), 4.5f});
    }
    for (uint32_t i = 0; i < 8; ++i) {
      const float a = hash01(seed_, i + 120) * kTau, spread = 0.25f + 0.55f * hash01(seed_, i + 130), speed = 5.0f + 5.0f * hash01(seed_, i + 140);
      const glm::vec3 dir = glm::normalize(direction_ + (side * std::cos(a) + kUp * std::sin(a)) * spread);
      const glm::vec3 vel = dir * speed + glm::vec3(0, -9.8f * since, 0);
      const glm::vec3 at = hand_ + dir * speed * since + glm::vec3(0, -4.9f * since * since, 0);
      if (at.y < feet_.y) continue;
      ice.push_back(chip(at, glm::normalize(vel), 0.24f + 0.16f * hash01(seed_, i + 150), 0.05f, a * 3.0f + since * 11.0f, 1.0f - since / 0.45f, hash01(seed_, i + 160)));
    }
  }

  // ==== LANÇA DE GELO: one great lance. It forms over the caster's hand out of the cold (through the noise, not out of nothing), is
  // thrown down the whole line to the board's edge, low and fast, a wide frost trail behind it, and ends driven into the ground at the
  // line's end, where it stands a while and then goes.
  if (style_ == Style::Lance) {
    constexpr float kLength = 4.4f, kRadius = 0.3f;
    const glm::vec3 held = hand_ + kUp * 1.0f - direction_ * 0.6f, heldAxis = glm::normalize(direction_ + kUp * 0.1f);
    const float far = reach_ + 0.7f, arrive = far / kLanceSpeed;
    const glm::vec3 landing = feet_ + direction_ * far;
    if (since < 0.0f) {
      const float made = ease(0.05f, 0.9f, age / gather);
      ice.push_back(chip(held + kUp * (0.08f * std::sin(age * 5.0f)), heldAxis, kLength * (0.6f + 0.4f * made), kRadius * (0.5f + 0.5f * made), age * 0.8f, 1.0f, hash01(seed_, 170),
                         1.0f - made, 1.0f));
      for (uint32_t i = 0; i < 8; ++i) {
        const float a = hash01(seed_, i + 171) * kTau + age * 3.0f, along = (hash01(seed_, i + 172) - 0.5f) * kLength * made;
        glints.push_back(card(held + heldAxis * along + (side * std::cos(a) + kUp * std::sin(a)) * 0.45f, 0.0f, 0.0f, made, glm::vec2(0.2f + 0.16f * hash01(seed_, i + 173)),
                              hash01(seed_, i + 174)));
      }
      lights.push_back({held, kIceLight, 5.0f * made, 4.0f});
    } else if (since < arrive) {
      const float gone = since * kLanceSpeed;
      const glm::vec3 to = landing + kUp * 0.9f;
      const glm::vec3 axis = glm::normalize(to - held), at = glm::mix(held, to, gone / far);
      ice.push_back(chip(at, axis, kLength, kRadius, since * 14.0f, 1.0f, hash01(seed_, 170), 0.0f, 1.0f));
      const float tail = std::min(gone + 0.5f, 7.0f);
      streak(streaks, at - axis * tail, at + axis * 1.2f, 1.2f, camera.position);
      streak(streaks, at - axis * tail * 0.6f, at + axis * 0.8f, 2.2f, camera.position);
      for (uint32_t i = 0; i < 12; ++i) {
        const float back = tail * hash01(seed_, i + 180), a = hash01(seed_, i + 190) * kTau + since * 9.0f + back * 2.0f;
        glints.push_back(card(at - axis * back + (side * std::cos(a) + kUp * std::sin(a)) * (0.2f + 0.35f * hash01(seed_, i + 200)), 0.0f, 0.0f, 1.0f - back / 7.0f,
                              glm::vec2(0.2f + 0.2f * hash01(seed_, i + 210)), hash01(seed_, i + 220)));
      }
      lights.push_back({at, kIceLight, 9.0f, 4.5f});
    } else {
      // driven in: its point in the ground, its shaft back up the way it came; a last shudder, then it stands
      const float s = since - arrive, hold = spec_.linger + 0.2f;
      const float out = std::clamp((s - hold) / kExit, 0.0f, 1.0f);
      const glm::vec3 axis = glm::normalize(direction_ - kUp * 0.62f + side * (0.05f * std::exp(-s * 9.0f) * std::sin(s * 60.0f)));
      ice.push_back(chip(landing + kUp * 0.1f - axis * (kLength * 0.3f), axis, kLength, kRadius, arrive * 14.0f, 1.0f, hash01(seed_, 170), out, 0.9f * (1.0f - out)));
      if (s < 0.16f) {
        const float k = s / 0.16f;
        flashes.push_back(card(landing + kUp * 0.5f, 0.0f, 0.0f, (1.0f - k) * (1.0f - k), glm::vec2(2.4f + 2.6f * k), 0.0f));
      }
      for (uint32_t j = 0; j < 14; ++j) {  // the ground breaks where it lands
        if (s > 0.8f) break;
        const float a = hash01(seed_, 300 + j) * kTau, outv = 1.5f + 2.2f * hash01(seed_, 320 + j), lift = 2.4f + 2.0f * hash01(seed_, 340 + j);
        const float y = 0.2f + lift * s - 4.9f * s * s;
        if (y < 0.0f) continue;
        const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
        ice.push_back(chip(landing + dir * (0.3f + outv * s) + glm::vec3(0, y, 0), glm::normalize(glm::vec3(std::cos(a * 3.1f), 0.5f + std::sin(s * 8.0f), std::sin(a * 3.1f))),
                           0.35f + 0.3f * hash01(seed_, 360 + j), 0.09f, a * 3.0f + s * 10.0f, 1.0f - ease(0.6f, 1.0f, s / 0.8f), hash01(seed_, 380 + j)));
      }
      for (uint32_t i = 0; i < 6; ++i) {
        const float up = hash01(seed_, i + 400), blink = std::fmod(s * (0.7f + hash01(seed_, i + 410)) + hash01(seed_, i + 420), 1.0f);
        glints.push_back(card(landing - axis * (kLength * (0.05f + 0.7f * up)) + kUp * 0.15f, 0.0f, 0.0f, (1.0f - out) * ease(0.0f, 0.2f, blink) * (1.0f - ease(0.5f, 1.0f, blink)),
                              glm::vec2(0.26f + 0.2f * hash01(seed_, i + 430)), hash01(seed_, i + 440)));
      }
      lights.push_back({landing + kUp * 1.0f, kIceLight, 12.0f * std::exp(-s * 7.0f) + 2.2f * (1.0f - out), 4.5f});
    }
    for (uint32_t i = 0; i < 10; ++i) {
      // mist left behind along the whole flight, sinking to the ground and spreading
      const float where = (static_cast<float>(i) + 0.5f) / 10.0f * far, born = where / kLanceSpeed, life = 1.6f, t = since - born;
      if (t < 0.0f || t > life) continue;
      const float p = t / life;
      mist.push_back(card(feet_ + direction_ * where + glm::vec3(0, 0.14f + 0.8f * (1.0f - easeOut(0.0f, 1.0f, p)), 0), hash01(seed_, i + 230) * kTau, p, 0.8f,
                          glm::vec2(1.5f + 1.6f * p, 0.9f + 0.4f * p), hash01(seed_, i + 240) * 13.0f));
    }
  }

  // ==== VENDAVAL GLACIAL: a wall of freezing wind out of the caster over the whole cone: long bands of frost racing outward in a
  // fan, a flight of ice in them, a front of cold mist running ahead; everything in it goes away from the caster.
  if (style_ == Style::Gale && since >= 0.0f && since < 1.5f) {
    constexpr float kHalf = 0.6f;  // the cone's half angle (its hexes are those within 37 degrees of the aim)
    const float far = reach_ + 1.2f, fade = 1.0f - ease(1.0f, 1.5f, since);
    auto fan = [&](float turn) { return direction_ * std::cos(turn) + side * std::sin(turn); };
    for (uint32_t i = 0; i < 34; ++i) {
      // a band of wind: born at the caster, racing out to the cone's far edge, again and again while the gale blows
      const float turn = (hash01(seed_, i + 500) * 2.0f - 1.0f) * kHalf, speed = 10.0f + 6.0f * hash01(seed_, i + 510);
      const float head = std::fmod(since * speed + hash01(seed_, i + 520) * far, far);
      const float length = 2.4f + 2.2f * hash01(seed_, i + 530), height = 0.25f + 1.9f * hash01(seed_, i + 540);
      const glm::vec3 dir = fan(turn), from = feet_ + glm::vec3(0, height, 0);
      streak(streaks, from + dir * std::max(head - length, 0.3f), from + dir * (head + 0.3f), 0.5f + 0.5f * hash01(seed_, i + 550), camera.position);
    }
    for (uint32_t i = 0; i < 46; ++i) {
      // the ice in the wind: tumbling, point first, each on its own line out through the cone
      const float turn = (hash01(seed_, i + 600) * 2.0f - 1.0f) * kHalf, speed = 9.0f + 7.0f * hash01(seed_, i + 610);
      const float gone = std::fmod(since * speed + hash01(seed_, i + 620) * far, far);
      const glm::vec3 dir = fan(turn);
      const glm::vec3 at = feet_ + dir * gone + glm::vec3(0, 0.3f + 1.7f * hash01(seed_, i + 630) + 0.25f * std::sin(gone * 2.0f + static_cast<float>(i)), 0);
      const float alpha = fade * ease(0.0f, 0.8f, gone) * (1.0f - ease(far - 1.2f, far, gone));
      if (alpha < 0.02f) continue;
      ice.push_back(chip(at, glm::normalize(dir + kUp * (0.25f * std::sin(since * 9.0f + static_cast<float>(i)))), 0.55f + 0.6f * hash01(seed_, i + 640), 0.09f + 0.05f * hash01(seed_, i + 650),
                         since * 8.0f + static_cast<float>(i), alpha, hash01(seed_, i + 660), 0.0f, 0.6f));
      if (i % 2 == 0) glints.push_back(card(at + kUp * 0.2f, 0.0f, 0.0f, alpha, glm::vec2(0.2f + 0.16f * hash01(seed_, i + 670)), hash01(seed_, i + 680)));
    }
    // the front: a bank of cold mist running out ahead of the wind
    const float front = std::min(since * 11.0f, far);
    for (uint32_t i = 0; i < 12; ++i) {
      const float turn = (static_cast<float>(i) / 11.0f * 2.0f - 1.0f) * kHalf;
      mist.push_back(card(feet_ + fan(turn) * front + glm::vec3(0, 0.6f, 0), turn, std::min(since / 1.3f, 1.0f), 0.9f * fade, glm::vec2(2.2f + 1.5f * since, 1.5f),
                          hash01(seed_, i + 690) * 13.0f));
    }
    lights.push_back({feet_ + direction_ * std::min(front, reach_ * 0.6f) + glm::vec3(0, 0.6f, 0), kIceLight, 9.0f * fade, 6.0f});
  }

  // ==== ZERO ABSOLUTO: the cold of the whole area falls in on its middle, and all of it freezes at once into one great body of ice:
  // a monolith at the middle and a crown of slabs opening outward over every hex round it (built below, hex by hex); the cold pours
  // off it over the ground for as long as it stands.
  if (style_ == Style::AbsoluteZero) {
    if (since < 0.0f) {
      const float pull = ease(0.0f, 1.0f, age / gather);
      for (uint32_t i = 0; i < 14; ++i) {
        const float p = std::fmod(age / std::max(gather, 0.3f) * (0.9f + 0.6f * hash01(seed_, i + 700)) + hash01(seed_, i + 710), 1.0f);
        const float a = hash01(seed_, i + 720) * kTau + age * 1.6f, r = (radius_ + 1.5f) * (1.0f - p) * (1.0f - p);
        mist.push_back(card(centre_ + glm::vec3(std::cos(a) * r, 0.3f + 1.2f * p, std::sin(a) * r), a, 0.5f * (1.0f - p), 0.7f * pull * ease(0.0f, 0.2f, p), glm::vec2(1.8f - 1.0f * p),
                            hash01(seed_, i + 730) * 13.0f));
        glints.push_back(card(centre_ + glm::vec3(std::cos(a * 1.7f) * r * 0.8f, 0.4f + 1.8f * p, std::sin(a * 1.7f) * r * 0.8f), 0.0f, 0.0f, pull, glm::vec2(0.2f + 0.2f * p),
                              hash01(seed_, i + 740)));
      }
      flashes.push_back(card(centre_ + glm::vec3(0, 1.4f, 0), 0.0f, 0.0f, 0.35f * pull * pull, glm::vec2((radius_ + 1.0f) * (1.6f - 1.2f * pull)), 0.0f));
      lights.push_back({centre_ + glm::vec3(0, 1.5f, 0), kIceLight, 7.0f * pull * pull, radius_ + 3.0f});
    } else {
      if (since < 0.28f) {
        const float k = since / 0.28f;
        flashes.push_back(card(centre_ + glm::vec3(0, 1.2f, 0), 0.0f, 0.0f, (1.0f - k) * (1.0f - k), glm::vec2((radius_ + 1.0f) * (1.4f + 2.2f * k)), 0.0f));
        lights.push_back({centre_ + glm::vec3(0, 1.6f, 0), kIceLight, 22.0f * (1.0f - k), radius_ + 5.0f});
      }
      const float gone = ease(exitAt, exitAt + kExit, since);
      for (uint32_t i = 0; i < 18; ++i) {
        // the cold pouring off the ice: down its sides and away over the ground
        const float life = 1.8f + 0.7f * hash01(seed_, i + 750);
        const float p = std::fmod(since / life + hash01(seed_, i + 760), 1.0f);
        const float a = hash01(seed_, i + 770) * kTau, r = 0.6f + (radius_ + 1.6f) * easeOut(0.0f, 1.0f, p);
        mist.push_back(card(centre_ + glm::vec3(std::cos(a) * r, 0.16f + 1.4f * (1.0f - easeOut(0.0f, 0.5f, p)), std::sin(a) * r), a, p, 0.95f * (1.0f - gone),
                            glm::vec2(2.0f + 2.2f * p, 1.1f + 0.5f * p), hash01(seed_, i + 780) * 13.0f));
      }
      lights.push_back({centre_ + glm::vec3(0, 1.8f, 0), kIceLight, 3.0f * (1.0f - gone), radius_ + 3.0f});
    }
  }

  // ---- every hex in its turn: what the spell leaves on it.
  size_t middle = 0;
  for (size_t i = 1; i < hexes_.size(); ++i)
    if (flat(hexes_[i], centre_) < flat(hexes_[middle], centre_)) middle = i;
  for (size_t i = 0; i < hexes_.size(); ++i) {
    const float t = since - delay_[i];
    if (t < 0.0f) continue;
    const glm::vec3 hex = hexes_[i];
    const uint32_t salt = seed_ + static_cast<uint32_t>(i) * 977u;
    const float exit = std::clamp((t - exitAt) / kExit, 0.0f, 1.0f);  // 0 standing .. 1 gone
    const float held = ease(0.0f, kErupt, t) * (1.0f - ease(0.0f, 0.5f, exit));
    glm::vec3 awayFlat = hex - feet_;
    awayFlat.y = 0.0f;
    const glm::vec3 away = glm::dot(awayFlat, awayFlat) > 1e-6f ? glm::normalize(awayFlat) : direction_;  // from the caster to this hex
    float top = 1.0f;  // how high this hex's ice stands (its glints, its splinters)

    if (style_ == Style::Frost) {
      // FROST: a cluster of spikes snapped up out of the ground (each a beat after the tall one), fanned all round; a last shiver,
      // then dissolved through the noise while it sinks a little.
      top = spec_.tall;
      for (int k = 0; k < spec_.shards; ++k) {
        const uint32_t n = static_cast<uint32_t>(k);
        const bool tall = k == 0;
        const float a = tall ? 0.0f : (static_cast<float>(k) + 0.6f * hash01(salt, n)) * kTau / static_cast<float>(spec_.shards - 1);
        const float out = tall ? 0.0f : 0.3f + 0.3f * hash01(salt, n + 10);
        const float lean = tall ? 0.07f : 0.38f + 0.3f * hash01(salt, n + 20);
        const float length = (tall ? spec_.tall : spec_.small) * (0.85f + 0.3f * hash01(salt, n + 30));
        const float radius = (tall ? 0.42f : 0.26f) * (0.85f + 0.35f * hash01(salt, n + 40)) * std::clamp(length / 2.0f, 0.6f, 1.25f);
        const float grow = easeBack((t - (tall ? 0.0f : 0.02f + 0.06f * hash01(salt, n + 50))) / 0.17f);
        if (grow <= 0.0f) continue;
        const glm::vec3 dir = glm::normalize(glm::vec3(std::cos(a) * std::sin(lean), std::cos(lean), std::sin(a) * std::sin(lean)) + direction_ * spec_.lean);
        glm::vec3 base = hex + glm::vec3(std::cos(a) * out, 0.0f, std::sin(a) * out);
        const float shiver = ease(exitAt - 0.18f, exitAt, t) * (1.0f - exit) * 0.02f * std::sin(t * 95.0f + static_cast<float>(k) * 2.0f);
        base.x += shiver, base.z -= shiver;
        base.y -= 0.1f * exit * length;
        ice.push_back(shard(base, dir, length * grow, radius * (0.55f + 0.45f * std::min(grow, 1.0f)), hash01(salt, n + 60) * kTau, 1.0f, hash01(salt, n + 70), exit,
                            0.75f * (1.0f - exit)));
      }
    } else if (style_ == Style::Lance) {
      // LANÇA: the furrow the lance ploughs as it passes: ice thrown up on both sides of the line, raked forward and outward like a
      // wake, the pair of each hex taller than the last one's was.
      top = 1.6f;
      for (int k = 0; k < 4; ++k) {
        const uint32_t n = static_cast<uint32_t>(k);
        const float sgn = k % 2 == 0 ? 1.0f : -1.0f, row = k < 2 ? 0.0f : 0.55f;
        const float length = (k < 2 ? 1.9f : 1.2f) * (0.85f + 0.3f * hash01(salt, n + 30));
        const float grow = easeBack((t - 0.03f * static_cast<float>(k)) / 0.14f);
        if (grow <= 0.0f) continue;
        const glm::vec3 dir = glm::normalize(kUp * 0.5f + direction_ * 0.62f + side * (sgn * 0.6f));
        const glm::vec3 base = hex + side * (sgn * (0.5f + 0.2f * hash01(salt, n + 10))) + direction_ * (row - 0.3f) - kUp * (0.1f * exit * length);
        ice.push_back(shard(base, dir, length * grow, 0.24f * (0.55f + 0.45f * std::min(grow, 1.0f)), hash01(salt, n + 60) * kTau, 1.0f, hash01(salt, n + 70), exit,
                            0.6f * (1.0f - exit)));
      }
    } else if (style_ == Style::AbsoluteZero) {
      // ZERO ABSOLUTO: nothing rises: the air itself turns to ice, all at once, through the noise (its dissolve run backwards). The
      // middle hex is the monolith; every other hex a great slab leaning out from the middle, the crown round it.
      const bool core = i == middle;
      const float formed = ease(0.0f, 0.22f, t), swell = 0.86f + 0.14f * formed;
      const float through = std::max(1.0f - formed, exit);
      glm::vec3 outFlat = hex - hexes_[middle];
      outFlat.y = 0.0f;
      const float outLen = glm::length(outFlat);
      const glm::vec3 out = outLen > 1e-4f ? outFlat / outLen : direction_;
      const float shiver = ease(exitAt - 0.2f, exitAt, t) * (1.0f - exit) * 0.03f * std::sin(t * 90.0f + static_cast<float>(i));
      if (core) {
        top = 5.0f;
        ice.push_back(shard(hex + glm::vec3(shiver, 0, -shiver), kUp, 5.0f * swell, 0.95f * swell, hash01(salt, 60) * kTau, 1.0f, hash01(salt, 70), through, 1.0f * (1.0f - exit)));
        for (int k = 0; k < 5; ++k) {  // its shoulders
          const uint32_t n = static_cast<uint32_t>(k);
          const float a = (static_cast<float>(k) + 0.5f * hash01(salt, n + 80)) * kTau / 5.0f;
          const glm::vec3 dir = glm::normalize(kUp + glm::vec3(std::cos(a), 0, std::sin(a)) * 0.3f);
          ice.push_back(shard(hex + glm::vec3(std::cos(a), 0, std::sin(a)) * 0.6f, dir, (2.6f + 0.8f * hash01(salt, n + 90)) * swell, 0.5f * swell, a, 1.0f, hash01(salt, n + 100),
                              through, 0.8f * (1.0f - exit)));
        }
      } else {
        top = 3.4f;
        const glm::vec3 dir = glm::normalize(kUp + out * 0.55f);
        const glm::vec3 base = hexes_[middle] + out * (outLen * 0.78f) + glm::vec3(shiver, 0, -shiver);
        ice.push_back(shard(base, dir, (3.3f + 0.8f * hash01(salt, 30)) * swell, 0.7f * swell, hash01(salt, 60) * kTau, 1.0f, hash01(salt, 70), through, 0.85f * (1.0f - exit)));
        const glm::vec3 across = glm::normalize(glm::cross(out, kUp));
        for (int k = 0; k < 2; ++k) {  // the smaller blades beside it, closing the crown
          const float sgn = k == 0 ? 1.0f : -1.0f;
          ice.push_back(shard(base + across * (sgn * 0.7f) + out * 0.25f, glm::normalize(kUp + out * 0.85f + across * (sgn * 0.3f)), (1.7f + 0.6f * hash01(salt, 31 + static_cast<uint32_t>(k))) * swell,
                              0.4f * swell, hash01(salt, 61 + static_cast<uint32_t>(k)) * kTau, 1.0f, hash01(salt, 71 + static_cast<uint32_t>(k)), through, 0.7f * (1.0f - exit)));
        }
      }
    } else {
      // VENDAVAL: no ice stands in a gale: what freezes on the ground is combed flat by the wind, long low blades all pointing away
      // from the caster, the snow driven off them.
      top = 0.6f;
      const glm::vec3 across = glm::normalize(glm::cross(away, kUp));
      for (int k = 0; k < 5; ++k) {
        const uint32_t n = static_cast<uint32_t>(k);
        const float lateral = (static_cast<float>(k) - 2.0f) * 0.36f + 0.15f * (hash01(salt, n + 10) - 0.5f);
        const float length = (1.5f + 1.1f * hash01(salt, n + 30)) * (k == 2 ? 1.25f : 1.0f);
        const float grow = easeBack((t - 0.03f * hash01(salt, n + 50)) / 0.2f);
        if (grow <= 0.0f) continue;
        const glm::vec3 dir = glm::normalize(away + kUp * (0.2f + 0.14f * hash01(salt, n + 20)) + across * (0.18f * (hash01(salt, n + 21) - 0.5f)));
        const glm::vec3 base = hex + across * lateral - away * 0.6f - kUp * (0.06f * exit);
        ice.push_back(shard(base, dir, length * grow, 0.2f * (0.55f + 0.45f * std::min(grow, 1.0f)), hash01(salt, n + 60) * kTau, 1.0f, hash01(salt, n + 70), exit,
                            0.6f * (1.0f - exit)));
      }
      if (t > 0.1f)
        for (uint32_t j = 0; j < 4; ++j) {  // snow blown off along the ground, always away
          const float life = 1.0f + 0.5f * hash01(salt, 560 + j);
          const float p = std::fmod((t - 0.1f) / life + hash01(salt, 570 + j), 1.0f);
          mist.push_back(card(hex + away * (2.6f * p - 0.4f) + across * (0.8f * (hash01(salt, 580 + j) - 0.5f)) + glm::vec3(0, 0.25f, 0), 0.0f, p, 0.8f * (1.0f - exit),
                              glm::vec2(1.6f + 1.4f * p, 0.7f), hash01(salt, 590 + j) * 13.0f));
        }
    }

    // glints on the ice while it stands
    if (t > kErupt)
      for (uint32_t g = 0; g < 5; ++g) {
        const float a = hash01(salt, g + 400) * kTau, r = 0.7f * hash01(salt, g + 405);
        const float blink = std::fmod(t * (0.7f + hash01(salt, g + 410)) + hash01(salt, g + 420), 1.0f);
        glints.push_back(card(hex + glm::vec3(std::cos(a) * r, top * (0.2f + 0.75f * hash01(salt, g + 415)), std::sin(a) * r), 0.0f, 0.0f,
                              held * ease(0.0f, 0.2f, blink) * (1.0f - ease(0.5f, 1.0f, blink)), glm::vec2(0.24f + 0.22f * hash01(salt, g + 430)), hash01(salt, g + 440)));
      }
    // Chips thrown out at the impact; splinters falling as the ice goes (the bigger the ice, the bigger its pieces). Each turns
    // about its own fixed axis as it flies.
    auto flung = [&](float when, uint32_t count, uint32_t channel, float height, float lift0, float life, float size) {
      const float s = t - when;
      if (s < 0.0f || s > life) return;
      for (uint32_t j = 0; j < count; ++j) {
        const float a = hash01(salt, channel + j) * kTau, outv = (0.9f + 1.4f * hash01(salt, channel + j + 20)) * (0.7f + 0.5f * size);
        const float lift = lift0 + 1.3f * hash01(salt, channel + j + 40), h0 = height * (0.3f + 0.7f * hash01(salt, channel + j + 50));
        const float y = h0 + lift * s - 4.9f * s * s;
        if (y < 0.0f) continue;
        const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
        const glm::vec3 axis = glm::normalize(glm::vec3(std::cos(a * 3.1f), 0.4f, std::sin(a * 3.1f)));
        ice.push_back(chip(hex + dir * (0.2f + outv * s) + glm::vec3(0, y, 0), glm::normalize(axis + dir * std::sin(s * 9.0f + a)),
                           (0.2f + 0.16f * hash01(salt, channel + j + 60)) * size, 0.045f * size, a * 3.0f + s * 10.0f, 1.0f - ease(0.6f, 1.0f, s / life), hash01(salt, channel + j + 70)));
      }
    };
    const float size = style_ == Style::AbsoluteZero ? 2.2f : style_ == Style::Frost ? 1.0f : 1.4f;
    if (style_ != Style::AbsoluteZero) flung(0.0f, 4, 100, 0.2f, 1.6f, 0.65f, size);
    flung(exitAt + 0.05f, style_ == Style::AbsoluteZero ? 9u : 6u, 200, top * 0.7f, style_ == Style::AbsoluteZero ? 1.6f : 0.4f, 0.8f, size);

    // The white flash of the impact (the Zero Absoluto has its one great flash instead).
    if (style_ != Style::AbsoluteZero && t < 0.14f) {
      const float k = t / 0.14f;
      flashes.push_back(card(hex + glm::vec3(0, 0.5f, 0), 0.0f, 0.0f, (1.0f - k) * (1.0f - k), glm::vec2(1.4f + 1.8f * k), 0.0f));
    }
    // Cold mist: a puff at the impact; while the ice stands, heavy mist that sinks and spreads over the ground (it never rises).
    if (style_ != Style::Gale) {
      if (t < 0.6f) {
        const float k = t / 0.6f;
        mist.push_back(card(hex + glm::vec3(0, 0.35f - 0.15f * k, 0), hash01(salt, 500) * kTau, k, 1.0f, glm::vec2(1.3f + 2.0f * k, 1.0f + 0.7f * k), hash01(salt, 501) * 13.0f));
      }
      if (t > 0.12f)
        for (uint32_t j = 0; j < 6; ++j) {
          const float life = 1.5f + 0.5f * hash01(salt, 510 + j);
          const float p = std::fmod((t - 0.12f) / life + hash01(salt, 520 + j), 1.0f);
          const float a = hash01(salt, 530 + j) * kTau, r = 0.15f + (0.5f + 0.6f * hash01(salt, 540 + j)) * easeOut(0.0f, 1.0f, p);
          mist.push_back(card(hex + glm::vec3(std::cos(a) * r, 0.14f + 0.5f * (1.0f - easeOut(0.0f, 0.8f, p)), std::sin(a) * r), a, p,
                              0.95f * ease(0.12f, 0.4f, t) * (1.0f - exit), glm::vec2(1.6f + 1.6f * p, 0.9f + 0.35f * p), hash01(salt, 550 + j) * 13.0f));
        }
    }

    // The cold light: a flash at the impact settling into a glow that goes as the ice does (not on every hex of a wide area).
    if (i < 4 || i % 3 == 0) {
      const float flash = 8.0f * std::exp(-t * 9.0f), glow = 1.5f * held;
      const float burst = t > exitAt ? 2.5f * std::exp(-(t - exitAt) * 8.0f) : 0.0f;
      lights.push_back({hex + glm::vec3(0, 0.9f, 0), kIceLight, flash + glow + burst, 3.4f});
    }
  }

  // ---- the layers, always in this order: mist, streaks, ice, flash, glints (the ground went first).
  draw(frame, age, kMist, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, centre_, &mist);
  if (!streaks.empty()) draw(frame, age, kStreak, frame.addVertices(streaks), streaks.size(), glm::mat4(1.0f), 1.0f, {0.85f, hash01(seed_, 7), 0, 0}, centre_, nullptr);
  draw(frame, age, kBody, crystalFirst, crystalGeometry().size(), glm::mat4(1.0f), 1.0f, {}, centre_, &ice);
  draw(frame, age, kFlash, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, centre_, &flashes);
  draw(frame, age, kGlint, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, centre_, &glints);
}

}  // namespace ember
