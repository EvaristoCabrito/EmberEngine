#include "render/vfx/MoonwardenFx.h"

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
constexpr float kRingUnderFeet = 0.6f;
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f, kModeSlash = 5.0f;

// Moonlight: the silver, the white core, the cold edge; the dust a pale grey.
constexpr glm::vec3 kMoon{0.72f, 0.8f, 1.0f}, kCore{0.96f, 0.97f, 1.0f}, kEdge{0.45f, 0.56f, 0.95f}, kMoonDust{0.62f, 0.64f, 0.7f};
constexpr glm::vec3 kMoonLight{0.7f, 0.78f, 1.0f};

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
glm::vec3 flatDir(glm::vec3 v, glm::vec3 fallback) {
  v.y = 0.0f;
  return glm::dot(v, v) > 1e-8f ? glm::normalize(v) : fallback;
}
glm::vec3 pulled(glm::vec3 p, glm::vec3 eye, float by) {
  const glm::vec3 d = eye - p;
  const float len = glm::length(d);
  return len > by + 0.5f ? p + d * (by / len) : p;
}
float bodyHeight(float h) { return std::clamp(h > 0.0f ? h : 1.8f, 1.0f, 4.0f); }

const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, -0.5f, 0.0f}, 1.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f},
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f}, {{-0.5f, 0.5f, 0.0f}, 0.0f, {0, 0, 1}, 1.0f}};
  return g;
}
const std::vector<VfxVertex>& sphereGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::sphere(1.0f, 28, 18);
  return g;
}
const std::vector<VfxVertex>& discGeometry() {  // radius 1 on y 0 (u around, v out), facing up
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
const std::vector<VfxVertex>& columnGeometry() {  // an open cylinder of radius 1 from y 0 to 1
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 32, up = 8;
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

}  // namespace

float MoonwardenFx::Effect::end() const {
  switch (kind) {
    case Kind::GuardedCut: return start + 1.0f;
    case Kind::Bash: return start + 1.3f;
    case Kind::Thrust: return start + 0.9f;
    case Kind::Crush: return start + 2.4f;
    case Kind::Reckoning: return start + 1.8f;
    case Kind::Brace: return start + 1.6f;
    case Kind::Interpose:
    case Kind::Affront:
    case Kind::Hold: return start + 1.5f;
    case Kind::Unbreakable: return start + 2.2f;
  }
  return start + 1.0f;
}

void MoonwardenFx::strike(const std::string& kind, const Target& foe, const Target& from, float impact, bool landed) {
  Effect e;
  e.kind = kind == "wardenBash" ? Kind::Bash : kind == "gapThrust" ? Kind::Thrust : kind == "crushingBlow" ? Kind::Crush : kind == "reckoning" ? Kind::Reckoning
                                                                                                                                                  : Kind::GuardedCut;
  e.at = {foe.feet, bodyHeight(foe.height)}, e.self = {from.feet, bodyHeight(from.height)};
  e.dir = flatDir(foe.feet - from.feet, glm::vec3(0.0f, 0.0f, 1.0f));
  e.start = impact, e.landed = landed;
  e.seed = 0x6d00u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}
void MoonwardenFx::ward(const std::string& kind, const Target& self, const std::vector<Target>& marked, float at) {
  Effect e;
  e.kind = kind == "interpose" ? Kind::Interpose : kind == "affront" ? Kind::Affront : kind == "holdTheLine" ? Kind::Hold : kind == "unbreakable" ? Kind::Unbreakable
                                                                                                                                                  : Kind::Brace;
  e.self = e.at = {self.feet, bodyHeight(self.height)};
  for (const Target& t : marked) e.targets.push_back({t.feet, bodyHeight(t.height)});
  e.start = at;
  e.seed = 0x3b00u + 7919u * ++serial_;
  effects_.push_back(std::move(e));
}

bool MoonwardenFx::active(float now) const {
  return std::any_of(effects_.begin(), effects_.end(), [&](const Effect& e) { return e.end() > now; });
}

void MoonwardenFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(effects_, [&](const Effect& e) { return e.end() <= now; });
  if (effects_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  columnFirst_ = frame.addVertices(columnGeometry()), columnCount_ = static_cast<uint32_t>(columnGeometry().size());
  streaks_.clear(), puffs_.clear();
  for (const Effect& e : effects_) {
    if (now < e.start) continue;
    if (e.kind == Kind::GuardedCut || e.kind == Kind::Bash || e.kind == Kind::Thrust || e.kind == Kind::Crush || e.kind == Kind::Reckoning)
      buildStrike(e, now, camera, frame, lights);
    else
      buildWard(e, now, camera, frame, lights);
  }
  const glm::vec3 sight = effects_.front().at.feet;
  const Draw draw{frame};
  if (!puffs_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
         static_cast<uint32_t>(puffs_.size()));
  }
  if (!streaks_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(kCore, 0.0f), sight, VfxBlend::Additive,
         first, static_cast<uint32_t>(streaks_.size()));
  }
}

// --- Shared pieces ---------------------------------------------------------------------------------------------------------

void MoonwardenFx::crescent(glm::vec3 centre, glm::vec3 u, glm::vec3 v, float r, float a0, float a1, float width, float intensity, float age, float sweep,
                            glm::vec3 eye, glm::vec3 sight, VfxFrame& frame) {
  constexpr float kHold = 0.06f, kFade = 0.3f;
  const float head = easeOut(0.0f, sweep, age);
  const float tail = ease(sweep * 0.5f, sweep + kHold + kFade, age);
  const float opacity = 1.0f - ease(sweep + kHold, sweep + kHold + kFade, age);
  if (opacity <= 0.0f || head <= tail) return;
  constexpr int kSegments = 28;
  const glm::vec3 normal = glm::normalize(glm::cross(u, v));
  std::vector<VfxVertex> sheet;
  sheet.reserve(kSegments * 6);
  std::vector<glm::vec3> outer(kSegments + 1), inner(kSegments + 1);
  for (int j = 0; j <= kSegments; ++j) {
    const float s = static_cast<float>(j) / kSegments, a = a0 + (a1 - a0) * s;
    const glm::vec3 radial = std::cos(a) * u + std::sin(a) * v;
    const float w = width * std::pow(std::sin(kPi * s), 0.7f) + 0.004f;  // a crescent: thick in the middle, a point at each end
    outer[j] = pulled(centre + radial * r, eye, kPull);
    inner[j] = pulled(centre + radial * (r - w), eye, kPull);
  }
  for (int j = 0; j < kSegments; ++j) {
    const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
    const VfxVertex a0v{inner[j], s0, normal, 0.0f}, a1v{outer[j], s0, normal, 1.0f}, b0{inner[j + 1], s1, normal, 0.0f}, b1{outer[j + 1], s1, normal, 1.0f};
    sheet.insert(sheet.end(), {a0v, a1v, b0, a1v, b1, b0});
  }
  const Draw draw{frame};
  draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, age, intensity, opacity, kMoon, tail, glm::vec4(kCore, head),
       sight, VfxBlend::Additive);
}
void MoonwardenFx::ring(glm::vec3 feet, float radius, float opacity, float age, glm::vec3 eye, glm::vec3 sight, VfxFrame& frame) {
  if (opacity <= 0.0f) return;
  const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(feet + glm::vec3(0.0f, 0.04f, 0.0f), eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {radius, 1.0f, radius});
  Draw{frame}(discFirst_, discCount_, m, kModeRing, age, 0.45f, opacity, kEdge, 0.0f, glm::vec4(kCore, 0.9f), sight, VfxBlend::Additive);
}
void MoonwardenFx::shell(glm::vec3 centre, glm::vec3 scale, float opacity, float age, glm::vec3 sight, VfxFrame& frame) {
  if (opacity <= 0.0f) return;
  const glm::mat4 m = glm::translate(glm::mat4(1.0f), centre) * glm::scale(glm::mat4(1.0f), scale);
  Draw{frame}(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.5f, opacity, kMoon, 0.05f, glm::vec4(kCore, 0.0f), sight, VfxBlend::Additive);
}
void MoonwardenFx::column(glm::vec3 feet, float radius, float height, float opacity, float age, glm::vec3 eye, glm::vec3 sight, VfxFrame& frame) {
  if (opacity <= 0.0f) return;
  const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(feet, eye, kPull * 0.6f)) * glm::scale(glm::mat4(1.0f), {radius, height + 1e-3f, radius});
  Draw{frame}(columnFirst_, columnCount_, m, kModeEnergy, age, 0.5f, opacity, kEdge, 0.0f, glm::vec4(kMoon, 0.2f), sight, VfxBlend::Additive);
}
// A thread of moonlight from a to b: motes running along it toward b.
void MoonwardenFx::thread(glm::vec3 a, glm::vec3 b, float width, float alpha, float age, uint32_t seed, glm::vec3 eye) {
  if (alpha <= 0.0f) return;
  const glm::vec3 d = b - a;
  const float len = glm::length(d);
  if (len < 1e-3f) return;
  const int count = std::clamp(static_cast<int>(len * 7.0f), 6, 40);
  for (int i = 0; i < count; ++i) {
    const float s = std::fmod(static_cast<float>(i) / count + age * 1.4f + hash01(seed, static_cast<uint32_t>(i), 9) * 0.02f, 1.0f);
    const glm::vec3 p = a + d * s + kUp * (0.18f * std::sin(kPi * s));  // a low arc between the two
    streaks_.push_back(streak(pulled(p, eye, kPull), d / len * 0.18f, width, alpha * std::sin(kPi * s), kMoon, 2.0f));
  }
}

// --- His blows ---------------------------------------------------------------------------------------------------------------

void MoonwardenFx::buildStrike(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float age = now - e.start;
  const float h = e.at.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 chest = e.at.feet + kUp * (h * 0.55f), side = glm::normalize(glm::cross(kUp, e.dir));
  const glm::vec3 sight = e.at.feet;
  auto flash = [&](glm::vec3 at, float size, float dur, float strength) {
    if (!e.landed || age > dur) return;
    const float f = 1.0f - ease(0.0f, dur, age);
    streaks_.push_back(streak(pulled(at, eye, kPull), glm::vec3(0.0f), size * (0.5f + 0.5f * f), f, kCore, strength));
  };
  auto light = [&](glm::vec3 at, float power, float dur, float reach) {
    if (age < dur) lights.push_back({at, kMoonLight, power * (1.0f - ease(0.0f, dur, age)), reach * k});
  };
  auto sparks = [&](glm::vec3 at, glm::vec3 bias, int count, float speed, float life, uint32_t salt) {
    if (!e.landed) return;
    for (int i = 0; i < count; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + salt;
      const float a = hash01(e.seed, n, 1) * kTau, up = 0.3f + 0.9f * hash01(e.seed, n, 2);
      const glm::vec3 v = glm::normalize(bias + glm::vec3(std::cos(a), up, std::sin(a)) * 0.8f) * speed * (0.5f + hash01(e.seed, n, 3));
      const float t = age - 0.02f * hash01(e.seed, n, 4);
      if (t < 0.0f || t > life) continue;
      const glm::vec3 p = at + v * t + glm::vec3(0.0f, -2.5f * t * t, 0.0f);
      streaks_.push_back(streak(pulled(p, eye, kPull), v * 0.04f, 0.035f * k, 1.0f - t / life, kMoon, 2.4f));
    }
  };
  auto dust = [&](int count, float spread, float lift, uint32_t salt) {
    if (!e.landed) return;
    for (int i = 0; i < count; ++i) {
      const uint32_t n = static_cast<uint32_t>(i) + salt;
      const float life = 0.9f * (0.8f + 0.4f * hash01(e.seed, n, 2));
      if (age > life) continue;
      const float u = age / life, a = hash01(e.seed, n, 3) * kTau;
      const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
      const glm::vec3 p = e.at.feet + out * (0.25f + spread * easeOut(0.0f, 1.0f, u)) + kUp * (0.06f + lift * easeOut(0.0f, 1.0f, u));
      puffs_.push_back(puff(pulled(p, eye, kGroundPull), 0.3f + 0.45f * u, a + age, hash01(e.seed, n, 4) * 37.0f, 0.34f * (1.0f - ease(0.35f, 1.0f, u)),
                            kMoonDust));
    }
  };
  switch (e.kind) {
    case Kind::GuardedCut: {
      // the crescent across the chest, then the guard on his own shield
      crescent(chest - e.dir * 0.35f, side, e.dir, 0.95f * k, -0.9f, 0.9f, 0.32f * k, 1.6f, age, 0.16f, eye, sight, frame);
      flash(chest, 0.55f * k, 0.16f, 2.4f);
      light(chest, 9.0f, 0.4f, 3.0f);
      if (e.landed && age > 0.12f) {
        const float t = age - 0.12f;
        const glm::vec3 shield = e.self.feet + kUp * (e.self.height * 0.5f) + e.dir * 0.35f;
        shell(pulled(shield, eye, kPull), glm::vec3(0.5f, 0.62f, 0.5f) * k, 0.45f * ease(0.0f, 0.1f, t) * (1.0f - ease(0.35f, 0.85f, t)), t, e.self.feet, frame);
      }
      break;
    }
    case Kind::Bash: {
      // the shield's face: a flat shell bursting at the chest, toward the push; a ground ring; moon dust thrown ahead
      if (age < 0.45f) {
        const float r = 0.25f * k + 0.85f * k * easeOut(0.0f, 0.3f, age);
        const glm::mat4 face = glm::translate(glm::mat4(1.0f), pulled(chest - e.dir * 0.2f, eye, kPull)) *
                               glm::mat4(glm::mat3(side, kUp, e.dir)) * glm::scale(glm::mat4(1.0f), {r, r * 1.15f, r * 0.25f});
        Draw{frame}(sphereFirst_, sphereCount_, face, kModeEnergy, age, 0.6f, 0.6f * (1.0f - ease(0.1f, 0.45f, age)), kMoon, 0.05f, glm::vec4(kCore, 0.0f),
                    sight, VfxBlend::Additive);
      }
      ring(e.at.feet, 0.4f * k + 1.3f * k * easeOut(0.0f, 0.45f, age), 0.7f * (1.0f - ease(0.15f, 0.6f, age)), age, eye, sight, frame);
      flash(chest, 0.7f * k, 0.18f, 2.6f);
      sparks(chest, e.dir * 1.2f, 18, 3.2f, 0.45f, 100u);
      dust(16, 1.1f, 0.35f, 200u);
      light(chest, 12.0f, 0.5f, 3.5f);
      break;
    }
    case Kind::Thrust: {
      // a lance of light through the body along the blow, its point bursting out behind
      if (age < 0.5f) {
        const float reach = easeOut(0.0f, 0.12f, age), fade = 1.0f - ease(0.15f, 0.5f, age);
        const glm::vec3 back = chest - e.dir * 1.1f * k, front = chest + e.dir * (1.6f * k) * reach;
        for (int i = 0; i < 14; ++i) {
          const float s = static_cast<float>(i) / 13.0f;
          const glm::vec3 p = glm::mix(back, front, s);
          streaks_.push_back(streak(pulled(p, eye, kPull), e.dir * 0.22f, (0.06f + 0.05f * s) * k, fade * (0.4f + 0.6f * s), s > 0.8f ? kCore : kMoon, 2.6f));
        }
      }
      flash(chest + e.dir * 0.3f, 0.45f * k, 0.14f, 2.6f);
      sparks(chest + e.dir * 0.25f, e.dir * 1.5f, 14, 3.6f, 0.4f, 300u);
      light(chest, 8.0f, 0.35f, 2.6f);
      break;
    }
    case Kind::Crush: {
      // a heavy crescent brought down, the ground ring, dust; then a dim ring settles on the foe (its next blow weighed down)
      crescent(chest + kUp * 0.1f, e.dir, kUp, 1.05f * k, 2.1f, 0.4f, 0.4f * k, 1.9f, age, 0.2f, eye, sight, frame);
      ring(e.at.feet, 0.5f * k + 1.5f * k * easeOut(0.15f, 0.6f, age), 0.65f * ease(0.12f, 0.18f, age) * (1.0f - ease(0.35f, 0.8f, age)), age, eye, sight, frame);
      if (age > 0.15f) flash(chest - kUp * 0.3f, 0.8f * k, 0.32f, 2.8f);
      if (age > 0.15f) dust(22, 1.4f, 0.5f, 400u);
      if (e.landed && age > 0.4f) {  // the weight: a slow dim ring sinking over the shoulders
        const float t = age - 0.4f;
        const float o = 0.4f * ease(0.0f, 0.25f, t) * (1.0f - ease(1.3f, 2.0f, t));
        const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(e.at.feet + kUp * (h * (0.8f - 0.25f * easeOut(0.0f, 1.6f, t))), eye, kPull)) *
                            glm::scale(glm::mat4(1.0f), {0.55f * k, 1.0f, 0.55f * k});
        Draw{frame}(discFirst_, discCount_, m, kModeRing, t, 0.35f, o, kEdge * 0.7f, 0.0f, glm::vec4(kMoon, 0.85f), sight, VfxBlend::Additive);
      }
      light(chest, 14.0f, 0.6f, 3.6f);
      break;
    }
    case Kind::Reckoning: {
      // a full moon flares behind the foe, the great crescent across it, a column of light
      if (age < 1.1f) {
        const glm::vec3 moon = chest + kUp * 0.35f - e.dir * 0.55f;
        const float s = (0.5f + 0.5f * easeOut(0.0f, 0.25f, age)) * 1.05f * k;
        shell(pulled(moon, eye, kPull * 0.5f), glm::vec3(s), 0.55f * ease(0.0f, 0.08f, age) * (1.0f - ease(0.5f, 1.1f, age)), age, sight, frame);
      }
      crescent(chest, side, kUp, 1.3f * k, -1.2f, 1.2f, 0.5f * k, 2.2f, age - 0.05f, 0.18f, eye, sight, frame);
      column(e.at.feet, 0.38f * k, h * 1.4f * easeOut(0.05f, 0.35f, age), 0.55f * ease(0.05f, 0.15f, age) * (1.0f - ease(0.7f, 1.4f, age)), age, eye, sight, frame);
      flash(chest, 1.0f * k, 0.24f, 3.0f);
      sparks(chest, kUp * 0.6f, 24, 3.4f, 0.6f, 500u);
      light(chest, 20.0f, 0.8f, 4.5f);
      break;
    }
    default: break;
  }
}

// --- His guards --------------------------------------------------------------------------------------------------------------

void MoonwardenFx::buildWard(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const glm::vec3 eye = camera.position;
  const float age = now - e.start;
  const float h = e.self.height, k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 chest = e.self.feet + kUp * (h * 0.55f);
  const glm::vec3 sight = e.self.feet;
  const float env = ease(0.0f, 0.15f, age) * (1.0f - ease(e.end() - e.start - 0.6f, e.end() - e.start, age));
  if (age < 0.6f) lights.push_back({chest, kMoonLight, 8.0f * (1.0f - ease(0.0f, 0.6f, age)), 3.0f * k});
  auto targetChest = [](const Target& t) { return t.feet + kUp * (t.height * 0.55f); };
  switch (e.kind) {
    case Kind::Brace: {  // a shell of moonlight round him, a crescent ring at his feet
      shell(pulled(chest, eye, kPull * 0.5f), glm::vec3(0.75f, 0.95f, 0.75f) * k * (0.85f + 0.15f * easeOut(0.0f, 0.2f, age)), 0.4f * env, age, sight, frame);
      ring(e.self.feet, 0.95f * k, 0.6f * env, age, eye, sight, frame);
      break;
    }
    case Kind::Interpose:
    case Kind::Hold: {  // a thread to each ally, a ring at its feet
      for (size_t i = 0; i < e.targets.size(); ++i) {
        const Target& t = e.targets[i];
        thread(chest, targetChest(t), 0.05f * k, 0.8f * env, age, e.seed + static_cast<uint32_t>(i) * 131u, eye);
        ring(t.feet, (e.kind == Kind::Hold ? 0.8f : 0.7f) * std::clamp(t.height / 2.0f, 0.7f, 1.4f), 0.55f * env, age, eye, t.feet, frame);
      }
      ring(e.self.feet, 0.75f * k, 0.4f * env, age, eye, sight, frame);
      break;
    }
    case Kind::Affront: {  // a thread to the foe, a crescent sigil under it
      for (size_t i = 0; i < e.targets.size(); ++i) {
        const Target& t = e.targets[i];
        thread(chest, targetChest(t), 0.06f * k, 0.9f * env, age, e.seed + static_cast<uint32_t>(i) * 131u, eye);
        const glm::vec3 g = t.feet + kUp * 0.06f;
        crescent(g, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.85f, -2.2f, 2.2f, 0.18f, 1.2f, std::min(age, 0.25f), 0.25f, eye, t.feet, frame);
        ring(t.feet, 0.95f, 0.35f * env, age, eye, t.feet, frame);
      }
      break;
    }
    case Kind::Unbreakable: {  // a barrier sphere round him (and the ally), a column rising through it
      std::vector<Target> all{e.self};
      all.insert(all.end(), e.targets.begin(), e.targets.end());
      for (size_t i = 0; i < all.size(); ++i) {
        const Target& t = all[i];
        const float kt = std::clamp(t.height / 2.0f, 0.7f, 1.4f);
        const glm::vec3 c = targetChest(t);
        shell(pulled(c, eye, kPull * 0.5f), glm::vec3(0.85f, 1.1f, 0.85f) * kt, (i == 0 ? 0.5f : 0.38f) * env, age, t.feet, frame);
        column(t.feet, 0.32f * kt, t.height * 1.3f * easeOut(0.0f, 0.4f, age), 0.45f * env, age, eye, t.feet, frame);
        for (int j = 0; j < 10; ++j) {  // motes rising inside
          const uint32_t n = static_cast<uint32_t>(j) + 40u * static_cast<uint32_t>(i);
          const float phase = std::fmod(hash01(e.seed, n, 1) + age * 0.8f, 1.0f), ang = hash01(e.seed, n, 2) * kTau + age * 1.6f;
          const glm::vec3 p = t.feet + glm::vec3(std::cos(ang) * 0.3f * kt, phase * t.height * 1.1f, std::sin(ang) * 0.3f * kt);
          streaks_.push_back(streak(pulled(p, eye, kPull), glm::vec3(0.0f), 0.05f * kt, std::sin(kPi * phase) * env, kCore, 2.2f));
        }
      }
      ring(e.self.feet, 1.0f * k, 0.5f * env, age, eye, sight, frame);
      break;
    }
    default: break;
  }
}

}  // namespace ember
