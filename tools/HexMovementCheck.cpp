// HexMovementCheck: deterministic pointy-topped hex 4-way movement pipeline verification.
// Covers:
//   1. Exact 6-to-4 axial direction mapping (odd-r <-> axial, all 6 neighbors, non-adjacent rejection).
//   2. Turn-penalized A* pathfinding (straight-segment grouping, obstacle turn avoidance, non-detour 0.15 < 1.0,
//      and real battle integration with AP budgets, ally pass-through, Corte Baixo, and AI planning).
//   3. Step-locked movement state machine (Idle -> StepStart -> Moving, linear interpolation, facing lock,
//      per-step snap and facing retention at Idle, and mid-step queue replacement/clear).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "world/ActionPoints.h"
#include "world/Battle.h"
#include "world/Hex.h"
#include "world/HexMovement.h"

using namespace ember;

namespace {

int gPassed = 0;
int gFailed = 0;

void check(bool ok, const std::string& what) {
  if (ok) ++gPassed;
  else ++gFailed;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

glm::vec2 oddRWorld(const HexLayout& L, int x, int y) {
  return {static_cast<float>(hexX(L, x, y)), static_cast<float>(hexZ(L, y))};
}

int countPathTurns(const std::vector<std::pair<int, int>>& path, bool* allAdjacent = nullptr) {
  if (allAdjacent) *allAdjacent = true;
  if (path.size() < 2) return 0;
  int turns = 0;
  int prevDir = -1;
  for (size_t i = 1; i < path.size(); ++i) {
    const AxialHex a = oddRToAxial(path[i - 1].first, path[i - 1].second);
    const AxialHex b = oddRToAxial(path[i].first, path[i].second);
    const int dir = axialDirectionIndex(b.q - a.q, b.r - a.r);
    if (dir < 0) {
      if (allAdjacent) *allAdjacent = false;
      return -1;
    }
    if (prevDir >= 0 && dir != prevDir) ++turns;
    prevDir = dir;
  }
  return turns;
}

int pathApCost(const Battle& b, size_t unitIdx, const std::vector<BattleHex>& route) {
  if (route.size() < 2) return 0;
  const BattleUnit& u = b.units()[unitIdx];
  const ApRules& apr = apRules();
  const bool apFree = b.roaming() && apr.movement.freeWhileRoaming;
  if (apFree) return 0;
  int total = 0;
  for (size_t i = 1; i < route.size(); ++i) {
    const BattleCell& c = b.cell(route[i].x, route[i].y);
    int stepAp = apStepCost(b, u, route[i].x, route[i].y, c.moveCost);
    if (static_cast<int>(i) <= u.cond.martial.lowCutHexes) stepAp += 1;
    total += stepAp;
  }
  return total;
}

}  // namespace

int main() {
  std::printf("HexMovementCheck\n");

  // --- 1. Exact 6-to-4 Axial Direction Mapping ---
  std::printf("1. Axial direction mapping\n");
  {
    bool roundTripOk = true;
    for (int y = -5; y <= 20; ++y) {
      for (int x = -5; x <= 20; ++x) {
        const AxialHex ax = oddRToAxial(x, y);
        const auto [rx, ry] = axialToOddR(ax.q, ax.r);
        if (rx != x || ry != y) roundTripOk = false;
      }
    }
    check(roundTripOk, "oddRToAxial / axialToOddR round-trip across even, odd, and negative rows");

    check(axialDeltaToFacing4(1, 0) == SpriteFacing4::Right && axialDirectionIndex(1, 0) == 0, "East (dq=+1, dr=0) -> Right, dir 0");
    check(axialDeltaToFacing4(1, -1) == SpriteFacing4::Up && axialDirectionIndex(1, -1) == 1, "North-East (dq=+1, dr=-1) -> Up, dir 1");
    check(axialDeltaToFacing4(0, -1) == SpriteFacing4::Up && axialDirectionIndex(0, -1) == 2, "North-West (dq=0, dr=-1) -> Up, dir 2");
    check(axialDeltaToFacing4(-1, 0) == SpriteFacing4::Left && axialDirectionIndex(-1, 0) == 3, "West (dq=-1, dr=0) -> Left, dir 3");
    check(axialDeltaToFacing4(-1, 1) == SpriteFacing4::Down && axialDirectionIndex(-1, 1) == 4, "South-West (dq=-1, dr=+1) -> Down, dir 4");
    check(axialDeltaToFacing4(0, 1) == SpriteFacing4::Down && axialDirectionIndex(0, 1) == 5, "South-East (dq=0, dr=+1) -> Down, dir 5");

    check(!axialDeltaToFacing4(0, 0).has_value() && axialDirectionIndex(0, 0) == -1, "zero delta rejected");
    check(!axialDeltaToFacing4(2, 0).has_value() && axialDirectionIndex(2, 0) == -1, "distance-2 E delta rejected");
    check(!axialDeltaToFacing4(1, 1).has_value() && axialDirectionIndex(1, 1) == -1, "non-adjacent (+1,+1) delta rejected");
    check(!axialDeltaToFacing4(-1, -1).has_value() && axialDirectionIndex(-1, -1) == -1, "non-adjacent (-1,-1) delta rejected");
    check(!axialDeltaToFacing4(0, 2).has_value() && axialDirectionIndex(0, 2) == -1, "distance-2 SE delta rejected");

    bool neighborsAgree = true;
    const SpriteFacing4 expectedByIdx[6] = {
        SpriteFacing4::Right, SpriteFacing4::Up, SpriteFacing4::Up,
        SpriteFacing4::Left,  SpriteFacing4::Down, SpriteFacing4::Down,
    };
    for (const BattleHex origin : {BattleHex{4, 4}, BattleHex{4, 5}}) {
      const std::vector<BattleHex> nbrs = Battle::neighbors(origin.x, origin.y);
      if (nbrs.size() != 6) {
        neighborsAgree = false;
        break;
      }
      const AxialHex oAx = oddRToAxial(origin.x, origin.y);
      for (size_t k = 0; k < 6; ++k) {
        const AxialHex nAx = oddRToAxial(nbrs[k].x, nbrs[k].y);
        const int dq = nAx.q - oAx.q;
        const int dr = nAx.r - oAx.r;
        if (dq != kAxialDirs[k].q || dr != kAxialDirs[k].r) neighborsAgree = false;
        if (axialDirectionIndex(dq, dr) != static_cast<int>(k)) neighborsAgree = false;
        if (hexStepToFacing4(origin.x, origin.y, nbrs[k].x, nbrs[k].y) != expectedByIdx[k]) neighborsAgree = false;
      }
    }
    check(neighborsAgree, "Battle::neighbors on even and odd rows match kAxialDirs[0..5] and hexStepToFacing4");
  }

  // --- 2. Turn-Penalized A* Pathfinding ---
  std::printf("2. Turn-penalized A* pathfinding\n");
  {
    auto openWalk = [](int, int) { return true; };
    const struct {
      const char* label;
      int sx, sy, gx, gy;
      int expectedSteps;
    } openCases[] = {
        {"3E + 3NE", 2, 6, 6, 3, 6},    // axial (2-3=-1, 6) -> (6-1=5, 3) => dq=+6, dr=-3
        {"2E + 2SE", 4, 4, 7, 6, 4},    // axial (2,4) -> (4,6) => dq=+2, dr=+2
        {"3W + 2SW", 10, 4, 6, 6, 5},   // axial (8,4) -> (3,6) => dq=-5, dr=+2
        {"2NE + 2NW", 6, 6, 6, 2, 4},   // axial (3,6) -> (5,2) => dq=+2, dr=-4
    };
    for (const auto& tc : openCases) {
      const auto p = findTurnPenalizedHexPath(16, 16, tc.sx, tc.sy, tc.gx, tc.gy, openWalk);
      bool adj = false;
      const int turns = countPathTurns(p, &adj);
      const int dist = axialDistance(oddRToAxial(tc.sx, tc.sy), oddRToAxial(tc.gx, tc.gy));
      check(adj && !p.empty() && static_cast<int>(p.size()) - 1 == tc.expectedSteps && dist == tc.expectedSteps && turns <= 1,
            std::string("open grid ") + tc.label + " groups straight segments (turns=" + std::to_string(turns) + ")");
    }

    // Obstacle case: (4,4) -> (7,6) with (5,4) blocked must choose SE, SE, E, E (1 turn: DOWN, DOWN, RIGHT, RIGHT)
    const auto blockedEast = findTurnPenalizedHexPath(12, 12, 4, 4, 7, 6, [](int x, int y) { return !(x == 5 && y == 4); });
    bool adjObs = false;
    const int obsTurns = countPathTurns(blockedEast, &adjObs);
    std::vector<SpriteFacing4> obsFacings;
    for (size_t i = 1; i < blockedEast.size(); ++i) {
      if (const auto f = hexStepToFacing4(blockedEast[i - 1].first, blockedEast[i - 1].second, blockedEast[i].first, blockedEast[i].second))
        obsFacings.push_back(*f);
    }
    const std::vector<SpriteFacing4> wantFacings{SpriteFacing4::Down, SpriteFacing4::Down, SpriteFacing4::Right, SpriteFacing4::Right};
    check(adjObs && blockedEast.size() == 5 && obsTurns == 1 && obsFacings == wantFacings,
          "blocked (5,4) on (4,4)->(7,6) yields SE,SE,E,E (1 turn: DOWN, DOWN, RIGHT, RIGHT)");

    // Non-detour check: 4-step / 2-turn path (cost 4.30) must beat 5-step / 1-turn detour (cost 5.15)
    // Start axial(0,5) -> odd-r (2,5); Goal axial(3,3) -> odd-r (4,3).
    // Block axial(1,5) -> odd-r (3,5) and axial(1,4) -> odd-r (3,4).
    const auto nonDetour = findTurnPenalizedHexPath(10, 10, 2, 5, 4, 3, [](int x, int y) {
      return !((x == 3 && y == 5) || (x == 3 && y == 4));
    });
    bool adjNd = false;
    const int ndTurns = countPathTurns(nonDetour, &adjNd);
    check(adjNd && nonDetour.size() == 5 && ndTurns == 2,
          "turn penalty 0.15 < 1.0 prefers 4-tile 2-turn shortest path (4.30) over 5-tile 1-turn detour (5.15)");
  }

  // --- 3. Real Battle Integration ---
  std::printf("3. Real battle integration\n");
  {
    const char* kBosque = "content/data/battles/bosque005.battle.json";
    std::optional<Battle> battle = Battle::load(kBosque);
    check(battle.has_value(), std::string("load ") + kBosque);
    if (battle) {
      size_t hero = 0;
      while (hero < battle->units().size() && (!battle->units()[hero].alive || battle->units()[hero].side != "player")) ++hero;
      check(hero < battle->units().size(), "bosque005 has living player hero");
      if (hero < battle->units().size()) {
        Battle b = *battle;
        b.units()[hero].ap = apMax(b, b.units()[hero]);
        const std::vector<ReachCell> reach = b.reachable(hero, true);
        bool allValid = !reach.empty();
        size_t checkedHexes = 0;
        for (const ReachCell& rc : reach) {
          const std::vector<BattleHex> p = b.findPath(hero, {rc.x, rc.y}, true);
          if (p.empty() || !(p.front() == BattleHex{b.units()[hero].x, b.units()[hero].y}) || !(p.back() == BattleHex{rc.x, rc.y})) {
            allValid = false;
            break;
          }
          if (p.size() >= 2 && !b.legalVoluntaryPath(hero, p)) {
            allValid = false;
            break;
          }
          if (pathApCost(b, hero, p) != rc.ap) {
            allValid = false;
            break;
          }
          ++checkedHexes;
        }
        check(allValid && checkedHexes == reach.size(),
              "every reachable hex (" + std::to_string(checkedHexes) + ") has legal findPath matching minimum ReachCell::ap");

        // Over-AP hex returns empty
        Battle lowAp = b;
        lowAp.units()[hero].ap = 1;
        BattleHex farGoal = b.units()[hero].x > 2 ? BattleHex{0, 0} : BattleHex{b.cols() - 1, b.rows() - 1};
        for (const ReachCell& rc : reach) {
          if (rc.ap > 4) {
            farGoal = {rc.x, rc.y};
            break;
          }
        }
        check(lowAp.findPath(hero, farGoal, true).empty(), "goal exceeding unit AP returns empty path");

        // Blocked cell returns empty
        BattleHex blockedHex{-1, -1};
        for (int y = 0; y < b.rows() && blockedHex.x < 0; ++y)
          for (int x = 0; x < b.cols() && blockedHex.x < 0; ++x)
            if (!b.cell(x, y).passable || b.cell(x, y).blocked) blockedHex = {x, y};
        if (blockedHex.x >= 0) check(b.findPath(hero, blockedHex, true).empty(), "impassable/blocked hex returns empty path");

        // Ally pass-through vs stop on ally
        size_t ally = 0;
        while (ally < b.units().size() && (ally == hero || !b.units()[ally].alive || b.units()[ally].side != "player")) ++ally;
        if (ally < b.units().size()) {
          Battle passTest = b;
          BattleUnit& hu = passTest.units()[hero];
          BattleUnit& au = passTest.units()[ally];
          // Find a flat open corridor of 3 hexes: hu -> au -> free
          bool foundCorridor = false;
          BattleHex hPos{}, aPos{}, beyondPos{};
          for (int y = 1; y < passTest.rows() - 1 && !foundCorridor; ++y) {
            for (int x = 1; x < passTest.cols() - 3 && !foundCorridor; ++x) {
              if (passTest.cell(x, y).terrain == "plains" && passTest.cell(x + 1, y).terrain == "plains" &&
                  passTest.cell(x + 2, y).terrain == "plains" && !passTest.cell(x, y).blocked && !passTest.cell(x + 1, y).blocked &&
                  !passTest.cell(x + 2, y).blocked && passTest.unitAt(x, y) < 0 && passTest.unitAt(x + 1, y) < 0 &&
                  passTest.unitAt(x + 2, y) < 0) {
                hPos = {x, y};
                aPos = {x + 1, y};
                beyondPos = {x + 2, y};
                foundCorridor = true;
              }
            }
          }
          if (foundCorridor) {
            hu.x = hPos.x, hu.y = hPos.y;
            au.x = aPos.x, au.y = aPos.y;
            hu.ap = 20;
            const auto stopOnAlly = passTest.findPath(hero, aPos, true);
            const auto joinOnAlly = passTest.findPath(hero, aPos, false);
            const auto passThrough = passTest.findPath(hero, beyondPos, true);
            check(stopOnAlly.empty(), "requireStop=true forbids ending on ally-occupied hex");
            check(joinOnAlly.size() == 2 && joinOnAlly.back() == aPos, "requireStop=false allows pathing to ally hex (join/cutscene)");
            check(passThrough.size() == 3 && passThrough.back() == beyondPos && passTest.legalVoluntaryPath(hero, passThrough),
                  "requireStop=true allows passing through ally hex to free hex beyond");
          }
        }
      }
    }

    // Corte Baixo + AI pathing check on aldeia015
    const char* kAldeia = "content/data/battles/aldeia015.battle.json";
    std::optional<Battle> aldeia = Battle::load(kAldeia);
    check(aldeia.has_value(), std::string("load ") + kAldeia);
    if (aldeia) {
      Battle b = *aldeia;
      size_t hero = 0;
      while (hero < b.units().size() && (!b.units()[hero].alive || b.units()[hero].side != "player")) ++hero;
      size_t enemy = 0;
      while (enemy < b.units().size() && (!b.units()[enemy].alive || b.units()[enemy].side != "enemy")) ++enemy;
      if (hero < b.units().size()) {
        BattleUnit& hu = b.units()[hero];
        hu.cond.martial.lowCutHexes = 2;
        const int firstStepAp = apStepCost(b, hu, hu.x, hu.y, b.cell(hu.x, hu.y).moveCost) + 1;
        const int twoStepBudget = firstStepAp * 2;
        hu.ap = twoStepBudget - 1;
        const std::vector<ReachCell> shortReach = b.reachable(hero, true);
        const bool anyTwoShort = std::any_of(shortReach.begin(), shortReach.end(), [](const ReachCell& rc) { return rc.cost >= 2; });
        check(!anyTwoShort, "aldeia015 hero under Corte Baixo cannot reach 2 flat hexes with 1 AP short of 2-step cost");

        hu.ap = twoStepBudget;
        const std::vector<ReachCell> reach = b.reachable(hero, true);
        const ReachCell* twoStep = nullptr;
        for (const ReachCell& rc : reach) {
          if (rc.cost == 2) {
            twoStep = &rc;
            break;
          }
        }
        check(twoStep != nullptr, "aldeia015 hero under Corte Baixo has reachable 2-hex destination within " + std::to_string(twoStepBudget) + " AP");
        if (twoStep) {
          const auto p = b.findPath(hero, {twoStep->x, twoStep->y}, true);
          check(!p.empty() && b.legalVoluntaryPath(hero, p) && pathApCost(b, hero, p) == twoStep->ap,
                "Corte Baixo surcharge matches findPath AP and legalVoluntaryPath");
        }
      }
      if (enemy < b.units().size()) {
        b.units()[enemy].ap = apMax(b, b.units()[enemy]);
        const AiPlan plan = b.planEnemy(enemy);
        const bool planOk = plan.path.empty() || (plan.path.size() >= 2 && b.legalVoluntaryPath(enemy, plan.path));
        check(planOk, "Battle::planEnemy yields valid path under findPath");
      }
    }
  }

  // --- 4. Step-Locked Movement State Machine (HexStepMover) ---
  std::printf("4. Step-locked movement state machine\n");
  {
    const HexLayout L = makeLayout(16, 16);
    // H0(4,4) -> H1(5,4)[E] -> H2(6,4)[E] -> H3(6,3)[NE] -> H4(7,2)[NE]
    const std::vector<BattleHex> hexes{{4, 4}, {5, 4}, {6, 4}, {6, 3}, {7, 2}};
    HexWaypoint startWp{hexes[0].x, hexes[0].y, oddRWorld(L, hexes[0].x, hexes[0].y)};
    std::vector<HexWaypoint> q;
    for (size_t k = 1; k < hexes.size(); ++k) q.push_back({hexes[k].x, hexes[k].y, oddRWorld(L, hexes[k].x, hexes[k].y)});

    HexStepMover mover;
    check(mover.state() == HexMoveState::Idle && !mover.active() && !mover.facingLocked() && !mover.animatingWalk(),
          "mover starts in Idle, unlocked, not animating");

    const bool started = mover.assignPath(startWp, q);
    check(started && mover.state() == HexMoveState::Moving && mover.facingLocked() && mover.animatingWalk(),
          "assignPath immediately executes STEP_START -> Moving, locks facing, starts walk");
    check(mover.facing() == SpriteFacing4::Right && mover.stepProgress() == 0.0f &&
              glm::length(mover.worldPos() - startWp.worldPos) < 1e-6f,
          "step 1 (E) initializes facing=Right, stepProgress=0.0, worldPos=H0");

    // Advance mid-step with uneven dt frames
    const float speed = std::sqrt(3.0f) / 0.22f;
    bool stepChanged = mover.update(0.05f, speed);
    check(!stepChanged && mover.state() == HexMoveState::Moving && mover.facing() == SpriteFacing4::Right && mover.facingLocked(),
          "mid-step frame 1 (dt=0.05s) preserves Right and keeps facing locked");
    const float expectedP1 = (speed * 0.05f) / glm::length(q[0].worldPos - startWp.worldPos);
    const glm::vec2 expectedPos1 = startWp.worldPos + (q[0].worldPos - startWp.worldPos) * expectedP1;
    check(std::abs(mover.stepProgress() - expectedP1) < 1e-5f && glm::length(mover.worldPos() - expectedPos1) < 1e-5f,
          "mid-step linear interpolation matches exact step_progress");

    const bool wroteWhileLocked = mover.setFacingIfUnlocked(SpriteFacing4::Left);
    check(!wroteWhileLocked && mover.facing() == SpriteFacing4::Right,
          "external facing write rejected while facingLocked");

    // Finish step 1 (H0 -> H1), must immediately transition to step 2 (H1 -> H2) with Right
    stepChanged = mover.update(0.18f, speed);  // total 0.23s > 0.22s
    check(stepChanged && mover.state() == HexMoveState::Moving && mover.currentHex().x == 5 && mover.currentHex().y == 4 &&
              mover.targetHex().x == 6 && mover.targetHex().y == 4 && mover.facing() == SpriteFacing4::Right &&
              mover.stepProgress() == 0.0f && glm::length(mover.worldPos() - q[0].worldPos) < 1e-5f,
          "arriving at H1 snaps to H1 center and starts step 2 (H1->H2, Right)");

    // Complete step 2 (H1 -> H2), must switch facing strictly at H2 center to Up for step 3 (H2 -> H3)
    stepChanged = mover.update(0.23f, speed);
    check(stepChanged && mover.currentHex().x == 6 && mover.currentHex().y == 4 &&
              mover.targetHex().x == 6 && mover.targetHex().y == 3 && mover.facing() == SpriteFacing4::Up &&
              mover.facingLocked() && mover.stepProgress() == 0.0f &&
              glm::length(mover.worldPos() - q[1].worldPos) < 1e-5f,
          "arriving at H2 snaps to H2 center and transitions strictly at STEP_START to Up for NE step");

    // Advance mid-step 3
    stepChanged = mover.update(0.11f, speed);
    check(!stepChanged && mover.facing() == SpriteFacing4::Up && mover.facingLocked() && mover.animatingWalk(),
          "mid-step 3 holds Up without twitching");

    // Finish step 3 -> start step 4 (H3 -> H4, NE)
    stepChanged = mover.update(0.12f, speed);
    check(stepChanged && mover.currentHex().x == 6 && mover.currentHex().y == 3 &&
              mover.targetHex().x == 7 && mover.targetHex().y == 2 && mover.facing() == SpriteFacing4::Up,
          "arriving at H3 transitions to step 4 (H3->H4, Up)");

    // Finish step 4 -> Idle keeping Up facing
    stepChanged = mover.update(0.23f, speed);
    check(!stepChanged && mover.state() == HexMoveState::Idle && !mover.active() && !mover.animatingWalk() &&
              !mover.facingLocked() && mover.currentHex().x == 7 && mover.currentHex().y == 2 &&
              glm::length(mover.worldPos() - q[3].worldPos) < 1e-5f && mover.facing() == SpriteFacing4::Up,
          "arriving at H4 snaps to H4, switches animation to idle, unlocks facing, and keeps Up facing");

    // Mid-step clearQueuedSteps and replaceQueuedSteps
    HexStepMover reroute;
    reroute.assignPath(startWp, q);  // locked moving H0 -> H1 (Right)
    reroute.update(0.05f, speed);
    reroute.replaceQueuedSteps({{4, 3, oddRWorld(L, 4, 3)}});  // Wait: from H1(5,4), NW is (4,3) -> wait odd-r (5,4) NW is (4,3)?
    // Let's verify (5,4) axial(3,4) -> NW is axial(3,3) -> odd-r (3 + 1 = 4, 3)! Yes, (4,3) is NW of (5,4)!
    check(reroute.facing() == SpriteFacing4::Right && reroute.facingLocked() && reroute.targetHex().x == 5 && reroute.targetHex().y == 4,
          "replaceQueuedSteps mid-step keeps current H0->H1 step and Right facing locked");
    const bool toNw = reroute.update(0.18f, speed);
    check(toNw && reroute.currentHex().x == 5 && reroute.currentHex().y == 4 &&
              reroute.targetHex().x == 4 && reroute.targetHex().y == 3 && reroute.facing() == SpriteFacing4::Up,
          "after H0->H1 completes, STEP_START switches to queued NW step (Up)");
    reroute.clearQueuedSteps();
    reroute.update(0.10f, speed);
    check(reroute.facing() == SpriteFacing4::Up && reroute.facingLocked(),
          "clearQueuedSteps mid-step keeps current NW step locked until arrival");
    reroute.update(0.15f, speed);
    check(reroute.state() == HexMoveState::Idle && !reroute.active() && reroute.currentHex().x == 4 && reroute.currentHex().y == 3 &&
              reroute.facing() == SpriteFacing4::Up,
          "after final cleared step completes, mover snaps to H1's neighbor and enters Idle keeping Up facing");
  }

  std::printf("HexMovementCheck: %d passed, %d failed\n", gPassed, gFailed);
  return gFailed == 0 ? 0 : 1;
}
