#include "world/Tempo.h"
#include "world/TempoSave.h"
#include "world/TempoConsumers.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace ember;
void require(bool ok,const char* msg) { if(!ok) throw std::runtime_error(msg); }
int main(int argc,char** argv) {
 try {
  tempo::Config c; c.enabled=true; c.seed=918273;
  std::uint64_t digest=1469598103934665603ull;
  const bool jump=argc>1 && std::string_view(argv[1])=="jump";
  const int days=60, count=days*1440;
  std::vector<tempo::Sample> samples(static_cast<size_t>(count));
  auto start=std::chrono::steady_clock::now();
  double worst=0;
  for(int i=0;i<count;++i) {
    const int m=jump?(i*7919)%count:i; // coprime: every instant visited in a different order
    samples[m]=tempo::minute(m,c);
  }
  auto finish=std::chrono::steady_clock::now();
  for(int m=0;m<count;++m) {
    for(int f=0;f<tempo::Count;++f) {
      const auto v=samples[m].v[f];
      digest=(digest^static_cast<std::uint64_t>(v))*1099511628211ull;
      const bool unit=f!=tempo::Base&&f!=tempo::Thickness&&f!=tempo::WindDirection&&f!=tempo::WindSpeed&&f!=tempo::Temperature;
      if(unit) require(v>=0&&v<=tempo::Q,"normalized range");
      if(m) {
        const double delta=std::abs(samples[m].value(f)-samples[m-1].value(f));
        const double bound=f==tempo::Base||f==tempo::Thickness?1200:f==tempo::WindDirection?2:f==tempo::WindSpeed?4:f==tempo::Temperature?2:0.3;
        if(delta>bound) { std::cerr<<"rate field="<<f<<" minute="<<m<<" delta="<<delta<<'\n'; require(false,"minute rate limit"); }
      }
    }
    require(tempo::at(static_cast<tempo::I>(m)*60000,c)==samples[m],"seek vs minute mismatch");
  }
  for(int chaos:{0,500,1000}) {
    c.chaos=chaos;
    for(int i=0;i<1000;++i) {
      auto t0=std::chrono::steady_clock::now();
      const auto s=tempo::at((i*100003ll+30*1440)*60000,c);
      auto t1=std::chrono::steady_clock::now();
      worst=std::max(worst,std::chrono::duration<double,std::milli>(t1-t0).count());
      require(s==tempo::at((i*100003ll+30*1440)*60000,c),"repeat determinism");
    }
  }
  // A warm front may not rain before its 6-8 h high-cloud warning.
  c.chaos=1000;
  int warm=0,cold=0;
  for(tempo::I slot=0;slot<400;++slot) {
    auto onset=slot*2880+tempo::rand(slot,c,10)*240/tempo::Q;
    auto hot=tempo::summer(onset), eq=tempo::Q-std::abs(hot-5000)*2;
    auto chance=900+5700+eq/8;
    if(tempo::rand(slot,c,11)>=chance) continue;
    if(tempo::rand(slot,c,12)<5000-eq/5) {
      ++warm;
      for(int m=0;m<=360;++m) require(tempo::synoptic(onset+m,c).v[tempo::Rain]==0,"warm rain before warning");
      auto rainStart=360+tempo::rand(slot,c,14)*120/tempo::Q;
      require(tempo::synoptic(onset+rainStart+1,c).v[tempo::Rain]>0,"missing warm rain");
    } else ++cold;
  }
  require(warm>0&&cold>0,"missing fronts");
  tempo::Timeline timeline;
  timeline.configure(c,0,true);
  auto before=timeline.at(600000);
  c.seed+=1; timeline.configure(c,600000);
  require(timeline.at(600000)==before,"config discontinuity");
  require(timeline.at(1200000)==tempo::at(1200000,c),"transition did not finish");
  timeline.configure(c,30ll*1440*60000,true);
  require(timeline.at(30ll*1440*60000)==tempo::at(30ll*1440*60000,c),"seek ramp");
  c.enabled=false;
  require(tempo::at(0,c)==tempo::at(9999999999,c),"off depends on time");
  timeline.configure(c,1800000);
  require(timeline.at(1800000)==tempo::at(1800000,c),"off must bypass transitions");
  for(const int fps:{30,60,144,240}) {
    RealtimeAccumulator accumulator;
    std::int64_t elapsed=0,previous=0;
    for(int frame=1;frame<=fps*60;++frame) {
      auto now=static_cast<std::int64_t>(frame)*60000000000ll/(fps*60);
      elapsed+=accumulator.consume(now-previous); previous=now;
    }
    require(elapsed==60000,"real-time clock depends on frame rate");
  }
  nlohmann::ordered_json save={{"gameClock",3},{"gameHour",23},{"gameMinute",59},
    {"heroPoisons",{{"Kael","lesser"}}},{"heroDiseases",{{"Neera","greater"}}}};
  require(tempo::Config{}.enabled,"new weather must be enabled by default");
  nlohmann::ordered_json oldWeather={{"weatherEnabled",false},{"weatherSeed",123}};
  ensureTempoSave(oldWeather);
  require(tempoFromSave(oldWeather).enabled,"old saves must migrate to active weather");
  tempo::Config manualOff; manualOff.enabled=false;
  writeTempoSave(oldWeather,manualOff); ensureTempoSave(oldWeather);
  require(!tempoFromSave(oldWeather).enabled,"explicit manual off must persist");
  const auto automatic=tempoForCampaign(oldWeather);
  require(automatic.enabled && automatic.forced==0 && automatic.chaos==tempoDefaults().chaos,"campaign must run automatic weather without player configuration");
  tempo::Config forced; forced.forced=5;
  const auto storm=tempo::minute(1000,forced);
  const auto effects=tempo::controls(storm);
  require(effects.rain>.9f && effects.coverage>.9f && effects.thickness>3000,"storm is disconnected from effects");
  require(effects.mieScale>1 && std::hypot(effects.windX,effects.windZ)>10,"air/wind controls");
  forced.forced=6;
  const auto fogControls=tempo::controls(tempo::minute(1000,forced));
  require(fogControls.fogMultiplier>3 && fogControls.valleyExtra>1,"fog modifiers");
  forced.forced=1;
  const auto clearControls=tempo::controls(tempo::minute(1000,forced));
  require(clearControls.coverage==0 && clearControls.rain==0,"forced clear effects");
  auto original=campaignMinute(save); syncAilmentDeadlines(save);
  advanceCampaignMinutes(save,1);
  require(save["gameClock"]==4&&save["gameHour"]==0&&save["gameMinute"]==0,"minute rollover");
  auto serialized=save.dump(); save=nlohmann::ordered_json::parse(serialized);
  require(save["heroPoisonTimers"]["Kael"]["expiresAtMinute"]==original+2880,"deadline changed after reload");
  advanceCampaignMinutes(save,2879);
  require(!save["heroPoisons"].contains("Kael"),"poison did not expire at 2 days");
  require(save["heroDiseases"].contains("Neera"),"strong disease expired early");
  advanceCampaignMinutes(save,4*1440);
  require(!save["heroDiseases"].contains("Neera"),"disease did not expire at 6 days");
  for(auto tier:{"lesser","poison","greater","deadly","lethal"}) {
    nlohmann::json s={{"heroPoisons",{{"P",tier}}},{"heroDiseases",{{"D",tier}}}};
    advanceCampaignMinutes(s,ailmentDays(tier)*1440-1);
    require(s["heroPoisons"].contains("P")&&s["heroDiseases"].contains("D"),"effect expired early");
    advanceCampaignMinutes(s,1);
    require(s["heroPoisons"].empty()&&s["heroDiseases"].empty(),"effect missed expiration");
  }
  std::cout<<"digest="<<digest<<" mode="<<(jump?"jump":"walk")<<" days="<<days
    <<" average_ms="<<std::chrono::duration<double,std::milli>(finish-start).count()/count
    <<" worst_seek_ms="<<worst<<" warm="<<warm<<" cold="<<cold<<" OK\n";
  return 0;
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
