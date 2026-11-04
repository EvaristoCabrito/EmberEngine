#pragma once
#include "world/Tempo.h"
#include "world/CampaignTime.h"
#include <fstream>

namespace ember {
inline tempo::Config tempoDefaults() {
  tempo::Config c;
  std::ifstream in("content/tempo/v1/settings.json");
  auto j = nlohmann::json::parse(in,nullptr,false);
  if(j.is_object()) c.chaos=std::clamp(j.value("chaosMilli",500),0,1000);
  return c; // new weather is enabled by default
}
template<class SaveJson>
inline void ensureTempoSave(SaveJson& save) {
  if(!save.contains("weatherSeed")) {
    const auto stamp=save.value("updatedAt",std::int64_t(1));
    save["weatherSeed"]=tempo::hash(stamp,static_cast<std::uint32_t>(stamp>>32),81);
  }
  if(!save.contains("campaignId")) save["campaignId"]=save["weatherSeed"];
  if(!save.contains("realPlaySeconds")) save["realPlaySeconds"]=0.0;
  if(save.value("weatherSystemVersion",0)<1) { save["weatherEnabled"]=true; save["weatherSystemVersion"]=1; }
  if(!save.contains("weatherChaosMilli")) save["weatherChaosMilli"]=tempoDefaults().chaos;
}
template<class SaveJson>
inline tempo::Config tempoFromSave(const SaveJson& save) {
  auto c=tempoDefaults(); c.enabled=save.value("weatherSystemVersion",0)<1 || save.value("weatherEnabled",true);
  c.seed=save.value("weatherSeed",std::uint32_t(1));
  c.chaos=std::clamp(save.value("weatherChaosMilli",c.chaos),0,1000);
  return c;
}
// Normal play has no weather configuration. Preserve only the campaign's sequence;
// activation and variability come from the game's algorithm/content, never debug sliders.
template<class SaveJson>
inline tempo::Config tempoForCampaign(const SaveJson& save) {
  auto c=tempoDefaults();
  c.enabled=true;
  c.seed=save.value("weatherSeed",std::uint32_t(1));
  c.forced=0;
  return c;
}
template<class SaveJson>
inline void writeTempoSave(SaveJson& save,tempo::Config c) {
  save["weatherSystemVersion"]=1; save["weatherEnabled"]=c.enabled; save["weatherSeed"]=c.seed; save["weatherChaosMilli"]=c.chaos;
  // Forced states are debugging only and never contaminate a campaign.
}
} // namespace ember
