#include "NavGrid.h"

// C++
#include <cmath>
#include <algorithm>
#include <cstdlib> // abs

// Engine
#include <Object3D/ObjectManager.h>
#include <Collision/Core/CollisionTypeIdDef.h>
#include <Collision/AABB/AABBCollider.h>

// ============================================================================
// 初期化
// ============================================================================

void NavGrid::Initialize(float worldMinX, float worldMaxX,
                         float worldMinZ, float worldMaxZ,
                         float cellSize)
{
    worldMinX_ = worldMinX;
    worldMaxX_ = worldMaxX;
    worldMinZ_ = worldMinZ;
    worldMaxZ_ = worldMaxZ;
    cellSize_  = (cellSize > 0.0f) ? cellSize : 1.0f;

    widthCells_ = static_cast<int>(std::ceilf((worldMaxX_ - worldMinX_) / cellSize_));
    depthCells_ = static_cast<int>(std::ceilf((worldMaxZ_ - worldMinZ_) / cellSize_));

    cells_.assign(widthCells_ * depthCells_, Cell{});
}

// ============================================================================
// ベイク
// ============================================================================

void NavGrid::Bake(ObjectManager* objectManager)
{
    if (!objectManager || !IsInitialized()) return;

    // 全セルをまず歩行可能にリセット
    Reset();

    // kNavObstacle コライダーが付いたオブジェクトを障害物としてマーク
    for (const auto* obj : objectManager->GetAllActiveObjects()) {
        if (!obj) continue;

        // テンプレートの typeId を確認
        const auto* tmpl = objectManager->FindTemplate(obj->modelName);
        if (!tmpl) continue;
        if (tmpl->typeId != CollisionTypeIdDef::kNavObstacle &&
            tmpl->typeId != CollisionTypeIdDef::kStaticWall) continue;

        // ワールドAABBを計算してグリッドに投影
        const Vector3 half   = tmpl->size * 0.5f;
        const Vector3 center = obj->position + tmpl->offset;
        AABB worldAABB;
        worldAABB.min = center - half;
        worldAABB.max = center + half;

        MarkObstacle(worldAABB, true);
    }

    // Erosion：エージェント半径分だけ障害物セルを膨張させる
    // こうすることで壁ギリギリを通らなくなる（UnityのAgent Radiusと同等）
    ApplyErosion();
}

// ============================================================================
// リセット
// ============================================================================

void NavGrid::Reset()
{
    for (auto& c : cells_) c.walkable = true;
}

// ============================================================================
// 単一AABBを障害物としてマーク
// ============================================================================

void NavGrid::MarkObstacle(const AABB& worldAABB, bool obstacle)
{
    ForEachOverlappingCell(worldAABB, [&](int gx, int gz) {
        cells_[CellIndex(gx, gz)].walkable = !obstacle;
    });
}

// ============================================================================
// 座標変換
// ============================================================================

NavGrid::GridPos NavGrid::WorldToGrid(const Vector3& worldPos) const
{
    GridPos gp;
    gp.x = static_cast<int>((worldPos.x - worldMinX_) / cellSize_);
    gp.z = static_cast<int>((worldPos.z - worldMinZ_) / cellSize_);

    // 範囲外はクランプ
    gp.x = std::clamp(gp.x, 0, widthCells_ - 1);
    gp.z = std::clamp(gp.z, 0, depthCells_ - 1);
    return gp;
}

Vector3 NavGrid::GridToWorld(const GridPos& gp) const
{
    return {
        worldMinX_ + (gp.x + 0.5f) * cellSize_,
        0.0f,
        worldMinZ_ + (gp.z + 0.5f) * cellSize_
    };
}

// ============================================================================
// クエリ
// ============================================================================

bool NavGrid::IsWalkable(int gx, int gz) const
{
    if (!InBounds(gx, gz)) return false;
    return cells_[CellIndex(gx, gz)].walkable;
}

bool NavGrid::IsWalkableWorld(const Vector3& worldPos) const
{
    return IsWalkable(WorldToGrid(worldPos));
}

// ============================================================================
// XZ平面レイキャスト（Bresenhamアルゴリズム）
//
// from → to の直線がグリッド上で通行不可セルを通過するかを判定する。
// 「通過しない＝視線が通る」なら true を返す。
//
// Unity の Physics.Raycast に相当するが、3Dフィジックスより軽量で
// 平坦なフィールドには十分。
// ============================================================================

bool NavGrid::HasLineOfSight(const Vector3& from, const Vector3& to) const
{
    if (!IsInitialized()) return true; // 未初期化なら遮蔽なしとして扱う

    GridPos gFrom = WorldToGrid(from);
    GridPos gTo   = WorldToGrid(to);

    int x0 = gFrom.x, z0 = gFrom.z;
    int x1 = gTo.x,   z1 = gTo.z;

    int dx = std::abs(x1 - x0);
    int dz = std::abs(z1 - z0);
    int sx = (x0 < x1) ? 1 : -1;
    int sz = (z0 < z1) ? 1 : -1;
    int err = dx - dz;

    while (true) {
        // 現在のセルが通行不可なら視線は遮られている
        if (!IsWalkable(x0, z0)) return false;

        if (x0 == x1 && z0 == z1) break;

        int e2 = 2 * err;
        if (e2 > -dz) { err -= dz; x0 += sx; }
        if (e2 <  dx) { err += dx; z0 += sz; }
    }

    return true; // 全セルが通行可能 → 視線が通る
}

// ============================================================================
// 内部：AABBが重なるセル範囲にコールバックを呼ぶ
// ============================================================================

template<typename Fn>
void NavGrid::ForEachOverlappingCell(const AABB& worldAABB, Fn fn) const
{
    // ワールド座標 → グリッド座標に変換（AABBの四隅）
    int gxMin = static_cast<int>((worldAABB.min.x - worldMinX_) / cellSize_);
    int gxMax = static_cast<int>((worldAABB.max.x - worldMinX_) / cellSize_);
    int gzMin = static_cast<int>((worldAABB.min.z - worldMinZ_) / cellSize_);
    int gzMax = static_cast<int>((worldAABB.max.z - worldMinZ_) / cellSize_);

    gxMin = std::clamp(gxMin, 0, widthCells_ - 1);
    gxMax = std::clamp(gxMax, 0, widthCells_ - 1);
    gzMin = std::clamp(gzMin, 0, depthCells_ - 1);
    gzMax = std::clamp(gzMax, 0, depthCells_ - 1);

    for (int gz = gzMin; gz <= gzMax; ++gz) {
        for (int gx = gxMin; gx <= gxMax; ++gx) {
            fn(gx, gz);
        }
    }
}

// ============================================================================
// 内部：Erosion
// 障害物セルの周囲 N セル（agentRadius / cellSize を切り上げ）も
// 通行不可にする。壁ギリギリを通ることを防ぐ。
// ============================================================================

void NavGrid::ApplyErosion()
{
    const int erosionCells = static_cast<int>(
        std::ceilf(agentRadius_ / cellSize_));
    if (erosionCells <= 0) return;

    // 現在の通行不可セルを記録（erosion前のスナップショット）
    const std::vector<Cell> snapshot = cells_;

    for (int gz = 0; gz < depthCells_; ++gz) {
        for (int gx = 0; gx < widthCells_; ++gx) {
            if (snapshot[CellIndex(gx, gz)].walkable) continue;

            // 通行不可セルの周囲 erosionCells 分を通行不可にする
            for (int dz = -erosionCells; dz <= erosionCells; ++dz) {
                for (int dx = -erosionCells; dx <= erosionCells; ++dx) {
                    int nx = gx + dx;
                    int nz = gz + dz;
                    if (InBounds(nx, nz)) {
                        cells_[CellIndex(nx, nz)].walkable = false;
                    }
                }
            }
        }
    }
}
