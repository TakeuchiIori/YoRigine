#include "NavGrid.h"

// C++
#include <cmath>
#include <algorithm>
#include <cstdlib> // abs
#include <cstdio>
#include <Windows.h>

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
    // YoRigine は行ベクトル規約 ((x,y,z,1) * M)。
    // 回転行列の row i がローカル軸 i のワールド方向ベクトルになる。
    //   ローカルX軸 (1,0,0) を Transform → (R[0][0], R[0][1], R[0][2])
    //   ローカルY軸 (0,1,0) を Transform → (R[1][0], R[1][1], R[1][2])
    //   ローカルZ軸 (0,0,1) を Transform → (R[2][0], R[2][1], R[2][2])
    // よってローカル軸 i の「ワールド成分 j」は R.m[i][j]。
    Matrix4x4 R = MakeRotateMatrixXYZ(obb.rotation);

    // ワールド各軸方向への AABB 半サイズ = Σ size[i] * |ローカル軸 i のワールド成分|
    float extX = obb.size.x * std::abs(R.m[0][0])
               + obb.size.y * std::abs(R.m[1][0])
               + obb.size.z * std::abs(R.m[2][0]);
    float extY = obb.size.x * std::abs(R.m[0][1])
               + obb.size.y * std::abs(R.m[1][1])
               + obb.size.z * std::abs(R.m[2][1]);
    float extZ = obb.size.x * std::abs(R.m[0][2])
               + obb.size.y * std::abs(R.m[1][2])
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

    int markedCount = 0;
    int outOfRangeCount = 0;
    int skippedNotNav = 0;
    int skippedNoCollider = 0;
    int skippedDisabled = 0;
    int skippedShape = 0;

    // kNavObstacle コライダーが付いたオブジェクトを障害物としてマーク
    for (const auto* obj : objectManager->GetAllActiveObjects()) {
        if (!obj) continue;
        if (obj->colliderTypeId != CollisionTypeIdDef::kNavObstacle &&
            obj->colliderTypeId != CollisionTypeIdDef::kStaticWall) { ++skippedNotNav; continue; }
        if (!obj->collider) { ++skippedNoCollider; continue; }
        if (!obj->colliderEnabled) { ++skippedDisabled; continue; }

        // 障害物が視錐台外にあると ObjectManager::Update() で
        // collider->Update() が culling によりスキップされ、AABB が古い matWorld の値で
        // 残っている可能性がある。Bake では正しい位置のセルをマークしたいので、
        // 読み取り前に強制的に Update を呼んで最新の matWorld を反映させる。
        obj->collider->Update();

        AABB worldAabb{};
        bool isShapeOk = false;

        // AABBCollider
        if (auto* aabb = dynamic_cast<AABBCollider*>(obj->collider.get())) {
            worldAabb = aabb->GetAABB();
            isShapeOk = true;
        }
        // OBBCollider — 回転を考慮してバウンディングAABBを計算してマーク
        else if (auto* obb = dynamic_cast<OBBCollider*>(obj->collider.get())) {
            worldAabb = ComputeAABBFromOBB(obb->GetOBB());
            isShapeOk = true;
        }

        if (!isShapeOk) {
            ++skippedShape;
            continue;
        }

        const int cells = MarkObstacle(worldAabb, true);
        if (cells == 0) {
            // グリッド範囲外。obj 位置と現在のグリッド範囲をログに残して原因切り分け
            ++outOfRangeCount;
            char dbg[256];
            sprintf_s(dbg,
                "[NavGrid::Bake] obj id=%d AABB=(%.1f,%.1f)..(%.1f,%.1f) は範囲外 grid=(%.1f,%.1f)..(%.1f,%.1f)\n",
                obj->id,
                worldAabb.min.x, worldAabb.min.z,
                worldAabb.max.x, worldAabb.max.z,
                worldMinX_, worldMinZ_, worldMaxX_, worldMaxZ_);
            OutputDebugStringA(dbg);
        }
        else {
            ++markedCount;
        }
    }

    // Erosion：エージェント半径分だけ障害物セルを膨張させる
    ApplyErosion();

    char buf[256];
    sprintf_s(buf,
        "[NavGrid::Bake] marked=%d out-of-range=%d type-mismatch=%d no-collider=%d disabled=%d unsupported-shape=%d cellSize=%.2f agentR=%.2f\n",
        markedCount, outOfRangeCount, skippedNotNav, skippedNoCollider, skippedDisabled, skippedShape,
        cellSize_, agentRadius_);
    OutputDebugStringA(buf);
}

// ============================================================================
// リセット
// ============================================================================

void NavGrid::Reset()
{
    for (auto& c : cells_) { c.walkable = true; c.rawObstacle = false; }
}

// ============================================================================
// 単一AABBを障害物としてマーク
// ============================================================================

int NavGrid::MarkObstacle(const AABB& worldAABB, bool obstacle)
{
    int marked = 0;
    // AABBの四隅からグリッド座標を計算して全セルをfalseに
    ForEachOverlappingCell(worldAABB, [&](int gx, int gz) {
        Cell& c = cells_[CellIndex(gx, gz)];
        c.walkable = !obstacle;
        if (obstacle) {
            // erosion 前の "実 obstacle footprint" を記録。デバッグ描画で
            // 配置オブジェクトの位置・サイズと一致する範囲を可視化するのに使う。
            c.rawObstacle = true;
        }
        ++marked;
    });
    return marked;
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
