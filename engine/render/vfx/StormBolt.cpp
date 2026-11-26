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
// Ira dos Céus: every bolt struck a second time down the same channel, kSecond after the first; the channel a faint thread between.
constexpr float kSecond = 0.4f;
Flash flashTwice(float age) {
  if (age < kFlashEnd) return flashAt(age);
  if (age < kSecond) return {0.05f, 0.0f, 0.8f};
  Flash f = flashAt(age - kSecond);
  f.trunk *= 0.8f, f.forks *= 0.6f;
  return f;
}
// Cadeia de Raios: a jump's own burning, shorter (the stroke, two re-flashes).
constexpr float kJumpEnd = 0.175f;
Flash flashJump(float age) {
  if (age < 0.0f) return {};
  if (age < 0.05f) return {1.6f, 1.0f, 1.35f};
  if (age < 0.08f) return {0.07f, 0.0f, 0.8f};
  if (age < 0.11f) return {1.1f, 0.25f, 1.1f};
  if (age < 0.15f) return {0.05f, 0.0f, 0.8f};
  if (age < kJumpEnd) return {0.8f, 0.0f, 0.95f};
  return {};
}

// A puff of smoke: one volume of grey gas, lit by the scene (GasVolume's puff, as vfx/PoisonFx draws its own).
void smoke(VfxFrame& frame, uint32_t quad, float time, glm::vec3 origin, float radius, float thick, float fade, float seed, glm::vec3 tint = {0.3f, 0.3f, 0.33f},
           float glow = 0.3f) {
  if (fade >= 0.995f || radius <= 0.02f) return;
  VfxDraw d;
  d.gpu.model = glm::mat4(0.0f);
  d.gpu.model[1] = glm::vec4(tint, 0.0f), d.gpu.model[2] = glm::vec4(kLight, 0.0f);
  d.gpu.params0 = {time, thick, 0.0f, radius};
  d.gpu.params1 = {origin, seed};
  d.gpu.color = {1.0f, std::max(fade, 0.0f), glow, 0.0f};
  d.gpu.firstVertex = quad, d.gpu.vertexCount = 6, d.gpu.firstInstance = 0xFFFFFFFFu;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::GasVolume);
  d.gpu.sightAt = origin;
  d.blend = VfxBlend::Alpha;
  d.depthTest = false;  // the shader stops its ray at the scene's depth itself
  frame.draws.push_back(d);
}

// One limb from `from` to `to` and what forks off it, added to `limbs`. A fork has two levels fewer than the limb it leaves (the tree
// ends), leaves at 22 to 52 degrees off the limb's own way (so a falling trunk's forks reach down and out), runs a part of what the
// limb still had to go (`reachMost` at most, for the trunk's), and is told where along the trunk it left: the leader coming down
// lights it as it passes.
void growTree(std::vector<BoltLimb>& limbs, glm::vec3 from, glm::vec3 to, int levels, int generation, float alongStart, float trunkLength, uint32_t seed,
              float reachMost, int trunkForks = 5, float trunkRough = 0.13f) {
  BoltLimb limb;
  limb.generation = generation;
  limb.points = fractalLine(from, to, levels, generation == 0 ? trunkRough : 0.17f, seed);
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
  limbs.push_back(std::move(limb));

  const int forks = generation == 0 ? trunkForks : generation == 1 ? 2 : 0;
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
    if (generation == 0) reach = std::min(reach, reachMost);
    growTree(limbs, points[index], points[index] + way * reach, levels - 2, generation + 1, alongStart + run[index] / trunk, trunk,
             seed * 31u + key * 977u + static_cast<uint32_t>(generation) * 7919u + 1u, reachMost, trunkForks, trunkRough);
  }
}

// What every part of a bolt is drawn with this frame.
struct Pen {
  Draw draw;
  VfxFrame& frame;
  std::vector<VfxLight>& lights;
  glm::vec3 eye;
  uint32_t quad, disc;
};

// One generation of a tree as one draw. `lit`: how far along the trunk's length the tree is lit (3 = all of it).
void drawTree(const Pen& pen, const std::vector<BoltLimb>& limbs, int generation, float radius, float intensity, float opacity, glm::vec3 color, glm::vec3 core,
              float lit, glm::vec3 sight) {
  if (opacity <= 0.004f) return;
  std::vector<VfxVertex> v;
  for (const BoltLimb& l : limbs)
    if (l.generation == generation) ribbon(v, l.points, l.along, l.width, radius, pen.eye, kPull);
  pen.draw(pen.frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, intensity, opacity, color, glm::vec4(core, lit), sight);
}
// The stepped leader: the tree lit down to `lit` of the trunk, dim, a bright point at its tip.
void drawLeader(const Pen& pen, const std::vector<BoltLimb>& limbs, float k, float lit, glm::vec3 sight, std::vector<VfxInstance>& streaks) {
  drawTree(pen, limbs, 0, 0.02f * k, 3.2f, 0.6f, kEnergy, kCore, lit * 1.03f, sight);
  drawTree(pen, limbs, 1, 0.012f * k, 2.6f, 0.45f, kEnergy, kCore, lit * 1.03f, sight);
  drawTree(pen, limbs, 2, 0.008f * k, 2.2f, 0.35f, kEnergy, kCore, lit * 1.03f, sight);
  const glm::vec3 tip = pointAt(limbs.front().points, limbs.front().along, lit);
  streaks.push_back(streak(pulled(tip, pen.eye, kPull), glm::vec3(0.0f), 0.42f * k, 0.9f, kCore, 3.0f));
  pen.lights.push_back({tip, kLight, 10.0f * k, 5.0f * k});
}
// The tree burning: the trunk as a wide soft glow, a shell and a thin white-hot core, thick where it starts; the forks and their twigs,
// thinner and dimmer at each generation; a soft glow hanging round the channel.
void drawBurning(const Pen& pen, const std::vector<BoltLimb>& limbs, float k, const Flash& flash, glm::vec3 sight, std::vector<VfxInstance>& streaks) {
  if (flash.trunk <= 0.0f) return;
  const float seen = std::min(flash.trunk, 1.0f), hot = std::max(flash.trunk, 1.0f);
  drawTree(pen, limbs, 0, 0.16f * k * flash.swell, 1.0f * hot, 0.1f * seen, kEnergy, kEnergy, 3.0f, sight);
  drawTree(pen, limbs, 0, 0.085f * k * flash.swell, 1.9f * hot, 0.55f * seen, kEnergy, kCore, 3.0f, sight);
  drawTree(pen, limbs, 0, 0.036f * k * flash.swell, 5.6f * hot, seen, kEnergy, kCore, 3.0f, sight);
  if (flash.forks > 0.0f) {
    drawTree(pen, limbs, 1, 0.05f * k, 1.4f, 0.4f * flash.forks, kEnergy, kCore, 3.0f, sight);
    drawTree(pen, limbs, 1, 0.018f * k, 3.8f, 0.85f * flash.forks, kEnergy, kCore, 3.0f, sight);
    drawTree(pen, limbs, 2, 0.01f * k, 2.8f, 0.65f * flash.forks, kEnergy, kCore, 3.0f, sight);
  }
  if (flash.on()) {
    const std::vector<glm::vec3>& trunk = limbs.front().points;
    for (size_t i = 2; i < trunk.size(); i += 4) streaks.push_back(streak(pulled(trunk[i], pen.eye, kPull), glm::vec3(0.0f), 0.6f * k * flash.swell, 0.2f * seen, kEnergy, 1.6f));
  }
}
// The trace a bolt leaves: its trunk in violet, thinning out (`gone` 0..1).
void drawTrace(const Pen& pen, const std::vector<BoltLimb>& limbs, float k, float gone, glm::vec3 sight) {
  drawTree(pen, limbs, 0, glm::mix(0.07f, 0.03f, gone) * k, 0.9f, 0.5f * (1.0f - gone), kViolet, glm::vec3(0.66f, 0.56f, 1.0f), 3.0f, sight);
}
void drawGround(const Pen& pen, glm::vec3 feet, float radius, float intensity, float opacity, glm::vec3 color, glm::vec3 core, float ringAt) {
  pen.draw(pen.disc, discGeometry().size(), glm::translate(glm::mat4(1.0f), feet + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(radius)), kRing,
           intensity, opacity, color, glm::vec4(core, ringAt), feet);
}
// Arcs crawling over a body, `count` of them, a new shape with every `shape`.
void drawBodyArcs(const Pen& pen, glm::vec3 body, float height, float k, uint32_t seed, uint32_t shape, uint32_t count, float opacity) {
  if (opacity <= 0.004f) return;
  std::vector<VfxVertex> v;
  for (uint32_t a = 0; a < count; ++a) {
    const uint32_t key = a + shape * 6u;
    const float a0 = hash01(seed, key, 31) * kTau, a1 = a0 + 0.6f + 1.2f * hash01(seed, key, 32);
    const float y0 = (hash01(seed, key, 33) - 0.5f) * 0.8f, y1 = std::clamp(y0 + (hash01(seed, key, 34) - 0.5f) * 0.9f, -0.45f, 0.45f);
    const glm::vec3 p0 = body + glm::vec3(std::cos(a0) * 0.3f * k, y0 * height, std::sin(a0) * 0.3f * k);
    const glm::vec3 p1 = body + glm::vec3(std::cos(a1) * 0.42f * k, y1 * height, std::sin(a1) * 0.42f * k);
    ribbon(v, arcLine(fractalLine(p0, p1, 3, 0.22f, seed + key * 53u), 1.0f, 0.3f), 0.011f * k, pen.eye, kPull);
  }
  pen.draw(pen.frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.4f, opacity, kEnergy, glm::vec4(kCore, 3.0f), body);
}

// What a stroke does where it lands, `age` after it (`burnEnd`: when its last flash is over; `tail`: when all of it is gone): arcs
// over the body and crawling out over the ground, the flash, rings racing out (one more with a second stroke), the ground left
// glowing, sparks that rise, fall and bounce once, smoke, and the light: it spikes with every flash and lights the whole area for
// that moment, almost none between them, then the trace's violet and the hot ground's glow die away.
void drawStruck(const Pen& pen, glm::vec3 feet, glm::vec3 body, float height, bool stands, float k, uint32_t seed, uint32_t shape, float age, const Flash& flash,
                float burnEnd, float tail, uint32_t sparks, std::vector<VfxInstance>& streaks, float crawlFor = 0.35f, uint32_t arcs = 3u) {
  const bool on = flash.on();
  const float crawl = burnEnd + crawlFor;  // the arcs go on crawling over the body this long after the last flash (`arcs` of them)
  if (stands && age < crawl)
    drawBodyArcs(pen, body, height, k, seed, shape, on ? arcs * 2u : arcs, on ? 0.9f : (arcs > 3u ? 0.8f : 0.4f) * (1.0f - ease(burnEnd, crawl, age)));
  if (age < burnEnd + 0.05f) {
    std::vector<VfxVertex> v;
    for (uint32_t a = 0; a < 7; ++a) {
      const uint32_t key = a + shape * 7u;
      const float spin = (static_cast<float>(a) + hash01(seed, key, 35)) / 7.0f * kTau, reach = (1.1f + 1.3f * hash01(seed, key, 36)) * k;
      const glm::vec3 from = feet + kUp * 0.06f;
      ribbon(v, arcLine(fractalLine(from, from + glm::vec3(std::cos(spin), 0.0f, std::sin(spin)) * reach, 4, 0.16f, seed + key * 71u, true), 1.0f, 0.0f), 0.013f * k,
             pen.eye, 0.0f);
    }
    pen.draw(pen.frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.2f, on ? 0.9f : 0.28f, kEnergy, glm::vec4(kCore, 0.15f + age / 0.07f), feet);
  }
  // No ring on the ground, in any lightning, ever (the user, 2026-11-25).
  const glm::vec3 point = stands ? body : feet + kUp * 0.2f;
  if (on) streaks.push_back(streak(pulled(point, pen.eye, kPull), glm::vec3(0.0f), 0.7f * k * flash.trunk, 0.6f * std::min(flash.trunk, 1.0f), kCore, 2.4f));  // the look the user chose (2026-11-25): the struck body stays seen
  if (age >= 0.05f) drawGround(pen, feet, 0.9f * k, 1.8f, 0.9f * (1.0f - ease(burnEnd, tail, age)), kEmber, glm::vec3(1.0f, 0.85f, 0.6f), 0.0f);
  for (uint32_t i = 0; i < sparks; ++i) {
    const float life = 0.4f + 0.35f * hash01(seed, i, 41);
    if (age > life) continue;
    glm::vec3 d = sphereDir(hash01(seed, i, 43), hash01(seed, i, 44));
    d.y = std::abs(d.y) * 0.9f + 0.25f;
    d = glm::normalize(d);
    const float speed = (2.2f + 3.2f * hash01(seed, i, 42)) * k, g = 9.0f;
    const glm::vec3 v0 = d * speed, from = feet + kUp * (0.1f + (stands ? 0.5f * height * hash01(seed, i, 45) : 0.0f));
    const float floorY = feet.y + 0.03f, h = from.y - floorY;
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
    streaks.push_back(streak(pulled(p, pen.eye, kPull), vel * 0.03f, 0.026f * k, 1.0f - age / life, i % 3 ? kEnergy : kCore, 2.6f));
  }
  if (age >= 0.05f) {
    const float t = age - 0.05f;
    smoke(pen.frame, pen.quad, pen.draw.time, feet + kUp * (0.15f + 0.5f * t), (0.35f + 0.55f * easeOut(0.0f, 0.6f, t)) * k, 1.1f, ease(0.35f, 0.95f, t), hash01(seed, 9, 9) * 30.0f);
  }
  if (age < burnEnd) {
    pen.lights.push_back({point + kUp * (1.2f * k), kLight, (38.0f * flash.trunk * (on ? 1.0f : 0.5f) + 2.0f) * k, 12.0f * k});
    pen.lights.push_back({point + kUp * (3.5f * k), kLight, 22.0f * k * (on ? flash.trunk : 0.0f), 9.0f * k});
  } else if (age < crawl) {
    pen.lights.push_back({point, glm::vec3(0.6f, 0.5f, 1.0f), 5.0f * k * (1.0f - ease(burnEnd, crawl, age)), 5.0f * k});
  }
  pen.lights.push_back({feet + kUp * 0.3f, kEmber, 2.5f * k * (1.0f - ease(burnEnd, tail, age)), 2.5f * k});
}

// The charge drawn in to the caster's hand (`f` 0..1 as it gathers): a ring on the ground closing in under the caster, a flare at the
// hand that jumps, motes of charge drawn in to it, three arcs jumping off the hand (a new shape every
// 0.05 s), its light.
void drawGather(const Pen& pen, glm::vec3 feet, glm::vec3 handAt, float k, float f, float fade, float since, uint32_t seed, std::vector<VfxInstance>& streaks) {
  const glm::vec3 hand = pulled(handAt, pen.eye, kPull);
  const uint32_t shape = static_cast<uint32_t>(std::max(0.0f, since / kShapeSeconds));
  streaks.push_back(streak(hand, glm::vec3(0.0f), (0.22f + 0.5f * f) * k * (0.7f + 0.3f * hash01(seed, shape, 5)), (0.45f + 0.55f * f) * fade, kCore, 2.6f));
  for (uint32_t i = 0; i < 12; ++i) {
    const float life = 0.35f + 0.25f * hash01(seed, i, 6), clock = std::max(since, 0.0f) + life * hash01(seed, i, 7);
    const float u = std::fmod(clock, life) / life;
    const uint32_t turn = i + 31u * static_cast<uint32_t>(clock / life);
    const glm::vec3 d = sphereDir(hash01(seed, turn, 8), hash01(seed, turn, 9));
    const float far = (0.9f + 0.6f * hash01(seed, turn, 10)) * k * (1.0f - u) * (1.0f - u);
    streaks.push_back(streak(hand + d * (far + 0.05f * k), -d * (0.12f * k * (1.0f - u) + 0.01f), 0.02f * k, u * f * fade, kEnergy, 2.4f));
  }
  // Three short arcs jumping off the hand, a new shape every 0.05 s: as in the picture the user chose (2026-11-25). No line from
  // the hand down to the ground, no larger bolts here.
  std::vector<VfxVertex> v;
  for (uint32_t a = 0; a < 3; ++a) {
    const uint32_t key = a + shape * 3u;
    const glm::vec3 d = glm::normalize(sphereDir(hash01(seed, key, 11), hash01(seed, key, 12)) + kUp * 0.4f);
    ribbon(v, arcLine(fractalLine(handAt, handAt + d * (0.45f + 0.55f * f) * k, 3, 0.2f, seed + key * 131u), 1.0f, 0.0f), 0.012f * k, pen.eye, kPull);
  }
  pen.draw(pen.frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.2f, (0.35f + 0.65f * f) * fade, kEnergy, glm::vec4(kCore, 3.0f), handAt);
  pen.lights.push_back({handAt, kLight, (4.0f + 12.0f * f) * (0.75f + 0.25f * hash01(seed, shape, 14)) * fade, 3.5f * k});
}
void drawStreaks(const Pen& pen, const std::vector<VfxInstance>& streaks, glm::vec3 sight) {
  pen.draw(pen.quad, quadGeometry().size(), glm::mat4(1.0f), kStreak, 1.0f, 1.0f, glm::vec3(1.0f), glm::vec4(kCore, 0.0f), sight, &streaks);
}
Pen penFor(float time, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  return {Draw{frame, time}, frame, lights, camera.position, frame.addVertices(quadGeometry()), frame.addVertices(discGeometry())};
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------
// StormBolt: the bolts that fall from the sky.

StormBolt::StormBolt(Style style, glm::vec3 casterFeet, float casterHeight, const std::vector<Target>& targets, float start, float impact, float scale, uint32_t seed)
    : style_(style), feet_(casterFeet), casterHeight_(std::clamp(casterHeight, 1.0f, 4.0f)), scale_(scale), start_(start), impact_(std::max(impact, start + 0.1f)),
      seed_(seed) {
  hand_ = casterFeet + kUp * (casterHeight_ * 0.7f);
  const float k = scale_;
  const bool cloud = style_ == Style::Storm || style_ == Style::Wrath;
  tail_ = style_ == Style::Wrath ? 1.0f + kSecond : 1.0f;
  // What the caster throws up: a thin bolt from the hand to the sky, thinning as it climbs.
  {
    const glm::vec3 lean{(hash01(seed_, 1, 70) - 0.5f) * 2.4f, 0.0f, (hash01(seed_, 2, 70) - 0.5f) * 2.4f};
    const Line line = arcLine(fractalLine(hand_, hand_ + kUp * (9.0f * k) + lean, 5, 0.07f, seed_ + 0x51u), 1.0f, 0.3f);
    thrown_.points = line.points, thrown_.along = line.along, thrown_.width = line.width;
  }
  // The cloud: over the middle of what is struck, wide enough to cover it.
  if (cloud && !targets.empty()) {
    glm::vec3 middle{0.0f};
    for (const Target& t : targets) middle += t.feet;
    middle /= static_cast<float>(targets.size());
    float reach = 0.0f;
    for (const Target& t : targets) reach = std::max(reach, glm::length(t.feet - middle));
    cloudAt_ = middle + kUp * 5.6f;
    cloudRadius_ = reach + (style_ == Style::Wrath ? 3.0f : 2.2f);
  }
  // One tree per struck body, grown once: every flash is on the very same path. When each one falls: a beat apart in their order
  // (Relâmpago; Ira dos Céus, almost at once), or in no order (Tempestade Elétrica).
  strikes_.reserve(targets.size());
  last_ = impact_;
  for (size_t i = 0; i < targets.size(); ++i) {
    Strike s;
    s.seed = seed_ + static_cast<uint32_t>(i + 1) * 0x9e37u;
    s.stands = targets[i].stands;
    s.height = s.stands ? std::clamp(targets[i].height, 1.0f, 4.0f) : 0.0f;
    s.feet = targets[i].feet;
    s.body = s.feet + kUp * (s.stands ? s.height * 0.5f : 0.2f);
    s.head = s.feet + kUp * (s.stands ? s.height * 0.9f : 0.05f);
    const float side = (hash01(s.seed, 1, 1) - 0.5f) * (cloud ? 2.0f : 4.0f);
    const glm::vec3 lean = glm::normalize(glm::vec3(hash01(s.seed, 2, 1) - 0.5f, 0.0f, hash01(s.seed, 3, 1) - 0.5f) + glm::vec3(1e-3f)) * side;
    const bool shock = style_ == Style::Shock;
    s.top = cloud   ? glm::vec3(s.feet.x, cloudAt_.y - 0.3f + 0.6f * hash01(s.seed, 4, 1), s.feet.z) + lean
            : shock ? s.feet + kUp * (7.5f + 2.0f * hash01(s.seed, 4, 1) + s.height) + lean  // from as high in the sky as the Raio (not scaled down)
                    : s.feet + kUp * ((7.5f + 2.0f * hash01(s.seed, 4, 1)) * k + s.height) + lean;
    // The Raio: its own tree, untouched. Choque: from the same sky, never the same shape (the user, 2026-11-25): a straighter
    // bolt that wanders less, two short forks instead of five.
    if (shock) growTree(s.limbs, s.top, s.head, 6, 0, 0.0f, 0.0f, s.seed, 1.6f * k, 2, 0.07f);
    else growTree(s.limbs, s.top, s.head, 6, 0, 0.0f, 0.0f, s.seed, 4.5f * k);
    const float n = static_cast<float>(i);
    s.at = style_ == Style::Storm ? impact_ + (i == 0 ? 0.0f : 0.13f * n * (0.6f + 0.8f * hash01(seed_, static_cast<uint32_t>(i), 71)))
                                  : impact_ + (style_ == Style::Wrath ? 0.08f : 0.07f) * n;
    last_ = std::max(last_, s.at);
    strikes_.push_back(std::move(s));
  }
}

void StormBolt::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  if (style_ != Style::Echo) buildCaster(now, camera, frame, lights);
  if (cloudRadius_ > 0.0f) buildCloud(now, camera, frame, lights);
  for (const Strike& s : strikes_) buildStrike(s, now - s.at, now, camera, frame, lights);
}

// The cast: the charge drawn in to the hand, then thrown up to the sky.
void StormBolt::buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float thrownAt = impact_ - kThrow, release = impact_ + 0.1f;
  if (now >= release) return;
  const Pen pen = penFor(now, camera, frame, lights);
  const float k = std::clamp(casterHeight_ / 2.0f, 0.7f, 1.4f) * scale_;
  const float f = ease(start_, std::max(thrownAt, start_ + 0.05f), now), fade = 1.0f - ease(impact_, release, now);
  std::vector<VfxInstance> streaks;
  drawGather(pen, feet_, hand_, k, f, fade, now - start_, seed_, streaks);
  // the charge thrown up: a thin bolt climbing from the hand to the sky in a few hundredths of a second, held a moment, then its trace.
  // Choque: only the arcs gathering in the hand, nothing thrown up (the user, 2026-11-25: "só os raios gathering nas mãos deles sem
  // lançar ao céu"); its bolt falls from the sky onto the body.
  if (style_ != Style::Shock && now >= thrownAt && now < thrownAt + 0.2f) {
    const float age = now - thrownAt;
    const std::vector<BoltLimb> limbs{thrown_};
    if (age < 0.08f) {
      drawTree(pen, limbs, 0, 0.05f * k, 1.6f, 0.4f, kEnergy, kCore, age / 0.035f, hand_);
      drawTree(pen, limbs, 0, 0.02f * k, 4.6f, 1.0f, kEnergy, kCore, age / 0.035f, hand_);
      streaks.push_back(streak(pulled(hand_, pen.eye, kPull), glm::vec3(0.0f), 1.1f * k, 0.9f, kCore, 3.0f));
      lights.push_back({hand_, kLight, 26.0f * fade, 6.5f * k});
    } else {
      drawTree(pen, limbs, 0, 0.03f * k, 0.9f, 0.4f * (1.0f - ease(0.08f, 0.2f, age)), kViolet, kViolet, 3.0f, hand_);
    }
  }
  drawStreaks(pen, streaks, hand_);
}

// The cloud of Tempestade Elétrica and Ira dos Céus: dark gas gathering over the area from the cast on, lit from inside whenever one
// of its bolts burns, lightning running through it from one bolt's root to the next before they fall and between them; it thins out
// when the last bolt is spent.
void StormBolt::buildCloud(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const Pen pen = penFor(now, camera, frame, lights);
  const float gathered = ease(start_, std::max(impact_ - 0.15f, start_ + 0.1f), now), leaving = ease(last_ + tail_ - 0.6f, last_ + tail_, now);
  float burning = 0.0f;
  for (const Strike& s : strikes_) {
    const Flash f = style_ == Style::Wrath ? flashTwice(now - s.at) : flashAt(now - s.at);
    if (f.on()) burning = std::max(burning, f.trunk);
  }
  const float fade = 1.0f - 0.8f * gathered * (1.0f - leaving);
  const glm::vec3 dark{0.1f, 0.11f, 0.17f};
  smoke(frame, pen.quad, now, cloudAt_, cloudRadius_ * 0.62f, 1.7f, fade, hash01(seed_, 1, 80) * 30.0f, dark, 0.12f + 0.8f * std::min(burning, 1.0f));
  for (uint32_t i = 0; i < 6; ++i) {
    const float spin = (static_cast<float>(i) + 0.5f * hash01(seed_, i, 81)) / 6.0f * kTau + now * 0.05f;
    const glm::vec3 at = cloudAt_ + glm::vec3(std::cos(spin), 0.0f, std::sin(spin)) * (cloudRadius_ * 0.55f) + kUp * ((hash01(seed_, i, 82) - 0.5f) * 0.7f);
    smoke(frame, pen.quad, now, at, cloudRadius_ * (0.4f + 0.12f * hash01(seed_, i, 83)), 1.5f, fade, hash01(seed_, i, 84) * 30.0f, dark,
          0.1f + 0.7f * std::min(burning, 1.0f));
  }
  // lightning inside it: from one bolt's root to another's, or out to the cloud's edge; a new shape every 0.05 s, on and off
  if (now >= impact_ - 0.7f && now < last_ + 0.3f && !strikes_.empty()) {
    const uint32_t shape = static_cast<uint32_t>((now - start_) / kShapeSeconds);
    std::vector<VfxVertex> v;
    glm::vec3 lit{0.0f};
    int count = 0;
    for (uint32_t a = 0; a < 3; ++a) {
      const uint32_t key = a + shape * 3u;
      if (hash01(seed_, key, 85) < 0.45f) continue;
      const size_t i0 = static_cast<size_t>(hash01(seed_, key, 86) * static_cast<float>(strikes_.size())) % strikes_.size();
      const size_t i1 = static_cast<size_t>(hash01(seed_, key, 87) * static_cast<float>(strikes_.size())) % strikes_.size();
      const float spin = hash01(seed_, key, 88) * kTau;
      const glm::vec3 from = strikes_[i0].top;
      const glm::vec3 to = i1 != i0 ? strikes_[i1].top : cloudAt_ + glm::vec3(std::cos(spin), 0.0f, std::sin(spin)) * (cloudRadius_ * 0.7f);
      ribbon(v, arcLine(fractalLine(from, glm::vec3(to.x, from.y + (hash01(seed_, key, 89) - 0.5f) * 0.5f, to.z), 4, 0.12f, seed_ + key * 211u), 1.0f, 0.2f), 0.02f, pen.eye,
             0.0f);
      lit += (from + to) * 0.5f, ++count;
    }
    pen.draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.0f, 0.75f, kEnergy, glm::vec4(kCore, 3.0f), cloudAt_);
    if (count > 0) lights.push_back({lit / static_cast<float>(count) - kUp * 0.8f, kLight, 9.0f * static_cast<float>(count), 7.0f});
  }
}

// One bolt on one body, `age` seconds after its return stroke (negative: before it).
void StormBolt::buildStrike(const Strike& s, float age, float time, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (age < -kForewarn || age >= tail_ || s.limbs.empty()) return;
  const Pen pen = penFor(time, camera, frame, lights);
  const float k = scale_;
  const uint32_t shape = static_cast<uint32_t>((age + kForewarn) / kShapeSeconds);
  std::vector<VfxInstance> streaks;

  if (age < 0.0f) {
    // ---- before the stroke: the body knows it first ----
    const float gather = ease(-kForewarn, -kLeader, age);
    // streamers reaching up off its head, longer as the charge builds
    std::vector<VfxVertex> v;
    for (uint32_t a = 0; a < 3; ++a) {
      const uint32_t key = a + shape * 3u;
      const float reach = (0.25f + 1.0f * gather) * (0.6f + 0.4f * hash01(s.seed, key, 51)) * k;
      const glm::vec3 from = s.head + glm::vec3((hash01(s.seed, key, 52) - 0.5f) * 0.3f, 0.0f, (hash01(s.seed, key, 53) - 0.5f) * 0.3f) * k;
      const glm::vec3 lean{(hash01(s.seed, key, 54) - 0.5f) * 0.7f, 0.0f, (hash01(s.seed, key, 55) - 0.5f) * 0.7f};
      ribbon(v, arcLine(fractalLine(from, from + (kUp + lean) * reach, 3, 0.2f, s.seed + key * 131u), 1.0f, 0.0f), 0.011f * k, pen.eye, kPull);
    }
    pen.draw(frame.addVertices(v), v.size(), glm::mat4(1.0f), kLightning, 3.0f, 0.45f + 0.45f * gather, kEnergy, glm::vec4(kCore, 3.0f), s.body);
    if (age < -kLeader) {
      // the pre-beam: the path marked by a faint thread, blinking
      if (age >= -0.36f) {
        const float blink = (static_cast<uint32_t>((age + 1.0f) / 0.03f) % 2u) ? 1.0f : 0.4f;
        drawTree(pen, s.limbs, 0, 0.008f * k, 2.0f, 0.3f * blink * ease(-0.36f, -0.3f, age), kEnergy, kCore, 3.0f, s.body);
      }
      lights.push_back({s.body, kLight, (1.5f + 4.0f * gather) * k * (0.7f + 0.3f * hash01(s.seed, shape, 56)), 4.0f * k});
    } else {
      // the stepped leader: down the tree in jumps
      const float step = std::floor((age + kLeader) / (kLeader / kSteps));
      drawLeader(pen, s.limbs, k, std::min((step + 1.0f) / kSteps, 1.0f), s.body, streaks);
      lights.push_back({s.body, kLight, 5.0f * k, 4.0f * k});
    }
    drawStreaks(pen, streaks, s.body);
    return;
  }

  // ---- the stroke ----
  const bool twice = style_ == Style::Wrath;
  const Flash flash = twice ? flashTwice(age) : flashAt(age);
  const float burnEnd = twice ? kSecond + kFlashEnd : kFlashEnd;
  drawBurning(pen, s.limbs, k, flash, s.body, streaks);
  if (age >= burnEnd && age < burnEnd + 0.35f) drawTrace(pen, s.limbs, k, ease(burnEnd, burnEnd + 0.35f, age), s.body);
  drawStruck(pen, s.feet, s.body, s.height, s.stands, k, s.seed, shape, age, flash, burnEnd, tail_, 40u, streaks);
  drawStreaks(pen, streaks, s.body);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// ChainBolt: Cadeia de Raios.

ChainBolt::ChainBolt(glm::vec3 casterFeet, float casterHeight, const std::vector<Target>& chain, float start, float impact, uint32_t seed)
    : feet_(casterFeet), casterHeight_(std::clamp(casterHeight, 1.0f, 4.0f)), start_(start), impact_(std::max(impact, start + 0.1f)), seed_(seed) {
  hand_ = casterFeet + kUp * (casterHeight_ * 0.7f);
  glm::vec3 from = hand_;
  links_.reserve(chain.size());
  for (size_t i = 0; i < chain.size(); ++i) {
    Link l;
    l.seed = seed_ + static_cast<uint32_t>(i + 1) * 0x9e37u;
    l.feet = chain[i].feet, l.body = chain[i].body;
    l.height = std::clamp(chain[i].height, 1.0f, 4.0f);
    l.at = impact_ + kStagger * static_cast<float>(i);
    l.size = std::pow(0.9f, static_cast<float>(i));  // each jump a little thinner
    growTree(l.limbs, from, l.body, 5, 0, 0.0f, 0.0f, l.seed, 1.5f * l.size);
    from = l.body + kUp * (0.15f * l.height);
    links_.push_back(std::move(l));
  }
  finale_ = (links_.empty() ? impact_ : links_.back().at) + 0.32f;
}

void ChainBolt::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  const Pen pen = penFor(now, camera, frame, lights);
  // the cast: the charge drawn in to the hand until the bolt leaves it
  if (now < impact_ + 0.1f) {
    std::vector<VfxInstance> streaks;
    const float k = std::clamp(casterHeight_ / 2.0f, 0.7f, 1.4f);
    drawGather(pen, feet_, hand_, k, ease(start_, std::max(impact_ - 0.1f, start_ + 0.05f), now), 1.0f - ease(impact_, impact_ + 0.1f, now), now - start_, seed_, streaks);
    drawStreaks(pen, streaks, hand_);
  }
  // the finale: when the last body is reached, the whole chain lights at once, one more time
  constexpr float kFinaleSeconds = 0.06f, kLeap = 0.08f;
  const float finale = now - finale_;
  const bool whole = finale >= 0.0f && finale < kFinaleSeconds;
  for (const Link& l : links_) {
    const float age = now - l.at;
    if (age < -kLeap) continue;
    const float k = l.size;
    const uint32_t shape = static_cast<uint32_t>((age + kLeap) / kShapeSeconds);
    std::vector<VfxInstance> streaks;
    if (age < 0.0f) {
      // the jump: the leader crossing to the body in four steps
      const float step = std::floor((age + kLeap) / (kLeap / 4.0f));
      drawLeader(pen, l.limbs, k, std::min((step + 1.0f) / 4.0f, 1.0f), l.body, streaks);
      drawStreaks(pen, streaks, l.body);
      continue;
    }
    Flash flash = flashJump(age);
    if (whole) flash = {1.4f, 0.6f, 1.25f};
    drawBurning(pen, l.limbs, k, flash, l.body, streaks);
    // its trace, after its own burning and again after the finale
    if (age >= kJumpEnd && age < kJumpEnd + 0.3f && !whole) drawTrace(pen, l.limbs, k, ease(kJumpEnd, kJumpEnd + 0.3f, age), l.body);
    if (finale >= kFinaleSeconds && finale < kFinaleSeconds + 0.35f) drawTrace(pen, l.limbs, k, ease(kFinaleSeconds, kFinaleSeconds + 0.35f, finale), l.body);
    if (age < 1.0f) drawStruck(pen, l.feet, l.body, l.height, true, k, l.seed, shape, age, flashJump(age), kJumpEnd, 1.0f, 24u, streaks);
    if (whole) {
      drawBodyArcs(pen, l.body, l.height, k, l.seed + 0x77u, static_cast<uint32_t>(finale / 0.03f), 6u, 0.9f);
      streaks.push_back(streak(pulled(l.body, pen.eye, kPull), glm::vec3(0.0f), 1.6f * k, 1.0f, kCore, 3.2f));
      lights.push_back({l.body, kLight, 90.0f * k, 10.0f * k});
    }
    drawStreaks(pen, streaks, l.body);
  }
}

}  // namespace ember
