#pragma once
// Atmos 2, the WeatherService (docs/ATMOS2.md §5-§7): runs the core on a worker thread, publishes snapshots, blends between
// steps, answers "what is the weather at this world position?", and hands the answer to the game's existing weather readers as a
// tempo::Sample (the clouds, the rain, the lightning, the world map read that struct already: nothing of theirs changes).
//
// Time: the game's one clock (the campaign's milliseconds; day 0 00:00 = the shipped state's first step). The worker keeps
// stepping until its newest snapshot is one step ahead of the game clock; a save far ahead of the state catches up in the
// background (deterministic: the same steps give the same bits), and the newest snapshot serves meanwhile. The clock never
// runs backwards here: a test hour behind the newest snapshot is served the oldest kept snapshot.
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "world/Atmos2.h"
#include "world/Tempo.h"

namespace ember::atmos2 {

// A compact snapshot of one step (floats per cell), published as shared_ptr<const Frame>.
struct Frame {
  std::int64_t step = 0;
  double timeSeconds = 0.0;  // world seconds of the state after the step
  std::vector<float> tAir, tSurface, rh, ps, uLow, vLow, uHigh, vHigh, coverLow, coverHigh, rain, convection, wet, qLow;
  static std::shared_ptr<const Frame> capture(const Core& core);
};

// The weather at one point, blended between two snapshots (§6) plus the values derived for gameplay (§7).
struct WeatherSample {
  double airTemperatureC = 15.0, surfaceTemperatureC = 15.0, humidity = 0.5, dewPointC = 5.0, pressureHPa = 1000.0;
  double windLowX = 0.0, windLowZ = 0.0, windHighX = 0.0, windHighZ = 0.0;  // m / s, x east, z north
  double coverLow = 0.0, coverHigh = 0.0, rainMmPerHour = 0.0, convection = 0.0;
  double sunCos = 0.0, sunAzimuth = 0.0, sunX = 0.0, sunY = 1.0, sunZ = 0.0;
  double wetness = 0.0;
  // Derived (§7)
  double visibilityKm = 40.0, traction = 1.0, speedOfSound = 340.0, soundAbsorption = 0.0, conductivity = 0.0;
  double lightningRisk = 0.0, feelsLikeC = 15.0;
  bool snow = false;  // the rain falls as snow (air under 0.5 C)
  bool ice = false;   // a wet surface under 0.5 C
  std::int64_t step = 0;
  int cell = 0;
};

class Service {
 public:
  static Service& instance();
  ~Service();

  // Loads the shipped state (content/atmos2/state-v01.bin) and the position config, starts the worker. False = no state.
  bool start(const std::string& statePath, const std::string& configPath);
  void stop();
  bool running() const { return running_; }
  bool enabled() const { return enabled_; }
  void setEnabled(bool on) { enabled_ = on; }

  // The game clock, world milliseconds (the campaign's). The worker steps toward it.
  void setGameTime(std::int64_t worldMs);
  // Where the party stands: the overworld's hex, mapped to longitude / latitude by the config.
  void setOverworldHex(int col, int row);
  void setLonLat(double lon, double lat) { lon_ = lon; lat_ = lat; }
  double lon() const { return lon_; }
  double lat() const { return lat_; }

  // The weather at the party's position at the game time (blended between the two newest snapshots).
  WeatherSample sample(std::int64_t worldMs) const;
  WeatherSample sampleAt(double lon, double lat, std::int64_t worldMs) const;
  // The game's weather struct from a sample (TempoConsumers' controls() reads it).
  static tempo::Sample toTempo(const WeatherSample& w);
  // A lightning strike this world minute at this cell: a hash of (step, cell), never a random number.
  static bool strike(std::int64_t worldMinute, int cell, double risk);

  std::int64_t newestStep() const;
  std::int64_t behindSteps() const;  // how many steps the worker still owes the game clock
  double lastStepMs() const { return lastStepMs_; }
  std::string stateLabel(const WeatherSample& w) const;

 private:
  Service() = default;
  void work();
  std::shared_ptr<const Frame> frameBefore(double seconds, std::shared_ptr<const Frame>& after) const;

  Core core_;
  std::thread worker_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  std::deque<std::shared_ptr<const Frame>> frames_;  // oldest first, the last kFramesKept steps
  std::atomic<bool> running_{false}, quit_{false}, enabled_{true};
  std::atomic<std::int64_t> targetMs_{0};
  std::atomic<double> lastStepMs_{0.0};
  double baseSeconds_ = 0.0;  // the shipped state's world seconds = game ms 0
  double lon_ = 0.0, lat_ = 45.0;
  double originLon_ = -8.0, originLat_ = 46.0, degreesPerCol_ = 0.05, degreesPerRow_ = 0.045;
  static constexpr size_t kFramesKept = 40;
};

}  // namespace ember::atmos2
