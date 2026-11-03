#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {
// Camera-local world-space precipitation. Absolute world cells keep trajectories stable while panning.
// No assets, global sky changes or map mutations. Appends to the spell frame without replacing spells.
class WeatherFx {
  static float random(uint32_t n) {
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; n ^= n >> 16;
    return float(n & 0xffffffu) / 16777216.0f;
  }
  static float fract(float x) { return x - std::floor(x); }
  static void quad(VfxFrame& f, glm::vec3 p, glm::vec3 r, glm::vec3 u) {
    const glm::vec3 n = glm::normalize(glm::cross(r, u));
    const glm::vec3 a = p-r-u, b = p+r-u, c = p+r+u, d = p-r+u;
    f.vertices.insert(f.vertices.end(), {{a,0,n,0},{b,1,n,0},{c,1,n,1},{a,0,n,0},{c,1,n,1},{d,0,n,1}});
  }
  static VfxDraw draw(const VfxFrame& f, float mode, glm::vec4 color) {
    VfxDraw d;
    d.gpu.model = glm::mat4(1);
    d.gpu.firstVertex = static_cast<uint32_t>(f.vertices.size());
    d.gpu.firstInstance = 0xffffffffu;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Weather);
    d.gpu.params0.y = mode;
    d.gpu.color = color;
    d.blend = VfxBlend::Alpha;
    return d;
  }
  static void finish(VfxFrame& f, VfxDraw d) {
    d.gpu.vertexCount = static_cast<uint32_t>(f.vertices.size()) - d.gpu.firstVertex;
    if (d.gpu.vertexCount) f.draws.push_back(d);
  }
 public:
  // Ground callback returns the actual elevation under each drop, including raised terrain.
  template<class Ground>
  static float append(VfxFrame& f, std::string_view kind, float intensity, float wind,
                      float time, const Camera& camera, glm::vec3 center, Ground ground) {
    if ((kind != "rain" && kind != "storm" && kind != "snow" && kind != "ash") || !std::isfinite(time)) return 0;
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    wind = std::clamp(wind, 0.0f, 1.0f);
    if (intensity <= 0) return 0;
    const bool snow = kind == "snow", storm = kind == "storm", ash = kind == "ash";
    const glm::vec3 up = glm::normalize(glm::cross(camera.right, camera.forward));
    const float speed = ash ? .65f : snow ? 1.2f : storm ? 19.0f : 13.0f;
    const glm::vec3 velocity{wind * (ash ? 1.4f : snow ? 2.0f : 7.0f), -speed, wind * 1.6f};
    const glm::vec3 axis = glm::normalize(velocity);
    glm::vec3 side = glm::cross(axis, camera.forward);
    side = glm::dot(side, side) < 0.001f ? camera.right : glm::normalize(side);
    auto drops = draw(f, ash ? 4.0f : snow ? 1.0f : 0.0f, ash ? glm::vec4(.23f,.215f,.20f,.8f) : snow ? glm::vec4(.85f,.88f,.9f,.8f) : glm::vec4(.56f,.64f,.7f,.35f));
    // 49 x 49 cells with two or three drops each: bounded independently of map size.
    const int cx = int(std::floor(center.x / 1.5f)), cz = int(std::floor(center.z / 1.5f));
    const int layers = storm ? 3 : 2;
    for (int z = cz-24; z <= cz+24; ++z) for (int x = cx-24; x <= cx+24; ++x) {
      const uint32_t cell = uint32_t(x)*73856093u ^ uint32_t(z)*19349663u;
      for (int k = 0; k < layers; ++k) {
        const uint32_t seed = cell ^ uint32_t(k+1)*83492791u;
        if (random(seed+1) > intensity) continue;
        const float life = fract(random(seed+2) + time * speed * (ash ? .65f + random(seed+9)*.7f : 1.0f) / 18.0f);
        const float h = (1.0f-life)*18.0f;
        glm::vec3 p{(float(x)+random(seed+3))*1.5f, 0, (float(z)+random(seed+4))*1.5f};
        p += glm::vec3(velocity.x, 0, velocity.z) * (h / speed);
        if (snow) { p.x += std::sin(time*.8f+random(seed+5)*6.28f)*.32f; p.z += std::sin(time*.6f+random(seed+6)*6.28f)*.24f; }
        if (ash) {
          // Lightweight charred fragments drift in broad eddies and tumble, rather than fall like snow.
          const float phase = random(seed+10)*6.283185f;
          p.x += std::sin(time*.37f+phase)*(.4f+wind*.7f);
          p.z += std::cos(time*.29f+phase)*(.3f+wind*.5f);
        }
        p.y = ground(p.x,p.z) + h + .025f;
        // Flakes right at the lens filled the screen as huge soft blobs (the editor preview): none within 2 units of the eye,
        // faded in up to 5.
        const float lens = std::clamp((glm::length(p-camera.position)-2.0f)/3.0f,0.0f,1.0f);
        const float edge = std::clamp((36.0f-glm::length(glm::vec2(p.x-center.x,p.z-center.z)))/5.0f,0.0f,1.0f) * lens;
        if (edge <= 0) continue;
        // Taper geometry at the volume edge; no abrupt walls following the camera.
        const float size = (.018f+random(seed+7)*.035f)*edge;
        if (ash) {
          const float turn = time*(.5f+random(seed+11)*1.2f)+random(seed+12)*6.283185f;
          const glm::vec3 across = camera.right*std::cos(turn)+up*std::sin(turn);
          const glm::vec3 along = up*std::cos(turn)-camera.right*std::sin(turn);
          const float tumble = .25f+.75f*std::abs(std::sin(turn*.73f));
          quad(f,p,across*size*1.5f*tumble,along*size*(.65f+random(seed+13)*1.4f));
        }
        else if (snow) quad(f,p,camera.right*size,up*size*(.7f+random(seed+8)*.6f));
        else quad(f,p,side*.018f*edge,axis*(.16f+random(seed+8)*.24f)*edge*std::min(h/.5f,1.0f));
      }
    }
    finish(f,drops);
    if (!snow && !ash) {
      auto splashes = draw(f,2,glm::vec4(.5f,.58f,.63f,.18f*intensity));
      for (int z = cz-16; z <= cz+16; ++z) for (int x = cx-16; x <= cx+16; ++x) {
        const uint32_t seed = uint32_t(x)*73856093u ^ uint32_t(z)*19349663u;
        if (random(seed+21) > intensity*.4f) continue;
        const float age = fract(time*(1.7f+random(seed+22))+random(seed+23));
        const float size = .025f+.13f*age;
        glm::vec3 p{(float(x)+random(seed+24))*1.5f,0,(float(z)+random(seed+25))*1.5f};
        p.y = ground(p.x,p.z)+.018f;
        if (age < .65f) quad(f,p,{size,0,0},{0,0,size});
      }
      finish(f,splashes);
    }
    // An infrequent double lightning pulse, drawn in world space and occluded by scene depth.
    // Stable event positions (not camera-relative) avoid moving a bolt during a pan.
    const int event = int(std::floor(time/11.0f));
    const float age = std::fmod(time,11.0f) - (3.0f+random(uint32_t(event)+92)*3.0f);
    const float flash = storm ? (age >= 0 && age < .10f ? 1.0f-age/ .10f : age >= .18f && age < .24f ? .5f*(1.0f-(age-.18f)/.06f) : 0) : 0;
    if (flash > 0) {
      const uint32_t seed = uint32_t(event)*7919u;
      glm::vec3 base{(random(seed+31)-.5f)*30,0,(random(seed+32)-.5f)*30};
      base.y = ground(base.x,base.z);
      auto bolt = draw(f,3,glm::vec4(.78f,.86f,1,flash*intensity));
      bolt.blend = VfxBlend::Additive;
      glm::vec3 p = base+glm::vec3(0,24,0);
      for (int j=1; j<=16; ++j) {
        glm::vec3 next = base+glm::vec3((random(seed+uint32_t(j)*3)-.5f)*1.6f,24.0f*(1.0f-float(j)/16), (random(seed+uint32_t(j)*3+1)-.5f)*1.2f);
        const glm::vec3 along = (next-p)*.5f;
        glm::vec3 across = glm::cross(along,camera.forward);
        across = glm::dot(across,across) < .0001f ? camera.right : glm::normalize(across);
        quad(f,(p+next)*.5f,across*.045f,along);
        p = next;
      }
      finish(f,bolt);
    }
    return flash * intensity;
  }
};
} // namespace ember
