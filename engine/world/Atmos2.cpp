// Atmos 2 core: see Atmos2.h and docs/ATMOS2.md. Pure C++, double precision, no random numbers. Every step runs the ten
// parts of the brief's §3 in that order; the forces act before the continuity (a forward-backward step), which keeps the
// gravity waves of a 180 s step stable on this grid where a plain forward step would not.
#include "world/Atmos2.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <fstream>

namespace ember::atmos2 {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kG = 9.80665, kR = 287.05, kCp = 1004.6, kKappa = kR / kCp, kOmega = 7.2921e-5, kRadius = 6.371e6;
constexpr double kLv = 2.501e6, kSigma = 5.670374e-8, kSolar = 1361.0, kRho = 1.1;
constexpr double kCloudScale = 3.0e-4;   // qc that makes 63 % cover
constexpr double kRainThreshold = 1.0e-4; // kg / kg of cloud water above which it rains (autoconversion: the excess falls in ~1 h)
constexpr double kAutoconversion = 3600.0; // s
constexpr double kSoilCapacity = 30.0;    // mm of water a saturated soil holds (wet = 1)
constexpr double kAlbedoOcean = 0.07, kAlbedoLand = 0.22, kAlbedoSnow = 0.60, kAlbedoCloud = 0.55;
constexpr double kDiffusion = 0.001;      // share of the 4-neighbour mean mixed in per step (light: a 10-cell wave lives ~5 days, the storms grow faster than that)
constexpr double kFrictionDay = 86400.0;  // the low layer's friction time scale
constexpr double kSurfaceToMean = 2.4;
constexpr double kCondLow = 0.85, kCondHigh = 0.40;  // the pressure share (of ps) where each layer's cloud forms    // surface vapour / the low layer's mean (q falls with height over the 5.5 km layer)

inline double sq(double x) { return x * x; }
inline double clamp01(double x) { return std::min(1.0, std::max(0.0, x)); }

// Value noise on integer hashes (no state, no random): the planet's terrain is the same every time.
inline std::uint32_t hash2(std::int64_t x, std::int64_t y, std::uint32_t channel) {
  auto h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u + channel * 2246822519u + 0x9E3779B9u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
inline double smooth(double t) { return t * t * (3.0 - 2.0 * t); }
double valueNoise(double x, double y, std::uint32_t channel) {
  const double fx = std::floor(x), fy = std::floor(y);
  const auto ix = static_cast<std::int64_t>(fx), iy = static_cast<std::int64_t>(fy);
  const double tx = smooth(x - fx), ty = smooth(y - fy);
  auto at = [&](std::int64_t a, std::int64_t b) { return static_cast<double>(hash2(a, b, channel) % 10000u) / 10000.0; };
  const double a = at(ix, iy), b = at(ix + 1, iy), c = at(ix, iy + 1), d = at(ix + 1, iy + 1);
  return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}
double fbm(double x, double y, std::uint32_t channel) {
  double sum = 0.0, amp = 0.5, freq = 1.0, norm = 0.0;
  for (int o = 0; o < 5; ++o) {
    sum += amp * valueNoise(x * freq, y * freq, channel + static_cast<std::uint32_t>(o));
    norm += amp; amp *= 0.5; freq *= 2.0;
  }
  return sum / norm;
}
}  // namespace

// Continents ~60 deg across; the longitude wraps because the noise is sampled on the unit circle of the longitude.
double Core::Terrain::height(double lonDeg, double latDeg) {
  const double ang = lonDeg * kPi / 180.0;
  const double x = (std::cos(ang) + 1.0) * 3.0, y = (std::sin(ang) + 1.0) * 3.0, z = (latDeg + 90.0) / 30.0;
  const double n = fbm(x + z * 0.37, y + z * 1.13, 7u);
  if (n < 0.52) return 0.0;                         // sea
  const double h = (n - 0.52) / 0.48;               // 0..1 over the land
  return std::min(2500.0, 50.0 + 2450.0 * h * h);   // gentle slopes, capped (steep terrain is unstable: §4)
}
bool Core::Terrain::land(double lonDeg, double latDeg) { return height(lonDeg, latDeg) > 0.0; }

Core::Core() {
  for (auto& l : layers_) l.resize(kCells);
  ps_.assign(kCells, 1000.0); ts_.assign(kCells, 288.0); wet_.assign(kCells, 0.5); terrain_.assign(kCells, 0.0);
  land_.assign(kCells, 0); upflow_.assign(kCells, 0.0); scratch_.assign(kCells, 0.0); scratch2_.assign(kCells, 0.0);
  xIdx_.assign(static_cast<size_t>(kCells) * 4, 0); yIdx_.assign(static_cast<size_t>(kCells) * 4, 0); xW_.assign(static_cast<size_t>(kCells) * 4, 0.0); yW_.assign(static_cast<size_t>(kCells) * 4, 0.0);
  diag_.resize(kCells);
  factorLow_.assign(kCells, 1.0); factorHigh_.assign(kCells, 1.0); factorSurface_.assign(kCells, 1.0); factorCondLow_.assign(kCells, 1.0); factorCondHigh_.assign(kCells, 1.0);
  refreshPressureFactors();
  dx_.resize(kNy); cosLat_.resize(kNy); area_.resize(kNy); fcor_.resize(kNy);
  const double dlat = kDlat * kPi / 180.0, dlon = kDlon * kPi / 180.0;
  dy_.assign(1, kRadius * dlat);
  for (int j = 0; j < kNy; ++j) {
    const double lat = latOf(j) * kPi / 180.0;
    cosLat_[static_cast<size_t>(j)] = std::cos(lat);
    dx_[static_cast<size_t>(j)] = kRadius * std::cos(lat) * dlon;
    area_[static_cast<size_t>(j)] = std::cos(lat);
    fcor_[static_cast<size_t>(j)] = 2.0 * kOmega * std::sin(lat);
  }
}

void Core::coldStart() {
  step_ = 0;
  for (int j = 0; j < kNy; ++j)
    for (int i = 0; i < kNx; ++i) {
      const int c = j * kNx + i;
      const double lon = lonOf(i), lat = latOf(j);
      terrain_[static_cast<size_t>(c)] = Terrain::height(lon, lat);
      land_[static_cast<size_t>(c)] = Terrain::land(lon, lat) ? 1 : 0;
    }
  // Smooth the peaks (two passes of the 4-neighbour mean over the land).
  for (int pass = 0; pass < 2; ++pass) {
    scratch_ = terrain_;
    for (int j = 0; j < kNy; ++j)
      for (int i = 0; i < kNx; ++i) {
        const int c = j * kNx + i;
        if (!land_[static_cast<size_t>(c)]) continue;
        const int w = j * kNx + (i + kNx - 1) % kNx, e = j * kNx + (i + 1) % kNx;
        const int s = std::max(0, j - 1) * kNx + i, n = std::min(kNy - 1, j + 1) * kNx + i;
        terrain_[static_cast<size_t>(c)] = 0.5 * scratch_[static_cast<size_t>(c)] + 0.125 * (scratch_[static_cast<size_t>(w)] + scratch_[static_cast<size_t>(e)] + scratch_[static_cast<size_t>(s)] + scratch_[static_cast<size_t>(n)]);
      }
  }
  for (int j = 0; j < kNy; ++j)
    for (int i = 0; i < kNx; ++i) {
      const size_t c = static_cast<size_t>(j * kNx + i);
      const double lat = latOf(j) * kPi / 180.0;
      ps_[c] = 1000.0;
      const double tLow = 290.0 - 32.0 * sq(std::sin(lat)) - 0.0065 * terrain_[c];
      const double tHigh = 228.0 - 8.0 * sq(std::sin(lat));
      layers_[kLow].theta[c] = tLow * std::pow(1000.0 / 750.0, kKappa);
      layers_[kHigh].theta[c] = tHigh * std::pow(1000.0 / 250.0, kKappa);
      layers_[kLow].q[c] = 0.6 * saturation(tLow, 750.0);
      layers_[kHigh].q[c] = 0.3 * saturation(tHigh, 250.0);
      layers_[kLow].qc[c] = layers_[kHigh].qc[c] = 0.0;
      layers_[kLow].u[c] = layers_[kLow].v[c] = layers_[kHigh].u[c] = layers_[kHigh].v[c] = 0.0;
      ts_[c] = tLow + 2.0;
      wet_[c] = land_[c] ? 0.5 : 1.0;
    }
  massReference_ = totalMass();
  refreshPressureFactors();
  diag_.resize(kCells);
}

double Core::saturation(double T, double pHPa) const {
  const double tc = T - 273.15;
  const double es = 6.112 * std::exp(17.67 * tc / (tc + 243.5));  // hPa (Bolton)
  return 0.622 * es / std::max(1.0, pHPa - 0.378 * es);
}
double Core::layerPressure(int k, int cell) const { return ps_[static_cast<size_t>(cell)] * (k == kLow ? 0.75 : 0.25); }
void Core::refreshPressureFactors() {
  for (int c = 0; c < kCells; ++c) {
    const size_t s = static_cast<size_t>(c);
    static const double kToLow = std::pow(0.75, kKappa), kToHigh = std::pow(0.25, kKappa), kToCondLow = std::pow(kCondLow, kKappa), kToCondHigh = std::pow(kCondHigh, kKappa);
    factorSurface_[s] = std::pow(ps_[s] / 1000.0, kKappa);
    factorLow_[s] = factorSurface_[s] * kToLow;
    factorHigh_[s] = factorSurface_[s] * kToHigh;
    factorCondLow_[s] = factorSurface_[s] * kToCondLow;
    factorCondHigh_[s] = factorSurface_[s] * kToCondHigh;
  }
}
double Core::airTemperature(int k, int cell) const {
  const size_t s = static_cast<size_t>(cell);
  return layers_[static_cast<size_t>(k)].theta[s] * (k == kLow ? factorLow_[s] : factorHigh_[s]);
}
double Core::surfaceAirTemperature(int cell) const {
  const size_t s = static_cast<size_t>(cell);
  return layers_[kLow].theta[s] * factorSurface_[s];
}
double Core::condensationTemperature(int k, int cell) const {
  const size_t s = static_cast<size_t>(cell);
  return layers_[static_cast<size_t>(k)].theta[s] * (k == kLow ? factorCondLow_[s] : factorCondHigh_[s]);
}
double Core::condensationPressure(int k, int cell) const { return ps_[static_cast<size_t>(cell)] * (k == kLow ? kCondLow : kCondHigh); }
double Core::relativeHumidity(int k, int cell) const {
  const double qs = saturation(condensationTemperature(k, cell), condensationPressure(k, cell));
  return clamp01(layers_[static_cast<size_t>(k)].q[static_cast<size_t>(cell)] / std::max(1e-9, qs));
}
double Core::cloudCover(int k, int cell) const {
  return 1.0 - std::exp(-std::max(0.0, layers_[static_cast<size_t>(k)].qc[static_cast<size_t>(cell)]) / kCloudScale);
}
double Core::geopotential(int k, int cell) const {
  const size_t c = static_cast<size_t>(cell);
  const double tLow = airTemperature(kLow, cell);
  const double low = kG * terrain_[c] + kR * tLow * std::log(ps_[c] / 750.0);
  if (k == kLow) return low;
  return low + kR * 0.5 * (tLow + airTemperature(kHigh, cell)) * std::log(3.0);
}

void Core::sunPosition(double worldSeconds, double lonDeg, double latDeg, double& cosZenith, double& azimuthRad) {
  const double day = worldSeconds / 86400.0;
  const double decl = (23.44 * kPi / 180.0) * std::sin(2.0 * kPi * (day - 80.0) / 365.0);
  const double frac = day - std::floor(day);
  const double hour = 2.0 * kPi * (frac - 0.5) + lonDeg * kPi / 180.0;  // 0 at local noon
  const double lat = latDeg * kPi / 180.0;
  cosZenith = std::sin(lat) * std::sin(decl) + std::cos(lat) * std::cos(decl) * std::cos(hour);
  const double sinz = std::sqrt(std::max(0.0, 1.0 - sq(cosZenith)));
  const double cosAz = sinz > 1e-9 ? (std::sin(decl) - std::sin(lat) * cosZenith) / (std::cos(lat) * sinz) : 1.0;
  double az = std::acos(std::min(1.0, std::max(-1.0, cosAz)));
  if (std::sin(hour) > 0.0) az = 2.0 * kPi - az;  // afternoon: west of north
  azimuthRad = az;
}


// The departure point of every cell for one layer's wind, as four source cells and their cubic weights, once per pass; then every
// field of the layer is carried with the same weights (the point depends on the wind alone).
void Core::departures(const std::vector<double>& u, const std::vector<double>& v) {
  auto weights = [](double t, double* w) {
    w[0] = -t * (t - 1.0) * (t - 2.0) / 6.0; w[1] = (t + 1.0) * (t - 1.0) * (t - 2.0) / 2.0;
    w[2] = -(t + 1.0) * t * (t - 2.0) / 2.0; w[3] = (t + 1.0) * t * (t - 1.0) / 6.0;
  };
  for (int j = 0; j < kNy; ++j) {
    const double dx = dx_[static_cast<size_t>(j)];
    for (int i = 0; i < kNx; ++i) {
      const size_t c = static_cast<size_t>(j * kNx + i);
      const double xd = i - u[c] * kDt / dx;
      const double fl = std::floor(xd);
      const int i1 = static_cast<int>(fl);
      weights(xd - fl, &xW_[c * 4]);
      for (int k = 0; k < 4; ++k) xIdx_[c * 4 + static_cast<size_t>(k)] = j * kNx + (((i1 - 1 + k) % kNx) + kNx) % kNx;
    }
  }
  const double dy = dy_[0];
  for (int j = 0; j < kNy; ++j)
    for (int i = 0; i < kNx; ++i) {
      const size_t c = static_cast<size_t>(j * kNx + i);
      const double yd = std::min(static_cast<double>(kNy - 1), std::max(0.0, j - v[c] * kDt / dy));
      const double fl = std::floor(yd);
      const int j1 = static_cast<int>(fl);
      weights(yd - fl, &yW_[c * 4]);
      for (int k = 0; k < 4; ++k) yIdx_[c * 4 + static_cast<size_t>(k)] = std::min(kNy - 1, std::max(0, j1 - 1 + k)) * kNx + i;
    }
}

void Core::advect(std::vector<double>& f) {
  auto apply = [](const std::vector<double>& src, const std::vector<int>& idx, const std::vector<double>& w, std::vector<double>& out) {
    for (size_t c = 0; c < static_cast<size_t>(kCells); ++c) {
      const double p0 = src[static_cast<size_t>(idx[c * 4])], p1 = src[static_cast<size_t>(idx[c * 4 + 1])];
      const double p2 = src[static_cast<size_t>(idx[c * 4 + 2])], p3 = src[static_cast<size_t>(idx[c * 4 + 3])];
      const double v = w[c * 4] * p0 + w[c * 4 + 1] * p1 + w[c * 4 + 2] * p2 + w[c * 4 + 3] * p3;
      const double lo = std::min(std::min(p0, p1), std::min(p2, p3)), hi = std::max(std::max(p0, p1), std::max(p2, p3));
      out[c] = v < lo ? lo : v > hi ? hi : v;  // clipped to the four points' range: no new extremes
    }
  };
  apply(f, xIdx_, xW_, scratch_);   // x pass (longitude wraps)
  apply(scratch_, yIdx_, yW_, f);   // y pass (latitude walls: clamped rows)
}

void Core::transport() {
  for (auto& l : layers_) {
    departures(l.u, l.v);  // the winds at the step's start carry everything, themselves included
    advect(l.theta); advect(l.q); advect(l.qc); advect(l.u); advect(l.v);
  }
}

void Core::forces() {
  for (int k = 0; k < 2; ++k) {
    Layer& l = layers_[static_cast<size_t>(k)];
    for (int c = 0; c < kCells; ++c) scratch2_[static_cast<size_t>(c)] = geopotential(k, c);
    const double dy = dy_[0];
    for (int j = 0; j < kNy; ++j) {
      const double dx = dx_[static_cast<size_t>(j)];
      for (int i = 0; i < kNx; ++i) {
        const size_t c = static_cast<size_t>(j * kNx + i);
        const size_t w = static_cast<size_t>(j * kNx + (i + kNx - 1) % kNx), e = static_cast<size_t>(j * kNx + (i + 1) % kNx);
        const int js = std::max(0, j - 1), jn = std::min(kNy - 1, j + 1);
        const size_t s = static_cast<size_t>(js * kNx + i), n = static_cast<size_t>(jn * kNx + i);
        const double gx = (scratch2_[e] - scratch2_[w]) / (2.0 * dx);
        const double gy = (scratch2_[n] - scratch2_[s]) / (static_cast<double>(jn - js) * dy);
        double u = l.u[c] - gx * kDt, v = l.v[c] - gy * kDt;
        // Coriolis: an exact rotation by f dt (to the right in the north, to the left in the south)
        const double ang = fcor_[static_cast<size_t>(j)] * kDt, ca = std::cos(ang), sa = std::sin(ang);
        const double ur = u * ca + v * sa, vr = -u * sa + v * ca;
        u = ur; v = vr;
        if (k == kLow) { u *= 1.0 - kDt / kFrictionDay; v *= 1.0 - kDt / kFrictionDay; }
        // The latitude walls: no flow through them
        if (j == 0 || j == kNy - 1) v *= 0.5;
        const double speed = std::hypot(u, v);
        if (speed > 200.0) { u *= 200.0 / speed; v *= 200.0 / speed; }  // a safety cap far above any real jet
        l.u[c] = u; l.v[c] = v;
      }
    }
  }
}

void Core::continuity() {
  // Divergence of ps * V per layer (spherical: the v flux weighted by cos lat), hPa / s
  const double dy = dy_[0];
  std::vector<double>& divLow = scratch_;
  std::vector<double>& divHigh = scratch2_;
  for (int k = 0; k < 2; ++k) {
    const Layer& l = layers_[static_cast<size_t>(k)];
    std::vector<double>& out = k == kLow ? divLow : divHigh;
    for (int j = 0; j < kNy; ++j) {
      const double dx = dx_[static_cast<size_t>(j)], cl = cosLat_[static_cast<size_t>(j)];
      for (int i = 0; i < kNx; ++i) {
        const size_t c = static_cast<size_t>(j * kNx + i);
        const size_t w = static_cast<size_t>(j * kNx + (i + kNx - 1) % kNx), e = static_cast<size_t>(j * kNx + (i + 1) % kNx);
        const int js = std::max(0, j - 1), jn = std::min(kNy - 1, j + 1);
        const size_t s = static_cast<size_t>(js * kNx + i), n = static_cast<size_t>(jn * kNx + i);
        const double fxw = ps_[w] * l.u[w], fxe = ps_[e] * l.u[e];
        const double fys = j == 0 ? 0.0 : ps_[s] * l.v[s] * cosLat_[static_cast<size_t>(js)];
        const double fyn = j == kNy - 1 ? 0.0 : ps_[n] * l.v[n] * cosLat_[static_cast<size_t>(jn)];
        const double span = static_cast<double>(jn - js) * dy;
        out[c] = (fxe - fxw) / (2.0 * dx) + (fyn - fys) / (span * cl);
      }
    }
  }
  for (int c = 0; c < kCells; ++c) {
    const size_t s = static_cast<size_t>(c);
    const double dps = -0.5 * (divLow[s] + divHigh[s]) * kDt;  // each layer holds half the column
    upflow_[s] = 0.25 * (divHigh[s] - divLow[s]) * kDt;        // hPa moved low -> high
    ps_[s] = std::max(300.0, ps_[s] + dps);
  }
  // Total air mass exactly constant
  const double scale = massReference_ / totalMass();
  for (double& p : ps_) p *= scale;
  refreshPressureFactors();
}

void Core::verticalExchange() {
  Layer& lo = layers_[kLow];
  Layer& hi = layers_[kHigh];
  for (int c = 0; c < kCells; ++c) {
    const size_t s = static_cast<size_t>(c);
    const double frac = std::min(0.5, std::max(-0.5, upflow_[s] / (0.5 * ps_[s])));
    if (frac > 0.0) {  // rising: the low layer's air arrives in the high layer (upwind)
      hi.theta[s] += frac * (lo.theta[s] - hi.theta[s]); hi.q[s] += frac * (lo.q[s] - hi.q[s]); hi.qc[s] += frac * (lo.qc[s] - hi.qc[s]);
      hi.u[s] += frac * (lo.u[s] - hi.u[s]); hi.v[s] += frac * (lo.v[s] - hi.v[s]);
    } else if (frac < 0.0) {
      const double f = -frac;
      lo.theta[s] += f * (hi.theta[s] - lo.theta[s]); lo.q[s] += f * (hi.q[s] - lo.q[s]); lo.qc[s] += f * (hi.qc[s] - lo.qc[s]);
      lo.u[s] += f * (hi.u[s] - lo.u[s]); lo.v[s] += f * (hi.v[s] - lo.v[s]);
    }
  }
}

void Core::radiation() {
  const double seconds = worldSeconds();
  for (int j = 0; j < kNy; ++j)
    for (int i = 0; i < kNx; ++i) {
      const int c = j * kNx + i;
      const size_t s = static_cast<size_t>(c);
      double cosz = 0.0, az = 0.0;
      sunPosition(seconds, lonOf(i), latOf(j), cosz, az);
      cosz = std::max(0.0, cosz);
      diag_.sunCos[s] = cosz;
      const double coverLow = cloudCover(kLow, c), coverHigh = cloudCover(kHigh, c);
      const double aHigh = kAlbedoCloud * coverHigh, aLow = kAlbedoCloud * coverLow;
      const double incident = kSolar * cosz;
      const double afterHigh = incident * (1.0 - aHigh);
      const double absHigh = afterHigh * 0.03;
      const double afterLow = (afterHigh - absHigh) * (1.0 - aLow);
      const double absLow = afterLow * 0.06;
      const double toGround = afterLow - absLow;
      const bool snow = land_[s] && ts_[s] < 272.5 && wet_[s] > 0.15;
      const double albedoGround = !land_[s] ? kAlbedoOcean : snow ? kAlbedoSnow : kAlbedoLand;
      const double absGround = toGround * (1.0 - albedoGround);
      // Infrared: the ground, then two grey layers (vapour and cloud make them greyer)
      const double tLow = airTemperature(kLow, c), tHigh = airTemperature(kHigh, c);
      const double eLow = std::min(0.98, 0.70 + 0.20 * coverLow + 0.10 * std::min(1.0, layers_[kLow].q[s] / 0.012));
      const double eHigh = std::min(0.95, 0.30 + 0.50 * coverHigh + 0.10 * std::min(1.0, layers_[kHigh].q[s] / 0.002));
      const double tAir = surfaceAirTemperature(c);
      const double tDown = tAir - 0.35 * (tAir - tLow);  // the low layer's warm bottom radiates to the ground
      const double G = kSigma * sq(sq(ts_[s])), L = kSigma * sq(sq(tLow)), Ld = kSigma * sq(sq(tDown)), H = kSigma * sq(sq(tHigh));
      const double passLow = (1.0 - eLow) * G;
      const double netLow = eLow * G + eLow * eHigh * H + absLow - eLow * (L + Ld);
      const double netHigh = eHigh * (passLow + eLow * L) + absHigh - 2.0 * eHigh * H;
      const double down = eLow * Ld + (1.0 - eLow) * eHigh * H;
      const double netGround = absGround + down - G;
      const double top = (1.0 - eHigh) * (passLow + eLow * L) + eHigh * H;
      const double cAir = kCp * (ps_[s] * 100.0 * 0.5) / kG;
      layers_[kLow].theta[s] += netLow * kDt / cAir / factorLow_[s];
      layers_[kHigh].theta[s] += netHigh * kDt / cAir / factorHigh_[s];
      const double cGround = land_[s] ? 1.2e6 : 1.5e7;
      ts_[s] += netGround * kDt / cGround;
      diag_.absorbed[s] = absGround + absLow + absHigh;
      diag_.emitted[s] = top;
    }
}

void Core::surfaceExchange() {
  Layer& lo = layers_[kLow];
  for (int c = 0; c < kCells; ++c) {
    const size_t s = static_cast<size_t>(c);
    const double speed = std::max(3.0, std::hypot(lo.u[s], lo.v[s]));
    const double tAir = surfaceAirTemperature(c);
    const double unstable = 1.0 + 0.15 * std::max(0.0, ts_[s] - tAir);
    const double coeff = kRho * 1.3e-3 * (0.7 * speed + 1.0) * unstable;
    const double Hs = coeff * kCp * (ts_[s] - tAir);                  // W / m2 to the air
    const double avail = land_[s] ? wet_[s] : 1.0;
    const double E = std::max(0.0, coeff * (saturation(ts_[s], ps_[s]) - kSurfaceToMean * lo.q[s]) * avail);  // kg / m2 / s
    const double cAir = kCp * (ps_[s] * 100.0 * 0.5) / kG;
    const double cGround = land_[s] ? 1.2e6 : 1.5e7;
    lo.theta[s] += Hs * kDt / cAir / factorLow_[s];
    lo.q[s] += E * kDt * kG / (ps_[s] * 100.0 * 0.5);
    ts_[s] -= (Hs + kLv * E) * kDt / cGround;
    if (land_[s]) wet_[s] = clamp01(wet_[s] - E * kDt / kSoilCapacity);
    else {  // the deep ocean mixes the slab toward its latitude's climate (20 days), with a small season
      const double lat = latOf(c / kNx) * 3.14159265358979323846 / 180.0;
      const double season = std::sin(2.0 * 3.14159265358979323846 * (worldSeconds() / 86400.0 - 110.0) / 365.0) * (lat >= 0 ? 1.0 : -1.0);
      const double clim = 301.0 - 38.0 * std::sin(lat) * std::sin(lat) + 3.0 * season * std::abs(std::sin(lat));
      ts_[s] += (clim - ts_[s]) * kDt / (20.0 * 86400.0);
    }
  }
}

void Core::convection() {
  Layer& lo = layers_[kLow];
  Layer& hi = layers_[kHigh];
  for (int c = 0; c < kCells; ++c) {
    const size_t s = static_cast<size_t>(c);
    const double tLow = airTemperature(kLow, c), tHigh = airTemperature(kHigh, c);
    const double zLow = geopotential(kLow, c) / kG, zHigh = geopotential(kHigh, c) / kG;
    const double hLow = kCp * surfaceAirTemperature(c) + kG * terrain_[s] + kLv * std::min(kSurfaceToMean * lo.q[s], saturation(surfaceAirTemperature(c), ps_[s]));
    const double hHighSat = kCp * tHigh + kG * zHigh + kLv * saturation(condensationTemperature(kHigh, c), condensationPressure(kHigh, c));
    const double excess = hLow - hHighSat;
    if (excess <= 0.0) { diag_.convection[s] = 0.0; continue; }
    const double strength = clamp01(excess / (kCp * 15.0));
    const double m = 0.25 * strength;
    auto mix = [m](double& a, double& b, double share) {
      const double mean = 0.5 * (a + b);
      a += m * share * (mean - a); b += m * share * (mean - b);
    };
    // The updraught carries vapour up (it condenses aloft in phaseChange and heats the high layer there) and momentum both ways.
    // Theta is NOT mixed: the high layer's theta is ~30 K over the low one's, so mixing it heated the low air without end (124 C).
    const double lift = m * lo.q[s];  // both layers hold the same air mass: vapour moves one for one
    lo.q[s] -= lift; hi.q[s] += lift;
    mix(lo.u[s], hi.u[s], 0.5); mix(lo.v[s], hi.v[s], 0.5);
    diag_.convection[s] = strength;
  }
}

void Core::phaseChange() {
  for (int k = 0; k < 2; ++k) {
    Layer& l = layers_[static_cast<size_t>(k)];
    for (int c = 0; c < kCells; ++c) {
      const size_t s = static_cast<size_t>(c);
      const double T = condensationTemperature(k, c), p = condensationPressure(k, c);
      const double qs = saturation(T, p), toTheta = 1.0 / (k == kLow ? factorLow_[s] : factorHigh_[s]);
      if (l.q[s] > qs) {
        const double ex = 0.5 * (l.q[s] - qs);
        l.qc[s] += ex; l.q[s] -= ex;
        l.theta[s] += kLv * ex / kCp * toTheta;
      } else if (l.qc[s] > 0.0) {
        const double ev = std::min(l.qc[s], qs - l.q[s]) * 0.3;
        l.q[s] += ev; l.qc[s] -= ev;
        l.theta[s] -= kLv * ev / kCp * toTheta;
      }
      if (l.q[s] < 0.0) l.q[s] = 0.0;
      if (l.qc[s] < 0.0) l.qc[s] = 0.0;
    }
  }
}

void Core::rain() {
  Layer& lo = layers_[kLow];
  Layer& hi = layers_[kHigh];
  for (int c = 0; c < kCells; ++c) {
    const size_t s = static_cast<size_t>(c);
    const double dp = ps_[s] * 100.0 * 0.5;  // Pa per layer
    const double rhLow = relativeHumidity(kLow, c);
    const double toThetaLow = 1.0 / factorLow_[s];
    double ground = 0.0;  // kg / kg of the low layer's mass equivalent, summed as kg / m2 below
    // High cloud: rain falls through the low layer, part evaporating there
    if (hi.qc[s] > kRainThreshold) {
      const double r = (hi.qc[s] - kRainThreshold) * std::min(1.0, kDt / kAutoconversion);
      hi.qc[s] -= r;
      const double evap = r * std::min(0.5, 1.0 - rhLow);
      lo.q[s] += evap; lo.theta[s] -= kLv * evap / kCp * toThetaLow;
      ground += (r - evap) * dp / kG;
    }
    if (lo.qc[s] > kRainThreshold) {
      const double r = (lo.qc[s] - kRainThreshold) * std::min(1.0, kDt / kAutoconversion);
      lo.qc[s] -= r;
      const double evap = r * std::min(0.3, 0.6 * (1.0 - rhLow));
      lo.q[s] += evap; lo.theta[s] -= kLv * evap / kCp * toThetaLow;
      ground += (r - evap) * dp / kG;
    }
    diag_.rain[s] = ground * 3600.0 / kDt;  // mm / h (1 kg / m2 = 1 mm)
    if (land_[s]) {
      wet_[s] = clamp01(wet_[s] + ground / kSoilCapacity);
      wet_[s] = clamp01(wet_[s] - kDt / (86400.0 * 12.0) * (1.0 - rhLow));  // slow drying under dry air
    } else wet_[s] = 1.0;
  }
}

void Core::smoothing() {
  auto diffuse = [&](std::vector<double>& f) {
    scratch_ = f;
    for (int j = 0; j < kNy; ++j) {
      const double lat = std::abs(latOf(j));
      const double polar = lat > 55.0 ? 0.5 * std::min(1.0, (lat - 55.0) / 20.0) : 0.0;
      for (int i = 0; i < kNx; ++i) {
        const size_t c = static_cast<size_t>(j * kNx + i);
        const size_t w = static_cast<size_t>(j * kNx + (i + kNx - 1) % kNx), e = static_cast<size_t>(j * kNx + (i + 1) % kNx);
        const size_t sIdx = static_cast<size_t>(std::max(0, j - 1) * kNx + i), n = static_cast<size_t>(std::min(kNy - 1, j + 1) * kNx + i);
        const double mean4 = 0.25 * (scratch_[w] + scratch_[e] + scratch_[sIdx] + scratch_[n]);
        double v = scratch_[c] + kDiffusion * (mean4 - scratch_[c]);
        if (polar > 0.0) {
          const size_t w2 = static_cast<size_t>(j * kNx + (i + kNx - 2) % kNx), e2 = static_cast<size_t>(j * kNx + (i + 2) % kNx);
          const double ew = 0.25 * (scratch_[w2] + scratch_[w] + scratch_[e] + scratch_[e2]);
          v += polar * (ew - v);
        }
        f[c] = v;
      }
    }
  };
  for (auto& l : layers_) { diffuse(l.u); diffuse(l.v); diffuse(l.theta); diffuse(l.q); diffuse(l.qc); }
  diffuse(ps_);
  const double scale = massReference_ / totalMass();
  for (double& p : ps_) p *= scale;
  refreshPressureFactors();
}

void Core::tick() {
  using Clk = std::chrono::steady_clock;
  auto t = Clk::now();
  auto lap = [&](int i) { const auto n = Clk::now(); profile_[static_cast<size_t>(i)] += std::chrono::duration<double, std::milli>(n - t).count(); t = n; };
  transport(); lap(0);
  forces(); lap(1);
  continuity(); lap(2);
  verticalExchange(); lap(3);
  radiation(); lap(4);
  surfaceExchange(); lap(5);
  convection(); lap(6);
  phaseChange(); lap(7);
  rain(); lap(8);
  smoothing(); lap(9);
  ++step_;
}

double Core::totalMass() const {
  double sum = 0.0, w = 0.0;
  for (int j = 0; j < kNy; ++j) {
    const double a = area_[static_cast<size_t>(j)];
    for (int i = 0; i < kNx; ++i) { sum += ps_[static_cast<size_t>(j * kNx + i)] * a; w += a; }
  }
  return sum / w;
}
double Core::meanAbsorbed() const {
  double sum = 0.0, w = 0.0;
  for (int j = 0; j < kNy; ++j) {
    const double a = area_[static_cast<size_t>(j)];
    for (int i = 0; i < kNx; ++i) { sum += diag_.absorbed[static_cast<size_t>(j * kNx + i)] * a; w += a; }
  }
  return sum / w;
}
double Core::meanEmitted() const {
  double sum = 0.0, w = 0.0;
  for (int j = 0; j < kNy; ++j) {
    const double a = area_[static_cast<size_t>(j)];
    for (int i = 0; i < kNx; ++i) { sum += diag_.emitted[static_cast<size_t>(j * kNx + i)] * a; w += a; }
  }
  return sum / w;
}
double Core::maxWind() const {
  double m = 0.0;
  for (const auto& l : layers_)
    for (int c = 0; c < kCells; ++c) m = std::max(m, std::hypot(l.u[static_cast<size_t>(c)], l.v[static_cast<size_t>(c)]));
  return m;
}
bool Core::finite() const {
  auto ok = [](const std::vector<double>& v) { return std::all_of(v.begin(), v.end(), [](double x) { return std::isfinite(x); }); };
  for (const auto& l : layers_) if (!ok(l.u) || !ok(l.v) || !ok(l.theta) || !ok(l.q) || !ok(l.qc)) return false;
  return ok(ps_) && ok(ts_) && ok(wet_);
}

// Binary state: "ATM2", version 1, nx, ny, step, massReference, then every field as little-endian doubles (the land mask as bytes).
namespace {
constexpr std::uint32_t kMagic = 0x324D5441u;  // "ATM2"
constexpr std::uint32_t kVersion = 1;
template <class T> void put(std::vector<std::uint8_t>& out, const T& v) {
  const size_t at = out.size(); out.resize(at + sizeof(T)); std::memcpy(out.data() + at, &v, sizeof(T));
}
void putDoubles(std::vector<std::uint8_t>& out, const std::vector<double>& v) {
  const size_t at = out.size(); out.resize(at + v.size() * sizeof(double)); std::memcpy(out.data() + at, v.data(), v.size() * sizeof(double));
}
template <class T> bool get(const std::vector<std::uint8_t>& in, size_t& at, T& v) {
  if (at + sizeof(T) > in.size()) return false;
  std::memcpy(&v, in.data() + at, sizeof(T)); at += sizeof(T); return true;
}
bool getDoubles(const std::vector<std::uint8_t>& in, size_t& at, std::vector<double>& v, size_t n) {
  if (at + n * sizeof(double) > in.size()) return false;
  v.resize(n); std::memcpy(v.data(), in.data() + at, n * sizeof(double)); at += n * sizeof(double); return true;
}
}  // namespace

std::vector<std::uint8_t> Core::exportState() const {
  std::vector<std::uint8_t> out;
  put(out, kMagic); put(out, kVersion); put(out, static_cast<std::uint32_t>(kNx)); put(out, static_cast<std::uint32_t>(kNy));
  put(out, step_); put(out, massReference_);
  for (const auto& l : layers_) { putDoubles(out, l.u); putDoubles(out, l.v); putDoubles(out, l.theta); putDoubles(out, l.q); putDoubles(out, l.qc); }
  putDoubles(out, ps_); putDoubles(out, ts_); putDoubles(out, wet_); putDoubles(out, terrain_);
  out.insert(out.end(), land_.begin(), land_.end());
  return out;
}
bool Core::importState(const std::vector<std::uint8_t>& in) {
  size_t at = 0;
  std::uint32_t magic = 0, version = 0, nx = 0, ny = 0;
  if (!get(in, at, magic) || !get(in, at, version) || !get(in, at, nx) || !get(in, at, ny)) return false;
  if (magic != kMagic || version != kVersion || nx != static_cast<std::uint32_t>(kNx) || ny != static_cast<std::uint32_t>(kNy)) return false;
  if (!get(in, at, step_) || !get(in, at, massReference_)) return false;
  for (auto& l : layers_)
    if (!getDoubles(in, at, l.u, kCells) || !getDoubles(in, at, l.v, kCells) || !getDoubles(in, at, l.theta, kCells) || !getDoubles(in, at, l.q, kCells) || !getDoubles(in, at, l.qc, kCells)) return false;
  if (!getDoubles(in, at, ps_, kCells) || !getDoubles(in, at, ts_, kCells) || !getDoubles(in, at, wet_, kCells) || !getDoubles(in, at, terrain_, kCells)) return false;
  if (at + kCells > in.size()) return false;
  land_.assign(in.begin() + static_cast<std::ptrdiff_t>(at), in.begin() + static_cast<std::ptrdiff_t>(at + kCells));
  factorLow_.assign(kCells, 1.0); factorHigh_.assign(kCells, 1.0); factorSurface_.assign(kCells, 1.0); factorCondLow_.assign(kCells, 1.0); factorCondHigh_.assign(kCells, 1.0);
  refreshPressureFactors();
  diag_.resize(kCells);
  return true;
}
bool Core::save(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  const auto bytes = exportState();
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(out);
}
bool Core::load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return importState(bytes);
}

}  // namespace ember::atmos2
