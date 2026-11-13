#include "render/vfx/HolyFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/IceParts.h"
#include "render/vfx/SigilParts.h"

namespace ember {

using namespace icefx;  // ease, hash01, quadGeometry, discGeometry

namespace {

constexpr float kPi = std::numbers::pi_v<float>, kTau = 2.0f * kPi;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
// Warm white at the core, pale gold in the middle, soft gold at the edges (never a flat yellow); Curar Doença's is pale and clean.
constexpr glm::vec3 kCore{1.0f, 0.95f, 0.8f}, kGold{1.0f, 0.78f, 0.4f}, kEdge{0.95f, 0.62f, 0.24f};
constexpr glm::vec3 kClean{0.80f, 1.0f, 0.92f}, kCleanEdge{0.45f, 0.9f, 0.78f}, kSick{0.10f, 0.13f, 0.05f};
// The Holy material's soft looks (shaders/vfx.hlsl).
constexpr float kPool = 1.0f, kMote = 2.0f, kLine = 4.0f, kRay = 5.0f, kBloom = 7.0f, kBeam = 8.0f;

float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

struct Painter {
  VfxFrame& frame;
  const Camera& camera;
  float time;
  glm::vec3 sight;
  uint32_t quad, disc, ring, star;

  void draw(float mode, uint32_t first, size_t count, const glm::mat4& model, glm::vec3 colour, glm::vec3 core, float intensity, float opacity, float extra,
            const std::vector<VfxInstance>* list = nullptr) const {
    if (opacity <= 0.003f) return;
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {time, mode, intensity, opacity};
    d.gpu.params1 = {core, extra};
    d.gpu.color = {colour, 1.0f};
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    if (list) {
      if (list->empty()) return;
      d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), list->begin(), list->end());
      d.instances = static_cast<uint32_t>(list->size());
    }
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
    d.gpu.sightAt = sight;
    d.blend = VfxBlend::Additive;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
  // A card of light from `from` (v 0) to `to` (v 1), `w0` / `w1` wide at its ends, turned about its length to face the eye.
  void strip(float mode, glm::vec3 from, glm::vec3 to, float w0, float w1, glm::vec3 colour, glm::vec3 core, float intensity, float opacity, float seed) const {
    const glm::vec3 along = to - from;
    glm::vec3 across = glm::cross(along, camera.position - (from + to) * 0.5f);
    if (glm::dot(across, across) < 1e-8f || opacity <= 0.003f) return;
    across = glm::normalize(across);
    const glm::vec3 n = glm::normalize(glm::cross(along, across));
    const VfxVertex a{from - across * (w0 * 0.5f), 0, n, 0}, b{from + across * (w0 * 0.5f), 1, n, 0}, c{to + across * (w1 * 0.5f), 1, n, 1}, d{to - across * (w1 * 0.5f), 0, n, 1};
    const uint32_t first = frame.addVertices({a, b, c, a, c, d});
    draw(mode, first, 6, glm::mat4(1.0f), colour, core, intensity, opacity, seed);
  }
  // The mark on the ground, the original holy spells' own (vfx/SigilParts.h; the user, 2026-11-13: "os mesmos estilos de runas
  // esotericas"): the fine ring with its tick marks, the hexagram turning against it, a pool of light under them; `radius` wide,
  // grown out with `cover` (0..1). Lying on the floor under the characters, as theirs.
  void circle(glm::vec3 at, float radius, float cover, glm::vec3 colour, glm::vec3 core, float intensity, float opacity) const {
    const float rad = radius * (0.4f + 0.6f * std::min(cover, 1.0f));
    const glm::mat4 base = glm::translate(glm::mat4(1.0f), at + glm::vec3(0, 0.05f, 0));
    const size_t from = frame.draws.size();
    draw(kLine, ring, sigil::ringGeometry().size(), base * glm::rotate(glm::mat4(1.0f), time * 0.22f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rad)), colour, core, 2.6f * intensity,
         opacity, 0.0f);
    draw(kLine, star, sigil::starGeometry().size(), base * glm::rotate(glm::mat4(1.0f), -time * 0.55f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(rad)), glm::mix(colour, core, 0.5f), core,
         2.8f * intensity, opacity, 0.0f);
    draw(kPool, disc, discGeometry().size(), base * glm::scale(glm::mat4(1.0f), {rad, 1.0f, rad}), colour, core, 0.42f * intensity, 0.7f * opacity, 0.3f + 0.7f * std::min(cover, 1.0f));
    for (size_t k = from; k < frame.draws.size(); ++k) frame.draws[k].underCards = true;
  }
  // The same mark in the air (the sky's opening): the ring and the hexagram, `radius` wide.
  void mark(glm::vec3 at, float radius, glm::vec3 colour, glm::vec3 core, float intensity, float opacity) const {
    const glm::mat4 base = glm::translate(glm::mat4(1.0f), at);
    draw(kLine, ring, sigil::ringGeometry().size(), base * glm::rotate(glm::mat4(1.0f), time * 0.22f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(radius)), colour, core, intensity, opacity, 0.0f);
    draw(kLine, star, sigil::starGeometry().size(), base * glm::rotate(glm::mat4(1.0f), -time * 0.55f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(radius)), glm::mix(colour, core, 0.5f), core,
         intensity, opacity, 0.0f);
  }
  // The fine ring alone, in the air (a halo, a sweep through the body).
  void halo(glm::vec3 at, float radius, glm::vec3 colour, glm::vec3 core, float intensity, float opacity) const {
    draw(kLine, ring, sigil::ringGeometry().size(), glm::translate(glm::mat4(1.0f), at) * glm::rotate(glm::mat4(1.0f), time * 0.6f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(radius)), colour,
         core, intensity, opacity, 0.0f);
  }
};

// A camera-facing dot of light (a mote, or a bloom): a = centre + size, b = turn, -, alpha, intensity, c = tint.
VfxInstance dot(glm::vec3 at, float size, float alpha, glm::vec3 tint, float intensity = 1.0f) {
  VfxInstance inst;
  inst.a = glm::vec4(at, size);
  inst.b = {0.0f, 0.0f, alpha, intensity};
  inst.c = glm::vec4(tint, 0.0f);
  return inst;
}

}  // namespace

HolySpell::HolySpell(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, std::vector<glm::vec3> area, float impact, uint32_t seed)
    : style_(style), caster_(casterFeet), feet_(targetFeet), casterHeight_(std::max(casterHeight, 1.2f)), height_(std::max(targetHeight, 1.2f)), impact_(impact), area_(std::move(area)), seed_(seed) {
  for (const glm::vec3& h : area_) radius_ = std::max(radius_, glm::length(glm::vec2(h.x - feet_.x, h.z - feet_.z)) + 1.0f);
  start_ = impact_ - leadIn(style_);
  end_ = impact_ + (style_ == Style::Minor ? 1.5f : style_ == Style::Medium ? 2.6f : style_ == Style::Hands ? 2.0f : style_ == Style::Disease ? 2.2f : 2.0f);
}

void HolySpell::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  const Painter p{frame, camera, now - start_, feet_, frame.addVertices(quadGeometry()), frame.addVertices(discGeometry()), frame.addVertices(sigil::ringGeometry()),
                  frame.addVertices(sigil::starGeometry())};
  const float lead = impact_ - start_, age = now - start_, since = now - impact_, life = end_ - impact_;
  const float in = ease(0.0f, 1.0f, age / lead);                            // the light arriving
  const float out = 1.0f - ease(life * 0.55f, life, std::max(since, 0.0f));  // ... and going
  const float env = in * out;
  const glm::vec3 chest = feet_ + kUp * (height_ * 0.55f), head = feet_ + kUp * height_;
  std::vector<VfxInstance> motes, blooms;

  // Motes that rise slowly round the body (healing rises), `count` of them inside `reach`, up to `top`.
  auto rising = [&](uint32_t count, float reach, float top, glm::vec3 tint, float size) {
    for (uint32_t i = 0; i < count; ++i) {
      const float cycle = 1.4f + 1.2f * hash01(seed_, i + 10), q = std::fmod(std::max(since, 0.0f) / cycle + hash01(seed_, i + 200), 1.0f);
      const float a = hash01(seed_, i + 400) * kTau + age * (0.5f + hash01(seed_, i + 600)), r = reach * (0.25f + 0.75f * hash01(seed_, i + 800)) * (1.0f - 0.3f * q);
      motes.push_back(dot(feet_ + glm::vec3(std::cos(a) * r, 0.1f + top * q, std::sin(a) * r), size * (0.6f + 0.8f * hash01(seed_, i + 1000)),
                          env * ease(0.0f, 0.15f, q) * (1.0f - ease(0.6f, 1.0f, q)) * ease(-0.05f, 0.2f, since), tint, 1.6f));
    }
  };

  switch (style_) {
    case Style::Minor: {
      // One soft pillar: three rays, a little apart and a little slanted, down onto the ally; a small circle; a few motes.
      const float tall = height_ + 4.5f;
      for (int k = 0; k < 3; ++k) {
        const float a = static_cast<float>(k) * kTau / 3.0f + 0.4f;
        const glm::vec3 top = feet_ + glm::vec3(std::cos(a) * 0.5f + 0.5f, tall, std::sin(a) * 0.5f);
        p.strip(kRay, feet_ + glm::vec3(std::cos(a) * 0.12f, 0.0f, std::sin(a) * 0.12f), top, 1.2f, 1.9f, kGold, kCore, 1.1f, 0.5f * env, hash01(seed_, static_cast<uint32_t>(k)));
      }
      p.circle(feet_, 1.4f, 0.2f + 1.0f * easeOut(-lead, 0.3f, since), kGold, kCore, 1.3f, env);
      blooms.push_back(dot(chest, 2.0f + 0.25f * std::sin(age * 5.0f), 0.55f * env, kGold));
      rising(14, 0.8f, height_ + 0.8f, kGold, 0.1f);
      lights.push_back({chest, kGold, 7.0f * env, 4.5f});
      break;
    }
    case Style::Medium: {
      // The sky opens over the ally: a ring of light high above; a wide fan of god rays down through it; a halo settling from over
      // the head to the feet; a great circle with its rune; a cloud of motes.
      const float sky = height_ + 7.0f;
      p.mark(feet_ + kUp * sky, 3.2f * (0.4f + 0.6f * easeOut(-lead, 0.2f, since)), kGold, kCore, 1.6f, 0.9f * env);
      for (int k = 0; k < 9; ++k) {
        const float a = static_cast<float>(k) * kTau / 9.0f + age * 0.08f, spread = k == 0 ? 0.0f : 1.0f;
        const glm::vec3 top = feet_ + glm::vec3(std::cos(a) * 0.9f * spread, sky, std::sin(a) * 0.9f * spread);
        const glm::vec3 foot = feet_ + glm::vec3(std::cos(a) * 1.7f * spread, 0.0f, std::sin(a) * 1.7f * spread);
        const float len = easeOut(-lead, 0.15f, since);  // they reach down as the light arrives
        p.strip(kRay, top + (foot - top) * len, top, 1.9f, 1.3f, kGold, kCore, 1.2f, (k == 0 ? 0.7f : 0.36f) * env, hash01(seed_, static_cast<uint32_t>(k)));
      }
      p.circle(feet_, 2.6f, 0.15f + 1.05f * easeOut(-lead * 0.5f, 0.5f, since), kGold, kCore, 1.6f, env);
      const float settle = ease(0.0f, 1.3f, since);
      p.halo(feet_ + kUp * ((height_ + 0.6f) * (1.0f - settle) + 0.1f), 1.1f, kGold, kCore, 2.0f, env * ease(-0.1f, 0.1f, since) * (1.0f - ease(1.2f, 1.6f, since)));
      blooms.push_back(dot(chest, 3.4f + 0.4f * std::sin(age * 4.0f), 0.7f * env, kGold));
      blooms.push_back(dot(feet_ + kUp * sky, 5.0f, 0.5f * env, kCore));
      rising(44, 1.9f, height_ + 2.2f, kGold, 0.12f);
      lights.push_back({chest + kUp * 0.5f, kGold, (12.0f + 8.0f * std::exp(-std::max(since, 0.0f) * 5.0f) * ease(-0.05f, 0.0f, since)) * env, 7.0f});
      break;
    }
    case Style::Hands: {
      // The laying on of hands. No sky: one warm light where the healer's hands rest on the ally, and from there the light spreads
      // through the body as a single soft sheet rising from the feet to over the head; two streams of motes wind from the healer's
      // hands into the ally; the sigil under the ally, small. Quiet: it grows, holds and goes.
      const glm::vec3 hands = caster_ + kUp * (casterHeight_ * 0.6f);
      const glm::vec3 touch = glm::length(chest - hands) > 0.4f ? chest + glm::normalize(hands - chest) * 0.3f : chest;
      const float fill = ease(0.0f, 0.9f, since);
      p.strip(kRay, feet_, feet_ + kUp * ((height_ + 1.6f) * (0.25f + 0.75f * fill)), 1.5f, 1.9f, kGold, kCore, 1.0f, 0.55f * env, 0.4f);
      p.strip(kBeam, hands, touch, 0.5f, 0.5f, kGold, kCore, 0.9f, 0.6f * env, 0.7f);
      blooms.push_back(dot(touch, 1.5f + 0.15f * std::sin(age * 4.0f), 0.75f * env, kGold));
      blooms.push_back(dot(chest, 2.6f, 0.3f * env * fill, kGold));
      p.circle(feet_, 1.5f, 0.2f + 0.8f * fill, kGold, kCore, 1.2f, env);
      for (uint32_t i = 0; i < 28; ++i) {  // the two streams winding in from the hands
        const float cycle = 0.9f + 0.4f * hash01(seed_, i + 10), q = std::fmod(age / cycle + hash01(seed_, i + 200), 1.0f);
        const float turn = q * kTau * 1.5f + (i % 2 ? kPi : 0.0f), wide = 0.35f * std::sin(kPi * q);
        const glm::vec3 way = glm::mix(hands, touch, q);
        motes.push_back(dot(way + glm::vec3(std::cos(turn) * wide, std::sin(turn) * wide, std::sin(turn + 1.3f) * wide), 0.09f, env * ease(0.0f, 0.15f, q) * (1.0f - ease(0.85f, 1.0f, q)), kGold, 1.6f));
      }
      rising(12, 0.7f, height_ + 0.6f, kGold, 0.09f);
      lights.push_back({touch, kGold, 8.0f * env, 4.5f});
      break;
    }
    case Style::Disease: {
      // Pale and clean: two rings of light sweep up through the body from the feet; the sickness leaves it as dark motes thrown
      // outward and down; a wide clean circle spreads; a slim pale ray from above.
      p.strip(kRay, feet_, feet_ + kUp * (height_ + 5.0f) + glm::vec3(0.3f, 0, 0), 1.4f, 2.2f, kClean, kCore, 1.0f, 0.5f * env, 0.3f);
      for (int k = 0; k < 2; ++k) {
        const float sweep = std::fmod(std::max(since, 0.0f) / 0.9f + 0.5f * static_cast<float>(k), 1.0f);
        p.halo(feet_ + kUp * (0.1f + (height_ + 0.3f) * sweep), 1.0f, kClean, kCore, 2.0f, env * ease(-0.05f, 0.05f, since) * ease(0.0f, 0.15f, sweep) * (1.0f - ease(0.8f, 1.0f, sweep)));
      }
      p.circle(feet_, 3.0f, 0.1f + 1.1f * easeOut(0.0f, 1.2f, since), kCleanEdge, kClean, 1.2f, env);
      blooms.push_back(dot(chest, 2.6f, 0.55f * env, kClean));
      rising(22, 1.0f, height_ + 1.2f, kClean, 0.1f);
      if (since > 0.0f && !motes.empty()) p.draw(kMote, p.quad, 6, glm::mat4(1.0f), kClean, kCore, 1.0f, 1.0f, 0.0f, &motes);
      motes.clear();
      {
        std::vector<VfxInstance> sick;  // dark: drawn alpha-blended, not as light
        for (uint32_t i = 0; i < 30; ++i) {
          const float born = 0.6f * hash01(seed_, i + 1200), t = since - born;
          if (t < 0.0f || t > 1.1f) continue;
          const float a = hash01(seed_, i + 1400) * kTau, speed = 1.2f + 1.6f * hash01(seed_, i + 1600);
          sick.push_back(dot(feet_ + glm::vec3(std::cos(a) * speed * t, height_ * (0.3f + 0.5f * hash01(seed_, i + 1800)) + 0.8f * t - 2.0f * t * t, std::sin(a) * speed * t),
                             0.16f + 0.12f * hash01(seed_, i + 2000), 0.9f * (1.0f - t / 1.1f), kSick, 1.0f));
        }
        if (!sick.empty()) {
          VfxDraw d;
          d.gpu.model = glm::mat4(1.0f);
          d.gpu.color = {1, 1, 0, 0}, d.gpu.params1 = {1, 1, 0, 0};
          d.gpu.firstVertex = p.quad, d.gpu.vertexCount = 6;
          d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
          for (VfxInstance& s : sick) s.c.w = 1.0f;  // the ImpactParticle material's soft dot
          frame.instances.insert(frame.instances.end(), sick.begin(), sick.end());
          d.instances = static_cast<uint32_t>(sick.size());
          d.gpu.material = static_cast<uint32_t>(VfxMaterial::ImpactParticle);
          d.gpu.sightAt = feet_;
          d.blend = VfxBlend::Alpha;
          frame.draws.push_back(d);
        }
      }
      lights.push_back({chest, kClean, 8.0f * env, 5.5f});
      break;
    }
    case Style::Bolt: {
      // Fast, vertical, heavy. The light gathers high over the hex, a thin beam marks the spot and the circle shows the area; the
      // strike: a great white beam from the sky for a blink, a flash, a ring racing out over the whole area, sparks, the scene lit
      // for a moment; then the beam thins to a line and goes, a glow stays on the ground, motes drift up.
      const glm::vec3 sky = feet_ + kUp * 16.0f;
      if (since < 0.0f) {
        const float g = ease(0.0f, 1.0f, age / lead);
        blooms.push_back(dot(feet_ + kUp * 11.0f, 1.5f + 3.5f * g, 0.9f * g, kCore));
        p.strip(kBeam, feet_, sky, 0.12f + 0.1f * g, 0.3f, kGold, kCore, 1.0f, 0.5f * g, 0.2f);
        p.circle(feet_, radius_, 0.2f + 0.85f * g, kEdge, kGold, 1.0f, 0.8f * g);
        lights.push_back({feet_ + kUp * 3.0f, kGold, 4.0f * g, radius_ + 3.0f});
      } else {
        const float strike = 1.0f - ease(0.1f, 0.5f, since);  // full for a blink, then thinning
        p.strip(kBeam, feet_, sky, 0.2f + 2.6f * strike, 0.3f + 3.4f * strike, kGold, kCore, 2.2f + 2.0f * strike, 1.0f - ease(0.9f, 1.5f, since), 0.6f);
        p.strip(kBeam, feet_, sky, 0.1f + 5.0f * strike, 6.0f * strike + 0.1f, kEdge, kGold, 1.0f, 0.5f * strike, 0.9f);
        if (since < 0.22f) blooms.push_back(dot(feet_ + kUp * 1.2f, 6.0f + 12.0f * since / 0.22f, std::pow(1.0f - since / 0.22f, 2.0f), kCore, 2.0f));
        // the ring racing out over the area, and the glow it leaves
        p.circle(feet_, radius_ + 1.4f, 0.05f + 1.25f * easeOut(0.0f, 0.4f, since), kGold, kCore, 2.4f, 1.0f - ease(0.3f, 0.9f, since));
        p.circle(feet_, radius_, 1.1f, kEdge, kGold, 1.0f, 0.9f * out);
        blooms.push_back(dot(feet_ + kUp * 0.6f, radius_ * 2.2f, 0.45f * out, kGold));
        for (const glm::vec3& h : area_) {  // the hexes round it take less: a short ray and a flash on each
          if (glm::length(glm::vec2(h.x - feet_.x, h.z - feet_.z)) < 0.5f) continue;
          const float t = since - 0.08f;
          if (t < 0.0f || t > 0.7f) continue;
          p.strip(kRay, h, h + kUp * 5.0f, 1.1f, 1.6f, kGold, kCore, 1.2f, 0.6f * (1.0f - t / 0.7f), hash01(seed_, static_cast<uint32_t>(h.x * 7.0f + h.z * 13.0f)));
          blooms.push_back(dot(h + kUp * 0.5f, 2.2f, 0.6f * (1.0f - t / 0.7f), kGold));
        }
        for (uint32_t i = 0; i < 52; ++i) {  // sparks thrown out, falling
          const float a = hash01(seed_, i + 10) * kTau, speed = 3.0f + 6.0f * hash01(seed_, i + 200), lift = 3.0f + 5.0f * hash01(seed_, i + 400);
          const float y = lift * since - 4.9f * since * since;
          if (y < 0.0f || since > 1.1f) continue;
          motes.push_back(dot(feet_ + glm::vec3(std::cos(a) * speed * since, 0.1f + y, std::sin(a) * speed * since), 0.12f + 0.1f * hash01(seed_, i + 600), 1.0f - since / 1.1f, kCore, 2.4f));
        }
        for (uint32_t i = 0; i < 24; ++i) {  // afterwards, motes drifting up out of the glow
          const float t = since - 0.3f - 0.5f * hash01(seed_, i + 800);
          if (t < 0.0f) continue;
          const float a = hash01(seed_, i + 1000) * kTau, r = radius_ * std::sqrt(hash01(seed_, i + 1200));
          motes.push_back(dot(feet_ + glm::vec3(std::cos(a) * r, 0.2f + 0.9f * t, std::sin(a) * r), 0.09f, out * ease(0.0f, 0.2f, t), kGold, 1.5f));
        }
        lights.push_back({feet_ + kUp * 2.0f, kCore, 44.0f * std::exp(-since * 9.0f) + 4.0f * out, radius_ + 8.0f});
      }
      break;
    }
  }

  if (!blooms.empty()) p.draw(kBloom, p.quad, 6, glm::mat4(1.0f), kGold, kCore, 1.0f, 1.0f, 0.0f, &blooms);
  if (!motes.empty()) p.draw(kMote, p.quad, 6, glm::mat4(1.0f), kGold, kCore, 1.0f, 1.0f, 0.0f, &motes);
}

}  // namespace ember
