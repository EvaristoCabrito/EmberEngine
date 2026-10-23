#include "world/ProbeBake.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace ember {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// The 9 real spherical harmonics in the order and scale of shaders/atmosphere.hlsl shBasis (the sky's SH are built with it).
void shBasis(const glm::vec3& n, float b[9]) {
  b[0] = 0.282095f;
  b[1] = 0.488603f * n.y;
  b[2] = 0.488603f * n.z;
  b[3] = 0.488603f * n.x;
  b[4] = 1.092548f * n.x * n.y;
  b[5] = 1.092548f * n.y * n.z;
  b[6] = 0.315392f * (3.0f * n.z * n.z - 1.0f);
  b[7] = 1.092548f * n.x * n.z;
  b[8] = 0.546274f * (n.x * n.x - n.y * n.y);
}

// A bounding volume hierarchy over the room's triangles (median split on the widest centroid axis, 4 per leaf).
class Bvh {
 public:
  explicit Bvh(const std::vector<glm::vec3>& corners) {
    const size_t count = corners.size() / 3;
    tris_.resize(count);
    std::vector<uint32_t> order(count);
    std::vector<glm::vec3> centre(count);
    for (size_t t = 0; t < count; ++t) {
      Tri& tri = tris_[t];
      tri.a = corners[t * 3], tri.e1 = corners[t * 3 + 1] - tri.a, tri.e2 = corners[t * 3 + 2] - tri.a;
      const glm::vec3 n = glm::cross(tri.e1, tri.e2);
      tri.n = glm::length(n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f);
      centre[t] = tri.a + (tri.e1 + tri.e2) / 3.0f;
      order[t] = static_cast<uint32_t>(t);
    }
    nodes_.reserve(count * 2 + 1);
    nodes_.push_back({});
    build(0, order, centre, 0, static_cast<uint32_t>(count));
    std::vector<Tri> sorted(count);
    for (size_t i = 0; i < count; ++i) sorted[i] = tris_[order[i]];
    tris_ = std::move(sorted);
  }

  struct Hit {
    float t = 0.0f;
    glm::vec3 normal{0.0f};
    bool back = false;
  };
  // The nearest hit along the ray within (1e-4, far).
  bool closest(const glm::vec3& o, const glm::vec3& d, float far, Hit& hit) const { return trace(o, d, far, &hit); }
  bool occluded(const glm::vec3& o, const glm::vec3& d, float far) const { return trace(o, d, far, nullptr); }

 private:
  struct Tri {
    glm::vec3 a, e1, e2, n;
  };
  struct Node {
    glm::vec3 lo{0.0f}, hi{0.0f};
    uint32_t first = 0, count = 0;  // count > 0: a leaf over tris [first, first + count); else children first, first + 1
  };

  void build(uint32_t index, std::vector<uint32_t>& order, const std::vector<glm::vec3>& centre, uint32_t begin, uint32_t end) {
    glm::vec3 lo(1e30f), hi(-1e30f), clo(1e30f), chi(-1e30f);
    for (uint32_t i = begin; i < end; ++i) {
      const Tri& t = tris_[order[i]];
      for (const glm::vec3& p : {t.a, t.a + t.e1, t.a + t.e2}) lo = glm::min(lo, p), hi = glm::max(hi, p);
      clo = glm::min(clo, centre[order[i]]), chi = glm::max(chi, centre[order[i]]);
    }
    nodes_[index].lo = lo, nodes_[index].hi = hi;
    if (end - begin <= 4) {
      nodes_[index].first = begin, nodes_[index].count = end - begin;
      return;
    }
    const glm::vec3 extent = chi - clo;
    const int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
    const uint32_t mid = (begin + end) / 2;
    std::nth_element(order.begin() + begin, order.begin() + mid, order.begin() + end,
                     [&](uint32_t x, uint32_t y) { return centre[x][axis] < centre[y][axis]; });
    const uint32_t left = static_cast<uint32_t>(nodes_.size());
    nodes_.push_back({});
    nodes_.push_back({});
    nodes_[index].first = left, nodes_[index].count = 0;
    build(left, order, centre, begin, mid);
    build(left + 1, order, centre, mid, end);
  }

  static bool box(const Node& n, const glm::vec3& o, const glm::vec3& inv, float far) {
    const glm::vec3 t0 = (n.lo - o) * inv, t1 = (n.hi - o) * inv;
    const glm::vec3 tmin = glm::min(t0, t1), tmax = glm::max(t0, t1);
    const float enter = std::max({tmin.x, tmin.y, tmin.z, 0.0f}), leave = std::min({tmax.x, tmax.y, tmax.z, far});
    return enter <= leave;
  }

  bool trace(const glm::vec3& o, const glm::vec3& d, float far, Hit* hit) const {
    if (tris_.empty()) return false;
    const glm::vec3 inv(1.0f / (std::abs(d.x) > 1e-9f ? d.x : 1e-9f), 1.0f / (std::abs(d.y) > 1e-9f ? d.y : 1e-9f),
                        1.0f / (std::abs(d.z) > 1e-9f ? d.z : 1e-9f));
    std::array<uint32_t, 64> stack;
    uint32_t top = 0;
    stack[top++] = 0;
    bool found = false;
    while (top > 0) {
      const Node& n = nodes_[stack[--top]];
      if (!box(n, o, inv, far)) continue;
      if (n.count == 0) {
        if (top + 2 <= stack.size()) stack[top++] = n.first, stack[top++] = n.first + 1;
        continue;
      }
      for (uint32_t i = n.first; i < n.first + n.count; ++i) {
        const Tri& t = tris_[i];
        // Moller-Trumbore, both sides
        const glm::vec3 p = glm::cross(d, t.e2);
        const float det = glm::dot(t.e1, p);
        if (std::abs(det) < 1e-12f) continue;
        const float invDet = 1.0f / det;
        const glm::vec3 s = o - t.a;
        const float u = glm::dot(s, p) * invDet;
        if (u < 0.0f || u > 1.0f) continue;
        const glm::vec3 q = glm::cross(s, t.e1);
        const float v = glm::dot(d, q) * invDet;
        if (v < 0.0f || u + v > 1.0f) continue;
        const float dist = glm::dot(t.e2, q) * invDet;
        if (dist <= 1e-4f || dist >= far) continue;
        if (!hit) return true;
        far = dist, found = true;
        hit->t = dist, hit->normal = t.n, hit->back = glm::dot(t.n, d) > 0.0f;
      }
    }
    return found;
  }

  std::vector<Tri> tris_;
  std::vector<Node> nodes_;
};

// xorshift32
struct Rng {
  uint32_t s;
  float next() {
    s ^= s << 13, s ^= s >> 17, s ^= s << 5;
    return static_cast<float>(s >> 8) * (1.0f / 16777216.0f);
  }
};

glm::vec3 cosineAround(const glm::vec3& n, Rng& rng) {
  const float u = rng.next(), v = rng.next();
  const float r = std::sqrt(u), phi = 2.0f * kPi * v;
  const glm::vec3 t = std::abs(n.y) < 0.99f ? glm::normalize(glm::cross(n, glm::vec3(0, 1, 0))) : glm::vec3(1, 0, 0);
  const glm::vec3 b = glm::cross(n, t);
  return glm::normalize(t * (r * std::cos(phi)) + b * (r * std::sin(phi)) + n * std::sqrt(std::max(0.0f, 1.0f - u)));
}

}  // namespace

std::vector<glm::vec3> probeKeyDirections(uint32_t count) {
  std::vector<glm::vec3> out(count);
  const float golden = kPi * (3.0f - std::sqrt(5.0f));
  for (uint32_t i = 0; i < count; ++i) {
    const float y = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);  // 0 (the horizon) .. 1 (straight up)
    const float r = std::sqrt(std::max(0.0f, 1.0f - y * y)), a = golden * static_cast<float>(i);
    out[i] = {r * std::cos(a), y, r * std::sin(a)};
  }
  return out;
}

ProbeBakeOutput bakeProbes(const ProbeBakeInput& in, const std::function<void(uint32_t, const std::function<void(uint32_t, uint32_t)>&)>& parallel) {
  const auto started = std::chrono::steady_clock::now();
  ProbeBakeOutput out;
  out.probeCount = static_cast<uint32_t>(in.dims.x * in.dims.y * in.dims.z);
  out.keyCount = static_cast<uint32_t>(std::min<size_t>(in.keyDirections.size(), 64));
  out.valid.assign(out.probeCount, 0);
  out.transfer.assign(size_t{out.probeCount} * ProbeBakeOutput::kTransfer, 0.0f);
  out.key.assign(size_t{out.probeCount} * out.keyCount * ProbeBakeOutput::kKeyCoefficients, 0.0f);
  out.keyVisible.assign(out.probeCount, 0);
  out.fire.assign(size_t{out.probeCount} * ProbeBakeOutput::kFire, 0.0f);
  const Bvh bvh(in.triangles);
  constexpr float kFar = 1.0e4f, kLift = 2.0e-3f;
  const float weight = 4.0f * kPi / static_cast<float>(std::max(in.paths, 1u));
  const float golden = kPi * (3.0f - std::sqrt(5.0f));

  parallel(out.probeCount, [&](uint32_t begin, uint32_t end) {
    for (uint32_t index = begin; index < end; ++index) {
      if (index >= in.candidate.size() || !in.candidate[index]) continue;
      const int x = static_cast<int>(index % static_cast<uint32_t>(in.dims.x));
      const int z = static_cast<int>((index / static_cast<uint32_t>(in.dims.x)) % static_cast<uint32_t>(in.dims.z));
      const int y = static_cast<int>(index / static_cast<uint32_t>(in.dims.x * in.dims.z));
      const glm::vec3 p = in.origin + glm::vec3(x, y, z) * in.spacing;
      Rng rng{index * 9781u + 6271u};
      float* m = &out.transfer[size_t{index} * ProbeBakeOutput::kTransfer];
      float* key = &out.key[size_t{index} * out.keyCount * ProbeBakeOutput::kKeyCoefficients];
      float* fire = &out.fire[size_t{index} * ProbeBakeOutput::kFire];
      uint32_t backs = 0;
      const float spin = rng.next() * 2.0f * kPi;
      for (uint32_t k = 0; k < in.paths; ++k) {
        // stratified over the sphere (Fibonacci, turned per probe)
        const float yy = 1.0f - 2.0f * (static_cast<float>(k) + 0.5f) / static_cast<float>(in.paths);
        const float rr = std::sqrt(std::max(0.0f, 1.0f - yy * yy)), aa = golden * static_cast<float>(k) + spin;
        const glm::vec3 first{rr * std::cos(aa), yy, rr * std::sin(aa)};
        float yFirst[9];
        shBasis(first, yFirst);
        glm::vec3 o = p, d = first;
        float throughput = 1.0f;
        for (uint32_t bounce = 0;; ++bounce) {
          Bvh::Hit hit;
          if (!bvh.closest(o, d, kFar, hit)) {
            float yOut[9];
            shBasis(d, yOut);
            for (int i = 0; i < 9; ++i)
              for (int j = 0; j < 9; ++j) m[i * 9 + j] += weight * yFirst[i] * throughput * yOut[j];
            break;
          }
          if (hit.back) {
            if (bounce == 0) ++backs;
            break;  // inside a solid
          }
          const glm::vec3 at = o + d * hit.t + hit.normal * kLift;
          for (uint32_t j = 0; j < out.keyCount; ++j) {
            const float c = glm::dot(hit.normal, in.keyDirections[j]);
            if (c <= 0.0f || bvh.occluded(at, in.keyDirections[j], kFar)) continue;
            const float radiance = throughput * in.albedo / kPi * c;
            for (uint32_t i = 0; i < ProbeBakeOutput::kKeyCoefficients; ++i) key[j * ProbeBakeOutput::kKeyCoefficients + i] += weight * yFirst[i] * radiance;
          }
          // The fires this surface sees, as the renderer lights it with them (I / d^2 in the shader's window (1 - (d / range)^4)^2).
          for (const ProbeBakeInput::Fire& f : in.fires) {
            const glm::vec3 toFire = f.position - at;
            const float d = glm::length(toFire);
            if (d >= f.range || d < 1e-3f) continue;
            const glm::vec3 l = toFire / d;
            const float c = glm::dot(hit.normal, l);
            if (c <= 0.0f || bvh.occluded(at, l, d - 0.05f)) continue;
            const float ratio = d / f.range, window = std::clamp(1.0f - ratio * ratio * ratio * ratio, 0.0f, 1.0f);
            const glm::vec3 radiance = f.color * (throughput * in.albedo / kPi * c * window * window / std::max(d * d, 0.01f));
            for (int i = 0; i < 9; ++i)
              for (int ch = 0; ch < 3; ++ch) fire[i * 3 + ch] += weight * yFirst[i] * radiance[ch];
          }
          if (bounce + 1 >= in.bounces) break;
          throughput *= in.albedo;
          o = at;
          d = cosineAround(hit.normal, rng);
        }
      }
      // A probe whose rays leave mostly through back faces stands inside a wall, the ground or the roof.
      if (static_cast<float>(backs) > 0.2f * static_cast<float>(in.paths)) {
        std::fill(m, m + ProbeBakeOutput::kTransfer, 0.0f);
        std::fill(key, key + out.keyCount * ProbeBakeOutput::kKeyCoefficients, 0.0f);
        std::fill(fire, fire + ProbeBakeOutput::kFire, 0.0f);
        continue;
      }
      out.valid[index] = 1;
      uint64_t visible = 0;
      for (uint32_t j = 0; j < out.keyCount; ++j)
        if (!bvh.occluded(p, in.keyDirections[j], kFar)) visible |= uint64_t{1} << j;
      out.keyVisible[index] = visible;
    }
  });
  out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  return out;
}

namespace {
constexpr uint32_t kCacheVersion = 2;  // the bake's own numbers or the file's layout changed: a new number
struct Fnv {
  uint64_t h = 1469598103934665603ull;
  void bytes(const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
  }
  template <class T>
  void pod(const T& v) { bytes(&v, sizeof(v)); }
  template <class T>
  void vec(const std::vector<T>& v) {
    pod(v.size());
    if (!v.empty()) bytes(v.data(), v.size() * sizeof(T));
  }
};
}  // namespace

uint64_t probeBakeKey(const ProbeBakeInput& in) {
  Fnv f;
  f.pod(kCacheVersion);
  f.vec(in.triangles);
  f.pod(in.origin), f.pod(in.spacing), f.pod(in.dims);
  f.vec(in.candidate);
  f.vec(in.keyDirections);
  // The fires by their place to half a unit and their strength to a percent: a card's flame leans with the camera, and the bounce
  // that far does not change (the same room must find its bake from any view).
  for (const ProbeBakeInput::Fire& fire : in.fires) {
    const glm::ivec3 at = glm::ivec3(glm::round(fire.position * 2.0f));
    const glm::ivec3 strength = glm::ivec3(glm::round(glm::log2(glm::max(fire.color, glm::vec3(1e-6f))) * 64.0f));
    f.pod(at), f.pod(strength), f.pod(static_cast<int>(std::round(fire.range * 4.0f)));
  }
  f.pod(in.albedo), f.pod(in.paths), f.pod(in.bounces);
  return f.h;
}

bool loadProbeBake(const std::string& path, uint64_t key, ProbeBakeOutput& out) {
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (!file) return false;
  bool ok = false;
  uint32_t version = 0, probes = 0, keys = 0;
  uint64_t stored = 0;
  if (std::fread(&version, 4, 1, file) == 1 && std::fread(&stored, 8, 1, file) == 1 && std::fread(&probes, 4, 1, file) == 1 &&
      std::fread(&keys, 4, 1, file) == 1 && version == kCacheVersion && stored == key) {
    out.probeCount = probes, out.keyCount = keys;
    out.valid.resize(probes), out.keyVisible.resize(probes);
    out.transfer.resize(size_t{probes} * ProbeBakeOutput::kTransfer);
    out.key.resize(size_t{probes} * keys * ProbeBakeOutput::kKeyCoefficients);
    out.fire.resize(size_t{probes} * ProbeBakeOutput::kFire);
    ok = std::fread(out.valid.data(), 1, out.valid.size(), file) == out.valid.size() &&
         std::fread(out.keyVisible.data(), 8, out.keyVisible.size(), file) == out.keyVisible.size() &&
         std::fread(out.transfer.data(), 4, out.transfer.size(), file) == out.transfer.size() &&
         std::fread(out.key.data(), 4, out.key.size(), file) == out.key.size() &&
         std::fread(out.fire.data(), 4, out.fire.size(), file) == out.fire.size();
  }
  std::fclose(file);
  out.seconds = 0.0;
  return ok;
}

void saveProbeBake(const std::string& path, uint64_t key, const ProbeBakeOutput& out) {
  const std::string temp = path + ".tmp";
  std::FILE* file = std::fopen(temp.c_str(), "wb");
  if (!file) return;
  const uint32_t version = kCacheVersion;
  std::fwrite(&version, 4, 1, file);
  std::fwrite(&key, 8, 1, file);
  std::fwrite(&out.probeCount, 4, 1, file);
  std::fwrite(&out.keyCount, 4, 1, file);
  std::fwrite(out.valid.data(), 1, out.valid.size(), file);
  std::fwrite(out.keyVisible.data(), 8, out.keyVisible.size(), file);
  std::fwrite(out.transfer.data(), 4, out.transfer.size(), file);
  std::fwrite(out.key.data(), 4, out.key.size(), file);
  std::fwrite(out.fire.data(), 4, out.fire.size(), file);
  const bool written = std::fflush(file) == 0;
  std::fclose(file);
  std::remove(path.c_str());
  if (!written || std::rename(temp.c_str(), path.c_str()) != 0) std::remove(temp.c_str());
}

}  // namespace ember
