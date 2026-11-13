#include "render/vfx/PoisonFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/IceParts.h"

namespace ember {

using namespace icefx;  // ease, hash01, quadGeometry

namespace {

constexpr float kPi = std::numbers::pi_v<float>, kTau = 2.0f * kPi;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
// The gas's body is a desaturated olive; the saturated acid green is its glow only. Peste Negra's is near black; Névoa Ácida's yellow.
constexpr glm::vec3 kOlive{0.46f, 0.42f, 0.13f}, kAcid{0.42f, 1.0f, 0.22f}, kBlack{0.13f, 0.15f, 0.11f}, kYellow{0.62f, 0.54f, 0.12f};
constexpr glm::vec3 kPoisonLight{0.45f, 1.0f, 0.42f};
constexpr float kExit = 0.9f;  // the gas thins out over this long
// GasVolume's bodies (shaders/vfx.hlsl gasDensity).
constexpr float kPool = 0.0f, kPuff = 1.0f, kCone = 2.0f, kStream = 3.0f;

float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

// One volume of gas.
void gas(VfxFrame& frame, uint32_t quad, float time, float body, glm::vec3 origin, float radius, float depth, glm::vec3 along, float thick, float fade, float glow,
         glm::vec3 tint, glm::vec3 shine, float seed) {
  if (fade >= 0.995f || radius <= 0.02f || thick <= 0.0f) return;
  VfxDraw d;
  d.gpu.model = glm::mat4(0.0f);
  d.gpu.model[0] = glm::vec4(along, 0.0f), d.gpu.model[1] = glm::vec4(tint, 0.0f), d.gpu.model[2] = glm::vec4(shine, 0.0f);
  d.gpu.params0 = {time, thick, depth, radius};
  d.gpu.params1 = {origin, seed};
  d.gpu.color = {body, std::max(fade, 0.0f), glow, 0.0f};
  d.gpu.firstVertex = quad, d.gpu.vertexCount = 6, d.gpu.firstInstance = 0xFFFFFFFFu;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::GasVolume);
  d.gpu.sightAt = origin;
  d.blend = VfxBlend::Alpha;
  d.depthTest = false;  // the shader stops its ray at the scene's depth itself
  frame.draws.push_back(d);
}

// A droplet or a speck (the ImpactParticle material's soft dot, tinted): a = centre + size, b = turn, frame, alpha, intensity, c = tint, kind.
VfxInstance speck(glm::vec3 at, float size, float alpha, glm::vec3 tint, float intensity) {
  VfxInstance inst;
  inst.a = glm::vec4(at, size);
  inst.b = {0.0f, 0.0f, alpha, intensity};
  inst.c = glm::vec4(tint, 1.0f);
  return inst;
}

}  // namespace

PoisonSpell::PoisonSpell(Style style, glm::vec3 casterFeet, float casterHeight, std::vector<glm::vec3> hexes, float start, float impact, uint32_t seed)
    : style_(style),
      feet_(casterFeet),
      hand_(casterFeet + glm::vec3(0.0f, casterHeight * 0.66f, 0.0f)),
      start_(start),
      impact_(std::max(impact, start + 0.25f)),
      hexes_(std::move(hexes)),
      seed_(seed) {
  auto flat = [](glm::vec3 a, glm::vec3 b) { return glm::length(glm::vec2(a.x - b.x, a.z - b.z)); };
  centre_ = feet_;
  if (!hexes_.empty()) {
    centre_ = glm::vec3(0.0f);
    for (const glm::vec3& h : hexes_) centre_ += h;
    centre_ /= static_cast<float>(hexes_.size());
    for (const glm::vec3& h : hexes_) radius_ = std::max(radius_, flat(h, centre_) + 1.0f), reach_ = std::max(reach_, flat(h, feet_));
    const glm::vec3 aim = style_ == Style::Breath ? centre_ : hexes_.front();
    glm::vec2 d{aim.x - feet_.x, aim.z - feet_.z};
    if (glm::dot(d, d) > 1e-6f) {
      d = glm::normalize(d);
      direction_ = {d.x, 0.0f, d.y};
    }
  }
  if (style_ == Style::Cloud) {
    impact_ = start_;
    end_ = 1.0e9f;
    return;
  }
  linger_ = style_ == Style::Minor ? 1.7f : style_ == Style::Caustic ? 2.7f : style_ == Style::Plague ? 3.0f : style_ == Style::Pestilence ? 3.4f : 2.3f;
  end_ = impact_ + linger_ + kExit + 0.1f;
}

void PoisonSpell::stop(float now) {
  if (stopped_) return;
  stopped_ = true;
  end_ = std::min(end_, now + kExit + 0.3f);
}

void PoisonSpell::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  (void)camera;
  if (now < start_ || done(now)) return;
  const uint32_t quad = frame.addVertices(quadGeometry());
  const float gather = impact_ - start_, age = now - start_, since = now - impact_;
  const float out = ease(linger_, linger_ + kExit, since);  // 0 lying .. 1 thinned out
  const glm::vec3 side = glm::normalize(glm::cross(direction_, kUp));
  const float s0 = hash01(seed_, 1) * 40.0f;
  std::vector<VfxInstance> specks;

  switch (style_) {
    case Style::Breath: {
      // The breath: a cone of gas out of the caster's mouth, rolling out to the cone's far edge, as wide there as the cone is; it
      // slows, loses its shape and sinks into two pools lying along the cone.
      const float far = reach_ + 1.0f;
      if (since > -0.05f) {
        const float grown = easeOut(0.0f, 0.5f, since);
        gas(frame, quad, age, kCone, hand_ + kUp * 0.3f, far * 0.5f, 0.0f, glm::normalize(direction_ + kUp * 0.1f) * (far * (0.15f + 0.85f * grown)), 2.6f, ease(0.8f, 1.7f, since), 1.0f, kOlive, kAcid, s0);
        const float settle = ease(0.7f, 1.6f, since);
        gas(frame, quad, age, kPool, feet_ + direction_ * (far * 0.34f), far * 0.4f * (0.6f + 0.4f * settle), 1.0f * settle, {}, 1.3f, std::max(out, 1.0f - ease(0.6f, 1.2f, since)), 0.6f, kOlive, kAcid, s0 + 3.0f);
        gas(frame, quad, age, kPool, feet_ + direction_ * (far * 0.74f), far * 0.56f * (0.6f + 0.4f * settle), 1.15f * settle, {}, 1.3f, std::max(out, 1.0f - ease(0.7f, 1.4f, since)), 0.6f,
            kOlive, kAcid, s0 + 7.0f);
        for (uint32_t i = 0; i < 44; ++i) {
          // spittle: flung out through the cone, falling short
          const float turn = (hash01(seed_, i + 10) * 2.0f - 1.0f) * 0.6f, speed = 5.0f + 6.0f * hash01(seed_, i + 60), t = since - 0.25f * hash01(seed_, i + 110);
          if (t < 0.0f || t > 1.2f) continue;
          const glm::vec3 dir = direction_ * std::cos(turn) + side * std::sin(turn);
          const float y = hand_.y - feet_.y + (0.6f + 1.4f * hash01(seed_, i + 160)) * t - 4.9f * t * t;
          if (y < 0.03f) continue;
          specks.push_back(speck(feet_ + dir * std::min(speed * t, far) + glm::vec3(0, y, 0), 0.1f + 0.1f * hash01(seed_, i + 210), 1.0f - t / 1.2f, kAcid, 1.6f));
        }
        lights.push_back({hand_ + direction_ * (far * 0.4f * easeOut(0.0f, 0.5f, since)), kPoisonLight, 6.0f * (1.0f - ease(0.4f, 1.6f, since)) + 1.6f * (1.0f - out), far * 0.8f});
      } else {
        // it gathers in the mouth
        const float rise = ease(0.0f, 1.0f, age / gather);
        gas(frame, quad, age, kPuff, hand_, 0.45f * rise, 0.0f, {}, 2.0f, 0.0f, 1.2f, kOlive, kAcid, s0);
        lights.push_back({hand_, kPoisonLight, 3.0f * rise, 3.0f});
      }
      break;
    }
    case Style::Minor:
    case Style::Caustic: {
      // The glob: thrown from the hand in an arc to the aimed hex, gas trailing off it. The Cáustico's is half as big again, and
      // boils as it flies.
      const bool strong = style_ == Style::Caustic;
      const glm::vec3 aim = hexes_.empty() ? feet_ + direction_ * 3.0f : hexes_.front();
      const float size = strong ? 1.0f : 0.62f;
      if (since < 0.0f) {
        const float p = std::clamp(age / gather, 0.0f, 1.0f), fly = ease(0.18f, 1.0f, p);
        const glm::vec3 from = hand_ + kUp * 0.35f;
        const glm::vec3 at = glm::mix(from, aim + kUp * 0.3f, fly) + kUp * (std::sin(kPi * fly) * std::max(1.4f, glm::length(aim - from) * 0.22f));
        const glm::vec3 before = glm::mix(from, aim + kUp * 0.3f, std::max(fly - 0.12f, 0.0f)) + kUp * (std::sin(kPi * std::max(fly - 0.12f, 0.0f)) * std::max(1.4f, glm::length(aim - from) * 0.22f));
        gas(frame, quad, age, kPuff, at - kUp * (0.2f * size), 0.55f * size * ease(0.0f, 0.2f, p), 0.0f, {}, 3.2f, 0.0f, 1.5f, kOlive, kAcid, s0);
        if (glm::length(before - at) > 0.05f) gas(frame, quad, age, kStream, at, 0.34f * size, 0.0f, (before - at) * 2.2f, 1.8f, 0.25f, 0.9f, kOlive, kAcid, s0 + 2.0f);
        for (uint32_t i = 0; i < (strong ? 9u : 5u); ++i) {  // drips falling off it
          const float t = std::fmod(age * 1.6f + hash01(seed_, i + 10), 0.6f);
          const glm::vec3 drop = glm::mix(from, at, 1.0f - 0.25f * hash01(seed_, i + 20)) - kUp * (4.9f * t * t);
          if (drop.y > aim.y) specks.push_back(speck(drop, 0.1f * size, 0.9f * (1.0f - t / 0.6f), kAcid, 1.8f));
        }
        lights.push_back({at, kPoisonLight, (strong ? 7.0f : 4.0f) * ease(0.0f, 0.3f, p), 3.5f});
      } else {
        const float area = radius_ + (strong ? 0.7f : 0.2f);
        if (strong) {
          // a geyser of gas up out of the hex, falling back on itself
          const float up = 4.4f * easeOut(0.0f, 0.22f, since) * (1.0f - ease(0.45f, 1.2f, since));
          gas(frame, quad, age, kCone, aim, 1.5f, 0.0f, kUp * std::max(up, 0.05f), 2.2f, ease(0.5f, 1.3f, since), 1.3f, kOlive, kAcid, s0 + 1.0f);
        }
        // the burst: it mushrooms up off the ground, then falls back
        const float lift = (strong ? 0.9f : 0.6f) * std::sin(kPi * std::min(since / 0.9f, 1.0f));
        gas(frame, quad, age, kPuff, aim + kUp * lift, (strong ? 2.0f : 1.35f) * (0.35f + 0.65f * easeOut(0.0f, 0.4f, since)), 0.0f, {}, strong ? 2.0f : 1.5f,
            std::max(ease(0.7f, 1.8f, since), out), strong ? 1.2f : 0.8f, kOlive, kAcid, s0 + 4.0f);
        // the pool it leaves over the whole area: deep and rolling for the Cáustico, a thin skin for the Menor
        const float spread = easeOut(0.05f, 0.9f, since);
        gas(frame, quad, age, kPool, centre_, area * (0.25f + 0.75f * spread), (strong ? 1.35f : 0.6f) * ease(0.1f, 0.8f, since), {}, strong ? 1.7f : 0.9f, out, strong ? 0.9f : 0.5f, kOlive,
            kAcid, s0 + 8.0f);
        for (uint32_t i = 0; i < (strong ? 46u : 16u); ++i) {  // acid thrown up and out
          const float a = hash01(seed_, i + 40) * kTau, outv = (0.8f + 2.6f * hash01(seed_, i + 90)) * (strong ? 1.2f : 0.7f), lift0 = (strong ? 5.5f : 3.0f) * (0.5f + 0.5f * hash01(seed_, i + 140));
          const float y = lift0 * since - 4.9f * since * since;
          if (y < 0.0f) continue;
          specks.push_back(speck(aim + glm::vec3(std::cos(a) * outv * since, y + 0.1f, std::sin(a) * outv * since), (0.1f + 0.12f * hash01(seed_, i + 190)) * (strong ? 1.3f : 1.0f), 1.0f, kAcid, 2.0f));
        }
        lights.push_back({aim + kUp * 0.9f, kPoisonLight, (strong ? 14.0f : 7.0f) * std::exp(-since * 5.0f) + (strong ? 2.6f : 1.4f) * (1.0f - out), area + 2.5f});
      }
      break;
    }
    case Style::Plague: {
      // The plague crawls: a rope of gas over the ground from the caster to the foe, reaching it as the cast lands; there it winds
      // up round the body in two coils and lies thick at its feet; spores rise off it.
      const glm::vec3 foe = hexes_.empty() ? feet_ + direction_ * 3.0f : hexes_.front();
      const glm::vec3 from = feet_ + kUp * 0.25f, to = foe + kUp * 0.25f;
      const float crawl = ease(0.0f, 1.0f, std::min(age / gather, 1.0f));
      gas(frame, quad, age, kStream, from, 0.5f, 0.0f, (to - from) * std::max(crawl, 0.03f), 2.0f, since > 0.0f ? ease(0.4f, 1.6f, since) : 0.0f, 1.0f, kOlive, kAcid, s0);
      if (since > 0.0f) {
        const float wound = easeOut(0.0f, 0.6f, since);
        for (int k = 0; k < 2; ++k) {
          const float a = age * 1.3f + static_cast<float>(k) * kPi;
          const glm::vec3 base = foe + glm::vec3(std::cos(a) * 0.5f, 0.1f, std::sin(a) * 0.5f);
          const glm::vec3 lean = glm::vec3(std::cos(a + 1.7f) * 0.5f, 2.9f * wound, std::sin(a + 1.7f) * 0.5f);
          gas(frame, quad, age, kStream, base, 0.42f, 0.0f, lean, 2.0f, out, 1.1f, kOlive, kAcid, s0 + 3.0f + static_cast<float>(k));
        }
        gas(frame, quad, age, kPool, foe, 1.5f * (0.4f + 0.6f * wound), 1.1f * wound, {}, 1.6f, out, 0.8f, kOlive, kAcid, s0 + 9.0f);
        for (uint32_t i = 0; i < 26; ++i) {  // spores, drifting up slowly round the body
          const float life = 1.6f + hash01(seed_, i + 10), p = std::fmod(since / life + hash01(seed_, i + 60), 1.0f);
          const float a = hash01(seed_, i + 110) * kTau + since * 0.6f, r = 0.3f + 0.7f * hash01(seed_, i + 160);
          specks.push_back(speck(foe + glm::vec3(std::cos(a) * r, 0.2f + 2.6f * p, std::sin(a) * r), 0.07f + 0.06f * hash01(seed_, i + 210),
                                 (1.0f - out) * ease(0.0f, 0.15f, p) * (1.0f - ease(0.6f, 1.0f, p)), kAcid, 1.6f));
        }
        lights.push_back({foe + kUp * 1.0f, kPoisonLight, 8.0f * std::exp(-since * 5.0f) + 2.2f * (1.0f - out), 4.0f});
      } else {
        lights.push_back({from + (to - from) * crawl, kPoisonLight, 3.0f, 3.0f});
      }
      break;
    }
    case Style::Pestilence: {
      // Black smoke seeps up out of the ground all over the area while the cast gathers (a column on each hex, each in its turn);
      // then the whole of it comes down as one dome, near black, green in its veins, over everything, and lies there.
      const float area = radius_ + 1.0f;
      const size_t columns = std::min<size_t>(hexes_.size(), 9);
      for (size_t i = 0; i < columns; ++i) {
        const glm::vec3 hex = hexes_[i * hexes_.size() / columns];
        const float rise = ease(0.0f, 1.0f, (age - 0.08f * static_cast<float>(i)) / std::max(gather * 0.8f, 0.25f));
        if (rise <= 0.0f) continue;
        gas(frame, quad, age, kCone, hex, 0.8f, 0.0f, kUp * (3.6f * rise) + side * (0.5f * std::sin(age + static_cast<float>(i))), 2.6f, since > 0.0f ? ease(0.1f, 0.9f, since) : 0.0f, 0.4f,
            kBlack, kAcid, s0 + static_cast<float>(i));
      }
      if (since > 0.0f) {
        const float fall = easeOut(0.0f, 0.6f, since);
        gas(frame, quad, age, kPuff, centre_ + kUp * (2.2f * (1.0f - fall)), area * (0.5f + 0.5f * fall), 0.0f, {}, 2.2f, out, 0.9f, kBlack, kAcid, s0 + 20.0f);
        gas(frame, quad, age, kPool, centre_, (area + 0.8f) * (0.4f + 0.6f * fall), 1.0f * ease(0.2f, 0.9f, since), {}, 1.9f, out, 0.8f, kBlack, kAcid, s0 + 25.0f);
        for (uint32_t i = 0; i < 54; ++i) {  // flies over it
          const float a = hash01(seed_, i + 10) * kTau + since * (1.2f + 2.0f * hash01(seed_, i + 60)), r = area * (0.2f + 0.8f * hash01(seed_, i + 110));
          specks.push_back(speck(centre_ + glm::vec3(std::cos(a) * r, 0.5f + 2.4f * hash01(seed_, i + 160) + 0.3f * std::sin(since * 5.0f + static_cast<float>(i)), std::sin(a) * r),
                                 0.06f + 0.05f * hash01(seed_, i + 210), 0.9f * (1.0f - out) * ease(0.0f, 0.4f, since), glm::vec3(0.5f, 0.9f, 0.3f), 1.2f));
        }
        lights.push_back({centre_ + kUp * 1.2f, kPoisonLight, 12.0f * std::exp(-since * 4.0f) + 2.4f * (1.0f - out), area + 3.0f});
      }
      break;
    }
    case Style::Cloud: {
      // The field: poured out from its middle over the whole of it, then a deep blanket turning slowly, two thicker banks drifting
      // through it; it bubbles. It stands until the rules drop it, then thins out.
      const float gone = stopped_ ? std::clamp(1.0f - (end_ - 0.3f - now) / kExit, 0.0f, 1.0f) : 0.0f;
      const float poured = easeOut(0.0f, 1.2f, age);
      gas(frame, quad, age, kPool, centre_, (radius_ + 0.6f) * (0.2f + 0.8f * poured), 1.3f * ease(0.0f, 0.9f, age), {}, 1.4f, gone, 0.6f, kYellow, kAcid, s0);
      for (int k = 0; k < 2; ++k) {
        const float a = age * 0.16f + static_cast<float>(k) * kPi + s0;
        gas(frame, quad, age, kPool, centre_ + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * (radius_ * 0.4f), radius_ * 0.6f * poured, 1.7f * poured, {}, 1.3f, gone, 0.7f, kYellow, kAcid,
            s0 + 5.0f + static_cast<float>(k));
      }
      if (age < 1.0f) gas(frame, quad, age, kPuff, centre_ + kUp * 0.4f, 1.6f * (0.4f + 0.6f * easeOut(0.0f, 0.5f, age)), 0.0f, {}, 2.0f, ease(0.3f, 1.0f, age), 1.2f, kYellow, kAcid, s0 + 11.0f);
      for (uint32_t i = 0; i < 30; ++i) {  // bubbles breaking on it
        const float life = 0.9f + 0.8f * hash01(seed_, i + 10), p = std::fmod(age / life + hash01(seed_, i + 60), 1.0f);
        const float a = hash01(seed_, i + 110) * kTau, r = radius_ * std::sqrt(hash01(seed_, i + 160));
        specks.push_back(speck(centre_ + glm::vec3(std::cos(a) * r, 0.5f + 1.1f * p, std::sin(a) * r), 0.08f + 0.08f * hash01(seed_, i + 210),
                               (1.0f - gone) * poured * ease(0.0f, 0.2f, p) * (1.0f - ease(0.7f, 1.0f, p)), kAcid, 1.5f));
      }
      lights.push_back({centre_ + kUp * 0.9f, kPoisonLight, (2.2f + 6.0f * std::exp(-age * 3.0f)) * (1.0f - gone), radius_ + 2.5f});
      break;
    }
  }

  // The droplets and specks, over the gas.
  if (!specks.empty()) {
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.color = {1, 1, 0, 0};
    d.gpu.params1 = {1, 1, 0, 0};
    d.gpu.firstVertex = quad, d.gpu.vertexCount = 6;
    d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), specks.begin(), specks.end());
    d.instances = static_cast<uint32_t>(specks.size());
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::ImpactParticle);
    d.gpu.sightAt = centre_;
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
}

}  // namespace ember
