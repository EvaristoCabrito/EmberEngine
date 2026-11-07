#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>
#include <vector>

#include "render/VfxDraw.h"

namespace ember {

// World-space compressed ground field. The caller supplies terrain-sampled hex centres and owns its lifetime.
class BurialWeightFx {
 public:
  void start(int id, std::vector<glm::vec3> cells, glm::vec3 centre, int tier, float now) {
    fields_[id] = {std::move(cells), centre, std::clamp(tier, 0, 2), now, -1.0f};
  }
  void end(int id, float now) { if (auto it = fields_.find(id); it != fields_.end()) it->second.end = now; }
  void rootStart(int id, glm::vec3 feet, float height, float now) { roots_[id] = {feet, std::clamp(height,1.0f,4.0f), now, -1.0f}; }
  void rootMove(int id, glm::vec3 feet, float height) { if (auto it=roots_.find(id); it!=roots_.end()) it->second.feet=feet, it->second.height=std::clamp(height,1.0f,4.0f); }
  void rootEnd(int id, float now) { if (auto it=roots_.find(id); it!=roots_.end()) it->second.end=now; }
  void clear() { fields_.clear(); roots_.clear(); }
  bool active(float) const { return !fields_.empty() || !roots_.empty(); }

  void build(float now, VfxFrame& frame, std::vector<VfxLight>& lights) {
    std::erase_if(fields_, [now](const auto& item) { return item.second.end >= 0.0f && now >= item.second.end + 1.2f; });
    std::erase_if(roots_, [now](const auto& item) { return item.second.end >= 0.0f && now >= item.second.end + 0.7f; });
    for (const auto& [id, r] : roots_) {
      (void)id;
      const float fade=r.end<0.0f?0.42f:std::clamp((r.end+0.7f-now)/0.7f,0.0f,1.0f);
      const float y=r.feet.y+r.height*0.23f, radius=0.36f;
      std::vector<VfxVertex> mesh;
      for(int k=0;k<12;++k){
        const float a=2.0f*std::numbers::pi_v<float>*k/12.0f,b=2.0f*std::numbers::pi_v<float>*(k+1)/12.0f;
        const glm::vec3 p{r.feet.x+std::cos(a)*radius,y,r.feet.z+std::sin(a)*radius},q{r.feet.x+std::cos(b)*radius,y,r.feet.z+std::sin(b)*radius};
        const glm::vec3 n{0,1,0}; const float w=0.012f;
        mesh.insert(mesh.end(),{{p+glm::vec3(w,0,0),0,n,0},{q+glm::vec3(w,0,0),1,n,1},{p-glm::vec3(w,0,0),0,n,0},{p-glm::vec3(w,0,0),0,n,0},{q+glm::vec3(w,0,0),1,n,1},{q-glm::vec3(w,0,0),1,n,1}});
      }
      VfxDraw draw; draw.gpu.model=glm::mat4(1.0f); draw.gpu.color=glm::vec4(0.43f,0.12f,0.08f,fade); draw.gpu.firstVertex=frame.addVertices(mesh); draw.gpu.vertexCount=static_cast<uint32_t>(mesh.size());
      draw.gpu.firstInstance=0xFFFFFFFFu; draw.gpu.material=static_cast<uint32_t>(VfxMaterial::Basic); draw.gpu.sightAt=r.feet; draw.depthTest=true; draw.blend=VfxBlend::Additive; frame.draws.push_back(draw);
    }
    for (const auto& [id, f] : fields_) {
      (void)id;
      const float age = std::max(0.0f, now - f.start);
      const float fade = f.end < 0.0f ? 0.72f : std::clamp((f.end + 1.2f - now) / 1.2f, 0.0f, 1.0f);
      const float slam = std::clamp(age / 0.24f, 0.0f, 1.0f);
      const float pulse = 0.5f + 0.5f * std::sin(now * 3.7f);
      std::vector<VfxVertex> mesh;
      // Six low, terrain-level pressure ribs around the selected hex and six adjacent hexes.
      for (size_t i = 0; i < f.cells.size(); ++i) {
        const glm::vec3 p = f.cells[i];
        const float r = i == 0 ? 0.72f : 0.28f;
        const float y = p.y + 0.025f;
        for (int k = 0; k < 6; ++k) {
          const float a = 2.0f * std::numbers::pi_v<float> * k / 6.0f, b = 2.0f * std::numbers::pi_v<float> * (k + 1) / 6.0f;
          const glm::vec3 q{p.x + std::cos(a) * r, y, p.z + std::sin(a) * r};
          const glm::vec3 w{p.x + std::cos(b) * r, y, p.z + std::sin(b) * r};
          const float width = 0.025f + f.tier * 0.006f;
          const glm::vec3 n{0.0f, 1.0f, 0.0f};
          const VfxVertex q0{q + glm::vec3(width,0,0),0,n,0}, q1{q - glm::vec3(width,0,0),1,n,0};
          const VfxVertex w0{w + glm::vec3(width,0,0),0,n,1}, w1{w - glm::vec3(width,0,0),1,n,1};
          mesh.insert(mesh.end(), {q0,w0,q1,q1,w0,w1});
        }
      }
      // Six shallow fracture wedges, with a dark stone body and restrained ember red at the compressed seam.
      const float radius = 0.8f + f.tier * 0.12f;
      for (int k = 0; k < 6; ++k) {
        const float a = 2.0f * std::numbers::pi_v<float> * (k + 0.13f) / 6.0f;
        const glm::vec3 from{f.centre.x + std::cos(a)*0.12f, f.centre.y + 0.035f, f.centre.z + std::sin(a)*0.12f};
        const glm::vec3 to{f.centre.x + std::cos(a)*radius, f.centre.y + 0.018f, f.centre.z + std::sin(a)*radius};
        const glm::vec3 side{-std::sin(a)*0.018f,0,std::cos(a)*0.018f};
        const glm::vec3 n{0,1,0};
        mesh.insert(mesh.end(), {{from-side,0,n,0},{to,1,n,1},{from+side,1,n,0}});
      }
      VfxDraw draw; draw.gpu.model = glm::mat4(1.0f); draw.gpu.color = glm::vec4(0.32f + 0.12f*pulse, 0.09f, 0.075f, fade * (0.4f + 0.6f*slam));
      draw.gpu.firstVertex = frame.addVertices(mesh); draw.gpu.vertexCount = static_cast<uint32_t>(mesh.size()); draw.gpu.firstInstance = 0xFFFFFFFFu;
      draw.gpu.material = static_cast<uint32_t>(VfxMaterial::Basic); draw.gpu.sightAt = f.centre; draw.depthTest = true; draw.blend = VfxBlend::Alpha;
      frame.draws.push_back(draw);
      // Small physical chips rise on impact, then fall; they are deliberately short and sparse.
      for (int k = 0; k < 9; ++k) {
        const float phase = age * (2.4f + k * 0.08f) - k * 0.16f;
        if (phase < 0.0f || phase > 1.4f) continue;
        const float angle = k * 2.39996f, r = 0.16f + 0.11f * k;
        const float lift = 0.42f * std::sin(std::min(phase, 1.0f) * std::numbers::pi_v<float>) - std::max(0.0f, phase - 1.0f) * 0.22f;
        const glm::vec3 p{f.centre.x + std::cos(angle)*r, f.centre.y + 0.045f + std::max(0.0f,lift), f.centre.z + std::sin(angle)*r};
        const float s = 0.035f + f.tier * 0.008f;
        std::vector<VfxVertex> shard{{p+glm::vec3(-s,0,0),0,{0,1,0},0},{p+glm::vec3(s,0,0),1,{0,1,0},0},{p+glm::vec3(0,s*2.0f,0),0,{0,1,0},1}};
        VfxDraw bit; bit.gpu.model=glm::mat4(1.0f); bit.gpu.color=glm::vec4(0.29f,0.25f,0.22f,fade*std::clamp(1.0f-phase/1.4f,0.0f,1.0f));
        bit.gpu.firstVertex=frame.addVertices(shard); bit.gpu.vertexCount=3; bit.gpu.firstInstance=0xFFFFFFFFu; bit.gpu.material=static_cast<uint32_t>(VfxMaterial::Basic); bit.gpu.sightAt=f.centre; bit.depthTest=true;
        frame.draws.push_back(bit);
      }
      lights.push_back({f.centre + glm::vec3(0,0.2f,0), {0.42f,0.09f,0.06f}, fade * (0.7f + f.tier*0.45f) * (1.0f-slam*0.35f), 3.0f});
    }
  }
 private:
  struct Root { glm::vec3 feet; float height,start,end; };
  struct Field { std::vector<glm::vec3> cells; glm::vec3 centre; int tier; float start, end; };
  std::unordered_map<int, Field> fields_;
  std::unordered_map<int, Root> roots_;
};
}  // namespace ember
