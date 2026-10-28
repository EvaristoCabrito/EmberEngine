#pragma once

// Ember's Dev Controls switches (src/game/gfx/three/devGfx.ts) and graphics quality presets (graphicsQuality.ts):
// the settings themselves, shared by Dev Controls (TestModeStage), the Opções window (EmberMenus.h) and the battle
// (MapStage). Ember kept them in the browser's localStorage; here they live in %LOCALAPPDATA%\Ember\dev-gfx.json
// (written whole through a temporary file and a rename). How they act on a map: DevGfx.h.
//
//   realShadows       the sun's shadow layer (lighting layer 2)
//   shadowResolution  the sun / card shadow maps' edge (Renderer::setShadowResolution)
//   softShadows       the shadow filter widened (4 texels instead of 1.6)
//   localLights       the map's light sources (lighting layer 8)
//   fogOfWar          the fog of war on maps that use it (off: everything seen, every enemy shown)
//   fogDebug          the fog's states as flat colours (renderer debug view 4)
//   atmosphericFx     the map's mist, haze, wisps and dust motes
//   sun               a fixed sun (Ember's sliders: azimuth = the screen direction shadows fall, 0 right, 90 down;
//                     elevation above the horizon) instead of the time of day's sun, once a slider is moved
// Not in EmberEngine yet, so not offered: contact shadows, ambient occlusion, render scale (maxDpr), the moon's
// sliders (the moon stands where MapStage's moon placement puts it).

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

namespace ember {

struct MapStageOptions;
class Renderer;

struct DevGfx {
  std::string quality = "high";  // low / medium / high
  bool realShadows = true;
  uint32_t shadowResolution = 4096;
  bool softShadows = false;
  bool localLights = true;
  bool fogOfWar = true;
  bool fogDebug = false;
  bool atmosphericFx = true;
  bool sunMoved = false;  // the sliders were moved: a fixed sun
  float sunAzimuth = 53.13f, sunElevation = 45.0f;

  // graphicsQuality.ts profiles (only the switches EmberEngine has).
  struct Profile {
    uint32_t shadowResolution;
    bool realShadows, softShadows, localLights, atmosphericFx;
  };
  static Profile profile(const std::string& q) {
    if (q == "low") return {1024, false, false, true, false};
    if (q == "medium") return {2048, true, true, true, true};
    return {4096, true, false, true, true};
  }
  void setQuality(const std::string& q) {
    quality = q;
    const Profile p = profile(q);
    shadowResolution = p.shadowResolution, realShadows = p.realShadows, softShadows = p.softShadows, localLights = p.localLights,
    atmosphericFx = p.atmosphericFx;
  }
  bool custom() const {
    const Profile p = profile(quality);
    return p.shadowResolution != shadowResolution || p.realShadows != realShadows || p.softShadows != softShadows || p.localLights != localLights ||
           p.atmosphericFx != atmosphericFx;
  }

  static std::filesystem::path path() {
    const char* base = std::getenv("LOCALAPPDATA");
    return std::filesystem::path(base ? base : ".") / "Ember" / "dev-gfx.json";
  }
  static DevGfx load(const std::filesystem::path& file = path()) {
    DevGfx g;
    std::ifstream in(file, std::ios::binary);
    if (!in) return g;
    const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (!j.is_object()) return g;
    g.quality = j.value("quality", g.quality);
    g.realShadows = j.value("realShadows", g.realShadows);
    g.shadowResolution = j.value("shadowResolution", g.shadowResolution);
    g.softShadows = j.value("softShadows", g.softShadows);
    g.localLights = j.value("localLights", g.localLights);
    g.fogOfWar = j.value("fogOfWar", g.fogOfWar);
    g.fogDebug = j.value("fogDebug", g.fogDebug);
    g.atmosphericFx = j.value("atmosphericFx", g.atmosphericFx);
    g.sunMoved = j.value("sunMoved", g.sunMoved);
    g.sunAzimuth = j.value("sunAzimuth", g.sunAzimuth);
    g.sunElevation = j.value("sunElevation", g.sunElevation);
    return g;
  }
  bool save() const {
    const nlohmann::json j = {{"quality", quality},     {"realShadows", realShadows}, {"shadowResolution", shadowResolution},
                              {"softShadows", softShadows}, {"localLights", localLights}, {"fogOfWar", fogOfWar},
                              {"fogDebug", fogDebug},   {"atmosphericFx", atmosphericFx}, {"sunMoved", sunMoved},
                              {"sunAzimuth", sunAzimuth}, {"sunElevation", sunElevation}};
    std::error_code ec;
    std::filesystem::create_directories(path().parent_path(), ec);
    const std::filesystem::path tmp = path().string() + ".tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      if (!out) return false;
      out << j.dump(1);
      if (!out) return false;
    }
    std::filesystem::rename(tmp, path(), ec);
    return !ec;
  }

  // Onto a map about to open, and the renderer it draws with (DevGfx.h).
  void apply(MapStageOptions& m, Renderer& renderer) const;
};

}  // namespace ember
