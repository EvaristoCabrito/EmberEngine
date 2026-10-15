#pragma once

// Ember's time of day (Engine2 Mission.timeOfDay), as Engine2 defines it: game design data, ported as is.
//   battleLight  C:/Engine2/src/ember/battleLight.ts TIME_OF_DAY_LIGHT: the user's tuned key and ambient strengths,
//                colours, and the low sun of dawn (20 deg) and dusk (15 deg); a map's own sunIntensity /
//                ambientIntensity override the strengths.
//   stage        C:/Engine2/src/render/stage.ts TIME_PRESETS: what Engine2's 3D stage actually drew (key light
//                direction and colour, hemisphere sky/ground; it ignored the map's sliders). EmberEngine keeps it
//                as the "Engine2" lighting mode, the floor to compare against.
//   moon         C:/Engine2/src/render/moon.ts: at night the key light is the moon, at its pose in the night.

#include <cmath>
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

// Towards the light, for an Engine2 azimuth / elevation (stage.ts placeLight: x = sin az cos el, z = cos az cos el).
inline void lightDirection(float azimuth, float elevation, float out[3]) {
  out[0] = std::sin(azimuth) * std::cos(elevation);
  out[1] = std::sin(elevation);
  out[2] = std::cos(azimuth) * std::cos(elevation);
}

}  // namespace ember::e2
