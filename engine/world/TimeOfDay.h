#pragma once

// Ember's time of day (Engine2 Mission.timeOfDay), as Engine2 defines it: game design data, ported as is.
//   battleLight  C:/Engine2/src/ember/battleLight.ts TIME_OF_DAY_LIGHT: the user's tuned key and ambient strengths,
//                colours, and the low sun of dawn (20 deg) and dusk (15 deg); a map's own sunIntensity /
//                ambientIntensity override the strengths.
//   stage        C:/Engine2/src/render/stage.ts TIME_PRESETS: what Engine2's 3D stage actually drew (key light
//                direction and colour, hemisphere sky/ground; it ignored the map's sliders). EmberEngine keeps it
//                as the "Engine2" lighting mode, the floor to compare against.
//   moon         C:/Engine2/src/render/moon.ts: at night the key light is the moon, at its pose in the night;
//                C:/Engine2/src/game/moonPhase.ts: tonight's phase (its art on the disc, its light's strength).

#include <cmath>
#include <string>
#include <string_view>

namespace ember::e2 {

struct TimeOfDayLight {
  const char* name;
  bool moon;                   // the key light is the moon
  float key, ambient;          // battleLight: default strengths (a map's sliders override them)
  unsigned keyColor, skyColor; // battleLight colours (sRGB hex)
  float elevationDeg;          // battleLight's low sun (dawn, dusk); < 0 = the stage preset's
  // stage.ts (Engine2's 3D): key light colour, strength, direction (radians), hemisphere sky/ground and strength
  unsigned stageSun;
  float stageSunIntensity, stageAzimuth, stageElevation;
  unsigned stageSky, stageGround;
  float stageHemi;
};

inline constexpr TimeOfDayLight kTimesOfDay[] = {
    {"day", false, 3.5f, 1.4f, 0xfff0d6, 0xfff2df, -1.0f, 0xfff0d4, 3.0f, -0.95f, 0.95f, 0xd4e6ff, 0x5d4b33, 1.0f},
    {"noon", false, 5.0f, 2.0f, 0xfff0d6, 0xfff2df, -1.0f, 0xffffff, 3.5f, -0.6f, 1.25f, 0xdcecff, 0x64523a, 1.1f},
    {"dawn", false, 3.2f, 1.3f, 0xffc8a8, 0xf2d8d4, 20.0f, 0xffd2a6, 3.0f, -1.6f, 0.45f, 0xe2d0de, 0x6a5848, 1.05f},
    {"dusk", false, 2.8f, 1.1f, 0xffca9f, 0xf7ddc3, 15.0f, 0xffb47e, 2.9f, 1.6f, 0.42f, 0xc4b2d4, 0x5e4840, 1.0f},
    {"brightNight", true, 1.8f, 0.6f, 0x9fb4ff, 0xb4c0e4, -1.0f, 0xc2d0ff, 2.2f, -0.7f, 0.9f, 0x7d90ba, 0x3a3a48, 0.0f},
    {"darkNight", true, 0.9f, 0.24f, 0x9fb4ff, 0xb4c0e4, -1.0f, 0xb0c0f4, 1.7f, -0.7f, 0.9f, 0x62759e, 0x2e2e3a, 0.0f},
};

// The map's time of day ("" or unknown = day, as Engine2).
inline const TimeOfDayLight& timeOfDay(std::string_view name) {
  for (const TimeOfDayLight& t : kTimesOfDay)
    if (name == t.name) return t;
  return kTimesOfDay[0];
}

// The hour of the 24 h sky a map's own time of day stands for (the user, 2026-10-23: "todo mapa deve seguir o horário da campanha, no
// debug o horário que estiver no mapa"): Modo teste and the Map Editor's maps have no clock, so their sky takes this hour. The day
// hours put the cycle's sun (62 deg at noon, MapStage::cycleLighting) at the height the time of day already had: day 54 deg (10.1 h),
// dawn 20 deg (7.25 h), dusk 15 deg (17.07 h); the nights are the panel's own noite (22 h) and madrugada (3 h).
inline float mapHour(std::string_view name) {
  if (name == "noon") return 12.0f;
  if (name == "dawn") return 7.25f;
  if (name == "dusk") return 17.07f;
  if (name == "brightNight") return 22.0f;
  if (name == "darkNight") return 3.0f;
  return 10.1f;  // "day", "" or unknown
}

// Engine2's moon pose (moon.ts Moon.pose): azimuth and elevation (radians) `progress` through the night
// (0 rising, 0.5 highest, 1 setting); the stage keeps it at 0.35 by default.
struct MoonPose {
  float azimuth, elevation;
};
inline MoonPose moonPose(float progress) {
  constexpr float kStartYaw = -30.0f * 3.14159265f / 180.0f;  // cameraRig START_YAW
  const float ahead = std::atan2(-std::sin(kStartYaw), -std::cos(kStartYaw));
  const float p = progress < 0.0f ? 0.0f : progress > 1.0f ? 1.0f : progress;
  const float height = std::sin(3.14159265f * p);
  return {ahead + 0.5f * (1.0f - 2.0f * p), 0.45f + (0.75f - 0.45f) * height};
}

// Engine2's moon phases (moonPhase.ts): an 8-day cycle, and the blood moon in place of every third full moon.
inline constexpr const char* kMoonPhases[] = {"new",  "waxingCrescent", "firstQuarter", "waxingGibbous", "full",
                                              "waningGibbous", "lastQuarter", "waningCrescent", "blood"};
// The campaign's moon on its day (save.gameClock; the world map's moon plate, OverworldStage.h): an 8-day cycle from the new moon on
// day 1, every third full moon a blood moon.
inline const char* campaignMoonPhase(int day) {
  const int d = day > 1 ? day - 1 : 0;
  if (d % 8 == 4 && (d / 8) % 3 == 2) return "blood";
  return kMoonPhases[d % 8];
}
// The phase's art in Engine2's public folder (moonPhaseIcon).
inline std::string moonPhaseIcon(std::string_view phase) {
  return phase == "full" ? std::string("game/ui/moon/moon-full-v2.png") : "game/ui/moon/moon-" + std::string(phase) + ".png";
}
// Tonight's moonlight (moonlightFor): its strength (x the night's), and the blood moon's red cast (light, sky).
struct Moonlight {
  float strength = 1.0f;
  unsigned color = 0, sky = 0;  // 0 = the night's own
};
inline Moonlight moonlightFor(std::string_view phase) {
  if (phase == "blood") return {1.35f, 0xe0a49c, 0x86707c};
  int i = 4;  // full
  for (int k = 0; k < 8; ++k)
    if (phase == kMoonPhases[k]) i = k;
  const float lit = (1.0f - std::cos(i / 8.0f * 2.0f * 3.14159265f)) / 2.0f;
  return {0.5f + 0.9f * lit};
}

// Towards the light, for an Engine2 azimuth / elevation (stage.ts placeLight: x = sin az cos el, z = cos az cos el).
inline void lightDirection(float azimuth, float elevation, float out[3]) {
  out[0] = std::sin(azimuth) * std::cos(elevation);
  out[1] = std::sin(elevation);
  out[2] = std::cos(azimuth) * std::cos(elevation);
}

}  // namespace ember::e2
