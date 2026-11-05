#pragma once
#include "world/Tempo.h"
#include <imgui.h>

namespace ember {
struct TempoPanel {
  std::array<std::array<float,73>,tempo::Count> forecast{};
  tempo::Config forecastConfig{};
  std::int64_t forecastMinute=INT64_MIN;
  bool draw(tempo::Config& config, tempo::Timeline& timeline, std::int64_t milliseconds) {
    ImGui::SetNextItemOpen(true,ImGuiCond_Once);
    if(!ImGui::TreeNode("Tempo global")) return false;
    bool changed=ImGui::Checkbox("Tempo por semente",&config.enabled);
    changed|=ImGui::InputScalar("Semente",ImGuiDataType_U32,&config.seed);
    float chaos=config.chaos/1000.0f;
    if(ImGui::SliderFloat("Caos",&chaos,0,1,"%.3f")) { config.chaos=static_cast<int>(chaos*1000+0.5f); changed=true; }
    const char* states="Natural\0Limpo\0Nublado\0Frente quente\0Frente fria\0Tempestade\0Neblina\0";
    changed|=ImGui::Combo("Forcar estado (teste)",&config.forced,states);
    if(changed) timeline.configure(config,milliseconds);
    auto current=timeline.at(milliseconds);
    ImGui::Text("Estado: %s",tempo::label(current.state));
    ImGui::TextUnformatted(config.enabled?"Ativo; transicao de controles: 10 minutos de jogo":"Desligado; valores visuais existentes preservados");
    if(ImGui::TreeNode("Numeros e previsao: proximas 72 horas")) {
      auto m=tempo::floorDiv(milliseconds,60000);
      if(forecastMinute!=m || !(forecastConfig==config)) {
        for(int hour=0;hour<=72;++hour) {
          const auto s=tempo::minute(m+hour*60,config);
          for(int f=0;f<tempo::Count;++f) forecast[f][hour]=static_cast<float>(s.value(f));
        }
        forecastMinute=m; forecastConfig=config;
      }
      for(int f=0;f<tempo::Count;++f) {
        ImGui::Text("%s: %.4f",tempo::names[f],current.value(f));
        ImGui::PushID(f);
        ImGui::PlotLines("##72h",forecast[f].data(),73,0,"0 a 72 horas (1 h/amostra)",FLT_MAX,FLT_MAX,{0,46});
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
    ImGui::TreePop(); return changed;
  }
};
} // namespace ember
