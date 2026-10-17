#include "world/Battle.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Log.h"

namespace ember::e2 {

namespace {

using json = nlohmann::json;

int integer(const json& j, const char* key, int fallback = 0) { return j.contains(key) && j[key].is_number() ? j[key].get<int>() : fallback; }
std::string text(const json& j, const char* key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); }
double number(const json& j, const char* key, double fallback = 0) { return j.contains(key) && j[key].is_number() ? j[key].get<double>() : fallback; }
bool yes(const json& j, const char* key) { return j.contains(key) && j[key].is_boolean() && j[key].get<bool>(); }

// pathfinding.ts footprintFrontRow for a unit with no authored shape: `width` hexes centred on (x, y).
void frontRow(int x, int y, int width, std::vector<BattleHex>& out) {
  const int start = x - (width - 1) / 2;
  for (int i = 0; i < width; ++i) out.push_back({start + i, y});
}

// pathfinding.ts footprint for a body of `size` with an optional authored shape.
std::vector<BattleHex> bodyAt(int size, const std::optional<std::vector<BattleHex>>& offsets, int width, int height, int x, int y) {
  const int s = std::max(1, size);
  if (s <= 1) return {{x, y}};
  std::vector<BattleHex> out;
  if (offsets) {
    // Offsets are authored for an even anchor row; on an odd one an odd-dy row shifts one column right.
    const int shift = y & 1;
    for (const BattleHex& o : *offsets) out.push_back({x + o.x + ((o.y & 1) ? shift : 0), y + o.y});
    return out;
  }
  if (s >= 4) {
    for (int dy = 0; dy > -height; --dy) frontRow(dy % 2 != 0 ? x - 1 : x, y + dy, width, out);
    return out;
  }
  out.push_back({x, y});
  const size_t want = s <= 2 ? 2 : static_cast<size_t>(std::min(4, s));
  for (const BattleHex& n : Battle::neighbors(x, y)) {
    out.push_back(n);
    if (out.size() >= want) break;
  }
  return out;
}

bool fliesOverWater(const BattleUnit& u, const BattleCell& c) {
  // Only these two classes cross water; a blocking decoration on the water still stops them.
  return c.terrain == "water" && (u.classId == "swampBlueCalf" || u.classId == "roccoTheBird") && !c.blocked;
}

bool isTroll(const BattleUnit& u) { return u.classId == "troll" || u.classId == "troll2"; }

// Every class's own movement (Engine2's class table, tools/scripts/engine2/export_classes.ts), read once: -1 = unknown.
int classMov(const std::string& classId) {
  static const json table = [] {
    std::ifstream in("content/engine2/classes.json", std::ios::binary);
    json j = in ? json::parse(in, nullptr, false) : json();
    if (!in || j.is_discarded() || !j.contains("classes")) {
      logError("battle: cannot read content/engine2/classes.json (export it with tools/scripts/engine2/export_classes.ts)");
      return json::object();
    }
    return j["classes"];
  }();
  return table.contains(classId) ? integer(table[classId], "mov", -1) : -1;
}

}  // namespace

std::optional<Battle> Battle::load(const std::filesystem::path& battleJson) {
  std::ifstream in(battleJson, std::ios::binary);
  const json j = in ? json::parse(in, nullptr, false) : json();
  if (!in || j.is_discarded() || !j.contains("cells") || !j.contains("units")) {
    logError("battle: cannot read {} (export it with tools/scripts/engine2/export_battle.ts)", battleJson.string());
    return std::nullopt;
  }
  Battle b;
  b.map_ = text(j, "map");
  b.debugFreeCast_ = j.value("debugFreeCast", false);
  b.roaming_ = j.value("explore", false);
  // A file from before `explore` / `baseMov`: an explore mission shows as every unit carrying the whole board as its
  // movement (Engine2's free-roam reach). It roams like a new file, each unit moving as its class does, so every map
  // behaves the same whenever its file was made.
  bool olderExplore = false;
  if (!j.contains("explore") && j["units"].is_array() && !j["units"].empty()) {
    const int board = integer(j, "cols") * integer(j, "rows");
    olderExplore = board > 0 && std::all_of(j["units"].begin(), j["units"].end(), [&](const json& u) { return integer(u, "mov") == board; });
    if (olderExplore) {
      b.roaming_ = true;
      logInfo("battle {}: an explore file from before baseMov: each unit's movement from its class", b.map_);
    }
  }
  b.cols_ = integer(j, "cols");
  b.rows_ = integer(j, "rows");
  for (const json& c : j["cells"]) {
    BattleCell o;
    o.terrain = text(c, "terrain");
    o.passable = yes(c, "passable");
    o.moveCost = integer(c, "moveCost", 1);
    o.def = integer(c, "def");
    o.atk = integer(c, "atk");
    o.blocked = yes(c, "blocked");
    o.cover = integer(c, "cover");
    o.hazardDice = integer(c, "hazardDice");
    o.hazardFaces = integer(c, "hazardFaces", 8);
    if (c.contains("raw")) {
      const json& r = c["raw"];
      o.raw = {integer(r, "def"), integer(r, "atk"), integer(r, "height"), integer(r, "cover"), yes(r, "blocksShot")};
    }
    if (c.contains("shot")) o.shot = {integer(c["shot"], "height"), yes(c["shot"], "blocksShot")};
    b.cells_.push_back(std::move(o));
  }
  if (b.cells_.size() != static_cast<size_t>(b.cols_ * b.rows_)) {
    logError("battle: {} has {} cells for a {}x{} board", battleJson.string(), b.cells_.size(), b.cols_, b.rows_);
    return std::nullopt;
  }
  for (const json& u : j["units"]) {
    BattleUnit o;
    o.id = text(u, "id");
    o.side = text(u, "side");
    o.name = text(u, "name");
    o.classId = text(u, "classId");
    o.className = text(u, "className");
    o.sprite = text(u, "sprite");
    o.x = integer(u, "x");
    o.y = integer(u, "y");
    o.alive = yes(u, "alive");
    // An explore mission's file gives the free-roam reach as "mov" and the unit's own as "baseMov".
    o.mov = u.contains("baseMov") ? integer(u, "baseMov") : integer(u, "mov");
    if (olderExplore && !u.contains("baseMov")) {
      const int own = classMov(text(u, "classId"));
      if (own >= 0) o.mov = own;
      else logError("battle {}: class '{}' of {} is not in content/engine2/classes.json", b.map_, text(u, "classId"), text(u, "name"));
    }
    o.hp = integer(u, "hp");
    o.maxHp = integer(u, "maxHp");
    o.atk = integer(u, "atk");
    o.def = integer(u, "def");
    o.mag = integer(u, "mag");
    o.dex = integer(u, "dex");
    o.minRange = integer(u, "minRange", 1);
    o.maxRange = integer(u, "maxRange", 1);
    o.size = integer(u, "size", 1);
    o.initiativeBonus = integer(u, "initiativeBonus");
    o.level = integer(u, "level", 1);
    o.summoned = yes(u, "summoned");
    o.undead = yes(u, "undead");
    if (u.contains("combat") && u["combat"].is_object()) {
      const json& c = u["combat"];
      CombatProfile& p = o.combat;
      if (c.contains("weapon") && c["weapon"].is_object()) {
        const json& w = c["weapon"];
        p.hasWeapon = true;
        p.dice = integer(w, "dice"), p.faces = integer(w, "faces"), p.bonus = integer(w, "bonus");
        p.ranged = yes(w, "ranged"), p.classBonus = yes(w, "classBonus");
      }
      p.enh = integer(c, "weaponEnh");
      if (c.contains("mastery")) p.accuracy = number(c["mastery"], "accuracy", 100), p.damageMul = number(c["mastery"], "damage", 1);
      if (c.contains("offMastery")) p.offAccuracy = number(c["offMastery"], "accuracy", 100), p.offDamageMul = number(c["offMastery"], "damage", 1);
      if (c.contains("offHand") && c["offHand"].is_object()) {
        const json& h = c["offHand"];
        p.offHandWeapon = text(h, "kind") == "weapon";
        p.offHandShield = text(h, "kind") == "shield";
        p.offMinRange = integer(h, "minRange", 1), p.offMaxRange = integer(h, "maxRange", 1);
        p.offDice = integer(h, "dice", 1), p.offFaces = integer(h, "faces", 4), p.offBonus = integer(h, "bonus");
      }
      if (c.contains("staff") && c["staff"].is_object()) {
        p.staff = true;
        p.staffElement = text(c["staff"], "element");
        p.staffBonusPct = number(c["staff"], "bonusPct");
        p.staffLifeStealPct = number(c["staff"], "lifeSteal");
        if (c["staff"].contains("bonuses") && c["staff"]["bonuses"].is_object())
          for (const auto& [k, v] : c["staff"]["bonuses"].items())
            if (v.is_number()) p.staffBonuses.emplace_back(k, v.get<double>());
      }
      p.arcane = yes(c, "arcane"), p.arrow = yes(c, "arrow");
      if (c.contains("resistances") && c["resistances"].is_object())
        for (const auto& [k, v] : c["resistances"].items())
          if (v.is_number()) p.resistances.emplace_back(k, v.get<double>());
      if (c.contains("faceDx") && c["faceDx"].is_number()) o.faceDx = c["faceDx"].get<double>();
      if (c.contains("faceDy") && c["faceDy"].is_number()) o.faceDy = c["faceDy"].get<double>();
      o.facing = integer(c, "facing", 1);
    }
    if (u.contains("footprintOffsets") && u["footprintOffsets"].is_array()) {
      std::vector<BattleHex> offsets;
      for (const json& f : u["footprintOffsets"]) offsets.push_back({integer(f, "dx"), integer(f, "dy")});
      o.footprintOffsets = std::move(offsets);
    }
    if (u.contains("spells") && u["spells"].is_object())
      for (int t = 0; t < 10; ++t) o.spells[t] = integer(u["spells"], ("tier" + std::to_string(t + 1)).c_str());
    if (u.contains("spellList") && u["spellList"].is_array())
      for (const json& k : u["spellList"])
        if (k.is_string()) o.spellList.push_back(k.get<std::string>());
    o.shockCharges = integer(u, "shockCharges");
    o.frostCharges = integer(u, "frostCharges");
    o.fantomForceCharges = integer(u, "fantomForceCharges");
    if (u.contains("fantomForceDice") && u["fantomForceDice"].is_object())
      o.fantomDice = integer(u["fantomForceDice"], "dice"), o.fantomFaces = integer(u["fantomForceDice"], "faces");
    o.xp = integer(u, "xp");
    o.guaranteedDrop = yes(u, "guaranteedDrop");
    o.weaponType = text(u, "weaponType");
    o.trainsWeapon = yes(u, "trainsWeapon");
    o.weaponSkill = number(u, "weaponSkill");
    o.healingSkill = number(u, "healingSkill");
    o.offWeaponType = text(u, "offWeaponType");
    o.offTrainsWeapon = yes(u, "offTrainsWeapon");
    o.offWeaponSkill = number(u, "offWeaponSkill");
    if (u.contains("bag") && u["bag"].is_object())
      for (int k = 0; k < 7; ++k) o.bag[k] = integer(u["bag"], kPotionIds[k]);
    o.diseased = yes(u, "diseased");
    if (u.contains("diseaseBase") && u["diseaseBase"].is_object()) {
      const json& d = u["diseaseBase"];
      o.diseaseBase = BattleUnit::Stats{integer(d, "atk"), integer(d, "mag"), integer(d, "def"), integer(d, "dex"), integer(d, "mov")};
    }
    o.poisoned = yes(u, "poisoned");
    o.poisonTier = text(u, "poisonTier");
    if (u.contains("fullness") && u["fullness"].is_number()) o.fullness = u["fullness"].get<double>();
    o.hungerPenaltyPct = number(u, "hungerPenaltyPct");
    o.stunned = yes(u, "stunned"), o.stunTurns = integer(u, "stunTurns");
    o.asleep = yes(u, "asleep"), o.sleepTurns = integer(u, "sleepTurns");
    o.bleeding = yes(u, "bleeding"), o.crippled = yes(u, "crippled");
    if (u.contains("bleedRoundsLeft") && u["bleedRoundsLeft"].is_number()) o.bleedRoundsLeft = u["bleedRoundsLeft"].get<int>();
    o.fearTurns = integer(u, "fearTurns"), o.fearSourceId = text(u, "fearSourceId");
    o.blessedHitBonusPct = number(u, "blessedHitBonusPct"), o.blessedRoundsLeft = integer(u, "blessedRoundsLeft");
    o.summonerId = text(u, "summonerId"), o.spellCharges = integer(u, "spellCharges");
    if (u.contains("lifeDrainCharges") && u["lifeDrainCharges"].is_number()) o.lifeDrainCharges = u["lifeDrainCharges"].get<int>();
    if (u.contains("growth") && u["growth"].is_array()) {
      for (const json& row : u["growth"]) {
        std::array<int, 16> r{};
        for (size_t i = 0; i < 16 && i < row.size(); ++i) r[i] = row[i].get<int>();
        o.growth.push_back(r);
      }
      if (o.level >= 1 && static_cast<size_t>(o.level) <= o.growth.size()) {
        const std::array<int, 16>& g = o.growth[static_cast<size_t>(o.level - 1)];
        o.gearHp = o.maxHp - g[0], o.gearAtk = o.atk - g[1], o.gearMag = o.mag - g[2], o.gearDef = o.def - g[3], o.gearDex = o.dex - g[4];
        o.gearMov = o.mov - g[15];
      }
    }
    o.footprintW = integer(u, "footprintW", 2);
    o.footprintH = integer(u, "footprintH", 4);
    if (u.contains("reach") && u["reach"].is_array())
      for (const json& r : u["reach"])
        if (r.is_array() && r.size() == 5)
          o.reference.push_back({r[0].get<int>(), r[1].get<int>(), r[2].get<int>(), r[3].get<int>(), r[4].get<int>()});
    b.units_.push_back(std::move(o));
  }
  if (j.contains("affinityBonus") && j["affinityBonus"].is_object())
    for (const auto& [a, row] : j["affinityBonus"].items())
      for (const auto& [n, v] : row.items())
        if (v.is_number()) b.affinity_.emplace_back(a, n, v.get<double>());
  if (j.contains("forecasts"))
    for (const json& f : j["forecasts"])
      if (f.is_array() && f.size() == 11)
        b.forecastRefs_.push_back({f[0].get<int>(), f[1].get<int>(), f[2].get<int>(), f[3].get<int>(), f[4].get<int>(), f[5].get<int>(),
                                   f[6].get<double>(), f[7].get<double>(), f[8].get<int>() != 0, f[9].get<int>() != 0, f[10].get<int>() != 0});
  if (j.contains("rolls"))
    for (const json& r : j["rolls"])
      if (r.is_array() && r.size() == 11)
        b.rollRefs_.push_back({r[0].get<int>(), r[1].get<int>(), r[2].get<int>(), r[3].get<int>(), r[4].get<uint32_t>(), r[5].get<int>(),
                               r[6].get<int>() != 0, r[7].get<int>() != 0, r[8].get<double>(), r[9].get<int>(), r[10].get<int>()});
  if (j.contains("casts"))
    for (const json& c : j["casts"]) {
      if (!c.is_array() || c.size() < 7) continue;
      CastReference r;
      r.caster = c[0].get<int>();
      r.kind = c[1].get<std::string>();
      r.cell = {c[2].get<int>(), c[3].get<int>()};
      r.valid = c[4].get<int>() != 0;
      r.at = {c[5].get<int>(), c[6].get<int>()};
      if (r.valid && c.size() >= 13) {
        r.seed = c[7].get<uint32_t>();
        r.calls = c[8].get<int>();
        for (const json& a : c[9]) {
          CastReference::After o;
          o.hp = a[0].get<int>(), o.alive = a[1].get<int>() != 0;
          if (a[2].is_array()) o.shock = BattleUnit::Echo{a[2][0].get<int>(), a[2][1].get<int>(), a[2][2].get<int>(), a[2][3].get<double>()};
          o.maxHp = a[3].get<int>(), o.level = a[4].get<int>(), o.xp = a[5].get<int>();
          r.after.push_back(o);
        }
        for (int t = 0; t < 10; ++t) r.spells[t] = integer(c[10], ("tier" + std::to_string(t + 1)).c_str());
        r.shockCharges = c[11].get<int>(), r.frostCharges = c[12].get<int>();
      }
      b.castRefs_.push_back(std::move(r));
    }
  if (j.contains("skillCasts"))
    for (const json& c : j["skillCasts"]) {
      if (!c.is_array() || c.size() < 7) continue;
      CastReference r;
      r.skill = true;
      r.caster = c[0].get<int>();
      r.kind = c[1].get<std::string>();
      r.cell = {c[2].get<int>(), c[3].get<int>()};
      r.valid = c[4].get<int>() != 0;
      r.at = {c[5].get<int>(), c[6].get<int>()};
      if (r.valid && c.size() >= 13) {
        r.seed = c[7].get<uint32_t>();
        r.calls = c[8].get<int>();
        for (const json& u : c[9]) {
          std::vector<double> v;
          for (const json& x : u) v.push_back(x.get<double>());
          r.ext.push_back(std::move(v));
        }
        r.zones = c[10], r.enmity = c[11], r.unitCount = c[12].get<int>();
        if (c.size() > 13 && c[13].is_array() && c[13].size() >= 2) r.patchTier = c[13][0].get<int>(), r.patchValue = c[13][1].get<int>();
      }
      b.castRefs_.push_back(std::move(r));
    }
  if (j.contains("turnStarts"))
    for (const json& c : j["turnStarts"]) {
      if (!c.is_array() || c.size() < 9) continue;
      TurnStartReference r;
      r.unit = c[0].get<int>(), r.scenario = c[1].get<std::string>(), r.seed = c[2].get<uint32_t>(), r.calls = c[3].get<int>();
      for (const json& u : c[4]) {
        std::vector<double> v;
        for (const json& x : u) v.push_back(x.get<double>());
        r.ext.push_back(std::move(v));
      }
      r.zones = c[5], r.enmity = c[6];
      r.moved = c[7][0].get<int>() != 0, r.acted = c[7][1].get<int>() != 0, r.restrained = c[7][2].get<int>() != 0;
      for (const json& p : c[8]) r.flee.push_back({p[0].get<int>(), p[1].get<int>()});
      b.turnRefs_.push_back(std::move(r));
    }
  // The support casts and potions, in the hurt scene: [caster, kind, x, y, valid, at x, at y, seed, calls, after, spells,
  // shock, frost, lootRations, potion]; each unit afterwards as [hp, alive, echo, maxHp, level, xp, atk, mag, def, dex,
  // mov, diseased, poisoned, fullness, spells, bag].
  if (j.contains("supportCasts"))
    for (const json& c : j["supportCasts"]) {
      if (!c.is_array() || c.size() < 7) continue;
      CastReference r;
      r.hurt = true;
      r.caster = c[0].get<int>();
      r.kind = c[1].get<std::string>();
      r.cell = {c[2].get<int>(), c[3].get<int>()};
      r.valid = c[4].get<int>() != 0;
      r.at = {c[5].get<int>(), c[6].get<int>()};
      if (c.size() >= 15 && c[14].is_string()) r.potion = c[14].get<std::string>();
      if (r.valid && c.size() >= 14) {
        r.seed = c[7].get<uint32_t>();
        r.calls = c[8].get<int>();
        for (const json& a : c[9]) {
          CastReference::After o;
          o.hp = a[0].get<int>(), o.alive = a[1].get<int>() != 0;
          if (a[2].is_array()) o.shock = BattleUnit::Echo{a[2][0].get<int>(), a[2][1].get<int>(), a[2][2].get<int>(), a[2][3].get<double>()};
          o.maxHp = a[3].get<int>(), o.level = a[4].get<int>(), o.xp = a[5].get<int>();
          if (a.size() >= 16) {
            o.hasStatus = true;
            o.atk = a[6].get<int>(), o.mag = a[7].get<int>(), o.def = a[8].get<int>(), o.dex = a[9].get<int>(), o.mov = a[10].get<int>();
            o.diseased = a[11].get<int>() != 0, o.poisoned = a[12].get<int>() != 0;
            o.fullness = a[13].is_number() ? a[13].get<double>() : 100;
            for (int t = 0; t < 10; ++t) o.spells[t] = integer(a[14], ("tier" + std::to_string(t + 1)).c_str());
            for (int k = 0; k < 7; ++k) o.bag[k] = integer(a[15], kPotionIds[k]);
          }
          r.after.push_back(o);
        }
        for (int t = 0; t < 10; ++t) r.spells[t] = integer(c[10], ("tier" + std::to_string(t + 1)).c_str());
        r.shockCharges = c[11].get<int>(), r.frostCharges = c[12].get<int>();
        r.lootRations = c[13].get<int>();
      }
      b.castRefs_.push_back(std::move(r));
    }
  auto readAi = [&](const json& list, bool near) {
    for (const json& a : list) {
      AiReference r;
      r.unit = integer(a, "unit");
      if (a.contains("spells") && a["spells"].is_array())
        for (const json& s : a["spells"]) {
          AiReference::Cast cast;
          cast.kind = text(s, "kind");
          for (const json& i : s["ids"]) cast.ids.push_back(i.get<int>());
          for (const json& t : s["tiles"]) cast.tiles.push_back({t[0].get<int>(), t[1].get<int>()});
          r.casts.push_back(std::move(cast));
        }
      if (near && a.contains("at") && a["at"].is_array()) r.at = BattleHex{a["at"][0].get<int>(), a["at"][1].get<int>()};
      if (a.contains("path") && a["path"].is_array())
        for (const json& p : a["path"]) r.plan.path.push_back({p[0].get<int>(), p[1].get<int>()});
      r.plan.target = integer(a, "target", -1);
      r.spell = !text(a, "other").empty() || !r.casts.empty();
      b.aiRefs_.push_back(std::move(r));
    }
  };
  if (j.contains("ai") && j["ai"].is_array()) readAi(j["ai"], false);
  if (j.contains("aiNear") && j["aiNear"].is_array()) readAi(j["aiNear"], true);
  return b;
}

std::vector<BattleHex> Battle::neighbors(int x, int y) {
  static constexpr int even[6][2] = {{1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}};
  static constexpr int odd[6][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {0, 1}, {1, 1}};
  const auto& d = (y & 1) ? odd : even;
  std::vector<BattleHex> out;
  out.reserve(6);
  for (const auto& o : d) out.push_back({x + o[0], y + o[1]});
  return out;
}

int Battle::distance(BattleHex a, BattleHex b) {
  const int aq = a.x - (a.y - (a.y & 1)) / 2, bq = b.x - (b.y - (b.y & 1)) / 2;
  const int dq = aq - bq, dr = a.y - b.y;
  return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

std::vector<BattleHex> Battle::footprint(const BattleUnit& u, int x, int y) {
  return bodyAt(u.size, u.footprintOffsets, u.footprintW, u.footprintH, x, y);
}

std::vector<int> Battle::occupancy() const {
  std::vector<int> occ(cells_.size(), -1);
  for (size_t i = 0; i < units_.size(); ++i) {
    if (!units_[i].alive) continue;
    for (const BattleHex& p : footprint(units_[i], units_[i].x, units_[i].y))
      if (inBounds(p.x, p.y)) occ[static_cast<size_t>(p.y * cols_ + p.x)] = static_cast<int>(i);  // a later unit wins, as there
  }
  return occ;
}

int Battle::unitAt(int x, int y) const {
  for (size_t i = 0; i < units_.size(); ++i) {
    if (!units_[i].alive) continue;
    for (const BattleHex& p : footprint(units_[i], units_[i].x, units_[i].y))
      if (p.x == x && p.y == y) return static_cast<int>(i);
  }
  return -1;
}

// pathfinding.ts footprintCost: what stepping the unit's anchor onto (x, y) costs, or nothing when it cannot.
std::optional<int> Battle::stepCost(int x, int y, const BattleUnit& self, const std::vector<int>& occ, bool stop) const {
  // A body with an authored shape checks only its front row for movement (its trailing rows never turn with the
  // walk, and would lock it in place near an edge); others check their whole body (the default size rectangle).
  std::vector<BattleHex> body;
  if (self.footprintOffsets) {
    for (const BattleHex& o : *self.footprintOffsets)
      if (o.y == 0) body.push_back({x + o.x, y});
  } else {
    body = bodyAt(self.size, std::nullopt, 2, 4, x, y);
  }
  int cost = 1;
  for (const BattleHex& p : body) {
    if (!inBounds(p.x, p.y)) return std::nullopt;
    const BattleCell& c = cell(p.x, p.y);
    const bool flies = fliesOverWater(self, c);
    if (!c.passable && !flies && !(c.terrain == "barricade" && isTroll(self))) return std::nullopt;
    const int here = flies ? 1 : c.terrain == "barricade" && isTroll(self) ? 2 : c.moveCost;
    cost = std::max(cost, here);
    const int who = occ[static_cast<size_t>(p.y * cols_ + p.x)];
    if (who < 0 || units_[static_cast<size_t>(who)].id == self.id) continue;
    const BattleUnit& other = units_[static_cast<size_t>(who)];
    if (other.side != self.side) return std::nullopt;
    // A big creature's body is never walked into or through, friend or foe.
    if (other.footprintOffsets) return std::nullopt;
    if (stop) return std::nullopt;
  }
  return cost;
}

std::vector<ReachCell> Battle::reachable(size_t unit, bool pruneStops) const {
  const BattleUnit& u = units_[unit];
  // Free roam (Engine2 effectiveUnitForReach on an explore mission): any reachable hex, however far.
  // effectiveUnitForReach: a unit whose cell was webbed when its turn began (turnRestrained) walks one hex at most.
  const int remaining = std::max(0, u.mov - u.moveUsed);
  const int mov = roaming_ ? cols_ * rows_ : turnRestrained_ ? std::min(1, remaining) : remaining;
  const std::vector<int> occ = occupancy();
  // The reach in Engine2's insertion order (a Map there): `at` finds a hex's entry.
  std::vector<ReachCell> result;
  std::vector<int> at(cells_.size(), -1);
  auto slot = [&](int x, int y) -> int* { return inBounds(x, y) ? &at[static_cast<size_t>(y * cols_ + x)] : nullptr; };
  result.push_back({u.x, u.y, 0, -1, -1});
  if (int* s = slot(u.x, u.y)) *s = 0;
  // Dijkstra exactly as there: the queue re-sorted by cost before every pop (a stable sort: ties keep their order).
  std::vector<ReachCell> queue{result.front()};
  while (!queue.empty()) {
    std::stable_sort(queue.begin(), queue.end(), [](const ReachCell& a, const ReachCell& b) { return a.cost < b.cost; });
    const ReachCell cur = queue.front();
    queue.erase(queue.begin());
    if (cur.cost >= mov) continue;
    for (const BattleHex& n : neighbors(cur.x, cur.y)) {
      const std::optional<int> step = stepCost(n.x, n.y, u, occ, false);
      if (!step) continue;
      const int next = cur.cost + *step;
      if (next > mov) continue;
      int* s = slot(n.x, n.y);  // a legal step is always on the board
      if (*s >= 0 && result[static_cast<size_t>(*s)].cost <= next) continue;
      const ReachCell c{n.x, n.y, next, cur.x, cur.y};
      if (*s >= 0) {
        result[static_cast<size_t>(*s)] = c;  // Map.set keeps the key's place
      } else {
        *s = static_cast<int>(result.size());
        result.push_back(c);
      }
      queue.push_back(c);
    }
  }
  if (!pruneStops) return result;
  // Hexes it may pass through but not stop on (a friend stands there) leave the reach.
  std::vector<ReachCell> kept;
  for (const ReachCell& c : result)
    if ((c.x == u.x && c.y == u.y) || stepCost(c.x, c.y, u, occ, true)) kept.push_back(c);
  return kept;
}

std::vector<BattleHex> Battle::path(const std::vector<ReachCell>& reach, int x, int y) {
  std::vector<BattleHex> out;
  auto find = [&](int cx, int cy) -> const ReachCell* {
    for (const ReachCell& c : reach)
      if (c.x == cx && c.y == cy) return &c;
    return nullptr;
  };
  for (const ReachCell* c = find(x, y); c && out.size() <= reach.size(); c = c->parentX >= 0 ? find(c->parentX, c->parentY) : nullptr)
    out.push_back({c->x, c->y});
  std::reverse(out.begin(), out.end());
  return out;
}

bool Battle::move(size_t unit, int x, int y) {
  BattleUnit& u = units_[unit];
  for (const ReachCell& c : reachable(unit))
    if (c.x == x && c.y == y) {
      u.x = x, u.y = y;
      u.moveUsed += c.cost;
      return true;
    }
  return false;
}

double Battle::affinityBonus(const std::string& a, const std::string& b) const {
  for (const auto& [x, y, v] : affinity_)
    if (x == a && y == b) return v;
  return 0;
}

bool Battle::isWebCell(int x, int y) const {
  for (const WebZone& z : webZones_)
    if (std::any_of(z.cells.begin(), z.cells.end(), [&](const BattleHex& c) { return c.x == x && c.y == y; })) return true;
  return false;
}

double Battle::webCellSleepChance(int x, int y) const {
  for (const WebZone& z : webZones_)
    if (std::any_of(z.cells.begin(), z.cells.end(), [&](const BattleHex& c) { return c.x == x && c.y == y; })) return z.sleepChance;
  return 0.25;  // WEB_OF_DREAMS.sleepChance
}

double Battle::zoneDamageMul(const BattleUnit& defender) const {
  double mul = 1;
  for (const AuraZone& z : auraZones_) {
    if (!std::any_of(z.cells.begin(), z.cells.end(), [&](const BattleHex& c) { return c.x == defender.x && c.y == defender.y; })) continue;
    if (z.kind == "protection" && z.side == defender.side) mul *= 1 - z.pct;
    if (z.kind == "intimidation" && z.side != defender.side) mul *= 1 + z.pct;
  }
  return mul;
}

void Battle::newTurn() {
  for (BattleUnit& u : units_) u.moveUsed = 0, u.acted = false;
}

}  // namespace ember::e2
