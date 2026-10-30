#include "world/Overworld.h"
#include "world/Locais.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>
#include <cstdio>
#include <set>

#include "core/Log.h"
#include "world/Battle.h"
#include "world/Combat.h"
#include "world/Progression.h"

namespace ember {

namespace {

double gHexSize = 5.0;  // the loaded board's "hexSize" (hexToWorld is static)
constexpr double kRecoveryPct = 0.08;
constexpr double kBattleEncounterChance = 0.10;  // 0.15 before 2026-10-29 (the user: "reduzir batalha normal para 10%")
constexpr double kAlertBattleChance = 0.30;      // on alert days (the user: "em alerta continua com 30%")
constexpr double kTextEncounterChance = 0.25;
constexpr double kTravelDiseaseChance = 0.02;
constexpr int kHungerGraceDays = 3;
constexpr double kHungerPenaltyPerDay = 0.1;
constexpr double kHungerPenaltyMax = 0.9;
const char* const kHeroes[] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};  // HERO_BASE_CLASS order

// The road texts: the eight of the old rule source (any terrain) and, since 2026-10-29 (the user: "criar 5 novos textos com
// consequências reais para cada tipo de terreno"), five per region of the board (road / mountain / plains / forest), picked
// only on a hex of that region. Consequences: rations lost (1..rationsDice) or gained (rationsGain), ember gained / gold lost
// (1..goldLossDice), a loot bag, a potion or lockpick lost, a disease roll, alert days (battles twice as likely).
struct RoadEncounter {
  const char* text;
  int rationsDice = 0, ember = 0, goldLossDice = 0;
  bool lootBag = false, losePotion = false, loseLockpick = false;
  double diseaseChance = 0;
  int alertDays = 0;
  const char* region = nullptr;  // null = any hex
  int rationsGain = 0;
};
const RoadEncounter kEncounters[] = {
    {.text = "Um bando de corvos assusta a coluna e parte das rações se perde na correria.", .rationsDice = 8},
    {.text = "Vestígios de um acampamento abandonado — e uma bolsa esquecida.", .lootBag = true},
    {.text = "Chuva forte atrasa a marcha, mas ninguém se machuca.", .diseaseChance = 0.05},
    {.text = "Pegadas grandes demais cruzam o caminho. O grupo segue mais alerta.", .alertDays = 3},
    {.text = "Uma carroça atolada cede de vez; comida e mantimentos caem no barro.", .rationsDice = 4},
    {.text = "Um frasco se solta durante a descida e se quebra nas pedras.", .losePotion = true},
    {.text = "Ladrões passam pelo acampamento durante a noite e levam uma gazua.", .loseLockpick = true},
    {.text = "A ponte podre arrebenta sob a carga: parte do ouro e das rações vai para o rio.", .rationsDice = 3, .goldLossDice = 8},
    // estrada
    {.text = "Jogo de dados numa taverna de beira de estrada: alguém do grupo aposta e perde.", .goldLossDice = 10, .region = "road"},
    {.text = "Um mercador ambulante com a roda quebrada; o grupo ajuda e ele paga em moedas.", .ember = 12, .region = "road"},
    {.text = "Carroça virada na beira da estrada: entre os destroços, um saco de rações intacto.", .region = "road", .rationsGain = 4},
    {.text = "Poeira e sol a pino o dia inteiro: a marcha cansa e as rações vão embora mais depressa.", .rationsDice = 3, .region = "road"},
    {.text = "Um batedor de passagem avisa: salteadores à frente, na estrada.", .alertDays = 2, .region = "road"},
    // montanha
    {.text = "Uma avalanche de pedras fecha o desfiladeiro; o desvio pelos cumes custa provisões.", .rationsDice = 5, .region = "mountain"},
    {.text = "Frio cortante nos cumes: alguém passa a noite tremendo.", .diseaseChance = 0.5, .region = "mountain"},
    {.text = "Numa fenda da rocha, os ossos de um viajante e a bolsa que ele carregava.", .lootBag = true, .region = "mountain"},
    {.text = "O gelo cede sob um dos mulos: um frasco vai parar no fundo da ravina.", .losePotion = true, .region = "mountain"},
    {.text = "Vultos nas cristas acompanham a coluna; o grupo dorme em vigília.", .alertDays = 3, .region = "mountain"},
    // planície
    {.text = "Um lavrador oferece pão e queijo em troca de um dia de ajuda na colheita.", .region = "plains", .rationsGain = 4},
    {.text = "Uma nuvem de gafanhotos passa pelo acampamento e devora parte das rações.", .rationsDice = 6, .region = "plains"},
    {.text = "Um cavalo solto arrasta uma sacola pelo capim: parte do ouro se perde.", .goldLossDice = 6, .region = "plains"},
    {.text = "Um marco de pedra tombado; debaixo dele, moedas antigas enterradas.", .ember = 8, .region = "plains"},
    {.text = "Fogueiras na planície, longe demais para serem de lavradores.", .alertDays = 2, .region = "plains"},
    // floresta
    {.text = "Cogumelos venenosos misturados às provisões: parte das rações vai fora.", .rationsDice = 4, .region = "forest"},
    {.text = "Um carrapato no pescoço de alguém: a febre da mata chega à noite.", .diseaseChance = 0.6, .region = "forest"},
    {.text = "Uma clareira com o esconderijo de caçadores, abandonado há pouco.", .lootBag = true, .region = "forest"},
    {.text = "Galhos baixos arrancam a gazua do cinto de alguém; ninguém a encontra no mato.", .loseLockpick = true, .region = "forest"},
    {.text = "Uivos cercam o acampamento a noite inteira.", .alertDays = 3, .region = "forest"},
};

// The Underworld's road texts (the user, 2026-10-29: "crie mais 20 textos para encontros no underworld"): no battles, no alert
// there; rations, ember / gold, potions, lockpicks and disease. Applied by Overworld::undergroundText on an Underworld step.
const RoadEncounter kUnderworldEncounters[] = {
    {.text = "Esporos luminosos no ar das Marchas: alguém respira fundo demais.", .diseaseChance = 0.5},
    {.text = "Um bolsão de gás das fendas de ember apaga as lanternas; no escuro, rações rolam para o abismo.", .rationsDice = 5},
    {.text = "Cogumelos comestíveis, dos que os Ferreiros exilados conhecem: provisões a mais.", .rationsGain = 4},
    {.text = "Um veio de ember cru brilha na parede; as lascas valem bom ouro em Khar-Dûm.", .ember = 15},
    {.text = "Um barqueiro do Mar de Vidro cobra a travessia; não há outro caminho pela água.", .goldLossDice = 8},
    {.text = "A maré do Mar de Vidro sobe sem aviso: um frasco afunda.", .losePotion = true},
    {.text = "A ferrugem dos esporos come a gazua de alguém durante o descanso.", .loseLockpick = true},
    {.text = "Caçadores das Marchas Pálidas trocam carne seca de cego-das-cavernas por moedas.", .goldLossDice = 4, .rationsGain = 3},
    {.text = "A ponte de cordas sobre o abismo balança demais: uma mochila cai.", .rationsDice = 6},
    {.text = "Um Penitente acorrentado pede água e pão; o grupo divide as provisões.", .rationsDice = 2},
    {.text = "Mariposas de luz guiam a coluna até um bolsão de cristais.", .ember = 6},
    {.text = "O ar quente das forjas de Khar-Dûm apodrece a comida mais depressa.", .rationsDice = 4},
    {.text = "Um deslizamento de lascas de vidro corta as mãos; a febre vem à noite.", .diseaseChance = 0.4},
    {.text = "Um contrabandista das Marchas vende um mapa falso; o desvio custa rações e o preço do mapa.", .rationsDice = 3, .goldLossDice = 5},
    {.text = "Os ossos de um explorador da superfície e o saco de moedas que ele não gastou.", .ember = 10},
    {.text = "O calor dos respiradouros de ember racha um frasco na mochila.", .losePotion = true},
    {.text = "Um ninho de besouros gigantes abandonado: casulos cheios de larvas comestíveis.", .rationsGain = 5},
    {.text = "Ratos-cegos roem os sacos durante o descanso.", .rationsDice = 3},
    {.text = "Um Ferreiro Cinzento troca uma lasca de ember por uma gazua, sem perguntar de quem é.", .ember = 6, .loseLockpick = true},
    {.text = "A água do Mar de Vidro parece limpa. Não é.", .diseaseChance = 0.5},
};

double jsRound(double v) { return std::floor(v + 0.5); }

// `obj?.[key]` as a number: nullopt when absent, null or not a number (JS `?? fallback` then applies).
std::optional<double> num(const Json& obj, const std::string& key) {
  if (!obj.is_object()) return std::nullopt;
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_number()) return std::nullopt;
  return it->get<double>();
}
const Json& child(const Json& obj, const std::string& key) {
  static const Json kNull;
  if (!obj.is_object()) return kNull;
  const auto it = obj.find(key);
  return it == obj.end() ? kNull : *it;
}
bool has(const Json& array, const std::string& value) {
  if (!array.is_array()) return false;
  for (const Json& v : array)
    if (v.is_string() && v.get<std::string>() == value) return true;
  return false;
}
// JS truthiness of obj[key].
bool truthy(const Json& obj, const std::string& key) {
  const Json& v = child(obj, key);
  if (v.is_null()) return false;
  if (v.is_boolean()) return v.get<bool>();
  if (v.is_number()) return v.get<double>() != 0.0;
  if (v.is_string()) return !v.get<std::string>().empty();
  return true;
}
std::string key(int col, int row) { return std::to_string(col) + "," + std::to_string(row); }
// A JS number as JSON: integral values stay integers (as JSON.parse reads them back).
Json number(double v) {
  if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 9.0e15) return Json(static_cast<long long>(v));
  return Json(v);
}
std::string jsNumber(double v) {
  if (v == std::floor(v) && std::abs(v) < 9.0e15) return std::to_string(static_cast<long long>(v));
  return std::format("{}", v);
}

// poison.ts poisonTierOf
std::string poisonTierOf(const Json& value) {
  if (value.is_boolean() && value.get<bool>()) return "lesser";
  if (value.is_number()) {
    const double v = value.get<double>();
    if (v == 4) return "lesser";
    if (v == 10) return "poison";
    return {};
  }
  if (value.is_string()) {
    const std::string s = value.get<std::string>();
    if (s == "lesser" || s == "poison" || s == "greater" || s == "deadly" || s == "lethal") return s;
  }
  return {};
}

double hungerPenaltyFor(double streak) {
  const double daysPast = std::max(0.0, streak - kHungerGraceDays);
  return std::min(kHungerPenaltyMax, daysPast * kHungerPenaltyPerDay);
}

}  // namespace

// --- loading -------------------------------------------------------------------------------------------------------

bool Overworld::load(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    logError("overworld data not found: {}", file.string());
    return false;
  }
  const Json j = Json::parse(in, nullptr, false);
  if (j.is_discarded()) {
    logError("overworld data unreadable: {}", file.string());
    return false;
  }
  const Json& board = j["board"];
  auto hexOf = [](const Json& h) { return OverworldHex{h.value("col", 0), h.value("row", 0)}; };
  start_ = hexOf(board["startHex"]), inn_ = hexOf(board["innHex"]), ashenBlocked_ = hexOf(board["ashenBlocked"]);
  if (board.contains("wispHex") && board["wispHex"].is_object()) wisp_ = hexOf(board["wispHex"]);
  westEdgeCol_ = board.value("westEdgeCol", 0);
  gHexSize = board.value("hexSize", 5.0);
  for (const Json& id : board.value("stoneBridgeMissionIds", Json::array())) stoneBridgeMissionIds_.push_back(id.get<std::string>());
  for (const Json& c : board["cells"]) {
    OverworldCell cell;
    // Regra do jogo (o usuário, 2026-10-28): todo passo no mapa-múndi leva 6 horas (3 na grade de meio tamanho: "stepHours");
    // as horas de cada célula não são lidas.
    cell.col = c.value("col", 0), cell.row = c.value("row", 0), cell.hours = board.value("stepHours", 6);
    cell.region = c.value("region", ""), cell.x = c.value("x", 0.0), cell.y = c.value("y", 0.0);
    for (const Json& e : c.value("encounters", Json::array())) cell.encounters.push_back(e.get<std::string>());
    cells_.push_back(std::move(cell));
  }
  for (const Json& l : j["locations"]) {
    MapLocation m;
    m.id = l.value("id", ""), m.name = l.value("name", "");
    m.x = l.value("x", 0.0), m.y = l.value("y", 0.0), m.col = l.value("col", 0), m.row = l.value("row", 0);
    for (const Json& id : l.value("missionIds", Json::array())) m.missionIds.push_back(id.get<std::string>());
    m.openAccess = l.value("openAccess", false), m.encountersAllowed = l.value("encountersAllowed", false), m.warpCity = l.value("warpCity", false);
    if (l.contains("deadlineDay") && l["deadlineDay"].is_number()) m.deadlineDay = l["deadlineDay"].get<double>();
    locations_.push_back(std::move(m));
  }
  for (const Json& m : j["missions"]) {
    MapMission x;
    x.id = m.value("id", ""), x.title = m.value("title", ""), x.index = m.value("index", 0);
    x.win = m.contains("win") && m["win"].is_string() ? m["win"].get<std::string>() : std::string();
    x.hub = m.value("hub", false), x.crossing = m.value("crossing", false), x.gated = m.value("gated", false);
    x.keepsSpawns = m.value("keepsSpawns", false);
    missions_.push_back(std::move(x));
  }
  tables_ = j["tables"];
  for (const Json& w : tables_["weapons"])
    loot_.push_back({true, w["id"].get<std::string>(), w["name"].get<std::string>(), w["price"].get<double>(), w["power"].get<int>(), w["starting"].get<bool>()});
  for (const Json& e : tables_["equipment"])
    loot_.push_back({false, e["id"].get<std::string>(), e["name"].get<std::string>(), e["price"].get<double>(), e["power"].get<int>(), e["starting"].get<bool>()});
  logInfo("overworld: {} cells, {} locations, {} missions", cells_.size(), locations_.size(), missions_.size());
  // The undead road encounters (the Prece do Caminho keeps them away), kept beside the world data; none when the file is missing.
  undeadEncounters_.clear();
  if (std::ifstream undead{file.parent_path() / "undead-encounters.json", std::ios::binary}; undead) {
    const Json u = Json::parse(undead, nullptr, false);
    if (u.is_object())
      for (const Json& id : u.value("encounters", Json::array()))
        if (id.is_string()) undeadEncounters_.insert(id.get<std::string>());
    logInfo("overworld: {} road encounters field undead", undeadEncounters_.size());
  }
  shippedLocations_ = locations_, shippedMissions_ = missions_;
  shippedEncounters_.clear();
  for (const OverworldCell& c : cells_) shippedEncounters_.push_back(c.encounters);
  return true;
}

void Overworld::applyLocais(const Locais& locais, const CampaignData& campaign) {
  locations_ = shippedLocations_, missions_ = shippedMissions_;
  for (size_t i = 0; i < cells_.size() && i < shippedEncounters_.size(); ++i) cells_[i].encounters = shippedEncounters_[i];
  if (locais.own) {
    std::vector<MapLocation> next;
    for (const Locais::Base& b : locais.locations()) {
      MapLocation m;
      for (const MapLocation& s : shippedLocations_)
        if (s.id == b.id) m = s;
      if (m.id.empty()) m.id = b.id, m.name = b.name;
      m.missionIds = b.missionIds;
      next.push_back(std::move(m));
    }
    locations_ = std::move(next);
  }
  // Uma fonte só (o usuário, 2026-10-29: "não tem Stone Bridge em Stone Bridge"): toda missão da campanha entra no mapa-múndi,
  // com ou sem Locais salvos, na campanha e no Modo teste; antes só as 13 da tabela antiga do overworld.json existiam aqui.
  for (const CampaignMission& c : campaign.missions)
    if (!mission(c.id)) missions_.push_back(MapMission{c.id, c.title, c.win, c.index});
  if (locais.ownRegions)
    for (OverworldCell& c : cells_) {
      c.encounters.clear();
      for (const EncounterRegion& r : locais.regions)
        if (r.id == c.region) c.encounters = r.encounterIds;
    }
}

// --- the board -----------------------------------------------------------------------------------------------------

const OverworldCell* Overworld::cell(int col, int row) const {
  for (const OverworldCell& c : cells_)
    if (c.col == col && c.row == row) return &c;
  return nullptr;
}

std::vector<OverworldHex> Overworld::neighbors(int col, int row) {
  std::vector<OverworldHex> out;
  for (const BattleHex& h : Battle::neighbors(col, row)) out.push_back({h.x, h.y});
  return out;
}

// overworld.ts hexToWorld (odd-r offset to cube, pointy-top, hex size 5 % of the image; the board's own "hexSize").
void Overworld::hexToWorld(int col, int row, double& x, double& y) {
  const double q = col - (row - (row & 1)) / 2, r = row;
  x = gHexSize * std::sqrt(3.0) * (q + r / 2);
  y = gHexSize * 1.5 * r;
}

double Overworld::hexSize() { return gHexSize; }

Json Overworld::fitSave(Json save) const {
  if (!save.is_object()) return save;
  const double from = save.value("overworldGrid", 5.0);
  if (from == gHexSize) return save;
  // The cell of this board nearest to where (col, row) stood on the other grid.
  auto moved = [&](int col, int row) {
    const double q = col - (row - (row & 1)) / 2, x = from * std::sqrt(3.0) * (q + row / 2.0), y = from * 1.5 * row;
    const OverworldCell* best = nullptr;
    double bestD = 0;
    for (const OverworldCell& c : cells_)
      if (const double d = std::hypot(c.x - x, c.y - y); !best || d < bestD) best = &c, bestD = d;
    return best ? OverworldHex{best->col, best->row} : OverworldHex{col, row};
  };
  if (save.contains("overworldPos") && save["overworldPos"].is_object()) {
    const OverworldHex h = moved(save["overworldPos"].value("col", 0), save["overworldPos"].value("row", 0));
    save["overworldPos"] = Json{{"col", h.col}, {"row", h.row}};
  }
  if (save.contains("exploredHexes") && save["exploredHexes"].is_array()) {
    Json list = Json::array();
    std::set<std::string> seen;
    for (const Json& k : save["exploredHexes"]) {
      int c = 0, r = 0;
      if (!k.is_string() || std::sscanf(k.get<std::string>().c_str(), "%d,%d", &c, &r) != 2) continue;
      const OverworldHex h = moved(c, r);
      if (const std::string key = std::format("{},{}", h.col, h.row); seen.insert(key).second) list.push_back(key);
    }
    save["exploredHexes"] = std::move(list);
  }
  if (save.contains("runes") && save["runes"].is_array())
    for (Json& r : save["runes"])
      if (r.is_object() && r.contains("col") && r.contains("row")) {
        const OverworldHex h = moved(r.value("col", 0), r.value("row", 0));
        r["col"] = h.col, r["row"] = h.row;
      }
  save["overworldGrid"] = gHexSize;
  return save;
}

const MapLocation* Overworld::locationAt(int col, int row) const {
  const MapLocation* found = nullptr;
  for (const MapLocation& l : locations_)
    if (l.col == col && l.row == row) found = &l;
  return found;
}

const MapMission* Overworld::mission(const std::string& id) const {
  for (const MapMission& m : missions_)
    if (m.id == id) return &m;
  return nullptr;
}

std::vector<const MapMission*> Overworld::missionsOf(const MapLocation& location) const {
  std::vector<const MapMission*> out;
  for (const std::string& id : location.missionIds)
    if (const MapMission* m = mission(id)) out.push_back(m);
  return out;
}

int Overworld::missionIndex(const std::string& id) const {
  const MapMission* m = mission(id);
  return m ? m->index : -1;
}

// --- small rules ---------------------------------------------------------------------------------------------------

std::string Overworld::travelTimeLabel(int hours) {
  return hours == 24 ? "1 dia" : hours == 36 ? "1 dia e 12 horas" : std::to_string(hours) + " horas";
}

double Overworld::hungerPenalty(double streak) { return hungerPenaltyFor(streak); }

double Overworld::fullness(const Json& value) {
  return value.is_number() && std::isfinite(value.get<double>()) ? std::max(0.0, std::min(120.0, value.get<double>())) : 100.0;
}

int Overworld::campaignHour(const Json& save) {
  const std::optional<double> h = num(save, "gameHour");
  return h ? static_cast<int>(*h) : 8;
}

std::string Overworld::timeOfDay(int hour) {
  if (hour < 5) return "darkNight";
  if (hour < 8) return "dawn";
  if (hour < 11) return "day";
  if (hour < 15) return "noon";
  if (hour < 18) return "day";
  if (hour < 21) return "dusk";
  return "brightNight";
}

// data.ts heroRecruited (HERO_JOIN_INDEX exported per hero; null = never through a mission).
bool Overworld::heroRecruited(const std::string& hero, const Json& save) const {
  if (has(child(save, "flags"), "recruited:" + hero)) return true;
  const Json& join = child(tables_["joinIndex"], hero);
  if (!join.is_number()) return false;
  const int joinIndex = join.get<int>();
  if (joinIndex <= 0) return true;
  for (const Json& id : child(save, "completed"))
    if (id.is_string() && missionIndex(id.get<std::string>()) >= joinIndex - 1) return true;
  return false;
}

bool Overworld::inParty(const std::string& hero, const Json& save, bool test) const { return test || heroRecruited(hero, save); }

std::vector<std::string> Overworld::activeParty(const Json& save, bool test) const {
  std::vector<std::string> out;
  const Json& chosen = child(save, "partyActive");
  if (chosen.is_array()) {
    for (const Json& h : chosen)
      if (h.is_string() && out.size() < kActivePartyMax && inParty(h.get<std::string>(), save, test) &&
          std::find(out.begin(), out.end(), h.get<std::string>()) == out.end())
        out.push_back(h.get<std::string>());
    if (!out.empty()) return out;
  }
  for (const char* h : kPartyHeroes)  // no choice saved yet: the first six of the party, in the heroes' order
    if (out.size() < kActivePartyMax && inParty(h, save, test)) out.push_back(h);
  return out;
}

int Overworld::classHp(const std::string& classId, int level) const {
  const Json& c = child(tables_["classes"], classId);
  if (!c.is_object()) return 0;
  const Json& hp = c["hp"];
  const int i = std::clamp(level, 1, static_cast<int>(hp.size())) - 1;  // statsFor: n = max(0, min(MAX, level) - 1)
  return progressionClassHp(classId, i + 1, hp[static_cast<size_t>(i)].get<int>());  // a hero's class: EmberEngine's own growth
}

double Overworld::classResistance(const std::string& classId, const std::string& element) const {
  return num(child(child(tables_["classes"], classId), "resistances"), element).value_or(0.0);
}

// overworld.ts maxHpFor: the hero's class (promotion first), statsFor(class, level).hp; 0 without a class.
int Overworld::maxHp(const Json& save, const std::string& hero) const {
  std::string classId;
  const Json& promo = child(child(save, "promotions"), hero);
  if (promo.is_string()) classId = promo.get<std::string>();
  else if (const Json& base = child(tables_["baseClass"], hero); base.is_string()) classId = base.get<std::string>();
  if (classId.empty()) return 0;
  return classHp(classId, static_cast<int>(num(child(save, "levels"), hero).value_or(1.0)));
}

// gearStatBonus(Object.values(save.equipment[hero])).resistances[element]: 0 (no weapon), then each item in order.
double Overworld::gearResistance(const Json& save, const std::string& hero, const std::string& element) const {
  double total = 0.0;
  total = total + 0.0;  // the weapon's magic (none here)
  const Json& slots = child(child(save, "equipment"), hero);
  if (!slots.is_object()) return total;
  for (const auto& [slot, id] : slots.items()) {
    if (!id.is_string()) continue;
    for (const Json& e : tables_["equipment"])
      if (e["id"].get<std::string>() == id.get<std::string>()) {
        total = total + num(e["resistances"], element).value_or(0.0);
        break;
      }
  }
  return total;
}

// data.ts partyBagHasRoom: loose weapons + loose gear + ration stacks against 10 per recruited hero (test: per hero of
// all six) + pouches.
bool Overworld::bagHasRoom(const Json& save, int extra, bool test) const {
  std::set<std::string> worn;
  for (const auto& [hero, id] : child(save, "equipped").items())
    if (id.is_string()) worn.insert(id.get<std::string>());
  int used = 0;
  for (const auto& [id, level] : child(save, "weapons").items())
    if (!worn.contains(id)) ++used;
  double gear = 0;
  for (const auto& [id, qty] : child(save, "looseEquipment").items()) gear += qty.is_number() ? qty.get<double>() : 0.0;
  const double rations = num(save, "rations").value_or(0.0);
  const double stacks = std::ceil(rations / tables_.value("rationStackMax", 30));
  int heroes = 0;
  for (const char* h : kHeroes)
    if (inParty(h, save, test)) ++heroes;
  double pouches = 0;
  for (const auto& [hero, slots] : child(save, "equipment").items()) {
    const Json& waist = child(slots, "waist");
    if (!waist.is_string()) continue;
    for (const Json& e : tables_["equipment"])
      if (e["id"].get<std::string>() == waist.get<std::string>()) pouches += e.value("pouch", 0);
  }
  const double capacity = tables_.value("partyBagPerHero", 10) * std::max(1, heroes) + pouches;
  return used + gear + stacks + extra <= capacity;
}

// --- travel --------------------------------------------------------------------------------------------------------

bool Overworld::wispForestUncleared(const Json& save) const {
  for (const MapLocation& l : locations_)
    if (l.id == "wisp-forest") {
      for (const MapMission* m : missionsOf(l))
        if (!has(child(save, "completed"), m->id)) return true;
      return false;
    }
  return false;
}

// overworld.ts canStepOverworld.
bool Overworld::canStep(const Json& save, OverworldHex from, OverworldHex to, bool test) const {
  if (!isCell(to.col, to.row)) return false;
  // No Ashen Forest block (the user, 2026-10-27: "não existe bloqueio de floresta cinzenta"); `ashenBlocked_` is read for nothing now.
  if (test) return true;  // Modo teste: the whole grid open (removed cells stay excluded)
  const Json& completed = child(save, "completed");
  if (!has(completed, "vau")) return false;
  if (wisp_ && from.col <= wisp_->col && to.col > wisp_->col && wispForestUncleared(save)) return false;
  if (has(completed, "estalagem") || from == inn_) return true;
  // The early road: the start's row from the start to the Inn, and the Stone Bridge's hex (on the start's row on the 5 % grid,
  // the row below on the half grid). Back (west) always; east up to the bridge; past it once the bridge's missions are done.
  const MapLocation* bridge = nullptr;
  for (const MapLocation& l : locations_)
    if (l.id == "stonebridge") bridge = &l;
  const bool onBridge = bridge && to.col == bridge->col && to.row == bridge->row;
  if (!onBridge && (to.row != start_.row || to.col < start_.col || to.col > inn_.col)) return false;
  const OverworldCell* a = cell(from.col, from.row);
  const OverworldCell* b = cell(to.col, to.row);
  const OverworldCell* bridgeCell = bridge ? cell(bridge->col, bridge->row) : nullptr;
  if (!a || b->x < a->x) return true;
  if (bridgeCell && a->x < bridgeCell->x) return b->x <= bridgeCell->x;
  for (const std::string& id : stoneBridgeMissionIds_)
    if (!has(completed, id)) return false;
  return b->x > a->x;
}

Json Overworld::teleport(const Json& save, OverworldHex to) {
  Json out = save;
  out["overworldPos"] = Json{{"col", to.col}, {"row", to.row}};
  return out;
}

// overworld.ts stepOverworld.
std::optional<OverworldEvent> Overworld::undergroundText(Json& save, Mulberry32& rng, bool test) const {
  if (rng() >= kTextEncounterChance) return std::nullopt;
  constexpr size_t kCount = sizeof(kUnderworldEncounters) / sizeof(kUnderworldEncounters[0]);
  const RoadEncounter& pick = kUnderworldEncounters[static_cast<size_t>(std::floor(rng() * static_cast<double>(kCount)))];
  std::vector<std::string> parts{pick.text};
  const double rationsLost = pick.rationsDice ? 1 + std::floor(rng() * pick.rationsDice) : 0;
  const double goldLost = pick.goldLossDice ? std::min(num(save, "ember").value_or(0.0), 1 + std::floor(rng() * pick.goldLossDice)) : 0;
  save["rations"] = number(std::max(0.0, num(save, "rations").value_or(0.0) - rationsLost + pick.rationsGain));
  save["ember"] = number(std::max(0.0, num(save, "ember").value_or(0.0) + pick.ember - goldLost));
  if (rationsLost > 0) parts.push_back("−" + jsNumber(rationsLost) + (rationsLost == 1 ? " ração" : " rações"));
  if (pick.rationsGain > 0) parts.push_back("+" + std::to_string(pick.rationsGain) + " rações");
  if (goldLost > 0) parts.push_back("−" + jsNumber(goldLost) + " Gold");
  if (pick.ember > 0) parts.push_back("+" + std::to_string(pick.ember) + " Gold");
  if (pick.losePotion || pick.loseLockpick) {
    Json bags = child(save, "bags").is_object() ? child(save, "bags") : Json::object();
    std::vector<std::pair<std::string, std::string>> carried;  // hero, kind
    for (const auto& [hero, bag] : bags.items()) {
      if (pick.loseLockpick) {
        if (num(bag, "lockpick").value_or(0.0) > 0) carried.push_back({hero, "lockpick"});
      } else {
        for (const Json& kind : tables_["potionOrder"])
          if (num(bag, kind.get<std::string>()).value_or(0.0) > 0) carried.push_back({hero, kind.get<std::string>()});
      }
    }
    const double r = rng();
    if (!carried.empty()) {
      const auto& [hero, kind] = carried[static_cast<size_t>(std::floor(r * static_cast<double>(carried.size())))];
      bags[hero][kind] = number(std::max(0.0, num(bags[hero], kind).value_or(0.0) - 1));
      if (kind == "lockpick") parts.push_back("−1 Gazua");
      else
        for (const Json& p : tables_["potions"])
          if (p["id"].get<std::string>() == kind) parts.push_back("−1 " + p["name"].get<std::string>());
      save["bags"] = std::move(bags);
    }
  }
  if (pick.diseaseChance > 0 && rng() < pick.diseaseChance) {
    Json heroDiseases = child(save, "heroDiseases").is_object() ? child(save, "heroDiseases") : Json::object();
    std::vector<std::string> still;
    for (const char* hero : kHeroes)
      if ((test || heroRecruited(hero, save)) && !truthy(heroDiseases, hero)) still.push_back(hero);
    if (!still.empty()) {
      const std::string hero = still[static_cast<size_t>(std::floor(rng() * static_cast<double>(still.size())))];
      heroDiseases[hero] = true;
      parts.push_back(hero + " adoeceu (−10% nos atributos até ser curado)");
      save["heroDiseases"] = std::move(heroDiseases);
    }
  }
  std::string text;
  for (size_t i = 0; i < parts.size(); ++i) text += (i ? " · " : "") + parts[i];
  return OverworldEvent{"encounter", text, ""};
}

Json Overworld::step(const Json& save, OverworldHex to, Mulberry32& rng, std::optional<OverworldEvent>& event, bool test) const {
  event.reset();
  const Json& pos = child(save, "overworldPos");
  const OverworldHex from{pos.value("col", 0), pos.value("row", 0)};
  const std::vector<OverworldHex> around = neighbors(from.col, from.row);
  if (!isCell(to.col, to.row) || std::find(around.begin(), around.end(), to) == around.end() || !canStep(save, from, to, test)) return save;

  const Json& completed = child(save, "completed");
  Json heroHunger = child(save, "heroHunger").is_object() ? child(save, "heroHunger") : Json::object();
  const int travelHours = cell(to.col, to.row)->hours;
  const int totalHours = campaignHour(save) + travelHours;
  const std::string arrivalTime = timeOfDay(totalHours % 24);
  const int elapsedDays = totalHours / 24;
  // Modo teste: everyone shown in the party ages, recruited or not, whatever their HP record says.
  auto ages = [&](const std::string& hero) {
    if (test) return true;
    if (!heroRecruited(hero, save)) return false;
    const std::optional<double> hp = num(child(save, "unitHp"), hero);
    return (hp ? *hp : static_cast<double>(maxHp(save, hero))) > 0;
  };
  for (const char* hero : kHeroes)
    if (ages(hero)) heroHunger[hero] = number(std::max(0.0, fullness(child(heroHunger, hero)) - travelHungerCost(travelHours)));

  // Half the spent spell charges come back per day crossed.
  Json spellUses = Json::object();
  for (const auto& [hero, tiers] : child(save, "spellUses").items()) {
    Json next = Json::object();
    if (tiers.is_object())
      for (const auto& [tier, spent] : tiers.items()) {
        const double recovered = std::floor((spent.is_number() ? spent.get<double>() : 0.0) / std::pow(2.0, elapsedDays));
        if (recovered > 0) next[tier] = number(recovered);
      }
    if (!next.empty()) spellUses[hero] = std::move(next);
  }

  std::vector<std::string> travelling;
  for (const char* hero : kHeroes)
    if (ages(hero)) travelling.push_back(hero);
  bool fed = true;
  for (const std::string& hero : travelling)
    if (!(num(heroHunger, hero).value_or(100.0) > 0)) fed = false;
  const double rations = num(save, "rations").value_or(0.0);
  const double saveStreak = num(save, "hungerStreak").value_or(0.0);
  const double prevPenalty = hungerPenaltyFor(saveStreak);
  double startingFullness = std::numeric_limits<double>::infinity();
  for (const std::string& hero : travelling) startingFullness = std::min(startingFullness, fullness(child(child(save, "heroHunger"), hero)));
  const double newlyHungryHours = std::max(0.0, travelHours - startingFullness * 24 / tables_.value("dailyHungerCost", 50.0));
  const double priorHours = num(save, "hungerHours").value_or(saveStreak * 24);
  const double hungerHours = fed ? 0.0 : (startingFullness > 0 ? 0.0 : priorHours) + newlyHungryHours;
  const double hungerStreak = std::floor(hungerHours / 24);

  Json unitHp = child(save, "unitHp").is_object() ? child(save, "unitHp") : Json::object();
  if (fed && elapsedDays > 0)
    for (const char* hero : kHeroes) {
      const int max = maxHp(save, hero);
      if (max <= 0) continue;
      const double current = num(unitHp, hero).value_or(max);
      if (current <= 0 || current >= max) continue;
      unitHp[hero] = number(std::min(static_cast<double>(max), jsRound(current + max * kRecoveryPct * elapsedDays)));
    }
  // Poison ticks on the road (never below 1 HP).
  for (const char* hero : kHeroes) {
    const std::string tier = poisonTierOf(child(child(save, "heroPoisons"), hero));
    if (tier.empty() || !ages(hero)) continue;
    const int max = maxHp(save, hero);
    const double current = num(unitHp, hero).value_or(max);
    if (current <= 0) continue;
    const Json& t = tables_["poisonTiers"][tier];
    const int dice = t["dice"].get<int>(), faces = t["faces"].get<int>();
    double dmg = 0;
    for (int i = 0; i < dice; ++i) dmg += 1 + std::floor(rng() * faces);
    std::string cls;
    const Json& promo = child(child(save, "promotions"), hero);
    cls = promo.is_string() ? promo.get<std::string>() : child(tables_["baseClass"], hero).get<std::string>();
    double resist = 0.0;
    resist = resist + classResistance(cls, "poison");
    resist = resist + gearResistance(save, hero, "poison");
    resist = resist + num(child(child(save, "heroSkills"), hero), "poisonResistance").value_or(0.0);
    const double mag = num(child(save, "heroPoisonMag"), hero).value_or(0.0);
    const double effective = std::max(-50.0, std::min(100.0, resist - t["penalty"].get<double>() - mag / 2));
    dmg = std::max(0.0, jsRound(dmg * (1 - effective / 100)));
    unitHp[hero] = number(std::max(1.0, current - dmg));
  }

  const double newPenalty = hungerPenaltyFor(hungerStreak);
  if (newPenalty > prevPenalty) {
    event = newPenalty >= kHungerPenaltyMax
                ? OverworldEvent{"hungry", "O grupo desmaia de fome. Preciso retornar a uma estalagem.", ""}
                : OverworldEvent{"hungry", std::format("Fome: o grupo já passa {} dias sem comer (−{}% nos atributos).", jsNumber(hungerStreak),
                                                       jsNumber(jsRound(newPenalty * 100))),
                                 ""};
  }

  double rationsDelta = 0, emberDelta = 0;
  Json weapons = child(save, "weapons"), looseEquipment = child(save, "looseEquipment"), bags = child(save, "bags");
  Json heroDiseases = child(save, "heroDiseases").is_object() ? child(save, "heroDiseases") : Json::object();
  std::vector<std::string> healthy;
  for (const char* hero : kHeroes)
    if (ages(hero) && !truthy(heroDiseases, hero)) healthy.push_back(hero);
  std::string diseaseText;
  if (elapsedDays > 0 && !healthy.empty() && rng() < 1 - std::pow(1 - kTravelDiseaseChance, elapsedDays)) {
    const std::string hero = healthy[static_cast<size_t>(std::floor(rng() * static_cast<double>(healthy.size())))];
    heroDiseases[hero] = true;
    diseaseText = hero + " contraiu uma doença na estrada (−10% nos atributos até ser curado).";
  }
  const MapLocation* landed = locationAt(to.col, to.row);
  const Json emptyList = Json::array();
  const Json& seenRoad = child(save, "roadEncountersSeen").is_array() ? child(save, "roadEncountersSeen") : emptyList;
  const bool merchantCanAppear = !has(completed, "thebridge") || arrivalTime == "brightNight" || arrivalTime == "darkNight";
  // Prece do Caminho (ours, world/FieldSpells): while the save still counts hexes under it, the undead encounters are left out of
  // the roll and a battle is half as likely; this hex spends one.
  const int prayer = std::max(0, static_cast<int>(num(save, "roadPrayer").value_or(0)));
  std::vector<std::string> travelIds;
  for (const std::string& id : cell(to.col, to.row)->encounters) {
    if (prayer > 0 && undeadEncounters_.contains(id)) continue;
    if (id == "random-encounter-11" || id == "random-encounter-14") {
      if (merchantCanAppear) travelIds.push_back(id);
    } else if (!has(completed, id) && !has(seenRoad, id)) {
      travelIds.push_back(id);
    }
  }
  const double saveAlert = num(save, "alertStreak").value_or(std::numeric_limits<double>::quiet_NaN());
  const double battleChance = (saveAlert > 0 ? kAlertBattleChance : kBattleEncounterChance) * (prayer > 0 ? 0.5 : 1.0);
  double alertStreak = std::isnan(saveAlert) ? saveAlert : std::max(0.0, saveAlert - elapsedDays);
  const Json& lastRoad = child(save, "lastRoadEncounterId");
  std::optional<Json> lastRoadEncounterId;
  if (save.contains("lastRoadEncounterId")) lastRoadEncounterId = lastRoad;
  Json roadEncountersSeen = seenRoad;
  if (!event && (!landed || landed->encountersAllowed) && !travelIds.empty() && rng() < battleChance) {
    std::vector<std::string> pool = travelIds;
    if (travelIds.size() > 1)
      std::erase_if(pool, [&](const std::string& id) { return lastRoad.is_string() && lastRoad.get<std::string>() == id; });
    const std::string missionId = pool[static_cast<size_t>(std::floor(rng() * static_cast<double>(pool.size())))];
    lastRoadEncounterId = missionId;
    if (!has(roadEncountersSeen, missionId)) roadEncountersSeen.push_back(missionId);
    event = OverworldEvent{"battle", "", missionId};
  }
  if (!event && !landed && rng() < kTextEncounterChance) {
    // the hex's region's texts plus the eight of any terrain
    const std::string& region = cell(to.col, to.row)->region;
    std::vector<const RoadEncounter*> pool;
    for (const RoadEncounter& e : kEncounters)
      if (!e.region || region == e.region) pool.push_back(&e);
    const RoadEncounter& pick = *pool[static_cast<size_t>(std::floor(rng() * static_cast<double>(pool.size())))];
    if (pick.diseaseChance > 0 && rng() < pick.diseaseChance) {
      std::vector<std::string> still;
      for (const char* hero : kHeroes)
        if (ages(hero) && !truthy(heroDiseases, hero)) still.push_back(hero);
      if (!still.empty()) {
        const std::string hero = still[static_cast<size_t>(std::floor(rng() * static_cast<double>(still.size())))];
        heroDiseases[hero] = true;
        const std::string text = hero + " pegou um resfriado na chuva (−10% nos atributos até ser curado).";
        diseaseText = diseaseText.empty() ? text : diseaseText + " " + text;
      }
    }
    const Json& chest = tables_["chestLoot"];
    if (pick.lootBag) {
      // A small chest's roll: Ember, a weighted potion for the first hero with room, maybe gear, maybe rations.
      const double gain = chest["emberBase"].get<double>() + std::floor(rng() * chest["emberDice"].get<double>());
      emberDelta = gain;
      std::vector<std::string> found;
      std::string potionKind;
      {
        double total = 0;
        for (const Json& p : tables_["potions"]) total += p["weight"].get<double>();
        double roll = rng() * total;
        for (const Json& p : tables_["potions"]) {
          roll -= p["weight"].get<double>();
          if (roll <= 0) {
            potionKind = p["id"].get<std::string>();
            break;
          }
        }
        if (potionKind.empty()) potionKind = tables_["potions"].back()["id"].get<std::string>();
      }
      const Json* potion = nullptr;
      for (const Json& p : tables_["potions"])
        if (p["id"].get<std::string>() == potionKind) potion = &p;
      for (const char* hero : kHeroes) {
        if (!test && !heroRecruited(hero, save)) continue;
        if (num(child(bags, hero), potionKind).value_or(0.0) >= (*potion)["carryMax"].get<double>()) continue;
        Json bag = child(bags, hero).is_object() ? child(bags, hero) : tables_["emptyBag"];
        bag[potionKind] = number(num(child(bags, hero), potionKind).value_or(0.0) + 1);
        bags[hero] = std::move(bag);
        found.push_back((*potion)["name"].get<std::string>());
        break;
      }
      if (rng() < chest["gearChance"].get<double>()) {
        // data.ts weightedLootPick(rng, MAX_LEVEL, owned weapons): rarer as the price climbs.
        std::set<std::string> owned;
        for (const auto& [id, level] : child(save, "weapons").items()) owned.insert(id);
        const int maxLevel = tables_.value("maxLevel", 30);
        std::vector<std::pair<const LootEntry*, double>> entries;
        for (const LootEntry& e : loot_)
          if (!e.starting && e.power <= maxLevel && !(e.weapon && owned.contains(e.id))) entries.push_back({&e, 1 / std::sqrt(std::max(1.0, e.price))});
        double total = 0;
        for (const auto& [e, w] : entries) total += w;
        double roll = rng() * total;
        const LootEntry* drop = entries.back().first;
        for (const auto& [e, w] : entries) {
          roll -= w;
          if (roll <= 0) {
            drop = e;
            break;
          }
        }
        Json probe = save;
        if (drop->weapon) {
          probe["weapons"] = weapons;
          probe["weapons"][drop->id] = 0;
        } else {
          probe["looseEquipment"] = looseEquipment;
          probe["looseEquipment"][drop->id] = number(num(looseEquipment, drop->id).value_or(0.0) + 1);
        }
        if (bagHasRoom(probe, 0, test)) {
          if (drop->weapon) weapons[drop->id] = 0;
          else looseEquipment[drop->id] = number(num(looseEquipment, drop->id).value_or(0.0) + 1);
          found.push_back(drop->name);
        }
      }
      if (rng() < 0.4) {
        const double qty = 1 + std::floor(rng() * 4);
        rationsDelta += qty;
        found.push_back("Rações ×" + jsNumber(qty));
      }
      std::string text = std::string(pick.text) + " +" + jsNumber(gain) + " Gold";
      if (!found.empty()) {
        text += " · achou ";
        for (size_t i = 0; i < found.size(); ++i) text += (i ? ", " : "") + found[i];
      }
      event = OverworldEvent{"encounter", text, ""};
    } else {
      const double rationsLost = pick.rationsDice ? 1 + std::floor(rng() * pick.rationsDice) : 0;
      rationsDelta = -rationsLost + pick.rationsGain;
      const double goldLost = pick.goldLossDice ? std::min(num(save, "ember").value_or(0.0), 1 + std::floor(rng() * pick.goldLossDice)) : 0;
      emberDelta = pick.ember - goldLost;
      std::string lostItem;
      if (pick.losePotion) {
        std::vector<std::pair<std::string, std::string>> carried;  // hero, kind: bags in key order, POTIONS order within
        for (const auto& [hero, bag] : bags.items())
          for (const Json& kind : tables_["potionOrder"])
            if (num(bag, kind.get<std::string>()).value_or(0.0) > 0) carried.push_back({hero, kind.get<std::string>()});
        const double r = rng();
        if (!carried.empty()) {
          const auto& [hero, kind] = carried[static_cast<size_t>(std::floor(r * static_cast<double>(carried.size())))];
          bags[hero][kind] = number(std::max(0.0, num(bags[hero], kind).value_or(0.0) - 1));
          for (const Json& p : tables_["potions"])
            if (p["id"].get<std::string>() == kind) lostItem = "−1 " + p["name"].get<std::string>();
        }
      }
      if (pick.loseLockpick) {
        std::vector<std::string> holders;
        for (const auto& [hero, bag] : bags.items())
          if (num(bag, "lockpick").value_or(0.0) > 0) holders.push_back(hero);
        const double r = rng();
        if (!holders.empty()) {
          const std::string hero = holders[static_cast<size_t>(std::floor(r * static_cast<double>(holders.size())))];
          bags[hero]["lockpick"] = number(std::max(0.0, num(bags[hero], "lockpick").value_or(0.0) - 1));
          lostItem = "−1 Gazua";
        }
      }
      if (pick.alertDays) alertStreak = pick.alertDays;
      std::vector<std::string> parts{pick.text};
      if (rationsLost > 0) parts.push_back("−" + jsNumber(rationsLost) + (rationsLost == 1 ? " ração" : " rações"));
      if (pick.rationsGain > 0) parts.push_back("+" + std::to_string(pick.rationsGain) + " rações");
      if (goldLost > 0) parts.push_back("−" + jsNumber(goldLost) + " Gold");
      if (pick.ember > 0) parts.push_back("+" + std::to_string(pick.ember) + " Gold");
      if (!lostItem.empty()) parts.push_back(lostItem);
      if (pick.alertDays) parts.push_back("chance de emboscada dobrada por " + std::to_string(pick.alertDays) + " dias");
      std::string text;
      for (size_t i = 0; i < parts.size(); ++i) text += (i ? " · " : "") + parts[i];
      event = OverworldEvent{"encounter", text, ""};
    }
  }
  if (!diseaseText.empty() && (!event || event->kind != "battle")) {
    if (event) event->text += " · " + diseaseText;
    else event = OverworldEvent{"encounter", diseaseText, ""};
  }

  Json exploredHexes = child(save, "exploredHexes").is_array() ? child(save, "exploredHexes") : Json::array();
  if (!has(exploredHexes, key(to.col, to.row))) exploredHexes.push_back(key(to.col, to.row));
  // skills.ts advanceTravelTraining: each travelling hero with a chosen skill banks the step's hours, +0.1 per 12 h.
  Json heroSkills = child(save, "heroSkills").is_object() ? child(save, "heroSkills") : Json::object();
  Json trainingHours = child(save, "travelTrainingHours").is_object() ? child(save, "travelTrainingHours") : Json::object();
  const double stepHours = tables_.value("travelTrainingHours", 12.0), gain = tables_.value("skillGain", 0.1), cap = tables_.value("skillCap", 100.0);
  for (const std::string& hero : travelling) {
    const Json& id = child(child(save, "travelTraining"), hero);
    if (!id.is_string() || id.get<std::string>().empty()) continue;
    const double total = num(trainingHours, hero).value_or(0.0) + std::max(0, travelHours);
    const double gains = std::floor(total / stepHours);
    trainingHours[hero] = number(total - gains * stepHours);
    if (gains > 0) {
      const double next = std::min(cap, jsRound((num(child(heroSkills, hero), id.get<std::string>()).value_or(0.0) + gains * gain) * 10) / 10);
      Json skills = child(heroSkills, hero).is_object() ? child(heroSkills, hero) : Json::object();
      skills[id.get<std::string>()] = number(next);
      heroSkills[hero] = std::move(skills);
    }
  }

  Json out = save;
  out["overworldPos"] = Json{{"col", to.col}, {"row", to.row}};
  out["exploredHexes"] = std::move(exploredHexes);
  out["gameClock"] = number(num(save, "gameClock").value_or(0.0) + elapsedDays);
  out["gameHour"] = totalHours % 24;
  out["overworldMoveBudgetUsed"] = number(num(save, "overworldMoveBudgetUsed").value_or(0.0) + 1);
  out["heroHunger"] = std::move(heroHunger);
  out["heroDiseases"] = std::move(heroDiseases);
  out["rations"] = number(std::max(0.0, rations + rationsDelta));
  out["ember"] = number(std::max(0.0, num(save, "ember").value_or(0.0) + emberDelta));
  out["hungerStreak"] = number(hungerStreak);
  out["hungerHours"] = number(hungerHours);
  out["alertStreak"] = std::isnan(alertStreak) ? Json() : number(alertStreak);
  if (lastRoadEncounterId) out["lastRoadEncounterId"] = *lastRoadEncounterId;
  out["roadEncountersSeen"] = std::move(roadEncountersSeen);
  out["unitHp"] = std::move(unitHp);
  if (!weapons.is_null()) out["weapons"] = std::move(weapons);
  if (!looseEquipment.is_null()) out["looseEquipment"] = std::move(looseEquipment);
  if (!bags.is_null()) out["bags"] = std::move(bags);
  out["spellUses"] = std::move(spellUses);
  out["heroSkills"] = std::move(heroSkills);
  out["travelTrainingHours"] = std::move(trainingHours);
  if (prayer > 1) out["roadPrayer"] = prayer - 1;
  else if (prayer == 1) out.erase("roadPrayer");
  return out;
}

// --- progression (progression.ts, quests.ts) and access (access.ts) -----------------------------------------------

int Overworld::questStage(const std::string& questId, const Json& save) const {
  for (const Json& q : tables_["quests"]) {
    if (q.value("id", "") != questId) continue;
    if (has(child(save, "questsDone"), questId)) return 3;
    if (!has(child(save, "questsActive"), questId)) return 0;
    int haveCount = 0, total = 0;
    const std::string kind = q.value("kind", "");
    if (kind == "kill") total = 1, haveCount = has(child(save, "questKills"), q.value("targetName", "")) ? 1 : 0;
    else if (kind == "recruit") total = 1, haveCount = has(child(save, "flags"), "recruited:" + q.value("targetName", "")) ? 1 : 0;
    else
      for (const Json& p : q.value("pickups", Json::array())) {
        ++total;
        if (has(child(save, "questItems"), questId + ":" + p.value("id", ""))) ++haveCount;
      }
    return haveCount >= total ? 2 : 1;
  }
  return 0;
}

bool Overworld::evaluate(const Json& c, const Json& save, bool storedChapter) const {
  if (c.is_null()) return true;
  if (c.contains("all")) {
    for (const Json& x : c["all"])
      if (!evaluate(x, save, storedChapter)) return false;
    return true;
  }
  if (c.contains("any")) {
    for (const Json& x : c["any"])
      if (evaluate(x, save, storedChapter)) return true;
    return false;
  }
  if (c.contains("not")) return !evaluate(c["not"], save, storedChapter);
  if (c.contains("chapterReached")) {
    const int chapter = storedChapter ? std::max(1, static_cast<int>(num(save, "chapter").value_or(1))) : currentChapter(save);
    return chapter >= c["chapterReached"].get<int>();
  }
  if (c.contains("missionCompleted")) return has(child(save, "completed"), c["missionCompleted"].get<std::string>());
  if (c.contains("npcTalkedTo")) return has(child(save, "npcTalked"), c["npcTalkedTo"].get<std::string>());
  if (c.contains("flagSet")) return has(child(save, "flags"), c["flagSet"].get<std::string>());
  if (c.contains("itemOwned")) {
    const std::string id = c["itemOwned"].get<std::string>();
    if (child(save, "weapons").contains(id) || child(save, "looseEquipment").contains(id)) return true;
    for (const auto& [hero, slots] : child(save, "equipment").items())
      for (const auto& [slot, item] : slots.items())
        if (item.is_string() && item.get<std::string>() == id) return true;
    return false;
  }
  if (c.contains("partyMemberPresent")) return heroRecruited(c["partyMemberPresent"].get<std::string>(), save);
  if (c.contains("questDiscovered")) {
    const std::string id = c["questDiscovered"].get<std::string>();
    return questStage(id, save) >= 1 || has(child(save, "questsDiscovered"), id);
  }
  if (c.contains("questActive")) {
    const int stage = questStage(c["questActive"].get<std::string>(), save);
    return stage == 1 || stage == 2;
  }
  if (c.contains("questStage")) return questStage(c["questStage"][0].get<std::string>(), save) >= c["questStage"][1].get<int>();
  if (c.contains("questCompleted")) return questStage(c["questCompleted"].get<std::string>(), save) >= 3;
  return false;
}

int Overworld::currentChapter(const Json& save) const {
  int chapter = std::max(1, static_cast<int>(num(save, "chapter").value_or(1)));
  for (const Json& def : tables_["progression"].value("chapters", Json::array())) {
    const int id = def.value("id", 0);
    if (id > chapter && def.contains("reachedWhen") && !def["reachedWhen"].is_null() && evaluate(def["reachedWhen"], save, true)) chapter = id;
  }
  return chapter;
}

std::string Overworld::missionAccess(const std::string& id, const Json& save, bool test) const {
  if (has(child(save, "completed"), id)) return "completed";
  const Json& gates = tables_["progression"]["missionGates"];
  if (test || !gates.contains(id)) return "available";
  const Json& gate = gates[id];
  if (!evaluate(child(gate, "visible"), save, false)) return "hidden";
  if (!evaluate(child(gate, "unlock"), save, false)) return "revealed";
  return "available";
}

const MapLocation* Overworld::previousPopulatedLocation(const MapLocation& location) const {
  const auto at = std::find_if(locations_.begin(), locations_.end(), [&](const MapLocation& l) { return l.id == location.id; });
  for (auto it = at; it != locations_.begin();) {
    --it;
    for (const MapMission* m : missionsOf(*it))
      if (!m->gated) return &*it;
  }
  return nullptr;
}

bool Overworld::lockedMission(const std::string& id, const Json& save, const std::vector<std::string>& order, bool test) const {
  if (test) return false;
  const Json& completed = child(save, "completed");
  if (has(completed, id)) return true;
  if (const MapMission* m = mission(id); m && m->gated) return missionAccess(id, save) != "available";
  const MapLocation* location = nullptr;
  for (const MapLocation& l : locations_)
    if (std::find(l.missionIds.begin(), l.missionIds.end(), id) != l.missionIds.end()) {
      location = &l;
      break;
    }
  if (location && location->openAccess) return false;
  if (!location) {
    const auto at = std::find(order.begin(), order.end(), id);
    if (at == order.end()) return true;
    for (auto it = order.begin(); it != at; ++it)
      if (!has(completed, *it)) return true;
    return false;
  }
  std::vector<std::string> ids;
  for (const MapMission* m : missionsOf(*location))
    if (!m->gated) ids.push_back(m->id);
  const auto at = std::find(ids.begin(), ids.end(), id);
  if (at == ids.end()) return true;
  if (at != ids.begin()) {
    for (auto it = ids.begin(); it != at; ++it)
      if (!has(completed, *it)) return true;
    return false;
  }
  const MapLocation* previous = previousPopulatedLocation(*location);
  if (!previous) return false;
  for (const MapMission* m : missionsOf(*previous))
    if (!m->gated && !has(completed, m->id)) return true;
  return false;
}

std::string Overworld::missionStatus(const std::string& id, const Json& save, const std::vector<std::string>& order, bool test) const {
  if (has(child(save, "completed"), id)) return "done";
  return lockedMission(id, save, order, test) ? "locked" : "available";
}

std::vector<const MapMission*> Overworld::visibleMissionsOf(const MapLocation& location, const Json& save, bool test) const {
  std::vector<const MapMission*> out;
  for (const MapMission* m : missionsOf(location))
    if (test || missionAccess(m->id, save) != "hidden") out.push_back(m);
  return out;
}

bool Overworld::locationVisible(const MapLocation& location, const Json& save, bool test) const {
  for (const std::string& id : location.missionIds)
    if (test || missionAccess(id, save) != "hidden") return true;
  return false;
}

// access.ts locationStatus. Debug mode opens every location, even one with no missions yet.
std::string Overworld::locationStatus(const MapLocation& location, const Json& save, bool test) const {
  const std::vector<const MapMission*> missions = visibleMissionsOf(location, save, test);
  std::vector<const MapMission*> pending;
  for (const MapMission* m : missions)
    if (!has(child(save, "completed"), m->id)) pending.push_back(m);
  if (pending.empty()) return !missions.empty() ? "done" : test ? "available" : "locked";
  std::vector<std::string> order;
  for (const MapMission* m : missionsOf(location)) order.push_back(m->id);
  for (const MapMission* m : pending)
    if (missionStatus(m->id, save, order, test) != "locked") return "available";
  return "locked";
}

// --- the check against Ember -------------------------------------------------------------------------------------

namespace {

// Deep equality as JSON.stringify would see it (key order ignored, numbers compared by value).
bool same(const Json& a, const Json& b) {
  if (a.is_number() && b.is_number()) return a.get<double>() == b.get<double>();
  if (a.type() != b.type()) return false;
  if (a.is_object()) {
    if (a.size() != b.size()) return false;
    for (const auto& [k, v] : a.items())
      if (!b.contains(k) || !same(v, b[k])) return false;
    return true;
  }
  if (a.is_array()) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
      if (!same(a[i], b[i])) return false;
    return true;
  }
  return a == b;
}

std::string firstDifference(const Json& a, const Json& b, const std::string& path = "") {
  if (a.is_object() && b.is_object()) {
    for (const auto& [k, v] : a.items()) {
      if (!b.contains(k)) return path + "." + k + " (only in C++)";
      if (!same(v, b[k])) return firstDifference(v, b[k], path + "." + k);
    }
    for (const auto& [k, v] : b.items())
      if (!a.contains(k)) return path + "." + k + " (only in Ember)";
  }
  return path + ": C++ " + a.dump() + " vs Ember " + b.dump();
}

}  // namespace

int checkOverworld(const Overworld& world, const std::filesystem::path& references) {
  std::ifstream in(references, std::ios::binary);
  const Json refs = Json::parse(in, nullptr, false);
  if (refs.is_discarded()) {
    logError("overworld references unreadable: {}", references.string());
    return 1;
  }
  int differences = 0;
  auto report = [&](const std::string& what) {
    if (++differences <= 20) logError("overworld differs: {}", what);
  };
  int statusChecks = 0, stepPairs = 0, steps = 0, events = 0;
  // One block of answers: the campaign's (test false) or Modo teste's (test true).
  auto checkBlock = [&](const Json& block, bool test) {
  const std::string tag = test ? "test " : "";
  // Location and mission status.
  for (const Json& r : block["locations"]) {
    const Json& save = r["save"];
    if (r.contains("chapter") && world.currentChapter(save) != r["chapter"].get<int>()) report("chapter");
    for (const Json& l : r["locations"]) {
      const MapLocation* loc = nullptr;
      for (const MapLocation& x : world.locations())
        if (x.id == l["id"].get<std::string>()) loc = &x;
      if (!loc) {
        report("location " + l["id"].get<std::string>() + " missing");
        continue;
      }
      ++statusChecks;
      if (world.locationStatus(*loc, save, test) != l["status"].get<std::string>())
        report(std::format("{}location {} status: C++ {} vs Ember {}", tag, loc->id, world.locationStatus(*loc, save, test), l["status"].get<std::string>()));
      if (l.contains("visible") && world.locationVisible(*loc, save, test) != l["visible"].get<bool>()) report(tag + "location " + loc->id + " visibility");
      std::vector<std::string> order;
      for (const MapMission* m : world.missionsOf(*loc)) order.push_back(m->id);
      const std::vector<const MapMission*> visible = world.visibleMissionsOf(*loc, save, test);
      if (visible.size() != l["missions"].size()) report(tag + "location " + loc->id + " visible missions");
      for (const Json& m : l["missions"]) {
        const std::string id = m["id"].get<std::string>();
        if (world.missionAccess(id, save, test) != m["access"].get<std::string>()) report(tag + "mission " + id + " access");
        if (world.missionStatus(id, save, order, test) != m["status"].get<std::string>()) report(tag + "mission " + id + " status");
      }
    }
  }
  // Allowed steps.
  for (const Json& r : block["canStep"]) {
    std::set<std::string> expected;
    for (const Json& a : r["allowed"]) expected.insert(a.get<std::string>());
    for (const OverworldCell& c : world.cells())
      for (const OverworldHex& n : Overworld::neighbors(c.col, c.row)) {
        const std::string k = std::format("{},{}>{},{}", c.col, c.row, n.col, n.row);
        ++stepPairs;
        if (world.canStep(r["save"], {c.col, c.row}, n, test) != expected.contains(k)) report(tag + "canStep " + k);
      }
  }
  // Seeded steps: the save afterwards, the event, and how many random numbers were drawn.
  for (const Json& r : block["steps"]) {
    ++steps;
    Mulberry32 base(r["seed"].get<uint32_t>());
    int calls = 0;
    Mulberry32 rng(r["seed"].get<uint32_t>());
    std::optional<OverworldEvent> event;
    const Json after = world.step(r["save"], {r["to"]["col"].get<int>(), r["to"]["row"].get<int>()}, rng, event, test);
    // How many draws the port made: step a second generator until it matches the first one's next value.
    {
      const double next = rng();
      for (calls = 0; calls < 64 && base() != next; ++calls) {
      }
    }
    if (calls != r["calls"].get<int>()) report(std::format("{}step {}: {} random numbers vs Ember {}", tag, steps, calls, r["calls"].get<int>()));
    if (!same(after, r["after"])) report(std::format("{}step {}: save {}", tag, steps, firstDifference(after, r["after"])));
    const Json& e = r["event"];
    if (e.is_null() != !event.has_value()) {
      report(std::format("{}step {}: event {} vs Ember {}", tag, steps, event ? event->text : "none", e.dump()));
    } else if (event) {
      ++events;
      if (event->kind != e.value("kind", "") || event->text != e.value("text", "") || event->missionId != e.value("missionId", ""))
        report(std::format("{}step {}: event '{}' vs Ember '{}'", tag, steps, event->text, e.value("text", "")));
    }
  }
  };
  checkBlock(refs, false);
  const int campaignSteps = steps;
  if (refs.contains("test")) checkBlock(refs["test"], true);
  else report("no Modo teste answers in the references (re-run export_overworld.ts)");
  if (!differences)
    logInfo("overworld matches Ember ({} location checks, {} step pairs, {} seeded steps ({} in Modo teste) with {} events)", statusChecks,
            stepPairs, steps, steps - campaignSteps, events);
  else
    logError("overworld: {} differences", differences);
  return differences;
}

}  // namespace ember
