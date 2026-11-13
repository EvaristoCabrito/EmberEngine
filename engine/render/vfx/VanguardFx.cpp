#include "render/vfx/VanguardFx.h"

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
constexpr float kPull = 1.3f, kGroundPull = 0.7f;  // as CombatFx / WarriorFx: in front of the card, drawn 1.2 units nearer
constexpr float kGravity = 9.8f;
constexpr float kRingUnderFeet = 0.6f;
constexpr float kModeStreak = 0.0f, kModePuff = 1.0f, kModeEnergy = 2.0f, kModeRing = 3.0f, kModeTrail = 4.0f, kModeSlash = 5.0f;
constexpr float kHex = 1.7320508f;

// Kael's special: the sword lit violet, its dark smoke (content/sprites/kael-v3 "special")
constexpr glm::vec3 kViolet{0.66f, 0.4f, 1.0f}, kVioletHot{0.9f, 0.8f, 1.0f}, kVioletDeep{0.42f, 0.16f, 0.85f};
constexpr glm::vec3 kWhite{1.0f, 0.98f, 0.96f}, kSteel{0.78f, 0.86f, 1.0f};
constexpr glm::vec3 kSmoke{0.1f, 0.08f, 0.13f}, kDust{0.46f, 0.4f, 0.33f}, kStone{0.52f, 0.5f, 0.48f};

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
glm::vec3 bezier(glm::vec3 a, glm::vec3 b, glm::vec3 c, float s) { return (1.0f - s) * (1.0f - s) * a + 2.0f * (1.0f - s) * s * b + s * s * c; }

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
const std::vector<VfxVertex>& rockGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> r = vfxgeo::icosahedron(1.0f, 0);
    for (VfxVertex& v : r) v.position *= glm::vec3(1.0f, 0.7f, 0.85f);
    for (size_t i = 0; i + 2 < r.size(); i += 3) {
      const glm::vec3 n = glm::normalize(glm::cross(r[i + 1].position - r[i].position, r[i + 2].position - r[i].position));
      r[i].normal = r[i + 1].normal = r[i + 2].normal = n;
    }
    return r;
  }();
  return g;
}
const std::vector<VfxVertex>& chipGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::box(1.0f, 0.22f, 0.8f);
  return g;
}
// A broken ring: two arcs of a thin torus with a gap between them, lying flat (around +y).
const std::vector<VfxVertex>& arcGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> out;
    const glm::mat4 flat = glm::rotate(glm::mat4(1.0f), -kPi * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    for (int k = 0; k < 2; ++k) {
      std::vector<VfxVertex> arc = vfxgeo::torus(1.0f, 0.07f, 6, 40, 2.35f);
      const glm::mat4 m = flat * glm::rotate(glm::mat4(1.0f), static_cast<float>(k) * kPi + 0.4f, glm::vec3(0.0f, 0.0f, 1.0f));
      for (VfxVertex& v : arc) {
        v.position = glm::vec3(m * glm::vec4(v.position, 1.0f));
        v.normal = glm::vec3(m * glm::vec4(v.normal, 0.0f));
        out.push_back(v);
      }
    }
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
    if (mode == kModeRing && std::abs(model[3].y - sight.y) < kRingUnderFeet) d.underCards = true;  // a ring at the feet lies under every card
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

constexpr const char* kKinds[] = {"measuredCut",  "longstepSlash", "crossguard",     "pommelCheck",       "lowCut",       "partingCut",    "overheadHew",
                                  "steadyFooting", "challenge",     "breakGuard",     "feint",             "riposte",      "passingStep",   "needlePoint",
                                  "secondIntention", "readTheBlade", "severTheRhythm", "perfectCut",       "deadMeasure",  "masterstroke",  "drivingCut",
                                  "wideCleave",   "sidecast",      "plantTheLine",   "shoulderThrough",   "crushAgainstStone", "splitTheRank", "reapTheFlanks",
                                  "rollingAdvance", "shatterTheLine"};

}  // namespace

bool VanguardFx::handles(const std::string& kind) {
  return std::any_of(std::begin(kKinds), std::end(kKinds), [&](const char* k) { return kind == k; });
}

// --- Pieces --------------------------------------------------------------------------------------------------------------------------

void VanguardFx::add(Piece p) {
  p.seed = 0x5a17u + 7919u * ++serial_;
  pieces_.push_back(p);
}
void VanguardFx::cut(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 thick, float width, float start, float sweep, float intensity, bool heavy) {
  Piece p;
  p.shape = Shape::Sheet, p.a = a, p.b = b, p.c = c, p.thick = thick, p.width = width, p.start = start, p.sweep = sweep, p.intensity = intensity;
  p.color = heavy ? kVioletDeep : kViolet, p.core = kVioletHot;
  p.size = heavy ? 0.12f : 0.05f;   // hold
  p.size2 = heavy ? 0.36f : 0.24f;  // fade
  p.life = sweep + p.size + p.size2;
  add(p);
}
void VanguardFx::flash(glm::vec3 at, float size, float when, float life, glm::vec3 color) {
  Piece p;
  p.shape = Shape::Flash, p.a = at, p.size = size, p.start = when, p.life = life, p.color = glm::dot(color, color) > 0.0f ? color : kVioletHot;
  add(p);
}
void VanguardFx::sparks(glm::vec3 at, glm::vec3 dir, int count, float speed, float when, float ground) {
  Piece p;
  p.shape = Shape::Sparks, p.a = at, p.thick = dir, p.count = count, p.speed = speed, p.start = when, p.life = 0.5f, p.ground = ground;
  add(p);
}
void VanguardFx::ring(glm::vec3 at, float from, float to, float when, float life, glm::vec3 color, float alpha) {
  Piece p;
  p.shape = Shape::Ring, p.a = at, p.size = from, p.size2 = to, p.start = when, p.life = life, p.color = color, p.alpha = alpha;
  add(p);
}
void VanguardFx::shell(glm::vec3 at, float from, float to, float when, float life, glm::vec3 color, bool dark, float alpha) {
  Piece p;
  p.shape = Shape::Shell, p.a = at, p.size = from, p.size2 = to, p.start = when, p.life = life, p.color = color, p.dark = dark, p.alpha = alpha;
  add(p);
}
void VanguardFx::smoke(glm::vec3 at, float out, int count, float when, float life, bool dark, float alpha) {
  Piece p;
  p.shape = Shape::Smoke, p.a = at, p.size2 = out, p.count = count, p.start = when, p.life = life, p.dark = dark, p.alpha = alpha;
  add(p);
}
void VanguardFx::line(glm::vec3 a, glm::vec3 c, float width, float when, float life, glm::vec3 color, float intensity) {
  Piece p;
  p.shape = Shape::Line, p.a = a, p.c = c, p.width = width, p.start = when, p.life = life, p.color = color, p.intensity = intensity;
  add(p);
}
void VanguardFx::sigil(glm::vec3 at, float size, float when, float life, glm::vec3 color) {
  Piece p;
  p.shape = Shape::Sigil, p.a = at, p.size = size, p.start = when, p.life = life, p.color = color;
  add(p);
}
void VanguardFx::debris(glm::vec3 at, glm::vec3 dir, int count, float speed, float when, float ground, bool steel) {
  Piece p;
  p.shape = Shape::Debris, p.a = at, p.thick = dir, p.count = count, p.speed = speed, p.start = when, p.life = 1.6f, p.ground = ground, p.steel = steel;
  add(p);
}
void VanguardFx::light(glm::vec3 at, float reach, float when, float life, glm::vec3 color, float intensity) {
  Piece p;
  p.shape = Shape::Light, p.a = at, p.size = reach, p.start = when, p.life = life, p.color = color, p.intensity = intensity;
  add(p);
}
void VanguardFx::crossed(glm::vec3 at, glm::vec3 facing, float size, float when, float life) {
  Piece p;
  p.shape = Shape::Cross, p.a = at, p.thick = facing, p.size = size, p.start = when, p.life = life;
  add(p);
}
void VanguardFx::contact(const Target& foe, glm::vec3 dir, float when, float scale) {
  const float h = bodyHeight(foe.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f) * scale;
  const glm::vec3 chest = foe.feet + kUp * (h * 0.5f) - dir * 0.15f;
  flash(chest, 0.55f * k, when, 0.14f);
  sparks(chest, dir, static_cast<int>(10 * scale) + 4, 3.0f * k, when, foe.feet.y);
  light(chest, 2.6f * k, when, 0.25f, {0.75f, 0.55f, 1.0f}, 11.0f * scale);
}

// --- The looks -----------------------------------------------------------------------------------------------------------------------

bool VanguardFx::technique(const std::string& kind, const Target& caster, const std::vector<Target>& targets, float start, float impact) {
  if (!handles(kind)) return false;
  const float hk = bodyHeight(caster.height), kk = std::clamp(hk / 2.0f, 0.7f, 1.4f);
  const glm::vec3 K = caster.feet;
  // the struck / marked body (a Self technique: Kael himself)
  Target foe = targets.empty() ? caster : targets.front();
  const bool self = targets.empty() || glm::length(glm::vec2(foe.feet.x - K.x, foe.feet.z - K.z)) < 0.3f;
  if (foe.height <= 0.0f) foe.height = 1.8f;
  const float h = bodyHeight(foe.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 F = foe.feet;
  // on himself: the way he faces is toward the foe the stage names after his own hex (the nearest one)
  const glm::vec3 dir = self && targets.size() > 1 ? flatDir(targets[1].feet - K, glm::vec3(0.0f, 0.0f, 1.0f)) : flatDir(F - K, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 side = glm::normalize(glm::cross(kUp, dir));
  const glm::vec3 chest = F + kUp * (h * 0.5f) - dir * 0.15f;
  const glm::vec3 kChest = K + kUp * (hk * 0.55f) + dir * 0.35f;
  const float t = impact;
  // the arc / line techniques: one sweep for the whole technique, each later foe only its contact
  std::erase_if(recent_, [&](const Recent& r) { return r.at < start - 3.0f; });
  int step = 0;
  for (Recent& r : recent_)
    if (r.kind == kind && glm::length(r.caster - K) < 0.4f && start - r.at < 2.5f) step = r.step + 1;
  recent_.push_back({kind, K, start, step});

  // a diagonal cut across the body, top to bottom (`flip`: from the other shoulder), bowed toward the striker
  auto diagonal = [&](float when, float sweep, float scale, bool flip, float width, float intensity, bool heavy) {
    const glm::vec3 s = flip ? -side : side;
    const glm::vec3 a = F + kUp * (h * (0.62f + 0.4f * scale)) - s * (0.55f * scale * k) - dir * 0.1f;
    const glm::vec3 c = F + kUp * (h * (0.38f - 0.22f * scale)) + s * (0.6f * scale * k) - dir * 0.2f;
    cut(a, (a + c) * 0.5f - dir * (0.4f * k), c, dir, width * k, when - sweep, sweep, intensity, heavy);
  };
  // a level cut across the body at `y` of its height (`curl`: how far it bows round toward the striker)
  auto level = [&](float when, float sweep, float y, float span, float curl, float width, float intensity, bool heavy, bool flip = false) {
    const glm::vec3 s = flip ? -side : side;
    const glm::vec3 at = F + kUp * (h * y) - dir * 0.15f;
    cut(at - s * (span * k), at - dir * (curl * k), at + s * (span * k), kUp, width * k, when - sweep, sweep, intensity, heavy);
  };
  auto dust = [&](glm::vec3 at, float out, int n, float when) { smoke(at + kUp * 0.05f, out, n, when, 0.9f, false, 0.4f); };

  // ---- Vanguarda ----
  if (kind == "measuredCut") {  // compact diagonal cut and a quick return to guard
    diagonal(t, 0.1f, 0.75f, false, 0.14f, 1.6f, false);
    contact(foe, dir, t);
  } else if (kind == "longstepSlash") {  // the long lead step into a level cut; the boot's scuff
    level(t, 0.12f, 0.5f, 0.95f, 0.45f, 0.13f, 1.7f, false);
    dust(K - dir * 0.3f, 0.9f * kk, 9, t - 0.15f);
    contact(foe, dir, t);
  } else if (kind == "crossguard") {  // the blade held across the chest: two blades of light crossed before him, a steel ring at the touch
    const glm::vec3 eyeward = dir;
    crossed(K + kUp * (hk * 0.55f) + eyeward * 0.4f, eyeward, 0.75f * kk, t - 0.05f, 1.2f);
    sigil(K + kUp * (hk * 0.55f) + eyeward * 0.35f, 0.42f * kk, t, 1.2f, kViolet);
    flash(K + kUp * (hk * 0.55f) + eyeward * 0.4f, 0.4f * kk, t, 0.16f, kWhite);
    light(K + kUp * (hk * 0.55f), 2.4f * kk, t, 1.0f, {0.7f, 0.55f, 1.0f}, 5.0f);
  } else if (kind == "pommelCheck") {  // the pommel driven forward: a dull knock, no edge (a grey shell, grit off the armour)
    shell(chest, 0.12f * k, 0.55f * k, t, 0.28f, {0.62f, 0.6f, 0.66f}, false, 0.45f);
    flash(chest, 0.32f * k, t, 0.1f, {0.85f, 0.82f, 0.9f});
    debris(chest, dir * 0.6f + kUp * 0.3f, 7, 1.6f * k, t, F.y, true);
    sigil(F + kUp * (h * 0.92f), 0.24f * k, t + 0.1f, 1.0f, {0.6f, 0.58f, 0.7f});  // dazed: a dull ring over the head
    light(chest, 1.8f, t, 0.18f, {0.8f, 0.78f, 0.9f}, 6.0f);
  } else if (kind == "lowCut") {  // the low sweep at the knees, the ground skimmed
    level(t, 0.12f, 0.2f, 1.0f, 0.35f, 0.11f, 1.6f, false);
    smoke(F + kUp * 0.05f + dir * 0.2f, 0.9f * k, 10, t, 0.8f, false, 0.4f);
    sparks(F + kUp * (h * 0.2f), dir, 8, 2.4f * k, t, F.y);
    ring(F + glm::vec3(0.0f, 0.04f, 0.0f), 0.2f * k, 0.8f * k, t, 0.5f, kViolet, 0.5f);
    light(F + kUp * (h * 0.2f), 2.0f, t, 0.2f, {0.72f, 0.5f, 1.0f}, 8.0f);
  } else if (kind == "partingCut") {  // a short rising cut, then the step back (dust at his heel)
    const glm::vec3 a = F + kUp * (h * 0.2f) + side * (0.5f * k) - dir * 0.2f, c = F + kUp * (h * 0.95f) - side * (0.45f * k) - dir * 0.1f;
    cut(a, (a + c) * 0.5f - dir * (0.35f * k), c, dir, 0.13f * k, t - 0.11f, 0.11f, 1.6f);
    contact(foe, dir, t);
    dust(K - dir * 0.5f, 0.8f * kk, 8, t + 0.15f);
  } else if (kind == "overheadHew") {  // the two-handed overhead swing: a great cut from above the head, the ground cracked
    const glm::vec3 a = F + kUp * (h * 1.3f) - dir * 0.1f, c = F + kUp * (h * 0.12f) - dir * 0.25f;
    cut(a, (a + c) * 0.5f - dir * (0.35f * k), c, dir, 0.24f * k, t - 0.13f, 0.13f, 2.0f, true);
    contact(foe, dir, t, 1.4f);
    ring(F + glm::vec3(0.0f, 0.04f, 0.0f), 0.3f * k, 1.3f * k, t, 0.55f, {0.8f, 0.5f, 1.0f});
    debris(F + kUp * 0.05f, kUp * 0.4f + dir * 0.3f, 10, 2.2f * k, t, F.y, false);
    dust(F, 1.3f * k, 12, t);
  } else if (kind == "steadyFooting") {  // the weight dropped onto the rear foot: dust bursts, a stone-grey ring stays at his feet
    smoke(K + kUp * 0.05f, 1.2f * kk, 14, t, 1.0f, false, 0.5f);
    ring(K + glm::vec3(0.0f, 0.04f, 0.0f), 0.9f * kk, 0.7f * kk, t, 1.3f, kStone, 0.75f);
    ring(K + glm::vec3(0.0f, 0.05f, 0.0f), 0.2f * kk, 1.1f * kk, t, 0.45f, {0.7f, 0.55f, 1.0f}, 0.6f);
    debris(K + kUp * 0.05f, kUp * 0.2f, 6, 1.2f * kk, t, K.y, false);
    light(K + kUp * 0.3f, 2.0f * kk, t, 0.9f, {0.95f, 0.8f, 0.6f}, 4.0f);
  } else if (kind == "challenge") {  // a short bark and the point of the blade: a shout's pulse, a line of light to the foe, a ring on it
    shell(kChest, 0.2f * kk, 0.9f * kk, t, 0.32f, {0.95f, 0.7f, 0.45f}, false, 0.35f);
    line(kChest, chest, 0.05f, t, 0.6f, kViolet, 2.0f);
    sigil(chest, 0.38f * k, t + 0.12f, 1.4f, {1.0f, 0.55f, 0.35f});
    light(chest, 2.0f * k, t + 0.1f, 0.8f, {1.0f, 0.6f, 0.4f}, 5.0f);
  } else if (kind == "breakGuard") {  // the guard wrenched aside with the crossguard (a sideways clack), then the chop; the guard left open
    const glm::vec3 hand = F + kUp * (h * 0.55f) + side * (0.35f * k) - dir * 0.25f;
    flash(hand, 0.35f * k, t - 0.18f, 0.12f, kWhite);
    sparks(hand, side, 8, 2.6f * k, t - 0.18f, F.y);
    diagonal(t, 0.11f, 0.95f, true, 0.18f, 1.8f, true);
    contact(foe, dir, t, 1.2f);
    sigil(chest + kUp * 0.1f, 0.48f * k, t + 0.1f, 2.2f, kViolet);
  }
  // ---- Mestre da Espada ----
  else if (kind == "feint") {  // a high cut begun and pulled across the body: half drawn, faint; the foe left Exposto
    const glm::vec3 a = F + kUp * (h * 1.1f) - side * (0.5f * k) - dir * 0.1f, c = F + kUp * (h * 0.7f) + side * (0.1f * k) - dir * 0.2f;
    cut(a, (a + c) * 0.5f - dir * (0.3f * k), c, dir, 0.08f * k, t - 0.14f, 0.14f, 0.7f);
    sigil(chest, 0.44f * k, t + 0.05f, 1.8f, kViolet);
    line(chest - side * (0.5f * k), chest + side * (0.5f * k), 0.025f, t + 0.05f, 1.6f, kVioletHot, 1.4f);  // the mark's slit
    light(chest, 1.8f * k, t, 1.2f, {0.7f, 0.5f, 1.0f}, 4.0f);
  } else if (kind == "passingStep") {  // the cut on the turn: curved round the foe's side
    const glm::vec3 a = F + kUp * (h * 0.6f) - dir * (0.7f * k) - side * (0.5f * k);
    const glm::vec3 c = F + kUp * (h * 0.45f) + dir * (0.4f * k) + side * (0.6f * k);
    cut(a, F + kUp * (h * 0.52f) - side * (0.9f * k), c, kUp, 0.13f * k, t - 0.13f, 0.13f, 1.7f);
    contact(foe, dir, t);
    smoke(K + kUp * 0.05f, 0.7f * kk, 8, t - 0.2f, 0.8f, false, 0.35f);
  } else if (kind == "needlePoint") {  // the point-first thrust: a needle of light through the body, a narrow spray out the back
    const glm::vec3 a = chest - dir * (1.1f * k), c = chest + dir * (0.7f * k);
    line(a, c, 0.05f * k, t - 0.04f, 0.3f, kVioletHot, 2.6f);
    flash(chest, 0.28f * k, t, 0.12f, kWhite);
    sparks(chest + dir * 0.2f, dir * 2.0f, 12, 4.0f * k, t, F.y);
    ring(chest, 0.05f, 0.35f * k, t, 0.22f, kViolet, 0.6f);
    light(chest, 2.0f, t, 0.2f, {0.8f, 0.7f, 1.0f}, 9.0f);
  } else if (kind == "secondIntention") {  // the short cut draws the guard, the low return cut: one look per blow
    if (step % 2 == 0) diagonal(t, 0.09f, 0.6f, false, 0.12f, 1.4f, false);
    else level(t, 0.1f, 0.28f, 0.85f, 0.4f, 0.12f, 1.7f, false, true);
    contact(foe, dir, t, step % 2 == 0 ? 0.8f : 1.1f);
  } else if (kind == "readTheBlade") {  // the guard held still: a thin blade of light, a ring turning at the hand, his eye on the foe
    const glm::vec3 hand = K + kUp * (hk * 0.55f) + dir * 0.45f;
    line(hand - kUp * (0.2f * kk), hand + kUp * (0.75f * kk) + dir * 0.15f, 0.04f, t - 0.05f, 1.2f, kVioletHot, 1.8f);
    sigil(hand, 0.3f * kk, t, 1.2f, kViolet);
    line(hand, chest, 0.015f, t, 0.9f, kViolet, 1.0f);
    light(hand, 1.8f * kk, t, 1.0f, {0.7f, 0.55f, 1.0f}, 4.0f);
  } else if (kind == "severTheRhythm") {  // the opposing weapon beaten down (two clacks), then the cut across the opening; its beat broken
    const glm::vec3 hand = F + kUp * (h * 0.48f) + side * (0.4f * k) - dir * 0.3f;
    flash(hand, 0.3f * k, t - 0.24f, 0.1f, kWhite), sparks(hand, -kUp + side * 0.3f, 6, 2.2f * k, t - 0.24f, F.y);
    flash(hand - kUp * 0.15f, 0.3f * k, t - 0.12f, 0.1f, kWhite), sparks(hand - kUp * 0.15f, -kUp - side * 0.3f, 6, 2.2f * k, t - 0.12f, F.y);
    level(t, 0.1f, 0.55f, 0.85f, 0.3f, 0.13f, 1.7f, false);
    contact(foe, dir, t);
    ring(chest, 0.5f * k, 0.15f * k, t + 0.05f, 0.4f, {0.7f, 0.45f, 1.0f}, 0.8f);  // the pulse that breaks inward
  } else if (kind == "perfectCut") {  // the pause at guard, then one cut in one short motion: thin, very bright, its glint splitting after
    const glm::vec3 a = F + kUp * (h * 1.15f) - side * (0.7f * k) - dir * 0.1f, c = F + kUp * (h * 0.1f) + side * (0.7f * k) - dir * 0.2f;
    cut(a, (a + c) * 0.5f - dir * (0.15f * k), c, dir, 0.07f * k, t - 0.06f, 0.06f, 2.6f);
    line(a, c, 0.02f, t + 0.06f, 0.45f, kWhite, 3.0f);
    flash(chest, 0.7f * k, t, 0.12f, kWhite);
    sparks(chest, dir, 14, 3.4f * k, t, F.y);
    light(chest, 3.0f * k, t, 0.3f, {0.85f, 0.75f, 1.0f}, 14.0f);
  } else if (kind == "deadMeasure") {  // the point tracking the marked foe: a line from the blade, a ring under its feet
    const glm::vec3 point = K + kUp * (hk * 0.5f) + dir * 0.7f;
    line(point, chest, 0.02f, t, 1.6f, kViolet, 1.3f);
    ring(F + glm::vec3(0.0f, 0.04f, 0.0f), 0.75f * k, 0.6f * k, t, 1.6f, {0.85f, 0.35f, 0.55f}, 0.75f);
    sigil(F + kUp * 0.06f, 0.62f * k, t, 1.6f, {0.85f, 0.35f, 0.6f});
  } else if (kind == "masterstroke") {  // both hands on the hilt, one diagonal stroke through the opening: the widest sheet, a shock ring,
                                        // the cut left hanging in the air
    const glm::vec3 a = F + kUp * (h * 1.4f) - side * (1.0f * k) - dir * 0.05f, c = F + kUp * (h * 0.02f) + side * (1.0f * k) - dir * 0.25f;
    cut(a, (a + c) * 0.5f - dir * (0.5f * k), c, dir, 0.32f * k, t - 0.15f, 0.15f, 2.4f, true);
    line(a, c, 0.035f, t + 0.02f, 0.9f, kVioletHot, 2.2f);
    contact(foe, dir, t, 1.7f);
    shell(chest, 0.2f * k, 1.4f * k, t, 0.35f, {0.75f, 0.5f, 1.0f}, false, 0.4f);
    ring(F + glm::vec3(0.0f, 0.04f, 0.0f), 0.4f * k, 2.0f * k, t, 0.6f, kViolet);
    smoke(F, 1.6f * k, 14, t, 1.2f, true, 0.4f);
    debris(chest, dir * 0.8f + kUp * 0.2f, 10, 2.4f * k, t, F.y, true);
  }
  // ---- Arauto da Guerra ----
  else if (kind == "drivingCut") {  // the two-handed cut on a forward step, the boot stamped; the push's dust behind the foe
    level(t, 0.12f, 0.5f, 1.0f, 0.55f, 0.17f, 1.9f, true);
    contact(foe, dir, t, 1.2f);
    dust(K, 0.9f * kk, 10, t - 0.08f);
    smoke(F + dir * (kHex * 0.5f), 0.8f * k, 10, t + 0.08f, 0.9f, false, 0.45f);
  } else if (kind == "wideCleave") {  // the broad waist-height sweep across the front arc (once), each foe its own contact
    if (step == 0) {
      const glm::vec3 at = K + kUp * (hk * 0.5f);
      const float r = 1.25f * kHex;
      const glm::vec3 a = at + (dir * 0.25f - side) * r, c = at + (dir * 0.25f + side) * r;
      cut(a, at + dir * (r * 1.6f), c, kUp, 0.22f * kk, t - 0.16f, 0.16f, 1.8f, true);
      dust(K, 1.0f * kk, 10, t);
    }
    contact(foe, dir, t + 0.02f * static_cast<float>(step));
  } else if (kind == "sidecast") {  // the low-to-high cut through the hips; the slide's dust to where it goes
    const glm::vec3 a = F + kUp * (h * 0.15f) - side * (0.6f * k) - dir * 0.2f, c = F + kUp * (h * 0.9f) + side * (0.5f * k) - dir * 0.15f;
    cut(a, (a + c) * 0.5f - dir * (0.4f * k), c, dir, 0.15f * k, t - 0.12f, 0.12f, 1.7f);
    contact(foe, dir, t);
    const glm::vec3 to = targets.size() > 1 ? targets[1].feet : F + side * kHex;
    for (int i = 0; i < 4; ++i) smoke(glm::mix(F, to, static_cast<float>(i) / 3.0f), 0.4f * k, 4, t + 0.05f + 0.05f * static_cast<float>(i), 0.8f, false, 0.4f);
  } else if (kind == "plantTheLine") {  // the stance widened, the point dropped: the front arc drawn on the ground in light, dust at his feet
    for (int i = -1; i <= 1; ++i) {
      const float ang = static_cast<float>(i) * kPi / 3.0f;
      const glm::vec3 d = dir * std::cos(ang) + side * std::sin(ang);
      ring(K + d * kHex + glm::vec3(0.0f, 0.04f, 0.0f), 0.62f, 0.55f, t, 1.6f, {0.85f, 0.55f, 1.0f}, 0.7f);
    }
    line(K + kUp * (hk * 0.4f) + dir * 0.3f, K + dir * (kHex * 0.9f) + kUp * 0.08f, 0.04f, t - 0.05f, 1.5f, kVioletHot, 1.6f);
    dust(K, 0.8f * kk, 10, t);
    light(K + dir * kHex + kUp * 0.3f, 2.4f, t, 1.4f, {0.75f, 0.55f, 1.0f}, 4.0f);
  } else if (kind == "shoulderThrough") {  // driven into the gap with the shoulder (a dusty shock), then the sword; the foe pushed on
    shell(chest - dir * 0.2f, 0.15f * k, 0.8f * k, t - 0.1f, 0.3f, {0.7f, 0.62f, 0.55f}, false, 0.45f);
    debris(chest, dir * 0.6f + kUp * 0.2f, 6, 1.8f * k, t - 0.1f, F.y, true);
    diagonal(t, 0.1f, 0.8f, false, 0.16f, 1.8f, true);
    contact(foe, dir, t, 1.1f);
    smoke(F + dir * (kHex * 0.5f), 0.8f * k, 9, t + 0.08f, 0.9f, false, 0.45f);
  } else if (kind == "crushAgainstStone") {  // blade and body weight driven forward (the stone's grit comes from the stage when it hits)
    const glm::vec3 a = chest - dir * (1.0f * k) + kUp * 0.15f, c = chest + dir * (0.4f * k) - kUp * 0.05f;
    line(a, c, 0.09f * k, t - 0.05f, 0.25f, kVioletHot, 2.2f);
    level(t, 0.11f, 0.45f, 0.7f, 0.3f, 0.16f, 1.8f, true);
    contact(foe, dir, t, 1.3f);
  } else if (kind == "splitTheRank") {  // the long through-cut down the line: one sweep to the second hex, a contact on each
    if (step == 0) {
      const glm::vec3 far = targets.size() > 1 ? targets.back().feet : F + dir * kHex;
      const glm::vec3 a = K + kUp * (hk * 0.5f) + dir * 0.4f, c = far + kUp * (h * 0.5f) + dir * 0.4f;
      cut(a, (a + c) * 0.5f + kUp * 0.15f, c, kUp, 0.12f * kk, t - 0.12f, 0.12f, 2.0f);
      line(a, c, 0.03f, t, 0.4f, kVioletHot, 2.0f);
    }
    contact(foe, dir, t, step == 0 ? 1.1f : 0.9f);
  } else if (kind == "reapTheFlanks") {  // two controlled cuts sweeping out from the centre, one to each flank
    const glm::vec3 at = K + kUp * (hk * 0.48f);
    const float r = 1.15f * kHex;
    const glm::vec3 to = flatDir(F - K, dir);
    cut(at + dir * r * 0.9f, at + (dir + to) * (r * 0.75f), at + to * r, kUp, 0.16f * kk, t - 0.12f, 0.12f, 1.8f);
    contact(foe, dir, t);
    smoke(F, 0.7f * k, 8, t + 0.06f, 0.8f, false, 0.4f);
  } else if (kind == "rollingAdvance") {  // the heavy cut and the advancing stomp; then the second cut
    if (step % 2 == 0) {
      level(t, 0.12f, 0.5f, 0.95f, 0.5f, 0.17f, 1.9f, true);
      contact(foe, dir, t, 1.2f);
      dust(K, 1.0f * kk, 10, t);
    } else {
      diagonal(t, 0.1f, 0.85f, true, 0.17f, 1.9f, true);
      contact(foe, dir, t, 1.2f);
      ring(K + glm::vec3(0.0f, 0.04f, 0.0f), 0.3f * kk, 1.2f * kk, t - 0.1f, 0.4f, kStone, 0.7f);  // the stomp
    }
    smoke(F + dir * (kHex * 0.5f), 0.8f * k, 9, t + 0.08f, 0.9f, false, 0.45f);
  } else if (kind == "shatterTheLine") {  // the loaded two-handed sweep: a great shock at the centre, lighter flashes on the flanks
    if (step == 0) {
      const glm::vec3 at = K + kUp * (hk * 0.5f);
      const float r = 1.3f * kHex;
      cut(at + (dir * 0.2f - side) * r, at + dir * (r * 1.7f), at + (dir * 0.2f + side) * r, kUp, 0.3f * kk, t - 0.18f, 0.18f, 2.4f, true);
      shell(chest, 0.25f * k, 1.6f * k, t, 0.38f, {0.78f, 0.5f, 1.0f}, false, 0.45f);
      ring(F + glm::vec3(0.0f, 0.04f, 0.0f), 0.4f * k, 2.2f * k, t, 0.65f, kViolet);
      smoke(F, 1.6f * k, 16, t, 1.2f, true, 0.45f);
      debris(F + kUp * 0.05f, dir * 0.5f + kUp * 0.4f, 10, 2.4f * k, t, F.y, false);
      contact(foe, dir, t, 1.6f);
    } else {
      contact(foe, dir, t, 0.8f);
      sigil(F + kUp * (h * 0.92f), 0.24f * k, t + 0.05f, 1.2f, {0.7f, 0.5f, 0.9f});  // the shaken flank's weaker next blow
    }
  }
  return true;
}

void VanguardFx::riposte(const Target& kael, const Target& attacker, float impact) {
  const float hk = bodyHeight(kael.height), kk = std::clamp(hk / 2.0f, 0.7f, 1.4f);
  const float h = bodyHeight(attacker.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 dir = flatDir(attacker.feet - kael.feet, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 guard = kael.feet + kUp * (hk * 0.55f) + dir * 0.4f;
  const glm::vec3 chest = attacker.feet + kUp * (h * 0.5f) - dir * 0.15f;
  // the parry still ringing: a ring of sparks at his guard, then the blade snapped back down the attacker's line
  ring(guard, 0.1f, 0.5f * kk, impact - 0.22f, 0.25f, kVioletHot, 0.8f);
  sparks(guard, -dir + kUp * 0.3f, 10, 3.0f * kk, impact - 0.22f, kael.feet.y);
  line(guard, chest + dir * (0.3f * k), 0.06f * k, impact - 0.05f, 0.25f, kVioletHot, 2.6f);
  contact(attacker, dir, impact, 1.2f);
  sigil(chest, 0.36f * k, impact + 0.08f, 1.4f, kViolet);  // Exposto
}

void VanguardFx::reaction(const std::string& kind, const Target& kael, const Target& walker, float impact) {
  const float h = bodyHeight(walker.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 dir = flatDir(walker.feet - kael.feet, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 side = glm::normalize(glm::cross(kUp, dir));
  const glm::vec3 chest = walker.feet + kUp * (h * 0.5f) - dir * 0.15f;
  if (kind == "deadMeasure") {  // one short lunge as it breaks away: a thrust down the line, the ring under it snapping shut
    line(chest - dir * (1.2f * k), chest + dir * (0.4f * k), 0.07f * k, impact - 0.05f, 0.28f, kVioletHot, 2.6f);
    ring(walker.feet + glm::vec3(0.0f, 0.04f, 0.0f), 0.8f * k, 0.15f * k, impact, 0.3f, {0.85f, 0.35f, 0.6f}, 0.9f);
  } else {  // Firmar a Linha: the short advancing cut on the one that came
    const glm::vec3 at = walker.feet + kUp * (h * 0.5f) - dir * 0.15f;
    cut(at - side * (0.8f * k), at - dir * (0.4f * k), at + side * (0.8f * k), kUp, 0.15f * k, impact - 0.1f, 0.1f, 1.8f, true);
  }
  contact(walker, dir, impact, 1.1f);
}

void VanguardFx::stone(const Target& foe, glm::vec3 dir, float at) {
  const float h = bodyHeight(foe.height), k = std::clamp(h / 2.0f, 0.7f, 1.4f);
  const glm::vec3 d = flatDir(dir, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 back = foe.feet + d * 0.6f + kUp * (h * 0.4f);
  // hard stone at its back: grit and loose stones thrown, a dust cloud, a crack ring under it
  debris(back, -d * 0.3f + kUp * 0.5f, 16, 2.6f * k, at, foe.feet.y, false);
  smoke(foe.feet + d * 0.5f, 1.1f * k, 14, at, 1.1f, false, 0.55f);
  ring(foe.feet + glm::vec3(0.0f, 0.04f, 0.0f), 0.3f * k, 1.2f * k, at, 0.5f, {0.85f, 0.65f, 0.45f});
  flash(back, 0.6f * k, at, 0.14f, {1.0f, 0.85f, 0.7f});
  light(back, 2.8f * k, at, 0.3f, {1.0f, 0.8f, 0.6f}, 12.0f);
}

bool VanguardFx::active(float now) const {
  return std::any_of(pieces_.begin(), pieces_.end(), [&](const Piece& p) { return p.end() > now; });
}
float VanguardFx::latestEnd() const {
  float end = 0.0f;
  for (const Piece& p : pieces_) end = std::max(end, p.end());
  return end;
}

// --- Drawing -------------------------------------------------------------------------------------------------------------------------

void VanguardFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  std::erase_if(pieces_, [&](const Piece& p) { return p.end() <= now; });
  if (pieces_.empty()) return;
  quadFirst_ = frame.addVertices(quadGeometry()), quadCount_ = static_cast<uint32_t>(quadGeometry().size());
  sphereFirst_ = frame.addVertices(sphereGeometry()), sphereCount_ = static_cast<uint32_t>(sphereGeometry().size());
  discFirst_ = frame.addVertices(discGeometry()), discCount_ = static_cast<uint32_t>(discGeometry().size());
  rockFirst_ = frame.addVertices(rockGeometry()), rockCount_ = static_cast<uint32_t>(rockGeometry().size());
  chipFirst_ = frame.addVertices(chipGeometry()), chipCount_ = static_cast<uint32_t>(chipGeometry().size());
  arcFirst_ = frame.addVertices(arcGeometry()), arcCount_ = static_cast<uint32_t>(arcGeometry().size());
  streaks_.clear(), puffs_.clear(), rocks_.clear(), chips_.clear();
  const glm::vec3 eye = camera.position;
  for (const Piece& p : pieces_)
    if (now >= p.start) buildPiece(p, now, eye, frame, lights);
  // the particles of everything in play, one draw each: the smoke and dust first (they cover), the thrown pieces, then the light
  const glm::vec3 sight = pieces_.front().a;
  const Draw draw{frame};
  if (!puffs_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), puffs_.begin(), puffs_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModePuff, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(0.0f), sight, VfxBlend::Alpha, first,
         static_cast<uint32_t>(puffs_.size()));
  }
  for (int set = 0; set < 2; ++set) {
    const std::vector<VfxInstance>& pieces = set == 0 ? rocks_ : chips_;
    if (pieces.empty()) continue;
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), pieces.begin(), pieces.end());
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {set == 0 ? 0.92f : 0.45f, set == 0 ? 0.0f : 0.6f, 0.0f, 0.0f};  // roughness, metalness
    d.gpu.params1 = glm::vec4(0.0f);
    d.gpu.color = glm::vec4(1.0f);
    d.gpu.firstVertex = set == 0 ? rockFirst_ : chipFirst_;
    d.gpu.vertexCount = set == 0 ? rockCount_ : chipCount_;
    d.gpu.firstInstance = first;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Standard);
    d.gpu.sightAt = sight;
    d.instances = static_cast<uint32_t>(pieces.size());
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
  if (!streaks_.empty()) {
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), streaks_.begin(), streaks_.end());
    draw(quadFirst_, quadCount_, glm::mat4(1.0f), kModeStreak, now, 1.0f, 1.0f, glm::vec3(1.0f), 1.0f, glm::vec4(1.0f, 0.97f, 0.9f, 0.0f), sight,
         VfxBlend::Additive, first, static_cast<uint32_t>(streaks_.size()));
  }
}

void VanguardFx::buildPiece(const Piece& p, float now, glm::vec3 eye, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const Draw draw{frame};
  const float age = now - p.start;
  switch (p.shape) {
    case Shape::Sheet: {  // drawn in from a to c as the blade passes, held, then fading from the tail (WarriorFx's downwardCut, on any curve)
      const float hold = p.size, fade = p.size2;
      const float head = easeOut(0.0f, p.sweep, age);
      const float tail = ease(p.sweep * 0.5f, p.sweep + hold + fade, age);
      const float opacity = 1.0f - ease(p.sweep + hold, p.sweep + hold + fade, age);
      if (opacity <= 0.0f || head <= tail) return;
      constexpr int kSegments = 28;
      std::vector<VfxVertex> sheet;
      sheet.reserve(kSegments * 6);
      std::vector<glm::vec3> edge(kSegments + 1), inner(kSegments + 1);
      for (int j = 0; j <= kSegments; ++j) {
        const float s = static_cast<float>(j) / kSegments;
        const float w = p.width * std::pow(std::sin(kPi * s), 0.6f) + 0.004f;
        const glm::vec3 at = bezier(p.a, p.b, p.c, s);
        edge[j] = pulled(at, eye, kPull);
        inner[j] = pulled(at + p.thick * w, eye, kPull);
      }
      const glm::vec3 n = glm::normalize(glm::cross(p.c - p.a, p.thick) + glm::vec3(0.0f, 1e-4f, 0.0f));
      for (int j = 0; j < kSegments; ++j) {
        const float s0 = static_cast<float>(j) / kSegments, s1 = static_cast<float>(j + 1) / kSegments;
        const VfxVertex a0{inner[j], s0, n, 0.0f}, a1{edge[j], s0, n, 1.0f}, b0{inner[j + 1], s1, n, 0.0f}, b1{edge[j + 1], s1, n, 1.0f};
        sheet.insert(sheet.end(), {a0, a1, b0, a1, b1, b0});
      }
      draw(frame.addVertices(sheet), static_cast<uint32_t>(sheet.size()), glm::mat4(1.0f), kModeSlash, age, p.intensity, opacity, p.color, tail, glm::vec4(p.core, head),
           p.a, VfxBlend::Additive);
      // the edge's glint riding the cut, and its light
      if (age < p.sweep) {
        const glm::vec3 at = bezier(p.a, p.b, p.c, head);
        streaks_.push_back(streak(pulled(at, eye, kPull), glm::vec3(0.0f), 0.16f, 0.9f, kWhite, 2.4f));
        lights.push_back({at, {0.75f, 0.6f, 1.0f}, 7.0f, 2.4f});
      }
      return;
    }
    case Shape::Flash: {
      const float f = 1.0f - ease(0.0f, p.life, age);
      streaks_.push_back(streak(pulled(p.a, eye, kPull), glm::vec3(0.0f), p.size * (0.6f + 0.4f * f), f, p.color, 2.2f));
      return;
    }
    case Shape::Sparks: {
      const glm::vec3 dir = flatDir(p.thick, glm::vec3(0.0f, 0.0f, 1.0f));
      const glm::vec3 side = glm::normalize(glm::cross(kUp, dir));
      for (int i = 0; i < p.count; ++i) {
        const uint32_t n = static_cast<uint32_t>(i) + 100u;
        const float life = 0.16f + 0.24f * hash01(p.seed, n, 5);
        const float tau = age - hash01(p.seed, n, 6) * 0.04f;
        if (tau < 0.0f || tau > life) continue;
        const glm::vec3 d = glm::normalize(p.thick * (0.3f + 0.5f * hash01(p.seed, n, 1)) + side * (hash01(p.seed, n, 2) - 0.5f) * 2.0f + kUp * (0.1f + 0.7f * hash01(p.seed, n, 3)));
        const glm::vec3 v0 = d * p.speed * (0.7f + 0.6f * hash01(p.seed, n, 4));
        const glm::vec3 at = p.a + v0 * tau + glm::vec3(0.0f, -0.5f * kGravity * tau * tau, 0.0f);
        if (at.y < p.ground + 0.02f) continue;
        const float w = tau / life;
        streaks_.push_back(streak(pulled(at, eye, kPull), (v0 + glm::vec3(0.0f, -kGravity * tau, 0.0f)) * 0.05f, 0.03f, 1.0f - w * w,
                                  glm::mix(kVioletHot, glm::vec3(0.75f, 0.35f, 1.0f), w), 3.2f));
      }
      return;
    }
    case Shape::Ring: {
      const float r = p.size + (p.size2 - p.size) * easeOut(0.0f, std::min(p.life, 0.5f), age);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p.a, eye, kGroundPull)) * glm::scale(glm::mat4(1.0f), {std::max(r, 0.01f), 1.0f, std::max(r, 0.01f)});
      const float opacity = p.alpha * ease(0.0f, 0.05f, age) * (1.0f - ease(p.life * 0.55f, p.life, age));
      draw(discFirst_, discCount_, m, kModeRing, age, 0.45f, opacity, p.color, 0.0f, glm::vec4(glm::mix(p.color, kWhite, 0.5f), 0.88f), p.a, VfxBlend::Additive);
      return;
    }
    case Shape::Shell: {
      const float r = p.size + (p.size2 - p.size) * easeOut(0.0f, p.life * 0.8f, age);
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p.a, eye, kPull)) * glm::scale(glm::mat4(1.0f), glm::vec3(std::max(r, 0.01f)));
      const float opacity = p.alpha * (1.0f - ease(p.life * 0.25f, p.life, age));
      if (p.dark)
        draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 1.0f, opacity, kSmoke, 0.1f, glm::vec4(0.3f, 0.12f, 0.4f, 0.3f), p.a, VfxBlend::Alpha);
      else
        draw(sphereFirst_, sphereCount_, m, kModeEnergy, age, 0.5f, opacity, p.color, 0.05f, glm::vec4(glm::mix(p.color, kWhite, 0.6f), 0.0f), p.a, VfxBlend::Additive);
      return;
    }
    case Shape::Smoke: {
      for (int i = 0; i < p.count; ++i) {
        const uint32_t n = static_cast<uint32_t>(i) + 200u;
        const float life = p.life * (0.8f + 0.3f * hash01(p.seed, n, 1));
        const float tau = age - hash01(p.seed, n, 5) * 0.1f;
        if (tau < 0.0f || tau > life) continue;
        const float w = tau / life;
        const float a = (static_cast<float>(i) + 0.5f * hash01(p.seed, n, 2)) / static_cast<float>(std::max(p.count, 1)) * kTau;
        const glm::vec3 out{std::cos(a), 0.0f, std::sin(a)};
        const glm::vec3 at = p.a + out * (0.15f + p.size2 * easeOut(0.0f, 0.7f, w)) + kUp * (0.06f + 0.3f * w);
        const float alpha = p.alpha * ease(0.0f, 0.06f, tau) * (1.0f - ease(0.3f, 1.0f, w));
        const glm::vec3 colour = p.dark ? kSmoke * (0.8f + 0.4f * hash01(p.seed, n, 4)) : kDust * (0.85f + 0.3f * hash01(p.seed, n, 4));
        puffs_.push_back(puff(pulled(at, eye, kGroundPull), 0.28f + 0.55f * easeOut(0.0f, 1.0f, w), a + tau, hash01(p.seed, n, 3) * 37.0f, alpha, colour));
      }
      return;
    }
    case Shape::Line: {
      const float env = ease(0.0f, 0.04f, age) * (1.0f - ease(p.life * 0.5f, p.life, age));
      if (env <= 0.0f) return;
      const glm::vec3 a = pulled(p.a, eye, kPull), c = pulled(p.c, eye, kPull);
      streaks_.push_back(streak((a + c) * 0.5f, c - a, p.width, env, p.color, p.intensity));
      return;
    }
    case Shape::Sigil: {
      const float env = ease(0.0f, 0.15f, age) * (1.0f - ease(p.life - 0.4f, p.life, age));
      if (env <= 0.0f) return;
      const float r = p.size * (0.85f + 0.15f * easeOut(0.0f, 0.2f, age));
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(p.a, eye, kPull)) * glm::rotate(glm::mat4(1.0f), age * 1.9f, kUp) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(22.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(r));
      draw(arcFirst_, arcCount_, m, kModeTrail, age, 1.8f, 0.9f * env, p.color, 0.0f, glm::vec4(glm::mix(p.color, kWhite, 0.5f), 0.0f), p.a, VfxBlend::Additive);
      lights.push_back({p.a, p.color, 2.5f * env, 1.4f});
      return;
    }
    case Shape::Debris: {
      if (age > 1.6f) return;
      for (int i = 0; i < p.count; ++i) {
        const uint32_t n = static_cast<uint32_t>(i) + 300u;
        const float ang = hash01(p.seed, n, 1) * kTau;
        const glm::vec3 radial{std::cos(ang), 0.0f, std::sin(ang)};
        const glm::vec3 hdir = glm::normalize(p.thick + radial * 0.9f);
        const glm::vec3 vh = hdir * p.speed * (0.6f + 0.9f * hash01(p.seed, n, 3));
        const float vy = p.speed * (0.9f + 0.9f * hash01(p.seed, n, 4));
        const float drop = std::max(p.a.y - p.ground, 0.0f);
        const float t1 = (vy + std::sqrt(vy * vy + 2.0f * kGravity * drop)) / kGravity, vy2 = vy * 0.28f, t2 = 2.0f * vy2 / kGravity;
        glm::vec3 at;
        float spin = age;
        if (age < t1) at = p.a + vh * age + glm::vec3(0.0f, vy * age - 0.5f * kGravity * age * age, 0.0f);
        else if (age < t1 + t2) {
          const float s = age - t1;
          at = p.a + vh * t1 + vh * 0.4f * s + glm::vec3(0.0f, vy2 * s - 0.5f * kGravity * s * s, 0.0f);
          at.y = std::max(at.y, p.ground + 0.02f);
        } else {
          at = p.a + vh * t1 + vh * 0.4f * t2;
          at.y = p.ground + 0.02f;
          spin = t1 + t2;
        }
        const float fade = 1.0f - ease(1.1f, 1.6f, age);
        if (fade <= 0.0f) continue;
        const float sz = (p.steel ? 0.06f : 0.055f) * (0.7f + 0.6f * hash01(p.seed, n, 5)) * fade;
        const glm::vec3 axis = sphereDir(hash01(p.seed, n, 6), hash01(p.seed, n, 7));
        const glm::mat4 m = glm::translate(glm::mat4(1.0f), pulled(at, eye, kGroundPull)) *
                            glm::rotate(glm::mat4(1.0f), spin * (8.0f + 10.0f * hash01(p.seed, n, 8)), axis) * glm::scale(glm::mat4(1.0f), glm::vec3(sz));
        const glm::vec3 colour = p.steel ? glm::vec3(0.52f, 0.55f, 0.6f) : glm::vec3(0.34f, 0.3f, 0.26f);
        const VfxInstance inst{m, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(colour * (0.8f + 0.4f * hash01(p.seed, n, 9)), 1.0f)};
        (p.steel ? chips_ : rocks_).push_back(inst);
      }
      return;
    }
    case Shape::Light: {
      const float env = ease(0.0f, 0.04f, age) * (1.0f - ease(p.life * 0.4f, p.life, age));
      if (env > 0.0f) lights.push_back({p.a, p.color, p.intensity * env, p.size});
      return;
    }
    case Shape::Cross: {  // two bars of light crossed before the chest (Guarda Cruzada), flaring then held, a glint at the crossing
      const float env = ease(0.0f, 0.08f, age) * (1.0f - ease(p.life - 0.35f, p.life, age));
      if (env <= 0.0f) return;
      const glm::vec3 facing = flatDir(p.thick, glm::vec3(0.0f, 0.0f, 1.0f));
      const glm::vec3 side = glm::normalize(glm::cross(kUp, facing));
      const glm::vec3 at = pulled(p.a, eye, kPull);
      const float flare = 1.0f + 0.6f * (1.0f - ease(0.0f, 0.25f, age));
      streaks_.push_back(streak(at, (side + kUp) * (p.size * 0.7f), 0.05f, env, kVioletHot, 2.2f * flare));
      streaks_.push_back(streak(at, (side - kUp) * (p.size * 0.7f), 0.05f, env, kVioletHot, 2.2f * flare));
      streaks_.push_back(streak(at, glm::vec3(0.0f), 0.18f * flare, env * 0.8f, kWhite, 2.0f));
      return;
    }
  }
}

}  // namespace ember
