#pragma once

// The real sky (the user, 2026-11-11: "um ciclo de dia e noite vivo verdadeiro que segue horário real do jogo ... igual no mundo
// real, mesma intensidade e luminosidade real de sol e lua ... nada de emulação"). Nothing here is a curve by the hour: the world's
// clock gives an instant, the instant and the place on the planet give where the sun and the moon stand (the low-precision
// ephemerides of the Astronomical Almanac: the sun within 0.01 deg, the moon within 0.3 deg), and where they stand gives the light
// in lux. Everything else follows by itself: the day's length through the year, the sun's height at noon, the moon's phase (the
// angle between the two as seen from the ground, never a counter), its rising ~50 minutes later every day, the nights with no
// moon, the full moon riding high in winter and low in summer, the 30 % between perigee and apogee, the morning equal to the
// afternoon at the same sun height.
//
// Pure C++ (no engine, no GPU): the maps, the world map and the tests read the same functions.
//
// The world's calendar: the campaign's clock (CampaignTime.h: milliseconds since day 0, 00:00) is the local mean solar time of the
// place, and day 0 is a 1st of January (the weather's own calendar: Tempo.h's summer peaks on day 172, Atmos 2's equinox falls on
// day 80). Day 0 is pinned to the real 1 January 2014, whose new moon fell at 11:14 UT: the campaign opens under a new moon, as
// Ember's moon did.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ember::sky {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kRad = kPi / 180.0;
inline constexpr double kMsPerDay = 86400000.0;
inline constexpr double kEpochJ2000Days = 5113.5;  // 2014-01-01 00:00, in days since J2000.0 (2000-01-01 12:00)

inline double sinD(double a) { return std::sin(a * kRad); }
inline double cosD(double a) { return std::cos(a * kRad); }
inline double tanD(double a) { return std::tan(a * kRad); }
inline double asinD(double x) { return std::asin(std::clamp(x, -1.0, 1.0)) / kRad; }
inline double acosD(double x) { return std::acos(std::clamp(x, -1.0, 1.0)) / kRad; }
inline double atan2D(double y, double x) { return std::atan2(y, x) / kRad; }
inline double wrap360(double a) {
  a = std::fmod(a, 360.0);
  return a < 0.0 ? a + 360.0 : a;
}

// Where on the planet (degrees; north and east positive). The realm's default is Atmos 2's origin (content/atmos2/config.json).
struct Place {
  double latitude = 46.0, longitude = -8.0;
};

// The sky at one instant, seen from one place.
struct State {
  double days = 0.0;                                // days since J2000.0 (UT)
  double sunAltitude = 0.0, sunAzimuth = 0.0;       // degrees; azimuth 0 = north, 90 = east
  double sunDistance = 1.0;                         // AU
  double moonAltitude = 0.0, moonAzimuth = 0.0;     // degrees, the parallax taken off
  double moonDistance = 60.27;                      // Earth radii
  double phaseAngle = 0.0;                          // degrees: 0 = full, 180 = new
  double elongation = 0.0;                          // degrees east of the sun, 0..360: 0 new, 90 first quarter, 180 full, 270 last
  double illuminated = 1.0;                         // the lit share of the disc, 0..1
  bool waxing = true;
  double siderealDegrees = 0.0;                     // the local sidereal time
  std::int64_t lunation = 0;                        // new moons since the world's day 0
};

// Days since J2000.0 (UT) of a world instant: the clock is the place's own mean solar time.
inline double daysOf(std::int64_t worldMs, const Place& p) {
  return kEpochJ2000Days + static_cast<double>(worldMs) / kMsPerDay - p.longitude / 360.0;
}

struct Equatorial {
  double ra = 0.0, dec = 0.0, distance = 1.0, longitude = 0.0, parallax = 0.0;
};
// The sun (distance in AU).
inline Equatorial sunAt(double d) {
  const double eps = 23.439 - 0.0000004 * d;
  const double L = wrap360(280.460 + 0.9856474 * d);
  const double g = wrap360(357.528 + 0.9856003 * d);
  const double lam = L + 1.915 * sinD(g) + 0.020 * sinD(2.0 * g);
  Equatorial e;
  e.longitude = wrap360(lam);
  e.ra = atan2D(cosD(eps) * sinD(lam), cosD(lam));
  e.dec = asinD(sinD(eps) * sinD(lam));
  e.distance = 1.00014 - 0.01671 * cosD(g) - 0.00014 * cosD(2.0 * g);
  return e;
}
// The moon (distance in Earth radii, mean 60.27).
inline Equatorial moonAt(double d) {
  const double T = d / 36525.0, eps = 23.439 - 0.0000004 * d;
  const double lam = 218.32 + 481267.881 * T + 6.29 * sinD(135.0 + 477198.87 * T) - 1.27 * sinD(259.3 - 413335.36 * T) +
                     0.66 * sinD(235.7 + 890534.22 * T) + 0.21 * sinD(269.9 + 954397.74 * T) - 0.19 * sinD(357.5 + 35999.05 * T) -
                     0.11 * sinD(186.5 + 966404.03 * T);
  const double bet = 5.13 * sinD(93.3 + 483202.02 * T) + 0.28 * sinD(228.2 + 960400.89 * T) - 0.28 * sinD(318.3 + 6003.15 * T) -
                     0.17 * sinD(217.6 - 407332.21 * T);
  Equatorial e;
  e.parallax = 0.9508 + 0.0518 * cosD(135.0 + 477198.87 * T) + 0.0095 * cosD(259.3 - 413335.36 * T) +
               0.0078 * cosD(235.7 + 890534.22 * T) + 0.0028 * cosD(269.9 + 954397.74 * T);
  e.longitude = wrap360(lam);
  e.ra = atan2D(sinD(lam) * cosD(eps) - tanD(bet) * sinD(eps), cosD(lam));
  e.dec = asinD(sinD(bet) * cosD(eps) + cosD(bet) * sinD(eps) * sinD(lam));
  e.distance = 1.0 / sinD(e.parallax);
  return e;
}
// The moon's mean elongation, unwrapped (degrees): one new moon every 360.
inline double meanElongation(double d) { return 297.8501921 + 445267.1114034 * (d / 36525.0); }

inline double siderealDegrees(double d, const Place& p) { return wrap360((18.697374558 + 24.06570982441908 * d) * 15.0 + p.longitude); }

// Equatorial to the local sky: altitude, and azimuth from north through east.
inline void horizontal(double hourAngle, double dec, double latitude, double& altitude, double& azimuth) {
  altitude = asinD(sinD(latitude) * sinD(dec) + cosD(latitude) * cosD(dec) * cosD(hourAngle));
  azimuth = wrap360(atan2D(sinD(hourAngle), cosD(hourAngle) * sinD(latitude) - tanD(dec) * cosD(latitude)) + 180.0);
}

// The sun's altitude alone (the solvers below).
inline double sunAltitudeAt(double d, const Place& p) {
  const Equatorial s = sunAt(d);
  double alt, az;
  horizontal(siderealDegrees(d, p) - s.ra, s.dec, p.latitude, alt, az);
  return alt;
}

// The whole sky at `d` days since J2000.0 (UT).
inline State stateAt(double d, const Place& p) {
  State k;
  k.days = d;
  const Equatorial s = sunAt(d), m = moonAt(d);
  k.siderealDegrees = siderealDegrees(d, p);
  horizontal(k.siderealDegrees - s.ra, s.dec, p.latitude, k.sunAltitude, k.sunAzimuth);
  horizontal(k.siderealDegrees - m.ra, m.dec, p.latitude, k.moonAltitude, k.moonAzimuth);
  k.moonAltitude -= m.parallax * cosD(k.moonAltitude);  // the moon is near: seen from the ground it stands up to 1 deg lower
  k.sunDistance = s.distance, k.moonDistance = m.distance;
  const double apart = acosD(sinD(s.dec) * sinD(m.dec) + cosD(s.dec) * cosD(m.dec) * cosD(s.ra - m.ra));
  k.phaseAngle = 180.0 - apart;
  k.illuminated = (1.0 + cosD(k.phaseAngle)) / 2.0;
  k.elongation = wrap360(m.longitude - s.longitude);
  k.waxing = k.elongation < 180.0;
  k.lunation = static_cast<std::int64_t>(std::floor(meanElongation(d) / 360.0) - std::floor(meanElongation(kEpochJ2000Days) / 360.0));
  return k;
}
// ... at a world instant (the campaign's milliseconds).
inline State at(std::int64_t worldMs, const Place& p) { return stateAt(daysOf(worldMs, p), p); }

// --- Light (lux on level ground, clear sky) -----------------------------------------------------------------------------------

// The air between the ground and space along a line `altitude` degrees up (Kasten & Young), x the zenith's.
inline double airMass(double altitude) {
  const double h = std::max(altitude, 0.0);
  return 1.0 / (sinD(h) + 0.50572 * std::pow(h + 6.07995, -1.6364));
}
// The sun's beam on level ground.
inline double sunDirectLux(double altitude, double distanceAu) {
  if (altitude <= 0.0) return 0.0;
  return 133800.0 / (distanceAu * distanceAu) * std::exp(-0.21 * airMass(altitude)) * sinD(altitude);
}
// The sky's own light: the blue sky by day, the twilights after (civil ends at -6 deg with 3.4 lux, nautical at -12 with 0.008,
// astronomical at -18, where the sun gives nothing more).
inline double skyDiffuseLux(double sunAltitude) {
  if (sunAltitude > 0.0) return 800.0 + 15500.0 * std::sqrt(sinD(sunAltitude));
  static constexpr double kAlt[4] = {0.0, -6.0, -12.0, -18.0}, kLog[4] = {2.90, 0.53, -2.10, -4.0};
  if (sunAltitude <= kAlt[3]) return 0.0;
  int i = 0;
  while (i < 2 && sunAltitude < kAlt[i + 1]) ++i;
  const double t = (sunAltitude - kAlt[i]) / (kAlt[i + 1] - kAlt[i]);
  return std::pow(10.0, kLog[i] + (kLog[i + 1] - kLog[i]) * t);
}
// The moon's light above the air, facing it: its magnitude by the phase angle (Allen: a quarter moon gives 8 % of the full one,
// not half) and its distance (perigee to apogee: 30 %).
inline double moonAboveAirLux(double phaseAngle, double distanceEarthRadii) {
  const double a = std::abs(phaseAngle);
  const double v = -12.73 + 0.026 * a + 4e-9 * a * a * a * a;
  const double closer = 60.27 / distanceEarthRadii;
  return std::pow(10.0, -0.4 * (v + 14.18)) * closer * closer;
}
// The moon's beam on level ground.
inline double moonDirectLux(const State& k) {
  if (k.moonAltitude <= 0.0) return 0.0;
  return moonAboveAirLux(k.phaseAngle, k.moonDistance) * std::exp(-0.21 * airMass(k.moonAltitude)) * sinD(k.moonAltitude);
}
inline constexpr double kStarlightLux = 0.001;  // the stars and the air's own glow: a clear night with no moon
// Everything a clear sky gives level ground at that instant.
inline double clearLux(const State& k) {
  return sunDirectLux(k.sunAltitude, k.sunDistance) + skyDiffuseLux(k.sunAltitude) + moonDirectLux(k) + kStarlightLux;
}

// --- The eye --------------------------------------------------------------------------------------------------------------------
// The real range spans eight orders of magnitude; no screen shows that. The eye follows the light but never all the way: the
// darker the hour, the darker the picture, by fewer stops than the light itself lost. `ev` = log2(lux / 2.5) of the clear sky's
// light; the result = how many stops under a full exposure the picture is shown.
//   a full day (20,000 lux and up)   0      the whole picture
//   sunrise / sunset (800 lux)       1.0    half
//   a high full moon (0.23 lux)      1.5    35 %  (the night the user approved on 2026-11-10: at 2 stops it was "um breu")
//   a high quarter moon (0.02 lux)   2.4    19 %
//   no moon (0.001 lux)              3.5    9 %
// The weather's own stops come on top (MapStage: an overcast night with no moon ends near 5 %).
inline double eyeDeficitStops(double ev) {
  static constexpr double kEv[4] = {13.0, 8.3, -3.5, -11.3}, kStops[4] = {0.0, 1.0, 1.5, 3.5};
  if (ev >= kEv[0]) return 0.0;
  int i = 0;
  while (i < 2 && ev < kEv[i + 1]) ++i;
  return kStops[i] + (kStops[i + 1] - kStops[i]) * (kEv[i] - ev) / (kEv[i] - kEv[i + 1]);  // (past the last point: the same slope)
}
inline double evOf(double lux) { return std::log2(std::max(lux, 1e-7) / 2.5); }
// How far the eye has gone over to night vision (0 above 10 lux, 1 under 0.01): colours fade and drift to blue with it.
inline double nightVision(double lux) { return std::clamp((1.0 - std::log10(std::max(lux, 1e-7))) / 3.0, 0.0, 1.0); }

// --- The moon's name ------------------------------------------------------------------------------------------------------------
// The phase's art (TimeOfDay.h kMoonPhases: 0 new .. 4 full .. 7 waning crescent): the nearest eighth of the lunation.
inline int phaseIndex(const State& k) { return static_cast<int>(std::floor(k.elongation / 45.0 + 0.5)) % 8; }
// The realm's blood moon (Ember's rule, kept: every third full moon): the night that full moon stands opposite the sun.
inline bool bloodMoon(const State& k) { return k.lunation > 0 && k.lunation % 3 == 0 && k.phaseAngle < 8.0; }

// --- Solvers --------------------------------------------------------------------------------------------------------------------
// The day that holds a world instant, as its first millisecond.
inline std::int64_t dayStart(std::int64_t worldMs) {
  const std::int64_t day = static_cast<std::int64_t>(std::floor(static_cast<double>(worldMs) / kMsPerDay));
  return day * static_cast<std::int64_t>(kMsPerDay);
}
// The instant the sun stands highest that day.
inline std::int64_t solarNoon(std::int64_t worldMs, const Place& p) {
  const std::int64_t start = dayStart(worldMs);
  double lo = 10.0 * 3600000.0, hi = 14.0 * 3600000.0;
  for (int i = 0; i < 40; ++i) {
    const double a = lo + (hi - lo) / 3.0, b = hi - (hi - lo) / 3.0;
    if (sunAltitudeAt(daysOf(start + static_cast<std::int64_t>(a), p), p) < sunAltitudeAt(daysOf(start + static_cast<std::int64_t>(b), p), p)) lo = a;
    else hi = b;
  }
  return start + static_cast<std::int64_t>((lo + hi) / 2.0);
}
// The instant, that day, the sun crosses `altitude` degrees on its way up (morning) or down. A day whose sun never gets that
// high gives its noon; one whose sun never gets that low gives the end of that half of the day.
inline std::int64_t sunCrossing(std::int64_t worldMs, const Place& p, double altitude, bool morning) {
  const std::int64_t noon = solarNoon(worldMs, p);
  const std::int64_t half = static_cast<std::int64_t>(kMsPerDay / 2.0);
  std::int64_t high = noon, low = morning ? noon - half : noon + half;
  if (sunAltitudeAt(daysOf(high, p), p) <= altitude) return noon;
  if (sunAltitudeAt(daysOf(low, p), p) >= altitude) return low;
  for (int i = 0; i < 40; ++i) {
    const std::int64_t mid = (high + low) / 2;
    if (sunAltitudeAt(daysOf(mid, p), p) > altitude) high = mid;
    else low = mid;
  }
  return (high + low) / 2;
}
inline constexpr double kHorizon = -0.833;  // the sun's upper edge on the horizon, through the air's refraction: sunrise and sunset
// The hour a painted day would show for this instant (the world map's hand-painted looks are keyed to a day whose sun rises at 6,
// stands highest at 12 and sets at 18): the real sunrise, noon and sunset of that day are laid over those three, so the painting's
// morning starts when the real sun rises and its night when it sets, short in winter and long in summer.
inline double paintedHour(std::int64_t worldMs, const Place& p) {
  const double start = static_cast<double>(dayStart(worldMs));
  const double t = (static_cast<double>(worldMs) - start) / 3600000.0;
  const double noon = (static_cast<double>(solarNoon(worldMs, p)) - start) / 3600000.0;
  const double rise = (static_cast<double>(sunCrossing(worldMs, p, kHorizon, true)) - start) / 3600000.0;
  const double set = (static_cast<double>(sunCrossing(worldMs, p, kHorizon, false)) - start) / 3600000.0;
  if (noon - rise < 0.05 || set - noon < 0.05) return t;  // the sun never rises or never sets today: the clock's own hour
  const double night = 24.0 - (set - rise);
  double h;
  if (t < rise) h = 6.0 - 12.0 * (rise - t) / night;
  else if (t < noon) h = 6.0 + 6.0 * (t - rise) / (noon - rise);
  else if (t < set) h = 12.0 + 6.0 * (t - noon) / (set - noon);
  else h = 18.0 + 12.0 * (t - set) / night;
  return h < 0.0 ? h + 24.0 : h >= 24.0 ? h - 24.0 : h;
}

// The instant, at the same hour of the clock within the lunation around `worldMs`, when the moon stands nearest the wanted
// eighth of its cycle (0 new .. 4 full .. 7): how a map or a test asks for "a full moon" without leaving the real sky: the
// calendar moves to a day that has it. `blood` = a full moon of a blood lunation.
inline std::int64_t instantWithPhase(std::int64_t worldMs, const Place& p, int phase, bool blood) {
  const std::int64_t day = static_cast<std::int64_t>(kMsPerDay);
  std::int64_t best = worldMs;
  double bestOff = 1e9;
  const int span = blood ? 90 : 30;
  for (int i = 0; i < span; ++i) {
    const std::int64_t t = worldMs + i * day;
    const State k = at(t, p);
    if (blood && (k.lunation <= 0 || k.lunation % 3 != 0)) continue;
    double off = std::abs(k.elongation - 45.0 * phase);
    off = std::min(off, 360.0 - off);
    if (off < bestOff) bestOff = off, best = t;
  }
  return best;
}

}  // namespace ember::sky
