#include "render/vfx/DreamWeb.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPi;
// DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS (Ember's), the thread a third thicker: the board is seen from further than Ember's tile view.
constexpr float kRadius = 1, kFilamentThickness = 0.027f * 1.35f, kCurvature = 0.42f, kVerticalSpread = 1.05f, kDisplacement = 0.055f;
constexpr int kTargetFilamentCount = 7, kNodeCount = 12, kSecondaryLightCount = 2;
constexpr float kPulseSpeed = 1.25f, kPulseBrightness = 2.8f, kNodeSize = 0.075f, kTightening = 0.3f;
constexpr float kCentralLight = 12, kCentralLightRadius = 5.5f, kSecondaryLight = 6, kSecondaryLightRadius = 4.2f, kBindingFlash = 20, kSustained = 2.2f;
constexpr uint32_t kSeed = 50827;
constexpr int kTubeSides = 5, kTubeSegments = 18;
// Ours: the net hangs as a canopy at body height (Ember's z 0 was its flat card plane; half a net under our floor is gone).
constexpr float kLift = 0.85f;

// JavaScript's Math.round (halves round up).
float jsRound(float x) { return std::floor(x + 0.5f); }

// One tube: `centres` rings with their frames, tapered like tubeBundle (0.4 + 0.6 sin(pi t)); u = aAlong, v packs aDelay and aPhase.
void emitTube(std::vector<VfxVertex>& out, const std::vector<glm::vec3>& centres, const std::vector<glm::vec3>& normals,
              const std::vector<glm::vec3>& binormals, float radius, float delay, float phase, int sides) {
  const int rows = static_cast<int>(centres.size()) - 1;
  if (rows < 1) return;
  const float packed = delay + 2.0f * jsRound(phase / kTau * 1000.0f);
  std::vector<VfxVertex> grid;
  grid.reserve(static_cast<size_t>((rows + 1) * (sides + 1)));
  for (int row = 0; row <= rows; ++row) {
    const float t = static_cast<float>(row) / static_cast<float>(rows);
    const float taper = radius * (0.4f + 0.6f * std::sin(kPi * t));
    for (int side = 0; side <= sides; ++side) {
      const float angle = static_cast<float>(side) / static_cast<float>(sides) * kTau;
      const glm::vec3 radial = normals[static_cast<size_t>(row)] * std::cos(angle) + binormals[static_cast<size_t>(row)] * std::sin(angle);
      grid.push_back({centres[static_cast<size_t>(row)] + radial * taper, t, radial, packed});
    }
  }
  for (int row = 0; row < rows; ++row)
    for (int side = 0; side < sides; ++side) {
      const size_t a = static_cast<size_t>(row * (sides + 1) + side), b = a + static_cast<size_t>(sides) + 1;
      out.insert(out.end(), {grid[a], grid[b], grid[a + 1], grid[b], grid[b + 1], grid[a + 1]});
    }
}

// tubeBundle(strands, thickness): every strand a tapered tube along its centripetal curve (Frenet frames).
std::vector<VfxVertex> tubeBundle(const std::vector<DreamWeb::Strand>& strands, float thickness, int segments, int sides) {
  std::vector<VfxVertex> out;
  for (const DreamWeb::Strand& strand : strands) {
    if (strand.points.size() < 2) continue;
    const three::CatmullRom curve{strand.points};
    std::vector<glm::vec3> tangents, normals, binormals;
    three::frenetFrames(curve, segments, tangents, normals, binormals);
    const std::vector<float> lengths = curve.getLengths();
    std::vector<glm::vec3> centres;
    for (int row = 0; row <= segments; ++row) centres.push_back(three::getPointAt(curve, static_cast<float>(row) / static_cast<float>(segments), lengths));
    emitTube(out, centres, normals, binormals, thickness * strand.width, strand.delay, strand.phase, sides);
  }
  return out;
}

// A tube along a polyline (the shot's strands, rebuilt every frame): frames by projecting the previous normal.
void polylineTube(std::vector<VfxVertex>& out, const std::vector<glm::vec3>& centres, float radius, float phase, int sides) {
  const size_t n = centres.size();
  if (n < 2) return;
  std::vector<glm::vec3> normals(n), binormals(n);
  glm::vec3 previous{0.0f};
  for (size_t i = 0; i < n; ++i) {
    const glm::vec3 t = three::normalize(centres[std::min(i + 1, n - 1)] - centres[i > 0 ? i - 1 : 0]);
    glm::vec3 normal;
    if (i == 0) {
      normal = three::normalize(glm::cross(t, std::abs(t.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
    } else {
      normal = previous - t * glm::dot(previous, t);
      normal = glm::length(normal) > 1e-5f ? glm::normalize(normal) : three::normalize(glm::cross(t, glm::vec3(0, 1, 0)));
    }
    previous = normal;
    normals[i] = normal;
    binormals[i] = glm::cross(t, normal);
  }
  emitTube(out, centres, normals, binormals, radius, 0.0f, phase, sides);
}

const std::vector<VfxVertex>& knotGeometry() { static const auto g = vfxgeo::icosahedron(0.5f, 1); return g; }
const std::vector<VfxVertex>& shardGeometry() { static const auto g = vfxgeo::tetrahedron(0.5f, 0); return g; }

// The orb web laid on a hex (Ember frame, z up, hex circumradius 1, corners at 30 + 60 k degrees): six spokes and three
// scalloped rings, each ring thread sagging inward between two spokes. Two variants so neighbours differ.
std::vector<VfxVertex> orbWeb(uint32_t seed) {
  three::Mulberry random{seed};
  std::vector<DreamWeb::Strand> strands;
  glm::vec3 corner[6];
  for (int k = 0; k < 6; ++k) {
    const float angle = (30.0f + 60.0f * static_cast<float>(k)) * kPi / 180.0f;
    corner[k] = {std::cos(angle), std::sin(angle), 0.0f};
  }
  for (int k = 0; k < 6; ++k) {
    DreamWeb::Strand s;
    for (float f : {0.0f, 0.3f, 0.62f, 0.93f}) s.points.push_back(corner[k] * f + glm::vec3(0, 0, 0.012f + (random.next() - 0.5f) * 0.01f));
    s.delay = random.next() * 0.1f, s.phase = random.next() * 6.28f, s.width = 1.0f;
    strands.push_back(s);
  }
  for (int ring = 0; ring < 3; ++ring) {
    const float radius = 0.3f + 0.24f * static_cast<float>(ring) + (random.next() - 0.5f) * 0.05f;
    for (int k = 0; k < 6; ++k) {
      const glm::vec3 a = corner[k] * radius, b = corner[(k + 1) % 6] * radius;
      const glm::vec3 mid = (a + b) * 0.5f * (0.84f + random.next() * 0.06f);
      DreamWeb::Strand s;
      s.points = {a + glm::vec3(0, 0, 0.012f), mid + glm::vec3(0, 0, 0.004f), b + glm::vec3(0, 0, 0.012f)};
      s.delay = 0.28f + 0.2f * static_cast<float>(ring) + random.next() * 0.08f;
      s.phase = random.next() * 6.28f;
      s.width = 0.75f;
      strands.push_back(s);
    }
  }
  return tubeBundle(strands, kFilamentThickness * 0.9f, 6, 4);
}

// A unit's own strands (updateTargets), in a frame where x, y are across and z is up: three coils wound round the body and
// loose threads over it. Ember's vertical was its y (a flat card's up); here it is the body's own up, and the winding is round
// (circular) so it reads the same from every side of the orbiting camera. Body height 1.5 = 1.28 of strand length.
std::vector<VfxVertex> wrapStrands(int key) {
  uint32_t sum = static_cast<uint32_t>(key) * 131u + 7u;
  three::Mulberry r{kSeed + sum};
  std::vector<DreamWeb::Strand> strands;
  const int wraps = std::min(3, std::max(1, static_cast<int>(jsRound(kTargetFilamentCount / 3.0f))));
  for (int wrap = 0; wrap < wraps; ++wrap) {
    DreamWeb::Strand s;
    const float offset = static_cast<float>(wrap) / static_cast<float>(wraps) * kTau + r.next() * 0.28f;
    for (int step = 0; step <= 12; ++step) {
      const float t = static_cast<float>(step) / 12.0f;
      const float angle = offset + t * kTau * 1.15f;
      const float radius = 0.3f + std::sin(t * kPi) * 0.07f;
      s.points.push_back({std::cos(angle) * radius, std::sin(angle) * radius, -0.5f + t * 1.28f + (r.next() - 0.5f) * 0.04f});
    }
    s.delay = 0.12f + static_cast<float>(wrap) * 0.08f;
    s.phase = r.next() * 6.28f;
    s.width = 1.25f;
    strands.push_back(s);
  }
  const int n = std::max(3, std::min(12, kTargetFilamentCount));
  for (int i = 0; i < n; ++i) {
    const float angle = static_cast<float>(i) / static_cast<float>(n) * kTau + r.next() * 0.24f;
    const float radius = 0.22f + r.next() * 0.18f;
    const float sy = -0.38f + r.next() * 0.2f;
    const float oy = 0.12f + r.next() * 0.58f;
    const float fy = -0.12f + r.next() * 0.68f;
    const float ey = -0.42f + r.next() * 0.9f;
    auto on = [&](float turn, float scale, float z) { return glm::vec3(std::cos(angle + turn) * radius * scale, std::sin(angle + turn) * radius * scale, z); };
    const glm::vec3 a = on(0.0f, 1.0f, sy), over = on(1.15f, 0.6f, oy), far = on(2.55f, 1.0f, fy), end = on(kPi, 0.82f, ey);
    DreamWeb::Strand s;
    s.points = {a, glm::mix(a, over, 0.6f), over, far, end};
    s.delay = 0.52f + r.next() * 0.42f;
    s.phase = r.next() * 6.28f;
    s.width = 0.72f + r.next() * 0.62f;
    strands.push_back(s);
  }
  return tubeBundle(strands, kFilamentThickness * 1.2f, kTubeSegments, kTubeSides);
}

}  // namespace

DreamWeb::DreamWeb(glm::vec3 casterBody, glm::vec3 centre, std::vector<glm::vec3> hexes, int radius, std::vector<Occupant> occupants, float start,
                   bool standing)
    : caster_(casterBody), centre_(centre), hexes_(std::move(hexes)), radius_(std::clamp(radius, 1, 3)), start_(start), standing_(standing) {
  releaseAt_ = standing ? 1e9f : landed() + kPreviewHold;
  netCentre_ = vfxToEmber(centre_ + glm::vec3(0.0f, kLift, 0.0f));
  // WebZoneVFX: the zone's footprint is webZoneRadiusTiles(radius) = radius x sqrt 3 + 1 tiles; the seeds and threads grow with it.
  const float radiusTiles = static_cast<float>(radius_) * std::sqrt(3.0f) + 1.0f;
  three::Mulberry random{static_cast<uint32_t>(kSeed + static_cast<int>(jsRound(netCentre_.x * 11.0f)) + static_cast<int>(jsRound(netCentre_.y * 7.0f)))};
  extent_ = std::max(0.7f, radiusTiles * 0.95f) * kRadius;
  const int count = std::clamp(8 + 2 * (radius_ - 1), 4, 12);
  for (int i = 0; i < count; ++i) {
    // The anchor knots around the perimeter, so the spell reads as a net.
    const float angle = static_cast<float>(i) / static_cast<float>(count) * kTau + (random.next() - 0.5f) * 0.12f;
    const float r = extent_ * (0.9f + random.next() * 0.08f);
    const float z = (random.next() - 0.5f) * kVerticalSpread * 1.45f;
    nodes_.push_back({std::cos(angle) * r, std::sin(angle) * r * 0.72f, z});
    phase_.push_back(random.next() * kTau);
  }
  std::vector<Strand> network;
  const int filamentCount = std::clamp(24 + 8 * (radius_ - 1), 6, 40);
  auto addThread = [&](int from, int to, float delay, float width) {
    const glm::vec3 a = nodes_[static_cast<size_t>(from)], b = nodes_[static_cast<size_t>(to)];
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
    const int from = static_cast<int>(std::floor(random.next() * static_cast<float>(count)));
    const int offset = 2 + static_cast<int>(std::floor(random.next() * static_cast<float>(std::max(1, count - 3))));
    const float delay = 0.22f + random.next() * 0.2f;
    const float width = 0.62f + random.next() * 0.18f;
    connect(from, (from + offset) % count, delay, width);
  }
  // Thin, incomplete tendrils reach beyond a few knots and fade into empty space.
  for (int i = 0; i < std::min(5, filamentCount / 5); ++i) {
    const glm::vec3 a = nodes_[static_cast<size_t>((i * 3 + 1) % count)];
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
  net_ = tubeBundle(network, kFilamentThickness, kTubeSegments, kTubeSides);
  floor_[0] = orbWeb(kSeed + 11u);
  floor_[1] = orbWeb(kSeed + 29u);
  setOccupants(occupants);
}

void DreamWeb::setOccupants(const std::vector<Occupant>& units) {
  std::map<int, Wrap> next;
  for (const Occupant& o : units) {
    const auto it = wraps_.find(o.key);
    Wrap w = it != wraps_.end() ? std::move(it->second) : Wrap{wrapStrands(o.key), o};
    w.at = o;
    next[o.key] = std::move(w);
  }
  wraps_ = std::move(next);
}

// WEB_SHOT: nine strands, apart at the tail and converging on the head, over the current length of the beam.
void DreamWeb::buildShot(float now, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float flight = now - start_;
  if (flight <= 0.0f) return;
  const float k = std::min(1.0f, flight / kShotTravel);
  const float fade = 1.0f - three::smoothstep(now - landed(), 0.0f, 0.2f);
  if (fade <= 0.01f || k < 0.03f) return;
  const glm::vec3 up{0.0f, 1.0f, 0.0f};
  const glm::vec3 a = caster_, b = centre_ + up * 0.55f;
  const glm::vec3 delta = b - a;
  const float length = glm::length(delta);
  const glm::vec3 dir = length > 1e-3f ? delta / length : glm::vec3(0, 0, 1);
  glm::vec3 side = glm::cross(dir, up);
  side = glm::length(side) > 0.05f ? glm::normalize(side) : glm::vec3(1, 0, 0);
  const glm::vec3 lift = glm::normalize(glm::cross(side, dir));
  const float arc = 0.1f * length + 0.15f;
  auto pathAt = [&](float s) {
    const float t = s * k;
    return a + delta * t + up * (arc * std::sin(kPi * t));
  };
  const float seed = std::fmod(std::abs(centre_.x * 3.1f + centre_.z * 1.7f), 6.0f);
  constexpr int kStrands = 9, kRows = 16;
  std::vector<VfxVertex> tubes;
  for (int i = 0; i < kStrands; ++i) {
    const float lane = static_cast<float>(i) / 8.0f * 2.0f - 1.0f;
    std::vector<glm::vec3> centres;
    for (int row = 0; row <= kRows; ++row) {
      const float s = static_cast<float>(row) / kRows, x = -1.0f + 2.0f * s;
      const float spread = glm::mix(0.85f, 0.06f, three::smoothstep(x, -1.0f, 0.7f));
      const float wobble = std::sin(x * 6.0f + lane * kTau + seed + static_cast<float>(i)) * 0.12f * spread;
      const float lateral = (lane * spread + wobble) * 0.4f;
      const float depth = std::cos(x * 5.0f + static_cast<float>(i) * 1.7f + seed) * 0.5f * spread * 0.3f;
      centres.push_back(pathAt(s) + side * lateral + lift * depth);
    }
    polylineTube(tubes, centres, kFilamentThickness * 1.5f, static_cast<float>(i) * 0.7f + 0.3f, kTubeSides);
  }
  const glm::vec3 head = pathAt(1.0f);
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, glm::vec4 p0, glm::vec4 p1, glm::vec4 color, VfxBlend blend,
                  uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) {
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = p0;
    d.gpu.params1 = p1;
    d.gpu.color = color;
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(material);
    d.gpu.sightAt = head;
    d.instances = instances;
    d.blend = blend;
    d.depthTest = true;
    frame.draws.push_back(d);
  };
  draw(VfxMaterial::WebThread, frame.addVertices(tubes), tubes.size(), {flight, 2.4f, 2.0f, 0.0f}, {0.03f, 1.5f, 0.95f * fade, 0.0f}, {}, VfxBlend::Additive);
  // The knot at the head: a few gems tumbling round it.
  std::vector<VfxInstance> gems;
  for (int j = 0; j < 4; ++j) {
    const float turn = flight * 9.0f + static_cast<float>(j) * kPi * 0.5f;
    const glm::vec3 p = head + side * (std::cos(turn) * 0.1f) + lift * (std::sin(turn) * 0.1f);
    const float size = (0.07f + 0.02f * static_cast<float>(j % 2)) * fade;
    gems.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(turn, turn * 0.7f, static_cast<float>(j)) * glm::scale(glm::mat4(1.0f), glm::vec3(size))});
  }
  const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
  frame.instances.insert(frame.instances.end(), gems.begin(), gems.end());
  draw(VfxMaterial::Standard, frame.addVertices(knotGeometry()), knotGeometry().size(), {0.25f, 0.08f, 0, 0},
       glm::vec4(three::hex(0x7959ff), 3.2f), glm::vec4(three::hex(0xb8a2ff), 0.95f * fade), VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(gems.size()));
  lights.push_back({head, glm::vec3(0.59f, 0.23f, 0.84f), 7.0f * fade, 3.6f});
}

void DreamWeb::build(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) {
  if (now < start_ || done(now)) return;
  buildShot(now, frame, lights);
  if (now < landed()) return;
  // The field's own clock stops while it releases (Ember: elapsed frozen, releaseElapsed runs).
  const bool releasing = now >= releaseAt_;
  const float e = std::min(now, releaseAt_) - landed();
  const float release = releasing ? three::smoothstep(now - releaseAt_, 0.0f, kRelease) : 0.0f;
  const float formation = std::min(2.0f, e / kFormation);
  const float bind = std::exp(-std::pow((e - 1.42f) / 0.13f, 2.0f));
  const float sustained = e > kFormation ? kSustained : 0.0f;
  const float live = releasing ? 1.0f - release : 1.0f;
  if (live <= 0.01f) return;
  const glm::mat4 toWorld = vfxEmberToWorld();
  const glm::mat4 group = toWorld * glm::translate(glm::mat4(1.0f), netCentre_);
  const float tightening = kTightening * three::smoothstep(e, 1.04f, 1.48f);
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec3 sight, glm::vec4 p0, glm::vec4 p1,
                  glm::vec4 color, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) {
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

  // --- The floor: an orb web on every hex of the zone, growing outward from the middle ---
  {
    const uint32_t first[2] = {frame.addVertices(floor_[0]), frame.addVertices(floor_[1])};
    for (const glm::vec3& hex : hexes_) {
      const float rings = std::round(glm::length(glm::vec2(hex.x - centre_.x, hex.z - centre_.z)) / std::sqrt(3.0f));
      const float grown = std::min(2.0f, (e - 0.1f * rings) / 0.6f);
      if (grown <= 0.0f) continue;
      const int variant = ((static_cast<int>(std::floor(hex.x * 3.0f)) * 7) ^ (static_cast<int>(std::floor(hex.z * 5.0f)) * 13)) & 1;
      const glm::mat4 model = glm::translate(glm::mat4(1.0f), hex + glm::vec3(0.0f, 0.05f, 0.0f)) * toWorld * glm::scale(glm::mat4(1.0f), glm::vec3(0.95f));
      draw(VfxMaterial::WebThread, first[variant], floor_[variant].size(), model, hex, {e, kPulseSpeed, grown, 0.0f},
           {0.012f, 0.85f * (1.0f + bind * 0.9f), 0.85f * live, 0.0f}, {}, VfxBlend::Additive);
    }
  }

  // --- The net, hung over the zone ---
  draw(VfxMaterial::WebThread, frame.addVertices(net_), net_.size(), group, centre_, {e, kPulseSpeed, formation, tightening},
       {kDisplacement, 0.9f * (1.0f + bind * kPulseBrightness * 0.42f), 0.9f * live, 0}, {}, VfxBlend::Additive);

  // --- The knots (updateNodes) ---
  {
    std::vector<VfxInstance> knots;
    const int count = std::max(2, std::min(static_cast<int>(nodes_.size()), kNodeCount));
    for (int i = 0; i < count; ++i) {
      const glm::vec3 p = nodes_[static_cast<size_t>(i) % nodes_.size()];
      const float pulse = 1.0f + 0.18f * std::sin(e * 2.2f + phase_[static_cast<size_t>(i)]);
      const float tighten = 1.0f - kTightening * 0.24f * three::smoothstep(e, 1.08f, 1.5f);
      const float size = kNodeSize * pulse * tighten;
      knots.push_back({glm::translate(glm::mat4(1.0f), {p.x, p.y, p.z + std::sin(e * 1.6f + phase_[static_cast<size_t>(i)]) * 0.035f}) *
                       three::eulerXYZ(e * 0.24f + phase_[static_cast<size_t>(i)], e * 0.31f, phase_[static_cast<size_t>(i)] * 0.4f) *
                       glm::scale(glm::mat4(1.0f), {size * (1.0f + std::sin(e * 1.3f + static_cast<float>(i)) * 0.14f), size, size * 0.74f})});
    }
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), knots.begin(), knots.end());
    draw(VfxMaterial::Standard, frame.addVertices(knotGeometry()), knotGeometry().size(), group, centre_, {0.25f, 0.08f, 0, 0},
         glm::vec4(three::hex(0x7959ff), 1.8f + std::exp(-std::pow((e - 1.42f) / 0.14f, 2.0f)) * 4.0f), glm::vec4(three::hex(0xb8a2ff), 0.92f * live),
         VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(knots.size()));
  }

  // --- Drifting shards (updateParticles) ---
  {
    std::vector<VfxInstance> shards;
    const int count = std::min(120, static_cast<int>(nodes_.size()) * 4);
    for (int i = 0; i < count; ++i) {
      const float phase = phase_[static_cast<size_t>(i) % phase_.size()] + static_cast<float>(i) * 0.31f;
      const float radius = (0.28f + static_cast<float>((i * 37) % 100) / 100.0f * 0.72f) * (extent_ / 2.6f);
      const float angle = phase + e * (0.16f + static_cast<float>(i % 5) * 0.035f);
      const float pull = three::smoothstep(e, 0.8f, 1.45f) * 0.58f;
      const glm::vec3 p{std::cos(angle) * radius * (1.0f - pull), std::sin(angle * 1.17f) * radius * 0.75f * (1.0f - pull),
                        std::sin(angle * 0.73f + phase) * kVerticalSpread * 0.45f};
      const float size = (0.018f + static_cast<float>(i % 4) * 0.006f) * (0.7f + 0.3f * std::sin(e * 3.0f + phase));
      shards.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(angle, angle * 0.7f, phase) * glm::scale(glm::mat4(1.0f), glm::vec3(size))});
    }
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), shards.begin(), shards.end());
    draw(VfxMaterial::Standard, frame.addVertices(shardGeometry()), shardGeometry().size(), group, centre_, {0.38f, 0.05f, 0, 0},
         glm::vec4(three::hex(0x8061ff), 1.25f), glm::vec4(three::hex(0xb9a5ff), live), VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(shards.size()));
  }

  // --- Each unit inside: its own strands wound round its body (renderOrder 4) ---
  for (const auto& [key, w] : wraps_) {
    const float scale = std::clamp(w.at.height / 1.5f, 0.7f, 2.4f);
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), w.at.feet + glm::vec3(0.0f, w.at.height * 0.46f, 0.0f)) * toWorld *
                            glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    draw(VfxMaterial::WebThread, frame.addVertices(w.strands), w.strands.size(), model, w.at.feet, {e, kPulseSpeed, formation, tightening},
         {kDisplacement, 1.1f * (1.0f + bind * kPulseBrightness * 0.5f), 0.98f * live, 0}, {}, VfxBlend::Additive);
  }

  // --- Lights: the central one forming, flashing on the bind, sustained; two travelling with the pulses ---
  const float formationLight = kCentralLight * three::smoothstep(e, 0.02f, 0.54f);
  lights.push_back({centre_ + glm::vec3(0.0f, 0.7f, 0.0f), three::hex(0x8667ff), (formationLight + bind * kBindingFlash + sustained) * live,
                    kCentralLightRadius * std::max(1.0f, extent_ / 2.6f)});
  for (int i = 0; i < kSecondaryLightCount; ++i) {
    const size_t k = static_cast<size_t>(i * 3 + 1) % nodes_.size();
    const float travelPulse = 0.5f + 0.5f * std::sin(e * kPulseSpeed + phase_[k]);
    lights.push_back({vfxFromEmber(netCentre_ + nodes_[k] + glm::vec3(0, 0, 0.16f)), three::hex(0x9276ff),
                      (kSecondaryLight * (0.35f + travelPulse * 0.65f) + bind * kBindingFlash * 0.35f + sustained * 0.28f) * live, kSecondaryLightRadius});
  }
}

}  // namespace ember
