#pragma once
#include "world/Tempo.h"
#include <cmath>

namespace ember::tempo {
// Projection into the existing effects. The full three-layer state remains available to cloud renderers.
struct Controls {
  float coverage=0, base=0, thickness=0, gray=0, scale=0;
  float rain=0, heavyRain=0, lightning=0, windX=0, windZ=0;
  float fogMultiplier=1, valleyExtra=0, mieScale=1;
};
inline Controls controls(const Sample& s) {
  Controls c;
  const auto f=[&](int field){return static_cast<float>(s.value(field));};
  c.coverage=1-(1-f(Low))*(1-f(Middle)*.75f)*(1-f(High)*.5f);
  c.base=f(Base); c.thickness=f(Thickness)*(1+f(Vertical));
  c.gray=std::clamp(f(Rain)*.7f+f(Vertical)*.25f,0.0f,1.0f);
  c.scale=300+180*f(Shape)+220*f(Vertical);
  c.rain=f(Rain); c.heavyRain=f(HeavyRain); c.lightning=f(Lightning);
  const double angle=s.value(WindDirection)*3.14159265358979323846/180;
  const float speed=f(WindSpeed)*(1+f(Gust)*.35f);
  c.windX=static_cast<float>(std::cos(angle))*speed;
  c.windZ=static_cast<float>(std::sin(angle))*speed;
  c.fogMultiplier=1+f(Fog)*3;
  c.valleyExtra=f(ValleyFog)*2;
  c.mieScale=1+f(Humidity)*.5f+f(Haze)*2;
  return c;
}
// One seeded decision per game minute; frame order and frame rate cannot change its probability.
inline bool lightningAtMinute(I minute,const Config& c,const Sample& s) {
  return hash(minute,c.seed,91)%Q < static_cast<std::uint32_t>(s.v[Lightning]);
}
} // namespace ember::tempo
