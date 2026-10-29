#pragma once

// An Ember map and its board, as Ember builds them (src/map/board.ts, ember/tileVariants.ts),
// ported to C++. Ember's game design is the reference: same cells, elevations, surfaces and ground
// layers, so every saved map keeps its look and its rules.
//
// The tables come from content/data/board.json, exported from Ember's own source
// (tools/scripts/data/export_board.ts); the rules that read them are here.

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "world/Water3D.h"
#include "world/Hex.h"

namespace ember {

struct TerrainInfo {
  std::string name;
  std::string surface;  // "grass", "forest", ..., "void"
  bool passable = true;
  double minLevel = 0;  // built-in minimum level (hill: 1)
  double raise = 0;     // extra levels the ground rises (column: 4)
};

struct GroundLayerSource {
  std::string key;
  bool absent = false;
  std::string procedural;           // "smooth-plains": generated, no file
  std::string color, normal, rough;  // URLs under Ember's public/ ("/game/..."); empty = none
  double relief = 0;                 // vertex relief height (world units) when reliefGrid is set
  std::string reliefGrid;
  std::string file;                  // EmberEngine's own tile (kEmberGroundTiles): a project file, in place of `color`
  float tile = 0;                    // world units per repeat; 0 = the board's usual 4
};

struct GroundVariantLayer {
  std::string key, terrain;
  int variant = 0;
  double relief = 0;
};

// Ember's board and ground tables.
struct BoardData {
  int baseLevels = 2;
  double mountainRock = 7, mountainSnow = 11;
  std::unordered_map<std::string, int> surfaces;  // grass 0 ... plains049 7
  std::unordered_map<std::string, TerrainInfo> terrain;
  std::vector<std::string> groundLayerTerrains;    // the 12 base terrain layers
  int groundLayerCount = 0;                         // base + city, shallow water, river rocks + variant layers + EmberEngine's tiles
  int classicLayerCount = 0;                        // the layers Ember knows (the first ones)
  std::vector<GroundLayerSource> groundLayers;      // groundLayerCount entries
  std::vector<GroundVariantLayer> groundVariantLayers;
  std::vector<std::pair<std::string, int>> classicV3;  // (terrain, variant), in table order; key "city" first match
  std::vector<std::string> classicV3Keys;
  int classicV3CityVariant = 50;
  std::unordered_map<std::string, int> claudeVariant;
  std::vector<std::string> architectureDecorations;  // sorted
  // "<set>|<terrain>" -> the layer of EmberEngine's own ground tile (appended after Ember's); terrain "" = its grass /
  // forest surfaces (GroundAssets.h kEmberGroundTiles)
  std::unordered_map<std::string, int> emberGroundLayer;
  std::unordered_set<std::string> emberGroundOver;  // the "set|terrain" keys whose tile also covers the terrain's baked art

  static std::optional<BoardData> load(const std::filesystem::path& boardJson);
  int groundVariantLayer(const std::string& terrain, int variant) const;
  int baseLayerIndex(const std::string& terrain) const;
  bool isArchitecture(const std::string& decorationId) const;
};

struct MapDecoration {
  std::string id;
  int x = 0, y = 0;
  double rot = 0;
  bool mirrorX = false, returnConnector = false;  // floor connectors: which way they lead
  std::string connectorDirection;                 // "up" / "down" / "" (not set)
  bool blocksPath = false;                        // the editor's "Bloquear caminho"
};

struct MapSpawn {
  std::string name, classId;
  int x = 0, y = 0;
  nlohmann::json dialog;  // an NPC's conversation (Ember Spawn.dialog: a DialogTree), null when it has none
};

// An Ember map file ({serial, savedAt, draft}); only the fields EmberEngine reads so far.
struct MapDraft {
  std::string id, title;
  int cols = 0, rows = 0;
  std::vector<std::string> tiles;
  std::vector<std::optional<int>> tileVariants;          // per cell; empty when the map has none
  std::vector<std::optional<double>> terrainElevations;  // per cell; empty when the map has none
  std::vector<MapDecoration> decorations;
  std::vector<MapSpawn> playerSpawns, enemySpawns, neutralSpawns;
  std::string environment, timeOfDay;
  std::string moonPhase;  // the Map Editor's Lua (ember::kMoonPhases name; "" = full): Modo teste's moon on this map
  bool fog = false;  // the mission hides what the party has not seen (Ember mission.fog)
  // The map editor's atmosphere (Ember Mission): its mist (Névoa type, intensity, speed), wisps and bloom.
  std::string mistType;
  float mistIntensity = 0.0f, mistSpeed = 1.0f;
  float wispIntensity = 0.0f, wispSpeed = 1.0f;
  unsigned wispColor = 0xffa552;
  float bloomIntensity = 0.0f;
  // EmberEngine's own: the Atmosfera climate of this map (a profile's name in content/atmosphere/v1/profiles.json, picked
  // in the Map Editor); empty = the profile whose map keys name this map.
  std::string atmosphereProfile;
  // EmberEngine's own: the set of EmberEngine ground tiles this map wears (GroundAssets.h kEmberGroundTiles); empty = the
  // set named by the map's id, if any.
  std::string emberTiles;
  // EmberEngine's own: a painting (content/backdrops) laid flat under the board as the ground far below it (A Ponte de Pedra 023:
  // the gorge under the bridge), `emberFloorDepth` under the board's lowest ground, `emberFloorTile` world units wide; "" = none.
  std::string emberFloor;
  float emberFloorDepth = 40.0f, emberFloorTile = 240.0f;
  // EmberEngine's own: the number of a fog look drawn on this map without a fog of war (render/FogLooks.h; 010 = the gorge's
  // wind fog outside the bridge walls); 0 = none.
  int emberWindFog = 0;
  // EmberEngine's own: the board's ground ends at the walls standing on its first and last rows (A Ponte de Pedra 023: the deck cut
  // along the parapets' planes and at their ends, MapStage deckClip).
  bool emberDeckClip = false;
  // EmberEngine's own: the board stands with no side walls under its edges (A Ponte de Pedra 026; the user, 2026-10-25: "a ponte não
  // deve ter laterais", the bridge suspended over the ground): BoardGround adds no closeBoardSides walls. Every other map: false.
  bool emberNoSides = false;
  // EmberEngine's own: an underground map ("Subterrâneo" in the Map Editor's Ambiente; the user, 2026-10-21): the only maps free of
  // the sun, the moon and the sky. Every other map takes the game's real clock (real sun, real moon).
  bool underground = false;
  // EmberEngine's own: an interior above the ground ("Interna acima do solo" in the Map Editor's Ambiente; the user, 2026-10-23: the
  // Torre de Vigia's settings saved as this kind): environment "indoor" + "aboveGround": true. The room is lit from outside only through
  // its windows and doors (Luz das janelas on by default, the light probes, the openings' lights), by the game's clock.
  bool aboveGround = false;
  // EmberEngine's own: this map's base levels in place of the board's BASE_LEVELS (2: a level-0 floor stands 2 units over the void's
  // ground, and the board's sides go down to it). 0 = the floor at the void's ground (the Torre de Vigia's gate floor, 2026-10-23, the
  // user: "encostar o tabuleiro da torre até a porta chegar no chão": the ground image lies at the board's base, so the gate's threshold
  // touches it and no side wall stands under the door). Absent = the board's own.
  std::optional<int> emberBaseLevels;

  // The map's backdrop pick (Ember draft.backgroundImage: absent = the map's own, "" = none) and its lighting
  // sliders as authored (absent = the time of day's default).
  std::optional<std::string> backgroundImage;
  // The map's top-down Ground image around the board: absent = the selected Panorama's Ground, "" = none, otherwise
  // an image under content/backdrops. This is separate from per-hex tile variants and from emberFloor below the board.
  std::optional<std::string> groundImage;
  std::optional<double> sunIntensity, ambientIntensity, bloomSlider;
  // Ember's 3D water (the Map Editor's Água 3D: waterLevels, waterFootprints, waterPatches, waterVersion).
  Water3DMap water3d;

  static std::optional<MapDraft> load(const std::filesystem::path& mapJson);
  // The same from a draft already in memory (Ember's MapDraft JSON: the map editor's working copy).
  static std::optional<MapDraft> fromJson(const nlohmann::json& draft);
  int variant(int i) const { return i < static_cast<int>(tileVariants.size()) && tileVariants[i] ? *tileVariants[i] : 0; }
  bool hasVariant(int i) const { return i < static_cast<int>(tileVariants.size()) && tileVariants[i].has_value(); }
};

struct Cell {
  int c = 0, r = 0, i = 0;
  double x = 0, z = 0;
  std::string type;
  double level = 0;   // gameplay elevation (Ember's rule)
  int surface = -1;   // surface index for the ground material, -1 = void
  int ground = -1;    // baked ground layer, -1 = the procedural surface only
  double groundY = 0; // world height of the ground's top
  bool water = false;
  double waterY = 0;  // water cells: world height of the surface
};

// Height grid of a photographic ground layer's real relief (ground-variants-3d/<key>/relief.json).
struct ReliefGrid {
  int size = 0;
  std::vector<double> values;  // 0..255
};

class Board {
 public:
  Board(const BoardData& data, MapDraft draft);

  const BoardData& data() const { return data_; }
  const MapDraft& draft() const { return draft_; }
  // The base levels the ground's heights stand on: the draft's own (MapDraft::emberBaseLevels), else 0 on every map (the user,
  // 2026-10-29: "nenhum tabuleiro alto em nenhum mapa do jogo", "todos no ground level o mais baixo possível", the maps' own
  // elevations kept above it); the Inn's floors keep the board's BASE_LEVELS (2) as they were (locked without the user's word).
  int baseLevels() const {
    if (draft_.emberBaseLevels) return *draft_.emberBaseLevels;
    return draft_.id.rfind("estalagem", 0) == 0 ? data_.baseLevels : 0;
  }
  const HexLayout& layout() const { return layout_; }
  const std::vector<Cell>& cells() const { return cells_; }
  const Cell& cell(int c, int r) const { return cells_[indexOf(layout_, c, r)]; }
  const Cell& cellAt(double x, double z) const {
    const auto [c, r] = hexAt(layout_, x, z);
    return cell(c, r);
  }
  bool hasWater() const;

  struct Bounds {
    double x0, x1, z0, z1;
  };
  Bounds bounds() const { return {-layout_.ox, layout_.ox, -layout_.oz - 0.6, layout_.oz + 0.6}; }

  // Relief grids for the photographic layers this map uses (Ember: prepareTerrainRelief), read from
  // Ember's public/ folder. False when a needed grid is missing or damaged.
  bool loadRelief(const std::filesystem::path& artRoot);
  // Ember's terrainReliefAt: real relief of the photographic layers under a point.
  double reliefAt(double x, double z) const;

 private:
  const BoardData& data_;
  MapDraft draft_;
  HexLayout layout_;
  std::vector<Cell> cells_;
  std::map<int, ReliefGrid> relief_;  // variant layer index -> grid
};

}  // namespace ember
