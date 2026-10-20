// Engine2's remaining skills and spells (game/engine.ts start* / cast* / spellAimValid, game/data.ts power tables), ported
// step for step. Included by Spells.cpp inside ember::e2's first anonymous namespace, after its helpers (rollDice, occAt,
// piercingRay, hexesWithin, spendTier ...). Cooker check-battle casts each one at every hex and compares with Engine2.

// --- Level tables (data.ts) ------------------------------------------------------------------------------------------

struct DiceFaces {
  int dice = 0, faces = 0;
};
double bloodyShotMul(int level) { return level >= 13 ? 2.5 : level >= 10 ? 2.25 : level >= 6 ? 2 : 1.5; }
DiceFaces bloodyShotBleed(int level) {
  if (level >= 15) return {2, 6};
  if (level >= 12) return {2, 4};
  if (level >= 8) return {1, 8};
  return {1, 6};
}
constexpr int kBloodyShotRange = 6, kMultiShotRange = 6;
struct ProvokeP {
  int range, radius;
};
ProvokeP provokePower(int level) {
  if (level >= 15) return {8, 4};
  if (level >= 12) return {7, 3};
  if (level >= 9) return {6, 2};
  if (level >= 6) return {5, 1};
  return {4, 0};
}
constexpr int kProvokeUnlock = 3;
constexpr double kProvokeCe = 1, kProvokeVe = 1800;  // ENMITY.provoke
DiceFaces cleavePower(int level) {
  if (level >= 14) return {2, 8};
  if (level >= 11) return {2, 6};
  if (level >= 9) return {2, 4};
  return {1, 8};
}
constexpr int kCleaveHexes = 3, kCleaveLargeHexes = 3;
constexpr double kCleaveLargeMul = 2;
struct BullRushP {
  int chargeRange, dice, faces, knockback, wallDice, wallFaces;
};
BullRushP bullRushPower(int level) {
  if (level >= 27) return {4, 2, 12, 3, 2, 10};
  if (level >= 23) return {4, 2, 10, 2, 2, 8};
  if (level >= 19) return {4, 2, 8, 2, 2, 8};
  if (level >= 16) return {4, 2, 6, 2, 2, 6};
  if (level >= 13) return {3, 2, 6, 2, 2, 6};
  if (level >= 10) return {3, 1, 10, 1, 1, 8};
  if (level >= 7) return {3, 1, 8, 1, 1, 8};
  if (level >= 4) return {3, 1, 6, 1, 1, 6};
  return {2, 1, 4, 1, 1, 4};
}
constexpr int kBullRushRange = 4, kBullRushUnlock = 3;
struct ExecP {
  int dice, faces;
  double threshold, mult;
};
ExecP executionerStrikePower(int level) {
  if (level >= 28) return {3, 10, 0.4, 2.25};
  if (level >= 25) return {2, 12, 0.4, 2.0};
  if (level >= 21) return {2, 10, 0.35, 2.0};
  if (level >= 17) return {2, 8, 0.35, 1.75};
  if (level >= 14) return {2, 6, 0.3, 1.75};
  if (level >= 11) return {1, 10, 0.3, 1.5};
  return {1, 8, 0.3, 1.5};
}
struct ShieldP {
  int dice, faces, stunTurns;
};
ShieldP shieldBashPower(int level) {
  if (level >= 27) return {2, 12, 3};
  if (level >= 23) return {2, 10, 3};
  if (level >= 19) return {2, 8, 2};
  if (level >= 16) return {2, 6, 2};
  if (level >= 13) return {1, 10, 2};
  if (level >= 10) return {1, 8, 1};
  if (level >= 7) return {1, 6, 1};
  return {1, 4, 1};
}
int multiShotTargets(int level) { return level >= 11 ? 3 : 2; }
DiceFaces multiShotPower(int level) {
  if (level >= 13) return {2, 4};
  if (level >= 8) return {1, 4};
  return {0, 0};
}
DiceFaces divineWrathPower(int level) {
  if (level >= 30) return {3, 10};
  if (level >= 26) return {3, 8};
  if (level >= 22) return {2, 10};
  if (level >= 19) return {2, 8};
  return {1, 10};
}
struct SmashP {
  int dice, faces, hexes;
};
SmashP shoulderSmashPower(int level) {
  if (level >= 28) return {2, 12, 4};
  if (level >= 24) return {2, 10, 3};
  if (level >= 20) return {2, 8, 2};
  if (level >= 16) return {1, 10, 2};
  return {1, 8, 1};
}
constexpr int kShoulderSmashKnockback = 2, kWrathRange = 4;
DiceFaces stampedePower(int level) {
  if (level >= 30) return {3, 10};
  if (level >= 27) return {3, 8};
  if (level >= 24) return {2, 10};
  if (level >= 21) return {2, 8};
  return {1, 10};
}
constexpr int kSweepRadius = 2, kSweepKnockback = 1;
constexpr int kTripBonusFaces = 8, kTripBonusBonus = 0, kTripStunRounds = 2;
constexpr double kTripStatPenalty = 0.1;
// fireball / venoms / divine bolt
constexpr int kFireballRange = 5, kFireballSize = 2, kFireballDice = 2, kFireballFaces = 6, kFireballBonus = 0;
constexpr double kFireballMul = 1.35;
struct BoltData {
  int size, range, centerDice, centerFaces, centerBonus, splashDice, splashFaces, splashBonus;
  double centerMul, splashMul;
};
constexpr BoltData kCausticVenom{3, 7, 1, 10, 0, 1, 6, 0, 1.5, 1.1};
constexpr BoltData kMinorVenom{2, 7, 1, 10, 0, 1, 6, 0, 1.5, 1.1};
constexpr BoltData kDivineBolt{1, 6, 2, 4, 0, 1, 4, 0, 1.4, 1.0};
constexpr int kLightningT3Range = 7, kLightningT3Dice = 3, kLightningT3Faces = 12, kLightningT3Bonus = 8;
constexpr double kLightningT3Mul = 3.0;
constexpr BattleUnit::Echo kLightningT3Echo{2, 12, 8, 0};
constexpr int kPhantasmalRange = 6, kPhantasmalUnlock = 2, kWebRange = 7, kLifeDrainRange = 1;
DiceFaces lifeDrainDice(int level) { return {1, 4 + (std::max(1, level) - 1) / 3 * 2}; }
DiceFaces phantasmalForceDice(int level) { return {1, 4 + (std::max(1, level) - 1) / 2 * 2}; }
double lifeDrainHealMul(int level) {
  if (level >= 30) return 1;
  if (level >= 23) return 0.75;
  if (level >= 17) return 0.6;
  if (level >= 10) return 0.4;
  return 0.25;
}
// ice storm
struct IceStormP {
  int range, size, areaHexes, durationRounds, dice, faces;
  double mul;
};
IceStormP iceStormPower(int level) {
  const int step = std::max(0, std::min(5, (level - 8) / 4 + (level - 8 < 0 ? -1 : 0)));  // Math.floor((level - 8) / 4), clamped
  static const int dice[6] = {1, 1, 2, 2, 3, 3}, faces[6] = {6, 8, 6, 8, 6, 8};
  return {5 + step, 1, level < 12 ? 3 : 7, 2 + step, dice[step], faces[step], 0.65 + step * 0.05};
}
constexpr int kIceStormUnlock = 8;
// burning hands / poison breath
int burningHandsRadius(int level) { return std::min(6, 1 + static_cast<int>(std::floor((std::max(1, level) - 1) * 5.0 / 14.0))); }
struct ConeP {
  int range, radius, dice, faces;
  double mul;
};
ConeP burningHandsPower(int level) {
  const int radius = burningHandsRadius(level);
  if (level >= 25) return {radius, radius, 2, 10, 1.35};
  if (level >= 21) return {radius, radius, 2, 8, 1.3};
  if (level >= 17) return {radius, radius, 2, 6, 1.25};
  if (level >= 13) return {radius, radius, 2, 4, 1.2};
  if (level >= 9) return {radius, radius, 1, 8, 1.15};
  if (level >= 5) return {radius, radius, 1, 6, 1.1};
  return {radius, radius, 1, 4, 1.05};
}
constexpr int kPoisonBreathUnlock = 3, kBurningHandsUnlock = 5;
ConeP poisonBreathPower(int level) {
  const int progression = std::max(1, level - kPoisonBreathUnlock + 1);
  ConeP p = burningHandsPower(std::max(1, progression - 2));
  const int radius = std::max(1, burningHandsRadius(progression) - 1);
  p.range = radius, p.radius = radius;
  return p;
}
// turn undead
struct UndeadP {
  int level, range, radius, dice, faces;
  double mul;
};
constexpr UndeadP kTurnUndead[] = {{6, 4, 4, 4, 6, 1.6}, {9, 5, 5, 5, 6, 1.8}, {12, 6, 6, 6, 6, 2}, {15, 7, 7, 7, 6, 2.2}, {18, 8, 8, 8, 6, 2.4}, {22, 9, 9, 10, 6, 2.6}};
UndeadP turnUndeadPower(int level) {
  const int capped = std::max(1, std::min(22, level));
  UndeadP p = kTurnUndead[0];
  for (const UndeadP& q : kTurnUndead)
    if (capped >= q.level) p = q;
  return p;
}
constexpr int kTurnUndeadFearTurns = 2;
// bless / aura / web
int blessHitBonusPct(int level) { return std::min(10, std::max(1, level - 2)); }
int blessDuration(int level) { return level >= 15 ? 8 : level >= 12 ? 7 : level >= 9 ? 6 : level >= 7 ? 5 : level >= 5 ? 4 : 3; }
constexpr int kBlessRadius = 3, kBlessUnlock = 3;
struct AuraP {
  int radius;
  double pct;
  int duration;
};
AuraP auraPower(int level) {
  if (level >= 30) return {3, 0.35, 4};
  if (level >= 28) return {3, 0.3, 4};
  if (level >= 26) return {3, 0.25, 4};
  if (level >= 24) return {2, 0.25, 4};
  if (level >= 22) return {2, 0.25, 3};
  if (level >= 20) return {2, 0.2, 3};
  return {1, 0.2, 3};
}
int webOfDreamsSize(int level) { return level >= 12 ? 3 : level >= 7 ? 2 : 1; }
double webOfDreamsSleepChance(int level) {
  const double t = std::max(0.0, std::min(1.0, (level - 3) / 27.0));
  return 0.25 + (0.85 - 0.25) * t;
}
constexpr int kWebDuration = 3, kWebSleepDice = 1, kWebSleepFaces = 4;
constexpr double kWebSleepBonusDamage = 0.25;

// --- Geometry (pathfinding.ts) ---------------------------------------------------------------------------------------

struct CubeI {
  int q = 0, r = 0, s = 0;
  bool operator==(const CubeI&) const = default;
};
constexpr CubeI kCubeDirs[6] = {{1, 0, -1}, {1, -1, 0}, {0, -1, 1}, {-1, 0, 1}, {-1, 1, 0}, {0, 1, -1}};
CubeI toCubeI(BattleHex h) {
  const int q = h.x - (h.y - (h.y & 1)) / 2;
  return {q, h.y, -q - h.y};
}
BattleHex fromCubeI(int q, int r) { return {q + (r - (r & 1)) / 2, r}; }
bool inBoardXY(const Battle& b, int x, int y) { return x >= 0 && y >= 0 && x < b.cols() && y < b.rows(); }

std::vector<BattleHex> hexAreaTiles(const Battle& b, BattleHex origin, int radius) {
  std::vector<BattleHex> out;
  for (int y = 0; y < b.rows(); ++y)
    for (int x = 0; x < b.cols(); ++x)
      if (hexDist(origin, {x, y}) <= radius) out.push_back({x, y});
  return out;
}
// axisDir: the exact hex axis from one hex to another, or none.
std::optional<CubeI> axisDir(BattleHex from, BattleHex to) {
  const CubeI A = toCubeI(from), B = toCubeI(to);
  const int dq = B.q - A.q, dr = B.r - A.r, ds = B.s - A.s;
  if (dq == 0 && dr == 0 && ds == 0) return std::nullopt;
  if (dq != 0 && dr != 0 && ds != 0) return std::nullopt;
  const int n = std::max({std::abs(dq), std::abs(dr), std::abs(ds)});
  if (n <= 0) return std::nullopt;
  if (dq % n != 0 || dr % n != 0 || ds % n != 0) return std::nullopt;
  return CubeI{dq / n, dr / n, ds / n};
}
std::vector<BattleHex> hexRay(const Battle& b, BattleHex from, CubeI dir) {
  std::vector<BattleHex> out;
  CubeI c = toCubeI(from);
  c = {c.q + dir.q, c.r + dir.r, c.s + dir.s};
  for (int i = 0; i < b.cols() + b.rows() + 4; ++i) {
    const BattleHex p = fromCubeI(c.q, c.r);
    if (!inBoardXY(b, p.x, p.y)) break;
    out.push_back(p);
    c = {c.q + dir.q, c.r + dir.r, c.s + dir.s};
  }
  return out;
}
// cleaveHexes: `count` hexes of the caster's neighbour ring, from the aimed one round.
std::vector<BattleHex> cleaveHexes(const Battle& b, BattleHex from, BattleHex start, int count) {
  const std::vector<BattleHex> ring = Battle::neighbors(from.x, from.y);
  int i = -1;
  for (size_t k = 0; k < ring.size(); ++k)
    if (ring[k] == start) {
      i = static_cast<int>(k);
      break;
    }
  if (i < 0) return {};
  const int n = std::max(1, std::min(6, count));
  std::vector<BattleHex> out;
  for (int k = 0; k < n; ++k) {
    const BattleHex p = ring[static_cast<size_t>((i + k) % 6)];
    if (inBoardXY(b, p.x, p.y)) out.push_back(p);
  }
  return out;
}
BattleHex stepFromHex(BattleHex p, CubeI dir) {
  const CubeI c = toCubeI(p);
  return fromCubeI(c.q + dir.q, c.r + dir.r);
}
// coneSector: a slim hex cone `radius` rows deep, 1, 3, 3, 5, 5, 7 wide: a single hex next to the caster, opening from there (the user,
// 2026-10-19: "it must start with a single hex and open up to three or else its not a cone"; Engine2's was 3, 3, 5, 5, 7, 7, which
// starts three wide). Shared by Mãos Flamejantes and Sopro Venenoso.
std::vector<BattleHex> coneSector(const Battle& b, BattleHex from, CubeI dir, int radius) {
  int i = -1;
  for (int k = 0; k < 6; ++k)
    if (kCubeDirs[k] == dir) i = k;
  if (i < 0) return {};
  const CubeI left = kCubeDirs[(i + 5) % 6], right = kCubeDirs[(i + 1) % 6];
  const CubeI o = toCubeI(from);
  std::vector<BattleHex> out;
  for (int k = 1; k <= radius; ++k) {
    std::vector<CubeI> row;
    for (int a = k; a >= 0; --a) row.push_back({left.q * a + dir.q * (k - a), left.r * a + dir.r * (k - a), 0});
    for (int a = 1; a <= k; ++a) row.push_back({right.q * a + dir.q * (k - a), right.r * a + dir.r * (k - a), 0});
    const int width = 1 + 2 * (k / 2);
    const int half = (width - 1) / 2;
    for (int j = k - half; j <= k + half; ++j) {
      if (j < 0 || j >= static_cast<int>(row.size())) continue;
      const CubeI c = row[static_cast<size_t>(j)];
      const BattleHex p = fromCubeI(o.q + c.q, o.r + c.r);
      if (inBoardXY(b, p.x, p.y)) out.push_back(p);
    }
  }
  return out;
}
// wrathRay: piercingRay capped to `range`.
std::vector<BattleHex> wrathRay(const Battle& b, BattleHex from, BattleHex through, int range) {
  std::vector<BattleHex> raw = piercingRay(b, from, through);
  if (raw.size() > static_cast<size_t>(range)) raw.resize(static_cast<size_t>(range));
  return raw;
}
// iceStormAreaTiles: the three adjoining hexes below level 12, else the seven-hex ring.
std::vector<BattleHex> iceStormAreaTiles(const Battle& b, BattleHex origin, int level) {
  const std::vector<BattleHex> ring = hexAreaTiles(b, origin, 1);
  if (iceStormPower(level).areaHexes == 7) return ring;
  std::vector<BattleHex> neighbours;
  for (const BattleHex& c : ring)
    if (c.x != origin.x || c.y != origin.y) neighbours.push_back(c);
  for (const BattleHex& first : neighbours) {
    const std::vector<BattleHex> adjoining = hexAreaTiles(b, first, 1);
    for (const BattleHex& cell : neighbours) {
      if (cell == first) continue;
      if (std::any_of(adjoining.begin(), adjoining.end(), [&](const BattleHex& o) { return o == cell; })) return {origin, first, cell};
    }
  }
  std::vector<BattleHex> out(ring.begin(), ring.begin() + std::min<size_t>(3, ring.size()));
  return out;
}
// occupiedHexCount / cleaveDoublesVs
int occupiedHexCount(const BattleUnit& u) {
  if (u.footprintOffsets && !u.footprintOffsets->empty()) return static_cast<int>(u.footprintOffsets->size());
  return std::max(1, u.size);
}
// footprintFrontRow (width 2 for a body with no authored shape).
std::vector<BattleHex> frontRowOf(const BattleUnit& u, int x, int y) {
  std::vector<BattleHex> out;
  if (u.footprintOffsets) {
    for (const BattleHex& o : *u.footprintOffsets)
      if (o.y == 0) out.push_back({x + o.x, y});
    return out;
  }
  const int start = x - (2 - 1) / 2;
  for (int i = 0; i < 2; ++i) out.push_back({start + i, y});
  return out;
}

// knockBack: shoves `foe` one hex further from `att`, doing nothing when that hex is off the board, impassable or taken.
void knockBack(Battle& b, size_t att, size_t foe) {
  const BattleUnit& a = b.units()[att];
  BattleUnit& f = b.units()[foe];
  std::optional<BattleHex> dest;
  int best = hexDist({a.x, a.y}, {f.x, f.y});
  for (const BattleHex& n : Battle::neighbors(f.x, f.y)) {
    if (!inBoardXY(b, n.x, n.y)) continue;
    const int d = hexDist({a.x, a.y}, n);
    if (d > best) best = d, dest = n;
  }
  if (!dest) return;
  if (!b.cell(dest->x, dest->y).passable) return;
  if (firstAt(b, *dest) >= 0) return;
  f.x = dest->x, f.y = dest->y;
}

// --- Bull Rush (bullRushCharge) ----------------------------------------------------------------------------------------

struct RushPush {
  std::vector<BattleHex> path;
  bool blocked = false;
};
struct RushLeg {
  std::vector<BattleHex> path;
  size_t foe = 0;
  RushPush push;
};
using OccMap = std::map<std::pair<int, int>, int>;
OccMap occupancyMap(const Battle& b) {
  OccMap occ;
  for (size_t i = 0; i < b.units().size(); ++i) {
    const BattleUnit& u = b.units()[i];
    if (!u.alive) continue;
    for (const BattleHex& p : Battle::footprint(u, u.x, u.y)) occ[{p.x, p.y}] = static_cast<int>(i);
  }
  return occ;
}
bool chargeTerrainBlocked(const Battle& b, int x, int y) { return !inBoardXY(b, x, y) || !b.cell(x, y).passable; }
bool rushPushFits(const Battle& b, size_t foe, BattleHex to, const OccMap& occ) {
  const BattleUnit& f = b.units()[foe];
  for (const BattleHex& c : frontRowOf(f, to.x, to.y))
    if (chargeTerrainBlocked(b, c.x, c.y)) return false;
  for (const BattleHex& c : Battle::footprint(f, to.x, to.y)) {
    if (!inBoardXY(b, c.x, c.y)) continue;
    const auto it = occ.find({c.x, c.y});
    if (it != occ.end() && it->second >= 0 && b.units()[static_cast<size_t>(it->second)].id != f.id) return false;
  }
  return true;
}
RushPush rushPush(const Battle& b, size_t foe, CubeI dir, const OccMap& occ) {
  const BattleUnit& f = b.units()[foe];
  const int dist = f.footprintOffsets && f.footprintOffsets->size() > 1 ? 1 : 2;
  RushPush out;
  const std::vector<BattleHex> ray = hexRay(b, {f.x, f.y}, dir);
  for (size_t i = 0; i < ray.size() && static_cast<int>(i) < dist; ++i) {
    if (!rushPushFits(b, foe, ray[i], occ)) break;
    out.path.push_back(ray[i]);
  }
  out.blocked = static_cast<int>(out.path.size()) < dist;
  return out;
}
std::optional<std::vector<RushLeg>> bullRushCharge(const Battle& b, size_t casterIdx, BattleHex cell) {
  const BattleUnit& caster = b.units()[casterIdx];
  if (hexDist({caster.x, caster.y}, cell) > kBullRushRange) return std::nullopt;
  OccMap occ = occupancyMap(b);
  const auto tIt = occ.find({cell.x, cell.y});
  if (tIt == occ.end()) return std::nullopt;
  const size_t target = static_cast<size_t>(tIt->second);
  if (!b.units()[target].alive || b.units()[target].side == caster.side) return std::nullopt;
  std::vector<BattleHex> line = hexLine({caster.x, caster.y}, cell);
  line.erase(line.begin());
  std::vector<RushLeg> legs;
  std::vector<BattleHex> run;
  BattleHex at{caster.x, caster.y};
  const bool bigCalf = caster.classId == "bigBlueCalf";
  auto place = [&](size_t u, BattleHex to) {
    const BattleUnit& v = b.units()[u];
    for (const BattleHex& c : Battle::footprint(v, v.x, v.y)) {
      const auto it = occ.find({c.x, c.y});
      if (it != occ.end() && it->second == static_cast<int>(u)) occ.erase(it);
    }
    for (const BattleHex& c : Battle::footprint(v, to.x, to.y)) occ[{c.x, c.y}] = static_cast<int>(u);
  };
  for (int i = 0; i < static_cast<int>(line.size()); ++i) {
    const BattleHex pt = line[static_cast<size_t>(i)];
    if (chargeTerrainBlocked(b, pt.x, pt.y)) return std::nullopt;
    const std::vector<BattleHex> body = Battle::footprint(caster, pt.x, pt.y);
    if (bigCalf && std::any_of(body.begin(), body.end(), [&](const BattleHex& c) { return chargeTerrainBlocked(b, c.x, c.y); })) return std::nullopt;
    int who = -1;
    if (bigCalf) {
      for (const BattleHex& c : body) {
        const auto it = occ.find({c.x, c.y});
        if (it != occ.end() && b.units()[static_cast<size_t>(it->second)].id != caster.id) {
          who = it->second;
          break;
        }
      }
    } else {
      const auto it = occ.find({pt.x, pt.y});
      if (it != occ.end()) who = it->second;
    }
    if (who < 0 || b.units()[static_cast<size_t>(who)].id == caster.id) {
      if (bigCalf)
        for (const BattleHex& c : body) {
          const auto it = occ.find({c.x, c.y});
          if (chargeTerrainBlocked(b, c.x, c.y) || (it != occ.end() && b.units()[static_cast<size_t>(it->second)].id != caster.id)) return std::nullopt;
        }
      run.push_back(pt);
      at = pt;
      continue;
    }
    const BattleUnit& w = b.units()[static_cast<size_t>(who)];
    if (!w.alive || w.side == caster.side) return std::nullopt;
    const std::optional<CubeI> dir = axisDir(at, pt);
    if (!dir) return std::nullopt;
    // The charger's landing hex for this leg (and only that one), so pushes never land on it.
    for (auto it = occ.begin(); it != occ.end();) it = it->second == static_cast<int>(casterIdx) ? occ.erase(it) : std::next(it);
    for (const BattleHex& c : Battle::footprint(caster, at.x, at.y)) occ[{c.x, c.y}] = static_cast<int>(casterIdx);
    if (static_cast<size_t>(who) != target) {
      int di = 0;
      for (int k = 0; k < 6; ++k)
        if (kCubeDirs[k] == *dir) di = k;
      std::set<std::pair<int, int>> ahead;
      for (size_t k = static_cast<size_t>(i); k < line.size(); ++k) ahead.insert({line[k].x, line[k].y});
      std::optional<RushPush> side;
      for (const CubeI s : {kCubeDirs[(di + 1) % 6], kCubeDirs[(di + 5) % 6]}) {
        const RushPush push = rushPush(b, static_cast<size_t>(who), s, occ);
        if (push.path.empty()) continue;
        const BattleHex land = push.path.back();
        bool clear = true;
        for (const BattleHex& c : Battle::footprint(w, land.x, land.y))
          if (ahead.count({c.x, c.y})) clear = false;
        if (clear) {
          side = push;
          break;
        }
      }
      if (side) {
        legs.push_back({run, static_cast<size_t>(who), *side});
        place(static_cast<size_t>(who), side->path.back());
        run.clear();
        --i;  // re-check this hex, now empty
        continue;
      }
    }
    legs.push_back({run, static_cast<size_t>(who), rushPush(b, static_cast<size_t>(who), *dir, occ)});
    return legs;
  }
  return std::nullopt;
}


// --- Summons (castSummonFamiliar): the conjurer's familiars and the Cao Zumbi -------------------------------------------

struct SummonClass {
  std::string id, name, role, sprite;
  int mov = 5, minRange = 1, maxRange = 1, size = 1, initiativeBonus = 0;
  std::optional<std::vector<BattleHex>> offsets;
  int footprintW = 2, footprintH = 4;
  std::vector<std::pair<std::string, double>> resistances;
};
struct SummonData {
  std::map<std::string, SummonClass> classes;
  std::vector<int> spellCharges, missileCharges, drainCharges;
  int dogVenom = 2;
};
const SummonData& summonData() {
  static const SummonData data = [] {
    SummonData d;
    std::ifstream in("content/engine2/summons.json", std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    if (!in || j.is_discarded() || !j.contains("classes")) {
      logError("battle: cannot read content/engine2/summons.json (export it with tools/scripts/engine2/export_summons.ts)");
      return d;
    }
    for (const auto& [id, c] : j["classes"].items()) {
      SummonClass s;
      s.id = c.value("id", id), s.name = c.value("name", id), s.role = c.value("role", std::string()), s.sprite = c.value("sprite", id);
      s.mov = c.value("mov", 5), s.minRange = c.value("minRange", 1), s.maxRange = c.value("maxRange", 1), s.size = c.value("size", 1);
      s.initiativeBonus = c.value("initiativeBonus", 0);
      if (c.contains("footprintOffsets") && c["footprintOffsets"].is_array()) {
        std::vector<BattleHex> o;
        for (const nlohmann::json& f : c["footprintOffsets"]) o.push_back({f.value("dx", 0), f.value("dy", 0)});
        s.offsets = std::move(o);
      }
      if (c.contains("footprintW") && c["footprintW"].is_number()) s.footprintW = c["footprintW"].get<int>();
      if (c.contains("footprintH") && c["footprintH"].is_number()) s.footprintH = c["footprintH"].get<int>();
      if (c.contains("resistances") && c["resistances"].is_object())
        for (const auto& [k, v] : c["resistances"].items())
          if (v.is_number()) s.resistances.emplace_back(k, v.get<double>());
      d.classes[id] = std::move(s);
    }
    auto table = [&](const char* key) {
      std::vector<int> t;
      if (j.contains("tables") && j["tables"].contains(key))
        for (const nlohmann::json& v : j["tables"][key]) t.push_back(v.get<int>());
      return t;
    };
    d.spellCharges = table("familiarSpellCharges"), d.missileCharges = table("familiarMagicMissileCharges"), d.drainCharges = table("familiarLifeDrainCharges");
    d.dogVenom = j.value("zombieDogVenomCharges", 2);
    return d;
  }();
  return data;
}
struct SummonSpec {
  const char* kind;
  const char* classId;
  double scale;
  int range;
  const char* prefix;
};
constexpr SummonSpec kSummons[] = {{"summonFamiliar", "familiar", 0.5, 4, "Familiar de"},
                                   {"summonFamiliar2", "familiar2", 0.75, 4, "Familiar Maior de"},
                                   {"summonFamiliar3", "familiar3", 1.0, 5, "Familiar Titã de"},
                                   {"summonFamiliar4", "familiar4", 0.75, 4, "Familiar Radiante de"},
                                   {"summonZombieDog", "zombieDog", 1.0, 4, "Cão Zumbi de"}};
const SummonSpec* summonSpec(const std::string& kind) {
  for (const SummonSpec& s : kSummons)
    if (kind == s.kind) return &s;
  return nullptr;
}
int chargesAt(const std::vector<int>& table, int level) {
  if (table.empty()) return 1;
  return table[static_cast<size_t>(std::max(1, std::min(static_cast<int>(table.size()), level)) - 1)];
}
bool hasFamiliarOut(const Battle& b, const BattleUnit& caster, const std::string& classId) {
  for (const BattleUnit& v : b.units())
    if (v.alive && v.summonerId == caster.id && v.classId == classId) return true;
  return false;
}
// Where a summon of this class would stand: every hex of its body free, passable and on the board.
bool summonCellsFree(const Battle& b, const SummonClass& cls, BattleHex cell) {
  BattleUnit probe;
  probe.size = cls.size, probe.footprintOffsets = cls.offsets, probe.footprintW = cls.footprintW, probe.footprintH = cls.footprintH;
  const bool body = cls.id == "familiar3" || cls.id == "zombieDog";
  const std::vector<BattleHex> cells = body ? Battle::footprint(probe, cell.x, cell.y) : std::vector<BattleHex>{cell};
  const OccMap occ = occupancyMap(b);
  for (const BattleHex& p : cells)
    if (!inBoardXY(b, p.x, p.y) || !b.cell(p.x, p.y).passable || occ.count({p.x, p.y})) return false;
  return true;
}
// The familiar itself, as castSummonFamiliar builds it from its summoner at the moment the cast plays.
BattleUnit makeSummon(const Battle& b, const BattleUnit& caster, const SummonSpec& sp, const SummonClass& cls, BattleHex cell, int initiativeRoll) {
  const SummonData& d = summonData();
  BattleUnit f;
  f.id = std::format("player-familiar-{}", b.units().size());
  f.name = std::string(sp.prefix) + " " + caster.name;
  f.classId = cls.id, f.className = cls.name, f.sprite = cls.sprite, f.side = "player";
  f.x = cell.x, f.y = cell.y, f.alive = true;
  f.maxHp = std::max(1, static_cast<int>(jsRound(caster.maxHp * sp.scale))), f.hp = f.maxHp;
  f.atk = static_cast<int>(jsRound(caster.atk * sp.scale)), f.mag = static_cast<int>(jsRound(caster.mag * sp.scale));
  f.def = static_cast<int>(jsRound(caster.def * sp.scale)), f.dex = static_cast<int>(jsRound(caster.dex * sp.scale));
  f.combat.resistances = cls.resistances;
  f.initiativeBonus = cls.initiativeBonus, f.initiative = initiativeRoll + cls.initiativeBonus;
  f.mov = cls.mov, f.minRange = cls.minRange, f.maxRange = cls.maxRange, f.size = cls.size;
  f.footprintOffsets = cls.offsets, f.footprintW = cls.footprintW, f.footprintH = cls.footprintH;
  f.level = caster.level, f.xp = 0, f.summoned = true, f.faceDx = 1.0, f.faceDy = 0.0, f.facing = 1;
  f.fullness = 100;
  const std::string k = sp.kind;
  f.spellCharges = k == "summonFamiliar4" ? 3 : k == "summonZombieDog" ? d.dogVenom : k == "summonFamiliar3" ? chargesAt(d.spellCharges, caster.level) : chargesAt(d.missileCharges, caster.level);
  if (k == "summonFamiliar2" || k == "summonFamiliar4") f.lifeDrainCharges = chargesAt(d.drainCharges, caster.level);
  f.summonerId = caster.id;
  return f;
}

// --- Aim validity of the remaining spells (spellAimValid) ---------------------------------------------------------------

bool boltReaches(const Battle& b, const BattleUnit& u, BattleHex cell, int range) {
  return hexDist({u.x, u.y}, cell) <= range && clearShot(b, {u.x, u.y}, cell, false);
}
std::string rangeMsg(int range) { return std::format("Casa fora de alcance (máximo {} hexes).", range); }
std::string targetRangeMsg(int range) { return std::format("Alvo fora de alcance (máximo {} hexes).", range); }

// Whether a kind is one of these; the verdict goes in `valid`, the reason in `error`.
bool aimE2(const Battle& b, size_t caster, const std::string& kind, BattleHex cell, bool& valid, std::string* error) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  const int here = occAt(b, cell);
  const int d = hexDist(from, cell);
  auto no = [&](const std::string& why) {
    valid = false;
    if (error) *error = why;
    return true;
  };
  auto yes = [&]() {
    valid = true;
    return true;
  };
  // the generic reason when a target is required (spellAimError)
  auto targetError = [&]() -> std::string {
    if (here < 0) return "Não há unidade na casa selecionada.";
    const BattleUnit& t = b.units()[static_cast<size_t>(here)];
    if (t.hidden) return "Escolha um inimigo visível.";
    if (t.side == u.side) return "Essa habilidade não pode mirar em uma aliada.";
    return "";
  };
  auto bolt = [&](int range) {
    if (!inBoardXY(b, cell.x, cell.y)) return no("Fora do mapa.");
    if (d > range) return no(rangeMsg(range));
    return clearShot(b, from, cell, false) ? yes() : no("O tiro está bloqueado.");
  };
  const auto grown = [&](int base) { return grownReach(kind, base, u.level); };  // the reach that grows with the level
  if (kind == "fireball") return bolt(grown(kFireballRange));
  if (kind == "iceStorm") return bolt(grown(iceStormPower(u.level).range));
  if (kind == "causticVenom") return bolt(kCausticVenom.range);
  if (kind == "minorVenom") return bolt(kMinorVenom.range);
  if (kind == "divineBolt") return bolt(grown(kDivineBolt.range));
  if (kind == "webOfDreams") return bolt(grown(kWebRange));
  // The verdict is spellAimValid's (targetable, range, line of fire); the reason, when it fails, spellAimError's.
  auto whyNotTarget = [&]() -> std::string {
    if (const std::string e = targetError(); !e.empty()) return e;
    return "Escolha um inimigo visível.";
  };
  if (kind == "bloodyShot" || kind == "multiShot") {
    const int range = grown(kind == "bloodyShot" ? kBloodyShotRange : kMultiShotRange);
    if (!targetable(b, here)) return no(whyNotTarget());
    if (d < u.minRange) return no(std::format("Alvo perto demais (alcance mínimo {} hexes).", u.minRange));
    if (d > range) return no(std::format("Alvo fora de alcance (máximo {} hexes).", range));
    return clearShot(b, from, cell, true) ? yes() : no("O tiro está bloqueado.");
  }
  if (kind == "lightningTier3") {
    if (!targetable(b, here)) return no(whyNotTarget());
    return d <= grown(kLightningT3Range) ? yes() : no(targetRangeMsg(grown(kLightningT3Range)));
  }
  if (kind == "phantasmalForce") {
    if (!targetable(b, here)) return no(whyNotTarget());
    if (d > grown(kPhantasmalRange)) return no(targetRangeMsg(grown(kPhantasmalRange)));
    return clearShot(b, from, cell, false) ? yes() : no("O tiro está bloqueado.");
  }
  if (kind == "trip" || kind == "familiarLifeDrain" || kind == "executionerStrike" || kind == "shieldBash") {
    if (const std::string e = targetError(); !e.empty()) return no(e);
    const BattleUnit& f = b.units()[static_cast<size_t>(here)];
    if (f.alive && f.side != u.side && canHitFrom(b, caster, from, static_cast<size_t>(here))) return yes();
    return no("Esse inimigo não está ao alcance ou não pode ser atingido daqui.");
  }
  if (kind == "provoke") {
    if (const std::string e = targetError(); !e.empty()) return no(e);
    const BattleUnit& f = b.units()[static_cast<size_t>(here)];
    if (!(f.alive && f.side == "enemy" && targetable(b, here))) return no("Esse alvo não pode ser atingido por esta habilidade.");
    return d <= provokePower(u.level).range ? yes() : no(targetRangeMsg(provokePower(u.level).range));
  }
  if (kind == "cleave" || kind == "shoulderSmash") {
    const std::vector<BattleHex> ring = Battle::neighbors(u.x, u.y);
    return std::any_of(ring.begin(), ring.end(), [&](const BattleHex& p) { return p == cell; }) ? yes() : no("Escolha um hex vizinho ao personagem.");
  }
  if (kind == "bullRush") return bullRushCharge(b, caster, cell) ? yes() : no("Não há um caminho livre com espaço para concluir a investida nesse alvo.");
  if (kind == "burningHands" || kind == "poisonBreath") {
    const ConeP p = kind == "poisonBreath" ? poisonBreathPower(u.level) : burningHandsPower(u.level);
    return !wrathRay(b, from, cell, p.range).empty() ? yes() : no("Escolha uma linha reta válida dentro do alcance da habilidade.");
  }
  if (kind == "divineWrath" || kind == "stampede")
    return !wrathRay(b, from, cell, kWrathRange).empty() ? yes() : no("Escolha uma linha reta válida dentro do alcance da habilidade.");
  if (kind == "sweep") return d <= kSweepRadius ? yes() : no(std::format("Escolha uma casa dentro do raio {}.", kSweepRadius));
  if (kind == "turnUndead" || kind == "bless" || kind == "auraOfProtection" || kind == "intimidatingPresence")
    return cell == from ? yes() : no("Esta magia não precisa de alvo.");
  if (const SummonSpec* sp = summonSpec(kind)) {
    const int reach = sp->range + reachGrowth(u.level);  // a summoner's reach grows with its level too
    if (d > reach) return no(std::format("Ponto de invocação fora de alcance (máximo {} hexes).", reach));
    if (!inBoardXY(b, cell.x, cell.y)) return no("O ponto de invocação fica fora do mapa.");
    const SummonData& data = summonData();
    const auto it = data.classes.find(sp->classId);
    if (it == data.classes.end()) return no("Esta criatura não existe aqui.");
    if (summonCellsFree(b, it->second, cell)) return yes();
    if (here >= 0) return no("O ponto de invocação está ocupado.");
    if (!b.cell(cell.x, cell.y).passable) return no("O terreno bloqueia a invocação.");
    return no("Não há espaço livre para essa criatura nesse ponto.");
  }
  if (kind == "warp") {  // a free passable hex
    if (!inBoardXY(b, cell.x, cell.y) || !b.cell(cell.x, cell.y).passable) return no("Escolha uma casa livre.");
    for (size_t i = 0; i < b.units().size(); ++i)
      if (b.units()[i].alive && occupies(b.units()[i], cell.x, cell.y)) return no("Escolha uma casa livre.");
    return yes();
  }
  return false;
}

// --- The casts (castX): the steps each one queues -------------------------------------------------------------------------

// Every unit standing on one of the hexes, once each, in the order the hexes are walked (the casts' `ids`).
std::vector<size_t> unitsAlong(const Battle& b, const std::vector<BattleHex>& tiles, std::optional<size_t> except = std::nullopt, const std::string* foesOf = nullptr) {
  std::vector<size_t> ids;
  for (const BattleHex& t : tiles) {
    const int who = firstAt(b, t);
    if (who < 0) continue;
    const size_t w = static_cast<size_t>(who);
    if (except && w == *except) continue;
    if (foesOf && b.units()[w].side == *foesOf) continue;
    if (std::find(ids.begin(), ids.end(), w) == ids.end()) ids.push_back(w);
  }
  return ids;
}

// spendFamiliarOrTier: a familiar's own spell draws from its charges.
bool familiarOwnSpell(const BattleUnit& u, const std::string& kind) {
  return (u.classId == "familiar" || u.classId == "familiar2") ? kind == "magicMissile"
         : u.classId == "familiar3"                              ? kind == "fireball"
         : u.classId == "familiar4"                              ? kind == "shock"
         : u.classId == "zombieDog"                              ? kind == "minorVenom"
                                                                 : false;
}
void spendFamiliarOrTier(BattleUnit& u, const std::string& kind) {
  if (familiarOwnSpell(u, kind)) u.spellCharges = std::max(0, u.spellCharges - 1);
  else spendTier(u, kind);
}
int familiarRemaining(const BattleUnit& u, const std::string& kind) { return familiarOwnSpell(u, kind) ? u.spellCharges : tierRemaining(u, kind); }

std::optional<std::vector<BattleStep>> castE2(Battle& b, size_t caster, const std::string& kind, const std::vector<BattleHex>& cells, std::string* error, bool checkUses) {
  BattleUnit& u = b.units()[caster];
  auto fail = [&](const std::string& why) -> std::optional<std::vector<BattleStep>> {
    if (error) *error = why;
    return std::nullopt;
  };
  const BattleHex cell = cells[0];
  const BattleHex from{u.x, u.y};
  BattleStep s;
  s.att = caster;
  s.kind = kind;
  std::vector<BattleStep> steps;
  auto weaponSpell = [&](std::vector<BattleHex> tiles, std::vector<size_t> ids, DiceFaces bonus, int bonusFlat) {
    s.tiles = std::move(tiles), s.ids = std::move(ids);
    s.weaponBonusDice = bonus.dice, s.weaponBonusFaces = bonus.faces, s.weaponBonusBonus = bonusFlat;
    steps.push_back(s);
  };
  if (kind == "bloodyShot") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Não há unidade na casa selecionada.");
    spendTier(u, kind);
    s.tiles = {cell}, s.ids = {static_cast<size_t>(foe)}, s.dmgMul = bloodyShotMul(u.level);
    steps.push_back(s);
  } else if (kind == "multiShot") {
    // one step per arrow, each aimed on its own (castMultiShot collects them, then spends one use)
    std::vector<std::pair<size_t, BattleHex>> shots;
    for (const BattleHex& c : cells) {
      const int foe = occAt(b, c);
      if (foe < 0) return fail("Não há unidade na casa selecionada.");
      shots.emplace_back(static_cast<size_t>(foe), c);
    }
    spendTier(u, kind);
    const DiceFaces p = multiShotPower(u.level);
    for (const auto& [foe, c] : shots) {
      BattleStep m = s;
      m.tiles = {c}, m.ids = {foe};
      m.weaponBonusDice = p.dice, m.weaponBonusFaces = p.faces, m.weaponBonusBonus = 0;
      steps.push_back(m);
    }
  } else if (kind == "trip") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Spell was not cast.");
    spendTier(u, kind);
    s.type = BattleStep::Type::Combat, s.def = static_cast<size_t>(foe), s.noCounter = true;
    s.bonusDice = kTripBonusFaces, s.bonusDiceCount = 1, s.bonusFlat = kTripBonusBonus;
    steps.push_back(s);
  } else if (kind == "executionerStrike" || kind == "shieldBash") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Spell was not cast.");
    if (checkUses && kind == "shieldBash" && !u.combat.offHandShield) return fail("Requer um escudo equipado.");
    spendTier(u, kind);
    const DiceFaces p = kind == "shieldBash" ? DiceFaces{shieldBashPower(u.level).dice, shieldBashPower(u.level).faces}
                                             : DiceFaces{executionerStrikePower(u.level).dice, executionerStrikePower(u.level).faces};
    s.type = BattleStep::Type::Combat, s.def = static_cast<size_t>(foe);
    s.bonusDice = p.faces, s.bonusDiceCount = p.dice, s.bonusFlat = 0;
    steps.push_back(s);
  } else if (kind == "cleave" || kind == "shoulderSmash") {
    const bool smash = kind == "shoulderSmash";
    if (checkUses && smash && u.combat.offHandShield) return fail("Requer as mãos livres (sem escudo).");
    const SmashP sp = shoulderSmashPower(u.level);
    const std::vector<BattleHex> tiles = cleaveHexes(b, from, cell, smash ? sp.hexes : kCleaveHexes);
    if (tiles.empty()) return fail("Toque num hex vizinho.");
    const std::vector<size_t> ids = unitsAlong(b, tiles, caster, &u.side);
    spendTier(u, kind);
    weaponSpell(tiles, ids, smash ? DiceFaces{sp.dice, sp.faces} : cleavePower(u.level), 0);
  } else if (kind == "sweep") {
    std::vector<BattleHex> tiles = hexAreaTiles(b, from, kSweepRadius);
    std::erase_if(tiles, [&](const BattleHex& t) { return t == from; });
    const std::vector<size_t> ids = unitsAlong(b, tiles, caster, &u.side);
    spendTier(u, kind);
    s.tiles = tiles, s.ids = ids;
    steps.push_back(s);
  } else if (kind == "divineWrath" || kind == "stampede") {
    const std::vector<BattleHex> tiles = wrathRay(b, from, cell, kWrathRange);
    if (tiles.empty()) return fail("Escolha uma linha reta válida dentro do alcance da habilidade.");
    const bool wrath = kind == "divineWrath";
    const std::vector<size_t> ids = unitsAlong(b, tiles, caster, wrath ? &u.side : nullptr);
    spendTier(u, kind);
    weaponSpell(tiles, ids, wrath ? divineWrathPower(u.level) : stampedePower(u.level), wrath ? static_cast<int>(std::floor(u.mag / 2)) : 0);
  } else if (kind == "burningHands" || kind == "poisonBreath") {
    const ConeP p = kind == "poisonBreath" ? poisonBreathPower(u.level) : burningHandsPower(u.level);
    const std::vector<BattleHex> ray = wrathRay(b, from, cell, p.range);
    const std::optional<CubeI> dir = ray.empty() ? std::nullopt : axisDir(from, ray[0]);
    if (!dir) return fail("Escolha uma linha reta válida dentro do alcance da habilidade.");
    const std::vector<BattleHex> tiles = coneSector(b, from, *dir, p.radius);
    const std::vector<size_t> ids = unitsAlong(b, tiles, caster);
    spendTier(u, kind);
    s.tiles = tiles, s.ids = ids, s.dice = p.dice, s.faces = p.faces, s.bonus = 0, s.spellMul = p.mul, s.poison = kind == "poisonBreath";
    steps.push_back(s);
  } else if (kind == "fireball" || kind == "causticVenom" || kind == "divineBolt" || kind == "minorVenom") {
    const bool fire = kind == "fireball";
    const BoltData& d = kind == "causticVenom" ? kCausticVenom : kind == "minorVenom" ? kMinorVenom : kDivineBolt;
    const std::vector<BattleHex> tiles = hexAreaTiles(b, cell, fire ? kFireballSize : d.size);
    const std::vector<size_t> ids = unitsAlong(b, tiles, std::nullopt);
    const int center = firstAt(b, cell);
    spendFamiliarOrTier(u, kind);
    s.tiles = tiles, s.ids = ids;
    if (fire) {
      s.dice = kFireballDice, s.faces = kFireballFaces, s.bonus = kFireballBonus, s.spellMul = kFireballMul;
    } else {
      s.dice = d.splashDice, s.faces = d.splashFaces, s.bonus = d.splashBonus, s.spellMul = d.splashMul;
      if (center >= 0) s.centerId = static_cast<size_t>(center);
      s.centerDice = d.centerDice, s.centerFaces = d.centerFaces, s.centerBonus = d.centerBonus, s.centerMul = d.centerMul;
      s.poison = kind != "divineBolt";
    }
    steps.push_back(s);
  } else if (kind == "iceStorm") {
    const IceStormP p = iceStormPower(u.level);
    const std::vector<BattleHex> area = iceStormAreaTiles(b, cell, u.level);
    Battle::IceZone z;
    z.cells = area, z.roundsLeft = p.durationRounds, z.center = cell, z.radius = p.size;
    z.damageDice = p.dice, z.damageFaces = p.faces, z.damageMul = p.mul;
    z.casterMag = u.mag, z.casterLevel = u.level, z.casterId = u.id, z.side = u.side;
    b.iceZones().push_back(std::move(z));
    spendTier(u, kind);
    s.tiles = {cell}, s.ids = {};
    steps.push_back(s);
  } else if (kind == "turnUndead") {
    const UndeadP p = turnUndeadPower(u.level);
    std::vector<BattleHex> tiles;
    for (const BattleHex& t : hexAreaTiles(b, from, p.radius))
      if (b.cell(t.x, t.y).terrain != "void") tiles.push_back(t);
    std::vector<size_t> ids;
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& t = b.units()[i];
      if (t.alive && t.undead && std::any_of(tiles.begin(), tiles.end(), [&](const BattleHex& c) { return occupies(t, c.x, c.y); })) ids.push_back(i);
    }
    spendTier(u, kind);
    s.tiles = tiles, s.ids = ids, s.dice = p.dice, s.faces = p.faces, s.bonus = 0, s.spellMul = p.mul;
    steps.push_back(s);
  } else if (kind == "lightningTier3") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Spell was not cast.");
    spendTier(u, kind);
    s.tiles = {cell}, s.ids = {static_cast<size_t>(foe)};
    s.dice = kLightningT3Dice, s.faces = kLightningT3Faces, s.bonus = kLightningT3Bonus, s.spellMul = kLightningT3Mul, s.echo = kLightningT3Echo;
    steps.push_back(s);
  } else if (kind == "familiarLifeDrain" || kind == "phantasmalForce") {
    const int foe = occAt(b, cell);
    if (foe < 0) return fail("Spell was not cast.");
    if (kind == "familiarLifeDrain") u.lifeDrainCharges = std::max(0, u.lifeDrainCharges.value_or(1) - 1);
    else spendTier(u, kind);
    const DiceFaces p = kind == "familiarLifeDrain" ? lifeDrainDice(u.level) : phantasmalForceDice(u.level);
    s.tiles = {cell}, s.ids = {static_cast<size_t>(foe)};
    s.dice = p.dice, s.faces = p.faces, s.bonus = 0, s.spellMul = 1;
    steps.push_back(s);
  } else if (kind == "provoke") {
    const ProvokeP p = provokePower(u.level);
    std::vector<size_t> foes;
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& f = b.units()[i];
      if (!f.alive || f.side != "enemy" || !targetable(b, static_cast<int>(i))) continue;
      const std::vector<BattleHex> body = Battle::footprint(f, f.x, f.y);
      if (std::any_of(body.begin(), body.end(), [&](const BattleHex& c) { return hexDist(cell, c) <= p.radius; })) foes.push_back(i);
    }
    spendTier(u, kind);
    s.type = BattleStep::Type::Effect, s.ids = foes, s.tiles = {cell};
    steps.push_back(s);
  } else if (kind == "bullRush") {
    const std::optional<std::vector<RushLeg>> legs = bullRushCharge(b, caster, cell);
    if (!legs) return fail("Toque num inimigo a até 4 hexes, sem aliado ou obstáculo no caminho.");
    spendTier(u, kind);
    BullRushP p = bullRushPower(u.level);
    if (u.side == "player" && bullRushTier3()) p.dice += 1;  // ours: the tier-3 Investida hits one die harder (unitTierOf)
    BattleHex at = from;
    for (const RushLeg& leg : *legs) {
      if (!leg.path.empty()) {
        BattleStep m;
        m.type = BattleStep::Type::Move, m.att = caster, m.kind = "bullRush";
        m.path.push_back(at);
        for (const BattleHex& h : leg.path) m.path.push_back(h);
        at = leg.path.back();
        steps.push_back(m);
      }
      BattleStep c;
      c.type = BattleStep::Type::Combat, c.att = caster, c.def = leg.foe, c.kind = "bullRush", c.noCounter = true;
      c.bonusDice = p.faces, c.bonusDiceCount = p.dice, c.bonusFlat = 0;
      if (leg.push.blocked) c.wallDice = p.wallDice, c.wallFaces = p.wallFaces;
      if (!leg.push.path.empty()) c.knockTo = leg.push.path.back();
      steps.push_back(c);
    }
  } else if (const SummonSpec* sp = summonSpec(kind)) {
    const SummonData& data = summonData();
    const auto it = data.classes.find(sp->classId);
    if (it == data.classes.end()) return fail("Esta criatura não existe aqui.");
    if (std::string(kind) == "summonFamiliar2" && u.level < 5) return fail("Familiar Maior disponível a partir do nível 5.");
    if (hasFamiliarOut(b, u, it->second.id)) return fail(std::format("{} já tem {} invocado.", u.name, it->second.name));
    spendTier(u, kind);
    b.noteAware(caster, kSupportCe, kSupportVe);
    s.type = BattleStep::Type::Effect, s.tiles = {cell};
    steps.push_back(s);
  } else if (kind == "bless") {
    // Bênção: the caster is the centre and every living ally within three hexes gets the level's accuracy bonus
    std::vector<size_t> allies;
    std::vector<BattleHex> tiles;
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& t = b.units()[i];
      if (t.alive && t.side == u.side && hexDist(from, {t.x, t.y}) <= kBlessRadius) allies.push_back(i), tiles.push_back({t.x, t.y});
    }
    spendTier(u, kind);
    b.noteAware(caster, kSupportCe, kSupportVe);
    s.tiles = tiles, s.ids = allies;
    steps.push_back(s);
  } else if (kind == "webOfDreams") {
    // a sticky patch centred on the aimed hex; whoever stands in it (either side) may fall asleep (rolled as the cast plays)
    const int radius = webOfDreamsSize(u.level);
    const double chance = webOfDreamsSleepChance(u.level);
    const std::vector<BattleHex> cellsHit = hexAreaTiles(b, cell, radius);
    b.webZones().push_back({cellsHit, kWebDuration, cell, radius, chance});
    spendTier(u, kind);
    b.noteAware(caster, kSupportCe, kSupportVe);
    s.type = BattleStep::Type::Effect, s.tiles = cellsHit, s.spellMul = chance;
    steps.push_back(s);
  } else if (kind == "auraOfProtection" || kind == "intimidatingPresence") {
    const AuraP p = auraPower(u.level);
    b.auraZones().push_back({hexAreaTiles(b, from, p.radius), p.duration, kind == "auraOfProtection" ? "protection" : "intimidation", u.side, p.pct});
    spendTier(u, kind);
    b.noteAware(caster, kSupportCe, kSupportVe);
    s.type = BattleStep::Type::Effect, s.tiles = {from};
    steps.push_back(s);
  } else if (kind == "warp") {
    // two openings: one beside the caster (towards the nearest foe), one beside the aimed hex; a unit stepping on either
    // comes out beside the other
    auto taken = [&](BattleHex c) {
      for (size_t i = 0; i < b.units().size(); ++i)
        if (b.units()[i].alive && occupies(b.units()[i], c.x, c.y)) return true;
      return std::any_of(b.warpGates().begin(), b.warpGates().end(), [&](const Battle::WarpGate& g) { return c == g.a || c == g.b; });
    };
    auto freeNear = [&](BattleHex centre, int radius) {
      std::vector<BattleHex> out;
      for (const BattleHex& c : hexAreaTiles(b, centre, radius))
        if (b.cell(c.x, c.y).passable && !taken(c)) out.push_back(c);
      std::stable_sort(out.begin(), out.end(), [&](const BattleHex& p, const BattleHex& q) {
        const int dp = hexDist(p, centre), dq = hexDist(q, centre);
        return dp != dq ? dp < dq : p.y != q.y ? p.y < q.y : p.x < q.x;
      });
      return out;
    };
    std::vector<BattleHex> sources;
    for (const BattleHex& c : Battle::neighbors(from.x, from.y))
      if (inBoardXY(b, c.x, c.y) && b.cell(c.x, c.y).passable && !taken(c)) sources.push_back(c);
    std::optional<size_t> nearest;
    for (size_t i = 0; i < b.units().size(); ++i) {
      const BattleUnit& f = b.units()[i];
      if (f.alive && f.side != "player" && (!nearest || hexDist(from, {f.x, f.y}) < hexDist(from, {b.units()[*nearest].x, b.units()[*nearest].y}))) nearest = i;
    }
    std::stable_sort(sources.begin(), sources.end(), [&](const BattleHex& p, const BattleHex& q) {
      const int dp = nearest ? hexDist(p, {b.units()[*nearest].x, b.units()[*nearest].y}) : 0, dq = nearest ? hexDist(q, {b.units()[*nearest].x, b.units()[*nearest].y}) : 0;
      return dp != dq ? dp < dq : p.y != q.y ? p.y < q.y : p.x < q.x;
    });
    const std::vector<BattleHex> far = freeNear(cell, 1);
    if (sources.empty() || far.empty() || sources.front() == far.front()) return fail("Não há espaço livre para abrir os dois portais.");
    const int rounds = 3 + (u.level >= 15 ? 1 : 0) + (u.level >= 23 ? 1 : 0);
    b.warpGates().push_back({sources.front(), far.front(), rounds, u.level});
    spendTier(u, kind);
    b.noteAware(caster, kSupportCe, kSupportVe);
    s.tiles = {sources.front(), far.front()}, s.ids = {};
    steps.push_back(s);
  } else {
    return std::nullopt;
  }
  return steps;
}

bool e2Kind(const std::string& k) {
  static const char* const kinds[] = {"bloodyShot", "multiShot",  "trip",       "executionerStrike", "shieldBash", "cleave",         "shoulderSmash",
                                      "sweep",      "divineWrath", "stampede",   "burningHands",      "poisonBreath", "fireball",     "causticVenom",
                                      "divineBolt", "minorVenom", "iceStorm",   "turnUndead",        "lightningTier3", "familiarLifeDrain",  "phantasmalForce",
                                      "provoke",    "bullRush",     "bless",     "webOfDreams", "auraOfProtection", "intimidatingPresence", "warp",
                                      "summonFamiliar", "summonFamiliar2", "summonFamiliar3", "summonFamiliar4", "summonZombieDog"};
  for (const char* c : kinds)
    if (k == c) return true;
  return false;
}

// The hexes the spell would cover aimed at `cell` (spellArea's default branch for these).
std::vector<BattleHex> areaE2(const Battle& b, size_t caster, const std::string& kind, BattleHex cell) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  if (kind == "cleave") return cleaveHexes(b, from, cell, kCleaveHexes);
  if (kind == "shoulderSmash") return cleaveHexes(b, from, cell, shoulderSmashPower(u.level).hexes);
  if (kind == "sweep") {
    std::vector<BattleHex> t = hexAreaTiles(b, from, kSweepRadius);
    std::erase_if(t, [&](const BattleHex& h) { return h == from; });
    return t;
  }
  if (kind == "divineWrath" || kind == "stampede") return wrathRay(b, from, cell, kWrathRange);
  if (kind == "burningHands" || kind == "poisonBreath") {
    const ConeP p = kind == "poisonBreath" ? poisonBreathPower(u.level) : burningHandsPower(u.level);
    const std::vector<BattleHex> ray = wrathRay(b, from, cell, p.range);
    const std::optional<CubeI> dir = ray.empty() ? std::nullopt : axisDir(from, ray[0]);
    return dir ? coneSector(b, from, *dir, p.radius) : std::vector<BattleHex>{};
  }
  if (kind == "fireball") return hexAreaTiles(b, cell, kFireballSize);
  if (kind == "causticVenom") return hexAreaTiles(b, cell, kCausticVenom.size);
  if (kind == "minorVenom") return hexAreaTiles(b, cell, kMinorVenom.size);
  if (kind == "divineBolt") return hexAreaTiles(b, cell, kDivineBolt.size);
  if (kind == "webOfDreams") return hexAreaTiles(b, cell, webOfDreamsSize(u.level));
  if (kind == "iceStorm") return iceStormAreaTiles(b, cell, u.level);
  if (kind == "provoke") return hexAreaTiles(b, cell, provokePower(u.level).radius);
  if (kind == "turnUndead") {
    std::vector<BattleHex> t;
    for (const BattleHex& h : hexAreaTiles(b, from, turnUndeadPower(u.level).radius))
      if (b.cell(h.x, h.y).terrain != "void") t.push_back(h);
    return t;
  }
  if (kind == "bullRush") {
    std::vector<BattleHex> t = hexLine(from, cell);
    if (!t.empty()) t.erase(t.begin());
    return t;
  }
  if (kind == "auraOfProtection" || kind == "intimidatingPresence") return hexAreaTiles(b, from, auraPower(u.level).radius);
  if (kind == "bless") return hexAreaTiles(b, from, kBlessRadius);
  return {cell};
}
