#include "world/Atmos2Service.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

#include "core/Log.h"

namespace ember::atmos2 {
namespace {
constexpr double kPi = 3.14159265358979323846;
inline double clamp01(double x) { return std::min(1.0, std::max(0.0, x)); }
}  // namespace

std::shared_ptr<const Frame> Frame::capture(const Core& core) {
  auto f = std::make_shared<Frame>();
  f->step = core.step();
  f->timeSeconds = core.worldSeconds();
  const auto n = static_cast<size_t>(kCells);
  f->tAir.resize(n); f->tSurface.resize(n); f->rh.resize(n); f->ps.resize(n); f->uLow.resize(n); f->vLow.resize(n);
  f->uHigh.resize(n); f->vHigh.resize(n); f->coverLow.resize(n); f->coverHigh.resize(n); f->rain.resize(n); f->convection.resize(n);
  f->wet.resize(n); f->qLow.resize(n);
  const Layer& lo = core.layer(kLow);
  const Layer& hi = core.layer(kHigh);
  for (int c = 0; c < kCells; ++c) {
    const size_t s = static_cast<size_t>(c);
    f->tAir[s] = static_cast<float>(core.surfaceAirTemperature(c));
    f->tSurface[s] = static_cast<float>(core.ts()[s]);
    f->rh[s] = static_cast<float>(core.relativeHumidity(kLow, c));
    f->ps[s] = static_cast<float>(core.ps()[s]);
    f->uLow[s] = static_cast<float>(lo.u[s]); f->vLow[s] = static_cast<float>(lo.v[s]);
    f->uHigh[s] = static_cast<float>(hi.u[s]); f->vHigh[s] = static_cast<float>(hi.v[s]);
    f->coverLow[s] = static_cast<float>(core.cloudCover(kLow, c));
    f->coverHigh[s] = static_cast<float>(core.cloudCover(kHigh, c));
    f->rain[s] = static_cast<float>(core.diagnostics().rain[s]);
    f->convection[s] = static_cast<float>(core.diagnostics().convection[s]);
    f->wet[s] = static_cast<float>(core.wet()[s]);
    f->qLow[s] = static_cast<float>(lo.q[s]);
  }
  return f;
}

Service& Service::instance() {
  static Service s;
  return s;
}
Service::~Service() { stop(); }

bool Service::start(const std::string& statePath, const std::string& configPath) {
  if (running_) return true;
  if (!core_.load(statePath)) {
    logWarn("atmos2: no state at {}: the living atmosphere stays off", statePath);
    return false;
  }
  baseSeconds_ = core_.worldSeconds();
  if (std::ifstream in(configPath); in) {
    auto j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_object()) {
      originLon_ = j.value("originLon", originLon_); originLat_ = j.value("originLat", originLat_);
      degreesPerCol_ = j.value("degreesPerCol", degreesPerCol_); degreesPerRow_ = j.value("degreesPerRow", degreesPerRow_);
    }
  }
  lon_ = originLon_; lat_ = originLat_;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.clear();
    frames_.push_back(Frame::capture(core_));
  }
  quit_ = false; running_ = true;
  worker_ = std::thread([this] { work(); });
  logInfo("atmos2: state {} at step {} (day {:.1f}), origin lon {:.2f} lat {:.2f}", statePath, core_.step(), baseSeconds_ / 86400.0, originLon_, originLat_);
  return true;
}

void Service::stop() {
  if (!running_) return;
  quit_ = true;
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
  running_ = false;
}

void Service::setGameTime(std::int64_t worldMs) {
  if (worldMs != targetMs_.load()) { targetMs_ = worldMs; wake_.notify_all(); }
}

void Service::setOverworldHex(int col, int row) {
  lon_ = originLon_ + col * degreesPerCol_;
  lat_ = originLat_ - row * degreesPerRow_;
}

std::int64_t Service::newestStep() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return frames_.empty() ? 0 : frames_.back()->step;
}
std::int64_t Service::behindSteps() const {
  const double want = baseSeconds_ + static_cast<double>(targetMs_.load()) / 1000.0 + kDt;
  std::lock_guard<std::mutex> lock(mutex_);
  if (frames_.empty()) return 0;
  return std::max<std::int64_t>(0, static_cast<std::int64_t>(std::ceil((want - frames_.back()->timeSeconds) / kDt)));
}

// The worker: step until the newest snapshot is one step past the game clock, then sleep until the clock moves.
void Service::work() {
  while (!quit_) {
    const double want = baseSeconds_ + static_cast<double>(targetMs_.load()) / 1000.0 + kDt;
    double newest = 0.0;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      newest = frames_.empty() ? core_.worldSeconds() : frames_.back()->timeSeconds;
    }
    if (newest >= want) {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait_for(lock, std::chrono::milliseconds(200));
      continue;
    }
    const auto t0 = std::chrono::steady_clock::now();
    core_.tick();
    auto frame = Frame::capture(core_);
    lastStepMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.push_back(std::move(frame));
    while (frames_.size() > kFramesKept) frames_.pop_front();
  }
}

std::shared_ptr<const Frame> Service::frameBefore(double seconds, std::shared_ptr<const Frame>& after) const {
  std::lock_guard<std::mutex> lock(mutex_);
  after.reset();
  if (frames_.empty()) return nullptr;
  if (seconds <= frames_.front()->timeSeconds) return frames_.front();
  for (size_t i = frames_.size(); i-- > 0;) {
    if (frames_[i]->timeSeconds <= seconds) {
      if (i + 1 < frames_.size()) after = frames_[i + 1];
      return frames_[i];
    }
  }
  return frames_.back();
}

namespace {
// Bilinear read of one field at (lon, lat): longitude wraps, latitude clamps.
double readField(const std::vector<float>& f, double lon, double lat) {
  const double x = (lon + 180.0) / kDlon - 0.5, y = (lat - kLatSouth) / kDlat - 0.5;
  const double fx = std::floor(x), fy = std::floor(y);
  const int i0 = static_cast<int>(fx), j0 = static_cast<int>(fy);
  const double tx = x - fx, ty = y - fy;
  auto at = [&](int i, int j) {
    i = ((i % kNx) + kNx) % kNx; j = std::min(kNy - 1, std::max(0, j));
    return static_cast<double>(f[static_cast<size_t>(j * kNx + i)]);
  };
  const double a = at(i0, j0), b = at(i0 + 1, j0), c = at(i0, j0 + 1), d = at(i0 + 1, j0 + 1);
  return (a + (b - a) * tx) * (1.0 - ty) + (c + (d - c) * tx) * ty;
}
double dewPoint(double tC, double rh) {
  const double a = 17.67, b = 243.5, g = std::log(std::max(1e-4, rh)) + a * tC / (b + tC);
  return b * g / (a - g);
}
}  // namespace

WeatherSample Service::sample(std::int64_t worldMs) const { return sampleAt(lon_, lat_, worldMs); }

WeatherSample Service::sampleAt(double lon, double lat, std::int64_t worldMs) const {
  WeatherSample w;
  const double seconds = baseSeconds_ + static_cast<double>(worldMs) / 1000.0;
  std::shared_ptr<const Frame> after;
  const std::shared_ptr<const Frame> before = frameBefore(seconds, after);
  if (!before) return w;
  const double blend = after ? clamp01((seconds - before->timeSeconds) / kDt) : 0.0;
  auto read = [&](const std::vector<float> Frame::*field) {
    const double a = readField(before.get()->*field, lon, lat);
    if (!after) return a;
    return a + (readField(after.get()->*field, lon, lat) - a) * blend;
  };
  w.airTemperatureC = read(&Frame::tAir) - 273.15;
  w.surfaceTemperatureC = read(&Frame::tSurface) - 273.15;
  w.humidity = clamp01(read(&Frame::rh));
  w.dewPointC = dewPoint(w.airTemperatureC, w.humidity);
  w.pressureHPa = read(&Frame::ps);
  w.windLowX = read(&Frame::uLow); w.windLowZ = read(&Frame::vLow);
  w.windHighX = read(&Frame::uHigh); w.windHighZ = read(&Frame::vHigh);
  w.coverLow = clamp01(read(&Frame::coverLow)); w.coverHigh = clamp01(read(&Frame::coverHigh));
  w.rainMmPerHour = std::max(0.0, read(&Frame::rain));
  w.convection = clamp01(read(&Frame::convection));
  w.wetness = clamp01(read(&Frame::wet));
  w.step = before->step;
  {
    const int i = ((static_cast<int>(std::floor((lon + 180.0) / kDlon)) % kNx) + kNx) % kNx;
    const int j = std::min(kNy - 1, std::max(0, static_cast<int>(std::floor((lat - kLatSouth) / kDlat))));
    w.cell = j * kNx + i;
  }
  Core::sunPosition(seconds, lon, lat, w.sunCos, w.sunAzimuth);
  {
    const double sinz = std::sqrt(std::max(0.0, 1.0 - w.sunCos * w.sunCos));
    w.sunX = sinz * std::sin(w.sunAzimuth); w.sunZ = sinz * std::cos(w.sunAzimuth); w.sunY = w.sunCos;
  }
  // §7
  const double rainVis = 1.0 / (1.0 + 0.6 * w.rainMmPerHour);
  const double humidVis = w.humidity > 0.9 ? std::pow(1.0 - (w.humidity - 0.9) / 0.1, 2.0) * 0.95 + 0.05 : 1.0;
  w.visibilityKm = std::max(0.2, 40.0 * rainVis * humidVis);
  w.snow = w.rainMmPerHour > 0.0 && w.airTemperatureC < 0.5;
  w.ice = w.wetness > 0.2 && w.airTemperatureC < 0.5;
  w.traction = w.ice ? 0.15 : w.wetness > 0.2 ? 1.0 - 0.4 * std::min(1.0, (w.wetness - 0.2) / 0.5) : 1.0;
  w.speedOfSound = 331.3 * std::sqrt(1.0 + w.airTemperatureC / 273.15);
  w.soundAbsorption = 0.02 + 0.08 * (1.0 - w.humidity);
  w.conductivity = clamp01(0.6 * w.wetness + 0.4 * w.humidity);
  w.lightningRisk = clamp01(w.convection * std::min(1.0, w.rainMmPerHour / 4.0) * w.coverHigh * 3.0);
  if (w.airTemperatureC > 27.0) {
    const double T = w.airTemperatureC, R = w.humidity * 100.0;
    w.feelsLikeC = -8.78469475556 + 1.61139411 * T + 2.33854883889 * R - 0.14611605 * T * R - 0.012308094 * T * T - 0.0164248277778 * R * R + 0.002211732 * T * T * R + 0.00072546 * T * R * R - 0.000003582 * T * T * R * R;
  } else if (w.airTemperatureC < 10.0) {
    const double v = std::max(4.8, std::hypot(w.windLowX, w.windLowZ) * 3.6), T = w.airTemperatureC;
    w.feelsLikeC = 13.12 + 0.6215 * T - 11.37 * std::pow(v, 0.16) + 0.3965 * T * std::pow(v, 0.16);
  } else w.feelsLikeC = w.airTemperatureC;
  return w;
}

tempo::Sample Service::toTempo(const WeatherSample& w) {
  using tempo::Q;
  tempo::Sample s;
  auto fix = [](double x) { return static_cast<tempo::I>(std::llround(x * static_cast<double>(Q))); };
  const double speed = std::hypot(w.windLowX, w.windLowZ);
  const double rain = clamp01(w.rainMmPerHour / 6.0), heavy = clamp01((w.rainMmPerHour - 3.0) / 6.0);
  const double fog = (w.humidity > 0.96 && speed < 3.0 && w.rainMmPerHour < 0.2) ? clamp01((w.humidity - 0.96) / 0.04) * clamp01(1.0 - speed / 3.0) : 0.0;
  s.v[tempo::Low] = fix(w.coverLow);
  s.v[tempo::Middle] = fix(0.5 * w.coverLow * (0.5 + 0.5 * w.humidity));
  s.v[tempo::High] = fix(w.coverHigh);
  s.v[tempo::Shape] = fix(0.2 + 0.7 * w.convection);
  s.v[tempo::Vertical] = fix(w.convection);
  s.v[tempo::Base] = fix(std::max(300.0, 900.0 - 600.0 * clamp01((w.humidity - 0.6) / 0.4)));
  s.v[tempo::Thickness] = fix(450.0 + 2000.0 * w.convection + 800.0 * w.coverLow);
  s.v[tempo::Rain] = fix(rain);
  s.v[tempo::HeavyRain] = fix(heavy);
  s.v[tempo::Lightning] = fix(w.lightningRisk * 8.0);  // strikes a minute
  // the direction the wind blows FROM, degrees clockwise from north (meteorological)
  double from = std::atan2(-w.windLowX, -w.windLowZ) * 180.0 / kPi;
  if (from < 0.0) from += 360.0;
  s.v[tempo::WindDirection] = fix(from);
  s.v[tempo::WindSpeed] = fix(speed);
  s.v[tempo::Gust] = fix(0.1 + 0.6 * w.convection + 0.2 * heavy);
  s.v[tempo::Humidity] = fix(w.humidity);
  s.v[tempo::Haze] = fix(0.3 * (1.0 - w.humidity));
  s.v[tempo::Fog] = fix(fog);
  s.v[tempo::ValleyFog] = fix(fog * 0.85);
  s.v[tempo::Temperature] = fix(w.airTemperatureC);
  s.v[tempo::Wetness] = fix(w.wetness);
  if (heavy > 0.2 && w.lightningRisk > 0.05) s.state = tempo::State::ColdStorm;
  else if (heavy > 0.2) s.state = tempo::State::ColdShowers;
  else if (rain > 0.05) s.state = tempo::State::Nimbostratus;
  else if (fog > 0.25) s.state = tempo::State::RadiationFog;
  else if (w.coverLow > 0.7) s.state = tempo::State::Stratus;
  else if (w.coverHigh > 0.6) s.state = tempo::State::Cirrostratus;
  else if (w.coverHigh > 0.3) s.state = tempo::State::Cirrus;
  else s.state = tempo::State::Seasonal;
  return s;
}

bool Service::strike(std::int64_t worldMinute, int cell, double risk) {
  if (risk <= 0.0) return false;
  const std::uint32_t h = tempo::hash(worldMinute, static_cast<std::uint32_t>(cell) * 2654435761u + 7u, 97u);
  return (h % 10000u) < static_cast<std::uint32_t>(clamp01(risk) * 10000.0);
}

std::string Service::stateLabel(const WeatherSample& w) const {
  if (w.snow) return "Neve";
  if (w.rainMmPerHour > 3.0 && w.lightningRisk > 0.05) return "Tempestade";
  if (w.rainMmPerHour > 3.0) return "Chuva forte";
  if (w.rainMmPerHour > 0.3) return "Chuva";
  if (w.humidity > 0.96 && std::hypot(w.windLowX, w.windLowZ) < 3.0) return "Neblina";
  if (w.coverLow > 0.7) return "Nublado";
  if (w.coverLow > 0.35 || w.coverHigh > 0.5) return "Parcialmente nublado";
  return "Limpo";
}

}  // namespace ember::atmos2
