#include "world/Locais.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "core/Log.h"

namespace ember {

namespace {

Json readFile(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return Json();
  Json j = Json::parse(in, nullptr, false);
  return j.is_discarded() ? Json() : j;
}

bool writeFile(const std::filesystem::path& file, const Json& j, int indent = 2) {
  std::error_code ec;
  std::filesystem::create_directories(file.parent_path(), ec);
  const std::filesystem::path tmp = file.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << j.dump(indent) << "\n";
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
  // the one place list: each place's maps, floors and slots, in campaign order
  const Json places = readFile(file());
  for (const Json& loc : places.is_object() ? places.value("locations", Json::array()) : Json::array()) {
    if (!loc.is_object() || !loc.contains("id")) continue;
    const std::string id = loc.value("id", "");
    l.base.push_back({id, loc.value("name", id), strings(loc.value("missionIds", Json::array()))});
    l.order[id] = l.base.back().missionIds, l.locationOrder.push_back(id);
    if (loc.contains("slots") && loc["slots"].is_number()) l.slots[id] = loc["slots"].get<int>();
    std::vector<LocaisFloor>& floors = l.defaultSubmaps[id];
    for (const Json& f : loc.value("submaps", Json::array()))
      if (f.is_object() && f.contains("missionId")) floors.push_back({f["missionId"].get<std::string>(), f.value("floor", 0)});
  }
  l.submaps = l.defaultSubmaps;
  l.own = !l.base.empty();
  if (!l.own) logError("Locais: no places in {}", file().generic_string());
  l.regions = regionsOf(e.value("encounterRegions", Json::array()));
  const Json regions = readFile(regionsFile());
  if (regions.is_object() && regions.contains("regions")) l.regions = regionsOf(regions), l.ownRegions = true;
  return l;
}

bool Locais::save() {
  Json places = readFile(file());
  if (!places.is_object()) places = Json::object();
  const Json old = places.value("locations", Json::array());
  // the places in the editor's order, then any it does not name
  std::vector<std::string> ids = locationOrder;
  for (const Base& b : base)
    if (std::find(ids.begin(), ids.end(), b.id) == ids.end()) ids.push_back(b.id);
  std::set<std::string> floorIds;
  for (const auto& [loc, list] : submaps)
    for (const LocaisFloor& f : list) floorIds.insert(f.missionId);
  Json next = Json::array();
  std::vector<Base> saved;
  for (const std::string& id : ids) {
    Json p = Json{{"id", id}};
    for (const Json& o : old)
      if (o.is_object() && o.value("id", "") == id) p = o;
    if (!p.contains("name"))
      for (const Base& b : base)
        if (b.id == id) p["name"] = b.name;
    std::vector<std::string> maps;
    if (const auto it = order.find(id); it != order.end())
      for (const std::string& m : it->second)
        if (!floorIds.contains(m) && std::find(maps.begin(), maps.end(), m) == maps.end()) maps.push_back(m);
    p["missionIds"] = maps;
    Json floors = Json::array();
    if (const auto it = submaps.find(id); it != submaps.end())
      for (const LocaisFloor& f : it->second) floors.push_back(Json{{"missionId", f.missionId}, {"floor", f.floor}});
    if (floors.empty()) p.erase("submaps");
    else p["submaps"] = floors;
    if (const int n = slotsFor(id); n > 0) p["slots"] = n;
    else p.erase("slots");
    saved.push_back({id, p.value("name", id), maps});
    next.push_back(std::move(p));
  }
  places["locations"] = next;
  if (!writeFile(file(), places, 1)) return false;
  base = std::move(saved), defaultSubmaps = submaps, knownMissionIds.reset(), own = true;
  for (const Base& b : base) order[b.id] = b.missionIds;
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
    if (w.id.empty()) continue;  // no hex on the map (the editor's only)
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

}  // namespace ember
