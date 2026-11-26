#pragma once

// Deterministic movement pipeline for a unit on a pointy-topped hexagonal grid with 4-way sprites:
//   1. Exact 6-to-4 axial direction mapping from discrete (dq, dr) deltas (no world-velocity or angle heuristics).
//   2. Turn-penalized A* pathfinding storing incoming direction (0..5) per path node with step cost 1.0 and turn penalty 0.15.
//   3. Step-locked discrete waypoint state machine (Idle -> StepStart -> Moving) locking facing and animation per hex step.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace ember {

struct AxialHex {
  int q = 0;
  int r = 0;
  bool operator==(const AxialHex&) const = default;
};

// Odd-r pointy-top coordinates (odd rows shifted half a hex right, +x right, +z down) <-> axial (q, r).
inline constexpr AxialHex oddRToAxial(int x, int y) {
  return {x - (y - (y & 1)) / 2, y};
}

inline constexpr std::pair<int, int> axialToOddR(int q, int r) {
  return {q + (r - (r & 1)) / 2, r};
}

inline int axialDistance(AxialHex a, AxialHex b) {
  const int dq = a.q - b.q;
  const int dr = a.r - b.r;
  return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

enum class SpriteFacing4 : uint8_t {
  Right = 0,
  Left = 1,
  Up = 2,
  Down = 3,
};

inline constexpr int kHexDirCount = 6;
inline constexpr int kHexDirNone = 6;

// Six axial directions in Battle::neighbors order: E, NE, NW, W, SW, SE.
inline constexpr std::array<AxialHex, kHexDirCount> kAxialDirs{{
    {1, 0},    // 0: East
    {1, -1},   // 1: North-East
    {0, -1},   // 2: North-West
    {-1, 0},   // 3: West
    {-1, 1},   // 4: South-West
    {0, 1},    // 5: South-East
}};

inline constexpr int axialDirectionIndex(int dq, int dr) {
  for (int i = 0; i < kHexDirCount; ++i) {
    if (kAxialDirs[static_cast<size_t>(i)].q == dq && kAxialDirs[static_cast<size_t>(i)].r == dr) return i;
  }
  return -1;
}

// Exact 6-to-4 lookup from discrete axial delta (dq, dr).
inline constexpr std::optional<SpriteFacing4> axialDeltaToFacing4(int dq, int dr) {
  if (dq == 1 && dr == 0) return SpriteFacing4::Right;   // East
  if (dq == -1 && dr == 0) return SpriteFacing4::Left;   // West
  if (dq == 1 && dr == -1) return SpriteFacing4::Up;     // North-East
  if (dq == 0 && dr == -1) return SpriteFacing4::Up;     // North-West
  if (dq == 0 && dr == 1) return SpriteFacing4::Down;    // South-East
  if (dq == -1 && dr == 1) return SpriteFacing4::Down;   // South-West
  return std::nullopt;
}

inline constexpr std::optional<SpriteFacing4> hexStepToFacing4(int fromX, int fromY, int toX, int toY) {
  const AxialHex a = oddRToAxial(fromX, fromY);
  const AxialHex b = oddRToAxial(toX, toY);
  return axialDeltaToFacing4(b.q - a.q, b.r - a.r);
}

inline constexpr double kHexBaseStepCost = 1.0;
inline constexpr double kHexTurnPenalty = 0.15;

// Turn-penalized A* pathfinder on an odd-r pointy-topped hex grid.
// State encodes (cell, incoming_dir) so equal-cost paths arriving from different directions never collapse
// and accidentally discard a straight-segment continuation.
// stepEval(int nx, int ny, int stepsSoFar, bool isGoal) -> std::optional<int> (AP step cost, or nullopt if impassable).
template <typename StepEval>
std::vector<std::pair<int, int>> findTurnPenalizedHexPath(int cols,
                                                          int rows,
                                                          int startX,
                                                          int startY,
                                                          int goalX,
                                                          int goalY,
                                                          int apBudget,
                                                          int movBudget,
                                                          int minStepAp,
                                                          StepEval&& stepEval) {
  auto inBounds = [&](int x, int y) { return x >= 0 && y >= 0 && x < cols && y < rows; };
  if (cols <= 0 || rows <= 0 || !inBounds(startX, startY) || !inBounds(goalX, goalY)) return {};
  if (startX == goalX && startY == goalY) return {{startX, startY}};
  if (movBudget <= 0) return {};

  const int statesPerCell = kHexDirCount + 1;  // 0..5 incoming directions + 6 (kHexDirNone) for start
  const size_t stateCount = static_cast<size_t>(cols) * static_cast<size_t>(rows) * static_cast<size_t>(statesPerCell);
  auto stateOf = [&](int x, int y, int dir) -> int { return (y * cols + x) * statesPerCell + dir; };
  auto quantizeScore = [](double s) -> long long { return std::llround(s * 1000000.0); };

  std::vector<int> bestAp(stateCount, std::numeric_limits<int>::max());
  std::vector<long long> bestScoreQ(stateCount, std::numeric_limits<long long>::max());
  std::vector<int> parentState(stateCount, -1);
  std::vector<uint8_t> closed(stateCount, 0);

  struct OpenNode {
    int fAp = 0;
    double fScore = 0.0;
    long long fScoreQ = 0;
    int gAp = 0;
    double gScore = 0.0;
    int steps = 0;
    int turns = 0;
    int x = 0;
    int y = 0;
    int dir = kHexDirNone;
    int state = 0;
    uint32_t seq = 0;
  };
  struct OpenNodeCmp {
    bool operator()(const OpenNode& a, const OpenNode& b) const {
      if (a.fAp != b.fAp) return a.fAp > b.fAp;
      if (a.fScoreQ != b.fScoreQ) return a.fScoreQ > b.fScoreQ;
      if (a.steps != b.steps) return a.steps < b.steps;
      return a.seq > b.seq;
    }
  };

  std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeCmp> open;
  const AxialHex goalAx = oddRToAxial(goalX, goalY);
  const AxialHex startAx = oddRToAxial(startX, startY);
  const int startState = stateOf(startX, startY, kHexDirNone);
  const int hStart = axialDistance(startAx, goalAx);
  const double startFScore = static_cast<double>(hStart) * kHexBaseStepCost;
  const int clampedMinAp = std::max(0, minStepAp);
  const int startFAp = static_cast<int>(std::min<long long>(
      std::numeric_limits<int>::max(), static_cast<long long>(hStart) * static_cast<long long>(clampedMinAp)));

  bestAp[static_cast<size_t>(startState)] = 0;
  bestScoreQ[static_cast<size_t>(startState)] = 0;
  uint32_t seqCounter = 0;
  open.push({startFAp, startFScore, quantizeScore(startFScore), 0, 0.0, 0, 0, startX, startY, kHexDirNone, startState, seqCounter++});

  while (!open.empty()) {
    const OpenNode cur = open.top();
    open.pop();
    if (closed[static_cast<size_t>(cur.state)]) continue;
    closed[static_cast<size_t>(cur.state)] = 1;

    if (cur.x == goalX && cur.y == goalY) {
      std::vector<std::pair<int, int>> path;
      for (int s = cur.state; s >= 0; s = parentState[static_cast<size_t>(s)]) {
        const int cell = s / statesPerCell;
        path.emplace_back(cell % cols, cell / cols);
      }
      std::reverse(path.begin(), path.end());
      return path;
    }

    const AxialHex curAx = oddRToAxial(cur.x, cur.y);
    for (int dir = 0; dir < kHexDirCount; ++dir) {
      const auto [nx, ny] = axialToOddR(curAx.q + kAxialDirs[static_cast<size_t>(dir)].q,
                                        curAx.r + kAxialDirs[static_cast<size_t>(dir)].r);
      if (!inBounds(nx, ny)) continue;

      const bool isGoal = (nx == goalX && ny == goalY);
      const std::optional<int> apStep = stepEval(nx, ny, cur.steps, isGoal);
      if (!apStep) continue;

      const int nextSteps = cur.steps + 1;
      if (nextSteps > movBudget) continue;

      const long long nextApWide = static_cast<long long>(cur.gAp) + static_cast<long long>(*apStep);
      if (nextApWide > static_cast<long long>(apBudget)) continue;
      const int nextAp = static_cast<int>(nextApWide);

      const bool turned = (cur.dir != kHexDirNone && dir != cur.dir);
      const int nextTurns = cur.turns + (turned ? 1 : 0);
      const double nextScore = static_cast<double>(nextSteps) * kHexBaseStepCost + static_cast<double>(nextTurns) * kHexTurnPenalty;
      const long long nextScoreQ = quantizeScore(nextScore);

      const int nextState = stateOf(nx, ny, dir);
      const size_t nextIdx = static_cast<size_t>(nextState);
      if (closed[nextIdx]) continue;

      if (nextAp > bestAp[nextIdx] || (nextAp == bestAp[nextIdx] && nextScoreQ >= bestScoreQ[nextIdx])) continue;

      bestAp[nextIdx] = nextAp;
      bestScoreQ[nextIdx] = nextScoreQ;
      parentState[nextIdx] = cur.state;

      const int hDist = axialDistance(oddRToAxial(nx, ny), goalAx);
      const long long fApWide = nextApWide + static_cast<long long>(hDist) * static_cast<long long>(clampedMinAp);
      const int fAp = static_cast<int>(std::min<long long>(std::numeric_limits<int>::max(), fApWide));
      const double fScore = static_cast<double>(nextSteps + hDist) * kHexBaseStepCost + static_cast<double>(nextTurns) * kHexTurnPenalty;
      open.push({fAp, fScore, quantizeScore(fScore), nextAp, nextScore, nextSteps, nextTurns, nx, ny, dir, nextState, seqCounter++});
    }
  }
  return {};
}

// Convenience overload for uniform-cost hex grids without AP budgets.
template <typename CanWalk>
std::vector<std::pair<int, int>> findTurnPenalizedHexPath(int cols,
                                                          int rows,
                                                          int startX,
                                                          int startY,
                                                          int goalX,
                                                          int goalY,
                                                          CanWalk&& canWalk) {
  return findTurnPenalizedHexPath(
      cols, rows, startX, startY, goalX, goalY, std::numeric_limits<int>::max(), cols * rows, 0,
      [&](int nx, int ny, int /*stepsSoFar*/, bool /*isGoal*/) -> std::optional<int> {
        return canWalk(nx, ny) ? std::optional<int>(0) : std::nullopt;
      });
}

enum class HexMoveState : uint8_t {
  Idle = 0,
  StepStart = 1,
  Moving = 2,
};

struct HexWaypoint {
  int x = 0;
  int y = 0;
  glm::vec2 worldPos{0.0f, 0.0f};
};

// Discrete waypoint state machine consuming one hex step at a time and locking sprite facing
// and walk animation from hex center to hex center.
class HexStepMover {
 public:
  bool assignPath(const HexWaypoint& start, std::vector<HexWaypoint> pathQueue) {
    current_ = start;
    target_ = start;
    startWorldPos_ = start.worldPos;
    targetWorldPos_ = start.worldPos;
    worldPos_ = start.worldPos;
    stepProgress_ = 0.0f;
    travelled_ = 0.0f;
    facingLocked_ = false;
    animatingWalk_ = false;
    state_ = HexMoveState::Idle;
    queue_.assign(pathQueue.begin(), pathQueue.end());
    if (!queue_.empty()) {
      state_ = HexMoveState::StepStart;
      return runStepStart();
    }
    return false;
  }

  void replaceQueuedSteps(std::vector<HexWaypoint> pathQueue) { queue_.assign(pathQueue.begin(), pathQueue.end()); }

  void clearQueuedSteps() { queue_.clear(); }

  void reset(const HexWaypoint& at = {}) {
    queue_.clear();
    current_ = at;
    target_ = at;
    startWorldPos_ = at.worldPos;
    targetWorldPos_ = at.worldPos;
    worldPos_ = at.worldPos;
    stepProgress_ = 0.0f;
    travelled_ = 0.0f;
    facingLocked_ = false;
    animatingWalk_ = false;
    state_ = HexMoveState::Idle;
  }

  bool setFacingIfUnlocked(SpriteFacing4 facing) {
    if (facingLocked_) return false;
    facing_ = facing;
    return true;
  }

  // Advances movement by deltaTime seconds at moveSpeed world-units/s.
  // Returns true iff STEP_START executed during this update.
  bool update(float deltaTime, float moveSpeed) {
    if (state_ == HexMoveState::Idle) {
      if (!queue_.empty()) {
        state_ = HexMoveState::StepStart;
        return runStepStart();
      }
      return false;
    }
    if (state_ == HexMoveState::StepStart) return runStepStart();

    // State MOVING:
    if (deltaTime <= 0.0f || moveSpeed <= 0.0f) return false;
    const glm::vec2 prevPos = worldPos_;
    const float segDist = glm::length(targetWorldPos_ - startWorldPos_);
    stepProgress_ += (segDist <= 1e-6f) ? 1.0f : (moveSpeed * deltaTime) / segDist;
    if (stepProgress_ >= 1.0f) {
      worldPos_ = targetWorldPos_;
      travelled_ += glm::length(worldPos_ - prevPos);
      current_ = target_;
      facingLocked_ = false;
      if (queue_.empty()) {
        animatingWalk_ = false;
        stepProgress_ = 0.0f;
        state_ = HexMoveState::Idle;  // keep current facing_
        return false;
      }
      state_ = HexMoveState::StepStart;
      return runStepStart();
    }
    worldPos_ = startWorldPos_ + (targetWorldPos_ - startWorldPos_) * stepProgress_;
    travelled_ += glm::length(worldPos_ - prevPos);
    // Do not touch facing_, facingLocked_, or animatingWalk_ while mid-step.
    return false;
  }

  HexMoveState state() const { return state_; }
  bool active() const { return state_ != HexMoveState::Idle || !queue_.empty(); }
  bool animatingWalk() const { return animatingWalk_; }
  bool facingLocked() const { return facingLocked_; }
  SpriteFacing4 facing() const { return facing_; }
  // +1 / -1: the side the last step headed (up-right and down-right are right, up-left and down-left are left); 0 before any step.
  int side() const { return side_; }
  const HexWaypoint& currentHex() const { return current_; }
  const HexWaypoint& targetHex() const { return target_; }
  glm::vec2 worldPos() const { return worldPos_; }
  glm::vec2 startWorldPos() const { return startWorldPos_; }
  glm::vec2 targetWorldPos() const { return targetWorldPos_; }
  float stepProgress() const { return stepProgress_; }
  float travelled() const { return travelled_; }
  size_t remainingSteps() const { return queue_.size(); }
  // The hex after the one it is walking to (null on the last step): what a walk's heading and its rounded corners look ahead to.
  const HexWaypoint* nextQueued() const { return queue_.empty() ? nullptr : &queue_.front(); }

 private:
  bool runStepStart() {
    if (queue_.empty()) {
      facingLocked_ = false;
      animatingWalk_ = false;
      stepProgress_ = 0.0f;
      state_ = HexMoveState::Idle;
      return false;
    }
    target_ = queue_.front();
    queue_.pop_front();

    const AxialHex curAx = oddRToAxial(current_.x, current_.y);
    const AxialHex targetAx = oddRToAxial(target_.x, target_.y);
    const int dq = targetAx.q - curAx.q;
    const int dr = targetAx.r - curAx.r;
    if (const std::optional<SpriteFacing4> mapped = axialDeltaToFacing4(dq, dr)) facing_ = *mapped;
    // The side of the step's six directions: east, north-east and south-east head right, the other three left
    // (a pointy-top step moves 2 dq + dr half-hexes along x).
    if (const int halfX = 2 * dq + dr; halfX != 0) side_ = halfX > 0 ? 1 : -1;

    animatingWalk_ = true;
    facingLocked_ = true;
    startWorldPos_ = current_.worldPos;
    targetWorldPos_ = target_.worldPos;
    worldPos_ = startWorldPos_;
    stepProgress_ = 0.0f;
    state_ = HexMoveState::Moving;
    return true;
  }

  HexMoveState state_ = HexMoveState::Idle;
  std::deque<HexWaypoint> queue_;
  HexWaypoint current_;
  HexWaypoint target_;
  glm::vec2 startWorldPos_{0.0f, 0.0f};
  glm::vec2 targetWorldPos_{0.0f, 0.0f};
  glm::vec2 worldPos_{0.0f, 0.0f};
  float stepProgress_ = 0.0f;
  float travelled_ = 0.0f;
  SpriteFacing4 facing_ = SpriteFacing4::Right;
  int side_ = 0;
  bool facingLocked_ = false;
  bool animatingWalk_ = false;
};

}  // namespace ember
