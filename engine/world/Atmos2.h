#pragma once
// Atmos 2: the living atmosphere (docs/ATMOS2.md, the user's brief of 2026-11-05). Weather computed by physics on a planet grid,
// no random rolls, no scripted events: the same starting state always gives the same weather, bit for bit. The game only reads
// from it. This file is the core: a pure C++ class, no engine dependency, fixed 180 s steps.
//
// Grid: 96 x 40 latitude-longitude cells between 78 S and 78 N (3.75 x 3.9 deg), row 0 the south edge, longitude wrapping,
// latitude walled. Two layers (low ~750 hPa, high ~250 hPa): wind u (east) / v (north), potential temperature theta, water
// vapour q, cloud water qc. Per cell once: surface pressure ps (hPa), ground / sea temperature Ts (K), soil moisture wet (0..1),
// terrain height (m), land flag. Storage: flat std::vector<double>, index j * 96 + i.
//
// One step, in order (docs/ATMOS2.md §3): semi-Lagrangian cubic transport (x pass, then y), pressure gradient + exact Coriolis
// rotation + surface friction, mass continuity (ps from the divergence, the leftover as vertical flow, total mass rescaled),
// upwind vertical exchange, sunlight from the world clock (tilt 23.44 deg, 365-day year, 24 h day) with cloud albedo and a
// two-layer greenhouse, surface heat / evaporation growing with the wind, convection, phase change with latent heat, rain
// (part evaporating on the way down, the rest wetting the soil), light diffusion + east-west smoothing above 55 deg.
//
// Repeatable: no random numbers anywhere; a strike is a hash of (step, cell). Never compile this with fast-math.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ember::atmos2 {

inline constexpr int kNx = 96, kNy = 40, kCells = kNx * kNy;
inline constexpr double kLatSouth = -78.0, kLatNorth = 78.0;
inline constexpr double kDlon = 360.0 / kNx, kDlat = (kLatNorth - kLatSouth) / kNy;  // 3.75, 3.9
inline constexpr double kDt = 180.0;                                                  // seconds of world time per step
inline constexpr int kLow = 0, kHigh = 1;

// Longitude / latitude of a cell's centre.
inline double lonOf(int i) { return -180.0 + (i + 0.5) * kDlon; }
inline double latOf(int j) { return kLatSouth + (j + 0.5) * kDlat; }

// One layer's prognostic fields.
struct Layer {
  std::vector<double> u, v, theta, q, qc;
  void resize(int n) { u.assign(n, 0.0); v.assign(n, 0.0); theta.assign(n, 0.0); q.assign(n, 0.0); qc.assign(n, 0.0); }
};

// Diagnostics the last step produced (per cell), read by the snapshot.
struct Diagnostics {
  std::vector<double> rain;        // mm / h reaching the ground
  std::vector<double> convection;  // 0..1: how hard the layers mixed
  std::vector<double> sunCos;      // cosine of the sun's zenith angle (0 at night)
  std::vector<double> absorbed;    // W / m2 of sunlight absorbed (ground + air)
  std::vector<double> emitted;     // W / m2 of infrared leaving the top
  void resize(int n) { rain.assign(n, 0.0); convection.assign(n, 0.0); sunCos.assign(n, 0.0); absorbed.assign(n, 0.0); emitted.assign(n, 0.0); }
};

class Core {
 public:
  Core();

  // A fresh planet: the terrain (m) and land mask from a deterministic procedural map (Terrain below), an isothermal
  // atmosphere at rest, 1000 hPa everywhere. Needs 60-90 simulated days to settle (the Cooker's atmos2-spinup).
  void coldStart();
  // One 180 s step. `worldSeconds` = the world clock at the step's start (seconds since day 0 00:00): the sun's position.
  void tick();
  // Many steps.
  void run(std::int64_t steps) { for (std::int64_t s = 0; s < steps; ++s) tick(); }

  // State in / out: a binary image of every field + the step number (little-endian doubles; bit-exact round trip).
  std::vector<std::uint8_t> exportState() const;
  bool importState(const std::vector<std::uint8_t>& bytes);
  bool save(const std::string& path) const;
  bool load(const std::string& path);

  std::int64_t step() const { return step_; }
  // The world clock at the current state: step * 180 s (day 0 00:00 = step 0).
  double worldSeconds() const { return static_cast<double>(step_) * kDt; }

  const Layer& layer(int k) const { return layers_[static_cast<size_t>(k)]; }
  const std::vector<double>& ps() const { return ps_; }
  const std::vector<double>& ts() const { return ts_; }
  const std::vector<double>& wet() const { return wet_; }
  const std::vector<double>& terrain() const { return terrain_; }
  const std::vector<std::uint8_t>& land() const { return land_; }
  const Diagnostics& diagnostics() const { return diag_; }

  // Derived per cell (the snapshot reads these).
  double airTemperature(int k, int cell) const;  // K at the layer's pressure
  double surfaceAirTemperature(int cell) const;  // K: the low layer brought down to the surface along a dry adiabat
  double saturation(double T, double pHPa) const;  // kg / kg
  double relativeHumidity(int k, int cell) const;
  double condensationTemperature(int k, int cell) const;  // K at the layer's cloud level (850 / 400 hPa of ps)
  double condensationPressure(int k, int cell) const;     // hPa
  double cloudCover(int k, int cell) const;        // 0..1 from the layer's cloud water
  double layerPressure(int k, int cell) const;     // hPa at the layer's middle (ps * 0.75 / ps * 0.25)

  // Global checks (the acceptance tests).
  double totalMass() const;        // area-weighted mean ps (hPa)
  double meanAbsorbed() const;     // W / m2 (area-weighted)
  double meanEmitted() const;      // W / m2
  double maxWind() const;          // m / s, any layer
  bool finite() const;

  // Sun: declination and hour angle for the world clock (shared with the snapshot: direction at a point).
  static void sunPosition(double worldSeconds, double lonDeg, double latDeg, double& cosZenith, double& azimuthRad);

  // The deterministic planet: terrain height (m) and land flag for a cell, from a fixed noise (no file needed).
  struct Terrain { static double height(double lonDeg, double latDeg); static bool land(double lonDeg, double latDeg); };

 private:
  void transport();
  void forces();
  void continuity();
  void verticalExchange();
  void radiation();
  void surfaceExchange();
  void convection();
  void phaseChange();
  void rain();
  void smoothing();
  void departures(const std::vector<double>& u, const std::vector<double>& v);
  void advect(std::vector<double>& f);
  double geopotential(int k, int cell) const;
  void refreshPressureFactors();

  std::array<Layer, 2> layers_;
  std::vector<double> ps_, ts_, wet_, terrain_;
  std::vector<std::uint8_t> land_;
  std::vector<double> upflow_;  // kg / m2 moved low -> high this step (negative = sinking)
  std::vector<double> dx_, dy_, cosLat_, area_, fcor_;  // per row (dx, cosLat, area, f) or constant
  std::vector<double> scratch_, scratch2_;
  std::vector<int> xIdx_, yIdx_;      // 4 source cells per cell for the x / y pass of this layer
  std::vector<double> xW_, yW_;       // their cubic weights
  std::vector<double> factorLow_, factorHigh_, factorSurface_, factorCondLow_, factorCondHigh_;  // (p / 1000)^kappa per cell, refreshed when ps changes
  Diagnostics diag_;
  std::int64_t step_ = 0;
  double massReference_ = 1000.0;
 public:
  std::array<double, 10> profile_{};  // ms spent per part of tick(), summed (the Cooker's test prints it)
};

}  // namespace ember::atmos2
