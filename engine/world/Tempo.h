#pragma once

// CPU-only, random-access weather. All generation uses integer fixed point;
// floating point appears only at the presentation boundary. No renderer or GPU dependency.
#include <algorithm>
#include <array>
#include <cstdint>

namespace ember::tempo {
using I = std::int64_t;
inline constexpr I Q = 10000;
enum Field { Low, Middle, High, Shape, Vertical, Base, Thickness, Rain, HeavyRain,
             Lightning, WindDirection, WindSpeed, Gust, Humidity, Haze, Fog, ValleyFog,
             Temperature, Wetness, Count };
inline constexpr const char* names[Count] = {
  "Nuvens baixas", "Nuvens medias", "Nuvens altas", "Tipo baixo", "Crescimento vertical",
  "Base (m)", "Espessura (m)", "Chuva", "Chuva forte", "Raios por minuto",
  "Vento (graus)", "Vento (m/s)", "Rajada", "Umidade", "Nevoa seca", "Neblina no chao",
  "Neblina de vale", "Temperatura (C)", "Chao molhado"};
enum class State { Seasonal, Cirrus, Cirrostratus, Altostratus, Nimbostratus, Stratus,
                   WarmClearing, ColdStorm, ColdShowers, ColdClearing, HeatStorm, RadiationFog };
inline const char* label(State s) {
  static constexpr const char* labels[] = {"media da estacao", "frente quente: cirrus", "frente quente: cirrostratus",
    "frente quente: altostratus", "frente quente: chuva continua", "frente quente: stratus",
    "frente quente: abrindo", "frente fria: tempestade", "frente fria: pancadas",
    "frente fria: cumulus", "tempestade de calor", "neblina de radiacao"};
  return labels[static_cast<int>(s)];
}
struct Config {
  bool enabled = true;
  std::uint32_t seed = 1;
  int chaos = 500; // thousandths, serialized exactly
  int forced = 0; // 0 natural, 1 clear, 2 overcast, 3 warm, 4 cold, 5 storm, 6 fog
  bool operator==(const Config&) const = default;
};
struct Sample {
  std::array<I, Count> v{};
  State state = State::Seasonal;
  double value(int field) const { return static_cast<double>(v[static_cast<size_t>(field)]) / Q; }
  bool operator==(const Sample&) const = default;
};
inline I floorDiv(I a, I b) { return a / b - (a % b < 0); }
inline I mod(I a, I b) { return a - floorDiv(a,b)*b; }
// Same wrapping integer-hash family as world/Noise.h.
inline std::uint32_t hash(I slot, std::uint32_t seed, unsigned channel) {
  auto h = static_cast<std::uint32_t>(slot) * 374761393u + seed * 668265263u + channel * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
inline I rand(I slot, const Config& c, unsigned channel) { return hash(slot,c.seed,channel) % Q; }
inline I mul(I a, I b) { return a*b/Q; }
inline I lerp(I a, I b, I u) { return a + mul(b-a,u); }
inline I ramp(I age, I duration) {
  I u = std::clamp<I>(age*Q/duration,0,Q);
  return mul(mul(u,u),3*Q-2*u);
}
inline I pulse(I t, I start, I end, I edge) {
  return mul(ramp(t-start,edge), Q-ramp(t-(end-edge),edge));
}
inline I wave(I t, I period, I peak) {
  I x = mod(t-peak,period);
  I d = std::min(x,period-x);
  return Q-ramp(d,period/2);
}
inline I noise(I minute, I span, const Config& c, unsigned channel) {
  I slot = floorDiv(minute,span);
  return lerp(rand(slot,c,channel),rand(slot+1,c,channel),ramp(mod(minute,span),span));
}
inline I summer(I minute) { return wave(minute,365*1440,172*1440); }
inline Sample background(I minute, const Config& c) {
  Sample s;
  I hot = summer(minute), chaos = std::clamp(c.chaos,0,1000)*10;
  I n = mul(noise(minute,720,c,1)-5000,chaos);
  s.v[Low] = std::clamp<I>(2500 + (Q-hot)/5+n/3,0,Q);
  s.v[Middle] = 1200 + (Q-hot)/8+n/6;
  s.v[High] = 1600 + n/5;
  const I afternoon=pulse(mod(minute,1440),720,1140,180);
  s.v[Low]+=mul(mul(hot,1800),afternoon);
  s.v[Shape] = 3500+hot/3+mul(afternoon,1500);
  s.v[Vertical] = hot/10+mul(mul(hot,2500),afternoon);
  s.v[Base] = 900*Q; s.v[Thickness] = 450*Q;
  // Direction stays in a continuous, unwrapped degree interval (no 359 -> 0 discontinuity).
  s.v[WindDirection] = 180*Q+mul(noise(minute,1080,c,2)-5000,60*Q);
  s.v[WindSpeed] = 2*Q+mul(noise(minute,720,c,3),3*Q);
  s.v[Gust] = 1000+mul(chaos,noise(minute,120,c,4))/5;
  s.v[Humidity] = 5500+(Q-hot)/6+n/6;
  s.v[Haze] = 800+hot/10;
  s.v[Temperature] = 2*Q+mul(hot,24*Q)+mul(wave(minute,1440,900),6*Q)+n*3;
  return s;
}
inline void blend(Sample& s, const Sample& target, I weight) {
  for (int i=0;i<Count;++i) s.v[i] = lerp(s.v[i],target.v[i],weight);
}
inline Sample frontTarget(const Sample& base, int phase, bool warm) {
  Sample s=base;
  if (warm) {
    constexpr I low[] = {2000,2500,5000,9500,8500,2000};
    constexpr I mid[] = {1500,3500,8500,9000,5000,1500};
    constexpr I high[] = {6500,9000,9500,9000,5000,2000};
    constexpr I rain[] = {0,0,0,6500,2500,0};
    s.v[Low]=low[phase]; s.v[Middle]=mid[phase]; s.v[High]=high[phase]; s.v[Rain]=rain[phase];
    s.v[Shape]=1000; s.v[Base]=(phase>=3?450:900)*Q; s.v[Humidity]=phase>=3?9200:7500;
    s.v[Lightning]=0; s.v[HeavyRain]=phase>=3?1500:0; s.v[Vertical]=1000;
    s.v[Temperature]+=phase>=4?4*Q:0;
    s.state=static_cast<State>(static_cast<int>(State::Cirrus)+phase);
  } else {
    constexpr I low[]={9500,6500,3000};
    constexpr I rain[]={9500,3500,0};
    s.v[Low]=low[phase]; s.v[Middle]=low[phase]/2; s.v[Rain]=rain[phase];
    s.v[HeavyRain]=phase==0?9000:2500; s.v[Lightning]=phase==0?6500:0;
    s.v[Shape]=9000; s.v[Vertical]=phase==0?Q:3000;
    s.v[Thickness]=(phase==0?3500:700)*Q; s.v[WindSpeed]=(phase==0?16:7)*Q;
    s.v[Gust]=phase==0?9000:3500; s.v[Humidity]=phase==0?9500:6500;
    s.v[Temperature]-=4*Q;
    s.state=static_cast<State>(static_cast<int>(State::ColdStorm)+phase);
  }
  return s;
}
inline Sample synoptic(I minute, const Config& c) {
  Sample s=background(minute,c);
  const I chaos=std::clamp(c.chaos,0,1000)*10, slot=floorDiv(minute,2880);
  const I onset=slot*2880+rand(slot,c,10)*240/Q;
  const I hot=summer(onset), equinox=Q-std::abs(hot-5000)*2;
  const I chance=900+mul(chaos,5700)+equinox/8;
  if(rand(slot,c,11)>=chance) return s;
  const bool warm=rand(slot,c,12)<5000-equinox/5;
  const I duration=warm ? 720+mul(rand(slot,c,13)*1440/Q,Q-chaos/3) : 360+mul(rand(slot,c,13)*540/Q,Q-chaos/2);
  const I age=minute-onset;
  if(age<0 || age>=duration) return s;
  // Warm rain starts 6-8 hours after the first high clouds. The sequence never changes.
  const I rainStart=360+rand(slot,c,14)*120/Q;
  const std::array<I,8> knots=warm ? std::array<I,8>{0,90,180,rainStart,rainStart+60,
    rainStart+(duration-rainStart)/2,duration-90,duration} :
    std::array<I,8>{0,60,duration/3,duration*2/3,duration, duration,duration,duration};
  int k=0;
  while(k<(warm?6:3) && age>=knots[k+1]) ++k;
  auto node=[&](int i) {
    if(i==0 || i==(warm?7:4)) return background(minute,c);
    return frontTarget(background(minute,c),std::min(i-1,warm?5:2),warm);
  };
  Sample a=node(k),b=node(k+1);
  I weight=ramp(age-knots[k],knots[k+1]-knots[k]);
  blend(a,b,weight);
  a.state=weight<5000?a.state:b.state;
  blend(s,a,3500+mul(chaos,6500)); s.state=a.state;
  return s;
}
inline Sample atmosphere(I minute, const Config& c) {
  Sample s=synoptic(minute,c);
  const I day=floorDiv(minute,1440), tod=mod(minute,1440), hot=summer(minute);
  const I chaos=std::clamp(c.chaos,0,1000)*10;
  // One isolated cell per day, 20-30 minutes, only in the afternoon.
  const I chance=mul(hot,500+mul(chaos,4000)) + (hot<3000?mul(chaos,150):0);
  const I start=day*1440+840+rand(day,c,20)*180/Q;
  if(rand(day,c,21)<chance && (hot>7500 || chaos>0)) {
    I cell=pulse(minute,start,start+20+rand(day,c,22)*10/Q,8);
    cell=mul(cell,Q-s.v[Rain]);
    Sample storm=frontTarget(s,0,false); blend(s,storm,cell);
    if(cell>5000) s.state=State::HeatStorm;
  }
  // Radiation fog requires a clear, calm preceding night, rather than today's random fog roll.
  Sample night=synoptic(day*1440+180,c);
  I clear=Q-std::max({night.v[Low],night.v[Middle],night.v[High]});
  I calm=Q-ramp(night.v[WindSpeed]-Q,4*Q);
  I fog=mul(mul(clear,calm),mul(3500+(Q-hot)/2,pulse(tod,240,600,120)));
  fog=mul(fog,Q-s.v[Rain]);
  s.v[Fog]=fog; s.v[ValleyFog]=mul(fog,8500);
  if(fog>2500) s.state=State::RadiationFog;
  return s;
}
inline Sample minute(I minute, const Config& c) {
  if(!c.enabled) {
    Sample s; s.v[Low]=4000; s.v[Base]=380*Q; s.v[Thickness]=260*Q;
    // Legacy renderer values remain untouched: callers must bypass adapters when disabled.
    return s;
  }
  Sample s=atmosphere(minute,c);
  // Finite, fixed-cost history convolution: wet ground remembers rain, even after a time jump.
  I wet=0, weights=0;
  for(I k=0;k<12;++k) { I w=12-k; wet+=atmosphere(minute-k*60,c).v[Rain]*w; weights+=w; }
  s.v[Wetness]=std::min(Q,wet*2/weights);
  if(c.forced) {
    if(c.forced==1) s.v[Low]=s.v[Middle]=s.v[High]=s.v[Rain]=s.v[HeavyRain]=s.v[Lightning]=s.v[Vertical]=s.v[Fog]=s.v[ValleyFog]=0;
    if(c.forced==2) { s.v[Low]=9000; s.v[Middle]=8000; }
    if(c.forced==3) s=frontTarget(s,3,true);
    if(c.forced==4 || c.forced==5) s=frontTarget(s,0,false);
    if(c.forced==6) { s.v[Fog]=8000; s.v[ValleyFog]=9000; s.v[WindSpeed]=Q; s.v[Rain]=s.v[Lightning]=0; }
  }
  return s;
}
// Linear interpolation between deterministic minute samples. Seeking and walking use the same path.
inline Sample at(I milliseconds, const Config& c) {
  I m=floorDiv(milliseconds,60000), u=mod(milliseconds,60000)*Q/60000;
  Sample a=minute(m,c); if(u==0) return a;
  Sample b=minute(m+1,c); blend(a,b,u); return a;
}
// One coherent sky (the user, 2026-11-10: "YOU CANT HAVE RAIN WITHOUT RAINY CLOUDS", rain fell from a blue sky with a few white
// clouds under a full sun). Whatever made the sample (the seeded Tempo, Atmos 2's two layers, a forced state), every reader gets
// it through this: rain closes the sky with a low, thick, grey deck in step with its strength, lightning needs heavy rain, fog
// needs a calm, humid air. Nothing is taken from a sky that is already closed; a dry sample comes back unchanged.
inline Sample settle(Sample s) {
  // A trace of rain under a mostly open sky does not fall (Atmos 2 gives 0.05-0.1 mm/h under a third of cloud: it closed a sunny
  // morning into a grey one): rain stands when it is real (over a quarter of full strength) or the sky is already cloudy.
  const I keep = std::max(ramp(std::max(s.v[Rain], s.v[HeavyRain]) - 1500, 2000), ramp(s.v[Low] - 4000, 3000));
  s.v[Rain] = mul(s.v[Rain], keep); s.v[HeavyRain] = mul(s.v[HeavyRain], keep);
  const I rain = std::clamp<I>(std::max(s.v[Rain], s.v[HeavyRain]), 0, Q);
  if (rain > 0) {
    const I onset = ramp(rain, 1500);                    // the deck closes over the first 15 % of the rain's strength
    const I deck = mul(onset, lerp(8500, Q, rain));     // drizzle: 85 % cover; a downpour: the whole sky
    s.v[Low] = std::max(s.v[Low], deck);
    s.v[Middle] = std::max(s.v[Middle], mul(deck, 7000));
    s.v[Base] = std::min(s.v[Base], lerp(900 * Q, 350 * Q, onset));
    s.v[Thickness] = std::max(s.v[Thickness], lerp(450 * Q, 1600 * Q, mul(onset, 5000 + rain / 2)));
    s.v[Humidity] = std::max(s.v[Humidity], lerp(s.v[Humidity], 9000, onset));
    s.v[Wetness] = std::max(s.v[Wetness], mul(onset, 4000));
  }
  s.v[Lightning] = mul(s.v[Lightning], ramp(std::clamp<I>(s.v[HeavyRain], 0, Q), 3000));
  return s;
}
class Timeline {
  Config config_{};
  Sample origin_{};
  I changedAt_=0;
  bool initialized_=false;
  mutable I cachedMinute_=INT64_MIN;
  mutable Sample cachedA_{}, cachedB_{};
 public:
  const Config& config() const { return config_; }
  Sample at(I milliseconds) const {
    if(!config_.enabled) return minute(0,config_);
    I m=floorDiv(milliseconds,60000);
    if(m!=cachedMinute_) {
      cachedA_=minute(m,config_); cachedB_=minute(m+1,config_); cachedMinute_=m;
    }
    Sample target=cachedA_;
    blend(target,cachedB_,mod(milliseconds,60000)*Q/60000);
    if(initialized_ && milliseconds>=changedAt_ && milliseconds<changedAt_+600000) {
      Sample s=origin_; blend(s,target,ramp(milliseconds-changedAt_,600000)); s.state=target.state; return s;
    }
    return target;
  }
  void configure(Config c, I milliseconds, bool seek=false) {
    if(!initialized_ || seek) { config_=c; initialized_=true; changedAt_=milliseconds-600000; cachedMinute_=INT64_MIN; return; }
    if(c==config_) return;
    origin_=at(milliseconds); config_=c; changedAt_=milliseconds; cachedMinute_=INT64_MIN;
  }
};
} // namespace ember::tempo
