#include "render/vfx/WaterFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/IceParts.h"

namespace ember {

using namespace icefx;

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr float kPi = std::numbers::pi_v<float>;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kWaterLight{0.42f, 0.84f, 0.95f};  // the water's own light: sea teal
constexpr float kWater = 14.0f;                         // the IceStorm material's water look (shaders/vfx.hlsl)
constexpr float kGravity = 9.8f;
constexpr float kCrest = 2.0f;  // added to an instance's foam: the shader foams the mesh's top (the wave's crest)

float rnd(uint32_t seed, uint32_t i, uint32_t salt) { return hash01(seed * 0x2545F491u + i * 0x9E3779B1u, salt); }
float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

// A grid surface from at(u, v) (u, v 0..1), its normals from the grid itself (or straight out from the middle: a sphere).
template <class F>
std::vector<VfxVertex> surface(int cols, int rows, F at, bool radial = false) {
  const int w = cols + 1;
  std::vector<glm::vec3> p(static_cast<size_t>(w * (rows + 1))), n(p.size());
  for (int r = 0; r <= rows; ++r)
    for (int c = 0; c <= cols; ++c) p[static_cast<size_t>(r * w + c)] = at(static_cast<float>(c) / static_cast<float>(cols), static_cast<float>(r) / static_cast<float>(rows));
  for (int r = 0; r <= rows; ++r)
    for (int c = 0; c <= cols; ++c) {
      const glm::vec3& q = p[static_cast<size_t>(r * w + c)];
      glm::vec3 out;
      if (radial) {
        out = glm::dot(q, q) > 1e-8f ? glm::normalize(q) : kUp;
      } else {
        const glm::vec3 du = p[static_cast<size_t>(r * w + std::min(c + 1, cols))] - p[static_cast<size_t>(r * w + std::max(c - 1, 0))];
        const glm::vec3 dv = p[static_cast<size_t>(std::min(r + 1, rows) * w + c)] - p[static_cast<size_t>(std::max(r - 1, 0) * w + c)];
        out = glm::cross(du, dv);
        out = glm::dot(out, out) > 1e-10f ? glm::normalize(out) : kUp;
      }
      n[static_cast<size_t>(r * w + c)] = out;
    }
  std::vector<VfxVertex> g;
  g.reserve(static_cast<size_t>(cols * rows * 6));
  auto vtx = [&](int r, int c) {
    const size_t i = static_cast<size_t>(r * w + c);
    return vertex(p[i], n[i], static_cast<float>(c) / static_cast<float>(cols), static_cast<float>(r) / static_cast<float>(rows));
  };
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c) {
      const VfxVertex a = vtx(r, c), b = vtx(r, c + 1), cc = vtx(r + 1, c + 1), d = vtx(r + 1, c);
      g.insert(g.end(), {a, b, cc, a, cc, d});
    }
  return g;
}

// A unit sphere: the orb, the jet, a drop, a burst of water, a fleck of foam.
const std::vector<VfxVertex>& sphereGeometry() {
  static const std::vector<VfxVertex> g = surface(
      18, 11,
      [](float u, float v) {
        const float a = u * kTau, b = v * kPi;
        return glm::vec3(std::sin(b) * std::cos(a), -std::cos(b), std::sin(b) * std::sin(a));
      },
      true);
  return g;
}
// A ring of water lying on the ground (radius 1, its tube 0.1): a ripple.
const std::vector<VfxVertex>& torusGeometry() {
  static const std::vector<VfxVertex> g = surface(40, 6, [](float u, float v) {
    const float a = u * kTau, b = v * kTau, r = 1.0f + 0.1f * std::cos(b);
    return glm::vec3(r * std::cos(a), 0.1f * std::sin(b), r * std::sin(a));
  });
  return g;
}
// A splash crown (radius 1, height 1 to its spikes' 1.5): a sheet of water thrown up and out where water hits the ground, its rim
// torn into points.
const std::vector<VfxVertex>& crownGeometry() {
  static const std::vector<VfxVertex> g = surface(48, 9, [](float u, float v) {
    const float a = u * kTau;
    const float spike = std::pow(0.5f + 0.5f * std::cos(a * 9.0f + 0.8f * std::sin(a * 4.0f)), 5.0f);
    const float r = 0.45f + 0.55f * std::pow(v, 0.75f);
    const float y = v * (1.0f + 0.55f * spike * v * v * v);
    return glm::vec3(r * std::cos(a), y, r * std::sin(a));
  });
  return g;
}
// A curling wall of water, facing +z: x -1..1 across, its back from z -1.2 rising to the crest (y 1) that curls over the front and
// hangs as a lip; bowed forward in the middle, lower at its two ends, its crest uneven.
const std::vector<VfxVertex>& waveGeometry() {
  static const std::vector<VfxVertex> g = surface(30, 24, [](float u, float v) {
    static const glm::vec2 kProfile[] = {{-1.25f, 0.0f}, {-0.7f, 0.3f}, {-0.25f, 0.72f}, {0.08f, 1.0f}, {0.38f, 0.96f}, {0.56f, 0.72f}, {0.5f, 0.5f}};
    constexpr int kLast = static_cast<int>(std::size(kProfile)) - 1;
    const float s = v * static_cast<float>(kLast);
    const int i = std::min(static_cast<int>(s), kLast - 1);
    const float t = s - static_cast<float>(i);
    const glm::vec2 p0 = kProfile[std::max(i - 1, 0)], p1 = kProfile[i], p2 = kProfile[i + 1], p3 = kProfile[std::min(i + 2, kLast)];
    const float t2 = t * t, t3 = t2 * t;
    const glm::vec2 q = 0.5f * (2.0f * p1 + (p2 - p0) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (3.0f * p1 - p0 - p2 * 3.0f + p3) * t3);
    const float x = u * 2.0f - 1.0f;
    const float y = q.y * (1.0f - 0.35f * std::pow(std::abs(x), 4.0f)) + 0.06f * std::sin(x * 9.0f + 1.3f) * v * v;
    const float z = q.x + 0.2f * (1.0f - x * x);
    return glm::vec3(x, y, z);
  });
  return g;
}

// A body of water: c = alpha, seed + 100 x its thickness (the shader's depth colour), breakup 0..1, foam (+ kCrest: its top foams).
VfxInstance water(const glm::mat4& m, float alpha, float seed, float thick, float breakup, float foam) {
  VfxInstance inst;
  inst.matrix = m;
  inst.c = {alpha, std::min(seed, 0.999f) + std::floor(std::clamp(thick, 0.01f, 1.5f) * 100.0f), breakup, foam};
  return inst;
}
// A ball of water at `at`, stretched `stretch` times along `along` (its motion).
glm::mat4 blob(glm::vec3 at, glm::vec3 along, float radius, float stretch) {
  const glm::vec3 axis = glm::dot(along, along) > 1e-8f ? glm::normalize(along) : kUp;
  return glm::translate(glm::mat4(1.0f), at) * alongAxis(axis, 0.0f) * glm::scale(glm::mat4(1.0f), {radius, radius * stretch, radius});
}
glm::mat4 flatOn(glm::vec3 at, glm::vec3 size, float spin = 0.0f) {
  return glm::rotate(glm::translate(glm::mat4(1.0f), at), spin, kUp) * glm::scale(glm::mat4(1.0f), size);
}

void draw(VfxFrame& frame, float time, uint32_t first, size_t count, const std::vector<VfxInstance>& list, glm::vec3 sight) {
  if (list.empty()) return;
  VfxDraw d;
  d.gpu.model = glm::mat4(1.0f);
  d.gpu.params0 = {time, kWater, 1.0f, 1.0f};
  d.gpu.params1 = glm::vec4(0.0f);
  d.gpu.color = {1, 1, 1, 1};
  d.gpu.firstVertex = first;
  d.gpu.vertexCount = static_cast<uint32_t>(count);
  d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
  frame.instances.insert(frame.instances.end(), list.begin(), list.end());
  d.instances = static_cast<uint32_t>(list.size());
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::IceStorm);
  d.gpu.sightAt = sight;
  d.blend = VfxBlend::Alpha;
  d.depthTest = true;
  frame.draws.push_back(d);
}

// The batches one build fills: each kind of mesh, one draw.
struct Batch {
  std::vector<VfxInstance> spheres, rings, crowns, waves;
};

// A drop thrown from `from` at `velocity` at `born`, falling at real gravity onto the ground at `ground`; on landing, its own small
// splash (a tiny crown) and a ripple running out.
void drop(Batch& out, float now, glm::vec3 from, glm::vec3 velocity, float born, float ground, float radius, float seed) {
  const float age = now - born;
  if (age < 0.0f) return;
  const float dy = std::max(from.y - ground, 0.0f);
  const float land = (velocity.y + std::sqrt(velocity.y * velocity.y + 2.0f * kGravity * dy)) / kGravity;
  if (age < land) {
    const glm::vec3 at = from + velocity * age - kUp * (0.5f * kGravity * age * age);
    const glm::vec3 v = velocity - kUp * (kGravity * age);
    out.spheres.push_back(water(blob(at, v, radius, 1.0f + std::min(glm::length(v) * 0.09f, 1.6f)), 0.9f, seed, radius * 2.0f, 0.0f, 0.12f));
    return;
  }
  const float since = age - land;
  glm::vec3 at = from + velocity * land - kUp * (0.5f * kGravity * land * land);
  at.y = ground + 0.015f;
  if (since < 0.28f) {
    const float rise = easeOut(0.0f, 0.1f, since);
    out.crowns.push_back(water(flatOn(at, glm::vec3(radius * (1.6f + 1.6f * rise), radius * 3.2f * rise, radius * (1.6f + 1.6f * rise)), seed * kTau), 0.85f, seed,
                               radius, ease(0.08f, 0.28f, since), 0.35f));
  }
  if (since < 0.75f) {
    const float r = radius * 2.0f + 0.4f * easeOut(0.0f, 0.75f, since);
    out.rings.push_back(water(flatOn(at, glm::vec3(r, 0.3f, r)), 0.55f * (1.0f - ease(0.25f, 0.75f, since)), seed, 0.03f, 0.0f, 0.45f));
  }
}

}  // namespace

float WaterSpell::lead(Style style, glm::vec3 casterFeet, glm::vec3 hand, glm::vec3 first) {
  if (style == Style::Jet) return kJetGather + glm::length(first + kUp * 0.9f - hand) / kJetSpeed;
  glm::vec2 d{first.x - casterFeet.x, first.z - casterFeet.z};
  return kWaveGather + std::max(glm::length(d) - kWaveFrom, 0.0f) / kWaveSpeed;
}

WaterSpell::WaterSpell(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 hand, std::vector<Target> hexes, float start, float impact, uint32_t seed)
    : style_(style),
      feet_(casterFeet),
      hand_(hand),
      casterHeight_(std::clamp(casterHeight, 1.0f, 4.0f)),
      start_(start),
      impact_(std::max(impact, start + 0.25f)),
      hexes_(std::move(hexes)),
      seed_(seed) {
  if (hexes_.empty()) hexes_.push_back({feet_ + glm::vec3(0.0f, 0.0f, 2.0f), 0.0f});
  if (style_ == Style::Jet) {
    // the Sahagin's: small, at his hands' size (the user, 2026-11-26: "smaller on the sahagin to match his casting video")
    k_ = std::clamp(casterHeight_ / 2.0f, 0.75f, 1.2f) * 0.85f;
    const Target& t = hexes_.front();
    const glm::vec3 body = t.feet + kUp * (t.height > 0.0f ? t.height * 0.55f : 0.9f);
    const float flight = glm::length(body - hand_) / kJetSpeed;
    release_ = std::max(start_ + 0.2f, impact_ - flight);
    glm::vec2 d{body.x - hand_.x, body.z - hand_.z};
    if (glm::dot(d, d) > 1e-6f) d = glm::normalize(d), direction_ = {d.x, 0.0f, d.y};
    end_ = impact_ + 1.7f;
    return;
  }
  // The wave: its way (from the caster to the middle of the cone), how far it runs, how fast the cone widens, when it crashes on each hex.
  k_ = 1.0f;
  glm::vec3 centre(0.0f);
  for (const Target& t : hexes_) centre += t.feet;
  centre /= static_cast<float>(hexes_.size());
  glm::vec2 d{centre.x - feet_.x, centre.z - feet_.z};
  if (glm::dot(d, d) > 1e-6f) d = glm::normalize(d), direction_ = {d.x, 0.0f, d.y};
  const glm::vec3 right = glm::normalize(glm::cross(kUp, direction_));
  firstAt_ = 1e9f, reach_ = 0.0f, spread_ = 0.3f;
  for (const Target& t : hexes_) {
    const glm::vec3 o = t.feet - feet_;
    const float along = glm::dot(o, direction_), across = std::abs(glm::dot(o, right));
    firstAt_ = std::min(firstAt_, along), reach_ = std::max(reach_, along);
    spread_ = std::max(spread_, across / std::max(along, 0.8f));
  }
  firstAt_ = std::max(firstAt_, kWaveFrom);
  reach_ = std::max(reach_, firstAt_) + 0.6f;
  release_ = std::max(start_ + 0.3f, impact_ - (firstAt_ - kWaveFrom) / kWaveSpeed);
  for (const Target& t : hexes_) crashAt_.push_back(release_ + (std::max(glm::dot(t.feet - feet_, direction_), kWaveFrom) - kWaveFrom) / kWaveSpeed);
  end_ = release_ + (reach_ - kWaveFrom) / kWaveSpeed + 2.0f;
}

void WaterSpell::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  if (style_ == Style::Jet) buildJet(now, camera, frame, lights);
  else buildWave(now, camera, frame, lights);
}

void WaterSpell::buildJet(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  Batch out;
  const float k = k_;
  const Target& target = hexes_.front();
  const glm::vec3 body = target.feet + kUp * (target.height > 0.0f ? target.height * 0.55f : 0.9f);
  const float flight = std::max(impact_ - release_, 0.05f);
  const float dist = glm::length(body - hand_);
  auto path = [&](float s) { return glm::mix(hand_, body, s) + kUp * (std::sin(kPi * s) * 0.12f * dist); };

  // The gathering: an orb of water grows at the hand, wobbling, drops spiralling in to it.
  if (now < release_ + 0.04f) {
    const float f = easeOut(start_, release_, now);
    const float r = (0.045f + 0.12f * f) * k, wob = 1.0f + 0.13f * std::sin(now * 19.0f) * f;
    out.spheres.push_back(water(flatOn(hand_, glm::vec3(r * wob, r / wob, r * wob), now * 1.7f), 0.95f, rnd(seed_, 1, 2), r * 2.0f, 0.0f, 0.12f));
    for (uint32_t j = 0; j < 9; ++j) {
      const float phase = rnd(seed_, j, 11), s = std::fmod((now - start_) * 1.7f + phase, 1.0f);
      const float a = phase * kTau + s * 5.0f, out_ = (1.0f - s) * 0.5f * k;
      const glm::vec3 at = hand_ + glm::vec3(std::cos(a) * out_, (1.0f - s) * 0.22f * k * std::sin(phase * 9.0f), std::sin(a) * out_);
      out.spheres.push_back(water(blob(at, hand_ - at, 0.022f * k * (0.6f + 0.6f * s), 1.6f), 0.85f * f, phase, 0.04f, 0.0f, 0.1f));
    }
    lights.push_back({hand_, kWaterLight, (1.0f + 4.0f * f) * k, 2.6f * k});
  }
  // The flight: the orb stretched into a jet, its trail thinning behind it, drops shed along the way falling to the ground.
  if (now >= release_ && now < impact_) {
    const float s = (now - release_) / flight;
    const glm::vec3 head = path(s), ahead = path(std::min(s + 0.03f, 1.0f)) - head;
    out.spheres.push_back(water(blob(head, ahead, 0.11f * k, 2.4f), 0.95f, rnd(seed_, 2, 3), 0.22f * k, 0.0f, 0.15f));
    for (int j = 1; j <= 10; ++j) {
      const float sj = s - 0.032f * static_cast<float>(j);
      if (sj < 0.0f) break;
      const glm::vec3 at = path(sj);
      const float fall = 1.0f - static_cast<float>(j) / 12.0f;
      out.spheres.push_back(water(blob(at, path(std::min(sj + 0.03f, 1.0f)) - at, 0.1f * k * fall, 1.9f), 0.9f * fall, rnd(seed_, 3 + static_cast<uint32_t>(j), 4),
                                  0.2f * k * fall, 0.0f, 0.12f + 0.04f * static_cast<float>(j)));
    }
    lights.push_back({head, kWaterLight, 3.0f * k, 2.4f * k});
  }
  for (uint32_t b = 0; b < 7; ++b) {  // shed drops (they go on falling after the hit)
    const float sb = (static_cast<float>(b) + 0.5f) / 7.0f, born = release_ + sb * flight;
    const glm::vec3 lateral = glm::normalize(glm::cross(kUp, direction_)) * ((rnd(seed_, b, 21) - 0.5f) * 1.2f);
    drop(out, now, path(sb), direction_ * (kJetSpeed * 0.25f) + lateral + kUp * (0.4f + 0.8f * rnd(seed_, b, 22)), born,
         glm::mix(feet_.y, target.feet.y, sb), (0.018f + 0.014f * rnd(seed_, b, 23)) * k, rnd(seed_, b, 24));
  }
  // The hit: the water thrown open on the body, a crown where it falls to the ground, drops flung out, ripples running out.
  if (now >= impact_) {
    const float a = now - impact_;
    if (a < 0.42f)
      out.spheres.push_back(water(flatOn(body, glm::vec3((0.13f + 0.42f * easeOut(0.0f, 0.2f, a)) * k), a * 3.0f), 0.9f, rnd(seed_, 5, 6), 0.3f * k,
                                  ease(0.04f, 0.38f, a), 0.45f));
    if (a >= 0.05f && a < 0.7f) {
      const float rise = easeOut(0.05f, 0.22f, a);
      out.crowns.push_back(water(flatOn(target.feet + kUp * 0.02f, glm::vec3(0.34f * k * (0.6f + 0.6f * rise), 0.36f * k * rise, 0.34f * k * (0.6f + 0.6f * rise)),
                                        rnd(seed_, 7, 8) * kTau),
                                 0.9f, rnd(seed_, 7, 9), 0.1f, ease(0.3f, 0.68f, a), 0.5f));
    }
    const glm::vec3 right = glm::normalize(glm::cross(kUp, direction_));
    for (uint32_t j = 0; j < 22; ++j) {
      const float r1 = rnd(seed_, j, 31), r2 = rnd(seed_, j, 32), r3 = rnd(seed_, j, 33);
      const glm::vec3 v = direction_ * (0.6f + 1.8f * r1) + right * ((r2 - 0.5f) * 3.2f) + kUp * (0.9f + 2.1f * r3);
      drop(out, now, body + direction_ * 0.05f, v * k, impact_, target.feet.y, (0.018f + 0.026f * rnd(seed_, j, 34)) * k, rnd(seed_, j, 35));
    }
    for (int j = 0; j < 3; ++j) {
      const float since = a - (0.08f + 0.14f * static_cast<float>(j));
      if (since < 0.0f || since > 0.95f) continue;
      const float r = (0.18f + 0.85f * easeOut(0.0f, 0.95f, since)) * k;
      out.rings.push_back(water(flatOn(target.feet + kUp * 0.02f, glm::vec3(r, 0.35f, r)), 0.7f * (1.0f - ease(0.3f, 0.95f, since)), rnd(seed_, 40 + static_cast<uint32_t>(j), 41),
                                0.04f, 0.0f, 0.5f));
    }
    if (a < 0.6f) lights.push_back({body, kWaterLight, (7.0f * std::exp(-a * 7.0f) + 0.8f) * k, 3.4f * k});
  }
  const uint32_t sphere = frame.addVertices(sphereGeometry()), ring = frame.addVertices(torusGeometry()), crown = frame.addVertices(crownGeometry());
  draw(frame, now, ring, torusGeometry().size(), out.rings, target.feet);
  draw(frame, now, crown, crownGeometry().size(), out.crowns, target.feet);
  draw(frame, now, sphere, sphereGeometry().size(), out.spheres, body);
}

void WaterSpell::buildWave(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  Batch out;
  const glm::vec3 right = glm::normalize(glm::cross(kUp, direction_));
  auto groundNear = [&](glm::vec3 p) {
    float best = 1e9f, y = feet_.y;
    for (const Target& t : hexes_)
      if (const float dd = glm::length(glm::vec2(t.feet.x - p.x, t.feet.z - p.z)); dd < best) best = dd, y = t.feet.y;
    return y;
  };
  const glm::vec3 mound = feet_ + direction_ * kWaveFrom;
  const float runEnd = release_ + (reach_ - kWaveFrom) / kWaveSpeed;

  // The gathering: water wells up out of the ground before the caster into a mound, drops climbing to the hands, an orb there.
  if (now < release_ + 0.35f) {
    const float f = easeOut(start_, release_, now), gone = ease(release_, release_ + 0.35f, now);
    const float wob = 1.0f + 0.08f * std::sin(now * 11.0f);
    out.spheres.push_back(water(flatOn(mound, glm::vec3(0.95f * f * wob, 0.5f * f / wob, 0.95f * f * wob), now * 0.8f), 0.92f, rnd(seed_, 1, 2), 0.7f,
                                gone, 0.25f));
    for (uint32_t j = 0; j < 16 && now < release_; ++j) {
      const float phase = rnd(seed_, j, 11), s = std::fmod((now - start_) * 1.3f + phase, 1.0f);
      const float a = phase * kTau + s * 4.0f;
      const glm::vec3 from = mound + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * 0.7f;
      const glm::vec3 at = glm::mix(from, hand_, s) + glm::vec3(std::cos(a + s * 3.0f), 0.0f, std::sin(a + s * 3.0f)) * (0.25f * (1.0f - s));
      out.spheres.push_back(water(blob(at, hand_ - from, 0.035f * (0.6f + 0.6f * s), 1.8f), 0.85f * f, phase, 0.06f, 0.0f, 0.1f));
    }
    if (now < release_ + 0.05f) {
      const float r = 0.08f + 0.2f * f;
      out.spheres.push_back(water(flatOn(hand_, glm::vec3(r * wob, r / wob, r * wob), now * 1.3f), 0.95f, rnd(seed_, 3, 4), r * 2.0f, 0.0f, 0.15f));
    }
    for (int j = 0; j < 2; ++j) {  // ripples round the mound
      const float since = std::fmod(now - start_ + 0.45f * static_cast<float>(j), 0.9f);
      const float r = 0.6f + 1.2f * easeOut(0.0f, 0.9f, since);
      out.rings.push_back(water(flatOn(mound + kUp * 0.02f, glm::vec3(r, 0.35f, r)), 0.55f * f * (1.0f - ease(0.4f, 0.9f, since)) * (1.0f - gone), rnd(seed_, 5, 6), 0.04f,
                                0.0f, 0.5f));
    }
    lights.push_back({mound + kUp * 0.8f, kWaterLight, 2.0f + 6.0f * f * (1.0f - gone), 5.0f});
  }
  // The run: a curling wall of water rolls out over the cone, as wide as the cone where it is, foam on its crest, spray thrown ahead.
  const float runFor = std::max(runEnd - release_, 0.05f);
  if (now >= release_ && now < runEnd + 0.55f) {
    const float front = kWaveFrom + std::min(now - release_, runFor) * kWaveSpeed;
    const float half = 0.9f + front * spread_;
    const float collapse = 1.0f - ease(runEnd - 0.05f, runEnd + 0.5f, now);
    const float tall = 1.65f * easeOut(release_, release_ + 0.3f, now) * (0.35f + 0.65f * collapse);
    glm::vec3 at = feet_ + direction_ * front;
    at.y = groundNear(at);
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(right * half, 0.0f), m[1] = glm::vec4(kUp * tall, 0.0f), m[2] = glm::vec4(direction_ * (tall * 0.95f), 0.0f), m[3] = glm::vec4(at, 1.0f);
    out.waves.push_back(water(m, 0.93f, rnd(seed_, 8, 9), 0.75f, ease(runEnd, runEnd + 0.55f, now), 0.3f + kCrest));
    for (uint32_t j = 0; j < 14; ++j) {  // flecks of foam riding the crest
      const float u = (static_cast<float>(j) + rnd(seed_, j, 51)) / 14.0f * 2.0f - 1.0f;
      const glm::vec3 p = at + right * (u * half * 0.92f) + kUp * (tall * (0.93f + 0.06f * std::sin(now * 9.0f + static_cast<float>(j)))) +
                          direction_ * (tall * 0.25f);
      out.spheres.push_back(water(flatOn(p, glm::vec3(0.07f + 0.05f * rnd(seed_, j, 52)), now * 2.0f + static_cast<float>(j)), 0.85f * collapse, rnd(seed_, j, 53), 0.05f,
                                  0.0f, 1.0f));
    }
    lights.push_back({at + kUp * 1.1f, kWaterLight, 7.0f * collapse + 1.0f, half + 3.5f});
  }
  for (uint32_t b = 0; b < 34; ++b) {  // spray thrown off the crest as it runs (it goes on falling after)
    const float born = release_ + (static_cast<float>(b) + 0.5f) / 34.0f * runFor;
    const float front = kWaveFrom + (born - release_) * kWaveSpeed;
    const float half = 0.9f + front * spread_;
    glm::vec3 from = feet_ + direction_ * (front + 0.4f) + right * ((rnd(seed_, b, 61) * 2.0f - 1.0f) * half * 0.9f);
    const float ground = groundNear(from);
    from.y = ground + 1.5f;
    const glm::vec3 v = direction_ * (kWaveSpeed * 0.8f + 1.6f * rnd(seed_, b, 62)) + kUp * (1.0f + 1.8f * rnd(seed_, b, 63)) + right * (rnd(seed_, b, 64) - 0.5f);
    drop(out, now, from, v, born, ground, 0.03f + 0.03f * rnd(seed_, b, 65), rnd(seed_, b, 66));
  }
  // Each hex as the wave crashes on it: a tall crown, a spray of drops, ripples racing out; on a body, the water bursting over it.
  for (size_t i = 0; i < hexes_.size(); ++i) {
    const Target& t = hexes_[i];
    const float a = now - crashAt_[i];
    const uint32_t h = static_cast<uint32_t>(i) * 977u;
    if (a < 0.0f) continue;
    if (a < 0.95f) {
      const float rise = easeOut(0.0f, 0.2f, a);
      out.crowns.push_back(water(flatOn(t.feet + kUp * 0.02f, glm::vec3(0.6f * (0.55f + 0.7f * rise), 1.15f * rise, 0.6f * (0.55f + 0.7f * rise)), rnd(seed_, h, 71) * kTau),
                                 0.9f, rnd(seed_, h, 72), 0.12f, ease(0.4f, 0.92f, a), 0.55f));
    }
    if (t.height > 0.0f && a < 0.55f) {
      const glm::vec3 body = t.feet + kUp * (t.height * 0.5f);
      out.spheres.push_back(water(flatOn(body, glm::vec3(0.3f + 0.6f * easeOut(0.0f, 0.25f, a)), a * 2.0f), 0.9f, rnd(seed_, h, 73), 0.5f, ease(0.08f, 0.5f, a), 0.5f));
    }
    for (uint32_t j = 0; j < 9; ++j) {
      const float r1 = rnd(seed_, h + j, 81), r2 = rnd(seed_, h + j, 82);
      const float ang = r1 * kTau;
      const glm::vec3 v = (glm::vec3(std::cos(ang), 0.0f, std::sin(ang)) * 1.4f + direction_ * 1.2f) * (0.6f + 0.8f * r2) + kUp * (2.2f + 2.0f * rnd(seed_, h + j, 83));
      drop(out, now, t.feet + kUp * 0.6f, v, crashAt_[i] + 0.04f, t.feet.y, 0.03f + 0.03f * rnd(seed_, h + j, 84), rnd(seed_, h + j, 85));
    }
    for (int j = 0; j < 2; ++j) {
      const float since = a - 0.1f - 0.18f * static_cast<float>(j);
      if (since < 0.0f || since > 1.1f) continue;
      const float r = 0.4f + 1.3f * easeOut(0.0f, 1.1f, since);
      out.rings.push_back(water(flatOn(t.feet + kUp * 0.02f, glm::vec3(r, 0.4f, r)), 0.65f * (1.0f - ease(0.35f, 1.1f, since)), rnd(seed_, h, 91 + static_cast<uint32_t>(j)),
                                0.05f, 0.0f, 0.5f));
    }
    if (a < 0.7f) lights.push_back({t.feet + kUp * 0.9f, kWaterLight, 5.0f * std::exp(-a * 5.0f) + 0.6f, 3.6f});
  }
  // The end of its run: the wall breaks and falls in one last spray along its whole width.
  if (now >= runEnd) {
    const float half = 0.9f + reach_ * spread_;
    for (uint32_t j = 0; j < 26; ++j) {
      glm::vec3 from = feet_ + direction_ * reach_ + right * ((rnd(seed_, j, 101) * 2.0f - 1.0f) * half);
      const float ground = groundNear(from);
      from.y = ground + 1.2f;
      const glm::vec3 v = direction_ * (1.0f + 2.0f * rnd(seed_, j, 102)) + kUp * (1.5f + 2.2f * rnd(seed_, j, 103)) + right * (rnd(seed_, j, 104) - 0.5f) * 1.5f;
      drop(out, now, from, v, runEnd, ground, 0.035f + 0.035f * rnd(seed_, j, 105), rnd(seed_, j, 106));
    }
  }
  const uint32_t sphere = frame.addVertices(sphereGeometry()), ring = frame.addVertices(torusGeometry()), crown = frame.addVertices(crownGeometry()),
                 wave = frame.addVertices(waveGeometry());
  const glm::vec3 sight = feet_ + direction_ * std::min(firstAt_, reach_);
  draw(frame, now, ring, torusGeometry().size(), out.rings, sight);
  draw(frame, now, crown, crownGeometry().size(), out.crowns, sight);
  draw(frame, now, wave, waveGeometry().size(), out.waves, sight);
  draw(frame, now, sphere, sphereGeometry().size(), out.spheres, sight);
}

}  // namespace ember
