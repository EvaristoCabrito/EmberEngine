#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace ember {
inline constexpr int kMinutesPerDay = 1440;
// Preserve fractional milliseconds instead of losing time at high frame rates.
struct RealtimeAccumulator {
  std::int64_t remainderNs=0;
  std::int64_t consume(std::int64_t elapsedNs) {
    const auto ns=std::max<std::int64_t>(0,elapsedNs)+remainderNs;
    remainderNs=ns%1000000;
    return ns/1000000;
  }
};
inline int ailmentDays(std::string_view tier) {
  if (tier == "poison") return 4;
  if (tier == "greater") return 6;
  if (tier == "deadly") return 10;
  if (tier == "lethal") return 20;
  return 2;
}
template<class SaveJson>
inline std::int64_t campaignMinute(const SaveJson& save) {
  return static_cast<std::int64_t>(save.value("gameClock", 0.0)) * kMinutesPerDay +
         save.value("gameHour", 8) * 60 + save.value("gameMinute", 0);
}
template<class SaveJson>
inline void setCampaignMinute(SaveJson& save, std::int64_t minute) {
  minute = std::max<std::int64_t>(0, minute);
  save["gameClock"] = minute / kMinutesPerDay;
  save["gameHour"] = (minute / 60) % 24;
  save["gameMinute"] = minute % 60;
}
// Keep legacy flags/strings for all existing readers. Deadlines live separately and survive reloads.
template<class SaveJson>
inline void syncAilmentDeadlines(SaveJson& save, std::int64_t startMinute = -1) {
  const auto now = campaignMinute(save);
  if (startMinute < 0) startMinute = now;
  for (bool poison : {true, false}) {
    auto& flags = save[poison ? "heroPoisons" : "heroDiseases"];
    auto& timers = save[poison ? "heroPoisonTimers" : "heroDiseaseTimers"];
    if (!flags.is_object()) flags = nlohmann::json::object();
    if (!timers.is_object()) timers = nlohmann::json::object();
    for (auto it = timers.begin(); it != timers.end();) {
      if (!flags.contains(it.key()) || flags[it.key()] == false || flags[it.key()].is_null()) it = timers.erase(it);
      else ++it;
    }
    for (auto it = flags.begin(); it != flags.end();) {
      if (it.value() == false || it.value().is_null()) { ++it; continue; }
      const std::string tier = it.value().is_string() ? it.value().template get<std::string>() :
        it.value().is_object() ? it.value().value("tier",std::string("lesser")) :
        it.value().is_number() && it.value()==10 ? "poison" : "lesser";
      const std::string type=it.value().is_object()?it.value().value("type",std::string("disease")):poison?"poison":"disease";
      if(poison) it.value()=tier;
      auto& timer = timers[it.key()];
      if (!timer.is_object() || timer.value("tier", std::string()) != tier)
        timer = {{"tier", tier}, {"type", type},
                 {"expiresAtMinute", startMinute + ailmentDays(tier) * kMinutesPerDay}};
      if (timer.value("expiresAtMinute", now + 1) <= now) {
        timers.erase(it.key());
        it = flags.erase(it);
      } else ++it;
    }
  }
}
template<class SaveJson>
inline void advanceCampaignMinutes(SaveJson& save, std::int64_t elapsed) {
  const auto before = campaignMinute(save);
  syncAilmentDeadlines(save);
  setCampaignMinute(save, before + elapsed);
  syncAilmentDeadlines(save, before);
}
template<class SaveJson>
inline std::int64_t campaignMilliseconds(const SaveJson& save) {
  return campaignMinute(save)*60000 + save.value("gameSubMinuteMs", std::int64_t(0));
}
template<class SaveJson>
inline void setCampaignMilliseconds(SaveJson& save, std::int64_t milliseconds) {
  milliseconds=std::max<std::int64_t>(0,milliseconds);
  setCampaignMinute(save,milliseconds/60000);
  save["gameSubMinuteMs"]=milliseconds%60000;
}
}  // namespace ember
