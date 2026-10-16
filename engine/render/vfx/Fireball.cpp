#include "render/vfx/Fireball.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
// BattleSpells3D: fx.pixel = 1 / ZOOM_RADII[1] (one hex is Ember's 34 px); Fireball's pixel-sized constants scale by it.
constexpr float kPixel = 1.0f / 34.0f;

const std::vector<VfxVertex>& ballGeometry() { static const auto g = vfxgeo::icosahedron(1.0f, 24); return g; }

// PlaneGeometry(1, 1) as two triangles (uv v up), for the impact's camera-facing particles.
const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = [] {
    const VfxVertex a{{-0.5f, -0.5f, 0}, 0, {0, 0, 1}, 0}, b{{0.5f, -0.5f, 0}, 1, {0, 0, 1}, 0}, c{{0.5f, 0.5f, 0}, 1, {0, 0, 1}, 1},
        d{{-0.5f, 0.5f, 0}, 0, {0, 0, 1}, 1};
    return std::vector<VfxVertex>{a, b, c, a, c, d};
  }();
  return g;
}

float smooth(float a, float b, float x) {
  const float t = three::clamp((x - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

float Fireball::travelSeconds(glm::vec3 origin, glm::vec3 target) {
  return three::clamp(glm::length(target - origin) / (260.0f * kPixel), 1.25f, 2.5f);
}

Fireball::Fireball(glm::vec3 origin, glm::vec3 target, float aoeRadius, float start)
    : start3_(vfxToEmber(origin)), target_(vfxToEmber(target)), aoe_(std::max(0.0f, aoeRadius)), start_(start) {
  travel_ = travelSeconds(origin, target);
  // cast(): Etapa 02 was authored for a one-hex preview; scaled to the combat footprint, the footprint its hard limit.
  impactScale_ = std::max(1.0f, aoe_ * 0.72f);
  // FireballImpactVFX.setOrigin: 0.02 above (along Engine2's own y, which in the Ember frame lies on the board).
  origin_ = target_ + glm::vec3(0.0f, 0.02f, 0.0f);
  buildParticles();
}

// FireballImpactVFX.build: every layer's particles from the seed, in Engine2's order (smoke first).
void Fireball::buildParticles() {
  three::Lcg rand;
  rand.state = s_.seed;
  particles_.clear();
  enum class Mode { Sphere, Ground, Up };
  auto add = [&](Kind kind, int count, float delay, float life, float size, float speed, float drag, float gravity, float height, Mode mode) {
    for (int i = 0; i < count && particles_.size() < 512; ++i) {
      const float angle = rand.next() * kPi * 2.0f;
      const float y = mode == Mode::Ground ? 0.015f + rand.next() * 0.11f : mode == Mode::Up ? 0.1f + rand.next() * 0.36f : (rand.next() - 0.5f) * height;
      const float radius = mode == Mode::Up ? 0.12f + rand.next() * 0.52f : mode == Mode::Ground ? rand.next() * 0.17f : rand.next() * 0.24f;
      const float hAngle = std::acos(2.0f * rand.next() - 1.0f);
      glm::vec3 dir;
      if (mode == Mode::Ground) dir = {std::cos(angle), 0.0f, std::sin(angle)};
      else if (mode == Mode::Up) dir = three::normalize({std::cos(angle) * 0.7f, 0.35f + rand.next() * 0.8f, std::sin(angle) * 0.7f});
      else dir = {std::sin(hAngle) * std::cos(angle), std::cos(hAngle), std::sin(hAngle) * std::sin(angle)};
      if (kind == Kind::Primary && dir.y < -0.15f) dir.y = std::abs(dir.y) * 0.48f;
      if (kind == Kind::Secondary && dir.y < 0.0f) dir.y *= 0.28f;
      if (kind == Kind::Smoke) {
        const float dx = (rand.next() - 0.5f) * 0.4f, dy = 0.2f + rand.next() * 0.4f, dz = (rand.next() - 0.5f) * 0.4f;
        dir = {dx, dy, dz};
      }
      const float v = speed * (0.62f + rand.next() * 0.7f);
      Particle p{};
      p.kind = kind;
      p.delay = std::max(0.0f, delay + (rand.next() - 0.5f) * std::min(0.045f, life * 0.08f));
      p.life = life * (0.78f + rand.next() * 0.42f);
      p.size = size * (0.62f + rand.next() * 0.8f);
      p.phase = rand.next() * 16.0f;
      p.spin = (rand.next() - 0.5f) * (kind == Kind::Smoke ? 0.8f : 4.0f);
      p.rotation = rand.next() * kPi * 2.0f;
      p.drag = std::max(0.0f, drag * (0.72f + rand.next() * 0.56f));
      p.gravity = gravity * (0.7f + rand.next() * 0.6f);
      p.initial = {std::cos(angle) * radius, y, std::sin(angle) * radius};
      p.velocity = dir * v;
      particles_.push_back(p);
    }
  };
  add(Kind::Smoke, s_.smokeCount, 0.18f + s_.smokeOffset, s_.smokeLife, s_.smokeSize, s_.smokeRise, 0.8f, -0.24f, 0.5f, Mode::Up);
  add(Kind::Primary, s_.primaryCount, 0.02f + s_.primaryOffset, s_.primaryLife, s_.primarySize, s_.primarySpeed, s_.primaryDrag, 1.6f, s_.verticalSpread,
      Mode::Sphere);
  add(Kind::Secondary, s_.secondaryCount, 0.08f + s_.secondaryOffset, s_.secondaryLife, s_.secondarySize, s_.secondarySpeed, 2.6f, -s_.buoyancy, 0.5f,
      Mode::Up);
  add(Kind::Radial, s_.radialCount, 0.07f + s_.radialOffset, 0.4f, 0.36f, s_.radialSpeed, 4.3f, -1.8f, 0.1f, Mode::Ground);
  add(Kind::Spark, s_.sparkCount, 0.03f + s_.sparkOffset, s_.sparkLife, 0.075f, s_.sparkSpeed, s_.sparkDrag, s_.sparkGravity, 1.4f, Mode::Sphere);
  add(Kind::Ember, s_.emberCount, 0.08f + s_.emberOffset, s_.emberLife, 0.16f, s_.emberSpeed, 0.9f, s_.emberGravity, 1.0f, Mode::Sphere);
  duration_ = 1.45f;
  for (const Particle& p : particles_) duration_ = std::max(duration_, p.delay + p.life);
}

void Fireball::build(float now, const Camera& camera, const VfxFlipbook& fire, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  const glm::mat4 toWorld = vfxEmberToWorld();
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  glm::vec3 sight, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1, uint32_t texture = 0xFFFFFFFFu) {
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
    d.gpu.texture = texture;
    d.instances = instances;
    d.blend = blend;
    d.depthTest = true;  // Engine2: the projectile and the impact (worldOcclusion) are depth-tested
    frame.draws.push_back(d);
  };

  if (t < travel_) {
    // --- The flight (updateProjectile): smoothstep along the line, arched up by sin(pi p) ---
    const float progress = travel_ > 0.0f ? t / travel_ : 1.0f;
    const float eased = progress * progress * (3.0f - 2.0f * progress);
    const float dist = glm::length(target_ - start3_);
    const glm::vec3 pos = glm::mix(start3_, target_, eased) + glm::vec3(0, 0, 1) * (std::sin(kPi * progress) * std::max(15.0f * kPixel, dist * 0.12f));
    const glm::vec3 worldPos = vfxFromEmber(pos);
    const glm::mat4 group = toWorld * glm::translate(glm::mat4(1.0f), pos) * glm::scale(glm::mat4(1.0f), glm::vec3(0.47f));
    const uint32_t ball = frame.addVertices(ballGeometry());
    // OldFireBall.update: core spins one way, the shell (1.45x) the other, the shell's noise running ahead.
    draw(VfxMaterial::Flame, ball, ballGeometry().size(), group * glm::rotate(glm::mat4(1.0f), t * 0.6f, glm::vec3(0, 0, 1)), {t, 0.28f, 1.0f, 0.0f}, {},
         {}, worldPos, VfxBlend::Additive);
    draw(VfxMaterial::Flame, ball, ballGeometry().size(),
         group * glm::rotate(glm::mat4(1.0f), -t * 0.45f, glm::vec3(0, 0, 1)) * glm::scale(glm::mat4(1.0f), glm::vec3(1.45f)),
         {t * 1.15f + 3.7f, 0.55f, 0.45f, 1.0f}, {}, {}, worldPos, VfxBlend::Additive);
    lights.push_back({worldPos, three::hex(0xff8a38), 8.0f * 4.9f * (0.94f + std::sin(t * 7.2f) * 0.06f), 5.0f});
    return;
  }

  // --- The impact (FireballImpactVFX.update + upload), on its own clock from the hit ---
  const float ti = t - travel_;
  const float ws = impactScale_;
  const glm::vec3 worldOrigin = vfxFromEmber(origin_);
  std::vector<VfxInstance> live;
  for (const Particle& p : particles_) {
    const float age = ti - p.delay;
    const bool active = age >= 0.0f && age < p.life;
    if (!active) continue;  // alpha 0: nothing drawn
    const float tt = three::clamp(age / p.life, 0.0f, 1.0f);
    const float dragTerm = p.drag < 0.001f ? age : (1.0f - std::exp(-p.drag * std::max(0.0f, age))) / p.drag;
    const float fall = 0.5f * p.gravity * age * age;
    float offsetX = (p.initial.x + p.velocity.x * dragTerm) * ws;
    float offsetY = (p.initial.y + p.velocity.y * dragTerm - fall) * ws;
    float scale = p.size * ws;
    float alpha = std::pow(std::sin(kPi * std::max(0.0f, tt)), p.kind == Kind::Smoke ? 0.45f : 0.72f);
    float emission = 1.0f;
    glm::vec3 color{1.0f};
    float kind = 0.0f;
    switch (p.kind) {
      case Kind::Primary:
        kind = 1, scale *= 0.68f + std::sin(kPi * tt) * 0.92f, emission = s_.primaryEmission * (1.35f - tt * 0.62f), color = {1, 0.9f - tt * 0.58f, 0.48f - tt * 0.38f};
        break;
      case Kind::Secondary:
        kind = 1, scale *= 0.72f + tt * 0.7f, emission = 1.25f * (1.0f - tt * 0.5f), color = {1, 0.72f - tt * 0.46f, 0.24f - tt * 0.17f};
        break;
      case Kind::Radial:
        kind = 0, scale *= 0.58f + std::sin(kPi * tt) * 0.45f, emission = 1.8f * (1.0f - tt), color = {1, 0.66f - tt * 0.42f, 0.16f};
        break;
      case Kind::Spark:
        kind = 1, scale *= 0.7f + (1.0f - tt) * 0.45f, emission = 5.2f * (1.0f - tt), color = {1, 0.97f - tt * 0.68f, 0.74f - tt * 0.58f};
        break;
      case Kind::Ember:
        kind = 1, scale *= 0.7f + std::sin(kPi * tt) * 0.25f, emission = 2.4f * (1.0f - tt), color = {1, 0.56f - tt * 0.38f, 0.12f + (1.0f - tt) * 0.1f};
        break;
      case Kind::Smoke:
        kind = 2, scale *= 0.65f + tt * s_.smokeExpansion * 2.2f, alpha *= s_.smokeOpacity * std::pow(1.0f - tt, 0.65f), emission = 0.42f,
        color = {0.24f, 0.22f, 0.2f};
        break;
    }
    // Every visible particle kept inside the spell's area (the damage cells' radius).
    const float centerDistance = std::hypot(offsetX, offsetY);
    const float halfExtent = scale * 0.7072f;
    const float available = std::max(0.0f, aoe_ - halfExtent);
    if (centerDistance > available && centerDistance > 0.0f) {
      const float fit = available / centerDistance;
      offsetX *= fit;
      offsetY *= fit;
    }
    scale = std::min(scale, std::max(0.001f, aoe_ * 1.4144f));
    VfxInstance inst;
    inst.a = {origin_.x + offsetX, origin_.y + offsetY, origin_.z + (p.initial.z + p.velocity.z * dragTerm) * ws, scale};
    inst.b = {p.rotation + p.spin * std::max(0.0f, age), std::fmod(p.phase + std::max(0.0f, age) * s_.flipbookFps, 16.0f), alpha, emission};
    inst.c = {color, kind};
    live.push_back(inst);
  }
  if (!live.empty()) {
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), live.begin(), live.end());
    // params1: a cell's size and the crop in pixels; color.xy: 1 / the texture's size (vfx.hlsl ImpactParticle kind 0).
    draw(VfxMaterial::ImpactParticle, frame.addVertices(quadGeometry()), quadGeometry().size(), toWorld, {},
         {fire.cellWidth, fire.cellHeight, fire.cropX, fire.cropY}, {1.0f / fire.textureWidth, 1.0f / fire.textureHeight, 0, 0}, worldOrigin,
         VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(live.size()), fire.texture);
  }

  // The opening flash: a camera-facing glow 0.08 above the impact, growing as it fades.
  const float flashT = (ti - s_.flashOffset) / std::max(0.015f, s_.flashDuration);
  const float flashPulse = ti - s_.flashOffset >= 0.0f && flashT < 1.0f ? std::pow(1.0f - flashT, 2.4f) : 0.0f;
  if (flashPulse > 0.003f) {
    const float size = s_.flashSize * (0.35f + std::max(0.0f, flashT) * 3.2f) * ws;
    const glm::vec3 centre = vfxFromEmber(origin_ + glm::vec3(0.0f, 0.08f, 0.0f));
    const glm::vec3 up = glm::cross(camera.right, camera.forward);
    std::vector<VfxVertex> quad = quadGeometry();
    for (VfxVertex& v : quad) v.position = centre + camera.right * (v.position.x * size) + up * (v.position.y * size);
    draw(VfxMaterial::ImpactFlash, frame.addVertices(quad), quad.size(), glm::mat4(1.0f), {flashPulse * std::min(1.0f, s_.flashIntensity / 18.0f), s_.flashIntensity, 0, 0},
         {}, glm::vec4(three::hex(0xfff2c1), 1.0f), worldOrigin, VfxBlend::Additive);
  }

  // The impact light: a fast flare, then a flickering residue; its reach grows and shrinks inside the area.
  const float lightAttack = smooth(0.0f, 0.045f, ti) * (1.0f - smooth(0.1f, std::max(0.12f, s_.lightDecay), ti));
  const float residual =
      ti > 0.36f && ti < s_.lightDecay ? 0.13f * (0.65f + 0.35f * std::sin(ti * 49.0f)) * (1.0f - smooth(0.55f, s_.lightDecay, ti)) : 0.0f;
  const float intensity = s_.peakLight * ws * ws * (lightAttack + residual) * 1.5f;  // FireballVFX: impact light x 1.5
  const float distance = std::min(aoe_, s_.lightRadius * ws * (0.62f + 0.58f * smooth(0.0f, 0.09f, ti) * (1.0f - smooth(0.18f, 0.62f, ti))));
  if (intensity > 0.0f) lights.push_back({worldOrigin, three::hex(0xff7624), intensity, distance});
}

}  // namespace ember
