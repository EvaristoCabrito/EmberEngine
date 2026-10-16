#include "world/Locais.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "core/Log.h"

namespace ember::e2 {

namespace {

Json readFile(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return Json();
  Json j = Json::parse(in, nullptr, false);
  return j.is_discarded() ? Json() : j;
}

bool writeFile(const std::filesystem::path& file, const Json& j) {
  std::error_code ec;
  std::filesystem::create_directories(file.parent_path(), ec);
  const std::filesystem::path tmp = file.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << j.dump(2) << "\n";
    if (!out) return false;
  }
  std::filesystem::rename(tmp, file, ec);
  return !ec;
}

std::vector<std::string> strings(const Json& j) {
  std::vector<std::string> out;
  if (j.is_array())
    for (const Json& s : j)
      if (s.is_string()) out.push_back(s.get<std::string>());
  return out;
}

std::map<std::string, std::vector<LocaisFloor>> floorsOf(const Json& j) {
  std::map<std::string, std::vector<LocaisFloor>> out;
  if (!j.is_object()) return out;
  for (const auto& [loc, list] : j.items()) {
    std::vector<LocaisFloor>& v = out[loc];
    if (list.is_array())
      for (const Json& f : list)
        if (f.is_object() && f.contains("missionId")) v.push_back({f["missionId"].get<std::string>(), f.value("floor", 0)});
  }
  return out;
}

Json floorsJson(const std::map<std::string, std::vector<LocaisFloor>>& m) {
  Json out = Json::object();
  for (const auto& [loc, list] : m) {
    Json a = Json::array();
    for (const LocaisFloor& f : list) a.push_back(Json{{"missionId", f.missionId}, {"floor", f.floor}});
    out[loc] = a;
  }
  return out;
}

std::vector<EncounterRegion> regionsOf(const Json& j) {
  std::vector<EncounterRegion> out;
  const Json& rows = j.is_object() && j.contains("regions") ? j["regions"] : j;
  if (!rows.is_array()) return out;
  for (const Json& r : rows)
    if (r.is_object() && r.contains("id")) out.push_back({r["id"].get<std::string>(), r.value("name", r["id"].get<std::string>()), strings(r.value("encounterIds", Json::array()))});
  return out;
}

}  // namespace

Locais Locais::load(const std::filesystem::path& catalogFile) {
  Locais l;
  const Json cat = readFile(catalogFile);
  const Json e = cat.is_object() && cat.contains("editor") ? cat["editor"] : Json::object();
  for (const Json& loc : e.value("locations", Json::array())) l.base.push_back({loc.value("id", ""), loc.value("name", ""), strings(loc.value("missionIds", Json::array()))});
  const Json shipped = e.value("locais", Json::object());
  l.defaultSubmaps = floorsOf(shipped.value("submaps", Json::object()));
  // the shipped state (no saved copy: ALL_LOCATIONS, LOCATION_SLOTS, DEFAULT_LOCATION_SUBMAPS)
  for (const Base& b : l.base) l.order[b.id] = b.missionIds, l.locationOrder.push_back(b.id);
  const Json slotsTable = shipped.value("slots", Json::object());  // a copy: items() of a temporary dangles
  for (const auto& [loc, n] : slotsTable.items())
    if (n.is_number()) l.slots[loc] = n.get<int>();
  l.submaps = l.defaultSubmaps;
  l.regions = regionsOf(e.value("encounterRegions", Json::array()));
  // EmberEngine's own saved copy (loadLocaisLocal: order, slots and locationOrder required)
  const Json own = readFile(file());
  if (own.is_object() && own.contains("order") && own["order"].is_object() && own.contains("slots") && own["slots"].is_object() &&
      own.contains("locationOrder") && own["locationOrder"].is_array()) {
    l.own = true;
    l.order.clear(), l.slots.clear();
    for (const auto& [loc, ids] : own["order"].items()) l.order[loc] = strings(ids);
    for (const auto& [loc, n] : own["slots"].items())
      if (n.is_number()) l.slots[loc] = n.get<int>();
    l.locationOrder = strings(own["locationOrder"]);
    std::map<std::string, std::vector<LocaisFloor>> groups = l.defaultSubmaps;
    for (auto& [loc, list] : floorsOf(own.value("submaps", Json::object()))) groups[loc] = list;
    l.submaps = groups;
    if (own.contains("knownMissionIds") && own["knownMissionIds"].is_array()) l.knownMissionIds = strings(own["knownMissionIds"]);
  }
  const Json regions = readFile(regionsFile());
  if (regions.is_object() && regions.contains("regions")) l.regions = regionsOf(regions), l.ownRegions = true;
  return l;
}

bool Locais::save() {
  Json j = Json::object();
  Json o = Json::object();
  for (const auto& [loc, ids] : order) o[loc] = ids;
  j["order"] = o;
  Json s = Json::object();
  for (const auto& [loc, n] : slots) s[loc] = n;
  j["slots"] = s;
  j["locationOrder"] = locationOrder;
  j["submaps"] = floorsJson(submaps);
  j["floorLayoutVersion"] = 1;
  std::vector<std::string> known;
  for (const Base& b : base) known.insert(known.end(), b.missionIds.begin(), b.missionIds.end());
  j["knownMissionIds"] = known;
  if (!writeFile(file(), j)) return false;
  own = true, knownMissionIds = known;
  return true;
}

bool Locais::saveRegions() {
  Json rows = Json::array();
  for (const EncounterRegion& r : regions) rows.push_back(Json{{"id", r.id}, {"name", r.name}, {"encounterIds", r.encounterIds}});
  if (!writeFile(regionsFile(), Json{{"regions", rows}})) return false;
  ownRegions = true;
  return true;
}

std::vector<Locais::Base> Locais::locations() const {
  std::map<std::string, std::vector<LocaisFloor>> groups = defaultSubmaps;
  for (const auto& [loc, list] : submaps) groups[loc] = list;
  std::set<std::string> floorIds, assigned;
  for (const auto& [loc, list] : groups)
    for (const LocaisFloor& f : list) floorIds.insert(f.missionId);
  for (const auto& [loc, ids] : order) assigned.insert(ids.begin(), ids.end());
  assigned.insert(floorIds.begin(), floorIds.end());
  std::set<std::string> known;
  if (knownMissionIds) known.insert(knownMissionIds->begin(), knownMissionIds->end());
  std::vector<Base> locs;
  for (const Base& b : base) {
    const auto it = order.find(b.id);
    const std::vector<std::string> chosen = it == order.end() ? std::vector<std::string>() : it->second;
    Base out{b.id, b.name, {}};
    std::vector<std::string> all = chosen;
    for (const std::string& id : b.missionIds)
      if (!assigned.contains(id) && std::find(chosen.begin(), chosen.end(), id) == chosen.end() && !(knownMissionIds && known.contains(id))) all.push_back(id);
    for (const std::string& id : all)
      if (!floorIds.contains(id)) out.missionIds.push_back(id);
    locs.push_back(std::move(out));
  }
  // inLocationOrder: the wanted ids first, then the rest as they were
  std::vector<Base> ordered;
  std::set<std::string> placed;
  for (const std::string& id : locationOrder)
    for (const Base& b : locs)
      if (b.id == id && !placed.contains(id)) ordered.push_back(b), placed.insert(id);
  for (const Base& b : locs)
    if (!placed.contains(b.id)) ordered.push_back(b);
  return ordered;
}

std::map<std::string, std::vector<std::string>> Locais::editorOrder() const {
  if (!own) return order;
  std::map<std::string, std::vector<std::string>> out;
  for (const Base& b : locations()) out[b.id] = b.missionIds;
  return out;
}

int Locais::slotsFor(const std::string& location) const {
  const auto it = slots.find(location);
  return it == slots.end() || it->second <= 0 ? 0 : it->second;
}

void applyLocais(CampaignData& data, const Locais& locais, const std::filesystem::path& savedMaps) {
  data.locations = data.shippedLocations;
  data.missions = data.shippedMissions;
  if (!locais.own) return;
  std::vector<WorldLocation> next;
  for (const Locais::Base& b : locais.locations()) {
    WorldLocation w;
    for (const WorldLocation& s : data.shippedLocations)
      if (s.id == b.id) w = s;
    if (w.id.empty()) w.id = b.id, w.name = b.name;
    w.missionIds = b.missionIds;
    next.push_back(std::move(w));
  }
  data.locations = std::move(next);
  // missions only the editor knows: their newest saved file
  for (const WorldLocation& w : data.locations)
    for (const std::string& id : w.missionIds) {
      if (data.mission(id)) continue;
      int best = -1;
      std::filesystem::path file;
      std::error_code ec;
      if (std::filesystem::is_directory(savedMaps, ec))
        for (const auto& e : std::filesystem::directory_iterator(savedMaps, ec)) {
          const std::string name = e.path().filename().string();
          if (name.size() != id.size() + 8 || !name.starts_with(id) || !name.ends_with(".json")) continue;
          const std::string digits = name.substr(id.size(), 3);
          if (!std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) continue;
          if (std::stoi(digits) > best) best = std::stoi(digits), file = e.path();
        }
      if (best < 0) continue;
      const Json j = readFile(file);
      const Json d = j.is_object() && j.contains("draft") ? j["draft"] : j;
      if (!d.is_object()) continue;
      CampaignMission m;
      m.id = id, m.title = d.value("title", id), m.place = d.value("place", ""), m.briefing = d.value("briefing", "");
      m.objective = d.value("objective", ""), m.win = d.value("win", "rout"), m.index = d.value("index", 0), m.own = true;
      data.missions.push_back(std::move(m));
    }
}

}  // namespace ember::e2
