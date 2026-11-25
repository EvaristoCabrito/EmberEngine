#include "render/vfx/StormBolt.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPi;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
// A unit's card is drawn 1.2 units nearer in depth: what happens on a body is pulled toward the eye to show over it (CombatFx).
constexpr float kPull = 1.3f;
// The Combat material's modes (shaders/vfx.hlsl).
constexpr float kStreak = 0.0f, kRing = 3.0f, kLightning = 6.0f;

// The strike's clock, in seconds before (-) and after the return stroke.
constexpr float kForewarn = 0.5f;       // the body's warning starts
constexpr float kLeader = 0.14f;        // the stepped leader starts down
constexpr float kSteps = 7.0f;          // ... in this many jumps
constexpr float kShapeSeconds = 0.05f;  // the arcs take a new shape (3 frames)
constexpr float kThrow = 0.22f;         // the caster throws the charge up, this long before the first return stroke

constexpr glm::vec3 kLight{0.72f, 0.84f, 1.0f};    // the light it throws
constexpr glm::vec3 kEnergy{0.42f, 0.6f, 1.0f};    // the glow round the bolt: blue leaning to violet
constexpr glm::vec3 kViolet{0.5f, 0.36f, 1.0f};    // the trace it leaves
constexpr glm::vec3 kCore{0.97f, 0.98f, 1.0f};     // its white-hot thread
constexpr glm::vec3 kEmber{1.0f, 0.45f, 0.15f};    // the struck ground, still hot

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
glm::vec3 sphereDir(float a, float b) {
  const float z = 2.0f * a - 1.0f, r = std::sqrt(std::max(0.0f, 1.0f - z * z)), phi = b * kTau;
  return {r * std::cos(phi), z, r * std::sin(phi)};
}
glm::vec3 pulled(glm::vec3 p, glm::vec3 eye, float by) {
  const glm::vec3 d = eye - p;
  const float len = glm::length(d);
  return len > by + 0.5f ? p + d * (by / len) : p;
}
glm::vec3 unit(glm::vec3 v, glm::vec3 fallback) {
  const float l2 = glm::dot(v, v);
  return l2 > 1e-10f ? v / std::sqrt(l2) : fallback;
}

const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, -0.5f, 0.0f}, 1.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f},
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f}, {{-0.5f, 0.5f, 0.0f}, 0.0f, {0, 0, 1}, 1.0f}};
  return g;
}
// A flat disc of radius 1 on y 0 (u around, v out), facing up.
const std::vector<VfxVertex>& discGeometry() {
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

// One draw of the Combat material (params0 = time, mode, intensity, opacity).
struct Draw {
  VfxFrame& frame;
  float time;
  void operator()(uint32_t first, size_t count, const glm::mat4& model, float mode, float intensity, float opacity, glm::vec3 color, glm::vec4 params1,
                  glm::vec3 sight, const std::vector<VfxInstance>* list = nullptr) const {
    if (opacity <= 0.004f || count == 0) return;
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {time, mode, intensity, opacity};
    d.gpu.params1 = params1;
    d.gpu.color = glm::vec4(color, 0.0f);
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    if (list) {
      if (list->empty()) return;
      d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), list->begin(), list->end());
      d.instances = static_cast<uint32_t>(list->size());
    }
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
    d.gpu.sightAt = sight;
    d.blend = VfxBlend::Additive;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
};
// A Combat streak (a spark along its motion, or a soft round dot when `axis` is shorter than its width).
VfxInstance streak(glm::vec3 centre, glm::vec3 axis, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(axis, alpha), glm::vec4(tint, intensity)};
}

// Midpoint displacement: the line from `a` to `b` is split at its middle and the middle pushed aside at random, then each half the
// same, `levels` times, the push (rough x the line's length, at first) halved at every level: big bends carrying small ones. Fixed by
// the seed. `flat`: pushed sideways over the ground only (an arc crawling on the floor).
std::vector<glm::vec3> fractalLine(glm::vec3 a, glm::vec3 b, int levels, float rough, uint32_t seed, bool flat = false) {
  std::vector<glm::vec3> pts{a, b};
  const float length = glm::length(b - a);
  if (length < 1e-4f) return pts;
  const glm::vec3 d = (b - a) / length;
  const glm::vec3 e1 = glm::normalize(glm::cross(d, std::abs(d.y) < 0.9f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f)));
  const glm::vec3 e2 = flat ? glm::vec3(0.0f) : glm::cross(d, e1);
  float push = rough * length;
  for (int l = 0; l < levels; ++l) {
    std::vector<glm::vec3> next;
    next.reserve(pts.size() * 2);
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
      const uint32_t key = static_cast<uint32_t>(l) * 4099u + static_cast<uint32_t>(i);
      next.push_back(pts[i]);
      next.push_back((pts[i] + pts[i + 1]) * 0.5f + e1 * ((hash01(seed, key, 95) - 0.5f) * 2.0f * push) + e2 * ((hash01(seed, key, 96) - 0.5f) * 2.0f * push));
    }
    next.push_back(pts.back());
    pts.swap(next);
    push *= 0.5f;
  }
  return pts;
}

// What a ribbon is drawn from: points, how far along each lies (the shader's u), its width there.
struct Line {
  std::vector<glm::vec3> points;
  std::vector<float> along, width;
};
// A loose arc: u by its own length, its width from w0 at its start to w1 at its end.
Line arcLine(std::vector<glm::vec3> points, float w0, float w1) {
  Line line;
  const size_t n = points.size();
  line.along.resize(n, 0.0f), line.width.resize(n, w0);
  for (size_t i = 1; i < n; ++i) line.along[i] = line.along[i - 1] + glm::length(points[i] - points[i - 1]);
  const float total = std::max(line.along.back(), 1e-4f);
  for (size_t i = 0; i < n; ++i) line.along[i] /= total, line.width[i] = glm::mix(w0, w1, line.along[i]);
  line.points = std::move(points);
  return line;
}

// A ribbon along a polyline, `radius` x its width to each side, turned to face the eye at every point, its joints mitred so the kinks
// stay sharp and whole. Five columns across, their normals fanned from one edge over the middle to the other: the lightning shader
// (which reads a tube: white-hot where the surface faces the eye, the glow at its rim) draws it as a round bolt. u = along, v across.
// Every point pulled toward the eye by `pull` (over the cards; 0 = where it is).
void ribbon(std::vector<VfxVertex>& out, const std::vector<glm::vec3>& points, const std::vector<float>& along, const std::vector<float>& width, float radius,
            glm::vec3 eye, float pull) {
  const size_t n = points.size();
  if (n < 2) return;
  static constexpr float kAcross[5] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
  std::array<VfxVertex, 5> last{}, row{};
  glm::vec3 side{1.0f, 0.0f, 0.0f};
  for (size_t i = 0; i < n; ++i) {
    const glm::vec3 at = pull > 0.0f ? pulled(points[i], eye, pull) : points[i];
    const glm::vec3 in = unit(i > 0 ? points[i] - points[i - 1] : points[1] - points[0], kUp);
    const glm::vec3 on = unit(i + 1 < n ? points[i + 1] - points[i] : points[i] - points[i - 1], in);
    const glm::vec3 tangent = unit(in + on, in);
    const glm::vec3 toEye = unit(eye - at, kUp);
    side = unit(glm::cross(tangent, toEye), side);
    const float mitre = 1.0f / std::max(std::sqrt(std::max(0.5f * (1.0f + glm::dot(in, on)), 0.0f)), 0.5f);
    const float half = radius * width[i] * mitre;
    for (int c = 0; c < 5; ++c) {
      const float x = kAcross[c], turn = x * kPi * 0.5f;
      row[c] = VfxVertex{at + side * (x * half), along[i], glm::normalize(toEye * std::cos(turn) + side * std::sin(turn)), (x + 1.0f) * 0.5f};
    }
    if (i > 0)
      for (int c = 0; c < 4; ++c) out.insert(out.end(), {last[c], row[c], last[c + 1], last[c + 1], row[c], row[c + 1]});
    last = row;
  }
}
void ribbon(std::vector<VfxVertex>& out, const Line& line, float radius, glm::vec3 eye, float pull) {
  ribbon(out, line.points, line.along, line.width, radius, eye, pull);
}

// The point of a line at `along` (its u).
glm::vec3 pointAt(const std::vector<glm::vec3>& points, const std::vector<float>& along, float at) {
  for (size_t i = 1; i < points.size(); ++i)
    if (along[i] >= at) return glm::mix(points[i - 1], points[i], std::clamp((at - along[i - 1]) / std::max(along[i] - along[i - 1], 1e-5f), 0.0f, 1.0f));
  return points.back();
}

// How the bolt burns `age` after the return stroke: the trunk's brightness, the forks', and how swollen the channel is. Switched,
// never faded: the return stroke (the whole tree), then three re-flashes down the trunk (the first still catches the forks, faintly),
// the channel a faint thread in the dark between them.
struct Flash {
  float trunk = 0.0f, forks = 0.0f, swell = 1.0f;
  bool on() const { return trunk > 0.5f; }
};
Flash flashAt(float age) {
  if (age < 0.0f) return {};
  if (age < 0.05f) return {1.7f, 1.0f, 1.4f};
  if (age < 0.085f) return {0.07f, 0.0f, 0.8f};
  if (age < 0.115f) return {1.25f, 0.3f, 1.15f};
  if (age < 0.15f) return {0.05f, 0.0f, 0.8f};
  if (age < 0.175f) return {1.0f, 0.0f, 1.0f};
  if (age < 0.225f) return {0.04f, 0.0f, 0.8f};
  if (age < 0.25f) return {0.75f, 0.0f, 0.9f};
  return {};
}
constexpr float kFlashEnd = 0.25f;

// A puff of smoke: one volume of grey gas, lit by the scene (GasVolume's puff, as vfx/PoisonFx draws its own).
void smoke(VfxFrame& frame, uint32_t quad, float time, glm::vec3 origin, float radius, float thick, float fade, float seed) {
  if (fade >= 0.995f || radius <= 0.02f) return;
  VfxDraw d;
  d.gpu.model = glm::mat4(0.0f);
  d.gpu.model[1] = glm::vec4(0.3f, 0.3f, 0.33f, 0.0f), d.gpu.model[2] = glm::vec4(kLight, 0.0f);
  d.gpu.params0 = {time, thick, 0.0f, radius};
  d.gpu.params1 = {origin, seed};
  d.gpu.color = {1.0f, std::max(fade, 0.0f), 0.3f, 0.0f};
  d.gpu.firstVertex = quad, d.gpu.vertexCount = 6, d.gpu.firstInstance = 0xFFFFFFFFu;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::GasVolume);
  d.gpu.sightAt = origin;
  d.blend = VfxBlend::Alpha;
  d.depthTest = false;  // the shader stops its ray at the scene's depth itself
  frame.draws.push_back(d);
}

}  // namespace

StormBolt::StormBolt(glm::vec3 casterFeet, float casterHeight, std::vector<Target> targets, float start, float impact, float scale, uint32_t seed)
    : feet_(casterFeet), casterHeight_(std::clamp(casterHeight, 1.0f, 4.0f)), scale_(scale), start_(start), impact_(std::max(impact, start + 0.1f)), seed_(seed) {
  hand_ = casterFeet + kUp * (casterHeight_ * 0.7f);
  const float k = scale_;
  // What the caster throws up: a thin bolt from the hand to the sky, thinning as it climbs.
  {
    const glm::vec3 lean{(hash01(seed_, 1, 70) - 0.5f) * 2.4f, 0.0f, (hash01(seed_, 2, 70) - 0.5f) * 2.4f};
    const Line line = arcLine(fractalLine(hand_, hand_ + kUp * (9.0f * k) + lean, 5, 0.07f, seed_ + 0x51u), 1.0f, 0.3f);
    thrown_.points = line.points, thrown_.along = line.along, thrown_.width = line.width;
  }
  // One tree per struck body, grown once: every flash is on the very same path.
  strikes_.reserve(targets.size());
  for (size_t i = 0; i < targets.size(); ++i) {
    Strike s;
    s.seed = seed_ + static_cast<uint32_t>(i + 1) * 0x9e37u;
    s.height = std::clamp(targets[i].height, 1.0f, 4.0f);
    s.feet = targets[i].feet;
    s.body = s.feet + kUp * (s.height * 0.5f);
    s.head = s.feet + kUp * (s.height * 0.9f);
    const float side = (hash01(s.seed, 1, 1) - 0.5f) * 4.0f;
    const glm::vec3 lean = glm::normalize(glm::vec3(hash01(s.seed, 2, 1) - 0.5f, 0.0f, hash01(s.seed, 3, 1) - 0.5f) + glm::vec3(1e-3f)) * side;
    const glm::vec3 top = s.feet + kUp * ((7.5f + 2.0f * hash01(s.seed, 4, 1)) * k + s.height) + lean;
    grow(s, top, s.head, 6, 0, 0.0f, 0.0f, s.seed);
    strikes_.push_back(std::move(s));
  }
}

// One limb from `from` to `to` and what forks off it. A fork has two levels fewer than the limb it leaves (the tree ends), leaves at
// 22 to 52 degrees off the limb's own way (so the trunk's forks reach down and out), runs a part of what the limb still had to go,
// and is told where along the trunk it left: the leader coming down lights it as it passes.
void StormBolt::grow(Strike& strike, glm::vec3 from, glm::vec3 to, int levels, int generation, float alongStart, float trunkLength, uint32_t seed) const {
  Limb limb;
  limb.generation = generation;
  limb.points = fractalLine(from, to, levels, generation == 0 ? 0.13f : 0.17f, seed);
  const size_t n = limb.points.size();
  std::vector<float> run(n, 0.0f);
  for (size_t i = 1; i < n; ++i) run[i] = run[i - 1] + glm::length(limb.points[i] - limb.points[i - 1]);
  const float total = std::max(run.back(), 1e-4f);
  const float trunk = generation == 0 ? total : trunkLength;
  limb.along.resize(n), limb.width.resize(n);
  for (size_t i = 0; i < n; ++i) {
    const float s = run[i] / total;
    limb.along[i] = alongStart + run[i] / trunk;
    limb.width[i] = generation == 0 ? glm::mix(1.0f, 0.6f, s) : std::pow(1.0f - s, 0.65f);  // the trunk thins toward its foot; a fork ends in a point
  }
  const std::vector<glm::vec3> points = limb.points;
  strike.limbs.push_back(std::move(limb));

  const int forks = generation == 0 ? 5 : generation == 1 ? 2 : 0;
  if (forks == 0 || levels - 2 < 2 || n < 3) return;
  const glm::vec3 d = unit(to - from, -kUp);
  const glm::vec3 e1 = glm::normalize(glm::cross(d, std::abs(d.y) < 0.9f ? kUp : glm::vec3(1.0f, 0.0f, 0.0f))), e2 = glm::cross(d, e1);
  for (int f = 0; f < forks; ++f) {
    const uint32_t key = static_cast<uint32_t>(f);
    const float where = (generation == 0 ? 0.1f : 0.25f) + (generation == 0 ? 0.7f : 0.5f) * (static_cast<float>(f) + hash01(seed, key, 21)) / static_cast<float>(forks);
    const size_t index = std::clamp(static_cast<size_t>(std::lround(where * static_cast<float>(n - 1))), size_t{1}, n - 2);
    const float off = glm::radians(22.0f + 30.0f * hash01(seed, key, 22)), spin = hash01(seed, key, 23) * kTau;
    const glm::vec3 way = glm::normalize(d * std::cos(off) + (e1 * std::cos(spin) + e2 * std::sin(spin)) * std::sin(off));
    float reach = (total - run[index]) * (0.35f + 0.35f * hash01(seed, key, 24));
    if (generation == 0) reach = std::min(reach, 4.5f * scale_);
    grow(strike, points[index], points[index] + way * reach, levels - 2, generation + 1, alongStart + run[index] / trunk, trunk,
         seed * 31u + key * 977u + static_cast<uint32_t>(generation) * 7919u + 1u);
  }
}

void StormBolt::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  buildCaster(now, camera, frame, lights);
  for (size_t i = 0; i < strikes_.size(); ++i) buildStrike(strikes_[i], now - (impact_ + kStagger * static_cast<float>(i)), now, camera, frame, lights);
}

// The cast: the charge drawn in to the hand, then thrown up to the sky.
void StormBolt::buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float thrownAt = impact_ - kThrow, release = impact_ + 0.1f;
  if (now >= release) return;
  const Draw draw{frame, now};
  const glm::vec3 eye = camera.position;
  const float k = std::clamp(casterHeight_ / 2.0f, 0.7f, 1.4f) * scale_;
  const float f = ease(start_, std::max(thrownAt, start_ + 0.05f), now), fade = 1.0f - ease(impact_, release, now);
  const uint32_t quad = frame.addVertices(quadGeometry()), disc = frame.addVertices(discGeometry());
  std::vector<VfxInstance> streaks;
  const glm::vec3 hand = pulled(hand_, eye, kPull);
  const uint32_t shape = static_cast<uint32_t>(std::max(0.0f, (now - start_) / kShapeSeconds));

  // a ring on the ground closing in under the caster as the charge gathers
  draw(disc, discGeometry().size(), glm::translate(glm::mat4(1.0f), feet_ + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3((1.25f - 0.6f * f) * k)),
       kRing, 1.4f, (0.25f + 0.75f * f) * fade, kEnergy, glm::vec4(kCore, 0.86f), feet_);
  // the hand: a flare that jumps, motes of charge drawn in to it
  streaks.push_back(streak(hand, glm::vec3(0.0f), (0.22f + 0.5f * f) * k * (0.7f + 0.3f * hash01(seed_, shape, 5)), (0.45f + 0.55f * f) * fade, kCore, 2.6f));
  for (uint32_t i = 0; i < 12; ++i) {
    const float life = 0.35f + 0.25f * hash01(seed_, i, 6);
    const float t = std::fmod(now - start_ + life * hash01(seed_, i, 7), life), u = t / life;
    const uint32_t spin = i + 31u * static_cast<uint32_t>((now - start_ + life * hash01(seed_, i, 7)) / life);
    const glm::vec3 d = sphereDir(hash01(seed_, spin, 8), hash01(seed_, spin, 9));
    const float far = (0.9f + 0.6f * hash01(seed_, spin, 10)) * k * (1.0f - u) * (1.0f - u);
    streaks.push_back(streak(hand + d * (far + 0.05f * k), -d * (0.12f * k * (1.0f - u) + 0.01f), 0.02f * k, u * f * fade, kEnergy, 2.4f));
  }
  // arcs: three jumping off the hand, one from the hand down to the ground, a new shape every 0.05 s
  {
    std::vector<VfxVertex> v;
    for (uint32_t a = 0; a < 3; ++a) {
      const uint32_t key = a + shape * 3u;
      const glm::vec3 d = glm::normalize(sphereDir(hash01(seed_, key, 11), hash01(seed_, key, 12)) + kUp * 0.4f);
      ribbon(v, arcLine(fractalLine(hand_, hand_ + d * (0.45f + 0.55f * f) * k, 3, 0.2f, seed_ + key * 131u), 1.0f, 0.0f), 0.012f * k, eye, kPull);
    }
    const float spin = hash01(seed_, shape, 13) * kTau;
    const glm::vec3 ground = feet_ + glm::vec3(std::cos(spin), 0.0f, std::sin(spin)) * (0.5f * k) + kUp * 0.05f;
    ribbon(v, arcLine(fractalLine(hand_, ground, 4, 0.14f, seed_ + shape * 577u), 1.0f, 0.25f), 0.011f * k, eye, kPull);
    draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.2f, (0.35f + 0.65f * f) * fade, kEnergy, glm::vec4(kCore, 3.0f), hand_);
  }
  // the charge thrown up: a thin bolt climbing from the hand to the sky in a few hundredths of a second, held a moment, then its trace
  float thrown = 0.0f;
  if (now >= thrownAt && now < thrownAt + 0.2f) {
    const float age = now - thrownAt;
    std::vector<VfxVertex> v;
    if (age < 0.08f) {
      thrown = 1.0f;
      ribbon(v, thrown_.points, thrown_.along, thrown_.width, 0.05f * k, eye, kPull);
      const uint32_t first = frame.addVertices(v);
      draw(first, v.size(), glm::mat4(1.0f), kLightning, 1.6f, 0.4f, kEnergy, glm::vec4(kCore, age / 0.035f), hand_);
      v.clear();
      ribbon(v, thrown_.points, thrown_.along, thrown_.width, 0.02f * k, eye, kPull);
      draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 4.6f, 1.0f, kEnergy, glm::vec4(kCore, age / 0.035f), hand_);
      streaks.push_back(streak(hand, glm::vec3(0.0f), 1.1f * k, 0.9f, kCore, 3.0f));
    } else {
      ribbon(v, thrown_.points, thrown_.along, thrown_.width, 0.03f * k, eye, kPull);
      draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 0.9f, 0.4f * (1.0f - ease(0.08f, 0.2f, age)), kViolet, glm::vec4(kViolet, 3.0f), hand_);
    }
  }
  draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(kCore, 0.0f), hand_, &streaks);
  lights.push_back({hand_, kLight, ((4.0f + 12.0f * f) * (0.75f + 0.25f * hash01(seed_, shape, 14)) + 26.0f * thrown) * fade, 3.5f * k + 3.0f * thrown});
}

// One bolt on one body, `age` seconds after its return stroke (negative: before it).
void StormBolt::buildStrike(const Strike& s, float age, float time, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (age < -kForewarn || age >= 1.0f || s.limbs.empty()) return;
  const Draw draw{frame, time};
  const glm::vec3 eye = camera.position;
  const float k = scale_;
  const Limb& trunk = s.limbs.front();
  const uint32_t shape = static_cast<uint32_t>((age + kForewarn) / kShapeSeconds);
  const uint32_t quad = frame.addVertices(quadGeometry()), disc = frame.addVertices(discGeometry());
  std::vector<VfxInstance> streaks;
  // One generation of the tree as one draw. `lit`: how far down the trunk's length the tree is lit (3 = all of it).
  const auto tree = [&](int generation, float radius, float intensity, float opacity, glm::vec3 color, glm::vec3 core, float lit) {
    if (opacity <= 0.004f) return;
    std::vector<VfxVertex> v;
    for (const Limb& l : s.limbs)
      if (l.generation == generation) ribbon(v, l.points, l.along, l.width, radius, eye, kPull);
    draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, intensity, opacity, color, glm::vec4(core, lit), s.body);
  };
  const auto ground = [&](float radius, float intensity, float opacity, glm::vec3 color, glm::vec3 core, float ringAt) {
    draw(disc, discGeometry().size(), glm::translate(glm::mat4(1.0f), s.feet + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(radius)), kRing,
         intensity, opacity, color, glm::vec4(core, ringAt), s.feet);
  };

  if (age < 0.0f) {
    // ---- before the stroke: the body knows it first ----
    const float gather = ease(-kForewarn, -kLeader, age);
    ground((1.7f - 1.25f * gather) * k, 1.3f, 0.15f + 0.6f * gather, kEnergy, kCore, 0.86f);
    // streamers reaching up off its head, longer as the charge builds
    std::vector<VfxVertex> v;
    for (uint32_t a = 0; a < 3; ++a) {
      const uint32_t key = a + shape * 3u;
      const float reach = (0.25f + 1.0f * gather) * (0.6f + 0.4f * hash01(s.seed, key, 51)) * k;
      const glm::vec3 from = s.head + glm::vec3((hash01(s.seed, key, 52) - 0.5f) * 0.3f, 0.0f, (hash01(s.seed, key, 53) - 0.5f) * 0.3f) * k;
      const glm::vec3 lean{(hash01(s.seed, key, 54) - 0.5f) * 0.7f, 0.0f, (hash01(s.seed, key, 55) - 0.5f) * 0.7f};
      ribbon(v, arcLine(fractalLine(from, from + (kUp + lean) * reach, 3, 0.2f, s.seed + key * 131u), 1.0f, 0.0f), 0.011f * k, eye, kPull);
    }
    draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.0f, 0.45f + 0.45f * gather, kEnergy, glm::vec4(kCore, 3.0f), s.body);
    if (age < -kLeader) {
      // the pre-beam: the path marked by a faint thread, blinking
      if (age >= -0.36f) {
        const float blink = (static_cast<uint32_t>((age + 1.0f) / 0.03f) % 2u) ? 1.0f : 0.4f;
        tree(0, 0.008f * k, 2.0f, 0.3f * blink * ease(-0.36f, -0.3f, age), kEnergy, kCore, 3.0f);
      }
      lights.push_back({s.body, kLight, (1.5f + 4.0f * gather) * k * (0.7f + 0.3f * hash01(s.seed, shape, 56)), 4.0f * k});
    } else {
      // the stepped leader: down the tree in jumps, dim, a bright point at its tip
      const float step = std::floor((age + kLeader) / (kLeader / kSteps));
      const float lit = std::min((step + 1.0f) / kSteps, 1.0f);
      tree(0, 0.02f * k, 3.2f, 0.6f, kEnergy, kCore, lit * 1.03f);
      tree(1, 0.012f * k, 2.6f, 0.45f, kEnergy, kCore, lit * 1.03f);
      tree(2, 0.008f * k, 2.2f, 0.35f, kEnergy, kCore, lit * 1.03f);
      const glm::vec3 tip = pointAt(trunk.points, trunk.along, lit);
      streaks.push_back(streak(pulled(tip, eye, kPull), glm::vec3(0.0f), 0.42f * k, 0.9f, kCore, 3.0f));
      lights.push_back({tip, kLight, 10.0f * k, 5.0f * k});
      lights.push_back({s.body, kLight, 5.0f * k, 4.0f * k});
    }
    draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(kCore, 0.0f), s.body, &streaks);
    return;
  }

  // ---- the stroke ----
  const Flash flash = flashAt(age);
  const bool on = flash.on();
  if (flash.trunk > 0.0f) {
    const float seen = std::min(flash.trunk, 1.0f), hot = std::max(flash.trunk, 1.0f);
    // the trunk: a wide soft glow, a shell, a thin white-hot core; thick where it leaves the sky
    tree(0, 0.2f * k * flash.swell, 1.2f * hot, 0.16f * seen, kEnergy, kEnergy, 3.0f);
    tree(0, 0.085f * k * flash.swell, 1.9f * hot, 0.55f * seen, kEnergy, kCore, 3.0f);
    tree(0, 0.036f * k * flash.swell, 5.6f * hot, seen, kEnergy, kCore, 3.0f);
    // the forks and their twigs: thinner and dimmer at each generation
    if (flash.forks > 0.0f) {
      tree(1, 0.05f * k, 1.4f, 0.4f * flash.forks, kEnergy, kCore, 3.0f);
      tree(1, 0.018f * k, 3.8f, 0.85f * flash.forks, kEnergy, kCore, 3.0f);
      tree(2, 0.01f * k, 2.8f, 0.65f * flash.forks, kEnergy, kCore, 3.0f);
    }
    // the glow hanging round the channel while it burns, soft
    if (on)
      for (size_t i = 2; i < trunk.points.size(); i += 4)
        streaks.push_back(streak(pulled(trunk.points[i], eye, kPull), glm::vec3(0.0f), 0.6f * k * flash.swell, 0.2f * seen, kEnergy, 1.6f));
  }
  // the afterglow: a violet trace of the path, thinning out
  if (age >= kFlashEnd && age < 0.6f) {
    const float gone = ease(kFlashEnd, 0.6f, age);
    tree(0, glm::mix(0.07f, 0.03f, gone) * k, 0.9f, 0.5f * (1.0f - gone), kViolet, glm::vec3(0.66f, 0.56f, 1.0f), 3.0f);
  }
  // arcs over the body: many while a flash is on, a few still crawling after
  if (age < 0.6f) {
    const uint32_t count = on ? 6u : 3u;
    std::vector<VfxVertex> v;
    for (uint32_t a = 0; a < count; ++a) {
      const uint32_t key = a + shape * 6u;
      const float a0 = hash01(s.seed, key, 31) * kTau, a1 = a0 + 0.6f + 1.2f * hash01(s.seed, key, 32);
      const float y0 = (hash01(s.seed, key, 33) - 0.5f) * 0.8f, y1 = std::clamp(y0 + (hash01(s.seed, key, 34) - 0.5f) * 0.9f, -0.45f, 0.45f);
      const glm::vec3 p0 = s.body + glm::vec3(std::cos(a0) * 0.3f * k, y0 * s.height, std::sin(a0) * 0.3f * k);
      const glm::vec3 p1 = s.body + glm::vec3(std::cos(a1) * 0.42f * k, y1 * s.height, std::sin(a1) * 0.42f * k);
      ribbon(v, arcLine(fractalLine(p0, p1, 3, 0.22f, s.seed + key * 53u), 1.0f, 0.3f), 0.011f * k, eye, kPull);
    }
    draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.4f, on ? 0.9f : 0.4f * (1.0f - ease(kFlashEnd, 0.6f, age)), kEnergy, glm::vec4(kCore, 3.0f),
         s.body);
  }
  // arcs crawling out over the ground from the feet
  if (age < 0.3f) {
    std::vector<VfxVertex> v;
    for (uint32_t a = 0; a < 7; ++a) {
      const uint32_t key = a + shape * 7u;
      const float spin = (static_cast<float>(a) + hash01(s.seed, key, 35)) / 7.0f * kTau, reach = (1.1f + 1.3f * hash01(s.seed, key, 36)) * k;
      const glm::vec3 from = s.feet + kUp * 0.06f;
      ribbon(v, arcLine(fractalLine(from, from + glm::vec3(std::cos(spin), 0.0f, std::sin(spin)) * reach, 4, 0.16f, s.seed + key * 71u, true), 1.0f, 0.0f), 0.013f * k,
             eye, 0.0f);
    }
    draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.2f, on ? 0.9f : 0.28f, kEnergy, glm::vec4(kCore, 0.15f + age / 0.07f), s.feet);
  }
  // the flash where it struck
  if (on) streaks.push_back(streak(pulled(s.body, eye, kPull), glm::vec3(0.0f), 1.4f * k * flash.trunk, std::min(flash.trunk, 1.0f), kCore, 3.2f));
  // two rings racing out over the ground: the return stroke's, then the first re-flash's
  if (age < 0.45f) ground((0.3f + 2.6f * easeOut(0.0f, 0.35f, age)) * k, 2.6f, 1.0f - ease(0.08f, 0.45f, age), kEnergy, kCore, 0.9f);
  if (age >= 0.085f && age < 0.5f) ground((0.3f + 1.6f * easeOut(0.085f, 0.4f, age)) * k, 2.0f, 0.7f * (1.0f - ease(0.15f, 0.5f, age)), kViolet, kCore, 0.88f);
  // the ground it struck, left glowing
  if (age >= 0.05f) ground(0.9f * k, 1.8f, 0.9f * (1.0f - ease(kFlashEnd, 1.0f, age)), kEmber, glm::vec3(1.0f, 0.85f, 0.6f), 0.0f);
  // sparks thrown up from the feet: they rise, fall and bounce once
  for (uint32_t i = 0; i < 40; ++i) {
    const float life = 0.4f + 0.35f * hash01(s.seed, i, 41);
    if (age > life) continue;
    glm::vec3 d = sphereDir(hash01(s.seed, i, 43), hash01(s.seed, i, 44));
    d.y = std::abs(d.y) * 0.9f + 0.25f;
    d = glm::normalize(d);
    const float speed = (2.2f + 3.2f * hash01(s.seed, i, 42)) * k, g = 9.0f;
    const glm::vec3 v0 = d * speed, from = s.feet + kUp * (0.1f + 0.5f * s.height * hash01(s.seed, i, 45));
    const float floorY = s.feet.y + 0.03f, h = from.y - floorY;
    const float land = (v0.y + std::sqrt(v0.y * v0.y + 2.0f * g * h)) / g;  // when it reaches the ground
    glm::vec3 p, vel;
    if (age < land) {
      p = from + v0 * age - kUp * (0.5f * g * age * age);
      vel = v0 - kUp * (g * age);
    } else {
      const float t = age - land, up = (g * land - v0.y) * 0.35f;
      p = glm::vec3(from.x + v0.x * (land + t * 0.6f), std::max(floorY + up * t - 0.5f * g * t * t, floorY), from.z + v0.z * (land + t * 0.6f));
      vel = glm::vec3(v0.x * 0.6f, up - g * t, v0.z * 0.6f);
    }
    streaks.push_back(streak(pulled(p, eye, kPull), vel * 0.03f, 0.026f * k, 1.0f - age / life, i % 3 ? kEnergy : kCore, 2.6f));
  }
  draw(quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(kCore, 0.0f), s.body, &streaks);
  // smoke off the struck ground
  if (age >= 0.05f) {
    const float t = age - 0.05f;
    smoke(frame, quad, time, s.feet + kUp * (0.15f + 0.5f * t), (0.35f + 0.55f * easeOut(0.0f, 0.6f, t)) * k, 1.1f, ease(0.35f, 0.95f, t), hash01(s.seed, 9, 9) * 30.0f);
  }
  // the light: it spikes with every flash and lights the whole area for that moment; between the flashes almost none; then the
  // trace's violet and the hot ground's glow die away
  if (age < kFlashEnd) {
    lights.push_back({s.body, kLight, (110.0f * flash.trunk * (on ? 1.0f : 0.5f) + 2.0f) * k, 12.0f * k});
    lights.push_back({s.body + kUp * (3.0f * k), kLight, 34.0f * k * (on ? flash.trunk : 0.0f), 8.0f * k});
  } else if (age < 0.6f) {
    lights.push_back({s.body, glm::vec3(0.6f, 0.5f, 1.0f), 5.0f * k * (1.0f - ease(kFlashEnd, 0.6f, age)), 5.0f * k});
  }
  lights.push_back({s.feet + kUp * 0.3f, kEmber, 2.5f * k * (1.0f - ease(kFlashEnd, 1.0f, age)), 2.5f * k});
}

}  // namespace ember
