#include "world/Interlude.h"

#include <algorithm>
#include <format>
#include <fstream>

#include "core/Log.h"
#include "world/Overworld.h"

namespace ember::e2 {

namespace {

bool hasString(const Json& list, const std::string& v) {
  if (!list.is_array()) return false;
  return std::any_of(list.begin(), list.end(), [&](const Json& x) { return x.is_string() && x.get<std::string>() == v; });
}
void addFlag(Json& save, const std::string& flag) {
  Json& flags = save["flags"];
  if (!flags.is_array()) flags = Json::array();
  if (!hasString(flags, flag)) flags.push_back(flag);
}
void removeFlag(Json& save, const std::string& flag) {
  if (!save.contains("flags") || !save["flags"].is_array()) return;
  Json& flags = save["flags"];
  for (auto it = flags.begin(); it != flags.end();) {
    if (it->is_string() && it->get<std::string>() == flag) it = flags.erase(it);
    else ++it;
  }
}
const Json* child(const Json& j, const char* key) {
  if (!j.is_object()) return nullptr;
  const auto it = j.find(key);
  return it == j.end() ? nullptr : &*it;
}

}  // namespace

// --- the data ----------------------------------------------------------------------------------------------------------

bool InterludeData::load(const std::string& file, std::string* why) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    if (why) *why = "no such file: " + file;
    return false;
  }
  const Json j = Json::parse(in, nullptr, false);
  if (j.is_discarded()) {
    if (why) *why = "not valid JSON: " + file;
    return false;
  }
  return parse(j, why);
}

bool InterludeData::parse(const Json& j, std::string* why) {
  auto refuse = [&](const std::string& w) {
    if (why) *why = w;
    return false;
  };
  if (!j.is_object()) return refuse("not an object");
  if (j.value("format", "") != kFormat) return refuse(std::format("format \"{}\" is not \"{}\"", j.value("format", ""), kFormat));
  ruinsLocation = j.value("ruinsLocation", "ruins");
  requiredQuests.clear();
  if (const Json* q = child(j, "requiredQuests"); q && q->is_array())
    for (const Json& id : *q)
      if (id.is_string() && !id.get<std::string>().empty()) requiredQuests.push_back(id.get<std::string>());
  party.clear();
  const Json* p = child(j, "party");
  if (!p || !p->is_array()) return refuse("no \"party\" array");
  for (const Json& h : *p) {
    if (!h.is_object()) return refuse("a party entry is not an object");
    InterludeHero hero;
    hero.name = h.value("name", ""), hero.classId = h.value("classId", ""), hero.weapon = h.value("weapon", "");
    hero.level = h.value("level", 1);
    hero.persistent = h.value("persistent", false), hero.temporary = h.value("temporary", false);
    if (hero.name.empty() || hero.classId.empty()) return refuse("a party entry has no name or classId");
    if (hero.level < 1) return refuse(hero.name + ": level under 1");
    if (hero.persistent && hero.temporary) return refuse(hero.name + ": both persistent and temporary");
    if (this->hero(hero.name)) return refuse(hero.name + ": listed twice");
    party.push_back(std::move(hero));
  }
  if (party.size() != 3) return refuse(std::format("the party has {} members, the Interlude has exactly three", party.size()));
  leader = j.value("leader", party.front().name);
  if (!hero(leader)) return refuse("leader \"" + leader + "\" is not in the party");
  missions.clear();
  if (const Json* m = child(j, "missions"); m && m->is_array())
    for (const Json& id : *m)
      if (id.is_string() && !id.get<std::string>().empty()) missions.push_back(id.get<std::string>());
  if (missions.empty()) return refuse("no \"missions\"");
  persistentHero.clear();
  for (const InterludeHero& h : party)
    if (h.persistent) {
      if (!persistentHero.empty()) return refuse("two persistent heroes; the Interlude keeps one (Isolde)");
      persistentHero = h.name;
    }
  if (persistentHero.empty()) return refuse("no persistent hero (Isolde)");
  return true;
}

bool InterludeData::hasMission(const std::string& id) const { return std::find(missions.begin(), missions.end(), id) != missions.end(); }

const InterludeHero* InterludeData::hero(const std::string& name) const {
  for (const InterludeHero& h : party)
    if (h.name == name) return &h;
  return nullptr;
}

// --- the state ----------------------------------------------------------------------------------------------------------

InterludeState interludeState(const Json& save) {
  const Json* i = child(save, "interlude");
  if (!i || !i->is_object()) return InterludeState::NotStarted;
  const std::string s = i->value("state", "");
  if (s == "active") return InterludeState::Active;
  if (s == "completed") return InterludeState::Completed;
  return InterludeState::NotStarted;
}

const char* interludeStateName(InterludeState s) {
  switch (s) {
    case InterludeState::NotStarted: return "NOT_STARTED";
    case InterludeState::Active: return "ACTIVE";
    case InterludeState::Completed: return "COMPLETED";
  }
  return "?";
}

std::string ruinsLockedReason(const InterludeData& data, const Overworld& world, const Json& save) {
  if (hasString(save.value("flags", Json::array()), "interlude:ruins-open")) return {};
  if (data.requiredQuests.empty()) return "unconfigured: no required quest listed (content/interlude.json requiredQuests)";
  std::vector<std::string> missing;
  for (const std::string& q : data.requiredQuests)
    if (!world.conditionHolds(Json{{"questCompleted", q}}, save)) missing.push_back(q);
  if (missing.empty()) return {};
  std::string list;
  for (const std::string& q : missing) list += (list.empty() ? "" : ", ") + q;
  return "quests not done: " + list;
}

bool ruinsOpen(const InterludeData& data, const Overworld& world, const Json& save, bool test) {
  if (test) return true;
  return ruinsLockedReason(data, world, save).empty();
}

std::string interludeMission(const InterludeData& data, const Json& save) {
  if (interludeState(save) != InterludeState::Active) return {};
  const int at = save["interlude"].value("mission", 0);
  if (at < 0 || at >= static_cast<int>(data.missions.size())) return {};
  return data.missions[static_cast<size_t>(at)];
}

const std::vector<std::string>& interludePartyKeys() {
  static const std::vector<std::string> keys{
      "levels",     "xp",         "statPointAllocations", "bags",       "promotions",     "weapons",      "equipped",      "equipment",
      "looseEquipment", "spellUses", "ember",              "unitHp",     "heroHunger",     "heroDiseases", "heroPoisons",   "heroSkills",
      "partyLeader", "partyFormation", "hotbars",         "affinityScores", "runes",      "unknownWeapons", "unknownEquipment", "unknownPotions",
      "rations",    "hungerStreak", "travelTrainingHours", "pendingMission", "battle"};
  return keys;
}

bool interludeBegin(const InterludeData& data, Json& save, std::string* why) {
  if (interludeState(save) != InterludeState::NotStarted) {
    if (why) *why = std::string("the Interlude is ") + interludeStateName(interludeState(save));
    return false;
  }
  Json next = save;
  // The present party's fields, put aside whole (the keys absent stay absent on the way back).
  Json present = Json::object();
  for (const std::string& k : interludePartyKeys())
    if (next.contains(k)) present[k] = next[k];
  for (const std::string& k : interludePartyKeys()) next.erase(k);
  // The historical party: levels as the data says, XP 0, a fresh starter bag each, the class's weapon in hand.
  Json levels = Json::object(), xp = Json::object(), bags = Json::object(), equipped = Json::object(), weapons = Json::object();
  for (const InterludeHero& h : data.party) {
    levels[h.name] = h.level;
    xp[h.name] = 0;
    bags[h.name] = Json{{"mid", 2}, {"weak", 2}, {"potent", 1}, {"disease", 1}, {"manaSmall", 1}, {"manaMid", 0}, {"manaLarge", 0}, {"lockpick", 3}};
    if (!h.weapon.empty()) {
      equipped[h.name] = h.weapon;
      weapons[h.weapon] = 0;
    }
    addFlag(next, "recruited:" + h.name);
  }
  next["levels"] = levels, next["xp"] = xp, next["statPointAllocations"] = Json::object(), next["bags"] = bags;
  next["promotions"] = Json::object(), next["weapons"] = weapons, next["equipped"] = equipped, next["equipment"] = Json::object();
  next["looseEquipment"] = Json::object(), next["spellUses"] = Json::object(), next["ember"] = 0, next["unitHp"] = Json::object();
  next["heroHunger"] = Json::object(), next["heroDiseases"] = Json::object(), next["heroPoisons"] = Json::object(), next["heroSkills"] = Json::object();
  next["partyLeader"] = data.leader, next["hotbars"] = Json::object(), next["affinityScores"] = Json::object();
  next["rations"] = 20, next["hungerStreak"] = 0;
  next["pendingMission"] = data.missions.front();
  next["battle"] = nullptr;
  next["interlude"] = Json{{"state", "active"}, {"mission", 0}, {"present", std::move(present)}};
  save = std::move(next);
  return true;
}

bool interludeComplete(const InterludeData& data, Json& save, std::string* why) {
  if (interludeState(save) != InterludeState::Active) {
    if (why) *why = std::string("the Interlude is ") + interludeStateName(interludeState(save));
    return false;
  }
  Json next = save;
  Json& inter = next["interlude"];
  // Isolde's progression as the chapter leaves it.
  const std::string& iso = data.persistentHero;
  Json isolde = Json::object();
  isolde["level"] = next.value("levels", Json::object()).value(iso, 1);
  isolde["xp"] = next.value("xp", Json::object()).value(iso, 0);
  isolde["heroSkills"] = next.value("heroSkills", Json::object()).value(iso, Json::object());
  isolde["statPointAllocations"] = next.value("statPointAllocations", Json::object()).value(iso, Json::object());
  isolde["promotion"] = next.value("promotions", Json::object()).value(iso, Json());
  // The present party back, whole: every moved key as it was, the ones it did not have gone.
  const Json present = inter.value("present", Json::object());
  for (const std::string& k : interludePartyKeys()) {
    if (present.contains(k)) next[k] = present[k];
    else next.erase(k);
  }
  for (const InterludeHero& h : data.party) removeFlag(next, "recruited:" + h.name);
  next["pendingMission"] = nullptr;
  next["battle"] = nullptr;
  next["interlude"] = Json{{"state", "completed"}, {"isolde", std::move(isolde)}, {"isoldeRecruited", false}};
  save = std::move(next);
  return true;
}

bool interludeAdvance(const InterludeData& data, Json& save, const std::string& missionDone) {
  if (interludeState(save) != InterludeState::Active) return false;
  const std::string current = interludeMission(data, save);
  if (current.empty() || current != missionDone) return false;
  const int at = save["interlude"].value("mission", 0) + 1;
  if (at >= static_cast<int>(data.missions.size())) return interludeComplete(data, save);
  save["interlude"]["mission"] = at;
  save["pendingMission"] = data.missions[static_cast<size_t>(at)];
  save["battle"] = nullptr;
  return true;
}

bool recruitIsolde(const InterludeData& data, Json& save, std::string* why) {
  if (interludeState(save) != InterludeState::Completed) {
    if (why) *why = "the Interlude is not completed: no progression kept";
    return false;
  }
  Json& inter = save["interlude"];
  if (inter.value("isoldeRecruited", false)) {
    if (why) *why = "already recruited";
    return false;
  }
  const Json kept = inter.value("isolde", Json::object());
  const std::string& iso = data.persistentHero;
  save["levels"][iso] = kept.value("level", 1);
  save["xp"][iso] = kept.value("xp", 0);
  if (const Json s = kept.value("heroSkills", Json::object()); s.is_object() && !s.empty()) save["heroSkills"][iso] = s;
  if (const Json p = kept.value("statPointAllocations", Json::object()); p.is_object() && !p.empty()) save["statPointAllocations"][iso] = p;
  if (const Json p = kept.value("promotion", Json()); p.is_string()) save["promotions"][iso] = p;
  addFlag(save, "recruited:" + iso);
  inter["isoldeRecruited"] = true;
  return true;
}

// --- the check -----------------------------------------------------------------------------------------------------------

int checkInterlude(const InterludeData& data, const Overworld& world, const Json& emptySave) {
  int fails = 0;
  auto check = [&](bool ok, const std::string& what) {
    if (ok) logInfo("check-interlude: {}: ok", what);
    else logError("check-interlude: {}: FAIL", what), ++fails;
  };
  const Json fresh = emptySave;
  // 1. The gate: an old save (no interlude object) is NOT_STARTED; unconfigured quests keep the Ruins shut; Modo teste opens them.
  check(interludeState(fresh) == InterludeState::NotStarted, "a save without the Interlude is NOT_STARTED");
  InterludeData unconfigured = data;
  unconfigured.requiredQuests.clear();
  check(!ruinsOpen(unconfigured, world, fresh), "an unconfigured quest list keeps the Ruins shut");
  check(ruinsOpen(unconfigured, world, fresh, true), "Modo teste opens the Ruins");
  InterludeData gated = data;
  gated.requiredQuests = {"suspeito-watchtower-captive"};  // a quest the tables know
  check(!ruinsOpen(gated, world, fresh), "the Ruins shut while a required quest is not done");
  Json done = fresh;
  done["questsDone"] = Json::array({"suspeito-watchtower-captive"});
  check(ruinsOpen(gated, world, done), "the Ruins open once every required quest is done");
  Json flagged = fresh;
  flagged["flags"] = Json::array({"interlude:ruins-open"});
  check(ruinsOpen(unconfigured, world, flagged), "the developer flag opens the Ruins");
  // 2. Begin: the present party put aside, the historical one in.
  Json save = fresh;
  save["levels"]["Kael"] = 7, save["xp"]["Kael"] = 310, save["ember"] = 125, save["weapons"]["espada-larga"] = 2;
  save["partyLeader"] = "Neera", save["heroSkills"]["Kael"]["dodging"] = 12.5;
  save["completed"] = Json::array({"vau", "bosque"});
  const Json before = save;
  std::string why;
  check(interludeBegin(data, save, &why), "begin on a fresh campaign" + (why.empty() ? "" : " (" + why + ")"));
  check(interludeState(save) == InterludeState::Active, "state ACTIVE after begin");
  check(!interludeBegin(data, save, &why), "a second begin is refused");
  check(save.value("pendingMission", Json()) == Json(data.missions.front()), "the first mission is pending");
  check(interludeMission(data, save) == data.missions.front(), "interludeMission names the first mission");
  bool onlyHistorical = save["levels"].is_object() && save["levels"].size() == data.party.size();
  for (const InterludeHero& h : data.party) onlyHistorical = onlyHistorical && save["levels"].contains(h.name) && save["levels"][h.name] == h.level;
  check(onlyHistorical, "levels hold exactly the three historical heroes at their starting levels");
  check(!save["levels"].contains("Kael") && save.value("ember", -1) == 0, "no present hero or purse in the historical party");
  check(save["completed"] == before["completed"], "campaign-wide fields (completed) untouched by begin");
  bool flags = true;
  for (const InterludeHero& h : data.party) flags = flags && hasString(save["flags"], "recruited:" + h.name);
  check(flags, "the historical heroes count as recruited while active");
  // 3. A fight: Isolde gains levels and a skill, young Salazar too; the present's Salazar is not touched.
  save["levels"][data.persistentHero] = 4, save["xp"][data.persistentHero] = 120;
  save["heroSkills"][data.persistentHero]["dodging"] = 7.0;
  save["statPointAllocations"][data.persistentHero] = Json{{"mag", 2}};
  save["levels"]["Salazar Jovem"] = 6, save["ember"] = 40;
  // The first mission done: with one mission the chapter completes; with more, the next is pending.
  if (data.missions.size() > 1) {
    check(interludeAdvance(data, save, data.missions[0]), "the first mission done: the next pending");
    check(save.value("pendingMission", Json()) == Json(data.missions[1]), "pendingMission is the second mission");
    check(!interludeAdvance(data, save, data.missions[0]), "the same mission done again changes nothing");
    for (size_t i = 1; i < data.missions.size(); ++i) interludeAdvance(data, save, data.missions[i]);
  } else {
    check(!interludeAdvance(data, save, "not-an-interlude-mission"), "a stray mission id changes nothing");
    check(interludeAdvance(data, save, data.missions[0]), "the only mission done completes the chapter");
  }
  // 4. Complete: the present party back byte for byte, Isolde kept, Salazar untouched, Selene gone.
  check(interludeState(save) == InterludeState::Completed, "state COMPLETED");
  bool restored = true;
  for (const std::string& k : interludePartyKeys()) {
    const bool had = before.contains(k), has = save.contains(k);
    if (had != has || (had && before[k] != save[k])) restored = false, logError("check-interlude: field {} differs after the return", k);
  }
  check(restored, "every party field restored as before the chapter");
  check(save["levels"].value("Salazar", 0) == before["levels"].value("Salazar", 0), "present Salazar's level untouched by young Salazar's");
  check(!save["levels"].contains("Selene") && !save["levels"].contains("Salazar Jovem"), "Selene and young Salazar are not in the present party");
  check(!save["levels"].contains(data.persistentHero), "Isolde is not in the present party before her recruitment");
  check(save["interlude"]["isolde"].value("level", 0) == 4 && save["interlude"]["isolde"].value("xp", 0) == 120, "Isolde's level 4 / 120 XP kept");
  check(!hasString(save["flags"], "recruited:Selene") && !hasString(save["flags"], "recruited:" + data.persistentHero), "recruitment flags of the historical party removed");
  check(save["completed"] == before["completed"], "completed untouched by the chapter's bookkeeping");
  check(!interludeBegin(data, save, &why), "the chapter never begins again once completed");
  // 5. Recruitment, once.
  why.clear();
  check(recruitIsolde(data, save, &why), "Isolde recruited later" + (why.empty() ? "" : " (" + why + ")"));
  check(save["levels"].value(data.persistentHero, 0) == 4 && save["xp"].value(data.persistentHero, 0) == 120, "recruited at her Interlude level and XP");
  check(save["heroSkills"][data.persistentHero].value("dodging", 0.0) == 7.0, "her trained skill came along");
  check(save["statPointAllocations"][data.persistentHero].value("mag", 0) == 2, "her attribute points came along");
  check(hasString(save["flags"], "recruited:" + data.persistentHero), "her recruitment flag set");
  check(!recruitIsolde(data, save, &why), "a second recruitment is refused");
  check(save["levels"].value("Kael", 0) == 7 && save.value("ember", 0) == 125, "the present party's own numbers still there");
  // 6. Resumption: a save left mid-chapter names its mission.
  Json mid = before;
  interludeBegin(data, mid);
  check(interludeMission(data, mid) == data.missions.front() && mid.value("pendingMission", Json()) == Json(data.missions.front()), "a save left ACTIVE resumes at its pending mission");
  logInfo("check-interlude: {}", fails == 0 ? "PASS" : std::format("{} FAIL", fails));
  return fails;
}

}  // namespace ember::e2
