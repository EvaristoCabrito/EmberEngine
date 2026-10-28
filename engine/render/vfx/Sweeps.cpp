#include "render/vfx/Sweeps.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
// DEFAULT_VARREDURA_SETTINGS
constexpr float kAngle = 2.25f, kRadius = 3.1f, kWaveSpeed = 1, kThickness = 0.42f, kHeight = 1.2f, kLeadingEdge = 0.055f, kTurbulence = 0.2f;
constexpr float kBreakup = 0.45f, kTrailLength = 1.25f, kTrailThickness = 0.16f, kDustAmount = 1, kDebrisVelocity = 1.2f, kImpactSize = 0.28f;
constexpr float kSpearLight = 3.4f, kSpearRadius = 1.7f, kImpactLight = 5, kImpactRadius = 1.35f, kResidual = 0.28f;
constexpr int kDebrisCount = 48;
constexpr uint32_t kSeed = 1977;
constexpr float kWaveDuration = 0.46f / std::max(0.25f, kWaveSpeed), kWaveStart = 0.36f, kWaveEnd = kWaveStart + kWaveDuration;

struct Xorshift {
  uint32_t state;
  float next() {
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return static_cast<float>(static_cast<double>(x) / 4294967296.0);
  }
};

// makeCrescent(tile 1, radiusScale, depth, thicknessScale): a tube around the arc with a noisy radius (smooth normals).
std::vector<VfxVertex> crescent(Xorshift& rng, float radiusScale, float depth, float thicknessScale) {
  constexpr int segments = 52, sides = 8;
  std::vector<glm::vec3> p;
  const float radius = kRadius * radiusScale, start = -kAngle * 0.56f, end = kAngle * 0.44f;
  for (int i = 0; i <= segments; ++i) {
    const float u = static_cast<float>(i) / segments, a = start + (end - start) * u;
    const float noise = (rng.next() - 0.5f) * kBreakup * 0.055f;
    const float r = radius * (0.88f + 0.12f * u + noise);
    const float cx = std::cos(a) * r, cy = std::sin(a) * r;
    const float thick = kThickness * thicknessScale * (0.75f + 0.4f * std::sin(kPi * u));
    for (int j = 0; j < sides; ++j) {
      const float v = static_cast<float>(j) / sides * kPi * 2.0f;
      p.push_back({cx + std::cos(v) * thick * 0.5f, cy + std::sin(v) * thick * 0.15f, std::sin(v) * thick * depth + (0.12f + u * kHeight * 0.14f)});
    }
  }
  std::vector<int> idx;
  for (int i = 0; i < segments; ++i)
    for (int j = 0; j < sides; ++j) {
      const int a = i * sides + j, b = i * sides + (j + 1) % sides, c = (i + 1) * sides + j, d = (i + 1) * sides + (j + 1) % sides;
      idx.insert(idx.end(), {a, c, b, b, c, d});
    }
  return vfxgeo::expandSmooth(p, idx);
}

const std::vector<VfxVertex>& spearGeometry() { static const auto g = vfxgeo::icosahedron(0.075f, 1); return g; }
const std::vector<VfxVertex>& stoneGeometry() { static const auto g = vfxgeo::box(0.065f, 0.075f, 0.065f); return g; }
const std::vector<VfxVertex>& outerRing() { static const auto g = vfxgeo::torus(3.2f, 0.085f, 8, 96, 2.0f * kPi); return g; }
const std::vector<VfxVertex>& innerRing() { static const auto g = vfxgeo::torus(3.2f * 0.78f, 0.045f, 10, 96, 2.0f * kPi); return g; }
const std::vector<VfxVertex>& shardGeometry() { static const auto g = vfxgeo::icosahedron(0.035f, 0); return g; }

struct Drawer {
  VfxFrame& frame;
  glm::vec3 sight;
  void operator()(VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color, VfxBlend blend,
                  uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) const {
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
    d.depthTest = true;
    frame.draws.push_back(d);
  }
};

}  // namespace

CleaveSweep::CleaveSweep(glm::vec3 origin, const std::vector<glm::vec3>& targets, float start) : origin_(vfxToEmber(origin)), start_(start + kQueued) {
  glm::vec2 average{0.0f};
  for (const glm::vec3& w : targets) {
    targets_.push_back(vfxToEmber(w));
    average += glm::vec2(targets_.back().x - origin_.x, targets_.back().y - origin_.y);
  }
  if (glm::dot(average, average) < 0.001f) average = {1, 0};
  direction_ = glm::normalize(average);
  // The three crescents, in Ember's order (the seed's numbers run through them).
  Xorshift rng{kSeed};
  blade_ = crescent(rng, 0.54f, 1.18f, 1.0f);
  edge_ = crescent(rng, 0.94f, 1.03f, std::max(0.08f, kLeadingEdge / kThickness));
  wake_ = crescent(rng, 0.46f * kTrailLength, 0.87f, std::max(0.08f, kTrailThickness / kThickness));
}

glm::vec3 CleaveSweep::spearAt(float t) const {
  const float grow = three::smoothstep(t, kWaveStart, kWaveEnd);
  const float sweep = three::clamp((t - kWaveStart) / kWaveDuration, 0.0f, 1.0f);
  const float sweepAngle = (sweep - 0.5f) * kAngle;
  const float radius = 0.36f + kRadius * 0.64f * grow;
  return {std::cos(sweepAngle) * radius, std::sin(sweepAngle) * radius, 0.22f + kHeight * 0.22f * std::sin(sweep * kPi)};
}

void CleaveSweep::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  const float baseAngle = std::atan2(direction_.y, direction_.x);
  const glm::mat4 root = vfxEmberToWorld() * glm::translate(glm::mat4(1.0f), origin_) * glm::rotate(glm::mat4(1.0f), baseAngle, glm::vec3(0, 0, 1));
  const Drawer draw{frame, vfxFromEmber(origin_)};
  const float grow = three::smoothstep(t, kWaveStart, kWaveEnd);
  const float collapse = 1.0f - three::smoothstep(t, kWaveEnd, kWaveEnd + 0.31f + kResidual);
  const float cs = std::cos(baseAngle), sn = std::sin(baseAngle);

  // --- Debris (opaque, first): stones kicked off each target as the arc reaches it ---
  {
    std::vector<VfxInstance> stones;
    const int n = std::min(120, std::max(1, static_cast<int>(std::lround(kDebrisCount * kDustAmount))));
    for (int i = 0; i < n; ++i) {
      const bool hasTarget = !targets_.empty();
      const glm::vec3 target = hasTarget ? targets_[static_cast<size_t>(i) % targets_.size()] : origin_;
      const glm::vec2 d{target.x - origin_.x, target.y - origin_.y};
      const float arrival = hasTarget ? 0.5f + three::clamp(glm::dot(d, direction_) / kRadius, 0.0f, 1.0f) * 0.26f : 0.5f;
      const float age = std::max(0.0f, t - arrival);
      const float distance = age * kDebrisVelocity * 1.4f;
      const float radial = d.x * direction_.x + d.y * direction_.y, side = -d.x * direction_.y + d.y * direction_.x;
      const glm::vec3 p{radial + direction_.x * distance + std::sin(i * 8.31f + age * 9.0f) * kTurbulence * 0.25f,
                        side + std::cos(i * 5.17f + age * 7.0f) * kTurbulence * 0.2f, 0.14f + std::abs(std::sin(age * 9.0f + i)) * kHeight * 0.3f};
      const float fade = hasTarget ? 1.0f - three::smoothstep(age, 0.2f, 0.75f) : 1.0f - three::smoothstep(t, 0.72f, 1.1f);
      stones.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(age * 7.0f + i, age * 9.0f, age * 4.0f) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(std::max(0.001f, fade * (0.4f + (i % 4) * 0.15f))))});
    }
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), stones.begin(), stones.end());
    draw(VfxMaterial::Standard, frame.addVertices(stoneGeometry()), stoneGeometry().size(), root, {1.0f, 0.0f, 0, 0}, glm::vec4(0.0f),
         glm::vec4(three::hex(0x9a9487), 1.0f), VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(stones.size()));
  }

  // --- The spearhead (MeshBasic, opaque) while the arc runs ---
  const glm::vec3 spear = spearAt(t);
  if (t > kWaveStart && t < kWaveEnd)
    draw(VfxMaterial::Basic, frame.addVertices(spearGeometry()), spearGeometry().size(), root * glm::translate(glm::mat4(1.0f), spear), {}, {},
         glm::vec4(three::hex(0xf4fbff), 1.0f), VfxBlend::Alpha);

  // --- The crescents (scaled along x as they grow): wake, blade, edge ---
  const glm::mat4 growing = glm::scale(glm::mat4(1.0f), {grow, 1.0f, 1.0f});
  if (t > 0.47f && collapse > 0.01f)
    draw(VfxMaterial::Standard, frame.addVertices(wake_), wake_.size(), root * glm::translate(glm::mat4(1.0f), {0, 0, 0.08f}) * growing,
         {1.0f, 0.0f, 0, 0}, glm::vec4(three::hex(0x597882), 0.5f), glm::vec4(three::hex(0x9aaeb5), 0.42f * collapse), VfxBlend::Alpha);
  if (t > 0.34f && collapse > 0.01f) {
    draw(VfxMaterial::Standard, frame.addVertices(blade_), blade_.size(), root * glm::translate(glm::mat4(1.0f), {0, 0, 0.17f}) * growing,
         {0.3f, 0.08f, 0, 0}, glm::vec4(three::hex(0x7ca9b9), 1.8f), glm::vec4(three::hex(0xc8e6f2), 0.76f * collapse), VfxBlend::Alpha);
    draw(VfxMaterial::Basic, frame.addVertices(edge_), edge_.size(), root * glm::translate(glm::mat4(1.0f), {0, 0, 0.23f}) * growing, {}, {},
         glm::vec4(three::hex(0xf2fbff), 0.95f), VfxBlend::Alpha);
  }

  // --- The trail: the spearhead's last 30 places (one per 60 Hz frame, as Ember's per-frame history) ---
  if (t > kWaveStart && t < kWaveEnd + 0.22f) {
    std::vector<glm::vec3> points;
    for (int k = 29; k >= 0; --k) {
      const float when = t - k / 60.0f;
      if (when < 0.0f) continue;
      points.push_back(glm::vec3(root * glm::vec4(spearAt(when), 1.0f)));
    }
    if (points.size() > 1) {
      const std::vector<VfxVertex> tris = vfxgeo::lineRibbon(points, camera.position, 0.006f);
      draw(VfxMaterial::Basic, frame.addVertices(tris), tris.size(), glm::mat4(1.0f), {}, {}, glm::vec4(three::hex(0xdff7ff), 0.82f), VfxBlend::Alpha);
    }
  }

  // --- Lights: the spearhead's, and a flash on each target as the arc arrives ---
  if (t > kWaveStart && t < kWaveEnd) {
    const glm::vec3 at = origin_ + glm::vec3(spear.x * cs - spear.y * sn, spear.x * sn + spear.y * cs, spear.z);
    lights.push_back({vfxFromEmber(at), three::hex(0xe8f8ff), kSpearLight * std::sin(kPi * three::clamp((t - kWaveStart) / kWaveDuration, 0.0f, 1.0f)),
                      kSpearRadius});
  }
  for (const glm::vec3& target : targets_) {
    const float along = glm::dot(glm::vec2(target.x - origin_.x, target.y - origin_.y), direction_);
    const float arrival = kWaveStart + three::clamp(along / kRadius, 0.0f, 1.0f) * kWaveDuration;
    if (t < arrival) continue;
    const float amp = 1.0f - three::smoothstep(t - arrival, 0.0f, 0.14f);
    if (amp > 0.0f) lights.push_back({vfxFromEmber(target + glm::vec3(0, 0, 0.5f)), three::hex(0xf3fbff), kImpactLight * amp, kImpactRadius * (1.0f + kImpactSize)});
  }
}

Varredura::Varredura(glm::vec3 origin, const std::vector<glm::vec3>& targets, float start) : origin_(vfxToEmber(origin)), start_(start + CleaveSweep::kQueued) {
  for (const glm::vec3& w : targets) offsets_.push_back(vfxToEmber(w) - origin_);
}

void Varredura::build(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  constexpr float kRingRadius = 3.2f;
  const float progress = three::clamp((t - 0.12f) / 0.68f, 0.0f, 1.0f);
  const float radius = kRingRadius * three::smoothstep(progress, 0.0f, 1.0f);
  const float fade = 1.0f - three::smoothstep(t, 0.72f, 1.12f);
  const glm::mat4 root = vfxEmberToWorld() * glm::translate(glm::mat4(1.0f), origin_) * glm::rotate(glm::mat4(1.0f), std::sin(t * 5.5f) * 0.035f, glm::vec3(0, 0, 1));
  const Drawer draw{frame, vfxFromEmber(origin_)};

  // Shards riding the wave (opaque, first).
  {
    std::vector<VfxInstance> shards;
    constexpr int count = 42;
    for (int i = 0; i < count; ++i) {
      const float angle = static_cast<float>(i) / count * kPi * 2.0f + std::sin(t * 6.0f + i * 1.7f) * 0.07f;
      const float travel = radius * (0.72f + (i % 5) * 0.07f);
      const glm::vec3 p{std::cos(angle) * travel, std::sin(angle) * travel, 0.1f + std::abs(std::sin(t * 12.0f + i * 2.3f)) * 0.36f};
      const float scale = (0.35f + (i % 4) * 0.16f) * fade;
      shards.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(t * 6.0f + i, t * 4.0f - i, t * 7.0f + i * 0.3f) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(std::max(0.001f, scale)))});
    }
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), shards.begin(), shards.end());
    draw(VfxMaterial::Standard, frame.addVertices(shardGeometry()), shardGeometry().size(), root, {0.9f, 0.0f, 0, 0}, glm::vec4(0.0f),
         glm::vec4(three::hex(0xcbdce0), 1.0f), VfxBlend::Alpha, firstInstance, count);
  }
  const glm::mat4 scaled = glm::scale(glm::mat4(1.0f), {progress, progress, 1.0f});
  if (t >= 0.12f && fade > 0.01f)
    draw(VfxMaterial::Basic, frame.addVertices(outerRing()), outerRing().size(), root * glm::translate(glm::mat4(1.0f), {0, 0, 0.12f}) * scaled, {}, {},
         glm::vec4(three::hex(0xe7fbff), 0.9f * fade), VfxBlend::Alpha);
  if (t >= 0.18f && fade > 0.01f)
    draw(VfxMaterial::Standard, frame.addVertices(innerRing()), innerRing().size(), root * glm::translate(glm::mat4(1.0f), {0, 0, 0.16f}) * scaled,
         {1.0f, 0.0f, 0, 0}, glm::vec4(three::hex(0x7ac8dc), 1.25f), glm::vec4(three::hex(0xa8d5e1), 0.58f * fade), VfxBlend::Alpha);

  // Lights: one orbiting the wave's edge, a flash on each target as the wave arrives.
  const float orbit = t * 9.5f;
  if (t > 0.12f && t < 0.86f)
    lights.push_back({vfxFromEmber(origin_ + glm::vec3(std::cos(orbit) * radius, std::sin(orbit) * radius, 0.42f)), three::hex(0xdaf8ff),
                      3.2f * (0.55f + 0.45f * std::sin(orbit * 0.5f)), 2.8f});
  for (const glm::vec3& offset : offsets_) {
    const float arrival = 0.12f + std::min(1.0f, std::hypot(offset.x, offset.y) / kRingRadius) * 0.68f;
    if (t < arrival) continue;
    const float amp = 1.0f - three::smoothstep(t - arrival, 0.0f, 0.12f);
    if (amp > 0.0f) lights.push_back({vfxFromEmber(origin_ + offset + glm::vec3(0, 0, 0.45f)), three::hex(0xe7fbff), 5.2f * amp, 1.4f});
  }
}

}  // namespace ember
