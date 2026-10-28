#include "world/BattleExport.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <deque>
#include <fstream>
#include <initializer_list>
#include <set>

#include "core/Log.h"
#include "world/Board.h"
#include "world/Loadout.h"

namespace ember {

int classLevelCap(const std::string& classId);  // world/Spells.h (the Cruzado stops at 10)
int heroLevelCap(const std::string& name, const std::string& classId);  // world/Spells.h (Selene stops at 10)

namespace {

using json = nlohmann::json;

// The weapons' table the fight's loadouts read (content/data/weapons.json): a hero with arms of her own wields an off-hand piece
const Loadouts& sharedLoadouts() {
  static const Loadouts arms = [] {
    Loadouts l;
    l.load("content/data/weapons.json");
    return l;
  }();
  return arms;
}

struct Hex {
  int x = 0, y = 0;
  bool operator==(const Hex&) const = default;
  auto operator<=>(const Hex&) const = default;
};

// pathfinding.ts hexNeighbors (its order matters: searches take the first fit)
std::vector<Hex> neighbors(int x, int y) {
  static constexpr int even[6][2] = {{1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}};
  static constexpr int odd[6][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {0, 1}, {1, 1}};
  const auto& d = (y & 1) ? odd : even;
  std::vector<Hex> out;
  out.reserve(6);
  for (const auto& o : d) out.push_back({x + o[0], y + o[1]});
  return out;
}

int hexDist(Hex a, Hex b) {
  const int aq = a.x - (a.y - (a.y & 1)) / 2, bq = b.x - (b.y - (b.y & 1)) / 2;
  const int dq = aq - bq, dr = a.y - b.y;
  return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

// JS Math.round for the positive numbers used here
int jsRound(double v) { return static_cast<int>(std::floor(v + 0.5)); }

std::string str(const json& j, const char* key, const std::string& fallback = {}) {
  return j.is_object() && j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : fallback;
}
int integer(const json& j, const char* key, int fallback = 0) {
  return j.is_object() && j.contains(key) && j[key].is_number() ? static_cast<int>(j[key].get<double>()) : fallback;
}
double number(const json& j, const char* key, double fallback = 0) {
  return j.is_object() && j.contains(key) && j[key].is_number() ? j[key].get<double>() : fallback;
}
bool flag(const json& j, const char* key) { return j.is_object() && j.contains(key) && j[key].is_boolean() && j[key].get<bool>(); }
bool has(const json& j, const char* key) { return j.is_object() && j.contains(key) && !j[key].is_null(); }

// --- the rule tables (content/data/battle-rules.json) --------------------------------------------------------------

class Rules {
 public:
  static const Rules* shared() {
    static const std::optional<Rules> rules = []() -> std::optional<Rules> {
      std::ifstream in("content/data/battle-rules.json", std::ios::binary);
      json j = in ? json::parse(in, nullptr, false) : json();
      if (!in || j.is_discarded() || !j.contains("classes") || !j.contains("terrain") || !j.contains("decorations")) {
        logError("battle export: cannot read content/data/battle-rules.json (tools/scripts/data/export_battle_rules.ts)");
        return std::nullopt;
      }
      Rules r;
      r.j_ = std::move(j);
      for (const auto& [name, ids] : r.j_["decorSets"].items())
        for (const json& id : ids) r.sets_[name].insert(id.get<std::string>());
      r.maxLevel_ = integer(r.j_, "maxLevel", 30);
      // EmberEngine's own hero classes (content/ember-classes.json heroClasses; the Cruzado, 2026-10-25): the same fields as the
      // table's, and every melee weapon of the types it trains usable by it (no shield, no other gear)
      std::ifstream own("content/ember-classes.json", std::ios::binary);
      const json ember = own ? json::parse(own, nullptr, false) : json();
      const auto listed = [](const json& list, const std::string& v) {
        return list.is_array() && std::any_of(list.begin(), list.end(), [&](const json& x) { return x.is_string() && x.get<std::string>() == v; });
      };
      if (ember.is_object() && ember.contains("heroClasses") && ember["heroClasses"].is_object())
        for (const auto& [id, def] : ember["heroClasses"].items()) {
          r.j_["classes"][id] = def;
          for (auto& [wid, w] : r.j_["weapons"].items())
            if (w.is_object() && !flag(w, "ranged") && listed(def["weaponTypes"], str(w, "weaponType")) && w["usableBy"].is_array() && !listed(w["usableBy"], id))
              w["usableBy"].push_back(id);
        }
      // EmberEngine's own heroes (content/ember-classes.json heroes; Isolde, 2026-10-25: "the full party at debug mode should include
      // her, as a stress test too because the goal is a 8 members party"): added after the table's six, so every prepared map seats
      // them; the campaign keeps only the heroes its save recruited (main.cpp setUpParty), Modo teste seats everyone
      if (ember.is_object() && ember.contains("heroes") && ember["heroes"].is_array())
        for (const json& h : ember["heroes"]) {
          const std::string name = str(h, "name"), classId = str(h, "classId");
          if (name.empty() || classId.empty()) continue;
          if (!listed(r.j_["campaignHeroOrder"], name)) r.j_["campaignHeroOrder"].push_back(name);
          r.j_["campaignHeroClasses"][name] = classId;
        }
      return r;
    }();
    return rules ? &*rules : nullptr;
  }
  const json& all() const { return j_; }
  int maxLevel() const { return maxLevel_; }
  const json& terrain(const std::string& id) const {
    const json& t = j_["terrain"];
    return t.contains(id) ? t[id] : t["plains"];
  }
  bool knownTerrain(const std::string& id) const { return j_["terrain"].contains(id); }
  const json* cls(const std::string& id) const { return find(j_["classes"], id); }
  const json* decoration(const std::string& id) const { return find(j_["decorations"], id); }
  const json* weapon(const std::string& id) const { return find(j_["weapons"], id); }
  const json* equipment(const std::string& id) const { return find(j_["equipment"], id); }
  bool inSet(const char* set, const std::string& id) const {
    const auto it = sets_.find(set);
    return it != sets_.end() && it->second.contains(id);
  }
  // a per-level row (level 1 = row 0), clamped to the table
  template <typename T>
  const T& row(const T& table, int level) const {
    const int n = static_cast<int>(table.size());
    return table[static_cast<size_t>(std::clamp(level, 1, std::max(1, n)) - 1)];
  }

 private:
  static const json* find(const json& table, const std::string& id) {
    const auto it = table.find(id);
    return it != table.end() ? &*it : nullptr;
  }
  json j_;
  std::map<std::string, std::set<std::string>> sets_;
  int maxLevel_ = 30;
};

// --- footprints (data.ts rotateFootprint / placedFootprint / placedBlockingFootprint) ---------------------------------

std::vector<Hex> rotateFootprint(const json& footprint, int ax, int ay, double rot) {
  const int steps = ((static_cast<int>(std::lround(rot)) % 6) + 6) % 6;
  std::vector<Hex> out;
  for (const json& f : footprint) out.push_back({integer(f, "dx"), integer(f, "dy")});
  if (steps == 0) return out;
  const auto parity = [](int row) { return ((row % 2) + 2) % 2; };
  struct Cube { int x, y, z; };
  const auto toCube = [&](int col, int row) { const int x = col - (row - parity(row)) / 2; return Cube{x, -x - row, row}; };
  const Cube anchor = toCube(ax, ay);
  for (Hex& h : out) {
    const Cube cell = toCube(ax + h.x, ay + h.y);
    int x = cell.x - anchor.x, y = cell.y - anchor.y, z = cell.z - anchor.z;
    for (int i = 0; i < steps; ++i) {
      const int nx = -z, ny = -x, nz = -y;
      x = nx, y = ny, z = nz;
    }
    const int bx = x + anchor.x, bz = z + anchor.z;
    h = {bx + (bz - parity(bz)) / 2 - ax, bz - ay};
  }
  return out;
}

std::vector<Hex> quarterTurned(const json& footprint, double rot) {
  const int turn = ((static_cast<int>(rot) % 4) + 4) % 4;
  std::vector<Hex> out;
  for (const json& f : footprint) {
    const int dx = integer(f, "dx"), dy = integer(f, "dy");
    out.push_back(turn == 1 ? Hex{-dy, dx} : turn == 2 ? Hex{-dx, -dy} : turn == 3 ? Hex{dy, -dx} : Hex{dx, dy});
  }
  return out;
}

std::vector<Hex> placedFootprint(const Rules& r, const json& p) {
  const json* def = r.decoration(str(p, "id"));
  if (!def) return {};
  const double rot = number(p, "rot");
  if (integer(*def, "architectureSpan") > 0) return quarterTurned((*def)["footprint"], rot);
  return rotateFootprint((*def)["footprint"], integer(p, "x"), integer(p, "y"), rot);
}

std::vector<Hex> placedBlockingFootprint(const Rules& r, const json& p) {
  const json* def = r.decoration(str(p, "id"));
  if (!def) return {};
  const double rot = number(p, "rot");
  const json& blocking = (*def)["blockingFootprint"];
  if (integer(*def, "architectureSpan") > 0) {
    std::vector<Hex> cells = placedFootprint(r, p);
    if (!blocking.is_array()) return cells;
    const std::vector<Hex> keep = quarterTurned(blocking, rot);
    std::erase_if(cells, [&](const Hex& c) { return std::find(keep.begin(), keep.end(), c) == keep.end(); });
    return cells;
  }
  return rotateFootprint(blocking.is_array() ? blocking : (*def)["footprint"], integer(p, "x"), integer(p, "y"), rot);
}

// --- the board as the rules see it (hexprops.ts) ----------------------------------------------------------------------

constexpr int kBlocked = 1, kHigh = 2, kOpenShot = 4;

struct Def {
  bool passable = true;
  int moveCost = 1, def = 0, atk = 0, height = 0, cover = 0, hazardDice = 0, hazardFaces = 8;
  bool blocksShot = false;
};

Def baseDef(const json& t) {
  Def d;
  d.passable = flag(t, "passable");
  d.moveCost = integer(t, "moveCost", 1), d.def = integer(t, "def"), d.atk = integer(t, "atk");
  d.height = integer(t, "height"), d.cover = integer(t, "cover");
  d.hazardDice = integer(t, "hazardDice"), d.hazardFaces = integer(t, "hazardFaces", 8);
  d.blocksShot = flag(t, "blocksShot");
  return d;
}

Def derive(const Def& base, int bits) {
  Def out = base;
  if (bits & kBlocked) {
    out.passable = false;
    out.moveCost = 99;
    out.blocksShot = (bits & kOpenShot) ? base.blocksShot : true;
  }
  if (bits & kHigh) {
    out.height = 1;
    out.atk = std::max(base.atk, 2);
    out.def = std::max(base.def, 1);
    if (out.passable) out.moveCost = std::max(base.moveCost, 2);
  }
  return out;
}

struct Field {
  const Rules& r;
  int cols = 0, rows = 0;
  std::vector<std::string> tiles;
  std::vector<json> decorations;
  std::vector<std::optional<double>> elevations;
  std::vector<uint8_t> overlay;

  bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < cols && y < rows; }
  const std::string& tileAt(int x, int y) const { return tiles[static_cast<size_t>(y * cols + x)]; }
  Def hexDef(int x, int y) const {
    const Def base = baseDef(r.terrain(tileAt(x, y)));
    const int bits = overlay.empty() ? 0 : overlay[static_cast<size_t>(y * cols + x)];
    return bits == 0 ? base : derive(base, bits);
  }
  // hexprops.ts buildDecorOverlay
  void refreshOverlay() {
    overlay.assign(static_cast<size_t>(cols * rows), 0);
    for (size_t i = 0; i < overlay.size() && i < elevations.size(); ++i)
      if (elevations[i] && std::isfinite(*elevations[i]) && *elevations[i] > 0) overlay[i] |= kHigh;
    std::vector<size_t> solid;
    for (const json& p : decorations) {
      const std::string id = str(p, "id");
      const json* def = r.decoration(id);
      const bool house = r.inSet("house", id) || r.inSet("bigHouse", id) || r.inSet("solidHouse", id);
      const bool chest = r.inSet("chest", id), barricade = r.inSet("barricadeLike", id), cart = r.inSet("solidCart", id), rock = r.inSet("solidRock", id);
      const std::string arch = def ? str(*def, "model3d") : "";
      const bool solidArch = arch == "wall" || arch == "door" || arch == "secretDoor" ||
                             (arch == "doorway" && def && integer(*def, "architectureSpan") > 0 && (*def)["blockingFootprint"].is_array());
      // The user (2026-10-25: "NÃO QUERO DECORAÇÃO BLOQUEANDO MAIS NADA ... a única coisa que bloqueia é barricada"): a decoration
      // blocks a hex only when it is a barricade (its id says so, or the rules' barricadeLike set); houses, chests, carts, rocks and
      // the editor's blocksPath mark block nothing. Architecture stays solid (walls, doors, secret doors, a closed doorway span: a
      // room keeps its walls). The other sets are read for the shot rules only.
      const bool barricadeId = id.find("barricade") != std::string::npos;
      (void)house, (void)chest, (void)cart, (void)rock;
      const bool lowProp = false;
      const int bits = (solidArch || barricade || barricadeId ? kBlocked : 0) | (flag(p, "yieldsHighGround") ? kHigh : 0) | (lowProp ? kOpenShot : 0);
      if (!bits) continue;
      for (const Hex& c : placedBlockingFootprint(r, p)) {
        const int x = integer(p, "x") + c.x, y = integer(p, "y") + c.y;
        if (!inBounds(x, y)) continue;
        overlay[static_cast<size_t>(y * cols + x)] |= static_cast<uint8_t>(bits);
        if ((bits & kBlocked) && !lowProp) solid.push_back(static_cast<size_t>(y * cols + x));
      }
    }
    for (const size_t i : solid) overlay[i] &= static_cast<uint8_t>(~kOpenShot);
  }
  // export_battle.ts cellRecord
  json cellRecord(int x, int y) const {
    const Def d = hexDef(x, y);
    const int bits = overlay.empty() ? 0 : overlay[static_cast<size_t>(y * cols + x)];
    const Def raw = baseDef(r.terrain(tileAt(x, y)));
    return {{"terrain", tileAt(x, y)}, {"passable", d.passable}, {"moveCost", d.moveCost}, {"def", d.def}, {"atk", d.atk},
            {"blocked", (bits & kBlocked) != 0}, {"cover", d.cover}, {"hazardDice", d.hazardDice}, {"hazardFaces", d.hazardFaces},
            {"raw", {{"def", raw.def}, {"atk", raw.atk}, {"height", raw.height}, {"cover", raw.cover}, {"blocksShot", raw.blocksShot}}},
            {"shot", {{"height", d.height}, {"blocksShot", d.blocksShot}}}};
  }
};

// data.ts clearRockColumnTiles: legacy column terrain under solid rocks becomes ordinary ground
void clearRockColumnTiles(const Rules& r, std::vector<std::string>& tiles, int cols, int rows, const std::vector<json>& decorations, const std::string& baseTile) {
  std::string ground;
  if (!baseTile.empty() && r.knownTerrain(baseTile) && flag(r.terrain(baseTile), "passable") && baseTile != "water" && baseTile != "void") ground = baseTile;
  else if (std::find(tiles.begin(), tiles.end(), "nave") != tiles.end()) ground = "nave";
  else if (std::find(tiles.begin(), tiles.end(), "snow") != tiles.end()) ground = "snow";
  else ground = "plains";
  for (const json& p : decorations) {
    const std::string id = str(p, "id");
    const json* def = r.decoration(id);
    if (!r.inSet("solidRock", id) && !(def && has(*def, "rockStyle") && str(*def, "model3d") == "wall")) continue;
    for (const Hex& c : placedFootprint(r, p)) {
      const int x = integer(p, "x") + c.x, y = integer(p, "y") + c.y;
      if (x < 0 || y < 0 || x >= cols || y >= rows) continue;
      std::string& t = tiles[static_cast<size_t>(y * cols + x)];
      if (t == "column") t = ground;
    }
  }
}

// data.ts floorConnectorDirection
std::string connectorDirection(const json& p) {
  if (has(p, "connectorDirection") && p["connectorDirection"].is_string()) return p["connectorDirection"].get<std::string>();
  const std::string id = str(p, "id");
  if (id == "stone-stairs-up-001") return "up";
  if (id == "stone-stairs-down-001") return "down";
  return flag(p, "returnConnector") ? "up" : "down";
}

// watchtowerDungeon.ts closeWatchtowerWalls (the six watchtower floors only)
std::vector<json> closeWatchtowerWalls(const Rules& r, const std::string& id, const std::vector<std::string>& tiles, int cols, int rows, std::vector<json> placements) {
  if (!id.starts_with("watchtower-")) return placements;
  const json& floors = r.all()["watchtowerFloors"];
  const auto floor = [&](int x, int y) { return x >= 0 && y >= 0 && x < cols && y < rows && tiles[static_cast<size_t>(y * cols + x)] != "void"; };
  const auto boundary = [&](int x, int y) { return floor(x, y) && (!floor(x - 1, y) || !floor(x + 1, y) || !floor(x, y - 1) || !floor(x, y + 1)); };
  const auto model3d = [&](const json& p) { const json* d = r.decoration(str(p, "id")); return d ? str(*d, "model3d") : std::string(); };
  const auto exitKind = [&](const json& p) { const json* d = r.decoration(str(p, "id")); return d ? str(*d, "exitKind") : std::string(); };
  std::vector<json> result;
  for (const json& p0 : placements) {
    if (flag(p0, "waypointStairs")) continue;
    const std::string kind = exitKind(p0);
    if (!(id == "watchtower-gate-floor" || kind.empty() || kind == "connector")) continue;
    json p = p0;
    if (!(has(p, "connectorDirection") && p["connectorDirection"].is_string())) {
      const std::string target = str(p, "targetMapId");
      if (str(p, "id") == "floor-connector" && !target.empty() && floors.contains(target) && floors.contains(id))
        p["connectorDirection"] = floors[target].get<int>() > floors[id].get<int>() ? "up" : "down";
    }
    if (str(p, "id") == "city-stone-banner-wall") p["id"] = "wall-3d-dungeon";
    result.push_back(std::move(p));
  }
  // one wall per cell: the first stays
  {
    std::vector<json> unique;
    for (const json& p : result) {
      if (model3d(p) == "wall") {
        const auto first = std::find_if(result.begin(), result.end(), [&](const json& a) { return model3d(a) == "wall" && integer(a, "x") == integer(p, "x") && integer(a, "y") == integer(p, "y"); });
        if (&*first != &p) continue;
      }
      unique.push_back(p);
    }
    result = std::move(unique);
  }
  const auto cellsOf = [&](const json& p) {
    std::vector<Hex> out;
    for (const Hex& c : placedFootprint(r, p)) out.push_back({integer(p, "x") + c.x, integer(p, "y") + c.y});
    return out;
  };
  std::set<Hex> entrance;
  for (const json& p : result)
    if (str(p, "id") == "watchtower-stone-open-door-2hex")
      for (const Hex& c : cellsOf(p)) entrance.insert(c);
  // removeWallsUnderWatchtowerEntrances
  std::erase_if(result, [&](const json& p) {
    if (model3d(p) != "wall") return false;
    for (const Hex& c : cellsOf(p))
      if (entrance.contains(c)) return true;
    return false;
  });
  std::set<Hex> occupied = entrance;
  for (const json& p : result)
    if (!model3d(p).empty()) occupied.insert({integer(p, "x"), integer(p, "y")});
  std::string wallId = "wall-3d-dungeon";
  for (const json& p : result)
    if (model3d(p) == "wall") { wallId = str(p, "id"); break; }
  for (int y = 0; y < rows; ++y)
    for (int x = 0; x < cols; ++x)
      if (boundary(x, y) && !occupied.contains({x, y})) {
        result.push_back({{"id", wallId}, {"x", x}, {"y", y}});
        occupied.insert({x, y});
      }
  const auto degree = [&](int x, int y) {
    int n = 0;
    for (const Hex& c : {Hex{x - 1, y}, Hex{x + 1, y}, Hex{x, y - 1}, Hex{x, y + 1}}) n += occupied.contains(c) ? 1 : 0;
    return n;
  };
  std::vector<Hex> ends;
  for (const json& p : result)
    if (model3d(p) == "wall" && degree(integer(p, "x"), integer(p, "y")) < 2) ends.push_back({integer(p, "x"), integer(p, "y")});
  for (const Hex& a : ends)
    for (const Hex& b : ends) {
      if (std::abs(a.x - b.x) != 1 || std::abs(a.y - b.y) != 1) continue;
      for (const Hex& j : {Hex{a.x, b.y}, Hex{b.x, a.y}})
        if (floor(j.x, j.y) && !occupied.contains(j)) {
          result.push_back({{"id", wallId}, {"x", j.x}, {"y", j.y}});
          occupied.insert(j);
          break;
        }
    }
  for (json& p : result)
    if (exitKind(p) == "connector") {
      const std::string dir = connectorDirection(p);
      p["id"] = dir == "up" ? "stone-stairs-up-001" : "stone-stairs-down-001";
      p.erase("waypointStairs");
      p["connectorDirection"] = dir;
    }
  return result;
}

// --- the units (engine.ts spawnUnit, no roster: a fresh party, enemies at the mission's level) -------------------------

struct Unit {
  std::string id, side, name, classId, className, sprite;
  int x = 0, y = 0, level = 1;
  bool alive = true, holdsPosition = false, summoned = false, guaranteedDrop = false, undead = false, boss = false;
  int hp = 0, maxHp = 0, atk = 0, mag = 0, def = 0, dex = 0, mov = 0, minRange = 1, maxRange = 1, size = 1;
  int initiative = 0, shockCharges = 0, frostCharges = 0, fantomForceCharges = 0;
  std::array<int, 10> spells{};
  std::string weaponId;  // empty = none
  std::string ownWeapon;  // a hero with arms of her own (heroArms; Selene): her main hand's piece (an off-hand dagger), weaponId empty
  json footprintOffsets = nullptr, footprintW = nullptr, footprintH = nullptr;
  json resistances = json::object(), bag = json::object();
  std::vector<std::string> weaponTypes;  // the class's pool (weaponTypesForClass), players only
  int facing = 1;
};

const char* kElements[] = {"fire", "lightning", "ice", "arcane", "darkness", "holy", "poison", "ember"};

json sumResistances(const std::vector<const json*>& sources) {
  json out = json::object();
  for (const char* e : kElements) {
    double sum = 0;
    for (const json* s : sources)
      if (s && s->is_object() && s->contains(e) && (*s)[e].is_number()) sum += (*s)[e].get<double>();
    out[e] = sum;
  }
  return out;
}

bool contains(const json& list, const std::string& v) {
  if (!list.is_array()) return false;
  for (const json& x : list)
    if (x.is_string() && x.get<std::string>() == v) return true;
  return false;
}

bool oneOf(const std::string& v, std::initializer_list<const char*> options) {
  for (const char* o : options)
    if (v == o) return true;
  return false;
}

bool isBirolho(const std::string& c) { return oneOf(c, {"birolho", "birolho2", "birolho3", "birolhoLegs", "birolhoLegs2"}); }

struct Spawner {
  const Rules& r;
  const json& classes = r.all()["classes"];
  const json& pools = r.all()["enemyPools"];

  int tierUses(const std::string& classId, int tier, int level) const {
    const json* c = r.cls(classId);
    if (!c) return 0;
    const json& row = r.row((*c)["tiers"], level);
    return tier >= 1 && tier <= static_cast<int>(row.size()) ? row[static_cast<size_t>(tier - 1)].get<int>() : 0;
  }
  const json& spellList(const std::string& classId, int level, const std::string& name) const {
    static const json none = json::array();
    const json* c = r.cls(classId);
    if (!c) return none;
    if (name == "Salazar" && c->contains("spellsSalazar")) return r.row((*c)["spellsSalazar"], level);
    return r.row((*c)["spells"], level);
  }
  // engine.ts isArrowAttack / isArcaneCaster
  bool isArrowAttack(const Unit& u) const {
    if (oneOf(u.classId, {"pikeman", "basePikeman", "lancer", "aldric", "sandoval", "sentinel", "templar"})) return false;
    if (oneOf(u.classId, {"brigand", "baseCrossbowman", "brigandCrossbowman"})) return true;
    if (!u.weaponId.empty()) { const json* w = r.weapon(u.weaponId); return w && flag(*w, "ranged"); }
    return oneOf(u.classId, {"archer", "ranger", "assassin"});
  }
  static bool isArcaneCaster(const std::string& c) {
    return oneOf(c, {"mage", "voss", "elementalist", "warlock", "cultist", "cultistV2"}) || isBirolho(c);
  }
  // Isolde (ours, the user 2026-10-24): a conjurer whose plain attack and counter work like a mage's ("its counter and ATT like a
  // mage"): the bolt at range, the staff beside her (MapStage); her class, reach and spells stay the conjurer's.
  static bool isArcaneCaster(const Unit& u) { return isArcaneCaster(u.classId) || u.name == "Isolde"; }
  // weaponSkills.ts equippedWeaponType (the main hand)
  std::string weaponType(const Unit& u) const {
    if (u.weaponId.empty()) return "";
    const json* w = r.weapon(u.weaponId);
    return w && contains((*w)["usableBy"], u.classId) ? str(*w, "weaponType") : "";
  }
  // weaponSkills.ts weaponTypesForClass includes the type (any side)
  bool trains(const Unit& u, const std::string& type) const {
    const json* c = r.cls(u.classId);
    return !type.empty() && c && contains((*c)["weaponTypes"], type);
  }
  json mastery(const Unit& u, const std::string& type) const {
    if (u.side == "enemy") return {{"accuracy", 90 + u.dex}, {"damage", 1}};
    if (!trains(u, type)) return {{"accuracy", 100}, {"damage", 1}};
    return {{"accuracy", number(r.all()["weaponSkill"], "accuracy", 75)}, {"damage", number(r.all()["weaponSkill"], "damage", 1)}};
  }
  // the staff's own magic (engine.ts staffMagic)
  const json* staffMagic(const Unit& u) const {
    if (u.weaponId.empty()) return nullptr;
    const json* w = r.weapon(u.weaponId);
    return w && contains((*w)["usableBy"], u.classId) && has(*w, "magic") ? &(*w)["magic"] : nullptr;
  }

  std::optional<Unit> spawn(const json& s, const std::string& side, int i, int enemyLevel, std::string* error) const {
    Unit u;
    u.name = str(s, "name");
    u.classId = u.name == "Kael" ? "swordsman" : str(s, "classId");
    const json* cls = r.cls(u.classId);
    if (!cls) {
      if (error) *error = "unknown " + side + " class \"" + u.classId + "\" for \"" + u.name + "\" at " + std::to_string(integer(s, "x")) + "," + std::to_string(integer(s, "y"));
      return std::nullopt;
    }
    const json& notorious = r.all()["notoriousLevelBonus"];
    u.level = side == "player" ? std::min(enemyLevel, heroLevelCap(u.name, u.classId)) : enemyLevel + (side == "enemy" && notorious.contains(u.name) ? notorious[u.name].get<int>() : 0);
    const json& st = r.row((*cls)["stats"], u.level);
    int hp = st[0].get<int>(), atk = st[1].get<int>(), mag = st[2].get<int>(), def = st[3].get<int>(), dex = st[4].get<int>();
    if (side == "enemy") {
      const double boost = 1 + std::floor(u.level / 5.0) * 0.1;
      hp = jsRound(hp * boost), atk = jsRound(atk * boost), mag = jsRound(mag * boost), def = jsRound(def * boost), dex = jsRound(dex * boost);
    }
    // the fresh party's gear: the class's starter weapon, no off hand, no worn pieces
    if (side == "player" && has(*cls, "starterWeapon")) u.weaponId = str(*cls, "starterWeapon");
    const json* weapon = u.weaponId.empty() ? nullptr : r.weapon(u.weaponId);
    if (weapon && !contains((*weapon)["usableBy"], u.classId)) { u.weaponId = str(*cls, "starterWeapon"); weapon = u.weaponId.empty() ? nullptr : r.weapon(u.weaponId); }
    // A hero with arms of her own (content/hero-arms.json; Selene): her starter dagger in the main hand, never her class's staff
    const HeroArms* ownArms = side == "player" ? heroArms(u.name) : nullptr;
    const OffHandDef* ownPiece = ownArms ? sharedLoadouts().offHand(ownArms->starter) : nullptr;
    if (ownArms) u.weaponId.clear(), u.ownWeapon = ownArms->starter, weapon = nullptr;
    const json* magic = weapon && has(*weapon, "magic") ? &(*weapon)["magic"] : nullptr;  // gearStatBonus: the weapon's own magic
    u.minRange = ownPiece ? ownPiece->minRange : weapon ? integer(*weapon, "minRange", 1) : integer(*cls, "minRange", 1);
    u.maxRange = ownPiece ? ownPiece->maxRange
                          : (weapon ? integer(*weapon, "maxRange", 1) : integer(*cls, "maxRange", 1)) + (contains(r.all()["mageRangeBonusClasses"], u.classId) ? 1 : 0);
    u.maxHp = u.hp = hp;
    u.atk = atk;
    u.mag = mag + (magic ? integer(*magic, "magBonus") : 0);
    u.def = def;
    u.dex = dex + (magic ? integer(*magic, "dexBonus") : 0);
    const json* magicRes = magic && has(*magic, "resistances") ? &(*magic)["resistances"] : nullptr;
    u.resistances = sumResistances({&(*cls)["resistances"], magicRes});
    if (ownArms) u.weaponTypes = ownArms->weaponTypes;
    else if (side == "player") for (const json& t : (*cls)["weaponTypes"]) u.weaponTypes.push_back(t.get<std::string>());
    u.initiative = integer(*cls, "initiativeBonus");
    u.holdsPosition = flag(s, "holdsPosition");
    u.mov = u.holdsPosition ? 0 : integer(*cls, "mov");
    u.side = side;
    u.id = side + "-" + u.name + "-" + std::to_string(i);
    u.className = str(*cls, "name");
    const json& heroSprites = r.all()["heroSprites"];
    u.sprite = !flag(s, "useClassSprite") && heroSprites.contains(u.name) ? heroSprites[u.name].get<std::string>() : str(*cls, "sprite");
    u.x = integer(s, "x"), u.y = integer(s, "y");
    u.facing = side == "player" ? 1 : -1;
    u.bag = side == "player" ? (u.classId == "healer" ? r.all()["emptyBag"] : r.all()["startingBag"]) : r.all()["emptyBag"];
    // spell uses per tier (engine.ts spawnUnit `spells`: the enemy-only pools, else the class's own for the party)
    const std::string& c = u.classId;
    const auto own = [&](int tier) { return side == "player" ? tierUses(c, tier, u.level) : 0; };
    const json& cultist = r.row(pools["cultist"], u.level);
    const json& brigand = r.row(pools["brigand"], u.level);
    const json& birolho = r.row(pools["birolho"], u.level);
    u.spells[0] = c == "carnivorousPlant" ? 2 : c == "sapling" ? 1 : c == "swampBlueCalf" ? tierUses(c, 1, u.level) : c == "bigBlueCalf" ? 3 : c == "roccoTheBird" ? 2
                : c == "emberedWraith" ? 0 : (c == "cultist" || c == "cultistV2") ? cultist[0].get<int>() : c == "baseCrossbowman" ? 2 : c == "brigand" ? brigand[0].get<int>()
                : isBirolho(c) ? birolho[0].get<int>() : own(1);
    u.spells[1] = c == "roccoTheBird" ? 3 : c == "emberedWraith" ? 1 : c == "stagWolf" ? 2 : (c == "cultist" || c == "cultistV2") ? cultist[1].get<int>()
                : c == "baseCrossbowman" ? 1 : c == "brigandCrossbowman" ? 2 : c == "brigand" ? brigand[1].get<int>() : isBirolho(c) ? birolho[2].get<int>() : own(2);
    u.spells[2] = c == "carnivorousPlant" ? 2 : c == "brigandCrossbowman" ? 1 : own(3);
    u.spells[3] = c == "carnivorousPlant" ? 3 : isBirolho(c) ? birolho[1].get<int>() : (c == "undeadOx" || c == "plagueBearingCattle") ? 2 : own(4);
    for (int t = 5; t <= 10; ++t) u.spells[static_cast<size_t>(t - 1)] = own(t);
    u.size = integer(*cls, "size", 1);
    u.footprintW = (*cls)["footprintW"], u.footprintH = (*cls)["footprintH"], u.footprintOffsets = (*cls)["footprintOffsets"];
    u.shockCharges = side == "enemy" ? integer(*cls, "shockCharges") : 0;
    u.frostCharges = side == "enemy" && c == "cultistV2" ? r.row(pools["frost"], u.level).get<int>() : 0;
    u.fantomForceCharges = side == "enemy" ? integer(*cls, "fantomForceCharges") : 0;
    u.summoned = flag(*cls, "summon");
    u.guaranteedDrop = side == "enemy" && flag(s, "guaranteedDrop");
    u.undead = flag(*cls, "undead"), u.boss = flag(*cls, "boss");
    return u;
  }
};

// pathfinding.ts footprint
std::vector<Hex> body(const Unit& u) {
  const int s = std::max(1, u.size);
  if (s <= 1) return {{u.x, u.y}};
  std::vector<Hex> out;
  if (u.footprintOffsets.is_array()) {
    const int shift = u.y & 1;
    for (const json& o : u.footprintOffsets) {
      const int dx = integer(o, "dx"), dy = integer(o, "dy");
      out.push_back({u.x + dx + ((dy & 1) ? shift : 0), u.y + dy});
    }
    return out;
  }
  if (s >= 4) {
    const int width = u.footprintW.is_number() ? u.footprintW.get<int>() : 2, height = u.footprintH.is_number() ? u.footprintH.get<int>() : 4;
    for (int dy = 0; dy > -height; --dy) {
      const int cx = (dy % 2 != 0) ? u.x - 1 : u.x, start = cx - (width - 1) / 2;
      for (int i = 0; i < width; ++i) out.push_back({start + i, u.y + dy});
    }
    return out;
  }
  out.push_back({u.x, u.y});
  const size_t want = s <= 2 ? 2 : static_cast<size_t>(std::min(4, s));
  for (const Hex& n : neighbors(u.x, u.y)) {
    out.push_back(n);
    if (out.size() >= want) break;
  }
  return out;
}

std::map<Hex, size_t> occupancy(const std::vector<Unit>& units) {
  std::map<Hex, size_t> occ;
  for (size_t i = 0; i < units.size(); ++i)
    if (units[i].alive)
      for (const Hex& p : body(units[i])) occ[p] = i;
  return occ;
}

// pathfinding.ts canTraverseWater
bool traversesWater(const Unit& u, const Field& f, int x, int y) {
  if (f.tileAt(x, y) != "water" || (u.classId != "swampBlueCalf" && u.classId != "roccoTheBird")) return false;
  return f.overlay.empty() || (f.overlay[static_cast<size_t>(y * f.cols + x)] & kBlocked) == 0;
}

// engine.ts nudgeOffHazard / nudgeOffWaypoint: a breadth-first walk to the nearest hex the unit may stand on
void nudge(Unit& u, const Field& f, const std::vector<Unit>& units, size_t self, const std::set<Hex>* avoid, bool water) {
  const auto ok = [&](int x, int y) { const Def d = f.hexDef(x, y); return d.passable || (water && traversesWater(u, f, x, y)); };
  if (!avoid && ok(u.x, u.y)) return;
  if (avoid && !avoid->contains({u.x, u.y})) return;
  const std::map<Hex, size_t> occ = occupancy(units);
  const Hex start{u.x, u.y};
  std::set<Hex> seen{start};
  std::deque<Hex> q{start};
  while (!q.empty()) {
    const Hex cur = q.front();
    q.pop_front();
    for (const Hex& n : neighbors(cur.x, cur.y)) {
      if (!f.inBounds(n.x, n.y) || !seen.insert(n).second) continue;
      const auto who = occ.find(n);
      const bool free = who == occ.end() || who->second == self;
      if (ok(n.x, n.y) && (!avoid || !avoid->contains(n)) && free) {
        u.x = n.x, u.y = n.y;
        return;
      }
      q.push_back(n);
    }
  }
}

// uniqueNeutralNpcSpawns' name key: accents stripped, trimmed, lower case
std::string plainName(const std::string& s) {
  static const char* table = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";  // U+00C0..U+00FF
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == 0xC3 && i + 1 < s.size()) {
      const unsigned char d = static_cast<unsigned char>(s[++i]);
      if (d >= 0x80 && d <= 0xBF) out.push_back(table[d - 0x80]);
      continue;
    }
    out.push_back(static_cast<char>(c));
  }
  const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  out.erase(out.begin(), std::find_if(out.begin(), out.end(), notSpace));
  out.erase(std::find_if(out.rbegin(), out.rend(), notSpace).base(), out.end());
  for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return out;
}

// --- the sound cues (export_battle.ts soundsFor through audio.ts, recorded once per cue in the rules) -------------------

struct Cues {
  const Rules& r;
  const Spawner& sp;
  const json& table = r.all()["sounds"]["cues"];
  const json& monsterTable = r.all()["sounds"]["monster"];
  static json none() { return json::array(); }
  json cue(const char* name) const { return table.contains(name) ? table[name] : none(); }
  const json* monster(const std::string& sprite, const char* kind) const {
    const auto it = monsterTable.find(sprite);
    return it != monsterTable.end() && it->contains(kind) ? &(*it)[kind] : nullptr;
  }
  json monsterOr(const std::string& sprite, const char* kind, json fallback) const { const json* m = monster(sprite, kind); return m ? *m : fallback; }
  json arrowAttack(const Unit& u, bool longDraw) const { return cue(u.sprite == "neera" ? (longDraw ? "arrowAttack:neera:long" : "arrowAttack:neera") : (longDraw ? "arrowAttack:other:long" : "arrowAttack:other")); }
  // engine.ts isBladeAttack / playMeleeCue (main hand)
  std::string weaponOrStarter(const Unit& u) const {
    if (!u.weaponId.empty()) return u.weaponId;
    const json* c = r.cls(u.classId);
    return c ? str(*c, "starterWeapon") : "";
  }
  bool blade(const Unit& u) const {
    const std::string id = weaponOrStarter(u);
    if (id.empty()) return false;
    const json* q = r.equipment(id);
    const std::string type = q ? str(*q, "weaponType") : "";
    if (!type.empty()) return type == "sword" || type == "axe" || type == "dagger";
    for (const char* prefix : {"espada", "machado", "lamina", "adaga", "punhal", "katar"})
      if (id.starts_with(prefix)) return true;
    return false;
  }
  json melee(const Unit& u, const std::string& skill, bool kaelStart) const {
    if (u.name == "Kael" && oneOf(skill, {"cleave", "sweep", "shoulderSmash", "stampede", "piercingThrust", "trip", "doubleStrike", "bullRush", "executionerStrike"})) return cue("kaelBladeSkill");
    const std::string id = weaponOrStarter(u);
    const json* q = id.empty() ? nullptr : r.equipment(id);
    if (q && str(*q, "weaponType") == "dagger") return cue("daggerAttack");
    return blade(u) ? cue(kaelStart ? "meleeAttack:blade:kael" : "meleeAttack:blade") : cue("meleeAttack:blunt");
  }
  // the special creatures' own cues, else nothing (an empty result means "not handled")
  std::optional<json> creature(const std::string& sprite, const char* kind) const {
    const bool attack = std::string_view(kind) == "attack";
    if (sprite == "minor-horror-001") return cue(attack ? "minorHorrorAttack" : "minorHorrorCast");
    if (sprite == "carnivorous-plant-001") return cue(attack ? "carnivorousPlantAttack" : "carnivorousPlantCast");
    if (sprite == "sapling-001") return cue(attack ? "saplingAttack" : "saplingCast");
    if (sprite == "plague-bearing-cattle") return cue(attack ? "plagueCattleAttack" : "plagueCattleCast");
    return std::nullopt;
  }
  json attack(const Unit& u, bool counter) const {
    const bool arrow = counter ? sp.isArrowAttack(u) : (oneOf(u.classId, {"brigand", "baseCrossbowman", "brigandCrossbowman"}) || sp.isArrowAttack(u));
    if (arrow) return arrowAttack(u, true);
    if (Spawner::isArcaneCaster(u)) {
      if (u.sprite == "cultist-v2") return cue("cultistV2Attack");
      return monsterOr(u.sprite, "attack", cue("magicAttack"));
    }
    if (!counter && u.sprite == "kaelFinal") return none();
    if (const std::optional<json> own = creature(u.sprite, "attack")) return *own;
    if (const json* m = monster(u.sprite, "attack")) return *m;
    if (counter && u.sprite == "kaelFinal") return none();
    return melee(u, "", false);
  }
  json spell(const Unit& u, const std::string& kind) const {
    if (oneOf(kind, {"longShot", "bloodyShot", "multiShot", "piercing"})) return arrowAttack(u, true);
    if (kind == "tendrilSwipe") return cue("carnivorousPlantAttack");
    if (oneOf(kind, {"cleave", "sweep", "shoulderSmash", "stampede", "piercingThrust", "trip", "doubleStrike"})) return melee(u, kind, false);
    if (kind == "bless") return monsterOr(u.sprite, "heal", cue("heal"));
    const json& classes = r.all()["spellClass"];
    const bool support = classes.contains(kind) && classes[kind] == "utility";
    if (support && monster(u.sprite, "heal")) return *monster(u.sprite, "heal");
    if (const std::optional<json> own = creature(u.sprite, "cast")) return *own;
    if (const json* m = monster(u.sprite, "cast")) return *m;
    if (u.sprite == "cultist-v2") return cue("cultistV2Spellcast");
    return cue("spell");
  }
  json walk(const Unit& u, bool left) const {
    if (u.sprite == "minor-horror-001") return cue("minorHorrorWalk");
    if (u.sprite == "sapling-001") return cue("saplingWalk");
    if (u.sprite == "plague-bearing-cattle") return cue("plagueCattleWalk");
    if (const json* m = monster(u.sprite, "walk")) return *m;
    if (u.sprite == "cultist-v2") return cue(left ? "cultistV2WalkLeft" : "cultistV2WalkRight");
    return cue("move");
  }
  json all(const Unit& u) const {
    const bool arrow = oneOf(u.classId, {"brigand", "baseCrossbowman", "brigandCrossbowman"}) || sp.isArrowAttack(u);
    const bool arcane = !arrow && Spawner::isArcaneCaster(u);
    json spells = json::object();
    std::vector<std::string> kinds;
    for (const json& k : sp.spellList(u.classId, u.level, u.name)) kinds.push_back(k.get<std::string>());
    for (const char* k : {"shock", "frost", "fantomForce"})
      if (std::find(kinds.begin(), kinds.end(), k) == kinds.end()) kinds.push_back(k);
    for (const std::string& k : kinds) spells[k] = spell(u, k);
    const bool plant = u.sprite == "carnivorous-plant-001" || u.sprite == "sapling-001";
    json hit = json::array(), death = json::array();
    const auto append = [](json& to, const json& from) { for (const json& c : from) to.push_back(c); };
    if (plant) append(hit, cue(u.sprite == "sapling-001" ? "saplingHit" : "carnivorousPlantHit"));
    if (const json* m = monster(u.sprite, "hit")) append(hit, *m);
    if (plant) append(death, cue(u.sprite == "sapling-001" ? "saplingDeath" : "carnivorousPlantDeath"));
    if (u.sprite == "plague-bearing-cattle") append(death, cue("plagueCattleDeath"));
    if (const json* m = monster(u.sprite, "death")) append(death, *m);
    return {{"attack", attack(u, false)},
            {"strike", u.sprite == "kaelFinal" && !arrow && !arcane ? melee(u, "", true) : none()},
            {"counter", attack(u, true)},
            {"spells", spells},
            {"release", cue(u.sprite == "neera" ? "arrowRelease:neera" : "arrowRelease:other")},
            {"heal", monsterOr(u.sprite, "heal", cue("heal"))},
            {"walk", walk(u, false)},
            {"walkLeft", walk(u, true)},
            {"walkFade", u.sprite == "plague-bearing-cattle" || monster(u.sprite, "walk") != nullptr},
            {"hit", hit},
            {"hitOnDeath", u.classId != "bigBlueCalf"},
            {"death", death}};
  }
};

// --- the map file ----------------------------------------------------------------------------------------------------

const json& draftOf(const json& file) { return file.contains("draft") ? file["draft"] : file; }

std::vector<std::string> tilesOf(const Rules& r, const json& draft) {
  std::vector<std::string> tiles;
  if (draft.contains("tiles") && draft["tiles"].is_array())
    for (const json& t : draft["tiles"]) {
      const std::string id = t.is_string() ? t.get<std::string>() : "plains";
      tiles.push_back(r.knownTerrain(id) ? id : "plains");  // parseLayout: a char no terrain owns reads as plains
    }
  const size_t n = static_cast<size_t>(integer(draft, "cols") * integer(draft, "rows"));
  tiles.resize(n, "plains");
  return tiles;
}

std::vector<json> listOf(const json& draft, const char* key) {
  std::vector<json> out;
  if (draft.contains(key) && draft[key].is_array())
    for (const json& x : draft[key]) out.push_back(x);
  return out;
}

// mapstore.ts legacyClassId on every spawn
void legacyClassIds(const Rules& r, json& draft) {
  const json& legacy = r.all()["legacyClassIds"];
  for (const char* list : {"playerSpawns", "enemySpawns", "neutralSpawns"})
    if (draft.contains(list) && draft[list].is_array())
      for (json& s : draft[list]) {
        const std::string c = str(s, "classId");
        if (legacy.contains(c)) s["classId"] = legacy[c];
      }
}

}  // namespace

std::optional<json> partyBattle(const std::vector<std::string>& heroes, std::string* error) {
  const Rules* rules = Rules::shared();
  if (!rules) {
    if (error) *error = "content/data/battle-rules.json missing";
    return std::nullopt;
  }
  const json& classes = rules->all()["campaignHeroClasses"];
  json draft = {{"id", "party-sheet"}, {"index", 0}, {"cols", std::max<size_t>(1, heroes.size())}, {"rows", 1}, {"playerSpawns", json::array()}};
  for (size_t i = 0; i < heroes.size(); ++i) {
    const std::string classId = classes.is_object() ? str(classes, heroes[i].c_str()) : std::string();
    if (classId.empty()) {
      if (error) *error = "no campaign class for " + heroes[i];
      return std::nullopt;
    }
    draft["playerSpawns"].push_back({{"name", heroes[i]}, {"classId", classId}, {"x", static_cast<int>(i)}, {"y", 0}});
  }
  return exportBattle(draft, error);
}

std::vector<std::string> classSpellsAt(const std::string& classId, int level, const std::string& name) {
  std::vector<std::string> out;
  const Rules* r = Rules::shared();
  if (!r) return out;
  for (const json& k : Spawner{*r}.spellList(classId, level, name))
    if (k.is_string()) out.push_back(k.get<std::string>());
  return out;
}

// --- make_test_map.ts ------------------------------------------------------------------------------------------------

bool prepareCampaignMap(const std::filesystem::path& mapFile, const std::filesystem::path& outFile, std::string* note) {
  const Rules* r = Rules::shared();
  if (!r) return false;
  std::ifstream in(mapFile, std::ios::binary);
  json file = in ? json::parse(in, nullptr, false) : json();
  if (!in || file.is_discarded() || !file.is_object()) {
    logError("prepare map: cannot read {}", mapFile.string());
    return false;
  }
  json& draft = file.contains("draft") ? file["draft"] : file;
  const json& heroClasses = r->all()["campaignHeroClasses"];
  // Older maps used the hero's name as a class id (Malrec: "malrec"): repaired in the prepared copy
  if (draft.contains("playerSpawns") && draft["playerSpawns"].is_array())
    for (json& s : draft["playerSpawns"]) {
      const std::string name = str(s, "name");
      if (!r->cls(str(s, "classId")) && heroClasses.contains(name)) s["classId"] = heroClasses[name];
    }
  json mission = draft;
  legacyClassIds(*r, mission);
  const int cols = integer(mission, "cols"), rows = integer(mission, "rows");
  std::vector<json> added;
  if (!flag(mission, "explore")) {
    std::vector<json> decorations = closeWatchtowerWalls(*r, str(mission, "id"), tilesOf(*r, mission), cols, rows, listOf(mission, "decorations"));
    std::vector<std::string> terrain = tilesOf(*r, mission);
    clearRockColumnTiles(*r, terrain, cols, rows, decorations, str(mission, "baseTile"));
    std::set<std::string> present;
    std::set<Hex> occupied;
    for (const json& s : listOf(mission, "playerSpawns")) present.insert(str(s, "name"));
    for (const char* list : {"playerSpawns", "enemySpawns", "neutralSpawns"})
      for (const json& s : listOf(mission, list)) occupied.insert({integer(s, "x"), integer(s, "y")});
    const std::vector<json> players = listOf(mission, "playerSpawns");
    const Hex anchor = players.empty() ? Hex{0, 0} : Hex{integer(players[0], "x"), integer(players[0], "y")};
    std::vector<std::string> order;  // the heroes in their roster order (a JSON object's keys sort alphabetically here)
    if (r->all().contains("campaignHeroOrder"))
      for (const json& n : r->all()["campaignHeroOrder"]) order.push_back(n.get<std::string>());
    else
      for (const auto& [n, c] : heroClasses.items()) order.push_back(n);
    for (const std::string& name : order) {
      if (present.contains(name) || !heroClasses.contains(name)) continue;
      const json& classId = heroClasses[name];
      std::set<Hex> seen{anchor};
      std::deque<Hex> q{anchor};
      std::optional<Hex> placed;
      while (!q.empty() && !placed) {
        const Hex cur = q.front();
        q.pop_front();
        for (const Hex& n : neighbors(cur.x, cur.y)) {
          if (n.x < 0 || n.y < 0 || n.x >= cols || n.y >= rows || !seen.insert(n).second) continue;
          const std::string& t = terrain[static_cast<size_t>(n.y * cols + n.x)];
          if (r->knownTerrain(t) && flag(r->terrain(t), "passable") && !occupied.contains(n)) {
            placed = n;
            break;
          }
          q.push_back(n);
        }
      }
      if (!placed) continue;
      occupied.insert(*placed);
      added.push_back({{"name", name}, {"classId", classId}, {"x", placed->x}, {"y", placed->y}});
    }
  }
  if (!draft.contains("playerSpawns") || !draft["playerSpawns"].is_array()) draft["playerSpawns"] = json::array();
  for (const json& a : added) draft["playerSpawns"].push_back(a);
  std::error_code ec;
  std::filesystem::create_directories(outFile.parent_path(), ec);
  std::ofstream out(outFile, std::ios::binary);
  if (!out) {
    logError("prepare map: cannot write {}", outFile.string());
    return false;
  }
  out << file.dump();
  if (note) {
    std::string list;
    for (const json& a : added) list += (list.empty() ? "" : ", ") + str(a, "name") + " at " + std::to_string(integer(a, "x")) + "," + std::to_string(integer(a, "y"));
    *note = str(draft, "id") + ": campaign map, " + std::to_string(draft["playerSpawns"].size()) + " heroes (" + (added.empty() ? "none added" : "added " + list) + ")";
  }
  return true;
}

// --- export_battle.ts ------------------------------------------------------------------------------------------------

std::optional<json> exportBattle(const json& mapFile, std::string* error, int playerLevel) {
  const Rules* rules = Rules::shared();
  if (!rules) {
    if (error) *error = "content/data/battle-rules.json missing";
    return std::nullopt;
  }
  const Rules& r = *rules;
  json draft = draftOf(mapFile);
  if (!draft.is_object() || integer(draft, "cols") <= 0 || integer(draft, "rows") <= 0) {
    if (error) *error = "no board in the map file";
    return std::nullopt;
  }
  legacyClassIds(r, draft);
  const std::string mapId = str(draft, "id");
  const std::string baseTile = str(draft, "baseTile");
  Field f{r};
  f.cols = integer(draft, "cols"), f.rows = integer(draft, "rows");
  f.tiles = tilesOf(r, draft);
  if (draft.contains("terrainElevations") && draft["terrainElevations"].is_array())
    for (const json& v : draft["terrainElevations"]) f.elevations.push_back(v.is_number() ? std::optional<double>(v.get<double>()) : std::nullopt);
  // mapstore.ts normalizeDraft, then the battle's own opening: the watchtower's walls closed, legacy rock columns cleared,
  // decorations with a tile of their own stamping it, a barricade decoration on every barricade tile
  f.decorations = closeWatchtowerWalls(r, mapId, f.tiles, f.cols, f.rows, listOf(draft, "decorations"));
  clearRockColumnTiles(r, f.tiles, f.cols, f.rows, f.decorations, baseTile);
  f.decorations = closeWatchtowerWalls(r, mapId, f.tiles, f.cols, f.rows, f.decorations);
  clearRockColumnTiles(r, f.tiles, f.cols, f.rows, f.decorations, baseTile);
  for (const json& p : f.decorations) {
    const json* def = r.decoration(str(p, "id"));
    if (!def || !has(*def, "tile")) continue;
    for (const Hex& c : placedFootprint(r, p)) {
      const int x = integer(p, "x") + c.x, y = integer(p, "y") + c.y;
      if (f.inBounds(x, y)) f.tiles[static_cast<size_t>(y * f.cols + x)] = str(*def, "tile");
    }
  }
  for (int y = 0; y < f.rows; ++y)
    for (int x = 0; x < f.cols; ++x) {
      if (f.tileAt(x, y) != "barricade") continue;
      const bool placed = std::any_of(f.decorations.begin(), f.decorations.end(), [&](const json& d) { return str(d, "id") == "barricade" && integer(d, "x") == x && integer(d, "y") == y; });
      if (!placed) f.decorations.push_back({{"id", "barricade"}, {"x", x}, {"y", y}});
    }
  f.refreshOverlay();

  // The game's own classes (content/ember-classes.json): spawned as the stand-in, named back below
  // `sounds`: the class's own cues (content/ember-classes.json "sounds", by cue name: attack, counter, walk, walkLeft, walkFade, hit,
  // death, ...), over the stand-in's (the Chain-Bound Penitent's cleave and steps, the user's own files, 2026-10-27)
  struct Ember { std::string classId, sprite, name; bool named = false; json sounds; };
  std::map<Hex, Ember> emberAt;
  {
    std::ifstream in("content/ember-classes.json", std::ios::binary);
    json ember = in ? json::parse(in, nullptr, false) : json();
    const json classes = ember.is_object() && ember.contains("classes") ? ember["classes"] : json::object();
    const std::string standIn = ember.is_object() ? str(ember, "standIn", "zombie") : "zombie";
    for (const char* list : {"enemySpawns", "neutralSpawns"})
      if (draft.contains(list) && draft[list].is_array())
        for (json& s : draft[list]) {
          const std::string c = str(s, "classId");
          if (!classes.contains(c)) continue;
          const json& e = classes[c];
          emberAt[{integer(s, "x"), integer(s, "y")}] = {c, str(e, "sprite"), str(e, "name"), !str(s, "name").empty() && str(s, "name") != str(e, "name"),
                                                        e.contains("sounds") && e["sounds"].is_object() ? e["sounds"] : json()};
          s["classId"] = standIn;
        }
  }

  const Spawner sp{r};
  const int enemyLevel = r.row(r.all()["enemyLevelFor"], integer(draft, "index") + 1).get<int>();
  std::vector<Unit> units;
  const auto spawnAll = [&](const char* list, const std::string& side) {
    const std::vector<json> spawns = listOf(draft, list);
    std::set<std::string> names, appearances;
    for (size_t i = 0; i < spawns.size(); ++i) {
      const json& s = spawns[i];
      if (side == "neutral" && has(s, "dialog")) {  // uniqueNeutralNpcSpawns: one NPC per name and per look
        const json* c = r.cls(str(s, "classId"));
        const std::string name = plainName(str(s, "name")), appearance = c ? str(*c, "sprite") : str(s, "classId");
        if (names.contains(name) || appearances.contains(appearance)) continue;
        names.insert(name), appearances.insert(appearance);
      }
      std::optional<Unit> u = sp.spawn(s, side, static_cast<int>(i), side == "player" ? std::max(1, playerLevel) : enemyLevel, error);
      if (!u) return false;
      units.push_back(std::move(*u));
    }
    return true;
  };
  if (!spawnAll("playerSpawns", "player") || !spawnAll("enemySpawns", "enemy") || !spawnAll("neutralSpawns", "neutral")) return std::nullopt;
  std::set<Hex> waypointCells;
  for (const json& p : f.decorations) {
    const json* def = r.decoration(str(p, "id"));
    if (!def || !has(*def, "exitKind")) continue;
    for (const Hex& c : placedFootprint(r, p)) waypointCells.insert({integer(p, "x") + c.x, integer(p, "y") + c.y});
  }
  for (size_t i = 0; i < units.size(); ++i) {
    nudge(units[i], f, units, i, nullptr, true);
    if (units[i].side == "player") nudge(units[i], f, units, i, &waypointCells, false);
  }

  json out;
  out["source"] = "the game's own map preparation (engine/world/BattleExport.cpp exportBattle), map " + mapId;
  out["map"] = mapId;
  out["cols"] = f.cols, out["rows"] = f.rows;
  out["debugFreeCast"] = false;
  const bool explore = flag(draft, "explore");
  out["explore"] = explore;
  json cells = json::array();
  for (int y = 0; y < f.rows; ++y)
    for (int x = 0; x < f.cols; ++x) cells.push_back(f.cellRecord(x, y));
  out["cells"] = cells;
  // the waypoints
  json exits = json::array();
  for (const json& d : f.decorations) {
    const json* def = r.decoration(str(d, "id"));
    if (!def || !has(*def, "exitKind")) continue;
    json cellsOut = json::array();
    for (const Hex& c : placedFootprint(r, d)) cellsOut.push_back({integer(d, "x") + c.x, integer(d, "y") + c.y});
    exits.push_back({{"id", str(d, "id")}, {"kind", (*def)["exitKind"]}, {"x", integer(d, "x")}, {"y", integer(d, "y")},
                     {"targetMapId", has(d, "targetMapId") && d["targetMapId"].is_string() ? d["targetMapId"] : json(nullptr)},
                     {"returnConnector", flag(d, "returnConnector")},
                     {"connectorDirection", has(d, "connectorDirection") && d["connectorDirection"].is_string() ? d["connectorDirection"] : json(nullptr)},
                     {"cells", cellsOut}});
  }
  out["exits"] = exits;
  // the locks, each with the cells its opening changes (useLockpick on a copy, a hero with a Gazua beside it)
  struct LockSpot { int x, y; std::string kind, id, tier; };
  std::vector<LockSpot> lockList;
  for (int y = 0; y < f.rows; ++y)
    for (int x = 0; x < f.cols; ++x)
      if (f.tileAt(x, y) == "door") lockList.push_back({x, y, "doorTile", "door", "base"});
  const auto doorDecor = [&](const json& d) { const json* def = r.decoration(str(d, "id")); const std::string m = def ? str(*def, "model3d") : ""; return m == "door" || m == "secretDoor"; };
  for (const json& d : f.decorations)
    if (doorDecor(d)) lockList.push_back({integer(d, "x"), integer(d, "y"), "doorDecor", str(d, "id"), "base"});
  const std::vector<json> betterChests = listOf(draft, "betterChests");
  for (const json& d : f.decorations) {
    const std::string id = str(d, "id");
    if (!r.inSet("chest", id)) continue;
    const int x = integer(d, "x"), y = integer(d, "y");
    const bool betterSpot = std::any_of(betterChests.begin(), betterChests.end(), [&](const json& c) { return integer(c, "x") == x && integer(c, "y") == y; });
    lockList.push_back({x, y, "chest", id, id == "chest-large" ? "best" : (id == "chest-medium" || betterSpot) ? "better" : "base"});
  }
  json locks = json::array();
  const json& variants = r.all()["doorVariants"];
  for (const LockSpot& lk : lockList) {
    json after = json::array();
    Field g = f;  // a fresh battle
    const std::map<Hex, size_t> occ = occupancy(units);
    std::vector<Hex> spots;
    for (const Hex& n : neighbors(lk.x, lk.y))
      if (g.inBounds(n.x, n.y) && g.hexDef(n.x, n.y).passable && !occ.contains(n)) spots.push_back(n);
    const auto hero = std::find_if(units.begin(), units.end(), [](const Unit& v) { return v.side == "player" && v.alive && v.size == 1; });
    if (!spots.empty() && hero != units.end()) {
      const Hex at = spots.front();
      // adjacentLock from where the hero stands: the first neighbour holding a lock (not always this one)
      std::optional<Hex> target;
      for (const Hex& p : neighbors(at.x, at.y)) {
        if (!g.inBounds(p.x, p.y)) continue;
        const bool lock = g.tileAt(p.x, p.y) == "door" ||
                          std::any_of(g.decorations.begin(), g.decorations.end(), [&](const json& d) { return (doorDecor(d) || r.inSet("chest", str(d, "id"))) && integer(d, "x") == p.x && integer(d, "y") == p.y; });
        if (lock) { target = p; break; }
      }
      if (target) {
        const size_t i = static_cast<size_t>(target->y * g.cols + target->x);
        const bool wasChest = std::any_of(g.decorations.begin(), g.decorations.end(), [&](const json& d) { return r.inSet("chest", str(d, "id")) && integer(d, "x") == target->x && integer(d, "y") == target->y; });
        const auto door = std::find_if(g.decorations.begin(), g.decorations.end(), [&](const json& d) { return doorDecor(d) && integer(d, "x") == target->x && integer(d, "y") == target->y; });
        if (!wasChest && door == g.decorations.end()) g.tiles[i] = baseTile.empty() ? "nave" : baseTile;
        if (door != g.decorations.end()) {
          const json* def = r.decoration(str(*door, "id"));
          const std::string style = def && has(*def, "doorStyle") ? str(*def, "doorStyle") : "oak";
          if (variants.contains(style)) (*door)["id"] = variants[style]["open"];
          door->erase("blocksPath");
          g.refreshOverlay();
        }
        bool removed = false;
        for (size_t d = g.decorations.size(); d-- > 0;)
          if (r.inSet("chest", str(g.decorations[d], "id")) && integer(g.decorations[d], "x") == target->x && integer(g.decorations[d], "y") == target->y) {
            g.decorations.erase(g.decorations.begin() + static_cast<std::ptrdiff_t>(d));
            removed = true;
          }
        if (removed) g.refreshOverlay();
        for (int k = 0; k < g.cols * g.rows; ++k) {
          const json rec = g.cellRecord(k % g.cols, k / g.cols);
          if (rec != cells[static_cast<size_t>(k)]) after.push_back({k, rec});
        }
      }
    }
    locks.push_back({{"x", lk.x}, {"y", lk.y}, {"kind", lk.kind}, {"id", lk.id}, {"tier", lk.tier}, {"after", after}});
  }
  out["locks"] = locks;

  // the units
  const Cues cues{r, sp};
  json unitsOut = json::array();
  for (const Unit& u : units) {
    json o;
    o["id"] = u.id, o["side"] = u.side, o["name"] = u.name, o["classId"] = u.classId, o["className"] = u.className, o["sprite"] = u.sprite;
    o["x"] = u.x, o["y"] = u.y, o["alive"] = u.alive;
    o["mov"] = (u.side == "enemy" && u.holdsPosition) ? 0 : explore ? f.cols * f.rows : u.mov;
    o["baseMov"] = u.mov;
    o["hp"] = u.hp, o["maxHp"] = u.maxHp, o["atk"] = u.atk, o["def"] = u.def, o["mag"] = u.mag, o["dex"] = u.dex;
    o["minRange"] = u.minRange, o["maxRange"] = u.maxRange, o["size"] = u.size, o["summoned"] = u.summoned, o["level"] = u.level;
    o["footprintOffsets"] = u.footprintOffsets, o["footprintW"] = u.footprintW, o["footprintH"] = u.footprintH;
    o["undead"] = u.undead, o["boss"] = u.boss;
    o["stunned"] = false, o["stunTurns"] = 0, o["asleep"] = false, o["sleepTurns"] = 0, o["bleeding"] = false, o["bleedRoundsLeft"] = nullptr;
    o["crippled"] = false, o["fearTurns"] = 0, o["fearSourceId"] = nullptr, o["blessedHitBonusPct"] = 0, o["blessedRoundsLeft"] = 0;
    o["summonerId"] = nullptr, o["spellCharges"] = 0, o["lifeDrainCharges"] = nullptr;
    o["initiativeBonus"] = u.initiative;
    json spells = json::object();
    for (int t = 1; t <= 10; ++t) spells["tier" + std::to_string(t)] = u.spells[static_cast<size_t>(t - 1)];
    o["spells"] = spells;
    o["spellList"] = sp.spellList(u.classId, u.level, u.name);
    o["shockCharges"] = u.shockCharges, o["frostCharges"] = u.frostCharges, o["fantomForceCharges"] = u.fantomForceCharges;
    const json& fantom = r.row(r.all()["enemyPools"]["fantomDice"], u.level);
    o["fantomForceDice"] = {{"dice", fantom[0]}, {"faces", fantom[1]}};
    o["xp"] = 0, o["guaranteedDrop"] = u.guaranteedDrop;
    const OffHandDef* ownPiece = u.ownWeapon.empty() ? nullptr : sharedLoadouts().offHand(u.ownWeapon);  // Selene's dagger
    const std::string type = ownPiece ? ownPiece->weaponType : sp.weaponType(u);
    o["weaponType"] = type.empty() ? json(nullptr) : json(type);
    o["trainsWeapon"] = ownPiece ? true : sp.trains(u, type);
    o["weaponSkill"] = 0, o["offWeaponType"] = nullptr, o["offTrainsWeapon"] = false, o["offWeaponSkill"] = 0, o["healingSkill"] = 0;
    o["bag"] = u.bag, o["diseased"] = false, o["diseaseBase"] = nullptr, o["poisoned"] = false, o["poisonTier"] = nullptr;
    o["fullness"] = 100, o["hungerPenaltyPct"] = 0;
    if (u.side == "player") {
      const json* cls = r.cls(u.classId);
      json growth = json::array();
      for (int lv = 1; lv <= r.maxLevel(); ++lv) {
        const json& st = r.row((*cls)["stats"], lv);
        json row = {st[0], st[1], st[2], st[3], st[4]};
        for (int t = 1; t <= 10; ++t) row.push_back(sp.tierUses(u.classId, t, lv));
        row.push_back(integer(*cls, "mov"));
        growth.push_back(row);
      }
      o["growth"] = growth;
    } else {
      o["growth"] = nullptr;
    }
    json combat;
    const json* w = u.weaponId.empty() ? nullptr : r.weapon(u.weaponId);
    combat["weapon"] = w ? json{{"dice", (*w)["dice"]}, {"faces", (*w)["faces"]}, {"bonus", integer(*w, "bonus")}, {"ranged", flag(*w, "ranged")}, {"classBonus", str(*w, "bonusClass") == u.classId}} : json(nullptr);
    if (ownPiece) combat["weapon"] = {{"dice", ownPiece->dice}, {"faces", ownPiece->faces}, {"bonus", ownPiece->bonus}, {"ranged", false}, {"classBonus", false}};
    combat["weaponEnh"] = 0;
    combat["mastery"] = ownPiece ? json{{"accuracy", number(r.all()["weaponSkill"], "accuracy", 75)}, {"damage", number(r.all()["weaponSkill"], "damage", 1)}}
                                 : sp.mastery(u, type);
    combat["offMastery"] = sp.mastery(u, "");
    combat["offHand"] = nullptr;
    if (const json* magic = sp.staffMagic(u)) {
      const std::string element = str(*magic, "attackElement", "arcane");
      const json bonuses = has(*magic, "elementalBonusPct") ? (*magic)["elementalBonusPct"] : json::object();
      combat["staff"] = {{"element", element}, {"bonusPct", number(bonuses, element.c_str())}, {"bonuses", bonuses}, {"lifeSteal", number(*magic, "lifeStealPct")}, {"regeneration", number(*magic, "regeneration")}};
    } else {
      combat["staff"] = nullptr;
    }
    combat["arcane"] = Spawner::isArcaneCaster(u);
    combat["arrow"] = sp.isArrowAttack(u);
    combat["resistances"] = u.resistances;
    combat["faceDx"] = u.facing, combat["faceDy"] = 0, combat["facing"] = u.facing;
    o["combat"] = combat;
    o["sounds"] = cues.all(u);
    if (u.side != "player") {
      const auto e = emberAt.find({u.x, u.y});
      if (e != emberAt.end()) {
        o["classId"] = e->second.classId, o["sprite"] = e->second.sprite, o["className"] = e->second.name;
        if (!e->second.named) o["name"] = e->second.name;
        o["undead"] = false, o["boss"] = false, o["spellList"] = json::array();
        if (e->second.sounds.is_object())
          for (const auto& [k, v] : e->second.sounds.items()) o["sounds"][k] = v;
      }
    }
    unitsOut.push_back(std::move(o));
  }
  out["units"] = unitsOut;
  out["affinityBonus"] = json::object();  // a fresh party: no affinity yet
  out["affinityScores"] = json::object();
  out["win"] = has(draft, "win") && draft["win"].is_string() ? draft["win"] : json("rout");
  out["ownedWeapons"] = json::array();
  out["sounds"] = {{"levelUp", cues.cue("levelUp")}};
  // the battle's music: the mission's own track when it names one that exists, else the theme for its id
  const std::string named = str(draft, "music");
  const json& music = r.all()["music"];
  if (!named.empty() && (contains(music["tracks"], named) || std::filesystem::exists(std::filesystem::path("content/music") / named))) {
    out["music"] = {{"file", named}, {"base", 0.4}};
  } else {
    const std::string theme = mapId.starts_with("cemiterio") ? "cemetery" : mapId == "templo" ? "temple" : mapId == "aldeia" ? "aldeia"
                            : oneOf(mapId, {"vau", "bosque", "cripta", "vertente"}) ? "early" : mapId == "estalagem" ? "inn"
                            : oneOf(mapId, {"muralha", "fortaleza"}) ? "siege" : oneOf(mapId, {"colina", "passagem"}) ? "hill"
                            : mapId == "portao" ? "portao" : "early";
    const json& t = music["themes"][theme];
    out["music"] = {{"file", t[0]}, {"base", t[1]}, {"theme", theme}};
  }
  return out;
}

// --- export_scene.ts's file from a built scene ---------------------------------------------------------------------------

json sceneJson(const SceneData& s, const MapDraft& draft, const std::map<std::string, int>& skipped) {
  const auto light = [](const std::optional<SceneLight>& l) -> json {
    if (!l) return nullptr;
    json out = {{"color", {l->color[0], l->color[1], l->color[2]}}, {"intensity", l->intensity}, {"radius", l->radius}, {"flicker", l->flicker}, {"minHeight", l->minHeight}, {"seed", l->seed}};
    if (l->offsetX != 0 || l->offsetZ != 0) out["offset"] = {l->offsetX, l->offsetZ};
    return out;
  };
  const auto opt = [](const std::optional<float>& v) -> json { return v ? json(*v) : json(nullptr); };
  json decor = json::array();
  for (const SceneCard& c : s.cards)
    decor.push_back({{"kind", "card"}, {"id", c.id}, {"image", c.image}, {"x", c.x}, {"z", c.z}, {"ground", {{"smooth", c.groundSmooth}, {"hard", c.groundHard}}},
                     {"w", c.w}, {"h", c.h}, {"mirror", c.mirror}, {"onFloor", c.onFloor}, {"castShadow", c.castShadow},
                     {"lockedYaw", c.lockedYaw ? json(*c.lockedYaw) : json(nullptr)}, {"nudge", c.nudge}, {"standOn", c.standOn}, {"light", light(c.light)},
                     {"art", {{"base", {{"u", c.art.baseU}, {"v", c.art.baseV}}}, {"flame", {{"u", c.art.flameU}, {"v", c.art.flameV}}}, {"flameFound", c.art.flameFound}, {"size", {c.art.width, c.art.height}}}}});
  for (const SceneModel& m : s.models)
    decor.push_back({{"kind", "model"}, {"id", m.id}, {"model", m.model}, {"file", m.file}, {"height", m.height}, {"x", m.x}, {"z", m.z},
                     {"ground", {{"smooth", m.groundSmooth}, {"hard", m.groundHard}}}, {"rotY", m.rotY}});
  for (const SceneArchitecture& a : s.pieces) decor.push_back({{"kind", "architecture"}, {"id", a.id}, {"at", {a.x, a.y}}, {"rot", a.rot}});
  json units = json::array();
  for (const SceneUnit& u : s.units)
    units.push_back({{"side", u.side}, {"name", u.name}, {"classId", u.classId}, {"key", u.key}, {"sprite", u.sprite}, {"set", u.set}, {"clip", u.clip},
                     {"x", u.x}, {"z", u.z}, {"ground", {{"smooth", u.groundSmooth}, {"hard", u.groundHard}}}, {"facing", u.facing}, {"mirror", u.mirror},
                     {"box", {{"w", u.boxW}, {"h", u.boxH}, {"foot", u.foot}}}, {"nat", {u.natW, u.natH}}, {"frames", u.frames}, {"pace", u.pace}, {"light", light(u.light)}});
  json out;
  out["source"] = "the game's own map preparation (engine/world/SceneBuilder.cpp buildScene), map " + s.map;
  out["map"] = s.map, out["title"] = s.title, out["cols"] = draft.cols, out["rows"] = draft.rows;
  out["lighting"] = {{"timeOfDay", s.timeOfDay.empty() ? "day" : s.timeOfDay}, {"sunIntensity", opt(s.sunIntensity)}, {"ambientIntensity", opt(s.ambientIntensity)},
                     {"environment", s.environment.empty() ? "outdoor" : s.environment}, {"bloomIntensity", opt(s.bloomIntensity)}, {"mistType", s.mistType.empty() ? json(nullptr) : json(s.mistType)}};
  if (!s.moonPhase.empty()) out["lighting"]["moonPhase"] = s.moonPhase;  // the editor's Lua (maps without one: the file as before)
  out["backdrop"] = s.backdrop.empty() ? json(nullptr) : json(s.backdrop);
  out["decor"] = decor, out["units"] = units;
  json skippedOut = json::object();
  for (const auto& [why, n] : skipped) skippedOut[why] = n;
  out["skipped"] = skippedOut;
  return out;
}

// --- Cooker check-battle-export ---------------------------------------------------------------------------------------

int checkBattleExport(const std::filesystem::path& maps, const std::vector<std::filesystem::path>& references, int playerLevel) {
  int failed = 0;
  for (const std::filesystem::path& ref : references) {
    std::string stem = ref.filename().string();
    if (const size_t dot = stem.find(".battle.json"); dot != std::string::npos) stem.resize(dot);
    const std::filesystem::path mapFile = maps / (stem + ".json");
    std::ifstream in(ref, std::ios::binary), mapIn(mapFile, std::ios::binary);
    const json expected = in ? json::parse(in, nullptr, false) : json();
    const json mapJson = mapIn ? json::parse(mapIn, nullptr, false) : json();
    if (!in || expected.is_discarded() || !mapIn || mapJson.is_discarded()) {
      logError("battle export {}: cannot read {} or {}", stem, ref.string(), mapFile.string());
      ++failed;
      continue;
    }
    std::string error;
    const std::optional<json> made = exportBattle(mapJson, &error, playerLevel);
    if (!made) {
      logError("battle export {}: {}", stem, error);
      ++failed;
      continue;
    }
    int differences = 0;
    const auto report = [&](const std::string& what) { if (++differences <= 20) logError("battle export {}: {}", stem, what); };
    for (const char* key : {"map", "cols", "rows", "debugFreeCast", "explore", "win", "affinityBonus", "affinityScores", "ownedWeapons", "sounds", "music", "locks"}) {
      const json a = expected.contains(key) ? expected[key] : json(nullptr), b = made->contains(key) ? (*made)[key] : json(nullptr);
      if (a != b) report(std::string(key) + ": " + a.dump().substr(0, 160) + " vs " + b.dump().substr(0, 160));
    }
    // exits: the keys the reference has (files from before 2026-10-17 lack returnConnector / connectorDirection)
    {
      const json ea = expected.contains("exits") ? expected["exits"] : json::array(), eb = (*made)["exits"];
      if (ea.size() != eb.size()) report("exits: " + std::to_string(ea.size()) + " vs " + std::to_string(eb.size()));
      for (size_t i = 0; i < ea.size() && i < eb.size(); ++i)
        for (const auto& [k, v] : ea[i].items())
          if (!eb[i].contains(k) || eb[i][k] != v) report("exit " + std::to_string(i) + " " + k + ": " + v.dump() + " vs " + (eb[i].contains(k) ? eb[i][k].dump() : "missing"));
    }
    if (expected["cells"] != (*made)["cells"]) {
      const json& a = expected["cells"];
      const json& b = (*made)["cells"];
      if (a.size() != b.size()) report("cells: " + std::to_string(a.size()) + " vs " + std::to_string(b.size()));
      for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        if (a[i] != b[i]) report("cell " + std::to_string(i) + ": " + a[i].dump() + " vs " + b[i].dump());
    }
    const json& ua = expected["units"];
    const json& ub = (*made)["units"];
    if (ua.size() != ub.size()) report("units: " + std::to_string(ua.size()) + " vs " + std::to_string(ub.size()));
    for (size_t i = 0; i < ua.size() && i < ub.size(); ++i) {
      std::set<std::string> keys;
      for (const auto& [k, v] : ua[i].items()) keys.insert(k);
      for (const auto& [k, v] : ub[i].items()) keys.insert(k);
      keys.erase("reach");
      for (const std::string& k : keys) {
        const json a = ua[i].contains(k) ? ua[i][k] : json(nullptr), b = ub[i].contains(k) ? ub[i][k] : json(nullptr);
        if (a != b) report("unit " + std::to_string(i) + " (" + str(ua[i], "name") + ") " + k + ": " + a.dump().substr(0, 200) + " vs " + b.dump().substr(0, 200));
      }
    }
    if (differences) {
      logError("battle export {}: {} differences", stem, differences);
      ++failed;
    } else {
      logInfo("battle export {}: matches the exporter ({} units, {} cells, {} locks, {} exits)", stem, ub.size(), (*made)["cells"].size(), (*made)["locks"].size(), (*made)["exits"].size());
    }
  }
  return failed;
}

}  // namespace ember
