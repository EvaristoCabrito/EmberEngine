#include "world/Campaign.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <format>
#include <fstream>
#include <sstream>

#include "core/Log.h"

namespace ember::e2 {

namespace {

std::string str(const nlohmann::json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

bool readJson(const std::filesystem::path& file, nlohmann::json& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  out = nlohmann::json::parse(in, nullptr, false);
  return !out.is_discarded();
}

// JS truthiness of a JSON value as Engine2 reads it (`!!s.pendingMission`).
bool truthy(const nlohmann::json& j) {
  if (j.is_null()) return false;
  if (j.is_boolean()) return j.get<bool>();
  if (j.is_number()) return j.get<double>() != 0.0;
  if (j.is_string()) return !j.get<std::string>().empty();
  return true;  // objects and arrays
}

size_t sizeOf(const nlohmann::json& j, const char* key) {
  const auto it = j.find(key);
  return it != j.end() && (it->is_object() || it->is_array()) ? it->size() : 0;
}

}  // namespace

bool CampaignData::load(const std::filesystem::path& file) {
  nlohmann::json j;
  if (!readJson(file, j)) {
    logError("campaign data not found or unreadable: {}", file.string());
    return false;
  }
  saveVersion = j.value("saveVersion", 19);
  slotCount = j.value("slotCount", 6);
  emptySave = j.value("emptySave", nlohmann::json::object());
  for (const nlohmann::json& m : j.value("missions", nlohmann::json::array())) {
    CampaignMission c;
    c.id = str(m, "id"), c.title = str(m, "title"), c.place = str(m, "place"), c.briefing = str(m, "briefing");
    c.objective = str(m, "objective"), c.win = str(m, "win"), c.mapFile = str(m, "mapFile");
    c.index = m.value("index", 0);
    missions.push_back(std::move(c));
  }
  for (const nlohmann::json& l : j.value("locations", nlohmann::json::array())) {
    WorldLocation w;
    w.id = str(l, "id"), w.name = str(l, "name");
    w.col = l.value("col", 0), w.row = l.value("row", 0);
    for (const nlohmann::json& id : l.value("missionIds", nlohmann::json::array()))
      if (id.is_string()) w.missionIds.push_back(id.get<std::string>());
    locations.push_back(std::move(w));
  }
  slotProgressCases = j.value("slotProgress", nlohmann::json::array());
  return true;
}

const CampaignMission* CampaignData::mission(const std::string& id) const {
  for (const CampaignMission& m : missions)
    if (m.id == id) return &m;
  return nullptr;
}

// overworld.ts locationAt: a Map keyed by hex, so of two locations on one hex the later one wins.
const WorldLocation* CampaignData::locationAt(int col, int row) const {
  const WorldLocation* found = nullptr;
  for (const WorldLocation& l : locations)
    if (l.col == col && l.row == row) found = &l;
  return found;
}

std::filesystem::path Campaign::savePath() {
  const char* local = std::getenv("LOCALAPPDATA");
  const std::filesystem::path base = local && *local ? std::filesystem::path(local) : std::filesystem::current_path();
  return base / "Ember" / "ember-save-bank.json";
}

bool Campaign::open(const std::filesystem::path& data, const std::filesystem::path& bankFile) {
  bankFile_ = bankFile;
  if (!data_.load(data)) return false;
  // save.ts parseBank: slots that are not objects or hold nothing read as empty; lastSlot clamped and moved to an
  // occupied slot when it points at an empty one.
  bank_ = {};
  bank_.slots.assign(static_cast<size_t>(data_.slotCount), std::nullopt);
  nlohmann::json raw;
  if (readJson(bankFile_, raw) && raw.is_object()) {
    bank_.muted = raw.value("muted", false) == true;
    const nlohmann::json slots = raw.value("slots", nlohmann::json::array());
    for (size_t i = 0; i < bank_.slots.size() && i < slots.size(); ++i)
      if (slots[i].is_object() && occupied(slots[i])) bank_.slots[i] = slots[i];
    int last = raw.value("lastSlot", 0);
    last = std::clamp(last, 0, data_.slotCount - 1);
    if (!bank_.slots[static_cast<size_t>(last)]) {
      const auto first = std::find_if(bank_.slots.begin(), bank_.slots.end(), [](const auto& s) { return s.has_value(); });
      last = first == bank_.slots.end() ? 0 : static_cast<int>(first - bank_.slots.begin());
    }
    bank_.lastSlot = last;
    logInfo("campaign: save bank {} ({} of {} slots used)", bankFile_.string(),
            std::count_if(bank_.slots.begin(), bank_.slots.end(), [](const auto& s) { return s.has_value(); }), bank_.slots.size());
  }
  return true;
}

nlohmann::json Campaign::emptySave() const {
  nlohmann::json s = data_.emptySave;
  s["muted"] = bank_.muted;
  s["updatedAt"] = nowMs();
  return s;
}

// save.ts slotOccupied.
bool Campaign::occupied(const std::optional<nlohmann::json>& slot) {
  if (!slot || !slot->is_object()) return false;
  const nlohmann::json& s = *slot;
  const std::string mapMode = str(s, "mapMode");
  return sizeOf(s, "completed") > 0 || sizeOf(s, "unitHp") > 0 || (s.contains("pendingMission") && truthy(s["pendingMission"])) ||
         (s.contains("battle") && truthy(s["battle"])) || sizeOf(s, "companionConversations") > 0 || mapMode == "classic" ||
         mapMode == "rpg";
}

bool Campaign::hasAnySave() const {
  return std::any_of(bank_.slots.begin(), bank_.slots.end(), [](const auto& s) { return occupied(s); });
}

nlohmann::json Campaign::activeSave() const {
  const auto& slot = bank_.slots[static_cast<size_t>(bank_.lastSlot)];
  return slot ? *slot : emptySave();
}

// save.ts slotProgress.
SlotProgress Campaign::progress(const std::optional<nlohmann::json>& slot) const {
  if (!slot || !occupied(slot)) return {"Vazio", "Nenhuma campanha"};
  const nlohmann::json& s = *slot;
  auto titleOf = [&](const std::string& id) {
    const CampaignMission* m = data_.mission(id);
    return m ? m->title : id;
  };
  if (s.contains("battle") && truthy(s["battle"])) {
    const nlohmann::json& b = s["battle"];
    const std::string id = str(b, "missionId");
    const nlohmann::json turn = b.value("turn", nlohmann::json());
    std::string t = turn.is_number_integer() ? std::to_string(turn.get<long long>()) : turn.is_number() ? std::format("{}", turn.get<double>()) : "undefined";
    return {titleOf(id), "Em combate · turno " + t};
  }
  if (s.contains("pendingMission") && truthy(s["pendingMission"])) return {titleOf(str(s, "pendingMission")), "Início do combate"};
  if (str(s, "mapMode") == "rpg" && s.contains("overworldPos") && truthy(s["overworldPos"])) {
    const int col = s["overworldPos"].value("col", 0), row = s["overworldPos"].value("row", 0);
    const nlohmann::json clock = s.value("gameClock", nlohmann::json());
    const std::string day = clock.is_number_integer() ? std::to_string(clock.get<long long>()) : clock.is_number() ? std::format("{}", clock.get<double>()) : "0";
    if (const WorldLocation* here = data_.locationAt(col, row)) return {here->name, "Dia " + day + " · mapa"};
    // Nearest location by hex distance (odd-r offset to cube), the first of equals.
    auto toCube = [](int x, int y) {
      const int q = x - (y - (y & 1)) / 2;
      return std::array<int, 3>{q, y, -q - y};
    };
    const auto a = toCube(col, row);
    const WorldLocation* nearest = nullptr;
    int best = 0;
    for (const WorldLocation& l : data_.locations) {
      const auto b = toCube(l.col, l.row);
      const int d = (std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2])) / 2;
      if (!nearest || d < best) nearest = &l, best = d;
    }
    if (nearest) return {"Estrada perto de " + nearest->name, "Dia " + day + " · mapa"};
  }
  const nlohmann::json completed = s.value("completed", nlohmann::json::array());
  if (completed.empty()) return {"Campanha nova", "Mapa de cenários"};
  const std::string lastId = completed.back().is_string() ? completed.back().get<std::string>() : std::string();
  const CampaignMission* last = data_.mission(lastId);
  const CampaignMission* next = nullptr;
  if (last)
    for (const CampaignMission& m : data_.missions)
      if (m.index == last->index + 1) {
        next = &m;
        break;
      }
  if (next) return {next->title, "Após " + (last ? last->title : lastId)};
  return {last ? last->title : lastId, "Campanha concluída"};
}

bool Campaign::writeSlot(int index, nlohmann::json save) {
  const int i = std::clamp(index, 0, data_.slotCount - 1);
  save["version"] = data_.saveVersion;
  save["muted"] = bank_.muted;
  save["updatedAt"] = nowMs();
  bank_.slots[static_cast<size_t>(i)] = std::move(save);
  bank_.lastSlot = i;
  return persist();
}

bool Campaign::selectSlot(int index) {
  bank_.lastSlot = std::clamp(index, 0, data_.slotCount - 1);
  return persist();
}

bool Campaign::persist() {
  nlohmann::json out = {{"version", data_.saveVersion}, {"lastSlot", bank_.lastSlot}, {"muted", bank_.muted}};
  nlohmann::json slots = nlohmann::json::array();
  for (const auto& s : bank_.slots) slots.push_back(s ? *s : nlohmann::json());
  out["slots"] = std::move(slots);
  std::error_code error;
  std::filesystem::create_directories(bankFile_.parent_path(), error);
  const std::filesystem::path temp = bankFile_.string() + ".tmp";
  {
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file) {
      logError("campaign: could not write {}", temp.string());
      return false;
    }
    const std::string text = out.dump();
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();
    if (!file) {
      logError("campaign: writing {} failed", temp.string());
      return false;
    }
  }
  std::filesystem::rename(temp, bankFile_, error);
  if (error) {
    logError("campaign: could not replace {}: {}", bankFile_.string(), error.message());
    return false;
  }
  return true;
}

double Campaign::nowMs() {
  return static_cast<double>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// Intl.DateTimeFormat("pt-BR", { timeZone: "America/Sao_Paulo", dateStyle: "medium", timeStyle: "short" }):
// "15 de out. de 2026, 20:45". Brazil keeps UTC-3 all year (no daylight saving since 2019).
std::string Campaign::formatSaveDate(double updatedAtMs) {
  static const char* kMonths[12] = {"jan.", "fev.", "mar.", "abr.", "mai.", "jun.", "jul.", "ago.", "set.", "out.", "nov.", "dez."};
  const std::time_t t = static_cast<std::time_t>(std::floor(updatedAtMs / 1000.0)) - 3 * 3600;
  std::tm tm{};
  if (gmtime_s(&tm, &t) != 0) return {};
  return std::format("{} de {} de {}, {:02}:{:02}", tm.tm_mday, kMonths[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
}

int checkSlotProgress(const Campaign& campaign) {
  int differences = 0, cases = 0;
  for (const nlohmann::json& c : campaign.data().slotProgressCases) {
    ++cases;
    const SlotProgress p = campaign.progress(c["save"]);
    const std::string title = c.value("title", ""), detail = c.value("detail", "");
    if (p.title != title || p.detail != detail) {
      ++differences;
      logError("slotProgress differs: Engine2 '{}' / '{}', C++ '{}' / '{}'", title, detail, p.title, p.detail);
    }
  }
  if (!differences) logInfo("campaign: slotProgress matches Engine2 ({} saves)", cases);
  return differences;
}

}  // namespace ember::e2
