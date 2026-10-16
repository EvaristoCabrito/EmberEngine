#include "render/vfx/MagicMissileV2.h"

#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kLineWidth = 0.012f;  // a three.js Line (1 px) as a ribbon this wide (world units)

const std::vector<VfxVertex>& coreGeometry() { static const auto g = vfxgeo::icosahedron(1.0f, 3); return g; }
const std::vector<VfxVertex>& shellGeometry() { static const auto g = vfxgeo::icosahedron(1.0f, 2); return g; }
const std::vector<VfxVertex>& burstGeometry() { static const auto g = vfxgeo::torus(0.7f, 0.08f, 8, 32, 2.0f * kPi); return g; }
const std::vector<VfxVertex>& fractureGeometry() { static const auto g = vfxgeo::torus(0.74f, 0.035f, 5, 28, kPi * 0.72f); return g; }
const std::vector<VfxVertex>& shardGeometry() { static const auto g = vfxgeo::octahedron(1.0f, 0); return g; }

// A world-space polyline as a camera-facing ribbon (three.js draws Line at 1 px).
std::vector<VfxVertex> ribbon(const std::vector<glm::vec3>& points, glm::vec3 eye, float halfWidth) {
  std::vector<VfxVertex> out;
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    const glm::vec3 a = points[i], b = points[i + 1];
    const glm::vec3 side = three::normalize(glm::cross(b - a, eye - (a + b) * 0.5f)) * halfWidth;
    const VfxVertex v0{a - side, 0, {0, 0, 1}, 0}, v1{a + side, 0, {0, 0, 1}, 1}, v2{b - side, 1, {0, 0, 1}, 0}, v3{b + side, 1, {0, 0, 1}, 1};
    out.insert(out.end(), {v0, v1, v2, v1, v3, v2});
  }
  return out;
}

}  // namespace

float MagicMissileV2::impactDelay(glm::vec3 origin, glm::vec3 target) {
  const Settings s;
  return kLaunchAt + three::clamp(glm::length(target - origin) / std::max(1.0f, s.projectileSpeed), 0.8f, 1.3f);
}

MagicMissileV2::MagicMissileV2(glm::vec3 origin, glm::vec3 target, float start)
    : origin_(vfxToEmber(origin)), target_(vfxToEmber(target)), start_(start) {
  // castSpell: the seed's first number places the bolt's wobble; the flight from the distance.
  rng_.state = s_.seed;
  seed_ = rng_.next() * 100.0f;
  flight_ = three::clamp(glm::length(target_ - origin_) / std::max(1.0f, s_.projectileSpeed), 0.8f, 1.3f);
  impactAt_ = kLaunchAt + flight_;
  // The path (update(): fixed once launched): start, two control points bowed sideways and up, the target.
  const glm::vec3 start3 = formation(kLaunchAt);
  const glm::vec3 forward = three::normalize(target_ - start3), up{0, 0, 1};
  glm::vec3 side = three::normalize(glm::cross(forward, up));
  if (glm::dot(side, side) < 0.01f) side = {1, 0, 0};
  const glm::vec3 c1 = glm::mix(start3, target_, 0.31f) + side * s_.trajectoryCurvature + up * s_.trajectoryHeight;
  const glm::vec3 c2 = glm::mix(start3, target_, 0.72f) + side * (-s_.trajectoryCurvature * 0.32f) + up * (s_.trajectoryHeight * 0.4f);
  curve_.points = {start3, c1, c2, target_};
}

glm::vec3 MagicMissileV2::formation(float t) const {
  return origin_ + glm::vec3(std::sin(t * 8.0f) * 0.025f, 0.46f, 0.16f);
}

glm::vec3 MagicMissileV2::flightPoint(float t, glm::vec3* velocity) const {
  const float progress = three::clamp((t - kLaunchAt) / std::max(0.18f, flight_), 0.0f, 1.0f);
  const float accelerated = std::pow(progress, std::max(0.45f, s_.acceleration));
  const float eased = three::lerp(progress, accelerated, s_.seekingStrength);
  const glm::vec3 wiggle{std::sin(t * 8.0f + seed_) * s_.turbulence, std::cos(t * 7.0f + seed_ * 0.4f) * s_.turbulence * 0.6f,
                         std::sin(t * 9.0f + seed_ * 0.8f) * s_.turbulence};
  const glm::vec3 pos = curve_.getPoint(eased) + wiggle * (1.0f - progress * 0.85f);
  if (velocity) *velocity = three::normalize(curve_.getPoint(std::min(1.0f, eased + 0.025f)) - pos);
  return pos;
}

void MagicMissileV2::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  const glm::mat4 toWorld = vfxEmberToWorld();
  const float buildup = three::smoothstep(t, 0.0f, 0.75f);
  const bool launched = t >= kLaunchAt, hit = t >= impactAt_;
  const float impactT = hit ? t - impactAt_ : 0.0f;
  glm::vec3 velocity{0.0f}, pos = formation(t);
  float progress = 0.0f;
  if (launched) {
    progress = three::clamp((t - kLaunchAt) / std::max(0.18f, flight_), 0.0f, 1.0f);
    pos = flightPoint(t, &velocity);
  }
  const glm::vec3 worldPos = vfxFromEmber(pos), worldTarget = vfxFromEmber(target_);

  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  glm::vec3 sight, VfxBlend blend = VfxBlend::Additive, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = p0;
    d.gpu.params1 = p1;
    d.gpu.color = color;
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(material);
    d.gpu.sightAt = sight;
    d.instances = instances;
    d.blend = blend;
    d.depthTest = false;  // Engine2: a foreground effect (depthTest false on every part)
    frame.draws.push_back(d);
  };

  // --- The bolt: core and shell (update(): formation growth, the hit's collapse, the approach's flare) ---
  const float formationGrowth = launched ? 1.0f : 0.12f + buildup * 0.88f;
  const float shellScale = s_.missileScale * formationGrowth * (hit ? std::max(0.02f, 1.0f - impactT * 2.2f) : 1.0f + progress * 0.2f);
  glm::quat q(1, 0, 0, 0);
  if (glm::dot(velocity, velocity) > 0.001f) q = three::fromUnitVectors({0, 1, 0}, velocity);
  else if (!launched) q = three::fromUnitVectors({0, 1, 0}, {0, 0, 1});
  const glm::mat4 bolt = toWorld * glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(q);
  const float approach = progress > 0.74f ? 1.0f + (progress - 0.74f) * 1.6f : 1.0f;

  // --- The trail (createTrail / updateTrail): the last 34 places of the bolt (one per 60 Hz frame, as Engine2's
  // history), the tip on the bolt, widening toward it ---
  const float trailFade = hit ? std::max(0.0f, 1.0f - impactT / 0.68f) : 1.0f;
  if (launched && trailFade > 0.01f) {
    constexpr int kCount = 34;
    const int active = std::clamp(static_cast<int>(std::lround(s_.trailLength * 95.0f)), 8, kCount);
    const float last = hit ? impactAt_ - 1e-4f : t;
    std::vector<glm::vec3> history;
    for (int k = active - 1; k >= 0; --k) {
      const float when = last - static_cast<float>(k) / 60.0f;
      if (when >= kLaunchAt) history.push_back(flightPoint(when, nullptr));
    }
    if (history.size() > 2) {
      std::vector<glm::vec3> points = history;
      while (points.size() < kCount) points.insert(points.begin(), points.front());
      points.back() = pos;
      std::vector<VfxVertex> strip(kCount * 2);
      for (int i = 0; i < kCount; ++i) {
        const glm::vec3 p = points[i];
        const glm::vec3 tangent = i < kCount - 1 ? three::normalize(points[i + 1] - p) : three::normalize(velocity);
        glm::vec3 side = three::normalize(glm::cross(tangent, glm::vec3(0, 0, 1)));
        if (glm::dot(side, side) < 0.01f) side = {1, 0, 0};
        const float u = static_cast<float>(i) / (kCount - 1);
        const float width = s_.trailThickness * (0.12f + u * u) * (1.0f - s_.trailFragmentation * (i % 4 == 0 ? 0.56f : 0.0f));
        strip[i * 2] = {p + side * width, u, {0, 0, 1}, 0.0f};
        strip[i * 2 + 1] = {p - side * width, u, {0, 0, 1}, 1.0f};
      }
      std::vector<VfxVertex> tris;
      for (int i = 0; i < kCount - 1; ++i) {
        const int k = i * 2;
        tris.insert(tris.end(), {strip[k], strip[k + 1], strip[k + 2], strip[k + 1], strip[k + 3], strip[k + 2]});
      }
      draw(VfxMaterial::MissileTrail, frame.addVertices(tris), tris.size(), toWorld, {t, s_.emissive * 0.24f * trailFade, s_.trailFragmentation, 0},
           {}, {}, worldPos);
    }
  }

  if (!hit || impactT < 0.12f) {
    const float opacity = hit ? std::max(0.0f, 1.0f - impactT * 1.3f) : 0.92f;
    draw(VfxMaterial::MissileShell, frame.addVertices(shellGeometry()), shellGeometry().size(), bolt,
         {t, s_.shellDistortion, shellScale * 1.2f, s_.emissive * 0.7f}, {opacity, 0, 0, 0}, {}, worldPos);
  }
  if (!hit || impactT < 0.09f) {
    const float glow = s_.emissive * (0.55f + buildup * 0.4f + progress * 0.35f) * approach;
    draw(VfxMaterial::MissileCore, frame.addVertices(coreGeometry()), coreGeometry().size(), bolt, {t, s_.turbulence * 0.4f, shellScale * 0.8f, glow}, {},
         {}, worldPos);
  }

  // --- Three arcane threads trailing from the bolt (a 3-point curve each, 9 points) ---
  if (!hit) {
    for (int j = 0; j < 3; ++j) {
      const float phase = t * 4.2f + seed_ + j * 2.1f;
      const float opacity = 0.8f * (0.42f + 0.58f * std::abs(std::sin(phase * 1.4f)));
      if (opacity <= 0.02f) continue;
      const float side = static_cast<float>(j - 1);
      auto local = [&](glm::vec3 l) { return q * l * shellScale + pos; };
      three::CatmullRom thread{{local({side * 0.12f, 0.16f, 0.1f}), local({side * 0.2f, -0.16f, 0.11f + std::sin(phase) * 0.04f}),
                                local({side * 0.36f, -0.55f, -0.06f})}};
      std::vector<glm::vec3> world;
      for (const glm::vec3& p : thread.getPoints(8)) world.push_back(vfxFromEmber(p));
      const std::vector<VfxVertex> tris = ribbon(world, camera.position, kLineWidth * 0.5f);
      draw(VfxMaterial::Basic, frame.addVertices(tris), tris.size(), glm::mat4(1.0f), {}, {}, glm::vec4(three::hex(0xc5a2ff), opacity), worldPos);
    }
  }

  // --- The hit: a burst ring, four fracture arcs ---
  if (hit) {
    const float life = std::max(0.0f, 1.0f - impactT / kImpactLife);
    if (life > 0.0f) {
      const float scale = s_.impactSize * s_.finalImpactMultiplier * (0.25f + impactT * 1.6f);
      const glm::mat4 m = toWorld * glm::translate(glm::mat4(1.0f), target_) * three::eulerXYZ(kPi / 2.0f, 0.0f, impactAt_ * 2.0f) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(scale));
      draw(VfxMaterial::Basic, frame.addVertices(burstGeometry()), burstGeometry().size(), m, {}, {}, glm::vec4(three::hex(0xd9bdff), life * 0.9f),
           worldTarget);
    }
    const float opacity = std::max(0.0f, 1.0f - impactT / kImpactLife) * 0.82f;
    if (opacity > 0.01f) {
      const uint32_t first = frame.addVertices(fractureGeometry());
      for (int j = 0; j < 4; ++j) {
        const glm::mat4 m = toWorld * glm::translate(glm::mat4(1.0f), target_) *
                            three::eulerXYZ(j * 0.7f + 0.3f, j * 1.1f, t * (j % 2 ? -1.8f : 1.5f) + j * 1.9f) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(s_.impactSize * (0.65f + impactT * 1.1f)));
        draw(VfxMaterial::Basic, first, fractureGeometry().size(), m, {}, {}, glm::vec4(three::hex(j % 2 ? 0x8143e8 : 0xd5bcff), opacity), worldTarget);
      }
    }
  }

  // --- 28 crystal shards (InstancedMesh, MeshStandardMaterial): streaming behind the bolt, then swirling in on the hit ---
  std::vector<VfxInstance> shards;
  if (launched && !hit) {
    rng_.advance();
    for (int i = 0; i < s_.particleCount; ++i) {
      const float age = std::fmod(t * 1.6f + static_cast<float>(i) / std::max(1, s_.particleCount), 1.0f);
      const float seed = rng_.next();
      glm::vec3 drift{(seed - 0.5f) * 0.2f, (rng_.next() - 0.5f) * 0.2f, (rng_.next() - 0.5f) * 0.2f};
      const glm::vec3 p = pos - velocity * (age * s_.trailLength) + drift * age;
      shards.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(t * 6.0f + i, t * 4.0f + i * 0.3f, static_cast<float>(i)) *
                        glm::scale(glm::mat4(1.0f), glm::vec3((1.0f - age) * 0.035f))});
    }
  } else if (hit && impactT < kImpactLife) {
    const float age = three::clamp(impactT / kImpactLife, 0.0f, 1.0f);
    for (int i = 0; i < s_.particleCount; ++i) {
      const float seed = i * 2.399f;
      const float angle = seed + impactT * (2.5f + (i % 5) * 0.25f);
      const float radius = 0.84f * (1.0f - age) + 0.03f;
      const glm::vec3 p = target_ + glm::vec3(std::cos(angle) * radius, std::sin(angle * 0.83f) * radius * 0.55f, std::sin(angle) * radius * 0.72f);
      shards.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(angle, angle * 0.7f, angle * 1.3f) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(0.028f * std::max(0.04f, 1.0f - age)))});
    }
  }
  if (!shards.empty()) {
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), shards.begin(), shards.end());
    draw(VfxMaterial::Standard, frame.addVertices(shardGeometry()), shardGeometry().size(), toWorld, {0.32f, 0.08f, 0, 0},
         glm::vec4(three::hex(0x8a4aff), s_.emissive), glm::vec4(three::hex(0xbda1ff), 1.0f), hit ? worldTarget : worldPos, VfxBlend::Alpha,
         firstInstance, static_cast<uint32_t>(shards.size()));
  }

  // --- Lights: one riding with the bolt (gathering with it before the launch), the hit's flash ---
  if (launched && !hit) lights.push_back({worldPos, three::hex(0x9a66ff), s_.travelLightIntensity, s_.travelLightRadius});
  else if (!launched) lights.push_back({vfxFromEmber(formation(t)), three::hex(0x9a66ff), s_.travelLightIntensity * buildup, s_.travelLightRadius});
  if (hit) {
    const float life = std::max(0.0f, 1.0f - impactT / kImpactLife);
    if (life > 0.0f)
      lights.push_back({worldTarget, three::hex(0xe6d4ff), s_.impactLightIntensity * life * s_.finalImpactMultiplier, s_.impactLightRadius});
  }
}

}  // namespace ember
