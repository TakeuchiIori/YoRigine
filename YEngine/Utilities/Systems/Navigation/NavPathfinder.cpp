#include "NavPathfinder.h"

// C++
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <cmath>

// ============================================================================
// ヒューリスティック（ユークリッド距離）
// ============================================================================

float NavPathfinder::Heuristic(const NavGrid::GridPos& a, const NavGrid::GridPos& b)
{
    float dx = static_cast<float>(a.x - b.x);
    float dz = static_cast<float>(a.z - b.z);
    return std::sqrtf(dx * dx + dz * dz);
}

// ============================================================================
// A* 経路探索（ワールド座標 → ワールド座標）
// ============================================================================

std::vector<Vector3> NavPathfinder::FindPath(const Vector3& from, const Vector3& to,
                                              int maxNodes) const
{
    if (!grid_ || !grid_->IsInitialized()) return {};

    NavGrid::GridPos gFrom = grid_->WorldToGrid(from);
    NavGrid::GridPos gTo   = grid_->WorldToGrid(to);
    return FindPath(gFrom, gTo, maxNodes);
}

// ============================================================================
// A* 経路探索（グリッド座標 → ワールド座標列）
// ============================================================================

std::vector<Vector3> NavPathfinder::FindPath(const NavGrid::GridPos& fromGrid,
                                              const NavGrid::GridPos& toGrid,
                                              int maxNodes) const
{
    if (!grid_ || !grid_->IsInitialized()) return {};

    // ゴールが通行不可なら、周辺の最も近い通行可能セルに補正する
    NavGrid::GridPos goal = toGrid;
    if (!grid_->IsWalkable(goal)) {
        // 周囲3x3で最初に見つかった通行可能セルを代替ゴールにする
        bool found = false;
        for (int r = 1; r <= 5 && !found; ++r) {
            for (int dz = -r; dz <= r && !found; ++dz) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    NavGrid::GridPos candidate{ goal.x + dx, goal.z + dz };
                    if (grid_->IsWalkable(candidate)) {
                        goal = candidate;
                        found = true;
                    }
                }
            }
        }
        if (!found) return {}; // ゴール周辺に通行可能セルがない
    }

    // スタートも通行不可なら同様に補正
    NavGrid::GridPos start = fromGrid;
    if (!grid_->IsWalkable(start)) {
        bool found = false;
        for (int r = 1; r <= 3 && !found; ++r) {
            for (int dz = -r; dz <= r && !found; ++dz) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    NavGrid::GridPos candidate{ start.x + dx, start.z + dz };
                    if (grid_->IsWalkable(candidate)) {
                        start = candidate;
                        found = true;
                    }
                }
            }
        }
        if (!found) return {};
    }

    // ────────────────────────────────────────────────────────────────────
    // A* 本体
    // ────────────────────────────────────────────────────────────────────

    const int W = grid_->GetWidthCells();

    // セルIndexをキーにしたコスト管理
    std::unordered_map<int, float> gCost; // セルIndex → g値
    std::unordered_map<int, int>   parent;// セルIndex → 親セルIndex

    // オープンリスト（f値が小さいものを優先）
    // pair<f値, セルIndex>
    using OpenEntry = std::pair<float, int>;
    std::priority_queue<OpenEntry,
                        std::vector<OpenEntry>,
                        std::greater<OpenEntry>> openList;

    auto cellIdx = [&](const NavGrid::GridPos& gp) {
        return gp.z * W + gp.x;
    };

    int startIdx = cellIdx(start);
    int goalIdx  = cellIdx(goal);

    gCost[startIdx] = 0.0f;
    parent[startIdx] = -1;
    float h0 = Heuristic(start, goal);
    openList.push({ h0, startIdx });

    int explored = 0;

    while (!openList.empty() && explored < maxNodes) {
        auto [f, idx] = openList.top();
        openList.pop();
        ++explored;

        if (idx == goalIdx) break; // ゴール到達

        int gx = idx % W;
        int gz = idx / W;

        for (int d = 0; d < kDirCount; ++d) {
            int nx = gx + kDirX[d];
            int nz = gz + kDirZ[d];

            if (!grid_->IsWalkable(nx, nz)) continue;

            // 斜め移動時、両軸方向も通行可能でないとすり抜けを防ぐ
            if (d >= 4) { // 斜め方向
                if (!grid_->IsWalkable(gx + kDirX[d], gz) ||
                    !grid_->IsWalkable(gx, gz + kDirZ[d])) continue;
            }

            int nIdx = nz * W + nx;
            float newG = gCost[idx] + kDirCost[d];

            if (gCost.find(nIdx) == gCost.end() || newG < gCost[nIdx]) {
                gCost[nIdx]  = newG;
                parent[nIdx] = idx;
                float h = Heuristic({ nx, nz }, goal);
                openList.push({ newG + h, nIdx });
            }
        }
    }

    // ゴールに到達していなければ空を返す
    if (parent.find(goalIdx) == parent.end() && startIdx != goalIdx) {
        return {};
    }

    // ────────────────────────────────────────────────────────────────────
    // 経路の再構築（ゴール → スタートを逆順に辿る）
    // ────────────────────────────────────────────────────────────────────

    std::vector<Vector3> rawPath;
    int cur = goalIdx;
    while (cur != -1) {
        int gx = cur % W;
        int gz = cur / W;
        rawPath.push_back(grid_->GridToWorld({ gx, gz }));

        auto it = parent.find(cur);
        cur = (it != parent.end()) ? it->second : -1;
    }

    std::reverse(rawPath.begin(), rawPath.end());

    // ────────────────────────────────────────────────────────────────────
    // Funnel（スムージング）を適用してジグザグを除去
    // ────────────────────────────────────────────────────────────────────
    return SmoothPath(rawPath);
}

// ============================================================================
// 簡易Funnel（String-Pulling）
//
// 現在地点から直線が通るウェイポイントをスキップしていく。
// これにより「障害物を避けつつ最短に近い直線ルート」になる。
// ============================================================================

std::vector<Vector3> NavPathfinder::SmoothPath(const std::vector<Vector3>& rawPath) const
{
    if (!grid_ || rawPath.size() <= 2) return rawPath;

    std::vector<Vector3> smoothed;
    smoothed.push_back(rawPath.front());

    size_t anchor = 0; // 現在の「視点」となるウェイポイントのインデックス

    while (anchor < rawPath.size() - 1) {
        // anchor から最も遠くて視線が通るウェイポイントを探す
        size_t farthest = anchor + 1;
        for (size_t i = anchor + 2; i < rawPath.size(); ++i) {
            if (grid_->HasLineOfSight(rawPath[anchor], rawPath[i])) {
                farthest = i;
            } else {
                break; // 視線が通らなくなった時点で打ち切り
            }
        }

        smoothed.push_back(rawPath[farthest]);
        anchor = farthest;
    }

    return smoothed;
}
