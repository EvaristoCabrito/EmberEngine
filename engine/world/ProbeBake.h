#pragma once

// Diffuse light probes for a room lit from outside (Luz das janelas), after Final Fantasy XV's ambient system ("Rendering
// Techniques of Final Fantasy XV", Elcott, Chang, Miyamoto, Metaaphanon, SIGGRAPH 2016 Talks): the sky's light reaches a room's
// probes through Precomputed Radiance Transfer (Sloan, Kautz, Snyder 2002), computed here by our own path tracer, so the hour's real
// sky (whatever the time of day and the weather) lights the room through its openings and its bounces at run time with one 9 x 9
// matrix per probe. The key light (the sun, the moon) adds its bounce: for a set of key-light directions, how much of it reaches
// each probe after one or more bounces off the room (the direct beam itself is the shadow map's), interpolated at run time
// along the key light's real direction.
//
// Units: the transfer maps the environment's radiance (9 spherical harmonics) to the radiance arriving at the probe (9 spherical
// harmonics); the key table maps a unit irradiance from the key light (on a surface facing it) to the radiance arriving at the probe
// (4 spherical harmonics, bands 0-1).

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace ember {

struct ProbeBakeInput {
  std::vector<glm::vec3> triangles;  // the room's surfaces, three corners each (walls, the roof slab, the ground)
  glm::vec3 origin{0.0f};            // the first probe's position
  glm::vec3 spacing{1.0f};
  glm::ivec3 dims{0};
  std::vector<uint8_t> candidate;    // per probe (x fastest, then z, then y): 1 where a probe may stand (inside the room)
  std::vector<glm::vec3> keyDirections;  // towards the key light
  // The room's fires (torches, braziers, candles: lights that do not move), whose light bounces off the room into the probes, as
  // Final Fantasy XV's irradiance volumes did for its static local lights. `color` = candela per channel; `range` = where it ends.
  struct Fire {
    glm::vec3 position{0.0f}, color{0.0f};
    float range = 0.0f;
  };
  std::vector<Fire> fires;
  float albedo = 0.3f;               // the surfaces' diffuse reflectance (dressed stone: 0.2-0.4)
  uint32_t paths = 160;              // per probe
  uint32_t bounces = 3;
};

struct ProbeBakeOutput {
  static constexpr uint32_t kTransfer = 81, kKeyCoefficients = 4, kFire = 27;
  uint32_t probeCount = 0, keyCount = 0;
  std::vector<uint8_t> valid;        // per probe: 1 = in the room, not inside a solid
  std::vector<float> transfer;       // per probe: kTransfer (row i = the probe's coefficient, column k = the sky's)
  std::vector<float> key;            // per probe: keyCount x kKeyCoefficients
  std::vector<float> fire;           // per probe: the fires' bounced radiance, 9 coefficients x rgb (candela-based: exposure not in)
  std::vector<uint64_t> keyVisible;  // per probe: bit j = the key light in direction j reaches the probe's point straight
  double seconds = 0.0;
};

// `parallel(count, body)` runs body(begin, end) over [0, count) (the job system's parallelFor).
ProbeBakeOutput bakeProbes(const ProbeBakeInput& input, const std::function<void(uint32_t, const std::function<void(uint32_t, uint32_t)>&)>& parallel);

// The key-light directions the bake uses: `count` directions over the upper hemisphere (Fibonacci), the horizon included.
std::vector<glm::vec3> probeKeyDirections(uint32_t count);

// A hash of everything a bake reads (the cache's key: the same room, fires and numbers bake the same probes).
uint64_t probeBakeKey(const ProbeBakeInput& input);
// The bake kept on disk: false when the file is missing, from another version or does not hold `probeCount` probes.
bool loadProbeBake(const std::string& path, uint64_t key, ProbeBakeOutput& out);
void saveProbeBake(const std::string& path, uint64_t key, const ProbeBakeOutput& out);

}  // namespace ember
