#include "render/vfx/CausticVenom.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kPixel = 1.0f / 34.0f;  // BattleSpells3D: fx.pixel = 1 / ZOOM_RADII[1]
constexpr uint32_t kGreen = 0x83ff22, kDeepGreen = 0x07561b, kHighlight = 0xd9ff83;
constexpr float kPoisonV2Scale = 0.78f;
// makeMaterial: MeshPhysicalMaterial roughness 0.16, metalness 0.08 (clearcoat / transmission not carried over).
constexpr float kRoughness = 0.16f, kMetalness = 0.08f;

const std::vector<VfxVertex>& orbGeometry() { static const auto g = vfxgeo::icosahedron(1.0f, 3); return g; }
const std::vector<VfxVertex>& beadGeometry() { static const auto g = vfxgeo::sphere(1.0f, 12, 8); return g; }
const std::vector<VfxVertex>& puffGeometry() { static const auto g = vfxgeo::sphere(1.0f, 20, 14); return g; }

float smooth(float a, float b, float x) {
  const float t = three::clamp((x - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

float CausticVenom::travelSeconds(glm::vec3 origin, glm::vec3 target) {
  return three::clamp(glm::length(target - origin) / (235.0f * kPixel), 0.7f, 1.65f);
}

CausticVenom::CausticVenom(glm::vec3 origin, glm::vec3 target, std::vector<glm::vec3> impactHexes, float start)
    : start3_(vfxToEmber(origin)), target_(vfxToEmber(target)), start_(start) {
  for (const glm::vec3& h : impactHexes) hexes_.push_back(vfxToEmber(h));
  travel_ = travelSeconds(origin, target);
  // buildFlightPath
  const glm::vec3 axis = three::normalize(target_ - start3_);
  pathSide_ = three::normalize(glm::vec3(-axis.y, axis.x, 0.0f));
  if (glm::dot(pathSide_, pathSide_) < 0.1f) pathSide_ = {0, 1, 0};
  const float distance = glm::length(target_ - start3_);
  flightArc_ = std::max(0.38f, std::min(distance * 0.12f, 1.05f));
  struct Layer {
    float radius, turns, phase;
  };
  const Layer layers[3] = {{0.12f, 2.4f, 0.0f}, {0.075f, 3.2f, 2.05f}, {0.045f, 1.9f, 4.1f}};
  for (const Layer& layer : layers) {
    three::CatmullRom curve;
    for (int i = 0; i <= 72; ++i) {
      const float t = static_cast<float>(i) / 72.0f;
      glm::vec3 centre = pathPosition(t);
      const float angle = t * kPi * 2.0f * layer.turns + layer.phase;
      centre += pathSide_ * (std::sin(angle) * layer.radius);
      centre.z += std::cos(angle) * layer.radius * 0.85f;
      curve.points.push_back(centre);
    }
    ribbons_.push_back(vfxgeo::tube(curve, 144, layer.radius * 0.52f, 7));
  }
}

glm::vec3 CausticVenom::pathPosition(float t) const {
  glm::vec3 p = glm::mix(start3_, target_, t);
  const float span = glm::length(target_ - start3_);
  const glm::vec3 side = three::normalize(glm::vec3(-(target_.y - start3_.y), target_.x - start3_.x, 0.0f));
  if (glm::dot(side, side) > 0.1f) p += side * (std::sin(t * kPi) * std::min(0.1f, span * 0.025f));
  p += glm::vec3(0, 0, 1) * (std::sin(t * kPi) * flightArc_);
  p.z += std::sin(t * kPi) * 0.16f;
  return p;
}

void CausticVenom::build(float now, const Camera&, const ElementSpritesV2& poison, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  const glm::mat4 toWorld = vfxEmberToWorld();
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  glm::vec3 sight, VfxBlend blend, bool depthTest = true) {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = p0;
    d.gpu.params1 = p1;
    d.gpu.color = color;
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    d.gpu.material = static_cast<uint32_t>(material);
    d.gpu.sightAt = sight;
    d.blend = blend;
    d.depthTest = depthTest;
    frame.draws.push_back(d);
  };
  // makeMaterial(colour, emissive, emissiveIntensity, opacity): a glassy lit surface (NormalBlending, double-sided).
  auto glass = [&](uint32_t first, size_t count, const glm::mat4& model, uint32_t colour, uint32_t emissive, float intensity, float opacity,
                   glm::vec3 sight) {
    draw(VfxMaterial::Standard, first, count, model, {kRoughness, kMetalness, 0, 0}, glm::vec4(three::hex(emissive), intensity),
         glm::vec4(three::hex(colour), opacity), sight, VfxBlend::Alpha);
  };
  // setLight(radius, strength): 3.8 x strength, reaching `radius`.
  auto light = [&](glm::vec3 atEmber, float radius, float strength) {
    lights.push_back({vfxFromEmber(atEmber), three::hex(kGreen), 3.8f * strength, radius});
  };

  if (t < kCharge) {
    // --- The charge: a glassy orb swelling and pulsing at the caster ---
    const float ct = three::clamp(t / kCharge, 0.0f, 1.0f);
    const float pulse = 0.72f + std::sin(t * 26.0f) * 0.12f + ct * 0.28f;
    const glm::mat4 group = toWorld * glm::translate(glm::mat4(1.0f), start3_) * three::eulerXYZ(t * 2.3f, t * 1.6f, t * 2.9f) *
                            glm::scale(glm::mat4(1.0f), glm::vec3((0.14f + 0.12f * ct) * pulse));
    const uint32_t orb = frame.addVertices(orbGeometry());
    const glm::vec3 sight = vfxFromEmber(start3_);
    glass(orb, orbGeometry().size(), group * glm::scale(glm::mat4(1.0f), glm::vec3(1.48f)), kGreen, 0x37b509, 1.15f, 0.12f, sight);
    glass(orb, orbGeometry().size(), group, kHighlight, 0x69d51b, 1.9f, 0.18f, sight);
    light(start3_, 3.8f, 0.52f + 0.48f * ct);
    return;
  }

  if (t < kCharge + travel_) {
    // --- The flight: the ribbons and beads grow along the path behind the head ---
    const float e = t - kCharge;
    const float tt = three::clamp(e / travel_, 0.0f, 1.0f);
    const float eased = tt * tt * (3.0f - 2.0f * tt);
    const glm::vec3 head = pathPosition(eased);
    const glm::vec3 sight = vfxFromEmber(head);
    static const uint32_t kColours[3] = {kGreen, kHighlight, kDeepGreen};
    static const float kOpacity[3] = {0.74f, 0.85f, 0.9f};
    for (size_t i = 0; i < ribbons_.size(); ++i) {
      const size_t shown = static_cast<size_t>(std::floor(static_cast<float>(ribbons_[i].size()) * eased)) / 3 * 3;  // setDrawRange
      if (shown == 0) continue;
      glass(frame.addVertices(ribbons_[i]), shown, toWorld, kColours[i], 0x52d817, 1.4f, kOpacity[i] * (0.8f + 0.2f * std::sin(e * 15.0f)), sight);
    }
    const glm::mat4 headModel = toWorld * glm::translate(glm::mat4(1.0f), head) * three::eulerXYZ(e * 4.8f, e * 3.3f, e * 2.1f) *
                                glm::scale(glm::mat4(1.0f), {0.22f + 0.06f * std::sin(e * 22.0f), 0.17f, 0.14f});
    glass(frame.addVertices(orbGeometry()), orbGeometry().size(), headModel, kHighlight, 0x60df12, 2.2f, 0.12f, sight);
    uint32_t bead = 0xFFFFFFFFu;
    for (int i = 0; i < 7; ++i) {
      const float revealAt = static_cast<float>(i + 1) / 8.0f;
      if (eased < revealAt) continue;
      if (bead == 0xFFFFFFFFu) bead = frame.addVertices(beadGeometry());
      const float scale = (0.035f + static_cast<float>(static_cast<int>(std::lround(revealAt * 7.0f)) % 3) * 0.009f) *
                          (0.84f + 0.16f * std::sin(e * 20.0f + revealAt * 9.0f));
      const glm::mat4 m = toWorld * glm::translate(glm::mat4(1.0f), pathPosition(revealAt)) * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
      glass(bead, beadGeometry().size(), m, i % 2 ? kGreen : kHighlight, 0x7cf219, 1.8f, 0.84f, sight);
    }
    light(head, 3.1f, 0.88f + 0.12f * std::sin(e * 19.0f));
    return;
  }

  // --- The impact: on every struck hex, smoke lobes (drawn first, over everything), the Poison V2 effect, a light ---
  const float e = t - kCharge - travel_;
  const float it = three::clamp(e / kImpact, 0.0f, 1.0f);
  const float settle = 1.0f - smooth(0.42f, 1.0f, it);
  ElementSpritesV2 plume = poison;
  plume.scale = kPoisonV2Scale, plume.intensity = 1.15f, plume.emissive = 1.35f, plume.opacity = 1.0f, plume.animationSpeed = 1.1f;
  uint32_t puff = 0xFFFFFFFFu;
  for (size_t index = 0; index < hexes_.size(); ++index) {
    const glm::vec3 c = hexes_[index];
    const glm::vec3 sight = vfxFromEmber(c);
    if (puff == 0xFFFFFFFFu) puff = frame.addVertices(puffGeometry());
    for (int p = 0; p < 6; ++p) {
      const float seed = static_cast<float>(index) * 7.31f + p * 2.17f;
      const float angle = p * kPi * 2.0f / 6.0f + static_cast<float>(index) * 0.7f;
      const float radius = 0.045f + (p % 2) * 0.055f;
      // x drifts by sin(e x 0.7 + seed) x 0.0007 every frame (Engine2: 60 per second), integrated.
      const float x = c.x + std::cos(angle) * radius + 0.0007f * 60.0f / 0.7f * (std::cos(seed) - std::cos(e * 0.7f + seed));
      const float y = c.y + p * 0.13f + std::sin(seed) * 0.045f + it * (0.12f + p * 0.018f);
      const float z = c.z + (0.18f + p * 0.085f);
      const float expand = 1.0f + it * (0.24f + p * 0.025f);
      const float base = p == 0 ? 0.37f : 0.275f + (p % 3) * 0.025f;
      const glm::mat4 m = toWorld * glm::translate(glm::mat4(1.0f), {x, y, z}) * three::eulerXYZ(angle * 0.45f, angle * 0.3f, angle + e * (0.12f + p * 0.025f)) *
                          glm::scale(glm::mat4(1.0f), {base * expand, base * (1.1f + (p % 2) * 0.16f) * expand, base * (0.72f + (p % 3) * 0.09f) * expand});
      draw(VfxMaterial::VenomSmoke, puff, puffGeometry().size(), m, {e, (p == 0 ? 0.7f : 0.58f) * settle, seed, 0}, {}, {}, sight, VfxBlend::Alpha, false);
    }
  }
  for (size_t index = 0; index < hexes_.size(); ++index) {
    plume.build(hexes_[index], e, 1.0f, frame);
    const float flash = 0.65f + 0.35f * std::sin(kPi * std::min(1.0f, e / 0.8f));
    lights.push_back({vfxFromEmber(hexes_[index] + glm::vec3(0, 0, 0.38f)), three::hex(kGreen), 2.4f * flash * settle, 1.8f});
  }
}

}  // namespace ember
