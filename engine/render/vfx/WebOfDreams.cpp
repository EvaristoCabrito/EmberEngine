#include "render/vfx/WebOfDreams.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
// DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS
constexpr float kRadius = 1, kFilamentThickness = 0.027f, kCurvature = 0.42f, kVerticalSpread = 1.05f, kDisplacement = 0.055f;
constexpr int kSeedCount = 8, kFilamentCount = 24, kTargetFilamentCount = 7, kNodeCount = 8, kSecondaryLightCount = 2;
constexpr float kFormationSpeed = 1, kPulseSpeed = 1.25f, kPulseBrightness = 2.8f, kNodeSize = 0.075f, kTightening = 0.3f;
constexpr float kCentralLight = 12, kCentralLightRadius = 5.5f, kSecondaryLight = 6, kSecondaryLightRadius = 4.2f, kBindingFlash = 20, kSustained = 2.2f;
constexpr uint32_t kSeed = 50827;
constexpr int kTubeSides = 5, kTubeSegments = 18;

// JavaScript's Math.round (halves round up).
float jsRound(float x) { return std::floor(x + 0.5f); }

// tubeBundle(strands, thickness): every strand a tapered tube (centripetal curve, Frenet frames, 18 x 5); u = aAlong,
// v packs aDelay and aPhase (vfx.hlsl WebThread).
std::vector<VfxVertex> tubeBundle(const std::vector<WebOfDreams::Strand>& strands, float thickness) {
  std::vector<VfxVertex> out;
  for (const WebOfDreams::Strand& strand : strands) {
    if (strand.points.size() < 2) continue;
    const three::CatmullRom curve{strand.points};
    std::vector<glm::vec3> tangents, normals, binormals;
    three::frenetFrames(curve, kTubeSegments, tangents, normals, binormals);
    const std::vector<float> lengths = curve.getLengths();
    const float packed = strand.delay + 2.0f * jsRound(strand.phase / (2.0f * kPi) * 1000.0f);
    std::vector<VfxVertex> grid;
    for (int row = 0; row <= kTubeSegments; ++row) {
      const float t = static_cast<float>(row) / kTubeSegments;
      const glm::vec3 centre = three::getPointAt(curve, t, lengths);
      const float taper = thickness * strand.width * (0.4f + 0.6f * std::sin(kPi * t));
      for (int side = 0; side <= kTubeSides; ++side) {
        const float angle = static_cast<float>(side) / kTubeSides * kPi * 2.0f;
        const glm::vec3 radial = normals[row] * std::cos(angle) + binormals[row] * std::sin(angle);
        grid.push_back({centre + radial * taper, t, radial, packed});
      }
    }
    for (int row = 0; row < kTubeSegments; ++row)
      for (int side = 0; side < kTubeSides; ++side) {
        const int a = row * (kTubeSides + 1) + side, b = a + kTubeSides + 1;
        out.insert(out.end(), {grid[a], grid[b], grid[a + 1], grid[b], grid[b + 1], grid[a + 1]});
      }
  }
  return out;
}

const std::vector<VfxVertex>& knotGeometry() { static const auto g = vfxgeo::icosahedron(0.5f, 1); return g; }
const std::vector<VfxVertex>& shardGeometry() { static const auto g = vfxgeo::tetrahedron(0.5f, 0); return g; }

}  // namespace

WebOfDreams::WebOfDreams(glm::vec3 center, float radiusTiles, const std::vector<glm::vec3>& targets, float start, float releaseAt)
    : center_(vfxToEmber(center)), start_(start), releaseAt_(releaseAt) {
  three::Mulberry random{static_cast<uint32_t>(kSeed + static_cast<int>(jsRound(center_.x * 11.0f)) + static_cast<int>(jsRound(center_.y * 7.0f)))};
  extent_ = std::max(0.7f, radiusTiles * 0.95f) * kRadius;
  const int count = std::max(4, std::min(12, kSeedCount));
  for (int i = 0; i < count; ++i) {
    // The anchor knots around the perimeter, so the spell reads as a net.
    const float angle = static_cast<float>(i) / count * kPi * 2.0f + (random.next() - 0.5f) * 0.12f;
    const float r = extent_ * (0.9f + random.next() * 0.08f);
    const float z = (random.next() - 0.5f) * kVerticalSpread * 1.45f;
    nodes_.push_back({std::cos(angle) * r, std::sin(angle) * r * 0.72f, z});
    phase_.push_back(random.next() * kPi * 2.0f);
  }
  std::vector<Strand> network;
  const int filamentCount = std::max(6, std::min(40, kFilamentCount));
  auto addThread = [&](int from, int to, float delay, float width) {
    const glm::vec3 a = nodes_[from], b = nodes_[to];
    const glm::vec3 midpoint = glm::mix(a, b, 0.5f);
    const float bx = (random.next() - 0.5f) * kCurvature * 0.8f;
    const float by = (random.next() - 0.5f) * kCurvature * 0.8f;
    const float sign = random.next() < 0.5f ? -1.0f : 1.0f;
    const glm::vec3 bend{bx, by, sign * (0.16f + random.next() * 0.44f) * kVerticalSpread};
    const glm::vec3 first = glm::mix(a, midpoint, 0.58f) + bend * 0.55f;
    const glm::vec3 second = glm::mix(midpoint, b, 0.42f) + bend;
    Strand s;
    s.points = {a, first, midpoint + bend, second, b};
    s.delay = delay + random.next() * 0.22f;
    s.phase = random.next() * 6.28f;
    s.width = width * (0.82f + random.next() * 0.34f);
    network.push_back(s);
  };
  std::set<std::pair<int, int>> used;
  auto connect = [&](int from, int to, float delay, float width) {
    const std::pair<int, int> key{std::min(from, to), std::max(from, to)};
    if (from == to || used.contains(key)) return;
    used.insert(key);
    addThread(from, to, delay, width);
  };
  for (int i = 0; i < count; ++i) connect(i, (i + 1) % count, 0.12f, 0.82f);
  for (int attempt = 0; static_cast<int>(network.size()) < filamentCount && attempt < 180; ++attempt) {
    const int from = static_cast<int>(std::floor(random.next() * count));
    const int offset = 2 + static_cast<int>(std::floor(random.next() * std::max(1, count - 3)));
    const float delay = 0.22f + random.next() * 0.2f;
    const float width = 0.62f + random.next() * 0.18f;
    connect(from, (from + offset) % count, delay, width);
  }
  // Thin, incomplete tendrils reach beyond a few knots and fade into empty space.
  for (int i = 0; i < std::min(5, filamentCount / 5); ++i) {
    const glm::vec3 a = nodes_[(i * 3 + 1) % count];
    const float angle = std::atan2(a.y, a.x) + (random.next() - 0.5f) * 0.8f;
    const float dz = (random.next() - 0.5f) * kVerticalSpread;
    const glm::vec3 b = a + glm::vec3(std::cos(angle), std::sin(angle) * 0.72f, dz) * (extent_ * (0.16f + random.next() * 0.16f));
    const float mx = (random.next() - 0.5f) * kCurvature, my = (random.next() - 0.5f) * kCurvature, mz = (random.next() - 0.5f) * kVerticalSpread;
    const glm::vec3 mid = glm::mix(a, b, 0.54f) + glm::vec3(mx, my, mz);
    Strand s;
    s.points = {a, glm::mix(a, mid, 0.62f), mid, b};
    s.delay = 0.56f + random.next() * 0.2f;
    s.phase = random.next() * 6.28f;
    s.width = 0.48f + random.next() * 0.28f;
    network.push_back(s);
  }
  net_ = tubeBundle(network, kFilamentThickness);

  // updateTargets: each unit's own wrapping strands (seeded by its id: here its index in the zone).
  for (size_t ti = 0; ti < targets.size(); ++ti) {
    const std::string id = "u" + std::to_string(ti);
    uint32_t sum = 0;
    for (char ch : id) sum += static_cast<unsigned char>(ch);
    three::Mulberry r{kSeed + sum};
    std::vector<Strand> strands;
    const int wraps = std::min(3, std::max(1, static_cast<int>(jsRound(kTargetFilamentCount / 3.0f))));
    for (int wrap = 0; wrap < wraps; ++wrap) {
      Strand s;
      const float offset = static_cast<float>(wrap) / wraps * kPi * 2.0f + r.next() * 0.28f;
      for (int step = 0; step <= 12; ++step) {
        const float t = step / 12.0f;
        const float angle = offset + t * kPi * 2.0f * 1.15f;
        const float radius = 0.3f + std::sin(t * kPi) * 0.07f;
        s.points.push_back({std::cos(angle) * radius, -0.5f + t * 1.28f, std::sin(angle) * 0.22f + (r.next() - 0.5f) * 0.04f});
      }
      s.delay = 0.12f + wrap * 0.08f;
      s.phase = r.next() * 6.28f;
      s.width = 1.25f;
      strands.push_back(s);
    }
    const int n = std::max(3, std::min(12, kTargetFilamentCount));
    for (int i = 0; i < n; ++i) {
      const float angle = static_cast<float>(i) / n * kPi * 2.0f + r.next() * 0.24f;
      const float radius = 0.22f + r.next() * 0.18f;
      const float sy = -0.38f + r.next() * 0.2f, sz = (r.next() - 0.5f) * 0.32f;
      const float oy = 0.12f + r.next() * 0.58f, oz = (r.next() - 0.5f) * 0.52f;
      const float fy = -0.12f + r.next() * 0.68f, fz = (r.next() - 0.5f) * 0.46f;
      const float ey = -0.42f + r.next() * 0.9f, ez = (r.next() - 0.5f) * 0.36f;
      const glm::vec3 a{std::cos(angle) * radius, sy, sz}, over{std::cos(angle + 1.15f) * radius * 0.6f, oy, oz};
      const glm::vec3 far{std::cos(angle + 2.55f) * radius, fy, fz}, end{std::cos(angle + kPi) * radius * 0.82f, ey, ez};
      Strand s;
      s.points = {a, glm::mix(a, over, 0.6f), over, far, end};
      s.delay = 0.52f + r.next() * 0.42f;
      s.phase = r.next() * 6.28f;
      s.width = 0.72f + r.next() * 0.62f;
      strands.push_back(s);
    }
    targets_.push_back({vfxToEmber(targets[ti]) - center_, tubeBundle(strands, kFilamentThickness * 1.2f)});
  }
}

void WebOfDreams::build(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float since = now - start_ - kShotTravel;
  if (since < 0.0f || done(now)) return;
  // The field's own clock stops while it releases (Engine2: elapsed frozen, releaseElapsed runs).
  const bool releasing = now >= releaseAt_;
  const float e = std::min(since, releaseAt_ - start_ - kShotTravel) * kFormationSpeed;
  const float release = releasing ? three::smoothstep(now - releaseAt_, 0.0f, kRelease) : 0.0f;
  const float formation = std::min(2.0f, e / kFormation);
  const float bind = std::exp(-std::pow((e - 1.42f) / 0.13f, 2.0f));
  const float sustained = e > kFormation ? kSustained : 0.0f;
  const float live = releasing ? 1.0f - release : 1.0f;
  if (live <= 0.01f) return;
  const glm::mat4 group = vfxEmberToWorld() * glm::translate(glm::mat4(1.0f), center_);
  const glm::vec3 sight = vfxFromEmber(center_);
  const float tightening = kTightening * three::smoothstep(e, 1.04f, 1.48f);
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) {
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
  };

  // --- The net (renderOrder 3) ---
  draw(VfxMaterial::WebThread, frame.addVertices(net_), net_.size(), group, {e, kPulseSpeed, formation, tightening},
       {kDisplacement, 0.9f * (1.0f + bind * kPulseBrightness * 0.42f), 0.9f * live, 0}, {}, VfxBlend::Additive);

  // --- The knots (updateNodes) ---
  {
    std::vector<VfxInstance> knots;
    const int count = std::max(2, std::min(static_cast<int>(nodes_.size()), kNodeCount));
    for (int i = 0; i < count; ++i) {
      const glm::vec3 p = nodes_[static_cast<size_t>(i) % nodes_.size()];
      const float pulse = 1.0f + 0.18f * std::sin(e * 2.2f + phase_[i]);
      const float tighten = 1.0f - kTightening * 0.24f * three::smoothstep(e, 1.08f, 1.5f);
      const float size = kNodeSize * pulse * tighten;
      knots.push_back({glm::translate(glm::mat4(1.0f), {p.x, p.y, p.z + std::sin(e * 1.6f + phase_[i]) * 0.035f}) *
                       three::eulerXYZ(e * 0.24f + phase_[i], e * 0.31f, phase_[i] * 0.4f) *
                       glm::scale(glm::mat4(1.0f), {size * (1.0f + std::sin(e * 1.3f + i) * 0.14f), size, size * 0.74f})});
    }
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), knots.begin(), knots.end());
    draw(VfxMaterial::Standard, frame.addVertices(knotGeometry()), knotGeometry().size(), group, {0.25f, 0.08f, 0, 0},
         glm::vec4(three::hex(0x7959ff), 1.8f + std::exp(-std::pow((e - 1.42f) / 0.14f, 2.0f)) * 4.0f), glm::vec4(three::hex(0xb8a2ff), 0.92f * live),
         VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(knots.size()));
  }

  // --- 32 drifting shards (updateParticles) ---
  {
    std::vector<VfxInstance> shards;
    const int count = std::max(0, std::min(120, kSeedCount * 4));
    for (int i = 0; i < count; ++i) {
      const float phase = phase_[static_cast<size_t>(i) % phase_.size()] + i * 0.31f;
      const float radius = 0.28f + static_cast<float>((i * 37) % 100) / 100.0f * 0.72f;
      const float angle = phase + e * (0.16f + (i % 5) * 0.035f);
      const float pull = three::smoothstep(e, 0.8f, 1.45f) * 0.58f;
      const glm::vec3 p{std::cos(angle) * radius * (1.0f - pull), std::sin(angle * 1.17f) * radius * 0.75f * (1.0f - pull),
                        std::sin(angle * 0.73f + phase) * kVerticalSpread * 0.45f};
      const float size = (0.018f + (i % 4) * 0.006f) * (0.7f + 0.3f * std::sin(e * 3.0f + phase));
      shards.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(angle, angle * 0.7f, phase) * glm::scale(glm::mat4(1.0f), glm::vec3(size))});
    }
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), shards.begin(), shards.end());
    draw(VfxMaterial::Standard, frame.addVertices(shardGeometry()), shardGeometry().size(), group, {0.38f, 0.05f, 0, 0},
         glm::vec4(three::hex(0x8061ff), 1.25f), glm::vec4(three::hex(0xb9a5ff), live), VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(shards.size()));
  }

  // --- Each unit inside: its own strands (renderOrder 4) ---
  for (const Wrapped& w : targets_)
    draw(VfxMaterial::WebThread, frame.addVertices(w.strands), w.strands.size(), group * glm::translate(glm::mat4(1.0f), w.offset),
         {e, kPulseSpeed, formation, tightening}, {kDisplacement, 1.1f * (1.0f + bind * kPulseBrightness * 0.5f), 0.98f * live, 0}, {},
         VfxBlend::Additive);

  // --- Lights: the central one forming, flashing on the bind, sustained; two travelling with the pulses ---
  const float formationLight = kCentralLight * three::smoothstep(e, 0.02f, 0.54f);
  lights.push_back({vfxFromEmber(center_ + glm::vec3(0, 0, 0.12f)), three::hex(0x8667ff), (formationLight + bind * kBindingFlash + sustained) * live,
                    kCentralLightRadius});
  for (int i = 0; i < kSecondaryLightCount; ++i) {
    const size_t k = static_cast<size_t>(i * 3 + 1) % nodes_.size();
    const glm::vec3 node = nodes_[k];
    const float travelPulse = 0.5f + 0.5f * std::sin(e * kPulseSpeed + phase_[k % phase_.size()]);
    lights.push_back({vfxFromEmber(center_ + node + glm::vec3(0, 0, 0.16f)), three::hex(0x9276ff),
                      (kSecondaryLight * (0.35f + travelPulse * 0.65f) + bind * kBindingFlash * 0.35f + sustained * 0.28f) * live, kSecondaryLightRadius});
  }
}

}  // namespace ember
