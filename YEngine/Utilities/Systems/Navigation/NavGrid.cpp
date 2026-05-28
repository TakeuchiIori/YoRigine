#include "NavGrid.h"

// C++
#include <cmath>
#include <algorithm>
#include <cstdlib> // abs

// Engine
#include <Object3D/ObjectManager.h>
#include <Collision/Core/CollisionTypeIdDef.h>
#include <Collision/AABB/AABBCollider.h>
#include <Collision/OBB/OBBCollider.h>

// Math
#include "Matrix4x4.h"

// ============================================================================
// OBBのAABBを計算するヘルパー（NavGridベイク専用）
// OBBの各ローカル軸をワールド軸に射影した和がAABBの半サイズになる
// ============================================================================
static AABB ComputeAABBFromOBB(const OBB& obb)
{
    // OBBの回転行列を再構築（列ベクトル規約: 列i = ローカル軸i のワールド方向）
    Matrix4x4 R = MakeRotateMatrixXYZ(obb.rotation);

    // ワールド各軸方向へのAABB半サイズ = Σ |ローカル軸i × 半サイズi| の射影
    // 行0の各要素 = ローカル軸のワールドX成分
    float extX = obb.size.x * std::abs(R.m[0][0])
               + obb.size.y * std::abs(R.m[0][1])
               + obb.size.z * std::abs(R.m[0][2]);
    float extY = obb.size.x * std::abs(R.m[1][0])
               + obb.size.y * std::abs(R.m[1][1])
               + obb.size.z * std::abs(R.m[1][2]);
    float extZ = obb.size.x * std::abs(R.m[2][0])
               + obb.size.y * std::abs(R.m[2][1])
               + obb.size.z * std::abs(R.m[2][2]);

    return AABB{
        { obb.center.x - extX, obb.center.y - extY, obb.center.z - extZ },
        { obb.center.x + extX, obb.center.y + extY, obb.center.z + extZ }
    };
}

// ============================================================================
// 初期化
// ============================================================================

void NavGrid::Initialize(float worldMinX, float worldMaxX,
                         float worldMinZ, float worldMaxZ,
                         float cellSize)
{
	// 範囲とセルサイズを設定する
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

void NavGrid::Bake(ObjectManager* objectManager) {
    if (!objectManager || !IsInitialized()) return;

    // 全セルをまず歩行可能にリセット
    Reset();

    // kNavObstacle コライダーが付いたオブジェクトを障害物としてマーク
    for (const auto* obj : objectManager->GetAllActiveObjects()) {
        if (!obj) continue;
        // typeId は引き続きテンプレートで判定、形状はコライダーから取得
        const auto* tmpl = objectManager->FindTemplate(obj->modelName);
        if (!tmpl) continue;
        if (tmpl->typeId != CollisionTypeIdDef::kNavObstacle &&
            tmpl->typeId != CollisionTypeIdDef::kStaticWall) continue;
        if (!obj->collider || !obj->colliderEnabled) continue;

        // AABBCollider
        if (auto* aabb = dynamic_cast<AABBCollider*>(obj->collider.get())) {
            MarkObstacle(aabb->GetAABB(), true);
            continue;
        }
        // OBBCollider — 回転を考慮してバウンディングAABBを計算してマーク
        if (auto* obb = dynamic_cast<OBBCollider*>(obj->collider.get())) {
            MarkObstacle(ComputeAABBFromOBB(obb->GetOBB()), true);
            continue;
        }
    }

    // Erosion：エージェント半径分だけ障害物セルを膨張させる
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
    // AABBの四隅からグリッド座標を計算して全セルをfalseに
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
// AABBが重なるセル範囲にコールバックを呼ぶ
// ============================================================================

template<typename Fn>
void NavGrid::ForEachOverlappingCell(const AABB& worldAABB, Fn fn) const
{
    // ワールド座標 → グリッド浮動小数点座標に変換
    float rawXMin = (worldAABB.min.x - worldMinX_) / cellSize_;
    float rawXMax = (worldAABB.max.x - worldMinX_) / cellSize_;
    float rawZMin = (worldAABB.min.z - worldMinZ_) / cellSize_;
    float rawZMax = (worldAABB.max.z - worldMinZ_) / cellSize_;

    // AABBがグリッド範囲外ならスキップ（クランプでの誤マーク防止）
    if (rawXMax < 0.0f || rawXMin >= static_cast<float>(widthCells_) ||
        rawZMax < 0.0f || rawZMin >= static_cast<float>(depthCells_)) {
        return;
    }

    int gxMin = std::max(static_cast<int>(rawXMin), 0);
    int gxMax = std::min(static_cast<int>(rawXMax), widthCells_ - 1);
    int gzMin = std::max(static_cast<int>(rawZMin), 0);
    int gzMax = std::min(static_cast<int>(rawZMax), depthCells_ - 1);

    for (int gz = gzMin; gz <= gzMax; ++gz) {
        for (int gx = gxMin; gx <= gxMax; ++gx) {
            fn(gx, gz);
        }
    }
}

// ============================================================================
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
