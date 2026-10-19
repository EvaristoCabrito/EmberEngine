#pragma once

// The real night sky (2026-10-19; the user: the random star field had "nenhum pensamento colocado nelas, nenhuma tentativa de
// parecer um céu de verdade com constelações"): content/sky/stars.json holds the brightest stars with their real position
// (right ascension, declination), magnitude and colour; this turns them, for an observer at the file's latitude and a local
// sidereal time, into world directions the sky shader draws (atmosphere.hlsli realStars, star look "real"). The galactic
// frame (the Milky Way's plane and centre) is turned the same way for the band.
// World frame (TimeOfDay.h lightDirection): +y up, azimuth 0 = +z, azimuth 90 deg = +x; here north = +z, east = +x, as the
// sun's path in the 24 h cycle (rising at +x, setting at -x) needs for the stars to turn the same way.

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

namespace ember {

// One star as the GPU reads it (atmosphere.hlsli: two float4 per star). 32 bytes.
struct GpuStar {
  glm::vec3 direction;  // world, unit, towards the star
  float magnitude;      // visual magnitude (brighter = smaller)
  glm::vec3 color;      // linear, from the B-V colour index, luminance ~1
  float pad = 0.0f;
};
static_assert(sizeof(GpuStar) == 32);

class StarSky {
 public:
  struct Star {
    std::string name;
    float ra = 0.0f, dec = 0.0f, mag = 0.0f, bv = 0.0f;  // hours, degrees, magnitude, colour index
  };
  struct Cloud {  // a Magellanic cloud: a soft patch of the galaxy's light
    float ra = 0.0f, dec = 0.0f, radiusDegrees = 1.0f, brightness = 1.0f;
  };

  bool load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    nlohmann::json j;
    try {
      in >> j;
    } catch (...) {
      return false;
    }
    latitude_ = j.value("latitude", -23.0f);
    lstAtMidnight_ = j.value("lstAtMidnight", 17.0f);
    stars_.clear();
    for (const auto& s : j.value("stars", nlohmann::json::array())) {
      if (!s.is_array() || s.size() < 5) continue;
      stars_.push_back({s[0].get<std::string>(), s[1].get<float>(), s[2].get<float>(), s[3].get<float>(), s[4].get<float>()});
    }
    clouds_.clear();
    for (const auto& c : j.value("clouds", nlohmann::json::array())) {
      if (!c.is_array() || c.size() < 5) continue;
      clouds_.push_back({c[1].get<float>(), c[2].get<float>(), c[3].get<float>(), c[4].get<float>()});
    }
    return !stars_.empty();
  }
  bool loaded() const { return !stars_.empty(); }
  size_t count() const { return stars_.size(); }
  const std::vector<Cloud>& clouds() const { return clouds_; }

  // The local sidereal time (hours) at the day's `hour` (0..24).
  float siderealHours(float hour) const { return std::fmod(lstAtMidnight_ + hour * 1.0027379f + 48.0f, 24.0f); }

  // A sky direction (ra hours, dec degrees) as a world direction for the observer at the file's latitude when the local
  // sidereal time is `lstHours`: the hour angle H = LST - RA, then altitude / azimuth (north = +z, east = +x).
  glm::vec3 worldDirection(float raHours, float decDegrees, float lstHours) const {
    const float pi = std::numbers::pi_v<float>;
    const float h = (lstHours - raHours) * (pi / 12.0f);  // hour angle, radians (west positive)
    const float dec = decDegrees * (pi / 180.0f), lat = latitude_ * (pi / 180.0f);
    const float sinAlt = std::sin(dec) * std::sin(lat) + std::cos(dec) * std::cos(lat) * std::cos(h);
    const float alt = std::asin(std::clamp(sinAlt, -1.0f, 1.0f));
    // Azimuth from north through east: sin A = -sin H cos dec / cos alt; cos A = (sin dec - sin alt sin lat) / (cos alt cos lat).
    const float cosAlt = std::max(std::cos(alt), 1e-5f);
    const float sinA = -std::sin(h) * std::cos(dec) / cosAlt;
    const float cosA = (std::sin(dec) - sinAlt * std::sin(lat)) / (cosAlt * std::cos(lat));
    const float az = std::atan2(sinA, cosA);
    return glm::normalize(glm::vec3(std::sin(az) * cosAlt, std::sin(alt), std::cos(az) * cosAlt));
  }

  // The stars for the GPU at that sidereal time.
  std::vector<GpuStar> gpuStars(float lstHours) const {
    std::vector<GpuStar> out;
    out.reserve(stars_.size());
    for (const Star& s : stars_) out.push_back({worldDirection(s.ra, s.dec, lstHours), s.mag, colorOf(s.bv)});
    return out;
  }
  // The galactic frame in the world: the north galactic pole (RA 12h51.4m, Dec +27.13) and the galactic centre (17h45.6m, -28.94).
  glm::vec3 galacticPole(float lstHours) const { return worldDirection(12.857f, 27.13f, lstHours); }
  glm::vec3 galacticCentre(float lstHours) const { return worldDirection(17.760f, -28.94f, lstHours); }

  // A star's colour (linear RGB, luminance about 1) from its B-V index: blue-white (-0.3) through white, yellow, orange to red (2.0).
  static glm::vec3 colorOf(float bv) {
    static constexpr float kBv[7] = {-0.3f, 0.0f, 0.3f, 0.6f, 1.0f, 1.5f, 2.0f};
    static constexpr glm::vec3 kRgb[7] = {{0.62f, 0.75f, 1.00f}, {0.80f, 0.87f, 1.00f}, {0.97f, 0.97f, 1.00f}, {1.00f, 0.96f, 0.90f},
                                          {1.00f, 0.88f, 0.72f}, {1.00f, 0.78f, 0.55f}, {1.00f, 0.70f, 0.45f}};
    const float t = std::clamp(bv, kBv[0], kBv[6]);
    for (int i = 0; i < 6; ++i)
      if (t <= kBv[i + 1]) {
        const float k = (t - kBv[i]) / (kBv[i + 1] - kBv[i]);
        const glm::vec3 c = glm::mix(kRgb[i], kRgb[i + 1], k);
        return c / (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b);
      }
    return glm::vec3(1.0f);
  }

 private:
  float latitude_ = -23.0f, lstAtMidnight_ = 17.0f;
  std::vector<Star> stars_;
  std::vector<Cloud> clouds_;
};

}  // namespace ember
