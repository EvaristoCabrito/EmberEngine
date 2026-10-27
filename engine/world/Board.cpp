#include "world/Board.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "world/GroundAssets.h"

namespace ember::e2 {

namespace {

using json = nlohmann::json;

std::optional<json> readJson(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  json j = json::parse(in, nullptr, false);
  if (j.is_discarded()) return std::nullopt;
  return j;
}

std::string str(const json& j, const char* key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); }
double num(const json& j, const char* key, double fallback = 0) { return j.contains(key) && j[key].is_number() ? j[key].get<double>() : fallback; }

// URL under Engine2's public folder, with "dir/../" segments resolved ("/game/a/../b/x.png" -> "/game/b/x.png").
std::string cleanUrl(const std::string& url) {
  if (url.empty()) return url;
  return std::filesystem::path(url).lexically_normal().generic_string();
}

std::vector<MapSpawn> spawns(const json& d, const char* key) {
  std::vector<MapSpawn> out;
  if (!d.contains(key) || !d[key].is_array()) return out;
  for (const json& s : d[key]) {
    out.push_back({str(s, "name"), str(s, "classId"), static_cast<int>(num(s, "x")), static_cast<int>(num(s, "y"))});
    if (s.is_object() && s.contains("dialog") && s["dialog"].is_object()) out.back().dialog = s["dialog"];
  }
  return out;
}

}  // namespace

std::optional<BoardData> BoardData::load(const std::filesystem::path& boardJson) {
  const std::optional<json> j = readJson(boardJson);
  if (!j) {
    logError("board data: cannot read {}", boardJson.string());
    return std::nullopt;
  }
  BoardData d;
  const json& c = (*j)["constants"];
  d.baseLevels = static_cast<int>(num(c, "BASE_LEVELS", 2));
  d.mountainRock = num(c, "MOUNTAIN_ROCK", 7);
  d.mountainSnow = num(c, "MOUNTAIN_SNOW", 11);
  for (const auto& [k, v] : (*j)["surfaces"].items()) d.surfaces[k] = v.get<int>();
  for (const auto& [k, v] : (*j)["terrain"].items())
    d.terrain[k] = {str(v, "name"), str(v, "surface"), v.value("passable", true), num(v, "minLevel"), num(v, "raise")};
  d.groundLayerTerrains = (*j)["groundLayerTerrains"].get<std::vector<std::string>>();
  d.groundLayerCount = (*j)["groundLayerCount"].get<int>();
  for (const json& l : (*j)["groundLayers"]) {
    GroundLayerSource s;
    s.key = str(l, "key");
    s.absent = l.value("absent", false);
    s.procedural = str(l, "procedural");
    s.color = cleanUrl(str(l, "color"));
    s.normal = cleanUrl(str(l, "normal"));
    s.rough = cleanUrl(str(l, "rough"));
    s.relief = num(l, "relief");
    s.reliefGrid = cleanUrl(str(l, "reliefGrid"));
    d.groundLayers.push_back(std::move(s));
  }
  for (const json& v : (*j)["groundVariantLayers"])
    d.groundVariantLayers.push_back({str(v, "key"), str(v, "terrain"), static_cast<int>(num(v, "variant")), num(v, "relief")});
  for (const json& v : (*j)["engine2V3GroundVariants"]) {
    d.engine2V3.emplace_back(str(v, "terrain"), static_cast<int>(num(v, "variant")));
    d.engine2V3Keys.push_back(str(v, "key"));
  }
  d.engine2V3CityVariant = (*j)["engine2V3CityVariant"].get<int>();
  for (const auto& [k, v] : (*j)["claudeVariant"].items()) d.claudeVariant[k] = v.get<int>();
  d.architectureDecorations = (*j)["architectureDecorations"].get<std::vector<std::string>>();
  std::sort(d.architectureDecorations.begin(), d.architectureDecorations.end());
  if (static_cast<int>(d.groundLayers.size()) != d.groundLayerCount) {
    logError("board data: {} ground layer sources for {} layers", d.groundLayers.size(), d.groundLayerCount);
    return std::nullopt;
  }
  d.engine2LayerCount = d.groundLayerCount;
  for (const EmberGroundVariant& v : kEmberGroundVariants) {
    GroundLayerSource s;
    s.key = v.key;
    s.file = v.file;
    s.tile = v.tile;
    d.groundVariantLayers.push_back({std::string(v.key), std::string(v.terrain), v.variant, 0.0});
    d.groundLayers.push_back(std::move(s));
  }
  for (const EmberGroundTile& t : kEmberGroundTiles) {
    GroundLayerSource s;
    s.key = t.key;
    s.file = t.file;
    s.tile = t.tile;
    d.emberGroundLayer[std::string(t.map) + "|" + std::string(t.terrain)] = static_cast<int>(d.groundLayers.size());
    if (t.overBaked) d.emberGroundOver.insert(std::string(t.map) + "|" + std::string(t.terrain));
    d.groundLayers.push_back(std::move(s));
  }
  d.groundLayerCount = static_cast<int>(d.groundLayers.size());
  return d;
}

int BoardData::groundVariantLayer(const std::string& terrainId, int variant) const {
  for (size_t i = 0; i < groundVariantLayers.size(); ++i)
    if (groundVariantLayers[i].terrain == terrainId && groundVariantLayers[i].variant == variant) return static_cast<int>(i);
  return -1;
}

int BoardData::baseLayerIndex(const std::string& terrainId) const {
  const auto it = std::find(groundLayerTerrains.begin(), groundLayerTerrains.end(), terrainId);
  return it == groundLayerTerrains.end() ? -1 : static_cast<int>(it - groundLayerTerrains.begin());
}

bool BoardData::isArchitecture(const std::string& decorationId) const {
  return std::binary_search(architectureDecorations.begin(), architectureDecorations.end(), decorationId);
}

std::optional<MapDraft> MapDraft::load(const std::filesystem::path& mapJson) {
  const std::optional<json> file = readJson(mapJson);
  if (!file) {
    logError("map: cannot read {}", mapJson.string());
    return std::nullopt;
  }
  std::optional<MapDraft> m = fromJson(file->contains("draft") ? (*file)["draft"] : *file);
  if (!m) logError("map: {} has no board", mapJson.string());
  return m;
}

std::optional<MapDraft> MapDraft::fromJson(const json& d) {
  MapDraft m;
  m.id = str(d, "id");
  m.title = str(d, "title");
  m.cols = static_cast<int>(num(d, "cols"));
  m.rows = static_cast<int>(num(d, "rows"));
  if (m.cols <= 0 || m.rows <= 0 || !d.contains("tiles") || !d["tiles"].is_array()) return std::nullopt;
  for (const json& t : d["tiles"]) m.tiles.push_back(t.is_string() ? t.get<std::string>() : std::string("plains"));
  if (d.contains("tileVariants") && d["tileVariants"].is_array())
    for (const json& v : d["tileVariants"]) m.tileVariants.push_back(v.is_number() ? std::optional<int>(v.get<int>()) : std::nullopt);
  if (d.contains("terrainElevations") && d["terrainElevations"].is_array())
    for (const json& v : d["terrainElevations"])
      m.terrainElevations.push_back(v.is_number() ? std::optional<double>(v.get<double>()) : std::nullopt);
  if (d.contains("decorations") && d["decorations"].is_array())
    for (const json& p : d["decorations"]) {
      MapDecoration o{str(p, "id"), static_cast<int>(num(p, "x")), static_cast<int>(num(p, "y")), num(p, "rot")};
      o.mirrorX = p.contains("mirrorX") && p["mirrorX"].is_boolean() && p["mirrorX"].get<bool>();
      o.returnConnector = p.contains("returnConnector") && p["returnConnector"].is_boolean() && p["returnConnector"].get<bool>();
      o.connectorDirection = str(p, "connectorDirection");
      o.blocksPath = p.contains("blocksPath") && p["blocksPath"].is_boolean() && p["blocksPath"].get<bool>();
      m.decorations.push_back(std::move(o));
    }
  m.playerSpawns = spawns(d, "playerSpawns");
  m.water3d = Water3DMap::fromDraft(d);
  m.enemySpawns = spawns(d, "enemySpawns");
  m.neutralSpawns = spawns(d, "neutralSpawns");
  m.environment = str(d, "environment");
  m.timeOfDay = str(d, "timeOfDay");
  m.moonPhase = str(d, "moonPhase");
  m.fog = d.contains("fog") && d["fog"].is_boolean() && d["fog"].get<bool>();
  m.mistType = str(d, "mistType");
  m.atmosphereProfile = str(d, "atmosphereProfile");
  m.emberTiles = str(d, "emberTiles");
  m.emberFloor = str(d, "emberFloor");
  auto num = [&](const char* k, float fallback) { return d.contains(k) && d[k].is_number() ? d[k].get<float>() : fallback; };
  m.emberFloorDepth = num("emberFloorDepth", m.emberFloorDepth);
  m.emberFloorTile = num("emberFloorTile", m.emberFloorTile);
  m.emberWindFog = static_cast<int>(num("emberWindFog", 0.0f));
  m.emberDeckClip = d.contains("emberDeckClip") && d["emberDeckClip"].is_boolean() && d["emberDeckClip"].get<bool>();
  m.emberNoSides = d.contains("emberNoSides") && d["emberNoSides"].is_boolean() && d["emberNoSides"].get<bool>();
  m.underground = d.contains("underground") && d["underground"].is_boolean() && d["underground"].get<bool>();
  m.aboveGround = !m.underground && d.contains("aboveGround") && d["aboveGround"].is_boolean() && d["aboveGround"].get<bool>();
  if (d.contains("emberBaseLevels") && d["emberBaseLevels"].is_number()) m.emberBaseLevels = d["emberBaseLevels"].get<int>();
  m.mistIntensity = num("mistIntensity", m.mistType.empty() ? 0.0f : 0.2f);
  m.mistSpeed = num("mistSpeed", 1.0f);
  m.wispIntensity = num("wispIntensity", 0.0f);
  m.wispSpeed = num("wispSpeed", 1.0f);
  if (d.contains("wispColor") && d["wispColor"].is_number()) m.wispColor = d["wispColor"].get<unsigned>();
  m.bloomIntensity = num("bloomIntensity", 0.0f);
  if (d.contains("backgroundImage") && d["backgroundImage"].is_string()) m.backgroundImage = d["backgroundImage"].get<std::string>();
  if (d.contains("groundImage") && d["groundImage"].is_string()) m.groundImage = d["groundImage"].get<std::string>();
  if (d.contains("sunIntensity") && d["sunIntensity"].is_number()) m.sunIntensity = d["sunIntensity"].get<double>();
  if (d.contains("ambientIntensity") && d["ambientIntensity"].is_number()) m.ambientIntensity = d["ambientIntensity"].get<double>();
  if (d.contains("bloomIntensity") && d["bloomIntensity"].is_number()) m.bloomSlider = d["bloomIntensity"].get<double>();
  return m;
}

Board::Board(const BoardData& data, MapDraft draft) : data_(data), draft_(std::move(draft)), layout_(makeLayout(draft_.cols, draft_.rows)) {
  const TerrainInfo& plains = data_.terrain.at("plains");
  const int baseLayers = static_cast<int>(data_.groundLayerTerrains.size());
  for (int r = 0; r < draft_.rows; r++) {
    for (int c = 0; c < draft_.cols; c++) {
      Cell cell;
      cell.c = c;
      cell.r = r;
      cell.i = indexOf(layout_, c, r);
      const int i = cell.i;
      cell.type = i < static_cast<int>(draft_.tiles.size()) ? draft_.tiles[i] : "plains";
      const auto found = data_.terrain.find(cell.type);
      const TerrainInfo& info = found != data_.terrain.end() ? found->second : plains;
      const double authored = i < static_cast<int>(draft_.terrainElevations.size()) && draft_.terrainElevations[i] ? *draft_.terrainElevations[i] : 0;
      cell.level = std::max({authored, info.minLevel, -1.0});
      cell.water = cell.type == "water";
      // High ground reads as mountain: bare rock up high, snow on the peaks.
      std::string surf = info.surface;
      if (surf != "void" && !cell.water) {
        if (cell.level >= data_.mountainSnow) surf = "snow";
        else if (cell.level >= data_.mountainRock && (surf == "grass" || surf == "forest" || surf == "ash")) surf = "rock";
      }
      const int variant = draft_.variant(i);
      const bool plains049 = cell.type == "plains" && surf == "grass" && draft_.hasVariant(i) && variant == 48;
      cell.surface = surf == "void" ? -1 : plains049 ? data_.surfaces.at("plains049") : data_.surfaces.at(surf);
      // Baked layers are chosen by the tile's saved version (Engine2's rule, kept so saved maps keep their art).
      int v3 = -1;
      for (size_t k = 0; k < data_.engine2V3.size(); ++k)
        if (data_.engine2V3[k].first == cell.type && data_.engine2V3[k].second == variant) {
          v3 = static_cast<int>(k);
          break;
        }
      const bool cityTile = cell.type == "plains" && (variant == data_.engine2V3CityVariant || (variant >= 21 && variant <= 38 && variant != 22));
      const int v3Layer = v3 >= 0 && data_.engine2V3Keys[v3] == "city" ? baseLayers : v3 >= 0 ? data_.baseLayerIndex(cell.type) : -1;
      const int variantLayer = data_.groundVariantLayer(cell.type, variant);
      const auto claude = data_.claudeVariant.find(cell.type);
      const bool claudeTile = claude != data_.claudeVariant.end() && variant == claude->second;
      if (surf == "void") cell.ground = -1;
      else if (((cell.type == "plains" && variant == 50) || (cell.type == "nave" && variant == 20)) && variantLayer >= 0)
        cell.ground = baseLayers + 3 + variantLayer;
      else if (cityTile) cell.ground = baseLayers;
      else if (v3 >= 0) cell.ground = v3Layer;
      else if (variantLayer >= 0) cell.ground = baseLayers + 3 + variantLayer;
      else cell.ground = claudeTile ? data_.baseLayerIndex(cell.type) : -1;
      // The map's own ground tiles (GroundAssets.h kEmberGroundTiles) on the hexes the procedural surfaces had: one for the
      // cell's terrain, else the set's tile for grass and forest.
      // A set's tile marked overBaked takes its terrain's hexes even where baked art lies (A Ponte de Pedra 023's deck over "ruins").
      if (surf != "void" && !draft_.emberTiles.empty()) {
        const std::string key = draft_.emberTiles + "|" + cell.type;
        if (const auto own = data_.emberGroundLayer.find(key); own != data_.emberGroundLayer.end() && data_.emberGroundOver.contains(key)) cell.ground = own->second;
      }
      if (cell.ground < 0 && surf != "void") {
        const std::string set = draft_.emberTiles.empty() ? draft_.id : draft_.emberTiles;
        if (const auto own = data_.emberGroundLayer.find(set + "|" + cell.type); own != data_.emberGroundLayer.end()) cell.ground = own->second;
        else if (surf == "grass" || surf == "forest")
          if (const auto any = data_.emberGroundLayer.find(set + "|"); any != data_.emberGroundLayer.end()) cell.ground = any->second;
      }
      // (baseLevels(): the draft's own base levels when it has them, MapDraft::emberBaseLevels.)
      if (info.surface == "void") cell.groundY = 0;
      else if (cell.water) cell.groundY = (baseLevels() - 1 + cell.level) * kStep;
      else cell.groundY = (baseLevels() + cell.level + info.raise) * kStep;
      cell.x = hexX(layout_, c, r);
      cell.z = hexZ(layout_, r);
      cell.waterY = cell.water ? (baseLevels() - 0.35 + cell.level) * kStep : 0;
      cells_.push_back(std::move(cell));
    }
  }
}

bool Board::hasWater() const {
  return std::any_of(cells_.begin(), cells_.end(), [](const Cell& c) { return c.water; });
}

bool Board::loadRelief(const std::filesystem::path& engine2Public) {
  const int first = static_cast<int>(data_.groundLayerTerrains.size()) + 3;
  for (const Cell& c : cells_) {
    const int index = c.ground - first;
    if (index < 0 || index >= static_cast<int>(data_.groundVariantLayers.size()) || data_.groundVariantLayers[index].relief <= 0) continue;
    if (relief_.contains(index)) continue;
    const std::string& url = data_.groundLayers[first + index].reliefGrid;
    const std::optional<json> j = readJson(engine2Public / std::filesystem::path(url.substr(url.starts_with('/') ? 1 : 0)));
    ReliefGrid grid;
    if (j) {
      grid.size = static_cast<int>(num(*j, "size"));
      if (j->contains("values")) grid.values = (*j)["values"].get<std::vector<double>>();
    }
    if (grid.size < 2 || grid.values.size() != static_cast<size_t>(grid.size) * grid.size) {
      logError("map: missing or invalid ground relief {}", url);
      return false;
    }
    relief_[index] = std::move(grid);
  }
  return true;
}

double Board::reliefAt(double x, double z) const {
  if (relief_.empty()) return 0;
  const int first = static_cast<int>(data_.groundLayerTerrains.size()) + 3;
  double result = 0;
  // Seven samples, matching the material blend, so relief never opens gaps between tile types.
  for (int k = 0; k < 7; k++) {
    const double a = (k - 1) * kPi / 3;
    const Cell& cell = cellAt(x + (k ? std::sin(a) * 0.3 : 0), z + (k ? std::cos(a) * 0.3 : 0));
    const auto it = relief_.find(cell.ground - first);
    if (it == relief_.end()) continue;
    const ReliefGrid& g = it->second;
    const double u = std::fmod(std::fmod(x / 4, 1) + 1, 1) * g.size, v = std::fmod(std::fmod(z / 4, 1) + 1, 1) * g.size;
    const int ix = static_cast<int>(std::floor(u)), iz = static_cast<int>(std::floor(v));
    const double fx = u - ix, fz = v - iz;
    const auto at = [&](int dx, int dz) { return g.values[((iz + dz) % g.size) * g.size + (ix + dx) % g.size] / 255; };
    const double sample = (at(0, 0) * (1 - fx) + at(1, 0) * fx) * (1 - fz) + (at(0, 1) * (1 - fx) + at(1, 1) * fx) * fz;
    result += sample * data_.groundVariantLayers[it->first].relief / 7;
  }
  return result;
}

}  // namespace ember::e2
